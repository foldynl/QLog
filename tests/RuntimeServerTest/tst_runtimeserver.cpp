#include <QtTest>
#include <QJsonDocument>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>
#ifdef Q_OS_LINUX
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "mcpbridge/RuntimeContext.h"
#include "mcpbridge/RuntimeProvider.h"
#include "mcpbridge/RuntimeServer.h"

class RuntimeServerFixture
{
public:
    QObject owner;
    int value = 42;
    int reads = 0;
    RuntimeContext context;
    RuntimeServer server;
    const QString endpoint = "qlog-runtime-test-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool started;

    RuntimeServerFixture() : server(context)
    {
        auto provider = new RuntimeProvider(&owner);
        provider->addNumber("sample_value", "Sample value", QString(), [this]()
        {
            ++reads;
            return value;
        });
        started = context.addProvider("sample", "Test source sample", provider) && server.listen(endpoint);
    }
};

class RuntimeServerWorker : public QThread
{
public:
    explicit RuntimeServerWorker(QObject *owner)
    {
        owner->moveToThread(this);
        connect(this, &QThread::finished, owner, &QObject::deleteLater);
    }

    ~RuntimeServerWorker()
    {
        if ( !isRunning() && !isFinished() )
            start();
        quit();
        wait();
    }
};

class RuntimeEnvironment
{
public:
    const QByteArray original = qgetenv("XDG_RUNTIME_DIR");

    explicit RuntimeEnvironment(const QString &directory)
    {
        qputenv("XDG_RUNTIME_DIR", QFile::encodeName(directory));
    }

    ~RuntimeEnvironment()
    {
        if ( original.isNull() )
            qunsetenv("XDG_RUNTIME_DIR");
        else
            qputenv("XDG_RUNTIME_DIR", original);
    }
};

class RuntimeServerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void defaultEndpointUsesPrivateRuntimeDirectory_data();
    void defaultEndpointUsesPrivateRuntimeDirectory();
    void unsafeRuntimeDirectoryIsRejected_data();
    void unsafeRuntimeDirectoryIsRejected();
    void serviceInformation_data();
    void serviceInformation();
    void fragmentedAndMultipleFrames();
    void schemaAndFreshSnapshots();
    void sourceCatalogDoesNotReadValues();
    void selectedSourcesSkipOtherWorkers();
    void invalidRequests_data();
    void invalidRequests();
    void boundarySizedFrameIsAccepted();
    void oversizedFrameDisconnectsOnlyItsClient();
    void oversizedReplyDisconnectsOnlyItsClient();
    void identicalIdsOnDifferentClientsRemainIndependent();
    void readsWorkerStateInItsThread();
    void disconnectedClientDoesNotReceiveLateReply();
    void listenFailurePreservesExistingServer();
    void staleEndpointIsRecovered();
    void listeningEndpointIsPreserved();
    void regularFileIsPreserved();
    void clientLimitRejectsOnlyExcessConnections();
    void idleConnectionTimesOut_data();
    void idleConnectionTimesOut();
    void completeRequestRefreshesIdleDeadline();
    void serverShutdownClosesConnections();

private:
    static QJsonObject readReply(QLocalSocket &client);
};

void RuntimeServerTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
    QCoreApplication::setApplicationVersion("test-version");
}

void RuntimeServerTest::defaultEndpointUsesPrivateRuntimeDirectory_data()
{
    QTest::addColumn<bool>("existingDirectory");
    QTest::addColumn<bool>("sandboxLink");
    QTest::newRow("native-create") << false << false;
    QTest::newRow("native-reuse") << true << false;
    QTest::newRow("sandbox-app-link") << true << true;
}

void RuntimeServerTest::defaultEndpointUsesPrivateRuntimeDirectory()
{
#ifndef Q_OS_LINUX
    QSKIP("The shared Flatpak endpoint is Linux-specific");
#else
    QFETCH(bool, existingDirectory);
    QFETCH(bool, sandboxLink);
    QTemporaryDir directory;
    QTemporaryDir backing;
    QVERIFY(directory.isValid());
    QVERIFY(backing.isValid());
    RuntimeEnvironment environment(directory.path());
    if ( sandboxLink )
        QVERIFY(QFile::link(backing.path(), directory.filePath("app")));
    const QString appDirectory = directory.filePath("app/io.github.foldynl.QLog");
    const QString endpoint = appDirectory + "/qlog-runtime";
    if ( existingDirectory )
    {
        QVERIFY(QDir().mkpath(appDirectory));
        QVERIFY(QFile::setPermissions(appDirectory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }
    {
        RuntimeContext context;
        RuntimeServer server(context);
        QVERIFY(server.listen());
        struct stat info;
        QVERIFY(::stat(QFile::encodeName(appDirectory).constData(), &info) == 0);
        QCOMPARE(info.st_mode & 0777, mode_t(0700));
        QCOMPARE(info.st_uid, ::geteuid());
        QVERIFY(::lstat(QFile::encodeName(endpoint).constData(), &info) == 0);
        QVERIFY(S_ISSOCK(info.st_mode));
        QCOMPARE(info.st_mode & 0777, mode_t(0600));
        QLocalSocket client;
        client.connectToServer(endpoint);
        QVERIFY(client.waitForConnected(1000));
        client.write("{\"id\":1,\"method\":\"system.get_info\"}\n");
        QCOMPARE(readReply(client).value("result").toObject().value("application").toString(), QString("QLog"));
    }
    QVERIFY(!QFile::exists(endpoint));
    QVERIFY(QFile::exists(appDirectory));
#endif
}

void RuntimeServerTest::unsafeRuntimeDirectoryIsRejected_data()
{
    QTest::addColumn<QString>("problem");
    for ( const QString &problem : QStringList{"unset", "relative", "root-link", "shared-root", "shared-app", "app-link", "app-file"} )
        QTest::newRow(qPrintable(problem)) << problem;
}

void RuntimeServerTest::unsafeRuntimeDirectoryIsRejected()
{
#ifndef Q_OS_LINUX
    QSKIP("The shared Flatpak endpoint is Linux-specific");
#else
    QFETCH(QString, problem);
    QTemporaryDir directory;
    QTemporaryDir backing;
    QVERIFY(directory.isValid());
    QVERIFY(backing.isValid());
    RuntimeEnvironment environment(problem == "relative" ? QString("relative") : directory.path());
    if ( problem == "unset" )
        qunsetenv("XDG_RUNTIME_DIR");
    if ( problem == "root-link" )
    {
        const QString alias = directory.filePath("runtime-link");
        QVERIFY(QFile::link(backing.path(), alias));
        qputenv("XDG_RUNTIME_DIR", QFile::encodeName(alias));
    }
    const QString appDirectory = directory.filePath("app/io.github.foldynl.QLog");
    if ( problem == "shared-root" )
        QVERIFY(QFile::setPermissions(directory.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadOther | QFile::ExeOther));
    if ( problem == "shared-app" )
    {
        QVERIFY(QDir().mkpath(appDirectory));
        QVERIFY(QFile::setPermissions(appDirectory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadOther | QFile::ExeOther));
    }
    if ( problem == "app-link" || problem == "app-file" )
    {
        QVERIFY(QDir().mkpath(directory.filePath("app")));
        if ( problem == "app-link" )
            QVERIFY(QFile::link(backing.path(), appDirectory));
        else
        {
            QFile file(appDirectory);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write("keep"), qint64(4));
        }
    }
    RuntimeContext context;
    RuntimeServer server(context);
    QVERIFY(!server.listen());
    QVERIFY(!QFile::exists(appDirectory + "/qlog-runtime"));
    if ( problem == "app-file" )
    {
        QFile file(appDirectory);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("keep"));
    }
#endif
}

QJsonObject RuntimeServerTest::readReply(QLocalSocket &client)
{
    QElapsedTimer timer;
    timer.start();
    while ( !client.canReadLine() && client.state() != QLocalSocket::UnconnectedState
            && timer.elapsed() < 3000 )
        QTest::qWait(1);
    return QJsonDocument::fromJson(client.readLine()).object();
}

void RuntimeServerTest::serviceInformation_data()
{
    QTest::addColumn<QJsonValue>("id");
    QTest::newRow("string") << QJsonValue("info");
    QTest::newRow("large-string") << QJsonValue("9007199254740993");
    QTest::newRow("largest-safe-number") << QJsonValue(9007199254740991.0);
    QTest::newRow("smallest-safe-number") << QJsonValue(-9007199254740991.0);
}

void RuntimeServerTest::serviceInformation()
{
    QFETCH(QJsonValue, id);
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QLocalSocket client;
    client.connectToServer(fixture.endpoint);
    QVERIFY(client.waitForConnected(1000));
    client.write(QJsonDocument(QJsonObject{{"id", id}, {"method", "system.get_info"}})
                 .toJson(QJsonDocument::Compact) + '\n');
    const QJsonObject reply = readReply(client);
    QCOMPARE(reply.value("id"), id);
    const QJsonObject result = reply.value("result").toObject();
    QCOMPARE(result.value("protocol").toInt(), 1);
    QCOMPARE(result.value("application").toString(), QString("QLog"));
    QCOMPARE(result.value("version").toString(), QString("test-version"));
    QCOMPARE(fixture.reads, 0);
}

void RuntimeServerTest::fragmentedAndMultipleFrames()
{
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QLocalSocket client;
    client.connectToServer(fixture.endpoint);
    QVERIFY(client.waitForConnected(1000));
    client.write("{\"id\":1,\"method\":\"system.");
    client.flush();
    QTest::qWait(1);
    QCOMPARE(client.bytesAvailable(), qint64(0));
    client.write("get_info\"}\n{\"id\":\"line\\nbreak\",\"method\":\"runtime.get_schema\"}\n");
    QCOMPARE(readReply(client).value("id").toInt(), 1);
    const QJsonObject reply = readReply(client);
    QCOMPARE(reply.value("id").toString(), QString("line\nbreak"));
    QVERIFY(reply.value("result").toObject().value("fields").toObject().contains("sample_value"));
}

void RuntimeServerTest::schemaAndFreshSnapshots()
{
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QLocalSocket client;
    client.connectToServer(fixture.endpoint);
    QVERIFY(client.waitForConnected(1000));
    client.write("{\"id\":1,\"method\":\"runtime.get_schema\",\"params\":{}}\n");
    const QJsonObject schema = readReply(client).value("result").toObject();
    QCOMPARE(schema.value("fields").toObject().value("sample_value").toObject()
             .value("type").toString(), QString("number"));
    for ( const int value : {42, 17} )
    {
        fixture.value = value;
        client.write("{\"id\":2,\"method\":\"runtime.get_context\"}\n");
        const QJsonObject reply = readReply(client);
        QCOMPARE(reply.value("id").toInt(), 2);
        const QJsonObject result = reply.value("result").toObject();
        QCOMPARE(result.value("values").toObject().value("sample_value").toInt(), value);
        QVERIFY(result.value("issues").toObject().isEmpty());
    }
    QCOMPARE(fixture.reads, 2);
}

void RuntimeServerTest::sourceCatalogDoesNotReadValues()
{
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QLocalSocket client;
    client.connectToServer(fixture.endpoint);
    QVERIFY(client.waitForConnected(1000));
    client.write("{\"id\":1,\"method\":\"runtime.list_sources\"}\n");
    const QJsonObject sources = readReply(client).value("result").toObject().value("sources").toObject();
    QCOMPARE(sources.size(), 1);
    QVERIFY(!sources.value("sample").toObject().value("description").toString().isEmpty());
    QCOMPARE(fixture.reads, 0);
}

void RuntimeServerTest::selectedSourcesSkipOtherWorkers()
{
    auto owner = new QObject;
    auto provider = new RuntimeProvider(owner);
    provider->addBoolean("worker_ready", "Whether this worker is ready", [owner]()
    {
        return QThread::currentThread() == owner->thread();
    });
    RuntimeServerWorker worker(owner);
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QVERIFY(fixture.context.addProvider("worker", "Test source worker", provider));
    QSignalSpy snapshots(provider, &RuntimeProvider::snapshotReady);
    QLocalSocket client;
    client.connectToServer(fixture.endpoint);
    QVERIFY(client.waitForConnected(1000));
    client.write("{\"id\":1,\"method\":\"runtime.get_schema\",\"params\":{\"sources\":[\"sample\"]}}\n");
    const QJsonObject fields = readReply(client).value("result").toObject().value("fields").toObject();
    QCOMPARE(fields.size(), 1);
    QVERIFY(fields.contains("sample_value"));
    QCOMPARE(fixture.reads, 0);
    client.write("{\"id\":2,\"method\":\"runtime.get_context\",\"params\":{\"sources\":[\"sample\"]}}\n");
    const QJsonObject result = readReply(client).value("result").toObject();
    QCOMPARE(result.value("values").toObject(), QJsonObject({{"sample_value", 42}}));
    QVERIFY(result.value("issues").toObject().isEmpty());
    QCOMPARE(fixture.reads, 1);
    // An explicit request after startup proves no earlier collection was queued to this worker.
    worker.start();
    provider->collect(999);
    QTRY_COMPARE(snapshots.count(), 1);
    QCOMPARE(snapshots.first().at(0).toULongLong(), quint64(999));
    client.write("{\"id\":3,\"method\":\"runtime.get_context\",\"params\":{\"sources\":[\"sample\",\"worker\"]}}\n");
    const QJsonObject both = readReply(client).value("result").toObject();
    QCOMPARE(both.value("values").toObject(), QJsonObject({{"sample_value", 42}, {"worker_ready", true}}));
    QVERIFY(both.value("issues").toObject().isEmpty());
}

void RuntimeServerTest::invalidRequests_data()
{
    QTest::addColumn<QByteArray>("request");
    QTest::addColumn<QString>("code");
    QTest::addColumn<QJsonValue>("id");
    QTest::newRow("malformed") << QByteArray("{bad json}\n") << QString("parse_error") << QJsonValue();
    QTest::newRow("array") << QByteArray("[]\n") << QString("invalid_request") << QJsonValue();
    QTest::newRow("missing-id") << QByteArray("{\"method\":\"system.get_info\"}\n")
                              << QString("invalid_request") << QJsonValue();
    QTest::newRow("invalid-id") << QByteArray("{\"id\":true,\"method\":\"system.get_info\"}\n")
                              << QString("invalid_request") << QJsonValue();
    QTest::newRow("imprecise-id") << QByteArray("{\"id\":9007199254740993,\"method\":\"system.get_info\"}\n")
                                << QString("invalid_request") << QJsonValue();
    QTest::newRow("fractional-id") << QByteArray("{\"id\":0.5,\"method\":\"system.get_info\"}\n")
                                 << QString("invalid_request") << QJsonValue();
    QTest::newRow("invalid-method") << QByteArray("{\"id\":3,\"method\":4}\n")
                                  << QString("invalid_request") << QJsonValue(3);
    QTest::newRow("unknown-method") << QByteArray("{\"id\":3,\"method\":\"missing\"}\n")
                                  << QString("method_not_found") << QJsonValue(3);
    QTest::newRow("write-method") << QByteArray("{\"id\":3,\"method\":\"rig.set_frequency\"}\n")
                                << QString("method_not_found") << QJsonValue(3);
    QTest::newRow("unknown-property") << QByteArray("{\"id\":3,\"method\":\"system.get_info\",\"extra\":1}\n")
                                    << QString("invalid_request") << QJsonValue(3);
    QTest::newRow("params-type") << QByteArray("{\"id\":3,\"method\":\"runtime.get_context\",\"params\":null}\n")
                               << QString("invalid_params") << QJsonValue(3);
    QTest::newRow("unknown-param") << QByteArray("{\"id\":3,\"method\":\"runtime.get_context\",\"params\":{\"write\":true}}\n")
                                 << QString("invalid_params") << QJsonValue(3);
    const QList<QByteArray> invalidSources = {"[]", "null", "\"sample\"", "[7]", "[\"\"]",
                                              "[\"missing\"]", "[\"sample\",\"missing\"]"};
    for ( int i = 0; i < invalidSources.size(); ++i )
    {
        for ( const QByteArray &method : {QByteArray("runtime.get_context"), QByteArray("runtime.get_schema")} )
        {
            const QByteArray row = method + '-' + QByteArray::number(i);
            QTest::newRow(row.constData())
                << QByteArray("{\"id\":3,\"method\":\"") + method
                   + "\",\"params\":{\"sources\":" + invalidSources.at(i) + "}}\n"
                << QString("invalid_params") << QJsonValue(3);
        }
    }
    QTest::newRow("sources-on-catalog")
        << QByteArray("{\"id\":3,\"method\":\"runtime.list_sources\",\"params\":{\"sources\":[\"sample\"]}}\n")
        << QString("invalid_params") << QJsonValue(3);
}

void RuntimeServerTest::invalidRequests()
{
    QFETCH(QByteArray, request);
    QFETCH(QString, code);
    QFETCH(QJsonValue, id);
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QLocalSocket client;
    client.connectToServer(fixture.endpoint);
    QVERIFY(client.waitForConnected(1000));
    client.write(request);
    const QJsonObject reply = readReply(client);
    QCOMPARE(reply.value("id"), id);
    QCOMPARE(reply.value("error").toObject().value("code").toString(), code);
    QVERIFY(!reply.value("error").toObject().value("message").toString().isEmpty());
    QVERIFY(!reply.contains("result"));
    QCOMPARE(fixture.reads, 0);
    client.write("{\"id\":\"next\",\"method\":\"system.get_info\"}\n");
    QCOMPARE(readReply(client).value("id").toString(), QString("next"));
}

void RuntimeServerTest::boundarySizedFrameIsAccepted()
{
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QLocalSocket client;
    client.connectToServer(fixture.endpoint);
    QVERIFY(client.waitForConnected(1000));
    QByteArray request("{\"id\":1,\"method\":\"system.get_info\"}");
    request.append(QByteArray(65536 - request.size() - 1, ' '));
    request.append('\n');
    client.write(request);
    QCOMPARE(readReply(client).value("id").toInt(), 1);
    client.write("{\"id\":2,\"method\":\"system.get_info\"}\n");
    QCOMPARE(readReply(client).value("id").toInt(), 2);
}

void RuntimeServerTest::oversizedFrameDisconnectsOnlyItsClient()
{
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QLocalSocket client;
    client.connectToServer(fixture.endpoint);
    QVERIFY(client.waitForConnected(1000));
    client.write(QByteArray(65537, 'x'));
    QTRY_COMPARE(client.state(), QLocalSocket::UnconnectedState);
    QLocalSocket other;
    other.connectToServer(fixture.endpoint);
    QVERIFY(other.waitForConnected(1000));
    other.write("{\"id\":1,\"method\":\"system.get_info\"}\n");
    QCOMPARE(readReply(other).value("id").toInt(), 1);
}

void RuntimeServerTest::oversizedReplyDisconnectsOnlyItsClient()
{
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QObject owner;
    auto provider = new RuntimeProvider(&owner);
    provider->addString("large_value", "Large value", []()
    {
        return QString(1024 * 1024, QLatin1Char('x'));
    });
    QVERIFY(fixture.context.addProvider("worker", "Test source worker", provider));
    QLocalSocket client;
    client.connectToServer(fixture.endpoint);
    QVERIFY(client.waitForConnected(1000));
    client.write("{\"id\":1,\"method\":\"runtime.get_context\"}\n");
    QTRY_COMPARE(client.state(), QLocalSocket::UnconnectedState);
    QLocalSocket other;
    other.connectToServer(fixture.endpoint);
    QVERIFY(other.waitForConnected(1000));
    other.write("{\"id\":2,\"method\":\"system.get_info\"}\n");
    QCOMPARE(readReply(other).value("id").toInt(), 2);
}

void RuntimeServerTest::identicalIdsOnDifferentClientsRemainIndependent()
{
    auto owner = new QObject;
    auto provider = new RuntimeProvider(owner);
    provider->addNumber("worker_value", "Worker value", QString(), []() { return 7; });
    RuntimeServerWorker worker(owner);
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QVERIFY(fixture.context.addProvider("worker", "Test source worker", provider));
    QLocalSocket first;
    QLocalSocket second;
    first.connectToServer(fixture.endpoint);
    second.connectToServer(fixture.endpoint);
    QVERIFY(first.waitForConnected(1000));
    QVERIFY(second.waitForConnected(1000));
    first.write("{\"id\":7,\"method\":\"runtime.get_context\"}\n");
    QTRY_COMPARE(fixture.reads, 1);
    second.write("{\"id\":7,\"method\":\"runtime.get_context\"}\n");
    const QJsonObject busy = readReply(second);
    QCOMPARE(busy.value("id").toInt(), 7);
    QCOMPARE(busy.value("result").toObject().value("issues").toObject()
             .value("sample_value").toString(), QString("context_busy"));
    const QJsonObject reply = readReply(first);
    QCOMPARE(reply.value("id").toInt(), 7);
    const QJsonObject result = reply.value("result").toObject();
    QCOMPARE(result.value("values").toObject().value("sample_value").toInt(), 42);
    QVERIFY(result.value("values").toObject().value("worker_value").isNull());
    QCOMPARE(result.value("issues").toObject().value("worker_value").toString(),
             QString("provider_timeout"));
}

void RuntimeServerTest::readsWorkerStateInItsThread()
{
    auto owner = new QObject;
    auto provider = new RuntimeProvider(owner);
    provider->addString("worker_thread", "Owner thread", [owner]()
    {
        return QThread::currentThread() == owner->thread() ? QString("worker") : QString("wrong");
    });
    RuntimeServerWorker worker(owner);
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QVERIFY(fixture.context.addProvider("worker", "Test source worker", provider));
    worker.start();
    QLocalSocket client;
    client.connectToServer(fixture.endpoint);
    QVERIFY(client.waitForConnected(1000));
    client.write("{\"id\":1,\"method\":\"runtime.get_context\"}\n");
    QCOMPARE(readReply(client).value("result").toObject().value("values").toObject()
             .value("worker_thread").toString(), QString("worker"));
}

void RuntimeServerTest::disconnectedClientDoesNotReceiveLateReply()
{
    auto owner = new QObject;
    auto provider = new RuntimeProvider(owner);
    provider->addNumber("worker_value", "Worker value", QString(), []() { return 7; });
    RuntimeServerWorker worker(owner);
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QVERIFY(fixture.context.addProvider("worker", "Test source worker", provider));
    QSignalSpy collected(&fixture.context, &RuntimeContext::contextReady);
    QLocalSocket first;
    first.connectToServer(fixture.endpoint);
    QVERIFY(first.waitForConnected(1000));
    first.write("{\"id\":1,\"method\":\"runtime.get_context\"}\n");
    QTRY_COMPARE(fixture.reads, 1);
    first.abort();
    worker.start();
    QTRY_COMPARE(collected.count(), 1);
    QLocalSocket second;
    second.connectToServer(fixture.endpoint);
    QVERIFY(second.waitForConnected(1000));
    second.write("{\"id\":1,\"method\":\"runtime.get_context\"}\n");
    const QJsonObject result = readReply(second).value("result").toObject();
    QCOMPARE(result.value("values").toObject().value("worker_value").toInt(), 7);
    QVERIFY(result.value("issues").toObject().isEmpty());
    QCOMPARE(fixture.reads, 2);
}

void RuntimeServerTest::listenFailurePreservesExistingServer()
{
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QTest::ignoreMessage(QtWarningMsg, "QLocalServer::listen() called when already listening");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Cannot listen on runtime endpoint.*"));
    QVERIFY(!fixture.server.listen(fixture.endpoint));
    QLocalSocket client;
    client.connectToServer(fixture.endpoint);
    QVERIFY(client.waitForConnected(1000));
    client.write("{\"id\":1,\"method\":\"system.get_info\"}\n");
    QCOMPARE(readReply(client).value("id").toInt(), 1);
}

void RuntimeServerTest::staleEndpointIsRecovered()
{
#ifndef Q_OS_LINUX
    QSKIP("Automatic stale socket recovery is Linux-specific");
#else
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString original = directory.filePath("original");
    const QString endpoint = directory.filePath("stale");
    QLocalServer abandoned;
    QVERIFY(abandoned.listen(original));
    // Closing removes the original name; the renamed socket remains without a listener.
    QVERIFY(QFile::rename(abandoned.fullServerName(), endpoint));
    abandoned.close();
    QVERIFY(QFile::exists(endpoint));
    RuntimeContext context;
    RuntimeServer server(context);
    QVERIFY(server.listen(endpoint));
    QLocalSocket client;
    client.connectToServer(endpoint);
    QVERIFY(client.waitForConnected(1000));
    client.write("{\"id\":1,\"method\":\"system.get_info\"}\n");
    QCOMPARE(readReply(client).value("id").toInt(), 1);
#endif
}

void RuntimeServerTest::listeningEndpointIsPreserved()
{
#ifndef Q_OS_UNIX
    QSKIP("Exclusive filesystem endpoints are Unix-specific");
#else
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    RuntimeContext context;
    RuntimeServer other(context);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Cannot listen on runtime endpoint.*"));
    QVERIFY(!other.listen(fixture.endpoint));
    QLocalSocket client;
    client.connectToServer(fixture.endpoint);
    QVERIFY(client.waitForConnected(1000));
    client.write("{\"id\":1,\"method\":\"runtime.get_context\"}\n");
    QCOMPARE(readReply(client).value("result").toObject().value("values").toObject()
             .value("sample_value").toInt(), 42);
#endif
}

void RuntimeServerTest::regularFileIsPreserved()
{
#ifndef Q_OS_UNIX
    QSKIP("Filesystem endpoint collision is Unix-specific");
#else
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString endpoint = directory.filePath("existing-file");
    QFile file(endpoint);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("keep"), qint64(4));
    file.close();
    RuntimeContext context;
    RuntimeServer server(context);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Cannot listen on runtime endpoint.*"));
    QVERIFY(!server.listen(endpoint));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("keep"));
#endif
}

void RuntimeServerTest::clientLimitRejectsOnlyExcessConnections()
{
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QObject owner;
    QList<QLocalSocket *> accepted;
    for ( int i = 0; i < 16; ++i )
    {
        auto client = new QLocalSocket(&owner);
        client->connectToServer(fixture.endpoint);
        QVERIFY(client->waitForConnected(1000));
        client->write("{\"id\":1,\"method\":\"system.get_info\"}\n");
        QCOMPARE(readReply(*client).value("id").toInt(), 1);
        accepted.append(client);
    }
    QLocalSocket excess;
    excess.connectToServer(fixture.endpoint);
    QVERIFY(excess.waitForConnected(1000));
    excess.write("{\"id\":2,\"method\":\"system.get_info\"}\n");
    QTRY_COMPARE_WITH_TIMEOUT(excess.state(), QLocalSocket::UnconnectedState, 1000);
    accepted.last()->write("{\"id\":3,\"method\":\"system.get_info\"}\n");
    QCOMPARE(readReply(*accepted.last()).value("id").toInt(), 3);
    accepted.first()->abort();
    QTest::qWait(20);
    QLocalSocket replacement;
    replacement.connectToServer(fixture.endpoint);
    QVERIFY(replacement.waitForConnected(1000));
    replacement.write("{\"id\":4,\"method\":\"system.get_info\"}\n");
    QCOMPARE(readReply(replacement).value("id").toInt(), 4);
}

void RuntimeServerTest::idleConnectionTimesOut_data()
{
    QTest::addColumn<bool>("partialRequest");
    QTest::newRow("silent") << false;
    QTest::newRow("incomplete-request") << true;
}

void RuntimeServerTest::idleConnectionTimesOut()
{
    QFETCH(bool, partialRequest);
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QLocalSocket client;
    client.connectToServer(fixture.endpoint);
    QVERIFY(client.waitForConnected(1000));
    if ( partialRequest )
    {
        QTest::qWait(3000);
        client.write("{\"id\":");
        client.flush();
    }
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), QLocalSocket::UnconnectedState,
                              partialRequest ? 3000 : 6500);
}

void RuntimeServerTest::completeRequestRefreshesIdleDeadline()
{
    RuntimeServerFixture fixture;
    QVERIFY(fixture.started);
    QLocalSocket client;
    client.connectToServer(fixture.endpoint);
    QVERIFY(client.waitForConnected(1000));
    client.write("{\"id\":1,\"method\":\"system.get_info\"}\n");
    QCOMPARE(readReply(client).value("id").toInt(), 1);
    QTest::qWait(3000);
    client.write("{\"id\":2,\"method\":\"system.get_info\"}\n");
    QCOMPARE(readReply(client).value("id").toInt(), 2);
    QTest::qWait(3000);
    QCOMPARE(client.state(), QLocalSocket::ConnectedState);
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), QLocalSocket::UnconnectedState, 3000);
}

void RuntimeServerTest::serverShutdownClosesConnections()
{
    RuntimeContext context;
    QObject availableOwner;
    int reads = 0;
    auto available = new RuntimeProvider(&availableOwner);
    available->addNumber("available", "Available value", QString(), [&]() { return ++reads; });
    QVERIFY(context.addProvider("available", "Test source available", available));
    auto owner = new QObject;
    auto provider = new RuntimeProvider(owner);
    provider->addNumber("worker", "Worker value", QString(), []() { return 7; });
    RuntimeServerWorker worker(owner);
    QVERIFY(context.addProvider("worker", "Test source worker", provider));
    QSignalSpy collected(&context, &RuntimeContext::contextReady);
    const QString endpoint = "qlog-runtime-test-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QLocalSocket client;
    {
        RuntimeServer server(context);
        QVERIFY(server.listen(endpoint));
        client.connectToServer(endpoint);
        QVERIFY(client.waitForConnected(1000));
        client.write("{\"id\":1,\"method\":\"runtime.get_context\"}\n");
        QTRY_COMPARE(reads, 1);
    }
    QTRY_COMPARE(client.state(), QLocalSocket::UnconnectedState);
    worker.start();
    QTRY_COMPARE(collected.count(), 1);
    RuntimeServer replacement(context);
    QVERIFY(replacement.listen(endpoint));
    QLocalSocket nextClient;
    nextClient.connectToServer(endpoint);
    QVERIFY(nextClient.waitForConnected(1000));
    nextClient.write("{\"id\":1,\"method\":\"runtime.get_context\"}\n");
    const QJsonObject result = readReply(nextClient).value("result").toObject();
    QCOMPARE(result.value("values").toObject().value("available").toInt(), 2);
    QCOMPARE(result.value("values").toObject().value("worker").toInt(), 7);
    QVERIFY(result.value("issues").toObject().isEmpty());
}

QTEST_GUILESS_MAIN(RuntimeServerTest)

#include "tst_runtimeserver.moc"
