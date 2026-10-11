#include <QtTest>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#ifdef Q_OS_UNIX
#include <sys/resource.h>
#include <signal.h>
#endif

#include "core/LogDatabase.h"
#include "core/PlatformParameterManager.h"

class DatabaseImportTest : public QObject
{
    Q_OBJECT

private slots:
    void inspectionRequiresExportSchema_data();
    void inspectionRequiresExportSchema();
    void inspectionDoesNotCreateMissingFile();
    void savedParametersReplacePreviousSelection_data();
    void savedParametersReplacePreviousSelection();
    void parameterWriteFailurePreservesPreviousSelection();
};

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

void DatabaseImportTest::savedParametersReplacePreviousSelection_data()
{
    QTest::addColumn<bool>("emptySelection");
    QTest::newRow("empty") << true;
    QTest::newRow("changed") << false;
}

void DatabaseImportTest::savedParametersReplacePreviousSelection()
{
    QFETCH(bool, emptySelection);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("pending.params");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray stale(R"({"parameters":[{"key":"old","newValue":"stale"}],
        "profilePorts":[{"tableName":"rig_profiles","profileName":"old",
        "columnName":"port_pathname","newValue":"stale","isExec":false}]})");
    QCOMPARE(file.write(stale), qint64(stale.size()));
    file.close();
    QList<PlatformParameter> parameters;
    QList<ProfileParameter> profiles;
    if ( !emptySelection )
    {
        parameters.append({"new", QString(), QString(), "selected", false});
        profiles.append({"rig_profiles", "new", "port_pathname", QString(), QString(), "selected", false});
    }

    QVERIFY(PlatformParameterManager::saveParametersToFile(parameters, profiles, path));
    const QList<PlatformParameter> savedParameters = PlatformParameterManager::loadParametersFromFile(path);
    const QList<ProfileParameter> savedProfiles = PlatformParameterManager::loadProfileParametersFromFile(path);
    QCOMPARE(savedParameters.size(), emptySelection ? 0 : 1);
    QCOMPARE(savedProfiles.size(), emptySelection ? 0 : 1);
    if ( !emptySelection )
    {
        QCOMPARE(savedParameters.first().key, QString("new"));
        QCOMPARE(savedParameters.first().newValue, QString("selected"));
        QCOMPARE(savedProfiles.first().profileName, QString("new"));
        QCOMPARE(savedProfiles.first().newValue, QString("selected"));
    }
}

void DatabaseImportTest::parameterWriteFailurePreservesPreviousSelection()
{
#ifdef Q_OS_UNIX
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("pending.params");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray previous("previous parameters");
    QCOMPARE(file.write(previous), qint64(previous.size()));
    file.close();

    struct rlimit originalLimit;
    QVERIFY(getrlimit(RLIMIT_FSIZE, &originalLimit) == 0);
    struct rlimit restrictedLimit = originalLimit;
    restrictedLimit.rlim_cur = 0;
    const auto previousSignal = signal(SIGXFSZ, SIG_IGN);
    const int restricted = setrlimit(RLIMIT_FSIZE, &restrictedLimit);
    bool result = false;
    if ( restricted == 0 )
        result = PlatformParameterManager::saveParametersToFile({}, {}, path);
    const int restored = setrlimit(RLIMIT_FSIZE, &originalLimit);
    signal(SIGXFSZ, previousSignal);
    QVERIFY(restricted == 0);
    QVERIFY(restored == 0);
    QVERIFY(!result);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), previous);
#else
    QSKIP("Requires the Unix file-size limit to force a real write failure.");
#endif
}

QTEST_GUILESS_MAIN(DatabaseImportTest)
#include "tst_databaseimport.moc"
