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
        << v.operatorName << v.operatorCallsign << v.qthName << v.iota
        << v.sota << v.sig << v.sigInfo << v.vucc
        << v.wwff << v.pota << v.ituz << v.cqz << v.dxcc << v.country << v.county << v.darcDOK;
    return out;
}

QDataStream& operator>>(QDataStream& in, StationProfile& v)
{
    in >> v.profileName;
    in >> v.callsign;
    in >> v.locator;
    in >> v.operatorName;
    in >> v.operatorCallsign;
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
    in >> v.county;
    in >> v.darcDOK;

    return in;
}

StationProfilesManager::StationProfilesManager() :
    ProfileManagerSQL<StationProfile>("station_profiles")
{
    FCT_IDENTIFICATION;

    QSettings settings;

    QSqlQuery profileQuery;

    if ( ! profileQuery.prepare("SELECT profile_name, callsign, locator, "
                                "operator_name, qth_name, iota, sota, sig, sig_info, vucc, pota, "
                                "ituz, cqz, dxcc, country, county, operator_callsign, darc_dok "
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
            profileDB.county = profileQuery.value(15).toString();
            profileDB.operatorCallsign = profileQuery.value(16).toString();
            profileDB.darcDOK = profileQuery.value(17).toString();

            addProfile(profileDB.profileName, profileDB);
        }
    }
    else
    {
        qInfo() << "Station Profile DB select error " << profileQuery.lastError().text();
    }
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

    if ( ! insertQuery.prepare("INSERT INTO station_profiles(profile_name, callsign, locator, operator_name, qth_name, iota, sota, sig, sig_info, vucc, wwff, pota, ituz, cqz, dxcc, country, county, operator_callsign, darc_dok) "
                        "VALUES (:profile_name, :callsign, :locator, :operator_name, :qth_name, :iota, :sota, :sig, :sig_info, :vucc, :wwff, :pota, :ituz, :cqz, :dxcc, :country, :county, :operator_callsign, :darc_dok)") )
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
            insertQuery.bindValue(":county", stationProfile.county);
            insertQuery.bindValue(":operator_callsign", stationProfile.operatorCallsign);
            insertQuery.bindValue(":darc_dok", stationProfile.darcDOK);

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
            && profile.country == this->country
            && profile.county == this->county
            && profile.operatorCallsign == this->operatorCallsign
            && profile.darcDOK == this->darcDOK
            );
}

bool StationProfile::operator!=(const StationProfile &profile)
{
    return !operator==(profile);
}

QString StationProfile::toHTMLString() const
{
    QString ret = "<b>" + QObject::tr("Logging Station Callsign") + ":</b> " + callsign + "<br/>" +
                  ((!locator.isEmpty()) ? "<b>" + QObject::tr("My Gridsquare") + ":</b> " + locator + "<br/>" : "") +
                  ((!operatorName.isEmpty()) ? "<b>" + QObject::tr("Operator Name") + ":</b> " + operatorName + "<br/>" : "") +
                  ((!operatorCallsign.isEmpty()) ? "<b>" + QObject::tr("Operator Callsign") + ":</b> " + operatorCallsign + "<br/>" : "") +
                  ((!qthName.isEmpty()) ? "<b>" + QObject::tr("My City") + ":</b> " + qthName + "<br/>" : "") +
                  ((!country.isEmpty()) ? "<b>" + QObject::tr("My Country") + ":</b> " + country + "<br/>" : "") +
                  ((!county.isEmpty()) ? "<b>" + QObject::tr("My County") + ":</b> " + county + "<br/>" : "") +
                  ((!iota.isEmpty()) ? "<b>" + QObject::tr("My IOTA") + ":</b> " + iota + "<br/>" : "") +
                  ((!sota.isEmpty()) ? "<b>" + QObject::tr("My SOTA") + ":</b> " + sota + "<br/>" : "" ) +
                  ((!sig.isEmpty()) ? "<b>" + QObject::tr("My Special Interest Activity") + ":</b> " + sig + "<br/>" : "" )+
                  ((!sigInfo.isEmpty()) ? "<b>" + QObject::tr("My Spec. Interes Activity Info") + ":</b> " + sigInfo + "<br/>" : "" )+
                  ((!vucc.isEmpty()) ? "<b>" + QObject::tr("My VUCC Grids") + ":</b> " + vucc + "<br/>" : "") +
                  ((!wwff.isEmpty()) ? "<b>" + QObject::tr("My WWFF") + ":</b> " + wwff + "<br/>" : "") +
                  ((!pota.isEmpty()) ? "<b>" + QObject::tr("My POTA Ref") + ":</b> " + pota : "") +
                  ((!pota.isEmpty()) ? "<b>" + QObject::tr("My DARC DOK") + ":</b> " + darcDOK : "") +
                  ((ituz != 0) ? "<b>" + QObject::tr("My ITU") + ":</b> " + QString::number(ituz) : "") + " " +
                  ((cqz != 0) ? "<b>" + QObject::tr("My CQZ") + ":</b> " + QString::number(cqz) : "") + " " +
                  ((dxcc != 0) ? "<b>" + QObject::tr("My DXCC") + ":</b> " + QString::number(dxcc) : "");
    return ret;
}

QString StationProfile::getContactInnerJoin() const
{
    QStringList ret({"contacts.station_callsign = station_profiles.callsign",
                     "contacts.my_gridsquare = station_profiles.locator"});

    auto addIfNoEmpty = [&](const QString &field, const QString& contact, const QString &profile)
    {
        if ( ! field.isEmpty() ) ret << "contacts." + contact + " = station_profiles." + profile;
    };

    auto addIfNoEmptyNumber = [&](int field, const QString& contact, const QString &profile)
    {
        if ( field ) ret << "contacts." + contact + " = station_profiles." + profile;
    };

    addIfNoEmpty(operatorName, "my_name_intl", "operator_name");
    addIfNoEmpty(qthName, "my_city_intl", "qth_name");
    addIfNoEmpty(iota, "my_iota", "iota");
    addIfNoEmpty(sota, "my_sota", "sota");
    addIfNoEmpty(sig, "my_sig_intl", "sig");
    addIfNoEmpty(sigInfo, "my_sig_info_intl", "sig_info");
    addIfNoEmpty(vucc, "my_vucc_grids", "vucc");
    addIfNoEmpty(wwff, "my_wwff_ref", "wwff");
    addIfNoEmpty(pota, "my_pota_ref", "pota");
    addIfNoEmptyNumber(ituz, "my_itu_zone", "ituz");
    addIfNoEmptyNumber(cqz, "my_cq_zone", "cqz");
    addIfNoEmptyNumber(dxcc, "my_dxcc", "dxcc");
    // skipping Country - depends on dxcc
    addIfNoEmpty(county, "my_cnty", "county");
    addIfNoEmpty(operatorCallsign, "operator", "operator_callsign");
    addIfNoEmpty(darcDOK, "my_darc_dok", "darc_doc");

    return "(" + ret.join(" AND ") + ")";
}
