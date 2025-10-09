#ifndef QLOG_ROTATOR_DRIVERS_GENERICROTDRV_H
#define QLOG_ROTATOR_DRIVERS_GENERICROTDRV_H

#include <QObject>
#include "data/RotProfile.h"

class GenericRotDrv : public QObject
{
    Q_OBJECT

public:
    explicit GenericRotDrv(const RotProfile &profile,
                           QObject *parent = nullptr);

    virtual ~GenericRotDrv() {};
    const RotProfile getCurrRotProfile() const;
    const QString lastError() const;

    virtual bool open() = 0;
    virtual void sendState() = 0;
    virtual void setPosition(double azimuth, double elevation) = 0;
    virtual void stopTimers() = 0;
    double normalizeAzimuth(double azimuth) const;

signals:
    void rotIsReady();
    void positioningChanged(double azimuth, double elevation);

    // Error Signal
    void errorOccured(QString, QString);

protected:
    RotProfile rotProfile;
    QString lastErrorText;
    bool opened;
    double azimuth;
    double elevation;
};

#endif // QLOG_ROTATOR_DRIVERS_GENERICROTDRV_H
