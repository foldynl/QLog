#ifndef QLOG_CORE_LOGDATABASE_H
#define QLOG_CORE_LOGDATABASE_H

#include <QString>
#include <QDir>

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

    // Returns true if the last processPendingImport() failed to import passwords
    // (passwords were deleted but could not be restored from the encrypted store)
    bool hadPasswordImportWarning() const;

    bool atomicCopy(const QString &filename);
    bool openDatabase();
    bool schemaVersionUpgrade(bool force = false, QString *backupSnapshot = nullptr);
    bool createSQLFunctions();

private:
    LogDatabase();
    bool passwordImportWarning = false;
};

#endif // QLOG_CORE_LOGDATABASE_H
