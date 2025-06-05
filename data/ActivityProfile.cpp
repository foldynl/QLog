#include "ActivityProfile.h"
#include "core/debug.h"
#include "data/ProfileManager.h"
#include "data/AntProfile.h"
#include "data/MainLayoutProfile.h"
#include "data/RigProfile.h"
#include "data/RotProfile.h"
#include "data/StationProfile.h"

MODULE_IDENTIFICATION("qlog.data.activityprofile");

// a compilation issue under 20.04 Ubuntu because qt 5.12 does not
// support >> for QHash but it is not used now.

// QDataStream& operator<<(QDataStream& out, const ActivityProfile& v)
// {
//     out << v.profileName
//         << v.profiles
//         << v.fieldValues;

//     return out;
// }

// QDataStream& operator<<(QDataStream& out, const ActivityProfile::ProfileRecord& v)
// {
//     out << v.name
//         << v.params;

//     return out;
// }

// QDataStream& operator>>(QDataStream& in, ActivityProfile& v)
// {
//     in >> v.profileName;
//     in >> v.profiles;
//     in >> v.fieldValues;

//     return in;
// }

// QDataStream& operator>>(QDataStream& in, ActivityProfile::ProfileRecord& v)
// {
//     in >> v.name;
//     in >> v.params;

//     return in;
// }

ActivityProfile::ActivityProfile(const QString &name, const QJsonDocument &config)
{
    /* Config example
     * {
     *    "activityName" = "name",
     *    "profiles" = [
     *                 { "profileType" = 1,
     *                   "name" = "name",
     *                   "params" = [
     *                                {
     *                                   "name" = "connect"
     *                                   "value" : true
     *                                }
     *                              ]
     *                 }
     *                 ]
     *     "fieldValues" = [
     *                   { "fieldID" = 12,
     *                     "value" = "Contest"
     *                   }
     *                ]
     * }
     */

    if ( config.isEmpty() )
        return;

    profileName = name;
    const QJsonArray &profilesArray = config["profiles"].toArray();

    for ( const QJsonValue &value : profilesArray )
    {
        const QJsonObject &obj = value.toObject();
        ActivityProfile::ProfileRecord profileRec;

        profileRec.name = obj["name"].toString();

        const QJsonArray &profilesParamsArray = value["params"].toArray();
        for ( const QJsonValue &paramValue : profilesParamsArray )
        {
            const QJsonObject &inner = paramValue.toObject();
            profileRec.params[getParamID(inner["name"].toString())] = inner["value"].toVariant();
        }

        profiles[static_cast<ActivityProfile::ProfileType>(obj["profileType"].toInt())] = profileRec;
    }

    const QJsonArray &fieldValuesArray = config["fieldValues"].toArray();
    for ( const QJsonValue &fieldValue : fieldValuesArray )
    {
        const QJsonObject &obj = fieldValue.toObject();
        fieldValues[obj["fieldID"].toInt()] = obj["value"].toString();
    }
}

QByteArray ActivityProfile::toJson() const
{
    /* Config example
     * {
     *    "activityName" = "name",
     *    "profiles" = [
     *                 { "profileType" = 1,
     *                   "name" = "name",
     *                   "params" = [
     *                                {
     *                                   "name" = "connect"
     *                                   "value" : true
     *                                }
     *                              ]
     *                 }
     *                 ]
     *     "fieldValues" = [
     *                   { "fieldID" = 12,
     *                     "value" = "Contest"
     *                   }
     *                ]
     * }
     */

    QJsonObject activityObject;

    activityObject["activityName"] = profileName;

    if ( !profiles.isEmpty() )
    {
        QJsonArray profilesArray;
        for (auto i = profiles.begin(); i != profiles.end(); ++i)
        {
            QJsonObject profileObject;
            profileObject["profileType"] = i.key();
            profileObject["name"] = i.value().name;

            if ( !i.value().params.isEmpty() )
            {
                QJsonArray paramsArray;

                for (auto j = i.value().params.begin(); j != i.value().params.end(); ++j )
                {
                    QJsonObject profileParamObject;
                    profileParamObject["name"] = getParamName(j.key());
                    profileParamObject["value"] = j.value().toJsonValue();
                    paramsArray.push_back(profileParamObject);
                }
                profileObject["params"] = paramsArray;
            }
            profilesArray.push_back(profileObject);
        }
        activityObject["profiles"] = profilesArray;
    }

    if ( !fieldValues.isEmpty() )
    {
        QJsonArray fieldValuesArray;
        for (auto i = fieldValues.begin(); i != fieldValues.end(); i++ )
        {
            QJsonObject fieldObject;
            fieldObject["fieldID"] = i.key();
            fieldObject["value"] = i.value().toJsonValue();
            fieldValuesArray.push_back(fieldObject);
        }
        activityObject["fieldValues"] = fieldValuesArray;
    }
    QJsonDocument doc(activityObject);
    return doc.toJson();
}

bool ActivityProfile::operator==(const ActivityProfile &profile)
{
    return ( profile.profileName == this->profileName
             && profile.profiles == this->profiles
             && profile.fieldValues == this->fieldValues
            );
}

bool ActivityProfile::operator!=(const ActivityProfile &profile)
{
    return !operator==(profile);
}

void ActivityProfilesManager::setAllProfiles()
{
    ActivityProfile currActivity = getCurProfile1();

    if (currActivity == ActivityProfile() )
        return;

    for( auto i = currActivity.profiles.begin(); i != currActivity.profiles.end(); i++ )
    {
        qCDebug(runtime) << i.key() << i.value().name;
        switch ( i.key() )
        {
        case ActivityProfile::ANTENNA_PROFILE:
            AntProfilesManager::instance()->setCurProfile1(i.value().name);
            break;
        case ActivityProfile::STATION_PROFILE:
            StationProfilesManager::instance()->setCurProfile1(i.value().name);
            break;
        case ActivityProfile::RIG_PROFILE:
            RigProfilesManager::instance()->setCurProfile1(i.value().name);
            break;
        case ActivityProfile::ROT_PROFILE:
            RotProfilesManager::instance()->setCurProfile1(i.value().name);
            break;
        case ActivityProfile::MAIN_LAYOUT_PROFILE:
            MainLayoutProfilesManager::instance()->setCurProfile1(i.value().name);
            break;
        default:
            qWarning() << "Unknown Activity profile" << i.key();
        }
    }
    emit changeFinished(currActivity.profileName);
}

ActivityProfilesManager::ActivityProfilesManager() :
    ProfileManagerSQL<ActivityProfile>("activity_profiles")
{
    FCT_IDENTIFICATION;

    QSqlQuery profileQuery;

    if ( ! profileQuery.prepare("SELECT profile_name, config FROM activity_profiles") )
        qWarning()<< "Cannot prepare select";

    if ( profileQuery.exec() )
    {
        while (profileQuery.next())
        {
            ActivityProfile profileDB(profileQuery.value(0).toString(),
                                      QJsonDocument::fromJson(profileQuery.value(1).toByteArray()));

            addProfile(profileDB.profileName, profileDB);
        }
    }
    else
        qInfo() << "Activity Profile DB select error " << profileQuery.lastError().text();

    connect (this, &ActivityProfilesManager::profileChanged,
             this, &ActivityProfilesManager::setAllProfiles);
}

void ActivityProfilesManager::save()
{
    FCT_IDENTIFICATION;

    QSqlQuery deleteQuery;
    QSqlQuery insertQuery;

    if ( ! deleteQuery.prepare("DELETE FROM activity_profiles") )
    {
        qWarning() << "cannot prepare Delete statement";
        return;
    }

    if ( ! insertQuery.prepare("INSERT INTO activity_profiles(profile_name, config) "
                               "VALUES (:profile_name, :config)") )
    {
        qWarning() << "cannot prepare Insert statement";
        return;
    }

    if ( deleteQuery.exec() )
    {
        const QStringList &keys = profileNameList();
        for ( const QString &key: keys )
        {
            const ActivityProfile &activityProfile = getProfile(key);

            insertQuery.bindValue(":profile_name", key);
            insertQuery.bindValue(":config", activityProfile.toJson());

            if ( ! insertQuery.exec() )
                qInfo() << "Station Profile DB insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
        }
    }
    else
        qInfo() << "Activity Profile DB delete error " << deleteQuery.lastError().text();

    saveCurProfile1();
}
