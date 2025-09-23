#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtXml>
#include <QDebug>
#include "QRZ.h"
#include <QMessageBox>
#include "core/debug.h"
#include "core/CredentialStore.h"
#include "data/Callsign.h"
#include "core/LogParam.h"
#include "data/Data.h"

//https://www.qrz.com/docs/logbook/QRZLogbookAPI.html

MODULE_IDENTIFICATION("qlog.core.qrz");
const QString QRZBase::SECURE_STORAGE_KEY = "QRZCOM";
const QString QRZBase::SECURE_STORAGE_API_KEY = "QRZCOMAPI";
const QString QRZBase::CONFIG_USERNAME_API_CONST = "logbookapi";
const QString QRZCallbook::CALLBOOK_NAME = "qrzcom";

const QString QRZBase::getUsername()
{
    FCT_IDENTIFICATION;

    return LogParam::getQRZCOMCallbookUsername();
}

const QString QRZBase::getPassword()
{
    FCT_IDENTIFICATION;

    return CredentialStore::instance()->getPassword(QRZBase::SECURE_STORAGE_KEY,
                                                    getUsername());
}

void QRZBase::saveUsernamePassword(const QString &newUsername, const QString &newPassword)
{
    FCT_IDENTIFICATION;

    const QString &oldUsername = getUsername();
    if ( oldUsername != newUsername )
    {
        CredentialStore::instance()->deletePassword(QRZBase::SECURE_STORAGE_KEY,
                                                    oldUsername);
    }

    LogParam::setQRZCOMCallbookUsername(newUsername);

    CredentialStore::instance()->savePassword(QRZBase::SECURE_STORAGE_KEY,
                                              newUsername,
                                              newPassword);
}

const QString QRZBase::getLogbookAPIKey(const QString &internalUsername)
{
    FCT_IDENTIFICATION;

    return CredentialStore::instance()->getPassword(QRZBase::SECURE_STORAGE_API_KEY,
                                                    internalUsername);
}


void QRZBase::saveLogbookAPIKey(const QString &newKey, const QString &internalUsername)
{
    FCT_IDENTIFICATION;

    CredentialStore::instance()->deletePassword(QRZBase::SECURE_STORAGE_API_KEY,
                                                internalUsername);

    if ( ! newKey.isEmpty() )
    {
        CredentialStore::instance()->savePassword(QRZBase::SECURE_STORAGE_API_KEY,
                                                  internalUsername,
                                                  newKey);
    }
}

const QStringList QRZBase::getLogbookAPIAddlCallsigns()
{
    FCT_IDENTIFICATION;

    return LogParam::getQRZCOMAPICallsignsList();
}

void QRZBase::setLogbookAPIAddlCallsigns(const QStringList &list)
{
    FCT_IDENTIFICATION;

    LogParam::setQRZCOMAPICallsignsList(list);
}

QRZCallbook::QRZCallbook(QObject* parent) :
    GenericCallbook(parent),
    QRZBase(),
    incorrectLogin(false),
    lastSeenPassword(QString()),
    currentReply(nullptr)
{
    FCT_IDENTIFICATION;
}

QRZCallbook::~QRZCallbook()
{
    if ( currentReply )
    {
        currentReply->abort();
        currentReply->deleteLater();
    }
}

QString QRZCallbook::getDisplayName()
{
    FCT_IDENTIFICATION;

    return QString(tr("QRZ.com"));
}

void QRZCallbook::queryCallsign(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<< callsign;

    if (sessionId.isEmpty())
    {
        queuedCallsign = callsign;
        authenticate();
        return;
    }

    QUrlQuery params;
    params.addQueryItem("s", sessionId);

    const Callsign qCall(callsign);

    // currently QRZ.com does not handle correctly prefixes and suffixes.
    // That's why it's better to give it away if possible
    params.addQueryItem("callsign", (qCall.isValid()) ? qCall.getBase()
                                                      : callsign);

    QUrl url(API_URL);

    if ( currentReply )
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    qCDebug(runtime) << url;

    currentReply = getNetworkAccessManager()->post(request, params.query(QUrl::FullyEncoded).toUtf8());

    currentReply->setProperty("queryCallsign", QVariant(callsign));
    currentReply->setProperty("messageType", QVariant("callsignInfoQuery"));

    // Attention, variable callsign and queuedCallsign point to the same object
    // queuedCallsign must be cleared after the last use of the callsign variable
    queuedCallsign = QString();
}

void QRZCallbook::abortQuery()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        //currentReply->deleteLater(); // pointer is deleted later in processReply
        currentReply = nullptr;
    }
}

void QRZCallbook::authenticate()
{
    FCT_IDENTIFICATION;

    const QString &username = getUsername();
    const QString &password = getPassword();

    if ( incorrectLogin && password == lastSeenPassword)
    {
        /* User already knows that login failed */
        emit callsignNotFound(queuedCallsign);
        queuedCallsign = QString();
        return;
    }

    if ( !username.isEmpty() && !password.isEmpty() )
    {
        QUrlQuery params;
        params.addQueryItem("username", username.toUtf8().toPercentEncoding());
        params.addQueryItem("password", password.toUtf8().toPercentEncoding());
        params.addQueryItem("agent", "QLog");

        QUrl url(API_URL);

        if ( currentReply )
            qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        currentReply = getNetworkAccessManager()->post(request, params.query(QUrl::FullyEncoded).toUtf8());
        currentReply->setProperty("messageType", QVariant("authenticate"));
        lastSeenPassword = password;
    }
    else
    {
        emit callsignNotFound(queuedCallsign);
        qCDebug(runtime) << "Empty username or password";
    }
}

void QRZCallbook::processReply(QNetworkReply* reply)
{
    FCT_IDENTIFICATION;

    /* always process one requests per class */
    currentReply = nullptr;

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if ( reply->error() != QNetworkReply::NoError
         || replyStatusCode < 200
         || replyStatusCode >= 300)
    {
        qCDebug(runtime) << "QRZ.com error URL " << reply->request().url().toString();
        qCDebug(runtime) << "QRZ.com error" << reply->errorString();
        qCDebug(runtime) << "HTTP Status Code" << replyStatusCode;

        if ( reply->error() != QNetworkReply::OperationCanceledError )
        {
            emit lookupError(reply->errorString());
            reply->deleteLater();
        }
        return;
    }

    const QString &messageType = reply->property("messageType").toString();

    qCDebug(runtime) << "Received Message Type: " << messageType;

    /*********************/
    /* callsignInfoQuery */
    /*********************/
    if ( messageType == "callsignInfoQuery"
         || messageType == "authenticate" )
    {
        const QByteArray &response = reply->readAll();
        qCDebug(runtime) << response;
        QXmlStreamReader xml(response);
        CallbookResponseData resposeData;

        /* Reset Session Key */
        /* Every response contains a valid key. If the key is not present */
        /* then it is needed to request a new one */

        sessionId = QString();

        while ( !xml.atEnd() && !xml.hasError() )
        {
            QXmlStreamReader::TokenType token = xml.readNext();

            if (token != QXmlStreamReader::StartElement)
                continue;

            const QString elementName = xml.name().toString();

            if ( elementName == "Error" )
            {
                queuedCallsign = QString();
                QString errorString = xml.readElementText();

                if ( errorString.contains("Username/password incorrect"))
                {
                    incorrectLogin = true;
                    emit loginFailed();
                    emit lookupError(errorString);
                    return;
                }
                else if ( errorString.contains("Not found:") )
                {
                    incorrectLogin = false;
                    emit callsignNotFound(reply->property("queryCallsign").toString());
                    //return;
                }
                else
                {
                    qInfo() << "QRZ Error - " << errorString;
                    emit lookupError(errorString);
                }

                // do not call return here, we need to obtain Key from error message (if present)
            }
            else
                incorrectLogin = false;

            if      (elementName == "Key")       sessionId = xml.readElementText();
            else if (elementName == "call")      resposeData.call = xml.readElementText().toUpper();
            else if (elementName == "dxcc")      resposeData.dxcc = xml.readElementText();
            else if (elementName == "fname")     resposeData.fname = xml.readElementText();
            else if (elementName == "name")      resposeData.lname = xml.readElementText();
            else if (elementName == "addr1")     resposeData.addr1 = xml.readElementText();
            else if (elementName == "addr2")     resposeData.qth = xml.readElementText();
            else if (elementName == "state")     resposeData.us_state = xml.readElementText();
            else if (elementName == "zip")       resposeData.zipcode = xml.readElementText();
            else if (elementName == "country")   resposeData.country = xml.readElementText();
            else if (elementName == "lat")       resposeData.latitude = xml.readElementText();
            else if (elementName == "lon")       resposeData.longitude = xml.readElementText();
            else if (elementName == "county")    resposeData.county = xml.readElementText();
            else if (elementName == "grid")      resposeData.gridsquare = xml.readElementText().toUpper();
            else if (elementName == "efdate")    resposeData.lic_year = xml.readElementText();
            else if (elementName == "qslmgr")    resposeData.qsl_via = xml.readElementText();
            else if (elementName == "email")     resposeData.email = xml.readElementText();
            else if (elementName == "GMTOffset") resposeData.utc_offset = xml.readElementText();
            else if (elementName == "eqsl")      resposeData.eqsl = (xml.readElementText() == "1") ? "Y" : "N";
            else if (elementName == "mqsl")      resposeData.pqsl = xml.readElementText();
            else if (elementName == "cqzone")    resposeData.cqz = xml.readElementText();
            else if (elementName == "ituzone")   resposeData.ituz = xml.readElementText();
            else if (elementName == "born")      resposeData.born = xml.readElementText();
            else if (elementName == "lotw")      resposeData.lotw = (xml.readElementText() == "1") ? "Y" : "N";
            else if (elementName == "iota")      resposeData.iota = xml.readElementText();
            else if (elementName == "nickname")  resposeData.nick = xml.readElementText();
            else if (elementName == "url")       resposeData.url = xml.readElementText();
            else if (elementName == "name_fmt")  resposeData.name_fmt = xml.readElementText();
            else if (elementName == "image")     resposeData.image_url = xml.readElementText();
        }

        if (!resposeData.call.isEmpty())
            emit callsignResult(resposeData);

        if (!queuedCallsign.isEmpty())
            queryCallsign(queuedCallsign);
    }

    else

    reply->deleteLater();
}

/* https://www.qrz.com/docs/logbook/QRZLogbookAPI.html */
/* ??? QRZ Support all ADIF Fields ??? */

QRZUploader::QRZUploader(QObject *parent) :
    GenericQSOUploader(QStringList(), parent),
    QRZBase(),
    currentReply(nullptr),
    cancelUpload(false)
{
    FCT_IDENTIFICATION;
}

QRZUploader::~QRZUploader()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        currentReply->deleteLater();
    }
}

void QRZUploader::uploadContact(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    //qCDebug(function_parameters) << record;

    QByteArray data = generateADIF({record});
    cancelUpload = false;
    QString stationCallsign = record.value("station_callsign").toString();
    if ( stationCallsign.isEmpty() )
        stationCallsign = record.value("operator").toString();

    qCDebug(runtime) << "Using station Callsign" << stationCallsign;

    const QString &logbookAPIKey = (addlCallsign.contains(stationCallsign)) ? getLogbookAPIKey(stationCallsign)
                                                                            : getLogbookAPIKey();
    actionInsert(logbookAPIKey, data, "REPLACE");
    currentReply->setProperty("contactID", record.value("id"));
}

void QRZUploader::uploadQSOList(const QList<QSqlRecord>& qsos, const QVariantMap &)
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
    addlCallsign.clear();
    addlCallsign = QRZBase::getLogbookAPIAddlCallsigns();
    uploadContact(queuedContacts4Upload.first());
    queuedContacts4Upload.removeFirst();
}

void QRZUploader::actionInsert(const QString &logbookAPIKey, QByteArray& data, const QString &insertPolicy)
{
    FCT_IDENTIFICATION;

    QUrlQuery params;
    params.addQueryItem("KEY", logbookAPIKey);
    params.addQueryItem("ACTION", "INSERT");
    params.addQueryItem("OPTION", insertPolicy);
    params.addQueryItem("ADIF", data.trimmed().toPercentEncoding());

    QUrl url(API_LOGBOOK_URL);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QString rheader = QString("QLog/%1").arg(VERSION);
    request.setRawHeader("User-Agent", rheader.toUtf8());

    qCDebug(runtime) << Data::safeQueryString(params);

    if ( currentReply )
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";

    currentReply = getNetworkAccessManager()->post(request, params.query(QUrl::FullyEncoded).toUtf8());

    currentReply->setProperty("messageType", QVariant("actionsInsert"));
}

void QRZUploader::abortRequest()
{
    FCT_IDENTIFICATION;

    cancelUpload = true;
    if ( currentReply )
    {
        currentReply->abort();
        currentReply = nullptr;
    }
}

void QRZUploader::processReply(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    /* always process one requests per class */
    currentReply = nullptr;

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if ( reply->error() != QNetworkReply::NoError
         || replyStatusCode < 200
         || replyStatusCode >= 300)
    {
        qCDebug(runtime) << "QRZ.com error URL " << reply->request().url().toString();
        qCDebug(runtime) << "QRZ.com error" << reply->errorString();
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

    /*****************/
    /* actionsInsert */
    /*****************/
    if ( messageType == "actionsInsert")
    {
        const QString replayString(reply->readAll());
        qCDebug(runtime) << replayString;

        const QMap<QString, QString> &data = parseActionResponse(replayString);
        const QString &status = data.value("RESULT", "FAILED");

        if ( status == "OK" || status == "REPLACE" )
        {
            qCDebug(runtime) << "Confirmed Upload for QSO Id " << reply->property("contactID").toULongLong();
            emit uploadedQSO(reply->property("contactID").toULongLong());

            if ( queuedContacts4Upload.isEmpty() )
            {
                cancelUpload = false;
                emit uploadFinished();
            }
            else
            {
                if ( ! cancelUpload )
                {
                    uploadContact(queuedContacts4Upload.first());
                    queuedContacts4Upload.removeFirst();
                }
            }
        }
        else
        {
            emit uploadError(data.value("REASON", tr("General Error")));
            cancelUpload = false;
        }
    }
    reply->deleteLater();
}

QMap<QString, QString> QRZUploader::parseActionResponse(const QString &reponseString) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << reponseString;

    QMap<QString, QString> data;
    const QStringList &parsedResponse = reponseString.split("&");

    for ( const QString &param : parsedResponse )
    {
        QStringList parsedParams;
        parsedParams << param.split("=");

        if ( parsedParams.count() == 1 )
            data[parsedParams.at(0)] = QString();
        else if ( parsedParams.count() >= 2 )
            data[parsedParams.at(0)] = parsedParams.at(1);
    }

    return data;
}
