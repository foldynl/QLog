#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDomDocument>

#include "HRDLog.h"
#include "core/debug.h"
#include "core/CredentialStore.h"
#include "rig/macros.h"
#include "core/LogParam.h"
#include "data/Data.h"

MODULE_IDENTIFICATION("qlog.core.hrdlog");

const QString HRDLogBase::SECURE_STORAGE_KEY = "HRDLog";

// http://www.iw1qlh.net/projects/hrdlog/HRDLognet_4.pdf

QStringList HRDLogUploader::uploadedFields =
{
    "start_time",
    "freq",
    "band",
    "callsign",
    "mode",
    "submode",
    "rst_rcvd",
    "rst_sent",
    "qsl_sent",
    "qsl_rcvd",
    "qsl_via",
    "eqsl_qslrdate",
    "eqsl_qsl_rcvd",
    "eqsl_qslsdate",
    "eqsl_qsl_sent",
    "lotw_qsl_rcvd",
    "lotw_qslrdate",
    "lotw_qsl_sent",
    "lotw_qslsdate",
    "gridsquare",
    "dxcc",
    "qslmsg",
    "comment",
    "distance",
    "operator",
    "sat_mode",
    "sat_name",
    "prop_mode"
};

const QString HRDLogBase::getRegisteredCallsign()
{
    FCT_IDENTIFICATION;

    return LogParam::getHRDLogLogbookReqCallsign();
}

const QString HRDLogBase::getUploadCode()
{
    FCT_IDENTIFICATION;

    return CredentialStore::instance()->getPassword(HRDLogBase::SECURE_STORAGE_KEY,
                                                    getRegisteredCallsign());
}

void HRDLogBase::saveUploadCode(const QString &newUsername, const QString &newPassword)
{
    FCT_IDENTIFICATION;

    QString oldUsername = getRegisteredCallsign();
    if ( oldUsername != newUsername )
    {
        CredentialStore::instance()->deletePassword(HRDLogBase::SECURE_STORAGE_KEY,
                                                    oldUsername);
    }

    LogParam::setHRDLogLogbookReqCallsign(newUsername);

    CredentialStore::instance()->savePassword(HRDLogBase::SECURE_STORAGE_KEY,
                                              newUsername,
                                              newPassword);
}

bool HRDLogBase::getOnAirEnabled()
{
    FCT_IDENTIFICATION;

    return LogParam::getHRDLogOnAir();
}

void HRDLogBase::saveOnAirEnabled(bool state)
{
    FCT_IDENTIFICATION;

    LogParam::setHRDLogOnAir(state);
}

// http://www.iw1qlh.net/projects/hrdlog/HRDLognet_4.pdf
// It is not clear what QLog should send to HRDLog. Therefore it will
// send all ADIF-fields

HRDLogUploader::HRDLogUploader(QObject *parent)
    : GenericQSOUploader(uploadedFields, parent),
      HRDLogBase(),
      currentReply(nullptr),
      cancelUpload(false)
{
    FCT_IDENTIFICATION;
}

HRDLogUploader::~HRDLogUploader()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        currentReply->deleteLater();
    }
}

void HRDLogUploader::abortRequest()
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

void HRDLogUploader::uploadAdif(const QByteArray &data,
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

    qCDebug(runtime) << Data::safeQueryString(params);

    if ( currentReply )
    {
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";
    }

    currentReply = getNetworkAccessManager()->post(request, params.query(QUrl::FullyEncoded).toUtf8());
    currentReply->setProperty("messageType", "uploadQSO");
    currentReply->setProperty("ADIFData", data);
    currentReply->setProperty("contactID", contactID);
}

void HRDLogUploader::uploadContact(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    QByteArray data = generateADIF({record});
    cancelUpload = false;
    uploadAdif(data.trimmed(),
               record.value("id"),
               (record.value("hrdlog_qso_upload_status").toString() == "M"));
}

void HRDLogUploader::uploadQSOList(const QList<QSqlRecord> &qsos, const QVariantMap &)
{
    FCT_IDENTIFICATION;

    /* always process one requests per class */

    if ( qsos.isEmpty() )
    {
        /* Nothing to do */
        emit uploadFinished();
        return;
    }

    cancelUpload = false;
    queuedContacts4Upload = qsos;

    uploadContact(queuedContacts4Upload.first());
    queuedContacts4Upload.removeFirst();
}

void HRDLogUploader::sendOnAir(double freq, const QString &mode)
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

    qCDebug(runtime) << Data::safeQueryString(params);

    if ( currentReply )
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";

    currentReply = getNetworkAccessManager()->post(request, params.query(QUrl::FullyEncoded).toUtf8());
    currentReply->setProperty("messageType", QVariant("onAir"));
}

void HRDLogUploader::processReply(QNetworkReply *reply)
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
                emit uploadedQSO(reply->property("contactID").toULongLong());

                if ( queuedContacts4Upload.isEmpty() )
                {
                    cancelUpload = false;
                    emit uploadFinished();
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

