#include "StationRuntimeProvider.h"
#include "StationProfile.h"
#include "mcpbridge/RuntimeProvider.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.stationruntimeprovider");

RuntimeProvider *createStationRuntimeProvider(StationProfilesManager *manager)
{
    FCT_IDENTIFICATION;

    auto provider = new RuntimeProvider(manager);

    // All getters run in one event-loop turn, so a profile switch cannot split a snapshot.
    provider->addString("station_profile",
                        QStringLiteral("Name of QLog's currently selected logging-station profile. "
                                       "Defines the local station identity, location and activation "
                                       "references used for logging. Not a radio, antenna or rotator "
                                       "control profile. Null when no station profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().profileName;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_callsign",
                        QStringLiteral("Local logging-station callsign from the selected station profile, "
                                       "including any configured portable or other suffix. This is your "
                                       "station's on-air identity, not the contacted station or necessarily "
                                       "the operator's personal callsign. Null when unset or no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().callsign;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_locator",
                        QStringLiteral("Configured Maidenhead grid locator of the local logging station "
                                       "in the selected profile, for example JO70AA. Describes the station "
                                       "location, not the contact's locator or a live GPS fix. Returned "
                                       "as configured. Null when unset or no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().locator;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_operator_name",
                        QStringLiteral("Configured personal name of the operator of the local station "
                                       "in the selected profile. Not the contacted operator's name. "
                                       "Null when unset or no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().operatorName;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_operator_callsign",
                        QStringLiteral("Personal callsign of the local operator configured in the selected "
                                       "profile. May differ from station_callsign when operating a club, "
                                       "special-event or other station. Null means not configured; do not "
                                       "infer it from station_callsign. Null also when no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().operatorCallsign;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_qth",
                        QStringLiteral("Configured QTH name, such as a city or operating site, of the "
                                       "local logging station in the selected profile. Not a live location "
                                       "measurement or the contact's QTH. Null when unset or no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().qthName;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_country",
                        QStringLiteral("Configured country or DXCC entity name of the local logging station "
                                       "in the selected profile. Paired with station_dxcc; not a country "
                                       "inferred from the current callsign or GPS. Null when unset or no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().country;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_county",
                        QStringLiteral("Configured county or administrative subdivision of the local "
                                       "station in the selected profile. Meaning and format depend on "
                                       "the country; returned as configured. Null when unset or no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().county;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addNumber("station_dxcc",
                        QStringLiteral("Configured ADIF DXCC entity code of the local logging station "
                                       "in the selected profile, for example 503 for the Czech Republic. "
                                       "An entity identifier, not a count of worked or confirmed countries. "
                                       "Null when the code is unset (zero) or no profile is selected."), QString(),
                        [manager]()
                        {
                            const int value = manager->getCurProfile1().dxcc;
                            return value == 0 ? QJsonValue() : QJsonValue(value);
                        });

    provider->addNumber("station_cq_zone",
                        QStringLiteral("Configured CQ amateur-radio zone number of the local logging station "
                                       "in the selected profile (CQ zones are numbered 1 to 40). Not the "
                                       "contact's zone or an ITU zone. Null when unset (zero) or no profile is selected."), QString(),
                        [manager]()
                        {
                            const int value = manager->getCurProfile1().cqz;
                            return value == 0 ? QJsonValue() : QJsonValue(value);
                        });

    provider->addNumber("station_itu_zone",
                        QStringLiteral("Configured ITU radio zone number of the local logging station in "
                                       "the selected profile (ITU zones are numbered 1 to 90). Distinct "
                                       "from station_cq_zone; not the contact's zone. Null when unset "
                                       "(zero) or no profile is selected."), QString(),
                        [manager]()
                        {
                            const int value = manager->getCurProfile1().ituz;
                            return value == 0 ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_iota",
                        QStringLiteral("Configured Islands on the Air (IOTA) island-group reference for "
                                       "the local station's location in the selected profile, for example "
                                       "EU-001. Not an island worked or confirmed in the log. Null when "
                                       "unset or no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().iota;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_pota",
                        QStringLiteral("Configured Parks on the Air (POTA) reference or references for "
                                       "the local station's activation in the selected profile. Returned "
                                       "as one configured string, possibly containing multiple references. "
                                       "Not parks hunted, an activation schedule or proof of presence "
                                       "in a park. Null when unset or no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().pota;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_sota",
                        QStringLiteral("Configured Summits on the Air (SOTA) summit reference for the "
                                       "local station's activation in the selected profile, for example "
                                       "OK/ST-001. Not a summit chased or proof of current physical "
                                       "presence. Null when unset or no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().sota;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_wwff",
                        QStringLiteral("Configured World Wide Flora and Fauna (WWFF) protected-area "
                                       "reference for the local station's activation in the selected "
                                       "profile, for example OKFF-0001. Not an area worked or confirmed. "
                                       "Null when unset or no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().wwff;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_sig",
                        QStringLiteral("Configured special-interest group or activity name for the local "
                                       "station in the selected profile (ADIF MY_SIG). station_sig_info "
                                       "contains its associated reference or details. Not a QLog alert "
                                       "or general application status. Null when unset or no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().sig;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_sig_info",
                        QStringLiteral("Configured reference or additional information associated with "
                                       "station_sig for the local station in the selected profile "
                                       "(ADIF MY_SIG_INFO). Interpret using station_sig; returned as "
                                       "configured. Null when unset or no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().sigInfo;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_vucc_grids",
                        QStringLiteral("Configured comma-separated Maidenhead grids for the local station "
                                       "operating on a grid boundary for VHF/UHF Century Club (VUCC) "
                                       "credit (ADIF MY_VUCC_GRIDS). Not a list of worked or confirmed "
                                       "grids. Returned as one string. Null when unset or no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().vucc;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    provider->addString("station_darc_dok",
                        QStringLiteral("Configured DARC district/local-club identifier (DOK) of the local "
                                       "station in the selected profile (ADIF MY_DARC_DOK), for example "
                                       "A01. Not the contact's DOK or a worked-club count. Null when "
                                       "unset or no profile is selected."),
                        [manager]()
                        {
                            const QString value = manager->getCurProfile1().darcDOK;
                            return value.isEmpty() ? QJsonValue() : QJsonValue(value);
                        });

    return provider;
}
