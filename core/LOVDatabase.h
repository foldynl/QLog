#ifndef QLOG_CORE_LOVDATABASE_H
#define QLOG_CORE_LOVDATABASE_H

#include <QThread>
#include <QSqlDatabase>
#include "LOVDownloader.h"

namespace csv
{
class CSVFormat;
}
class QTextStream;

class LOVDatabase : public QThread
{
    Q_OBJECT
public:
    struct Job
    {
        LOVDownloader::SourceDefinition source;
        QString directory;
        QByteArray payload;
        QString inputFile;
        QStringList schemaStatements;
        QDate downloadDate;
        bool cachePayload = false;
    };

    enum class ApplyResult
    {   NO_PACKAGE,
        APPLIED,
        RETRY,
        INVALID,
        DATABASE_ERROR
    };

    explicit LOVDatabase(const Job &job, QObject *parent = nullptr);
    ~LOVDatabase() override;
    // Results are read after the worker has finished.
    bool succeeded() const { return success; }
    QString errorString() const { return success ? QString() : tr("Could not prepare the reference list."); }
    static bool discardPrepared(const QString &directory);
    // Adoption uses the main connection before models and shared caches are initialized.
    static ApplyResult applyReady(const LOVDownloader::SourceDefinition &source,
                                  const QString &directory, QString *error = nullptr);

signals:
    void processingSize(qint64 size);
    void progress(qint64 count);

protected:
    void run() override;

private:
    bool buildPackage(QSqlDatabase &database, const QByteArray &data);
    bool parseData(QSqlDatabase &database, QTextStream &data);
    bool parseCTY(QSqlDatabase &database, QTextStream &data);
    bool parseDelimited(QSqlDatabase &database, QTextStream &data, QChar delimiter);
    bool parseIOTA(QSqlDatabase &database, QTextStream &data);
    bool parseClubLogCTY(QSqlDatabase &database, QTextStream &data);
    bool parseCSVGeneric(QSqlDatabase &database, QTextStream &data,
                         const QString &insertSQL, const QStringList &columns,
                         csv::CSVFormat format, const QString &header = QString());
    static bool execSql(QSqlDatabase &database, const QString &sql);
    static QStringList tableColumns(QSqlDatabase &database, const QString &schema, const QString &table);
    static bool validatePackage(QSqlDatabase &database, const LOVDownloader::SourceDefinition &source,
                                QDate &downloadDate, QStringList &columnLists);
    static bool removePackage(const QString &filename);
    bool isCSVComplete(const QByteArray &text, const csv::CSVFormat &format) const;

    const Job job;
    bool success = false;
};

#endif // QLOG_CORE_LOVDATABASE_H
