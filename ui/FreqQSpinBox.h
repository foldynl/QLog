#ifndef QLOG_UI_FREQQSPINBOX_H
#define QLOG_UI_FREQQSPINBOX_H

#include <QSpinBox>
#include <data/Band.h>

class FreqQSpinBox : public QDoubleSpinBox
{
public:
    FreqQSpinBox(QWidget *parent = nullptr);

public slots:
    void loadBands();

protected:
    void keyPressEvent(QKeyEvent *event);
    void wheelEvent(QWheelEvent *event);

private:
    void increaseByBand();
    void decreaseByBand();

    QList<Band> enabledBands;
};

#endif // QLOG_UI_FREQQSPINBOX_H
