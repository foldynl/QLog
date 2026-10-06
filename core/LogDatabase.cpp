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

#include "LogDatabase.h"
#include "core/debug.h"
#include "core/Migration.h"
#include "core/LogParam.h"
#include "core/CredentialStore.h"
#include "core/PlatformParameterManager.h"

MODULE_IDENTIFICATION("qlog.core.logdatabase");

QString LogDatabase::PLATFORM_WINDOWS = "Windows";
QString LogDatabase::PLATFORM_MACOS = "MacOS";
QString LogDatabase::PLATFORM_LINUX = "Linux";
QString LogDatabase::PLATFORM_LINUXFLATPAK = "LinuxFlatpak";

QDir LogDatabase::dbDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
}

QString LogDatabase::dbFilename()
{
    return dbDirectory().filePath("qlog.db");
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

bool LogDatabase::hadPasswordImportWarning() const
{
    return passwordImportWarning;
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
    sqlite3_create_function(db_handle,
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
    sqlite3_create_collation(db_handle,
                             "LOCALEAWARE",
                             SQLITE_UTF16,
                             nullptr,
                             [](void *, int ll, const void * l, int rl, const void * r) {
                                const QString &left = QString::fromUtf16(reinterpret_cast<const char16_t *>(l), ll/2);
                                const QString &right = QString::fromUtf16(reinterpret_cast<const char16_t *>(r), rl/2);
                                return QString::localeAwareCompare(left, right); // controlled by LC_COLLATE
                             });

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

bool LogDatabase::openDatabase()
{
    FCT_IDENTIFICATION;

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbFilename());
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

bool LogDatabase::schemaVersionUpgrade(bool force, QString *backupSnapshot)
{
    FCT_IDENTIFICATION;

    DBSchemaMigration m;
    return m.run(force, backupSnapshot);
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

        if ( !db.open() )
        {
            info.errorMessage = db.lastError().text();
            qWarning() << "Cannot open database for inspection:" << info.errorMessage;
            break;
        }

        // Check if it's a valid QLog database by looking for schema_versions table
        QSqlQuery query(db);
        if ( !query.exec("SELECT version FROM schema_versions ORDER BY version DESC LIMIT 1") )
        {
            info.errorMessage = QObject::tr("Not a valid QLog database");
            db.close();
            break;
        }

        info.schemaVersion = ( query.first() ) ? query.value(0).toInt() : 0;

        // Check if schema version is too new
        if ( info.schemaVersion > DBSchemaMigration::latestVersion )
        {
            info.errorMessage = QObject::tr("Database version too new (requires newer QLog version)");
            db.close();
            break;
        }

        // Read source platform and encrypted passwords from log_param
        if ( query.exec("SELECT value FROM log_param WHERE name = 'sourceplatform'") )
        {
            if ( query.first() )
                info.sourcePlatform = query.value(0).toString();

            if ( info.sourcePlatform.isEmpty() )
            {
                info.errorMessage = QObject::tr("Database is not QLog Export file");
                db.close();
                break;
            }
        }

        if ( query.exec("SELECT value FROM log_param WHERE name = 'security/encryptedpasswords'") )
        {
            if ( query.first() && !query.value(0).toString().isEmpty() )
                info.hasEncryptedPasswords = true;
        }

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
    return dbDirectory().filePath("qlog.db.pending");
}

bool LogDatabase::hasPendingImport()
{
    FCT_IDENTIFICATION;

    bool exists = QFile::exists(pendingImportPath());
    qCDebug(runtime) << "Pending import exists:" << exists;
    return exists;
}

bool LogDatabase::processPendingImport(QString *backupSnapshot)
{
    FCT_IDENTIFICATION;

    const QString pendingPath = pendingImportPath();

    if ( !QFile::exists(pendingPath) )
    {
        qCDebug(runtime) << "No pending import";
        return true;
    }

    qCDebug(runtime) << "Processing pending database import";

    const QString currentDbPath = dbFilename();
    const QString walPath = currentDbPath + "-wal"; // remove also support files
    const QString shmPath = currentDbPath + "-shm"; // remove also support files

    if ( QFile::exists(currentDbPath) )
    {
        if ( !QFile::remove(currentDbPath) )
        {
            qWarning() << "Cannot remove current database:" << currentDbPath;
            return false;
        }
    }

    if ( QFile::exists(walPath) )
        QFile::remove(walPath);

    if ( QFile::exists(shmPath) )
        QFile::remove(shmPath);

    // Rename pending to current
    if ( !QFile::rename(pendingPath, currentDbPath) )
    {
        qWarning() << "Cannot rename pending database to current";
        return false;
    }

    qCDebug(runtime) << "Pending database moved to current";

    if ( !openDatabase() )
    {
        qCritical() << "Cannot open imported database";
        return false;
    }

    if ( !schemaVersionUpgrade(false, backupSnapshot) )
    {
        qCritical() << "Schema migration failed";
        return false;
    }

    const QString passphrase = CredentialStore::instance()->getImportPassphrase();
    if ( !passphrase.isEmpty() )
    {
        qCDebug(runtime) << "Importing passwords from encrypted store";

        CredentialStore::instance()->deleteAllPasswords();

        if ( !CredentialStore::instance()->importPasswords(passphrase) )
        {
            qCWarning(runtime) << "Password import failed";
            passwordImportWarning = true;
        }

        CredentialStore::instance()->deleteImportPassphrase();
    }

    const QString paramsPath = PlatformParameterManager::pendingParametersPath();
    if ( QFile::exists(paramsPath) )
    {
        qCDebug(runtime) << "Applying platform-specific parameters";

        QList<PlatformParameter> params = PlatformParameterManager::loadParametersFromFile(paramsPath);
        PlatformParameterManager::applyParameters(params);

        QList<ProfileParameter> profileParams = PlatformParameterManager::loadProfileParametersFromFile(paramsPath);
        PlatformParameterManager::applyProfileParameters(profileParams);

        QFile::remove(paramsPath);
    }

    // For Flatpak target: apply fixed paths (TQSL, rigctld_path)
    // This overrides any imported values with Flatpak-specific paths
    PlatformParameterManager::applyFlatpakFixedPaths();

    LogParam::removeEncryptedPasswords();
    LogParam::removeSourcePlatform();

    // Generate new LogID because we want to uniquie identified every log
    QString newLogId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    LogParam::setLogID(newLogId);
    qCDebug(runtime) << "New LogID generated:" << newLogId;

    qCDebug(runtime) << "Database import completed successfully";

    return true;
}

