#ifndef QLOG_DATA_SPOTALERT_H
#define QLOG_DATA_SPOTALERT_H

#include <QString>
#include <QDateTime>
#include <QMetaType>
#include "Dxcc.h"
#include "core/MembershipQE.h"
#include "data/BandPlan.h"
#include "core/Wsjtx.h"

struct SpotAlert
{
    enum ALERTSOURCETYPE
    {
        DXSPOT = 0b1,
        WSJTXCQSPOT = 0b10
    };

    QDateTime dateTime;
    ALERTSOURCETYPE source;
    QStringList ruleName;
    QString callsign;
    double freq;
    QString band;
    QList<ClubInfo> callsign_member;
    QString modeGroupString;
    BandPlan::BandPlanMode bandPlanMode;
    DxccEntity dxcc;
    DxccStatus status;
    QString comment;
    QString spotter;
    DxccEntity dxcc_spotter;
    WsjtxDecode wsjtxDecode;

    QStringList memberList2StringList() const
    {
        QStringList ret;
        for ( const ClubInfo &member : static_cast<const QList<ClubInfo>&>(callsign_member) )
        {
            ret << member.getClubInfo();
        }
        return ret;
    };
};

Q_DECLARE_METATYPE(SpotAlert::ALERTSOURCETYPE);

#endif // QLOG_DATA_SPOTALERT_H
