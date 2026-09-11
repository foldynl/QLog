#include "QSOFilterDateRangeEdit.h"
#include "ui_QSOFilterDateRangeEdit.h"
#include "ui_QSOFilterDateBoundary.h"
#include "ui_QSOFilterDateRangeDialog.h"

#include <QSignalBlocker>
#include <QTimeZone>
#include "core/LogLocale.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.component.qsofilterdaterangeedit");

using Boundary = QSOFilterDateRange::Boundary;
using Anchor = Boundary::Anchor;

QSOFilterDateRangeEdit::QSOFilterDateRangeEdit(const QString &value, QWidget *parent)
    : QWidget(parent)
{
    FCT_IDENTIFICATION;

    Ui::QSOFilterDateRangeEdit ui;
    ui.setupUi(this);
    preset = ui.periodPreset;
    days = ui.periodDays;
    customButton = ui.customPeriodButton;
    setObjectName("valueDateRange");
    QSOFilterDateRange::fromString(value, range);

    // Display order and translated captions live in the .ui file.
    // Logic uses item data, never the displayed text.
    int index = 0;
    for ( auto id : {Preset::Today, Preset::Yesterday, Preset::LastDays, Preset::Week,
                     Preset::Month, Preset::Year, Preset::Custom} )
        preset->setItemData(index++, static_cast<int>(id));

    Q_ASSERT(index == preset->count());

    updateControls();

    connect(preset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QSOFilterDateRangeEdit::presetChanged);
    connect(days, QOverload<int>::of(&QSpinBox::valueChanged), this, &QSOFilterDateRangeEdit::presetChanged);
    connect(customButton, &QPushButton::clicked, this, &QSOFilterDateRangeEdit::editCustomRange);
}

void QSOFilterDateRangeEdit::updateControls()
{
    FCT_IDENTIFICATION;

    const QSignalBlocker presetBlocker(preset), daysBlocker(days);
    const Boundary from = Boundary::fromString(range.from);
    const Boundary to = Boundary::fromString(range.to);
    Preset selected = Preset::Custom;

    if ( from.isRelativeTo(Anchor::Today) && to.isRelativeTo(Anchor::Today) )
        selected = Preset::Today;
    else if ( from.isRelativeTo(Anchor::Today, -1) && to.isRelativeTo(Anchor::Today, -1) )
        selected = Preset::Yesterday;
    else if ( from.anchor == Anchor::Today && from.offsetDays < 0 && to.isRelativeTo(Anchor::Today) )
        selected = Preset::LastDays;
    else if ( from.isRelativeTo(Anchor::WeekStart) && to.isRelativeTo(Anchor::Today) )
        selected = Preset::Week;
    else if ( from.isRelativeTo(Anchor::MonthStart) && to.isRelativeTo(Anchor::Today) )
        selected = Preset::Month;
    else if ( from.isRelativeTo(Anchor::YearStart) && to.isRelativeTo(Anchor::YearEnd) )
        selected = Preset::Year;

    preset->setCurrentIndex(preset->findData(static_cast<int>(selected)));

    if ( selected == Preset::LastDays )
        days->setValue(static_cast<int>(qBound(days->minimum(), 1 - from.offsetDays, days->maximum())));

    days->setVisible(selected == Preset::LastDays);
    customButton->setVisible(selected == Preset::Custom);

    QDateTime start, end;
    if ( range.resolve(QDateTime::currentDateTimeUtc().date(), start, end) )
    {
        const LogLocale locale;
        customButton->setToolTip(tr("%1 – %2 UTC")
                                .arg(locale.toString(start, QLocale::ShortFormat),
                                     locale.toString(end.addSecs(-1), QLocale::ShortFormat)));
    }
}

void QSOFilterDateRangeEdit::presetChanged()
{
    FCT_IDENTIFICATION;

    const Preset selected = static_cast<Preset>(preset->currentData().toInt());
    Boundary from, to;

    switch ( selected )
    {
    case Preset::Today:
        break;
    case Preset::Yesterday:
        from = to = Boundary(Anchor::Today, -1);
        break;
    case Preset::LastDays:
        from.offsetDays = 1 - days->value();
        break;
    case Preset::Week:
        from.anchor = Anchor::WeekStart;
        break;
    case Preset::Month:
        from.anchor = Anchor::MonthStart;
        break;
    case Preset::Year:
        from.anchor = Anchor::YearStart;
        to.anchor = Anchor::YearEnd;
        break;
    case Preset::Custom:
        days->hide();
        customButton->show();
        editCustomRange();
        return;
    }
    range.from = from.toString();
    range.to = to.toString();
    days->setVisible(selected == Preset::LastDays);
    customButton->hide();
    emit valueChanged();
}

void QSOFilterDateRangeEdit::setupBoundaryEditor(const Boundary &boundary,
                                               Ui::QSOFilterDateBoundary &ui, QWidget *editor)
{
    FCT_IDENTIFICATION;

    ui.setupUi(editor);
    int index = 0;
    for ( auto id : {Anchor::Date, Anchor::DateTime, Anchor::Today, Anchor::WeekStart,
                     Anchor::MonthStart, Anchor::YearStart, Anchor::YearEnd} )
        ui.anchor->setItemData(index++, static_cast<int>(id));

    Q_ASSERT(index == ui.anchor->count());

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    ui.fixed->setTimeZone(QTimeZone::UTC);
#else
    ui.fixed->setTimeSpec(Qt::UTC);
#endif
    ui.fixed->setDateTime(boundary.fixedDateTime.isValid() ? boundary.fixedDateTime
                                                           : QDateTime::currentDateTimeUtc());
    ui.anchor->setCurrentIndex(ui.anchor->findData(static_cast<int>(boundary.anchor == Anchor::Invalid ? Anchor::DateTime
                                                                                                       : boundary.anchor)));
    ui.offset->setValue(boundary.offsetDays);

    connect(ui.anchor, QOverload<int>::of(&QComboBox::currentIndexChanged), editor, [ui]()
    {
        const Boundary selected(static_cast<Anchor>(ui.anchor->currentData().toInt()));
        const LogLocale locale;
        ui.fixed->setDisplayFormat(locale.formatDateShortWithYYYY()
                                   + (selected.anchor == Anchor::DateTime ? " " + locale.formatTimeLongWithoutTZ()
                                                                          : QString()));
        ui.fixed->setVisible(!selected.isRelative());
        ui.offset->setVisible(selected.isRelative());
    });

    update();
}

QString QSOFilterDateRangeEdit::boundaryValue(const Ui::QSOFilterDateBoundary &ui) const
{
    Boundary boundary(static_cast<Anchor>(ui.anchor->currentData().toInt()), ui.offset->value());
    boundary.fixedDateTime = ui.fixed->dateTime();
    return boundary.toString();
}

void QSOFilterDateRangeEdit::editCustomRange()
{
    FCT_IDENTIFICATION;

    QDialog dialog(this, window()->windowType() == Qt::Popup ? Qt::Popup : Qt::Dialog);
    Ui::QSOFilterDateRangeDialog ui;
    ui.setupUi(&dialog);
    ui.errorLabel->hide();
    Ui::QSOFilterDateBoundary fromEditor, toEditor;

    setupBoundaryEditor(Boundary::fromString(range.from), fromEditor, ui.fromBoundary);
    setupBoundaryEditor(Boundary::fromString(range.to), toEditor, ui.toBoundary);

    if ( dialog.windowType() == Qt::Popup ) dialog.move(mapToGlobal(QPoint(0, height())));

    connect(ui.buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(ui.buttonBox, &QDialogButtonBox::accepted, &dialog, [this, &dialog, &ui, &fromEditor, &toEditor]()
    {
        QSOFilterDateRange candidate;
        candidate.from = boundaryValue(fromEditor);
        candidate.to = boundaryValue(toEditor);
        QDateTime start, end;
        if ( !candidate.resolve(QDateTime::currentDateTimeUtc().date(), start, end) )
        {
            ui.errorLabel->show();
            return;
        }
        range = candidate;
        dialog.accept();
    });
    const bool accepted = dialog.exec() == QDialog::Accepted;
    updateControls();
    if ( accepted ) emit valueChanged();
}
