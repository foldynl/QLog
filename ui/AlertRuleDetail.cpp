#include <QComboBox>
#include <QRadioButton>
#include <QApplication>
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

    setupLogStatus();

    /*********/
    /* Alarm */
    /*********/
    connect(ui->alarmNoneRadio, &QRadioButton::toggled, this, &AlertRuleDetail::alarmTypeChanged);
    connect(ui->alarmBellRadio, &QRadioButton::toggled, this, &AlertRuleDetail::alarmTypeChanged);
    connect(ui->alarmCommandRadio, &QRadioButton::toggled, this, &AlertRuleDetail::alarmTypeChanged);
    connect(ui->alarmTestButton, &QPushButton::clicked, this, &AlertRuleDetail::testAlarm);
    alarmTypeChanged();

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
    if ( clone || ruleName.isEmpty() )
        loadRuleNames();

    if ( ! ruleName.isEmpty() )
    {
        loadRule(ruleName);

        if ( clone )
        {
            ui->ruleNameEdit->setEnabled(true);
            ui->ruleNameEdit->clear();
            ui->ruleNameEdit->setPlaceholderText(tr("Enter a new name"));
            ui->ruleNameEdit->setFocus();
        }
    }
    else
    {
        /* get Rule name from DB to checking whether a new filter name
         * will be unique */
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
    // an unchecked Log Status box means that the log is not consulted
    if ( ui->logStatusGroupBox->isChecked() )
        rule.setLogStatus(static_cast<AlertRule::LogStatusNeed>(ui->logStatusNeedCombo->currentData().toInt()),
                          static_cast<AlertRule::LogStatusUntil>(ui->logStatusUntilCombo->currentData().toInt()));
    else
        rule.setLogStatus(AlertRule::LogStatusNeed::Any, AlertRule::LogStatusUntil::Confirmed);

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

    /*********
     * Alarm
     ********/
    rule.alarm = currentAlarmType();
    rule.alarmCommand = ui->alarmCommandEdit->text().trimmed();
    rule.alarmBackoff = ui->alarmBackoffSpin->value() * 60;

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

void AlertRuleDetail::setupLogStatus()
{
    FCT_IDENTIFICATION;

    // The Log Status reads as a sentence: "I need this <need> until it is <until>"
    QComboBox *need = ui->logStatusNeedCombo;

    need->addItem(tr("New Entity"), static_cast<int>(AlertRule::LogStatusNeed::NewEntity));
    need->addItem(tr("New Band"), static_cast<int>(AlertRule::LogStatusNeed::NewBand));
    need->addItem(tr("New Mode"), static_cast<int>(AlertRule::LogStatusNeed::NewMode));
    need->addItem(tr("New Band & Mode"), static_cast<int>(AlertRule::LogStatusNeed::NewBandMode));

    QComboBox *until = ui->logStatusUntilCombo;

    until->addItem(tr("Worked"), static_cast<int>(AlertRule::LogStatusUntil::Worked));
    until->setItemData(until->count() - 1,
                       tr("At least one QSO is in the log."),
                       Qt::ToolTipRole);

    until->addItem(tr("Confirmed"), static_cast<int>(AlertRule::LogStatusUntil::Confirmed));
    until->setItemData(until->count() - 1,
                       tr("A QSL is received: LoTW, eQSL or paper, "
                          "as selected in Settings - Sync & QSL - DXCC Status."),
                       Qt::ToolTipRole);

    connect(until, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AlertRuleDetail::updateLogStatusToolTips);

    updateLogStatusToolTips();
}

void AlertRuleDetail::updateLogStatusToolTips()
{
    FCT_IDENTIFICATION;

    // The explanations follow the "until it is" choice, so that "New Entity"
    // reads as "not yet confirmed" or "not yet worked" depending on the rule
    const bool confirmed = ui->logStatusUntilCombo->currentData().toInt()
                           == static_cast<int>(AlertRule::LogStatusUntil::Confirmed);
    const QString state = confirmed ? tr("confirmed") : tr("worked");

    QComboBox *need = ui->logStatusNeedCombo;

    need->setItemData(need->findData(static_cast<int>(AlertRule::LogStatusNeed::NewEntity)),
                      tr("The country is not yet %1 on any band, in any mode.").arg(state),
                      Qt::ToolTipRole);
    need->setItemData(need->findData(static_cast<int>(AlertRule::LogStatusNeed::NewBand)),
                      tr("The country is not yet %1 on this band, in any mode.").arg(state),
                      Qt::ToolTipRole);
    need->setItemData(need->findData(static_cast<int>(AlertRule::LogStatusNeed::NewMode)),
                      tr("The country is not yet %1 in this mode, on any band.").arg(state),
                      Qt::ToolTipRole);
    need->setItemData(need->findData(static_cast<int>(AlertRule::LogStatusNeed::NewBandMode)),
                      tr("The country is not yet %1 on this band in this mode.").arg(state),
                      Qt::ToolTipRole);
}

AlertRule::AlarmType AlertRuleDetail::currentAlarmType() const
{
    FCT_IDENTIFICATION;

    if ( ui->alarmBellRadio->isChecked() )    return AlertRule::AlarmType::Bell;
    if ( ui->alarmCommandRadio->isChecked() ) return AlertRule::AlarmType::Command;
    return AlertRule::AlarmType::None;
}

void AlertRuleDetail::alarmTypeChanged()
{
    FCT_IDENTIFICATION;

    const AlertRule::AlarmType type = currentAlarmType();
    const bool command = ( type == AlertRule::AlarmType::Command );
    const bool any = ( type != AlertRule::AlarmType::None );

    ui->alarmCommandLabel->setEnabled(command);
    ui->alarmCommandEdit->setEnabled(command);
    ui->alarmPlaceholdersLabel->setEnabled(command);
    ui->alarmBackoffLabel->setEnabled(any);
    ui->alarmBackoffPrefixLabel->setEnabled(any);
    ui->alarmBackoffSpin->setEnabled(any);
    ui->alarmBackoffSuffixLabel->setEnabled(any);
    ui->alarmTestButton->setEnabled(any);
}

void AlertRuleDetail::testAlarm()
{
    FCT_IDENTIFICATION;

    if ( currentAlarmType() == AlertRule::AlarmType::Bell )
    {
        QApplication::beep();
        return;
    }

    // a sample spot so that the placeholders show something sensible
    DxSpot sample;
    sample.callsign = QStringLiteral("6Y9A");
    sample.band = QStringLiteral("10m");
    sample.modeGroupString = BandPlan::MODE_GROUP_STRING_CW;
    sample.freq = 28.025;
    sample.dxcc.country = QStringLiteral("Jamaica");

    const QStringList commandLine = AlertRule::alarmCommandLine(ui->alarmCommandEdit->text(),
                                                                sample,
                                                                ui->ruleNameEdit->text());

    if ( commandLine.isEmpty() || !AlertEvaluator::startAlarm(commandLine) )
    {
        QMessageBox::warning(this, QMessageBox::tr("QLog Warning"),
                             tr("The command could not be started:<br><b>%1</b>")
                                 .arg(ui->alarmCommandEdit->text().toHtmlEscaped()));
    }
}

void AlertRuleDetail::setDefaultValues()
{
    FCT_IDENTIFICATION;

    ui->logStatusGroupBox->setChecked(false);
    ui->logStatusNeedCombo->setCurrentIndex(ui->logStatusNeedCombo->findData(static_cast<int>(AlertRule::LogStatusNeed::NewEntity)));
    ui->logStatusUntilCombo->setCurrentIndex(ui->logStatusUntilCombo->findData(static_cast<int>(AlertRule::LogStatusUntil::Confirmed)));
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

void AlertRuleDetail::loadRule(const QString &ruleName)
{

    FCT_IDENTIFICATION;

    ui->ruleNameEdit->setText(ruleName);
    ui->ruleNameEdit->setEnabled(false);

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
        const bool consultsLog = ( rule.logStatusNeed() != AlertRule::LogStatusNeed::Any );
        ui->logStatusGroupBox->setChecked(consultsLog);
        ui->logStatusNeedCombo->setCurrentIndex(ui->logStatusNeedCombo->findData(static_cast<int>(consultsLog ? rule.logStatusNeed()
                                                                                                              : AlertRule::LogStatusNeed::NewEntity)));
        ui->logStatusUntilCombo->setCurrentIndex(ui->logStatusUntilCombo->findData(static_cast<int>(rule.logStatusUntil())));

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

        /*********
         * Alarm
         ********/
        switch ( rule.alarm )
        {
        case AlertRule::AlarmType::Bell:    ui->alarmBellRadio->setChecked(true); break;
        case AlertRule::AlarmType::Command: ui->alarmCommandRadio->setChecked(true); break;
        default:                            ui->alarmNoneRadio->setChecked(true); break;
        }
        ui->alarmCommandEdit->setText(rule.alarmCommand);
        ui->alarmBackoffSpin->setValue(rule.alarmBackoff / 60);
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
