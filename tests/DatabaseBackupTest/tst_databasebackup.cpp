#include <QtTest>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <sqlite3.h>

#include "core/DatabaseBackup.h"
#include "core/LogDatabase.h"
#include "core/LogParam.h"

class ReadOnlyWalVfs : public sqlite3_vfs
{
public:
    ReadOnlyWalVfs() : original(sqlite3_vfs_find(nullptr))
    {
        *static_cast<sqlite3_vfs *>(this) = *original;
        zName = "qlog-readonly-wal-test";
        pNext = nullptr;
        xOpen = open;
        sqlite3_vfs_register(this, 1);
    }

    ~ReadOnlyWalVfs()
    {
        sqlite3_vfs_register(original, 1);
        sqlite3_vfs_unregister(this);
    }

private:
    static int open(sqlite3_vfs *vfs, const char *name, sqlite3_file *file, int flags, int *outputFlags)
    {
        ReadOnlyWalVfs *testVfs = static_cast<ReadOnlyWalVfs *>(vfs);
        if ( flags & SQLITE_OPEN_MAIN_DB )
            testVfs->readOnly = flags & SQLITE_OPEN_READONLY;

        // Model platforms that cannot create a missing WAL through a read-only connection.
        if ( (flags & SQLITE_OPEN_WAL) && testVfs->readOnly
             && !QFileInfo::exists(QString::fromUtf8(name)) )
            return SQLITE_CANTOPEN;

        return testVfs->original->xOpen(testVfs->original, name, file, flags, outputFlags);
    }

    sqlite3_vfs *original;
    bool readOnly = false;
};

class DatabaseBackupTest : public QObject
{
    Q_OBJECT

private slots:
    void currentSnapshotIsProtectedAfterClockCorrection();
    void futureSnapshotsDoNotDisplaceCurrentBackup();
    void futureBackupDateDoesNotBlockCurrentBackup();
    void pendingAdxDoesNotSuppressNewSnapshot();
    void existingAdxIsReexportedFromPendingDatabase();
    void existingAdxDoesNotHideFailedExport();
    void externalCopyIsComparedByContent();
    void interruptedExternalCopyKeepsPendingDatabase();
    void init();
    void cleanup();
    void firstLaunchDoesNotCreateBackup();
    void automaticBackupRespectsBuildMode();
    void exportUsesSnapshotAndPreservesUnicode();
    void walSnapshotCanBeExported_data();
    void walSnapshotCanBeExported();
    void successfulExportRemovesSnapshotSidecars();
    void orphanedSnapshotSidecarsAreCleaned();
    void migrationSnapshotSurvivesUntilNextStart();
    void pendingSnapshotIsNotOverwritten();
    void repeatedInterruptionsKeepNewSnapshots();
    void newestSnapshotIsExportedFirst_data();
    void newestSnapshotIsExportedFirst();
    void pendingRetentionProtectsMigrationSnapshots();
    void failedExportPreservesSnapshotAndOlderBackups();
    void externalCopyFailureKeepsInternalBackupAndRetries();
    void externalRetentionDoesNotTouchOtherFiles();
    void externalAliasCannotPruneInternalBackups();
    void interruptedExportIsRetried();
    void everyPendingExportIsCopied();
    void invalidXmlKeepsTheDatabaseCopy();
    void unrelatedDatabaseCopiesAreIgnored();
    void backupSettingsSurviveInvalidChanges();
    void completedBackupRestoresDateAfterInterruptedNotification();

private:
    void createLog();
    void runBackup(const BackupConfig &config = BackupConfig(), const QString &snapshotName = QString());
    QStringList snapshots() const;
    QStringList exports() const;
    QByteArray readFile(const QString &path) const;
    QTemporaryDir storage;
};

void DatabaseBackupTest::init()
{
    QVERIFY(storage.isValid());
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
#ifdef Q_OS_LINUX
    qputenv("XDG_DATA_HOME", storage.path().toUtf8());
#else
    QStandardPaths::setTestModeEnabled(true);
#endif
    QCoreApplication::setApplicationName(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QVERIFY(QDir().mkpath(LogDatabase::dbDirectory().absolutePath()));
    QVERIFY(LogDatabase::instance()->openDatabase());
}

void DatabaseBackupTest::cleanup()
{
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
#ifndef Q_OS_LINUX
    QVERIFY(LogDatabase::dbDirectory().removeRecursively());
#endif
}

void DatabaseBackupTest::createLog()
{
    QSqlQuery query;
    QVERIFY(query.exec("CREATE TABLE log_param (name TEXT PRIMARY KEY, value TEXT)"));
    QVERIFY(query.exec("CREATE TABLE contacts (callsign TEXT, start_time TEXT, name_intl TEXT)"));
    QVERIFY(query.prepare("INSERT INTO contacts VALUES ('OK1ABC', '2026-01-01T12:00:00Z', :name)"));
    query.bindValue(":name", QString::fromUtf8("Jiří"));
    QVERIFY(query.exec());
    QVERIFY(LogParam::setLastBackupDate(QDate()));
    QVERIFY(LogParam::setBackupError(QString()));
}

void DatabaseBackupTest::runBackup(const BackupConfig &config, const QString &snapshotName)
{
    DatabaseBackup backup(config, snapshotName);
    QSignalSpy finished(&backup, &QThread::finished);
    backup.start();
    QTRY_COMPARE(finished.count(), 1);
}

QStringList DatabaseBackupTest::snapshots() const
{
    return LogDatabase::dbDirectory().entryList({"qlog_backup_*.db"}, QDir::Files, QDir::Name);
}

QStringList DatabaseBackupTest::exports() const
{
    return LogDatabase::dbDirectory().entryList({"qlog_backup_*.adx"}, QDir::Files, QDir::Name);
}

QByteArray DatabaseBackupTest::readFile(const QString &path) const
{
    QFile file(path);
    if ( !file.open(QIODevice::ReadOnly) )
        return QByteArray();
    return file.readAll();
}

void DatabaseBackupTest::firstLaunchDoesNotCreateBackup()
{
    QVERIFY(DatabaseBackup::prepare(true));
    createLog();
    runBackup();
    QVERIFY(snapshots().isEmpty());
    QVERIFY(exports().isEmpty());
}

void DatabaseBackupTest::automaticBackupRespectsBuildMode()
{
    createLog();
    QVERIFY(LogParam::setLastBackupDate(QDate::currentDate()));
    QFile existing(LogDatabase::dbDirectory().filePath("qlog_backup_" + QDate::currentDate().toString("yyyyMMdd") + ".adx"));
    QVERIFY(existing.open(QIODevice::WriteOnly));
    QCOMPARE(existing.write("previous backup"), qint64(15));
    existing.close();
    QVERIFY(DatabaseBackup::prepare());
#ifdef QLOG_FORCE_AUTO_BACKUP
    QCOMPARE(snapshots().size(), 1);
    QVERIFY(!snapshots().first().endsWith(".migration.db"));
    runBackup();
    QCOMPARE(exports().size(), 2);
#else
    QVERIFY(snapshots().isEmpty());
#endif
    QCOMPARE(readFile(existing.fileName()), QByteArray("previous backup"));
}

void DatabaseBackupTest::exportUsesSnapshotAndPreservesUnicode()
{
    createLog();
    QVERIFY(DatabaseBackup::prepare());
    QCOMPARE(snapshots().size(), 1);
    QSqlQuery query;
    QVERIFY(query.exec("UPDATE contacts SET callsign='OK2XYZ'"));
    runBackup();
    QCOMPARE(exports().size(), 1);
    const QByteArray output = readFile(LogDatabase::dbDirectory().filePath(exports().first()));
    QVERIFY(output.contains("OK1ABC"));
    QVERIFY(!output.contains("OK2XYZ"));
    QVERIFY(output.contains(QString::fromUtf8("Jiří").toUtf8()));
    QXmlStreamReader reader(output);
    while ( !reader.atEnd() )
        reader.readNext();
    QVERIFY(!reader.hasError());
    QVERIFY(snapshots().isEmpty());
    QCOMPARE(LogParam::getLastBackupDate(), QDate::currentDate());
    QVERIFY(DatabaseBackup::prepare());
    QVERIFY(snapshots().isEmpty());
}

void DatabaseBackupTest::walSnapshotCanBeExported_data()
{
    QTest::addColumn<bool>("committedWal");
    QTest::newRow("missing-sidecars") << false;
    QTest::newRow("committed-wal") << true;
}

void DatabaseBackupTest::walSnapshotCanBeExported()
{
    QFETCH(bool, committedWal);
    createLog();
    QVERIFY(DatabaseBackup::prepare(true));
    const QString snapshot = LogDatabase::dbDirectory().filePath(snapshots().first());
    QCOMPARE(readFile(snapshot).at(18), char(2));
    QVERIFY(!QFile::exists(snapshot + "-wal"));
    QVERIFY(!QFile::exists(snapshot + "-shm"));

    if ( committedWal )
    {
        sqlite3 *database = nullptr;
        QCOMPARE(sqlite3_open(snapshot.toUtf8().constData(), &database), SQLITE_OK);
        const int configured = sqlite3_db_config(database, SQLITE_DBCONFIG_NO_CKPT_ON_CLOSE, 1, nullptr);
        const int updated = sqlite3_exec(database, "UPDATE contacts SET callsign='OK2XYZ'", nullptr, nullptr, nullptr);
        const int closed = sqlite3_close(database);
        QCOMPARE(configured, SQLITE_OK);
        QCOMPARE(updated, SQLITE_OK);
        QCOMPARE(closed, SQLITE_OK);
        QVERIFY(QFileInfo(snapshot + "-wal").size() > 0);
    }

    ReadOnlyWalVfs vfs;
    runBackup();
    QCOMPARE(exports().size(), 1);
    const QByteArray output = readFile(LogDatabase::dbDirectory().filePath(exports().first()));
    QVERIFY(output.contains(committedWal ? "OK2XYZ" : "OK1ABC"));
    QVERIFY(LogParam::getBackupError().isEmpty());
    QCOMPARE(LogDatabase::dbDirectory().entryList({"*.db.ready"}, QDir::Files).size(), 1);
}

void DatabaseBackupTest::successfulExportRemovesSnapshotSidecars()
{
    createLog();
    QVERIFY(DatabaseBackup::prepare());
    const QString snapshot = LogDatabase::dbDirectory().filePath(snapshots().first());
    runBackup();
    QCOMPARE(exports().size(), 1);
    QVERIFY(!QFile::exists(snapshot));
    QVERIFY(!QFile::exists(snapshot + "-wal"));
    QVERIFY(!QFile::exists(snapshot + "-shm"));
    QVERIFY(QFile::exists(LogDatabase::dbFilename() + "-wal"));
    QVERIFY(QFile::exists(LogDatabase::dbFilename() + "-shm"));
    QSqlQuery query;
    QVERIFY(query.exec("SELECT callsign FROM contacts"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("OK1ABC"));
}

void DatabaseBackupTest::orphanedSnapshotSidecarsAreCleaned()
{
    createLog();
    const QStringList removed = {"qlog_backup_20260101000000000.db",
                                 "qlog_backup_20260101000000001.migration.db",
                                 "qlog_backup_20260101000000002.migration.db.ready",
                                 "qlog_backup_20260101000000003.db.tmp"};
    const QStringList kept = {"qlog_backup_20260101000000004.db", "qlog_backup_20260101.db"};
    QStringList names = removed;
    names.append(kept);
    for ( const QString &name : names )
    {
        for ( const QString &suffix : {QString("-wal"), QString("-shm")} )
        {
            QFile file(LogDatabase::dbDirectory().filePath(name + suffix));
            QVERIFY(file.open(QIODevice::WriteOnly));
            const QByteArray contents = name == kept.first() ? QByteArray("preserve recovery data")
                                       : suffix == "-shm" ? QByteArray("unused shared memory") : QByteArray();
            QCOMPARE(file.write(contents), qint64(contents.size()));
        }
    }
    runBackup();
    for ( const QString &name : names )
    {
        for ( const QString &suffix : {QString("-wal"), QString("-shm")} )
        {
            const QString path = LogDatabase::dbDirectory().filePath(name + suffix);
            QCOMPARE(QFile::exists(path), kept.contains(name));
            if ( name == kept.first() )
                QCOMPARE(readFile(path), QByteArray("preserve recovery data"));
        }
    }
}

void DatabaseBackupTest::migrationSnapshotSurvivesUntilNextStart()
{
    createLog();
    QVERIFY(DatabaseBackup::prepare(true));
    runBackup();
    QCOMPARE(exports().size(), 1);
    QCOMPARE(LogDatabase::dbDirectory().entryList({"*.db.ready"}, QDir::Files).size(), 1);
    runBackup();
    QVERIFY(LogDatabase::dbDirectory().entryList({"*.db.ready"}, QDir::Files).isEmpty());
}

void DatabaseBackupTest::pendingSnapshotIsNotOverwritten()
{
    createLog();
    QVERIFY(DatabaseBackup::prepare());
    const QString pending = snapshots().first();
    const QByteArray original = readFile(LogDatabase::dbDirectory().filePath(pending));
    QSqlQuery query;
    QVERIFY(query.exec("UPDATE contacts SET callsign='OK2XYZ'"));
    QVERIFY(DatabaseBackup::prepare());
    QCOMPARE(snapshots().size(), 2);
    QCOMPARE(readFile(LogDatabase::dbDirectory().filePath(pending)), original);
    QVERIFY(DatabaseBackup::prepare(true));
    QCOMPARE(snapshots().size(), 3);
    QCOMPARE(readFile(LogDatabase::dbDirectory().filePath(pending)), original);
}

void DatabaseBackupTest::repeatedInterruptionsKeepNewSnapshots()
{
    createLog();
    QSqlQuery query;
    const QStringList callsigns = {"OK1ABC", "OK2XYZ", "OK3DEF"};
    for ( const QString &callsign : callsigns )
    {
        QVERIFY(query.prepare("UPDATE contacts SET callsign=:callsign"));
        query.bindValue(":callsign", callsign);
        QVERIFY(query.exec());
        QVERIFY(DatabaseBackup::prepare());
        DatabaseBackup backup{BackupConfig()};
        connect(&backup, &QThread::started, &backup, &QThread::requestInterruption, Qt::DirectConnection);
        QSignalSpy finished(&backup, &QThread::finished);
        backup.start();
        QTRY_COMPARE(finished.count(), 1);
    }
    QCOMPARE(snapshots().size(), callsigns.size());
    QVERIFY(exports().isEmpty());
    runBackup();
    QCOMPARE(exports().size(), callsigns.size());
    QVERIFY(snapshots().isEmpty());
    for ( int index = 0; index < callsigns.size(); ++index )
        QVERIFY(readFile(LogDatabase::dbDirectory().filePath(exports().at(index))).contains(callsigns.at(index).toUtf8()));
}

void DatabaseBackupTest::newestSnapshotIsExportedFirst_data()
{
    QTest::addColumn<bool>("migration");
    QTest::newRow("ordinary") << false;
    QTest::newRow("migration") << true;
}

void DatabaseBackupTest::newestSnapshotIsExportedFirst()
{
    QFETCH(bool, migration);
    createLog();
    QSqlQuery query;
    QVERIFY(query.prepare("UPDATE contacts SET name_intl=:name"));
    query.bindValue(":name", QStringLiteral("Invalid") + QChar(1));
    QVERIFY(query.exec());
    QVERIFY(DatabaseBackup::prepare());
    const QString pending = snapshots().first();
    QVERIFY(query.exec("UPDATE contacts SET name_intl='Valid', callsign='OK2XYZ'"));
    QString freshSnapshot;
    QVERIFY(DatabaseBackup::prepare(migration, &freshSnapshot));

    BackupConfig config;
    config.enabled = true;
    config.directory = storage.filePath(migration ? "priority-copy-migration" : "priority-copy-ordinary");
    // Stop on the older invalid snapshot; the newer ADX must already be published.
    const QtMessageHandler previousHandler = qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &message)
    {
        if ( type == QtWarningMsg && message.startsWith(QStringLiteral("Backup export failed")) )
            QThread::currentThread()->requestInterruption();
    });
    runBackup(config, freshSnapshot);
    qInstallMessageHandler(previousHandler);

    QCOMPARE(exports().size(), 1);
    QVERIFY(readFile(LogDatabase::dbDirectory().filePath(exports().first())).contains("OK2XYZ"));
    QVERIFY(QFile::exists(LogDatabase::dbDirectory().filePath(pending)));
    const QString completed = LogDatabase::dbDirectory().filePath(freshSnapshot);
    QVERIFY(!QFile::exists(completed));
    QCOMPARE(QFile::exists(completed + ".ready"), migration);
    // Interruption in the next export must not leave the completed snapshot's sidecars.
    QVERIFY(!QFile::exists(completed + "-wal"));
    QVERIFY(!QFile::exists(completed + "-shm"));
    QCOMPARE(QDir(config.directory).entryList({"*.adx"}, QDir::Files).size(), 1);
    QCOMPARE(readFile(QDir(config.directory).filePath(exports().first())),
             readFile(LogDatabase::dbDirectory().filePath(exports().first())));
}

void DatabaseBackupTest::pendingRetentionProtectsMigrationSnapshots()
{
    createLog();
    const QDir directory = LogDatabase::dbDirectory();
    QVERIFY(DatabaseBackup::prepare(true));
    const QString ready = snapshots().first() + ".ready";
    QVERIFY(QFile::rename(directory.filePath(snapshots().first()), directory.filePath(ready)));
    const QByteArray readyContents = readFile(directory.filePath(ready));
    QVERIFY(DatabaseBackup::prepare(true));
    const QString migration = snapshots().first();
    const QByteArray migrationContents = readFile(directory.filePath(migration));
    const QString oldest = "qlog_backup_20000101000000000.db";
    const QString recovery = "qlog_backup_20000201000000000.db";
    const QString legacy = "qlog_backup_20000101.db";
    for ( const QString &name : {oldest, oldest + "-wal", oldest + "-shm",
                                recovery, recovery + "-wal", recovery + "-shm", legacy} )
    {
        QFile file(directory.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        if ( !name.endsWith("-wal") || name.startsWith(recovery) )
            QCOMPARE(file.write("preserved data"), qint64(14));
    }
    for ( int index = 0; index < 12; ++index )
        QVERIFY(DatabaseBackup::prepare());

    QStringList ordinary = snapshots();
    ordinary.removeAll(migration);
    ordinary.removeAll(recovery);
    ordinary.removeAll(legacy);
    QCOMPARE(ordinary.size(), 10);
    QCOMPARE(readFile(directory.filePath(migration)), migrationContents);
    QCOMPARE(readFile(directory.filePath(ready)), readyContents);
    QCOMPARE(readFile(directory.filePath(legacy)), QByteArray("preserved data"));
    for ( const QString &suffix : {QString(), QString("-wal"), QString("-shm")} )
        QCOMPARE(readFile(directory.filePath(recovery + suffix)), QByteArray("preserved data"));
    QVERIFY(!LogParam::getBackupError().isEmpty());
    QVERIFY(!QFile::exists(directory.filePath(oldest)));
    QVERIFY(!QFile::exists(directory.filePath(oldest + "-wal")));
    QVERIFY(!QFile::exists(directory.filePath(oldest + "-shm")));
}

void DatabaseBackupTest::failedExportPreservesSnapshotAndOlderBackups()
{
    createLog();
    QVERIFY(DatabaseBackup::prepare());
    const QString pending = snapshots().first();
    const QString target = pending.left(pending.size() - 3) + ".adx";
    QVERIFY(LogDatabase::dbDirectory().mkdir(target));
    for ( int day = 1; day <= 11; ++day )
    {
        QFile file(LogDatabase::dbDirectory().filePath(QString("qlog_backup_202501%1.adx").arg(day, 2, 10, QLatin1Char('0'))));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("previous backup"), qint64(15));
    }
    runBackup();
    QCOMPARE(snapshots(), QStringList{pending});
    QCOMPARE(exports().size(), 11);
    QVERIFY(!LogParam::getLastBackupDate().isValid());
    QVERIFY(!LogParam::getBackupError().isEmpty());

    QVERIFY(LogDatabase::dbDirectory().rmdir(target));
    runBackup();
    QVERIFY(snapshots().isEmpty());
    const QByteArray output = readFile(LogDatabase::dbDirectory().filePath(target));
    QVERIFY(output.contains("OK1ABC"));
    QVERIFY(output.contains(QString::fromUtf8("Jiří").toUtf8()));
}

void DatabaseBackupTest::externalCopyFailureKeepsInternalBackupAndRetries()
{
    createLog();
    BackupConfig config;
    config.enabled = true;
    config.directory = storage.filePath("external");
    config.copies = 2;
    QFile blocked(config.directory);
    QVERIFY(blocked.open(QIODevice::WriteOnly));
    blocked.close();
    QVERIFY(DatabaseBackup::prepare());
    runBackup(config);
    QCOMPARE(exports().size(), 1);
    QCOMPARE(snapshots().size(), 1);
    QVERIFY(blocked.remove());
    runBackup(config);
    QVERIFY(snapshots().isEmpty());
    const QString name = exports().first();
    QCOMPARE(readFile(QDir(config.directory).filePath(name)),
             readFile(LogDatabase::dbDirectory().filePath(name)));
}

void DatabaseBackupTest::externalRetentionDoesNotTouchOtherFiles()
{
    createLog();
    BackupConfig config;
    config.enabled = true;
    config.directory = storage.filePath("retention");
    config.copies = 2;
    QVERIFY(QDir().mkpath(config.directory));
    for ( const QString &name : QStringList{"qlog_backup_20250101.adx", "qlog_backup_20250102.adx", "operator.adx"} )
    {
        QFile file(QDir(config.directory).filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("previous backup"), qint64(15));
    }
    QVERIFY(DatabaseBackup::prepare());
    runBackup(config);
    const QStringList files = QDir(config.directory).entryList({"*.adx"}, QDir::Files);
    QCOMPARE(files.size(), 3);
    QVERIFY(files.contains("operator.adx"));
    QVERIFY(files.contains("qlog_backup_20250102.adx"));
    QVERIFY(!files.contains("qlog_backup_20250101.adx"));
}

void DatabaseBackupTest::externalAliasCannotPruneInternalBackups()
{
#ifndef Q_OS_UNIX
    QSKIP("This case exercises a Unix directory symlink.");
#else
    createLog();
    for ( int day = 1; day <= 3; ++day )
    {
        QFile file(LogDatabase::dbDirectory().filePath(QString("qlog_backup_202501%1.adx").arg(day, 2, 10, QLatin1Char('0'))));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("previous backup"), qint64(15));
    }
    BackupConfig config;
    config.enabled = true;
    config.directory = storage.filePath("internal-alias");
    config.copies = 1;
    QVERIFY(QFile::link(LogDatabase::dbDirectory().absolutePath(), config.directory));
    runBackup(config);
    QCOMPARE(exports().size(), 3);
#endif
}

void DatabaseBackupTest::interruptedExportIsRetried()
{
    createLog();
    QVERIFY(DatabaseBackup::prepare());
    const QStringList pending = snapshots();
    {
        DatabaseBackup backup{BackupConfig()};
        connect(&backup, &QThread::started, &backup, &QThread::requestInterruption, Qt::DirectConnection);
        QSignalSpy finished(&backup, &QThread::finished);
        backup.start();
        QTRY_COMPARE(finished.count(), 1);
    }
    QCOMPARE(snapshots(), pending);
    QVERIFY(exports().isEmpty());
    QVERIFY(!LogParam::getLastBackupDate().isValid());
    runBackup();
    QVERIFY(snapshots().isEmpty());
    QCOMPARE(exports().size(), 1);
}

void DatabaseBackupTest::everyPendingExportIsCopied()
{
    createLog();
    QVERIFY(DatabaseBackup::prepare());
    QVERIFY(DatabaseBackup::prepare(true));
    BackupConfig config;
    config.enabled = true;
    config.directory = storage.filePath("pending-exports");
    runBackup(config);
    QCOMPARE(exports().size(), 2);
    QCOMPARE(QDir(config.directory).entryList({"*.adx"}, QDir::Files).size(), 2);
}

void DatabaseBackupTest::invalidXmlKeepsTheDatabaseCopy()
{
    createLog();
    QSqlQuery query;
    QVERIFY(query.prepare("UPDATE contacts SET name_intl=:name"));
    query.bindValue(":name", QStringLiteral("Invalid") + QChar(1));
    QVERIFY(query.exec());
    QVERIFY(DatabaseBackup::prepare());
    runBackup();
    QVERIFY(exports().isEmpty());
    QCOMPARE(snapshots().size(), 1);
    QVERIFY(!LogParam::getLastBackupDate().isValid());
}

void DatabaseBackupTest::unrelatedDatabaseCopiesAreIgnored()
{
    createLog();
    const QStringList names{"qlog_backup_20250101.db", "qlog_backup_20250101.db.tmp"};
    for ( const QString &name : names )
    {
        QFile file(LogDatabase::dbDirectory().filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("operator copy"), qint64(13));
    }
    runBackup();
    for ( const QString &name : names )
        QCOMPARE(readFile(LogDatabase::dbDirectory().filePath(name)), QByteArray("operator copy"));
    QVERIFY(exports().isEmpty());
    QVERIFY(LogParam::getBackupError().isEmpty());
}

void DatabaseBackupTest::backupSettingsSurviveInvalidChanges()
{
    createLog();
    BackupConfig config;
    config.enabled = true;
    config.directory = storage.filePath(QString::fromUtf8("Jiří"));
    config.copies = 3;
    QVERIFY(LogParam::setBackupConfig(config));
    QCOMPARE(LogParam::getBackupConfig().directory, config.directory);
    QCOMPARE(LogParam::getBackupConfig().copies, 3);
    QVERIFY(LogParam::getBackupConfig().enabled);
    config.copies = 0;
    QVERIFY(!LogParam::setBackupConfig(config));
    QCOMPARE(LogParam::getBackupConfig().copies, 3);
    config.copies = 3;
    config.directory.clear();
    QVERIFY(!LogParam::setBackupConfig(config));
    QVERIFY(!LogParam::getBackupConfig().directory.isEmpty());
}

void DatabaseBackupTest::completedBackupRestoresDateAfterInterruptedNotification()
{
    createLog();
    QVERIFY(DatabaseBackup::prepare());
    runBackup();
    QVERIFY(LogParam::setLastBackupDate(QDate()));
    QVERIFY(DatabaseBackup::prepare());
    QVERIFY(snapshots().isEmpty());
    QCOMPARE(LogParam::getLastBackupDate(), QDate::currentDate());
}


void DatabaseBackupTest::futureSnapshotsDoNotDisplaceCurrentBackup()
{
    createLog();
    const QDir directory = LogDatabase::dbDirectory();
    const QString futureDay = QDate::currentDate().addYears(10).toString("yyyyMMdd");
    for ( int index = 0; index < 10; ++index )
        QVERIFY(LogDatabase::instance()->atomicCopy(QString("qlog_backup_%1%2.db").arg(futureDay).arg(index, 9, 10, QLatin1Char('0'))));
    QSqlQuery query;
    QVERIFY(query.exec("UPDATE contacts SET callsign='OK2XYZ'"));
    QVERIFY(DatabaseBackup::prepare());
    QCOMPARE(snapshots().size(), 10);
    const QString todayPrefix = "qlog_backup_" + QDate::currentDate().toString("yyyyMMdd");
    QString current;
    for ( const QString &name : snapshots() )
    {
        if ( name.startsWith(todayPrefix) )
            current = name.left(name.size()-3) + ".adx";
    }
    QVERIFY(!current.isEmpty());
    // Fill ADX retention too; it must not discard the current export.
    for ( int index = 10; index < 20; ++index )
    {
        QFile file(directory.filePath(QString("qlog_backup_%1%2.adx").arg(futureDay).arg(index, 9, 10, QLatin1Char('0'))));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("old future backup"), qint64(17));
    }
    BackupConfig config;
    config.enabled = true;
    config.directory = storage.filePath("future-copy");
    config.copies = 1;
    runBackup(config);
    QCOMPARE(exports().size(), 10);
    QVERIFY(readFile(directory.filePath(current)).contains("OK2XYZ"));
    QCOMPARE(QDir(config.directory).entryList({"*.adx"}, QDir::Files), QStringList{current});
    QCOMPARE(readFile(QDir(config.directory).filePath(current)), readFile(directory.filePath(current)));
    QCOMPARE(LogParam::getLastBackupDate(), QDate::currentDate());
}
void DatabaseBackupTest::futureBackupDateDoesNotBlockCurrentBackup()
{
    createLog();
    const QDate future = QDate::currentDate().addYears(10);
    QVERIFY(LogParam::setLastBackupDate(future));
    QFile file(LogDatabase::dbDirectory().filePath("qlog_backup_" + future.toString("yyyyMMdd") + "000000000.adx"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("old future backup"), qint64(17));
    file.close();
    QVERIFY(DatabaseBackup::prepare());
    QCOMPARE(snapshots().size(), 1);
    runBackup();
    QCOMPARE(LogParam::getLastBackupDate(), QDate::currentDate());
}
void DatabaseBackupTest::existingAdxIsReexportedFromPendingDatabase()
{
    createLog();
    QVERIFY(DatabaseBackup::prepare());
    const QString snapshot = snapshots().first();
    const QString name = snapshot.left(snapshot.size()-3) + ".adx";
    QFile file(LogDatabase::dbDirectory().filePath(name));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    runBackup();
    QVERIFY(snapshots().isEmpty());
    const QByteArray output = readFile(file.fileName());
    QVERIFY(output.contains("OK1ABC"));
    QXmlStreamReader reader(output);
    while ( !reader.atEnd() )
        reader.readNext();
    QVERIFY(!reader.hasError());
}
void DatabaseBackupTest::existingAdxDoesNotHideFailedExport()
{
    createLog();
    QSqlQuery query;
    QVERIFY(query.prepare("UPDATE contacts SET name_intl=:name"));
    query.bindValue(":name", QStringLiteral("Invalid") + QChar(1));
    QVERIFY(query.exec());
    QVERIFY(DatabaseBackup::prepare());
    const QString snapshot = snapshots().first();
    QFile file(LogDatabase::dbDirectory().filePath(snapshot.left(snapshot.size()-3) + ".adx"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("previous ADX"), qint64(12));
    file.close();
    runBackup();
    QCOMPARE(snapshots(), QStringList{snapshot});
    QCOMPARE(readFile(file.fileName()), QByteArray("previous ADX"));
    QVERIFY(!LogParam::getBackupError().isEmpty());
}
void DatabaseBackupTest::externalCopyIsComparedByContent()
{
    createLog();
    QSqlQuery query;
    QVERIFY(query.prepare("UPDATE contacts SET name_intl=:name"));
    query.bindValue(":name", QString(140000, QLatin1Char('A')));
    QVERIFY(query.exec());
    QVERIFY(DatabaseBackup::prepare());
    runBackup();
    BackupConfig config;
    config.enabled = true;
    config.directory = storage.filePath("same-size-copy");
    QVERIFY(QDir().mkpath(config.directory));
    const QString name = exports().first();
    const QByteArray correct = readFile(LogDatabase::dbDirectory().filePath(name));
    QFile file(QDir(config.directory).filePath(name));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QByteArray wrong = correct;
    wrong[wrong.size()-1] = 'X';
    QCOMPARE(file.write(wrong), qint64(wrong.size()));
    file.close();
    runBackup(config);
    QCOMPARE(readFile(file.fileName()), correct);
    // An identical destination must not be rewritten.
    QVERIFY(file.open(QIODevice::ReadWrite));
    const QDateTime modified(QDate(2000, 1, 1), QTime(0, 0));
    QVERIFY(file.setFileTime(modified, QFileDevice::FileModificationTime));
    file.close();
    runBackup(config);
    QCOMPARE(readFile(file.fileName()), correct);
    QCOMPARE(QFileInfo(file.fileName()).lastModified(), modified);
}
void DatabaseBackupTest::interruptedExternalCopyKeepsPendingDatabase()
{
    createLog();
    QVERIFY(DatabaseBackup::prepare());
    const QString pending = snapshots().first();
    BackupConfig config;
    config.enabled = true;
    config.directory = storage.filePath("interrupted-copy");
    const QtMessageHandler previousHandler = qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &message)
    {
        if ( type == QtWarningMsg && message.startsWith(QStringLiteral("Cannot copy additional backup")) )
            QThread::currentThread()->requestInterruption();
    });
    // A directory at the destination filename forces the copy to fail.
    QVERIFY(QDir().mkpath(QDir(config.directory).filePath(pending.left(pending.size()-3) + ".adx")));
    runBackup(config);
    qInstallMessageHandler(previousHandler);
    QCOMPARE(snapshots(), QStringList{pending});
    QCOMPARE(exports().size(), 1);
}


void DatabaseBackupTest::pendingAdxDoesNotSuppressNewSnapshot()
{
    createLog();
    QVERIFY(DatabaseBackup::prepare());
    const QString pending = snapshots().first();
    QFile file(LogDatabase::dbDirectory().filePath(pending.left(pending.size()-3) + ".adx"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    QVERIFY(DatabaseBackup::prepare());
    QCOMPARE(snapshots().size(), 2);
    QVERIFY(!LogParam::getLastBackupDate().isValid());
}


void DatabaseBackupTest::currentSnapshotIsProtectedAfterClockCorrection()
{
    createLog();
    QString snapshotName;
    QVERIFY(DatabaseBackup::prepare(false, &snapshotName));
    QVERIFY(!snapshotName.isEmpty());
    // Simulate correction after capture by giving the current snapshot a future timestamp.
    const QString current = "qlog_backup_" + QDate::currentDate().addYears(10).toString("yyyyMMdd") + "000000000.db";
    const QDir directory = LogDatabase::dbDirectory();
    QVERIFY(QFile::rename(directory.filePath(snapshotName), directory.filePath(current)));
    for ( int day = 1; day <= 11; ++day )
    {
        const QString oldStem = "qlog_backup_" + QDate::currentDate().addDays(-day).toString("yyyyMMdd") + "000000000";
        QFile file(directory.filePath(oldStem + ".adx"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("older ADX"), qint64(9));
        if ( day == 1 )
        {
            QSqlQuery query;
            QVERIFY(query.prepare("UPDATE contacts SET name_intl=:name"));
            query.bindValue(":name", QStringLiteral("Invalid") + QChar(1));
            QVERIFY(query.exec());
            QVERIFY(LogDatabase::instance()->atomicCopy(oldStem + ".db"));
        }
    }
    BackupConfig config;
    config.enabled = true;
    config.directory = storage.filePath("clock-correction-copy");
    config.copies = 1;
    const QtMessageHandler previousHandler = qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &message)
    {
        if ( type == QtWarningMsg && message.startsWith(QStringLiteral("Backup export failed")) )
            QThread::currentThread()->requestInterruption();
    });
    runBackup(config, current);
    qInstallMessageHandler(previousHandler);
    const QString currentAdx = current.left(current.size() - 3) + ".adx";
    QVERIFY(readFile(directory.filePath(currentAdx)).contains("OK1ABC"));
    QCOMPARE(QDir(config.directory).entryList({"*.adx"}, QDir::Files), QStringList{currentAdx});
    QVERIFY(!QFile::exists(directory.filePath(current)));
    // A subsequent complete run must also protect this startup's ADX during internal retention.
    QFile invalid(directory.filePath("qlog_backup_" + QDate::currentDate().addDays(-1).toString("yyyyMMdd") + "000000000.db"));
    QVERIFY(invalid.remove());
    QSqlQuery query;
    QVERIFY(query.exec("UPDATE contacts SET name_intl='Valid'"));
    QVERIFY(LogDatabase::instance()->atomicCopy(current));
    runBackup(config, current);
    QCOMPARE(exports().size(), 10);
    QVERIFY(QFile::exists(directory.filePath(currentAdx)));
    QCOMPARE(QDir(config.directory).entryList({"*.adx"}, QDir::Files), QStringList{currentAdx});
}

QTEST_GUILESS_MAIN(DatabaseBackupTest)
#include "tst_databasebackup.moc"
