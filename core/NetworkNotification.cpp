#include <QUuid>

#include "NetworkNotification.h"
#include "debug.h"
#include "LogParam.h"

MODULE_IDENTIFICATION("qlog.ui.networknotification");

NetworkNotification::NetworkNotification(QObject *parent)
    : QObject(parent)
{
    FCT_IDENTIFICATION;
}

QString NetworkNotification::getNotifQSOAdiAddrs()
{
    FCT_IDENTIFICATION;

    return LogParam::getNetworkNotifLogQSOAddrs();
}

void NetworkNotification::saveNotifQSOAdiAddrs(const QString &addresses)
{
    FCT_IDENTIFICATION;

    LogParam::setNetworkNotifLogQSOAddrs(addresses);
}

QString NetworkNotification::getNotifDXSpotAddrs()
{
    FCT_IDENTIFICATION;

    return LogParam::getNetworkNotifDXCSpotAddrs();
}

void NetworkNotification::saveNotifDXSpotAddrs(const QString &addresses)
{
    FCT_IDENTIFICATION;

    LogParam::setNetworkNotifDXCSpotAddrs(addresses);
}

QString NetworkNotification::getNotifWSJTXCQSpotAddrs()
{
    FCT_IDENTIFICATION;

    return LogParam::getNetworkNotifWSJTXCQSpotAddrs();
}

void NetworkNotification::saveNotifWSJTXCQSpotAddrs(const QString &addresses)
{
    FCT_IDENTIFICATION;

    LogParam::setNetworkNotifWSJTXCQSpotAddrs(addresses);
}

QString NetworkNotification::getNotifSpotAlertAddrs()
{
    FCT_IDENTIFICATION;

    return LogParam::getNetworkNotifAlertsSpotAddrs();
}

void NetworkNotification::saveNotifSpotAlertAddrs(const QString &addresses)
{
    FCT_IDENTIFICATION;

    LogParam::setNetworkNotifAlertsSpotAddrs(addresses);
}

QString NetworkNotification::getNotifRigStateAddrs()
{
    FCT_IDENTIFICATION;

    return LogParam::getNetworkNotifRigStateAddrs();
}

void NetworkNotification::saveNotifRigStateAddrs(const QString &addresses)
{
    FCT_IDENTIFICATION;

     LogParam::setNetworkNotifRigStateAddrs(addresses);
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

void NetworkNotification::rigStatus(const Rig::Status &status)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "Rig Status";

    HostsPortString destList(getNotifRigStateAddrs());

    if ( destList.getAddrList().size() > 0 )
    {
        RigStatusNotificationMsg rigStatusMsg(status);
        send(rigStatusMsg.getJson(), destList);
    }
}

void NetworkNotification::send(const QByteArray &data, const HostsPortString &dests)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << QString(data);

    if ( data.isEmpty() )
        return;

    const QList<HostPortAddress> &addrList = dests.getAddrList();

    for ( const HostPortAddress &addr : addrList )
    {
        qCDebug(runtime) << "Sending to " << addr;
        udpSocket.writeDatagram(data, addr, addr.getPort());
    }
}

GenericNotificationMsg::GenericNotificationMsg(QObject *parent) :
    QObject(parent)
{
    FCT_IDENTIFICATION;

    msg["appid"] = "QLog";
    msg["logid"] = LogParam::getLogID();
    msg["time"] = QDateTime::currentMSecsSinceEpoch();
}

/* QSO Notification Message
 * Example
 *
{
   "appid":"QLog",
   "data":{
      "operation":"insert",
      "rowid":355,
      "type":"adif",
      "value":"<call:7>OK1TEST<qso_date:8:D>20220320<time_on:6:T>183536<qso_date_off:8:D>20220320<time_off:6:T>183557<rst_rcvd:3>599<rst_sent:3>599<name:12>Testing Name<qth:6>Prague<gridsquare:6>JO70GB<cqz:2>15<ituz:2>28<freq:8:N>10.12649<band:3>30m<mode:2>CW<cont:2>EU<dxcc:3>503<country:14>Czech Republic<qsl_rcvd:1>N<qsl_sent:1>N<lotw_qsl_rcvd:1>N<lotw_qsl_sent:1>N<a_index:1>5<band_rx:3>30m<distance:17>9.266243887046823<eqsl_qsl_rcvd:1>N<eqsl_qsl_sent:1>N<freq_rx:8>10.12649<hrdlog_qso_upload_status:1>N<k_index:4>1.33<my_city:5>PRAHA<my_gridsquare:6>JO70GD<my_rig:9>moje_nove<operator:5>LADAS<sfi:2>94<station_callsign:6>OK1MLG<eor>"
   },
   "logid":"{2046e323-b340-4634-8d52-4e70a4231978}",
   "msgtype":"qso",
   "time":1647801358067
}
  * More info https://github.com/foldynl/QLog/wiki/Notifications
  */
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

/* DXC Spot Message
 * Example
 *
{
    "appid": "QLog",
    "data": {
        "band": "40m",
        "comment": "tnx qso",
        "dx": {
            "call": "YB0AR",
            "cont": "OC",
            "country": "Indonesia",
            "cqz": 28,
            "dxcc": 327,
            "ituz": 54,
            "member": ["LoTW", "eQSLAG"]
            "pfx": "YB",
            "utcoffset": -7
        },
        "freq": "7.1880",
        "mode": "PHONE",
        "rcvtime": "20220316 20:04:30",
        "spotter": {
            "call": "G0DEF",
            "cont": "EU",
            "country": "England",
            "cqz": 14,
            "dxcc": 223,
            "ituz": 27,
            "pfx": "G",
            "utcoffset": 0
        },
        "status": "newentity"
    },
    "logid":"{2046e323-b340-4634-8d52-4e70a4231978}",
    "msgtype": "dxspot",
    "time": 1647461070837
}
  * More info https://github.com/foldynl/QLog/wiki/Notifications
  */
DXSpotNotificationMsg::DXSpotNotificationMsg(const DxSpot &spot, QObject *parent) :
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

/* WSJTX Spot Message
 * Example
 *
{
   "appid":"QLog",
   "data":{
      "band":"80m",
      "comment":"CQ OK1MLG JO70",
      "dx":{
         "call":"OK1MLG",
         "cont":"EU",
         "country":"Europe",
         "cqz":15,
         "dxcc":503,
         "grid":"JO70",
         "ituz":28,
         "member":["LOTW", "eQSLAG"]
         "pfx":"OK",
         "utcoffset":-2
      },
      "freq":"3.5730",
      "mode":"FT8",
      "rcvtime":"20220318 17:04:29",
      "status":"newband"
   },
   "logid":"{2046e323-b340-4634-8d52-4e70a4231978}",
   "msgtype":"wsjtxcqspot",
   "time":1647623069705
}
  * More info https://github.com/foldynl/QLog/wiki/Notifications
  */
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

/* Spot Alert Message
 * Example
 *
{
   "appid":"QLog",
   "data":{
      "band":"17m",
      "comment":"CW     7 dB  25 WPM  CQ     ",
      "dx":{
         "call":"ZL3CW",
         "cont":"OC",
         "country":"New Zealand",
         "cqz":32,
         "dxcc":170,
         "ituz":60,
         "member": ["LOTW", "eQSLAG"]
         "pfx":"ZL",
         "utcoffset":-12
      },
      "freq":18.073,
      "mode":"CW",
      "rcvtime":"20220510 08:42:34",
      "rules":[
         "rule1",
         "rule2"
      ],
      "spotter":{
         "call":"SM6FMB",
         "cont":"EU",
         "country":"Sweden",
         "cqz":14,
         "dxcc":284,
         "ituz":18,
         "pfx":"SM",
         "utcoffset":-1
      },
      "status":"newentity"
   },
   "logid":"{2046e323-b340-4634-8d52-4e70a4231978}",
   "msgtype":"spotalert",
   "time":1652172154472
}
  * More info https://github.com/foldynl/QLog/wiki/Notifications
  */
SpotAlertNotificationMsg::SpotAlertNotificationMsg(const SpotAlert &alert, QObject *parent) :
    GenericSpotNotificationMsg(parent)
{
    FCT_IDENTIFICATION;

    QJsonObject spotData;
    spotData["rcvtime"] = alert.spot.dateTime.toString("yyyyMMdd hh:mm:ss");
    spotData["freq"] = qRound(alert.spot.freq * 10000.0) / 10000.0;
    spotData["band"] = alert.spot.band;
    spotData["mode"] = alert.spot.modeGroupString;
    spotData["comment"] = alert.spot.comment;
    spotData["status"] = DxccStatus2String.value(alert.spot.status, "unknown");
    spotData["rules"] = QJsonArray::fromStringList(alert.ruleNameList);

    QJsonObject dxInfo;
    dxInfo["call"] = alert.spot.callsign;
    dxInfo["country"] = alert.spot.dxcc.country;
    dxInfo["pfx"] = alert.spot.dxcc.prefix;
    dxInfo["dxcc"] = alert.spot.dxcc.dxcc;
    dxInfo["cont"] = alert.spot.dxcc.cont;
    dxInfo["cqz"] = alert.spot.dxcc.cqz;
    dxInfo["ituz"] = alert.spot.dxcc.ituz;
    dxInfo["utcoffset"] = alert.spot.dxcc.tz;
    dxInfo["member"] = QJsonArray::fromStringList(alert.spot.memberList2StringList());

    QJsonObject spotterInfo;
    spotterInfo["call"] = alert.spot.spotter;
    spotterInfo["country"] = alert.spot.dxcc_spotter.country;
    spotterInfo["pfx"] = alert.spot.dxcc_spotter.prefix;
    spotterInfo["dxcc"] = alert.spot.dxcc_spotter.dxcc;
    spotterInfo["cont"] = alert.spot.dxcc_spotter.cont;
    spotterInfo["cqz"] = alert.spot.dxcc_spotter.cqz;
    spotterInfo["ituz"] = alert.spot.dxcc_spotter.ituz;
    spotterInfo["utcoffset"] = alert.spot.dxcc_spotter.tz;

    spotData["spotter"] = spotterInfo;
    spotData["dx"] = dxInfo;

    msg["msgtype"] = "spotalert";
    msg["data"] = spotData;

}

/* WCY Spot Message
 * Example
 *
{
   "appid":"QLog",
   "data":{
      "A":13,
      "Au":"no",
      "GMF":"act",
      "K":3,
      "R":0,
      "SA":"qui",
      "SFI":68,
      "expK":3,
      "rcvtime":"20220923 11:29:16"
   },
   "logid":"{c804ab21-c1bf-4b7f-90a6-8927bdb10dd0}",
   "msgtype":"wcyspot",
   "time":1663932556260
}
  * More info https://github.com/foldynl/QLog/wiki/Notifications
  */
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

/* WWVSpot Message
 * Example
 *
{
   "appid":"QLog",
   "data":{
      "A":12,
      "Info1":"No Storms",
      "Info2":"No Storms",
      "K":2,
      "SFI":68,
      "rcvtime":"20220923 11:29:16"
   },
   "logid":"{c804ab21-c1bf-4b7f-90a6-8927bdb10dd0}",
   "msgtype":"wwvspot",
   "time":1663932556294
}
  * More info https://github.com/foldynl/QLog/wiki/Notifications
  */
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

/* ToAllSpot Message
 * Example
 *
{
   "appid":"QLog",
   "data":{
      "message":"GB7RDX New Users Welcome. cluster.g3ldi.co.uk Port 7000",
      "rcvtime":"20220923 11:29:16",
      "spotter":{
         "call":"G3LDI",
         "cont":"EU",
         "country":"England",
         "cqz":14,
         "dxcc":223,
         "ituz":27,
         "pfx":"G",
         "utcoffset":0
      }
   },
   "logid":"{c804ab21-c1bf-4b7f-90a6-8927bdb10dd0}",
   "msgtype":"toallspot",
   "time":1663932556327
}
  * More info https://github.com/foldynl/QLog/wiki/Notifications
  */
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

/* Rig Status Message
 * Example
 *
{
   "appid":"QLog",
   "msgtype":"rigstatus",
   "time":1647197319251,
   "data":{
             "profile" : "IC7300",
             "connected" : true,
             "txvfo" : "CURR",
             "rxvfo" : "CURR",                -- TX/RX VFOs have the same value - split is not supported
             "txpower" : 0.01                 <in W>
             "keyspeed" : 23
             "vfostates" :
              [
                  {
                     "vfo" : "CURR",
                     "freq" : "7.18800"       <in MHz always 5 dec. places>
                     "mode" : "FM",
                     "rawmode" : "FM",        <raw mode from Rig>
                     "ptt" : false,
                     "rit" : 0.1              <in MHz>
                     "xit" : 0.0              <in MHz>
                     "bandwidth" : 2400       <in Hz>
                  },
                  {
                     ....                     -- currently, the second VFO is not present
                  }
              ]
          },
   "logid":"{2046e323-b340-4634-8d52-4e70a4231978}"
}
  * Empty/Zero fields are not sent. Sent is everything that is known at the given moment of event.
  * More info https://github.com/foldynl/QLog/wiki/Notifications
*/
RigStatusNotificationMsg::RigStatusNotificationMsg(const Rig::Status &status, QObject *parent) :
    GenericNotificationMsg(parent)
{
    FCT_IDENTIFICATION;

    QJsonObject vfoState;

    auto addIfNoEmpty = [&](const QString &key,
                            const QString &value,
                            bool addCond = true)
    {
        if ( !value.isEmpty() && addCond)
            vfoState[key] = value;
    };

    auto addBoolIfNoEmpty = [&](const QString &key,
                            const qint8 &value,
                            bool addCond = true)
    {
        if ( addCond )
            vfoState[key] = ( value != 0 );
    };

    auto addDoubleIfNoEmpty = [&](const QString &key,
                                const double &value,
                                bool addCond = true)
    {
        if ( value != 0.0 && addCond )
            vfoState[key] = value;
    };

    addIfNoEmpty("vfo", status.vfo);
    addIfNoEmpty("freq",  QString::number(status.freq, 'f', 5), status.freq != 0.0);
    addIfNoEmpty("mode", status.mode);
    addIfNoEmpty("submode", status.submode);
    addIfNoEmpty("rawmode", status.rawmode);
    addBoolIfNoEmpty("ptt", status.ptt, status.ptt != -1);
    addDoubleIfNoEmpty("rit", status.rit);
    addDoubleIfNoEmpty("xit", status.xit);
    addDoubleIfNoEmpty("bandwidth", status.bandwidth);

    QJsonArray vfoStates;
    vfoStates.append(vfoState);

    QJsonObject rigData;
    rigData["profile"] = status.profile;
    rigData["connected"] = status.isConnected;

    if ( !status.vfo.isEmpty() )
    {
        rigData["txvfo"] = status.vfo;
        rigData["rxvfo"] = status.vfo; // split mode is not supported yet
    }

    if ( status.power > 0.0 )
        rigData["txpower"] = status.power;

    if ( vfoState.size() > 0 )
        rigData["vfostates"] = vfoStates;

    if ( status.keySpeed > 0 )
        rigData["keyspeed"] = status.keySpeed;

    msg["msgtype"] = "rigstatus";
    msg["data"] = rigData;
}
