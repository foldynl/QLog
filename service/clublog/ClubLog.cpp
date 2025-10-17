#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QSqlError>
#include <QSqlField>
#include <QTimeZone>
#include "ClubLog.h"
#include "core/debug.h"
#include "core/CredentialStore.h"
#include "core/LogParam.h"
#include "data/Data.h"

MODULE_IDENTIFICATION("qlog.core.clublog");

const QString ClubLogBase::SECURE_STORAGE_KEY = "Clublog";

// https://clublog.freshdesk.com/support/solutions/articles/53202-which-adif-fields-does-club-log-use-
QStringList ClubLogUploader::uploadedFields =
{
    "start_time",
    "qsl_rdate",
    "qsl_sdate",
    "callsign",
    "operator",
    "mode",
    "band",
    "band_rx",
    "freq",
    "qsl_rcvd",
    "lotw_qsl_rcvd",
    "qsl_sent",
    "dxcc",
    "prop_mode",
    "credit_granted",
    "rst_sent",
    "rst_rcvd",
    "notes",
    "gridsquare",
    "vucc_grids",
    "sat_name"
};

const QString ClubLogBase::getEmail()
{
    FCT_IDENTIFICATION;

    return LogParam::getClublogLogbookReqEmail();
}

bool ClubLogBase::isUploadImmediatelyEnabled()
{
    FCT_IDENTIFICATION;

    return LogParam::getClublogUploadImmediatelyEnabled();
}

const QString ClubLogBase::getPassword()
{
    FCT_IDENTIFICATION;

    return CredentialStore::instance()->getPassword(ClubLogBase::SECURE_STORAGE_KEY,
                                                    getEmail());
}

void ClubLogBase::saveUsernamePassword(const QString &newEmail, const QString &newPassword)
{
    FCT_IDENTIFICATION;

    const QString &oldEmail = getEmail();
    if ( oldEmail != newEmail )
    {
        CredentialStore::instance()->deletePassword(ClubLogBase::SECURE_STORAGE_KEY,
                                                    oldEmail);
    }
    LogParam::setClublogLogbookReqEmail(newEmail);
    CredentialStore::instance()->savePassword(ClubLogBase::SECURE_STORAGE_KEY,
                                              newEmail,
                                              newPassword);
}

void ClubLogBase::saveUploadImmediatelyConfig(bool value)
{
    FCT_IDENTIFICATION;

    LogParam::setClublogUploadImmediatelyEnabled(value);
}


ClubLogUploader::ClubLogUploader(QObject *parent) :
    GenericQSOUploader(uploadedFields, parent),
    ClubLogBase(),
    rx("[a-zA-Z]")
{
    FCT_IDENTIFICATION;

    if ( !query_updateRT.prepare("UPDATE contacts "
                                 "SET clublog_qso_upload_status='Y', clublog_qso_upload_date = strftime('%Y-%m-%d',DATETIME('now', 'utc')) "
                                 "WHERE id = :id AND callsign = :callsign") )
        qCWarning(runtime) << "Update statement is not prepared";
}

ClubLogUploader::~ClubLogUploader()
{
    FCT_IDENTIFICATION;

    if ( activeReplies.count() > 0 )
    {
        QMutableListIterator<QNetworkReply*> i(activeReplies);

        while ( i.hasNext() )
        {
            QNetworkReply* curr = i.next();
            curr->abort();
            curr->deleteLater();
            i.remove();
        }
    }
}

void ClubLogUploader::sendRealtimeRequest(const OnlineUploadCommand command,
                                          const QSqlRecord &record,
                                          const QString &uploadCallsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << command << uploadCallsign;

    if ( !isUploadImmediatelyEnabled() )
    {
        qCDebug(runtime) << "Instant Upload is disabled, no action";
        return;
    }

    const QString &email = getEmail();
    const QString &password = getPassword();

    if ( email.isEmpty() || uploadCallsign.isEmpty() || password.isEmpty() )
        return;

    QUrl url;
    QUrlQuery query;

    query.addQueryItem("email", email.toUtf8().toPercentEncoding());
    query.addQueryItem("callsign", uploadCallsign.toUtf8().toPercentEncoding());
    query.addQueryItem("password", password.toUtf8().toPercentEncoding());
    query.addQueryItem("api", API_KEY);

    switch (command)
    {
    case ClubLogUploader::INSERT_QSO:
    {
        url.setUrl(API_LIVE_UPLOAD_URL);
        QByteArray data = generateADIF({record});
        data.replace("\n", " ");
        query.addQueryItem("adif", data.trimmed().toPercentEncoding());
    }
        break;
    case ClubLogUploader::UPDATE_QSO:
    case ClubLogUploader::DELETE_QSO:
        url.setUrl(API_LIVE_DELETE_URL);
        query.addQueryItem("dxcall", record.value("callsign").toByteArray());
        query.addQueryItem("datetime", record.value("start_time").toDateTime().toTimeZone(QTimeZone::utc()).toString("yyyy-MM-dd hh:mm:ss").toUtf8());
        query.addQueryItem("bandid", record.value("band").toString().replace(rx, "").toUtf8()); //clublog support non-ADIF bands enumaration, need remove m, cm, mm string
        break;
    default:
        qCWarning(runtime) << "Unsupported RT Command" << command;
        return;
    }

    qCDebug(runtime) << Data::safeQueryString(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *currentReply = getNetworkAccessManager()->post(request, query.query(QUrl::FullyEncoded).toUtf8());

    QVariant messageType;
    switch ( command )
    {
    case ClubLogUploader::INSERT_QSO:
        messageType = "realtimeInsert";
        currentReply->setProperty("dxcall", record.value("callsign"));
        break;
    case ClubLogUploader::UPDATE_QSO:
        messageType = "realtimeUpdate";
        RTupdatesInProgress.insert(record.value("id").toULongLong(), record);
        break;
    case ClubLogUploader::DELETE_QSO:
        messageType = "realtimeDelete";
        break;
    }

    currentReply->setProperty("contactID", record.value("id"));
    currentReply->setProperty("messageType", messageType);
    currentReply->setProperty("uploadCallsign", uploadCallsign);
    activeReplies << currentReply;
}

void ClubLogUploader::uploadQSOList(const QList<QSqlRecord> &qsos, const QVariantMap &addlParams)
{
    FCT_IDENTIFICATION;

    uploadAdif(generateADIF(qsos),
               addlParams["uploadCallsign"].toString(),
               addlParams["clearFlag"].toBool());
}

void ClubLogUploader::uploadAdif(const QByteArray& data,
                         const QString &uploadCallsign,
                         bool clearFlag)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << data;

    const QString &email = getEmail();
    const QString &password = getPassword();

    if ( email.isEmpty() || uploadCallsign.isEmpty() || password.isEmpty() )
        return;

    QUrl url(API_LOG_UPLOAD_URL);

    QHttpMultiPart* multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart emailPart;
    emailPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"email\""));
    emailPart.setBody(email.toUtf8());

    QHttpPart callsignPart;
    callsignPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"callsign\""));
    callsignPart.setBody(uploadCallsign.toUtf8());

    QHttpPart passwordPart;
    passwordPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"password\""));
    passwordPart.setBody(password.toUtf8());

    QHttpPart clearPart;
    clearPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"clear\""));
    clearPart.setBody( (clearFlag) ? "1" : "0");

    QHttpPart apiPart;
    apiPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"api\""));
    apiPart.setBody(API_KEY.toUtf8());

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"file\"; filename=\"clublog.adi\""));
    filePart.setBody(data);

    multipart->append(emailPart);
    multipart->append(passwordPart);
    multipart->append(callsignPart);
    multipart->append(clearPart);
    multipart->append(apiPart);
    multipart->append(filePart);

    QNetworkRequest request(url);

    if ( activeReplies.count() > 0 )
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";

    QNetworkReply * currentReply = getNetworkAccessManager()->post(request, multipart);
    currentReply->setProperty("messageType", QVariant("uploadADIFFile"));
    currentReply->setProperty("uploadCallsign", uploadCallsign);
    multipart->setParent(currentReply);
    activeReplies << currentReply;
}

void ClubLogUploader::processReply(QNetworkReply* reply)
{
    FCT_IDENTIFICATION;

    activeReplies.removeAll(reply);

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if ( reply->error() != QNetworkReply::NoError
         || replyStatusCode < 200
         || replyStatusCode >= 300)
    {
        qCDebug(runtime) << "Clublog error URL " << reply->request().url().toString();
        qCDebug(runtime) << "Clublog error" << reply->errorString();
        qCDebug(runtime) << "HTTP Status Code" << replyStatusCode;

        if ( reply->error() != QNetworkReply::OperationCanceledError )
        {
            emit uploadError(tr("Clublog Operation for Callsign %1 failed.<br>%2").arg(reply->property("uploadCallsign").toString(),
                                                                                    reply->errorString()));
            reply->deleteLater();
        }
        return;
    }

    const QString &messageType = reply->property("messageType").toString();

    qCDebug(runtime) << "Received Message Type: " << messageType;

    /******************/
    /* uploadADIFFile */
    /******************/
    if ( messageType == "uploadADIFFile" )
    {
        emit uploadFinished();
    }
    /******************/
    /* realtimeInsert */
    /******************/
    else if ( messageType == "realtimeInsert" )
    {
        query_updateRT.bindValue(":id", reply->property("contactID"));
        query_updateRT.bindValue(":callsign", reply->property("dxcall")); //to be sure that the QSO with the ID is still the same sa before sending
        if ( !query_updateRT.exec() )
            qCWarning(runtime) << "RT Response: SQL Error" << query_updateRT.lastError();
        else
            emit uploadedQSO(reply->property("contactID").toULongLong());
    }
    /******************/
    /* realtimeUpdate */
    /******************/
    else if ( messageType == "realtimeUpdate")
    {
        QSqlRecord insertRecord = RTupdatesInProgress.take(reply->property("contactID").toULongLong());

        if ( insertRecord != QSqlRecord() )
        {
            sendRealtimeRequest(ClubLogUploader::INSERT_QSO,
                                insertRecord,
                                reply->property("uploadCallsign").toString());
        }
        else
            qWarning() << "Cannot find record for update in Update In-Progress Table";
    }
    /******************/
    /* realtimeDelete */
    /******************/
    else if ( messageType == "realtimeDelete")
    {
        //delete response - no action
    }
    /*************/
    /* Otherwise */
    /*************/
    else
        qWarning() << "Unrecognized Clublog response" << reply->property("messageType").toString();

    reply->deleteLater(); 
}

void ClubLogUploader::abortRequest()
{
    FCT_IDENTIFICATION;

    if ( activeReplies.count() > 0 )
    {
        QMutableListIterator<QNetworkReply*> i(activeReplies);

        while ( i.hasNext() )
        {
            QNetworkReply* curr = i.next();
            curr->abort();
            //curr->deleteLater(); // pointer is deleted later in processReply
            i.remove();
        }
    }
    RTupdatesInProgress.clear();
}

void ClubLogUploader::insertQSOImmediately(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    sendRealtimeRequest(ClubLogUploader::INSERT_QSO,
                        record,
                        generateUploadCallsign(record));
}

void ClubLogUploader::updateQSOImmediately(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    const QString &uploadStatus = record.value("clublog_qso_upload_status").toString();

    if ( uploadStatus.isEmpty() || uploadStatus == "N" )
    {
        qCDebug(runtime) << "QSO would not be uploaded to Clublog - nothing to do";
        return;
    }
    sendRealtimeRequest(ClubLogUploader::UPDATE_QSO,
                        record,
                        generateUploadCallsign(record));
}

void ClubLogUploader::deleteQSOImmediately(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    const QString &uploadStatus = record.value("clublog_qso_upload_status").toString();

    if ( uploadStatus.isEmpty() || uploadStatus == "N" )
    {
        qCDebug(runtime) << "QSO would not be uploaded to Clublog - nothing to do";
        return;
    }

    sendRealtimeRequest(ClubLogUploader::DELETE_QSO,
                        record,
                        generateUploadCallsign(record));
}

const QString ClubLogUploader::generateUploadCallsign(const QSqlRecord &record) const
{
    FCT_IDENTIFICATION;

#if 0
    //for cases when QSOs are uploaded to the Clublog log with QSO's station_callsign without the prefix
    Callsign uploadCallsign(record.value("station_callsign").toString());

    if ( !uploadCallsign.isValid() )
        qCWarning(runtime) << "Station callsign is not valid" << record.value("station_callsign").toString();

    // QSOs are uploaded to the Clublog log with a name such
    // as QSO's station_callsign without the prefix
    return uploadCallsign.getHostPrefixWithDelimiter() + uploadCallsign.getBase();
#endif
    return record.value("station_callsign").toString();
}
