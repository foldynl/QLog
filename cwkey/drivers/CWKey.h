#ifndef QLOG_CWKEY_DRIVERS_CWKEY_H
#define QLOG_CWKEY_DRIVERS_CWKEY_H

#include <QObject>
#include <QtSerialPort>
#include <QUdpSocket>

class CWKeySerialInterface
{
public:
    explicit CWKeySerialInterface(const QString &portName,
                                  const qint32 baudrate,
                                  const qint32 timeout);
    ~CWKeySerialInterface() {};

protected:

    virtual qint64 sendDataAndWait(const QByteArray &data);
    virtual qint64 receiveDataAndWait(QByteArray &data);
    virtual qint64 writeAsyncData(const QByteArray &);

    QSerialPort serial;
    qint32 timeout;
    QMutex portMutex;
};
class CWKeyIPInterface
{
public:
    explicit CWKeyIPInterface(const QString &hostname,
                               const quint16 port);
    ~CWKeyIPInterface() {};

protected:
    virtual qint64 sendData(const QByteArray &data) = 0;
    virtual bool isSocketReady() = 0;

    QHostAddress serverName;
    quint16 port;
    bool hasIPAddress;
};

class CWKeyUDPInterface : public CWKeyIPInterface
{
public:
    explicit CWKeyUDPInterface(const QString &hostname,
                               const quint16 port);
    ~CWKeyUDPInterface() {};

protected:
    virtual qint64 sendData(const QByteArray &data) override;
    virtual bool isSocketReady() override;

    QUdpSocket socket;
};

class CWKey : public QObject
{
    Q_OBJECT;

public:
    enum CWKeyTypeID
    {
        DUMMY_KEYER = 0,
        WINKEY_KEYER = 1,
        MORSEOVERCAT = 2,
        CWDAEMON_KEYER = 3,
        FLDIGI_KEYER = 4,
        LAST_MODEL = 4
    };

    enum CWKeyModeID
    {
        SINGLE_PADDLE = 0,
        IAMBIC_A = 1,
        IAMBIC_B = 2,
        ULTIMATE = 3,
        LAST_MODE = 4
    };

signals:
    void keyError(QString, QString);
    void keyChangedWPMSpeed(qint32);
    void keyEchoText(QString);
    void keyHWButton1Pressed();
    void keyHWButton2Pressed();
    void keyHWButton3Pressed();
    void keyHWButton4Pressed();

public:
    explicit CWKey(CWKeyModeID mode, qint32 defaultWPM, QObject *parent = nullptr);
    virtual ~CWKey() {};

    virtual bool open() = 0;
    virtual bool close() = 0;
    virtual bool sendText(const QString &text) = 0;
    virtual bool setWPM(const qint16 wpm) = 0;
    virtual QString lastError() = 0;
    virtual bool immediatelyStop() = 0;
    virtual bool canStopSending() { return stopSendingCap;}
    virtual bool canEchoChar() { return echoCharsCap;}
    virtual bool mustRigConnected() { return rigMustConnectedCap;}
    virtual bool canSetSpeed() { return canSetKeySpeed; };

    void printKeyCaps();

    static CWKeyTypeID intToTypeID(int);
    static CWKeyModeID intToModeID(int);
    static bool isNetworkKey(const CWKeyTypeID &type);

    friend QDataStream& operator<<(QDataStream& out, const CWKeyTypeID& v);
    friend QDataStream& operator>>(QDataStream& in, CWKeyTypeID& v);

    friend QDataStream& operator<<(QDataStream& out, const CWKeyModeID& v);
    friend QDataStream& operator>>(QDataStream& in, CWKeyModeID& v);

protected:
    CWKeyModeID keyMode;
    qint32 defaultWPMSpeed;

    bool stopSendingCap;
    bool echoCharsCap;
    bool rigMustConnectedCap;
    bool canSetKeySpeed;
};

#endif // QLOG_CWKEY_DRIVERS_CWKEY_H
