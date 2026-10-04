#include <QCoreApplication>
#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableView>
#include <utility>
#include "AwardPGAHunter.h"
#include "ui_PGAHunterTable.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.awards.awardpgahunter");

QVariant AwardPGAHunter::TableModel::data(const QModelIndex &index, int role) const
{
    FCT_IDENTIFICATION;

    if ( !index.isValid() )
        return QVariant();

    if ( role == Qt::TextAlignmentRole && index.column() >= 3 )
        return int(Qt::AlignCenter);

    if ( role == Qt::FontRole || role == Qt::BackgroundRole )
    {
        const bool detailRow = QSqlQueryModel::data(this->index(index.row(), 0)).toInt() == 2;
        if ( role == Qt::FontRole && !detailRow )
        {
            QFont font;
            font.setBold(true);
            return font;
        }
        if ( role == Qt::BackgroundRole && detailRow && index.column() >= 3
             && !QSqlQueryModel::data(index).toString().isEmpty() )
            return QColor(255, 165, 0);
    }

    return QSqlQueryModel::data(index, role);
}

QString AwardPGAHunter::displayName() const
{
    FCT_IDENTIFICATION;
    return QCoreApplication::translate("AwardsDialog", "PGA Hunter - local progress");
}

QString AwardPGAHunter::rulesUrl() const
{
    FCT_IDENTIFICATION;
    return QStringLiteral("http://www.spga.pl/regulamin.php");
}

QWidget* AwardPGAHunter::createWidget(QWidget *parent)
{
    FCT_IDENTIFICATION;

    QTableView *tableView = new QTableView(parent);
    Ui::PGAHunterTable ui;
    ui.setupUi(tableView);
    model = new TableModel(tableView);
    tableView->setModel(model);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_widget = tableView;
    return m_widget;
}

QString AwardPGAHunter::eligibleContactCondition(const QString &referenceColumn)
{
    FCT_IDENTIFICATION;

    // PGA's HF category includes 160m. Reference changes follow the SPGA list,
    // rather than applying today's administrative boundaries to old QSOs.
    return QStringLiteral("start_time >= '2008-01-01'"
                          " AND band IN ('160m','80m','60m','40m','30m','20m','17m','15m','12m','10m')"
                          " AND UPPER(COALESCE(prop_mode, '')) <> 'RPT'"
                          " AND CASE %1"
                          " WHEN 'BA16' THEN start_time >= '2025-01-01'"
                          " WHEN 'LI13' THEN start_time >= '2025-01-01'"
                          " WHEN 'WB01' THEN start_time >= '2013-01-01'"
                          " WHEN 'AB04' THEN start_time < '2013-01-01'"
                          " WHEN 'DP04' THEN start_time < '2019-01-01'"
                          " WHEN 'ZG10' THEN start_time < '2015-01-01'"
                          " ELSE 1 END").arg(referenceColumn);
}

QString AwardPGAHunter::sqlQuery(bool notWorkedOnly) const
{
    FCT_IDENTIFICATION;

    // Aggregate by reference before joining the directory: names are not unique
    // and a gmina can occur as both a contacted and an operated location.
    return QStringLiteral(
        "WITH source_contacts AS ("
        " SELECT start_time, band, prop_mode, dxcc, cnty, my_dxcc, my_cnty FROM contacts"
        " WHERE (dxcc = 269 OR my_dxcc = 269) AND %1),"
        "refs AS ("
        " SELECT UPPER(TRIM(cnty)) code, 1 worked, 0 own FROM source_contacts"
        " WHERE dxcc = 269 AND %2"
        " UNION ALL"
        " SELECT UPPER(TRIM(my_cnty)) code, 0 worked, 1 own FROM source_contacts"
        " WHERE my_dxcc = 269 AND %3),"
        "counts AS (SELECT code, SUM(worked) worked, SUM(own) own FROM refs GROUP BY code),"
        "detail AS ("
        " SELECT d.code, d.subdivision_name || CASE WHEN d.aux = 'suspended'"
        " THEN :suspended ELSE '' END name,"
        " COALESCE(c.worked, 0) worked, COALESCE(c.own, 0) own"
        " FROM adif_enum_secondary_subdivision d LEFT JOIN counts c ON d.code = c.code"
        " WHERE d.dxcc = 269)"
        " SELECT 0 row_type, :total_worked code, '' name,"
        " COALESCE(SUM(worked > 0), 0) worked, COALESCE(SUM(own > 0), 0) own FROM detail"
        " UNION ALL"
        " SELECT 1, :total_covered, '', COALESCE(SUM(worked > 0 OR own > 0), 0), NULL FROM detail"
        " UNION ALL"
        " SELECT 2, code, name, CASE WHEN worked > 0 THEN :yes ELSE '' END,"
        " CASE WHEN own > 0 THEN :yes ELSE '' END FROM detail %4"
        " ORDER BY row_type, code")
        .arg(currentContactFilter,
             eligibleContactCondition(QStringLiteral("UPPER(TRIM(cnty))")),
             eligibleContactCondition(QStringLiteral("UPPER(TRIM(my_cnty))")),
             notWorkedOnly ? QStringLiteral("WHERE worked = 0 AND own = 0") : QString());
}

void AwardPGAHunter::updateData(const AwardFilterParams &params)
{
    FCT_IDENTIFICATION;

    if ( !model )
        return;

    currentContactFilter = QStringLiteral("mode IN (SELECT name FROM modes WHERE dxcc IN (%1))"
                                          " AND (1=1 %2)")
                               .arg(params.modes.isEmpty() ? QStringLiteral("NULL") : params.modes.join(','),
                                    params.userFilterWhereClause);
    QSqlQuery query;
    if ( !query.prepare(sqlQuery(params.notWorkedOnly)) )
    {
        qCWarning(runtime) << query.lastError();
        model->clear();
        return;
    }
    query.bindValue(QStringLiteral(":total_worked"), QObject::tr("TOTAL Worked"));
    query.bindValue(QStringLiteral(":total_covered"), QObject::tr("TOTAL Local Progress"));
    query.bindValue(QStringLiteral(":yes"), QObject::tr("Yes"));
    query.bindValue(QStringLiteral(":suspended"), QObject::tr(" (Suspended)"));
    if ( !query.exec() )
    {
        qCWarning(runtime) << query.lastError();
        model->clear();
        return;
    }

    model->setQuery(std::move(query));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("Reference"));
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("Gmina"));
    model->setHeaderData(3, Qt::Horizontal, QObject::tr("Worked"));
    model->setHeaderData(4, Qt::Horizontal, QObject::tr("Own operation"));
    if ( m_widget )
        qobject_cast<QTableView*>(m_widget)->setColumnHidden(0, true);
}

AwardDefinition::ConditionResult AwardPGAHunter::getConditionSelected(const QModelIndex &index) const
{
    FCT_IDENTIFICATION;

    ConditionResult result;
    if ( !model || !index.isValid() || index.model() != model
         || model->data(model->index(index.row(), 0)).toInt() != 2 )
        return result;

    const QString code = model->data(model->index(index.row(), 1)).toString();
    // ConditionResult uses the existing logbook SQL-filter interface. Only
    // directory codes are inserted here, with SQL literal escaping.
    QString escapedCode = code;
    escapedCode.replace('\'', QStringLiteral("''"));
    const QString worked = QStringLiteral("(dxcc = 269 AND UPPER(TRIM(cnty)) = '%1')").arg(escapedCode);
    const QString own = QStringLiteral("(my_dxcc = 269 AND UPPER(TRIM(my_cnty)) = '%1')").arg(escapedCode);
    const QString referenceFilter = index.column() == 3 ? worked
                                  : index.column() == 4 ? own
                                  : QStringLiteral("(%1 OR %2)").arg(worked, own);
    result.filter = QStringLiteral("(%1 AND (%2) AND %3)")
                        .arg(currentContactFilter, referenceFilter,
                             eligibleContactCondition(QStringLiteral("'%1'").arg(escapedCode)));
    result.valid = true;
    return result;
}
