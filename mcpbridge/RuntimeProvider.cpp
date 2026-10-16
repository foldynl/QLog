#include "RuntimeProvider.h"
#include "core/debug.h"
#include <QThread>
#include <QMutexLocker>
#include <QtMath>

MODULE_IDENTIFICATION("qlog.mcpbridge.runtimeprovider");

RuntimeProvider::RuntimeProvider(QObject *owner)
    : QObject(owner)
{
    FCT_IDENTIFICATION;    
}

bool RuntimeProvider::addBoolean(const QString &name,
                                 const QString &description,
                                 const std::function<QJsonValue()> &getter)
{
    FCT_IDENTIFICATION;

    return addField(name, QJsonValue::Bool, description, QString(), getter);
}

bool RuntimeProvider::addNumber(const QString &name,
                                const QString &description,
                                const QString &unit,
                                const std::function<QJsonValue()> &getter)
{
    FCT_IDENTIFICATION;

    return addField(name, QJsonValue::Double, description, unit, getter);
}

bool RuntimeProvider::addString(const QString &name,
                                const QString &description,
                                const std::function<QJsonValue()> &getter)
{
    FCT_IDENTIFICATION;

    return addField(name, QJsonValue::String, description, QString(), getter);
}

bool RuntimeProvider::addField(const QString &name,
                               QJsonValue::Type type,
                               const QString &description,
                               const QString &unit,
                               const std::function<QJsonValue()> &getter)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << name << unit;

    if ( QThread::currentThread() != thread() )
    {
        qCWarning(runtime) << "Runtime fields must be defined in the owner's thread";
        return false;
    }

    QMutexLocker locker(&fieldsMutex);
    if ( frozen
         || name.trimmed().isEmpty()
         || description.trimmed().isEmpty()
         || fields.contains(name)
         || !getter )
    {
        qCWarning(runtime) << "Cannot define runtime field" << name;
        return false;
    }

    const QString typeName = type == QJsonValue::Bool ? QStringLiteral("boolean")
                           : type == QJsonValue::Double ? QStringLiteral("number")
                                                       : QStringLiteral("string");
    QJsonObject metadata{{"type", typeName}, {"description", description}, {"nullable", true}};

    if ( !unit.isEmpty() )
        metadata.insert("unit", unit);

    fields.insert(name, metadata);
    getters.append(Field{name, type, getter});
    return true;
}

QJsonObject RuntimeProvider::schema() const
{
    FCT_IDENTIFICATION;
    QMutexLocker locker(&fieldsMutex);
    return fields;
}

void RuntimeProvider::freeze()
{
    FCT_IDENTIFICATION;
    QMutexLocker locker(&fieldsMutex);
    frozen = true;
}

void RuntimeProvider::collect(quint64 collectionId)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << collectionId;

    if ( QThread::currentThread() != thread() )
    {
        if ( !QMetaObject::invokeMethod(this, "collect", Qt::QueuedConnection,
                                       Q_ARG(quint64, collectionId)) )
            qCWarning(runtime) << "Cannot queue runtime snapshot";
        return;
    }

    // Freeze before reading so registration or a getter cannot alter the field list.
    freeze();
    QJsonObject values;

    for ( const Field &field : getters )
    {
        QJsonValue value;
        try
        {
            value = field.getter();
        }
        catch ( ... )
        {
            qCWarning(runtime) << "Cannot read runtime field" << field.name;
        }
        if ( !value.isNull() && (value.type() != field.type
                                 || (value.isDouble()
                                     && !qIsFinite(value.toDouble()))) )
        {
            qCWarning(runtime) << "Invalid runtime field value" << field.name;
            value = QJsonValue();
        }

        values.insert(field.name, value);
    }
    emit snapshotReady(collectionId, values);
}
