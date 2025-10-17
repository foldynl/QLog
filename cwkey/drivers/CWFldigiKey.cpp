#include <QNetworkReply>

#include "CWFldigiKey.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.cwkey.driver.cwfldigikey");

//response timeout in ms
#define RESPONSE_TIMEOUT 10000

CWFldigiKey::CWFldigiKey(const QString &hostname,
                         const quint16 port,
                         const CWKey::CWKeyModeID mode,
                         const qint32 defaultSpeed,
                         QObject *parent) :
    CWKey(mode, defaultSpeed, parent),
    isOpen(false),
    nam(new QNetworkAccessManager(this)),
    hostname(hostname),
    port(port),
    RXString("^r"),
    transmittingText(false)
{
    FCT_IDENTIFICATION;
}

bool CWFldigiKey::open()
{
    FCT_IDENTIFICATION;

    if ( isOpen )
    {
        return true;
    }

    QByteArray resp;

    // Check if QLog is connecting to FLDigi
    // And FLDigi contains all necessary commands
    if ( !sendXMLRPCCall("fldigi.list", resp) )

    {
        qCDebug(runtime) << "Connection error";
        return false;
    }

    if ( resp.contains("fldigi.list")
         && resp.contains("text.add_tx")
         && resp.contains("text.clear_tx")
         && resp.contains("main.tx") )
    {
        qCDebug(runtime) << "Connection check OK";
    }
    else
    {
        qCDebug(runtime) << "Connection checks failed";
        lastLogicalError = tr("Connected device is not FLDigi");
        return false;
    }

    if ( resp.contains("main.abort") )
    {
        qCDebug(runtime) << "Enabling stopSendingCap";
        stopSendingCap = true;
    }

    if ( resp.contains("tx.get_data") )
    {
        qCDebug(runtime) << "Enabling echoCharsCap";
        echoCharsCap = true;
    }

    printKeyCaps();

    isOpen = true;

    getEcho();

    return true;
}

bool CWFldigiKey::close()
{
    FCT_IDENTIFICATION;

    isOpen = false;
    lastLogicalError = QString();
    return true;
}

QString CWFldigiKey::lastError()
{
    FCT_IDENTIFICATION;

    return lastLogicalError;
}

bool CWFldigiKey::sendText(const QString &text)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << text;

    if ( text.isEmpty() )
        return true;

    if ( !isOpen )
    {
        qCWarning(runtime) << "Key is not connected";
        lastLogicalError = tr("Keyer is not connected");
        emit keyError(tr("Cannot send Text to FLDigi"), lastLogicalError);
        return false;
    }

    QByteArray resp;

    if ( !transmittingText )
    {
        if ( !sendXMLRPCCall("text.clear_tx", resp) )
        {
            emit keyError(tr("Cannot send the Clear command to FLDigi"), lastError());
            return false;
        }

        if ( !sendXMLRPCCall("main.tx", resp) )
        {
            emit keyError(tr("Cannot send the TX command to FLDigi"), lastError());
            return false;
        }
        transmittingText = true;
    }

    QList<QPair<QString, QString>> params;

    QString chpString(text);

    if ( chpString.contains("\n") )
    {
        chpString.replace("\n", RXString);
        transmittingText = false;
    }

    params << QPair<QString, QString>("string", chpString );

    if ( !sendXMLRPCCall("text.add_tx", resp, &params) )
    {
        transmittingText = false;
        emit keyError(tr("Cannot send the Text command to FLDigi"), lastError());
        return false;
    }

    return true;
}

bool CWFldigiKey::setWPM(const qint16)
{
    FCT_IDENTIFICATION;

    // currently I don't know which FLDigi function I should call.
    return true;
}

bool CWFldigiKey::immediatelyStop()
{
    FCT_IDENTIFICATION;

    if ( ! stopSendingCap )
    {
        qCDebug(runtime) << "STOP is not supported by FLDigi";
        return true;
    }

    if ( !isOpen )
    {
        qCWarning(runtime) << "Key is not connected";
        lastLogicalError = tr("Keyer is not connected");
        emit keyError(tr("Cannot send the Stop command to FLDigi"), lastLogicalError);
        return false;
    }

    QByteArray resp;

    if ( !sendXMLRPCCall("main.abort", resp) )
    {
        emit keyError(tr("Cannot send the Abort command to FLDigi"), lastError());
        return false;
    }

    if ( !sendXMLRPCCall("text.clear_tx", resp) )
    {
        emit keyError(tr("Cannot send the Clear command to FLDigi"), lastError());
        return false;
    }

    return true;
}

void CWFldigiKey::getEcho()
{
    FCT_IDENTIFICATION;

    if ( ! echoCharsCap )
    {
        qCDebug(runtime) << "Echo Char is not supported by FLDigi";
        return;
    }

    if ( !isOpen )
    {
        qCDebug(runtime) << "Key is not connected";
        return;
    }

    QByteArray resp;

    if ( !sendXMLRPCCall("tx.get_data", resp) )
    {
        qWarning() << "Cannot receive the TX Data from FLDigi";
        emit keyError(tr("Cannot receive data from FLDigi"), lastError());
        return;
    }

    qsizetype start = resp.indexOf("<base64>");
    qsizetype stop = resp.indexOf("</base64>");

    if ( start == -1 || stop == -1 ||
         stop < start )
    {
        qCDebug(runtime) << "based64 block not found";
        // TODO: EMIT ERROR and disconnect? currently without an error, I will see
        return;
    }

    QString echoString(QByteArray::fromBase64(resp.mid(start + 8, stop - start - 8)));
    qCDebug(runtime) << "\tEcho String" << echoString;
    emit keyEchoText(echoString);

    // check periodically
    QTimer::singleShot(1 * 1000, this, &CWFldigiKey::getEcho);
}

bool CWFldigiKey::sendXMLRPCCall(const QString & methodName,
                                 QByteArray &response,
                                 const QList<QPair<QString, QString>> *params)
{
    FCT_IDENTIFICATION;

    QEventLoop loop;
    QString ret;
    QXmlStreamWriter writer(&ret);
    QNetworkRequest request(QUrl(QString("http://%1:%2/RPC").arg(hostname, QString::number(port))));
    QNetworkReply* reply = nullptr;
    QTimer timer;

    timer.setSingleShot(true);

    response = QByteArray();
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/xml");

    writer.writeStartDocument();
    writer.writeStartElement("methodCall");
    writer.writeTextElement("methodName", methodName);

    if ( params && !params->isEmpty() )
    {
        writer.writeStartElement("params");

        for( const QPair<QString, QString>& param : static_cast<const QList<QPair<QString, QString>>&>(*params) )
        {
            writer.writeStartElement("param");
            writer.writeStartElement("value");
            writer.writeTextElement(param.first, param.second);
            writer.writeEndElement();
            writer.writeEndElement();
        }
        writer.writeEndElement();
    }
    writer.writeEndElement();

    writer.writeEndDocument();

    qCDebug(runtime) << ret.toLocal8Bit();

    reply= nam->post(request, ret.toLocal8Bit());

    // blocking call is used
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply , &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(RESPONSE_TIMEOUT);  // timer for logical timeout - in ms
    loop.exec();

    // Timeout occurred
    if (!timer.isActive())
    {
        disconnect(reply , &QNetworkReply::finished, &loop, &QEventLoop::quit);
        qWarning() << "XMLRPC Call Timeout" << RESPONSE_TIMEOUT << "ms";
        lastLogicalError = tr("FLDigi connection timeout");
        reply->abort();
        reply->deleteLater();
        return false;
    }

    timer.stop();

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if ( reply->error() != QNetworkReply::NoError
         || replyStatusCode < 200
         || replyStatusCode >= 300 )
    {
        qWarning() << "XMLRPC Call Reply Error" << reply->errorString();
        qCDebug(runtime) << "HTTP Status Code" << replyStatusCode;
        lastLogicalError = tr("FLDigi connection error");
        reply->deleteLater();
        return false;
    }

    response = reply->readAll();

    qCDebug(runtime) << response;

    if ( response.contains("<fault>") )
    {
        qWarning() << "XMLRPC Call Logical Error" << response;
        lastLogicalError = tr("FLDigi command error");
        reply->deleteLater();
        return false;
    }

    reply->deleteLater();    

    return true;
}
