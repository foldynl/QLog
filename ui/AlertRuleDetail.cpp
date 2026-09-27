#include <QCheckBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QMessageBox>

#include "AlertRuleDetail.h"
#include "ui_AlertRuleDetail.h"
#include "ui/AlertRuleLogStatusDialog.h"
#include "core/debug.h"
#include "notification/DesktopNotification.h"
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
    ui->editLogStatusButton->setVisible(false);

    updatingLogStatusUi = true;
    populateLogStatusCombos();
    setLogStatusCombos(AlertDxccGoal::Confirmed, AlertDxccContext::Anywhere);
    updatingLogStatusUi = false;

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

    /****************
     * Notification *
     ****************/
    rule.notificationEnabled = ui->notificationCheckBox->isChecked();
    rule.notificationSoundEnabled = ui->notificationSoundCheckBox->isChecked();
    rule.notificationRepeatMinutes = ui->notificationRepeatSpinBox->value();

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
    rule.dxLogStatusMap = DxccStatus::All;
    rule.dxAlertGoalSet = false;
    rule.dxAlertContextSet = false;

    if ( ui->logStatusCheckBox->isChecked() )
    {
        if ( logStatusStorage == LogStatusStorage::LegacyCondition )
        {
            rule.dxLogStatusMap = legacyLogStatus;
        }
        else if ( logStatusStorage == LogStatusStorage::InvalidCondition )
        {
            rule.dxLogStatusMap = legacyLogStatus;
            rule.dxAlertGoal = loadedDxAlertGoal;
            rule.dxAlertContext = loadedDxAlertContext;
            rule.dxAlertGoalSet = loadedDxAlertGoalSet;
            rule.dxAlertContextSet = loadedDxAlertContextSet;
        }
        else
        {
            rule.dxAlertGoal = ui->dxAlertGoalCombo->currentData().toInt();
            rule.dxAlertContext = ui->dxAlertContextCombo->currentData().toInt();
            rule.dxAlertGoalSet = true;
            rule.dxAlertContextSet = true;
        }
    }

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

void AlertRuleDetail::testNotification()
{
    FCT_IDENTIFICATION;

    DesktopNotification *const notification = DesktopNotification::instance();
    if ( notification->show(DesktopNotificationCategory::ALERT,
                            tr("QLog Test Notification"),
                            tr("This is a test notification from QLog."),
                            ui->notificationSoundCheckBox->isChecked()) )
        return;

    QMessageBox::warning(this, tr("QLog Info"),
                         tr("The notification could not be sent. Check notification settings in your operating system."));
}

void AlertRuleDetail::logStatusEnabled(bool)
{
    FCT_IDENTIFICATION;

    if ( !updatingLogStatusUi )
        updateLogStatusUi();
}

void AlertRuleDetail::logStatusConditionChanged()
{
    FCT_IDENTIFICATION;

    if ( updatingLogStatusUi ) return;

    const int goal = ui->dxAlertGoalCombo->currentData().toInt();
    if ( goal == SELECTED_DXCC_STATUSES )
    {
        editLogStatuses();
        return;
    }

    if ( goal == INVALID_DXCC_CONDITION ) return;

    currentDxAlertGoal = goal;
    currentDxAlertContext = ui->dxAlertContextCombo->currentData().toInt();
    logStatusStorage = LogStatusStorage::NewCondition;
    updateLogStatusUi();
}

void AlertRuleDetail::editLogStatuses()
{
    FCT_IDENTIFICATION;

    const int status = logStatusStorage == LogStatusStorage::LegacyCondition
                       ? legacyLogStatus : 0;
    AlertRuleLogStatusDialog dialog(status, this);
    if ( dialog.exec() != QDialog::Accepted )
    {
        updateLogStatusUi();
        return;
    }

    logStatusStorage = LogStatusStorage::LegacyCondition;
    legacyLogStatus = dialog.status();
    updateLogStatusUi();
}

void AlertRuleDetail::updateLogStatusUi()
{
    FCT_IDENTIFICATION;

    const bool invalid = logStatusStorage == LogStatusStorage::InvalidCondition;
    bool selectedStatuses = false;
    AlertDxccGoal goal = static_cast<AlertDxccGoal>(currentDxAlertGoal);
    AlertDxccContext context = static_cast<AlertDxccContext>(currentDxAlertContext);

    if ( logStatusStorage == LogStatusStorage::LegacyCondition )
    {
        selectedStatuses = !legacyStatusAsNewCondition(legacyLogStatus, goal, context);
    }

    updatingLogStatusUi = true;
    int invalidIndex = ui->dxAlertGoalCombo->findData(INVALID_DXCC_CONDITION);
    if ( invalid )
    {
        if ( invalidIndex < 0 )
        {
            ui->dxAlertGoalCombo->addItem(tr("Invalid log status condition"),
                                          INVALID_DXCC_CONDITION);
            invalidIndex = ui->dxAlertGoalCombo->count() - 1;
        }
        ui->dxAlertGoalCombo->setCurrentIndex(invalidIndex);
    }
    else
    {
        if ( invalidIndex >= 0 )
            ui->dxAlertGoalCombo->removeItem(invalidIndex);

        if ( selectedStatuses )
            ui->dxAlertGoalCombo->setCurrentIndex(
                ui->dxAlertGoalCombo->findData(SELECTED_DXCC_STATUSES));
        else
            setLogStatusCombos(goal, context);
    }
    updatingLogStatusUi = false;

    const bool showContext = !invalid && !selectedStatuses;
    ui->dxAlertContextLabel->setVisible(showContext);
    ui->dxAlertContextCombo->setVisible(showContext);

    ui->editLogStatusButton->setVisible(selectedStatuses);
    ui->editLogStatusButton->setEnabled(selectedStatuses && ui->logStatusCheckBox->isChecked());
}

void AlertRuleDetail::populateLogStatusCombos()
{
    FCT_IDENTIFICATION;

    ui->dxAlertGoalCombo->addItem(tr("DXCC has not been worked"),
                                  static_cast<int>(AlertDxccGoal::Worked));
    ui->dxAlertGoalCombo->addItem(tr("DXCC has not been confirmed"),
                                  static_cast<int>(AlertDxccGoal::Confirmed));
    ui->dxAlertGoalCombo->addItem(tr("Selected DXCC statuses..."),
                                  SELECTED_DXCC_STATUSES);
    ui->dxAlertGoalCombo->setItemData(
        0, tr("Alert if no qualifying QSO exists in the log."), Qt::ToolTipRole);
    ui->dxAlertGoalCombo->setItemData(
        1,
        tr("Alert if no qualifying QSO is confirmed using the DXCC confirmation sources selected in Settings."),
        Qt::ToolTipRole);
    ui->dxAlertGoalCombo->setItemData(
        2,
        tr("Alert for any of the individually selected DXCC statuses."),
        Qt::ToolTipRole);

    ui->dxAlertContextCombo->addItem(tr("All bands and modes"),
                                     static_cast<int>(AlertDxccContext::Anywhere));
    ui->dxAlertContextCombo->addItem(tr("Spot's band, any mode"),
                                     static_cast<int>(AlertDxccContext::Band));
    ui->dxAlertContextCombo->addItem(tr("Any band, spot's mode"),
                                     static_cast<int>(AlertDxccContext::Mode));
    ui->dxAlertContextCombo->addItem(tr("Spot's band and mode"),
                                     static_cast<int>(AlertDxccContext::BandMode));
    ui->dxAlertContextCombo->setItemData(
        0, tr("Any band and any DXCC mode."), Qt::ToolTipRole);
    ui->dxAlertContextCombo->setItemData(
        1, tr("The band of the evaluated spot, regardless of mode."), Qt::ToolTipRole);
    ui->dxAlertContextCombo->setItemData(
        2, tr("The DXCC mode of the evaluated spot, regardless of band."), Qt::ToolTipRole);
    ui->dxAlertContextCombo->setItemData(
        3, tr("Both the band and DXCC mode of the evaluated spot."), Qt::ToolTipRole);
}

void AlertRuleDetail::setLogStatusCombos(AlertDxccGoal goal, AlertDxccContext context)
{
    FCT_IDENTIFICATION;

    currentDxAlertGoal = static_cast<int>(goal);
    currentDxAlertContext = static_cast<int>(context);
    ui->dxAlertGoalCombo->setCurrentIndex(
        ui->dxAlertGoalCombo->findData(currentDxAlertGoal));
    ui->dxAlertContextCombo->setCurrentIndex(
        ui->dxAlertContextCombo->findData(currentDxAlertContext));
}

bool AlertRuleDetail::legacyStatusAsNewCondition(int status,
                                                 AlertDxccGoal &goal,
                                                 AlertDxccContext &context) const
{
    FCT_IDENTIFICATION;

    goal = AlertDxccGoal::Worked;

    switch ( status )
    {
        case DxccStatus::NewEntity:
            context = AlertDxccContext::Anywhere;
            return true;
        case DxccStatus::NewEntity | DxccStatus::NewBand:
            context = AlertDxccContext::Band;
            return true;
        case DxccStatus::NewEntity | DxccStatus::NewMode:
            context = AlertDxccContext::Mode;
            return true;
        case DxccStatus::NewEntity | DxccStatus::NewBand
             | DxccStatus::NewMode | DxccStatus::NewSlot:
            context = AlertDxccContext::BandMode;
            return true;
        case DxccStatus::NewEntity | DxccStatus::NewBand
             | DxccStatus::NewMode | DxccStatus::NewSlot | DxccStatus::Worked:
            goal = AlertDxccGoal::Confirmed;
            context = AlertDxccContext::BandMode;
            return true;
        default:
            return false;
    }
}

void AlertRuleDetail::setDefaultValues()
{
    FCT_IDENTIFICATION;

    ui->notificationCheckBox->setChecked(true);
    ui->notificationSoundCheckBox->setChecked(true);
    ui->notificationRepeatSpinBox->setValue(AlertRule::DEFAULT_NOTIFICATION_REPEAT_MINUTES);
    updatingLogStatusUi = true;
    logStatusStorage = LogStatusStorage::NewCondition;
    legacyLogStatus = DxccStatus::All;
    setLogStatusCombos(AlertDxccGoal::Confirmed, AlertDxccContext::Anywhere);
    ui->logStatusCheckBox->setChecked(false);
    updatingLogStatusUi = false;
    updateLogStatusUi();
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

        /****************
         * Notification *
         ****************/
        ui->notificationCheckBox->setChecked(rule.notificationEnabled);
        ui->notificationSoundCheckBox->setChecked(rule.notificationSoundEnabled);
        ui->notificationRepeatSpinBox->setValue(rule.notificationRepeatMinutes);

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
        updatingLogStatusUi = true;
        legacyLogStatus = rule.dxLogStatusMap;
        loadedDxAlertGoal = rule.dxAlertGoal;
        loadedDxAlertContext = rule.dxAlertContext;
        loadedDxAlertGoalSet = rule.dxAlertGoalSet;
        loadedDxAlertContextSet = rule.dxAlertContextSet;

        if ( rule.hasDxAlertCondition() )
        {
            logStatusStorage = LogStatusStorage::NewCondition;
            setLogStatusCombos(static_cast<AlertDxccGoal>(rule.dxAlertGoal),
                               static_cast<AlertDxccContext>(rule.dxAlertContext));
            ui->logStatusCheckBox->setChecked(true);
        }
        else if ( rule.hasInvalidDxAlertCondition() )
        {
            logStatusStorage = LogStatusStorage::InvalidCondition;
            ui->logStatusCheckBox->setChecked(true);
        }
        else if ( rule.dxLogStatusMap == DxccStatus::All )
        {
            logStatusStorage = LogStatusStorage::NewCondition;
            setLogStatusCombos(AlertDxccGoal::Confirmed, AlertDxccContext::Anywhere);
            ui->logStatusCheckBox->setChecked(false);
        }
        else
        {
            logStatusStorage = LogStatusStorage::LegacyCondition;
            ui->logStatusCheckBox->setChecked(true);
        }

        updatingLogStatusUi = false;
        updateLogStatusUi();

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
