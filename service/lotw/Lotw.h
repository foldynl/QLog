#ifndef QLOG_SERVISE_LOTW_LOTW_H
#define QLOG_SERVISE_LOTW_LOTW_H

#include <QObject>
#include <QNetworkReply>
#include <logformat/LogFormat.h>
#include "service/GenericQSOUploader.h"
#include "service/GenericQSLDownloader.h"
#include "core/CredentialStore.h"

class QNetworkAccessManager;

struct TQSLVersion
{
    int major = -1;
    int minor = -1;
    int patch = -1;
    bool isValid() const { return major >= 0; }
};

struct TQSLStationLocation
{
    QString name;
    QString callsign;
    QString grid;
};

class LotwBase : public SecureServiceBase<LotwBase>
{
protected:
    static const QString SECURE_STORAGE_KEY;

public:
    explicit LotwBase() {};
    virtual ~LotwBase() {};

    DECLARE_SECURE_SERVICE(LotwBase);

    static const QString getUsername();
    static const QString getPasswd();
    static const QString getTQSLPath(const QString &defaultPath = QDir::rootPath());
    static QString findTQSLPath();
    static TQSLVersion getTQSLVersion(const QString &tqslPath = QString());
    static QString getTQSLStationDataPath();
    static QList<TQSLStationLocation> getTQSLStationLocations();

    static void saveUsernamePassword(const QString&, const QString&);
    static void saveTQSLPath(const QString&);

    // Returns the QLog dxcc group ("CW", "PHONE", "DIGITAL") for the given
    // LoTW generic mode group name, or an empty string for specific mode names.
    static QString lotwGroupNameToDxcc(const QString &lotwMode)
    {
        static const QMap<QString, QString> map =
        {
            { "DATA",  "DIGITAL" },
            { "PHONE", "PHONE"   },
            { "CW",    "CW"      },
            { "IMAGE", "DIGITAL" }
        };
        return map.value(lotwMode.toUpper());
    }
};

class LotwUploader : public GenericQSOUploader, private LotwBase
{
    Q_OBJECT

public:
    static QStringList uploadedFields()
    {
        return {
            "callsign",
            "freq",
            "band",
            "freq_rx",
            "band_rx",
            "mode",
            "submode",
            "start_time",
            "prop_mode",
            "sat_name",
            "station_callsign",
            "operator",
            "rst_sent",
            "rst_rcvd",
            "my_state",
            "my_cnty",
            "my_vucc_grids"
        };
    }

    static QVariantMap generateUploadConfigMap(const QString &location)
    {
        return QVariantMap({{"tqsl_location", location}});
    }
    explicit LotwUploader(QObject *parent = nullptr);
    virtual ~LotwUploader();

    void uploadAdif(const QByteArray &, const QString &location = QString());
    virtual void uploadQSOList(const QList<QSqlRecord>& qsos, const QVariantMap &addlParams) override;

public slots:
    virtual void abortRequest() override;

private:
    QTemporaryFile file;
    virtual void processReply(QNetworkReply*) override {};
};

class LotwQSLDownloader : public GenericQSLDownloader, private LotwBase
{
    Q_OBJECT

public:
    explicit LotwQSLDownloader(QObject *parent = nullptr);
    virtual ~LotwQSLDownloader();

    virtual void receiveQSL(const QDate &, bool, const QString &) override;
    void receiveQSLs(const QDate &, bool, const QStringList &stationCallsigns);

signals:
    void stationCallsignComplete(const QString &stationCallsign);

public slots:
    virtual void abortDownload() override;

private:
    QNetworkReply *currentReply;
    QList<QPair<QString, QString>> requestParams;
    QStringList stationCallsignQueue;
    QString currentStationCallsign;
    QDate startDate;
    bool qsoSince;
    bool continueOnCallsignError;
    QSLMergeStat downloadStat;
    int retryCount;
    bool aborted;
    const QString ADIF_API = "https://lotw.arrl.org/lotwuser/lotwreport.adi";
    enum { MAX_REQUEST_RETRIES = 2 };

    virtual void processReply(QNetworkReply* reply) override;
    void requestNextCallsign();
    void completeCallsign(const QSLMergeStat &stats);
    void skipCallsign(const QString &error);
    static void mergeQSLStats(QSLMergeStat &target, const QSLMergeStat &source);
    bool scheduleRetry(QNetworkReply *reply, const QString &reason);
    void get(const QList<QPair<QString, QString>> &params);
};

class LotwDXCCCreditDownloader : public QObject, private LotwBase
{
    Q_OBJECT

public:
    explicit LotwDXCCCreditDownloader(QObject *parent = nullptr);
    virtual ~LotwDXCCCreditDownloader();

    static const QString dxccModeGroupFromLotw(const QString &lotwModeGroup);
    void downloadCredits(const QString &entity = QString());

signals:
    void downloadProgress(qulonglong value);
    void downloadStarted();
    void downloadComplete(QSLMergeStat);
    void downloadFailed(QString);

public slots:
    void abortDownload();

private:
    QNetworkAccessManager *nam;
    QNetworkReply *currentReply;
    const QString DXCC_CREDIT_API = "https://lotw.arrl.org/lotwuser/logbook/qslcards.php";

    void processReply(QNetworkReply *reply);
};

#endif // QLOG_SERVISE_LOTW_LOTW_H
