#ifndef MIGRATION_H
#define MIGRATION_H

#include <QSqlQuery>

class QString;

class Migration {
public:
    Migration() {}

    bool run();
bool functionMigration(int version);
private:
    bool migrate(int toVersion);
    int getVersion();
    bool setVersion(int version);

    bool runSqlFile(QString filename);


    int tableRows(QString name);

    bool updateExternalResource();
    bool fixIntlFields();
    bool insertUUID();
    QString fixIntlField(QSqlQuery &query, const QString &columName, const QString &columnNameIntl);

    static const int latestVersion = 10;
};

#endif // MIGRATION_H
