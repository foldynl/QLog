#include "ActivityRuntimeProvider.h"
#include "ActivityProfile.h"
#include "mcpbridge/RuntimeProvider.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.activityruntimeprovider");

RuntimeProvider *createActivityRuntimeProvider(ActivityProfilesManager *manager)
{
    FCT_IDENTIFICATION;

    auto provider = new RuntimeProvider(manager);
    provider->addString("activity_profile",
                        QStringLiteral("Name of QLog's currently selected Activity preset. An Activity "
                                       "groups station and equipment profile selections and connection "
                                       "requests applied on activation. It is configured operating context, "
                                       "not proof of an ongoing QSO, contest participation, transmitting "
                                       "or successful hardware connection. Manual changes and asynchronous "
                                       "equipment setup can make actual state differ from this preset. "
                                       "Read station, antenna, rig and rotator for actual current state. "
                                       "Null when no Activity is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().profileName;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("activity_station_profile",
                        QStringLiteral("Station profile requested by the selected Activity on activation. "
                                       "Read station_profile in the station source for the current selected "
                                       "station and its callsign, location and activation references; this "
                                       "preset reference may differ after manual changes. Null means this "
                                       "Activity does not specify a station, or no Activity is selected. "
                                       "An empty string explicitly selects None, clearing station selection."),
                        [manager]()
                        {
                            const ActivityProfile profile = manager->getCurProfile1();
                            return profile.profiles.contains(ActivityProfile::STATION_PROFILE)
                                   ? QJsonValue(profile.profiles.value(ActivityProfile::STATION_PROFILE).name)
                                   : QJsonValue();
                        });

    provider->addString("activity_rig_profile",
                        QStringLiteral("Rig-control profile requested by the selected Activity on activation; "
                                       "a configured reference, not the connected radio's model or confirmation "
                                       "of a connection. Read activity_rig_connect for the connection request "
                                       "and the rig source for actual rig_profile and rig_connected. Null "
                                       "means no rig profile is specified, or no Activity is selected. An "
                                       "empty string explicitly selects None, clearing rig profile selection."),
                        [manager]()
                        {
                            const ActivityProfile profile = manager->getCurProfile1();
                            return profile.profiles.contains(ActivityProfile::RIG_PROFILE)
                                   ? QJsonValue(profile.profiles.value(ActivityProfile::RIG_PROFILE).name)
                                   : QJsonValue();
                        });

    provider->addString("activity_antenna_profile",
                        QStringLiteral("Antenna profile requested by the selected Activity. A valid named "
                                       "profile is preferred during automatic antenna selection even if its "
                                       "configured bands do not match the radio's band. Without a valid "
                                       "preference, QLog uses normal band-based antenna selection. Read "
                                       "antenna_profile in the antenna source for the actual selected antenna "
                                       "and its azimuth offset. Selection does not verify physical RF switching. "
                                       "Null means no antenna preference is specified, or no Activity is "
                                       "selected. An empty string selects None on activation and leaves "
                                       "no named preference for subsequent band-based selection."),
                        [manager]()
                        {
                            const ActivityProfile profile = manager->getCurProfile1();
                            return profile.profiles.contains(ActivityProfile::ANTENNA_PROFILE)
                                   ? QJsonValue(profile.profiles.value(ActivityProfile::ANTENNA_PROFILE).name)
                                   : QJsonValue();
                        });

    provider->addString("activity_rotator_profile",
                        QStringLiteral("Rotator Override configured in the selected Activity. A nonempty "
                                       "name takes priority over antenna_rotator_profile, the selected "
                                       "antenna's rotator assignment; activity_rotator_connect supplies "
                                       "the connection request. An empty string explicitly overrides with "
                                       "None: QLog disconnects the rotator and suppresses the antenna's "
                                       "assignment. Null means no override, or no Activity is selected; "
                                       "QLog may then use the antenna's assignment. Null does not mean "
                                       "disconnected. Read rotator_profile and rotator_connected for the "
                                       "actual connection. The selected antenna's azimuth offset still "
                                       "applies with an override; rotator_azimuth already includes that "
                                       "correction, so never apply it again."),
                        [manager]()
                        {
                            const ActivityProfile profile = manager->getCurProfile1();
                            return profile.profiles.contains(ActivityProfile::ROT_PROFILE)
                                   ? QJsonValue(profile.profiles.value(ActivityProfile::ROT_PROFILE).name)
                                   : QJsonValue();
                        });

    provider->addBoolean("activity_rig_connect",
                         QStringLiteral("Stored rig connection request in the selected Activity: true "
                                        "requests connection of activity_rig_profile, false requests "
                                        "disconnection when that profile is current. Null means no explicit "
                                        "request, or no Activity is selected; it is not false. This is "
                                        "preset intent, not connection success or transmit/PTT state. "
                                        "Read rig_connected in the rig source for actual connection state."),
                         [manager]()
                         {
                             const QVariant value = manager->getCurProfile1().getProfileParam(
                                 ActivityProfile::RIG_PROFILE, ActivityProfile::CONNECT);
                             return value.isNull() ? QJsonValue() : QJsonValue(value.toBool());
                         });

    provider->addBoolean("activity_rotator_connect",
                         QStringLiteral("Stored rotator connection request for the Activity's Rotator "
                                        "Override: true requests connection, false requests disconnection. "
                                        "Null means no explicit request, or no Activity is selected. If an "
                                        "override is present but this parameter is absent, QLog treats the "
                                        "request as false when applying it. An empty override name always "
                                        "disconnects. With no override, the antenna's assigned rotator "
                                        "can be connected independently of this field. Read rotator_connected "
                                        "for actual connection; a request does not confirm success or movement."),
                         [manager]()
                         {
                             const QVariant value = manager->getCurProfile1().getProfileParam(
                                 ActivityProfile::ROT_PROFILE, ActivityProfile::CONNECT);
                             return value.isNull() ? QJsonValue() : QJsonValue(value.toBool());
                         });

    return provider;
}
