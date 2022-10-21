#include <QSettings>
#include <QStringListModel>
#include <QSqlTableModel>
#include <hamlib/rig.h>
#include <hamlib/rotator.h>
#include <QFileDialog>
#include <QMessageBox>

#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "models/RigTypeModel.h"
#include "models/RotTypeModel.h"
#include "../core/GenericCallbook.h"
#include "../core/QRZ.h"
#include "../core/HamQTH.h"
#include "../core/Lotw.h"
#include "../core/ClubLog.h"
#include "../core/Eqsl.h"
#include "../ui/StyleItemDelegate.h"
#include "core/debug.h"
#include "core/CredentialStore.h"
#include "data/StationProfile.h"
#include "data/RigProfile.h"
#include "data/AntProfile.h"
#include "data/Data.h"
#include "core/Gridsquare.h"
#include "core/Wsjtx.h"
#include "core/PaperQSL.h"
#include "core/NetworkNotification.h"
#include "core/Rig.h"
#include "core/Rotator.h"
#include "core/LogParam.h"
#include "core/Callsign.h"
#include "core/CWKeyer.h"

#define WIDGET_INDEX_SERIAL_RIG  0
#define STACKED_WIDGET_NETWORK_RIG 1


MODULE_IDENTIFICATION("qlog.ui.settingdialog");


SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    stationProfManager(StationProfilesManager::instance()),
    rigProfManager(RigProfilesManager::instance()),
    rotProfManager(RotProfilesManager::instance()),
    antProfManager(AntProfilesManager::instance()),
    cwKeyProfManager(CWKeyProfilesManager::instance()),
    cwShortcutProfManager(CWShortcutProfilesManager::instance()),
    ui(new Ui::SettingsDialog)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    RotTypeModel* rotTypeModel = new RotTypeModel(this);
    ui->rotModelSelect->setModel(rotTypeModel);

    QStringListModel* rotModel = new QStringListModel();
    ui->rotProfilesListView->setModel(rotModel);

    QStringListModel* antModel = new QStringListModel();
    ui->antProfilesListView->setModel(antModel);

    QStringListModel* cwKeyModel = new QStringListModel();
    ui->cwProfilesListView->setModel(cwKeyModel);

    QStringListModel* cwShortcutModel = new QStringListModel();
    ui->cwShortcutListView->setModel(cwShortcutModel);

    QStringListModel* profilesModes = new QStringListModel();
    ui->stationProfilesListView->setModel(profilesModes);

    QStringListModel* cwKeysModel = new QStringListModel();
    ui->rigAssignedCWKeyCombo->setModel(cwKeysModel);

    /* Rig Models must be initialized after rigAssignedCWKeyCombo model !!!! */
    /* becase rigChanged is called and it constain uninitialized
     * CW Model */
    RigTypeModel* rigTypeModel = new RigTypeModel(this);
    ui->rigModelSelect->setModel(rigTypeModel);

    QStringListModel* rigModel = new QStringListModel();
    ui->rigProfilesListView->setModel(rigModel);

    modeTableModel = new QSqlTableModel(this);
    modeTableModel->setTable("modes");
    modeTableModel->setEditStrategy(QSqlTableModel::OnFieldChange);
    modeTableModel->setSort(1, Qt::AscendingOrder);
    modeTableModel->setHeaderData(1, Qt::Horizontal, tr("Name"));
    modeTableModel->setHeaderData(3, Qt::Horizontal, tr("Report"));
    modeTableModel->setHeaderData(4, Qt::Horizontal, tr("DXCC"));
    modeTableModel->setHeaderData(5, Qt::Horizontal, tr("State"));
    ui->modeTableView->setModel(modeTableModel);

    ui->modeTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->modeTableView->hideColumn(0);
    ui->modeTableView->hideColumn(2);
    ui->modeTableView->setItemDelegateForColumn(5,new CheckBoxDelegate(ui->modeTableView));
    modeTableModel->select();

    bandTableModel = new QSqlTableModel(this);
    bandTableModel->setTable("bands");
    bandTableModel->setEditStrategy(QSqlTableModel::OnFieldChange);
    bandTableModel->setSort(2, Qt::AscendingOrder);
    bandTableModel->setHeaderData(1, Qt::Horizontal, tr("Name"));
    bandTableModel->setHeaderData(2, Qt::Horizontal, tr("Start (MHz)"));
    bandTableModel->setHeaderData(3, Qt::Horizontal, tr("End (MHz)"));
    bandTableModel->setHeaderData(4, Qt::Horizontal, tr("State"));
    ui->bandTableView->setModel(bandTableModel);

    ui->bandTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->bandTableView->hideColumn(0); // primary key
    ui->bandTableView->hideColumn(5); // last_seen_freq
    ui->bandTableView->setItemDelegateForColumn(2, new UnitFormatDelegate("", 6, 0.001, ui->bandTableView));
    ui->bandTableView->setItemDelegateForColumn(3, new UnitFormatDelegate("", 6, 0.001, ui->bandTableView));
    ui->bandTableView->setItemDelegateForColumn(4,new CheckBoxDelegate(ui->bandTableView));

    bandTableModel->select();

    ui->stationCallsignEdit->setValidator(new QRegularExpressionValidator(Callsign::callsignRegEx(), this));
    ui->stationLocatorEdit->setValidator(new QRegularExpressionValidator(Gridsquare::gridRegEx(), this));
    ui->stationVUCCEdit->setValidator(new QRegularExpressionValidator(Gridsquare::gridVUCCRegEx(), this));

    static QRegularExpression comPortRE(
#if defined(Q_OS_WIN)
                                    "^COM[0-9]+$",
#else
                                    ".*",
#endif
                                    QRegularExpression::CaseInsensitiveOption);

    ui->rigPortEdit->setValidator(new QRegularExpressionValidator(comPortRE, this));
    ui->rotPortEdit->setValidator(new QRegularExpressionValidator(comPortRE, this));
    ui->cwPortEdit->setValidator(new QRegularExpressionValidator(comPortRE, this));

    iotaCompleter = new QCompleter(Data::instance()->iotaIDList(), this);
    iotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    iotaCompleter->setFilterMode(Qt::MatchContains);
    iotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->stationIOTAEdit->setCompleter(iotaCompleter);

    sotaCompleter = new QCompleter(Data::instance()->sotaIDList(), this);
    sotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    sotaCompleter->setFilterMode(Qt::MatchStartsWith);
    sotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->stationSOTAEdit->setCompleter(nullptr);

    ui->primaryCallbookCombo->addItem(tr("Disabled"), QVariant(GenericCallbook::CALLBOOK_NAME));
    ui->primaryCallbookCombo->addItem(tr("HamQTH"),   QVariant(HamQTH::CALLBOOK_NAME));
    ui->primaryCallbookCombo->addItem(tr("QRZ.com"),  QVariant(QRZ::CALLBOOK_NAME));

    ui->rigModelSelect->setCurrentIndex(ui->rigModelSelect->findData(DEFAULT_RIG_MODEL));
    ui->rotModelSelect->setCurrentIndex(ui->rotModelSelect->findData(DEFAULT_ROT_MODEL));

    ui->rigFlowControlSelect->addItem(tr("None"), Rig::SERIAL_FLOWCONTROL_NONE);
    ui->rigFlowControlSelect->addItem(tr("Hardware"), Rig::SERIAL_FLOWCONTROL_HARDWARE);
    ui->rigFlowControlSelect->addItem(tr("Software"), Rig::SERIAL_FLOWCONTROL_SOFTWARE);

    ui->rigParitySelect->addItem(tr("No"), Rig::SERIAL_PARITY_NO);
    ui->rigParitySelect->addItem(tr("Even"), Rig::SERIAL_PARITY_EVEN);
    ui->rigParitySelect->addItem(tr("Odd"), Rig::SERIAL_PARITY_ODD);
    ui->rigParitySelect->addItem(tr("Mark"), Rig::SERIAL_PARITY_MARK);
    ui->rigParitySelect->addItem(tr("Space"), Rig::SERIAL_PARITY_SPACE);

    ui->rotFlowControlSelect->addItem(tr("None"), Rotator::SERIAL_FLOWCONTROL_NONE);
    ui->rotFlowControlSelect->addItem(tr("Hardware"), Rotator::SERIAL_FLOWCONTROL_HARDWARE);
    ui->rotFlowControlSelect->addItem(tr("Software"), Rotator::SERIAL_FLOWCONTROL_SOFTWARE);

    ui->rotParitySelect->addItem(tr("No"), Rotator::SERIAL_PARITY_NO);
    ui->rotParitySelect->addItem(tr("Even"), Rotator::SERIAL_PARITY_EVEN);
    ui->rotParitySelect->addItem(tr("Odd"), Rotator::SERIAL_PARITY_ODD);
    ui->rotParitySelect->addItem(tr("Mark"), Rotator::SERIAL_PARITY_MARK);
    ui->rotParitySelect->addItem(tr("Space"), Rotator::SERIAL_PARITY_SPACE);
;
    ui->cwModelSelect->addItem(tr("Dummy"), CWKey::DUMMY_KEYER);
    ui->cwModelSelect->addItem(tr("Morse Over CAT"), CWKey::MORSEOVERCAT);
    ui->cwModelSelect->addItem(tr("WinKey v2"), CWKey::WINKEY2_KEYER);
    ui->cwModelSelect->setCurrentIndex(ui->cwModelSelect->findData(DEFAULT_CWKEY_MODEL));

    ui->cwKeyModeSelect->addItem(tr("Single Paddle"), CWKey::SINGLE_PADDLE);
    ui->cwKeyModeSelect->addItem(tr("IAMBIC A"), CWKey::IAMBIC_A);
    ui->cwKeyModeSelect->addItem(tr("IAMBIC B"), CWKey::IAMBIC_B);
    ui->cwKeyModeSelect->addItem(tr("Ultimate"), CWKey::ULTIMATE);
    ui->cwKeyModeSelect->setCurrentIndex(ui->cwKeyModeSelect->findData(CWKey::IAMBIC_B));

    connect(&sotaApi, &SOTAApi::summitResult,
            this, &SettingsDialog::sotaSummitResult);

    readSettings();
}

void SettingsDialog::save() {
    FCT_IDENTIFICATION;

    if ( stationProfManager->profileNameList().isEmpty() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Please, define at least one Station Locations Profile"));
        return;
    }

    QString pleaseModifyTXT = tr("Press <b>Modify</b> to confirm the profile changes or <b>Cancel</b>.");

    if ( ui->stationAddProfileButton->text() == tr("Modify") )
    {
        ui->tabWidget->setCurrentIndex(0);
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),pleaseModifyTXT);
        return;
    }

    if ( ui->antAddProfileButton->text() == tr("Modify") )
    {
        ui->tabWidget->setCurrentIndex(1);
        ui->equipmentTabWidget->setCurrentIndex(0);
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),pleaseModifyTXT);
        return;
    }

    if ( ui->cwAddProfileButton->text() == tr("Modify") )
    {
        ui->tabWidget->setCurrentIndex(1);
        ui->equipmentTabWidget->setCurrentIndex(1);
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),pleaseModifyTXT);
        return;
    }

    if ( ui->cwShortcutAddProfileButton->text() == tr("Modify") )
    {
        ui->tabWidget->setCurrentIndex(1);
        ui->equipmentTabWidget->setCurrentIndex(1);
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),pleaseModifyTXT);
        return;
    }

    if ( ui->rigAddProfileButton->text() == tr("Modify") )
    {
        ui->tabWidget->setCurrentIndex(1);
        ui->equipmentTabWidget->setCurrentIndex(2);
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),pleaseModifyTXT);
        return;
    }

    if ( ui->rotAddProfileButton->text() == tr("Modify") )
    {
        ui->tabWidget->setCurrentIndex(1);
        ui->equipmentTabWidget->setCurrentIndex(3);
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),pleaseModifyTXT);
        return;
    }

    writeSettings();
    accept();
}

void SettingsDialog::addRigProfile()
{
    FCT_IDENTIFICATION;

    if ( ui->rigProfileNameEdit->text().isEmpty() )
    {
        ui->rigProfileNameEdit->setPlaceholderText(tr("Must not be empty"));
        return;
    }

    if ( ! ui->rigPortEdit->text().isEmpty() )
    {
        if ( ! ui->rigPortEdit->hasAcceptableInput() )
        {
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 QMessageBox::tr("Rig port must be a valid COM port.<br>For Windows use COMxx, for unix-like OS use a path to device"));
            return;
        }
    }

    if ( ui->rigTXFreqMaxSpinBox->value() == 0.0 )
    {
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                              QMessageBox::tr("<b>TX Range</b>: Max Frequency must not be 0."));

        return;
    }

    if ( ui->rigTXFreqMaxSpinBox->value() <= ui->rigTXFreqMinSpinBox->value() )
    {
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                              QMessageBox::tr("<b>TX Range</b>: Max Frequency must not be under Min Frequency."));

        return;
    }

    if ( ui->rigAddProfileButton->text() == tr("Modify"))
    {
        ui->rigAddProfileButton->setText(tr("Add"));
    }

    RigProfile profile;


    profile.profileName = ui->rigProfileNameEdit->text();

    profile.model = ui->rigModelSelect->currentData().toInt();

    profile.txFreqStart = ui->rigTXFreqMinSpinBox->value();
    profile.txFreqEnd = ui->rigTXFreqMaxSpinBox->value();

    profile.hostname = ( ui->rigStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                QString() :
                ui->rigHostNameEdit->text();

    profile.netport = ( ui->rigStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                0 :
                ui->rigNetPortSpin->value();

    profile.portPath = ( ui->rigStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                ui->rigPortEdit->text() :
                QString();

    profile.baudrate = ( ui->rigStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                ui->rigBaudSelect->currentText().toInt() :
                0;

    profile.databits = ( ui->rigStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                ui->rigDataBitsSelect->currentText().toInt():
                0;

    profile.stopbits = ( ui->rigStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                ui->rigStopBitsSelect->currentText().toFloat() :
                0;

    profile.flowcontrol = ( ui->rigStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                ui->rigFlowControlSelect->currentData().toString() :
                QString();

    profile.parity = ( ui->rigStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                ui->rigParitySelect->currentData().toString():
                QString();

    profile.pollInterval = ( ui->rigStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                ui->rigPollIntervalSpinBox->value() :
                500; // 500ms for Internet Rigs

    profile.ritOffset = ui->rigRXOffsetSpinBox->value();
    profile.xitOffset = ui->rigTXOffsetSpinBox->value();
    profile.defaultPWR = ui->rigPWRDefaultSpinBox->value();
    profile.assignedCWKey = ui->rigAssignedCWKeyCombo->currentText();

    profile.getFreqInfo = ui->rigGetFreqCheckBox->isChecked();
    profile.getModeInfo = ui->rigGetModeCheckBox->isChecked();
    profile.getVFOInfo = ui->rigGetVFOCheckBox->isChecked();
    profile.getPWRInfo = ui->rigGetPWRCheckBox->isChecked();
    profile.getRITInfo = ui->rigGetRITCheckBox->isChecked();
    profile.getXITInfo = ui->rigGetXITCheckBox->isChecked();
    profile.getPTTInfo = ui->rigGetPTTStateCheckBox->isChecked();
    profile.QSYWiping = ui->rigQSYWipingCheckBox->isChecked();
    profile.getKeySpeed = ui->rigGetKeySpeedCheckBox->isChecked();

    rigProfManager->addProfile(profile.profileName, profile);

    refreshRigProfilesView();

    clearRigProfileForm();

}

void SettingsDialog::delRigProfile()
{
    FCT_IDENTIFICATION;

    foreach (QModelIndex index, ui->rigProfilesListView->selectionModel()->selectedRows())
    {
        rigProfManager->removeProfile(ui->rigProfilesListView->model()->data(index).toString());
        ui->rigProfilesListView->model()->removeRow(index.row());
    }
    ui->rigProfilesListView->clearSelection();

    clearRigProfileForm();
}

void SettingsDialog::doubleClickRigProfile(QModelIndex i)
{
    FCT_IDENTIFICATION;

    RigProfile profile;

    profile = rigProfManager->getProfile(ui->rigProfilesListView->model()->data(i).toString());

    ui->rigProfileNameEdit->setText(profile.profileName);

    ui->rigModelSelect->setCurrentIndex(ui->rigModelSelect->findData(profile.model));

    ui->rigPortEdit->setText(profile.portPath);
    ui->rigHostNameEdit->setText(profile.hostname);
    ui->rigNetPortSpin->setValue(profile.netport);
    ui->rigBaudSelect->setCurrentText(QString::number(profile.baudrate));
    ui->rigDataBitsSelect->setCurrentText(QString::number(profile.databits));
    ui->rigStopBitsSelect->setCurrentText(QString::number(profile.stopbits));
    ui->rigFlowControlSelect->setCurrentIndex(ui->rigFlowControlSelect->findData(profile.flowcontrol.toLower()));
    ui->rigParitySelect->setCurrentIndex(ui->rigParitySelect->findData(profile.parity.toLower()));
    ui->rigPollIntervalSpinBox->setValue(profile.pollInterval);
    ui->rigTXFreqMinSpinBox->setValue(profile.txFreqStart);
    ui->rigTXFreqMaxSpinBox->setValue(profile.txFreqEnd);
    ui->rigPWRDefaultSpinBox->setValue(profile.defaultPWR);
    ui->rigAssignedCWKeyCombo->setCurrentText(profile.assignedCWKey);
    ui->rigGetFreqCheckBox->setChecked(profile.getFreqInfo);
    ui->rigGetModeCheckBox->setChecked(profile.getModeInfo);
    ui->rigGetVFOCheckBox->setChecked(profile.getVFOInfo);
    ui->rigGetPWRCheckBox->setChecked(profile.getPWRInfo);
    ui->rigGetRITCheckBox->setChecked(profile.getRITInfo);
    ui->rigGetXITCheckBox->setChecked(profile.getXITInfo);
    ui->rigRXOffsetSpinBox->setValue(profile.ritOffset);
    ui->rigTXOffsetSpinBox->setValue(profile.xitOffset);
    ui->rigGetPTTStateCheckBox->setChecked(profile.getPTTInfo);
    ui->rigQSYWipingCheckBox->setChecked(profile.QSYWiping);
    ui->rigGetKeySpeedCheckBox->setChecked(profile.getKeySpeed);

    setUIBasedOnRigCaps(rig_get_caps(profile.model));

    ui->rigAddProfileButton->setText(tr("Modify"));
}

void SettingsDialog::clearRigProfileForm()
{
    FCT_IDENTIFICATION;

    ui->rigProfileNameEdit->setPlaceholderText(QString());
    ui->rigPortEdit->setPlaceholderText(QString());
    ui->rigHostNameEdit->setPlaceholderText(QString());

    ui->rigProfileNameEdit->clear();
    ui->rigModelSelect->setCurrentIndex(ui->rigModelSelect->findData(DEFAULT_RIG_MODEL));
    ui->rigTXFreqMinSpinBox->setValue(0.0);
    ui->rigTXFreqMaxSpinBox->setValue(0.0);
    ui->rigPWRDefaultSpinBox->setValue(100.0);
    ui->rigAssignedCWKeyCombo->setCurrentIndex(0);
    ui->rigPollIntervalSpinBox->setValue(500.0);
    ui->rigPortEdit->clear();
    ui->rigHostNameEdit->clear();
    ui->rigBaudSelect->setCurrentIndex(0);
    ui->rigDataBitsSelect->setCurrentIndex(0);
    ui->rigStopBitsSelect->setCurrentIndex(0);
    ui->rigFlowControlSelect->setCurrentIndex(0);
    ui->rigParitySelect->setCurrentIndex(0);
    ui->rigGetFreqCheckBox->setChecked(true);
    ui->rigGetModeCheckBox->setChecked(true);
    ui->rigGetVFOCheckBox->setChecked(true);
    ui->rigGetPWRCheckBox->setChecked(true);
    ui->rigGetRITCheckBox->setChecked(false);
    ui->rigGetXITCheckBox->setChecked(false);
    ui->rigRXOffsetSpinBox->setValue(0.0);
    ui->rigTXOffsetSpinBox->setValue(0.0);
    ui->rigGetPTTStateCheckBox->setChecked(false);
    ui->rigQSYWipingCheckBox->setChecked(true);
    ui->rigGetKeySpeedCheckBox->setChecked(true);
    ui->rigAddProfileButton->setText(tr("Add"));
}

void SettingsDialog::rigRXOffsetChanged(int)
{
    FCT_IDENTIFICATION;

    if ( ui->rigGetRITCheckBox->isChecked() )
    {
        ui->rigRXOffsetSpinBox->setValue(0.0);
        ui->rigRXOffsetSpinBox->setEnabled(false);
    }
    else
    {
        ui->rigRXOffsetSpinBox->setEnabled(true);
    }
}

void SettingsDialog::rigTXOffsetChanged(int)
{
    FCT_IDENTIFICATION;

    if ( ui->rigGetXITCheckBox->isChecked() )
    {
        ui->rigTXOffsetSpinBox->setValue(0.0);
        ui->rigTXOffsetSpinBox->setEnabled(false);
    }
    else
    {
        ui->rigTXOffsetSpinBox->setEnabled(true);
    }
}

void SettingsDialog::rigGetFreqChanged(int)
{
    FCT_IDENTIFICATION;

    ui->rigQSYWipingCheckBox->setEnabled(ui->rigGetFreqCheckBox->isChecked());
    ui->rigQSYWipingCheckBox->setChecked(ui->rigGetFreqCheckBox->isChecked());
}

void SettingsDialog::addRotProfile()
{
    FCT_IDENTIFICATION;

    if ( ui->rotProfileNameEdit->text().isEmpty() )
    {
        ui->rotProfileNameEdit->setPlaceholderText(tr("Must not be empty"));
        return;
    }

    if ( ! ui->rotPortEdit->text().isEmpty() )
    {
        if ( ! ui->rotPortEdit->hasAcceptableInput() )
        {
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 QMessageBox::tr("Rotator port must be a valid COM port.<br>For Windows use COMxx, for unix-like OS use a path to device"));
            return;
        }
    }

    if ( ui->rotAddProfileButton->text() == tr("Modify"))
    {
        ui->rotAddProfileButton->setText(tr("Add"));
    }

    RotProfile profile;


    profile.profileName = ui->rotProfileNameEdit->text();

    profile.model = ui->rotModelSelect->currentData().toInt();

    profile.hostname = ( ui->rotStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                QString() :
                ui->rotHostNameEdit->text();

    profile.netport = ( ui->rotStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                0 :
                ui->rotNetPortSpin->value();

    profile.portPath = ( ui->rotStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                ui->rotPortEdit->text() :
                QString();

    profile.baudrate = ( ui->rotStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                ui->rotBaudSelect->currentText().toInt() :
                0;

    profile.databits = ( ui->rotStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                ui->rotDataBitsSelect->currentText().toInt():
                0;

    profile.stopbits = ( ui->rotStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                ui->rotStopBitsSelect->currentText().toFloat() :
                0;

    profile.flowcontrol = ( ui->rotStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                ui->rotFlowControlSelect->currentData().toString() :
                QString();

    profile.parity = ( ui->rotStackedWidget->currentIndex() == WIDGET_INDEX_SERIAL_RIG ) ?
                ui->rotParitySelect->currentData().toString():
                QString();

    rotProfManager->addProfile(profile.profileName, profile);

    refreshRotProfilesView();

    clearRotProfileForm();
}

void SettingsDialog::delRotProfile()
{
    FCT_IDENTIFICATION;

    foreach (QModelIndex index, ui->rotProfilesListView->selectionModel()->selectedRows())
    {
        rotProfManager->removeProfile(ui->rotProfilesListView->model()->data(index).toString());
        ui->rotProfilesListView->model()->removeRow(index.row());
    }
    ui->rotProfilesListView->clearSelection();

    clearRotProfileForm();
}

void SettingsDialog::refreshRotProfilesView()
{
    FCT_IDENTIFICATION;

    QStringList profiles;
    profiles << rotProfManager->profileNameList();

    QStringListModel* model = static_cast<QStringListModel*>(ui->rotProfilesListView->model());
    if ( model ) model->setStringList(profiles);
}

void SettingsDialog::doubleClickRotProfile(QModelIndex i)
{
    FCT_IDENTIFICATION;

    RotProfile profile;

    profile = rotProfManager->getProfile(ui->rotProfilesListView->model()->data(i).toString());

    ui->rotProfileNameEdit->setText(profile.profileName);

    ui->rotModelSelect->setCurrentIndex(ui->rotModelSelect->findData(profile.model));
    ui->rotPortEdit->setText(profile.portPath);
    ui->rotHostNameEdit->setText(profile.hostname);
    ui->rotNetPortSpin->setValue(profile.netport);
    ui->rotBaudSelect->setCurrentText(QString::number(profile.baudrate));
    ui->rotDataBitsSelect->setCurrentText(QString::number(profile.databits));
    ui->rotStopBitsSelect->setCurrentText(QString::number(profile.stopbits));
    ui->rotFlowControlSelect->setCurrentIndex(ui->rotFlowControlSelect->findData(profile.flowcontrol.toLower()));
    ui->rotParitySelect->setCurrentIndex(ui->rotParitySelect->findData(profile.parity.toLower()));
    ui->rotAddProfileButton->setText(tr("Modify"));
}

void SettingsDialog::clearRotProfileForm()
{
    FCT_IDENTIFICATION;

    ui->rotProfileNameEdit->setPlaceholderText(QString());
    ui->rotPortEdit->setPlaceholderText(QString());
    ui->rotHostNameEdit->setPlaceholderText(QString());

    ui->rotProfileNameEdit->clear();
    ui->rotModelSelect->setCurrentIndex(ui->rotModelSelect->findData(DEFAULT_ROT_MODEL));
    ui->rotPortEdit->clear();
    ui->rotHostNameEdit->clear();
    ui->rotBaudSelect->setCurrentIndex(0);
    ui->rotDataBitsSelect->setCurrentIndex(0);
    ui->rotStopBitsSelect->setCurrentIndex(0);
    ui->rotFlowControlSelect->setCurrentIndex(0);
    ui->rotParitySelect->setCurrentIndex(0);

    ui->rotAddProfileButton->setText(tr("Add"));
}

void SettingsDialog::addAntProfile()
{
    FCT_IDENTIFICATION;

    if ( ui->antProfileNameEdit->text().isEmpty() )
    {
        ui->antProfileNameEdit->setPlaceholderText(tr("Must not be empty"));
        return;
    }

    if ( ui->antAddProfileButton->text() == tr("Modify"))
    {
        ui->antAddProfileButton->setText(tr("Add"));
    }

    AntProfile profile;

    profile.profileName = ui->antProfileNameEdit->text();
    profile.description = ui->antDescEdit->toPlainText();

    antProfManager->addProfile(profile.profileName, profile);

    refreshAntProfilesView();

    clearAntProfileForm();

}

void SettingsDialog::delAntProfile()
{
    FCT_IDENTIFICATION;

    foreach (QModelIndex index, ui->antProfilesListView->selectionModel()->selectedRows())
    {
        antProfManager->removeProfile(ui->antProfilesListView->model()->data(index).toString());
        ui->antProfilesListView->model()->removeRow(index.row());
    }
    ui->antProfilesListView->clearSelection();

    clearAntProfileForm();
}

void SettingsDialog::refreshAntProfilesView()
{
    FCT_IDENTIFICATION;

    QStringList profiles;
    profiles << antProfManager->profileNameList();

    QStringListModel* model = static_cast<QStringListModel*>(ui->antProfilesListView->model());
    if ( model ) model->setStringList(profiles);
}

void SettingsDialog::doubleClickAntProfile(QModelIndex i)
{
    AntProfile profile;

    profile = antProfManager->getProfile(ui->antProfilesListView->model()->data(i).toString());

    ui->antProfileNameEdit->setText(profile.profileName);
    ui->antDescEdit->setPlainText(profile.description);

    ui->antAddProfileButton->setText(tr("Modify"));

}

void SettingsDialog::clearAntProfileForm()
{
    FCT_IDENTIFICATION;

    ui->antProfileNameEdit->setPlaceholderText(QString());

    ui->antProfileNameEdit->clear();
    ui->antDescEdit->clear();

    ui->antAddProfileButton->setText(tr("Add"));
}

void SettingsDialog::addCWKeyProfile()
{
    FCT_IDENTIFICATION;

    if ( ui->cwProfileNameEdit->text().isEmpty() )
    {
        ui->cwProfileNameEdit->setPlaceholderText(tr("Must not be empty"));
        return;
    }

    if ( ! ui->cwPortEdit->text().isEmpty() )
    {
        if ( ! ui->cwPortEdit->hasAcceptableInput() )
        {
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 QMessageBox::tr("CW Key port must be a valid COM port.<br>For Windows use COMxx, for unix-like OS use a path to device"));
            return;
        }
    }

    CWKeyProfile cwKeyNewProfile;

    cwKeyNewProfile.model = CWKey::intToTypeID(ui->cwModelSelect->currentData().toInt());
    cwKeyNewProfile.profileName = ui->cwProfileNameEdit->text();
    cwKeyNewProfile.defaultSpeed = ui->cwDefaulSpeed->value();
    cwKeyNewProfile.keyMode = CWKey::intToModeID(ui->cwKeyModeSelect->currentData().toInt());
    cwKeyNewProfile.portPath = ui->cwPortEdit->text();
    cwKeyNewProfile.baudrate = ui->cwBaudSelect->currentText().toInt();

    QStringList noMorseCATSupportRigs;

    if ( cwKeyNewProfile.model == CWKey::MORSEOVERCAT )
    {
        // changing Key to Morse Over CAT model
        // needed to verify if all rigs where the key is assigned, supports Morse over CAT
        QStringList availableRigProfileNames = rigProfManager->profileNameList();

        for ( const QString &rigProfileName : qAsConst(availableRigProfileNames) )
        {
            qCDebug(runtime) << "Checking Rig Profile" << rigProfileName;
            RigProfile testedRig = rigProfManager->getProfile(rigProfileName);

            if ( testedRig.assignedCWKey == cwKeyNewProfile.profileName )
            {
                const struct rig_caps *caps;
                caps = rig_get_caps(testedRig.model);

                if ( caps && caps->send_morse )
                {
                    qCDebug(runtime) << testedRig.profileName << " has morse support - OK";
                }
                else
                {
                    qCDebug(runtime) << "no morse support for " << testedRig.profileName << " - must not change";
                    noMorseCATSupportRigs << testedRig.profileName;
                }
            }
        }
    }

    if ( ! noMorseCATSupportRigs.isEmpty() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Cannot change the CW Key Model to <b>Morse over CAT</b><br>No Morse over CAT support for Rig(s) <b>%1</b>").arg(noMorseCATSupportRigs.join(", ")));
        return;
    }

    if ( ui->cwAddProfileButton->text() == tr("Modify"))
    {
        ui->cwAddProfileButton->setText(tr("Add"));
    }

    cwKeyProfManager->addProfile(cwKeyNewProfile.profileName, cwKeyNewProfile);

    refreshCWKeyProfilesView();
    clearCWKeyProfileForm();
    refreshRigAssignedCWKeyCombo();
}

void SettingsDialog::delCWKeyProfile()
{
    FCT_IDENTIFICATION;


    foreach (QModelIndex index, ui->cwProfilesListView->selectionModel()->selectedRows())
    {
        QStringList  dependentRigs;
        QString removedCWProfile = ui->cwProfilesListView->model()->data(index).toString();
        QStringList availableRigProfileNames = rigProfManager->profileNameList();

        /* needed to verify whether removed Key is not used in Rig Profile as an assigned Key*/
        for ( const QString &rigProfileName : qAsConst(availableRigProfileNames) )
        {
            qCDebug(runtime) << "Checking Rig Profile" << rigProfileName;
            RigProfile testedRig = rigProfManager->getProfile(rigProfileName);
            if ( testedRig.assignedCWKey == removedCWProfile )
            {
                dependentRigs << testedRig.profileName;
            }
        }

        if ( dependentRigs.isEmpty() )
        {
            cwKeyProfManager->removeProfile(removedCWProfile);
            ui->cwProfilesListView->model()->removeRow(index.row());
        }
        else
        {
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 QMessageBox::tr("Cannot delete the CW Key Profile<br>The CW Key Profile is used by Rig(s): <b>%1</b>").arg(dependentRigs.join(", ")));
        }

        dependentRigs.clear();
    }
    ui->cwProfilesListView->clearSelection();

    clearCWKeyProfileForm();

    refreshRigAssignedCWKeyCombo();
}

void SettingsDialog::refreshCWKeyProfilesView()
{
    FCT_IDENTIFICATION;

    QStringList profiles;
    profiles << cwKeyProfManager->profileNameList();

    QStringListModel* model = static_cast<QStringListModel*>(ui->cwProfilesListView->model());
    if ( model ) model->setStringList(profiles);
}

void SettingsDialog::doubleClickCWKeyProfile(QModelIndex i)
{
    FCT_IDENTIFICATION;

    CWKeyProfile profile;

    profile = cwKeyProfManager->getProfile(ui->cwProfilesListView->model()->data(i).toString());

    ui->cwProfileNameEdit->setText(profile.profileName);
    ui->cwModelSelect->setCurrentIndex(ui->cwModelSelect->findData(profile.model));
    ui->cwDefaulSpeed->setValue(profile.defaultSpeed);
    ui->cwKeyModeSelect->setCurrentIndex(ui->cwKeyModeSelect->findData(profile.keyMode));
    ui->cwPortEdit->setText(profile.portPath);
    ui->cwBaudSelect->setCurrentText(QString::number(profile.baudrate));

    ui->cwAddProfileButton->setText(tr("Modify"));
}

void SettingsDialog::clearCWKeyProfileForm()
{
    FCT_IDENTIFICATION;

    ui->cwProfileNameEdit->setPlaceholderText(QString());
    ui->cwProfileNameEdit->clear();
    ui->cwModelSelect->setCurrentIndex(ui->cwModelSelect->findData(DEFAULT_CWKEY_MODEL));
    ui->cwKeyModeSelect->setCurrentIndex(ui->cwKeyModeSelect->findData(CWKey::IAMBIC_B));
    ui->cwDefaulSpeed->setValue(20);
    ui->cwPortEdit->clear();
    ui->cwBaudSelect->setCurrentIndex(0);

    ui->cwAddProfileButton->setText(tr("Add"));
}

void SettingsDialog::addCWShortcutProfile()
{
    FCT_IDENTIFICATION;

    if ( ui->cwShortcutProfileNameEdit->text().isEmpty() )
    {
        ui->cwShortcutProfileNameEdit->setPlaceholderText(tr("Must not be empty"));
        return;
    }

    if ( ui->cwShortcutAddProfileButton->text() == tr("Modify"))
    {
        ui->cwShortcutAddProfileButton->setText(tr("Add"));
    }

    CWShortcutProfile profile;

    profile.profileName = ui->cwShortcutProfileNameEdit->text();

    profile.shortDescs[0] = ui->cwShortcutF1ShortEdit->text();
    profile.macros[0] = ui->cwShortcutF1MacroEdit->text();

    profile.shortDescs[1] = ui->cwShortcutF2ShortEdit->text();
    profile.macros[1] = ui->cwShortcutF2MacroEdit->text();

    profile.shortDescs[2] = ui->cwShortcutF3ShortEdit->text();
    profile.macros[2] = ui->cwShortcutF3MacroEdit->text();

    profile.shortDescs[3] = ui->cwShortcutF4ShortEdit->text();
    profile.macros[3] = ui->cwShortcutF4MacroEdit->text();

    profile.shortDescs[4] = ui->cwShortcutF5ShortEdit->text();
    profile.macros[4] = ui->cwShortcutF5MacroEdit->text();

    profile.shortDescs[5] = ui->cwShortcutF6ShortEdit->text();
    profile.macros[5] = ui->cwShortcutF6MacroEdit->text();

    profile.shortDescs[6] = ui->cwShortcutF7ShortEdit->text();
    profile.macros[6] = ui->cwShortcutF7MacroEdit->text();

    cwShortcutProfManager->addProfile(profile.profileName, profile);

    refreshCWShortcutProfilesView();

    clearCWShortcutProfileForm();
}

void SettingsDialog::delCWShortcutProfile()
{
    FCT_IDENTIFICATION;

    foreach (QModelIndex index, ui->cwShortcutListView->selectionModel()->selectedRows())
    {
        cwShortcutProfManager->removeProfile(ui->cwShortcutListView->model()->data(index).toString());
        ui->cwShortcutListView->model()->removeRow(index.row());
    }
    ui->cwShortcutListView->clearSelection();

    clearCWShortcutProfileForm();
}

void SettingsDialog::refreshCWShortcutProfilesView()
{
    FCT_IDENTIFICATION;

    QStringList profiles;
    profiles << cwShortcutProfManager->profileNameList();

    QStringListModel* model = static_cast<QStringListModel*>(ui->cwShortcutListView->model());
    if ( model ) model->setStringList(profiles);
}

void SettingsDialog::doubleClickCWShortcutProfile(QModelIndex i)
{
    FCT_IDENTIFICATION;

    CWShortcutProfile profile;

    profile = cwShortcutProfManager->getProfile(ui->cwShortcutListView->model()->data(i).toString());

    ui->cwShortcutProfileNameEdit->setText(profile.profileName);

    ui->cwShortcutF1ShortEdit->setText(profile.shortDescs[0]);
    ui->cwShortcutF1MacroEdit->setText(profile.macros[0]);

    ui->cwShortcutF2ShortEdit->setText(profile.shortDescs[1]);
    ui->cwShortcutF2MacroEdit->setText(profile.macros[1]);

    ui->cwShortcutF3ShortEdit->setText(profile.shortDescs[2]);
    ui->cwShortcutF3MacroEdit->setText(profile.macros[2]);

    ui->cwShortcutF4ShortEdit->setText(profile.shortDescs[3]);
    ui->cwShortcutF4MacroEdit->setText(profile.macros[3]);

    ui->cwShortcutF5ShortEdit->setText(profile.shortDescs[4]);
    ui->cwShortcutF5MacroEdit->setText(profile.macros[4]);

    ui->cwShortcutF6ShortEdit->setText(profile.shortDescs[5]);
    ui->cwShortcutF6MacroEdit->setText(profile.macros[5]);

    ui->cwShortcutF7ShortEdit->setText(profile.shortDescs[6]);
    ui->cwShortcutF7MacroEdit->setText(profile.macros[6]);

    ui->cwShortcutAddProfileButton->setText(tr("Modify"));
}

void SettingsDialog::clearCWShortcutProfileForm()
{
    FCT_IDENTIFICATION;

    ui->cwShortcutProfileNameEdit->setPlaceholderText(QString());
    ui->cwShortcutProfileNameEdit->clear();

    ui->cwShortcutF1ShortEdit->clear();
    ui->cwShortcutF1MacroEdit->clear();

    ui->cwShortcutF2ShortEdit->clear();
    ui->cwShortcutF2MacroEdit->clear();

    ui->cwShortcutF3ShortEdit->clear();
    ui->cwShortcutF3MacroEdit->clear();

    ui->cwShortcutF4ShortEdit->clear();
    ui->cwShortcutF4MacroEdit->clear();

    ui->cwShortcutF5ShortEdit->clear();
    ui->cwShortcutF5MacroEdit->clear();

    ui->cwShortcutF6ShortEdit->clear();
    ui->cwShortcutF6MacroEdit->clear();

    ui->cwShortcutF7ShortEdit->clear();
    ui->cwShortcutF7MacroEdit->clear();

    ui->cwShortcutAddProfileButton->setText(tr("Add"));
}

void SettingsDialog::refreshRigProfilesView()
{
    FCT_IDENTIFICATION;

    QStringList profiles;
    profiles << rigProfManager->profileNameList();

    QStringListModel* model = static_cast<QStringListModel*>(ui->rigProfilesListView->model());
    if ( model ) model->setStringList(profiles);
}

void SettingsDialog::refreshStationProfilesView()
{
    FCT_IDENTIFICATION;

    QStringList profiles;
    profiles << stationProfManager->profileNameList();

    QStringListModel* model = static_cast<QStringListModel*>(ui->stationProfilesListView->model());
    if ( model ) model->setStringList(profiles);
}

void SettingsDialog::addStationProfile()
{
    FCT_IDENTIFICATION;

    if ( ui->stationProfileNameEdit->text().isEmpty() )
    {
        ui->stationProfileNameEdit->setPlaceholderText(tr("Must not be empty"));
        return;
    }

    if ( ui->stationCallsignEdit->text().isEmpty() )
    {
        ui->stationCallsignEdit->setPlaceholderText(tr("Must not be empty"));
        return;
    }

    if ( ui->stationLocatorEdit->text().isEmpty() )
    {
        ui->stationLocatorEdit->setPlaceholderText(tr("Must not be empty"));
        return;
    }

    if ( ! ui->stationCallsignEdit->hasAcceptableInput() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Callsign has an invalid format"));
        return;
    }

    if ( ! ui->stationLocatorEdit->hasAcceptableInput() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Locator has an invalid format"));
        return;
    }

    if ( ! ui->stationVUCCEdit->text().isEmpty() )
    {
        if ( ! ui->stationVUCCEdit->hasAcceptableInput() )
        {
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 QMessageBox::tr("VUCC Locator has an invalid format (must be 2 or 4 locators separated by ',')"));
            return;
        }
    }

    if ( ui->stationAddProfileButton->text() == tr("Modify"))
    {
        ui->stationAddProfileButton->setText(tr("Add"));
    }

    StationProfile profile;

    profile.profileName = ui->stationProfileNameEdit->text();
    profile.callsign = ui->stationCallsignEdit->text().toUpper();
    profile.locator = ui->stationLocatorEdit->text().toUpper();
    profile.operatorName = ui->stationOperatorEdit->text();
    profile.qthName = ui->stationQTHEdit->text();
    profile.iota = ui->stationIOTAEdit->text().toUpper();
    profile.sota = ui->stationSOTAEdit->text().toUpper();
    profile.sig = ui->stationSIGEdit->text();
    profile.sigInfo = ui->stationSIGInfoEdit->text();
    profile.vucc = ui->stationVUCCEdit->text().toUpper();
    profile.wwff = ui->stationWWFFEdit->text().toUpper();

    stationProfManager->addProfile(profile.profileName, profile);

    refreshStationProfilesView();

    clearStationProfileForm();

}

void SettingsDialog::deleteStationProfile()
{
    FCT_IDENTIFICATION;

    foreach (QModelIndex index, ui->stationProfilesListView->selectionModel()->selectedRows()) {
        stationProfManager->removeProfile(ui->stationProfilesListView->model()->data(index).toString());
        ui->stationProfilesListView->model()->removeRow(index.row());
    }
    ui->stationProfilesListView->clearSelection();

    clearStationProfileForm();
}

void SettingsDialog::doubleClickStationProfile(QModelIndex i)
{
    FCT_IDENTIFICATION;

    StationProfile profile;

    profile = stationProfManager->getProfile(ui->stationProfilesListView->model()->data(i).toString());

    ui->stationProfileNameEdit->setText(profile.profileName);
    ui->stationCallsignEdit->setText(profile.callsign);
    ui->stationLocatorEdit->setText(profile.locator);
    ui->stationOperatorEdit->setText(profile.operatorName);
    ui->stationQTHEdit->setText(profile.qthName);
    ui->stationIOTAEdit->setText(profile.iota);
    ui->stationSOTAEdit->setText(profile.sota);
    ui->stationSIGEdit->setText(profile.sig);
    ui->stationSIGInfoEdit->setText(profile.sigInfo);
    ui->stationVUCCEdit->setText(profile.vucc);
    ui->stationWWFFEdit->setText(profile.wwff);

    ui->stationAddProfileButton->setText(tr("Modify"));
}

void SettingsDialog::clearStationProfileForm()
{
    FCT_IDENTIFICATION;

    ui->stationProfileNameEdit->clear();
    ui->stationProfileNameEdit->setPlaceholderText(QString());
    ui->stationCallsignEdit->clear();
    ui->stationCallsignEdit->setPlaceholderText(QString());
    ui->stationLocatorEdit->clear();
    ui->stationLocatorEdit->setPlaceholderText(QString());
    ui->stationOperatorEdit->clear();
    ui->stationQTHEdit->clear();
    ui->stationSOTAEdit->clear();
    ui->stationIOTAEdit->clear();
    ui->stationSIGEdit->clear();
    ui->stationSIGInfoEdit->clear();
    ui->stationVUCCEdit->clear();
    ui->stationWWFFEdit->clear();

    ui->stationAddProfileButton->setText(tr("Add"));
}

/* This function is called when an user change Rig Combobox */
/* new rig entered */
void SettingsDialog::rigChanged(int index)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<index;

    const struct rig_caps *caps;

    int rigID = ui->rigModelSelect->currentData().toInt();

    caps = rig_get_caps(rigID);

    if ( caps )
    {
        if ( Rig::isNetworkRig(caps) )
        {
            ui->rigStackedWidget->setCurrentIndex(1);
        }
        else
        {
            ui->rigStackedWidget->setCurrentIndex(0);
            ui->rigDataBitsSelect->setCurrentText(QString::number(caps->serial_data_bits));
            ui->rigStopBitsSelect->setCurrentText(QString::number(caps->serial_stop_bits));
        }

        /* Set default rig Caps */
        ui->rigGetFreqCheckBox->setEnabled(true);
        ui->rigGetFreqCheckBox->setChecked(true);

        ui->rigGetModeCheckBox->setEnabled(true);
        ui->rigGetModeCheckBox->setChecked(true);

        ui->rigGetVFOCheckBox->setEnabled(true);
        ui->rigGetVFOCheckBox->setChecked(true);

        ui->rigGetPWRCheckBox->setEnabled(true);
        ui->rigGetPWRCheckBox->setChecked(true);

        ui->rigGetRITCheckBox->setEnabled(true);
        ui->rigGetRITCheckBox->setChecked(false);

        ui->rigGetXITCheckBox->setEnabled(true);
        ui->rigGetXITCheckBox->setChecked(false);

        ui->rigGetPTTStateCheckBox->setEnabled(true);
        ui->rigGetPTTStateCheckBox->setChecked(false);

        ui->rigQSYWipingCheckBox->setEnabled(true);
        ui->rigQSYWipingCheckBox->setChecked(true);

        ui->rigGetKeySpeedCheckBox->setEnabled(true);
        ui->rigGetKeySpeedCheckBox->setChecked(true);

        /* disable what is unimplemented */
        setUIBasedOnRigCaps(caps);
    }
    else
    {
        ui->rigStackedWidget->setCurrentIndex(0);
    }

    refreshRigAssignedCWKeyCombo();
}

void SettingsDialog::rotChanged(int index)
{
    FCT_IDENTIFICATION;

    const struct rot_caps *caps;

    QModelIndex rot_index = ui->rotModelSelect->model()->index(index, 0);
    caps = rot_get_caps(rot_index.internalId());    

    if ( caps )
    {
        if ( Rotator::isNetworkRot(caps) )
        {
            ui->rotStackedWidget->setCurrentIndex(1);
        }
        else
        {
            ui->rotStackedWidget->setCurrentIndex(0);
        }
    }
    else
    {
        ui->rotStackedWidget->setCurrentIndex(0);
    }

}

void SettingsDialog::cwKeyChanged(int)
{
    FCT_IDENTIFICATION;

    if ( ui->cwModelSelect->currentData().toInt() == CWKey::MORSEOVERCAT )
    {
        ui->cwBaudSelect->setEnabled(false);
        ui->cwPortEdit->setEnabled(false);
        ui->cwPortEdit->clear();
        ui->cwKeyModeSelect->setEnabled(false);
        return;
    }
    else
    {
        ui->cwBaudSelect->setEnabled(true);
        ui->cwPortEdit->setEnabled(true);
        ui->cwKeyModeSelect->setEnabled(true);
    }

    if ( ui->cwModelSelect->currentData().toInt() == CWKey::WINKEY2_KEYER )
    {
        ui->cwBaudSelect->setCurrentText("1200");
    }
    else
    {
        ui->cwBaudSelect->setCurrentText("115200");
    }
}

void SettingsDialog::rigStackWidgetChanged(int)
{
    FCT_IDENTIFICATION;

    ui->rigPortEdit->clear();
    ui->rigHostNameEdit->clear();
}

void SettingsDialog::rotStackWidgetChanged(int)
{
    FCT_IDENTIFICATION;

    ui->rotPortEdit->clear();
    ui->rotHostNameEdit->clear();
}

void SettingsDialog::cwKeyStackWidgetChanged(int)
{
    FCT_IDENTIFICATION;

    ui->cwPortEdit->clear();
    ui->cwHostNameEdit->clear();
}

void SettingsDialog::tqslPathBrowse()
{
    FCT_IDENTIFICATION;

    QString filename = QFileDialog::getOpenFileName(this,
                                                    tr("Select File"),
                                                    "",
#if defined(Q_OS_WIN)
                                                    "TQSL (*.exe)"
#elif defined(Q_OS_MACOS)
                                                    "TQSL (*.app)"
#else
                                                    "TQSL (tqsl)"
#endif
                                                   );
    if ( !filename.isEmpty() )
    {
        ui->tqslPathEdit->setText(filename);
    }
}

void SettingsDialog::adjustCallsignTextColor()
{
    FCT_IDENTIFICATION;

    setValidationResultColor(ui->stationCallsignEdit);
}

void SettingsDialog::adjustLocatorTextColor()
{
    FCT_IDENTIFICATION;

    setValidationResultColor(ui->stationLocatorEdit);
}

void SettingsDialog::adjustVUCCLocatorTextColor()
{
    FCT_IDENTIFICATION;

    setValidationResultColor(ui->stationVUCCEdit);
}

void SettingsDialog::adjustRotCOMPortTextColor()
{
    FCT_IDENTIFICATION;

    setValidationResultColor(ui->cwPortEdit);
}

void SettingsDialog::adjustRigCOMPortTextColor()
{
    FCT_IDENTIFICATION;

    setValidationResultColor(ui->rigPortEdit);
}

void SettingsDialog::adjustCWKeyCOMPortTextColor()
{
    FCT_IDENTIFICATION;

    setValidationResultColor(ui->cwPortEdit);
}

void SettingsDialog::eqslDirBrowse()
{
    FCT_IDENTIFICATION;

    QString dir = QFileDialog::getExistingDirectory(this,
                                                    tr("Select Directory"),
#if defined (Q_OS_WIN)
                                                    "C:\\",
#else
                                                    "~",
#endif
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if ( !dir.isEmpty() )
    {
        ui->eqslFolderPathEdit->setText(dir);
    }
}

void SettingsDialog::paperDirBrowse()
{
    FCT_IDENTIFICATION;

    QString dir = QFileDialog::getExistingDirectory(this,
                                                    tr("Select Directory"),
#if defined (Q_OS_WIN)
                                                    "C:\\",
#else
                                                    "~",
#endif
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if ( !dir.isEmpty() )
    {
        ui->paperFolderPathEdit->setText(dir);
    }
}

void SettingsDialog::cancelled()
{
    FCT_IDENTIFICATION;

    if ( stationProfManager->profileNameList().isEmpty() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Please, define at least one Station Locations Profile"));
        return;
    }

    reject();
}

void SettingsDialog::sotaChanged(QString newSOTA)
{
    FCT_IDENTIFICATION;

    if ( newSOTA.length() >= 3 )
    {
        ui->stationSOTAEdit->setCompleter(sotaCompleter);
    }
    else
    {
        ui->stationSOTAEdit->setCompleter(nullptr);
    }
}

void SettingsDialog::sotaSummitResult(const QMap<QString, QString> &data)
{
    FCT_IDENTIFICATION;

    if ( data.value("summitCode") != ui->stationSOTAEdit->text() )
    {
        return;
    }
    // always replace locator and QTH
    ui->stationLocatorEdit->setText(data.value("locator").toUpper());
    ui->stationQTHEdit->setText(data.value("name"));
}

void SettingsDialog::sotaEditFinished()
{
    FCT_IDENTIFICATION;

    sotaApi.querySummit(ui->stationSOTAEdit->text());
}

void SettingsDialog::primaryCallbookChanged(int index)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << index;

    QString primaryCallbookSelection = ui->primaryCallbookCombo->itemData(index).toString();

    if ( primaryCallbookSelection == GenericCallbook::CALLBOOK_NAME )
    {
        ui->secondaryCallbookCombo->clear();
        ui->secondaryCallbookCombo->setEnabled(false);
    }
    else if ( primaryCallbookSelection == HamQTH::CALLBOOK_NAME )
    {
        ui->secondaryCallbookCombo->setEnabled(true);
        ui->secondaryCallbookCombo->clear();
        ui->secondaryCallbookCombo->addItem(tr("Disabled"), QVariant(GenericCallbook::CALLBOOK_NAME));
        ui->secondaryCallbookCombo->addItem(tr("QRZ.com"),  QVariant(QRZ::CALLBOOK_NAME));
    }
    else if ( primaryCallbookSelection == QRZ::CALLBOOK_NAME )
    {
        ui->secondaryCallbookCombo->setEnabled(true);
        ui->secondaryCallbookCombo->clear();
        ui->secondaryCallbookCombo->addItem(tr("Disabled"), QVariant(GenericCallbook::CALLBOOK_NAME));
        ui->secondaryCallbookCombo->addItem(tr("HamQTH"),  QVariant(HamQTH::CALLBOOK_NAME));
    }
}

void SettingsDialog::secondaryCallbookChanged(int index)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << index;
}

void SettingsDialog::readSettings() {
    FCT_IDENTIFICATION;

    QSettings settings;

    QStringList profiles = stationProfManager->profileNameList();
    (static_cast<QStringListModel*>(ui->stationProfilesListView->model()))->setStringList(profiles);

    QStringList rigs = rigProfManager->profileNameList();
    (static_cast<QStringListModel*>(ui->rigProfilesListView->model()))->setStringList(rigs);

    QStringList rots = rotProfManager->profileNameList();
    (static_cast<QStringListModel*>(ui->rotProfilesListView->model()))->setStringList(rots);

    QStringList ants = antProfManager->profileNameList();
    (static_cast<QStringListModel*>(ui->antProfilesListView->model()))->setStringList(ants);

    QStringList cwKeys = cwKeyProfManager->profileNameList();
    (static_cast<QStringListModel*>(ui->cwProfilesListView->model()))->setStringList(cwKeys);

    QStringList cwShortcut = cwShortcutProfManager->profileNameList();
    (static_cast<QStringListModel*>(ui->cwShortcutListView->model()))->setStringList(cwShortcut);

    refreshRigAssignedCWKeyCombo();

    /************/
    /* Callbook */
    /************/

    int primaryCallbookIndex = ui->primaryCallbookCombo->findData(settings.value(GenericCallbook::CONFIG_PRIMARY_CALLBOOK_KEY,
                                                                                 GenericCallbook::CALLBOOK_NAME));

    ui->primaryCallbookCombo->setCurrentIndex(primaryCallbookIndex);

    int secondaryCallbookIndex = ui->secondaryCallbookCombo->findData(settings.value(GenericCallbook::CONFIG_SECONDARY_CALLBOOK_KEY,
                                                                                     GenericCallbook::CALLBOOK_NAME));

    ui->secondaryCallbookCombo->setCurrentIndex(secondaryCallbookIndex);

    ui->hamQthUsernameEdit->setText(HamQTH::getUsername());
    ui->hamQthPasswordEdit->setText(HamQTH::getPassword());

    ui->qrzUsernameEdit->setText(QRZ::getUsername());
    ui->qrzPasswordEdit->setText(QRZ::getPassword());

    /********/
    /* LoTW */
    /********/
    ui->lotwUsernameEdit->setText(Lotw::getUsername());
    ui->lotwPasswordEdit->setText(Lotw::getPassword());
    ui->tqslPathEdit->setText(Lotw::getTQSLPath());

    /***********/
    /* ClubLog */
    /***********/
    ui->clublogEmailEdit->setText(ClubLog::getEmail());
    ui->clublogCallsignEdit->setText(ClubLog::getRegisteredCallsign());
    ui->clublogPasswordEdit->setText(ClubLog::getPassword());

    /********/
    /* eQSL */
    /********/
    ui->eqslUsernameEdit->setText(EQSL::getUsername());
    ui->eqslPasswordEdit->setText(EQSL::getPassword());
    ui->eqslFolderPathEdit->setText(EQSL::getQSLImageFolder());

    /***********/
    /* QRZ.COM */
    /***********/
    ui->qrzApiKeyEdit->setText(QRZ::getLogbookAPIKey());

    /*************/
    /* Paper QSL */
    /*************/
    ui->paperFolderPathEdit->setText(PaperQSL::getQSLImageFolder());

    /********/
    /* DXCC */
    /********/
    if (!settings.value("dxcc/start").isNull()) {
       ui->dxccStartDateCheckBox->setCheckState(Qt::Checked);
       ui->dxccStartDate->setDate(settings.value("dxcc/start").toDate());
    }
    else {
        ui->dxccStartDateCheckBox->setCheckState(Qt::Unchecked);
        ui->dxccStartDate->setDate(QDate::currentDate());
    }

    /***********/
    /* NETWORK */
    /***********/

    ui->wsjtPortSpin->setValue(Wsjtx::getConfigPort());
    ui->wsjtForwardEdit->setText(Wsjtx::getConfigForwardAddresses());

    ui->notifLogIDEdit->setText(LogParam::getParam("logid"));
    ui->notifQSOEdit->setText(NetworkNotification::getNotifQSOAdiAddrs());
    ui->notifDXSpotsEdit->setText(NetworkNotification::getNotifDXSpotAddrs());
    ui->notifWSJTXCQSpotsEdit->setText(NetworkNotification::getNotifWSJTXCQSpotAddrs());
    ui->notifSpotAlertEdit->setText(NetworkNotification::getNotifSpotAlertAddrs());

    /******************/
    /* END OF Reading */
    /******************/

    //hamlib has hardcoded port number. Therefore we disable the SpinBox
    //until hamlib guyes fix it.
    ui->rigNetPortSpin->setDisabled(true);
    ui->rotNetPortSpin->setDisabled(true);
}

void SettingsDialog::writeSettings() {
    FCT_IDENTIFICATION;

    QSettings settings;

    stationProfManager->save();
    rigProfManager->save();
    rotProfManager->save();
    antProfManager->save();
    cwKeyProfManager->save();
    cwShortcutProfManager->save();

    /************/
    /* Callbook */
    /************/
    HamQTH::saveUsernamePassword(ui->hamQthUsernameEdit->text(),
                                 ui->hamQthPasswordEdit->text());

    QRZ::saveUsernamePassword(ui->qrzUsernameEdit->text(),
                              ui->qrzPasswordEdit->text());

    settings.setValue(GenericCallbook::CONFIG_PRIMARY_CALLBOOK_KEY,
                      ui->primaryCallbookCombo->itemData(ui->primaryCallbookCombo->currentIndex()).toString());
    settings.setValue(GenericCallbook::CONFIG_SECONDARY_CALLBOOK_KEY,
                      ui->secondaryCallbookCombo->itemData(ui->secondaryCallbookCombo->currentIndex()).toString());

    /********/
    /* LoTW */
    /********/

    Lotw::saveUsernamePassword(ui->lotwUsernameEdit->text(),
                               ui->lotwPasswordEdit->text());
    Lotw::saveTQSLPath(ui->tqslPathEdit->text());

    /***********/
    /* ClubLog */
    /***********/
    ClubLog::saveRegistredCallsign(ui->clublogCallsignEdit->text());
    ClubLog::saveUsernamePassword(ui->clublogEmailEdit->text(),
                                  ui->clublogPasswordEdit->text());

    /********/
    /* eQSL */
    /********/

    EQSL::saveUsernamePassword(ui->eqslUsernameEdit->text(),
                               ui->eqslPasswordEdit->text());
    EQSL::saveQSLImageFolder(ui->eqslFolderPathEdit->text());

    /***********/
    /* QRZ.COM */
    /***********/
    QRZ::saveLogbookAPI(ui->qrzApiKeyEdit->text());


    /*********/
    /* DXCC  */
    /*********/
    if (ui->dxccStartDateCheckBox->isChecked()) {
        settings.setValue("dxcc/start", ui->dxccStartDate->date());
    }
    else {
        settings.setValue("dxcc/start", QVariant());
    }

    /*************/
    /* Paper QSL */
    /*************/
    PaperQSL::saveQSLImageFolder(ui->paperFolderPathEdit->text());

    /***********/
    /* NETWORK */
    /***********/
    Wsjtx::saveConfigPort(ui->wsjtPortSpin->value());
    Wsjtx::saveConfigForwardAddresses(ui->wsjtForwardEdit->text());

    NetworkNotification::saveNotifQSOAdiAddrs(ui->notifQSOEdit->text());
    NetworkNotification::saveNotifDXSpotAddrs(ui->notifDXSpotsEdit->text());
    NetworkNotification::saveNotifWSJTXCQSpotAddrs(ui->notifWSJTXCQSpotsEdit->text());
    NetworkNotification::saveNotifSpotAlertAddrs(ui->notifSpotAlertEdit->text());
}

/* this function is called when user modify rig progile
 * there may be situations where hamlib change the cap
 * for rig and it is necessary to change the settings of the rig.
 * This feature does it */
void SettingsDialog::setUIBasedOnRigCaps(const struct rig_caps *caps)
{
    FCT_IDENTIFICATION;

    if ( caps )
    {
        /* due to a hamlib issue #855 (https://github.com/Hamlib/Hamlib/issues/855)
         * the PWR will be disabled for 4.3.x
         * if someone tells me how to make a nice version identification of hamlib
         * under Win / Lin / Mac , then I'll be happy to change it
         */

        if ( Rig::isNetworkRig(caps)
#if !defined(Q_OS_WIN)
             && ( QString(hamlib_version).contains("4.2.")
                  || QString(hamlib_version).contains("4.3.") )
#endif
             )
        {
            ui->rigGetPWRCheckBox->setEnabled(false);
            ui->rigGetPWRCheckBox->setChecked(false);
        }

        if ( ! caps->get_freq )
        {
            ui->rigGetFreqCheckBox->setEnabled(false);
            ui->rigGetFreqCheckBox->setChecked(false);
        }

        if ( ! caps->get_mode )
        {
            ui->rigGetModeCheckBox->setEnabled(false);
            ui->rigGetModeCheckBox->setChecked(false);
        }

        if ( ! caps->get_vfo )
        {
            ui->rigGetVFOCheckBox->setEnabled(false);
            ui->rigGetVFOCheckBox->setChecked(false);
        }

        if ( ! ((caps->has_get_level) & (RIG_LEVEL_RFPOWER))
             || ! caps->power2mW )
        {
            ui->rigGetPWRCheckBox->setEnabled(false);
            ui->rigGetPWRCheckBox->setChecked(false);
        }

        if ( ! caps->get_rit
             || ! ((caps->has_get_func) & (RIG_FUNC_RIT)) )
        {
            ui->rigGetRITCheckBox->setEnabled(false);
            ui->rigGetRITCheckBox->setChecked(false);
        }

        if ( ! caps->get_xit
             || ! ((caps->has_get_func) & (RIG_FUNC_XIT)) )
        {
            ui->rigGetXITCheckBox->setEnabled(false);
            ui->rigGetXITCheckBox->setChecked(false);
        }

        if ( ! caps->get_ptt
             || !(caps->ptt_type == RIG_PTT_RIG
                   || caps->ptt_type == RIG_PTT_RIG_MICDATA))   //currently only CAT PTT is supported
        {
            ui->rigGetPTTStateCheckBox->setEnabled(false);
            ui->rigGetPTTStateCheckBox->setChecked(false);
        }

        if ( ! ui->rigGetFreqCheckBox->isChecked() )
        {
            ui->rigQSYWipingCheckBox->setEnabled(false);
            ui->rigQSYWipingCheckBox->setChecked(false);
        }

        if ( ! ((caps->has_get_level) & (RIG_LEVEL_KEYSPD)) )
        {
            ui->rigGetKeySpeedCheckBox->setEnabled(false);
            ui->rigGetKeySpeedCheckBox->setChecked(false);
        }
    }
}

/* Based on selected Rig model, it is needed to prepare AssignedCWKeyCombo
 * content
 * The combo has to contain only supported keyes
 * If selected rig does not support MORSE over CAT then Combo must not contain
 * CW Key profiles where Morse Over CAT is used.
 */
void SettingsDialog::refreshRigAssignedCWKeyCombo()
{
    FCT_IDENTIFICATION;

    QString cwKeyName = ui->rigAssignedCWKeyCombo->currentText();
    QStringList availableCWProfileNames = cwKeyProfManager->profileNameList();
    QStringList approvedCWProfiles;
    const struct rig_caps *selectedRigCaps;
    int rigID = ui->rigModelSelect->currentData().toInt();

    selectedRigCaps = rig_get_caps(rigID);

    approvedCWProfiles << " "; // add empty profile (like NONE)

    if ( selectedRigCaps && selectedRigCaps->send_morse )
    {
        approvedCWProfiles << availableCWProfileNames;
    }
    else
    {
        // remove unsupported Morse Over CAT Profile Names
        for (const QString &cwProfileName : qAsConst(availableCWProfileNames))
        {
            CWKeyProfile testedKey = cwKeyProfManager->getProfile(cwProfileName);
            if ( testedKey.model != CWKey::MORSEOVERCAT )
            {
                approvedCWProfiles << cwProfileName;
            }
        }
    }

 /*
    for (const QString &cwProfileName : qAsConst(availableCWProfileNames))
    {
        CWKeyProfile testedKey = cwKeyProfManager->getProfile(cwProfileName);

        if ( testedKey.model == CWKey::MORSEOVERCAT )
        {
            //need to check whether selected rig support Morse over CAT
            if ( selectedRigCaps
                 && selectedRigCaps->send_morse )
            {
                // the rig has Morse Send function
                approvedCWProfiles << cwProfileName;
            }
            else
            {
                // do not add it because we know nothing about the rig
            }
        }
        else
        {
            // it is not Morse Over CAT key therefore add it automatically
            approvedCWProfiles << cwProfileName;
        }
    }
*/
    QStringListModel* model = static_cast<QStringListModel*>(ui->rigAssignedCWKeyCombo->model());
    if ( model ) model->setStringList(approvedCWProfiles);

    ui->rigAssignedCWKeyCombo->setCurrentText(cwKeyName);
}

void SettingsDialog::setValidationResultColor(QLineEdit *editBox)
{
    FCT_IDENTIFICATION;

    QPalette p;

    if ( ! editBox )
    {
        return;
    }

    if ( ! editBox->hasAcceptableInput() )
    {
        p.setColor(QPalette::Text,Qt::red);
    }
    else
    {
        p.setColor(QPalette::Text,qApp->palette().text().color());
    }
    editBox->setPalette(p);
}

SettingsDialog::~SettingsDialog() {
    FCT_IDENTIFICATION;

    modeTableModel->deleteLater();
    bandTableModel->deleteLater();
    sotaCompleter->deleteLater();
    iotaCompleter->deleteLater();
    delete ui;
}
