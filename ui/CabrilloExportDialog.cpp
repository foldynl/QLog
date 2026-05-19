#include "CabrilloExportDialog.h"
#include "ui_CabrilloExportDialog.h"

#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QProgressDialog>
#include <QSettings>
#include <QTimer>

#include "core/debug.h"
#include "core/QSOFilterManager.h"
#include "data/BandPlan.h"
#include "data/StationProfile.h"
#include "logformat/CabrilloFormat.h"
#include "ui/CabrilloTemplateDialog.h"

MODULE_IDENTIFICATION("qlog.ui.cabrilloexportdialog");

CabrilloExportDialog::CabrilloExportDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CabrilloExportDialog),
    bandStartFreq(0.0),
    bandEndFreq(0.0),
    bandFilterActive(false)
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

    QSettings settings;
    const QString &lastPath = ( ui->fileEdit->text().isEmpty() )
        ? settings.value("export/last_path", QDir::homePath()).toString()
        : ui->fileEdit->text();

    const QString filename = QFileDialog::getSaveFileName(this, nullptr, lastPath,
                                                          tr("Cabrillo Files (*.log);;CBR Files (*.cbr);;All Files (*)"));
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
            query.bindValue(":end_date", ui->endDateEdit->dateTime().toUTC());
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

    bandFilterActive = false;
    const QString selectedBand = ui->catBandCombo->currentData().toString();

    if ( selectedBand == CabrilloFormat::BAND_ALL
         || selectedBand == CabrilloFormat::BAND_VHF_3_BAND
         || selectedBand == CabrilloFormat::BAND_VHF_FM_ONLY
         || selectedBand == CabrilloFormat::BAND_LIGHT )
    {
        return;
    }

    // Cabrillo VHF/UHF designators use different naming than QLog bands table
    static const QMap<QString, QString> cabrilloToBandName =
    {
        {CabrilloFormat::BAND_222,  "1.25m"},
        {CabrilloFormat::BAND_432,  "70cm"},
        {CabrilloFormat::BAND_902,  "33cm"},
        {CabrilloFormat::BAND_1_2G, "23cm"}
    };

    const QString bandName = cabrilloToBandName.value(selectedBand, selectedBand);
    const Band band = BandPlan::bandName2Band(bandName);

    if ( !band.name.isEmpty() )
    {
        bandStartFreq = band.start;
        bandEndFreq = band.end;
        bandFilterActive = true;
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

    if ( bandFilterActive )
        conditions << "freq BETWEEN :band_start_freq AND :band_end_freq";

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
        query.bindValue(":export_end_date", ui->endDateEdit->dateTime().toUTC());
    }

    if ( bandFilterActive )
    {
        query.bindValue(":band_start_freq", bandStartFreq);
        query.bindValue(":band_end_freq", bandEndFreq);
    }

    const QString selectedMode = ui->catModeCombo->currentData().toString();

    if ( selectedMode != CabrilloFormat::MODE_MIXED && selectedMode != CabrilloFormat::MODE_DIGI )
        query.bindValue(":export_mode", selectedMode);
}

void CabrilloExportDialog::accept()
{
    FCT_IDENTIFICATION;

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

    resolveBandFilter();

    const QString whereClause = buildWhereClause();
    QString sql = QString("SELECT * FROM contacts WHERE %1 ORDER BY start_time ASC").arg(whereClause);

    qCDebug(runtime) << sql;

    QSqlQuery query;
    if ( !query.prepare(sql) )
    {
        qCWarning(runtime) << "Failed to prepare export query:" << query.lastError().text();
        QMessageBox::critical(this, tr("QLog Error"), tr("Failed to prepare export query."));
        return;
    }
    bindWhereClause(query);

    if ( !query.exec() )
    {
        qCWarning(runtime) << "Failed to execute export query:" << query.lastError().text();
        QMessageBox::critical(this, tr("QLog Error"), tr("Failed to query QSOs for export."));
        return;
    }

    QList<QSqlRecord> records;
    while ( query.next() )
        records.append(query.record());

    QFile file(ui->fileEdit->text());
    if ( !file.open(QIODevice::WriteOnly | QIODevice::Text) )
    {
        QMessageBox::critical(this, tr("QLog Error"),
                              tr("Cannot open file %1 for writing.").arg(ui->fileEdit->text()));
        return;
    }

    QTextStream stream(&file);
    CabrilloFormat format(stream);
    format.setTemplateId(ui->templateCombo->currentData().toInt());
    format.setHeaderData(buildHeaderData());
    format.setTransmitterId(ui->transmitterIdSpin->value());

    QProgressDialog progress(tr("Exporting Cabrillo..."), QString(), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    connect(&format, &CabrilloFormat::exportProgress, this, [&progress](float pct) {
        progress.setValue(static_cast<int>(pct));
    });

    long count = format.runExport(records);

    file.close();
    progress.close();

    QMessageBox::information(this, tr("QLog Information"),
                             tr("Exported %n QSO(s) to Cabrillo file.", "", count));

    QDialog::accept();
}
