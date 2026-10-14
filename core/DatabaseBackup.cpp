#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QRegularExpression>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>
#include <algorithm>
#include <sqlite3.h>

#include "DatabaseBackup.h"
#include "core/LogDatabase.h"
#include "core/LogParam.h"
#include "logformat/AdxFormat.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.databasebackup");

DatabaseBackup::DatabaseBackup(const BackupConfig &config, const QString &snapshotName, QObject *parent) :
    QThread(parent),
    config(config)
{
    FCT_IDENTIFICATION;

    if ( !snapshotName.isEmpty() )
        priorityBackup = snapshotName.left(snapshotName.indexOf(QLatin1Char('.'))) + QStringLiteral(".adx");

    error = LogParam::getBackupError();
    connect(this, &QThread::finished, this, &DatabaseBackup::complete);
}

DatabaseBackup::~DatabaseBackup()
{
    FCT_IDENTIFICATION;

    requestInterruption();
    wait();
}

bool DatabaseBackup::prepare(bool force, QString *snapshotName)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << force;

    if ( snapshotName )
        snapshotName->clear();

    // Opening a new database creates its file before the initial migration.
    if ( !QSqlDatabase::database().tables().contains(QStringLiteral("contacts")) )
        return true;

    if ( !LogParam::setBackupError(QString()) )
        qCWarning(runtime) << "Cannot clear backup status";

    const QDir directory(LogDatabase::dbDirectory());

    if ( !force )
    {
        QDate lastBackupDate = LogParam::getLastBackupDate();
        const QDate today = QDate::currentDate();

        if ( lastBackupDate > today )
            lastBackupDate = QDate();

        QStringList completed = backupFiles(directory, QStringLiteral(".adx"));
        // ADX next to an unfinished database copy is not a completed backup.
        for ( int index = completed.size() - 1; index >= 0; --index )
        {
            const QString &name = completed.at(index);
            const QString stem = directory.filePath(name.left(name.size() - 4));

            if ( QFileInfo::exists(stem + QStringLiteral(".db"))
                 || QFileInfo::exists(stem + QStringLiteral(".migration.db")) )
                completed.removeAt(index);
        }
        // New ADX files are published atomically, even if the completion notification is lost.
        if ( !completed.isEmpty()
             && completed.last().size() == QStringLiteral("qlog_backup_yyyyMMddHHmmsszzz.adx").size() )
        {
            const QDate completedDate = QDate::fromString(completed.last().mid(12, 8), QStringLiteral("yyyyMMdd"));

            if ( completedDate <= today && completedDate > lastBackupDate )
            {
                lastBackupDate = completedDate;
                if ( !LogParam::setLastBackupDate(completedDate) )
                    qCWarning(runtime) << "Cannot restore the successful backup date";
            }
        }
#ifndef QLOG_FORCE_AUTO_BACKUP
        if ( lastBackupDate.isValid() && lastBackupDate.addDays(7) > today )
            return true;

        const QString todayPrefix = QStringLiteral("qlog_backup_") + QDate::currentDate().toString("yyyyMMdd");
        for ( const QString &name : completed )
        {
            if ( name.startsWith(todayPrefix) )
                return true;
        }
#endif
    }

    QDateTime timestamp = QDateTime::currentDateTime();
    QString stem;
    QString filename;

    do
    {
        stem = QStringLiteral("qlog_backup_") + timestamp.toString("yyyyMMddHHmmsszzz");
        filename = stem + (force ? QStringLiteral(".migration.db") : QStringLiteral(".db"));
        timestamp = timestamp.addMSecs(1);
    } while ( !directory.entryList({stem + QLatin1Char('*')}, QDir::Files).isEmpty() );

    // An interrupted copy must never be mistaken for a complete snapshot.
    const QString temporaryName = filename + QStringLiteral(".tmp");

    if ( !LogDatabase::instance()->atomicCopy(temporaryName)
         || !QFile::rename(directory.filePath(temporaryName), directory.filePath(filename)) )
    {
        qCWarning(runtime) << "Cannot prepare backup snapshot" << filename;

        QFile::remove(directory.filePath(temporaryName));

        if ( !LogParam::setBackupError(tr("Could not create the backup. Your previous backups have been kept.")) )
            qCWarning(runtime) << "Cannot save backup status";

        return false;
    }

    if ( snapshotName )
        *snapshotName = filename;

    // Retire old ordinary snapshots only after a replacement is complete.
    const QStringList pending = backupFiles(directory, QStringLiteral(".db"));
    uint ordinaryCount = force ? 0 : 1;
    bool cleaned = true;

    for ( int index = pending.size() - 1; index >= 0; --index )
    {
        const QString &name = pending.at(index);

        if ( name == filename || name.endsWith(QStringLiteral(".migration.db")) )
            continue;

        if ( ++ordinaryCount <= MAX_INTERNAL_COPIES )
            continue;

        const QString snapshot = directory.filePath(name);
        // Keep a non-empty WAL together with its snapshot for recovery.
        if ( QFileInfo(snapshot + QStringLiteral("-wal")).size() > 0 || !QFile::remove(snapshot) )
        {
            qCWarning(runtime) << "Cannot retire pending backup snapshot" << snapshot;
            cleaned = false;
            continue;
        }

        if ( !removeSnapshotSidecars(snapshot) )
            cleaned = false;
    }

    if ( !cleaned && !LogParam::setBackupError(tr("Could not remove older pending backup files. The new database copy has been kept.")) )
        qCWarning(runtime) << "Cannot save backup status";

    return true;
}

void DatabaseBackup::run()
{
    FCT_IDENTIFICATION;

    const QDir directory(LogDatabase::dbDirectory());

    for ( const QString &name : backupFiles(directory, QStringLiteral(".db.tmp")) )
    {
        if ( isInterruptionRequested() )
            return;

        if ( !QFile::remove(directory.filePath(name)) )
            qCWarning(runtime) << "Cannot remove interrupted database copy" << name;
    }

    // Clean leftovers from earlier starts before a long export can be interrupted again.
    for ( const QString &suffix : {QStringLiteral(".db-wal"), QStringLiteral(".db-shm"),
                                   QStringLiteral(".db.ready-wal"), QStringLiteral(".db.ready-shm"),
                                   QStringLiteral(".db.tmp-wal"), QStringLiteral(".db.tmp-shm")} )
    {
        for ( const QString &name : backupFiles(directory, suffix) )
        {
            if ( isInterruptionRequested() )
                return;

            if ( !removeSnapshotSidecars(directory.filePath(name.left(name.size() - 4))) && error.isEmpty() )
                error = tr("Could not remove unused backup files.");
        }
    }

    QStringList snapshots = backupFiles(directory, QStringLiteral(".db"));
    snapshots.append(backupFiles(directory, QStringLiteral(".db.ready")));
    sortBackups(snapshots);

    // The copy made at this startup takes priority even if the clock changes again.
    if ( !priorityBackup.isEmpty() )
    {
        const QString prefix = priorityBackup.left(priorityBackup.size() - 4) + QLatin1Char('.');
        for ( int index = 0; index < snapshots.size(); ++index )
        {
            if ( snapshots.at(index).startsWith(prefix) )
            {
                snapshots.append(snapshots.takeAt(index));
                break;
            }
        }
    }

    QStringList processedExports;

    for ( int index = snapshots.size() - 1; index >= 0; --index )
    {
        if ( isInterruptionRequested() )
            return;

        const QString &name = snapshots.at(index);
        const bool ready = name.endsWith(QStringLiteral(".ready"));
        const bool migration = name.contains(QStringLiteral(".migration."));
        const QString stem = name.left(name.indexOf(QLatin1Char('.')));

        const QString snapshot = directory.filePath(name);
        const QString outputName = stem + QStringLiteral(".adx");
        const QString output = directory.filePath(outputName);

        if ( !exportSnapshot(snapshot, output) )
            continue;

        QDate date = QDate::fromString(stem.mid(12, 8), QStringLiteral("yyyyMMdd"));
        const QDate today = QDate::currentDate();

        if ( outputName == priorityBackup && date > today )
            date = today;

        if ( date <= today && date > backupDate )
            backupDate = date;

        processedExports.append(outputName);

        if ( config.enabled && !copyAdditionalBackups(directory, {outputName}) )
            continue;

        if ( isInterruptionRequested() )
            return;

        // Preserve the full pre-migration database until another successful startup.
        const bool cleaned = migration && !ready ? QFile::rename(snapshot, snapshot + QStringLiteral(".ready"))
                                                 : QFile::remove(snapshot);
        if ( !cleaned )
            qCWarning(runtime) << "Cannot retire backup snapshot" << snapshot;
        else if ( !removeSnapshotSidecars(snapshot) && error.isEmpty() )
            error = tr("Could not remove unused backup files.");
    }

    if ( config.enabled && !isInterruptionRequested() )
    {
        QStringList remaining = backupFiles(directory, QStringLiteral(".adx"));

        for ( const QString &name : processedExports )
            remaining.removeAll(name);

        copyAdditionalBackups(directory, remaining);
    }
    if ( backupDate.isValid() && !isInterruptionRequested() )
        pruneBackups(directory.absolutePath(), MAX_INTERNAL_COPIES);
}

bool DatabaseBackup::removeSnapshotSidecars(const QString &snapshot)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << snapshot;

    // Read-only WAL connections can leave sidecars after closing. Only empty, orphaned WALs are disposable.
    if ( QFileInfo::exists(snapshot) || QFileInfo(snapshot + QStringLiteral("-wal")).size() > 0 )
        return true;

    bool cleaned = true;
    for ( const QString &suffix : {QStringLiteral("-wal"), QStringLiteral("-shm")} )
    {
        const QString path = snapshot + suffix;
        if ( QFileInfo::exists(path) && !QFile::remove(path) )
        {
            qCWarning(runtime) << "Cannot remove unused backup file" << path;
            cleaned = false;
        }
    }
    return cleaned;
}

bool DatabaseBackup::copyAdditionalBackups(const QDir &directory, const QStringList &requestedFiles)
{
    FCT_IDENTIFICATION;

    if ( requestedFiles.isEmpty() )
        return true;

    QStringList backups = backupFiles(directory, QStringLiteral(".adx"));

    if ( backups.removeOne(priorityBackup) )
        backups.append(priorityBackup);

    QStringList files;

    for ( qint64 index = qMax<qint64>(0, backups.size() - qMax(1, config.copies)); index < backups.size(); ++index )
        if ( requestedFiles.contains(backups.at(index)) )
            files.append(backups.at(index));

    if ( files.isEmpty() )
        return true;

    if ( config.directory.isEmpty() || !QDir().mkpath(config.directory) )
    {
        qCWarning(runtime) << "Cannot access additional backup folder" << config.directory;

        if ( error.isEmpty() )
            error = tr("Could not save the additional backup. Check the backup folder. The internal backup has been kept.");
        return false;
    }

    const QDir destinationDirectory(config.directory);
    // A symlink to the internal folder must not apply external retention there.
    if ( destinationDirectory == directory )
        return true;

    bool saved = false;
    bool succeeded = true;

    for ( int index = files.size() - 1; index >= 0; --index )
    {
        if ( isInterruptionRequested() )
            return false;

        const QString source = directory.filePath(files.at(index));
        const QString destination = destinationDirectory.filePath(files.at(index));

        if ( copyBackup(source, destination) )
            saved = true;
        else if ( !isInterruptionRequested() )
        {
            qCWarning(runtime) << "Cannot copy additional backup to" << destination;
            succeeded = false;
            if ( error.isEmpty() )
                error = tr("Could not save the additional backup. Check the backup folder. The internal backup has been kept.");
        }
    }

    if ( saved )
        pruneBackups(config.directory, config.copies);

    return succeeded && !isInterruptionRequested();
}

bool DatabaseBackup::exportSnapshot(const QString &snapshot, const QString &output)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << snapshot << output;

    bool exported = false;
    const QString connectionName = QStringLiteral("BackupExport");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        // Allow WAL sidecar creation without creating a missing snapshot; query_only below protects its data.
        QUrl snapshotUrl = QUrl::fromLocalFile(snapshot);
        snapshotUrl.setQuery(QStringLiteral("mode=rw"));
        database.setDatabaseName(QString::fromUtf8(snapshotUrl.toEncoded()));
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_URI"));

        if ( database.open() )
        {
            // Cancel even a long SELECT when the application is closing.
            QVariant handle = database.driver()->handle();
            sqlite3 *sqlite = handle.isValid() && qstrcmp(handle.typeName(), "sqlite3*") == 0 ? *static_cast<sqlite3 **>(handle.data())
                                                                                              : nullptr;
            if ( sqlite )
                sqlite3_progress_handler(sqlite, 1000, [](void *context)
                {
                    return static_cast<DatabaseBackup *>(context)->isInterruptionRequested() ? 1 : 0;
                }, this);

            QSqlQuery query(database);
            query.setForwardOnly(true);
            QSaveFile file(output);
            bool xmlError = false;

            if ( query.exec(QStringLiteral("PRAGMA query_only=ON"))
                 && query.exec(QStringLiteral("SELECT * FROM contacts ORDER BY start_time ASC"))
                 && file.open(QIODevice::WriteOnly) )
            {
                QTextStream stream(&file);
                AdxFormat format(stream);
                format.exportStart();

                while ( !isInterruptionRequested() && query.next() )
                {
                    format.exportContact(query.record());
                    if ( file.error() != QFileDevice::NoError )
                        break;
                }

                xmlError = format.hasExportError();
                format.exportEnd();
                stream.flush();

                if ( !isInterruptionRequested() && !xmlError && !query.lastError().isValid()
                     && stream.status() == QTextStream::Ok )
                    exported = file.commit();
                else
                    file.cancelWriting();
            }

            if ( !exported && !isInterruptionRequested() )
                qCWarning(runtime) << "Backup export failed" << query.lastError() << file.errorString()
                                   << "XML error:" << xmlError;
            if ( sqlite )
                sqlite3_progress_handler(sqlite, 0, nullptr, nullptr);
        }
        else
            qCWarning(runtime) << "Cannot open backup snapshot" << database.lastError();
    }
    QSqlDatabase::removeDatabase(connectionName);

    if ( !exported && !isInterruptionRequested() && error.isEmpty() )
        error = tr("Could not export the backup to ADX. The database copy and previous backups have been kept.");

    return exported;
}

bool DatabaseBackup::copyBackup(const QString &source, const QString &destination)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << source << destination;

    QFile input(source);

    if ( !input.open(QIODevice::ReadOnly) )
        return false;

    {
        QFile existing(destination);
        if ( existing.open(QIODevice::ReadOnly) && existing.size() == input.size() )
        {
            bool identical = true;
            while ( !input.atEnd() )
            {
                if ( isInterruptionRequested() )
                    return false;

                const QByteArray block = input.read(64 * 1024);
                if ( block.isEmpty() )
                    return false;

                if ( block != existing.read(block.size()) )
                {
                    identical = false;
                    break;
                }
            }
            if ( identical && existing.atEnd() && input.error() == QFileDevice::NoError
                 && existing.error() == QFileDevice::NoError )
                return !isInterruptionRequested();

            if ( !input.seek(0) )
                return false;
        }
    }

    QSaveFile output(destination);
    if ( !output.open(QIODevice::WriteOnly) )
        return false;

    QByteArray buffer;
    buffer.resize(64 * 1024);

    while ( !input.atEnd() )
    {
        if ( isInterruptionRequested() )
            return false;

        const qint64 size = input.read(buffer.data(), buffer.size());

        if ( size <= 0 || output.write(buffer.constData(), size) != size )
            return false;
    }

    return !isInterruptionRequested()
            && input.error() == QFileDevice::NoError
            && output.commit();
}

void DatabaseBackup::pruneBackups(const QString &path, int copies)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << path << copies;

    const QDir directory(path);
    const QStringList files = backupFiles(directory, QStringLiteral(".adx"));
    int remaining = files.size() - qMax(1, copies);

    for ( const QString &name : files )
    {
        if ( remaining <= 0 )
            return;

        if ( name == priorityBackup )
            continue;

        if ( isInterruptionRequested() )
            return;

        if ( !QFile::remove(directory.filePath(name)) )
        {
            qCWarning(runtime) << "Cannot remove old backup" << name;
            if ( error.isEmpty() )
                error = tr("The backup was saved, but an older backup could not be removed.");
        }
        else
            --remaining;
    }
}

QStringList DatabaseBackup::backupFiles(const QDir &directory, const QString &suffix)
{
    FCT_IDENTIFICATION;

    const QString timestamp = suffix.startsWith(QStringLiteral(".db")) ? QStringLiteral("\\d{17}(\\.migration)?")
                                                                       : QStringLiteral("\\d{8}(\\d{6}(\\d{3})?)?");
    const QRegularExpression pattern(QStringLiteral("^qlog_backup_") + timestamp
                                     + QRegularExpression::escape(suffix) + QLatin1Char('$'));
    QStringList files;

    for ( const QString &name : directory.entryList({QStringLiteral("qlog_backup_*") + suffix}, QDir::Files, QDir::Unsorted) )
        if ( pattern.match(name).hasMatch() )
            files.append(name);

    sortBackups(files);
    return files;
}

void DatabaseBackup::sortBackups(QStringList &files)
{
    FCT_IDENTIFICATION;

    // A clock correction must not give future-dated files priority over current backups.
    const QString today = QDate::currentDate().toString(QStringLiteral("yyyyMMdd"));
    std::sort(files.begin(), files.end(), [&today](const QString &left, const QString &right)
    {
        const bool leftFuture = left.mid(12, 8) > today;
        const bool rightFuture = right.mid(12, 8) > today;
        return leftFuture != rightFuture ? leftFuture : left < right;
    });
}

void DatabaseBackup::complete()
{
    FCT_IDENTIFICATION;

    // The live database and LogParam cache belong to the main thread.
    const QDate lastBackupDate = LogParam::getLastBackupDate();

    if ( backupDate.isValid() && (backupDate > lastBackupDate || lastBackupDate > QDate::currentDate())
         && !LogParam::setLastBackupDate(backupDate) )
    {
        qCWarning(runtime) << "Cannot save the successful backup date";
        error = tr("The backup was saved, but its date could not be recorded.");
    }

    if ( !LogParam::setBackupError(error) )
        qCWarning(runtime) << "Cannot save backup status";
}
