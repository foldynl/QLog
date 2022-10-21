#ifndef SOTAAPI_H
#define SOTAAPI_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QCache>

class SOTAApi : public QObject
{
    Q_OBJECT

public:
    explicit SOTAApi(QObject *parent = nullptr);
    ~SOTAApi();

public slots:
    void querySummit(const QString &);
    void abortQuery();

signals:
    void summitResult(const QMap<QString, QString>& data);
    void lookupError(const QString &);
    void summitNotFound(const QString&);

private slots:
    void processReply(QNetworkReply* reply);

private:
     QNetworkAccessManager* nam;
     QNetworkReply *currentReply;
     QCache<QString, QMap<QString, QString>> querySummitCache;
};

#endif // SOTAAPI_H
