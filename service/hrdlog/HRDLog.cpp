#include <QSettings>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDomDocument>

#include "HRDLog.h"
#include "core/debug.h"
#include "core/CredentialStore.h"
#include "logformat/AdiFormat.h"
#include "rig/macros.h"

MODULE_IDENTIFICATION("qlog.core.hrdlog");

#define API_LOG_UPLOAD_URL "https://robot.hrdlog.net/NewEntry.aspx"
#define API_ONAIR_URL "https://robot.hrdlog.net/OnAir.aspx"

// http://www.iw1qlh.net/projects/hrdlog/HRDLognet_4.pdf

HRDLog::HRDLog(QObject *parent)
    : QObject(parent),
      currentReply(nullptr),
      cancelUpload(false)
{
    FCT_IDENTIFICATION;

    nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished,
            this, &HRDLog::processReply);
}

HRDLog::~HRDLog()
{
    FCT_IDENTIFICATION;

    nam->deleteLater();

    if ( currentReply )
    {
        currentReply->abort();
        currentReply->deleteLater();
    }
}

void HRDLog::abortRequest()
{
    FCT_IDENTIFICATION;

    cancelUpload = true;

    if ( currentReply )
    {
        currentReply->abort();
        //currentReply->deleteLater(); // pointer is deleted later in processReply
        currentReply = nullptr;
    }
}

const QString HRDLog::getRegisteredCallsign()
{
    FCT_IDENTIFICATION;
    QSettings settings;

    return settings.value(HRDLog::CONFIG_CALLSIGN_KEY).toString().trimmed();
}

const QString HRDLog::getUploadCode()
{
    FCT_IDENTIFICATION;
    QSettings settings;

    return CredentialStore::instance()->getPassword(HRDLog::SECURE_STORAGE_KEY,
                                                    getRegisteredCallsign());
}

bool HRDLog::getOnAirEnabled()
{
    FCT_IDENTIFICATION;
    QSettings settings;

    return settings.value(HRDLog::CONFIG_ONAIR_ENABLED_KEY, false).toBool();
}

void HRDLog::saveUploadCode(const QString &newUsername, const QString &newPassword)
{
    FCT_IDENTIFICATION;

    QSettings settings;

    QString oldUsername = getRegisteredCallsign();
    if ( oldUsername != newUsername )
    {
        CredentialStore::instance()->deletePassword(HRDLog::SECURE_STORAGE_KEY,
                                                    oldUsername);
    }

    settings.setValue(HRDLog::CONFIG_CALLSIGN_KEY, newUsername);

    CredentialStore::instance()->savePassword(HRDLog::SECURE_STORAGE_KEY,
                                              newUsername,
                                              newPassword);
}

void HRDLog::saveOnAirEnabled(bool state)
{
    FCT_IDENTIFICATION;

    QSettings settings;
    settings.setValue(HRDLog::CONFIG_ONAIR_ENABLED_KEY, state);
}

void HRDLog::uploadAdif(const QByteArray &data,
                        const QVariant &contactID,
                        bool update)
{
    FCT_IDENTIFICATION;

    QUrlQuery params;
    params.addQueryItem("Callsign", getRegisteredCallsign().toUtf8().toPercentEncoding());
    params.addQueryItem("Code", getUploadCode().toUtf8().toPercentEncoding());
    params.addQueryItem("App", "QLog");
    params.addQueryItem("ADIFData", data.trimmed().toPercentEncoding());

    if ( update )
    {
        params.addQueryItem("ADIFKey", data.trimmed().toPercentEncoding());
        params.addQueryItem("Cmd", "UPDATE");
    }

    QUrl url(API_LOG_UPLOAD_URL);
    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    qCDebug(runtime) << url;

    if ( currentReply )
    {
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";
    }

    currentReply = nam->post(request, params.query(QUrl::FullyEncoded).toUtf8());
    currentReply->setProperty("messageType", QVariant("uploadQSO"));
    currentReply->setProperty("ADIFData", data);
    currentReply->setProperty("contactID", contactID);
}

void HRDLog::uploadContact(QSqlRecord record)
{
    FCT_IDENTIFICATION;

    QByteArray data;
    QTextStream stream(&data, QIODevice::ReadWrite);

    AdiFormat adi(stream);
    adi.exportContact(record);
    stream.flush();

    cancelUpload = false;
    uploadAdif(data.trimmed(),
               record.value("id"),
               (record.value("hrdlog_qso_upload_status").toString() == "M"));
}

void HRDLog::uploadContacts(const QList<QSqlRecord> &qsos)
{
    FCT_IDENTIFICATION;

    //qCDebug(function_parameters) << qsos;

    /* always process one requests per class */

    if ( qsos.isEmpty() )
    {
        /* Nothing to do */
        emit uploadFinished(false);
        return;
    }

    cancelUpload = false;
    queuedContacts4Upload = qsos;

    uploadContact(queuedContacts4Upload.first());
    queuedContacts4Upload.removeFirst();
}

void HRDLog::sendOnAir(double freq, const QString &mode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << freq << mode;

    QUrlQuery params;

    params.addQueryItem("Callsign", getRegisteredCallsign().toUtf8().toPercentEncoding());
    params.addQueryItem("Code", getUploadCode().toUtf8().toPercentEncoding());
    params.addQueryItem("App", "QLog");
    params.addQueryItem("Frequency", QString::number(static_cast<unsigned long long>(MHz(freq))));
    params.addQueryItem("Mode", mode);
    params.addQueryItem("Radio", " ");

    QUrl url(API_ONAIR_URL);
    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    qCDebug(runtime) << url;

    if ( currentReply )
    {
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";
    }

    currentReply = nam->post(request, params.query(QUrl::FullyEncoded).toUtf8());
    currentReply->setProperty("messageType", QVariant("onAir"));
}

void HRDLog::processReply(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    /* always process one requests per class */
    currentReply = nullptr;

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if ( reply->error() != QNetworkReply::NoError
         || replyStatusCode < 200
         || replyStatusCode >= 300)
    {
        qCDebug(runtime) << "HDRLog.com error URL " << reply->request().url().toString();
        qCDebug(runtime) << "HDRLog.com error" << reply->errorString();
        qCDebug(runtime) << "HTTP Status Code" << replyStatusCode;

        if ( reply->error() != QNetworkReply::OperationCanceledError )
        {
            emit uploadError(reply->errorString());
            reply->deleteLater();
        }
        cancelUpload = true;
        return;
    }

    const QString &messageType = reply->property("messageType").toString();

    qCDebug(runtime) << "Received Message Type: " << messageType;

    const QByteArray &response = reply->readAll();
    qCDebug(runtime) << response;

    /*************/
    /* uploadQSO */
    /*************/
    if ( messageType == "uploadQSO" )
    {
        QDomDocument doc;

        if ( !doc.setContent(response) )
        {
            qWarning() << "Failed to parse XML document from HRDLog";
            emit uploadError(tr("Response message malformed"));
            cancelUpload = true;
        }
        else
        {
            QDomElement root = doc.documentElement();
            QDomNodeList errorNodes = root.elementsByTagName("error");

            if ( !errorNodes.isEmpty() )
            {
                QDomElement errorElement = errorNodes.at(0).toElement();
                QString errorText = errorElement.text();
                qCDebug(runtime) << "XML contains an error element:" << errorText;
                if ( errorText.contains("Unable to find QSO") )
                {
                    // Try to resend it without UPDATE Flag
                    uploadAdif(reply->property("ADIFData").toByteArray(),
                               reply->property("contactID"));
                }
                else
                {
                    emit uploadError(errorText);
                    cancelUpload = true;
                }
            }
            else
            {
                qCDebug(runtime) << "Confirmed Upload for QSO Id " << reply->property("contactID").toInt();
                emit uploadedQSO(reply->property("contactID").toInt());

                if ( queuedContacts4Upload.isEmpty() )
                {
                    cancelUpload = false;
                    emit uploadFinished(true);
                }
                else if ( ! cancelUpload )
                {
                    uploadContact(queuedContacts4Upload.first());
                    queuedContacts4Upload.removeFirst();
                }
            }
        }
    }
    else if ( messageType == "onAir" )
    {
        // Do no handle onAir response - error handling is unclear from spec
    }

    reply->deleteLater();
}

const QString HRDLog::SECURE_STORAGE_KEY = "HRDLog";
const QString HRDLog::CONFIG_CALLSIGN_KEY = "hrdlog/callsign";
const QString HRDLog::CONFIG_ONAIR_ENABLED_KEY = "hrdlog/onair";
