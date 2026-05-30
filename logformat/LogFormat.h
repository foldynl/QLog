#ifndef QLOG_LOGFORMAT_LOGFORMAT_H
#define QLOG_LOGFORMAT_LOGFORMAT_H

#include <QtCore>
#include <QMap>
#include <QSqlQuery>

#include "core/LogLocale.h"
#include "data/StationProfile.h"

class QSqlRecord;

struct QSLMergeStat {
    QStringList newQSLs;
    QStringList updatedQSOs;
    QStringList unmatchedQSLs;
    QStringList errorQSLs;
    int qsosDownloaded;
};

class LogFormat : public QObject {
    Q_OBJECT

public:
    enum Type {
        ADI,
        ADX,
        CABRILLO,
        JSON,
        CSV,
        POTA
    };

    enum QSLFrom {
        LOTW,
        EQSL,
        UNKNOW
    };

    enum duplicateQSOBehaviour {
        ASK_NEXT,
        SKIP_ONE,
        SKIP_ALL,
        ACCEPT_ONE,
        ACCEPT_ALL
    };

    explicit LogFormat(QTextStream& stream);

    virtual ~LogFormat();

    static LogFormat* open(QString type, QTextStream& stream);
    static LogFormat* open(Type type, QTextStream& stream);

    unsigned long runImport(QTextStream& importLogStream,
                            const StationProfile *defaultStationProfile,
                            unsigned long *warnings,
                            unsigned long *errors);
    void runQSLImport(QSLFrom fromService);
    void runDXCCCreditImport();
    long runExport();
    long runExport(const QList<QSqlRecord>&);
    void setDefaults(QMap<QString, QString>& defaults);
    void setFilterDateRange(const QDate &start, const QDate &end);
    void setFilterMyCallsign(const QString &myCallsing);
    void setFilterMyGridsquare(const QString &myGridsquare);
    void setFilterSentPaperQSL(bool includeNo, bool includeIgnore, bool includeAlreadySent);
    void setFilterSendVia(const QString &value);
    void setFilterStationProfile(const StationProfile &profile);
    void setUserFilter(const QString&value);
    void setPotaOnly(bool only);
    QString getWhereClause();
    void bindWhereClause(QSqlQuery &);
    void setExportedFields(const QStringList& fieldsList);
    void setFillMissingDxcc(bool fillMissingDxcc);
    void setDuplicateQSOCallback(duplicateQSOBehaviour (*func)(QSqlRecord *, QSqlRecord *));

    virtual void importStart() {}
    virtual void importEnd() {}
    virtual bool importNext(QSqlRecord&) { return false; }

    virtual void exportStart() {}
    virtual void exportEnd() {}
    virtual void exportContact(const QSqlRecord&, QMap<QString, QString> * = nullptr) {}

signals:
    void importPosition(qint64 value);
    void exportProgress(float value);
    void finished(int count);
    void QSLMergeFinished(QSLMergeStat stats);

protected:
    struct DXCCCreditRecord
    {
        QString call;
        QString band;
        QString dxcc;
        QString mode;
        QString propMode;
        QString dxccModeGroup;
        QString creditGranted;
        QString awardEntity;
        QDate qsoDate;
    };

    QTextStream& stream;
    QMap<QString, QString>* defaults;
    virtual bool importNextDXCCCredit(DXCCCreditRecord&) { return false; }

private:
    struct DXCCCreditMatch
    {
        qulonglong id = 0;
        QString callsign;
        QString band;
        QString mode;
        QDateTime startTime;
        QString propMode;
        QString creditGranted;
    };

    enum ImportLogSeverity
    {
        INFO_SEVERITY,
        WARNING_SEVERITY,
        ERROR_SEVERITY
    };

    enum DXCCCreditCallMatch
    {
        NO_CALL_MATCH,
        EXACT_CALL_MATCH,
        PREFIX_CALL_MATCH
    };

    bool isDateRange();
    bool inDateRange(QDate date);

    QString importLogSeverityToString(ImportLogSeverity);

    static QStringList splitCreditValues(const QString &value);
    static QString mergeCreditValues(const QString &currentValue,
                                     const QString &newValue);
    static bool isSatelliteDXCCCredit(const DXCCCreditRecord &credit);
    static bool isDXCCEntityCode(const QString &call);
    static QString escapeSqlLikePattern(const QString &value);
    static QString formatDXCCCreditReport(const DXCCCreditRecord &credit,
                                          const QStringList &addInfo = QStringList());
    static bool selectDXCCCreditMatches(const DXCCCreditRecord &credit,
                                        const DXCCCreditCallMatch callMatch,
                                        const bool matchMode,
                                        QList<DXCCCreditMatch> &matches,
                                        QString &error);

    void writeImportLog(QTextStream& errorLogStream,
                        ImportLogSeverity severity,
                        const QString &msg);
    void writeImportLog(QTextStream& errorLogStream,
                        ImportLogSeverity severity,
                        unsigned long *error,
                        unsigned long *warning,
                        const unsigned long recordNo,
                        const QSqlRecord &record,
                        const QString &msg);
    QDate filterStartDate, filterEndDate;
    QString filterMyCallsign;
    QString filterMyGridsquare;
    QStringList filterSentPaperQSL;
    QString filterSendVia;
    StationProfile filterStationProfile;
    bool filterStationProfileSet = false;
    QStringList whereClause;
    QStringList exportedFields;
    QString userFilter;
    bool filterPOTAOnly = false;
    bool fillMissingDxcc = false;
    duplicateQSOBehaviour (*duplicateQSOFunc)(QSqlRecord *, QSqlRecord *);
    LogLocale locale;
};

#endif // QLOG_LOGFORMAT_LOGFORMAT_H
