#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>
#include "AwardsDialog.h"
#include "ui_AwardsDialog.h"
#include "models/SqlListModel.h"
#include "core/debug.h"
#include "core/QSOFilterManager.h"

#include "awards/AwardDXCC.h"
#include "awards/AwardITU.h"
#include "awards/AwardWAC.h"
#include "awards/AwardWAZ.h"
#include "awards/AwardWAS.h"
#include "awards/AwardWPX.h"
#include "awards/AwardIOTA.h"
#include "awards/AwardPOTAHunter.h"
#include "awards/AwardPOTAActivator.h"
#include "awards/AwardSOTA.h"
#include "awards/AwardWWFF.h"
#include "awards/AwardGridsquare.h"
#include "awards/AwardUSCounty.h"
#include "awards/AwardRDA.h"
#include "awards/AwardJapan.h"
#include "awards/AwardNZ.h"
#include "awards/AwardSpanishDME.h"
#include "awards/AwardUKD.h"

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

    m_awards = createAwards();

    ui->awardComboBox->blockSignals(true);
    for ( const AwardDefinition *award : static_cast<const QList<AwardDefinition*>>(m_awards) )
        ui->awardComboBox->addItem(award->displayName(), award->key());
    ui->awardComboBox->blockSignals(false);

    ui->userFilterComboBox->blockSignals(true);
    ui->userFilterComboBox->setModel(QSOFilterManager::QSOFilterModel(tr("No User Filter"),
                                                                      ui->userFilterComboBox));
    ui->userFilterComboBox->blockSignals(false);

    refreshTable(0);
}

AwardsDialog::~AwardsDialog()
{
    FCT_IDENTIFICATION;

    qDeleteAll(m_awards);
    delete ui;
}

void AwardsDialog::refreshTable(int)
{
    FCT_IDENTIFICATION;

    AwardDefinition *award = currentAward();
    if ( !award )
        return;

    setEntityInputEnabled(award->entityInputEnabled());
    setNotWorkedEnabled(award->notWorkedEnabled());

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
    if ( idx >= 0 && idx < m_awards.size() )
        return m_awards.at(idx);
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
        params.confirmedConditions << " eqsl_qsl_rcvd = 'Y' ";
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

void AwardsDialog::setEntityInputEnabled(bool enabled)
{
    FCT_IDENTIFICATION;

    ui->myEntityComboBox->setVisible(enabled);
    ui->myEntityLabel->setVisible(enabled);
}

void AwardsDialog::setNotWorkedEnabled(bool enabled)
{
    FCT_IDENTIFICATION;

    ui->notWorkedCheckBox->blockSignals(true);
    ui->notWorkedCheckBox->setVisible(enabled);
    ui->notWorkedLabel->setVisible(enabled);
    ui->notWorkedCheckBox->setChecked(enabled && ui->notWorkedCheckBox->isChecked());
    ui->notWorkedCheckBox->blockSignals(false);
    ui->notConfirmedCheckBox->blockSignals(true);
    ui->notConfirmedCheckBox->setVisible(enabled);
    ui->notConfirmedCheckBox->setChecked(enabled && ui->notConfirmedCheckBox->isChecked());
    ui->notConfirmedCheckBox->blockSignals(false);
}

QList<AwardDefinition*> AwardsDialog::createAwards()
{
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
        new AwardNZ(),
        new AwardSpanishDME(),
        new AwardUKD(),
    };
}
