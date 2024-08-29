#include <QSqlQuery>
#include <QSqlError>

#include "RotUsrButtonsProfile.h"

MODULE_IDENTIFICATION("qlog.data.rotusrbuttonsprofile");


QDataStream& operator<<(QDataStream& out, const RotUsrButtonsProfile& v)
{

    out << v.profileName;

    for( int i = 0; i < v.shortDescs.size(); i++ )
    {
        out << v.shortDescs[i];
    }

    for( int i = 0; i < v.bearings.size(); i++ )
    {
        out << v.bearings[i];
    }

    return out;
}

QDataStream& operator>>(QDataStream& in, RotUsrButtonsProfile& v)
{
    in >> v.profileName;

    for( int i = 0; i < v.shortDescs.size(); i++ )
    {
        in >> v.shortDescs[i];
    }

    for( int i = 0; i < v.bearings.size(); i++ )
    {
        in >> v.bearings[i];
    }

    return in;
}

RotUsrButtonsProfilesManager::RotUsrButtonsProfilesManager(QObject *parent) :
    QObject(parent),
    ProfileManagerSQL<RotUsrButtonsProfile>("rot_user_buttons_profiles")
{
    FCT_IDENTIFICATION;

    QSqlQuery profileQuery;

    if ( ! profileQuery.prepare("SELECT profile_name, button1_short, button1_value, button2_short, button2_value, "
                                "button3_short, button3_value, button4_short, button4_value "
                                "FROM rot_user_buttons_profiles") )
    {
        qWarning()<< "Cannot prepare select";
    }

    if ( profileQuery.exec() )
    {
        while (profileQuery.next())
        {
            RotUsrButtonsProfile profileDB;

            int column = 0;
            profileDB.profileName = profileQuery.value(column++).toString();

            for ( int i = 0; i < profileDB.shortDescs.size(); i++ )
            {
                profileDB.shortDescs[i] = profileQuery.value(column++).toString();
                profileDB.bearings[i] = profileQuery.value(column++).toDouble();
            }

            addProfile(profileDB.profileName, profileDB);
        }
    }
    else
    {
        qInfo() << "Rot User Button Profile DB select error " << profileQuery.lastError().text();
    }

}

RotUsrButtonsProfilesManager *RotUsrButtonsProfilesManager::instance()
{
    FCT_IDENTIFICATION;

    static RotUsrButtonsProfilesManager instance;
    return &instance;
}

void RotUsrButtonsProfilesManager::save()
{
    FCT_IDENTIFICATION;

    QSqlQuery deleteQuery;
    QSqlQuery insertQuery;

    if ( ! deleteQuery.prepare("DELETE FROM rot_user_buttons_profiles") )
    {
        qWarning() << "Cannot prepare Delete statement";
        return;
    }

    if ( ! insertQuery.prepare("INSERT INTO rot_user_buttons_profiles(profile_name, button1_short, button1_value, button2_short, button2_value,"
                               "button3_short, button3_value, button4_short, button4_value)"
                        "VALUES (:profile_name, :b1_short, :b1_value, :b2_short, :b2_value,"
                               ":b3_short, :b3_value, :b4_short, :b4_value)") )
    {
        qWarning() << "Cannot prepare Insert statement";
        return;
    }

    if ( deleteQuery.exec() )
    {
        auto keys = profileNameList();
        for ( auto &key: qAsConst(keys) )
        {
            RotUsrButtonsProfile rotUsrButtonProfile = getProfile(key);

            insertQuery.bindValue(":profile_name", key);
            for ( int i = 0; i < rotUsrButtonProfile.shortDescs.size(); i++ )
            {
                insertQuery.bindValue(QString(":b%1_short").arg(i+1), rotUsrButtonProfile.shortDescs[i]);
            }

            for ( int i = 0; i < rotUsrButtonProfile.bearings.size(); i++ )
            {
                insertQuery.bindValue(QString(":b%1_value").arg(i+1), rotUsrButtonProfile.bearings[i]);
            }

            if ( ! insertQuery.exec() )
            {
                qInfo() << "Rot User Button Profile DB insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
            }
        }
    }
    else
    {
        qInfo() << "Rot User Button Profile DB delete error " << deleteQuery.lastError().text();
    }

    saveCurProfile1();

}

bool RotUsrButtonsProfile::operator==(const RotUsrButtonsProfile &profile)
{
    return (profile.profileName == this->profileName
            && profile.shortDescs == this->shortDescs
            && profile.bearings == this->bearings);
}

bool RotUsrButtonsProfile::operator!=(const RotUsrButtonsProfile &profile)
{
    return !operator==(profile);
}
