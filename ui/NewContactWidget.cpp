#include <QtSql/QtSql>
#include <QShortcut>
#include <QDesktopServices>
#include <QDebug>
#include <QCompleter>
#include <QMessageBox>
#include <QSqlField>
#include <QTimeZone>
#include <QKeyEvent>

#include "rig/Rig.h"
#include "rig/macros.h"
#include "rotator/Rotator.h"
#include "NewContactWidget.h"
#include "ui_NewContactWidget.h"
#include "core/debug.h"
#include "core/Gridsquare.h"
#include "data/StationProfile.h"
#include "core/HamQTH.h"
#include "core/QRZ.h"
#include "data/RigProfile.h"
#include "data/AntProfile.h"
#include "data/CWKeyProfile.h"
#include "data/Data.h"
#include "core/Callsign.h"
#include "core/PropConditions.h"
#include "core/MembershipQE.h"
#include "logformat/AdiFormat.h"
#include "data/MainLayoutProfile.h"
#include "models/LogbookModel.h"
#include "data/BandPlan.h"

MODULE_IDENTIFICATION("qlog.ui.newcontactwidget");

NewContactWidget::NewContactWidget(QWidget *parent) :
    QWidget(parent),
    rig(Rig::instance()),
    dxDistance(qQNaN()),
    contactTimer(new QTimer(this)),
    ui(new Ui::NewContactWidget),
    uiDynamic(new NewContactDynamicWidgets(true, this)),
    prop_cond(nullptr),
    QSOFreq(0.0),
    bandwidthFilter(BANDWIDTH_UNKNOWN),
    rigOnline(false),
    isManualEnterMode(false),
    callbookSearchPaused(false)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);
    ui->rstRcvdEdit->spaceForbidden(true);
    ui->rstSentEdit->spaceForbidden(true);

    setupCustomUi();

    CWKeyProfilesManager::instance(); //TODO remove, make it better - workaround

    ui->dateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->timeOnEdit->setDisplayFormat(locale.formatTimeLongWithoutTZ());

    /**************************/
    /* QSL Send Combo Content */
    /**************************/
    /* do no use Data::qslSentBox for it because
     * Data::qslSentBox has a different ordering.
     * Ordering below is optimized for a new Contact Widget only
     */
    ui->qslSentBox->addItem(tr("No"), QVariant("N"));
    ui->qslSentBox->addItem(tr("Yes"), QVariant("Y"));
    ui->qslSentBox->addItem(tr("Requested"), QVariant("R"));
    ui->qslSentBox->addItem(tr("Queued"), QVariant("Q"));
    ui->qslSentBox->addItem(tr("Ignored"), QVariant("I"));

    /**************************/
    /* QSL Send LoTW Combo Content */
    /**************************/
    /* do no use Data::eQSLSentBox for it because
     * Data::eQSLSentBox has a different ordering.
     * Ordering below is optimized for a new Contact Widget only
     */
    ui->eQSLSentBox->addItem(tr("No"), QVariant("N"));
    ui->eQSLSentBox->addItem(tr("Yes"), QVariant("Y"));
    ui->eQSLSentBox->addItem(tr("Requested"), QVariant("R"));
    ui->eQSLSentBox->addItem(tr("Queued"), QVariant("Q"));
    ui->eQSLSentBox->addItem(tr("Ignored"), QVariant("I"));

    /**************************/
    /* QSL Send eQSL Combo Content */
    /**************************/
    /* do no use Data::lotwQslSentBox for it because
     * Data::lotwQslSentBox has a different ordering.
     * Ordering below is optimized for a new Contact Widget only
     */
    ui->lotwQslSentBox->addItem(tr("No"), QVariant("N"));
    ui->lotwQslSentBox->addItem(tr("Yes"), QVariant("Y"));
    ui->lotwQslSentBox->addItem(tr("Requested"), QVariant("R"));
    ui->lotwQslSentBox->addItem(tr("Queued"), QVariant("Q"));
    ui->lotwQslSentBox->addItem(tr("Ignored"), QVariant("I"));

    /*****************************/
    /* QSL SendVia Combo Content */
    /*****************************/
    /* do no use Data::qslSentViaBox for it because
     * Data::qslSentViaBox has a different ordering.
     * Ordering below is optimized for a new Contact Widget only
     */
    ui->qslSentViaBox->addItem("", QVariant(""));
    ui->qslSentViaBox->addItem(tr("Bureau"), QVariant("B"));
    ui->qslSentViaBox->addItem(tr("Direct"), QVariant("D"));
    ui->qslSentViaBox->addItem(tr("Electronic"), QVariant("E"));

    /****************/
    /* Rig Profile  */
    /****************/
    QStringListModel* rigModel = new QStringListModel(this);
    ui->rigEdit->setModel(rigModel);
    refreshRigProfileCombo();

    /****************/
    /* Ant Profile  */
    /****************/
    QStringListModel* antModel = new QStringListModel(this);
    ui->antennaEdit->setModel(antModel);
    refreshAntProfileCombo();

    /*******************/
    /* Station Profile */
    /*******************/
    QStringListModel* stationProfilesModel = new QStringListModel(this);
    ui->stationProfileCombo->setModel(stationProfilesModel);
    refreshStationProfileCombo();

    /******************/
    /* Submodes Combo */
    /******************/
    QStringListModel* submodeModel = new QStringListModel(this);
    ui->submodeEdit->setModel(submodeModel);

    /****************/
    /* Modes Combo  */
    /****************/
    QSqlTableModel* modeModel = new QSqlTableModel();
    modeModel->setTable("modes");
    modeModel->setFilter("enabled = true");
    modeModel->setSort(modeModel->fieldIndex("name"), Qt::AscendingOrder);
    ui->modeEdit->setModel(modeModel);
    ui->modeEdit->setModelColumn(modeModel->fieldIndex("name"));
    modeModel->select();

    /**********************/
    /* Propagation Combo  */
    /**********************/
    QStringList propagationModeList = Data::instance()->propagationModesList();
    propagationModeList.prepend("");
    QStringListModel* propagationModeModel = new QStringListModel(propagationModeList, this);
    ui->propagationModeEdit->setModel(propagationModeModel);

    /***************/
    /* Completers  */
    /***************/
    wwffCompleter = new QCompleter(Data::instance()->wwffIDList(), this);
    wwffCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    wwffCompleter->setFilterMode(Qt::MatchStartsWith);
    wwffCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    uiDynamic->wwffEdit->setCompleter(nullptr);

    potaCompleter = new MultiselectCompleter(Data::instance()->potaIDList(), this);
    potaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    potaCompleter->setFilterMode(Qt::MatchStartsWith);
    potaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    uiDynamic->potaEdit->setCompleter(nullptr);

    sotaCompleter = new QCompleter(Data::instance()->sotaIDList(), this);
    sotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    sotaCompleter->setFilterMode(Qt::MatchStartsWith);
    sotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    uiDynamic->sotaEdit->setCompleter(nullptr);

    sigCompleter = new QCompleter(uiDynamic->sigEdit);
    sigCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    sigCompleter->setFilterMode(Qt::MatchStartsWith);
    sigCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    uiDynamic->sigEdit->setCompleter(sigCompleter);

    /**************/
    /* CONNECTs   */
    /**************/
    connect(rig, &Rig::frequencyChanged,
            this, &NewContactWidget::changeFrequency);

    connect(rig, &Rig::modeChanged,
            this, &NewContactWidget::changeModefromRig);

    connect(rig, &Rig::powerChanged,
            this, &NewContactWidget::changePower);

    connect(rig, &Rig::rigConnected,
            this, &NewContactWidget::rigConnected);

    connect(rig, &Rig::rigDisconnected,
            this, &NewContactWidget::rigDisconnected);

    connect(&callbookManager, &CallbookManager::callsignResult,
            this, &NewContactWidget::callsignResult);

    connect(&callbookManager, &CallbookManager::loginFailed, this, [this](const QString &callbookString)
    {
        QMessageBox::critical(this, tr("QLog Error"), callbookString + " " + tr("Callbook login failed"));
        setCallbookStatusEnabled(callbookManager.isActive());
    });

    connect(contactTimer, &QTimer::timeout, this, &NewContactWidget::updateTimeOff);

    connect(MembershipQE::instance(), &MembershipQE::clubStatusResult, this, &NewContactWidget::clubQueryResult);

    /******************************/
    /* CONNECTs  DYNAMIC WIDGETS  */
    /******************************/
    connect(uiDynamic->gridEdit, &QLineEdit::textChanged, this, &NewContactWidget::gridChanged);
    connect(uiDynamic->potaEdit, &QLineEdit::editingFinished, this, &NewContactWidget::potaEditFinished);
    connect(uiDynamic->potaEdit, &QLineEdit::textChanged, this, &NewContactWidget::potaChanged);
    connect(uiDynamic->sotaEdit, &QLineEdit::editingFinished, this, &NewContactWidget::sotaEditFinished);
    connect(uiDynamic->sotaEdit, &QLineEdit::textChanged, this, &NewContactWidget::sotaChanged);
    connect(uiDynamic->wwffEdit, &QLineEdit::editingFinished, this, &NewContactWidget::wwffEditFinished);
    connect(uiDynamic->wwffEdit, &QLineEdit::textChanged, this, &NewContactWidget::wwffChanged);
    connect(uiDynamic->satNameEdit, &QLineEdit::textChanged, this, &NewContactWidget::satNameChanged);
    connect(uiDynamic->sigEdit, &NewContactEditLine::focusIn, this, &NewContactWidget::refreshSIGCompleter);

    ui->rstSentEdit->installEventFilter(this);
    ui->rstRcvdEdit->installEventFilter(this);

    /********************/
    /* Local SHORTCUTs  */
    /********************/
    new QShortcut(QKeySequence(Qt::Key_Escape), this, SLOT(resetContact()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(QKeySequence(Qt::Key_F10), this, SLOT(saveContact()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(QKeySequence(Qt::Key_F9), this, SLOT(stopContactTimer()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(QKeySequence(Qt::Key_F8), this, SLOT(startContactTimer()), nullptr, Qt::ApplicationShortcut);

    /****************************/
    /* Set Visiable for widgets */
    /****************************/
    ui->freqTXEdit->setVisible(false);
    ui->bandTXLabel->setVisible(false);
    ui->freqRXLabel->setVisible(false);
    ui->freqTXLabel->setVisible(false);

    /***********************/
    /* Read Widget Setting */
    /***********************/
    readWidgetSettings();

    /*************************/
    /* Reload Global Setting */
    /*************************/
    readGlobalSettings();

    /**********************/
    /* Clear Contact Form */
    /**********************/
    resetContact();

    /************************/
    /* Connect Field Change */
    /* signals to determine */
    /* whether form changed */
    /************************/
    connectFieldChanged();

    // SQL query returns two QSOs. The first one is the last QSO with Base Callsign
    // and the second one is the last QSO for the Callsign from a portable QTH.
    isprevQSOQueryPrepared = prevQSOQuery.prepare(QLatin1String("SELECT callsign, "
                                                                "       name_intl, "
                                                                "       qth_intl, "
                                                                "       gridsquare, "
                                                                "       notes_intl, "
                                                                "       email, "
                                                                "       web , "
                                                                "       darc_dok "
                                                                "FROM ( "
                                                                "  SELECT "
                                                                "    callsign, "
                                                                "    name_intl, "
                                                                "    qth_intl, "
                                                                "    gridsquare, "
                                                                "    notes_intl, "
                                                                "    email, "
                                                                "    web , "
                                                                "    darc_dok "
                                                                "  FROM contacts "
                                                                "  WHERE callsign = :exactCallsign "
                                                                "  ORDER BY start_time DESC "
                                                                "  LIMIT 1 "
                                                                "  ) "
                                                                "UNION ALL "
                                                                "SELECT callsign, "
                                                                "       name_intl, "
                                                                "       qth_intl, "
                                                                "       gridsquare, "
                                                                "       notes_intl, "
                                                                "       email, "
                                                                "       web , "
                                                                "       darc_dok "
                                                                "FROM ( "
                                                                "  SELECT "
                                                                "     callsign, "
                                                                "     name_intl, "
                                                                "     qth_intl, "
                                                                "     gridsquare, "
                                                                "     notes_intl, "
                                                                "     email, "
                                                                "     web , "
                                                                "     darc_dok "
                                                                "  FROM contacts c, contacts_autovalue a "
                                                                "  WHERE c.id = a.contactid "
                                                                "        AND a.base_callsign = :partialCallsign "
                                                                "  ORDER BY start_time DESC "
                                                                "  LIMIT 1 "
                                                                "  )"));

    if ( !isprevQSOQueryPrepared)
        qWarning() << "Cannot prepare prevQSOquery statement";
}

void NewContactWidget::setComboBaseData(QComboBox *combo, const QString &data)
{
    FCT_IDENTIFICATION;

    int index =  combo->findData(data);
    if ( index != -1 )
    {
       combo->setCurrentIndex(index);
    }
}

void NewContactWidget::queryMemberList()
{
    FCT_IDENTIFICATION;

    if ( callsign.size() >= 3 )
    {
        MembershipQE::instance()->asyncQueryDetails(callsign, ui->bandRXLabel->text(), ui->modeEdit->currentText());
    }
}

void NewContactWidget::readWidgetSettings()
{
    FCT_IDENTIFICATION;

    realRigFreq = settings.value("newcontact/frequency", 3.5).toDouble();
    ui->modeEdit->setCurrentText(settings.value("newcontact/mode", "CW").toString());
    ui->submodeEdit->setCurrentText(settings.value("newcontact/submode").toString());
    ui->powerEdit->setValue(settings.value("newcontact/power", 100).toDouble());
    ui->qsoTabs->setCurrentIndex(settings.value("newcontact/tabindex", 0).toInt());
    setComboBaseData(ui->qslSentBox, settings.value("newcontact/qslsent", "Q").toString());
    setComboBaseData(ui->lotwQslSentBox, settings.value("newcontact/lotwqslsent", "Q").toString());
    setComboBaseData(ui->eQSLSentBox, settings.value("newcontact/eqslqslsent", "Q").toString());
    setComboBaseData(ui->qslSentViaBox, settings.value("newcontact/qslsentvia", "").toString());
    ui->propagationModeEdit->setCurrentText(Data::instance()->propagationModeIDToText(settings.value("newcontact/propmode", QString()).toString()));
}

void NewContactWidget::writeWidgetSetting()
{
    FCT_IDENTIFICATION;

    settings.setValue("newcontact/mode", ui->modeEdit->currentText());
    settings.setValue("newcontact/submode", ui->submodeEdit->currentText());
    settings.setValue("newcontact/frequency", realRigFreq);
    settings.setValue("newcontact/power", ui->powerEdit->value());
    settings.setValue("newcontact/tabindex", ui->qsoTabs->currentIndex());
    settings.setValue("newcontact/qslsent", ui->qslSentBox->itemData(ui->qslSentBox->currentIndex()));
    settings.setValue("newcontact/eqslqslsent", ui->eQSLSentBox->itemData(ui->eQSLSentBox->currentIndex()));
    settings.setValue("newcontact/eqslqslsent", ui->lotwQslSentBox->itemData(ui->lotwQslSentBox->currentIndex()));
    settings.setValue("newcontact/qslsentvia", ui->qslSentViaBox->itemData(ui->qslSentViaBox->currentIndex()));
    settings.setValue("newcontact/propmode", Data::instance()->propagationModeTextToID(ui->propagationModeEdit->currentText()));
}

/* function read global setting, called when starting or when Setting is reloaded */
void NewContactWidget::readGlobalSettings()
{
    FCT_IDENTIFICATION;

    /*************************/
    /* Refresh mode combobox */
    /*************************/
    QString current_mode = ui->modeEdit->currentText();
    QString current_submode = ui->submodeEdit->currentText();
    ui->modeEdit->blockSignals(true);
    ui->submodeEdit->blockSignals(true);
    dynamic_cast<QSqlTableModel*>(ui->modeEdit->model())->select();
    ui->modeEdit->blockSignals(false);
    ui->submodeEdit->blockSignals(false);
    ui->modeEdit->setCurrentText(current_mode);
    ui->submodeEdit->setCurrentText(current_submode);

    /********************/
    /* Reload Callbooks */
    /********************/
    callbookManager.initCallbooks();
    setCallbookStatusEnabled(callbookManager.isActive());

    /***********************************/
    /* Refresh all Profiles Comboboxes */
    /***********************************/
    refreshStationProfileCombo();
    refreshRigProfileCombo();
    refreshAntProfileCombo();

    // recalculate all stats
    queryDxcc(ui->callsignEdit->text().toUpper());

    ui->freqRXEdit->loadBands();
    ui->freqTXEdit->loadBands();
}

/* function is called when an operator change Callsign Edit */
void NewContactWidget::callsignChanged()
{
    FCT_IDENTIFICATION;

    QString newCallsign = ui->callsignEdit->text().toUpper();

    qCDebug(runtime) << "newcallsign " << newCallsign << " old callsign " << callsign;

    // if operator presses a spacebar at the end of callsign then begins QSO and skips RST Fields
    if ( newCallsign.endsWith(" "))
    {
        newCallsign.chop(1);
        ui->callsignEdit->blockSignals(true);
        ui->callsignEdit->setText(newCallsign);
        ui->callsignEdit->blockSignals(false);
        if ( newCallsign.isEmpty() )
            return;

        QLineEdit *nextLineEdit = qobject_cast<QLineEdit*>(ui->rstRcvdEdit->nextInFocusChain());
        if ( nextLineEdit )
        {
            nextLineEdit->setFocus();
            startContactTimer();
        }
    }

    if ( newCallsign == callsign )
        return;

    callsign = newCallsign;

    clearCallbookQueryFields();
    clearMemberQueryFields();

    if ( callsign.isEmpty() )
    {
        updateTime();
        stopContactTimer();
    }
    else
    {
        queryDxcc(callsign);

        if ( callsign.length() >= 3 )
            fillFieldsFromLastQSO(callsign);
    }
}

/* function is called when Callsign Edit is finished - example pressed enter */
/* if callsign is entered then QLog call callbook query */
void NewContactWidget::editCallsignFinished()
{
    FCT_IDENTIFICATION;

    if ( callsign.size() >= 3 )
    {
        queryMemberList();
        if ( !callbookSearchPaused )
                callbookManager.queryCallsign(callsign);
    }
}


void NewContactWidget::setCurrentDxcc(const DxccEntity &curr)
{
    FCT_IDENTIFICATION;

    dxccEntity = curr;

    if ( dxccEntity.dxcc )
    {
        ui->dxccInfo->setText(QCoreApplication::translate("DBStrings", dxccEntity.country.toUtf8().constData()));
        uiDynamic->cqzEdit->setText(QString::number(dxccEntity.cqz));
        uiDynamic->ituEdit->setText(QString::number(dxccEntity.ituz));
        updateCoordinates(dxccEntity.latlon[0], dxccEntity.latlon[1], COORD_DXCC);
        ui->dxccTableWidget->setDxcc(dxccEntity.dxcc, BandPlan::freq2Band(ui->freqTXEdit->value()));
        uiDynamic->contEdit->setCurrentText(dxccEntity.cont);

        updateDxccStatus();

        if ( !dxccEntity.flag.isEmpty() )
        {
            QPixmap flag(QString(":/flags/64/%1.png").arg(dxccEntity.flag));
            ui->flagView->setPixmap(flag);
        }
        else
            ui->flagView->setPixmap(QPixmap());
    }
    else
    {
        ui->flagView->setPixmap(QPixmap());
        ui->dxccTableWidget->clear();
        ui->dxccStatus->clear();
        ui->distanceInfo->clear();
        dxDistance = qQNaN();
        ui->bearingInfo->clear();
        partnerTimeZone = QTimeZone();
        ui->partnerLocTimeInfo->clear();
        ui->dxccInfo->setText(" ");
        uiDynamic->cqzEdit->clear();
        uiDynamic->ituEdit->clear();
        uiDynamic->contEdit->setCurrentText("");

        emit newTarget(qQNaN(), qQNaN());
    }
}

/* Obtain DXCC info from local database */
void NewContactWidget::queryDxcc(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    setCurrentDxcc(Data::instance()->lookupDxcc(callsign));
}

void NewContactWidget::fillFieldsFromLastQSO(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    if ( !isprevQSOQueryPrepared )
        return;

    const Callsign enteredCallsign(callsign);

    if ( !enteredCallsign.isValid() )
    {
        emit filterCallsign(QString());
        return;
    }

    const QString &baseCallsign = enteredCallsign.getBase();
    // search the base_callsign
    prevQSOQuery.bindValue(":exactCallsign",baseCallsign );
    prevQSOQuery.bindValue(":partialCallsign", baseCallsign);

    if ( !prevQSOQuery.exec() )
    {
        qWarning() << "Cannot execute statement" << prevQSOQuery.lastError();
        emit filterCallsign(QString());
        return;
    }

    if ( prevQSOQuery.next() )
    {
        // SQL query returns two QSOs. The first one is the last QSO with Base Callsign
        // and the second one is the last QSO for callsign, which was made from a portable QTH.
        // The recognition is possible because the record where Callsign == Based Callsign
        // is a QSO with the given Callsign from the base QTH.
        // Otherwise, it's a QSO with a given Callsign from a portable QTH.
        if ( prevQSOQuery.value("callsign").toString() == baseCallsign
             && enteredCallsign.getHostPrefix().isEmpty()
             && enteredCallsign.getSuffix().isEmpty() )
        {
            // entered callsign is base callsign - no portable QTH. Get all fields from
            // previous QSO
            uiDynamic->qthEdit->setText(prevQSOQuery.value("qth_intl").toString());
            uiDynamic->gridEdit->setText(prevQSOQuery.value("gridsquare").toString());
            uiDynamic->dokEdit->setText(prevQSOQuery.value("darc_dok").toString());
        }
        uiDynamic->nameEdit->setText(prevQSOQuery.value("name_intl").toString());
        ui->noteEdit->insertPlainText(prevQSOQuery.value("notes_intl").toString());
        uiDynamic->emailEdit->setText(prevQSOQuery.value("email").toString());
        uiDynamic->urlEdit->setText(prevQSOQuery.value("web").toString());

        emit filterCallsign(baseCallsign);
    }
    else
    {
        //callsign not found
        qCDebug(runtime) << "Callsign not match in the Logbook";
        emit filterCallsign(QString());
    }
}

/* function handles a response from Callbook classes */
void NewContactWidget::callsignResult(const QMap<QString, QString>& data)
{
    FCT_IDENTIFICATION;

    if ( data.value("call") != callsign )
        return;

    /* not filled or not fully filled then update it */
    const QString fnamelname = QString("%1 %2").arg(data.value("fname"),
                                                    data.value("lname"));
    if ( uiDynamic->nameEdit->text().isEmpty()
         || data.value("name_fmt").contains(uiDynamic->nameEdit->text())
         || fnamelname.contains(uiDynamic->nameEdit->text())
         || data.value("nick").contains(uiDynamic->nameEdit->text()) )
    {
        QString name = data.value("name_fmt");

        if ( name.isEmpty() )
            name = ( data.value("fname").isEmpty() && data.value("lname").isEmpty() ) ? data.value("nick")
                                                                                      : fnamelname;
        uiDynamic->nameEdit->setText(name);
    }

    if ( uiDynamic->gridEdit->text().isEmpty()
         || data.value("gridsquare").contains(uiDynamic->gridEdit->text()) )
    {
        uiDynamic->gridEdit->setText(data.value("gridsquare"));
    }

    if ( uiDynamic->qthEdit->text().isEmpty()
         || data.value("qth").contains(uiDynamic->qthEdit->text()))
    {
        uiDynamic->qthEdit->setText(data.value("qth"));
    }

    if ( uiDynamic->dokEdit->text().isEmpty() )
    {
        uiDynamic->dokEdit->setText(data.value("dok"));
    }

    if ( uiDynamic->iotaEdit->text().isEmpty() )
    {
        uiDynamic->iotaEdit->setText(data.value("iota"));
    }

    if ( uiDynamic->emailEdit->text().isEmpty() )
    {
        uiDynamic->emailEdit->setText(data.value("email"));
    }

    if ( uiDynamic->countyEdit->text().isEmpty() )
    {
        uiDynamic->countyEdit->setText(data.value("county"));
    }

    if ( ui->qslViaEdit->text().isEmpty() )
    {
        ui->qslViaEdit->setText(data.value("qsl_via"));
    }

    if ( uiDynamic->urlEdit->text().isEmpty() )
    {
        uiDynamic->urlEdit->setText(data.value("url"));
    }

    if ( uiDynamic->stateEdit->text().isEmpty() )
    {
        uiDynamic->stateEdit->setText(data.value("us_state"));
    }

    if ( data.value("eqsl") == "Y" )
    {
        ui->eqslLabel->setText("eQSL");
    }

    if ( data.value("lotw") == "Y" )
    {
        ui->lotwLabel->setText("LoTW");
    }

    if ( !data.value("dxcc").isEmpty() )
    {
        int callbookDXCC = data.value("dxcc").toInt();

        if ( callbookDXCC != 0 && callbookDXCC != dxccEntity.dxcc )
        {
            qCDebug(runtime) << "Received different DXCC Info" << data.value("dxcc")
                             << dxccEntity.dxcc;
            setCurrentDxcc(Data::instance()->lookupDxccID(callbookDXCC));
        }
    }

    // always replace cqz/itu
    if ( !data.value("ituz").isEmpty() )
        uiDynamic->ituEdit->setText(data.value("ituz"));

    if ( !data.value("cqz").isEmpty() )
        uiDynamic->cqzEdit->setText(data.value("cqz"));

    emit callboolImageUrl(data.value("image_url"));

    lastCallbookQueryData = QMap<QString, QString>(data);
}

void NewContactWidget::clubQueryResult(const QString &in_callsign, QMap<QString, ClubStatusQuery::ClubStatus> data)
{
    FCT_IDENTIFICATION;

    if ( in_callsign != callsign )
    {
        // do not need this result
        return;
    }

    QString memberText;

    QMapIterator<QString, ClubStatusQuery::ClubStatus> clubs(data);

    QPalette palette;

    //"<font color='red'>Hello</font> <font color='green'>World</font>"
    while ( clubs.hasNext() )
    {
        clubs.next();
        QColor color = Data::statusToColor(static_cast<DxccStatus>(clubs.value()), palette.color(QPalette::Text));
        memberText.append(QString("<font color='%1'>%2</font>&nbsp;&nbsp;&nbsp;").arg(Data::colorToHTMLColor(color), clubs.key()));
    }
    ui->memberListLabel->setText(memberText);
}


/* function just refresh Station Profile Combo */
void NewContactWidget::refreshStationProfileCombo()
{
    FCT_IDENTIFICATION;

    ui->stationProfileCombo->blockSignals(true);

    QStringList currProfiles = StationProfilesManager::instance()->profileNameList();
    QStringListModel* model = dynamic_cast<QStringListModel*>(ui->stationProfileCombo->model());

    model->setStringList(currProfiles);

    if ( StationProfilesManager::instance()->getCurProfile1().profileName.isEmpty()
         && currProfiles.count() > 0 )
    {
        /* changing profile from empty to something */
        ui->stationProfileCombo->setCurrentText(currProfiles.first());
        stationProfileComboChanged(currProfiles.first());
    }
    else
    {
        /* no profile change, just refresh the combo and preserve current profile */
        ui->stationProfileCombo->setCurrentText(StationProfilesManager::instance()->getCurProfile1().profileName);
    }

    ui->stationProfileCombo->blockSignals(false);
}

/* function just refresh Rig Profile Combo */
void NewContactWidget::refreshRigProfileCombo()
{
    FCT_IDENTIFICATION;

    ui->rigEdit->blockSignals(true);

    QStringList currProfiles = RigProfilesManager::instance()->profileNameList();
    QString currentText = ui->rigEdit->currentText();
    QStringListModel* model = dynamic_cast<QStringListModel*>(ui->rigEdit->model());

    model->setStringList(currProfiles);
    if ( RigProfilesManager::instance()->getCurProfile1().profileName.isEmpty()
         && currProfiles.count() > 0 )
    {
        /* changing profile from empty to something */
        currentText = currProfiles.first();
        ui->rigEdit->setCurrentText(currentText);
        rigProfileComboChanged(currProfiles.first());
    }
    else
    {
        /* no profile change, just refresh the combo and preserve current profile */
        currentText = (isManualEnterMode ) ? currentText
                                           : RigProfilesManager::instance()->getCurProfile1().profileName;
        ui->rigEdit->setCurrentText(currentText);
        //do not call rigProfileComboChanged because it did not change
    }

    ui->rigEdit->blockSignals(false);

    if ( isManualEnterMode )
        return;

    ui->freqRXEdit->setValue(realRigFreq + RigProfilesManager::instance()->getProfile(currentText).ritOffset);
    ui->freqTXEdit->setValue(realRigFreq + RigProfilesManager::instance()->getProfile(currentText).xitOffset);
    ui->powerEdit->setValue(RigProfilesManager::instance()->getProfile(currentText).defaultPWR);

}

void NewContactWidget::refreshAntProfileCombo()
{
    FCT_IDENTIFICATION;

    ui->antennaEdit->blockSignals(true);

    QStringList currProfiles = AntProfilesManager::instance()->profileNameList();
    QStringListModel* model = dynamic_cast<QStringListModel*>(ui->antennaEdit->model());

    model->setStringList(currProfiles);

    if ( AntProfilesManager::instance()->getCurProfile1().profileName.isEmpty()
         && currProfiles.count() > 0 )
    {
        /* changing profile from empty to something */
        ui->antennaEdit->setCurrentText(currProfiles.first());
        antProfileComboChanged(currProfiles.first());
    }
    else
    {
        /* no profile change, just refresh the combo and preserve current profile */
        ui->antennaEdit->setCurrentText(AntProfilesManager::instance()->getCurProfile1().profileName);
    }

    ui->antennaEdit->blockSignals(false);
}

void NewContactWidget::__modeChanged()
{
    FCT_IDENTIFICATION;

    QSqlTableModel* modeModel = dynamic_cast<QSqlTableModel*>(ui->modeEdit->model());
    QSqlRecord record = modeModel->record(ui->modeEdit->currentIndex());
    QString submodes = record.value("submodes").toString();

    QStringList submodeList = QJsonDocument::fromJson(submodes.toUtf8()).toVariant().toStringList();
    QStringListModel* model = dynamic_cast<QStringListModel*>(ui->submodeEdit->model());
    model->setStringList(submodeList);

    if (!submodeList.isEmpty()) {
        submodeList.prepend("");
        model->setStringList(submodeList);
        ui->submodeEdit->setVisible(true);
        ui->submodeEdit->setCurrentIndex(1);
    }
    else {
        QStringList list;
        model->setStringList(list);
        ui->submodeEdit->setVisible(false);
        ui->submodeEdit->setCurrentIndex(-1);
    }

    // Adjuste Combos Size
    int maxWidth = qMax(ui->modeEdit->sizeHint().width(),
                        ui->submodeEdit->sizeHint().width());
    ui->modeEdit->setFixedWidth(maxWidth);
    ui->submodeEdit->setFixedWidth(maxWidth);

    defaultReport = record.value("rprt").toString();

    setDefaultReport();
    updateDxccStatus();
    queryMemberList();
}

/* Mode is changed from GUI */
void NewContactWidget::changeMode()
{
    FCT_IDENTIFICATION;

    ui->submodeEdit->blockSignals(true);
    __modeChanged();
    ui->submodeEdit->blockSignals(false);

    // if manual mode is not enabled then change the mode
    if ( !isManualEnterMode )
    {
        rig->setMode(ui->modeEdit->currentText(), ui->submodeEdit->currentText());
        emit userModeChanged(VFO1, QString(),
                             ui->modeEdit->currentText(), ui->submodeEdit->currentText(),
                             bandwidthFilter);
    }
}

/* mode is changed from RIG */
/* Receveived from RIG */
void NewContactWidget::changeModefromRig(VFOID, const QString &, const QString &mode,
                                         const QString &subMode, qint32 width)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode << subMode << width;

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    bandwidthFilter = width;

    changeModeWithoutSignals(mode, subMode);
}

void NewContactWidget::subModeChanged()
{
    FCT_IDENTIFICATION;

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    rig->setMode(ui->modeEdit->currentText(), ui->submodeEdit->currentText());
    emit userModeChanged(VFO1, QString(),
                         ui->modeEdit->currentText(), ui->submodeEdit->currentText(),
                         bandwidthFilter);
}

void NewContactWidget::updateTXBand(double freq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<freq;

    bandTX = BandPlan::freq2Band(freq);

    if (bandTX.name.isEmpty())
    {
        ui->bandTXLabel->setText("OOB!");
    }
    else if (bandTX.name != ui->bandTXLabel->text())
    {
        ui->bandTXLabel->setText(bandTX.name);
    }

    updateSatMode();
    updateDxccStatus();
    ui->dxccTableWidget->setDxcc(dxccEntity.dxcc, BandPlan::freq2Band(ui->freqTXEdit->value()));
}

void NewContactWidget::updateRXBand(double freq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<freq;

    bandRX = BandPlan::freq2Band(freq);

    if (bandRX.name.isEmpty())
    {
        setBandLabel("OOB!");
    }
    else if (bandRX.name != ui->bandRXLabel->text())
    {
        setBandLabel(bandRX.name);
    }
    updateSatMode();
    updateDxccStatus();
}

void NewContactWidget::gridChanged()
{
    FCT_IDENTIFICATION;

    Gridsquare newGrid(uiDynamic->gridEdit->text());

    if (!newGrid.isValid())
    {
        coordPrec = COORD_NONE;
        queryDxcc(ui->callsignEdit->text().toUpper());
        return;
    }

    updateCoordinates(newGrid.getLatitude(), newGrid.getLongitude(), COORD_GRID);
}

void NewContactWidget::clearCallbookQueryFields()
{
    FCT_IDENTIFICATION;

    uiDynamic->nameEdit->clear();
    uiDynamic->gridEdit->clear();
    uiDynamic->qthEdit->clear();
    uiDynamic->dokEdit->clear();
    uiDynamic->iotaEdit->clear();
    uiDynamic->emailEdit->clear();
    uiDynamic->countyEdit->clear();
    uiDynamic->urlEdit->clear();
    uiDynamic->stateEdit->clear();

    ui->qslViaEdit->clear();
    ui->eqslLabel->setText(QString());
    ui->lotwLabel->setText(QString());
    emit callboolImageUrl("");
}

void NewContactWidget::clearMemberQueryFields()
{
    FCT_IDENTIFICATION;

    ui->memberListLabel->clear();
}

void NewContactWidget::resetContact()
{
    FCT_IDENTIFICATION;

    ui->callsignEdit->clear();
    uiDynamic->commentEdit->clear();
    ui->noteEdit->clear();
    ui->dxccInfo->setText(" ");
    ui->distanceInfo->clear();
    dxDistance = qQNaN();
    ui->bearingInfo->clear();
    ui->partnerLocTimeInfo->clear();
    uiDynamic->cqzEdit->clear();
    uiDynamic->ituEdit->clear();
    uiDynamic->contEdit->setCurrentText("");
    uiDynamic->sotaEdit->clear();
    uiDynamic->potaEdit->clear();
    uiDynamic->sigEdit->clear();
    uiDynamic->sigInfoEdit->clear();
    uiDynamic->vuccEdit->clear();
    uiDynamic->wwffEdit->clear();
    ui->dxccTableWidget->clear();
    ui->dxccStatus->clear();
    ui->flagView->setPixmap(QPixmap());
    uiDynamic->ageEdit->clear();

    clearCallbookQueryFields();
    clearMemberQueryFields();

    ui->callsignEdit->setPalette(QPalette());
    ui->callsignEdit->setFocus();

    partnerTimeZone = QTimeZone();

    updateTime();
    stopContactTimer();

    setDefaultReport();

    callsign = QString();
    dxccEntity = DxccEntity();
    coordPrec = COORD_NONE;
    QSOFreq = 0.0;

    emit filterCallsign(QString());
    emit newTarget(qQNaN(), qQNaN());
}

void NewContactWidget::addAddlFields(QSqlRecord &record, const StationProfile &profile)
{
    FCT_IDENTIFICATION;


    if ( record.value("pfx").toString().isEmpty() )
    {
        const QString &pfxRef = Callsign(record.value("callsign").toString()).getWPXPrefix();

        if ( !pfxRef.isEmpty() )
        {
            record.setValue("pfx", pfxRef);
        }
    }

    if ( record.value("qsl_sent").toString().isEmpty() )
    {
        QVariant sentState = ui->qslSentBox->itemData(ui->qslSentBox->currentIndex());
        record.setValue("qsl_sent", sentState);
        if ( sentState == QVariant("Y") )
        {
            record.setValue("qsl_sdate", QDate::currentDate());
        }
    }

    if ( record.value("lotw_qsl_sent").toString().isEmpty() )
    {
        QVariant sentState = ui->lotwQslSentBox->itemData(ui->lotwQslSentBox->currentIndex());
        record.setValue("lotw_qsl_sent", sentState);
        if ( sentState == QVariant("Y") )
        {
            record.setValue("lotw_qslsdate", QDate::currentDate());
        }
    }

    if ( record.value("eqsl_qsl_sent").toString().isEmpty() )
    {
        QVariant sentState = ui->eQSLSentBox->itemData(ui->eQSLSentBox->currentIndex());
        record.setValue("eqsl_qsl_sent", sentState);
        if ( sentState == QVariant("Y") )
        {
            record.setValue("eqsl_qslsdate", QDate::currentDate());
        }
    }

    record.setValue("qsl_rcvd", "N");
    record.setValue("lotw_qsl_rcvd", "N");
    record.setValue("eqsl_qsl_rcvd", "N");

    /* isNull is not necessary to use because NULL Text fields are empty */
    if ( record.value("my_gridsquare").toString().isEmpty()
         && !profile.locator.isEmpty())
    {
        record.setValue("my_gridsquare", profile.locator.toUpper());
    }

    if ( ! record.value("gridsquare").toString().isEmpty()
         && ! record.value("my_gridsquare").toString().isEmpty() )
    {
        Gridsquare myGrid(record.value("my_gridsquare").toString());
        double distance;
        if ( myGrid.distanceTo(Gridsquare(record.value("gridsquare").toString()), distance) )
        {
            record.setValue("distance", distance);
        }
    }

    if ( Rotator::instance()->isRotConnected() )
    {
        record.setValue("ant_az", Rotator::instance()->getAzimuth());
        record.setValue("ant_el", Rotator::instance()->getElevation());
    }

    if ( prop_cond )
    {
        if ( record.value("sfi").toString().isEmpty()
             && prop_cond->isFluxValid() )
        {
            record.setValue("sfi", prop_cond->getFlux());
        }

        if ( record.value("k_index").toString().isEmpty()
             && prop_cond->isKIndexValid() )
        {
            record.setValue("k_index", prop_cond->getKIndex());
        }

        if ( record.value("a_index").toString().isEmpty()
             && prop_cond->isAIndexValid() )
        {
            record.setValue("a_index", prop_cond->getAIndex());
        }
    }

    if ( (record.value("tx_pwr").toString().isEmpty() || record.value("tx_pwr") == 0.0 )
         && ui->powerEdit->value() != 0.0)
    {
        record.setValue("tx_pwr", ui->powerEdit->value());
    }

    if ( record.value("band").toString().isEmpty()
         && ! record.value("freq").toString().isEmpty() )
    {
        record.setValue("band", BandPlan::freq2Band(record.value("freq").toDouble()).name);
    }

    if ( record.value("prop_mode").toString().isEmpty()
         && !ui->propagationModeEdit->currentText().isEmpty())
    {
        record.setValue("prop_mode", Data::instance()->propagationModeTextToID(ui->propagationModeEdit->currentText()));
    }

    if ( record.value("prop_mode").toString() == "SAT" )
    {
        // we only allow SAT_MODE information to be taken from the GUI
        // if the Log record TX Band/Freq matches the GUI's Band/Freq. Unfortunately, RX Freq will not be used,
        // because it might be missing from the log record.
        if ( record.value("sat_mode").toString().isEmpty()
             && !uiDynamic->satModeEdit->currentText().isEmpty()
             && record.value("band").toString() == ui->bandTXLabel->text())
        {
            record.setValue("sat_mode", Data::instance()->satModeTextToID(uiDynamic->satModeEdit->currentText()));
        }

        if ( record.value("sat_name").toString().isEmpty()
             && !uiDynamic->satNameEdit->text().isEmpty() )
        {
            record.setValue("sat_name", Data::removeAccents(uiDynamic->satNameEdit->text().toUpper()));
        }
    }

    if ( record.value("my_rig_intl").toString().isEmpty()
         && !ui->rigEdit->currentText().isEmpty() )
    {
       record.setValue("my_rig_intl", ui->rigEdit->currentText());
    }

    if ( record.value("my_antenna_intl").toString().isEmpty()
         && !ui->antennaEdit->currentText().isEmpty())
    {
       record.setValue("my_antenna_intl", ui->antennaEdit->currentText());
    }

    if ( record.value("my_city_intl").toString().isEmpty()
         && !profile.qthName.isEmpty())
    {
       record.setValue("my_city_intl", profile.qthName);
    }

    if ( record.value("station_callsign").toString().isEmpty()
         && !profile.callsign.isEmpty() )
    {
       record.setValue("station_callsign", profile.callsign.toUpper());
    }

    if ( record.value("my_dxcc").toString().isEmpty()
         && profile.dxcc != 0 )
    {
        record.setValue("my_dxcc", profile.dxcc);
        record.setValue("my_country_intl", profile.country);
    }

    if ( record.value("my_itu_zone").toString().isEmpty()
         && profile.ituz != 0 )
    {
        record.setValue("my_itu_zone", profile.ituz);
    }

    if ( record.value("my_cq_zone").toString().isEmpty()
         && profile.cqz != 0 )
    {
        record.setValue("my_cq_zone", profile.cqz);
    }

    if ( record.value("my_name_intl").toString().isEmpty()
         && !profile.operatorName.isEmpty() )
    {
       record.setValue("my_name_intl", profile.operatorName);
    }

    if ( record.value("my_iota").toString().isEmpty()
         && !profile.iota.isEmpty())
    {
       record.setValue("my_iota", Data::removeAccents(profile.iota.toUpper()));
    }

    if ( record.value("my_sota_ref").toString().isEmpty()
         && !profile.sota.isEmpty())
    {
       record.setValue("my_sota_ref", Data::removeAccents(profile.sota.toUpper()));
    }

    if ( ! record.value("my_sota_ref").toString().isEmpty() )
    {
        SOTAEntity sotaInfo = Data::instance()->lookupSOTA(record.value("my_sota_ref").toString());
        if ( sotaInfo.summitCode.toUpper() == record.value("my_sota_ref").toString().toUpper()
             && !sotaInfo.summitName.isEmpty() )
        {
            record.setValue("my_altitude", sotaInfo.altm);
        }
    }

    if ( record.value("my_pota_ref").toString().isEmpty()
         && !profile.pota.isEmpty())
    {
       record.setValue("my_pota_ref", Data::removeAccents(profile.pota.toUpper()));
    }

    if ( record.value("my_sig_intl").toString().isEmpty()
         && !profile.sig.isEmpty())
    {
       record.setValue("my_sig_intl", profile.sig);
    }

    if ( record.value("my_sig_info_intl").toString().isEmpty()
         && !profile.sigInfo.isEmpty())
    {
       record.setValue("my_sig_info_intl", profile.sigInfo);
    }

    if ( record.value("my_vucc_grids").toString().isEmpty()
         && !profile.vucc.isEmpty())
    {
       record.setValue("my_vucc_grids", profile.vucc.toUpper());
    }

    if ( record.value("my_wwff_ref").toString().isEmpty()
         && !profile.wwff.isEmpty())
    {
       record.setValue("my_wwff_ref", profile.wwff.toUpper());
    }
}

bool NewContactWidget::eventFilter(QObject *object, QEvent *event)
{
    //FCT_IDENTIFICATION;

    if ( event->type() == QEvent::FocusIn
         && object == ui->rstSentEdit )
    {

        if ( ui->callsignEdit->text().isEmpty()
             && ! ui->nearStationLabel->text().isEmpty() )
        {
            changeCallsignManually(ui->nearStationLabel->text());
        }

        if ( callsign.size() >= 3
             && !contactTimer->isActive() )
        {
            startContactTimer();
        }
    }

    // Event handle to handle Enter press event for the Custom UI row.
    // The basic idea is:
    //   - if enter is pressed on the Row A or Row B or RSTr(s) and QSO Timer is active then QSO is saved
    //   - hadnler is registere automaticaly when fields are added to Row.
    //   - But Callsign, POTA, SOTA, WWFF fields enter means "search" the value in callbook or xOTA directories
    //     Therefore Enter event is blocked for these fields with one exception:
    //       if Callbook is not active then Callsign enter causes "save QSO" event too.
    //       The callsign exception was added to make it possible to quickly insert QSOs during pile-up.
    //       Therefore there is no condition for an active QSO timer.
    if ( event->type() == QEvent::KeyPress
         && static_cast<QKeyEvent *>(event)
         && ( static_cast<QKeyEvent *>(event)->key() == Qt::Key_Return
              || static_cast<QKeyEvent *>(event)->key() == Qt::Key_Enter )
         && ( ( object == ui->callsignEdit && (!callbookManager.isActive() || callbookSearchPaused) )
              || ( object != ui->callsignEdit
                   && object != uiDynamic->potaEdit
                   && object != uiDynamic->sotaEdit
                   && object != uiDynamic->wwffEdit
                   && contactTimer->isActive() ) ) )
    {
        saveContact();
    }

    return false;
}

bool NewContactWidget::isQSOTimeStarted()
{
    FCT_IDENTIFICATION;

    bool ret = ( contactTimer ) ? contactTimer->isActive() : false;

    qCDebug(runtime) << ret;
    return ret;
}

void NewContactWidget::QSYContactWiping(double newFreq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << newFreq;

    qint32 QSYWipingWidth = bandwidthFilter;

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    if ( QSYWipingWidth <= BANDWIDTH_UNKNOWN )
    {
        QSYWipingWidth = Rig::getNormalBandwidth(ui->modeEdit->currentText(),
                                                ui->submodeEdit->currentText());
        qCDebug(runtime) << "Passband is not defined - derived value " << QSYWipingWidth;
    }

    qCDebug(runtime) << "Rig online: " << rigOnline << " "
                     << "QSO Freq: " << QSOFreq << " "
                     << "QSO Time: " << isQSOTimeStarted() << " "
                     << "Mode/submode: " << ui->modeEdit->currentText() << ui->submodeEdit->currentText()
                     << "RIG Filter width: " << bandwidthFilter
                     << "QSYWipingWidth: " << QSYWipingWidth << QSTRING_FREQ(Hz2MHz(QSYWipingWidth))
                     << "Diff: " << qAbs(QSOFreq - newFreq)
                     << "Rig Profile QSO Wiping: " << RigProfilesManager::instance()->getCurProfile1().QSYWiping;

    if ( RigProfilesManager::instance()->getCurProfile1().QSYWiping
         && rigOnline            // only if Rig is connected
         && QSOFreq > 0.0        // it means that Form is "dirty" and contain freq when it got dirty
         && !isQSOTimeStarted()  // operator is not in QSO
         && QSYWipingWidth != BANDWIDTH_UNKNOWN
         && qAbs(QSOFreq - newFreq) > Hz2MHz(QSYWipingWidth) / 1.5 )  //1.5 is a magic constant - determined experimentally
    {
        resetContact();
    }
}

void NewContactWidget::connectFieldChanged()
{
    FCT_IDENTIFICATION;

    connect(ui->callsignEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(ui->rstSentEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(ui->rstRcvdEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->nameEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->qthEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->commentEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(ui->noteEdit, &QTextEdit::textChanged,
            this, &NewContactWidget::formFieldChanged);

    connect(uiDynamic->contEdit, &QComboBox::currentTextChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->ituEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->cqzEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->stateEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->countyEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->ageEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->iotaEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->sotaEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->potaEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->sigEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->sigInfoEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->dokEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->vuccEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->wwffEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(ui->qslSentBox, &QComboBox::currentTextChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(ui->lotwQslSentBox, &QComboBox::currentTextChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(ui->eQSLSentBox, &QComboBox::currentTextChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(ui->qslSentViaBox, &QComboBox::currentTextChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(ui->qslViaEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->emailEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->urlEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    connect(uiDynamic->gridEdit, &QLineEdit::textChanged,
            this, &NewContactWidget::formFieldChangedString);

    /* no other fields are currently considered
     * as an attempt to fill out the form */
}

void NewContactWidget::saveContact()
{
    FCT_IDENTIFICATION;

    const StationProfile &profile = StationProfilesManager::instance()->getProfile(ui->stationProfileCombo->currentText());

    if ( profile.callsign.isEmpty() )
    {
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                              QMessageBox::tr("Your callsign is empty. Please, set your Station Profile"));
        return;
    }

    // if operator wants to save a QSO and QSO's Timer is not running,
    // then it is needed to update the QSO start time before saving
    if ( !isQSOTimeStarted() )
        updateTime();

    QDateTime start = QDateTime(ui->dateEdit->date(), ui->timeOnEdit->time(), Qt::UTC);
    QDateTime end = ( isManualEnterMode ) ? start.addSecs(QTime(0,0).secsTo(ui->qsoDurationEdit->time()))
                                          : timeOff;

    QSqlTableModel model;
    model.setTable("contacts");
    QSqlField idField = model.record().field(model.fieldIndex("id"));
    model.removeColumn(model.fieldIndex("id"));

    QSqlRecord record = model.record(0);

    record.setValue("start_time", start);
    record.setValue("end_time", end);

    if ( ! callsign.isEmpty() )
    {
        record.setValue("callsign", callsign);
    }

    if ( ! ui->rstSentEdit->text().isEmpty() )
    {
        record.setValue("rst_sent", ui->rstSentEdit->text());
    }

    if ( ! ui->rstRcvdEdit->text().isEmpty() )
    {
        record.setValue("rst_rcvd", ui->rstRcvdEdit->text());
    }

    if ( ! uiDynamic->nameEdit->text().isEmpty() )
    {
        record.setValue("name_intl", uiDynamic->nameEdit->text());
    }

    if ( ! uiDynamic->qthEdit->text().isEmpty() )
    {
        record.setValue("qth_intl", uiDynamic->qthEdit->text());
    }

    if ( ! uiDynamic->gridEdit->text().isEmpty() )
    {
        record.setValue("gridsquare", uiDynamic->gridEdit->text().toUpper());
    }

    record.setValue("freq", ui->freqTXEdit->value());
    record.setValue("band", ui->bandTXLabel->text());

    record.setValue("freq_rx", ui->freqRXEdit->value());
    record.setValue("band_rx", ui->bandRXLabel->text());

    if ( ! ui->modeEdit->currentText().isEmpty() )
    {
        record.setValue("mode", ui->modeEdit->currentText());
    }

    if ( ! ui->submodeEdit->currentText().isEmpty() )
    {
        record.setValue("submode", ui->submodeEdit->currentText());
    }

    record.setValue("cqz", uiDynamic->cqzEdit->text().toInt());
    record.setValue("ituz", uiDynamic->ituEdit->text().toInt());

    if ( !dxccEntity.country.isEmpty() )
    {
        record.setValue("dxcc", dxccEntity.dxcc);
        record.setValue("country", Data::removeAccents(dxccEntity.country));
        record.setValue("country_intl", dxccEntity.country);
    }

    if ( ! uiDynamic->contEdit->currentText().isEmpty() )
    {
        record.setValue("cont", uiDynamic->contEdit->currentText());
    }

    if ( ! uiDynamic->countyEdit->text().isEmpty() )
    {
        record.setValue("cnty", Data::removeAccents(uiDynamic->countyEdit->text()));
    }

    if ( ! uiDynamic->stateEdit->text().isEmpty() )
    {
        record.setValue("state", Data::removeAccents(uiDynamic->stateEdit->text()));
    }

    if ( ! uiDynamic->iotaEdit->text().isEmpty() )
    {
        record.setValue("iota", Data::removeAccents(uiDynamic->iotaEdit->text().toUpper()));
    }

    if ( ! uiDynamic->sigEdit->text().isEmpty() )
    {
        record.setValue("sig_intl", uiDynamic->sigEdit->text());
    }

    if ( ! uiDynamic->sigInfoEdit->text().isEmpty() )
    {
        record.setValue("sig_info_intl", uiDynamic->sigInfoEdit->text());
    }

    if ( ! ui->qslSentViaBox->currentText().isEmpty() )
    {
        record.setValue("qsl_sent_via", Data::removeAccents(ui->qslSentViaBox->itemData(ui->qslSentViaBox->currentIndex()).toString()));
    }

    if ( coordPrec >= COORD_GRID && !qIsNaN(dxDistance))
    {
        record.setValue("distance", dxDistance);
    }

    if ( !ui->AMLSInfo->text().isEmpty() )
    {
        record.setValue("altitude", ui->AMLSInfo->text().split(" ")[0]);
    }

    if ( !uiDynamic->sotaEdit->text().isEmpty() )
    {
        record.setValue("sota_ref", Data::removeAccents(uiDynamic->sotaEdit->text().toUpper()));
    }

    if ( !uiDynamic->potaEdit->text().isEmpty() )
    {
        record.setValue("pota_ref", Data::removeAccents(uiDynamic->potaEdit->text().toUpper()));
    }

    if ( !uiDynamic->dokEdit->text().isEmpty() )
    {
        record.setValue("darc_dok", Data::removeAccents(uiDynamic->dokEdit->text().toUpper()));
    }

    if ( !uiDynamic->vuccEdit->text().isEmpty() )
    {
        record.setValue("vucc_grids", uiDynamic->vuccEdit->text().toUpper());
    }

    if ( !uiDynamic->wwffEdit->text().isEmpty() )
    {
        record.setValue("wwff_ref", uiDynamic->wwffEdit->text().toUpper());
    }

    if (!uiDynamic->commentEdit->text().isEmpty())
    {
        record.setValue("comment_intl", uiDynamic->commentEdit->text());
    }

    if (!ui->noteEdit->toPlainText().isEmpty())
    {
        record.setValue("notes_intl", ui->noteEdit->toPlainText());
    }

    if (!ui->qslViaEdit->text().isEmpty())
    {
        record.setValue("qsl_via", Data::removeAccents(ui->qslViaEdit->text().toUpper()));
    }

    if (!uiDynamic->ageEdit->text().isEmpty()
        && uiDynamic->ageEdit->text().toInt() > 0 )
    {
        record.setValue("age", uiDynamic->ageEdit->text().toInt());
    }

    if (!uiDynamic->emailEdit->text().isEmpty()) {
        record.setValue("email", Data::removeAccents(uiDynamic->emailEdit->text()));
    }

    if (!uiDynamic->urlEdit->text().isEmpty()) {
        record.setValue("web", Data::removeAccents(uiDynamic->urlEdit->text()));
    }

    AdiFormat::preprocessINTLFields<QSqlRecord>(record);

    addAddlFields(record, profile);

    AdiFormat::preprocessINTLFields<QSqlRecord>(record);

    qCDebug(runtime) << record;

    if ( !model.insertRecord(-1, record) )
    {
        qWarning() << "Cannot insert a record to Contact Table - " << model.lastError();
        qCDebug(runtime) << record;
        return;
    }

    if ( !model.submitAll() )
    {
        qWarning() << "Cannot commit changes to Contact Table - " << model.lastError();
        return;
    }

    /* at this moment, there is no reliable way to get the last ID
     * therefore running SQL with MAX(id) does a good job */
    QSqlQuery tmpQuery;
    if (tmpQuery.exec("SELECT MAX(id) FROM contacts"))
    {
        tmpQuery.next();
        record.insert(0,idField);
        record.setValue("id", tmpQuery.value(0));
        qDebug(runtime)<<"Last Inserted ID: " << tmpQuery.value(0);
    }

    resetContact();

    setNearestSpotColor(ui->nearStationLabel->text());

    emit contactAdded(record);
}

void NewContactWidget::saveExternalContact(QSqlRecord record)
{
    FCT_IDENTIFICATION;

    const QString &savedCallsign = record.value("callsign").toString();

    if ( savedCallsign.isEmpty() ) return;

    changeCallsignManually(savedCallsign);

    QSqlTableModel model;

    model.setTable("contacts");
    QSqlField idField = model.record().field(model.fieldIndex("id"));
    model.removeColumn(model.fieldIndex("id"));

    const DxccEntity &dxcc = Data::instance()->lookupDxcc(savedCallsign);

    if ( dxcc.dxcc != 0 )
    {
        if ( record.value("country_intl").toString().isEmpty()
             && record.value("country").toString().isEmpty() )
            record.setValue("country_intl", dxcc.country);

        if ( record.value("cqz").toString().isEmpty() )
            record.setValue("cqz", dxcc.cqz);

        if ( record.value("ituz").toString().isEmpty() )
            record.setValue("ituz", dxcc.ituz);

        if ( record.value("cont").toString().isEmpty() )
            record.setValue("cont", dxcc.cont);

        if ( record.value("dxcc").toString().isEmpty() )
            record.setValue("dxcc", dxcc.dxcc);
    }

    // add information from callbook if it is a known callsign
    // based on the poll #420, QLog adds more information from callbook
    if ( savedCallsign == ui->callsignEdit->text() )
    {
        // information independent of QTH
        if ( record.value("name_intl").toString().isEmpty()
             && record.value("name").toString().isEmpty()
             && !uiDynamic->nameEdit->text().isEmpty() )
            record.setValue("name_intl", uiDynamic->nameEdit->text());

        if ( record.value("email").toString().isEmpty()
             && !uiDynamic->emailEdit->text().isEmpty() )
            record.setValue("email", uiDynamic->emailEdit->text());

        if ( record.value("qsl_via").toString().isEmpty()
             && !ui->qslViaEdit->text().isEmpty() )
            record.setValue("qsl_via", ui->qslViaEdit->text());

        if ( record.value("web").toString().isEmpty()
             && !uiDynamic->urlEdit->text().isEmpty() )
            record.setValue("web", uiDynamic->urlEdit->text());

        if ( record.value("darc_dok").toString().isEmpty()
             && !uiDynamic->dokEdit->text().isEmpty() )
            record.setValue("darc_dok", uiDynamic->dokEdit->text());

        if ( record.value("pota_ref").toString().isEmpty()
            && !uiDynamic->potaEdit->text().isEmpty())
            record.setValue("pota_ref", uiDynamic->potaEdit->text());

        // information depending on QTH (Grid)
        const QString &savedGrid = record.value("gridsquare").toString();
        if ( savedGrid.startsWith(uiDynamic->gridEdit->text(), Qt::CaseSensitivity::CaseInsensitive)
             || uiDynamic->gridEdit->text().startsWith(savedGrid, Qt::CaseSensitivity::CaseInsensitive ) )
        {
            if ( uiDynamic->gridEdit->text().size() > savedGrid.size() )
                record.setValue("gridsquare", uiDynamic->gridEdit->text());

            if ( record.value("qth_intl").toString().isEmpty()
                 && record.value("qth").toString().isEmpty()
                 && !uiDynamic->qthEdit->text().isEmpty() )
                record.setValue("qth_intl", uiDynamic->qthEdit->text());

            if ( record.value("iota").toString().isEmpty()
                 && !uiDynamic->iotaEdit->text().isEmpty() )
                record.setValue("iota", uiDynamic->iotaEdit->text());

            if ( record.value("cnty").toString().isEmpty()
                 && !uiDynamic->countyEdit->text().isEmpty() )
                record.setValue("cnty", uiDynamic->countyEdit->text());

            if ( record.value("state").toString().isEmpty()
                 && !uiDynamic->stateEdit->text().isEmpty() )
                record.setValue("state", uiDynamic->stateEdit->text());

            // fix ITUz and CQz from callbook, if necessary
            if ( record.value("ituz").toString() != uiDynamic->ituEdit->text() )
                record.setValue("ituz", uiDynamic->ituEdit->text());

            if ( record.value("cqz").toString() != uiDynamic->cqzEdit->text() )
                record.setValue("cqz", uiDynamic->cqzEdit->text());
        }
    }

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();

    AdiFormat::preprocessINTLFields<QSqlRecord>(record);

    addAddlFields(record, profile);

    AdiFormat::preprocessINTLFields<QSqlRecord>(record);

    qCDebug(runtime) << record;

    if ( !model.insertRecord(-1, record) )
    {
        qWarning() << "Cannot insert a record to Contact Table - " << model.lastError();
        qCDebug(runtime) << record;
        return;
    }

    if ( !model.submitAll() )
    {
        qWarning() << "Cannot commit changes to Contact Table - " << model.lastError();
        return;
    }


    /* at this moment, there is no reliable way to get the last ID
     * therefore running SQL with MAX(id) does a good job */
    QSqlQuery tmpQuery;
    if (tmpQuery.exec("SELECT MAX(id) FROM contacts"))
    {
        tmpQuery.next();
        record.insert(0,idField);
        record.setValue("id", tmpQuery.value(0));
        qDebug(runtime)<<"Last Inserted ID: " << tmpQuery.value(0);
    }

    setNearestSpotColor(ui->nearStationLabel->text());

    emit contactAdded(record);
}

void NewContactWidget::startContactTimer()
{
    FCT_IDENTIFICATION;

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    updateTime();

    if ( !isQSOTimeStarted() )
        contactTimer->start(500);
}

void NewContactWidget::stopContactTimer()
{
    FCT_IDENTIFICATION;

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    if ( isQSOTimeStarted() )
        contactTimer->stop();

    updateTimeOff();
}

void NewContactWidget::markContact()
{
    FCT_IDENTIFICATION;

    if ( !ui->callsignEdit->text().isEmpty() )
    {
        DxSpot spot;

        spot.time = QDateTime::currentDateTime().toTimeSpec(Qt::UTC);
        spot.freq = ui->freqRXEdit->value();
        spot.band = BandPlan::freq2Band(spot.freq).name;
        spot.callsign = ui->callsignEdit->text().toUpper();
        emit markQSO(spot);
    }
}

void NewContactWidget::updateTime()
{
    FCT_IDENTIFICATION;

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    timeOff = QDateTime::currentDateTimeUtc();
    ui->dateEdit->setDate(timeOff.date());
    ui->timeOnEdit->setTime(timeOff.time());
    ui->qsoDurationEdit->setTime(QTime(0,0,0));
}

void NewContactWidget::updateTimeOff()
{
    FCT_IDENTIFICATION;

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    static bool shouldHighlighted = true;

    timeOff = QDateTime::currentDateTimeUtc();
    qint64 seconds = ui->timeOnEdit->dateTime().secsTo(timeOff);
    QTime t = QTime(0,0).addSecs(seconds % 86400);
    ui->qsoDurationEdit->setTime(t);

    //QColor(76, 200, 80)
    ui->qsoDurationEdit->setStyleSheet( ( shouldHighlighted && isQSOTimeStarted() ) ? "background-color: #4CC850 ;"
                                                                                    : "");

    shouldHighlighted = ( isQSOTimeStarted() ) ? !shouldHighlighted
                                               : false;

    updatePartnerLocTime();
}

void NewContactWidget::updateCoordinates(double lat, double lon, CoordPrecision prec)
{
    FCT_IDENTIFICATION;

    if (prec < coordPrec) return;

    Gridsquare myGrid(StationProfilesManager::instance()->getProfile(ui->stationProfileCombo->currentText()).locator);
    double distance;
    double bearing;

    if ( myGrid.distanceTo(lat, lon, distance)
         && myGrid.bearingTo(lat, lon, bearing) )
    {
        dxDistance = distance;
        QString unit;
        double showDistance = Gridsquare::distance2localeUnitDistance(dxDistance, unit);
        double LPBearing = bearing - 180;

        if ( LPBearing < 0 )
            LPBearing += 360;

        ui->distanceInfo->setText(QString::number(showDistance, '.', 1) + QString(" %1").arg(unit));
        ui->bearingInfo->setText(QString("%1° (%2: %3°)").arg(QString::number(bearing, '.', 1),
                                                              tr("LP"),
                                                              QString::number(LPBearing, '.', 1)));

        QString partnerTimeZoneString = Data::instance()->getIANATimeZone(lat, lon);

        if ( !partnerTimeZoneString.isEmpty() )
        {
            partnerTimeZone = QTimeZone(partnerTimeZoneString.toUtf8());
        }
        else
        {
            partnerTimeZone = QTimeZone();
        }

        coordPrec = prec;

        updatePartnerLocTime();

        emit newTarget(lat, lon);
    }

}

void NewContactWidget::updateDxccStatus()
{
    FCT_IDENTIFICATION;

    setNearestSpotColor(ui->nearStationLabel->text());

    if ( callsign.isEmpty() )
    {
        ui->dxccStatus->clear();
        ui->callsignEdit->setPalette(QPalette());
        return;
    }

    DxccStatus status = Data::dxccStatus(dxccEntity.dxcc, ui->bandRXLabel->text(), ui->modeEdit->currentText());

    switch (status)
    {
    case DxccStatus::NewEntity:
        ui->dxccStatus->setText(tr("New Entity!"));
        break;
    case DxccStatus::NewBand:
        ui->dxccStatus->setText(tr("New Band!"));
        break;
    case DxccStatus::NewMode:
        ui->dxccStatus->setText(tr("New Mode!"));
        break;
    case DxccStatus::NewBandMode:
        ui->dxccStatus->setText(tr("New Band & Mode!"));
        break;
    case DxccStatus::NewSlot:
        ui->dxccStatus->setText(tr("New Slot!"));
        break;
    case DxccStatus::Worked:
        ui->dxccStatus->setText(tr("Worked"));
        break;
    case DxccStatus::Confirmed:
        ui->dxccStatus->setText(tr("Confirmed"));
        break;
    default:
        ui->dxccStatus->clear();
    }

    QPalette palette;
    palette.setColor(QPalette::Text, Data::statusToColor(status, palette.color(QPalette::Text)));
    ui->callsignEdit->setPalette(palette);

}

void NewContactWidget::updatePartnerLocTime()
{
    FCT_IDENTIFICATION;

    if ( partnerTimeZone.isValid() )
    {
        ui->partnerLocTimeInfo->setText(QDateTime::currentDateTime().toTimeZone(partnerTimeZone).toString(locale.formatTimeLong())
                                        + " (" + getGreeting() +")");
    }
}

/* the function is called when a newcontact frequency spinbox is changed */
void NewContactWidget::frequencyTXChanged()
{
    FCT_IDENTIFICATION;

    double xitFreq = ui->freqTXEdit->value();

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore freq change";
        updateTXBand(xitFreq);
        /* Do not change RX */
        return;
    }


    realRigFreq = xitFreq - RigProfilesManager::instance()->getCurProfile1().xitOffset;
    double ritFreq = (isManualEnterMode) ? ui->freqRXEdit->value()
                                     : realRigFreq + RigProfilesManager::instance()->getCurProfile1().ritOffset;

    __changeFrequency(VFO1, realRigFreq, ritFreq, xitFreq);

    // TODO: qlog should call queryMemberList but for saving time we will omit it. and callsign is usually
    // cleared
    // queryMemberList();

    if ( ! isManualEnterMode )
    {
        qCDebug(runtime) << "rig real freq: " << realRigFreq;
        rig->setFrequency(MHz(realRigFreq));  // set rig frequency
        emit userFrequencyChanged(VFO1, realRigFreq, ritFreq, xitFreq);
    }
    else
    {

    }
}

/* the function is called when a newcontact RX frequecy spinbox is changed */
void NewContactWidget::frequencyRXChanged()
{
    FCT_IDENTIFICATION;

    double ritFreq = ui->freqRXEdit->value();

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore freq change";
        updateRXBand(ritFreq);

        /* Change also TX freq
           on the oposite site, TX change does not change RX */
        ui->freqTXEdit->setValue(ritFreq);
        return;
    }

    realRigFreq = ritFreq - RigProfilesManager::instance()->getCurProfile1().ritOffset;
    double xitFreq = realRigFreq + RigProfilesManager::instance()->getCurProfile1().xitOffset;

    __changeFrequency(VFO1, realRigFreq, ritFreq, xitFreq);

    qCDebug(runtime) << "rig real freq: " << realRigFreq;
    rig->setFrequency(MHz(realRigFreq));  // set rig frequency
    emit userFrequencyChanged(VFO1, realRigFreq, ritFreq, xitFreq);
}

/* the function is called when rig freq is changed */
/* Received from RIG */
void NewContactWidget::changeFrequency(VFOID vfoid, double vfoFreq, double ritFreq, double xitFreq)
{
    FCT_IDENTIFICATION;

    realFreqForManualExit = vfoFreq;

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }
    __changeFrequency (vfoid, vfoFreq, ritFreq, xitFreq);
}

void NewContactWidget::changeModeWithoutSignals(const QString &mode, const QString &subMode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode << subMode;

    if ( ui->modeEdit->currentText() == mode)
    {
        qCDebug(runtime) << "Mode did not change, changing submode";

        ui->submodeEdit->blockSignals(true);
        ui->submodeEdit->setCurrentText(subMode);
        ui->submodeEdit->blockSignals(false);
        return;
    }

    qCDebug(runtime) << "Mode changed - updating submode list";

    ui->modeEdit->blockSignals(true);
    ui->submodeEdit->blockSignals(true);
    ui->modeEdit->setCurrentText(mode);
    __modeChanged();
    ui->submodeEdit->setCurrentText(subMode);
    ui->submodeEdit->blockSignals(false);
    ui->modeEdit->blockSignals(false);
}

void NewContactWidget::showRXTXFreqs(bool enable)
{
    FCT_IDENTIFICATION;

    ui->freqTXEdit->setVisible(enable);
    ui->bandTXLabel->setVisible(enable);
    ui->freqRXLabel->setVisible(enable);
    ui->freqTXLabel->setVisible(enable);
}

/* Generic function to change freq */
void NewContactWidget::__changeFrequency(VFOID, double vfoFreq, double ritFreq, double xitFreq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << vfoFreq << " " << ritFreq << " " << xitFreq;

    QSYContactWiping(ritFreq);

    realRigFreq = vfoFreq;

    ui->freqTXEdit->blockSignals(true);
    ui->freqTXEdit->setValue(xitFreq);
    updateTXBand(xitFreq);
    ui->freqTXEdit->blockSignals(false);

    ui->freqRXEdit->blockSignals(true);
    ui->freqRXEdit->setValue(ritFreq);
    updateRXBand(ritFreq);
    ui->freqRXEdit->blockSignals(false);

    showRXTXFreqs(( ritFreq != xitFreq
                    || RigProfilesManager::instance()->getCurProfile1().ritOffset != 0.0
                    || RigProfilesManager::instance()->getCurProfile1().xitOffset != 0.0
                    || isManualEnterMode ));
}

/* Power is changed from RIG */
/* Received from RIG */
void NewContactWidget::changePower(VFOID, double power)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << power;

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    ui->powerEdit->blockSignals(true);
    ui->powerEdit->setValue(power);
    ui->powerEdit->blockSignals(false);
}

/* connection slot */
/* received from RIG */
void NewContactWidget::rigConnected()
{
    FCT_IDENTIFICATION;

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    RigProfile currProfile = RigProfilesManager::instance()->getCurProfile1();

    /* allow modify PWR only in case when Rig is not connected or user
     * does not want to get PWR from RIG */
    if ( currProfile.getPWRInfo )
    {
        ui->powerEdit->setEnabled(false);
        ui->powerEdit->setValue(0.0);
    }
    else
    {
        ui->powerEdit->setEnabled(true);
        ui->powerEdit->setValue(currProfile.defaultPWR);
    }

    rigOnline = true;
}

/* disconnection slot */
/* received from RIG */
void NewContactWidget::rigDisconnected()
{
    FCT_IDENTIFICATION;

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    ui->powerEdit->setEnabled(true);
    ui->powerEdit->setValue(RigProfilesManager::instance()->getCurProfile1().defaultPWR);

    rigOnline = false;
}

void NewContactWidget::nearestSpot(const DxSpot &spot)
{
    FCT_IDENTIFICATION;

    setNearestSpotColor(spot.callsign);
}

void NewContactWidget::setNearestSpotColor(const QString &call)
{
    FCT_IDENTIFICATION;

    if ( call.isEmpty() )
    {
        ui->nearStationLabel->setText(call);
        return;
    }

    QPalette palette;

    const DxccEntity &spotEntity = Data::instance()->lookupDxcc(call);
    const DxccStatus &status = Data::dxccStatus(spotEntity.dxcc,
                                                ui->bandRXLabel->text(),
                                                ui->modeEdit->currentText());
    palette.setColor(QPalette::WindowText,
                     Data::statusToColor(status,
                                         palette.color(QPalette::Text)));
    ui->nearStationLabel->setPalette(palette);
    ui->nearStationLabel->setText(call);
}

void NewContactWidget::setManualMode(bool isEnabled)
{
    FCT_IDENTIFICATION;

    bool isExitManualMode = ! isEnabled && isManualEnterMode;

    if ( isEnabled && rigOnline )
    {
        rigDisconnected();
    }

    isManualEnterMode = isEnabled;

    if ( isExitManualMode )
    {
        realRigFreq = realFreqForManualExit;
        exitManualMode();
        showRXTXFreqs((RigProfilesManager::instance()->getCurProfile1().ritOffset != 0.0
                       || RigProfilesManager::instance()->getCurProfile1().xitOffset != 0.0));
    }
    else
    {
        realFreqForManualExit = realRigFreq;
        resetContact();
        showRXTXFreqs(true);
        ui->dateEdit->setReadOnly(false);
        ui->timeOnEdit->setReadOnly(false);
        ui->qsoDurationEdit->setReadOnly(false);
        ui->timeOnEdit->setFocusPolicy(Qt::StrongFocus);
        ui->dateEdit->setFocusPolicy(Qt::StrongFocus);
        ui->qsoDurationEdit->setFocusPolicy(Qt::StrongFocus);
        ui->qsoDurationEdit->setCurrentSection(QDateTimeEdit::MinuteSection);
        ui->thirdLineWidget->setTabOrder(ui->dateEdit, ui->timeOnEdit);
        ui->thirdLineWidget->setTabOrder(ui->timeOnEdit, ui->qsoDurationEdit);
    }

    QString styleString = (isManualEnterMode) ? "background-color: orange;"
                                              : "";

    ui->modeLabel->setStyleSheet(styleString);
    ui->frequencyLabel->setStyleSheet(styleString);
    ui->dateLabel->setStyleSheet(styleString);
    ui->timeOnLabel->setStyleSheet(styleString);
    ui->qsoDurationLabel->setStyleSheet(styleString);
    ui->stationProfileLabel->setStyleSheet(styleString);
    ui->rigLabel->setStyleSheet(styleString);
    ui->antennaLabel->setStyleSheet(styleString);
    ui->powerLabel->setStyleSheet(styleString);
}

void NewContactWidget::exitManualMode()
{
    FCT_IDENTIFICATION;

    // set date/time
    // clear form
    resetContact();

    ui->dateEdit->setReadOnly(true);
    ui->timeOnEdit->setReadOnly(true);
    ui->qsoDurationEdit->setReadOnly(true);
    ui->timeOnEdit->setFocusPolicy(Qt::ClickFocus);
    ui->dateEdit->setFocusPolicy(Qt::ClickFocus);
    ui->qsoDurationEdit->setFocusPolicy(Qt::ClickFocus);

    //rig connected/disconnected
    if ( rig->isRigConnected() )
    {
        rigConnected();
        // set mode/submode
        // set frequency
        rig->sendState(); //resend rig state via signals
    }
    else
    {
        rigDisconnected();
    }

    // reset my profiles
    refreshRigProfileCombo();
    refreshAntProfileCombo();
    refreshStationProfileCombo();
}

void NewContactWidget::setupCustomUi()
{
    FCT_IDENTIFICATION;

    // Clear Custom Lines
    QList<QHBoxLayout *> customUiRows = ui->customLayout->findChildren<QHBoxLayout *>();
    for ( auto &rowLayout : qAsConst(customUiRows) )
    {        
        qCDebug(runtime) << "Removing objects from " << rowLayout->objectName();

        QLayoutItem *rowItem;
        while ( (rowItem = rowLayout->takeAt(0)) != nullptr )
        {
            if ( rowItem->widget() != nullptr)
            {
                qCDebug(runtime) << "Removing widget" << rowItem->widget()->objectName();
                rowItem->widget()->removeEventFilter(this); // only row fields has Special Event Filter  (enter handling)
                rowItem->widget()->setHidden(true);
            }
        }
    }

    // Clear Detail Columns
    QList<QFormLayout *> detailColumns;
    detailColumns << ui->detailColA << ui->detailColB << ui->detailColC;

    for ( QFormLayout * layout : qAsConst(detailColumns) )
    {
        qCDebug(runtime) << "Removing" << layout->rowCount() <<"object(s) from" << layout->objectName();

        int rows = layout->rowCount();

        for ( int i = 0 ; i < rows; i++ )
        {
            if ( layout == ui->detailColC && i < 4 )
            {
                qCDebug(runtime) << "Skipping row" << i << "because static content";
                continue;
            }

            qCDebug(runtime) << "Deleting row" << i;
            QFormLayout::TakeRowResult result = layout->takeRow((layout == ui->detailColC) ? 4 : 0);

            if ( result.labelItem && result.fieldItem )
            {
                qCDebug(runtime) << "Removing Widgets" << result.labelItem->widget()->objectName()
                                 << result.fieldItem->widget()->objectName();
                result.labelItem->widget()->setHidden(true);
                result.fieldItem->widget()->setHidden(true);
            }
            else
            {
                qCDebug(runtime) << "Row is empty";
            }
        }
    }

    MainLayoutProfile layoutProfile = MainLayoutProfilesManager::instance()->getCurProfile1();
    QList<QWidget *> addedWidgets;

    // Empty Profile means Classic Layout
    if ( layoutProfile == MainLayoutProfile() )
        layoutProfile = MainLayoutProfile::getClassicLayout();

    addedWidgets << setupCustomUiRow(ui->customRowALayout, layoutProfile.rowA);
    addedWidgets << setupCustomUiRow(ui->customRowBLayout, layoutProfile.rowB);

    setupCustomUiRowsTabOrder(addedWidgets);

    setupCustomDetailColumn(ui->detailColA, layoutProfile.detailColA);
    setupCustomDetailColumn(ui->detailColB, layoutProfile.detailColB);
    setupCustomDetailColumn(ui->detailColC, layoutProfile.detailColC);

    ui->qsoTabs->adjustSize();
    update();
}

QList<QWidget *> NewContactWidget::setupCustomUiRow(QHBoxLayout *row, const QList<int>& widgetsList)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << row->objectName() << widgetsList;

    QWidget *currCustomWidget = nullptr;
    QList<QWidget *> ret;

    for ( int widgetID : widgetsList )
    {
        currCustomWidget = uiDynamic->getRowWidget(widgetID);
        if ( !currCustomWidget )
        {
            qWarning() << "Missing fieldIndex2WidgetMapping for index" << widgetID;
            continue;
        }

        currCustomWidget->installEventFilter(this); // only row fields have a special event filter (enter handling)
        qCDebug(runtime) << "Adding widget" << currCustomWidget->objectName();
        row->addWidget(currCustomWidget);
        ret << currCustomWidget;
    }
    return ret;
}

QList<QWidget *> NewContactWidget::setupCustomDetailColumn(QFormLayout *column, const QList<int> &widgetsList)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << column->objectName() << widgetsList;

    QWidget *currCustomLabel = nullptr;
    QWidget *currCustomEditor = nullptr;
    QList<QWidget *> ret;

    for ( int widgetID : widgetsList )
    {
        currCustomLabel = uiDynamic->getLabel(widgetID);
        currCustomEditor = uiDynamic->getEditor(widgetID);

        if ( !currCustomLabel || !currCustomEditor )
        {
            qWarning() << "Missing fieldIndex2WidgetMapping for index" << widgetID;
            continue;
        }

        qCDebug(runtime) << "Adding widget" << currCustomEditor->objectName();
        column->addRow(currCustomLabel, currCustomEditor);
        ret << currCustomEditor;
    }
    return ret;
}

void NewContactWidget::setupCustomUiRowsTabOrder(const QList<QWidget *> &customWidgets)
{
    FCT_IDENTIFICATION;

    QWidget *prevCustomWidget = nullptr;

    for ( QWidget *currentWidget : customWidgets )
    {
        if ( prevCustomWidget )
        {
            QWidget *fromWidget = prevCustomWidget->findChild<NewContactEditLine*>();
            if ( !fromWidget )
                fromWidget = prevCustomWidget->findChild<QComboBox*>();

            QWidget *toWidget = currentWidget->findChild<NewContactEditLine*>();
            if ( !toWidget )
                toWidget = currentWidget->findChild<QComboBox*>();

            if ( fromWidget && toWidget )
            {
                //ui->customLayoutWidget->setTabOrder(fromWidget, toWidget);
                setTabOrder(fromWidget, toWidget);
            }
        }
        else
        {
            QWidget *toWidget = currentWidget->findChild<NewContactEditLine*>();
            if ( !toWidget )
                toWidget = currentWidget->findChild<QComboBox*>();

            if ( toWidget )
            {
                setTabOrder(ui->rstRcvdEdit, toWidget);
            }
        }
        prevCustomWidget = currentWidget;
    }
    setTabOrder(prevCustomWidget, ui->callsignEdit);
}

void NewContactWidget::setBandLabel(const QString &band)
{
    FCT_IDENTIFICATION;

    ui->bandRXLabel->setText(band);
}

void NewContactWidget::updateSatMode()
{
    FCT_IDENTIFICATION;

    if ( Data::instance()->propagationModeTextToID(ui->propagationModeEdit->currentText()) != "SAT")
        return;

    uiDynamic->satModeEdit->setCurrentText(Data::instance()->satModeIDToText(( bandTX.satDesignator.isEmpty()
                                                                               || bandRX.satDesignator.isEmpty() ) ? ""
                                                                                                                   : bandTX.satDesignator + bandRX.satDesignator));
}

void NewContactWidget::tuneDx(const QString &callsign,
                              double frequency,
                              const BandPlan::BandPlanMode bandPlanMode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign<< frequency << bandPlanMode;

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    if ( frequency > 0.0 )
    {
        QString subMode;
        QString mode = BandPlan::bandPlanMode2ExpectedMode(bandPlanMode,
                                                           subMode);

        if ( mode.isEmpty() )
        {
            qCDebug(runtime) << "mode not found" << bandPlanMode;
            mode = BandPlan::freq2ExpectedMode(frequency,
                                               subMode);
        }

        if ( !mode.isEmpty() )
        {
            // in case of SSB, do not sent 2 mode changes to rig
            // therefore change Mode without signals and then set the
            // final mode
            changeModeWithoutSignals(mode, subMode);
            if ( mode == "FT8" )
            {
                // if rig is connected then FT8 mode is overwrotten by rit
                // but it the rig is not connected then mode contains a correct
                // mode
                rig->setMode("SSB", "USB");
            }
            else
            {
                rig->setMode(ui->modeEdit->currentText(), ui->submodeEdit->currentText());
            }
            emit userModeChanged(VFO1, QString(), mode, subMode, bandwidthFilter);
        }
    }
    else
    {
        frequency = ui->freqRXEdit->value();
    }

    ui->freqRXEdit->setValue(frequency);
    resetContact();
    changeCallsignManually(callsign, frequency);
}

void NewContactWidget::fillCallsignGrid(const QString &callsign, const QString &grid)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << callsign<< grid;
    tuneDx(callsign, -1, BandPlan::BAND_MODE_UNKNOWN);
    uiDynamic->gridEdit->setText(grid);
}

void NewContactWidget::showDx(const QString &callsign, const QString &grid)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<callsign<< " " << grid;

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    resetContact();
    changeCallsignManually(callsign);
    uiDynamic->gridEdit->setText(grid);
}

void NewContactWidget::setDefaultReport() {
    FCT_IDENTIFICATION;

    if (defaultReport.isEmpty()) {
        defaultReport = "";
    }

    ui->rstRcvdEdit->setText(defaultReport);
    ui->rstSentEdit->setText(defaultReport);
}

void NewContactWidget::webLookup()
{
    FCT_IDENTIFICATION;

    if ( !callsign.isEmpty() )
        QDesktopServices::openUrl(GenericCallbook::getWebLookupURL(callsign));
}

void NewContactWidget::refreshSIGCompleter()
{
    FCT_IDENTIFICATION;

    QStringListModel *model = static_cast<QStringListModel*>(sigCompleter->model());

    if( !model )
        model = new QStringListModel();

    model->setStringList(Data::instance()->sigIDList());
    sigCompleter->setModel(model);
}

QString NewContactWidget::getCallsign() const
{
    FCT_IDENTIFICATION;

    return ui->callsignEdit->text();
}

QString NewContactWidget::getName() const
{
    FCT_IDENTIFICATION;

    return uiDynamic->nameEdit->text();
}

QString NewContactWidget::getRST() const
{
    FCT_IDENTIFICATION;

    return ui->rstSentEdit->text();
}

QString NewContactWidget::getGreeting() const
{
    FCT_IDENTIFICATION;

    QString greeting(tr("GE"));

    if ( partnerTimeZone.isValid() )
    {

        QDateTime currPartnerTime = QDateTime::currentDateTime().toTimeZone(partnerTimeZone);

        if ( currPartnerTime.time().hour() >= 5
             && currPartnerTime.time().hour() < 12 )
        {
            greeting = tr("GM");

        }
        else if ( currPartnerTime.time().hour() >=12
                  && currPartnerTime.time().hour() < 18 )
        {
            greeting = tr("GA");

        }
    }
    return greeting;
}

QString NewContactWidget::getMyCallsign() const
{
    FCT_IDENTIFICATION;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();
    return profile.callsign;
}

QString NewContactWidget::getMyName() const
{
    FCT_IDENTIFICATION;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();
    return profile.operatorName;
}

QString NewContactWidget::getMyQTH() const
{
    FCT_IDENTIFICATION;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();
    return profile.qthName;
}

QString NewContactWidget::getMyLocator() const
{
    FCT_IDENTIFICATION;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();
    return profile.locator;
}

QString NewContactWidget::getMySIG() const
{
    FCT_IDENTIFICATION;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();
    return profile.sig;
}

QString NewContactWidget::getMySIGInfo() const
{
    FCT_IDENTIFICATION;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();
    return profile.sigInfo;
}

QString NewContactWidget::getMyIOTA() const
{
    FCT_IDENTIFICATION;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();
    return profile.iota;
}

QString NewContactWidget::getMySOTA() const
{
    FCT_IDENTIFICATION;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();
    return profile.sota;
}

QString NewContactWidget::getMyPOTA() const
{
    FCT_IDENTIFICATION;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();
    return profile.pota;
}

QString NewContactWidget::getMyWWFT() const
{
    FCT_IDENTIFICATION;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();
    return profile.wwff;
}

QString NewContactWidget::getMyVUCC() const
{
    FCT_IDENTIFICATION;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();
    return profile.vucc;
}

QString NewContactWidget::getMyPWR() const
{
    FCT_IDENTIFICATION;

    return QString::number(ui->powerEdit->value(), 'f');
}

QString NewContactWidget::getBand() const
{
    FCT_IDENTIFICATION;

    return ui->bandRXLabel->text();
}

QString NewContactWidget::getMode() const
{
    FCT_IDENTIFICATION;

    return ui->modeEdit->currentText();
}

double NewContactWidget::getQSOBearing() const
{
    FCT_IDENTIFICATION;


    const QString &bearingString = ui->bearingInfo->text();
    return ( !bearingString.isEmpty() ? bearingString.mid(0,bearingString.indexOf("°")).toDouble()
                                      : qQNaN());
}

double NewContactWidget::getQSODistance() const
{
    FCT_IDENTIFICATION;

    return dxDistance;
}

void NewContactWidget::propModeChanged(const QString &propModeText)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "propModeText: " << propModeText << " mode: "<< Data::instance()->propagationModeIDToText("SAT");
    if ( propModeText == Data::instance()->propagationModeIDToText("SAT") )
    {
        uiDynamic->satNameEdit->setText(settings.value("newcontact/satname", QString()).toString());
        updateSatMode();
        uiDynamic->satModeEdit->setEnabled(true);
        uiDynamic->satNameEdit->setEnabled(true);
    }
    else
    {
        uiDynamic->satModeEdit->setCurrentIndex(-1);
        uiDynamic->satNameEdit->clear();
        uiDynamic->satModeEdit->setEnabled(false);
        uiDynamic->satNameEdit->setEnabled(false);
    }
}

void NewContactWidget::stationProfileComboChanged(const QString &profileName)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << profileName;

    // My Grid change
    gridChanged();

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    StationProfilesManager::instance()->setCurProfile1(profileName);

    emit stationProfileChanged();

    // recalculate all stats
    queryDxcc(ui->callsignEdit->text().toUpper());
}

void NewContactWidget::rigProfileComboChanged(const QString &profileName)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << profileName;

    // set just power from the new profile
    ui->powerEdit->setValue(RigProfilesManager::instance()->getProfile(profileName).defaultPWR);

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    RigProfilesManager::instance()->setCurProfile1(profileName);

    ui->freqRXEdit->setValue(realRigFreq + RigProfilesManager::instance()->getCurProfile1().ritOffset);
    ui->freqTXEdit->setValue(realRigFreq + RigProfilesManager::instance()->getCurProfile1().xitOffset);

    emit rigProfileChanged();
}

void NewContactWidget::antProfileComboChanged(const QString &profileName)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << profileName;

    if ( isManualEnterMode )
    {
        qCDebug(runtime) << "Manual mode enabled - ignore event";
        return;
    }

    AntProfilesManager::instance()->setCurProfile1(profileName);

    emit antProfileChanged();
}

void NewContactWidget::sotaChanged(const QString &newSOTA)
{
    FCT_IDENTIFICATION;

    if ( newSOTA.length() >= 3 )
    {
        uiDynamic->sotaEdit->setCompleter(sotaCompleter);
    }
    else
    {
        uiDynamic->sotaEdit->setCompleter(nullptr);
    }

    if ( uiDynamic->qthEdit->text() == lastSOTA.summitName )
    {
        uiDynamic->qthEdit->clear();
    }

    Gridsquare SOTAGrid(lastSOTA.latitude, lastSOTA.longitude);
    if ( uiDynamic->gridEdit->text() == SOTAGrid.getGrid() )
    {
        uiDynamic->gridEdit->clear();
    }

    ui->AMLSInfo->clear();
}

bool NewContactWidget::isSOTAValid(SOTAEntity *entity)
{
    FCT_IDENTIFICATION;

    if ( uiDynamic->sotaEdit->text().isEmpty() )
        return false;

    SOTAEntity sotaInfo = Data::instance()->lookupSOTA(uiDynamic->sotaEdit->text());
    if ( entity ) *entity = sotaInfo;
    return ( sotaInfo.summitCode.toUpper() == uiDynamic->sotaEdit->text().toUpper()
             && !sotaInfo.summitName.isEmpty());
}

void NewContactWidget::sotaEditFinished()
{
    FCT_IDENTIFICATION;

    SOTAEntity sotaInfo;

    if ( isSOTAValid(&sotaInfo) )
    {
        uiDynamic->qthEdit->setText(sotaInfo.summitName);
        Gridsquare SOTAGrid(sotaInfo.latitude, sotaInfo.longitude);
        if ( SOTAGrid.isValid() )
        {
            uiDynamic->gridEdit->setText(SOTAGrid.getGrid());
        }
        ui->AMLSInfo->setText(QString::number(sotaInfo.altm) + tr(" m"));
        lastSOTA = sotaInfo;
    }
    else if ( isPOTAValid(nullptr) )
    {
        potaEditFinished();
    }
    else if ( isWWFFValid(nullptr) )
    {
        wwffEditFinished();
    } 
}

void NewContactWidget::potaChanged(const QString &newPOTA)
{
    FCT_IDENTIFICATION;

    if ( newPOTA.length() >= 3 )
    {
        uiDynamic->potaEdit->setCompleter(potaCompleter);
    }
    else
    {
        uiDynamic->potaEdit->setCompleter(nullptr);
    }

    if ( uiDynamic->qthEdit->text() == lastPOTA.name )
    {
        uiDynamic->qthEdit->clear();
    }

    Gridsquare POTAGrid(lastPOTA.grid);

    if ( uiDynamic->gridEdit->text() == POTAGrid.getGrid() )
    {
        uiDynamic->gridEdit->clear();
    }
}

bool NewContactWidget::isPOTAValid(POTAEntity *entity)
{
    FCT_IDENTIFICATION;

    if ( uiDynamic->potaEdit->text().isEmpty() )
        return false;

    const QStringList &potaList = uiDynamic->potaEdit->text().split("@");

    QString potaString;

    if ( potaList.size() > 0 )
    {
        potaString = potaList[0];
    }
    else
    {
        potaString = uiDynamic->potaEdit->text();
    }

    POTAEntity potaInfo = Data::instance()->lookupPOTA(potaString);

    if ( entity ) *entity = potaInfo;
    return (potaInfo.reference.toUpper() == potaString.toUpper()
            && !potaInfo.name.isEmpty());
}

void NewContactWidget::potaEditFinished()
{
    FCT_IDENTIFICATION;

    POTAEntity potaInfo;

    if ( isPOTAValid(&potaInfo) )
    {
        uiDynamic->qthEdit->setText(potaInfo.name);
        Gridsquare POTAGrid(potaInfo.grid);
        if ( POTAGrid.isValid() )
        {
            uiDynamic->gridEdit->setText(POTAGrid.getGrid());
        }
        lastPOTA = potaInfo;
    }
    else if ( isSOTAValid(nullptr) )
    {
        sotaEditFinished();
    }
    else if ( isWWFFValid(nullptr) )
    {
        wwffEditFinished();
    }
}

bool NewContactWidget::isWWFFValid(WWFFEntity *entity)
{
    FCT_IDENTIFICATION;


    if ( uiDynamic->wwffEdit->text().isEmpty() )
        return false;

    WWFFEntity wwffInfo = Data::instance()->lookupWWFF(uiDynamic->wwffEdit->text());

    if ( entity ) *entity = wwffInfo;

    return (wwffInfo.reference.toUpper() == uiDynamic->wwffEdit->text().toUpper()
            && !wwffInfo.name.isEmpty());
}

void NewContactWidget::wwffEditFinished()
{
    FCT_IDENTIFICATION;

    WWFFEntity wwffInfo;

    if ( isWWFFValid(&wwffInfo) )
    {
        uiDynamic->qthEdit->setText(wwffInfo.name);
        if ( ! wwffInfo.iota.isEmpty()
             && wwffInfo.iota != "-" )
        {
            uiDynamic->iotaEdit->setText(wwffInfo.iota.toUpper());
        }
        uiDynamic->gridEdit->setText(QString()); // WWFF's Grid is unrealiable information
        lastWWFF = wwffInfo;
    }
    else if ( isSOTAValid(nullptr) )
    {
        sotaEditFinished();
    }
    else if ( isPOTAValid(nullptr) )
    {
        potaEditFinished();
    }
}

void NewContactWidget::wwffChanged(const QString &newWWFF)
{
    FCT_IDENTIFICATION;

    if ( newWWFF.length() >= 3 )
    {
        uiDynamic->wwffEdit->setCompleter(wwffCompleter);
    }
    else
    {
        uiDynamic->wwffEdit->setCompleter(nullptr);
    }

    if ( uiDynamic->qthEdit->text() == lastWWFF.name )
    {
        uiDynamic->qthEdit->clear();
        uiDynamic->gridEdit->clear();
    }
}

void NewContactWidget::formFieldChangedString(const QString &)
{
    FCT_IDENTIFICATION;

    QSOFreq = ui->freqRXEdit->value();
}

void NewContactWidget::formFieldChanged()
{
    FCT_IDENTIFICATION;

    formFieldChangedString(QString());
}

void NewContactWidget::useNearestCallsign()
{
    FCT_IDENTIFICATION;

    updateTime();
    changeCallsignManually(ui->nearStationLabel->text());
    ui->callsignEdit->setFocus();
}

void NewContactWidget::setCallbookStatusEnabled(bool callbookEnabled)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callbookEnabled;

    if ( callbookEnabled )
    {
        ui->callbookStatusButton->setIcon((callbookSearchPaused) ? QIcon(":/icons/search-globe_orange.svg")
                                                                 : QIcon(":/icons/search-globe_green.svg"));
    }
    else
    {
        callbookSearchPaused = false;
        ui->callbookStatusButton->setIcon(QIcon(":/icons/search-globe_red.svg"));
    }

    if ( !callbookEnabled || callbookSearchPaused )
    {
        ui->callbookStatusButton->setToolTip(tr("Callbook search is inactive"));
        ui->callsignEdit->installEventFilter(this);
    }
    else
    {
        ui->callbookStatusButton->setToolTip(tr("Callbook search is active"));
        ui->callsignEdit->removeEventFilter(this);
    }
}

void NewContactWidget::changeCallbookSearchStatus()
{
    FCT_IDENTIFICATION;

    callbookSearchPaused = !callbookSearchPaused;
    setCallbookStatusEnabled(callbookManager.isActive());
}

void NewContactWidget::satNameChanged()
{
    FCT_IDENTIFICATION;

    if ( Data::instance()->propagationModeTextToID(ui->propagationModeEdit->currentText()) == "SAT")
        settings.setValue("newcontact/satname", uiDynamic->satNameEdit->text());
}

NewContactWidget::~NewContactWidget() {
    FCT_IDENTIFICATION;

    writeWidgetSetting();
    delete ui;
}

void NewContactWidget::assignPropConditions(PropConditions *cond)
{
    FCT_IDENTIFICATION;
    prop_cond = cond;
}

void NewContactWidget::changeCallsignManually(const QString &callsign)
{
    FCT_IDENTIFICATION;

    changeCallsignManually(callsign, ui->freqRXEdit->value());
}


void NewContactWidget::changeCallsignManually(const QString &callsign, double freq)
{
    FCT_IDENTIFICATION;

    QSOFreq = freq; // Important !!! - to prevent QSY Contact Reset when the frequency is set
    ui->callsignEdit->setText(callsign);
    ui->callsignEdit->end(false);
    callsignChanged();
    editCallsignFinished();
    stopContactTimer();
}

NewContactDynamicWidgets::NewContactDynamicWidgets(bool allocateWidgets,
                                                   QWidget *parent) :
    parent(parent),
    logbookmodel(new LogbookModel()),
    widgetsAllocated(allocateWidgets)
{

    initializeWidgets(LogbookModel::COLUMN_NAME_INTL, "name", nameLabel, nameEdit);
    initializeWidgets(LogbookModel::COLUMN_QTH_INTL, "qth", qthLabel, qthEdit);
    initializeWidgets(LogbookModel::COLUMN_GRID, "grid", gridLabel, gridEdit);
    initializeWidgets(LogbookModel::COLUMN_COMMENT_INTL, "comment", commentLabel, commentEdit);
    initializeWidgets(LogbookModel::COLUMN_CONTINENT, "cont", contLabel, contEdit);
    initializeWidgets(LogbookModel::COLUMN_ITUZ, "itu", ituLabel, ituEdit);
    initializeWidgets(LogbookModel::COLUMN_CQZ, "cqz", cqzLabel, cqzEdit);
    initializeWidgets(LogbookModel::COLUMN_STATE, "state", stateLabel, stateEdit);
    initializeWidgets(LogbookModel::COLUMN_COUNTY, "county", countyLabel, countyEdit);
    initializeWidgets(LogbookModel::COLUMN_AGE, "age", ageLabel, ageEdit);
    initializeWidgets(LogbookModel::COLUMN_VUCC_GRIDS, "vucc", vuccLabel, vuccEdit);
    initializeWidgets(LogbookModel::COLUMN_DARC_DOK, "dok", dokLabel, dokEdit);
    initializeWidgets(LogbookModel::COLUMN_IOTA, "iota", iotaLabel, iotaEdit);
    initializeWidgets(LogbookModel::COLUMN_POTA_REF, "pota", potaLabel, potaEdit);
    initializeWidgets(LogbookModel::COLUMN_SOTA_REF, "sota", sotaLabel, sotaEdit);
    initializeWidgets(LogbookModel::COLUMN_WWFF_REF, "wwff", wwffLabel, wwffEdit);
    initializeWidgets(LogbookModel::COLUMN_SIG_INTL, "sig", sigLabel, sigEdit);
    initializeWidgets(LogbookModel::COLUMN_SIG_INFO_INTL, "sigInfo", sigInfoLabel, sigInfoEdit);
    initializeWidgets(LogbookModel::COLUMN_EMAIL, "email", emailLabel, emailEdit);
    initializeWidgets(LogbookModel::COLUMN_WEB, "url", urlLabel, urlEdit);
    initializeWidgets(LogbookModel::COLUMN_SAT_NAME, "satName", satNameLabel, satNameEdit);
    initializeWidgets(LogbookModel::COLUMN_SAT_MODE, "satMode", satModeLabel, satModeEdit);

    if ( allocateWidgets )
    {
        nameEdit->setMaxLength(50);

        qthEdit->setMaxLength(75);

        gridEdit->setMaximumSize(QSize(100, 16777215));
        gridEdit->setMaxLength(10);
        gridEdit->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Maximum);
        gridEdit->setValidator(new QRegularExpressionValidator(Gridsquare::gridRegEx(), gridEdit));
        gridEdit->spaceForbidden(true);

        contEdit->setMaximumSize(QSize(50, 16777215));
        contEdit->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        contEdit->addItem(QString());
        contEdit->addItem(QString("AF"));
        contEdit->addItem(QString("AN"));
        contEdit->addItem(QString("AS"));
        contEdit->addItem(QString("EU"));
        contEdit->addItem(QString("NA"));
        contEdit->addItem(QString("OC"));
        contEdit->addItem(QString("SA"));

        ituEdit->setMaximumSize(QSize(40, 16777215));
        ituEdit->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Maximum);
        ituEdit->setMaxLength(2);
        ituEdit->setValidator(new QIntValidator(Data::getITUZMin(), Data::getITUZMax(), ituEdit));
        ituEdit->spaceForbidden(true);

        cqzEdit->setMaximumSize(QSize(40, 16777215));
        cqzEdit->setMaxLength(2);
        cqzEdit->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Maximum);
        cqzEdit->setValidator(new QIntValidator(Data::getCQZMin(), Data::getCQZMax(), cqzEdit));
        cqzEdit->spaceForbidden(true);

        //stateEdit->setMaximumSize(QSize(200, 16777215));

        //countyEdit->setMaximumSize(QSize(200, 16777215));

        //ageEdit->setMaximumSize(QSize(50, 16777215));
        ageEdit->setMaximumSize(QSize(40, 16777215));
        ageEdit->setMaxLength(3);
        ageEdit->setValidator(new QIntValidator(0, 999, ageEdit));
        ageEdit->spaceForbidden(true);

        //vuccEdit->setMaximumSize(QSize(200, 16777215));
        vuccEdit->setValidator(new QRegularExpressionValidator(Gridsquare::gridVUCCRegEx(), vuccEdit));
        vuccEdit->setToolTip(QCoreApplication::translate("NewContactWidget", "two or four adjacent Maidenhead grid locators, each four characters long, (ex. EN98,FM08,EM97,FM07)", nullptr));

        //dokEdit->setMaximumSize(QSize(200, 16777215));
        dokEdit->setToolTip(QCoreApplication::translate("NewContactWidget", "the contacted station's DARC DOK (District Location Code) (ex. A01)", nullptr));

        //iotaEdit->setMaximumSize(QSize(200, 16777215));
        QCompleter *iotaCompleter = new QCompleter(Data::instance()->iotaIDList(), iotaEdit);
        iotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        iotaCompleter->setFilterMode(Qt::MatchContains);
        iotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
        iotaEdit->setCompleter(iotaCompleter);
        iotaEdit->spaceForbidden(true);

        //potaEdit->setMaximumSize(QSize(200, 16777215));
        //potaEdit->setFocusPolicy(Qt::ClickFocus);
        //it has an external completer

        //sotaEdit->setMaximumSize(QSize(200, 16777215));
        //sotaEdit->setFocusPolicy(Qt::ClickFocus);
        //it has an external completer

        //wwffEdit->setMaximumSize(QSize(200, 16777215));
        //wwffEdit->setFocusPolicy(Qt::ClickFocus);
        wwffEdit->setToolTip(QCoreApplication::translate("NewContactWidget", "World Wide Flora & Fauna", nullptr));

        //sigEdit->setMaximumSize(QSize(200, 16777215));
        //sigEdit->setFocusPolicy(Qt::ClickFocus);
        sigEdit->setToolTip(QCoreApplication::translate("NewContactWidget", "Special Activity Group", nullptr));

        //sigInfoEdit->setMaximumSize(QSize(200, 16777215));
        //sigInfoEdit->setFocusPolicy(Qt::ClickFocus);
        sigInfoEdit->setToolTip(QCoreApplication::translate("NewContactWidget", "Special Activity Group Information", nullptr));

        //emailEdit->setMaximumSize(QSize(200, 16777215));
        //emailEdit->setFocusPolicy(Qt::ClickFocus);

        //urlEdit->setMaximumSize(QSize(200, 16777215));
        //urlEdit->setFocusPolicy(Qt::ClickFocus);

        //satNameEdit->setMaximumSize(QSize(200, 16777215));
        satNameEdit->setFocusPolicy(Qt::ClickFocus);
        satNameEdit->setEnabled(false);
        QSqlTableModel* satModel = new QSqlTableModel();
        satModel->setTable("sat_info");
        QCompleter *satCompleter = new QCompleter(satNameEdit);
        satCompleter->setModel(satModel);
        satCompleter->setCompletionColumn(satModel->fieldIndex("name"));
        satCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        satNameEdit->setCompleter(satCompleter);
        satModel->select();

        satModeEdit->setMinimumSize(QSize(300, 0));
        satModeEdit->setFocusPolicy(Qt::ClickFocus);
        satModeEdit->setEnabled(false);
        QStringList satModesList = Data::instance()->satModeList();
        satModesList.prepend("");
        QStringListModel* satModesModel = new QStringListModel(satModesList, satModeEdit);
        satModeEdit->setModel(satModesModel);
    }
}

QWidget *NewContactDynamicWidgets::getRowWidget(int index)
{
    FCT_IDENTIFICATION;

    if ( !widgetMapping.contains(index) )
        return nullptr;

    widgetMapping.value(index).rowWidget->setHidden(false);
    widgetMapping.value(index).label->setHidden(false);
    widgetMapping.value(index).label->setFocusPolicy(Qt::NoFocus);
    widgetMapping.value(index).editor->setHidden(false);
    widgetMapping.value(index).editor->setFocusPolicy(Qt::StrongFocus);
    // recreate layout because getLabel destroy parent
    widgetMapping.value(index).rowWidget->layout()->addWidget(widgetMapping.value(index).label);
    widgetMapping.value(index).rowWidget->layout()->addWidget(widgetMapping.value(index).editor);
    return widgetMapping.value(index).rowWidget;
}

QWidget *NewContactDynamicWidgets::getLabel(int index)
{
    FCT_IDENTIFICATION;

    if ( !widgetMapping.contains(index) )
        return nullptr;

    widgetMapping.value(index).label->setHidden(false);
    widgetMapping.value(index).label->setFocusPolicy(Qt::NoFocus);
    return widgetMapping.value(index).label;
}

QWidget *NewContactDynamicWidgets::getEditor(int index)
{
    FCT_IDENTIFICATION;

    widgetMapping.value(index).editor->setHidden(false);
    widgetMapping.value(index).editor->setFocusPolicy(Qt::ClickFocus);
    return widgetMapping.value(index).editor;
}

QStringList NewContactDynamicWidgets::getAllFieldLabelNames() const
{
    FCT_IDENTIFICATION;

    QStringList ret;
    const QList<DynamicWidget> &dynWidget = widgetMapping.values();

    for (const DynamicWidget &widget : qAsConst(dynWidget))
    {
        ret << widget.fieldLabelName;
    }
    return ret;
}

int NewContactDynamicWidgets::getIndex4FieldLabelName(const QString &value) const
{
    FCT_IDENTIFICATION;

    QHashIterator<int, DynamicWidget> i(widgetMapping);
    while ( i.hasNext() )
    {
        i.next();
        if ( i.value().fieldLabelName == value )
            return i.key();
    }
    return -1;
}

QString NewContactDynamicWidgets::getFieldLabelName4Index(int i) const
{
    FCT_IDENTIFICATION;

    return widgetMapping.value(i).fieldLabelName;
}

void NewContactDynamicWidgets::initializeWidgets(int DBIndexMapping,
                                                 const QString &objectName,
                                                 QLabel *&retLabel,
                                                 NewContactEditLine *&retWidget)
{
    DynamicWidget widget;

    widget.fieldLabelName = logbookmodel->headerData(DBIndexMapping, Qt::Horizontal).toString();
    widget.baseObjectName = objectName;
    widget.label = retLabel = nullptr;
    widget.editor = retLabel = nullptr;
    widget.rowWidget = retLabel = nullptr;

    if ( widgetsAllocated )
    {
        QWidget *rowWidget = new QWidget(parent);
        rowWidget->setObjectName(objectName + "Widget");

        QVBoxLayout *rowWidgetLayout = new QVBoxLayout(rowWidget);
        rowWidgetLayout->setSpacing(0);
        rowWidgetLayout->setObjectName(objectName + "Layout");
        rowWidgetLayout->setContentsMargins(0,0,0,0);

        widget.label = retLabel = new QLabel(widget.fieldLabelName, rowWidget);
        retLabel->setObjectName(objectName + "Label");

        widget.editor = retWidget = new NewContactEditLine(rowWidget);
        retWidget->setObjectName(objectName + "Edit");

        rowWidgetLayout->addWidget(retLabel);
        rowWidgetLayout->addWidget(retWidget);

        rowWidget->hide();
        widget.rowWidget = rowWidget;
    }

    widgetMapping[DBIndexMapping] = widget;
}

void NewContactDynamicWidgets::initializeWidgets(int DBIndexMapping,
                                                 const QString &objectName,
                                                 QLabel *&retLabel,
                                                 QComboBox *&retWidget)
{
    DynamicWidget widget;

    widget.fieldLabelName = logbookmodel->headerData(DBIndexMapping, Qt::Horizontal).toString();
    widget.baseObjectName = objectName;
    widget.label = retLabel = nullptr;
    widget.editor = retLabel = nullptr;
    widget.rowWidget = retLabel = nullptr;

    if ( widgetsAllocated )
    {
        QWidget *rowWidget = new QWidget(parent);
        rowWidget->setObjectName(objectName + "Widget");

        QVBoxLayout *rowWidgetLayout = new QVBoxLayout(rowWidget);
        rowWidgetLayout->setSpacing(0);
        rowWidgetLayout->setObjectName(objectName + "Layout");
        rowWidgetLayout->setContentsMargins(0,0,0,0);

        widget.label = retLabel = new QLabel(widget.fieldLabelName, rowWidget);
        retLabel->setObjectName(objectName + "Label");

        widget.editor = retWidget = new QComboBox(rowWidget);
        retWidget->setObjectName(objectName + "Edit");

        rowWidgetLayout->addWidget(retLabel);
        rowWidgetLayout->addWidget(retWidget);

        rowWidget->hide();
        widget.rowWidget = rowWidget;
    }

    widgetMapping[DBIndexMapping] = widget;
}
