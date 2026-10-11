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

    // Process pending import (called at startup)
    // Returns true if import was successful or no import was pending
    bool processPendingImport(QString *backupSnapshot = nullptr);
    // Consumes a colocated temporary DB on success; other sources use atomic copying.
    bool preparePendingImport(const QString &sourceFile,
                              const QList<PlatformParameter> &parameters,
                              const QList<ProfileParameter> &profileParameters,
                              const QString &passphrase);
    bool cancelPendingImport();
    QString pendingImportError() const;

    bool atomicCopy(const QString &filename);
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
        ABORTED
    };
    static const QStringList IMPORT_PHASE_NAMES;

    LogDatabase();
    QString importError;
    bool failImport(const QString &message);
    bool prepareImportDatabase(QJsonObject &record, QJsonArray &incoming, QString *backupSnapshot);
    bool publishImportDatabase(QJsonObject &record, QJsonArray &incoming, bool decoded);
    bool finishImport(QJsonObject &record, ImportPhase phase, QJsonArray &incoming, bool decoded);
    static bool decodeImportPasswords(bool requiresPassphrase, QJsonArray &incoming);
    static bool readImportRecord(QJsonObject &record, ImportPhase &phase);
    static bool writeImportRecord(QJsonObject &record, ImportPhase phase);
    static bool checkpointDatabase(QSqlDatabase database);
    static bool readLogId(const QString &filename, QString &logId);
};

#endif // QLOG_CORE_LOGDATABASE_H
