#include <QStringListModel>
#include <QSqlTableModel>
#include <QSqlRecord>
#include <QLineEdit>
#include <QShortcut>

#include "RigWidget.h"
#include "ui_RigWidget.h"
#include "rig/macros.h"
#include "core/debug.h"
#include "data/Data.h"
#include "core/HRDLog.h"
#include "data/BandPlan.h"

// On AIR pinging to HRDLog [in sec]
#define ONAIR_INTERVAL (1 * 60)

MODULE_IDENTIFICATION("qlog.ui.rigwidget");

RigWidget::RigWidget(QWidget *parent) :
    QWidget(parent),
    lastSeenFreq(0.0),
    rigOnline(false),
    ui(new Ui::RigWidget),
    hrdlog(new HRDLog(this))
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    QStringListModel* rigModel = new QStringListModel(this);
    ui->rigProfilCombo->setModel(rigModel);
    ui->rigProfilCombo->setStyleSheet("QComboBox {color: red}");

    QSqlTableModel* bandComboModel = new QSqlTableModel();
    bandComboModel->setTable("bands");
    bandComboModel->setSort(bandComboModel->fieldIndex("start_freq"), Qt::AscendingOrder);
    ui->bandComboBox->setModel(bandComboModel);
    ui->bandComboBox->setModelColumn(bandComboModel->fieldIndex("name"));

    bandComboModel->select();

    QStringListModel* modesModel = new QStringListModel(this);
    ui->modeComboBox->setModel(modesModel);

    refreshRigProfileCombo();
    refreshBandCombo();
    refreshModeCombo();

    Rig* rig = Rig::instance();
    connect(rig, &Rig::frequencyChanged, this, &RigWidget::updateFrequency);
    connect(rig, &Rig::modeChanged, this, &RigWidget::updateMode);
    connect(rig, &Rig::vfoChanged, this, &RigWidget::updateVFO);
    connect(rig, &Rig::powerChanged, this, &RigWidget::updatePWR);
    connect(rig, &Rig::rigConnected, this, &RigWidget::rigConnected);
    connect(rig, &Rig::rigDisconnected, this, &RigWidget::rigDisconnected);
    connect(rig, &Rig::xitChanged, this, &RigWidget::updateXIT);
    connect(rig, &Rig::ritChanged, this, &RigWidget::updateRIT);
    connect(rig, &Rig::pttChanged, this, &RigWidget::updatePTT);

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

    Q_UNUSED(vfoid)

    qCDebug(function_parameters) << vfoFreq << ritFreq << xitFreq;

    ui->freqLabel->setText(QString("%1 MHz").arg(QSTRING_FREQ(vfoFreq)));
    const QString& bandName = BandPlan::freq2Band(vfoFreq).name;

    if ( bandName != ui->bandComboBox->currentText() )
    {
        ui->bandComboBox->blockSignals(true);
        saveLastSeenFreq();
        ui->bandComboBox->setCurrentText(bandName);
        ui->bandComboBox->blockSignals(false);
    }
    lastSeenFreq = vfoFreq;
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

    ui->pttLabel->setText((ptt)? "TX" : "RX");

    if ( ptt )
    {
        ui->pttLabel->setStyleSheet("QLabel { border-radius: 16px;background-color : red; }");
    }
    else
    {
        ui->pttLabel->setStyleSheet("");
    }

}

void RigWidget::bandComboChanged(const QString &newBand)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "new Band:" << newBand;

    saveLastSeenFreq();

    QSqlTableModel* bandComboModel = dynamic_cast<QSqlTableModel*>(ui->bandComboBox->model());
    QSqlRecord record = bandComboModel->record(ui->bandComboBox->currentIndex());

    double newFreq = record.value("start_freq").toDouble();

    if ( ! record.value("last_seen_freq").isNull() )
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

    ui->pttLabel->setHidden(!RigProfilesManager::instance()->getCurProfile1().getPTTInfo);

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

    ui->pttLabel->setHidden(!RigProfilesManager::instance()->getCurProfile1().getPTTInfo);

    ui->rigProfilCombo->blockSignals(false);
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
    refreshBandCombo();
    refreshModeCombo();
}

void RigWidget::rigConnected()
{
    FCT_IDENTIFICATION;

    ui->rigProfilCombo->setStyleSheet("QComboBox {color: green}");
    rigOnline = true;
    ui->bandComboBox->setEnabled(true);
    ui->modeComboBox->setEnabled(true);
    refreshModeCombo();
}

void RigWidget::rigDisconnected()
{
    FCT_IDENTIFICATION;

    saveLastSeenFreq();
    ui->rigProfilCombo->setStyleSheet("QComboBox {color: red}");
    rigOnline = false;
    resetRigInfo();
    ui->bandComboBox->setEnabled(false);
    ui->modeComboBox->setEnabled(false);
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

void RigWidget::sendOnAirState()
{
    FCT_IDENTIFICATION;

    if ( rigOnline && hrdlog->getOnAirEnabled() )
    {
        hrdlog->sendOnAir(lastSeenFreq, lastSeenMode);
    }
}

void RigWidget::resetRigInfo()
{
    QString empty;

    updateMode(VFO1, empty, empty, empty, 0);
    ui->pwrLabel->setText(QString(""));
    updateVFO(VFO1, empty);
    updateFrequency(VFO1, 0, 0, 0);
    updateRIT(VFO1, RigProfilesManager::instance()->getCurProfile1().ritOffset);
    updateXIT(VFO1, RigProfilesManager::instance()->getCurProfile1().xitOffset);
    updatePTT(VFO1, false);
}

void RigWidget::saveLastSeenFreq()
{
    FCT_IDENTIFICATION;

    if ( rigOnline && lastSeenFreq != 0.0 )
    {
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
