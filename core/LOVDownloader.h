#ifndef QLOG_CORE_LOVDOWNLOADER_H
#define QLOG_CORE_LOVDOWNLOADER_H

#include <QObject>
#include <QDate>
#include <QNetworkAccessManager>
#include <QSet>
#include <QStringList>

class LOVDatabase;

class LOVDownloader : public QObject
{
    Q_OBJECT

public:
    enum SourceType
    {
        CTY = 0,
        SATLIST = 1,
        SOTASUMMITS = 2,
        WWFFDIRECTORY = 3,
        IOTALIST = 4,
        POTADIRECTORY = 5,
        MEMBERSHIPCONTENTLIST = 6,
        CLUBLOGCTY = 7,
        UNDEF = 8
    };

public:
    LOVDownloader(QObject *parent = nullptr);
    ~LOVDownloader();
    void update(const SourceType &, bool force = false);

public slots:
    void abortRequest();

signals:
    void processingSize(qint64);
    void progress(qint64 count);
    void finished(bool result);
    void noUpdate();

public:
    class SourceDefinition
    {
    public:
        SourceDefinition(const SourceType type,
                         const QString &URL,
                         const QString &fileName,
                         const QString &lastTimeConfigName,
                         const QString &tableName,
                         int ageTime,
                         const QString &packageStem,
                         const QStringList &relatedTables = QStringList(),
                         const QStringList &optionalTables = QStringList()) : type(type),
                                        URL(URL),
                                        fileName(fileName),
                                        lastTimeConfigName(lastTimeConfigName),
                                        tableNames(QStringList({tableName}) + relatedTables + optionalTables),
                                        nonEmptyTables(QStringList({tableName}) + relatedTables),
                                        packageStem(packageStem),
                                        ageTime(ageTime) {};
        SourceDefinition() : type(LOVDownloader::UNDEF),
                                  ageTime(0){};
        SourceType type;
        QString URL;
        QString fileName;
        QString lastTimeConfigName;
        QStringList tableNames;
        QStringList nonEmptyTables;
        QString packageStem;
        int ageTime;
    };

    static QList<SourceType> sources();
    static SourceDefinition sourceDefinition(SourceType type);
    bool isSourceAvailable(SourceType type) const;
    void startUpdates();
    void stopUpdates();

private:
    void download(const SourceDefinition &source);
    void startWorker(const SourceDefinition &source, const QByteArray &payload,
                     const QString &inputFile, const QDate &date, bool cachePayload = false);
    void complete(bool success);
    void nextUpdate();
    void loadBundledCTY(const SourceDefinition &source);
    static QString directory();
    QNetworkAccessManager *nam;
    QNetworkReply *currentReply = nullptr;
    LOVDatabase *worker = nullptr;
    QSet<SourceType> attemptedSources;
    QList<SourceType> pendingSources;
    bool stopping = false;
    bool canceled = false;

private slots:
    void processReply(QNetworkReply *reply);
};

#endif // QLOG_CORE_LOVDOWNLOADER_H
