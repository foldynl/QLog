#ifndef QLOG_DATA_DXSPOT_H
#define QLOG_DATA_DXSPOT_H

#include <QtCore>
#include "Dxcc.h"
#include "core/MembershipQE.h"
#include "data/BandPlan.h"

class DxSpot
{

public:

    QDateTime dateTime;
    QString callsign;
    QList<ClubInfo> callsign_member;
    double freq;
    QString band;
    QString modeGroupString;
    BandPlan::BandPlanMode bandPlanMode;
    QString spotter;
    QString comment;
    qulonglong dupeCount = 0;
    QString wwffRef;
    QString potaRef;
    QString sotaRef;
    QString iotaRef;
    DxccEntity dxcc;
    DxccEntity dxcc_spotter;
    DxccStatus status;
    QString state;
    bool containsWWFF;
    bool containsPOTA;
    bool containsSOTA;
    bool containsIOTA;

    DxSpot() : freq(0.0),
        bandPlanMode(BandPlan::BAND_MODE_UNKNOWN),
        status(DxccStatus::UnknownStatus),
        containsWWFF(false), containsPOTA(false),
        containsSOTA(false), containsIOTA(false)
        {};

    QStringList memberList2StringList() const
    {
        QStringList ret;
        for ( const ClubInfo &member : static_cast<const QList<ClubInfo>&>(callsign_member) )
            ret << member.getClubInfo();
        return ret;
    };

    QSet<QString> memberList2Set() const
    {
        QSet<QString> ret;

        for ( const ClubInfo &member : static_cast<const QList<ClubInfo>&>(callsign_member) )
            ret << member.getClubInfo();
        return ret;
    }

    operator QString() const
    {
        return QString("DxSpot ")
               + "Country: " + QString::number(dxcc.dxcc) + " "
               + "CQZ: " + QString::number(dxcc.cqz) + " "
               + "ITUZ: " + QString::number(dxcc.ituz) + " "
               + "Status: " + QString::number(status) + " "
               + "ModeGroup: " + modeGroupString  + " "
               + "Band: " + band  + " "
               + "spotter Country: " + QString::number(dxcc_spotter.dxcc) + " "
               + "Continent: " + dxcc.cont + " "
               + "Spotter Continent: " + dxcc_spotter.cont + " "
               + "Callsign: " + callsign + " "
               + "Message: " + comment + " "
               + "DX Member: " + memberList2StringList().join(", ") + " "
               + "POTA: " + potaRef  + " " + "POTA present: " + (containsPOTA ? "true" : "false") + " "
               + "SOTA: " + sotaRef  + " " + "SOTA present: " + (containsSOTA ? "true" : "false") + " "
               + "WWFF: " + wwffRef + " " + "WWFF present: " + (containsWWFF ? "true" : "false") + " "
               + "IOTA: " + iotaRef + " " + "IOTA present: " + (containsIOTA ? "true" : "false") + " ";
    }
};

Q_DECLARE_METATYPE(DxSpot);

#endif // QLOG_DATA_DXSPOT_H
