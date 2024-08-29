#include <QFileDialog>
#include <QMessageBox>
#include "ImportDialog.h"
#include "ui_ImportDialog.h"
#include "logformat/LogFormat.h"
#include "core/debug.h"
#include "data/StationProfile.h"
#include "core/Gridsquare.h"
#include "data/RigProfile.h"
#include "data/Data.h"

MODULE_IDENTIFICATION("qlog.ui.importdialog");

ImportDialog::ImportDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ImportDialog)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->allCheckBox->setChecked(true);
    ui->startDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->startDateEdit->setDate(QDate::currentDate());
    ui->endDateEdit->setDate(QDate::currentDate());
    ui->endDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->progressBar->setValue(0);
    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(100);
    //ui->progressBar->setDisabled(true);

    QStringList rigs = RigProfilesManager::instance()->profileNameList();
    QStringListModel* rigModel = new QStringListModel(rigs, this);
    ui->rigSelect->setModel(rigModel);
    if (!ui->rigSelect->currentText().isEmpty())
    {
        ui->rigCheckBox->setChecked(true);
        ui->rigSelect->setCurrentText(RigProfilesManager::instance()->getCurProfile1().profileName);
    }

    QStringList profiles = StationProfilesManager::instance()->profileNameList();
    QStringListModel* profileModel = new QStringListModel(profiles, this);
    ui->profileSelect->setModel(profileModel);
    if (!ui->profileSelect->currentText().isEmpty())
    {
        ui->profileSelect->setCurrentText(StationProfilesManager::instance()->getCurProfile1().profileName);
        ui->profileCheckBox->setChecked(true);
    }

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&Import"));
}

void ImportDialog::browse()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    const QString &lastPath = ( ui->fileEdit->text().isEmpty() ) ? settings.value("import/last_path", QDir::homePath()).toString()
                                                                 : ui->fileEdit->text();

    QString filename = QFileDialog::getOpenFileName(this, tr("Select File"),
                                                    lastPath,
                                                    ui->typeSelect->currentText().toUpper() + "(*." + ui->typeSelect->currentText().toLower() + ")",
                                                    nullptr,
#if defined(Q_OS_LINUX)
                                                    // Do not use the Native Dialog under Linux because the dialog is case-sensitive.
                                                    // QT variant looks different but it is case-insensitive.
                                                    // More information:
                                                    // https://stackoverflow.com/questions/34858220/qt-how-to-set-a-case-insensitive-filter-on-qfiledialog
                                                    // https://bugreports.qt.io/browse/QTBUG-51712
                                                    QFileDialog::DontUseNativeDialog
#else
                                                    QFileDialog::Options()
#endif

                                                    );
    if ( !filename.isEmpty() )
    {
        settings.setValue("import/last_path", QFileInfo(filename).path());
        ui->fileEdit->setText(filename);
    }
}

void ImportDialog::toggleAll() {
    FCT_IDENTIFICATION;

    ui->startDateEdit->setEnabled(!ui->allCheckBox->isChecked());
    ui->endDateEdit->setEnabled(!ui->allCheckBox->isChecked());
}

void ImportDialog::toggleMyProfile()
{
    FCT_IDENTIFICATION;

    ui->profileSelect->setEnabled(ui->profileCheckBox->isChecked());
}
void ImportDialog::toggleMyRig()
{
    FCT_IDENTIFICATION;

    ui->rigSelect->setEnabled(ui->rigCheckBox->isChecked());
}

void ImportDialog::commentChanged(const QString &comment)
{
    QString toolTip = tr("The value is used when an input record does not contain the ADIF value") + "<br/>"
            + "<b>" + tr("Comment") + ":</b> " + comment + "<br/>";

    ui->commentEdit->setToolTip(toolTip);
    ui->commentCheckBox->setToolTip(toolTip);
}

void ImportDialog::toggleComment()
{
    FCT_IDENTIFICATION;

    ui->commentEdit->setEnabled(ui->commentCheckBox->isChecked());
    commentChanged(ui->commentEdit->text());
}

void ImportDialog::computeProgress(qint64 position)
{
    FCT_IDENTIFICATION;

    int progress = (int)(position * 100 / size);
    ui->progressBar->setValue(progress);
    QCoreApplication::processEvents();
}

void ImportDialog::stationProfileTextChanged(const QString &newProfileName)
{
    FCT_IDENTIFICATION;

    selectedStationProfile = StationProfilesManager::instance()->getProfile(newProfileName);
    QString toolTip = tr("The values below will be used when an input record does not contain the ADIF values") + "<br/>"
                         + selectedStationProfile.toHTMLString();
    ui->profileSelect->setToolTip(toolTip);
    ui->profileCheckBox->setToolTip(toolTip);
}

void ImportDialog::rigProfileTextChanged(const QString &newProfileName)
{
    FCT_IDENTIFICATION;
    QString toolTip = tr("The values below will be used when an input record does not contain the ADIF values") + "<br/>"
            + RigProfilesManager::instance()->getProfile(newProfileName).toHTMLString();
    ui->rigSelect->setToolTip(toolTip);
    ui->rigCheckBox->setToolTip(toolTip);
}

LogFormat::duplicateQSOBehaviour ImportDialog::showDuplicateDialog(QSqlRecord *imported, QSqlRecord *original)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << *imported << " " << *original;

    LogFormat::duplicateQSOBehaviour ret = LogFormat::ACCEPT_ONE;

    QMessageBox::StandardButton reply;

    QString inLogQSO = tr("<p><b>In-Log QSO:</b></p><p>")
                       + original->value("start_time").toString() + " "
                       + original->value("callsign").toString() + "</p>";

    QString importedQSO = tr("<p><b>Importing:</b></p><p>")
                       + imported->value("start_time").toString() + " "
                       + imported->value("callsign").toString() + "<p> ";

    reply = QMessageBox::question(nullptr,
                                  tr("Duplicate QSO"),
                                  tr("<p>Do you want to import duplicate QSO?</p>%1 %2").arg(inLogQSO,importedQSO),
                                  QMessageBox::Yes|QMessageBox::No|QMessageBox::YesAll|QMessageBox::NoAll);
    switch ( reply )
    {
    case QMessageBox::Yes:
        ret = LogFormat::ACCEPT_ONE;
        break;
    case QMessageBox::YesAll:
        ret = LogFormat::ACCEPT_ALL;
        break;
    case QMessageBox::No:
        ret = LogFormat::SKIP_ONE;
        break;
    case QMessageBox::NoAll:
        ret = LogFormat::SKIP_ALL;
        break;
    default:
        ret = LogFormat::ASK_NEXT;
    }

    qCDebug(runtime) << "ret: " << ret;
    return ret;
}

void ImportDialog::saveImportDetails(const QString &importDetail, const QString &filename,
                                     const int count, const unsigned long warnings, const unsigned long errors)
{
    FCT_IDENTIFICATION;

    QSettings settings;

    const QString &lastPath = settings.value("import/last_path_importdetails", QDir::homePath()).toString();

    QString filePath = QFileDialog::getSaveFileName(this, tr("Save to File"),
                                                    lastPath,
                                                    "TXT (*.txt)");

    if ( !filePath.isEmpty() )
    {

        QFile file(filePath);
        if ( file.open(QIODevice::WriteOnly | QIODevice::Text) )
        {
            const QDateTime &currTime = QDateTime::currentDateTimeUtc();

            QTextStream out(&file);
            out << tr("QLog Import Summary") << "\n"
                << "\n"
                << tr("Import date") << ": " << currTime.toString(locale.formatDateShortWithYYYY()) << " " << currTime.toString(locale.formatTimeLongWithoutTZ()) << " UTC\n"
                << tr("Imported file") << ": " << filename
                << "\n\n"
                << tr("Imported: %n contact(s)", "", count) << "\n"
                << tr("Warning(s): %n", "", warnings) << "\n"
                << tr("Error(s): %n", "", errors) << "\n"
                << "\n"
                << tr("Details") << ":\n"
                << importDetail;

            file.close();
            settings.setValue("import/last_path_importdetails", QFileInfo(filePath).path());
        }
    }
}

void ImportDialog::runImport() {
    FCT_IDENTIFICATION;

    if ( ui->fileEdit->text().isEmpty() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Filename is empty"));
        return;
    }

    QFile file(ui->fileEdit->text());
    file.open(QFile::ReadOnly | QFile::Text);
    QTextStream in(&file);

    size = file.size();

    QMap<QString, QString> defaults;

    if (ui->rigCheckBox->isChecked())
    {
        defaults["my_rig_intl"] = ui->rigSelect->currentText();
    }

    if (ui->commentCheckBox->isChecked())
    {
        defaults["comment_intl"] = ui->commentEdit->text();
    }

    if ( ui->profileCheckBox->isChecked()
         && selectedStationProfile != StationProfile() )
    {
        if ( !selectedStationProfile.callsign.isEmpty() )
        {
            defaults["station_callsign"] = selectedStationProfile.callsign.toUpper();
        }

        if ( !selectedStationProfile.locator.isEmpty() )
        {
            defaults["my_gridsquare"] = selectedStationProfile.locator.toUpper();
        }

        if ( !selectedStationProfile.operatorName.isEmpty() )
        {
            defaults["my_name_intl"] = selectedStationProfile.operatorName;
        }

        if ( !selectedStationProfile.qthName.isEmpty() )
        {
            defaults["my_city_intl"] = selectedStationProfile.qthName;
        }

        if ( !selectedStationProfile.iota.isEmpty() )
        {
            defaults["my_iota"] = Data::removeAccents(selectedStationProfile.iota.toUpper());
        }

        if ( !selectedStationProfile.sota.isEmpty() )
        {
            defaults["my_sota_ref"] = Data::removeAccents(selectedStationProfile.sota.toUpper());
        }

        if ( !selectedStationProfile.sig.isEmpty() )
        {
            defaults["my_sig_intl"] = Data::removeAccents(selectedStationProfile.sig);
        }

        if ( !selectedStationProfile.sigInfo.isEmpty() )
        {
            defaults["my_sig_info_intl"] = selectedStationProfile.sigInfo;
        }

        if ( !selectedStationProfile.vucc.isEmpty() )
        {
            defaults["my_vucc_grids"] = selectedStationProfile.vucc.toUpper();
        }

        if ( !selectedStationProfile.vucc.isEmpty() )
        {
            defaults["my_wwff_ref"] = Data::removeAccents(selectedStationProfile.vucc.toUpper());
        }

        if ( !selectedStationProfile.pota.isEmpty() )
        {
            defaults["my_pota_ref"] = Data::removeAccents(selectedStationProfile.pota.toUpper());
        }

        defaults["my_dxcc"] = QString::number(selectedStationProfile.dxcc);
        defaults["my_itu_zone"] = QString::number(selectedStationProfile.ituz);
        defaults["my_cq_zone"] = QString::number(selectedStationProfile.cqz);
        defaults["my_country_intl"] = selectedStationProfile.country;
    }

    LogFormat* format = LogFormat::open(ui->typeSelect->currentText(), in);

    if (!format) {
        qCritical() << "unknown log format";
        return;
    }

    format->setDefaults(defaults);
    format->setUpdateDxcc(ui->updateDxccCheckBox->isChecked());

    if (!ui->allCheckBox->isChecked()) {
        format->setFilterDateRange(ui->startDateEdit->date(), ui->endDateEdit->date());
    }

    format->setDuplicateQSOCallback(showDuplicateDialog);

    connect(format, &LogFormat::importPosition, this, &ImportDialog::computeProgress);

    ui->buttonBox->setEnabled(false);
    ui->fileEdit->setEnabled(false);
    ui->typeSelect->setEnabled(false);
    ui->browseButton->setEnabled(false);
    ui->startDateEdit->setEnabled(false);
    ui->endDateEdit->setEnabled(false);
    ui->allCheckBox->setEnabled(false);
    ui->profileCheckBox->setEnabled(false);
    ui->profileSelect->setEnabled(false);
    ui->rigCheckBox->setEnabled(false);
    ui->rigSelect->setEnabled(false);
    ui->commentCheckBox->setEnabled(false);
    ui->commentEdit->setEnabled(false);
    ui->updateDxccCheckBox->setEnabled(false);

    QString s;
    QTextStream out(&s);
    unsigned long errors = 0L;
    unsigned long warnings = 0L;

    int count = format->runImport(out, &warnings, &errors);

    QString report = QObject::tr("<b>Imported</b>: %n contact(s)", "", count) + "<br/>" +
                     QObject::tr("<b>Warning(s)</b>: %n", "", warnings) + "<br/>" +
                     QObject::tr("<b>Error(s)</b>: %n", "", errors);

    QMessageBox msgBox;
    QAbstractButton* pButtonYes = nullptr;

    msgBox.setWindowTitle(tr("Import Result"));
    msgBox.setText(report);
    msgBox.setDetailedText(s);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);

    if ( !s.isEmpty() )
    {
         pButtonYes = msgBox.addButton(tr("Save Details..."), QMessageBox::ActionRole);
    }

    QSpacerItem* horizontalSpacer = new QSpacerItem(500, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QGridLayout* layout = (QGridLayout*)msgBox.layout();
    layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());

    msgBox.exec();

    if ( pButtonYes
         && msgBox.clickedButton() == pButtonYes )
    {
        saveImportDetails(s, ui->fileEdit->text(),
                          count, warnings, errors);
    }

    qCDebug(runtime).noquote() << s;

    accept();
}

ImportDialog::~ImportDialog()
{
    FCT_IDENTIFICATION;

    delete ui;
}
