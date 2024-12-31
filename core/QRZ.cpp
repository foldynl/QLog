#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSettings>
#include <QtXml>
#include <QDebug>
#include "QRZ.h"
#include <QMessageBox>
#include "debug.h"
#include "core/CredentialStore.h"
#include "logformat/AdiFormat.h"
#include "core/Callsign.h"

#define API_URL "https://xmldata.qrz.com/xml/current/"
#define API_LOGBOOK_URL "https://logbook.qrz.com/api"

//https://www.qrz.com/docs/logbook/QRZLogbookAPI.html

MODULE_IDENTIFICATION("qlog.core.qrz");

QRZ::QRZ(QObject* parent) :
    GenericCallbook(parent),
    incorrectLogin(false),
    lastSeenPassword(QString()),
    cancelUpload(false),
    currentReply(nullptr)
{
    FCT_IDENTIFICATION;

    nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished,
            this, &QRZ::processReply);
}

QRZ::~QRZ()
{
    nam->deleteLater();

    if ( currentReply )
    {
        currentReply->abort();
        currentReply->deleteLater();
    }
}

void QRZ::queryCallsign(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<< callsign;

    if (sessionId.isEmpty()) {
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

    currentReply = nam->post(request, params.query(QUrl::FullyEncoded).toUtf8());

    currentReply->setProperty("queryCallsign", QVariant(callsign));
    currentReply->setProperty("messageType", QVariant("callsignInfoQuery"));

    // Attention, variable callsign and queuedCallsign point to the same object
    // queuedCallsign must be cleared after the last use of the callsign variable
    queuedCallsign = QString();
}

void QRZ::abortQuery()
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

void QRZ::uploadContact(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    //qCDebug(function_parameters) << record;

    QByteArray data;
    QTextStream stream(&data, QIODevice::ReadWrite);

    AdiFormat adi(stream);
    adi.exportContact(record);
    stream.flush();

    cancelUpload = false;
    actionInsert(data, "REPLACE");
    currentReply->setProperty("contactID", record.value("id"));
}

void QRZ::actionInsert(QByteArray& data, const QString &insertPolicy)
{
    FCT_IDENTIFICATION;

    const QString &logbookAPIKey = getLogbookAPIKey();

    QUrlQuery params;
    params.addQueryItem("KEY", logbookAPIKey);
    params.addQueryItem("ACTION", "INSERT");
    params.addQueryItem("OPTION", insertPolicy);
    params.addQueryItem("ADIF", data.trimmed().toPercentEncoding());

    QUrl url(API_LOGBOOK_URL);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QString rheader = QString("QLog/version %1 <OK1MLG>").arg(VERSION);

    request.setRawHeader("User-Agent",rheader.toUtf8() );
    qCDebug(runtime) << url;

    if ( currentReply )
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";

    currentReply = nam->post(request, params.query(QUrl::FullyEncoded).toUtf8());

    currentReply->setProperty("messageType", QVariant("actionsInsert"));
}


void QRZ::uploadContacts(const QList<QSqlRecord> &qsos)
{
    FCT_IDENTIFICATION;

    //qCDebug(function_parameters) << qsos;

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

const QString QRZ::getUsername()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    return settings.value(QRZ::CONFIG_USERNAME_KEY).toString().trimmed();
}

const QString QRZ::getPassword()
{
    FCT_IDENTIFICATION;

    return CredentialStore::instance()->getPassword(QRZ::SECURE_STORAGE_KEY,
                                                    getUsername());

}

const QString QRZ::getLogbookAPIKey()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    return CredentialStore::instance()->getPassword(QRZ::SECURE_STORAGE_API_KEY,
                                        settings.value(QRZ::CONFIG_USERNAME_API_KEY,
                                                       QRZ::CONFIG_USERNAME_API_CONST).toString());
}

void QRZ::saveUsernamePassword(const QString &newUsername, const QString &newPassword)
{
    FCT_IDENTIFICATION;

    QSettings settings;

    const QString &oldUsername = getUsername();
    if ( oldUsername != newUsername )
    {
        CredentialStore::instance()->deletePassword(QRZ::SECURE_STORAGE_KEY,
                                                    oldUsername);
    }

    settings.setValue(QRZ::CONFIG_USERNAME_KEY, newUsername);

    CredentialStore::instance()->savePassword(QRZ::SECURE_STORAGE_KEY,
                                              newUsername,
                                              newPassword);
}

void QRZ::saveLogbookAPI(const QString &newKey)
{
    FCT_IDENTIFICATION;

    QSettings settings;

    settings.setValue(QRZ::CONFIG_USERNAME_API_KEY, QRZ::CONFIG_USERNAME_API_CONST);

    CredentialStore::instance()->deletePassword(QRZ::SECURE_STORAGE_API_KEY,
                                                QRZ::CONFIG_USERNAME_API_CONST);

    if ( ! newKey.isEmpty() )
    {
        CredentialStore::instance()->savePassword(QRZ::SECURE_STORAGE_API_KEY,
                                                  QRZ::CONFIG_USERNAME_API_CONST,
                                                  newKey);
    }

}

QString QRZ::getDisplayName()
{
    FCT_IDENTIFICATION;

    return QString(tr("QRZ.com"));
}

void QRZ::authenticate()
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

        currentReply = nam->post(request, params.query(QUrl::FullyEncoded).toUtf8());
        currentReply->setProperty("messageType", QVariant("authenticate"));
        lastSeenPassword = password;
    }
    else
    {
        emit callsignNotFound(queuedCallsign);
        qCDebug(runtime) << "Empty username or password";
    }
}

void QRZ::processReply(QNetworkReply* reply)
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

        if ( reply->error() != QNetworkReply::OperationCanceledError )
        {
            emit uploadError(reply->errorString());
            emit lookupError(reply->errorString());
            reply->deleteLater();
        }

        cancelUpload = true;
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

        /* Reset Session Key */
        /* Every response contains a valid key. If the key is not present */
        /* then it is needed to request a new one */

        sessionId = QString();

        QMap<QString, QString> data;

        while ( !xml.atEnd() && !xml.hasError() )
        {
            QXmlStreamReader::TokenType token = xml.readNext();

            if (token != QXmlStreamReader::StartElement) {
                continue;
            }

            if (xml.name() == QString("Error") )
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
            {
                incorrectLogin = false;
            }

            if (xml.name() == QString("Key") )
            {
                sessionId = xml.readElementText();
            }
            else if (xml.name() == QString("call") )
            {
                data["call"] = xml.readElementText().toUpper();
            }
            else if (xml.name() == QString("dxcc") )
            {
                data["dxcc"] = xml.readElementText();
            }
            else if (xml.name() == QString("fname") )
            {
                data["fname"] = xml.readElementText();
            }
            else if (xml.name() == QString("name") )
            {
                data["lname"] = xml.readElementText();
            }
            else if (xml.name() == QString("addr1") )
            {
                data["addr1"] = xml.readElementText();
            }
            else if (xml.name() == QString("addr2") )
            {
                data["qth"] = xml.readElementText();
            }
            else if (xml.name() == QString("state") )
            {
                data["us_state"] = xml.readElementText();
            }
            else if (xml.name() == QString("zip") )
            {
                data["zipcode"] = xml.readElementText();
            }
            else if (xml.name() == QString("country") )
            {
                data["country"] = xml.readElementText();
            }
            else if (xml.name() == QString("lat") )
            {
                data["latitude"] = xml.readElementText();
            }
            else if (xml.name() == QString("lon") )
            {
                data["longitude"] = xml.readElementText();
            }
            else if (xml.name() == QString("county") )
            {
                data["county"] = xml.readElementText();
            }
            else if (xml.name() == QString("grid") )
            {
                data["gridsquare"] = xml.readElementText().toUpper();
            }
            else if (xml.name() == QString("efdate") )
            {
                data["lic_year"] = xml.readElementText();
            }
            else if (xml.name() == QString("qslmgr") )
            {
                data["qsl_via"] = xml.readElementText();
            }
            else if (xml.name() == QString("email") )
            {
                data["email"] = xml.readElementText();
            }
            else if (xml.name() == QString("GMTOffset") )
            {
                data["utc_offset"] = xml.readElementText();
            }
            else if (xml.name() == QString("eqsl") )
            {
                data["eqsl"] = ( xml.readElementText() == "1" ) ? "Y" : "N";
            }
            else if (xml.name() == QString("mqsl") )
            {
                data["pqsl"] = xml.readElementText();
            }
            else if (xml.name() == QString("cqzone") )
            {
                data["cqz"] = xml.readElementText();
            }
            else if (xml.name() == QString("ituzone") )
            {
                data["ituz"] = xml.readElementText();
            }
            else if (xml.name() == QString("born") )
            {
                data["born"] = xml.readElementText();
            }
            else if (xml.name() == QString("lotw") )
            {
                data["lotw"] =  ( xml.readElementText() == "1" ) ? "Y" : "N";
            }
            else if (xml.name() == QString("iota") )
            {
                data["iota"] = xml.readElementText();
            }
            else if (xml.name() == QString("nickname") )
            {
                data["nick"] = xml.readElementText();
            }
            else if (xml.name() == QString("url") )
            {
                data["url"] = xml.readElementText();
            }
            else if (xml.name() ==  QString("name_fmt"))
            {
                data["name_fmt"] = xml.readElementText();
            }
            else if (xml.name() == QString("image"))
            {
                data["image_url"] = xml.readElementText();
            }
        }

        if (data.size())
        {
            emit callsignResult(data);
        }

        if (!queuedCallsign.isEmpty())
        {
            queryCallsign(queuedCallsign);
        }
    }
    /*****************/
    /* actionsInsert */
    /*****************/
    else if ( messageType == "actionsInsert")
    {
         const QString replayString(reply->readAll());
         qCDebug(runtime) << replayString;

         const QMap<QString, QString> &data = parseActionResponse(replayString);

         const QString &status = data.value("RESULT", "FAILED");

         if ( status == "OK" || status == "REPLACE" )
         {
             qCDebug(runtime) << "Confirmed Upload for QSO Id " << reply->property("contactID").toInt();
             emit uploadedQSO(reply->property("contactID").toInt());

             if ( queuedContacts4Upload.isEmpty() )
             {
                 cancelUpload = false;
                 emit uploadFinished(true);
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

QMap<QString, QString> QRZ::parseActionResponse(const QString &reponseString) const
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
        {
            data[parsedParams.at(0)] = QString();
        }
        else if ( parsedParams.count() >= 2 )
        {
            data[parsedParams.at(0)] = parsedParams.at(1);
        }
    }

    return data;
}

const QString QRZ::SECURE_STORAGE_KEY = "QRZCOM";
const QString QRZ::SECURE_STORAGE_API_KEY = "QRZCOMAPI";
const QString QRZ::CONFIG_USERNAME_KEY = "qrzcom/username";
const QString QRZ::CONFIG_USERNAME_API_KEY = "qrzcom/usernameapi";
const QString QRZ::CONFIG_USERNAME_API_CONST = "logbookapi";
const QString QRZ::CALLBOOK_NAME = "qrzcom";
