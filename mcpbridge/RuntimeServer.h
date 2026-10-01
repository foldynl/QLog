#ifndef QLOG_MCPBRIDGE_RUNTIMESERVER_H
#define QLOG_MCPBRIDGE_RUNTIMESERVER_H

#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QSet>

class RuntimeContext;
class QLocalServer;
class QLocalSocket;

// Protocol 1: UTF-8 JSON objects separated by '\n', with {id, method, params?}.
// IDs are strings or integers in [-2^53 + 1, 2^53 - 1]. Schema/context params may
// contain sources: a non-empty array of registered names; omitted selection reads all.
// Replies contain {id, result} or {id, error: {code, message}}.
class RuntimeServer : public QObject
{
    Q_OBJECT

public:
    // Construct in the context's thread; the context owns the server.
    explicit RuntimeServer(RuntimeContext &context);
    ~RuntimeServer();
    bool listen(const QString &name);

private:
    struct PendingRequest
    {
        QPointer<QLocalSocket> client;
        QJsonValue id;
    };

    enum
    {
        MAX_CLIENTS = 16,
        CLIENT_IDLE_TIMEOUT_MS = 5000,
        MAX_REQUEST_SIZE = 64 * 1024,
        MAX_RESPONSE_BACKLOG = 1024 * 1024
    };

    void acceptConnections();
    void readClient(QLocalSocket *client);
    void dispatch(QLocalSocket *client, const QByteArray &message);
    void collectionReady(quint64 requestId, const QJsonObject &result);
    void removeClient(QLocalSocket *client);
    void sendReply(QLocalSocket *client, const QJsonObject &reply);
    void sendError(QLocalSocket *client, const QJsonValue &id,
                   const QString &code, const QString &message);

    RuntimeContext &context;
    QLocalServer *server;
    QSet<QLocalSocket *> clients;
    QHash<quint64, PendingRequest> requests;
    quint64 nextRequestId = 0;
};

#endif // QLOG_MCPBRIDGE_RUNTIMESERVER_H
