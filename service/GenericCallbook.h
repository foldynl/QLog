#ifndef QLOG_SERVICE_GENERICCALLBOOK_H
#define QLOG_SERVICE_GENERICCALLBOOK_H

#include <QObject>
#include <QNetworkReply>

class GenericCallbook : public QObject
{
    Q_OBJECT
public:
    explicit GenericCallbook(QObject *parent = nullptr);
    virtual ~GenericCallbook() {};

    const static QString SECURE_STORAGE_KEY;
    const static QString CONFIG_USERNAME_KEY;
    const static QString CONFIG_PRIMARY_CALLBOOK_KEY;
    const static QString CONFIG_SECONDARY_CALLBOOK_KEY;
    const static QString CALLBOOK_NAME;
    const static QString CONFIG_WEB_LOOKUP_URL;
    static const QString getWebLookupURL(const QString &callsign,
                                         const QString &URL = QString(),
                                         const bool replaceMacros = true);

    virtual QString getDisplayName() = 0;

signals:
    void callsignResult(const QMap<QString, QString>& data);
    void lookupError(const QString);
    void loginFailed();
    void callsignNotFound(QString);

public slots:
    virtual void queryCallsign(const QString &callsign) = 0;
    virtual void abortQuery() = 0;

};
#endif // QLOG_SERVICE_GENERICCALLBOOK_H
