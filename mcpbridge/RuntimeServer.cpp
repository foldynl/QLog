#include "RuntimeServer.h"
#include "RuntimeContext.h"
#include "core/debug.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QLocalServer>
#include <QLocalSocket>
#include <QThread>
#include <QTimer>
#include <QtMath>
#include <limits>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#ifdef Q_OS_LINUX
#include <sys/stat.h>
#endif
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

MODULE_IDENTIFICATION("qlog.mcpbridge.runtimeserver");

RuntimeServer::RuntimeServer(RuntimeContext &context) :
    QObject(&context),
    context(context),
    server(new QLocalServer(this))
{
    FCT_IDENTIFICATION;
    Q_ASSERT(QThread::currentThread() == context.thread());
#ifdef Q_OS_WIN
    // Qt applies a user-only ACL to the named pipe; filesystem permissions do not apply.
    server->setSocketOptions(QLocalServer::UserAccessOption);
#endif

    connect(server, &QLocalServer::newConnection, this, &RuntimeServer::acceptConnections);
    connect(&context, &RuntimeContext::contextReady, this, &RuntimeServer::collectionReady);
}

RuntimeServer::~RuntimeServer()
{
    FCT_IDENTIFICATION;

    server->close();
    // Child socket destruction must not call handlers after our registries are destroyed.
    for ( QLocalSocket *client : clients )
    {
        QObject::disconnect(client, nullptr, this, nullptr);
        client->abort();
    }
}

QString RuntimeServer::defaultEndpoint()
{
    FCT_IDENTIFICATION;

#ifdef Q_OS_LINUX
    // Do not use QStandardPaths' /tmp fallback: the MCP client must use the same desktop session.
    const QString runtimeDirectory = QFile::decodeName(qgetenv("XDG_RUNTIME_DIR"));
    if ( !QDir::isAbsolutePath(runtimeDirectory) || !isPrivateDirectory(QFileInfo(runtimeDirectory)) )
    {
        qCWarning(runtime) << "Cannot use private XDG_RUNTIME_DIR" << runtimeDirectory;
        return QString();
    }
    // Flatpak bind-mounts this host directory; its intermediate 'app' path can be a symlink.
    const QString appDirectory = QDir(runtimeDirectory).filePath("app/io.github.foldynl.QLog");
#elif defined(Q_OS_MACOS)
    // RuntimeLocation is ~/Library/Application Support on macOS, independent of GUI/CLI TMPDIR.
    const QString runtimeDirectory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if ( !QDir::isAbsolutePath(runtimeDirectory) )
    {
        qCWarning(runtime) << "Cannot locate runtime directory" << runtimeDirectory;
        return QString();
    }
    const QString appDirectory = QDir(runtimeDirectory).filePath("io.github.foldynl.QLog");
#else
    return QStringLiteral("qlog-runtime");
#endif

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    QFileInfo appInfo(appDirectory);
    if ( !appInfo.exists() && !appInfo.isSymLink() )
    {
        if ( !QDir().mkpath(appDirectory)
             || !QFile::setPermissions(appDirectory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner) )
        {
            qCWarning(runtime) << "Cannot create private runtime directory" << appDirectory;
            return QString();
        }
        appInfo.refresh();
    }
    // Preserve existing directories and reject insecure permissions, files and final symlinks.
    if ( !isPrivateDirectory(appInfo) )
    {
        qCWarning(runtime) << "Cannot use private runtime directory" << appDirectory;
        return QString();
    }
    return QDir(appDirectory).filePath("qlog-runtime");
#endif
}

#ifdef Q_OS_UNIX
bool RuntimeServer::isPrivateDirectory(const QFileInfo &info)
{
    FCT_IDENTIFICATION;

    const QFile::Permissions accessMask = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                         | QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup
                                         | QFile::ReadOther | QFile::WriteOther | QFile::ExeOther;
    const QFile::Permissions privatePermissions = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
    // QFileInfo also reports ReadUser/WriteUser/ExeUser; only the Unix owner/group/other bits matter.
    return info.isDir() && !info.isSymLink() && info.ownerId() == uint(::geteuid())
           && (info.permissions() & accessMask) == privatePermissions;
}
#endif

bool RuntimeServer::listen(const QString &name)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << name;
    Q_ASSERT(QThread::currentThread() == thread());

    const QString endpoint = name.isEmpty() ? defaultEndpoint() : name;
    if ( endpoint.isEmpty() )
        return false;

    // Unix keeps Qt's default bind behavior: access options can replace an existing endpoint.
    bool listening = server->listen(endpoint);
#ifdef Q_OS_LINUX
    // Other Unix systems can refuse connections even when a live listener's backlog is full.
    if ( !listening && server->serverError() == QAbstractSocket::AddressInUseError )
    {
        QLocalSocket probe;
        probe.connectToServer(endpoint);
        // An inconclusive probe must not unlink a potentially live server.
        if ( !probe.waitForConnected(0) && probe.error() == QLocalSocket::ConnectionRefusedError )
        {
            const QString path = QDir::isAbsolutePath(endpoint) ? endpoint : QDir::temp().filePath(endpoint);
            const QByteArray encodedPath = QFile::encodeName(path);
            struct stat fileInfo;
            // QFileInfo has no socket type check; refusal also occurs for files, so keep lstat here.
            if ( ::lstat(encodedPath.constData(), &fileInfo) == 0 && S_ISSOCK(fileInfo.st_mode)
                 && fileInfo.st_uid == ::geteuid() && QLocalServer::removeServer(endpoint) )
                listening = server->listen(endpoint);
        }
    }
#endif
    if ( !listening )
    {
        qCWarning(runtime) << "Cannot listen on runtime endpoint" << endpoint << server->errorString();
        return false;
    }
#ifdef Q_OS_UNIX
    // The private app directory protects the default endpoint until its permissions are restricted.
    if ( !QFile::setPermissions(server->fullServerName(), QFile::ReadOwner | QFile::WriteOwner) )
    {
        qCWarning(runtime) << "Cannot secure runtime socket" << server->fullServerName();
        server->close();
        return false;
    }
#endif
    qCDebug(runtime) << "Runtime endpoint" << server->fullServerName();
    return true;
}

void RuntimeServer::acceptConnections()
{
    FCT_IDENTIFICATION;

    while ( server->hasPendingConnections() )
    {
        QLocalSocket *client = server->nextPendingConnection();
        client->setParent(this);

        if ( clients.size() >= MAX_CLIENTS )
        {
            client->abort();
            client->deleteLater();
            continue;
        }

        client->setReadBufferSize(MAX_REQUEST_SIZE + 1);
        clients.insert(client);

        auto idleTimer = new QTimer(client);
        idleTimer->setSingleShot(true);
        idleTimer->setInterval(CLIENT_IDLE_TIMEOUT_MS);
        connect(idleTimer, &QTimer::timeout, client, &QLocalSocket::abort);
        idleTimer->start();

        connect(client, &QLocalSocket::readyRead, this, [this, client, idleTimer]()
        {
            // Partial bytes must not keep an unfinished request alive indefinitely.
            if ( client->canReadLine() )
                idleTimer->start();
            readClient(client);
        });

        connect(client, &QLocalSocket::disconnected, this, [this, client]()
        {
            removeClient(client);
        });

        readClient(client);
    }
}

void RuntimeServer::readClient(QLocalSocket *client)
{
    FCT_IDENTIFICATION;

    // QLocalSocket retains incomplete lines; one read may contain multiple requests.
    while ( client->state() == QLocalSocket::ConnectedState && client->canReadLine() )
    {
        const QByteArray message = client->readLine(MAX_REQUEST_SIZE + 2);
        if ( message.size() > MAX_REQUEST_SIZE )
        {
            client->abort();
            return;
        }
        dispatch(client, message);
    }
    if ( client->bytesAvailable() > MAX_REQUEST_SIZE )
        client->abort();
}

void RuntimeServer::dispatch(QLocalSocket *client, const QByteArray &message)
{
    FCT_IDENTIFICATION;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);

    if ( parseError.error != QJsonParseError::NoError )
    {
        sendError(client, QJsonValue(), QStringLiteral("parse_error"), tr("Invalid JSON request"));
        return;
    }

    const QJsonObject request = document.object();
    const QJsonValue id = request.value("id");
    const double numberId = id.toDouble();

    // Qt5 stores JSON numbers as doubles. Limit IDs to 2^53 - 1 so rounding cannot
    // change the identifier echoed in the reply.
    static const double MAX_SAFE_JSON_INTEGER = static_cast<double>((quint64(1) << std::numeric_limits<double>::digits) - 1);
    const bool validId = id.isString()
                         || (id.isDouble() && qAbs(numberId) <= MAX_SAFE_JSON_INTEGER
                             && numberId == static_cast<qint64>(numberId));

    if ( !document.isObject() || !validId )
    {
        sendError(client, QJsonValue(), QStringLiteral("invalid_request"), tr("Invalid request identifier"));
        return;
    }

    for ( auto field = request.constBegin(); field != request.constEnd(); ++field )
    {
        if ( field.key() != "id" && field.key() != "method" && field.key() != "params" )
        {
            sendError(client, id, QStringLiteral("invalid_request"), tr("Unknown request property"));
            return;
        }
    }

    const QJsonValue methodValue = request.value("method");

    if ( !methodValue.isString() || methodValue.toString().isEmpty() )
    {
        sendError(client, id, QStringLiteral("invalid_request"), tr("Invalid request method"));
        return;
    }

    const QString method = methodValue.toString();
    const QJsonValue params = request.value("params");

    if ( !params.isUndefined() && !params.isObject() )
    {
        sendError(client, id, QStringLiteral("invalid_params"), tr("Parameters must be an object"));
        return;
    }

    QStringList sources;
    const QJsonObject parameters = params.toObject();
    const bool acceptsSources = method == "runtime.get_schema" || method == "runtime.get_context";

    for ( auto parameter = parameters.constBegin(); parameter != parameters.constEnd(); ++parameter )
    {
        if ( !acceptsSources || parameter.key() != "sources" )
        {
            sendError(client, id, QStringLiteral("invalid_params"), tr("Unknown method parameter"));
            return;
        }
    }

    if ( parameters.contains("sources") )
    {
        const QJsonValue selection = parameters.value("sources");
        if ( !selection.isArray() || selection.toArray().isEmpty() )
        {
            sendError(client, id, QStringLiteral("invalid_params"), tr("Sources must be a non-empty array of names"));
            return;
        }
        for ( const QJsonValue &source : selection.toArray() )
        {
            if ( !source.isString() || !context.hasSource(source.toString()) )
            {
                sendError(client, id, QStringLiteral("invalid_params"), tr("Unknown or invalid runtime source"));
                return;
            }
            sources.append(source.toString());
        }
    }

    qCDebug(function_parameters) << method.left(128);

    if ( method == "system.get_info" )
    {
        const QJsonObject info{{"protocol", 1}, {"application", "QLog"},
                               {"version", QCoreApplication::applicationVersion()}};
        sendReply(client, QJsonObject{{"id", id}, {"result", info}});
    }
    else if ( method == "runtime.list_sources" )
        sendReply(client, QJsonObject{{"id", id}, {"result", context.sources()}});
    else if ( method == "runtime.get_schema" )
        sendReply(client, QJsonObject{{"id", id}, {"result", context.schema(sources)}});
    else if ( method == "runtime.get_context" )
    {
        const quint64 requestId = ++nextRequestId;
        PendingRequest pending;
        pending.client = client;
        pending.id = id;
        // Empty/busy collections may reply synchronously, so register before collecting.
        requests.insert(requestId, pending);
        context.collect(requestId, 1000, sources);
    }
    else
        sendError(client, id, QStringLiteral("method_not_found"), tr("Unknown runtime method"));
}

void RuntimeServer::collectionReady(quint64 requestId, const QJsonObject &result)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << requestId;

    auto request = requests.find(requestId);

    if ( request == requests.end() )
        return;

    const PendingRequest pending = request.value();
    requests.erase(request);

    if ( pending.client && pending.client->state() == QLocalSocket::ConnectedState )
        sendReply(pending.client, QJsonObject{{"id", pending.id}, {"result", result}});
}

void RuntimeServer::removeClient(QLocalSocket *client)
{
    FCT_IDENTIFICATION;

    clients.remove(client);
    for ( auto request = requests.begin(); request != requests.end(); )
    {
        if ( request->client == client )
            request = requests.erase(request);
        else
            ++request;
    }
    client->deleteLater();
}

void RuntimeServer::sendReply(QLocalSocket *client, const QJsonObject &reply)
{
    FCT_IDENTIFICATION;

    const QByteArray message = QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n';
    // A client that stops reading must not create an unbounded write buffer.
    if ( client->bytesToWrite() + message.size() > MAX_RESPONSE_BACKLOG )
    {
        client->abort();
        return;
    }

    if ( client->write(message) == -1 )
    {
        qCWarning(runtime) << "Cannot write runtime response" << client->errorString();
        client->abort();
    }
}

void RuntimeServer::sendError(QLocalSocket *client, const QJsonValue &id,
                              const QString &code, const QString &message)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << code;

    const QJsonObject error{{"code", code}, {"message", message}};
    sendReply(client, QJsonObject{{"id", id}, {"error", error}});
}
