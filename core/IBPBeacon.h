#ifndef QLOG_CORE_IBPBEACON_H
#define QLOG_CORE_IBPBEACON_H

#include <QList>
#include <QString>

class IBPBeacon
{
public:
    struct Band
    {
        Band() = default;
        Band(const QString &inName,
             double inFrequency)
            : name(inName),
              frequency(inFrequency)
        {
        }

        QString name;
        double frequency = 0.0;
    };

    struct Station
    {
        Station() = default;
        Station(const QString &inCallsign,
                double inLatitude,
                double inLongitude,
                bool inActive)
            : callsign(inCallsign),
              latitude(inLatitude),
              longitude(inLongitude),
              active(inActive)
        {
        }

        QString callsign;
        double latitude = 0.0;
        double longitude = 0.0;
        bool active = false;
    };

    static const QList<Band> &bands();
    static const QList<Station> &beacons();
    static double frequency(const QString &bandName);
};

#endif // QLOG_CORE_IBPBEACON_H
