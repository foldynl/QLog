#ifndef MIGRATION_H
#define MIGRATION_H

#include <QSqlQuery>
#include <QObject>
#include <QProgressDialog>
#include "core/LOVDownloader.h"

class QString;

class Migration : public QObject
{
    Q_OBJECT

public:
    Migration(QObject *parent = nullptr) : QObject(parent) {}

    bool run();
    bool functionMigration(int version);
private:
    bool migrate(int toVersion);
    int getVersion();
    bool setVersion(int version);

    bool runSqlFile(QString filename);


    int tableRows(QString name);

    bool updateExternalResource();
    bool updateExternalResourceProgress(QProgressDialog&,
                                        LOVDownloader&,
                                        const LOVDownloader::SourceType & sourceType,
                                        const QString &counter);
    bool fixIntlFields();
    bool insertUUID();
    bool fillMyDXCC();
    QString fixIntlField(QSqlQuery &query, const QString &columName, const QString &columnNameIntl);

    static const int latestVersion = 17;
};

#endif // MIGRATION_H
