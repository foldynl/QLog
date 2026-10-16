#include <QtTest>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "core/LogParam.h"
#include "core/PasswordCipher.h"
#include "core/CredentialStore.h"
#include "core/Migration.h"
#include "core/DatabaseBackup.h"
#include <QApplication>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>
#include <QScopedValueRollback>
#include "../CredentialStoreImportTest/fakes/KeychainJobs.h"
#ifdef Q_OS_UNIX
#include <sys/resource.h>
#include <signal.h>
#endif

#include "core/LogDatabase.h"
#include "core/PlatformParameterManager.h"
#include "service/clublog/ClubLog.h"

const QString ClubLogBase::API_KEY = "test";

QMap<QPair<QString, QString>, QString> QKeychain::Job::passwords;
QKeychain::Job::Operation QKeychain::Job::failedOperation = QKeychain::Job::READ;
QString QKeychain::Job::failedKey;

QString configuredAccount;
QStringList configuredDxAccounts;

class MigrationSqlTest_FriendAccessor
{
public:
    static bool migrate(DBSchemaMigration &migration, int version)
    {
        return migration.migrate(version);
    }
};

#ifdef IMPORT_INTERRUPTION_WRAPPERS
#include <unistd.h>
QString interruptionBoundary;

// Stop after real durable operations, without production hooks or simulated SQL.
extern "C" bool __real__ZN9QSaveFile6commitEv(QSaveFile *file);
extern "C" bool __wrap__ZN9QSaveFile6commitEv(QSaveFile *file)
{
    const bool result = __real__ZN9QSaveFile6commitEv(file);
    if ( result && !interruptionBoundary.isEmpty() )
    {
        QString boundary;
        if ( file->fileName() == LogDatabase::pendingImportPath() )
            boundary = "CANDIDATE_STAGED";
        else if ( file->fileName() == LogDatabase::pendingImportPath() + ".params" )
        {
            QFile record(file->fileName());
            if ( record.open(QIODevice::ReadOnly) )
                boundary = QJsonDocument::fromJson(record.readAll()).object().value("phase").toString();
        }
        if ( boundary == interruptionBoundary )
            _exit(91);
    }
    return result;
}

extern "C" bool __real__ZN5QFile6renameERK7QStringS2_(const QString &source, const QString &target);
extern "C" bool __wrap__ZN5QFile6renameERK7QStringS2_(const QString &source, const QString &target)
{
    if ( interruptionBoundary == "FAIL_PUBLICATION" && source == LogDatabase::pendingImportPath() )
        return false;
    if ( interruptionBoundary == "FAIL_BACKUP_PUBLICATION" && source.endsWith(".db.tmp") && target.endsWith(".db") )
        return false;
    const bool result = __real__ZN5QFile6renameERK7QStringS2_(source, target);
    if ( result && ((interruptionBoundary == "ORIGINAL_MOVED" && source == LogDatabase::dbFilename())
                   || (interruptionBoundary == "DATABASE_PUBLISHED" && source == LogDatabase::pendingImportPath())
                   || (interruptionBoundary == "CANCEL_CANDIDATE_MOVED" && target == LogDatabase::pendingImportPath())
                   || (interruptionBoundary == "CANCEL_ORIGINAL_RESTORED" && source == LogDatabase::dbDirectory().filePath("qlog.db.preimport"))) )
        _exit(91);
    return result;
}

extern "C" bool __real__ZN5QFile6removeERK7QString(const QString &filename);
extern "C" bool __wrap__ZN5QFile6removeERK7QString(const QString &filename)
{
    const bool result = __real__ZN5QFile6removeERK7QString(filename);
    if ( result && interruptionBoundary == "ORIGINAL_REMOVED"
         && filename == LogDatabase::dbDirectory().filePath("qlog.db.preimport") )
        _exit(91);
    return result;
}
#endif

class DatabaseImportTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void inspectionRequiresExportSchema_data();
    void inspectionRequiresExportSchema();
    void inspectionDoesNotCreateMissingFile();
    void emptyAndSamePlatformImportsReplaceMetadata_data();
    void emptyAndSamePlatformImportsReplaceMetadata();
    void importProgressRecordCompatibility_data();
    void importProgressRecordCompatibility();
    void preparationWriteFailurePreservesOriginal();
    void orphanCandidatePreservesOriginal();
    void preparationFailurePreservesOriginal();
    void candidateFailureDoesNotReplaceOriginal();
    void candidateFailureDoesNotReplaceOriginal_data();
    void publishedImportRetryPreservesNewData();
    void deferredImportDoesNotOverwriteManualPassword();
    void deferredImportChecksSchema_data();
    void deferredImportChecksSchema();
    void deferredImportPreparesRoutineBackup();
    void readyImportChecksSchema_data();
    void readyImportChecksSchema();
    void finishWithoutPasswords_data();
    void finishWithoutPasswords();
    void skippedImportCleanupCanBeRetried();
    void interruptedPasswordAbandonmentIsRecoverable();
    void finishWithoutPasswordsRejectsMismatchedDatabase();
    void stoppingRecoveryPreservesDataOnWriteFailure();
    void databaseSwitchClearsSettingsCache();
    void schemaCheckDoesNotPrepareRoutineBackup_data();
    void schemaCheckDoesNotPrepareRoutineBackup();
    void startupWithoutImportInitializesDatabase();
    void firstStartupKeepsFirstQsoBackupDue();
    void passwordActionsReuseInitializedConnection();
    void passwordRecoveryRequiresOpenActiveDatabase_data();
    void passwordRecoveryRequiresOpenActiveDatabase();
    void backupPreparationFailureControlsStartup_data();
    void backupPreparationFailureControlsStartup();
    void cancelledImportIsNeverAdopted();
    void credentialReplacementUsesOriginalAccounts();
    void credentialCaseChangesKeepImportedPassword_data();
    void credentialCaseChangesKeepImportedPassword();
    void unfinishedImportCannotOverwriteRecoveryCredentials();
    void interruptedPublicationIsRecoverable_data();
    void interruptedPublicationIsRecoverable();
    void missingPassphrasePreservesRecovery();
    void cancelledImportCleanupCanBeRetried();
    void unpublishedImportCanBeCancelled_data();
    void unpublishedImportCanBeCancelled();
    void interruptedCancellationIsRecoverable_data();
    void interruptedCancellationIsRecoverable();
    void cancellationPreservesFilesWhenOriginalIsMissing();
    void abortedImportReportsOriginalOpenFailure();
    void cancellationRecordFailurePreventsAdoption();
    void migratedArchivePreservesData_data();
    void migratedArchivePreservesData();
    void colocatedPreparationAvoidsDatabaseCopy();
    void publicationFailureRestoresOriginal();
    void contradictoryRecoveryFilesArePreserved();

private:
    static QPair<QString, QString> credentialIdentity(const QString &storageKey, const QString &user);
    QScopedPointer<QTemporaryDir> directory;
    QString createExport(const QString &name, const QByteArray &credentials = QByteArray(),
                         int version = DBSchemaMigration::latestVersion);
    void createDatabase(const QString &filename, int version);
    QByteArray readFile(const QString &path);
    bool runFixture(const QStringList &arguments, int exitCode);
    void setImportPhase(const QString &phase);
};

void DatabaseImportTest::publicationFailureRestoresOriginal()
{
#ifdef IMPORT_INTERRUPTION_WRAPPERS
    const QString source = createExport("source.db");
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(runFixture({"--make-wal", LogDatabase::dbFilename()}, 91));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, {}));
    QVERIFY(runFixture({"--recover", "FAIL_PUBLICATION"}, 1));
    QVERIFY(QFile::exists(LogDatabase::pendingImportPath()));
    QVERIFY(!QFile::exists(LogDatabase::dbDirectory().filePath("qlog.db.preimport")));
    QVERIFY(LogDatabase::instance()->openDatabase());
    {
        QSqlQuery query("SELECT callsign FROM contacts WHERE id = 9");
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QString("WAL-QSO"));
    }
    cleanup();
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::COMPLETE);
    QVERIFY(!LogDatabase::hasPendingImport());
#else
    QSKIP("Requires a linker wrapper for a deterministic publication failure.");
#endif
}

void DatabaseImportTest::contradictoryRecoveryFilesArePreserved()
{
#ifdef IMPORT_INTERRUPTION_WRAPPERS
    const QString source = createExport("source.db");
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    const QByteArray original = readFile(LogDatabase::dbFilename());
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, {}));
    QVERIFY(runFixture({"--recover", "READY"}, 91));
    const QString backup = LogDatabase::dbDirectory().filePath("qlog.db.preimport");
    QVERIFY(QFile::copy(source, backup));
    const QByteArray candidate = readFile(LogDatabase::pendingImportPath());
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::FAILED);
    QCOMPARE(readFile(LogDatabase::dbFilename()), original);
    QCOMPARE(readFile(backup), original);
    QCOMPARE(readFile(LogDatabase::pendingImportPath()), candidate);
#else
    QSKIP("Requires interruption after the actual READY boundary.");
#endif
}

void DatabaseImportTest::colocatedPreparationAvoidsDatabaseCopy()
{
#ifdef Q_OS_UNIX
    const QString source = createExport("source.db");
    const QString colocated = LogDatabase::dbDirectory().filePath("qlog-import-source.db");
    QVERIFY(QFile::copy(source, colocated));
    struct rlimit originalLimit;
    QVERIFY(getrlimit(RLIMIT_FSIZE, &originalLimit) == 0);
    struct rlimit restrictedLimit = originalLimit;
    restrictedLimit.rlim_cur = 1024;
    const auto previousSignal = signal(SIGXFSZ, SIG_IGN);
    const int restricted = setrlimit(RLIMIT_FSIZE, &restrictedLimit);
    bool result = false;
    if ( restricted == 0 )
        result = LogDatabase::instance()->preparePendingImport(colocated, {}, {}, {});
    const int restored = setrlimit(RLIMIT_FSIZE, &originalLimit);
    signal(SIGXFSZ, previousSignal);
    QVERIFY(restricted == 0);
    QVERIFY(restored == 0);
    QVERIFY(result);
    QVERIFY(!QFile::exists(colocated));
    QCOMPARE(readFile(LogDatabase::pendingImportPath()), readFile(source));
#else
    QSKIP("Requires the Unix file-size limit to detect an unnecessary DB copy.");
#endif
}

void DatabaseImportTest::createDatabase(const QString &filename, int version)
{
    QVERIFY(LogDatabase::instance()->openDatabase(filename));
    QSqlQuery query;
    QVERIFY(query.exec("PRAGMA foreign_keys = OFF"));
    DBSchemaMigration migration;
    if ( version == DBSchemaMigration::latestVersion )
        QVERIFY(migration.run());
    else
        for ( int step = 1; step <= version; ++step )
            QVERIFY2(MigrationSqlTest_FriendAccessor::migrate(migration, step), qPrintable(QString::number(step)));
    QVERIFY(query.exec("PRAGMA foreign_keys = ON"));
    // These files model standalone archives. WAL scenarios enable WAL through openDatabase().
    QVERIFY(query.exec("PRAGMA journal_mode = DELETE"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("delete"));
}

void DatabaseImportTest::migratedArchivePreservesData_data()
{
    QTest::addColumn<int>("version");
    QTest::newRow("current") << int(DBSchemaMigration::latestVersion);
    QTest::newRow("previous") << DBSchemaMigration::latestVersion - 1;
    QTest::newRow("legacy42") << 42;
}

void DatabaseImportTest::migratedArchivePreservesData()
{
    QFETCH(int, version);
    const QString source = directory->filePath("real-schema.db");
    createDatabase(source, version);
    if ( QTest::currentTestFailed() )
        return;
    {
        QSqlQuery query;
        QVERIFY(query.exec("INSERT INTO contacts (callsign) VALUES ('OK1REAL')"));
        QVERIFY(query.exec("INSERT INTO rig_profiles (profile_name, model, port_pathname) VALUES ('Test rig', 1, 'old port')"));
    }
    QVERIFY(LogParam::setSourcePlatform("Windows"));
    cleanup();
    const QList<PlatformParameter> parameters {
        {"services/lotw/callbook/tqsl", {}, {}, "/selected/tqsl", false}
    };
    const QList<ProfileParameter> profiles {
        {"rig_profiles", "Test rig", "port_pathname", {}, {}, "/selected/port", false}
    };
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, parameters, profiles, {}));
    QVERIFY2(LogDatabase::instance()->initialize() == LogDatabase::ImportResult::COMPLETE, qPrintable(LogDatabase::instance()->pendingImportError()));
    QCOMPARE(LogParam::getLoTWTQSLPath(QString()), QString("/selected/tqsl"));
    QSqlQuery query("SELECT callsign FROM contacts");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("OK1REAL"));
    QVERIFY(query.exec("SELECT port_pathname FROM rig_profiles WHERE profile_name = 'Test rig'"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("/selected/port"));
    QVERIFY(query.exec("SELECT MAX(version) FROM schema_versions"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), DBSchemaMigration::latestVersion);
    const QStringList snapshots = LogDatabase::dbDirectory().entryList({"qlog_backup_*.db"}, QDir::Files);
    QCOMPARE(snapshots.size(), 1);
    QCOMPARE(snapshots.first().endsWith(".migration.db"), version < DBSchemaMigration::latestVersion);
    QVERIFY(!LogDatabase::hasPendingImport());
}

bool DatabaseImportTest::runFixture(const QStringList &arguments, int exitCode)
{
    QProcess process;
    process.start(QCoreApplication::applicationFilePath(), arguments);
    if ( !process.waitForStarted(5000) || !process.waitForFinished(10000) )
        return false;
    if ( process.exitStatus() != QProcess::NormalExit || process.exitCode() != exitCode )
    {
        qWarning().noquote() << process.readAllStandardError() << process.readAllStandardOutput();
        return false;
    }
    return true;
}

void DatabaseImportTest::interruptedPublicationIsRecoverable_data()
{
    QTest::addColumn<QString>("boundary");
    QTest::addColumn<bool>("hadOriginal");
    for ( const QString &boundary : {QString("CANDIDATE_STAGED"), QString("PREPARED"), QString("READY"),
                                    QString("ORIGINAL_MOVED"), QString("DATABASE_PUBLISHED"),
                                    QString("PUBLISHED"), QString("CREDENTIALS_RESTORED"), QString("ORIGINAL_REMOVED")} )
    {
        QTest::newRow(qPrintable(boundary)) << boundary << true;
        if ( boundary != "ORIGINAL_MOVED" && boundary != "ORIGINAL_REMOVED" )
            QTest::newRow(qPrintable(boundary + "-first-install")) << boundary << false;
    }
}

void DatabaseImportTest::interruptedPublicationIsRecoverable()
{
#ifdef IMPORT_INTERRUPTION_WRAPPERS
    QFETCH(QString, boundary);
    QFETCH(bool, hadOriginal);
    const QString source = createExport("source.db");
    QVERIFY(!source.isEmpty());
    if ( hadOriginal )
    {
        QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
        QVERIFY(runFixture({"--make-wal", LogDatabase::dbFilename()}, 91));
        QVERIFY(QFileInfo(LogDatabase::dbFilename() + "-wal").size() > 0);
    }
    if ( boundary == "CANDIDATE_STAGED" || boundary == "PREPARED" )
        QVERIFY(runFixture({"--prepare", source, boundary}, 91));
    else
    {
        QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, {}));
        QVERIFY(runFixture({"--make-wal", LogDatabase::pendingImportPath()}, 91));
        QVERIFY(QFileInfo(LogDatabase::pendingImportPath() + "-wal").size() > 0);
        QVERIFY(runFixture({"--recover", boundary}, 91));
    }
    if ( boundary == "CANDIDATE_STAGED" )
    {
        QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::FAILED);
        QVERIFY(QFile::exists(LogDatabase::pendingImportPath()));
        QCOMPARE(QFile::exists(LogDatabase::dbFilename()), hadOriginal);
        return;
    }
    const QString backup = LogDatabase::dbDirectory().filePath("qlog.db.preimport");
    if ( hadOriginal && QFile::exists(backup) )
    {
        QVERIFY(LogDatabase::instance()->openDatabase(backup));
        QSqlQuery query("SELECT callsign FROM contacts WHERE id = 9");
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QString("WAL-QSO"));
    }
    cleanup();
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::COMPLETE);
    QVERIFY(!LogDatabase::hasPendingImport());
    QVERIFY(!QFile::exists(backup));
    QSqlQuery query("SELECT callsign FROM contacts WHERE id = 1");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("OK1TEST"));
    if ( boundary != "PREPARED" )
    {
        QVERIFY(query.exec("SELECT callsign FROM contacts WHERE id = 9"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QString("WAL-QSO"));
    }
#else
    QSKIP("Requires linker wrappers to interrupt actual file publication operations.");
#endif
}

void DatabaseImportTest::initTestCase()
{
#ifdef Q_OS_MACOS
    QSKIP("Disabled on macOS due to Apple SQLite WAL read-only and schema-write restrictions.");
#endif
    CredentialRegistry::instance().add("TestService", []()
    {
        QList<CredentialDescriptor> descriptors {
            {"TestService", []() { return configuredAccount; }}
        };
        for ( const QString &user : configuredDxAccounts )
            descriptors.append({"DX", [user]() { return user; }});
        return descriptors;
    });
}

void DatabaseImportTest::init()
{
    QCoreApplication::setApplicationName("DatabaseImportTest");
    QString directoryTemplate;
#ifndef Q_OS_LINUX
    // A unique organization isolates Windows/macOS data while preserving credential service names.
    QCoreApplication::setOrganizationName(QString());
    const QDir parent = QFileInfo(LogDatabase::dbDirectory().absolutePath()).dir();
    QVERIFY(QDir().mkpath(parent.absolutePath()));
    directoryTemplate = parent.filePath("DatabaseImportTest-XXXXXX");
#endif
    directory.reset(new QTemporaryDir(directoryTemplate));
    QVERIFY(directory->isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory->path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, directory->path());
#ifdef Q_OS_LINUX
    qputenv("XDG_DATA_HOME", directory->path().toUtf8());
    qputenv("XDG_CONFIG_HOME", directory->path().toUtf8());
#else
    QCoreApplication::setOrganizationName(QFileInfo(directory->path()).fileName());
    QCOMPARE(QFileInfo(LogDatabase::dbDirectory().absolutePath()).dir().absolutePath(), directory->path());
#endif
    QVERIFY(QDir().mkpath(LogDatabase::dbDirectory().path()));
    QKeychain::Job::passwords.clear();
    QKeychain::Job::failedKey.clear();
    configuredAccount.clear();
    configuredDxAccounts.clear();
}

void DatabaseImportTest::cleanup()
{
    if ( QSqlDatabase::contains() )
    {
        QSqlDatabase::database().close();
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }
}

QPair<QString, QString> DatabaseImportTest::credentialIdentity(const QString &storageKey, const QString &user)
{
    const QString service = QCoreApplication::applicationName() + QLatin1Char(':') + storageKey;
#ifdef Q_OS_WIN
    return QKeychain::Job::credentialIdentity(service, service + QLatin1Char(':') + user);
#else
    return QKeychain::Job::credentialIdentity(service, user);
#endif
}

QByteArray DatabaseImportTest::readFile(const QString &path)
{
    QFile file(path);
    if ( !file.open(QIODevice::ReadOnly) )
        return {};
    return file.readAll();
}

QString DatabaseImportTest::createExport(const QString &name, const QByteArray &credentials, int version)
{
    const QString path = directory->filePath(name);
    createDatabase(path, version);
    if ( QTest::currentTestFailed() )
        return {};
    {
        QSqlQuery query;
        const QStringList statements {
            "INSERT OR REPLACE INTO log_param (name, value) VALUES ('sourceplatform', 'Linux')",
            "INSERT OR REPLACE INTO log_param (name, value) VALUES ('logid', 'old-id')",
            "INSERT OR REPLACE INTO log_param (name, value) VALUES ('last_backup', '2099-01-01')",
            "INSERT INTO contacts (id, callsign) VALUES (1, 'OK1TEST')"
        };
        for ( const QString &statement : statements )
            if ( !query.exec(statement) )
                return {};
        if ( !credentials.isEmpty() )
        {
            QByteArray encrypted;
            if ( !PasswordCipher::encrypt("passphrase", credentials, encrypted)
                 || !query.prepare("INSERT INTO log_param (name, value) VALUES ('security/encryptedpasswords', :value)") )
                return {};
            query.bindValue(":value", encrypted.toBase64());
            if ( !query.exec() )
                return {};
        }
    }
    cleanup();
    return path;
}

void DatabaseImportTest::preparationFailurePreservesOriginal()
{
    const QString source = createExport("source.db", R"({"credentials":[]})");
    QVERIFY(!source.isEmpty());
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    const QByteArray original = readFile(LogDatabase::dbFilename());
    const QPair<QString, QString> identity = credentialIdentity("ImportPassphrase", "import");
    QKeychain::Job::failedOperation = QKeychain::Job::WRITE;
    QKeychain::Job::failedKey = identity.second;
    QVERIFY(!LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QCOMPARE(readFile(LogDatabase::dbFilename()), original);
    QVERIFY(!LogDatabase::hasPendingImport());
    QKeychain::Job::failedKey.clear();
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QVERIFY(!LogDatabase::instance()->preparePendingImport(source, {}, {}, "different passphrase"));
    QCOMPARE(QKeychain::Job::passwords.value(identity), QString("passphrase"));
    QCOMPARE(readFile(LogDatabase::dbFilename()), original);
}

void DatabaseImportTest::candidateFailureDoesNotReplaceOriginal_data()
{
    QTest::addColumn<QString>("failure");
    for ( const QString &failure : {QString("missing-profile"), QString("invalid-identifier"),
                                   QString("settings-write"), QString("commit"), QString("integrity"), QString("index-integrity"),
                                   QString("validation"), QString("migration")} )
        QTest::newRow(qPrintable(failure)) << failure;
}

void DatabaseImportTest::candidateFailureDoesNotReplaceOriginal()
{
    QFETCH(QString, failure);
    const QString source = createExport("source.db", {}, failure == "migration" ? 42 : DBSchemaMigration::latestVersion);
    QVERIFY(!source.isEmpty());
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    const QByteArray original = readFile(LogDatabase::dbFilename());
    QList<ProfileParameter> profiles {
        {"rig_profiles", "missing profile", "port_pathname", {}, {}, "selected", false}
    };
    if ( failure == "invalid-identifier" )
        profiles[0].columnName = "port_pathname = 'injected'; DELETE FROM contacts; --";
    else if ( failure != "missing-profile" )
        profiles.clear();
    const QList<PlatformParameter> parameters {
        {"services/lotw/callbook/tqsl", {}, {}, "selected", false}
    };
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, parameters, profiles, {}));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "inject");
        db.setDatabaseName(LogDatabase::pendingImportPath());
        db.setConnectOptions("QSQLITE_ENABLE_REGEXP");
        QVERIFY(db.open());
        QSqlQuery query(db);
        if ( failure == "validation" )
            QVERIFY(query.exec(QString("INSERT INTO schema_versions VALUES (%1, datetime('now'))")
                               .arg(DBSchemaMigration::latestVersion + 1)));
        if ( failure == "migration" )
            QVERIFY(query.exec("CREATE TRIGGER fail_migration BEFORE INSERT ON schema_versions "
                               "BEGIN SELECT RAISE(ABORT, 'test migration failure'); END"));
        if ( failure == "settings-write" )
            QVERIFY(query.exec("CREATE TRIGGER reject_setting BEFORE INSERT ON log_param "
                               "WHEN NEW.name = 'services/lotw/callbook/tqsl' BEGIN SELECT RAISE(FAIL, 'test'); END"));
        if ( failure == "index-integrity" )
        {
            QVERIFY(query.exec("CREATE INDEX import_test_callsign_idx ON contacts(callsign)"));
            QVERIFY(query.exec("SELECT rootpage FROM sqlite_master WHERE name = 'import_test_callsign_idx'"));
            QVERIFY(query.next());
            const int rootPage = query.value(0).toInt();
            QVERIFY(query.exec("PRAGMA writable_schema = ON"));
            QVERIFY(query.exec("DELETE FROM sqlite_master WHERE name = 'import_test_callsign_idx'"));
            query.finish();
            db.close();
            QVERIFY(db.open());
            // Keep the index entries while changing the table through a connection without that index.
            QVERIFY(query.exec("UPDATE contacts SET callsign = 'CHANGED' WHERE id = 1"));
            QVERIFY(query.exec("PRAGMA writable_schema = ON"));
            QVERIFY(query.prepare("INSERT INTO sqlite_master (type, name, tbl_name, rootpage, sql) "
                                  "VALUES ('index', 'import_test_callsign_idx', 'contacts', :rootpage, "
                                  "'CREATE INDEX import_test_callsign_idx ON contacts(callsign)')"));
            query.bindValue(":rootpage", rootPage);
            QVERIFY(query.exec());
        }
        if ( failure == "commit" || failure == "integrity" )
        {
            QVERIFY(query.exec("CREATE TABLE parent (id INTEGER PRIMARY KEY)"));
            QVERIFY(query.exec("CREATE TABLE child (parent INTEGER REFERENCES parent(id) DEFERRABLE INITIALLY DEFERRED)"));
            if ( failure == "commit" )
                QVERIFY(query.exec("CREATE TRIGGER reject_commit AFTER INSERT ON log_param "
                                   "WHEN NEW.name = 'logid' BEGIN INSERT INTO child VALUES (123); END"));
            else
                QVERIFY(query.exec("INSERT INTO child VALUES (123)"));
        }
        db.close();
    }
    QSqlDatabase::removeDatabase("inject");
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::FAILED);
    QCOMPARE(readFile(LogDatabase::dbFilename()), original);
    QVERIFY(LogDatabase::hasPendingImport());
    if ( failure != "validation" && failure != "integrity" && failure != "index-integrity" )
        QCOMPARE(LogParam::getLoTWTQSLPath("original"), QString("original"));
}

void DatabaseImportTest::missingPassphrasePreservesRecovery()
{
    const QString source = createExport("source.db", R"({"credentials":[]})");
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    const QByteArray original = readFile(LogDatabase::dbFilename());
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QKeychain::Job::passwords.clear();
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::FAILED);
    QCOMPARE(readFile(LogDatabase::dbFilename()), original);
    QVERIFY(!LogParam::getEncryptedPasswords().isEmpty());
    QVERIFY(LogDatabase::hasPendingImport());
    QVERIFY(CredentialStore::instance()->saveImportPassphrase("passphrase"));
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::COMPLETE);
}

void DatabaseImportTest::cancelledImportCleanupCanBeRetried()
{
    const QString source = createExport("source.db", R"({"credentials":[]})");
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    // Resume a cancellation left by a version that required keychain cleanup to succeed.
    setImportPhase("ABORTED");
    QVERIFY(QFile::remove(LogDatabase::pendingImportPath()));
    QKeychain::Job::failedOperation = QKeychain::Job::DELETE;
    QKeychain::Job::failedKey = credentialIdentity("ImportPassphrase", "import").second;
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::COMPLETE);
    QCOMPARE(LogParam::getLogID(), QString("old-id"));
    QVERIFY(!LogDatabase::hasPendingImport());
}

void DatabaseImportTest::interruptedCancellationIsRecoverable_data()
{
    QTest::addColumn<QString>("boundary");
    QTest::newRow("intent-saved") << QString("ABORTED");
    QTest::newRow("candidate-moved") << QString("CANCEL_CANDIDATE_MOVED");
    QTest::newRow("original-restored") << QString("CANCEL_ORIGINAL_RESTORED");
}

void DatabaseImportTest::interruptedCancellationIsRecoverable()
{
#ifdef IMPORT_INTERRUPTION_WRAPPERS
    QFETCH(QString, boundary);
    const QString source = createExport("source.db");
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(runFixture({"--make-wal", LogDatabase::dbFilename()}, 91));
    QVERIFY(QFileInfo(LogDatabase::dbFilename() + "-wal").size() > 0);
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, {}));
    QVERIFY(runFixture({"--recover", "DATABASE_PUBLISHED"}, 91));
    QVERIFY(runFixture({"--cancel", boundary}, 91));
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::COMPLETE);
    QVERIFY(!LogDatabase::hasPendingImport());
    QCOMPARE(LogParam::getLogID(), QString("old-id"));
    QSqlQuery query("SELECT callsign FROM contacts WHERE id = 9");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("WAL-QSO"));
#else
    QSKIP("Requires linker wrappers to interrupt real cancellation operations.");
#endif
}

void DatabaseImportTest::setImportPhase(const QString &phase)
{
    const QString path = LogDatabase::pendingImportPath() + ".params";
    QJsonObject record = QJsonDocument::fromJson(readFile(path)).object();
    QVERIFY(!record.isEmpty());
    record.insert("phase", phase);
    QSaveFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray bytes = QJsonDocument(record).toJson();
    QCOMPARE(file.write(bytes), qint64(bytes.size()));
    QVERIFY(file.commit());
}

void DatabaseImportTest::unpublishedImportCanBeCancelled_data()
{
    QTest::addColumn<QString>("boundary");
    QTest::addColumn<bool>("hadOriginal");
    for ( const QString &boundary : {QString("PREPARED"), QString("READY"),
                                    QString("ORIGINAL_MOVED"), QString("DATABASE_PUBLISHED")} )
    {
        QTest::newRow(qPrintable(boundary)) << boundary << true;
        if ( boundary != "ORIGINAL_MOVED" )
            QTest::newRow(qPrintable(boundary + "-first-install")) << boundary << false;
    }
}

void DatabaseImportTest::unpublishedImportCanBeCancelled()
{
    QFETCH(QString, boundary);
    QFETCH(bool, hadOriginal);
    const QString source = createExport("source.db", R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"incoming"}]})");
    const QByteArray archive = readFile(source);
    if ( hadOriginal )
    {
        const QString original = createExport("original.db");
        QVERIFY(QFile::copy(original, LogDatabase::dbFilename()));
        QVERIFY(LogDatabase::instance()->openDatabase());
        QVERIFY(LogParam::setLogID("original-id"));
        {
            QSqlQuery query;
            QVERIFY(query.exec("UPDATE contacts SET callsign = 'OLD-QSO'"));
        }
        cleanup();
    }
    QKeychain::Job::passwords[credentialIdentity("TestService", "old")] = "original-secret";
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    if ( boundary != "PREPARED" )
    {
        const QJsonObject record = QJsonDocument::fromJson(readFile(LogDatabase::pendingImportPath() + ".params")).object();
        QVERIFY(LogDatabase::instance()->openDatabase(LogDatabase::pendingImportPath()));
        QVERIFY(LogParam::setLogID(record.value("targetLogId").toString()));
        cleanup();
        setImportPhase("READY");
        if ( hadOriginal && boundary != "READY" )
            QVERIFY(QFile::rename(LogDatabase::dbFilename(), LogDatabase::dbDirectory().filePath("qlog.db.preimport")));
        if ( boundary == "DATABASE_PUBLISHED" )
            QVERIFY(QFile::rename(LogDatabase::pendingImportPath(), LogDatabase::dbFilename()));
    }
    QKeychain::Job::failedOperation = QKeychain::Job::READ;
    QKeychain::Job::failedKey = credentialIdentity("ImportPassphrase", "import").second;
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::FAILED);
    const QString error = LogDatabase::instance()->pendingImportError();
    QVERIFY(LogDatabase::instance()->canCancelPendingImport());
    QCOMPARE(LogDatabase::instance()->pendingImportError(), error);
    QKeychain::Job::failedOperation = QKeychain::Job::DELETE;
    QVERIFY2(LogDatabase::instance()->cancelPendingImport(), qPrintable(LogDatabase::instance()->pendingImportError()));
    QVERIFY(!LogDatabase::hasPendingImport());
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::COMPLETE);
    QCOMPARE(readFile(source), archive);
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("TestService", "old")), QString("original-secret"));
    QVERIFY(!QKeychain::Job::passwords.contains(credentialIdentity("TestService", "first")));
    QSqlQuery query("SELECT callsign FROM contacts");
    if ( hadOriginal )
    {
        QCOMPARE(LogParam::getLogID(), QString("original-id"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QString("OLD-QSO"));
    }
    else
        QVERIFY(!query.next());
}

void DatabaseImportTest::cancellationPreservesFilesWhenOriginalIsMissing()
{
    const QString source = createExport("source.db");
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, {}));
    const QByteArray candidate = readFile(LogDatabase::pendingImportPath());
    const QByteArray record = readFile(LogDatabase::pendingImportPath() + ".params");
    QVERIFY(QFile::rename(LogDatabase::dbFilename(), directory->filePath("original.db")));
    QVERIFY(!LogDatabase::instance()->cancelPendingImport());
    QCOMPARE(readFile(LogDatabase::pendingImportPath()), candidate);
    QCOMPARE(readFile(LogDatabase::pendingImportPath() + ".params"), record);
    QVERIFY(!LogDatabase::instance()->pendingImportError().isEmpty());
}

void DatabaseImportTest::abortedImportReportsOriginalOpenFailure()
{
    const QString source = createExport("source.db", R"({"credentials":[]})");
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    setImportPhase("ABORTED");
    QVERIFY(QFile::remove(LogDatabase::pendingImportPath()));
    QFile original(LogDatabase::dbFilename());
    QVERIFY(original.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(original.write("invalid database"), qint64(16));
    original.close();
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::FAILED);
    QVERIFY(!LogDatabase::instance()->pendingImportError().isEmpty());
}

void DatabaseImportTest::cancellationRecordFailurePreventsAdoption()
{
#ifdef Q_OS_UNIX
    const QString source = createExport("source.db");
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    const QByteArray original = readFile(LogDatabase::dbFilename());
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, {}));
    struct rlimit originalLimit;
    QVERIFY(getrlimit(RLIMIT_FSIZE, &originalLimit) == 0);
    struct rlimit restrictedLimit = originalLimit;
    restrictedLimit.rlim_cur = 0;
    const auto previousSignal = signal(SIGXFSZ, SIG_IGN);
    const int restricted = setrlimit(RLIMIT_FSIZE, &restrictedLimit);
    bool result = true;
    if ( restricted == 0 )
        result = LogDatabase::instance()->cancelPendingImport();
    const int restored = setrlimit(RLIMIT_FSIZE, &originalLimit);
    signal(SIGXFSZ, previousSignal);
    QVERIFY(restricted == 0);
    QVERIFY(restored == 0);
    QVERIFY(!result);
    QVERIFY(!QFile::exists(LogDatabase::pendingImportPath()));
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::FAILED);
    QCOMPARE(readFile(LogDatabase::dbFilename()), original);
#else
    QSKIP("Requires the Unix file-size limit to force a real record write failure.");
#endif
}

void DatabaseImportTest::publishedImportRetryPreservesNewData()
{
    const QByteArray credentials(R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"one"},
        {"storagekey":"TestService","username":"second","password":"two"}]})");
    const QString source = createExport("source.db", credentials);
    QVERIFY(!source.isEmpty());
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QKeychain::Job::failedOperation = QKeychain::Job::WRITE;
    QKeychain::Job::failedKey = credentialIdentity("TestService", "second").second;
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::NEEDS_ATTENTION);
    QVERIFY(!QFile::exists(LogDatabase::pendingImportPath()));
    QVERIFY(LogDatabase::hasPendingImport());
    QVERIFY(QFile::exists(LogDatabase::dbDirectory().filePath("qlog.db.preimport")));
    const QString logId = LogParam::getLogID();
    QVERIFY(!logId.isEmpty());
    QVERIFY(logId != "old-id");
    QVERIFY(!LogParam::getEncryptedPasswords().isEmpty());
    {
        QSqlQuery query;
        QVERIFY(query.exec("INSERT INTO contacts (id, callsign) VALUES (2, 'NEW-QSO')"));
    }
    QVERIFY(!LogDatabase::instance()->canCancelPendingImport());
    QVERIFY(!LogDatabase::instance()->cancelPendingImport());
    cleanup();
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::NEEDS_ATTENTION);
    QKeychain::Job::failedKey.clear();
    QCOMPARE(LogDatabase::instance()->recoverImportPasswords(LogDatabase::ImportAction::RETRY_PASSWORDS), LogDatabase::ImportResult::COMPLETE);
    QCOMPARE(LogParam::getLogID(), logId);
    QSqlQuery query("SELECT callsign FROM contacts WHERE id = 2");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("NEW-QSO"));
    QVERIFY(!LogDatabase::hasPendingImport());
    QVERIFY(LogParam::getEncryptedPasswords().isEmpty());
    QVERIFY(!QFile::exists(LogDatabase::dbDirectory().filePath("qlog.db.preimport")));
    QVERIFY(CredentialStore::instance()->getImportPassphrase().isEmpty());
}

void DatabaseImportTest::deferredImportDoesNotOverwriteManualPassword()
{
    const QString source = createExport("source.db", R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"imported"},
        {"storagekey":"TestService","username":"second","password":"secret"}]})");
    QVERIFY(!source.isEmpty());
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QKeychain::Job::failedOperation = QKeychain::Job::WRITE;
    QKeychain::Job::failedKey = credentialIdentity("TestService", "second").second;
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::NEEDS_ATTENTION);
    QKeychain::Job::passwords[credentialIdentity("TestService", "first")] = "manual";
    cleanup();
    QKeychain::Job::failedKey.clear();
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::NEEDS_ATTENTION);
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("TestService", "first")), QString("manual"));
    QVERIFY(LogDatabase::hasPendingImport());
    QSqlQuery query;
    QVERIFY(query.exec("INSERT INTO contacts (id, callsign) VALUES (2, 'NEW-QSO')"));
}

void DatabaseImportTest::deferredImportChecksSchema_data()
{
    QTest::addColumn<QString>("schema");
    QTest::addColumn<QString>("phase");
    for ( const QString &schema : {QString("upgrade"), QString("future"), QString("failed-migration")} )
        QTest::newRow(qPrintable(schema)) << schema << QString("PUBLISHED");
    for ( const QString &phase : {QString("CREDENTIALS_RESTORED"), QString("CREDENTIALS_SKIPPED")} )
        QTest::newRow(qPrintable("future-" + phase)) << QString("future") << phase;
}

void DatabaseImportTest::deferredImportChecksSchema()
{
    QFETCH(QString, schema);
    QFETCH(QString, phase);
    // Model recovery files published by an older QLog, before running the current migrations.
    const QString source = createExport("source.db", R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"imported"}]})", 42);
    QVERIFY(!source.isEmpty());
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    const QString progressPath = LogDatabase::pendingImportPath() + ".params";
    const QJsonObject record = QJsonDocument::fromJson(readFile(progressPath)).object();
    const QString logId = record.value("targetLogId").toString();
    QVERIFY(LogDatabase::instance()->openDatabase(LogDatabase::pendingImportPath()));
    QVERIFY(LogParam::setLogID(logId));
    {
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA foreign_keys = OFF"));
        QVERIFY2(query.exec("INSERT INTO contacts (callsign, hrdlog_qso_upload_status) VALUES ('NEW-QSO', 'R')"), qPrintable(query.lastError().text()));
        QVERIFY(query.exec("PRAGMA foreign_keys = ON"));
        if ( schema == "future" )
            QVERIFY(query.exec(QString("INSERT INTO schema_versions VALUES (%1, datetime('now'))")
                               .arg(DBSchemaMigration::latestVersion + 1)));
        else if ( schema == "failed-migration" )
            QVERIFY(query.exec("CREATE TRIGGER fail_migration BEFORE INSERT ON schema_versions "
                               "BEGIN SELECT RAISE(ABORT, 'test migration failure'); END"));
    }
    const QByteArray payload = LogParam::getEncryptedPasswords();
    cleanup();
    const QString originalPath = LogDatabase::dbDirectory().filePath("qlog.db.preimport");
    QVERIFY(QFile::rename(LogDatabase::dbFilename(), originalPath));
    QVERIFY(QFile::rename(LogDatabase::pendingImportPath(), LogDatabase::dbFilename()));
    setImportPhase(phase);
    const QByteArray progress = readFile(progressPath);
    const QByteArray original = readFile(originalPath);
    QKeychain::Job::passwords[credentialIdentity("TestService", "first")] = "manual";
    const auto passwords = QKeychain::Job::passwords;
    cleanup();
    QKeychain::Job::failedKey.clear();
    QString snapshot;
    const auto result = LogDatabase::instance()->initialize(&snapshot);
    if ( schema != "upgrade" )
    {
        QCOMPARE(result, LogDatabase::ImportResult::FAILED);
        QVERIFY(!LogDatabase::instance()->pendingImportError().isEmpty());
        QCOMPARE(readFile(progressPath), progress);
        QCOMPARE(readFile(originalPath), original);
        QCOMPARE(LogParam::getEncryptedPasswords(), payload);
        QVERIFY(QKeychain::Job::passwords == passwords);
        QVERIFY(LogDatabase::hasPendingImport());
        {
            QSqlQuery query("SELECT hrdlog_qso_upload_status FROM contacts WHERE callsign = 'NEW-QSO'");
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toString(), QString("R"));
            QVERIFY(query.exec(schema == "future"
                               ? QString("DELETE FROM schema_versions WHERE version = %1").arg(DBSchemaMigration::latestVersion + 1)
                               : QString("DROP TRIGGER fail_migration")));
        }
        cleanup();
        if ( phase == "PUBLISHED" )
        {
            QCOMPARE(LogDatabase::instance()->initialize(&snapshot), LogDatabase::ImportResult::NEEDS_ATTENTION);
            QCOMPARE(LogDatabase::instance()->recoverImportPasswords(LogDatabase::ImportAction::RETRY_PASSWORDS),
                     LogDatabase::ImportResult::COMPLETE);
        }
        else
            QCOMPARE(LogDatabase::instance()->initialize(&snapshot), LogDatabase::ImportResult::COMPLETE);
    }
    else
    {
        QCOMPARE(result, LogDatabase::ImportResult::NEEDS_ATTENTION);
        QCOMPARE(LogParam::getEncryptedPasswords(), payload);
        QCOMPARE(readFile(progressPath), progress);
        QCOMPARE(readFile(originalPath), original);
        QVERIFY(QKeychain::Job::passwords == passwords);
        QCOMPARE(LogDatabase::instance()->recoverImportPasswords(LogDatabase::ImportAction::RETRY_PASSWORDS),
                 LogDatabase::ImportResult::COMPLETE);
    }
    QCOMPARE(LogParam::getLogID(), logId);
    QSqlQuery query("SELECT MAX(version) FROM schema_versions");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), DBSchemaMigration::latestVersion);
    QVERIFY(query.exec("SELECT hrdlog_qso_upload_status FROM contacts WHERE callsign = 'NEW-QSO'"));
    QVERIFY(query.next());
    QVERIFY(query.value(0).isNull());
    QVERIFY(snapshot.endsWith(".migration.db"));
    QVERIFY(QFile::exists(LogDatabase::dbDirectory().filePath(snapshot)));
}

void DatabaseImportTest::deferredImportPreparesRoutineBackup()
{
    const QString source = createExport("source.db", R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"imported"}]})");
    QVERIFY(!source.isEmpty());
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QKeychain::Job::failedOperation = QKeychain::Job::WRITE;
    QKeychain::Job::failedKey = credentialIdentity("TestService", "first").second;
    QString snapshot;
    QCOMPARE(LogDatabase::instance()->initialize(&snapshot), LogDatabase::ImportResult::NEEDS_ATTENTION);
    const QDir backups = LogDatabase::dbDirectory();
    QVERIFY(!snapshot.isEmpty());
    QCOMPARE(backups.entryList({"qlog_backup_*.db"}, QDir::Files), QStringList{snapshot});
    // Completed ADX conversion normally retires the internal database copies.
    QVERIFY(QFile::remove(backups.filePath(snapshot)));
    QVERIFY(LogParam::setLastBackupDate(QDate::currentDate().addDays(-8)));
    {
        QSqlQuery query;
        QVERIFY(query.exec("INSERT INTO contacts (id, callsign) VALUES (2, 'NEW-QSO')"));
    }
    cleanup();
    snapshot.clear();
    QCOMPARE(LogDatabase::instance()->initialize(&snapshot), LogDatabase::ImportResult::NEEDS_ATTENTION);
    QVERIFY(!snapshot.isEmpty());
    QCOMPARE(backups.entryList({"qlog_backup_*.db"}, QDir::Files), QStringList{snapshot});
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "ImportBackupCheck");
        db.setDatabaseName(backups.filePath(snapshot));
        QVERIFY(db.open());
        QSqlQuery query(db);
        QVERIFY(query.exec("SELECT callsign FROM contacts WHERE id = 2"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QString("NEW-QSO"));
    }
    QSqlDatabase::removeDatabase("ImportBackupCheck");
    const QString prioritySnapshot = snapshot;
    for ( int attempt = 0; attempt < 2; ++attempt )
    {
        QCOMPARE(LogDatabase::instance()->recoverImportPasswords(LogDatabase::ImportAction::RETRY_PASSWORDS),
                 LogDatabase::ImportResult::NEEDS_ATTENTION);
        QCOMPARE(snapshot, prioritySnapshot);
        QCOMPARE(backups.entryList({"qlog_backup_*.db"}, QDir::Files), QStringList{prioritySnapshot});
    }
    QCOMPARE(LogDatabase::instance()->recoverImportPasswords(LogDatabase::ImportAction::FINISH_WITHOUT_PASSWORDS),
             LogDatabase::ImportResult::COMPLETE);
    QCOMPARE(snapshot, prioritySnapshot);
    QCOMPARE(backups.entryList({"qlog_backup_*.db"}, QDir::Files), QStringList{prioritySnapshot});
}

void DatabaseImportTest::readyImportChecksSchema_data()
{
    QTest::addColumn<QString>("boundary");
    QTest::addColumn<bool>("future");
    for ( const QString &boundary : {QString("READY"), QString("DATABASE_PUBLISHED")} )
    {
        QTest::newRow(qPrintable(boundary + "-future")) << boundary << true;
        QTest::newRow(qPrintable(boundary + "-failed-migration")) << boundary << false;
    }
}

void DatabaseImportTest::readyImportChecksSchema()
{
    QFETCH(QString, boundary);
    QFETCH(bool, future);
    const QString source = createExport("source.db", {}, 42);
    QVERIFY(!source.isEmpty());
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    const QByteArray original = readFile(LogDatabase::dbFilename());
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, {}));
    const bool published = boundary == "DATABASE_PUBLISHED";
    const QString progressPath = LogDatabase::pendingImportPath() + ".params";
    const QJsonObject record = QJsonDocument::fromJson(readFile(progressPath)).object();
    QVERIFY(LogDatabase::instance()->openDatabase(LogDatabase::pendingImportPath()));
    QVERIFY(LogParam::setLogID(record.value("targetLogId").toString()));
    {
        // These files were prepared by an older QLog; keep their actual historical schema.
        QSqlQuery query;
        if ( future )
            QVERIFY(query.exec(QString("INSERT INTO schema_versions VALUES (%1, datetime('now'))")
                               .arg(DBSchemaMigration::latestVersion + 1)));
        else
            QVERIFY(query.exec("CREATE TRIGGER fail_migration BEFORE INSERT ON schema_versions "
                               "BEGIN SELECT RAISE(ABORT, 'test migration failure'); END"));
    }
    cleanup();
    setImportPhase("READY");
    if ( published )
    {
        QVERIFY(QFile::rename(LogDatabase::dbFilename(), LogDatabase::dbDirectory().filePath("qlog.db.preimport")));
        QVERIFY(QFile::rename(LogDatabase::pendingImportPath(), LogDatabase::dbFilename()));
    }
    const QByteArray progress = readFile(progressPath);
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::FAILED);
    QCOMPARE(readFile(progressPath), progress);
    QCOMPARE(QFile::exists(LogDatabase::pendingImportPath()), !published);
    const QString originalPath = published ? LogDatabase::dbDirectory().filePath("qlog.db.preimport") : LogDatabase::dbFilename();
    QCOMPARE(readFile(originalPath), original);
}

void DatabaseImportTest::finishWithoutPasswords_data()
{
    QTest::addColumn<QString>("failure");
    QTest::newRow("restore") << QString("restore");
    QTest::newRow("read-passphrase") << QString("read-passphrase");
    QTest::newRow("old-password-deletion") << QString("old-password-deletion");
    QTest::newRow("passphrase-deletion") << QString("passphrase-deletion");
}

void DatabaseImportTest::finishWithoutPasswords()
{
    QFETCH(QString, failure);
    const QString source = createExport("source.db", R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"imported"},
        {"storagekey":"TestService","username":"second","password":"secret"}]})");
    QVERIFY(!source.isEmpty());
    const QByteArray archive = readFile(source);
    configuredAccount = "old";
    QKeychain::Job::passwords[credentialIdentity("TestService", "old")] = "old secret";
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    const bool failRestore = failure == "restore" || failure == "read-passphrase";
    QKeychain::Job::failedOperation = failRestore ? QKeychain::Job::WRITE : QKeychain::Job::DELETE;
    QKeychain::Job::failedKey = failure == "passphrase-deletion" ? credentialIdentity("ImportPassphrase", "import").second
                              : credentialIdentity("TestService", failRestore ? "second" : "old").second;
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::NEEDS_ATTENTION);
    if ( failure == "read-passphrase" )
    {
        QKeychain::Job::failedOperation = QKeychain::Job::READ;
        QKeychain::Job::failedKey = credentialIdentity("ImportPassphrase", "import").second;
        QCOMPARE(LogDatabase::instance()->recoverImportPasswords(LogDatabase::ImportAction::RETRY_PASSWORDS),
                 LogDatabase::ImportResult::NEEDS_ATTENTION);
    }
    const QString logId = LogParam::getLogID();
    {
        QSqlQuery query;
        QVERIFY(query.exec("INSERT INTO contacts (id, callsign) VALUES (2, 'NEW-QSO')"));
    }
    QKeychain::Job::passwords[credentialIdentity("TestService", "first")] = "manual";
    const auto passwords = QKeychain::Job::passwords;
    // A permanently inaccessible passphrase must not prevent the explicit abandonment.
    QKeychain::Job::failedOperation = QKeychain::Job::DELETE;
    QKeychain::Job::failedKey = credentialIdentity("ImportPassphrase", "import").second;
    QCOMPARE(LogDatabase::instance()->recoverImportPasswords(LogDatabase::ImportAction::FINISH_WITHOUT_PASSWORDS),
             LogDatabase::ImportResult::COMPLETE);
    QVERIFY(QKeychain::Job::passwords == passwords);
    QVERIFY(LogParam::getEncryptedPasswords().isEmpty());
    QVERIFY(LogParam::getSourcePlatform().isEmpty());
    QVERIFY(!LogDatabase::hasPendingImport());
    QCOMPARE(LogParam::getLogID(), logId);
    QCOMPARE(readFile(source), archive);
    cleanup();
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::COMPLETE);
    QVERIFY(LogDatabase::instance()->openDatabase());
    {
        QSqlQuery query("SELECT callsign FROM contacts WHERE id = 2");
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QString("NEW-QSO"));
    }
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QKeychain::Job::failedKey.clear();
    QVERIFY(LogDatabase::instance()->cancelPendingImport());
}

void DatabaseImportTest::skippedImportCleanupCanBeRetried()
{
    const QString source = createExport("source.db", R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"imported"}]})");
    QVERIFY(!source.isEmpty());
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QKeychain::Job::failedOperation = QKeychain::Job::WRITE;
    QKeychain::Job::failedKey = credentialIdentity("TestService", "first").second;
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::NEEDS_ATTENTION);
    QKeychain::Job::passwords[credentialIdentity("TestService", "first")] = "manual";
    const QString sidecar = LogDatabase::dbDirectory().filePath("qlog.db.preimport-wal");
    {
        QFile file(sidecar);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }
    QCOMPARE(LogDatabase::instance()->recoverImportPasswords(LogDatabase::ImportAction::FINISH_WITHOUT_PASSWORDS),
             LogDatabase::ImportResult::NEEDS_ATTENTION);
    const QString progressPath = LogDatabase::pendingImportPath() + ".params";
    QCOMPARE(QJsonDocument::fromJson(readFile(progressPath)).object().value("phase").toString(), QString("CREDENTIALS_SKIPPED"));
    QVERIFY(LogParam::getEncryptedPasswords().isEmpty());
    cleanup();
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::NEEDS_ATTENTION);
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("TestService", "first")), QString("manual"));
    QVERIFY(QFile::remove(sidecar));
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::COMPLETE);
    QVERIFY(!LogDatabase::hasPendingImport());
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("TestService", "first")), QString("manual"));
}

void DatabaseImportTest::interruptedPasswordAbandonmentIsRecoverable()
{
#ifdef IMPORT_INTERRUPTION_WRAPPERS
    const QString source = createExport("source.db", R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"imported"}]})");
    QVERIFY(!source.isEmpty());
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QKeychain::Job::failedOperation = QKeychain::Job::WRITE;
    QKeychain::Job::failedKey = credentialIdentity("TestService", "first").second;
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::NEEDS_ATTENTION);
    QKeychain::Job::passwords[credentialIdentity("TestService", "first")] = "manual";
    cleanup();
    QVERIFY(runFixture({"--finish-password-recovery", "CREDENTIALS_SKIPPED"}, 91));
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::COMPLETE);
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("TestService", "first")), QString("manual"));
    QVERIFY(!LogDatabase::hasPendingImport());
#else
    QSKIP("Requires a linker wrapper to interrupt a durable import decision.");
#endif
}

void DatabaseImportTest::finishWithoutPasswordsRejectsMismatchedDatabase()
{
    const QString source = createExport("source.db", R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"secret"}]})");
    QVERIFY(!source.isEmpty());
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QKeychain::Job::failedOperation = QKeychain::Job::WRITE;
    QKeychain::Job::failedKey = credentialIdentity("TestService", "first").second;
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::NEEDS_ATTENTION);
    const QByteArray passwords = LogParam::getEncryptedPasswords();
    QVERIFY(LogParam::setLogID("different-database"));
    QCOMPARE(LogDatabase::instance()->recoverImportPasswords(LogDatabase::ImportAction::FINISH_WITHOUT_PASSWORDS),
             LogDatabase::ImportResult::FAILED);
    QVERIFY(LogDatabase::hasPendingImport());
    const QJsonObject record = QJsonDocument::fromJson(readFile(LogDatabase::pendingImportPath() + ".params")).object();
    QCOMPARE(record.value("phase").toString(), QString("PUBLISHED"));
    QVERIFY(LogDatabase::instance()->openDatabase());
    QCOMPARE(LogParam::getEncryptedPasswords(), passwords);
}

void DatabaseImportTest::stoppingRecoveryPreservesDataOnWriteFailure()
{
#ifdef Q_OS_UNIX
    const QString source = createExport("source.db", R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"imported"}]})");
    QVERIFY(!source.isEmpty());
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QKeychain::Job::failedOperation = QKeychain::Job::WRITE;
    QKeychain::Job::failedKey = credentialIdentity("TestService", "first").second;
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::NEEDS_ATTENTION);
    QKeychain::Job::passwords[credentialIdentity("TestService", "first")] = "manual";
    const QByteArray payload = LogParam::getEncryptedPasswords();
    const QString progressPath = LogDatabase::pendingImportPath() + ".params";
    QJsonObject record = QJsonDocument::fromJson(readFile(progressPath)).object();
    record["parameters"] = QJsonArray{QJsonObject{{"key", "unused"}, {"newValue", QString(256 * 1024, 'x')}}};
    {
        QSaveFile file(progressPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray data = QJsonDocument(record).toJson();
        QCOMPARE(file.write(data), qint64(data.size()));
        QVERIFY(file.commit());
    }
    const QByteArray progress = readFile(progressPath);
    const auto passwords = QKeychain::Job::passwords;
    struct rlimit originalLimit;
    QVERIFY(getrlimit(RLIMIT_FSIZE, &originalLimit) == 0);
    struct rlimit restrictedLimit = originalLimit;
    restrictedLimit.rlim_cur = 64 * 1024;
    const auto previousSignal = signal(SIGXFSZ, SIG_IGN);
    const int restricted = setrlimit(RLIMIT_FSIZE, &restrictedLimit);
    LogDatabase::ImportResult result = LogDatabase::ImportResult::FAILED;
    if ( restricted == 0 )
        result = LogDatabase::instance()->recoverImportPasswords(LogDatabase::ImportAction::FINISH_WITHOUT_PASSWORDS);
    const int restored = setrlimit(RLIMIT_FSIZE, &originalLimit);
    signal(SIGXFSZ, previousSignal);
    QVERIFY(restricted == 0);
    QVERIFY(restored == 0);
    QCOMPARE(result, LogDatabase::ImportResult::NEEDS_ATTENTION);
    QCOMPARE(readFile(progressPath), progress);
    QCOMPARE(LogParam::getEncryptedPasswords(), payload);
    QVERIFY(QKeychain::Job::passwords == passwords);
    QVERIFY(LogDatabase::hasPendingImport());
    QKeychain::Job::failedKey.clear();
    QCOMPARE(LogDatabase::instance()->recoverImportPasswords(LogDatabase::ImportAction::RETRY_PASSWORDS),
             LogDatabase::ImportResult::COMPLETE);
#else
    QSKIP("Requires the Unix file-size limit to force a real record write failure.");
#endif
}

void DatabaseImportTest::schemaCheckDoesNotPrepareRoutineBackup_data()
{
    QTest::addColumn<bool>("existingSnapshot");
    QTest::newRow("no-snapshot") << false;
    QTest::newRow("migration-snapshot") << true;
}

void DatabaseImportTest::schemaCheckDoesNotPrepareRoutineBackup()
{
    QFETCH(bool, existingSnapshot);
    const QString source = createExport("source.db");
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->openDatabase());
    QVERIFY(LogParam::setLastBackupDate(QDate::currentDate().addDays(-8)));
    QString snapshot;
    if ( existingSnapshot )
        QVERIFY(DatabaseBackup::prepare(true, &snapshot));
    const QString previousSnapshot = snapshot;
    const QDir backups = LogDatabase::dbDirectory();
    const QStringList previousFiles = backups.entryList({"qlog_backup_*.db"}, QDir::Files);
    QVERIFY(LogDatabase::instance()->schemaVersionUpgrade(&snapshot));
    QCOMPARE(snapshot, previousSnapshot);
    QCOMPARE(backups.entryList({"qlog_backup_*.db"}, QDir::Files), previousFiles);
}

void DatabaseImportTest::startupWithoutImportInitializesDatabase()
{
    const QString source = createExport("source.db");
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(!LogDatabase::hasPendingImport());
    QString snapshot;
    QCOMPARE(LogDatabase::instance()->initialize(&snapshot), LogDatabase::ImportResult::COMPLETE);
    QVERIFY(QSqlDatabase::database(QSqlDatabase::defaultConnection, false).isOpen());
    QVERIFY(!snapshot.isEmpty());
    QSqlQuery query("SELECT callsign FROM contacts WHERE id = 1");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("OK1TEST"));
    QCOMPARE(LogParam::getLogID(), QString("old-id"));
    QCOMPARE(LogDatabase::dbDirectory().entryList({"qlog_backup_*.db"}, QDir::Files), QStringList{snapshot});
}

void DatabaseImportTest::firstStartupKeepsFirstQsoBackupDue()
{
    QVERIFY(!QFile::exists(LogDatabase::dbFilename()));
    QString snapshot;
    QCOMPARE(LogDatabase::instance()->initialize(&snapshot), LogDatabase::ImportResult::COMPLETE);
    QVERIFY(snapshot.isEmpty());
    QVERIFY(LogDatabase::dbDirectory().entryList({"qlog_backup_*"}, QDir::Files).isEmpty());
    QVERIFY(!LogParam::getLastBackupDate().isValid());
    {
        QSqlQuery query;
        QVERIFY(query.exec("INSERT INTO contacts (callsign) VALUES ('OK1FIRST')"));
    }
    cleanup();
    QCOMPARE(LogDatabase::instance()->initialize(&snapshot), LogDatabase::ImportResult::COMPLETE);
    QVERIFY(!snapshot.isEmpty());
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "FirstQsoBackupCheck");
        db.setDatabaseName(LogDatabase::dbDirectory().filePath(snapshot));
        QVERIFY(db.open());
        QSqlQuery query("SELECT callsign FROM contacts", db);
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QString("OK1FIRST"));
        db.close();
    }
    QSqlDatabase::removeDatabase("FirstQsoBackupCheck");
}

void DatabaseImportTest::passwordActionsReuseInitializedConnection()
{
    const QString source = createExport("source.db", R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"imported"}]})");
    QVERIFY(!source.isEmpty());
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QKeychain::Job::failedOperation = QKeychain::Job::WRITE;
    QKeychain::Job::failedKey = credentialIdentity("TestService", "first").second;
    QString snapshot;
    QCOMPARE(LogDatabase::instance()->initialize(&snapshot), LogDatabase::ImportResult::NEEDS_ATTENTION);
    const QDir backups = LogDatabase::dbDirectory();
    const QStringList snapshots = backups.entryList({"qlog_backup_*.db"}, QDir::Files);
    QCOMPARE(snapshots, QStringList{snapshot});
    {
        QSqlQuery query;
        QVERIFY(query.exec("CREATE TEMP TABLE connection_marker (value TEXT)"));
        QVERIFY(query.exec("INSERT INTO connection_marker VALUES ('same connection')"));
        QVERIFY(query.exec("INSERT INTO contacts (id, callsign) VALUES (2, 'NEW-QSO')"));
    }
    for ( int attempt = 0; attempt < 3; ++attempt )
    {
        const auto action = attempt == 2 ? LogDatabase::ImportAction::FINISH_WITHOUT_PASSWORDS
                                        : LogDatabase::ImportAction::RETRY_PASSWORDS;
        QCOMPARE(LogDatabase::instance()->recoverImportPasswords(action),
                 attempt == 2 ? LogDatabase::ImportResult::COMPLETE : LogDatabase::ImportResult::NEEDS_ATTENTION);
        QSqlQuery query("SELECT value FROM connection_marker");
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QString("same connection"));
        QVERIFY(query.exec("SELECT callsign FROM contacts WHERE id = 2"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QString("NEW-QSO"));
        QCOMPARE(backups.entryList({"qlog_backup_*.db"}, QDir::Files), snapshots);
    }
    QVERIFY(!LogDatabase::hasPendingImport());
}

void DatabaseImportTest::passwordRecoveryRequiresOpenActiveDatabase_data()
{
    QTest::addColumn<bool>("otherDatabase");
    QTest::newRow("closed") << false;
    QTest::newRow("different-database") << true;
}

void DatabaseImportTest::passwordRecoveryRequiresOpenActiveDatabase()
{
    QFETCH(bool, otherDatabase);
    const QString source = createExport("source.db", R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"imported"}]})");
    QVERIFY(!source.isEmpty());
    // A first installation makes unintended creation of the active DB observable.
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    const QByteArray progress = readFile(LogDatabase::pendingImportPath() + ".params");
    const QByteArray candidate = readFile(LogDatabase::pendingImportPath());
    const auto passwords = QKeychain::Job::passwords;
    if ( otherDatabase )
        QVERIFY(LogDatabase::instance()->openDatabase(source));
    for ( const auto action : {LogDatabase::ImportAction::RETRY_PASSWORDS, LogDatabase::ImportAction::FINISH_WITHOUT_PASSWORDS} )
    {
        QCOMPARE(LogDatabase::instance()->recoverImportPasswords(action), LogDatabase::ImportResult::FAILED);
        QVERIFY(!QFile::exists(LogDatabase::dbFilename()));
        QCOMPARE(readFile(LogDatabase::pendingImportPath() + ".params"), progress);
        QCOMPARE(readFile(LogDatabase::pendingImportPath()), candidate);
        QVERIFY(QKeychain::Job::passwords == passwords);
        if ( otherDatabase )
        {
            QVERIFY(QSqlDatabase::database(QSqlDatabase::defaultConnection, false).isOpen());
            QCOMPARE(QSqlDatabase::database(QSqlDatabase::defaultConnection, false).databaseName(), source);
        }
        else
            QVERIFY(!QSqlDatabase::contains());
    }
}

void DatabaseImportTest::backupPreparationFailureControlsStartup_data()
{
    QTest::addColumn<bool>("migrationRequired");
    QTest::newRow("mandatory-migration-backup") << true;
    QTest::newRow("routine-backup") << false;
}

void DatabaseImportTest::backupPreparationFailureControlsStartup()
{
#ifdef IMPORT_INTERRUPTION_WRAPPERS
    QFETCH(bool, migrationRequired);
    const int version = DBSchemaMigration::latestVersion - (migrationRequired ? 1 : 0);
    const QString source = directory->filePath("real-schema.db");
    createDatabase(source, version);
    if ( QTest::currentTestFailed() )
        return;
    QVERIFY(LogParam::setSourcePlatform("Linux"));
    QVERIFY(LogParam::setLogID("old-id"));
    // A recent routine backup must not suppress the mandatory migration copy.
    QVERIFY(LogParam::setLastBackupDate(QDate::currentDate().addDays(migrationRequired ? 0 : -8)));
    {
        QSqlQuery query;
        QVERIFY(query.exec("INSERT INTO contacts (callsign) VALUES ('OK1REAL')"));
    }
    QString previousSnapshot;
    QVERIFY(DatabaseBackup::prepare(true, &previousSnapshot));
    const QString previousPath = LogDatabase::dbDirectory().filePath(previousSnapshot);
    const QByteArray previous = readFile(previousPath);
    cleanup();
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    const QByteArray original = readFile(LogDatabase::dbFilename());
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, {}));
    const QString progressPath = LogDatabase::pendingImportPath() + ".params";
    const QByteArray progress = readFile(progressPath);
    LogDatabase::ImportResult result;
    {
        QScopedValueRollback<QString> failure(interruptionBoundary, "FAIL_BACKUP_PUBLICATION");
        result = LogDatabase::instance()->initialize();
    }
    QCOMPARE(result, migrationRequired ? LogDatabase::ImportResult::FAILED : LogDatabase::ImportResult::COMPLETE);
    QCOMPARE(readFile(previousPath), previous);
    QVERIFY(!LogParam::getBackupError().isEmpty());
    QSqlQuery query("SELECT MAX(version) FROM schema_versions");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), version);
    QVERIFY(query.exec("SELECT callsign FROM contacts"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("OK1REAL"));
    if ( migrationRequired )
    {
        QCOMPARE(readFile(LogDatabase::dbFilename()), original);
        QCOMPARE(readFile(progressPath), progress);
        QVERIFY(QFile::exists(LogDatabase::pendingImportPath()));
    }
    else
        QVERIFY(!LogDatabase::hasPendingImport());
    QCOMPARE(LogDatabase::dbDirectory().entryList({"qlog_backup_*.db"}, QDir::Files), QStringList{previousSnapshot});
#else
    QSKIP("Requires the existing linker wrapper to fail backup publication deterministically.");
#endif
}

void DatabaseImportTest::databaseSwitchClearsSettingsCache()
{
    const QString first = createExport("first.db");
    const QString second = createExport("second.db");
    QVERIFY(LogDatabase::instance()->openDatabase(first));
    QVERIFY(LogParam::setLogID("first-id"));
    QCOMPARE(LogParam::getLogID(), QString("first-id"));
    QVERIFY(LogDatabase::instance()->openDatabase(second));
    QCOMPARE(LogParam::getLogID(), QString("old-id"));
}

void DatabaseImportTest::cancelledImportIsNeverAdopted()
{
    const QString source = createExport("source.db");
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->openDatabase());
    QVERIFY(LogParam::setLogID("original-id"));
    const QByteArray original = readFile(LogDatabase::dbFilename());
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, {}));
    QVERIFY(LogDatabase::instance()->cancelPendingImport());
    QVERIFY(!LogDatabase::hasPendingImport());
    QCOMPARE(readFile(LogDatabase::dbFilename()), original);
    QVERIFY(QSqlDatabase::database(QSqlDatabase::defaultConnection, false).isOpen());
    QCOMPARE(LogParam::getLogID(), QString("original-id"));
    {
        QSqlQuery query;
        QVERIFY(query.exec("INSERT INTO contacts (id, callsign) VALUES (2, 'AFTER-CANCEL')"));
    }
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::COMPLETE);
    QCOMPARE(LogParam::getLogID(), QString("original-id"));
    QSqlQuery query("SELECT callsign FROM contacts WHERE id = 1");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("OK1TEST"));
    QVERIFY(query.exec("SELECT callsign FROM contacts WHERE id = 2"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("AFTER-CANCEL"));
}

void DatabaseImportTest::credentialReplacementUsesOriginalAccounts()
{
    const QString source = createExport("source.db", R"({"credentials":[
        {"storagekey":"TestService","username":"shared","password":"new secret"},
        {"storagekey":"TestService","username":"imported","password":"imported secret"}]})");
    configuredAccount = "shared";
    configuredDxAccounts = QStringList{"old DX account"};
    QKeychain::Job::passwords[credentialIdentity("TestService", "shared")] = "old secret";
    QKeychain::Job::passwords[credentialIdentity("DX", "old DX account")] = "DX secret";
    QKeychain::Job::passwords[credentialIdentity("Other", "unrelated")] = "untouched";
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    configuredAccount = "imported";
    configuredDxAccounts.clear();
    QKeychain::Job::failedOperation = QKeychain::Job::DELETE;
    QKeychain::Job::failedKey = credentialIdentity("DX", "old DX account").second;
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::NEEDS_ATTENTION);
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("TestService", "shared")), QString("new secret"));
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("DX", "old DX account")), QString("DX secret"));
    QVERIFY(!LogDatabase::instance()->cancelPendingImport());
    QKeychain::Job::failedKey.clear();
    QCOMPARE(LogDatabase::instance()->recoverImportPasswords(LogDatabase::ImportAction::RETRY_PASSWORDS), LogDatabase::ImportResult::COMPLETE);
    QVERIFY(!QKeychain::Job::passwords.contains(credentialIdentity("DX", "old DX account")));
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("TestService", "shared")), QString("new secret"));
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("TestService", "imported")), QString("imported secret"));
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("Other", "unrelated")), QString("untouched"));
}

void DatabaseImportTest::credentialCaseChangesKeepImportedPassword_data()
{
    QTest::addColumn<QString>("storageKey");
    QTest::addColumn<QString>("username");
    QTest::newRow("username-case") << QString("TestService") << QString("OK1ABC");
    QTest::newRow("storage-key-case") << QString("testservice") << QString("ok1abc");
}

void DatabaseImportTest::credentialCaseChangesKeepImportedPassword()
{
    QFETCH(QString, storageKey);
    QFETCH(QString, username);
    const QJsonObject entry{{"storagekey", storageKey}, {"username", username}, {"password", "B"}};
    const QString source = createExport("source.db", QJsonDocument(QJsonObject{{"credentials", QJsonArray{entry}}}).toJson());
    QVERIFY(!source.isEmpty());
    configuredAccount = "ok1abc";
    configuredDxAccounts = QStringList{"obsolete"};
    QKeychain::Job::passwords[credentialIdentity("TestService", "ok1abc")] = "A";
    QKeychain::Job::passwords[credentialIdentity("DX", "obsolete")] = "obsolete secret";
    QKeychain::Job::passwords[credentialIdentity("Other", "unrelated")] = "untouched";
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::COMPLETE);
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity(storageKey, username)), QString("B"));
    QVERIFY(!QKeychain::Job::passwords.values().contains("A"));
    QVERIFY(!QKeychain::Job::passwords.contains(credentialIdentity("DX", "obsolete")));
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("Other", "unrelated")), QString("untouched"));
    QCOMPARE(QKeychain::Job::passwords.size(), 2);
    QVERIFY(!LogDatabase::hasPendingImport());
}

void DatabaseImportTest::unfinishedImportCannotOverwriteRecoveryCredentials()
{
    const QString source = createExport("source.db", R"({"credentials":[
        {"storagekey":"TestService","username":"failed","password":"secret"}]})");
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QKeychain::Job::failedOperation = QKeychain::Job::WRITE;
    QKeychain::Job::failedKey = credentialIdentity("TestService", "failed").second;
    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::NEEDS_ATTENTION);
    const QByteArray payload = LogParam::getEncryptedPasswords();
    QVERIFY(!payload.isEmpty());
    QVERIFY(!CredentialStore::instance()->exportPasswords("different passphrase"));
    QCOMPARE(LogParam::getEncryptedPasswords(), payload);
}

void DatabaseImportTest::orphanCandidatePreservesOriginal()
{
    QFile original(LogDatabase::dbFilename());
    QVERIFY(original.open(QIODevice::WriteOnly));
    QCOMPARE(original.write("original database"), qint64(17));
    original.close();
    QFile pending(LogDatabase::pendingImportPath());
    QVERIFY(pending.open(QIODevice::WriteOnly));
    QCOMPARE(pending.write("invalid candidate"), qint64(17));
    pending.close();

    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::FAILED);
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(original.readAll(), QByteArray("original database"));
    QVERIFY(QFile::exists(LogDatabase::pendingImportPath()));
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

void DatabaseImportTest::inspectionRequiresExportSchema_data()
{
    QTest::addColumn<QStringList>("statements");
    QTest::addColumn<bool>("valid");
    const QStringList schema {
        "CREATE TABLE schema_versions (version INTEGER)",
        QString("INSERT INTO schema_versions VALUES (%1)").arg(DBSchemaMigration::latestVersion),
        "CREATE TABLE log_param (name TEXT PRIMARY KEY, value TEXT)",
        "INSERT INTO log_param VALUES ('sourceplatform', 'Linux')",
        "CREATE TABLE contacts (id INTEGER PRIMARY KEY, callsign TEXT)"
    };
    QTest::newRow("current-export") << schema << true;
    QStringList modified = schema;
    modified[1] = QString("INSERT INTO schema_versions VALUES (%1)").arg(DBSchemaMigration::latestVersion - 1);
    QTest::newRow("older-export") << modified << true;
    modified[1] = QString("INSERT INTO schema_versions VALUES (%1)").arg(DBSchemaMigration::latestVersion + 1);
    QTest::newRow("too-new") << modified << false;
    modified[1] = "INSERT INTO schema_versions VALUES ('invalid')";
    QTest::newRow("invalid-version") << modified << false;
    modified = schema;
    modified.removeAt(1);
    QTest::newRow("empty-versions") << modified << false;
    QTest::newRow("only-versions") << QStringList(schema.mid(0, 2)) << false;
    modified = schema;
    modified[2] = "CREATE TABLE log_param (name TEXT PRIMARY KEY)";
    modified.removeAt(3);
    QTest::newRow("missing-value-column") << modified << false;
    modified = schema;
    modified.removeAt(3);
    QTest::newRow("missing-platform") << modified << false;
    modified = schema;
    modified[3] = "INSERT INTO log_param VALUES ('sourceplatform', '')";
    QTest::newRow("empty-platform") << modified << false;
    modified = schema;
    modified.removeLast();
    QTest::newRow("missing-contacts") << modified << false;
    modified.append("CREATE TABLE contacts (id INTEGER PRIMARY KEY)");
    QTest::newRow("missing-callsign-column") << modified << false;
}

void DatabaseImportTest::inspectionRequiresExportSchema()
{
    QFETCH(QStringList, statements);
    QFETCH(bool, valid);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("candidate.db");
    const QString connectionName("ImportFixture");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QSqlQuery query(db);
        for ( const QString &statement : statements )
            QVERIFY(query.exec(statement));
    }
    QSqlDatabase::removeDatabase(connectionName);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray original = file.readAll();
    file.close();

    for ( int attempt = 0; attempt < 2; ++attempt )
    {
        const DatabaseInfo info = LogDatabase::inspectDatabase(path);
        QCOMPARE(info.valid, valid);
        if ( !valid )
            QVERIFY(!info.errorMessage.isEmpty());
        QVERIFY(!QSqlDatabase::contains("InspectDB"));
    }
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), original);
}

void DatabaseImportTest::inspectionDoesNotCreateMissingFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("missing.db");
    QVERIFY(!LogDatabase::inspectDatabase(path).valid);
    QVERIFY(!QFile::exists(path));
}

void DatabaseImportTest::emptyAndSamePlatformImportsReplaceMetadata_data()
{
    QTest::addColumn<bool>("emptySelection");
    QTest::newRow("empty") << true;
    QTest::newRow("changed") << false;
}

void DatabaseImportTest::emptyAndSamePlatformImportsReplaceMetadata()
{
    QFETCH(bool, emptySelection);
    const QString source = createExport("source.db");
    QVERIFY(!source.isEmpty());
    const QList<PlatformParameter> stale {
        {"services/lotw/callbook/tqsl", {}, {}, "stale", false}
    };
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, stale, {}, {}));
    QVERIFY(LogDatabase::instance()->cancelPendingImport());
    QList<PlatformParameter> parameters;
    QList<ProfileParameter> profiles;
    if ( !emptySelection )
    {
        parameters.append({"services/lotw/callbook/tqsl", {}, {}, "selected", false});
        profiles.append({"rig_profiles", "new", "port_pathname", {}, {}, "selected", false});
    }
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, parameters, profiles, {}));
    const QJsonObject record = QJsonDocument::fromJson(readFile(LogDatabase::pendingImportPath() + ".params")).object();
    QVERIFY(!record.contains("version"));
    const QJsonArray savedParameters = record.value("parameters").toArray();
    const QJsonArray savedProfiles = record.value("profilePorts").toArray();
    QCOMPARE(savedParameters.size(), emptySelection ? 0 : 1);
    QCOMPARE(savedProfiles.size(), emptySelection ? 0 : 1);
    if ( !emptySelection )
    {
        QCOMPARE(savedParameters.first().toObject().value("newValue").toString(), QString("selected"));
        QCOMPARE(savedProfiles.first().toObject().value("profileName").toString(), QString("new"));
    }
}

void DatabaseImportTest::importProgressRecordCompatibility_data()
{
    QTest::addColumn<bool>("legacyVersion");
    QTest::newRow("unversioned") << false;
    QTest::newRow("legacy-version-1") << true;
}

void DatabaseImportTest::importProgressRecordCompatibility()
{
    QFETCH(bool, legacyVersion);
    const QString source = createExport("source.db");
    QVERIFY(!source.isEmpty());
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, {}));
    const QString progressPath = LogDatabase::pendingImportPath() + ".params";
    QJsonObject record = QJsonDocument::fromJson(readFile(progressPath)).object();
    if ( legacyVersion )
        record["version"] = 1;
    else
        record.remove("version");

    QFile progress(progressPath);
    QVERIFY(progress.open(QIODevice::WriteOnly));
    const QByteArray data = QJsonDocument(record).toJson();
    QCOMPARE(progress.write(data), qint64(data.size()));
    progress.close();

    QCOMPARE(LogDatabase::instance()->initialize(), LogDatabase::ImportResult::COMPLETE);
    QVERIFY(!LogDatabase::hasPendingImport());
    QSqlQuery query("SELECT callsign FROM contacts WHERE id = 1");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("OK1TEST"));
}

void DatabaseImportTest::preparationWriteFailurePreservesOriginal()
{
#ifdef Q_OS_UNIX
    const QString source = createExport("source.db");
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    const QByteArray original = readFile(LogDatabase::dbFilename());
    const QList<PlatformParameter> largeSelection {
        {"services/lotw/callbook/tqsl", {}, {}, QString(int(QFileInfo(source).size()) + 1024, 'x'), false}
    };
    struct rlimit originalLimit;
    QVERIFY(getrlimit(RLIMIT_FSIZE, &originalLimit) == 0);
    struct rlimit restrictedLimit = originalLimit;
    restrictedLimit.rlim_cur = QFileInfo(source).size();
    const auto previousSignal = signal(SIGXFSZ, SIG_IGN);
    const int restricted = setrlimit(RLIMIT_FSIZE, &restrictedLimit);
    bool result = true;
    if ( restricted == 0 )
        result = LogDatabase::instance()->preparePendingImport(source, largeSelection, {}, {});
    const int restored = setrlimit(RLIMIT_FSIZE, &originalLimit);
    signal(SIGXFSZ, previousSignal);
    QVERIFY(restricted == 0);
    QVERIFY(restored == 0);
    QVERIFY(!result);
    QCOMPARE(readFile(LogDatabase::dbFilename()), original);
    QVERIFY(!LogDatabase::hasPendingImport());
#else
    QSKIP("Requires the Unix file-size limit to force a real write failure.");
#endif
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("DatabaseImportTest");
#ifndef Q_OS_LINUX
    QStandardPaths::setTestModeEnabled(true);
#endif
    const QStringList arguments = app.arguments();
#ifdef IMPORT_INTERRUPTION_WRAPPERS
    if ( arguments.value(1) == "--make-wal" )
    {
        if ( !LogDatabase::instance()->openDatabase(arguments.value(2)) )
            return 2;
        QSqlQuery query;
        if ( !query.exec("PRAGMA wal_autocheckpoint = 0")
             || !query.exec("INSERT INTO contacts (id, callsign) VALUES (9, 'WAL-QSO')") )
            return 3;
        _exit(91);
    }
    if ( arguments.value(1) == "--prepare" || arguments.value(1) == "--recover" || arguments.value(1) == "--cancel"
         || arguments.value(1) == "--finish-password-recovery" )
    {
        interruptionBoundary = arguments.last();
        bool result;
        if ( arguments.value(1) == "--prepare" )
            result = LogDatabase::instance()->preparePendingImport(arguments.value(2), {}, {}, {});
        else if ( arguments.value(1) == "--cancel" )
            result = LogDatabase::instance()->cancelPendingImport();
        else
        {
            LogDatabase::ImportResult initialized = LogDatabase::instance()->initialize();
            if ( arguments.value(1) == "--finish-password-recovery" && initialized == LogDatabase::ImportResult::NEEDS_ATTENTION )
                initialized = LogDatabase::instance()->recoverImportPasswords(LogDatabase::ImportAction::FINISH_WITHOUT_PASSWORDS);
            result = initialized == LogDatabase::ImportResult::COMPLETE;
        }
        return result ? 0 : 1;
    }
#endif
    DatabaseImportTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_databaseimport.moc"
