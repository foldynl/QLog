#ifndef QLOG_ROTATOR_DRIVERS_HAMLIBROTDRV_H
#define QLOG_ROTATOR_DRIVERS_HAMLIBROTDRV_H

#include <QTimer>
#include <hamlib/rotator.h>
#include "GenericRotDrv.h"
#include "rotator/RotCaps.h"

class HamlibRotDrv : public GenericRotDrv
{
public:
    static QList<QPair<int, QString>> getModelList();
    static RotCaps getCaps(int model);

    explicit HamlibRotDrv(const RotProfile &profile,
                       QObject *parent = nullptr);

    virtual ~HamlibRotDrv();

    virtual bool open() override;
    virtual void sendState() override;
    virtual void setPosition(double azimuth, double elevation) override;
    virtual void stopTimers() override;

private slots:
    void checkRotStateChange();
    void checkErrorCounter();

private:
    static int addRig(const struct rot_caps* caps, void* data);

    void checkChanges();
    void checkAzEl();
    bool isRotRespOK(int errorStatus,
                     const QString errorName,
                     bool emitError = true);

    serial_handshake_e stringToHamlibFlowControl(const QString &in_flowcontrol);
    serial_parity_e stringToHamlibParity(const QString &in_parity);
    QString hamlibErrorString(int);
    void commandSleep();

    ROT* rot;
    QTimer timer;
    QTimer errorTimer;
    QMutex drvLock;

    bool forceSendState;
    QHash<QString, QString>postponedErrors;
};

#endif // QLOG_ROTATOR_DRIVERS_HAMLIBROTDRV_H
