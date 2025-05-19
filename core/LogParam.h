#ifndef QLOG_CORE_LOGPARAM_H
#define QLOG_CORE_LOGPARAM_H

#include <QObject>
#include <QDate>
#include <QVariant>
#include <QMutex>

#include "data/Data.h"
#include "models/LogbookModel.h"

class LogParam : public QObject
{
    Q_OBJECT
public:
    explicit LogParam(QObject *parent = nullptr);
    static bool setLOVParam(const QString &LOVName, const QVariant &value)
    {
        return setParam(LOVName, value);
    };
    static QDate getLOVaParam(const QString &LOVName)
    {
        return getParam(LOVName).toDate();
    }
    static bool setLastBackupDate(const QDate date)
    {
        return setParam("last_backup", date);
    };
    static QDate getLastBackupDate()
    {
        return getParam("last_backup").toDate();
    };
    static bool setLogID(const QString &id)
    {
        return setParam("logid", id);
    };
    static QString getLogID()
    {
        return getParam("logid").toString();
    };
    static bool setContestSeqnoType(const QVariant &data)
    {
        return setParam("contest/seqnotype", data);
    };
    static int getContestSeqnoType()
    {
        return getParam("contest/seqnotype", Data::SeqType::SINGLE).toInt();
    };
    static bool setContestManuDupeType(const QVariant &data)
    {
        return setParam("contest/dupetype", data);
    };
    static int getContestDupeType()
    {
        return getParam("contest/dupetype", Data::DupeType::ALL_BANDS).toInt();
    };
    static bool setContestLinkExchange(const QVariant &data)
    {
        return setParam("contest/linkexchangetype", data);
    };
    static int getContestLinkExchange()
    {
        return getParam("contest/linkexchangetype", LogbookModel::COLUMN_INVALID).toInt();
    };
    static bool setContestFilter(const QString &filterName)
    {
        return setParam("contest/filter", filterName);
    };
    static QString getContestFilter()
    {
        return getParam("contest/filter").toString();
    };
    static bool setContestID(const QString &contestID)
    {
        return setParam("contest/contestid", contestID);
    };
    static QString getContestID()
    {
        return getParam("contest/contestid", QString()).toString();
    };
    static bool setContestDupeDate(const QDateTime date)
    {
        return setParam("contest/dupeDate", date);
    };
    static QDateTime getContestDupeDate()
    {
        return getParam("contest/dupeDate").toDateTime();
    };
    static void removeConetstDupeDate()
    {
        removeParamGroup("contest/dupeDate");
    }
    static bool getDxccConfirmedByLotwState()
    {
        return getParam("others/dxccconfirmedbylotw", true).toBool();
    };
    static bool setDxccConfirmedByLotwState(bool state)
    {
        return setParam("others/dxccconfirmedbylotw", state);
    };
    static bool setDxccConfirmedByPaperState(bool state)
    {
        return setParam("others/dxccconfirmedbypaper", state);
    };
    static bool getDxccConfirmedByPaperState()
    {
        return getParam("others/dxccconfirmedbypaper", true).toBool();
    };
    static bool setDxccConfirmedByEqslState(bool state)
    {
        return setParam("others/dxccconfirmedbyeqsl", state);
    };
    static bool getDxccConfirmedByEqslState()
    {
        return getParam("others/dxccconfirmedbyeqsl", false).toBool();
    };
    static int getContestSeqno(const QString &band = QString())
    {
        return getParam(( band.isEmpty() ) ? "contest/seqnos/single"
                                           : QString("contest/seqnos/%1").arg(band), 1).toInt();
    }
    static bool setContestSeqno(int value, const QString &band = QString())
    {
        return setParam(( band.isEmpty() ) ? "contest/seqnos/single"
                                           : QString("contest/seqnos/%1").arg(band), value);
    }
    static void removeContestSeqno()
    {
        removeParamGroup("contest/seqnos/");
    }

    static bool setDXCTrendContinent(const QString &cont)
    {
        return setParam("dxc/trendContinent", cont);
    }

    static QString getDXCTrendContinent(const QString &def)
    {
        return getParam("dxc/trendContinent", def).toString();
    }

    static void removeDXCTrendContinent()
    {
        removeParamGroup("dxc/trendContinent");
    }

    static QStringList bandmapsWidgets()
    {
        return getKeys("bandmap/");
    }

    static void removeBandmapWidgetGroup(const QString &group)
    {
        removeParamGroup("bandmap/" + group);
    }

    static double getBandmapScrollFreq(const QString& widgetID, const QString &bandName)
    {
        return getParam("bandmap/" + widgetID + "/" + bandName + "/scrollfreq" , 0.0).toDouble();
    }

    static bool setBandmapScrollFreq(const QString& widgetID, const QString &bandName, double scroll)
    {
        return setParam("bandmap/" + widgetID + "/" + bandName + "/scrollfreq", scroll);
    }

    static QVariant getBandmapZoom(const QString& widgetID, const QString &bandName, const QVariant &defaultValue)
    {
        return getParam("bandmap/" + widgetID + "/" + bandName + "/zoom", defaultValue);
    }

    static bool setBandmapZoom(const QString& widgetID, const QString &bandName, const QVariant &zoom)
    {
        return setParam("bandmap/" + widgetID + "/" + bandName + "/zoom", zoom);
    }

    static bool setBandmapAging(const QString& widgetID, int aging)
    {
        return setParam("bandmap/" + widgetID + "/spotaging", aging);
    }

    static int getBandmapAging(const QString& widgetID)
    {
        return getParam("bandmap/" + widgetID + "/spotaging", 0).toInt();
    }

    static bool setBandmapCenterRX(const QString& widgetID, bool centerRX)
    {
        return setParam("bandmap/" + widgetID + "/centerrx", centerRX);
    }

    static bool getBandmapCenterRX(const QString& widgetID)
    {
        return getParam("bandmap/" + widgetID + "/centerrx", true).toBool();
    }

    static QString getUploadQSOLastCall()
    {
        return getParam("uploadqso/lastcall").toString();
    }

    static void setUploadQSOLastCall(const QString &call)
    {
        setParam("uploadqso/lastcall", call);
    }

    static bool getUploadeqslQSLComment()
    {
        return getParam("uploadqso/eqsl/last_checkcomment", false).toBool();
    }

    static void setUploadeqslQSLComment(const bool state)
    {
        setParam("uploadqso/eqsl/last_checkcomment", state);
    }

    static bool getUploadeqslQSLMessage()
    {
        return getParam("uploadqso/eqsl/last_checkqslsmsg", false).toBool();
    }

    static QString getUploadeqslQTHProfile()
    {
        return getParam("uploadqso/eqsl/last_QTHProfile").toString();
    }

    static void setUploadeqslQTHProfile(const QString &qthProfile)
    {
        setParam("uploadqso/eqsl/last_QTHProfile", qthProfile);
    }

    static void setUploadeqslQSLMessage(const bool state)
    {
        setParam("uploadqso/eqsl/last_checkqslsmsg", state);
    }

    static bool getUploadServiceState(const QString& name)
    {
        return getParam("uploadqso/" + name + "/enabled", false).toBool();
    }

    static void setUploadServiceState(const QString& name, bool state)
    {
        setParam("uploadqso/" + name + "/enabled", state);
    }

    static int getUploadQSOFilterType()
    {
        return getParam("uploadqso/filtertype").toInt();
    }

    static void setUploadQSOFilterType(int filterID)
    {
        setParam("uploadqso/filtertype", filterID);
    }

    static bool getDownloadQSLServiceState(const QString& name)
    {
        return getParam("downloadqsl/" + name + "/enabled", false).toBool();
    }

    static void setDownloadQSLServiceState(const QString& name, bool state)
    {
        setParam("downloadqsl/" + name + "/enabled", state);
    }

    static QDate getDownloadQSLServiceLastDate(const QString& name)
    {
        return getParam("downloadqsl/" + name + "/lastdate", QDate(1900, 1, 1)).toDate();
    }

    static void setDownloadQSLServiceLastDate(const QString& name, const QDate &date)
    {
        setParam("downloadqsl/" + name + "/lastdate", date);
    }

    static bool getDownloadQSLServiceLastQSOQSL(const QString& name)
    {
        return getParam("downloadqsl/" + name + "/qsoqsl", true).toBool();
    }

    static void setDownloadQSLServiceLastQSOQSL(const QString& name, bool state)
    {
        setParam("downloadqsl/" + name + "/qsoqsl", state);
    }

    static QString getDownloadQSLLoTWLastCall()
    {
        return getParam("downloadqsl/lotw/lastmycallsign").toString();
    }

    static void setDownloadQSLLoTWLastCall(const QString &call)
    {
        setParam("downloadqsl/lotw/lastmycallsign", call);
    }

    static QString getDownloadQSLeQSLLastProfile()
    {
        return getParam("downloadqsl/lotw/lastmycallsign").toString();
    }

    static void setDownloadQSLeQSLLastProfile(const QString &profile)
    {
        setParam("downloadqsl/lotw/lastqthprofile", profile);
    }

    static QString getQRZCOMCallbookUsername()
    {
        return getParam("services/qrzcom/callbook/username").toString().trimmed();
    }

    static void setQRZCOMCallbookUsername(const QString& username)
    {
        setParam("services/qrzcom/callbook/username", username);
    }

    static QString getClublogLogbookReqEmail()
    {
        return getParam("services/clublog/logbook/regemail").toString().trimmed();
    }

    static void setClublogLogbookReqEmail(const QString& email)
    {
        setParam("services/clublog/logbook/regemail", email);
    }

    static bool getClublogUploadImmediatelyEnabled()
    {
        return getParam("services/clublog/logbook/uploadimmediately", false).toBool();
    }

    static void setClublogUploadImmediatelyEnabled(bool state)
    {
        setParam("services/clublog/logbook/uploadimmediately", state);
    }

    static QString getEQSLLogbookUsername()
    {
        return getParam("services/eqsl/logbook/username").toString().trimmed();
    }

    static void setEQSLLogbookUsername(const QString& username)
    {
        setParam("services/eqsl/logbook/username", username);
    }

    static QString getHamQTHCallbookUsername()
    {
        return getParam("services/hamqth/callbook/username").toString().trimmed();
    }

    static void setHamQTHCallbookUsername(const QString& username)
    {
        setParam("services/hamqth/callbook/username", username);
    }

    static QString getHRDLogLogbookReqCallsign()
    {
        return getParam("services/hrdlog/logbook/regcallsign").toString().trimmed();
    }

    static void setHRDLogLogbookReqCallsign(const QString& callsign)
    {
        setParam("services/hrdlog/logbook/regcallsign", callsign);
    }

    static bool getHRDLogOnAir()
    {
        return getParam("services/hrdlog/logbook/onair", false).toBool();
    }

    static void setHRDLogOnAir(bool state)
    {
        setParam("services/hrdlog/logbook/onair", state);
    }

    static QString getKSTChatUsername()
    {
        return getParam("services/kst/chat/username").toString().trimmed();
    }

    static void setKSTChatUsername(const QString& username)
    {
        setParam("services/kst/chat/username", username);
    }

    static QString getLoTWCallbookUsername()
    {
        return getParam("services/lotw/callbook/username").toString().trimmed();
    }

    static void setLoTWCallbookUsername(const QString& username)
    {
        setParam("services/lotw/callbook/username", username);
    }

    static QString getLoTWTQSLPath(const QString &defaultPath)
    {
        return getParam("services/lotw/callbook/tqsl", defaultPath).toString().trimmed();
    }

    static void setLoTWTQSLPath(const QString& path)
    {
        setParam("services/lotw/callbook/tqsl", path);
    }


    static QString getPrimaryCallbook(const QString &defaultValue)
    {
        return getParam("callbook/primary", defaultValue).toString();
    }

    static void setPrimaryCallbook(const QString& callbookName)
    {
        setParam("callbook/primary", callbookName);
    }

    static QString getSecondaryCallbook(const QString &defaultValue)
    {
        return getParam("callbook/secondary", defaultValue).toString();
    }

    static void setSecondaryCallbook(const QString& callbookName)
    {
        setParam("callbook/secondary", callbookName);
    }

    static QString getCallbookWebLookupURL(const QString &defaultURL)
    {
        return getParam("callbook/weblookupurl", defaultURL).toString();
    }

    static void setCallbookWebLookupURL(const QString& url)
    {
        setParam("callbook/weblookupurl", url);
    }

    static QString getNetworkNotifLogQSOAddrs()
    {
        return getParam("network/notif/log/qso/addrs").toString();
    }

    static void setNetworkNotifLogQSOAddrs(const QString &addrs)
    {
        setParam("network/notif/log/qso/addrs", addrs);
    }

    static QString getNetworkNotifDXCSpotAddrs()
    {
        return getParam("network/notif/dxc/spot/addrs").toString();
    }

    static void setNetworkNotifDXCSpotAddrs(const QString &addrs)
    {
        setParam("network/notif/dxc/spot/addrs", addrs);
    }

    static QString getNetworkNotifWSJTXCQSpotAddrs()
    {
        return getParam("network/notif/wsjtx/cqspot/addrs").toString();
    }

    static void setNetworkNotifWSJTXCQSpotAddrs(const QString &addrs)
    {
        setParam("network/notif/wsjtx/cqspot/addrs", addrs);
    }

    static QString getNetworkNotifAlertsSpotAddrs()
    {
        return getParam("network/notif/alerts/spot/addrs").toString();
    }

    static void setNetworkNotifAlertsSpotAddrs(const QString &addrs)
    {
        setParam("network/notif/alerts/spot/addrs", addrs);
    }

    static QString getNetworkNotifRigStateAddrs()
    {
        return getParam("network/notif/rig/state/addrs").toString();
    }

    static void setNetworkNotifRigStateAddrs(const QString &addrs)
    {
        setParam("network/notif/rig/state/addrs", addrs);
    }

    static int getNetworkWsjtxListenerPort(int defaultPort)
    {
        return getParam("network/listener/wsjtx/port", defaultPort).toInt();
    }

    static void setNetworkNotifRigStateAddrs(int port)
    {
        setParam("network/listener/wsjtx/port", port);
    }

    static QString getNetworkWsjtxForwardAddrs()
    {
        return getParam("network/forwarder/wsjtx/addrs").toString();
    }

    static void setNetworkWsjtxForwardAddrs(const QString &addrs)
    {
        setParam("network/forwarder/wsjtx/addrs", addrs);
    }

    static bool getNetworkWsjtxListenerJoinMulticast()
    {
        return getParam("network/listener/wsjtx/multicast/join", false).toBool();
    }

    static void setNetworkWsjtxListenerJoinMulticast(bool state)
    {
        setParam("network/listener/wsjtx/multicast/join", state);
    }

    static QString getNetworkWsjtxListenerMulticastAddr()
    {
        return getParam("network/listener/wsjtx/multicast/addr").toString();
    }

    static void setNetworkWsjtxListenerMulticastAddr(const QString &addr)
    {
        setParam("network/listener/wsjtx/multicast/addr", addr);
    }

    static int getNetworkWsjtxListenerMulticastTTL()
    {
        return getParam("network/listener/wsjtx/multicast/ttl").toInt();
    }

    static void setNetworkWsjtxListenerMulticastTTL(int ttl)
    {
        setParam("network/listener/wsjtx/multicast/ttl", ttl);
    }

    static QStringList getEnabledMemberlists()
    {
        return getParamStringList("memberlist/enabledlists");
    }

    static void setEnabledMemberlists(const QStringList &list)
    {
        setParam("memberlist/enabledlists", list);
    }

private:
    static QCache<QString, QVariant> localCache;
    static QMutex cacheMutex;

    static bool setParam(const QString&, const QVariant &);
    static bool setParam(const QString&, const QStringList &);
    static QVariant getParam(const QString&, const QVariant &defaultValue = QVariant());
    static QStringList getParamStringList(const QString&, const QStringList &defaultValue = QStringList());
    static void removeParamGroup(const QString&);
    static QStringList getKeys(const QString &);
    static QString escapeString(const QString &input, QChar escapeChar = '\\', QChar delimiter = ',');
    static QString unescapeString(const QString &input, QChar escapeChar = '\\');
    static QString serializeStringList(const QStringList &list, QChar delimiter = ',', QChar escapeChar = '\\');
    static QStringList deserializeStringList(const QString &input, QChar delimiter = ',', QChar escapeChar = '\\');
};

#endif // QLOG_CORE_LOGPARAM_H
