#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtXml>
#include <QSqlError>
#include "QRZCallEU.h"
#include "core/debug.h"
#include "core/CredentialStore.h"
#include "core/LogParam.h"
#include "data/Callsign.h"
#include "data/Data.h"

// QRZCALL.EU exposes QRZ-compatible endpoints:
//   callbook lookup - https://api.qrzcall.eu/v1/pub/callsign_xml.php
//   QSO upload      - https://api.qrzcall.eu/v1/pub/logbook_api.php
// A single Personal Access Token (pat_...) authenticates both.

MODULE_IDENTIFICATION("qlog.core.qrzcalleu");

const QString QRZCallEUBase::SECURE_STORAGE_API_KEY = "QRZCALLEU";
const QString QRZCallEUBase::CONFIG_USERNAME_API_CONST = "qrzcalleuapi";
const QString QRZCallEUCallbook::CALLBOOK_NAME = "qrzcalleu";

REGISTRATION_SECURE_SERVICE(QRZCallEUBase);

const QString QRZCallEUBase::getPAT()
{
    FCT_IDENTIFICATION;

    return getPassword(SECURE_STORAGE_API_KEY, getUsername());
}

void QRZCallEUBase::savePAT(const QString &newPAT)
{
    FCT_IDENTIFICATION;

    deletePassword(SECURE_STORAGE_API_KEY, getUsername());

    if ( newPAT.isEmpty() )
        return;

    savePassword(SECURE_STORAGE_API_KEY, getUsername(), newPAT);
}

bool QRZCallEUBase::isUploadImmediatelyEnabled()
{
    FCT_IDENTIFICATION;

    return LogParam::getQRZCallEUUploadImmediatelyEnabled();
}

void QRZCallEUBase::saveUploadImmediatelyConfig(bool value)
{
    FCT_IDENTIFICATION;

    LogParam::setQRZCallEUUploadImmediatelyEnabled(value);
}

void QRZCallEUBase::registerCredentials()
{
    CredentialRegistry::instance().add(SECURE_STORAGE_API_KEY, []()
    {
        return QList<CredentialDescriptor>
        {
            { SECURE_STORAGE_API_KEY, [](){ return getUsername(); } }
        };
    });
}

/**********/
/* Lookup */
/**********/

QRZCallEUCallbook::QRZCallEUCallbook(QObject *parent) :
    GenericCallbook(parent),
    QRZCallEUBase(),
    currentReply(nullptr)
{
    FCT_IDENTIFICATION;
}

QRZCallEUCallbook::~QRZCallEUCallbook()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
        currentReply->abort();
}

QString QRZCallEUCallbook::getDisplayName()
{
    FCT_IDENTIFICATION;

    return QString(tr("QRZCALL.EU"));
}

void QRZCallEUCallbook::queryCallsign(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    const QString &pat = getPAT();

    if ( pat.isEmpty() )
    {
        qCDebug(runtime) << "Empty QRZCALL.EU token";
        emit callsignNotFound(callsign);
        return;
    }

    const Callsign qCall(callsign);

    QUrlQuery params;
    // QRZCALL.EU, like QRZ.com, looks up the base callsign best.
    params.addQueryItem("callsign", (qCall.isValid()) ? qCall.getBase()
                                                      : callsign);

    QUrl url(API_URL);
    url.setQuery(params);

    if ( currentReply )
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArray("Bearer ") + pat.toUtf8());
    request.setRawHeader("User-Agent", QString("QLog/%1").arg(VERSION).toUtf8());

    qCDebug(runtime) << url.toString(QUrl::RemoveQuery);

    currentReply = getNetworkAccessManager()->get(request);
    currentReply->setProperty("queryCallsign", QVariant(callsign));
}

void QRZCallEUCallbook::abortQuery()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        currentReply = nullptr;
    }
}

void QRZCallEUCallbook::processReply(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    /* always process one request per class */
    currentReply = nullptr;

    const int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString &queryCallsign = reply->property("queryCallsign").toString();

    if ( reply->error() != QNetworkReply::NoError
         && reply->error() != QNetworkReply::ContentAccessDenied
         && reply->error() != QNetworkReply::ContentNotFoundError
         && reply->error() != QNetworkReply::AuthenticationRequiredError )
    {
        qCDebug(runtime) << "QRZCALL.EU error" << reply->errorString();
        qCDebug(runtime) << "HTTP Status Code" << replyStatusCode;

        if ( reply->error() != QNetworkReply::OperationCanceledError )
        {
            emit lookupError(reply->errorString());
            reply->deleteLater();
        }
        return;
    }

    /* An expired/invalid token is reported by QRZCALL.EU as HTTP 401/403. */
    if ( replyStatusCode == 401 || replyStatusCode == 403 )
    {
        qCDebug(runtime) << "QRZCALL.EU authentication failed - HTTP" << replyStatusCode;
        emit loginFailed();
        emit lookupError(tr("QRZCALL.EU token is invalid or the subscription has expired"));
        reply->deleteLater();
        return;
    }

    const QByteArray &response = reply->readAll();
    qCDebug(runtime) << response;

    QXmlStreamReader xml(response);
    CallbookResponseData responseData;

    while ( !xml.atEnd() && !xml.hasError() )
    {
        const QXmlStreamReader::TokenType token = xml.readNext();

        if ( token != QXmlStreamReader::StartElement )
            continue;

        const QString elementName = xml.name().toString();

        if ( elementName == "Error" )
        {
            const QString &errorString = xml.readElementText();

            if ( replyStatusCode == 404
                 || errorString.contains("not found", Qt::CaseInsensitive) )
            {
                emit callsignNotFound(queryCallsign);
            }
            else
            {
                qCInfo(runtime) << "QRZCALL.EU Error -" << errorString;
                emit lookupError(errorString);
            }
            continue;
        }

        if      (elementName == "call")      responseData.call = decodeHtmlEntities(xml.readElementText().toUpper());
        else if (elementName == "fname")     responseData.fname = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "name")      responseData.lname = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "nickname")  responseData.nick = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "born")      responseData.born = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "addr1")     responseData.addr1 = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "attn")      responseData.qth = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "state")     responseData.us_state = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "zip")       responseData.zipcode = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "county")    responseData.county = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "country")   responseData.country = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "dxcc")      responseData.dxcc = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "lat")       responseData.latitude = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "lon")       responseData.longitude = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "ituzone")   responseData.ituz = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "cqzone")    responseData.cqz = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "grid")      responseData.gridsquare = decodeHtmlEntities(xml.readElementText().toUpper());
        else if (elementName == "iota")      responseData.iota = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "efdate")    responseData.lic_year = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "qslmgr")    responseData.qsl_via = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "email")     responseData.email = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "url")       responseData.url = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "image")     responseData.image_url = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "lotw")      responseData.lotw = decodeHtmlEntities(xml.readElementText().toUpper());
        else if (elementName == "eqsl")      responseData.eqsl = decodeHtmlEntities(xml.readElementText().toUpper());
        else if (elementName == "mqsl")      responseData.pqsl = decodeHtmlEntities(xml.readElementText().toUpper());
    }

    if ( !responseData.call.isEmpty() )
        emit callsignResult(responseData);

    reply->deleteLater();
}

/**********/
/* Upload */
/**********/

QRZCallEUUploader::QRZCallEUUploader(QObject *parent) :
    GenericQSOUploader(QStringList(), parent),
    QRZCallEUBase(),
    currentReply(nullptr),
    cancelUpload(false)
{
    FCT_IDENTIFICATION;

    // Used by the real-time ("Immediately Upload") path to mark a QSO as
    // uploaded directly in the database after a successful upload.
    if ( !query_updateRT.prepare("UPDATE contacts "
                                 "SET qrzcalleu_qso_upload_status = 'Y', "
                                 "    qrzcalleu_qso_upload_date = strftime('%Y-%m-%d', DATETIME('now', 'utc')) "
                                 "WHERE id = :id") )
        qCWarning(runtime) << "Cannot prepare the real-time upload-status update";
}

QRZCallEUUploader::~QRZCallEUUploader()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        currentReply->deleteLater();
    }
}

void QRZCallEUUploader::uploadContact(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    QByteArray data = generateADIF({record});
    cancelUpload = false;

    actionInsert(getPAT(), data, "REPLACE");
    currentReply->setProperty("contactID", record.value("id"));
}

void QRZCallEUUploader::uploadQSOList(const QList<QSqlRecord> &qsos, const QVariantMap &)
{
    FCT_IDENTIFICATION;

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

void QRZCallEUUploader::sendRealtimeUpload(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    QByteArray data = generateADIF({record});
    cancelUpload = false;

    actionInsert(getPAT(), data, "REPLACE");
    currentReply->setProperty("contactID", record.value("id"));
    // Tag this reply as real-time so processReply() writes the upload status
    // straight into the database (no UploadQSODialog is involved here).
    currentReply->setProperty("messageType", QVariant("realtimeInsert"));
}

void QRZCallEUUploader::insertQSOImmediately(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    if ( !isUploadImmediatelyEnabled() )
        return;

    if ( getPAT().isEmpty() )
    {
        qCDebug(runtime) << "QRZCALL.EU token not set - skipping real-time upload";
        return;
    }

    sendRealtimeUpload(record);
}

void QRZCallEUUploader::updateQSOImmediately(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    if ( !isUploadImmediatelyEnabled() )
        return;

    // Only re-upload QSOs that were already sent to QRZCALL.EU; an edit of a
    // never-uploaded QSO has nothing to update there.
    const QString &uploadStatus = record.value("qrzcalleu_qso_upload_status").toString();

    if ( uploadStatus.isEmpty() || uploadStatus == "N" )
    {
        qCDebug(runtime) << "QSO not previously uploaded to QRZCALL.EU - nothing to update";
        return;
    }

    if ( getPAT().isEmpty() )
        return;

    sendRealtimeUpload(record);
}

void QRZCallEUUploader::actionInsert(const QString &pat, QByteArray &data, const QString &insertPolicy)
{
    FCT_IDENTIFICATION;

    QUrlQuery params;
    params.addQueryItem("KEY", pat);
    params.addQueryItem("ACTION", "INSERT");
    params.addQueryItem("OPTION", insertPolicy);
    params.addQueryItem("ADIF", data.trimmed().toPercentEncoding());

    QUrl url(API_LOGBOOK_URL);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setRawHeader("User-Agent", QString("QLog/%1").arg(VERSION).toUtf8());

    qCDebug(runtime) << Data::safeQueryString(params);

    if ( currentReply )
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";

    currentReply = getNetworkAccessManager()->post(request, params.query(QUrl::FullyEncoded).toUtf8());
    currentReply->setProperty("messageType", QVariant("actionsInsert"));
}

void QRZCallEUUploader::abortRequest()
{
    FCT_IDENTIFICATION;

    cancelUpload = true;

    if ( currentReply )
    {
        currentReply->abort();
        currentReply = nullptr;
    }
}

void QRZCallEUUploader::processReply(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    /* always process one request per class */
    currentReply = nullptr;

    const int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if ( reply->error() != QNetworkReply::NoError
         || replyStatusCode < 200
         || replyStatusCode >= 300 )
    {
        qCDebug(runtime) << "QRZCALL.EU error" << reply->errorString();
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

    // "actionsInsert"  - QSO from the manual UploadQSODialog queue
    // "realtimeInsert" - QSO uploaded immediately on log/edit ("Immediately Upload")
    if ( messageType == "actionsInsert" || messageType == "realtimeInsert" )
    {
        const QString replyString(reply->readAll());
        qCDebug(runtime) << replyString;

        const QMap<QString, QString> &data = parseActionResponse(replyString);
        const QString &status = data.value("RESULT", "FAIL");
        const QString &reason = data.value("REASON", QString());
        const qulonglong contactID = reply->property("contactID").toULongLong();

        // RESULT=OK      - QSO inserted
        // RESULT=REPLACE - QSO already present, updated in place
        // RESULT=FAIL with a "duplicate" reason - QSO already present; treat
        //                as success so re-uploads do not block the queue.
        const bool uploadOK = ( status == "OK" )
                              || ( status == "REPLACE" )
                              || ( status == "FAIL" && reason.contains("duplicate", Qt::CaseInsensitive) );

        if ( uploadOK )
        {
            qCDebug(runtime) << "Confirmed Upload for QSO Id" << contactID;

            // Real-time path: write the upload status straight to the DB.
            // The manual path leaves that to UploadQSODialog.
            if ( messageType == "realtimeInsert" )
            {
                query_updateRT.bindValue(":id", contactID);
                if ( !query_updateRT.exec() )
                    qCWarning(runtime) << "Cannot update real-time upload status" << query_updateRT.lastError();
            }

            emit uploadedQSO(contactID);

            if ( messageType == "actionsInsert" )
            {
                if ( queuedContacts4Upload.isEmpty() )
                {
                    cancelUpload = false;
                    emit uploadFinished();
                }
                else if ( !cancelUpload )
                {
                    uploadContact(queuedContacts4Upload.first());
                    queuedContacts4Upload.removeFirst();
                }
            }
        }
        else
        {
            emit uploadError(reason.isEmpty() ? tr("General Error") : reason);
            cancelUpload = false;
        }
    }

    reply->deleteLater();
}

QMap<QString, QString> QRZCallEUUploader::parseActionResponse(const QString &responseString) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << responseString;

    QMap<QString, QString> data;
    const QStringList &parsedResponse = responseString.split("&");

    for ( const QString &param : parsedResponse )
    {
        const QStringList parsedParams = param.split("=");

        if ( parsedParams.count() == 1 )
            data[parsedParams.at(0)] = QString();
        else if ( parsedParams.count() >= 2 )
            data[parsedParams.at(0)] = QUrl::fromPercentEncoding(parsedParams.at(1).toUtf8());
    }

    return data;
}
