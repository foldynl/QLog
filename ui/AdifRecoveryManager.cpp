#include "AdifRecoveryManager.h"

#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

#include "core/LogParam.h"
#include "core/debug.h"
#include "data/StationProfile.h"

MODULE_IDENTIFICATION("qlog.ui.adifrecoverymanager");

AdifRecoveryManager::AdifRecoveryManager(QObject *parent) :
    QObject(parent)
{
    FCT_IDENTIFICATION;

    qRegisterMetaType<AdifRecoveryConfig>("AdifRecoveryConfig");
    qRegisterMetaType<AdifRecoveryState>("AdifRecoveryState");
    qRegisterMetaType<AdifRecoveryScanResult>("AdifRecoveryScanResult");

    reloadSettings();
}

AdifRecoveryManager::~AdifRecoveryManager()
{
    FCT_IDENTIFICATION;
    cleanupWorker();
}

void AdifRecoveryManager::reloadSettings()
{
    FCT_IDENTIFICATION;

    if ( workerThread )
    {
        reloadAfterScan = true;
        return;
    }

    configs = LogParam::getAdifRecoveryFiles();
    configByKey.clear();
    pendingKeys.clear();

    for ( const AdifRecoveryConfig &config : static_cast<const QList<AdifRecoveryConfig>&>(configs) )
    {
        if ( config.path.trimmed().isEmpty() )
            continue;

        qCDebug(runtime) << "Adding file" << config.path << config.enabled;
        configByKey.insert(AdifRecovery::fileKey(config.path, config.stationProfileName), config);
    }
}

void AdifRecoveryManager::startStartupRecovery()
{
    FCT_IDENTIFICATION;

    if ( workerThread )
        return;

    const QSet<QString> profiles = stationProfiles();
    disableMissingProfileConfigs(profiles);
    rebuildPendingQueue(profiles);

    startNextScan();
}

void AdifRecoveryManager::startNextScan()
{
    FCT_IDENTIFICATION;

    if ( workerThread )
        return;

    // Only one reader thread is allowed at a time. The loop does not start
    // multiple workers; it only skips stale queue entries. A queued file can
    // become stale when settings are reloaded while another file is being read.
    while ( !pendingKeys.isEmpty() )
    {
        const QString fileKey = pendingKeys.dequeue();
        const AdifRecoveryConfig config = configByKey.value(fileKey);

        if ( !config.enabled || config.path.trimmed().isEmpty() || config.stationProfileName.trimmed().isEmpty() )
            continue;

        AdifRecoveryState state = LogParam::getAdifRecoveryState(fileKey);
        state.path = config.path;

        workerThread = new QThread(this);
        worker = new AdifRecoveryReaderWorker();
        worker->moveToThread(workerThread);

        qCDebug(runtime) << "starting a new thread for " << state.path;

        connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
        connect(worker, &AdifRecoveryReaderWorker::scanFinished, this, &AdifRecoveryManager::scanFinished);
        connect(workerThread, &QThread::started, this, [this, config, state]()
        {
            QMetaObject::invokeMethod(worker, "readTail", Qt::QueuedConnection,
                                      Q_ARG(AdifRecoveryConfig, config),
                                      Q_ARG(AdifRecoveryState, state),
                                      Q_ARG(int, MAX_AUTOMATIC_CONTACTS));
        });

        workerThread->start();
        return;
    }
}

void AdifRecoveryManager::scanFinished(const AdifRecoveryScanResult &result)
{
    FCT_IDENTIFICATION;

    const AdifRecoveryConfig config = configByKey.value(result.fileKey);
    const AdifRecoveryState state = LogParam::getAdifRecoveryState(result.fileKey);

    qCDebug(runtime) << "Message" << result.message;

    if ( result.tooMany )
    {
        saveState(result.fileKey, config, result.nextOffset, result.message);
        const QString text = tr("Startup ADI found more than %1 new QSOs in %2. Use the standard Import. Load point was moved to the end of the file.")
                             .arg(MAX_AUTOMATIC_CONTACTS)
                             .arg(QFileInfo(result.path).fileName());
        qCWarning(runtime) << "Too many QSOs";
        emit problem(text);
    }
    else if ( !result.message.isEmpty() && result.nextOffset >= 0 && result.nextOffset != state.offset )
    {
        saveState(result.fileKey, config, result.nextOffset, result.message);
        if ( result.reset )
            emit problem(result.message);
    }
    else if ( !result.message.isEmpty() )
    {
        qCWarning(runtime) << result.message;
        emit problem(result.message);
    }
    else if ( !result.adifText.isEmpty() )
    {
        qCDebug(runtime) << "Importing contacts";
        importRecoveredContacts(result, config);
    }

    cleanupWorker();

    if ( reloadAfterScan )
    {
        reloadAfterScan = false;
        reloadSettings();
        startStartupRecovery();
        return;
    }

    startNextScan();
}

void AdifRecoveryManager::importRecoveredContacts(const AdifRecoveryScanResult &result,
                                                  const AdifRecoveryConfig &config)
{
    FCT_IDENTIFICATION;

    const StationProfile stationProfile = StationProfilesManager::instance()->getProfile(config.stationProfileName);

    if ( stationProfile.profileName.isEmpty() )
    {
        const QString text = tr("Startup ADI Station Profile does not exist: %1").arg(config.stationProfileName);
        qCWarning(runtime) << "Profile name is empty";
        emit problem(text);
        return;
    }

    QString adifText = result.adifText;
    QTextStream stream(&adifText, QIODevice::ReadOnly);
    LogFormat *format = LogFormat::open("adi", stream);

    if ( !format )
    {
        const QString text = tr("Cannot open Startup ADI records from %1").arg(config.path);
        qCWarning(runtime) << "Cannot open file";
        emit problem(text);
        return;
    }

    QMap<QString, QString> defaults = qslSentDefaults(config);

    if ( !defaults.isEmpty() )
        format->setDefaults(defaults);

    format->setFillMissingDxcc(true);
    format->setDuplicateQSOCallback(skipAllDuplicates);

    QString importLog;
    QTextStream importLogStream(&importLog);
    unsigned long warnings = 0;
    unsigned long errors = 0;
    const int importedCount = format->runImport(importLogStream, &stationProfile,
                                                &warnings, &errors);
    format->deleteLater();

    if ( errors == 0 )
    {
        saveState(result.fileKey, config, result.nextOffset);
        if ( importedCount > 0 )
            emit contactsRecovered(importedCount);
    }
    else
    {
        const QString text = tr("Startup ADI from %1 finished with %n error(s); load point was not advanced.",
                                "", errors).arg(QFileInfo(config.path).fileName());
        qCWarning(runtime).noquote() << text << importLog;
        emit problem(text);
    }

    Q_UNUSED(warnings)
}

QMap<QString, QString> AdifRecoveryManager::qslSentDefaults(const AdifRecoveryConfig &config) const
{
    QMap<QString, QString> defaults;

    const QString status = config.qslSentStatusDefault.isEmpty() ? "Q" : config.qslSentStatusDefault;

    if ( status == "custom" )
    {
        defaults.insert("qsl_sent", LogParam::getAdifRecoveryQslSentStatusPaper());
        defaults.insert("lotw_qsl_sent", LogParam::getAdifRecoveryQslSentStatusLoTW());
        defaults.insert("eqsl_qsl_sent", LogParam::getAdifRecoveryQslSentStatusEQSL());
        defaults.insert("dcl_qsl_sent", LogParam::getAdifRecoveryQslSentStatusDCL());
    }
    else
    {
        defaults.insert("qsl_sent", status);
        defaults.insert("lotw_qsl_sent", status);
        defaults.insert("eqsl_qsl_sent", status);
        defaults.insert("dcl_qsl_sent", status);
    }

    return defaults;
}

void AdifRecoveryManager::saveState(const QString &fileKey,
                                    const AdifRecoveryConfig &config,
                                    qint64 offset,
                                    const QString &message)
{
    AdifRecoveryState state;
    state.path = config.path;
    state.offset = offset;
    state.lastRecoveryAt = QDateTime::currentDateTimeUtc();
    state.lastMessage = message;
    LogParam::setAdifRecoveryState(fileKey, state);
}

void AdifRecoveryManager::cleanupWorker()
{
    if ( !workerThread )
        return;

    QThread *thread = workerThread;
    workerThread = nullptr;
    worker = nullptr;
    thread->quit();
    thread->wait();
    thread->deleteLater();
}

QSet<QString> AdifRecoveryManager::stationProfiles() const
{
    const QStringList profileNames = StationProfilesManager::instance()->profileNameList();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    return QSet<QString>(profileNames.begin(), profileNames.end());
#else
    return QSet<QString>::fromList(profileNames);
#endif
}

bool AdifRecoveryManager::isRunnableConfig(const AdifRecoveryConfig &config,
                                           const QSet<QString> &profiles) const
{
    return config.enabled
           && !config.path.trimmed().isEmpty()
           && !config.stationProfileName.trimmed().isEmpty()
           && profiles.contains(config.stationProfileName);
}

void AdifRecoveryManager::disableMissingProfileConfigs(const QSet<QString> &profiles)
{
    QStringList missingProfiles;

    for ( AdifRecoveryConfig &config : configs )
    {
        if ( !config.enabled
             || config.stationProfileName.trimmed().isEmpty()
             || profiles.contains(config.stationProfileName) )
            continue;

        config.enabled = false;
        missingProfiles.append(config.stationProfileName);
    }

    if ( missingProfiles.isEmpty() )
        return;

    LogParam::setAdifRecoveryFiles(configs);
    reloadSettings();

    const QString text = tr("Startup ADI was disabled for %n file(s) because the assigned Station Profile no longer exists.",
                            "", missingProfiles.count());
    qCWarning(runtime) << text << missingProfiles;
    emit problem(text);
}

void AdifRecoveryManager::rebuildPendingQueue(const QSet<QString> &profiles)
{
    pendingKeys.clear();

    for ( const AdifRecoveryConfig &config : static_cast<const QList<AdifRecoveryConfig>&>(configs) )
    {
        if ( !isRunnableConfig(config, profiles) )
            continue;

        pendingKeys.enqueue(AdifRecovery::fileKey(config.path, config.stationProfileName));
    }
}

LogFormat::duplicateQSOBehaviour AdifRecoveryManager::skipAllDuplicates(QSqlRecord *, QSqlRecord *)
{
    return LogFormat::SKIP_ALL;
}
