#ifndef REPEATBUTTON_H
#define REPEATBUTTON_H

#include <QPushButton>
#include <QElapsedTimer>
#include <QTimer>
#include <QDateTime>

class RepeatButton : public QPushButton
{
    Q_OBJECT
public:
    RepeatButton(QWidget *parent = nullptr);

public slots:
    void repeatClick();
    void stop();
    void handleClick();
    void resetInterval();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onTimeout();
    void updateProgress();

private:
    void startRepeating();
    void repeateEventDetected();
    QTimer *timer;
    QTimer updateTimer;
    QDateTime lastPressTime;
    QElapsedTimer elapsed;
    int interval = 0;
    int progress = 0;
    QColor filledColor;
};

#endif // REPEATBUTTON_H
