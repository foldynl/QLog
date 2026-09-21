#ifndef QLOG_UI_DUPLICATECONTACTSDIALOG_H
#define QLOG_UI_DUPLICATECONTACTSDIALOG_H

#include <QDialog>
#include <QSet>
#include <QList>
#include <QColor>
#include <QMap>
#include "core/LogLocale.h"
#include <QTableWidgetItem>
#include <QSqlRecord>

namespace Ui {
class DuplicateContactsDialog;
}

class DuplicateContactsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DuplicateContactsDialog(QWidget *parent = nullptr);
    ~DuplicateContactsDialog();

private slots:
    void autoSelectDuplicates();
    void unselectAll();
    void removeChecked();
    void mergeGroups();
    void onItemChanged(QTableWidgetItem *item);

private:
    Ui::DuplicateContactsDialog *ui;
    LogLocale locale;

    void findDuplicates();
    void addRow(int row, const QList<QVariant> &data, const QColor &bgColor, bool checked);
    QSet<int> getCheckedIds() const;
    void updateRemoveButton();
    bool mergeGroupById(const QList<int> &group, int &removedOut);
    QList<QList<int>> duplicateGroups;
    QList<int> groupFirstRows;
};

#endif // QLOG_UI_DUPLICATECONTACTSDIALOG_H
