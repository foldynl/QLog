#ifndef ROTATORWIDGET_H
#define ROTATORWIDGET_H

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
    void gotoPosition();
    void setBearing(double);
    void positionChanged(int, int);
    void redrawMap();
    void rotProfileComboChanged(QString);
    void rotUserButtonProfileComboChanged(QString);
    void reloadSettings();
    void rotConnected();
    void rotDisconnected();

protected:
    void showEvent(QShowEvent* event);
    void resizeEvent(QResizeEvent* event);


private slots:
    void qsoBearingClicked();
    void userButton1();
    void userButton2();
    void userButton3();
    void userButton4();

private:
    void refreshRotProfileCombo();
    void refreshRotUserButtonProfileCombo();
    void refreshRotUserButtons();
    void setUserButtonDesc(QPushButton *button, const QString&, const double);

    QGraphicsPathItem* compassNeedle;
    QGraphicsScene* compassScene;
    Ui::RotatorWidget *ui;
    int azimuth;
    const NewContactWidget *contact;
};

#endif // ROTATORWIDGET_H
