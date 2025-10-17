#ifndef QLOG_CWKEY_DRIVERS_CWDUMMYKEY_H
#define QLOG_CWKEY_DRIVERS_CWDUMMYKEY_H

#include "CWKey.h"

class CWDummyKey : public CWKey
{
    Q_OBJECT

public:
    explicit CWDummyKey(QObject *parent = nullptr);

    virtual bool open() override;
    virtual bool close() override;
    virtual bool sendText(const QString &text) override;
    virtual bool setWPM(const qint16 wpm) override;
    virtual QString lastError() override;
    virtual bool immediatelyStop() override;

private:
    bool isUsed;
};

#endif // QLOG_CWKEY_DRIVER_CWDUMMYKEY_H
