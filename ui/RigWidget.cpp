#include "RigWidget.h"
#include "ui_RigWidget.h"
#include "rig/macros.h"
#include "core/debug.h"
#include "data/Data.h"
#include "service/hrdlog/HRDLog.h"
#include "data/BandPlan.h"

MODULE_IDENTIFICATION("qlog.ui.rigwidget");

RigWidget::RigWidget(QWidget *parent) :
    QWidget(parent),
    lastSeenFreq(0.0),
    rigOnline(false),
    splitEnabled(false),
    ui(new Ui::RigWidget),
    hrdlog(new HRDLogUploader(this))
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);
    ui->freqLabel->setSelectionModeEnabled(false);
    ui->freqLabel->setDebounceEnabled(true);
    ui->freqLabel->setDebounceIntervalMs(250);
    ui->freqLabel->setLocale(QLocale::c());
    connect(ui->freqLabel, &FreqQSpinBox::debouncedValueChanged,
            this, &RigWidget::freqChanged);

    ui->txFreqLabel->setVisible(false);
    ui->txFreqLabel->setSelectionModeEnabled(false);
    ui->txFreqLabel->setDebounceEnabled(true);
    ui->txFreqLabel->setDebounceIntervalMs(250);
    ui->txFreqLabel->setLocale(QLocale::c());
    connect(ui->txFreqLabel, &FreqQSpinBox::debouncedValueChanged,
            this, &RigWidget::txFreqChanged);

    ui->splitOffButton->setVisible(false);
    connect(ui->splitOffButton, &QToolButton::clicked,
            this, []() { Rig::instance()->setSplit(false); });

    QStringListModel* rigModel = new QStringListModel(this);
    ui->rigProfilCombo->setModel(rigModel);
    ui->rigProfilCombo->setStyleSheet("QComboBox {color: red}");

    QSqlTableModel* bandComboModel = new QSqlTableModel(this);
    bandComboModel->setTable("bands");
    bandComboModel->setSort(bandComboModel->fieldIndex("start_freq"), Qt::AscendingOrder);
    ui->bandComboBox->setModel(bandComboModel);
    ui->bandComboBox->setModelColumn(bandComboModel->fieldIndex("name"));

    bandComboModel->select();

    QStringListModel* modesModel = new QStringListModel(this);
    ui->modeComboBox->setModel(modesModel);

    refreshRigProfileCombo();

    QTimer *onAirTimer = new QTimer(this);
    connect(onAirTimer, &QTimer::timeout, this, &RigWidget::sendOnAirState);
    onAirTimer->start(ONAIR_INTERVAL * 1000);
    resetRigInfo();

    rigDisconnected();
}

RigWidget::~RigWidget()
{
    FCT_IDENTIFICATION;

    hrdlog->deleteLater();
    delete ui;
}

void RigWidget::updateFrequency(VFOID vfoid, double vfoFreq, double ritFreq, double xitFreq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << vfoid << vfoFreq << ritFreq << xitFreq;

    if ( vfoid == VFO2 )
    {
        // TX VFO frequency update (only when split is active)
        ui->txFreqLabel->blockSignals(true);
        ui->txFreqLabel->setValue(vfoFreq);
        ui->txFreqLabel->blockSignals(false);
        ui->txFreqLabel->setReadOnly(true);
        return;
    }

    // VFO1 — RX frequency
    ui->freqLabel->blockSignals(true);
    ui->freqLabel->setValue(vfoFreq);
    ui->freqLabel->blockSignals(false);
    const QString &bandName = BandPlan::freq2Band(vfoFreq).name;

    if ( bandName != ui->bandComboBox->currentText() )
    {
        ui->bandComboBox->blockSignals(true);
        saveLastSeenFreq();
        ui->bandComboBox->setCurrentText(bandName);
        ui->bandComboBox->blockSignals(false);
    }
    lastSeenFreq = vfoFreq;
}

void RigWidget::updateSplit(VFOID, bool enabled)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << enabled;

    splitEnabled = enabled;
    ui->txFreqLabel->setVisible(enabled);
    ui->splitOffButton->setVisible(enabled);
}

void RigWidget::updateMode(VFOID vfoid, const QString &rawMode, const QString &mode,
                           const QString &submode, qint32)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<mode;

    Q_UNUSED(submode)
    Q_UNUSED(vfoid)

    ui->modeLabel->setText(rawMode);

    if ( mode != ui->modeComboBox->currentText() )
    {
        ui->modeComboBox->blockSignals(true);
        ui->modeComboBox->setCurrentText(rawMode);
        ui->modeComboBox->blockSignals(false);
    }

    lastSeenMode = mode;
}

void RigWidget::updatePWR(VFOID vfoid, double pwr)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<pwr;

    Q_UNUSED(vfoid)

    ui->pwrLabel->setText(QString(tr("PWR: %1W")).arg(pwr));
}

void RigWidget::updateVFO(VFOID vfoid, const QString &vfo)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<vfo;

    Q_UNUSED(vfoid)

    ui->vfoLabel->setText(vfo);
}

void RigWidget::updateXIT(VFOID, double xit)
{
    FCT_IDENTIFICATION;

    if ( xit != 0.0 )
    {
        ui->xitOffset->setVisible(true);
        QString unit;
        unsigned char decP;
        double xitDisplay = Data::MHz2UserFriendlyFreq(xit, unit, decP);
        ui->xitOffset->setText(QString("XIT: %1 %2").arg(QString::number(xitDisplay, 'f', decP),
                                                          unit));
    }
    else
    {
        ui->xitOffset->setVisible(false);
    }
}

void RigWidget::updateRIT(VFOID, double rit)
{
    FCT_IDENTIFICATION;

    if ( rit != 0.0 )
    {
        ui->ritOffset->setVisible(true);
        QString unit;
        unsigned char decP;
        double ritDisplay = Data::MHz2UserFriendlyFreq(rit, unit, decP);
        ui->ritOffset->setText(QString("RIT: %1 %2").arg(QString::number(ritDisplay, 'f', decP),
                                                         unit));
    }
    else
    {
        ui->ritOffset->setVisible(false);
    }
}

void RigWidget::updatePTT(VFOID, bool ptt)
{
    FCT_IDENTIFICATION;

    ui->pttLabel->setText(ptt ? "TX" : "RX");

    if ( ptt )
        ui->pttLabel->setStyleSheet("QLabel { font-weight: bold; color: white; border-radius: 8px; background-color: red; padding: 2px 4px; }");
    else
        ui->pttLabel->setStyleSheet("QLabel { font-weight: bold; padding: 2px 4px; }");
}

void RigWidget::bandComboChanged(const QString &newBand)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "new Band:" << newBand;

    saveLastSeenFreq();

    QSqlTableModel* bandComboModel = dynamic_cast<QSqlTableModel*>(ui->bandComboBox->model());
    QSqlRecord record = bandComboModel->record(ui->bandComboBox->currentIndex());

    double newFreq = record.value("start_freq").toDouble();

    if ( ! record.value("last_seen_freq").toString().isEmpty() )
    {
        newFreq = record.value("last_seen_freq").toDouble();
    }

    qCDebug(runtime) << "Tunning freq: " << newFreq;

    Rig::instance()->setFrequency(MHz(newFreq));
}

void RigWidget::modeComboChanged(const QString &newMode)
{
    FCT_IDENTIFICATION;

    Rig::instance()->setRawMode(newMode);
}

void RigWidget::rigProfileComboChanged(const QString &profileName)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << profileName;

    RigProfilesManager::instance()->setCurProfile1(profileName);
    refreshBandCombo();
    refreshModeCombo();
    resetRigInfo();

    ui->pttLabel->setVisible(rigOnline && RigProfilesManager::instance()->getCurProfile1().getPTTInfo);
    emit rigProfileChanged();
}

void RigWidget::refreshRigProfileCombo()
{
    ui->rigProfilCombo->blockSignals(true);

    RigProfilesManager *rigManager =  RigProfilesManager::instance();

    QStringList currProfiles = rigManager->profileNameList();
    QStringListModel* model = dynamic_cast<QStringListModel*>(ui->rigProfilCombo->model());

    model->setStringList(currProfiles);

    if ( rigManager->getCurProfile1().profileName.isEmpty()
         && currProfiles.count() > 0 )
    {
        /* changing profile from empty to something */
        ui->rigProfilCombo->setCurrentText(currProfiles.first());
        rigProfileComboChanged(currProfiles.first());
    }
    else
    {
        /* no profile change, just refresh the combo and preserve current profile */
        ui->rigProfilCombo->setCurrentText(rigManager->getCurProfile1().profileName);
    }

    updateRIT(VFO1, rigManager->getCurProfile1().ritOffset);
    updateXIT(VFO1, rigManager->getCurProfile1().xitOffset);

    ui->pttLabel->setVisible(rigOnline && RigProfilesManager::instance()->getCurProfile1().getPTTInfo);

    ui->rigProfilCombo->blockSignals(false);
    refreshBandCombo();
    refreshModeCombo();
}

void RigWidget::refreshBandCombo()
{
    FCT_IDENTIFICATION;

    QString currSelection = ui->bandComboBox->currentText();
    const RigProfile &profile = RigProfilesManager::instance()->getCurProfile1();

    ui->bandComboBox->blockSignals(true);
    QSqlTableModel *bandComboModel = dynamic_cast<QSqlTableModel*>(ui->bandComboBox->model());
    bandComboModel->setFilter(QString("enabled = 1 AND start_freq >= %1 AND end_freq <= %2").arg(profile.txFreqStart + profile.xitOffset)
                                                                                            .arg(profile.txFreqEnd + profile.xitOffset));
    bandComboModel->select();
    ui->bandComboBox->setCurrentText(currSelection);
    ui->bandComboBox->blockSignals(false);
}

void RigWidget::refreshModeCombo()
{
    FCT_IDENTIFICATION;

    QString currSelection = ui->modeComboBox->currentText();

    ui->modeComboBox->blockSignals(true);
    QStringListModel* model = dynamic_cast<QStringListModel*>(ui->modeComboBox->model());
    model->setStringList(Rig::instance()->getAvailableRawModes());
    ui->modeComboBox->setCurrentText(currSelection);
    ui->modeComboBox->blockSignals(false);
}

void RigWidget::reloadSettings()
{
    FCT_IDENTIFICATION;

    refreshRigProfileCombo();
}

void RigWidget::rigConnected()
{
    FCT_IDENTIFICATION;

    ui->rigProfilCombo->setStyleSheet("QComboBox {color: green}");
    rigOnline = true;
    ui->bandComboBox->blockSignals(true);
    ui->modeComboBox->blockSignals(true);
    ui->bandComboBox->setEnabled(true);
    ui->modeComboBox->setEnabled(true);
    ui->modeComboBox->blockSignals(false);
    ui->bandComboBox->blockSignals(false);
    ui->freqLabel->setReadOnly(false);
    ui->txFreqLabel->setReadOnly(false);
    ui->pttLabel->setVisible(RigProfilesManager::instance()->getCurProfile1().getPTTInfo);
    refreshModeCombo();
}

void RigWidget::rigDisconnected()
{
    FCT_IDENTIFICATION;

    ui->bandComboBox->blockSignals(true);
    ui->modeComboBox->blockSignals(true);

    saveLastSeenFreq();
    ui->rigProfilCombo->setStyleSheet("QComboBox {color: red}");
    rigOnline = false;
    resetRigInfo();

    ui->bandComboBox->setEnabled(false);
    ui->modeComboBox->setEnabled(false);

    ui->modeComboBox->blockSignals(false);
    ui->bandComboBox->blockSignals(false);

    ui->freqLabel->setReadOnly(true);
    ui->txFreqLabel->setReadOnly(true);
    ui->pttLabel->setVisible(false);
}

void RigWidget::bandUp()
{
    FCT_IDENTIFICATION;

    if ( !rigOnline )
        return;

    int currentIndex = ui->bandComboBox->currentIndex();

    if ( currentIndex < ui->bandComboBox->count() - 1)
        ui->bandComboBox->setCurrentIndex(currentIndex + 1);
}

void RigWidget::bandDown()
{
    FCT_IDENTIFICATION;

    if ( !rigOnline )
        return;

    int currentIndex = ui->bandComboBox->currentIndex();

    if ( currentIndex > 0 )
        ui->bandComboBox->setCurrentIndex(currentIndex - 1);
}

void RigWidget::setBand(const QString &band)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << band;

    if ( !rigOnline )
        return;

    ui->bandComboBox->setCurrentText(band);
}

void RigWidget::sendOnAirState()
{
    FCT_IDENTIFICATION;

    if ( rigOnline && HRDLogBase::getOnAirEnabled() )
    {
        hrdlog->sendOnAir(lastSeenFreq, lastSeenMode);
    }
}

void RigWidget::freqChanged(double)
{
    FCT_IDENTIFICATION;

    if ( !rigOnline ) return;

    Rig::instance()->setFrequency(MHz(ui->freqLabel->value()));
}

void RigWidget::txFreqChanged(double)
{
    FCT_IDENTIFICATION;

    if ( !rigOnline || !splitEnabled ) return;

    Rig::instance()->setFrequency(VFO2, MHz(ui->txFreqLabel->value()));
}

void RigWidget::resetRigInfo()
{
    QString empty;

    updateMode(VFO1, empty, empty, empty, 0);
    ui->pwrLabel->setText(QString(""));
    updateVFO(VFO1, empty);
    updateFrequency(VFO1, 0, 0, 0);
    updateSplit(VFO1, false);
    updateRIT(VFO1, RigProfilesManager::instance()->getCurProfile1().ritOffset);
    updateXIT(VFO1, RigProfilesManager::instance()->getCurProfile1().xitOffset);
    updatePTT(VFO1, false);
}

void RigWidget::saveLastSeenFreq()
{
    FCT_IDENTIFICATION;

    if ( rigOnline && lastSeenFreq != 0.0 )
    {
        qCDebug(runtime) << "Last Seen Freq" << lastSeenFreq;
        QSqlTableModel *bandComboModel = dynamic_cast<QSqlTableModel*>(ui->bandComboBox->model());

        QModelIndexList bandIndex = bandComboModel->match(bandComboModel->index(0,bandComboModel->fieldIndex("name")),
                                                          Qt::DisplayRole,
                                                          BandPlan::freq2Band(lastSeenFreq).name,1, Qt::MatchExactly);
        if ( bandIndex.size() > 0 )
        {
            bandComboModel->setData(bandComboModel->index(bandIndex.at(0).row(),
                                                          bandComboModel->fieldIndex("last_seen_freq")),
                                    lastSeenFreq);
            bandComboModel->submitAll();
        }
    }
}
