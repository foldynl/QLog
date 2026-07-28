#include <QSqlError>
#include <QSqlRecord>
#include "QSOFilterManager.h"
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
    insertRuleStmt.bindValue(":valueString", (rule.value.isEmpty()) ? QVariant()
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
                                       "FROM qso_filter_rules r, qso_filters f "
                                       "WHERE f.filter_name = :filter "
                                       "      AND f.filter_name = r.filter_name")) )
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
    FCT_IDENTIFICATION;

    QSqlQuery userFilterQuery;
    QString ret;
    QString finalColumnPfx = columnPrefix;
    finalColumnPfx += finalColumnPfx.isEmpty() ? "" : ".";

    const QDate today = QDateTime::currentDateTimeUtc().date();
    const QDate tomorrow = today.addDays(1);
    const QDate weekStart = today.addDays(1 - today.dayOfWeek());
    const QDate monthStart(today.year(), today.month(), 1);
    const QDate yearStart(today.year(), 1, 1);

    // Date preset tokens are stored with the rule so saved filters remain relative. Resolve them
    // into half-open UTC date ranges here each time a filter is used.
    const QString fieldExpression = "'" + finalColumnPfx + "' || c.name";
    const QString dateRangeStartExpression =
            "CASE r.value "
            "WHEN '@date-range:today' THEN quote(:today) "
            "WHEN '@date-range:yesterday' THEN quote(:yesterday) "
            "WHEN '@date-range:week-to-date' THEN quote(:weekStart) "
            "WHEN '@date-range:month-to-date' THEN quote(:monthStart) "
            "WHEN '@date-range:last-7-days' THEN quote(:last7Days) "
            "WHEN '@date-range:last-30-days' THEN quote(:last30Days) "
            "WHEN '@date-range:this-year' THEN quote(:yearStart) "
            "END";
    const QString dateRangeEndExpression =
            "CASE r.value "
            "WHEN '@date-range:yesterday' THEN quote(:today) "
            "WHEN '@date-range:this-year' THEN quote(:nextYear) "
            "ELSE quote(:tomorrow) "
            "END";

    if ( ! userFilterQuery.prepare(
                  QString(       "SELECT "
                                 "'(' || GROUP_CONCAT(CASE WHEN r.value LIKE '@date-range:%' THEN CASE "
                                 "WHEN r.operator_id IN (1, 3) THEN ' (' || " + fieldExpression + " || ' < ' || " + dateRangeStartExpression + " || ' OR ' || " + fieldExpression + " || ' >= ' || " + dateRangeEndExpression + " || ') ' "
                                 "WHEN r.operator_id = 4 THEN ' ' || " + fieldExpression + " || ' >= (' || " + dateRangeEndExpression + " || ') ' "
                                 "WHEN r.operator_id = 5 THEN ' ' || " + fieldExpression + " || ' < (' || " + dateRangeStartExpression + " || ') ' "
                                 "ELSE ' (' || " + fieldExpression + " || ' >= ' || " + dateRangeStartExpression + " || ' AND ' || " + fieldExpression + " || ' < ' || " + dateRangeEndExpression + " || ') ' END "
                                 "ELSE ' ' || " + fieldExpression + " || ' ' || CASE WHEN r.value IS NULL AND o.sql_operator IN ('=', 'like') THEN 'IS' "
                                 "                                                  WHEN r.value IS NULL and r.operator_id NOT IN ('=', 'like') THEN 'IS NOT' "
                                 "                                                  WHEN o.sql_operator = ('starts with') THEN 'like' "
                                 "                                                  ELSE o.sql_operator END || "
                                 "' (' || quote(CASE o.sql_operator WHEN 'like' THEN '%' || r.value || '%' "
                                 "                                  WHEN 'not like' THEN '%' || r.value || '%' "
                                 "                                  WHEN 'starts with' THEN r.value || '%' "
                                 "                                  ELSE r.value END)  || ') ' END, m.sql_operator) || ')' "
                                 "FROM qso_filters f, qso_filter_rules r, "
                                 "qso_filter_operators o, qso_filter_matching_types m, "
                                 "PRAGMA_TABLE_INFO('contacts') c "
                                 "WHERE f.filter_name = :filterName "
                                 "      AND f.filter_name = r.filter_name "
                                 "      AND o.operator_id = r.operator_id "
                                 "      AND m.matching_id = f.matching_type "
                                 "      AND c.cid = r.table_field_index")) )
    {
        qWarning() << "Cannot prepare select statement";
        return ret;
    }

    userFilterQuery.bindValue(":filterName", filterName);
    userFilterQuery.bindValue(":today", today.toString(Qt::ISODate));
    userFilterQuery.bindValue(":tomorrow", tomorrow.toString(Qt::ISODate));
    userFilterQuery.bindValue(":yesterday", today.addDays(-1).toString(Qt::ISODate));
    userFilterQuery.bindValue(":weekStart", weekStart.toString(Qt::ISODate));
    userFilterQuery.bindValue(":monthStart", monthStart.toString(Qt::ISODate));
    userFilterQuery.bindValue(":last7Days", today.addDays(-6).toString(Qt::ISODate));
    userFilterQuery.bindValue(":last30Days", today.addDays(-29).toString(Qt::ISODate));
    userFilterQuery.bindValue(":yearStart", yearStart.toString(Qt::ISODate));
    userFilterQuery.bindValue(":nextYear", yearStart.addYears(1).toString(Qt::ISODate));

    qCDebug(runtime) << "User filter SQL: " << userFilterQuery.lastQuery();

    if ( userFilterQuery.exec() )
    {
        userFilterQuery.next();
        ret = QString("( %1 )").arg(userFilterQuery.value(0).toString());
    }
    else
        qCDebug(runtime) << "User filter error - " << userFilterQuery.lastError().text();

    // Manual conditions, when used with fields that contain time, only work by luck.
    // These fields are Timeon/Timeoff. They are stored by QSO Filter Dialog as values in the format
    // YYYY-MM-DDThh:mm:ss without a timezone. This is fine, since all times in QLog
    // are internally in UTC. The problem arises because this WHERE clause is later
    // used in a SELECT in the logbook. SQL does not compare the values as datetime,
    // but as strings — otherwise both sides would need to be proper datetime types.
    // Fortunately, both sides are strings in the same format, except that Timeon/Timeoff
    // includes a timezone at the end. Therefore, string comparison of the dates still works.
    // Preset ranges intentionally use ISO dates without a time component; their half-open
    // comparisons work for both date-only fields and ISO date-time fields.
    return ret;
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
