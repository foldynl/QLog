#ifndef QLOG_CORE_MIGRATION_H
#define QLOG_CORE_MIGRATION_H

#include "core/LOVDownloader.h"

class QProgressDialog;

class DBSchemaMigration : public QObject
{
    Q_OBJECT

public:
    DBSchemaMigration(QObject *parent = nullptr) : QObject(parent) {}
    bool run(bool force = false);
    static bool backupAllQSOsToADX(bool force = false);

    static constexpr int latestVersion = 38;

private:
    bool functionMigration(int version);
    bool migrate(int toVersion);
    int getVersion();
    bool setVersion(int version);
    bool runSqlFile(QString filename);
    int tableRows(const QString &name);
    bool updateExternalResource(bool force = false);
    void updateExternalResourceProgress(QProgressDialog&,
                                        LOVDownloader&,
                                        const LOVDownloader::SourceType & sourceType,
                                        const QString &counter,
                                        bool force = false);
    bool fixIntlFields();
    bool insertUUID();
    bool fillMyDXCC();
    bool createTriggers();
    bool importQSLCards2DB();
    bool fillCQITUZStationProfiles();
    bool resetConfigs();
    bool profiles2DB();
    bool settings2DB();
    bool removeSettings2DB();
    bool setSelectedProfile(const QString &tablename, const QString &profileName);
    QString fixIntlField(QSqlQuery &query, const QString &columName, const QString &columnNameIntl);
    bool refreshUploadStatusTrigger();

    friend class MigrationSqlTest_FriendAccessor;
};

#endif // QLOG_CORE_MIGRATION_H
