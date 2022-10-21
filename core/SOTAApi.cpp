#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include "SOTAApi.h"
#include "debug.h"

MODULE_IDENTIFICATION("qlog.core.sotaapi");

//https://api2.sota.org.uk/api/summits/OK/JC-001
#define API_URL "https://api2.sota.org.uk/api/summits/"

SOTAApi::SOTAApi(QObject *parent)
    : QObject{parent},
      currentReply(nullptr)
{
    FCT_IDENTIFICATION;

    nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished,
            this, &SOTAApi::processReply);
}

SOTAApi::~SOTAApi()
{
    FCT_IDENTIFICATION;

    nam->deleteLater();

    if ( currentReply )
    {
        currentReply->abort();
        currentReply->deleteLater();
    }
}

void SOTAApi::querySummit(const QString &summit)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<< summit;

    if ( querySummitCache.contains(summit) )
    {
        emit summitResult(QMap<QString, QString>(*querySummitCache.object(summit)));
        return;
    }

    // create an empty object in cache
    // if there is the second query for the same call immediatelly after
    // the first query, then it returns a result of empty object
    querySummitCache.insert(summit, new QMap<QString, QString>());

    QString queryUrl = QString(API_URL).append(summit);

    QUrl url(queryUrl);

    if ( currentReply )
    {
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";
    }

    currentReply = nam->get(QNetworkRequest(url));
    currentReply->setProperty("summitName", summit);
}

void SOTAApi::abortQuery()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        currentReply = nullptr;
    }
}

void SOTAApi::processReply(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    /* always process one requests per class */
    currentReply = nullptr;

    if ( reply->error() != QNetworkReply::NoError )
    {
        qCDebug(runtime) << "SOTA API error" << reply->errorString();

        if ( reply->error() != QNetworkReply::OperationCanceledError )
        {
            emit lookupError(reply->errorString());
            emit summitNotFound(reply->property("summitName").toString());
            reply->deleteLater();
        }
        return;
    }

    QByteArray response = reply->readAll();
    qCDebug(runtime) << response;

    QMap<QString, QString> data;
    QJsonDocument replyJson = QJsonDocument::fromJson(response);

    if ( ! replyJson.isNull() )
    {
        QJsonObject summitObject = replyJson.object();
        QStringList summitKeys = summitObject.keys();
        for ( const QString& key : qAsConst(summitKeys) )
        {
            QJsonValue value = summitObject.value(key);
            qCDebug(runtime) << "Key = " << key << ", Value = " << value.toString();
            data[key] = value.toString();
        }
    }

    querySummitCache.insert(reply->property("summitName").toString(), new QMap<QString, QString>(data));

    reply->deleteLater();

    if (data.size())
    {
        emit summitResult(data);
    }
}
