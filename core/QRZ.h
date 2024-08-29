#ifndef QLOG_CORE_QRZ_H
#define QLOG_CORE_QRZ_H

#include <QObject>
#include <QString>
#include <QSqlRecord>
#include "core/GenericCallbook.h"

class QNetworkAccessManager;
class QNetworkReply;

class QRZ : public GenericCallbook
{
    Q_OBJECT

public:
    explicit QRZ(QObject *parent = nullptr);
    ~QRZ();

    void uploadContact(const QSqlRecord &record);
    void uploadContacts(const QList<QSqlRecord>&);

    static const QString getUsername();
    static const QString getPassword();
    static const QString getLogbookAPIKey();

    static void saveUsernamePassword(const QString&, const QString&);
    static void saveLogbookAPI(const QString&);

    QString getDisplayName() override;

    const static QString CALLBOOK_NAME;

signals:
    void uploadFinished(bool result);
    void uploadedQSO(int);
    void uploadError(QString);

public slots:
    void queryCallsign(const QString &callsign) override;
    void abortQuery() override;

private slots:
    void processReply(QNetworkReply* reply);

private:
    QNetworkAccessManager* nam;
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

    const static QString SECURE_STORAGE_KEY;
    const static QString SECURE_STORAGE_API_KEY;
    const static QString CONFIG_USERNAME_KEY;
    const static QString CONFIG_USERNAME_API_KEY;
    const static QString CONFIG_USERNAME_API_CONST;

};

#endif // QLOG_CORE_QRZ_H
