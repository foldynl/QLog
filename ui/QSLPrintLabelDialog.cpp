#include "ui/QSLPrintLabelDialog.h"
#include "ui_QSLPrintLabelDialog.h"

#include <utility>

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
#include <QSaveFile>
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
#include "data/Data.h"
#include "data/StationProfile.h"
#include "models/SqlListModel.h"
#include "ui/component/LogbookFieldComboBox.h"

MODULE_IDENTIFICATION("qlog.ui.qslprintlabeldialog");

QSLPrintLabelDialog::QSLPrintLabelDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QSLPrintLabelDialog),
    labelBuilder(new QSLLabelBuilder)
{
    FCT_IDENTIFICATION;

    refreshTimer.setSingleShot(true);
    connect(&refreshTimer, &QTimer::timeout, this, &QSLPrintLabelDialog::applyPendingUpdate);

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

    ui->orderComboBox->addItem(tr("Callsign"), static_cast<int>(QSLLabelOrder::Callsign));
    ui->orderComboBox->addItem(tr("QSL Route"), static_cast<int>(QSLLabelOrder::QslRoute));

    ui->printPageSizeComboBox->addItem("A4", static_cast<int>(QPageSize::A4));
    ui->printPageSizeComboBox->addItem("Letter", static_cast<int>(QPageSize::Letter));
    ui->printPageSizeComboBox->addItem(tr("Custom"), static_cast<int>(QPageSize::Custom));

    /* Populate template combo: predefined + Custom */
    const QList<LabelTemplate> templates = QSLPrintLabelRenderer::predefinedTemplates();
    for ( const LabelTemplate &tmpl : templates )
        ui->templateComboBox->addItem(tmpl.name);
    ui->templateComboBox->addItem(tr("Custom"));

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

    setupCollapsibleSection(ui->filterSectionButton, ui->filterSectionContent);
    setupCollapsibleSection(ui->templateSectionButton, ui->templateSectionContent);
    setupCollapsibleSection(ui->cardSectionButton, ui->cardSectionContent);
    setupCollapsibleSection(ui->printOptionsSectionButton, ui->printOptionsSectionContent);

    /* Child signals stay blocked while persisted settings are restored. */
    loadSettings();
    updateFilterUi();

    for ( QWidget *w : widgets )
        w->blockSignals(false);

    refreshData();

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

void QSLPrintLabelDialog::setupCollapsibleSection(QToolButton *button, QWidget *content)
{
    FCT_IDENTIFICATION;

    connect(button, &QToolButton::toggled, this, [button, content](bool checked)
    {
        button->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        content->setMaximumHeight(checked ? QWIDGETSIZE_MAX : 0);
    });
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
    int orderIdx = ui->orderComboBox->findData(LogParam::getQslLabelOrder());
    ui->orderComboBox->setCurrentIndex(orderIdx >= 0 ? orderIdx : 0);
    ui->qslManagerCheckBox->setChecked(LogParam::getQslLabelPrintQslManager());
    ui->routePrefixCheckBox->setChecked(LogParam::getQslLabelPrintRoutePrefix());
    int outputPageSizeIdx = ui->printPageSizeComboBox->findData(LogParam::getQslLabelPageSize());
    ui->printPageSizeComboBox->setCurrentIndex(outputPageSizeIdx >= 0 ? outputPageSizeIdx : 0);
    ui->customPageWidthSpinBox->setValue(LogParam::getQslLabelCustomPageWidth());
    ui->customPageHeightSpinBox->setValue(LogParam::getQslLabelCustomPageHeight());
    updateOutputPageSizeUi();

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
    LogParam::setQslLabelPageSize(static_cast<int>(currentOutputPageSizeId()));
    LogParam::setQslLabelCustomPageWidth(ui->customPageWidthSpinBox->value());
    LogParam::setQslLabelCustomPageHeight(ui->customPageHeightSpinBox->value());
    LogParam::setQslLabelPrintBorders(ui->printBordersCheckBox->isChecked());
    LogParam::setQslLabelOrder(ui->orderComboBox->currentData().toInt());
    LogParam::setQslLabelPrintQslManager(ui->qslManagerCheckBox->isChecked());
    LogParam::setQslLabelPrintRoutePrefix(ui->routePrefixCheckBox->isChecked());
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

    const QSignalBlocker colsBlocker(ui->colsSpinBox);
    const QSignalBlocker rowsBlocker(ui->rowsSpinBox);
    const QSignalBlocker labelWidthBlocker(ui->labelWidthSpinBox);
    const QSignalBlocker labelHeightBlocker(ui->labelHeightSpinBox);
    const QSignalBlocker leftMarginBlocker(ui->leftMarginSpinBox);
    const QSignalBlocker topMarginBlocker(ui->topMarginSpinBox);
    const QSignalBlocker hSpacingBlocker(ui->hSpacingSpinBox);
    const QSignalBlocker vSpacingBlocker(ui->vSpacingSpinBox);

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
    tmpl.pageSize = currentOutputPageSizeId();
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

QSLLabelOrder QSLPrintLabelDialog::currentLabelOrder() const
{
    FCT_IDENTIFICATION;

    return static_cast<QSLLabelOrder>(ui->orderComboBox->currentData().toInt());
}

QPageSize::PageSizeId QSLPrintLabelDialog::currentOutputPageSizeId() const
{
    FCT_IDENTIFICATION;

    return static_cast<QPageSize::PageSizeId>(ui->printPageSizeComboBox->currentData().toInt());
}

QPageSize QSLPrintLabelDialog::currentOutputPageSize() const
{
    FCT_IDENTIFICATION;

    const QPageSize::PageSizeId pageSizeId = currentOutputPageSizeId();

    if ( pageSizeId == QPageSize::Custom )
    {
        return QPageSize(QSizeF(ui->customPageWidthSpinBox->value(),
                               ui->customPageHeightSpinBox->value()),
                         QPageSize::Millimeter,
                         tr("Custom"),
                         QPageSize::ExactMatch);
    }

    return QPageSize(pageSizeId);
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
    styleOpts.showQslManager = ui->qslManagerCheckBox->isChecked();
    styleOpts.showRoutePrefix = ui->routePrefixCheckBox->isChecked();

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

    for ( const QChar &ch : static_cast<const QString &>(base) )
    {
        if ( ch.isLetterOrNumber() || ch == '_' || ch == '-' )
            sanitized += ch;
        else if ( ch.isSpace() || ch == '/' || ch == ':' || ch == '.' )
            sanitized += '_';
    }

    while ( sanitized.contains("__") )
        sanitized.replace("__", "_");

    sanitized = sanitized.trimmed();

    if ( sanitized.startsWith("_") ) sanitized.remove(0, 1);
    if ( sanitized.endsWith("_") ) sanitized.chop(1);
    if ( sanitized.isEmpty() ) sanitized = QStringLiteral("qsl_card");

    return QString("%1_%2.jpg").arg(sanitized, QString::number(index + 1).rightJustified(3, '0'));
}

void QSLPrintLabelDialog::updateRendererOptions()
{
    FCT_IDENTIFICATION;

    renderer.setTemplate(currentLabelTemplate());
    renderer.setPrintMode(currentPrintMode());
    renderer.setPageSize(currentOutputPageSize());
    renderer.setFooterLeft(ui->footerLeftEdit->text());
    renderer.setFooterRight(ui->footerRightEdit->text());
    renderer.setPrintBorders(ui->printBordersCheckBox->isChecked());
    renderer.setCardLayout(buildCardLayout());
    renderer.setCardBackgroundImage(cardBackgroundImageData);
    renderer.setStyleOptions(buildStyleOptions());
}

void QSLPrintLabelDialog::updateOutputPageSizeUi()
{
    FCT_IDENTIFICATION;

    const bool custom = currentOutputPageSizeId() == QPageSize::Custom;
    ui->customPageWidthLabel->setVisible(custom);
    ui->customPageWidthSpinBox->setVisible(custom);
    ui->customPageHeightLabel->setVisible(custom);
    ui->customPageHeightSpinBox->setVisible(custom);
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

void QSLPrintLabelDialog::updateFilterUi()
{
    FCT_IDENTIFICATION;

    const bool dateRangeEnabled = ui->dateRangeCheckBox->isChecked();
    const bool myCallsignEnabled = ui->myCallsignCheckBox->isChecked();
    const bool stationProfileEnabled = ui->stationProfileCheckBox->isChecked();

    ui->startDateEdit->setEnabled(dateRangeEnabled);
    ui->endDateEdit->setEnabled(dateRangeEnabled);
    ui->myCallsignComboBox->setEnabled(myCallsignEnabled);
    ui->stationProfileComboBox->setEnabled(stationProfileEnabled);
    ui->qslSentComboBox->setEnabled(ui->qslSentCheckBox->isChecked());
    ui->userFilterComboBox->setEnabled(ui->userFilterCheckBox->isChecked());

    if ( stationProfileEnabled )
    {
        const StationProfile &profile = StationProfilesManager::instance()->getProfile(
            ui->stationProfileComboBox->currentText());
        ui->stationProfileComboBox->setToolTip(tr("QSOs matching this station profile")
                                               + "<br/>"
                                               + profile.toHTMLString());
    }
    else
    {
        ui->stationProfileComboBox->setToolTip({});
    }
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

void QSLPrintLabelDialog::filterChanged()
{
    FCT_IDENTIFICATION;

    if ( sender() == ui->myCallsignCheckBox && ui->myCallsignCheckBox->isChecked() )
        ui->stationProfileCheckBox->setChecked(false);
    else if ( sender() == ui->stationProfileCheckBox && ui->stationProfileCheckBox->isChecked() )
        ui->myCallsignCheckBox->setChecked(false);

    updateFilterUi();
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

    refreshPreviewFromFirstPage();
}

void QSLPrintLabelDialog::skipChanged(int value)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << value;

    renderer.setSkipLabels(value);
    refreshPreview();
}

void QSLPrintLabelDialog::zoomChanged(int value)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << value;

    zoomPercent = value;
    scalePreview();
}

void QSLPrintLabelDialog::customTemplateFieldChanged()
{
    FCT_IDENTIFICATION;

    /* Only respond if Custom template is selected */
    const QList<LabelTemplate> templates = QSLPrintLabelRenderer::predefinedTemplates();
    int index = ui->templateComboBox->currentIndex();

    if ( index >= 0 && index < templates.size() )
        return;

    refreshPreviewFromFirstPage();
}

void QSLPrintLabelDialog::outputPageSizeChanged()
{
    FCT_IDENTIFICATION;

    updateOutputPageSizeUi();
    refreshPreviewFromFirstPage();
}

void QSLPrintLabelDialog::printModeChanged(int index)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << index;

    updatePrintModeUi();
    refreshPreviewFromFirstPage();
}

void QSLPrintLabelDialog::cardLayoutChanged()
{
    FCT_IDENTIFICATION;

    updateCardLabelBackgroundColorUi();
    refreshPreviewFromFirstPage();
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
    refreshPreview();
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
    refreshPreview();
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
    refreshPreview();
}

void QSLPrintLabelDialog::clearCardBackgroundImage()
{
    FCT_IDENTIFICATION;

    cardBackgroundImageData = QImage();
    updateCardBackgroundUi();
    refreshPreview();
}

void QSLPrintLabelDialog::resizeEvent(QResizeEvent *event)
{
    FCT_IDENTIFICATION;

    QWidget::resizeEvent(event);
    scalePreview();
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

void QSLPrintLabelDialog::ContactFilter::bind(QSqlQuery &query) const
{
    FCT_IDENTIFICATION;

    for ( auto it = bindings.cbegin(); it != bindings.cend(); ++it )
        query.bindValue(it.key(), it.value());
}

QSLPrintLabelDialog::ContactFilter QSLPrintLabelDialog::currentFilter() const
{
    FCT_IDENTIFICATION;

    ContactFilter filter;
    QStringList where ({"1 = 1"});

    if ( ui->dateRangeCheckBox->isChecked() )
    {
        where << "(datetime(start_time) BETWEEN datetime(:start_date) AND datetime(:end_date))";

        const QDate startDate = ui->startDateEdit->date();
        const QDate endDate = ui->endDateEdit->date();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        filter.bindings.insert(":start_date", startDate.startOfDay());
        filter.bindings.insert(":end_date", endDate.endOfDay());
#else
        filter.bindings.insert(":start_date", QDateTime(startDate));
        filter.bindings.insert(":end_date", QDateTime(endDate));
#endif
    }

    if ( ui->myCallsignCheckBox->isChecked() )
    {
        where << "upper(station_callsign) = upper(:stationCallsign)";
        filter.bindings.insert(":stationCallsign", ui->myCallsignComboBox->currentText());
    }

    if ( ui->stationProfileCheckBox->isChecked() )
    {
        const StationProfile &stProfile = StationProfilesManager::instance()->getProfile(
            ui->stationProfileComboBox->currentText());

        where << QString("EXISTS ("
                         "SELECT 1 FROM station_profiles"
                         " WHERE station_profiles.profile_name = :stationProfileName"
                         " AND %1"
                         ")").arg(stProfile.getContactInnerJoin());
        filter.bindings.insert(":stationProfileName",
                               ui->stationProfileComboBox->currentText());
    }

    if ( ui->qslSentCheckBox->isChecked() )
    {
        where << "upper(qsl_sent) = upper(:qslSent)";
        filter.bindings.insert(":qslSent", ui->qslSentComboBox->currentData().toString());
    }

    if ( ui->userFilterCheckBox->isChecked() )
    {
        QString filterClause = QSOFilterManager::getWhereClause(ui->userFilterComboBox->currentText());
        if ( !filterClause.isEmpty() )
            where << filterClause;
    }

    filter.whereClause = where.join(" AND ");
    return filter;
}

void QSLPrintLabelDialog::loadData()
{
    FCT_IDENTIFICATION;

    labelBuilder->clear();

    const ContactFilter filter = currentFilter();
    const QString extraCol = ui->extraColumnComboBox->currentData().toString();
    const QString extraColSelect = extraCol.isEmpty() ? QString()
                                                      : QString(", \"%1\"").arg(extraCol);

    const QString sql = QString("SELECT start_time, callsign, band, mode, submode, "
                          "qsl_rcvd, lotw_qsl_rcvd, eqsl_qsl_rcvd, dcl_qsl_rcvd, "
                          "qsl_via, dxcc, pfx%1 "
                          "FROM contacts "
                          "WHERE %2 "
                          "ORDER BY callsign COLLATE NOCASE ASC, start_time ASC")
                          .arg(extraColSelect, filter.whereClause);

    QSqlQuery query;
    query.setForwardOnly(true);

    if ( !query.prepare(sql) )
    {
        qCWarning(runtime) << "Cannot prepare QSL label query" << query.lastError().text();
        return;
    }

    filter.bind(query);

    if ( !query.exec() )
    {
        qCWarning(runtime) << "Cannot execute QSL label query" << query.lastError().text();
        return;
    }

    QVector<QSLSourceQso> sourceQsos;

    while ( query.next() )
    {
        QSLSourceQso source;
        source.startTime = query.value(0).toDateTime().toUTC();
        source.callsign = query.value(1).toString().toUpper();
        source.row.time = source.startTime.toString("HH:mm");
        source.row.band = query.value(2).toString().toUpper();
        source.row.extra = extraCol.isEmpty() ? QString() : query.value(12).toString();
        source.qslVia = query.value(9).toString().trimmed().toUpper();
        source.dxcc = query.value(10).toInt();
        source.prefix = query.value(11).toString().trimmed().toUpper();

        const QString mode = query.value(3).toString().toUpper();
        const QString submode = query.value(4).toString().toUpper();

        if ( mode == "SSB" && ( submode == "USB" || submode == "LSB" ) )
            source.row.mode = "SSB";
        else if ( !submode.isEmpty() )
            source.row.mode = submode;
        else
            source.row.mode = mode;

        const bool qslReceived = query.value(5).toString().compare("Y", Qt::CaseInsensitive) == 0
                                 || query.value(6).toString().compare("Y", Qt::CaseInsensitive) == 0
                                 || query.value(7).toString().compare("Y", Qt::CaseInsensitive) == 0
                                 || query.value(8).toString().compare("Y", Qt::CaseInsensitive) == 0;

        source.row.qsl = qslReceived ? "TNX" : "PSE";

        sourceQsos.append(source);
    }

    labelBuilder->setSourceQsos(std::move(sourceQsos));
}

void QSLPrintLabelDialog::rebuildLabels()
{
    FCT_IDENTIFICATION;

    QSLLabelBuildOptions options;
    options.dateFormat = ui->dateFormatEdit->text();
    options.maxRows = ui->maxRowsSpinBox->value();
    options.order = currentLabelOrder();
    options.showQslManager = ui->qslManagerCheckBox->isChecked();
    options.showRoutePrefix = ui->routePrefixCheckBox->isChecked();

    labelsData = labelBuilder->build(options);
    renderer.setLabels(labelsData);
    renderer.setSkipLabels(ui->skipSpinBox->value());
    currentPage = 0;
}

void QSLPrintLabelDialog::refreshData()
{
    FCT_IDENTIFICATION;

    scheduleUpdate(UpdateLevel::Data);
}

void QSLPrintLabelDialog::refreshLabels()
{
    FCT_IDENTIFICATION;

    scheduleUpdate(UpdateLevel::Labels);
}

void QSLPrintLabelDialog::refreshPreview()
{
    FCT_IDENTIFICATION;

    scheduleUpdate(UpdateLevel::Preview);
}

void QSLPrintLabelDialog::refreshPreviewFromFirstPage()
{
    FCT_IDENTIFICATION;

    currentPage = 0;
    refreshPreview();
}

void QSLPrintLabelDialog::scheduleUpdate(UpdateLevel level)
{
    FCT_IDENTIFICATION;

    if ( level > pendingUpdate )
        pendingUpdate = level;

    if ( pendingUpdate == UpdateLevel::Data )
        showRefreshState(tr("Loading QSOs..."));
    else if ( pendingUpdate == UpdateLevel::Labels )
        showRefreshState(tr("Updating labels..."));

    const int interval = pendingUpdate == UpdateLevel::Data ? 50
                       : pendingUpdate == UpdateLevel::Labels ? 150
                                                             : 100;
    refreshTimer.start(interval);
}

void QSLPrintLabelDialog::applyPendingUpdate()
{
    FCT_IDENTIFICATION;

    const UpdateLevel update = pendingUpdate;
    pendingUpdate = UpdateLevel::None;

    if ( update == UpdateLevel::None )
        return;

    if ( update >= UpdateLevel::Data )
        loadData();

    if ( update >= UpdateLevel::Labels )
        rebuildLabels();

    renderCurrentPage();
}

void QSLPrintLabelDialog::showRefreshState(const QString &message)
{
    FCT_IDENTIFICATION;

    previewPagePixmap = QPixmap();
    ui->previewLabel->clear();
    ui->previewLabel->setText(message);
    ui->previewLabel->setAlignment(Qt::AlignCenter);
    ui->statusLabel->setText(message);

    ui->printButton->setEnabled(false);
    ui->exportPdfButton->setEnabled(false);
    ui->exportImagesButton->setEnabled(false);
    ui->prevPageButton->setEnabled(false);
    ui->nextPageButton->setEnabled(false);
}

void QSLPrintLabelDialog::renderCurrentPage()
{
    FCT_IDENTIFICATION;

    updateRendererOptions();

    int totalPages = renderer.pageCount();
    int totalLabels = renderer.labelCount();

    bool hasLabels = ( totalLabels > 0 );
    bool hasPages = ( totalPages > 0 );
    ui->printButton->setEnabled(hasLabels && hasPages);
    ui->exportPdfButton->setEnabled(hasLabels && hasPages);
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
        previewPagePixmap = QPixmap::fromImage(renderer.renderPage(currentPage));
        scalePreview();
    }
    else
    {
        previewPagePixmap = QPixmap();
        ui->previewLabel->clear();

        if ( hasLabels
             && currentPrintMode() == QSLPrintMode::DirectCard )
            ui->previewLabel->setText(tr("The QSL card does not fit on the selected page size"));
        else
            ui->previewLabel->setText(tr("No matching QSOs found"));

        ui->previewLabel->setAlignment(Qt::AlignCenter);
    }
}

void QSLPrintLabelDialog::scalePreview()
{
    FCT_IDENTIFICATION;

    if ( previewPagePixmap.isNull() )
        return;

    const int fitWidth = ui->previewScrollArea->viewport()->width() - 10;

    if ( fitWidth > 0 )
    {
        const int scaledWidth = fitWidth * zoomPercent / 100;
        ui->previewLabel->setPixmap(previewPagePixmap.scaledToWidth(scaledWidth,
                                                                    Qt::SmoothTransformation));
    }
    else
        ui->previewLabel->setPixmap(previewPagePixmap);

    ui->previewLabel->setAlignment(Qt::AlignCenter);
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
        refreshPreview();
    }
}

void QSLPrintLabelDialog::nextPage()
{
    FCT_IDENTIFICATION;

    int totalPages = renderer.pageCount();
    if ( totalPages > 0 && currentPage < totalPages - 1 )
    {
        currentPage++;
        refreshPreview();
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
        if ( !renderer.printAll(&printer) )
        {
            QMessageBox::warning(this, tr("Print Failed"), tr("Cannot print QSLs. QSOs were not marked as sent."));
            return;
        }
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

    updateRendererOptions();
#ifdef QLOG_FLATPAK
    QSaveFile file(filename);
    if ( !file.open(QIODevice::WriteOnly) || !renderer.writePdf(&file)
         || file.error() != QFileDevice::NoError || !file.flush() || !file.commit() )
    {
        qCWarning(runtime) << "Cannot save QSL PDF" << filename << file.errorString();
        QMessageBox::warning(this, tr("Export Failed"),
                              tr("Cannot save %1:\n%2\n\nQSOs were not marked as sent.").arg(filename, file.errorString()));
        return;
    }
#else
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filename);

    if ( !renderer.printAll(&printer) )
    {
        QMessageBox::warning(this, tr("Export Failed"), tr("Cannot export QSLs. QSOs were not marked as sent."));
        return;
    }
#endif
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

    const ContactFilter filter = currentFilter();
    const QString sql = QString("UPDATE contacts "
                                "SET qsl_sent = 'Y', "
                                "qsl_sdate = strftime('%Y-%m-%d', DATETIME('now', 'utc')) "
                                "WHERE %1").arg(filter.whereClause);

    QSqlQuery query;

    if ( !query.prepare(sql) )
    {
        qCWarning(runtime) << "Cannot prepare mark-sent query" << query.lastError().text();
        return;
    }

    filter.bind(query);

    if ( !query.exec() )
        qCWarning(runtime) << "Cannot execute mark-sent query" << query.lastError().text();
}
