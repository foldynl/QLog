#ifndef QLOG_SERVICE_QRZ_QRZ_H
#define QLOG_SERVICE_QRZ_QRZ_H

#include <QObject>
#include <QString>
#include <QSqlRecord>
#include "service/GenericCallbook.h"

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

    void uploadContact(const QSqlRecord &record);
    void uploadContacts(const QList<QSqlRecord>&);

    QString getDisplayName() override;

signals:
    void uploadFinished(bool result);
    void uploadedQSO(int);
    void uploadError(QString);

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
    QList<QSqlRecord> queuedContacts4Upload;
    bool cancelUpload;
    QNetworkReply *currentReply;

    void authenticate();
    void actionInsert(QByteArray& data, const QString &insertPolicy);
    QMap<QString, QString> parseActionResponse(const QString&) const;
};

#endif // QLOG_SERVICE_QRZ_QRZ_H
