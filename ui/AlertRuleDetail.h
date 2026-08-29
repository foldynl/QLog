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
    void enabledLogStatusAll(bool enabled);

private:
    void setDefaultValues();
    bool ruleExists(const QString &ruleName);
    void loadRuleNames();
    void loadRule(const QString &ruleName);
    void generateMembershipCheckboxes(const AlertRule * rule = nullptr);

    const quint8 MAXCOLUMNS = 8;
    const quint8 ALLCOUNTRYIDX = 0;
};

#endif // QLOG_UI_ALERTRULEDETAIL_H
