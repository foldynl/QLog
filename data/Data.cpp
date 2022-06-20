#include <QJsonDocument>
#include <QSqlQuery>
#include <QSqlError>
#include <QColor>
#include "Data.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.data");

Data::Data(QObject *parent) :
   QObject(parent),
   zd(nullptr)
{
    FCT_IDENTIFICATION;

    loadContests();
    loadPropagationModes();
    loadLegacyModes();
    loadDxccFlags();
    loadSatModes();
    loadIOTA();
    loadSOTA();
    loadTZ();
}

Data::~Data()
{
    FCT_IDENTIFICATION;

    if ( zd )
    {
        ZDCloseDatabase(zd);
    }
}

Data* Data::instance() {
    FCT_IDENTIFICATION;

    static Data instance;
    return &instance;
}

DxccStatus Data::dxccStatus(int dxcc, const QString &band, const QString &mode) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << dxcc << " " << band << " " << mode;

    QString filter;

    QSettings settings;
    QVariant start = settings.value("dxcc/start");
    if (!start.isNull()) {
        filter = QString("AND start_time >= '%1'").arg(start.toDate().toString("yyyy-MM-dd"));
    }

    QString sql_mode = ":mode ";
    if (mode != Data::MODE_CW && mode != Data::MODE_PHONE && mode != Data::MODE_DIGITAL) {
        sql_mode = "(SELECT modes.dxcc FROM modes WHERE modes.name = :mode LIMIT 1) ";
    }

    QSqlQuery query;

    if ( ! query.prepare("SELECT (SELECT contacts.callsign FROM contacts WHERE dxcc = :dxcc " + filter + " LIMIT 1) as entity,"
                  "(SELECT contacts.callsign FROM contacts WHERE dxcc = :dxcc AND band = :band " + filter + " LIMIT 1) as band,"
                  "(SELECT contacts.callsign FROM contacts INNER JOIN modes ON (modes.name = contacts.mode)"
                  "        WHERE contacts.dxcc = :dxcc AND modes.dxcc = " + sql_mode + filter +
                  "        LIMIT 1) as mode,"
                  "(SELECT contacts.callsign FROM contacts INNER JOIN modes ON (modes.name = contacts.mode)"
                  "        WHERE contacts.dxcc = :dxcc AND modes.dxcc = " + sql_mode + filter +
                  "        AND band = :band LIMIT 1) as slot;") )
    {
        qWarning() << "Cannot prepare Select statement";
        return DxccStatus::UnknownStatus;
    }

    query.bindValue(":dxcc", dxcc);
    query.bindValue(":band", band);
    query.bindValue(":mode", mode);

    if ( ! query.exec() )
    {
        qWarning() << "Cannot execute Select statement" << query.lastError();
        return DxccStatus::UnknownStatus;
    }

    if (query.next()) {
        if (query.value(0).isNull()) {
            return DxccStatus::NewEntity;
        }
        if (query.value(1).isNull()) {
            if (query.value(2).isNull()) {
                return DxccStatus::NewBandMode;
            }
            else {
                return DxccStatus::NewBand;
            }
        }
        if (query.value(2).isNull()) {
            return DxccStatus::NewMode;
        }
        if (query.value(3).isNull()) {
            return DxccStatus::NewSlot;
        }
        else {
            return DxccStatus::Worked;
        }
    }
    else {
        return DxccStatus::UnknownStatus;
    }
}

Band Data::band(double freq) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << freq;

    QSqlQuery query;
    if ( ! query.prepare("SELECT name, start_freq, end_freq FROM bands WHERE :freq BETWEEN start_freq AND end_freq") )
    {
        qWarning() << "Cannot prepare Select statement";
        return Band();
    }
    query.bindValue(0, freq);

    if ( ! query.exec() )
    {
        qWarning() << "Cannot execute select statement" << query.lastError();
        return Band();
    }

    if (query.next()) {
        Band band;
        band.name = query.value(0).toString();
        band.start = query.value(1).toDouble();
        band.end = query.value(2).toDouble();
        return band;
    }
    else {
        return Band();
    }
}

QList<Band> Data::enabledBandsList()
{
    FCT_IDENTIFICATION;

    QSqlQuery query;
    QList<Band> ret;

    if ( ! query.prepare("SELECT name, start_freq, end_freq FROM bands WHERE enabled = 1 ORDER BY start_freq") )
    {
        qWarning() << "Cannot prepare Select statement";
        return ret;
    }

    if ( ! query.exec() )
    {
        qWarning() << "Cannot execute select statement" << query.lastError();
        return ret;
    }

    while ( query.next() )
    {
        Band band;
        band.name = query.value(0).toString();
        band.start = query.value(1).toDouble();
        band.end = query.value(2).toDouble();
        ret << band;
    }

    return ret;
}

QString Data::freqToBand(double freq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << freq;

    if (freq <= 2.0 && freq >= 1.8) return "160m";
    else if (freq <= 3.8 && freq >= 3.5) return "80m";
    else if (freq <= 7.5 && freq >= 7.0) return "40m";
    else if (freq <= 10.150 && freq >= 10.1) return"30m";
    else if (freq <= 14.350 && freq >= 14.0) return "20m";
    else if (freq <= 18.168 && freq >= 18.068) return "17m";
    else if (freq <= 21.450 && freq >= 21.000) return "15m";
    else if (freq <= 24.990 && freq >= 24.890) return "12m";
    else if (freq <= 29.700 && freq >= 28.000) return "10m";
    else if (freq <= 52.0 && freq >= 50.0) return "6m";
    else if (freq <= 71.0 && freq >= 70.0) return "4m";
    else if (freq <= 148.0 && freq >= 144.0) return "2m";
    else if (freq <= 440.0 && freq >= 430.0) return "70cm";
    else if (freq <= 928.0 && freq >= 902.0) return "33cm";
    else if (freq <= 1300.0 && freq >= 1240.0) return "23cm";
    else if (freq <= 2450.0 && freq >= 2300.0) return "13cm";
    else if (freq <= 3500.0 && freq >= 3300.0) return "9cm";
    else if (freq <= 5925.0 && freq >= 5650.0) return "6cm";
    else if (freq <= 10500.0 && freq >= 10000.0) return "3cm";
    else if (freq <= 24250.0 && freq >= 24000.0) return "1.25cm";
    else if (freq <= 47200.0 && freq >= 47000.0) return "6mm";
    else if (freq <= 81000.0 && freq >= 75500.0) return "4mm";
    else if (freq <= 120020.0 && freq >= 119980.0) return "2.5mm";
    else if (freq <= 149000.0 && freq >= 142000.0) return "2mm";
    else if (freq <= 250000.0 && freq >= 241000.0) return "1mm";
    else return QString();
}

QString Data::freqToMode(double freq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << freq;

    // 2200m
    if (freq >= 0.1357 && freq <= 0.1378) return Data::MODE_CW;

    // 630m
    else if (freq >= 0.472 && freq <= 0.475) return Data::MODE_CW;
    else if (freq <= 0.479) return Data::MODE_DIGITAL;

    // 160m
    else if (freq >= 1.800 && freq < 1.838) return Data::MODE_CW;
    else if (freq >= 1.838 && freq < 1.840) return Data::MODE_DIGITAL;
    else if (freq == 1.840) return Data::MODE_FT8;
    else if (freq > 1.840 && freq < 2.000) return Data::MODE_LSB;

    // 80m
    else if (freq > 3.500 && freq < 3.573) return Data::MODE_CW;
    else if (freq == 3.573) return Data::MODE_FT8;
    else if (freq > 3.573 && freq < 3.580) return Data::MODE_CW;
    else if (freq >= 3.580 && freq < 3.600) return Data::MODE_DIGITAL;
    else if (freq >= 3.600 && freq < 4.000) return Data::MODE_LSB;

    // 60m
    else if (freq >= 5.3515 && freq <= 5.354) return Data::MODE_CW;
    else if (freq <= 5.500) return Data::MODE_LSB;

    // 40m
    else if (freq >= 7.000 && freq < 7.040) return Data::MODE_CW;
    else if (freq >= 7.040 && freq < 7.050) return Data::MODE_DIGITAL;
    else if (freq >= 7.050 && freq < 7.074) return Data::MODE_LSB;
    else if (freq == 7.074) return Data::MODE_FT8;
    else if (freq > 7.074 && freq < 7.300) return Data::MODE_LSB;

    // 30m
    else if (freq >= 10.100 && freq < 10.136) return Data::MODE_CW;
    else if (freq == 10.136 ) return Data::MODE_FT8;
    else if (freq > 10.136 && freq < 10.140) return Data::MODE_CW;
    else if (freq >= 10.140 && freq < 10.150) return Data::MODE_DIGITAL;

    // 20m
    else if (freq >= 14.000 && freq < 14.070) return Data::MODE_CW;
    else if (freq >= 14.070 && freq < 14.074) return Data::MODE_DIGITAL;
    else if (freq == 14.074) return Data::MODE_FT8;
    else if (freq > 14.074 && freq <= 14.099) return Data::MODE_DIGITAL;
    else if (freq > 14.099 && freq < 14.350) return Data::MODE_USB;

    // 17m
    else if (freq >= 18.068 && freq < 18.095) return Data::MODE_CW;
    else if (freq >= 18.095 && freq < 18.100) return Data::MODE_DIGITAL;
    else if (freq == 18.100 ) return Data::MODE_FT8;
    else if (freq > 18.100 && freq < 18.110) return Data::MODE_DIGITAL;
    else if (freq >= 18.110 && freq < 18.268) return Data::MODE_USB;

    // 15m
    else if (freq >= 21.000 && freq < 21.070) return Data::MODE_CW;
    else if (freq >= 21.070 && freq < 21.074) return Data::MODE_LSB;
    else if (freq == 21.074) return Data::MODE_FT8;
    else if (freq > 21.074 && freq < 21.150) return Data::MODE_DIGITAL;
    else if (freq >= 21.150 && freq < 21.450) return Data::MODE_USB;

    // 12m
    else if (freq >= 24.890 && freq < 24.915) return Data::MODE_CW;
    else if (freq == 24.915 ) return Data::MODE_FT8;
    else if (freq > 24.915 && freq < 24.930) return Data::MODE_DIGITAL;
    else if (freq >= 24.930 && freq < 24.990) return Data::MODE_USB;

    // 10m
    else if (freq >= 28.000 && freq < 28.074) return Data::MODE_CW;
    else if (freq == 28.074) return Data::MODE_FT8;
    else if (freq > 28.074 && freq < 28.190) return Data::MODE_DIGITAL;
    else if (freq >= 28.190 && freq < 29.700) return Data::MODE_USB;

    // 6m
    else if (freq >= 50.000 && freq < 50.100) return Data::MODE_CW;
    else if (freq > 50.100 && freq < 50.313) return Data::MODE_USB;
    else if (freq == 50.313 ) return Data::MODE_FT8;
    else if (freq > 50.313 && freq < 54.000) return Data::MODE_USB;

    // 4m
    else if (freq >=70.000 && freq < 70.100) return Data::MODE_CW;
    else if (freq >=70.100 && freq < 70.250) return Data::MODE_USB;
    else if (freq >=70.2500 && freq < 70.500) return Data::MODE_USB;

    // 2m
    else if (freq >= 144.000 && freq < 144.150) return Data::MODE_CW;
    else if (freq >= 144.150 && freq < 144.174) return Data::MODE_USB;
    else if (freq >= 144.174 && freq <= 144.175) return Data::MODE_FT8;
    else if (freq > 144.175 && freq < 148.000) return Data::MODE_USB;

    else return QString();
}

QColor Data::statusToColor(const DxccStatus &status, const QColor &defaultColor) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << status;

    switch (status) {
        case DxccStatus::NewEntity:
            return QColor(255, 58, 9);
        case DxccStatus::NewBand:
        case DxccStatus::NewMode:
        case DxccStatus::NewBandMode:
            return QColor(76, 200, 80);
        case DxccStatus::NewSlot:
            return QColor(30, 180, 230);
        default:
            return defaultColor;
    }
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
        case DxccStatus::Worked:
            return tr("Worked");
        default:
            return QString("Unknown");
    }
}

QRegularExpression Data::callsignRegEx()
{
    FCT_IDENTIFICATION;

    return QRegularExpression(callsignRegExString(), QRegularExpression::CaseInsensitiveOption);
}

QString Data::callsignRegExString()
{
    FCT_IDENTIFICATION;

    return QString("^([A-Z0-9]+[\\/])?([A-Z][0-9]|[A-Z]{1,2}|[0-9][A-Z])([0-9]|[0-9]+)([A-Z]+)([\\/][A-Z0-9]+)?");
}

QString Data::removeAccents(const QString &input)
{
    FCT_IDENTIFICATION;
    /* http://archives.miloush.net/michkap/archive/2007/05/14/2629747.html */
    /* https://www.medo64.com/2020/10/stripping-diacritics-in-qt/ */
    /* More about normalization https://unicode.org/reports/tr15/ */

    QString formD = input.normalized(QString::NormalizationForm_D);

    QString filtered;
    for (int i = 0; i < formD.length(); i++)
    {
        if (formD.at(i).category() != QChar::Mark_NonSpacing)
        {
            filtered.append(formD.at(i));
        }
    }

    QString ret = filtered.normalized(QString::NormalizationForm_C).toLatin1().replace('?',"");

    /* If stripped string is empty then QString to store NULL value do DB */
    if ( ret.length() == 0 )
    {
        ret = QString();
    }
    return ret;

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

QColor Data::statusToInverseColor(const DxccStatus &status, const QColor &defaultColor) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << status;

    switch (status) {
        case DxccStatus::NewEntity:
            return QColor(Qt::white);
        case DxccStatus::NewBand:
        case DxccStatus::NewMode:
        case DxccStatus::NewBandMode:
            return QColor(Qt::white);
        case DxccStatus::NewSlot:
            return QColor(Qt::black);
        default:
            return defaultColor;
    }
}

QPair<QString, QString> Data::legacyMode(const QString &mode) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode;
    return legacyModes.value(mode);
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

void Data::loadContests() {
    FCT_IDENTIFICATION;

    QFile file(":/res/data/contests.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray data = file.readAll();

    auto objectList = QJsonDocument::fromJson(data).toVariant().toList();
    for (auto &object : qAsConst(objectList))
    {
        QVariantMap contestData = object.toMap();

        QString id = contestData.value("id").toString();
        QString name = contestData.value("name").toString();

        contests.insert(id, name);
    }
}

void Data::loadPropagationModes() {
    FCT_IDENTIFICATION;

    QFile file(":/res/data/propagation_modes.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray data = file.readAll();

    auto objects = QJsonDocument::fromJson(data).toVariant().toList();
    for (auto &object : qAsConst(objects))
    {
        QVariantMap propagationModeData = object.toMap();

        QString id = propagationModeData.value("id").toString();
        QString name = propagationModeData.value("name").toString();

        propagationModes.insert(id, name);
    }
}

void Data::loadLegacyModes() {
    FCT_IDENTIFICATION;

    QFile file(":/res/data/legacy_modes.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray data = file.readAll();

    QVariantMap modes = QJsonDocument::fromJson(data).toVariant().toMap();
    auto keys = modes.keys();

    for (auto &key : qAsConst(keys))
    {
        QVariantMap legacyModeData = modes[key].toMap();

        QString mode = legacyModeData.value("mode").toString();
        QString submode = legacyModeData.value("submode").toString();
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

    auto objects = QJsonDocument::fromJson(data).toVariant().toList();
    for (auto &object : qAsConst(objects))
    {
        QVariantMap dxccData = object.toMap();

        int id = dxccData.value("id").toInt();
        QString flag = dxccData.value("flag").toString();

        flags.insert(id, flag);
    }
}

void Data::loadSatModes()
{
    FCT_IDENTIFICATION;

    QFile file(":/res/data/sat_modes.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray data = file.readAll();

    auto objects = QJsonDocument::fromJson(data).toVariant().toList();
    for (auto &object : qAsConst(objects))
    {
        QVariantMap satModesData = object.toMap();

        QString id = satModesData.value("id").toString();
        QString name = satModesData.value("name").toString();

        satModes.insert(id, name);
    }
}

void Data::loadIOTA()
{
    FCT_IDENTIFICATION;

    QFile file(":/res/data/iota.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray data = file.readAll();

    auto objects = QJsonDocument::fromJson(data).toVariant().toList();
    for (auto &object : qAsConst(objects))
    {
        QVariantMap iotaData = object.toMap();

        QString id = iotaData.value("id").toString();
        QString name = iotaData.value("name").toString();

        iotaRef.insert(id, name);
    }
}

void Data::loadSOTA()
{
    FCT_IDENTIFICATION;

    QFile file(":/res/data/sota.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray data = file.readAll();

    auto objects = QJsonDocument::fromJson(data).toVariant().toList();
    for (auto &object : qAsConst(objects))
    {
        QVariantMap sotaData = object.toMap();

        QString id = sotaData.value("id").toString();
        QString name = ""; // later - use UTF8 string
        sotaRef.insert(id, name);
    }
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
    static QCache<QString, DxccEntity> localCache(1000);

    qCDebug(function_parameters) << callsign;

    DxccEntity dxccRet;
    DxccEntity * dxccCached = localCache.object(callsign);

    if ( dxccCached )
    {
        dxccRet.dxcc = dxccCached->dxcc;
        dxccRet.country = dxccCached->country;
        dxccRet.prefix = dxccCached->prefix;
        dxccRet.cont = dxccCached->cont;
        dxccRet.cqz = dxccCached->cqz;
        dxccRet.ituz = dxccCached->ituz;
        dxccRet.latlon[0] = dxccCached->latlon[0];
        dxccRet.latlon[1] = dxccCached->latlon[1];
        dxccRet.tz = dxccCached->tz;
        dxccRet.flag = dxccCached->flag;
    }
    else
    {
        QSqlQuery query;
        if ( ! query.prepare(
                 "SELECT\n"
                 "    dxcc_entities.id,\n"
                 "    dxcc_entities.name,\n"
                 "    dxcc_entities.prefix,\n"
                 "    dxcc_entities.cont,\n"
                 "    CASE\n"
                 "        WHEN (dxcc_prefixes.cqz != 0)\n"
                 "        THEN dxcc_prefixes.cqz\n"
                 "        ELSE dxcc_entities.cqz\n"
                 "    END AS cqz,\n"
                 "    CASE\n"
                 "        WHEN (dxcc_prefixes.ituz != 0)\n"
                 "        THEN dxcc_prefixes.ituz\n"
                 "        ELSE dxcc_entities.ituz\n"
                 "    END AS ituz\n,"
                 "    dxcc_entities.lat,\n"
                 "    dxcc_entities.lon,\n"
                 "    dxcc_entities.tz\n"
                 "FROM dxcc_prefixes\n"
                 "INNER JOIN dxcc_entities ON (dxcc_prefixes.dxcc = dxcc_entities.id)\n"
                 "WHERE (dxcc_prefixes.prefix = :callsign and dxcc_prefixes.exact = true)\n"
                 "    OR (dxcc_prefixes.exact = false and :callsign LIKE dxcc_prefixes.prefix || '%')\n"
                 "ORDER BY dxcc_prefixes.prefix\n"
                 "DESC LIMIT 1\n"
                 ) )
        {
            qWarning() << "Cannot prepare Select statement";
            return DxccEntity();
        }

        query.bindValue(":callsign", callsign);

        if ( ! query.exec() )
        {
            qWarning() << "Cannot execte Select statement" << query.lastError();
            return DxccEntity();
        }
        if (query.next())
        {
            dxccRet.dxcc = query.value(0).toInt();
            dxccRet.country = query.value(1).toString();
            dxccRet.prefix = query.value(2).toString();
            dxccRet.cont = query.value(3).toString();
            dxccRet.cqz = query.value(4).toInt();
            dxccRet.ituz = query.value(5).toInt();
            dxccRet.latlon[0] = query.value(6).toDouble();
            dxccRet.latlon[1] = query.value(7).toDouble();
            dxccRet.tz = query.value(8).toFloat();
            dxccRet.flag = flags.value(dxccRet.dxcc);

            dxccCached = new DxccEntity;

            if ( dxccCached )
            {

                dxccCached->dxcc = dxccRet.dxcc;
                dxccCached->country = dxccRet.country;
                dxccCached->prefix = dxccRet.prefix;
                dxccCached->cont = dxccRet.cont;
                dxccCached->cqz = dxccRet.cqz;
                dxccCached->ituz = dxccRet.ituz;
                dxccCached->latlon[0] = dxccRet.latlon[0];
                dxccCached->latlon[1] = dxccRet.latlon[1];
                dxccCached->tz = dxccRet.tz;
                dxccCached->flag = dxccRet.flag;

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

QString Data::dxccFlag(int dxcc) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << dxcc;
    return flags.value(dxcc);
}

const QString Data::MODE_CW = "CW";
const QString Data::MODE_DIGITAL = "DIGITAL";
const QString Data::MODE_FT8 = "FT8";
const QString Data::MODE_LSB = "PHONE"; // use just generic label
const QString Data::MODE_USB = "PHONE"; // use just generic label
const QString Data::MODE_PHONE = "PHONE";
