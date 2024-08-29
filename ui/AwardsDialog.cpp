#include <QPushButton>
#include <QSqlQuery>
#include "AwardsDialog.h"
#include "ui_AwardsDialog.h"
#include "models/SqlListModel.h"
#include "core/debug.h"
#include "data/Band.h"
#include "data/Data.h"
#include "data/BandPlan.h"
#include <QSqlError>
MODULE_IDENTIFICATION("qlog.ui.awardsdialog");

AwardsDialog::AwardsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AwardsDialog),
    detailedViewModel(new AwardsTableModel(this))
{
    FCT_IDENTIFICATION;
    ui->setupUi(this);

    entityCallsignModel = new SqlListModel("SELECT my_dxcc, my_country_intl || ' (' || CASE WHEN LENGTH(GROUP_CONCAT(station_callsign, ', ')) > 50 "
                                           "THEN SUBSTR(GROUP_CONCAT(station_callsign, ', '), 0, 50) || '...' ELSE GROUP_CONCAT(station_callsign, ', ') END || ')' "
                                           "FROM(SELECT DISTINCT my_dxcc, my_country_intl, station_callsign FROM contacts) GROUP BY my_dxcc ORDER BY my_dxcc;", "", this);

    ui->myEntityComboBox->blockSignals(true);
    while (entityCallsignModel->canFetchMore())
    {
        entityCallsignModel->fetchMore();
    }
    ui->myEntityComboBox->setModel(entityCallsignModel);
    ui->myEntityComboBox->setModelColumn(1);
    ui->myEntityComboBox->blockSignals(false);

    ui->awardComboBox->addItem(tr("DXCC"), QVariant("dxcc"));
    ui->awardComboBox->addItem(tr("ITU"), QVariant("itu"));
    ui->awardComboBox->addItem(tr("WAC"), QVariant("wac"));
    ui->awardComboBox->addItem(tr("WAZ"), QVariant("waz"));
    ui->awardComboBox->addItem(tr("WAS"), QVariant("was"));
    ui->awardComboBox->addItem(tr("WPX"), QVariant("wpx"));
    ui->awardComboBox->addItem(tr("IOTA"), QVariant("iota"));
    ui->awardComboBox->addItem(tr("POTA Hunter"), QVariant("potah"));
    ui->awardComboBox->addItem(tr("POTA Activator"), QVariant("potaa"));
    ui->awardComboBox->addItem(tr("SOTA"), QVariant("sota"));
    ui->awardComboBox->addItem(tr("WWFF"), QVariant("wwff"));

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Done"));
    ui->awardTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->awardTableView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    refreshTable(0);
}

AwardsDialog::~AwardsDialog()
{
    FCT_IDENTIFICATION;

    delete ui;
    detailedViewModel->deleteLater();
    entityCallsignModel->deleteLater();
}

void AwardsDialog::refreshTable(int)
{
    FCT_IDENTIFICATION;

    const QList<Band>& dxccBands = BandPlan::bandsList(true, true);

    if ( dxccBands.size() == 0 )
        return;

    QStringList confirmed("1=2 ");
    QStringList modes("'NONE'");
    QString headersColumns;
    QString uniqColumns;
    QString addWherePart;
    QString sourceContactsTable;
    QString sqlPartDetailTable;
    QStringList stmt_max_part;
    QStringList stmt_total_padding;
    QStringList stmt_sum_confirmed;
    QStringList stmt_sum_worked;
    QStringList stmt_sum_total;
    QStringList stmt_having;
    QStringList addlCTEs;

    const QString &awardSelected = getSelectedAward();

    if ( ui->cwCheckBox->isChecked() )
        modes << "'CW'";

    if ( ui->phoneCheckBox->isChecked() )
        modes << "'PHONE'";

    if ( ui->digiCheckBox->isChecked() )
        modes << "'DIGITAL'";

    if ( ui->eqslCheckBox->isChecked() )
        confirmed << " eqsl_qsl_rcvd = 'Y' ";

    if ( ui->lotwCheckBox->isChecked() )
        confirmed << " lotw_qsl_rcvd = 'Y' ";

    if ( ui->paperCheckBox->isChecked() )
        confirmed << " qsl_rcvd = 'Y' ";

    const QString innerConfirmedCase(QString(" CASE WHEN (%1) THEN 2 ELSE 1 END ").arg(confirmed.join("or")));

    for ( const Band& band : dxccBands )
    {
        stmt_max_part << QString(" MAX(CASE WHEN band = '%1' AND m.dxcc IN (%2) THEN %3 ELSE 0 END) as '%4'").arg(band.name,
                                                                                                                  modes.join(","),
                                                                                                                  innerConfirmedCase,
                                                                                                                  band.name);
        stmt_total_padding << QString(" NULL '%1'").arg(band.name);
        stmt_sum_confirmed << QString("SUM(CASE WHEN a.'%1' > 1 THEN 1 ELSE 0 END) '%2'").arg(band.name, band.name);
        stmt_sum_worked << QString("SUM(CASE WHEN a.'%1' > 0 THEN 1 ELSE 0 END) '%2'").arg(band.name, band.name);
        stmt_sum_total << QString("SUM(d.'%1') '%2'").arg(band.name, band.name);
        stmt_having << QString("SUM(d.'%1') = 0").arg(band.name);
    }

    stmt_max_part << QString(" MAX(CASE WHEN prop_mode = 'SAT' AND m.dxcc IN (%1) THEN %2 ELSE 0 END) as 'SAT' ").arg(modes.join(","), innerConfirmedCase)
                  << QString(" MAX(CASE WHEN prop_mode = 'EME' AND m.dxcc IN (%1) THEN %2 ELSE 0 END) as 'EME' ").arg(modes.join(","), innerConfirmedCase);
    stmt_total_padding << " NULL 'SAT' "
                       << " NULL 'EME' ";
    stmt_sum_confirmed << " SUM(CASE WHEN a.'SAT' > 1 THEN 1 ELSE 0 END) 'SAT' "
                       << " SUM(CASE WHEN a.'EME' > 1 THEN 1 ELSE 0 END) 'EME' ";
    stmt_sum_worked << " SUM(CASE WHEN a.'SAT' > 0 THEN 1 ELSE 0 END) 'SAT' "
                    << " SUM(CASE WHEN a.'EME' > 0 THEN 1 ELSE 0 END) 'EME' ";
    stmt_sum_total << " SUM(d.'SAT') 'SAT' "
                   << " SUM(d.'EME') 'EME' ";
    stmt_having << " SUM(d.'SAT') = 0"
                << " SUM(d.'EME') = 0";

    sourceContactsTable = " source_contacts AS ("
                          "  SELECT * "
                          "  FROM contacts )";

    if ( awardSelected == "dxcc" )
    {
        setEntityInputEnabled(true);
        setNotWorkedEnabled(true);
        const QString &entitySelected = getSelectedEntity();
        headersColumns = "translate_to_locale(d.name) col1, d.prefix col2 ";
        uniqColumns = "c.dxcc";
        sqlPartDetailTable = " FROM dxcc_entities d"
                             "   LEFT OUTER JOIN source_contacts c ON d.id = c.dxcc"
                             "   LEFT OUTER JOIN modes m on c.mode = m.name"
                             " WHERE (c.id is NULL or c.my_dxcc = '" + entitySelected + "') ";
        addWherePart = " AND (c.id is NULL OR c.my_dxcc = '" + entitySelected + "') ";
    }
    else if ( awardSelected == "waz" )
    {
        setEntityInputEnabled(true);
        setNotWorkedEnabled(true);
        const QString &entitySelected = getSelectedEntity();
        headersColumns = "d.n col1, null col2 ";
        uniqColumns = "c.cqz";
        addlCTEs<< " cqzCTE AS ( "
                   "   SELECT 1 AS n, 1 AS value"
                   "   UNION ALL"
                   "   SELECT n + 1, value + 1"
                   "   FROM cqzCTE"
                   "   WHERE n < 40 )";
        sqlPartDetailTable = " FROM cqzCTE d "
                             "   LEFT OUTER JOIN source_contacts c ON d.n = c.cqz"
                             "   LEFT OUTER JOIN modes m on c.mode = m.name "
                             " WHERE (c.id IS NULL OR c.my_dxcc = '" + entitySelected + "') ";
        addWherePart = " AND (c.id IS NULL OR c.my_dxcc = '" + entitySelected + "') ";
    }
    else if ( awardSelected == "itu" )
    {
        setEntityInputEnabled(true);
        setNotWorkedEnabled(true);
        const QString &entitySelected = getSelectedEntity();
        headersColumns = "d.n col1, null col2 ";
        uniqColumns = "c.ituz";
        addlCTEs << " ituzCTE AS ("
                    "   SELECT 1 AS n, 1 AS value"
                    "   UNION ALL"
                    "   SELECT n + 1, value + 1"
                    "   FROM ituzCTE"
                    "   WHERE n < 90 )";
        sqlPartDetailTable = " FROM ituzCTE d "
                             "   LEFT OUTER JOIN source_contacts c ON d.n = c.ituz"
                             "   LEFT OUTER JOIN modes m on c.mode = m.name"
                             " WHERE (c.id is NULL or c.my_dxcc = '" + entitySelected + "') ";
        addWherePart = " AND (c.id is NULL OR c.my_dxcc = '" + entitySelected + "') ";

    }
    else if ( awardSelected == "wac" )
    {
        setEntityInputEnabled(true);
        setNotWorkedEnabled(true);
        const QString &entitySelected = getSelectedEntity();
        headersColumns = "d.column2 col1, d.column1 col2 ";
        uniqColumns = "c.cont";
        addlCTEs << "  continents as "
                    "     (values ('NA', '" + tr("North America") + "'),"
                    "             ('SA', '" + tr("South America") + "'),"
                    "             ('EU', '" + tr("Europe") + "'),"
                    "             ('AF', '" + tr("Africa") + "'),"
                    "             ('OC', '" + tr("Oceania") + "'),"
                    "             ('AS', '" + tr("Asia") + "'),"
                    "             ('AN', '" + tr("Antarctica") + "'))";
        sqlPartDetailTable = " FROM continents d "
                             "   LEFT OUTER JOIN source_contacts c ON d.column1 = c.cont "
                             "   LEFT OUTER JOIN modes m on c.mode = m.name "
                             " WHERE (c.id is NULL or c.my_dxcc = '" + entitySelected + "') ";
        addWherePart = " AND (c.id is NULL OR c.my_dxcc = '" + entitySelected + "') ";

    }
    else if ( awardSelected == "was" )
    {
        setEntityInputEnabled(true);
        setNotWorkedEnabled(true);
        const QString &entitySelected = getSelectedEntity();
        headersColumns = "d.subdivision_name col1, d.code col2 ";
        uniqColumns = "c.state";
        sqlPartDetailTable = " FROM adif_enum_primary_subdivision d"
                             "   LEFT OUTER JOIN source_contacts c ON d.dxcc = c.dxcc AND d.code = c.state"
                             "   LEFT OUTER JOIN modes m on c.mode = m.name"
                             " WHERE (c.id is NULL or c.my_dxcc = '" + entitySelected + "' AND d.dxcc in (6, 110, 291)) ";
        addWherePart = " AND (c.id is NULL or c.my_dxcc = '" + entitySelected + "' AND c.dxcc in (6, 110, 291)) ";
    }
    else if ( awardSelected == "wpx" )
    {
        setEntityInputEnabled(true);
        setNotWorkedEnabled(false);
        const QString &entitySelected = getSelectedEntity();
        headersColumns = "c.pfx col1, null col2 ";
        uniqColumns = "c.pfx";
        sqlPartDetailTable = " FROM source_contacts c"
                             "      INNER JOIN modes m ON c.mode = m.name"
                             " WHERE c.pfx is not null"
                             "       AND c.my_dxcc = '" + entitySelected + "'";
        addWherePart = " AND c.my_dxcc = '" + entitySelected + "' ";
    }
    else if ( awardSelected == "iota" )
    {
        setEntityInputEnabled(true);
        setNotWorkedEnabled(false);
        const QString &entitySelected = getSelectedEntity();
        headersColumns = "c.iota col1, NULL col2 ";
        uniqColumns = "c.iota";
        sqlPartDetailTable = " FROM source_contacts c"
                             "      INNER JOIN modes m ON c.mode = m.name"
                             " WHERE c.my_dxcc = '" + entitySelected + "' ";
        addWherePart = " AND c.iota is not NULL"
                       " AND c.my_dxcc = '" + entitySelected + "' ";
    }
    else if ( awardSelected == "potah" )
    {
        setEntityInputEnabled(false);
        setNotWorkedEnabled(false);
        headersColumns = "p.reference col1, p.name col2 ";
        uniqColumns = "c.pota";
        sqlPartDetailTable = " FROM pota_directory p "
                             "      INNER JOIN source_contacts c ON SUBSTR(c.pota, 1, COALESCE(NULLIF(INSTR(c.pota, '@'), 0) - 1, LENGTH(c.pota))) = p.reference"
                             "      INNER JOIN modes m on c.mode = m.name ";
        addlCTEs << " split(id, callsign, station_callsign, my_dxcc, band, dxcc, eqsl_qsl_rcvd, lotw_qsl_rcvd, qsl_rcvd,prop_mode,mode, pota, str) AS ("
                    "   SELECT id, callsign, station_callsign, my_dxcc, band, "
                    "          dxcc, eqsl_qsl_rcvd, lotw_qsl_rcvd, qsl_rcvd,prop_mode,mode, "
                    "          '', pota_ref||',' "
                    "   FROM contacts "
                    "   UNION ALL "
                    "   SELECT id, callsign, station_callsign, my_dxcc, band, "
                    "          dxcc, eqsl_qsl_rcvd, lotw_qsl_rcvd, qsl_rcvd,prop_mode,mode, "
                    "          substr(str, 0, instr(str, ',')), TRIM(substr(str, instr(str, ',') + 1)) "
                    "   FROM split "
                    "   WHERE str != '') ";
        sourceContactsTable = " source_contacts AS ("
                              "   SELECT id, callsign, station_callsign, my_dxcc, band, dxcc, eqsl_qsl_rcvd, lotw_qsl_rcvd, qsl_rcvd,prop_mode,mode, pota "
                              "   FROM split "
                              "   WHERE pota != '' ) ";
        addWherePart = " AND c.pota is not NULL ";
    }
    else if ( awardSelected == "potaa" )
    {
        setEntityInputEnabled(false);
        setNotWorkedEnabled(false);
        headersColumns = "p.reference col1, p.name col2 ";
        uniqColumns = "c.my_pota_ref_str";
        sqlPartDetailTable = " FROM pota_directory p "
                             "      INNER JOIN source_contacts c ON SUBSTR(c.my_pota_ref_str, 1, COALESCE(NULLIF(INSTR(c.my_pota_ref_str, '@'), 0) - 1, LENGTH(c.my_pota_ref_str))) = p.reference"
                             "      INNER JOIN modes m on c.mode = m.name ";
        addlCTEs << " split(id, callsign, station_callsign, my_dxcc, band, dxcc, eqsl_qsl_rcvd, lotw_qsl_rcvd, qsl_rcvd, prop_mode, mode, my_pota_ref_str, str) AS ("
                    "   SELECT id, callsign, station_callsign, my_dxcc, band, "
                    "          dxcc, eqsl_qsl_rcvd, lotw_qsl_rcvd, qsl_rcvd,prop_mode,mode, "
                    "          '', my_pota_ref||',' "
                    "   FROM contacts "
                    "   UNION ALL "
                    "   SELECT id, callsign, station_callsign, my_dxcc, band, "
                    "          dxcc, eqsl_qsl_rcvd, lotw_qsl_rcvd, qsl_rcvd, prop_mode, mode, "
                    "          substr(str, 0, instr(str, ',')), TRIM(substr(str, instr(str, ',') + 1)) "
                    "   FROM split "
                    "   WHERE str != '') ";
        sourceContactsTable = " source_contacts AS ("
                              "   SELECT id, callsign, station_callsign, my_dxcc, band, dxcc, eqsl_qsl_rcvd, lotw_qsl_rcvd, qsl_rcvd,prop_mode, mode, my_pota_ref_str "
                              "   FROM split "
                              "   WHERE my_pota_ref_str != '' ) ";
        addWherePart = " AND c.my_pota_ref_str is not NULL ";
    }
    else if ( awardSelected == "sota" )
    {
        setEntityInputEnabled(false);
        setNotWorkedEnabled(false);
        headersColumns = "s.summit_code col1, NULL col2 ";
        uniqColumns = "c.sota_ref";

        sqlPartDetailTable = " FROM sota_summits s "
                          "     INNER JOIN source_contacts c ON c.sota_ref = s.summit_code "
                          "     INNER JOIN modes m on c.mode = m.name ";
    }
    else if ( awardSelected == "wwff" )
    {
        setEntityInputEnabled(false);
        setNotWorkedEnabled(false);
        headersColumns = "w.reference col1, w.name col2 ";
        uniqColumns = "c.wwff_ref";
        sqlPartDetailTable = " FROM wwff_directory w "
                          "     INNER JOIN source_contacts c ON c.wwff_ref = w.reference "
                          "     INNER JOIN modes m on c.mode = m.name ";
    }

    addlCTEs.append(sourceContactsTable);

    QString finalSQL(QString(
              "WITH "
              "   %1, "
              "   detail_table AS ( "
              "     SELECT %2, %3 "
              "     %4"
              "     %5"
              "     GROUP BY  1,2) "
              "SELECT * FROM ( "
              "     SELECT 0 column_idx, '%6', COUNT(DISTINCT %7), %8"
              "     FROM source_contacts c INNER JOIN modes m ON c.mode = m.name"
              "     WHERE m.dxcc IN (%9)"
              "           %10"
              "   UNION ALL "
              "     SELECT 0 column_idx, '%11', COUNT(DISTINCT %12), %13"
              "     FROM source_contacts c INNER JOIN modes m ON c.mode = m.name "
              "     WHERE (%14)"
              "           AND m.dxcc IN (%15)"
              "           %16"
              "   UNION ALL "
              "     SELECT 1 column_idx, '%17', NULL prefix, %18"
              "     FROM detail_table a "
              "     GROUP BY 1 "
              "   UNION ALL "
              "     SELECT 2 column_idx, '%19', NULL prefix, %20"
              "     FROM detail_table a "
              "     GROUP BY 1 "
              "   UNION ALL "
              "     SELECT 3 column_idx, col1, col2, %21"
              "     FROM detail_table d "
              "     GROUP BY 2,3 "
              "     %22"
              ") "
              "ORDER BY 1,2 COLLATE LOCALEAWARE ASC ").arg(addlCTEs.join(","),
                                                           headersColumns,
                                                           stmt_max_part.join(","),
                                                           sqlPartDetailTable,
                                                           addWherePart,
                                                           tr("TOTAL Worked"),
                                                           uniqColumns,
                                                           stmt_total_padding.join(","),
                                                           modes.join(",")
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
                                                           ,
#else
                                                           ).arg(
#endif
                                                           addWherePart,
                                                           tr("TOTAL Confirmed"),
                                                           uniqColumns,
                                                           stmt_total_padding.join(","),
                                                           confirmed.join("or"),
                                                           modes.join(","),
                                                           addWherePart,
                                                           tr("Confirmed"),
                                                           stmt_sum_confirmed.join(",")
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
                         ,
#else
                                                           ).arg(
#endif
                                                           tr("Worked")).arg(
                                                           stmt_sum_worked.join(","),
                                                           stmt_sum_total.join(","),
                                                           ui->notWorkedCheckBox->isChecked() ? QString("HAVING %1").arg(stmt_having.join(" AND ")) : QString() ));
    qDebug(runtime) << finalSQL;

    detailedViewModel->setQuery(finalSQL);
    detailedViewModel->setHeaderData(1, Qt::Horizontal, "");
    detailedViewModel->setHeaderData(2, Qt::Horizontal, "");
    ui->awardTableView->setModel(detailedViewModel);
    ui->awardTableView->setColumnHidden(0,true);
}

void AwardsDialog::awardTableDoubleClicked(QModelIndex idx)
{
    FCT_IDENTIFICATION;

    const QString &awardSelected = getSelectedAward();
    QStringList addlFilters;
    QString band;
    QString country;

    if ( ui->myEntityComboBox->isEnabled() )
        addlFilters << QString("my_dxcc='%1'").arg(getSelectedEntity());

    if ( idx.row() > 3 )
    {
        if ( awardSelected == "dxcc" )
        {
            country = detailedViewModel->data(detailedViewModel->index(idx.row(),1),Qt::DisplayRole).toString();
        }
        else if ( awardSelected == "itu" )
        {
            addlFilters << QString("ituz = '%1'").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),1),Qt::DisplayRole).toString());
        }
        else if ( awardSelected == "iota" )
        {
            addlFilters << QString("iota = '%1'").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),1),Qt::DisplayRole).toString());
        }
        else if ( awardSelected == "wac" )
        {
            addlFilters << QString("cont = '%1'").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),2),Qt::DisplayRole).toString());
        }
        else if ( awardSelected == "was" )
        {
            addlFilters << QString("state = '%1' and dxcc in (6, 110, 291)").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),2),Qt::DisplayRole).toString());
        }
        else if ( awardSelected == "waz" )
        {
            addlFilters << QString("cqz = '%1'").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),1),Qt::DisplayRole).toString());
        }
        else if ( awardSelected == "wpx" )
        {
            addlFilters << QString("pfx = '%1'").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),1),Qt::DisplayRole).toString());
        }
        else if ( awardSelected == "potah" )
        {
            addlFilters << QString("pota_ref LIKE '%%1%'").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),1),Qt::DisplayRole).toString());
        }
        else if ( awardSelected == "potaa" )
        {
            addlFilters << QString("my_pota_ref LIKE = '%%1%'").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),1),Qt::DisplayRole).toString());
        }
        else if ( awardSelected == "sota" )
        {
            addlFilters << QString("sota_ref = '%1'").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),1),Qt::DisplayRole).toString());
        }
        else if ( awardSelected == "wwff" )
        {
            addlFilters << QString("wwff_ref = '%1'").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),1),Qt::DisplayRole).toString());
        }

        if ( idx.column() > 2 )
        {
            band = detailedViewModel->headerData( idx.column(), Qt::Horizontal ).toString();
        }

        emit AwardConditionSelected(country, band, QString("(") + addlFilters.join(" and ") + QString(")"));
    }
}

const QString AwardsDialog::getSelectedEntity() const
{
    FCT_IDENTIFICATION;

    int row = ui->myEntityComboBox->currentIndex();
    const QModelIndex &idx = ui->myEntityComboBox->model()->index(row,0);
    const QVariant &comboData = ui->myEntityComboBox->model()->data(idx);

    qCDebug(runtime) << comboData.toString();

    return comboData.toString();
}

const QString AwardsDialog::getSelectedAward() const
{
    FCT_IDENTIFICATION;

    QString ret = ui->awardComboBox->itemData(ui->awardComboBox->currentIndex()).toString();
    qCDebug(runtime) << ret;
    return ret;
}

void AwardsDialog::setEntityInputEnabled(bool enabled)
{
    FCT_IDENTIFICATION;

    ui->myEntityComboBox->setVisible(enabled);
    ui->myEntityLabel->setVisible(enabled);
}

void AwardsDialog::setNotWorkedEnabled(bool enabled)
{
    FCT_IDENTIFICATION;

    ui->notWorkedCheckBox->blockSignals(true);
    ui->notWorkedCheckBox->setVisible(enabled);
    ui->notWorkedLabel->setVisible(enabled);
    ui->notWorkedCheckBox->setChecked(enabled && ui->notWorkedCheckBox->isChecked());
    ui->notWorkedCheckBox->blockSignals(false);
}
