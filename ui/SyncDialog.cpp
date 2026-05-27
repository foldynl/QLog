#include <QFileDialog>
#include <QMessageBox>

#include "SyncDialog.h"
#include "ui_SyncDialog.h"
#include "core/ContactSync.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.syncdialog");

SyncDialog::SyncDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::SyncDialog)
{
    ui->setupUi(this);

    ContactSync *sync = ContactSync::instance();

    ui->enabledCheckBox->setChecked(sync->isEnabled());
    ui->folderLineEdit->setText(sync->folder());

    ui->nodeIdLineEdit->setText(sync->selfNodeId());

    // The combo's index matches DuplicatePolicy's integer value:
    //   0 = Skip, 1 = Merge. The .ui file lists Merge first so we map.
    const int policyValue = static_cast<int>(sync->duplicatePolicy());
    ui->dupePolicyComboBox->setCurrentIndex(policyValue == 0 ? 1 : 0);

    refreshStatus();
    applyEnabledUiState();

    connect(ui->enabledCheckBox,  &QCheckBox::toggled, this, &SyncDialog::onEnabledToggled);
    connect(ui->browseButton,     &QPushButton::clicked, this, &SyncDialog::onBrowseClicked);
    connect(ui->folderLineEdit,   &QLineEdit::editingFinished, this, &SyncDialog::onFolderEdited);
    connect(ui->saveNodeIdButton, &QPushButton::clicked, this, &SyncDialog::onSaveNodeIdClicked);
    connect(ui->dupePolicyComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SyncDialog::onDupePolicyChanged);
    connect(ui->flushNowButton,   &QPushButton::clicked, this, &SyncDialog::onFlushNowClicked);
    connect(ui->pullNowButton,    &QPushButton::clicked, this, &SyncDialog::onPullNowClicked);
    connect(sync, &ContactSync::flushed, this, [this](int, int) { refreshStatus(); });
    connect(sync, &ContactSync::pulled,  this, [this](int)      { refreshStatus(); });
    connect(sync, &ContactSync::flushFailed, this, [this](const QString &m)
    {
        QMessageBox::warning(this, tr("Sync"), m);
    });
    connect(sync, &ContactSync::pullFailed, this, [this](const QString &m)
    {
        QMessageBox::warning(this, tr("Sync"), m);
    });
    connect(sync, &ContactSync::busyChanged, this, [this](bool busy)
    {
        // Disable buttons while a cycle is running; re-enable when it ends.
        ui->flushNowButton->setEnabled(!busy && ui->enabledCheckBox->isChecked()
                                       && !ui->folderLineEdit->text().trimmed().isEmpty());
        ui->pullNowButton->setEnabled(!busy && ui->enabledCheckBox->isChecked()
                                       && !ui->folderLineEdit->text().trimmed().isEmpty());
    });
}

SyncDialog::~SyncDialog()
{
    delete ui;
}

void SyncDialog::applyEnabledUiState()
{
    const bool on = ui->enabledCheckBox->isChecked();
    const bool ready = on && !ui->folderLineEdit->text().trimmed().isEmpty();
    ui->flushNowButton->setEnabled(ready);
    ui->pullNowButton->setEnabled(ready);
}

void SyncDialog::onEnabledToggled(bool checked)
{
    FCT_IDENTIFICATION;

    QString err;
    if ( !ContactSync::instance()->setEnabled(checked, &err) )
    {
        QMessageBox::warning(this, tr("Sync"), err);
        // Revert checkbox without re-triggering the slot
        ui->enabledCheckBox->blockSignals(true);
        ui->enabledCheckBox->setChecked(false);
        ui->enabledCheckBox->blockSignals(false);
    }
    applyEnabledUiState();
    refreshStatus();
}

void SyncDialog::onBrowseClicked()
{
    const QString chosen = QFileDialog::getExistingDirectory(this,
                                                             tr("Choose Sync Folder"),
                                                             ui->folderLineEdit->text());
    if ( chosen.isEmpty() )
        return;
    ui->folderLineEdit->setText(chosen);
    onFolderEdited();
}

void SyncDialog::onFolderEdited()
{
    FCT_IDENTIFICATION;

    QString err;
    if ( !ContactSync::instance()->setFolder(ui->folderLineEdit->text().trimmed(), &err) )
        QMessageBox::warning(this, tr("Sync"), err);
    applyEnabledUiState();
    refreshStatus();
}

void SyncDialog::onSaveNodeIdClicked()
{
    FCT_IDENTIFICATION;

    QString err;
    if ( !ContactSync::instance()->setSelfNodeId(ui->nodeIdLineEdit->text(), &err) )
    {
        QMessageBox::warning(this, tr("Sync"), err);
        ui->nodeIdLineEdit->setText(ContactSync::instance()->selfNodeId());
        return;
    }
    refreshStatus();
}

void SyncDialog::onDupePolicyChanged(int index)
{
    FCT_IDENTIFICATION;

    // Combo: 0 = Merge, 1 = Skip; enum: 0 = Skip, 1 = Merge.
    const auto policy = (index == 0) ? ContactSync::DuplicatePolicy::Merge
                                     : ContactSync::DuplicatePolicy::Skip;
    ContactSync::instance()->setDuplicatePolicy(policy);
}

void SyncDialog::onFlushNowClicked()
{
    FCT_IDENTIFICATION;

    // Fire-and-forget: the cycle runs on a worker thread and posts back via
    // flushed / flushFailed / busyChanged signals connected in the ctor.
    ContactSync::instance()->requestFlush();
}

void SyncDialog::onPullNowClicked()
{
    FCT_IDENTIFICATION;

    ContactSync::instance()->requestPull();
}

void SyncDialog::refreshStatus()
{
    ContactSync *sync = ContactSync::instance();

    const QDateTime lastFlush = sync->lastFlushTime();
    ui->lastFlushValueLabel->setText(lastFlush.isValid()
                                     ? lastFlush.toLocalTime().toString(Qt::ISODate)
                                     : tr("never"));

    const QDateTime lastPull = sync->lastPullTime();
    ui->lastPullValueLabel->setText(lastPull.isValid()
                                    ? lastPull.toLocalTime().toString(Qt::ISODate)
                                    : tr("never"));

    ui->pendingValueLabel->setText(QString::number(sync->pendingChangesCount()));
}
