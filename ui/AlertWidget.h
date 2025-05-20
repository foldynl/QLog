#ifndef QLOG_UI_ALERTWIDGET_H
#define QLOG_UI_ALERTWIDGET_H

#include <QWidget>
#include "data/SpotAlert.h"
#include "models/AlertTableModel.h"
#include "data/DxSpot.h"
#include "component/ShutdownAwareWidget.h"

namespace Ui {
class AlertWidget;
}

class AlertWidget : public QWidget, public ShutdownAwareWidget
{
    Q_OBJECT

public:
    explicit AlertWidget(QWidget *parent = nullptr);
    ~AlertWidget();

   int alertCount() const;
   virtual void finalizeBeforeAppExit() override;

public slots:
    void addAlert(const SpotAlert &alert);
    void clearAllAlerts();
    void entryDoubleClicked(QModelIndex index);
    void alertAgingChanged(int);
    void showEditRules();
    void resetDupe();
    void updateSpotsStatusWhenQSOAdded(const QSqlRecord &record);
    void updateSpotsStatusWhenQSOUpdated(const QSqlRecord &record);
    void updateSpotsDupeWhenQSODeleted(const QSqlRecord &record);
    void updateSpotsDxccStatusWhenQSODeleted(const QSet<uint> &entities);
    void recalculateDupe();
    void recalculateDxccStatus();
    void saveTableHeaderState();

private slots:
    void showColumnVisibility();

signals:
    void alertsCleared();
    void tuneDx(DxSpot);
    void tuneWsjtx(WsjtxDecode);
    void rulesChanged();

private:
    Ui::AlertWidget *ui;
    AlertTableModel* alertTableModel;
    QSortFilterProxyModel *proxyModel;
    QTimer *aging_timer;

private slots:
    void alertAging();
    void restoreTableHeaderState();

};

#endif // QLOG_UI_ALERTWIDGET_H
