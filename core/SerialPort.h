#ifndef QLOG_CORE_SERIALPORT_H
#define QLOG_CORE_SERIALPORT_H

#include <QObject>
#include <hamlib/rig.h>

class SerialPort : public QObject
{
    Q_OBJECT
public:
    explicit SerialPort(QObject *parent = nullptr);

    static const QString SERIAL_FLOWCONTROL_NONE;
    static const QString SERIAL_FLOWCONTROL_SOFTWARE;
    static const QString SERIAL_FLOWCONTROL_HARDWARE;
    static const QString SERIAL_PARITY_EVEN;
    static const QString SERIAL_PARITY_ODD;
    static const QString SERIAL_PARITY_MARK;
    static const QString SERIAL_PARITY_SPACE;
    static const QString SERIAL_PARITY_NO;
};

#endif // QLOG_CORE_SERIALPORT_H
