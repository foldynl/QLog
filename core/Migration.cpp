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
#include "service/clublog/ClubLog.h"
#include "service/hrdlog/HRDLog.h"
#include "logformat/AdxFormat.h"
#include "ui/DxWidget.h"

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
    else if ( currentVersion > latestVersion )
    {
        qCritical() << "Database from the future" << currentVersion;
        return false;
    }

    qCDebug(runtime) << "Backup before migration";
    backupDatabase(true);

    qCDebug(runtime) << "Starting database migration";

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

bool Migration::backupDatabase(bool force)
{
    FCT_IDENTIFICATION;

    const int backupCount = 10;
    const int backupIntervalDays = 7;

    const QDate &lastBackupDate = LogParam::getLastBackupDate();
    const QDate &now = QDate::currentDate();

    qCDebug(runtime) << "The last backup date" << lastBackupDate
                     << "Force" << force;

    if ( !force
         && lastBackupDate.isValid()
         && lastBackupDate.addDays(backupIntervalDays) > now )
    {
        qCDebug(runtime) << "Backup skipped";
        return true;
    }

    const QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    const QString todayBackupName = "qlog_backup_" + QDateTime::currentDateTime().toString("yyyyMMdd") + ".adx";
    const QString todayBackupPath = dir.filePath(todayBackupName);

    // old backup file had a timestamp YYYYMMDDHHmmSS
    // new backup file has only YYYYMMDD
    // to be able to clean new and old backup files, following regexp is needed
    const QRegularExpression regex("^qlog_backup_\\d{8}(\\d{6})?\\.adx$");

    const QFileInfoList &fileList = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    QFileInfoList filteredList;

    for ( const QFileInfo &fileInfo : fileList )
    {
        if ( regex.match(fileInfo.fileName()).hasMatch() ) // clazy:exclude=use-static-qregularexpression
        {
            filteredList.append(fileInfo);
        }
    }

    qCDebug(runtime) << filteredList;

    // does today backup exists ?
    if ( !force )
    {
        for ( const QFileInfo &fileInfo : filteredList )
        {
            if ( fileInfo.fileName() == todayBackupName )
            {
                qCDebug(runtime) << "Backup for today already exists: " << todayBackupName;
                return true;
            }
        }
    }

    /* Keep the minimum number of backups */
    /* If a number of backups is greater than backupCount, remove oldest files */
    if ( filteredList.size() >= backupCount )
    {
        std::sort(filteredList.begin(), filteredList.end(), [](const QFileInfo &a, const QFileInfo &b) {
            return a.fileName() < b.fileName();
        });

        // remove old files
        while ( filteredList.size() > backupCount )
        {
            QFileInfo oldestFile = filteredList.takeFirst();
            const QString &filepath = oldestFile.absoluteFilePath();
            if ( QFile::remove(filepath) )
                qCDebug(runtime) << "Removing old backup file: " << filepath;
            else
                qWarning() << "Failed to remove old backup file: " << filepath;
        }
    }

    /* Make a backup file */
    QFile backupFile(todayBackupPath);

    if ( !backupFile.open(QFile::ReadWrite | QIODevice::Text) )
    {
        qWarning() << "Cannot open backup file " << todayBackupPath << " for writing";
        return false;
    }

    qCDebug(runtime) << "Exporting a Database backup to " << todayBackupPath;

    QTextStream stream(&backupFile);
    AdxFormat adx(stream);

    adx.runExport();
    stream.flush();
    backupFile.close();

    LogParam::setLastBackupDate(now);

    qCDebug(runtime) << "Database backup finished";
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

    const QStringList &sqlQueries = QTextStream(&sqlFile).readAll().split('\n').join(" ").split(';');
    qCDebug(runtime) << sqlQueries;

    for (const QString &sqlQuery : sqlQueries)
    {
        if (sqlQuery.trimmed().isEmpty()) continue;

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
    case 34:
        ret = settings2DB();
        break;
    default:
        ret = true;
    }

    return ret;
}

int Migration::tableRows(const QString &name)
{
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

    updateExternalResourceProgress(progress, downloader, LOVDownloader::CTY, "(1/8)");
    updateExternalResourceProgress(progress, downloader, LOVDownloader::CLUBLOGCTY, "2/8");
    updateExternalResourceProgress(progress, downloader, LOVDownloader::SATLIST, "(3/8)");
    updateExternalResourceProgress(progress, downloader, LOVDownloader::SOTASUMMITS, "(4/8)");
    updateExternalResourceProgress(progress, downloader, LOVDownloader::WWFFDIRECTORY, "(5/8)");
    updateExternalResourceProgress(progress, downloader, LOVDownloader::IOTALIST, "(6/8)");
    updateExternalResourceProgress(progress, downloader, LOVDownloader::POTADIRECTORY, "(7/8)");
    updateExternalResourceProgress(progress, downloader, LOVDownloader::MEMBERSHIPCONTENTLIST, "(8/8)");

    return true;
}

void Migration::updateExternalResourceProgress(QProgressDialog& progress,
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
    case LOVDownloader::SourceType::CLUBLOGCTY:
        stringInfo = tr("Clublog CTY.XML");
        break;
    default:
        stringInfo = tr("List of Values");
    }

    progress.setLabelText(tr("Updating ") + stringInfo + " " + counter +" ...");
    progress.setMinimum(0);

    progress.show();

    downloader.update(sourceType);

    if ( progress.wasCanceled() )
        qCDebug(runtime) << "Update was canceled";
    else
    {
        if ( !progress.exec() )
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 stringInfo + tr(" Update Failed"));
    }
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

    return LogParam::setLogID(QUuid::createUuid().toString());
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
    const QFileInfoList &file_list = dir.entryInfoList(QStringList("????????_*_*_qsl_*.*"),
                                                QDir::Files,
                                                QDir::Name);
    if ( !insert.prepare("INSERT INTO contacts_qsl_cards (contactid, source, name, data) "
                         " VALUES (:contactid, 0, :name, :data)" ) )
    {
        qWarning()<< " Cannot prepare a migration script - importQSLCards2DB 2" << insert.lastError();
        return false;
    }

    for ( auto &file : file_list )
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

void removeSettings2DB()
{
    // TODO call it later when we will sure that everything is working
    QSettings settings;

    settings.remove("qrzcom/username");
    settings.remove("qrzcom/usernameapi");
    settings.remove("clublog/email");
    settings.remove("clublog/callsign");
    settings.remove("clublog/upload_immediately");
    settings.remove("eqsl/username");
    settings.remove("eqsl/last_update");
    settings.remove("eqsl/last_qsoqsl");
    settings.remove("eqsl/last_QTHProfile");
    settings.remove("hamqth/username");
    settings.remove("hrdlog/callsign");
    settings.remove("hrdlog/onair");
    settings.remove("kst/username");
    settings.remove("lotw/username");
    settings.remove("lotw/tqsl");
    settings.remove("lotw/last_update");
    settings.remove("lotw/last_qsoqsl");
    settings.remove("callbook/primary");
    settings.remove("callbook/secondary");
    settings.remove("callbook/weblookupurl");
    settings.remove("network/notification/qso/adi_addrs");
    settings.remove("network/notification/dxspot/addrs");
    settings.remove("network/notification/wsjtx/cqspot/addrs");
    settings.remove("network/notification/alerts/spot/addrs");
    settings.remove("network/notification/rig/state/addrs");
    settings.remove("network/wsjtx_port");
    settings.remove("network/wsjtx_forward");
    settings.remove("network/wsjtx_multicast");
    settings.remove("network/wsjtx_multicast_addr");
    settings.remove("network/wsjtx_multicast_ttl");
    settings.remove("memberlists/enabled");
    settings.remove("alert/alert_aging");
    settings.remove("alert/state");
    settings.remove("cwconsole/sendWord");
    settings.remove("chat/last_selected_room");
    settings.remove("newcontact/frequency");
    settings.remove("newcontact/mode");
    settings.remove("newcontact/submode");
    settings.remove("newcontact/power");
    settings.remove("newcontact/tabindex");
    settings.remove("newcontact/qslsent");
    settings.remove("newcontact/lotwqslsent");
    settings.remove("newcontact/eqslqslsent");
    settings.remove("newcontact/qslsentvia");
    settings.remove("newcontact/propmode");
    settings.remove("newcontact/tabsexpanded");
    settings.remove("newcontact/satname");
    settings.beginGroup("onlinemap");
    settings.remove("");
    settings.endGroup();
    settings.beginGroup("qsodetail");
    settings.remove("");
    settings.endGroup();
    settings.remove("wsjtx/filter_dxcc_status");
    settings.remove("wsjtx/filter_cont_regexp");
    settings.remove("wsjtx/filter_distance");
    settings.remove("wsjtx/filter_snr");
    settings.remove("wsjtx/filter_dx_member_list");
    settings.remove("wsjtx/state");
    settings.remove("dxc/filter_dxcc_status");
    settings.remove("dxc/filter_cont_regexp");
    settings.remove("dxc/filter_spotter_cont_regexp");
    settings.remove("dxc/filter_deduplication");
    settings.remove("dxc/filter_duplicationtime");
    settings.remove("dxc/filter_deduplicationfreq");
    settings.remove("dxc/filter_dx_member_list");
    settings.remove("dxc/autoconnect");
    settings.remove("dxc/keepqsos");
    settings.remove("dxc/dxtablestate");
    settings.remove("dxc/wcytablestate");
    settings.remove("dxc/wwvtablestate");
    settings.remove("dxc/toalltablestate");
    settings.remove("dxc/consolefontsize");
    settings.remove("dxc/servers");
    settings.remove("dxc/last_server");
    settings.remove("dxc/filter_mode_cw");
    settings.remove("dxc/filter_mode_phone");
    settings.remove("dxc/filter_mode_digital");
    settings.remove("dxc/filter_mode_ft8");
    settings.remove("equipment/rigconnected");
    settings.remove("equipment/rotconnected");
    settings.remove("equipment/cwkeyconnected");
    settings.remove("equipment/keepoptions");
    settings.remove("export/min");
    settings.remove("export/qsl");
    settings.remove("export/c1");
    settings.remove("export/c2");
    settings.remove("export/c3");
    settings.remove("logbook/state");
    settings.remove("logbook/filters/band");
    settings.remove("logbook/filters/mode");
    settings.remove("logbook/filters/country");
    settings.remove("logbook/filters/user");
    settings.remove("logbook/filters/member");
    settings.remove("alertbeep");
    settings.remove("darkmode");
    settings.remove("geometry");
    settings.remove("windowState");
    settings.remove("bandmapwidgets");
}

bool Migration::settings2DB()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    if (settings.contains("qrzcom/username"))
        LogParam::setQRZCOMCallbookUsername(settings.value("qrzcom/username").toString().trimmed());

    if (settings.contains("clublog/email"))
        LogParam::setClublogLogbookReqEmail(settings.value("clublog/email").toString().trimmed());

    if (settings.contains("clublog/upload_immediately"))
        LogParam::setClublogUploadImmediatelyEnabled(settings.value("clublog/upload_immediately").toBool());

    if (settings.contains("eqsl/username"))
        LogParam::setEQSLLogbookUsername(settings.value("eqsl/username").toString());
    if (settings.contains("eqsl/last_update"))
        LogParam::setDownloadQSLServiceLastDate("eqsl", settings.value("eqsl/last_update").toDate());
    if (settings.contains("eqsl/last_qsoqsl"))
        LogParam::setDownloadQSLServiceLastQSOQSL("eqsl", settings.value("eqsl/last_qsoqsl").toBool());
    if (settings.contains("eqsl/last_QTHProfile"))
    {
        LogParam::setDownloadQSLeQSLLastProfile(settings.value("eqsl/last_QTHProfile").toString());
        LogParam::setUploadeqslQTHProfile(settings.value("eqsl/last_QTHProfile").toString());
    }

    if (settings.contains("hamqth/username"))
        LogParam::setHamQTHCallbookUsername(settings.value("hamqth/username").toString());

    if (settings.contains("hrdlog/callsign"))
        LogParam::setHRDLogLogbookReqCallsign(settings.value("hrdlog/callsign").toString());
    if (settings.contains("hrdlog/onair"))
        LogParam::setHRDLogOnAir(settings.value("hrdlog/onair").toBool());

    if (settings.contains("kst/username"))
        LogParam::setKSTChatUsername(settings.value("kst/username").toString());

    if (settings.contains("lotw/username"))
        LogParam::setLoTWCallbookUsername(settings.value("lotw/username").toString());
    if (settings.contains("lotw/tqsl"))
        LogParam::setLoTWTQSLPath(settings.value("lotw/tqsl").toString());
    if (settings.contains("lotw/last_update"))
        LogParam::setDownloadQSLServiceLastDate("lotw", settings.value("lotw/last_update").toDate());
    if (settings.contains("lotw/last_qsoqsl"))
        LogParam::setDownloadQSLServiceLastQSOQSL("lotw", settings.value("lotw/last_qsoqsl").toBool());

    if (settings.contains("callbook/primary"))
        LogParam::setPrimaryCallbook(settings.value("callbook/primary").toString());
    if (settings.contains("callbook/secondary"))
        LogParam::setSecondaryCallbook(settings.value("callbook/secondary").toString());
    if (settings.contains("callbook/weblookupurl"))
        LogParam::setCallbookWebLookupURL(settings.value("callbook/weblookupurl").toString());

    if (settings.contains("network/notification/qso/adi_addrs"))
        LogParam::setNetworkNotifLogQSOAddrs(settings.value("network/notification/qso/adi_addrs").toString());
    if (settings.contains("network/notification/dxspot/addrs"))
        LogParam::setNetworkNotifDXCSpotAddrs(settings.value("network/notification/dxspot/addrs").toString());
    if (settings.contains("network/notification/wsjtx/cqspot/addrs"))
        LogParam::setNetworkNotifWSJTXCQSpotAddrs(settings.value("network/notification/wsjtx/cqspot/addrs").toString());
    if (settings.contains("network/notification/alerts/spot/addrs"))
        LogParam::setNetworkNotifAlertsSpotAddrs(settings.value("network/notification/alerts/spot/addrs").toString());
    if (settings.contains("network/notification/rig/state/addrs"))
        LogParam::setNetworkNotifRigStateAddrs(settings.value("network/notification/rig/state/addrs").toString());

    if (settings.contains("network/wsjtx_port"))
        LogParam::setNetworkNotifRigStateAddrs(settings.value("network/wsjtx_port").toInt());
    if (settings.contains("network/wsjtx_forward"))
        LogParam::setNetworkWsjtxForwardAddrs(settings.value("network/wsjtx_forward").toString());
    if (settings.contains("network/wsjtx_multicast"))
        LogParam::setNetworkWsjtxListenerJoinMulticast(settings.value("network/wsjtx_multicast").toBool());
    if (settings.contains("network/wsjtx_multicast_addr"))
        LogParam::setNetworkWsjtxListenerMulticastAddr(settings.value("network/wsjtx_multicast_addr").toString());
    if (settings.contains("network/wsjtx_multicast_ttl"))
        LogParam::setNetworkWsjtxListenerMulticastTTL(settings.value("network/wsjtx_multicast_ttl").toInt());

    if (settings.contains("memberlists/enabled"))
        LogParam::setEnabledMemberlists(settings.value("memberlists/enabled").toStringList());

    if (settings.contains("alert/alert_aging"))
        LogParam::setAlertAging(settings.value("alert/alert_aging").toInt());
    if (settings.contains("alert/state"))
        LogParam::setAlertWidgetState(settings.value("alert/state").toByteArray());

    if (settings.contains("cwconsole/sendWord"))
        LogParam::setCWConsoleSendWord(settings.value("cwconsole/sendWord", false).toBool());

    if (settings.contains("chat/last_selected_room"))
        LogParam::setChatSelectedRoom(settings.value("chat/last_selected_room", 0).toInt());

    if (settings.contains("newcontact/frequency"))
        LogParam::setNewContactFreq(settings.value("newcontact/frequency").toDouble());
    if (settings.contains("newcontact/mode"))
        LogParam::setNewContactMode(settings.value("newcontact/mode").toString());
    if (settings.contains("newcontact/submode"))
        LogParam::setNewContactSubMode(settings.value("newcontact/submode").toString());
    if (settings.contains("newcontact/power"))
        LogParam::setNewContactPower(settings.value("newcontact/power").toDouble());
    if (settings.contains("newcontact/tabindex"))
        LogParam::setNewContactTabIndex(settings.value("newcontact/tabindex", 0).toInt());
    if (settings.contains("newcontact/qslsent"))
        LogParam::setNewContactQSLSent(settings.value("newcontact/qslsent").toString());
    if (settings.contains("newcontact/lotwqslsent"))
        LogParam::setNewContactLoTWQSLSent(settings.value("newcontact/lotwqslsent").toString());
    if (settings.contains("newcontact/eqslqslsent"))
        LogParam::setNewContactEQSLQSLSent(settings.value("newcontact/eqslqslsent").toString());
    if (settings.contains("newcontact/qslsentvia"))
        LogParam::setNewContactQSLVia(settings.value("newcontact/qslsentvia").toString());
    if (settings.contains("newcontact/propmode"))
        LogParam::setNewContactPropMode(settings.value("newcontact/propmode").toString());
    if (settings.contains("newcontact/tabsexpanded"))
        LogParam::setNewContactTabsExpanded(settings.value("newcontact/tabsexpanded").toBool());
    if (settings.contains("newcontact/satname"))
        LogParam::setNewContactSatName(settings.value("newcontact/satname").toString());

    settings.beginGroup("onlinemap/layerstate");
    const QStringList &onlineLayerKeys = settings.allKeys();
    for ( const QString &key : onlineLayerKeys)
        LogParam::setMapLayerState("onlinemap", key, settings.value(key).toBool());
    settings.endGroup();

    settings.beginGroup("qsodetail/layerstate");
    const QStringList &qsoDetailLayerKeys = settings.allKeys();
    for ( const QString &key : qsoDetailLayerKeys)
        LogParam::setMapLayerState("qsodetail", key, settings.value(key).toBool());
    settings.endGroup();

    if (settings.contains("wsjtx/filter_dxcc_status"))
        LogParam::setWsjtxFilterDxccStatus(settings.value("wsjtx/filter_dxcc_status").toUInt());
    if (settings.contains("wsjtx/filter_cont_regexp"))
        LogParam::setWsjtxFilterContRE(settings.value("wsjtx/filter_cont_regexp").toString());
    if (settings.contains("wsjtx/filter_distance"))
        LogParam::setWsjtxFilterDistance(settings.value("wsjtx/filter_distance").toInt());
    if (settings.contains("wsjtx/filter_snr"))
        LogParam::setWsjtxFilterDistance(settings.value("wsjtx/filter_snr").toInt());
    if (settings.contains("wsjtx/filter_dx_member_list"))
        LogParam::setWsjtxMemberlists(settings.value("wsjtx/filter_dx_member_list").toStringList());
    if (settings.contains("wsjtx/state"))
        LogParam::setWsjtxWidgetState(settings.value("wsjtx/state").toByteArray());

    if (settings.contains("dxc/filter_dxcc_status"))
        LogParam::setDXCFilterDxccStatus(settings.value("dxc/filter_dxcc_status").toUInt());
    if (settings.contains("dxc/filter_cont_regexp"))
        LogParam::setDXCFilterContRE(settings.value("dxc/filter_cont_regexp").toString());
    if (settings.contains("dxc/filter_spotter_cont_regexp"))
        LogParam::setDXCFilterSpotterContRE(settings.value("dxc/filter_spotter_cont_regexp").toString());
    if (settings.contains("dxc/filter_deduplication"))
        LogParam::setDXCFilterDedup(settings.value("dxc/filter_deduplication").toBool());
    if (settings.contains("dxc/filter_duplicationtime"))
        LogParam::setDXCFilterDedupTime(settings.value("dxc/filter_duplicationtime").toInt());
    if (settings.contains("dxc/filter_deduplicationfreq"))
        LogParam::setDXCFilterDedupFreq(settings.value("dxc/filter_deduplicationfreq", DEDUPLICATION_FREQ_TOLERANCE).toInt());
    if (settings.contains("dxc/filter_dx_member_list"))
        LogParam::setDXCFilterMemberlists(settings.value("dxc/filter_dx_member_list").toStringList());
    if (settings.contains("dxc/autoconnect"))
        LogParam::setDXCAutoconnectServer(settings.value("dxc/autoconnect").toBool());
    if (settings.contains("dxc/keepqsos"))
        LogParam::setDXCKeepQSOs(settings.value("dxc/keepqsos").toBool());
    if (settings.contains("dxc/dxtablestate"))
        LogParam::setDXCDXTableState(settings.value("dxc/dxtablestate").toByteArray());
    if (settings.contains("dxc/wcytablestate"))
        LogParam::setDXCWCYTableState(settings.value("dxc/wcytablestate").toByteArray());
    if (settings.contains("dxc/wwvtablestate"))
        LogParam::setDXCWWVTableState(settings.value("dxc/wwvtablestate").toByteArray());
    if (settings.contains("dxc/toalltablestate"))
        LogParam::setDXCTOALLTableState(settings.value("dxc/toalltablestate").toByteArray());
    if (settings.contains("dxc/consolefontsize"))
        LogParam::setDXCConsoleFontSize(settings.value("dxc/consolefontsize").toInt());
    if (settings.contains("dxc/servers"))
        LogParam::setDXCServerlist(settings.value("dxc/servers").toStringList());
    if (settings.contains("dxc/last_server"))
        LogParam::setDXCLastServer(settings.value("dxc/last_server").toString());

    if (settings.contains("dxc/filter_mode_cw")
        || settings.contains("dxc/filter_mode_phone")
        || settings.contains("dxc/filter_mode_digital")
        || settings.contains("dxc/filter_mode_ft8") )
    {
        QString value("NOTHING");
        if ( settings.value("dxc/filter_mode_cw",true).toBool() ) value.append("|" + BandPlan::MODE_GROUP_STRING_CW);
        if ( settings.value("dxc/filter_mode_phone",true).toBool() ) value.append("|" + BandPlan::MODE_GROUP_STRING_PHONE);
        if ( settings.value("dxc/filter_mode_digital",true).toBool() ) value.append("|" + BandPlan::MODE_GROUP_STRING_DIGITAL);
        if ( settings.value("dxc/filter_mode_ft8",true).toBool() ) value.append("|" + BandPlan::MODE_GROUP_STRING_FT8);
        LogParam::setDXCFilterModeRE(value);
    }

    const QList<Band> &bands = BandPlan::bandsList(false, true);
    QStringList excludedBandFilter;
    for ( const Band &band : bands )
    {
        if ( !settings.value("dxc/filter_band_" + band.name,true).toBool())
            excludedBandFilter << band.name;
    }
    if ( !excludedBandFilter.isEmpty() )
        LogParam::setDXCExcludedBands(excludedBandFilter);

    if (settings.contains("export/min"))
        LogParam::setExportColumnSet("min", settings.value("export/min").value<QSet<int>>());
    if (settings.contains("export/sql"))
        LogParam::setExportColumnSet("sql", settings.value("export/sql").value<QSet<int>>());
    if (settings.contains("export/c1"))
        LogParam::setExportColumnSet("c1", settings.value("export/c1").value<QSet<int>>());
    if (settings.contains("export/c2"))
        LogParam::setExportColumnSet("c2", settings.value("export/c2").value<QSet<int>>());
    if (settings.contains("export/c3"))
        LogParam::setExportColumnSet("c3", settings.value("export/c3").value<QSet<int>>());

    if (settings.contains("logbook/state"))
        LogParam::setLogbookState(settings.value("logbook/state").toByteArray());
    if (settings.contains("logbook/filters/band"))
        LogParam::setLogbookFilterBand(settings.value("logbook/filters/band").toString());
    if (settings.contains("logbook/filters/mode"))
        LogParam::setLogbookFilterMode(settings.value("logbook/filters/mode").toString());
    if (settings.contains("logbook/filters/country"))
        LogParam::setLogbookFilterCountry(settings.value("logbook/filters/country").toString());
    if (settings.contains("logbook/filters/user"))
        LogParam::setLogbookFilterUserFilter(settings.value("logbook/filters/user").toString());
    if (settings.contains("logbook/filters/member"))
        LogParam::setLogbookFilterClub(settings.value("logbook/filters/member").toString());

    if (settings.contains("alertbeep"))
        LogParam::setMainWindowAlertBeep(settings.value("alertbeep").toBool());
    if (settings.contains("darkmode"))
        LogParam::setMainWindowDarkMode(settings.value("darkmode").toBool());
    if (settings.contains("geometry"))
        LogParam::setMainWindowGeometry(settings.value("geometry").toByteArray());
    if (settings.contains("windowState"))
        LogParam::setMainWindowState(settings.value("windowState").toByteArray());
    if (settings.contains("bandmapwidgets"))
        LogParam::setMainWindowBandmapWidgets(settings.value("bandmapwidgets").toString());

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

    auto generateListSpecificConditions = [](const QStringList &columns)
    {
        QStringList result;
        for (const QString &col : columns)
            result << QString("old.\"%1\" != new.\"%2\"").arg(col, col);
        return result.join(" OR ");
    };

    if ( !stmt.exec(QString("CREATE TRIGGER update_contacts_upload_status "
                            "AFTER UPDATE OF %1 "
                            "ON contacts "
                            "WHEN (%2) "
                            "BEGIN "
                            "   UPDATE contacts "
                            "   SET hrdlog_qso_upload_status =   CASE WHEN old.hrdlog_qso_upload_status = 'Y' AND (%3) THEN 'M' ELSE old.hrdlog_qso_upload_status END, "
                            "       qrzcom_qso_upload_status =   CASE WHEN old.qrzcom_qso_upload_status = 'Y' THEN 'M' ELSE old.qrzcom_qso_upload_status END , "
                            "       hamlogeu_qso_upload_status = CASE WHEN old.hamlogeu_qso_upload_status = 'Y' THEN 'M' ELSE old.hamlogeu_qso_upload_status END ,  "
                            "       hamqth_qso_upload_status =   CASE WHEN old.hamqth_qso_upload_status = 'Y' THEN 'M' ELSE old.hamqth_qso_upload_status END ,  "
                            "       clublog_qso_upload_status =  CASE WHEN old.clublog_qso_upload_status = 'Y' AND (%4) THEN 'M' ELSE old.clublog_qso_upload_status END "
                            "   WHERE id = new.id;"
                            "   UPDATE contacts_autovalue "
                            "   SET wavelog_qso_upload_status = 'M' "
                            "   WHERE contactid = new.id AND wavelog_qso_upload_status = 'Y'; "
                            "END").arg(afterUpdateClause.join(","),
                                       whenClause.join(" OR "),
                                       generateListSpecificConditions(HRDLogUploader::uploadedFields),
                                       generateListSpecificConditions(ClubLogUploader::uploadedFields))) )
    {
        qWarning().noquote() << "Cannot create Update Contacts Upload Trigger" << stmt.lastQuery();
        return false;
    }
    return true;
}
