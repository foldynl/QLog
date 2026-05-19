#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QHeaderView>
#include <QDateTime>
#include <algorithm>
#include <functional>

#include "ui/DuplicateContactsDialog.h"
#include "ui_DuplicateContactsDialog.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.duplicatecontactsdialog");

// Column indices
static const int COL_DELETE    = 0;
static const int COL_ID        = 1;
static const int COL_DATETIME  = 2;
static const int COL_CALLSIGN  = 3;
static const int COL_MODE      = 4;
static const int COL_MODEGROUP = 5;
static const int COL_BAND      = 6;
static const int COL_FREQ      = 7;
static const int COL_QSL       = 8;
static const int COL_LOTW      = 9;
static const int COL_EQSL      = 10;
static const int COL_MYCALL    = 11;
static const int COL_COUNT     = 12;

// data[] layout per contact (11 values from query)
static const int D_ID        = 0;
static const int D_TIME      = 1;
static const int D_CALL      = 2;
static const int D_MODE      = 3;
static const int D_BAND      = 4;
static const int D_FREQ      = 5;
static const int D_MYCALL    = 6;
static const int D_QSL       = 7;
static const int D_LOTW      = 8;
static const int D_EQSL      = 9;
static const int D_MODEGROUP = 10;

// Group background colors are resolved at runtime from the widget palette so
// they automatically follow the active theme (native / light / dark).

DuplicateContactsDialog::DuplicateContactsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DuplicateContactsDialog)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->tableWidget->setColumnCount(COL_COUNT);
    ui->tableWidget->setHorizontalHeaderLabels({
        tr("Delete"), tr("ID"), tr("Date/Time (UTC)"), tr("Callsign"),
        tr("Mode"), tr("Mode Group"), tr("Band"), tr("Freq (MHz)"),
        tr("QSL Rcvd"), tr("LoTW Rcvd"), tr("eQSL Rcvd"), tr("My Callsign")
    });
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget->verticalHeader()->hide();
    ui->tableWidget->setShowGrid(true);

    connect(ui->autoSelectButton, &QPushButton::clicked,
            this, &DuplicateContactsDialog::autoSelectDuplicates);
    connect(ui->unselectAllButton, &QPushButton::clicked,
            this, &DuplicateContactsDialog::unselectAll);
    connect(ui->mergeButton, &QPushButton::clicked,
            this, &DuplicateContactsDialog::mergeGroups);
    connect(ui->removeButton, &QPushButton::clicked,
            this, &DuplicateContactsDialog::removeChecked);
    connect(ui->tableWidget, &QTableWidget::itemChanged,
            this, &DuplicateContactsDialog::onItemChanged);

    findDuplicates();
}

DuplicateContactsDialog::~DuplicateContactsDialog()
{
    FCT_IDENTIFICATION;

    delete ui;
}

void DuplicateContactsDialog::findDuplicates()
{
    FCT_IDENTIFICATION;

    ui->tableWidget->blockSignals(true);
    ui->tableWidget->setRowCount(0);
    ui->tableWidget->blockSignals(false);

    ui->statusLabel->setText(tr("Searching for duplicates..."));
    updateRemoveButton();

    /*
     * Duplicate criteria (mirrors ADIF import logic, but with DXCC mode groups):
     *   - Same callsign
     *   - Same band  (case-insensitive)
     *   - Same satellite name (or both empty)
     *   - Start times within 30 minutes
     *   - Same DXCC mode group: CW / PHONE / DIGITAL
     *     DATA-U and DATA-L are treated as DIGITAL even if absent from the
     *     modes table.  All other unknown modes fall back to their literal name.
     */
    QSqlQuery query;
    if (!query.prepare(
        "SELECT "
        "  c1.id, c1.start_time, c1.callsign, c1.mode, c1.band, c1.freq, "
        "  c1.station_callsign, c1.qsl_rcvd, c1.lotw_qsl_rcvd, c1.eqsl_qsl_rcvd, "
        "  COALESCE(CASE WHEN c1.mode IN ('DATA-U','DATA-L','DATA') THEN 'DIGITAL' ELSE NULL END, mg1.dxcc, c1.mode), "
        "  c2.id, c2.start_time, c2.callsign, c2.mode, c2.band, c2.freq, "
        "  c2.station_callsign, c2.qsl_rcvd, c2.lotw_qsl_rcvd, c2.eqsl_qsl_rcvd, "
        "  COALESCE(CASE WHEN c2.mode IN ('DATA-U','DATA-L','DATA') THEN 'DIGITAL' ELSE NULL END, mg2.dxcc, c2.mode) "
        "FROM contacts c1 "
        "JOIN contacts c2 ON c1.callsign = c2.callsign AND c1.id < c2.id "
        "LEFT JOIN modes mg1 ON mg1.name = c1.mode "
        "LEFT JOIN modes mg2 ON mg2.name = c2.mode "
        "WHERE upper(c1.band) = upper(c2.band) "
        "AND COALESCE(c1.sat_name, '') = COALESCE(c2.sat_name, '') "
        "AND ABS(JULIANDAY(c1.start_time) - JULIANDAY(c2.start_time))*24*60 < 30 "
        "AND COALESCE(CASE WHEN c1.mode IN ('DATA-U','DATA-L','DATA') THEN 'DIGITAL' ELSE NULL END, mg1.dxcc, c1.mode) "
        "  = COALESCE(CASE WHEN c2.mode IN ('DATA-U','DATA-L','DATA') THEN 'DIGITAL' ELSE NULL END, mg2.dxcc, c2.mode) "
        "ORDER BY c1.start_time DESC"))
    {
        qWarning() << "Cannot prepare duplicate query:" << query.lastError();
        ui->statusLabel->setText(tr("Error: could not prepare search query."));
        return;
    }

    if (!query.exec())
    {
        qWarning() << "Cannot execute duplicate query:" << query.lastError();
        ui->statusLabel->setText(tr("Error: could not execute search query."));
        return;
    }

    /*
     * Collect all pairs and contact data, then use union-find to group contacts
     * that are all duplicates of each other.  This means three contacts A, B, C
     * that are mutually duplicate appear as one group of three rows rather than
     * three separate pairs.
     */
    QMap<int, QList<QVariant>> contactData; // id -> 11-element field list
    QList<QPair<int,int>> pairs;

    while (query.next())
    {
        const int id1 = query.value(0).toInt();
        const int id2 = query.value(11).toInt();

        if (!contactData.contains(id1))
        {
            contactData[id1] = {
                query.value(0),  query.value(1),  query.value(2),
                query.value(3),  query.value(4),  query.value(5),
                query.value(6),  query.value(7),  query.value(8),
                query.value(9),  query.value(10)
            };
        }
        if (!contactData.contains(id2))
        {
            contactData[id2] = {
                query.value(11), query.value(12), query.value(13),
                query.value(14), query.value(15), query.value(16),
                query.value(17), query.value(18), query.value(19),
                query.value(20), query.value(21)
            };
        }
        pairs.append({id1, id2});
    }

    // Union-Find: path-compressed
    QMap<int,int> parent;
    for (int id : contactData.keys())
        parent[id] = id;

    std::function<int(int)> findRoot = [&](int x) -> int
    {
        if (parent[x] != x) parent[x] = findRoot(parent[x]);
        return parent[x];
    };

    for (const QPair<int,int> &p : pairs)
    {
        const int rx = findRoot(p.first);
        const int ry = findRoot(p.second);
        if (rx != ry) parent[rx] = ry;
    }

    // Build groups: root -> sorted list of IDs (ascending = lowest ID first)
    QMap<int, QList<int>> groups;
    for (int id : contactData.keys())
        groups[findRoot(id)].append(id);

    for (QList<int> &g : groups)
        std::sort(g.begin(), g.end());

    // Persist groups for use by mergeGroups()
    duplicateGroups.clear();
    for (const QList<int> &g : groups)
        duplicateGroups.append(g);

    // Populate table
    groupFirstRows.clear();
    int rowIndex      = 0;
    int groupColorIdx = 0;

    // Use the same two palette roles Qt uses for alternatingRowColors so the
    // group shading follows the active theme (native / light / dark) automatically.
    const QColor colorBase = ui->tableWidget->palette().color(QPalette::Base);
    const QColor colorAlt  = ui->tableWidget->palette().color(QPalette::AlternateBase);

    ui->tableWidget->blockSignals(true);

    for (const QList<int> &group : groups)
    {
        const QColor &bgColor = (groupColorIdx % 2 == 0) ? colorBase : colorAlt;
        groupFirstRows.append(rowIndex);
        bool isFirst = true;
        for (int id : group)
        {
            ui->tableWidget->insertRow(rowIndex);
            addRow(rowIndex, contactData.value(id), bgColor, !isFirst);
            rowIndex++;
            isFirst = false;
        }
        groupColorIdx++;
    }

    ui->tableWidget->blockSignals(false);

    const int groupCount   = groups.size();
    const int contactCount = contactData.size();

    if (groupCount == 0)
    {
        ui->statusLabel->setText(tr("No duplicate contacts found."));
    }
    else
    {
        ui->statusLabel->setText(
            tr("Found %1 duplicate group(s) involving %2 contact(s). "
               "All but the first in each group are pre-selected for removal.")
            .arg(groupCount).arg(contactCount));
    }

    ui->mergeButton->setEnabled(groupCount > 0);
    ui->tableWidget->resizeColumnsToContents();
    updateRemoveButton();
}

void DuplicateContactsDialog::addRow(int row, const QList<QVariant> &data,
                                      const QColor &bgColor, bool checked)
{
    FCT_IDENTIFICATION;

    auto makeItem = [&](const QString &text, int align = Qt::AlignLeft | Qt::AlignVCenter)
    {
        QTableWidgetItem *item = new QTableWidgetItem(text);
        item->setBackground(bgColor);
        item->setTextAlignment(align);
        return item;
    };

    // Col 0: Delete checkbox — ID stored in UserRole
    QTableWidgetItem *checkItem = new QTableWidgetItem();
    checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    checkItem->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    checkItem->setData(Qt::UserRole, data[D_ID].toInt());
    checkItem->setBackground(bgColor);
    checkItem->setTextAlignment(Qt::AlignCenter);
    ui->tableWidget->setItem(row, COL_DELETE, checkItem);

    ui->tableWidget->setItem(row, COL_ID,
        makeItem(data[D_ID].toString(), Qt::AlignRight | Qt::AlignVCenter));
    ui->tableWidget->setItem(row, COL_DATETIME,
        makeItem(data[D_TIME].toString()));
    ui->tableWidget->setItem(row, COL_CALLSIGN,
        makeItem(data[D_CALL].toString()));
    ui->tableWidget->setItem(row, COL_MODE,
        makeItem(data[D_MODE].toString()));
    ui->tableWidget->setItem(row, COL_MODEGROUP,
        makeItem(data[D_MODEGROUP].toString()));
    ui->tableWidget->setItem(row, COL_BAND,
        makeItem(data[D_BAND].toString()));
    ui->tableWidget->setItem(row, COL_FREQ,
        makeItem(data[D_FREQ].toString(), Qt::AlignRight | Qt::AlignVCenter));
    ui->tableWidget->setItem(row, COL_QSL,
        makeItem(data[D_QSL].toString(), Qt::AlignCenter));
    ui->tableWidget->setItem(row, COL_LOTW,
        makeItem(data[D_LOTW].toString(), Qt::AlignCenter));
    ui->tableWidget->setItem(row, COL_EQSL,
        makeItem(data[D_EQSL].toString(), Qt::AlignCenter));
    ui->tableWidget->setItem(row, COL_MYCALL,
        makeItem(data[D_MYCALL].toString()));
}

static const QStringList QSL_STATUS_PRIORITY = {"Y", "R", "I", "Q", "N"};

static bool qslStatusBetter(const QString &candidate, const QString &current)
{
    int ic = QSL_STATUS_PRIORITY.indexOf(candidate.toUpper());
    int ii = QSL_STATUS_PRIORITY.indexOf(current.toUpper());
    if (ic < 0) return false;               // unknown candidate — don't replace
    if (ii < 0) return true;                // unknown current   — take candidate
    return ic < ii;                         // lower index = higher priority
}

static bool freqMorePrecise(const QVariant &candidate, const QVariant &current)
{
    const double candVal = candidate.toDouble();
    if (candVal == 0.0 || candidate.isNull() || candidate.toString().isEmpty()) return false;
    const double currVal = current.toDouble();
    if (currVal == 0.0 || current.isNull()  || current.toString().isEmpty())   return true;
    auto decimals = [](const QString &s) -> int
    {
        const int dot = s.indexOf('.');
        return dot >= 0 ? s.length() - dot - 1 : 0;
    };
    return decimals(candidate.toString()) > decimals(current.toString());
}

static bool dateLater(const QString &candidate, const QString &current)
{
    if (candidate.isEmpty()) return false;
    if (current.isEmpty())   return true;
    return candidate > current;             // ISO / ADIF date strings sort lexicographically
}

static bool isSkipField(const QString &name)
{
    static const QSet<QString> SKIP = {
        "id", "start_time", "end_time", "callsign", "band", "sat_name",
        "station_callsign",
        // My-station fields
        "my_dxcc", "my_country", "my_country_intl", "my_gridsquare",
        "my_name",  "my_name_intl", "my_city", "my_city_intl",
        "my_iota",  "my_pota_ref",  "my_sota_ref",
        "my_sig",   "my_sig_intl",  "my_sig_info", "my_sig_info_intl",
        "my_vucc_grids", "my_itu_zone", "my_cq_zone", "my_cnty", "my_darc_dok",
        "operator",
        // Handled explicitly below
        "mode", "submode", "freq", "freq_rx",
        "qsl_rcvd", "qsl_sent",
        "lotw_qsl_rcvd", "lotw_qsl_sent",
        "eqsl_qsl_rcvd", "eqsl_qsl_sent",
        "qsl_rdate", "qsl_sdate",
        "lotw_qslrdate", "lotw_qslsdate",
        "eqsl_qslrdate", "eqsl_qslsdate",
        // JSON blob — too complex to merge
        "fields"
    };
    return SKIP.contains(name);
}

bool DuplicateContactsDialog::mergeGroupById(const QList<int> &group, int &removedOut)
{
    FCT_IDENTIFICATION;

    removedOut = 0;
    if (group.size() < 2) return true;

     QStringList idStrs;
    for (int id : group) idStrs << QString::number(id);

    QSqlQuery sel;
    if (!sel.exec("SELECT * FROM contacts WHERE id IN (" + idStrs.join(",") + ") ORDER BY id ASC"))
    {
        qWarning() << "mergeGroupById: SELECT failed:" << sel.lastError();
        return false;
    }

    QList<QSqlRecord> recs;
    while (sel.next())
        recs.append(sel.record());

    if (recs.size() < 2)
        return true; // nothing to do (maybe already deleted)

    const QSqlRecord &base = recs[0];
    const int baseId = base.value("id").toInt();

    QVariantMap updates;

    for (int di = 1; di < recs.size(); di++)
    {
        const QSqlRecord &dup = recs[di];

        for (const QString &f : {"qsl_rcvd", "lotw_qsl_rcvd", "eqsl_qsl_rcvd"})
        {
            const QString cur  = updates.contains(f) ? updates[f].toString()
                                                      : base.value(f).toString();
            const QString cand = dup.value(f).toString();
            if (qslStatusBetter(cand, cur))
                updates[f] = cand;
        }

        for (const QString &f : {"qsl_sent", "lotw_qsl_sent", "eqsl_qsl_sent"})
        {
            const QString cur  = updates.contains(f) ? updates[f].toString()
                                                      : base.value(f).toString();
            const QString cand = dup.value(f).toString();
            if (qslStatusBetter(cand, cur))
                updates[f] = cand;
        }

        for (const QString &f : {"qsl_rdate", "qsl_sdate",
                                  "lotw_qslrdate", "lotw_qslsdate",
                                  "eqsl_qslrdate", "eqsl_qslsdate"})
        {
            const QString cur  = updates.contains(f) ? updates[f].toString()
                                                      : base.value(f).toString();
            const QString cand = dup.value(f).toString();
            if (dateLater(cand, cur))
                updates[f] = cand;
        }

        for (const QString &f : {"freq", "freq_rx"})
        {
            const QVariant cur  = updates.contains(f) ? updates[f]
                                                       : base.value(f);
            const QVariant cand = dup.value(f);
            if (freqMorePrecise(cand, cur))
                updates[f] = cand;
        }

        {
            const QString baseMode = base.value("mode").toString();
            const QString dupMode  = dup.value("mode").toString();
            const QString baseSub  = updates.contains("submode") ? updates["submode"].toString()
                                                                  : base.value("submode").toString();
            const QString dupSub   = dup.value("submode").toString();
            if (baseMode == dupMode && baseSub.isEmpty() && !dupSub.isEmpty())
                updates["submode"] = dupSub;
        }

        for (int col = 0; col < dup.count(); col++)
        {
            const QString fname = dup.fieldName(col);
            if (isSkipField(fname)) continue;

            const QVariant cur  = updates.contains(fname) ? updates[fname]
                                                           : base.value(fname);
            const QVariant cand = dup.value(fname);
            if ((cur.isNull() || cur.toString().isEmpty()) &&
                !cand.isNull() && !cand.toString().isEmpty())
            {
                updates[fname] = cand;
            }
        }
    }

    if (!updates.isEmpty())
    {
        QStringList setClauses;
        QVariantList bindValues;
        for (auto it = updates.constBegin(); it != updates.constEnd(); ++it)
        {
            setClauses  << QString("\"%1\"=?").arg(it.key());
            bindValues  << it.value();
        }

        QSqlQuery upd;
        if (!upd.prepare(QString("UPDATE contacts SET %1 WHERE id=?")
                         .arg(setClauses.join(", "))))
        {
            qWarning() << "mergeGroupById: UPDATE prepare failed:" << upd.lastError();
            return false;
        }
        for (const QVariant &v : bindValues)
            upd.addBindValue(v);
        upd.addBindValue(baseId);

        if (!upd.exec())
        {
            qWarning() << "mergeGroupById: UPDATE exec failed:" << upd.lastError();
            return false;
        }
    }

    // Delete the duplicates (all but the base)
    QStringList dupIds;
    for (int i = 1; i < recs.size(); i++)
        dupIds << QString::number(recs[i].value("id").toInt());

    QSqlQuery del;
    if (!del.exec("DELETE FROM contacts WHERE id IN (" + dupIds.join(",") + ")"))
    {
        qWarning() << "mergeGroupById: DELETE failed:" << del.lastError();
        return false;
    }

    removedOut = del.numRowsAffected();
    return true;
}

void DuplicateContactsDialog::mergeGroups()
{
    FCT_IDENTIFICATION;

    if (duplicateGroups.isEmpty())
    {
        QMessageBox::information(this, tr("Nothing to Merge"),
                                 tr("No duplicate groups are currently shown."));
        return;
    }

    int totalRemoved = 0;
    for (const QList<int> &g : duplicateGroups)
        totalRemoved += g.size() - 1;

    const QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Confirm Merge"),
        tr("Merge %1 duplicate group(s) (%2 contact(s) will be removed)?\n\n"
           "The earliest contact in each group (lowest ID) is kept.\n"
           "Fields are merged using these rules:\n"
           "  • QSL status: best value wins (Y \u003e R \u003e I \u003e Q \u003e N)\n"
           "  • QSL dates: fill blank; prefer latest when both present\n"
           "  • Frequency: most-precise (most decimal digits) wins\n"
           "  • Submode: copied from duplicate only when modes match exactly\n"
           "  • Other fields: copied from duplicate only when kept contact is blank\n\n"
           "This cannot be undone.")
            .arg(duplicateGroups.size()).arg(totalRemoved),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes) return;

    QSqlDatabase::database().transaction();

    int mergedGroups = 0;
    int removedContacts = 0;
    bool anyFailed = false;

    for (const QList<int> &group : duplicateGroups)
    {
        int removed = 0;
        if (mergeGroupById(group, removed))
        {
            mergedGroups++;
            removedContacts += removed;
        }
        else
        {
            anyFailed = true;
            break;
        }
    }

    if (anyFailed)
    {
        QSqlDatabase::database().rollback();
        QMessageBox::critical(this, tr("Error"),
                              tr("Merge failed. No changes were made."));
        return;
    }

    QSqlDatabase::database().commit();
    QMessageBox::information(this, tr("Merge Complete"),
                             tr("Merged %1 group(s), removed %2 contact(s).")
                             .arg(mergedGroups).arg(removedContacts));
    findDuplicates();
}

void DuplicateContactsDialog::autoSelectDuplicates()
{
    FCT_IDENTIFICATION;

    ui->tableWidget->blockSignals(true);

    // Uncheck the first row of each group (keep the original); check all others
    const QSet<int> firstRows(groupFirstRows.begin(), groupFirstRows.end());
    for (int row = 0; row < ui->tableWidget->rowCount(); row++)
    {
        QTableWidgetItem *item = ui->tableWidget->item(row, COL_DELETE);
        if (!item) continue;
        item->setCheckState(firstRows.contains(row) ? Qt::Unchecked : Qt::Checked);
    }

    ui->tableWidget->blockSignals(false);
    updateRemoveButton();
}

void DuplicateContactsDialog::unselectAll()
{
    FCT_IDENTIFICATION;

    ui->tableWidget->blockSignals(true);
    for (int row = 0; row < ui->tableWidget->rowCount(); row++)
    {
        QTableWidgetItem *item = ui->tableWidget->item(row, COL_DELETE);
        if (item) item->setCheckState(Qt::Unchecked);
    }
    ui->tableWidget->blockSignals(false);
    updateRemoveButton();
}

void DuplicateContactsDialog::removeChecked()
{
    FCT_IDENTIFICATION;

    const QSet<int> idsToDelete = getCheckedIds();

    if (idsToDelete.isEmpty())
    {
        QMessageBox::information(this, tr("No Selection"),
                                 tr("No contacts are checked for removal."));
        return;
    }

    const QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Confirm Removal"),
        tr("Permanently remove %1 selected contact(s)? This cannot be undone.")
            .arg(idsToDelete.size()),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes) return;

    QStringList idStrings;
    for (int id : idsToDelete)
        idStrings << QString::number(id);

    QSqlQuery delQuery;
    if (!delQuery.exec(QString("DELETE FROM contacts WHERE id IN (%1)").arg(idStrings.join(","))))
    {
        qWarning() << "Cannot delete contacts:" << delQuery.lastError();
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to remove contacts: %1").arg(delQuery.lastError().text()));
        return;
    }

    const int removed = delQuery.numRowsAffected();
    QMessageBox::information(this, tr("Done"),
                             tr("Removed %1 contact(s).").arg(removed));

    findDuplicates();
}

void DuplicateContactsDialog::onItemChanged(QTableWidgetItem *item)
{
    FCT_IDENTIFICATION;

    if (!item || item->column() != COL_DELETE) return;
    updateRemoveButton();
}

void DuplicateContactsDialog::updateRemoveButton()
{
    FCT_IDENTIFICATION;

    int count = 0;
    for (int row = 0; row < ui->tableWidget->rowCount(); row++)
    {
        const QTableWidgetItem *item = ui->tableWidget->item(row, COL_DELETE);
        if (item && item->checkState() == Qt::Checked)
            count++;
    }

    ui->removeButton->setText(tr("Remove Checked (%1)").arg(count));
    ui->removeButton->setEnabled(count > 0);
}

QSet<int> DuplicateContactsDialog::getCheckedIds() const
{
    FCT_IDENTIFICATION;

    QSet<int> ids;
    for (int row = 0; row < ui->tableWidget->rowCount(); row++)
    {
        const QTableWidgetItem *item = ui->tableWidget->item(row, COL_DELETE);
        if (item && item->checkState() == Qt::Checked)
            ids.insert(item->data(Qt::UserRole).toInt());
    }
    return ids;
}
