#include <cstring>

#include <QAbstractSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>

#include "RBNNetwork.h"
#include "core/debug.h"
#include "data/BandPlan.h"
#include "data/Data.h"
#include "data/Gridsquare.h"

MODULE_IDENTIFICATION("qlog.service.rbn.rbnnetwork");

const char RBNNetwork::RbnHost[] = "telnet.reversebeacon.net";
const char RBNNetwork::RbnNodeListUrl[] = "https://www.reversebeacon.net/cont_includes/status.php?t=skt";
const char RBNNetwork::CallsignPrompt[] = "Please enter your call:";

RBNNetwork::RBNNetwork(QObject *parent)
    : QObject(parent),
      socket(new QTcpSocket(this)),
      reconnectTimer(new QTimer(this)),
      connectionTimer(new QTimer(this)),
      nodeListRefreshTimer(new QTimer(this)),
      networkManager(new QNetworkAccessManager(this)),
      state(State::Disconnected),
      reconnectDelayMs(InitialReconnectDelayMs),
      nodeListRequestPending(false)
{
    FCT_IDENTIFICATION;

    reconnectTimer->setSingleShot(true);
    connectionTimer->setSingleShot(true);
    nodeListRefreshTimer->setInterval(NodeListRefreshIntervalMs);

    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    connect(reconnectTimer, &QTimer::timeout, this, &RBNNetwork::connectToServer);
    connect(connectionTimer, &QTimer::timeout, this, &RBNNetwork::connectionTimedOut);
    connect(nodeListRefreshTimer, &QTimer::timeout, this, &RBNNetwork::refreshNodeList);
    connect(socket, &QTcpSocket::connected, this, &RBNNetwork::socketConnected);
    connect(socket, &QTcpSocket::disconnected, this, &RBNNetwork::socketDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &RBNNetwork::socketReadyRead);
    connect(networkManager, &QNetworkAccessManager::finished, this, &RBNNetwork::nodeListReceived);

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &RBNNetwork::socketError);
#else
    connect(socket, &QTcpSocket::errorOccurred, this, &RBNNetwork::socketError);
#endif    
}

void RBNNetwork::setCallsign(const QString &newCallsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << newCallsign;

    const QString normalizedCallsign = newCallsign.trimmed().toUpper();

    if ( callsign == normalizedCallsign )
        return;

    callsign = normalizedCallsign;
    callsignBytes = callsign.toLatin1();
    stopConnection();
    updateConnection();
}

void RBNNetwork::setCurrentMode(const QString &mode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode;

    const QString normalizedMode = mode.trimmed().toUpper();

    if ( currentMode == normalizedMode )
        return;

    const bool wasEnabled = shouldBeConnected();
    currentMode = normalizedMode;
    const bool isEnabled = shouldBeConnected();

    /* CW and RTTY share port 7000. Switching between them therefore only
     * changes the local spot filter and must not restart the TCP connection. */
    if ( wasEnabled != isEnabled )
        updateConnection();
}

bool RBNNetwork::shouldBeConnected() const
{
    FCT_IDENTIFICATION;

    return !callsign.isEmpty()
           && (currentMode == BandPlan::MODE_GROUP_STRING_CW
               || currentMode == QStringLiteral("RTTY"));
}

void RBNNetwork::updateConnection()
{
    FCT_IDENTIFICATION;

    if ( !shouldBeConnected() )
    {
        stopConnection();
        return;
    }

    if ( !nodeListRefreshTimer->isActive() )
        nodeListRefreshTimer->start();

    if ( nodeLocators.isEmpty() )
        refreshNodeList();

    if ( socket->state() == QAbstractSocket::UnconnectedState )
    {
        reconnectDelayMs = InitialReconnectDelayMs;
        reconnectTimer->start(0);
    }
}

void RBNNetwork::stopConnection()
{
    FCT_IDENTIFICATION;

    reconnectTimer->stop();
    connectionTimer->stop();
    nodeListRefreshTimer->stop();
    receiveBuffer.clear();
    state = State::Disconnected;
    reconnectDelayMs = InitialReconnectDelayMs;

    if ( socket->state() != QAbstractSocket::UnconnectedState )
    {
        qCDebug(runtime) << "Disconnecting from RBN";
        socket->abort();
    }
}

void RBNNetwork::connectToServer()
{
    FCT_IDENTIFICATION;

    if ( !shouldBeConnected()
         || socket->state() != QAbstractSocket::UnconnectedState )
        return;

    receiveBuffer.clear();
    state = State::Connecting;

    qCDebug(runtime) << "Connecting to RBN" << RbnHost << RbnPort;

    socket->connectToHost(QString::fromLatin1(RbnHost), RbnPort);
    connectionTimer->start(ConnectionTimeoutMs);
}

void RBNNetwork::socketConnected()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "RBN TCP connection established";
    state = State::AwaitingCallsignPrompt;
}

void RBNNetwork::socketDisconnected()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "RBN TCP connection disconnected";
    scheduleReconnect();
}

void RBNNetwork::socketError()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "RBN socket error:" << socket->errorString();

    if ( socket->state() != QAbstractSocket::UnconnectedState )
        socket->abort();

    scheduleReconnect();
}

void RBNNetwork::connectionTimedOut()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "RBN connection timed out";

    socket->abort();
    scheduleReconnect();
}

void RBNNetwork::scheduleReconnect()
{
    FCT_IDENTIFICATION;

    connectionTimer->stop();
    receiveBuffer.clear();
    state = State::Disconnected;

    if ( !shouldBeConnected() || reconnectTimer->isActive() )
        return;

    qCDebug(runtime) << "Reconnecting to RBN in" << reconnectDelayMs << "ms";

    reconnectTimer->start(reconnectDelayMs);
    reconnectDelayMs = qMin(reconnectDelayMs * ReconnectBackoffMultiplier,
                            static_cast<int>(MaximumReconnectDelayMs));
}

void RBNNetwork::socketReadyRead()
{
    FCT_IDENTIFICATION;

    const QByteArray data = socket->readAll();

    qCDebug(function_parameters) << data.size();

    receiveBuffer.append(data);

    if ( state == State::AwaitingCallsignPrompt
         && receiveBuffer.contains(CallsignPrompt) )
    {
        qCDebug(runtime) << "Sending RBN login callsign" << callsign;
        socket->write(callsignBytes + QByteArrayLiteral("\r\n"));
        state = State::AwaitingLoginConfirmation;
    }

    /* RBN can carry a very high global spot rate during contests. Scan the
     * QByteArray in place, remove consumed data only once, and reject other
     * callsigns before QString conversion, regular expressions or DB work. */
    int lineStart = 0;
    int lineEnd = -1;

    while ( (lineEnd = receiveBuffer.indexOf('\n', lineStart)) >= 0 )
    {
        const int lineLength = lineEnd - lineStart;
        const char *line = receiveBuffer.constData() + lineStart;

        //qCDebug(runtime) << line;

        if ( state == State::AwaitingLoginConfirmation )
            processLoginLine(line, lineLength);
        else if ( state == State::Ready
                  && lineLength <= MaximumLineLength
                  && lineMatchesCallsign(line, lineLength) )
            processMatchingSpot(line, lineLength);

        lineStart = lineEnd + 1;
    }

    if ( lineStart > 0 )
        receiveBuffer.remove(0, lineStart);

    if ( receiveBuffer.size() > MaximumLineLength )
    {
        qCDebug(runtime) << "Discarding an oversized incomplete RBN line";
        receiveBuffer.clear();
    }
}

bool RBNNetwork::lineMatchesCallsign(const char *line, int length) const
{
    /* This is the hot path for every RBN spot. Keep it allocation-free and do
     * not add FCT_IDENTIFICATION or per-line logging here. Expected format:
     *
     *   DX de SKIMMER-#: frequency SPOTTED-CALL mode ...
     */
    static const char prefix[] = "DX de ";
    const int prefixLength = static_cast<int>(sizeof(prefix) - 1);

    if ( !line
         || length <= prefixLength
         || std::memcmp(line, prefix, static_cast<size_t>(prefixLength)) != 0 )
        return false;

    int index = prefixLength;

    while ( index < length && line[index] != ':' )
        ++index;

    if ( index == length )
        return false;

    ++index;

    while ( index < length && (line[index] == ' ' || line[index] == '\t') )
        ++index;

    while ( index < length && line[index] != ' ' && line[index] != '\t' )
        ++index;

    while ( index < length && (line[index] == ' ' || line[index] == '\t') )
        ++index;

    const int spottedCallStart = index;

    while ( index < length
            && line[index] != ' '
            && line[index] != '\t'
            && line[index] != '\r' )
        ++index;

    const int spottedCallLength = index - spottedCallStart;
    return spottedCallLength == callsignBytes.size()
           && std::memcmp(line + spottedCallStart,
                          callsignBytes.constData(),
                          static_cast<size_t>(spottedCallLength)) == 0;
}

void RBNNetwork::processLoginLine(const char *line, int length)
{
    FCT_IDENTIFICATION;

    const QByteArray loginLine(line, length);
    if ( !loginLine.contains("Connected.") )
        return;

    connectionTimer->stop();
    reconnectDelayMs = InitialReconnectDelayMs;
    state = State::Ready;
    qCDebug(runtime) << "RBN login completed";
}

void RBNNetwork::processMatchingSpot(const char *line, int length)
{
    FCT_IDENTIFICATION;

    /* RBN line after the fast callsign filter:
     *
     *   DX de DK9IP-1-#: 14036.10 SP7RON CW 20 dB 22 WPM CQ 1551Z
     *
     * The text after the spotted callsign differs by mode. Parse only the
     * stable outer fields here and extract the dB report separately. */
    static const QRegularExpression spotExpression(
        QStringLiteral("^DX de\\s+([^:]+):\\s+([0-9.]+)\\s+\\S+\\s+(\\S+)\\s+(.*?)\\s+\\d{4}Z\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reportExpression(
        QStringLiteral("(-?\\d+)\\s*dB"),
        QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch match = spotExpression.match(
        QString::fromLatin1(line, length));

    if ( !match.hasMatch() )
    {
        qCDebug(runtime) << "Ignoring malformed matching RBN spot";
        return;
    }

    const QString mode = match.captured(3).toUpper();

    if ( mode != currentMode )
        return;

    bool frequencyValid = false;
    const double frequency = match.captured(2).toDouble(&frequencyValid) / 1000.0;

    if ( !frequencyValid )
        return;

    const Band band = BandPlan::freq2Band(frequency);

    if ( band.name.isEmpty() )
        return;

    QString nodeCallsign = match.captured(1).trimmed().toUpper();

    if ( nodeCallsign.endsWith(QStringLiteral("-#")) )
        nodeCallsign.chop(2);

    const QString locator = nodeLocators.value(nodeCallsign);

    if ( locator.isEmpty() )
    {
        qCDebug(runtime) << "No RBN locator for" << nodeCallsign;
        requestNodeListForUnknownNode();
        return;
    }

    const QRegularExpressionMatch reportMatch = reportExpression.match(match.captured(4));

    if ( !reportMatch.hasMatch() )
        return;

    const QString lookupCallsign = stationCallsign(nodeCallsign);
    const QString modeGroup = (mode == BandPlan::MODE_GROUP_STRING_CW)
                              ? BandPlan::MODE_GROUP_STRING_CW
                              : BandPlan::MODE_GROUP_STRING_DIGITAL;
    const DxccEntity dxcc = Data::instance()->lookupDxcc(lookupCallsign);

    HeardMeSpot spot;
    spot.callsign = nodeCallsign;
    spot.locator = locator;
    spot.frequency = frequency;
    spot.report = reportMatch.captured(1).toInt();
    spot.status = dxcc.dxcc
                  ? Data::instance()->currentDxccStatus(dxcc.dxcc, band.name, modeGroup)
                  : DxccStatus::UnknownStatus;
    spot.dupeCount = Data::countDupe(lookupCallsign, band.name, modeGroup);
    spot.displayGroup = (mode == BandPlan::MODE_GROUP_STRING_CW)
                        ? HeardMeSpot::DisplayGroup::CW
                        : HeardMeSpot::DisplayGroup::RTTY;

    qCDebug(runtime) << "RBN heard-me spot"
                     << spot.callsign << spot.locator << spot.frequency
                     << mode << spot.report;
    emit heardMeSpotReceived(spot);
}

void RBNNetwork::refreshNodeList()
{
    FCT_IDENTIFICATION;

    if ( nodeListRequestPending )
        return;

    nodeListRequestPending = true;

    lastNodeListRequest = QDateTime::currentDateTimeUtc();

    const QUrl url(QString::fromLatin1(RbnNodeListUrl));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QLog RBNNetwork"));

    qCDebug(runtime) << "Downloading RBN node locators" << url;
    networkManager->get(request);
}

void RBNNetwork::nodeListReceived(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    nodeListRequestPending = false;

    if ( !reply )
        return;

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if ( reply->error() != QNetworkReply::NoError
         || httpStatus < 200
         || httpStatus >= 300 )
    {
        qCDebug(runtime) << "RBN node locator download failed:"
                         << reply->errorString() << "HTTP" << httpStatus;
        reply->deleteLater();
        return;
    }

    const QString html = QString::fromUtf8(reply->readAll());
    reply->deleteLater();

    static const QRegularExpression rowExpression(
        QStringLiteral("<tr[^>]*>(.*?)</tr>"),
        QRegularExpression::DotMatchesEverythingOption
        | QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression cellExpression(
        QStringLiteral("<td[^>]*>(.*?)</td>"),
        QRegularExpression::DotMatchesEverythingOption
        | QRegularExpression::CaseInsensitiveOption);

    QHash<QString, QString> newLocators;
    QRegularExpressionMatchIterator rows = rowExpression.globalMatch(html);

    while ( rows.hasNext() )
    {
        const QString row = rows.next().captured(1);
        QRegularExpressionMatchIterator cells = cellExpression.globalMatch(row);
        QStringList values;

        while ( cells.hasNext() && values.size() < 3 )
            values.append(htmlText(cells.next().captured(1)));

        if ( values.size() < 3 )
            continue;

        const QString nodeCallsign = values.at(0).toUpper();
        const QString locator = values.at(2).toUpper();

        if ( !nodeCallsign.isEmpty() && Gridsquare(locator).isValid() )
            newLocators.insert(nodeCallsign, locator);
    }

    if ( newLocators.isEmpty() )
    {
        qCDebug(runtime) << "RBN node list contained no valid locators";
        return;
    }

    nodeLocators.swap(newLocators);
    qCDebug(runtime) << "Loaded RBN node locators" << nodeLocators.size();
}

void RBNNetwork::requestNodeListForUnknownNode()
{
    FCT_IDENTIFICATION;

    if ( nodeListRequestPending )
        return;

    const QDateTime now = QDateTime::currentDateTimeUtc();

    if ( !lastNodeListRequest.isValid()
         || lastNodeListRequest.secsTo(now) >= UnknownNodeRefreshIntervalSeconds )
        refreshNodeList();
}

QString RBNNetwork::htmlText(QString value)
{
    /* Called for thousands of table cells during one node-list refresh. Keep
     * this parser helper free of per-cell diagnostic setup and logging. */
    static const QRegularExpression tagExpression(QStringLiteral("<[^>]*>"));
    value.remove(tagExpression);
    value.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    return value.simplified();
}

QString RBNNetwork::stationCallsign(const QString &nodeCallsign)
{
    FCT_IDENTIFICATION;

    const int separator = nodeCallsign.lastIndexOf(QLatin1Char('-'));
    if ( separator < 0 )
        return nodeCallsign;

    bool numericSuffix = false;
    nodeCallsign.mid(separator + 1).toUInt(&numericSuffix);
    return numericSuffix ? nodeCallsign.left(separator) : nodeCallsign;
}
