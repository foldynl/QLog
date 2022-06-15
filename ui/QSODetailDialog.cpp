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
#include "core/Eqsl.h"
#include "models/SqlListModel.h"
#include "core/Gridsquare.h"

MODULE_IDENTIFICATION("qlog.ui.qsodetaildialog");

#define CHANGECSS "color: orange;"

QSODetailDialog::QSODetailDialog(const QSqlRecord &qso,
                                 QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QSODetailDialog),
    mapper(new QDataWidgetMapper()),
    model(new LogbookModelPrivate),
    editedRecord(new QSqlRecord(qso)),
    isMainPageLoaded(false),
    main_page(new QWebEnginePage(this))
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    /* model setting */
    model->setFilter(QString("id = '%1'").arg(qso.value("id").toString()));
    model->select();
    connect(model, &QSqlTableModel::beforeUpdate, this, &QSODetailDialog::handleBeforeUpdate);

    /* mapView setting */
    ui->mapView->setPage(main_page);
    main_page->load(QUrl(QStringLiteral("qrc:/res/map/onlinemap.html")));
    ui->mapView->setFocusPolicy(Qt::ClickFocus);
    connect(ui->mapView, &QWebEngineView::loadFinished, this, &QSODetailDialog::mapLoaded);

    /* Edit Button */
    editButton = new QPushButton(EDIT_BUTTON_TEXT);
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
    });
    lookupButtonWaitingStyle(false);

    /* Mapper setting */
    mapper->setModel(model);
    QSOEditMapperDelegate *QSOitemDelegate = new QSOEditMapperDelegate;
    mapper->setItemDelegate(QSOitemDelegate);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    connect(QSOitemDelegate, &QSOEditMapperDelegate::keyEscapePressed, this, &QSODetailDialog::resetKeyPressed);

    /* Callbook Signals registration */
    connect(&callbookManager, &CallbookManager::callsignResult,
            this, &QSODetailDialog::callsignFound);

    connect(&callbookManager, &CallbookManager::callsignNotFound,
            this, &QSODetailDialog::callsignNotFound);

    connect(&callbookManager, &CallbookManager::loginFailed, this, [this](QString callbookString)
    {
        QMessageBox::critical(this, tr("QLog Error"), callbookString + " " + tr("Callbook login failed"));
    });

    connect(&callbookManager, &CallbookManager::loginFailed, this, [this](QString callbookString)
    {
        QMessageBox::critical(this, tr("QLog Error"), callbookString + " " + tr("Callbook login failed"));
    });

    /*******************/
    /* Main Screen GUI */
    /*******************/

    /* ITU Zones Validators */
    ui->ituEdit->setValidator(new QIntValidator(Data::getITUZMin(), Data::getITUZMax(), this));

    /* CQ Zones Validators */
    ui->ituEdit->setValidator(new QIntValidator(Data::getCQZMin(), Data::getCQZMax(), this));

    /* Submode mapping */
    QStringListModel* submodeModel = new QStringListModel(this);
    ui->submodeEdit->setModel(submodeModel);

    /* Mode mapping */
    QSqlTableModel* modeModel = new QSqlTableModel();
    modeModel->setTable("modes");
    modeModel->setSort(modeModel->fieldIndex("name"), Qt::AscendingOrder);
    ui->modeEdit->setModel(modeModel);
    ui->modeEdit->setModelColumn(modeModel->fieldIndex("name"));
    modeModel->select();

    /* IOTA Completer */
    iotaCompleter = new QCompleter(Data::instance()->iotaIDList(), this);
    iotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    iotaCompleter->setFilterMode(Qt::MatchContains);
    iotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->iotaEdit->setCompleter(iotaCompleter);

    /* SOTA Completer */
    sotaCompleter = new QCompleter(Data::instance()->sotaIDList(), this);
    sotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    sotaCompleter->setFilterMode(Qt::MatchStartsWith);
    sotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->sotaEdit->setCompleter(nullptr);

    /* MyIOTA Completer */
    myIotaCompleter = new QCompleter(Data::instance()->iotaIDList(), this);
    myIotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    myIotaCompleter->setFilterMode(Qt::MatchContains);
    myIotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->myIOTAEdit->setCompleter(myIotaCompleter);

    /* MySOTA Completer */
    mySotaCompleter = new QCompleter(Data::instance()->sotaIDList(), this);
    mySotaCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    mySotaCompleter->setFilterMode(Qt::MatchStartsWith);
    mySotaCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    ui->mySOTAEdit->setCompleter(nullptr);

    DEFINE_CONTACT_FIELDS_ENUMS;

    /* Combo Mapping */
    /* do no use DEFINE_CONTACT_FIELDS_ENUMS for it because
     * DEFINE_CONTACT_FIELDS_ENUMS has a different ordering.
     * Ordering below is optimized for a new Contact Widget only
     */
    ui->qslPaperSentStatusBox->addItem(tr("No"), QVariant("N"));
    ui->qslPaperSentStatusBox->addItem(tr("Yes"), QVariant("Y"));
    ui->qslPaperSentStatusBox->addItem(tr("Requested"), QVariant("R"));
    ui->qslPaperSentStatusBox->addItem(tr("Queued"), QVariant("Q"));
    ui->qslPaperSentStatusBox->addItem(tr("Ignored"), QVariant("I"));

    QMapIterator<QString, QString> iter(qslRcvdEnum);

    while( iter.hasNext() )
    {
        iter.next();
        ui->qslPaperReceiveStatusBox->addItem(iter.value(), iter.key());
    }

    /* do no use DEFINE_CONTACT_FIELDS_ENUMS for it because
     * DEFINE_CONTACT_FIELDS_ENUMS has a different ordering.
     * Ordering below is optimized for a new Contact Widget only
     */
    ui->qslSentViaBox->addItem("", QVariant(""));
    ui->qslSentViaBox->addItem(tr("Bureau"), QVariant("B"));
    ui->qslSentViaBox->addItem(tr("Direct"), QVariant("D"));
    ui->qslSentViaBox->addItem(tr("Electronic"), QVariant("E"));

    /* Propagation */
    QStringList propagationModeList = Data::instance()->propagationModesList();
    propagationModeList.prepend("");
    QStringListModel* propagationModeModel = new QStringListModel(propagationModeList, this);
    ui->propagationModeEdit->setModel(propagationModeModel);

    /* Sat Modes & sat names */
    QSqlTableModel* satModel = new QSqlTableModel();
    satModel->setTable("sat_info");
    QCompleter *satCompleter = new QCompleter();
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
    SqlListModel* countryModel = new SqlListModel("SELECT id, name FROM dxcc_entities ORDER BY name;", "", this);
    while ( countryModel->canFetchMore() )
    {
        countryModel->fetchMore();
    }
    ui->countryCombo->setModel(countryModel);
    ui->countryCombo->setModelColumn(1);

    /* Assign Validators */
    ui->callsignEdit->setValidator(new QRegularExpressionValidator(Data::callsignRegEx(), this));
    ui->myCallsignEdit->setValidator(new QRegularExpressionValidator(Data::callsignRegEx(), this));
    ui->gridEdit->setValidator(new QRegularExpressionValidator(Gridsquare::gridRegEx(), this));
    ui->myGridEdit->setValidator(new QRegularExpressionValidator(Gridsquare::gridRegEx(), this));
    ui->vuccEdit->setValidator(new QRegularExpressionValidator(Gridsquare::gridVUCCRegEx(), this));
    ui->myVUCCEdit->setValidator(new QRegularExpressionValidator(Gridsquare::gridVUCCRegEx(), this));

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
    mapper->addMapping(ui->sigEdit, LogbookModel::COLUMN_SIG_INTL);
    mapper->addMapping(ui->sigInfoEdit, LogbookModel::COLUMN_SIG_INFO_INTL);
    mapper->addMapping(ui->dokEdit, LogbookModel::COLUMN_DARC_DOK);
    mapper->addMapping(ui->vuccEdit, LogbookModel::COLUMN_VUCC_GRIDS);
    mapper->addMapping(ui->countryCombo, LogbookModel::COLUMN_DXCC);
    mapper->addMapping(ui->emailEdit, LogbookModel::COLUMN_EMAIL);
    mapper->addMapping(ui->urlEdit, LogbookModel::COLUMN_WEB);
    mapper->addMapping(ui->propagationModeEdit, LogbookModel::COLUMN_PROP_MODE);
    mapper->addMapping(ui->satNameEdit, LogbookModel::COLUMN_SAT_NAME);
    mapper->addMapping(ui->satModeEdit,LogbookModel::COLUMN_SAT_MODE);

    /* My Station */
    mapper->addMapping(ui->myCallsignEdit, LogbookModel::COLUMN_STATION_CALLSIGN);
    mapper->addMapping(ui->myOperatorNameEdit, LogbookModel::COLUMN_OPERATOR);
    mapper->addMapping(ui->myQTHEdit, LogbookModel::COLUMN_MY_CITY_INTL);
    mapper->addMapping(ui->myGridEdit, LogbookModel::COLUMN_MY_GRIDSQUARE);
    mapper->addMapping(ui->mySOTAEdit, LogbookModel::COLUMN_MY_SOTA_REF);
    mapper->addMapping(ui->myIOTAEdit, LogbookModel::COLUMN_MY_IOTA);
    mapper->addMapping(ui->mySIGEdit, LogbookModel::COLUMN_MY_SIG);
    mapper->addMapping(ui->mySIGInfoEdit, LogbookModel::COLUMN_MY_SIG_INFO_INTL);
    mapper->addMapping(ui->myRigEdit, LogbookModel::COLUMN_MY_RIG_INTL);
    mapper->addMapping(ui->myAntEdit, LogbookModel::COLUMN_MY_ANTENNA_INTL);
    mapper->addMapping(ui->myVUCCEdit, LogbookModel::COLUMN_MY_VUCC_GRIDS);
    mapper->addMapping(ui->powerEdit, LogbookModel::COLUMN_TX_POWER);

    /* Notes */
    mapper->addMapping(ui->noteEdit, LogbookModel::COLUMN_NOTES_INTL);

    /* QSL */
    mapper->addMapping(ui->qslPaperSentStatusBox, LogbookModel::COLUMN_QSL_SENT);
    mapper->addMapping(ui->qslPaperReceiveStatusBox, LogbookModel::COLUMN_QSL_RCVD);
    mapper->addMapping(ui->qslEqslReceiveDateLabel, LogbookModel::COLUMN_EQSL_QSLRDATE);
    mapper->addMapping(ui->qslEqslSentDateLabel, LogbookModel::COLUMN_EQSL_QSLSDATE);
    mapper->addMapping(ui->qslLotwReceiveDateLabel, LogbookModel::COLUMN_LOTW_RCVD_DATE);
    mapper->addMapping(ui->qslLotwSentDateLabel, LogbookModel::COLUMN_LOTW_SENT_DATE);
    mapper->addMapping(ui->qslEqslReceiveStatusLabel, LogbookModel::COLUMN_EQSL_QSL_RCVD);
    mapper->addMapping(ui->qslEqslSentStatusLabel, LogbookModel::COLUMN_EQSL_QSL_SENT);
    mapper->addMapping(ui->qslLotwReceiveStatusLabel, LogbookModel::COLUMN_LOTW_RCVD);
    mapper->addMapping(ui->qslLotwSentStatusLabel, LogbookModel::COLUMN_LOTW_SENT);
    mapper->addMapping(ui->qslReceivedMsgEdit, LogbookModel::COLUMN_QSLMSG, "text");
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

    enableWidgetChangeHandlers();
}

void QSODetailDialog::accept()
{
    FCT_IDENTIFICATION;

    if (editButton->text() == SAVE_BUTTON_TEXT )
    {
        QSODetailDialog::SubmitError error = submitAllChanges();
        if ( error == QSODetailDialog::SubmitMapperError
             || error == QSODetailDialog::SubmitModelError )
        {
            QMessageBox::critical(this, tr("QLog Error"), tr("Cannot save all changes - internal error"));
        }
    }

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

    if ( editButton->text() == SAVE_BUTTON_TEXT )
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
        editButton->setText(EDIT_BUTTON_TEXT);
        ui->timeLockButton->setChecked(true);
        ui->freqLockButton->setChecked(true);
    }
    else
    {
        editButton->setText(SAVE_BUTTON_TEXT);
        setReadOnlyMode(false);
        timeLockDiff = ui->dateTimeOnEdit->dateTime().msecsTo(ui->dateTimeOffEdit->dateTime());
        freqLockDiff = ui->freqTXEdit->value() -  ui->freqRXEdit->value();
    }
}

void QSODetailDialog::resetButtonPressed()
{
    FCT_IDENTIFICATION;

    editButton->setText(EDIT_BUTTON_TEXT);

    blockMappedWidgetSignals(true);
    mapper->revert();
    blockMappedWidgetSignals(false);

    resetButton->setEnabled(false);
    ui->timeLockButton->setChecked(true);
    ui->freqLockButton->setChecked(true);
    setReadOnlyMode(true);
    doValidation();
}

void QSODetailDialog::lookupButtonPressed()
{
    FCT_IDENTIFICATION;

    lookupButton->setEnabled(false);
    resetButton->setEnabled(false);
    editButton->setEnabled(false);
    lookupButtonWaitingStyle(true);
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
        else if ( QLabel *label = qobject_cast<QLabel*>(widget) )
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
        }
    }

    if ( !inReadOnly )
    {
        if ( ui->propagationModeEdit->currentText() != Data::instance()->propagationModeIDToText("SAT") )
        {
            /* Do not enable sat fields when SAT prop is not selected */
            ui->satModeEdit->setCurrentIndex(-1);
            ui->satNameEdit->clear();
            ui->satModeEdit->setEnabled(false);
            ui->satNameEdit->setEnabled(false);
        }
        this->setWindowTitle(tr("Edit QSO"));
    }
    else
    {
        this->setWindowTitle(tr("QSO Detail"));
    }

    ui->timeLockButton->setEnabled(!inReadOnly);
    ui->freqLockButton->setEnabled(!inReadOnly);
    resetButton->setEnabled(!inReadOnly);
    lookupButton->setEnabled(!inReadOnly);

    if ( ui->qslEqslReceiveStatusLabel->property("originvalue").toString() != "Y" )
    {
        ui->qslEqslPicButton->setEnabled(false);
    }
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

    EQSL *eQSL = new EQSL(dialog);

    connect(eQSL, &EQSL::QSLImageFound, this, [dialog, eQSL](QString imgFile)
    {
        dialog->done(0);
        QDesktopServices::openUrl(imgFile);
        eQSL->deleteLater();
    });

    connect(eQSL, &EQSL::QSLImageError, this, [this, dialog, eQSL](QString error)
    {
        dialog->done(1);
        QMessageBox::critical(this, tr("QLog Error"), tr("eQSL Download Image failed: ") + error);
        eQSL->deleteLater();
    });

    connect(dialog, &QProgressDialog::canceled, this, [eQSL]()
    {
        qCDebug(runtime)<< "Operation canceled";
        eQSL->abortRequest();
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

void QSODetailDialog::callsignChanged(QString)
{
    FCT_IDENTIFICATION;

    /* In general, we assume that an operator will modify just suffix therefore QLog will not update ITU, COUNTRY, CQZ, DXCC Countitne */
    /* If an operator will need to modify ITU, so do it manually now */

    //nothing to do
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

    allValid &= highlightInvalid(ui->callsignLabel,
                                 ui->callsignEdit->text().isEmpty(),
                                 tr("DX Callsign must not be empty"));

    allValid &= highlightInvalid(ui->callsignLabel,
                                 !ui->callsignEdit->text().isEmpty() && !ui->callsignEdit->hasAcceptableInput(),
                                 tr("DX callsign has an incorrect format"));

    allValid &= highlightInvalid(ui->freqTXLabel,
                                 ui->freqTXEdit->value() == 0.0,
                                 tr("TX Frequency must not be 0"));

    allValid &= highlightInvalid(ui->gridLabel,
                                 !ui->gridEdit->text().isEmpty() && !ui->gridEdit->hasAcceptableInput(),
                                 tr("DX Grid has an incorrect format"));

    DxccEntity dxccEntity = Data::instance()->lookupDxcc(ui->callsignEdit->text());

    if ( dxccEntity.dxcc )
    {
        allValid &= highlightInvalid(ui->countryLabel,
                                     ui->countryCombo->currentText() != dxccEntity.country,
                                     tr("DXCC Country based on callsign is different from entered - expecting") + "<b> " + dxccEntity.country + "</b>");

        allValid &= highlightInvalid(ui->contLabel,
                                     ui->contEdit->currentText() != dxccEntity.cont,
                                     tr("DXCC Continent based on callsign is different from entered") + "<b> " + dxccEntity.cont + "</b>");

        allValid &= highlightInvalid(ui->ituLabel,
                                     ui->ituEdit->text() != QString::number(dxccEntity.ituz),
                                     tr("DXCC ITU based on callsign is different from entered") + "<b> " + QString::number(dxccEntity.ituz) + "</b>");

        allValid &= highlightInvalid(ui->cqLabel,
                                     ui->cqEdit->text() != QString::number(dxccEntity.cqz),
                                     tr("DXCC CQZ based on callsign is different from entered") + "<b> " + QString::number(dxccEntity.cqz) + "</b>");
    }

    allValid &= highlightInvalid(ui->vuccLabel,
                                 !ui->vuccEdit->text().isEmpty() && !ui->vuccEdit->hasAcceptableInput(),
                                 tr("VUCC has an incorrect format"));

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
                                 tr("Own VUCC Grid has an incorrect format"));

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

    QSettings settings;

    isMainPageLoaded = true;
    main_page->runJavaScript(postponedScripts);

    bool darkmode = settings.value("darkmode", false).toBool();

    if ( darkmode )
    {
        QString themeJavaScript = "map.getPanes().tilePane.style.webkitFilter=\"brightness(0.6) invert(1) contrast(3) hue-rotate(200deg) saturate(0.3) brightness(0.9)\";";
        main_page->runJavaScript(themeJavaScript);
    }
}

void QSODetailDialog::myGridChanged(QString newGrid)
{
    FCT_IDENTIFICATION;

    drawMyQTHOnMap(ui->myCallsignEdit->text(), Gridsquare(newGrid));

    return;
}

void QSODetailDialog::DXGridChanged(QString newGrid)
{
    FCT_IDENTIFICATION;

    drawDXOnMap(ui->callsignEdit->text(), Gridsquare(newGrid));

    return;
}

void QSODetailDialog::callsignFound(const QMap<QString, QString> &data)
{
    FCT_IDENTIFICATION;

    lookupButton->setEnabled(true);
    resetButton->setEnabled(true);
    editButton->setEnabled(true);
    lookupButtonWaitingStyle(false);

    /* blank or not fully filled then update it */
    if ( ui->nameEdit->text().isEmpty() )
    {
        QString name = data.value("name");

        if ( name.isEmpty() )
        {
            name = data.value("fname");
        }

        if ( ui->nameEdit->text().isEmpty() )
        {
            ui->nameEdit->setText(name);
        }
    }

    if ( ui->gridEdit->text().isEmpty()
         || data.value("gridsquare").contains(ui->gridEdit->text()) )
    {
        ui->gridEdit->setText(data.value("gridsquare"));
    }

    if ( ui->qthEdit->text().isEmpty() )
    {
        ui->qthEdit->setText(data.value("qth"));
    }

    if ( ui->dokEdit->text().isEmpty() )
    {
        ui->dokEdit->setText(data.value("dok"));
    }

    if ( ui->iotaEdit->text().isEmpty() )
    {
        ui->iotaEdit->setText(data.value("iota"));
    }

    if ( ui->emailEdit->text().isEmpty() )
    {
        ui->emailEdit->setText(data.value("email"));
    }

    if ( ui->countyEdit->text().isEmpty() )
    {
        ui->countyEdit->setText(data.value("county"));
    }

    if ( ui->qslViaEdit->text().isEmpty() )
    {
        ui->qslViaEdit->setText(data.value("qsl_via"));
    }

    if ( ui->urlEdit->text().isEmpty() )
    {
        ui->urlEdit->setText(data.value("url"));
    }

    if ( ui->stateEdit->text().isEmpty() )
    {
        ui->stateEdit->setText(data.value("us_state"));
    }
}

void QSODetailDialog::callsignNotFound(QString)
{
    FCT_IDENTIFICATION;

    lookupButton->setEnabled(true);
    resetButton->setEnabled(true);
    editButton->setEnabled(true);
    lookupButtonWaitingStyle(false);
}

void QSODetailDialog::handleBeforeUpdate(int, QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    emit contactUpdated(record);
}

void QSODetailDialog::sotaChanged(QString newSOTA)
{
    FCT_IDENTIFICATION;

    if ( newSOTA.length() >= 3 )
    {
        ui->sotaEdit->setCompleter(sotaCompleter);
    }
    else
    {
        ui->sotaEdit->setCompleter(nullptr);
    }
}

void QSODetailDialog::mySotaChanged(QString newSOTA)
{
    FCT_IDENTIFICATION;

    if ( newSOTA.length() >= 3 )
    {
        ui->mySOTAEdit->setCompleter(sotaCompleter);
    }
    else
    {
        ui->mySOTAEdit->setCompleter(nullptr);
    }
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
    double distance = 0;

    if ( dxGrid.distanceTo(Gridsquare(ui->myGridEdit->text()), distance) )
    {
        popupString.append(QString("</br> %1 km").arg(QString::number(distance, 'f', 0)));
    }

    double lat = dxGrid.getLatitude();
    double lon = dxGrid.getLongitude();
    stationString.append(QString("[\"%1\", %2, %3]").arg(popupString).arg(lat).arg(lon));

    QString javaScript = QString("grids_confirmed = [];"
                                 "grids_worked = [];"
                                 "if ( typeof QSOGroup !== 'undefined' ) { map.removeLayer(QSOGroup)};"
                                 " var QSOGroup = L.layerGroup().addTo(map); "
                                 " locations = [ %1 ]; "
                                 "   QSOGroup.addLayer(L.marker([locations[0][1], locations[0][2]],{icon: yellowIcon})"
                                 "   .bindPopup(locations[0][0]));"
                                 "maidenheadConfWorked.redraw();"
                                 "map.flyTo([locations[0][1], locations[0][2]],6);").arg(stationString);

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
    stationString.append(QString("[\"%1\", %2, %3]").arg(label).arg(lat).arg(lon));

    QString javaScript = QString("grids_confirmed = [];"
                                 "grids_worked = [];"
                                 "if ( typeof myLocGroup !== 'undefined' ) { map.removeLayer(myLocGroup)};"
                                 " var myLocGroup = L.layerGroup().addTo(map); "
                                 " mylocations = [ %1 ]; "
                                 "   myLocGroup.addLayer(L.marker([mylocations[0][1], mylocations[0][2]],{icon: homeIcon}) "
                                 "   .bindPopup(mylocations[0][0]));"
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

void QSOEditMapperDelegate::setEditorData(QWidget *editor,
                                          const QModelIndex &index) const
{
    if ( editor->objectName() == "qslSentBox"
         || editor->objectName() == "qslSentViaBox"
         || editor->objectName() == "qslPaperSentStatusBox"
         || editor->objectName() == "qslPaperReceiveStatusBox"
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
              || editor->objectName() == "qslEqslSentDateLabel"
              || editor->objectName() == "qslLotwReceiveDateLabel"
              || editor->objectName() == "qslLotwSentDateLabel"
            )
    {
        QLabel* label = qobject_cast<QLabel*>(editor);

        if ( label )
        {
            QLocale locale;
            if ( !index.data().toString().isEmpty() )
            {
                label->setText(index.data().toDate().toString(locale.dateFormat(QLocale::ShortFormat)));
            }
        }
        return;
    }
    else if ( editor->objectName() == "qslEqslReceiveStatusLabel"
              || editor->objectName() == "qslEqslSentStatusLabel"
              || editor->objectName() == "qslLotwReceiveStatusLabel"
              || editor->objectName() == "qslLotwSentStatusLabel"
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
    else if ( editor->objectName() == "countryCombo" )
    {
        QComboBox* combo = qobject_cast<QComboBox*>(editor);

        if ( combo )
        {
            QModelIndexList countryIndex = combo->model()->match(combo->model()->index(0,0),
                                                                 Qt::DisplayRole, index.data());
            if ( countryIndex.size() >= 1 )
            {
                combo->setCurrentIndex(countryIndex.at(0).row());
            }

        }
        return;
    }
    else if ( editor->objectName() == "noteEdit" )
    {
        QTextEdit *textEdit = static_cast<QTextEdit*>(editor);

        if ( textEdit )
        {
            QString value = index.data().toString();
            textEdit->setPlainText(value);
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
              || editor->objectName() == "qslPaperSentDateEdit")
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
              || editor->objectName() == "qslPaperSentDateEdit" )
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
    else if ( editor->objectName() == "countryCombo" )
    {
        QComboBox* combo = static_cast<QComboBox*>(editor);

        if ( combo )
        {
            int row = combo->currentIndex();
            QModelIndex idx = combo->model()->index(row,0);
            QVariant data = combo->model()->data(idx);

            model->setData(index, data);
            model->setData(model->index(index.row(), LogbookModel::COLUMN_COUNTRY_INTL),combo->currentText());
        }
        return;
    }
    else if (    editor->objectName() == "qslEqslReceiveDateLabel"
              || editor->objectName() == "qslEqslSentDateLabel"
              || editor->objectName() == "qslEqslReceiveStatusLabel"
              || editor->objectName() == "qslEqslSentStatusLabel"
              || editor->objectName() == "qslLotwReceiveDateLabel"
              || editor->objectName() == "qslLotwSentDateLabel"
              || editor->objectName() == "qslLotwReceiveStatusLabel"
              || editor->objectName() == "qslLotwSentStatusLabel"
              || editor->objectName() == "qslReceivedMsgEdit"
            )
    {
        /* do not save */
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
    return QSqlTableModel::data(index, role);
}

bool QSODetailDialog::LogbookModelPrivate::setData(const QModelIndex &index, const QVariant &value, int role)
{
    bool main_update_result = true;
    bool depend_update_result = true;

    if ( role == Qt::EditRole )
    {
        switch ( index.column() )
        {

        case COLUMN_FREQUENCY:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_BAND), QVariant(Data::freqToBand(value.toDouble())), role );
            break;
        }

        case COLUMN_FREQ_RX:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_BAND_RX), QVariant(Data::freqToBand(value.toDouble())), role );
            break;
        }

        case COLUMN_GRID:
        {
            if ( ! value.toString().isEmpty() )
            {
                Gridsquare newgrid(value.toString());

                if ( newgrid.isValid() )
                {
                    Gridsquare mygrid(QSqlTableModel::data(this->index(index.row(), COLUMN_MY_GRIDSQUARE), Qt::DisplayRole).toString());
                    double distance;

                    if ( mygrid.distanceTo(newgrid, distance) )
                    {
                        depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(distance),role);
                    }
                    else
                    {
                        depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(),role);
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
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(),role);
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
                    Gridsquare dxgrid(QSqlTableModel::data(this->index(index.row(), COLUMN_GRID), Qt::DisplayRole).toString());
                    double distance;

                    if ( mynewGrid.distanceTo(dxgrid, distance) )
                    {
                        depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(distance),role);
                    }
                    else
                    {
                        depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(),role);
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
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(),role);
            }
            break;
        }

        case COLUMN_ADDRESS_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_ADDRESS), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_COMMENT_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_COMMENT), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_COUNTRY_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_COUNTRY), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_ANTENNA_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_ANTENNA), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_CITY_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_CITY), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_COUNTRY_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_COUNTRY), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_NAME_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_NAME), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_POSTAL_CODE_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_POSTAL_CODE), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_RIG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_RIG), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_SIG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_SIG), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_SIG_INFO_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_SIG_INFO), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_STREET_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_STREET), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_NAME_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_NAME), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_NOTES_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_NOTES), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_QSLMSG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_QSLMSG), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_QTH_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_QTH), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_RIG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_RIG), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_SIG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_SIG), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_SIG_INFO_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_SIG_INFO), Data::removeAccents(value.toString()),role);
            break;
        }

       }
       updateExternalServicesUploadStatus(index, role, depend_update_result);

       if ( depend_update_result )
       {
           switch ( index.column() )
           {
           case COLUMN_FREQUENCY:
           case COLUMN_FREQ_RX:
               if ( value.toDouble() == 0.0 )
               {
                   /* store NULL when 0.0MHz */
                   main_update_result = QSqlTableModel::setData(index, QVariant(), role);
               }
               else
               {
                   main_update_result = QSqlTableModel::setData(index, value, role);
               }
               break;

           case COLUMN_SOTA_REF:
           case COLUMN_MY_SOTA_REF:
           case COLUMN_IOTA:
           case COLUMN_MY_IOTA:
           case COLUMN_MY_GRIDSQUARE:
           case COLUMN_CALL:
           case COLUMN_GRID:
           case COLUMN_VUCC_GRIDS:
           case COLUMN_MY_VUCC_GRIDS:
               main_update_result = QSqlTableModel::setData(index, value.toString().toUpper(), role);
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
               main_update_result = QSqlTableModel::setData(index, value.toString(), role);
               break;

           default:
               main_update_result = QSqlTableModel::setData(index, Data::removeAccents(value.toString()), role);
           }
       }
    }

    return main_update_result && depend_update_result;
}

const QString QSODetailDialog::SAVE_BUTTON_TEXT = tr("&Save");
const QString QSODetailDialog::EDIT_BUTTON_TEXT = tr("&Edit");
