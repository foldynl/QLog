#include <QRegularExpression>
#include <QTimer>
#include <QSqlQuery>
#include <QSqlError>
#ifdef Q_OS_WIN
#include <Ws2tcpip.h>
#include <winsock2.h>
#include <Mstcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#include "KSTChat.h"
#include "core/debug.h"
#include "data/Data.h"
#include "data/StationProfile.h"
#include "core/CredentialStore.h"
#include "core/LogParam.h"

MODULE_IDENTIFICATION("qlog.core.kstchat");

#define KST_HOSTNAME "www.on4kst.info"
#define KST_PORT 23000

// update user list every 3*60 seconds
#define KST_UPDATE_USERS_LIST 3*60

KSTChat::KSTChat(int chatRoomIndex,
                 const QString &username,
                 const QString &password,
                 const NewContactWidget *contact,
                 QObject *parent)
    : QObject{parent},
      chatRoomIdx(chatRoomIndex),
      userName(username),
      password(password),
      socket(nullptr),
      currCommand(NO_CMD),
      contact(contact)
{
    FCT_IDENTIFICATION;

#if 0 // Only for debug. It generate a testing message.
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        KSTChatMsg msg;
        msg.time = QDateTime::currentDateTimeUtc().toString("hhmm");
        msg.sender = "OK1MLF";
        msg.message = "Toto je pokusna zprava pro vsechny";
        msg.grid = Gridsquare("JN79HK");
        emit chatMsg(msg);
    });
    timer->start(5000);
#endif
}

KSTChat::~KSTChat()
{
    FCT_IDENTIFICATION;

    disconnectChat();
}

QList<KSTUsersInfo> KSTChat::getUsersList() const
{
    FCT_IDENTIFICATION;

    return userList;
}

KSTUsersInfo KSTChat::getUserInfo(const QString &username) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << username;

    for ( const KSTUsersInfo &info : static_cast<const QList<KSTUsersInfo>&>(userList) )
    {
        if ( info.callsign == username )
            return info;
    }
    return KSTUsersInfo();
}

const QString KSTChat::getUsername()
{
    FCT_IDENTIFICATION;

    return LogParam::getKSTChatUsername();
}

const QString KSTChat::getPassword()
{
    FCT_IDENTIFICATION;

    return CredentialStore::instance()->getPassword(KSTChat::SECURE_STORAGE_KEY,
                                                    getUsername());
}

void KSTChat::saveUsernamePassword(const QString &newUsername, const QString &newPassword)
{
    FCT_IDENTIFICATION;

    const QString &oldUsername = getUsername();

    if ( oldUsername != newUsername )
    {
        CredentialStore::instance()->deletePassword(KSTChat::SECURE_STORAGE_KEY,
                                                    oldUsername);
    }
    LogParam::setKSTChatUsername(newUsername);
    CredentialStore::instance()->savePassword(KSTChat::SECURE_STORAGE_KEY,
                                              newUsername,
                                              newPassword);
}

void KSTChat::connectChat()
{
    FCT_IDENTIFICATION;

    socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::readyRead, this, &KSTChat::receiveData);
    connect(socket, &QTcpSocket::connected, this, &KSTChat::socketConnected);
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &KSTChat::socketError);
#else
    connect(socket, &QTcpSocket::errorOccurred, this, &KSTChat::socketError);
#endif
    socket->connectToHost(KST_HOSTNAME, KST_PORT);
}

void KSTChat::disconnectChat()
{
    FCT_IDENTIFICATION;

    if ( socket )
    {
       socket->disconnect();
       socket->close();

       socket->deleteLater();
       socket = nullptr;
    }
    currCommand = NO_CMD;
    commandQueue.clear();
    receiveBuffer.clear();
    commandLineBuffer.clear();
    emit chatDisconnected();
}

void KSTChat::sendMessage(const QString &msg)
{
    FCT_IDENTIFICATION;

    if ( msg.length() == 0 )
        return;

    if ( msg.startsWith("/chat",Qt::CaseInsensitive) )
    {
        emit chatError("Changing chat is not supported");
        return;
    }

    sendCommand(( msg.startsWith("/") ) ? USER_CMD : NO_CMD, msg);
}

void KSTChat::reloadStationProfile()
{
    FCT_IDENTIFICATION;

    sendSetGridCommand();
}

void KSTChat::resetDupe()
{
    FCT_IDENTIFICATION;

    for ( KSTUsersInfo &user: userList )
        user.dupeCount = 0;
    emit usersListUpdated();
}

void KSTChat::recalculateDupe()
{
    FCT_IDENTIFICATION;

    if ( !contact )
        return;

    for ( KSTUsersInfo &user: userList )
        user.dupeCount = Data::countDupe(user.callsign, contact->getBand(), contact->getMode());

    emit usersListUpdated();
}

void KSTChat::recalculateDxccStatus()
{
    FCT_IDENTIFICATION;

    if ( !contact )
        return;

    const QString &currBand = contact->getBand();
    const QString &modeGroupString = BandPlan::modeToDXCCModeGroup(contact->getMode());

    for ( KSTUsersInfo &user: userList )
        user.status = Data::instance()->dxccStatus(user.dxcc.dxcc, currBand, modeGroupString);

    emit usersListUpdated();

}

void KSTChat::updateSpotsStatusWhenQSOAdded(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    if ( !contact )
        return;

    qint32 dxcc = record.value("dxcc").toInt();
    const QString &band = record.value("band").toString();
    const QString &dxccModeGroup = BandPlan::modeToDXCCModeGroup(record.value("mode").toString());
    const QString &callsign = record.value("callsign").toString();
    const QString &currBand = contact->getBand();
    const QString &modeGroupString = BandPlan::modeToDXCCModeGroup(contact->getMode());

    for ( KSTUsersInfo &user: userList )
    {
        user.status = Data::dxccNewStatusWhenQSOAdded(user.status,
                                                      user.dxcc.dxcc,
                                                      currBand,
                                                      modeGroupString,
                                                      dxcc,
                                                      currBand,
                                                      dxccModeGroup);
        if ( user.callsign == callsign )
            user.dupeCount = Data::dupeNewCountWhenQSOAdded(user.dupeCount,
                                                            currBand,
                                                            modeGroupString,
                                                            band,
                                                            dxccModeGroup);
    }
    emit usersListUpdated();
}

void KSTChat::updateSpotsStatusWhenQSODeleted(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    if ( !contact )
        return;

    const QString &band = record.value("band").toString();
    const QString &dxccModeGroup = BandPlan::modeToDXCCModeGroup(record.value("mode").toString());
    const QString &callsign = record.value("callsign").toString();
    const QString &currBand = contact->getBand();
    const QString &modeGroupString = BandPlan::modeToDXCCModeGroup(contact->getMode());

    for ( KSTUsersInfo &user: userList )
    {
        if ( user.dupeCount && user.callsign == callsign )
            user.dupeCount = Data::dupeNewCountWhenQSODelected(user.dupeCount,
                                                               currBand,
                                                               modeGroupString,
                                                               band,
                                                               dxccModeGroup);
    }

}

void KSTChat::updateSpotsDxccStatusWhenQSODeleted(const QSet<uint> &entities)
{
    FCT_IDENTIFICATION;

    // this method is called at the end of QSO Delete (after commit).

    if ( entities.isEmpty() || !contact)
        return;

    const QString &currBand = contact->getBand();
    const QString &modeGroupString = BandPlan::modeToDXCCModeGroup(contact->getMode());

    for ( KSTUsersInfo &user: userList )
    {
        if ( !entities.contains(user.dxcc.dxcc) )
            continue;

        user.status = Data::instance()->dxccStatus(user.dxcc.dxcc, currBand, modeGroupString);
    }
    emit usersListUpdated();
}

void KSTChat::sendShowUsersCommand()
{
    FCT_IDENTIFICATION;

    sendCommand(SHOW_USERS_CMD, "/sh us");
}

void KSTChat::sendCommand(const Command &cmd, const QString &msg)
{
    FCT_IDENTIFICATION;

    if ( currCommand != NO_CMD )
    {
        qCDebug(runtime) << "Storing: " << msg;
        commandQueue << QPair<Command, QString>(cmd, msg);
        return;
    }

    currCommand = cmd;

    if ( socket && socket->isOpen() )
    {
        QByteArray data;
        data.append(msg.toLatin1());
        data.append("\r\n");
        qCDebug(runtime) << "Sending: " << msg;
        socket->write(data);
    }
}

void KSTChat::sendSetGridCommand()
{
    FCT_IDENTIFICATION;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();
    sendCommand(SET_GRID_CMD, "/set qra " + profile.locator);
}

QStringList KSTChat::joinLines(const QByteArray &data)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << data;

    QByteArray fixedData(data);
    QStringList retList;

    fixedData.replace('\0', "");
    receiveBuffer.append(QString::fromUtf8(fixedData));
    int index = receiveBuffer.indexOf("\n");

    while ( index != -1 )
    {
        QString line = receiveBuffer.left(index + 1); //including "\n" char
        receiveBuffer = receiveBuffer.mid(index + 1);

        retList.append(line.trimmed());
        index = receiveBuffer.indexOf('\n');
    }
    return retList;
}

void KSTChat::receiveData()
{
    FCT_IDENTIFICATION;

    static QRegularExpression chatLineRE("([0-9]{4})Z (.*)>(.*)");
    QRegularExpressionMatch chatLineMatch;
    QString chatName(chatRooms.at(chatRoomIdx-1));
    QRegularExpression chatCMDEndRE("([0-9]{4})Z " + userName.toUpper() + " " + QRegularExpression::escape(chatName) + " chat>(.*)");
    QRegularExpressionMatch chatCMDEndMatch;

    const QStringList &lines = joinLines(socket->readAll());

    for (const QString &line : lines )
    {
        qCDebug(runtime) << "Processing Line" << line << "CMD" << currCommand;
        // Skip empty lines
        if ( line.length() == 0 )
        {
            continue;
        }
        else if ( line.startsWith("Login:") )
        {
            sendMessage(userName);
            return;
        }
        else if ( line.startsWith("Password:") )
        {
            sendMessage(password);
            return;
        }
        else if ( line.startsWith("Your choice           :") )
        {
            sendCommand(LOGIN_CMD, QString::number(chatRoomIdx));
            return;
        }
        else if ( line.startsWith("Unknown user") )
        {
            emit chatError(tr("Unknown User"));
            disconnectChat();
            return;
        }
        else if ( line.startsWith("Wrong password!") )
        {
            emit chatError(tr("Invalid password"));
            disconnectChat();
            return;
        }
        else
        {
            chatCMDEndMatch = chatCMDEndRE.match(line);

            if ( chatCMDEndMatch.hasMatch() )
            {
                qCDebug(runtime) << "CMD" << currCommand << " - End Detected";

                KSTChatMsg msg;
                msg.time = QDateTime::currentDateTimeUtc().toString("hhmm");
                msg.sender = QString();

                switch ( currCommand )
                {
                case LOGIN_CMD:
                    emit chatConnected();
                    msg.message = commandLineBuffer.join("\n");
                    emit chatMsg(msg);
                    sendSetGridCommand();
                    sendShowUsersCommand();
                    break;
                case SET_GRID_CMD:
                    msg.message = commandLineBuffer.join("\n");
                    emit chatMsg(msg);
                    break;
                case SHOW_USERS_CMD:
                    finalizeShowUsersCommand(commandLineBuffer);
                    break;
                case USER_CMD:
                    msg.message = commandLineBuffer.join("\n");
                    emit chatMsg(msg);
                    break;
                default:
                {}
                }
                currCommand = NO_CMD;
                commandLineBuffer.clear();
                if ( !commandQueue.isEmpty() )
                {
                    QPair<Command, QString> cmd = commandQueue.takeFirst();
                    sendCommand(cmd.first, cmd.second);
                }
            }
            else
            {
                chatLineMatch = chatLineRE.match(line);
                if ( chatLineMatch.hasMatch() )
                {
                    KSTChatMsg msg;
                    msg.time = QDateTime::currentDateTimeUtc().toString("hhmm");// own time to be better // chatLineMatch.captured(1);
                    msg.sender = chatLineMatch.captured(2);
                    msg.message = chatLineMatch.captured(3);
                    msg.grid = getUserInfo(msg.sender).grid;
                    emit chatMsg(msg);
                }
                else
                {
                    if ( currCommand != NO_CMD)
                        commandLineBuffer.append(line);
                }
            }
        }
    }
}

void KSTChat::socketConnected()
{
    FCT_IDENTIFICATION;

    if ( !socket )
    {
        qWarning() << "Socket is not opened";
        return;
    }

    int fd = socket->socketDescriptor();

#ifdef Q_OS_WIN
    DWORD  dwBytesRet = 0;

    struct tcp_keepalive   alive;    // your options for "keepalive" mode
    alive.onoff = TRUE;              // turn it on
    alive.keepalivetime = 10000;     // delay (ms) between requests, here is 10s, default is 2h (7200000)
    alive.keepaliveinterval = 5000;  // delay between "emergency" ping requests, their number (6) is not configurable
      /* So with this config  socket will send keepalive requests every 30 seconds after last data transaction when everything is ok.
          If there is no reply (wire plugged out) it'll send 6 requests with 5s delay  between them and then close.
          As a result we will get disconnect after approximately 1 min timeout.
       */
    if (WSAIoctl(fd, SIO_KEEPALIVE_VALS, &alive, sizeof(alive), NULL, 0, &dwBytesRet, NULL, NULL) == SOCKET_ERROR) {
           qWarning() << "WSAIotcl(SIO_KEEPALIVE_VALS) failed with err#" <<  WSAGetLastError();
    }
#else
    int enableKeepAlive = 1;
    int maxIdle = 10;
    int count = 3;
    int interval = 10;

    if ( setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enableKeepAlive, sizeof(enableKeepAlive)) !=0 )
    {
         qWarning() << "Cannot set keepalive for DXC";
    }
    else
    {
#ifndef Q_OS_MACOS
        if ( setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &maxIdle, sizeof(maxIdle)) != 0 )
#else
        if ( setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &maxIdle, sizeof(maxIdle)) != 0 )
#endif /* Q_OS_MACOS */
        {
            qWarning() << "Cannot set keepalive idle for DXC";
        }

        if ( setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) != 0 )
        {
            qWarning() << "Cannot set keepalive counter for DXC";
        }

        if ( setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) != 0 )
        {
            qWarning() << "Cannot set keepalive interval for DXC";
        }

        // TODO: setup TCP_USER_TIMEOUT????
    }
#endif
    receiveBuffer.clear();
    commandLineBuffer.clear();
}

void KSTChat::socketError(QAbstractSocket::SocketError socker_error)
{
    FCT_IDENTIFICATION;

    QString error_msg;

    qCDebug(runtime) << socker_error;

    switch (socker_error)
    {
    case QAbstractSocket::ConnectionRefusedError:
        error_msg.append(QObject::tr("Connection Refused"));
        break;
    case QAbstractSocket::RemoteHostClosedError:
        error_msg.append(QObject::tr("Host closed the connection"));
        disconnectChat();
        //reconectRequested = true;
        break;
    case QAbstractSocket::HostNotFoundError:
        error_msg.append(QObject::tr("Host not found"));
        break;
    case QAbstractSocket::SocketTimeoutError:
        error_msg.append(QObject::tr("Timeout"));
        disconnectChat();
        //reconectRequested = true;
        break;
    case QAbstractSocket::NetworkError:
        error_msg.append(QObject::tr("Network Error"));
        disconnectChat();
        break;
    default:
        error_msg.append(QObject::tr("Internal Error"));
        disconnectChat();
    }
   emit chatError(error_msg);
}

void KSTChat::finalizeShowUsersCommand(const QStringList &buffer)
{
    FCT_IDENTIFICATION;

    static QRegularExpression recordRE("^(\\S{3,})\\s{1,}(\\S+)\\s(.*)$");

    userList.clear();

    for ( const QString &record : static_cast<const QList<QString>&>(buffer) )
    {
        QRegularExpressionMatch match = recordRE.match(record);

        if ( match.hasMatch() )
        {
            KSTUsersInfo user;
            user.callsign = match.captured(1).remove('(').remove(')');
            user.grid = Gridsquare(match.captured(2));
            user.stationComment = match.captured(3);
            user.dxcc = Data::instance()->lookupDxcc(user.callsign);
            user.status = DxccStatus::UnknownStatus;
            if ( contact )
            {
                user.status = Data::instance()->dxccStatus(user.dxcc.dxcc, contact->getBand(), contact->getMode());
                user.dupeCount = Data::countDupe(user.callsign, contact->getBand(), contact->getMode());
            }
            userList << user;
        }
        else
        {
            qCDebug(runtime) << "Record does not match the pattern";
        }
    }
    emit usersListUpdated();
    QTimer::singleShot(1000 * KST_UPDATE_USERS_LIST, this, [this]()
    {
        qCDebug(runtime) << "Updating User List";
        sendShowUsersCommand();
    });
}

const QStringList KSTChat::chatRooms = {"50/70 MHz",
                                       "144/432 MHz",
                                       "Microwave",
                                       "EME/JT65",
                                       "Low Band (160-80m)",
                                       "50 MHz IARU Region 3",
                                       "50 MHz IARU Region 2",
                                       "144/432 MHz IARU R 2",
                                       "144/432 MHz IARU R 3",
                                       "kHz (2000-630m)",
                                       "Warc (30,17,12m)",
                                       "28 MHz",
                                       "40 MHz"};

const QString KSTChat::SECURE_STORAGE_KEY = "KST";

chatHighlightEvaluator::chatHighlightEvaluator(const int roomIndex,
                                               QObject *parent)
    : QObject(parent),
      roomIndex(roomIndex)
{
    FCT_IDENTIFICATION;
    loadRules();
}

void chatHighlightEvaluator::clearRules()
{
    FCT_IDENTIFICATION;

    qDeleteAll(ruleList);
    ruleList.clear();
}

QStringList chatHighlightEvaluator::getAllRuleNames()
{
    FCT_IDENTIFICATION;

    QStringList ret;

    QSqlQuery ruleStmt;
    if ( ! ruleStmt.prepare("SELECT rule_name FROM chat_highlight_rules ORDER BY rule_name") )
    {
        qWarning() << "Cannot prepare select statement";
    }
    else
    {
        if ( ruleStmt.exec() )
        {
            while (ruleStmt.next())
            {
                ret << ruleStmt.value(0).toString();
            }
        }
        else
        {
            qWarning()<< "Cannot get rule names from DB" << ruleStmt.lastError();;
        }
    }
    return ret;
}

void chatHighlightEvaluator::loadRules()
{
    FCT_IDENTIFICATION;

    if ( ruleList.size() > 0 )
    {
        clearRules();
    }

    QSqlQuery ruleStmt;

    if ( ! ruleStmt.prepare("SELECT rule_name FROM chat_highlight_rules "
                            "WHERE (room_id = :room_id or room_id = 0) AND enabled = 1") )
    {
        qWarning() << "Cannot prepare select statement";
    }
    else
    {
        ruleStmt.bindValue(":room_id", roomIndex);
        if ( ruleStmt.exec() )
        {
            while ( ruleStmt.next() )
            {
                chatHighlightRule *rule;
                rule = new chatHighlightRule();
                if ( rule )
                {
                    if ( rule->load(ruleStmt.value(0).toString()) )
                        ruleList.append(rule);
                    else
                        rule->deleteLater();
                }
            }
        }
        else
        {
            qWarning()<< "Cannot get rule names from DB" << ruleStmt.lastError();
        }
    }
}

bool chatHighlightEvaluator::shouldHighlight(const KSTChatMsg &msg,
                                             QStringList &matchedRules)
{
    FCT_IDENTIFICATION;

    for ( const chatHighlightRule *rule : static_cast<const QList<chatHighlightRule *>&>(ruleList) )
    {
        qCDebug(runtime) << "Processing " << rule->ruleName;
        if ( rule->match(roomIndex, msg) )
        {
            matchedRules << rule->ruleName;
        }
    }

    return ( matchedRules.size() > 0 );
}


chatHighlightRule::chatHighlightRule(QObject *parent) :
    QObject(parent),
    enabled(false),
    ruleRoomIndex(-1),
    interConditionOperand(OPERAND_OR),
    ruleValid(false)
{
    FCT_IDENTIFICATION;
}

bool chatHighlightRule::save()
{
    FCT_IDENTIFICATION;

    if ( ruleName.isEmpty() )
    {
        qCDebug(runtime) << "rule name is empty - do not save";
        return false;
    }

    QSqlQuery insertUpdateStmt;

    if ( ! insertUpdateStmt.prepare("INSERT INTO chat_highlight_rules(rule_name, room_id, enabled, rule_definition) "
                                    " VALUES (:ruleName, :room_id, :enabled, :rule_definition)  "
                                    " ON CONFLICT(rule_name) DO UPDATE SET rule_definition = :rule_definition, enabled = :enabled, room_id = :room_id "
                                    " WHERE rule_name = :ruleName"))
    {
        qWarning() << "Cannot prepare insert/update Alert Rule statement" << insertUpdateStmt.lastError();
        return false;
    }

    insertUpdateStmt.bindValue(":ruleName", ruleName);
    insertUpdateStmt.bindValue(":enabled", enabled);
    insertUpdateStmt.bindValue(":room_id", ruleRoomIndex);
    insertUpdateStmt.bindValue(":rule_definition", toJson());

    if ( ! insertUpdateStmt.exec() )
    {
        qCDebug(runtime)<< "Cannot Update Alert Rules - " << insertUpdateStmt.lastError().text();
        return false;
    }
    return true;
}

bool chatHighlightRule::load(const QString &ruleName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << ruleName;

    QSqlQuery query;

    if ( ! query.prepare("SELECT rule_definition "
                         "FROM chat_highlight_rules "
                         "WHERE rule_name = :rule_name") )
    {
        qWarning() << "Cannot prepare select statement";
        return false;
    }

    query.bindValue(":rule_name", ruleName);

    if ( query.exec()  && query.next() )
    {
        fromJson(QJsonDocument::fromJson(query.value(0).toByteArray()));
    }
    else
    {
        qCDebug(runtime) << "SQL execution error: " << query.lastError().text();
        return false;
    }
    return true;
}

QByteArray chatHighlightRule::toJson()
{
    FCT_IDENTIFICATION;

    QJsonObject ruleJsonObject;

    ruleJsonObject["rulename"] = ruleName;
    ruleJsonObject["enabled"] = enabled;
    ruleJsonObject["roomId"] = ruleRoomIndex;
    ruleJsonObject["operand"] = interConditionOperand;

    QJsonArray conditionsArray;

    for ( const Condition &condition : static_cast<const QList<Condition>&>(conditions) )
    {
        QJsonObject conditionObject;

        conditionObject["source"] = condition.source;
        conditionObject["operatorid"] = condition.operatorID;
        conditionObject["value"] = condition.value;
        conditionsArray.push_back(conditionObject);
    }
    ruleJsonObject["conditions"] = conditionsArray;

    QJsonDocument doc(ruleJsonObject);
    return doc.toJson();
}

void chatHighlightRule::fromJson(const QJsonDocument &ruleDefinition)
{
    FCT_IDENTIFICATION;

    if ( ruleDefinition.isNull() )
        return;
    /*
     * {
     *    rulename = "xxxx",
     *    enabled = true/false,
     *    roomId = x,
     *    operand = 1,
     *    conditions = [ {source = 1,
     *                    operatorID = 1,
     *                    value = "xxxx'},
     *                 ]
     *  }
     */
    ruleName = ruleDefinition["rulename"].toString();
    enabled = ruleDefinition["enabled"].toBool();
    ruleRoomIndex = ruleDefinition["roomId"].toInt();
    interConditionOperand = static_cast<InterConditionOperand>(ruleDefinition["operand"].toInt());
    const QJsonArray &conditionArray = ruleDefinition["conditions"].toArray();
    for ( const QJsonValue &value : conditionArray )
    {
        QJsonObject obj = value.toObject();
        Condition condition;
        condition.source = static_cast<InfoSource>(obj["source"].toInt());
        condition.operatorID = static_cast<Operator>(obj["operatorid"].toInt());
        condition.value = obj["value"].toString();
        conditions.append(condition);
    }
    ruleValid = true;
}

bool chatHighlightRule::match(const int inRoomIndex,
                              const KSTChatMsg &msg) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << inRoomIndex
                                 << ruleRoomIndex
                                 << msg.sender
                                 << msg.message
                                 << msg.grid.getGrid();

    if ( !ruleValid )
    {
        qCDebug(runtime) << "Rule is invalid";
        return false;
    }

    if ( !enabled )
    {
        qCDebug(runtime) << "Rule is disabled";
        return false;
    }

    if ( inRoomIndex != ruleRoomIndex
         && ruleRoomIndex != 0 )
    {
        qCDebug(runtime) << "Rule for different room ID";
        return false;
    }

    bool result = false;
    bool isFirstCondition = true;

    for ( const Condition &condition : static_cast<const QList<Condition>&>(conditions) )
    {
        QString columnValue;

        switch ( condition.source )
        {
        case SENDER:
            columnValue = msg.sender;
            qCDebug(runtime) << "Sender";
            break;
        case MESSAGE:
            columnValue = msg.message;
            qCDebug(runtime) << "Message";
            break;
        case GRIDSQUARE:
            columnValue = msg.grid.getGrid();
            qCDebug(runtime) << "Grid";
            break;
        }

        bool operatorResult = false;

        switch ( condition.operatorID )
        {
        case OPERATOR_CONTAINS:
            qCDebug(runtime) << "Contains" << condition.value;
            operatorResult = columnValue.contains(condition.value, Qt::CaseInsensitive);
            break;
        case OPERATOR_STARTWITH:
            qCDebug(runtime) << "StartWith" << condition.value;
            operatorResult = columnValue.startsWith(condition.value, Qt::CaseInsensitive);
            break;
        }

        if ( isFirstCondition )
        {
            result = operatorResult;
        }
        else
        {
            switch( interConditionOperand )
            {
            case OPERAND_OR:
                qCDebug(runtime) << "OR";
                result = result || operatorResult;
                break;
            case OPERAND_AND:
                qCDebug(runtime) << "AND";
                result = result && operatorResult;
            }
        }
        qCDebug(runtime) << "matching sub-result" << result;
        isFirstCondition = false;
    }

    qCDebug(runtime) << "matching result"<< result;
    return result;
}
