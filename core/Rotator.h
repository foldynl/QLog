#ifndef ROTATOR_H
#define ROTATOR_H

#include <QtCore>
#include <hamlib/rotator.h>
#include "data/RotProfile.h"

struct rot;

class Rotator : public QObject
{
    Q_OBJECT

public:
    static Rotator* instance();
    int getAzimuth();
    int getElevation();
    bool isRotConnected();

signals:
    void positionChanged(int azimuth, int elevation);
    void rotErrorPresent(QString);

public slots:
    void start();
    void update();
    void open();
    void close();

    void setPosition(int azimuth, int elevation);

private:
    Rotator(QObject *parent = nullptr);
    ~Rotator();

    void __closeRot();
    void __openRot();

    int azimuth;
    int elevation;

    ROT* rot;
    RotProfile connectedRotProfile;
    QMutex rotLock;
    QTimer* timer;
};

#endif // ROTATOR_H
