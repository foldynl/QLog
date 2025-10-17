#ifndef RIG_DRIVERS_GENERICRIGDRV_H
#define RIG_DRIVERS_GENERICRIGDRV_H

#include <QObject>
#include "data/RigProfile.h"
#include "data/DxSpot.h"

class GenericRigDrv : public QObject
{
    Q_OBJECT
public:

    explicit GenericRigDrv(const RigProfile &profile,
                        QObject *parent = nullptr);
    virtual ~GenericRigDrv() {};
    const RigProfile getCurrRigProfile() const;
    const QString lastError() const;

    virtual bool open() = 0;
    virtual bool isMorseOverCatSupported() = 0;
    virtual QStringList getAvailableModes() = 0;

    virtual void setFrequency(double) = 0;
    virtual void setRawMode(const QString &) = 0;
    virtual void setMode(const QString &, const QString &, bool) = 0;
    virtual void setPTT(bool) = 0;
    virtual void setKeySpeed(qint16 wpm) = 0;
    virtual void syncKeySpeed(qint16 wpm) = 0;
    virtual void sendMorse(const QString &) = 0;
    virtual void stopMorse() = 0;
    virtual void sendState() = 0;
    virtual void stopTimers() = 0;
    virtual void sendDXSpot(const DxSpot &spot) = 0;

signals:
    // STATE Signals
    void rigIsReady();
    void frequencyChanged(double, double, double);
    void pttChanged(bool);
    void modeChanged(QString, QString, QString, qint32);
    void vfoChanged(QString);
    void powerChanged(double);
    void ritChanged(double);
    void xitChanged(double);
    void keySpeedChanged(unsigned int);

    // Error Signal
    void errorOccurred(QString, QString);

protected:
    RigProfile rigProfile;
    QString lastErrorText;
    bool opened;
};

#endif // RIG_DRIVERS_GENERICRIGDRV_H
