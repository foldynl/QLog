#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include <cmath>

#include "data/BandPlan.h"

namespace {
QString lastErrorString(const QSqlQuery &query)
{
    return query.lastError().isValid() ? query.lastError().text() : QString();
}
}

class BandPlanTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void freq2BandMode_data();
    void freq2BandMode();
    void freq2BandModeGroupString_data();
    void freq2BandModeGroupString();
    void freq2ExpectedMode_data();
    void freq2ExpectedMode();
    void freq2Band_data();
    void freq2Band();
    void resolveBand();
    void bandUsesWholeHzBoundaries();
    void bandsList_onlyDXCC();
    void modeToDXCCModeGroup_data();
    void modeToDXCCModeGroup();
    void isFTxMode_data();
    void isFTxMode();
    void isFTxBandMode_data();
    void isFTxBandMode();
};

void BandPlanTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));

    qRegisterMetaType<BandPlan::BandPlanMode>("BandPlan::BandPlanMode");

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    QVERIFY(db.open());

    QSqlQuery createBands;
    QVERIFY2(createBands.exec("CREATE TABLE bands ("
                              "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                              "name TEXT UNIQUE NOT NULL,"
                              "start_freq FLOAT,"
                              "end_freq FLOAT,"
                              "enabled BOOLEAN,"
                              "sat_designator TEXT)"),
             qPrintable(lastErrorString(createBands)));

    struct BandRow {
        const char *name;
        double start;
        double end;
        int enabled;
    };

    const BandRow bandRows[] = {
        {"2190m", 0.134, 0.140, 1},
        {"630m", 0.470, 0.490, 1},
        {"160m", 1.800, 2.000, 1},
        {"80m", 3.500, 4.000, 1},
        {"60m", 5.350, 5.450, 1},
        {"40m", 7.000, 7.300, 1},
        {"30m", 10.100, 10.150, 1},
        {"20m", std::nextafter(14.000, 0.0), std::nextafter(14.350, 15.0), 1},
        {"17m", 18.068, 18.168, 1},
        {"15m", 21.000, 21.450, 1},
        {"12m", 24.890, 24.990, 1},
        {"10m", 28.000, 29.700, 1},
        {"6m", 50.000, 54.000, 1},
        {"4m", 70.000, 71.000, 1},
        {"2m", 144.000, 148.000, 1},
        {"1.25m", 222.000, 225.000, 1},
        {"70cm", 420.000, 450.000, 1},
        {"33cm", 902.000, 928.000, 1},
        {"23cm", 1240.000, 1300.000, 1},
        {"13cm", 2300.000, 2450.000, 1},
        {"3cm", 10000.000, 11000.000, 1}
    };

    QSqlQuery insertBand;
    QVERIFY2(insertBand.prepare("INSERT INTO bands "
                                "(name, start_freq, end_freq, enabled, sat_designator) "
                                "VALUES (?, ?, ?, ?, ?)"),
             qPrintable(lastErrorString(insertBand)));
    for (const BandRow &row : bandRows)
    {
        insertBand.bindValue(0, QString::fromLatin1(row.name));
        insertBand.bindValue(1, row.start);
        insertBand.bindValue(2, row.end);
        insertBand.bindValue(3, row.enabled);
        insertBand.bindValue(4, QString());
        QVERIFY2(insertBand.exec(), qPrintable(lastErrorString(insertBand)));
    }

    QSqlQuery createModes;
    QVERIFY2(createModes.exec("CREATE TABLE modes ("
                              "name TEXT PRIMARY KEY,"
                              "dxcc TEXT NOT NULL)"),
             qPrintable(lastErrorString(createModes)));

    struct ModeRow { const char *name; const char *dxcc; };
    const ModeRow modeRows[] = {
        {"CW", "CW"},
        {"SSB", "PHONE"},
        {"FT8", "DIGITAL"},
        {"FT4", "DIGITAL"},
        {"FT2", "DIGITAL"},
        {"RTTY", "DIGITAL"}
    };

    QSqlQuery insertMode;
    QVERIFY2(insertMode.prepare("INSERT INTO modes (name, dxcc) VALUES (?, ?)"),
             qPrintable(lastErrorString(insertMode)));
    for (const ModeRow &row : modeRows)
    {
        insertMode.bindValue(0, QString::fromLatin1(row.name));
        insertMode.bindValue(1, QString::fromLatin1(row.dxcc));
        QVERIFY2(insertMode.exec(), qPrintable(lastErrorString(insertMode)));
    }
}

void BandPlanTest::cleanupTestCase()
{
    const QString connectionName = QString::fromLatin1(QSqlDatabase::defaultConnection);
    {
        QSqlDatabase db = QSqlDatabase::database();
        if (db.isValid())
        {
            db.close();
        }
    }
    if (QSqlDatabase::contains(connectionName))
    {
        QSqlDatabase::removeDatabase(connectionName);
    }
}

void BandPlanTest::freq2BandMode_data()
{
    QTest::addColumn<double>("frequency");
    QTest::addColumn<BandPlan::BandPlanMode>("expectedMode");

    QTest::newRow("cw") << 14.0 << BandPlan::BAND_MODE_CW;
    QTest::newRow("digital") << 14.071 << BandPlan::BAND_MODE_DIGITAL;
    QTest::newRow("ft8") << 14.074 << BandPlan::BAND_MODE_FT8;
    QTest::newRow("ft8_from_below_double")
            << std::nextafter(14.074, 0.0)
            << BandPlan::BAND_MODE_FT8;
    QTest::newRow("usb") << 14.200 << BandPlan::BAND_MODE_USB;
    QTest::newRow("out_of_band") << 1.0 << BandPlan::BAND_MODE_PHONE;
    QTest::newRow("negative") << -1.0 << BandPlan::BAND_MODE_PHONE;
}

void BandPlanTest::freq2BandMode()
{
    QFETCH(double, frequency);
    QFETCH(BandPlan::BandPlanMode, expectedMode);

    QCOMPARE(BandPlan::freq2BandMode(frequency), expectedMode);
}

void BandPlanTest::freq2BandModeGroupString_data()
{
    QTest::addColumn<double>("frequency");
    QTest::addColumn<QString>("expectedGroup");

    QTest::newRow("cw") << 14.0 << QStringLiteral("CW");
    QTest::newRow("digital") << 14.071 << QStringLiteral("DIGITAL");
    QTest::newRow("ft8") << 14.074 << QStringLiteral("FTx");
    QTest::newRow("phone") << 14.200 << QStringLiteral("PHONE");
    QTest::newRow("out_of_band") << 1.0 << QStringLiteral("PHONE");
    QTest::newRow("negative") << -1.0 << QStringLiteral("PHONE");
}

void BandPlanTest::freq2BandModeGroupString()
{
    QFETCH(double, frequency);
    QFETCH(QString, expectedGroup);

    QCOMPARE(BandPlan::freq2BandModeGroupString(frequency), expectedGroup);
}

void BandPlanTest::freq2ExpectedMode_data()
{
    QTest::addColumn<double>("frequency");
    QTest::addColumn<QString>("expectedMode");
    QTest::addColumn<QString>("expectedSubmode");

    QTest::newRow("cw") << 14.0 << QStringLiteral("CW") << QString();
    QTest::newRow("digital_usb") << 14.071 << QStringLiteral("SSB") << QStringLiteral("USB");
    QTest::newRow("ft8") << 14.074 << QStringLiteral("FT8") << QString();
    QTest::newRow("ft4") << 14.081 << QStringLiteral("MFSK") << QStringLiteral("FT4");
    QTest::newRow("usb_voice") << 14.200 << QStringLiteral("SSB") << QStringLiteral("USB");
}

void BandPlanTest::freq2ExpectedMode()
{
    QFETCH(double, frequency);
    QFETCH(QString, expectedMode);
    QFETCH(QString, expectedSubmode);

    QString submode;
    QCOMPARE(BandPlan::freq2ExpectedMode(frequency, submode), expectedMode);
    QCOMPARE(submode, expectedSubmode);
}

void BandPlanTest::freq2Band_data()
{
    QTest::addColumn<double>("frequency");
    QTest::addColumn<QString>("expectedBand");

    const struct Case { double freq; const char *name; } cases[] = {
        {0.136, "2190m"},
        {0.473, "630m"},
        {1.9, "160m"},
        {3.6, "80m"},
        {5.36, "60m"},
        {7.1, "40m"},
        {10.11, "30m"},
        {14.2, "20m"},
        {18.1, "17m"},
        {21.1, "15m"},
        {24.9, "12m"},
        {28.1, "10m"},
        {50.1, "6m"},
        {70.1, "4m"},
        {144.1, "2m"},
        {222.1, "1.25m"},
        {430.1, "70cm"},
        {902.1, "33cm"},
        {1240.1, "23cm"}
    };

    for (const Case &c : cases)
    {
        QTest::newRow(QByteArray::number(c.freq).constData())
            << c.freq
            << QString::fromLatin1(c.name);
    }

    QTest::newRow("20m_start_from_below_double")
            << std::nextafter(14.000, 0.0)
            << QStringLiteral("20m");
    QTest::newRow("20m_end_from_above_double")
            << std::nextafter(14.350, 15.0)
            << QStringLiteral("20m");
}

void BandPlanTest::freq2Band()
{
    QFETCH(double, frequency);
    QFETCH(QString, expectedBand);

    const Band band = BandPlan::freq2Band(frequency);
    QCOMPARE(band.name, expectedBand);
}

void BandPlanTest::resolveBand()
{
    QCOMPARE(BandPlan::resolveBand(14.074, QStringLiteral("40m")).name,
             QStringLiteral("20m"));
    QCOMPARE(BandPlan::resolveBand(0.0, QStringLiteral("40m")).name,
             QStringLiteral("40m"));
    QVERIFY(BandPlan::resolveBand(0.0, QStringLiteral("invalid")).name.isEmpty());
}

void BandPlanTest::bandUsesWholeHzBoundaries()
{
    Band band;
    band.name = QStringLiteral("20m");
    band.start = 14.000;
    band.end = 14.350;

    QVERIFY(band.contains(std::nextafter(band.start, 0.0)));
    QVERIFY(band.contains(std::nextafter(band.end, 15.0)));
    QVERIFY(!band.contains(13.999999));
    QVERIFY(!band.contains(14.350001));

    Band sameBand = band;
    sameBand.start = std::nextafter(band.start, 0.0);
    sameBand.end = std::nextafter(band.end, 15.0);
    QVERIFY(band == sameBand);
}

void BandPlanTest::bandsList_onlyDXCC()
{
    const QList<Band> bands = BandPlan::bandsList(true);
    QStringList bandNames;
    for (const Band &band : bands)
    {
        bandNames << band.name;
    }

    const QStringList expected = {
        QStringLiteral("160m"),
        QStringLiteral("80m"),
        QStringLiteral("40m"),
        QStringLiteral("30m"),
        QStringLiteral("20m"),
        QStringLiteral("17m"),
        QStringLiteral("15m"),
        QStringLiteral("12m"),
        QStringLiteral("10m"),
        QStringLiteral("6m"),
        QStringLiteral("2m"),
        QStringLiteral("70cm"),
        QStringLiteral("23cm"),
        QStringLiteral("13cm"),
        QStringLiteral("3cm")
    };

    QCOMPARE(bandNames, expected);
}

void BandPlanTest::modeToDXCCModeGroup_data()
{
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QString>("expectedGroup");

    QTest::newRow("ssb") << QStringLiteral("SSB") << QStringLiteral("PHONE");
    QTest::newRow("cw") << QStringLiteral("CW") << QStringLiteral("CW");
    QTest::newRow("ft8") << QStringLiteral("FT8") << QStringLiteral("DIGITAL");
    QTest::newRow("ft4") << QStringLiteral("FT4") << QStringLiteral("DIGITAL");
    QTest::newRow("ft2") << QStringLiteral("FT2") << QStringLiteral("DIGITAL");
    QTest::newRow("rtty") << QStringLiteral("RTTY") << QStringLiteral("DIGITAL");
}

void BandPlanTest::modeToDXCCModeGroup()
{
    QFETCH(QString, mode);
    QFETCH(QString, expectedGroup);

    QCOMPARE(BandPlan::modeToDXCCModeGroup(mode), expectedGroup);
}

void BandPlanTest::isFTxMode_data()
{
    QTest::addColumn<QString>("mode");
    QTest::addColumn<bool>("expected");

    QTest::newRow("ft8")  << QStringLiteral("FT8")  << true;
    QTest::newRow("ft4")  << QStringLiteral("FT4")  << true;
    QTest::newRow("ft2")  << QStringLiteral("FT2")  << true;
    QTest::newRow("cw")   << QStringLiteral("CW")   << false;
    QTest::newRow("rtty") << QStringLiteral("RTTY") << false;
    QTest::newRow("ft")   << QStringLiteral("FT")   << false;
}

void BandPlanTest::isFTxMode()
{
    QFETCH(QString, mode);
    QFETCH(bool, expected);

    QCOMPARE(BandPlan::isFTxMode(mode), expected);
}

void BandPlanTest::isFTxBandMode_data()
{
    QTest::addColumn<BandPlan::BandPlanMode>("mode");
    QTest::addColumn<bool>("expected");

    QTest::newRow("ft8")     << BandPlan::BAND_MODE_FT8     << true;
    QTest::newRow("ft4")     << BandPlan::BAND_MODE_FT4     << true;
    QTest::newRow("ft2")     << BandPlan::BAND_MODE_FT2     << true;
    QTest::newRow("cw")      << BandPlan::BAND_MODE_CW      << false;
    QTest::newRow("digital") << BandPlan::BAND_MODE_DIGITAL << false;
    QTest::newRow("phone")   << BandPlan::BAND_MODE_PHONE   << false;
}

void BandPlanTest::isFTxBandMode()
{
    QFETCH(BandPlan::BandPlanMode, mode);
    QFETCH(bool, expected);

    QCOMPARE(BandPlan::isFTxBandMode(mode), expected);
}

QTEST_MAIN(BandPlanTest)

#include "tst_bandplan.moc"
