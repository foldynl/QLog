#include <QSqlDatabase>
#include <QSqlDriver>
#include <sqlite3.h>
#include <QSqlError>
#include <QSqlQuery>
#include <QCoreApplication>
#include <QUuid>

#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>

#include "LogDatabase.h"
#include "core/debug.h"
#include "core/Migration.h"
#include "core/DatabaseBackup.h"
#include "core/LogParam.h"
#include "core/CredentialStore.h"
#include "core/PlatformParameterManager.h"
#include "core/LOVDatabase.h"

MODULE_IDENTIFICATION("qlog.core.logdatabase");

QString LogDatabase::PLATFORM_WINDOWS = "Windows";
QString LogDatabase::PLATFORM_MACOS = "MacOS";
QString LogDatabase::PLATFORM_LINUX = "Linux";
QString LogDatabase::PLATFORM_LINUXFLATPAK = "LinuxFlatpak";

const QString LogDatabase::DATABASE_FILENAME = "qlog.db";
const QString LogDatabase::IMPORT_DATABASE_FILENAME = DATABASE_FILENAME + ".pending";
const QString LogDatabase::IMPORT_RECORD_FILENAME = IMPORT_DATABASE_FILENAME + ".params";
const QString LogDatabase::IMPORT_BACKUP_FILENAME = DATABASE_FILENAME + ".preimport";
const QString LogDatabase::SQLITE_WAL_SUFFIX = "-wal";
const QString LogDatabase::SQLITE_SHM_SUFFIX = "-shm";

const QStringList LogDatabase::IMPORT_PHASE_NAMES =
{
    "PREPARED", "READY", "PUBLISHED", "CREDENTIALS_RESTORED", "ABORTED", "CREDENTIALS_SKIPPED"
};

QDir LogDatabase::dbDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
}

QString LogDatabase::dbFilename()
{
    FCT_IDENTIFICATION;
    return dbDirectory().filePath(DATABASE_FILENAME);
}

QString LogDatabase::currentPlatformId()
{
#if defined(Q_OS_WIN)
    return LogDatabase::PLATFORM_WINDOWS;
#elif defined(Q_OS_MACOS)
    return LogDatabase::PLATFORM_MACOS;
#elif defined(QLOG_FLATPAK)
    return LogDatabase::PLATFORM_LINUXFLATPAK;
#else
    return LogDatabase::PLATFORM_LINUX;
#endif
}

LogDatabase::LogDatabase()
{
    FCT_IDENTIFICATION;
}

QString LogDatabase::pendingImportError() const
{
    FCT_IDENTIFICATION;
    return importError;
}

bool LogDatabase::createSQLFunctions()
{
    FCT_IDENTIFICATION;

    QVariant v = QSqlDatabase::database().driver()->handle();

    if ( !v.isValid()
         || qstrcmp(v.typeName(), "sqlite3*") != 0 )
    {
        qCritical() << "Cannot get SQLite driver handle";
        return false;
    }

    sqlite3 *db_handle = *static_cast<sqlite3 **>(v.data());
    if ( db_handle == 0 )
    {
        qCritical() << "Cannot define new SQLite functions";
        return false;
    }

    sqlite3_initialize();
    const int functionResult = sqlite3_create_function(db_handle,
                            "translate_to_locale",
                            1,
                            SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                            nullptr,
                            [](sqlite3_context *ctx, int argc, sqlite3_value **argv) {
                                if ( argc != 1 )
                                {
                                    sqlite3_result_error(ctx, "Invalid arguments", -1);
                                    return;
                                }

                                switch ( sqlite3_value_type(argv[0]) )
                                {
                                case SQLITE_TEXT:
                                {
                                    const char *text = reinterpret_cast<const char*>(sqlite3_value_text(argv[0]));
                                    const QString &translatedText = QCoreApplication::translate("DBStrings", text);
                                    sqlite3_result_text(ctx, translatedText.toUtf8().constData(), -1, SQLITE_TRANSIENT);
                                }
                                    break;
                                case SQLITE_NULL:
                                    sqlite3_result_null(ctx);
                                    break;
                                case SQLITE_INTEGER:
                                    sqlite3_result_int(ctx, sqlite3_value_int(argv[0]));
                                    break;
                                case SQLITE_FLOAT:
                                    sqlite3_result_double(ctx, sqlite3_value_double(argv[0]));
                                    break;
                                default:
                                    sqlite3_result_error(ctx, "Invalid arguments", -1);
                                }
                            }, nullptr, nullptr);
    if ( functionResult != SQLITE_OK )
    {
        qCWarning(runtime) << "Cannot register SQLite translation function:"
                           << functionResult << sqlite3_errmsg(db_handle);
        return false;
    }

    const int collationResult = sqlite3_create_collation(db_handle,
                             "LOCALEAWARE",
                             SQLITE_UTF16,
                             nullptr,
                             [](void *, int ll, const void * l, int rl, const void * r) {
                                const QString &left = QString::fromUtf16(reinterpret_cast<const char16_t *>(l), ll/2);
                                const QString &right = QString::fromUtf16(reinterpret_cast<const char16_t *>(r), rl/2);
                                return QString::localeAwareCompare(left, right); // controlled by LC_COLLATE
                             });
    if ( collationResult != SQLITE_OK )
    {
        qCWarning(runtime) << "Cannot register SQLite locale collation:"
                           << collationResult << sqlite3_errmsg(db_handle);
        return false;
    }

    return true;
}

bool LogDatabase::atomicCopy(const QString &filename, QString *error)
{
    FCT_IDENTIFICATION;
    if ( error )
        error->clear();

    QSqlDatabase db = QSqlDatabase::database();

    if ( !db.isOpen() )
    {
        qWarning() << "Database is not opened";
        if ( error )
            *error = QObject::tr("Cannot copy the logbook database %1 because it is not open.")
                         .arg(db.databaseName().isEmpty() ? dbFilename() : db.databaseName());
        return false;
    }

    // Validate filename — must be a plain filename, no path separators
    if ( filename.isEmpty() )
    {
        qWarning() << "Invalid filename:" << filename;
        if ( error )
            *error = QObject::tr("No file name was provided for the database copy.");
        return false;
    }

    QVariant v = db.driver()->handle();
    if ( !v.isValid() || qstrcmp(v.typeName(), "sqlite3*") != 0)
    {
        qWarning() << "Cannot get Database Driver Handler";
        if ( error )
            *error = QObject::tr("Cannot access the logbook database %1 for copying. The database driver does not support this operation.")
                         .arg(db.databaseName());
        return false;
    }

    sqlite3 *srcHandle = *static_cast<sqlite3 **>(v.data());
    sqlite3 *dstHandle = nullptr;
    sqlite3_backup *backup = nullptr;
    bool ok = false;
    int stepSize = 5000;

    const QString newDBFilename = dbDirectory().filePath(filename);

    if (sqlite3_open_v2(newDBFilename.toUtf8().constData(), &dstHandle,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
    {
        qWarning() << "Cannot open a new database file:" << sqlite3_errmsg(dstHandle);
        if ( error )
            *error = QObject::tr("Cannot open the destination for a database copy %1:\n%2")
                         .arg(newDBFilename, QString::fromUtf8(sqlite3_errmsg(dstHandle)));
        goto cleanup;
    }

    backup = sqlite3_backup_init(dstHandle, "main", srcHandle, "main");
    if ( !backup )
    {
        qWarning() << "Cannot Init a backup" << sqlite3_errmsg(dstHandle);
        if ( error )
            *error = QObject::tr("Cannot start copying the logbook database to %1:\n%2")
                         .arg(newDBFilename, QString::fromUtf8(sqlite3_errmsg(dstHandle)));
        goto cleanup;
    }

    while (true)
    {
        int rc = sqlite3_backup_step(backup, stepSize);

        int total = sqlite3_backup_pagecount(backup);
        int remaining = sqlite3_backup_remaining(backup);

        int done = total - remaining;
        int percent = total > 0 ? (done * 100 / total) : 0;

        qCDebug(runtime) << percent;

        if (rc == SQLITE_DONE)
        {
            ok = true;
            break;
        }
        else if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED)
        {
            sqlite3_sleep(50);
            continue;
        }
        else if (rc == SQLITE_OK)
        {
            continue;
        }
        else
        {
            qWarning() << "Backup Error" << sqlite3_errmsg(dstHandle);
            if ( error )
                *error = QObject::tr("Cannot copy the logbook data to %1:\n%2")
                             .arg(newDBFilename, QString::fromUtf8(sqlite3_errmsg(dstHandle)));
            break;
        }
    }

    // sqlite3_backup_finish always releases the backup handle,
    // even on error — never call it twice
    if ( sqlite3_backup_finish(backup) != SQLITE_OK )
    {
        qWarning() << "Cannot finalize the database copy" << sqlite3_errmsg(dstHandle);
        if ( error && error->isEmpty() )
            *error = QObject::tr("Cannot finish the logbook database copy %1:\n%2")
                         .arg(newDBFilename, QString::fromUtf8(sqlite3_errmsg(dstHandle)));
        ok = false;
    }
    backup = nullptr;

cleanup:
    if ( backup )
        sqlite3_backup_finish(backup);

    if ( dstHandle )
        sqlite3_close(dstHandle);

    if ( !ok )
        QFile::remove(newDBFilename);

    return ok;
}

bool LogDatabase::openDatabase(const QString &filename)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filename;

    if ( QSqlDatabase::contains() )
    {
        QSqlDatabase::database(QSqlDatabase::defaultConnection, false).close();
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }

    LogParam::clearCache();

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(filename);
    db.setConnectOptions("QSQLITE_ENABLE_REGEXP");

    if ( !db.open() )
    {
        qCritical() << db.lastError();
        return failImport(QObject::tr("Cannot open the logbook database %1:\n%2").arg(filename, db.lastError().text()));
    }

    QSqlQuery query;
    if ( !query.exec("PRAGMA foreign_keys = ON") )
    {
        qCritical() << "Cannot set PRAGMA foreign_keys";
        return failImport(QObject::tr("Cannot enable logbook data checks in %1:\n%2").arg(filename, query.lastError().text()));
    }

    if ( !query.exec("PRAGMA journal_mode = WAL") )
    {
        qCritical() << "Cannot set PRAGMA journal_mode";
        return failImport(QObject::tr("Cannot enable safe database writes in %1:\n%2").arg(filename, query.lastError().text()));
    }

    while ( query.next() )
    {
        QString pragma = query.value(0).toString();
        qCDebug(runtime) << "Pragma result:" << pragma;
    }

    if ( !createSQLFunctions() )
        return failImport(QObject::tr("Cannot initialize the logbook database functions in %1.").arg(filename));
    return true;
}

bool LogDatabase::schemaVersionUpgrade(QString *backupSnapshot)
{
    FCT_IDENTIFICATION;

    DBSchemaMigration m;
    if ( !m.run(backupSnapshot) )
        return failImport(m.errorString());
    return true;
}

DatabaseInfo LogDatabase::inspectDatabase(const QString &filename)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filename;

    DatabaseInfo info;
    const QString connectionName = QStringLiteral("InspectDB");

    // Use do-while(false) pattern to ensure removeDatabase is called after db/query are out of scope
    do {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(filename);
        db.setConnectOptions("QSQLITE_OPEN_READONLY");

        if ( !db.open() )
        {
            info.errorMessage = QObject::tr("Cannot open the database for unpacking inspection %1:\n%2").arg(filename, db.lastError().text());
            qCWarning(runtime) << "Cannot open database for inspection:" << info.errorMessage;
            break;
        }

        // Check if it's a valid QLog database by looking for schema_versions table
        QSqlQuery query(db);
        if ( !query.exec("SELECT version FROM schema_versions ORDER BY version DESC LIMIT 1")
             || !query.first() )
        {
            qCWarning(runtime) << "Cannot read database schema version:" << query.lastError();
            info.errorMessage = query.lastError().isValid()
                                    ? QObject::tr("Cannot read the database format version from %1:\n%2").arg(filename, query.lastError().text())
                                    : QObject::tr("The database format version is missing from %1.").arg(filename);
            db.close();
            break;
        }

        bool validVersion = false;
        info.schemaVersion = query.value(0).toInt(&validVersion);
        if ( !validVersion || info.schemaVersion <= 0 )
        {
            info.errorMessage = QObject::tr("The database format version is invalid in %1.").arg(filename);
            db.close();
            break;
        }

        // Check if schema version is too new
        if ( info.schemaVersion > DBSchemaMigration::latestVersion )
        {
            info.errorMessage = QObject::tr("Cannot unpack database %1: its format version is %2, but this QLog supports up to %3. Use a newer QLog version to unpack it.")
                                    .arg(filename, QString::number(info.schemaVersion), QString::number(DBSchemaMigration::latestVersion));
            db.close();
            break;
        }

        if ( !query.exec("SELECT id, callsign FROM contacts LIMIT 0") )
        {
            qCWarning(runtime) << "Cannot inspect contacts:" << query.lastError();
            info.errorMessage = QObject::tr("Cannot inspect contacts in the unpacked database %1:\n%2").arg(filename, query.lastError().text());
            db.close();
            break;
        }

        // Export metadata is required, even when the credentials list is empty.
        if ( !query.exec("SELECT value FROM log_param WHERE name = 'sourceplatform'")
             || !query.first() || query.value(0).toString().isEmpty() )
        {
            qCWarning(runtime) << "Cannot read export platform:" << query.lastError();
            info.errorMessage = query.lastError().isValid()
                                    ? QObject::tr("Cannot read the export platform from %1:\n%2").arg(filename, query.lastError().text())
                                    : QObject::tr("The export platform is missing from %1. Choose a file created by Pack Data & Settings.").arg(filename);
            db.close();
            break;
        }
        info.sourcePlatform = query.value(0).toString();

        if ( !query.exec("SELECT value FROM log_param WHERE name = 'security/encryptedpasswords'") )
        {
            qCWarning(runtime) << "Cannot read export credentials metadata:" << query.lastError();
            info.errorMessage = QObject::tr("Cannot inspect saved passwords in the unpacked database %1:\n%2").arg(filename, query.lastError().text());
            db.close();
            break;
        }
        info.hasEncryptedPasswords = query.first() && !query.value(0).toString().isEmpty();

        info.valid = true;
        db.close();
    } while (false);

    QSqlDatabase::removeDatabase(connectionName);

    qCDebug(runtime) << "Database info - valid:" << info.valid
                     << "version:" << info.schemaVersion
                     << "platform:" << info.sourcePlatform
                     << "hasPasswords:" << info.hasEncryptedPasswords;

    return info;
}

QString LogDatabase::pendingImportPath()
{
    FCT_IDENTIFICATION;
    return dbDirectory().filePath(IMPORT_DATABASE_FILENAME);
}

bool LogDatabase::hasPendingImport()
{
    FCT_IDENTIFICATION;

    for ( const QString &name : {pendingImportPath(), dbDirectory().filePath(IMPORT_RECORD_FILENAME),
                                 dbDirectory().filePath(IMPORT_BACKUP_FILENAME)} )
        for ( const QString &suffix : {QString(), SQLITE_WAL_SUFFIX, SQLITE_SHM_SUFFIX} )
            if ( QFile::exists(name + suffix) )
                return true;
    return false;
}

bool LogDatabase::failImport(const QString &message)
{
    FCT_IDENTIFICATION;
    importError = message;
    qCWarning(runtime) << message;
    return false;
}

bool LogDatabase::readImportRecord(QJsonObject &record, ImportPhase &phase, QString *reason)
{
    FCT_IDENTIFICATION;

    QFile file(dbDirectory().filePath(IMPORT_RECORD_FILENAME));
    if ( reason )
        *reason = QObject::tr("The unpacking progress file %1 contains incomplete or invalid recovery data.").arg(file.fileName());
    if ( !file.open(QIODevice::ReadOnly) )
    {
        if ( reason )
            *reason = QObject::tr("Cannot open the unpacking progress file %1:\n%2").arg(file.fileName(), file.errorString());
        return false;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);

    if ( file.error() != QFile::NoError )
    {
        if ( reason )
            *reason = QObject::tr("Cannot read the unpacking progress file %1:\n%2").arg(file.fileName(), file.errorString());
        return false;
    }
    if ( error.error != QJsonParseError::NoError || !document.isObject() )
    {
        if ( reason && error.error != QJsonParseError::NoError )
            *reason = QObject::tr("Cannot understand the unpacking progress file %1:\n%2").arg(file.fileName(), error.errorString());
        return false;
    }

    record = document.object();
    phase = static_cast<ImportPhase>(IMPORT_PHASE_NAMES.indexOf(record.value("phase").toString()));

    if ( phase == ImportPhase::INVALID )
        return false;

    if ( QUuid(record.value("targetLogId").toString()).isNull() )
        return false;

    if ( !record.value("hadOriginal").isBool() || !record.value("requiresPassphrase").isBool() )
        return false;

    if ( !record.value("parameters").isArray() || !record.value("profilePorts").isArray()
         || !record.value("previousCredentials").isArray() )
        return false;

    const auto validEntries = [](const QJsonArray &entries, const QStringList &fields)
    {
        for ( const QJsonValue &entry : entries )
        {
            if ( !entry.isObject() )
                return false;
            const QJsonObject object = entry.toObject();
            for ( const QString &field : fields )
                if ( !object.value(field).isString() )
                    return false;
        }
        return true;
    };
    const bool valid = validEntries(record.value("parameters").toArray(), {"key", "newValue"})
                       && validEntries(record.value("profilePorts").toArray(), {"tableName", "columnName", "profileName", "newValue"})
                       && validEntries(record.value("previousCredentials").toArray(), {"storagekey", "username"});
    if ( valid && reason )
        reason->clear();
    return valid;
}

bool LogDatabase::writeImportRecord(QJsonObject &record, ImportPhase phase)
{
    FCT_IDENTIFICATION;

    record["phase"] = IMPORT_PHASE_NAMES.at(static_cast<int>(phase));
    QSaveFile file(dbDirectory().filePath(IMPORT_RECORD_FILENAME));
    file.setDirectWriteFallback(false);
    const QByteArray data = QJsonDocument(record).toJson(QJsonDocument::Compact);

    if ( !file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.flush() || !file.commit() )
    {
        const QString message = QObject::tr("Cannot save import progress %1:\n%2").arg(file.fileName(), file.errorString());
        return failImport(message);
    }

    return true;
}

bool LogDatabase::preparePendingImport(const QString &sourceFile,
                                       const QList<PlatformParameter> &parameters,
                                       const QList<ProfileParameter> &profileParameters,
                                       const QString &passphrase)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << sourceFile;
    importError.clear();

    if ( hasPendingImport() )
        return failImport(QObject::tr("An unfinished import already exists in %1. Preserve these files and finish or explicitly cancel that import first.")
                          .arg(dbDirectory().path()));

    const DatabaseInfo info = inspectDatabase(sourceFile);

    if ( !info.valid )
        return failImport(info.errorMessage);

    if ( QFileInfo(sourceFile).absoluteFilePath() == QFileInfo(dbFilename()).absoluteFilePath() )
        return failImport(QObject::tr("The active database cannot be used as an import candidate."));

    if ( info.hasEncryptedPasswords && passphrase.isEmpty() )
        return failImport(QObject::tr("The import passphrase is required."));

    // A decompressed archive is standalone; never stage a main file while its WAL holds data.
    if ( QFileInfo(sourceFile + SQLITE_WAL_SUFFIX).size() > 0 )
        return failImport(QObject::tr("The source database has unfinished writes and cannot be imported."));

    QJsonObject record;
    record["targetLogId"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record["hadOriginal"] = QFile::exists(dbFilename());
    record["requiresPassphrase"] = info.hasEncryptedPasswords;
    QJsonArray params;
    for ( const PlatformParameter &parameter : parameters )
        if ( !parameter.newValue.isEmpty() )
            params.append(QJsonObject{{"key", parameter.key}, {"newValue", parameter.newValue}});
    record["parameters"] = params;
    QJsonArray profiles;
    for ( const ProfileParameter &parameter : profileParameters )
        if ( !parameter.newValue.isEmpty() )
            profiles.append(QJsonObject{{"tableName", parameter.tableName}, {"columnName", parameter.columnName},
                                       {"profileName", parameter.profileName}, {"newValue", parameter.newValue}});
    record["profilePorts"] = profiles;

    QJsonArray previous;
    QSet<QPair<QString, QString>> identities;
    for ( const CredentialDescriptor &descriptor : CredentialRegistry::instance().allDescriptors() )
    {
        const QString username = descriptor.usernameFn();
        const QPair<QString, QString> identity(descriptor.storageKey, username);
        if ( username.isEmpty() || descriptor.storageKey.isEmpty() || identities.contains(identity) )
            continue;
        identities.insert(identity);
        previous.append(QJsonObject{{"storagekey", descriptor.storageKey}, {"username", username}});
    }

    record["previousCredentials"] = previous;

    QFile source(sourceFile);
    QSaveFile candidate(pendingImportPath());
    candidate.setDirectWriteFallback(false);
    const bool colocated = QFileInfo(sourceFile).absolutePath() == dbDirectory().absolutePath();

    if ( colocated )
        for ( const QString &suffix : {SQLITE_WAL_SUFFIX, SQLITE_SHM_SUFFIX} )
        {
            QFile sidecar(sourceFile + suffix);
            if ( sidecar.exists() && !sidecar.remove() )
                return failImport(QObject::tr("Cannot remove temporary import file %1:\n%2").arg(sidecar.fileName(), sidecar.errorString()));
        }

    if ( !colocated )
    {
        if ( !source.open(QIODevice::ReadOnly) )
            return failImport(QObject::tr("Cannot open the unpacked database for import preparation %1:\n%2").arg(sourceFile, source.errorString()));
        if ( !candidate.open(QIODevice::WriteOnly) )
            return failImport(QObject::tr("Cannot prepare imported database %1:\n%2").arg(candidate.fileName(), candidate.errorString()));
    }

    if ( info.hasEncryptedPasswords && !CredentialStore::instance()->saveImportPassphrase(passphrase) )
        return failImport(QObject::tr("Failed to save the import passphrase."));

    // The import dialog decompresses beside the DB, so the usual preparation is a rename.
    bool staged = false;
    QString preparationError;
    if ( colocated )
    {
        staged = source.rename(pendingImportPath());
        if ( !staged )
            preparationError = QObject::tr("Cannot move unpacked database to %1:\n%2").arg(pendingImportPath(), source.errorString());
    }
    else
    {
        bool copied = true;
        while ( !source.atEnd() )
        {
            const QByteArray data = source.read(1024 * 1024);
            if ( data.isEmpty() || candidate.write(data) != data.size() )
            {
                copied = false;
                break;
            }
        }
        staged = copied && source.error() == QFile::NoError && candidate.flush() && candidate.commit();
        if ( !staged )
            preparationError = source.error() != QFileDevice::NoError
                                   ? QObject::tr("Cannot read unpacked database %1:\n%2").arg(sourceFile, source.errorString())
                                   : QObject::tr("Cannot save imported database %1:\n%2").arg(candidate.fileName(), candidate.errorString());
    }

    if ( !staged || !writeImportRecord(record, ImportPhase::PREPARED) )
    {
        if ( staged )
            preparationError = importError;
        if ( staged && colocated )
        {
            if ( !QFile::rename(pendingImportPath(), sourceFile) )
                qCWarning(runtime) << "Cannot return unaccepted candidate to its temporary path";
        }
        else if ( QFile::exists(pendingImportPath()) && !QFile::remove(pendingImportPath()) )
            qCWarning(runtime) << "Cannot remove unaccepted import candidate";

        if ( info.hasEncryptedPasswords && !CredentialStore::instance()->deleteImportPassphrase() )
            qCWarning(runtime) << "Cannot remove unused import passphrase";

        return failImport(QObject::tr("%1\nThe current database was preserved.").arg(preparationError));
    }
    return true;
}

bool LogDatabase::checkpointDatabase(QSqlDatabase database)
{
    FCT_IDENTIFICATION;

    const QString filename = database.databaseName();
    {
        QSqlQuery query(database);
        if ( !query.exec("PRAGMA wal_checkpoint(TRUNCATE)") || !query.next() || query.value(0).toInt() != 0 )
        {
            qCWarning(runtime) << "Cannot checkpoint database:" << filename << query.lastError();
            const QString reason = query.lastError().isValid() ? query.lastError().text() : QObject::tr("The database is still in use.");
            return failImport(QObject::tr("Cannot finish pending database writes in %1:\n%2").arg(filename, reason));
        }
    }
    database.close();
    for ( const QString &suffix : {SQLITE_WAL_SUFFIX, SQLITE_SHM_SUFFIX} )
    {
        QFile file(filename + suffix);
        if ( file.exists() && !file.remove() )
        {
            qCWarning(runtime) << "Cannot remove checkpointed database sidecar:" << filename + suffix;
            return failImport(QObject::tr("Cannot remove the temporary file after finishing database writes %1:\n%2")
                                  .arg(file.fileName(), file.errorString()));
        }
    }

    return true;
}

bool LogDatabase::readLogId(const QString &filename, QString &logId, QString *reason)
{
    FCT_IDENTIFICATION;
    logId.clear();
    if ( reason )
        reason->clear();
    bool result = false;
    const QString name = "ImportIdentity";
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setConnectOptions("QSQLITE_OPEN_READONLY");
        db.setDatabaseName(filename);
        if ( db.open() )
        {
            QSqlQuery query(db);
            result = query.exec("SELECT value FROM log_param WHERE name = 'logid'");
            if ( result && query.next() )
                logId = query.value(0).toString();
            if ( query.lastError().isValid() )
                result = false;
            if ( !result )
            {
                qCWarning(runtime) << "Cannot identify import database:" << query.lastError();
                if ( reason )
                    *reason = QObject::tr("Cannot read the logbook identity from %1:\n%2").arg(filename, query.lastError().text());
            }
        }
        else
        {
            qCWarning(runtime) << "Cannot open import database:" << db.lastError();
            if ( reason )
                *reason = QObject::tr("Cannot open the database to check its logbook identity %1:\n%2").arg(filename, db.lastError().text());
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
    return result;
}

bool LogDatabase::canCancelImport(const QJsonObject &record, ImportPhase phase, bool &currentIsImport)
{
    FCT_IDENTIFICATION;
    currentIsImport = false;
    if ( phase != ImportPhase::PREPARED && phase != ImportPhase::READY && phase != ImportPhase::ABORTED )
        return false;

    const QString targetId = record.value("targetLogId").toString();
    const bool hadOriginal = record.value("hadOriginal").toBool();
    const QString original = dbDirectory().filePath(IMPORT_BACKUP_FILENAME);
    const bool currentExists = QFile::exists(dbFilename());
    const bool originalExists = QFile::exists(original);
    QString currentId;
    QString originalId;
    if ( (currentExists && !readLogId(dbFilename(), currentId))
         || (originalExists && (!readLogId(original, originalId) || originalId == targetId)) )
        return false;

    currentIsImport = currentExists && currentId == targetId;
    if ( (currentIsImport && QFile::exists(pendingImportPath()))
         || (phase == ImportPhase::PREPARED && (currentIsImport || originalExists)) )
        return false;

    if ( hadOriginal )
        return originalExists ? !currentExists || currentIsImport : currentExists && !currentIsImport;
    return !originalExists && (!currentExists || currentIsImport);
}

bool LogDatabase::canCancelPendingImport() const
{
    FCT_IDENTIFICATION;
    QJsonObject record;
    ImportPhase phase = ImportPhase::INVALID;
    bool currentIsImport = false;
    return readImportRecord(record, phase) && canCancelImport(record, phase, currentIsImport);
}

bool LogDatabase::cancelPendingImport()
{
    FCT_IDENTIFICATION;
    QJsonObject record;
    ImportPhase phase = ImportPhase::INVALID;

    if ( !hasPendingImport() )
        return true;

    bool currentIsImport = false;
    QString reason;
    if ( !readImportRecord(record, phase, &reason) )
        return failImport(QObject::tr("Cannot read the data needed to cancel unpacking:\n%1").arg(reason));
    if ( !canCancelImport(record, phase, currentIsImport) )
        return failImport(QObject::tr("The import files in %1 are inconsistent, so unpacking cannot be cancelled safely. Its recovery files were preserved.")
                              .arg(dbDirectory().absolutePath()));

    // A failed preparation may still hold the candidate open; Windows cannot delete it then.
    // Preserve the original connection when cancellation follows a failed restart from the GUI.
    if ( QSqlDatabase::contains() )
    {
        QSqlDatabase db = QSqlDatabase::database(QSqlDatabase::defaultConnection, false);
        if ( db.databaseName() == pendingImportPath() )
        {
            db.close();
            LogParam::clearCache();
        }
    }

    if ( !writeImportRecord(record, ImportPhase::ABORTED) )
    {
        const QString recordError = importError;
        if ( phase != ImportPhase::PREPARED )
            return failImport(QObject::tr("Cannot record the cancellation of unpacking:\n%1\nRetry cancellation before restarting.").arg(recordError));
        // Removing the unpublished candidate also prevents adoption if progress cannot be saved.
        QFile candidate(pendingImportPath());
        if ( candidate.exists() && !candidate.remove() )
            return failImport(QObject::tr("Cannot remove the prepared database while cancelling unpacking %1:\n%2\nThe import may still run at the next startup.")
                                  .arg(candidate.fileName(), candidate.errorString()));

        return failImport(QObject::tr("The prepared import was removed, but its cancellation could not be recorded:\n%1\nThe current database was preserved.").arg(recordError));
    }

    // READY may have been interrupted between renames. It has never allowed logging or password writes.
    // Keep ABORTED until the original is back in place so interruption can retry the same cleanup.
    if ( currentIsImport )
    {
        if ( !openDatabase() || !checkpointDatabase(QSqlDatabase::database()) )
            return false;
        QFile current(dbFilename());
        if ( !current.rename(pendingImportPath()) )
            return failImport(QObject::tr("Cannot move the cancelled import from %1 to %2:\n%3\nAll recovery files were preserved.")
                                  .arg(dbFilename(), pendingImportPath(), current.errorString()));
    }
    const QString original = dbDirectory().filePath(IMPORT_BACKUP_FILENAME);
    if ( QFile::exists(original) )
    {
        if ( !openDatabase(original) || !checkpointDatabase(QSqlDatabase::database()) )
            return false;
        QFile originalFile(original);
        if ( !originalFile.rename(dbFilename()) )
            return failImport(QObject::tr("Cannot restore the original database from %1 to %2 after cancelling unpacking:\n%3")
                                  .arg(original, dbFilename(), originalFile.errorString()));
    }

    for ( const QString &suffix : {SQLITE_WAL_SUFFIX, SQLITE_SHM_SUFFIX, QString()} )
    {
        QFile file(pendingImportPath() + suffix);
        if ( file.exists() && !file.remove() )
            return failImport(QObject::tr("Cannot remove a temporary file after cancelling unpacking %1:\n%2").arg(file.fileName(), file.errorString()));
    }

    if ( record.value("requiresPassphrase").toBool() && !CredentialStore::instance()->deleteImportPassphrase() )
        qCWarning(runtime) << "Cancelled import passphrase could not be removed; the next import will replace it";

    QFile progress(dbDirectory().filePath(IMPORT_RECORD_FILENAME));
    if ( !progress.remove() )
        return failImport(QObject::tr("Cannot remove the cancelled unpacking progress file %1:\n%2").arg(progress.fileName(), progress.errorString()));
    return true;
}

LogDatabase::ImportResult LogDatabase::initialize(QString *backupSnapshot)
{
    FCT_IDENTIFICATION;
    importError.clear();
    QString unusedSnapshot;
    QString &snapshot = backupSnapshot ? *backupSnapshot : unusedSnapshot;
    snapshot.clear();
    QJsonObject record;
    ImportPhase phase = ImportPhase::INVALID;
    QJsonArray incoming;
    bool newlyPublished = false;
    if ( hasPendingImport() )
    {
        if ( !readImportRecord(record, phase, &importError) )
        {
            failImport(QObject::tr("Cannot resume unpacking:\n%1\nPreserve these files and prepare the import again.").arg(importError));
            return ImportResult::FAILED;
        }
        if ( phase == ImportPhase::ABORTED )
        {
            if ( !cancelPendingImport() )
                return ImportResult::FAILED;
            phase = ImportPhase::INVALID;
        }
        else
        {
            if ( phase == ImportPhase::PREPARED || phase == ImportPhase::READY )
            {
                const bool preparedThisStartup = phase == ImportPhase::PREPARED;
                if ( preparedThisStartup && !prepareImportDatabase(record, incoming, &snapshot) )
                    return ImportResult::FAILED;
                if ( !publishImportDatabase(record, incoming, preparedThisStartup, &snapshot) )
                    return ImportResult::FAILED;
                phase = ImportPhase::PUBLISHED;
                newlyPublished = true;
            }

            if ( !validatePublishedImport(record, phase) )
                return ImportResult::FAILED;
        }
    }

    // Every startup must validate the active schema, including deferred password recovery.
    if ( !openDatabase() )
    {
        return ImportResult::FAILED;
    }
    const bool hadContacts = QSqlDatabase::database().tables().contains(QStringLiteral("contacts"));
    if ( !schemaVersionUpgrade(&snapshot) )
    {
        return ImportResult::FAILED;
    }

    // Routine failures are reported by DatabaseBackup and do not block logging.
    // Preserve first-startup behavior: an empty newly created schema must not postpone the first QSO backup.
    if ( hadContacts && snapshot.isEmpty() )
        DatabaseBackup::prepare(false, &snapshot);

    if ( phase == ImportPhase::INVALID )
        return ImportResult::COMPLETE;

    if ( !newlyPublished && phase == ImportPhase::PUBLISHED
         && (record.value("requiresPassphrase").toBool() || !record.value("previousCredentials").toArray().isEmpty()) )
    {
        failImport(QObject::tr("Password recovery is unfinished. You can retry it or continue logging."));
        return ImportResult::NEEDS_ATTENTION;
    }

    return finishImport(record, phase, ImportAction::RETRY_PASSWORDS, newlyPublished ? &incoming : nullptr);
}

LogDatabase::ImportResult LogDatabase::recoverImportPasswords(ImportAction action)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << static_cast<int>(action);
    importError.clear();
    const QSqlDatabase db = QSqlDatabase::contains()
                            ? QSqlDatabase::database(QSqlDatabase::defaultConnection, false) : QSqlDatabase();
    if ( !db.isOpen() || db.databaseName() != dbFilename() )
    {
        failImport(QObject::tr("Password recovery requires the active database to be initialized first."));
        return ImportResult::FAILED;
    }
    if ( !hasPendingImport() )
        return ImportResult::COMPLETE;

    QJsonObject record;
    ImportPhase phase = ImportPhase::INVALID;
    if ( !readImportRecord(record, phase, &importError) )
    {
        failImport(QObject::tr("Cannot resume unpacked password recovery:\n%1\nAll recovery files were preserved.").arg(importError));
        return ImportResult::FAILED;
    }
    if ( !validatePublishedImport(record, phase) )
        return ImportResult::FAILED;
    return finishImport(record, phase, action);
}

bool LogDatabase::decodeImportPasswords(bool requiresPassphrase, QJsonArray &incoming)
{
    FCT_IDENTIFICATION;
    if ( !requiresPassphrase )
        return true;

    CredentialStore *credentials = CredentialStore::instance();
    const QString passphrase = credentials->getImportPassphrase();
    return !passphrase.isEmpty() && credentials->decodePasswords(passphrase, incoming);
}

bool LogDatabase::prepareImportDatabase(QJsonObject &record, QJsonArray &incoming, QString *backupSnapshot)
{
    FCT_IDENTIFICATION;

    const QString pending = pendingImportPath();
    const QString original = dbDirectory().filePath(IMPORT_BACKUP_FILENAME);
    const QString targetId = record.value("targetLogId").toString();
    const bool hadOriginal = record.value("hadOriginal").toBool();
    const bool requiresPassphrase = record.value("requiresPassphrase").toBool();

    const DatabaseInfo info = inspectDatabase(pending);
    if ( !info.valid )
        return failImport(QObject::tr("Cannot validate the prepared import:\n%1\nThe original database was preserved.").arg(info.errorMessage));
    if ( info.hasEncryptedPasswords != requiresPassphrase
         || QFile::exists(original)
         || QFile::exists(dbFilename()) != hadOriginal )
        return failImport(QObject::tr("The prepared import database %1 does not match its recovery data in %2. The original database was preserved.")
                              .arg(pending, dbDirectory().filePath(IMPORT_RECORD_FILENAME)));

    if ( !openDatabase(pending) )
        return false;
    if ( !schemaVersionUpgrade(backupSnapshot) )
        return failImport(QObject::tr("%1\nThe original database was preserved while preparing the import.").arg(importError));

    QList<PlatformParameter> params;
    const QJsonArray paramJsonArray = record.value("parameters").toArray();
    for ( const QJsonValue &value :  paramJsonArray )
    {
        const QJsonObject entry = value.toObject();
        params.append({entry.value("key").toString(), {}, {}, entry.value("newValue").toString(), false});
    }

    QList<ProfileParameter> profiles;
    const QJsonArray profilesJsonArray = record.value("profilePorts").toArray();
    for ( const QJsonValue &value :  profilesJsonArray )
    {
        const QJsonObject entry = value.toObject();
        profiles.append({entry.value("tableName").toString(), entry.value("profileName").toString(),
                         entry.value("columnName").toString(), {}, {}, entry.value("newValue").toString(), false});
    }

    {
        QSqlDatabase db = QSqlDatabase::database();
        if ( !db.transaction() )
            return failImport(QObject::tr("Failed to start the import settings transaction."));

        if ( !PlatformParameterManager::applyParameters(params)
             || !PlatformParameterManager::applyProfileParameters(profiles)
             || !PlatformParameterManager::applyFlatpakFixedPaths()
             || !LogParam::setLogID(targetId)
             || !db.commit() )
        {
            qCWarning(runtime) << "Cannot apply import settings:" << db.lastError();
            if ( !db.rollback() )
                qCWarning(runtime) << "Cannot roll back import settings:" << db.lastError();
            LogParam::clearCache();
            return failImport(QObject::tr("Import settings could not be applied. The original database was preserved."));
        }

        QSqlQuery query(db);
        const bool integrityCheckPassed = query.exec("PRAGMA integrity_check")
                                          && query.next()
                                          && query.value(0).toString() == "ok"
                                          && !query.next()
                                          && !query.lastError().isValid();
        const bool integrityValid = integrityCheckPassed
                                    && query.exec("PRAGMA foreign_key_check")
                                    && !query.next()
                                    && !query.lastError().isValid();

        if ( !integrityValid )
        {
            qCWarning(runtime) << "Imported database integrity check failed:" << query.lastError();
            return failImport(QObject::tr("The imported database failed its integrity check. The original database was preserved."));
        }
    }

    if ( !decodeImportPasswords(requiresPassphrase, incoming) )
        return failImport(QObject::tr("The import passphrase is unavailable or the passwords could not be decrypted. All recovery files were preserved."));

    if ( !checkpointDatabase(QSqlDatabase::database()) )
        return false;
    if ( !writeImportRecord(record, ImportPhase::READY) )
        return failImport(QObject::tr("Cannot record that the unpacked database is ready:\n%1\nThe original database was preserved.").arg(importError));
    return true;
}

bool LogDatabase::publishImportDatabase(QJsonObject &record, QJsonArray &incoming, bool preparedThisStartup, QString *backupSnapshot)
{
    FCT_IDENTIFICATION;

    const QString pending = pendingImportPath();
    const QString original = dbDirectory().filePath(IMPORT_BACKUP_FILENAME);
    const QString targetId = record.value("targetLogId").toString();
    const bool hadOriginal = record.value("hadOriginal").toBool();
    const bool requiresPassphrase = record.value("requiresPassphrase").toBool();

    QString currentId;
    QString candidateId = preparedThisStartup ? targetId : QString();
    QString readError;
    const bool candidateExists = QFile::exists(pending);
    const bool currentExists = QFile::exists(dbFilename());
    const bool originalExists = QFile::exists(original);
    bool filesMatch = (!currentExists || readLogId(dbFilename(), currentId, &readError))
                       && (!candidateExists || preparedThisStartup || readLogId(pending, candidateId, &readError));
    if ( !filesMatch )
        return failImport(QObject::tr("Cannot verify the files before replacing the logbook database:\n%1").arg(readError));

    if ( candidateExists )
    {
        // Before publication the original is either current or already renamed, never both.
        const bool originalPreserved = hadOriginal ? currentExists != originalExists
                                                   : !currentExists && !originalExists;
        filesMatch = filesMatch && candidateId == targetId && currentId != targetId && originalPreserved;
    }
    else
        filesMatch = filesMatch && currentExists && currentId == targetId && originalExists == hadOriginal;

    if ( !filesMatch )
        return failImport(QObject::tr("Cannot resume database replacement in %1 because its files do not match the unpacking progress file %2. All recovery files were preserved.")
                              .arg(dbDirectory().absolutePath(), dbDirectory().filePath(IMPORT_RECORD_FILENAME)));

    if ( !preparedThisStartup )
    {
        const QString filename = candidateExists ? pending : dbFilename();
        if ( !openDatabase(filename) )
            return false;
        if ( !schemaVersionUpgrade(backupSnapshot) )
            return failImport(QObject::tr("%1\nThe resumed import recovery files were preserved.").arg(importError));
        if ( !decodeImportPasswords(requiresPassphrase, incoming) )
            return failImport(QObject::tr("The import passwords could not be restored. All recovery files were preserved."));
        if ( !checkpointDatabase(QSqlDatabase::database()) )
            return false;
    }

    if ( candidateExists )
    {
        if ( !LOVDatabase::discardPrepared(dbDirectory().path()) )
            return failImport(QObject::tr("Failed to discard prepared reference lists before import."));

        if ( hadOriginal && !originalExists )
        {
            bool checkpointed = false;
            const QString name = "ImportOriginal";
            {
                QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
                db.setDatabaseName(dbFilename());
                if ( !db.open() )
                    failImport(QObject::tr("Cannot open the original logbook database before import %1:\n%2").arg(dbFilename(), db.lastError().text()));
                else
                    checkpointed = checkpointDatabase(db);
                if ( !checkpointed )
                    qCWarning(runtime) << "Cannot finish original database writes:" << db.lastError();
                db.close();
            }
            QSqlDatabase::removeDatabase(name);
            if ( !checkpointed )
                return false;
            QFile current(dbFilename());
            if ( !current.rename(original) )
                return failImport(QObject::tr("Cannot preserve the original logbook database by moving %1 to %2:\n%3\nImport was stopped.")
                                      .arg(dbFilename(), original, current.errorString()));
        }

        QFile candidate(pending);
        if ( !candidate.rename(dbFilename()) )
        {
            const QString reason = candidate.errorString();
            if ( hadOriginal && !QFile::rename(original, dbFilename()) )
                qCWarning(runtime) << "Cannot restore original database name";
            return failImport(QObject::tr("Cannot install the unpacked database by moving %1 to %2:\n%3\nAll recovery files were preserved.")
                                  .arg(pending, dbFilename(), reason));
        }
    }

    if ( !writeImportRecord(record, ImportPhase::PUBLISHED) )
        return failImport(QObject::tr("The unpacked database is installed, but this step could not be recorded:\n%1\nRestart to retry.").arg(importError));

    return true;
}

bool LogDatabase::validatePublishedImport(const QJsonObject &record, ImportPhase phase)
{
    FCT_IDENTIFICATION;

    const QString pending = pendingImportPath();
    const QString original = dbDirectory().filePath(IMPORT_BACKUP_FILENAME);
    const QString targetId = record.value("targetLogId").toString();
    const bool hadOriginal = record.value("hadOriginal").toBool();
    QString publishedId;
    QString readError;
    const bool originalMatches = hadOriginal ? phase == ImportPhase::CREDENTIALS_RESTORED
                                              || phase == ImportPhase::CREDENTIALS_SKIPPED || QFile::exists(original)
                                             : !QFile::exists(original);

    if ( (phase != ImportPhase::PUBLISHED && phase != ImportPhase::CREDENTIALS_RESTORED
          && phase != ImportPhase::CREDENTIALS_SKIPPED)
         || QFile::exists(pending)
         || !QFile::exists(dbFilename())
         || !readLogId(dbFilename(), publishedId, &readError)
         || publishedId != targetId
         || !originalMatches )
        return failImport(readError.isEmpty()
                              ? QObject::tr("The active unpacked database %1 does not match its recovery record %2. All files were preserved.")
                                    .arg(dbFilename(), dbDirectory().filePath(IMPORT_RECORD_FILENAME))
                              : QObject::tr("Cannot verify the active unpacked database:\n%1").arg(readError));

    return true;
}

LogDatabase::ImportResult LogDatabase::finishImport(QJsonObject &record, ImportPhase phase, ImportAction action,
                                                   const QJsonArray *incoming)
{
    FCT_IDENTIFICATION;

    const QString original = dbDirectory().filePath(IMPORT_BACKUP_FILENAME);
    const bool requiresPassphrase = record.value("requiresPassphrase").toBool();
    CredentialStore *credentials = CredentialStore::instance();

    const auto deferImport = [this](const QString &message)
    {
        failImport(message);
        return ImportResult::NEEDS_ATTENTION;
    };

    if ( action == ImportAction::FINISH_WITHOUT_PASSWORDS && phase != ImportPhase::CREDENTIALS_SKIPPED )
    {
        // Persist the decision before removing recovery data so a restart cannot restore passwords again.
        if ( !writeImportRecord(record, ImportPhase::CREDENTIALS_SKIPPED) )
            return deferImport(QObject::tr("Cannot record the decision to stop unpacked password recovery:\n%1\nRecovery data was preserved.").arg(importError));
        phase = ImportPhase::CREDENTIALS_SKIPPED;
    }

    if ( phase == ImportPhase::PUBLISHED )
    {
        QJsonArray decoded;
        if ( !incoming )
        {
            if ( !decodeImportPasswords(requiresPassphrase, decoded) )
                return deferImport(QObject::tr("The database was imported, but its saved passwords could not be decrypted."));
            incoming = &decoded;
        }
        if ( !credentials->restorePasswords(*incoming) )
            return deferImport(QObject::tr("The database was imported, but its passwords could not be saved securely on this computer."));

        const auto credentialIdentity = [](const QString &storageKey, const QString &username)
        {
#ifdef Q_OS_WIN
            // Windows credential targets differing only in case refer to the same stored password.
            return qMakePair(storageKey.toCaseFolded(), username.toCaseFolded());
#else
            return qMakePair(storageKey, username);
#endif
        };
        QSet<QPair<QString, QString>> restored;

        for ( const QJsonValue &value : *incoming )
        {
            const QJsonObject entry = value.toObject();
            restored.insert(credentialIdentity(entry.value("storagekey").toString(), entry.value("username").toString()));
        }

        bool deleted = true;
        const QJsonArray prevJsonCred = record.value("previousCredentials").toArray();
        for ( const QJsonValue &value : prevJsonCred )
        {
            const QJsonObject previous = value.toObject();
            const QPair<QString, QString> identity(previous.value("storagekey").toString(), previous.value("username").toString());
            if ( !restored.contains(credentialIdentity(identity.first, identity.second))
                 && !credentials->deletePassword(identity.first, identity.second) )
                deleted = false;
        }
        if ( !deleted )
            return deferImport(QObject::tr("The database was imported, but the previous saved passwords could not be removed."));
        if ( !writeImportRecord(record, ImportPhase::CREDENTIALS_RESTORED) )
            return deferImport(QObject::tr("Cannot record completion of unpacked password recovery:\n%1").arg(importError));
    }

    {
        QSqlDatabase db = QSqlDatabase::database();
        if ( !db.transaction() )
        {
            failImport(QObject::tr("Cannot start cleaning up unpacked database settings in %1:\n%2\nRestart to retry.")
                           .arg(dbFilename(), db.lastError().text()));
            return ImportResult::FAILED;
        }

        QString cleanupError;
        if ( !LogParam::removeEncryptedPasswords() )
            cleanupError = QObject::tr("Cannot remove the imported password data from the logbook database %1.").arg(dbFilename());
        else if ( !LogParam::removeSourcePlatform() )
            cleanupError = QObject::tr("Cannot remove the import platform marker from the logbook database %1.").arg(dbFilename());
        else if ( !db.commit() )
            cleanupError = QObject::tr("Cannot save the completed import cleanup in %1:\n%2").arg(dbFilename(), db.lastError().text());
        if ( !cleanupError.isEmpty() )
        {
            qCWarning(runtime) << "Cannot remove import metadata:" << db.lastError();
            if ( !db.rollback() )
                qCWarning(runtime) << "Cannot roll back import cleanup:" << db.lastError();

            LogParam::clearCache();

            failImport(QObject::tr("%1\nThe import is complete. Restart to retry cleanup.").arg(cleanupError));
            return ImportResult::FAILED;
        }
    }
    if ( requiresPassphrase && !credentials->deleteImportPassphrase() && phase != ImportPhase::CREDENTIALS_SKIPPED )
        return deferImport(QObject::tr("The import is complete, but its saved passphrase could not be removed."));

    for ( const QString &suffix : {SQLITE_WAL_SUFFIX, SQLITE_SHM_SUFFIX} )
        if ( QFile::exists(original + suffix) )
            return deferImport(QObject::tr("Import cleanup is waiting because the original database still has a temporary file %1.").arg(original + suffix));

    QFile originalFile(original);
    if ( originalFile.exists() && !originalFile.remove() )
        return deferImport(QObject::tr("Cannot remove the original database after successful unpacking %1:\n%2").arg(original, originalFile.errorString()));
    QFile progress(dbDirectory().filePath(IMPORT_RECORD_FILENAME));
    if ( !progress.remove() )
        return deferImport(QObject::tr("Cannot remove the completed unpacking progress file %1:\n%2").arg(progress.fileName(), progress.errorString()));

    return ImportResult::COMPLETE;
}
