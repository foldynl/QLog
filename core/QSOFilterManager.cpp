#include <QSqlError>
#include <QSqlRecord>
#include "QSOFilterManager.h"
#include "QSOFilterDateRange.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.qsofiltermanager");

QSOFilterManager::QSOFilterManager(QObject *parent)
    : QObject(parent),
    stmtsReady(true)
{
    FCT_IDENTIFICATION;

    if ( !insertRuleStmt.prepare(QLatin1String("INSERT INTO qso_filter_rules(filter_name, table_field_index, operator_id, value) "
                                              "VALUES (:filterName, :tableFieldIndex, :operatorID, :valueString)")  ) )
    {
        qWarning() << "cannot preapre insert insertRuleStmt";
        stmtsReady = false;
    }

    if ( !insertFilterStmt.prepare(QLatin1String("INSERT INTO qso_filters (filter_name, matching_type) VALUES (:filterName, :matchingType) "
                                                "ON CONFLICT(filter_name) DO UPDATE SET matching_type = :matchingType WHERE filter_name = :filterName") ) )
    {
        qWarning() << "cannot preapre insert insertFilterStmt";
        stmtsReady = false;
    }

    if ( !deleteFilterStmt.prepare(QLatin1String("DELETE FROM qso_filter_rules WHERE filter_name = :filterName") ) )
    {
        qWarning() << "cannot preapre insert deleteFilterStmt";
        stmtsReady = false;
    }
}

bool QSOFilterManager::deleteFilterRules(const QString &filterName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filterName;

    deleteFilterStmt.bindValue(":filterName", filterName);
    bool ret = deleteFilterStmt.exec();
    if ( !ret )
        qCDebug(runtime) << "SQL Error"
                         << deleteFilterStmt.lastError().text();
    return ret;
}

bool QSOFilterManager::replaceFilter(const QString &filterName, const int matchingType)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filterName << matchingType;

    insertFilterStmt.bindValue(":filterName", filterName);
    insertFilterStmt.bindValue(":matchingType", matchingType);

    bool ret = insertFilterStmt.exec();
    if ( !ret )
        qCDebug(runtime) << "SQL Error"
                         << insertFilterStmt.lastError().text();
    return ret;
}

bool QSOFilterManager::insertFilterRule(const QString & filterName,
                                        const QSOFilterRule &rule)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filterName
                                 << rule.tableFieldIndex
                                 << rule.operatorID
                                 << rule.value;

    insertRuleStmt.bindValue(":filterName", filterName);
    insertRuleStmt.bindValue(":tableFieldIndex", rule.tableFieldIndex);
    insertRuleStmt.bindValue(":operatorID", rule.operatorID);
    insertRuleStmt.bindValue(":valueString", (rule.value.isNull()) ? QVariant()
                                                                    : rule.value);
    bool ret = insertRuleStmt.exec();
    if ( !ret )
        qCDebug(runtime) << "SQL Error"
                         << insertRuleStmt.lastError().text();
    return ret;
}

bool QSOFilterManager::save(const QSOFilter &filter)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filter.filterName
                                 << filter.machingType;

    if ( !stmtsReady )
        return false;

    QSqlDatabase::database().transaction();

    if ( !replaceFilter(filter.filterName, filter.machingType) )
    {
        QSqlDatabase::database().rollback();
        return false;
    }

    if ( !deleteFilterRules(filter.filterName) )
    {
        QSqlDatabase::database().rollback();
        return false;
    }

    for ( const QSOFilterRule &rule : filter.rules )
    {
        if ( !insertFilterRule(filter.filterName, rule) )
        {
            QSqlDatabase::database().rollback();
            return false;
        }
    }

    QSqlDatabase::database().commit();
    return true;
}

bool QSOFilterManager::remove(const QString &filterName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filterName;

    QSqlQuery filterStmt;
    if ( ! filterStmt.prepare(QLatin1String("DELETE FROM qso_filters "
                                            "WHERE filter_name = :filterName;")) )
    {
        qWarning() << "Cannot prepare delete statement";
        return false;
    }

    filterStmt.bindValue(":filterName", filterName);

    if ( ! filterStmt.exec() )
    {
        qInfo()<< "Cannot get filters names from DB" << filterStmt.lastError();
        return false;
    }

    return true;
}

QStringList QSOFilterManager::getFilterList() const
{
    FCT_IDENTIFICATION;

    QStringList ret;

    QSqlQuery filterStmt;
    if ( ! filterStmt.prepare(QLatin1String("SELECT filter_name "
                                            "FROM qso_filters "
                                            "ORDER BY filter_name")) )
    {
        qWarning() << "Cannot prepare select statement";
        return ret;
    }

    if ( filterStmt.exec() )
    {
        while ( filterStmt.next() )
            ret << filterStmt.value(0).toString();
    }
    else
        qInfo()<< "Cannot get filters names from DB" << filterStmt.lastError();;

    return ret;
}

QSOFilter QSOFilterManager::getFilter(const QString &filterName) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filterName;

    QSOFilter ret;
    QSqlQuery query;
    if ( ! query.prepare(QLatin1String("SELECT matching_type, table_field_index, operator_id, value "
                                       "FROM qso_filters f LEFT JOIN qso_filter_rules r ON f.filter_name = r.filter_name "
                                       "WHERE f.filter_name = :filter "
                                       "ORDER BY r.rowid")) )
    {
        qWarning() << "Cannot prepare select statement";
        return ret;
    }

    query.bindValue(":filter", filterName);

    if ( query.exec() )
    {
        ret.filterName = filterName;
        while ( query.next() )
        {
            QSOFilterRule rule;
            const QSqlRecord &record = query.record();

            ret.machingType = record.value("matching_type").toInt();
            if ( record.value("table_field_index").isNull() ) continue;
            rule.tableFieldIndex = record.value("table_field_index").toInt();
            rule.operatorID = record.value("operator_id").toInt();
            rule.value = record.value("value").toString();
            ret.addRule(rule);
        }
    }
    else
        qCDebug(runtime) << "SQL execution error: " << query.lastError().text();

    return ret;
}

QString QSOFilterManager::getWhereClause(const QString &filterName, const QString &columnPrefix)
{
    return getWhereClause(instance()->getFilter(filterName), columnPrefix);
}

namespace
{
QString valueCondition(const QSOFilterRule &rule, const QString &field, QSqlQuery &quoteQuery)
{
    QString value = rule.value;
    QString sqlOperator;
    switch ( rule.operatorID )
    {
    case QSOFilterRule::Equal:       sqlOperator = "="; break;
    case QSOFilterRule::NotEqual:    sqlOperator = "<>"; break;
    case QSOFilterRule::GreaterThan: sqlOperator = ">"; break;
    case QSOFilterRule::LessThan:    sqlOperator = "<"; break;
    case QSOFilterRule::Contains:
    case QSOFilterRule::NotContains:
        sqlOperator = rule.operatorID == QSOFilterRule::Contains ? "like" : "not like";
        if ( !value.isNull() ) value = '%' + value + '%';
        break;
    case QSOFilterRule::StartsWith:
        sqlOperator = "like";
        if ( !value.isNull() ) value += '%';
        break;
    case QSOFilterRule::RegExp:
        sqlOperator = "regexp";
        if ( !value.isNull() ) value.prepend("(?i)");
        break;
    default:
        return {};
    }

    // Keep the legacy NULL behavior, including Contains vs. StartsWith.
    if ( value.isNull() )
        sqlOperator = (rule.operatorID == QSOFilterRule::Equal || rule.operatorID == QSOFilterRule::Contains)
                       ? "IS" : "IS NOT";

    // Let SQLite quote the literal; do not duplicate escaping or NULL handling.
    quoteQuery.bindValue(0, value.isNull() ? QVariant() : QVariant(value));
    if ( !quoteQuery.exec() || !quoteQuery.next() ) return {};
    return QString(" %1 COLLATE NOCASE %2 (%3) ").arg(field, sqlOperator, quoteQuery.value(0).toString());
}

QString dateRangeCondition(const QSOFilterRule &rule, QString field, const QDate &today)
{
    QSOFilterDateRange range;
    QDateTime start, end;

    if ( !QSOFilterDateRange::fromString(rule.value, range) || !range.resolve(today, start, end) )
        return {};

    QString lower, upper;
    if ( start.time() == QTime(0, 0) && end.time() == QTime(0, 0) )
    {
        // Whole days work for ISO dates and UTC timestamps and retain index use.
        lower = "'" + start.date().toString(Qt::ISODate) + "'";
        upper = "'" + end.date().toString(Qt::ISODate) + "'";
    }
    else
    {
        // Custom times compare equally with and without an ISO zone.
        field = "julianday(" + field + ")";
        lower = "julianday('" + start.toString(Qt::ISODate) + "')";
        upper = "julianday('" + end.toString(Qt::ISODate) + "')";
    }

    switch ( rule.operatorID )
    {
    case QSOFilterRule::InDateRange:
        return QString(" (%1 >= %2 AND %1 < %3) ").arg(field, lower, upper);
    case QSOFilterRule::OutsideDateRange:
        return QString(" (%1 < %2 OR %1 >= %3) ").arg(field, lower, upper);
    case QSOFilterRule::BeforeDateRange:
        return QString(" %1 < %2 ").arg(field, lower);
    case QSOFilterRule::AfterDateRange:
        return QString(" %1 >= %2 ").arg(field, upper);
    default:
        return {};
    }
}
}

QString QSOFilterManager::getWhereClause(const QSOFilter &filter, const QString &columnPrefix,
                                       const QDate &today)
{
    FCT_IDENTIFICATION;

    const QSqlRecord columns = QSqlDatabase::database().record("contacts");
    const QString prefix = columnPrefix.isEmpty() ? QString() : columnPrefix + '.';
    QSqlQuery quoteQuery;

    if ( !quoteQuery.prepare("SELECT quote(?)") ) return QStringLiteral("(0)");

    QStringList conditions;

    for ( const QSOFilterRule &rule : filter.rules )
    {
        if ( rule.tableFieldIndex < 0 || rule.tableFieldIndex >= columns.count() )
            return QStringLiteral("(0)");
        const QString field = prefix + columns.fieldName(rule.tableFieldIndex);
        const QString condition = rule.isDateRange() ? dateRangeCondition(rule, field, today)
                                                    : valueCondition(rule, field, quoteQuery);
        // A malformed rule must not broaden an OR filter.
        if ( condition.isEmpty() ) return QStringLiteral("(0)");
        conditions.append(condition);
    }

    if ( conditions.isEmpty() ) return QStringLiteral("(  )"); // Existing empty-filter behavior.
    if ( filter.machingType != QSOFilter::All && filter.machingType != QSOFilter::Any )
        return QStringLiteral("(0)");
    return QString("( (%1) )").arg(conditions.join(filter.machingType == QSOFilter::All ? "AND" : "OR"));
}

SqlListModel *QSOFilterManager::QSOFilterModel(const QString &firstValue, QObject *parent)
{
    FCT_IDENTIFICATION;

    return new SqlListModel("SELECT filter_name "
                            "FROM qso_filters "
                            "ORDER BY filter_name COLLATE LOCALEAWARE ASC",
                            firstValue,
                            parent);
}
