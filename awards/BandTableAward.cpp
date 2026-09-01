#include <QHeaderView>
#include "BandTableAward.h"
#include "core/debug.h"
#include "data/Band.h"
#include "data/BandPlan.h"

MODULE_IDENTIFICATION("qlog.awards.bandtableaward");

QWidget* BandTableAward::createWidget(QWidget *parent)
{
    FCT_IDENTIFICATION;

    m_tableView = new QTableView(parent);
    m_model = new AwardsTableModel(m_tableView);

    m_tableView->setFocusPolicy(Qt::ClickFocus);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tableView->horizontalHeader()->setVisible(true);
    m_tableView->horizontalHeader()->setCascadingSectionResizes(true);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->verticalHeader()->setMinimumSectionSize(20);
    m_tableView->verticalHeader()->setDefaultSectionSize(20);
    m_tableView->verticalHeader()->setHighlightSections(false);

    m_widget = m_tableView;
    return m_widget;
}

void BandTableAward::updateData(const AwardFilterParams &params)
{
    FCT_IDENTIFICATION;

    const QList<Band>& dxccBands = BandPlan::bandsList(false, true);

    if ( dxccBands.size() == 0 )
        return;

    m_currentEntity = params.entitySelected;

    QStringList confirmedConditions = params.confirmedConditions;
    if ( !acceptsEqslConfirmation() )
    {
        for ( int i = confirmedConditions.size() - 1; i >= 0; --i )
        {
            if ( confirmedConditions.at(i).contains(QLatin1String("eqsl_qsl_rcvd"),
                                                     Qt::CaseInsensitive) )
                confirmedConditions.removeAt(i);
        }
    }

    const QString innerConfirmedCase(QString(" CASE WHEN (%1) THEN 2 ELSE 1 END ")
                                         .arg(confirmedConditions.isEmpty()
                                                  ? QStringLiteral("1=2")
                                                  : confirmedConditions.join(" or ")));

    QStringList stmt_max_part;
    QStringList stmt_total_padding;
    QStringList stmt_sum_confirmed;
    QStringList stmt_sum_worked;
    QStringList stmt_sum_total;
    QStringList stmt_having;
    QStringList stmt_total_band_condition_work;
    QStringList stmt_total_band_condition_confirmed;
    QStringList stmt_not_confirmed;
    QStringList stmt_any_worked;
    const bool independentSatellite = hasIndependentSatelliteCategory();
    const bool independentEME = hasIndependentEMECategory();
    m_currentIndependentEME = independentEME;
    m_currentModeFilter = QString("mode IN (SELECT name FROM modes WHERE dxcc IN (%1))")
                              .arg(params.modes.join(","));
    m_currentUserFilter = params.userFilterWhereClause.trimmed();
    if ( m_currentUserFilter.startsWith(QLatin1String("AND "), Qt::CaseInsensitive) )
        m_currentUserFilter.remove(0, 4);
    const QString regularPropagationCondition = independentSatellite
                                                ? QStringLiteral(" AND UPPER(COALESCE(prop_mode, '')) <> 'SAT'")
                                                : QString();

    for ( const Band& band : dxccBands )
    {
        stmt_max_part << QString(" MAX(CASE WHEN band = '%1'%2 AND m.dxcc IN (%3) THEN %4 ELSE 0 END) as '%5'")
                             .arg(band.name, regularPropagationCondition, params.modes.join(","), innerConfirmedCase, band.name);
        stmt_total_padding << QString(" NULL '%1'").arg(band.name);
        stmt_sum_confirmed << QString("SUM(CASE WHEN a.'%1' > 1 THEN 1 ELSE 0 END) '%2'").arg(band.name, band.name);
        stmt_sum_worked << QString("SUM(CASE WHEN a.'%1' > 0 THEN 1 ELSE 0 END) '%2'").arg(band.name, band.name);
        stmt_sum_total << QString("SUM(d.'%1') '%2'").arg(band.name, band.name);
        stmt_having << QString("SUM(d.'%1') = 0").arg(band.name);
        stmt_total_band_condition_work << QString("e.'%1' > 0").arg(band.name);
        stmt_total_band_condition_confirmed << QString("e.'%1' > 1").arg(band.name);
        stmt_not_confirmed << QString("MAX(d.'%1') < 2").arg(band.name);
        stmt_any_worked << QString("MAX(d.'%1') > 0").arg(band.name);
    }

    const QString satelliteModeCondition = independentSatellite
                                           ? QString()
                                           : QString(" AND m.dxcc IN (%1)").arg(params.modes.join(","));
    const QString emeModeCondition = independentEME
                                     ? QString()
                                     : QString(" AND m.dxcc IN (%1)").arg(params.modes.join(","));
    stmt_max_part << QString(" MAX(CASE WHEN UPPER(prop_mode) = 'SAT'%1 THEN %2 ELSE 0 END) as 'SAT' ").arg(satelliteModeCondition, innerConfirmedCase)
                  << QString(" MAX(CASE WHEN UPPER(prop_mode) = 'EME'%1 THEN %2 ELSE 0 END) as 'EME' ").arg(emeModeCondition, innerConfirmedCase);
    stmt_total_padding << " NULL 'SAT' "
                       << " NULL 'EME' ";
    stmt_sum_confirmed << " SUM(CASE WHEN a.'SAT' > 1 THEN 1 ELSE 0 END) 'SAT' "
                       << " SUM(CASE WHEN a.'EME' > 1 THEN 1 ELSE 0 END) 'EME' ";
    stmt_sum_worked << " SUM(CASE WHEN a.'SAT' > 0 THEN 1 ELSE 0 END) 'SAT' "
                    << " SUM(CASE WHEN a.'EME' > 0 THEN 1 ELSE 0 END) 'EME' ";
    stmt_sum_total << " SUM(d.'SAT') 'SAT' "
                   << " SUM(d.'EME') 'EME' ";
    if ( !independentSatellite )
    {
        stmt_having << " SUM(d.'SAT') = 0";
        stmt_total_band_condition_work << "e.'SAT' > 0";
        stmt_total_band_condition_confirmed << "e.'SAT' > 1";
        stmt_not_confirmed << " MAX(d.'SAT') < 2";
        stmt_any_worked << " MAX(d.'SAT') > 0";
    }
    stmt_having << " SUM(d.'EME') = 0";
    stmt_total_band_condition_work << "e.'EME' > 0";
    stmt_total_band_condition_confirmed << "e.'EME' > 1";
    stmt_not_confirmed << " MAX(d.'EME') < 2";
    stmt_any_worked << " MAX(d.'EME') > 0";

    const QString &entity = params.entitySelected;

    QString sourceContactsTable = sourceContactsOverride(params.userFilterWhereClause);
    if ( sourceContactsTable.isEmpty() )
    {
        sourceContactsTable = QString(" source_contacts AS "
                                      " (SELECT * "
                                      "  FROM contacts "
                                      "  WHERE 1=1 %1 ) ").arg(params.userFilterWhereClause);
    }

    QStringList addlCTEs = additionalCTEs(entity, params.userFilterWhereClause);
    addlCTEs.append(sourceContactsTable);

    QStringList havingConditions;
    if ( params.notWorkedOnly )
        havingConditions << QString("(%1)").arg(stmt_having.join(" AND "));
    if ( params.notConfirmedOnly )
        havingConditions << QString("(%1) AND (%2)").arg(stmt_not_confirmed.join(" AND "),
                                                         stmt_any_worked.join(" OR "));

    QString havingClause;
    if ( !havingConditions.isEmpty() )
        havingClause = QString("HAVING %1").arg(havingConditions.join(" OR "));

    QString finalSQL(QString(
              "WITH "
              "   %1, "
              "   detail_table AS ( "
              "     SELECT %2, %3 "
              "     %4"
              "     WHERE 1=1 %5"
              "     GROUP BY  1,2), "
              "   unique_worked AS ("
              "     SELECT DISTINCT col1"
              "     FROM detail_table e"
              "      WHERE %6), "
              "   unique_confirmed AS ("
              "     SELECT DISTINCT col1"
              "     FROM detail_table e"
              "      WHERE %7) "
              "SELECT * FROM ( "
              "     SELECT 0 column_idx, '%8', COUNT(*), %9"
              "     FROM unique_worked"
              "   UNION ALL "
              "     SELECT 0 column_idx, '%10', COUNT(*), %11"
              "     FROM unique_confirmed"
              "   UNION ALL "
              "     SELECT 1 column_idx, '%12', NULL prefix, %13"
              "     FROM detail_table a "
              "     GROUP BY 1 "
              "   UNION ALL "
              "     SELECT 2 column_idx, '%14', NULL prefix, %15"
              "     FROM detail_table a "
              "     GROUP BY 1 "
              "   UNION ALL "
              "     SELECT 3 column_idx, col1, col2, %16"
              "     FROM detail_table d "
              "     GROUP BY 2,3 "
              "     %17"
              ") "
              "ORDER BY 1,2 COLLATE LOCALEAWARE ASC ").arg(addlCTEs.join(","), // 1
                                                           headersColumns(entity), // 2
                                                           stmt_max_part.join(","), // 3
                                                           sqlDetailTable(entity), // 4
                                                           additionalWhere(entity), // 5
                                                           stmt_total_band_condition_work.join(" OR "), // 6
                                                           stmt_total_band_condition_confirmed.join(" OR "), // 7
                                                           QObject::tr("TOTAL Worked"), // 8
                                                           stmt_total_padding.join(",") // 9
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
                                                           ,
#else
                                                           ).arg(
#endif
                                                           QObject::tr("TOTAL Confirmed"), // 10
                                                           stmt_total_padding.join(","), // 11
                                                           QObject::tr("Confirmed"), // 12
                                                           stmt_sum_confirmed.join(",") // 13
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
                         ,
#else
                                                           ).arg(
#endif
                                                           QObject::tr("Worked")).arg(  // 14
                                                           stmt_sum_worked.join(","), // 15
                                                           stmt_sum_total.join(","), // 16
                                                           havingClause) // 17
                                                           );
    qCDebug(runtime) << finalSQL;

    m_model->setQuery(finalSQL);
    m_model->setHeaderData(1, Qt::Horizontal, "");
    m_model->setHeaderData(2, Qt::Horizontal, "");
    if ( independentSatellite )
    {
        for ( int column = 0; column < m_model->columnCount(); ++column )
        {
            if ( m_model->headerData(column, Qt::Horizontal).toString() == QLatin1String("SAT") )
            {
                m_model->setHeaderData(column,
                                       Qt::Horizontal,
                                       QObject::tr("Satellite DXCC, all modes"),
                                       Qt::ToolTipRole);
                break;
            }
        }
    }
    if ( independentEME )
    {
        for ( int column = 0; column < m_model->columnCount(); ++column )
        {
            if ( m_model->headerData(column, Qt::Horizontal).toString() == QLatin1String("EME") )
            {
                m_model->setHeaderData(column,
                                       Qt::Horizontal,
                                       QObject::tr("EME WAZ, all bands and modes"),
                                       Qt::ToolTipRole);
                break;
            }
        }
    }
    m_tableView->setModel(m_model);
    m_tableView->setColumnHidden(0, true);
}

BandTableAward::ConditionResult BandTableAward::getConditionSelected(const QModelIndex &clickedIndex) const
{
    FCT_IDENTIFICATION;

    ConditionResult result;

    if ( !clickedIndex.isValid() || clickedIndex.row() <= 3 )
        return result;

    const QString col1Value = m_model->data(m_model->index(clickedIndex.row(), 1), Qt::DisplayRole).toString();
    const QString col2Value = m_model->data(m_model->index(clickedIndex.row(), 2), Qt::DisplayRole).toString();

    QStringList addlFilters;

    if ( entityInputEnabled() )
        addlFilters << QString("my_dxcc='%1'").arg(m_currentEntity);

    if ( clickUsesCountryName() )
    {
        result.country = col1Value;
    }
    else
    {
        const QString filter = clickFilter(col1Value, col2Value);
        Q_ASSERT_X(!filter.isEmpty(), "BandTableAward::getConditionSelected",
                    "clickFilter() returned empty string — subclass must override clickFilter() or clickUsesCountryName()");
        addlFilters << filter;
    }

    if ( clickedIndex.column() > 2 )
    {
        const QString column = m_model->headerData(clickedIndex.column(), Qt::Horizontal).toString();
        if ( column == QLatin1String("SAT") || column == QLatin1String("EME") )
        {
            addlFilters << QString("UPPER(prop_mode)='%1'").arg(column);
            if ( column == QLatin1String("EME") )
            {
                if ( !m_currentIndependentEME )
                    addlFilters << m_currentModeFilter;
                if ( !m_currentUserFilter.isEmpty() )
                    addlFilters << m_currentUserFilter;
            }
        }
        else
            result.band = column;
    }

    result.filter = QString("(") + addlFilters.join(" and ") + QString(")");
    result.valid = true;

    return result;
}

QStringList BandTableAward::additionalCTEs(const QString &, const QString &) const
{
    return QStringList();
}

QString BandTableAward::sourceContactsOverride(const QString &) const
{
    return QString();
}

QString BandTableAward::clickFilter(const QString &, const QString &) const
{
    return QString();
}

bool BandTableAward::clickUsesCountryName() const
{
    return false;
}

bool BandTableAward::hasIndependentSatelliteCategory() const
{
    return false;
}

bool BandTableAward::hasIndependentEMECategory() const
{
    return false;
}

QString BandTableAward::generateNumberRangeCTE(const QString &name, int min, int max)
{
    return QString(" %1 AS ("
                   "   SELECT %2 AS n, %2 AS value"
                   "   UNION ALL"
                   "   SELECT n + 1, value + 1"
                   "   FROM %1"
                   "   WHERE n < %3 )").arg(name).arg(min).arg(max);
}

QString BandTableAward::generateValuesCTE(const QString &name,
                                           const QList<QPair<QString,QString>> &values)
{
    QStringList rows;
    for ( const QPair<QString,QString> &pair : values )
        rows << QString("('%1', '%2')").arg(pair.first, pair.second);

    return QString(" %1 AS (VALUES %2)").arg(name, rows.join(","));
}

QString BandTableAward::generateSplitCTE(const QString &sourceColumn,
                                          const QString &outputColumn,
                                          const QString &contactFilter)
{
    return QString(" split(id, callsign, station_callsign, my_dxcc, band, dxcc, eqsl_qsl_rcvd, lotw_qsl_rcvd, qsl_rcvd, prop_mode, mode, %1, str) AS ("
                   "   SELECT id, callsign, station_callsign, my_dxcc, band, "
                   "          dxcc, eqsl_qsl_rcvd, lotw_qsl_rcvd, qsl_rcvd, prop_mode, mode, "
                   "          '', %2||',' "
                   "   FROM contacts WHERE 1=1 %3"
                   "   UNION ALL "
                   "   SELECT id, callsign, station_callsign, my_dxcc, band, "
                   "          dxcc, eqsl_qsl_rcvd, lotw_qsl_rcvd, qsl_rcvd, prop_mode, mode, "
                   "          substr(str, 0, instr(str, ',')), TRIM(substr(str, instr(str, ',') + 1)) "
                   "   FROM split "
                   "   WHERE str != '') ").arg(outputColumn, sourceColumn, contactFilter);
}

QString BandTableAward::generateSplitSourceContacts(const QString &outputColumn)
{
    return QString(" source_contacts AS ("
                   "   SELECT id, callsign, station_callsign, my_dxcc, band, dxcc, eqsl_qsl_rcvd, lotw_qsl_rcvd, qsl_rcvd, prop_mode, mode, %1 "
                   "   FROM split "
                   "   WHERE %1 != '' ) ").arg(outputColumn);
}
