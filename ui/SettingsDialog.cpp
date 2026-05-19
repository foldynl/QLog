#include <QStringListModel>
#include <QSqlTableModel>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QStandardItemModel>
#include <QProgressDialog>

#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "models/RigTypeModel.h"
#include "models/RotTypeModel.h"
#include "service/GenericCallbook.h"
#include "service/qrzcom/QRZ.h"
#include "service/hamqth/HamQTH.h"
#include "service/lotw/Lotw.h"
#include "service/clublog/ClubLog.h"
#include "service/eqsl/Eqsl.h"
#include "service/hrdlog/HRDLog.h"
#include "ui/component/StyleItemDelegate.h"
#include "core/debug.h"
#include "data/StationProfile.h"
#include "data/RigProfile.h"
#include "data/AntProfile.h"
#include "data/Data.h"
#include "data/Gridsquare.h"
#include "core/WsjtxUDPReceiver.h"
#include "core/NetworkNotification.h"
#include "rig/Rig.h"
#include "rig/RigCaps.h"
#include "rotator/Rotator.h"
#include "rotator/RotCaps.h"
#include "core/LogParam.h"
#include "data/Callsign.h"
#include "core/MembershipQE.h"
#include "models/SqlListModel.h"
#include "service/kstchat/KSTChat.h"
#include "data/HostsPortString.h"
#include "models/ShortcutEditorModel.h"
#include "ui/component/StyleItemDelegate.h"
#include "data/SerialPort.h"
#include "service/cloudlog/Cloudlog.h"
#include "ui/RigctldAdvancedDialog.h"
#include "cwkey/drivers/CWWinKey.h"

MODULE_IDENTIFICATION("qlog.ui.settingsdialog");

void SettingsDialog::refreshProfileView(QAbstractItemView *view, const QStringList &names)
{
    QStringListModel *model = static_cast<QStringListModel*>(view->model());
    if ( model ) model->setStringList(names);
}

void SettingsDialog::disableCapCheckbox(QCheckBox *checkbox)
{
    checkbox->setChecked(false);
    checkbox->setEnabled(false);
}

void SettingsDialog::initProfileListView(QAbstractItemView *view)
{
    view->setModel(new QStringListModel(view));
}

void SettingsDialog::populateFlowControlCombo(QComboBox *combo)
{
    combo->addItem(tr("None"), SerialPort::SERIAL_FLOWCONTROL_NONE);
    combo->addItem(tr("Hardware"), SerialPort::SERIAL_FLOWCONTROL_HARDWARE);
    combo->addItem(tr("Software"), SerialPort::SERIAL_FLOWCONTROL_SOFTWARE);
}

void SettingsDialog::populateParityCombo(QComboBox *combo)
{
    combo->addItem(tr("No"), SerialPort::SERIAL_PARITY_NO);
    combo->addItem(tr("Even"), SerialPort::SERIAL_PARITY_EVEN);
    combo->addItem(tr("Odd"), SerialPort::SERIAL_PARITY_ODD);
    combo->addItem(tr("Mark"), SerialPort::SERIAL_PARITY_MARK);
    combo->addItem(tr("Space"), SerialPort::SERIAL_PARITY_SPACE);
}

void SettingsDialog::populateSignalCombo(QComboBox *combo)
{
    combo->addItem(tr("None"), SerialPort::SERIAL_SIGNAL_NONE);
    combo->addItem(tr("High"), SerialPort::SERIAL_SIGNAL_HIGH);
    combo->addItem(tr("Low"), SerialPort::SERIAL_SIGNAL_LOW);
}

void SettingsDialog::setComboByData(QComboBox *combo, const QVariant &data, int fallback)
{
    const int idx = combo->findData(data);
    combo->setCurrentIndex((idx < 0) ? fallback : idx);
}

void SettingsDialog::updateOffsetSpinBox(QCheckBox *checkbox, QDoubleSpinBox *spinBox)
{
    if ( checkbox->isChecked() )
    {
        spinBox->setValue(0.0);
        spinBox->setEnabled(false);
    }
    else
        spinBox->setEnabled(true);
}

void SettingsDialog::deleteSelectedProfiles(QAbstractItemView *view,
                                            const std::function<void(const QString &)> &removeProfile)
{
    const QModelIndexList selectedRows = view->selectionModel()->selectedRows();
    for ( const QModelIndex &index : selectedRows )
    {
        removeProfile(view->model()->data(index).toString());
        view->model()->removeRow(index.row());
    }
    view->clearSelection();
}

SettingsDialog::SettingsDialog(MainWindow *parent) :
    QDialog(parent),
    stationProfManager(StationProfilesManager::instance()),
    rigProfManager(RigProfilesManager::instance()),
    rotProfManager(RotProfilesManager::instance()),
    antProfManager(AntProfilesManager::instance()),
    cwKeyProfManager(CWKeyProfilesManager::instance()),
    cwShortcutProfManager(CWShortcutProfilesManager::instance()),
    rotUsrButtonsProfManager(RotUsrButtonsProfilesManager::instance()),
    countyCompleter(nullptr),
    ui(new Ui::SettingsDialog),
    sotaFallback(false),
    potaFallback(false),
    wwffFallback(false),
    tqslVersionTimer(new QTimer(this))
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->dateFormatResultLabel->setVisible(false);
    ui->dateFormatStringEdit->setVisible(false);
    ui->dateFormatDocLabel->setVisible(false);

    ui->rigPortTypeCombo->addItem(tr("Serial"));
    ui->rigPortTypeCombo->addItem(tr("Network"));
    ui->rigPortTypeCombo->addItem(tr("Special - Omnirig"));

#ifdef QLOG_FLATPAK
    ui->lotwTextMessage->setVisible(true);
    ui->tqslPathEdit->setEnabled(false);
    ui->tqslPathEdit->setPlaceholderText(tr("Cannot be changed"));
    ui->tqslPathButton->setEnabled(false);
    ui->tqslAutoButton->setEnabled(false);
#else
    ui->lotwTextMessage->setVisible(false);
#endif

    tqslVersionTimer->setSingleShot(true);
    tqslVersionTimer->setInterval(500);
    connect(tqslVersionTimer, &QTimer::timeout, this, &SettingsDialog::updateTQSLVersionLabel);
    connect(ui->tqslPathEdit, &QLineEdit::textChanged, this, [this]() {
        tqslVersionTimer->start();
    });

    RotTypeModel* rotTypeModel = new RotTypeModel(ui->rotModelSelect);
    ui->rotModelSelect->setModel(rotTypeModel);

    initProfileListView(ui->rotProfilesListView);
    initProfileListView(ui->rotUsrButtonListView);
    initProfileListView(ui->antProfilesListView);
    initProfileListView(ui->cwProfilesListView);
    initProfileListView(ui->cwShortcutListView);
    initProfileListView(ui->stationProfilesListView);

    QStringListModel* cwKeysModel = new QStringListModel(ui->rigAssignedCWKeyCombo);
    ui->rigAssignedCWKeyCombo->setModel(cwKeysModel);

    /* Rig Models must be initialized after rigAssignedCWKeyCombo model !!!! */
    /* becase rigChanged is called and it constain uninitialized
     * CW Model */
    RigTypeModel* rigTypeModel = new RigTypeModel(ui->rigModelSelect);
    ui->rigModelSelect->setModel(rigTypeModel);

    for ( const QPair<int, QString> &driver : Rig::instance()->getDriverList() )
        ui->rigInterfaceCombo->addItem(driver.second, driver.first);

    for ( const QPair<int, QString> &driver : Rotator::instance()->getDriverList() )
        ui->rotInterfaceCombo->addItem(driver.second, driver.first);

    initProfileListView(ui->rigProfilesListView);

    /* Country Combo */
    SqlListModel* countryModel = new SqlListModel("SELECT id, translate_to_locale(name), name  "
                                                  "FROM dxcc_entities_ad1c "
                                                  "ORDER BY 2 COLLATE LOCALEAWARE ASC;", " ", ui->stationCountryCombo);
    while ( countryModel->canFetchMore() )
        countryModel->fetchMore();

    ui->stationCountryCombo->setModel(countryModel);
    ui->stationCountryCombo->setModelColumn(1);

    ShortcutEditorModel *shortcutModel = new ShortcutEditorModel( parent->getUserDefinedShortcutActionList(),
                                                                  parent->getBuiltInStaticShortcutList(), ui->shortcutsTableView);
    ui->shortcutsTableView->setModel(shortcutModel);
    ui->shortcutsTableView->horizontalHeader()->setSectionResizeMode(ShortcutEditorModel::COLUMN_DESCRIPTION, QHeaderView::Stretch);
    ui->shortcutsTableView->setItemDelegateForColumn(ShortcutEditorModel::COLUMN_SHORTCUT, new ShortcutDelegate(ui->shortcutsTableView));
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

    modeTableModel = new QSqlTableModel(ui->modeTableView);
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
    ui->modeTableView->setItemDelegateForColumn(1, new ReadOnlyDelegate(ui->modeTableView));
    ui->modeTableView->setItemDelegateForColumn(4, new ComboFormatDelegate(QStringList() << "CW"<< "PHONE" << "DIGITAL", ui->modeTableView));
    ui->modeTableView->setItemDelegateForColumn(5, new CheckBoxDelegate(ui->modeTableView));
    modeTableModel->select();

    bandTableModel = new QSqlTableModel(ui->bandTableView);
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
    ui->bandTableView->setItemDelegateForColumn(1, new ReadOnlyDelegate(ui->bandTableView));
    ui->bandTableView->setItemDelegateForColumn(2, new UnitFormatDelegate("", 6, 0.001, ui->bandTableView));
    ui->bandTableView->setItemDelegateForColumn(3, new UnitFormatDelegate("", 6, 0.001, ui->bandTableView));
    ui->bandTableView->setItemDelegateForColumn(4,new CheckBoxDelegate(ui->bandTableView));

    bandTableModel->select();

    ui->stationCallsignEdit->setValidator(new QRegularExpressionValidator(Callsign::callsignRegEx(), ui->stationCallsignEdit));
    ui->stationOperatorCallsignEdit->setValidator(new QRegularExpressionValidator(Callsign::callsignRegEx(), ui->stationOperatorCallsignEdit));
    ui->stationLocatorEdit->setValidator(new QRegularExpressionValidator(Gridsquare::gridRegEx(), ui->stationLocatorEdit));
    ui->stationVUCCEdit->setValidator(new QRegularExpressionValidator(Gridsquare::gridVUCCRegEx(), ui->stationVUCCEdit));

    /* https://stackoverflow.com/questions/13145397/regex-for-multicast-ip-address */
    static QRegularExpression multicastAddress("^2(?:2[4-9]|3\\d)(?:\\.(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d?|0)){3}$");

    ui->wsjtMulticastAddressEdit->setValidator(new QRegularExpressionValidator(multicastAddress, ui->wsjtMulticastAddressEdit));
    const QRegularExpression hostsPortRe = HostsPortString::hostsPortRegEx();
    QLineEdit * const hostsPortEdits[] =
    {
        ui->wsjtForwardEdit, ui->notifQSOEdit,
        ui->notifDXSpotsEdit, ui->notifWSJTXCQSpotsEdit,
        ui->notifSpotAlertEdit, ui->notifRigEdit
    };

    for ( QLineEdit *edit : hostsPortEdits )
        edit->setValidator(new QRegularExpressionValidator(hostsPortRe, edit));

    iotaCompleter = new QCompleter(Data::instance()->iotaIDList(), ui->stationIOTAEdit);
    iotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    iotaCompleter->setFilterMode(Qt::MatchContains);
    iotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->stationIOTAEdit->setCompleter(iotaCompleter);

    sotaCompleter = new QCompleter(Data::instance()->sotaIDList(), ui->stationSOTAEdit);
    sotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    sotaCompleter->setFilterMode(Qt::MatchStartsWith);
    sotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->stationSOTAEdit->setCompleter(nullptr);

    wwffCompleter = new QCompleter(Data::instance()->wwffIDList(), ui->stationWWFFEdit);
    wwffCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    wwffCompleter->setFilterMode(Qt::MatchStartsWith);
    wwffCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->stationWWFFEdit->setCompleter(nullptr);

    potaCompleter = new MultiselectCompleter(Data::instance()->potaIDList(), ui->stationPOTAEdit);
    potaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    potaCompleter->setFilterMode(Qt::MatchStartsWith);
    potaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->stationPOTAEdit->setCompleter(nullptr);

    sigCompleter = new QCompleter(Data::instance()->sigIDList(), ui->stationSIGEdit);
    sigCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    sigCompleter->setFilterMode(Qt::MatchStartsWith);
    sigCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->stationSIGEdit->setCompleter(sigCompleter);

    connect(ui->stationCountryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int row)
    {
        const QModelIndex &idx = ui->stationCountryCombo->model()->index(row, 0);
        updateCountyCompleter(ui->stationCountryCombo->model()->data(idx).toInt());
    });

    ui->primaryCallbookCombo->addItem(tr("Disabled"), QVariant(GenericCallbook::CALLBOOK_NAME));
    ui->primaryCallbookCombo->addItem(tr("HamQTH"),   QVariant(HamQTHCallbook::CALLBOOK_NAME));
    ui->primaryCallbookCombo->addItem(tr("QRZ.com"),  QVariant(QRZCallbook::CALLBOOK_NAME));

    populateFlowControlCombo(ui->rigFlowControlSelect);
    populateParityCombo(ui->rigParitySelect);
    populateFlowControlCombo(ui->rotFlowControlSelect);
    populateParityCombo(ui->rotParitySelect);

    ui->cwModelSelect->addItem(tr("Dummy"), CWKey::DUMMY_KEYER);
    ui->cwModelSelect->addItem(tr("Morse Over CAT"), CWKey::MORSEOVERCAT);
    ui->cwModelSelect->addItem(tr("WinKey"), CWKey::WINKEY_KEYER);
    ui->cwModelSelect->addItem(tr("CWDaemon"), CWKey::CWDAEMON_KEYER);
    ui->cwModelSelect->addItem(tr("FLDigi"), CWKey::FLDIGI_KEYER);
    ui->cwModelSelect->setCurrentIndex(ui->cwModelSelect->findData(DEFAULT_CWKEY_MODEL));

    ui->cwKeyModeSelect->addItem(tr("Single Paddle"), CWKey::SINGLE_PADDLE);
    ui->cwKeyModeSelect->addItem(tr("IAMBIC A"), CWKey::IAMBIC_A);
    ui->cwKeyModeSelect->addItem(tr("IAMBIC B"), CWKey::IAMBIC_B);
    ui->cwKeyModeSelect->addItem(tr("Ultimate"), CWKey::ULTIMATE);
    ui->cwKeyModeSelect->setCurrentIndex(ui->cwKeyModeSelect->findData(CWKey::IAMBIC_B));

    const QList<QPair<QString, int>> sideToneFreqsList = CWWinKey::sidetoneFrequencies();
    for ( const QPair<QString, int> &f : sideToneFreqsList )
        ui->cwSidetoneFreqCombo->addItem(f.first, f.second);
    ui->cwSidetoneFreqCombo->setCurrentIndex(ui->cwSidetoneFreqCombo->findData(CWWinKey::defaultSidetoneFrequency()));

    populateSignalCombo(ui->rigDTRCombo);
    populateSignalCombo(ui->rigRTSCombo);

    /* disable WSJTX Multicast by default */
    joinMulticastChanged(false);

    generateMembershipCheckboxes();

    readSettings();
}

void SettingsDialog::save()
{
    FCT_IDENTIFICATION;

    if ( stationProfManager->profileNameList().isEmpty() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Please, define at least one Station Locations Profile"));
        return;
    }

    const QString pleaseModifyTXT = tr("Press <b>Modify</b> to confirm the profile changes or <b>Cancel</b>.");

    struct PendingModify { QPushButton *button; int tabIndex; int equipTabIndex; };
    const PendingModify modifyChecks[] =
    {
        {ui->stationAddProfileButton,       0, -1},
        {ui->antAddProfileButton,           1,  0},
        {ui->cwAddProfileButton,            1,  1},
        {ui->cwShortcutAddProfileButton,    1,  1},
        {ui->rigAddProfileButton,           1,  2},
        {ui->rotAddProfileButton,           1,  3},
        {ui->rotUsrButtonAddProfileButton,  1,  3},
    };
    for ( const PendingModify &check : modifyChecks )
    {
        if ( check.button->text() == tr("Modify") )
        {
            ui->tabWidget->setCurrentIndex(check.tabIndex);
            if ( check.equipTabIndex >= 0 )
                ui->equipmentTabWidget->setCurrentIndex(check.equipTabIndex);
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"), pleaseModifyTXT);
            return;
        }
    }

    if ( ui->wsjtMulticastCheckbox->isChecked()
         && ! ui->wsjtMulticastAddressEdit->hasAcceptableInput())
    {
        ui->tabWidget->setCurrentIndex(8);
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("WSJTX Multicast is enabled but the Address is not a multicast address."));
        return;
    }

    if ( !ui->wsjtForwardEdit->text().isEmpty() )
    {
        HostsPortString list(ui->wsjtForwardEdit->text());
        if ( list.hasLocalIPWithPort(ui->wsjtPortSpin->value()) )
        {
            ui->tabWidget->setCurrentIndex(8);
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 QMessageBox::tr("Loop detected. Raw UDP forward uses the same port as the WSJT-X receiving port."));
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

    if ( ! ui->rigPortEdit->text().isEmpty()
         && ! ui->rigPortEdit->hasAcceptableInput())
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Rig port must be a valid COM port.<br>For Windows use COMxx, for unix-like OS use a path to device"));
        return;
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
        ui->rigAddProfileButton->setText(tr("Add"));

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
        profile.rts = ui->rigRTSCombo->currentData().toString();
        profile.dtr = ui->rigDTRCombo->currentData().toString();
        profile.civAddr = ui->rigCIVAddrSpinBox->value();
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
    profile.getSplitInfo = ui->rigGetSplitCheckBox->isChecked();

    // Rigctld sharing settings
    profile.shareRigctld = ui->rigShareCheckBox->isChecked();
    profile.rigctldPort = ui->rigSharePortSpinBox->value();
    profile.rigctldPath = rigctldPath;
    profile.rigctldArgs = rigctldArgs;

    rigProfManager->addProfile(profile.profileName, profile);

    refreshRigProfilesView();

    clearRigProfileForm();
}

void SettingsDialog::delRigProfile()
{
    FCT_IDENTIFICATION;
    deleteSelectedProfiles(ui->rigProfilesListView, [this](const QString &name) {
        rigProfManager->removeProfile(name);
    });
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
    ui->rigGetSplitCheckBox->setChecked(profile.getSplitInfo);

    setComboByData(ui->rigFlowControlSelect, profile.flowcontrol.toLower());
    setComboByData(ui->rigParitySelect, profile.parity.toLower());

    const RigCaps &caps = Rig::instance()->getRigCaps(static_cast<Rig::DriverID>(profile.driver), profile.model);

    setComboByData(ui->rigPTTTypeCombo, profile.pttType, PTT_TYPE_CAT_INDEX);
    ui->rigPTTPortEdit->setText(profile.pttPortPath);
    setComboByData(ui->rigRTSCombo, profile.rts, PTT_TYPE_CAT_INDEX);
    setComboByData(ui->rigDTRCombo, profile.dtr, PTT_TYPE_CAT_INDEX);

    ui->rigCIVAddrSpinBox->setValue(( profile.civAddr >= 0 ) ? profile.civAddr : CIVADDR_DISABLED_VALUE);

    // Rigctld sharing settings
    ui->rigShareCheckBox->setChecked(profile.shareRigctld);
    ui->rigSharePortSpinBox->setValue(profile.rigctldPort);
    rigctldPath = profile.rigctldPath;
    rigctldArgs = profile.rigctldArgs;

    setUIBasedOnRigCaps(caps);
    updateRigShareEnabled();

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
    ui->rigDTRCombo->setCurrentIndex(0);
    ui->rigRTSCombo->setCurrentIndex(0);
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
    ui->rigCIVAddrSpinBox->setValue(CIVADDR_DISABLED_VALUE);
    ui->rigGetSplitCheckBox->setChecked(false);

    // Rigctld sharing settings
    ui->rigShareCheckBox->setChecked(false);
    ui->rigSharePortSpinBox->setValue(4532);
    rigctldPath.clear();
    rigctldArgs.clear();

    rigChanged(ui->rigModelSelect->currentIndex());
    updateRigShareEnabled();
}

void SettingsDialog::rigRXOffsetChanged(int)
{
    FCT_IDENTIFICATION;
    updateOffsetSpinBox(ui->rigGetRITCheckBox, ui->rigRXOffsetSpinBox);
}

void SettingsDialog::rigTXOffsetChanged(int)
{
    FCT_IDENTIFICATION;
    updateOffsetSpinBox(ui->rigGetXITCheckBox, ui->rigTXOffsetSpinBox);
}

void SettingsDialog::rigGetFreqChanged(int)
{
    FCT_IDENTIFICATION;

    ui->rigQSYWipingCheckBox->setChecked(ui->rigGetFreqCheckBox->isChecked());
    ui->rigQSYWipingCheckBox->setEnabled(ui->rigGetFreqCheckBox->isChecked());
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
    ui->rigModelSelect->setCurrentIndex(( driverID == Rig::HAMLIB_DRIVER ) ? ui->rigModelSelect->findData(Rig::DEFAULT_MODEL)
                                                                           : 0 );
    ui->rigPTTTypeCombo->clear();
    setComboByData(ui->rigRTSCombo, SerialPort::SERIAL_SIGNAL_NONE);
    setComboByData(ui->rigDTRCombo, SerialPort::SERIAL_SIGNAL_NONE);

    const QList<QPair<QString, QString>> &pttTypes = Rig::instance()->getPTTTypeList(static_cast<Rig::DriverID>(driverID));

    for ( const QPair<QString, QString> &type : pttTypes )
        ui->rigPTTTypeCombo->addItem(type.second, type.first);

    ui->rigRTSCombo->setVisible((driverID == Rig::HAMLIB_DRIVER));
    ui->rigDTRCombo->setVisible((driverID == Rig::HAMLIB_DRIVER));
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

    if ( ! ui->rotPortEdit->text().isEmpty()
         && ! ui->rotPortEdit->hasAcceptableInput() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Rotator port must be a valid COM port.<br>For Windows use COMxx, for unix-like OS use a path to device"));
        return;
    }

    if ( ui->rotAddProfileButton->text() == tr("Modify"))
        ui->rotAddProfileButton->setText(tr("Add"));

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
    deleteSelectedProfiles(ui->rotProfilesListView, [this](const QString &name) {
        rotProfManager->removeProfile(name);
    });
    clearRotProfileForm();
}

void SettingsDialog::refreshRotProfilesView()
{
    FCT_IDENTIFICATION;
    refreshProfileView(ui->rotProfilesListView, rotProfManager->profileNameList());
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

    setComboByData(ui->rotFlowControlSelect, profile.flowcontrol.toLower());
    setComboByData(ui->rotParitySelect, profile.parity.toLower());

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
        ui->rotModelSelect->setCurrentIndex(ui->rotModelSelect->findData(Rig::DEFAULT_MODEL));
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

    QLineEdit * const buttonEdits[] = { ui->rotUsrButton1Edit, ui->rotUsrButton2Edit, ui->rotUsrButton3Edit, ui->rotUsrButton4Edit };
    QSpinBox * const spinBoxes[]   = { ui->rotUsrButtonSpinBox1, ui->rotUsrButtonSpinBox2, ui->rotUsrButtonSpinBox3, ui->rotUsrButtonSpinBox4 };
    for ( int i = 0; i < 4; ++i )
    {
        profile.shortDescs[i] = buttonEdits[i]->text();
        profile.bearings[i]   = spinBoxes[i]->value();
    }

    rotUsrButtonsProfManager->addProfile(profile.profileName, profile);

    refreshRotUsrButtonsProfilesView();

    clearRotUsrButtonsProfileForm();
}

void SettingsDialog::delRotUsrButtonsProfile()
{
    FCT_IDENTIFICATION;
    deleteSelectedProfiles(ui->rotUsrButtonListView, [this](const QString &name) {
        rotUsrButtonsProfManager->removeProfile(name);
    });
    clearRotUsrButtonsProfileForm();
}

void SettingsDialog::refreshRotUsrButtonsProfilesView()
{
    FCT_IDENTIFICATION;
    refreshProfileView(ui->rotUsrButtonListView, rotUsrButtonsProfManager->profileNameList());
}

void SettingsDialog::doubleClickRotUsrButtonsProfile(QModelIndex i)
{
    FCT_IDENTIFICATION;

    RotUsrButtonsProfile profile = rotUsrButtonsProfManager->getProfile(ui->rotUsrButtonListView->model()->data(i).toString());

    ui->rotUsrButtonProfileNameEdit->setText(profile.profileName);

    QLineEdit * const buttonEdits[] = { ui->rotUsrButton1Edit, ui->rotUsrButton2Edit, ui->rotUsrButton3Edit, ui->rotUsrButton4Edit };
    QSpinBox * const spinBoxes[]   = { ui->rotUsrButtonSpinBox1, ui->rotUsrButtonSpinBox2, ui->rotUsrButtonSpinBox3, ui->rotUsrButtonSpinBox4 };
    for ( int i = 0; i < 4; ++i )
    {
        buttonEdits[i]->setText(profile.shortDescs[i]);
        spinBoxes[i]->setValue(profile.bearings[i]);
    }

    ui->rotUsrButtonAddProfileButton->setText(tr("Modify"));
}

void SettingsDialog::clearRotUsrButtonsProfileForm()
{
    FCT_IDENTIFICATION;

    ui->rotUsrButtonProfileNameEdit->setPlaceholderText(QString());
    ui->rotUsrButtonProfileNameEdit->clear();

    QLineEdit * const buttonEdits[] = { ui->rotUsrButton1Edit, ui->rotUsrButton2Edit, ui->rotUsrButton3Edit, ui->rotUsrButton4Edit };
    QSpinBox * const spinBoxes[]   = { ui->rotUsrButtonSpinBox1, ui->rotUsrButtonSpinBox2, ui->rotUsrButtonSpinBox3, ui->rotUsrButtonSpinBox4 };
    for ( int i = 0; i < 4; ++i )
    {
        buttonEdits[i]->clear();
        spinBoxes[i]->setValue(-1);
    }

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
    deleteSelectedProfiles(ui->antProfilesListView, [this](const QString &name) {
        antProfManager->removeProfile(name);
    });
    clearAntProfileForm();
}

void SettingsDialog::refreshAntProfilesView()
{
    FCT_IDENTIFICATION;
    refreshProfileView(ui->antProfilesListView, antProfManager->profileNameList());
}

void SettingsDialog::doubleClickAntProfile(QModelIndex i)
{
    AntProfile profile = antProfManager->getProfile(ui->antProfilesListView->model()->data(i).toString());

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

    if ( ! ui->cwPortEdit->text().isEmpty()
         && ! ui->cwPortEdit->hasAcceptableInput() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("CW Keyer port must be a valid COM port.<br>For Windows use COMxx, for unix-like OS use a path to device"));
        return;
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

    cwKeyNewProfile.paddleSwap = cwKeyNewProfile.model == CWKey::WINKEY_KEYER
                                 && cwKeyNewProfile.keyMode != CWKey::SINGLE_PADDLE
                                 && ui->cwSwapPaddlesCheckbox->isEnabled()
                                 && ui->cwSwapPaddlesCheckbox->isChecked();

    cwKeyNewProfile.paddleOnlySidetone = cwKeyNewProfile.model == CWKey::WINKEY_KEYER
                                      && ui->cwPaddleOnlySidetoneCheckbox->isEnabled()
                                      && ui->cwPaddleOnlySidetoneCheckbox->isChecked();

    cwKeyNewProfile.sidetoneFrequency = ( cwKeyNewProfile.model == CWKey::WINKEY_KEYER
                                          || cwKeyNewProfile.model == CWKey::CWDAEMON_KEYER )
                                        ? ui->cwSidetoneFreqCombo->currentData().toInt()
                                        : CWWinKey::defaultSidetoneFrequency();

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


    const QModelIndexList cwSelectedRows = ui->cwProfilesListView->selectionModel()->selectedRows();
    for ( const QModelIndex &index : cwSelectedRows )
    {
        QStringList  dependentRigs;
        QString removedCWProfile = ui->cwProfilesListView->model()->data(index).toString();
        const QStringList &availableRigProfileNames = rigProfManager->profileNameList();

        /* needed to verify whether removed Key is not used in Rig Profile as an assigned Key*/
        for ( const QString &rigProfileName : availableRigProfileNames )
        {
            qCDebug(runtime) << "Checking Rig Profile" << rigProfileName;
            RigProfile testedRig = rigProfManager->getProfile(rigProfileName);
            if ( testedRig.assignedCWKey == removedCWProfile )
                dependentRigs << testedRig.profileName;
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
    refreshProfileView(ui->cwProfilesListView, cwKeyProfManager->profileNameList());
}

void SettingsDialog::doubleClickCWKeyProfile(QModelIndex i)
{
    FCT_IDENTIFICATION;

    CWKeyProfile profile = cwKeyProfManager->getProfile(ui->cwProfilesListView->model()->data(i).toString());

    ui->cwProfileNameEdit->setText(profile.profileName);
    ui->cwModelSelect->setCurrentIndex(ui->cwModelSelect->findData(profile.model));
    ui->cwDefaulSpeed->setValue(profile.defaultSpeed);
    ui->cwKeyModeSelect->setCurrentIndex(ui->cwKeyModeSelect->findData(profile.keyMode));
    ui->cwPortEdit->setText(profile.portPath);
    ui->cwBaudSelect->setCurrentText(QString::number(profile.baudrate));
    ui->cwHostNameEdit->setText(profile.hostname);
    ui->cwNetPortSpin->setValue(profile.netport);
    ui->cwSwapPaddlesCheckbox->setChecked(profile.paddleSwap);
    ui->cwPaddleOnlySidetoneCheckbox->setChecked(profile.paddleOnlySidetone);
    {
        int idx = ui->cwSidetoneFreqCombo->findData(profile.sidetoneFrequency);
        ui->cwSidetoneFreqCombo->setCurrentIndex(idx >= 0 ? idx : ui->cwSidetoneFreqCombo->findData(CWWinKey::defaultSidetoneFrequency()));
    }

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
    ui->cwSwapPaddlesCheckbox->setChecked(false);
    ui->cwPaddleOnlySidetoneCheckbox->setChecked(false);
    ui->cwSidetoneFreqCombo->setCurrentIndex(ui->cwSidetoneFreqCombo->findData(CWWinKey::defaultSidetoneFrequency()));

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
        ui->cwShortcutAddProfileButton->setText(tr("Add"));

    CWShortcutProfile profile;

    profile.profileName = ui->cwShortcutProfileNameEdit->text();

    QLineEdit * const shortEdits[] = { ui->cwShortcutF1ShortEdit, ui->cwShortcutF2ShortEdit, ui->cwShortcutF3ShortEdit, ui->cwShortcutF4ShortEdit, ui->cwShortcutF5ShortEdit, ui->cwShortcutF6ShortEdit, ui->cwShortcutF7ShortEdit };
    QLineEdit * const macroEdits[] = { ui->cwShortcutF1MacroEdit, ui->cwShortcutF2MacroEdit, ui->cwShortcutF3MacroEdit, ui->cwShortcutF4MacroEdit, ui->cwShortcutF5MacroEdit, ui->cwShortcutF6MacroEdit, ui->cwShortcutF7MacroEdit };
    for ( int i = 0; i < 7; ++i )
    {
        profile.shortDescs[i] = shortEdits[i]->text();
        profile.macros[i]     = macroEdits[i]->text();
    }

    cwShortcutProfManager->addProfile(profile.profileName, profile);

    refreshCWShortcutProfilesView();

    clearCWShortcutProfileForm();
}

void SettingsDialog::delCWShortcutProfile()
{
    FCT_IDENTIFICATION;
    deleteSelectedProfiles(ui->cwShortcutListView, [this](const QString &name) {
        cwShortcutProfManager->removeProfile(name);
    });
    clearCWShortcutProfileForm();
}

void SettingsDialog::refreshCWShortcutProfilesView()
{
    FCT_IDENTIFICATION;
    refreshProfileView(ui->cwShortcutListView, cwShortcutProfManager->profileNameList());
}

void SettingsDialog::doubleClickCWShortcutProfile(QModelIndex i)
{
    FCT_IDENTIFICATION;

    CWShortcutProfile profile = cwShortcutProfManager->getProfile(ui->cwShortcutListView->model()->data(i).toString());

    ui->cwShortcutProfileNameEdit->setText(profile.profileName);

    QLineEdit * const shortEdits[] = { ui->cwShortcutF1ShortEdit, ui->cwShortcutF2ShortEdit, ui->cwShortcutF3ShortEdit, ui->cwShortcutF4ShortEdit, ui->cwShortcutF5ShortEdit, ui->cwShortcutF6ShortEdit, ui->cwShortcutF7ShortEdit };
    QLineEdit * const macroEdits[] = { ui->cwShortcutF1MacroEdit, ui->cwShortcutF2MacroEdit, ui->cwShortcutF3MacroEdit, ui->cwShortcutF4MacroEdit, ui->cwShortcutF5MacroEdit, ui->cwShortcutF6MacroEdit, ui->cwShortcutF7MacroEdit };
    for ( int i = 0; i < 7; ++i )
    {
        shortEdits[i]->setText(profile.shortDescs[i]);
        macroEdits[i]->setText(profile.macros[i]);
    }

    ui->cwShortcutAddProfileButton->setText(tr("Modify"));
}

void SettingsDialog::clearCWShortcutProfileForm()
{
    FCT_IDENTIFICATION;

    ui->cwShortcutProfileNameEdit->setPlaceholderText(QString());
    ui->cwShortcutProfileNameEdit->clear();

    QLineEdit * const shortEdits[] = { ui->cwShortcutF1ShortEdit, ui->cwShortcutF2ShortEdit, ui->cwShortcutF3ShortEdit, ui->cwShortcutF4ShortEdit, ui->cwShortcutF5ShortEdit, ui->cwShortcutF6ShortEdit, ui->cwShortcutF7ShortEdit };
    QLineEdit * const macroEdits[] = { ui->cwShortcutF1MacroEdit, ui->cwShortcutF2MacroEdit, ui->cwShortcutF3MacroEdit, ui->cwShortcutF4MacroEdit, ui->cwShortcutF5MacroEdit, ui->cwShortcutF6MacroEdit, ui->cwShortcutF7MacroEdit };
    for ( int i = 0; i < 7; ++i )
    {
        shortEdits[i]->clear();
        macroEdits[i]->clear();
    }

    ui->cwShortcutAddProfileButton->setText(tr("Add"));
}

void SettingsDialog::refreshRigProfilesView()
{
    FCT_IDENTIFICATION;
    refreshProfileView(ui->rigProfilesListView, rigProfManager->profileNameList());
}

void SettingsDialog::refreshStationProfilesView()
{
    FCT_IDENTIFICATION;
    refreshProfileView(ui->stationProfilesListView, stationProfManager->profileNameList());
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

    if ( !ui->stationOperatorCallsignEdit->text().isEmpty()
         && ! ui->stationOperatorCallsignEdit->hasAcceptableInput() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Operator Callsign has an invalid format"));
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
    profile.operatorCallsign = ui->stationOperatorCallsignEdit->text().toUpper();
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
    profile.county = ui->stationCountyEdit->text();
    profile.darcDOK = ui->stationDarcDokEdit->text().toUpper();

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
    deleteSelectedProfiles(ui->stationProfilesListView, [this](const QString &name) {
        stationProfManager->removeProfile(name);
    });
    clearStationProfileForm();
}

void SettingsDialog::doubleClickStationProfile(QModelIndex i)
{
    FCT_IDENTIFICATION;

    StationProfile profile = stationProfManager->getProfile(ui->stationProfilesListView->model()->data(i).toString());

    ui->stationProfileNameEdit->setText(profile.profileName);
    ui->stationCallsignEdit->blockSignals(true);
    ui->stationCallsignEdit->setText(profile.callsign);
    setValidationResultColor(ui->stationCallsignEdit);
    ui->stationCallsignEdit->blockSignals(false);
    ui->stationLocatorEdit->setText(profile.locator);
    ui->stationOperatorEdit->setText(profile.operatorName);
    ui->stationOperatorCallsignEdit->setText(profile.operatorCallsign);
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
    ui->stationCountyEdit->setText(profile.county);
    ui->stationDarcDokEdit->setText(profile.darcDOK);
    const QModelIndexList &countryIndex = ui->stationCountryCombo->model()->match(ui->stationCountryCombo->model()->index(0,0),
                                                                           Qt::DisplayRole, profile.dxcc,
                                                                           1, Qt::MatchExactly);
    if ( countryIndex.size() >= 1 )
        ui->stationCountryCombo->setCurrentIndex(countryIndex.at(0).row());

    updateCountyCompleter(profile.dxcc);

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
    ui->stationOperatorCallsignEdit->clear();
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
    ui->stationCountyEdit->clear();
    ui->stationDarcDokEdit->clear();

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
         || driverID == Rig::OMNIRIGV2_DRIVER )
    {
        ui->rigPortTypeCombo->setCurrentIndex(RIGPORT_SPECIAL_OMNIRIG_INDEX);
        ui->rigPortTypeCombo->setEnabled(false);
    }
    else
    {
        ui->rigPortTypeCombo->setCurrentIndex(caps.isNetworkOnly ? RIGPORT_NETWORK_INDEX : RIGPORT_SERIAL_INDEX);
        ui->rigPortTypeCombo->setEnabled(!caps.isNetworkOnly);

        if ( !caps.isNetworkOnly )
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

    ui->rigGetSplitCheckBox->setEnabled(true);
    ui->rigGetSplitCheckBox->setChecked(true);

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
        ui->rotPortTypeCombo->setEnabled(true);

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

    ui->cwStackedWidget->setCurrentIndex(( CWKey::isNetworkKey(currentType) ) ? STACKED_WIDGET_NETWORK_SETTING
                                                                              : STACKED_WIDGET_SERIAL_SETTING);
    if ( currentType == CWKey::MORSEOVERCAT
         || currentType == CWKey::CWDAEMON_KEYER
         || currentType == CWKey::FLDIGI_KEYER )
    {
        ui->cwBaudSelect->setEnabled(false);
        ui->cwPortEdit->clear();
        ui->cwPortEdit->setEnabled(false);
        ui->cwKeyModeSelect->setEnabled(false);
        ui->cwDefaulSpeed->setEnabled(true);
        ui->cwSwapPaddlesCheckbox->setEnabled(false);
        ui->cwPaddleOnlySidetoneCheckbox->setEnabled(false);
        ui->cwSidetoneFreqCombo->setEnabled(currentType == CWKey::CWDAEMON_KEYER);

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
    else   // WINKEYS
    {
        ui->cwBaudSelect->setEnabled(true);
        ui->cwPortEdit->setEnabled(true);
        ui->cwKeyModeSelect->setEnabled(true);
        ui->cwDefaulSpeed->setEnabled(true);
        ui->cwSwapPaddlesCheckbox->setEnabled(true);
        ui->cwPaddleOnlySidetoneCheckbox->setEnabled(currentType == CWKey::WINKEY_KEYER);
        ui->cwSidetoneFreqCombo->setEnabled(currentType == CWKey::WINKEY_KEYER);
    }

    ui->cwBaudSelect->setCurrentText(( currentType == CWKey::WINKEY_KEYER ) ? "1200"
                                                                            : "115200");
}

void SettingsDialog::cwModeChanged(int)
{
    FCT_IDENTIFICATION;

    ui->cwSwapPaddlesCheckbox->setEnabled( CWKey::intToTypeID(ui->cwModelSelect->currentData().toInt()) == CWKey::WINKEY_KEYER
                                           && CWKey::intToModeID(ui->cwKeyModeSelect->currentData().toInt()) != CWKey::SINGLE_PADDLE );
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

void SettingsDialog::tqslAutoDetect()
{
    FCT_IDENTIFICATION;

    const QString detectedPath = LotwBase::findTQSLPath();

    if ( detectedPath.isEmpty() )
    {
        updateTQSLVersionLabel();
        QMessageBox::warning(this, tr("Auto Detect"),
                             tr("TQSL was not found on this system.\n"
                                "Please install TQSL or specify the path manually."));
    }
    else
    {
        ui->tqslPathEdit->setText(detectedPath);
    }
}

void SettingsDialog::updateTQSLVersionLabel()
{
    FCT_IDENTIFICATION;

    const QString path = ui->tqslPathEdit->text().trimmed();
    const bool isAutoDetect = path.isEmpty();
    const QString resolvedPath = isAutoDetect ? LotwBase::findTQSLPath() : path;
    const QString NOTFOUND = QString("<span style=\"color: red;\">✗ %1</span>").arg(tr("Not found"));

    if ( resolvedPath.isEmpty() )
    {
        ui->tqslVersionValueLabel->setText(NOTFOUND);
        return;
    }

    const TQSLVersion version = LotwBase::getTQSLVersion(resolvedPath);

    if ( !version.isValid() )
    {
        ui->tqslVersionValueLabel->setText(NOTFOUND);
        return;
    }

    const QString versionStr = QString("<span style=\"color: green;\">✓ %1.%2.%3</span>")
                                   .arg(version.major)
                                   .arg(version.minor)
                                   .arg(version.patch);

    if ( isAutoDetect )
        ui->tqslVersionValueLabel->setText(QString("%1 (%2)").arg(versionStr, resolvedPath));
    else
        ui->tqslVersionValueLabel->setText(versionStr);
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

    setValidationResultColor(ui->rotPortEdit);
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

void SettingsDialog::adjustOperatorTextColor()
{
    FCT_IDENTIFICATION;

    setValidationResultColor(ui->stationOperatorCallsignEdit);
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

    ui->stationSOTAEdit->setCompleter(( newSOTA.length() >= 3 ) ? sotaCompleter : nullptr);
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
            ui->stationLocatorEdit->setText(SOTAGrid.getGrid());
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

    ui->stationPOTAEdit->setCompleter(( newPOTA.length() >= 3 ) ? potaCompleter : nullptr);
    ui->stationQTHEdit->clear();
    ui->stationLocatorEdit->clear();
}

void SettingsDialog::potaEditFinished()
{
    FCT_IDENTIFICATION;

    QStringList potaList = ui->stationPOTAEdit->text().split("@");
    QString potaString;

    if ( !potaList.isEmpty() )
        potaString = potaList[0];
    else
        potaString = ui->stationPOTAEdit->text();

    POTAEntity potaInfo = Data::instance()->lookupPOTA(potaString);

    if ( potaInfo.reference.toUpper() == potaString.toUpper()
         && !potaInfo.name.isEmpty() )
    {
        potaFallback = false;
        ui->stationQTHEdit->setText(potaInfo.name);
        Gridsquare POTAGrid(potaInfo.grid);
        if ( POTAGrid.isValid() )
            ui->stationLocatorEdit->setText(POTAGrid.getGrid());
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

    ui->stationWWFFEdit->setCompleter(( newWWFF.length() >= 3 ) ? wwffCompleter : nullptr);

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
    else if ( primaryCallbookSelection == HamQTHCallbook::CALLBOOK_NAME )
    {
        ui->secondaryCallbookCombo->setEnabled(true);
        ui->secondaryCallbookCombo->clear();
        ui->secondaryCallbookCombo->addItem(tr("Disabled"), QVariant(GenericCallbook::CALLBOOK_NAME));
        ui->secondaryCallbookCombo->addItem(tr("QRZ.com"),  QVariant(QRZCallbook::CALLBOOK_NAME));
    }
    else if ( primaryCallbookSelection == QRZCallbook::CALLBOOK_NAME )
    {
        ui->secondaryCallbookCombo->setEnabled(true);
        ui->secondaryCallbookCombo->clear();
        ui->secondaryCallbookCombo->addItem(tr("Disabled"), QVariant(GenericCallbook::CALLBOOK_NAME));
        ui->secondaryCallbookCombo->addItem(tr("HamQTH"),  QVariant(HamQTHCallbook::CALLBOOK_NAME));
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
        ui->hrdlogOnAirCheckBox->setChecked(false);
}

void SettingsDialog::clublogSettingChanged()
{
    FCT_IDENTIFICATION;

    ui->clublogUploadImmediatelyCheckbox->setEnabled(!ui->clublogEmailEdit->text().isEmpty()
                                                 && !ui->clublogPasswordEdit->text().isEmpty());
    if ( !ui->clublogUploadImmediatelyCheckbox->isEnabled() )
        ui->clublogUploadImmediatelyCheckbox->setChecked(false);
}

void SettingsDialog::updateDateFormatResult()
{
    FCT_IDENTIFICATION;

    ui->dateFormatResultLabel->setText(QDate::currentDate().toString(ui->dateFormatStringEdit->text()));
}

void SettingsDialog::rigFlowControlChanged(int)
{
    FCT_IDENTIFICATION;

    // if HW handshake is enabled then RTS must be None
    bool isHWControlEnabled = (ui->rigFlowControlSelect->currentData().toString() == SerialPort::SERIAL_FLOWCONTROL_HARDWARE);

    if ( isHWControlEnabled )
        setComboByData(ui->rigRTSCombo, SerialPort::SERIAL_SIGNAL_NONE);
    ui->rigRTSCombo->setEnabled(!isHWControlEnabled);
}

void SettingsDialog::showRigctldAdvanced()
{
    FCT_IDENTIFICATION;

    RigctldAdvancedDialog dialog(this);
    dialog.setPath(rigctldPath);
    dialog.setArgs(rigctldArgs);

    if (dialog.exec() == QDialog::Accepted)
    {
        rigctldPath = dialog.getPath();
        rigctldArgs = dialog.getArgs();
    }
}

void SettingsDialog::rigShareChanged(int)
{
    FCT_IDENTIFICATION;

    updateRigShareEnabled();
}

void SettingsDialog::updateRigShareEnabled()
{
    FCT_IDENTIFICATION;

    Rig::DriverID driverID = static_cast<Rig::DriverID>(ui->rigInterfaceCombo->currentData().toInt());
    int portType = ui->rigPortTypeCombo->currentIndex();

    // Share rigctld is only available for Hamlib driver with serial connection
    bool canShare = (driverID == Rig::HAMLIB_DRIVER) && (portType == RIGPORT_SERIAL_INDEX);

    ui->rigShareCheckBox->setEnabled(canShare);
    ui->rigSharePortSpinBox->setEnabled(canShare && ui->rigShareCheckBox->isChecked());
    ui->rigShareAdvancedButton->setEnabled(canShare && ui->rigShareCheckBox->isChecked());

    if ( !canShare )
    {
        ui->rigShareCheckBox->setChecked(false);

        if (driverID != Rig::HAMLIB_DRIVER)
            ui->rigShareCheckBox->setToolTip(tr("Rig sharing is only available for Hamlib driver"));
        else
            ui->rigShareCheckBox->setToolTip(tr("Rig sharing is not available for network connection"));
    }
    else
        ui->rigShareCheckBox->setToolTip(tr("Start rigctld daemon to share rig with other applications (e.g. WSJT-X)"));
}

void SettingsDialog::qrzAddCallsignAPIKey()
{
    FCT_IDENTIFICATION;

    QAbstractItemModel *model = ui->qrzCallsignApiKeyTableView->model();

    int newRow = model->rowCount();
    model->insertRow(newRow);
    QModelIndex index = model->index(newRow, 0);
    ui->qrzCallsignApiKeyTableView->setCurrentIndex(index);
    ui->qrzCallsignApiKeyTableView->edit(index);
}

void SettingsDialog::qrzDelCallsignAPIKey()
{
    FCT_IDENTIFICATION;

    QModelIndex index = ui->qrzCallsignApiKeyTableView->currentIndex();
    if ( !index.isValid() ) return;

    ui->qrzCallsignApiKeyTableView->model()->removeRow(index.row());
}

void SettingsDialog::onDeleteAllPasswords()
{
    FCT_IDENTIFICATION;

    CredentialStore::instance()->deleteAllPasswords();
    QMessageBox::information(this, tr("Delete Passwords"), tr("All passwords have been deleted"));

    // It is necessary to close the dialog, because we would have to delete
    // all the passwords in the dialog. It would be safer to close it.
    reject();
}

void SettingsDialog::onDeleteAllQSOs()
{
    FCT_IDENTIFICATION;

    QProgressDialog *progress = new QProgressDialog(tr("Deleting all QSOs..."), QString(), 0, 0, this);
    progress->setWindowModality(Qt::ApplicationModal);
    progress->setMinimumDuration(0);
    progress->setAttribute(Qt::WA_DeleteOnClose, true);
    progress->show();

    QTimer::singleShot(0, this, [this, progress]()
    {
        QSqlQuery query;
        bool ok = query.exec(QStringLiteral("DELETE FROM contacts"));

        if (!ok)
            qCDebug(runtime) << "Cannot delete QSOs:" << query.lastError().text();

        progress->close();

        if (!ok)
            QMessageBox::warning(this, tr("Error"), tr("Failed to delete all QSOs."));
        accept();
    });
}

void SettingsDialog::readSettings()
{
    FCT_IDENTIFICATION;

    refreshStationProfilesView();
    refreshRigProfilesView();
    refreshRotProfilesView();
    refreshRotUsrButtonsProfilesView();
    refreshAntProfilesView();
    refreshCWKeyProfilesView();
    refreshCWShortcutProfilesView();
    refreshRigAssignedCWKeyCombo();

    /************/
    /* Callbook */
    /************/

    int primaryCallbookIndex = ui->primaryCallbookCombo->findData(LogParam::getPrimaryCallbook(GenericCallbook::CALLBOOK_NAME));

    ui->primaryCallbookCombo->setCurrentIndex(primaryCallbookIndex);

    int secondaryCallbookIndex = ui->secondaryCallbookCombo->findData(LogParam::getSecondaryCallbook(GenericCallbook::CALLBOOK_NAME));

    ui->secondaryCallbookCombo->setCurrentIndex(secondaryCallbookIndex);

    ui->hamQthUsernameEdit->setText(HamQTHBase::getUsername());
    ui->hamQthPasswordEdit->setText(HamQTHBase::getPasswd());

    ui->qrzUsernameEdit->setText(QRZBase::getUsername());
    ui->qrzPasswordEdit->setText(QRZBase::getPasswd(QRZBase::getUsername()));

    ui->webLookupURLEdit->setText(GenericCallbook::getWebLookupURL("", QString(), false));

    /********/
    /* LoTW */
    /********/
    ui->lotwUsernameEdit->setText(LotwBase::getUsername());
    ui->lotwPasswordEdit->setText(LotwBase::getPasswd());
    ui->tqslPathEdit->setText(LotwBase::getTQSLPath());
    updateTQSLVersionLabel();

    /***********/
    /* ClubLog */
    /***********/
    ui->clublogEmailEdit->setText(ClubLogBase::getEmail());
    ui->clublogPasswordEdit->setText(ClubLogBase::getPasswd());
    ui->clublogUploadImmediatelyCheckbox->setChecked(ClubLogBase::isUploadImmediatelyEnabled());

    /********/
    /* eQSL */
    /********/
    ui->eqslUsernameEdit->setText(EQSLBase::getUsername());
    ui->eqslPasswordEdit->setText(EQSLBase::getPasswd());

    /**********/
    /* HRDLog */
    /**********/
    ui->hrdlogCallsignEdit->setText(HRDLogBase::getRegisteredCallsign());
    ui->hrdlogUploadCodeEdit->setText(HRDLogBase::getUploadCode());
    ui->hrdlogOnAirCheckBox->setChecked(HRDLogBase::getOnAirEnabled());

    /***********/
    /* QRZ.COM */
    /***********/
    ui->qrzApiKeyEdit->setText(QRZBase::getLogbookAPIKey(QRZBase::getInternalAPIUsername()));
    generateQRZAPICallsignTable();

    /***********/
    /* Wavelog */
    /***********/
    ui->wavelogAddQSOEndpointEdit->setText(CloudlogBase::getAPIEndpoint());
    ui->wavelogApiKeyEdit->setText(CloudlogBase::getLogbookAPIKey());

    /***********************/
    /* Others - DXCC Group */
    /***********************/
    ui->dxccConfirmedByLotwCheckBox->setChecked(LogParam::getDxccConfirmedByLotwState());
    ui->dxccConfirmedByPaperCheckBox->setChecked(LogParam::getDxccConfirmedByPaperState());
    ui->dxccConfirmedByEqslCheckBox->setChecked(LogParam::getDxccConfirmedByEqslState());

    /***************/
    /* ON4KST Chat */
    /***************/
    ui->kstUsernameEdit->setText(KSTChat::getUsername());
    ui->kstPasswordEdit->setText(KSTChat::getPasswd());

    /***********/
    /* MEMBERS */
    /***********/


    /***********/
    /* NETWORK */
    /***********/

    ui->wsjtPortSpin->setValue(WsjtxUDPReceiver::getConfigPort());
    ui->wsjtForwardEdit->setText(WsjtxUDPReceiver::getConfigForwardAddresses());
    ui->wsjtMulticastCheckbox->setChecked(WsjtxUDPReceiver::getConfigMulticastJoin());
    ui->wsjtMulticastAddressEdit->setText(WsjtxUDPReceiver::getConfigMulticastAddress());
    ui->wsjtMulticastTTLSpin->setValue(WsjtxUDPReceiver::getConfigMulticastTTL());
    ui->wsjtColorCqSpotsCheckbox->setChecked(WsjtxUDPReceiver::getConfigOutputColorCQSpot());

    ui->notifLogIDEdit->setText(LogParam::getLogID());
    ui->notifQSOEdit->setText(NetworkNotification::getNotifQSOAdiAddrs());
    ui->notifDXSpotsEdit->setText(NetworkNotification::getNotifDXSpotAddrs());
    ui->notifWSJTXCQSpotsEdit->setText(NetworkNotification::getNotifWSJTXCQSpotAddrs());
    ui->notifSpotAlertEdit->setText(NetworkNotification::getNotifSpotAlertAddrs());
    ui->notifRigEdit->setText(NetworkNotification::getNotifRigStateAddrs());

    /*******/
    /* GUI */
    /*******/
    bool timeformat24 = locale.getSettingUse24hformat();
    ui->timeFormat24RadioButton->setChecked(timeformat24);
    ui->timeFormat12RadioButton->setChecked(!timeformat24);

    bool dateSystemFormat = locale.getSettingUseSystemDateFormat();
    ui->dateFormatSystemRadioButton->setChecked(dateSystemFormat);
    ui->dateFormatCustomRadioButton->setChecked(!dateSystemFormat);
    ui->dateFormatStringEdit->setText(locale.getSettingDateFormat());

    bool unitFormatMetric =  locale.getSettingUseMetric();
    ui->unitFormatMetricRadioButton->setChecked(unitFormatMetric);
    ui->unitFormatImperialRadioButton->setChecked(!unitFormatMetric);

    /******************/
    /* END OF Reading */
    /******************/
}

void SettingsDialog::writeSettings()
{
    FCT_IDENTIFICATION;

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
    HamQTHBase::saveUsernamePassword(ui->hamQthUsernameEdit->text(),
                                 ui->hamQthPasswordEdit->text());

    QRZBase::saveUsernamePassword(ui->qrzUsernameEdit->text(),
                              ui->qrzPasswordEdit->text());

    LogParam::setPrimaryCallbook(ui->primaryCallbookCombo->itemData(ui->primaryCallbookCombo->currentIndex()).toString());
    LogParam::setSecondaryCallbook(ui->secondaryCallbookCombo->itemData(ui->secondaryCallbookCombo->currentIndex()).toString());
    LogParam::setCallbookWebLookupURL(ui->webLookupURLEdit->text());

    /********/
    /* LoTW */
    /********/

    LotwBase::saveUsernamePassword(ui->lotwUsernameEdit->text(),
                               ui->lotwPasswordEdit->text());
    LotwBase::saveTQSLPath(ui->tqslPathEdit->text());

    /***********/
    /* ClubLog */
    /***********/
    ClubLogBase::saveUsernamePassword(ui->clublogEmailEdit->text(),
                                  ui->clublogPasswordEdit->text());

    ClubLogBase::saveUploadImmediatelyConfig(ui->clublogUploadImmediatelyCheckbox->isChecked());

    /********/
    /* eQSL */
    /********/

    EQSLBase::saveUsernamePassword(ui->eqslUsernameEdit->text(),
                               ui->eqslPasswordEdit->text());

    /**********/
    /* HRDLog */
    /**********/
    HRDLogBase::saveUploadCode(ui->hrdlogCallsignEdit->text(),
                           ui->hrdlogUploadCodeEdit->text());
    HRDLogBase::saveOnAirEnabled(ui->hrdlogOnAirCheckBox->isChecked());

    /***********/
    /* QRZ.COM */
    /***********/
    QRZBase::saveLogbookAPIKey(ui->qrzApiKeyEdit->text(), QRZBase::getInternalAPIUsername());
    saveQRZAPICallsignTable();

    /***********/
    /* Wavelog */
    /***********/
    CloudlogBase::setAPIEndpoint(ui->wavelogAddQSOEndpointEdit->text());
    CloudlogBase::saveLogbookAPIKey(ui->wavelogApiKeyEdit->text());

    /***********************/
    /* Others - DXCC Group */
    /***********************/
    LogParam::setDxccConfirmedByLotwState(ui->dxccConfirmedByLotwCheckBox->isChecked());
    LogParam::setDxccConfirmedByPaperState(ui->dxccConfirmedByPaperCheckBox->isChecked());
    LogParam::setDxccConfirmedByEqslState(ui->dxccConfirmedByEqslCheckBox->isChecked());

    /***************/
    /* ON4KST Chat */
    /***************/
    KSTChat::saveUsernamePassword(ui->kstUsernameEdit->text(),
                                  ui->kstPasswordEdit->text());

    /***********/
    /* MEMBERS */
    /***********/

    QStringList enabledLists;

    for ( QCheckBox* item: static_cast<const QList<QCheckBox *>&>(memberListCheckBoxes) )
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
    WsjtxUDPReceiver::saveConfigPort(ui->wsjtPortSpin->value());
    WsjtxUDPReceiver::saveConfigForwardAddresses(ui->wsjtForwardEdit->text());
    WsjtxUDPReceiver::saveConfigMulticastJoin(ui->wsjtMulticastCheckbox->isChecked());
    WsjtxUDPReceiver::saveConfigMulticastAddress(ui->wsjtMulticastAddressEdit->text());
    WsjtxUDPReceiver::saveConfigMulticastTTL(ui->wsjtMulticastTTLSpin->value());
    WsjtxUDPReceiver::saveConfigOutputColorCQSpot(ui->wsjtColorCqSpotsCheckbox->isChecked());
    NetworkNotification::saveNotifQSOAdiAddrs(ui->notifQSOEdit->text());
    NetworkNotification::saveNotifDXSpotAddrs(ui->notifDXSpotsEdit->text());
    NetworkNotification::saveNotifWSJTXCQSpotAddrs(ui->notifWSJTXCQSpotsEdit->text());
    NetworkNotification::saveNotifSpotAlertAddrs(ui->notifSpotAlertEdit->text());
    NetworkNotification::saveNotifRigStateAddrs(ui->notifRigEdit->text());

    /*******/
    /* GUI */
    /*******/
    locale.setSettingUse24hformat(ui->timeFormat24RadioButton->isChecked());

    bool systemDateChecked = ui->dateFormatSystemRadioButton->isChecked();
    locale.setSettingUseSystemDateFormat(systemDateChecked);
    if ( !systemDateChecked )
        locale.setSettingDateFormat(ui->dateFormatStringEdit->text());

    locale.setSettingUseMetric(ui->unitFormatMetricRadioButton->isChecked());
}

/* this function is called when user modify rig progile
 * there may be situations where hamlib change the cap
 * for rig and it is necessary to change the settings of the rig.
 * This feature does it */
void SettingsDialog::setUIBasedOnRigCaps(const RigCaps &caps)
{
    FCT_IDENTIFICATION;

    if ( !caps.canGetFreq ) disableCapCheckbox(ui->rigGetFreqCheckBox);
    if ( !caps.canGetMode ) disableCapCheckbox(ui->rigGetModeCheckBox);
    if ( !caps.canGetVFO ) disableCapCheckbox(ui->rigGetVFOCheckBox);
    if ( !caps.canGetPWR )disableCapCheckbox(ui->rigGetPWRCheckBox);
    if ( !caps.canGetRIT )disableCapCheckbox(ui->rigGetRITCheckBox);
    if ( !caps.canGetXIT )disableCapCheckbox(ui->rigGetXITCheckBox);
    if ( !caps.canGetPTT )disableCapCheckbox(ui->rigGetPTTStateCheckBox);
    if ( !ui->rigGetFreqCheckBox->isChecked() )disableCapCheckbox(ui->rigQSYWipingCheckBox);
    if ( !caps.canGetKeySpeed )
    {
        disableCapCheckbox(ui->rigGetKeySpeedCheckBox);
        disableCapCheckbox(ui->rigKeySpeedSyncCheckBox);
    }
    if ( !caps.canProcessDXSpot )disableCapCheckbox(ui->rigDXSpots2RigCheckBox);
    if ( !caps.canGetSplit )disableCapCheckbox(ui->rigGetSplitCheckBox);
    if ( ui->rigAssignedCWKeyCombo->currentText() != EMPTY_CWKEY_PROFILE )
    {
        CWKeyProfile selectedKeyProfile;
        selectedKeyProfile = cwKeyProfManager->getProfile(ui->rigAssignedCWKeyCombo->currentText());
        if ( ! caps.canGetKeySpeed
             || (selectedKeyProfile.model == CWKey::MORSEOVERCAT) )
        {
            ui->rigKeySpeedSyncCheckBox->setChecked(false);
            ui->rigKeySpeedSyncCheckBox->setEnabled(false);
        }
    }
    if ( !caps.isCIVAddrSupported )ui->rigCIVAddrSpinBox->setValue(CIVADDR_DISABLED_VALUE);

    ui->rigCIVAddrLabel->setVisible(caps.isCIVAddrSupported);
    ui->rigCIVAddrSpinBox->setVisible(caps.isCIVAddrSupported);

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

    QStringListModel* model = static_cast<QStringListModel*>(ui->rigAssignedCWKeyCombo->model());
    if ( model ) model->setStringList(approvedCWProfiles);

    ui->rigAssignedCWKeyCombo->setCurrentText(cwKeyName);
}

void SettingsDialog::setValidationResultColor(QLineEdit *editBox)
{
    FCT_IDENTIFICATION;

    QPalette p;

    if ( ! editBox )
        return;

    p.setColor( QPalette::Text, ( ! editBox->hasAcceptableInput() ) ? Qt::red : qApp->palette().text().color());
    editBox->setPalette(p);
}

void SettingsDialog::generateMembershipCheckboxes()
{
    FCT_IDENTIFICATION;

    QSqlTableModel membershipDirectoryModel;
    membershipDirectoryModel.setTable("membership_directory");
    membershipDirectoryModel.setSort(0, Qt::AscendingOrder);
    membershipDirectoryModel.select();
    QStringList currentlyEnabledLists = MembershipQE::getEnabledClubLists();

    for ( int i = 0 ; i < membershipDirectoryModel.rowCount(); i++ )
    {
        QCheckBox *columnCheckbox = new QCheckBox(this);

        const QString shortDesc = membershipDirectoryModel.data(membershipDirectoryModel.index(i, membershipDirectoryModel.fieldIndex("short_desc"))).toString();
        const QString longDesc = membershipDirectoryModel.data(membershipDirectoryModel.index(i, membershipDirectoryModel.fieldIndex("long_desc"))).toString();
        const QString records = membershipDirectoryModel.data(membershipDirectoryModel.index(i, membershipDirectoryModel.fieldIndex("num_records"))).toString();

        columnCheckbox->setText(shortDesc);
        columnCheckbox->setToolTip(longDesc + QString(" (" + tr("members") + ": %1)").arg(records));
        columnCheckbox->setChecked(currentlyEnabledLists.contains(shortDesc));
        memberListCheckBoxes.append(columnCheckbox);
        ui->clubListGrig->addWidget(columnCheckbox, i / 6, i % 6);
    }

    if ( memberListCheckBoxes.isEmpty() )
        ui->clubListGrig->addWidget(new QLabel(tr("Required internet connection during application start")));
}

void SettingsDialog::generateQRZAPICallsignTable()
{
    FCT_IDENTIFICATION;

    QStandardItemModel* tableModel = new QStandardItemModel(ui->qrzCallsignApiKeyTableView);
    tableModel->setHorizontalHeaderLabels({tr("Callsign"), tr("API Key")});

    const QStringList &addlCallsign = QRZBase::getLogbookAPIAddlCallsigns();

    for ( const QString &callsign : addlCallsign )
    {
        QList<QStandardItem*> rowItems({new QStandardItem(callsign),
                                        new QStandardItem(QRZBase::getLogbookAPIKey(callsign))});
        tableModel->appendRow(rowItems);
    }

    ui->qrzCallsignApiKeyTableView->setModel(tableModel);
    ui->qrzCallsignApiKeyTableView->resizeColumnsToContents();
    ui->qrzCallsignApiKeyTableView->setItemDelegateForColumn(0, new UpperCaseUniqueDelegate(this));
    ui->qrzCallsignApiKeyTableView->setItemDelegateForColumn(1, new PasswordDelegate(this));
}

void SettingsDialog::saveQRZAPICallsignTable()
{
    FCT_IDENTIFICATION;

    const QStringList &addlCallsignsAPIList = LogParam::getQRZCOMAPICallsignsList();
    for ( const QString &callsign : addlCallsignsAPIList)
    {
        qCDebug(runtime) << "Deleting QRZ Callsign API" << callsign;
        QRZBase::saveLogbookAPIKey({}, callsign); // side-effect - an empty key causes deleting its old value in the secure store.
    }

    // delete original list of callsigns
    QRZBase::setLogbookAPIAddlCallsigns({});

    QAbstractItemModel *model = ui->qrzCallsignApiKeyTableView->model();
    QStringList newAddlCallsignsAPIList;
    for ( int row = 0; row < model->rowCount(); ++row )
    {
        const QString &newCallsign = model->data(model->index(row, 0)).toString();
        if ( !newCallsign.isEmpty() )
        {
            qCDebug(runtime) << "Saving a new QRZ callsign API" << newCallsign;
            const QString &newPassword = model->data(model->index(row, 1)).toString();
            if ( !newPassword.isEmpty() ) newAddlCallsignsAPIList.append(newCallsign);
            QRZBase::saveLogbookAPIKey(newPassword, newCallsign);
        }
    }
    if ( !newAddlCallsignsAPIList.isEmpty() )
        QRZBase::setLogbookAPIAddlCallsigns(newAddlCallsignsAPIList);
}

void SettingsDialog::updateCountyCompleter(int dxcc)
{
    FCT_IDENTIFICATION;

    ui->stationCountyEdit->setCompleter(nullptr);
    delete countyCompleter;
    countyCompleter = nullptr;
    if ( dxcc == 0 )
        return;
    countyCompleter = Data::createCountyCompleter(dxcc, this);
    ui->stationCountyEdit->setCompleter(countyCompleter);
}

SettingsDialog::~SettingsDialog() {
    FCT_IDENTIFICATION;

    sotaCompleter->deleteLater();
    iotaCompleter->deleteLater();
    sigCompleter->deleteLater();
    if ( countyCompleter )
        countyCompleter->deleteLater();
    delete ui;
}
