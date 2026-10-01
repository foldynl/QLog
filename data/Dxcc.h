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

enum class AlertDxccGoal
{
    Worked = 0,
    Confirmed = 1
};

enum class AlertDxccContext
{
    Anywhere = 0,
    Band = 1,
    Mode = 2,
    BandMode = 3
};

enum class DxccConfirmationResult
{
    Error,
    NotConfirmed,
    Confirmed
};

enum DxccConfirmationSource
{
    NoConfirmationSource = 0,
    LotwConfirmation = 1,
    PaperConfirmation = 2,
    EqslConfirmation = 4
};
Q_DECLARE_FLAGS(DxccConfirmationSources, DxccConfirmationSource)
Q_DECLARE_OPERATORS_FOR_FLAGS(DxccConfirmationSources)

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
