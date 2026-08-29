#ifndef QLOG_UI_EDITACTIVITIESDIALOG_H
#define QLOG_UI_EDITACTIVITIESDIALOG_H

#include <QDialog>
#include <QStringListModel>

namespace Ui {
class EditActivitiesDialog;
}

class EditActivitiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditActivitiesDialog(QWidget *parent = nullptr);
    ~EditActivitiesDialog();

private:
    Ui::EditActivitiesDialog *ui;
    QStringListModel *profilesModel;

    void loadProfiles();
    void setSelectionActionsEnabled(bool enabled);

public slots:
    void addButton();
    void removeButton();
    void editEvent(const QModelIndex &);
    void editButton();
    void cloneButton();
};

#endif // QLOG_UI_EDITACTIVITIESDIALOG_H
