#include "EditActivitiesDialog.h"
#include "ui_EditActivitiesDialog.h"
#include "core/debug.h"
#include "ui/ActivityEditor.h"
#include "data/ActivityProfile.h"

MODULE_IDENTIFICATION("qlog.ui.EditLayoutDialog");

EditActivitiesDialog::EditActivitiesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditActivitiesDialog),
    profilesModel(new QStringListModel(this))
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);
    ui->listView->setModel(profilesModel);

    connect(ui->listView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]()
    {
        setSelectionActionsEnabled(ui->listView->selectionModel()->hasSelection());
    });
    connect(profilesModel, &QAbstractItemModel::modelAboutToBeReset,
            this, [this]()
    {
        setSelectionActionsEnabled(false);
    });

    setSelectionActionsEnabled(false);
    loadProfiles();
}

EditActivitiesDialog::~EditActivitiesDialog()
{
    FCT_IDENTIFICATION;
    delete ui;
}

void EditActivitiesDialog::loadProfiles()
{
    FCT_IDENTIFICATION;

    profilesModel->setStringList(ActivityProfilesManager::instance()->profileNameList());
}

void EditActivitiesDialog::setSelectionActionsEnabled(bool enabled)
{
    ui->editButton->setEnabled(enabled);
    ui->cloneButton->setEnabled(enabled);
    ui->removeButton->setEnabled(enabled);
}

void EditActivitiesDialog::addButton()
{
    FCT_IDENTIFICATION;

    ActivityEditor dialog(QString(), this);
    dialog.exec();
    loadProfiles();
}

void EditActivitiesDialog::removeButton()
{
    FCT_IDENTIFICATION;

    const QString &removeProfileName = ui->listView->currentIndex().data().toString();
    ActivityProfilesManager::instance()->removeProfile(removeProfileName);
    ActivityProfilesManager::instance()->save();
    MainLayoutProfilesManager::instance()->removeProfile(removeProfileName);
    MainLayoutProfilesManager::instance()->save();
    loadProfiles();
}

void EditActivitiesDialog::editEvent(const QModelIndex &idx)
{
    FCT_IDENTIFICATION;

    ActivityEditor dialog(ui->listView->model()->data(idx).toString(), this);
    dialog.exec();
    loadProfiles();
}

void EditActivitiesDialog::editButton()
{
    FCT_IDENTIFICATION;

    const QModelIndexList &selected = ui->listView->selectionModel()->selectedIndexes();
    if (!selected.isEmpty())
        editEvent(selected.first());
}

void EditActivitiesDialog::cloneButton()
{
    FCT_IDENTIFICATION;

    const QModelIndexList &selected = ui->listView->selectionModel()->selectedIndexes();
    if ( selected.isEmpty() )
        return;

    ActivityEditor dialog(selected.first().data().toString(), this, true);
    dialog.exec();
    loadProfiles();
}
