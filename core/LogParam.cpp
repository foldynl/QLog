#include <QSqlQuery>
#include <QCache>
#include <QSqlError>

#include "LogParam.h"
#include "debug.h"
#include "data/Data.h"
#include "models/LogbookModel.h"
#include "data/BandPlan.h"

MODULE_IDENTIFICATION("qlog.core.logparam");

#define PARAMMUTEXLOCKER     qCDebug(runtime) << "Waiting for mutex"; \
                             QMutexLocker locker(&cacheMutex); \
                             qCDebug(runtime) << "Using logparam"

LogParam::LogParam(QObject *parent) :
    QObject(parent)
{
    FCT_IDENTIFICATION;
}

bool LogParam::setLOVParam(const QString &LOVName, const QVariant &value)
{
    return setParam(LOVName, value);
}

QDate LogParam::getLOVaParam(const QString &LOVName)
{
    return getParam(LOVName).toDate();
}

bool LogParam::setLastBackupDate(const QDate date)
{
    return setParam("last_backup", date);
}

QDate LogParam::getLastBackupDate()
{
    return getParam("last_backup").toDate();
}

bool LogParam::setLogID(const QString &id)
{
    return setParam("logid", id);
}

QString LogParam::getLogID()
{
    return getParam("logid").toString();
}

bool LogParam::setContestSeqnoType(const QVariant &data)
{
    return setParam("contest/seqnotype", data);
}

int LogParam::getContestSeqnoType()
{
    return getParam("contest/seqnotype", Data::SeqType::SINGLE).toInt();
}

bool LogParam::setContestManuDupeType(const QVariant &data)
{
    return setParam("contest/dupetype", data);
}

int LogParam::getContestDupeType()
{
    return getParam("contest/dupetype", Data::DupeType::ALL_BANDS).toInt();
}

bool LogParam::setContestLinkExchange(const QVariant &data)
{
    return setParam("contest/linkexchangetype", data);
}

int LogParam::getContestLinkExchange()
{
    return getParam("contest/linkexchangetype", LogbookModel::COLUMN_INVALID).toInt();
}

bool LogParam::setContestFilter(const QString &filterName)
{
    return setParam("contest/filter", filterName);
}

QString LogParam::getContestFilter()
{
    return getParam("contest/filter").toString();
}

bool LogParam::setContestID(const QString &contestID)
{
    return setParam("contest/contestid", contestID);
}

QString LogParam::getContestID()
{
    return getParam("contest/contestid", QString()).toString();
}

bool LogParam::setContestDupeDate(const QDateTime date)
{
    return setParam("contest/dupeDate", date);
}

QDateTime LogParam::getContestDupeDate()
{
    return getParam("contest/dupeDate").toDateTime();
}

void LogParam::removeConetstDupeDate()
{
    removeParamGroup("contest/dupeDate");
}

bool LogParam::getDxccConfirmedByLotwState()
{
    return getParam("others/dxccconfirmedbylotw", true).toBool();
}

bool LogParam::setDxccConfirmedByLotwState(bool state)
{
    return setParam("others/dxccconfirmedbylotw", state);
}

bool LogParam::setDxccConfirmedByPaperState(bool state)
{
    return setParam("others/dxccconfirmedbypaper", state);
}

bool LogParam::getDxccConfirmedByPaperState()
{
    return getParam("others/dxccconfirmedbypaper", true).toBool();
}

bool LogParam::setDxccConfirmedByEqslState(bool state)
{
    return setParam("others/dxccconfirmedbyeqsl", state);
}

bool LogParam::getDxccConfirmedByEqslState()
{
    return getParam("others/dxccconfirmedbyeqsl", false).toBool();
}

int LogParam::getContestSeqno(const QString &band)
{
    return getParam(( band.isEmpty() ) ? "contest/seqnos/single"
                                       : QString("contest/seqnos/%1").arg(band), 1).toInt();
}

bool LogParam::setContestSeqno(int value, const QString &band)
{
    return setParam(( band.isEmpty() ) ? "contest/seqnos/single"
                                       : QString("contest/seqnos/%1").arg(band), value);
}

void LogParam::removeContestSeqno()
{
    removeParamGroup("contest/seqnos/");
}

bool LogParam::setDXCTrendContinent(const QString &cont)
{
    return setParam("dxc/trendContinent", cont);
}

QString LogParam::getDXCTrendContinent(const QString &def)
{
    return getParam("dxc/trendContinent", def).toString();
}

void LogParam::removeDXCTrendContinent()
{
    removeParamGroup("dxc/trendContinent");
}

QStringList LogParam::bandmapsWidgets()
{
    return getKeys("bandmap/");
}

void LogParam::removeBandmapWidgetGroup(const QString &group)
{
    removeParamGroup("bandmap/" + group);
}

double LogParam::getBandmapScrollFreq(const QString &widgetID, const QString &bandName)
{
    return getParam("bandmap/" + widgetID + "/" + bandName + "/scrollfreq" , 0.0).toDouble();
}

bool LogParam::setBandmapScrollFreq(const QString &widgetID, const QString &bandName, double scroll)
{
    return setParam("bandmap/" + widgetID + "/" + bandName + "/scrollfreq", scroll);
}

QVariant LogParam::getBandmapZoom(const QString &widgetID, const QString &bandName, const QVariant &defaultValue)
{
    return getParam("bandmap/" + widgetID + "/" + bandName + "/zoom", defaultValue);
}

bool LogParam::setBandmapZoom(const QString &widgetID, const QString &bandName, const QVariant &zoom)
{
    return setParam("bandmap/" + widgetID + "/" + bandName + "/zoom", zoom);
}

bool LogParam::setBandmapAging(const QString &widgetID, int aging)
{
    return setParam("bandmap/" + widgetID + "/spotaging", aging);
}

int LogParam::getBandmapAging(const QString &widgetID)
{
    return getParam("bandmap/" + widgetID + "/spotaging", 0).toInt();
}

bool LogParam::setBandmapCenterRX(const QString &widgetID, bool centerRX)
{
    return setParam("bandmap/" + widgetID + "/centerrx", centerRX);
}

bool LogParam::getBandmapCenterRX(const QString &widgetID)
{
    return getParam("bandmap/" + widgetID + "/centerrx", true).toBool();
}

QString LogParam::getUploadQSOLastCall()
{
    return getParam("uploadqso/lastcall").toString();
}

void LogParam::setUploadQSOLastCall(const QString &call)
{
    setParam("uploadqso/lastcall", call);
}

bool LogParam::getUploadeqslQSLComment()
{
    return getParam("uploadqso/eqsl/last_checkcomment", false).toBool();
}

void LogParam::setUploadeqslQSLComment(const bool state)
{
    setParam("uploadqso/eqsl/last_checkcomment", state);
}

bool LogParam::getUploadeqslQSLMessage()
{
    return getParam("uploadqso/eqsl/last_checkqslsmsg", false).toBool();
}

QString LogParam::getUploadeqslQTHProfile()
{
    return getParam("uploadqso/eqsl/last_QTHProfile").toString();
}

void LogParam::setUploadeqslQTHProfile(const QString &qthProfile)
{
    setParam("uploadqso/eqsl/last_QTHProfile", qthProfile);
}

void LogParam::setUploadeqslQSLMessage(const bool state)
{
    setParam("uploadqso/eqsl/last_checkqslsmsg", state);
}

bool LogParam::getUploadServiceState(const QString &name)
{
    return getParam("uploadqso/" + name + "/enabled", false).toBool();
}

void LogParam::setUploadServiceState(const QString &name, bool state)
{
    setParam("uploadqso/" + name + "/enabled", state);
}

int LogParam::getUploadQSOFilterType()
{
    return getParam("uploadqso/filtertype").toInt();
}

void LogParam::setUploadQSOFilterType(int filterID)
{
    setParam("uploadqso/filtertype", filterID);
}

bool LogParam::getDownloadQSLServiceState(const QString &name)
{
    return getParam("downloadqsl/" + name + "/enabled", false).toBool();
}

void LogParam::setDownloadQSLServiceState(const QString &name, bool state)
{
    setParam("downloadqsl/" + name + "/enabled", state);
}

QDate LogParam::getDownloadQSLServiceLastDate(const QString &name)
{
    return getParam("downloadqsl/" + name + "/lastdate", QDate(1900, 1, 1)).toDate();
}

void LogParam::setDownloadQSLServiceLastDate(const QString &name, const QDate &date)
{
    setParam("downloadqsl/" + name + "/lastdate", date);
}

bool LogParam::getDownloadQSLServiceLastQSOQSL(const QString &name)
{
    return getParam("downloadqsl/" + name + "/qsoqsl", true).toBool();
}

void LogParam::setDownloadQSLServiceLastQSOQSL(const QString &name, bool state)
{
    setParam("downloadqsl/" + name + "/qsoqsl", state);
}

QString LogParam::getDownloadQSLLoTWLastCall()
{
    return getParam("downloadqsl/lotw/lastmycallsign").toString();
}

void LogParam::setDownloadQSLLoTWLastCall(const QString &call)
{
    setParam("downloadqsl/lotw/lastmycallsign", call);
}

QString LogParam::getDownloadQSLeQSLLastProfile()
{
    return getParam("downloadqsl/eqsl/lastqthprofile").toString();
}

void LogParam::setDownloadQSLeQSLLastProfile(const QString &profile)
{
    setParam("downloadqsl/eqsl/lastqthprofile", profile);
}

QString LogParam::getQRZCOMCallbookUsername()
{
    return getParam("services/qrzcom/callbook/username").toString().trimmed();
}

void LogParam::setQRZCOMCallbookUsername(const QString &username)
{
    setParam("services/qrzcom/callbook/username", username);
}

QStringList LogParam::getQRZCOMAPICallsignsList()
{
    return getParamStringList("services/qrzcom/logbook/apicallsigns");
}

void LogParam::setQRZCOMAPICallsignsList(const QStringList &list)
{
    setParam("services/qrzcom/logbook/apicallsigns", list);
}

QString LogParam::getCloudlogAPIEndpoint()
{
    return getParam("services/cloudlog/logbook/endpoint", "http://localhost").toString();
}

void LogParam::setCloudlogAPIEndpoint(const QString &endpoint)
{
    setParam("services/cloudlog/logbook/endpoint", endpoint);
}

uint LogParam::getCloudlogStationID()
{
    return getParam("services/cloudlog/logbook/stationid").toUInt();
}

void LogParam::setCloudlogStationID(uint stationID)
{
    setParam("services/cloudlog/logbook/stationid", stationID);
}

QString LogParam::getClublogLogbookReqEmail()
{
    return getParam("services/clublog/logbook/regemail").toString().trimmed();
}

void LogParam::setClublogLogbookReqEmail(const QString &email)
{
    setParam("services/clublog/logbook/regemail", email);
}

bool LogParam::getClublogUploadImmediatelyEnabled()
{
    return getParam("services/clublog/logbook/uploadimmediately", false).toBool();
}

void LogParam::setClublogUploadImmediatelyEnabled(bool state)
{
    setParam("services/clublog/logbook/uploadimmediately", state);
}

QString LogParam::getEQSLLogbookUsername()
{
    return getParam("services/eqsl/logbook/username").toString().trimmed();
}

void LogParam::setEQSLLogbookUsername(const QString &username)
{
    setParam("services/eqsl/logbook/username", username);
}

QString LogParam::getHamQTHCallbookUsername()
{
    return getParam("services/hamqth/callbook/username").toString().trimmed();
}

void LogParam::setHamQTHCallbookUsername(const QString &username)
{
    setParam("services/hamqth/callbook/username", username);
}

QString LogParam::getHRDLogLogbookReqCallsign()
{
    return getParam("services/hrdlog/logbook/regcallsign").toString().trimmed();
}

void LogParam::setHRDLogLogbookReqCallsign(const QString &callsign)
{
    setParam("services/hrdlog/logbook/regcallsign", callsign);
}

bool LogParam::getHRDLogOnAir()
{
    return getParam("services/hrdlog/logbook/onair", false).toBool();
}

void LogParam::setHRDLogOnAir(bool state)
{
    setParam("services/hrdlog/logbook/onair", state);
}

QString LogParam::getKSTChatUsername()
{
    return getParam("services/kst/chat/username").toString().trimmed();
}

void LogParam::setKSTChatUsername(const QString &username)
{
    setParam("services/kst/chat/username", username);
}

QString LogParam::getLoTWCallbookUsername()
{
    return getParam("services/lotw/callbook/username").toString().trimmed();
}

void LogParam::setLoTWCallbookUsername(const QString &username)
{
    setParam("services/lotw/callbook/username", username);
}

QString LogParam::getLoTWTQSLPath(const QString &defaultPath)
{
    return getParam("services/lotw/callbook/tqsl", defaultPath).toString().trimmed();
}

void LogParam::setLoTWTQSLPath(const QString &path)
{
    setParam("services/lotw/callbook/tqsl", path);
}

QString LogParam::getPrimaryCallbook(const QString &defaultValue)
{
    return getParam("callbook/primary", defaultValue).toString();
}

void LogParam::setPrimaryCallbook(const QString &callbookName)
{
    setParam("callbook/primary", callbookName);
}

QString LogParam::getSecondaryCallbook(const QString &defaultValue)
{
    return getParam("callbook/secondary", defaultValue).toString();
}

void LogParam::setSecondaryCallbook(const QString &callbookName)
{
    setParam("callbook/secondary", callbookName);
}

QString LogParam::getCallbookWebLookupURL(const QString &defaultURL)
{
    return getParam("callbook/weblookupurl", defaultURL).toString();
}

void LogParam::setCallbookWebLookupURL(const QString &url)
{
    setParam("callbook/weblookupurl", url);
}

QString LogParam::getNetworkNotifLogQSOAddrs()
{
    return getParam("network/notif/log/qso/addrs").toString();
}

void LogParam::setNetworkNotifLogQSOAddrs(const QString &addrs)
{
    setParam("network/notif/log/qso/addrs", addrs);
}

QString LogParam::getNetworkNotifDXCSpotAddrs()
{
    return getParam("network/notif/dxc/spot/addrs").toString();
}

void LogParam::setNetworkNotifDXCSpotAddrs(const QString &addrs)
{
    setParam("network/notif/dxc/spot/addrs", addrs);
}

QString LogParam::getNetworkNotifWSJTXCQSpotAddrs()
{
    return getParam("network/notif/wsjtx/cqspot/addrs").toString();
}

void LogParam::setNetworkNotifWSJTXCQSpotAddrs(const QString &addrs)
{
    setParam("network/notif/wsjtx/cqspot/addrs", addrs);
}

QString LogParam::getNetworkNotifAlertsSpotAddrs()
{
    return getParam("network/notif/alerts/spot/addrs").toString();
}

void LogParam::setNetworkNotifAlertsSpotAddrs(const QString &addrs)
{
    setParam("network/notif/alerts/spot/addrs", addrs);
}

QString LogParam::getNetworkNotifRigStateAddrs()
{
    return getParam("network/notif/rig/state/addrs").toString();
}

void LogParam::setNetworkNotifRigStateAddrs(const QString &addrs)
{
    setParam("network/notif/rig/state/addrs", addrs);
}

int LogParam::getNetworkWsjtxListenerPort(int defaultPort)
{
    return getParam("network/listener/wsjtx/port", defaultPort).toInt();
}

void LogParam::setNetworkNotifRigStateAddrs(int port)
{
    setParam("network/listener/wsjtx/port", port);
}

QString LogParam::getNetworkWsjtxForwardAddrs()
{
    return getParam("network/forwarder/wsjtx/addrs").toString();
}

void LogParam::setNetworkWsjtxForwardAddrs(const QString &addrs)
{
    setParam("network/forwarder/wsjtx/addrs", addrs);
}

bool LogParam::getNetworkWsjtxListenerJoinMulticast()
{
    return getParam("network/listener/wsjtx/multicast/join", false).toBool();
}

void LogParam::setNetworkWsjtxListenerJoinMulticast(bool state)
{
    setParam("network/listener/wsjtx/multicast/join", state);
}

QString LogParam::getNetworkWsjtxListenerMulticastAddr()
{
    return getParam("network/listener/wsjtx/multicast/addr").toString();
}

void LogParam::setNetworkWsjtxListenerMulticastAddr(const QString &addr)
{
    setParam("network/listener/wsjtx/multicast/addr", addr);
}

int LogParam::getNetworkWsjtxListenerMulticastTTL()
{
    return getParam("network/listener/wsjtx/multicast/ttl").toInt();
}

void LogParam::setNetworkWsjtxListenerMulticastTTL(int ttl)
{
    setParam("network/listener/wsjtx/multicast/ttl", ttl);
}

QStringList LogParam::getEnabledMemberlists()
{
    return getParamStringList("memberlist/enabledlists");
}

void LogParam::setEnabledMemberlists(const QStringList &list)
{
    setParam("memberlist/enabledlists", list);
}

int LogParam::getAlertAging()
{
    return getParam("alert/aging").toInt();
}

void LogParam::setAlertAging(int aging)
{
    setParam("alert/aging", aging);
}

QByteArray LogParam::getAlertWidgetState()
{
    return QByteArray::fromBase64(getParam("alert/widgetstate").toByteArray());
}

void LogParam::setAlertWidgetState(const QByteArray &state)
{
    setParam("alert/widgetstate", state.toBase64());
}

bool LogParam::getCWConsoleSendWord()
{
    return getParam("cwconsole/sendword", false).toBool();
}

void LogParam::setCWConsoleSendWord(bool state)
{
    setParam("cwconsole/sendword", state);
}

bool LogParam::getChatSelectedRoom()
{
    return getParam("chat/selectedroom", 0).toInt();
}

void LogParam::setChatSelectedRoom(int room)
{
    setParam("chat/selectedroom", room);
}

double LogParam::getNewContactFreq()
{
    return getParam("newcontact/freq", 3.5).toDouble();
}

void LogParam::setNewContactFreq(double freq)
{
    setParam("newcontact/freq", freq);
}

QString LogParam::getNewContactMode()
{
    return getParam("newcontact/mode", "CW").toString();
}

void LogParam::setNewContactMode(const QString &mode)
{
    setParam("newcontact/mode", mode);
}

QString LogParam::getNewContactSubMode()
{
    return getParam("newcontact/submode").toString();
}

void LogParam::setNewContactSubMode(const QString &submode)
{
    setParam("newcontact/submode", submode);
}

double LogParam::getNewContactPower()
{
    return getParam("newcontact/power", 100).toDouble();
}

void LogParam::setNewContactPower(double power)
{
    setParam("newcontact/power", power);
}

int LogParam::getNewContactTabIndex()
{
    return getParam("newcontact/tabindex", 0).toInt();
}

void LogParam::setNewContactTabIndex(int index)
{
    setParam("newcontact/tabindex", index);
}

QString LogParam::getNewContactQSLSent()
{
    return getParam("newcontact/sqlsent", "Q").toString();
}

void LogParam::setNewContactQSLSent(const QString &qslsent)
{
    setParam("newcontact/sqlsent", qslsent);
}

QString LogParam::getNewContactLoTWQSLSent()
{
    return getParam("newcontact/lotwsqlsent", "Q").toString();
}

void LogParam::setNewContactLoTWQSLSent(const QString &qslsent)
{
    setParam("newcontact/lotwsqlsent", qslsent);
}

QString LogParam::getNewContactEQSLWQSLSent()
{
    return getParam("newcontact/eqslsqlsent", "Q").toString();
}

void LogParam::setNewContactEQSLQSLSent(const QString &qslsent)
{
    setParam("newcontact/eqslsqlsent", qslsent);
}

QString LogParam::getNewContactQSLVia()
{
    return getParam("newcontact/qslvia").toString();
}

void LogParam::setNewContactQSLVia(const QString &qslvia)
{
    setParam("newcontact/qslvia", qslvia);
}

QString LogParam::getNewContactPropMode()
{
    return getParam("newcontact/propmode").toString();
}

void LogParam::setNewContactPropMode(const QString &propmode)
{
    setParam("newcontact/propmode", propmode);
}

bool LogParam::getNewContactTabsExpanded()
{
    return getParam("newcontact/tabsexpand", true).toBool();
}

void LogParam::setNewContactTabsExpanded(bool state)
{
    setParam("newcontact/tabsexpand", state);
}

QString LogParam::getNewContactSatName()
{
    return getParam("newcontact/satname").toString();
}

void LogParam::setNewContactSatName(const QString &name)
{
    setParam("newcontact/satname", name);
}

QStringList LogParam::getMapLayerStates(const QString &widgetID)
{
    return getKeys(widgetID + "/layerstate/");
}

bool LogParam::getMapLayerState(const QString &widgetID, const QString &layerName)
{
    return getParam(widgetID + "/layerstate/" + layerName).toBool();
}

void LogParam::setMapLayerState(const QString &widgetID, const QString &layerName, bool state)
{
    setParam(widgetID + "/layerstate/" + layerName, state);
}

uint LogParam::getWsjtxFilterDxccStatus()
{
    return getParam("wsjtx/filter/dxccstatus", (DxccStatus::NewEntity | DxccStatus::NewBand |
                                                DxccStatus::NewMode   | DxccStatus::NewSlot |
                                                DxccStatus::Worked    | DxccStatus::Confirmed)).toUInt();
}

void LogParam::setWsjtxFilterDxccStatus(uint mask)
{
    setParam("wsjtx/filter/dxccstatus", mask);
}

QString LogParam::getWsjtxFilterContRE()
{
    return getParam("wsjtx/filter/contregexp", "NOTHING|AF|AN|AS|EU|NA|OC|SA").toString();
}

void LogParam::setWsjtxFilterContRE(const QString &re)
{
    setParam("wsjtx/filter/contregexp", re);
}

int LogParam::getWsjtxFilterDistance()
{
    return getParam("wsjtx/filter/distance", 0).toInt();
}

void LogParam::setWsjtxFilterDistance(int dist)
{
    setParam("wsjtx/filter/distance", dist);
}

int LogParam::getWsjtxFilterSNR()
{
    return getParam("wsjtx/filter/snr", -41).toInt();
}

void LogParam::setWsjtxFilterSNR(int snr)
{
    setParam("wsjtx/filter/snr", snr);
}

QStringList LogParam::getWsjtxMemberlists()
{
    return getParamStringList("wsjtx/filter/memberlists");
}

void LogParam::setWsjtxMemberlists(const QStringList &list)
{
    setParam("wsjtx/filter/memberlists", list);
}

QByteArray LogParam::getWsjtxWidgetState()
{
    return QByteArray::fromBase64(getParam("wsjtx/widgetstate").toByteArray());
}

void LogParam::setWsjtxWidgetState(const QByteArray &state)
{
    setParam("wsjtx/widgetstate", state.toBase64());
}

uint LogParam::getDXCFilterDxccStatus()
{
    return getParam("dxc/filter/dx/dxccstatus", (DxccStatus::NewEntity | DxccStatus::NewBand |
                                                 DxccStatus::NewMode   | DxccStatus::NewSlot |
                                                 DxccStatus::Worked    | DxccStatus::Confirmed)).toUInt();
}

void LogParam::setDXCFilterDxccStatus(uint mask)
{
    setParam("dxc/filter/dx/dxccstatus", mask);
}

QString LogParam::getDXCFilterContRE()
{
    return getParam("dxc/filter/dx/contregexp", "NOTHING|AF|AN|AS|EU|NA|OC|SA").toString();
}

void LogParam::setDXCFilterContRE(const QString &re)
{
    setParam("dxc/filter/dx/contregexp", re);
}

QString LogParam::getDXCFilterSpotterContRE()
{
    return getParam("dxc/filter/spotter/contregexp", "NOTHING|AF|AN|AS|EU|NA|OC|SA").toString();
}

void LogParam::setDXCFilterSpotterContRE(const QString &re)
{
    setParam("dxc/filter/spotter/contregexp", re);
}

bool LogParam::getDXCFilterDedup()
{
    return getParam("dxc/filter/dedup", false).toBool();
}

void LogParam::setDXCFilterDedup(bool state)
{
    setParam("dxc/filter/dedup", state);
}

int LogParam::getDXCFilterDedupTime(int defaultValue)
{
    return getParam("dxc/filter/deduptime", defaultValue).toInt();
}

void LogParam::setDXCFilterDedupTime(int value)
{
    setParam("dxc/filter/deduptime", value);
}

int LogParam::getDXCFilterDedupFreq(int defaultValue)
{
    return getParam("dxc/filter/dedupfreq", defaultValue).toInt();
}

void LogParam::setDXCFilterDedupFreq(int value)
{
    setParam("dxc/filter/dedupfreq", value);
}

QStringList LogParam::getDXCFilterMemberlists()
{
    return getParamStringList("dxc/filter/dx/memberlists");
}

void LogParam::setDXCFilterMemberlists(const QStringList &list)
{
    setParam("dxc/filter/dx/memberlists", list);
}

bool LogParam::getDXCAutoconnectServer()
{
    return getParam("dxc/autoconnect", false).toBool();
}

void LogParam::setDXCAutoconnectServer(bool state)
{
    setParam("dxc/autoconnect", state);
}

bool LogParam::getDXCKeepQSOs()
{
    return getParam("dxc/keepqsos", false).toBool();
}

void LogParam::setDXCKeepQSOs(bool state)
{
    setParam("dxc/keepqsos", state);
}

QByteArray LogParam::getDXCDXTableState()
{
    return QByteArray::fromBase64(getParam("dxc/dxtablestate").toByteArray());
}

void LogParam::setDXCDXTableState(const QByteArray &state)
{
    setParam("dxc/dxtablestate", state.toBase64());
}

QByteArray LogParam::getDXCWCYTableState()
{
    return QByteArray::fromBase64(getParam("dxc/wcytablestate").toByteArray());
}

void LogParam::setDXCWCYTableState(const QByteArray &state)
{
    setParam("dxc/wcytablestate", state.toBase64());
}

QByteArray LogParam::getDXCWWVTableState()
{
    return QByteArray::fromBase64(getParam("dxc/wwvtablestate").toByteArray());
}

void LogParam::setDXCWWVTableState(const QByteArray &state)
{
    setParam("dxc/wwvtablestate", state.toBase64());
}

QByteArray LogParam::getDXCTOALLTableState()
{
    return QByteArray::fromBase64(getParam("dxc/toalltablestate").toByteArray());
}

void LogParam::setDXCTOALLTableState(const QByteArray &state)
{
    setParam("dxc/toalltablestate", state.toBase64());
}

int LogParam::getDXCConsoleFontSize()
{
    return getParam("dxc/console/fontsize", -1).toInt();
}

void LogParam::setDXCConsoleFontSize(int value)
{
    setParam("dxc/console/fontsize", value);
}

QStringList LogParam::getDXCServerlist()
{
    return getParamStringList("dxc/serverlist", {"hamqth.com:7300"});
}

void LogParam::setDXCServerlist(const QStringList &list)
{
    setParam("dxc/serverlist", list);
}

QString LogParam::getDXCLastServer()
{
    return getParam("dxc/lastserver").toString();
}

void LogParam::setDXCLastServer(const QString &server)
{
    setParam("dxc/lastserver", server);
}

QString LogParam::getDXCFilterModeRE()
{
    QString defaultValue = "NOTHING|"
            + BandPlan::MODE_GROUP_STRING_PHONE + "|"
            + BandPlan::MODE_GROUP_STRING_CW + "|"
            + BandPlan::MODE_GROUP_STRING_FT8 + "|"
            + BandPlan::MODE_GROUP_STRING_DIGITAL;
    return getParam("dxc/filter/dx/moderegexp", defaultValue).toString();
}

void LogParam::setDXCFilterModeRE(const QString &re)
{
    setParam("dxc/filter/dx/moderegexp", re);
}

QStringList LogParam::getDXCExcludedBands()
{
    return getParamStringList("dxc/filter/excludedbands");
}

void LogParam::setDXCExcludedBands(const QStringList &excluded)
{
    setParam("dxc/filter/excludedbands", excluded);
}

QSet<int> LogParam::getExportColumnSet(const QString &paramName, const QSet<int> &defaultValue)
{
    QSet<int> set;
    QStringList defaultValueList;

    for (int val : defaultValue)
        defaultValueList << QString::number(val);

    const QStringList sourceList = getParamStringList("exportadi/" + paramName, defaultValueList);

    for (const QString &s : sourceList)
        set.insert(s.toInt());

    return set;
}

void LogParam::setExportColumnSet(const QString &paramName, const QSet<int> &set)
{
    QStringList valueList;

    for (int val : set)
        valueList << QString::number(val);

    setParam("exportadi/" + paramName, valueList);
}

QByteArray LogParam::getLogbookState()
{
    return QByteArray::fromBase64(getParam("logbook/maintablestate").toByteArray());
}

void LogParam::setLogbookState(const QByteArray &state)
{
    setParam("logbook/maintablestate", state.toBase64());
}

int LogParam::getLogbookFilterSearchType(int defaultValue)
{
    return getParam("logbook/filter/searchtype", defaultValue).toInt();
}

void LogParam::setLogbookFilterSearchType(int type)
{
    setParam("logbook/filter/searchtype", type);
}

QString LogParam::getLogbookFilterBand()
{
    return getParam("logbook/filter/band").toString();
}

void LogParam::setLogbookFilterBand(const QString &name)
{
    setParam("logbook/filter/band", name);
}

QString LogParam::getLogbookFilterMode()
{
    return getParam("logbook/filter/mode").toString();
}

void LogParam::setLogbookFilterMode(const QString &name)
{
    setParam("logbook/filter/mode", name);
}

QString LogParam::getLogbookFilterCountry()
{
    return getParam("logbook/filter/country").toString();
}

void LogParam::setLogbookFilterCountry(const QString &name)
{
    setParam("logbook/filter/country", name);
}

QString LogParam::getLogbookFilterUserFilter()
{
    return getParam("logbook/filter/userfilter").toString();
}

void LogParam::setLogbookFilterUserFilter(const QString &name)
{
    setParam("logbook/filter/userfilter", name);
}

QString LogParam::getLogbookFilterClub()
{
    return getParam("logbook/filter/club").toString();
}

void LogParam::setLogbookFilterClub(const QString &name)
{
    setParam("logbook/filter/club", name);
}

bool LogParam::getMainWindowAlertBeep()
{
    return getParam("mainwindow/alertbeep", false).toBool();
}

void LogParam::setMainWindowAlertBeep(bool state)
{
    setParam("mainwindow/alertbeep", state);
}

int LogParam::getMainWindowDarkMode()
{
    return getParam("mainwindow/darkmode", 0).toInt();
}

void LogParam::setMainWindowDarkMode(int state)
{
    setParam("mainwindow/darkmode", state);
}

QByteArray LogParam::getMainWindowGeometry()
{
    return QByteArray::fromBase64(getParam("mainwindow/geometry").toByteArray());
}

void LogParam::setMainWindowGeometry(const QByteArray &state)
{
    setParam("mainwindow/geometry", state.toBase64());
}

QByteArray LogParam::getMainWindowState()
{
    return QByteArray::fromBase64(getParam("mainwindow/state").toByteArray());
}

void LogParam::setMainWindowState(const QByteArray &state)
{
    setParam("mainwindow/state", state.toBase64());
}

QString LogParam::getMainWindowBandmapWidgets()
{
    return getParam("mainwindow/bandmapwidgets").toString();
}

void LogParam::setMainWindowBandmapWidgets(const QString &value)
{
    setParam("mainwindow/bandmapwidgets", value);
}

void LogParam::removeMainWindowBandmapWidgets()
{
    removeParamGroup("mainwindow/bandmapwidgets");
}

bool LogParam::setParam(const QString &name, const QVariant &value)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << name << value;

    QSqlQuery query;

    if ( ! query.prepare("INSERT OR REPLACE INTO log_param (name, value) "
                         "VALUES (:nam, :val)") )
    {
        qWarning()<< "Cannot prepare insert parameter statement for parameter" << name;
        return false;
    }

    query.bindValue(":nam", name);
    query.bindValue(":val", value);

    PARAMMUTEXLOCKER;

    if ( !query.exec() )
    {
        qWarning() << "SET - Cannot exec an insert parameter statement for" << name;
        return false;
    }

    localCache.remove(name);

    qCDebug(runtime) << "SET:" << name << "value" << value;
    return true;
}

QVariant LogParam::getParam(const QString &name, const QVariant &defaultValue)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << name;

    PARAMMUTEXLOCKER;

    const QVariant *valueCached = localCache.object(name);

    if ( valueCached )
    {
        qCDebug(runtime) << "GET:" << name << "Cached value: " << *valueCached;
        return *valueCached;
    }

    QSqlQuery query;

    if ( ! query.prepare("SELECT value FROM log_param WHERE name = :nam") )
    {
        qWarning()<< "GET - Cannot prepare select parameter statement for" << name;
        return defaultValue;
    }

    query.bindValue(":nam", name);

    if ( ! query.exec() )
    {
        qWarning() << "GET - Cannot execute GetParam Select for" << name << "using default" << defaultValue;
        return defaultValue;
    }

    if ( query.first() )
    {
        if ( !query.isNull(0) )
        {
            QVariant dbValue = query.value(0);
            localCache.insert(name, new QVariant(dbValue));
            qCDebug(runtime) << "GET:" << name << "DB value: " << dbValue;
            return dbValue;
        }
        qCDebug(runtime) << "GET:" << name << "NULL value in DB - using default " << defaultValue;
    }
    else
        qCDebug(runtime) << "GET:" << name << "Key not found in DB - using default " << defaultValue;

    //localCache.insert(name, new QVariant(defaultValue)); // Do not insert default value here because
                                                           // in some use cases — for example, DXC and station_profile
                                                           // the profile is not yet accessible during the first call,
                                                           // and an empty value is incorrectly inserted into the cache.

    return defaultValue;
}

bool LogParam::setParam(const QString &name, const QStringList &value)
{
    FCT_IDENTIFICATION;

    return setParam(name, serializeStringList(value));
}

QStringList LogParam::getParamStringList(const QString &name, const QStringList &defaultValue)
{
    FCT_IDENTIFICATION;

    return deserializeStringList(getParam(name, serializeStringList(defaultValue)).toString());
}

void LogParam::removeParamGroup(const QString &paramGroup)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << paramGroup;

    PARAMMUTEXLOCKER;

    const QStringList &keys = localCache.keys();

    for ( const QString& key : keys )
        if (key.startsWith(paramGroup)) localCache.remove(key);

    QSqlQuery query;

    if ( ! query.prepare("DELETE FROM log_param WHERE name LIKE :group ") )
    {
        qWarning()<< "Cannot prepare delete parameter statement for" << paramGroup;
        return;
    }

    query.bindValue(":group", paramGroup + "%");

    if ( ! query.exec() )
        qWarning() << "Cannot execute removeParamGroup statement for" << paramGroup;

    return;
}

QStringList LogParam::getKeys(const QString &group)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << group;

    PARAMMUTEXLOCKER;

    QSet<QString> keys; // unique values;
    QSqlQuery query;

    if ( ! query.prepare("SELECT name FROM log_param WHERE name LIKE :group ") )
    {
        qWarning()<< "Cannot prepare select parameter statement for" << group << query.lastError().text();
        return QStringList();
    }

    query.bindValue(":group", group + "%");

    if ( ! query.exec() )
    {
        qWarning() << "Cannot execute removeParamGroup statement for" << group;
        return QStringList();
    }

    while ( query.next() )
    {
        const QString &param = query.value(0).toString();
        const QString &subKey = param.mid(group.length()).section("/", 0, 0);
        if ( !subKey.isEmpty() ) keys.insert(subKey);
    }
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    return QStringList(keys.begin(), keys.end());
#else
    return keys.toList();
#endif
}

QString LogParam::escapeString(const QString &input, QChar escapeChar, QChar delimiter)
{
    QString result;
    for (QChar ch : input)
    {
        if (ch == escapeChar || ch == delimiter) result += escapeChar;
        result += ch;
    }
    return result;
}

QString LogParam::unescapeString(const QString &input, QChar escapeChar)
{
    QString result;
    bool escaping = false;
    for ( QChar ch : input )
    {
        if ( escaping )
        {
            result += ch;
            escaping = false;
        }
        else if ( ch == escapeChar )
            escaping = true;
        else
            result += ch;
    }
    return result;
}

QString LogParam::serializeStringList(const QStringList &list, QChar delimiter, QChar escapeChar)
{
    QStringList escapedList;
    for ( const QString &item : list )
        escapedList << escapeString(item, escapeChar, delimiter);
    return escapedList.join(delimiter);
}

QStringList LogParam::deserializeStringList(const QString &input, QChar delimiter, QChar escapeChar)
{
    QStringList result;

    if ( !input.isEmpty() )
    {
        QString current;
        bool escaping = false;

        for ( QChar ch : input )
        {
            if ( escaping )
            {
                current += ch;
                escaping = false;
            }
            else if ( ch == escapeChar )
                escaping = true;
            else if ( ch == delimiter )
            {
                result << current;
                current.clear();
            }
            else
                current += ch;
        }
        result << current;
    }
    return result;
}

QCache<QString, QVariant> LogParam::localCache(300);

QMutex LogParam::cacheMutex;

#undef PARAMMUTEXLOCKER
