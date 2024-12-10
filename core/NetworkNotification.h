#ifndef QLOG_CORE_NETWORKNOTIFICATION_H
#define QLOG_CORE_NETWORKNOTIFICATION_H

#include <QObject>
#include <QSqlRecord>
#include <QJsonObject>
#include <QJsonDocument>
#include "core/HostsPortString.h"
#include "logformat/LogFormat.h"
#include "data/DxSpot.h"
#include "data/WsjtxEntry.h"
#include "data/SpotAlert.h"
#include "data/WCYSpot.h"
#include "data/WWVSpot.h"
#include "data/ToAllSpot.h"
#include "rig/Rig.h"

class GenericNotificationMsg : public QObject
{
    Q_OBJECT

public:
    explicit GenericNotificationMsg(QObject *parent = nullptr);
    QByteArray getJson() const  { return QJsonDocument(msg).toJson(QJsonDocument::Compact); };

protected:
    QJsonObject msg;
};

class QSONotificationMsg : public GenericNotificationMsg
{

public:
    enum QSOOperation
    {
        QSO_INSERT = 0,
        QSO_UPDATE = 1,
        QSO_DELETE = 2
    };

    explicit QSONotificationMsg(const QSqlRecord&,
                                const QSOOperation,
                                QObject *parent = nullptr);

private:
    QMap<int, QString> QSOOperation2String = {
        {QSOOperation::QSO_INSERT, "insert"},
        {QSOOperation::QSO_UPDATE, "update"},
        {QSOOperation::QSO_DELETE, "delete"},
    };
};

class GenericSpotNotificationMsg : public GenericNotificationMsg
{

public:
    explicit GenericSpotNotificationMsg(QObject *parent = nullptr);

protected:
    QMap<int, QString> DxccStatus2String = {
        {DxccStatus::NewEntity, "newentity"},
        {DxccStatus::NewBandMode, "newbandmode"},
        {DxccStatus::NewBand, "newband"},
        {DxccStatus::NewMode, "newmode"},
        {DxccStatus::NewSlot, "newslot"},
        {DxccStatus::Worked, "worked"},
        {DxccStatus::Confirmed, "confirmed"},
        {DxccStatus::UnknownStatus, "unknown"},
    };
};

class DXSpotNotificationMsg : public GenericSpotNotificationMsg
{

public:
    explicit DXSpotNotificationMsg(const DxSpot&, QObject *parent = nullptr);
};

class WSJTXCQSpotNotificationMsg : public GenericSpotNotificationMsg
{

public:
    explicit WSJTXCQSpotNotificationMsg(const WsjtxEntry&, QObject *parent = nullptr);

};

class SpotAlertNotificationMsg : public GenericSpotNotificationMsg
{

public:
    explicit SpotAlertNotificationMsg(const SpotAlert&, QObject *parent = nullptr);

};

class WCYSpotNotificationMsg : public GenericNotificationMsg
{

public:
    explicit WCYSpotNotificationMsg(const WCYSpot&, QObject *parent = nullptr);

};

class WWVSpotNotificationMsg : public GenericNotificationMsg
{

public:
    explicit WWVSpotNotificationMsg(const WWVSpot&, QObject *parent = nullptr);

};

class ToAllSpotNotificationMsg : public GenericNotificationMsg
{

public:
    explicit ToAllSpotNotificationMsg(const ToAllSpot&, QObject *parent = nullptr);

};

class RadioNotificationMsg : public GenericNotificationMsg
{

public:
    explicit RadioNotificationMsg(const double,const QString,const QString,const QString,const QString,const QString,const QString,const QString,const QString, QObject *parent = nullptr);

};

class NetworkNotification : public QObject
{
    Q_OBJECT
public:
    explicit NetworkNotification(QObject *parent = nullptr);

    static QString getNotifQSOAdiAddrs();
    static void saveNotifQSOAdiAddrs(const QString &);
    static QString getNotifDXSpotAddrs();
    static void saveNotifDXSpotAddrs(const QString &);
    static QString getNotifWSJTXCQSpotAddrs();
    static void saveNotifWSJTXCQSpotAddrs(const QString &);
    static QString getNotifSpotAlertAddrs();
    static void saveNotifSpotAlertAddrs(const QString &);
    static QString getNotifRadioAlertAddrs();
    static void saveNotifRadioAlertAddrs(const QString &);

public slots:
    void QSOInserted(const QSqlRecord &);
    void QSOUpdated(const QSqlRecord &);
    void QSODeleted(const QSqlRecord &);
    void dxSpot(const DxSpot&);
    void wcySpot(const WCYSpot&);
    void wwvSpot(const WWVSpot&);
    void toAllSpot(const ToAllSpot&);
    void WSJTXCQSpot(const WsjtxEntry&);
    void spotAlert(const SpotAlert&);
    // to receive RIG instructions
    void updateFrequency(VFOID, double, double, double);
    void updateMode(VFOID, const QString&, const QString&,
                    const QString&, qint32);
    void updatePWR(VFOID, double);
    void updateVFO(VFOID, const QString&);
    void updateXIT(VFOID, double);
    void updateRIT(VFOID, double);
    void updatePTT(VFOID, bool);
    void rigConnected();
    void rigDisconnected();

private:

    void send(const QByteArray &, const HostsPortString &);

    static QString CONFIG_NOTIF_QSO_ADI_ADDRS_KEY;
    static QString CONFIG_NOTIF_DXSPOT_ADDRS_KEY;
    static QString CONFIG_NOTIF_WSJTXCQSPOT_ADDRS_KEY;
    static QString CONFIG_NOTIF_SPOTALERT_ADDRS_KEY;
    static QString CONFIG_NOTIF_RADIO_ADDRS_KEY;
    double lastSeenFreq;
    QString lastSeenMode;
    QString lastSeenSubMode;
    QString lastSeenPWR;
    QString lastSeenRIT;
    QString lastSeenXIT;
    QString lastSeenPTT;
    QString lastSeenVFO;
    bool rigOnline;
    void resetRigInfo();
    void rigUpdated();
    GenericNotificationMsg RigNotificationMsg();
    void updateRadio();
};

#endif // QLOG_CORE_NETWORKNOTIFICATION_H
