#include "IBPBeacon.h"

#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.ibpbeacon");

const QList<IBPBeacon::Band> &IBPBeacon::bands()
{
    FCT_IDENTIFICATION;

    static const QList<Band> bands = QList<Band>()
                                     << Band(QStringLiteral("20m"), 14.1)
                                     << Band(QStringLiteral("17m"), 18.11)
                                     << Band(QStringLiteral("15m"), 21.15)
                                     << Band(QStringLiteral("12m"), 24.93)
                                     << Band(QStringLiteral("10m"), 28.2);

    return bands;
}

const QList<IBPBeacon::Station> &IBPBeacon::beacons()
{
    FCT_IDENTIFICATION;

    static const QList<Station> beacons = QList<Station>()
                                          << Station(QStringLiteral("4U1UN"), 40.7501, -73.9682, true)
                                          << Station(QStringLiteral("VE8AT"), 79.9949, -85.8451, true)
                                          << Station(QStringLiteral("W6WX"), 37.1599, -121.9083, true)
                                          << Station(QStringLiteral("KH6RS"), 20.7652, -156.3502, true)
                                          << Station(QStringLiteral("ZL6B"), -41.04350, 175.5952, true)
                                          << Station(QStringLiteral("VK6RBP"), -32.1093, 116.0712, true)
                                          << Station(QStringLiteral("JA2IGY"), 34.4613, 136.7818, true)
                                          << Station(QStringLiteral("RR9O"), 55.0484, 82.9227, true)
                                          << Station(QStringLiteral("VR2B"), 22.2705, 114.1507, true)
                                          << Station(QStringLiteral("4S7B"), 6.8915, 79.8559, false)
                                          << Station(QStringLiteral("ZS6DN"), -26.6531, 27.9474, true)
                                          << Station(QStringLiteral("5Z4B"), -1.2687, 36.8094, true)
                                          << Station(QStringLiteral("4X6TU"), 32.0622, 34.8069, true)
                                          << Station(QStringLiteral("OH2B"), 60.2920, 24.3942, false)
                                          << Station(QStringLiteral("CS3B"), 32.8217, -17.2325, true)
                                          << Station(QStringLiteral("LU4AA"), -34.6439, -58.4138, true)
                                          << Station(QStringLiteral("OA4B"), -12.0940, -77.0165, true)
                                          << Station(QStringLiteral("YV5B"), 9.0964, -67.8239, false);

    return beacons;
}
