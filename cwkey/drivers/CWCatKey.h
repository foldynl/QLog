#ifndef QLOG_CWKEY_DRIVERS_CWCATKEY_H
#define QLOG_CWKEY_DRIVERS_CWCATKEY_H

#include <QObject>
#include <QMutex>
#include "CWKey.h"
#include "rig/Rig.h"

class CWCatKey : public CWKey
{

    Q_OBJECT

public:
    explicit CWCatKey(const CWKey::CWKeyModeID mode,
                      const qint32 defaultSpeed,
                      QObject *parent = nullptr);
    virtual ~CWCatKey();

    virtual bool open() override;
    virtual bool close() override;
    virtual QString lastError() override;

    virtual bool sendText(const QString &text) override;
    virtual bool setWPM(const qint16 wpm) override;
    virtual bool immediatelyStop() override;

private:
    QMutex commandMutex;
    bool isKeyConnected;
    QString lastErrorText; //user only in open part of communication

    void __close();

private slots:
    void rigKeySpeedChanged(VFOID, unsigned int);
};

#endif // QLOG_CWKEY_DRIVERS_CWCATKEY_H
