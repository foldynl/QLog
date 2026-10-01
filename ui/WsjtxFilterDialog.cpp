#include <QMessageBox>

#include "WsjtxFilterDialog.h"
#include "ui_WsjtxFilterDialog.h"
#include "core/debug.h"
#include "data/Dxcc.h"
#include "core/MembershipQE.h"
#include "data/Gridsquare.h"
#include "core/LogParam.h"

MODULE_IDENTIFICATION("qlog.ui.wsjtxfilterdialog");

WsjtxFilterDialog::WsjtxFilterDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WsjtxFilterDialog)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    QString unit;
    Gridsquare::distance2localeUnitDistance(0, unit, locale);
    ui->distanceSpinBox->setSuffix(" " + unit);

    ui->logStatusWidget->setCondition(LogParam::getWsjtxFilterLogStatus());

    /************************/
    /* Continent Checkboxes */
    /************************/
    const QString &contregexp = LogParam::getWsjtxFilterContRE();

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
    ui->distanceSpinBox->setValue(LogParam::getWsjtxFilterDistance());

    /********/
    /* SNR  */
    /********/
    ui->snrSpinBox->setValue(LogParam::getWsjtxFilterSNR());

    /**********/
    /* MEMBER */
    /**********/

    generateMembershipCheckboxes();
}

void WsjtxFilterDialog::accept()
{
    FCT_IDENTIFICATION;    

    if ( !LogParam::setWsjtxFilterLogStatus(ui->logStatusWidget->condition()) )
    {
        QMessageBox::critical(this, tr("QLog Error"), tr("Cannot save Log Status filter."));
        return;
    }

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
    LogParam::setWsjtxFilterContRE(contregexp);

    /*************/
    /* Distance  */
    /*************/
    LogParam::setWsjtxFilterDistance(ui->distanceSpinBox->value());

    /********/
    /* SNR  */
    /********/
    LogParam::setWsjtxFilterSNR(ui->snrSpinBox->value());

    /**********/
    /* MEMBER */
    /**********/

    QStringList memberList;

    if ( ui->memberGroupBox->isChecked() )
    {
        memberList.append("DUMMYCLUB");

        for ( QCheckBox* item: static_cast<const QList<QCheckBox *>&>(memberListCheckBoxes) )
            if ( item->isChecked() ) memberList.append(item->text());
    }
    LogParam::setWsjtxMemberlists(memberList);

    done(QDialog::Accepted);
}

void WsjtxFilterDialog::generateMembershipCheckboxes()
{
    FCT_IDENTIFICATION;

    const QStringList &currentFilter = LogParam::getWsjtxMemberlists();
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
