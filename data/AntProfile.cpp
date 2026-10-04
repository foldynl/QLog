#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonArray>
#include <QJsonDocument>

#include "AntProfile.h"
#include "AntennaRuntimeProvider.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.antprofile");

QDataStream& operator<<(QDataStream& out, const AntProfile& v)
{

    out << v.profileName << v.description << v.azimuthBeamWidth
        << v.azimuthOffset << v.bands << v.rotProfileName;
    return out;
}

QDataStream& operator>>(QDataStream& in, AntProfile& v)
{
    in >> v.profileName;
    in >> v.description;
    in >> v.azimuthBeamWidth;
    in >> v.azimuthOffset;
    in >> v.bands;
    in >> v.rotProfileName;

    return in;
}

AntProfilesManager::AntProfilesManager() :
    ProfileManagerSQL<AntProfile>("ant_profiles")
{
    FCT_IDENTIFICATION;

    QSqlQuery profileQuery;

    if ( ! profileQuery.prepare("SELECT profile_name, desc, azimuth_beamwidth, "
                                "       azimuth_offset, bands, rot_profile "
                                "FROM ant_profiles") )
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
            const QJsonArray bands = QJsonDocument::fromJson(profileQuery.value(4).toByteArray()).array();
            for ( const QJsonValue &band : bands )
                profileDB.bands.append(band.toString());
            profileDB.rotProfileName = profileQuery.value(5).toString();

            addProfile(profileDB.profileName, profileDB);
        }
    }
    else
        qInfo() << "Ant Profile DB select error " << profileQuery.lastError().text();

    statusProvider = createAntennaRuntimeProvider(this);
}

RuntimeProvider *AntProfilesManager::getRuntimeProvider() const
{
    FCT_IDENTIFICATION;
    return statusProvider;
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

    if ( ! insertQuery.prepare("INSERT INTO ant_profiles(profile_name, desc, azimuth_beamwidth, azimuth_offset, bands, rot_profile) "
                        "VALUES (:profile_name, :desc, :azimuth_beamwidth, :azimuth_offset, :bands, :rot_profile)") )
    {
        qWarning() << "cannot prepare Insert statement";
        return;
    }

    if ( deleteQuery.exec() )
    {
        const QStringList &keys = profileNameList();
        for ( const QString &key: keys )
        {
            const AntProfile &antProfile = getProfile(key);
            const QJsonDocument bands(QJsonArray::fromStringList(antProfile.bands));

            insertQuery.bindValue(":profile_name", key);
            insertQuery.bindValue(":desc", antProfile.description);
            insertQuery.bindValue(":azimuth_beamwidth", antProfile.azimuthBeamWidth);
            insertQuery.bindValue(":azimuth_offset", antProfile.azimuthOffset);
            insertQuery.bindValue(":bands", QString::fromUtf8(bands.toJson(QJsonDocument::Compact)));
            insertQuery.bindValue(":rot_profile", antProfile.rotProfileName);

            if ( ! insertQuery.exec() )
                qInfo() << "Ant Profile DB insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
        }
    }
    else
        qInfo() << "Ant Profile DB delete error " << deleteQuery.lastError().text();

    saveCurProfile1();

}

QStringList AntProfilesManager::profileNamesForBand(const QString &bandName)
{
    FCT_IDENTIFICATION;

    QStringList matchingProfiles;

    for ( const QString &profileName : profileNameList() )
        if ( getProfile(profileName).bands.contains(bandName) )
            matchingProfiles.append(profileName);

    return matchingProfiles;
}

QString AntProfilesManager::profileNameForBand(const QString &bandName,
                                               const QString &preferredProfileName)
{
    FCT_IDENTIFICATION;

    if ( profileNameList().contains(preferredProfileName) )
        return preferredProfileName;

    const QStringList &matchingProfiles = profileNamesForBand(bandName);
    const AntProfile &currentProfile = getCurProfile1();

    if ( matchingProfiles.contains(currentProfile.profileName) )
        return currentProfile.profileName;

    return matchingProfiles.isEmpty() ? QString() : matchingProfiles.first();
}

bool AntProfile::operator==(const AntProfile &profile)
{
    return (profile.profileName == this->profileName
            && profile.description == this->description
            && profile.azimuthBeamWidth == this->azimuthBeamWidth
            && profile.azimuthOffset == this->azimuthOffset
            && profile.bands == this->bands
            && profile.rotProfileName == this->rotProfileName);
}

bool AntProfile::operator!=(const AntProfile &profile)
{
   return !operator==(profile);
}
