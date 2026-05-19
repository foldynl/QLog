#include "CabrilloFormat.h"

#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDateTime>

#include "core/debug.h"
#include "data/BandPlan.h"

MODULE_IDENTIFICATION("qlog.logformat.cabrilloformat");

const QString CabrilloFormat::FMT_NONE;
const QString CabrilloFormat::FMT_FREQ_KHZ = "freq_khz";
const QString CabrilloFormat::FMT_TIME_HHMM = "time_hhmm";
const QString CabrilloFormat::FMT_DATE_YYYY_MM_DD = "date_yyyy_mm_dd";
const QString CabrilloFormat::FMT_RST_SHORT = "rst_short";
const QString CabrilloFormat::FMT_UPPER = "upper";
const QString CabrilloFormat::FMT_MODE_CABRILLO = "mode_cabrillo";
const QString CabrilloFormat::FMT_TRANSMITTER_ID = "transmitter_id";
const QString CabrilloFormat::FMT_PADDED_NR = "padded_nr";

const QString CabrilloFormat::BAND_ALL = "ALL";
const QString CabrilloFormat::BAND_160M = "160M";
const QString CabrilloFormat::BAND_80M = "80M";
const QString CabrilloFormat::BAND_40M = "40M";
const QString CabrilloFormat::BAND_20M = "20M";
const QString CabrilloFormat::BAND_15M = "15M";
const QString CabrilloFormat::BAND_10M = "10M";
const QString CabrilloFormat::BAND_6M = "6M";
const QString CabrilloFormat::BAND_4M = "4M";
const QString CabrilloFormat::BAND_2M = "2M";
const QString CabrilloFormat::BAND_222 = "222";
const QString CabrilloFormat::BAND_432 = "432";
const QString CabrilloFormat::BAND_902 = "902";
const QString CabrilloFormat::BAND_1_2G = "1.2G";
const QString CabrilloFormat::BAND_LIGHT = "LIGHT";
const QString CabrilloFormat::BAND_VHF_3_BAND = "VHF-3-BAND";
const QString CabrilloFormat::BAND_VHF_FM_ONLY = "VHF-FM-ONLY";

const QString CabrilloFormat::MODE_CW = "CW";
const QString CabrilloFormat::MODE_SSB = "SSB";
const QString CabrilloFormat::MODE_RTTY = "RTTY";
const QString CabrilloFormat::MODE_FM = "FM";
const QString CabrilloFormat::MODE_DIGI = "DIGI";
const QString CabrilloFormat::MODE_MIXED = "MIXED";

const QString CabrilloFormat::POWER_HIGH = "HIGH";
const QString CabrilloFormat::POWER_LOW = "LOW";
const QString CabrilloFormat::POWER_QRP = "QRP";

const QString CabrilloFormat::OPERATOR_SINGLE = "SINGLE-OP";
const QString CabrilloFormat::OPERATOR_MULTI = "MULTI-OP";
const QString CabrilloFormat::OPERATOR_CHECKLOG = "CHECKLOG";

const QString CabrilloFormat::ASSISTED_NO = "NON-ASSISTED";
const QString CabrilloFormat::ASSISTED_YES = "ASSISTED";

const QString CabrilloFormat::STATION_FIXED = "FIXED";
const QString CabrilloFormat::STATION_MOBILE = "MOBILE";
const QString CabrilloFormat::STATION_PORTABLE = "PORTABLE";
const QString CabrilloFormat::STATION_ROVER = "ROVER";
const QString CabrilloFormat::STATION_ROVER_LIMITED = "ROVER-LIMITED";
const QString CabrilloFormat::STATION_ROVER_UNLIMITED = "ROVER-UNLIMITED";
const QString CabrilloFormat::STATION_EXPEDITION = "EXPEDITION";
const QString CabrilloFormat::STATION_HQ = "HQ";
const QString CabrilloFormat::STATION_SCHOOL = "SCHOOL";
const QString CabrilloFormat::STATION_DISTRIBUTED = "DISTRIBUTED";

const QString CabrilloFormat::TRANSMITTER_ONE = "ONE";
const QString CabrilloFormat::TRANSMITTER_TWO = "TWO";
const QString CabrilloFormat::TRANSMITTER_LIMITED = "LIMITED";
const QString CabrilloFormat::TRANSMITTER_UNLIMITED = "UNLIMITED";
const QString CabrilloFormat::TRANSMITTER_SWL = "SWL";

const QString CabrilloFormat::TIME_6_HOURS = "6-HOURS";
const QString CabrilloFormat::TIME_12_HOURS = "12-HOURS";
const QString CabrilloFormat::TIME_24_HOURS = "24-HOURS";

const QString CabrilloFormat::OVERLAY_CLASSIC = "CLASSIC";
const QString CabrilloFormat::OVERLAY_ROOKIE = "ROOKIE";
const QString CabrilloFormat::OVERLAY_TB_WIRES = "TB-WIRES";
const QString CabrilloFormat::OVERLAY_NOVICE_TECH = "NOVICE-TECH";
const QString CabrilloFormat::OVERLAY_OVER_50 = "OVER-50";

CabrilloFormat::CabrilloFormat(QTextStream &stream) :
    LogFormat(stream),
    templateId(-1),
    transmitterId(0),
    multiOpEnabled(false)
{
    FCT_IDENTIFICATION;
}

void CabrilloFormat::setTemplateId(int id)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << id;
    templateId = id;
}

void CabrilloFormat::setHeaderData(const HeaderData &data)
{
    FCT_IDENTIFICATION;
    headerData = data;
}

void CabrilloFormat::setTransmitterId(int id)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << id;
    transmitterId = id;
}

QMap<QString, QString> CabrilloFormat::buildHeaderFields() const
{
    FCT_IDENTIFICATION;

    QMap<QString, QString> fields;

    fields["CALLSIGN"] = headerData.callsign;

    auto addIfNotEmpty = [&](const QString& key, const QString& value)
    {
        if (!value.isEmpty())
            fields[key] = value;
    };

    addIfNotEmpty("NAME", headerData.operatorName);
    addIfNotEmpty("EMAIL", headerData.email);
    addIfNotEmpty("OPERATORS", headerData.operators);
    addIfNotEmpty("ADDRESS", headerData.address);
    addIfNotEmpty("GRID-LOCATOR", headerData.gridLocator);

    addIfNotEmpty("CATEGORY-OPERATOR", headerData.categoryOperator);
    addIfNotEmpty("CATEGORY-BAND", headerData.categoryBand);
    addIfNotEmpty("CATEGORY-POWER", headerData.categoryPower);
    addIfNotEmpty("CATEGORY-MODE", headerData.categoryMode);
    addIfNotEmpty("CATEGORY-ASSISTED", headerData.categoryAssisted);
    addIfNotEmpty("CATEGORY-STATION", headerData.categoryStation);
    addIfNotEmpty("CATEGORY-TRANSMITTER", headerData.categoryTransmitter);
    addIfNotEmpty("CATEGORY-TIME", headerData.categoryTime);
    addIfNotEmpty("CATEGORY-OVERLAY", headerData.categoryOverlay);

    addIfNotEmpty("LOCATION", headerData.location);
    addIfNotEmpty("CLUB", headerData.club);
    addIfNotEmpty("OFFTIME", headerData.offtime);
    addIfNotEmpty("SOAPBOX", headerData.soapbox);

    return fields;
}

QList<CabrilloFormat::TemplateInfo> CabrilloFormat::templateList()
{
    FCT_IDENTIFICATION;

    QList<TemplateInfo> result;

    QSqlQuery query("SELECT id, name, "
                    "contest_name_for_header, default_category_mode "
                    "FROM cabrillo_templates ORDER BY name");
    while ( query.next() )
    {
        TemplateInfo info;
        info.id = query.value(0).toInt();
        info.name = query.value(1).toString();
        info.contestName = query.value(2).toString();
        info.defaultCategoryMode = query.value(3).toString();
        result.append(info);
    }

    return result;
}

CabrilloFormat::TemplateInfo CabrilloFormat::templateInfo(int templateId)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << templateId;

    TemplateInfo info;
    info.id = -1;

    QSqlQuery query;
    if ( !query.prepare("SELECT id, name, "
                        "contest_name_for_header, default_category_mode "
                        "FROM cabrillo_templates WHERE id = :id") )
    {
        qCWarning(runtime) << query.lastError().text();
        return info;
    }

    query.bindValue(":id", templateId);
    if ( query.exec() && query.next() )
    {
        info.id = query.value(0).toInt();
        info.name = query.value(1).toString();
        info.contestName = query.value(2).toString();
        info.defaultCategoryMode = query.value(3).toString();
    }
    else
    {
        qCWarning(runtime) << "Template not found:" << templateId;
    }

    return info;
}

QList<CabrilloFormat::CategoryItem> CabrilloFormat::bandCategories()
{
    return {
        { BAND_ALL,          QCoreApplication::translate("CabrilloFormat","All Bands") },
        { BAND_160M,         "160m" },
        { BAND_80M,          "80m" },
        { BAND_40M,          "40m" },
        { BAND_20M,          "20m" },
        { BAND_15M,          "15m" },
        { BAND_10M,          "10m" },
        { BAND_6M,           "6m" },
        { BAND_4M,           "4m" },
        { BAND_2M,           QCoreApplication::translate("CabrilloFormat","2m (144 MHz)") },
        { BAND_222,          QCoreApplication::translate("CabrilloFormat","1.25m (222 MHz)") },
        { BAND_432,          QCoreApplication::translate("CabrilloFormat","70cm (432 MHz)") },
        { BAND_902,          QCoreApplication::translate("CabrilloFormat","33cm (902 MHz)") },
        { BAND_1_2G,         QCoreApplication::translate("CabrilloFormat","23cm (1.2 GHz)") },
        { BAND_LIGHT,        QCoreApplication::translate("CabrilloFormat","Light") },
        { BAND_VHF_3_BAND,   QCoreApplication::translate("CabrilloFormat","VHF 3-Band") },
        { BAND_VHF_FM_ONLY,  QCoreApplication::translate("CabrilloFormat","VHF FM Only") },
    };
}

QList<CabrilloFormat::CategoryItem> CabrilloFormat::modeCategories()
{
    return {
        { MODE_CW,     "CW" },
        { MODE_SSB,    "SSB" },
        { MODE_RTTY,   "RTTY" },
        { MODE_FM,     "FM" },
        { MODE_DIGI,   QCoreApplication::translate("CabrilloFormat","Digital") },
        { MODE_MIXED,  QCoreApplication::translate("CabrilloFormat","Mixed") },
    };
}

QList<CabrilloFormat::CategoryItem> CabrilloFormat::powerCategories()
{
    return {
        { POWER_HIGH, QCoreApplication::translate("CabrilloFormat","High") },
        { POWER_LOW,  QCoreApplication::translate("CabrilloFormat","Low") },
        { POWER_QRP,  QCoreApplication::translate("CabrilloFormat","QRP") },
    };
}

QList<CabrilloFormat::CategoryItem> CabrilloFormat::operatorCategories()
{
    return {
        { OPERATOR_SINGLE,   QCoreApplication::translate("CabrilloFormat", "Single Operator") },
        { OPERATOR_MULTI,    QCoreApplication::translate("CabrilloFormat","Multi Operator") },
        { OPERATOR_CHECKLOG, QCoreApplication::translate("CabrilloFormat","Check Log") },
    };
}

QList<CabrilloFormat::CategoryItem> CabrilloFormat::assistedCategories()
{
    return {
        { ASSISTED_NO,  QCoreApplication::translate("CabrilloFormat","Non-Assisted") },
        { ASSISTED_YES, QCoreApplication::translate("CabrilloFormat","Assisted") },
    };
}

QList<CabrilloFormat::CategoryItem> CabrilloFormat::stationCategories()
{
    return {
        { STATION_FIXED,           QCoreApplication::translate("CabrilloFormat","Fixed") },
        { STATION_MOBILE,          QCoreApplication::translate("CabrilloFormat","Mobile") },
        { STATION_PORTABLE,        QCoreApplication::translate("CabrilloFormat","Portable") },
        { STATION_ROVER,           QCoreApplication::translate("CabrilloFormat","Rover") },
        { STATION_ROVER_LIMITED,   QCoreApplication::translate("CabrilloFormat","Rover Limited") },
        { STATION_ROVER_UNLIMITED, QCoreApplication::translate("CabrilloFormat","Rover Unlimited") },
        { STATION_EXPEDITION,      QCoreApplication::translate("CabrilloFormat","Expedition") },
        { STATION_HQ,              QCoreApplication::translate("CabrilloFormat","HQ") },
        { STATION_SCHOOL,          QCoreApplication::translate("CabrilloFormat","School") },
        { STATION_DISTRIBUTED,     QCoreApplication::translate("CabrilloFormat","Distributed") },
    };
}

QList<CabrilloFormat::CategoryItem> CabrilloFormat::transmitterCategories()
{
    return {
        { TRANSMITTER_ONE,       QCoreApplication::translate("CabrilloFormat","One") },
        { TRANSMITTER_TWO,       QCoreApplication::translate("CabrilloFormat","Two") },
        { TRANSMITTER_LIMITED,   QCoreApplication::translate("CabrilloFormat","Limited") },
        { TRANSMITTER_UNLIMITED, QCoreApplication::translate("CabrilloFormat","Unlimited") },
        { TRANSMITTER_SWL,       QCoreApplication::translate("CabrilloFormat","SWL") },
    };
}

QList<CabrilloFormat::CategoryItem> CabrilloFormat::timeCategories()
{
    return {
        { TIME_6_HOURS,  QCoreApplication::translate("CabrilloFormat","6 Hours") },
        { TIME_12_HOURS, QCoreApplication::translate("CabrilloFormat","12 Hours") },
        { TIME_24_HOURS, QCoreApplication::translate("CabrilloFormat","24 Hours") },
    };
}

QList<CabrilloFormat::CategoryItem> CabrilloFormat::overlayCategories()
{
    return {
        { OVERLAY_CLASSIC,     QCoreApplication::translate("CabrilloFormat","Classic") },
        { OVERLAY_ROOKIE,      QCoreApplication::translate("CabrilloFormat","Rookie") },
        { OVERLAY_TB_WIRES,    QCoreApplication::translate("CabrilloFormat","TB Wires") },
        { OVERLAY_NOVICE_TECH, QCoreApplication::translate("CabrilloFormat","Novice/Tech") },
        { OVERLAY_OVER_50,     QCoreApplication::translate("CabrilloFormat","Over 50") },
    };
}

QList<CabrilloFormat::CategoryItem> CabrilloFormat::formatterTypes()
{
    return {
        { FMT_NONE,            QCoreApplication::translate("CabrilloFormat","Text (left-aligned)") },
        { FMT_FREQ_KHZ,        QCoreApplication::translate("CabrilloFormat","Frequency (kHz)") },
        { FMT_TIME_HHMM,       QCoreApplication::translate("CabrilloFormat","Time (HHMM)") },
        { FMT_DATE_YYYY_MM_DD, QCoreApplication::translate("CabrilloFormat","Date (YYYY-MM-DD)") },
        { FMT_RST_SHORT,       QCoreApplication::translate("CabrilloFormat","RST Short (drop last digit)") },
        { FMT_UPPER,           QCoreApplication::translate("CabrilloFormat","Uppercase") },
        { FMT_MODE_CABRILLO,   QCoreApplication::translate("CabrilloFormat","Mode (Cabrillo)") },
        { FMT_PADDED_NR,       QCoreApplication::translate("CabrilloFormat","Zero-Padded Nr.") },
        { FMT_TRANSMITTER_ID,  QCoreApplication::translate("CabrilloFormat","Transmitter ID") },
    };
}

void CabrilloFormat::loadTemplate()
{
    FCT_IDENTIFICATION;

    columns.clear();
    const TemplateInfo info = templateInfo(templateId);
    contestName = info.contestName;
    columns = loadTemplateColumns(templateId);
}

QList<CabrilloFormat::ColumnDef> CabrilloFormat::loadTemplateColumns(int templateId)
{
    FCT_IDENTIFICATION;

    QList<ColumnDef> result;

    QSqlQuery colQuery;
    if ( !colQuery.prepare("SELECT position, db_field, width, formatter, label "
                           "FROM cabrillo_template_columns "
                           "WHERE template_id = :tid ORDER BY position") )
    {
        qCWarning(runtime) << colQuery.lastError().text();
        return result;
    }
    colQuery.bindValue(":tid", templateId);
    if ( !colQuery.exec() )
    {
        qCWarning(runtime) << colQuery.lastError().text();
        return result;
    }

    while ( colQuery.next() )
    {
        ColumnDef col;
        col.position = colQuery.value(0).toInt();
        col.dbField = colQuery.value(1).toString();
        col.width = colQuery.value(2).toInt();
        col.formatter = colQuery.value(3).toString();
        col.label = colQuery.value(4).toString();
        result.append(col);
    }

    return result;
}

QString CabrilloFormat::formatField(const QString &value,
                                    const QString &formatter,
                                    int width)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << value << formatter << width;

    QString result = value;

    if ( !formatter.isEmpty() )
    {
        if ( formatter == FMT_FREQ_KHZ )
        {
            // MHz string -> kHz integer
            bool ok;
            double mhz = value.toDouble(&ok);
            if ( ok )
                result = QString::number(static_cast<int>(mhz * 1000));
        }
        else if ( formatter == FMT_TIME_HHMM )
        {
            QDateTime dt = QDateTime::fromString(value, Qt::ISODate);
            if ( dt.isValid() )
                result = dt.toString("HHmm");
        }
        else if ( formatter == FMT_DATE_YYYY_MM_DD )
        {
            QDateTime dt = QDateTime::fromString(value, Qt::ISODate);
            if ( dt.isValid() )
                result = dt.toString("yyyy-MM-dd");
        }
        else if ( formatter == FMT_RST_SHORT )
        {
            // 599 -> 59 (drop last digit)
            if ( result.length() >= 2 )
                result = result.left(result.length() - 1);
        }
        else if ( formatter == FMT_UPPER )
        {
            result = result.toUpper();
        }
        else if ( formatter == FMT_MODE_CABRILLO )
        {
            // ADIF mode -> Cabrillo mode: CW, PH, FM, RY, DG
            if ( value == "CW" )
                result = "CW";
            else if ( value == "FM" )
                result = "FM";
            else if ( value == "RTTY" )
                result = "RY";
            else
            {
                QString dxcc = BandPlan::modeToDXCCModeGroup(value);
                result = ( dxcc == "PHONE" ) ? "PH" : "DG";
            }
        }
        else if ( formatter == FMT_PADDED_NR )
        {
            bool ok;
            unsigned long nr = value.toLongLong(&ok);
            result = (ok) ? QString::number(nr).rightJustified(3, '0') : "";
        }
    }

    // Pad or truncate to exactly 'width' characters (left-aligned)
    if ( width > 0 )
        result = result.leftJustified(width, ' ', true);

    qCDebug(runtime) << result;

    return result;
}

void CabrilloFormat::writeMultiLineField(const QString &key, const QString &value,
                                         int maxLines)
{
    FCT_IDENTIFICATION;

    const QString prefix = key + ": ";
    const int maxContent = MAX_LINE_LENGTH - prefix.length();

    const QStringList inputLines = value.split('\n');
    int lineCount = 0;

    for ( const QString &inputLine : inputLines )
    {
        if ( maxLines > 0 && lineCount >= maxLines )
            break;

        const QString trimmed = inputLine.trimmed();
        if ( trimmed.isEmpty() )
            continue;

        if ( trimmed.length() <= maxContent )
        {
            stream << prefix << trimmed << '\n';
            ++lineCount;
            continue;
        }

        // Word-wrap long lines
        const QStringList words = trimmed.split(' ',
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                                                 Qt::SkipEmptyParts);
#else
                                                 QString::SkipEmptyParts);
#endif

        QString line;
        line.reserve(maxContent);

        for ( const QString &word : words )
        {
            if ( maxLines > 0 && lineCount >= maxLines )
                break;

            const int extra = line.isEmpty() ? word.length()
                                             : word.length() + 1;

            if ( !line.isEmpty() && line.length() + extra > maxContent )
            {
                stream << prefix << line << '\n';
                ++lineCount;
                line.clear();
            }

            if ( !line.isEmpty() ) line += ' ';
            line += word;
        }

        if ( !line.isEmpty() && (maxLines <= 0 || lineCount < maxLines) )
        {
            stream << prefix << line << '\n';
            ++lineCount;
        }
    }
}

void CabrilloFormat::exportStart()
{
    FCT_IDENTIFICATION;

    loadTemplate();

    const QMap<QString, QString> headerFields = buildHeaderFields();

    stream << "START-OF-LOG: " + FORMAT_VERSION_STRING << "\n";
    stream << "CREATED-BY: " + PROGRAMID_STRING << "\n";

    if ( !contestName.isEmpty() )
        stream << "CONTEST: " << contestName << "\n";

    for ( const QString &key : headerOrder )
    {
        if ( !headerFields.contains(key) )
            continue;

        const QString &val = headerFields.value(key);

        if ( key == "CATEGORY-OPERATOR" && val == OPERATOR_MULTI )
            multiOpEnabled = true;

        if ( key == "ADDRESS" )
            writeMultiLineField(key, val, MAX_ADDRESS_LINES);
        else if ( key == "SOAPBOX" )
            writeMultiLineField(key, val);
        else if ( !val.isEmpty() )
            stream << key << ": " << val << "\n";
    }
}

void CabrilloFormat::exportContact(const QSqlRecord &record,
                                   QMap<QString, QString> *)
{
    FCT_IDENTIFICATION;

    QString line = "QSO:";

    for ( const ColumnDef &col : static_cast<const QList<ColumnDef>&>(columns) )
    {
        if ( col.formatter == FMT_TRANSMITTER_ID )
        {
            line += " " + ((multiOpEnabled) ? QString::number(transmitterId).leftJustified(col.width, ' ', true)
                                            : " ");
            continue;
        }

        int fieldIndex = record.indexOf(col.dbField);
        QString value;
        if ( fieldIndex >= 0 )
            value = record.value(fieldIndex).toString();

        line += " " + formatField(value, col.formatter, col.width);
    }

    qCDebug(runtime) << line;
    stream << line << "\n";
}

void CabrilloFormat::exportEnd()
{
    FCT_IDENTIFICATION;

    stream << "END-OF-LOG:" << "\n";
}
