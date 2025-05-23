#include <QSqlQuery>
#include <QCache>
#include <QSqlError>

#include "LogParam.h"
#include "debug.h"

MODULE_IDENTIFICATION("qlog.core.logparam");

#define PARAMMUTEXLOCKER     qCDebug(runtime) << "Waiting for mutex"; \
                             QMutexLocker locker(&cacheMutex); \
                             qCDebug(runtime) << "Using logparam"

LogParam::LogParam(QObject *parent) :
    QObject(parent)
{
    FCT_IDENTIFICATION;
}

bool LogParam::setParam(const QString &name, const QVariant &value)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << name << " " << value;

    QSqlQuery query;

    if ( ! query.prepare("INSERT OR REPLACE INTO log_param (name, value) "
                         "VALUES (:nam, :val)") )
    {
        qWarning()<< "Cannot prepare insert parameter statement for parameter" << name;
        return false;
    }

    query.bindValue(":nam", name);
    query.bindValue(":val", value);

    PARAMMUTEXLOCKER;

    if ( !query.exec() )
    {
        qWarning() << "Cannot exec an insert parameter statement for" << name;
        return false;
    }

    localCache.remove(name);

    qCWarning(runtime) << "Param:" << name << "Set - value" << value;
    return true;
}

QVariant LogParam::getParam(const QString &name, const QVariant &defaultValue)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << name;

    PARAMMUTEXLOCKER;

    const QVariant *valueCached = localCache.object(name);

    if ( valueCached )
    {
        qDebug(runtime) << "Param:" << name << "Cached value: " << *valueCached;
        return *valueCached;
    }

    QSqlQuery query;

    if ( ! query.prepare("SELECT value FROM log_param WHERE name = :nam") )
    {
        qWarning()<< "Cannot prepare select parameter statement for" << name;
        return defaultValue;
    }

    query.bindValue(":nam", name);

    if ( ! query.exec() )
    {
        qWarning() << "Cannot execute GetParam Select for" << name << "using default" << defaultValue;
        return defaultValue;
    }

    if ( query.first() )
    {
        if ( !query.isNull(0) )
        {
            QVariant dbValue = query.value(0);
            localCache.insert(name, new QVariant(dbValue));
            qDebug(runtime) << "Param:" << name << "DB value: " << dbValue;
            return dbValue;
        }
        qDebug(runtime) << "Param:" << name << "NULL value in DB - using default " << defaultValue;
    }
    else
        qDebug(runtime) << "Param:" << name << "Key not found in DB - using default " << defaultValue;

    return defaultValue;
}

bool LogParam::setParam(const QString &name, const QStringList &value)
{
    FCT_IDENTIFICATION;

    return setParam(name, serializeStringList(value));
}

QStringList LogParam::getParamStringList(const QString &name, const QStringList &defaultValue)
{
    FCT_IDENTIFICATION;

    return deserializeStringList(getParam(name, serializeStringList(defaultValue)).toString());
}

void LogParam::removeParamGroup(const QString &paramGroup)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << paramGroup;

    PARAMMUTEXLOCKER;

    const QStringList &keys = localCache.keys();

    for ( const QString& key : keys )
        if (key.startsWith(paramGroup)) localCache.remove(key);

    QSqlQuery query;

    if ( ! query.prepare("DELETE FROM log_param WHERE name LIKE :group ") )
    {
        qWarning()<< "Cannot prepare delete parameter statement for" << paramGroup;
        return;
    }

    query.bindValue(":group", paramGroup + "%");

    if ( ! query.exec() )
        qWarning() << "Cannot execute removeParamGroup statement for" << paramGroup;

    return;
}

QStringList LogParam::getKeys(const QString &group)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << group;

    PARAMMUTEXLOCKER;

    QSet<QString> keys; // unique values;
    QSqlQuery query;

    if ( ! query.prepare("SELECT name FROM log_param WHERE name LIKE :group ") )
    {
        qWarning()<< "Cannot prepare select parameter statement for" << group << query.lastError().text();
        return QStringList();
    }

    query.bindValue(":group", group + "%");

    if ( ! query.exec() )
    {
        qWarning() << "Cannot execute removeParamGroup statement for" << group;
        return QStringList();
    }

    while ( query.next() )
    {
        const QString &param = query.value(0).toString();
        const QString &subKey = param.mid(group.length()).section("/", 0, 0);
        if ( !subKey.isEmpty() ) keys.insert(subKey);
    }
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    return QStringList(keys.begin(), keys.end());
#else
    return keys.toList();
#endif
}

QString LogParam::escapeString(const QString &input, QChar escapeChar, QChar delimiter)
{
    QString result;
    for (QChar ch : input)
    {
        if (ch == escapeChar || ch == delimiter) result += escapeChar;
        result += ch;
    }
    return result;
}

QString LogParam::unescapeString(const QString &input, QChar escapeChar)
{
    QString result;
    bool escaping = false;
    for ( QChar ch : input )
    {
        if ( escaping )
        {
            result += ch;
            escaping = false;
        }
        else if ( ch == escapeChar )
            escaping = true;
        else
            result += ch;
    }
    return result;
}

QString LogParam::serializeStringList(const QStringList &list, QChar delimiter, QChar escapeChar)
{
    QStringList escapedList;
    for ( const QString &item : list )
        escapedList << escapeString(item, escapeChar, delimiter);
    return escapedList.join(delimiter);
}

QStringList LogParam::deserializeStringList(const QString &input, QChar delimiter, QChar escapeChar)
{
    QStringList result;
    QString current;
    bool escaping = false;

    if ( !input.isEmpty() )
    {
        for ( QChar ch : input )
        {
            if ( escaping )
            {
                current += ch;
                escaping = false;
            }
            else if ( ch == escapeChar )
                escaping = true;
            else if ( ch == delimiter )
            {
                result << current;
                current.clear();
            }
            else
                current += ch;
        }
        result << current;
    }
    return result;
}

QCache<QString, QVariant> LogParam::localCache(300);

QMutex LogParam::cacheMutex;

#undef PARAMMUTEXLOCKER
