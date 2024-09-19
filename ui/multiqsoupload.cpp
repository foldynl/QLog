#include "multiqsoupload.h"
#include "ui_multiqsoupload.h"

#include <QTextStream>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QNetworkReply>

#include "LotwDialog.h"
#include "logformat/AdiFormat.h"
#include "core/Lotw.h"
#include "core/debug.h"
#include "ui/QSLImportStatDialog.h"
#include "models/SqlListModel.h"
#include "ui/ShowUploadDialog.h"
#include "data/StationProfile.h"
#include "core/Eqsl.h"
#include "core/ClubLog.h"
#include "core/HRDLog.h"
#include "core/QRZ.h"

MODULE_IDENTIFICATION("qlog.ui.multiqsoupload");

MultiQSOUpload::MultiQSOUpload(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MultiQSOUpload)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    /* Upload */

    ui->myCallsignCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(station_callsign) FROM contacts ORDER BY station_callsign", ""));
    ui->myGridCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(my_gridsquare) FROM contacts WHERE station_callsign ='"
                                                   + ui->myCallsignCombo->currentText()
                                                   + "' ORDER BY my_gridsquare", ""));
    loadDialogState();
}

MultiQSOUpload::~MultiQSOUpload()
{
    FCT_IDENTIFICATION;

    delete ui;
}

void MultiQSOUpload::upload()
{
    FCT_IDENTIFICATION;

    if ( ! Lotw::getUsername().isEmpty() && ui->LoTWcheckBox->isChecked())
    {
        LoTWUpload();
    }
    else if (ui->LoTWcheckBox->isChecked())
    {
        QMessageBox::warning(this, tr("QLog Warning"), tr("LoTW is not configured properly.<p> Please, use <b>Settings</b> dialog to configure it.</p>"));
    }

    if ( ! EQSL::getUsername().isEmpty() && ui->eQSLcheckBox->isChecked())
    {
        EQSLupload();
    }
    else if (ui->eQSLcheckBox->isChecked())
    {
        QMessageBox::warning(this, tr("QLog Warning"), tr("eQSL is not configured properly.<p> Please, use <b>Settings</b> dialog to configure it.</p>"));
    }

    if ( ! ClubLog::getEmail().isEmpty() && ui->ClubLogcheckBox->isChecked() )
    {
        ClubLogUpload();
    }
    else if (ui->ClubLogcheckBox->isChecked())
    {
        QMessageBox::warning(this, tr("QLog Warning"), tr("Clublog is not configured properly.<p> Please, use <b>Settings</b> dialog to configure it.</p>"));
    }

    if ( ! HRDLog::getRegisteredCallsign().isEmpty() && ui->HRDLogcheckBox->isChecked())
    {
        HRDLogUpload();
    }
    else if (ui->HRDLogcheckBox->isChecked())
    {
        QMessageBox::warning(this, tr("QLog Warning"), tr("HRDLog is not configured properly.<p> Please, use <b>Settings</b> dialog to configure it.</p>"));
    }

    QString logbookAPIKey = QRZ::getLogbookAPIKey();

    if ( !logbookAPIKey.isEmpty() && ui->QRZcheckBox->isChecked())
    {
        QRZUpload();
    }
    else if (ui->QRZcheckBox->isChecked())
    {
        QMessageBox::warning(this, tr("QLog Warning"), tr("QRZ.com is not configured properly.<p> Please, use <b>Settings</b> dialog to configure it.</p>"));
    }

    saveDialogState();
}

void MultiQSOUpload::saveDialogState()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    settings.setValue("MultiQSOUpload/last_mycallsign", ui->myCallsignCombo->currentText());
    settings.setValue("MultiQSOUpload/last_mygrid", ui->myGridCombo->currentText());
    settings.setValue("MultiQSOUplaod/LoTW", ui->LoTWcheckBox->isChecked());
    settings.setValue("MultiQSOUplaod/QRZ", ui->QRZcheckBox->isChecked());
    settings.setValue("MultiQSOUplaod/eQSL", ui->eQSLcheckBox->isChecked());
    settings.setValue("MultiQSOUplaod/ClubLog", ui->ClubLogcheckBox->isChecked());
    settings.setValue("MultiQSOUplaod/HRDLog", ui->HRDLogcheckBox->isChecked());
}

void MultiQSOUpload::loadDialogState()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();

    int index = ui->myCallsignCombo->findText(profile.callsign);

    if ( index >= 0 )
        ui->myCallsignCombo->setCurrentIndex(index);
    else
        ui->myCallsignCombo->setCurrentText(settings.value("MultiQSOUpload/last_mycallsign").toString());

    index = ui->myGridCombo->findText(profile.locator);

    if ( index >= 0 )
        ui->myGridCombo->setCurrentIndex(index);
    else
        ui->myGridCombo->setCurrentText(settings.value("MultiQSOUpload/last_mygrid").toString());

    ui->LoTWcheckBox->setChecked(settings.value("MultiQSOUplaod/LoTW",0).toBool());
    ui->QRZcheckBox->setChecked(settings.value("MultiQSOUplaod/QRZ",0).toBool());
    ui->eQSLcheckBox->setChecked(settings.value("MultiQSOUplaod/eQSL",0).toBool());
    ui->ClubLogcheckBox->setChecked(settings.value("MultiQSOUplaod/ClubLog",0).toBool());
    ui->HRDLogcheckBox->setChecked(settings.value("MultiQSOUplaod/HRDLog",0).toBool());
}

void MultiQSOUpload::LoTWUpload() {
    FCT_IDENTIFICATION;

    QByteArray data;
    QTextStream stream(&data, QIODevice::ReadWrite);

    AdiFormat adi(stream);
    QString QSOList;
    int count = 0;

    QStringList qslSentStatuses = {"'R'", "'Q'"};

    if ( ui->addlSentStatusI_Lotw->isChecked() )
    {
        qslSentStatuses << "'I'";
    }

    if ( ui->addlSentStatusN_Lotw->isChecked() )
    {
        qslSentStatuses << "'N'";
    }

    QString query_string = "SELECT callsign, freq, band, freq_rx, "
                           "       mode, submode, start_time, prop_mode, "
                           "       sat_name, station_callsign, operator, "
                           "       rst_sent, rst_rcvd, my_state, my_cnty, "
                           "       my_vucc_grids "
                           "FROM contacts ";
    QString query_where =  QString("WHERE (upper(lotw_qsl_sent) in (%1) OR lotw_qsl_sent is NULL) "
                                  "               AND (upper(prop_mode) NOT IN ('INTERNET', 'RPT', 'ECH', 'IRL') OR prop_mode IS NULL) ").arg(qslSentStatuses.join(","));
    QString query_order = " ORDER BY start_time ";

    saveDialogState();

    if ( !ui->myCallsignCombo->currentText().isEmpty() )
    {
        query_where.append(" AND station_callsign = '" + ui->myCallsignCombo->currentText() + "'");
    }

    if ( !ui->myGridCombo->currentText().isEmpty() )
    {
        query_where.append(" AND my_gridsquare = '" + ui->myGridCombo->currentText() + "'");
    }

    query_string = query_string + query_where + query_order;

    qCDebug(runtime) << query_string;

    QSqlQuery query(query_string);

    while (query.next())
    {
        QSqlRecord record = query.record();

        QSOList.append(" "
                       + record.value("start_time").toDateTime().toTimeSpec(Qt::UTC).toString(locale.formatDateTimeShortWithYYYY())
                       + "\t" + record.value("callsign").toString()
                       + "\t" + record.value("mode").toString()
                       + "\n");

        adi.exportContact(record);
        count++;
    }

    stream.flush();

    if (count > 0)
    {
        ShowUploadDialog showDialog(QSOList);

        if ( showDialog.exec() == QDialog::Accepted )
        {

            QProgressDialog* dialog = new QProgressDialog(tr("Processing LoTW"),
                                                          "",
                                                          0, 0, this);
            dialog->setCancelButton(nullptr);
            dialog->setWindowModality(Qt::WindowModal);
            dialog->setAttribute(Qt::WA_DeleteOnClose, true);
            dialog->setAutoClose(true);
            dialog->setMinimumDuration(0);
            dialog->show();

            Lotw *lotw = new Lotw(dialog);

            connect(lotw, &Lotw::uploadFinished, this, [this, lotw, dialog, query_where, count]()
                    {
                        dialog->done(QDialog::Accepted);

                        QMessageBox::information(this, tr("QLog Information"), tr("%n QSO(s) uploaded.", "", count));

                        QString queryString = "UPDATE contacts "
                                              "SET lotw_qsl_sent='Y', lotw_qslsdate = strftime('%Y-%m-%d',DATETIME('now', 'utc')) "
                                              + query_where;

                        qCDebug(runtime) << queryString;

                        QSqlQuery query_update(queryString);
                        if ( ! query_update.exec() )
                        {
                            qWarning() << "Cannot execute update query" << query_update.lastError().text();
                            return;
                        }
                        lotw->deleteLater();
                    });

            connect(lotw, &Lotw::uploadError, this, [this, lotw, dialog](const QString &errorString)
                    {
                        qInfo() << "Error" << errorString;
                        dialog->done(QDialog::Accepted);
                        qCInfo(runtime) << "LoTW Upload Error: " << errorString;
                        QMessageBox::warning(this, tr("QLog Warning"),
                                             tr("Cannot upload the LoTW QSO(s): ") + errorString);
                        lotw->deleteLater();
                    });

            lotw->uploadAdif(data);
        }
    }
    else {
        QMessageBox::information(this, tr("QLog Information"), tr("No LoTW QSOs found to upload."));
    }
}

void MultiQSOUpload::EQSLupload()
{
    FCT_IDENTIFICATION;

    QByteArray data;
    QTextStream stream(&data, QIODevice::ReadWrite);

    AdiFormat adi(stream);
    QString QSOList;
    int count = 0;

    QStringList qslSentStatuses = {"'R'", "'Q'"};

    if ( ui->addlSentStatusI_eQSL->isChecked() )
    {
        qslSentStatuses << "'I'";
    }

    if ( ui->addlSentStatusN_eQSL->isChecked() )
    {
        qslSentStatuses << "'N'";
    }

    /* http://www.eqsl.cc/qslcard/ADIFContentSpecs.cfm */
    QString query_string = "SELECT start_time, callsign, mode, freq, band, "
                           "       prop_mode, rst_sent, submode, "
                           "       sat_mode, sat_name, "
                           "       my_cnty, my_gridsquare ";
    QString query_from   = "FROM contacts ";
    QString query_where =  QString("WHERE (upper(eqsl_qsl_sent) in (%1) OR eqsl_qsl_sent is NULL) ").arg(qslSentStatuses.join(","));
    QString query_order = " ORDER BY start_time ";

    saveDialogState();

    if ( ui->uploadcommentCheck->isChecked() )
    {
        query_string.append(", substr(comment,1,240) as qslmsg ");
    }

    if ( !ui->myCallsignCombo->currentText().isEmpty() )
    {
        query_where.append(" AND station_callsign = '" + ui->myCallsignCombo->currentText() + "'");
    }

    if ( !ui->myGridCombo->currentText().isEmpty() )
    {
        query_where.append(" AND my_gridsquare = '" + ui->myGridCombo->currentText() + "'");
    }

    query_string = query_string + query_from + query_where + query_order;

    qCDebug(runtime) << query_string;

    QSqlQuery query(query_string);

    adi.exportStart();

    QMap<QString, QString> *applTags = nullptr;
    if ( !ui->qthProfileUploadEdit->text().isEmpty() )
    {
        applTags = new QMap<QString, QString>;
        applTags->insert("app_eqsl_qth_nickname", ui->qthProfileUploadEdit->text());
    }

    while (query.next())
    {
        QSqlRecord record = query.record();

        QSOList.append(" "
                       + record.value("start_time").toDateTime().toTimeSpec(Qt::UTC).toString(locale.formatDateTimeShortWithYYYY())
                       + "\t" + record.value("callsign").toString()
                       + "\t" + record.value("mode").toString()
                       + "\n");

        adi.exportContact(record, applTags);
        count++;
    }

    if ( applTags )
    {
        delete applTags;
    }

    stream.flush();

    if (count > 0)
    {
        ShowUploadDialog showDialog(QSOList);

        if ( showDialog.exec() == QDialog::Accepted )
        {
            QProgressDialog* dialog = new QProgressDialog(tr("Uploading to eQSL"), tr("Cancel"), 0, 0, this);
            dialog->setWindowModality(Qt::WindowModal);
            dialog->setRange(0, 0);
            dialog->show();

            EQSL *eQSL = new EQSL(dialog);

            connect(eQSL, &EQSL::uploadOK, this, [this, eQSL, dialog, query_where, count](const QString &msg)
                    {
                        dialog->done(QDialog::Accepted);
                        qCDebug(runtime) << "eQSL Upload OK: " << msg;
                        QMessageBox::information(this, tr("QLog Information"), tr("%n QSO(s) uploaded.", "", count));
                        QString query_string = "UPDATE contacts "
                                               "SET eqsl_qsl_sent='Y', eqsl_qslsdate = strftime('%Y-%m-%d',DATETIME('now', 'utc')) "
                                               + query_where;

                        qCDebug(runtime) << query_string;

                        QSqlQuery query_update(query_string);
                        query_update.exec();
                        eQSL->deleteLater();
                    });

            connect(eQSL, &EQSL::uploadError, this, [this, eQSL, dialog](const QString &msg)
                    {
                        dialog->done(QDialog::Accepted);
                        qCInfo(runtime) << "eQSL Upload Error: " << msg;
                        QMessageBox::warning(this, tr("QLog Warning"), tr("Cannot upload the eQSL QSO(s): ") + msg);
                        eQSL->deleteLater();
                    });

            connect(dialog, &QProgressDialog::canceled, this, [eQSL]()
                    {
                        qCDebug(runtime)<< "Operation canceled";
                        eQSL->abortRequest();
                        eQSL->deleteLater();
                    });

            eQSL->uploadAdif(data);
        }
    }
    else
    {
        QMessageBox::information(this, tr("QLog Information"), tr("No eQSL QSOs found to upload."));
    }
}

void MultiQSOUpload::ClubLogUpload()
{
    FCT_IDENTIFICATION;

    QByteArray data;
    QTextStream stream(&data, QIODevice::ReadWrite);

    AdiFormat adi(stream);

    QString QSOList;
    int count = 0;

    QStringList qslUploadStatuses = {"'M'"};

    if ( ui->addlUploadStatusN_ClubLog->isChecked() )
    {
        qslUploadStatuses << "'N'";
    }

    if ( ui->clearReuploadCheckbox_ClubLog->isChecked() )
    {
        //reupload all QSOs (except N)
        qslUploadStatuses << "'Y'";
    }

    /* https://clublog.freshdesk.com/support/solutions/articles/54905-how-to-upload-logs-directly-into-club-log */
    /* https://clublog.freshdesk.com/support/solutions/articles/53202-which-adif-fields-does-club-log-use- */
    QString query_string = QString("SELECT %1 FROM contacts ").arg(ClubLog::supportedDBFields.join(" , "));
    QString query_where =  QString("WHERE (upper(clublog_qso_upload_status) in (%1) OR clublog_qso_upload_status is NULL) ").arg(qslUploadStatuses.join(","));
    QString query_order = " ORDER BY start_time ";

    saveDialogState();

    if ( !ui->myCallsignCombo->currentText().isEmpty() )
    {
        query_where.append(" AND station_callsign = '" + ui->myCallsignCombo->currentText() + "'");
    }

    if ( !ui->myGridCombo->currentText().isEmpty() )
    {
        query_where.append(" AND my_gridsquare = '" + ui->myGridCombo->currentText() + "'");
    }

    query_string = query_string  + query_where + query_order;

    qCDebug(runtime) << query_string;

    QSqlQuery query(query_string);

    adi.exportStart();

    while (query.next())
    {
        const QSqlRecord &record = query.record();

        QSOList.append(" "
                       + record.value("start_time").toDateTime().toTimeSpec(Qt::UTC).toString(locale.formatDateTimeShortWithYYYY())
                       + "\t" + record.value("callsign").toString()
                       + "\t" + record.value("mode").toString()
                       + "\n");

        adi.exportContact(record);
        count++;
    }

    stream.flush();

    if (count > 0)
    {
        ShowUploadDialog showDialog(QSOList);

        if ( showDialog.exec() == QDialog::Accepted )
        {
            QProgressDialog* dialog = new QProgressDialog(tr("Uploading to Clublog"), tr("Cancel"), 0, 0, this);
            dialog->setWindowModality(Qt::WindowModal);
            dialog->setRange(0, 0);
            dialog->setAttribute(Qt::WA_DeleteOnClose, true);
            dialog->show();

            ClubLog *clublog = new ClubLog(dialog);

            connect(clublog, &ClubLog::uploadFileOK, this, [this, dialog, query_where, count, clublog](const QString &msg)
                    {
                        dialog->done(QDialog::Accepted);
                        qCDebug(runtime) << "Clublog Upload OK: " << msg;
                        QMessageBox::information(this, tr("QLog Information"), tr("%n QSO(s) uploaded.", "", count));
                        QString query_string = "UPDATE contacts "
                                               "SET clublog_qso_upload_status='Y', clublog_qso_upload_date = strftime('%Y-%m-%d',DATETIME('now', 'utc')) "
                                               + query_where;

                        qCDebug(runtime) << query_string;

                        QSqlQuery query_update(query_string);
                        query_update.exec();
                        clublog->deleteLater();
                    });

            connect(clublog, &ClubLog::uploadError, this, [this, dialog, clublog](const QString &msg)
                    {
                        dialog->done(QDialog::Accepted);
                        qCInfo(runtime) << "Clublog Upload Error: " << msg;
                        QMessageBox::warning(this, tr("QLog Warning"), tr("Cannot upload the Clublog QSO(s): ") + msg);
                        clublog->deleteLater();
                    });

            connect(dialog, &QProgressDialog::canceled, this, [clublog]()
                    {
                        qCDebug(runtime)<< "Operation canceled";
                        clublog->abortRequest();
                        clublog->deleteLater();
                    });

            clublog->uploadAdif(data,
                                ui->myCallsignCombo->currentText(),
                                ui->clearReuploadCheckbox_ClubLog->isChecked());
        }
    }
    else
    {
        QMessageBox::information(this, tr("QLog Information"), tr("No Clublog QSOs found to upload."));
    }
}

void MultiQSOUpload::HRDLogUpload()
{
    FCT_IDENTIFICATION;

    QString QSOList;
    int count = 0;
    QStringList qslUploadStatuses = {"'M'"};

    if ( ui->addlUploadStatusN_HRD->isChecked() )
    {
        qslUploadStatuses << "'N'";
    }

    // http://www.iw1qlh.net/projects/hrdlog/HRDLognet_4.pdf
    // It is not clear what QLog should send to HRDLog. Therefore it will
    // send all ADIF-fields
    QString query_string = "SELECT * ";
    QString query_from   = "FROM contacts ";
    QString query_where =  QString("WHERE (upper(hrdlog_qso_upload_status) in (%1) OR hrdlog_qso_upload_status is NULL) ").arg(qslUploadStatuses.join(","));
    QString query_order = " ORDER BY start_time ";

    saveDialogState();

    if ( !ui->myCallsignCombo->currentText().isEmpty() )
    {
        query_where.append(" AND station_callsign = '" + ui->myCallsignCombo->currentText() + "'");
    }

    if ( !ui->myGridCombo->currentText().isEmpty() )
    {
        query_where.append(" AND my_gridsquare = '" + ui->myGridCombo->currentText() + "'");
    }

    query_string = query_string + query_from + query_where + query_order;

    qCDebug(runtime) << query_string;

    QSqlQuery query(query_string);
    QList<QSqlRecord> qsos;

    while (query.next())
    {
        QSqlRecord record = query.record();

        QSOList.append(" "
                       + record.value("start_time").toDateTime().toTimeSpec(Qt::UTC).toString(locale.formatDateTimeShortWithYYYY())
                       + "\t" + record.value("callsign").toString()
                       + "\t" + record.value("mode").toString()
                       + "\n");

        qsos.append(record);
    }

    count = qsos.count();

    if (count > 0)
    {
        ShowUploadDialog showDialog(QSOList);

        if ( showDialog.exec() == QDialog::Accepted )
        {
            QProgressDialog* dialog = new QProgressDialog(tr("Uploading to HRDLOG"),
                                                          tr("Cancel"),
                                                          0, count, this);
            dialog->setWindowModality(Qt::WindowModal);
            dialog->setValue(0);
            dialog->setAttribute(Qt::WA_DeleteOnClose, true);
            dialog->show();

            HRDLog *hrdlog = new HRDLog(dialog);

            connect(hrdlog, &HRDLog::uploadedQSO, this, [hrdlog, dialog](int qsoID)
                    {
                        QString query_string = "UPDATE contacts "
                                               "SET hrdlog_qso_upload_status='Y', hrdlog_qso_upload_date = strftime('%Y-%m-%d',DATETIME('now', 'utc')) "
                                               "WHERE id = :qsoID";

                        qCDebug(runtime) << query_string;

                        QSqlQuery query_update;

                        query_update.prepare(query_string);
                        query_update.bindValue(":qsoID", qsoID);

                        if ( ! query_update.exec() )
                        {
                            qInfo() << "Cannot Update HRDLog status for QSO number " << qsoID << " " << query_update.lastError().text();
                            hrdlog->abortRequest();
                            hrdlog->deleteLater();
                        }
                        dialog->setValue(dialog->value() + 1);
                    });

            connect(hrdlog, &HRDLog::uploadFinished, this, [this, hrdlog, dialog, count](bool)
                    {
                        dialog->done(QDialog::Accepted);
                        QMessageBox::information(this, tr("QLog Information"),
                                                 tr("%n HRDLog QSO(s) uploaded.", "", count));
                        hrdlog->deleteLater();
                    });

            connect(hrdlog, &HRDLog::uploadError, this, [this, hrdlog, dialog](const QString &msg)
                    {
                        dialog->done(QDialog::Accepted);
                        qCInfo(runtime) << "HRDLog.com Upload Error: " << msg;
                        QMessageBox::warning(this, tr("QLog Warning"),
                                             tr("Cannot upload the HRDLog QSO(s): ") + msg);
                        hrdlog->deleteLater();
                    });

            connect(dialog, &QProgressDialog::canceled, hrdlog, &HRDLog::abortRequest);

            hrdlog->uploadContacts(qsos);
        }
    }
    else
    {
        QMessageBox::information(this, tr("QLog Information"), tr("No HRDLog QSOs found to upload."));
    }
}

void MultiQSOUpload::QRZUpload()
{
    FCT_IDENTIFICATION;

    QString QSOList;
    int count = 0;
    QStringList qslUploadStatuses = {"'M'"};

    if ( ui->addlUploadStatusN_QRZ->isChecked() )
    {
        qslUploadStatuses << "'N'";
    }

    /* https://www.qrz.com/docs/logbook/QRZLogbookAPI.html */
    /* ??? QRZ Support all ADIF Fields ??? */
    QString query_string = "SELECT * ";
    QString query_from   = "FROM contacts ";
    QString query_where =  QString("WHERE (upper(qrzcom_qso_upload_status) in (%1) OR qrzcom_qso_upload_status is NULL) ").arg(qslUploadStatuses.join(","));
    QString query_order = " ORDER BY start_time ";

    saveDialogState();

    if ( !ui->myCallsignCombo->currentText().isEmpty() )
    {
        query_where.append(" AND station_callsign = '" + ui->myCallsignCombo->currentText() + "'");
    }

    if ( !ui->myGridCombo->currentText().isEmpty() )
    {
        query_where.append(" AND my_gridsquare = '" + ui->myGridCombo->currentText() + "'");
    }

    query_string = query_string + query_from + query_where + query_order;

    qCDebug(runtime) << query_string;

    QSqlQuery query(query_string);
    QList<QSqlRecord> qsos;

    while (query.next())
    {
        QSqlRecord record = query.record();

        QSOList.append(" "
                       + record.value("start_time").toDateTime().toTimeSpec(Qt::UTC).toString(locale.formatDateTimeShortWithYYYY())
                       + "\t" + record.value("callsign").toString()
                       + "\t" + record.value("mode").toString()
                       + "\n");

        qsos.append(record);
    }

    count = qsos.count();

    if (count > 0)
    {
        ShowUploadDialog showDialog(QSOList);

        if ( showDialog.exec() == QDialog::Accepted )
        {
            QProgressDialog* dialog = new QProgressDialog(tr("Uploading to QRZ.com"),
                                                          tr("Cancel"),
                                                          0, count, this);
            dialog->setWindowModality(Qt::WindowModal);
            dialog->setValue(0);
            dialog->setAttribute(Qt::WA_DeleteOnClose, true);
            dialog->show();

            QRZ *qrz = new QRZ(dialog);

            connect(qrz, &QRZ::uploadedQSO, this, [qrz, dialog](int qsoID)
                    {
                        QString query_string = "UPDATE contacts "
                                               "SET qrzcom_qso_upload_status='Y', qrzcom_qso_upload_date = strftime('%Y-%m-%d',DATETIME('now', 'utc')) "
                                               "WHERE id = :qsoID";

                        qCDebug(runtime) << query_string;

                        QSqlQuery query_update;

                        query_update.prepare(query_string);
                        query_update.bindValue(":qsoID", qsoID);

                        if ( ! query_update.exec() )
                        {
                            qInfo() << "Cannot Update QRZCOM status for QSO number " << qsoID << " " << query_update.lastError().text();
                            qrz->abortQuery();
                            qrz->deleteLater();
                        }
                        dialog->setValue(dialog->value() + 1);
                    });

            connect(qrz, &QRZ::uploadFinished, this, [this, qrz, dialog, count](bool)
                    {
                        dialog->done(QDialog::Accepted);
                        QMessageBox::information(this, tr("QLog Information"),
                                                 tr("%n QRZ QSO(s) uploaded.", "", count));
                        qrz->deleteLater();
                    });

            connect(qrz, &QRZ::uploadError, this, [this, qrz, dialog](const QString &msg)
                    {
                        dialog->done(QDialog::Accepted);
                        qCInfo(runtime) << "QRZ.com Upload Error: " << msg;
                        QMessageBox::warning(this, tr("QLog Warning"),
                                             tr("Cannot upload the QRZ QSO(s): ") + msg);
                        qrz->deleteLater();
                    });

            connect(dialog, &QProgressDialog::canceled, qrz, &QRZ::abortQuery);

            qrz->uploadContacts(qsos);
        }
    }
    else
    {
        QMessageBox::information(this, tr("QLog Information"), tr("No QRZ QSOs found to upload."));
    }

}
