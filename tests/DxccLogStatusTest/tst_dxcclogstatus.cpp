#include <QtTest>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include "core/LogParam.h"

class DxccLogStatusTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void legacyFallbackAndRoundTrip();
    void failedWriteKeepsPreviousCondition();
};

void DxccLogStatusTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY2(db.open(), qPrintable(db.lastError().text()));
    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral("CREATE TABLE log_param (name TEXT PRIMARY KEY, value TEXT)")));
}

void DxccLogStatusTest::cleanupTestCase()
{
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

void DxccLogStatusTest::legacyFallbackAndRoundTrip()
{
    QCOMPARE(LogParam::getWsjtxFilterLogStatus().mask, DxccStatus::All & ~DxccStatus::UnknownStatus);
    QCOMPARE(LogParam::getDXCFilterLogStatus().mask, DxccStatus::All & ~DxccStatus::UnknownStatus);
    QVERIFY(!LogParam::getWsjtxFilterLogStatus().isFiltered());
    QVERIFY(!LogParam::getDXCFilterLogStatus().isFiltered());

    LogParam::setWsjtxFilterDxccStatus(NewMode);
    LogParam::setDXCFilterDxccStatus(NewBand);
    QCOMPARE(LogParam::getWsjtxFilterLogStatus().mask, static_cast<int>(NewMode));
    QCOMPARE(LogParam::getDXCFilterLogStatus().mask, static_cast<int>(NewBand));
    QVERIFY(!LogParam::getDXCFilterLogStatus().goalSet);

    const DxccLogStatusCondition legacy = LogParam::getWsjtxFilterLogStatus();
    QVERIFY(LogParam::setWsjtxFilterLogStatus(legacy));
    QCOMPARE(LogParam::getWsjtxFilterLogStatus().mask, legacy.mask);
    QVERIFY(!LogParam::getWsjtxFilterLogStatus().goalSet);
    QVERIFY(!LogParam::getWsjtxFilterLogStatus().contextSet);

    DxccLogStatusCondition condition;
    condition.goalSet = condition.contextSet = true;
    condition.context = static_cast<int>(AlertDxccContext::Mode);
    QVERIFY(LogParam::setDXCFilterLogStatus(condition));
    QCOMPARE(LogParam::getDXCFilterLogStatus().toJson(), condition.toJson());
    QCOMPARE(LogParam::getWsjtxFilterLogStatus().mask, legacy.mask);
    // The new condition does not overwrite the original operator's bitmap.
    QCOMPARE(LogParam::getDXCFilterDxccStatus(), static_cast<uint>(NewBand));

    condition.goalSet = false;
    QVERIFY(LogParam::setDXCFilterLogStatus(condition));
    QVERIFY(LogParam::getDXCFilterLogStatus().isInvalid());
    QVERIFY(LogParam::setDXCFilterLogStatus(DxccLogStatusCondition()));
    QVERIFY(!LogParam::getDXCFilterLogStatus().isFiltered());
    QCOMPARE(LogParam::getDXCFilterLogStatus().mask, static_cast<int>(DxccStatus::All));
}

void DxccLogStatusTest::failedWriteKeepsPreviousCondition()
{
    const DxccLogStatusCondition original(NewEntity);
    QVERIFY(LogParam::setDXCFilterLogStatus(original));
    QCOMPARE(LogParam::getDXCFilterLogStatus().toJson(), original.toJson());
    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral("PRAGMA query_only = ON")));
    QVERIFY(!LogParam::setDXCFilterLogStatus(DxccLogStatusCondition(NewSlot)));
    QCOMPARE(LogParam::getDXCFilterLogStatus().toJson(), original.toJson());
    QVERIFY(query.exec(QStringLiteral("PRAGMA query_only = OFF")));
}

QTEST_GUILESS_MAIN(DxccLogStatusTest)

#include "tst_dxcclogstatus.moc"
