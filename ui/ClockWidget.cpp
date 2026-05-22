#include <QTimer>
#include <QDateTime>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include "ClockWidget.h"
#include "ui_ClockWidget.h"
#include "core/debug.h"
#include "data/Gridsquare.h"
#include "data/StationProfile.h"

#define MSECS_PER_DAY (24.0 * 60.0 * 60.0 * 1000.0)
#define MSECS_PER_DAY_INT (24 * 60 * 60 * 1000)

MODULE_IDENTIFICATION("qlog.ui.clockwidget");

SunTimelineWidget::SunTimelineWidget(QWidget *parent) :
    QWidget(parent),
    sunState(NoSunTimes)
{
    setMinimumHeight(18);
}

QSize SunTimelineWidget::sizeHint() const
{
    return QSize(200, 18);
}

QSize SunTimelineWidget::minimumSizeHint() const
{
    return QSize(120, 18);
}

void SunTimelineWidget::setSunTimes(const QTime &newSunrise,
                                    const QTime &newSunset,
                                    SunState newSunState)
{
    sunrise = newSunrise;
    sunset = newSunset;
    sunState = newSunState;
    update();
}

void SunTimelineWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bar = rect().adjusted(4.0, 5.0, -4.0, -5.0);
    if ( bar.width() <= 0.0 || bar.height() <= 0.0 )
        return;

    const QColor nightColor(42, 68, 90);
    const QColor dayColor(255, 255, 0);
    const QColor twilightColor(232, 105, 72);
    const QColor currentColor(220, 60, 55);

    QPainterPath clipPath;
    clipPath.addRoundedRect(bar, bar.height() / 2.0, bar.height() / 2.0);

    painter.fillPath(clipPath, nightColor);
    painter.setClipPath(clipPath);

    const auto drawDaySegment = [&](qreal start, qreal end, bool sunriseAtStart, bool sunsetAtEnd)
    {
        if ( end <= start )
            return;

        const qreal width = end - start;
        const qreal edge = qMin(0.35, 9.0 / width);

        QLinearGradient gradient(start, 0.0, end, 0.0);
        gradient.setColorAt(0.0, sunriseAtStart ? twilightColor : dayColor);
        gradient.setColorAt(edge, dayColor);
        gradient.setColorAt(1.0 - edge, dayColor);
        gradient.setColorAt(1.0, sunsetAtEnd ? twilightColor : dayColor);

        painter.fillRect(QRectF(start, bar.top(), width, bar.height()), gradient);
    };

    if ( sunState == AllDay )
    {
        painter.fillRect(bar, dayColor);
    }
    else if ( sunState == NormalSunTimes && sunrise.isValid() && sunset.isValid() )
    {
        const qreal rise = bar.left() + sunrise.msecsSinceStartOfDay() / MSECS_PER_DAY * bar.width();
        const qreal set = bar.left() + sunset.msecsSinceStartOfDay() / MSECS_PER_DAY * bar.width();

        if ( set > rise )
        {
            drawDaySegment(rise, set, true, true);
        }
        else
        {
            drawDaySegment(bar.left(), set, false, true);
            drawDaySegment(rise, bar.right(), true, false);
        }
    }

    painter.setClipping(false);
    painter.setPen(QPen(palette().color(QPalette::Mid), 1.0));
    painter.drawPath(clipPath);

    const qreal current = bar.left()
                          + QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay()
                            / MSECS_PER_DAY * bar.width();
    QPen currentPen(currentColor);
    currentPen.setWidthF(1.5);
    painter.setPen(currentPen);
    painter.drawLine(QPointF(current, bar.top() - 3.0),
                     QPointF(current, bar.bottom() + 3.0));
}

ClockWidget::ClockWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ClockWidget),
    timer(new QTimer(this)),
    clockScene(new QGraphicsScene(this)),
    clockItem(new QGraphicsTextItem),
    sunState(SunTimelineWidget::NoSunTimes)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    timer->setSingleShot(true);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, &ClockWidget::updateClock);

    QFont font = clockItem->font();
    font.setPointSize(20);
    clockItem->setFont(font);
    clockScene->addItem(clockItem.data());
    ui->clockGraphicsView->setScene(clockScene.data());

    updateSun();
    updateSunGraph();
    updateClock();
}

void ClockWidget::scheduleClockUpdate()
{
    FCT_IDENTIFICATION;

    const QTime now = QDateTime::currentDateTimeUtc().time();
    int msecsToNextSecond = 1000 - now.msec();
    timer->start(msecsToNextSecond);
}

void ClockWidget::updateClock()
{
    FCT_IDENTIFICATION;

    QDateTime now = QDateTime::currentDateTime().toTimeZone(QTimeZone::utc());
    QColor textColor = qApp->palette().color(QPalette::Text);
    clockItem->setDefaultTextColor(textColor);
    clockItem->setPlainText(locale.toString(now, locale.formatTimeLongWithoutTZ()));

    if ( now.time().second() == 0 )
    {
        updateSun();
        updateSunGraph();
    }

    scheduleClockUpdate();

    /* Use only in case when you want to debug which widget is focussed*/
//#define SHOW_FOCUS
#ifdef SHOW_FOCUS
    QWidget * fw = qApp->focusWidget();
    if ( fw ) qInfo() << fw->objectName() << fw->parent()->objectName();
#endif

}

void ClockWidget::updateSun()
{
    FCT_IDENTIFICATION;

    Gridsquare myGrid (StationProfilesManager::instance()->getCurProfile1().locator);

    if ( myGrid.isValid() )
    {
        double lat = myGrid.getLatitude();
        double lon = myGrid.getLongitude();

        qint64 julianDay = QDateTime::currentDateTimeUtc().date().toJulianDay();
        double n = static_cast<double>(julianDay) - 2451545.0 + 0.0008;

        double Js = n - lon / 360.0;
        double M = fmod(357.5291 + 0.98560028 * Js, 360.0);
        double C = 1.9148 * sin(M / 180.0 * M_PI) + 0.0200 * sin(2 * M / 180.0 * M_PI) + 0.0003 * sin(3 * M / 180.0 * M_PI);
        double L = fmod(M + C + 180 + 102.9372, 360.0);
        double Jt = 2451545.0 + Js + 0.0053 * sin(M / 180.0 * M_PI) - 0.0069 * sin(2 * L / 180.0 * M_PI);
        double sind = sin(L / 180.0 * M_PI) * sin(23.44 / 180.0 * M_PI);
        double cosd = cos(asin(sind));
        const double cosHourAngleNumerator = sin(-0.83 / 180.0 * M_PI)
                                             - sin(lat / 180.0 * M_PI) * sind;
        const double cosHourAngleDenominator = cos(lat / 180.0 * M_PI) * cosd;
        double cosHourAngle = cosHourAngleNumerator / cosHourAngleDenominator;

        if ( !qIsFinite(cosHourAngle) )
            cosHourAngle = (cosHourAngleNumerator <= 0.0) ? -2.0 : 2.0;

        if ( cosHourAngle > 1.0 )
        {
            sunrise = QTime();
            sunset = QTime();
            sunState = SunTimelineWidget::AllNight;
            ui->sunRiseLabel->setText(tr("N/A"));
            ui->sunSetLabel->setText(tr("N/A"));
            return;
        }

        if ( cosHourAngle < -1.0 )
        {
            sunrise = QTime();
            sunset = QTime();
            sunState = SunTimelineWidget::AllDay;
            ui->sunRiseLabel->setText(tr("N/A"));
            ui->sunSetLabel->setText(tr("N/A"));
            return;
        }

        double w = acos(qBound(-1.0, cosHourAngle, 1.0));

        double Jrise = Jt - w / (2*M_PI) + 0.5;
        double Jset = Jt + w / (2*M_PI) + 0.5;

        sunrise = timeFromJulianDayFraction(Jrise);
        sunset = timeFromJulianDayFraction(Jset);
        sunState = SunTimelineWidget::NormalSunTimes;

        ui->sunRiseLabel->setText(locale.toString(sunrise, locale.formatTimeShort()));
        ui->sunSetLabel->setText(locale.toString(sunset, locale.formatTimeShort()));
    }
    else
    {
        ui->sunRiseLabel->setText(tr("N/A"));
        ui->sunSetLabel->setText(tr("N/A"));
        sunrise = QTime();
        sunset = QTime();
        sunState = SunTimelineWidget::NoSunTimes;
    }
}

QTime ClockWidget::timeFromJulianDayFraction(double julianDay)
{
    FCT_IDENTIFICATION;

    if ( !qIsFinite(julianDay) )
        return QTime();

    const double fraction = julianDay - qFloor(julianDay);
    qint64 msecs = qRound64(fraction * MSECS_PER_DAY);
    msecs %= MSECS_PER_DAY_INT;

    return QTime::fromMSecsSinceStartOfDay(static_cast<int>(msecs));
}

void ClockWidget::updateSunGraph()
{
    FCT_IDENTIFICATION;

    ui->sunTimelineWidget->setSunTimes(sunrise, sunset, sunState);
}

ClockWidget::~ClockWidget()
{
    FCT_IDENTIFICATION;

    delete ui;
}
