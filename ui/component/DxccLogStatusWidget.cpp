#include <QSignalBlocker>
#include "ui/component/DxccLogStatusWidget.h"
#include "ui_DxccLogStatusWidget.h"
#include "ui/AlertRuleLogStatusDialog.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.component.dxcclogstatuswidget");

DxccLogStatusWidget::DxccLogStatusWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DxccLogStatusWidget)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);
    const QSignalBlocker goalBlocker(ui->goalCombo);
    const QSignalBlocker contextBlocker(ui->contextCombo);
    ui->goalCombo->addItem(tr("DXCC has not been worked"), static_cast<int>(AlertDxccGoal::Worked));
    ui->goalCombo->addItem(tr("DXCC has not been confirmed"), static_cast<int>(AlertDxccGoal::Confirmed));
    ui->goalCombo->addItem(tr("Selected DXCC statuses..."), SELECTED_DXCC_STATUSES);
    ui->goalCombo->setItemData(0, tr("Match if no qualifying QSO exists in the log."), Qt::ToolTipRole);
    ui->goalCombo->setItemData(1, tr("Match if no qualifying QSO is confirmed using the DXCC confirmation sources selected in Settings."), Qt::ToolTipRole);
    ui->goalCombo->setItemData(2, tr("Match any of the individually selected DXCC statuses."), Qt::ToolTipRole);
    ui->contextCombo->addItem(tr("All bands and modes"), static_cast<int>(AlertDxccContext::Anywhere));
    ui->contextCombo->addItem(tr("Spot's band, any mode"), static_cast<int>(AlertDxccContext::Band));
    ui->contextCombo->addItem(tr("Any band, spot's mode"), static_cast<int>(AlertDxccContext::Mode));
    ui->contextCombo->addItem(tr("Spot's band and mode"), static_cast<int>(AlertDxccContext::BandMode));
    setCondition(DxccLogStatusCondition());
    setFocusProxy(ui->enabledCheckBox);
}

DxccLogStatusWidget::~DxccLogStatusWidget()
{
    FCT_IDENTIFICATION;

    delete ui;
}

void DxccLogStatusWidget::setCondition(const DxccLogStatusCondition &condition)
{
    FCT_IDENTIFICATION;

    currentCondition = condition;
    const bool enabled = condition.goalSet || condition.contextSet || condition.mask != DxccStatus::All;
    if ( !enabled )
    {
        currentCondition = DxccLogStatusCondition();
        currentCondition.goalSet = currentCondition.contextSet = true;
    }
    ui->enabledCheckBox->setChecked(enabled);
    updateUi();
}

DxccLogStatusCondition DxccLogStatusWidget::condition() const
{
    FCT_IDENTIFICATION;

    return ui->enabledCheckBox->isChecked() ? currentCondition : DxccLogStatusCondition();
}

void DxccLogStatusWidget::updateUi()
{
    FCT_IDENTIFICATION;

    const QSignalBlocker goalBlocker(ui->goalCombo);
    const QSignalBlocker contextBlocker(ui->contextCombo);
    const bool enabled = ui->enabledCheckBox->isChecked();
    const bool invalid = currentCondition.isInvalid();
    AlertDxccGoal goal = static_cast<AlertDxccGoal>(currentCondition.goal);
    AlertDxccContext context = static_cast<AlertDxccContext>(currentCondition.context);
    const bool selected = !currentCondition.hasCondition() && !invalid
                          && !legacyStatusAsNewCondition(currentCondition.mask, goal, context);
    int invalidIndex = ui->goalCombo->findData(INVALID_DXCC_CONDITION);
    if ( invalid && invalidIndex < 0 )
    {
        ui->goalCombo->addItem(tr("Invalid log status condition"), INVALID_DXCC_CONDITION);
        invalidIndex = ui->goalCombo->count() - 1;
    }
    else if ( !invalid && invalidIndex >= 0 )
        ui->goalCombo->removeItem(invalidIndex);

    ui->goalCombo->setCurrentIndex(ui->goalCombo->findData(
        invalid ? INVALID_DXCC_CONDITION : selected ? SELECTED_DXCC_STATUSES : static_cast<int>(goal)));
    ui->contextCombo->setCurrentIndex(ui->contextCombo->findData(static_cast<int>(context)));
    ui->goalLabel->setEnabled(enabled);
    ui->goalCombo->setEnabled(enabled);
    ui->contextLabel->setEnabled(enabled);
    ui->contextCombo->setEnabled(enabled);
    ui->contextLabel->setVisible(!invalid && !selected);
    ui->contextCombo->setVisible(!invalid && !selected);
    ui->editButton->setVisible(selected);
    ui->editButton->setEnabled(enabled && selected);
}

void DxccLogStatusWidget::conditionChanged()
{
    FCT_IDENTIFICATION;

    const int goal = ui->goalCombo->currentData().toInt();
    if ( goal == SELECTED_DXCC_STATUSES )
    {
        editLogStatuses();
        return;
    }
    if ( goal == INVALID_DXCC_CONDITION ) return;

    currentCondition.goal = goal;
    currentCondition.context = ui->contextCombo->currentData().toInt();
    currentCondition.goalSet = currentCondition.contextSet = true;
    updateUi();
}

void DxccLogStatusWidget::editLogStatuses()
{
    FCT_IDENTIFICATION;

    AlertRuleLogStatusDialog dialog(currentCondition.mask, this);
    if ( dialog.exec() == QDialog::Accepted )
        currentCondition = DxccLogStatusCondition(dialog.status());
    updateUi();
}

bool DxccLogStatusWidget::legacyStatusAsNewCondition(int status,
                                                   AlertDxccGoal &goal,
                                                   AlertDxccContext &context) const
{
    FCT_IDENTIFICATION;

    goal = AlertDxccGoal::Worked;

    switch ( status )
    {
        case DxccStatus::NewEntity:
            context = AlertDxccContext::Anywhere;
            return true;
        case DxccStatus::NewEntity | DxccStatus::NewBand:
            context = AlertDxccContext::Band;
            return true;
        case DxccStatus::NewEntity | DxccStatus::NewMode:
            context = AlertDxccContext::Mode;
            return true;
        case DxccStatus::NewEntity | DxccStatus::NewBand
             | DxccStatus::NewMode | DxccStatus::NewSlot:
            context = AlertDxccContext::BandMode;
            return true;
        case DxccStatus::NewEntity | DxccStatus::NewBand
             | DxccStatus::NewMode | DxccStatus::NewSlot | DxccStatus::Worked:
            goal = AlertDxccGoal::Confirmed;
            context = AlertDxccContext::BandMode;
            return true;
        default:
            return false;
    }
}
