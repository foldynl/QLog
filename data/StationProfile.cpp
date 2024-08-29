#include "data/StationProfile.h"
#include "core/debug.h"

#include <QSettings>
#include <QVariant>
#include <QSqlQuery>
#include <QSqlError>

MODULE_IDENTIFICATION("qlog.data.stationprofile");

QDataStream& operator<<(QDataStream& out, const StationProfile& v)
{
    out << v.profileName << v.callsign << v.locator
        << v.operatorName << v.qthName << v.iota
        << v.sota << v.sig << v.sigInfo << v.vucc
        << v.wwff << v.pota << v.ituz << v.cqz << v.dxcc << v.country;
    return out;
}

QDataStream& operator>>(QDataStream& in, StationProfile& v)
{
    in >> v.profileName;
    in >> v.callsign;
    in >> v.locator;
    in >> v.operatorName;
    in >> v.qthName;
    in >> v.iota;
    in >> v.sota;
    in >> v.sig;
    in >> v.sigInfo;
    in >> v.vucc;
    in >> v.wwff;
    in >> v.pota;
    in >> v.ituz;
    in >> v.cqz;
    in >> v.dxcc;
    in >> v.country;

    return in;
}

StationProfilesManager::StationProfilesManager(QObject *parent) :
    QObject(parent),
    ProfileManagerSQL<StationProfile>("station_profiles")
{
    FCT_IDENTIFICATION;

    QSettings settings;

    QSqlQuery profileQuery;

    if ( ! profileQuery.prepare("SELECT profile_name, callsign, locator, "
                                "operator_name, qth_name, iota, sota, sig, sig_info, vucc, pota, "
                                "ituz, cqz, dxcc, country "
                                "FROM station_profiles") )
    {
        qWarning()<< "Cannot prepare select";
    }

    if ( profileQuery.exec() )
    {
        while (profileQuery.next())
        {
            StationProfile profileDB;
            profileDB.profileName = profileQuery.value(0).toString();
            profileDB.callsign =  profileQuery.value(1).toString();
            profileDB.locator =  profileQuery.value(2).toString();
            profileDB.operatorName =  profileQuery.value(3).toString();
            profileDB.qthName =  profileQuery.value(4).toString();
            profileDB.iota =  profileQuery.value(5).toString();
            profileDB.sota =  profileQuery.value(6).toString();
            profileDB.sig =  profileQuery.value(7).toString();
            profileDB.sigInfo =  profileQuery.value(8).toString();
            profileDB.vucc =  profileQuery.value(9).toString();
            profileDB.pota = profileQuery.value(10).toString();
            profileDB.ituz = profileQuery.value(11).toInt();
            profileDB.cqz = profileQuery.value(12).toInt();
            profileDB.dxcc = profileQuery.value(13).toInt();
            profileDB.country = profileQuery.value(14).toString();

            addProfile(profileDB.profileName, profileDB);
        }
    }
    else
    {
        qInfo() << "Station Profile DB select error " << profileQuery.lastError().text();
    }
}

StationProfilesManager *StationProfilesManager::instance()
{
    FCT_IDENTIFICATION;

    static StationProfilesManager instance;
    return &instance;
}

void StationProfilesManager::save()
{
    FCT_IDENTIFICATION;
    QSettings settings;

    QSqlQuery deleteQuery;
    QSqlQuery insertQuery;

    if ( ! deleteQuery.prepare("DELETE FROM station_profiles") )
    {
        qWarning() << "cannot prepare Delete statement";
        return;
    }

    if ( ! insertQuery.prepare("INSERT INTO station_profiles(profile_name, callsign, locator, operator_name, qth_name, iota, sota, sig, sig_info, vucc, wwff, pota, ituz, cqz, dxcc, country) "
                        "VALUES (:profile_name, :callsign, :locator, :operator_name, :qth_name, :iota, :sota, :sig, :sig_info, :vucc, :wwff, :pota, :ituz, :cqz, :dxcc, :country)") )
    {
        qWarning() << "cannot prepare Insert statement";
        return;
    }

    if ( deleteQuery.exec() )
    {
        const QStringList &keys = profileNameList();
        for ( const QString &key : keys )
        {
            StationProfile stationProfile = getProfile(key);

            insertQuery.bindValue(":profile_name", key);
            insertQuery.bindValue(":callsign", stationProfile.callsign);
            insertQuery.bindValue(":locator", stationProfile.locator);
            insertQuery.bindValue(":operator_name", stationProfile.operatorName);
            insertQuery.bindValue(":qth_name", stationProfile.qthName);
            insertQuery.bindValue(":iota", stationProfile.iota);
            insertQuery.bindValue(":sota", stationProfile.sota);
            insertQuery.bindValue(":sig", stationProfile.sig);
            insertQuery.bindValue(":sig_info", stationProfile.sigInfo);
            insertQuery.bindValue(":vucc", stationProfile.vucc);
            insertQuery.bindValue(":wwff", stationProfile.wwff);
            insertQuery.bindValue(":pota", stationProfile.pota);
            insertQuery.bindValue(":ituz", stationProfile.ituz);
            insertQuery.bindValue(":cqz", stationProfile.cqz);
            insertQuery.bindValue(":dxcc", stationProfile.dxcc);
            insertQuery.bindValue(":country", stationProfile.country);

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

bool StationProfile::operator==(const StationProfile &profile)
{
    return (profile.profileName == this->profileName
            && profile.callsign == this->callsign
            && profile.locator == this->locator
            && profile.operatorName == this->operatorName
            && profile.qthName == this->qthName
            && profile.iota == this->iota
            && profile.sota == this->sota
            && profile.sig == this->sig
            && profile.sigInfo == this->sigInfo
            && profile.vucc == this->vucc
            && profile.wwff == this->wwff
            && profile.pota == this->pota
            && profile.ituz == this->ituz
            && profile.cqz == this->cqz
            && profile.dxcc == this->dxcc
            && profile.country == this->country);
}

bool StationProfile::operator!=(const StationProfile &profile)
{
    return !operator==(profile);
}

QString StationProfile::toHTMLString() const
{
    QString ret = "<b>" + QObject::tr("Logging Station Callsign") + ":</b> " + callsign + "<br/>" +
                  ((!locator.isEmpty()) ? "<b>" + QObject::tr("My Gridsquare") + ":</b> " + locator + "<br/>" : "") +
                  ((!operatorName.isEmpty()) ? "<b>" + QObject::tr("My Name") + ":</b> " + operatorName + "<br/>" : "") +
                  ((!qthName.isEmpty()) ? "<b>" + QObject::tr("My City") + ":</b> " + qthName + "<br/>" : "") +
                  ((!iota.isEmpty()) ? "<b>" + QObject::tr("My IOTA") + ":</b> " + iota + "<br/>" : "") +
                  ((!sota.isEmpty()) ? "<b>" + QObject::tr("My SOTA") + ":</b> " + sota + "<br/>" : "" ) +
                  ((!sig.isEmpty()) ? "<b>" + QObject::tr("My Special Interest Activity") + ":</b> " + sig + "<br/>" : "" )+
                  ((!sigInfo.isEmpty()) ? "<b>" + QObject::tr("My Spec. Interes Activity Info") + ":</b> " + sigInfo + "<br/>" : "" )+
                  ((!vucc.isEmpty()) ? "<b>" + QObject::tr("My VUCC Grids") + ":</b> " + vucc + "<br/>" : "") +
                  ((!wwff.isEmpty()) ? "<b>" + QObject::tr("My WWFF") + ":</b> " + wwff + "<br/>" : "") +
                  ((!pota.isEmpty()) ? "<b>" + QObject::tr("My POTA Ref") + ":</b> " + pota : "") +
                  ((ituz != 0) ? "<b>" + QObject::tr("My ITU") + ":</b> " + QString::number(ituz) : "") + " " +
                  ((cqz != 0) ? "<b>" + QObject::tr("My CQZ") + ":</b> " + QString::number(cqz) : "") + " " +
                  ((dxcc != 0) ? "<b>" + QObject::tr("My DXCC") + ":</b> " + QString::number(dxcc) : "");
    return ret;
}
