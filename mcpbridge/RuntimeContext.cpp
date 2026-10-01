#include "RuntimeContext.h"
#include "RuntimeProvider.h"
#include "core/debug.h"
#include <QThread>

MODULE_IDENTIFICATION("qlog.mcpbridge.runtimecontext");

// A per-provider sender lets Qt discard requests safely when a worker is destroyed.
// The registry never invokes a possibly destroyed worker through a raw pointer.
class RuntimeProviderRequest : public QObject
{
    Q_OBJECT

public:
    explicit RuntimeProviderRequest(QObject *parent) : QObject(parent)
    {
        FCT_IDENTIFICATION;
    }

signals:
    void collectRequested(quint64 collectionId);
};

RuntimeContext::RuntimeContext(QObject *parent) :
    QObject(parent),
    collectionTimer(new QTimer(this))
{
    FCT_IDENTIFICATION;

    collectionTimer->setSingleShot(true);
    connect(collectionTimer, &QTimer::timeout, this, &RuntimeContext::collectionTimedOut);
}

bool RuntimeContext::addProvider(const QString &source,
                                 const QString &description,
                                 RuntimeProvider *provider)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << source;

    if ( QThread::currentThread() != thread() || !provider )
    {
        qCWarning(runtime) << "Runtime provider registration requires the context thread";
        return false;
    }

    provider->freeze();

    if ( source.trimmed().isEmpty()
         || source != source.trimmed()
         || description.trimmed().isEmpty()
         || hasSource(source) )
    {
        qCWarning(runtime) << "Invalid or duplicate runtime source" << source;
        return false;
    }

    const QJsonObject fields = provider->schema();
    const QJsonObject registered = schema().value("fields").toObject();

    if ( fields.isEmpty() )
    {
        qCWarning(runtime) << "Runtime provider has no valid field definitions";
        return false;
    }

    for ( auto field = fields.constBegin(); field != fields.constEnd(); ++field )
    {
        if ( registered.contains(field.key()) )
        {
            qCWarning(runtime) << "Duplicate runtime field" << field.key();
            return false;
        }
    }

    const quint64 providerId = ++nextProviderId;
    Provider entry;
    entry.source = source;
    entry.description = description;
    entry.request = new RuntimeProviderRequest(this);
    entry.fields = fields;

    connect(entry.request, &RuntimeProviderRequest::collectRequested,
            provider, &RuntimeProvider::collect, Qt::QueuedConnection);

    connect(provider, &RuntimeProvider::snapshotReady, this,
            [this, providerId](quint64 collectionId, const QJsonObject &snapshot)
    {
        snapshotReceived(providerId, collectionId, snapshot);
    }, Qt::QueuedConnection);

    connect(provider, &QObject::destroyed, this, [this, providerId]()
    {
        removeProvider(providerId);
    }, Qt::QueuedConnection);

    connect(provider->thread(), &QThread::finished, this, [this, providerId]()
    {
        removeProvider(providerId);
    }, Qt::QueuedConnection);

    providers.insert(providerId, entry);
    return true;
}

QJsonObject RuntimeContext::sources() const
{
    FCT_IDENTIFICATION;
    Q_ASSERT(QThread::currentThread() == thread());

    QJsonObject result;

    for ( const Provider &provider : providers )
        result.insert(provider.source, QJsonObject{{"description", provider.description}});

    return QJsonObject{{"sources", result}};
}

bool RuntimeContext::hasSource(const QString &source) const
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << source;
    Q_ASSERT(QThread::currentThread() == thread());

    for ( const Provider &provider : providers )
    {
        if ( provider.source == source )
            return true;
    }
    return false;
}

QJsonObject RuntimeContext::schema(const QStringList &sources) const
{
    FCT_IDENTIFICATION;
    Q_ASSERT(QThread::currentThread() == thread());

    QJsonObject fields;

    for ( const Provider &provider : providers )
    {
        if ( !sources.isEmpty() && !sources.contains(provider.source) )
            continue;

        for ( auto field = provider.fields.constBegin(); field != provider.fields.constEnd(); ++field )
        {
            QJsonObject metadata = field.value().toObject();
            metadata.insert("source", provider.source);
            fields.insert(field.key(), metadata);
        }
    }

    return QJsonObject{{"fields", fields}};
}

void RuntimeContext::collect(quint64 requestId, int timeoutMs, const QStringList &sources)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << requestId << timeoutMs << sources;

    if ( QThread::currentThread() != thread() )
    {
        if ( !QMetaObject::invokeMethod(this, "collect", Qt::QueuedConnection,
                                       Q_ARG(quint64, requestId), Q_ARG(int, timeoutMs),
                                       Q_ARG(QStringList, sources)) )
            qCWarning(runtime) << "Cannot queue runtime context collection";
        return;
    }

    if ( activeCollection || timeoutMs <= 0 )
    {
        QJsonObject unavailable;
        QJsonObject failures;
        const QJsonObject fields = schema(sources).value("fields").toObject();

        for ( auto field = fields.constBegin(); field != fields.constEnd(); ++field )
        {
            unavailable.insert(field.key(), QJsonValue());
            failures.insert(field.key(), activeCollection ? "context_busy" : "invalid_timeout");
        }

        emit contextReady(requestId, QJsonObject{{"values", unavailable},
                                                 {"issues", failures}});
        return;
    }

    activeCollection = ++nextCollectionId;
    activeRequest = requestId;
    values = QJsonObject();
    issues = QJsonObject();
    collectionTimer->start(timeoutMs);

    for ( auto provider = providers.begin(); provider != providers.end(); ++provider )
    {
        if ( !sources.isEmpty() && !sources.contains(provider->source) )
            continue;

        for ( auto field = provider->fields.constBegin(); field != provider->fields.constEnd(); ++field )
            values.insert(field.key(), QJsonValue());

        if ( provider->pendingCollection )
        {
            setIssue(provider->fields, QStringLiteral("provider_busy"));
            continue;
        }

        provider->pendingCollection = activeCollection;
        pendingProviders.insert(provider.key());
        emit provider->request->collectRequested(activeCollection);
    }

    if ( pendingProviders.isEmpty() )
        finishCollection();
}

void RuntimeContext::snapshotReceived(quint64 providerId, quint64 collectionId,
                                      const QJsonObject &snapshot)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << providerId << collectionId;

    auto provider = providers.find(providerId);

    if ( provider == providers.end() || provider->pendingCollection != collectionId )
        return;

    provider->pendingCollection = 0;

    if ( collectionId != activeCollection )
        return;

    for ( auto field = provider->fields.constBegin(); field != provider->fields.constEnd(); ++field )
        values.insert(field.key(), snapshot.value(field.key()));

    pendingProviders.remove(providerId);

    if ( pendingProviders.isEmpty() )
        finishCollection();
}

void RuntimeContext::removeProvider(quint64 providerId)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << providerId;

    auto provider = providers.find(providerId);

    if ( provider == providers.end() )
        return;

    if ( pendingProviders.remove(providerId) )
        setIssue(provider->fields, QStringLiteral("provider_unavailable"));

    provider->request->deleteLater();
    providers.erase(provider);

    if ( activeCollection && pendingProviders.isEmpty() )
        finishCollection();
}

void RuntimeContext::setIssue(const QJsonObject &fields, const QString &issue)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << issue;

    for ( auto field = fields.constBegin(); field != fields.constEnd(); ++field )
        issues.insert(field.key(), issue);
}

void RuntimeContext::collectionTimedOut()
{
    FCT_IDENTIFICATION;

    for ( const quint64 providerId : pendingProviders )
        setIssue(providers.value(providerId).fields, QStringLiteral("provider_timeout"));
    // Keep each outstanding provider request until its reply; retries must not grow its queue.
    finishCollection();
}

void RuntimeContext::finishCollection()
{
    FCT_IDENTIFICATION;

    const quint64 requestId = activeRequest;
    const QJsonObject result{{"values", values}, {"issues", issues}};
    collectionTimer->stop();
    activeCollection = 0;
    pendingProviders.clear();
    values = QJsonObject();
    issues = QJsonObject();
    emit contextReady(requestId, result);
}

#include "RuntimeContext.moc"
