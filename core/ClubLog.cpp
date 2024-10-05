#include <QSettings>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QSqlError>
#include <QSqlField>
#include "logformat/AdiFormat.h"
#include "ClubLog.h"
#include "debug.h"
#include "core/CredentialStore.h"

#define API_KEY "21507885dece41ca049fec7fe02a813f2105aff2"
#define API_LIVE_UPLOAD_URL "https://clublog.org/realtime.php"
#define API_LIVE_DELETE_URL "https://clublog.org/delete.php"
#define API_LOG_UPLOAD_URL "https://clublog.org/putlogs.php"

MODULE_IDENTIFICATION("qlog.core.clublog");

ClubLog::ClubLog(QObject *parent) :
    QObject(parent)
{
    FCT_IDENTIFICATION;

    if ( !query_updateRT.prepare("UPDATE contacts "
                                 "SET clublog_qso_upload_status='Y', clublog_qso_upload_date = strftime('%Y-%m-%d',DATETIME('now', 'utc')) "
                                 "WHERE id = :id AND callsign = :callsign") )
        qCWarning(runtime) << "Update statement is not prepared";

    nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished, this, &ClubLog::processReply);
}

ClubLog::~ClubLog()
{
    FCT_IDENTIFICATION;

    nam->deleteLater();

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

const QString ClubLog::getEmail()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    return settings.value(ClubLog::CONFIG_EMAIL_KEY).toString().trimmed();

}

bool ClubLog::isUploadImmediatelyEnabled()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    return settings.value(ClubLog::CONFIG_UPLOAD_IMMEDIATELY_KEY, false).toBool();
}

const QString ClubLog::getPassword()
{
    FCT_IDENTIFICATION;

    return CredentialStore::instance()->getPassword(ClubLog::SECURE_STORAGE_KEY,
                                                    getEmail());
}

void ClubLog::saveUsernamePassword(const QString &newEmail, const QString &newPassword)
{
    FCT_IDENTIFICATION;

    QSettings settings;

    const QString &oldEmail = getEmail();
    if ( oldEmail != newEmail )
    {
        CredentialStore::instance()->deletePassword(ClubLog::SECURE_STORAGE_KEY,
                                                    oldEmail);
    }
    settings.setValue(ClubLog::CONFIG_EMAIL_KEY, newEmail);

    CredentialStore::instance()->savePassword(ClubLog::SECURE_STORAGE_KEY,
                                              newEmail,
                                              newPassword);

}

void ClubLog::saveUploadImmediatelyConfig(bool value)
{
    FCT_IDENTIFICATION;

    QSettings settings;

    settings.setValue(ClubLog::CONFIG_UPLOAD_IMMEDIATELY_KEY, value);
}

void ClubLog::sendRealtimeRequest(const OnlineCommand command,
                                  const QSqlRecord &record,
                                  const QString &uploadCallsign)
{
    FCT_IDENTIFICATION;

    static QRegularExpression rx("[a-zA-Z]");

    qCDebug(function_parameters) << command << uploadCallsign;// << record;

    if ( !isUploadImmediatelyEnabled() )
    {
        qCDebug(runtime) << "Instant Upload is disabled, no action";
        return;
    }

    const QString &email = getEmail();
    const QString &password = getPassword();

    if ( email.isEmpty()
         || uploadCallsign.isEmpty()
         || password.isEmpty() )
        return;

    QUrl url;
    QUrlQuery query;
    query.addQueryItem("email", email.toUtf8().toPercentEncoding());
    query.addQueryItem("callsign", uploadCallsign.toUtf8().toPercentEncoding());
    query.addQueryItem("password", password.toUtf8().toPercentEncoding());
    query.addQueryItem("api", API_KEY);

    switch (command)
    {
    case ClubLog::INSERT_QSO:
    {
        url.setUrl(API_LIVE_UPLOAD_URL);
        QByteArray data;
        QTextStream stream(&data, QIODevice::ReadWrite);
        AdiFormat adi(stream);
        adi.exportContact(record);
        stream.flush();
        data.replace("\n", " ");
        query.addQueryItem("adif", data.trimmed().toPercentEncoding());
    }
        break;
    case ClubLog::UPDATE_QSO:
    case ClubLog::DELETE_QSO:
        url.setUrl(API_LIVE_DELETE_URL);
        query.addQueryItem("dxcall", record.value("callsign").toByteArray());
        query.addQueryItem("datetime", record.value("start_time").toDateTime().toTimeSpec(Qt::UTC).toString("yyyy-MM-dd hh:mm:ss").toUtf8());
        query.addQueryItem("bandid", record.value("band").toString().replace(rx, "").toUtf8()); //clublog support non-ADIF bands enumaration, need remove m, cm, mm string
        break;
    default:
        qCWarning(runtime) << "Unsupported RT Command" << command;
        return;
    }

    qCDebug(runtime) << query.query();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *currentReply = nam->post(request, query.query(QUrl::FullyEncoded).toUtf8());

    QVariant messageType;
    switch ( command )
    {
    case ClubLog::INSERT_QSO:
        messageType = "realtimeInsert";
        currentReply->setProperty("dxcall", record.value("callsign"));
        break;
    case ClubLog::UPDATE_QSO:
        messageType = "realtimeUpdate";
        RTupdatesInProgress.insert(record.value("id").toULongLong(), record);
        break;
    case ClubLog::DELETE_QSO:
        messageType = "realtimeDelete";
        break;
    }

    currentReply->setProperty("contactID", record.value("id"));
    currentReply->setProperty("messageType", messageType);
    currentReply->setProperty("uploadCallsign", uploadCallsign);
    activeReplies << currentReply;
}


void ClubLog::uploadAdif(QByteArray& data,
                         const QString &uploadCallsign,
                         bool clearFlag)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << data;

    const QString &email = getEmail();
    const QString &password = getPassword();

    if ( email.isEmpty()
         || uploadCallsign.isEmpty()
         || password.isEmpty() )
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
    apiPart.setBody(API_KEY);

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
    {
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";
    }

    QNetworkReply * currentReply = nam->post(request, multipart);
    currentReply->setProperty("messageType", QVariant("uploadADIFFile"));
    currentReply->setProperty("uploadCallsign", uploadCallsign);
    multipart->setParent(currentReply);
    activeReplies << currentReply;
}

void ClubLog::processReply(QNetworkReply* reply)
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
        emit uploadFileOK("OK");
    }
    /******************/
    /* realtimeInsert */
    /******************/
    else if ( messageType == "realtimeInsert" )
    {
        query_updateRT.bindValue(":id", reply->property("contactID"));
        query_updateRT.bindValue(":callsign", reply->property("dxcall")); //to be sure that the QSO with the ID is still the same sa before sending
        if ( !query_updateRT.exec() )
        {
            qCWarning(runtime) << "RT Response: SQL Error" << query_updateRT.lastError();
        }
        else
        {
            emit QSOUploaded();
        }
    }
    /******************/
    /* realtimeUpdate */
    /******************/
    else if ( messageType == "realtimeUpdate")
    {
        QSqlRecord insertRecord = RTupdatesInProgress.take(reply->property("contactID").toULongLong());

        if ( insertRecord != QSqlRecord() )
        {
            sendRealtimeRequest(ClubLog::INSERT_QSO,
                                insertRecord,
                                reply->property("uploadCallsign").toString());
        }
        else
        {
            qWarning() << "Cannot find record for update in Update In-Progress Table";
        }
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
    {
        qWarning() << "Unrecognized Clublog reponse" << reply->property("messageType").toString();
    }

    reply->deleteLater(); 
}

void ClubLog::abortRequest()
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

void ClubLog::insertQSOImmediately(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    sendRealtimeRequest(ClubLog::INSERT_QSO,
                        record,
                        generateUploadCallsign(record));
}

void ClubLog::updateQSOImmediately(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    const QString &uploadStatus = record.value("clublog_qso_upload_status").toString();

    if ( uploadStatus.isEmpty()
         || uploadStatus == "N" )
    {
        qCDebug(runtime) << "QSO would not be uploaded to Clublog - nothing to do";
        return;
    }
    sendRealtimeRequest(ClubLog::UPDATE_QSO,
                        record,
                        generateUploadCallsign(record));
}

void ClubLog::deleteQSOImmediately(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    const QString &uploadStatus = record.value("clublog_qso_upload_status").toString();

    if ( uploadStatus.isEmpty()
         || uploadStatus == "N" )
    {
        qCDebug(runtime) << "QSO would not be uploaded to Clublog - nothing to do";
        return;
    }

    sendRealtimeRequest(ClubLog::DELETE_QSO,
                        record,
                        generateUploadCallsign(record));
}

const QString ClubLog::generateUploadCallsign(const QSqlRecord &record) const
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

QSqlRecord ClubLog::stripRecord(const QSqlRecord &inRecord)
{
    FCT_IDENTIFICATION;

    QSqlRecord ret;

    for ( int i = 0; i < inRecord.count(); i++ )
    {
        QSqlField curr = inRecord.field(i);
        if ( supportedDBFields.contains(curr.name()) )
        {
            ret.append(curr);
        }
    }

    qCDebug(runtime) << "Stripped" << ret;

    return ret;
}


const QString ClubLog::SECURE_STORAGE_KEY = "Clublog";
const QString ClubLog::CONFIG_EMAIL_KEY = "clublog/email";
//const QString ClubLog::CONFIG_CALLSIGN_KEY = "clublog/callsign";  //TODO Remove later
const QString ClubLog::CONFIG_UPLOAD_IMMEDIATELY_KEY = "clublog/upload_immediately";

// https://clublog.freshdesk.com/support/solutions/articles/53202-which-adif-fields-does-club-log-use-
QStringList ClubLog::supportedDBFields = {"start_time",
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
