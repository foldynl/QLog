#include <QProgressDialog>
#include <QMessageBox>
#include <QtSql>
#include <QDebug>
#include <QUuid>
#include <sqlite3.h>
#include "core/Migration.h"
#include "debug.h"
#include "data/Data.h"
#include "data/BandPlan.h"
#include "data/DxSpot.h"
#include "LogParam.h"
#include "service/clublog/ClubLog.h"
#include "service/hrdlog/HRDLog.h"
#include "core/DatabaseBackup.h"
#include "core/LogDatabase.h"

MODULE_IDENTIFICATION("qlog.core.migration");

bool DBSchemaMigration::failMigration(const QString &message)
{
    FCT_IDENTIFICATION;

    // Restoring database settings after failure must not hide the original cause.
    if ( error.isEmpty() )
        error = message;
    qCWarning(runtime) << message;
    return false;
}

/**
 * Migrate the database to the latest schema version.
 * Returns true on success.
 */
bool DBSchemaMigration::run(QString *backupSnapshot)
{
    FCT_IDENTIFICATION;

    error.clear();
    const QString filename = QSqlDatabase::database().databaseName();
    int currentVersion = getVersion();
    if ( !error.isEmpty() )
        return false;

    if ( currentVersion > latestVersion )
        return failMigration(tr("The database %1 uses format version %2, but this QLog supports up to %3. Use a newer QLog version to open it.")
                                 .arg(filename, QString::number(currentVersion), QString::number(latestVersion)));


    if (currentVersion == latestVersion) {
        qCDebug(runtime) << "Database schema already up to date";
        // temporarily added to create a trigger without calling db migration
        //refreshUploadStatusTrigger();
        return true;
    }

    // A schema upgrade always requires its own snapshot, regardless of recent backups.
    if ( !DatabaseBackup::prepare(true, backupSnapshot) )
        return failMigration(tr("Cannot create a safety copy before updating the database %1:\n%2").arg(filename, LogParam::getBackupError()));

    qCDebug(runtime) << "Starting database migration";

    QProgressDialog progress(tr("Migrating the database..."), nullptr, currentVersion, latestVersion);
    progress.setWindowTitle(tr("Database Migration"));
    progress.show();

    // Historical migrations rebuild tables and may temporarily violate current
    // foreign keys. Validate the final schema before committing its last step.
    bool result = setForeignKeysEnabled(false);
    while ( result && currentVersion < latestVersion )
    {
        if ( !migrate(currentVersion+1) )
        {
            result = false;
            break;
        }
        ++currentVersion;
        // sometimes (especially when DROP INDEX is called), it is needed to
        // reopen database. (issue occurs between DB versions 15 and 16)
        QSqlDatabase::database().close();

        if ( !QSqlDatabase::database().open() )
        {
            result = failMigration(tr("Cannot reopen the database after update %1 in %2:\n%3")
                                       .arg(QString::number(currentVersion), filename, QSqlDatabase::database().lastError().text()));
            break;
        }

        // Connection-local settings and SQL functions are lost on close/open.
        result = setForeignKeysEnabled(false);
        if ( result && !LogDatabase::instance()->createSQLFunctions() )
            result = failMigration(tr("Cannot initialize logbook functions after database update %1 in %2.")
                                       .arg(QString::number(currentVersion), filename));
        progress.setValue(currentVersion);
    }

    if ( result )
    {
        QSqlDatabase db = QSqlDatabase::database();
        if ( !db.transaction() )
        {
            result = failMigration(tr("Cannot start refreshing logbook upload rules in %1:\n%2").arg(filename, db.lastError().text()));
        }
        else
        {
            result = refreshUploadStatusTrigger();
            if ( result && !db.commit() )
                result = failMigration(tr("Cannot save the refreshed logbook upload rules in %1:\n%2").arg(filename, db.lastError().text()));
            if ( !result && !db.rollback() )
                qCCritical(runtime) << "Cannot roll back upload status trigger refresh:" << db.lastError();
        }
    }

    // Restore enforcement even after a failed migration and rollback.
    const bool foreignKeysRestored = setForeignKeysEnabled(true);
    progress.close();
    if ( !result || !foreignKeysRestored )
        return false;

    qCDebug(runtime) << "Database migration successful";

    return true;
}

bool DBSchemaMigration::setForeignKeysEnabled(bool enabled)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << enabled;

    const QSqlDatabase db = QSqlDatabase::database(QSqlDatabase::defaultConnection, false);
    if ( !db.isOpen() )
        return failMigration(tr("Cannot change database data checks in %1 because the database is closed.").arg(db.databaseName()));


    QSqlQuery query(db);
    if ( !query.exec(enabled ? "PRAGMA foreign_keys = ON" : "PRAGMA foreign_keys = OFF")
         || !query.exec("PRAGMA foreign_keys")
         || !query.next()
         || query.value(0).toInt() != int(enabled) )
    {
        const QString reason = query.lastError().isValid() ? query.lastError().text() : tr("The database did not accept the requested setting.");
        return failMigration(enabled ? tr("Cannot enable database data checks in %1:\n%2").arg(db.databaseName(), reason)
                                     : tr("Cannot disable database data checks for updating %1:\n%2").arg(db.databaseName(), reason));
    }

    return true;
}

/**
 * Returns the current user_version of the database.
 */
int DBSchemaMigration::getVersion()
{
    FCT_IDENTIFICATION;

    const QSqlDatabase database = QSqlDatabase::database();
    QSqlQuery query(database);
    if ( !query.exec("SELECT version FROM schema_versions ORDER BY version DESC LIMIT 1") )
    {
        // A new empty logbook has no version table until its first migration.
        if ( query.lastError().nativeErrorCode() == QString::number(SQLITE_ERROR) && database.tables().isEmpty() )
            return 0;
        failMigration(tr("Cannot read the database format version before updating %1:\n%2").arg(database.databaseName(), query.lastError().text()));
        return -1;
    }
    if ( !query.first() )
    {
        failMigration(query.lastError().isValid()
                          ? tr("Cannot load the stored database format version from %1:\n%2").arg(database.databaseName(), query.lastError().text())
                          : tr("No database format version is recorded for updating %1.").arg(database.databaseName()));
        return -1;
    }
    bool valid = false;
    const int i = query.value(0).toInt(&valid);
    if ( !valid || i < 0 )
    {
        failMigration(tr("The stored database format version is invalid in %1.").arg(database.databaseName()));
        return -1;
    }
    qCDebug(runtime) << i;
    return i;
}

/**
 * Changes the user_version of the database to version.
 * Returns true of the operation was successful.
 */
bool DBSchemaMigration::setVersion(int version)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << version;

    QSqlQuery query;
    if ( ! query.prepare("INSERT INTO schema_versions (version, updated) "
                  "VALUES (:version, datetime('now'))") )
    {
        return failMigration(tr("Cannot prepare recording database format version %1 in %2:\n%3")
                                 .arg(QString::number(version), QSqlDatabase::database().databaseName(), query.lastError().text()));
    }

    query.bindValue(":version", version);

    if (!query.exec()) {
        return failMigration(tr("Cannot record database format version %1 in %2:\n%3")
                                 .arg(QString::number(version), QSqlDatabase::database().databaseName(), query.lastError().text()));
    }
    else {
        return true;
    }
}

/**
 * Migrate the database to the given version.
 * Returns true if the operation was successful.
 */
bool DBSchemaMigration::migrate(int toVersion)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "migrate to" << toVersion;

    QSqlDatabase db = QSqlDatabase::database();
    if ( !db.transaction() )
        return failMigration(tr("Cannot start database update %1 in %2:\n%3")
                                 .arg(QString::number(toVersion), db.databaseName(), db.lastError().text()));

    QString migration_file = QString(":/res/sql/migration_%1.sql").arg(toVersion, 3, 10, QChar('0'));
    bool result = runSqlFile(migration_file);

    result = result && functionMigration(toVersion);

    if ( result && toVersion == latestVersion )
    {
        QSqlQuery query;
        result = query.exec("PRAGMA foreign_key_check") && !query.next() && !query.lastError().isValid();
        if ( !result )
        {
            const QString reason = query.lastError().isValid()
                                       ? query.lastError().text()
                                       : tr("Some records refer to missing data in table %1 (row %2).")
                                             .arg(query.value(0).toString(), query.value(1).toString());
            failMigration(tr("The data consistency check failed after database update %1 in %2:\n%3")
                              .arg(QString::number(toVersion), db.databaseName(), reason));
        }
    }

    if ( result )
        result = setVersion(toVersion);
    if ( result && !db.commit() )
        result = failMigration(tr("Cannot save database update %1 in %2:\n%3")
                                   .arg(QString::number(toVersion), db.databaseName(), db.lastError().text()));
    if ( result )
        return true;

    if ( !db.rollback() )
        qCCritical(runtime) << "Cannot roll back database update" << toVersion << db.lastError();
    return false;
}

bool DBSchemaMigration::runSqlFile(QString filename)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filename;

    QFile sqlFile(filename);
    if ( !sqlFile.open(QIODevice::ReadOnly | QIODevice::Text) )
        return failMigration(tr("Cannot open the database update file %1 for %2:\n%3")
                                 .arg(filename, QSqlDatabase::database().databaseName(), sqlFile.errorString()));


    QTextStream stream(&sqlFile);
    const QStringList &sqlQueries = stream.readAll().split('\n').join(" ").split(';');
    if ( sqlFile.error() != QFileDevice::NoError || stream.status() != QTextStream::Ok )
        return failMigration(tr("Cannot read the database update file %1 for %2:\n%3")
                                 .arg(filename, QSqlDatabase::database().databaseName(), sqlFile.errorString()));
    qCDebug(runtime) << sqlQueries;

    for (const QString &sqlQuery : sqlQueries)
    {
        if (sqlQuery.trimmed().isEmpty()) continue;

        qCDebug(runtime) << sqlQuery;

        QSqlQuery query;
        if (!query.exec(sqlQuery))
        {
            return failMigration(tr("Cannot apply database update %1 to %2:\n%3")
                                     .arg(filename, QSqlDatabase::database().databaseName(), query.lastError().text()));
        }
        query.finish();
    }

    return true;
}

bool DBSchemaMigration::functionMigration(int version)
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
    case 35:
        ret = removeSettings2DB();
        break;
    case 44:
        ret = refreshUploadStatusTrigger();
        break;
    default:
        ret = true;
    }

    return ret;
}

int DBSchemaMigration::tableRows(const QString &name)
{
    FCT_IDENTIFICATION;

    int i = 0;
    QSqlQuery query(QString("SELECT count(*) FROM %1").arg(name));
    i = query.first() ? query.value(0).toInt() : 0;
    qCDebug(runtime) << i;
    return i;
}

/* Fixing error when QLog stored UTF characters to non-Intl field of ADIF (contact) table */
/* The fix has two steps
 * 1) Update contact to move all non-intl to intl fields
 * 2) transform intl field to non-intl field by calloni removeAccents
 */
bool DBSchemaMigration::fixIntlFields()
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
        return failMigration(tr("Cannot prepare logbook text conversion in %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), query.lastError().text()));
    }

    if( !query.exec() )
    {
        return failMigration(tr("Cannot read text fields for the database update in %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), query.lastError().text()));
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
        return failMigration(tr("Cannot prepare converted logbook text for saving in %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), update.lastError().text()));
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
            return failMigration(tr("Cannot save converted text for contact %1 in database %2:\n%3")
                                     .arg(query.value("id").toString(), QSqlDatabase::database().databaseName(), update.lastError().text()));
        }
    }

    if ( query.lastError().isValid() )
        return failMigration(tr("Cannot finish reading logbook text while updating %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), query.lastError().text()));

    return true;
}

bool DBSchemaMigration::insertUUID()
{
    FCT_IDENTIFICATION;

    if ( !LogParam::setLogID(QUuid::createUuid().toString()) )
        return failMigration(tr("Cannot save the logbook identifier in %1 while updating its format.").arg(QSqlDatabase::database().databaseName()));
    return true;
}

bool DBSchemaMigration::fillMyDXCC()
{
    FCT_IDENTIFICATION;

    QSqlQuery query;
    QSqlQuery update;

    if ( !query.prepare( "SELECT DISTINCT station_callsign FROM contacts" ) )
    {
        return failMigration(tr("Cannot prepare reading station callsigns for updating %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), query.lastError().text()));
    }

    if( !query.exec() )
    {
        return failMigration(tr("Cannot read station callsigns while updating database %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), query.lastError().text()));
    }

    if ( !update.prepare("UPDATE contacts "
                         "SET my_dxcc = CASE WHEN my_dxcc IS NULL THEN :my_dxcc ELSE my_dxcc END, "
                         "    my_itu_zone = CASE WHEN my_itu_zone IS NULL THEN :my_itu_zone ELSE my_itu_zone END, "
                         "    my_cq_zone = CASE WHEN my_cq_zone IS NULL THEN :my_cq_zone ELSE my_cq_zone END, "
                         "    my_country = CASE WHEN my_country IS NULL THEN :my_country ELSE my_country END, "
                         "    my_country_intl = CASE WHEN my_country_intl IS NULL THEN :my_country_intl ELSE my_country_intl END "
                         "WHERE station_callsign = :station_callsign") )
    {
        return failMigration(tr("Cannot prepare converted station geography in %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), update.lastError().text()));
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
                return failMigration(tr("Cannot save converted station geography for %1 in %2:\n%3")
                                         .arg(myCallsign, QSqlDatabase::database().databaseName(), update.lastError().text()));
            }
        }
    }

    if ( query.lastError().isValid() )
        return failMigration(tr("Cannot finish reading station callsigns while updating %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), query.lastError().text()));

    return true;
}

bool DBSchemaMigration::createTriggers()
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
        return failMigration(tr("Cannot create the callsign update rule in database %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), query.lastError().text()));
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
        return failMigration(tr("Cannot create the rule for new contact callsigns in database %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), query.lastError().text()));
    }

    return true;
}

bool DBSchemaMigration::importQSLCards2DB()
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
        return failMigration(tr("Cannot prepare QSL image conversion for database %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), insert.lastError().text()));
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

bool DBSchemaMigration::fillCQITUZStationProfiles()
{
    FCT_IDENTIFICATION;

    QSqlQuery query;
    QSqlQuery update;

    if ( !query.prepare( "SELECT DISTINCT callsign FROM station_profiles" ) )
        return failMigration(tr("Cannot prepare reading station profiles while updating %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), query.lastError().text()));


    if( !query.exec() )
    {
        return failMigration(tr("Cannot read station profiles while updating %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), query.lastError().text()));
    }

    if ( !update.prepare("UPDATE station_profiles "
                         "SET ituz = :ituz, cqz = :cqz, dxcc = :dxcc, country = :country "
                         "WHERE upper(callsign) = :callsign") )
    {
        return failMigration(tr("Cannot prepare converted station zone data in %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), update.lastError().text()));
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
                return failMigration(tr("Cannot save converted station zone data in %1:\n%2")
                                         .arg(QSqlDatabase::database().databaseName(), update.lastError().text()));
            }
        }
    }

    if ( query.lastError().isValid() )
        return failMigration(tr("Cannot finish reading station profiles while updating %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), query.lastError().text()));

    return true;
}

bool DBSchemaMigration::resetConfigs()
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

bool DBSchemaMigration::profiles2DB()
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

bool DBSchemaMigration::removeSettings2DB()
{
    // all platform-independent settings are already in the DB,
    // no unusual errors occur. It's time to delete old values
    // ​​from the config.
    // Only platform-dependent settings remain in the config now.

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
    return true;
}

bool DBSchemaMigration::settings2DB()
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
        if ( settings.value("dxc/filter_mode_ft8",true).toBool() ) value.append("|" + BandPlan::MODE_GROUP_STRING_FTx);
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
        LogParam::setAlertNotificationEnabled(settings.value("alertbeep").toBool());
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

bool DBSchemaMigration::setSelectedProfile(const QString &tablename, const QString &profileName)
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

QString DBSchemaMigration::fixIntlField(const QSqlQuery &query, const QString &columName, const QString &columnNameIntl)
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

bool DBSchemaMigration::refreshUploadStatusTrigger()
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
        return failMigration(tr("Cannot inspect contact fields for updated upload rules in %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), query.lastError().text()));


    QStringList whenClause;
    QStringList afterUpdateClause;

    while ( query.next() )
    {
        const QString &columnName = query.value(0).toString();
        afterUpdateClause << QString("\"" + columnName + "\"");
        whenClause << QString("old.\"%1\" IS NOT new.\"%2\"").arg(columnName, columnName);
    }
    if ( query.lastError().isValid() )
        return failMigration(tr("Cannot finish reading contact fields for database upload rules in %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), query.lastError().text()));

    QSqlQuery stmt;

    if ( !stmt.exec("DROP TRIGGER IF EXISTS update_contacts_upload_status") )
        return failMigration(tr("Cannot remove the previous logbook upload rules in %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), stmt.lastError().text()));


    auto generateListSpecificConditions = [](const QStringList &columns)
    {
        QStringList result;
        for (const QString &col : columns)
            result << QString("old.\"%1\" IS NOT new.\"%2\"").arg(col, col);
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
        qCDebug(runtime) << stmt.lastQuery();
        return failMigration(tr("Cannot create the updated logbook upload rules in %1:\n%2")
                                 .arg(QSqlDatabase::database().databaseName(), stmt.lastError().text()));
    }
    return true;
}
