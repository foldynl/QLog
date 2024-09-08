#include <QSettings>
#include <QStringListModel>
#include <QSqlTableModel>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>

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
#include "../core/HRDLog.h"
#include "../ui/StyleItemDelegate.h"
#include "core/debug.h"
#include "core/CredentialStore.h"
#include "data/StationProfile.h"
#include "data/RigProfile.h"
#include "data/AntProfile.h"
#include "data/Data.h"
#include "core/Gridsquare.h"
#include "core/Wsjtx.h"
#include "core/QSLStorage.h"
#include "core/NetworkNotification.h"
#include "rig/Rig.h"
#include "rig/RigCaps.h"
#include "rotator/Rotator.h"
#include "rotator/RotCaps.h"
#include "core/LogParam.h"
#include "core/Callsign.h"
#include "cwkey/CWKeyer.h"
#include "core/MembershipQE.h"
#include "models/SqlListModel.h"
#include "core/GenericCallbook.h"
#include "core/KSTChat.h"
#include "core/HostsPortString.h"
#include "models/ShortcutEditorModel.h"
#include "ui/StyleItemDelegate.h"

#define STACKED_WIDGET_SERIAL_SETTING  0
#define STACKED_WIDGET_NETWORK_SETTING 1
#define STACKED_WIDGET_SPECIAL_OMNIRIG_SETTING 2

#define RIGPORT_SERIAL_INDEX 0
#define RIGPORT_NETWORK_INDEX 1
#define RIGPORT_SPECIAL_OMNIRIG_INDEX 2

#define ROTPORT_SERIAL_INDEX 0
#define ROTPORT_NETWORK_INDEX 1

#define EMPTY_CWKEY_PROFILE " "

MODULE_IDENTIFICATION("qlog.ui.settingdialog");

#define RIG_NET_DEFAULT_PORT 4532
#define ROT_NET_DEFAULT_PORT 4533
#define ROT_NET_DEFAULT_PSTROT 12000
#define CW_NET_CWDAEMON_PORT 6789
#define CW_NET_FLDIGI_PORT 7362
#define CW_DEFAULT_KEY_SPEED 20
#define CW_KEY_SPEED_DISABLED 0
#define PTT_TYPE_NONE_INDEX 0
#define PTT_TYPE_CAT_INDEX 1

SettingsDialog::SettingsDialog(MainWindow *parent) :
    QDialog(parent),
    stationProfManager(StationProfilesManager::instance()),
    rigProfManager(RigProfilesManager::instance()),
    rotProfManager(RotProfilesManager::instance()),
    antProfManager(AntProfilesManager::instance()),
    cwKeyProfManager(CWKeyProfilesManager::instance()),
    cwShortcutProfManager(CWShortcutProfilesManager::instance()),
    rotUsrButtonsProfManager(RotUsrButtonsProfilesManager::instance()),
    ui(new Ui::SettingsDialog),
    sotaFallback(false),
    potaFallback(false),
    wwffFallback(false)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->rigPortTypeCombo->addItem(tr("Serial"));
    ui->rigPortTypeCombo->addItem(tr("Network"));
    ui->rigPortTypeCombo->addItem(tr("Special - Omnirig"));

#ifdef QLOG_FLATPAK
    ui->lotwTextMessage->setVisible(true);
    ui->tqslPathEdit->setVisible(false);
    ui->tqslPathButton->setVisible(false);
    ui->lotwtqslPathLabel->setVisible(false);
#else
    ui->lotwTextMessage->setVisible(false);
#endif

    RotTypeModel* rotTypeModel = new RotTypeModel(this);
    ui->rotModelSelect->setModel(rotTypeModel);

    QStringListModel* rotModel = new QStringListModel();
    ui->rotProfilesListView->setModel(rotModel);

    QStringListModel* rotUsrButtonModel = new QStringListModel();
    ui->rotUsrButtonListView->setModel(rotUsrButtonModel);

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

    for ( const QPair<int, QString> &driver : Rig::instance()->getDriverList() )
    {
        ui->rigInterfaceCombo->addItem(driver.second, driver.first);
    }

    for ( const QPair<int, QString> &driver : Rotator::instance()->getDriverList() )
    {
        ui->rotInterfaceCombo->addItem(driver.second, driver.first);
    }

    QStringListModel* rigModel = new QStringListModel();
    ui->rigProfilesListView->setModel(rigModel);

    /* Country Combo */
    SqlListModel* countryModel = new SqlListModel("SELECT id, translate_to_locale(name), name  "
                                                  "FROM dxcc_entities "
                                                  "ORDER BY 2 COLLATE LOCALEAWARE ASC;", " ", this);
    while ( countryModel->canFetchMore() )
        countryModel->fetchMore();

    ui->stationCountryCombo->setModel(countryModel);
    ui->stationCountryCombo->setModelColumn(1);

    ShortcutEditorModel *shortcutModel = new ShortcutEditorModel( parent->getUserDefinedShortcutActionList(),
                                                                  parent->getBuiltInStaticShortcutList());
    ui->shortcutsTableView->setModel(shortcutModel);
    ui->shortcutsTableView->horizontalHeader()->setSectionResizeMode(ShortcutEditorModel::COLUMN_DESCRIPTION, QHeaderView::Stretch);
    ui->shortcutsTableView->setItemDelegateForColumn(ShortcutEditorModel::COLUMN_SHORTCUT, new ShortcutDelegate(this));
    connect(shortcutModel, &ShortcutEditorModel::conflictDetected, this, [this](const QString& text)
    {
        ui->shortcutInfoLabel->setText("<b>" + text + "</b>");
    });

    connect(shortcutModel, &ShortcutEditorModel::dataChanged, this, [this]()
    {
        ui->shortcutInfoLabel->clear();
    });

    ui->stationCQZEdit->setValidator(new QIntValidator(Data::getCQZMin(), Data::getCQZMax(), ui->stationCQZEdit));
    ui->stationITUEdit->setValidator(new QIntValidator(Data::getITUZMin(), Data::getITUZMax(), ui->stationITUEdit));

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
    ui->modeTableView->setItemDelegateForColumn(4,new ComboFormatDelegate(QStringList()<<"CW"<< "PHONE" << "DIGITAL"));
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
    bandTableModel->setHeaderData(6, Qt::Horizontal, tr("SAT Mode"));
    ui->bandTableView->setModel(bandTableModel);

    ui->bandTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->bandTableView->horizontalHeader()->moveSection(6, 4);
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
    ui->rigPTTPortEdit->setValidator(new QRegularExpressionValidator(comPortRE, this));

    /* https://stackoverflow.com/questions/13145397/regex-for-multicast-ip-address */
    static QRegularExpression multicastAddress("^2(?:2[4-9]|3\\d)(?:\\.(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d?|0)){3}$");

    ui->wsjtMulticastAddressEdit->setValidator(new QRegularExpressionValidator(multicastAddress, this));

    ui->notifQSOEdit->setValidator(new QRegularExpressionValidator(HostsPortString::hostsPortRegEx(), this));
    ui->notifDXSpotsEdit->setValidator(new QRegularExpressionValidator(HostsPortString::hostsPortRegEx(), this));
    ui->notifWSJTXCQSpotsEdit->setValidator(new QRegularExpressionValidator(HostsPortString::hostsPortRegEx(), this));
    ui->notifSpotAlertEdit->setValidator(new QRegularExpressionValidator(HostsPortString::hostsPortRegEx(), this));

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

    wwffCompleter = new QCompleter(Data::instance()->wwffIDList(), this);
    wwffCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    wwffCompleter->setFilterMode(Qt::MatchStartsWith);
    wwffCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->stationWWFFEdit->setCompleter(nullptr);

    potaCompleter = new MultiselectCompleter(Data::instance()->potaIDList(), this);
    potaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    potaCompleter->setFilterMode(Qt::MatchStartsWith);
    potaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->stationPOTAEdit->setCompleter(nullptr);

    sigCompleter = new QCompleter(Data::instance()->sigIDList(), this);
    sigCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    sigCompleter->setFilterMode(Qt::MatchStartsWith);
    sigCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->stationSIGEdit->setCompleter(sigCompleter);

    ui->primaryCallbookCombo->addItem(tr("Disabled"), QVariant(GenericCallbook::CALLBOOK_NAME));
    ui->primaryCallbookCombo->addItem(tr("HamQTH"),   QVariant(HamQTH::CALLBOOK_NAME));
    ui->primaryCallbookCombo->addItem(tr("QRZ.com"),  QVariant(QRZ::CALLBOOK_NAME));

    ui->rigFlowControlSelect->addItem(tr("None"), SerialPort::SERIAL_FLOWCONTROL_NONE);
    ui->rigFlowControlSelect->addItem(tr("Hardware"), SerialPort::SERIAL_FLOWCONTROL_HARDWARE);
    ui->rigFlowControlSelect->addItem(tr("Software"), SerialPort::SERIAL_FLOWCONTROL_SOFTWARE);

    ui->rigParitySelect->addItem(tr("No"), SerialPort::SERIAL_PARITY_NO);
    ui->rigParitySelect->addItem(tr("Even"), SerialPort::SERIAL_PARITY_EVEN);
    ui->rigParitySelect->addItem(tr("Odd"), SerialPort::SERIAL_PARITY_ODD);
    ui->rigParitySelect->addItem(tr("Mark"), SerialPort::SERIAL_PARITY_MARK);
    ui->rigParitySelect->addItem(tr("Space"), SerialPort::SERIAL_PARITY_SPACE);

    ui->rotFlowControlSelect->addItem(tr("None"), SerialPort::SERIAL_FLOWCONTROL_NONE);
    ui->rotFlowControlSelect->addItem(tr("Hardware"), SerialPort::SERIAL_FLOWCONTROL_HARDWARE);
    ui->rotFlowControlSelect->addItem(tr("Software"), SerialPort::SERIAL_FLOWCONTROL_SOFTWARE);

    ui->rotParitySelect->addItem(tr("No"), SerialPort::SERIAL_PARITY_NO);
    ui->rotParitySelect->addItem(tr("Even"), SerialPort::SERIAL_PARITY_EVEN);
    ui->rotParitySelect->addItem(tr("Odd"), SerialPort::SERIAL_PARITY_ODD);
    ui->rotParitySelect->addItem(tr("Mark"), SerialPort::SERIAL_PARITY_MARK);
    ui->rotParitySelect->addItem(tr("Space"), SerialPort::SERIAL_PARITY_SPACE);
;
    ui->cwModelSelect->addItem(tr("Dummy"), CWKey::DUMMY_KEYER);
    ui->cwModelSelect->addItem(tr("Morse Over CAT"), CWKey::MORSEOVERCAT);
    ui->cwModelSelect->addItem(tr("WinKey v2"), CWKey::WINKEY2_KEYER);
    ui->cwModelSelect->addItem(tr("CWDaemon"), CWKey::CWDAEMON_KEYER);
    ui->cwModelSelect->addItem(tr("FLDigi"), CWKey::FLDIGI_KEYER);
    ui->cwModelSelect->setCurrentIndex(ui->cwModelSelect->findData(DEFAULT_CWKEY_MODEL));

    ui->cwKeyModeSelect->addItem(tr("Single Paddle"), CWKey::SINGLE_PADDLE);
    ui->cwKeyModeSelect->addItem(tr("IAMBIC A"), CWKey::IAMBIC_A);
    ui->cwKeyModeSelect->addItem(tr("IAMBIC B"), CWKey::IAMBIC_B);
    ui->cwKeyModeSelect->addItem(tr("Ultimate"), CWKey::ULTIMATE);
    ui->cwKeyModeSelect->setCurrentIndex(ui->cwKeyModeSelect->findData(CWKey::IAMBIC_B));

    /* disable WSJTX Multicast by default */
    joinMulticastChanged(false);

    generateMembershipCheckboxes();

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

    if ( ui->rotUsrButtonAddProfileButton->text() == tr("Modify") )
    {
        ui->tabWidget->setCurrentIndex(1);
        ui->equipmentTabWidget->setCurrentIndex(3);
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),pleaseModifyTXT);
        return;
    }

    if ( ui->wsjtMulticastCheckbox->isChecked() )
    {
        if ( ! ui->wsjtMulticastAddressEdit->hasAcceptableInput() )
        {
            ui->tabWidget->setCurrentIndex(7);
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 QMessageBox::tr("WSJTX Multicast is enabled but the Address is not a multicast address."));
            return;
        }
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

    if ( ui->rigStackedWidget->currentIndex() == STACKED_WIDGET_SERIAL_SETTING )
    {
        if ( ui->rigPTTTypeCombo->currentIndex() != PTT_TYPE_NONE_INDEX
             && ui->rigPTTTypeCombo->currentIndex() != PTT_TYPE_CAT_INDEX
             && ui->rigPTTPortEdit->text().isEmpty() )
        {
            ui->rigPTTPortEdit->setPlaceholderText(tr("Must not be empty"));
            return;
        }

        if ( ! ui->rigPTTPortEdit->text().isEmpty()
             && ! ui->rigPTTPortEdit->hasAcceptableInput() )
        {
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 QMessageBox::tr("Rig PTT port must be a valid COM port.<br>For Windows use COMxx, for unix-like OS use a path to device"));
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

    profile.driver = ui->rigInterfaceCombo->currentData().toInt();

    profile.model = ui->rigModelSelect->currentData().toInt();

    profile.txFreqStart = ui->rigTXFreqMinSpinBox->value();
    profile.txFreqEnd = ui->rigTXFreqMaxSpinBox->value();

    if ( ui->rigStackedWidget->currentIndex() == STACKED_WIDGET_NETWORK_SETTING )
    {
        profile.hostname = ui->rigHostNameEdit->text();
        profile.netport = ui->rigNetPortSpin->value();
    }

    if ( ui->rigStackedWidget->currentIndex() == STACKED_WIDGET_SERIAL_SETTING )
    {
        profile.portPath = ui->rigPortEdit->text();
        profile.baudrate =  ui->rigBaudSelect->currentText().toInt();
        profile.databits = ui->rigDataBitsSelect->currentText().toInt();
        profile.stopbits = ui->rigStopBitsSelect->currentText().toFloat();
        profile.flowcontrol = ui->rigFlowControlSelect->currentData().toString();
        profile.parity = ui->rigParitySelect->currentData().toString();
        profile.pttType = ui->rigPTTTypeCombo->currentData().toString();
        profile.pttPortPath = ui->rigPTTPortEdit->text();
    }

    if ( ui->rigPollIntervalSpinBox->isEnabled() )
    {
        profile.pollInterval = ui->rigPollIntervalSpinBox->value();
    }

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
    profile.keySpeedSync = ui->rigKeySpeedSyncCheckBox->isChecked();
    profile.dxSpot2Rig = ui->rigDXSpots2RigCheckBox->isChecked();

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

    const RigProfile &profile = rigProfManager->getProfile(ui->rigProfilesListView->model()->data(i).toString());

    ui->rigProfileNameEdit->setText(profile.profileName);

    ui->rigInterfaceCombo->setCurrentIndex(ui->rigInterfaceCombo->findData(profile.driver));
    ui->rigModelSelect->setCurrentIndex(ui->rigModelSelect->findData(profile.model));

    int portIndex;
    switch ( profile.getPortType() )
    {
    case RigProfile::SERIAL_ATTACHED:
        portIndex = RIGPORT_SERIAL_INDEX;
        break;
    case RigProfile::NETWORK_ATTACHED:
        portIndex = RIGPORT_NETWORK_INDEX;
        break;
    case RigProfile::SPECIAL_OMNIRIG_ATTACHED:
        portIndex = RIGPORT_SPECIAL_OMNIRIG_INDEX;
        break;
    default:
        qWarning() << "cannot set correct Rig Port - unsupported" << profile.getPortType();
        portIndex = RIGPORT_SERIAL_INDEX;
    }

    ui->rigPortTypeCombo->setCurrentIndex(portIndex);
    ui->rigPortEdit->setText(profile.portPath);
    ui->rigHostNameEdit->setText(profile.hostname);
    ui->rigNetPortSpin->setValue(profile.netport);
    ui->rigBaudSelect->setCurrentText(QString::number(profile.baudrate));
    ui->rigDataBitsSelect->setCurrentText(QString::number(profile.databits));
    ui->rigStopBitsSelect->setCurrentText(QString::number(profile.stopbits));

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
    ui->rigKeySpeedSyncCheckBox->setChecked(profile.keySpeedSync);
    ui->rigDXSpots2RigCheckBox->setChecked(profile.dxSpot2Rig);

    int flowControlIndex = ui->rigFlowControlSelect->findData(profile.flowcontrol.toLower());
    ui->rigFlowControlSelect->setCurrentIndex((flowControlIndex < 0) ? 0 : flowControlIndex);

    int parityIndex = ui->rigParitySelect->findData(profile.parity.toLower());
    ui->rigParitySelect->setCurrentIndex((parityIndex < 0) ? 0 : parityIndex);

    const RigCaps &caps = Rig::instance()->getRigCaps(static_cast<Rig::DriverID>(profile.driver), profile.model);

    int pttIndex = ui->rigPTTTypeCombo->findData(profile.pttType);
    ui->rigPTTTypeCombo->setCurrentIndex(( pttIndex < 0 ) ? PTT_TYPE_CAT_INDEX : pttIndex);
    ui->rigPTTPortEdit->setText(profile.pttPortPath);

    setUIBasedOnRigCaps(caps);

    ui->rigAddProfileButton->setText(tr("Modify"));
}

void SettingsDialog::clearRigProfileForm()
{
    FCT_IDENTIFICATION;

    ui->rigProfileNameEdit->setPlaceholderText(QString());
    ui->rigPortEdit->setPlaceholderText(QString());
    ui->rigHostNameEdit->setPlaceholderText(QString());
    ui->rigPTTPortEdit->setPlaceholderText(QString());

    ui->rigProfileNameEdit->clear();
    ui->rigTXFreqMinSpinBox->setValue(0.0);
    ui->rigTXFreqMaxSpinBox->setValue(0.0);
    ui->rigPWRDefaultSpinBox->setValue(100.0);
    ui->rigAssignedCWKeyCombo->setCurrentIndex(0);
    ui->rigPollIntervalSpinBox->setValue(500.0);
    ui->rigPortEdit->clear();
    ui->rigHostNameEdit->clear();
    ui->rigNetPortSpin->setValue(RIG_NET_DEFAULT_PORT);
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
    ui->rigKeySpeedSyncCheckBox->setChecked(false);
    ui->rigDXSpots2RigCheckBox->setChecked(false);
    ui->rigAddProfileButton->setText(tr("Add"));
    ui->rigPTTPortEdit->clear();
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

void SettingsDialog::rigPortTypeChanged(int index)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << index;

    switch (index)
    {
    // Serial
    case RIGPORT_SERIAL_INDEX:
    {
        const RigCaps &caps = Rig::instance()->getRigCaps(static_cast<Rig::DriverID>(ui->rigInterfaceCombo->currentData().toInt()),
                                                          ui->rigModelSelect->currentData().toInt());
        ui->rigStackedWidget->setCurrentIndex(STACKED_WIDGET_SERIAL_SETTING);
        ui->rigDataBitsSelect->setCurrentText(QString::number(caps.serialDataBits));
        ui->rigStopBitsSelect->setCurrentText(QString::number(caps.serialStopBits));
        ui->rigHostNameEdit->clear();
    }
        break;

    // Network
    case RIGPORT_NETWORK_INDEX:
        ui->rigStackedWidget->setCurrentIndex(STACKED_WIDGET_NETWORK_SETTING);
        ui->rigPortEdit->clear();
        ui->rigNetPortSpin->setValue(RIG_NET_DEFAULT_PORT);
        break;

    // Omnirig Special
    case RIGPORT_SPECIAL_OMNIRIG_INDEX:
        ui->rigStackedWidget->setCurrentIndex(STACKED_WIDGET_SPECIAL_OMNIRIG_SETTING);
        ui->rigHostNameEdit->clear();
        ui->rigPortEdit->clear();
        break;
    default:
        qWarning() << "Unsupported Rig Port" << index;
    }
}

void SettingsDialog::rigInterfaceChanged(int)
{
    FCT_IDENTIFICATION;

    RigTypeModel* rigTypeModel = qobject_cast<RigTypeModel*> (ui->rigModelSelect->model());

    if ( !rigTypeModel )
        return;

    ui->rigPortTypeCombo->removeItem(STACKED_WIDGET_SPECIAL_OMNIRIG_SETTING);

    Rig::DriverID driverID = static_cast<Rig::DriverID>(ui->rigInterfaceCombo->currentData().toInt());

    if ( driverID == Rig::OMNIRIG_DRIVER
        || driverID == Rig::OMNIRIGV2_DRIVER )
    {
        ui->rigPortTypeCombo->insertItem(STACKED_WIDGET_SPECIAL_OMNIRIG_SETTING, tr("Special - Omnirig"));
    }

    rigTypeModel->select(driverID);
    ui->rigModelSelect->setCurrentIndex(( driverID == Rig::HAMLIB_DRIVER ) ? ui->rigModelSelect->findData(DEFAULT_HAMLIB_RIG_MODEL)
                                                                           : 0 );
    ui->rigPTTTypeCombo->clear();

    const QList<QPair<QString, QString>> &pttTypes = Rig::instance()->getPTTTypeList(static_cast<Rig::DriverID>(driverID));

    for ( const QPair<QString, QString> &type : pttTypes )
        ui->rigPTTTypeCombo->addItem(type.second, type.first);

    ui->rigPTTTypeCombo->setVisible(( driverID == Rig::HAMLIB_DRIVER ));
    ui->rigPTTTypeLabel->setVisible(( driverID == Rig::HAMLIB_DRIVER ));
    ui->rigPTTPortEdit->setVisible(( driverID == Rig::HAMLIB_DRIVER ));
    ui->rigPTTPortLabel->setVisible(( driverID == Rig::HAMLIB_DRIVER ));
    ui->rigPTTTypeCombo->setCurrentIndex(( driverID == Rig::HAMLIB_DRIVER ) ? PTT_TYPE_CAT_INDEX : 0);
}

void SettingsDialog::rigPTTTypeChanged(int index)
{
    FCT_IDENTIFICATION;

    ui->rigPTTPortEdit->setVisible((index != PTT_TYPE_CAT_INDEX && index != PTT_TYPE_NONE_INDEX));
    ui->rigPTTPortLabel->setVisible((index != PTT_TYPE_CAT_INDEX && index != PTT_TYPE_NONE_INDEX));
    if ( index == PTT_TYPE_CAT_INDEX || index == PTT_TYPE_NONE_INDEX )
        ui->rigPTTPortEdit->clear();
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

    profile.driver = ui->rotInterfaceCombo->currentData().toInt();

    profile.model = ui->rotModelSelect->currentData().toInt();

    if ( ui->rotStackedWidget->currentIndex() == STACKED_WIDGET_NETWORK_SETTING )
    {
        profile.hostname = ui->rotHostNameEdit->text();
        profile.netport = ui->rotNetPortSpin->value();
    }

    if ( ui->rotStackedWidget->currentIndex() == STACKED_WIDGET_SERIAL_SETTING )
    {
        profile.portPath = ui->rotPortEdit->text();
        profile.baudrate =  ui->rotBaudSelect->currentText().toInt();
        profile.databits = ui->rotDataBitsSelect->currentText().toInt();
        profile.stopbits = ui->rotStopBitsSelect->currentText().toFloat();
        profile.flowcontrol = ui->rotFlowControlSelect->currentData().toString();
        profile.parity = ui->rotParitySelect->currentData().toString();
    }

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

    const RotProfile &profile = rotProfManager->getProfile(ui->rotProfilesListView->model()->data(i).toString());

    ui->rotProfileNameEdit->setText(profile.profileName);

    ui->rotInterfaceCombo->setCurrentIndex(ui->rotInterfaceCombo->findData(profile.driver));

    ui->rotPortTypeCombo->setCurrentIndex( (profile.getPortType() == RotProfile::SERIAL_ATTACHED) ? ROTPORT_SERIAL_INDEX
                                                                                                  : ROTPORT_NETWORK_INDEX);
    ui->rotModelSelect->setCurrentIndex(ui->rotModelSelect->findData(profile.model));

    ui->rotPortEdit->setText(profile.portPath);
    ui->rotHostNameEdit->setText(profile.hostname);
    ui->rotNetPortSpin->setValue(profile.netport);
    ui->rotBaudSelect->setCurrentText(QString::number(profile.baudrate));
    ui->rotDataBitsSelect->setCurrentText(QString::number(profile.databits));
    ui->rotStopBitsSelect->setCurrentText(QString::number(profile.stopbits));

    int flowControlIndex = ui->rotFlowControlSelect->findData(profile.flowcontrol.toLower());
    ui->rotFlowControlSelect->setCurrentIndex((flowControlIndex < 0) ? 0 : flowControlIndex);

    int parityIndex = ui->rotParitySelect->findData(profile.parity.toLower());
    ui->rotParitySelect->setCurrentIndex((parityIndex < 0) ? 0 : parityIndex);

    ui->rotAddProfileButton->setText(tr("Modify"));
}

void SettingsDialog::clearRotProfileForm()
{
    FCT_IDENTIFICATION;

    ui->rotProfileNameEdit->setPlaceholderText(QString());
    ui->rotPortEdit->setPlaceholderText(QString());
    ui->rotHostNameEdit->setPlaceholderText(QString());

    ui->rotProfileNameEdit->clear();
    ui->rotPortEdit->clear();
    ui->rotHostNameEdit->clear();
    ui->rotBaudSelect->setCurrentIndex(0);
    ui->rotDataBitsSelect->setCurrentIndex(0);
    ui->rotStopBitsSelect->setCurrentIndex(0);
    ui->rotFlowControlSelect->setCurrentIndex(0);
    ui->rotParitySelect->setCurrentIndex(0);

    rotInterfaceChanged(ui->rotInterfaceCombo->currentIndex());
    ui->rotAddProfileButton->setText(tr("Add"));
}

void SettingsDialog::rotPortTypeChanged(int index)
{
    FCT_IDENTIFICATION;

    switch (index)
    {
    // Serial
    case ROTPORT_SERIAL_INDEX:
    {
        const RotCaps &caps = Rotator::instance()->getRotCaps(static_cast<Rotator::DriverID>(ui->rotInterfaceCombo->currentData().toInt()),
                                                              ui->rotModelSelect->currentData().toInt());
        ui->rotStackedWidget->setCurrentIndex(STACKED_WIDGET_SERIAL_SETTING);
        ui->rotDataBitsSelect->setCurrentText(QString::number(caps.serialDataBits));
        ui->rotStopBitsSelect->setCurrentText(QString::number(caps.serialStopBits));
        ui->rotHostNameEdit->clear();
    }
        break;

        // Network
    case RIGPORT_NETWORK_INDEX:
        ui->rotStackedWidget->setCurrentIndex(STACKED_WIDGET_NETWORK_SETTING);
        ui->rotPortEdit->clear();
        ui->rotNetPortSpin->setValue(ROT_NET_DEFAULT_PORT);
        break;
    default:
        qWarning() << "Unsupported Rot Port" << index;
    }

}
void SettingsDialog::rotInterfaceChanged(int)
{
    FCT_IDENTIFICATION;

    RotTypeModel* rotTypeModel = qobject_cast<RotTypeModel*> (ui->rotModelSelect->model());

    if ( !rotTypeModel )
        return;

    Rotator::DriverID driverID = static_cast<Rotator::DriverID>(ui->rotInterfaceCombo->currentData().toInt());

    rotTypeModel->select(driverID);

    if ( driverID == Rotator::HAMLIB_DRIVER )
    {
        ui->rotModelSelect->setCurrentIndex(ui->rotModelSelect->findData(DEFAULT_HAMLIB_RIG_MODEL));
        ui->rotNetPortSpin->setValue(ROT_NET_DEFAULT_PORT);
    }
    else
    {
        ui->rotModelSelect->setCurrentIndex(0);
        ui->rotNetPortSpin->setValue(ROT_NET_DEFAULT_PSTROT);
    }
}

void SettingsDialog::addRotUsrButtonsProfile()
{
    FCT_IDENTIFICATION;

    if ( ui->rotUsrButtonProfileNameEdit->text().isEmpty() )
    {
        ui->rotUsrButtonProfileNameEdit->setPlaceholderText(tr("Must not be empty"));
        return;
    }

    if ( ui->rotUsrButtonAddProfileButton->text() == tr("Modify"))
    {
        ui->rotUsrButtonAddProfileButton->setText(tr("Add"));
    }

    RotUsrButtonsProfile profile;

    profile.profileName = ui->rotUsrButtonProfileNameEdit->text();

    profile.shortDescs[0] = ui->rotUsrButton1Edit->text();
    profile.bearings[0] = ui->rotUsrButtonSpinBox1->value();

    profile.shortDescs[1] = ui->rotUsrButton2Edit->text();
    profile.bearings[1] = ui->rotUsrButtonSpinBox2->value();

    profile.shortDescs[2] = ui->rotUsrButton3Edit->text();
    profile.bearings[2] = ui->rotUsrButtonSpinBox3->value();

    profile.shortDescs[3] = ui->rotUsrButton4Edit->text();
    profile.bearings[3] = ui->rotUsrButtonSpinBox4->value();

    rotUsrButtonsProfManager->addProfile(profile.profileName, profile);

    refreshRotUsrButtonsProfilesView();

    clearRotUsrButtonsProfileForm();
}

void SettingsDialog::delRotUsrButtonsProfile()
{
    FCT_IDENTIFICATION;

    foreach (QModelIndex index, ui->rotUsrButtonListView->selectionModel()->selectedRows())
    {
        rotUsrButtonsProfManager->removeProfile(ui->rotUsrButtonListView->model()->data(index).toString());
        ui->rotUsrButtonListView->model()->removeRow(index.row());
    }
    ui->rotUsrButtonListView->clearSelection();

    clearRotUsrButtonsProfileForm();

}

void SettingsDialog::refreshRotUsrButtonsProfilesView()
{
    FCT_IDENTIFICATION;

    QStringList profiles;
    profiles << rotUsrButtonsProfManager->profileNameList();

    QStringListModel* model = static_cast<QStringListModel*>(ui->rotUsrButtonListView->model());
    if ( model ) model->setStringList(profiles);

}

void SettingsDialog::doubleClickRotUsrButtonsProfile(QModelIndex i)
{
    FCT_IDENTIFICATION;

    RotUsrButtonsProfile profile;

    profile = rotUsrButtonsProfManager->getProfile(ui->rotUsrButtonListView->model()->data(i).toString());

    ui->rotUsrButtonProfileNameEdit->setText(profile.profileName);

    ui->rotUsrButton1Edit->setText(profile.shortDescs[0]);
    ui->rotUsrButtonSpinBox1->setValue(profile.bearings[0]);

    ui->rotUsrButton2Edit->setText(profile.shortDescs[1]);
    ui->rotUsrButtonSpinBox2->setValue(profile.bearings[1]);

    ui->rotUsrButton3Edit->setText(profile.shortDescs[2]);
    ui->rotUsrButtonSpinBox3->setValue(profile.bearings[2]);

    ui->rotUsrButton4Edit->setText(profile.shortDescs[3]);
    ui->rotUsrButtonSpinBox4->setValue(profile.bearings[3]);

    ui->rotUsrButtonAddProfileButton->setText(tr("Modify"));
}

void SettingsDialog::clearRotUsrButtonsProfileForm()
{
    FCT_IDENTIFICATION;

    ui->rotUsrButtonProfileNameEdit->setPlaceholderText(QString());
    ui->rotUsrButtonProfileNameEdit->clear();

    ui->rotUsrButton1Edit->clear();
    ui->rotUsrButtonSpinBox1->setValue(-1);

    ui->rotUsrButton2Edit->clear();
    ui->rotUsrButtonSpinBox2->setValue(-1);

    ui->rotUsrButton3Edit->clear();
    ui->rotUsrButtonSpinBox3->setValue(-1);

    ui->rotUsrButton4Edit->clear();
    ui->rotUsrButtonSpinBox4->setValue(-1);

    ui->rotUsrButtonAddProfileButton->setText(tr("Add"));
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
    profile.azimuthBeamWidth = ui->antAzBeamWidthSpinBox->value();
    profile.azimuthOffset = ui->antAzOffsetSpinBox->value();

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
    ui->antAzBeamWidthSpinBox->setValue(profile.azimuthBeamWidth);
    ui->antAzOffsetSpinBox->setValue(profile.azimuthOffset);

    ui->antAddProfileButton->setText(tr("Modify"));

}

void SettingsDialog::clearAntProfileForm()
{
    FCT_IDENTIFICATION;

    ui->antProfileNameEdit->setPlaceholderText(QString());

    ui->antProfileNameEdit->clear();
    ui->antDescEdit->clear();
    ui->antAzBeamWidthSpinBox->setValue(0.0);
    ui->antAzOffsetSpinBox->setValue(0.0);

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
                                 QMessageBox::tr("CW Keyer port must be a valid COM port.<br>For Windows use COMxx, for unix-like OS use a path to device"));
            return;
        }
    }

    CWKeyProfile cwKeyNewProfile;

    cwKeyNewProfile.model = CWKey::intToTypeID(ui->cwModelSelect->currentData().toInt());
    cwKeyNewProfile.profileName = ui->cwProfileNameEdit->text();
    cwKeyNewProfile.defaultSpeed = ui->cwDefaulSpeed->value();
    cwKeyNewProfile.keyMode = CWKey::intToModeID(ui->cwKeyModeSelect->currentData().toInt());

    cwKeyNewProfile.hostname = ( ui->cwStackedWidget->currentIndex() == STACKED_WIDGET_SERIAL_SETTING ) ?
                QString() :
                ui->cwHostNameEdit->text();

    cwKeyNewProfile.netport = ( ui->cwStackedWidget->currentIndex() == STACKED_WIDGET_SERIAL_SETTING ) ?
                0 :
                ui->cwNetPortSpin->value();

    cwKeyNewProfile.portPath = ( ui->cwStackedWidget->currentIndex() == STACKED_WIDGET_SERIAL_SETTING ) ?
                ui->cwPortEdit->text() :
                QString();

    cwKeyNewProfile.baudrate = ( ui->cwStackedWidget->currentIndex() == STACKED_WIDGET_SERIAL_SETTING ) ?
                ui->cwBaudSelect->currentText().toInt() :
                0;

    QStringList noMorseCATSupportRigs;

    if ( cwKeyNewProfile.model == CWKey::MORSEOVERCAT )
    {
        // changing Key to Morse Over CAT model
        // needed to verify if all rigs where the key is assigned, supports Morse over CAT
        const QStringList &availableRigProfileNames = rigProfManager->profileNameList();

        for ( const QString &rigProfileName : availableRigProfileNames )
        {
            qCDebug(runtime) << "Checking Rig Profile" << rigProfileName;
            const RigProfile &testedRig = rigProfManager->getProfile(rigProfileName);

            if ( testedRig.assignedCWKey == cwKeyNewProfile.profileName )
            {
                if ( Rig::instance()->getRigCaps(static_cast<Rig::DriverID>(testedRig.driver),
                                                 testedRig.model).canSendMorse )
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
                             QMessageBox::tr("Cannot change the CW Keyer Model to <b>Morse over CAT</b><br>No Morse over CAT support for Rig(s) <b>%1</b>").arg(noMorseCATSupportRigs.join(", ")));
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
                                 QMessageBox::tr("Cannot delete the CW Keyer Profile<br>The CW Key Profile is used by Rig(s): <b>%1</b>").arg(dependentRigs.join(", ")));
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
    ui->cwHostNameEdit->setText(profile.hostname);
    ui->cwNetPortSpin->setValue(profile.netport);

    ui->cwAddProfileButton->setText(tr("Modify"));
}

void SettingsDialog::clearCWKeyProfileForm()
{
    FCT_IDENTIFICATION;

    ui->cwProfileNameEdit->setPlaceholderText(QString());
    ui->cwProfileNameEdit->clear();
    ui->cwModelSelect->setCurrentIndex(ui->cwModelSelect->findData(DEFAULT_CWKEY_MODEL));
    ui->cwKeyModeSelect->setCurrentIndex(ui->cwKeyModeSelect->findData(CWKey::IAMBIC_B));
    ui->cwDefaulSpeed->setValue(CW_DEFAULT_KEY_SPEED);
    ui->cwPortEdit->clear();
    ui->cwBaudSelect->setCurrentIndex(0);
    ui->cwHostNameEdit->clear();
    ui->cwNetPortSpin->setValue(CW_NET_CWDAEMON_PORT);

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
                             QMessageBox::tr("Gridsquare has an invalid format"));
        return;
    }

    if ( ! ui->stationVUCCEdit->text().isEmpty() )
    {
        if ( ! ui->stationVUCCEdit->hasAcceptableInput() )
        {
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 QMessageBox::tr("VUCC Grids have an invalid format (must be 2 or 4 Gridsquares separated by ',')"));
            return;
        }
    }

    if ( ui->stationCountryCombo->currentIndex() == 0 )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Country must not be empty"));
        return;
    }

    if ( ui->stationCQZEdit->text().isEmpty() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("CQZ must not be empty"));
        return;
    }

    if ( ui->stationITUEdit->text().isEmpty() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("ITU must not be empty"));
        return;
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
    profile.pota = ui->stationPOTAEdit->text().toUpper();
    profile.sig = ui->stationSIGEdit->text();
    profile.sigInfo = ui->stationSIGInfoEdit->text();
    profile.vucc = ui->stationVUCCEdit->text().toUpper();
    profile.wwff = ui->stationWWFFEdit->text().toUpper();
    profile.cqz = ui->stationCQZEdit->text().toInt();
    profile.ituz = ui->stationITUEdit->text().toInt();

    int row = ui->stationCountryCombo->currentIndex();
    const QModelIndex &idxDXCC = ui->stationCountryCombo->model()->index(row,0);
    const QVariant &dataDXCC = ui->stationCountryCombo->model()->data(idxDXCC);
    const QModelIndex &idxCountryEN = ui->stationCountryCombo->model()->index(row,2);
    QVariant dataCountryEN = ui->stationCountryCombo->model()->data(idxCountryEN);
    profile.dxcc = dataDXCC.toInt();
    profile.country = dataCountryEN.toString();

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
    ui->stationCallsignEdit->blockSignals(true);
    ui->stationCallsignEdit->setText(profile.callsign);
    setValidationResultColor(ui->stationCallsignEdit);
    ui->stationCallsignEdit->blockSignals(false);
    ui->stationLocatorEdit->setText(profile.locator);
    ui->stationOperatorEdit->setText(profile.operatorName);
    ui->stationQTHEdit->setText(profile.qthName);
    ui->stationIOTAEdit->setText(profile.iota);
    ui->stationSOTAEdit->blockSignals(true);
    ui->stationSOTAEdit->setText(profile.sota);
    ui->stationSOTAEdit->blockSignals(false);
    ui->stationPOTAEdit->blockSignals(true);
    ui->stationPOTAEdit->setText(profile.pota);
    ui->stationPOTAEdit->blockSignals(false);
    ui->stationSIGEdit->setText(profile.sig);
    ui->stationSIGInfoEdit->setText(profile.sigInfo);
    ui->stationVUCCEdit->setText(profile.vucc);
    ui->stationWWFFEdit->blockSignals(true);
    ui->stationWWFFEdit->setText(profile.wwff);
    ui->stationWWFFEdit->blockSignals(false);
    ui->stationCQZEdit->setText(QString::number(profile.cqz));
    ui->stationITUEdit->setText(QString::number(profile.ituz));
    const QModelIndexList &countryIndex = ui->stationCountryCombo->model()->match(ui->stationCountryCombo->model()->index(0,0),
                                                                           Qt::DisplayRole, profile.dxcc,
                                                                           1, Qt::MatchExactly);
    if ( countryIndex.size() >= 1 )
        ui->stationCountryCombo->setCurrentIndex(countryIndex.at(0).row());

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
    ui->stationPOTAEdit->clear();
    ui->stationIOTAEdit->clear();
    ui->stationSIGEdit->clear();
    ui->stationSIGInfoEdit->clear();
    ui->stationVUCCEdit->clear();
    ui->stationWWFFEdit->clear();
    ui->stationCQZEdit->clear();
    ui->stationITUEdit->clear();
    ui->stationCountryCombo->setCurrentIndex(0);

    ui->stationAddProfileButton->setText(tr("Add"));
}

/* This function is called when an user change Rig Combobox */
/* new rig entered */
void SettingsDialog::rigChanged(int index)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<index;

    refreshRigAssignedCWKeyCombo();

    int rigID = ui->rigModelSelect->currentData().toInt();
    Rig::DriverID driverID = static_cast<Rig::DriverID>(ui->rigInterfaceCombo->currentData().toInt());
    const RigCaps &caps = Rig::instance()->getRigCaps(driverID, rigID);

    if ( driverID == Rig::OMNIRIG_DRIVER
         || driverID == Rig::OMNIRIGV2_DRIVER)
    {
        ui->rigPortTypeCombo->setCurrentIndex(RIGPORT_SPECIAL_OMNIRIG_INDEX);
        ui->rigPortTypeCombo->setEnabled(false);
    }
    else
    {
        ui->rigPortTypeCombo->setEnabled(true);

        if ( caps.isNetworkOnly )
        {
            ui->rigPortTypeCombo->setCurrentIndex(RIGPORT_NETWORK_INDEX);
            ui->rigPortTypeCombo->setEnabled(false);
        }
        else
        {
            ui->rigPortTypeCombo->setCurrentIndex(RIGPORT_SERIAL_INDEX);
            ui->rigPortTypeCombo->setEnabled(true);
        }

        if ( ui->rigPortTypeCombo->currentIndex() == RIGPORT_SERIAL_INDEX )
        {
            ui->rigDataBitsSelect->setCurrentText(QString::number(caps.serialDataBits));
            ui->rigStopBitsSelect->setCurrentText(QString::number(caps.serialStopBits));
        }
    }

    ui->rigPollIntervalSpinBox->setEnabled(caps.needPolling);

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
    if ( ui->rigPortTypeCombo->currentIndex() == RIGPORT_SPECIAL_OMNIRIG_INDEX
         || driverID == Rig::TCI_DRIVER )
        ui->rigGetPTTStateCheckBox->setChecked(true);
    else
        ui->rigGetPTTStateCheckBox->setChecked(false);

    ui->rigQSYWipingCheckBox->setEnabled(true);
    ui->rigQSYWipingCheckBox->setChecked(true);

    ui->rigGetKeySpeedCheckBox->setEnabled(true);
    ui->rigGetKeySpeedCheckBox->setChecked(true);

    ui->rigKeySpeedSyncCheckBox->setEnabled(true);
    ui->rigKeySpeedSyncCheckBox->setChecked(false);

    ui->rigDXSpots2RigCheckBox->setEnabled(true);
    ui->rigDXSpots2RigCheckBox->setChecked(false);

    setUIBasedOnRigCaps(caps);
}

void SettingsDialog::rotChanged(int index)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << index;

    int rotID = ui->rotModelSelect->currentData().toInt();

    Rotator::DriverID driverID = static_cast<Rotator::DriverID>(ui->rotInterfaceCombo->currentData().toInt());
    const RotCaps &caps = Rotator::instance()->getRotCaps(driverID, rotID);

    if ( caps.isNetworkOnly )
    {
        ui->rotPortTypeCombo->setCurrentIndex(ROTPORT_NETWORK_INDEX);
        ui->rotPortTypeCombo->setEnabled(false);
    }
    else
    {
        ui->rotPortTypeCombo->setEnabled(true);
    }

    if ( ui->rotPortTypeCombo->currentIndex() == RIGPORT_SERIAL_INDEX )
    {
        ui->rotDataBitsSelect->setCurrentText(QString::number(caps.serialDataBits));
        ui->rotStopBitsSelect->setCurrentText(QString::number(caps.serialStopBits));
    }
}

void SettingsDialog::cwKeyChanged(int)
{
    FCT_IDENTIFICATION;

    const CWKey::CWKeyTypeID currentType = CWKey::intToTypeID(ui->cwModelSelect->currentData().toInt());

    ui->cwDefaulSpeed->setValue(CW_DEFAULT_KEY_SPEED);

    if ( CWKey::isNetworkKey(currentType) )
    {
        ui->cwStackedWidget->setCurrentIndex(STACKED_WIDGET_NETWORK_SETTING);
    }
    else
    {
        ui->cwStackedWidget->setCurrentIndex(STACKED_WIDGET_SERIAL_SETTING);
    }

    if ( currentType == CWKey::MORSEOVERCAT
         || currentType == CWKey::CWDAEMON_KEYER
         || currentType == CWKey::FLDIGI_KEYER )
    {
        ui->cwBaudSelect->setEnabled(false);
        ui->cwPortEdit->setEnabled(false);
        ui->cwPortEdit->clear();
        ui->cwKeyModeSelect->setEnabled(false);
        ui->cwDefaulSpeed->setEnabled(true);

        if ( currentType == CWKey::CWDAEMON_KEYER )
        {
            ui->cwNetPortSpin->setValue(CW_NET_CWDAEMON_PORT);
        }
        else if ( currentType == CWKey::FLDIGI_KEYER )
        {
            ui->cwDefaulSpeed->setEnabled(false);
            ui->cwNetPortSpin->setValue(CW_NET_FLDIGI_PORT);
            ui->cwDefaulSpeed->setValue(CW_KEY_SPEED_DISABLED);
        }

        return;
    }
    else
    {
        ui->cwBaudSelect->setEnabled(true);
        ui->cwPortEdit->setEnabled(true);
        ui->cwKeyModeSelect->setEnabled(true);
        ui->cwDefaulSpeed->setEnabled(true);
    }

    if ( currentType == CWKey::WINKEY2_KEYER )
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

    const QString &lastPath = ( ui->tqslPathEdit->text().isEmpty() ) ? QDir::rootPath()
                                                                     : ui->tqslPathEdit->text();

    QString filename = QFileDialog::getOpenFileName(this,
                                                    tr("Select File"),
                                                    lastPath,
#if defined(Q_OS_WIN)
                                                    "TQSL (*.exe)",
#elif defined(Q_OS_MACOS)
                                                    "TQSL (*.app)",
#else
                                                    "TQSL (tqsl)",
#endif
                                                    nullptr,
#if defined(Q_OS_LINUX)
                                                    // Do not use the Native Dialog under Linux because the dialog is case-sensitive.
                                                    // QT variant looks different but it is case-insensitive.
                                                    // More information:
                                                    // https://stackoverflow.com/questions/34858220/qt-how-to-set-a-case-insensitive-filter-on-qfiledialog
                                                    // https://bugreports.qt.io/browse/QTBUG-51712
                                                    QFileDialog::DontUseNativeDialog
#else
                                                    QFileDialog::Options()
#endif
                                                   );
    if ( !filename.isEmpty() )
    {
        ui->tqslPathEdit->setText(filename);
    }
}

void SettingsDialog::stationCallsignChanged()
{
    FCT_IDENTIFICATION;

    setValidationResultColor(ui->stationCallsignEdit);

    const QString &callsign = ui->stationCallsignEdit->text();
    const DxccEntity &dxccEntity = Data::instance()->lookupDxcc(callsign);

    if ( dxccEntity.dxcc )
    {
        ui->stationITUEdit->setText(QString::number(dxccEntity.ituz));
        ui->stationCQZEdit->setText(QString::number(dxccEntity.cqz));
        const QModelIndexList &countryIndex = ui->stationCountryCombo->model()->match(ui->stationCountryCombo->model()->index(0,0),
                                                                               Qt::DisplayRole, dxccEntity.dxcc,
                                                                               1, Qt::MatchExactly);
        if ( countryIndex.size() >= 1 )
            ui->stationCountryCombo->setCurrentIndex(countryIndex.at(0).row());
    }
    else
    {
        ui->stationCountryCombo->setCurrentIndex(0);
        ui->stationCQZEdit->clear();
        ui->stationITUEdit->clear();
    }
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

void SettingsDialog::sotaChanged(const QString &newSOTA)
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

    ui->stationQTHEdit->clear();
    ui->stationLocatorEdit->clear();
}

void SettingsDialog::sotaEditFinished()
{
    FCT_IDENTIFICATION;

    SOTAEntity sotaInfo = Data::instance()->lookupSOTA(ui->stationSOTAEdit->text());

    if ( sotaInfo.summitCode.toUpper() == ui->stationSOTAEdit->text().toUpper()
         && !sotaInfo.summitName.isEmpty() )
    {
        sotaFallback = false;
        ui->stationQTHEdit->setText(sotaInfo.summitName);
        Gridsquare SOTAGrid(sotaInfo.latitude, sotaInfo.longitude);
        if ( SOTAGrid.isValid() )
        {
            ui->stationLocatorEdit->setText(SOTAGrid.getGrid());
        }
    }
    else if ( !ui->stationPOTAEdit->text().isEmpty() && !potaFallback )
    {
        potaFallback = true;
        potaEditFinished();
    }
    else if ( !ui->stationWWFFEdit->text().isEmpty() && !wwffFallback )
    {
        wwffFallback = true;
        wwffEditFinished();
    }
}

void SettingsDialog::potaChanged(const QString &newPOTA)
{
    FCT_IDENTIFICATION;

    if ( newPOTA.length() >= 3 )
    {
        ui->stationPOTAEdit->setCompleter(potaCompleter);
    }
    else
    {
        ui->stationPOTAEdit->setCompleter(nullptr);
    }

    ui->stationQTHEdit->clear();
    ui->stationLocatorEdit->clear();
}

void SettingsDialog::potaEditFinished()
{
    FCT_IDENTIFICATION;

    QStringList potaList = ui->stationPOTAEdit->text().split("@");
    QString potaString;

    if ( potaList.size() > 0 )
    {
        potaString = potaList[0];
    }
    else
    {
        potaString = ui->stationPOTAEdit->text();
    }

    POTAEntity potaInfo = Data::instance()->lookupPOTA(potaString);

    if ( potaInfo.reference.toUpper() == potaString.toUpper()
         && !potaInfo.name.isEmpty() )
    {
        potaFallback = false;
        ui->stationQTHEdit->setText(potaInfo.name);
        Gridsquare POTAGrid(potaInfo.grid);
        if ( POTAGrid.isValid() )
        {
            ui->stationLocatorEdit->setText(POTAGrid.getGrid());
        }
    }
    else if ( !ui->stationSOTAEdit->text().isEmpty() && !sotaFallback )
    {
        sotaEditFinished();
    }
    else if ( !ui->stationWWFFEdit->text().isEmpty() && !wwffFallback )
    {
        wwffEditFinished();
    }
}

void SettingsDialog::wwffChanged(const QString &newWWFF)
{
    FCT_IDENTIFICATION;

    if ( newWWFF.length() >= 3 )
    {
        ui->stationWWFFEdit->setCompleter(wwffCompleter);
    }
    else
    {
        ui->stationWWFFEdit->setCompleter(nullptr);
    }

    if ( ui->stationSOTAEdit->text().isEmpty() )
    {
        //do not clear IOTA - IOTA info seems to be not reliable from WWFF and IOTA
        //can be added manually by operator
        ui->stationQTHEdit->clear();
    }
}

void SettingsDialog::wwffEditFinished()
{
    FCT_IDENTIFICATION;

    WWFFEntity wwffInfo = Data::instance()->lookupWWFF(ui->stationWWFFEdit->text());

    if ( wwffInfo.reference.toUpper() == ui->stationWWFFEdit->text().toUpper()
         && !wwffInfo.name.isEmpty()
         && ui->stationQTHEdit->text().isEmpty() )
    {
        wwffFallback = false;
        ui->stationQTHEdit->setText(wwffInfo.name);
        if ( ! wwffInfo.iota.isEmpty()
             && wwffInfo.iota != "-" )
        ui->stationIOTAEdit->setText(wwffInfo.iota.toUpper());
    }
    else if ( !ui->stationSOTAEdit->text().isEmpty() && !sotaFallback )
    {
        sotaEditFinished();
    }
    else if ( !ui->stationPOTAEdit->text().isEmpty() && !potaFallback )
    {
        potaEditFinished();
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

void SettingsDialog::assignedKeyChanged(int index)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << index;

    ui->rigKeySpeedSyncCheckBox->setEnabled(true);
    ui->rigKeySpeedSyncCheckBox->setChecked(false);

    const RigCaps &caps = Rig::instance()->getRigCaps(static_cast<Rig::DriverID>(ui->rigInterfaceCombo->currentData().toInt()),
                                                      ui->rigModelSelect->currentData().toInt());

    setUIBasedOnRigCaps(caps);
}

void SettingsDialog::testWebLookupURL()
{
    FCT_IDENTIFICATION;

    QDesktopServices::openUrl(GenericCallbook::getWebLookupURL(stationProfManager->getCurProfile1().callsign,
                                                               ui->webLookupURLEdit->text()));
}

void SettingsDialog::joinMulticastChanged(int state)
{
    FCT_IDENTIFICATION;

    ui->wsjtMulticastAddressLabel->setVisible(state);
    ui->wsjtMulticastAddressEdit->setVisible(state);
    ui->wsjtMulticastTTLLabel->setVisible(state);
    ui->wsjtMulticastTTLSpin->setVisible(state);
}

void SettingsDialog::adjustWSJTXMulticastAddrTextColor()
{
    FCT_IDENTIFICATION;

    setValidationResultColor(ui->wsjtMulticastAddressEdit);
}

void SettingsDialog::hrdlogSettingChanged()
{
    FCT_IDENTIFICATION;

    ui->hrdlogOnAirCheckBox->setEnabled(!ui->hrdlogCallsignEdit->text().isEmpty()
                                         && !ui->hrdlogUploadCodeEdit->text().isEmpty());
    if ( !ui->hrdlogOnAirCheckBox->isEnabled() )
    {
        ui->hrdlogOnAirCheckBox->setChecked(false);
    }
}

void SettingsDialog::clublogSettingChanged()
{
    FCT_IDENTIFICATION;

    ui->clublogUploadImmediatelyCheckbox->setEnabled(!ui->clublogEmailEdit->text().isEmpty()
                                                 && !ui->clublogPasswordEdit->text().isEmpty());
    if ( !ui->clublogUploadImmediatelyCheckbox->isEnabled() )
    {
        ui->clublogUploadImmediatelyCheckbox->setChecked(false);
    }
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

    QStringList rotButtons = rotUsrButtonsProfManager->profileNameList();
    (static_cast<QStringListModel*>(ui->rotUsrButtonListView->model()))->setStringList(rotButtons);

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

    ui->webLookupURLEdit->setText(GenericCallbook::getWebLookupURL("", QString(), false));

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
    ui->clublogPasswordEdit->setText(ClubLog::getPassword());
    ui->clublogUploadImmediatelyCheckbox->setChecked(ClubLog::isUploadImmediatelyEnabled());

    /********/
    /* eQSL */
    /********/
    ui->eqslUsernameEdit->setText(EQSL::getUsername());
    ui->eqslPasswordEdit->setText(EQSL::getPassword());

    /**********/
    /* HRDLog */
    /**********/
    ui->hrdlogCallsignEdit->setText(HRDLog::getRegisteredCallsign());
    ui->hrdlogUploadCodeEdit->setText(HRDLog::getUploadCode());
    ui->hrdlogOnAirCheckBox->setChecked(HRDLog::getOnAirEnabled());

    /***********/
    /* QRZ.COM */
    /***********/
    ui->qrzApiKeyEdit->setText(QRZ::getLogbookAPIKey());

    /***************/
    /* ON4KST Chat */
    /***************/
    ui->kstUsernameEdit->setText(KSTChat::getUsername());
    ui->kstPasswordEdit->setText(KSTChat::getPassword());

    /***********/
    /* MEMBERS */
    /***********/


    /***********/
    /* NETWORK */
    /***********/

    ui->wsjtPortSpin->setValue(Wsjtx::getConfigPort());
    ui->wsjtForwardEdit->setText(Wsjtx::getConfigForwardAddresses());
    ui->wsjtMulticastCheckbox->setChecked(Wsjtx::getConfigMulticastJoin());
    ui->wsjtMulticastAddressEdit->setText(Wsjtx::getConfigMulticastAddress());
    ui->wsjtMulticastTTLSpin->setValue(Wsjtx::getConfigMulticastTTL());

    ui->notifLogIDEdit->setText(LogParam::getParam("logid").toString());
    ui->notifQSOEdit->setText(NetworkNotification::getNotifQSOAdiAddrs());
    ui->notifDXSpotsEdit->setText(NetworkNotification::getNotifDXSpotAddrs());
    ui->notifWSJTXCQSpotsEdit->setText(NetworkNotification::getNotifWSJTXCQSpotAddrs());
    ui->notifSpotAlertEdit->setText(NetworkNotification::getNotifSpotAlertAddrs());
    ui->notifFreqEdit->setText(NetworkNotification::getNotifFreqAlertAddrs());

    /******************/
    /* END OF Reading */
    /******************/
}

void SettingsDialog::writeSettings() {
    FCT_IDENTIFICATION;

    QSettings settings;

    stationProfManager->save();
    rigProfManager->save();
    rotProfManager->save();
    rotUsrButtonsProfManager->save();
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

    settings.setValue(GenericCallbook::CONFIG_WEB_LOOKUP_URL,
                      ui->webLookupURLEdit->text());
    /********/
    /* LoTW */
    /********/

    Lotw::saveUsernamePassword(ui->lotwUsernameEdit->text(),
                               ui->lotwPasswordEdit->text());
    Lotw::saveTQSLPath(ui->tqslPathEdit->text());

    /***********/
    /* ClubLog */
    /***********/
    ClubLog::saveUsernamePassword(ui->clublogEmailEdit->text(),
                                  ui->clublogPasswordEdit->text());

    ClubLog::saveUploadImmediatelyConfig(ui->clublogUploadImmediatelyCheckbox->isChecked());

    /********/
    /* eQSL */
    /********/

    EQSL::saveUsernamePassword(ui->eqslUsernameEdit->text(),
                               ui->eqslPasswordEdit->text());

    /**********/
    /* HRDLog */
    /**********/
    HRDLog::saveUploadCode(ui->hrdlogCallsignEdit->text(),
                           ui->hrdlogUploadCodeEdit->text());
    HRDLog::saveOnAirEnabled(ui->hrdlogOnAirCheckBox->isChecked());

    /***********/
    /* QRZ.COM */
    /***********/
    QRZ::saveLogbookAPI(ui->qrzApiKeyEdit->text());

    /***************/
    /* ON4KST Chat */
    /***************/
    KSTChat::saveUsernamePassword(ui->kstUsernameEdit->text(),
                                  ui->kstPasswordEdit->text());

    /***********/
    /* MEMBERS */
    /***********/

    QStringList enabledLists;

    for ( QCheckBox* item: qAsConst(memberListCheckBoxes) )
    {
        if ( item->isChecked() )
        {
            enabledLists << item->text();
        }
    }
    MembershipQE::saveEnabledClubLists(enabledLists);

    /***********/
    /* NETWORK */
    /***********/
    Wsjtx::saveConfigPort(ui->wsjtPortSpin->value());
    Wsjtx::saveConfigForwardAddresses(ui->wsjtForwardEdit->text());
    Wsjtx::saveConfigMulticastJoin(ui->wsjtMulticastCheckbox->isChecked());
    Wsjtx::saveConfigMulticastAddress(ui->wsjtMulticastAddressEdit->text());
    Wsjtx::saveConfigMulticastTTL(ui->wsjtMulticastTTLSpin->value());

    NetworkNotification::saveNotifQSOAdiAddrs(ui->notifQSOEdit->text());
    NetworkNotification::saveNotifDXSpotAddrs(ui->notifDXSpotsEdit->text());
    NetworkNotification::saveNotifWSJTXCQSpotAddrs(ui->notifWSJTXCQSpotsEdit->text());
    NetworkNotification::saveNotifSpotAlertAddrs(ui->notifSpotAlertEdit->text());
    NetworkNotification::saveNotifFreqAlertAddrs(ui->notifFreqEdit->text());
}

/* this function is called when user modify rig progile
 * there may be situations where hamlib change the cap
 * for rig and it is necessary to change the settings of the rig.
 * This feature does it */
void SettingsDialog::setUIBasedOnRigCaps(const RigCaps &caps)
{
    FCT_IDENTIFICATION;

    if ( ! caps.canGetFreq )
    {
        ui->rigGetFreqCheckBox->setEnabled(false);
        ui->rigGetFreqCheckBox->setChecked(false);
    }

    if ( ! caps.canGetMode )
    {
        ui->rigGetModeCheckBox->setEnabled(false);
        ui->rigGetModeCheckBox->setChecked(false);
    }

    if ( ! caps.canGetVFO )
    {
        ui->rigGetVFOCheckBox->setEnabled(false);
        ui->rigGetVFOCheckBox->setChecked(false);
    }

    if ( ! caps.canGetPWR )
    {
        ui->rigGetPWRCheckBox->setEnabled(false);
        ui->rigGetPWRCheckBox->setChecked(false);
    }


    if ( ! caps.canGetRIT )
    {
        ui->rigGetRITCheckBox->setEnabled(false);
        ui->rigGetRITCheckBox->setChecked(false);
    }

    if ( ! caps.canGetXIT )
    {
        ui->rigGetXITCheckBox->setEnabled(false);
        ui->rigGetXITCheckBox->setChecked(false);
    }

    if ( ! caps.canGetPTT)
    {
        ui->rigGetPTTStateCheckBox->setEnabled(false);
        ui->rigGetPTTStateCheckBox->setChecked(false);
    }

    if ( ! ui->rigGetFreqCheckBox->isChecked() )
    {
        ui->rigQSYWipingCheckBox->setEnabled(false);
        ui->rigQSYWipingCheckBox->setChecked(false);
    }

    if ( ! caps.canGetKeySpeed )
    {
        ui->rigGetKeySpeedCheckBox->setEnabled(false);
        ui->rigGetKeySpeedCheckBox->setChecked(false);
        ui->rigKeySpeedSyncCheckBox->setEnabled(false);
        ui->rigKeySpeedSyncCheckBox->setChecked(false);
    }

    if ( ! caps.canProcessDXSpot )
    {
        ui->rigDXSpots2RigCheckBox->setEnabled(false);
        ui->rigDXSpots2RigCheckBox->setChecked(false);
    }

    if ( ui->rigAssignedCWKeyCombo->currentText() != EMPTY_CWKEY_PROFILE )
    {
        CWKeyProfile selectedKeyProfile;
        selectedKeyProfile = cwKeyProfManager->getProfile(ui->rigAssignedCWKeyCombo->currentText());
        if ( ! caps.canGetKeySpeed
             || (selectedKeyProfile.model == CWKey::MORSEOVERCAT) )
        {
            ui->rigKeySpeedSyncCheckBox->setEnabled(false);
            ui->rigKeySpeedSyncCheckBox->setChecked(false);
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

    const QString &cwKeyName = ui->rigAssignedCWKeyCombo->currentText();
    const QStringList &availableCWProfileNames = cwKeyProfManager->profileNameList();
    QStringList approvedCWProfiles;

    const RigCaps &caps = Rig::instance()->getRigCaps(static_cast<Rig::DriverID>(ui->rigInterfaceCombo->currentData().toInt()),
                                                      ui->rigModelSelect->currentData().toInt());

    approvedCWProfiles << EMPTY_CWKEY_PROFILE; // add empty profile (like NONE)

    if ( caps.canSendMorse )
    {
        approvedCWProfiles << availableCWProfileNames;
    }
    else
    {
        // remove unsupported Morse Over CAT Profile Names
        for ( const QString &cwProfileName : availableCWProfileNames )
        {
            const CWKeyProfile  &testedKey = cwKeyProfManager->getProfile(cwProfileName);
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

QString SettingsDialog::getMemberListComboValue(const QComboBox *combo)
{
    FCT_IDENTIFICATION;

    QSqlTableModel *model = static_cast<QSqlTableModel*>(combo->model());
    QModelIndex index = model->index(combo->currentIndex(), 0);
    return model->data(index).toString();
}

void SettingsDialog::generateMembershipCheckboxes()
{
    FCT_IDENTIFICATION;

    QSqlTableModel membershipDirectoryModel;
    membershipDirectoryModel.setTable("membership_directory");
    membershipDirectoryModel.setSort(0, Qt::AscendingOrder);
    membershipDirectoryModel.select();
    QStringList currentlyEnabledLists = MembershipQE::getEnabledClubLists();

    for ( int i = 0 ; i < membershipDirectoryModel.rowCount(); i++)
    {
        QCheckBox *columnCheckbox = new QCheckBox(this);

        QString shortDesc = membershipDirectoryModel.data(membershipDirectoryModel.index(i, membershipDirectoryModel.fieldIndex("short_desc"))).toString();
        QString longDesc = membershipDirectoryModel.data(membershipDirectoryModel.index(i, membershipDirectoryModel.fieldIndex("long_desc"))).toString();
        QString records = membershipDirectoryModel.data(membershipDirectoryModel.index(i, membershipDirectoryModel.fieldIndex("num_records"))).toString();

        columnCheckbox->setText(shortDesc);
        columnCheckbox->setToolTip(longDesc + QString(" (" + tr("members") + ": %1)").arg(records));
        columnCheckbox->setChecked(currentlyEnabledLists.contains(shortDesc));
        memberListCheckBoxes.append(columnCheckbox);
    }


    if ( memberListCheckBoxes.size() == 0 )
    {
        ui->clubListGrig->addWidget(new QLabel(tr("Required internet connection during application start")));
    }
    else
    {
        int elementIndex = 0;

        for ( QCheckBox* item: qAsConst(memberListCheckBoxes) )
        {
            ui->clubListGrig->addWidget(item, elementIndex / 6, elementIndex % 6);
            elementIndex++;
        }
    }
}

SettingsDialog::~SettingsDialog() {
    FCT_IDENTIFICATION;

    modeTableModel->deleteLater();
    bandTableModel->deleteLater();
    sotaCompleter->deleteLater();
    iotaCompleter->deleteLater();
    sigCompleter->deleteLater();
    delete ui;
}
