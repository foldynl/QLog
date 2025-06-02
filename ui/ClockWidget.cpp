#include <QTimer>
#include <QDateTime>
#include <QtMath>
#include "ClockWidget.h"
#include "ui_ClockWidget.h"
#include "core/debug.h"
#include "data/Gridsquare.h"
#include "data/StationProfile.h"

#define EARTH_RADIUS 6371
#define EARTH_CIRCUM 40075

#define MSECS_PER_DAY (24.0 * 60.0 * 60.0 * 1000.0)
MODULE_IDENTIFICATION("qlog.ui.clockwidget");

ClockWidget::ClockWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ClockWidget),
    sunScene(new QGraphicsScene(this)),
    clockScene(new QGraphicsScene(this)),
    clockItem(new QGraphicsTextItem)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ClockWidget::updateClock);
    timer->start(500);

    sunScene->setSceneRect(0, 0, 200, 10);
    ui->sunGraphicsView->setScene(sunScene.data());

    QFont font = clockItem->font();
    font.setPointSize(20);
    clockItem->setFont(font);
    clockScene->addItem(clockItem.data());
    ui->clockGraphicsView->setScene(clockScene.data());

    updateClock();
    updateSun();
    updateSunGraph();
}

void ClockWidget::updateClock()
{
    FCT_IDENTIFICATION;

    QDateTime now = QDateTime::currentDateTime().toTimeZone(QTimeZone::utc());
    QColor textColor = qApp->palette().color(QPalette::Text);
    clockItem->setDefaultTextColor(textColor);
    clockItem->setPlainText(locale.toString(now, locale.formatTimeLongWithoutTZ()));

    if (now.time().second() == 0)
    {
        updateSun();
        updateSunGraph();
    }

    /* Use only in case when you want to debug which widget is focussed*/
//    QWidget * fw = qApp->focusWidget();

//    if ( fw )
//    {
//        qInfo() << fw->objectName();
//    }
}

void ClockWidget::updateSun()
{
    FCT_IDENTIFICATION;

    Gridsquare myGrid (StationProfilesManager::instance()->getCurProfile1().locator);

    if ( myGrid.isValid() )
    {
        double lat = myGrid.getLatitude();
        double lon = myGrid.getLongitude();

        qint64 julianDay = QDate::currentDate().toJulianDay();
        double n = static_cast<double>(julianDay) - 2451545.0 + 0.0008;

        double Js = n - lon / 360.0;
        double M = fmod(357.5291 + 0.98560028 * Js, 360.0);
        double C = 1.9148 * sin(M / 180.0 * M_PI) + 0.0200 * sin(2 * M / 180.0 * M_PI) + 0.0003 * sin(3 * M / 180.0 * M_PI);
        double L = fmod(M + C + 180 + 102.9372, 360.0);
        double Jt = 2451545.0 + Js + 0.0053 * sin(M / 180.0 * M_PI) - 0.0069 * sin(2 * L / 180.0 * M_PI);
        double sind = sin(L / 180.0 * M_PI) * sin(23.44 / 180.0 * M_PI);
        double cosd = cos(asin(sind));
        double w = acos((sin(-0.83 / 180.0 * M_PI) - sin(lat / 180.0 * M_PI) * sind) / (cos(lat / 180.0 * M_PI) * cosd));

        double Jrise = Jt - w / (2*M_PI) + 0.5;
        double Jset = Jt + w / (2*M_PI) + 0.5;

        sunrise = QTime::fromMSecsSinceStartOfDay(static_cast<int>(fmod(Jrise, 1.0) * MSECS_PER_DAY));
        sunset = QTime::fromMSecsSinceStartOfDay(static_cast<int>(fmod(Jset, 1.0) * MSECS_PER_DAY));

        ui->sunRiseLabel->setText(locale.toString(sunrise, locale.formatTimeShort()));
        ui->sunSetLabel->setText(locale.toString(sunset, locale.formatTimeShort()));
    }
    else
    {
        ui->sunRiseLabel->setText(tr("N/A"));
        ui->sunSetLabel->setText(tr("N/A"));
    }
}

void ClockWidget::updateSunGraph()
{
    FCT_IDENTIFICATION;

    QColor dayColor(255, 253, 59);
    QColor nightColor(33, 150, 243);
    QColor currentColor(229, 57, 53);

    qreal width = sunScene->width();

    double rise = sunrise.msecsSinceStartOfDay() / MSECS_PER_DAY * width;
    double set = sunset.msecsSinceStartOfDay() / MSECS_PER_DAY * width;
    double cur = QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay() / MSECS_PER_DAY * width;

    sunScene->clear();

    if ( set > rise )
    {
        sunScene->addRect(0, 0, rise, 10, QPen(Qt::NoPen), QBrush(nightColor, Qt::SolidPattern));
        sunScene->addRect(rise, 0, set-rise, 10, QPen(Qt::NoPen), QBrush(dayColor, Qt::SolidPattern));
        sunScene->addRect(set, 0, width-set, 10, QPen(Qt::NoPen), QBrush(nightColor, Qt::SolidPattern));
    }
    else
    {
        sunScene->addRect(0, 0, set, 10, QPen(Qt::NoPen), QBrush(dayColor, Qt::SolidPattern));
        sunScene->addRect(set, 0, rise-set, 10, QPen(Qt::NoPen), QBrush(nightColor, Qt::SolidPattern));
        sunScene->addRect(rise, 0, width-rise, 10, QPen(Qt::NoPen), QBrush(dayColor, Qt::SolidPattern));
    }

    QPen currentPen(currentColor);
    currentPen.setWidthF(2.0);
    sunScene->addLine(cur, 0, cur, 10, currentPen);
}

ClockWidget::~ClockWidget()
{
    FCT_IDENTIFICATION;

    delete ui;
}
