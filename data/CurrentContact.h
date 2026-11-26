#ifndef QLOG_DATA_CURRENTCONTACT_H
#define QLOG_DATA_CURRENTCONTACT_H

#include <QMetaType>
#include <QString>
#include <QtNumeric>

struct CurrentContact
{
    QString callsign;
    QString grid;
    double lat = qQNaN();
    double lon = qQNaN();
    double azimuth = qQNaN();
    double lpAzimuth = qQNaN();
    double distance = qQNaN();

    bool operator==(const CurrentContact &other) const
    {
        return callsign == other.callsign
               && grid == other.grid
               && sameNumber(lat, other.lat)
               && sameNumber(lon, other.lon)
               && sameNumber(azimuth, other.azimuth)
               && sameNumber(lpAzimuth, other.lpAzimuth)
               && sameNumber(distance, other.distance);
    }

    bool operator!=(const CurrentContact &other) const
    {
        return !( *this == other );
    }

private:
    static bool sameNumber(double a, double b)
    {
        return ( qIsNaN(a) && qIsNaN(b) ) || ( a == b );
    }
};

Q_DECLARE_METATYPE(CurrentContact)

#endif // QLOG_DATA_CURRENTCONTACT_H
