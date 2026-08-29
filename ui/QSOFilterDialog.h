#ifndef QLOG_UI_QSOFILTERDIALOG_H
#define QLOG_UI_QSOFILTERDIALOG_H

#include <QSqlTableModel>
#include <QDialog>


namespace Ui {
class QSOFilterDialog;
}

class QSOFilterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QSOFilterDialog(QWidget *parent = nullptr);
    ~QSOFilterDialog();

private:
    Ui::QSOFilterDialog *ui;
    QSqlTableModel* filterModel;
    void setSelectionActionsEnabled(bool enabled);

public slots:
    void addFilter();
    void removeFilter();
    void editFilter(QModelIndex);
    void editFilterButton();
    void cloneFilter();
};

#endif // QLOG_UI_QSOFILTERDIALOG_H
