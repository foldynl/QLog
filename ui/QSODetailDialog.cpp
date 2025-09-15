#include <QPushButton>
#include <QSqlRecord>
#include <QCompleter>
#include <QKeyEvent>
#include <QProgressDialog>
#include <QDesktopServices>
#include <QMessageBox>
#include <QMovie>

#include "QSODetailDialog.h"
#include "ui_QSODetailDialog.h"
#include "core/debug.h"
#include "data/Data.h"
#include "PaperQSLDialog.h"
#include "service/eqsl/Eqsl.h"
#include "models/SqlListModel.h"
#include "data/Gridsquare.h"
#include "data/Callsign.h"
#include "data/BandPlan.h"
#include "core/LogParam.h"
#include "component/SmartSearchBox.h"

MODULE_IDENTIFICATION("qlog.ui.qsodetaildialog");

#define CHANGECSS "color: orange;"
#define SAVE_BUTTON_TEXT 0
#define EDIT_BUTTON_TEXT 1

QSODetailDialog::QSODetailDialog(const QSqlRecord &qso,
                                 QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QSODetailDialog),
    mapper(new QDataWidgetMapper(this)),
    model(new LogbookModelPrivate(this)),
    editedRecord(new QSqlRecord(qso)),
    isMainPageLoaded(false),
    main_page(new WebEnginePage(this)),
    layerControlHandler("qsodetail", parent)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    /* model setting */
    model->setFilter(QString("id = '%1'").arg(qso.value("id").toString()));
    model->select();
    connect(model, &QSqlTableModel::beforeUpdate, this, &QSODetailDialog::handleBeforeUpdate);

    /* mapView setting */
    main_page->setWebChannel(&channel);
    ui->mapView->setPage(main_page);
    main_page->load(QUrl(QStringLiteral("qrc:/res/map/onlinemap.html")));
    ui->mapView->setFocusPolicy(Qt::ClickFocus);
    connect(ui->mapView, &QWebEngineView::loadFinished, this, &QSODetailDialog::mapLoaded);
    channel.registerObject("layerControlHandler", &layerControlHandler);

    /* Edit Button */
    editButton = new QPushButton(getButtonText(EDIT_BUTTON_TEXT));
    ui->buttonBox->addButton(editButton, QDialogButtonBox::ActionRole);
    connect(editButton, &QPushButton::clicked, this, &QSODetailDialog::editButtonPressed);

    /* Reset Button */
    resetButton = new QPushButton(tr("&Reset"));
    ui->buttonBox->addButton(resetButton, QDialogButtonBox::ActionRole);
    connect(resetButton, &QPushButton::clicked, this, &QSODetailDialog::resetButtonPressed);

    /* Lookup Button */
    lookupButton = new QPushButton(tr("&Lookup"));
    ui->buttonBox->addButton(lookupButton, QDialogButtonBox::ActionRole);
    connect(lookupButton, &QPushButton::clicked, this, &QSODetailDialog::lookupButtonPressed);
    lookupButtonMovie = new QMovie(this);
    lookupButtonMovie->setFileName(":/icons/loading.gif");
    connect(lookupButtonMovie, &QMovie::frameChanged, this, [this]
    {
          this->lookupButton->setIcon(this->lookupButtonMovie->currentPixmap());
    });    lookupButtonWaitingStyle(false);

    /* timeformat for DateTime */
    ui->dateTimeOnEdit->setDisplayFormat(locale.formatDateShortWithYYYY() + " " + locale.formatTimeLongWithoutTZ());
    ui->dateTimeOffEdit->setDisplayFormat(locale.formatDateShortWithYYYY() + " " + locale.formatTimeLongWithoutTZ());
    ui->qslPaperReceiveDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->qslPaperSentDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->qslEqslSentDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->qslLotwSentDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());

    /* Mapper setting */
    mapper->setModel(model);
    QSOEditMapperDelegate *QSOitemDelegate = new QSOEditMapperDelegate(mapper);
    mapper->setItemDelegate(QSOitemDelegate);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    connect(QSOitemDelegate, &QSOEditMapperDelegate::keyEscapePressed, this, &QSODetailDialog::resetKeyPressed);

    /* Callbook Signals registration */
    connect(&callbookManager, &CallbookManager::callsignResult,
            this, &QSODetailDialog::callsignFound);

    connect(&callbookManager, &CallbookManager::callsignNotFound,
            this, &QSODetailDialog::callsignNotFound);

    connect(&callbookManager, &CallbookManager::loginFailed,
            this, &QSODetailDialog::callbookLoginFailed);

    connect(&callbookManager, &CallbookManager::lookupError,
            this, &QSODetailDialog::callbookError);

    connect(MembershipQE::instance(), &MembershipQE::clubStatusResult,
            this, &QSODetailDialog::clubQueryResult);

    /*******************/
    /* Main Screen GUI */
    /*******************/

    /* ITU Zones Validators */
    ui->ituEdit->setValidator(new QIntValidator(Data::getITUZMin(), Data::getITUZMax(), this));
    ui->myITUEdit->setValidator(new QIntValidator(Data::getITUZMin(), Data::getITUZMax(), this));

    /* CQ Zones Validators */
    ui->cqEdit->setValidator(new QIntValidator(Data::getCQZMin(), Data::getCQZMax(), this));
    ui->myCQEdit->setValidator(new QIntValidator(Data::getCQZMin(), Data::getCQZMax(), this));

    /* Submode mapping */
    QStringListModel* submodeModel = new QStringListModel(this);
    ui->submodeEdit->setModel(submodeModel);

    /* Mode mapping */
    QSqlTableModel* modeModel = new QSqlTableModel(this);
    modeModel->setTable("modes");
    modeModel->setSort(modeModel->fieldIndex("name"), Qt::AscendingOrder);
    ui->modeEdit->setModel(modeModel);
    ui->modeEdit->setModelColumn(modeModel->fieldIndex("name"));
    modeModel->select();

    /* IOTA Completer */
    iotaCompleter.reset(new QCompleter(Data::instance()->iotaIDList(), this));
    iotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    iotaCompleter->setFilterMode(Qt::MatchContains);
    iotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->iotaEdit->setCompleter(iotaCompleter.data());

    /* SOTA Completer */
    sotaCompleter.reset(new QCompleter(Data::instance()->sotaIDList(), this));
    sotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    sotaCompleter->setFilterMode(Qt::MatchStartsWith);
    sotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->sotaEdit->setCompleter(nullptr);

    /* POTA Completer */
    potaCompleter.reset(new MultiselectCompleter(Data::instance()->potaIDList(), this));
    potaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    potaCompleter->setFilterMode(Qt::MatchStartsWith);
    potaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->potaEdit->setCompleter(nullptr);

    /* WWFF Completer */
    wwffCompleter.reset(new QCompleter(Data::instance()->wwffIDList(), this));
    wwffCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    wwffCompleter->setFilterMode(Qt::MatchStartsWith);
    wwffCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->wwffEdit->setCompleter(nullptr);

    /* MyIOTA Completer */
    myIotaCompleter.reset(new QCompleter(Data::instance()->iotaIDList(), this));
    myIotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    myIotaCompleter->setFilterMode(Qt::MatchContains);
    myIotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->myIOTAEdit->setCompleter(myIotaCompleter.data());

    /* MySOTA Completer */
    mySotaCompleter.reset(new QCompleter(Data::instance()->sotaIDList(), this));
    mySotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    mySotaCompleter->setFilterMode(Qt::MatchStartsWith);
    mySotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->mySOTAEdit->setCompleter(nullptr);

    /* MyPOTA Completer */
    myPotaCompleter.reset(new QCompleter(Data::instance()->potaIDList(), this));
    myPotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    myPotaCompleter->setFilterMode(Qt::MatchStartsWith);
    myPotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->myPOTAEdit->setCompleter(nullptr);

    /* MyWWFF Completer */
    myWWFFCompleter.reset(new QCompleter(Data::instance()->wwffIDList(), this));
    myWWFFCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    myWWFFCompleter->setFilterMode(Qt::MatchStartsWith);
    myWWFFCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->myWWFFEdit->setCompleter(nullptr);

    /* SIF Completer */
    sigCompleter.reset(new QCompleter(Data::instance()->sigIDList(), this));
    sigCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    sigCompleter->setFilterMode(Qt::MatchStartsWith);
    sigCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->sigEdit->setCompleter(sigCompleter.data());

    /* Combo Mapping */
    /* do no use Data::qslPaperSentStatusBox for it because
     * Data::qslPaperSentStatusBox has a different ordering.
     * Ordering below is optimized for a new Contact Widget only
     */
    ui->qslPaperSentStatusBox->addItem(tr("No"), QVariant("N"));
    ui->qslPaperSentStatusBox->addItem(tr("Yes"), QVariant("Y"));
    ui->qslPaperSentStatusBox->addItem(tr("Requested"), QVariant("R"));
    ui->qslPaperSentStatusBox->addItem(tr("Queued"), QVariant("Q"));
    ui->qslPaperSentStatusBox->addItem(tr("Ignored"), QVariant("I"));

    /* Combo Mapping */
    /* do no use Data::qslLotwSentStatusBox for it because
     * Data::qslLotwSentStatusBox has a different ordering.
     * Ordering below is optimized for a new Contact Widget only
     */
    ui->qslLotwSentStatusBox->addItem(tr("No"), QVariant("N"));
    ui->qslLotwSentStatusBox->addItem(tr("Yes"), QVariant("Y"));
    ui->qslLotwSentStatusBox->addItem(tr("Requested"), QVariant("R"));
    ui->qslLotwSentStatusBox->addItem(tr("Queued"), QVariant("Q"));
    ui->qslLotwSentStatusBox->addItem(tr("Ignored"), QVariant("I"));

    /* Combo Mapping */
    /* do no use Data::qslEqslSentStatusBox for it because
     * Data::qslEqslSentStatusBox has a different ordering.
     * Ordering below is optimized for a new Contact Widget only
     */
    ui->qslEqslSentStatusBox->addItem(tr("No"), QVariant("N"));
    ui->qslEqslSentStatusBox->addItem(tr("Yes"), QVariant("Y"));
    ui->qslEqslSentStatusBox->addItem(tr("Requested"), QVariant("R"));
    ui->qslEqslSentStatusBox->addItem(tr("Queued"), QVariant("Q"));
    ui->qslEqslSentStatusBox->addItem(tr("Ignored"), QVariant("I"));

    QMapIterator<QString, QString> iter(Data::instance()->qslRcvdEnum);

    while( iter.hasNext() )
    {
        iter.next();
        ui->qslPaperReceiveStatusBox->addItem(iter.value(), iter.key());
    }

    /* do no use Data::qslSentViaBox for it because
     * Data::qslSentViaBox has a different ordering.
     * Ordering below is optimized for a new Contact Widget only
     */
    ui->qslSentViaBox->addItem("", QVariant(""));
    ui->qslSentViaBox->addItem(tr("Bureau"), QVariant("B"));
    ui->qslSentViaBox->addItem(tr("Direct"), QVariant("D"));
    ui->qslSentViaBox->addItem(tr("Electronic"), QVariant("E"));

    /* Propagation */
    QStringListModel* propagationModeModel = new QStringListModel(Data::instance()->propagationModesList(), this);
    ui->propagationModeEdit->setModel(propagationModeModel);

    /* Sat Modes & sat names */
    QSqlTableModel* satModel = new QSqlTableModel(ui->satNameEdit);
    satModel->setTable("sat_info");
    QCompleter *satCompleter = new QCompleter(ui->satNameEdit);
    satCompleter->setModel(satModel);
    satCompleter->setCompletionColumn(satModel->fieldIndex("name"));
    satCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    ui->satNameEdit->setCompleter(satCompleter);
    satModel->select();

    QStringList satModesList = Data::instance()->satModeList();
    satModesList.prepend("");
    QStringListModel* satModesModel = new QStringListModel(satModesList, this);
    ui->satModeEdit->setModel(satModesModel);

    /* Country */
    ui->countryBox->setModel(new SqlListModel("SELECT id, translate_to_locale(name), name  "
                                              "FROM dxcc_entities "
                                              "ORDER BY 2 COLLATE LOCALEAWARE ASC;", " ", ui->countryBox));
    ui->countryBox->setModelColumn(1);
    ui->countryBox->adjustMaxSize();

    /* My Country Combo */
    ui->myCountryBox->setModel(new SqlListModel("SELECT id, translate_to_locale(name), name  "
                                                "FROM dxcc_entities "
                                                "ORDER BY 2 COLLATE LOCALEAWARE ASC;", " ", ui->myCountryBox));
    ui->myCountryBox->setModelColumn(1);
    ui->myCountryBox->adjustMaxSize();

    /* Band Combos */
    SqlListModel* bandModel = new SqlListModel("SELECT name FROM bands ORDER BY start_freq;", tr("Blank"), this);
    while ( bandModel->canFetchMore() )
    {
        bandModel->fetchMore();
    }
    ui->bandTXCombo->setModel(bandModel);
    ui->bandRXCombo->setModel(bandModel);

    /* Assign Validators */
    ui->callsignEdit->setValidator(new QRegularExpressionValidator(Callsign::callsignRegEx(), this));
    ui->myCallsignEdit->setValidator(new QRegularExpressionValidator(Callsign::callsignRegEx(), this));
    ui->gridEdit->setValidator(new QRegularExpressionValidator(Gridsquare::gridRegEx(), this));
    ui->myGridEdit->setValidator(new QRegularExpressionValidator(Gridsquare::gridRegEx(), this));
    ui->vuccEdit->setValidator(new QRegularExpressionValidator(Gridsquare::gridVUCCRegEx(), this));
    ui->myVUCCEdit->setValidator(new QRegularExpressionValidator(Gridsquare::gridVUCCRegEx(), this));
    ui->fistsEdit->setValidator(new QIntValidator(0, INT_MAX, ui->fistsEdit));
    ui->fistsCCEdit->setValidator(new QIntValidator(0, INT_MAX, ui->fistsCCEdit));
    ui->tentenEdit->setValidator(new QIntValidator(0, INT_MAX, ui->tentenEdit));
    ui->uksmgEdit->setValidator(new QIntValidator(0, INT_MAX, ui->uksmgEdit));

    /***********/
    /* Mapping */
    /***********/

    /* Detail */
    mapper->addMapping(ui->dateTimeOnEdit, LogbookModel::COLUMN_TIME_ON);
    mapper->addMapping(ui->dateTimeOffEdit, LogbookModel::COLUMN_TIME_OFF);
    mapper->addMapping(ui->callsignEdit, LogbookModel::COLUMN_CALL);
    mapper->addMapping(ui->rstSentEdit, LogbookModel::COLUMN_RST_SENT);
    mapper->addMapping(ui->rstRcvdEdit, LogbookModel::COLUMN_RST_RCVD);
    mapper->addMapping(ui->modeEdit, LogbookModel::COLUMN_MODE, "currentText");
    mapper->addMapping(ui->submodeEdit, LogbookModel::COLUMN_SUBMODE, "currentText");
    mapper->addMapping(ui->freqRXEdit, LogbookModel::COLUMN_FREQ_RX);
    mapper->addMapping(ui->freqTXEdit, LogbookModel::COLUMN_FREQUENCY);
    mapper->addMapping(ui->bandRXCombo, LogbookModel::COLUMN_BAND_RX);
    mapper->addMapping(ui->bandTXCombo, LogbookModel::COLUMN_BAND);
    mapper->addMapping(ui->nameEdit, LogbookModel::COLUMN_NAME_INTL);
    mapper->addMapping(ui->qthEdit, LogbookModel::COLUMN_QTH_INTL);
    mapper->addMapping(ui->gridEdit, LogbookModel::COLUMN_GRID);
    mapper->addMapping(ui->commentEdit, LogbookModel::COLUMN_COMMENT_INTL);
    mapper->addMapping(ui->contEdit, LogbookModel::COLUMN_CONTINENT, "currentText");
    mapper->addMapping(ui->ituEdit, LogbookModel::COLUMN_ITUZ);
    mapper->addMapping(ui->cqEdit, LogbookModel::COLUMN_CQZ);
    mapper->addMapping(ui->stateEdit, LogbookModel::COLUMN_STATE);
    mapper->addMapping(ui->countyEdit, LogbookModel::COLUMN_COUNTY);
    mapper->addMapping(ui->ageEdit, LogbookModel::COLUMN_AGE);
    mapper->addMapping(ui->iotaEdit, LogbookModel::COLUMN_IOTA);
    mapper->addMapping(ui->sotaEdit, LogbookModel::COLUMN_SOTA_REF);
    mapper->addMapping(ui->potaEdit, LogbookModel::COLUMN_POTA_REF);
    mapper->addMapping(ui->sigEdit, LogbookModel::COLUMN_SIG_INTL);
    mapper->addMapping(ui->sigInfoEdit, LogbookModel::COLUMN_SIG_INFO_INTL);
    mapper->addMapping(ui->dokEdit, LogbookModel::COLUMN_DARC_DOK);
    mapper->addMapping(ui->vuccEdit, LogbookModel::COLUMN_VUCC_GRIDS);
    mapper->addMapping(ui->wwffEdit, LogbookModel::COLUMN_WWFF_REF);
    mapper->addMapping(ui->countryBox, LogbookModel::COLUMN_DXCC);
    mapper->addMapping(ui->emailEdit, LogbookModel::COLUMN_EMAIL);
    mapper->addMapping(ui->urlEdit, LogbookModel::COLUMN_WEB);
    mapper->addMapping(ui->propagationModeEdit, LogbookModel::COLUMN_PROP_MODE);
    mapper->addMapping(ui->satNameEdit, LogbookModel::COLUMN_SAT_NAME);
    mapper->addMapping(ui->satModeEdit,LogbookModel::COLUMN_SAT_MODE);
    mapper->addMapping(ui->fistsEdit,LogbookModel::COLUMN_FISTS);
    mapper->addMapping(ui->fistsCCEdit,LogbookModel::COLUMN_FISTS_CC);
    mapper->addMapping(ui->skccEdit,LogbookModel::COLUMN_SKCC);
    mapper->addMapping(ui->tentenEdit,LogbookModel::COLUMN_TEN_TEN);
    mapper->addMapping(ui->uksmgEdit,LogbookModel::COLUMN_UKSMG);

    /* My Station */
    mapper->addMapping(ui->myCallsignEdit, LogbookModel::COLUMN_STATION_CALLSIGN);
    mapper->addMapping(ui->myOperatorNameEdit, LogbookModel::COLUMN_MY_NAME_INTL);
    mapper->addMapping(ui->myCountryBox, LogbookModel::COLUMN_MY_DXCC);
    mapper->addMapping(ui->myITUEdit, LogbookModel::COLUMN_MY_ITU_ZONE);
    mapper->addMapping(ui->myCQEdit, LogbookModel::COLUMN_MY_CQ_ZONE);
    mapper->addMapping(ui->myQTHEdit, LogbookModel::COLUMN_MY_CITY_INTL);
    mapper->addMapping(ui->myGridEdit, LogbookModel::COLUMN_MY_GRIDSQUARE);
    mapper->addMapping(ui->mySOTAEdit, LogbookModel::COLUMN_MY_SOTA_REF);
    mapper->addMapping(ui->myPOTAEdit, LogbookModel::COLUMN_MY_POTA_REF);
    mapper->addMapping(ui->myIOTAEdit, LogbookModel::COLUMN_MY_IOTA);
    mapper->addMapping(ui->mySIGEdit, LogbookModel::COLUMN_MY_SIG);
    mapper->addMapping(ui->mySIGInfoEdit, LogbookModel::COLUMN_MY_SIG_INFO_INTL);
    mapper->addMapping(ui->myRigEdit, LogbookModel::COLUMN_MY_RIG_INTL);
    mapper->addMapping(ui->myAntEdit, LogbookModel::COLUMN_MY_ANTENNA_INTL);
    mapper->addMapping(ui->myVUCCEdit, LogbookModel::COLUMN_MY_VUCC_GRIDS);
    mapper->addMapping(ui->myWWFFEdit, LogbookModel::COLUMN_MY_WWFF_REF);
    mapper->addMapping(ui->powerEdit, LogbookModel::COLUMN_TX_POWER);
    mapper->addMapping(ui->myCountyEdit, LogbookModel::COLUMN_MY_CNTY);
    mapper->addMapping(ui->myOperatorCallsignEdit, LogbookModel::COLUMN_OPERATOR);
    mapper->addMapping(ui->myDOKEdit, LogbookModel::COLUMN_MY_DARC_DOK);

    /* Notes */
    mapper->addMapping(ui->noteEdit, LogbookModel::COLUMN_NOTES_INTL);

    /* QSL */
    mapper->addMapping(ui->qslPaperSentStatusBox, LogbookModel::COLUMN_QSL_SENT);
    mapper->addMapping(ui->qslPaperReceiveStatusBox, LogbookModel::COLUMN_QSL_RCVD);
    mapper->addMapping(ui->qslEqslReceiveDateLabel, LogbookModel::COLUMN_EQSL_QSLRDATE);
    mapper->addMapping(ui->qslEqslSentDateEdit, LogbookModel::COLUMN_EQSL_QSLSDATE);
    mapper->addMapping(ui->qslLotwReceiveDateLabel, LogbookModel::COLUMN_LOTW_RCVD_DATE);
    mapper->addMapping(ui->qslLotwSentDateEdit, LogbookModel::COLUMN_LOTW_SENT_DATE);
    mapper->addMapping(ui->qslEqslReceiveStatusLabel, LogbookModel::COLUMN_EQSL_QSL_RCVD);
    mapper->addMapping(ui->qslEqslSentStatusBox, LogbookModel::COLUMN_EQSL_QSL_SENT);
    mapper->addMapping(ui->qslLotwReceiveStatusLabel, LogbookModel::COLUMN_LOTW_RCVD);
    mapper->addMapping(ui->qslLotwSentStatusBox, LogbookModel::COLUMN_LOTW_SENT);
    mapper->addMapping(ui->qslReceivedMsgEdit, LogbookModel::COLUMN_QSLMSG_RCVD, "text");
    mapper->addMapping(ui->qslSentMsgEdit, LogbookModel::COLUMN_QSLMSG_INTL, "text");
    mapper->addMapping(ui->qslSentViaBox, LogbookModel::COLUMN_QSL_SENT_VIA);
    mapper->addMapping(ui->qslViaEdit, LogbookModel::COLUMN_QSL_VIA);
    mapper->addMapping(ui->qslPaperReceiveDateEdit, LogbookModel::COLUMN_QSL_RCVD_DATE);
    mapper->addMapping(ui->qslPaperSentDateEdit, LogbookModel::COLUMN_QSL_SENT_DATE);

    /* Contest */
    mapper->addMapping(ui->contestIDEdit, LogbookModel::COLUMN_CONTEST_ID);
    mapper->addMapping(ui->srxEdit, LogbookModel::COLUMN_SRX);
    mapper->addMapping(ui->srxStringEdit, LogbookModel::COLUMN_SRX_STRING);
    mapper->addMapping(ui->stxEdit, LogbookModel::COLUMN_STX);
    mapper->addMapping(ui->stxStringEdit, LogbookModel::COLUMN_STX_STRING);

    /**************/
    /* Get Record */
    /**************/
    //only one record is selected therefore calling toFirst is OK

    blockMappedWidgetSignals(true);
    mapper->toFirst();
    blockMappedWidgetSignals(false);

    setReadOnlyMode(true);

    drawDXOnMap(ui->callsignEdit->text(), Gridsquare(ui->gridEdit->text()));
    drawMyQTHOnMap(ui->myCallsignEdit->text(), Gridsquare(ui->myGridEdit->text()));
    setStaticMapTime(ui->dateTimeOnEdit->dateTime());
    refreshDXStatTabs();

    queryMemberList();

    enableWidgetChangeHandlers();
}

void QSODetailDialog::accept()
{
    FCT_IDENTIFICATION;

    if (editButton->text() == getButtonText(SAVE_BUTTON_TEXT) )
    {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Submit changes"), tr("Really submit all changes?"),
                                      QMessageBox::Yes|QMessageBox::No);

        if (reply == QMessageBox::Yes)
        {
            QSODetailDialog::SubmitError error = submitAllChanges();
            if ( error == QSODetailDialog::SubmitMapperError
                 || error == QSODetailDialog::SubmitModelError )
            {
                QMessageBox::critical(this, tr("QLog Error"), tr("Cannot save all changes - internal error"));
            }
        }
    }

    callbookManager.abortQuery();

    done(QDialog::Accepted);
}

void QSODetailDialog::keyPressEvent(QKeyEvent *evt)
{
    FCT_IDENTIFICATION;

    /* suppress Enter press because it automatically save all changes */
    if( evt->key() == Qt::Key_Enter
        || evt->key() == Qt::Key_Return )
    {
        return;
    }
    QDialog::keyPressEvent(evt);
}

QSODetailDialog::~QSODetailDialog()
{
    FCT_IDENTIFICATION;

    delete editedRecord;
    delete ui;
}

void QSODetailDialog::editButtonPressed()
{
    FCT_IDENTIFICATION;

    if ( editButton->text() == getButtonText(SAVE_BUTTON_TEXT) )
    {
        QSODetailDialog::SubmitError error = submitAllChanges();
        if ( error == QSODetailDialog::SubmitCancelledByUser )
        {
            /* an user wants to fix invalid fields, edit mode remains active */
            return;
        }
        else if ( error == QSODetailDialog::SubmitMapperError
                  || error == QSODetailDialog::SubmitModelError )
        {
            QMessageBox::critical(this, tr("QLog Error"), tr("Cannot save all changes - try to reset all changes"));
            /* edit mode remains active to fix a possible problem */
            return;
        }

        setReadOnlyMode(true);
    }
    else
    {
        setReadOnlyMode(false);
        timeLockDiff = ui->dateTimeOnEdit->dateTime().msecsTo(ui->dateTimeOffEdit->dateTime());
        freqLockDiff = ui->freqTXEdit->value() -  ui->freqRXEdit->value();
    }
}

void QSODetailDialog::resetButtonPressed()
{
    FCT_IDENTIFICATION;

    blockMappedWidgetSignals(true);
    mapper->revert();
    blockMappedWidgetSignals(false);

    queryMemberList();

    setReadOnlyMode(true);
    doValidation();
}

void QSODetailDialog::lookupButtonPressed()
{
    FCT_IDENTIFICATION;

    callbookLookupStart();
    callbookManager.queryCallsign(ui->callsignEdit->text());
}

/* called when qdatawidgetmapper reverts a widget value by pressing ESC */
void QSODetailDialog::resetKeyPressed(QObject *inObject)
{
    FCT_IDENTIFICATION;

    QWidget *widget = qobject_cast<QWidget*>(inObject);
    if ( widget )
    {
        // datawidgetmapper resets a value when ESC is pressed
        // therefore it is needed to reset stylesheet when resetKey (ESC) is pressed
        widget->setStyleSheet("");
    }
}

void QSODetailDialog::setReadOnlyMode(bool inReadOnly)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << inReadOnly;

    for ( int i = 0; i < LogbookModel::COLUMN_LAST_ELEMENT; i++)
    {
        QWidget *widget = mapper->mappedWidgetAt(i);

        if ( !widget ) continue;

        if ( QLineEdit *line = qobject_cast<QLineEdit*>(widget) )
        {
            line->setReadOnly(inReadOnly);
            if ( inReadOnly )
            {
                line->setStyleSheet("");
            }
        }
        else if ( QTextEdit *edit = qobject_cast<QTextEdit*>(widget) )
        {
            edit->setReadOnly(inReadOnly);
            if ( inReadOnly )
            {
                edit->setStyleSheet("");
            }
        }
        else if ( QDoubleSpinBox *spin = qobject_cast<QDoubleSpinBox*>(widget) )
        {
            spin->setReadOnly(inReadOnly);
            if ( inReadOnly )
            {
                spin->setStyleSheet("");
            }
        }
        else if ( QDateTimeEdit *datetime = qobject_cast<QDateTimeEdit*>(widget) )
        {
            datetime->setReadOnly(inReadOnly);
            if ( inReadOnly )
            {
                datetime->setStyleSheet("");
            }
        }
        else if ( qobject_cast<QLabel*>(widget) )
        {
            //do nothing - naturally readonly
        }
        else if ( QComboBox *combo = qobject_cast<QComboBox*>(widget) )
        {
            combo->setEnabled(!inReadOnly);
            if ( inReadOnly )
            {
                combo->setStyleSheet("");
            }
        }
        else if ( widget )
        {
            widget->setEnabled(!inReadOnly);
            if ( inReadOnly )
            {
                widget->setStyleSheet("");
            }
        }
    }

    if ( ui->propagationModeEdit->currentText() != Data::instance()->propagationModeIDToText("SAT") )
    {
        /* Do not enable sat fields when SAT prop is not selected */
        ui->satModeEdit->setCurrentIndex(-1);
        ui->satNameEdit->clear();
        ui->satModeEdit->setEnabled(false);
        ui->satNameEdit->setEnabled(false);
    }
    else
    {
        ui->satNameEdit->setEnabled(true);
    }

    editButton->setText((( inReadOnly) ? getButtonText(EDIT_BUTTON_TEXT)
                                       : getButtonText(SAVE_BUTTON_TEXT) ));

    ui->timeLockButton->setEnabled(!inReadOnly);
    ui->freqLockButton->setEnabled(!inReadOnly);
    resetButton->setEnabled(!inReadOnly);

    lookupButton->setEnabled(!inReadOnly && callbookManager.isActive());

    if ( ui->qslEqslReceiveStatusLabel->property("originvalue").toString() != "Y" )
    {
        ui->qslEqslPicButton->setEnabled(false);
    }

    setWindowTitle( (inReadOnly)? tr("QSO Detail") : tr("Edit QSO"));
}

void QSODetailDialog::modeChanged(QString)
{
    FCT_IDENTIFICATION;

    QSqlTableModel* modeModel = dynamic_cast<QSqlTableModel*>(ui->modeEdit->model());
    QSqlRecord record = modeModel->record(ui->modeEdit->currentIndex());
    QString submodes = record.value("submodes").toString();

    QStringList submodeList = QJsonDocument::fromJson(submodes.toUtf8()).toVariant().toStringList();
    QStringListModel* model = dynamic_cast<QStringListModel*>(ui->submodeEdit->model());
    model->setStringList(submodeList);

    if (!submodeList.isEmpty())
    {
        submodeList.prepend("");
        model->setStringList(submodeList);
        ui->submodeEdit->setEnabled(true);
    }
    else
    {
        QStringList list;
        model->setStringList(list);
        ui->submodeEdit->setEnabled(false);
    }

    queryMemberList();
}

void QSODetailDialog::showPaperButton()
{
    FCT_IDENTIFICATION;
    PaperQSLDialog dialog(*editedRecord);
    dialog.exec();
}

void QSODetailDialog::showEQSLButton()
{
    FCT_IDENTIFICATION;

    QProgressDialog* dialog = new QProgressDialog(tr("Downloading eQSL Image"), tr("Cancel"), 0, 0, this);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setRange(0, 0);
    dialog->setAutoClose(true);
    dialog->show();

    EQSLQSLDownloader *eQSL = new EQSLQSLDownloader(dialog);

    connect(eQSL, &EQSLQSLDownloader::QSLImageFound, this, [dialog, eQSL](QString imgFile)
    {
        dialog->done(0);
        QDesktopServices::openUrl(QUrl::fromLocalFile(imgFile));
        eQSL->deleteLater();
    });

    connect(eQSL, &EQSLQSLDownloader::QSLImageError, this, [this, dialog, eQSL](const QString &error)
    {
        dialog->done(1);
        QMessageBox::critical(this, tr("QLog Error"), tr("eQSL Download Image failed: ") + error);
        eQSL->deleteLater();
    });

    connect(dialog, &QProgressDialog::canceled, this, [eQSL]()
    {
        qCDebug(runtime)<< "Operation canceled";
        eQSL->abortDownload();
        eQSL->deleteLater();
    });

    eQSL->getQSLImage(*editedRecord);
}

void QSODetailDialog::dateTimeOnChanged(const QDateTime &timeOn)
{
    FCT_IDENTIFICATION;

    ui->dateTimeOffEdit->blockSignals(true);

    if ( ui->timeLockButton->isChecked() )
    {
        ui->dateTimeOffEdit->setDateTime(timeOn.addMSecs(timeLockDiff));
        ui->dateTimeOffEdit->setStyleSheet(CHANGECSS); //change handles are off, mark field as "changed" manually
    }
    else if ( ui->dateTimeOffEdit->dateTime() < timeOn )
    {
        ui->dateTimeOffEdit->setDateTime(timeOn);
        ui->dateTimeOffEdit->setStyleSheet(CHANGECSS); //change handles are off, mark field as "changed" manually
    }

    ui->dateTimeOffEdit->blockSignals(false);
}

void QSODetailDialog::dateTimeOffChanged(const QDateTime &timeOff)
{
    FCT_IDENTIFICATION;

    ui->dateTimeOnEdit->blockSignals(true);

    if ( ui->timeLockButton->isChecked() )
    {
        ui->dateTimeOnEdit->setDateTime(timeOff.addMSecs(-timeLockDiff));
        ui->dateTimeOnEdit->setStyleSheet(CHANGECSS); //change handles are off, mark field as "changed" manually
    }
    else if ( ui->dateTimeOnEdit->dateTime() > timeOff )
    {
        ui->dateTimeOnEdit->setDateTime(timeOff);
        ui->dateTimeOnEdit->setStyleSheet(CHANGECSS); //change handles are off, mark field as "changed" manually
    }

    ui->dateTimeOnEdit->blockSignals(false);
}

void QSODetailDialog::freqTXChanged(double)
{
    FCT_IDENTIFICATION;

    ui->freqRXEdit->blockSignals(true);

    if ( ui->freqLockButton->isChecked()
         && ui->freqRXEdit->value() != 0.0 )
    {
        double shiftedRX = ui->freqTXEdit->value() - freqLockDiff;
        ui->freqRXEdit->setValue((shiftedRX < 0.0) ? 0.0 : shiftedRX);
        ui->freqRXEdit->setStyleSheet(CHANGECSS); //change handles are off, mark field as "changed" manually
    }

    ui->freqRXEdit->blockSignals(false);

    // TODO: qlog should call queryMemberList but for saving time we will omit it.
    // queryMemberList();
}

void QSODetailDialog::freqRXChanged(double)
{
    FCT_IDENTIFICATION;

    ui->freqTXEdit->blockSignals(true);

    if ( ui->freqLockButton->isChecked() )
    {
        double shiftedTX = ui->freqRXEdit->value() + freqLockDiff;
        ui->freqTXEdit->setValue((shiftedTX < 0.0) ? 0.0 : shiftedTX);
        ui->freqTXEdit->setStyleSheet(CHANGECSS); //change handles are off, mark field as "changed" manually
    }

    ui->freqTXEdit->blockSignals(false);
}

void QSODetailDialog::timeLockToggled(bool toggled)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << toggled;

    if ( toggled )
    {
        if ( ui->dateTimeOffEdit->dateTime() < ui->dateTimeOnEdit->dateTime() )
        {
            ui->dateTimeOffEdit->blockSignals(true);
            ui->dateTimeOffEdit->setDateTime(ui->dateTimeOnEdit->dateTime());
            ui->dateTimeOffEdit->blockSignals(false);
            timeLockDiff =  0;
        }
        else
        {
            timeLockDiff = ui->dateTimeOnEdit->dateTime().msecsTo(ui->dateTimeOffEdit->dateTime());
        }
    }
}

void QSODetailDialog::freqLockToggled(bool toggled)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << toggled;

    if ( toggled )
    {
        freqLockDiff = ui->freqTXEdit->value() - ui->freqRXEdit->value();
    }
}

void QSODetailDialog::callsignChanged(const QString &)
{
    FCT_IDENTIFICATION;

    refreshDXStatTabs();
}

void QSODetailDialog::callsignEditFinished()
{
    FCT_IDENTIFICATION;

    queryMemberList();
}

void QSODetailDialog::queryMemberList()
{
    FCT_IDENTIFICATION;

    if ( ui->callsignEdit->text().size() >= 3 )
    {
        MembershipQE::instance()->asyncQueryDetails(ui->callsignEdit->text(),
                                                    BandPlan::freq2Band(ui->freqTXEdit->value()).name,
                                                    ui->modeEdit->currentText());
    }
}

void QSODetailDialog::propagationModeChanged(const QString &propModeText)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << propModeText;

    if ( propModeText == Data::instance()->propagationModeIDToText("SAT") )
    {
        ui->satModeEdit->setEnabled(true);
        ui->satNameEdit->setEnabled(true);
    }
    else
    {
        ui->satModeEdit->setCurrentIndex(-1);
        ui->satNameEdit->clear();
        ui->satModeEdit->setEnabled(false);
        ui->satNameEdit->setEnabled(false);
    }
}

bool QSODetailDialog::doValidation()
{
    FCT_IDENTIFICATION;

    bool allValid = true;

    const QList<QLabel *> &list = findChildren<QLabel *>();

    for ( QLabel *label : list )
    {
        if ( label ) label->setToolTip(QString());
    }

    allValid &= highlightInvalid(ui->callsignLabel,
                                 ui->callsignEdit->text().isEmpty(),
                                 tr("DX Callsign must not be empty"));

    allValid &= highlightInvalid(ui->callsignLabel,
                                 !ui->callsignEdit->text().isEmpty() && !ui->callsignEdit->hasAcceptableInput(),
                                 tr("DX callsign has an incorrect format"));

    allValid &= highlightInvalid(ui->freqTXLabel,
                                 ui->freqTXEdit->value() == 0.0 && ui->bandTXCombo->currentIndex() == 0,
                                 tr("TX Frequency or Band must be filled"));

    const Band &bandTX = BandPlan::freq2Band(ui->freqTXEdit->value());
    const QString &bandTXString = bandTX.name;

    allValid &= highlightInvalid(ui->bandLabel,
                                 ui->freqTXEdit->value() != 0.0 && ui->bandTXCombo->currentText() != bandTXString,
                                 tr("TX Band should be ") + "<b>" + (bandTXString.isEmpty() ? "OOB" : bandTXString) + "</b>");

    allValid &= highlightInvalid(ui->bandLabel,
                                 ui->freqTXEdit->value() == 0.0 && ui->bandTXCombo->currentIndex() == 0,
                                 tr("TX Frequency or Band must be filled"));

    const Band &bandRX = BandPlan::freq2Band(ui->freqRXEdit->value());
    const QString &bandRXString = bandRX.name;

    allValid &= highlightInvalid(ui->bandLabel,
                                 ui->freqRXEdit->value() != 0.0 && ui->bandRXCombo->currentText() != bandRXString,
                                 tr("RX Band should be ") + "<b>" + (bandRXString.isEmpty() ? "OOB" : bandRXString) + "</b>");

    allValid &= highlightInvalid(ui->gridLabel,
                                 !ui->gridEdit->text().isEmpty() && !ui->gridEdit->hasAcceptableInput(),
                                 tr("DX Grid has an incorrect format"));

    const DxccEntity &dxccEntity = Data::instance()->lookupDxcc(ui->callsignEdit->text());

    allValid &= highlightInvalid(ui->countryLabel,
                                 dxccEntity.dxcc && ui->countryBox->currentText() != QCoreApplication::translate("DBStrings", dxccEntity.country.toUtf8().constData()),
                                 tr("Based on callsign, DXCC Country is different from the entered value - expecting ") + "<b> " + QCoreApplication::translate("DBStrings", dxccEntity.country.toUtf8().constData()) + "</b>");

    allValid &= highlightInvalid(ui->contLabel,
                                 dxccEntity.dxcc && ui->contEdit->currentText() != dxccEntity.cont,
                                 tr("Based on callsign, DXCC Continent is different from the entered value - expecting ") + "<b> " + dxccEntity.cont + "</b>");

    allValid &= highlightInvalid(ui->ituLabel,
                                 dxccEntity.dxcc && ui->ituEdit->text() != QString::number(dxccEntity.ituz),
                                 tr("Based on callsign, DXCC ITU is different from the entered value - expecting ") + "<b> " + QString::number(dxccEntity.ituz) + "</b>");

    allValid &= highlightInvalid(ui->cqLabel,
                                 dxccEntity.dxcc && ui->cqEdit->text() != QString::number(dxccEntity.cqz),
                                 tr("Based on callsign, DXCC CQZ is different from the entered value - expecting ") + "<b> " + QString::number(dxccEntity.cqz) + "</b>");

    allValid &= highlightInvalid(ui->vuccLabel,
                                 !ui->vuccEdit->text().isEmpty() && !ui->vuccEdit->hasAcceptableInput(),
                                 tr("VUCC has an incorrect format"));

    const QString &expectedSatMode = (bandTX.satDesignator.isEmpty() || bandRX.satDesignator.isEmpty() ) ? ""
                                                                                                         : bandTX.satDesignator + bandRX.satDesignator;
    allValid &= highlightInvalid(ui->satModeLabel,
                                 ui->satModeEdit->currentIndex() > 0 && Data::instance()->satModeTextToID(ui->satModeEdit->currentText()) != expectedSatMode,
                                 tr("Based on Frequencies, Sat Mode should be ") + "<b>" + ( (expectedSatMode.isEmpty()) ? tr("blank") : expectedSatMode) + "</b>");

    allValid &= highlightInvalid(ui->satNameLabel,
                                 Data::instance()->propagationModeTextToID(ui->propagationModeEdit->currentText()) == "SAT" && ui->satNameEdit->text().isEmpty(),
                                 tr("Sat name must not be empty"));

    const DxccEntity &myDxccEntity = Data::instance()->lookupDxcc(ui->myCallsignEdit->text());

    allValid &= highlightInvalid(ui->myCallsignLabel,
                                 ui->myCallsignEdit->text().isEmpty(),
                                 tr("Own Callsign must not be empty"));

    allValid &= highlightInvalid(ui->myCallsignLabel,
                                 !ui->myCallsignEdit->text().isEmpty() && !ui->myCallsignEdit->hasAcceptableInput(),
                                 tr("Own callsign has an incorrect format"));

    allValid &= highlightInvalid(ui->myGridLabel,
                                 !ui->myGridEdit->text().isEmpty() && !ui->myGridEdit->hasAcceptableInput(),
                                 tr("DX Grid has an incorrect format"));

    allValid &= highlightInvalid(ui->myVUCCLabel,
                                 !ui->myVUCCEdit->text().isEmpty() && !ui->myVUCCEdit->hasAcceptableInput(),
                                 tr("Own VUCC Grids have an incorrect format"));

    allValid &= highlightInvalid(ui->myITULabel,
                                 myDxccEntity.dxcc && ui->myITUEdit->text() != QString::number(myDxccEntity.ituz),
                                 tr("Based on own callsign, own DXCC ITU is different from the entered value - expecting ") + "<b> " + QString::number(myDxccEntity.ituz) + "</b>");

    allValid &= highlightInvalid(ui->myCQLabel,
                                 myDxccEntity.dxcc && ui->myCQEdit->text() != QString::number(myDxccEntity.cqz),
                                 tr("Based on own callsign, own DXCC CQZ is different from the entered value - expecting ") + "<b> " + QString::number(myDxccEntity.cqz) + "</b>");

    allValid &= highlightInvalid(ui->myCountryLabel,
                                 myDxccEntity.dxcc && ui->myCountryBox->currentText() != QCoreApplication::translate("DBStrings", myDxccEntity.country.toUtf8().constData()),
                                 tr("Based on own callsign, own DXCC Country is different from the entered value - expecting ") + "<b> " + QCoreApplication::translate("DBStrings", myDxccEntity.country.toUtf8().constData()) + "</b>");

    SOTAEntity sotaInfo;
    POTAEntity potaInfo;

    if ( !ui->sotaEdit->text().isEmpty() )
    {
        sotaInfo = Data::instance()->lookupSOTA(ui->sotaEdit->text());
    }

    if ( !ui->potaEdit->text().isEmpty() )
    {
        potaInfo = Data::instance()->lookupPOTA(ui->potaEdit->text());
    }

    allValid &= highlightInvalid(ui->qthLabel,
                                 sotaInfo.summitCode.toUpper() == ui->sotaEdit->text().toUpper()
                                 && !sotaInfo.summitName.isEmpty()
                                 && ui->qthEdit->text().toUpper() != sotaInfo.summitName.toUpper(),
                                 tr("Based on SOTA Summit, QTH does not match SOTA Summit Name - expecting ")+ "<b> " + sotaInfo.summitName + "</b>");

    Gridsquare SOTAGrid(sotaInfo.latitude, sotaInfo.longitude);

    allValid &= highlightInvalid(ui->gridLabel,
                                 sotaInfo.summitCode.toUpper() == ui->sotaEdit->text().toUpper()
                                 && !sotaInfo.summitName.isEmpty()
                                 && SOTAGrid.isValid()
                                 && ui->gridEdit->text().toUpper() != SOTAGrid.getGrid().toUpper(),
                                 tr("Based on SOTA Summit, Grid does not match SOTA Grid - expecting ")+ "<b> " + SOTAGrid.getGrid() + "</b>");

    allValid &= highlightInvalid(ui->qthLabel,
                                 potaInfo.reference.toUpper() == ui->potaEdit->text().toUpper()
                                 && !potaInfo.name.isEmpty()
                                 && ui->qthEdit->text().toUpper() != potaInfo.name.toUpper(),
                                 tr("Based on POTA record, QTH does not match POTA Name - expecting ")+ "<b> " + potaInfo.name + "</b>");

    Gridsquare POTAGrid(potaInfo.grid);

    allValid &= highlightInvalid(ui->gridLabel,
                                 potaInfo.reference.toUpper() == ui->potaEdit->text().toUpper()
                                 && !potaInfo.name.isEmpty()
                                 && POTAGrid.isValid()
                                 && ui->gridEdit->text().toUpper() != POTAGrid.getGrid().toUpper(),
                                 tr("Based on POTA record, Grid does not match POTA Grid - expecting ")+ "<b> " + POTAGrid.getGrid() + "</b>");

    SOTAEntity mySotaInfo;
    POTAEntity myPotaInfo;

    if ( !ui->mySOTAEdit->text().isEmpty() )
    {
        mySotaInfo = Data::instance()->lookupSOTA(ui->mySOTAEdit->text());
    }

    if ( !ui->myPOTAEdit->text().isEmpty() )
    {
        myPotaInfo = Data::instance()->lookupPOTA(ui->myPOTAEdit->text());
    }

    allValid &= highlightInvalid(ui->myQTHLabel,
                                 mySotaInfo.summitCode.toUpper() == ui->mySOTAEdit->text().toUpper()
                                 && !mySotaInfo.summitName.isEmpty()
                                 && ui->myQTHEdit->text().toUpper() != mySotaInfo.summitName.toUpper(),
                                 tr("Based on SOTA Summit, my QTH does not match SOTA Summit Name - expecting ")+ "<b> " + mySotaInfo.summitName + "</b>");

    Gridsquare MySOTAGrid(mySotaInfo.latitude, mySotaInfo.longitude);

    allValid &= highlightInvalid(ui->myGridLabel,
                                 mySotaInfo.summitCode.toUpper() == ui->mySOTAEdit->text().toUpper()
                                 && !mySotaInfo.summitName.isEmpty()
                                 && MySOTAGrid.isValid()
                                 && ui->myGridEdit->text().toUpper() != MySOTAGrid.getGrid().toUpper(),
                                 tr("Based on SOTA Summit, my Grid does not match SOTA Grid - expecting ")+ "<b> " + MySOTAGrid.getGrid() + "</b>");

    allValid &= highlightInvalid(ui->myQTHLabel,
                                 myPotaInfo.reference.toUpper() == ui->myPOTAEdit->text().toUpper()
                                 && !myPotaInfo.name.isEmpty()
                                 && ui->myQTHEdit->text().toUpper() != myPotaInfo.name.toUpper(),
                                 tr("Based on POTA record, my QTH does not match POTA Name - expecting ")+ "<b> " + myPotaInfo.name + "</b>");

    Gridsquare myPOTAGrid(myPotaInfo.grid);

    allValid &= highlightInvalid(ui->myGridLabel,
                                 myPotaInfo.reference.toUpper() == ui->myPOTAEdit->text().toUpper()
                                 && !myPotaInfo.name.isEmpty()
                                 && myPOTAGrid.isValid()
                                 && ui->myGridEdit->text().toUpper() != myPOTAGrid.getGrid().toUpper(),
                                 tr("Based on POTA record, my Grid does not match POTA Grid - expecting ")+ "<b> " + myPOTAGrid.getGrid() + "</b>");

    allValid &= highlightInvalid(ui->lotwHeaderLabel,
                                 ui->qslLotwSentDateEdit->date() != ui->qslLotwSentDateEdit->minimumDate()
                                 && ui->qslLotwSentStatusBox->currentData().toString() == "N",
                                 tr("LoTW Sent Status to <b>No</b> does not make any sense if QSL Sent Date is set. Set Date to 1.1.1900 to leave the date field blank"));

    allValid &= highlightInvalid(ui->lotwHeaderLabel,
                                 ui->qslLotwSentDateEdit->date() == ui->qslLotwSentDateEdit->minimumDate()
                                 && ( ui->qslLotwSentStatusBox->currentData().toString() == "Y"
                                 //     || ui->qslLotwSentStatusBox->currentData().toString() == "Q" // QLog does not set date for Q state
                                 //     || ui->qslLotwSentStatusBox->currentData().toString() == "I" // QLog does not set date for I state
                                 ),
                                 tr("Date should be present for LoTW Sent Status <b>Yes</b>"));

    allValid &= highlightInvalid(ui->eqslHeaderLabel,
                                 ui->qslEqslSentDateEdit->date() != ui->qslEqslSentDateEdit->minimumDate()
                                 && ui->qslEqslSentStatusBox->currentData().toString() == "N",
                                 tr("eQSL Sent Status to <b>No</b> does not make any sense if QSL Sent Date is set. Set Date to 1.1.1900 to leave the date field blank"));

    allValid &= highlightInvalid(ui->eqslHeaderLabel,
                                 ui->qslEqslSentDateEdit->date() == ui->qslEqslSentDateEdit->minimumDate()
                                 && ( ui->qslEqslSentStatusBox->currentData().toString() == "Y"
                                 //     || ui->qslEqslSentStatusBox->currentData().toString() == "Q" // QLog does not set date for Q state
                                 //     || ui->qslEqslSentStatusBox->currentData().toString() == "I" // QLog does not set date for I state
                                 ),
                                 tr("Date should be present for eQSL Sent Status <b>Yes</b>"));

    allValid &= highlightInvalid(ui->qslSentLabel,
                                 ui->qslPaperSentDateEdit->date() != ui->qslPaperSentDateEdit->minimumDate()
                                 && ui->qslPaperSentStatusBox->currentData().toString() == "N",
                                 tr("Paper Sent Status to <b>No</b> does not make any sense if QSL Sent Date is set. Set Date to 1.1.1900 to leave the date field blank"));

    allValid &= highlightInvalid(ui->qslSentLabel,
                                 ui->qslPaperSentDateEdit->date() == ui->qslPaperSentDateEdit->minimumDate()
                                 && ( ui->qslPaperSentStatusBox->currentData().toString() == "Y"
                                 //     || ui->qslPaperSentStatusBox->currentData().toString() == "Q" // QLog does not set date for Q state
                                 //     || ui->qslPaperSentStatusBox->currentData().toString() == "I" // QLog does not set date for I state
                                 ),
                                 tr("Date should be present for Paper Sent Status <b>Yes</b>"));

    qCDebug(runtime) << "Validation result: " << allValid;
    return allValid;
}

void QSODetailDialog::doValidationDateTime(const QDateTime &)
{
    doValidation();
}

void QSODetailDialog::doValidationDouble(double)
{
    doValidation();
}

void QSODetailDialog::mapLoaded(bool)
{
    FCT_IDENTIFICATION;

    isMainPageLoaded = true;

    /* which layers will be active */
    postponedScripts += layerControlHandler.generateMapMenuJS(true,
                                                              true,
                                                              false,
                                                              false,
                                                              false,
                                                              false,
                                                              false,
                                                              false,
                                                              true);

    main_page->runJavaScript(postponedScripts);

    const QPalette &defaultPalette = this->palette();
    const QColor &text = defaultPalette.color(QPalette::WindowText);
    const QColor &window = defaultPalette.color(QPalette::Window);
    bool isDark = text.lightness() > window.lightness();

    if ( isDark )
    {
        QString themeJavaScript = "map.getPanes().tilePane.style.webkitFilter=\"brightness(0.6) invert(1) contrast(3) hue-rotate(200deg) saturate(0.3) brightness(0.9)\";";
        main_page->runJavaScript(themeJavaScript);
    }

    layerControlHandler.restoreLayerControlStates(main_page);
}

void QSODetailDialog::myGridChanged(const QString &newGrid)
{
    FCT_IDENTIFICATION;

    drawMyQTHOnMap(ui->myCallsignEdit->text(), Gridsquare(newGrid));

    return;
}

void QSODetailDialog::DXGridChanged(const QString &newGrid)
{
    FCT_IDENTIFICATION;

    drawDXOnMap(ui->callsignEdit->text(), Gridsquare(newGrid));

    return;
}

void QSODetailDialog::callsignFound(const CallbookResponseData &data)
{
    FCT_IDENTIFICATION;

    callbookLookupFinished();

    /* blank or not fully filled then update it */
    const QString fnamelname = QString("%1 %2").arg(data.fname, data.lname);

    if (  ui->nameEdit->text().isEmpty()
          || data.name_fmt.contains(ui->nameEdit->text())
          || fnamelname.contains(ui->nameEdit->text())
          || data.nick.contains(ui->nameEdit->text()) )
    {
        QString name = data.name_fmt;

        if ( name.isEmpty() )
            name = ( data.fname.isEmpty() && data.lname.isEmpty() ) ? data.nick
                                                                    : fnamelname;
        ui->nameEdit->setText(name);
    }

    if ( ui->gridEdit->text().isEmpty()
         || data.gridsquare.contains(ui->gridEdit->text()) )
        ui->gridEdit->setText(data.gridsquare);

    if ( ui->qthEdit->text().isEmpty()
         || data.qth.contains(ui->qthEdit->text()))
        ui->qthEdit->setText(data.qth);

    if ( ui->dokEdit->text().isEmpty() )    ui->dokEdit->setText(data.dok);
    if ( ui->iotaEdit->text().isEmpty() )   ui->iotaEdit->setText(data.iota);
    if ( ui->emailEdit->text().isEmpty() )  ui->emailEdit->setText(data.email);
    if ( ui->countyEdit->text().isEmpty() ) ui->countyEdit->setText(data.county);
    if ( ui->qslViaEdit->text().isEmpty() ) ui->qslViaEdit->setText(data.qsl_via);
    if ( ui->urlEdit->text().isEmpty() )    ui->urlEdit->setText(data.url);
    if ( ui->stateEdit->text().isEmpty() )  ui->stateEdit->setText(data.us_state);
    if ( ui->ituEdit->text().isEmpty() )    ui->ituEdit->setText(data.ituz);
    if ( ui->cqEdit->text().isEmpty() )     ui->cqEdit->setText(data.cqz);
}

void QSODetailDialog::callsignNotFound(const QString &)
{
    FCT_IDENTIFICATION;

    /* Do not show any info, not needed */
    callbookLookupFinished();
}

void QSODetailDialog::callbookLoginFailed(const QString&)
{
    FCT_IDENTIFICATION;

    /* It is not needed to show an Error dialog because Login failed emits also callbookError signal */
    /* QLog will show only callbookError */
    //QMessageBox::critical(this, tr("QLog Error"), callbookString + " " + tr("Callbook login failed"));
}

void QSODetailDialog::callbookError(const QString &error)
{
    FCT_IDENTIFICATION;

    QMessageBox::critical(this, tr("QLog Error"), tr("Callbook error: ") + error);
}

void QSODetailDialog::handleBeforeUpdate(int, QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    emit contactUpdated(record);
}

void QSODetailDialog::sotaChanged(const QString &newSOTA)
{
    FCT_IDENTIFICATION;

    if ( newSOTA.length() >= 3 )
    {
        ui->sotaEdit->setCompleter(sotaCompleter.data());
    }
    else
    {
        ui->sotaEdit->setCompleter(nullptr);
    }
}

void QSODetailDialog::potaChanged(const QString &newPOTA)
{
    FCT_IDENTIFICATION;

    if ( newPOTA.length() >= 3 )
    {
        ui->potaEdit->setCompleter(potaCompleter.data());
    }
    else
    {
        ui->potaEdit->setCompleter(nullptr);
    }
}

void QSODetailDialog::wwffChanged(const QString &newWWFF)
{
    FCT_IDENTIFICATION;

    if ( newWWFF.length() >= 3 )
    {
        ui->wwffEdit->setCompleter(wwffCompleter.data());
    }
    else
    {
        ui->wwffEdit->setCompleter(nullptr);
    }
}

void QSODetailDialog::mySotaChanged(const QString &newSOTA)
{
    FCT_IDENTIFICATION;

    if ( newSOTA.length() >= 3 )
    {
        ui->mySOTAEdit->setCompleter(sotaCompleter.data());
    }
    else
    {
        ui->mySOTAEdit->setCompleter(nullptr);
    }
}

void QSODetailDialog::myPOTAChanged(const QString &newPOTA)
{
    FCT_IDENTIFICATION;

    if ( newPOTA.length() >= 3 )
    {
        ui->myPOTAEdit->setCompleter(potaCompleter.data());
    }
    else
    {
        ui->myPOTAEdit->setCompleter(nullptr);
    }
}

void QSODetailDialog::myWWFFChanged(const QString &newWWFF)
{
    FCT_IDENTIFICATION;

    if ( newWWFF.length() >= 3 )
    {
        ui->myWWFFEdit->setCompleter(wwffCompleter.data());
    }
    else
    {
        ui->myWWFFEdit->setCompleter(nullptr);
    }
}

void QSODetailDialog::clubQueryResult(const QString &in_callsign,
                                      QMap<QString, ClubStatusQuery::ClubInfo> data)
{
    FCT_IDENTIFICATION;

    if ( in_callsign != ui->callsignEdit->text().toUpper() )
    {
        // do not need this result
        return;
    }

    QString memberText;

    QMapIterator<QString, ClubStatusQuery::ClubInfo> clubs(data);

    QPalette palette;

    //"<font color='red'>Hello</font> <font color='green'>World</font>"
    while ( clubs.hasNext() )
    {
        clubs.next();
        QColor color = Data::statusToColor(static_cast<DxccStatus>(clubs.value().status), false, palette.color(QPalette::Text));
        memberText.append(QString("<font color='%1'>%2</font>&nbsp;&nbsp;&nbsp;").arg(Data::colorToHTMLColor(color), clubs.key()));
    }
    ui->memberListLabel->setText(memberText);
}

bool QSODetailDialog::highlightInvalid(QLabel *labelWidget, bool cond, const QString &toolTip)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << labelWidget->objectName()
                                 << cond
                                 << toolTip;

    QString currToolTip(labelWidget->toolTip());

    if ( cond )
    {
        if ( ! currToolTip.contains(toolTip) )
        {
            currToolTip.append(tr("<b>Warning: </b>") + toolTip + "<br>");
        }
    }
    else
    {
        currToolTip.remove(tr("<b>Warning: </b>") + toolTip + "<br>");
    }

    if ( currToolTip.isEmpty() )
    {
        labelWidget->setStyleSheet("");
    }
    else
    {
        labelWidget->setStyleSheet("color: black; border-radius: 5px; background: yellow;");
    }

    labelWidget->setToolTip(currToolTip);
    return !cond;
}

void QSODetailDialog::blockMappedWidgetSignals(bool inBlocking)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << inBlocking;

    for ( int i = 0; i < LogbookModel::COLUMN_LAST_ELEMENT; i++)
    {
        QWidget *widget = mapper->mappedWidgetAt(i);

        if ( widget )
        {
            widget->blockSignals(inBlocking);
        }
    }

    ui->modeEdit->blockSignals(false); //exception - submode must be filled based on Mode combobox
}

void QSODetailDialog::drawDXOnMap(const QString &label, const Gridsquare &dxGrid)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << label << dxGrid;

    if ( !dxGrid.isValid() )
    {
        return;
    }

    QString stationString;
    QString popupString = label;

    Gridsquare myGrid = Gridsquare(ui->myGridEdit->text());
    double distance = 0;

    if (dxGrid.distanceTo(myGrid, distance))
    {
        QString unit;
        distance = Gridsquare::distance2localeUnitDistance(distance, unit);
        popupString.append(QString("</br> %1 %2").arg(QString::number(distance, 'f', 0), unit));
    }

    double lat = dxGrid.getLatitude();
    double lon = dxGrid.getLongitude();
    stationString.append(QString("[[\"%1\", %2, %3, yellowIcon]]").arg(popupString).arg(lat).arg(lon));

    QString shortPath = QString("[%1, %2, %3, %4]")
                            .arg(myGrid.getLatitude())
                            .arg(myGrid.getLongitude())
                            .arg(lat)
                            .arg(lon);

    QString javaScript = QString("grids_confirmed = [];"
                                 "grids_worked = [];"
                                 "drawPoints(%1);"
                                 "drawShortPaths([%2]);"
                                 "maidenheadConfWorked.redraw();"
                                 "flyToPoint(%3[0], 6);")
                             .arg(stationString, shortPath, stationString);

    qCDebug(runtime) << javaScript;

    if ( !isMainPageLoaded )
    {
        postponedScripts.append(javaScript);
    }
    else
    {
        main_page->runJavaScript(javaScript);
    }
}

void QSODetailDialog::drawMyQTHOnMap(const QString &label, const Gridsquare &myGrid)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << label << myGrid;

    if ( ! myGrid.isValid() )
    {
        return;
    }

    QString stationString;
    double lat = myGrid.getLatitude();
    double lon = myGrid.getLongitude();
    stationString.append(QString("[[\"%1\", %2, %3, homeIcon]]").arg(label).arg(lat).arg(lon));

    QString javaScript = QString("grids_confirmed = [];"
                                 "grids_worked = [];"
                                 "drawPointsGroup2(%1);"
                                 "maidenheadConfWorked.redraw();").arg(stationString);

    qCDebug(runtime) << javaScript;

    if ( !isMainPageLoaded )
    {
        postponedScripts.append(javaScript);
    }
    else
    {
        main_page->runJavaScript(javaScript);
    }
}

void QSODetailDialog::setStaticMapTime(const QDateTime &dateTime)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << dateTime;

    QString javaScript = QString("setStaticMapTime(new Date(%1));").arg(dateTime.toMSecsSinceEpoch());

    qCDebug(runtime) << javaScript;

    if (!isMainPageLoaded) {
        postponedScripts.append(javaScript);
    } else {
        main_page->runJavaScript(javaScript);
    }
}

void QSODetailDialog::enableWidgetChangeHandlers()
{
    FCT_IDENTIFICATION;

    for ( int i = 0; i < LogbookModel::COLUMN_LAST_ELEMENT; i++)
    {
        QWidget *widget = mapper->mappedWidgetAt(i);

        if ( !widget ) continue;

        if ( QLineEdit *line = qobject_cast<QLineEdit*>(widget) )
        {
            connect(line, &QLineEdit::textChanged, this, &QSODetailDialog::doValidation);
            connect(line, &QLineEdit::textChanged, this, [line]()
            {
                line->setStyleSheet(CHANGECSS);
            });
        }
        else if ( QTextEdit *edit = qobject_cast<QTextEdit*>(widget) )
        {
            connect(edit, &QTextEdit::textChanged, this, &QSODetailDialog::doValidation);
            connect(edit, &QTextEdit::textChanged, this, [edit]()
            {
                edit->setStyleSheet(CHANGECSS);
            });
        }
        else if ( QDoubleSpinBox *spin = qobject_cast<QDoubleSpinBox*>(widget) )
        {
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &QSODetailDialog::doValidationDouble);
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [spin](double)
            {
                spin->setStyleSheet(CHANGECSS);
            });
        }
        else if ( QDateTimeEdit *datetime = qobject_cast<QDateTimeEdit*>(widget) )
        {
            connect(datetime, &QDateTimeEdit::dateTimeChanged, this, &QSODetailDialog::doValidationDateTime);
            connect(datetime, &QDateTimeEdit::dateTimeChanged, this, [datetime](QDateTime)
            {
                datetime->setStyleSheet(CHANGECSS);
            });
        }
        else if ( QComboBox *combo = qobject_cast<QComboBox*>(widget) )
        {
            connect(combo, &QComboBox::currentTextChanged, this, &QSODetailDialog::doValidation);
            connect(combo, &QComboBox::currentTextChanged, this, [combo](QString)
            {
                combo->setStyleSheet(CHANGECSS);
            });
        }
        else if ( SmartSearchBox *combo = qobject_cast<SmartSearchBox*>(widget) )
        {
            connect(combo, &SmartSearchBox::currentTextChanged, this, &QSODetailDialog::doValidation);
            connect(combo, &SmartSearchBox::currentTextChanged, this, [combo](QString)
            {
                combo->setStyleSheet(CHANGECSS);
            });
        }
    }
}

void QSODetailDialog::lookupButtonWaitingStyle(bool isWaiting)
{
    FCT_IDENTIFICATION;

    if ( isWaiting )
    {
        lookupButtonMovie->start();
    }
    else
    {
        lookupButtonMovie->stop();

        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/baseline-search-24px.svg"));
        lookupButton->setIcon(icon);
    }
}

QSODetailDialog::SubmitError QSODetailDialog::submitAllChanges()
{
    FCT_IDENTIFICATION;

    if ( ! doValidation() )
    {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Validation"), tr("Yellow marked fields are invalid.<p>Nevertheless, save the changes?</p>"),
                                      QMessageBox::Yes|QMessageBox::No);

        if (reply != QMessageBox::Yes) return QSODetailDialog::SubmitCancelledByUser;
    }

    if ( ! mapper->submit() )
    {
        qWarning("Mapper was not submitted");
        return QSODetailDialog::SubmitMapperError;
    }

    if ( ! model->submit() )
    {
        qWarning("Model was not submitted");
        return QSODetailDialog::SubmitModelError;
    }

    return QSODetailDialog::SubmitOK;
}

void QSODetailDialog::callbookLookupFinished()
{
    FCT_IDENTIFICATION;

    lookupButton->setEnabled(true);
    resetButton->setEnabled(true);
    editButton->setEnabled(true);
    lookupButtonWaitingStyle(false);
}

void QSODetailDialog::callbookLookupStart()
{
    FCT_IDENTIFICATION;

    lookupButton->setEnabled(false);
    resetButton->setEnabled(false);
    editButton->setEnabled(false);
    lookupButtonWaitingStyle(true);
}

void QSODetailDialog::refreshDXStatTabs()
{
    FCT_IDENTIFICATION;

    const DxccEntity &dxccEntity = Data::instance()->lookupDxccID(editedRecord->field("dxcc").value().toInt());
    const Band &currBand = BandPlan::freq2Band(ui->freqTXEdit->value());

    ui->dxccTableWidget->setDxcc(dxccEntity.dxcc, currBand);
    ui->stationTableWidget->setDxCallsign(ui->callsignEdit->text(), currBand);
}

const QString QSODetailDialog::getButtonText(int index) const
{
    FCT_IDENTIFICATION;

    static const char *buttonText[] =
    {
        QT_TR_NOOP("&Save"),
        QT_TR_NOOP("&Edit")
    };

    return tr(buttonText[index]);
}

void QSOEditMapperDelegate::setEditorData(QWidget *editor,
                                          const QModelIndex &index) const
{
    if ( editor->objectName() == "qslSentBox"
         || editor->objectName() == "qslSentViaBox"
         || editor->objectName() == "qslPaperSentStatusBox"
         || editor->objectName() == "qslPaperReceiveStatusBox"
         || editor->objectName() == "qslLotwSentStatusBox"
         || editor->objectName() == "qslEqslSentStatusBox"
         )
    {
        QComboBox* combo = qobject_cast<QComboBox*>(editor);

        if ( combo )
        {
            combo->setCurrentIndex(combo->findData(index.data()));
        }
        return;
    }
    else if ( editor->objectName() == "propagationModeEdit" )
    {
        QComboBox* combo = qobject_cast<QComboBox*>(editor);

        if ( combo )
        {
            combo->setCurrentText(Data::instance()->propagationModeIDToText(index.data().toString()));
        }
        return;
    }
    else if ( editor->objectName() == "satModeEdit" )
    {
        QComboBox* combo = qobject_cast<QComboBox*>(editor);

        if ( combo )
        {
            combo->setCurrentText(Data::instance()->satModeIDToText(index.data().toString()));
        }
        return;
    }
    else if ( editor->objectName() == "qslEqslReceiveDateLabel"
              || editor->objectName() == "qslLotwReceiveDateLabel" )
    {
        QLabel* label = qobject_cast<QLabel*>(editor);

        if ( label )
        {
            if ( !index.data().toString().isEmpty() )
            {
                label->setText(index.data().toDate().toString(locale.formatDateShortWithYYYY()));
            }
        }
        return;
    }
    else if ( editor->objectName() == "qslEqslReceiveStatusLabel"
              || editor->objectName() == "qslLotwReceiveStatusLabel"
            )
    {
        QLabel* label = qobject_cast<QLabel*>(editor);

        if ( label )
        {
          QString statusIcon = QString("<img src=':/icons/%1-24px.svg'></td>").arg((index.data().toString() == "Y") ? "done" : "close");
          label->setText(statusIcon);
          label->setProperty("originvalue", index.data());
        }
        return;
    }
    else if ( editor->objectName() == "countryBox"
              || editor->objectName() == "myCountryBox" )
    {
        SmartSearchBox* combo = qobject_cast<SmartSearchBox*>(editor);

        if ( combo )
            combo->setCurrentValue(index.data(), 1);
        return;
    }
    else if ( editor->objectName() == "noteEdit" )
    {
        QTextEdit *textEdit = static_cast<QTextEdit*>(editor);

        if ( textEdit )
        {
            QString value = index.data().toString();
            textEdit->setPlainText(value);
            textEdit->setAcceptRichText(false);
            return;
        }
    }
    else if ( editor->objectName() == "freqRXEdit" )
    {
        QDoubleSpinBox *spin = static_cast<QDoubleSpinBox*>(editor);

        if ( spin )
        {
            spin->setValue(index.data().toDouble());
            return;
        }
    }
    else if ( editor->objectName() == "qslPaperReceiveDateEdit"
              || editor->objectName() == "qslPaperSentDateEdit"
              || editor->objectName() == "qslLotwSentDateEdit"
              || editor->objectName() == "qslEqslSentDateEdit" )
    {
        QDateEdit *dateEdit = qobject_cast<QDateEdit*>(editor);

        if ( dateEdit )
        {
            if ( !index.data().toDate().isValid() )
            {
                dateEdit->setDate(dateEdit->minimumDate());
                return;
            }
        }
    }

    QItemDelegate::setEditorData(editor, index);

    // Hack: all NewContactEditLines should display
    // an initial part of the line.
    // Do not insert this functionality to NewContactEditLines because
    // this function is wanted only for the QSODetail Widget.
    NewContactEditLine* lineEdit = qobject_cast<NewContactEditLine*>(editor);
    if ( lineEdit )
    {
        lineEdit->home(false);
    }
}

void QSOEditMapperDelegate::setModelData(QWidget *editor,
                                         QAbstractItemModel *model,
                                         const QModelIndex &index) const
{
    /* ALL combos with Data */
    if ( editor->objectName() == "qslSentBox"
         || editor->objectName() == "qslSentViaBox"
         || editor->objectName() == "qslPaperSentStatusBox"
         || editor->objectName() == "qslPaperReceiveStatusBox"
         || editor->objectName() == "qslLotwSentStatusBox"
         || editor->objectName() == "qslEqslSentStatusBox"
       )
    {
        QComboBox* combo = static_cast<QComboBox*>(editor);

        if ( combo )
        {
            model->setData(index, combo->currentData());
            return;
        }
    }
    else if ( editor->objectName() == "propagationModeEdit" )
    {
        QComboBox* combo = static_cast<QComboBox*>(editor);

        if ( combo )
        {
            model->setData(index, Data::instance()->propagationModeTextToID(combo->currentText()));
            return;
        }
    }
    else if ( editor->objectName() == "satModeEdit" )
    {
        QComboBox* combo = static_cast<QComboBox*>(editor);

        if ( combo )
        {
            model->setData(index, Data::instance()->satModeTextToID(combo->currentText()));
            return;
        }
    }
    else if ( editor->objectName() == "noteEdit" )
    {
        QTextEdit *textEdit = static_cast<QTextEdit*>(editor);

        if ( textEdit )
        {
            model->setData(index, textEdit->toPlainText());
            return;
        }
    }
    else if ( editor->objectName() == "qslPaperReceiveDateEdit"
              || editor->objectName() == "qslPaperSentDateEdit"
              || editor->objectName() == "qslLotwSentDateEdit"
              || editor->objectName() == "qslEqslSentDateEdit" )
    {
        QDateEdit *dateEdit = qobject_cast<QDateEdit*>(editor);

        if ( dateEdit )
        {
            if ( dateEdit->date() == dateEdit->minimumDate() )
            {
                 model->setData(index, QVariant());
            }
            else
            {
                 model->setData(index, dateEdit->date());
            }
            return;
        }
    }
    else if ( editor->objectName() == "countryBox"
              || editor->objectName() == "myCountryBox")
    {
        SmartSearchBox *box = qobject_cast<SmartSearchBox*>(editor);

        if ( box )
        {
            int row = box->currentIndex();
            QVariant dataDXCC;
            QVariant dataCountryEN;

            if ( row > 0 ) // the first line is an empty line
            {
                dataDXCC = box->currentValue(1);
                dataCountryEN = box->currentValue(3);
            }

            model->setData(index, dataDXCC);
            model->setData(model->index(index.row(),
                                        (editor->objectName() == "countryBox" ) ? LogbookModel::COLUMN_COUNTRY_INTL
                                                                                : LogbookModel::COLUMN_MY_COUNTRY_INTL),
                           dataCountryEN);
        }
        return;
    }
    else if (    editor->objectName() == "qslEqslReceiveDateLabel"
              || editor->objectName() == "qslEqslReceiveStatusLabel"
              || editor->objectName() == "qslLotwReceiveDateLabel"
              || editor->objectName() == "qslLotwReceiveStatusLabel"
            )
    {
        /* do not save */
        return;
    }
    else if ( editor->objectName() == "bandRXCombo"
              || editor->objectName() == "bandTXCombo" )
    {
        QComboBox* combo = static_cast<QComboBox*>(editor);

        if ( combo )
        {
            if ( combo->currentIndex() == 0 )
            {
                model->setData(index, QVariant());
            }
            else
            {
                model->setData(index, combo->currentText());
            }
        }
        return;
    }

    QItemDelegate::setModelData(editor, model, index);
}

bool QSOEditMapperDelegate::eventFilter(QObject *object, QEvent *event)
{

    /* need to be eventFilter before Key Press IF because
     * this signal is used to reset Stylesheet for widget that is reset. And emit
     * has to be callled at the end of operation */
    bool ret = QItemDelegate::eventFilter(object, event);

    if (event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)
        {
            emit keyEscapePressed(object);
        }
    }

    return ret;
}

QSODetailDialog::LogbookModelPrivate::LogbookModelPrivate(QObject *parent, QSqlDatabase db)
    : LogbookModel(parent, db)
{
    setTable("contacts");
    setEditStrategy(QSqlTableModel::OnRowChange);
}

QVariant QSODetailDialog::LogbookModelPrivate::data(const QModelIndex &index, int role) const
{
    return QSqlTableModel::data(index, role); // clazy:exclude=skipped-base-method
}

bool QSODetailDialog::LogbookModelPrivate::setData(const QModelIndex &index, const QVariant &value, int role)
{
    bool main_update_result = true;
    bool depend_update_result = true;

    if ( role == Qt::EditRole )
    {
        switch ( index.column() )
        {

        case COLUMN_CALL:
        {
            const QString &pfxRef = Callsign(value.toString()).getWPXPrefix();

            if ( !pfxRef.isEmpty() )
            {
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_PREFIX), QVariant(pfxRef), role); // clazy:exclude=skipped-base-method
            }
            break;
        }

        case COLUMN_GRID:
        {
            if ( ! value.toString().isEmpty() )
            {
                Gridsquare newgrid(value.toString());

                if ( newgrid.isValid() )
                {
                    Gridsquare mygrid(QSqlTableModel::data(this->index(index.row(), COLUMN_MY_GRIDSQUARE), Qt::DisplayRole).toString()); // clazy:exclude=skipped-base-method
                    double distance;

                    if ( mygrid.distanceTo(newgrid, distance) )
                    {
                        depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(distance),role); // clazy:exclude=skipped-base-method
                    }
                    else
                    {
                        depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(),role); // clazy:exclude=skipped-base-method
                    }
                }
                else
                {
                    /* do not update field with invalid Grid */
                    depend_update_result = false;
                }
            }
            else
            {
                /* empty grid is valid (when removing a value); need to remove also Distance */
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(),role); // clazy:exclude=skipped-base-method
            }
            break;
        }

        case COLUMN_MY_GRIDSQUARE:
        {
            if ( ! value.toString().isEmpty() )
            {
                Gridsquare mynewGrid(value.toString());

                if ( mynewGrid.isValid() )
                {
                    Gridsquare dxgrid(QSqlTableModel::data(this->index(index.row(), COLUMN_GRID), Qt::DisplayRole).toString()); // clazy:exclude=skipped-base-method
                    double distance;

                    if ( mynewGrid.distanceTo(dxgrid, distance) )
                    {
                        depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(distance),role); // clazy:exclude=skipped-base-method
                    }
                    else
                    {
                        depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(),role); // clazy:exclude=skipped-base-method
                    }
                }
                else
                {
                    /* do not update field with invalid Grid */
                    depend_update_result = false;
                }
            }
            else
            {
                /* empty grid is valid (when removing a value); need to remove also Distance */
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(),role); // clazy:exclude=skipped-base-method
            }
            break;
        }

        case COLUMN_ADDRESS_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_ADDRESS), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_COMMENT_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_COMMENT), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_COUNTRY_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_COUNTRY), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_MY_ANTENNA_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_ANTENNA), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_MY_CITY_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_CITY), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_MY_COUNTRY_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_COUNTRY), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_MY_NAME_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_NAME), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_MY_POSTAL_CODE_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_POSTAL_CODE), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_MY_RIG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_RIG), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_MY_SIG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_SIG), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_MY_SIG_INFO_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_SIG_INFO), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_MY_STREET_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_STREET), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_NAME_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_NAME), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_NOTES_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_NOTES), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_QSLMSG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_QSLMSG), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_QTH_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_QTH), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_RIG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_RIG), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_SIG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_SIG), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_SIG_INFO_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_SIG_INFO), Data::removeAccents(value.toString()),role); // clazy:exclude=skipped-base-method
            break;
        }

        case COLUMN_SOTA_REF:
        {
            SOTAEntity sotaInfo = Data::instance()->lookupSOTA(value.toString());
            if ( sotaInfo.summitCode.toUpper() == value.toString().toUpper()
                 && !sotaInfo.summitName.isEmpty() )
            {
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_ALTITUDE), sotaInfo.altm, role); // clazy:exclude=skipped-base-method
            }
            else
            {
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_ALTITUDE), QVariant(), role); // clazy:exclude=skipped-base-method
            }
            break;
        }

        case COLUMN_MY_SOTA_REF:
        {
            SOTAEntity sotaInfo = Data::instance()->lookupSOTA(value.toString());
            if ( sotaInfo.summitCode.toUpper() == value.toString().toUpper()
                 && !sotaInfo.summitName.isEmpty() )
            {
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_ALTITUDE), sotaInfo.altm, role); // clazy:exclude=skipped-base-method
            }
            else
            {
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_ALTITUDE), QVariant(), role); // clazy:exclude=skipped-base-method
            }
            break;
        }
       }
       //updateExternalServicesUploadStatus(index, role, depend_update_result);

       if ( depend_update_result )
       {
           switch ( index.column() )
           {
           case COLUMN_FREQUENCY:
           case COLUMN_FREQ_RX:
           case COLUMN_TX_POWER:
               /* store NULL when 0.0MHz */
               main_update_result = QSqlTableModel::setData(index, ( value.toDouble() == 0.0 ) ? QVariant() // clazy:exclude=skipped-base-method
                                                                                               : value, role);
               break;

           case COLUMN_SOTA_REF:
           case COLUMN_MY_SOTA_REF:
           case COLUMN_POTA_REF:
           case COLUMN_MY_POTA_REF:
           case COLUMN_IOTA:
           case COLUMN_MY_IOTA:
           case COLUMN_MY_GRIDSQUARE:
           case COLUMN_CALL:
           case COLUMN_GRID:
           case COLUMN_VUCC_GRIDS:
           case COLUMN_MY_VUCC_GRIDS:
           case COLUMN_MY_WWFF_REF:
           case COLUMN_WWFF_REF:
           case COLUMN_STATION_CALLSIGN:
           case COLUMN_OPERATOR:
           case COLUMN_DARC_DOK:
           case COLUMN_MY_DARC_DOK:
               main_update_result = QSqlTableModel::setData(index, ( !value.toString().isEmpty() ) ? value.toString().toUpper() // clazy:exclude=skipped-base-method
                                                                                                   : QVariant(), role);
               break;

           case COLUMN_ADDRESS_INTL:
           case COLUMN_COMMENT_INTL:
           case COLUMN_COUNTRY_INTL:
           case COLUMN_MY_ANTENNA_INTL:
           case COLUMN_MY_CITY_INTL:
           case COLUMN_MY_COUNTRY_INTL:
           case COLUMN_MY_NAME_INTL:
           case COLUMN_MY_POSTAL_CODE_INTL:
           case COLUMN_MY_RIG_INTL:
           case COLUMN_MY_SIG_INTL:
           case COLUMN_MY_SIG_INFO_INTL:
           case COLUMN_MY_STREET_INTL:
           case COLUMN_NAME_INTL:
           case COLUMN_NOTES_INTL:
           case COLUMN_QSLMSG_INTL:
           case COLUMN_QTH_INTL:
           case COLUMN_RIG_INTL:
           case COLUMN_SIG_INTL:
           case COLUMN_SIG_INFO_INTL:
               main_update_result = QSqlTableModel::setData(index, ( !value.toString().isEmpty() ) ? value              // clazy:exclude=skipped-base-method
                                                                                                   : QVariant(), role);
               break;

           default:
               main_update_result = QSqlTableModel::setData(index, ( !value.toString().isEmpty() ) ? Data::removeAccents(value.toString()) // clazy:exclude=skipped-base-method
                                                                                                   : QVariant(), role);
           }
       }
    }

    return main_update_result && depend_update_result;
}
