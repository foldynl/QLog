#include "ui/AlertRuleLogStatusDialog.h"
#include "ui_AlertRuleLogStatusDialog.h"
#include "core/debug.h"
#include "data/Dxcc.h"

MODULE_IDENTIFICATION("qlog.ui.alertrulelogstatusdialog");

AlertRuleLogStatusDialog::AlertRuleLogStatusDialog(int status, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AlertRuleLogStatusDialog)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->newEntityCheckbox->setChecked(status & DxccStatus::NewEntity);
    ui->newBandCheckbox->setChecked(status & DxccStatus::NewBand);
    ui->newModeCheckbox->setChecked(status & DxccStatus::NewMode);
    ui->newSlotCheckbox->setChecked(status & DxccStatus::NewSlot);
    ui->workedCheckbox->setChecked(status & DxccStatus::Worked);
    ui->confirmedCheckbox->setChecked(status & DxccStatus::Confirmed);
}

AlertRuleLogStatusDialog::~AlertRuleLogStatusDialog()
{
    FCT_IDENTIFICATION;

    delete ui;
}

int AlertRuleLogStatusDialog::status() const
{
    FCT_IDENTIFICATION;

    int status = 0;
    if ( ui->newEntityCheckbox->isChecked() ) status |= DxccStatus::NewEntity;
    if ( ui->newBandCheckbox->isChecked() ) status |= DxccStatus::NewBand;
    if ( ui->newModeCheckbox->isChecked() ) status |= DxccStatus::NewMode;
    if ( ui->newSlotCheckbox->isChecked() ) status |= DxccStatus::NewSlot;
    if ( ui->workedCheckbox->isChecked() ) status |= DxccStatus::Worked;
    if ( ui->confirmedCheckbox->isChecked() ) status |= DxccStatus::Confirmed;
    return status;
}
