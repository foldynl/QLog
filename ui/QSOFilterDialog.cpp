#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlTableModel>
#include "QSOFilterDialog.h"
#include "ui_QSOFilterDialog.h"
#include "core/debug.h"
#include "ui/QSOFilterDetail.h"

MODULE_IDENTIFICATION("qlog.ui.qsofilterdialog");

QSOFilterDialog::QSOFilterDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QSOFilterDialog)
{
    FCT_IDENTIFICATION;
    ui->setupUi(this);

    filterModel = new QSqlTableModel();
    filterModel->setTable("qso_filters");
    ui->filtersListView->setModel(filterModel);
    ui->filtersListView->setModelColumn(filterModel->fieldIndex("filter_name"));
    ui->filtersListView->setSelectionMode(QAbstractItemView::SingleSelection);
    filterModel->select();
}

QSOFilterDialog::~QSOFilterDialog()
{
    FCT_IDENTIFICATION;
    delete ui;
}

void QSOFilterDialog::addFilter()
{
    FCT_IDENTIFICATION;
    QSOFilterDetail dialog(QString(), this);
    dialog.exec();
    filterModel->select();
}

void QSOFilterDialog::removeFilter()
{
    FCT_IDENTIFICATION;

    filterModel->removeRow(ui->filtersListView->currentIndex().row());
    ui->filtersListView->clearSelection();
    filterModel->select();
}

void QSOFilterDialog::editFilter(QModelIndex idx)
{
    FCT_IDENTIFICATION;

    QString filterName = ui->filtersListView->model()->data(idx).toString();

    QSOFilterDetail dialog(filterName, this);
    dialog.exec();
}

void QSOFilterDialog::editFilterButton()
{
    FCT_IDENTIFICATION;

    foreach (QModelIndex index, ui->filtersListView->selectionModel()->selectedIndexes())
    {
       editFilter(index);
    }
}
