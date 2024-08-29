#include "SerialPort.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.serialport");

SerialPort::SerialPort(QObject *parent)
    : QObject{parent}
{
    FCT_IDENTIFICATION;

}

const QString SerialPort::SERIAL_FLOWCONTROL_NONE = "none";
const QString SerialPort::SERIAL_FLOWCONTROL_HARDWARE = "hardware";
const QString SerialPort::SERIAL_FLOWCONTROL_SOFTWARE = "software";
const QString SerialPort::SERIAL_PARITY_EVEN = "even";
const QString SerialPort::SERIAL_PARITY_ODD = "odd";
const QString SerialPort::SERIAL_PARITY_MARK = "mark";
const QString SerialPort::SERIAL_PARITY_SPACE  = "space";
const QString SerialPort::SERIAL_PARITY_NO  = "no";
