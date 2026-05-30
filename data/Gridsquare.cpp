#include "Gridsquare.h"
#include <core/debug.h>
#include <QtMath>
#include <QRegularExpression>
#include <cmath>
#include <QLocale>

MODULE_IDENTIFICATION("qlog.core.gridsquare");

static const double DEG_TO_RAD = M_PI / 180.0;
static const double RAD_TO_DEG = 180.0 / M_PI;
static const double IARU_EARTH_RADIUS_KM = 40032.0 / (2.0 * M_PI);

static bool gridCharInRange(const QString &grid, int index, char min, char max)
{
    const char ch = grid.at(index).toLatin1();
    return ch >= min && ch <= max;
}

static bool gridCharIsDigit(const QString &grid, int index)
{
    return gridCharInRange(grid, index, '0', '9');
}

static bool isValidGrid(const QString &grid)
{
    const int size = grid.size();

    if ( size != 2 && size != 4 && size != 6 && size != 8 )
        return false;

    if ( !gridCharInRange(grid, 0, 'A', 'R')
         || !gridCharInRange(grid, 1, 'A', 'R') )
        return false;

    if ( size == 2 )
        return true;

    if ( !gridCharIsDigit(grid, 2)
         || !gridCharIsDigit(grid, 3) )
        return false;

    if ( size == 4 )
        return true;

    if ( !gridCharInRange(grid, 4, 'A', 'X')
         || !gridCharInRange(grid, 5, 'A', 'X') )
        return false;

    if ( size == 6 )
        return true;

    return gridCharIsDigit(grid, 6)
           && gridCharIsDigit(grid, 7);
}

Gridsquare::Gridsquare(const QString &in_grid)
{
    FCT_IDENTIFICATION;

    if ( !in_grid.isEmpty() )
    {
        grid = in_grid.toUpper();

        if ( isValidGrid(grid) )
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

Gridsquare::Gridsquare(double inlat, double inlon) :
    lat(inlat),
    lon(inlon)
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

Gridsquare Gridsquare::mapDisplayGrid(const QString &grid)
{
    FCT_IDENTIFICATION;

    const Gridsquare strictGrid(grid);

    return (strictGrid.isValid() || grid.size() <= 8) ? strictGrid
                                                      : Gridsquare(grid.left(8));
}

const QRegularExpression &Gridsquare::gridRegEx()
{
    FCT_IDENTIFICATION;

    static const QRegularExpression regex(QStringLiteral("^[A-Ra-r]{2}(?:[0-9]{2}|"
                                                         "[0-9]{2}[A-Xa-x]{2}|"
                                                         "[0-9]{2}[A-Xa-x]{2}[0-9]{2})?$"));
    return regex;
    //return QRegularExpression("^[A-Ra-r]{2}[0-9]{2}([A-Xa-x]{2})?([0-9]{2})?$");
}

const QRegularExpression &Gridsquare::gridVUCCRegEx()
{
    FCT_IDENTIFICATION;

    static const QRegularExpression regex(QStringLiteral("^[A-Ra-r]{2}(?:[0-9]{2}|"
                                                         "[0-9]{2}[A-Xa-x]{2}),[ ]*"
                                                         "[A-Ra-r]{2}(?:[0-9]{2}|"
                                                         "[0-9]{2}[A-Xa-x]{2})$|"
                                                         "^[A-Ra-r]{2}(?:[0-9]{2}|"
                                                         "[0-9]{2}[A-Xa-x]{2}),[ ]*"
                                                         "[A-Ra-r]{2}(?:[0-9]{2}|"
                                                         "[0-9]{2}[A-Xa-x]{2}),[ ]*"
                                                         "[A-Ra-r]{2}(?:[0-9]{2}|"
                                                         "[0-9]{2}[A-Xa-x]{2}),[ ]*"
                                                         "[A-Ra-r]{2}(?:[0-9]{2}|"
                                                         "[0-9]{2}[A-Xa-x]{2})$"));
    return regex;
}

const QRegularExpression &Gridsquare::gridExtRegEx()
{
    FCT_IDENTIFICATION;

    static const QRegularExpression regex(QStringLiteral("^[A-Xa-x]{2}?([0-9]{2})?$"));
    return regex;
}

double Gridsquare::distance2localeUnitDistance(double km,
                                               QString &unit,
                                               const LogLocale &locale)
{
    FCT_IDENTIFICATION;

    unit = QObject::tr("km");
    double ret = km;

    // All imperial systems
    if ( ! locale.getSettingUseMetric() )
    {
        unit = QObject::tr("miles");
        ret = km * localeDistanceCoef(locale);
    }
    return ret;
}

double Gridsquare::localeDistanceCoef(const LogLocale &locale)
{
    FCT_IDENTIFICATION;

    return ( ! locale.getSettingUseMetric() ) ? 0.6213711922
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
    const double dLat = (lat - this->lat) * DEG_TO_RAD;
    const double dLon = (lon - this->lon) * DEG_TO_RAD;

    const double lat1 = this->lat * DEG_TO_RAD;
    const double lat2 = lat * DEG_TO_RAD;

    const double sinDLat = sin(dLat / 2.0);
    const double sinDLon = sin(dLon / 2.0);
    const double a = sinDLat * sinDLat +
                     sinDLon * sinDLon * cos(lat1) * cos(lat2);

    const double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

    // Based on IARU Rules
    // The centre of the Large Locator Square (e.g. IO84MM to IO91MM) is used for distance calculations.
    // In order to make contest scores comparable, for the conversion from degrees to kilometres a factor
    // of 111.2 should be used when calculating distances with the aid of the spherical geometry equation.

    // It means that 111.2km/° * 360 =40032km
    // It means that R = 40032 / (2*PI)

    distance = IARU_EARTH_RADIUS_KM * c;

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

    const double dLon = (lon - this->lon) * DEG_TO_RAD;
    const double lat1 = this->lat * DEG_TO_RAD;
    const double lat2 = lat * DEG_TO_RAD;

    const double y = sin(dLon) * cos(lat2);
    const double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dLon);

    bearing = fmod((atan2(y, x) * RAD_TO_DEG + 360.0), 360.0);

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
