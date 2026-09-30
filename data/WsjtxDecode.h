#ifndef QLOG_DATA_WSJTXDECODE_H
#define QLOG_DATA_WSJTXDECODE_H

#include <QString>
#include <QTime>

class WsjtxDecode
{
public:
    QString id, mode, message;
    bool is_new = false;
    bool low_confidence = false;
    bool off_air = false;
    QTime time;
    qint32 snr = 0;
    quint32 df = 0;
    double dt = 0.0;
    operator QString() const
    {
        return QString("WsjtxDecode: ")
                + "("
                + "ID: "            + id + "; "
                + "IsNew: "         + QString::number(is_new) + "; "
                + "Time: "          + time.toString() + "; "
                + "SNR: "           + QString::number(snr) + "; "
                + "DeltaTime: "     + QString::number(dt) + "; "
                + "DeltaFreq: "     + QString::number(df) + "; "
                + "Mode: "          + mode + "; "
                + "Message: "       + message + "; "
                + "LowConfidence: " + QString::number(low_confidence) + "; "
                + "OffAir: "        + QString::number(off_air) + "; "
                + ")";
    }
};

#endif // QLOG_DATA_WSJTXDECODE_H
