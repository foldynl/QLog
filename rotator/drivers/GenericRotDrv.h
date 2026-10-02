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
    bool isAzimuthKnown() const { return azimuthKnown; }
    bool isElevationKnown() const { return elevationKnown; }

signals:
    void rotIsReady();
    void positioningChanged(double azimuth, double elevation);

    // Error Signal
    void errorOccurred(QString, QString);

protected:
    double toRotatorAzimuth(double azimuth) const;
    double fromRotatorAzimuth(double azimuth) const;

    RotProfile rotProfile;
    QString lastErrorText;
    bool opened;
    double azimuth;
    double elevation;
    // Set independently only after a successful report for the respective axis.
    bool azimuthKnown = false;
    bool elevationKnown = false;
};

#endif // QLOG_ROTATOR_DRIVERS_GENERICROTDRV_H
