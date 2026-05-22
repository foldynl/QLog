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

class SunTimelineWidget : public QWidget
{
public:
    explicit SunTimelineWidget(QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void setSunTimes(const QTime &sunrise, const QTime &sunset);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QTime sunrise;
    QTime sunset;
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
    Ui::ClockWidget *ui;
    QScopedPointer<QGraphicsScene> clockScene;
    QScopedPointer<QGraphicsTextItem> clockItem;
    LogLocale locale;
    QTime sunrise;
    QTime sunset;
};

#endif // QLOG_UI_CLOCKWIDGET_H
