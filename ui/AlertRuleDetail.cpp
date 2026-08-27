#include <QCheckBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QMessageBox>

#include "AlertRuleDetail.h"
#include "ui_AlertRuleDetail.h"
#include "core/debug.h"
#include "../models/SqlListModel.h"
#include "data/Data.h"
#include "data/SpotAlert.h"
#include "data/BandPlan.h"

MODULE_IDENTIFICATION("qlog.ui.alerruledetail");

AlertRuleDetail::AlertRuleDetail(const QString &ruleName, QWidget *parent, bool clone) :
    QDialog(parent),
    ui(new Ui::AlertRuleDetail),
    ruleName(ruleName)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->cqzEdit->setValidator(new QIntValidator(Data::getCQZMin(), Data::getCQZMax(), ui->cqzEdit));
    ui->ituEdit->setValidator(new QIntValidator(Data::getITUZMin(), Data::getITUZMax(), ui->ituEdit));

    /*************/
    /* Get Bands */
    /*************/
    const QList<Band> bands = BandPlan::bandsList(false, true);
    int i = 0;
    for ( const Band &enabledBand : bands )
    {
        const QString &bandName = enabledBand.name;
        QCheckBox *bandCheckbox = new QCheckBox(ui->band_group->parentWidget());
        bandCheckbox->setText(bandName);
        bandCheckbox->setObjectName("band_" + bandName);

        int row = i / MAXCOLUMNS;
        int column = i % MAXCOLUMNS;
        ui->band_group->addWidget(bandCheckbox, row, column);
        i++;
    }

    /****************/
    /* DX Countries */
    /****************/
    const QLatin1String countryStmt("SELECT id, translate_to_locale(name) "
                                    "FROM dxcc_entities_ad1c "
                                    "ORDER BY 2 COLLATE LOCALEAWARE ASC;");

    SqlListModel *countryModel = new SqlListModel(countryStmt, tr("All"), ui->countryCombo);
    while (countryModel->canFetchMore()) countryModel->fetchMore();
    ui->countryCombo->setModel(countryModel);
    ui->countryCombo->setModelColumn(1);
    ui->countryCombo->adjustMaxSize();

    /********************/
    /* Spotter Coutries */
    /********************/
    SqlListModel *countryModel2 = new SqlListModel(countryStmt, tr("All"), ui->spotterCountryCombo);
    while (countryModel2->canFetchMore()) countryModel2->fetchMore();
    ui->spotterCountryCombo->setModel(countryModel2);
    ui->spotterCountryCombo->setModelColumn(1);    
    ui->spotterCountryCombo->adjustMaxSize();

    /**************************************/
    /* Load or Prepare Rule Dialog Values */
    /**************************************/
    if ( ! ruleName.isEmpty() )
    {
        if ( clone )
            loadRuleNames();

        loadRule(ruleName, !clone);

        if ( clone )
        {
            ui->ruleNameEdit->clear();
            ui->ruleNameEdit->setPlaceholderText(tr("Enter a new name"));
            ui->ruleNameEdit->setFocus();
        }
    }
    else
    {
        /* get Rule name from DB to checking whether a new filter name
         * will be unique */
        loadRuleNames();
        setDefaultValues();
        generateMembershipCheckboxes();
    }
}

AlertRuleDetail::~AlertRuleDetail()
{
    FCT_IDENTIFICATION;
    delete ui;
}

void AlertRuleDetail::save()
{
    FCT_IDENTIFICATION;


    if ( ui->ruleNameEdit->text().isEmpty() )
    {
        ui->ruleNameEdit->setPlaceholderText(tr("Must not be empty"));
        return;
    }

    if ( ruleExists(ui->ruleNameEdit->text()) )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Info"),
                              QMessageBox::tr("Rule name is already exists."));
        return;
    }

    QRegularExpression rxCall(ui->dxCallsignEdit->text());
    QRegularExpression rxComm(ui->spotCommentEdit->text());

    if ( !rxCall.isValid() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Info"),
                              QMessageBox::tr("Callsign Regular Expression is incorrect."));
        return;
    }

    if ( !rxComm.isValid() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Info"),
                              QMessageBox::tr("Comment Regular Expression is incorrect."));
        return;
    }

    AlertRule rule;
    /*************
     * Rule Name *
     *************/
    rule.ruleName = ui->ruleNameEdit->text();

    /***********
     * Enabled *
     ***********/
    rule.enabled = ui->ruleEnabledCheckBox->isChecked();

    /**********
     * Source *
     **********/
    int finalSource = 0;
    finalSource |= (ui->dxcCheckBox->isChecked()   ? SpotAlert::DXSPOT      : 0)
                |  (ui->wsjtxCheckBox->isChecked() ? SpotAlert::WSJTXCQSPOT : 0);

    rule.sourceMap = finalSource;

    /***************
     * DX Callsign *
     ***************/
    rule.dxCallsign = ui->dxCallsignEdit->text();

    /**************
     * DX Country *
     **************/
    bool OK = false;
    int countryCode = ui->countryCombo->currentValue(1).toInt(&OK);
    rule.dxCountry = ( OK && countryCode > 0 ) ? countryCode : 0; // 0 = all

    /*************
     * DX Member *
     *************/
    QStringList dxMember;

    if ( ui->memberGroupBox->isChecked() )
    {
        for ( QCheckBox* item : static_cast<const QList<QCheckBox*>&>(memberListCheckBoxes) )
            if ( item->isChecked() ) dxMember.append(QString("%1").arg(item->text()));
    }
    else
        dxMember.append("*");

    rule.dxMember = dxMember;

    /*****************
     * DX Log Status *
     *****************/
    int status = 0;
    if ( ui->newEntityCheckbox->isChecked() ) status |=  DxccStatus::NewEntity;
    if ( ui->newBandCheckbox->isChecked() )   status |=  DxccStatus::NewBand;
    if ( ui->newModeCheckbox->isChecked() )   status |=  DxccStatus::NewMode;
    if ( ui->newSlotCheckbox->isChecked() )   status |=  DxccStatus::NewSlot;
    if ( ui->workedCheckbox->isChecked() )    status |=  DxccStatus::Worked;
    if ( ui->confirmedCheckbox->isChecked() ) status |=  DxccStatus::Confirmed;
    if ( ui->allCheckbox->isChecked() )       status = DxccStatus::All;
    rule.dxLogStatusMap = status;

    /****************
     * DX Continent *
     ****************/
    QString continentRE("*");

    if ( ui->continent->isChecked() )
    {
        continentRE = "NOTHING";

        if ( ui->afcheckbox->isChecked() ) continentRE.append("|AF");
        if ( ui->ancheckbox->isChecked() ) continentRE.append("|AN");
        if ( ui->ascheckbox->isChecked() ) continentRE.append("|AS");
        if ( ui->eucheckbox->isChecked() ) continentRE.append("|EU");
        if ( ui->nacheckbox->isChecked() ) continentRE.append("|NA");
        if ( ui->occheckbox->isChecked() ) continentRE.append("|OC");
        if ( ui->sacheckbox->isChecked() ) continentRE.append("|SA");
    }

    rule.dxContinent = continentRE;

    /*******************
     * Spotter Comment *
     *******************/
    rule.dxComment = ui->spotCommentEdit->text();

    /********
     * Mode *
     ********/
    QString modeRE("*");

    if ( ui->modes->isChecked() )
    {
        modeRE = "NOTHING";
        if ( ui->cwcheckbox->isChecked() ) modeRE.append("|" + BandPlan::MODE_GROUP_STRING_CW);
        if ( ui->phonecheckbox->isChecked() ) modeRE.append("|" + BandPlan::MODE_GROUP_STRING_PHONE);
        if ( ui->digitalcheckbox->isChecked() ) modeRE.append("|" + BandPlan::MODE_GROUP_STRING_DIGITAL);
        if ( ui->ftxcheckbox->isChecked() ) modeRE.append("|" + BandPlan::MODE_GROUP_STRING_FTx);
    }

    rule.mode = modeRE;

    /********
     * band *
     ********/
    QString bandRE("*");

    if ( ui->bands->isChecked() )
    {
        bandRE = "NOTHING";

        for ( int i = 0; i < ui->band_group->count(); i++)
        {
            QLayoutItem *item = ui->band_group->itemAt(i);
            if ( !item || !item->widget() ) continue;
            QCheckBox *bandcheckbox = qobject_cast<QCheckBox*>(item->widget());

            if ( bandcheckbox )
            {
                if ( bandcheckbox->isChecked() )
                {
                    //NOTHING|20m|40m
                    bandRE.append("|" + bandcheckbox->objectName().split("_").at(1));
                }
            }
        }
    }

    rule.band = bandRE;

    /*******************
     * Spotter Country *
     *******************/
    OK = false;
    int countryCodeSpotter = ui->spotterCountryCombo->currentValue(1).toInt(&OK);
    rule.spotterCountry = ( OK && countryCodeSpotter > 0 ) ? countryCodeSpotter : 0; // 0 = all

    /*********************
     * Spotter Continent *
     *********************/
    QString spotterContinentRE("*");

    if ( ui->continent_spotter->isChecked() )
    {
        spotterContinentRE = "NOTHING" ;

        if ( ui->afcheckbox_spotter->isChecked() ) spotterContinentRE.append("|AF");
        if ( ui->ancheckbox_spotter->isChecked() ) spotterContinentRE.append("|AN");
        if ( ui->ascheckbox_spotter->isChecked() ) spotterContinentRE.append("|AS");
        if ( ui->eucheckbox_spotter->isChecked() ) spotterContinentRE.append("|EU");
        if ( ui->nacheckbox_spotter->isChecked() ) spotterContinentRE.append("|NA");
        if ( ui->occheckbox_spotter->isChecked() ) spotterContinentRE.append("|OC");
        if ( ui->sacheckbox_spotter->isChecked() ) spotterContinentRE.append("|SA");
    }

    rule.spotterContinent = spotterContinentRE;

    /************
     * CQ Zones
     ***********/
    rule.cqz = (ui->cqzEdit->text().isEmpty() ? 0 : ui->cqzEdit->text().toInt());

    /************
     * ITU Zones
     ***********/
    rule.ituz = (ui->ituEdit->text().isEmpty() ? 0 : ui->ituEdit->text().toInt());

    /***********
     * POTA
     **********/
    rule.pota = ui->potaCheckbox->isChecked();

    /***********
     * SOTA
     **********/
    rule.sota = ui->sotaCheckbox->isChecked();

    /***********
     * IOTA
     **********/
    rule.iota = ui->iotaCheckbox->isChecked();

    /***********
     * WWFF
     **********/
    rule.wwff = ui->wwffCheckbox->isChecked();

    qCDebug(runtime) << rule;

    if ( ! rule.save() )
    {
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                              QMessageBox::tr("Cannot Update Alert Rules"));
        return;
    }

    accept();
}

void AlertRuleDetail::ruleNameChanged(const QString &newRuleName)
{
    FCT_IDENTIFICATION;

    QPalette p;

    p.setColor(QPalette::Text, ( ruleExists(newRuleName) ) ? Qt::red
                                                           : qApp->palette().text().color());
    ui->ruleNameEdit->setPalette(p);
}

void AlertRuleDetail::callsignChanged(const QString &enteredRE)
{
    FCT_IDENTIFICATION;

    QPalette p;

    QRegularExpression rx(enteredRE);

    p.setColor(QPalette::Text, ( !rx.isValid() ) ? Qt::red : qApp->palette().text().color());
    ui->dxCallsignEdit->setPalette(p);
}

void AlertRuleDetail::spotCommentChanged(const QString &enteredRE)
{
    FCT_IDENTIFICATION;

    QPalette p;

    QRegularExpression rx(enteredRE);

    p.setColor(QPalette::Text, ( !rx.isValid() ) ? Qt::red : qApp->palette().text().color());
    ui->spotCommentEdit->setPalette(p);
}

void AlertRuleDetail::enabledLogStatusAll(bool enabled)
{
    FCT_IDENTIFICATION;

    if ( enabled )
    {
        ui->newEntityCheckbox->setChecked(enabled);
        ui->newBandCheckbox->setChecked(enabled);
        ui->newModeCheckbox->setChecked(enabled);
        ui->newSlotCheckbox->setChecked(enabled);
        ui->workedCheckbox->setChecked(enabled);
        ui->confirmedCheckbox->setChecked(enabled);
    }

    ui->newEntityCheckbox->setEnabled(!enabled);
    ui->newBandCheckbox->setEnabled(!enabled);
    ui->newModeCheckbox->setEnabled(!enabled);
    ui->newSlotCheckbox->setEnabled(!enabled);
    ui->workedCheckbox->setEnabled(!enabled);
    ui->confirmedCheckbox->setEnabled(!enabled);
}

void AlertRuleDetail::setDefaultValues()
{
    FCT_IDENTIFICATION;

    ui->allCheckbox->setChecked(true);
    ui->countryCombo->setCurrentValue(ALLCOUNTRYIDX, 1);
}

bool AlertRuleDetail::ruleExists(const QString &ruleName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << ruleName;

    return ruleNamesList.contains(ruleName);
}

void AlertRuleDetail::loadRuleNames()
{
    FCT_IDENTIFICATION;

    QSqlQuery ruleStmt;
    if ( !ruleStmt.prepare("SELECT rule_name FROM alert_rules ORDER BY rule_name") )
    {
        qWarning() << "Cannot prepare select statement";
    }
    else if ( ruleStmt.exec() )
    {
        while ( ruleStmt.next() )
            ruleNamesList << ruleStmt.value(0).toString();
    }
    else
    {
        qWarning() << "Cannot get alert rule names from DB" << ruleStmt.lastError();
    }
}

void AlertRuleDetail::loadRule(const QString &ruleName, bool lockName)
{

    FCT_IDENTIFICATION;

    ui->ruleNameEdit->setText(ruleName);
    ui->ruleNameEdit->setEnabled(!lockName);

    AlertRule rule;

    if ( rule.load(ruleName) )
    {
        /***********
         * Enabled *
         ***********/
        ui->ruleEnabledCheckBox->setChecked(rule.enabled);

        /**********
         * Source *
         **********/
        ui->dxcCheckBox->setChecked((rule.sourceMap & SpotAlert::DXSPOT));
        ui->wsjtxCheckBox->setChecked((rule.sourceMap & SpotAlert::WSJTXCQSPOT));

        /***************
         * DX Callsign *
         ***************/
        ui->dxCallsignEdit->setText(rule.dxCallsign);

        /**************
         * DX Country *
         **************/
        ui->countryCombo->setCurrentValue(rule.dxCountry, 1);

        /*************
         * DX Member *
         *************/
        generateMembershipCheckboxes(&rule);
        const bool isDefaultAny = (rule.dxMember.size() == 1 && rule.dxMember.front() == "*");
        ui->memberGroupBox->setChecked(!isDefaultAny);

        /*****************
         * DX Log Status *
         *****************/
        uint statusSetting = rule.dxLogStatusMap;
        ui->allCheckbox->setChecked(statusSetting == DxccStatus::All);
        ui->newEntityCheckbox->setChecked(statusSetting & DxccStatus::NewEntity);
        ui->newBandCheckbox->setChecked(statusSetting & DxccStatus::NewBand);
        ui->newModeCheckbox->setChecked(statusSetting & DxccStatus::NewMode);
        ui->newSlotCheckbox->setChecked(statusSetting & DxccStatus::NewSlot);
        ui->workedCheckbox->setChecked(statusSetting & DxccStatus::Worked);
        ui->confirmedCheckbox->setChecked(statusSetting & DxccStatus::Confirmed);

        /*************
         * Continent *
         *************/
        QString continentRE = rule.dxContinent;

        if ( continentRE == "*" )
        {
            ui->continent->setChecked(false);
        }
        else
        {
            ui->continent->setChecked(true);

            ui->afcheckbox->setChecked(continentRE.contains("|AF"));
            ui->ancheckbox->setChecked(continentRE.contains("|AN"));
            ui->ascheckbox->setChecked(continentRE.contains("|AS"));
            ui->eucheckbox->setChecked(continentRE.contains("|EU"));
            ui->nacheckbox->setChecked(continentRE.contains("|NA"));
            ui->occheckbox->setChecked(continentRE.contains("|OC"));
            ui->sacheckbox->setChecked(continentRE.contains("|SA"));
        }

        /*******************
         * Spotter Comment *
         *******************/
        ui->spotCommentEdit->setText(rule.dxComment);

        /********
         * Mode *
         ********/
        QString modeRE = rule.mode;

        if ( modeRE == "*" )
        {
            ui->modes->setChecked(false);
        }
        else
        {
            ui->modes->setChecked(true);

            ui->cwcheckbox->setChecked(modeRE.contains("|" + BandPlan::MODE_GROUP_STRING_CW));
            ui->phonecheckbox->setChecked(modeRE.contains("|" + BandPlan::MODE_GROUP_STRING_PHONE));
            ui->digitalcheckbox->setChecked(modeRE.contains("|" + BandPlan::MODE_GROUP_STRING_DIGITAL));
            ui->ftxcheckbox->setChecked(modeRE.contains("|" + BandPlan::MODE_GROUP_STRING_FTx));
        }

        /********
         * band *
         ********/
        QString bandRE = rule.band;

        if ( bandRE == "*" )
        {
            ui->bands->setChecked(false);
        }
        else
        {
            ui->bands->setChecked(true);

            for ( int i = 0; i < ui->band_group->count(); i++)
            {
                QLayoutItem *item = ui->band_group->itemAt(i);
                if ( !item || !item->widget() ) continue;
                QCheckBox *bandcheckbox = qobject_cast<QCheckBox*>(item->widget());

                if (bandcheckbox)
                {
                    // object name: ex. band_20m
                    // rule : NOTHING|20m|40m
                    bandcheckbox->setChecked(bandRE.contains("|" + bandcheckbox->objectName().split("_").at(1)));
                }
            }
        }

        /*******************
         * Spotter Country *
         *******************/
        ui->spotterCountryCombo->setCurrentValue(rule.spotterCountry, 1);

        /*********************
         * Spotter Continent *
         *********************/
        QString spotterContinentRE = rule.spotterContinent;

        if ( spotterContinentRE == "*" )
        {
            ui->continent_spotter->setChecked(false);
        }
        else
        {
            ui->continent_spotter->setChecked(true);

            ui->afcheckbox_spotter->setChecked(spotterContinentRE.contains("|AF"));
            ui->ancheckbox_spotter->setChecked(spotterContinentRE.contains("|AN"));
            ui->ascheckbox_spotter->setChecked(spotterContinentRE.contains("|AS"));
            ui->eucheckbox_spotter->setChecked(spotterContinentRE.contains("|EU"));
            ui->nacheckbox_spotter->setChecked(spotterContinentRE.contains("|NA"));
            ui->occheckbox_spotter->setChecked(spotterContinentRE.contains("|OC"));
            ui->sacheckbox_spotter->setChecked(spotterContinentRE.contains("|SA"));
        }

        /***********
         * CQ Zones
         **********/
        ui->cqzEdit->setText(( rule.cqz != 0) ? QString::number(rule.cqz) : QString());

        /***********
         * ITU Zones
         **********/
        ui->ituEdit->setText(( rule.ituz != 0) ? QString::number(rule.ituz) : QString());

        /***********
         * POTA
         **********/
        ui->potaCheckbox->setChecked(rule.pota);

        /***********
         * SOTA
         **********/
        ui->sotaCheckbox->setChecked(rule.sota);

        /***********
         * IOTA
         **********/
        ui->iotaCheckbox->setChecked(rule.iota);

        /***********
         * WWFF
         **********/
        ui->wwffCheckbox->setChecked(rule.wwff);
    }
    else
        qCDebug(runtime) << "Cannot load rule " << ruleName;
}

void AlertRuleDetail::generateMembershipCheckboxes(const AlertRule * rule)
{
    FCT_IDENTIFICATION;

    const QStringList enabledLists = MembershipQE::getEnabledClubLists();

    for ( const QString &enabledClub : enabledLists )
    {
        QCheckBox *columnCheckbox = new QCheckBox(ui->dxMemberGrid->parentWidget());
        columnCheckbox->setText(enabledClub);
        if ( rule ) columnCheckbox->setChecked(rule->dxMember.contains(enabledClub));
        memberListCheckBoxes.append(columnCheckbox);
    }

    if ( memberListCheckBoxes.isEmpty() )
    {
        ui->dxMemberGrid->addWidget(new QLabel(tr("No Club List is enabled"), this));
    }
    else
    {
        int elementIndex = 0;

        for ( QCheckBox* item : static_cast<const QList<QCheckBox*>&>(memberListCheckBoxes) )
        {
            ui->dxMemberGrid->addWidget(item, elementIndex / MAXCOLUMNS, elementIndex % MAXCOLUMNS);
            elementIndex++;
        }
    }
}
