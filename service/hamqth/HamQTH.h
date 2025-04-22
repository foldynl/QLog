#ifndef QLOG_SERVICE_HAMQTH_HAMQTH_H
#define QLOG_SERVICE_HAMQTH_HAMQTH_H

#include <QObject>
#include <QString>
#include "service/GenericCallbook.h"

class QNetworkAccessManager;
class QNetworkReply;

class HamQTH : public GenericCallbook
{
    Q_OBJECT

public:
    explicit HamQTH(QObject *parent = nullptr);
    virtual ~HamQTH();

    const static QString CALLBOOK_NAME;
    static const QString getUsername();
    static const QString getPassword();

    static void saveUsernamePassword(const QString&, const QString&);

    QString getDisplayName() override;

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
    QNetworkReply *currentReply;

    void authenticate();

    const static QString SECURE_STORAGE_KEY;
    const static QString CONFIG_USERNAME_KEY;
};

#endif // QLOG_SERVICE_HAMQTH_HAMQTH_H
