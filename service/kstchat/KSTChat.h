#ifndef QLOG_SERVICE_KSTCHAT_KSTCHAT_H
#define QLOG_SERVICE_KSTCHAT_KSTCHAT_H

#include <QObject>
#include <QTcpSocket>

#include "core/Gridsquare.h"
#include "data/Dxcc.h"
#include "ui/NewContactWidget.h"

struct KSTChatMsg
{
    QString time;
    QString sender;
    QString message;
    Gridsquare grid;
    QStringList matchedHighlightRules;
};

Q_DECLARE_METATYPE(KSTChatMsg);

struct KSTUsersInfo
{
    QString callsign; // do not use Callsign class because KST users a free text here
    Gridsquare grid;
    QString stationComment;
    DxccEntity dxcc;
    DxccStatus status;
    qulonglong dupeCount = 0;
};

Q_DECLARE_METATYPE(KSTUsersInfo);


class chatHighlightRule : public QObject
{
    Q_OBJECT

public:
    enum InfoSource {
        SENDER = 0,
        MESSAGE = 1,
        GRIDSQUARE = 2
    };

    enum Operator {
        OPERATOR_CONTAINS = 0,
        OPERATOR_STARTWITH = 1
    };

    enum InterConditionOperand{
        OPERAND_AND = 0,
        OPERAND_OR = 1
    };

    struct Condition
    {
        InfoSource source;
        Operator operatorID;
        QString value;
    };

public:
    explicit chatHighlightRule(QObject *parent = nullptr);
    ~chatHighlightRule(){};

    bool save();
    bool load(const QString &);
    bool match(const int roomIndex,
               const KSTChatMsg &msg) const;

    QString ruleName;
    bool enabled;
    int ruleRoomIndex;
    InterConditionOperand interConditionOperand;
    QList<Condition> conditions;
    bool ruleValid;

private:
    void fromJson(const QJsonDocument &ruleDefinition);
    QByteArray toJson();
};

class chatHighlightEvaluator : public QObject
{
    Q_OBJECT

public:
    explicit chatHighlightEvaluator(const int roomIndex,
                                    QObject *parent = nullptr);
    ~chatHighlightEvaluator() { clearRules();}

    void clearRules();

    static QStringList getAllRuleNames();

public slots:
    void loadRules();
    bool shouldHighlight(const KSTChatMsg &msg,
                         QStringList &matchedRules);

private:
    QList<chatHighlightRule *>ruleList;
    int roomIndex;
};

class KSTChat : public QObject
{
    Q_OBJECT

public:
    const static QStringList chatRooms;

    explicit KSTChat(int chatRoomIndex,
                     const QString &username,
                     const QString &password,
                     const NewContactWidget *contact,
                     QObject *parent = nullptr);
    ~KSTChat();

    QList<KSTUsersInfo> getUsersList() const;
    KSTUsersInfo getUserInfo(const QString& username) const;

    static const QString getUsername();
    static const QString getPassword();
    static void saveUsernamePassword(const QString&, const QString&);

public slots:
    void connectChat();
    void disconnectChat();
    void sendMessage(const QString&);
    void reloadStationProfile();
    void resetDupe();
    void recalculateDupe();
    void recalculateDxccStatus();
    void updateSpotsStatusWhenQSOAdded(const QSqlRecord &record);
    void updateSpotsStatusWhenQSODeleted(const QSqlRecord &record);
    void updateSpotsDxccStatusWhenQSODeleted(const QSet<uint> &entities);

private slots:
    void receiveData();
    void socketConnected();
    void socketError(QAbstractSocket::SocketError socker_error);

signals:
    void chatConnected();
    void chatDisconnected();
    void chatError(QString);
    void chatMsg(KSTChatMsg);
    void usersListUpdated();

private:
    enum Command
    {
        NO_CMD = 0,
        LOGIN_CMD = 1,
        USER_CMD = 2,
        SHOW_USERS_CMD = 3,
        SET_GRID_CMD = 4
    };

    int chatRoomIdx;
    QString userName;
    QString password;
    QTcpSocket* socket;
    Command currCommand;
    QString receiveBuffer;
    QStringList commandLineBuffer;

    void sendShowUsersCommand();
    void sendCommand(const Command&, const QString&);
    void sendSetGridCommand();
    void finalizeShowUsersCommand(const QStringList&);
    QStringList joinLines(const QByteArray &data);
    QList<KSTUsersInfo> userList;
    QList<QPair<Command, QString>> commandQueue;

    const NewContactWidget *contact;

    static const QString SECURE_STORAGE_KEY;
    static const QString CONFIG_USERNAME_KEY;
};

#endif // QLOG_SERVICE_KSTCHAT_KSTCHAT_H
