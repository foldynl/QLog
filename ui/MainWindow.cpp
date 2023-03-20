#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QColor>
#include <QSpacerItem>
#include <hamlib/rig.h>
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ui/SettingsDialog.h"
#include "ui/ImportDialog.h"
#include "ui/ExportDialog.h"
#include "ui/LotwDialog.h"
#include "core/Fldigi.h"
#include "core/Rig.h"
#include "core/Rotator.h"
#include "core/CWKeyer.h"
#include "core/Wsjtx.h"
#include "core/ClubLog.h"
#include "data/Data.h"
#include "core/debug.h"
#include "ui/NewContactWidget.h"
#include "ui/QSOFilterDialog.h"
#include "ui/Eqsldialog.h"
#include "ui/ClublogDialog.h"
#include "ui/QrzDialog.h"
#include "ui/AwardsDialog.h"
#include "core/Lotw.h"
#include "core/Eqsl.h"
#include "core/QRZ.h"
#include "core/CredentialStore.h"
#include "AlertSettingDialog.h"
#include "core/PropConditions.h"

MODULE_IDENTIFICATION("qlog.ui.mainwindow");

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    stats(new StatisticsWidget),
    alertWidget(new AlertWidget)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->cwconsoleWidget->registerContactWidget(ui->newContactWidget);
    ui->rotatorWidget->registerContactWidget(ui->newContactWidget);

    QSettings settings;

    // restore the window geometry
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());

    StationProfile profile = StationProfilesManager::instance()->getCurProfile1();

    conditionsLabel = new QLabel("", ui->statusBar);
    conditionsLabel->setIndent(20);
    conditionsLabel->setToolTip(QString("<img src='%1'>").arg(QUrl::fromLocalFile(PropConditions::solarSummaryFile()).toString()));
    callsignLabel = new QLabel(profile.callsign.toLower(), ui->statusBar);
    callsignLabel->setIndent(10);
    locatorLabel = new QLabel(profile.locator.toLower(), ui->statusBar);
    operatorLabel = new QLabel(profile.operatorName, ui->statusBar);
    alertButton = new QPushButton("0", ui->statusBar);
    alertButton->setIcon(QIcon(":/icons/alert.svg"));
    alertButton->setFlat(true);
    alertButton->setFocusPolicy(Qt::NoFocus);
    QMenu *menuAlert = new QMenu(this);
    menuAlert->addAction(ui->actionShowAlerts);
    menuAlert->addAction(ui->actionClearAlerts);
    menuAlert->addSeparator();
    menuAlert->addAction(ui->actionAlert);
    menuAlert->addAction(ui->actionBeepSettingAlert);
    ui->actionBeepSettingAlert->setChecked(settings.value("alertbeep", false).toBool());
    alertButton->setMenu(menuAlert);

    alertTextButton = new QPushButton(" ", ui->statusBar);
    alertTextButton->setFlat(true);
    alertTextButton->setFocusPolicy(Qt::NoFocus);

    ui->toolBar->hide();
    ui->statusBar->addWidget(callsignLabel);
    ui->statusBar->addWidget(locatorLabel);
    ui->statusBar->addWidget(operatorLabel);
    ui->statusBar->addWidget(conditionsLabel);

    ui->statusBar->addPermanentWidget(alertTextButton);
    ui->statusBar->addPermanentWidget(alertButton);

    connect(this, &MainWindow::themeChanged, ui->bandmapWidget, &BandmapWidget::update);
    connect(this, &MainWindow::themeChanged, ui->onlineMapWidget, &OnlineMapWidget::changeTheme);
    connect(this, &MainWindow::themeChanged, stats, &StatisticsWidget::changeTheme);

    /* Dark Mode is supported only in case of Fusion Style */
    if ( QApplication::style()->objectName().compare("fusion",
                                                     Qt::CaseSensitivity::CaseInsensitive) == 0 )
    {
        darkLightModeSwith = new SwitchButton("", ui->statusBar);
        darkIconLabel = new QLabel("<html><img src=':/icons/light-dark-24px.svg'></html>",ui->statusBar);

        ui->statusBar->addPermanentWidget(darkLightModeSwith);
        ui->statusBar->addPermanentWidget(darkIconLabel);

        connect(darkLightModeSwith, &SwitchButton::stateChanged, this, &MainWindow::darkModeToggle);
        darkLightModeSwith->setChecked(settings.value("darkmode", false).toBool());
    }
    else
    {
        darkLightModeSwith = nullptr;
        darkIconLabel = nullptr;
        darkModeToggle(Qt::Unchecked);
    }

    connect(Rig::instance(), &Rig::rigErrorPresent, this, &MainWindow::rigErrorHandler);
    connect(Rig::instance(), &Rig::rigCWKeyOpenRequest, this, &MainWindow::cwKeyerConnectProfile);
    connect(Rig::instance(), &Rig::rigCWKeyCloseRequest, this, &MainWindow::cwKeyerDisconnectProfile);
    connect(Rig::instance(), &Rig::frequencyChanged, ui->onlineMapWidget, &OnlineMapWidget::setIBPBand);

    connect(Rotator::instance(), &Rotator::rotErrorPresent, this, &MainWindow::rotErrorHandler);
    connect(CWKeyer::instance(), &CWKeyer::cwKeyerError, this, &MainWindow::cwKeyerErrorHandler);
    connect(CWKeyer::instance(), &CWKeyer::cwKeyWPMChanged, ui->cwconsoleWidget, &CWConsoleWidget::setWPM);
    connect(CWKeyer::instance(), &CWKeyer::cwKeyEchoText, ui->cwconsoleWidget, &CWConsoleWidget::appendCWEchoText);

    Fldigi* fldigi = new Fldigi(this);
    connect(fldigi, &Fldigi::addContact, ui->newContactWidget, &NewContactWidget::saveExternalContact);

    Wsjtx* wsjtx = new Wsjtx(this);
    connect(wsjtx, &Wsjtx::statusReceived, ui->wsjtxWidget, &WsjtxWidget::statusReceived);
    connect(wsjtx, &Wsjtx::decodeReceived, ui->wsjtxWidget, &WsjtxWidget::decodeReceived);
    connect(wsjtx, &Wsjtx::addContact, ui->newContactWidget, &NewContactWidget::saveExternalContact);
    connect(ui->wsjtxWidget, &WsjtxWidget::CQSpot, &networknotification, &NetworkNotification::WSJTXCQSpot);
    connect(ui->wsjtxWidget, &WsjtxWidget::CQSpot, &alertEvaluator, &AlertEvaluator::WSJTXCQSpot);
    connect(ui->wsjtxWidget, &WsjtxWidget::reply, wsjtx, &Wsjtx::startReply);
    connect(this, &MainWindow::settingsChanged, wsjtx, &Wsjtx::reloadSetting);
    connect(this, &MainWindow::settingsChanged, ui->rotatorWidget, &RotatorWidget::reloadSettings);
    connect(this, &MainWindow::settingsChanged, ui->rigWidget, &RigWidget::reloadSettings);
    connect(this, &MainWindow::settingsChanged, ui->cwconsoleWidget, &CWConsoleWidget::reloadSettings);
    connect(this, &MainWindow::alertRulesChanged, &alertEvaluator, &AlertEvaluator::loadRules);
    connect(this, &MainWindow::altBackslash, Rig::instance(), &Rig::setPTT);
    connect(this, &MainWindow::manualMode, ui->newContactWidget, &NewContactWidget::setManualMode);

    //ClubLog* clublog = new ClubLog(this);

    connect(ui->logbookWidget, &LogbookWidget::logbookUpdated, stats, &StatisticsWidget::refreshGraph);
    connect(ui->logbookWidget, &LogbookWidget::contactUpdated, &networknotification, &NetworkNotification::QSOUpdated);
    connect(ui->logbookWidget, &LogbookWidget::contactDeleted, &networknotification, &NetworkNotification::QSODeleted);

    connect(ui->newContactWidget, &NewContactWidget::contactAdded, ui->logbookWidget, &LogbookWidget::updateTable);
    connect(ui->newContactWidget, &NewContactWidget::contactAdded, &networknotification, &NetworkNotification::QSOInserted);
    connect(ui->newContactWidget, &NewContactWidget::contactAdded, ui->bandmapWidget, &BandmapWidget::spotsDxccStatusRecal);
    connect(ui->newContactWidget, &NewContactWidget::contactAdded, ui->dxWidget, &DxWidget::setLastQSO);

    connect(ui->newContactWidget, &NewContactWidget::newTarget, ui->mapWidget, &MapWidget::setTarget);
    connect(ui->newContactWidget, &NewContactWidget::newTarget, ui->onlineMapWidget, &OnlineMapWidget::setTarget);
    //connect(ui->newContactWidget, &NewContactWidget::contactAdded, clublog, &ClubLog::uploadContact);
    connect(ui->newContactWidget, &NewContactWidget::filterCallsign, ui->logbookWidget, &LogbookWidget::filterCallsign);
    connect(ui->newContactWidget, &NewContactWidget::userFrequencyChanged, ui->bandmapWidget, &BandmapWidget::updateTunedFrequency);
    connect(ui->newContactWidget, &NewContactWidget::userFrequencyChanged, ui->onlineMapWidget, &OnlineMapWidget::setIBPBand);
    connect(ui->newContactWidget, &NewContactWidget::stationProfileChanged, this, &MainWindow::stationProfileChanged);
    connect(ui->newContactWidget, &NewContactWidget::stationProfileChanged, ui->rotatorWidget, &RotatorWidget::redrawMap);
    connect(ui->newContactWidget, &NewContactWidget::markQSO, ui->bandmapWidget, &BandmapWidget::addSpot);
    connect(ui->newContactWidget, &NewContactWidget::rigProfileChanged, ui->rigWidget, &RigWidget::refreshRigProfileCombo);

    connect(ui->dxWidget, &DxWidget::newFilteredSpot, ui->bandmapWidget, &BandmapWidget::addSpot);
    connect(ui->dxWidget, &DxWidget::newSpot, &networknotification, &NetworkNotification::dxSpot);
    connect(ui->dxWidget, &DxWidget::newSpot, &alertEvaluator, &AlertEvaluator::dxSpot);
    connect(ui->dxWidget, &DxWidget::newWCYSpot, &networknotification, &NetworkNotification::wcySpot);
    connect(ui->dxWidget, &DxWidget::newWWVSpot, &networknotification, &NetworkNotification::wwvSpot);
    connect(ui->dxWidget, &DxWidget::newToAllSpot, &networknotification, &NetworkNotification::toAllSpot);
    connect(ui->dxWidget, &DxWidget::tuneDx, ui->newContactWidget, &NewContactWidget::tuneDx);

    connect(&alertEvaluator, &AlertEvaluator::spotAlert, this, &MainWindow::processSpotAlert);
    connect(&alertEvaluator, &AlertEvaluator::spotAlert, &networknotification, &NetworkNotification::spotAlert);

    connect(ui->bandmapWidget, &BandmapWidget::tuneDx, ui->newContactWidget, &NewContactWidget::tuneDx);
    connect(ui->bandmapWidget, &BandmapWidget::nearestSpotFound, ui->newContactWidget, &NewContactWidget::nearestSpot);

    connect(ui->wsjtxWidget, &WsjtxWidget::showDxDetails, ui->newContactWidget, &NewContactWidget::showDx);

    connect(ui->rigWidget, &RigWidget::rigProfileChanged, ui->newContactWidget, &NewContactWidget::refreshRigProfileCombo);

    connect(alertWidget, &AlertWidget::alertsCleared, this, &MainWindow::clearAlertEvent);
    connect(alertWidget, &AlertWidget::tuneDx, ui->newContactWidget, &NewContactWidget::tuneDx);

    conditions = new PropConditions();

    connect(conditions, &PropConditions::conditionsUpdated, this, &MainWindow::conditionsUpdated);
    connect(conditions, &PropConditions::auroraMapUpdated, ui->onlineMapWidget, &OnlineMapWidget::auroraDataUpdate);
    connect(conditions, &PropConditions::mufMapUpdated, ui->onlineMapWidget, &OnlineMapWidget::mufDataUpdate);

    ui->onlineMapWidget->assignPropConditions(conditions);
    ui->newContactWidget->assignPropConditions(conditions);

    if ( StationProfilesManager::instance()->profileNameList().isEmpty() )
    {
        showSettings();
    }
    /*************/
    /* SHORTCUTs */
    /*************/
    QShortcut *shortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Backslash),
                                        this,
                                        SLOT(shortcutALTBackslash()),
                                        nullptr, Qt::ApplicationShortcut);
    shortcut->setAutoRepeat(false);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    FCT_IDENTIFICATION;

    QSettings settings;

    // save the window geometry
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    if ( stats )
    {
        stats->close();
        stats->deleteLater();
        stats = nullptr;
    }

    if ( alertWidget )
    {
        alertWidget->close();
    }

    QMainWindow::closeEvent(event);
}

/* It has to be controlled via global scope because keyReleaseEvent handles
 * only events from focused widget */
void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    FCT_IDENTIFICATION;

    if ( event->key() == Qt::Key_Backslash
         && event->modifiers() == Qt::AltModifier
         && ! event->isAutoRepeat() )
    {
        emit altBackslash(false);
    }
}

void MainWindow::rigConnect() {
    FCT_IDENTIFICATION;

    if ( ui->actionConnectRig->isChecked() )
    {
        Rig::instance()->open();
    }
    else
    {
        Rig::instance()->close();
    }
}

void MainWindow::rigErrorHandler(const QString &error, const QString &errorDetail)
{
    FCT_IDENTIFICATION;

    QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                         QMessageBox::tr("<b>Rig Error:</b> ") + error
                                         + "<p>" + tr("<b>Error Detail:</b> ") + errorDetail + "</p>");
    ui->actionConnectRig->setChecked(false);
}

void MainWindow::rotErrorHandler(const QString &error, const QString &errorDetail)
{
    FCT_IDENTIFICATION;

    QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                         QMessageBox::tr("<b>Rotator Error:</b> ") + error
                                         + "<p>" + tr("<b>Error Detail:</b> ") + errorDetail + "</p>");
    ui->actionConnectRotator->setChecked(false);
}

void MainWindow::cwKeyerErrorHandler(const QString &error, const QString &errorDetail)
{
    FCT_IDENTIFICATION;

    QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                         QMessageBox::tr("<b>CW Keyer Error:</b> ") + error
                                         + "<p>" + tr("<b>Error Detail:</b> ") + errorDetail + "</p>");
    ui->actionConnectCWKeyer->setChecked(false);
}

void MainWindow::stationProfileChanged()
{
    FCT_IDENTIFICATION;

    StationProfile profile = StationProfilesManager::instance()->getCurProfile1();

    qCDebug(runtime) << profile.callsign << " " << profile.locator << " " << profile.operatorName;

    callsignLabel->setText(profile.callsign.toLower());
    locatorLabel->setText(profile.locator.toLower());
    operatorLabel->setText(profile.operatorName);

    emit settingsChanged();
}

void MainWindow::darkModeToggle(int mode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode;

    QSettings settings;
    bool darkMode = (mode == Qt::Checked) ? true: false;
    settings.setValue("darkmode", darkMode);

    if ( mode == Qt::Checked)
    {
        setDarkMode();
    }
    else
    {
        setLightMode();
    }

    QFile style(":/res/stylesheet.css");
    style.open(QFile::ReadOnly | QIODevice::Text);
    qApp->setStyleSheet(style.readAll());
    style.close();

    emit themeChanged(darkMode);

}

void MainWindow::processSpotAlert(SpotAlert alert)
{
    FCT_IDENTIFICATION;

    alertWidget->addAlert(alert);
    alertButton->setText(QString::number(alertWidget->alertCount()));
    alertTextButton->setText(alert.ruleName.join(", ") + ": " + alert.callsign + ", " + alert.band + ", " + alert.mode);
    alertTextButton->disconnect();

    connect(alertTextButton, &QPushButton::clicked, this, [this, alert]()
    {
        ui->newContactWidget->tuneDx(alert.callsign, alert.freq);
    });

    if ( ui->actionBeepSettingAlert->isChecked() )
    {
        QApplication::beep();
    }
}

void MainWindow::clearAlertEvent()
{
    FCT_IDENTIFICATION;

    int newCount = alertWidget->alertCount();

    alertButton->setText(QString::number(newCount));

    if ( newCount == 0 )
    {
        alertTextButton->setText(" ");
        alertTextButton->disconnect();
    }
}

void MainWindow::beepSettingAlerts()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    settings.setValue("alertbeep", ui->actionBeepSettingAlert->isChecked());

    if ( ui->actionBeepSettingAlert->isChecked() )
    {
        QApplication::beep();
    }
}

void MainWindow::shortcutALTBackslash()
{
    FCT_IDENTIFICATION;

    emit altBackslash(true);
}

void MainWindow::setManualContact(bool isChecked)
{
    FCT_IDENTIFICATION;

    emit manualMode(isChecked);
}

void MainWindow::setDarkMode()
{
    FCT_IDENTIFICATION;

    QPalette darkPalette;
    QColor darkColor = QColor(45,45,45);
    QColor disabledColor = QColor(127,127,127);
    darkPalette.setColor(QPalette::Window, darkColor);
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(18,18,18));
    darkPalette.setColor(QPalette::AlternateBase, darkColor);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, disabledColor);
    darkPalette.setColor(QPalette::Button, darkColor);
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledColor);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledColor);

    qApp->setPalette(darkPalette);
}

void MainWindow::setLightMode()
{
    FCT_IDENTIFICATION;

    qApp->setPalette(this->style()->standardPalette());
}

void MainWindow::rotConnect() {
    FCT_IDENTIFICATION;

    if ( ui->actionConnectRotator->isChecked() )
    {
        Rotator::instance()->open();
    }
    else
    {
        Rotator::instance()->close();
    }

}

void MainWindow::cwKeyerConnect()
{
    FCT_IDENTIFICATION;

    if ( ui->actionConnectCWKeyer->isChecked() )
    {
        CWKeyer::instance()->open();
    }
    else
    {
        CWKeyer::instance()->close();
    }
}

void MainWindow::cwKeyerConnectProfile(QString requestedProfile)
{
    FCT_IDENTIFICATION;

    // it is a hack, maybe it will be improved in the future
    if ( requestedProfile == EMPTY_PROFILE_NAME || requestedProfile.isEmpty() )
    {
        return;
    }

    CWKeyProfile testingProfile = CWKeyProfilesManager::instance()->getProfile(requestedProfile);

    if ( testingProfile == CWKeyProfile() ) //if requested profile does not exists then no change
    {
        qWarning() << "Rig request unknown CW Key Profile";
        return;
    }
    ui->actionConnectCWKeyer->setChecked(true);
    CWKeyProfilesManager::instance()->setCurProfile1(requestedProfile);

    cwKeyerConnect();
}

void MainWindow::cwKeyerDisconnectProfile(QString requestedProfile)
{
    FCT_IDENTIFICATION;

    // it is a hack, maybe it will be improved in the future
    if ( requestedProfile == EMPTY_PROFILE_NAME || requestedProfile.isEmpty() )
    {
        return;
    }

    CWKeyProfile testingProfile = CWKeyProfilesManager::instance()->getProfile(requestedProfile);

    if ( testingProfile == CWKeyProfile() ) //if requested profile does not exists then no change
    {
        qWarning() << "Rig requests an unknown CW Key Profile";
        return;
    }

    /* checking whether the user has changed the assigned key during the work. If so, leave it connected. */
    if ( testingProfile !=  CWKeyProfilesManager::instance()->getCurProfile1() )
    {
        return;
    }
    ui->actionConnectCWKeyer->setChecked(false);

    cwKeyerConnect();
}

void MainWindow::showSettings() {
    FCT_IDENTIFICATION;

    SettingsDialog sw;
    if (sw.exec() == QDialog::Accepted) {
        rigConnect();
        rotConnect();
        stationProfileChanged();
        //Do not call settingsChange because stationProfileChanged does it
        //emit settingsChanged();
    }
}

void MainWindow::showStatistics()
{
    FCT_IDENTIFICATION;

    if ( stats )
    {
       stats->show();
    }
}

void MainWindow::importLog() {
    FCT_IDENTIFICATION;

    ImportDialog dialog;
    dialog.exec();
    ui->logbookWidget->updateTable();
}

void MainWindow::exportLog() {
    FCT_IDENTIFICATION;

    ExportDialog dialog;
    dialog.exec();
}

void MainWindow::showLotw()
{
    FCT_IDENTIFICATION;

    if ( ! Lotw::getUsername().isEmpty() )
    {
        LotwDialog dialog;
        dialog.exec();
        ui->logbookWidget->updateTable();
    }
    else
    {
        QMessageBox::warning(this, tr("QLog Warning"), tr("LoTW is not configured properly.<p> Please, use <b>Settings</b> dialog to configure it.</p>"));
    }
}

void MainWindow::showeQSL()
{
    FCT_IDENTIFICATION;

    if ( ! EQSL::getUsername().isEmpty() )
    {
        EqslDialog dialog;
        dialog.exec();
        ui->logbookWidget->updateTable();
    }
    else
    {
        QMessageBox::warning(this, tr("QLog Warning"), tr("eQSL is not configured properly.<p> Please, use <b>Settings</b> dialog to configure it.</p>"));
    }
}

void MainWindow::showClublog()
{
    FCT_IDENTIFICATION;

    if ( ! ClubLog::getEmail().isEmpty() )
    {
        ClublogDialog dialog;
        dialog.exec();
        ui->logbookWidget->updateTable();
    }
    else
    {
        QMessageBox::warning(this, tr("QLog Warning"), tr("Clublog is not configured properly.<p> Please, use <b>Settings</b> dialog to configure it.</p>"));
    }
}

void MainWindow::showQRZ()
{
    FCT_IDENTIFICATION;

    QString logbookAPIKey = QRZ::getLogbookAPIKey();

    if ( !logbookAPIKey.isEmpty() )
    {
        QRZDialog dialog;
        dialog.exec();
        ui->logbookWidget->updateTable();
    }
    else
    {
        QMessageBox::warning(this, tr("QLog Warning"), tr("QRZ.com is not configured properly.<p> Please, use <b>Settings</b> dialog to configure it.</p>"));
    }

}

void MainWindow::showAwards()
{
    FCT_IDENTIFICATION;

    AwardsDialog dialog;
    connect(&dialog, &AwardsDialog::AwardConditionSelected,
            ui->logbookWidget, &LogbookWidget::filterCountryBand);
    connect(&dialog, &AwardsDialog::finished,
            ui->logbookWidget, &LogbookWidget::restoreFilters);
    dialog.exec();
}

void MainWindow::showAbout() {
    FCT_IDENTIFICATION;

    QString aboutText = tr("<h1>QLog %1</h1>"
                           "<p>&copy; 2019 Thomas Gatzweiler DL2IC<br/>"
                           "&copy; 2021-2023 Ladislav Foldyna OK1MLG</p>"
                           "<p>Based on Qt %2<br/>"
                           "%3<br/>"
                           "%4<br/>"
                           "%5</p>"
                           "<p>Icon by <a href='http://www.iconshock.com'>Icon Shock</a><br />"
                           "Satellite images by <a href='http://www.nasa.gov'>NASA</a><br />"
                           "ZoneDetect by <a href='https://github.com/BertoldVdb/ZoneDetect'>Bertold Van den Bergh</a><br />"
                           "TimeZone Database by <a href='https://github.com/evansiroky/timezone-boundary-builder'>Evan Siroky</a>");


    QString version = QCoreApplication::applicationVersion();
    QString hamlibVersion =
#if defined(Q_OS_WIN)
            QString(rig_version());
#else
            QString(hamlib_version);
#endif

    QString OSName = QString("%1 %2").arg(QSysInfo::prettyProductName()).arg(QSysInfo::currentCpuArchitecture());
    aboutText = aboutText.arg(version)
                         .arg(qVersion())
                         .arg(hamlibVersion)
                         .arg(QSslSocket::sslLibraryVersionString())
                         .arg(OSName);


    QMessageBox::about(this, tr("About"), aboutText);
}

void MainWindow::showWikiHelp()
{
    FCT_IDENTIFICATION;

    QDesktopServices::openUrl(QString("https://github.com/foldynl/QLog/wiki"));
}

void MainWindow::showReportBug()
{
    FCT_IDENTIFICATION;
    QDesktopServices::openUrl(QString("https://github.com/foldynl/QLog/blob/master/CONTRIBUTING.md#reporting-bugs"));
}

void MainWindow::showAlerts()
{
    FCT_IDENTIFICATION;

    alertWidget->show();
}

void MainWindow::clearAlerts()
{
    FCT_IDENTIFICATION;

    alertWidget->clearAllAlerts();
}

void MainWindow::conditionsUpdated() {
    FCT_IDENTIFICATION;

    QString kcolor, fluxcolor, acolor;

    QString k_index_string, flux_string, a_index_string;

    k_index_string = flux_string = a_index_string = tr("N/A");

    /* https://3fs.net.au/making-sense-of-solar-indices/ */
    if ( conditions->isKIndexValid() )
    {
        double k_index = conditions->getKIndex();

        if (k_index < 3.5) {
            kcolor = "green";
        }
        else if (k_index < 4.5) {
            kcolor = "orange";
        }
        else {
            kcolor = "red";
        }

        k_index_string = QString::number(k_index, 'g', 2);
    }

    if ( conditions->isFluxValid() )
    {
        if ( conditions->getFlux() < 100 )
        {
            fluxcolor = "red";
        }
        else if ( conditions->getFlux() < 200 )
        {
            fluxcolor = "orange";
        }
        else
        {
            fluxcolor = "green";
        }

        flux_string = QString::number(conditions->getFlux());

    }

    if ( conditions->isAIndexValid() )
    {
        if ( conditions->getAIndex() < 27 )
        {
            acolor = "green";
        }
        else if ( conditions->getAIndex() < 48 )
        {
            acolor = "orange";
        }
        else
        {
            acolor = "red";
        }

        a_index_string = QString::number(conditions->getAIndex());
    }

    conditionsLabel->setTextFormat(Qt::RichText);
    conditionsLabel->setText(QString("SFI <b style='color: %1'>%2</b> A <b style='color: %3'>%4</b> K <b style='color: %5'>%6</b>").arg(
                                 fluxcolor, flux_string, acolor, a_index_string, kcolor, k_index_string ));
}

void MainWindow::QSOFilterSetting()
{
    FCT_IDENTIFICATION;

    QSOFilterDialog dialog(this);
    dialog.exec();
    ui->logbookWidget->updateTable();
}

void MainWindow::alertRuleSetting()
{
    FCT_IDENTIFICATION;

    AlertSettingDialog dialog(this);
    dialog.exec();
    emit alertRulesChanged();
}

MainWindow::~MainWindow() {
    FCT_IDENTIFICATION;

    CWKeyer::instance()->close();
    QThread::msleep(500);

    Rig::instance()->stopTimer();
    Rotator::instance()->stopTimer();
    CWKeyer::instance()->stopTimer();

    alertWidget->deleteLater();
    conditions->deleteLater();
    conditionsLabel->deleteLater();
    callsignLabel->deleteLater();
    locatorLabel->deleteLater();
    operatorLabel->deleteLater();
    delete ui;
}
