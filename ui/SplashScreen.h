#ifndef QLOG_UI_SPLASHSCREEN_H
#define QLOG_UI_SPLASHSCREEN_H

#include <QSplashScreen>
#include <QThread>
#include <QApplication>

class SplashScreen : public QSplashScreen
{
public:
    explicit SplashScreen(const QPixmap &pixmap = QPixmap()) : QSplashScreen(pixmap) {}

private:
    bool painted=false;


    void paintEvent(QPaintEvent* e) override
    {
        QSplashScreen::paintEvent(e);
        painted=true;
    }
public:
    void ensureFirstPaint() const
    {
        while(!painted)
        {
            QThread::usleep(1e3);
            qApp->processEvents();
        }
    }
};

#endif // QLOG_UI_SPLASHSCREEN_H
