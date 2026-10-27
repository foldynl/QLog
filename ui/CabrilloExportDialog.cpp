#include "CabrilloExportDialog.h"
#include "ui_CabrilloExportDialog.h"

#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QMessageBox>
#include <QFileDialog>
#include "ui/QLogFileDialog.h"
#include <QSaveFile>
#include <QDir>
#include <QProgressDialog>
#include <QSettings>
#include <QTimer>

#include "core/debug.h"
#include "core/QSOFilterManager.h"
#include "data/BandPlan.h"
#include "data/StationProfile.h"
#include "logformat/CabrilloFormat.h"
#include "rig/macros.h"
#include "ui/CabrilloTemplateDialog.h"

MODULE_IDENTIFICATION("qlog.ui.cabrilloexportdialog");

CabrilloExportDialog::CabrilloExportDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CabrilloExportDialog)
{
    FCT_IDENTIFICATION;

    // Must be created before setupUi() because .ui signal connections
    // may trigger scheduleQsoCountUpdate() during initialization
    countDebounceTimer = new QTimer(this);
    countDebounceTimer->setSingleShot(true);
    countDebounceTimer->setInterval(300);
    connect(countDebounceTimer, &QTimer::timeout,
            this, &CabrilloExportDialog::updateQsoCount);

    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&Export"));

    for ( const CabrilloFormat::CategoryItem &item : static_cast<const QList<CabrilloFormat::CategoryItem>>(CabrilloFormat::bandCategories()) )
        ui->catBandCombo->addItem(item.label, item.value);

    for ( const CabrilloFormat::CategoryItem &item : static_cast<const QList<CabrilloFormat::CategoryItem>>(CabrilloFormat::modeCategories()) )
        ui->catModeCombo->addItem(item.label, item.value);

    for ( const CabrilloFormat::CategoryItem &item : static_cast<const QList<CabrilloFormat::CategoryItem>>(CabrilloFormat::powerCategories()) )
        ui->catPowerCombo->addItem(item.label, item.value);

    for ( const CabrilloFormat::CategoryItem &item : static_cast<const QList<CabrilloFormat::CategoryItem>>(CabrilloFormat::operatorCategories()) )
        ui->catOperatorCombo->addItem(item.label, item.value);

    for ( const CabrilloFormat::CategoryItem &item : static_cast<const QList<CabrilloFormat::CategoryItem>>(CabrilloFormat::assistedCategories()) )
        ui->catAssistedCombo->addItem(item.label, item.value);

    // Optional category combos — empty first item means "not included in output"
    ui->catStationCombo->addItem(QString(), QString());
    for ( const CabrilloFormat::CategoryItem &item : static_cast<const QList<CabrilloFormat::CategoryItem>>(CabrilloFormat::stationCategories()) )
        ui->catStationCombo->addItem(item.label, item.value);

    ui->catTransmitterCombo->addItem(QString(), QString());
    for ( const CabrilloFormat::CategoryItem &item : static_cast<const QList<CabrilloFormat::CategoryItem>>(CabrilloFormat::transmitterCategories()) )
        ui->catTransmitterCombo->addItem(item.label, item.value);

    ui->catTimeCombo->addItem(QString(), QString());
    for ( const CabrilloFormat::CategoryItem &item : static_cast<const QList<CabrilloFormat::CategoryItem>>(CabrilloFormat::timeCategories()) )
        ui->catTimeCombo->addItem(item.label, item.value);

    ui->catOverlayCombo->addItem(QString(), QString());
    for ( const CabrilloFormat::CategoryItem &item : static_cast<const QList<CabrilloFormat::CategoryItem>>(CabrilloFormat::overlayCategories()) )
        ui->catOverlayCombo->addItem(item.label, item.value);

    connect(ui->catOperatorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int)
    {
        bool isMultiOp = (ui->catOperatorCombo->currentData().toString()
                          == CabrilloFormat::OPERATOR_MULTI);
        ui->transmitterIdSpin->setEnabled(isMultiOp);
        if ( !isMultiOp )
            ui->transmitterIdSpin->setValue(0);
    });

    ui->startDateEdit->setDateTime(QDateTime(QDate::currentDate().addDays(-2), QTime(0, 0), Qt::UTC));
    ui->endDateEdit->setDateTime(QDateTime(QDate::currentDate(), QTime(23, 59), Qt::UTC));

    const QStringList filterList = QSOFilterManager::instance()->getFilterList();
    for ( const QString &name : filterList )
        ui->userFilterCombo->addItem(name);

    loadTemplates();
    refreshCallsigns();

    // select callsign from current station profile
    const StationProfile currentProfile = StationProfilesManager::instance()->getCurProfile1();
    if ( !currentProfile.callsign.isEmpty() )
    {
        int idx = ui->callsignCombo->findText(currentProfile.callsign.toUpper());
        if ( idx >= 0 )
            ui->callsignCombo->setCurrentIndex(idx);
    }
}

CabrilloExportDialog::~CabrilloExportDialog()
{
    FCT_IDENTIFICATION;

    delete ui;
}

void CabrilloExportDialog::browseFile()
{
    FCT_IDENTIFICATION;

    QSettings settings; //platform-dependent, must be present

    const QString &lastPath = ( ui->fileEdit->text().isEmpty() ) ? settings.value("export/last_path", QDir::homePath()).toString()
                                                                 : ui->fileEdit->text();

#ifdef QLOG_FLATPAK
    const QString filename = QLogFileDialog::getSaveFileName(this, windowTitle(), lastPath,
                                                             tr("Cabrillo Files (*.log);;CBR Files (*.cbr);;All Files (*)"), "cbr");
    if ( filename.isEmpty() )
        return;
    confirmedExportPath = filename;
#else
    QFileDialog dialog(this, windowTitle(), lastPath, tr("Cabrillo Files (*.log);;CBR Files (*.cbr);;All Files (*)"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix("cbr");

    if ( dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty() )
        return;

    const QString filename = dialog.selectedFiles().constFirst();
#endif

    if ( !filename.isEmpty() )
    {
        settings.setValue("export/last_path", QFileInfo(filename).path());
        ui->fileEdit->setText(filename);
    }
}

void CabrilloExportDialog::loadTemplates()
{
    FCT_IDENTIFICATION;

    ui->templateCombo->clear();

    const QList<CabrilloFormat::TemplateInfo> templates = CabrilloFormat::templateList();

    for ( const CabrilloFormat::TemplateInfo &tmpl : templates )
        ui->templateCombo->addItem(tmpl.name, tmpl.id);
}

void CabrilloExportDialog::manageTemplates()
{
    FCT_IDENTIFICATION;

    CabrilloTemplateDialog dialog(this);

    int currentId = ui->templateCombo->currentData().toInt();
    dialog.selectTemplate(currentId);

    if ( dialog.exec() == QDialog::Accepted )
    {
        loadTemplates();
        int idx = ui->templateCombo->findData(currentId);
        if ( idx >= 0 )
            ui->templateCombo->setCurrentIndex(idx);
    }
}

void CabrilloExportDialog::templateChanged(int index)
{
    FCT_IDENTIFICATION;

    if ( index < 0 )
        return;

    int tmplId = ui->templateCombo->itemData(index).toInt();
    const CabrilloFormat::TemplateInfo info = CabrilloFormat::templateInfo(tmplId);

    if ( !info.defaultCategoryMode.isEmpty() )
    {
        int modeIdx = ui->catModeCombo->findData(info.defaultCategoryMode);
        if ( modeIdx >= 0 )
            ui->catModeCombo->setCurrentIndex(modeIdx);
    }
}

void CabrilloExportDialog::refreshCallsigns()
{
    FCT_IDENTIFICATION;

    ui->callsignCombo->blockSignals(true);
    const QString previousCallsign = ui->callsignCombo->currentText();
    ui->callsignCombo->clear();

    QStringList conditions;
    conditions << "1 = 1";

    if ( ui->userFilterCheckBox->isChecked()
         && !ui->userFilterCombo->currentText().isEmpty() )
    {
        conditions << QSOFilterManager::getWhereClause(ui->userFilterCombo->currentText());
    }

    if ( ui->dateRangeCheckBox->isChecked() )
    {
        conditions << "(datetime(start_time) BETWEEN datetime(:start_date) AND datetime(:end_date))";
    }

    QString sql = QString("SELECT UPPER(station_callsign) as cs, COUNT(*) as cnt "
                          "FROM contacts WHERE %1 AND station_callsign IS NOT NULL "
                          "AND station_callsign != '' "
                          "GROUP BY cs ORDER BY cnt DESC")
                      .arg(conditions.join(" AND "));

    QSqlQuery query;
    if ( query.prepare(sql) )
    {
        if ( ui->dateRangeCheckBox->isChecked() )
        {
            query.bindValue(":start_date", ui->startDateEdit->dateTime().toUTC());
            query.bindValue(":end_date", ui->endDateEdit->dateTime().toUTC().addSecs(59));
        }

        if ( query.exec() )
        {
            while ( query.next() )
                ui->callsignCombo->addItem(query.value(0).toString());
        }
        else
        {
            qCWarning(runtime) << query.lastError().text();
        }
    }

    if ( !previousCallsign.isEmpty() )
    {
        int idx = ui->callsignCombo->findText(previousCallsign);
        if ( idx >= 0 )
            ui->callsignCombo->setCurrentIndex(idx);
    }

    ui->callsignCombo->blockSignals(false);

    if ( ui->callsignCombo->count() > 0 )
        callsignChanged(ui->callsignCombo->currentText());

    scheduleQsoCountUpdate();
}

void CabrilloExportDialog::callsignChanged(const QString &callsign)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << callsign;

    ui->operatorsEdit->setText(callsign);
    loadStationProfile(callsign);
    scheduleQsoCountUpdate();
}

void CabrilloExportDialog::scheduleQsoCountUpdate()
{
    FCT_IDENTIFICATION;

    const bool fmOnly = ui->catBandCombo->currentData().toString() == CabrilloFormat::BAND_VHF_FM_ONLY;
    ui->catModeCombo->setEnabled(!fmOnly);
    if ( fmOnly )
        ui->catModeCombo->setCurrentIndex(ui->catModeCombo->findData(CabrilloFormat::MODE_FM));

    countDebounceTimer->start();
}

void CabrilloExportDialog::updateQsoCount()
{
    FCT_IDENTIFICATION;

    if ( ui->callsignCombo->currentText().isEmpty() )
    {
        ui->qsoCountLabel->setText(tr("QSO(s): %1").arg(0));
        return;
    }

    resolveBandFilter();

    const QString sql = QString("SELECT COUNT(*) FROM contacts WHERE %1").arg(buildWhereClause());

    qCDebug(runtime) << sql;

    QSqlQuery query;
    if ( !query.prepare(sql) )
    {
        ui->qsoCountLabel->setText(tr("QSOs: ?"));
        return;
    }

    bindWhereClause(query);

    if ( query.exec() && query.next() )
        ui->qsoCountLabel->setText(tr("QSO(s): %1").arg(query.value(0).toInt()));
    else
        ui->qsoCountLabel->setText(tr("QSOs: ?"));
}

void CabrilloExportDialog::resolveBandFilter()
{
    FCT_IDENTIFICATION;

    bandFilterBands.clear();
    const QString selectedBand = ui->catBandCombo->currentData().toString();

    if ( selectedBand == CabrilloFormat::BAND_ALL )
        return;

    // Cabrillo VHF/UHF designators use different naming than QLog bands table
    static const QMap<QString, QStringList> cabrilloToBandNames =
    {
        {CabrilloFormat::BAND_222,         {"1.25m"}},
        {CabrilloFormat::BAND_432,         {"70cm"}},
        {CabrilloFormat::BAND_902,         {"33cm"}},
        {CabrilloFormat::BAND_1_2G,        {"23cm"}},
        {CabrilloFormat::BAND_LIGHT,       {"submm"}},
        {CabrilloFormat::BAND_VHF_3_BAND,  {"6m", "2m", "70cm"}},
        {CabrilloFormat::BAND_VHF_FM_ONLY, {"6m", "2m", "1.25m", "70cm", "33cm", "23cm"}}
    };

    const QStringList bandNames = cabrilloToBandNames.value(selectedBand, QStringList(selectedBand));
    const QList<Band> bands = BandPlan::bandsList();
    for ( const Band &band : bands )
    {
        if ( bandNames.contains(band.name, Qt::CaseInsensitive) )
            bandFilterBands.append(band);
    }
}

void CabrilloExportDialog::loadStationProfile(const QString &callsign)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << callsign;

    ui->nameValue->clear();
    ui->addressEdit->clear();
    ui->gridLocatorEdit->clear();

    if ( callsign.isEmpty() )
        return;

    const StationProfile profile = StationProfilesManager::instance()->findByCallsign(callsign);
    if ( !profile.callsign.isEmpty() )
    {
        ui->nameValue->setText(profile.operatorName);
        ui->addressEdit->setPlainText(profile.qthName);
        ui->gridLocatorEdit->setText(profile.locator);
    }
}

CabrilloFormat::HeaderData CabrilloExportDialog::buildHeaderData() const
{
    FCT_IDENTIFICATION;

    CabrilloFormat::HeaderData data;

    data.callsign = ui->callsignCombo->currentText();
    data.operatorName = ui->nameValue->text();
    data.email = ui->emailEdit->text();
    data.operators = ui->operatorsEdit->text();
    data.address = ui->addressEdit->toPlainText().trimmed();
    data.gridLocator = ui->gridLocatorEdit->text();

    data.categoryOperator = ui->catOperatorCombo->currentData().toString();
    data.categoryBand = ui->catBandCombo->currentData().toString();
    data.categoryPower = ui->catPowerCombo->currentData().toString();
    data.categoryMode = ui->catModeCombo->currentData().toString();
    data.categoryAssisted = ui->catAssistedCombo->currentData().toString();
    data.categoryStation = ui->catStationCombo->currentData().toString();
    data.categoryTransmitter = ui->catTransmitterCombo->currentData().toString();
    data.categoryTime = ui->catTimeCombo->currentData().toString();
    data.categoryOverlay = ui->catOverlayCombo->currentData().toString();

    data.location = ui->locationEdit->text();
    data.club = ui->clubEdit->text();
    data.offtime = ui->offtimeEdit->toPlainText().trimmed();
    data.soapbox = ui->soapboxEdit->toPlainText().trimmed();

    return data;
}

QString CabrilloExportDialog::buildWhereClause() const
{
    FCT_IDENTIFICATION;

    QStringList conditions;
    conditions << "UPPER(station_callsign) = UPPER(:callsign)";

    if ( ui->dateRangeCheckBox->isChecked() )
        conditions << "(datetime(start_time) BETWEEN datetime(:export_start_date) AND datetime(:export_end_date))";

    if ( ui->userFilterCheckBox->isChecked()
         && !ui->userFilterCombo->currentText().isEmpty() )
    {
        conditions << QSOFilterManager::getWhereClause(ui->userFilterCombo->currentText());
    }

    if ( ui->catBandCombo->currentData().toString() != CabrilloFormat::BAND_ALL )
    {
        QStringList frequencyConditions;
        QStringList bandParameters;
        for ( int index = 0; index < bandFilterBands.size(); ++index )
        {
            frequencyConditions << QString("CAST(ROUND(freq * 1000000.0) AS INTEGER) "
                                           "BETWEEN :band_start_freq_hz_%1 AND :band_end_freq_hz_%1").arg(index);
            bandParameters << QString(":band_name_%1").arg(index);
        }

        if ( frequencyConditions.isEmpty() )
            conditions << QStringLiteral("0");
        else
            conditions << QString("((freq > 0 AND (%1)) OR ((freq IS NULL OR freq <= 0) "
                                  "AND band COLLATE NOCASE IN (%2)))")
                          .arg(frequencyConditions.join(" OR "), bandParameters.join(", "));
    }

    const QString selectedMode = ui->catModeCombo->currentData().toString();
    if ( selectedMode != CabrilloFormat::MODE_MIXED )
    {
        if ( selectedMode == CabrilloFormat::MODE_DIGI )
            conditions << "mode IN (SELECT name FROM modes WHERE dxcc = 'DIGITAL')";
        else
            conditions << "UPPER(mode) = UPPER(:export_mode)";
    }

    return conditions.join(" AND ");
}

void CabrilloExportDialog::bindWhereClause(QSqlQuery &query) const
{
    FCT_IDENTIFICATION;

    query.bindValue(":callsign", ui->callsignCombo->currentText());

    if ( ui->dateRangeCheckBox->isChecked() )
    {
        query.bindValue(":export_start_date", ui->startDateEdit->dateTime().toUTC());
        query.bindValue(":export_end_date", ui->endDateEdit->dateTime().toUTC().addSecs(59));
    }

    for ( int index = 0; index < bandFilterBands.size(); ++index )
    {
        const Band &band = bandFilterBands.at(index);
        query.bindValue(QString(":band_start_freq_hz_%1").arg(index), MHz2Hz(band.start));
        query.bindValue(QString(":band_end_freq_hz_%1").arg(index), MHz2Hz(band.end));
        query.bindValue(QString(":band_name_%1").arg(index), band.name);
    }

    const QString selectedMode = ui->catModeCombo->currentData().toString();

    if ( selectedMode != CabrilloFormat::MODE_MIXED && selectedMode != CabrilloFormat::MODE_DIGI )
        query.bindValue(":export_mode", selectedMode);
}

void CabrilloExportDialog::accept()
{
    FCT_IDENTIFICATION;

#ifdef QLOG_FLATPAK
    if ( !ui->fileEdit->text().isEmpty() && ui->fileEdit->text() != confirmedExportPath )
    {
        browseFile();
        if ( ui->fileEdit->text() != confirmedExportPath )
            return;
    }
#endif

    if ( ui->templateCombo->currentIndex() < 0 )
    {
        QMessageBox::warning(this, tr("QLog Warning"),
                             tr("Please select a contest template."));
        return;
    }

    if ( ui->fileEdit->text().isEmpty() )
    {
        QMessageBox::warning(this, tr("QLog Warning"),
                             tr("Please select an output file."));
        return;
    }

    if ( ui->callsignCombo->currentText().isEmpty() )
    {
        QMessageBox::warning(this, tr("QLog Warning"),
                             tr("No callsign available. Check your filters."));
        return;
    }

    const CabrilloFormat::HeaderData headerData = buildHeaderData();
    if ( !CabrilloFormat::isValidOfftime(headerData.offtime) )
    {
        QMessageBox::warning(this, tr("QLog Warning"),
                             tr("Off-Time must contain one UTC interval in the format "
                                "yyyy-mm-dd hhmm yyyy-mm-dd hhmm. Use valid dates and times, "
                                "with the end at or after the start."));
        return;
    }

    resolveBandFilter();

    const QString whereClause = buildWhereClause();
    QString sql = QString("SELECT * FROM contacts WHERE %1 ORDER BY start_time ASC").arg(whereClause);

    qCDebug(runtime) << sql;

    QSqlQuery query;
    if ( !query.prepare(sql) )
    {
        qCWarning(runtime) << "Failed to prepare export query:" << query.lastError().text();
        QLogFileDialog::showError(this, tr("Export Cabrillo"),
                              tr("Cannot prepare contact selection for the Cabrillo export %1:\n%2").arg(ui->fileEdit->text(), query.lastError().text()));
        return;
    }
    bindWhereClause(query);

    if ( !query.exec() )
    {
        qCWarning(runtime) << "Failed to execute export query:" << query.lastError().text();
        QLogFileDialog::showError(this, tr("Export Cabrillo"),
                              tr("Cannot read contacts for the Cabrillo export %1:\n%2").arg(ui->fileEdit->text(), query.lastError().text()));
        return;
    }

    QList<QSqlRecord> records;
    while ( query.next() )
        records.append(query.record());
    if ( query.lastError().isValid() )
    {
        QLogFileDialog::showError(this, tr("Export Cabrillo"),
                              tr("Cannot finish reading contacts for the Cabrillo export %1:\n%2")
                                  .arg(ui->fileEdit->text(), query.lastError().text()));
        return;
    }

    bool hasFrequencyColumn = false;
    const QList<CabrilloFormat::ColumnDef> columns =
            CabrilloFormat::loadTemplateColumns(ui->templateCombo->currentData().toInt());
    for ( const CabrilloFormat::ColumnDef &column : columns )
    {
        if ( column.formatter == CabrilloFormat::FMT_FREQ_KHZ )
        {
            hasFrequencyColumn = true;
            break;
        }
    }

    QStringList unsupportedBandOnlyBands;
    if ( hasFrequencyColumn )
    {
        for ( const QSqlRecord &record : static_cast<const QList<QSqlRecord>&>(records) )
        {
            if ( record.value("freq").toDouble() <= 0.0 )
            {
                const QString band = record.value("band").toString();
                if ( CabrilloFormat::bandToFrequencyField(band).isEmpty() )
                    unsupportedBandOnlyBands << band;
            }
        }
    }

    unsupportedBandOnlyBands.removeDuplicates();
    if ( !unsupportedBandOnlyBands.isEmpty() )
    {
        QMessageBox::warning(this, tr("QLog Warning"),
                             tr("Cabrillo does not define a frequency designator for band-only QSO(s) on: %1. "
                                "Enter an exact frequency or exclude these QSOs from the export.")
                             .arg(unsupportedBandOnlyBands.join(", ")));
        return;
    }

    QSaveFile file(ui->fileEdit->text());
    if ( !file.open(QIODevice::WriteOnly | QIODevice::Text) )
    {
        QLogFileDialog::showError(this, tr("Export Cabrillo"),
                              tr("Cannot create the Cabrillo export file %1:\n%2").arg(ui->fileEdit->text(), file.errorString()));
        return;
    }

    QTextStream stream(&file);
    CabrilloFormat format(stream);
    format.setTemplateId(ui->templateCombo->currentData().toInt());
    format.setHeaderData(headerData);
    format.setTransmitterId(ui->transmitterIdSpin->value());

    QProgressDialog progress(tr("Exporting Cabrillo..."), QString(), 0, 100, this);
    progress.setWindowTitle(tr("Cabrillo Export Progress"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    connect(&format, &CabrilloFormat::exportProgress, this, [&progress](float pct) {
        progress.setValue(static_cast<int>(pct));
    });

    long count = format.runExport(records);

    stream.flush();
    progress.close();

    if ( stream.status() != QTextStream::Ok || !file.flush() || !file.commit() )
    {
        qCWarning(runtime) << "Failed to write Cabrillo export:" << file.fileName() << file.errorString();
        QLogFileDialog::showError(this, tr("Export Cabrillo"),
                              tr("Cannot finish saving the Cabrillo export %1:\n%2").arg(ui->fileEdit->text(), file.errorString()));
        return;
    }

    QMessageBox::information(this, tr("QLog Information"),
                             tr("Exported %n QSO(s) to Cabrillo file.", "", count));

    QDialog::accept();
}
