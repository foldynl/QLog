#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableView>
#include <QDesktopServices>
#include <QUrl>
#include "AwardsDialog.h"
#include "ui_AwardsDialog.h"
#include "models/SqlListModel.h"
#include "core/debug.h"
#include "core/LogParam.h"
#include "core/QSOFilterManager.h"

#include "awards/AwardDXCC.h"
#include "awards/AwardITU.h"
#include "awards/AwardWAC.h"
#include "awards/AwardWAZ.h"
#include "awards/AwardWAS.h"
#include "awards/AwardWPX.h"
#include "awards/AwardIOTA.h"
#include "awards/AwardPOTAHunter.h"
#include "awards/AwardPGAHunter.h"
#include "awards/AwardPOTAActivator.h"
#include "awards/AwardSOTA.h"
#include "awards/AwardWWFF.h"
#include "awards/AwardGridsquare.h"
#include "awards/AwardUSCounty.h"
#include "awards/AwardRDA.h"
#include "awards/AwardJapan.h"
#include "awards/AwardWAJA.h"
#include "awards/AwardNZ.h"
#include "awards/AwardSpanishDME.h"
#include "awards/AwardUKD.h"
#include "awards/AwardWAIP.h"
#include "awards/AwardWAAC.h"
#include "awards/AwardCanadaAward.h"
#include "awards/AwardWANA.h"
#include "awards/AwardWorkedAllRAC.h"

MODULE_IDENTIFICATION("qlog.ui.awardsdialog");

AwardsDialog::AwardsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AwardsDialog)
{
    FCT_IDENTIFICATION;
    ui->setupUi(this);

    entityCallsignModel = new SqlListModel("SELECT my_dxcc, my_country_intl || ' (' || CASE WHEN LENGTH(GROUP_CONCAT(station_callsign, ', ')) > 50 "
                                           "THEN SUBSTR(GROUP_CONCAT(station_callsign, ', '), 0, 50) || '...' ELSE GROUP_CONCAT(station_callsign, ', ') END || ')' "
                                           "FROM(SELECT DISTINCT my_dxcc, my_country_intl, station_callsign FROM contacts) GROUP BY my_dxcc ORDER BY my_dxcc;", "", this);

    ui->myEntityComboBox->blockSignals(true);
    while (entityCallsignModel->canFetchMore())
    {
        entityCallsignModel->fetchMore();
    }
    ui->myEntityComboBox->setModel(entityCallsignModel);
    ui->myEntityComboBox->setModelColumn(1);
    ui->myEntityComboBox->blockSignals(false);

    awardDefinitions = createAwards();

    ui->awardComboBox->blockSignals(true);
    const QList<AwardDefinition*> &definitions = awardDefinitions;
    for ( const AwardDefinition *award : definitions )
        ui->awardComboBox->addItem(award->displayName(), award->key());
    ui->awardComboBox->blockSignals(false);

    ui->userFilterComboBox->blockSignals(true);
    ui->userFilterComboBox->setModel(QSOFilterManager::QSOFilterModel(tr("No User Filter"),
                                                                      ui->userFilterComboBox));
    ui->userFilterComboBox->blockSignals(false);

    loadDialogState();

    connect(ui->rulesPushButton, &QPushButton::clicked, this, &AwardsDialog::openRulesUrl);
    connect(this, &QDialog::finished, this, &AwardsDialog::saveDialogState);

    refreshTableEnabled = true;
    refreshTable(0);
}

AwardsDialog::~AwardsDialog()
{
    FCT_IDENTIFICATION;

    qDeleteAll(awardDefinitions);
    delete ui;
}

void AwardsDialog::refreshTable(int)
{
    FCT_IDENTIFICATION;

    if ( !refreshTableEnabled )
        return;

    AwardDefinition *award = currentAward();
    if ( !award )
        return;

    if ( !ui->notWorkedCheckBox->isHidden() )
        notWorkedChecked = ui->notWorkedCheckBox->isChecked();
    if ( !ui->notConfirmedCheckBox->isHidden() )
        notConfirmedChecked = ui->notConfirmedCheckBox->isChecked();

    setEntityInputEnabled(award->entityInputEnabled());
    const bool confirmationEnabled = award->confirmationInputEnabled();
    setNotWorkedEnabled(award->notWorkedEnabled(), confirmationEnabled);
    ui->confirmedLabel->setVisible(confirmationEnabled);
    ui->lotwCheckBox->setVisible(confirmationEnabled);
    ui->eqslCheckBox->setVisible(confirmationEnabled);
    ui->paperCheckBox->setVisible(confirmationEnabled);
    updateRulesButton(award);

    if ( ui->eqslCheckBox->isEnabled() )
        eqslConfirmationChecked = ui->eqslCheckBox->isChecked();

    const bool acceptsEqsl = award->acceptsEqslConfirmation();
    const QSignalBlocker eqslBlocker(ui->eqslCheckBox);
    ui->eqslCheckBox->setChecked(acceptsEqsl && eqslConfirmationChecked);
    ui->eqslCheckBox->setEnabled(acceptsEqsl);
    ui->eqslCheckBox->setToolTip(acceptsEqsl
                                 ? QString()
                                 : tr("eQSL confirmations are not accepted for %1.")
                                       .arg(award->displayName()));

    if ( !award->widget() )
    {
        QWidget *w = award->createWidget(ui->stackedWidget);
        ui->stackedWidget->addWidget(w);

        const QTableView *tableView = qobject_cast<QTableView*>(w);
        if ( tableView )
        {
            connect(tableView, &QTableView::doubleClicked, this, [this](const QModelIndex &idx)
            {
                AwardDefinition::ConditionResult result = currentAward()->getConditionSelected(idx);
                if ( result.valid )
                    emit awardConditionSelected(result.country, result.band, result.filter);
            });
        }
    }

    ui->stackedWidget->setCurrentWidget(award->widget());
    award->updateData(buildFilterParams());
}

AwardDefinition* AwardsDialog::currentAward() const
{
    FCT_IDENTIFICATION;

    int idx = ui->awardComboBox->currentIndex();
    if ( idx >= 0 && idx < awardDefinitions.size() )
        return awardDefinitions.at(idx);
    return nullptr;
}

AwardFilterParams AwardsDialog::buildFilterParams() const
{
    FCT_IDENTIFICATION;

    AwardFilterParams params;

    params.entitySelected = getSelectedEntity();

    params.modes << "'NONE'";
    if ( ui->cwCheckBox->isChecked() )
        params.modes << "'CW'";
    if ( ui->phoneCheckBox->isChecked() )
        params.modes << "'PHONE'";
    if ( ui->digiCheckBox->isChecked() )
        params.modes << "'DIGITAL'";

    params.confirmedConditions << "1=2 ";
    if ( ui->eqslCheckBox->isChecked() )
    {
        params.confirmedConditions << ( currentAward()->requiresEqslAuthenticityGuaranteed()
                                            ? " (eqsl_qsl_rcvd = 'Y' AND eqsl_ag = 'Y') "
                                            : " eqsl_qsl_rcvd = 'Y' " );
    }
    if ( ui->lotwCheckBox->isChecked() )
        params.confirmedConditions << " lotw_qsl_rcvd = 'Y' ";
    if ( ui->paperCheckBox->isChecked() )
        params.confirmedConditions << " qsl_rcvd = 'Y' ";

    params.notWorkedOnly = ui->notWorkedCheckBox->isChecked();
    params.notConfirmedOnly = ui->notConfirmedCheckBox->isChecked();

    params.userFilterWhereClause = ( ui->userFilterComboBox->currentIndex() > 0 )
                                       ? "AND " + QSOFilterManager::instance()->getWhereClause(ui->userFilterComboBox->currentText())
                                       : "";
    return params;
}

QString AwardsDialog::getSelectedEntity() const
{
    FCT_IDENTIFICATION;

    int row = ui->myEntityComboBox->currentIndex();
    const QModelIndex &idx = ui->myEntityComboBox->model()->index(row,0);
    const QString comboData = ui->myEntityComboBox->model()->data(idx).toString();

    qCDebug(runtime) << comboData;

    return comboData;
}

void AwardsDialog::loadDialogState()
{
    FCT_IDENTIFICATION;

    const int awardIndex = ui->awardComboBox->findData(LogParam::getAwardsLastAward());
    ui->awardComboBox->setCurrentIndex(awardIndex >= 0 ? awardIndex : 0);

    const QString myEntity = LogParam::getAwardsLastMyEntity();
    for ( int row = 0; row < entityCallsignModel->rowCount(); row++ )
    {
        if ( entityCallsignModel->data(entityCallsignModel->index(row, 0)).toString() == myEntity )
        {
            ui->myEntityComboBox->setCurrentIndex(row);
            break;
        }
    }

    const int userFilterIndex = ui->userFilterComboBox->findText(LogParam::getAwardsLastUserFilter());
    ui->userFilterComboBox->setCurrentIndex(userFilterIndex >= 0 ? userFilterIndex : 0);

    ui->lotwCheckBox->setChecked(LogParam::getAwardsConfirmedByState("lotw"));
    eqslConfirmationChecked = LogParam::getAwardsConfirmedByState("eqsl");
    ui->eqslCheckBox->setChecked(eqslConfirmationChecked);
    ui->paperCheckBox->setChecked(LogParam::getAwardsConfirmedByState("paper"));
    ui->cwCheckBox->setChecked(LogParam::getAwardsModeState("cw"));
    ui->phoneCheckBox->setChecked(LogParam::getAwardsModeState("phone"));
    ui->digiCheckBox->setChecked(LogParam::getAwardsModeState("digital"));
    notWorkedChecked = LogParam::getAwardsShowState("notworked");
    ui->notWorkedCheckBox->setChecked(notWorkedChecked);
    notConfirmedChecked = LogParam::getAwardsShowState("notconfirmed");
    ui->notConfirmedCheckBox->setChecked(notConfirmedChecked);
}

void AwardsDialog::saveDialogState() const
{
    FCT_IDENTIFICATION;

    LogParam::setAwardsLastAward(ui->awardComboBox->currentData().toString());
    LogParam::setAwardsLastMyEntity(getSelectedEntity());
    LogParam::setAwardsLastUserFilter(ui->userFilterComboBox->currentIndex() > 0
                                         ? ui->userFilterComboBox->currentText()
                                         : QString());
    LogParam::setAwardsConfirmedByState("lotw", ui->lotwCheckBox->isChecked());
    LogParam::setAwardsConfirmedByState("eqsl", eqslConfirmationChecked);
    LogParam::setAwardsConfirmedByState("paper", ui->paperCheckBox->isChecked());
    LogParam::setAwardsModeState("cw", ui->cwCheckBox->isChecked());
    LogParam::setAwardsModeState("phone", ui->phoneCheckBox->isChecked());
    LogParam::setAwardsModeState("digital", ui->digiCheckBox->isChecked());
    LogParam::setAwardsShowState("notworked", notWorkedChecked);
    LogParam::setAwardsShowState("notconfirmed", notConfirmedChecked);
}

void AwardsDialog::openRulesUrl() const
{
    FCT_IDENTIFICATION;

    const AwardDefinition *award = currentAward();
    if ( !award )
        return;

    const QString url = award->rulesUrl();
    if ( !url.isEmpty() )
        QDesktopServices::openUrl(QUrl(url));
}

void AwardsDialog::setEntityInputEnabled(bool enabled)
{
    FCT_IDENTIFICATION;

    ui->myEntityComboBox->setVisible(enabled);
    ui->myEntityLabel->setVisible(enabled);
}

void AwardsDialog::setNotWorkedEnabled(bool enabled, bool confirmationEnabled)
{
    FCT_IDENTIFICATION;

    ui->notWorkedCheckBox->blockSignals(true);
    ui->notWorkedCheckBox->setVisible(enabled);
    ui->notWorkedLabel->setVisible(enabled);
    ui->notWorkedCheckBox->setChecked(enabled && notWorkedChecked);
    ui->notWorkedCheckBox->blockSignals(false);
    ui->notConfirmedCheckBox->blockSignals(true);
    ui->notConfirmedCheckBox->setVisible(enabled && confirmationEnabled);
    ui->notConfirmedCheckBox->setChecked(enabled && confirmationEnabled && notConfirmedChecked);
    ui->notConfirmedCheckBox->blockSignals(false);
}

void AwardsDialog::updateRulesButton(const AwardDefinition *award)
{
    FCT_IDENTIFICATION;

    const bool enabled = award && !award->rulesUrl().isEmpty();
    ui->rulesPushButton->setEnabled(enabled);
    ui->rulesPushButton->setToolTip(enabled ? award->rulesUrl() : QString());
}

QList<AwardDefinition*> AwardsDialog::createAwards()
{
    FCT_IDENTIFICATION;

    return {
        new AwardDXCC(),
        new AwardITU(),
        new AwardWAC(),
        new AwardWAZ(),
        new AwardWAS(),
        new AwardWPX(),
        new AwardIOTA(),
        new AwardPOTAHunter(),
        new AwardPOTAActivator(),
        new AwardSOTA(),
        new AwardWWFF(),
        new AwardGridsquare(2),
        new AwardGridsquare(4),
        new AwardGridsquare(6),
        new AwardUSCounty(),
        new AwardRDA(),
        new AwardJapan(),
        new AwardWAJA(),
        new AwardNZ(),
        new AwardSpanishDME(),
        new AwardUKD(),
        new AwardWAIP(),
        new AwardWAAC(),
        new AwardCanadaAward(),
        new AwardWANA(),
        new AwardWorkedAllRAC(),
        new AwardPGAHunter(),
    };
}
