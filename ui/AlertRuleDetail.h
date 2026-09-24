#ifndef QLOG_UI_ALERTRULEDETAIL_H
#define QLOG_UI_ALERTRULEDETAIL_H

#include <QDialog>
#include <QCheckBox>

#include "core/AlertEvaluator.h"

namespace Ui {
class AlertRuleDetail;
}

class AlertRuleDetail : public QDialog
{
    Q_OBJECT

public:
    explicit AlertRuleDetail(const QString &ruleName, QWidget *parent, bool clone = false);
    ~AlertRuleDetail();

public slots:
    void save();
    void ruleNameChanged(const QString&);
    void callsignChanged(const QString&);
    void spotCommentChanged(const QString&);

private:
    Ui::AlertRuleDetail *ui;
    QString ruleName;
    QStringList ruleNamesList;
    QList<QCheckBox*> memberListCheckBoxes;


private slots:
    void logStatusEnabled(bool enabled);
    void logStatusConditionChanged();
    void editLogStatuses();

private:
    enum class LogStatusStorage
    {
        NewCondition,
        LegacyCondition,
        InvalidCondition
    };

    enum LogStatusGoalItem
    {
        SELECTED_DXCC_STATUSES = -1,
        INVALID_DXCC_CONDITION = -2
    };

    void setDefaultValues();
    bool ruleExists(const QString &ruleName);
    void loadRuleNames();
    void loadRule(const QString &ruleName);
    void generateMembershipCheckboxes(const AlertRule * rule = nullptr);
    void updateLogStatusUi();
    void populateLogStatusCombos();
    void setLogStatusCombos(AlertDxccGoal goal, AlertDxccContext context);
    bool legacyStatusAsNewCondition(int status,
                                    AlertDxccGoal &goal,
                                    AlertDxccContext &context) const;

    LogStatusStorage logStatusStorage = LogStatusStorage::NewCondition;
    int legacyLogStatus = DxccStatus::All;
    int currentDxAlertGoal = static_cast<int>(AlertDxccGoal::Confirmed);
    int currentDxAlertContext = static_cast<int>(AlertDxccContext::Anywhere);
    int loadedDxAlertGoal = static_cast<int>(AlertDxccGoal::Confirmed);
    int loadedDxAlertContext = static_cast<int>(AlertDxccContext::Anywhere);
    bool loadedDxAlertGoalSet = false;
    bool loadedDxAlertContextSet = false;
    bool updatingLogStatusUi = false;

    const quint8 MAXCOLUMNS = 8;
    const quint8 ALLCOUNTRYIDX = 0;
};

#endif // QLOG_UI_ALERTRULEDETAIL_H
