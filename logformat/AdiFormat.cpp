#include <QSqlRecord>
#include <QDebug>
#include <QSqlField>
#include "data/Data.h"
#include "AdiFormat.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.logformat.adiformat");

#define ALWAYS_PRESENT true

void AdiFormat::exportStart()
{
    FCT_IDENTIFICATION;

    writeField("ADIF_VER", ALWAYS_PRESENT, ADIF_VERSION_STRING);
    writeField("PROGRAMID", ALWAYS_PRESENT, PROGRAMID_STRING);
    writeField("PROGRAMVERSION", ALWAYS_PRESENT, VERSION);
    writeField("CREATED_TIMESTAMP", ALWAYS_PRESENT,
               QDateTime::currentDateTimeUtc().toString("yyyyMMdd hhmmss"));
    stream << "<EOH>\n\n";
}

void AdiFormat::exportContact(const QSqlRecord& record,
                              QMap<QString, QString> *applTags)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<record;

    writeSQLRecord(record, applTags);

    stream << "<eor>\n\n";
}

void AdiFormat::writeField(const QString &name, bool presenceCondition,
                           const QString &value, const QString &type)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<< name
                                << presenceCondition
                                << value
                                << type;

    if (!presenceCondition) return;

    /* ADIF does not support UTF-8 characterset therefore the Accents are remove */
    QString accentless(Data::removeAccents(value));

    qCDebug(runtime) << "Accentless: " << accentless;

    if ( value.isEmpty() || accentless.isEmpty() ) return;

    stream << "<" << name << ":" << accentless.size();

    if (!type.isEmpty()) stream << ":" << type;

    stream << ">" << accentless << '\n';
}

void AdiFormat::writeSQLRecord(const QSqlRecord &record,
                               QMap<QString, QString> *applTags)
{
    FCT_IDENTIFICATION;

    for ( int i = 0; i <= record.count(); i++)
    {
        const QSqlField &tmpField = record.field(i);
        const ExportParams &ExportParams = DB2ADIFExportParams.value(tmpField.name());

        if ( ExportParams.isValid )
            writeField(ExportParams.ADIFName,
                       tmpField.isValid(),
                       formatOuput(ExportParams.formatter, tmpField.value()),
                       ExportParams.outputType );
    }

    const QVariant &startVariant = record.value("start_time");

    if ( startVariant.isValid() )
    {
        const QDateTime &time_start = startVariant.toDateTime().toTimeZone(QTimeZone::utc());
        writeField("qso_date", startVariant.isValid(),
                   formatOuput(OutputFieldFormatter::TODATE, time_start));
        writeField("time_on", startVariant.isValid(),
                   formatOuput(OutputFieldFormatter::TOTIME, time_start));
    }

    const QVariant &endVariant = record.value("end_time");

    if ( endVariant.isValid() )
    {
        const QDateTime &time_end = record.value("end_time").toDateTime().toTimeZone(QTimeZone::utc());
        writeField("qso_date_off", endVariant.isValid(),
                   formatOuput(OutputFieldFormatter::TODATE, time_end));
        writeField("time_off", endVariant.isValid(),
                   formatOuput(OutputFieldFormatter::TOTIME, time_end));
    }

    const QJsonObject &fields = QJsonDocument::fromJson(record.value("fields").toByteArray()).object();

    const QStringList &keys = fields.keys();
    for (const QString &key : keys)
    {
        writeField(key, ALWAYS_PRESENT, fields.value(key).toString());
    }

    /* Add application-specific tags */
    if ( applTags )
    {
       const QStringList& appKeys = applTags->keys();
       for (const QString &appkey : appKeys)
       {
           writeField(appkey, ALWAYS_PRESENT, applTags->value(appkey));
       }
    }
}

void AdiFormat::readField(QString& field, QString& value)
{
    FCT_IDENTIFICATION;

    //qCDebug(function_parameters)<<field<< " " << value;

    char c;

    QString typeString;
    QString lengthString;
    int length = 0;

    while (!stream.atEnd()) {
        switch (state) {
        case START:
            stream >> c;
            if (c == '<') {
                inHeader = false;
                state = KEY;
                field = "";
            }
            else {
                inHeader = true;
                state = FIELD;
            }
            break;

        case FIELD:
            stream >> c;
            if (c == '<') {
                state = KEY;
                field = "";
            }
            break;

        case KEY:
            stream >> c;
            if (c == ':') {
                state = SIZE;
                lengthString = "";
            }
            else if (c == '>') {
                state = FIELD;
                if (inHeader && field.toLower() == "eoh") {
                    inHeader = false;
                }
                else {
                    value = "";
                    return;
                }
            }
            else {
                field.append(c);
            }
            break;

        case SIZE:
            stream >> c;
            if (c == ':') {
                 if (!lengthString.isEmpty()) {
                    length = lengthString.toInt();
                 }
                 state = DATA_TYPE;
                 typeString = "";
            }
            else if (c == '>') {
                if (!lengthString.isEmpty()) {
                    length = lengthString.toInt();
                }

                if (length > 0) {
                    state = VALUE;
                    value = "";
                }
                else {
                    state = FIELD;
                    if (!inHeader) {
                        value = "";
                        return;
                    }

                }
            }
            else {
                lengthString.append(c);
            }
            break;

        case DATA_TYPE:
            stream >> c;
            if (c == '>') {
                if (length > 0) {
                    state = VALUE;
                    value = "";
                }
                else {
                    state = FIELD;
                    if (!inHeader) {
                        value = "";
                        return;
                    }
                }
            }
            else {
                typeString.append(c);
            }
            break;

        case VALUE:
            value = QString(stream.read(length));
            state = FIELD;
            if (!inHeader) {
                return;
            }
            break;
        }
    }
}

void AdiFormat::mapContact2SQLRecord(QMap<QString, QVariant> &contact,
                                     QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    preprocessINTLFields<QMap<QString, QVariant>>(contact);

    /* Set default values if not present */
    if ( defaults )
    {
        const QStringList &keys = defaults->keys();

        for ( const QString &key : keys )
        {
            if ( contact.value(key).toString().isEmpty() )
            {
                contact.insert(key, defaults->value(key));
            }
        }
        // re-evaluate the fields
        preprocessINTLFields<QMap<QString, QVariant>>(contact);
    }

    contactFields2SQLRecord(contact, record);

    /* If we have something unparsed then stored it as JSON to Field column */
    if ( contact.count() > 0 )
    {
        QJsonDocument doc = QJsonDocument::fromVariant(QVariant(contact));
        record.setValue("fields", QString(doc.toJson()));
    }
}

void AdiFormat::contactFields2SQLRecord(QMap<QString, QVariant> &contact, QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    record.setValue("callsign", contact.take("call").toString().toUpper());
    record.setValue("rst_rcvd", contact.take("rst_rcvd"));
    record.setValue("rst_sent", contact.take("rst_sent"));
    record.setValue("gridsquare", contact.take("gridsquare").toString().toUpper());
    record.setValue("cqz", contact.take("cqz"));
    record.setValue("ituz", contact.take("ituz"));
    record.setValue("freq", contact.take("freq"));
    record.setValue("band", contact.take("band").toString().toLower());
    record.setValue("cont", contact.take("cont").toString().toUpper());
    record.setValue("dxcc", contact.take("dxcc"));
    record.setValue("pfx", contact.take("pfx").toString().toUpper());
    record.setValue("state", contact.take("state"));
    record.setValue("cnty", contact.take("cnty"));
    record.setValue("cnty_alt", contact.take("cnty_alt"));
    record.setValue("iota", contact.take("iota").toString().toUpper());
    record.setValue("qsl_rcvd", parseQslRcvd(contact.take("qsl_rcvd").toString()));
    record.setValue("qsl_rdate", parseDate(contact.take("qslrdate").toString()));  //TODO: DIFF MAPPING
    record.setValue("qsl_sent", parseQslSent(contact.take("qsl_sent").toString()));
    record.setValue("qsl_sdate", parseDate(contact.take("qslsdate").toString()));   //TODO: DIFF MAPPING
    record.setValue("lotw_qsl_rcvd", parseQslRcvd(contact.take("lotw_qsl_rcvd").toString()));
    record.setValue("lotw_qslrdate", parseDate(contact.take("lotw_qslrdate").toString()));
    record.setValue("lotw_qsl_sent", parseQslSent(contact.take("lotw_qsl_sent").toString()));
    record.setValue("lotw_qslsdate", parseDate(contact.take("lotw_qslsdate").toString()));
    record.setValue("tx_pwr", contact.take("tx_pwr"));
    record.setValue("address", contact.take("address"));
    record.setValue("address_intl", contact.take("address_intl"));
    record.setValue("age", contact.take("age"));
    record.setValue("altitude", contact.take("altitude"));
    record.setValue("a_index", contact.take("a_index"));
    record.setValue("ant_az", contact.take("ant_az"));
    record.setValue("ant_el", contact.take("ant_el"));
    record.setValue("ant_path", contact.take("ant_path").toString().toUpper());
    record.setValue("arrl_sect", contact.take("arrl_sect"));
    record.setValue("award_submitted",contact.take("award_submitted"));
    record.setValue("award_granted",contact.take("award_granted"));
    record.setValue("band_rx",contact.take("band_rx").toString().toLower());
    record.setValue("check",contact.take("check"));
    record.setValue("class",contact.take("class"));
    record.setValue("clublog_qso_upload_date",parseDate(contact.take("clublog_qso_upload_date").toString()));
    record.setValue("clublog_qso_upload_status",parseUploadStatus(contact.take("clublog_qso_upload_status").toString()));
    record.setValue("contacted_op",contact.take("contacted_op"));
    record.setValue("comment",contact.take("comment"));
    record.setValue("comment_intl",contact.take("comment_intl"));
    record.setValue("contest_id",contact.take("contest_id"));
    record.setValue("country",contact.take("country"));
    record.setValue("country_intl",contact.take("country_intl"));
    record.setValue("credit_submitted",contact.take("credit_submitted"));
    record.setValue("credit_granted",contact.take("credit_granted"));
    record.setValue("darc_dok",contact.take("darc_dok").toString().toUpper());
    record.setValue("dcl_qslrdate",parseDate(contact.take("dcl_qslrdate").toString()));
    record.setValue("dcl_qslsdate",parseDate(contact.take("dcl_qslsdate").toString()));
    record.setValue("dcl_qsl_rcvd",parseQslRcvd(contact.take("dcl_qsl_rcvd").toString()));
    record.setValue("dcl_qsl_sent",parseQslSent(contact.take("dcl_qsl_sent").toString()));
    record.setValue("distance",contact.take("distance"));
    record.setValue("email",contact.take("email"));
    record.setValue("eq_call",contact.take("eq_call"));
    record.setValue("eqsl_ag",parseEqslAg(contact.take("eqsl_ag").toString()));
    record.setValue("eqsl_qslrdate",parseDate(contact.take("eqsl_qslrdate").toString()));
    record.setValue("eqsl_qslsdate",parseDate(contact.take("eqsl_qslsdate").toString()));
    record.setValue("eqsl_qsl_rcvd",parseQslRcvd(contact.take("eqsl_qsl_rcvd").toString()));
    record.setValue("eqsl_qsl_sent",parseQslSent(contact.take("eqsl_qsl_sent").toString()));
    record.setValue("fists",contact.take("fists"));
    record.setValue("fists_cc",contact.take("fists_cc"));
    record.setValue("force_init",contact.take("force_init").toString().toUpper());
    record.setValue("freq_rx",contact.take("freq_rx"));
    record.setValue("gridsquare_ext",contact.take("gridsquare_ext"));
    record.setValue("guest_op",contact.take("guest_op"));
    record.setValue("hamlogeu_qso_upload_date",parseDate(contact.take("hamlogeu_qso_upload_date").toString()));
    record.setValue("hamlogeu_qso_upload_status",parseUploadStatus(contact.take("hamlogeu_qso_upload_status").toString()));
    record.setValue("hamqth_qso_upload_date",parseDate(contact.take("hamqth_qso_upload_date").toString()));
    record.setValue("hamqth_qso_upload_status",parseUploadStatus(contact.take("hamqth_qso_upload_status").toString()));
    record.setValue("hrdlog_qso_upload_date",parseDate(contact.take("hrdlog_qso_upload_date").toString()));
    record.setValue("hrdlog_qso_upload_status",parseUploadStatus(contact.take("hrdlog_qso_upload_status").toString()));
    record.setValue("iota_island_id",contact.take("iota_island_id").toString().toUpper());
    record.setValue("k_index",contact.take("k_index"));
    record.setValue("lat",contact.take("lat"));
    record.setValue("lon",contact.take("lon"));
    record.setValue("max_bursts",contact.take("max_bursts"));
    record.setValue("morse_key_info",contact.take("morse_key_info"));
    record.setValue("morse_key_type", parseMorseKeyType(contact.take("morse_key_type").toString()));
    record.setValue("ms_shower",contact.take("ms_shower"));
    record.setValue("my_antenna",contact.take("my_antenna"));
    record.setValue("my_antenna_intl",contact.take("my_antenna_intl"));
    record.setValue("my_altitude",contact.take("my_altitude"));
    record.setValue("my_arrl_sect",contact.take("my_arrl_sect"));
    record.setValue("my_city",contact.take("my_city"));
    record.setValue("my_city_intl",contact.take("my_city_intl"));
    record.setValue("my_country",contact.take("my_country"));
    record.setValue("my_country_intl",contact.take("my_country_intl"));
    record.setValue("my_cnty",contact.take("my_cnty"));
    record.setValue("my_cnty_alt",contact.take("my_cnty_alt"));
    record.setValue("my_cq_zone",contact.take("my_cq_zone"));
    record.setValue("my_darc_dok",contact.take("my_darc_dok").toString().toUpper());
    record.setValue("my_dxcc",contact.take("my_dxcc"));
    record.setValue("my_fists",contact.take("my_fists"));
    record.setValue("my_gridsquare",contact.take("my_gridsquare").toString().toUpper());
    record.setValue("my_gridsquare_ext",contact.take("my_gridsquare_ext").toString().toUpper());
    record.setValue("my_iota",contact.take("my_iota").toString().toUpper());
    record.setValue("my_iota_island_id",contact.take("my_iota_island_id").toString().toUpper());
    record.setValue("my_itu_zone",contact.take("my_itu_zone"));
    record.setValue("my_lat",contact.take("my_lat"));
    record.setValue("my_lon",contact.take("my_lon"));
    record.setValue("my_morse_key_info",contact.take("my_morse_key_info"));
    record.setValue("my_morse_key_type", parseMorseKeyType(contact.take("my_morse_key_type").toString()));
    record.setValue("my_name",contact.take("my_name"));
    record.setValue("my_name_intl",contact.take("my_name_intl"));
    record.setValue("my_postal_code",contact.take("my_postal_code"));
    record.setValue("my_postal_code_intl",contact.take("my_postal_code_intl"));
    record.setValue("my_pota_ref",contact.take("my_pota_ref").toString().toUpper());
    record.setValue("my_rig",contact.take("my_rig"));
    record.setValue("my_rig_intl",contact.take("my_rig_intl"));
    record.setValue("my_sig",contact.take("my_sig"));
    record.setValue("my_sig_intl",contact.take("my_sig_intl"));
    record.setValue("my_sig_info",contact.take("my_sig_info"));
    record.setValue("my_sig_info_intl",contact.take("my_sig_info_intl"));
    record.setValue("my_sota_ref",contact.take("my_sota_ref").toString().toUpper());
    record.setValue("my_state",contact.take("my_state"));
    record.setValue("my_street",contact.take("my_street"));
    record.setValue("my_street_intl",contact.take("my_street_intl"));
    record.setValue("my_usaca_counties",contact.take("my_usaca_counties"));
    record.setValue("my_vucc_grids",contact.take("my_vucc_grids").toString().toUpper());
    record.setValue("my_wwff_ref",contact.take("my_wwff_ref").toString().toUpper());
    record.setValue("name",contact.take("name"));
    record.setValue("name_intl",contact.take("name_intl"));
    record.setValue("notes",contact.take("notes"));
    record.setValue("notes_intl",contact.take("notes_intl"));
    record.setValue("nr_bursts",contact.take("nr_bursts"));
    record.setValue("nr_pings",contact.take("nr_pings"));
    record.setValue("operator",contact.take("operator"));
    record.setValue("owner_callsign",contact.take("owner_callsign"));
    record.setValue("pota_ref",contact.take("pota_ref").toString().toUpper());
    record.setValue("precedence",contact.take("precedence"));
    record.setValue("prop_mode",contact.take("prop_mode"));
    record.setValue("public_key",contact.take("public_key"));
    record.setValue("qrzcom_qso_download_date",parseDate(contact.take("qrzcom_qso_download_date").toString()));
    record.setValue("qrzcom_qso_download_status",parseDownloadStatus(contact.take("qrzcom_qso_download_status").toString()));
    record.setValue("qrzcom_qso_upload_date",parseDate(contact.take("qrzcom_qso_upload_date").toString()));
    record.setValue("qrzcom_qso_upload_status",parseUploadStatus(contact.take("qrzcom_qso_upload_status").toString()));
    record.setValue("qsl_rcvd_via",contact.take("qsl_rcvd_via").toString().toUpper());
    record.setValue("qsl_sent_via",contact.take("qsl_sent_via").toString().toUpper());
    record.setValue("qsl_via",contact.take("qsl_via"));
    record.setValue("qso_complete",contact.take("qso_complete").toString().toUpper());
    record.setValue("qso_random",contact.take("qso_random").toString().toUpper());
    record.setValue("qslmsg",contact.take("qslmsg"));
    record.setValue("qslmsg_intl",contact.take("qslmsg_intl"));
    record.setValue("qslmsg_rcvd",contact.take("qslmsg_rcvd"));
    record.setValue("qth",contact.take("qth"));
    record.setValue("qth_intl",contact.take("qth_intl"));
    record.setValue("region",contact.take("region"));
    record.setValue("rig",contact.take("rig"));
    record.setValue("rig_intl",contact.take("rig_intl"));
    record.setValue("rx_pwr",contact.take("rx_pwr"));
    record.setValue("sat_mode",contact.take("sat_mode"));
    record.setValue("sat_name",contact.take("sat_name"));
    record.setValue("sfi",contact.take("sfi"));
    record.setValue("sig",contact.take("sig"));
    record.setValue("sig_intl",contact.take("sig_intl"));
    record.setValue("sig_info",contact.take("sig_info"));
    record.setValue("sig_info_intl",contact.take("sig_info_intl"));
    record.setValue("silent_key",contact.take("silent_key").toString().toUpper());
    record.setValue("skcc",contact.take("skcc"));
    record.setValue("sota_ref",contact.take("sota_ref").toString().toUpper());
    record.setValue("srx",contact.take("srx"));
    record.setValue("srx_string",contact.take("srx_string"));
    record.setValue("station_callsign",contact.take("station_callsign").toString().toUpper());
    record.setValue("stx",contact.take("stx"));
    record.setValue("stx_string",contact.take("stx_string"));
    record.setValue("swl",contact.take("swl").toString().toUpper());
    record.setValue("ten_ten",contact.take("ten_ten"));
    record.setValue("uksmg",contact.take("uksmg"));
    record.setValue("usaca_counties",contact.take("usaca_counties"));
    record.setValue("ve_prov",contact.take("ve_prov"));
    record.setValue("vucc_grids",contact.take("vucc_grids").toString().toUpper());
    record.setValue("web",contact.take("web"));
    record.setValue("wwff_ref",contact.take("wwff_ref").toString().toUpper());

    QString mode = contact.take("mode").toString().toUpper();
    QString submode = contact.take("submode").toString().toUpper();

    const QPair<QString, QString>& legacy = Data::instance()->legacyMode(mode);

    if ( !legacy.first.isEmpty() )
    {
        mode = legacy.first;
        submode = legacy.second;
    }

    record.setValue("mode", mode);
    record.setValue("submode", submode);

    const QDate &date_on = parseDate(contact.take("qso_date").toString());
    QDate date_off = parseDate(contact.take("qso_date_off").toString());

    if ( date_off.isNull() || !date_off.isValid() )
    {
        date_off = date_on;
    }

    QTime time_on = parseTime(contact.take("time_on").toString());
    QTime time_off = parseTime(contact.take("time_off").toString());

    if ( time_on.isValid() && time_off.isNull() )
    {
        time_off = time_on;
    }

    if ( time_off.isValid() && time_on.isNull() )
    {
        time_on = time_off;
    }

    QDateTime start_time(date_on, time_on, QTimeZone::utc());
    QDateTime end_time(date_off, time_off, QTimeZone::utc());

    if ( end_time < start_time )
    {
        qCDebug(runtime) << "End time before start time!" << record;
    }

    record.setValue("start_time", start_time);
    record.setValue("end_time", end_time);
}

const QString AdiFormat::formatOuput(OutputFieldFormatter formatter, const QVariant &in)
{
    QString ret;

    switch (formatter)
    {
    case TOLOWER:
        ret = toLower(in);
        break;
    case TOUPPER:
        ret = toUpper(in);
        break;
    case TODATE:
        ret = toDate(in);
        break;
    case TOTIME:
        ret = toTime(in);
        break;
    case REMOVEDEFAULTVALUEN:
        ret = removeDefaulValueN(in);
        break;
    case TOSTRING:
    default:
        ret = toString(in);
    }

    return ret;
}

const QString AdiFormat::toString(const QVariant &var)
{
    return var.toString();
}

const QString AdiFormat::toLower(const QVariant &var)
{
    return var.toString().toLower();
}

const QString AdiFormat::toUpper(const QVariant &var)
{
    return var.toString().toUpper();
}

const QString AdiFormat::toDate(const QVariant &var)
{
    return var.toDate().toString("yyyyMMdd");
}

const QString AdiFormat::toTime(const QVariant &var)
{
    return var.toTime().toString("hhmmss");
}

// There are fields with a default value of 'N'.
// Therefore, it is not necessary to explicitly export them when
// the local database also has the value 'N'. This approach can be
// helpful, for example, in the case of new fields in ADIF, where
// external systems are not yet prepared, and QLog would generate ADIF
// with the default value 'N'.
const QString AdiFormat::removeDefaulValueN(const QVariant &var)
{
    const QString value = var.toString();
    return (value == "N") ? QString() : value;
}

void AdiFormat::preprocessINTLField(const QString &fieldName,
                                    const QString &fieldIntlName,
                                    QMap<QString, QVariant> &contact)
{
    FCT_IDENTIFICATION;

    // NOTE: If modify this, modify also function below!!!!

    const QVariant &fld = contact.value(fieldName);
    const QVariant &fldIntl = contact.value(fieldIntlName);

    /* In general, it is a hack because ADI must not contain
     * _INTL fields. But some applications generate _INTL fields in ADI files
     * therefore it is needed to implement a logic how to convert INTL fields
     * to standard
     */
    if ( !fld.toString().isEmpty() && !fldIntl.toString().isEmpty() )
    {
        /* ascii and intl are present */
        //no action
    }
    else if ( !fld.toString().isEmpty() && fldIntl.toString().isEmpty() )
    {
        /* ascii is present but Intl is not present */
        contact[fieldIntlName] = fld;
    }
    else if ( fld.toString().isEmpty() && !fldIntl.toString().isEmpty() )
    {
        /* ascii is empty but Intl is present */
        contact[fieldName] = Data::removeAccents(fldIntl.toString());
    }
    else
    {
        /* both are empty */
        /* do nothing */
    }
}

void AdiFormat::preprocessINTLField(const QString &fieldName,
                                    const QString &fieldIntlName,
                                    QSqlRecord &contact)
{
    FCT_IDENTIFICATION;

    // NOTE: If modify this, modify also function above!!!!
    const QVariant &fld = contact.value(fieldName);
    const QVariant &fldIntl = contact.value(fieldIntlName);

    /* In general, it is a hack because ADI must not contain
     * _INTL fields. But some applications generate _INTL fields in ADI files
     * therefore it is needed to implement a logic how to convert INTL fields
     * to standard
     */
    if ( !fld.toString().isEmpty() && !fldIntl.toString().isEmpty() )
    {
        /* ascii and intl are present */
        //no action
    }
    else if ( !fld.toString().isEmpty() && fldIntl.toString().isEmpty() )
    {
        /* ascii is present but Intl is not present */
        contact.setValue(fieldIntlName, fld);
    }
    else if ( fld.toString().isEmpty() && !fldIntl.toString().isEmpty() )
    {
        /* ascii is empty but Intl is present */
        contact.setValue(fieldName, Data::removeAccents(fldIntl.toString()));
    }
    else
    {
        /* both are empty */
        /* do nothing */
    }
}

bool AdiFormat::readContact(QMap<QString, QVariant>& contact)
{
    FCT_IDENTIFICATION;

    while (!stream.atEnd())
    {
        QString field;
        QString value;

        readField(field, value);
        field = field.toLower();

        if (field == "eor")
        {
            return true;
        }

        if (!value.isEmpty())
        {
            contact[field] = QVariant(value);
        }
    }

    return false;
}

AdiFormat::AdiFormat(QTextStream &stream) :
    LogFormat(stream)
{
    FCT_IDENTIFICATION;

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    stream.setEncoding(QStringConverter::Latin1);
#else
    stream.setCodec("ISO 8859-1");
#endif
}

bool AdiFormat::importNext(QSqlRecord& record)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<record;

    QMap<QString, QVariant> contact;

    if ( !readContact(contact) )
    {
        return false;
    }

    mapContact2SQLRecord(contact, record);

    return true;
}

QDate AdiFormat::parseDate(const QString &date)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<date;

    if (date.length() == 8) {
        return QDate::fromString(date, "yyyyMMdd");
    }
    else {
        return QDate();
    }
}

QTime AdiFormat::parseTime(const QString &time)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<time;

    switch (time.length()) {
    case 4:
        return QTime::fromString(time, "hhmm");

    case 6:
        return QTime::fromString(time, "hhmmss");

    default:
        return QTime();
    }
}


QString AdiFormat::parseQslRcvd(const QString &value) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<value;

    if (!value.isEmpty())
    {
        switch (value.toUpper().at(0).toLatin1())
        {
        case 'Y': return "Y";
        case 'N': return "N";
        case 'R': return "R";
        case 'I': return "I";
        case 'V': return "Y";
        default: return "N";
        }
    }
    return "N";
}

QString AdiFormat::parseQslSent(const QString &value) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<value;

    if (!value.isEmpty())
    {
        switch (value.toUpper().at(0).toLatin1())
        {
        case 'Y': return "Y";
        case 'N': return "N";
        case 'R': return "R";
        case 'Q': return "Q";
        case 'I': return "I";
        default: return "N";
        }
    }
    return "N";
}

QString AdiFormat::parseUploadStatus(const QString &value)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<value;

    if (!value.isEmpty())
    {
        switch (value.toUpper().at(0).toLatin1())
        {
        case 'Y': return "Y";
        case 'N': return "N";
        case 'M': return "M";
        default: return QString();
        }
    }
    return QString();
}

QString AdiFormat::parseDownloadStatus(const QString &value)
{
    FCT_IDENTIFICATION;

    if ( value.isEmpty() )
        return QString();

    char firstChar = value.toUpper().at(0).toLatin1();

    return ( firstChar == 'Y' || firstChar == 'N' || firstChar == 'I' ) ? QString(firstChar)
                                                                        : QString();
}

QString AdiFormat::parseMorseKeyType(const QString &value)
{
    FCT_IDENTIFICATION;

    static QList<QString> allowedValues({"SK", "SS", "BUG", "FAB", "SP", "DP", "CPU"});
    const QString &inValue = value.toUpper();

    return (allowedValues.contains(inValue)) ? inValue : QString();
}

QString AdiFormat::parseEqslAg(const QString &value)
{
    FCT_IDENTIFICATION;

    if ( value.isEmpty() ) return QString();

    char firstChar = value.toUpper().at(0).toLatin1();

    return ( firstChar == 'Y' || firstChar == 'N' || firstChar == 'U' ) ? QString(firstChar)
                                                                        : QString();
}

QMap<QString, QString> AdiFormat::fieldname2INTLNameMapping =
{
    {"address", "address_intl"},
    {"comment", "comment_intl"},
    {"country", "country_intl"},
    {"my_antenna", "my_antenna_intl"},
    {"my_city", "my_city_intl"},
    {"my_country", "my_country_intl"},
    {"my_name", "my_name_intl"},
    {"my_postal_code", "my_postal_code_intl"},
    {"my_rig", "my_rig_intl"},
    {"my_sig", "my_sig_intl"},
    {"my_sig_info", "my_sig_info_intl"},
    {"my_street", "my_street_intl"},
    {"name", "name_intl"},
    {"notes", "notes_intl"},
    {"qslmsg", "qslmsg_intl"},
    {"qth", "qth_intl"},
    {"rig", "rig_intl"},
    {"sig", "sig_intl"},
    {"sig_info", "sig_info_intl"}
};

QHash<QString, AdiFormat::ExportParams> AdiFormat::DB2ADIFExportParams =
{
    { "callsign", ExportParams("call")},
//    { , ExportParams("qso_date")},     //SPECIAL
//    { , ExportParams("time_on")},      //SPECIAL
//    { , ExportParams("qso_date_off")}, //SPECIAL
//    { , ExportParams("time_off")},     //SPECIAL
    { "rst_rcvd", ExportParams("rst_rcvd")},
    { "rst_sent", ExportParams("rst_sent")},
    { "name", ExportParams("name")},
    { "qth", ExportParams("qth")},
    { "gridsquare", ExportParams("gridsquare")},
    { "cqz", ExportParams("cqz")},
    { "ituz", ExportParams("ituz")},
    { "freq", ExportParams("freq", OutputFieldFormatter::TOSTRING)},
    { "band", ExportParams("band", OutputFieldFormatter::TOLOWER)},
    { "mode", ExportParams("mode")},
    { "submode", ExportParams("submode")},
    { "cont", ExportParams("cont")},
    { "dxcc", ExportParams("dxcc")},
    { "country", ExportParams("country")},
    { "pfx", ExportParams("pfx")},
    { "state", ExportParams("state")},
    { "cnty", ExportParams("cnty")},
    { "cnty_alt", ExportParams("cnty_alt")},
    { "iota", ExportParams("iota", OutputFieldFormatter::TOUPPER)},
    { "qsl_rcvd", ExportParams("qsl_rcvd", OutputFieldFormatter::REMOVEDEFAULTVALUEN)},
    { "qsl_rdate", ExportParams("qslrdate", OutputFieldFormatter::TODATE)},
    { "qsl_sent", ExportParams("qsl_sent", OutputFieldFormatter::REMOVEDEFAULTVALUEN)},
    { "qsl_sdate", ExportParams("qslsdate", OutputFieldFormatter::TODATE)},
    { "lotw_qsl_rcvd", ExportParams("lotw_qsl_rcvd", OutputFieldFormatter::REMOVEDEFAULTVALUEN)},
    { "lotw_qslrdate", ExportParams("lotw_qslrdate", OutputFieldFormatter::TODATE)},
    { "lotw_qsl_sent", ExportParams("lotw_qsl_sent", OutputFieldFormatter::REMOVEDEFAULTVALUEN)},
    { "lotw_qslsdate", ExportParams("lotw_qslsdate", OutputFieldFormatter::TODATE)},
    { "tx_pwr", ExportParams("tx_pwr")},
    { "address", ExportParams("address")},
    { "age", ExportParams("age")},
    { "altitude", ExportParams("altitude")},
    { "a_index", ExportParams("a_index")},
    { "ant_az", ExportParams("ant_az")},
    { "ant_el", ExportParams("ant_el")},
    { "ant_path", ExportParams("ant_path")},
    { "arrl_sect", ExportParams("arrl_sect")},
    { "award_submitted", ExportParams("award_submitted")},
    { "award_granted", ExportParams("award_granted")},
    { "band_rx", ExportParams("band_rx", OutputFieldFormatter::TOLOWER)},
    { "check", ExportParams("check")},
    { "class", ExportParams("class")},
    { "clublog_qso_upload_date", ExportParams("clublog_qso_upload_date", OutputFieldFormatter::TODATE)},
    { "clublog_qso_upload_status", ExportParams("clublog_qso_upload_status")},
    { "comment", ExportParams("comment")},
    { "contacted_op", ExportParams("contacted_op")},
    { "contest_id", ExportParams("contest_id")},
    { "credit_submitted", ExportParams("credit_submitted")},
    { "credit_granted", ExportParams("credit_granted")},
    { "darc_dok", ExportParams("darc_dok")},
    { "dcl_qslrdate", ExportParams("dcl_qslrdate", OutputFieldFormatter::TODATE)},
    { "dcl_qslsdate", ExportParams("dcl_qslsdate", OutputFieldFormatter::TODATE)},
    { "dcl_qsl_rcvd", ExportParams("dcl_qsl_rcvd", OutputFieldFormatter::REMOVEDEFAULTVALUEN)},
    { "dcl_qsl_sent", ExportParams("dcl_qsl_sent", OutputFieldFormatter::REMOVEDEFAULTVALUEN)},
    { "distance", ExportParams("distance")},
    { "email", ExportParams("email")},
    { "eq_call", ExportParams("eq_call")},
    { "eqsl_ag", ExportParams("eqsl_ag", OutputFieldFormatter::TOUPPER)},
    { "eqsl_qslrdate", ExportParams("eqsl_qslrdate", OutputFieldFormatter::TODATE)},
    { "eqsl_qslsdate", ExportParams("eqsl_qslsdate", OutputFieldFormatter::TODATE)},
    { "eqsl_qsl_rcvd", ExportParams("eqsl_qsl_rcvd", OutputFieldFormatter::REMOVEDEFAULTVALUEN)},
    { "eqsl_qsl_sent", ExportParams("eqsl_qsl_sent", OutputFieldFormatter::REMOVEDEFAULTVALUEN)},
    { "fists", ExportParams("fists")},
    { "fists_cc", ExportParams("fists_cc")},
    { "force_init", ExportParams("force_init")},
    { "freq_rx", ExportParams("freq_rx")},
    { "gridsquare_ext", ExportParams("gridsquare_ext")},
    { "guest_op", ExportParams("guest_op")},
    { "hamlogeu_qso_upload_date", ExportParams("hamlogeu_qso_upload_date", OutputFieldFormatter::TODATE)},
    { "hamlogeu_qso_upload_status", ExportParams("hamlogeu_qso_upload_status")},
    { "hamqth_qso_upload_date", ExportParams("hamqth_qso_upload_date", OutputFieldFormatter::TODATE)},
    { "hamqth_qso_upload_status", ExportParams("hamqth_qso_upload_status")},
    { "hrdlog_qso_upload_date", ExportParams("hrdlog_qso_upload_date", OutputFieldFormatter::TODATE)},
    { "hrdlog_qso_upload_status", ExportParams("hrdlog_qso_upload_status")},
    { "iota_island_id", ExportParams("iota_island_id", OutputFieldFormatter::TOUPPER)},
    { "k_index", ExportParams("k_index")},
    { "lat", ExportParams("lat")},
    { "lon", ExportParams("lon")},
    { "max_bursts", ExportParams("max_bursts")},
    { "morse_key_info", ExportParams("morse_key_info")},
    { "morse_key_type", ExportParams("morse_key_type", OutputFieldFormatter::TOUPPER)},
    { "ms_shower", ExportParams("ms_shower")},
    { "my_altitude", ExportParams("my_altitude")},
    { "my_arrl_sect", ExportParams("my_arrl_sect")},
    { "my_antenna", ExportParams("my_antenna")},
    { "my_city", ExportParams("my_city")},
    { "my_cnty", ExportParams("my_cnty")},
    { "my_cnty_alt", ExportParams("my_cnty_alt")},
    { "my_country", ExportParams("my_country")},
    { "my_cq_zone", ExportParams("my_cq_zone")},
    { "my_darc_dok", ExportParams("my_darc_dok")},
    { "my_dxcc", ExportParams("my_dxcc")},
    { "my_fists", ExportParams("my_fists")},
    { "my_gridsquare", ExportParams("my_gridsquare")},
    { "my_gridsquare_ext", ExportParams("my_gridsquare_ext")},
    { "my_iota", ExportParams("my_iota", OutputFieldFormatter::TOUPPER)},
    { "my_iota_island_id", ExportParams("my_iota_island_id", OutputFieldFormatter::TOUPPER)},
    { "my_itu_zone", ExportParams("my_itu_zone")},
    { "my_lat", ExportParams("my_lat")},
    { "my_lon", ExportParams("my_lon")},
    { "my_morse_key_info", ExportParams("my_morse_key_info")},
    { "my_morse_key_type", ExportParams("my_morse_key_type", OutputFieldFormatter::TOUPPER)},
    { "my_name", ExportParams("my_name")},
    { "my_postal_code", ExportParams("my_postal_code")},
    { "my_pota_ref", ExportParams("my_pota_ref", OutputFieldFormatter::TOUPPER)},
    { "my_rig", ExportParams("my_rig")},
    { "my_sig", ExportParams("my_sig")},
    { "my_sig_info", ExportParams("my_sig_info")},
    { "my_sota_ref", ExportParams("my_sota_ref", OutputFieldFormatter::TOUPPER)},
    { "my_state", ExportParams("my_state")},
    { "my_street", ExportParams("my_street")},
    { "my_usaca_counties", ExportParams("my_usaca_counties")},
    { "my_vucc_grids", ExportParams("my_vucc_grids", OutputFieldFormatter::TOUPPER)},
    { "my_wwff_ref", ExportParams("my_wwff_ref", OutputFieldFormatter::TOUPPER)},
    { "notes", ExportParams("notes")},
    { "nr_bursts", ExportParams("nr_bursts")},
    { "nr_pings", ExportParams("nr_pings")},
    { "operator", ExportParams("operator")},
    { "owner_callsign", ExportParams("owner_callsign")},
    { "pota_ref", ExportParams("pota_ref", OutputFieldFormatter::TOUPPER)},
    { "precedence", ExportParams("precedence")},
    { "prop_mode", ExportParams("prop_mode")},
    { "public_key", ExportParams("public_key")},
    { "qrzcom_qso_download_date", ExportParams("qrzcom_qso_download_date", OutputFieldFormatter::TODATE)},
    { "qrzcom_qso_download_status", ExportParams("qrzcom_qso_download_status")},
    { "qrzcom_qso_upload_date", ExportParams("qrzcom_qso_upload_date", OutputFieldFormatter::TODATE)},
    { "qrzcom_qso_upload_status", ExportParams("qrzcom_qso_upload_status")},
    { "qslmsg", ExportParams("qslmsg")},
    { "qslmsg_rcvd", ExportParams("qslmsg_rcvd")},
    { "qsl_rcvd_via", ExportParams("qsl_rcvd_via")},
    { "qsl_sent_via", ExportParams("qsl_sent_via")},
    { "qsl_via", ExportParams("qsl_via")},
    { "qso_complete", ExportParams("qso_complete")},
    { "qso_random", ExportParams("qso_random")},
    { "region", ExportParams("region")},
    { "rig", ExportParams("rig")},
    { "rx_pwr", ExportParams("rx_pwr")},
    { "sat_mode", ExportParams("sat_mode")},
    { "sat_name", ExportParams("sat_name")},
    { "sfi", ExportParams("sfi")},
    { "sig", ExportParams("sig")},
    { "sig_info", ExportParams("sig_info")},
    { "silent_key", ExportParams("silent_key")},
    { "skcc", ExportParams("skcc")},
    { "sota_ref", ExportParams("sota_ref", OutputFieldFormatter::TOUPPER)},
    { "srx", ExportParams("srx")},
    { "srx_string", ExportParams("srx_string")},
    { "station_callsign", ExportParams("station_callsign")},
    { "stx", ExportParams("stx")},
    { "stx_string", ExportParams("stx_string")},
    { "swl", ExportParams("swl")},
    { "ten_ten", ExportParams("ten_ten")},
    { "uksmg", ExportParams("uksmg")},
    { "usaca_counties", ExportParams("usaca_counties")},
    { "ve_prov", ExportParams("ve_prov")},
    { "vucc_grids", ExportParams("vucc_grids", OutputFieldFormatter::TOUPPER)},
    { "web", ExportParams("web")},
    { "wwff_ref", ExportParams("wwff_ref", OutputFieldFormatter::TOUPPER)}
};

#undef ALWAYS_PRESENT
