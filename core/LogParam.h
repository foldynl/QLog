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

private:
    static QCache<QString, QVariant> localCache;
    static QMutex cacheMutex;

    static bool setParam(const QString&, const QVariant &);
    static QVariant getParam(const QString&, const QVariant &defaultValue = QVariant());
    static void removeParamGroup(const QString&);
    static QStringList getKeys(const QString &);
};

#endif // QLOG_CORE_LOGPARAM_H
