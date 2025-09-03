#include <QMouseEvent>
#include <QApplication>
#include <QPainter>
#include <QProgressBar>
#include "RepeatButton.h"

RepeatButton::RepeatButton(QWidget *parent) :
    QPushButton(parent),
    timer(new QTimer(this)),
    interval(0),
    progress(0)
{   
    connect(timer, &QTimer::timeout, this, &RepeatButton::onTimeout);
    connect(&updateTimer, &QTimer::timeout, this, &RepeatButton::updateProgress);

    QProgressBar bar;
    filledColor = bar.palette().color(QPalette::Highlight);
    filledColor.setAlpha(128);
}

void RepeatButton::stop()
{
    timer->stop();
    updateTimer.stop();
    progress = 0;
    update();
}

void RepeatButton::handleClick()
{
    lastPressTime = QDateTime::currentDateTime();

    if ( !timer->isActive() )
    {
        interval = 0;
        QPushButton::click();
    }
    else
        stop();
}

void RepeatButton::resetInterval()
{
    lastPressTime = QDateTime();
    interval = 0;
}

void RepeatButton::repeatClick()
{
    if ( timer->isActive() ) return;

    repeateEventDetected();
    QPushButton::click();
}

void RepeatButton::repeateEventDetected()
{
    if ( interval == 0 )
    {
        if ( lastPressTime.isNull() ) return;
        interval = lastPressTime.msecsTo(QDateTime::currentDateTime());
    }
    startRepeating();
}

void RepeatButton::startRepeating()
{
    if ( interval > 0 )
    {
        timer->start(interval);
        updateTimer.start(20);
        elapsed.start();
    }
}

void RepeatButton::mousePressEvent(QMouseEvent *event)
{
    if ( event->button() == Qt::LeftButton )
    {
        if( QApplication::keyboardModifiers() & Qt::ShiftModifier )
            repeateEventDetected();
        else
        {
            handleClick();
            return;
        }
    }
    QPushButton::mousePressEvent(event);
}

void RepeatButton::paintEvent(QPaintEvent *event)
{
    QPushButton::paintEvent(event);

    if ( timer->isActive() )
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(filledColor);
        p.setPen(Qt::NoPen);

        int fillWidth = width() * progress / 100;
        QRect rect(0, 0, fillWidth, height());
        p.drawRect(rect);
    }
}

void RepeatButton::onTimeout()
{
    QPushButton::click();
    elapsed.restart();
}

void RepeatButton::updateProgress()
{
    if ( timer->isActive() )
    {
        int i_progress = elapsed.elapsed() * 100 / interval;
        progress = qMin(i_progress, 100);
        update();
    }
}


