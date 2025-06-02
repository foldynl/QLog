#include "GenericQSLDownloader.h"
#include <QNetworkReply>

GenericQSLDownloader::GenericQSLDownloader(QObject *parent)
    : QObject{parent}
{
    nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished, this, &GenericQSLDownloader::onNetworkReply);
}

void GenericQSLDownloader::onNetworkReply(QNetworkReply *reply)
{
    processReply(reply);
}
