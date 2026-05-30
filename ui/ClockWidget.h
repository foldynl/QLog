#ifndef QLOG_UI_CLOCKWIDGET_H
#define QLOG_UI_CLOCKWIDGET_H

#include <QWidget>
#include <QTime>
#include <QScopedPointer>
#include <QGraphicsItem>

#include "core/LogLocale.h"

namespace Ui {
class ClockWidget;
}

class QGraphicsScene;
class QTimer;

class SunTimelineWidget : public QWidget
{
public:
    enum SunState
    {
        NoSunTimes,
        NormalSunTimes,
        AllDay,
        AllNight
    };

    explicit SunTimelineWidget(QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void setSunTimes(const QTime &sunrise, const QTime &sunset, SunState state);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QTime sunrise;
    QTime sunset;
    SunState sunState;
};

class ClockWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ClockWidget(QWidget *parent = nullptr);
    ~ClockWidget();

public slots:
    void updateClock();
    void updateSun();
    void updateSunGraph();

private:
    static QTime timeFromJulianDayFraction(double julianDay);
    void scheduleClockUpdate();

    Ui::ClockWidget *ui;
    QTimer *timer;
    QScopedPointer<QGraphicsScene> clockScene;
    QScopedPointer<QGraphicsTextItem> clockItem;
    LogLocale locale;
    QTime sunrise;
    QTime sunset;
    SunTimelineWidget::SunState sunState;
};

#endif // QLOG_UI_CLOCKWIDGET_H
