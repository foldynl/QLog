#ifndef GENERICQSLDOWNLOADER_H
#define GENERICQSLDOWNLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <logformat/LogFormat.h>

class GenericQSLDownloader : public QObject
{
    Q_OBJECT
public:
    explicit GenericQSLDownloader(QObject *parent = nullptr);
    virtual ~GenericQSLDownloader() {nam->deleteLater();};
    virtual void receiveQSL(const QDate&, bool, const QString&) = 0;

signals:
    void receiveQSLProgress(qulonglong value);
    void receiveQSLStarted();
    void receiveQSLComplete(QSLMergeStat);
    void receiveQSLFailed(QString);

public slots:
    virtual void abortDownload() = 0;
    QNetworkAccessManager* getNetworkAccessManager() {return nam;};

protected:
    virtual void processReply(QNetworkReply *reply) = 0;

private slots:
    void onNetworkReply(QNetworkReply *reply);

private:
        QNetworkAccessManager* nam;
};

#endif // GENERICQSLDOWNLOADER_H
