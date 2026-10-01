#ifndef QLOG_UI_COMPONENT_DXCCLOGSTATUSWIDGET_H
#define QLOG_UI_COMPONENT_DXCCLOGSTATUSWIDGET_H

#include <QWidget>
#include "data/DxccLogStatusCondition.h"

namespace Ui {
class DxccLogStatusWidget;
}

class DxccLogStatusWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DxccLogStatusWidget(QWidget *parent = nullptr);
    ~DxccLogStatusWidget();
    void setCondition(const DxccLogStatusCondition &condition);
    DxccLogStatusCondition condition() const;

private slots:
    void updateUi();
    void conditionChanged();
    void editLogStatuses();

private:
    bool legacyStatusAsNewCondition(int status, AlertDxccGoal &goal,
                                    AlertDxccContext &context) const;

    enum GoalItem
    {
        SELECTED_DXCC_STATUSES = -1,
        INVALID_DXCC_CONDITION = -2
    };

    Ui::DxccLogStatusWidget *ui;
    DxccLogStatusCondition currentCondition;
};

#endif // QLOG_UI_COMPONENT_DXCCLOGSTATUSWIDGET_H
