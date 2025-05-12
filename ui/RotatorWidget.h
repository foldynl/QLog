#ifndef QLOG_UI_ROTATORWIDGET_H
#define QLOG_UI_ROTATORWIDGET_H

#include <QWidget>
#include <QGraphicsPixmapItem>
#include <QPushButton>
#include "ui/NewContactWidget.h"

namespace Ui {
class RotatorWidget;
}

class QGraphicsScene;
class QGraphicsPathItem;

class RotatorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RotatorWidget(QWidget *parent = nullptr);
    ~RotatorWidget();
    void registerContactWidget(const NewContactWidget*);

signals:
    void rotProfileChanged();
    void rotUserButtonChanged();

public slots:
    void setBearing(double);
    void positionChanged(double, double);
    void redrawMap();
    void rotProfileComboChanged(QString);
    void rotUserButtonProfileComboChanged(QString);
    void reloadSettings();
    void rotConnected();
    void rotDisconnected();
    void refreshRotProfileCombo();
    void setQSOBearing(double, double);

protected:
    void showEvent(QShowEvent* event);
    void resizeEvent(QResizeEvent* event);
    virtual void mousePressEvent(QMouseEvent *event);

private slots:
    void userButton1();
    void userButton2();
    void userButton3();
    void userButton4();
    void gotoPosition();
    void qsoBearingLP();
    void qsoBearingSP();
    void setRequestedAz(double);

private:

    void refreshRotUserButtonProfileCombo();
    void refreshRotUserButtons();
    void setUserButtonDesc(QPushButton *button, const QString&, const double);
    double getQSOBearing();

    QGraphicsPathItem* antennaNeedle;
    QGraphicsPathItem* requestedAzimuthNeedle;
    QGraphicsPathItem* QSOAzimuthNeedle;
    bool waitingFirstValue;
    QGraphicsScene* compassScene;
    Ui::RotatorWidget *ui;
    double antennaAzimuth;
    double requestedAzimuth;
    double qsoAzimuth;
    const NewContactWidget *contact;

    const int MAP_RESOLUTION = 1000;
    const float GLOBE_RADIUS = 100.0;
    const int AZIMUTH_DEAD_BAND = 2;
};

#endif // QLOG_UI_ROTATORWIDGET_H
