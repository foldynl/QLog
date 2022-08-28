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
    ui(new Ui::SettingsDialog)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    RigTypeModel* rigTypeModel = new RigTypeModel(this);
    ui->rigModelSelect->setModel(rigTypeModel);

    RotTypeModel* rotTypeModel = new RotTypeModel(this);
    ui->rotModelSelect->setModel(rotTypeModel);

    QStringListModel* rigModel = new QStringListModel();
    ui->rigProfilesListView->setModel(rigModel);

    QStringListModel* rotModel = new QStringListModel();
    ui->rotProfilesListView->setModel(rotModel);

    QStringListModel* antModel = new QStringListModel();
    ui->antProfilesListView->setModel(antModel);

    QStringListModel* cwKeyModel = new QStringListModel();
    ui->cwProfilesListView->setModel(cwKeyModel);

    QStringListModel* profilesModes = new QStringListModel();
    ui->stationProfilesListView->setModel(profilesModes);

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

    ui->cwModelSelect->addItem(tr("Dummy"), CWKey::DUMMY_KEYER);
    ui->cwModelSelect->addItem(tr("K3NG with CLI"), CWKey::K3NG_WITH_CLI_KEYER);
    ui->cwModelSelect->setCurrentIndex(ui->cwModelSelect->findData(DEFAULT_CWKEY_MODEL));

    ui->cwKeyModeSelect->addItem(tr("Single Paddle"), CWKey::SINGLE_PADDLE);
    ui->cwKeyModeSelect->addItem(tr("IAMBIC A"), CWKey::IAMBIC_A);
    ui->cwKeyModeSelect->addItem(tr("IAMBIC B"), CWKey::IAMBIC_B);
    ui->cwKeyModeSelect->addItem(tr("Ultimate"), CWKey::ULTIMATE);
    ui->cwKeyModeSelect->setCurrentIndex(ui->cwKeyModeSelect->findData(CWKey::IAMBIC_B));

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

    profile.getFreqInfo = ui->rigGetFreqCheckBox->isChecked();
    profile.getModeInfo = ui->rigGetModeCheckBox->isChecked();
    profile.getVFOInfo = ui->rigGetVFOCheckBox->isChecked();
    profile.getPWRInfo = ui->rigGetPWRCheckBox->isChecked();
    profile.getRITInfo = ui->rigGetRITCheckBox->isChecked();
    profile.getXITInfo = ui->rigGetXITCheckBox->isChecked();
    profile.getPTTInfo = ui->rigGetPTTStateCheckBox->isChecked();
    profile.QSYWiping = ui->rigQSYWipingCheckBox->isChecked();

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

    fixRigCap(rig_get_caps(profile.model));

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
    QStringListModel* model = (QStringListModel*)ui->rotProfilesListView->model();
    QStringList profiles = model->stringList();

    profiles.clear();

    profiles << rotProfManager->profileNameList();

    model->setStringList(profiles);
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
    QStringListModel* model = (QStringListModel*)ui->antProfilesListView->model();
    QStringList profiles = model->stringList();

    profiles.clear();

    profiles << antProfManager->profileNameList();
    model->setStringList(profiles);
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

    if ( ui->cwAddProfileButton->text() == tr("Modify"))
    {
        ui->cwAddProfileButton->setText(tr("Add"));
    }

    CWKeyProfile profile;

    profile.profileName = ui->cwProfileNameEdit->text();
    profile.model = ui->cwModelSelect->currentData().toInt();
    profile.defaultSpeed = ui->cwDefaulSpeed->value();
    profile.keyMode = ui->cwKeyModeSelect->currentData().toInt();
    profile.portPath = ui->cwPortEdit->text();
    profile.baudrate = ui->cwBaudSelect->currentText().toInt();

    cwKeyProfManager->addProfile(profile.profileName, profile);

    refreshCWKeyProfilesView();

    clearCWKeyProfileForm();
}

void SettingsDialog::delCWKeyProfile()
{
    FCT_IDENTIFICATION;

    foreach (QModelIndex index, ui->cwProfilesListView->selectionModel()->selectedRows())
    {
        cwKeyProfManager->removeProfile(ui->cwProfilesListView->model()->data(index).toString());
        ui->cwProfilesListView->model()->removeRow(index.row());
    }
    ui->cwProfilesListView->clearSelection();

    clearCWKeyProfileForm();

}

void SettingsDialog::refreshCWKeyProfilesView()
{
    FCT_IDENTIFICATION;

    QStringListModel* model = (QStringListModel*)ui->cwProfilesListView->model();
    QStringList profiles = model->stringList();

    profiles.clear();


    profiles << cwKeyProfManager->profileNameList();

    model->setStringList(profiles);
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

void SettingsDialog::refreshRigProfilesView()
{
    FCT_IDENTIFICATION;
    QStringListModel* model = (QStringListModel*)ui->rigProfilesListView->model();
    QStringList profiles = model->stringList();

    profiles.clear();

    profiles << rigProfManager->profileNameList();

    model->setStringList(profiles);
}

void SettingsDialog::refreshStationProfilesView()
{
    FCT_IDENTIFICATION;
    QStringListModel* model = (QStringListModel*)ui->stationProfilesListView->model();
    QStringList profiles = model->stringList();

    profiles.clear();

    profiles << stationProfManager->profileNameList();

    model->setStringList(profiles);
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

        /* disable what is unimplemented */
        fixRigCap(caps);
    }
    else
    {
        ui->rigStackedWidget->setCurrentIndex(0);
    }

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

    QPalette p;

    if ( ! ui->stationCallsignEdit->hasAcceptableInput() )
    {
        p.setColor(QPalette::Text,Qt::red);
    }
    else
    {
        p.setColor(QPalette::Text,qApp->palette().text().color());
    }
    ui->stationCallsignEdit->setPalette(p);

}

void SettingsDialog::adjustLocatorTextColor()
{
    FCT_IDENTIFICATION;

    QPalette p;

    if ( ! ui->stationLocatorEdit->hasAcceptableInput() )
    {
        p.setColor(QPalette::Text,Qt::red);
    }
    else
    {
        p.setColor(QPalette::Text,qApp->palette().text().color());
    }

    ui->stationLocatorEdit->setPalette(p);

}

void SettingsDialog::adjustVUCCLocatorTextColor()
{
    FCT_IDENTIFICATION;

    QPalette p;

    if ( ! ui->stationVUCCEdit->hasAcceptableInput() )
    {
        p.setColor(QPalette::Text,Qt::red);
    }
    else
    {
        p.setColor(QPalette::Text,qApp->palette().text().color());
    }

    ui->stationVUCCEdit->setPalette(p);
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
    ((QStringListModel*)ui->stationProfilesListView->model())->setStringList(profiles);

    QStringList rigs = rigProfManager->profileNameList();
    ((QStringListModel*)ui->rigProfilesListView->model())->setStringList(rigs);

    QStringList rots = rotProfManager->profileNameList();
    ((QStringListModel*)ui->rotProfilesListView->model())->setStringList(rots);

    QStringList ants = antProfManager->profileNameList();
    ((QStringListModel*)ui->antProfilesListView->model())->setStringList(ants);

    QStringList cwKeys = cwKeyProfManager->profileNameList();
    ((QStringListModel*)ui->cwProfilesListView->model())->setStringList(cwKeys);

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
void SettingsDialog::fixRigCap(const struct rig_caps *caps)
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

        if ( ! caps->get_level || ! caps->power2mW )
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
    }
}

SettingsDialog::~SettingsDialog() {
    FCT_IDENTIFICATION;

    modeTableModel->deleteLater();
    bandTableModel->deleteLater();
    sotaCompleter->deleteLater();
    iotaCompleter->deleteLater();
    delete ui;
}
