#include <QtTest>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include "core/QSOFilterManager.h"
#include "core/QSOFilterDateRange.h"

class QSOFilterManagerTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanup();
    void legacyRules_data();
    void legacyRules();
    void savedValuesRemainUnchanged();
    void emptyFilterRetainsMatchingType();
    void periods_data();
    void periods();
    void periodOperators_data();
    void periodOperators();
    void periodStaysGroupedWithOr();
    void relativePeriodMovesWithUtcDate();
    void customTimesAndDateOnlyColumns();
    void invalidPeriodDoesNotRemoveFilter();
    void temporaryValuesDoNotChangeDefinition();
    void caseInsensitiveRules_data();
    void caseInsensitiveRules();

private:
    QString legacyWhere(const QString &name, const QString &prefix = {});
    QStringList matchingIds(const QString &where);
    QSOFilter periodFilter(const QString &from, const QString &to, int op = QSOFilterRule::InDateRange);
};

void QSOFilterManagerTest::initTestCase()
{
    auto db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    db.setConnectOptions("QSQLITE_ENABLE_REGEXP");
    QVERIFY(db.open());
    const QStringList schema = {
        "CREATE TABLE contacts (id INTEGER PRIMARY KEY, start_time TEXT, callsign TEXT, mode TEXT, qsl_date TEXT, amount REAL)",
        "CREATE TABLE qso_filters (filter_name TEXT PRIMARY KEY, matching_type INTEGER)",
        "CREATE TABLE qso_filter_rules (filter_name TEXT, table_field_index INTEGER, operator_id INTEGER, value TEXT)",
        "CREATE TABLE qso_filter_operators (operator_id INTEGER PRIMARY KEY, sql_operator TEXT)",
        "CREATE TABLE qso_filter_matching_types (matching_id INTEGER PRIMARY KEY, sql_operator TEXT)",
        "INSERT INTO qso_filter_matching_types VALUES (0,'AND'), (1,'OR')",
        "INSERT INTO qso_filter_operators VALUES (0,'='),(1,'<>'),(2,'like'),(3,'not like'),(4,'>'),(5,'<'),(6,'starts with'),(7,'regexp')",
        "INSERT INTO contacts VALUES (1,'2026-09-04T23:59:59Z','OK1ABC','CW','2026-09-04',1)",
        "INSERT INTO contacts VALUES (2,'2026-09-05T00:00:00Z','OK2ABC','SSB','2026-09-05',2)",
        "INSERT INTO contacts VALUES (3,'2026-09-05T12:00:00','O''NEIL','CW','2026-09-05',3)",
        "INSERT INTO contacts VALUES (4,'2026-09-05T23:59:59Z','',NULL,NULL,0)",
        "INSERT INTO contacts VALUES (5,'2026-09-06T00:00:00Z',NULL,'CW','2026-09-06',NULL)"
    };
    QSqlQuery query;
    for ( const auto &statement : schema )
        QVERIFY2(query.exec(statement), qPrintable(query.lastError().text()));

    // Apply the real additive migration on a database containing old filters.
    QVERIFY(query.exec("INSERT INTO qso_filters VALUES ('existing',1)"));
    QVERIFY(query.exec("INSERT INTO qso_filter_rules VALUES ('existing',2,0,'TODAY')"));
    QFile migration(QFINDTESTDATA("../../res/sql/migration_041.sql"));
    QVERIFY(migration.open(QIODevice::ReadOnly));
    for ( const auto &statement : QString::fromUtf8(migration.readAll()).split(';') )
        if ( !statement.trimmed().isEmpty() ) QVERIFY(query.exec(statement));
    QVERIFY(query.exec("SELECT f.matching_type, r.operator_id, r.value FROM qso_filters f JOIN qso_filter_rules r USING(filter_name)"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    QCOMPARE(query.value(1).toInt(), 0);
    QCOMPARE(query.value(2).toString(), QString("TODAY"));
    QSOFilterManager::instance();
}

void QSOFilterManagerTest::cleanup()
{
    QSqlQuery query;
    QVERIFY(query.exec("DELETE FROM qso_filter_rules"));
    QVERIFY(query.exec("DELETE FROM qso_filters"));
}

QString QSOFilterManagerTest::legacyWhere(const QString &name, const QString &prefix)
{
    // The pre-change SQL generator is the compatibility oracle. In particular,
    // do not normalize legacy timestamps, LIKE wildcards or NULL comparisons.
    QSqlQuery query;
    query.prepare(QString(
        "SELECT '(' || GROUP_CONCAT(' ' || '%1' || c.name || ' ' || "
        "CASE WHEN r.value IS NULL AND o.sql_operator IN ('=', 'like') THEN 'IS' "
        "WHEN r.value IS NULL and r.operator_id NOT IN ('=', 'like') THEN 'IS NOT' "
        "WHEN o.sql_operator = 'starts with' THEN 'like' ELSE o.sql_operator END || "
        "' (' || quote(CASE o.sql_operator WHEN 'like' THEN '%' || r.value || '%' "
        "WHEN 'not like' THEN '%' || r.value || '%' WHEN 'starts with' THEN r.value || '%' "
        "ELSE r.value END) || ') ', m.sql_operator) || ')' "
        "FROM qso_filters f, qso_filter_rules r, qso_filter_operators o, "
        "qso_filter_matching_types m, PRAGMA_TABLE_INFO('contacts') c "
        "WHERE f.filter_name=:name AND f.filter_name=r.filter_name AND o.operator_id=r.operator_id "
        "AND m.matching_id=f.matching_type AND c.cid=r.table_field_index")
        .arg(prefix.isEmpty() ? QString() : prefix + '.'));
    query.bindValue(":name", name);
    if ( !query.exec() || !query.next() ) return {};
    return QString("( %1 )").arg(query.value(0).toString());
}

QStringList QSOFilterManagerTest::matchingIds(const QString &where)
{
    QSqlQuery query;
    if ( !query.exec("SELECT id FROM contacts WHERE " + where + " ORDER BY id") )
    {
        QTest::qFail(qPrintable(query.lastError().text() + ": " + where), __FILE__, __LINE__);
        return {};
    }
    QStringList ids;
    while ( query.next() ) ids.append(query.value(0).toString());
    return ids;
}

void QSOFilterManagerTest::legacyRules_data()
{
    QTest::addColumn<int>("op");
    QTest::addColumn<int>("field");
    QTest::addColumn<QString>("value");
    QTest::addColumn<int>("matching");
    const QStringList values = {QString(), QStringLiteral(""), "OK", "O'NEIL", "%", "TODAY", "@date-range:today"};
    for ( int op = 0; op <= 7; ++op )
        for ( int i = 0; i < values.size(); ++i )
            for ( int matching = 0; matching <= 1; ++matching )
                QTest::newRow(qPrintable(QString("op%1-value%2-match%3").arg(op).arg(i).arg(matching)))
                    << op << 2 << values[i] << matching;
    QTest::newRow("legacy-time-equal") << 0 << 1 << QString("2026-09-05T00:00:00") << 0;
    QTest::newRow("legacy-time-greater") << 4 << 1 << QString("2026-09-05T00:00:00") << 0;
    QTest::newRow("legacy-number") << 4 << 5 << QString("1.5") << 0;
}

void QSOFilterManagerTest::legacyRules()
{
    QFETCH(int, op);
    QFETCH(int, field);
    QFETCH(QString, value);
    QFETCH(int, matching);
    QSOFilter filter;
    filter.filterName = "legacy";
    filter.machingType = matching;
    filter.addRule(QSOFilterRule(field, op, value));
    filter.addRule(QSOFilterRule(3, 0, "CW"));
    QVERIFY(QSOFilterManager::instance()->save(filter));
    const QString expected = legacyWhere(filter.filterName, "contacts");
    const QString actual = QSOFilterManager::getWhereClause(filter.filterName, "contacts");
    // Case sensitivity is intentionally changed; everything else in the old
    // SQL representation (including timestamp precision and NULLs) is retained.
    QString withoutCaseOptions = actual;
    withoutCaseOptions.replace(" COLLATE NOCASE ", " ");
    withoutCaseOptions.replace("('(?i)", "('");
    QCOMPARE(withoutCaseOptions, expected);
    QCOMPARE(matchingIds(actual), matchingIds(expected));
}

void QSOFilterManagerTest::savedValuesRemainUnchanged()
{
    QSOFilter filter;
    filter.filterName = "values";
    filter.addRule(QSOFilterRule(2, 0, QString()));
    filter.addRule(QSOFilterRule(2, 0, QStringLiteral("")));
    filter.addRule(QSOFilterRule(1, 0, "2026-09-05T12:00:00+00:00"));
    QVERIFY(QSOFilterManager::instance()->save(filter));
    const auto loaded = QSOFilterManager::instance()->getFilter(filter.filterName);
    QCOMPARE(loaded.rules, filter.rules);
    QVERIFY(QSOFilterManager::instance()->save(loaded));
    QCOMPARE(QSOFilterManager::instance()->getFilter(filter.filterName).rules, filter.rules);
}

void QSOFilterManagerTest::emptyFilterRetainsMatchingType()
{
    QSOFilter filter;
    filter.filterName = "empty";
    filter.machingType = 1;
    QVERIFY(QSOFilterManager::instance()->save(filter));
    QCOMPARE(QSOFilterManager::instance()->getFilter("empty").machingType, 1);
    QCOMPARE(QSOFilterManager::getWhereClause("empty"), legacyWhere("empty"));
}

QSOFilter QSOFilterManagerTest::periodFilter(const QString &from, const QString &to, int op)
{
    QSOFilterDateRange range;
    range.from = from;
    range.to = to;
    QSOFilter filter;
    filter.filterName = "period";
    filter.addRule(QSOFilterRule(1, op, range.toString()));
    return filter;
}

void QSOFilterManagerTest::periods_data()
{
    QTest::addColumn<QString>("from");
    QTest::addColumn<QString>("to");
    QTest::addColumn<QDate>("today");
    QTest::addColumn<QDate>("start");
    QTest::addColumn<QDate>("end");
    const QDate today(2026, 9, 5);
    QTest::newRow("today") << QString("TODAY") << QString("TODAY") << today << today << today.addDays(1);
    QTest::newRow("today-case") << QString("today") << QString("ToDaY") << today << today << today.addDays(1);
    QTest::newRow("offset-case") << QString("Today-45") << QString("today-15") << today << today.addDays(-45) << today.addDays(-14);
    QTest::newRow("week-case") << QString("week_start") << QString("today") << today << QDate(2026,8,31) << today.addDays(1);
    QTest::newRow("month-case") << QString("month_START") << QString("today") << today << QDate(2026,9,1) << today.addDays(1);
    QTest::newRow("year-case") << QString("year_start") << QString("Year_End") << today << QDate(2026,1,1) << QDate(2027,1,1);
    QTest::newRow("yesterday") << QString("TODAY-1") << QString("TODAY-1") << today << today.addDays(-1) << today;
    QTest::newRow("week") << QString("WEEK_START") << QString("TODAY") << today << QDate(2026,8,31) << today.addDays(1);
    QTest::newRow("month") << QString("MONTH_START") << QString("TODAY") << today << QDate(2026,9,1) << today.addDays(1);
    QTest::newRow("last7") << QString("TODAY-6") << QString("TODAY") << today << today.addDays(-6) << today.addDays(1);
    QTest::newRow("last30") << QString("TODAY-29") << QString("TODAY") << today << today.addDays(-29) << today.addDays(1);
    QTest::newRow("year") << QString("YEAR_START") << QString("YEAR_END") << today << QDate(2026,1,1) << QDate(2027,1,1);
    QTest::newRow("custom") << QString("TODAY-45") << QString("TODAY-15") << today << today.addDays(-45) << today.addDays(-14);
    QTest::newRow("leap-day") << QString("TODAY-1") << QString("TODAY") << QDate(2024,3,1) << QDate(2024,2,29) << QDate(2024,3,2);
    QTest::newRow("mixed") << QString("2025-12-31") << QString("TODAY") << today << QDate(2025,12,31) << today.addDays(1);
}

void QSOFilterManagerTest::periods()
{
    QFETCH(QString, from); QFETCH(QString, to); QFETCH(QDate, today);
    QFETCH(QDate, start); QFETCH(QDate, end);
    QSOFilterDateRange range;
    range.from = from; range.to = to;
    QDateTime actualStart, actualEnd;
    QVERIFY(range.resolve(today, actualStart, actualEnd));
    QCOMPARE(actualStart.date(), start);
    QCOMPARE(actualEnd.date(), end);
    QCOMPARE(actualStart.time(), QTime(0,0));
    QCOMPARE(actualEnd.time(), QTime(0,0));
    const auto filter = periodFilter(from, to);
    QVERIFY(QSOFilterManager::instance()->save(filter));
    QCOMPARE(QSOFilterManager::instance()->getFilter(filter.filterName).rules, filter.rules);
}

void QSOFilterManagerTest::periodOperators_data()
{
    QTest::addColumn<int>("op"); QTest::addColumn<QStringList>("ids");
    QTest::newRow("in") << 8 << (QStringList{"2","3","4"});
    QTest::newRow("outside") << 9 << (QStringList{"1","5"});
    QTest::newRow("before") << 10 << (QStringList{"1"});
    QTest::newRow("after") << 11 << (QStringList{"5"});
}

void QSOFilterManagerTest::periodOperators()
{
    QFETCH(int, op); QFETCH(QStringList, ids);
    QCOMPARE(matchingIds(QSOFilterManager::getWhereClause(periodFilter("TODAY","TODAY",op), {}, QDate(2026,9,5))), ids);
}

void QSOFilterManagerTest::periodStaysGroupedWithOr()
{
    auto filter = periodFilter("TODAY", "TODAY");
    filter.machingType = 1;
    filter.addRule(QSOFilterRule(2,0,"OK1ABC"));
    QCOMPARE(matchingIds(QSOFilterManager::getWhereClause(filter, {}, QDate(2026,9,5))), (QStringList{"1","2","3","4"}));
}

void QSOFilterManagerTest::relativePeriodMovesWithUtcDate()
{
    const auto filter = periodFilter("today", "ToDaY");
    QSOFilterDateRange range;
    QVERIFY(QSOFilterDateRange::fromString(filter.rules.first().value, range));
    QVERIFY(range.isRelative());
    QCOMPARE(matchingIds(QSOFilterManager::getWhereClause(filter, {}, QDate(2026,9,5))), (QStringList{"2","3","4"}));
    QCOMPARE(matchingIds(QSOFilterManager::getWhereClause(filter, {}, QDate(2026,9,6))), (QStringList{"5"}));
}

void QSOFilterManagerTest::customTimesAndDateOnlyColumns()
{
    auto filter = periodFilter("2026-09-05T12:00:00", "2026-09-05T12:00:00");
    QCOMPARE(matchingIds(QSOFilterManager::getWhereClause(filter)), (QStringList{"3"}));
    filter = periodFilter("2026-09-05", "2026-09-05");
    filter.rules[0].tableFieldIndex = 4;
    QCOMPARE(matchingIds(QSOFilterManager::getWhereClause(filter)), (QStringList{"2","3"}));
}

void QSOFilterManagerTest::invalidPeriodDoesNotRemoveFilter()
{
    auto filter = periodFilter("TODAY-unknown", "TODAY");
    filter.machingType = 1;
    filter.addRule(QSOFilterRule(3,0,"CW"));
    QVERIFY(matchingIds(QSOFilterManager::getWhereClause(filter)).isEmpty());
    filter = periodFilter("TODAY+1", "TODAY");
    QVERIFY(matchingIds(QSOFilterManager::getWhereClause(filter)).isEmpty());
}

void QSOFilterManagerTest::temporaryValuesDoNotChangeDefinition()
{
    auto filter = periodFilter("TODAY", "TODAY");
    QVERIFY(QSOFilterManager::instance()->save(filter));
    auto temporary = QSOFilterManager::instance()->getFilter(filter.filterName);
    temporary.rules = periodFilter("TODAY-1", "TODAY-1").rules;
    QCOMPARE(matchingIds(QSOFilterManager::getWhereClause(temporary, {}, QDate(2026,9,5))), (QStringList{"1"}));
    QCOMPARE(QSOFilterManager::instance()->getFilter(filter.filterName).rules, filter.rules);
}

void QSOFilterManagerTest::caseInsensitiveRules_data()
{
    QTest::addColumn<int>("op");
    QTest::addColumn<QString>("value");
    QTest::addColumn<QStringList>("ids");
    QTest::newRow("equal") << 0 << QString("ok1abc") << (QStringList{"1"});
    QTest::newRow("not-equal") << 1 << QString("oK1AbC") << (QStringList{"2","3","4"});
    QTest::newRow("contains") << 2 << QString("aBc") << (QStringList{"1","2"});
    QTest::newRow("not-contains") << 3 << QString("aBc") << (QStringList{"3","4"});
    QTest::newRow("greater") << 4 << QString("ok1abc") << (QStringList{"2"});
    QTest::newRow("less") << 5 << QString("ok1abc") << (QStringList{"3","4"});
    QTest::newRow("starts-with") << 6 << QString("oK") << (QStringList{"1","2"});
    QTest::newRow("regexp") << 7 << QString("^ok[12]abc$") << (QStringList{"1","2"});
    QTest::newRow("regexp-escape") << 7 << QString("^ok\\dabc$") << (QStringList{"1","2"});
    QTest::newRow("regexp-uppercase-escape") << 7 << QString("^\\D+$") << (QStringList{"3"});
    QTest::newRow("quote") << 0 << QString("o'neil") << (QStringList{"3"});
    QTest::newRow("equal-null") << 0 << QString() << (QStringList{"5"});
    QTest::newRow("not-equal-null") << 1 << QString() << (QStringList{"1","2","3","4"});
    QTest::newRow("regexp-null") << 7 << QString() << (QStringList{"1","2","3","4"});
    QTest::newRow("empty-text") << 0 << QStringLiteral("") << (QStringList{"4"});
    QTest::newRow("wildcards") << 6 << QString("ok_") << (QStringList{"1","2"});
    QTest::newRow("injection") << 0 << QString("ok1abc' OR 1=1 --") << QStringList{};
}

void QSOFilterManagerTest::caseInsensitiveRules()
{
    QFETCH(int, op);
    QFETCH(QString, value);
    QFETCH(QStringList, ids);
    QSOFilter filter;
    filter.filterName = "case";
    filter.addRule(QSOFilterRule(2, op, value));
    QVERIFY(QSOFilterManager::instance()->save(filter));
    for ( const auto &prefix : {QString(), QString("contacts")} )
    {
        // Saved filters and temporary parameters must use identical semantics.
        QCOMPARE(matchingIds(QSOFilterManager::getWhereClause(filter.filterName, prefix)), ids);
        QCOMPARE(matchingIds(QSOFilterManager::getWhereClause(filter, prefix)), ids);
    }
    QCOMPARE(QSOFilterManager::instance()->getFilter(filter.filterName).rules, filter.rules);

    // Both the stored field and the parameter may contain mixed case.
    QSqlQuery query;
    QVERIFY(query.exec("UPDATE contacts SET callsign = 'oK1aBc' WHERE id = 1"));
    QCOMPARE(matchingIds(QSOFilterManager::getWhereClause(filter)), ids);
    QVERIFY(query.exec("UPDATE contacts SET callsign = 'OK1ABC' WHERE id = 1"));
}

QTEST_GUILESS_MAIN(QSOFilterManagerTest)
#include "tst_qsofiltermanager.moc"
