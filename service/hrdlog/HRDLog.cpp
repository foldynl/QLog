#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDomDocument>
#include <QTimer>
#include <QSet>

#include "HRDLog.h"
#include "core/debug.h"
#include "core/CredentialStore.h"
#include "rig/macros.h"
#include "core/LogParam.h"
#include "data/Data.h"
#include "data/Callsign.h"

MODULE_IDENTIFICATION("qlog.service.hrdlog.hrdlog");

const QString HRDLogBase::SECURE_STORAGE_KEY = "HRDLog";
REGISTRATION_SECURE_SERVICE(HRDLogBase);

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

    return getPassword(HRDLogBase::SECURE_STORAGE_KEY,
                       getRegisteredCallsign());
}

void HRDLogBase::saveUploadCode(const QString &newUsername, const QString &newPassword)
{
    FCT_IDENTIFICATION;

    QString oldUsername = getRegisteredCallsign();
    if ( oldUsername != newUsername )
    {
        deletePassword(HRDLogBase::SECURE_STORAGE_KEY,
                       oldUsername);
    }

    LogParam::setHRDLogLogbookReqCallsign(newUsername);

    savePassword(HRDLogBase::SECURE_STORAGE_KEY,
                 newUsername, newPassword);
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

QStringList HRDLogBase::getAddlCallsigns()
{
    FCT_IDENTIFICATION;

    return LogParam::getHRDLogCallsignsList();
}

void HRDLogBase::setAddlCallsigns(const QStringList &list)
{
    FCT_IDENTIFICATION;

    LogParam::setHRDLogCallsignsList(list);
}

QString HRDLogBase::getCallsignUploadCode(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    return getPassword(HRDLogBase::SECURE_STORAGE_KEY, callsign);
}

void HRDLogBase::saveCallsignUploadCode(const QString &newCode, const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    // the default account has the same storage - its upload code must stay
    if ( callsign.compare(getRegisteredCallsign(), Qt::CaseInsensitive) == 0 )
        return;

    deletePassword(HRDLogBase::SECURE_STORAGE_KEY, callsign);

    if ( !newCode.isEmpty() )
        savePassword(HRDLogBase::SECURE_STORAGE_KEY, callsign, newCode);
}

QString HRDLogBase::resolveAccountCallsign(const QString &stationCallsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << stationCallsign;

    const QString normalizedCallsign = stationCallsign.trimmed().toUpper();
    const QString &defaultCallsign = getRegisteredCallsign();

    // QSOs without the station callsign keep using the default account
    if ( normalizedCallsign.isEmpty() )
        return defaultCallsign;

    const QStringList &addlCallsigns = getAddlCallsigns();

    if ( addlCallsigns.contains(normalizedCallsign) )
        return normalizedCallsign;

    if ( normalizedCallsign.compare(defaultCallsign, Qt::CaseInsensitive) == 0 )
        return defaultCallsign;

    // portable and prefixed callsigns (SQ8BWA/P, SP/SQ8BWA) use the account of the base
    // callsign unless they have an account of their own
    const Callsign callsign(normalizedCallsign);

    if ( !callsign.isValid() )
        return QString();

    if ( addlCallsigns.contains(callsign.getBase()) )
        return callsign.getBase();

    const Callsign parsedDefaultCallsign(defaultCallsign);

    if ( parsedDefaultCallsign.isValid() && parsedDefaultCallsign.getBase() == callsign.getBase() )
        return defaultCallsign;

    // HRDLog takes the station callsign from the account - another account must not be used
    return QString();
}

bool HRDLogBase::isConfigured()
{
    FCT_IDENTIFICATION;

    return !getRegisteredCallsign().isEmpty() || !getAddlCallsigns().isEmpty();
}

void HRDLogBase::registerCredentials()
{
    // both storage keys belong to the same logical service
    CredentialRegistry::instance().add(SECURE_STORAGE_KEY, []()
    {
        QList<CredentialDescriptor> ret;

        ret.append({ SECURE_STORAGE_KEY, [](){ return getRegisteredCallsign(); } });

        const QStringList &addlCallsigns = getAddlCallsigns();

        for ( QString callsign : addlCallsigns ) // do not use reference - the lambda keeps a copy
            ret.append({ SECURE_STORAGE_KEY, [callsign]() { return callsign; } });

        return ret;
    });
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
                                const QString &accountCallsign,
                                bool update)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << contactID << accountCallsign << update;

    QUrlQuery params;
    params.addQueryItem("Callsign", accountCallsign.toUtf8().toPercentEncoding());
    params.addQueryItem("Code", getCallsignUploadCode(accountCallsign).toUtf8().toPercentEncoding());
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
    currentReply->setProperty("accountCallsign", accountCallsign);
}

QString HRDLogUploader::recordStationCallsign(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    // based on LoTW source code and ADIF spec - priority STATION_CALLSIGN, OPERATOR
    const QString stationCallsign = record.value("station_callsign").toString();

    return stationCallsign.isEmpty() ? record.value("operator").toString() : stationCallsign;
}

void HRDLogUploader::uploadContact(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    QByteArray data = generateADIF({record});
    cancelUpload = false;
    uploadAdif(data.trimmed(),
               record.value("id"),
               resolveAccountCallsign(recordStationCallsign(record)),
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

    // HRDLog takes the station callsign from the account - QSOs of a callsign without
    // an account would be logged as QSOs of the default account callsign
    QStringList callsignsWithoutAccount;
    QSet<QString> checkedCallsigns; // the callsign is parsed only once

    for ( const QSqlRecord &qso : qsos )
    {
        const QString stationCallsign = recordStationCallsign(qso).trimmed().toUpper();

        if ( checkedCallsigns.contains(stationCallsign) )
            continue;

        checkedCallsigns.insert(stationCallsign);

        if ( resolveAccountCallsign(stationCallsign).isEmpty() )
            callsignsWithoutAccount << stationCallsign;
    }

    if ( !callsignsWithoutAccount.isEmpty() )
    {
        const QString error = tr("No HRDLog account is configured for %1").arg(callsignsWithoutAccount.join(", "));
        // report it asynchronously like any other upload error
        QTimer::singleShot(0, this, [this, error]() { emit uploadError(error); });
        return;
    }

    cancelUpload = false;
    queuedContacts4Upload = qsos;

    uploadContact(queuedContacts4Upload.first());
    queuedContacts4Upload.removeFirst();
}

void HRDLogUploader::sendOnAir(double freq, const QString &mode, const QString &stationCallsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << freq << mode << stationCallsign;

    // the account of the current station callsign, otherwise the default one as before
    QString accountCallsign = resolveAccountCallsign(stationCallsign);
    if ( accountCallsign.isEmpty() )
        accountCallsign = getRegisteredCallsign();

    QUrlQuery params;

    params.addQueryItem("Callsign", accountCallsign.toUtf8().toPercentEncoding());
    params.addQueryItem("Code", getCallsignUploadCode(accountCallsign).toUtf8().toPercentEncoding());
    params.addQueryItem("App", "QLog");
    params.addQueryItem("Frequency", QString::number(MHz2Hz(freq)));
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
                               reply->property("contactID"),
                               reply->property("accountCallsign").toString());
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
