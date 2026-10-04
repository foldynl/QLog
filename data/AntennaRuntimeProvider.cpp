#include "AntennaRuntimeProvider.h"
#include "AntProfile.h"
#include "mcpbridge/RuntimeProvider.h"
#include "core/debug.h"
#include <QtMath>

MODULE_IDENTIFICATION("qlog.data.antennaruntimeprovider");

RuntimeProvider *createAntennaRuntimeProvider(AntProfilesManager *manager)
{
    FCT_IDENTIFICATION;

    auto provider = new RuntimeProvider(manager);
    provider->addString("antenna_profile",
                        QStringLiteral("Name of QLog's currently selected antenna profile. Its settings "
                                       "describe the local antenna, associated bands, azimuth offset, "
                                       "beamwidth and assigned rotator-control profile. Selection is "
                                       "software configuration, not verification of a physical antenna "
                                       "connection or the radio's antenna port. Distinct from station_profile "
                                       "and rotator_profile. Null when no antenna profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().profileName;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("antenna_description",
                        QStringLiteral("User-entered description of the local antenna in antenna_profile, "
                                       "such as antenna type, model or installation notes. Returned as "
                                       "configured; not a hardware identification or verified specification. "
                                       "Null when unset or no antenna profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().description;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("antenna_bands",
                        QStringLiteral("Band names associated with antenna_profile, returned as one "
                                       "comma-separated string, for example '20m,15m'; not a JSON array. "
                                       "These configured associations are used by QLog's antenna selection "
                                       "for a band. Not the radio's current band, proof of tuning, measured "
                                       "SWR or guaranteed operating capability. Read the rig source for "
                                       "current radio frequency. Null when no bands are assigned or no "
                                       "antenna profile is selected."),
                        [manager]()
                        {
                            const QStringList bands = manager->getCurProfile1().bands;
                            return bands.isEmpty() ? QJsonValue() : QJsonValue(bands.join(','));
                        });

    provider->addNumber("antenna_azimuth_offset",
                        QStringLiteral("Signed azimuth correction configured in antenna_profile, in degrees. "
                                       "QLog converts each raw rotator report to antenna compass bearing "
                                       "using normalize(raw azimuth + offset) into [0, 360). For example, "
                                       "raw 100 degrees with offset +20 gives rotator_azimuth 120 degrees; "
                                       "raw 350 with +30 gives 20. The rotator source's rotator_azimuth "
                                       "ALREADY includes this correction: never add or subtract this field "
                                       "again when interpreting that bearing. When QLog sends an antenna "
                                       "target bearing to the rotator, its inverse conversion subtracts "
                                       "the offset before sending the driver coordinates. This is a "
                                       "horizontal correction, not an elevation offset, RIT or XIT. Zero "
                                       "is valid and means no correction; negative values are valid. "
                                       "This field is current configuration, whereas rotator_azimuth is "
                                       "the last report corrected when received; after a profile edit or "
                                       "switch they may reflect different moments until another report "
                                       "arrives. Do not reconstruct raw position using the current offset. "
                                       "Null when no antenna profile is selected or the value is non-finite."), "deg",
                        [manager]()
                        {
                            const AntProfile profile = manager->getCurProfile1();
                            return !profile.profileName.isEmpty() && qIsFinite(profile.azimuthOffset)
                                   ? QJsonValue(profile.azimuthOffset) : QJsonValue();
                        });

    provider->addNumber("antenna_azimuth_beamwidth",
                        QStringLiteral("Configured full horizontal beamwidth of the antenna in "
                                       "antenna_profile, in degrees; not a half-angle or rotator position. "
                                       "Use rotator_azimuth for the reported beam direction; that bearing "
                                       "already includes antenna_azimuth_offset. This user setting is not "
                                       "a measured radiation pattern, antenna gain or guarantee of contact "
                                       "coverage. Null when unspecified (zero), invalid (negative or "
                                       "non-finite), or no antenna profile is selected."), "deg",
                        [manager]()
                        {
                            const AntProfile profile = manager->getCurProfile1();
                            return !profile.profileName.isEmpty() && qIsFinite(profile.azimuthBeamWidth)
                                   && profile.azimuthBeamWidth > 0.0
                                   ? QJsonValue(profile.azimuthBeamWidth) : QJsonValue();
                        });

    provider->addString("antenna_rotator_profile",
                        QStringLiteral("Name of the rotator-control profile assigned in antenna_profile "
                                       "for automatic selection and connection when that antenna becomes "
                                       "active. An Activity Rotator Override takes priority, so this "
                                       "assignment may differ from the driver actually connected. Read "
                                       "the rotator source's rotator_profile and rotator_connected for "
                                       "the actual connection; assignment alone proves neither connection "
                                       "nor movement. QLog's azimuth conversion reads the selected antenna's "
                                       "offset even when the rotator profile differs. Null when no rotator "
                                       "is assigned (None) or no antenna profile is selected; None leaves "
                                       "the existing rotator profile and connection unchanged unless an "
                                       "Activity override applies. Null does not mean disconnected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().rotProfileName;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    return provider;
}
