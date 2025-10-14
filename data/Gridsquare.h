#ifndef QLOG_CORE_GRIDSQUARE_H
#define QLOG_CORE_GRIDSQUARE_H

#include <QObject>
#include <QString>
#include <QDebug>

#include "core/LogLocale.h"

class Gridsquare
{
public:
    explicit Gridsquare(const QString &in_grid = QString());
    explicit Gridsquare(const double inlat,const  double inlon);
    ~Gridsquare() {};
    static const QRegularExpression gridRegEx();
    static const QRegularExpression gridVUCCRegEx();
    static const QRegularExpression gridExtRegEx();
    static double distance2localeUnitDistance(double km, QString &unit, const LogLocale &locale);
    static double localeDistanceCoef(const LogLocale &locale);

    bool isValid() const;
    double getLongitude() const {return lon;};
    double getLatitude() const {return lat;};
    const QString getGrid() const { return grid;};
    bool distanceTo(const Gridsquare &in_grid, double &distance) const;
    bool distanceTo(double lat, double lon, double &distance) const;
    bool bearingTo(const Gridsquare &in_grid, double &bearing) const;
    bool bearingTo(double lat, double lon, double &bearing) const ;
    operator QString() const { return QString("Gridsquare: grid[%1]; valid[%2]; lat[%3]; lon[%4]")
                                      .arg(grid).arg(validGrid).arg(lat).arg(lon);};

private:
    QString grid;
    bool validGrid;
    double lat, lon;
};

#endif // QLOG_CORE_GRIDSQUARE_H
