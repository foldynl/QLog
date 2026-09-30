#ifndef QLOG_DATA_WSJTXENTRY_H
#define QLOG_DATA_WSJTXENTRY_H

#include "data/WsjtxDecode.h"
#include "data/DxSpot.h"

class WsjtxEntry : public DxSpot
{

public:

    WsjtxDecode decode;
    // Reply must echo the original CQ/QRZ decode even after the row receives later traffic.
    WsjtxDecode replyDecode;
    QString grid;
    double distance;
    float spotPeriod;
    QDateTime receivedTime;
    QString decodedMode;

    WsjtxEntry() : DxSpot(), distance(0.0), spotPeriod(120.0f){};

    WsjtxEntry(const DxSpot &other) :
        DxSpot(other), distance(0.0), spotPeriod(120.0f) {};

    operator QString() const
    {
        return QString("WsjtxEntry ")
        + "Country: " + QString::number(dxcc.dxcc) + " "
        + "CQZ" + QString::number(dxcc.cqz) + " "
        + "ITUZ" + QString::number(dxcc.ituz) + " "
        + "Status: " + QString::number(status) + " "
        + "Band: " + band + " "
        + "ModeGroup: " + (BandPlan::isFTxMode(decodedMode) ? BandPlan::MODE_GROUP_STRING_FTx
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
