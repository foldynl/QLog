#include <QDebug>
#include <QCheckBox>
#include <QSqlRecord>
#include <QLayoutItem>
#include <QLabel>

#include "DxFilterDialog.h"
#include "ui_DxFilterDialog.h"
#include "core/debug.h"
#include "data/Dxcc.h"
#include "core/MembershipQE.h"
#include "DxWidget.h"
#include "core/LogParam.h"
#include "data/BandPlan.h"

MODULE_IDENTIFICATION("qlog.ui.dxfilterdialog");

DxFilterDialog::DxFilterDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DxFilterDialog)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    const int columns = 6;
    const QList<Band> &bands = BandPlan::bandsList(false, true);
    const QStringList &excludedBandFilter = LogParam::getDXCExcludedBands();
    /********************/
    /* Bands Checkboxes */
    /********************/
    for (int i = 0; i < bands.size(); ++i)
    {
        const QString &bandName = bands.at(i).name;
        QCheckBox *bandCheckbox = new QCheckBox(this);
        bandCheckbox->setText(bandName);
        bandCheckbox->setProperty("bandName", bandName); // just to be sure that Bandmap is not translated
        bandCheckbox->setObjectName("bandCheckBox_" + bandName);
        bandCheckbox->setChecked(!excludedBandFilter.contains(bandName));

        int row = i / columns;
        int column = i % columns;
        ui->band_group->addWidget(bandCheckbox, row, column);
    }

    /*********************/
    /* Status Checkboxes */
    /*********************/
    uint statusSetting = LogParam::getDXCFilterDxccStatus();
    ui->newEntitycheckbox->setChecked(statusSetting & DxccStatus::NewEntity);
    ui->newBandcheckbox->setChecked(statusSetting & DxccStatus::NewBand);
    ui->newModecheckbox->setChecked(statusSetting & DxccStatus::NewMode);
    ui->newSlotcheckbox->setChecked(statusSetting & DxccStatus::NewSlot);
    ui->workedcheckbox->setChecked(statusSetting & DxccStatus::Worked);
    ui->confirmedcheckbox->setChecked(statusSetting & DxccStatus::Confirmed);

    /*******************/
    /* Mode Checkboxes */
    /*******************/
    const QString &moderegexp = LogParam::getDXCFilterModeRE();
    ui->cwcheckbox->setChecked(moderegexp.contains("|" + BandPlan::MODE_GROUP_STRING_CW));
    ui->phonecheckbox->setChecked(moderegexp.contains("|" + BandPlan::MODE_GROUP_STRING_PHONE));
    ui->digitalcheckbox->setChecked(moderegexp.contains("|" + BandPlan::MODE_GROUP_STRING_DIGITAL));
    ui->ft8checkbox->setChecked(moderegexp.contains("|" + BandPlan::MODE_GROUP_STRING_FT8));

    /************************/
    /* Continent Checkboxes */
    /************************/
    const QString &contregexp = LogParam::getDXCFilterContRE();
    ui->afcheckbox->setChecked(contregexp.contains("|AF"));
    ui->ancheckbox->setChecked(contregexp.contains("|AN"));
    ui->ascheckbox->setChecked(contregexp.contains("|AS"));
    ui->eucheckbox->setChecked(contregexp.contains("|EU"));
    ui->nacheckbox->setChecked(contregexp.contains("|NA"));
    ui->occheckbox->setChecked(contregexp.contains("|OC"));
    ui->sacheckbox->setChecked(contregexp.contains("|SA"));

    /********************************/
    /* Spotter Continent Checkboxes */
    /********************************/
    const QString &contregexp_spotter = LogParam::getDXCFilterSpotterContRE();
    ui->afcheckbox_spotter->setChecked(contregexp_spotter.contains("|AF"));
    ui->ancheckbox_spotter->setChecked(contregexp_spotter.contains("|AN"));
    ui->ascheckbox_spotter->setChecked(contregexp_spotter.contains("|AS"));
    ui->eucheckbox_spotter->setChecked(contregexp_spotter.contains("|EU"));
    ui->nacheckbox_spotter->setChecked(contregexp_spotter.contains("|NA"));
    ui->occheckbox_spotter->setChecked(contregexp_spotter.contains("|OC"));
    ui->sacheckbox_spotter->setChecked(contregexp_spotter.contains("|SA"));

    /*****************/
    /* Deduplication */
    /*****************/
    ui->deduplicationGroupBox->setChecked(LogParam::getDXCFilterDedup());
    ui->dedupTimeDiffSpinbox->setValue(LogParam::getDXCFilterDedupTime(DEDUPLICATION_TIME));
    ui->dedupFreqDiffSpinbox->setValue(LogParam::getDXCFilterDedupFreq(DEDUPLICATION_FREQ_TOLERANCE));

    /**********/
    /* MEMBER */
    /**********/

    generateMembershipCheckboxes();
}

void DxFilterDialog::accept()
{
    FCT_IDENTIFICATION;

    /********************/
    /* Bands Checkboxes */
    /********************/
    QStringList excludedBands;
    for ( int i = 0; i < ui->band_group->count(); i++)
    {
        QLayoutItem *item = ui->band_group->itemAt(i);
        if ( !item || !item->widget() ) continue;

        QCheckBox *bandcheckbox = qobject_cast<QCheckBox*>(item->widget());
        if ( bandcheckbox &&  !bandcheckbox->isChecked())
            excludedBands << bandcheckbox->property("bandName").toString();
    }
    LogParam::setDXCExcludedBands(excludedBands);

    /*********************/
    /* Status Checkboxes */
    /*********************/
    uint status = 0;
    if ( ui->newEntitycheckbox->isChecked() ) status |=  DxccStatus::NewEntity;
    if ( ui->newBandcheckbox->isChecked() ) status |=  DxccStatus::NewBand;
    if ( ui->newModecheckbox->isChecked() ) status |=  DxccStatus::NewMode;
    if ( ui->newSlotcheckbox->isChecked() ) status |=  DxccStatus::NewSlot;
    if ( ui->workedcheckbox->isChecked() ) status |=  DxccStatus::Worked;
    if ( ui->confirmedcheckbox->isChecked() ) status |=  DxccStatus::Confirmed;
    LogParam::setDXCFilterDxccStatus(status);

    /*******************/
    /* Mode Checkboxes */
    /*******************/
    QString moderegexp("NOTHING");
    if ( ui->cwcheckbox->isChecked() ) moderegexp.append("|" + BandPlan::MODE_GROUP_STRING_CW);
    if ( ui->phonecheckbox->isChecked() ) moderegexp.append("|" + BandPlan::MODE_GROUP_STRING_PHONE);
    if ( ui->digitalcheckbox->isChecked() ) moderegexp.append("|" + BandPlan::BandPlan::MODE_GROUP_STRING_DIGITAL);
    if ( ui->ft8checkbox->isChecked() ) moderegexp.append("|" + BandPlan::BandPlan::MODE_GROUP_STRING_FT8);
    LogParam::setDXCFilterModeRE(moderegexp);

    /************************/
    /* Continent Checkboxes */
    /************************/
    QString contregexp = "NOTHING";
    if ( ui->afcheckbox->isChecked() ) contregexp.append("|AF");
    if ( ui->ancheckbox->isChecked() ) contregexp.append("|AN");
    if ( ui->ascheckbox->isChecked() ) contregexp.append("|AS");
    if ( ui->eucheckbox->isChecked() ) contregexp.append("|EU");
    if ( ui->nacheckbox->isChecked() ) contregexp.append("|NA");
    if ( ui->occheckbox->isChecked() ) contregexp.append("|OC");
    if ( ui->sacheckbox->isChecked() ) contregexp.append("|SA");
    LogParam::setDXCFilterContRE(contregexp);

    /********************************/
    /* Spotter Continent Checkboxes */
    /********************************/
    QString contregexp_spotter = "NOTHING";
    if ( ui->afcheckbox_spotter->isChecked() ) contregexp_spotter.append("|AF");
    if ( ui->ancheckbox_spotter->isChecked() ) contregexp_spotter.append("|AN");
    if ( ui->ascheckbox_spotter->isChecked() ) contregexp_spotter.append("|AS");
    if ( ui->eucheckbox_spotter->isChecked() ) contregexp_spotter.append("|EU");
    if ( ui->nacheckbox_spotter->isChecked() ) contregexp_spotter.append("|NA");
    if ( ui->occheckbox_spotter->isChecked() ) contregexp_spotter.append("|OC");
    if ( ui->sacheckbox_spotter->isChecked() ) contregexp_spotter.append("|SA");
    LogParam::setDXCFilterSpotterContRE(contregexp_spotter);

    /*****************/
    /* Deduplication */
    /*****************/
    LogParam::setDXCFilterDedup(ui->deduplicationGroupBox->isChecked());
    LogParam::setDXCFilterDedupTime(ui->dedupTimeDiffSpinbox->value() );
    LogParam::setDXCFilterDedupFreq(ui->dedupFreqDiffSpinbox->value());

    /**********/
    /* MEMBER */
    /**********/

    QStringList memberList;

    if ( ui->memberGroupBox->isChecked() )
    {
        memberList.append("DUMMYCLUB");

        for ( QCheckBox* item: static_cast<const QList<QCheckBox*>&>(memberListCheckBoxes) )
            if ( item->isChecked() ) memberList.append(item->text());
    }
    LogParam::setDXCFilterMemberlists(memberList);

    done(QDialog::Accepted);
}

void DxFilterDialog::generateMembershipCheckboxes()
{
    FCT_IDENTIFICATION;

    const QStringList &currentFilter = LogParam::getDXCFilterMemberlists();
    const QStringList &enabledLists = MembershipQE::getEnabledClubLists();

    for ( int i = 0 ; i < enabledLists.size(); i++)
    {
        QCheckBox *columnCheckbox = new QCheckBox(this);
        const QString &shortDesc = enabledLists.at(i);

        columnCheckbox->setText(shortDesc);
        columnCheckbox->setChecked(currentFilter.contains(shortDesc));
        memberListCheckBoxes.append(columnCheckbox);
    }

    if ( memberListCheckBoxes.size() == 0 )
    {
        ui->dxMemberGrid->addWidget(new QLabel(tr("No Club List is enabled")));
    }
    else
    {
        int elementIndex = 0;

        for ( QCheckBox* item: static_cast<const QList<QCheckBox*>&>(memberListCheckBoxes) )
        {
            ui->dxMemberGrid->addWidget(item, elementIndex / 3, elementIndex % 3);
            elementIndex++;
        }
    }

    ui->memberGroupBox->setChecked(!currentFilter.isEmpty());
}

DxFilterDialog::~DxFilterDialog()
{
    FCT_IDENTIFICATION;

    delete ui;
}
