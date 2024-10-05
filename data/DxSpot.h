#ifndef QLOG_DATA_DXSPOT_H
#define QLOG_DATA_DXSPOT_H

#include <QtCore>
#include "Dxcc.h"
#include "core/MembershipQE.h"
#include "data/BandPlan.h"

class DxSpot
{
public:
    QDateTime time;
    QString callsign;
    QList<ClubInfo> callsign_member;
    double freq;
    QString band;
    QString modeGroupString;
    BandPlan::BandPlanMode bandPlanMode;
    QString spotter;
    QString comment;
    DxccEntity dxcc;
    DxccEntity dxcc_spotter;
    DxccStatus status;

    QStringList memberList2StringList() const
    {
        QStringList ret;
        for ( const ClubInfo &member : static_cast<const QList<ClubInfo>&>(callsign_member) )
        {
            ret << member.getClubInfo();
        }
        return ret;
    };

    QSet<QString> memberList2Set() const
    {
        QSet<QString> ret;

        for ( const ClubInfo &member : static_cast<const QList<ClubInfo>&>(callsign_member) )
        {
            ret << member.getClubInfo();
        }
        return ret;
    }
};

Q_DECLARE_METATYPE(DxSpot);

#endif // QLOG_DATA_DXSPOT_H
