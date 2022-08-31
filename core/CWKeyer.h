#ifndef CWKEYER_H
#define CWKEYER_H

#include <QObject>

#include "core/CWKey.h"

class CWKeyer : public QObject
{
    Q_OBJECT

public:
    static CWKeyer* instance();
    void stopTimer();

signals:
    void cwKeyerError(QString, QString);
    void cwKeyConnected();
    void cwKeyDisconnected();

public slots:
    void start();
    void update();
    void open();
    void close();

    void setMode(const CWKey::CWKeyModeID);
    void setSpeed(const qint16 wpm);
    void sendText(const QString&);

private slots:
    void openImpl();
    void closeImpl();
    void setModeImpl(const CWKey::CWKeyModeID);
    void setSpeedImpl(const qint16 wpm);
    void sendTextImpl(const QString&);
    void stopTimerImplt();

private:
    explicit CWKeyer(QObject *parent = nullptr);
    ~CWKeyer();

    void __closeCWKey();
    void __openCWKey();

    CWKey *cwKey;
    CWKeyProfile connectedRigProfile;
    QMutex cwKeyLock;
    QTimer* timer;
};

#endif // CWKEYER_H
