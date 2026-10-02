#include "RotatorRuntimeProvider.h"
#include "core/debug.h"
#include <QtMath>

MODULE_IDENTIFICATION("qlog.rotator.rotatorruntimeprovider");

RuntimeProvider *createRotatorRuntimeProvider(QObject *owner, const Rotator::Status &status)
{
    FCT_IDENTIFICATION;

    auto provider = new RuntimeProvider(owner);
    provider->addBoolean("rotator_connected",
                         QStringLiteral("Whether QLog's rotator-control driver has a ready connection. "
                                        "For PSTRotator this means the UDP interface is open, not a "
                                        "verified hardware connection. Does not indicate that the "
                                        "antenna is moving or has reached a target. False means no "
                                        "ready driver connection; null if collection failed."),
                         [&status]() { return QJsonValue(status.isConnected); });

    provider->addString("rotator_profile",
                        QStringLiteral("Name of the rotator-control profile used by the connected "
                                       "driver (device and connection settings). Not an antenna, "
                                       "radio or logging-station profile. Null when disconnected "
                                       "or no profile name is known."),
                        [&status]()
                        {
                            return status.isConnected && !status.profile.isEmpty() ? QJsonValue(status.profile)
                                                                                   : QJsonValue();
                        });

    provider->addString("rotator_backend",
                        QStringLiteral("Rotator-control backend used by QLog's connected driver: "
                                       "Hamlib or PSTRotator. Identifies the software interface, "
                                       "not the physical rotator model. Null when disconnected "
                                       "or unknown."),
                        [&status]()
                        {
                            return status.isConnected && !status.backend.isEmpty() ? QJsonValue(status.backend)
                                                                                   : QJsonValue();
                        });

    provider->addString("rotator_model",
                        QStringLiteral("Configured Hamlib rotator-driver label: manufacturer, "
                                       "model and driver version in parentheses. This is the "
                                       "selected driver, not a hardware identity or firmware "
                                       "version verified by querying the rotator. Null when "
                                       "disconnected, unknown, using PSTRotator, or using NET "
                                       "rotctl without a known underlying model."),
                        [&status]()
                        {
                            return status.isConnected && !status.modelName.isEmpty() ? QJsonValue(status.modelName)
                                                                                     : QJsonValue();
                        });

    provider->addNumber("rotator_azimuth",
                        QStringLiteral("Latest antenna azimuth reported to QLog by the rotator driver, "
                                       "in degrees clockwise from north: 0 north, 90 east, 180 south, "
                                       "270 west; range [0, 360). Includes the antenna-profile azimuth "
                                       "offset already applied by the driver; do not add it again. "
                                       "This is a reported position, not a requested target, bearing "
                                       "to a contact, or proof that movement has stopped. Null when "
                                       "disconnected or no valid azimuth report has arrived for this "
                                       "connection. Zero is a valid reported direction."), "deg",
                        [&status]()
                        {
                            return status.isConnected && status.azimuthKnown && qIsFinite(status.azimuth)
                                   && status.azimuth >= 0.0 && status.azimuth < 360.0
                                   ? QJsonValue(status.azimuth) : QJsonValue();
                        });

    provider->addNumber("rotator_elevation",
                        QStringLiteral("Latest elevation reported to QLog by the rotator driver, "
                                       "in degrees: 0 is the horizon, 90 is straight up. Mechanical "
                                       "ranges depend on the device; negative or over-90 values "
                                       "can be valid. Not a requested target or proof that movement "
                                       "has stopped. Azimuth and elevation may arrive separately. "
                                       "Null when disconnected or no valid elevation report has "
                                       "arrived for this connection. A reported zero does not "
                                       "prove that elevation control is supported."), "deg",
                        [&status]()
                        {
                            return status.isConnected && status.elevationKnown && qIsFinite(status.elevation)
                                   ? QJsonValue(status.elevation) : QJsonValue();
                        });

    return provider;
}
