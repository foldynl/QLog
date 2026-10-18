#include "ui/component/FreqQSpinBox.h"
#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPalette>
#include <QSignalBlocker>
#include <QStyle>
#include <QTextLayout>
#include <QToolButton>
#include <QWidgetAction>
#include <QtMath>
#include "data/BandPlan.h"
#include "rig/macros.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.component.freqqspinbox");

FreqQSpinBox::FreqQSpinBox(QWidget *parent) :
    BaseDoubleSpinBox(parent),
    selectionModeEnabled(true)
{
    loadBands();

    debounceTimer.setSingleShot(true);
    connect(&debounceTimer, &QTimer::timeout,
            this, &FreqQSpinBox::flushDebounced);
    connect(this, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &FreqQSpinBox::onValueChangedImmediate);
}

void FreqQSpinBox::setSelectionModeEnabled(bool enabled)
{
    selectionModeEnabled = enabled;

    if ( !enabled )
        if (auto le = lineEdit()) le->deselect();
}

void FreqQSpinBox::setDebounceEnabled(bool enabled)
{
    if ( debounceEnabled == enabled ) return;

    debounceEnabled = enabled;

    if ( !debounceEnabled )
    {
        if ( debounceTimer.isActive() )debounceTimer.stop();
        flushDebounced();
    }
}

void FreqQSpinBox::setMousePositionStepEnabled(bool enabled)
{
    FCT_IDENTIFICATION;

    mousePositionStepEnabled = enabled;
}

void FreqQSpinBox::setDebounceIntervalMs(int ms)
{
    debounceMs = qMax(0, ms);
}

void FreqQSpinBox::cancelPendingValueChange()
{
    FCT_IDENTIFICATION;

    debounceTimer.stop();
    hasPending = false;
}

void FreqQSpinBox::enableBandSelection()
{
    FCT_IDENTIFICATION;

    if ( bandMenuButton )
        return;

    bandMenu = new QMenu(this);
    connect(bandMenu, &QMenu::triggered,
            this, &FreqQSpinBox::bandMenuTriggered);

    // Let the style paint the arrow using the current palette instead of a fixed icon.
    bandMenuButton = new QWidgetAction(lineEdit());
    QToolButton *button = new QToolButton(lineEdit());
    button->setArrowType(Qt::DownArrow);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::ArrowCursor);
    button->setDefaultAction(bandMenuButton);
    bandMenuButton->setDefaultWidget(button);
    lineEdit()->addAction(bandMenuButton, QLineEdit::TrailingPosition);
    bandMenuButton->setText(tr("Select Band or Frequency"));
    bandMenuButton->setToolTip(tr("Select Band or Frequency"));
    connect(bandMenuButton, &QAction::triggered,
            this, &FreqQSpinBox::showBandMenu);

    populateBandMenu();
    updateButtonSymbols();
    updateOutOfBandState();
}

void FreqQSpinBox::setBandSelectionEnabled(bool enabled)
{
    bandSelectionEnabled = enabled;

    if ( bandMenuButton )
        bandMenuButton->setEnabled(enabled);

    if ( !enabled && bandMenu )
        bandMenu->close();
}

bool FreqQSpinBox::setBand(const QString &newBandName)
{
    if ( newBandName.isEmpty() )
    {
        enterFrequencyMode();
        return true;
    }

    const int newBandIndex = findBand(newBandName);
    if ( newBandIndex < 0 )
        return false;

    if ( hasFrequency() )
        lastFrequency = value();

    const Band &band = enabledBands.at(newBandIndex);
    if ( BandPlan::freq2Band(lastFrequency).name != band.name )
        lastFrequency = band.start;

    {
        const QSignalBlocker blocker(this);
        bandIndex = newBandIndex;
        setSpecialValueText(band.name);
        QDoubleSpinBox::setValue(minimum());
    }

    updateButtonSymbols();
    updateOutOfBandState();
    emit bandSelected(band.name);
    return true;
}

void FreqQSpinBox::setValue(double newValue)
{
    if ( bandIndex >= 0 )
    {
        bandIndex = -1;
        setSpecialValueText(QString());
        updateButtonSymbols();
    }

    if ( newValue > minimum() )
        lastFrequency = newValue;

    QDoubleSpinBox::setValue(newValue);
}

bool FreqQSpinBox::hasBand() const
{
    return bandIndex >= 0;
}

bool FreqQSpinBox::hasFrequency() const
{
    return !hasBand() && value() > minimum();
}

double FreqQSpinBox::displayedFrequency() const
{
    FCT_IDENTIFICATION;

    // No-focus actions can read the editor before Qt commits keyboard input.
    QString input = text();
    if ( hasBand() && input == selectedBand() )
        return 0.0;

    int position = lineEdit()->cursorPosition();
    if ( validate(input, position) != QValidator::Acceptable )
        return 0.0;

    return valueFromText(input);
}

QString FreqQSpinBox::selectedBand() const
{
    return bandIndex >= 0 && bandIndex < enabledBands.size()
            ? enabledBands.at(bandIndex).name
            : BandPlan::freq2Band(value()).name;
}

void FreqQSpinBox::loadBands()
{
    const QString currentBand = bandIndex >= 0 ? selectedBand() : QString();
    enabledBands = BandPlan::bandsList(false, true);

    if ( !currentBand.isEmpty() )
    {
        const int newBandIndex = findBand(currentBand);
        if ( newBandIndex >= 0 )
            bandIndex = newBandIndex;
        else
            setValue(lastFrequency);
    }
}

QValidator::State FreqQSpinBox::validate(QString &input, int &pos) const
{
    if ( bandIndex >= 0 && input == selectedBand() )
        return QValidator::Acceptable;

    if ( bandMenuButton && containsLettersOutsideSuffix(input) )
        return QValidator::Intermediate;

    return BaseDoubleSpinBox::validate(input, pos);
}

double FreqQSpinBox::valueFromText(const QString &text) const
{
    return bandMenuButton && containsLettersOutsideSuffix(text)
            ? value()
            : BaseDoubleSpinBox::valueFromText(text);
}

QAbstractSpinBox::StepEnabled FreqQSpinBox::stepEnabled() const
{
    if ( bandIndex < 0 )
        return BaseDoubleSpinBox::stepEnabled();

    if ( !bandSelectionEnabled )
        return QAbstractSpinBox::StepNone;

    QAbstractSpinBox::StepEnabled enabled = QAbstractSpinBox::StepNone;

    if ( bandIndex > 0 )
        enabled |= QAbstractSpinBox::StepDownEnabled;
    if ( bandIndex < enabledBands.size() - 1 )
        enabled |= QAbstractSpinBox::StepUpEnabled;

    return enabled;
}

void FreqQSpinBox::keyPressEvent(QKeyEvent *event)
{
    if ( isReadOnly() )
    {
        BaseDoubleSpinBox::keyPressEvent(event);
        return;
    }

    if ( event->key() == Qt::Key_PageUp )
    {
        if ( bandIndex >= 0 )
            stepBand(1);
        else
            increaseByBand();
        event->accept();
        return;
    }
    else if ( event->key() == Qt::Key_PageDown )
    {
        if ( bandIndex >= 0 )
            stepBand(-1);
        else
            decreaseByBand();
        event->accept();
        return;
    }

    BaseDoubleSpinBox::keyPressEvent(event);
}

void FreqQSpinBox::wheelEvent(QWheelEvent *event)
{
    if ( isReadOnly() )
    {
        BaseDoubleSpinBox::wheelEvent(event);
        return;
    }

    if ( event->modifiers() & Qt::ControlModifier )
    {
        if ( event->angleDelta().y() == 0 )
        {
            event->ignore();
            return;
        }

        const int direction = event->angleDelta().y() > 0 ? 1 : -1;
        if ( bandIndex >= 0 )
            stepBand(direction);
        else if ( direction > 0 )
            increaseByBand();
        else
            decreaseByBand();
        event->accept();
        return;
    }
    const double originalStep = singleStep();
    double wheelStep = 0.0;
    if ( mousePositionStepEnabled && bandIndex < 0 )
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        wheelStep = frequencyStepAt(event->position().toPoint());
#else
        wheelStep = frequencyStepAt(event->pos());
#endif
    }
    if ( wheelStep > 0.0 )
        setSingleStep(wheelStep);

    BaseDoubleSpinBox::wheelEvent(event);

    if ( wheelStep > 0.0 )
        setSingleStep(originalStep);
}

double FreqQSpinBox::frequencyStepAt(const QPoint &position) const
{
    FCT_IDENTIFICATION;

    const QLineEdit *editor = lineEdit();
    const QPoint editorPosition = editor->mapFrom(this, position);
    if ( !editor->contentsRect().marginsRemoved(editor->textMargins()).contains(editorPosition) )
        return 0.0;

    const QString text = editor->displayText();
    QTextLayout layout(text, editor->font(), editor);
    QTextOption option;
    option.setTextDirection(editor->layoutDirection());
    option.setFlags(QTextOption::IncludeTrailingSpaces);
    layout.setTextOption(option);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    line.setNumColumns(text.size());
    layout.endLayout();

    // The input-method caret rectangle follows the editor's actual alignment and scrolling.
    const QRectF caret = editor->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    const int caretWidth = editor->style()->pixelMetric(QStyle::PM_TextCursorWidth, nullptr, editor);
    const qreal originX = caret.center().x() - (caretWidth - 1) / 2.0
                         - qRound(line.cursorToX(editor->cursorPosition()));
    if ( !QRectF(originX, caret.top(), line.naturalTextWidth(), line.height()).contains(editorPosition) )
        return 0.0;

    const int index = line.xToCursor(editorPosition.x() - originX, QTextLine::CursorOnCharacter);
    const int numberEnd = text.size() - suffix().size();
    if ( index < prefix().size() || index >= numberEnd || !text.at(index).isDigit() )
        return 0.0;

    // Pasted text may contain fewer fractional digits than decimals().
    const int decimalIndex = text.indexOf(locale().decimalPoint(), prefix().size());
    int exponent = 0;
    for ( int i = prefix().size(); i < numberEnd; ++i )
    {
        if ( !text.at(i).isDigit() )
            continue;

        if ( i > index )
            ++exponent;
        if ( decimalIndex >= 0 && i > decimalIndex )
            --exponent;
    }

    return qPow(10.0, exponent);
}

void FreqQSpinBox::stepBy(int steps)
{
    if ( bandIndex >= 0 )
    {
        stepBand(steps);
        return;
    }

    BaseDoubleSpinBox::stepBy(steps);

    if ( !selectionModeEnabled )
        if (auto le = lineEdit()) le->deselect();
}

void FreqQSpinBox::onValueChangedImmediate(double v)
{
    if ( bandIndex >= 0 && v > minimum() )
    {
        bandIndex = -1;
        setSpecialValueText(QString());
        updateButtonSymbols();
    }

    if ( v > minimum() )
        lastFrequency = v;

    updateOutOfBandState();

    if ( !debounceEnabled )
    {
        emit debouncedValueChanged(v);
        return;
    }

    pendingValue = v;
    hasPending = true;

    if ( debounceMs == 0 )
    {
        flushDebounced();
        return;
    }
    debounceTimer.start(debounceMs);
}

void FreqQSpinBox::flushDebounced()
{
    if ( !hasPending ) return;

    hasPending = false;
    emit debouncedValueChanged(pendingValue);
}

void FreqQSpinBox::showBandMenu()
{
    if ( !bandSelectionEnabled || !bandMenu )
        return;

    populateBandMenu();
    bandMenu->popup(mapToGlobal(QPoint(0, height())));
}

void FreqQSpinBox::populateBandMenu()
{
    bandMenu->clear();

    QAction *frequencyAction = bandMenu->addAction(tr("MHz"));
    frequencyAction->setCheckable(true);
    frequencyAction->setChecked(bandIndex < 0);
    frequencyAction->setData(QString());
    bandMenu->addSeparator();

    for ( int i = 0; i < enabledBands.size(); ++i )
    {
        QAction *action = bandMenu->addAction(enabledBands.at(i).name);
        action->setCheckable(true);
        action->setChecked(i == bandIndex);
        action->setData(enabledBands.at(i).name);
    }
}

void FreqQSpinBox::bandMenuTriggered(QAction *action)
{
    if ( !bandSelectionEnabled || !action || action->isSeparator() )
        return;

    setBand(action->data().toString());
}

void FreqQSpinBox::increaseByBand()
{
    if ( enabledBands.size() == 0 )
        return;

    for ( const Band &band : static_cast<const QList<Band>&>(enabledBands) )
    {
        if ( MHz2Hz(band.start) > MHz2Hz(value()) )
        {
            setValue(band.start);
            maybeSelectAll();
            return;
        }
    }
}

void FreqQSpinBox::decreaseByBand()
{
    if ( enabledBands.size() == 0 )
        return;

    double result = enabledBands.at(0).start;

    for ( const Band &band : static_cast<const QList<Band>&>(enabledBands) )
    {
        if ( MHz2Hz(band.start) < MHz2Hz(value()) )
            result = band.start;
    }

    setValue(result);
    maybeSelectAll();
}

void FreqQSpinBox::stepBand(int steps)
{
    if ( !bandSelectionEnabled || bandIndex < 0 || steps == 0 )
        return;

    const int nextIndex = qBound(0, bandIndex + steps, enabledBands.size() - 1);
    if ( nextIndex != bandIndex )
        setBand(enabledBands.at(nextIndex).name);
}

int FreqQSpinBox::findBand(const QString &bandName) const
{
    for ( int i = 0; i < enabledBands.size(); ++i )
    {
        if ( enabledBands.at(i).name.compare(bandName, Qt::CaseInsensitive) == 0 )
            return i;
    }

    return -1;
}

void FreqQSpinBox::maybeSelectAll()
{
    if ( !selectionModeEnabled ) return;

    selectAll();
}

void FreqQSpinBox::enterFrequencyMode()
{
    bandIndex = -1;
    setSpecialValueText(QString());
    updateButtonSymbols();
    QDoubleSpinBox::setValue(lastFrequency);

    setFocus();
    selectAll();
}

void FreqQSpinBox::updateButtonSymbols()
{
    setButtonSymbols(hasBand() ? QAbstractSpinBox::NoButtons
                               : QAbstractSpinBox::UpDownArrows);
}

void FreqQSpinBox::updateOutOfBandState()
{
    if ( !bandMenuButton )
        return;

    const bool outOfBand = hasFrequency() && selectedBand().isEmpty();
    QPalette palette = lineEdit()->palette();
    palette.setColor(QPalette::Text, outOfBand ? Qt::red
                                               : qApp->palette().text().color());
    lineEdit()->setPalette(palette);
    setToolTip(outOfBand ? tr("Frequency is outside a known band") : QString());
}

bool FreqQSpinBox::containsLettersOutsideSuffix(const QString &text) const
{
    QString numberText = text;
    if ( !suffix().isEmpty() && numberText.endsWith(suffix()) )
        numberText.chop(suffix().size());

    for ( const QChar character : numberText )
    {
        if ( character.isLetter() )
            return true;
    }

    return false;
}
