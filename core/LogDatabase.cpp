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
    "PREPARED", "READY", "PUBLISHED", "CREDENTIALS_RESTORED", "ABORTED"
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

bool LogDatabase::atomicCopy(const QString &filename)
{
    FCT_IDENTIFICATION;

    QSqlDatabase db = QSqlDatabase::database();

    if ( !db.isOpen() )
    {
        qWarning() << "Database is not opened";
        return false;
    }

    // Validate filename — must be a plain filename, no path separators
    if ( filename.isEmpty() )
    {
        qWarning() << "Invalid filename:" << filename;
        return false;
    }

    QVariant v = db.driver()->handle();
    if ( !v.isValid() || qstrcmp(v.typeName(), "sqlite3*") != 0)
    {
        qWarning() << "Cannot get Database Driver Handler";
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
        goto cleanup;
    }

    backup = sqlite3_backup_init(dstHandle, "main", srcHandle, "main");
    if ( !backup )
    {
        qWarning() << "Cannot Init a backup" << sqlite3_errmsg(dstHandle);
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
            break;
        }
    }

    // sqlite3_backup_finish always releases the backup handle,
    // even on error — never call it twice
    if ( sqlite3_backup_finish(backup) != SQLITE_OK )
    {
        qWarning() << "Cannot finalize the database copy" << sqlite3_errmsg(dstHandle);
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
        return false;
    }

    QSqlQuery query;
    if ( !query.exec("PRAGMA foreign_keys = ON") )
    {
        qCritical() << "Cannot set PRAGMA foreign_keys";
        return false;
    }

    if ( !query.exec("PRAGMA journal_mode = WAL") )
    {
        qCritical() << "Cannot set PRAGMA journal_mode";
        return false;
    }

    while ( query.next() )
    {
        QString pragma = query.value(0).toString();
        qCDebug(runtime) << "Pragma result:" << pragma;
    }

    return createSQLFunctions();
}

bool LogDatabase::schemaVersionUpgrade(QString *backupSnapshot)
{
    FCT_IDENTIFICATION;

    DBSchemaMigration m;
    return m.run(backupSnapshot);
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
            info.errorMessage = db.lastError().text();
            qCWarning(runtime) << "Cannot open database for inspection:" << info.errorMessage;
            break;
        }

        // Check if it's a valid QLog database by looking for schema_versions table
        QSqlQuery query(db);
        if ( !query.exec("SELECT version FROM schema_versions ORDER BY version DESC LIMIT 1")
             || !query.first() )
        {
            qCWarning(runtime) << "Cannot read database schema version:" << query.lastError();
            info.errorMessage = QObject::tr("Not a valid QLog database");
            db.close();
            break;
        }

        bool validVersion = false;
        info.schemaVersion = query.value(0).toInt(&validVersion);
        if ( !validVersion || info.schemaVersion <= 0 )
        {
            info.errorMessage = QObject::tr("Not a valid QLog database");
            db.close();
            break;
        }

        // Check if schema version is too new
        if ( info.schemaVersion > DBSchemaMigration::latestVersion )
        {
            info.errorMessage = QObject::tr("Database version too new (requires newer QLog version)");
            db.close();
            break;
        }

        if ( !query.exec("SELECT id, callsign FROM contacts LIMIT 0") )
        {
            qCWarning(runtime) << "Cannot inspect contacts:" << query.lastError();
            info.errorMessage = QObject::tr("Not a valid QLog database");
            db.close();
            break;
        }

        // Export metadata is required, even when the credentials list is empty.
        if ( !query.exec("SELECT value FROM log_param WHERE name = 'sourceplatform'")
             || !query.first() || query.value(0).toString().isEmpty() )
        {
            qCWarning(runtime) << "Cannot read export platform:" << query.lastError();
            info.errorMessage = QObject::tr("Database is not QLog Export file");
            db.close();
            break;
        }
        info.sourcePlatform = query.value(0).toString();

        if ( !query.exec("SELECT value FROM log_param WHERE name = 'security/encryptedpasswords'") )
        {
            qCWarning(runtime) << "Cannot read export credentials metadata:" << query.lastError();
            info.errorMessage = QObject::tr("Not a valid QLog database");
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

bool LogDatabase::readImportRecord(QJsonObject &record, ImportPhase &phase)
{
    FCT_IDENTIFICATION;

    QFile file(dbDirectory().filePath(IMPORT_RECORD_FILENAME));
    if ( !file.open(QIODevice::ReadOnly) )
        return false;

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);

    if ( file.error() != QFile::NoError || error.error != QJsonParseError::NoError || !document.isObject() )
        return false;

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
    return validEntries(record.value("parameters").toArray(), {"key", "newValue"})
        && validEntries(record.value("profilePorts").toArray(), {"tableName", "columnName", "profileName", "newValue"})
        && validEntries(record.value("previousCredentials").toArray(), {"storagekey", "username"});
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
        qCWarning(runtime) << "Cannot save import progress:" << file.errorString();
        return false;
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
            if ( QFile::exists(sourceFile + suffix) && !QFile::remove(sourceFile + suffix) )
                return failImport(QObject::tr("The temporary database files could not be released for import."));

    if ( !colocated && (!source.open(QIODevice::ReadOnly) || !candidate.open(QIODevice::WriteOnly)) )
        return failImport(QObject::tr("Failed to prepare the database for import."));

    if ( info.hasEncryptedPasswords && !CredentialStore::instance()->saveImportPassphrase(passphrase) )
        return failImport(QObject::tr("Failed to save the import passphrase."));

    // The import dialog decompresses beside the DB, so the usual preparation is a rename.
    bool staged = false;
    if ( colocated )
        staged = QFile::rename(sourceFile, pendingImportPath());
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
    }

    if ( !staged || !writeImportRecord(record, ImportPhase::PREPARED) )
    {
        if ( staged && colocated )
        {
            if ( !QFile::rename(pendingImportPath(), sourceFile) )
                qCWarning(runtime) << "Cannot return unaccepted candidate to its temporary path";
        }
        else if ( QFile::exists(pendingImportPath()) && !QFile::remove(pendingImportPath()) )
            qCWarning(runtime) << "Cannot remove unaccepted import candidate";

        if ( info.hasEncryptedPasswords && !CredentialStore::instance()->deleteImportPassphrase() )
            qCWarning(runtime) << "Cannot remove unused import passphrase";

        return failImport(QObject::tr("Failed to prepare the database for import. The current database was preserved."));
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
            return false;
        }
    }
    database.close();
    for ( const QString &suffix : {SQLITE_WAL_SUFFIX, SQLITE_SHM_SUFFIX} )
        if ( QFile::exists(filename + suffix) && !QFile::remove(filename + suffix) )
        {
            qCWarning(runtime) << "Cannot remove checkpointed database sidecar:" << filename + suffix;
            return false;
        }

    return true;
}

bool LogDatabase::readLogId(const QString &filename, QString &logId)
{
    FCT_IDENTIFICATION;
    logId.clear();
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
            if ( !result )
                qCWarning(runtime) << "Cannot identify import database:" << query.lastError();
        }
        else
            qCWarning(runtime) << "Cannot open import database:" << db.lastError();
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
    return result;
}

bool LogDatabase::cancelPendingImport()
{
    FCT_IDENTIFICATION;
    QJsonObject record;
    ImportPhase phase = ImportPhase::INVALID;

    if ( !hasPendingImport() )
        return true;

    if ( !readImportRecord(record, phase)
         || (phase != ImportPhase::PREPARED && phase != ImportPhase::ABORTED)
         || QFile::exists(dbFilename()) != record.value("hadOriginal").toBool()
         || QFile::exists(dbDirectory().filePath(IMPORT_BACKUP_FILENAME)) )
        return failImport(QObject::tr("This import cannot be cancelled safely. Its recovery files were preserved."));

    if ( !writeImportRecord(record, ImportPhase::ABORTED) )
    {
        // Removing the unpublished candidate also prevents adoption if progress cannot be saved.
        if ( QFile::exists(pendingImportPath()) && !QFile::remove(pendingImportPath()) )
            return failImport(QObject::tr("Cancellation failed. The prepared import still exists and may run at the next startup. Preserve its files and resolve the cancellation before restarting."));

        return failImport(QObject::tr("The import candidate was removed, but cancellation could not be recorded. The current database was preserved."));
    }

    for ( const QString &suffix : {SQLITE_WAL_SUFFIX, SQLITE_SHM_SUFFIX, QString()} )
        if ( QFile::exists(pendingImportPath() + suffix) && !QFile::remove(pendingImportPath() + suffix) )
            return failImport(QObject::tr("The import was cancelled, but its temporary files could not be removed."));

    if ( record.value("requiresPassphrase").toBool() && !CredentialStore::instance()->deleteImportPassphrase() )
        return failImport(QObject::tr("The import was cancelled, but its passphrase could not be removed."));

    if ( !QFile::remove(dbDirectory().filePath(IMPORT_RECORD_FILENAME)) )
        return failImport(QObject::tr("The import was cancelled, but its progress file could not be removed."));
    return true;
}

bool LogDatabase::processPendingImport(QString *backupSnapshot)
{
    FCT_IDENTIFICATION;
    importError.clear();
    if ( !hasPendingImport() )
        return true;

    QJsonObject record;
    ImportPhase phase = ImportPhase::INVALID;
    if ( !readImportRecord(record, phase) )
        return failImport(QObject::tr("The unfinished import has missing or invalid preparation data. Preserve the files in %1 and explicitly prepare the import again.")
                          .arg(dbDirectory().path()));
    if ( phase == ImportPhase::ABORTED )
    {
        if ( !cancelPendingImport() )
            return false;

        if ( !openDatabase() || !schemaVersionUpgrade(backupSnapshot) )
            return failImport(QObject::tr("The import was cancelled, but the current database could not be opened or migrated."));

        return true;
    }

    QJsonArray incoming;
    bool decoded = false;
    if ( phase == ImportPhase::PREPARED )
    {
        if ( !prepareImportDatabase(record, incoming, backupSnapshot) )
            return false;
        phase = ImportPhase::READY;
        decoded = true;
    }

    if ( phase == ImportPhase::READY )
    {
        if ( !publishImportDatabase(record, incoming, decoded) )
            return false;
        phase = ImportPhase::PUBLISHED;
        decoded = true;
    }

    return finishImport(record, phase, incoming, decoded);
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
    if ( !info.valid
         || info.hasEncryptedPasswords != requiresPassphrase
         || QFile::exists(original)
         || QFile::exists(dbFilename()) != hadOriginal )
        return failImport(QObject::tr("The prepared import does not match its recovery files. The original database was preserved."));

    if ( !openDatabase(pending) || !schemaVersionUpgrade(backupSnapshot) )
        return failImport(QObject::tr("The imported database could not be opened or migrated. The original database was preserved."));

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

    if ( !checkpointDatabase(QSqlDatabase::database()) || !writeImportRecord(record, ImportPhase::READY) )
        return failImport(QObject::tr("Failed to finalize the prepared database. The original database was preserved."));
    return true;
}

bool LogDatabase::publishImportDatabase(QJsonObject &record, QJsonArray &incoming, bool decoded)
{
    FCT_IDENTIFICATION;

    const QString pending = pendingImportPath();
    const QString original = dbDirectory().filePath(IMPORT_BACKUP_FILENAME);
    const QString targetId = record.value("targetLogId").toString();
    const bool hadOriginal = record.value("hadOriginal").toBool();
    const bool requiresPassphrase = record.value("requiresPassphrase").toBool();

    QString currentId;
    QString candidateId = decoded ? targetId : QString();
    const bool candidateExists = QFile::exists(pending);
    const bool currentExists = QFile::exists(dbFilename());
    const bool originalExists = QFile::exists(original);
    bool filesMatch = (!currentExists || readLogId(dbFilename(), currentId))
                       && (!candidateExists || decoded || readLogId(pending, candidateId));

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
        return failImport(QObject::tr("The interrupted import has inconsistent recovery files. All files were preserved for recovery."));

    if ( !decoded )
    {
        if ( !openDatabase(candidateExists ? pending : dbFilename()) || !decodeImportPasswords(requiresPassphrase, incoming) )
            return failImport(QObject::tr("The import passwords could not be restored. All recovery files were preserved."));
        if ( !checkpointDatabase(QSqlDatabase::database()) )
            return failImport(QObject::tr("Failed to finish database writes before import."));
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
                checkpointed = db.open() && checkpointDatabase(db);
                if ( !checkpointed )
                    qCWarning(runtime) << "Cannot finish original database writes:" << db.lastError();
                db.close();
            }
            QSqlDatabase::removeDatabase(name);
            if ( !checkpointed || !QFile::rename(dbFilename(), original) )
                return failImport(QObject::tr("The original database could not be preserved. Import was stopped."));
        }

        if ( !QFile::rename(pending, dbFilename()) )
        {
            if ( hadOriginal && !QFile::rename(original, dbFilename()) )
                qCWarning(runtime) << "Cannot restore original database name";
            return failImport(QObject::tr("The prepared database could not be installed. All recovery files were preserved."));
        }
    }

    if ( !writeImportRecord(record, ImportPhase::PUBLISHED) )
        return failImport(QObject::tr("The imported database was installed, but its progress could not be saved. Restart to retry."));

    return true;
}

bool LogDatabase::finishImport(QJsonObject &record, ImportPhase phase, QJsonArray &incoming, bool decoded)
{
    FCT_IDENTIFICATION;

    const QString pending = pendingImportPath();
    const QString original = dbDirectory().filePath(IMPORT_BACKUP_FILENAME);
    const QString targetId = record.value("targetLogId").toString();
    const bool hadOriginal = record.value("hadOriginal").toBool();
    const bool requiresPassphrase = record.value("requiresPassphrase").toBool();
    CredentialStore *credentials = CredentialStore::instance();

    QString publishedId;
    const bool originalMatches = hadOriginal ? phase == ImportPhase::CREDENTIALS_RESTORED || QFile::exists(original)
                                             : !QFile::exists(original);

    if ( QFile::exists(pending)
         || !QFile::exists(dbFilename())
         || !readLogId(dbFilename(), publishedId)
         || publishedId != targetId
         || !originalMatches )
        return failImport(QObject::tr("The imported database does not match its recovery files. All files were preserved."));

    if ( !openDatabase() )
        return failImport(QObject::tr("The imported database could not be opened. Its recovery files were preserved."));

    if ( phase == ImportPhase::PUBLISHED )
    {
        if ( (!decoded && !decodeImportPasswords(requiresPassphrase, incoming)) || !credentials->restorePasswords(incoming) )
            return failImport(QObject::tr("The imported database was retained, but password restoration is incomplete. Restart QLog to retry."));

        QSet<QPair<QString, QString>> restored;

        for ( const QJsonValue &value : static_cast<const QJsonArray>(incoming) )
        {
            const QJsonObject entry = value.toObject();
            restored.insert(qMakePair(entry.value("storagekey").toString(), entry.value("username").toString()));
        }

        bool deleted = true;
        const QJsonArray prevJsonCred = record.value("previousCredentials").toArray();
        for ( const QJsonValue &value : prevJsonCred )
        {
            const QJsonObject previous = value.toObject();
            const QPair<QString, QString> identity(previous.value("storagekey").toString(), previous.value("username").toString());
            if ( !restored.contains(identity) && !credentials->deletePassword(identity.first, identity.second) )
                deleted = false;
        }
        if ( !deleted || !writeImportRecord(record, ImportPhase::CREDENTIALS_RESTORED) )
            return failImport(QObject::tr("The imported database was retained, but password replacement is incomplete. Restart QLog to retry."));
    }

    {
        QSqlDatabase db = QSqlDatabase::database();
        if ( !db.transaction() )
            return failImport(QObject::tr("The import is complete, but cleanup could not be started. Restart to retry."));

        if ( !LogParam::removeEncryptedPasswords() || !LogParam::removeSourcePlatform() || !db.commit() )
        {
            qCWarning(runtime) << "Cannot remove import metadata:" << db.lastError();
            if ( !db.rollback() )
                qCWarning(runtime) << "Cannot roll back import cleanup:" << db.lastError();

            LogParam::clearCache();

            return failImport(QObject::tr("The import is complete, but cleanup failed. Restart to retry."));
        }
    }
    if ( (requiresPassphrase && !credentials->deleteImportPassphrase())
         || QFile::exists(original + SQLITE_WAL_SUFFIX)
         || QFile::exists(original + SQLITE_SHM_SUFFIX)
         || (QFile::exists(original) && !QFile::remove(original))
         || !QFile::remove(dbDirectory().filePath(IMPORT_RECORD_FILENAME)) )
        return failImport(QObject::tr("The import is complete, but its recovery files could not be removed. Restart to retry."));

    return true;
}
