#include <QtTest>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "core/LogParam.h"
#include "core/PasswordCipher.h"
#include "core/CredentialStore.h"
#include "core/Migration.h"
#include <QApplication>
#include <QProcess>
#include <QSaveFile>
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
    static bool runSqlFile(DBSchemaMigration &migration, const QString &path)
    {
        return migration.runSqlFile(path);
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
    const bool result = __real__ZN5QFile6renameERK7QStringS2_(source, target);
    if ( result && ((interruptionBoundary == "ORIGINAL_MOVED" && source == LogDatabase::dbFilename())
                   || (interruptionBoundary == "DATABASE_PUBLISHED" && source == LogDatabase::pendingImportPath())) )
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
    void databaseSwitchClearsSettingsCache();
    void cancelledImportIsNeverAdopted();
    void credentialReplacementUsesOriginalAccounts();
    void unfinishedImportCannotOverwriteRecoveryCredentials();
    void interruptedPublicationIsRecoverable_data();
    void interruptedPublicationIsRecoverable();
    void missingPassphrasePreservesRecovery();
    void cancelledImportCleanupCanBeRetried();
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
    QString createExport(const QString &name, const QByteArray &credentials = QByteArray());
    QByteArray readFile(const QString &path);
    bool runFixture(const QStringList &arguments, int exitCode);
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
    QVERIFY(LogDatabase::instance()->processPendingImport());
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
    QVERIFY(!LogDatabase::instance()->processPendingImport());
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

void DatabaseImportTest::migratedArchivePreservesData_data()
{
    QTest::addColumn<int>("version");
    QTest::newRow("current") << 43;
    QTest::newRow("previous") << 42;
}

void DatabaseImportTest::migratedArchivePreservesData()
{
    QFETCH(int, version);
    const QString source = directory->filePath("real-schema.db");
    QVERIFY(LogDatabase::instance()->openDatabase(source));
    {
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA foreign_keys = OFF"));
        DBSchemaMigration migration;
        for ( int step = 1; step <= version; ++step )
        {
            const QString path = QString(":/res/sql/migration_%1.sql").arg(step, 3, 10, QChar('0'));
            QVERIFY2(MigrationSqlTest_FriendAccessor::runSqlFile(migration, path), qPrintable(path));
            QVERIFY(query.prepare("INSERT INTO schema_versions VALUES (:version, datetime('now'))"));
            query.bindValue(":version", step);
            QVERIFY(query.exec());
        }
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
    QVERIFY2(LogDatabase::instance()->processPendingImport(), qPrintable(LogDatabase::instance()->pendingImportError()));
    QCOMPARE(LogParam::getLoTWTQSLPath(QString()), QString("/selected/tqsl"));
    QSqlQuery query("SELECT callsign FROM contacts");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("OK1REAL"));
    QVERIFY(query.exec("SELECT port_pathname FROM rig_profiles WHERE profile_name = 'Test rig'"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("/selected/port"));
    QVERIFY(query.exec("SELECT MAX(version) FROM schema_versions"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 43);
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
        QVERIFY(!LogDatabase::instance()->processPendingImport());
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
    QVERIFY(LogDatabase::instance()->processPendingImport());
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
#ifdef Q_OS_LINUX
    qputenv("XDG_DATA_HOME", directory->path().toUtf8());
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
    return qMakePair(service, service + QLatin1Char(':') + user);
#else
    return qMakePair(service, user);
#endif
}

QByteArray DatabaseImportTest::readFile(const QString &path)
{
    QFile file(path);
    if ( !file.open(QIODevice::ReadOnly) )
        return {};
    return file.readAll();
}

QString DatabaseImportTest::createExport(const QString &name, const QByteArray &credentials)
{
    const QString path = directory->filePath(name);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "fixture");
        db.setDatabaseName(path);
        if ( !db.open() )
            return {};
        QSqlQuery query(db);
        const QStringList statements {
            "CREATE TABLE schema_versions (version INTEGER)",
            "INSERT INTO schema_versions VALUES (43)",
            "CREATE TABLE log_param (name TEXT PRIMARY KEY, value TEXT)",
            "INSERT INTO log_param VALUES ('sourceplatform', 'Linux')",
            "INSERT INTO log_param VALUES ('logid', 'old-id')",
            "INSERT INTO log_param VALUES ('last_backup', '2099-01-01')",
            "CREATE TABLE contacts (id INTEGER PRIMARY KEY, callsign TEXT)",
            "INSERT INTO contacts VALUES (1, 'OK1TEST')",
            "CREATE TABLE rig_profiles (profile_name TEXT PRIMARY KEY, port_pathname TEXT)"
        };
        for ( const QString &statement : statements )
            if ( !query.exec(statement) )
                return {};
        if ( !credentials.isEmpty() )
        {
            QByteArray encrypted;
            if ( !PasswordCipher::encrypt("passphrase", credentials, encrypted)
                 || !query.prepare("INSERT INTO log_param VALUES ('security/encryptedpasswords', :value)") )
                return {};
            query.bindValue(":value", encrypted.toBase64());
            if ( !query.exec() )
                return {};
        }
        db.close();
    }
    QSqlDatabase::removeDatabase("fixture");
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
    const QString source = createExport("source.db");
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
        QVERIFY(db.open());
        QSqlQuery query(db);
        if ( failure == "validation" )
            QVERIFY(query.exec("UPDATE schema_versions SET version = 99"));
        if ( failure == "migration" )
            QVERIFY(query.exec("UPDATE schema_versions SET version = 42"));
        if ( failure == "settings-write" )
            QVERIFY(query.exec("CREATE TRIGGER reject_setting BEFORE INSERT ON log_param "
                               "WHEN NEW.name = 'services/lotw/callbook/tqsl' BEGIN SELECT RAISE(FAIL, 'test'); END"));
        if ( failure == "index-integrity" )
        {
            QVERIFY(query.exec("CREATE INDEX contacts_callsign_idx ON contacts(callsign)"));
            QVERIFY(query.exec("SELECT rootpage FROM sqlite_master WHERE name = 'contacts_callsign_idx'"));
            QVERIFY(query.next());
            const int rootPage = query.value(0).toInt();
            QVERIFY(query.exec("PRAGMA writable_schema = ON"));
            QVERIFY(query.exec("DELETE FROM sqlite_master WHERE name = 'contacts_callsign_idx'"));
            query.finish();
            db.close();
            QVERIFY(db.open());
            // Keep the index entries while changing the table through a connection without that index.
            QVERIFY(query.exec("UPDATE contacts SET callsign = 'CHANGED' WHERE id = 1"));
            QVERIFY(query.exec("PRAGMA writable_schema = ON"));
            QVERIFY(query.prepare("INSERT INTO sqlite_master (type, name, tbl_name, rootpage, sql) "
                                  "VALUES ('index', 'contacts_callsign_idx', 'contacts', :rootpage, "
                                  "'CREATE INDEX contacts_callsign_idx ON contacts(callsign)')"));
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
    QVERIFY(!LogDatabase::instance()->processPendingImport());
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
    QVERIFY(!LogDatabase::instance()->processPendingImport());
    QCOMPARE(readFile(LogDatabase::dbFilename()), original);
    QVERIFY(!LogParam::getEncryptedPasswords().isEmpty());
    QVERIFY(LogDatabase::hasPendingImport());
    QVERIFY(CredentialStore::instance()->saveImportPassphrase("passphrase"));
    QVERIFY(LogDatabase::instance()->processPendingImport());
}

void DatabaseImportTest::cancelledImportCleanupCanBeRetried()
{
    const QString source = createExport("source.db", R"({"credentials":[]})");
    QVERIFY(QFile::copy(source, LogDatabase::dbFilename()));
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QKeychain::Job::failedOperation = QKeychain::Job::DELETE;
    QKeychain::Job::failedKey = credentialIdentity("ImportPassphrase", "import").second;
    QVERIFY(!LogDatabase::instance()->cancelPendingImport());
    QVERIFY(!QFile::exists(LogDatabase::pendingImportPath()));
    QVERIFY(LogDatabase::hasPendingImport());
    QKeychain::Job::failedKey.clear();
    QVERIFY(LogDatabase::instance()->processPendingImport());
    QCOMPARE(LogParam::getLogID(), QString("old-id"));
    QVERIFY(!LogDatabase::hasPendingImport());
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
    QKeychain::Job::failedOperation = QKeychain::Job::DELETE;
    QKeychain::Job::failedKey = credentialIdentity("ImportPassphrase", "import").second;
    QVERIFY(!LogDatabase::instance()->cancelPendingImport());
    QKeychain::Job::failedKey.clear();
    QFile original(LogDatabase::dbFilename());
    QVERIFY(original.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(original.write("invalid database"), qint64(16));
    original.close();
    QVERIFY(!LogDatabase::instance()->processPendingImport());
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
    QVERIFY(!LogDatabase::instance()->processPendingImport());
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
    QVERIFY(!LogDatabase::instance()->processPendingImport());
    QVERIFY(!QFile::exists(LogDatabase::pendingImportPath()));
    QVERIFY(LogDatabase::hasPendingImport());
    QVERIFY(QFile::exists(LogDatabase::dbDirectory().filePath("qlog.db.preimport")));
    const QString logId = LogParam::getLogID();
    QVERIFY(!logId.isEmpty());
    QVERIFY(logId != "old-id");
    QVERIFY(!LogParam::getEncryptedPasswords().isEmpty());
    {
        QSqlQuery query;
        QVERIFY(query.exec("INSERT INTO contacts VALUES (2, 'NEW-QSO')"));
    }
    cleanup();
    QKeychain::Job::failedKey.clear();
    QVERIFY(LogDatabase::instance()->processPendingImport());
    QCOMPARE(LogParam::getLogID(), logId);
    QSqlQuery query("SELECT callsign FROM contacts WHERE id = 2");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("NEW-QSO"));
    QVERIFY(!LogDatabase::hasPendingImport());
    QVERIFY(LogParam::getEncryptedPasswords().isEmpty());
    QVERIFY(!QFile::exists(LogDatabase::dbDirectory().filePath("qlog.db.preimport")));
    QVERIFY(CredentialStore::instance()->getImportPassphrase().isEmpty());
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
    const QByteArray original = readFile(LogDatabase::dbFilename());
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, {}));
    QVERIFY(LogDatabase::instance()->cancelPendingImport());
    QVERIFY(!LogDatabase::hasPendingImport());
    QVERIFY(LogDatabase::instance()->processPendingImport());
    QCOMPARE(readFile(LogDatabase::dbFilename()), original);
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
    QVERIFY(!LogDatabase::instance()->processPendingImport());
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("TestService", "shared")), QString("new secret"));
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("DX", "old DX account")), QString("DX secret"));
    QVERIFY(!LogDatabase::instance()->cancelPendingImport());
    QKeychain::Job::failedKey.clear();
    QVERIFY(LogDatabase::instance()->processPendingImport());
    QVERIFY(!QKeychain::Job::passwords.contains(credentialIdentity("DX", "old DX account")));
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("TestService", "shared")), QString("new secret"));
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("TestService", "imported")), QString("imported secret"));
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("Other", "unrelated")), QString("untouched"));
}

void DatabaseImportTest::unfinishedImportCannotOverwriteRecoveryCredentials()
{
    const QString source = createExport("source.db", R"({"credentials":[
        {"storagekey":"TestService","username":"failed","password":"secret"}]})");
    QVERIFY(LogDatabase::instance()->preparePendingImport(source, {}, {}, "passphrase"));
    QKeychain::Job::failedOperation = QKeychain::Job::WRITE;
    QKeychain::Job::failedKey = credentialIdentity("TestService", "failed").second;
    QVERIFY(!LogDatabase::instance()->processPendingImport());
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

    QVERIFY(!LogDatabase::instance()->processPendingImport());
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
        "INSERT INTO schema_versions VALUES (43)",
        "CREATE TABLE log_param (name TEXT PRIMARY KEY, value TEXT)",
        "INSERT INTO log_param VALUES ('sourceplatform', 'Linux')",
        "CREATE TABLE contacts (id INTEGER PRIMARY KEY, callsign TEXT)"
    };
    QTest::newRow("current-export") << schema << true;
    QStringList modified = schema;
    modified[1] = "INSERT INTO schema_versions VALUES (42)";
    QTest::newRow("older-export") << modified << true;
    modified[1] = "INSERT INTO schema_versions VALUES (44)";
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

    QVERIFY(LogDatabase::instance()->processPendingImport());
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
        {"services/lotw/callbook/tqsl", {}, {}, QString(1024 * 1024, 'x'), false}
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
    if ( arguments.value(1) == "--prepare" || arguments.value(1) == "--recover" )
    {
        interruptionBoundary = arguments.last();
        const bool result = arguments.value(1) == "--prepare"
            ? LogDatabase::instance()->preparePendingImport(arguments.value(2), {}, {}, {})
            : LogDatabase::instance()->processPendingImport();
        return result ? 0 : 1;
    }
#endif
    DatabaseImportTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_databaseimport.moc"
