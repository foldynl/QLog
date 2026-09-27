#include "core/QSLLabelBuilder.h"

#include <algorithm>
#include <utility>

#include "core/debug.h"
#include "data/Callsign.h"
#include "data/Data.h"

MODULE_IDENTIFICATION("qlog.core.qsllabelbuilder");

void QSLLabelBuilder::setSourceQsos(QVector<QSLSourceQso> qsos)
{
    FCT_IDENTIFICATION;

    sourceQsos = std::move(qsos);
    formattedDateFormat.clear();
}

void QSLLabelBuilder::clear()
{
    FCT_IDENTIFICATION;

    sourceQsos.clear();
    formattedDateFormat.clear();
}

QList<QSLLabelData> QSLLabelBuilder::build(const QSLLabelBuildOptions &options)
{
    FCT_IDENTIFICATION;

    QList<QSLLabelData> labels;
    QString dateFormat = options.dateFormat.trimmed();

    if ( dateFormat.isEmpty() )
        dateFormat = QStringLiteral("yyyy-MM-dd");

    if ( formattedDateFormat != dateFormat )
    {
        for ( QSLSourceQso &source : sourceQsos )
            source.row.date = source.startTime.toString(dateFormat);

        formattedDateFormat = dateFormat;
    }

    const int maxRows = qMax(1, options.maxRows);
    const bool orderByRoute = options.order == QSLLabelOrder::QslRoute;
    const bool routeAware = orderByRoute || options.showRoutePrefix;
    const bool needsRouting = routeAware || options.showQslManager;

    struct LabelGroup
    {
        QSLLabelData label;
        QString routeCallsign;
        QList<QSLLabelData::QsoRow> rows;
    };

    QString currentCallsign;
    QList<QSLLabelData::QsoRow> currentRows;

    const auto appendRows = [&labels, maxRows](const QString &callsign,
                                               QList<QSLLabelData::QsoRow> &rows)
    {
        while ( !rows.isEmpty() )
        {
            QSLLabelData label;
            label.callsign = callsign;
            label.qsos = rows.mid(0, maxRows);
            rows.erase(rows.begin(), rows.begin() + label.qsos.size());
            labels.append(label);
        }
    };

    QList<LabelGroup> groups;

    const auto appendGroups = [&groups, &labels, maxRows]()
    {
        for ( const LabelGroup &group : groups )
        {
            for ( int offset = 0; offset < group.rows.size(); offset += maxRows )
            {
                QSLLabelData label = group.label;
                label.qsos = group.rows.mid(offset, maxRows);
                labels.append(label);
            }
        }

        groups.clear();
    };

    for ( const QSLSourceQso &source : static_cast<const QVector<QSLSourceQso>&>(sourceQsos) )
    {
        if ( !needsRouting )
        {
            if ( source.callsign != currentCallsign )
            {
                appendRows(currentCallsign, currentRows);
                currentCallsign = source.callsign;
            }

            currentRows.append(source.row);

            if ( currentRows.size() == maxRows )
                appendRows(currentCallsign, currentRows);

            continue;
        }

        if ( !groups.isEmpty() && source.callsign != currentCallsign )
            appendGroups();

        currentCallsign = source.callsign;

        Route route{source.callsign, QString(), QString()};
        if ( routeAware )
            route = resolveRoute(source);
        else
            route.qslManager = qslManager(source.qslVia);

        int groupIndex = -1;
        for ( int i = 0; i < groups.size(); ++i )
        {
            const LabelGroup &group = groups.at(i);
            const bool sameRoute = !routeAware
                                   || ( group.routeCallsign == route.callsign
                                        && group.label.routePrefix == route.prefix );
            const bool sameManager = !options.showQslManager
                                     || group.label.qslManager == route.qslManager;

            if ( sameRoute && sameManager )
            {
                groupIndex = i;
                break;
            }
        }

        if ( groupIndex < 0 )
        {
            LabelGroup group;
            group.label.callsign = source.callsign;
            group.label.qslManager = route.qslManager;
            group.label.routePrefix = route.prefix;
            group.routeCallsign = route.callsign;
            groups.append(group);
            groupIndex = groups.size() - 1;
        }

        groups[groupIndex].rows.append(source.row);
    }

    if ( needsRouting )
        appendGroups();
    else
        appendRows(currentCallsign, currentRows);

    if ( orderByRoute )
    {
        std::stable_sort(labels.begin(), labels.end(),
                         [](const QSLLabelData &left, const QSLLabelData &right)
        {
            const bool leftResolved = !left.routePrefix.isEmpty();
            const bool rightResolved = !right.routePrefix.isEmpty();

            if ( leftResolved != rightResolved )
                return leftResolved;

            int comparison = QString::compare(left.routePrefix,
                                              right.routePrefix,
                                              Qt::CaseInsensitive);
            if ( comparison != 0 )
                return comparison < 0;

            const QString &leftRoute = left.qslManager.isEmpty() ? left.callsign : left.qslManager;
            const QString &rightRoute = right.qslManager.isEmpty() ? right.callsign : right.qslManager;
            comparison = QString::compare(leftRoute, rightRoute, Qt::CaseInsensitive);
            if ( comparison != 0 )
                return comparison < 0;

            return QString::compare(left.callsign,
                                    right.callsign,
                                    Qt::CaseInsensitive) < 0;
        });
    }

    return labels;
}

QSLLabelBuilder::Route QSLLabelBuilder::resolveRoute(const QSLSourceQso &source)
{
    FCT_IDENTIFICATION;

    const QString managerCallsign = qslManager(source.qslVia);

    if ( !managerCallsign.isEmpty() )
        return {managerCallsign, callsignPrefix(managerCallsign), managerCallsign};

    Route route{source.callsign, dxccPrefix(source.dxcc), QString()};

    if ( route.prefix.isEmpty() )
        route.prefix = source.prefix;

    if ( route.prefix.isEmpty() )
        route.prefix = callsignPrefix(source.callsign);

    return route;
}

QString QSLLabelBuilder::qslManager(const QString &qslVia)
{
    FCT_IDENTIFICATION;

    if ( qslVia.isEmpty() )
        return QString();

    const auto cached = qslManagerCache.constFind(qslVia);
    if ( cached != qslManagerCache.cend() )
        return cached.value();

    const Callsign parsedCallsign(qslVia);
    const QString parsedValue = parsedCallsign.getHostPrefixWithDelimiter()
                                + parsedCallsign.getBase()
                                + parsedCallsign.getSuffixWithDelimiter();
    const QString managerCallsign = parsedCallsign.isValid() ? parsedValue : QString();

    qslManagerCache.insert(qslVia, managerCallsign);
    return managerCallsign;
}

QString QSLLabelBuilder::callsignPrefix(const QString &callsign)
{
    FCT_IDENTIFICATION;

    const auto cached = callsignPrefixCache.constFind(callsign);
    if ( cached != callsignPrefixCache.cend() )
        return cached.value();

    const QString prefix = Data::instance()->lookupDxcc(callsign).prefix;
    callsignPrefixCache.insert(callsign, prefix);
    return prefix;
}

QString QSLLabelBuilder::dxccPrefix(int dxcc)
{
    FCT_IDENTIFICATION;

    if ( dxcc <= 0 )
        return QString();

    const auto cached = dxccPrefixCache.constFind(dxcc);
    if ( cached != dxccPrefixCache.cend() )
        return cached.value();

    const QString prefix = Data::instance()->lookupDxccID(dxcc).prefix;
    dxccPrefixCache.insert(dxcc, prefix);
    return prefix;
}
