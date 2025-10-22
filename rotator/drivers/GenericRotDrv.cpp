#include "GenericRotDrv.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.rot.driver.genericrotdrv");

GenericRotDrv::GenericRotDrv(const RotProfile &profile,
                             QObject *parent)
    : QObject{parent},
      rotProfile(profile),
      opened(false),
      azimuth(0.0),
      elevation(0.0)
{
    FCT_IDENTIFICATION;
}

const RotProfile GenericRotDrv::getCurrRotProfile() const
{
    FCT_IDENTIFICATION;

    return rotProfile;
}

const QString GenericRotDrv::lastError() const
{
    FCT_IDENTIFICATION;

    return lastErrorText;
}

double GenericRotDrv::normalizeAzimuth(double azimuth) const
{
    FCT_IDENTIFICATION;

    /* This function takes any azimuth value (in degrees), including
     * negative values or values greater than 360, and returns an
     * equivalent angle normalized to the range [0, 360).
     */

    double normalized = fmod(azimuth, 360.0);

    if ( normalized < 0.0 ) normalized += 360.0;

    qCDebug(runtime) << "Input azimuth:" << azimuth
                     << "Normalized azimuth:" << normalized;

    return normalized;
}
