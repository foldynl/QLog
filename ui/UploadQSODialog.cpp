#include <qpushbutton.h>
#include <QProgressDialog>
#include <QMessageBox>
#include "UploadQSODialog.h"
#include "ui_UploadQSODialog.h"
#include "core/debug.h"
#include "models/SqlListModel.h"
#include "core/LogParam.h"
#include "data/StationProfile.h"
#include "service/clublog/ClubLog.h"
#include "service/eqsl/Eqsl.h"
#include "service/lotw/Lotw.h"
#include "service/qrzcom/QRZ.h"
#include "service/hrdlog/HRDLog.h"
#include "service/cloudlog/Cloudlog.h"

MODULE_IDENTIFICATION("qlog.ui.uploadqsodialog");

UploadQSODialog::UploadQSODialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UploadQSODialog),
    detailQSOsModel(new QStandardItemModel(this)),
    executeQueryEnabled(false)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    onlineServices.insert(LOTWID, UploadTask(LOTWID,
                                       tr("LoTW"),
                                       new LotwUploader(this),
                                       "lotw_qsl_sent",
                                       "lotw_qslsdate",
                                       ui->lotwCheckbox,
                                       ui->lotwNumberLabel,
                                       !LotwBase::getUsername().isEmpty()));

    onlineServices.insert(EQSLID, UploadTask(EQSLID,
                                       tr("eQSL"),
                                       new EQSLUploader(this),
                                       "eqsl_qsl_sent",
                                       "eqsl_qslsdate",
                                       ui->eqslCheckbox,
                                       ui->eqslNumberLabel,
                                       !EQSLBase::getUsername().isEmpty()));

    onlineServices.insert(CLUBLOGID, UploadTask(CLUBLOGID,
                                          tr("Clublog"),
                                          new ClubLogUploader(this),
                                          "clublog_qso_upload_status",
                                          "clublog_qso_upload_date",
                                          ui->clublogCheckbox,
                                          ui->clublogNumberLabel,
                                          !ClubLogBase::getEmail().isEmpty()));

    onlineServices.insert(HRDLOGID, UploadTask(HRDLOGID,
                                         tr("HRDLog"),
                                         new HRDLogUploader(this),
                                         "hrdlog_qso_upload_status",
                                         "hrdlog_qso_upload_date",
                                         ui->hrdlogCheckbox,
                                         ui->hrdlogNumberLabel,
                                         !HRDLogBase::getRegisteredCallsign().isEmpty()));

    onlineServices.insert(QRZCOMID, UploadTask(QRZCOMID,
                                         tr("QRZ.com"),
                                         new QRZUploader(this),
                                         "qrzcom_qso_upload_status",
                                         "qrzcom_qso_upload_date",
                                         ui->qrzCheckbox,
                                         ui->qrzNumberLabel,
                                        !QRZBase::getLogbookAPIKey().isEmpty()));

    onlineServices.insert(WAVELOGID, UploadTask(WAVELOGID,
                                         tr("Wavelog"),
                                         new CloudlogUploader(this),
                                         "contacts_autovalue.wavelog_qso_upload_status",
                                         "contacts_autovalue.wavelog_qso_upload_date",
                                         ui->wavelogCheckbox,
                                         ui->wavelogNumberLabel,
                                        !CloudlogBase::getLogbookAPIKey().isEmpty()));

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&Upload"));

    ui->myCallsignCombo->blockSignals(true);
    ui->myGridCombo->blockSignals(true);

    // based on LoTW source code and ADIF spec - priority STATION_CALLSIGN, OPERATOR
    ui->myCallsignCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(COALESCE(NULLIF(TRIM(station_callsign), ''), TRIM(operator))) "
                                                   "FROM contacts "
                                                   "WHERE station_callsign IS NOT NULL OR operator IS NOT NULL "
                                                   "ORDER BY station_callsign", "", ui->myCallsignCombo));
    ui->myStationProfileCombo->setModel(new SqlListModel("SELECT DISTINCT profile_name FROM station_profiles ORDER BY profile_name", "", ui->myStationProfileCombo));

    setEQSLSettingVisible(false);
    setClublogSettingVisible(false);
    setQSODetailVisible(false);
    setWavelogSettingVisible(false);
    loadDialogState();
    getWavelogStationID();
    ui->myCallsignCombo->blockSignals(false);
    ui->myGridCombo->blockSignals(false);
    executeQueryEnabled = true;
    executeQuery();
}

UploadQSODialog::~UploadQSODialog()
{
    FCT_IDENTIFICATION;

    allSelectedQSOs.clear();
    detailQSOsModel->deleteLater();
    delete ui;
}

void UploadQSODialog::showQSODetails()
{
    FCT_IDENTIFICATION;

    setQSODetailVisible(!ui->detailQSOView->isVisible());
}

void UploadQSODialog::setEQSLSettingVisible(bool visible)
{
    FCT_IDENTIFICATION;

    ui->eqslGroup->setVisible(visible);
    adjustSize();
}

void UploadQSODialog::setClublogSettingVisible(bool visible)
{
    FCT_IDENTIFICATION;

    ui->clublogGroup->setVisible(visible);
    adjustSize();
}

void UploadQSODialog::setWavelogSettingVisible(bool visible)
{
    FCT_IDENTIFICATION;

    ui->wavelogGroup->setVisible(visible);
    adjustSize();
}

void UploadQSODialog::setQSODetailVisible(bool visible)
{
    FCT_IDENTIFICATION;

    ui->detailQSOView->setVisible(visible);
    ui->line->setVisible(visible);
    ui->showQSOButton->setText(visible ? tr("Hide QSOs...") : tr("Show QSOs..."));
    adjustSize();
}

void UploadQSODialog::loadDialogState()
{
    FCT_IDENTIFICATION;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();

    ui->clublogCheckbox->setChecked(ui->clublogCheckbox->isEnabled() && LogParam::getUploadServiceState("clublog"));
    ui->eqslCheckbox->setChecked(ui->eqslCheckbox->isEnabled() && LogParam::getUploadServiceState("eqsl"));
    ui->lotwCheckbox->setChecked(ui->lotwCheckbox->isEnabled() && LogParam::getUploadServiceState("lotw"));
    ui->hrdlogCheckbox->setChecked(ui->hrdlogCheckbox->isEnabled() && LogParam::getUploadServiceState("hrdlog"));
    ui->qrzCheckbox->setChecked(ui->qrzCheckbox->isEnabled() && LogParam::getUploadServiceState("qrzcom"));
    ui->wavelogCheckbox->setChecked(ui->wavelogCheckbox->isEnabled() && LogParam::getUploadServiceState("wavelog"));

    int index = ui->myCallsignCombo->findText(profile.callsign);

    if ( index >= 0 )
        ui->myCallsignCombo->setCurrentIndex(index);
    else
        ui->myCallsignCombo->setCurrentText(LogParam::getUploadQSOLastCall());

    handleCallsignChange(ui->myCallsignCombo->currentText());

    ui->myGridCombo->setCurrentIndex(0); // ANY value

    index = ui->myStationProfileCombo->findText(profile.profileName);
    ui->myStationProfileCombo->setCurrentIndex( ( index >= 0 ) ? index : -1);

    ui->eqslQSLComment->setChecked(LogParam::getUploadeqslQSLComment());
    ui->eqslQSLMessage->setChecked(LogParam::getUploadeqslQSLMessage());
    ui->eqslQTHProfileEdit->setText(LogParam::getUploadeqslQTHProfile());
    ui->myStationProfileCheckbox->setChecked(LogParam::getUploadQSOFilterType() == 1);
    ui->wavelogStationIDSpin->setValue(LogParam::getCloudlogStationID());
}

void UploadQSODialog::saveDialogState()
{
    FCT_IDENTIFICATION;

    LogParam::setUploadServiceState("clublog", ui->clublogCheckbox->isChecked());
    LogParam::setUploadServiceState("eqsl", ui->eqslCheckbox->isChecked());
    LogParam::setUploadServiceState("lotw", ui->lotwCheckbox->isChecked());
    LogParam::setUploadServiceState("hrdlog", ui->hrdlogCheckbox->isChecked());
    LogParam::setUploadServiceState("qrzcom", ui->qrzCheckbox->isChecked());
    LogParam::setUploadServiceState("wavelog", ui->wavelogCheckbox->isChecked());
    LogParam::setUploadQSOLastCall(ui->myCallsignCombo->currentText());
    LogParam::setUploadeqslQSLComment(ui->eqslQSLComment->isChecked());
    LogParam::setUploadeqslQSLMessage(ui->eqslQSLMessage->isChecked());
    LogParam::setUploadeqslQTHProfile(ui->eqslQTHProfileEdit->text());
    LogParam::setUploadQSOFilterType(ui->myCallsignCheckbox->isChecked() ? 0 : 1);
    LogParam::setCloudlogStationID(ui->wavelogStationIDSpin->value());
}

void UploadQSODialog::processNextUploader()
{
    FCT_IDENTIFICATION;

    if ( uploadTaskQueue.isEmpty() )
    {
        uploadFinished();
        return;
    }

    currentTask = uploadTaskQueue.takeFirst();
    GenericQSOUploader *uploader = currentTask.getUploader();

    if ( !uploader )
    {
        qWarning() << "Uploader is null - skipping";
        uploadFinished();
        return;
    }

    qCDebug(runtime) << "Uploading to" << currentTask.getServiceName();

    QProgressDialog* dialog = new QProgressDialog(tr("Uploading to %1").arg(currentTask.getServiceName()),
                                                  tr("Cancel"), 0, 0, this);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setValue(0);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->show();

    connect(uploader, &GenericQSOUploader::uploadedQSO, this, [this, dialog, uploader](qulonglong qsoID)
    {
        // this part updates one QSO - it is use in case of services when a batch upload
        // is not supported
        qCDebug(runtime) << "signal uploadedQSO";

        QString contactIDField("id");
        QString tableName("contacts");
        QString statusField = currentTask.getDBUploadStatusFieldName();
        QString dateField = currentTask.getDBUploadDateFieldName();

        if ( currentTask.getServiceID() == WAVELOGID )
        {
             statusField.remove("contacts_autovalue."); // sqlite does not support aliases in the Update statement.
             dateField.remove("contacts_autovalue.");
             contactIDField = "contactid";
             tableName = "contacts_autovalue";
        }

        const QString &statement = QString("UPDATE %1 "
                                          "SET %2='Y', %3 = strftime('%Y-%m-%d',DATETIME('now', 'utc')) "
                                          "WHERE %4 = :qsoID").arg(tableName, statusField, dateField, contactIDField);

        qCDebug(runtime) << statement;

        // update in-DB record
        QSqlQuery updateQuery;
        updateQuery.prepare(statement);
        updateQuery.bindValue(":qsoID", qsoID);

        if ( ! updateQuery.exec() )
        {
            const QString &taskName = currentTask.getServiceName();
            qWarning() << "Cannot Update" << taskName
                       << "Upload status for QSO number " << qsoID
                       << updateQuery.lastError().text();
            uploader->abortRequest();
            dialog->done(QDialog::Accepted);
            QMessageBox::warning(this, tr("QLog Warning - %1").arg(taskName), tr("Cannot update QSO Status"));
            processNextUploader();
        }

        // update in-memory record
        auto recordRef = allSelectedQSOs.value(qsoID);
        if ( !recordRef.isNull() )
        {
            recordRef->setValue(statusField, "Y");

            // I don't know but it seems that RETURNING clause does not work under QT.
            // So I'll set it to the current QT date.
            recordRef->setValue(dateField, QDateTime::currentDateTimeUtc().date().toString(Qt::ISODate));
        }

        dialog->setValue(dialog->value() + 1);
    });

    connect(uploader, &GenericQSOUploader::uploadFinished, this, [dialog, this]()
    {
        qCDebug(runtime) << "signal uploadFinished";

        // this part updates all uploaded QSOs - it is use in case of services when a batch upload
        // is supported

        dialog->done(QDialog::Accepted);
        if ( currentTask.getServiceID() != HRDLOGID
             && currentTask.getServiceID() != QRZCOMID
             && currentTask.getServiceID() != WAVELOGID )
        {

            const QString statusField = currentTask.getDBUploadStatusFieldName();
            const QString dateField = currentTask.getDBUploadDateFieldName();
            const QList<qulonglong> idsToUpdate = currentTask.getQSOIDs();

            QStringList idStrings;

            for ( qulonglong id : idsToUpdate )
                idStrings.append(QString::number(id));

            QString statement = QString("UPDATE contacts "
                                        "SET %1='Y', %2 = strftime('%Y-%m-%d',DATETIME('now', 'utc')) "
                                        "WHERE id IN (%3) ").arg(statusField, dateField, idStrings.join(","));

            QSqlQuery updateQuery;
            updateQuery.prepare(statement);

            if ( !updateQuery.exec() )
                qWarning() << "Cannot update" << currentTask.getServiceName()
                           << "Upload status in DB" << updateQuery.lastError().text();
            else
                currentTask.updateAllDBFieldValue(statusField, "Y",
                                                  dateField, QDateTime::currentDateTimeUtc().date().toString(Qt::ISODate));
        }
        processNextUploader();
    });

    connect(uploader, &GenericQSOUploader::uploadError, this, [this, dialog](const QString &msg)
    {
        qWarning() <<  currentTask.getServiceName() << "Upload Error: " << msg;
        dialog->done(QDialog::Accepted);
        QMessageBox::warning(this, tr("QLog Warning - %1").arg(currentTask.getServiceName()),
                             tr("Cannot upload the QSO(s): ") + msg);
        processNextUploader();
    });

    connect(dialog, &QProgressDialog::canceled, this, [this, uploader]()
    {
        qCDebug(runtime)<< "Operation canceled";
        uploader->abortRequest();
        uploadFinished();
    });

    QVariantMap uploadConfig;

    // Special params for selected services
    switch ( currentTask.getServiceID() )
    {
    case EQSLID:
        uploadConfig = EQSLUploader::generateUploadConfigMap(ui->eqslQTHProfileEdit->text(),
                                                             ui->eqslQSLNone->isChecked(),
                                                             ui->eqslQSLComment->isChecked());
        break;
    case CLUBLOGID:
        uploadConfig = ClubLogUploader::generateUploadConfigMap(ui->myCallsignCombo->currentText(),
                                                                ui->clublogClearCheckbox->isChecked());
        break;
    case WAVELOGID:
        uploadConfig = CloudlogUploader::generateUploadConfigMap(ui->wavelogStationIDSpin->value());
        break;
    default:
        break;
    }

    const QList<QSqlRecord> &list = currentTask.getQSOList();

    // set progress bar range for services that do not support the batch upload
    if ( currentTask.getServiceID() == HRDLOGID
         || currentTask.getServiceID() == QRZCOMID
         || currentTask.getServiceID() == WAVELOGID )
        dialog->setRange(0, list.size());

    uploader->uploadQSOList(list, uploadConfig);
}

void UploadQSODialog::startUploadQueue()
{
    FCT_IDENTIFICATION;

    saveDialogState();

    for ( auto it = onlineServices.begin(); it != onlineServices.end(); ++it )
        if ( !it.value().isQSOListEmpty() )
            uploadTaskQueue.append(it.value());

    if ( uploadTaskQueue.isEmpty() )
    {
         QMessageBox::information(this, tr("QLog Information"), tr("No QSO found to upload."));
         return;
    }

    processNextUploader();
}

void UploadQSODialog::uploadFinished()
{
    FCT_IDENTIFICATION;

    QMessageBox::information(this, tr("QLog Information"), tr("QSO(s) were uploaded to the selected services"));
    accept();
}

void UploadQSODialog::updateQSONumbers()
{
    FCT_IDENTIFICATION;

    for ( auto it = onlineServices.begin(); it != onlineServices.end(); ++it )
        it->updateQSONumberLabel();
}

void UploadQSODialog::getWavelogStationID()
{
    FCT_IDENTIFICATION;

    CloudlogUploader *ptr = qobject_cast<CloudlogUploader*>(onlineServices.value(WAVELOGID).getUploader());

    if ( ptr )
    {
        connect(ptr, &CloudlogUploader::stationIDsUpdated, this, [ptr, this]()
        {
            availableWavelogStationIDs = ptr->getAvailableStationIDs();
            updateWavelogStationLabel();
        });

        ptr->sendStationInfoReq();
    }
}

void UploadQSODialog::executeQuery()
{
    FCT_IDENTIFICATION;

    if ( !executeQueryEnabled )
    {
        qCDebug(runtime) << "query disabled";
        return;
    }

    allSelectedQSOs.clear();
    detailQSOsModel->clear();
    detailQSOsModel->setHorizontalHeaderLabels({tr("Time"), tr("Callsign"), tr("Mode"), tr("Upload to")});

    QStringList qslUploadStatuses = {"'M'", "'R'", "'Q'"}; // joined "Upload status" and "sent status"
    if ( ui->includeStatusNoCheckbox->isChecked() ) qslUploadStatuses << "'N'";
    if ( ui->includeIgnoreCheckbox->isChecked() ) qslUploadStatuses << "'I'";

    const QString whereTemplate("((UPPER(%1) in (%2) OR %1 IS NULL) %3)");
    const QString statusColumnTemplate("CASE WHEN (%1) THEN 1 ELSE 0 END AS status_%2");
    QStringList serviceSelectConditions;
    QStringList serviceStatusColumns;

    // prepare all params for building WHERE Clause
    for ( auto it = onlineServices.begin(); it != onlineServices.end(); ++it )
    {
        UploadTask &task = it.value();
        task.clearEnqueuedQSOs();

        if ( !task.isChecked() ) continue;

        QStringList uploadStatuses(qslUploadStatuses);

        if ( (    it.key() == CLUBLOGID && ui->clublogClearCheckbox->isChecked() )
             || ( it.key() == WAVELOGID  && ui->wavelogReuploadCheckbox->isChecked() ))
        {
            //reupload all QSOs (except N)
            uploadStatuses << "'Y'";
        }


        QString addlCondition = ( it.key() == LOTWID && ui->lotwCheckbox->isChecked() )
                                ? "AND (upper(prop_mode) NOT IN ('INTERNET', 'RPT', 'ECH', 'IRL') OR prop_mode IS NULL)"
                                : "";

        const QString serviceSelectCondition = whereTemplate.arg(task.getDBUploadStatusFieldName(),
                                                                 uploadStatuses.join(","),
                                                                 addlCondition);
        serviceSelectConditions << serviceSelectCondition;
        serviceStatusColumns << statusColumnTemplate.arg(serviceSelectCondition,
                                                         QString::number(task.getServiceID()));
    }

    const StationProfile &selectedStationProfile = StationProfilesManager::instance()->getProfile(ui->myStationProfileCombo->currentText());

    if ( ui->myStationProfileCheckbox->isChecked() )
    {
        QString toolTip = tr("The values below will be used when an input record does not contain the ADIF values") + "<br/>"
                             + selectedStationProfile.toHTMLString();
        ui->myStationProfileCombo->setToolTip(toolTip);
    }

    if ( serviceSelectConditions.isEmpty() )
    {
        updateQSONumbers();
        qCDebug(runtime) << "No service checked";
        ui->detailQSOView->setModel(nullptr);
        return;
    }

    QString selectStatement;
    QStringList whereParts;

    whereParts << "(" + serviceSelectConditions.join(" OR ") + ")";

    // Prepare Upload QSO Query
    QSqlQuery uploadQSOQuery;

    if ( ui->myCallsignCheckbox->isChecked() )
    {
        whereParts << QLatin1String("COALESCE(NULLIF(TRIM(station_callsign), ''), TRIM(operator)) = :station_callsign");

        if ( !ui->myGridCombo->currentText().isEmpty() && ui->myGridCombo->currentIndex() > 0 )
            whereParts << QLatin1String("my_gridsquare = :my_gridsquare");

        selectStatement = QString("SELECT *, %1 FROM contacts INNER JOIN contacts_autovalue ON contacts.id = contacts_autovalue.contactid "
                                  "WHERE %2 ORDER BY start_time").arg(serviceStatusColumns.join(", "),
                                                                      whereParts.join(" AND "));
        if ( !uploadQSOQuery.prepare(selectStatement) )
        {
            qCDebug(runtime) << "Cannot prepare SQL 1" << selectStatement;
            return;
        }
        uploadQSOQuery.bindValue(":station_callsign", ui->myCallsignCombo->currentText().trimmed());
        uploadQSOQuery.bindValue(":my_gridsquare", ui->myGridCombo->currentText().trimmed());

    }
    else
    {
        whereParts << QLatin1String("station_profiles.profile_name = :profile_name");

        selectStatement = QString("SELECT contacts.*, contacts_autovalue.*, %1 "
                                  "FROM contacts inner join station_profiles on %2 INNER JOIN contacts_autovalue ON contacts.id = contacts_autovalue.contactid "
                                  "WHERE %3 ORDER BY start_time").arg(serviceStatusColumns.join(", "),
                                                                      selectedStationProfile.getContactInnerJoin(),
                                                                      whereParts.join(" AND ") );

        if ( !uploadQSOQuery.prepare(selectStatement) )
        {
            qCDebug(runtime) << "Cannot prepare SQL 2" << selectStatement;
            return;
        }
        uploadQSOQuery.bindValue(":profile_name", selectedStationProfile.profileName);
    }

    qCDebug(runtime) << selectStatement << uploadQSOQuery.boundValues();

    if ( !uploadQSOQuery.exec() )
    {
        qCWarning(runtime) << "cannot execute statement" << uploadQSOQuery.lastError().text();
        return;
    }

    QList<QStandardItem*> rowItems;
    QStringList serviceNames;

    // update QSO Detail and QSO list
    while ( uploadQSOQuery.next() )
    {
        rowItems.clear();
        serviceNames.clear();

        auto rec = QSharedPointer<QSqlRecord>::create(uploadQSOQuery.record());
        allSelectedQSOs.insert(rec->value("id").toULongLong(), rec);

        bool eqslUploadStatus = rec->value("status_" + QString::number(ServiceID::EQSLID)).toBool();
        bool lotwUploadStatus = rec->value("status_" + QString::number(ServiceID::LOTWID)).toBool();

        for ( auto it = onlineServices.begin(); it != onlineServices.end(); ++it )
        {
            UploadTask &task = it.value();

            if ( !task.isChecked() ) continue;

            ServiceID taskID = task.getServiceID();
            bool forUploadMarked = false;

            // set forUploadMarked according to service dependency.
            switch (taskID)
            {
            case EQSLID: forUploadMarked = eqslUploadStatus; break;
            case LOTWID: forUploadMarked = lotwUploadStatus; break;
            case CLUBLOGID: forUploadMarked = lotwUploadStatus || rec->value("status_" + QString::number(taskID)).toBool(); break; // depends on the LoTW Receive field
            case HRDLOGID: forUploadMarked = eqslUploadStatus || lotwUploadStatus || rec->value("status_" + QString::number(taskID)).toBool(); break; //depends on EQSL and LoTW fields
            case QRZCOMID: forUploadMarked = (! serviceNames.empty()) || rec->value("status_" + QString::number(taskID)).toBool(); break; // all fields are sent
            case WAVELOGID: forUploadMarked = rec->value("status_" + QString::number(taskID)).toBool(); break;
            default: forUploadMarked = false;
            }

            if ( forUploadMarked )
            {
                serviceNames << task.getServiceName();
                task.addQSO(rec);
            }
        }
        rowItems << new QStandardItem(rec->value("start_time").toDateTime()
                                                              .toTimeZone(QTimeZone::utc())
                                                              .toString(locale.formatDateTimeShortWithYYYY()));
        rowItems << new QStandardItem(rec->value("callsign").toString());
        rowItems << new QStandardItem(rec->value("mode").toString());
        rowItems << new QStandardItem(serviceNames.join(", "));
        detailQSOsModel->appendRow(rowItems);
    }
    ui->detailQSOView->setModel(detailQSOsModel);
    ui->detailQSOView->resizeColumnsToContents();
    updateQSONumbers();
    qCDebug(runtime) << "finiched";
}

void UploadQSODialog::handleCallsignChange(const QString &myCallsign)
{
    FCT_IDENTIFICATION;

    ui->myGridLabel->blockSignals(true);
    ui->myGridCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(my_gridsquare) "
                                               "FROM contacts "
                                               "WHERE COALESCE(NULLIF(TRIM(station_callsign), ''), TRIM(operator)) ='" + myCallsign + "' ORDER BY my_gridsquare", tr("Any"), ui->myGridCombo));
    ui->myGridCombo->setCurrentIndex(0);
    executeQuery();
    ui->myGridLabel->blockSignals(false);
}

void UploadQSODialog::updateWavelogStationLabel()
{
    FCT_IDENTIFICATION;

    const CloudlogUploader::StationProfile &currProfile = availableWavelogStationIDs.value(ui->wavelogStationIDSpin->value());

    ui->wavelogStationIDProfileLabel->setText(( currProfile.station_id == ui->wavelogStationIDSpin->value()) ? currProfile.station_profile_name + " [" + currProfile.station_callsign + " / " + currProfile.station_gridsquare + "]"
                                                                                                             : tr("Unknown"));
}
