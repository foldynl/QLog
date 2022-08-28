#include <QSettings>
#include <QSqlQuery>
#include <QSqlError>

#include "CWKeyProfile.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.CWKeyProfile");

QDataStream& operator<<(QDataStream& out, const CWKeyProfile& v)
{

    out << v.profileName
        << v.model
        << v.defaultSpeed
        << v.keyMode
        << v.portPath
        << v.baudrate;
    return out;
}

QDataStream& operator>>(QDataStream& in, CWKeyProfile& v)
{
    in >> v.profileName;
    in >> v.model;
    in >> v.defaultSpeed;
    in >> v.keyMode;
    in >> v.portPath;
    in >> v.baudrate;

    return in;
}

CWKeyProfilesManager::CWKeyProfilesManager(QObject *parent) :
    QObject(parent),
    ProfileManager<CWKeyProfile>("equipment/cwkey")
{
    FCT_IDENTIFICATION;

    QSettings settings;

    QSqlQuery profileQuery;

    if ( ! profileQuery.prepare("SELECT profile_name, model, default_speed, key_mode, port_pathname, baudrate FROM cwkey_profiles") )
    {
        qWarning()<< "Cannot prepare select";
    }

    if ( profileQuery.exec() )
    {
        while (profileQuery.next())
        {
            CWKeyProfile profileDB;
            profileDB.profileName = profileQuery.value(0).toString();
            profileDB.model =  profileQuery.value(1).toInt();
            profileDB.defaultSpeed = profileQuery.value(2).toInt();
            profileDB.keyMode = profileQuery.value(3).toInt();
            profileDB.portPath =  profileQuery.value(4).toString();
            profileDB.baudrate =  profileQuery.value(5).toUInt();

            addProfile(profileDB.profileName, profileDB);
        }
    }
    else
    {
        qInfo() << "CW Key Profile DB select error " << profileQuery.lastError().text();
    }
}

CWKeyProfilesManager *CWKeyProfilesManager::instance()
{
    FCT_IDENTIFICATION;

    static CWKeyProfilesManager instance;
    return &instance;
}

void CWKeyProfilesManager::save()
{
    FCT_IDENTIFICATION;

    QSqlQuery deleteQuery;
    QSqlQuery insertQuery;

    if ( ! deleteQuery.prepare("DELETE FROM cwkey_profiles") )
    {
        qWarning() << "Cannot prepare Delete statement";
        return;
    }

    if ( ! insertQuery.prepare("INSERT INTO cwkey_profiles(profile_name, model, default_speed, key_mode, port_pathname, baudrate) "
                        "VALUES (:profile_name, :model, :default_speed, :key_mode, :port_pathname, :baudrate)") )
    {
        qWarning() << "Cannot prepare Insert statement";
        return;
    }

    if ( deleteQuery.exec() )
    {
        auto keys = profileNameList();
        for ( auto &key: qAsConst(keys) )
        {
            CWKeyProfile cwKeyProfile = getProfile(key);

            insertQuery.bindValue(":profile_name", key);
            insertQuery.bindValue(":model", cwKeyProfile.model);
            insertQuery.bindValue(":default_speed", cwKeyProfile.defaultSpeed);
            insertQuery.bindValue(":key_mode", cwKeyProfile.keyMode);
            insertQuery.bindValue(":port_pathname", cwKeyProfile.portPath);
            insertQuery.bindValue(":baudrate", cwKeyProfile.baudrate);

            if ( ! insertQuery.exec() )
            {
                qInfo() << "CW Key Profile DB insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
            }
        }
    }
    else
    {
        qInfo() << "CW Key Profile DB delete error " << deleteQuery.lastError().text();
    }

    saveCurProfile1();
}

bool CWKeyProfile::operator==(const CWKeyProfile &profile)
{
    return (profile.profileName == this->profileName
            && profile.model == this->model
            && profile.defaultSpeed == this->defaultSpeed
            && profile.keyMode == this->keyMode
            && profile.portPath == this->portPath
            && profile.baudrate == this->baudrate);
}

bool CWKeyProfile::operator!=(const CWKeyProfile &profile)
{
    return !operator==(profile);
}
