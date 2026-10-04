#include <QtTest>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>

#define private public
#include "awards/AwardPGAHunter.h"
#undef private

class PGAHunterTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void eligibility_data();
    void eligibility();
    void totalsAndNavigation();
    void emptyLog();
    void queryFailureClearsResults();
};

void PGAHunterTest::initTestCase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    QVERIFY(db.open());
    QSqlQuery query;
    QVERIFY(query.exec("CREATE TABLE contacts(id INTEGER PRIMARY KEY, callsign TEXT, "
                       "start_time TEXT, band TEXT, prop_mode TEXT, mode TEXT, dxcc INTEGER, "
                       "cnty TEXT, my_dxcc INTEGER, my_cnty TEXT, station_callsign TEXT, "
                       "qsl_rcvd TEXT, lotw_qsl_rcvd TEXT, eqsl_qsl_rcvd TEXT)"));
    QVERIFY(query.exec("CREATE TABLE modes(name TEXT, dxcc TEXT)"));
    QVERIFY(query.exec("INSERT INTO modes VALUES ('CW','CW'),('SSB','PHONE')"));
    QVERIFY(query.exec("CREATE TABLE adif_enum_secondary_subdivision "
                       "(dxcc INTEGER, code TEXT, subdivision_name TEXT, primary_code TEXT, aux TEXT)"));
    QVERIFY(query.exec("INSERT INTO adif_enum_secondary_subdivision VALUES "
                       "(269,'KQ04','Same name','D',''),(269,'WA01','Same name','M',''),"
                       "(269,'BA16','Grabówka','O',''),(269,'LI13','Szczawa','M',''),"
                       "(269,'AB04','Wałbrzych','D','suspended'),"
                       "(269,'WB01','Wałbrzych','D',''),(269,'DP04','Ostrowice','Z','suspended'),"
                       "(269,'ZG10','Zielona Góra','B','suspended'),(291,'KQ04','Other country','','')"));
}

void PGAHunterTest::cleanupTestCase()
{
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

void PGAHunterTest::eligibility_data()
{
    QTest::addColumn<QString>("reference");
    QTest::addColumn<QString>("date");
    QTest::addColumn<QString>("band");
    QTest::addColumn<QString>("propagation");
    QTest::addColumn<bool>("eligible");
    QTest::newRow("ordinary") << QStringLiteral("KQ04") << QStringLiteral("2025-01-01") << QStringLiteral("40m") << QStringLiteral("") << true;
    QTest::newRow("before-program") << QStringLiteral("KQ04") << QStringLiteral("2007-12-31T23:59:59") << QStringLiteral("40m") << QStringLiteral("") << false;
    QTest::newRow("program-start") << QStringLiteral("KQ04") << QStringLiteral("2008-01-01") << QStringLiteral("160m") << QStringLiteral("") << true;
    QTest::newRow("vhf") << QStringLiteral("KQ04") << QStringLiteral("2025-01-01") << QStringLiteral("2m") << QStringLiteral("") << false;
    QTest::newRow("repeater") << QStringLiteral("KQ04") << QStringLiteral("2025-01-01") << QStringLiteral("40m") << QStringLiteral("rpt") << false;
    QTest::newRow("grabowka-before") << QStringLiteral("BA16") << QStringLiteral("2024-12-31T23:59:59") << QStringLiteral("40m") << QStringLiteral("") << false;
    QTest::newRow("grabowka-start") << QStringLiteral("BA16") << QStringLiteral("2025-01-01") << QStringLiteral("40m") << QStringLiteral("") << true;
    QTest::newRow("szczawa-before") << QStringLiteral("LI13") << QStringLiteral("2024-12-31") << QStringLiteral("40m") << QStringLiteral("") << false;
    QTest::newRow("szczawa-start") << QStringLiteral("LI13") << QStringLiteral("2025-01-01") << QStringLiteral("40m") << QStringLiteral("") << true;
    QTest::newRow("walbrzych-old") << QStringLiteral("AB04") << QStringLiteral("2012-12-31T23:59:59") << QStringLiteral("40m") << QStringLiteral("") << true;
    QTest::newRow("walbrzych-old-end") << QStringLiteral("AB04") << QStringLiteral("2013-01-01") << QStringLiteral("40m") << QStringLiteral("") << false;
    QTest::newRow("walbrzych-new-before") << QStringLiteral("WB01") << QStringLiteral("2012-12-31") << QStringLiteral("40m") << QStringLiteral("") << false;
    QTest::newRow("walbrzych-new-start") << QStringLiteral("WB01") << QStringLiteral("2013-01-01") << QStringLiteral("40m") << QStringLiteral("") << true;
    QTest::newRow("ostrowice-last-day") << QStringLiteral("DP04") << QStringLiteral("2018-12-31T23:59:59") << QStringLiteral("40m") << QStringLiteral("") << true;
    QTest::newRow("ostrowice-end") << QStringLiteral("DP04") << QStringLiteral("2019-01-01") << QStringLiteral("40m") << QStringLiteral("") << false;
    QTest::newRow("zielona-gora-old") << QStringLiteral("ZG10") << QStringLiteral("2014-12-31") << QStringLiteral("40m") << QStringLiteral("") << true;
    QTest::newRow("zielona-gora-end") << QStringLiteral("ZG10") << QStringLiteral("2015-01-01") << QStringLiteral("40m") << QStringLiteral("") << false;
}

void PGAHunterTest::eligibility()
{
    QFETCH(QString, reference);
    QFETCH(QString, date);
    QFETCH(QString, band);
    QFETCH(QString, propagation);
    QFETCH(bool, eligible);
    QSqlQuery query;
    QVERIFY(query.exec("DELETE FROM contacts"));
    QVERIFY(query.prepare("INSERT INTO contacts(start_time, band, prop_mode, cnty) "
                          "VALUES (:date,:band,:propagation,:reference)"));
    query.bindValue(":date", date);
    query.bindValue(":band", band);
    query.bindValue(":propagation", propagation);
    query.bindValue(":reference", reference);
    QVERIFY(query.exec());
    QVERIFY2(query.exec("SELECT COUNT(*) FROM contacts WHERE " +
                       AwardPGAHunter::eligibleContactCondition("UPPER(TRIM(cnty))")),
             qPrintable(query.lastError().text()));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), eligible ? 1 : 0);
}

void PGAHunterTest::totalsAndNavigation()
{
    QSqlQuery query;
    QVERIFY(query.exec("DELETE FROM contacts"));
    QVERIFY(query.exec("INSERT INTO contacts VALUES "
                       "(1,'SP1A','2025-01-01','40m','','CW',269,' kq04 ',269,' wa01 ','OK1A','Y','Y','Y'),"
                       "(2,'SP1A','2025-01-02','20m','','CW',269,'KQ04',269,'WA01','OK1A','Y','Y','Y'),"
                       "(3,'SP2A','2025-01-02','40m','','CW',269,'WA01',269,'WA01','OK1A','Y','Y','Y'),"
                       "(4,'SP2A','2025-01-02','2m','','CW',269,'KQ04',269,'WA01','OK1A','Y','Y','Y'),"
                       "(5,'SP2A','2025-01-02','40m','RPT','CW',269,'KQ04',269,'WA01','OK1A','Y','Y','Y'),"
                       "(6,'W1A','2025-01-02','40m','','CW',291,'KQ04',291,'WA01','OK1A','Y','Y','Y'),"
                       "(7,'SP3A','2025-01-02','40m','','SSB',269,'BA16',269,'LI13','OK1A','Y','Y','Y'),"
                       "(8,'SP3A','2025-01-02','40m','','CW',269,'BA16',269,'LI13','OK2A','Y','Y','Y'),"
                       "(9,'SP3A','2025-01-02','40m','','CW',269,'BA16,LI13',291,'','OK1A','Y','Y','Y'),"
                       "(10,'SP3A','2025-01-02','40m','','CW',269,'UNKNOWN',291,'','OK1A','Y','Y','Y'),"
                       "(11,'SP3A','2024-12-31','40m','','CW',269,'UNKNOWN',269,'LI13','OK1A','Y','Y','Y')"));
    AwardPGAHunter award;
    QSqlQueryModel model;
    award.model = &model;
    AwardFilterParams params;
    params.modes << QStringLiteral("'CW'");
    params.userFilterWhereClause = "AND station_callsign = 'OK1A'";
    params.confirmedConditions << QStringLiteral("qsl_rcvd = 'Y'") << QStringLiteral("lotw_qsl_rcvd = 'Y'") << QStringLiteral("eqsl_qsl_rcvd = 'Y'");
    params.notWorkedOnly = false;
    params.notConfirmedOnly = true;
    award.updateData(params);
    QVERIFY2(!model.lastError().isValid(), qPrintable(model.lastError().text()));
    QCOMPARE(model.rowCount(), 10);
    // Identical names remain distinct credits; own operation overlaps only once.
    QCOMPARE(model.data(model.index(0, 3)).toInt(), 2);
    QCOMPARE(model.data(model.index(0, 4)).toInt(), 1);
    QCOMPARE(model.data(model.index(1, 3)).toInt(), 2);
    QVERIFY(!award.getConditionSelected(model.index(0, 1)).valid);
    for ( int row = 2; row < model.rowCount(); ++row )
    {
        const QString code = model.data(model.index(row, 1)).toString();
        if ( code != "KQ04" && code != "WA01" )
            continue;
        const auto filter = award.getConditionSelected(model.index(row, 3));
        QVERIFY(filter.valid);
        QVERIFY2(query.exec("SELECT id FROM contacts WHERE " + filter.filter + " ORDER BY id"),
                 qPrintable(query.lastError().text()));
        QList<int> ids;
        while ( query.next() ) ids << query.value(0).toInt();
        QCOMPARE(ids, code == "KQ04" ? QList<int>({1, 2}) : QList<int>({3}));
        if ( code == "WA01" )
        {
            const auto ownFilter = award.getConditionSelected(model.index(row, 4));
            QVERIFY(query.exec("SELECT id FROM contacts WHERE " + ownFilter.filter + " ORDER BY id"));
            ids.clear();
            while ( query.next() ) ids << query.value(0).toInt();
            QCOMPARE(ids, QList<int>({1, 2, 3}));
        }
    }
    params.notWorkedOnly = true;
    award.updateData(params);
    QCOMPARE(model.rowCount(), 8);
    // Filtering the detail must not change the summary totals.
    QCOMPARE(model.data(model.index(1, 3)).toInt(), 2);
    for ( int row = 2; row < model.rowCount(); ++row )
    {
        const QString code = model.data(model.index(row, 1)).toString();
        QVERIFY(code != "KQ04" && code != "WA01");
    }
}

void PGAHunterTest::emptyLog()
{
    QSqlQuery query;
    QVERIFY(query.exec("DELETE FROM contacts"));
    AwardPGAHunter award;
    QSqlQueryModel model;
    award.model = &model;
    AwardFilterParams params;
    params.modes << "'CW'";
    params.notWorkedOnly = false;
    params.notConfirmedOnly = false;
    award.updateData(params);
    QVERIFY2(!model.lastError().isValid(), qPrintable(model.lastError().text()));
    QCOMPARE(model.rowCount(), 10);
    QCOMPARE(model.data(model.index(0, 3)).toInt(), 0);
    QCOMPARE(model.data(model.index(0, 4)).toInt(), 0);
    QCOMPARE(model.data(model.index(1, 3)).toInt(), 0);
}

void PGAHunterTest::queryFailureClearsResults()
{
    AwardPGAHunter award;
    QSqlQueryModel model;
    award.model = &model;
    AwardFilterParams params;
    params.modes << "'CW'";
    params.notWorkedOnly = false;
    params.notConfirmedOnly = false;
    award.updateData(params);
    QVERIFY(model.rowCount() > 0);
    QSqlQuery query;
    QVERIFY(query.exec("ALTER TABLE modes RENAME TO missing_modes"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*no such table: modes.*"));
    award.updateData(params);
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(!award.getConditionSelected(model.index(0, 1)).valid);
    QVERIFY(query.exec("ALTER TABLE missing_modes RENAME TO modes"));
}

QTEST_GUILESS_MAIN(PGAHunterTest)
#include "tst_pgahunter.moc"
