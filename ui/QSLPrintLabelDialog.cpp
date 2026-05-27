#include "ui/QSLPrintLabelDialog.h"
#include "ui_QSLPrintLabelDialog.h"

#include <QDir>
#include <QColorDialog>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QMessageBox>
#include <QPixmap>
#include <QPrintDialog>
#include <QPrinter>
#include <QIODevice>
#include <QSettings>
#include <QSignalBlocker>
#include <QSqlError>
#include <QSqlQuery>
#include <QScroller>
#include <QWheelEvent>

#include "core/debug.h"
#include "core/LogParam.h"
#include "core/QSOFilterManager.h"
#include "data/StationProfile.h"
#include "models/SqlListModel.h"
#include "data/Data.h"
#include "ui/component/LogbookFieldComboBox.h"

MODULE_IDENTIFICATION("qlog.ui.qslprintlabeldialog");

QSLPrintLabelDialog::QSLPrintLabelDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QSLPrintLabelDialog)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    const QList<QWidget*> widgets = findChildren<QWidget*>();

    for ( QWidget *w : widgets )
        w->blockSignals(true);

    /* Sections start collapsed via maxHeight=0 instead of setVisible(false)
     * so that their width still contributes to the scroll area's sizeHint */
    ui->templateSectionContent->setMaximumHeight(0);
    ui->printOptionsSectionContent->setMaximumHeight(0);
    ui->cardSectionContent->setMaximumHeight(0);

    ui->printModeComboBox->addItem(tr("Label Sheet"), static_cast<int>(QSLPrintMode::LabelSheet));
    ui->printModeComboBox->addItem(tr("QSL Card"), static_cast<int>(QSLPrintMode::DirectCard));
    ui->printPageSizeComboBox->addItem("A4", static_cast<int>(QPageSize::A4));
    ui->printPageSizeComboBox->addItem("Letter", static_cast<int>(QPageSize::Letter));

    /* Populate template combo: predefined + Custom */
    const QList<LabelTemplate> templates = QSLPrintLabelRenderer::predefinedTemplates();
    for ( const LabelTemplate &tmpl : templates )
        ui->templateComboBox->addItem(tmpl.name);
    ui->templateComboBox->addItem(tr("Custom"));

    /* Populate page size combo */
    ui->pageSizeComboBox->addItem("A4", static_cast<int>(QPageSize::A4));
    ui->pageSizeComboBox->addItem("Letter", static_cast<int>(QPageSize::Letter));
    ui->pageSizeLabel->setVisible(false);
    ui->pageSizeComboBox->setVisible(false);

    /* Populate callsign combo */
    ui->myCallsignComboBox->setModel(new SqlListModel("SELECT DISTINCT UPPER(station_callsign) FROM contacts ORDER BY station_callsign",
                                                       "",
                                                       ui->myCallsignComboBox));
    ui->myCallsignComboBox->setCurrentText(StationProfilesManager::instance()->getCurProfile1().callsign.toUpper());

    /* Populate station profile combo */
    ui->stationProfileComboBox->addItems(StationProfilesManager::instance()->profileNameList());
    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();
    int profileIndex = ui->stationProfileComboBox->findText(profile.profileName);
    ui->stationProfileComboBox->setCurrentIndex( ( profileIndex >= 0 ) ? profileIndex : -1 );

    /* Populate QSL sent combo */
    populateQSLSentCombo();

    /* Populate user filter combo */
    ui->userFilterComboBox->setModel(QSOFilterManager::QSOFilterModel("", ui->userFilterComboBox));
    ui->userFilterCheckBox->setEnabled(ui->userFilterComboBox->count() > 0);

    /* Mono font combo: show only monospaced fonts */
    ui->monoFontComboBox->setFontFilters(QFontComboBox::MonospacedFonts);

    /* Populate extra column combobox from contacts table columns */
    populateExtraColumnCombo();

    /* Date edits */
    ui->startDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->startDateEdit->setDate(QDate::currentDate().addYears(-1));
    ui->endDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->endDateEdit->setDate(QDate::currentDate());

    /* Section collapse/expand -- toggle maxHeight and arrow direction.
     * Using maxHeight instead of setVisible keeps the widget in layout
     * calculations so the scroll area's width is always correct. */
    connect(ui->filterSectionButton, &QToolButton::toggled, this, [this](bool checked)
    {
        ui->filterSectionButton->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        ui->filterSectionContent->setMaximumHeight(checked ? QWIDGETSIZE_MAX : 0);
    });
    connect(ui->templateSectionButton, &QToolButton::toggled, this, [this](bool checked)
    {
        ui->templateSectionButton->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        ui->templateSectionContent->setMaximumHeight(checked ? QWIDGETSIZE_MAX : 0);
    });
    connect(ui->cardSectionButton, &QToolButton::toggled, this, [this](bool checked)
    {
        ui->cardSectionButton->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        ui->cardSectionContent->setMaximumHeight(checked ? QWIDGETSIZE_MAX : 0);
    });
    connect(ui->printOptionsSectionButton, &QToolButton::toggled, this, [this](bool checked)
    {
        ui->printOptionsSectionButton->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        ui->printOptionsSectionContent->setMaximumHeight(checked ? QWIDGETSIZE_MAX : 0);
    });

    /* Load persisted settings and refresh data BEFORE connecting signals
     * to avoid triggering a refresh storm during initialization */
    loadSettings();

    for ( QWidget *w : widgets )
        w->blockSignals(false);

    QTimer::singleShot(0, this, &QSLPrintLabelDialog::refreshData);

    QScroller::grabGesture(ui->previewScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    ui->previewScrollArea->viewport()->installEventFilter(this);

    connect(ui->exportImagesButton, &QPushButton::clicked, this, &QSLPrintLabelDialog::exportCardImages);
}

QSLPrintLabelDialog::~QSLPrintLabelDialog()
{
    FCT_IDENTIFICATION;

    saveSettings();
    delete ui;
}

void QSLPrintLabelDialog::loadSettings()
{
    FCT_IDENTIFICATION;

    int templateIndex = LogParam::getQslLabelTemplate();

    if ( templateIndex >= 0 && templateIndex < ui->templateComboBox->count() )
        ui->templateComboBox->setCurrentIndex(templateIndex);

    ui->footerLeftEdit->setText(LogParam::getQslLabelFooterLeft());
    ui->footerRightEdit->setText(LogParam::getQslLabelFooterRight());
    ui->skipSpinBox->setValue(LogParam::getQslLabelSkip());

    zoomPercent = LogParam::getQslLabelZoom();
    ui->zoomSlider->setValue(zoomPercent);
    ui->zoomSpinBox->setValue(zoomPercent);

    int printModeIdx = ui->printModeComboBox->findData(LogParam::getQslLabelPrintMode());
    ui->printModeComboBox->setCurrentIndex(printModeIdx >= 0 ? printModeIdx : 0);
    int outputPageSizeIdx = ui->printPageSizeComboBox->findData(LogParam::getQslLabelPageSize());
    ui->printPageSizeComboBox->setCurrentIndex(outputPageSizeIdx >= 0 ? outputPageSizeIdx : 0);

    ui->printBordersCheckBox->setChecked(LogParam::getQslLabelPrintBorders());

    ui->dateFormatEdit->setText(LogParam::getQslLabelDateFormat());

    const QString savedSansFont = LogParam::getQslLabelSansFont();
    if ( !savedSansFont.isEmpty() )
        ui->sansFontComboBox->setCurrentFont(QFont(savedSansFont));

    const QString savedMonoFont = LogParam::getQslLabelMonoFont();
    if ( !savedMonoFont.isEmpty() )
        ui->monoFontComboBox->setCurrentFont(QFont(savedMonoFont));
    labelTextColor = LogParam::getQslLabelTextColor();
    updateLabelTextColorUi();

    /* extraColumnComboBox is populated in constructor before loadSettings() is called */
    const QString savedExtraCol = LogParam::getQslLabelExtraColumn();
    int extraIdx = ui->extraColumnComboBox->findData(savedExtraCol);
    ui->extraColumnComboBox->setCurrentIndex(extraIdx >= 0 ? extraIdx : 0);
    ui->columnHeaderEdit->setText(LogParam::getQslLabelExtraColumnHeader());
    ui->toRadioTextEdit->setText(LogParam::getQslLabelToRadioText());
    ui->hdrDateEdit->setText(LogParam::getQslLabelHdrDate());
    ui->hdrTimeEdit->setText(LogParam::getQslLabelHdrTime());
    ui->hdrBandEdit->setText(LogParam::getQslLabelHdrBand());
    ui->hdrModeEdit->setText(LogParam::getQslLabelHdrMode());
    ui->hdrQslEdit->setText(LogParam::getQslLabelHdrQsl());
    ui->maxRowsSpinBox->setValue(LogParam::getQslLabelMaxRows());

    ui->toRadioSizeSpinBox->setValue(LogParam::getQslLabelFontSizeToRadio());
    ui->callsignSizeSpinBox->setValue(LogParam::getQslLabelFontSizeCallsign());
    ui->headerSizeSpinBox->setValue(LogParam::getQslLabelFontSizeHeader());
    ui->dataSizeSpinBox->setValue(LogParam::getQslLabelFontSizeData());

    /* Load custom template fields from LogParam */
    int pageSizeIdx = LogParam::getQslLabelCustomPageSize();
    if ( pageSizeIdx >= 0 && pageSizeIdx < ui->pageSizeComboBox->count() )
        ui->pageSizeComboBox->setCurrentIndex(pageSizeIdx);
    ui->colsSpinBox->setValue(LogParam::getQslLabelCustomCols());
    ui->rowsSpinBox->setValue(LogParam::getQslLabelCustomRows());
    ui->labelWidthSpinBox->setValue(LogParam::getQslLabelCustomLabelWidth());
    ui->labelHeightSpinBox->setValue(LogParam::getQslLabelCustomLabelHeight());
    ui->leftMarginSpinBox->setValue(LogParam::getQslLabelCustomLeftMargin());
    ui->topMarginSpinBox->setValue(LogParam::getQslLabelCustomTopMargin());
    ui->hSpacingSpinBox->setValue(LogParam::getQslLabelCustomHSpacing());
    ui->vSpacingSpinBox->setValue(LogParam::getQslLabelCustomVSpacing());

    ui->cardWidthSpinBox->setValue(LogParam::getQslLabelCardWidth());
    ui->cardHeightSpinBox->setValue(LogParam::getQslLabelCardHeight());
    ui->cardGapSpinBox->setValue(LogParam::getQslLabelCardGap());
    ui->cardLabelWidthSpinBox->setValue(LogParam::getQslLabelCardLabelWidth());
    ui->cardLabelHeightSpinBox->setValue(LogParam::getQslLabelCardLabelHeight());
    ui->cardLabelOffsetXSpinBox->setValue(LogParam::getQslLabelCardLabelOffsetX());
    ui->cardLabelOffsetYSpinBox->setValue(LogParam::getQslLabelCardLabelOffsetY());
    ui->cardLabelOpaqueBackgroundCheckBox->setChecked(LogParam::getQslLabelCardLabelOpaqueBackground());
    cardLabelBackgroundColor = LogParam::getQslLabelCardLabelBackgroundColor();
    updateCardLabelBackgroundColorUi();
    cardBackgroundImageData = LogParam::getQslLabelCardBackgroundImage();
    updateCardBackgroundUi();

    /* Apply template fields state based on selected template */
    const QList<LabelTemplate> templates = QSLPrintLabelRenderer::predefinedTemplates();
    if ( templateIndex >= 0 && templateIndex < templates.size() )
    {
        populateTemplateFields(templates.at(templateIndex));
        setTemplateFieldsEnabled(false);
    }
    else
    {
        setTemplateFieldsEnabled(true);
    }

    updatePrintModeUi();
}

void QSLPrintLabelDialog::saveSettings()
{
    FCT_IDENTIFICATION;

    LogParam::setQslLabelTemplate(ui->templateComboBox->currentIndex());
    LogParam::setQslLabelFooterLeft(ui->footerLeftEdit->text());
    LogParam::setQslLabelFooterRight(ui->footerRightEdit->text());
    LogParam::setQslLabelSkip(ui->skipSpinBox->value());
    LogParam::setQslLabelZoom(zoomPercent);
    LogParam::setQslLabelPrintMode(ui->printModeComboBox->currentData().toInt());
    LogParam::setQslLabelPageSize(static_cast<int>(currentOutputPageSize()));
    LogParam::setQslLabelPrintBorders(ui->printBordersCheckBox->isChecked());
    LogParam::setQslLabelDateFormat(ui->dateFormatEdit->text());
    LogParam::setQslLabelSansFont(ui->sansFontComboBox->currentFont().family());
    LogParam::setQslLabelMonoFont(ui->monoFontComboBox->currentFont().family());
    LogParam::setQslLabelTextColor(labelTextColor);
    LogParam::setQslLabelExtraColumn(ui->extraColumnComboBox->currentData().toString());
    LogParam::setQslLabelExtraColumnHeader(ui->columnHeaderEdit->text());
    LogParam::setQslLabelToRadioText(ui->toRadioTextEdit->text());
    LogParam::setQslLabelHdrDate(ui->hdrDateEdit->text());
    LogParam::setQslLabelHdrTime(ui->hdrTimeEdit->text());
    LogParam::setQslLabelHdrBand(ui->hdrBandEdit->text());
    LogParam::setQslLabelHdrMode(ui->hdrModeEdit->text());
    LogParam::setQslLabelHdrQsl(ui->hdrQslEdit->text());
    LogParam::setQslLabelMaxRows(ui->maxRowsSpinBox->value());
    LogParam::setQslLabelFontSizeToRadio(ui->toRadioSizeSpinBox->value());
    LogParam::setQslLabelFontSizeCallsign(ui->callsignSizeSpinBox->value());
    LogParam::setQslLabelFontSizeHeader(ui->headerSizeSpinBox->value());
    LogParam::setQslLabelFontSizeData(ui->dataSizeSpinBox->value());

    /* Save custom template fields */
    LogParam::setQslLabelCustomPageSize(ui->pageSizeComboBox->currentIndex());
    LogParam::setQslLabelCustomCols(ui->colsSpinBox->value());
    LogParam::setQslLabelCustomRows(ui->rowsSpinBox->value());
    LogParam::setQslLabelCustomLabelWidth(ui->labelWidthSpinBox->value());
    LogParam::setQslLabelCustomLabelHeight(ui->labelHeightSpinBox->value());
    LogParam::setQslLabelCustomLeftMargin(ui->leftMarginSpinBox->value());
    LogParam::setQslLabelCustomTopMargin(ui->topMarginSpinBox->value());
    LogParam::setQslLabelCustomHSpacing(ui->hSpacingSpinBox->value());
    LogParam::setQslLabelCustomVSpacing(ui->vSpacingSpinBox->value());
    LogParam::setQslLabelCardWidth(ui->cardWidthSpinBox->value());
    LogParam::setQslLabelCardHeight(ui->cardHeightSpinBox->value());
    LogParam::setQslLabelCardGap(ui->cardGapSpinBox->value());
    LogParam::setQslLabelCardLabelWidth(ui->cardLabelWidthSpinBox->value());
    LogParam::setQslLabelCardLabelHeight(ui->cardLabelHeightSpinBox->value());
    LogParam::setQslLabelCardLabelOffsetX(ui->cardLabelOffsetXSpinBox->value());
    LogParam::setQslLabelCardLabelOffsetY(ui->cardLabelOffsetYSpinBox->value());
    LogParam::setQslLabelCardLabelOpaqueBackground(ui->cardLabelOpaqueBackgroundCheckBox->isChecked());
    LogParam::setQslLabelCardLabelBackgroundColor(cardLabelBackgroundColor);
    LogParam::setQslLabelCardBackgroundImage(cardBackgroundImageData);
}

void QSLPrintLabelDialog::populateTemplateFields(const LabelTemplate &tmpl)
{
    FCT_IDENTIFICATION;

    const QSignalBlocker pageSizeBlocker(ui->pageSizeComboBox);
    const QSignalBlocker colsBlocker(ui->colsSpinBox);
    const QSignalBlocker rowsBlocker(ui->rowsSpinBox);
    const QSignalBlocker labelWidthBlocker(ui->labelWidthSpinBox);
    const QSignalBlocker labelHeightBlocker(ui->labelHeightSpinBox);
    const QSignalBlocker leftMarginBlocker(ui->leftMarginSpinBox);
    const QSignalBlocker topMarginBlocker(ui->topMarginSpinBox);
    const QSignalBlocker hSpacingBlocker(ui->hSpacingSpinBox);
    const QSignalBlocker vSpacingBlocker(ui->vSpacingSpinBox);

    int pageSizeIdx = ui->pageSizeComboBox->findData(static_cast<int>(tmpl.pageSize));
    if ( pageSizeIdx >= 0 )
        ui->pageSizeComboBox->setCurrentIndex(pageSizeIdx);

    ui->colsSpinBox->setValue(tmpl.cols);
    ui->rowsSpinBox->setValue(tmpl.rows);
    ui->labelWidthSpinBox->setValue(tmpl.labelWidthMm);
    ui->labelHeightSpinBox->setValue(tmpl.labelHeightMm);
    ui->leftMarginSpinBox->setValue(tmpl.leftMarginMm);
    ui->topMarginSpinBox->setValue(tmpl.topMarginMm);
    ui->hSpacingSpinBox->setValue(tmpl.hSpacingMm);
    ui->vSpacingSpinBox->setValue(tmpl.vSpacingMm);
}

void QSLPrintLabelDialog::setTemplateFieldsEnabled(bool enabled)
{
    FCT_IDENTIFICATION;

    ui->pageSizeComboBox->setEnabled(enabled);
    ui->colsSpinBox->setEnabled(enabled);
    ui->rowsSpinBox->setEnabled(enabled);
    ui->labelWidthSpinBox->setEnabled(enabled);
    ui->labelHeightSpinBox->setEnabled(enabled);
    ui->leftMarginSpinBox->setEnabled(enabled);
    ui->topMarginSpinBox->setEnabled(enabled);
    ui->hSpacingSpinBox->setEnabled(enabled);
    ui->vSpacingSpinBox->setEnabled(enabled);
}

LabelTemplate QSLPrintLabelDialog::buildCustomTemplate() const
{
    FCT_IDENTIFICATION;

    LabelTemplate tmpl;
    tmpl.name = tr("Custom");
    tmpl.orientation = QPageLayout::Portrait;
    tmpl.pageSize = currentOutputPageSize();
    tmpl.cols = ui->colsSpinBox->value();
    tmpl.rows = ui->rowsSpinBox->value();
    tmpl.labelWidthMm = ui->labelWidthSpinBox->value();
    tmpl.labelHeightMm = ui->labelHeightSpinBox->value();
    tmpl.leftMarginMm = ui->leftMarginSpinBox->value();
    tmpl.topMarginMm = ui->topMarginSpinBox->value();
    tmpl.hSpacingMm = ui->hSpacingSpinBox->value();
    tmpl.vSpacingMm = ui->vSpacingSpinBox->value();
    return tmpl;
}

LabelTemplate QSLPrintLabelDialog::currentLabelTemplate() const
{
    FCT_IDENTIFICATION;

    const QList<LabelTemplate> templates = QSLPrintLabelRenderer::predefinedTemplates();
    const int templateIndex = ui->templateComboBox->currentIndex();

    if ( templateIndex >= 0 && templateIndex < templates.size() )
        return templates.at(templateIndex);

    return buildCustomTemplate();
}

QSLPrintMode QSLPrintLabelDialog::currentPrintMode() const
{
    FCT_IDENTIFICATION;

    return static_cast<QSLPrintMode>(ui->printModeComboBox->currentData().toInt());
}

QPageSize::PageSizeId QSLPrintLabelDialog::currentOutputPageSize() const
{
    FCT_IDENTIFICATION;

    return static_cast<QPageSize::PageSizeId>(ui->printPageSizeComboBox->currentData().toInt());
}

QSLCardLayout QSLPrintLabelDialog::buildCardLayout() const
{
    FCT_IDENTIFICATION;

    QSLCardLayout layout;
    layout.cardWidthMm = ui->cardWidthSpinBox->value();
    layout.cardHeightMm = ui->cardHeightSpinBox->value();
    layout.cardGapMm = ui->cardGapSpinBox->value();
    layout.labelWidthMm = ui->cardLabelWidthSpinBox->value();
    layout.labelHeightMm = ui->cardLabelHeightSpinBox->value();
    layout.labelOffsetXMm = ui->cardLabelOffsetXSpinBox->value();
    layout.labelOffsetYMm = ui->cardLabelOffsetYSpinBox->value();
    layout.labelOpaqueBackground = ui->cardLabelOpaqueBackgroundCheckBox->isChecked();
    layout.labelBackgroundColor = cardLabelBackgroundColor;
    return layout;
}

LabelStyleOptions QSLPrintLabelDialog::buildStyleOptions() const
{
    FCT_IDENTIFICATION;

    LabelStyleOptions styleOpts;
    styleOpts.sansFontFamily = ui->sansFontComboBox->currentFont().family();
    styleOpts.monoFontFamily = ui->monoFontComboBox->currentFont().family();
    styleOpts.textColor = labelTextColor;
    styleOpts.toRadioFontSize = ui->toRadioSizeSpinBox->value();
    styleOpts.callsignFontSize = ui->callsignSizeSpinBox->value();
    styleOpts.headerFontSize = ui->headerSizeSpinBox->value();
    styleOpts.dataFontSize = ui->dataSizeSpinBox->value();

    const QString extraCol = ui->extraColumnComboBox->currentText();
    const QString customHeader = ui->columnHeaderEdit->text().trimmed();
    styleOpts.extraColumnHeader = (extraCol.isEmpty() || ui->extraColumnComboBox->currentIndex() == 0) ? QString()
                                  : (customHeader.isEmpty() ? extraCol : customHeader);
    styleOpts.maxQsoRows = ui->maxRowsSpinBox->value();

    const QString toRadioText = ui->toRadioTextEdit->text().trimmed();
    if ( !toRadioText.isEmpty() )
        styleOpts.toRadioText = toRadioText;

    const QString hdrDate = ui->hdrDateEdit->text().trimmed();
    if ( !hdrDate.isEmpty() )
        styleOpts.hdrDate = hdrDate;

    const QString hdrTime = ui->hdrTimeEdit->text().trimmed();
    if ( !hdrTime.isEmpty() )
        styleOpts.hdrTime = hdrTime;

    const QString hdrBand = ui->hdrBandEdit->text().trimmed();
    if ( !hdrBand.isEmpty() )
        styleOpts.hdrBand = hdrBand;

    const QString hdrMode = ui->hdrModeEdit->text().trimmed();
    if ( !hdrMode.isEmpty() )
        styleOpts.hdrMode = hdrMode;

    const QString hdrQsl = ui->hdrQslEdit->text().trimmed();
    if ( !hdrQsl.isEmpty() )
        styleOpts.hdrQsl = hdrQsl;

    return styleOpts;
}

QString QSLPrintLabelDialog::buttonContrastTextColor(const QColor &backgroundColor) const
{
    FCT_IDENTIFICATION;

    return backgroundColor.lightness() < 128 ? QStringLiteral("white") : QStringLiteral("black");
}

QString QSLPrintLabelDialog::imageExportFileName(const QSLLabelData &label, int index) const
{
    FCT_IDENTIFICATION;

    QString base = label.callsign.trimmed().toUpper();

    if ( base.isEmpty() )
        base = QStringLiteral("qsl_card");

    if ( !label.qsos.isEmpty() )
    {
        const QSLLabelData::QsoRow &firstQso = label.qsos.first();

        if ( !firstQso.date.isEmpty() )
            base += "_" + firstQso.date;
        if ( !firstQso.time.isEmpty() )
            base += "_" + firstQso.time;
    }

    QString sanitized;
    sanitized.reserve(base.size());

    for ( const QChar &ch : base )
    {
        if ( ch.isLetterOrNumber() || ch == '_' || ch == '-' )
            sanitized += ch;
        else if ( ch.isSpace() || ch == '/' || ch == ':' || ch == '.' )
            sanitized += '_';
    }

    while ( sanitized.contains("__") )
        sanitized.replace("__", "_");

    sanitized = sanitized.trimmed();

    if ( sanitized.startsWith("_") )
        sanitized.remove(0, 1);
    if ( sanitized.endsWith("_") )
        sanitized.chop(1);
    if ( sanitized.isEmpty() )
        sanitized = QStringLiteral("qsl_card");

    return QString("%1_%2.jpg").arg(sanitized, QString::number(index + 1).rightJustified(3, '0'));
}

void QSLPrintLabelDialog::updateRendererOptions()
{
    FCT_IDENTIFICATION;

    renderer.setPrintMode(currentPrintMode());
    renderer.setPageSize(currentOutputPageSize());
    renderer.setFooterLeft(ui->footerLeftEdit->text());
    renderer.setFooterRight(ui->footerRightEdit->text());
    renderer.setPrintBorders(ui->printBordersCheckBox->isChecked());
    renderer.setCardLayout(buildCardLayout());
    renderer.setCardBackgroundImage(cardBackgroundImageData);
    renderer.setStyleOptions(buildStyleOptions());
}

void QSLPrintLabelDialog::updatePrintModeUi()
{
    FCT_IDENTIFICATION;

    const bool directCard = currentPrintMode() == QSLPrintMode::DirectCard;

    ui->templateSectionButton->setEnabled(!directCard);
    ui->templateSectionContent->setEnabled(!directCard);
    ui->cardSectionButton->setEnabled(directCard);
    ui->cardSectionContent->setEnabled(directCard);
    ui->templateSectionButton->setChecked(!directCard);
    ui->cardSectionButton->setChecked(directCard);
    ui->exportImagesButton->setVisible(directCard);

    ui->templateSectionButton->setArrowType(ui->templateSectionButton->isChecked() ? Qt::DownArrow : Qt::RightArrow);
    ui->templateSectionContent->setMaximumHeight((!directCard && ui->templateSectionButton->isChecked()) ? QWIDGETSIZE_MAX : 0);
    ui->cardSectionButton->setArrowType(ui->cardSectionButton->isChecked() ? Qt::DownArrow : Qt::RightArrow);
    ui->cardSectionContent->setMaximumHeight((directCard && ui->cardSectionButton->isChecked()) ? QWIDGETSIZE_MAX : 0);
}

void QSLPrintLabelDialog::updateLabelTextColorUi()
{
    FCT_IDENTIFICATION;

    const QColor color = labelTextColor.isValid() ? labelTextColor : QColor(Qt::black);
    const QString buttonTextColor = buttonContrastTextColor(color);
    ui->labelTextColorButton->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                                            .arg(color.name(QColor::HexArgb), buttonTextColor));
}

void QSLPrintLabelDialog::updateCardLabelBackgroundColorUi()
{
    FCT_IDENTIFICATION;

    const QColor color = cardLabelBackgroundColor.isValid() ? cardLabelBackgroundColor : QColor(Qt::white);
    const QString buttonTextColor = buttonContrastTextColor(color);
    ui->cardLabelBackgroundColorButton->setEnabled(ui->cardLabelOpaqueBackgroundCheckBox->isChecked());
    ui->cardLabelBackgroundColorButton->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                                                      .arg(color.name(QColor::HexArgb), buttonTextColor));
}

void QSLPrintLabelDialog::updateCardBackgroundUi()
{
    FCT_IDENTIFICATION;

    const bool hasImage = !cardBackgroundImageData.isNull();
    ui->clearCardBackgroundButton->setEnabled(hasImage);
}

void QSLPrintLabelDialog::populateExtraColumnCombo()
{
    FCT_IDENTIFICATION;

    LogbookFieldComboBox::populateCombo(ui->extraColumnComboBox,
                                        LogbookFieldComboBox::ValueMode::DbFieldName,
                                        LogbookFieldComboBox::EmptyMode::EmptyLabel,
                                        tr("Empty"));
}

void QSLPrintLabelDialog::populateQSLSentCombo()
{
    FCT_IDENTIFICATION;
    QMapIterator<QString, QString> iter(Data::instance()->qslSentEnum);
    int iter_index = 0;
    int value_index = 0;
    while ( iter.hasNext() )
    {
        iter.next();
        ui->qslSentComboBox->addItem(iter.value(), iter.key());
        if ( iter.key() == "Q" ) // Queued by default
            value_index = iter_index;
        iter_index++;
    }
    ui->qslSentComboBox->setCurrentIndex(value_index);
}

void QSLPrintLabelDialog::toggleDateRange()
{
    FCT_IDENTIFICATION;

    ui->startDateEdit->setEnabled(ui->dateRangeCheckBox->isChecked());
    ui->endDateEdit->setEnabled(ui->dateRangeCheckBox->isChecked());
    refreshData();
}

void QSLPrintLabelDialog::toggleMyCallsign()
{
    FCT_IDENTIFICATION;

    ui->myCallsignComboBox->setEnabled(ui->myCallsignCheckBox->isChecked());
    if ( ui->myCallsignCheckBox->isChecked() )
        ui->stationProfileCheckBox->setChecked(false);
    refreshData();
}

void QSLPrintLabelDialog::toggleStationProfile()
{
    FCT_IDENTIFICATION;

    ui->stationProfileComboBox->setEnabled(ui->stationProfileCheckBox->isChecked());
    if ( ui->stationProfileCheckBox->isChecked() )
    {
        const StationProfile &selectedStationProfile = StationProfilesManager::instance()->getProfile(ui->stationProfileComboBox->currentText());
        const QString toolTip = tr("QSOs matching this station profile")
                                + "<br/>"
                                + selectedStationProfile.toHTMLString();
        ui->stationProfileComboBox->setToolTip(toolTip);

        ui->myCallsignCheckBox->setChecked(false);
    }
    else
        ui->stationProfileComboBox->setToolTip({});
    refreshData();
}

void QSLPrintLabelDialog::toggleQslSent()
{
    FCT_IDENTIFICATION;

    ui->qslSentComboBox->setEnabled(ui->qslSentCheckBox->isChecked());
    refreshData();
}

void QSLPrintLabelDialog::toggleUserFilter()
{
    FCT_IDENTIFICATION;

    ui->userFilterComboBox->setEnabled(ui->userFilterCheckBox->isChecked());
    refreshData();
}

void QSLPrintLabelDialog::templateChanged(int index)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << index;

    const QList<LabelTemplate> templates = QSLPrintLabelRenderer::predefinedTemplates();

    if ( index >= 0 && index < templates.size() )
    {
        /* Predefined template selected - show values, disable fields */
        const LabelTemplate &tmpl = templates.at(index);
        populateTemplateFields(tmpl);
        const int pageSizeIdx = ui->printPageSizeComboBox->findData(static_cast<int>(tmpl.pageSize));
        if ( pageSizeIdx >= 0 )
        {
            const QSignalBlocker pageSizeBlocker(ui->printPageSizeComboBox);
            ui->printPageSizeComboBox->setCurrentIndex(pageSizeIdx);
        }
        setTemplateFieldsEnabled(false);
    }
    else
    {
        /* Custom template - enable fields, load custom values from spinboxes */
        setTemplateFieldsEnabled(true);
    }

    refreshData();
}

void QSLPrintLabelDialog::skipChanged(int value)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << value;

    renderer.setSkipLabels(value);
    updatePreview();
}

void QSLPrintLabelDialog::zoomChanged(int value)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << value;

    zoomPercent = value;
    updatePreview();
}

void QSLPrintLabelDialog::customTemplateFieldChanged()
{
    FCT_IDENTIFICATION;

    /* Only respond if Custom template is selected */
    const QList<LabelTemplate> templates = QSLPrintLabelRenderer::predefinedTemplates();
    int index = ui->templateComboBox->currentIndex();

    if ( index >= 0 && index < templates.size() )
        return;

    refreshData();
}

void QSLPrintLabelDialog::printModeChanged(int index)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << index;

    updatePrintModeUi();
    refreshData();
}

void QSLPrintLabelDialog::cardLayoutChanged()
{
    FCT_IDENTIFICATION;

    updateCardLabelBackgroundColorUi();
    refreshData();
}

void QSLPrintLabelDialog::selectLabelTextColor()
{
    FCT_IDENTIFICATION;

    const QColor color = QColorDialog::getColor(labelTextColor.isValid() ? labelTextColor : QColor(Qt::black),
                                                this,
                                                tr("Select Label Text Color"),
                                                QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
    if ( !color.isValid() )
        return;

    labelTextColor = color;
    updateLabelTextColorUi();
    updatePreview();
}

void QSLPrintLabelDialog::selectCardLabelBackgroundColor()
{
    FCT_IDENTIFICATION;

    const QColor color = QColorDialog::getColor(cardLabelBackgroundColor.isValid() ? cardLabelBackgroundColor : QColor(Qt::white),
                                                this,
                                                tr("Select Label Background Color"),
                                                QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
    if ( !color.isValid() )
        return;

    cardLabelBackgroundColor = color;
    updateCardLabelBackgroundColorUi();
    updatePreview();
}

void QSLPrintLabelDialog::selectCardBackgroundImage()
{
    FCT_IDENTIFICATION;

    const QString filename = QFileDialog::getOpenFileName(this,
                                                          tr("Select QSL Card Background"),
                                                          QDir::homePath(),
                                                          tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if ( filename.isEmpty() )
        return;

    QFile file(filename);
    if ( !file.open(QIODevice::ReadOnly) )
    {
        QMessageBox::warning(this,
                             tr("Select QSL Card Background"),
                             tr("Cannot read selected image file."));
        return;
    }

    QImage image;
    if ( !image.loadFromData(file.readAll()) )
    {
        QMessageBox::warning(this,
                             tr("Select QSL Card Background"),
                             tr("Selected file is not a valid image."));
        return;
    }

    cardBackgroundImageData = image;
    updateCardBackgroundUi();
    updatePreview();
}

void QSLPrintLabelDialog::clearCardBackgroundImage()
{
    FCT_IDENTIFICATION;

    cardBackgroundImageData = QImage();
    updateCardBackgroundUi();
    updatePreview();
}

void QSLPrintLabelDialog::resizeEvent(QResizeEvent *event)
{
    FCT_IDENTIFICATION;

    QWidget::resizeEvent(event);
    updatePreview();
}

bool QSLPrintLabelDialog::eventFilter(QObject *obj, QEvent *event)
{
    if ( obj == ui->previewScrollArea->viewport() && event->type() == QEvent::Wheel )
    {
        QWheelEvent *wheelEvent = dynamic_cast<QWheelEvent *>(event);
        /*
         * CRTL + Mouse Wheel
         *
         * Zoom In/Out
         */
        if ( wheelEvent && wheelEvent->modifiers() & Qt::ControlModifier )
        {
            QPoint wheelDelta(wheelEvent->angleDelta());
            int delta = ( wheelDelta.y() > 0 ) ? 1 : -1;
            int newZoom = zoomPercent + delta * 5;
            ui->zoomSpinBox->setValue(newZoom);
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

QString QSLPrintLabelDialog::buildWhereClause() const
{
    FCT_IDENTIFICATION;

    QStringList where;
    where << "1 = 1";

    if ( ui->dateRangeCheckBox->isChecked() )
        where << "(datetime(start_time) BETWEEN datetime(:start_date) AND datetime(:end_date))";

    if ( ui->myCallsignCheckBox->isChecked() )
        where << "upper(station_callsign) = upper(:stationCallsign)";

    if ( ui->stationProfileCheckBox->isChecked() )
    {
        const StationProfile &stProfile = StationProfilesManager::instance()->getProfile(
            ui->stationProfileComboBox->currentText());

        where << QString("EXISTS ("
                         "SELECT 1 FROM station_profiles"
                         " WHERE station_profiles.profile_name = :stationProfileName"
                         " AND %1"
                         ")").arg(stProfile.getContactInnerJoin());
    }

    if ( ui->qslSentCheckBox->isChecked() )
        where << "upper(qsl_sent) = upper(:qslSent)";

    if ( ui->userFilterCheckBox->isChecked() )
    {
        QString filterClause = QSOFilterManager::getWhereClause(ui->userFilterComboBox->currentText());
        if ( !filterClause.isEmpty() )
            where << filterClause;
    }

    return where.join(" AND ");
}

void QSLPrintLabelDialog::bindWhereClause(QSqlQuery &query) const
{
    FCT_IDENTIFICATION;

    if ( ui->dateRangeCheckBox->isChecked() )
    {
        QDate startDate = ui->startDateEdit->date();
        QDate endDate = ui->endDateEdit->date();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        query.bindValue(":start_date", startDate.startOfDay());
        query.bindValue(":end_date", endDate.endOfDay());
#else
        query.bindValue(":start_date", QDateTime(startDate));
        query.bindValue(":end_date", QDateTime(endDate));
#endif
    }

    if ( ui->myCallsignCheckBox->isChecked() )
        query.bindValue(":stationCallsign", ui->myCallsignComboBox->currentText());

    if ( ui->stationProfileCheckBox->isChecked() )
        query.bindValue(":stationProfileName", ui->stationProfileComboBox->currentText());

    if ( ui->qslSentCheckBox->isChecked() )
        query.bindValue(":qslSent", ui->qslSentComboBox->currentData().toString());
}

void QSLPrintLabelDialog::buildLabels()
{
    FCT_IDENTIFICATION;

    labelsData.clear();

    const QString whereClause = buildWhereClause();

    const QString dateFormat = ui->dateFormatEdit->text().trimmed();
    const QString effectiveDateFormat = dateFormat.isEmpty() ? "yyyy-MM-dd" : dateFormat;
    const int maxRows = ui->maxRowsSpinBox->value();

    const QString extraCol = ui->extraColumnComboBox->currentData().toString();
    const QString extraColSelect = extraCol.isEmpty()
                                   ? QString()
                                   : QString(", \"%1\"").arg(extraCol);

    const QString sql = QString("SELECT start_time, callsign, band, mode, submode, "
                          "qsl_rcvd, lotw_qsl_rcvd, eqsl_qsl_rcvd, dcl_qsl_rcvd"
                          "%1 "
                          "FROM contacts "
                          "WHERE %2 "
                          "ORDER BY callsign COLLATE NOCASE ASC, start_time ASC")
                          .arg(extraColSelect, whereClause);

    QSqlQuery query;

    if ( !query.prepare(sql) )
    {
        qCWarning(runtime) << "Cannot prepare QSL label query" << query.lastError().text();
        return;
    }

    bindWhereClause(query);

    if ( !query.exec() )
    {
        qCWarning(runtime) << "Cannot execute QSL label query" << query.lastError().text();
        return;
    }

    QString currentCallsign;
    QList<QSLLabelData::QsoRow> currentRows;

    while ( query.next() )
    {
        const QDateTime startTime = query.value("start_time").toDateTime();
        const QString callsign = query.value("callsign").toString().toUpper();
        const QString band = query.value("band").toString().toUpper();
        const QString mode = query.value("mode").toString().toUpper();
        const QString submode = query.value("submode").toString().toUpper();

        /* Mode normalization:
         * - If submode is present and non-empty, use submode
         * - SSB with USB/LSB submode stays SSB
         * - Otherwise use mode */
        QString displayMode;
        if ( mode == "SSB"
             && ( submode == "USB" || submode == "LSB" ) )
        {
            displayMode = "SSB";
        }
        else if ( !submode.isEmpty() )
            displayMode = submode.toUpper();
        else
            displayMode = mode.toUpper();

        /* QSL logic: TNX if any *_rcvd == 'Y', else PSE */
        const QString qslRcvd = query.value("qsl_rcvd").toString().toUpper();
        const QString lotwRcvd = query.value("lotw_qsl_rcvd").toString().toUpper();
        const QString eqslRcvd = query.value("eqsl_qsl_rcvd").toString().toUpper();
        const QString dclRcvd = query.value("dcl_qsl_rcvd").toString().toUpper();

        const QString qslText = ( qslRcvd == "Y"
                                  || lotwRcvd == "Y"
                                  || eqslRcvd == "Y"
                                  || dclRcvd == "Y" ) ? "TNX" : "PSE";

        QSLLabelData::QsoRow row;
        row.date = startTime.toUTC().toString(effectiveDateFormat);
        row.time = startTime.toUTC().toString("HH:mm");
        row.band = band;
        row.mode = displayMode;
        row.qsl = qslText;
        row.extra = extraCol.isEmpty() ? QString() : query.value(extraCol).toString();

        /* Group by callsign, split into chunks of 4 QSOs per label */
        if ( callsign != currentCallsign )
        {
            /* Flush previous callsign's rows */
            while ( !currentRows.isEmpty() )
            {
                QSLLabelData label;
                label.callsign = currentCallsign;
                int count = qMin(maxRows, currentRows.size());
                for ( int i = 0; i < count; ++i )
                    label.qsos.append(currentRows.takeFirst());
                labelsData.append(label);
            }
            currentCallsign = callsign;
        }

        currentRows.append(row);

        /* If we have maxRows rows, emit a label */
        if ( currentRows.size() == maxRows )
        {
            QSLLabelData label;
            label.callsign = currentCallsign;
            for ( int i = 0; i < maxRows; ++i )
                label.qsos.append(currentRows.takeFirst());
            labelsData.append(label);
        }
    }

    /* Flush remaining rows for the last callsign */
    while ( !currentRows.isEmpty() )
    {
        QSLLabelData label;
        label.callsign = currentCallsign;
        int count = qMin(maxRows, currentRows.size());
        for ( int i = 0; i < count; ++i )
            label.qsos.append(currentRows.takeFirst());
        labelsData.append(label);
    }
}

void QSLPrintLabelDialog::refreshData()
{
    FCT_IDENTIFICATION;

    buildLabels();

    renderer.setTemplate(currentLabelTemplate());
    renderer.setLabels(labelsData);
    renderer.setSkipLabels(ui->skipSpinBox->value());
    currentPage = 0;

    updatePreview();
}

void QSLPrintLabelDialog::updatePreview()
{
    FCT_IDENTIFICATION;

    updateRendererOptions();

    int totalPages = renderer.pageCount();
    int totalLabels = renderer.labelCount();

    bool hasLabels = ( totalLabels > 0 );
    ui->printButton->setEnabled(hasLabels);
    ui->exportPdfButton->setEnabled(hasLabels);
    ui->exportImagesButton->setEnabled(hasLabels && currentPrintMode() == QSLPrintMode::DirectCard);

    if ( currentPrintMode() == QSLPrintMode::DirectCard )
        ui->statusLabel->setText(tr("Cards: %1 (%2 pages)")
                                     .arg(totalLabels)
                                     .arg(totalPages));
    else
        ui->statusLabel->setText(tr("Labels: %1 (%2 pages)")
                                     .arg(totalLabels)
                                     .arg(totalPages));

    updatePageNavigation();

    if ( totalPages > 0 && currentPage < totalPages )
    {
        QImage pageImage = renderer.renderPage(currentPage);

        /* Calculate scaled width: fit-to-viewport * zoom% */
        int fitWidth = ui->previewScrollArea->viewport()->width() - 10;

        if ( fitWidth > 0 )
        {
            int scaledWidth = fitWidth * zoomPercent / 100;
            QPixmap pixmap = QPixmap::fromImage(pageImage);
            ui->previewLabel->setPixmap(pixmap.scaledToWidth(scaledWidth, Qt::SmoothTransformation));
        }
        else
        {
            ui->previewLabel->setPixmap(QPixmap::fromImage(pageImage));
        }
        ui->previewLabel->setAlignment(Qt::AlignCenter);
    }
    else
    {
        ui->previewLabel->clear();
        ui->previewLabel->setText(tr("No matching QSOs found"));
        ui->previewLabel->setAlignment(Qt::AlignCenter);
    }
}

void QSLPrintLabelDialog::updatePageNavigation()
{
    FCT_IDENTIFICATION;

    int totalPages = renderer.pageCount();

    ui->prevPageButton->setEnabled(currentPage > 0);
    ui->nextPageButton->setEnabled(totalPages > 0 && currentPage < totalPages - 1);

    if ( totalPages > 0 )
        ui->pageLabel->setText(tr("Page %1 of %2").arg(currentPage + 1).arg(totalPages));
    else
        ui->pageLabel->setText(tr("Page 0 of 0"));
}

void QSLPrintLabelDialog::prevPage()
{
    FCT_IDENTIFICATION;

    if ( currentPage > 0 )
    {
        currentPage--;
        updatePreview();
    }
}

void QSLPrintLabelDialog::nextPage()
{
    FCT_IDENTIFICATION;

    int totalPages = renderer.pageCount();
    if ( totalPages > 0 && currentPage < totalPages - 1 )
    {
        currentPage++;
        updatePreview();
    }
}

void QSLPrintLabelDialog::print()
{
    FCT_IDENTIFICATION;

    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog printDialog(&printer, this);

    if ( printDialog.exec() == QDialog::Accepted )
    {
        updateRendererOptions();
        renderer.printAll(&printer);
        askAndMarkQslSent();
    }
}

void QSLPrintLabelDialog::exportPdf()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    const QString lastPath = settings.value("qsllabel/last_pdf_path", QDir::homePath()).toString();

    QString filename = QFileDialog::getSaveFileName(this,
                                                     tr("Export PDF"),
                                                     lastPath,
                                                     tr("PDF Files (*.pdf)"));
    if ( filename.isEmpty() )
        return;

    settings.setValue("qsllabel/last_pdf_path", QFileInfo(filename).path());

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filename);

    updateRendererOptions();
    renderer.printAll(&printer);
    askAndMarkQslSent();
}

void QSLPrintLabelDialog::exportCardImages()
{
    FCT_IDENTIFICATION;

    if ( currentPrintMode() != QSLPrintMode::DirectCard || labelsData.isEmpty() )
        return;

    const QString lastPath = LogParam::getQslLabelImageExportPath(QDir::homePath());
    const QString dirPath = QFileDialog::getExistingDirectory(this,
                                                              tr("Export QSL Card Images"),
                                                              lastPath);

    if ( dirPath.isEmpty() )
        return;

    LogParam::setQslLabelImageExportPath(dirPath);

    updateRendererOptions();

    const QDir dir(dirPath);
    QStringList fileNames;
    bool hasExistingFiles = false;

    for ( int i = 0; i < labelsData.size(); ++i )
    {
        const QString fileName = imageExportFileName(labelsData.at(i), i);
        fileNames.append(fileName);

        if ( QFileInfo::exists(dir.filePath(fileName)) )
            hasExistingFiles = true;
    }

    if ( hasExistingFiles )
    {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            tr("Export QSL Card Images"),
            tr("Some image files already exist. Overwrite them?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if ( answer != QMessageBox::Yes )
            return;
    }

    int saved = 0;
    QStringList failedFiles;

    for ( int i = 0; i < labelsData.size(); ++i )
    {
        const QImage cardImage = renderer.renderDirectCard(i);
        const QString fileName = fileNames.at(i);

        if ( cardImage.isNull() )
        {
            failedFiles.append(fileName);
            continue;
        }

        const QString filePath = dir.filePath(fileName);

        if ( cardImage.save(filePath, "JPG", 95) )
        {
            ++saved;
        }
        else
        {
            failedFiles.append(fileName);
            qCWarning(runtime) << "Cannot save QSL card image" << filePath;
        }
    }

    const QString dialogTitle = tr("Export QSL Card Images");

    if ( failedFiles.isEmpty() )
    {
        QMessageBox::information(this,
                                 dialogTitle,
                                 tr("Exported %n QSL card image(s).", "", saved));
        askAndMarkQslSent();
        return;
    }

    QMessageBox messageBox(QMessageBox::Warning,
                           dialogTitle,
                           tr("Exported %1 of %2 QSL card images.")
                               .arg(saved)
                               .arg(labelsData.size()),
                           QMessageBox::Ok,
                           this);
    messageBox.setInformativeText(tr("QSOs were not marked as sent."));
    messageBox.setDetailedText(failedFiles.join('\n'));
    messageBox.exec();
}

void QSLPrintLabelDialog::askAndMarkQslSent()
{
    FCT_IDENTIFICATION;

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Mark as Sent"),
        tr("Mark printed/exported QSOs as sent?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if ( answer != QMessageBox::Yes )
        return;

    const QString whereClause = buildWhereClause();
    const QString sql = QString("UPDATE contacts "
                                "SET qsl_sent = 'Y', "
                                "qsl_sdate = strftime('%Y-%m-%d', DATETIME('now', 'utc')) "
                                "WHERE %1").arg(whereClause);

    QSqlQuery query;

    if ( !query.prepare(sql) )
    {
        qCWarning(runtime) << "Cannot prepare mark-sent query" << query.lastError().text();
        return;
    }

    bindWhereClause(query);

    if ( !query.exec() )
        qCWarning(runtime) << "Cannot execute mark-sent query" << query.lastError().text();
}
