#include "AlertSettingDialog.h"
#include "ui_AlertSettingDialog.h"
#include "core/debug.h"
#include "ui/AlertRuleDetail.h"
#include "ui/component/StyleItemDelegate.h"

MODULE_IDENTIFICATION("qlog.ui.alertsettingdialog");

AlertSettingDialog::AlertSettingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AlertSettingDialog)
{
    ui->setupUi(this);

    rulesModel = new QSqlTableModel();
    rulesModel->setTable("alert_rules");
    rulesModel->setHeaderData(0, Qt::Horizontal, tr("Name"));
    rulesModel->setHeaderData(1, Qt::Horizontal, tr("State"));

    ui->rulesTableView->setModel(rulesModel);

    for ( int i = 0 ; i < rulesModel->columnCount(); i++ )
    {
        ui->rulesTableView->hideColumn(i);
    }

    ui->rulesTableView->showColumn(0);
    ui->rulesTableView->showColumn(1);

    ui->rulesTableView->setColumnWidth(0,300);

    ui->rulesTableView->setItemDelegateForColumn(1,new CheckBoxDelegate(ui->rulesTableView));

    rulesModel->select();
}

AlertSettingDialog::~AlertSettingDialog()
{
    delete ui;
}

void AlertSettingDialog::addRule()
{
    FCT_IDENTIFICATION;
    AlertRuleDetail dialog(QString(), this);
    dialog.exec();
    rulesModel->select();
}

void AlertSettingDialog::removeRule()
{
    FCT_IDENTIFICATION;

    rulesModel->removeRow(ui->rulesTableView->currentIndex().row());
    ui->rulesTableView->clearSelection();
    rulesModel->select();

}

void AlertSettingDialog::editRule(QModelIndex idx)
{
    FCT_IDENTIFICATION;

    QModelIndex nameIdx = ui->rulesTableView->model()->index(idx.row(),0);

    QString ruleName = ui->rulesTableView->model()->data(nameIdx).toString();

    AlertRuleDetail dialog(ruleName, this);
    dialog.exec();
    rulesModel->select();
}

void AlertSettingDialog::editRuleButton()
{
    FCT_IDENTIFICATION;

    foreach (QModelIndex index, ui->rulesTableView->selectionModel()->selectedRows())
    {
       editRule(index);
    }
}
