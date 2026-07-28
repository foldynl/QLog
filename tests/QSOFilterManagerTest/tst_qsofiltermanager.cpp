#include <QtTest>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "core/QSOFilterManager.h"

namespace {
QString lastErrorString(const QSqlQuery &query)
{
    return query.lastError().isValid() ? query.lastError().text() : QString();
}
}

class QSOFilterManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void todayPresetUsesCurrentUtcDay();
    void presetRanges_data();
    void presetRanges();
    void presetOperators_data();
    void presetOperators();
    void manualDateConditionIsUnchanged();
};

void QSOFilterManagerTest::initTestCase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(db.open());

    QSqlQuery query;
    const QStringList schema = {
        QStringLiteral("CREATE TABLE contacts (id INTEGER PRIMARY KEY, start_time TEXT)"),
        QStringLiteral("CREATE TABLE qso_filter_matching_types (matching_id INTEGER PRIMARY KEY, sql_operator TEXT NOT NULL)"),
        QStringLiteral("INSERT INTO qso_filter_matching_types VALUES (0, 'AND'), (1, 'OR')"),
        QStringLiteral("CREATE TABLE qso_filter_operators (operator_id INTEGER PRIMARY KEY, sql_operator TEXT NOT NULL)"),
        QStringLiteral("INSERT INTO qso_filter_operators VALUES (0, '='), (1, '<>'), (2, 'like'), (3, 'not like'), (4, '>'), (5, '<'), (6, 'starts with'), (7, 'regexp')"),
        QStringLiteral("CREATE TABLE qso_filters (filter_name TEXT PRIMARY KEY, matching_type INTEGER)"),
        QStringLiteral("CREATE TABLE qso_filter_rules (filter_name TEXT, table_field_index INTEGER NOT NULL, operator_id INTEGER, value TEXT)")
    };

    for ( const QString &statement : schema )
        QVERIFY2(query.exec(statement), qPrintable(lastErrorString(query)));

    QSOFilterManager::instance();
}

void QSOFilterManagerTest::cleanup()
{
    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral("DELETE FROM qso_filter_rules")));
    QVERIFY(query.exec(QStringLiteral("DELETE FROM qso_filters")));
    QVERIFY(query.exec(QStringLiteral("DELETE FROM contacts")));
}

void QSOFilterManagerTest::todayPresetUsesCurrentUtcDay()
{
    const QDate today = QDateTime::currentDateTimeUtc().date();

    QSOFilter filter;
    filter.filterName = QStringLiteral("Today");
    filter.addRule(QSOFilterRule(LogbookModel::COLUMN_TIME_ON, 0,
                                 QStringLiteral("@date-range:today")));
    QVERIFY(QSOFilterManager::instance()->save(filter));

    QSqlQuery insert;
    QVERIFY(insert.prepare(QStringLiteral("INSERT INTO contacts (start_time) VALUES (?)")));
    const QStringList dates = {
        today.addDays(-1).toString(Qt::ISODate) + QStringLiteral("T23:59:59Z"),
        today.toString(Qt::ISODate) + QStringLiteral("T00:00:00Z"),
        today.toString(Qt::ISODate) + QStringLiteral("T23:59:59Z"),
        today.addDays(1).toString(Qt::ISODate) + QStringLiteral("T00:00:00Z")
    };
    for ( const QString &date : dates )
    {
        insert.bindValue(0, date);
        QVERIFY2(insert.exec(), qPrintable(lastErrorString(insert)));
    }

    const QString whereClause = QSOFilterManager::getWhereClause(filter.filterName);
    QVERIFY2(whereClause.contains(today.toString(Qt::ISODate)), qPrintable(whereClause));

    QSqlQuery result;
    QVERIFY2(result.exec(QStringLiteral("SELECT COUNT(*) FROM contacts WHERE ") + whereClause),
             qPrintable(lastErrorString(result) + QStringLiteral("; clause: ") + whereClause));
    QVERIFY(result.next());
    QCOMPARE(result.value(0).toInt(), 2);
}

void QSOFilterManagerTest::presetRanges_data()
{
    QTest::addColumn<QString>("token");
    QTest::addColumn<QDate>("rangeStart");
    QTest::addColumn<QDate>("rangeEnd");

    const QDate today = QDateTime::currentDateTimeUtc().date();
    QTest::newRow("today") << QStringLiteral("@date-range:today")
                            << today << today.addDays(1);
    QTest::newRow("yesterday") << QStringLiteral("@date-range:yesterday")
                                << today.addDays(-1) << today;
    QTest::newRow("week-to-date") << QStringLiteral("@date-range:week-to-date")
                                  << today.addDays(1 - today.dayOfWeek()) << today.addDays(1);
    QTest::newRow("month-to-date") << QStringLiteral("@date-range:month-to-date")
                                   << QDate(today.year(), today.month(), 1) << today.addDays(1);
    QTest::newRow("last-7-days") << QStringLiteral("@date-range:last-7-days")
                                 << today.addDays(-6) << today.addDays(1);
    QTest::newRow("last-30-days") << QStringLiteral("@date-range:last-30-days")
                                  << today.addDays(-29) << today.addDays(1);
    QTest::newRow("this-year") << QStringLiteral("@date-range:this-year")
                               << QDate(today.year(), 1, 1) << QDate(today.year() + 1, 1, 1);
}

void QSOFilterManagerTest::presetRanges()
{
    QFETCH(QString, token);
    QFETCH(QDate, rangeStart);
    QFETCH(QDate, rangeEnd);

    QSOFilter filter;
    filter.filterName = QStringLiteral("Preset");
    filter.addRule(QSOFilterRule(LogbookModel::COLUMN_TIME_ON, 0, token));
    QVERIFY(QSOFilterManager::instance()->save(filter));

    const QString whereClause = QSOFilterManager::getWhereClause(filter.filterName);
    const QString expectedRange = QStringLiteral("start_time >= '%1' AND start_time < '%2'")
            .arg(rangeStart.toString(Qt::ISODate), rangeEnd.toString(Qt::ISODate));
    QVERIFY2(whereClause.contains(expectedRange), qPrintable(whereClause));
}

void QSOFilterManagerTest::presetOperators_data()
{
    QTest::addColumn<int>("operatorId");
    QTest::addColumn<int>("expectedCount");

    QTest::newRow("equal") << 0 << 2;
    QTest::newRow("not-equal") << 1 << 2;
    QTest::newRow("contains") << 2 << 2;
    QTest::newRow("not-contains") << 3 << 2;
    QTest::newRow("greater-than") << 4 << 1;
    QTest::newRow("less-than") << 5 << 1;
    QTest::newRow("starts-with") << 6 << 2;
    QTest::newRow("regexp") << 7 << 2;
}

void QSOFilterManagerTest::presetOperators()
{
    QFETCH(int, operatorId);
    QFETCH(int, expectedCount);

    const QDate today = QDateTime::currentDateTimeUtc().date();
    QSOFilter filter;
    filter.filterName = QStringLiteral("Operator");
    filter.addRule(QSOFilterRule(LogbookModel::COLUMN_TIME_ON, operatorId,
                                 QStringLiteral("@date-range:today")));
    QVERIFY(QSOFilterManager::instance()->save(filter));

    QSqlQuery insert;
    QVERIFY(insert.prepare(QStringLiteral("INSERT INTO contacts (start_time) VALUES (?)")));
    const QStringList dates = {
        today.addDays(-1).toString(Qt::ISODate) + QStringLiteral("T12:00:00Z"),
        today.toString(Qt::ISODate) + QStringLiteral("T00:00:00Z"),
        today.toString(Qt::ISODate) + QStringLiteral("T23:59:59Z"),
        today.addDays(1).toString(Qt::ISODate) + QStringLiteral("T00:00:00Z")
    };
    for ( const QString &date : dates )
    {
        insert.bindValue(0, date);
        QVERIFY2(insert.exec(), qPrintable(lastErrorString(insert)));
    }

    const QString whereClause = QSOFilterManager::getWhereClause(filter.filterName);
    QSqlQuery result;
    QVERIFY2(result.exec(QStringLiteral("SELECT COUNT(*) FROM contacts WHERE ") + whereClause),
             qPrintable(lastErrorString(result) + QStringLiteral("; clause: ") + whereClause));
    QVERIFY(result.next());
    QCOMPARE(result.value(0).toInt(), expectedCount);
}

void QSOFilterManagerTest::manualDateConditionIsUnchanged()
{
    QSOFilter filter;
    filter.filterName = QStringLiteral("Manual");
    filter.addRule(QSOFilterRule(LogbookModel::COLUMN_TIME_ON, 4,
                                 QStringLiteral("2026-01-02T03:04:05")));
    QVERIFY(QSOFilterManager::instance()->save(filter));

    const QString whereClause = QSOFilterManager::getWhereClause(filter.filterName);
    QVERIFY2(whereClause.contains(QStringLiteral("start_time > ('2026-01-02T03:04:05')")),
             qPrintable(whereClause));
}

QTEST_GUILESS_MAIN(QSOFilterManagerTest)

#include "tst_qsofiltermanager.moc"
