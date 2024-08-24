#ifndef STEPPIR_H
#define STEPPIR_H

#include <QWidget>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>
#include <QTcpSocket>
#include <QTimer>
#include <data/BandPlan.h>>

namespace Ui {
class Steppir;
}

class Steppir : public QWidget
{
    Q_OBJECT

public:
    explicit Steppir(QWidget *parent = nullptr);
    ~Steppir();
    void openSerialPort();
    void closeSerialPort();
    void sendStatusRequest();
    void sendSetCommand(QString frequency, char direction, char steppircommand);
    void open();
    void close();

private slots:
    void readData();
    void parseResponse(const QByteArray &response);
    void pollSteppir();
    void onConnected();
    void onError();
    void UpdateForm();
    void on_buttonNormal_clicked();
    void on_butonReverse_clicked();
    void on_buttonBi_clicked();
    void on_buttonRetract_clicked();
    void on_buttonLeftFast_clicked();
    void on_butonLeftSlow_clicked();
    void on_pushAutoTrack_clicked();
    void on_buttonRightSlow_clicked();
    void on_buttonRightFast_clicked();
    void on_pb80_2_clicked();
    void on_pb40_2_clicked();
    void on_pb30_2_clicked();
    void on_pb20_2_clicked();
    void on_pb17_2_clicked();
    void on_pb15_2_clicked();
    void on_pb12_2_clicked();
    void on_pb10_2_clicked();
    void on_pb6_2_clicked();

    void on_buttonCalibrate_clicked();

private:
    Ui::Steppir *ui;
    QSerialPort *serial;
    QTcpSocket *socket;
    QTimer *timer;
    QByteArray buffer;
    double Frequency;
    QString Direction;
    QString OldDirection;
    bool AutoTracking;
    bool Tuning;
    QString band;
    QString baud;
    QString serialport;
    int networkport;
    QString networkhost;
    int pollint;
};

#endif // STEPPIR_H
