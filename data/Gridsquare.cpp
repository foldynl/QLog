#include "Gridsquare.h"
#include <core/debug.h>
#include <QtMath>
#include <QRegularExpression>
#include <cmath>
#include <QLocale>

#define EARTH_RADIUS 6371
#define EARTH_CIRCUM 40075

MODULE_IDENTIFICATION("qlog.core.gridsquare");

Gridsquare::Gridsquare(const QString &in_grid) :
    validGrid(false), lat(qQNaN()), lon(qQNaN())
{
    FCT_IDENTIFICATION;

    if ( !in_grid.isEmpty() )
    {
        grid = in_grid.toUpper();

        if ( gridRegEx().match(grid).hasMatch() )
        {
            lon = (grid.at(0).toLatin1() - 'A') * 20 - 180;
            lat = (grid.at(1).toLatin1() - 'A') * 10 - 90;

            if ( grid.size() >= 4 )
            {
                lon += (grid.at(2).toLatin1() - '0') * 2;
                lat += (grid.at(3).toLatin1() - '0') * 1;

                if ( grid.size() >= 6 )
                {
                    lon += (grid.at(4).toLatin1() - 'A') * (5.0/60.0);
                    lat += (grid.at(5).toLatin1() - 'A') * (2.5/60.0);

                    if ( grid.size() >= 8 )
                    {
                        lon += (grid.at(6).toLatin1() - '0') * (30.0/3600.0);
                        lat += (grid.at(7).toLatin1() - '0') * (15.0/3600.0);

                        // move to the center
                        lon += 15.0/3600.0;
                        lat += 7.5/3600.0;
                    }
                    else
                    {
                        // move to the center
                        lon += 2.5/60.0;
                        lat += 1.25/60.0;
                    }

                }
                else
                {
                    // move to the center
                    lon += 1;
                    lat += 0.5;
                }
            }
            else
            {
                // move 0to the center
                lon += 10;
                lat += 5;
            }
            validGrid = true;
        }
        else
        {
            /* not valid grid */
            grid = QString();
        }
    }
}

Gridsquare::Gridsquare(const double inlat, const double inlon) :
    validGrid(false), lat(inlat), lon(inlon)
{
    FCT_IDENTIFICATION;

    QString U = "ABCDEFGHIJKLMNOPQRSTUVWX";

    if ( qIsNaN(inlat)
         || qIsNaN(inlon)
         || qAbs(inlat) >= 90.0
         || qAbs(inlon) >= 180.0 )
    {
        qCDebug(runtime) << "Invalid Grid lat/lon" << inlat << inlon;
        this->lat = this->lon = qQNaN();
    }
    else
    {
        // currently user only for SOTA where only 6 chars are enough
        double modifiedLat = lat + 90.0;
        double modifiedLon = lon + 180.0;
        QString grid1 = U.at(static_cast<int>(modifiedLon/20));
        QString grid2 = U.at(static_cast<int>(modifiedLat/10));
        QString grid3 = QString::number(static_cast<int>(fmod((modifiedLon/2), 10.0)));
        QString grid4 = QString::number(static_cast<int>(fmod(modifiedLat,10.0)));
        double rLat = (modifiedLat - static_cast<int>(modifiedLat)) * 60;
        double rLon = (modifiedLon - 2*static_cast<int>(modifiedLon/2)) *60;
        QString grid5 = U.at((int)(rLon/5));
        QString grid6 = U.at((int)(rLat/2.5));
        grid = grid1 + grid2 + grid3 + grid4 + grid5 + grid6;
        qCDebug(runtime) << grid;
        validGrid = true;
    }
}

const QRegularExpression Gridsquare::gridRegEx()
{
    FCT_IDENTIFICATION;

    return QRegularExpression("^[A-Ra-r]{2}(?:[0-9]{2}|[0-9]{2}[A-Xa-x]{2}|[0-9]{2}[A-Xa-x]{2}[0-9]{2})?$");
    //return QRegularExpression("^[A-Ra-r]{2}[0-9]{2}([A-Xa-x]{2})?([0-9]{2})?$");
}

const QRegularExpression Gridsquare::gridVUCCRegEx()
{
    FCT_IDENTIFICATION;

    return QRegularExpression("^[A-Ra-r]{2}(?:[0-9]{2}|[0-9]{2}[A-Xa-x]{2}),[ ]*[A-Ra-r]{2}(?:[0-9]{2}|[0-9]{2}[A-Xa-x]{2})$|"
                              "^[A-Ra-r]{2}(?:[0-9]{2}|[0-9]{2}[A-Xa-x]{2}),[ ]*[A-Ra-r]{2}(?:[0-9]{2}|[0-9]{2}[A-Xa-x]{2}),[ ]*[A-Ra-r]{2}(?:[0-9]{2}|[0-9]{2}[A-Xa-x]{2}),[ ]*[A-Ra-r]{2}(?:[0-9]{2}|[0-9]{2}[A-Xa-x]{2})$");
}

const QRegularExpression Gridsquare::gridExtRegEx()
{
    FCT_IDENTIFICATION;

    return QRegularExpression("^[A-Xa-x]{2}?([0-9]{2})?$");
}

double Gridsquare::distance2localeUnitDistance(double km,
                                               QString &unit)
{
    FCT_IDENTIFICATION;

    unit = QObject::tr("km");
    double ret = km;

    // All imperial systems
    if ( QLocale::system().measurementSystem() != QLocale::MetricSystem)
    {
        unit = QObject::tr("miles");
        ret = km * localeDistanceCoef();
    }
    return ret;
}

double Gridsquare::localeDistanceCoef()
{
    FCT_IDENTIFICATION;

    return (QLocale::system().measurementSystem() != QLocale::MetricSystem) ? 0.6213711922
                                                                            : 1.0;
}

bool Gridsquare::isValid() const
{
    FCT_IDENTIFICATION;
    return validGrid;
}

bool Gridsquare::distanceTo(double lat, double lon, double &distance) const
{
    FCT_IDENTIFICATION;

    if ( !isValid() )
    {
        distance = 0.0;
        return false;
    }

    /* https://www.movable-type.co.uk/scripts/latlong.html */
    double dLat = (lat - this->getLatitude()) * M_PI / 180;
    double dLon = (lon - this->getLongitude()) * M_PI / 180;

    double lat1 = this->getLatitude() * M_PI / 180;
    double lat2 = lat * M_PI / 180;

    double a = sin(dLat / 2) * sin(dLat / 2) +
               sin(dLon / 2) * sin(dLon / 2) * cos(lat1) * cos(lat2);

    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    distance = EARTH_RADIUS * c;

    return true;
}

bool Gridsquare::distanceTo(const Gridsquare &in_grid, double &distance) const
{
    FCT_IDENTIFICATION;

    if ( !in_grid.isValid() )
    {
        distance = 0.0;
        return false;
    }
    return distanceTo(in_grid.getLatitude(), in_grid.getLongitude(), distance);
}

bool Gridsquare::bearingTo(double lat, double lon, double &bearing) const
{
    FCT_IDENTIFICATION;

    if ( !isValid() )
    {
        bearing = 0.0;
        return false;
    }

    double dLon = (lon - this->getLongitude()) * M_PI / 180;
    double lat1 = this->getLatitude() * M_PI / 180;
    double lat2 = lat * M_PI / 180;

    double y = sin(dLon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dLon);

    bearing = fmod((180.0 * atan2(y, x) / M_PI + 360.0), 360.0);

    return true;
}

bool Gridsquare::bearingTo(const Gridsquare &in_grid, double &bearing) const
{
    FCT_IDENTIFICATION;

    if ( ! in_grid.isValid() )
    {
        bearing = 0.0;
        return false;
    }

    return bearingTo(in_grid.getLatitude(), in_grid.getLongitude(), bearing);
}
