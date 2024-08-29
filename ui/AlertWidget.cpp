#include "AlertWidget.h"
#include "ui_AlertWidget.h"
#include "core/debug.h"
#include "AlertSettingDialog.h"
#include "ui/ColumnSettingDialog.h"

MODULE_IDENTIFICATION("qlog.ui.alertwidget");

//Maximal Aging interval is 20s
#define ALERT_AGING_CHECK_TIME 20000

AlertWidget::AlertWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AlertWidget)
{
    FCT_IDENTIFICATION;
    ui->setupUi(this);

    proxyModel = new QSortFilterProxyModel(this);
    alertTableModel = new AlertTableModel(proxyModel);
    proxyModel->setSourceModel(alertTableModel);
    proxyModel->setSortRole(Qt::UserRole);

    ui->alertTableView->setModel(proxyModel);
    ui->alertTableView->setSortingEnabled(true);
    ui->alertTableView->sortByColumn(AlertTableModel::COLUMN_UPDATED, Qt::DescendingOrder);
    ui->alertTableView->horizontalHeader()->setSectionsMovable(true);

    ui->alertTableView->addAction(ui->actionEditRules);
    ui->alertTableView->addAction(ui->actionColumnVisibility);
    ui->alertTableView->addAction(ui->actionClear);

    restoreTableHeaderState();

    ui->clearAlertOlderSpinBox->setValue(settings.value("alert/alert_aging", 0).toInt());

    aging_timer = new QTimer;
    connect(aging_timer, &QTimer::timeout, this, &AlertWidget::alertAging);
    aging_timer->start(ALERT_AGING_CHECK_TIME);
}

AlertWidget::~AlertWidget()
{
    FCT_IDENTIFICATION;

    saveTableHeaderState();

    if ( aging_timer )
    {
        aging_timer->stop();
        aging_timer->deleteLater();
    }

    delete ui;
}

void AlertWidget::addAlert(const SpotAlert &alert)
{
    FCT_IDENTIFICATION;

    alertTableModel->addAlert(alert);
    ui->alertTableView->repaint();
}

void AlertWidget::clearAllAlerts()
{
    FCT_IDENTIFICATION;

    alertTableModel->clear();
    emit alertsCleared();
}

void AlertWidget::entryDoubleClicked(QModelIndex index)
{
    FCT_IDENTIFICATION;

    const QModelIndex &source_index = proxyModel->mapToSource(index);
    const AlertTableModel::AlertTableRecord &record = alertTableModel->getTableRecord(source_index);

    if ( record.alert.source == SpotAlert::WSJTXCQSPOT )
        emit tuneWsjtx(record.alert.wsjtxDecode);
    else
        emit tuneDx(record.alert.callsign,
                    record.alert.freq,
                    record.alert.bandPlanMode);
}

void AlertWidget::alertAgingChanged(int)
{
    FCT_IDENTIFICATION;

    settings.setValue("alert/alert_aging", ui->clearAlertOlderSpinBox->value());
}

void AlertWidget::showEditRules()
{
    FCT_IDENTIFICATION;

    AlertSettingDialog dialog(this);
    dialog.exec();
    emit rulesChanged();
}

void AlertWidget::showColumnVisibility()
{
    FCT_IDENTIFICATION;

    ColumnSettingSimpleDialog dialog(ui->alertTableView);
    dialog.exec();
    saveTableHeaderState();
}

void AlertWidget::alertAging()
{
    FCT_IDENTIFICATION;

    alertTableModel->aging(ui->clearAlertOlderSpinBox->value() * 60);
    ui->alertTableView->repaint();
    emit alertsCleared();
}

void AlertWidget::saveTableHeaderState()
{
    FCT_IDENTIFICATION;

    const QByteArray &state = ui->alertTableView->horizontalHeader()->saveState();
    settings.setValue("alert/state", state);
}

void AlertWidget::restoreTableHeaderState()
{
    FCT_IDENTIFICATION;

    const QVariant &state = settings.value("alert/state");

    if (!state.isNull())
    {
        ui->alertTableView->horizontalHeader()->restoreState(state.toByteArray());
    }
}

int AlertWidget::alertCount() const
{
    FCT_IDENTIFICATION;

    return alertTableModel->rowCount();
}
