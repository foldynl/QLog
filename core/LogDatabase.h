#ifndef QLOG_CORE_LOGDATABASE_H
#define QLOG_CORE_LOGDATABASE_H

#include <QString>
#include <QStringList>
#include <QDir>
#include <QJsonObject>
#include "PlatformParameterManager.h"

class QSqlDatabase;
class QJsonArray;

struct DatabaseInfo
{
    bool valid = false;
    int schemaVersion = 0;
    QString sourcePlatform;
    bool hasEncryptedPasswords = false;
    QString errorMessage;
};

class LogDatabase
{
public:

    static LogDatabase* instance()
    {
        static LogDatabase instance;
        return &instance;
    };

    static QString PLATFORM_WINDOWS;
    static QString PLATFORM_MACOS;
    static QString PLATFORM_LINUX;
    static QString PLATFORM_LINUXFLATPAK;

    static QDir dbDirectory();
    static QString dbFilename();
    static QString currentPlatformId();

    // Inspect a database file without opening it as main connection
    // Returns information about the database (schema version, platform, etc.)
    static DatabaseInfo inspectDatabase(const QString &filename);

    // Path where pending import database is stored
    static QString pendingImportPath();

    // Check if there is a pending import to process
    static bool hasPendingImport();

    enum class ImportResult
    {
        COMPLETE,
        NEEDS_ATTENTION,
        FAILED
    };
    enum class ImportAction
    {
        RETRY_PASSWORDS,
        FINISH_WITHOUT_PASSWORDS
    };

    // COMPLETE and NEEDS_ATTENTION leave the active database initialized for logging.
    ImportResult initialize(QString *backupSnapshot = nullptr);
    // Requires successful initialization; only retries credentials and import cleanup.
    ImportResult recoverImportPasswords(ImportAction action);
    // Consumes a colocated temporary DB on success; other sources use atomic copying.
    bool preparePendingImport(const QString &sourceFile,
                              const QList<PlatformParameter> &parameters,
                              const QList<ProfileParameter> &profileParameters,
                              const QString &passphrase);
    bool cancelPendingImport();
    bool canCancelPendingImport() const;
    QString pendingImportError() const;

    bool atomicCopy(const QString &filename, QString *error = nullptr);
    bool openDatabase(const QString &filename = dbFilename());
    bool schemaVersionUpgrade(QString *backupSnapshot = nullptr);
    bool createSQLFunctions();

private:
    static const QString DATABASE_FILENAME;
    static const QString IMPORT_DATABASE_FILENAME;
    static const QString IMPORT_RECORD_FILENAME;
    static const QString IMPORT_BACKUP_FILENAME;
    static const QString SQLITE_WAL_SUFFIX;
    static const QString SQLITE_SHM_SUFFIX;

    // Keep the order aligned with IMPORT_PHASE_NAMES; persisted phases remain strings.
    enum class ImportPhase
    {
        INVALID = -1,
        PREPARED,
        READY,
        PUBLISHED,
        CREDENTIALS_RESTORED,
        ABORTED,
        CREDENTIALS_SKIPPED
    };
    static const QStringList IMPORT_PHASE_NAMES;

    LogDatabase();
    QString importError;
    bool failImport(const QString &message);
    bool prepareImportDatabase(QJsonObject &record, QJsonArray &incoming, QString *backupSnapshot);
    bool publishImportDatabase(QJsonObject &record, QJsonArray &incoming, bool preparedThisStartup, QString *backupSnapshot);
    bool validatePublishedImport(const QJsonObject &record, ImportPhase phase);
    ImportResult finishImport(QJsonObject &record, ImportPhase phase, ImportAction action,
                              const QJsonArray *incoming = nullptr);
    static bool decodeImportPasswords(bool requiresPassphrase, QJsonArray &incoming);
    static bool readImportRecord(QJsonObject &record, ImportPhase &phase, QString *reason = nullptr);
    bool writeImportRecord(QJsonObject &record, ImportPhase phase);
    bool checkpointDatabase(QSqlDatabase database);
    static bool readLogId(const QString &filename, QString &logId, QString *reason = nullptr);
    static bool canCancelImport(const QJsonObject &record, ImportPhase phase, bool &currentIsImport);
};

#endif // QLOG_CORE_LOGDATABASE_H
