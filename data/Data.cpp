#include <QJsonDocument>
#include <QSqlQuery>
#include <QSqlError>
#include <QCompleter>
#include <QColor>
#include "Data.h"
#include "data/Callsign.h"
#include "core/debug.h"
#include "BandPlan.h"
#include "data/StationProfile.h"
#include "core/LogParam.h"

MODULE_IDENTIFICATION("qlog.data.data");

Data::Data(QObject *parent) :
   QObject(parent),
   zd(nullptr),
   isDXCCQueryValid(false)
{
    FCT_IDENTIFICATION;

    loadContests();
    loadPropagationModes();
    loadLegacyModes();
    loadDxccFlags();
    loadSatModes();
    loadIOTA();
    loadSOTA();
    loadWWFF();
    loadPOTA();
    loadTZ();


    // dxcc_prefixes_ad1c.exact DESC, dxcc_prefixes_ad1c.prefix DESC
    // is used because it prefers an exact-match record over a partial-match records
    isDXCCQueryValid = queryDXCC.prepare(
                "SELECT "
                "    dxcc_entities_ad1c.id, "
                "    dxcc_entities_ad1c.name, "
                "    dxcc_entities_ad1c.prefix, "
                "    dxcc_entities_ad1c.cont, "
                "    CASE "
                "        WHEN (dxcc_prefixes_ad1c.cqz != 0) "
                "        THEN dxcc_prefixes_ad1c.cqz "
                "        ELSE dxcc_entities_ad1c.cqz "
                "    END AS cqz, "
                "    CASE "
                "        WHEN (dxcc_prefixes_ad1c.ituz != 0) "
                "        THEN dxcc_prefixes_ad1c.ituz "
                "        ELSE dxcc_entities_ad1c.ituz "
                "    END AS ituz , "
                "    dxcc_entities_ad1c.lat, "
                "    dxcc_entities_ad1c.lon, "
                "    dxcc_entities_ad1c.tz, "
                "    dxcc_prefixes_ad1c.exact "
                "FROM dxcc_prefixes_ad1c "
                "INNER JOIN dxcc_entities_ad1c ON (dxcc_prefixes_ad1c.dxcc = dxcc_entities_ad1c.id) "
                "WHERE (dxcc_prefixes_ad1c.prefix = :callsign and dxcc_prefixes_ad1c.exact = true) "
                "    OR (dxcc_prefixes_ad1c.exact = false and :callsign LIKE dxcc_prefixes_ad1c.prefix || '%') "
                "ORDER BY dxcc_prefixes_ad1c.exact DESC, dxcc_prefixes_ad1c.prefix DESC "
                "LIMIT 1 "
                );

    isDXCCClublogQueryValid = queryDXCCClublog.prepare(
                "SELECT e.id, "
                "      e.name, "
                "      e.prefix, "
                "      e.cont, "
                "      COALESCE(z.cqz, "
                "               CASE  WHEN (p.cqz != 0) THEN p.cqz ELSE e.cqz END) AS cqz, "
                "      CASE WHEN z.cqz IS NOT NULL THEN NULL "
                "           WHEN (p.ituz != 0) THEN p.ituz "
                "           ELSE e.ituz END AS ituz, "
                "      e.lat, "
                "      e.lon, "
                "      p.exact "
                " FROM dxcc_prefixes_clublog p"
                "   INNER JOIN dxcc_entities_clublog e ON (p.dxcc = e.id) "
                "   LEFT JOIN dxcc_zone_exceptions_clublog z ON ( z.call = :exactcall AND :dxccdate BETWEEN COALESCE(z.start, '0001-01-01 00:00:00') "
                "                                                                                    AND COALESCE(z.end, '9999-12-31 23:59:59')) "
                " WHERE ((p.prefix = :exactcall and p.exact = 1) "
                "     OR (p.exact = 0 and :modifiedcall LIKE p.prefix || '%')) "
                "    AND :dxccdate BETWEEN COALESCE(p.start, '0001-01-01 00:00:00') "
                "                          AND COALESCE(p.end, '9999-12-31 23:59:59') "
                " ORDER BY p.exact DESC, p.prefix DESC LIMIT 1"
                );

    isDXCCIDAD1CQueryValid = queryDXCCIDAD1C.prepare(
                " SELECT dxcc_entities_ad1c.id, dxcc_entities_ad1c.name, dxcc_entities_ad1c.prefix, dxcc_entities_ad1c.cont, "
                "        dxcc_entities_ad1c.cqz, dxcc_entities_ad1c.ituz, dxcc_entities_ad1c.lat, dxcc_entities_ad1c.lon, dxcc_entities_ad1c.tz "
                " FROM dxcc_entities_ad1c "
                " WHERE dxcc_entities_ad1c.id = :dxccid"
                );

    isDXCCIDClublogQueryValid = queryDXCCIDClublog.prepare(
                " SELECT c.id, c.name, c.prefix, c.cont, "
                "        c.cqz, c.ituz, c.lat, c.lon "
                " FROM dxcc_entities_clublog c "
                " WHERE c.id = :dxccid"
                );

    isSOTAQueryValid = querySOTA.prepare(
                "SELECT summit_code,"
                "       association_name,"
                "       region_name,"
                "       summit_name,"
                "       altm,"
                "       altft,"
                "       gridref1,"
                "       gridref2,"
                "       longitude,"
                "       latitude,"
                "       points,"
                "       bonus_points,"
                "       valid_from,"
                "       valid_to "
                "FROM sota_summits "
                "WHERE summit_code = :code"
                );

    isPOTAQueryValid = queryPOTA.prepare(
                "SELECT reference,"
                "       name,"
                "       active,"
                "       entityID,"
                "       locationDesc,"
                "       latitude,"
                "       longitude,"
                "       grid "
                "FROM pota_directory "
                "WHERE reference = :code"
                );

    isWWFFQueryValid = queryWWFF.prepare(
                "SELECT reference,"
                "       status,"
                "       name,"
                "       program,"
                "       dxcc,"
                "       state,"
                "       county,"
                "       continent,"
                "       iota,"
                "       iaruLocator,"
                "       latitude,"
                "       longitude,"
                "       iucncat,"
                "       valid_from,"
                "       valid_to "
                "FROM wwff_directory "
                "WHERE reference = :reference"
                );
}

Data::~Data()
{
    FCT_IDENTIFICATION;

    if ( zd )
    {
        ZDCloseDatabase(zd);
    }
}

#define RETCODE(a)  \
    dxccStatusCache.insert(dxcc, myDXCC, band, mode, new DxccStatus(a)); \
    return ((a));

DxccStatus Data::dxccStatus(int dxcc, const QString &band, const QString &mode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << dxcc << " " << band << " " << mode;

    const int myDXCC = StationProfilesManager::instance()->getCurProfile1().dxcc;

    DxccStatus *statusFromCache = dxccStatusCache.value(dxcc, myDXCC, band, mode);

    if ( statusFromCache )
        return *statusFromCache;

    // FTx modes (FT8, FT4, FT2) are stored in contacts with modes.dxcc = 'DIGITAL',
    // so we use DIGITAL as the effective mode group when querying for FTx.
    const QString modeForQuery = ( mode == BandPlan::MODE_GROUP_STRING_FTx )
                                 ? BandPlan::MODE_GROUP_STRING_DIGITAL
                                 : mode;

    QString sql_mode(":mode");

    if ( modeForQuery != BandPlan::MODE_GROUP_STRING_CW
         && modeForQuery != BandPlan::MODE_GROUP_STRING_PHONE
         && modeForQuery != BandPlan::MODE_GROUP_STRING_DIGITAL )
    {
        sql_mode = "(SELECT modes.dxcc FROM modes WHERE modes.name = :mode LIMIT 1) ";
    }


    QStringList dxccConfirmedByCond(QLatin1String("0=1")); // if no option is selected then always false

    if ( LogParam::getDxccConfirmedByLotwState() )
        dxccConfirmedByCond << QLatin1String("all_dxcc_qsos.lotw_qsl_rcvd = 'Y'");

    if ( LogParam::getDxccConfirmedByPaperState() )
        dxccConfirmedByCond << QLatin1String("all_dxcc_qsos.qsl_rcvd = 'Y'");

    if ( LogParam::getDxccConfirmedByEqslState() )
        dxccConfirmedByCond << QLatin1String("all_dxcc_qsos.eqsl_qsl_rcvd = 'Y'");

    QSqlQuery query;
    QString sqlStatement = QString("WITH all_dxcc_qsos AS (SELECT DISTINCT contacts.mode, contacts.band, "
                                         "                                       contacts.qsl_rcvd, contacts.lotw_qsl_rcvd, contacts.eqsl_qsl_rcvd "
                                         "                       FROM contacts "
                                         "                       WHERE dxcc = :dxcc %1) "
                                         "  SELECT (SELECT 1 FROM all_dxcc_qsos LIMIT 1) as entity,"
                                         "         (SELECT 1 FROM all_dxcc_qsos WHERE band = :band LIMIT 1) as band, "
                                         "         (SELECT 1 FROM all_dxcc_qsos INNER JOIN modes ON (modes.name = all_dxcc_qsos.mode) "
                                         "          WHERE modes.dxcc = %2 LIMIT 1) as mode, "
                                         "         (SELECT 1 FROM all_dxcc_qsos INNER JOIN modes ON (modes.name = all_dxcc_qsos.mode) "
                                         "          WHERE modes.dxcc = %3 AND all_dxcc_qsos.band = :band LIMIT 1) as slot, "
                                         "         (SELECT 1 FROM all_dxcc_qsos INNER JOIN modes ON (modes.name = all_dxcc_qsos.mode) "
                                         "          WHERE modes.dxcc = %4 AND all_dxcc_qsos.band = :band "
                                         "                AND (%5) LIMIT 1) as confirmed")
                                         .arg(( myDXCC != 0 ) ? QString(" AND my_dxcc = %1").arg(myDXCC)
                                                                    : "",
                                              sql_mode,
                                              sql_mode,
                                              sql_mode,
                                              dxccConfirmedByCond.join(" OR "));

    if ( ! query.prepare(sqlStatement) )
    {
        qWarning() << "Cannot prepare Select statement";
        return DxccStatus::UnknownStatus;
    }

    query.bindValue(":dxcc", dxcc);
    query.bindValue(":band", band);
    query.bindValue(":mode", modeForQuery);

    if ( ! query.exec() )
    {
        qWarning() << "Cannot execute Select statement" << query.lastError();
        return DxccStatus::UnknownStatus;
    }

    if ( query.next() )
    {
        if ( query.value(0).toString().isEmpty() )
        {
            RETCODE(DxccStatus::NewEntity);
        }

        if ( query.value(1).toString().isEmpty() )
        {
            if ( query.value(2).toString().isEmpty() )
            {
                RETCODE(DxccStatus::NewBandMode);
            }
            else
            {
                RETCODE(DxccStatus::NewBand);
            }
        }

        if ( query.value(2).toString().isEmpty() )
        {
            RETCODE(DxccStatus::NewMode);
        }

        if ( query.value(3).toString().isEmpty() )
        {
            RETCODE(DxccStatus::NewSlot);
        }

        if ( query.value(4).toString().isEmpty() )
        {
            RETCODE(DxccStatus::Worked);
        }
        else
        {
            RETCODE(DxccStatus::Confirmed);
        }
    }

    RETCODE(DxccStatus::UnknownStatus);
}
#undef RETCODE

QStringList Data::contestList()
{
    FCT_IDENTIFICATION;

    QStringList contestLOV;

    QSqlQuery query(QLatin1String("SELECT DISTINCT contest_id FROM contacts ORDER BY 1 COLLATE LOCALEAWARE ASC"));

    while ( query.next() )
        contestLOV << query.value(0).toString();

    return contestLOV + contests.keys();
}

#define RETURNCODE(a) \
    qCDebug(runtime) << "new DXCC Status: " << (a); \
    return ((a))

DxccStatus Data::dxccNewStatusWhenQSOAdded(const DxccStatus &oldStatus,
                                  const qint32 oldDxcc,
                                  const QString &oldBand,
                                  const QString &oldMode,
                                  const qint32 newDxcc,
                                  const QString &newBand,
                                  const QString &newMode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << oldStatus
                               << oldDxcc
                               << oldBand
                               << oldMode
                               << newDxcc
                               << newBand
                               << newMode;

    if ( oldDxcc != newDxcc )
    {
        /* No change */
        RETURNCODE(oldStatus);
    }

    if ( oldBand == newBand
         && oldMode == newMode )
    {
        if ( oldStatus < DxccStatus::Worked )
        {
            RETURNCODE(DxccStatus::Worked);
        }
        else
        {
            RETURNCODE(oldStatus);
        }
    }

    /*************/
    /* NewEntity */
    /*************/
    if ( oldStatus == DxccStatus::NewEntity )
    {
        if ( oldBand != newBand
             && oldMode != newMode )
        {
            RETURNCODE(DxccStatus::NewBandMode);
        }

        if ( oldBand != newBand
             && oldMode == newMode )
        {
            RETURNCODE(DxccStatus::NewBand);
        }

        if ( oldBand == newBand
             && oldMode != newMode )
        {
            RETURNCODE(DxccStatus::NewMode);
        }
    }

    /***************/
    /* NewBandMode */
    /***************/
    if ( oldStatus == DxccStatus::NewBandMode )
    {
        if ( oldBand != newBand
             && oldMode != newMode )
        {
            RETURNCODE(DxccStatus::NewBandMode);
        }

        if ( oldBand != newBand
             && oldMode == newMode )
        {
            RETURNCODE(DxccStatus::NewBand);
        }

        if ( oldBand == newBand
             && oldMode != newMode )
        {
            RETURNCODE(DxccStatus::NewMode);
        }
    }

    /*************/
    /* NewBand   */
    /*************/
    if ( oldStatus == DxccStatus::NewBand )
    {
        if ( oldBand != newBand
             && oldMode != newMode )
        {
            RETURNCODE(DxccStatus::NewBand);
        }

        if ( oldBand != newBand
             && oldMode == newMode )
        {
            RETURNCODE(DxccStatus::NewBand);
        }

        if ( oldBand == newBand
             && oldMode != newMode )
        {
            RETURNCODE(DxccStatus::NewSlot);
        }
    }

    /*************/
    /* NewMode   */
    /*************/
    if ( oldStatus == DxccStatus::NewMode )
    {
        if ( oldBand != newBand
             && oldMode != newMode )
        {
            RETURNCODE(DxccStatus::NewMode);
        }

        if ( oldBand != newBand
             && oldMode == newMode )
        {
            RETURNCODE(DxccStatus::NewSlot);
        }

        if ( oldBand == newBand
             && oldMode != newMode )
        {
            RETURNCODE(DxccStatus::NewMode);
        }
    }

    /*************/
    /* NewSlot   */
    /*************/
    if ( oldStatus == DxccStatus::NewSlot )
    {
        RETURNCODE(DxccStatus::NewSlot);
    }

    /*************/
    /* Worked   */
    /*************/
    if ( oldStatus == DxccStatus::Worked )
    {
        RETURNCODE(DxccStatus::Worked);
    }

    /***************/
    /* Confirmed   */
    /***************/
    if ( oldStatus == DxccStatus::Confirmed )
    {
        RETURNCODE(DxccStatus::Confirmed);
    }

    RETURNCODE(DxccStatus::UnknownStatus);
}

qulonglong Data::dupeNewCountWhenQSOAdded(qulonglong oldCounter,
                                          const QString &oldBand,
                                          const QString &oldMode,
                                          const QString &addedBand,
                                          const QString &addedMode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << oldCounter
                                 << oldBand
                                 << oldMode
                                 << addedBand
                                 << addedMode;

    int dupeType = LogParam::getContestDupeType();
    const QString &contestID = LogParam::getContestID();

    qCDebug(runtime) << dupeType << contestID;

    if ( contestID.isEmpty() )
        return oldCounter;

    bool shouldIncrease = (dupeType == DupeType::ALL_BANDS) ||
                          (dupeType == DupeType::EACH_BAND && oldBand == addedBand) ||
                          (dupeType == DupeType::EACH_BAND_MODE && oldBand == addedBand && oldMode == addedMode);

    RETURNCODE( shouldIncrease ? oldCounter + 1 : oldCounter);
}

qulonglong Data::dupeNewCountWhenQSODelected(qulonglong oldCounter,
                                             const QString &oldBand,
                                             const QString &oldMode,
                                             const QString &deletedBand,
                                             const QString &deletedMode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << oldCounter
                                 << oldBand
                                 << oldMode
                                 << deletedBand
                                 << deletedMode;

    if ( oldCounter == 0 )
        return 0;

    int dupeType = LogParam::getContestDupeType();
    const QString &contestID = LogParam::getContestID();

    qCDebug(runtime) << dupeType << contestID;

    if ( contestID.isEmpty() )
        return oldCounter;

    bool shouldDecrease = (dupeType == DupeType::ALL_BANDS) ||
                          (dupeType == DupeType::EACH_BAND && oldBand == deletedBand) ||
                          (dupeType == DupeType::EACH_BAND_MODE && oldBand == deletedBand && oldMode == deletedMode);

    RETURNCODE( shouldDecrease ? oldCounter - 1 : oldCounter);
}

#undef RETURNCODE

QColor Data::statusToColor(const DxccStatus &status, bool isDupe, const QColor &defaultColor) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << status << isDupe;

    if ( isDupe )
        return QColor(109, 109, 109, 100);

    switch (status) {
        case DxccStatus::NewEntity:
            return QColor(255, 58, 9);
        case DxccStatus::NewBand:
        case DxccStatus::NewMode:
        case DxccStatus::NewBandMode:
            return QColor(76, 200, 80);
        case DxccStatus::NewSlot:
            return QColor(30, 180, 230);
        case DxccStatus::Worked:
            return QColor(255,165,0);
        default:
            return defaultColor;
    }
}

QString Data::colorToHTMLColor(const QColor &in_color)
{
    FCT_IDENTIFICATION;

    return in_color.name(QColor::HexRgb);
}

QString Data::statusToText(const DxccStatus &status) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << status;

    switch (status) {
        case DxccStatus::NewEntity:
            return tr("New Entity");
        case DxccStatus::NewBand:
            return tr("New Band");
        case DxccStatus::NewMode:
            return tr("New Mode");
        case DxccStatus::NewBandMode:
            return tr("New Band&Mode");
        case DxccStatus::NewSlot:
            return tr("New Slot");
        case DxccStatus::Confirmed:
            return tr("Confirmed");
        case DxccStatus::Worked:
            return tr("Worked");
        default:
            return QString("Unknown");
    }
}

int Data::getITUZMin()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << 1;

    return 1;
}

int Data::getITUZMax()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << 90;

    return 90;
}

int Data::getCQZMin()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << 1;

    return 1;
}

int Data::getCQZMax()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << 40;

    return 40;
}

QString Data::debugFilename()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    const QString debugFilename = "qlog_debug_" + QDateTime::currentDateTime().toString("yyyyMMddhhmmss") + ".log";
    return dir.filePath(debugFilename);
}

double Data::MHz2UserFriendlyFreq(double freqMHz,
                                  QString &unit,
                                  unsigned char &efectiveDecP)
{
    FCT_IDENTIFICATION;

    if ( freqMHz < 0.001 )
    {
        unit = tr("Hz");
        efectiveDecP = 0;
        return freqMHz * 1000000.0;
    }

    if ( freqMHz < 1 )
    {
        unit = tr("kHz");
        efectiveDecP = 3;
        return freqMHz * 1000.0;
    }

    if ( freqMHz >= 1000 )
    {
        unit = tr("GHz");
        efectiveDecP = 3;
        return freqMHz / 1000.0;
    }

    unit = tr("MHz");
    efectiveDecP = 3;
    return freqMHz;
}

const QStringList &Data::getContinentList()
{
    static const QStringList continents{"AF", "AN", "AS", "EU", "NA", "OC", "SA"};
    return continents;
}

QPair<QString, QString> Data::legacyMode(const QString &mode)
{
    FCT_IDENTIFICATION;

    // used in the case of external programs that generate an invalid ADIF modes.
    // Database Mode Table cannot be used because these programs have different mode-strings.

    qCDebug(function_parameters) << mode;
    return legacyModes.value(mode);
}

QStringList Data::submodesForMode(const QString &mode) const
{
    FCT_IDENTIFICATION;

    if ( mode.isEmpty() )
        return QStringList();

    QSqlQuery query;
    query.prepare("SELECT submodes FROM modes WHERE name = :mode");
    query.bindValue(":mode", mode);

    if ( !query.exec() || !query.next() )
        return QStringList();

    return QJsonDocument::fromJson(query.value(0).toString().toUtf8()).toVariant().toStringList();
}

bool Data::isSubmodeForMode(const QString &mode, const QString &submode) const
{
    FCT_IDENTIFICATION;

    // Empty submode is a valid way to store a plain ADIF mode without subtype.
    if ( submode.isEmpty() )
        return true;

    return submodesForMode(mode).contains(submode);
}

QString Data::getIANATimeZone(double lat, double lon)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << lat << lon;

    QString ret;

    if ( zd )
    {
        ret = ZDHelperSimpleLookupString(zd,
                                         static_cast<float>(lat),
                                         static_cast<float>(lon));
    }

    qCDebug(runtime) << ret;
    return ret;
}

QStringList Data::sigIDList()
{
    FCT_IDENTIFICATION;

    QStringList sigLOV;
    QSqlQuery query(QLatin1String("SELECT DISTINCT sig_intl FROM contacts"));

    while ( query.next() )
        sigLOV << query.value(0).toString();

    return sigLOV;
}

void Data::invalidateDXCCStatusCache(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    dxccStatusCache.invalidate(record.value("dxcc").toInt(), StationProfilesManager::instance()->getCurProfile1().dxcc);
}

void Data::invalidateSetOfDXCCStatusCache(const QSet<uint> &entities)
{
    FCT_IDENTIFICATION;

    int myDXCC = StationProfilesManager::instance()->getCurProfile1().dxcc;

    for ( uint entity : entities )
       dxccStatusCache.invalidate(entity, myDXCC);
}

void Data::clearDXCCStatusCache()
{
    FCT_IDENTIFICATION;

    dxccStatusCache.clear();
}

qulonglong Data::countDupe(const QString &callsign,
                           const QString &band,
                           const QString &mode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign
                                 << band
                                 << mode;

    int dupeType = LogParam::getContestDupeType();
    const QString &contestID = LogParam::getContestID();
    const QDateTime &dupeStartTime = LogParam::getContestDupeDate();

    qCDebug(runtime) << dupeType <<  dupeStartTime << contestID;

    if ( contestID.isEmpty()
         || dupeType == DupeType::NO_CHECK
         || !dupeStartTime.isValid() )
        return false;

    QStringList whereClause =
    {
        "callsign = :callsign",
        "contest_id = :contestid"
    };

    if ( dupeType >= DupeType::ALL_BANDS )
        whereClause << QLatin1String("datetime(start_time) >= datetime(:date)");

    if ( dupeType >= DupeType::EACH_BAND)
        whereClause << QLatin1String("band = :band");

    // FTx modes are stored with modes.dxcc = 'DIGITAL', so map FTx to DIGITAL.
    const QString modeForQuery = ( mode == BandPlan::MODE_GROUP_STRING_FTx )
                                 ? BandPlan::MODE_GROUP_STRING_DIGITAL
                                 : mode;

    if ( dupeType >= DupeType::EACH_BAND_MODE )
    {
        QString sql_mode = (modeForQuery != BandPlan::MODE_GROUP_STRING_CW &&
                            modeForQuery != BandPlan::MODE_GROUP_STRING_PHONE &&
                            modeForQuery != BandPlan::MODE_GROUP_STRING_DIGITAL)
                            ? "(SELECT modes.dxcc FROM modes WHERE modes.name = :mode LIMIT 1)"
                            : ":mode";
        whereClause << QString("m.dxcc = %1").arg(sql_mode);
    }

    QString queryString("SELECT COUNT(1) "
                        "FROM contacts c "
                        "INNER JOIN modes m ON (m.name = c.mode) "
                        "WHERE %1 ");

    QSqlQuery query;

    if ( ! query.prepare(queryString.arg(whereClause.join(" AND "))))
    {
        qWarning() << "Cannot prepare Select statement" << queryString.arg(whereClause.join(" AND "));
        return false;
    }

    query.bindValue(":callsign", callsign);
    query.bindValue(":date", dupeStartTime);
    query.bindValue(":band", band);
    query.bindValue(":mode", modeForQuery);
    query.bindValue(":contestid", contestID);

    if ( ! query.exec() )
    {
        qWarning() << "Cannot execute Select statement" << query.lastError() << query.lastQuery();
        return false;
    }

    return (query.first()) ? query.value(0).toULongLong() : 0ULL;
}

QString Data::safeQueryString(const QUrlQuery &query)
{
    FCT_IDENTIFICATION;

    QUrlQuery safe;
    const QList<QPair<QString, QString>> &items = query.queryItems(QUrl::FullyDecoded);

    for ( const auto &item : items )
    {
        if ( item.first.compare("password", Qt::CaseInsensitive) == 0
             || item.first.compare("code", Qt::CaseInsensitive) == 0)
            safe.addQueryItem(item.first, "***MASKED***");
        else
            safe.addQueryItem(item.first, item.second);
    }

    return safe.query(QUrl::FullyEncoded);
}

void Data::loadContests()
{
    FCT_IDENTIFICATION;

    QFile file(":/res/data/contests.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray data = file.readAll();

    const QList<QVariant> objectList = QJsonDocument::fromJson(data).toVariant().toList();
    for ( const QVariant &object : objectList )
    {
        const QVariantMap &contestData = object.toMap();
        const QString &id = contestData.value("id").toString();
        const QString &name = contestData.value("name").toString();
        contests.insert(id, name);
    }
}

void Data::loadPropagationModes()
{
    FCT_IDENTIFICATION;

    QFile file(":/res/data/propagation_modes.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray data = file.readAll();

    const QList<QVariant> objects = QJsonDocument::fromJson(data).toVariant().toList();

    for ( const QVariant &object : objects )
    {
        const QVariantMap &propagationModeData = object.toMap();
        const QString &id = propagationModeData.value("id").toString();
        const QString &name = tr(propagationModeData.value("name").toString().toUtf8().constData());
        propagationModes.insert(id, name);
    }
}

void Data::loadLegacyModes()
{
    FCT_IDENTIFICATION;

    // Load conversion table from non-ADIF mode to ADIF mode/submode
    // used in the case of external programs that generate an invalid ADIF modes.
    // Database Mode Table cannot be used because these programs have different mode-strings.

    QFile file(":/res/data/legacy_modes.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray data = file.readAll();

    QVariantMap extModes = QJsonDocument::fromJson(data).toVariant().toMap();
    const QList<QString> keys = extModes.keys();

    for ( const QString &key : keys )
    {
        const QVariantMap &legacyModeData = extModes[key].toMap();
        const QString &mode = legacyModeData.value("mode").toString();
        const QString &submode = legacyModeData.value("submode").toString();
        QPair<QString, QString> modes = QPair<QString, QString>(mode, submode);
        legacyModes.insert(key, modes);
    }
}

void Data::loadDxccFlags()
{
    FCT_IDENTIFICATION;

    QFile file(":/res/data/dxcc.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray data = file.readAll();

    const QList<QVariant> &objects = QJsonDocument::fromJson(data).toVariant().toList();

    for ( const QVariant &object : objects )
    {
        const QVariantMap &dxccData = object.toMap();
        int id = dxccData.value("id").toInt();
        dxccEntityStaticInfo.insert(id, dxccData);
    }
}

void Data::loadSatModes()
{
    FCT_IDENTIFICATION;

    QFile file(":/res/data/sat_modes.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray data = file.readAll();

    const QList<QVariant> &objects = QJsonDocument::fromJson(data).toVariant().toList();
    for ( const QVariant &object : objects )
    {
        const QVariantMap &satModesData = object.toMap();
        const QString &id = satModesData.value("id").toString();
        const QString &name = satModesData.value("name").toString();
        satModes.insert(id, name);
    }
}

void Data::loadIOTA()
{
    FCT_IDENTIFICATION;

    QSqlQuery query("SELECT iotaid, islandname FROM iota");

    while ( query.next() )
    {
        const QString &iotaID = query.value(0).toString();
        const QString &islandName = query.value(1).toString();
        iotaRef.insert(iotaID, islandName);
    }
}

void Data::loadSOTA()
{
    FCT_IDENTIFICATION;

    QSqlQuery query("SELECT summit_code FROM sota_summits");

    while ( query.next() )
    {
        const QString &summitCode = query.value(0).toString();
        sotaRefID.insert(summitCode, QString());
    }
}

void Data::loadWWFF()
{
    QSqlQuery query("SELECT reference FROM wwff_directory");

    while ( query.next() )
    {
        const QString &reference = query.value(0).toString();
        wwffRefID.insert(reference, QString());
    }
}

void Data::loadPOTA()
{
    FCT_IDENTIFICATION;

    QSqlQuery query("SELECT reference FROM pota_directory");

    while ( query.next() )
    {
        const QString &reference = query.value(0).toString();
        potaRefID.insert(reference, QString());
    }
}

QCompleter* Data::createCountyCompleter(int dxcc, QObject *parent)
{
    FCT_IDENTIFICATION;

    QSqlQuery query;

    if ( dxcc != 0 )
    {
        query.prepare("SELECT code FROM adif_enum_secondary_subdivision "
                      "WHERE dxcc = :dxcc ORDER BY code");
        query.bindValue(":dxcc", dxcc);
    }
    else
        query.prepare("SELECT code FROM adif_enum_secondary_subdivision ORDER BY code");

    if ( !query.exec() )
    {
        qCWarning(runtime) << query.lastError().text();
        return nullptr;
    }

    QStringList list;
    while ( query.next() )
        list << query.value(0).toString();

    if ( list.isEmpty() )
        return nullptr;

    QCompleter *completer = new QCompleter(list, parent);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchStartsWith);
    completer->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    return completer;
}

void Data::loadTZ()
{
    FCT_IDENTIFICATION;

    QFile file (":/res/data/timezone21.bin");
    file.open(QIODevice::ReadOnly);
    uchar *tzMap = file.map(0, file.size());

    if ( tzMap )
    {
        zd = ZDOpenDatabaseFromMemory(tzMap, file.size());
        if ( !zd )
        {
            qWarning() << "Cannot open TZ Database";
        }
    }
    else
    {
        qWarning() << "Cannot map TZ File to memory";
    }

}

DxccEntity Data::lookupDxcc(const QString &callsign)
{
    FCT_IDENTIFICATION;
#if 0
    //qInfo() << "Start AD1C";
    DxccEntity ad1cDXCCData = lookupDxccAD1C(callsign);
    //qInfo() << "Finished AD1C";
    DxccEntity clublogDXCCData = lookupDxccClublog(callsign);
    //qInfo() << "Finished Clublog";
    if ( ad1cDXCCData.dxcc != clublogDXCCData.dxcc
         || ad1cDXCCData.cqz != clublogDXCCData.cqz
         || ad1cDXCCData.ituz != clublogDXCCData.ituz)
    {
        qInfo(runtime) << "DIFF for call " << callsign
                         << "AD1C:" << ad1cDXCCData.dxcc << ad1cDXCCData.cqz << ad1cDXCCData.ituz
                         << "Clublog:" << clublogDXCCData.dxcc << clublogDXCCData.cqz << clublogDXCCData.ituz;
    }
    //qInfo() << "AD1C:" << ad1cDXCCData.dxcc << ad1cDXCCData.cqz << ad1cDXCCData.ituz
      //         << "Clublog:" << clublogDXCCData.dxcc << clublogDXCCData.cqz << clublogDXCCData.ituz;
    return ad1cDXCCData;
#else
    // LF: The AD1C method seems more accurate for current data regarding ITU and CQZ.
    // The Clublog method is good for determining historical data and CQZ, but unfortunately,
    // the ITU zone is not reliable in this method because it’s not listed and has to be
    // calculated. Therefore, I decided to use the AD1C method as the general approach and only
    // use Clublog in exceptional cases. Once Clublog starts distributing ITUZ, we should
    // completely switch to the Clublog method.
    // I also believes that the AD1C method is more accurate in determining special exceptions
    // for American callsigns.
    return lookupDxccAD1C(callsign);
#endif
}

DxccEntity Data::lookupDxccAD1C(const QString &callsign)
{
    FCT_IDENTIFICATION;
    static QCache<QString, DxccEntity> localCache(1000);

    qCDebug(function_parameters) << callsign;

    if ( callsign.isEmpty())
        return  DxccEntity();

    DxccEntity dxccRet;
    DxccEntity *dxccCached = localCache.object(callsign);

    if ( dxccCached )
    {
        dxccRet = *dxccCached;
    }
    else
    {
        if ( ! isDXCCQueryValid )
        {
            qWarning() << "Cannot prepare Select statement";
            return DxccEntity();
        }

        QString lookupPrefix = callsign; // use the callsign with optional prefix as default to find the dxcc
        const Callsign parsedCallsign(callsign); // use Callsign to split the callsign into its parts

        if ( parsedCallsign.isValid() )
        {
            QString suffix = parsedCallsign.getSuffix();
            if ( suffix.length() == 1 ) // some countries add single numbers as suffix to designate a call area, e.g. /4
            {
                bool isNumber = false;
                (void)suffix.toInt(&isNumber);
                if ( isNumber )
                {
                    lookupPrefix = parsedCallsign.getBasePrefix() + suffix; // use the call prefix and the number from the suffix to find the dxcc
                }
            }
            else if ( suffix.length() > 1
                      && !parsedCallsign.secondarySpecialSuffixes.contains(suffix) ) // if there is more than one character and it is not one of the special suffixes, we definitely have a call prefix as suffix
            {
                lookupPrefix = suffix;
            }
        }

        queryDXCC.bindValue(":callsign", lookupPrefix);

        if ( ! queryDXCC.exec() )
        {
            qWarning() << "Cannot execute Select statement" << queryDXCC.lastError() << queryDXCC.lastQuery();
            return DxccEntity();
        }

        if ( queryDXCC.next() )
        {
            dxccRet.dxcc = queryDXCC.value(0).toInt();
            dxccRet.country = queryDXCC.value(1).toString();
            dxccRet.prefix = queryDXCC.value(2).toString();
            dxccRet.cont = queryDXCC.value(3).toString();
            dxccRet.cqz = queryDXCC.value(4).toInt();
            dxccRet.ituz = queryDXCC.value(5).toInt();
            dxccRet.latlon[0] = queryDXCC.value(6).toDouble();
            dxccRet.latlon[1] = queryDXCC.value(7).toDouble();
            dxccRet.tz = queryDXCC.value(8).toFloat();
            bool isExactMatch = queryDXCC.value(9).toBool();
            dxccRet.flag = dxccFlag(dxccRet.dxcc);

            if ( !isExactMatch )
            {
                // find the exceptions to the exceptions
                if (  dxccRet.prefix == "KG4" && parsedCallsign.getBase().size() != 5 )
                {
                    //only KG4AA - KG4ZZ are US Navy in Guantanamo Bay. Other KG4s are USA
                    dxccRet = lookupDxccID(291); // USA

                    //do not overwrite the original prefix
                    dxccRet.prefix = "KG4";
                }
            }

            dxccCached = new DxccEntity;

            if ( dxccCached )
            {
                *dxccCached = dxccRet;
                localCache.insert(callsign, dxccCached);
            }
        }
        else
        {
            dxccRet.dxcc = 0;
            dxccRet.ituz = 0;
            dxccRet.cqz = 0;
            dxccRet.tz = 0;
        }
    }

    return dxccRet;
}

DxccEntity Data::lookupDxccIDAD1C(const int dxccID)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << dxccID;

    if ( !isDXCCIDAD1CQueryValid )
    {
        qWarning() << "Cannot prepare Select statement";
        return DxccEntity();
    }

    queryDXCCIDAD1C.bindValue(":dxccid", dxccID);
    if ( ! queryDXCCIDAD1C.exec() )
    {
        qWarning() << "Cannot execte Select statement" << queryDXCCIDAD1C.lastError();
        return DxccEntity();
    }

    DxccEntity dxccRet;

    if ( queryDXCCIDAD1C.next() )
    {
        dxccRet.dxcc = queryDXCCIDAD1C.value(0).toInt();
        dxccRet.country = queryDXCCIDAD1C.value(1).toString();
        dxccRet.prefix = queryDXCCIDAD1C.value(2).toString();
        dxccRet.cont = queryDXCCIDAD1C.value(3).toString();
        dxccRet.cqz = queryDXCCIDAD1C.value(4).toInt();
        dxccRet.ituz = queryDXCCIDAD1C.value(5).toInt();
        dxccRet.latlon[0] = queryDXCCIDAD1C.value(6).toDouble();
        dxccRet.latlon[1] = queryDXCCIDAD1C.value(7).toDouble();
        dxccRet.tz = queryDXCCIDAD1C.value(8).toFloat();
        dxccRet.flag = dxccFlag(dxccRet.dxcc);
    }
    else
    {
        dxccRet.dxcc = 0;
        dxccRet.ituz = 0;
        dxccRet.cqz = 0;
        dxccRet.tz = 0;
    }
    return dxccRet;
}

DxccEntity Data::lookupDxccIDClublog(const int dxccID)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << dxccID;

    if ( !isDXCCIDClublogQueryValid )
    {
        qWarning() << "Cannot prepare Select statement";
        return DxccEntity();
    }

    queryDXCCIDClublog.bindValue(":dxccid", dxccID);
    if ( ! queryDXCCIDClublog.exec() )
    {
        qWarning() << "Cannot execte Select statement" << queryDXCCIDClublog.lastError();
        return DxccEntity();
    }

    DxccEntity dxccRet;

    if ( queryDXCCIDClublog.next() )
    {
        dxccRet.dxcc = queryDXCCIDClublog.value(0).toInt();
        dxccRet.country = queryDXCCIDClublog.value(1).toString();
        dxccRet.prefix = queryDXCCIDClublog.value(2).toString();
        dxccRet.cont = queryDXCCIDClublog.value(3).toString();
        dxccRet.cqz = queryDXCCIDClublog.value(4).toInt();
        dxccRet.ituz = queryDXCCIDClublog.value(5).toInt();
        dxccRet.latlon[0] = queryDXCCIDClublog.value(6).toDouble();
        dxccRet.latlon[1] = queryDXCCIDClublog.value(7).toDouble();
        dxccRet.tz = queryDXCCIDClublog.value(8).toFloat();
        dxccRet.flag = dxccFlag(dxccRet.dxcc);
    }
    else
    {
        dxccRet.dxcc = 0;
        dxccRet.ituz = 0;
        dxccRet.cqz = 0;
        dxccRet.tz = 0;
    }
    return dxccRet;
}

DxccEntity Data::lookupDxccID(const int dxccID)
{
    FCT_IDENTIFICATION;

    // LF: The AD1C method seems more accurate for current data regarding ITU and CQZ.
    // The Clublog method is good for determining historical data and CQZ, but unfortunately,
    // the ITU zone is not reliable in this method because it’s not listed and has to be
    // calculated. Therefore, I decided to use the AD1C method as the general approach and only
    // use Clublog in exceptional cases. Once Clublog starts distributing ITUZ, we should
    // completely switch to the Clublog method.
    // I also believes that the AD1C method is more accurate in determining special exceptions
    // for American callsigns.
    return lookupDxccIDAD1C(dxccID);
}

DxccEntity Data::lookupDxccClublog(const QString &callsign, const QDateTime &date)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    if ( callsign.isEmpty()) return  DxccEntity();

    if ( ! isDXCCClublogQueryValid )
    {
        qWarning() << "Cannot prepare Select statement";
        return DxccEntity();
    }

    QString lookupPrefix = callsign; // use the callsign with optional prefix as default to find the dxcc
    const Callsign parsedCallsign(callsign); // use Callsign to split the callsign into its parts

    if ( parsedCallsign.isValid() )
    {
        QString suffix = parsedCallsign.getSuffix();
        if ( suffix.length() == 1 ) // some countries add single numbers as suffix to designate a call area, e.g. /4
        {
            bool isNumber = false;
            (void)suffix.toInt(&isNumber);
            if ( isNumber )
            {
                lookupPrefix = parsedCallsign.getBasePrefix() + suffix; // use the call prefix and the number from the suffix to find the dxcc
            }
        }
        else if ( suffix.length() > 1
                  && !parsedCallsign.secondarySpecialSuffixes.contains(suffix) ) // if there is more than one character and it is not one of the special suffixes, we definitely have a call prefix as suffix
        {
            lookupPrefix = suffix;
        }
    }

    queryDXCCClublog.bindValue(":modifiedcall", lookupPrefix);
    queryDXCCClublog.bindValue(":exactcall", callsign);
    queryDXCCClublog.bindValue(":dxccdate", date);

    if ( ! queryDXCCClublog.exec() )
    {
        qWarning() << "Cannot execute Select statement"
                   << queryDXCCClublog.lastError()
                   << queryDXCCClublog.lastQuery();
        return DxccEntity();
    }

    DxccEntity dxccRet;
    const DxccEntity &ad1cDXCCData = lookupDxccAD1C(callsign);

    if ( queryDXCCClublog.first() )
    {
        dxccRet.dxcc = queryDXCCClublog.value(0).toInt();
        dxccRet.country = queryDXCCClublog.value(1).toString();
        dxccRet.prefix = queryDXCCClublog.value(2).toString();
        dxccRet.cont = queryDXCCClublog.value(3).toString();
        dxccRet.cqz = queryDXCCClublog.value(4).toInt();
        dxccRet.ituz = queryDXCCClublog.value(5).toInt();
        dxccRet.latlon[0] = queryDXCCClublog.value(6).toDouble();
        dxccRet.latlon[1] = queryDXCCClublog.value(7).toDouble();
        dxccRet.tz = queryDXCCClublog.value(8).toFloat();
        bool isExactMatch = queryDXCCClublog.value(9).toBool();
        dxccRet.flag = dxccFlag(dxccRet.dxcc);

        if ( !isExactMatch )
        {
            // find the exceptions to the exceptions
            if (  dxccRet.prefix == "KG4" && parsedCallsign.getBase().size() != 5 )
            {
                //only KG4AA - KG4ZZ are US Navy in Guantanamo Bay. Other KG4s are USA
                dxccRet = lookupDxccID(291); // USA

                //do not overwrite the original prefix
                dxccRet.prefix = "KG4";
            }
        }
    }
    else
    {
        qCDebug(runtime) << "DXCC not found for " << lookupPrefix << "; Using AD1C Info";
        dxccRet = ad1cDXCCData;
    }

    if ( dxccRet.ituz == 0 )
    {
        qCDebug(runtime) << "Zone exception, using AD1C Info Info to find ITUZ for " << callsign;

        if ( ad1cDXCCData.dxcc == dxccRet.dxcc && ad1cDXCCData.cqz == dxccRet.cqz )
        {
            qCDebug(runtime) << "Fixing ITUZ for call" << callsign;
            dxccRet.ituz = ad1cDXCCData.ituz;
        }
        else
        {
            qCDebug(runtime) << "not match" << callsign
                             << ad1cDXCCData.dxcc << dxccRet.dxcc
                             << ad1cDXCCData.cqz << dxccRet.cqz;
        }
    }

    if ( ad1cDXCCData.dxcc == dxccRet.dxcc && ad1cDXCCData.cqz == dxccRet.cqz )
        dxccRet.ituz = ad1cDXCCData.ituz;

    return dxccRet;
}

SOTAEntity Data::lookupSOTA(const QString &SOTACode)
{
    FCT_IDENTIFICATION;

    if ( ! isSOTAQueryValid )
    {
        qWarning() << "Cannot prepare Select statement";
        return SOTAEntity();
    }

    querySOTA.bindValue(":code", SOTACode.toUpper());

    if ( ! querySOTA.exec() )
    {
        qWarning() << "Cannot execte Select statement" << querySOTA.lastError();
        return SOTAEntity();
    }

    SOTAEntity SOTARet;

    if (querySOTA.next())
    {
        SOTARet.summitCode = querySOTA.value(0).toString();
        SOTARet.associationName = querySOTA.value(1).toString();
        SOTARet.regionName = querySOTA.value(2).toString();
        SOTARet.summitName = querySOTA.value(3).toString();
        SOTARet.altm = querySOTA.value(4).toInt();
        SOTARet.altft = querySOTA.value(5).toInt();
        SOTARet.gridref1 = querySOTA.value(6).toDouble();
        SOTARet.gridref2 = querySOTA.value(7).toDouble();
        SOTARet.longitude = querySOTA.value(8).toDouble();
        SOTARet.latitude = querySOTA.value(9).toDouble();
        SOTARet.points = querySOTA.value(10).toInt();
        SOTARet.bonusPoints = querySOTA.value(11).toInt();
        SOTARet.validFrom = querySOTA.value(12).toDate();
        SOTARet.validTo = querySOTA.value(13).toDate();
    }
    else
    {
        SOTARet.altft = 0;
        SOTARet.altm  = 0;
        SOTARet.gridref1 = 0.0;
        SOTARet.gridref2  = 0.0;
        SOTARet.longitude = 0.0;
        SOTARet.latitude  = 0.0;
        SOTARet.points = 0;
        SOTARet.bonusPoints  = 0;
    }

    return SOTARet;
}

POTAEntity Data::lookupPOTA(const QString &POTACode)
{
    FCT_IDENTIFICATION;

    if ( ! isPOTAQueryValid )
    {
        qWarning() << "Cannot prepare Select statement";
        return POTAEntity();
    }

    queryPOTA.bindValue(":code", POTACode.toUpper());

    if ( ! queryPOTA.exec() )
    {
        qWarning() << "Cannot execte Select statement" << queryPOTA.lastError();
        return POTAEntity();
    }

    POTAEntity POTARet;

    if (queryPOTA.next())
    {
        POTARet.reference = queryPOTA.value(0).toString();
        POTARet.name = queryPOTA.value(1).toString();
        POTARet.active = queryPOTA.value(2).toBool();
        POTARet.entityID = queryPOTA.value(3).toInt();
        POTARet.locationDesc = queryPOTA.value(4).toString();
        POTARet.longitude = queryPOTA.value(5).toDouble();
        POTARet.latitude = queryPOTA.value(6).toDouble();
        POTARet.grid = queryPOTA.value(7).toString();
    }
    else
    {
        POTARet.active = false;
        POTARet.entityID = 0;
        POTARet.longitude = 0.0;
        POTARet.latitude  = 0.0;
    }

    return POTARet;
}

WWFFEntity Data::lookupWWFF(const QString &reference)
{
    FCT_IDENTIFICATION;

    if ( ! isWWFFQueryValid )
    {
        qWarning() << "Cannot prepare Select statement";
        return WWFFEntity();
    }

    queryWWFF.bindValue(":reference", reference.toUpper());

    if ( ! queryWWFF.exec() )
    {
        qWarning() << "Cannot execte Select statement" << queryWWFF.lastError();
        return WWFFEntity();
    }

    WWFFEntity WWFFRet;

    if (queryWWFF.next())
    {
        WWFFRet.reference = queryWWFF.value(0).toString();
        WWFFRet.status = queryWWFF.value(1).toString();
        WWFFRet.name = queryWWFF.value(2).toString();
        WWFFRet.program = queryWWFF.value(3).toString();
        WWFFRet.dxcc = queryWWFF.value(4).toString();
        WWFFRet.state = queryWWFF.value(5).toString();
        WWFFRet.county = queryWWFF.value(6).toString();
        WWFFRet.continent = queryWWFF.value(7).toString();
        WWFFRet.iota = queryWWFF.value(8).toString();
        WWFFRet.iaruLocator = queryWWFF.value(9).toString();
        WWFFRet.latitude = queryWWFF.value(10).toDouble();
        WWFFRet.longitude = queryWWFF.value(11).toDouble();
        WWFFRet.iucncat = queryWWFF.value(12).toString();
        WWFFRet.validFrom = queryWWFF.value(13).toDate();
        WWFFRet.validTo = queryWWFF.value(14).toDate();
    }
    else
    {
        WWFFRet.longitude = 0.0;
        WWFFRet.latitude  = 0.0;
    }

    return WWFFRet;
}
