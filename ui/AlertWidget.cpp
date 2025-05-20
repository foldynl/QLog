#include "AlertWidget.h"
#include "ui_AlertWidget.h"
#include "core/debug.h"
#include "AlertSettingDialog.h"
#include "ui/ColumnSettingDialog.h"
#include "core/LogParam.h"

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

    ui->clearAlertOlderSpinBox->setValue(LogParam::getWidgetAlertAging());

    aging_timer = new QTimer;
    connect(aging_timer, &QTimer::timeout, this, &AlertWidget::alertAging);
    aging_timer->start(ALERT_AGING_CHECK_TIME);
}

AlertWidget::~AlertWidget()
{
    FCT_IDENTIFICATION;

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
        emit tuneWsjtx(record.alert.spot.decode);
    else
        emit tuneDx(record.alert.getDxSpot());
}

void AlertWidget::alertAgingChanged(int)
{
    FCT_IDENTIFICATION;

    LogParam::setWidgetAlertAging(ui->clearAlertOlderSpinBox->value());
}

void AlertWidget::showEditRules()
{
    FCT_IDENTIFICATION;

    AlertSettingDialog dialog(this);
    dialog.exec();
    emit rulesChanged();
}

void AlertWidget::resetDupe()
{
    FCT_IDENTIFICATION;

    alertTableModel->resetDupe();
}

void AlertWidget::updateSpotsStatusWhenQSOAdded(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    alertTableModel->updateSpotsStatusWhenQSOAdded(record);
}

void AlertWidget::updateSpotsStatusWhenQSOUpdated(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    alertTableModel->updateSpotsStatusWhenQSOUpdated(record);
}

void AlertWidget::updateSpotsDupeWhenQSODeleted(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    // Pay attention: this method is called before the QSO is added to contacts
    alertTableModel->updateSpotsStatusWhenQSODeleted(record);
}

void AlertWidget::updateSpotsDxccStatusWhenQSODeleted(const QSet<uint> &entities)
{
    alertTableModel->updateSpotsDxccStatusWhenQSODeleted(entities);
}

void AlertWidget::recalculateDupe()
{
    FCT_IDENTIFICATION;

    alertTableModel->recalculateDupe();
}

void AlertWidget::recalculateDxccStatus()
{
    FCT_IDENTIFICATION;

    alertTableModel->recalculateDxccStatus();
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
    LogParam::setWidgetAlertWidgetState(state);
}

void AlertWidget::restoreTableHeaderState()
{
    FCT_IDENTIFICATION;

    const QByteArray &state = LogParam::getWidgetAlertWidgetState();

    if ( !state.isEmpty() )
        ui->alertTableView->horizontalHeader()->restoreState(state);
}

int AlertWidget::alertCount() const
{
    FCT_IDENTIFICATION;

    return alertTableModel->rowCount();
}

void AlertWidget::finalizeBeforeAppExit()
{
    FCT_IDENTIFICATION;

    saveTableHeaderState();
}
