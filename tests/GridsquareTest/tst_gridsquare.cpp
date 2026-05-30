#include <QtTest>
#include <QtMath>

#include "data/Gridsquare.h"

class GridsquareTest : public QObject
{
    Q_OBJECT

private slots:
    void initTest();
    void invalidGridStrings_data();
    void invalidGridStrings();
    void validGridStrings_data();
    void validGridStrings();
    void mapDisplayGrid_data();
    void mapDisplayGrid();
    void invalidCoordinateCtor_data();
    void invalidCoordinateCtor();
    void validCoordinateCtor_data();
    void validCoordinateCtor();
    void gridToLatLon_data();
    void gridToLatLon();
    void gridToLatLon_invalid_data();
    void gridToLatLon_invalid();
    void latLonInput_data();
    void latLonInput();
    void getGrid_data();
    void getGrid();
    void distanceToCoordinates_data();
    void distanceToCoordinates();
    void distanceToGrids_data();
    void distanceToGrids();
    void distanceSymmetry();
    void bearingToCoordinates_data();
    void bearingToCoordinates();
    void bearingToGrids_data();
    void bearingToGrids();
    void bearingSymmetry();
};

void GridsquareTest::initTest()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
}

void GridsquareTest::invalidGridStrings_data()
{
    QTest::addColumn<QString>("grid");

    const QStringList invalidGrids = {
        "aabbcc",
        "112233",
        "AA1122",
        "11AA22",
        "ZZ99AA",
        "AABBCCDD",
        "AABBCC11",
        "11223344",
        "AA112233",
        "11AA2233",
        "11AA22AA",
        "ZZ99AA00",
        "AA11AA00AA"
    };

    for (const QString &grid : invalidGrids)
    {
        const QByteArray rowId = grid.toUtf8();
        QTest::newRow(rowId.constData()) << grid;
    }
}

void GridsquareTest::invalidGridStrings()
{
    QFETCH(QString, grid);

    const Gridsquare gs(grid);
    QVERIFY(!gs.isValid());
    QCOMPARE(gs.getGrid(), QString());
    QVERIFY(qIsNaN(gs.getLatitude()));
    QVERIFY(qIsNaN(gs.getLongitude()));
}

void GridsquareTest::validGridStrings_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expectedGrid");

    const struct Case { const char *input; const char *expected; } cases[] = {
        {"AA", "AA"},
        {"AA11", "AA11"},
        {"AA11AA", "AA11AA"},
        {"AA11AA11", "AA11AA11"},
        {"aa", "AA"},
        {"aa11", "AA11"},
        {"aa11aa", "AA11AA"},
        {"aa11aa11", "AA11AA11"},
        {"aA", "AA"},
        {"aA11", "AA11"},
        {"aA11Aa", "AA11AA"},
        {"Aa11Aa11", "AA11AA11"}
    };

    for (const Case &c : cases)
    {
        QTest::newRow(c.input) << QString::fromLatin1(c.input)
                               << QString::fromLatin1(c.expected);
    }
}

void GridsquareTest::validGridStrings()
{
    QFETCH(QString, input);
    QFETCH(QString, expectedGrid);

    const Gridsquare gs(input);
    QVERIFY(gs.isValid());
    QCOMPARE(gs.getGrid(), expectedGrid);
}

void GridsquareTest::mapDisplayGrid_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("expectedValid");
    QTest::addColumn<QString>("expectedGrid");

    const struct Case
    {
        const char *name;
        const char *input;
        bool expectedValid;
        const char *expectedGrid;
    } cases[] = {
        {"valid_six", "AA11AA", true, "AA11AA"},
        {"valid_eight", "AA11AA00", true, "AA11AA00"},
        {"long_valid_prefix", "AA11AA00AA", true, "AA11AA00"},
        {"long_valid_prefix_lowercase", "aa11aa00aa", true, "AA11AA00"},
        {"long_invalid_prefix", "AA11A000AA", false, ""},
        {"short_invalid", "INVALID", false, ""}
    };

    for ( const Case &c : cases )
    {
        QTest::newRow(c.name)
            << QString::fromLatin1(c.input)
            << c.expectedValid
            << QString::fromLatin1(c.expectedGrid);
    }
}

void GridsquareTest::mapDisplayGrid()
{
    QFETCH(QString, input);
    QFETCH(bool, expectedValid);
    QFETCH(QString, expectedGrid);

    const Gridsquare gs = Gridsquare::mapDisplayGrid(input);
    QCOMPARE(gs.isValid(), expectedValid);
    QCOMPARE(gs.getGrid(), expectedGrid);
}

void GridsquareTest::invalidCoordinateCtor_data()
{
    QTest::addColumn<double>("latitude");
    QTest::addColumn<double>("longitude");

    const struct Case { double lat; double lon; } cases[] = {
        {250.0, 250.0},
        {-250.0, 250.0},
        {250.0, -250.0},
        {-250.0, -250.0}
    };

    const int casesCount = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < casesCount; ++i)
    {
        QTest::newRow(QString("invalid_coord_%1").arg(i).toUtf8().constData())
            << cases[i].lat << cases[i].lon;
    }
}

void GridsquareTest::invalidCoordinateCtor()
{
    QFETCH(double, latitude);
    QFETCH(double, longitude);

    const Gridsquare gs(latitude, longitude);
    QVERIFY(!gs.isValid());
    QVERIFY(qIsNaN(gs.getLatitude()));
    QVERIFY(qIsNaN(gs.getLongitude()));
    QCOMPARE(gs.getGrid(), QString());
}

void GridsquareTest::validCoordinateCtor_data()
{
    QTest::addColumn<double>("latitude");
    QTest::addColumn<double>("longitude");

    const struct Case { double lat; double lon; } cases[] = {
        {40.0, -179.0},
        {40.0, 179.0},
        {-40.0, -179.0},
        {-40.0, 179.0}
    };

    const int casesCount = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < casesCount; ++i)
    {
        QTest::newRow(QString("valid_coord_%1").arg(i).toUtf8().constData())
            << cases[i].lat << cases[i].lon;
    }
}

void GridsquareTest::validCoordinateCtor()
{
    QFETCH(double, latitude);
    QFETCH(double, longitude);

    const Gridsquare gs(latitude, longitude);
    QVERIFY(gs.isValid());
    QCOMPARE(gs.getLatitude(), latitude);
    QCOMPARE(gs.getLongitude(), longitude);
}

void GridsquareTest::gridToLatLon_data()
{
    QTest::addColumn<QString>("grid");
    QTest::addColumn<int>("expectedLatitude");
    QTest::addColumn<int>("expectedLongitude");

    const struct Case { const char *grid; int lat; int lon; } cases[] = {
        {"AA11", -88, -177},
        {"AR11", 82, -177},
        {"RA11", -88, 163},
        {"RR11", 82, 163},
        {"IJ11", 2, -17},
        {"aa11", -88, -177},
        {"ar11", 82, -177},
        {"AA11AA", -89, -178},
        {"AR11AA", 81, -178},
        {"RA11AA", -89, 162},
        {"RR11AA", 81, 162},
        {"IJ11AA", 1, -18},
        {"aa11aa", -89, -178},
        {"ar11aa", 81, -178},
        {"aa11AA", -89, -178},
        {"ar11AA", 81, -178},
        {"AA11aa", -89, -178},
        {"AR11aa", 81, -178},
        {"AA11AA11", -89, -178},
        {"AR11AA11", 81, -178},
        {"RA11AA11", -89, 162},
        {"RR11AA11", 81, 162},
        {"IJ11AA11", 1, -18},
        {"aa11aa11", -89, -178},
        {"ar11aa11", 81, -178},
        {"aa11AA11", -89, -178},
        {"ar11AA11", 81, -178},
        {"AA11aa11", -89, -178},
        {"AR11aa11", 81, -178}
    };

    for (const Case &c : cases)
    {
        QTest::newRow(c.grid)
            << QString::fromLatin1(c.grid)
            << c.lat
            << c.lon;
    }
}

void GridsquareTest::gridToLatLon()
{
    QFETCH(QString, grid);
    QFETCH(int, expectedLatitude);
    QFETCH(int, expectedLongitude);

    const Gridsquare gs(grid);
    QVERIFY(gs.isValid());
    // Use floor(d + 0.5) instead of qRound() to get consistent "round half up"
    // behaviour across platforms. qRound() for negative half-integers (e.g. -88.5)
    // produces different results on MSVC vs GCC due to floating-point precision.
    QCOMPARE(static_cast<int>(std::floor(gs.getLatitude()  + 0.5)), expectedLatitude);
    QCOMPARE(static_cast<int>(std::floor(gs.getLongitude() + 0.5)), expectedLongitude);
}

void GridsquareTest::gridToLatLon_invalid_data()
{
    QTest::addColumn<QString>("grid");

    const QStringList invalids = {
        "ZZ11",
        "ZZ11AA",
        "ZZ11AA11"
    };

    for (const QString &grid : invalids)
    {
        QTest::newRow(grid.toUtf8().constData()) << grid;
    }
}

void GridsquareTest::gridToLatLon_invalid()
{
    QFETCH(QString, grid);
    const Gridsquare gs(grid);
    QVERIFY(!gs.isValid());
    QVERIFY(qIsNaN(gs.getLatitude()));
    QVERIFY(qIsNaN(gs.getLongitude()));
}

void GridsquareTest::latLonInput_data()
{
    QTest::addColumn<double>("latitude");
    QTest::addColumn<double>("longitude");

    const struct Case { double lat; double lon; } cases[] = {
        {18.0, 17.9},
        {-14.9, 179.0},
        {14.9, -179.0},
        {-14.9, -179.0}
    };

    const int casesCount = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < casesCount; ++i)
    {
        QTest::newRow(QString("latlon_input_%1").arg(i).toUtf8().constData())
            << cases[i].lat << cases[i].lon;
    }
}

void GridsquareTest::latLonInput()
{
    QFETCH(double, latitude);
    QFETCH(double, longitude);

    const Gridsquare gs(latitude, longitude);
    QVERIFY(gs.isValid());
    QCOMPARE(gs.getLatitude(), latitude);
    QCOMPARE(gs.getLongitude(), longitude);
}

void GridsquareTest::getGrid_data()
{
    QTest::addColumn<bool>("fromCoordinates");
    QTest::addColumn<QString>("gridInput");
    QTest::addColumn<double>("latitude");
    QTest::addColumn<double>("longitude");
    QTest::addColumn<QString>("expectedGrid");

    struct Case
    {
        const char *name;
        bool fromCoordinates;
        const char *gridInput;
        double lat;
        double lon;
        const char *expected;
    };

    const Case cases[] = {
        {"invalid_string", false, "INVALID", 0.0, 0.0, ""},
        {"from_coordinates", true, "", 18.0, 17.9, "JK88WA"},
        {"two_chars", false, "AA", 0.0, 0.0, "AA"},
        {"four_chars", false, "AA11", 0.0, 0.0, "AA11"},
        {"six_chars", false, "AA11CD", 0.0, 0.0, "AA11CD"},
        {"eight_chars", false, "AA11CD22", 0.0, 0.0, "AA11CD22"},
        {"normalize", false, "AA11cd22", 0.0, 0.0, "AA11CD22"}
    };

    for (const Case &c : cases)
    {
        QTest::newRow(c.name)
            << c.fromCoordinates
            << QString::fromLatin1(c.gridInput)
            << c.lat
            << c.lon
            << QString::fromLatin1(c.expected);
    }
}

void GridsquareTest::getGrid()
{
    QFETCH(bool, fromCoordinates);
    QFETCH(QString, gridInput);
    QFETCH(double, latitude);
    QFETCH(double, longitude);
    QFETCH(QString, expectedGrid);

    const Gridsquare gs = fromCoordinates
                              ? Gridsquare(latitude, longitude)
                              : Gridsquare(gridInput);
    QCOMPARE(gs.getGrid(), expectedGrid);
}

void GridsquareTest::distanceToCoordinates_data()
{
    QTest::addColumn<bool>("fromCoordinates");
    QTest::addColumn<QString>("gridInput");
    QTest::addColumn<double>("sourceLat");
    QTest::addColumn<double>("sourceLon");
    QTest::addColumn<double>("targetLat");
    QTest::addColumn<double>("targetLon");
    QTest::addColumn<bool>("expectedResult");
    QTest::addColumn<int>("expectedRoundedDistance");

    struct Case
    {
        const char *name;
        bool fromCoordinates;
        const char *gridInput;
        double sourceLat;
        double sourceLon;
        double targetLat;
        double targetLon;
        bool expectedResult;
        int expectedRoundedDistance;
    };

    const Case cases[] = {
        {"invalid_grid", false, "18123", 0.0, 0.0, 12.0, 17.9, false, 0},
        {"from_latlon", true, "", 18.0, 17.9, 12.0, 17.9, true, 667},
        {"grid_aa11", false, "AA11", 0.0, 0.0, 12.0, 17.9, true, 11504},
        {"grid_aa11cd", false, "AA11CD", 0.0, 0.0, 12.0, 17.9, true, 11465},
        {"grid_aa11cd22", false, "AA11CD22", 0.0, 0.0, 12.0, 17.9, true, 11464},
        {"grid_aa11cd22_lower", false, "AA11cd22", 0.0, 0.0, 12.0, 17.9, true, 11464}
    };

    for (const Case &c : cases)
    {
        QTest::newRow(c.name)
            << c.fromCoordinates
            << QString::fromLatin1(c.gridInput)
            << c.sourceLat
            << c.sourceLon
            << c.targetLat
            << c.targetLon
            << c.expectedResult
            << c.expectedRoundedDistance;
    }
}

void GridsquareTest::distanceToCoordinates()
{
    QFETCH(bool, fromCoordinates);
    QFETCH(QString, gridInput);
    QFETCH(double, sourceLat);
    QFETCH(double, sourceLon);
    QFETCH(double, targetLat);
    QFETCH(double, targetLon);
    QFETCH(bool, expectedResult);
    QFETCH(int, expectedRoundedDistance);

    const Gridsquare source = fromCoordinates
                                  ? Gridsquare(sourceLat, sourceLon)
                                  : Gridsquare(gridInput);

    double distance = -1.0;
    QCOMPARE(source.distanceTo(targetLat, targetLon, distance), expectedResult);

    if (expectedResult)
    {
        QCOMPARE(qRound(distance), expectedRoundedDistance);
    }
    else
    {
        QCOMPARE(distance, 0.0);
    }
}

void GridsquareTest::distanceToGrids_data()
{
    QTest::addColumn<QString>("sourceGrid");
    QTest::addColumn<QString>("targetGrid");
    QTest::addColumn<int>("expectedRoundedDistance");

    const struct Case { const char *name; const char *source; const char *target; int distance; } cases[] = {
        {"long_short_path", "JO80AA", "CD01AA", 18189},
        {"long_short_path2", "JO80AA", "RE01AA", 17439},
        {"dateline_shortest", "RB80AA", "AA01AA", 1001}
    };

    for (const Case &c : cases)
    {
        QTest::newRow(c.name)
            << QString::fromLatin1(c.source)
            << QString::fromLatin1(c.target)
            << c.distance;
    }
}

void GridsquareTest::distanceToGrids()
{
    QFETCH(QString, sourceGrid);
    QFETCH(QString, targetGrid);
    QFETCH(int, expectedRoundedDistance);

    const Gridsquare source(sourceGrid);
    const Gridsquare target(targetGrid);

    double distance = -1.0;
    QVERIFY(source.distanceTo(target, distance));
    QCOMPARE(qRound(distance), expectedRoundedDistance);
}

void GridsquareTest::distanceSymmetry()
{
    const Gridsquare gridA("RB80AA");
    const Gridsquare gridB("AA01AA");

    QVERIFY(gridA.isValid());
    QVERIFY(gridB.isValid());

    double distanceAB = -1.0;
    double distanceBA = -1.0;
    QVERIFY(gridA.distanceTo(gridB, distanceAB));
    QVERIFY(gridB.distanceTo(gridA, distanceBA));
    QCOMPARE(distanceAB, distanceBA);

    double sameDistance = -1.0;
    QVERIFY(gridA.distanceTo(gridA, sameDistance));
    QCOMPARE(sameDistance, 0.0);
}

void GridsquareTest::bearingToCoordinates_data()
{
    QTest::addColumn<bool>("fromCoordinates");
    QTest::addColumn<QString>("gridInput");
    QTest::addColumn<double>("sourceLat");
    QTest::addColumn<double>("sourceLon");
    QTest::addColumn<double>("targetLat");
    QTest::addColumn<double>("targetLon");
    QTest::addColumn<bool>("expectedResult");
    QTest::addColumn<int>("expectedRoundedBearing");

    struct Case
    {
        const char *name;
        bool fromCoordinates;
        const char *gridInput;
        double sourceLat;
        double sourceLon;
        double targetLat;
        double targetLon;
        bool expectedResult;
        int expectedRoundedBearing;
    };

    const Case cases[] = {
        {"invalid_grid", false, "18123", 0.0, 0.0, 12.0, 17.9, false, 0},
        {"from_latlon", true, "", 18.0, 17.9, 12.0, 17.9, true, 180},
        {"grid_aa11", false, "AA11", 0.0, 0.0, 12.0, 17.9, true, 195},
        {"grid_aa11cd", false, "AA11CD", 0.0, 0.0, 12.0, 17.9, true, 196},
        {"grid_aa11cd22", false, "AA11CD22", 0.0, 0.0, 12.0, 17.9, true, 196},
        {"grid_aa11cd22_lower", false, "AA11cd22", 0.0, 0.0, 12.0, 17.9, true, 196}
    };

    for (const Case &c : cases)
    {
        QTest::newRow(c.name)
            << c.fromCoordinates
            << QString::fromLatin1(c.gridInput)
            << c.sourceLat
            << c.sourceLon
            << c.targetLat
            << c.targetLon
            << c.expectedResult
            << c.expectedRoundedBearing;
    }
}

void GridsquareTest::bearingToCoordinates()
{
    QFETCH(bool, fromCoordinates);
    QFETCH(QString, gridInput);
    QFETCH(double, sourceLat);
    QFETCH(double, sourceLon);
    QFETCH(double, targetLat);
    QFETCH(double, targetLon);
    QFETCH(bool, expectedResult);
    QFETCH(int, expectedRoundedBearing);

    const Gridsquare source = fromCoordinates
                                  ? Gridsquare(sourceLat, sourceLon)
                                  : Gridsquare(gridInput);

    double bearing = -1.0;
    QCOMPARE(source.bearingTo(targetLat, targetLon, bearing), expectedResult);

    if (expectedResult)
    {
        QCOMPARE(qRound(bearing), expectedRoundedBearing);
    }
    else
    {
        QCOMPARE(bearing, 0.0);
    }
}

void GridsquareTest::bearingToGrids_data()
{
    QTest::addColumn<QString>("sourceGrid");
    QTest::addColumn<QString>("targetGrid");
    QTest::addColumn<int>("expectedRoundedBearing");

    const struct Case { const char *name; const char *source; const char *target; int bearing; } cases[] = {
        {"long_short_path", "JO80AA", "CD01AA", 228},
        {"long_short_path2", "JO80AA", "RE01AA", 101},
        {"dateline_shortest", "RB80AA", "AA01AA", 180}
    };

    for (const Case &c : cases)
    {
        QTest::newRow(c.name)
            << QString::fromLatin1(c.source)
            << QString::fromLatin1(c.target)
            << c.bearing;
    }
}

void GridsquareTest::bearingToGrids()
{
    QFETCH(QString, sourceGrid);
    QFETCH(QString, targetGrid);
    QFETCH(int, expectedRoundedBearing);

    const Gridsquare source(sourceGrid);
    const Gridsquare target(targetGrid);

    double bearing = -1.0;
    QVERIFY(source.bearingTo(target, bearing));
    QCOMPARE(qRound(bearing), expectedRoundedBearing);
}

void GridsquareTest::bearingSymmetry()
{
    const Gridsquare gridA("JO80AA");
    const Gridsquare gridB("JO80AA");

    QVERIFY(gridA.isValid());
    QVERIFY(gridB.isValid());

    double bearingAB = -1.0;
    double bearingBA = -1.0;
    QVERIFY(gridA.bearingTo(gridB, bearingAB));
    QVERIFY(gridB.bearingTo(gridA, bearingBA));
    QCOMPARE(bearingAB, bearingBA);
    QCOMPARE(bearingAB, 0.0);
}

QTEST_APPLESS_MAIN(GridsquareTest)

#include "tst_gridsquare.moc"
