#include <QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "core/LogParam.h"

class LogParamCloudlogTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void stationID_withoutCallsign_usesGlobalValue();
    void stationID_unknownCallsign_fallsBackToGlobalValue();
    void stationID_perCallsign_isIndependent();
    void stationID_callsignIsNormalized();
    void stationID_setPerCallsign_keepsGlobalValue();
    void stationID_explicitDefaultValue();
    void stationID_callsignWithSlash();
    void apiCallsignsList_roundTrip();

private:
    QTemporaryDir *tempDir = nullptr;
};

void LogParamCloudlogTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));

    tempDir = new QTemporaryDir();
    QVERIFY(tempDir->isValid());

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(tempDir->filePath("test.db"));
    QVERIFY(db.open());

    QSqlQuery query(db);
    QVERIFY(query.exec("CREATE TABLE log_param (name TEXT PRIMARY KEY, value TEXT)"));
}

void LogParamCloudlogTest::cleanupTestCase()
{
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);

    delete tempDir;
    tempDir = nullptr;
}

void LogParamCloudlogTest::init()
{
    // every test starts from a known global value and no callsign-specific values
    QSqlQuery query;
    QVERIFY(query.exec("DELETE FROM log_param WHERE name LIKE 'services/cloudlog/%'"));
    LogParam::clearCache();
    LogParam::setCloudlogStationID(5u);
}

void LogParamCloudlogTest::stationID_withoutCallsign_usesGlobalValue()
{
    QCOMPARE(LogParam::getCloudlogStationID(QString()), 5u);
    QCOMPARE(LogParam::getCloudlogStationID(QStringLiteral("  ")), 5u);

    LogParam::setCloudlogStationID(QString(), 7u);
    QCOMPARE(LogParam::getCloudlogStationID(), 7u);
    QCOMPARE(LogParam::getCloudlogStationID(QString()), 7u);
}

void LogParamCloudlogTest::stationID_unknownCallsign_fallsBackToGlobalValue()
{
    QCOMPARE(LogParam::getCloudlogStationID(QStringLiteral("SQ8BWA")), 5u);
}

void LogParamCloudlogTest::stationID_perCallsign_isIndependent()
{
    LogParam::setCloudlogStationID(QStringLiteral("SQ8BWM"), 19u);
    LogParam::setCloudlogStationID(QStringLiteral("SQ8BWA"), 16u);

    QCOMPARE(LogParam::getCloudlogStationID(QStringLiteral("SQ8BWM")), 19u);
    QCOMPARE(LogParam::getCloudlogStationID(QStringLiteral("SQ8BWA")), 16u);
}

void LogParamCloudlogTest::stationID_callsignIsNormalized()
{
    LogParam::setCloudlogStationID(QStringLiteral(" sq8bwa "), 21u);

    QCOMPARE(LogParam::getCloudlogStationID(QStringLiteral("SQ8BWA")), 21u);
    QCOMPARE(LogParam::getCloudlogStationID(QStringLiteral("sq8bwa")), 21u);
}

void LogParamCloudlogTest::stationID_setPerCallsign_keepsGlobalValue()
{
    // a callsign with its own Wavelog account must not leak its Station ID to other callsigns
    LogParam::setCloudlogStationID(QStringLiteral("SQ8BWA"), 16u);

    QCOMPARE(LogParam::getCloudlogStationID(), 5u);
    QCOMPARE(LogParam::getCloudlogStationID(QStringLiteral("SQ8BWM")), 5u);
}

void LogParamCloudlogTest::stationID_explicitDefaultValue()
{
    QCOMPARE(LogParam::getCloudlogStationID(QStringLiteral("SQ8BWA"), 0u), 0u);

    LogParam::setCloudlogStationID(QStringLiteral("SQ8BWA"), 16u);
    QCOMPARE(LogParam::getCloudlogStationID(QStringLiteral("SQ8BWA"), 0u), 16u);
}

void LogParamCloudlogTest::stationID_callsignWithSlash()
{
    LogParam::setCloudlogStationID(QStringLiteral("SQ8BWM/P"), 22u);
    LogParam::setCloudlogStationID(QStringLiteral("SQ8BWM"), 19u);

    QCOMPARE(LogParam::getCloudlogStationID(QStringLiteral("SQ8BWM/P")), 22u);
    QCOMPARE(LogParam::getCloudlogStationID(QStringLiteral("SQ8BWM")), 19u);
}

void LogParamCloudlogTest::apiCallsignsList_roundTrip()
{
    QCOMPARE(LogParam::getCloudlogAPICallsignsList(), QStringList());

    const QStringList callsigns({QStringLiteral("SQ8BWA"), QStringLiteral("SN0LPU")});
    LogParam::setCloudlogAPICallsignsList(callsigns);
    QCOMPARE(LogParam::getCloudlogAPICallsignsList(), callsigns);

    LogParam::setCloudlogAPICallsignsList({});
    QCOMPARE(LogParam::getCloudlogAPICallsignsList(), QStringList());
}

QTEST_GUILESS_MAIN(LogParamCloudlogTest)

#include "tst_logparamcloudlog.moc"
