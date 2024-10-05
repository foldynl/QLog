#include "FreqQSpinBox.h"
#include <QKeyEvent>
#include "data/BandPlan.h"

FreqQSpinBox::FreqQSpinBox(QWidget *parent) :
    QDoubleSpinBox(parent)
{
    loadBands();
}

void FreqQSpinBox::loadBands()
{
    enabledBands = BandPlan::bandsList(false, true);
}

void FreqQSpinBox::keyPressEvent(QKeyEvent *event)
{    
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

    QDoubleSpinBox::keyPressEvent(event);
}

void FreqQSpinBox::wheelEvent(QWheelEvent *event)
{
    if ( event->modifiers() & Qt::ControlModifier )
    {
        if ( event->angleDelta().y() > 0 )
            increaseByBand();
        else
            decreaseByBand();
        event->accept();
        return;
    }
    QDoubleSpinBox::wheelEvent(event);
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
            selectAll();
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
    selectAll();
}
