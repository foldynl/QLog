#ifndef QLOG_LOGFORMAT_POTALOGFORMAT_H
#define QLOG_LOGFORMAT_POTALOGFORMAT_H
#include "AdiFormat.h"

/*
 * A specialized case of ADI export, where each activated park gets T'd into its
 * own file with some denormalization and values set to satisfy the pota.app
 * upload processes.
 */
class PotaAdiFormat : public AdiFormat
{
public:
    explicit PotaAdiFormat(QTextStream &stream);

    virtual void exportContact(const QSqlRecord &,
                               QMap<QString, QString> *applTags = nullptr) override;
    virtual void exportEnd() override;

    virtual bool importNext(QSqlRecord &) override { return false; }

    void setExportInfo(QFile &exportFile);
    ~PotaAdiFormat();

    static QList<QSqlRecord> splitActivatedParks(const QSqlRecord &);

private:
    QFileInfo *exportInfo;
    QMap<QString, AdiFormat *> parkFormats;
    QMap<QString, QFile *> parkFiles;
    QDateTime currentDate;
    AdiFormat *getParkFile(const QSqlRecord &record);
    static void duplicateField(QSqlRecord &record,
                               const QString &fromFieldName,
                               const QString &toFieldName);
};

#endif // QLOG_LOGFORMAT_POTALOGFORMAT_H
