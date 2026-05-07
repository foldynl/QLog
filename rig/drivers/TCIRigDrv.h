#ifndef RIG_DRIVERS_TCIRIGDRV_H
#define RIG_DRIVERS_TCIRIGDRV_H

#include <QtWebSockets>
#include <QHash>
#include "GenericRigDrv.h"
#include "rig/RigCaps.h"

class TCIRigDrv : public GenericRigDrv
{
    Q_OBJECT

public:
    static QList<QPair<int, QString>> getModelList();
    static RigCaps getCaps(int);

    explicit TCIRigDrv(const RigProfile &profile,
                    QObject *parent = nullptr);
    virtual ~TCIRigDrv();

    virtual bool open() override;
    virtual bool isMorseOverCatSupported() override;
    virtual QStringList getAvailableModes() override;

    virtual void setFrequency(double) override;
    virtual void setFrequency(VFOID, double) override;
    virtual void setSplit(bool) override;
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
    void onConnected();
    void onTextMessageReceived(const QString& message);
    void onSocketError(QAbstractSocket::SocketError socker_error);
private:

    typedef void (TCIRigDrv::*parseFce)(const QStringList&);

    void sendCmd(const QString &cmd,
                 bool addRigID,
                 const QStringList &args = QStringList());
    const QString getModeNormalizedText(const QString& rawMode, QString &submode);
    const QString mode2RawMode(const QString &mode, const QString &submode, bool digiVariant);

    // commands functions
    void rspPROTOCOL(const QStringList &);
    void rspREADY(const QStringList &);
    void rspSTART(const QStringList &);
    void rspSTOP(const QStringList &);
    void rspRECEIVE_ONLY(const QStringList &);
    void rspMODULATIONS_LIST(const QStringList &);
    void rspVFO(const QStringList &);
    void rspTRX(const QStringList &);
    void rspMODULATION(const QStringList &);
    void rspTUNE_DRIVE(const QStringList &);
    void rspDRIVE(const QStringList &);
    void rspRIT_OFFSET(const QStringList &);
    void rspXIT_OFFSET(const QStringList &);
    void rspCW_MACROS_SPEED(const QStringList &);
    void rspRIT_ENABLE(const QStringList &);
    void rspXIT_ENABLE(const QStringList &);
    void rspSPLIT_ENABLE(const QStringList &);

    double getRITFreq();
    void setRITFreq(double);
    double getXITFreq();
    void setXITFreq(double);
    double getRawRIT();
    double getRawXIT();

    QWebSocket ws;
    bool ready;
    bool receivedOnly;
    QStringList modeList;
    double currFreq;
    double currTxFreq;
    QString currMode;
    double currRIT;
    double currXIT;
    bool RITEnabled;
    bool XITEnabled;
    bool currSplitEnabled;

    const QHash<QString, TCIRigDrv::parseFce> responseParsers =
    {
        {"protocol", &TCIRigDrv::rspPROTOCOL},
        {"ready", &TCIRigDrv::rspREADY},
        {"start", &TCIRigDrv::rspSTART},
        {"stop", &TCIRigDrv::rspSTOP},
        {"receive_only", &TCIRigDrv::rspRECEIVE_ONLY},
        {"modulations_list", &TCIRigDrv::rspMODULATIONS_LIST},
        {"vfo", &TCIRigDrv::rspVFO},
        {"trx", &TCIRigDrv::rspTRX},
        {"modulation", &TCIRigDrv::rspMODULATION},
        {"tune_drive", &TCIRigDrv::rspTUNE_DRIVE},
        {"drive", &TCIRigDrv::rspDRIVE},
        {"rit_offset", &TCIRigDrv::rspRIT_OFFSET},
        {"xit_offset", &TCIRigDrv::rspXIT_OFFSET},
        {"cw_macros_speed", &TCIRigDrv::rspCW_MACROS_SPEED},
        {"rit_enable", &TCIRigDrv::rspRIT_ENABLE},
        {"xit_enable", &TCIRigDrv::rspXIT_ENABLE},
        {"split_enable", &TCIRigDrv::rspSPLIT_ENABLE},
    };
};

#endif // RIG_DRIVERS_TCIRIGDRV_H
