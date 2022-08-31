#ifndef CWDUMMYKEY_H
#define CWDUMMYKEY_H

#include "CWKey.h"

class CWDummyKey : public CWKey
{
public:
    explicit CWDummyKey(QObject *parent = nullptr);

    virtual bool open() override;
    virtual bool close() override;
    virtual bool sendText(QString text) override;
    virtual bool setWPM(qint16 wpm) override;
    virtual bool setMode(CWKeyModeID mode) override;
    virtual bool isConnected();

private:
    bool isUsed;
};

#endif // CWDUMMYKEY_H
