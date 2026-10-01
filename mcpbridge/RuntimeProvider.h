#ifndef QLOG_MCPBRIDGE_RUNTIMEPROVIDER_H
#define QLOG_MCPBRIDGE_RUNTIMEPROVIDER_H

#include <QObject>
#include <QJsonObject>
#include <QList>
#include <QMutex>
#include <functional>

class RuntimeProvider : public QObject
{
    Q_OBJECT

public:
    // Construct in the owner's thread; parent ownership keeps their affinities together.
    explicit RuntimeProvider(QObject *owner);

    // Define fields in the owner's thread before registration. Getters run there too.
    bool addBoolean(const QString &name,
                    const QString &description,
                    const std::function<QJsonValue()> &getter);
    bool addNumber(const QString &name,
                   const QString &description,
                   const QString &unit,
                   const std::function<QJsonValue()> &getter);
    bool addString(const QString &name,
                   const QString &description,
                   const std::function<QJsonValue()> &getter);

    // Metadata access is synchronized; registration freezes field definitions.
    QJsonObject schema() const;

public slots:
    void collect(quint64 collectionId);

signals:
    void snapshotReady(quint64 collectionId, QJsonObject values);

private:
    friend class RuntimeContext;

    struct Field
    {
        QString name;
        QJsonValue::Type type;
        std::function<QJsonValue()> getter;
    };

    bool addField(const QString &name,
                  QJsonValue::Type type,
                  const QString &description,
                  const QString &unit,
                  const std::function<QJsonValue()> &getter);
    void freeze();

    QJsonObject fields;
    QList<Field> getters;
    mutable QMutex fieldsMutex;
    // Registration or first collection permanently freezes field definitions and the getter list.
    // Never reset: RuntimeContext retains metadata and collect() iterates getters without a lock.
    // Values remain live: each collection calls the getters again.
    bool frozen = false;
};

#endif // QLOG_MCPBRIDGE_RUNTIMEPROVIDER_H
