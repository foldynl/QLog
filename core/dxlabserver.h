#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QLocale>
#include <rig/Rig.h>

class Rig; // forward

class DxlabServer : public QObject {
    Q_OBJECT
public:
    explicit DxlabServer(Rig *rig, QObject *parent = nullptr);
    ~DxlabServer() override;

    bool start(quint16 port = 52002);   // Commander default: base=52000, Commander=+2
    void stop();
    bool isRunning() const { return _srv.isListening(); }

signals:
    void info(const QString&);
    void warn(const QString&);

private slots:
    void onNewConn();
    void onReady();
    void onGone();

public slots:
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
    struct Inflight {
        QString name;
        QHash<QString, QByteArray> params;
        bool sawParameters = false;
    };
    struct ClientState {
        QByteArray rx;
        Inflight inflight;
        QTimer flushTimer;  // short timer to finalize commands that have no <parameters:...>
    };
    struct Token { QString name; int len = 0; QByteArray val; };

    // parsing
    static bool extractOneToken(QByteArray &buffer, Token &tok);
    void handleToken(QTcpSocket *sock, ClientState &st, const Token &tok);

    // dispatch
   // void handleCommand(QTcpSocket *sock, ClientState &st, const QString &cmd);
   // void handleParam(QTcpSocket *sock, ClientState &st, const Token &tok);
    void processInflight(QTcpSocket *sock, Inflight &in);

    // replies
    void replyRxFreqStd(QTcpSocket *s);     // CmdGetFreq   (comma thousands + period decimal)
    void replyTxFreqStd(QTcpSocket *s);     // CmdGetTXFreq
    void replyMode(QTcpSocket *s);          // CmdSendMode
    void replySplit(QTcpSocket *s);         // CmdSendSplit
    void replyPTT(QTcpSocket *s);           // CmdSendTX
    static void sendToken(QTcpSocket *sock, const QString &name, const QByteArray &value);

    // helpers
    static bool parseKhzToHz(const QByteArray &s, qint64 &outHz);   // accepts “14,080.055” or “14080.055”
    static bool isValidMode(const QString &m);
    static QString normMode(const QString &m);
    void buildModeMappingHash();

private:
    struct RigMode
    {
        QString mode;
        QString submode;
        bool digiMode;
    };

    QTcpServer _srv;
    Rig *_rig = nullptr;
    QLocale _loc{QLocale::system()};
    QString _qsxMode{"USB"};
    double currFrequency = 0;
    double currRIT = 0;
    double currXIT = 0;
    QString rawModeText;
    QString currMode;
    QString currSubMode;
    qint32 currPBWidth;
    double currPower;
    QString currVFO;
    bool currPTT;
    bool rigOnline;
    QHash<QString, RigMode> raw2ADIFModeMapping;
};
