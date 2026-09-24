#ifndef QLOG_UI_ALERTRULELOGSTATUSDIALOG_H
#define QLOG_UI_ALERTRULELOGSTATUSDIALOG_H

#include <QDialog>

namespace Ui {
class AlertRuleLogStatusDialog;
}

class AlertRuleLogStatusDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AlertRuleLogStatusDialog(int status, QWidget *parent = nullptr);
    ~AlertRuleLogStatusDialog();

    int status() const;

private:
    Ui::AlertRuleLogStatusDialog *ui;
};

#endif // QLOG_UI_ALERTRULELOGSTATUSDIALOG_H
