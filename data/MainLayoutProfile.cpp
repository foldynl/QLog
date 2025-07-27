#include <QSqlQuery>
#include <QSqlError>
#include <QByteArray>

#include "MainLayoutProfile.h"
#include "core/debug.h"
#include "models/LogbookModel.h"

MODULE_IDENTIFICATION("qlog.data.mainlayoutprofile");

QDataStream& operator<<(QDataStream& out, const MainLayoutProfile& v)
{
    out << v.profileName
        << v.rowA
        << v.rowB
        << v.detailColA
        << v.detailColB
        << v.detailColC
        << v.mainGeometry
        << v.mainState
        << v.darkMode
        << v.tabsexpanded
        << v.addlBandmaps;


    return out;
}

QDataStream& operator>>(QDataStream& in, MainLayoutProfile& v)
{
    in >> v.profileName;
    in >> v.rowA;
    in >> v.rowB;
    in >> v.detailColA;
    in >> v.detailColB;
    in >> v.detailColC;
    in >> v.mainGeometry;
    in >> v.mainState;
    in >> v.darkMode;
    in >> v.tabsexpanded;
    in >> v.addlBandmaps;

    return in;
}

MainLayoutProfilesManager::MainLayoutProfilesManager() :
    ProfileManagerSQL<MainLayoutProfile>("main_layout_profiles")
{
    FCT_IDENTIFICATION;

    QSqlQuery profileQuery;

    if ( ! profileQuery.prepare("SELECT profile_name, row_A, row_B, detail_col_A, "
                                "detail_col_B, detail_col_C, main_geometry, main_state, "
                                "dark_mode, tabsexpanded, addlbandmaps "
                                "FROM main_layout_profiles") )
    {
        qWarning()<< "Cannot prepare select";
    }

    if ( profileQuery.exec() )
    {
        while (profileQuery.next())
        {
            MainLayoutProfile profileDB;
            profileDB.profileName = profileQuery.value(0).toString();
            profileDB.rowA = toIntList(profileQuery.value(1).toString());
            profileDB.rowB = toIntList(profileQuery.value(2).toString());
            profileDB.detailColA = toIntList(profileQuery.value(3).toString());
            profileDB.detailColB = toIntList(profileQuery.value(4).toString());
            profileDB.detailColC = toIntList(profileQuery.value(5).toString());
            profileDB.mainGeometry = QByteArray::fromBase64(profileQuery.value(6).toString().toUtf8());
            profileDB.mainState = QByteArray::fromBase64(profileQuery.value(7).toString().toUtf8());
            profileDB.darkMode = profileQuery.value(8).toInt();
            profileDB.tabsexpanded = profileQuery.value(9).toBool();
            profileDB.addlBandmaps = toPairStringList(profileQuery.value(10).toString());
            addProfile(profileDB.profileName, profileDB);
        }
    }
    else
    {
        qInfo() << "MainLayout Profile DB select error " << profileQuery.lastError().text();
    }
}

void MainLayoutProfilesManager::save()
{
    FCT_IDENTIFICATION;

    QSqlQuery deleteQuery;
    QSqlQuery insertQuery;

    if ( ! deleteQuery.prepare("DELETE FROM main_layout_profiles") )
    {
        qWarning() << "Cannot prepare Delete statement";
        return;
    }

    if ( ! insertQuery.prepare("INSERT INTO main_layout_profiles(profile_name, row_A, row_B, detail_col_A, detail_col_B, detail_col_C, main_geometry, main_state, dark_mode, tabsexpanded, addlbandmaps) "
                               "VALUES (:profile_name, :row_A, :row_B, :detail_col_A, :detail_col_B, :detail_col_C, :main_geometry, :main_state, :dark_mode, :tabsexpanded, :addlbandmaps)") )
    {
        qWarning() << "Cannot prepare Insert statement";
        return;
    }

    if ( deleteQuery.exec() )
    {
        const QStringList &keys = profileNameList();
        for ( const QString &key: keys )
        {
            const MainLayoutProfile &layoutProfile = getProfile(key);

            insertQuery.bindValue(":profile_name", key);
            insertQuery.bindValue(":row_A", toDBStringList(layoutProfile.rowA));
            insertQuery.bindValue(":row_B", toDBStringList(layoutProfile.rowB));
            insertQuery.bindValue(":detail_col_A", toDBStringList(layoutProfile.detailColA));
            insertQuery.bindValue(":detail_col_B", toDBStringList(layoutProfile.detailColB));
            insertQuery.bindValue(":detail_col_C", toDBStringList(layoutProfile.detailColC));
            insertQuery.bindValue(":main_geometry", layoutProfile.mainGeometry.toBase64());
            insertQuery.bindValue(":main_state", layoutProfile.mainState.toBase64());
            insertQuery.bindValue(":dark_mode", layoutProfile.darkMode);
            insertQuery.bindValue(":tabsexpanded", layoutProfile.tabsexpanded);
            insertQuery.bindValue(":addlbandmaps", toDBStringList(layoutProfile.addlBandmaps));

            if ( ! insertQuery.exec() )
            {
                qInfo() << "MainLayoutProfile DB insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
            }
        }
    }
    else
    {
        qInfo() << "MainLayoutProfile Profile DB delete error " << deleteQuery.lastError().text();
    }

    saveCurProfile1();
}

QString MainLayoutProfilesManager::toDBStringList(const QList<int> &list)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << list;

    QStringList stringsList;
    for ( const int item : list)
    {
        stringsList << QString::number(item);
    }
    qCDebug(runtime) << "return:" << stringsList;
    return stringsList.join(",");
}

QString MainLayoutProfilesManager::toDBStringList(const QList<QPair<QString, QString> > &list)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << list;

    QStringList stringsList;
    for ( const QPair<QString, QString> &item : list)
        stringsList << item.first + "/" + item.second;

    qCDebug(runtime) << "return:" << stringsList;
    return stringsList.join(",");
}

QList<int> MainLayoutProfilesManager::toIntList(const QString &list) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << list;

    QList<int> retList;

    if ( list.isEmpty() )
        return retList;

    const QStringList splitList = list.split(",");
    for ( const QString &item : splitList )
    {
        retList << item.toInt();
    }

    qCDebug(runtime) << "return:" << retList;
    return retList;
}

QList<QPair<QString, QString> > MainLayoutProfilesManager::toPairStringList(const QString &list)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << list;

    QList<QPair<QString, QString>> retList;

    if ( list.isEmpty() )
        return retList;

    const QStringList &elements = list.split(",");

    for ( const QString &elementParams : elements )
    {
        const QStringList &params = elementParams.split("/");
        if ( params.size() == 2 )
            retList << QPair<QString, QString>(params.at(0), params.at(1));
        else
            qWarning() << "Unexpected number of Bandmap widget params" << elementParams;
    }

    return retList;
}

bool MainLayoutProfile::operator==(const MainLayoutProfile &profile)
{
    return (profile.profileName == this->profileName
            && profile.rowA == this->rowA
            && profile.rowB == this->rowB
            && profile.detailColA == this->detailColA
            && profile.detailColB == this->detailColB
            && profile.detailColC == this->detailColC
            && profile.mainGeometry == this->mainGeometry
            && profile.mainState == this->mainState
            && profile.darkMode == this->darkMode
            && profile.tabsexpanded == this->tabsexpanded
            && profile.addlBandmaps == this->addlBandmaps);
}

bool MainLayoutProfile::operator!=(const MainLayoutProfile &profile)
{
    return !operator==(profile);
}

MainLayoutProfile MainLayoutProfile::getClassicLayout()
{

    MainLayoutProfile ret;

    ret.rowA << LogbookModel::COLUMN_NAME_INTL
             << LogbookModel::COLUMN_QTH_INTL
             << LogbookModel::COLUMN_GRID
             << LogbookModel::COLUMN_COMMENT_INTL;

    ret.detailColA << LogbookModel::COLUMN_CONTINENT
                   << LogbookModel::COLUMN_ITUZ
                   << LogbookModel::COLUMN_CQZ
                   << LogbookModel::COLUMN_STATE
                   << LogbookModel::COLUMN_COUNTY
                   << LogbookModel::COLUMN_AGE
                   << LogbookModel::COLUMN_VUCC_GRIDS;

    ret.detailColB << LogbookModel::COLUMN_DARC_DOK
                   << LogbookModel::COLUMN_IOTA
                   << LogbookModel::COLUMN_POTA_REF
                   << LogbookModel::COLUMN_SOTA_REF
                   << LogbookModel::COLUMN_WWFF_REF
                   << LogbookModel::COLUMN_SIG_INTL
                   << LogbookModel::COLUMN_SIG_INFO_INTL;

    ret.detailColC << LogbookModel::COLUMN_EMAIL
                   << LogbookModel::COLUMN_WEB;

    return ret;
}
