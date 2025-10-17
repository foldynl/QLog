#ifndef QLOG_CWKEY_DRIVERS_CWDAEMONKEY_H
#define QLOG_CWKEY_DRIVERS_CWDAEMONKEY_H

#include <QObject>
#include "CWKey.h"

class CWDaemonKey : public CWKey,
                    protected CWKeyUDPInterface
{
    Q_OBJECT

public:
    explicit CWDaemonKey(const QString &hostname,
                         const quint16 port,
                         const CWKey::CWKeyModeID mode,
                         const qint32 defaultSpeed,
                         QObject *parent = nullptr);
    virtual ~CWDaemonKey(){};

    virtual bool open() override;
    virtual bool close() override;
    virtual QString lastError() override;

    virtual bool sendText(const QString &text) override;
    virtual bool setWPM(const qint16 wpm) override;
    virtual bool immediatelyStop() override;

protected:
    QString lastLogicalError;
    bool isOpen;
    const QChar ESCChar;
};

#endif // QLOG_CWKEY_DRIVER_CWDAEMONKEY_H
