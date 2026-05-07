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
    static QStringList uploadedFields;

    static QVariantMap generateUploadConfigMap(const QString &location)
    {
        return QVariantMap({{"tqsl_location", location}});
    }
    explicit LotwUploader(QObject *parent = nullptr);
    virtual ~LotwUploader();

    void uploadAdif(const QByteArray &, const QString &location = QString());
    virtual void uploadQSOList(const QList<QSqlRecord>& qsos, const QVariantMap &addlParams) override;

public slots:
    virtual void abortRequest() override {};

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

    bool LotwDXCCCredits = false;

    virtual void receiveQSL(const QDate &, bool, const QString &) override;

public slots:
    virtual void abortDownload() override;

private:
    QNetworkReply *currentReply;
    const QString ADIF_API = "https://lotw.arrl.org/lotwuser/lotwreport.adi";
    const QString DXCC_CREDIT_API = "https://lotw.arrl.org/lotwuser/logbook/qslcards.php";

    virtual void processReply(QNetworkReply* reply) override;
    void get(QList<QPair<QString, QString>> params);
};

#endif // QLOG_SERVISE_LOTW_LOTW_H
