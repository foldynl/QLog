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

AlertRuleDetail::AlertRuleDetail(const QString &ruleName, QWidget *parent) :
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
    int row = 0;
    int band_index = 0;
    SqlListModel *bands = new SqlListModel("SELECT name FROM bands WHERE enabled = 1 ORDER BY start_freq", "Band", this);

    while ( band_index < bands->rowCount() )
    {
        for ( int i = 0; i < 6; i++ )
        {
            band_index++;

            if ( band_index >= bands->rowCount() )
                break;

            QCheckBox *bandcheckbox=new QCheckBox(this);
            QString band_name = bands->data(bands->index(band_index,0)).toString();
            QString band_object_name = "band_" + band_name;
            bandcheckbox->setText(band_name);
            bandcheckbox->setObjectName(band_object_name);
            ui->band_group->addWidget(bandcheckbox, row, i );
        }
        row++;
    }

    /****************/
    /* DX Countries */
    /****************/
    SqlListModel *countryModel = new SqlListModel("SELECT id, translate_to_locale(name) "
                                                  "FROM dxcc_entities_ad1c "
                                                  "ORDER BY 2 COLLATE LOCALEAWARE ASC;",
                                                  tr("All"), this);
    while (countryModel->canFetchMore())
    {
        countryModel->fetchMore();
    }
    ui->countryCombo->setModel(countryModel);
    ui->countryCombo->setModelColumn(1);

    /********************/
    /* Spotter Coutries */
    /********************/
    SqlListModel *countryModel2 = new SqlListModel("SELECT id, translate_to_locale(name) "
                                                  "FROM dxcc_entities_ad1c "
                                                  "ORDER BY 2 COLLATE LOCALEAWARE ASC;",
                                                  tr("All"), this);
    while (countryModel2->canFetchMore())
    {
        countryModel2->fetchMore();
    }
    ui->spotterCountryCombo->setModel(countryModel2);
    ui->spotterCountryCombo->setModelColumn(1);

    /**************/
    /* Log Status */
    /**************/
    ui->logStatusCombo->addItem("All", (DxccStatus::NewEntity
                                        | DxccStatus::NewBand
                                        | DxccStatus::NewMode
                                        | DxccStatus::NewBandMode
                                        | DxccStatus::NewSlot
                                        | DxccStatus::Worked
                                        | DxccStatus::Confirmed
                                        | DxccStatus::UnknownStatus) );
    ui->logStatusCombo->addItem(Data::statusToText(DxccStatus::NewEntity), DxccStatus::NewEntity);
    ui->logStatusCombo->addItem(Data::statusToText(DxccStatus::NewBand), DxccStatus::NewBand);
    ui->logStatusCombo->addItem(Data::statusToText(DxccStatus::NewMode), DxccStatus::NewMode);
    ui->logStatusCombo->addItem(Data::statusToText(DxccStatus::NewBandMode), DxccStatus::NewBandMode);
    ui->logStatusCombo->addItem(Data::statusToText(DxccStatus::NewSlot), DxccStatus::NewSlot);
    ui->logStatusCombo->addItem(Data::statusToText(DxccStatus::Worked), DxccStatus::Worked);
    ui->logStatusCombo->addItem(Data::statusToText(DxccStatus::Confirmed), DxccStatus::Confirmed);

    /**************************************/
    /* Load or Prepare Rule Dialog Values */
    /**************************************/

    if ( ! ruleName.isEmpty() )
    {
        loadRule(ruleName);
    }
    else
    {
        /* get Rule name from DB to checking whether a new filter name
         * will be unique */
        QSqlQuery ruleStmt;
        if ( ! ruleStmt.prepare("SELECT rule_name FROM alert_rules ORDER BY rule_name") )
        {
            qWarning() << "Cannot prepare select statement";
        }
        else
        {
            if ( ruleStmt.exec() )
            {
                while (ruleStmt.next())
                {
                    ruleNamesList << ruleStmt.value(0).toString();
                }
            }
            else
            {
                qInfo()<< "Cannot get filters names from DB" << ruleStmt.lastError();
            }
        }
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

    if ( ui->dxcCheckBox->isChecked() )
    {
        finalSource |= SpotAlert::DXSPOT;
    }

    if ( ui->wsjtxCheckBox->isChecked() )
    {
        finalSource |= SpotAlert::WSJTXCQSPOT;
    }

    rule.sourceMap = finalSource;

    /***************
     * DX Callsign *
     ***************/
    rule.dxCallsign = ui->dxCallsignEdit->text();

    /**************
     * DX Country *
     **************/
    int row = ui->countryCombo->currentIndex();
    QVariant data;

    if ( row == 0 )
    {
        data = 0; //all
    }
    else
    {
        QModelIndex idx = ui->countryCombo->model()->index(row,0);
        data = ui->countryCombo->model()->data(idx);
    }

    rule.dxCountry = data.toInt();

    /*************
     * DX Member *
     *************/
    QStringList dxMember;

    if ( ui->memberGroupBox->isChecked() )
    {
        for ( QCheckBox* item: static_cast<const QList<QCheckBox*>&>(memberListCheckBoxes) )
        {
            if ( item->isChecked() )
            {
                dxMember.append(QString("%1").arg(item->text()));
            }
        }
    }
    else
    {
        dxMember.append("*");
    }

    rule.dxMember = dxMember;

    /*****************
     * DX Log Status *
     *****************/

    rule.dxLogStatusMap = ui->logStatusCombo->currentData().toInt();

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
        if ( ui->ft8checkbox->isChecked() ) modeRE.append("|" + BandPlan::MODE_GROUP_STRING_FT8);
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
            if ( !item || !item->widget() )
            {
                continue;
            }
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
    int tmp = ui->spotterCountryCombo->currentIndex();
    QVariant dataSpotter;

    if ( tmp == 0 )
    {
        dataSpotter = 0; //all
    }
    else
    {
        QModelIndex idx2 = ui->spotterCountryCombo->model()->index(tmp,0);
        dataSpotter = ui->spotterCountryCombo->model()->data(idx2);
    }

    rule.spotterCountry = dataSpotter.toInt();

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

bool AlertRuleDetail::ruleExists(const QString &ruleName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << ruleName;

    return ruleNamesList.contains(ruleName);

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
        if ( rule.dxCountry == 0 )
        {
            ui->countryCombo->setCurrentIndex(0);
        }
        else
        {
            QModelIndexList countryIndex = ui->countryCombo->model()->match(ui->countryCombo->model()->index(0,0),
                                                                            Qt::DisplayRole,
                                                                            rule.dxCountry,
                                                                            1, Qt::MatchExactly);

            if ( countryIndex.size() >= 1 )
            {
                ui->countryCombo->setCurrentIndex(countryIndex.at(0).row());
            }
        }

        /*************
         * DX Member *
         *************/

        generateMembershipCheckboxes(&rule);

        if ( rule.dxMember == QStringList("*") )
        {
            ui->memberGroupBox->setChecked(false);
        }
        else
        {
            ui->memberGroupBox->setChecked(true);
        }

        /*****************
         * DX Log Status *
         *****************/
        int statusIdx = ui->logStatusCombo->findData(rule.dxLogStatusMap);

        if ( statusIdx != -1 )
        {
            ui->logStatusCombo->setCurrentIndex(statusIdx);
        }
        else
        {
            ui->logStatusCombo->setCurrentIndex(0);
        }

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
            ui->ft8checkbox->setChecked(modeRE.contains("|" + BandPlan::MODE_GROUP_STRING_FT8));
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
                if ( !item || !item->widget() )
                {
                    continue;
                }
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
        if ( rule.spotterCountry == 0 )
        {
            ui->spotterCountryCombo->setCurrentIndex(0);
        }
        else
        {
            QModelIndexList countryIndex = ui->spotterCountryCombo->model()->match(ui->spotterCountryCombo->model()->index(0,0),
                                                                                   Qt::DisplayRole,
                                                                                   rule.spotterCountry,
                                                                                   1, Qt::MatchExactly);

            if ( countryIndex.size() >= 1 )
            {
                ui->spotterCountryCombo->setCurrentIndex(countryIndex.at(0).row());
            }
        }

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
    {
        qCDebug(runtime) << "Cannot load rule " << ruleName;
    }
}

void AlertRuleDetail::generateMembershipCheckboxes(const AlertRule * rule)
{
    FCT_IDENTIFICATION;

    QStringList enabledLists = MembershipQE::getEnabledClubLists();

    for ( int i = 0 ; i < enabledLists.size(); i++)
    {
        QCheckBox *columnCheckbox = new QCheckBox(this);

        QString shortDesc = enabledLists.at(i);

        columnCheckbox->setText(shortDesc);
        if ( rule ) columnCheckbox->setChecked(rule->dxMember.contains(shortDesc));
        memberListCheckBoxes.append(columnCheckbox);
    }

    if ( memberListCheckBoxes.size() == 0 )
    {
        ui->dxMemberGrid->addWidget(new QLabel(tr("No Club List is enabled"), this));
    }
    else
    {
        int elementIndex = 0;

        for ( QCheckBox* item: static_cast<const QList<QCheckBox*>&>(memberListCheckBoxes) )
        {
            ui->dxMemberGrid->addWidget(item, elementIndex / 6, elementIndex % 6);
            elementIndex++;
        }
    }
}
