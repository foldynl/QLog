#ifndef QLOG_CORE_LOVDOWNLOADER_H
#define QLOG_CORE_LOVDOWNLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QRegularExpression>
#include "service/clublog/ClubLog.h"

namespace csv
{
class CSVFormat;
}

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

private:
    class SourceDefinition
    {
    public:
        SourceDefinition(const SourceType type,
                         const QString &URL,
                         const QString &fileName,
                         const QString &lastTimeConfigName,
                         const QString &tableName,
                         int ageTime) : type(type),
                                        URL(URL),
                                        fileName(fileName),
                                        lastTimeConfigName(lastTimeConfigName),
                                        tableName(tableName),
                                        ageTime(ageTime) {};
        SourceDefinition() : type(LOVDownloader::UNDEF),
                                  ageTime(0){};
        SourceType type;
        QString URL;
        QString fileName;
        QString lastTimeConfigName;
        QString tableName;
        int ageTime;
    };

    QMap<SourceType, SourceDefinition> sourceMapping = {
        {CTY, SourceDefinition(CTY,
                               "https://www.country-files.com/bigcty/cty.csv",
                               "cty.csv",
                               "LOV/last_cty_update",
                               "dxcc_entities_ad1c",
                               7)},
        {CLUBLOGCTY, SourceDefinition(CLUBLOGCTY,
                               ClubLogBase::getCTYUrl(),
                               "clublog_cty.xml",
                               "LOV/last_clublogcty_update",
                               "dxcc_entities_clublog",
                               7)},
        {SATLIST, SourceDefinition(SATLIST,
                                   "https://raw.githubusercontent.com/foldynl/hamradio-value-lists/main/lists/satellites/satslist.csv",
                                   "satslist.csv",
                                   "LOV/last_sat_update",
                                   "sat_info",
                                   10)},
        {SOTASUMMITS, SourceDefinition(SOTASUMMITS,
                                   "https://raw.githubusercontent.com/foldynl/hamradio-value-lists/main/lists/SOTA/summitslist.csv.gz",
                                   "summitslist.csv.gz",
                                   "LOV/last_sotasummits_update",
                                   "sota_summits",
                                   30)},
        {WWFFDIRECTORY, SourceDefinition(WWFFDIRECTORY,
                                   "https://raw.githubusercontent.com/foldynl/hamradio-value-lists/main/lists/WWFF/wwff_directory.csv.gz",
                                   "wwff_directory.csv.gz",
                                   "LOV/last_wwffdirectory_update",
                                   "wwff_directory",
                                   21)},
        {IOTALIST, SourceDefinition(IOTALIST,
                                   "https://www.iota-world.org/islands-on-the-air/downloads/download-file.html?path=groups.json",
                                   "groups.json",
                                   "LOV/last_iota_update",
                                   "iota",
                                   30)},
        {POTADIRECTORY, SourceDefinition(POTADIRECTORY,
                                   "https://raw.githubusercontent.com/foldynl/hamradio-value-lists/main/lists/POTA/all_parks_ext.csv.gz",
                                   "all_parks_ext.csv.gz",
                                   "LOV/last_pota_update",
                                   "pota_directory",
                                   30)},
        {MEMBERSHIPCONTENTLIST, SourceDefinition(MEMBERSHIPCONTENTLIST,
                                   "https://raw.githubusercontent.com/foldynl/hamradio-membeship-lists/main/lists/content.csv",
                                   "content.csv",
                                   "LOV/last_membershipcontent_update",
                                   "membership_directory",
                                   7)}
    };


    QNetworkAccessManager* nam;
    QNetworkReply *currentReply;
    bool abortRequested;
    QRegularExpression CTYPrefixSeperatorRe;
    QRegularExpression CTYPrefixFormatRe;

private:
    bool isTableFilled(const QString &);
    bool deleteTable(const QString &);
    void download(const SourceDefinition &);
    void parseData(const LOVDownloader::SourceDefinition &,
                   QTextStream &);
    void parseCTY(const SourceDefinition &sourceDef, QTextStream& data);
    void parseSATLIST(const SourceDefinition &sourceDef, QTextStream& data);
    void parseSOTASummits(const SourceDefinition &sourceDef, QTextStream& data);
    void parseWWFFDirectory(const SourceDefinition &sourceDef, QTextStream& data);
    void parseIOTA(const SourceDefinition &sourceDef, QTextStream& data);
    void parsePOTA(const SourceDefinition &sourceDef, QTextStream& data);
    void parseMembershipContent(const SourceDefinition &sourceDef, QTextStream& data);
    void parseClubLogCTY(const SourceDefinition &sourceDef, QTextStream &data);
    bool parseCSVGeneric(const SourceDefinition &sourceDef,
                         QTextStream &data,
                         const QString &insertSQL,
                         const QStringList &csvColumns,
                         csv::CSVFormat format,
                         const QString &preValidateContains = QString());
private slots:
    void processReply(QNetworkReply*);
    void loadData(const LOVDownloader::SourceDefinition &);

};

#endif // QLOG_CORE_LOVDOWNLOADER_H
