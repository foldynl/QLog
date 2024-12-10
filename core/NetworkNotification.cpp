#include <QUuid>
#include <QUdpSocket>

#include "NetworkNotification.h"
#include "debug.h"
#include "LogParam.h"
#include "data/Data.h"

MODULE_IDENTIFICATION("qlog.ui.networknotification");

NetworkNotification::NetworkNotification(QObject *parent)
    : QObject(parent)
{
    FCT_IDENTIFICATION;
}

QString NetworkNotification::getNotifQSOAdiAddrs()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    return settings.value(NetworkNotification::CONFIG_NOTIF_QSO_ADI_ADDRS_KEY).toString();
}

void NetworkNotification::saveNotifQSOAdiAddrs(const QString &addresses)
{
    FCT_IDENTIFICATION;

    QSettings settings;

    settings.setValue(NetworkNotification::CONFIG_NOTIF_QSO_ADI_ADDRS_KEY, addresses);
}

QString NetworkNotification::getNotifDXSpotAddrs()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    return settings.value(NetworkNotification::CONFIG_NOTIF_DXSPOT_ADDRS_KEY).toString();
}

void NetworkNotification::saveNotifDXSpotAddrs(const QString &addresses)
{
    QSettings settings;

    settings.setValue(NetworkNotification::CONFIG_NOTIF_DXSPOT_ADDRS_KEY, addresses);
}

QString NetworkNotification::getNotifWSJTXCQSpotAddrs()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    return settings.value(NetworkNotification::CONFIG_NOTIF_WSJTXCQSPOT_ADDRS_KEY).toString();
}

void NetworkNotification::saveNotifWSJTXCQSpotAddrs(const QString &addresses)
{
    FCT_IDENTIFICATION;

    QSettings settings;

    settings.setValue(NetworkNotification::CONFIG_NOTIF_WSJTXCQSPOT_ADDRS_KEY, addresses);
}

QString NetworkNotification::getNotifSpotAlertAddrs()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    return settings.value(NetworkNotification::CONFIG_NOTIF_SPOTALERT_ADDRS_KEY).toString();
}

void NetworkNotification::saveNotifSpotAlertAddrs(const QString &addresses)
{
    FCT_IDENTIFICATION;

    QSettings settings;

    settings.setValue(NetworkNotification::CONFIG_NOTIF_SPOTALERT_ADDRS_KEY, addresses);

}

void NetworkNotification::QSOInserted(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "Inserted: " << record;

    HostsPortString destList(getNotifQSOAdiAddrs());

    if ( destList.getAddrList().size() > 0 )
    {
        QSONotificationMsg qsoMsg(record, QSONotificationMsg::QSO_INSERT);
        send(qsoMsg.getJson(), destList);
    }
}

void NetworkNotification::QSOUpdated(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << "Updated: " << record;

    HostsPortString destList(getNotifQSOAdiAddrs());

    if ( destList.getAddrList().size() > 0 )
    {
        QSONotificationMsg qsoMsg(record, QSONotificationMsg::QSO_UPDATE);
        send(qsoMsg.getJson(), destList);
    }
}

void NetworkNotification::QSODeleted(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << "Deleted: " << record;

    HostsPortString destList(getNotifQSOAdiAddrs());

    if ( destList.getAddrList().size() > 0 )
    {
        QSONotificationMsg qsoMsg(record, QSONotificationMsg::QSO_DELETE);
        send(qsoMsg.getJson(), destList);
    }
}

void NetworkNotification::dxSpot(const DxSpot &spot)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "DX Spot";

    HostsPortString destList(getNotifDXSpotAddrs());

    if ( destList.getAddrList().size() > 0 )
    {
        DXSpotNotificationMsg dxSpotMsg(spot);
        send(dxSpotMsg.getJson(), destList);
    }
}

void NetworkNotification::wcySpot(const WCYSpot &spot)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << "WCY Spot";

    HostsPortString destList(getNotifDXSpotAddrs());

    if ( destList.getAddrList().size() > 0 )
    {
        WCYSpotNotificationMsg WCYSpotMsg(spot);
        send(WCYSpotMsg.getJson(), destList);
    }
}

void NetworkNotification::wwvSpot(const WWVSpot &spot)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << "WWV Spot";

    HostsPortString destList(getNotifDXSpotAddrs());

    if ( destList.getAddrList().size() > 0 )
    {
        WWVSpotNotificationMsg WWVSpotMsg(spot);
        send(WWVSpotMsg.getJson(), destList);
    }
}

void NetworkNotification::toAllSpot(const ToAllSpot &spot)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << "ToALL Spot";

    HostsPortString destList(getNotifDXSpotAddrs());

    if ( destList.getAddrList().size() > 0 )
    {
        ToAllSpotNotificationMsg ToAllSpotMsg(spot);
        send(ToAllSpotMsg.getJson(), destList);
    }
}

void NetworkNotification::WSJTXCQSpot(const WsjtxEntry &spot)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "WSJTX CQ Spot";

    HostsPortString destList(getNotifWSJTXCQSpotAddrs());

    if ( destList.getAddrList().size() > 0 )
    {
        WSJTXCQSpotNotificationMsg dxSpotMsg(spot);
        send(dxSpotMsg.getJson(), destList);
    }
}

void NetworkNotification::spotAlert(const SpotAlert &spot)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "Usert Alert";

    HostsPortString destList(getNotifSpotAlertAddrs());

    if ( destList.getAddrList().size() > 0 )
    {
        SpotAlertNotificationMsg spotAlertMsg(spot);
        send(spotAlertMsg.getJson(), destList);
    }
}

void NetworkNotification::send(const QByteArray &data, const HostsPortString &dests)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << QString(data);

    if ( data.size() <= 0 )
    {
        return;
    }

    const QList<HostPortAddress> &addrList = dests.getAddrList();

    for ( const HostPortAddress &addr : addrList )
    {
        QUdpSocket udpSocket;

        qCDebug(runtime) << "Sending to " << addr;
        udpSocket.writeDatagram(data, addr, addr.getPort());
    }
}

QString NetworkNotification::CONFIG_NOTIF_QSO_ADI_ADDRS_KEY = "network/notification/qso/adi_addrs";
QString NetworkNotification::CONFIG_NOTIF_DXSPOT_ADDRS_KEY = "network/notification/dxspot/addrs";
QString NetworkNotification::CONFIG_NOTIF_WSJTXCQSPOT_ADDRS_KEY = "network/notification/wsjtx/cqspot/addrs";
QString NetworkNotification::CONFIG_NOTIF_SPOTALERT_ADDRS_KEY = "network/notification/alerts/spot/addrs";

GenericNotificationMsg::GenericNotificationMsg(QObject *parent) :
    QObject(parent)
{
    FCT_IDENTIFICATION;

    msg["appid"] = "QLog";
    msg["logid"] = LogParam::getParam("logid").toString();
    msg["time"] = QDateTime::currentMSecsSinceEpoch();
}

QSONotificationMsg::QSONotificationMsg(const QSqlRecord &record,
                                       const QSOOperation operation,
                                       QObject *parent) :
    GenericNotificationMsg(parent)
{
    FCT_IDENTIFICATION;

    QString data;
    QTextStream stream(&data, QIODevice::ReadWrite);

    /* currently only ADIF is supported */
    /* ADX and JSON do not support export single record */
    LogFormat* format = LogFormat::open(LogFormat::ADI, stream);

    if (!format) {
        qWarning() << "cannot created ADIF Export Formatter";
        return;
    }

    //format->exportStart();
    format->exportContact(record);
    stream.flush();
    //format->exportEnd();

    QJsonObject qsoData;
    qsoData["operation"] = QSOOperation2String.value(operation,"unknown");
    qsoData["type"] = "adif";
    qsoData["rowid"] = record.value(record.indexOf("id")).toInt();
    qsoData["value"] = data.replace("\n", "");

    msg["msgtype"] = "qso";
    msg["data"] = qsoData;

    delete format;
}

DXSpotNotificationMsg::DXSpotNotificationMsg(const DxSpot &spot, QObject *parent) :
    GenericSpotNotificationMsg(parent)
{
    FCT_IDENTIFICATION;

    QJsonObject spotData;
    spotData["rcvtime"] = spot.time.toString("yyyyMMdd hh:mm:ss");
    spotData["freq"] = qRound(spot.freq * 10000.0) / 10000.0;
    spotData["band"] = spot.band;
    spotData["mode"] = spot.modeGroupString; 
    spotData["comment"] = spot.comment;
    spotData["status"] = DxccStatus2String.value(spot.status, "unknown");

    QJsonObject dxInfo;
    dxInfo["call"] = spot.callsign;
    dxInfo["country"] = spot.dxcc.country;
    dxInfo["pfx"] = spot.dxcc.prefix;
    dxInfo["dxcc"] = spot.dxcc.dxcc;
    dxInfo["cont"] = spot.dxcc.cont;
    dxInfo["cqz"] = spot.dxcc.cqz;
    dxInfo["ituz"] = spot.dxcc.ituz;
    dxInfo["utcoffset"] = spot.dxcc.tz;
    dxInfo["member"] = QJsonArray::fromStringList(spot.memberList2StringList());

    QJsonObject spotterInfo;
    spotterInfo["call"] = spot.spotter;
    spotterInfo["country"] = spot.dxcc_spotter.country;
    spotterInfo["pfx"] = spot.dxcc_spotter.prefix;
    spotterInfo["dxcc"] = spot.dxcc_spotter.dxcc;
    spotterInfo["cont"] = spot.dxcc_spotter.cont;
    spotterInfo["cqz"] = spot.dxcc_spotter.cqz;
    spotterInfo["ituz"] = spot.dxcc_spotter.ituz;
    spotterInfo["utcoffset"] = spot.dxcc_spotter.tz;

    spotData["spotter"] = spotterInfo;
    spotData["dx"] = dxInfo;

    msg["msgtype"] = "dxspot";
    msg["data"] = spotData;
}

WSJTXCQSpotNotificationMsg::WSJTXCQSpotNotificationMsg(const WsjtxEntry &spot, QObject *parent) :
    GenericSpotNotificationMsg(parent)
{
    FCT_IDENTIFICATION;

    QJsonObject spotData;
    spotData["rcvtime"] = spot.receivedTime.toString("yyyyMMdd hh:mm:ss");
    spotData["freq"] = qRound(spot.freq * 10000.0) / 10000.0;
    spotData["band"] = spot.band;
    spotData["mode"] = spot.decodedMode;
    spotData["comment"] = spot.decode.message;
    spotData["status"] = DxccStatus2String.value(spot.status, "unknown");

    QJsonObject dxInfo;
    dxInfo["call"] = spot.callsign;
    dxInfo["country"] = spot.dxcc.country;
    dxInfo["pfx"] = spot.dxcc.prefix;
    dxInfo["dxcc"] = spot.dxcc.dxcc;
    dxInfo["cont"] = spot.dxcc.cont;
    dxInfo["cqz"] = spot.dxcc.cqz;
    dxInfo["ituz"] = spot.dxcc.ituz;
    dxInfo["utcoffset"] = spot.dxcc.tz;
    dxInfo["grid"] = spot.grid;
    dxInfo["member"] = QJsonArray::fromStringList(spot.memberList2StringList());

    spotData["dx"] = dxInfo;

    msg["msgtype"] = "wsjtxcqspot";
    msg["data"] = spotData;
}

GenericSpotNotificationMsg::GenericSpotNotificationMsg(QObject *parent)
    : GenericNotificationMsg(parent)
{
    FCT_IDENTIFICATION;
}

SpotAlertNotificationMsg::SpotAlertNotificationMsg(const SpotAlert &spot, QObject *parent) :
    GenericSpotNotificationMsg(parent)
{
    FCT_IDENTIFICATION;

    QJsonObject spotData;
    spotData["rcvtime"] = spot.dateTime.toString("yyyyMMdd hh:mm:ss");
    spotData["freq"] = qRound(spot.freq * 10000.0) / 10000.0;
    spotData["band"] = spot.band;
    spotData["mode"] = spot.modeGroupString;
    spotData["comment"] = spot.comment;
    spotData["status"] = DxccStatus2String.value(spot.status, "unknown");
    spotData["rules"] = QJsonArray::fromStringList(spot.ruleName);

    QJsonObject dxInfo;
    dxInfo["call"] = spot.callsign;
    dxInfo["country"] = spot.dxcc.country;
    dxInfo["pfx"] = spot.dxcc.prefix;
    dxInfo["dxcc"] = spot.dxcc.dxcc;
    dxInfo["cont"] = spot.dxcc.cont;
    dxInfo["cqz"] = spot.dxcc.cqz;
    dxInfo["ituz"] = spot.dxcc.ituz;
    dxInfo["utcoffset"] = spot.dxcc.tz;
    dxInfo["member"] = QJsonArray::fromStringList(spot.memberList2StringList());

    QJsonObject spotterInfo;
    spotterInfo["call"] = spot.spotter;
    spotterInfo["country"] = spot.dxcc_spotter.country;
    spotterInfo["pfx"] = spot.dxcc_spotter.prefix;
    spotterInfo["dxcc"] = spot.dxcc_spotter.dxcc;
    spotterInfo["cont"] = spot.dxcc_spotter.cont;
    spotterInfo["cqz"] = spot.dxcc_spotter.cqz;
    spotterInfo["ituz"] = spot.dxcc_spotter.ituz;
    spotterInfo["utcoffset"] = spot.dxcc_spotter.tz;

    spotData["spotter"] = spotterInfo;
    spotData["dx"] = dxInfo;

    msg["msgtype"] = "spotalert";
    msg["data"] = spotData;

}

WCYSpotNotificationMsg::WCYSpotNotificationMsg(const WCYSpot &spot, QObject *parent) :
    GenericNotificationMsg(parent)
{
    FCT_IDENTIFICATION;

    QJsonObject spotData;
    spotData["rcvtime"] = spot.time.toString("yyyyMMdd hh:mm:ss");
    spotData["K"] = spot.KIndex;
    spotData["expK"] = spot.expK;
    spotData["A"] = spot.AIndex;
    spotData["R"] = spot.RIndex;
    spotData["SFI"] = spot.SFI;
    spotData["SA"] = spot.SA;
    spotData["GMF"] = spot.GMF;
    spotData["Au"] = spot.Au;

    msg["msgtype"] = "wcyspot";
    msg["data"] = spotData;
}

WWVSpotNotificationMsg::WWVSpotNotificationMsg(const WWVSpot &spot, QObject *parent) :
    GenericNotificationMsg(parent)
{
    FCT_IDENTIFICATION;

    QJsonObject spotData;
    spotData["rcvtime"] = spot.time.toString("yyyyMMdd hh:mm:ss");
    spotData["SFI"] = spot.SFI;
    spotData["A"] = spot.AIndex;
    spotData["K"] = spot.KIndex;
    spotData["Info1"] = spot.info1;
    spotData["Info2"] = spot.info2;

    msg["msgtype"] = "wwvspot";
    msg["data"] = spotData;
}

ToAllSpotNotificationMsg::ToAllSpotNotificationMsg(const ToAllSpot &spot, QObject *parent) :
    GenericNotificationMsg(parent)
{
    FCT_IDENTIFICATION;

    QJsonObject spotData;
    spotData["rcvtime"] = spot.time.toString("yyyyMMdd hh:mm:ss");
    spotData["message"] = spot.message;

    QJsonObject spotterInfo;
    spotterInfo["call"] = spot.spotter;
    spotterInfo["country"] = spot.dxcc_spotter.country;
    spotterInfo["pfx"] = spot.dxcc_spotter.prefix;
    spotterInfo["dxcc"] = spot.dxcc_spotter.dxcc;
    spotterInfo["cont"] = spot.dxcc_spotter.cont;
    spotterInfo["cqz"] = spot.dxcc_spotter.cqz;
    spotterInfo["ituz"] = spot.dxcc_spotter.ituz;
    spotterInfo["utcoffset"] = spot.dxcc_spotter.tz;

    spotData["spotter"] = spotterInfo;

    msg["msgtype"] = "toallspot";
    msg["data"] = spotData;
}

void NetworkNotification::rigConnected()
{
    FCT_IDENTIFICATION;
    rigOnline = true;
}

void NetworkNotification::rigDisconnected()
{
    FCT_IDENTIFICATION;
    rigOnline = false;
    resetRigInfo();
}

void NetworkNotification::resetRigInfo()
{
    FCT_IDENTIFICATION;
    lastSeenFreq = 0;
    lastSeenMode = "";
    lastSeenSubMode = 0;
    lastSeenPWR = 0;
    lastSeenRIT = 0;
    lastSeenXIT = 0;
    lastSeenPTT = "RX";
    rigOnline = false;
}

void NetworkNotification::updateFrequency(VFOID vfoid, double vfoFreq, double ritFreq, double xitFreq)
{
    FCT_IDENTIFICATION;

    Q_UNUSED(vfoid)

    qCDebug(function_parameters) << vfoFreq << ritFreq << xitFreq;

    const QString& bandName = BandPlan::freq2Band(vfoFreq).name;
    lastSeenFreq = vfoFreq;
    updateRadio();
}

void NetworkNotification::updateMode(VFOID vfoid, const QString &rawMode, const QString &mode,
                           const QString &submode, qint32)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<mode;

    Q_UNUSED(submode)
    Q_UNUSED(vfoid)

    lastSeenMode = mode;
    lastSeenSubMode = submode;
    updateRadio();

}

void NetworkNotification::updatePWR(VFOID vfoid, double pwr)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<pwr;

    Q_UNUSED(vfoid)

    lastSeenPWR = QString(tr("PWR: %1W")).arg(pwr);
    updateRadio();

}

void NetworkNotification::updateXIT(VFOID, double xit)
{
    FCT_IDENTIFICATION;

    if ( xit != 0.0 )
    {
        QString unit;
        unsigned char decP;
        double xitDisplay = Data::MHz2UserFriendlyFreq(xit, unit, decP);
        lastSeenXIT = QString("XIT: %1 %2").arg(QString::number(xitDisplay, 'f', decP),
                                                         unit);
    }
    else
    {
        lastSeenXIT = "0";
    }
    updateRadio();

}

void NetworkNotification::updateRIT(VFOID, double rit)
{
    FCT_IDENTIFICATION;

    if ( rit != 0.0 )
    {
        QString unit;
        unsigned char decP;
        double ritDisplay = Data::MHz2UserFriendlyFreq(rit, unit, decP);
        lastSeenRIT = QString("RIT: %1 %2").arg(QString::number(ritDisplay, 'f', decP),
                                                         unit);
    }
    else
    {
        lastSeenRIT = "0";
    }
    updateRadio();

}

void NetworkNotification::updatePTT(VFOID, bool ptt)
{
    FCT_IDENTIFICATION;

    lastSeenPTT = (ptt)? "TX" : "RX";
    updateRadio();

}

void NetworkNotification::updateVFO(VFOID vfoid, const QString &vfo)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<vfo;

    Q_UNUSED(vfoid)

    lastSeenVFO = vfo;
    updateRadio();

}

void NetworkNotification::rigUpdated()
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "Rig Updated";

    HostsPortString destList(getNotifDXSpotAddrs());

    if ( destList.getAddrList().size() > 0 )
    {
        GenericNotificationMsg RigNotificationMsg;
        send(RigNotificationMsg.getJson(), destList);
    }

}


QString NetworkNotification::getNotifRadioAlertAddrs()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    return settings.value(NetworkNotification::CONFIG_NOTIF_RADIO_ADDRS_KEY).toString();
}

void NetworkNotification::saveNotifRadioAlertAddrs(const QString &addresses)
{
    FCT_IDENTIFICATION;

    QSettings settings;

    settings.setValue(NetworkNotification::CONFIG_NOTIF_RADIO_ADDRS_KEY, addresses);

}

QString NetworkNotification::CONFIG_NOTIF_RADIO_ADDRS_KEY = "network/notification/alerts/freq/addrs";

void NetworkNotification::updateRadio()
{
    FCT_IDENTIFICATION;

     HostsPortString destList(getNotifRadioAlertAddrs());

    if ( destList.getAddrList().size() > 0 )
    {
        const QString& bandName = BandPlan::freq2Band(lastSeenFreq).name;
         RadioNotificationMsg radioAlertMsg(lastSeenFreq, bandName, lastSeenMode,lastSeenSubMode, lastSeenPTT, lastSeenPWR, lastSeenRIT, lastSeenXIT,lastSeenVFO );
        send(radioAlertMsg.getJson(), destList);
    }
}

RadioNotificationMsg::RadioNotificationMsg(double lastSeenFreq, QString bandName,QString  lastSeenMode,QString lastSeenSubMode,QString  lastSeenPTT,QString  lastSeenPWR,QString  lastSeenRIT,QString  lastSeenXIT,QString lastSeenVFO,QObject *parent) :
    GenericNotificationMsg(parent)
{
    FCT_IDENTIFICATION;

    QJsonObject rigData;
    rigData["freq"] = lastSeenFreq;
    rigData["band"] = BandPlan::freq2Band(lastSeenFreq).name;
    rigData["mode"] = lastSeenMode;
    rigData["submode"] =lastSeenSubMode ;
    rigData["ptt"] = lastSeenPTT;
    rigData["power"] = lastSeenPWR;
    rigData["RIT"] = lastSeenRIT;
    rigData["XIT"] = lastSeenXIT;
    rigData["VFO"] = lastSeenVFO;

    msg["msgtype"] = "riginfo";
    msg["data"] = rigData;
}
