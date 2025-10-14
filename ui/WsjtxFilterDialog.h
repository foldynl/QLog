#ifndef QLOG_UI_WSJTXFILTERDIALOG_H
#define QLOG_UI_WSJTXFILTERDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QSet>

#include "core/LogLocale.h"

namespace Ui {
class WsjtxFilterDialog;
}

class WsjtxFilterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WsjtxFilterDialog(QWidget *parent = nullptr);
    ~WsjtxFilterDialog();
    void accept() override;

private:
    Ui::WsjtxFilterDialog *ui;
    QList<QCheckBox*> memberListCheckBoxes;
    QSet<QString> dxMemberFilter;
    LogLocale locale;

    void generateMembershipCheckboxes();

};

#endif // QLOG_UI_WSJTXFILTERDIALOG_H
