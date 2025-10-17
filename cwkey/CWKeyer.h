#ifndef QLOG_CWKEY_CWKEYER_H
#define QLOG_CWKEY_CWKEYER_H

#include <QObject>

#include "cwkey/drivers/CWKey.h"
#include "data/CWKeyProfile.h"

class CWKeyer : public QObject
{
    Q_OBJECT

public:
    static CWKeyer* instance()
    {
        static CWKeyer instance;
        return &instance;
    };
    void stopTimer();

signals:
    void cwKeyerError(QString, QString);
    void cwKeyConnected(QString);
    void cwKeyDisconnected();
    void cwKeyWPMChanged(qint32);
    void cwKeyEchoText(QString);
    void cwKeyHWButton(int);
    void cwKeyHWHaltPressed();

public slots:
    void start();
    void update();
    void open();
    void close();
    bool canStopSending();
    bool canEchoChar();
    bool rigMustConnected();
    bool canSetSpeed();

    void setSpeed(const qint16 wpm);
    void sendText(const QString&);
    void immediatelyStop();

private slots:
    void openImpl();
    void closeImpl();
    void setSpeedImpl(const qint16 wpm);
    void sendTextImpl(const QString&);
    void immediatelyStopImpl();
    void stopTimerImplt();
    void keyErrorHandler(const QString&, const QString&);
    void cwKeyWPMChangedHandler(qint32);
    void cwKeyEchoTextHandler(const QString&);
    void cwKeyHWButton1PressedHandler();
    void cwKeyHWButton2PressedHandler();
    void cwKeyHWButton3PressedHandler();
    void cwKeyHWButton4PressedHandler();

private:
    explicit CWKeyer(QObject *parent = nullptr);
    ~CWKeyer();

    void __closeCWKey();
    void __openCWKey();

    CWKey *cwKey;
    CWKeyProfile connectedCWKeyProfile;
    QMutex cwKeyLock;
    QTimer* timer;
};

#endif // QLOG_CWKEY_CWKEYER_H
