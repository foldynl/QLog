#include "PotaAdiFormat.h"
#include <QDebug>
#include <QSqlField>
#include <QSqlRecord>
#include "core/debug.h"
#include <models/LogbookModel.h>

MODULE_IDENTIFICATION("qlog.logformat.potalogformat");

#define ALWAYS_PRESENT true

PotaAdiFormat::PotaAdiFormat(QTextStream &stream)
    : AdiFormat(stream)
    , currentDate(QDateTime::currentDateTime())
{
    FCT_IDENTIFICATION;
}

void PotaAdiFormat::setExportInfo(QFile &exportFile)
{
    this->exportInfo = new QFileInfo(exportFile);
}

void PotaAdiFormat::exportContact(const QSqlRecord &sourceRecord, QMap<QString, QString> *applTags)
{
    FCT_IDENTIFICATION;
    if (this->exportInfo == nullptr) {
        return;
    }
    // break single record into child activated park records
    // assign records to files
    QList<QSqlRecord> records = PotaAdiFormat::splitActivatedParks(sourceRecord);
    for (QSqlRecord &record : records) {
        duplicateField(record, "my_pota_ref", "my_sig");
        record.field("my_sig").setValue(QString("POTA"));
        duplicateField(record, "my_pota_ref", "my_sig_info");
        if (!record.field("pota_ref").isNull()) {
            duplicateField(record, "pota_ref", "sig_info");
            duplicateField(record, "pota_ref", "sig");
            record.field("sig").setValue(QString("POTA"));
        }
        AdiFormat *parkOut = this->getParkFile(record);
        parkOut->exportContact(record, applTags);
        // let parent do ADI export as normal to specified file
        AdiFormat::exportContact(record, applTags);
    }
}

AdiFormat *PotaAdiFormat::getParkFile(const QSqlRecord &record)
{
    // https://docs.pota.app/docs/activator_reference/logging_made_easy.html#naming-your-files
    // station_callsign@park#-yyyymmdd
    const QString parkFileName(record.field("station_callsign").value().toString() + "@"
                               + record.field("my_sig_info").value().toString() + "-"
                               + currentDate.toString("yyyyMMdd-hhmm") + ".adif");

    if (!parkFormats.contains(parkFileName)) {
        parkFiles[parkFileName] = new QFile(exportInfo->canonicalPath() + QDir::separator()
                                            + parkFileName);
        if (!parkFiles[parkFileName]->open(QFile::WriteOnly | QFile::Text)) {
            qCCritical(runtime) << "Could not open POTA park file for writing "
                                << parkFiles[parkFileName]->fileName();
        }
        QTextStream *parkStream = new QTextStream(parkFiles[parkFileName]);
        parkFormats[parkFileName] = new AdiFormat(*parkStream);
        parkFormats[parkFileName]->exportStart();
    }
    qCDebug(runtime) << "using park file " << parkFileName << " is open? "
                     << parkFiles[parkFileName]->isOpen();

    return parkFormats[parkFileName];
}

QList<QSqlRecord> PotaAdiFormat::splitActivatedParks(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;
    // can contain multiple parks as a csv:
    // <MY_POTA_REF:40>K-0817,K-4566,K-4576,K-4573,K-4578@US-WY
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    const QStringList activatedParks = record.field("my_pota_ref")
                                           .value()
                                           .toString()
                                           .split(QRegularExpression("\\s*,\\s*"),
                                                  Qt::SplitBehaviorFlags::SkipEmptyParts);
#else /* Due to ubuntu 20.04 where qt5.12 is present */
    const QStringList activatedParks = record.field("my_pota_ref")
                                           .value()
                                           .toString()
                                           .split(QRegularExpression("\\s*,\\s*"),
                                                  QString::SkipEmptyParts);
#endif

    if (activatedParks.length() <= 0) {
        return QList<QSqlRecord>();
    } else if (activatedParks.length() == 1) {
        return QList<QSqlRecord>({record});
    } else {
        QList<QSqlRecord> records = QList<QSqlRecord>();
        for (const QString &parkID : activatedParks) {
            QSqlRecord single = QSqlRecord(record);
            single.setValue("my_pota_ref", parkID);

            // If this is a park to park - the remote park can also be a multi park
            // activation. These must also be split into multiple records.
            const QSqlField parkToPark = record.field("pota_ref");
            if (parkToPark.isNull() || !parkToPark.value().toString().contains(",")) {
                records.append(single);
            } else {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                QStringList remoteParks
                    = parkToPark.value().toString().split(QRegularExpression("\\s*,\\s*"),
                                                          Qt::SplitBehaviorFlags::SkipEmptyParts);
#else /* Due to ubuntu 20.04 where qt5.12 is present */
                QStringList remoteParks
                    = parkToPark.value().toString().split(QRegularExpression("\\s*,\\s*"),
                                                          QString::SkipEmptyParts);
#endif
                for (const QString &remoteParkID : remoteParks) {
                    QSqlRecord remoteSingle = QSqlRecord(single);
                    remoteSingle.setValue("pota_ref", remoteParkID);
                    records.append(remoteSingle);
                }
            }
        }
        return records;
    }
}

void PotaAdiFormat::exportEnd()
{
    for (AdiFormat *parkFormat : parkFormats.values()) {
        parkFormat->exportEnd();
    }
}

void PotaAdiFormat::duplicateField(QSqlRecord &record,
                                   const QString &fromFieldName,
                                   const QString &toFieldName)
{
    QSqlField dupped(record.field(fromFieldName));
    record.remove(record.indexOf(toFieldName));
    dupped.setName(toFieldName);
    record.append(dupped);
}

PotaAdiFormat::~PotaAdiFormat()
{
    FCT_IDENTIFICATION;
    qDeleteAll(parkFormats.values());
    parkFormats.clear();
    qDeleteAll(parkFiles.values());
    parkFiles.clear();
}
