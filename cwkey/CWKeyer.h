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
    void imediatellyStop();

private slots:
    void openImpl();
    void closeImpl();
    void setSpeedImpl(const qint16 wpm);
    void sendTextImpl(const QString&);
    void immediatellyStopImpl();
    void stopTimerImplt();
    void keyErrorHandler(const QString&, const QString&);
    void cwKeyWPMChangedHandler(qint32);
    void cwKeyEchoTextHandler(const QString&);

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
