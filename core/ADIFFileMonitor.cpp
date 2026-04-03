#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QMessageBox>
#include <QSpacerItem>
#include <QGridLayout>

#include "ADIFFileMonitor.h"
#include "core/LogParam.h"
#include "core/debug.h"
#include "data/StationProfile.h"
#include "logformat/LogFormat.h"

MODULE_IDENTIFICATION("qlog.core.adiffilemonitor");

ADIFFileMonitor::ADIFFileMonitor(QObject *parent)
    : QObject(parent)
{
    FCT_IDENTIFICATION;
}

ADIFFileMonitor::~ADIFFileMonitor()
{
    FCT_IDENTIFICATION;
}

QList<ADIFFileMonitor::SlotConfig> ADIFFileMonitor::getEnabledSlots(ImportFrequency freq) const
{
    FCT_IDENTIFICATION;

    QList<SlotConfig> result;

    for ( int i = 0; i < MAX_SLOTS; ++i )
    {
        SlotConfig cfg;
        cfg.enabled   = LogParam::getADIFMonitorEnabled(i);
        cfg.path      = LogParam::getADIFMonitorPath(i);
        cfg.frequency = static_cast<ImportFrequency>(LogParam::getADIFMonitorFrequency(i));
        cfg.profile   = LogParam::getADIFMonitorProfile(i);

        if ( cfg.enabled && cfg.frequency == freq && !cfg.path.isEmpty() )
            result.append(cfg);
    }

    return result;
}

LogFormat::duplicateQSOBehaviour ADIFFileMonitor::skipAllDuplicates(QSqlRecord *, QSqlRecord *)
{
    return LogFormat::SKIP_ALL;
}

void ADIFFileMonitor::runImportForSlot(const SlotConfig &slot)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "ADIF Monitor importing:" << slot.path;

    QFile file(slot.path);

    if ( !file.exists() )
    {
        qCWarning(runtime) << "ADIF Monitor: file not found:" << slot.path;
        return;
    }

    if ( !file.open(QFile::ReadOnly | QFile::Text) )
    {
        qCWarning(runtime) << "ADIF Monitor: cannot open file:" << slot.path;
        return;
    }

    QTextStream in(&file);

    LogFormat *format = LogFormat::open("adi", in);

    if ( !format )
    {
        qCWarning(runtime) << "ADIF Monitor: failed to open as ADIF:" << slot.path;
        return;
    }

    format->setDuplicateQSOCallback(skipAllDuplicates);

    StationProfile stationProfile;

    if ( !slot.profile.isEmpty() )
        stationProfile = StationProfilesManager::instance()->getProfile(slot.profile);

    if ( stationProfile == StationProfile() )
        stationProfile = StationProfilesManager::instance()->getCurProfile1();

    const StationProfile *profile = ( stationProfile != StationProfile() ) ? &stationProfile : nullptr;

    unsigned long warnings = 0;
    unsigned long errors   = 0;
    QString logStream;
    QTextStream logOut(&logStream);

    int count = format->runImport(logOut, profile, &warnings, &errors);

    if (count > 0)
    {
        QString s;
        QString report = QObject::tr("<b>Import File</b>: ") +  slot.path + "<br/>" +
                         QObject::tr("<b>Imported</b>: %n contact(s)", "", count) + "<br/>" +
                         QObject::tr("<b>Warning(s)</b>: %n", "", warnings) + "<br/>" +
                         QObject::tr("<b>Error(s)</b>: %n", "", errors);

        QMessageBox msgBox;

        msgBox.setWindowTitle(tr("Import Result"));
        msgBox.setText(report);
        msgBox.setDetailedText(s);
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);

        QSpacerItem* horizontalSpacer = new QSpacerItem(500, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = qobject_cast<QGridLayout*>(msgBox.layout());
        if ( !layout )
        {
            qWarning() << "Layout is null";
            delete horizontalSpacer;
            return;
        }
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());

        msgBox.exec();

    }
    delete format;

    qCDebug(runtime) << "ADIF Monitor import finished. Warnings:" << warnings << "Errors:" << errors;
}

void ADIFFileMonitor::runStartupImports()
{
    FCT_IDENTIFICATION;

    const QList<SlotConfig> fileSlots = getEnabledSlots(STARTUP);

    for ( const SlotConfig &cfg : fileSlots )
        runImportForSlot(cfg);
}

void ADIFFileMonitor::runShutdownImports()
{
    FCT_IDENTIFICATION;

    const QList<SlotConfig> fileSlots = getEnabledSlots(SHUTDOWN);

    for ( const SlotConfig &cfg : fileSlots )
        runImportForSlot(cfg);
}
