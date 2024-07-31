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
    ui->awardComboBox->addItem(tr("POTA Hunter"),QVariant("potah"));
    ui->awardComboBox->addItem(tr("POTA Activator"),QVariant("potaa"));
    ui->awardComboBox->addItem(tr("SOTA"),QVariant("sota"));
    ui->awardComboBox->addItem(tr("WWFF"),QVariant("wwff"));
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

    QStringList confirmed("1=2 ");
    QStringList modes("'NONE'");
    QString headersColumns;
    QString uniqColumns;
    QString excludePart;

    QString awardSelected = getSelectedAward();
    QString entitySelected = getSelectedEntity();

    QString sqlPart = "FROM contacts c, modes m  "
                      "WHERE c.mode = m.name"
                      "      AND c.my_dxcc = '" + entitySelected + "' ";

    if ( ui->cwCheckBox->isChecked() )
    {
        modes << "'CW'";
    }

    if ( ui->phoneCheckBox->isChecked() )
    {
        modes << "'PHONE'";
    }

    if ( ui->digiCheckBox->isChecked() )
    {
        modes << "'DIGITAL'";

    }

    if ( awardSelected == "dxcc" )
    {
        headersColumns = "translate_to_locale(d.name) col1, d.prefix col2 ";
        uniqColumns = "c.dxcc";
        sqlPart = " FROM dxcc_entities d "
                  "     LEFT OUTER JOIN contacts c ON d.id = c.dxcc "
                  "     LEFT OUTER JOIN modes m on c.mode = m.name "
                  "WHERE (c.id is NULL or c.my_dxcc = '" + entitySelected + "') ";
    }
    else if ( awardSelected == "waz" )
    {
        headersColumns = "d.n col1, null col2 ";
        uniqColumns = "c.cqz";
        sqlPart = " FROM cqzCTE d "
                  "     LEFT OUTER JOIN contacts c ON d.n = c.cqz "
                  "     LEFT OUTER JOIN modes m on c.mode = m.name "
                  "WHERE (c.id is NULL or c.my_dxcc = '" + entitySelected + "') ";
    }
    else if ( awardSelected == "itu" )
    {
        headersColumns = "d.n col1, null col2 ";
        uniqColumns = "c.ituz";
        sqlPart = " FROM ituzCTE d "
                  "     LEFT OUTER JOIN contacts c ON d.n = c.ituz "
                  "     LEFT OUTER JOIN modes m on c.mode = m.name "
                  "WHERE (c.id is NULL or c.my_dxcc = '" + entitySelected + "') ";

    }
    else if ( awardSelected == "wac" )
    {
        headersColumns = "d.column2 col1, d.column1 col2 ";
        uniqColumns = "c.cont";
        sqlPart = " FROM continents d "
                  "     LEFT OUTER JOIN contacts c ON d.column1 = c.cont "
                  "     LEFT OUTER JOIN modes m on c.mode = m.name "
                  "WHERE (c.id is NULL or c.my_dxcc = '" + entitySelected + "') ";

    }
    else if ( awardSelected == "was" )
    {
        headersColumns = "d.subdivision_name col1, d.code col2 ";
        uniqColumns = "c.state";
        sqlPart = " FROM adif_enum_primary_subdivision d "
                  "     LEFT OUTER JOIN contacts c ON d.dxcc = c.dxcc AND d.code = c.state "
                  "     LEFT OUTER JOIN modes m on c.mode = m.name "
                  "WHERE (c.id is NULL or c.my_dxcc = '" + entitySelected + "' AND d.dxcc in (6, 110, 291)) ";
    }
    else if ( awardSelected == "wpx" )
    {
        headersColumns = "c.pfx col1, null col2 ";
        uniqColumns = "c.pfx";
        sqlPart = "FROM contacts c, modes m  "
                  "WHERE c.mode = m.name"
                  "      AND c.pfx is not null"
                  "      AND c.my_dxcc = '" + entitySelected + "'";
    }
    else if ( awardSelected == "iota" )
    {
        headersColumns = "c.iota col1, NULL col2 ";
        uniqColumns = "c.iota";
        excludePart = " AND c.iota is not NULL ";
    }
    else if ( awardSelected == "potah" )
    {
        headersColumns = "p.reference col1, translate_to_locale(p.name) col2 ";
        uniqColumns = "c.pota_ref";
        sqlPart = " FROM pota_directory p "
                  "     INNER JOIN contacts c ON p.reference = c.pota_ref "
                  "     LEFT OUTER JOIN modes m on c.mode = m.name ";
    }
    else if ( awardSelected == "potaa" )
    {
        headersColumns = "p.reference col1, translate_to_locale(p.name) col2 ";
        uniqColumns = "c.my_pota_ref";
        sqlPart = " FROM pota_directory p "
                  "     INNER JOIN contacts c ON p.reference = c.my_pota_ref "
                  "     LEFT OUTER JOIN modes m on c.mode = m.name ";
    }   else if ( awardSelected == "sota" )
    {
        headersColumns = "s.summit_code col1, NULL col2 ";
        uniqColumns = "c.sota_ref";
        sqlPart = " FROM sota_summits s "
                  "     INNER JOIN contacts c ON s.summit_code = c.sota_ref "
                  "     LEFT OUTER JOIN modes m on c.mode = m.name ";
    }
    else if ( awardSelected == "wwff" )
    {
        headersColumns = "w.reference col1, translate_to_locale(w.name) col2 ";
        uniqColumns = "c.wwff_ref";
        sqlPart = " FROM wwff_directory w "
                  "     INNER JOIN contacts c ON w.reference = c.wwff_ref "
                  "     LEFT OUTER JOIN modes m on c.mode = m.name ";
    }
    if ( ui->eqslCheckBox->isChecked() )
    {
        confirmed << " eqsl_qsl_rcvd = 'Y' ";
    }

    if ( ui->lotwCheckBox->isChecked() )
    {
        confirmed << " lotw_qsl_rcvd = 'Y' ";
    }

    if ( ui->paperCheckBox->isChecked() )
    {
        confirmed << " qsl_rcvd = 'Y' ";
    }

    QString innerCase = " CASE WHEN (" + confirmed.join("or") + ") THEN 2 ELSE 1 END ";

    const QList<Band>& dxccBands = BandPlan::bandsList(true, true);

    if ( dxccBands.size() == 0 )
    {
        return;
    }

    QStringList stmt_max_part;
    QStringList stmt_total_padding;
    QStringList stmt_sum_confirmed;
    QStringList stmt_sum_worked;
    QStringList stmt_sum_total;

    for ( int i = 0; i < dxccBands.size(); i++ )
    {
        stmt_max_part << QString(" MAX(CASE WHEN band = '%1' AND m.dxcc IN (" + modes.join(",") + ") THEN " + innerCase + " ELSE 0 END) as '%2'").arg(dxccBands[i].name, dxccBands[i].name);
        stmt_total_padding << QString(" NULL '%1'").arg(dxccBands[i].name);
        stmt_sum_confirmed << QString("SUM(CASE WHEN a.'%1' > 1 THEN 1 ELSE 0 END) '%2'").arg(dxccBands[i].name, dxccBands[i].name);
        stmt_sum_worked << QString("SUM(CASE WHEN a.'%1' > 0 THEN 1 ELSE 0 END) '%2'").arg(dxccBands[i].name, dxccBands[i].name);
        stmt_sum_total << QString("SUM(d.'%1') '%2'").arg(dxccBands[i].name, dxccBands[i].name);
    }
    detailedViewModel->setQuery(
                    "WITH dxcc_summary AS ( "
                    "SELECT  " + headersColumns +", "
                    + stmt_max_part.join(",") + ", "
                    "    MAX(CASE WHEN prop_mode = 'SAT' AND m.dxcc IN (" + modes.join(",") + ") THEN " + innerCase + " ELSE 0 END) as 'SAT', "
                    "    MAX(CASE WHEN prop_mode = 'EME' AND m.dxcc IN (" + modes.join(",") + ") THEN " + innerCase + " ELSE 0 END) as 'EME' "
                    + sqlPart
                    + excludePart +
                    "GROUP BY  1,2), "
                    " ituzCTE AS ( "
                    " SELECT 1 AS n, 1 AS value "
                    " UNION ALL "
                    " SELECT n + 1, value + 1 "
                    " FROM ituzCTE "
                    " WHERE n < 90 ), "
                    " cqzCTE AS ( "
                    " SELECT 1 AS n, 1 AS value "
                    " UNION ALL "
                    " SELECT n + 1, value + 1 "
                    " FROM cqzCTE "
                    " WHERE n < 40 ), "
                    "continents as "
                    "(values ('NA', '" + tr("North America") + "'),('SA','" + tr("South America") + "'),('EU', '" + tr("Europe") + "'),('AF', '" + tr("Africa") + "'),('OC', '" + tr("Oceania") + "'),('AS', '" + tr("Asia") + "'),('AN', '" + tr("Antarctica") + "')) "
                    "SELECT * FROM ( "
                    "SELECT 0 column_idx, "
                    "       '" + tr("TOTAL Worked") + "',  "
                    "       count(DISTINCT " + uniqColumns + "), "
                    + stmt_total_padding.join(",") + ", " +
                    "       NULL 'SAT', "
                    "       NULL 'EME' "
                    "FROM contacts c, modes m "
                    "WHERE c.mode = m.name "
                    "      AND c.my_dxcc = '" + entitySelected + "' "
                    "      AND m.dxcc IN (" + modes.join(",") + ") "
                    + excludePart +
                    "UNION ALL "
                    "SELECT 0 column_idx, "
                    "       '" + tr("TOTAL Confirmed") + "',  "
                    "       count(DISTINCT " + uniqColumns + "), "
                    + stmt_total_padding.join(",") + ", " +
                    "       NULL 'SAT', "
                    "       NULL 'EME' "
                    "FROM contacts c, modes m "
                    "WHERE (" + confirmed.join("or") + ") "
                    "      AND c.mode = m.name "
                    "      AND m.dxcc IN (" + modes.join(",") + ") "
                    "      AND c.my_dxcc = '" + entitySelected + "' "
                    + excludePart +
                    "UNION ALL "
                    "SELECT 1 column_idx, "
                    "       '" + tr("Confirmed") + "', NULL prefix, "
                    + stmt_sum_confirmed.join(",") + ", " +
                    "       SUM(CASE WHEN a.'SAT' > 1 THEN 1 ELSE 0 END) 'SAT',  "
                    "       SUM(CASE WHEN a.'EME' > 1 THEN 1 ELSE 0 END) 'EME'  "
                    "FROM dxcc_summary a "
                    "GROUP BY 1 "
                    "UNION ALL "
                    "SELECT 2 column_idx, "
                    "       '" + tr("Worked") + "', NULL prefix, "
                    + stmt_sum_worked.join(",") + ", " +
                    "       SUM(CASE WHEN a.'SAT' > 0 THEN 1 ELSE 0 END) 'SAT',  "
                    "       SUM(CASE WHEN a.'EME' > 0 THEN 1 ELSE 0 END) 'EME'  "
                    "FROM dxcc_summary a "
                    "GROUP BY 1 "
                    "UNION ALL "
                    "SELECT 3 column_idx,  "
                    "       col1, col2, "
                    + stmt_sum_total.join(",") + ", " +
                    "       SUM(d.'SAT') 'SAT',  "
                    "       SUM(d.'EME') 'EME'  "
                    "       from dxcc_summary d "
                    "GROUP BY 2,3 "
                    ") "
                    "ORDER BY 1,2 COLLATE LOCALEAWARE ASC ");

    qDebug(runtime) << detailedViewModel->query().lastQuery();

    detailedViewModel->setHeaderData(1, Qt::Horizontal, "");
    detailedViewModel->setHeaderData(2, Qt::Horizontal, "");

    ui->awardTableView->setModel(detailedViewModel);
    ui->awardTableView->setColumnHidden(0,true);
}

void AwardsDialog::awardTableDoubleClicked(QModelIndex idx)
{
    FCT_IDENTIFICATION;

    QString myEntitySelected = getSelectedEntity();
    QString awardSelected = getSelectedAward();
    QStringList addlFilters;
    QString band;
    QString country;

    addlFilters << QString("my_dxcc='%1'").arg(myEntitySelected);

    if ( idx.row() > 3 )
    {
        if ( awardSelected == "dxcc" )
        {
            country = detailedViewModel->data(detailedViewModel->index(idx.row(),1),Qt::DisplayRole).toString();
        }
        if ( awardSelected == "itu" )
        {
            addlFilters << QString("ituz = '%1'").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),1),Qt::DisplayRole).toString());
        }
        if ( awardSelected == "iota" )
        {
            addlFilters << QString("upper(iota) = upper('%1')").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),1),Qt::DisplayRole).toString());
        }
        if ( awardSelected == "wac" )
        {
            addlFilters << QString("cont = '%1'").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),2),Qt::DisplayRole).toString());
        }
        if ( awardSelected == "was" )
        {
            addlFilters << QString("state = '%1' and dxcc in (6, 110, 291)").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),2),Qt::DisplayRole).toString());
        }
        if ( awardSelected == "waz" )
        {
            addlFilters << QString("cqz = '%1'").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),1),Qt::DisplayRole).toString());
        }
        if ( awardSelected == "wpx" )
        {
            addlFilters << QString("pfx = '%1'").arg(detailedViewModel->data(detailedViewModel->index(idx.row(),1),Qt::DisplayRole).toString());
        }

        if ( idx.column() > 2 )
        {
            band = detailedViewModel->headerData( idx.column(), Qt::Horizontal ).toString();
        }

        emit AwardConditionSelected(country, band, QString("(") + addlFilters.join(" and ") + QString(")"));
    }
}

QString AwardsDialog::getSelectedEntity()
{
    FCT_IDENTIFICATION;

    int row = ui->myEntityComboBox->currentIndex();
    QModelIndex idx = ui->myEntityComboBox->model()->index(row,0);
    QVariant comboData = ui->myEntityComboBox->model()->data(idx);

    qCDebug(runtime) << comboData.toString();

    return comboData.toString();
}

QString AwardsDialog::getSelectedAward()
{
    FCT_IDENTIFICATION;

    QString ret = ui->awardComboBox->itemData(ui->awardComboBox->currentIndex()).toString();
    qCDebug(runtime) << ret;
    return ret;
}
