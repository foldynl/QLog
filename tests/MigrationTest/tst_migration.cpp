#include <QtTest>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>


#include "core/Migration.h"
#include "core/LogDatabase.h"

class MigrationSqlTest_FriendAccessor
{
public:
    static int latestVersion() { return DBSchemaMigration::latestVersion; }
    static bool createTriggers(DBSchemaMigration &migration) { return migration.createTriggers(); }
    static bool runSqlFile(DBSchemaMigration &migration, const QString &path) { return migration.runSqlFile(path); }
    static bool setForeignKeysEnabled(DBSchemaMigration &migration, bool enabled) { return migration.setForeignKeysEnabled(enabled); }
};

class MigrationSqlTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void migrateVersion_data();
    void migrateVersion();
    void upgradesLegacyDatabase_data();
    void upgradesLegacyDatabase();
    void rejectsForeignKeyViolation();
    void migrationFailureRestoresForeignKeys();
    void sqlFunctionsRejectActiveReplacement();
    void foreignKeyModeRejectsActiveTransaction();
    void missingMigrationSqlFails();

private:
    QScopedPointer<QTemporaryDir> tempDir;
    QString dbPath;

    bool executeSqlFile(int version);
    int currentVersion() const;
    void createDatabase(int version);
};

void MigrationSqlTest::initTestCase()
{
    Q_INIT_RESOURCE(res);

    tempDir.reset(new QTemporaryDir);
    QVERIFY(tempDir->isValid());
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir->path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, tempDir->path());
#ifdef Q_OS_LINUX
    qputenv("XDG_DATA_HOME", tempDir->path().toUtf8());
#else
    QStandardPaths::setTestModeEnabled(true);
#endif
    dbPath = tempDir->filePath(QStringLiteral("migration_sql_test.sqlite"));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(dbPath);
    db.setConnectOptions("QSQLITE_ENABLE_REGEXP");
    QVERIFY2(db.open(), qPrintable(db.lastError().text()));

    QSqlQuery query(QStringLiteral("CREATE TABLE schema_versions (version INTEGER PRIMARY KEY, updated TEXT NOT NULL)"));
    QVERIFY2(query.isActive(), qPrintable(query.lastError().text()));

    QSqlQuery insert(QStringLiteral("INSERT INTO schema_versions (version, updated) VALUES (0, datetime('now'))"));
    QVERIFY2(insert.isActive(), qPrintable(insert.lastError().text()));
}

void MigrationSqlTest::cleanupTestCase()
{
    {
        QSqlDatabase db = QSqlDatabase::database();
        if (db.isValid())
            db.close();
    }
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

bool MigrationSqlTest::executeSqlFile(int version)
{
    const QString resourceName = QStringLiteral(":/res/sql/migration_%1.sql")
                                     .arg(version, 3, 10, QChar('0'));
    QFile sqlFile(resourceName);
    if (!sqlFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QWARN(qPrintable(QStringLiteral("Cannot open %1").arg(resourceName)));
        return false;
    }

    const QString sqlContent = QTextStream(&sqlFile).readAll();
    sqlFile.close();

    const QStringList statements = sqlContent.split('\n').join(QStringLiteral(" ")).split(';');

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);

    if (!db.transaction())
        return false;

    for (const QString &statement : statements)
    {
        const QString trimmed = statement.trimmed();
        if (trimmed.isEmpty())
            continue;

        if (!query.exec(trimmed))
        {
            qWarning() << "SQL execution failed for version" << version
                       << ":" << trimmed << query.lastError();
            db.rollback();
            return false;
        }
    }

    QSqlQuery versionInsert(db);
    if (!versionInsert.prepare(QStringLiteral("INSERT INTO schema_versions (version, updated) VALUES (:version, datetime('now'))")))
    {
        db.rollback();
        return false;
    }
    versionInsert.bindValue(QStringLiteral(":version"), version);
    if (!versionInsert.exec())
    {
        db.rollback();
        return false;
    }

    return db.commit();
}

int MigrationSqlTest::currentVersion() const
{
    QSqlQuery query(QStringLiteral("SELECT MAX(version) FROM schema_versions"));
    if (!query.first())
        return -1;
    return query.value(0).toInt();
}

void MigrationSqlTest::migrateVersion_data()
{
    QTest::addColumn<int>("targetVersion");

    const int latestVersion = MigrationSqlTest_FriendAccessor::latestVersion();
    for (int version = 1; version <= latestVersion; ++version)
    {
        QTest::newRow(QStringLiteral("migration_%1").arg(version, 3, 10, QChar('0')).toUtf8().constData())
            << version;
    }
}

void MigrationSqlTest::migrateVersion()
{
    QFETCH(int, targetVersion);

    const int expectedCurrent = targetVersion - 1;
    QCOMPARE(currentVersion(), expectedCurrent);

    const QList<int> legacyAlertStatuses = {1, 6, 31, 127};
    int existingSubdivisionCount = 0;
    if ( targetVersion == 42 )
    {
        QSqlQuery subdivisions(QStringLiteral("SELECT COUNT(*) FROM adif_enum_secondary_subdivision"));
        QVERIFY(subdivisions.next());
        existingSubdivisionCount = subdivisions.value(0).toInt();

        QSqlQuery insert;
        QVERIFY(insert.prepare(QStringLiteral(
            "INSERT INTO alert_rules(rule_name, dx_logstatus) VALUES (:name, :status)")));

        for ( int status : legacyAlertStatuses )
        {
            insert.bindValue(QStringLiteral(":name"), QStringLiteral("legacy_%1").arg(status));
            insert.bindValue(QStringLiteral(":status"), status);
            QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
        }

        QVERIFY(insert.exec(QStringLiteral(
            "INSERT INTO log_param(name, value) VALUES ('mainwindow/alertbeep', 0)")));
    }

    QVERIFY2(executeSqlFile(targetVersion),
             qPrintable(QStringLiteral("Migration SQL file %1 failed").arg(targetVersion, 3, 10, QChar('0'))));
    QCOMPARE(currentVersion(), targetVersion);

    if ( targetVersion == 42 )
    {
        QSqlQuery query(QStringLiteral(
            "SELECT dx_logstatus, dx_alert_goal, dx_alert_context, "
            "notification_enabled, notification_repeat_minutes "
            "FROM alert_rules ORDER BY dx_logstatus"));
        QVERIFY2(query.isActive(), qPrintable(query.lastError().text()));

        int row = 0;
        while ( query.next() )
        {
            QCOMPARE(query.value(0).toInt(), legacyAlertStatuses.at(row));
            QVERIFY(query.value(1).isNull());
            QVERIFY(query.value(2).isNull());
            QCOMPARE(query.value(3).toInt(), 1);
            QCOMPARE(query.value(4).toInt(), 5);
            ++row;
        }
        QCOMPARE(row, legacyAlertStatuses.size());

        QVERIFY(query.exec(QStringLiteral(
            "SELECT value FROM log_param WHERE name = 'alert/notification/enabled'")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toBool(), false);
        QVERIFY(!query.next());

        QVERIFY(query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM log_param "
            "WHERE name = 'mainwindow/alertbeep'")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);

        QVERIFY(query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM adif_enum_secondary_subdivision WHERE dxcc <> 269")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), existingSubdivisionCount);

        QVERIFY(query.exec(QStringLiteral(
            "SELECT COUNT(*), SUM(aux = 'suspended') "
            "FROM adif_enum_secondary_subdivision WHERE dxcc = 269")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 2482);
        QCOMPARE(query.value(1).toInt(), 3);

        QVERIFY(query.exec(QStringLiteral(
            "SELECT code, subdivision_name, primary_code "
            "FROM adif_enum_secondary_subdivision "
            "WHERE dxcc = 269 AND code IN ('KQ04', 'WA01') ORDER BY code")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("KQ04"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("Marciszów"));
        QCOMPARE(query.value(2).toString(), QStringLiteral("D"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("WA01"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("Andrychów"));
        QCOMPARE(query.value(2).toString(), QStringLiteral("M"));
        QVERIFY(!query.next());
    }
}

void MigrationSqlTest::createDatabase(int version)
{
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    QCoreApplication::setApplicationName(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QVERIFY(QDir().mkpath(LogDatabase::dbDirectory().absolutePath()));
    QVERIFY(LogDatabase::instance()->openDatabase());

    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));
    QVERIFY(query.exec(QStringLiteral("CREATE TABLE schema_versions (version INTEGER PRIMARY KEY, updated TEXT NOT NULL)")));
    QVERIFY(query.exec(QStringLiteral("INSERT INTO schema_versions VALUES (0, datetime('now'))")));
    query.finish();
    for ( int targetVersion = 1; targetVersion <= version; ++targetVersion )
        QVERIFY2(executeSqlFile(targetVersion), qPrintable(QString::number(targetVersion)));

    if ( version >= 17 )
    {
        DBSchemaMigration migration;
        QVERIFY(MigrationSqlTest_FriendAccessor::createTriggers(migration));
    }
}

void MigrationSqlTest::upgradesLegacyDatabase_data()
{
    QTest::addColumn<int>("startVersion");
    QTest::newRow("new") << 0;
    QTest::newRow("version14") << 14;
    QTest::newRow("version15") << 15;
    QTest::newRow("version41") << 41;
    QTest::newRow("version42") << 42;
}

void MigrationSqlTest::upgradesLegacyDatabase()
{
    QFETCH(int, startVersion);
    createDatabase(startVersion);
    if ( QTest::currentTestFailed() )
        return;

    if ( startVersion > 0 )
    {
        QSqlQuery query;
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO contacts(id, callsign, hrdlog_qso_upload_status, hrdlog_qso_upload_date) "
            "VALUES (1, 'OK1TEST', 'R', '2000-01-01'), (2, 'OK2TEST', 'I', '2000-01-02'), "
            "(3, 'OK3TEST', 'Y', '2000-01-03'), (4, 'OK4TEST', 'N', '2000-01-04'), "
            "(5, 'OK5TEST', NULL, NULL)")));
        if ( startVersion >= 16 )
            QVERIFY(query.exec(QStringLiteral(
                "INSERT INTO contacts(id, callsign, hrdlog_qso_upload_status, hrdlog_qso_upload_date) "
                "VALUES (6, 'OK6TEST', 'M', '2000-01-06')")));
    }
    {
        QSqlQuery query;
        QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys=ON")));
    }

    QString snapshot;
    DBSchemaMigration migration;
    QVERIFY(migration.run(&snapshot));
    QCOMPARE(currentVersion(), MigrationSqlTest_FriendAccessor::latestVersion());
    if ( startVersion > 0 )
        QVERIFY(QFileInfo::exists(LogDatabase::dbDirectory().filePath(snapshot)));

    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_key_check")));
    QVERIFY(!query.next());
    QVERIFY(query.exec(QStringLiteral("SELECT translate_to_locale('OK1TEST') COLLATE LOCALEAWARE")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("OK1TEST"));

    if ( startVersion > 0 )
    {
        QVERIFY(query.exec(QStringLiteral("SELECT hrdlog_qso_upload_status, hrdlog_qso_upload_date FROM contacts ORDER BY id")));
        const int contactCount = startVersion >= 16 ? 6 : 5;
        for ( int id = 1; id <= contactCount; ++id )
        {
            QVERIFY(query.next());
            if ( id == 1 || id == 2 || id == 5 )
                QVERIFY(query.value(0).isNull());
            else if ( id == 6 )
                QCOMPARE(query.value(0).toString(), QStringLiteral("M"));
            else
                QCOMPARE(query.value(0).toString(), id == 3 ? QStringLiteral("Y") : QStringLiteral("N"));
            if ( id != 5 )
                QCOMPARE(query.value(1).toString(), QStringLiteral("2000-01-0%1").arg(id));
        }
        QVERIFY(!query.next());
        QVERIFY(query.exec(QStringLiteral("UPDATE contacts SET comment = 'Edited after migration' WHERE id = 1")));
    }
    else
        QVERIFY(query.exec(QStringLiteral("INSERT INTO contacts(id, callsign) VALUES (1, 'OK1TEST')")));

    QVERIFY(query.exec(QStringLiteral("INSERT INTO contacts_qsl_cards(contactid, source, name, data) VALUES (1, 'test', 'test', 'test')")));
    QVERIFY(query.exec(QStringLiteral("DELETE FROM contacts WHERE id = 1")));
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM contacts_qsl_cards")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);
    query.finish();

    // The no-migration path must keep enforcement and registered functions too.
    QVERIFY(migration.run());
    QVERIFY(!query.exec(QStringLiteral("INSERT INTO contacts_qsl_cards(contactid) VALUES (999)")));
}

void MigrationSqlTest::rejectsForeignKeyViolation()
{
    createDatabase(42);
    if ( QTest::currentTestFailed() )
        return;
    {
        QSqlQuery query;
        QVERIFY(query.exec(QStringLiteral("INSERT INTO contacts(id, callsign, hrdlog_qso_upload_status) VALUES (1, 'OK1TEST', 'R')")));
        QVERIFY(query.exec(QStringLiteral("INSERT INTO contacts_qsl_cards(contactid, name) VALUES (999, 'preserve')")));
        QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys=ON")));
    }
    DBSchemaMigration migration;
    QVERIFY(!migration.run());
    QCOMPARE(currentVersion(), 42);
    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    QVERIFY(query.exec(QStringLiteral("SELECT hrdlog_qso_upload_status FROM contacts WHERE id=1")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("R"));
    QVERIFY(query.exec(QStringLiteral("SELECT name FROM contacts_qsl_cards WHERE contactid=999")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("preserve"));
}

void MigrationSqlTest::migrationFailureRestoresForeignKeys()
{
    createDatabase(41);
    if ( QTest::currentTestFailed() )
        return;
    {
        QSqlQuery query;
        QVERIFY(query.exec(QStringLiteral("ALTER TABLE alert_rules ADD dx_alert_goal INTEGER")));
        QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys=ON")));
    }
    DBSchemaMigration migration;
    QVERIFY(!migration.run());
    QCOMPARE(currentVersion(), 41);
    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM pragma_table_info('rig_profiles') WHERE name='cwkey_output'")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);
}

void MigrationSqlTest::sqlFunctionsRejectActiveReplacement()
{
    createDatabase(42);
    if ( QTest::currentTestFailed() )
        return;
    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral("SELECT translate_to_locale('one') UNION ALL SELECT translate_to_locale('two')")));
    QVERIFY(query.next());
    QVERIFY(!LogDatabase::instance()->createSQLFunctions());
    query.finish();
    QVERIFY(LogDatabase::instance()->createSQLFunctions());
}

void MigrationSqlTest::foreignKeyModeRejectsActiveTransaction()
{
    createDatabase(0);
    if ( QTest::currentTestFailed() )
        return;
    DBSchemaMigration migration;
    QVERIFY(MigrationSqlTest_FriendAccessor::setForeignKeysEnabled(migration, true));
    QVERIFY(QSqlDatabase::database().transaction());
    QVERIFY(!MigrationSqlTest_FriendAccessor::setForeignKeysEnabled(migration, false));
    QVERIFY(QSqlDatabase::database().rollback());
    QVERIFY(MigrationSqlTest_FriendAccessor::setForeignKeysEnabled(migration, false));
    QVERIFY(MigrationSqlTest_FriendAccessor::setForeignKeysEnabled(migration, true));
}

void MigrationSqlTest::missingMigrationSqlFails()
{
    createDatabase(42);
    if ( QTest::currentTestFailed() )
        return;
    DBSchemaMigration migration;
    QVERIFY(!MigrationSqlTest_FriendAccessor::runSqlFile(migration, QStringLiteral(":/res/sql/missing_migration.sql")));
    QCOMPARE(currentVersion(), 42);
}

QTEST_MAIN(MigrationSqlTest)

#include "tst_migration.moc"
