#include "ui/component/FreqQSpinBox.h"
#include <QKeyEvent>
#include <QLineEdit>
#include "data/BandPlan.h"

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

void FreqQSpinBox::setDebounceIntervalMs(int ms)
{
    debounceMs = qMax(0, ms);
}

void FreqQSpinBox::loadBands()
{
    enabledBands = BandPlan::bandsList(false, true);
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
        increaseByBand();
        event->accept();
        return;
    }
    else if ( event->key() == Qt::Key_PageDown )
    {
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
        if ( event->angleDelta().y() > 0 )
            increaseByBand();
        else
            decreaseByBand();
        event->accept();
        return;
    }
    BaseDoubleSpinBox::wheelEvent(event);
}

void FreqQSpinBox::stepBy(int steps)
{
    BaseDoubleSpinBox::stepBy(steps);

    if ( !selectionModeEnabled )
        if (auto le = lineEdit()) le->deselect();
}

void FreqQSpinBox::onValueChangedImmediate(double v)
{
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

void FreqQSpinBox::increaseByBand()
{
    if ( enabledBands.size() == 0 )
        return;

    for ( const Band &band : static_cast<const QList<Band>&>(enabledBands) )
    {
        if ( band.start > value() )
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
        if ( band.start < value() )
            result = band.start;
    }

    setValue(result);
    maybeSelectAll();
}

void FreqQSpinBox::maybeSelectAll()
{
    if ( !selectionModeEnabled ) return;

    selectAll();
}
