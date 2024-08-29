#include <QSqlQuery>
#include <QCache>

#include "LogParam.h"
#include "debug.h"

MODULE_IDENTIFICATION("qlog.core.logparam");

LogParam::LogParam(QObject *parent) :
    QObject(parent)
{
    FCT_IDENTIFICATION;
}

bool LogParam::setParam(const QString &name, const QString &value)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << name << " " << value;

    QSqlQuery query;

    if ( ! query.prepare("INSERT OR REPLACE INTO log_param (name, value) "
                         "VALUES (:nam, :val)") )
    {
        qWarning()<< "Cannot prepare insert parameter statement";
        return false;
    }

    query.bindValue(":nam", name);
    query.bindValue(":val", value);

    if ( !query.exec() )
    {
        qWarning() << "Cannot exec an insert parameter statement";
        return false;
    }

    localCache.remove(name);

    return true;
}

bool LogParam::setParam(const QString &name, const QDate &value)
{
    FCT_IDENTIFICATION;

    return setParam(name, value.toString(Qt::ISODate));
}

QVariant LogParam::getParam(const QString &name)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << name;

    QVariant ret;
    QVariant *valueCached = localCache.object(name);

    if ( valueCached )
    {
        ret = *valueCached;
    }
    else
    {
        QSqlQuery query;

        if ( ! query.prepare("SELECT value "
                             "FROM log_param "
                             "WHERE name = :nam") )
        {
            qWarning()<< "Cannot prepare insert parameter statement";
            return QString();
        }

        query.bindValue(":nam", name);

        if ( ! query.exec() )
        {
            qWarning() << "Cannot execute GetParam Select";
            return QString();
        }

        if ( query.first() )
        {
            ret = query.value(0);
            localCache.insert(name, new QVariant(ret));
        }
    }
    qDebug(runtime) << "value: " << ret;
    return ret;
}

QCache<QString, QVariant> LogParam::localCache(10);
