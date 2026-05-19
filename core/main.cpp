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

#include "debug.h"
#include "Migration.h"
#include "ui/MainWindow.h"
#include "rig/Rig.h"
#include "rotator/Rotator.h"
#include "cwkey/CWKeyer.h"
#include "AppGuard.h"
#include "core/zonedetect.h"
#include "ui/SplashScreen.h"
#include "core/MembershipQE.h"
#include "service/kstchat/KSTChat.h"
#include "data/Data.h"
#include "service/GenericCallbook.h"
#include "core/LogDatabase.h"

MODULE_IDENTIFICATION("qlog.core.main");

static QMutex debug_mutex;
static QFile debugLogFile;
static QTextStream debugLogStream;

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

static void startRigThread() {
    FCT_IDENTIFICATION;

    QThread* rigThread = new QThread;
    Rig* rig = Rig::instance();
    rig->moveToThread(rigThread);
    QObject::connect(rigThread, SIGNAL(started()), rig, SLOT(start()));
    rigThread->start();
}

static void startRotThread() {
    FCT_IDENTIFICATION;

    QThread* rotThread = new QThread;
    Rotator* rot = Rotator::instance();
    rot->moveToThread(rotThread);
    QObject::connect(rotThread, SIGNAL(started()), rot, SLOT(start()));
    rotThread->start();
}

static void startCWKeyerThread()
{
    FCT_IDENTIFICATION;

    QThread* cwKeyerThread = new QThread;
    CWKeyer* cwKeyer = CWKeyer::instance();
    cwKeyer->moveToThread(cwKeyerThread);
    QObject::connect(cwKeyerThread, SIGNAL(started()), cwKeyer, SLOT(start()));
    cwKeyerThread->start();
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

int main(int argc, char* argv[])
{

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

    // If started with --import-pending, wait a bit for the previous instance to fully terminate
    if ( isImportPending )
    {
        qCDebug(runtime) << "Start postponed";
        QCoreApplication::processEvents();
        QThread::msleep(1000);
    }

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
    if ( !guard.tryToRun() )
    {
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                              QMessageBox::tr("QLog is already running"));
        return 1;
    }

    QPixmap pixmap(":/res/qlog.png");
    SplashScreen splash(pixmap);

    splash.show();
    splash.ensureFirstPaint();

    createDataDirectory();

    // Process pending database import if exists
    if ( LogDatabase::hasPendingImport() )
    {
        splash.showMessage(QObject::tr("Importing Database"), Qt::AlignBottom|Qt::AlignCenter);
        QCoreApplication::processEvents();

        if ( !LogDatabase::instance()->processPendingImport() )
        {
            QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                                  QMessageBox::tr("Failed to process pending database import."));
            return 1;
        }

        if ( LogDatabase::instance()->hadPasswordImportWarning() )
        {
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 QMessageBox::tr("The database was imported successfully, but the stored passwords "
                                                 "could not be restored (decryption failed or the data is corrupted). "
                                                 "All service passwords have been cleared and must be re-entered in Settings."));
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

        splash.showMessage(QObject::tr("Backuping Database"), Qt::AlignBottom|Qt::AlignCenter);

        QCoreApplication::processEvents();

        /* a migration can break a database therefore a backup is call before it */
        if (!DBSchemaMigration::backupAllQSOsToADX())
        {
            QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                                  QMessageBox::tr("Could not export a QLog database to ADIF as a backup.<p>Try to export your log to ADIF manually"));
        }

        splash.showMessage(QObject::tr("Migrating Database"), Qt::AlignBottom|Qt::AlignCenter);

        QCoreApplication::processEvents();

        if ( ! LogDatabase::instance()->schemaVersionUpgrade(isForceLOVUpdate) )
        {
            QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                                  QMessageBox::tr("Database migration failed."));
            return 1;
        }
    }

    splash.showMessage(QObject::tr("Starting Application"), Qt::AlignBottom|Qt::AlignCenter);

    QCoreApplication::processEvents();

    startRigThread();
    startRotThread();
    startCWKeyerThread();

    MainWindow w;
    QIcon icon(":/res/qlog.png");

    w.setWindowIcon(icon);

    splash.finish(&w);
    w.show();

    w.setLayoutGeometry();

    // check version only for Windows and MacOS. Linux has own distribution points
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    w.checkNewVersion();
#endif
    return app.exec();
}
