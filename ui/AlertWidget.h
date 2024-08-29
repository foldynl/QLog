#ifndef QLOG_UI_ALERTWIDGET_H
#define QLOG_UI_ALERTWIDGET_H

#include <QWidget>
#include "data/SpotAlert.h"
#include "models/AlertTableModel.h"

namespace Ui {
class AlertWidget;
}

class AlertWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AlertWidget(QWidget *parent = nullptr);
    ~AlertWidget();

   int alertCount() const;

public slots:
    void addAlert(const SpotAlert &alert);
    void clearAllAlerts();
    void entryDoubleClicked(QModelIndex index);
    void alertAgingChanged(int);
    void showEditRules();

private slots:
    void showColumnVisibility();

signals:
    void alertsCleared();
    void tuneDx(QString, double, BandPlan::BandPlanMode);
    void tuneWsjtx(WsjtxDecode);
    void rulesChanged();

private:
    Ui::AlertWidget *ui;
    AlertTableModel* alertTableModel;
    QSortFilterProxyModel *proxyModel;
    QTimer *aging_timer;
    QSettings settings;

private slots:
    void alertAging();
    void saveTableHeaderState();
    void restoreTableHeaderState();

};

#endif // QLOG_UI_ALERTWIDGET_H
