#ifndef QLOG_SERVICE_QRZCALLEU_QRZCALLEU_H
#define QLOG_SERVICE_QRZCALLEU_QRZCALLEU_H

#include <QObject>
#include <QString>
#include <QSqlRecord>
#include <QSqlQuery>
#include "service/GenericCallbook.h"
#include "service/GenericQSOUploader.h"
#include "core/CredentialStore.h"

class QNetworkAccessManager;
class QNetworkReply;

// QRZCALL.EU uses a single Personal Access Token (pat_...) for both the
// callbook lookup and the QSO upload. The token is stored once in the
// credential store under a fixed internal username.
class QRZCallEUBase : public SecureServiceBase<QRZCallEUBase>
{
protected:
    const static QString SECURE_STORAGE_API_KEY;
    const static QString CONFIG_USERNAME_API_CONST;

public:
    explicit QRZCallEUBase() {};
    virtual ~QRZCallEUBase() {};

    DECLARE_SECURE_SERVICE(QRZCallEUBase);

    static QString getUsername() {return CONFIG_USERNAME_API_CONST;}
    static const QString getPAT();
    static void savePAT(const QString &newPAT);
    static bool isUploadImmediatelyEnabled();
    static void saveUploadImmediatelyConfig(bool value);
};

class QRZCallEUCallbook : public GenericCallbook, private QRZCallEUBase
{
    Q_OBJECT

public:
    const static QString CALLBOOK_NAME;

    explicit QRZCallEUCallbook(QObject *parent = nullptr);
    virtual ~QRZCallEUCallbook();

    QString getDisplayName() override;

public slots:
    virtual void queryCallsign(const QString &callsign) override;
    virtual void abortQuery() override;

protected:
    virtual void processReply(QNetworkReply *reply) override;

private:
    QNetworkReply *currentReply;
    const QString API_URL = "https://api.qrzcall.eu/v1/pub/callsign_xml.php";
};

class QRZCallEUUploader : public GenericQSOUploader, private QRZCallEUBase
{
    Q_OBJECT

public:
    explicit QRZCallEUUploader(QObject *parent = nullptr);
    virtual ~QRZCallEUUploader();

    void uploadContact(const QSqlRecord &record);
    virtual void uploadQSOList(const QList<QSqlRecord> &qsos, const QVariantMap &addlParams) override;

public slots:
    virtual void abortRequest() override;
    // Real-time upload ("Immediately Upload"): triggered when a QSO is logged
    // or edited. A no-op unless QRZCallEUBase::isUploadImmediatelyEnabled().
    void insertQSOImmediately(const QSqlRecord &record);
    void updateQSOImmediately(const QSqlRecord &record);

protected:
    virtual void processReply(QNetworkReply *reply) override;

private:
    QNetworkReply *currentReply;
    QList<QSqlRecord> queuedContacts4Upload;
    bool cancelUpload;
    QSqlQuery query_updateRT;
    const QString API_LOGBOOK_URL = "https://api.qrzcall.eu/v1/pub/logbook_api.php";

    void actionInsert(const QString &pat, QByteArray &data, const QString &insertPolicy);
    void sendRealtimeUpload(const QSqlRecord &record);
    QMap<QString, QString> parseActionResponse(const QString &) const;
};

#endif // QLOG_SERVICE_QRZCALLEU_QRZCALLEU_H
