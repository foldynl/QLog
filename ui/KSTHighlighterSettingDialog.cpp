#include "KSTHighlighterSettingDialog.h"
#include "ui_KSTHighlighterSettingDialog.h"
#include "core/debug.h"
#include "ui/component/StyleItemDelegate.h"
#include "ui/KSTHighlightRuleDetail.h"

MODULE_IDENTIFICATION("qlog.ui.ksthighlightersettingdialog");

KSTHighlighterSettingDialog::KSTHighlighterSettingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KSTHighlighterSettingDialog)
{
    ui->setupUi(this);

    rulesModel = new QSqlTableModel();
    rulesModel->setTable("chat_highlight_rules");
    rulesModel->setHeaderData(rulesModel->fieldIndex("rule_name"), Qt::Horizontal, tr("Name"));
    rulesModel->setHeaderData(rulesModel->fieldIndex("enabled"), Qt::Horizontal, tr("State"));

    ui->rulesView->setModel(rulesModel);

    for ( int i = 0 ; i < rulesModel->columnCount(); i++ )
    {
        ui->rulesView->hideColumn(i);
    }

    ui->rulesView->showColumn(rulesModel->fieldIndex("rule_name"));
    ui->rulesView->showColumn(rulesModel->fieldIndex("enabled"));

    ui->rulesView->setColumnWidth(0, 300);

    ui->rulesView->setItemDelegateForColumn(rulesModel->fieldIndex("enabled"), new CheckBoxDelegate(ui->rulesView));

    rulesModel->select();
}

KSTHighlighterSettingDialog::~KSTHighlighterSettingDialog()
{
    delete ui;
}

void KSTHighlighterSettingDialog::addRule()
{
    FCT_IDENTIFICATION;
    KSTHighlightRuleDetail dialog(QString(), this);
    dialog.exec();
    rulesModel->select();
}

void KSTHighlighterSettingDialog::removeRule()
{
    FCT_IDENTIFICATION;

    rulesModel->removeRow(ui->rulesView->currentIndex().row());
    ui->rulesView->clearSelection();
    rulesModel->select();

}

void KSTHighlighterSettingDialog::editRule(QModelIndex idx)
{
    FCT_IDENTIFICATION;

    const QModelIndex &nameIdx = ui->rulesView->model()->index(idx.row(),0);
    const QString &ruleName = ui->rulesView->model()->data(nameIdx).toString();

    KSTHighlightRuleDetail dialog(ruleName, this);
    dialog.exec();
    rulesModel->select();
}

void KSTHighlighterSettingDialog::editRuleButton()
{
    FCT_IDENTIFICATION;

    foreach (QModelIndex index, ui->rulesView->selectionModel()->selectedRows())
    {
       editRule(index);
    }
}
