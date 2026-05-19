#include "GenericRigDrv.h"
#include "core/debug.h"


MODULE_IDENTIFICATION("qlog.core.rig.driver.genericrigdrv");

GenericRigDrv::GenericRigDrv(const RigProfile &profile, QObject *parent)
    : QObject{parent},
      rigProfile(profile),
      opened(false)
{
    FCT_IDENTIFICATION;

}

const RigProfile GenericRigDrv::getCurrRigProfile() const
{
    FCT_IDENTIFICATION;

    return rigProfile;
}

const QString GenericRigDrv::lastError() const
{
    FCT_IDENTIFICATION;

    return lastErrorText;
}

void GenericRigDrv::setFrequency(VFOID vfoid, double freq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << vfoid << freq;

    // Default: VFO1 delegates to the single-frequency setter, VFO2 is ignored
    if ( vfoid == VFO1 )
        setFrequency(freq);
}

void GenericRigDrv::setSplit(bool)
{
    FCT_IDENTIFICATION;

    // Default: do nothing — driver does not support split
}
