#include <QProgressDialog>
#include <QMessageBox>
#include <QtSql>
#include <QDebug>
#include <QUuid>
#include "core/Migration.h"
#include "debug.h"
#include "data/Data.h"
#include "LogParam.h"
#include "LOVDownloader.h"
#include "ClubLog.h"

MODULE_IDENTIFICATION("qlog.core.migration");

/**
 * Migrate the database to the latest schema version.
 * Returns true on success.
 */
bool Migration::run() {
    FCT_IDENTIFICATION;

    int currentVersion = getVersion();

    if (currentVersion == latestVersion) {
        qCDebug(runtime) << "Database schema already up to date";
        updateExternalResource();
        // temporarily added to create a trigger without calling db migration
        //refreshUploadStatusTrigger();
        return true;
    }
    else if (currentVersion < latestVersion) {
        qCDebug(runtime) << "Starting database migration";
    }
    else {
        qCritical() << "database from the future";
        return false;
    }

    QProgressDialog progress("Migrating the database...", nullptr, currentVersion, latestVersion);
    progress.show();

    while ((currentVersion = getVersion()) < latestVersion)
    {
        bool res = migrate(currentVersion+1);
        if ( !res || getVersion() == currentVersion )
        {
            progress.close();
            return false;
        }
        // sometimes (especially when DROP INDEX is called), it is needed to
        // reopen database. (issue occurs between DB versions 15 and 16)
        QSqlDatabase::database().close();

        if ( !QSqlDatabase::database().open() )
        {
            progress.close();
            qCritical() << QSqlDatabase::database().lastError();
            return false;
        }
        progress.setValue(currentVersion);
    }

    if ( !refreshUploadStatusTrigger() )
    {
        qCritical() << "Cannot refresh Upload Status Trigger";
        progress.close();
        return false;
    }

    progress.close();

    updateExternalResource();

    qCDebug(runtime) << "Database migration successful";

    return true;
}

/**
 * Returns the current user_version of the database.
 */
int Migration::getVersion() {
    FCT_IDENTIFICATION;

    QSqlQuery query("SELECT version FROM schema_versions "
                    "ORDER BY version DESC LIMIT 1");

    int i = query.first() ? query.value(0).toInt() : 0;
    qCDebug(runtime) << i;
    return i;
}

/**
 * Changes the user_version of the database to version.
 * Returns true of the operation was successful.
 */
bool Migration::setVersion(int version) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << version;

    QSqlQuery query;
    if ( ! query.prepare("INSERT INTO schema_versions (version, updated) "
                  "VALUES (:version, datetime('now'))") )
    {
        qWarning() << "Cannot prepare Insert statement";
    }

    query.bindValue(":version", version);

    if (!query.exec()) {
        qWarning() << "setting schema version failed" << query.lastError();
        return false;
    }
    else {
        return true;
    }
}

/**
 * Migrate the database to the given version.
 * Returns true if the operation was successful.
 */
bool Migration::migrate(int toVersion) {
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "migrate to" << toVersion;

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        qCritical() << "transaction failed";
        return false;
    }

    QString migration_file = QString(":/res/sql/migration_%1.sql").arg(toVersion, 3, 10, QChar('0'));
    bool result = runSqlFile(migration_file);

    result = result && functionMigration(toVersion);

    if (result && setVersion(toVersion) && db.commit()) {
        return true;
    }
    else {
        if (!db.rollback()) {
            qCritical() << "rollback failed";
        }
        return false;
    }
}

bool Migration::runSqlFile(QString filename) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filename;

    QFile sqlFile(filename);
    sqlFile.open(QIODevice::ReadOnly | QIODevice::Text);

    QStringList sqlQueries = QTextStream(&sqlFile).readAll().split('\n').join(" ").split(';');
    qCDebug(runtime) << sqlQueries;

    foreach (QString sqlQuery, sqlQueries) {
        if (sqlQuery.trimmed().isEmpty()) {
            continue;
        }

        qCDebug(runtime) << sqlQuery;

        QSqlQuery query;
        if (!query.exec(sqlQuery))
        {
            qCDebug(runtime) << query.lastError();
            return false;
        }
        query.finish();
    }

    return true;
}

bool Migration::functionMigration(int version)
{
    FCT_IDENTIFICATION;

    bool ret = false;
    switch ( version )
    {
    case 4:
        ret = fixIntlFields();
        break;
    case 6:
        ret = insertUUID();
        break;
    case 15:
        ret = fillMyDXCC();
        break;
    case 17:
        ret = createTriggers();
        break;
    case 23:
        ret = importQSLCards2DB();
        break;
    case 26:
        ret = fillCQITUZStationProfiles();
        break;
    case 28:
        ret = resetConfigs();
        break;
    case 29:
        ret = profiles2DB();
        break;
    default:
        ret = true;
    }

    return ret;
}

int Migration::tableRows(QString name) {
    FCT_IDENTIFICATION;
    int i = 0;
    QSqlQuery query(QString("SELECT count(*) FROM %1").arg(name));
    i = query.first() ? query.value(0).toInt() : 0;
    qCDebug(runtime) << i;
    return i;
}

bool Migration::updateExternalResource()
{
    FCT_IDENTIFICATION;

    LOVDownloader downloader;

    QProgressDialog progress;

    connect(&downloader, &LOVDownloader::processingSize,
            &progress, &QProgressDialog::setMaximum);
    connect(&downloader, &LOVDownloader::progress,
            &progress, &QProgressDialog::setValue);
    connect(&downloader, &LOVDownloader::finished,
            &progress, &QProgressDialog::done);
    connect(&downloader, &LOVDownloader::noUpdate,
            &progress, &QProgressDialog::cancel);
    connect(&progress, &QProgressDialog::canceled,
            &downloader, &LOVDownloader::abortRequest);

    if ( ! updateExternalResourceProgress(progress, downloader, LOVDownloader::CTY, "(1/7)") )
        return false;
    if ( ! updateExternalResourceProgress(progress, downloader, LOVDownloader::SATLIST, "(2/7)") )
        return false;
    if ( ! updateExternalResourceProgress(progress, downloader, LOVDownloader::SOTASUMMITS, "(3/7)") )
        return false;
    if ( ! updateExternalResourceProgress(progress, downloader, LOVDownloader::WWFFDIRECTORY, "(4/7)") )
        return false;
    if ( ! updateExternalResourceProgress(progress, downloader, LOVDownloader::IOTALIST, "(5/7)") )
        return false;
    if ( ! updateExternalResourceProgress(progress, downloader, LOVDownloader::POTADIRECTORY, "(6/7)") )
        return false;
    if ( ! updateExternalResourceProgress(progress, downloader, LOVDownloader::MEMBERSHIPCONTENTLIST, "(7/7)") )
        return false;

    return true;
}

bool Migration::updateExternalResourceProgress(QProgressDialog& progress,
                                               LOVDownloader& downloader,
                                               const LOVDownloader::SourceType & sourceType,
                                               const QString &counter)
{
    FCT_IDENTIFICATION;

    QString stringInfo;

    progress.reset();
    switch ( sourceType )
    {
    case LOVDownloader::SourceType::CTY:
        stringInfo = tr("DXCC Entities");
        break;
    case LOVDownloader::SourceType::SATLIST:
        stringInfo = tr("Sats Info");
        break;
    case LOVDownloader::SourceType::SOTASUMMITS:
        stringInfo = tr("SOTA Summits");
        break;
    case LOVDownloader::SourceType::WWFFDIRECTORY:
        stringInfo = tr("WWFF Records");
        break;
    case LOVDownloader::SourceType::IOTALIST:
        stringInfo = tr("IOTA Records");
        break;
    case LOVDownloader::SourceType::POTADIRECTORY:
        stringInfo = tr("POTA Records");
        break;
    case LOVDownloader::SourceType::MEMBERSHIPCONTENTLIST:
        stringInfo = tr("Membership Directory Records");
        break;

    default:
        stringInfo = tr("List of Values");
    }

    progress.setLabelText(tr("Updating ") + stringInfo + " " + counter +" ...");
    progress.setMinimum(0);

    progress.show();

    downloader.update(sourceType);

    if ( progress.wasCanceled() )
    {
        qCDebug(runtime) << "Update was canceled";
    }
    else
    {
        if ( !progress.exec() )
        {
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 stringInfo + tr(" Update Failed"));
            return false;
        }
    }

    return true;
}

/* Fixing error when QLog stored UTF characters to non-Intl field of ADIF (contact) table */
/* The fix has two steps
 * 1) Update contact to move all non-intl to intl fields
 * 2) transform intl field to non-intl field by calloni removeAccents
 */
bool Migration::fixIntlFields()
{
    FCT_IDENTIFICATION;

    QSqlQuery query;
    QSqlQuery update;

    if ( !query.prepare( "SELECT id, name, name_intl, "
                         "           qth, qth_intl, "
                         "           comment, comment_intl, "
                         "           my_antenna, my_antenna_intl,"
                         "           my_city, my_city_intl,"
                         "           my_rig, my_rig_intl,"
                         "           my_sig, my_sig_intl,"
                         "           my_sig_info, my_sig_info_intl,"
                         "           sig, sig_intl,"
                         "           sig_info, sig_info_intl "
                         " FROM contacts" ) )
    {
        qWarning()<< " Cannot prepare a migration script - fixIntlField 1";
        return false;
    }

    if( !query.exec() )
    {
        qWarning()<< "Cannot exec a migration script - fixIntlFields 1";
        return false;
    }

    if ( !update.prepare("UPDATE contacts SET name    = :name,"
                         "                    qth     = :qth, "
                         "                    comment = :comment,"
                         "                    my_antenna = :my_antenna,"
                         "                    my_city = :my_city,"
                         "                    my_rig = :my_rig,"
                         "                    my_sig = :my_sig,"
                         "                    my_sig_info = :my_sig_info,"
                         "                    sig = :sig,"
                         "                    sig_info = :sig_info "
                         "WHERE id = :id") )
    {
        qWarning()<< " Cannot prepare a migration script - fixIntlField 2";
        return false;
    }

    while( query.next() )
    {
        update.bindValue(":id", query.value("id").toInt());
        update.bindValue(":name",       fixIntlField(query, "name", "name_intl"));
        update.bindValue(":qth",        fixIntlField(query, "qth", "qth_intl"));
        update.bindValue(":comment",    fixIntlField(query, "comment", "comment_intl"));
        update.bindValue(":my_antenna", fixIntlField(query, "my_antenna", "my_antenna_intl"));
        update.bindValue(":my_city",    fixIntlField(query, "my_city", "my_city_intl"));
        update.bindValue(":my_rig",     fixIntlField(query, "my_rig", "my_rig_intl"));
        update.bindValue(":my_sig",     fixIntlField(query, "my_sig", "my_sig_intl"));
        update.bindValue(":my_sig_info",fixIntlField(query, "my_sig_info", "my_sig_info_intl"));
        update.bindValue(":sig",        fixIntlField(query, "sig", "sig_intl"));
        update.bindValue(":sig_info",   fixIntlField(query, "sig_info", "sig_info_intl"));

        if ( !update.exec())
        {
            qWarning() << "Cannot exec a migration script - fixIntlFields 2";
            return false;
        }
    }

    return true;
}

bool Migration::insertUUID()
{
    FCT_IDENTIFICATION;

    return LogParam::setParam("logid", QUuid::createUuid().toString());
}

bool Migration::fillMyDXCC()
{
    FCT_IDENTIFICATION;

    QSqlQuery query;
    QSqlQuery update;

    if ( !query.prepare( "SELECT DISTINCT station_callsign FROM contacts" ) )
    {
        qWarning()<< " Cannot prepare a migration script - fillMyDXCC 1" << query.lastError();
        return false;
    }

    if( !query.exec() )
    {
        qWarning()<< "Cannot exec a migration script - fillMyDXCC 1" << query.lastError();
        return false;
    }

    if ( !update.prepare("UPDATE contacts "
                         "SET my_dxcc = CASE WHEN my_dxcc IS NULL THEN :my_dxcc ELSE my_dxcc END, "
                         "    my_itu_zone = CASE WHEN my_itu_zone IS NULL THEN :my_itu_zone ELSE my_itu_zone END, "
                         "    my_cq_zone = CASE WHEN my_cq_zone IS NULL THEN :my_cq_zone ELSE my_cq_zone END, "
                         "    my_country = CASE WHEN my_country IS NULL THEN :my_country ELSE my_country END, "
                         "    my_country_intl = CASE WHEN my_country_intl IS NULL THEN :my_country_intl ELSE my_country_intl END "
                         "WHERE station_callsign = :station_callsign") )
    {
        qWarning()<< " Cannot prepare a migration script - fillMyDXCC 2" << update.lastError();
        return false;
    }

    // it is a hack because the migration is running before migration (start/stop database).
    // It caused that SQL prepared in contructor are incorrectly created.
    // Therefore, Data are create temporary here
    Data tmp;

    while( query.next() )
    {
        QString myCallsign = query.value("station_callsign").toString();
        DxccEntity dxccEntity = tmp.lookupDxcc(myCallsign);

        if ( dxccEntity.dxcc )
        {
            update.bindValue(":my_dxcc",          dxccEntity.dxcc);
            update.bindValue(":my_itu_zone",      dxccEntity.ituz);
            update.bindValue(":my_cq_zone",       dxccEntity.cqz);
            update.bindValue(":my_country_intl",  dxccEntity.country);
            update.bindValue(":my_country",       Data::removeAccents(dxccEntity.country));
            update.bindValue(":station_callsign", myCallsign);

            if ( !update.exec())
            {
                qWarning() << "Cannot exec a migration script - fillMyDXCC 2" << update.lastError();
                return false;
            }
        }
    }

    return true;
}

bool Migration::createTriggers()
{
    FCT_IDENTIFICATION;

    // Migration procedure does not support to execute SQL code with Triggers.
    // It will be safer to create triggers here than to fix the migration procedure

    QSqlQuery query;

    if ( ! query.exec(QString("CREATE TRIGGER update_callsign_contacts_autovalue "
                              "AFTER UPDATE ON contacts "
                              "FOR EACH ROW "
                              "WHEN OLD.callsign <> NEW.callsign "
                              "BEGIN "
                              "  INSERT OR REPLACE INTO contacts_autovalue (contactid, base_callsign) "
                              "  VALUES (NEW.id, (WITH tokenizedCallsign(word, csv) AS ( SELECT '', NEW.callsign||'/' "
                              "                                                     UNION ALL "
                              "                                                     SELECT substr(csv, 0, instr(csv, '/')), substr(csv, instr(csv, '/') + 1) "
                              "                                                     FROM tokenizedCallsign "
                              "                                                     WHERE csv != '' ) "
                              "                   SELECT word FROM tokenizedCallsign "
                              "                   WHERE word != '' AND word REGEXP '^([A-Z][0-9]|[A-Z]{1,2}|[0-9][A-Z])([0-9]|[0-9]+)([A-Z]+)$' LIMIT 1));"
                              "END;")))
    {
        qWarning() << "Cannot create trigger update_callsign_contacts_autovalue " << query.lastError().text();
        return false;
    }

    if ( ! query.exec(QString("CREATE TRIGGER insert_contacts_autovalue "
                              "AFTER INSERT ON contacts "
                              "FOR EACH ROW "
                              "BEGIN "
                              "  INSERT OR REPLACE INTO contacts_autovalue (contactid, base_callsign) "
                              "  VALUES (NEW.id, (WITH tokenizedCallsign(word, csv) AS ( SELECT '', NEW.callsign||'/' "
                              "                                                UNION ALL "
                              "                                                SELECT substr(csv, 0, instr(csv, '/')), substr(csv, instr(csv, '/') + 1) "
                              "                                                FROM tokenizedCallsign "
                              "                                                WHERE csv != '' ) "
                              "                   SELECT word FROM tokenizedCallsign "
                              "                   WHERE word != '' and word REGEXP '^([A-Z][0-9]|[A-Z]{1,2}|[0-9][A-Z])([0-9]|[0-9]+)([A-Z]+)$' LIMIT 1)); "
                              "END;")))
    {
        qWarning() << "Cannot create trigger update_callsign_contacts_autovalue " << query.lastError().text();
        return false;
    }

    return true;
}

bool Migration::importQSLCards2DB()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    QSqlQuery insert;

    // don't migrate eqsl imagge because it is only a local cache. If the image is missing then
    // QLog will download it again
    static QRegularExpression re("^[0-9]{8}_([0-9]+)_.*_qsl_(.*)", QRegularExpression::CaseInsensitiveOption);

    QString qslFolder = settings.value("paperqsl/qslfolder", QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).toString();
    QDir dir(qslFolder);
    QFileInfoList file_list = dir.entryInfoList(QStringList("????????_*_*_qsl_*.*"),
                                                QDir::Files,
                                                QDir::Name);
    if ( !insert.prepare("INSERT INTO contacts_qsl_cards (contactid, source, name, data) "
                         " VALUES (:contactid, 0, :name, :data)" ) )
    {
        qWarning()<< " Cannot prepare a migration script - importQSLCards2DB 2" << insert.lastError();
        return false;
    }

    for ( auto &file : qAsConst(file_list) )
    {
        qCDebug(runtime) << "Processing file" << file.fileName();
        QRegularExpressionMatch match = re.match(file.fileName());

        if ( match.hasMatch() )
        {
            qCDebug(runtime) << "File matched - importing";
            QFile f(file.absoluteFilePath());
            if ( !f.open(QFile::ReadOnly))
            {
                qWarning() << "Cannot open file" << file.fileName();
                continue;
            }
            QByteArray blob = f.readAll();
            f.close();
            //do not remove the file - the file may be used by another application
            //do not use PaperQSL Class here for saving.
            // this is a migration sequence for filesystem to DB migration with specific properties
            insert.bindValue(":contactid", match.captured(1));
            insert.bindValue(":name", match.captured(2));
            insert.bindValue(":data", blob.toBase64());

            if ( !insert.exec() )
            {
                qWarning() << "Cannot import QSL file" << file.absoluteFilePath();
                continue;
            }
            qCDebug(runtime) << "File matched - imported";
        }
    }

    settings.remove("paperqsl/qslfolder");
    settings.remove("eqsl/qslfolder");
    return true;
}

bool Migration::fillCQITUZStationProfiles()
{
    FCT_IDENTIFICATION;

    QSqlQuery query;
    QSqlQuery update;

    if ( !query.prepare( "SELECT DISTINCT callsign FROM station_profiles" ) )
    {
        qWarning()<< " Cannot prepare a migration script - fillCQITUZStationProfiles 1" << query.lastError();
        return false;
    }

    if( !query.exec() )
    {
        qWarning()<< "Cannot exec a migration script - fillCQITUZStationProfiles 1" << query.lastError();
        return false;
    }

    if ( !update.prepare("UPDATE station_profiles "
                         "SET ituz = :ituz, cqz = :cqz, dxcc = :dxcc, country = :country "
                         "WHERE upper(callsign) = :callsign") )
    {
        qWarning()<< " Cannot prepare a migration script - fillCQITUZStationProfiles 2" << update.lastError();
        return false;
    }

    // it is a hack because the migration is running before migration (start/stop database).
    // It caused that SQL prepared in contructor are incorrectly created.
    // Therefore, Data are create temporary here
    Data tmp;

    while( query.next() )
    {
        const QString &myCallsign = query.value("callsign").toString().toUpper();
        const DxccEntity &dxccEntity = tmp.lookupDxcc(myCallsign);

        if ( dxccEntity.dxcc )
        {
            update.bindValue(":ituz",  dxccEntity.ituz);
            update.bindValue(":cqz",   dxccEntity.cqz);
            update.bindValue(":dxcc",  dxccEntity.dxcc);
            update.bindValue(":country",  dxccEntity.country);
            update.bindValue(":callsign", myCallsign);

            if ( !update.exec())
            {
                qWarning() << "Cannot exec a migration script - fillCQITUZStationProfiles 2" << update.lastError();
                return false;
            }
        }
    }

    return true;
}

bool Migration::resetConfigs()
{
    FCT_IDENTIFICATION;

    // to force redownload CTY file
    QSettings settings;
    settings.remove("last_cty_update");

    // it changes to sortable view.
    // I don't know why, but when the layout is saved, the sort indicator
    // is not displayed when the sortable view is turned on. So I rather remove the view state
    settings.remove("wsjtx/state");
    return true;
}

bool Migration::profiles2DB()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    setSelectedProfile("ant_profiles", settings.value("equipment/ant/profile1", QString()).toString());
    settings.remove("equipment/ant/profile1");
    setSelectedProfile("cwkey_profiles", settings.value("equipment/cwkey/profile1", QString()).toString());
    settings.remove("equipment/cwkey/profile1");
    setSelectedProfile("cwshortcut_profiles", settings.value("equipment/cwshortcut/profile1", QString()).toString());
    settings.remove("equipment/cwshortcut/profile1");
    setSelectedProfile("rig_profiles", settings.value("equipment/rig/profile1", QString()).toString());
    settings.remove("equipment/rig/profile1");
    setSelectedProfile("rot_profiles", settings.value("equipment/rot/profile1", QString()).toString());
    settings.remove("equipment/rot/profile1");
    setSelectedProfile("rot_user_buttons_profiles", settings.value("equipment/rotusrbuttons/profile1", QString()).toString());
    settings.remove("equipment/rotusrbuttons/profile1");
    setSelectedProfile("station_profiles", settings.value("station/profile1", QString()).toString());
    settings.remove("station/profile1");
    setSelectedProfile("main_layout_profiles", settings.value("newcontact/layoutprofile/profile1", QString()).toString());
    settings.remove("newcontact/layoutprofile/profile1");

    // LOVs to DB
    settings.remove("last_cty_update");
    settings.remove("last_sat_update");
    settings.remove("last_sotasummits_update");
    settings.remove("last_wwffdirectory_update");
    settings.remove("last_iota_update");
    settings.remove("last_pota_update");
    settings.remove("last_membershipcontent_update");

    settings.remove("dxcc/start");

    return true;
}

bool Migration::setSelectedProfile(const QString &tablename, const QString &profileName)
{
    FCT_IDENTIFICATION;

    QSqlQuery query;

    if ( !query.prepare(QString("UPDATE %1 "
                                      "SET selected = CASE "
                                      "               WHEN profile_name = :profileName THEN 1 "
                                      "               ELSE NULL "
                                      "               END "
                                      "WHERE selected = 1 OR profile_name = :profileName2").arg(tablename)) )
    {
        qWarning() << "Cannot prepare Update statement for" << tablename;
        return false;
    }

    query.bindValue(":profileName", profileName);
    query.bindValue(":profileName2", profileName);

    if( !query.exec() )
    {
        qWarning()<< "Cannot update the profile" << tablename << profileName << query.lastError();
        return false;
    }
    return true;
}

QString Migration::fixIntlField(QSqlQuery &query, const QString &columName, const QString &columnNameIntl)
{
    FCT_IDENTIFICATION;

    QString retValue;

    QString fieldValue = query.value(columName).toString();

    if ( !fieldValue.isEmpty() )
    {
        retValue = Data::removeAccents(fieldValue);
    }
    else
    {
        retValue = Data::removeAccents(query.value(columnNameIntl).toString());
    }

    return retValue;
}

bool Migration::refreshUploadStatusTrigger()
{
    FCT_IDENTIFICATION;

    QSqlQuery query("SELECT name "
                    "FROM PRAGMA_TABLE_INFO('contacts') c "
                    "WHERE c.name NOT IN ('clublog_qso_upload_date', "
                    "                     'clublog_qso_upload_status', "
                    "                     'hrdlog_qso_upload_date', "
                    "                     'hrdlog_qso_upload_status', "
                    "                     'qrzcom_qso_upload_date', "
                    "                     'qrzcom_qso_upload_status', "
                    "                     'hamlogeu_qso_upload_date', "
                    "                     'hamlogeu_qso_upload_status', "
                    "                     'hamqth_qso_upload_date', "
                    "                     'hamqth_qso_upload_status')");

    if ( !query.exec() )
    {
        qWarning() << "Cannot get Contacts columns";
        return false;
    }

    QStringList whenClause;
    QStringList afterUpdateClause;

    while ( query.next() )
    {
        const QString &columnName = query.value(0).toString();
        afterUpdateClause << QString("\"" + columnName + "\"");
        whenClause << QString("old.\"%1\" != new.\"%2\"").arg(columnName, columnName);
    }

    QSqlQuery stmt;

    if ( !stmt.exec("DROP TRIGGER IF EXISTS update_contacts_upload_status") )
    {
        qWarning() << "Cannot drop all Update Contacts Upload Trigger";
        return false;
    }

    QStringList clublogSupportedColumns;

    for ( const QString &clublogColumn : qAsConst(ClubLog::supportedDBFields) )
        clublogSupportedColumns << QString("old.%1 != new.%2").arg(clublogColumn, clublogColumn);

    if ( !stmt.exec(QString("CREATE TRIGGER update_contacts_upload_status "
                            "AFTER UPDATE OF %1 "
                            "ON contacts "
                            "WHEN (%2) "
                            "BEGIN "
                            "   UPDATE contacts "
                            "   SET hrdlog_qso_upload_status =  CASE old.hrdlog_qso_upload_status WHEN 'Y' THEN 'M' ELSE old.hrdlog_qso_upload_status END, "
                            "       qrzcom_qso_upload_status =  CASE old.qrzcom_qso_upload_status WHEN 'Y' THEN 'M' ELSE old.qrzcom_qso_upload_status END , "
                            "       hamlogeu_qso_upload_status =  CASE old.hamlogeu_qso_upload_status WHEN 'Y' THEN 'M' ELSE old.hamlogeu_qso_upload_status END ,  "
                            "       hamqth_qso_upload_status =  CASE old.hamqth_qso_upload_status WHEN 'Y' THEN 'M' ELSE old.hamqth_qso_upload_status END ,  "
                            "       clublog_qso_upload_status =  CASE WHEN old.clublog_qso_upload_status = 'Y'  "
                            "                                              AND (%3) "
                            "                                              THEN 'M' ELSE old.clublog_qso_upload_status END "
                            "   WHERE id = new.id;"
                            "END").arg(afterUpdateClause.join(","),
                                       whenClause.join(" OR "),
                                       clublogSupportedColumns.join(" OR "))) )
    {
        qWarning().noquote() << "Cannot create Update Contacts Upload Trigger" << stmt.lastQuery();
        return false;
    }
    return true;
}
