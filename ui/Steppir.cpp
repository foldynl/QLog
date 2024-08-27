#include "Steppir.h"
#include "ui_Steppir.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTcpSocket>
#include <QDebug>
#include <QTimer>
#include <data/BandPlan.h>
#include <QSettings>

Steppir::Steppir(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Steppir)
{

    ui->setupUi(this);

    socket = new QTcpSocket(this);
    serial = new QSerialPort(this);


}

Steppir::~Steppir()
{
    closeSerialPort();
    delete ui;
}

void Steppir::open()
{
    QSettings settings;
    baud = settings.value("steppir/baudrate").toString() ;
    serialport = settings.value("steppir/portedit").toString();
    networkport = settings.value("steppir/netport").toInt();
    networkhost = settings.value("steppir/hostname").toString();
    pollint = settings.value("steppir/poll").toInt();



    if (serialport.length() > 5 )
    {
        connect(serial, &QSerialPort::readyRead, this, &Steppir::readData);
        openSerialPort();
    }

    if (networkhost.length() > 5){
        socket->connectToHost(networkhost, networkport);
        if (socket->state() != QAbstractSocket::ConnectingState && socket->state() != QAbstractSocket::ConnectedState) {
            qDebug() << "Connection failed: " << socket->errorString();
        }

        if (socket->waitForConnected(3000)) {
            qDebug() << "Connected to Steppir antenna.";
        } else {
            qDebug() << "Failed to connect to Steppir antenna: " << socket->errorString();
        }
        connect(socket, &QTcpSocket::readyRead, this, &Steppir::readData);
        connect(socket, &QTcpSocket::connected, this, &Steppir::onConnected);
        connect(socket, &QTcpSocket::errorOccurred, this, &Steppir::onError);
    }

    timer = new QTimer;
    connect(timer, &QTimer::timeout, this, &Steppir::pollSteppir);

    if (serial->isOpen() || serial->open(QIODevice::ReadWrite)) {
        timer->start(pollint);
    } else if (socket->state() == QAbstractSocket::ConnectedState) {
        timer->start(pollint);
    }
}

void Steppir::close()
{
    if (serial->isOpen() || serial->open(QIODevice::ReadWrite)) {
        serial->close();
    } else if (socket->state() == QAbstractSocket::ConnectedState) {
        socket->close();
    }
    else {
        qDebug() << "Failed to open serial or network port.";
    }
}

void Steppir::openSerialPort() {
    qDebug() << "In openSerialPort";
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        if(serialport ==info.systemLocation()) {
            serial->setPortName(serialport); // Set the correct COM port
            serial->setBaudRate(baud.toInt());
            serial->setDataBits(QSerialPort::Data8);
            serial->setParity(QSerialPort::NoParity);
            serial->setStopBits(QSerialPort::OneStop);
            serial->setFlowControl(QSerialPort::NoFlowControl);

            if (serial->open(QIODevice::ReadWrite)) {
                qDebug() << "Serial port opened";
            } else {
                serial->close();
                qDebug() << serial->errorString();
                qDebug() << "Failed to open serial port";
            }
        }

    }


}

void Steppir::closeSerialPort() {
    if (serial->isOpen()) {
        serial->close();
    }
}

void Steppir::onConnected(){
    qDebug() << "Successfully connected to the Steppir Antenna";
}

void Steppir::onError(){
    qDebug() << "Connection error " << socket->errorString();
}

void Steppir::sendStatusRequest() {
    QByteArray command = "?A\r";
    serial->write(command);
}

void Steppir::sendSetCommand(QString frequency, char direction, char steppircommand) {
    QByteArray command;
    command.append('@'); // ASCII @ (40 hex)
    command.append('A'); // ASCII A (41 hex)
    command.append(char(0x00)); // The zero byte

    // Frequency
    int freq = frequency.toInt() * 100; // Multiply by 10 as per the specification
    command.append((freq >> 16) & 0xFF); // Fh
    command.append((freq >> 8) & 0xFF);  // Fm
    command.append(freq & 0xFF);         // Fl

    command.append(char(0x00)); // ac field placeholder (ignored)
    command.append(direction);// direction byte, ensure 'direction' is a char or similar type
    command.append(steppircommand); // cmd placeholder
    command.append(char(0x00)); // cmd placeholder
    command.append(0x0D); // End of message with CR

    if (serial->isOpen() || serial->open(QIODevice::ReadWrite)) {
        serial->write(command);
        serial->flush();
    } else if (socket->state() == QAbstractSocket::ConnectedState) {
        // Write the command to the TCP socket
        socket->write(command);
        socket->flush();
    }
    else {
        qDebug() << "Failed to open serial or network port.";
    }
}

void Steppir::readData() {
    if (serial->isOpen() || serial->open(QIODevice::ReadWrite)) {
      buffer.append(serial->readAll());
    } else if (socket->state() == QAbstractSocket::ConnectedState) {
      buffer.append(socket->readAll());
    }


    int index;
    while ((index = buffer.indexOf('\r')) != -1) {
        QByteArray response = buffer.left(index + 1);
        parseResponse(response); // Process the message
        buffer.remove(0, index + 1); // Remove the processed bytes from the buffer
    }

    //    QByteArray responseData = serial->readAll();
 //   qDebug() << "Response: " << responseData;
    // Parse and process the response data
}

void Steppir::parseResponse(const QByteArray &response) {
    if (response.size() != 11) {
        qDebug() << "Invalid response size";
        return;
    }
    QString resp = response.toHex();
    bool ok;

    // Frequency
    int tfrequency = resp.mid(6,6).toUInt(&ok,16);
    Frequency = tfrequency / 100.0;

    // Active Motor Flags
    bool dvrActive = response[6] & 0x04;
    bool dir1Active = response[6] & 0x08;
    bool reflActive = response[6] & 0x10;
    bool dir2Active = response[6] & 0x20;

    qDebug() << "DVR Active:" << dvrActive;
    qDebug() << "DIR1 Active:" << dir1Active;
    qDebug() << "Reflector Active:" << reflActive;
    qDebug() << "DIR2 Active:" << dir2Active;

    // Direction
    switch ((response[7] & 0xe0) >> 5) {
    case 0x00:
        Direction = "Normal";
        break;
    case 0x02:
        Direction = "180";
        break;
    case 0x04:
        Direction = "Bi-directional";
        break;
    case 0x03:
        Direction = "3/4 wave";
        break;
    default:
        Direction = "Unknown";
    }
    qDebug() << "Direction:" << Direction;

    // Version Number
    QString version = QString("%1%2").arg(response[8]).arg(response[9]);

    Tuning = false;

    if (response[6] != 0x00){
        Tuning = true;
    }

    quint8 TrackFlag = 0;
    TrackFlag = response[7];
    TrackFlag = (TrackFlag & 0b00000100) >> 2;
    AutoTracking = TrackFlag;
    UpdateForm();
    qDebug() << "Version:" << version;
}


void Steppir::UpdateForm()
{

    QColor colGreen = QColor(Qt::blue);
    QString cssGreen = QString("background-color: %1").arg(colGreen.name());
    QColor colYellow = QColor(Qt::yellow);
    QString cssYellow = QString("background-color: %1").arg(colYellow.name());

    ui->frequencyLabel->setText(QString::number(Frequency,'f',2));
    if(Tuning){
        ui->frequencyLabel->setStyleSheet(cssYellow);
    }else
    {
        ui->frequencyLabel->setStyleSheet("");
    }

    if(AutoTracking){
        ui->pushAutoTrack->setStyleSheet(cssGreen);
    }
    else
    {
        ui->pushAutoTrack->setStyleSheet("");
    }

    QStringList Bands;
    Bands << "6m" << "10m" << "12m" << "15m" << "17m" << "20m" << "30m" << "40m" << "80m";

    QString curband = BandPlan::freq2Band(Frequency/1000).name;

    if (band != curband){
        band = curband;
        ui->pb10_2->setStyleSheet("");
        ui->pb12_2->setStyleSheet("");
        ui->pb15_2->setStyleSheet("");
        ui->pb17_2->setStyleSheet("");
        ui->pb20_2->setStyleSheet("");
        ui->pb30_2->setStyleSheet("");
        ui->pb40_2->setStyleSheet("");
        ui->pb80_2->setStyleSheet("");

        if (curband == "6m") {
            ui->pb6_2->setStyleSheet(cssGreen);
        } else if (curband == "10m") {
            ui->pb10_2->setStyleSheet(cssGreen);
        } else if (curband == "12m") {
            ui->pb12_2->setStyleSheet(cssGreen);
        } else if (curband == "15m") {
            ui->pb15_2->setStyleSheet(cssGreen);
        } else if (curband == "17m") {
            ui->pb17_2->setStyleSheet(cssGreen);
        } else if (curband == "20m") {
            ui->pb20_2->setStyleSheet(cssGreen);
        } else if (curband == "30m") {
            ui->pb30_2->setStyleSheet(cssGreen);
        } else if (curband == "40m") {
            ui->pb40_2->setStyleSheet(cssGreen);
        } else if (curband == "80m") {
            ui->pb80_2->setStyleSheet(cssGreen);
        }
    }
    if(Direction != OldDirection){
        ui->buttonNormal->setStyleSheet("");
        ui->butonReverse->setStyleSheet("");
        ui->buttonBi->setStyleSheet("");
        OldDirection = Direction;
        if ( Direction == "Normal") {
            ui->buttonNormal->setStyleSheet(cssGreen);
        } else if (Direction == "180") {
            ui->butonReverse->setStyleSheet(cssGreen);
        } else if (Direction =="Bi-directional") {
            ui->buttonBi->setStyleSheet(cssGreen);
        }
    }
}

void Steppir::pollSteppir()
{
    QByteArray command;
    command.append('?'); // Send the status request command
    command.append('A');
    command.append(char(0x0D)); // Append CR (0x0D)

    // Write the command to the serial port
    if (serial->isOpen() || serial->open(QIODevice::ReadWrite)) {
        serial->write(command);
        serial->flush();
    } else if (socket->state() == QAbstractSocket::ConnectedState) {
        socket->write(command);
        socket->flush();
    }
    else {
        qDebug() << "Steppir - Nothing is open";
    }
}



void Steppir::on_buttonNormal_clicked()
{
    sendSetCommand(QString::number(Frequency),char(0x00),char(0x00));
}


void Steppir::on_butonReverse_clicked()
{
    sendSetCommand(QString::number(Frequency),char(0x40),char(0x00));
}


void Steppir::on_buttonBi_clicked()
{
    sendSetCommand(QString::number(Frequency),char(0x80),char(0x00));
}


void Steppir::on_buttonRetract_clicked()
{
    sendSetCommand(QString::number(Frequency),char(0x00),char(0x53));
}


void Steppir::on_buttonLeftFast_clicked()
{
    char dir = char(0x00);

    if ( Direction == "Normal") {
        dir = char(0x00);
    } else if (Direction == "180") {
        dir = char(0x40);
    } else if (Direction =="Bi-directional") {
        dir = char(0x80);
    }

    //if(AutoTracking){
    //    sendSetCommand(QString::number(Frequency),dir,char(0x55));
   // }
    sendSetCommand(QString::number(Frequency-100),dir,char(0x00));
}


void Steppir::on_butonLeftSlow_clicked()
{
    char dir = char(0x00);

    if ( Direction == "Normal") {
        dir = char(0x00);
    } else if (Direction == "180") {
        dir = char(0x40);
    } else if (Direction =="Bi-directional") {
        dir = char(0x80);
    }

    //if(AutoTracking){
    //    sendSetCommand(QString::number(Frequency),dir,char(0x55));
   // }
    sendSetCommand(QString::number(Frequency-25),dir,char(0x00));
}


void Steppir::on_pushAutoTrack_clicked()
{
    char dir = char(0x00);

    if ( Direction == "Normal") {
        dir = char(0x00);
    } else if (Direction == "180") {
        dir = char(0x40);
    } else if (Direction =="Bi-directional") {
        dir = char(0x80);
    }

    if(AutoTracking){
        sendSetCommand(QString::number(Frequency),dir,char(0x55));

    } else {
        sendSetCommand(QString::number(Frequency),dir,char(0x52));
    }
}


void Steppir::on_buttonRightSlow_clicked()
{
    char dir = char(0x00);

    if ( Direction == "Normal") {
        dir = char(0x00);
    } else if (Direction == "180") {
        dir = char(0x40);
    } else if (Direction =="Bi-directional") {
        dir = char(0x80);
    }

    //if(AutoTracking){
    //    sendSetCommand(QString::number(Frequency),dir,char(0x55));
   // }
    sendSetCommand(QString::number(Frequency+25),dir,char(0x00));
}


void Steppir::on_buttonRightFast_clicked()
{
    char dir = char(0x00);

    if ( Direction == "Normal") {
        dir = char(0x00);
    } else if (Direction == "180") {
        dir = char(0x40);
    } else if (Direction =="Bi-directional") {
        dir = char(0x80);
    }

    //if(AutoTracking){
    //    sendSetCommand(QString::number(Frequency),dir,char(0x55));
   // }
    sendSetCommand(QString::number(Frequency+100),dir,char(0x00));
}


void Steppir::on_pb80_2_clicked()
{
    char dir = char(0x00);

    if ( Direction == "Normal") {
        dir = char(0x00);
    } else if (Direction == "180") {
        dir = char(0x40);
    } else if (Direction =="Bi-directional") {
        dir = char(0x80);
    }

  //  if(AutoTracking){
  //      sendSetCommand(QString::number(Frequency),dir,char(0x55));
  //  }
    sendSetCommand(QString::number(3500),dir,char(0x00));
}


void Steppir::on_pb40_2_clicked()
{
    char dir = char(0x00);

    if ( Direction == "Normal") {
        dir = char(0x00);
    } else if (Direction == "180") {
        dir = char(0x40);
    } else if (Direction =="Bi-directional") {
        dir = char(0x80);
    }

  //  if(AutoTracking){
   //     sendSetCommand(QString::number(Frequency),dir,char(0x55));
   // }
    sendSetCommand(QString::number(7000),dir,char(0x00));

}


void Steppir::on_pb30_2_clicked()
{
    char dir = char(0x00);

    if ( Direction == "Normal") {
        dir = char(0x00);
    } else if (Direction == "180") {
        dir = char(0x40);
    } else if (Direction =="Bi-directional") {
        dir = char(0x80);
    }

   // if(AutoTracking){
   //     sendSetCommand(QString::number(Frequency),dir,char(0x55));
   // }
    sendSetCommand(QString::number(10100),dir,char(0x00));

}


void Steppir::on_pb20_2_clicked()
{
    char dir = char(0x00);

    if ( Direction == "Normal") {
        dir = char(0x00);
    } else if (Direction == "180") {
        dir = char(0x40);
    } else if (Direction =="Bi-directional") {
        dir = char(0x80);
    }

   // if(AutoTracking){
   //     sendSetCommand(QString::number(Frequency),dir,char(0x55));
   // }
    sendSetCommand(QString::number(14000),dir,char(0x00));

}


void Steppir::on_pb17_2_clicked()
{
    char dir = char(0x00);

    if ( Direction == "Normal") {
        dir = char(0x00);
    } else if (Direction == "180") {
        dir = char(0x40);
    } else if (Direction =="Bi-directional") {
        dir = char(0x80);
    }

    //if(AutoTracking){
    //    sendSetCommand(QString::number(Frequency),dir,char(0x55));
   // }
    sendSetCommand(QString::number(18068),dir,char(0x00));

}


void Steppir::on_pb15_2_clicked()
{
    char dir = char(0x00);

    if ( Direction == "Normal") {
        dir = char(0x00);
    } else if (Direction == "180") {
        dir = char(0x40);
    } else if (Direction =="Bi-directional") {
        dir = char(0x80);
    }

   // if(AutoTracking){
   //     sendSetCommand(QString::number(Frequency),dir,char(0x55));
   // }
    sendSetCommand(QString::number(21000),dir,char(0x00));

}


void Steppir::on_pb12_2_clicked()
{
    char dir = char(0x00);

    if ( Direction == "Normal") {
        dir = char(0x00);
    } else if (Direction == "180") {
        dir = char(0x40);
    } else if (Direction =="Bi-directional") {
        dir = char(0x80);
    }

  //  if(AutoTracking){
  //      sendSetCommand(QString::number(Frequency),dir,char(0x55));
  //  }
    sendSetCommand(QString::number(24890),dir,char(0x00));

}


void Steppir::on_pb10_2_clicked()
{
    char dir = char(0x00);

    if ( Direction == "Normal") {
        dir = char(0x00);
    } else if (Direction == "180") {
        dir = char(0x40);
    } else if (Direction =="Bi-directional") {
        dir = char(0x80);
    }

  //  if(AutoTracking){
  //      sendSetCommand(QString::number(Frequency),dir,char(0x55));
  //  }
    sendSetCommand(QString::number(28000),dir,char(0x00));

}


void Steppir::on_pb6_2_clicked()
{
    char dir = char(0x00);

    if ( Direction == "Normal") {
        dir = char(0x00);
    } else if (Direction == "180") {
        dir = char(0x40);
    } else if (Direction =="Bi-directional") {
        dir = char(0x80);
    }

   // if(AutoTracking){
   //     sendSetCommand(QString::number(Frequency),dir,char(0x55));
   // }
    sendSetCommand(QString::number(50000),dir,char(0x00));

}


void Steppir::on_buttonCalibrate_clicked()
{
    sendSetCommand(QString::number(Frequency),char(0x00),char(0x56));
}

