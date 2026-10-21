#ifndef QLOG_SERVICE_EQSL_EQSL_H
#define QLOG_SERVICE_EQSL_EQSL_H

#include <QObject>
#include <logformat/LogFormat.h>

#include "core/QSLStorage.h"
#include "service/GenericQSOUploader.h"
#include "service/GenericQSLDownloader.h"
#include "core/CredentialStore.h"

class QNetworkAccessManager;
class QNetworkReply;

class EQSLBase : public SecureServiceBase<EQSLBase>
{
protected:
    static const QString SECURE_STORAGE_KEY;

public:
    explicit EQSLBase() {};
    virtual ~EQSLBase() {};

    DECLARE_SECURE_SERVICE(EQSLBase);

    static const QString getUsername();
    static const QString getPasswd();
    static void saveUsernamePassword(const QString&, const QString&);

    // Callsign-specific accounts - the eQSL username is the callsign. The account above is
    // the default one, used for callsigns without an account of their own.
    static QStringList getAddlCallsigns();
    static void setAddlCallsigns(const QStringList &list);
    static QString getCallsignPasswd(const QString &callsign);
    static void saveCallsignPasswd(const QString &newPassword, const QString &callsign);
    static QString resolveUsername(const QString &stationCallsign);
    static QString getPasswdForUsername(const QString &username);
    static QStringList getAccountUsernames();
    static bool isConfigured();
};

class EQSLUploader : public GenericQSOUploader, private EQSLBase
{
    Q_OBJECT
public:
    static QStringList uploadedFields;
    static QVariantMap generateUploadConfigMap(const QString qthProfile, bool disableQSLMSG, bool commentAsQSLMSG)
    {
        return QVariantMap({{"qthprofile", qthProfile},
                            {"disableqslmsg", disableQSLMSG},
                            {"commentasqslmsg", commentAsQSLMSG}});
    }

    explicit EQSLUploader(QObject *parent = nullptr);
    virtual ~EQSLUploader();

    void uploadAdif(const QByteArray&, const QString &username);
    virtual void uploadQSOList(const QList<QSqlRecord>& qsos, const QVariantMap &addlParams) override;

public slots:
    virtual void abortRequest() override;

protected:
    virtual const QSqlRecord stripRecord(const QSqlRecord &record) override;

private:
    const QString UPLOAD_ADIF_PAGE = "https://www.eQSL.cc/qslcard/ImportADIF.cfm";
    QNetworkReply *currentReply;
    bool commentAsQSLMSG;
    bool disableqslmsg;
    // ADIF data per eQSL account (username) waiting for upload
    QList<QPair<QString, QByteArray>> queuedUploads;
    void uploadNextAccount();
    static bool isDefaultAccountCallsign(const QString &stationCallsign);
    virtual void processReply(QNetworkReply*) override;
};

class EQSLQSLDownloader : public GenericQSLDownloader, private EQSLBase
{
    Q_OBJECT

public:
    explicit EQSLQSLDownloader(QObject *parent = nullptr);
    virtual ~EQSLQSLDownloader();

    virtual void receiveQSL(const QDate &, bool, const QString &) override;
    void getQSLImage(const QSqlRecord&);
    // eQSL account (username) used by receiveQSL; the default account if not set
    void setAccountUsername(const QString &username);

signals:
    void QSLImageFound(QString);
    void QSLImageError(QString);

public slots:
    virtual void abortDownload() override;

private:
    QNetworkReply *currentReply;
    QSLStorage *qslStorage;
    QString accountUsername;
    const QString DOWNLOAD_1ST_PAGE = "https://www.eQSL.cc/qslcard/DownloadInBox.cfm";
    const QString DOWNLOAD_2ND_PAGE = "https://www.eQSL.cc/downloadedfiles/";
    const QString QSL_IMAGE_FILENAME_PAGE = "https://www.eQSL.cc/qslcard/GeteQSL.cfm";
    const QString QSL_IMAGE_DOWNLOAD_PAGE = "https://www.eQSL.cc";

    virtual void processReply(QNetworkReply* reply) override;
    void get(const QList<QPair<QString, QString> > &);
    void downloadADIF(const QString &);
    void downloadImage(const QString &, const QString &, const qulonglong);
    QString QSLImageFilename(const QSqlRecord &);
    QStringList otherAccountsStationCallsigns() const;
    bool isQSLImageInCache(const QSqlRecord &, QString &);
};

#endif // QLOG_SERVICE_EQSL_EQSL_H
