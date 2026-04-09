#include <QFileDialog>
#include <QMessageBox>
#include <QSharedPointer>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QColorDialog>
#include <QPixmap>
#include <QSqlField>
#include <QSqlRecord>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QScrollArea>
#include <QFontComboBox>
#include <QSpinBox>
#include <QDateTime>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QComboBox>
#include <QFileDialog>
#include <QStandardPaths>

#include "EmailQSLSettingsWidget.h"
#include "ui_EmailQSLSettingsWidget.h"
#include "service/emailqsl/EmailQSLService.h"
#include "ui/component/CardEditorWidget.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.emailqslsettingswidget");

// ---------------------------------------------------------------------------
// Column indices for the overlay table
// TEXT columns: TYPE FIELD X Y FONT SIZE COLOR BOLD ITALIC  (BOX cols greyed)
// BOX  columns: TYPE FIELD X Y W H FILL OPACITY RADIUS BORDER
// We use one wide model and show/grey non-applicable cells per row.
// ---------------------------------------------------------------------------
enum OverlayCol
{
    OC_TYPE     = 0,
    OC_FIELD    = 1,   // TEXT: merge key;  BOX: optional caption
    OC_X        = 2,
    OC_Y        = 3,
    OC_FONT     = 4,   // TEXT only
    OC_SIZE     = 5,   // TEXT only (also used for BOX caption font size)
    OC_COLOR    = 6,   // TEXT: text color; BOX: border color
    OC_BOLD     = 7,   // TEXT only
    OC_ITALIC   = 8,   // TEXT only
    OC_W        = 9,   // BOX only
    OC_H        = 10,  // BOX only
    OC_FILL     = 11,  // BOX only
    OC_OPACITY  = 12,  // BOX only
    OC_RADIUS   = 13,  // BOX only
    OC_COUNT    = 14
};

// ---------------------------------------------------------------------------
// Local delegate: merge-field combobox for the Field column
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Delegate: Type combo (TEXT / BOX)
// ---------------------------------------------------------------------------
class TypeDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override
    {
        QComboBox *cb = new QComboBox(parent);
        cb->addItem(QStringLiteral("TEXT"));
        cb->addItem(QStringLiteral("BOX"));
        cb->addItem(QStringLiteral("LABEL"));
        return cb;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        QComboBox *cb = static_cast<QComboBox *>(editor);
        cb->setCurrentIndex(cb->findText(index.data(Qt::DisplayRole).toString()));
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        model->setData(index, static_cast<QComboBox *>(editor)->currentText(), Qt::EditRole);
    }
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &) const override
    { editor->setGeometry(option.rect); }
};

// ---------------------------------------------------------------------------
// Delegate: merge-field combobox (TEXT Field column)
// ---------------------------------------------------------------------------
class MergeFieldDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override
    {
        QComboBox *cb = new QComboBox(parent);
        cb->setEditable(true);  // also allow free text (for BOX captions)
        for (const EmailQSLBase::MergeField &f : EmailQSLBase::availableMergeFields())
            cb->addItem(f.key);
        return cb;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        QComboBox *cb = static_cast<QComboBox *>(editor);
        const QString v = index.data(Qt::DisplayRole).toString();
        const int idx = cb->findText(v);
        if (idx >= 0) cb->setCurrentIndex(idx);
        else          cb->setEditText(v);
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        model->setData(index, static_cast<QComboBox *>(editor)->currentText(), Qt::EditRole);
    }
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &) const override
    { editor->setGeometry(option.rect); }
};

// ---------------------------------------------------------------------------
// Delegate: QFontComboBox for the Font column
// ---------------------------------------------------------------------------
class FontFamilyDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override
    { return new QFontComboBox(parent); }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        static_cast<QFontComboBox *>(editor)->setCurrentFont(
            QFont(index.data(Qt::DisplayRole).toString()));
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        model->setData(index,
            static_cast<QFontComboBox *>(editor)->currentFont().family(), Qt::EditRole);
    }
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &) const override
    { editor->setGeometry(option.rect); }
};

// ---------------------------------------------------------------------------
// Delegate: colored swatch — double-click opens QColorDialog
// ---------------------------------------------------------------------------
class ColorSwatchDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const QString hex = index.data(Qt::DisplayRole).toString();
        const QColor  c(hex.isEmpty() ? QStringLiteral("#000000") : hex);
        const QRect   r = option.rect.adjusted(2, 2, -2, -2);
        painter->fillRect(r, c);
        painter->setPen(QPen(Qt::gray, 1));
        painter->drawRect(r);
        painter->setPen(c.lightness() > 128 ? Qt::black : Qt::white);
        QFont f = painter->font(); f.setPointSize(8); painter->setFont(f);
        painter->drawText(option.rect, Qt::AlignCenter, hex);
    }

    QWidget *createEditor(QWidget *, const QStyleOptionViewItem &,
                          const QModelIndex &) const override { return nullptr; }

    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &, const QModelIndex &index) override
    {
        if (event->type() != QEvent::MouseButtonDblClick) return false;
        const QString cur = index.data(Qt::DisplayRole).toString();
        const QColor chosen = QColorDialog::getColor(
            QColor(cur.isEmpty() ? QStringLiteral("#000000") : cur),
            nullptr, tr("Choose Color"));
        if (chosen.isValid())
            model->setData(index, chosen.name(), Qt::EditRole);
        return true;
    }
};

// ---------------------------------------------------------------------------
// Delegate: compact QSpinBox — editor width constrained to cell
// ---------------------------------------------------------------------------
class SpinBoxDelegate : public QStyledItemDelegate
{
    int m_min, m_max;
public:
    SpinBoxDelegate(int min, int max, QObject *parent)
        : QStyledItemDelegate(parent), m_min(min), m_max(max) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &) const override
    {
        QSpinBox *sb = new QSpinBox(parent);
        sb->setRange(m_min, m_max);
        // Constrain width so the spinbox doesn't expand beyond the cell
        sb->setFixedWidth(option.rect.width());
        sb->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        return sb;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        static_cast<QSpinBox *>(editor)->setValue(index.data(Qt::DisplayRole).toInt());
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        model->setData(index, static_cast<QSpinBox *>(editor)->value(), Qt::EditRole);
    }
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &) const override
    { editor->setGeometry(option.rect); }
};

// ---------------------------------------------------------------------------
// SMTP presets
// ---------------------------------------------------------------------------
struct SmtpPreset
{
    QString label;
    QString host;
    int     port;
    int     encryption;
    QString hint;
};

static QList<SmtpPreset> smtpPresets()
{
    return {
        { QObject::tr("-- Select preset --"), {}, 587, 2, {} },
        { QStringLiteral("Gmail"),
          QStringLiteral("smtp.gmail.com"), 587, 2,
          QObject::tr("Gmail: go to myaccount.google.com → Security → 2-Step Verification → App Passwords.\n"
                      "Create an App Password for 'Mail'. Use the 16-character code as your password here.") },
        { QStringLiteral("Gmail (SSL/TLS port 465)"),
          QStringLiteral("smtp.gmail.com"), 465, 1,
          QObject::tr("Gmail SSL variant (port 465). Use your Gmail App Password.") },
        { QStringLiteral("Outlook / Hotmail / Microsoft 365"),
          QStringLiteral("smtp-mail.outlook.com"), 587, 2,
          QObject::tr("Outlook: use your full Microsoft email and password.\n"
                      "If MFA is on, generate an App Password at account.microsoft.com → Security.") },
        { QStringLiteral("Yahoo Mail"),
          QStringLiteral("smtp.mail.yahoo.com"), 587, 2,
          QObject::tr("Yahoo: go to Account Security and click 'Generate app password'.") },
        { QStringLiteral("iCloud Mail"),
          QStringLiteral("smtp.mail.me.com"), 587, 2,
          QObject::tr("iCloud: generate an App-Specific Password at appleid.apple.com → Sign-In and Security.") },
        { QStringLiteral("Zoho Mail"),
          QStringLiteral("smtp.zoho.com"), 587, 2,
          QObject::tr("Zoho: enable SMTP in Zoho Mail Settings → Mail Accounts → IMAP/POP/SMTP.") },
        { QStringLiteral("Custom"), {}, 587, 2, {} },
    };
}

// ---------------------------------------------------------------------------
// Helper — build a dummy QSqlRecord for previewing merge fields
// ---------------------------------------------------------------------------
static QSqlRecord buildDummyRecord()
{
    static const QStringList cols = {
        "callsign","start_time","freq","band","mode","submode",
        "rst_sent","rst_rcvd","name_intl","name","qth_intl","qth",
        "country_intl","country","gridsquare","dxcc","cqz","ituz",
        "tx_pwr","email","station_callsign","my_gridsquare","operator",
        "comment_intl","comment","sota_ref","pota_ref","wwff_ref",
        "iota","sig_intl","contest_id"
    };
    QSqlRecord r;
    for (const QString &c : cols)
    {
        QSqlField f(c, QVariant::String);
        r.append(f);
    }
    r.setValue("callsign",         QStringLiteral("W1AW"));
    r.setValue("start_time",       QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    r.setValue("freq",             QStringLiteral("14.225"));
    r.setValue("band",             QStringLiteral("20M"));
    r.setValue("mode",             QStringLiteral("SSB"));
    r.setValue("rst_sent",         QStringLiteral("59"));
    r.setValue("rst_rcvd",         QStringLiteral("59"));
    r.setValue("name_intl",        QStringLiteral("ARRL HQ"));
    r.setValue("station_callsign", QStringLiteral("AA5SH"));
    r.setValue("my_gridsquare",    QStringLiteral("EM20"));
    r.setValue("gridsquare",       QStringLiteral("FN31"));
    r.setValue("country_intl",     QStringLiteral("United States"));
    return r;
}

// ---------------------------------------------------------------------------
// Helper — build a default overlay
// ---------------------------------------------------------------------------
static EmailQSLFieldOverlay makeOverlay(const QString &field,
                                       int x, int y,
                                       int fontSize,
                                       bool bold,
                                       const QString &color = QStringLiteral("#000000"),
                                       const QString &font  = QStringLiteral("Arial"))
{
    EmailQSLFieldOverlay ov;
    ov.fieldName  = field;
    ov.x          = x;
    ov.y          = y;
    ov.fontSize   = fontSize;
    ov.bold       = bold;
    ov.fontFamily = font;
    ov.color      = color;
    return ov;
}

// ---------------------------------------------------------------------------
// EmailQSLSettingsWidget
// ---------------------------------------------------------------------------

EmailQSLSettingsWidget::EmailQSLSettingsWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EmailQSLSettingsWidget),
      m_overlayModel(new QStandardItemModel(0, OC_COUNT, this)),
      m_testService(new EmailQSLService(this)),
      m_syncing(false)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    // ---- Overlay table ----
    m_overlayModel->setHorizontalHeaderLabels({
        tr("Type"), tr("Field / Caption"),
        tr("X"), tr("Y"),
        tr("Font"), tr("Pt"),
        tr("Color"), tr("B"), tr("I"),
        tr("W"), tr("H"), tr("Fill"), tr("Opacity%"), tr("Radius")
    });
    ui->overlayTableView->setModel(m_overlayModel);

    // Delegates
    auto *spinCoord  = new SpinBoxDelegate(0,     99999, this);
    auto *spinSz     = new SpinBoxDelegate(4,     300,   this);
    auto *spinDim    = new SpinBoxDelegate(1,     99999, this);
    auto *spinOpac   = new SpinBoxDelegate(0,     100,   this);
    auto *spinRadius = new SpinBoxDelegate(0,     500,   this);

    ui->overlayTableView->setItemDelegateForColumn(OC_TYPE,    new TypeDelegate(this));
    ui->overlayTableView->setItemDelegateForColumn(OC_FIELD,   new MergeFieldDelegate(this));
    ui->overlayTableView->setItemDelegateForColumn(OC_X,       spinCoord);
    ui->overlayTableView->setItemDelegateForColumn(OC_Y,       spinCoord);
    ui->overlayTableView->setItemDelegateForColumn(OC_FONT,    new FontFamilyDelegate(this));
    ui->overlayTableView->setItemDelegateForColumn(OC_SIZE,    spinSz);
    ui->overlayTableView->setItemDelegateForColumn(OC_COLOR,   new ColorSwatchDelegate(this));
    ui->overlayTableView->setItemDelegateForColumn(OC_W,       spinDim);
    ui->overlayTableView->setItemDelegateForColumn(OC_H,       spinDim);
    ui->overlayTableView->setItemDelegateForColumn(OC_FILL,    new ColorSwatchDelegate(this));
    ui->overlayTableView->setItemDelegateForColumn(OC_OPACITY, spinOpac);
    ui->overlayTableView->setItemDelegateForColumn(OC_RADIUS,  spinRadius);

    // Column widths — fixed-width numeric columns stay compact
    auto *hdr = ui->overlayTableView->horizontalHeader();
    hdr->setSectionResizeMode(OC_TYPE,    QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(OC_FIELD,   QHeaderView::Stretch);
    hdr->setSectionResizeMode(OC_X,       QHeaderView::Fixed);
    hdr->setSectionResizeMode(OC_Y,       QHeaderView::Fixed);
    hdr->setSectionResizeMode(OC_FONT,    QHeaderView::Stretch);
    hdr->setSectionResizeMode(OC_SIZE,    QHeaderView::Fixed);
    hdr->setSectionResizeMode(OC_COLOR,   QHeaderView::Fixed);
    hdr->setSectionResizeMode(OC_BOLD,    QHeaderView::Fixed);
    hdr->setSectionResizeMode(OC_ITALIC,  QHeaderView::Fixed);
    hdr->setSectionResizeMode(OC_W,       QHeaderView::Fixed);
    hdr->setSectionResizeMode(OC_H,       QHeaderView::Fixed);
    hdr->setSectionResizeMode(OC_FILL,    QHeaderView::Fixed);
    hdr->setSectionResizeMode(OC_OPACITY, QHeaderView::Fixed);
    hdr->setSectionResizeMode(OC_RADIUS,  QHeaderView::Fixed);
    // Set pixel widths for fixed columns
    ui->overlayTableView->setColumnWidth(OC_X,       55);
    ui->overlayTableView->setColumnWidth(OC_Y,       55);
    ui->overlayTableView->setColumnWidth(OC_SIZE,    40);
    ui->overlayTableView->setColumnWidth(OC_COLOR,   60);
    ui->overlayTableView->setColumnWidth(OC_BOLD,    24);
    ui->overlayTableView->setColumnWidth(OC_ITALIC,  24);
    ui->overlayTableView->setColumnWidth(OC_W,       55);
    ui->overlayTableView->setColumnWidth(OC_H,       55);
    ui->overlayTableView->setColumnWidth(OC_FILL,    60);
    ui->overlayTableView->setColumnWidth(OC_OPACITY, 55);
    ui->overlayTableView->setColumnWidth(OC_RADIUS,  50);

    // ---- Merge field reference table ----
    const auto fields = EmailQSLBase::availableMergeFields();
    ui->mergeFieldRefTable->setRowCount(fields.size());
    for (int i = 0; i < fields.size(); ++i)
    {
        ui->mergeFieldRefTable->setItem(
            i, 0, new QTableWidgetItem(QLatin1Char('{') + fields[i].key + QLatin1Char('}')));
        ui->mergeFieldRefTable->setItem(
            i, 1, new QTableWidgetItem(fields[i].description));
    }
    ui->mergeFieldRefTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->mergeFieldRefTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    // ---- Merge field combos (subject/body insert) ----
    for (const EmailQSLBase::MergeField &f : fields)
    {
        const QString display = QLatin1Char('{') + f.key + QLatin1Char('}');
        ui->bodyFieldCombo->addItem(display);
        ui->subjectFieldCombo->addItem(display);
    }

    // ---- SMTP presets ----
    ui->smtpPresetCombo->blockSignals(true);
    for (const SmtpPreset &p : smtpPresets())
        ui->smtpPresetCombo->addItem(p.label);
    ui->smtpPresetCombo->blockSignals(false);

    // ---- Connections ----
    connect(ui->browseCardImageButton,  &QPushButton::clicked,
            this, &EmailQSLSettingsWidget::browseCardImage);
    connect(ui->cardImagePathEdit, &QLineEdit::textChanged,
            this, &EmailQSLSettingsWidget::onCardImageChanged);

    connect(ui->smtpPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EmailQSLSettingsWidget::onSmtpPresetChanged);
    connect(ui->testConnectionButton, &QPushButton::clicked,
            this, &EmailQSLSettingsWidget::testConnection);
    connect(m_testService, &EmailQSLService::testFinished,
            this, &EmailQSLSettingsWidget::onTestFinished);

    connect(ui->addOverlayButton,        &QPushButton::clicked,
            this, &EmailQSLSettingsWidget::addOverlay);
    connect(ui->addBoxButton,            &QPushButton::clicked,
            this, &EmailQSLSettingsWidget::addBox);
    connect(ui->addLabelButton,          &QPushButton::clicked,
            this, &EmailQSLSettingsWidget::addLabel);
    connect(ui->addDefaultFieldsButton,  &QPushButton::clicked,
            this, &EmailQSLSettingsWidget::addDefaultOverlays);
    connect(ui->removeOverlayButton,     &QPushButton::clicked,
            this, &EmailQSLSettingsWidget::removeOverlay);
    connect(ui->previewCardButton,       &QPushButton::clicked,
            this, &EmailQSLSettingsWidget::previewCard);

    connect(ui->insertBodyFieldButton,    &QPushButton::clicked,
            this, &EmailQSLSettingsWidget::insertMergeFieldBody);
    connect(ui->insertSubjectFieldButton, &QPushButton::clicked,
            this, &EmailQSLSettingsWidget::insertMergeFieldSubject);

    // Table ↔ editor bidirectional sync
    connect(m_overlayModel, &QStandardItemModel::dataChanged,
            this, &EmailQSLSettingsWidget::onTableDataChanged);
    connect(m_overlayModel, &QStandardItemModel::rowsRemoved,
            this, &EmailQSLSettingsWidget::onTableDataChanged);

    connect(ui->cardEditorWidget, &CardEditorWidget::overlayPositionChanged,
            this, &EmailQSLSettingsWidget::onEditorPositionChanged);
    connect(ui->cardEditorWidget, &CardEditorWidget::overlaySizeChanged,
            this, &EmailQSLSettingsWidget::onEditorSizeChanged);
    connect(ui->cardEditorWidget, &CardEditorWidget::overlaySelected,
            this, &EmailQSLSettingsWidget::onEditorOverlaySelected);

    connect(ui->overlayTableView->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex &current, const QModelIndex &) {
                ui->cardEditorWidget->setSelectedIndex(
                    current.isValid() ? current.row() : -1);
                ui->removeOverlayButton->setEnabled(current.isValid());
            });
}

EmailQSLSettingsWidget::~EmailQSLSettingsWidget()
{
    delete ui;
}

// ---------------------------------------------------------------------------
// readSettings / writeSettings
// ---------------------------------------------------------------------------

void EmailQSLSettingsWidget::readSettings()
{
    FCT_IDENTIFICATION;

    ui->smtpHostEdit->setText(EmailQSLBase::getSmtpHost());
    ui->smtpPortSpin->setValue(EmailQSLBase::getSmtpPort());
    ui->encryptionCombo->setCurrentIndex(EmailQSLBase::getSmtpEncryption());
    ui->smtpUsernameEdit->setText(EmailQSLBase::getSmtpUsername());
    ui->smtpPasswordEdit->setText(EmailQSLBase::getSmtpPassword());
    ui->fromAddressEdit->setText(EmailQSLBase::getFromAddress());
    ui->fromNameEdit->setText(EmailQSLBase::getFromName());
    ui->subjectTemplateEdit->setText(EmailQSLBase::getSubjectTemplate());
    ui->bodyTemplateEdit->setPlainText(EmailQSLBase::getBodyTemplate());
    ui->cardImagePathEdit->setText(EmailQSLBase::getCardImagePath());

    m_overlays = EmailQSLBase::getCardFieldOverlays();
    listToOverlayTable();

    // Trigger image load into the editor (textChanged may not fire if value unchanged)
    onCardImageChanged(ui->cardImagePathEdit->text());
}

void EmailQSLSettingsWidget::writeSettings()
{
    FCT_IDENTIFICATION;

    EmailQSLBase::setSmtpHost(ui->smtpHostEdit->text().trimmed());
    EmailQSLBase::setSmtpPort(ui->smtpPortSpin->value());
    EmailQSLBase::setSmtpEncryption(ui->encryptionCombo->currentIndex());
    EmailQSLBase::saveSmtpCredentials(ui->smtpUsernameEdit->text().trimmed(),
                                      ui->smtpPasswordEdit->text());
    EmailQSLBase::setFromAddress(ui->fromAddressEdit->text().trimmed());
    EmailQSLBase::setFromName(ui->fromNameEdit->text().trimmed());
    EmailQSLBase::setSubjectTemplate(ui->subjectTemplateEdit->text());
    EmailQSLBase::setBodyTemplate(ui->bodyTemplateEdit->toPlainText());
    EmailQSLBase::setCardImagePath(ui->cardImagePathEdit->text().trimmed());

    overlayTableToList();
    EmailQSLBase::setCardFieldOverlays(m_overlays);
}

// ---------------------------------------------------------------------------
// Overlay table ↔ m_overlays ↔ card editor
// ---------------------------------------------------------------------------

// Helper — make a greyed-out non-editable placeholder for N/A cells
static QStandardItem *naItem()
{
    QStandardItem *si = new QStandardItem(QStringLiteral("—"));
    si->setFlags(si->flags() & ~Qt::ItemIsEditable);
    si->setForeground(QColor(0xbb, 0xbb, 0xbb));
    si->setTextAlignment(Qt::AlignCenter);
    return si;
}

void EmailQSLSettingsWidget::listToOverlayTable()
{
    // Guard against re-entrant onTableDataChanged() firing during setRowCount(0)
    // which would wipe m_overlays before we finish rebuilding the table.
    m_syncing = true;
    m_overlayModel->setRowCount(0);
    for (const EmailQSLFieldOverlay &ov : m_overlays)
    {
        const bool isBox = (ov.type == QLatin1String("BOX"));

        auto item = [](const QString &text) { return new QStandardItem(text); };
        auto checkItem = [](bool checked) {
            QStandardItem *si = new QStandardItem();
            si->setCheckable(true);
            si->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
            si->setTextAlignment(Qt::AlignCenter);
            return si;
        };

        QList<QStandardItem*> row;
        row << item(ov.type);                        // OC_TYPE
        row << item(ov.fieldName);                   // OC_FIELD
        row << item(QString::number(ov.x));           // OC_X
        row << item(QString::number(ov.y));           // OC_Y
        // TEXT-only
        row << (isBox ? naItem() : item(ov.fontFamily));               // OC_FONT
        row << (isBox && ov.fieldName.isEmpty()
                ? naItem() : item(QString::number(ov.fontSize)));       // OC_SIZE
        row << item(ov.color);                        // OC_COLOR (border/text)
        row << (isBox ? naItem() : checkItem(ov.bold));                // OC_BOLD
        row << (isBox ? naItem() : checkItem(ov.italic));              // OC_ITALIC
        // BOX-only
        row << (isBox ? item(QString::number(ov.width))  : naItem()); // OC_W
        row << (isBox ? item(QString::number(ov.height)) : naItem()); // OC_H
        row << (isBox ? item(ov.fillColor)               : naItem()); // OC_FILL
        row << (isBox ? item(QString::number(ov.opacity)): naItem()); // OC_OPACITY
        row << (isBox ? item(QString::number(ov.cornerRadius)) : naItem()); // OC_RADIUS

        m_overlayModel->appendRow(row);
    }
    m_syncing = false;
    ui->cardEditorWidget->setOverlays(m_overlays);
}

void EmailQSLSettingsWidget::overlayTableToList()
{
    m_overlays.clear();
    for (int r = 0; r < m_overlayModel->rowCount(); ++r)
    {
        EmailQSLFieldOverlay ov;
        auto txt = [&](int col) { return m_overlayModel->item(r, col)->text(); };

        ov.type       = txt(OC_TYPE);
        ov.fieldName  = txt(OC_FIELD);
        ov.x          = txt(OC_X).toInt();
        ov.y          = txt(OC_Y).toInt();
        ov.color      = txt(OC_COLOR);

        if (ov.type == QLatin1String("BOX"))
        {
            ov.fontFamily   = QStringLiteral("Arial");
            ov.fontSize     = txt(OC_SIZE) == QStringLiteral("—") ? 11 : txt(OC_SIZE).toInt();
            ov.width        = txt(OC_W).toInt();
            ov.height       = txt(OC_H).toInt();
            ov.fillColor    = txt(OC_FILL);
            ov.opacity      = txt(OC_OPACITY).toInt();
            ov.cornerRadius = txt(OC_RADIUS).toInt();
        }
        else
        {
            ov.fontFamily   = txt(OC_FONT);
            ov.fontSize     = txt(OC_SIZE).toInt();
            ov.bold   = m_overlayModel->item(r, OC_BOLD)->checkState()   == Qt::Checked;
            ov.italic = m_overlayModel->item(r, OC_ITALIC)->checkState() == Qt::Checked;
        }
        m_overlays.append(ov);
    }
}

void EmailQSLSettingsWidget::onTableDataChanged()
{
    FCT_IDENTIFICATION;

    if (m_syncing) return;
    m_syncing = true;
    overlayTableToList();
    ui->cardEditorWidget->setOverlays(m_overlays);
    m_syncing = false;
}

void EmailQSLSettingsWidget::onEditorPositionChanged(int index, int x, int y)
{
    FCT_IDENTIFICATION;

    if (m_syncing || index < 0 || index >= m_overlayModel->rowCount()) return;
    m_syncing = true;
    m_overlayModel->item(index, OC_X)->setText(QString::number(x));
    m_overlayModel->item(index, OC_Y)->setText(QString::number(y));
    if (index < m_overlays.size())
    { m_overlays[index].x = x; m_overlays[index].y = y; }
    m_syncing = false;
}

void EmailQSLSettingsWidget::onEditorSizeChanged(int index, int w, int h)
{
    FCT_IDENTIFICATION;

    if (m_syncing || index < 0 || index >= m_overlayModel->rowCount()) return;
    m_syncing = true;
    auto *wi = m_overlayModel->item(index, OC_W);
    auto *hi = m_overlayModel->item(index, OC_H);
    if (wi && wi->text() != QStringLiteral("—")) wi->setText(QString::number(w));
    if (hi && hi->text() != QStringLiteral("—")) hi->setText(QString::number(h));
    if (index < m_overlays.size())
    { m_overlays[index].width = w; m_overlays[index].height = h; }
    m_syncing = false;
}

void EmailQSLSettingsWidget::onEditorOverlaySelected(int index)
{
    if (index >= 0)
        ui->overlayTableView->selectRow(index);
    else
        ui->overlayTableView->clearSelection();
    ui->removeOverlayButton->setEnabled(index >= 0);
}

// ---------------------------------------------------------------------------
// Slots: image
// ---------------------------------------------------------------------------

void EmailQSLSettingsWidget::browseCardImage()
{
    FCT_IDENTIFICATION;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select QSL Card Image"),
        ui->cardImagePathEdit->text(),
        tr("Images (*.jpg *.jpeg *.png *.bmp);;All files (*)"));
    if (!path.isEmpty())
        ui->cardImagePathEdit->setText(path);
}

void EmailQSLSettingsWidget::onCardImageChanged(const QString &path)
{
    FCT_IDENTIFICATION;

    QPixmap pm(path);

    // If there are existing overlays and a previous image, rescale all stored
    // pixel values (positions, font sizes, box dimensions) proportionally so
    // the layout stays in the same visual position on the new image.
    const QPixmap &prev = ui->cardEditorWidget->image();
    if (!prev.isNull() && !pm.isNull()
        && prev.size() != pm.size())
    {
        overlayTableToList(); // make sure m_overlays is current

        if (!m_overlays.isEmpty())
        {
            const double sx = static_cast<double>(pm.width())  / prev.width();
            const double sy = static_cast<double>(pm.height()) / prev.height();

            for (EmailQSLFieldOverlay &ov : m_overlays)
            {
                ov.x        = qRound(ov.x * sx);
                ov.y        = qRound(ov.y * sy);
                // Font size tracks horizontal scale (same axis as card width)
                ov.fontSize = qMax(1, qRound(ov.fontSize * sx));
                if (ov.type == QLatin1String("BOX"))
                {
                    ov.width  = qMax(20, qRound(ov.width  * sx));
                    ov.height = qMax(10, qRound(ov.height * sy));
                }
            }

            listToOverlayTable();
        }
    }

    ui->cardEditorWidget->setImage(pm);
}

// ---------------------------------------------------------------------------
// Slots: overlays
// ---------------------------------------------------------------------------

void EmailQSLSettingsWidget::addOverlay()
{
    FCT_IDENTIFICATION;

    // Default position: top-left area of image (or 50,50 if no image)
    const QPixmap &img = ui->cardEditorWidget->image();
    const int cx = img.isNull() ? 50 : img.width()  / 2;
    const int cy = img.isNull() ? 50 : img.height() / 4;

    overlayTableToList();
    m_overlays.append(makeOverlay(QStringLiteral("CALLSIGN"), cx, cy, 14, false));
    listToOverlayTable();

    const int newRow = m_overlayModel->rowCount() - 1;
    ui->overlayTableView->setCurrentIndex(m_overlayModel->index(newRow, 0));
    ui->overlayTableView->scrollToBottom();
    ui->cardEditorWidget->setSelectedIndex(newRow);
}

void EmailQSLSettingsWidget::addBox()
{
    FCT_IDENTIFICATION;

    const QPixmap &img = ui->cardEditorWidget->image();
    const int W = img.isNull() ? 1000 : img.width();
    const int H = img.isNull() ? 700  : img.height();

    EmailQSLFieldOverlay ov;
    ov.type         = QStringLiteral("BOX");
    ov.fieldName    = QString();          // no caption by default
    ov.x            = qRound(W * 0.05);
    ov.y            = qRound(H * 0.60);
    ov.width        = qRound(W * 0.25);
    ov.height       = qRound(H * 0.22);
    ov.fillColor    = QStringLiteral("#FFFF99");
    ov.color        = QStringLiteral("#888800");
    ov.opacity      = 80;
    ov.cornerRadius = qMax(4, qRound(H * 0.02));
    ov.fontSize     = 0;  // no caption

    overlayTableToList();
    m_overlays.append(ov);
    listToOverlayTable();

    const int newRow = m_overlayModel->rowCount() - 1;
    ui->overlayTableView->setCurrentIndex(m_overlayModel->index(newRow, 0));
    ui->overlayTableView->scrollToBottom();
    ui->cardEditorWidget->setSelectedIndex(newRow);
}

void EmailQSLSettingsWidget::addLabel()
{
    FCT_IDENTIFICATION;

    const QPixmap &img = ui->cardEditorWidget->image();
    const int cx = img.isNull() ? 50 : img.width()  / 2;
    const int cy = img.isNull() ? 50 : img.height() / 4;

    EmailQSLFieldOverlay ov;
    ov.type       = QStringLiteral("LABEL");
    ov.fieldName  = tr("Label Text");
    ov.x          = cx;
    ov.y          = cy;
    ov.fontFamily = QStringLiteral("Arial");
    ov.fontSize   = 14;
    ov.color      = QStringLiteral("#000000");
    ov.bold       = false;
    ov.italic     = false;

    overlayTableToList();
    m_overlays.append(ov);
    listToOverlayTable();

    const int newRow = m_overlayModel->rowCount() - 1;
    ui->overlayTableView->setCurrentIndex(m_overlayModel->index(newRow, 0));
    ui->overlayTableView->scrollToBottom();
    ui->cardEditorWidget->setSelectedIndex(newRow);
}

void EmailQSLSettingsWidget::addDefaultOverlays()
{
    FCT_IDENTIFICATION;

    const QPixmap &img = ui->cardEditorWidget->image();
    const int W = img.isNull() ? 1280 : img.width();
    const int H = img.isNull() ? 896  : img.height();

    // Font sizes are designed for a 1280-px-wide reference card and scaled
    // linearly to the actual image width so they always look proportional.
    // The same scale is applied to horizontal positions; vertical positions
    // use the image height fraction directly.
    static constexpr double REF_W = 1280.0;
    const double fs = W / REF_W;          // font scale (also used for x/horizontal)

    auto scalePt = [&](int refPt) { return qMax(1, qRound(refPt * fs)); };

    // Helper lambda to make a LABEL (static text) overlay
    auto makeLabel = [&](const QString &text, int x, int y, int refFontPt, bool bold,
                         const QString &color = QStringLiteral("#333333")) -> EmailQSLFieldOverlay
    {
        EmailQSLFieldOverlay ov;
        ov.type       = QStringLiteral("LABEL");
        ov.fieldName  = text;
        ov.x          = x;
        ov.y          = y;
        ov.fontSize   = scalePt(refFontPt);
        ov.bold       = bold;
        ov.fontFamily = QStringLiteral("Arial");
        ov.color      = color;
        return ov;
    };

    // Reference font sizes at 1280 px width
    const int refCallsignPt = 80;   // MY_CALLSIGN prominent header
    const int refDxCallPt   = 40;   // DX station callsign
    const int refDataPt     = 20;   // QSO data values
    const int refLabelPt    = 12;   // small caption labels above each field

    // Positions as fractions of card dimensions — same as before
    const int labelOffY = qRound(H * 0.045);

    const int rowName = qRound(H * 0.60);
    const int rowCall = qRound(H * 0.72);
    const int rowData = qRound(H * 0.84);

    const int col1    = qRound(W * 0.08);
    const int col2    = qRound(W * 0.35);
    const int col3    = qRound(W * 0.55);
    const int col4    = qRound(W * 0.68);
    const int col5    = qRound(W * 0.80);
    const int col6    = qRound(W * 0.90);
    const int colGrid = qRound(W * 0.40);

    const QList<EmailQSLFieldOverlay> defaults = {
        // My callsign centred near top — no label needed
        makeOverlay("MY_CALLSIGN", qRound(W * 0.50), qRound(H * 0.18), scalePt(refCallsignPt), true),

        // Name row
        makeLabel(tr("Name:"),         col1, rowName - labelOffY, refLabelPt, false),
        makeOverlay("NAME",            col1, rowName,              scalePt(refDataPt),   false),

        // Callsign + grid row
        makeLabel(tr("Callsign:"),     col1, rowCall - labelOffY, refLabelPt, false),
        makeOverlay("CALLSIGN",        col1, rowCall,              scalePt(refDxCallPt), true),
        makeLabel(tr("Grid:"),         colGrid, rowCall - labelOffY, refLabelPt, false),
        makeOverlay("GRIDSQUARE",      colGrid, rowCall,              scalePt(refDataPt), false),

        // QSO detail row
        makeLabel(tr("Date:"),         col1, rowData - labelOffY, refLabelPt, false),
        makeOverlay("QSO_DATE",        col1, rowData,              scalePt(refDataPt),  false),
        makeLabel(tr("Time (UTC):"),   col2, rowData - labelOffY, refLabelPt, false),
        makeOverlay("TIME_ON",         col2, rowData,              scalePt(refDataPt),  false),
        makeLabel(tr("Band:"),         col3, rowData - labelOffY, refLabelPt, false),
        makeOverlay("BAND",            col3, rowData,              scalePt(refDataPt),  false),
        makeLabel(tr("Mode:"),         col4, rowData - labelOffY, refLabelPt, false),
        makeOverlay("MODE",            col4, rowData,              scalePt(refDataPt),  false),
        makeLabel(tr("RST Snt:"),      col5, rowData - labelOffY, refLabelPt, false),
        makeOverlay("RST_SENT",        col5, rowData,              scalePt(refDataPt),  false),
        makeLabel(tr("RST Rcvd:"),     col6, rowData - labelOffY, refLabelPt, false),
        makeOverlay("RST_RCVD",        col6, rowData,              scalePt(refDataPt),  false),
    };

    overlayTableToList();
    for (const EmailQSLFieldOverlay &ov : defaults)
        m_overlays.append(ov);
    listToOverlayTable();
    ui->overlayTableView->scrollToBottom();
}

void EmailQSLSettingsWidget::removeOverlay()
{
    FCT_IDENTIFICATION;

    const QModelIndexList sel = ui->overlayTableView->selectionModel()->selectedRows();
    if (sel.isEmpty())
        return;

    overlayTableToList();
    m_overlays.removeAt(sel.first().row());
    listToOverlayTable();
}

// ---------------------------------------------------------------------------
// Full-size preview — renders using current UI state (not QSettings)
// ---------------------------------------------------------------------------

QPixmap EmailQSLSettingsWidget::renderPreviewPixmap(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    overlayTableToList();
    return EmailQSLBase::renderCard(ui->cardImagePathEdit->text().trimmed(),
                                    record, m_overlays);
}

void EmailQSLSettingsWidget::previewCard()
{
    FCT_IDENTIFICATION;

    const QSqlRecord dummy = buildDummyRecord();
    const QPixmap preview  = renderPreviewPixmap(dummy);

    if (preview.isNull())
    {
        QMessageBox::warning(this, tr("Preview"),
                             tr("Could not load the card image.\n"
                                "Please check the path and make sure the file exists."));
        return;
    }

    QDialog *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Card Preview (test data)"));
    QVBoxLayout *lay = new QVBoxLayout(dlg);

    QScrollArea *scroll = new QScrollArea(dlg);
    QLabel *lbl = new QLabel(scroll);
    const QPixmap scaled = preview.scaled(
        QSize(800, 600), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    lbl->setPixmap(scaled);
    lbl->adjustSize();
    scroll->setWidget(lbl);
    scroll->setMinimumSize(qMin(scaled.width()  + 20, 820),
                           qMin(scaled.height() + 20, 620));
    lay->addWidget(scroll);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    QPushButton *saveBtn = bb->addButton(tr("Save Card…"), QDialogButtonBox::ActionRole);

    connect(saveBtn, &QPushButton::clicked, this, [preview, this]()
    {
        const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        const QString path = QFileDialog::getSaveFileName(
            this,
            tr("Save QSL Card"),
            defaultDir + QStringLiteral("/QSL_preview.png"),
            tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg)"));

        if (path.isEmpty())
            return;

        if (!preview.save(path))
        {
            QMessageBox::warning(this, tr("Save Failed"),
                                 tr("Could not save the card image to:\n%1").arg(path));
        }
    });

    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    lay->addWidget(bb);
    dlg->show(); // non-modal
}

// ---------------------------------------------------------------------------
// SMTP
// ---------------------------------------------------------------------------

void EmailQSLSettingsWidget::onSmtpPresetChanged(int index)
{
    FCT_IDENTIFICATION;

    const QList<SmtpPreset> presets = smtpPresets();
    if (index <= 0 || index >= presets.size())
        return;

    const SmtpPreset &p = presets.at(index);
    if (!p.host.isEmpty())
    {
        ui->smtpHostEdit->setText(p.host);
        ui->smtpPortSpin->setValue(p.port);
        ui->encryptionCombo->setCurrentIndex(p.encryption);
    }
    if (!p.hint.isEmpty())
        QMessageBox::information(this, tr("Provider Notes"), p.hint);
}

void EmailQSLSettingsWidget::testConnection()
{
    FCT_IDENTIFICATION;

    ui->testResultLabel->setText(tr("Testing…"));
    ui->testConnectionButton->setEnabled(false);

    m_testService->testConnection(
        ui->smtpHostEdit->text().trimmed(),
        ui->smtpPortSpin->value(),
        ui->encryptionCombo->currentIndex(),
        ui->smtpUsernameEdit->text().trimmed(),
        ui->smtpPasswordEdit->text());
}

void EmailQSLSettingsWidget::onTestFinished(bool success, const QString &message)
{
    FCT_IDENTIFICATION;

    ui->testConnectionButton->setEnabled(true);
    if (success)
    {
        ui->testResultLabel->setStyleSheet(QStringLiteral("color: green; font-weight: bold;"));
        ui->testResultLabel->setText(tr("Connection OK"));
    }
    else
    {
        ui->testResultLabel->setStyleSheet(QStringLiteral("color: red;"));
        ui->testResultLabel->setText(tr("Failed: %1").arg(message));
    }
}

// ---------------------------------------------------------------------------
// Template insertion
// ---------------------------------------------------------------------------

void EmailQSLSettingsWidget::insertMergeFieldBody()
{
    FCT_IDENTIFICATION;

    const QString text = ui->bodyFieldCombo->currentText();
    if (!text.isEmpty())
        ui->bodyTemplateEdit->insertPlainText(text);
}

void EmailQSLSettingsWidget::insertMergeFieldSubject()
{
    FCT_IDENTIFICATION;

    const QString text = ui->subjectFieldCombo->currentText();
    if (!text.isEmpty())
        ui->subjectTemplateEdit->insert(text);
}
