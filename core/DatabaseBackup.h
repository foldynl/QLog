#ifndef QLOG_CORE_DATABASEBACKUP_H
#define QLOG_CORE_DATABASEBACKUP_H

#include <QDate>
#include <QStringList>
#include <QThread>

class QDir;

struct BackupConfig
{
    bool enabled = false;
    QString directory;
    int copies = 10;
};

class DatabaseBackup : public QThread
{
    Q_OBJECT

public:
    explicit DatabaseBackup(const BackupConfig &config, const QString &snapshotName = QString(), QObject *parent = nullptr);
    ~DatabaseBackup() override;

    static bool prepare(bool force = false, QString *snapshotName = nullptr);

protected:
    void run() override;

private:
    static constexpr uint MAX_INTERNAL_COPIES = 10;
    bool exportSnapshot(const QString &snapshot, const QString &output);
    bool copyBackup(const QString &source, const QString &destination);
    bool copyAdditionalBackups(const QDir &directory, const QStringList &requestedFiles);
    static bool removeSnapshotSidecars(const QString &snapshot);
    void pruneBackups(const QString &path, int copies);
    void complete();
    static QStringList backupFiles(const QDir &directory, const QString &suffix);
    static void sortBackups(QStringList &files);

    BackupConfig config;
    QString priorityBackup;
    QDate backupDate;
    QString error;
};

#endif // QLOG_CORE_DATABASEBACKUP_H
