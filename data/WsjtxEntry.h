#ifndef QLOG_DATA_WSJTXENTRY_H
#define QLOG_DATA_WSJTXENTRY_H

#include "core/Wsjtx.h"
#include "core/MembershipQE.h"
#include "data/Dxcc.h"

struct WsjtxEntry {
    WsjtxDecode decode;
    DxccEntity dxcc;
    DxccStatus status;
    QString callsign;
    QList<ClubInfo> callsign_member;
    QString grid;
    double distance;
    QDateTime receivedTime;
    double freq;
    QString band;
    QString decodedMode;
    QString spotter;
    DxccEntity dxcc_spotter;

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

#endif // QLOG_DATA_WSJTXENTRY_H
