#ifndef QLOG_MCPBRIDGE_RUNTIMECONTEXT_H
#define QLOG_MCPBRIDGE_RUNTIMECONTEXT_H

#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

class RuntimeProvider;
class RuntimeProviderRequest;

class RuntimeContext : public QObject
{
    Q_OBJECT

public:
    explicit RuntimeContext(QObject *parent = nullptr);

    // Register while the provider is alive, after assigning its final thread.
    // Registration and metadata access belong to this context's thread.
    bool addProvider(const QString &source, const QString &description, RuntimeProvider *provider);
    QJsonObject sources() const;
    bool hasSource(const QString &source) const;
    QJsonObject schema(const QStringList &sources = QStringList()) const;

public slots:
    // Empty selection preserves the all-provider behavior; callers validate names with hasSource().
    void collect(quint64 requestId, int timeoutMs = 1000, const QStringList &sources = QStringList());

signals:
    void contextReady(quint64 requestId, QJsonObject context);

private slots:
    void collectionTimedOut();

private:
    struct Provider
    {
        QString source;
        QString description;
        RuntimeProviderRequest *request;
        QJsonObject fields;
        quint64 pendingCollection = 0;
    };

    void snapshotReceived(quint64 providerId, quint64 collectionId, const QJsonObject &snapshot);
    void removeProvider(quint64 providerId);
    void finishCollection();
    void setIssue(const QJsonObject &fields, const QString &issue);

    QHash<quint64, Provider> providers;
    quint64 nextProviderId = 0;
    quint64 nextCollectionId = 0;
    quint64 activeCollection = 0;
    quint64 activeRequest = 0;
    QSet<quint64> pendingProviders;
    QJsonObject values;
    QJsonObject issues;
    QTimer *collectionTimer;
};

#endif // QLOG_MCPBRIDGE_RUNTIMECONTEXT_H
