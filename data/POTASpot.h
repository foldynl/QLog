#ifndef QLOG_DATA_POTASPOT_H
#define QLOG_DATA_POTASPOT_H

#include <QtCore>

class POTASpot
{
public:
    quint64 spotId = 0;
    QString activator;
    QString activatorBaseCallsign;
    double frequency = 0.0;
    QString mode;
    QString reference;
    QString parkName;
    QDateTime spotTime;
    QString spotter;
    QString comments;
    QString source;
    QString name;
    QString locationDesc;
};

#endif // QLOG_DATA_POTASPOT_H
