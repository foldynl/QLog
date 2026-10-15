#include <QApplication>
#include <QMessageBox>
#include <QResource>
#include <QDebug>
#include <QTime>
#include <QSystemSemaphore>
#include <QSharedMemory>
#include <QMessageBox>
#include <QDebug>
#include <QSplashScreen>
#include <QTemporaryDir>
#include <QStyleFactory>
#include <QThread>
#include <QTimer>
#include <QEventLoop>
#include <QProgressDialog>

#include "debug.h"
#include "ui/MainWindow.h"
#include "rig/Rig.h"
#include "rotator/Rotator.h"
#include "cwkey/CWKeyer.h"
#include "AppGuard.h"
#include "3rdparty/bertoldvdb/zonedetect/library/zonedetect.h"
#include "ui/SplashScreen.h"
#include "core/MembershipQE.h"
#include "service/kstchat/KSTChat.h"
#include "data/Data.h"
#include "data/StationProfile.h"
#include "data/AntProfile.h"
#include "data/ActivityProfile.h"
#include "service/GenericCallbook.h"
#include "core/LogDatabase.h"
#include "core/LOVDatabase.h"
#include "core/DatabaseBackup.h"
#include "core/LogParam.h"
#include "mcpbridge/RuntimeContext.h"
#include "mcpbridge/RuntimeServer.h"

MODULE_IDENTIFICATION("qlog.core.main");

static QMutex debug_mutex;
static QFile debugLogFile;
static QTextStream debugLogStream;
static QThread *rigThreadHandle = nullptr;
static QThread *rotThreadHandle = nullptr;
static QThread *cwKeyerThreadHandle = nullptr;

QTemporaryDir tempDir
#ifdef QLOG_FLATPAK
// hack: I don't know how to openn image file
// in sandbox via QDesktop::openurl
// therefore QLog creates a temp directory in home directory (home is allowed for flatpak)
(QDir::homePath() + "/.qlogXXXXXX");
#else
;
#endif

static void setupTranslator(QApplication* app,
                            const QString &lang,
                            const QString &translationFile)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << lang << translationFile;

    QString localeLang = ( lang.isEmpty() ) ? QLocale::system().name()
                                            : lang;

    QString mapLanguage = localeLang;
    mapLanguage.replace('_', '-');
    mapLanguage = mapLanguage.section('-', 0, 0).toLower();
    if ( mapLanguage == QLatin1String("c") )
        mapLanguage = QStringLiteral("en");
    app->setProperty("qlogLanguage", mapLanguage);

    QTranslator* qtTranslator = new QTranslator(app);
    if ( qtTranslator->load("qt_" + localeLang,
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
                       QLibraryInfo::location(QLibraryInfo::TranslationsPath)) )
#else
                       QLibraryInfo::path(QLibraryInfo::TranslationsPath)) )
#endif

    {
        app->installTranslator(qtTranslator);
    }

    // give translators the ability to dynamically load files.
    // first, try to load file from input parameter (if exsist)
    if ( !translationFile.isEmpty() )
    {
        qCDebug(runtime) << "External translation file defined - trying to load it";
        QTranslator* translator = new QTranslator(app);
        if ( translator->load(translationFile) )
        {
            qCDebug(runtime) << "Loaded successfully"
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                             << translator->filePath()
#endif
                             ;
            app->installTranslator(translator);
            return;
        }
        qWarning() << "External translation file not found";
        translator->deleteLater();
    }

    // searching in the following directories
    // Linux:
    //    application_folder/i18n
    //    "~/.local/share/hamradio/QLog/i18n",
    //    "/usr/local/share/hamradion/QLog/i18n",
    //    "/usr/share/hamradio/QLog/i18n"
    //
    // looking for filename
    //     qlog.fr_ca.qm
    //     qlog.fr_ca
    //     qlog.fr.qm
    //     qlog.fr
    //     qlog.qm
    //     qlog

    QStringList translationFolders;

    translationFolders  << qApp->applicationDirPath()
                        << QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation);

    for ( const QString& folder : static_cast<const QStringList&>(translationFolders) )
    {
        qCDebug(runtime) << "Looking for a translation in" << folder << QString("i18n%1qlog_%2").arg(QDir::separator(), localeLang);
        QTranslator* translator = new QTranslator(app);
        if ( translator->load(QStringLiteral("i18n%1qlog_%2").arg(QDir::separator(), localeLang), folder) )
        {
            qCDebug(runtime) << "Loaded successfully"
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                             << translator->filePath()
#endif
                             ;
            app->installTranslator(translator);
            return;
        }
        translator->deleteLater();
    }

    // last attempt -  build-in resources/i18n.
    qCDebug(runtime) << "Looking for a translation in QLog's resources";
    QTranslator* translator = new QTranslator(app);
    if ( translator->load(":/i18n/qlog_" + localeLang) )
    {
        qCDebug(runtime) << "Loaded successfully"
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                         << translator->filePath()
#endif
                         ;
        app->installTranslator(translator);
        return;
    }

    translator->deleteLater();

    qCDebug(runtime) << "Cannot find any translation file";
}

static void createDataDirectory() {
    FCT_IDENTIFICATION;

    QDir dataDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    qCDebug(runtime) << dataDir.path();

    if (!dataDir.exists()) {
        dataDir.mkpath(dataDir.path());
    }
}

static void startRigThread(RuntimeContext &runtimeContext)
{
    FCT_IDENTIFICATION;

    rigThreadHandle = new QThread;
    Rig* rig = Rig::instance();
    rig->moveToThread(rigThreadHandle);
    if ( !runtimeContext.addProvider(QStringLiteral("rig"),
            QStringLiteral("Latest state of the transceiver controlled by QLog: whether the radio "
                           "driver has a ready connection, active radio-control profile and "
                           "backend (Hamlib, OmniRig, FLRig or TCI), configured Hamlib radio "
                           "model/driver label when known, main VFO frequency and label, adjusted "
                           "RX/TX frequencies, manual RX/TX profile offsets, "
                           "operating mode, receiver filter width, RF power setting, CAT CW keying "
                           "speed, CAT RIT/XIT offsets and split state. Use for questions about "
                           "the connected radio, tuned "
                           "amateur band, mode, power, tuning offsets or split operation. "
                           "Frequencies, offsets and filter width are in hertz, power in watts "
                           "and CW speed in WPM; read the field schema for precise meanings. "
                           "Values are the latest held by QLog and can change at any time; each "
                           "current-state question needs a new read, without hardware polling. "
                           "Readings depend on the backend, radio model and enabled profile "
                           "settings. Null means disconnected, unsupported, disabled or not yet "
                           "reported; profile offsets are configuration values, where zero means "
                           "no offset. Profile offsets are distinct from CAT RIT/XIT, although "
                           "QLog may label them RIT/XIT in its rig panel. Power is a setting "
                           "(estimated for TCI), not an RF meter reading. RX/TX values already "
                           "include the driver's adjustments; do not add offsets again. During "
                           "split, TX is the separate VFO report; with unknown split state, TX "
                           "is QLog's main-VFO value and does not verify actual radio split. "
                           "This source "
                           "does not report PTT, antenna, external keyer state, historical QSOs "
                           "or logging-station identity."),
            rig->getRuntimeProvider()) )
        qCWarning(runtime) << "Cannot register radio runtime provider";
    QObject::connect(rigThreadHandle, SIGNAL(started()), rig, SLOT(start()));
    rigThreadHandle->start();
}

static void startRotThread(RuntimeContext &runtimeContext)
{
    FCT_IDENTIFICATION;

    rotThreadHandle = new QThread;
    Rotator* rot = Rotator::instance();
    rot->moveToThread(rotThreadHandle);
    if ( !runtimeContext.addProvider(QStringLiteral("rotator"),
            QStringLiteral("Latest state of the antenna rotator controlled by QLog: ready driver "
                           "connection, rotator-control profile, backend (Hamlib or PSTRotator), "
                           "configured Hamlib model/driver label when known, and reported antenna "
                           "azimuth and elevation in degrees. Use for questions about where the "
                           "antenna is pointing, its elevation, or the rotator connection. "
                           "Azimuth is clockwise from north in [0, 360), with the antenna-profile "
                           "offset already applied; do not add it again. These are last reported "
                           "positions, not requested targets, movement status, or proof that a "
                           "target has been reached. PSTRotator connection means the UDP interface "
                           "is open, not verified physical hardware readiness. Axes can arrive "
                           "separately; null means disconnected, unsupported, not yet reported "
                           "or invalid, while zero can be a valid reading. Values can change at "
                           "any time; make a new read for each current-state question. Reading "
                           "this source does not poll or move the hardware. This source does "
                           "not provide antenna identity, radio state or historical QSO bearings; "
                           "read its field schema for exact meanings and limitations."),
            rot->getRuntimeProvider()) )
        qCWarning(runtime) << "Cannot register rotator runtime provider";
    QObject::connect(rotThreadHandle, &QThread::started, rot, &Rotator::start);
    rotThreadHandle->start();
}

static void startCWKeyerThread()
{
    FCT_IDENTIFICATION;

    cwKeyerThreadHandle = new QThread;
    CWKeyer* cwKeyer = CWKeyer::instance();
    cwKeyer->moveToThread(cwKeyerThreadHandle);
    QObject::connect(cwKeyerThreadHandle, SIGNAL(started()), cwKeyer, SLOT(start()));
    cwKeyerThreadHandle->start();
}

static void stopWorkerThread(QThread *&threadHandle, const QString &threadName)
{
    FCT_IDENTIFICATION;

    if ( !threadHandle )
        return;

    threadHandle->quit();
    if ( !threadHandle->wait(5000) )
    {
        qCWarning(runtime) << threadName << "thread did not stop within timeout";
        return;
    }

    delete threadHandle;
    threadHandle = nullptr;
}

void closeDebugLogFile()
{
    QMutexLocker locker(&debug_mutex);

    if ( debugLogFile.isOpen() )
    {
        debugLogStream.flush();
        debugLogStream.setDevice(nullptr);
        debugLogFile.close();
    }
}

static void debugMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker locker(&debug_mutex);

    if ( isLogToFileEnabled() && !debugLogFile.isOpen() )
    {
        const QString filename = Data::debugFilename();
        debugLogFile.setFileName(filename);

        if ( debugLogFile.open(QIODevice::WriteOnly | QIODevice::Text) )
        {
            setCurrentDebugLogFilename(filename);
            debugLogStream.setDevice(&debugLogFile);
            debugLogStream << "App: " << QCoreApplication::applicationVersion() << "\n"
#ifdef QLOG_FLATPAK
                           << "Flatpak" << "\n"
#endif
                           << "QT: " << qVersion() << "\n"
                           << "OS: " << QString("%1 %2 (%3)").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(), QGuiApplication::platformName() ) << "\n"
                           << "SSL: " << QSslSocket::sslLibraryVersionString() << "\n\n";
        }
        else
        {
            qWarning() << "Cannot open the file for log";
            setLogToFile(false);
        }
    }

    const char *severity_string = nullptr;
    switch ( type )
    {
    case QtDebugMsg:    severity_string = "[DEBUG   ]"; break;
    case QtInfoMsg:     severity_string = "[INFO    ]"; break;
    case QtWarningMsg:  severity_string = "[WARNING ]"; break;
    case QtCriticalMsg: severity_string = "[CRITICAL]"; break;
    case QtFatalMsg:    severity_string = "[FATAL   ]"; break;
    default:            severity_string = "[UNKNOWN ]";
    }

    const QString &category = QString("[%1]").arg(context.category).leftJustified(50, ' ');

    const QString &logEntry = QString("%1 %2 [0x%3] %4 %5 [%6:%7:%8]\n")
                           .arg(QTime::currentTime().toString("HH:mm:ss.zzz"))
                           .arg(severity_string)
                           .arg(QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16))
                           .arg(category)
                           .arg(msg)
                           .arg(context.function ? context.function : "unknown")
                           .arg(context.file ? context.file : "unknown")
                           .arg(context.line);

    if ( isLogToFileEnabled() && debugLogFile.isOpen() )
    {
        debugLogStream << logEntry;
        debugLogStream.flush();
    }

    fprintf(stderr, "%s", logEntry.toUtf8().constData());

    if ( type == QtFatalMsg )
    {
        if (debugLogFile.isOpen()) debugLogFile.close();
        abort();
    }
}

#ifdef Q_OS_LINUX
void wayland_hacks()
{
    // due to QT's issue, Dock widget is not working (cannot be docked) under QT5, < ?6.7? on Linux
    // Therefore it is necessary to force set XCB (X11)
    const QByteArray &sessionType = qgetenv("XDG_SESSION_TYPE").toLower();
    const QByteArray &disableXCBFallback = qgetenv("QLOG_DISABLE_XCB");
    if ( sessionType.contains("wayland")
         && disableXCBFallback == QByteArray() )
    {
        qInfo() << "Force XCB";
        qputenv("QT_QPA_PLATFORM", "xcb");
    }
}
#endif

static QString lovName(LOVDownloader::SourceType type)
{
    FCT_IDENTIFICATION;

    switch ( type )
    {
    case LOVDownloader::CTY: return QObject::tr("DXCC Entities");
    case LOVDownloader::CLUBLOGCTY: return QObject::tr("ClubLog DXCC");
    case LOVDownloader::SATLIST: return QObject::tr("Sats Info");
    case LOVDownloader::SOTASUMMITS: return QObject::tr("SOTA Summits");
    case LOVDownloader::WWFFDIRECTORY: return QObject::tr("WWFF Records");
    case LOVDownloader::IOTALIST: return QObject::tr("IOTA Records");
    case LOVDownloader::POTADIRECTORY: return QObject::tr("POTA Records");
    case LOVDownloader::MEMBERSHIPCONTENTLIST: return QObject::tr("Membership Directory Records");
    default: return QObject::tr("List of Values");
    }
}

static bool initializeLOV(LOVDownloader &downloader, SplashScreen &splash, bool force)
{
    FCT_IDENTIFICATION;

    const QDir directory(LogDatabase::dbDirectory());
    QStringList failed;

    for ( const LOVDownloader::SourceType type : LOVDownloader::sources() )
    {
        const LOVDownloader::SourceDefinition source = LOVDownloader::sourceDefinition(type);
        const QString pending = directory.filePath(source.packageStem + QStringLiteral(".pending.db"));

        for ( const QString &suffix : {QString(), QStringLiteral("-journal"), QStringLiteral("-wal"), QStringLiteral("-shm")} )
        {
            if ( QFile::exists(pending + suffix) && !QFile::remove(pending + suffix) )
                qCWarning(runtime) << "Cannot remove unfinished reference list" << pending + suffix;
        }

        splash.showMessage(QObject::tr("Updating %1").arg(lovName(type)), Qt::AlignBottom|Qt::AlignCenter);

        QString error;
        LOVDatabase::ApplyResult adopted = LOVDatabase::applyReady(source, directory.path(), &error);

        if ( adopted == LOVDatabase::ApplyResult::DATABASE_ERROR )
            return false;

        if ( adopted == LOVDatabase::ApplyResult::INVALID )
        {
            qCWarning(runtime) << "Invalid reference-list package" << source.type << error;
            if ( !QFile::remove(directory.filePath(source.packageStem + QStringLiteral(".ready.db"))) )
                qCWarning(runtime) << "Cannot remove invalid reference-list package" << source.type;
        }

        if ( adopted == LOVDatabase::ApplyResult::RETRY )
        {
            failed << lovName(type);
            continue;
        }

        if ( !force && downloader.isSourceAvailable(type) )
            continue;

        QProgressDialog progress;
        progress.setWindowTitle(QObject::tr("Updating External Resources"));
        progress.setLabelText(QObject::tr("Updating %1…").arg(lovName(type)));
        progress.setAutoClose(false);
        progress.setAutoReset(false);
        progress.setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);
        progress.setModal(true);
        progress.setRange(0, 0);
        progress.show();
        QEventLoop wait;
        bool prepared = false;
        QObject::connect(&downloader, &LOVDownloader::processingSize, &progress, &QProgressDialog::setMaximum);
        QObject::connect(&downloader, &LOVDownloader::progress, &progress, &QProgressDialog::setValue);
        QObject::connect(&downloader, &LOVDownloader::finished, &wait, [&](bool success)
        {
            prepared = success;
            wait.quit();
        });
        QObject::connect(&downloader, &LOVDownloader::noUpdate, &wait, [&]()
        {
            prepared = true;
            wait.quit();
        });
        QObject::connect(&progress, &QProgressDialog::canceled, &downloader, &LOVDownloader::abortRequest);

        QTimer::singleShot(0, &downloader, [&downloader, type, force]() { downloader.update(type, force); });
        // Startup needs an event loop for networking. Unlike dialog.exec(), cancellation
        // must wait for the worker's terminal result before the next source or shared Data.
        wait.exec();
        progress.close();
        adopted = prepared ? LOVDatabase::applyReady(source, directory.path(), &error) : LOVDatabase::ApplyResult::RETRY;

        if ( adopted == LOVDatabase::ApplyResult::DATABASE_ERROR )
            return false;

        if ( adopted != LOVDatabase::ApplyResult::APPLIED && adopted != LOVDatabase::ApplyResult::NO_PACKAGE )
            failed << lovName(type);
    }
    if ( !failed.isEmpty() )
        QMessageBox::warning(nullptr, QObject::tr("QLog Warning"),
                             QObject::tr("Could not update these reference lists: %1. Existing data have been kept.").arg(failed.join(QStringLiteral(", "))));
    return true;
}

int main(int argc, char* argv[])
{
    FCT_IDENTIFICATION;

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

#ifdef Q_OS_LINUX
    wayland_hacks();
#endif

    bool stylePresent = false;

    /* Style option is deleted in QApplication constructor.
     * Therefore test for the parameter presence has to be performed here
     */
    for ( int i = 0; i < argc && !stylePresent; i++ )
    {
        stylePresent = QString(argv[i]).contains("-style");
    }

    QApplication app(argc, argv);
    app.setApplicationVersion(VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::tr("QLog Help"));
    parser.addHelpOption();
    parser.addVersionOption();

    /* Undocumented param used for debugging. A developer can run QLog with
     * a specific namespace. This helps in cases where it is possible
     * to simultaneously run a test/develop and production versions of QLog.
     *
     * The parameter changes the Application name. It causes that all runtime
     * files, settings, passwords and DB file are created in a different directory/namespace.
     *
     * however, it remains necessary only one instance of QLog to run at a time.
     * More Notes below (AppGuard Comment).
     *
     * NOTE: This is not a preparation for the ability to run separate databases.
     * It's just to make it easier for developers and testers.
     */

    QCommandLineOption environmentName(QStringList() << "n" << "namespace",
                QCoreApplication::translate("main", "Run with the specific namespace."),
                QCoreApplication::translate("main", "namespace"));
    QCommandLineOption translationFilename(QStringList() << "t" << "translation",
                QCoreApplication::translate("main", "Translation file - absolute or relative path and QM file name."),
                QCoreApplication::translate("main", "path/QM-filename"));
    QCommandLineOption forceLanguage(QStringList() << "l" << "language",
                QCoreApplication::translate("main", "Set language. <code> example: 'en' or 'en_US'. Ignore environment setting."),
                QCoreApplication::translate("main", "code"));
    QCommandLineOption debugFile(QStringList() << "d" << "debug",
                QCoreApplication::translate("main", "Writes debug messages to the debug file"));
    QCommandLineOption importPending("import-pending",
                QCoreApplication::translate("main", "Process pending database import (internal use)"));
    QCommandLineOption forceLOVUpdate(QStringList() << "f" << "force-update",
                QCoreApplication::translate("main", "Force update of all value lists (DXCC, SATs, etc.)"));

    parser.addOption(environmentName);
    parser.addOption(translationFilename);
    parser.addOption(forceLanguage);
    parser.addOption(debugFile);
    parser.addOption(importPending);
    parser.addOption(forceLOVUpdate);

    parser.process(app);
    QString environment = parser.value(environmentName);
    QString translation_file = parser.value(translationFilename);
    QString lang = parser.value(forceLanguage);
    setLogToFile(parser.isSet(debugFile));
    bool isImportPending = parser.isSet(importPending);
    bool isForceLOVUpdate = parser.isSet(forceLOVUpdate);

    app.setOrganizationName("hamradio");
    app.setApplicationName("QLog" + ((environment.isEmpty()) ? "" : environment.prepend("-")));

    /* If the Style parameter is not present then use a default - Fusion style */
    if ( !stylePresent )
    {
        app.setStyle(QStyleFactory::create("Fusion"));
    }

    qInstallMessageHandler(debugMessageOutput);
    qRegisterMetaType<VFOID>();
    qRegisterMetaType<ClubStatusQuery::ClubInfo>();
    qRegisterMetaType<QMap<QString, ClubStatusQuery::ClubInfo>>();
    qRegisterMetaType<KSTChatMsg>();
    qRegisterMetaType<KSTUsersInfo>();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    qRegisterMetaTypeStreamOperators<QSet<int>>("QSet<int>");
#endif
    qRegisterMetaType<DxSpot>();
    qRegisterMetaType<BandPlan::BandPlanMode>();
    qRegisterMetaType<SpotAlert>();
    qRegisterMetaType<Rig::Status>();
    qRegisterMetaType<Band>();
    qRegisterMetaType<CallbookResponseData>();

    set_debug_level(LEVEL_PRODUCTION); // you can set more verbose rules via
                                       // environment variable QT_LOGGING_RULES (project setting/debug)

    setupTranslator(&app, lang, translation_file);

    /* Application Singleton
     *
     * Only one instance of QLog application is allowed
     *
     * It is always necessary to run only one QLog on the
     * system, because the FLDigi interface has a fixed port.
     * Therefore, in the case of two or more instances,
     * there is a port conflict.
     */
    AppGuard guard( "QLog" );
    if ( !guard.tryToRun(isImportPending ? 20000 : 0) )
    {
        const QString guardError = guard.errorString();
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                              guardError.isEmpty() ? QMessageBox::tr("QLog is already running") : guardError);
        return 1;
    }

    QPixmap pixmap(":/res/qlog.png");
    SplashScreen splash(pixmap);

    splash.show();
    splash.ensureFirstPaint();

    createDataDirectory();
    QString backupSnapshot;

    // Process pending database import if exists
    if ( LogDatabase::hasPendingImport() )
    {
        splash.showMessage(QObject::tr("Importing Database"), Qt::AlignBottom|Qt::AlignCenter);
        QCoreApplication::processEvents();

        if ( !LogDatabase::instance()->processPendingImport(&backupSnapshot) )
        {
            QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                                  LogDatabase::instance()->pendingImportError());
            return 1;
        }
    }
    else
    {
        splash.showMessage(QObject::tr("Opening Database"), Qt::AlignBottom|Qt::AlignCenter);

        QCoreApplication::processEvents();

        if ( ! LogDatabase::instance()->openDatabase() )
        {
            QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                                  QMessageBox::tr("Could not connect to database."));
            return 1;
        }

        splash.showMessage(QObject::tr("Migrating Database"), Qt::AlignBottom|Qt::AlignCenter);

        QCoreApplication::processEvents();

        if ( ! LogDatabase::instance()->schemaVersionUpgrade(&backupSnapshot) )
        {
            QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                                  QMessageBox::tr("Database migration failed."));
            return 1;
        }
    }

    LOVDownloader lovDownloader;
    if ( !initializeLOV(lovDownloader, splash, isForceLOVUpdate) )
    {
        QMessageBox::critical(nullptr, QObject::tr("QLog Error"), QObject::tr("Could not complete reference-list initialization."));
        return 1;
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &lovDownloader, &LOVDownloader::stopUpdates);

    splash.showMessage(QObject::tr("Starting Application"), Qt::AlignBottom|Qt::AlignCenter);

    QCoreApplication::processEvents();

    int rc = 0;
    DatabaseBackup backup(LogParam::getBackupConfig(), backupSnapshot, &app);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &backup, &QThread::requestInterruption);
    {
        RuntimeContext runtimeContext;
        if ( !runtimeContext.addProvider(QStringLiteral("station"),
                                         QStringLiteral("Currently selected local logging-station profile: "
                                                        "station callsign, Maidenhead locator, operator name "
                                                        "and callsign, QTH, country, county, DXCC entity, CQ/ITU "
                                                        "zones, IOTA/POTA/SOTA/WWFF references, special-interest "
                                                        "activity, VUCC boundary grids and DARC DOK. Use for "
                                                        "questions about my station, who I am operating as, "
                                                        "my configured location or activation references. "
                                                        "These are profile settings, not GPS observations, "
                                                        "contact data, or proof of an activation. Historical "
                                                        "QSOs may use different station information. Profile "
                                                        "selection and contents can change while QLog runs; "
                                                        "read anew for each current-state question. Unset "
                                                        "values or no selected profile produce null fields."),
                                         StationProfilesManager::instance()->getRuntimeProvider()) )
            qCWarning(runtime) << "Cannot register station runtime provider";
        if ( !runtimeContext.addProvider(QStringLiteral("antenna"),
                                         QStringLiteral("Currently selected local antenna profile: name, "
                                                        "user description, configured bands, horizontal "
                                                        "beamwidth, signed azimuth offset and assigned "
                                                        "rotator-control profile. Use for questions about "
                                                        "my selected antenna, its settings or why antenna "
                                                        "bearing differs from the raw rotator display. "
                                                        "Read antenna and rotator together for their relation: "
                                                        "rotator_azimuth already includes the antenna offset "
                                                        "applied when the last position report arrived; never "
                                                        "apply it again. Assignment is configuration; an "
                                                        "Activity Rotator Override can choose a different "
                                                        "rotator. rotator_profile and rotator_connected report "
                                                        "the actual connection. Selecting an antenna does not "
                                                        "verify physical connection, RF switching or tuning. "
                                                        "Profiles and settings can change while QLog runs; "
                                                        "read anew for each current-state question. Antenna "
                                                        "settings and rotator reports are collected from "
                                                        "different owners, not one atomic hardware snapshot."),
                                         AntProfilesManager::instance()->getRuntimeProvider()) )
            qCWarning(runtime) << "Cannot register antenna runtime provider";
        if ( !runtimeContext.addProvider(QStringLiteral("activity"),
                                         QStringLiteral("Currently selected Activity preset: its name, "
                                                        "requested station, rig and antenna profiles, "
                                                        "Rotator Override and rig/rotator connection "
                                                        "requests. Use to understand my operating setup "
                                                        "and why QLog selects particular equipment. These "
                                                        "are preset instructions applied on activation, "
                                                        "not confirmation of actual connection or an "
                                                        "ongoing contact. Compare with station, antenna, "
                                                        "rig and rotator for actual state; manual changes "
                                                        "or asynchronous setup can make them differ. A "
                                                        "Rotator Override takes priority over the antenna's "
                                                        "assigned rotator. Empty profile names explicitly "
                                                        "select None; null means unspecified or no Activity. "
                                                        "The antenna's offset still corrects rotator_azimuth; "
                                                        "never apply it twice. Selection and preset contents "
                                                        "can change while QLog runs; read anew for each "
                                                        "current-state question. Sources are collected in "
                                                        "their owners' threads, not one atomic snapshot."),
                                         ActivityProfilesManager::instance()->getRuntimeProvider()) )
            qCWarning(runtime) << "Cannot register activity runtime provider";

        startRigThread(runtimeContext);
        startRotThread(runtimeContext);
        startCWKeyerThread();

        RuntimeServer runtimeServer(runtimeContext);
        runtimeServer.listen();

        MainWindow w;
        QIcon icon(":/res/qlog.png");

        w.setWindowIcon(icon);

        splash.finish(&w);
        w.show();

        w.setLayoutGeometry();

        QObject::connect(&backup, &QThread::finished, &w, &MainWindow::updateBackupStatus);
        backup.start();
        QTimer::singleShot(0, &lovDownloader, &LOVDownloader::startUpdates);

        // check version only for Windows and MacOS. Linux has own distribution points
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
        w.checkNewVersion();
#endif
        rc = app.exec();
    }

    stopWorkerThread(cwKeyerThreadHandle, "CWKeyer");
    stopWorkerThread(rotThreadHandle, "Rotator");
    stopWorkerThread(rigThreadHandle, "Rig");

    return rc;
}
