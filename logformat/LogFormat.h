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
    long runExport();
    long runExport(const QList<QSqlRecord>&);
    void setDefaults(QMap<QString, QString>& defaults);
    void setFilterDateRange(const QDate &start, const QDate &end);
    void setFilterMyCallsign(const QString &myCallsing);
    void setFilterMyGridsquare(const QString &myGridsquare);
    void setFilterSentPaperQSL(bool includeNo, bool includeIgnore, bool includeAlreadySent);
    void setFilterSendVia(const QString &value);
    void setUserFilter(const QString&value);
    void setPotaOnly(bool only);
    QString getWhereClause();
    void bindWhereClause(QSqlQuery &);
    void setExportedFields(const QStringList& fieldsList);
    void setUpdateDxcc(bool updateDxcc);
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
    QTextStream& stream;
    QMap<QString, QString>* defaults;

private:
    enum ImportLogSeverity
    {
        INFO_SEVERITY,
        WARNING_SEVERITY,
        ERROR_SEVERITY
    };

    bool isDateRange();
    bool inDateRange(QDate date);

    QString importLogSeverityToString(ImportLogSeverity);

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
    QStringList whereClause;
    QStringList exportedFields;
    QString userFilter;
    bool filterPOTAOnly = false;
    bool updateDxcc = false;
    duplicateQSOBehaviour (*duplicateQSOFunc)(QSqlRecord *, QSqlRecord *);
    LogLocale locale;
};

#endif // QLOG_LOGFORMAT_LOGFORMAT_H
