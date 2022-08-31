#ifndef CWKEY_H
#define CWKEY_H

#include <QObject>
#include <QtCore>
#include <QtSerialPort>

#include <hamlib/rig.h>
#include "data/CWKeyProfile.h"
#include "core/SerialPort.h"

class CWKey : public QObject
{
    Q_OBJECT

public:
    enum CWKeyModelID
    {
        DUMMY_KEYER = 0,
        K3NG_WITH_CLI_KEYER = 1,
        WINKEY2_KEYER = 2
    };

    enum CWKeyModeID
    {
        SINGLE_PADDLE = 0,
        IAMBIC_A = 1,
        IAMBIC_B = 2,
        ULTIMATE = 3
    };

public:
    explicit CWKey(QObject *parent = nullptr);
    ~CWKey() {};

    virtual bool open() = 0;
    virtual bool close() = 0;
    virtual bool sendText(QString text) = 0;
    virtual bool setWPM(qint16 wpm) = 0;
    virtual bool setMode(CWKeyModeID mode) = 0;
    virtual QString lastError() = 0;
    //virtual bool imediatellyStop() = 0;

};

#endif // CWKEY_H
