#include <QSqlQuery>
#include <QSqlError>
#include <QHeaderView>
#include <QMessageBox>
#include <QButtonGroup>

#include "DXCCSubmissionDialog.h"
#include "ui_DXCCSubmissionDialog.h"
#include "models/SqlListModel.h"
#include "core/debug.h"
#include "core/QSOFilterManager.h"
#include "data/BandPlan.h"
#include "ui/ExportDialog.h"
#include "ui/component/StyleItemDelegate.h"

MODULE_IDENTIFICATION("qlog.ui.dxccsubmissiondialog");

//
// ADIF credit_submitted / credit_granted are comma-delimited lists, e.g.:
//   "DXCC,DXCC_MODE,DXCC_BAND"
//
// Standard DXCC credit tokens:
//   DXCC      — basic DXCC award (any mode, any band)
//   DXCC_MODE — DXCC mode endorsement (CW / Phone / Digital)
//   DXCC_BAND — DXCC band endorsement (per-band, incl. 5-Band DXCC)
//
// We use comma-padding so "DXCC" never accidentally matches "DXCC_MODE" or
// "DXCC_BAND":  INSTR(',' || field || ',', ',TOKEN,') > 0
//
static QString creditHas(const QString &field, const QString &token)
{
    return QString("INSTR(',' || COALESCE(%1,'') || ',', ',%2,') > 0")
               .arg(field, token);
}

DXCCSubmissionDialog::DXCCSubmissionDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DXCCSubmissionDialog)
    , tableModel(new QSqlQueryModel(this))
    , entityCallsignModel(nullptr)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    /********************/
    /* My Entity Combo  */
    /********************/
    entityCallsignModel = new SqlListModel(
        "SELECT my_dxcc, my_country_intl || ' (' || "
        "CASE WHEN LENGTH(GROUP_CONCAT(station_callsign, ', ')) > 50 "
        "THEN SUBSTR(GROUP_CONCAT(station_callsign, ', '), 0, 50) || '...' "
        "ELSE GROUP_CONCAT(station_callsign, ', ') END || ')' "
        "FROM(SELECT DISTINCT my_dxcc, my_country_intl, station_callsign FROM contacts) "
        "GROUP BY my_dxcc ORDER BY my_dxcc;",
        "", ui->myEntityComboBox);

    ui->myEntityComboBox->blockSignals(true);
    while (entityCallsignModel->canFetchMore())
        entityCallsignModel->fetchMore();
    ui->myEntityComboBox->setModel(entityCallsignModel);
    ui->myEntityComboBox->setModelColumn(1);
    ui->myEntityComboBox->blockSignals(false);

    /***************/
    /* User Filter */
    /***************/
    ui->userFilterComboBox->blockSignals(true);
    ui->userFilterComboBox->setModel(QSOFilterManager::QSOFilterModel(tr("No User Filter"),
                                                                      ui->userFilterComboBox));
    ui->userFilterComboBox->blockSignals(false);

    /*********/
    /* Bands */
    /*********/
    ui->bandScopeComboBox->blockSignals(true);
    ui->bandScopeComboBox->addItem(tr("Any Band (Entity Level)"),
                                   QVariant(static_cast<int>(DXCCBandScope::EntityLevel)));
    ui->bandScopeComboBox->addItem(tr("5-Band DXCC (80/40/20/15/10m)"),
                                   QVariant(static_cast<int>(DXCCBandScope::FiveBand)));
    ui->bandScopeComboBox->addItem(tr("All DXCC Bands"),
                                   QVariant(static_cast<int>(DXCCBandScope::AllDXCCBands)));
    ui->bandScopeComboBox->addItem(tr("Custom Band Selection"),
                                   QVariant(static_cast<int>(DXCCBandScope::Custom)));
    ui->bandScopeComboBox->blockSignals(false);

    int i = 0;
    dxccBands = BandPlan::bandsList(false, true);
    for ( const Band &band : static_cast<const QList<Band>&>(dxccBands) )
    {
        const QString &bandName = band.name;
        QCheckBox *cb = new QCheckBox(bandName, this);
        cb->setChecked(true);
        cb->setObjectName("bandCheckBox_" + bandName);

        int row = i / MAXCOLUMNS;
        int column = i % MAXCOLUMNS;
        ui->bandGroup->addWidget(cb, row, column);
        i++;

        bandCheckBoxes.append(cb);
        connect(cb, &QCheckBox::stateChanged, this, &DXCCSubmissionDialog::refreshTable);
    }

    // Band controls only visible when Custom scope is active
    setBandControlsVisible(false);

    ui->submissionTableView->setModel(tableModel);
    ui->submissionTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(ui->buttonGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), this, [this](QAbstractButton*)
    {
        refreshTable();
    });

    ui->exportADIFButton->setEnabled(false);  // enabled once table has results

    ui->submissionTableView->setItemDelegateForColumn(6, new DateFormatDelegate(ui->submissionTableView));
    refreshTable();
}

DXCCSubmissionDialog::~DXCCSubmissionDialog()
{
    FCT_IDENTIFICATION;
    delete ui;
}

void DXCCSubmissionDialog::onBandScopeChanged(int)
{
    FCT_IDENTIFICATION;

    const DXCCBandScope scope = currentScope();
    setBandControlsVisible(scope == DXCCBandScope::Custom);
    refreshTable();
}

void DXCCSubmissionDialog::onFiveBandClicked()
{
    FCT_IDENTIFICATION;

    ui->bandScopeComboBox->blockSignals(true);
    ui->bandScopeComboBox->setCurrentIndex(
        ui->bandScopeComboBox->findData(static_cast<int>(DXCCBandScope::Custom)));
    ui->bandScopeComboBox->blockSignals(false);

    setBandControlsVisible(true);

    selectBandPreset(FIVE_BAND_DXCC);
    refreshTable();
}

void DXCCSubmissionDialog::onAllBandsClicked()
{
    FCT_IDENTIFICATION;

    for ( QCheckBox *cb : static_cast<const QList<QCheckBox*>&>(bandCheckBoxes) )
    {
        cb->blockSignals(true);
        cb->setChecked(true);
        cb->blockSignals(false);
    }
}

void DXCCSubmissionDialog::refreshTable()
{
    FCT_IDENTIFICATION;

    if ( dxccBands.isEmpty() )
        return;

    const DXCCBandScope scope   = currentScope();
    const bool perBand = (scope != DXCCBandScope::EntityLevel);
    const bool isMixed = ui->mixedRadioButton->isChecked();

    const QStringList selectedBands    = getSelectedBands(scope);
    const QString     modeGroupFilter  = buildModeGroupFilter();

    /**********************/
    /* Confirmation Level */
    /**********************/
    // ARRL DXCC accepts LoTW and paper/direct QSL cards only.
    QStringList confConds;
    if (ui->lotwCheckBox->isChecked())   confConds << "c.lotw_qsl_rcvd = 'Y'";
    if (ui->paperCheckBox->isChecked())  confConds << "c.qsl_rcvd = 'Y'";

    if ( confConds.isEmpty() )
    {
        clearTable();
        updateStatusLabel(0, selectedBands, scope);
        return;
    }

    const QString confFilter = confConds.join(" OR ");

    QString bandWhereClause;
    if ( perBand )
    {
        if ( selectedBands.isEmpty() )
        {
            clearTable();
            updateStatusLabel(0, selectedBands, scope);
            return;
        }
        QStringList quoted;
        for ( const QString &b : selectedBands )
            quoted << QString("'%1'").arg(b);
        bandWhereClause = QString("AND c.band IN (%1)").arg(quoted.join(","));
    }

    const QString myEntity  = getSelectedEntity();
    const QString userFilter = (ui->userFilterComboBox->currentIndex() > 0)
        ? "AND " + QSOFilterManager::instance()->getWhereClause(
                       ui->userFilterComboBox->currentText())
        : "";

    // ── ADIF credit token selection ────────────────────────────────────────
    //
    // ARRL DXCC credit tokens stored in credit_submitted / credit_granted:
    //   DXCC      — basic DXCC (any mode, any band)
    //   DXCC_MODE — mode endorsement (CW / Phone / Digital, entity-level)
    //   DXCC_BAND — band endorsement (any mode per band, incl. 5-Band DXCC)
    //
    // For per-band scope, only DXCC_BAND matters — 5-Band DXCC (and all band
    // endorsements) require one contact per entity per band, ANY mode.  There
    // is no separate "CW on 80m" credit token; the mode filter just controls
    // which contacts are eligible to show/submit, not the credit token itself.
    //
    // For entity-level:
    //   Mixed          → DXCC
    //   CW/Phone/Digi  → DXCC_MODE  (filter ensures only that mode is shown)

    const QString creditToken = perBand ? "DXCC_BAND"
                              : (isMixed ? "DXCC" : "DXCC_MODE");

    // ── slot_credits CTE ───────────────────────────────────────────────────
    //
    // Key fix: credit status is aggregated across ALL QSOs for the slot, not
    // just the single "best display" QSO selected by ROW_NUMBER().
    //
    // Example failure case without this: a newer LoTW-matched Alaska-10m QSO
    // is ranked #1 for display but has no credit_granted yet; an older QSO
    // that was actually submitted already has credit_granted=DXCC_BAND.
    // Without slot_credits the Alaska-10m slot would wrongly appear as
    // "not yet submitted."
    //
    // slot_credits does NOT apply confFilter — credit_granted is authoritative
    // regardless of the current QSL confirmation state in QLog.
    // For entity-level mode awards it DOES apply modeGroupFilter so we don't
    // mistake a Phone credit for a CW-mode check.
    // For per-band it does NOT apply modeGroupFilter (any mode earns DXCC_BAND).

    const QString slotGroupBy    = perBand ? "c.dxcc, c.band" : "c.dxcc";
    const QString slotModeFilter = (perBand || isMixed)
                                   ? ""
                                   : "AND (" + modeGroupFilter + ") ";

    const QString slotCreditsCTE =
        "slot_credits AS ( "
        "  SELECT " + slotGroupBy + ", "
        "    MAX(CASE WHEN " + creditHas("c.credit_submitted", creditToken) + " THEN 1 ELSE 0 END) AS slot_submitted, "
        "    MAX(CASE WHEN " + creditHas("c.credit_granted",   creditToken) + " THEN 1 ELSE 0 END) AS slot_granted "
        "  FROM contacts c "
        "  INNER JOIN modes m ON c.mode = m.name "
        "  WHERE c.my_dxcc = '" + myEntity + "' "
        "    AND c.dxcc IS NOT NULL "
        "    " + slotModeFilter +
        "    " + bandWhereClause + " "
        "    " + userFilter + " "
        "  GROUP BY " + slotGroupBy + " "
        ") ";

    // ── ranked CTE ────────────────────────────────────────────────────────
    // Picks the single best confirmed QSO per slot for display purposes.
    // Priority: LoTW confirmed > Paper confirmed > most recent.
    // Mode and confirmation filters are applied so we show the most relevant
    // contact matching the user's selection.

    const QString partitionClause = perBand
        ? "PARTITION BY c.dxcc, c.band"
        : "PARTITION BY c.dxcc";

    const QString rankedCTE =
        "ranked AS ( "
        "  SELECT "
        "    c.id, c.callsign, c.band, c.mode, c.start_time, c.dxcc, "
        "    c.lotw_qsl_rcvd, c.qsl_rcvd, "
        "    ROW_NUMBER() OVER ( "
        "      " + partitionClause + " "
        "      ORDER BY "
        "        CASE WHEN c.lotw_qsl_rcvd = 'Y' THEN 0 ELSE 1 END, "
        "        CASE WHEN c.qsl_rcvd       = 'Y' THEN 0 ELSE 1 END, "
        "        c.start_time DESC "
        "    ) AS rn "
        "  FROM contacts c "
        "  INNER JOIN modes m ON c.mode = m.name "
        "  WHERE c.my_dxcc = '" + myEntity + "' "
        "    AND c.dxcc IS NOT NULL "
        "    AND (" + confFilter + ") "
        "    AND (" + modeGroupFilter + ") "
        "    " + bandWhereClause + " "
        "    " + userFilter + " "
        ") ";

    // ── JOIN condition between ranked best-QSO and slot_credits ───────────
    const QString slotJoinOn = perBand
        ? "sc.dxcc = bc.dxcc AND sc.band = bc.band"
        : "sc.dxcc = bc.dxcc";

    // ── Status filter (uses aggregated slot_credits columns) ───────────────
    QStringList statusConds;
    if (ui->showUnsubmittedCheckBox->isChecked())
        statusConds << "(sc.slot_submitted = 0 AND sc.slot_granted = 0)";
    if (ui->showSubmittedCheckBox->isChecked())
        statusConds << "(sc.slot_submitted = 1 AND sc.slot_granted = 0)";
    if (ui->showGrantedCheckBox->isChecked())
        statusConds << "(sc.slot_granted = 1)";

    if ( statusConds.isEmpty() )
    {
        clearTable();
        updateStatusLabel(0, selectedBands, scope);
        return;
    }
    const QString statusFilter = "(" + statusConds.join(" OR ") + ")";

    // ── Final SQL ──────────────────────────────────────────────────────────
    // Column 0 is the contact id — hidden in the table view but used by
    // exportAsADIF() to fetch full contact records for the export.
    const QString sql =
        "WITH " + slotCreditsCTE + ", " + rankedCTE +
        "SELECT "
        "  bc.id, "                                                                        // col 0 (hidden)
        "  translate_to_locale(e.name)  AS '" + tr("Entity")    + "', "                   // col 1
        "  e.prefix AS '" + tr("Prefix")   + "', "                                        // col 2
        "  bc.callsign AS '" + tr("Callsign") + "', "                                     // col 3
        "  bc.band  AS '" + tr("Band")     + "', "                                        // col 4
        "  bc.mode  AS '" + tr("Mode")     + "', "                                        // col 5
        "  strftime('%Y-%m-%d', bc.start_time) AS '" + tr("Date") + "', "                 // col 6
        "  CASE WHEN bc.lotw_qsl_rcvd = 'Y' THEN 'Y' ELSE '' END AS '" + tr("LoTW")  + "', " // col 7
        "  CASE WHEN bc.qsl_rcvd      = 'Y' THEN 'Y' ELSE '' END AS '" + tr("Paper") + "', " // col 8
        "  CASE WHEN sc.slot_submitted = 1   THEN 'Y' ELSE '' END AS '" + tr("Submitted") + "', " // col 9
        "  CASE WHEN sc.slot_granted   = 1   THEN 'Y' ELSE '' END AS '" + tr("Granted")   + "'  " // col 10
        "FROM ranked bc "
        "INNER JOIN dxcc_entities_clublog e ON bc.dxcc = e.id "
        "INNER JOIN slot_credits sc ON " + slotJoinOn + " "
        "WHERE bc.rn = 1 "
        "  AND " + statusFilter + " "
        "ORDER BY e.name COLLATE LOCALEAWARE ASC, bc.band ASC ";

    qCDebug(runtime) << "DXCC Submission SQL:" << sql;

    tableModel->setQuery(sql);

    if ( tableModel->lastError().isValid() )
        qCWarning(runtime) << "DXCC Submission query error:"
                           << tableModel->lastError().text();

    while (tableModel->canFetchMore())
        tableModel->fetchMore();

    // Column 0 carries the contact id for export — keep it hidden from the user
    ui->submissionTableView->setColumnHidden(0, true);

    const int count = tableModel->rowCount();
    ui->exportADIFButton->setEnabled(count > 0);
    updateStatusLabel(count, selectedBands, scope);
}

void DXCCSubmissionDialog::exportAsADIF()
{
    FCT_IDENTIFICATION;

    // Collect contact IDs from hidden column 0 of the current result set
    QStringList ids;
    for ( int row = 0; row < tableModel->rowCount(); ++row )
    {
        const QString id = tableModel->data(tableModel->index(row, 0)).toString();
        if ( !id.isEmpty() )
            ids << id;
    }

    if ( ids.isEmpty() )
    {
        QMessageBox::information(this, tr("Export ADIF"),
                                 tr("No contacts to export."));
        return;
    }

    // Fetch full contact records so ExportDialog has all ADIF fields available
    QSqlQuery query;
    if (!query.exec(QString("SELECT * FROM contacts WHERE id IN (%1) "
                            "ORDER BY start_time ASC").arg(ids.join(","))))
    {
        qCWarning(runtime) << "DXCC export fetch error:" << query.lastError().text();
        QMessageBox::critical(this, tr("Export ADIF"),
                              tr("Failed to retrieve contact records."));
        return;
    }

    QList<QSqlRecord> records;
    while ( query.next() )
        records << query.record();

    if ( records.isEmpty() )
        return;

    ExportDialog dialog(records, this);
    dialog.setWindowTitle(tr("Export DXCC Submission List as ADIF"));
    dialog.exec();
}

const QString DXCCSubmissionDialog::getSelectedEntity() const
{
    FCT_IDENTIFICATION;

    const int row = ui->myEntityComboBox->currentIndex();
    const QModelIndex idx = ui->myEntityComboBox->model()->index(row, 0);
    return ui->myEntityComboBox->model()->data(idx).toString();
}

QStringList DXCCSubmissionDialog::getSelectedBands(DXCCBandScope scope) const
{
    FCT_IDENTIFICATION;

    switch (scope)
    {
    case DXCCBandScope::FiveBand:
        return FIVE_BAND_DXCC;

    case DXCCBandScope::AllDXCCBands:
    {
        QStringList all;
        all.reserve(dxccBands.size());
        for (const Band &b : dxccBands)
            all << b.name;
        return all;
    }

    case DXCCBandScope::Custom:
    {
        QStringList selected;
        for ( const QCheckBox *cb : static_cast<const QList<QCheckBox*>&>(bandCheckBoxes) )
            if (cb->isChecked())
                selected << cb->text();
        return selected;
    }

    default:
        return {};
    }
}

QString DXCCSubmissionDialog::buildModeGroupFilter() const
{
    FCT_IDENTIFICATION;

    if (ui->cwRadioButton->isChecked())           return "m.dxcc = 'CW'";
    if (ui->phoneRadioButton->isChecked())        return "m.dxcc = 'PHONE'";
    if (ui->digitalRadioButton->isChecked())      return "m.dxcc = 'DIGITAL'";
    return "m.dxcc IN ('CW', 'PHONE', 'DIGITAL')";  // Mixed
}

void DXCCSubmissionDialog::selectBandPreset(const QStringList &bands)
{
    FCT_IDENTIFICATION;

    for ( QCheckBox *cb : static_cast<const QList<QCheckBox*>&>(bandCheckBoxes) )
    {
        cb->blockSignals(true);
        cb->setChecked(bands.contains(cb->text(), Qt::CaseInsensitive));
        cb->blockSignals(false);
    }
}

void DXCCSubmissionDialog::updateStatusLabel(int count, const QStringList &selectedBands, DXCCBandScope scope)
{
    FCT_IDENTIFICATION;

    const bool perBand = ( scope != DXCCBandScope::EntityLevel) ;
    QString modeStr;

    if      ( ui->cwRadioButton->isChecked() )      modeStr = tr("CW");
    else if ( ui->phoneRadioButton->isChecked() )   modeStr = tr("Phone");
    else if ( ui->digitalRadioButton->isChecked() ) modeStr = tr("Digital");
    else                                            modeStr = tr("Mixed");

    QString scopeStr;

    switch ( scope )
    {
    case DXCCBandScope::EntityLevel:  scopeStr = tr("any band");  break;
    case DXCCBandScope::FiveBand:     scopeStr = tr("5-band");    break;
    case DXCCBandScope::AllDXCCBands: scopeStr = tr("all bands"); break;
    case DXCCBandScope::Custom:
        scopeStr = tr("%1 selected band(s)").arg(selectedBands.size());
        break;
    }

    if ( count == 0 )
        ui->statusLabel->setText(tr("No contacts match the selected criteria."));
    else
        ui->statusLabel->setText(
                    tr("%1 %2 %3 — DXCC %4 / %5")
                    .arg(count)
                    .arg(perBand ? tr("band-slot") : tr("entity"))
                    .arg(count == 1 ? tr("entry") : tr("entries"))
                    .arg(modeStr)
                    .arg(scopeStr));
}

void DXCCSubmissionDialog::setBandControlsVisible(bool visible)
{
    FCT_IDENTIFICATION;

    for (int i = 0; i < ui->bandGroup->count(); ++i)
    {
        QLayoutItem *item = ui->bandGroup->itemAt(i);
        if ( QWidget *w = item->widget() )
            w->setVisible(visible);
    }
    ui->bandsLabel->setVisible(visible);
    ui->fiveBandButton->setVisible(visible);
    ui->allBandsButton->setVisible(visible);
}

DXCCSubmissionDialog::DXCCBandScope DXCCSubmissionDialog::currentScope() const
{
    FCT_IDENTIFICATION;

    return static_cast<DXCCBandScope>(ui->bandScopeComboBox->currentData().toInt());
}

void DXCCSubmissionDialog::clearTable()
{
    tableModel->setQuery(QString());
}
