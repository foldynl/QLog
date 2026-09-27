#ifndef QLOG_DATA_DXCC_H
#define QLOG_DATA_DXCC_H

#include <QtCore>

enum DxccStatus {
    NewEntity     = 0b1,
    NewBand       = 0b10,
    NewMode       = 0b100,
    NewBandMode   = 0b110,
    NewSlot       = 0b1000,
    Worked        = 0b10000,
    Confirmed     = 0b100000,
    UnknownStatus = 0b1000000,
    All           = 0b1111111
};

/* Granularity at which a DXCC status is evaluated */
enum class DxccStatusScope
{
    BandMode = 0,   // per band and mode group (default; the status shown in DXC, WSJTX and Bandmap)
    Band     = 1,   // per band, regardless of mode
    Entity   = 2,   // per entity, regardless of band and mode
    Mode     = 3    // per mode group, regardless of band
};

class DxccEntity {
public:
    QString country;
    QString prefix;
    qint32 dxcc;
    QString cont;
    qint32 cqz;
    qint32 ituz;
    double latlon[2];
    float tz;
    QString flag;
};

struct DxccPrefix {
public:
    QString prefix;
    bool exact;
    qint32 dxcc;
    qint32 cqz;
    qint32 ituz;
    QString cont;
    double latlon[2];
};

#endif // QLOG_DATA_DXCC_H
