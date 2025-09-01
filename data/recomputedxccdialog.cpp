// RecomputeDxccDialog.cpp
#include "RecomputeDxccDialog.h"
#include "Data.h"
#include <QVBoxLayout>
#include <QProgressBar>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QThread>
#include <QCheckBox>
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.antprofile");

static QDateTime parseIsoDateTimeUtc(const QString &s)
{
    FCT_IDENTIFICATION;

#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(s, Qt::ISODate);
#else
    QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
#endif
    return dt.toUTC();
}

RecomputeDxccDialog::RecomputeDxccDialog(QWidget *parent)
    : QDialog(parent)
{
    FCT_IDENTIFICATION;

    setWindowTitle("Recompute DXCC");
    resize(720, 420);

    auto *v = new QVBoxLayout(this);

    onlyMissingCheck_ = new QCheckBox(tr("Only records with empty DXCC"), this);
    onlyMissingCheck_->setToolTip(tr("If checked, only contacts where DXCC is NULL or 0 will be processed."));
    onlyMissingCheck_->setChecked(false); // default off (process all)
    v->addWidget(onlyMissingCheck_);

    m_bar = new QProgressBar(this);
    m_bar->setRange(0, 0);
    v->addWidget(m_bar);

    m_stats = new QLabel("Processed: 0 | Updated: 0", this);
    v->addWidget(m_stats);

    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);
    v->addWidget(m_log, 1);

    auto* btns = new QHBoxLayout();
    m_startBtn  = new QPushButton(tr("Start"), this);
    m_cancelBtn  = new QPushButton(tr("Close"), this);
    btns->addStretch();
    btns->addWidget(m_startBtn);
    btns->addWidget(m_cancelBtn);

    connect(m_startBtn, &QPushButton::clicked, this, &RecomputeDxccDialog::onStart);
    connect(m_cancelBtn, &QPushButton::clicked, this, &RecomputeDxccDialog::onCancel);

    v->addLayout(btns);

    // Fire the job in a thread so UI stays alive
   // QTimer::singleShot(0, this, &RecomputeDxccDialog::runJob);
}

RecomputeDxccDialog::~RecomputeDxccDialog()
{
    FCT_IDENTIFICATION;

    m_cancel.store(true);
}

void RecomputeDxccDialog::onCancel()
{
    FCT_IDENTIFICATION;

    m_cancel.store(true);
    m_cancelBtn->setEnabled(false);
    accept();
}

void RecomputeDxccDialog::onStart()
{
    FCT_IDENTIFICATION;

    m_startBtn->setEnabled(false);
    onlyMissingCheck_->setEnabled(false);
    m_log->clear();

    QTimer::singleShot(0, this, &RecomputeDxccDialog::runJob);
}

void RecomputeDxccDialog::logLine(const QString &line)
{
    FCT_IDENTIFICATION;

    m_log->append(line);
}

void RecomputeDxccDialog::runJob()
{
    FCT_IDENTIFICATION;

    stopRequested = false;
    m_log->clear();

    // nice header in the log
    m_log->append(QStringLiteral("=== Recompute DXCC started @ %1 ===")
                          .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));

    QSqlQuery countQuery;
    QString countSql = "SELECT COUNT(*) FROM contacts";
    if (onlyMissingCheck_->isChecked()) {
        countSql += " WHERE dxcc = 0";  // per your request
    }
    if (!countQuery.exec(countSql) || !countQuery.next()) {
        m_log->append("Failed to count contacts: " + countQuery.lastError().text());
        m_stats->setText("Failed.");
        return;
    }
    int total = countQuery.value(0).toInt();
    if (total == 0) {
        m_stats->setText("No contacts found to process.");
        m_log->append("No contacts match the current filter. Nothing to do.");
        return;
    }

    m_bar->setRange(0, total);
    m_bar->setValue(0);

    QString sql = "SELECT id, callsign, start_time, dxcc, country FROM contacts";
    if (onlyMissingCheck_->isChecked()) {
        sql += " WHERE dxcc = 0 or dxcc is null ";  // per your request
    }
    sql += " ORDER BY id"; // keeps UI updates smoother/predictable

    QSqlQuery q;
    if (!q.exec(sql)) {
        m_log->append("Failed to query contacts: " + q.lastError().text());
        m_stats->setText("Failed.");
        return;
    }

    int processed = 0, updated = 0;

    while (q.next() && !stopRequested) {
        const int id           = q.value(0).toInt();
        const QString callsign = q.value(1).toString();
        const QString startIso = q.value(2).toString();
        const int oldDxcc      = q.value(3).toInt();
        const QString oldCtry  = q.value(4).toString();

        // robust ISO parsing + normalize to UTC
        QDateTime dt = QDateTime::fromString(startIso, Qt::ISODateWithMs);
        if (!dt.isValid()) dt = QDateTime::fromString(startIso, Qt::ISODate);
        if (!dt.isValid()) dt = QDateTime::fromString(startIso, "yyyy-MM-dd'T'HH:mm:sszzz");
        if (!dt.isValid()) dt = QDateTime::currentDateTimeUtc();
        dt = dt.toUTC();

        const DxccEntity ent = Data::instance()->lookupCallsign(callsign, dt);

        // update only if something actually changed and we have a meaningful dxcc
        const bool changed = (ent.dxcc != 0) && (ent.dxcc != oldDxcc);
        if (changed) {
            QSqlQuery u;
            u.prepare("UPDATE contacts SET dxcc = ?, country = ?, cont = ?, cqz = ?, ituz = ?, pfx = ? WHERE id = ?");
            u.addBindValue(ent.dxcc);
            u.addBindValue(ent.country);
            u.addBindValue(ent.cont);
            u.addBindValue(ent.cqz);
            u.addBindValue(ent.ituz);
            u.addBindValue(ent.prefix);
            u.addBindValue(id);

            if (!u.exec()) {
                m_log->append(QString("Failed to update %1 (id=%2): %3")
                                      .arg(callsign).arg(id).arg(u.lastError().text()));
            } else {
                ++updated;
                m_log->append(QString("Updated %1 (id=%2) @ %3   %4 -> %5 (DXCC %6)")
                                      .arg(callsign)
                                      .arg(id)
                                      .arg(dt.toString(Qt::ISODate))
                                      .arg(oldCtry.isEmpty() ? QStringLiteral("<empty>") : oldCtry)
                                      .arg(ent.country)
                                      .arg(ent.dxcc));
            }
        }

        ++processed;
        m_bar->setValue(processed);
        m_stats->setText(QString("Processed %1/%2, Updated %3")
                                .arg(processed).arg(total).arg(updated));
        QCoreApplication::processEvents();
    }

    // final status
    m_stats->setText(QString("Done. Processed %1, Updated %2").arg(processed).arg(updated));

    // append a summary block to the log (no popup)
    m_log->append(QString());
    m_log->append(QStringLiteral("=== Summary ==="));
    m_log->append(QString("Processed: %1").arg(processed));
    m_log->append(QString("Updated:   %1").arg(updated));
    m_log->append(QStringLiteral("Filter:   %1")
                          .arg(onlyMissingCheck_->isChecked()
                                   ? QStringLiteral("Only records with DXCC = 0")
                                   : QStringLiteral("All records")));
    m_log->append(QStringLiteral("Finished @ %1")
                          .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
}
