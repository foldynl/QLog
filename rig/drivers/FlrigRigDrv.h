#ifndef RIG_DRIVERS_FLRIGRIGDRV_H
#define RIG_DRIVERS_FLRIGRIGDRV_H

#include "GenericRigDrv.h"
#include "rig/RigCaps.h"

class FlrigRigDrv : public GenericRigDrv
{
    Q_OBJECT

public:
    static RigCaps getCaps(int);
    static QList<QPair<int, QString>> getModelList();

    explicit FlrigRigDrv(const RigProfile &profile,
                         QObject *parent = nullptr);
    virtual ~FlrigRigDrv();

    virtual bool open() override;
    virtual bool isMorseOverCatSupported() override;
    virtual QStringList getAvailableModes() override;

    virtual void setFrequency(double) override;
    virtual void setRawMode(const QString &) override;
    virtual void setMode(const QString &, const QString &, bool) override;
    virtual void setPTT(bool) override;
    virtual void setKeySpeed(qint16 wpm) override;
    virtual void syncKeySpeed(qint16 wpm) override;
    virtual void sendMorse(const QString &) override;
    virtual void stopMorse() override;
    virtual void sendState() override;
    virtual void stopTimers() override;
    virtual void sendDXSpot(const DxSpot &spot) override;

private slots:
    void startRigStatePoll();
    void reqGET_MODES();
    void reqCWIO_SEND(bool);
    void reqGET_VFO();
    void reqGET_MODE();
    void reqGET_BW();
    void reqGET_POWER();
    void reqGET_AB();
    void reqGET_PTT();
    void reqCWIO_GET_WPM();

private:
    struct RigMode
    {
        QString mode;
        QString submode;
        bool digiMode;
    };

    QHash<QString, RigMode> raw2ADIFModeMapping;
    QHash<QString, RigMode> rigAvailableModes;
    typedef void (FlrigRigDrv::*responseHandler)(const QVariant&);

    void resetCurrStates();
    void handleError(const QString &category, const QString &errorMsg);
    void sendXmlRpcCommand(const QString &method, const QList<QVariant> &params = {},
                           bool emitError = true);
    QVariantList parseArray(QXmlStreamReader &reader);
    QVariant parseSingleValue(QXmlStreamReader &reader);
    QVariantMap parseStruct(QXmlStreamReader &reader);
    QVariant parseValueFromResponse(const QByteArray &xml, bool *ok, QString &errorMessage);
    void buildModeMappingHash();
    void handleXmlRpcResponse(QNetworkReply *reply, const QString &method);
    const QString getModeNormalizedText(const QString& rawMode, QString &submode) const;
    const QString mode2RawMode(const QString &mode, const QString &submode, bool digiVariant) const;

    void rspGET_MODES(const QVariant &value);
    void rspGET_VFO(const QVariant &value);
    void rspGET_MODE(const QVariant &value);
    void rspGET_BW(const QVariant &value);
    void rspGET_POWER(const QVariant &value);
    void rspGET_AB(const QVariant &value);
    void rspGET_PTT(const QVariant &value);
    void rspCWIO_GET_WPM(const QVariant &value);

    QNetworkAccessManager *networkManager;
    double currFreq;
    QString currVFO;
    QString currRawMode;
    qint32 currBW;
    qint32 currKeySpeed;
    qlonglong currPWR;
    char currPTT;
    bool rigReady;
    QUrl hostUrl;
    QList<QTimer*> runningTimers;
    bool sendTextFlag;

    const QHash<QString, FlrigRigDrv::responseHandler> responseHandlers =
    {
        {"rig.get_modes", &FlrigRigDrv::rspGET_MODES},
        {"rig.get_vfo", &FlrigRigDrv::rspGET_VFO},
        {"rig.get_mode", &FlrigRigDrv::rspGET_MODE},
        {"rig.get_bw", &FlrigRigDrv::rspGET_BW},
        {"rig.get_power", &FlrigRigDrv::rspGET_POWER},
        {"rig.get_AB", &FlrigRigDrv::rspGET_AB},
        {"rig.get_ptt", &FlrigRigDrv::rspGET_PTT},
        {"rig.cwio_get_wpm", &FlrigRigDrv::rspCWIO_GET_WPM}
    };

    const uint RESPONSE_TIMEOUT = 5000; // in secs
};

#endif // RIG_DRIVERS_FLRIGRIGDRV_H
