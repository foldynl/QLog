#ifndef QLOG_SERVICE_GENERICCALLBOOK_H
#define QLOG_SERVICE_GENERICCALLBOOK_H

#include <QObject>
#include <QNetworkReply>

struct CallbookResponseData
{
    QString call;
    QString nick;
    QString qth;
    QString gridsquare;
    QString qsl_via;
    QString cqz;
    QString ituz;
    QString dok;
    QString iota;
    QString email;
    QString dxcc;
    QString name_fmt;
    QString fname;
    QString lname;
    QString addr1;
    QString us_state;
    QString zipcode;
    QString country;
    QString latitude;
    QString longitude;
    QString county;
    QString lic_year;
    QString utc_offset;
    QString eqsl;
    QString pqsl;
    QString born;
    QString lotw;
    QString url;
    QString image_url;
};

Q_DECLARE_METATYPE(CallbookResponseData)

class GenericCallbook : public QObject
{
    Q_OBJECT
public:
    explicit GenericCallbook(QObject *parent = nullptr);
    virtual ~GenericCallbook() {nam->deleteLater();};

    const static QString SECURE_STORAGE_KEY;
    const static QString CALLBOOK_NAME;

    static const QString getWebLookupURL(const QString &callsign,
                                         const QString &URL = QString(),
                                         bool replaceMacro = true);

    virtual QString getDisplayName() = 0;

protected:
    QNetworkAccessManager* getNetworkAccessManager() {return nam;};
    virtual void processReply(QNetworkReply *reply) = 0;
    QString decodeHtmlEntities(const QString &text);

signals:
    void callsignResult(CallbookResponseData);
    void lookupError(const QString);
    void loginFailed();
    void callsignNotFound(QString);

public slots:
    virtual void queryCallsign(const QString &callsign) = 0;
    virtual void abortQuery() = 0;

private:
    QNetworkAccessManager* nam;

private slots:
    void onNetworkReply(QNetworkReply *reply);
};
#endif // QLOG_SERVICE_GENERICCALLBOOK_H
