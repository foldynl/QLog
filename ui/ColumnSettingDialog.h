#ifndef QLOG_UI_COLUMNSETTINGDIALOG_H
#define QLOG_UI_COLUMNSETTINGDIALOG_H

#include <QDialog>
#include <QTableView>
#include <QGridLayout>
#include <QCheckBox>
#include <QPair>
#include <QList>
#include <QString>

#include "models/LogbookModel.h"
namespace Ui {
class ColumnSettingDialog;
class ColumnSettingSimpleDialog;
}

class ColumnSettingGenericDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ColumnSettingGenericDialog(const QAbstractItemModel *model,
                                        QWidget *parent = nullptr);
    ~ColumnSettingGenericDialog(){};

signals:
    void columnChanged(int index, bool state);

protected:
    void addSortedCheckboxes(QGridLayout *grid, QList<QCheckBox*> &checkboxlist, int elementsPerRow);
    void addSelectUnselect(QGridLayout *, int);
    const QAbstractItemModel *model;
};

class ColumnSettingSimpleDialog : public ColumnSettingGenericDialog
{
    Q_OBJECT

public:
    explicit ColumnSettingSimpleDialog(QTableView *table, QWidget *parent = nullptr);
    ~ColumnSettingSimpleDialog();

private:
    Ui::ColumnSettingSimpleDialog *ui;
    QTableView *table;
};

class ColumnSettingDialog : public ColumnSettingGenericDialog
{
    Q_OBJECT

public:
    explicit ColumnSettingDialog(QTableView *table,
                                 QWidget *parent = nullptr,
                                 const QList<LogbookModel::ColumnID> &columnIdExcludeFilter = QList<LogbookModel::ColumnID>());
    explicit ColumnSettingDialog(const QAbstractItemModel *model,
                                 const QSet<int> &defaultStates,
                                 QWidget *parent = nullptr,
                                 const QList<LogbookModel::ColumnID> &columnIdExcludeFilter = QList<LogbookModel::ColumnID>());
    ~ColumnSettingDialog();

private:
    void setupDialog();
    Ui::ColumnSettingDialog *ui;
    QTableView *table;
    QSet<int> defaultColumnsState;
    QList<LogbookModel::ColumnID> columnIdExcludeFilter;
};

#endif // QLOG_UI_COLUMNSETTINGDIALOG_H
