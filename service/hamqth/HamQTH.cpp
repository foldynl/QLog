#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtXml>
#include <QDebug>
#include "HamQTH.h"
#include "core/debug.h"
#include "core/CredentialStore.h"
#include "core/LogParam.h"

MODULE_IDENTIFICATION("qlog.core.hamqth");

const QString HamQTHBase::SECURE_STORAGE_KEY = "HamQTH";
const QString HamQTHCallbook::CALLBOOK_NAME = "hamqth";

const QString HamQTHBase::getUsername()
{
    FCT_IDENTIFICATION;

    return LogParam::getHamQTHCallbookUsername();
}

const QString HamQTHBase::getPassword()
{
    FCT_IDENTIFICATION;

    return CredentialStore::instance()->getPassword(HamQTHBase::SECURE_STORAGE_KEY,
                                                   getUsername());
}

void HamQTHBase::saveUsernamePassword(const QString &newUsername, const QString &newPassword)
{
    FCT_IDENTIFICATION;

    QString oldUsername = getUsername();
    if ( oldUsername != newUsername )
    {
        CredentialStore::instance()->deletePassword(HamQTHBase::SECURE_STORAGE_KEY,
                                                    oldUsername);
    }

    LogParam::setHamQTHCallbookUsername(newUsername);

    CredentialStore::instance()->savePassword(HamQTHBase::SECURE_STORAGE_KEY,
                                              newUsername,
                                              newPassword);
}

HamQTHCallbook::HamQTHCallbook(QObject* parent) :
    GenericCallbook(parent),
    HamQTHBase(),
    currentReply(nullptr)
{
    FCT_IDENTIFICATION;

    incorrectLogin = false;
}

HamQTHCallbook::~HamQTHCallbook()
{
    if ( currentReply )
    {
        currentReply->abort();
        currentReply->deleteLater();
    }
}

QString HamQTHCallbook::getDisplayName()
{
    FCT_IDENTIFICATION;
    return QString(tr("HamQTH"));
}

void HamQTHCallbook::queryCallsign(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<< callsign;

    if (sessionId.isEmpty())
    {
        queuedCallsign = callsign;
        authenticate();
        return;
    }

    QUrlQuery query;
    query.addQueryItem("id", sessionId);
    query.addQueryItem("callsign", callsign);
    query.addQueryItem("prg", "QLog");

    QUrl url(API_URL);
    url.setQuery(query);

    if ( currentReply )
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";

    currentReply = getNetworkAccessManager()->get(QNetworkRequest(url));
    currentReply->setProperty("queryCallsign", callsign);

    // Attention, variable callsign and queuedCallsign point to the same object
    // queuedCallsign must be cleared after the last use of the callsign variable
    queuedCallsign = QString();
}

void HamQTHCallbook::abortQuery()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        //currentReply->deleteLater(); // pointer is deleted later in processReply
        currentReply = nullptr;
    }
}

void HamQTHCallbook::authenticate()
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

    if (!username.isEmpty() && !password.isEmpty())
    {
        QUrlQuery query;
        query.addQueryItem("u", username.toUtf8().toPercentEncoding());
        query.addQueryItem("p", password.toUtf8().toPercentEncoding());

        QUrl url(API_URL);
        url.setQuery(query);

        if ( currentReply )
            qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";

        currentReply = getNetworkAccessManager()->get(QNetworkRequest(url));
        lastSeenPassword = password;
        qCDebug(runtime) << "Sent Auth message";
    }
    else
    {
        emit callsignNotFound(queuedCallsign);
        qCDebug(runtime) << "Empty username or password";
    }
}

void HamQTHCallbook::processReply(QNetworkReply* reply)
{
    FCT_IDENTIFICATION;

    /* always process one requests per class */
    currentReply = nullptr;

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if ( reply->error() != QNetworkReply::NoError
         || replyStatusCode < 200
         || replyStatusCode >= 300)
    {
        qCDebug(runtime) << "HamQTH error" << reply->errorString();
        qCDebug(runtime) << "HTTP Status Code" << replyStatusCode;
        emit lookupError(reply->errorString());
        reply->deleteLater();
        return;
    }

    const QByteArray &response = reply->readAll();
    qCDebug(runtime) << response;
    QXmlStreamReader xml(response);
    CallbookResponseData resposeData;

    while (!xml.atEnd() && !xml.hasError())
    {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token != QXmlStreamReader::StartElement)
            continue;

        const QString &elementName = xml.name().toString();

        if ( elementName == "error" )
        {
            queuedCallsign = QString();
            QString errorString = xml.readElementText();

            if ( errorString == "Wrong user name or password" )
            {
                incorrectLogin = true;
                emit loginFailed();
            }
            else if ( errorString == "Callsign not found" )
            {
                incorrectLogin = false;
                emit callsignNotFound(reply->property("queryCallsign").toString());
                return;
            }
            else
                qWarning() << "HamQTH Error - " << errorString;

            sessionId = QString();
            emit lookupError(errorString);
            return;
        }
        else
            incorrectLogin = false;

        if      (elementName == "session_id")  sessionId = xml.readElementText();
        else if (elementName == "callsign")    resposeData.call = decodeHtmlEntities(xml.readElementText().toUpper());
        else if (elementName == "nick")        resposeData.nick = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "qth")         resposeData.qth = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "grid")        resposeData.gridsquare = decodeHtmlEntities(xml.readElementText().toUpper());
        else if (elementName == "qsl_via")     resposeData.qsl_via = decodeHtmlEntities(xml.readElementText().toUpper());
        else if (elementName == "cq")          resposeData.cqz = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "itu")         resposeData.ituz = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "dok")         resposeData.dok = decodeHtmlEntities(xml.readElementText().toUpper());
        else if (elementName == "iota")        resposeData.iota = decodeHtmlEntities(xml.readElementText().toUpper());
        else if (elementName == "email")       resposeData.email = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "adif")        resposeData.dxcc = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "adr_name")    resposeData.name_fmt = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "adr_street1") resposeData.addr1 = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "us_state")    resposeData.us_state = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "adr_zip")     resposeData.zipcode = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "country")     resposeData.country = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "latitude")    resposeData.latitude = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "longitude")   resposeData.longitude = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "county")      resposeData.county = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "lic_year")    resposeData.lic_year = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "utc_offset")  resposeData.utc_offset = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "eqsl")        resposeData.eqsl = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "qsl")         resposeData.pqsl = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "birth_year")  resposeData.born = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "lotw")        resposeData.lotw = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "web")         resposeData.url = decodeHtmlEntities(xml.readElementText());
        else if (elementName == "picture")     resposeData.image_url = decodeHtmlEntities(xml.readElementText());

        // HamQTH sends "http" URLs, which are redirected automatically
        // to their "https" variants. It's pointless to implement redirection
        // so let's replace http with https
        if ( !resposeData.image_url.contains("https")) resposeData.image_url.replace("http", "https");
    }

    reply->deleteLater();

    if ( !resposeData.call.isEmpty() )
        emit callsignResult(resposeData);

    if (!queuedCallsign.isEmpty())
        queryCallsign(queuedCallsign);
}
