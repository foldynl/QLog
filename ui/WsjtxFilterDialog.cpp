#include "WsjtxFilterDialog.h"
#include "ui_WsjtxFilterDialog.h"
#include "core/debug.h"
#include "data/Dxcc.h"
#include "core/MembershipQE.h"
#include "core/Gridsquare.h"

MODULE_IDENTIFICATION("qlog.ui.wsjtxfilterdialog");

WsjtxFilterDialog::WsjtxFilterDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WsjtxFilterDialog)
{
    QSettings settings;

    ui->setupUi(this);

    QString unit;
    Gridsquare::distance2localeUnitDistance(0, unit);
    ui->distanceSpinBox->setSuffix(" " + unit);

    /*********************/
    /* Status Checkboxes */
    /*********************/
    uint statusSetting = settings.value("wsjtx/filter_dxcc_status", DxccStatus::All).toUInt();

    ui->newEntityCheckBox->setChecked(statusSetting & DxccStatus::NewEntity);
    ui->newBandCheckBox->setChecked(statusSetting & DxccStatus::NewBand);
    ui->newModeCheckBox->setChecked(statusSetting & DxccStatus::NewMode);
    ui->newSlotCheckBox->setChecked(statusSetting & DxccStatus::NewSlot);
    ui->workedCheckBox->setChecked(statusSetting & DxccStatus::Worked);
    ui->confirmedCheckBox->setChecked(statusSetting & DxccStatus::Confirmed);

    /************************/
    /* Continent Checkboxes */
    /************************/
    QString contregexp = settings.value("wsjtx/filter_cont_regexp", "NOTHING|AF|AN|AS|EU|NA|OC|SA").toString();

    ui->afCheckBox->setChecked(contregexp.contains("|AF"));
    ui->anCheckBox->setChecked(contregexp.contains("|AN"));
    ui->asCheckBox->setChecked(contregexp.contains("|AS"));
    ui->euCheckBox->setChecked(contregexp.contains("|EU"));
    ui->naCheckBox->setChecked(contregexp.contains("|NA"));
    ui->ocCheckBox->setChecked(contregexp.contains("|OC"));
    ui->saCheckBox->setChecked(contregexp.contains("|SA"));

    /*************/
    /* Distance  */
    /*************/
    ui->distanceSpinBox->setValue(settings.value("wsjtx/filter_distance", 0).toInt());

    /********/
    /* SNR  */
    /********/
    ui->snrSpinBox->setValue(settings.value("wsjtx/filter_snr", -41).toInt());

    /**********/
    /* MEMBER */
    /**********/

    generateMembershipCheckboxes();
}

void WsjtxFilterDialog::accept()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    /*********************/
    /* Status Checkboxes */
    /*********************/
    uint status = 0;

    if ( ui->newEntityCheckBox->isChecked() ) status |=  DxccStatus::NewEntity;
    if ( ui->newBandCheckBox->isChecked() ) status |=  DxccStatus::NewBand;
    if ( ui->newModeCheckBox->isChecked() ) status |=  DxccStatus::NewMode;
    if ( ui->newSlotCheckBox->isChecked() ) status |=  DxccStatus::NewSlot;
    if ( ui->workedCheckBox->isChecked() ) status |=  DxccStatus::Worked;
    if ( ui->confirmedCheckBox->isChecked() ) status |=  DxccStatus::Confirmed;

    settings.setValue("wsjtx/filter_dxcc_status", status);

    /************************/
    /* Continent Checkboxes */
    /************************/
    QString contregexp = "NOTHING";
    if ( ui->afCheckBox->isChecked() ) contregexp.append("|AF");
    if ( ui->anCheckBox->isChecked() ) contregexp.append("|AN");
    if ( ui->asCheckBox->isChecked() ) contregexp.append("|AS");
    if ( ui->euCheckBox->isChecked() ) contregexp.append("|EU");
    if ( ui->naCheckBox->isChecked() ) contregexp.append("|NA");
    if ( ui->ocCheckBox->isChecked() ) contregexp.append("|OC");
    if ( ui->saCheckBox->isChecked() ) contregexp.append("|SA");
    settings.setValue("wsjtx/filter_cont_regexp", contregexp);

    /*************/
    /* Distance  */
    /*************/
    settings.setValue("wsjtx/filter_distance", ui->distanceSpinBox->value());

    /********/
    /* SNR  */
    /********/
    settings.setValue("wsjtx/filter_snr", ui->snrSpinBox->value());

    /**********/
    /* MEMBER */
    /**********/

    QStringList memberList;

    if ( ui->memberGroupBox->isChecked() )
    {
        memberList.append("DUMMYCLUB");

        for ( QCheckBox* item: static_cast<const QList<QCheckBox *>&>(memberListCheckBoxes) )
        {
            if ( item->isChecked() )
            {
                memberList.append(QString("%1").arg(item->text()));
            }
        }
    }
    settings.setValue("wsjtx/filter_dx_member_list", memberList);

    done(QDialog::Accepted);
}

void WsjtxFilterDialog::generateMembershipCheckboxes()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    QStringList currentFilter = settings.value("wsjtx/filter_dx_member_list", QStringList()).toStringList();
    QStringList enabledLists = MembershipQE::getEnabledClubLists();

    for ( int i = 0 ; i < enabledLists.size(); i++)
    {
        QCheckBox *columnCheckbox = new QCheckBox(this);

        QString shortDesc = enabledLists.at(i);

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

        for ( QCheckBox* item: static_cast<const QList<QCheckBox *>&>(memberListCheckBoxes) )
        {
            ui->dxMemberGrid->addWidget(item, elementIndex / 3, elementIndex % 3);
            elementIndex++;
        }
    }

    ui->memberGroupBox->setChecked((currentFilter.size() != 0));
}

WsjtxFilterDialog::~WsjtxFilterDialog()
{
    delete ui;
}
