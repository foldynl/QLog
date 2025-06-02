#ifndef QLOG_DATA_WSJTXENTRY_H
#define QLOG_DATA_WSJTXENTRY_H

#include "core/WsjtxUDPReceiver.h"
#include "data/DxSpot.h"

class WsjtxEntry : public DxSpot
{

public:

    WsjtxDecode decode;
    QString grid;
    double distance;
    QDateTime receivedTime;
    QString decodedMode;

    WsjtxEntry() : DxSpot(), distance(0.0){};

    WsjtxEntry(const DxSpot &other) :
        DxSpot(other), distance(0.0) {};

    operator QString() const
    {
        return QString("WsjtxEntry ")
        + "Country: " + QString::number(dxcc.dxcc) + " "
        + "CQZ" + QString::number(dxcc.cqz) + " "
        + "ITUZ" + QString::number(dxcc.ituz) + " "
        + "Status: " + QString::number(status) + " "
        + "Band: " + band + " "
        + "ModeGroup: " + ((decodedMode == "FT8") ? BandPlan::MODE_GROUP_STRING_FT8
                                                  : BandPlan::MODE_GROUP_STRING_DIGITAL )  + " "
        + "spotter Country: " + QString::number(dxcc_spotter.dxcc)  + " "
        + "Continent: " + dxcc.cont  + " "
        + "Spotter Continent: " + dxcc_spotter.cont  + " "
        + "Callsign: " + callsign  + " "
        + "Message: " + decode.message  + " "
        + "DX Member: " + memberList2StringList().join(", ");
    };
};

#endif // QLOG_DATA_WSJTXENTRY_H
