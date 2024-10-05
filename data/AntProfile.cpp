#include <QSettings>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

#include "AntProfile.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.antprofile");

QDataStream& operator<<(QDataStream& out, const AntProfile& v)
{

    out << v.profileName << v.description << v.azimuthBeamWidth << v.azimuthOffset;
    return out;
}

QDataStream& operator>>(QDataStream& in, AntProfile& v)
{
    in >> v.profileName;
    in >> v.description;
    in >> v.azimuthBeamWidth;
    in >> v.azimuthOffset;

    return in;
}

AntProfilesManager::AntProfilesManager(QObject *parent) :
    QObject(parent),
    ProfileManagerSQL<AntProfile>("ant_profiles")
{
    FCT_IDENTIFICATION;

    QSqlQuery profileQuery;

    if ( ! profileQuery.prepare("SELECT profile_name, desc, azimuth_beamwidth, azimuth_offset FROM ant_profiles") )
    {
        qWarning()<< "Cannot prepare select";
    }

    if ( profileQuery.exec() )
    {
        while (profileQuery.next())
        {
            AntProfile profileDB;
            profileDB.profileName = profileQuery.value(0).toString();
            profileDB.description =  profileQuery.value(1).toString();
            profileDB.azimuthBeamWidth = profileQuery.value(2).toDouble();
            profileDB.azimuthOffset = profileQuery.value(3).toDouble();

            addProfile(profileDB.profileName, profileDB);
        }
    }
    else
    {
        qInfo() << "Station Profile DB select error " << profileQuery.lastError().text();
    }
}

void AntProfilesManager::save()
{
    FCT_IDENTIFICATION;

    QSqlQuery deleteQuery;
    QSqlQuery insertQuery;

    if ( ! deleteQuery.prepare("DELETE FROM ant_profiles") )
    {
        qWarning() << "cannot prepare Delete statement";
        return;
    }

    if ( ! insertQuery.prepare("INSERT INTO ant_profiles(profile_name, desc, azimuth_beamwidth, azimuth_offset) "
                        "VALUES (:profile_name, :desc, :azimuth_beamwidth, :azimuth_offset)") )
    {
        qWarning() << "cannot prepare Insert statement";
        return;
    }

    if ( deleteQuery.exec() )
    {
        const QStringList &keys = profileNameList();
        for ( auto &key: keys )
        {
            const AntProfile &antProfile = getProfile(key);

            insertQuery.bindValue(":profile_name", key);
            insertQuery.bindValue(":desc", antProfile.description);
            insertQuery.bindValue(":azimuth_beamwidth", antProfile.azimuthBeamWidth);
            insertQuery.bindValue(":azimuth_offset", antProfile.azimuthOffset);


            if ( ! insertQuery.exec() )
            {
                qInfo() << "Station Profile DB insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
            }
        }
    }
    else
    {
        qInfo() << "Station Profile DB delete error " << deleteQuery.lastError().text();
    }

    saveCurProfile1();

}

bool AntProfile::operator==(const AntProfile &profile)
{
    return (profile.profileName == this->profileName
            && profile.description == this->description
            && profile.azimuthBeamWidth == this->azimuthBeamWidth
            && profile.azimuthOffset == this->azimuthOffset);
}

bool AntProfile::operator!=(const AntProfile &profile)
{
   return !operator==(profile);
}
