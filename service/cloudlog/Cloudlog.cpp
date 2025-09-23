#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>

#include "Cloudlog.h"
#include "core/debug.h"
#include "core/CredentialStore.h"
#include "core/LogParam.h"
#include "logformat/AdiFormat.h"

MODULE_IDENTIFICATION("qlog.core.cloudlog");

const QString CloudlogBase::SECURE_STORAGE_API_KEY = "Cloudlog";
const QString CloudlogBase::CONFIG_USERNAME_API_CONST = "logbookapi";


QString CloudlogBase::getLogbookAPIKey(const QString &internalUsername)
{
    FCT_IDENTIFICATION;

    return CredentialStore::instance()->getPassword(CloudlogBase::SECURE_STORAGE_API_KEY,
                                                    internalUsername);
}

void CloudlogBase::saveLogbookAPIKey(const QString &newKey, const QString &internalUsername)
{
    FCT_IDENTIFICATION;

    CredentialStore::instance()->deletePassword(CloudlogBase::SECURE_STORAGE_API_KEY,
                                                internalUsername);
    if ( newKey.isEmpty() ) return;

    CredentialStore::instance()->savePassword(CloudlogBase::SECURE_STORAGE_API_KEY,
                                              internalUsername,
                                              newKey);
}

QString CloudlogBase::getAPIEndpoint()
{
    FCT_IDENTIFICATION;

    return LogParam::getCloudlogAPIEndpoint();
}

void CloudlogBase::setAPIEndpoint(const QString &endpoint)
{
    FCT_IDENTIFICATION;

    LogParam::setCloudlogAPIEndpoint(endpoint);
}

CloudlogUploader::CloudlogUploader(QObject *parent) :
      GenericQSOUploader(QStringList(), parent),
      CloudlogBase(),
      currentReply(nullptr),
      cancelUpload(false),
      stationID(0)
{
    FCT_IDENTIFICATION;
}

CloudlogUploader::~CloudlogUploader()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        currentReply->deleteLater();
    }
}

QVariantMap CloudlogUploader::generateUploadConfigMap(uint stationID)
{
    FCT_IDENTIFICATION;

    return QVariantMap({{"stationID", stationID}});
}

void CloudlogUploader::abortRequest()
{
    FCT_IDENTIFICATION;

    cancelUpload = true;
    if ( currentReply )
    {
        currentReply->abort();
        currentReply = nullptr;
    }
}

void CloudlogUploader::sendStationInfoReq()
{
    FCT_IDENTIFICATION;

    QUrl url(LogParam::getCloudlogAPIEndpoint() + "/api/station_info/" + getLogbookAPIKey());
    QNetworkRequest request(url);

    //qCDebug(runtime) << url;
    QNetworkReply *reply = getNetworkAccessManager()->get(request); // do not use currentReply
    reply->setProperty("messageType", QVariant("getStationID"));
}

void CloudlogUploader::uploadContact(const QSqlRecord &record, uint stationID)
{
    FCT_IDENTIFICATION;

    const QByteArray &data = generateADIF({record});
    cancelUpload = false;
    uploadAdif(data, stationID);
    currentReply->setProperty("contactID", record.value("id"));
}

void CloudlogUploader::uploadAdif(const QByteArray &data, uint stationID)
{
    FCT_IDENTIFICATION;

    QUrl url(LogParam::getCloudlogAPIEndpoint() + "/api/qso");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");

    QJsonObject json;
    json["key"] = getLogbookAPIKey();
    json["station_profile_id"] = (qint64)stationID;
    json["type"] = "adif";
    json["string"] = QString::fromUtf8(data);

    QJsonDocument doc(json);
    qCDebug(runtime) << "Request Json:" << doc.toJson();

    currentReply = getNetworkAccessManager()->post(request, doc.toJson());
    currentReply->setProperty("messageType", QVariant("uploadADIF"));
}

void CloudlogUploader::uploadQSOList(const QList<QSqlRecord> &qsos, const QVariantMap &addlParams)
{
    FCT_IDENTIFICATION;

    if ( qsos.isEmpty() )
    {
        /* Nothing to do */
        emit uploadFinished();
        return;
    }

    /* Even though Cloudlog can upload batches, we will upload QSOs one by one. This has several advantages
     *  - we don't have to verify the upload size limit for PHP
     *  - we don't have to search for errors for individual QSOs
     *  - we correctly display the upload progress dialog.
     */
    stationID = addlParams["stationID"].toUInt();
    cancelUpload = false;
    queuedContacts4Upload = qsos;
    uploadContact(queuedContacts4Upload.first(), stationID);
    queuedContacts4Upload.removeFirst();
}

const QMap<uint, CloudlogUploader::StationProfile> &CloudlogUploader::getAvailableStationIDs() const
{
    return availableStationIDs;
}

void CloudlogUploader::processReply(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    /* always process one requests per class */
    currentReply = nullptr;

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString &messageType = reply->property("messageType").toString();
    const QByteArray &response = reply->readAll();

    qCDebug(runtime) << "Received Message Type: " << messageType
                     << "HTTP Code" << replyStatusCode
                     << "Cloudlog URL" << reply->request().url().toString();
    qCDebug(runtime) << "Response:" << response;

    /*************/
    /* uploadQSO */
    /*************/
    if ( messageType == "uploadADIF" )
    {
        switch (replyStatusCode)
        {
            case 201: // Created
            case 400: // Bad Request (e.g., duplicate)
            {
                const QVariantMap &resp = parseResponse(response);
                const QString &status = resp.value("status").toString();
                const QStringList &messages = resp.value("messages").toStringList();
                QString reason = messages.isEmpty() ? QString() : messages.first();
                reason = (reason.isEmpty() && messages.size() >= 2) ? messages.at(1) : reason;
                bool success = (status == "created") ||
                               (status == "abort" && reason.contains("Duplicate for"));

                if (success)
                {
                    emit uploadedQSO(reply->property("contactID").toULongLong());

                    if (queuedContacts4Upload.isEmpty())
                    {
                        cancelUpload = false;
                        emit uploadFinished();
                    }
                    else if (!cancelUpload)
                    {
                        uploadContact(queuedContacts4Upload.first(), stationID);
                        queuedContacts4Upload.removeFirst();
                    }
                }
                else
                {
                    cancelUpload = false;
                    queuedContacts4Upload.clear();
                    emit uploadError(reason.isEmpty() ? reply->errorString() : reason);
                }
                break;
            }

            case 401: // Unauthorized
                cancelUpload = false;
                queuedContacts4Upload.clear();
                emit uploadError(tr("Invalid API Key"));
                break;

            default:
                qCWarning(runtime) << "Unexpected HTTP status code:" << replyStatusCode;
                cancelUpload = false;
                queuedContacts4Upload.clear();
                emit uploadError(reply->errorString());
                break;
        }
    }
    /*************/
    /* uploadQSO */
    /*************/
    else if ( messageType == "getStationID" )
    {
        if ( replyStatusCode == 200 )
        {
            QJsonDocument doc = QJsonDocument::fromJson(response);

            if (doc.isArray())
            {
                const QJsonArray &array = doc.array();
                for ( const QJsonValue &value : array )
                    if (value.isObject())
                    {
                        StationProfile profile = StationProfile::fromJson(value.toObject());
                        availableStationIDs.insert(profile.station_id, profile);
                    }
            }
            emit stationIDsUpdated();
        }
    }

    reply->deleteLater();
}

const QByteArray CloudlogUploader::generateADIF(const QList<QSqlRecord> &qsos, QMap<QString, QString> *applTags)
{
    FCT_IDENTIFICATION;

    QByteArray data;
    QTextStream stream(&data, QIODevice::ReadWrite);
    AdiFormat adi(stream);

    //adi.exportStart(); // don't add header
    for ( const QSqlRecord &qso : qsos )
        adi.exportContact(stripRecord(qso), applTags);
    //adi.exportEnd();   // don't add footer

    stream.flush();
    return data;
}

QVariantMap CloudlogUploader::parseResponse(const QByteArray &data)
{
    FCT_IDENTIFICATION;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if ( error.error != QJsonParseError::NoError || !doc.isObject())
    {
        qCWarning(runtime) << "JSON parse error:" << error.errorString();
        return {};
    }

    QJsonObject obj = doc.object();
    return obj.toVariantMap();
}


CloudlogUploader::StationProfile CloudlogUploader::StationProfile::fromJson(const QJsonObject &obj)
{
    FCT_IDENTIFICATION;

    StationProfile profile;
    profile.station_id = obj.value("station_id").toString().toInt();
    profile.station_profile_name = obj.value("station_profile_name").toString();
    profile.station_gridsquare = obj.value("station_gridsquare").toString();
    profile.station_callsign = obj.value("station_callsign").toString();
    profile.station_active = obj.contains("station_active") && obj.value("station_active").toString() == "1";
    return profile;
}
