#include <QStringListModel>
#include <QSqlTableModel>
#include <QSqlRecord>
#include <QLineEdit>

#include "RigWidget.h"
#include "ui_RigWidget.h"
#include "core/Rig.h"
#include "core/debug.h"
#include "models/SqlListModel.h"
#include "data/Data.h"

MODULE_IDENTIFICATION("qlog.ui.rigwidget");

RigWidget::RigWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RigWidget)
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

    resetRigInfo();
}

RigWidget::~RigWidget()
{
    FCT_IDENTIFICATION;

    delete ui;
}

void RigWidget::updateFrequency(VFOID vfoid, double vfoFreq, double ritFreq, double xitFreq)
{
    FCT_IDENTIFICATION;

    Q_UNUSED(vfoid)

    qCDebug(function_parameters) << vfoFreq << ritFreq << xitFreq;

    ui->freqLabel->setText(QString("%1 MHz").arg(QSTRING_FREQ(vfoFreq)));
    if ( Data::band(vfoFreq).name != ui->bandComboBox->currentText() )
    {
        ui->bandComboBox->blockSignals(true);
        ui->bandComboBox->setCurrentText(Data::band(vfoFreq).name);
        ui->bandComboBox->blockSignals(false);
    }
}

void RigWidget::updateMode(VFOID vfoid, QString rawMode, QString mode, QString submode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<mode;

    Q_UNUSED(mode)
    Q_UNUSED(submode)
    Q_UNUSED(vfoid)

    ui->modeLabel->setText(rawMode);

    if ( mode != ui->modeComboBox->currentText() )
    {
        ui->modeComboBox->blockSignals(true);
        ui->modeComboBox->setCurrentText(rawMode);
        ui->modeComboBox->blockSignals(false);
    }
}

void RigWidget::updatePWR(VFOID vfoid, double pwr)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<pwr;

    Q_UNUSED(vfoid)

    ui->pwrLabel->setText(QString(tr("PWR: %1W")).arg(pwr));
}

void RigWidget::updateVFO(VFOID vfoid, QString vfo)
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
        ui->xitOffset->setText(QString("XIT: %1 MHz").arg(QSTRING_FREQ(xit)));
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
        ui->ritOffset->setText(QString("RIT: %1 MHz").arg(QSTRING_FREQ(rit)));
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

void RigWidget::bandComboChanged(QString newBand)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "new Band:" << newBand;

    QSqlTableModel* bandComboModel = dynamic_cast<QSqlTableModel*>(ui->bandComboBox->model());
    QSqlRecord record = bandComboModel->record(ui->bandComboBox->currentIndex());

    double newFreq;

    if ( ! Data::instance()->haveFavoriteFreq(ui->bandComboBox->currentText(),
                                              Rig::getModeFromRigMode(ui->modeComboBox->currentText()),
                                              newFreq) )
    {
        newFreq = record.value("start_freq").toDouble();
    }

    qCDebug(runtime) << "Tunning freq: " << newFreq;

    Rig::instance()->setFrequency(MHz(newFreq));
}

void RigWidget::modeComboChanged(QString newMode)
{
    FCT_IDENTIFICATION;

    Rig::instance()->setMode(newMode);
}

void RigWidget::rigProfileComboChanged(QString profileName)
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
    RigProfile profile = RigProfilesManager::instance()->getCurProfile1();

    ui->bandComboBox->blockSignals(true);
    QSqlTableModel *bandComboModel = dynamic_cast<QSqlTableModel*>(ui->bandComboBox->model());
    bandComboModel->setFilter(QString("enabled = 1 AND start_freq >= %1 AND end_freq <= %2").arg(profile.txFreqStart + profile.xitOffset)
                                                                                            .arg(profile.txFreqEnd + profile.xitOffset));
    dynamic_cast<QSqlTableModel*>(ui->bandComboBox->model())->select();
    ui->bandComboBox->setCurrentText(currSelection);
    ui->bandComboBox->blockSignals(false);
}

void RigWidget::refreshModeCombo()
{
    FCT_IDENTIFICATION;

    QString currSelection = ui->modeComboBox->currentText();

    ui->modeComboBox->blockSignals(true);
    QStringListModel* model = dynamic_cast<QStringListModel*>(ui->modeComboBox->model());
    model->setStringList(Rig::instance()->getAvailableModes());
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
    ui->bandComboBox->setEnabled(true);
    ui->modeComboBox->setEnabled(true);
}

void RigWidget::rigDisconnected()
{
    FCT_IDENTIFICATION;

    ui->rigProfilCombo->setStyleSheet("QComboBox {color: red}");
    ui->bandComboBox->setEnabled(false);
    ui->modeComboBox->setEnabled(false);
    resetRigInfo();
}

void RigWidget::resetRigInfo()
{
    QString empty;

    updateMode(VFO1, empty, empty, empty);
    ui->pwrLabel->setText(QString(""));
    updateVFO(VFO1, empty);
    updateFrequency(VFO1, 0, 0, 0);
    updateRIT(VFO1, RigProfilesManager::instance()->getCurProfile1().ritOffset);
    updateXIT(VFO1, RigProfilesManager::instance()->getCurProfile1().xitOffset);
    updatePTT(VFO1, false);
}
