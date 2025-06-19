#include <QMessageBox>

#include "ActivityEditor.h"
#include "ui_ActivityEditor.h"
#include "core/debug.h"
#include "ui/NewContactWidget.h"
#include "ui/MainWindow.h"
#include "data/StationProfile.h"
#include "data/AntProfile.h"
#include "data/RigProfile.h"
#include "data/RotProfile.h"
#include "models/LogbookModel.h"
#include "data/Data.h"
#include "data/ActivityProfile.h"

MODULE_IDENTIFICATION("qlog.ui.mainlayouteditor");

ActivityEditor::ActivityEditor(const QString &activityName,
                                   QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ActivityEditor),
    availableFieldsModel(new StringListModel(this)),
    qsoRowAFieldsModel(new StringListModel(this)),
    qsoRowBFieldsModel(new StringListModel(this)),
    detailColAFieldsModel(new StringListModel(this)),
    detailColBFieldsModel(new StringListModel(this)),
    detailColCFieldsModel(new StringListModel(this)),
    dynamicWidgets(new NewContactDynamicWidgets(false, this))
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    // disabled for QT 5.12 (ubuntu 20.04) due to issue in QT
    // moveRow does not work
    ui->qsoRowADownButton->setVisible(false);
    ui->qsoRowAUpButton->setVisible(false);
    ui->qsoRowBDownButton->setVisible(false);
    ui->qsoRowBUpButton->setVisible(false);
    ui->detailColADownButton->setVisible(false);
    ui->detailColAUpButton->setVisible(false);
    ui->detailColBDownButton->setVisible(false);
    ui->detailColBUpButton->setVisible(false);
    ui->detailColCDownButton->setVisible(false);
    ui->detailColCUpButton->setVisible(false);
#endif

    availableFieldsModel->setStringList(dynamicWidgets->getAllFieldLabelNames());
    availableFieldsModel->sort(0);

    ui->availableFieldsListView->setModel(availableFieldsModel);
    ui->qsoRowAFieldsListView->setModel(qsoRowAFieldsModel);
    ui->qsoRowBFieldsListView->setModel(qsoRowBFieldsModel);
    ui->detailColAFieldsListView->setModel(detailColAFieldsModel);
    ui->detailColBFieldsListView->setModel(detailColBFieldsModel);
    ui->detailColCFieldsListView->setModel(detailColCFieldsModel);

    connectQSORowButtons();
    connectDetailColsButtons();

    if ( ! activityName.isEmpty() )
    {
        MainLayoutProfile profile = MainLayoutProfilesManager::instance()->getProfile(activityName);

        ui->activityNameEdit->setEnabled(false);
        ui->activityNameEdit->setText(profile.profileName);

        fillWidgets(profile);
    }
    else
        fillWidgets(MainLayoutProfile::getClassicLayout());

    setupValuesTab(activityName);
}

ActivityEditor::~ActivityEditor()
{
    delete ui;
    delete dynamicWidgets;
}

void ActivityEditor::save()
{
    FCT_IDENTIFICATION;

    if ( ui->activityNameEdit->text().isEmpty() )
    {
        ui->activityNameEdit->setPlaceholderText(tr("Must not be empty"));
        return;
    }

    if ( activityNameExists(ui->activityNameEdit->text()) )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Info"),
                              QMessageBox::tr("Activity name is already exists."));
        return;
    }


    /**********************
     * Save Layout profile
     **********************/
    MainLayoutProfile profile;

    profile.profileName = ui->activityNameEdit->text();
    profile.rowA = getFieldIndexes(qsoRowAFieldsModel);
    profile.rowB = getFieldIndexes(qsoRowBFieldsModel);
    profile.detailColA = getFieldIndexes(detailColAFieldsModel);
    profile.detailColB = getFieldIndexes(detailColBFieldsModel);
    profile.detailColC = getFieldIndexes(detailColCFieldsModel);
    profile.mainGeometry = mainGeometry;
    profile.mainState = mainState;
    profile.addlBandmaps = addlBandmaps;
    profile.darkMode = darkMode;
    MainLayoutProfilesManager::instance()->addProfile(profile.profileName, profile);
    MainLayoutProfilesManager::instance()->save();

    /***********************
     * Save Activity profile
     ***********************/

    ActivityProfile activity;

    activity.profileName = ui->activityNameEdit->text();

    auto insertProfile = [&](ActivityProfile::ProfileType profileType,
                             bool save,
                             const QString profileName,
                             QCheckBox* connectCheckbox = nullptr)
    {
        if ( save )
        {
            ActivityProfile::ProfileRecord rec;
            rec.name = profileName;

            if ( connectCheckbox )
                rec.params[ActivityProfile::ProfileParamType::CONNECT] = connectCheckbox->isChecked();

            activity.profiles.insert(profileType, rec);
        }
    };

    auto insertParam = [&] (LogbookModel::ColumnID fieldID,
                            QCheckBox* profileCheckbox,
                            QVariant value)
    {
        if ( profileCheckbox->isChecked() )
            activity.fieldValues.insert(fieldID, value);
    };

    insertProfile(ActivityProfile::ProfileType::MAIN_LAYOUT_PROFILE, true, ui->activityNameEdit->text());
    insertProfile(ActivityProfile::ProfileType::ANTENNA_PROFILE, ui->antennaProfileCheckbox->isChecked(), ui->antennaProfileCombo->currentText());
    insertProfile(ActivityProfile::ProfileType::STATION_PROFILE, ui->stationProfileCheckbox->isChecked(), ui->stationProfileCombo->currentText());
    insertProfile(ActivityProfile::ProfileType::RIG_PROFILE, ui->rigProfileCheckbox->isChecked(), ui->rigProfileCombo->currentText(), ui->rigAutoconnectCheckbox);
    insertProfile(ActivityProfile::ProfileType::ROT_PROFILE, ui->rotatorProfileCheckbox->isChecked(), ui->rotatorProfileCombo->currentText(), ui->rotatorAutoconnectCheckbox);

    insertParam(LogbookModel::COLUMN_CONTEST_ID, ui->contestIDCheckbox, ui->contestIDEdit->text());
    insertParam(LogbookModel::COLUMN_PROP_MODE, ui->propagationModeCheckbox, ui->propagationModeCombo->currentText());
    insertParam(LogbookModel::COLUMN_SAT_MODE, ui->satModeCheckbox, ui->satModeCombo->currentText());
    insertParam(LogbookModel::COLUMN_SAT_NAME, ui->satNameCheckbox, ui->satNameEdit->text());
    insertParam(LogbookModel::COLUMN_STX_STRING, ui->stxStringCheckbox, ui->stxStringEdit->text());

    ActivityProfilesManager::instance()->addProfile(ui->activityNameEdit->text(), activity);
    ActivityProfilesManager::instance()->save();
    accept();
}

void ActivityEditor::profileNameChanged(const QString &profileName)
{
    FCT_IDENTIFICATION;

    QPalette p;
    p.setColor(QPalette::Text, ( activityNameExists(profileName) ) ? Qt::red
                                                                   : qApp->palette().text().color());
    ui->activityNameEdit->setPalette(p);
}

void ActivityEditor::clearMainLayoutClick()
{
    FCT_IDENTIFICATION;

    mainGeometry = QByteArray();
    mainState = QByteArray();
    addlBandmaps = QList<QPair<QString, QString>>();
    darkMode = false;
    ui->mainLayoutStateLabel->setText(statusUnSavedText);
    ui->mainLayoutClearButton->setEnabled(false);
}

void ActivityEditor::setValueState()
{
    FCT_IDENTIFICATION;

    ui->antennaProfileCombo->setEnabled(ui->antennaProfileCheckbox->isChecked());
    ui->stationProfileCombo->setEnabled(ui->stationProfileCheckbox->isChecked());
    ui->rigProfileCombo->setEnabled(ui->rigProfileCheckbox->isChecked());
    ui->rigAutoconnectCheckbox->setEnabled(ui->rigProfileCheckbox->isChecked());
    ui->rotatorProfileCombo->setEnabled(ui->rotatorProfileCheckbox->isChecked());
    ui->rotatorAutoconnectCheckbox->setEnabled(ui->rotatorProfileCheckbox->isChecked());

    bool isContestActive = !availableFieldsModel->stringList().contains(LogbookModel::getFieldNameTranslation(LogbookModel::COLUMN_CONTEST_ID));
    bool isSTXStringActive = !availableFieldsModel->stringList().contains(LogbookModel::getFieldNameTranslation(LogbookModel::COLUMN_STX_STRING));

    if ( !isContestActive )
        ui->contestIDCheckbox->setChecked(false);
    ui->contestIDCheckbox->setVisible(isContestActive);
    ui->contestIDEdit->setVisible(isContestActive);
    ui->contestIDEdit->setEnabled(ui->contestIDCheckbox->isChecked());

    if ( !isSTXStringActive )
        ui->stxStringCheckbox->setChecked(false);
    ui->stxStringCheckbox->setVisible(isSTXStringActive);
    ui->stxStringEdit->setVisible(isSTXStringActive);
    ui->stxStringEdit->setEnabled(ui->stxStringCheckbox->isChecked());

    ui->propagationModeCombo->setEnabled(ui->propagationModeCheckbox->isChecked());

    bool isSatActive = ui->propagationModeCombo->currentText() == Data::instance()->propagationModeIDToText("SAT")
                       && ui->propagationModeCheckbox->isChecked();
    if ( !isSatActive )
    {
        ui->satModeCheckbox->setChecked(false);
        ui->satNameCheckbox->setChecked(false);
    }

    ui->satModeCheckbox->setVisible(isSatActive);
    ui->satModeCombo->setVisible(isSatActive);
    ui->satModeCombo->setEnabled(ui->satModeCheckbox->isChecked());

    ui->satNameCheckbox->setVisible(isSatActive);
    ui->satNameEdit->setVisible(isSatActive);
    ui->satNameEdit->setEnabled(ui->satNameCheckbox->isChecked());
}

void ActivityEditor::moveField(StringListModel *source,
                               StringListModel *destination,
                               const QModelIndexList &sourceIndexList)
{
    FCT_IDENTIFICATION;

    QModelIndexList selectedIndexes = sourceIndexList;

    if ( selectedIndexes.isEmpty() )
            return;

    std::sort(selectedIndexes.begin(),
              selectedIndexes.end(),
              [](const QModelIndex &a, const QModelIndex &b)
    {
        return a.row() > b.row();
    });

    for ( const QModelIndex &index : sourceIndexList )
        destination->append(source->data(index).toString());

    /* Delete the sorted index list becuase without sorting
     * it deletes wrong records
     */
    for ( const QModelIndex &index : selectedIndexes )
        source->deleteItem(index);

    setValueState();
}

void ActivityEditor::connectQSORowButtons()
{
    FCT_IDENTIFICATION;

    // Connect buttons for QSO Row A
    connectMoveButtons(ui->qsoRowADownButton, ui->qsoRowAUpButton,
                       ui->qsoRowAFieldsListView, qsoRowAFieldsModel);
    connectFieldButtons(ui->moveToQSORowAButton, ui->removeFromQSORowAButton,
                        qsoRowAFieldsModel, ui->qsoRowAFieldsListView);

    // Connect buttons for QSO Row B
    connectMoveButtons(ui->qsoRowBDownButton, ui->qsoRowBUpButton,
                       ui->qsoRowBFieldsListView, qsoRowBFieldsModel);
    connectFieldButtons(ui->moveToQSORowBButton, ui->removeFromQSORowBButton,
                        qsoRowBFieldsModel, ui->qsoRowBFieldsListView);
}

void ActivityEditor::connectDetailColsButtons()
{
    FCT_IDENTIFICATION;

    connectMoveButtons(ui->detailColADownButton, ui->detailColAUpButton,
                       ui->detailColAFieldsListView, detailColAFieldsModel);
    connectFieldButtons(ui->moveToDetailColAButton, ui->removeFromDetailColAButton,
                        detailColAFieldsModel, ui->detailColAFieldsListView);

    connectMoveButtons(ui->detailColBDownButton, ui->detailColBUpButton,
                       ui->detailColBFieldsListView, detailColBFieldsModel);
    connectFieldButtons(ui->moveToDetailColBButton, ui->removeFromDetailColBButton,
                        detailColBFieldsModel, ui->detailColBFieldsListView);

    connectMoveButtons(ui->detailColCDownButton, ui->detailColCUpButton,
                       ui->detailColCFieldsListView, detailColCFieldsModel);
    connectFieldButtons(ui->moveToDetailColCButton, ui->removeFromDetailColCButton,
                        detailColCFieldsModel, ui->detailColCFieldsListView);
}

void ActivityEditor::connectMoveButtons(QPushButton *downButton, QPushButton *upButton,
                                        QListView *listView, StringListModel *model)
{
    FCT_IDENTIFICATION;

    connect(downButton, &QPushButton::clicked, this, [listView, model]()
    {
        const QModelIndexList &modelList = listView->selectionModel()->selectedRows();
        if ( !modelList.isEmpty() )
            model->moveDown(modelList.at(0));
    });

    connect(upButton, &QPushButton::clicked, this, [listView, model]()
    {
        const QModelIndexList &modelList = listView->selectionModel()->selectedRows();
        if ( !modelList.isEmpty() )
            model->moveUp(modelList.at(0));
    });
}

void ActivityEditor::connectFieldButtons(QPushButton *moveToButton, QPushButton *removeButton,
                                         StringListModel *targetModel, QListView *targetListView)
{
    FCT_IDENTIFICATION;

    connect(moveToButton, &QPushButton::clicked, this, [this, targetModel]()
    {
         moveField(availableFieldsModel, targetModel,
         ui->availableFieldsListView->selectionModel()->selectedIndexes());
    });

    connect(removeButton, &QPushButton::clicked, this, [this, targetModel, targetListView]()
    {
         moveField(targetModel, availableFieldsModel,
         targetListView->selectionModel()->selectedRows());
    });
}

QList<int> ActivityEditor::getFieldIndexes(StringListModel *model)
{
    FCT_IDENTIFICATION;

    const QStringList &list = model->stringList();
    QList<int> ret;

    for ( const QString &fieldName : list )
    {
        int index = dynamicWidgets->getIndex4FieldLabelName(fieldName);
        if ( index >= 0 )
            ret << index;
    }

    return ret;
}

void ActivityEditor::setupValuesTab(const QString &activityName)
{
    FCT_IDENTIFICATION;

    auto assignCompleter = [&] (QLineEdit *field, const QStringList &list)
    {
        QStringListModel *model = new QStringListModel(list);
        QCompleter *completer = new QCompleter(model, field);
        model->setParent(completer);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchStartsWith);
        field->setCompleter(completer);
    };

    auto assignTableCompleter = [&] (QLineEdit *field, const QString tableName)
    {
         QSqlTableModel* model = new QSqlTableModel();
         model->setTable(tableName);
         QCompleter *completer = new QCompleter(model, field);
         model->setParent(completer);
         completer->setCaseSensitivity(Qt::CaseInsensitive);
         completer->setFilterMode(Qt::MatchStartsWith);
         field->setCompleter(completer);
         model->select();
    };


    auto assignModel = [&] (QComboBox *field, const QStringList &list)
    {
        QStringListModel *model = new QStringListModel(list, field);
        field->setModel(model);
    };

    auto setProfileVisible = [&] (QCheckBox *checkbox, QComboBox *field, QCheckBox *connectCheckbox = nullptr)
    {
        if ( field->count() == 0)
        {
            checkbox->setChecked(false);
            checkbox->setVisible(false);
            field->setVisible(false);
            if ( connectCheckbox )
                connectCheckbox->setVisible(false);
        }
        else
        {
            checkbox->setVisible(true);
            field->setVisible(true);
            if ( connectCheckbox )
                connectCheckbox->setVisible(true);
        }
    };

    auto loadProfileValue = [&] (const ActivityProfile &activityProfile,
                                 ActivityProfile::ProfileType profileType,
                                 QCheckBox *profileCheckbox,
                                 QComboBox *profileCombo,
                                 QCheckBox *connectCheckbox = nullptr)
    {
        auto currProfile = activityProfile.profiles.value(profileType);
        int index = profileCombo->findText(currProfile.name);
        profileCheckbox->setChecked(index != -1);
        if ( index != -1 )
        {
            profileCombo->setCurrentIndex(index);
            if ( connectCheckbox )
                connectCheckbox->setChecked(activityProfile.getProfileParam(profileType, ActivityProfile::ProfileParamType::CONNECT).toBool());
        }
    };

    ui->stationProfileCombo->addItems(StationProfilesManager::instance()->profileNameList());
    ui->antennaProfileCombo->addItems(AntProfilesManager::instance()->profileNameList());
    ui->rigProfileCombo->addItems(RigProfilesManager::instance()->profileNameList());
    ui->rotatorProfileCombo->addItems(RotProfilesManager::instance()->profileNameList());

    setProfileVisible(ui->antennaProfileCheckbox, ui->antennaProfileCombo);
    setProfileVisible(ui->stationProfileCheckbox, ui->stationProfileCombo);
    setProfileVisible(ui->rigProfileCheckbox, ui->rigProfileCombo, ui->rigAutoconnectCheckbox);
    setProfileVisible(ui->rotatorProfileCheckbox, ui->rotatorProfileCombo, ui->rotatorAutoconnectCheckbox);

    ui->contestIDCheckbox->setText(LogbookModel::getFieldNameTranslation(LogbookModel::COLUMN_CONTEST_ID));
    ui->propagationModeCheckbox->setText(LogbookModel::getFieldNameTranslation(LogbookModel::COLUMN_PROP_MODE));
    ui->satModeCheckbox->setText(LogbookModel::getFieldNameTranslation(LogbookModel::COLUMN_SAT_MODE));
    ui->satNameCheckbox->setText(LogbookModel::getFieldNameTranslation(LogbookModel::COLUMN_SAT_NAME));
    ui->stxStringCheckbox->setText(LogbookModel::getFieldNameTranslation(LogbookModel::COLUMN_STX_STRING));

    assignCompleter(ui->contestIDEdit, Data::instance()->contestList());
    assignTableCompleter(ui->satNameEdit, "sat_info");

    assignModel(ui->propagationModeCombo, Data::instance()->propagationModesList());
    assignModel(ui->satModeCombo, Data::instance()->satModeList());

    if ( !activityName.isEmpty() )
    {
        const ActivityProfile &activity = ActivityProfilesManager::instance()->getProfile(activityName);
        loadProfileValue(activity, ActivityProfile::ProfileType::ANTENNA_PROFILE, ui->antennaProfileCheckbox, ui->antennaProfileCombo);
        loadProfileValue(activity, ActivityProfile::ProfileType::STATION_PROFILE, ui->stationProfileCheckbox, ui->stationProfileCombo);
        loadProfileValue(activity, ActivityProfile::ProfileType::RIG_PROFILE, ui->rigProfileCheckbox, ui->rigProfileCombo, ui->rigAutoconnectCheckbox);
        loadProfileValue(activity, ActivityProfile::ProfileType::ROT_PROFILE, ui->rotatorProfileCheckbox, ui->rotatorProfileCombo, ui->rotatorAutoconnectCheckbox);

        for ( auto i = activity.fieldValues.begin(); i != activity.fieldValues.end(); i++ )
        {
            switch (i.key())
            {
            case LogbookModel::COLUMN_CONTEST_ID:
                ui->contestIDCheckbox->setChecked(true);
                ui->contestIDEdit->setText(i.value().toString());
                break;

            case LogbookModel::COLUMN_PROP_MODE:
                ui->propagationModeCheckbox->setChecked(true);
                ui->propagationModeCombo->setCurrentText(i.value().toString());
                break;

            case LogbookModel::COLUMN_SAT_MODE:
                ui->satModeCheckbox->setChecked(true);
                ui->satModeCombo->setCurrentText(i.value().toString());
                break;

            case LogbookModel::COLUMN_SAT_NAME:
                ui->satNameCheckbox->setChecked(true);
                ui->satNameEdit->setText(i.value().toString());
                break;

            case LogbookModel::COLUMN_STX_STRING:
                ui->stxStringCheckbox->setChecked(true);
                ui->stxStringEdit->setText(i.value().toString());
                break;

            default:
                qWarning() << "Unsupported Parameter" << i.key();
            }
        }
    }

    setValueState();
}

void ActivityEditor::fillWidgets(const MainLayoutProfile &profile)
{
    FCT_IDENTIFICATION;

    auto processFields = [this](const QList<int>& fieldIndices, StringListModel* targetModel)
    {
        for (int fieldIndex : fieldIndices)
        {
            const QString &fieldName = dynamicWidgets->getFieldLabelName4Index(fieldIndex);
            targetModel->append(fieldName);
            availableFieldsModel->deleteItem(fieldName);
        }
    };

    // Process each row and detail column using the helper
    processFields(static_cast<const QList<int>&>(profile.rowA), qsoRowAFieldsModel);
    processFields(static_cast<const QList<int>&>(profile.rowB), qsoRowBFieldsModel);
    processFields(static_cast<const QList<int>&>(profile.detailColA), detailColAFieldsModel);
    processFields(static_cast<const QList<int>&>(profile.detailColB), detailColBFieldsModel);
    processFields(static_cast<const QList<int>&>(profile.detailColC), detailColCFieldsModel);

    mainGeometry = profile.mainGeometry;
    mainState = profile.mainState;
    addlBandmaps = profile.addlBandmaps;
    darkMode = profile.darkMode;

    if ( mainGeometry == QByteArray()
         && mainState == QByteArray() )
    {
        ui->mainLayoutStateLabel->setText(statusUnSavedText);
        ui->mainLayoutClearButton->setEnabled(false);
    }
}

bool ActivityEditor::activityNameExists(const QString &activityName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << activityName;

    return  ui->activityNameEdit->isEnabled()
            && MainLayoutProfilesManager::instance()->profileNameList().contains(activityName);
}
