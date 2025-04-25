#ifndef QLOG_SERVICE_QRZ_QRZ_H
#define QLOG_SERVICE_QRZ_QRZ_H

#include <QObject>
#include <QString>
#include <QSqlRecord>
#include "service/GenericCallbook.h"
#include "service/GenericQSOUploader.h"

class QNetworkAccessManager;
class QNetworkReply;

class QRZBase
{
public:
    explicit QRZBase() {};
    virtual ~QRZBase() {};

    static const QString getUsername();
    static const QString getPassword();
    static const QString getLogbookAPIKey();
    static void saveUsernamePassword(const QString&, const QString&);
    static void saveLogbookAPI(const QString&);

protected:
    const static QString SECURE_STORAGE_KEY;
    const static QString SECURE_STORAGE_API_KEY;
    const static QString CONFIG_USERNAME_KEY;
    const static QString CONFIG_USERNAME_API_KEY;
    const static QString CONFIG_USERNAME_API_CONST;
};

class QRZCallbook : public GenericCallbook, private QRZBase
{
    Q_OBJECT

public:
    const static QString CALLBOOK_NAME;

    explicit QRZCallbook(QObject *parent = nullptr);
    virtual ~QRZCallbook();

    QString getDisplayName() override;

public slots:
    virtual void queryCallsign(const QString &callsign) override;
    virtual void abortQuery() override;

protected:
    virtual void processReply(QNetworkReply* reply) override;

private:
    QString sessionId;
    QString queuedCallsign;
    bool incorrectLogin;
    QString lastSeenPassword;
    QNetworkReply *currentReply;
    const QString API_URL = "https://xmldata.qrz.com/xml/current/";

    void authenticate();
};

class QRZUploader : public GenericQSOUploader, private QRZBase
{
    Q_OBJECT

public:
    explicit QRZUploader(QObject *parent = nullptr);
    virtual ~QRZUploader();

    void uploadContact(const QSqlRecord &record);
    virtual void uploadQSOList(const QList<QSqlRecord>& qsos, const QVariantMap &addlParams) override;

public slots:
    virtual void abortRequest() override;

protected:
    virtual void processReply(QNetworkReply* reply) override;

private:
    QNetworkReply *currentReply;
    QList<QSqlRecord> queuedContacts4Upload;
    bool cancelUpload;
    const QString API_LOGBOOK_URL = "https://logbook.qrz.com/api";

    void actionInsert(QByteArray& data, const QString &insertPolicy);
    QMap<QString, QString> parseActionResponse(const QString&) const;
};

#endif // QLOG_SERVICE_QRZ_QRZ_H
