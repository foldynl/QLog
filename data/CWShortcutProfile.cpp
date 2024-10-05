#include <QSqlQuery>
#include <QSqlError>

#include "CWShortcutProfile.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.cwshortcutprofile");

QDataStream& operator<<(QDataStream& out, const CWShortcutProfile& v)
{

    out << v.profileName;

    for( int i = 0; i < v.shortDescs.size(); i++ )
    {
        out << v.shortDescs[i];
    }

    for( int i = 0; i < v.macros.size(); i++ )
    {
        out << v.macros[i];
    }

    return out;
}

QDataStream& operator>>(QDataStream& in, CWShortcutProfile& v)
{
    in >> v.profileName;

    for( int i = 0; i < v.shortDescs.size(); i++ )
    {
        in >> v.shortDescs[i];
    }

    for( int i = 0; i < v.macros.size(); i++ )
    {
        in >> v.macros[i];
    }

    return in;
}

CWShortcutProfilesManager::CWShortcutProfilesManager(QObject *parent) :
    QObject(parent),
    ProfileManagerSQL<CWShortcutProfile>("cwshortcut_profiles")
{
    FCT_IDENTIFICATION;

    QSqlQuery profileQuery;

    if ( ! profileQuery.prepare("SELECT profile_name, f1_short, f1_macro, f2_short, f2_macro, "
                                "f3_short, f3_macro, f4_short, f4_macro, f5_short, f5_macro, "
                                "f6_short, f6_macro, f7_short, f7_macro FROM cwshortcut_profiles") )
    {
        qWarning()<< "Cannot prepare select";
    }

    if ( profileQuery.exec() )
    {
        while (profileQuery.next())
        {
            CWShortcutProfile profileDB;

            int column = 0;
            profileDB.profileName = profileQuery.value(column++).toString();

            for ( int i = 0; i < profileDB.shortDescs.size(); i++ )
            {
                profileDB.shortDescs[i] = profileQuery.value(column++).toString();
                profileDB.macros[i] = profileQuery.value(column++).toString();
            }

            addProfile(profileDB.profileName, profileDB);
        }
    }
    else
    {
        qInfo() << "CW Shortcut Profile DB select error " << profileQuery.lastError().text();
    }
}

void CWShortcutProfilesManager::save()
{
    FCT_IDENTIFICATION;

    QSqlQuery deleteQuery;
    QSqlQuery insertQuery;

    if ( ! deleteQuery.prepare("DELETE FROM cwshortcut_profiles") )
    {
        qWarning() << "Cannot prepare Delete statement";
        return;
    }

    if ( ! insertQuery.prepare("INSERT INTO cwshortcut_profiles(profile_name, f1_short, f1_macro, f2_short, f2_macro,"
                               "f3_short, f3_macro, f4_short, f4_macro, f5_short, f5_macro,"
                               "f6_short, f6_macro, f7_short, f7_macro)"
                        "VALUES (:profile_name, :f1_short, :f1_macro, :f2_short, :f2_macro,"
                               ":f3_short, :f3_macro, :f4_short, :f4_macro, :f5_short, :f5_macro,"
                               ":f6_short, :f6_macro, :f7_short, :f7_macro)") )
    {
        qWarning() << "Cannot prepare Insert statement";
        return;
    }

    if ( deleteQuery.exec() )
    {
        const QStringList &keys = profileNameList();
        for ( auto &key: keys )
        {
            const CWShortcutProfile &cwShortcutProfile = getProfile(key);

            insertQuery.bindValue(":profile_name", key);
            for ( int i = 0; i < cwShortcutProfile.shortDescs.size(); i++ )
            {
                insertQuery.bindValue(QString(":f%1_short").arg(i+1), cwShortcutProfile.shortDescs[i]);
            }

            for ( int i = 0; i < cwShortcutProfile.macros.size(); i++ )
            {
                insertQuery.bindValue(QString(":f%1_macro").arg(i+1), cwShortcutProfile.macros[i]);
            }

            if ( ! insertQuery.exec() )
            {
                qInfo() << "CW Shortcut Profile DB insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
            }
        }
    }
    else
    {
        qInfo() << "CW Shortcut Profile DB delete error " << deleteQuery.lastError().text();
    }

    saveCurProfile1();
}

bool CWShortcutProfile::operator==(const CWShortcutProfile &profile)
{
    return (profile.profileName == this->profileName
            && profile.shortDescs == this->shortDescs
            && profile.macros == this->macros);
}

bool CWShortcutProfile::operator!=(const CWShortcutProfile &profile)
{
    return !operator==(profile);
}
