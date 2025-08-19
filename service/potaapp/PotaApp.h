#ifndef QLOG_SERVICE_POTAAPP_POTAAPP_H
#define QLOG_SERVICE_POTAAPP_POTAAPP_H

#include <QObject>
#include "data/POTASpot.h"

class QNetworkAccessManager;
class QNetworkReply;

class PotaAppActivatorDownloader : public QObject
{
    Q_OBJECT

public:
    using ActivatorStorage = QMultiHash<QString, POTASpot>;

    explicit PotaAppActivatorDownloader(QObject *parent = nullptr);
    virtual ~PotaAppActivatorDownloader();
    void swapActivators(ActivatorStorage &a);

signals:
    void activatorsUpdated();

public slots:
    void updateActivators();

private slots:
    void processReply(QNetworkReply *reply);

private:
    QNetworkAccessManager *nam = nullptr;
    QNetworkReply *currReply = nullptr;
    QMutex activatorLock;
    ActivatorStorage activators;
    const QString API_URL = "https://api.pota.app/spot/activator";
};

#endif // QLOG_SERVICE_POTAAPP_POTAAPP_H
