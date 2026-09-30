#ifndef QLOG_DATA_WSJTXSTATUS_H
#define QLOG_DATA_WSJTXSTATUS_H

#include <QString>

class WsjtxStatus
{
public:
    QString id, mode, tx_mode, sub_mode;
    QString dx_call, dx_grid, de_call, de_grid;
    QString report;
    quint64 dial_freq = 0;
    qint32 rx_df = 0;
    qint32 tx_df = 0;
    bool tx_enabled = false;
    bool transmitting = false;
    bool decoding = false;
    bool tx_watchdog = false;
    bool fast_mode = false;
    quint8 special_op_mode = 0;
    quint32 freq_tolerance = 0;
    quint32 tr_period = 0;
    QString conf_name, tx_message;

    operator QString() const
    {
        return QString("WsjtxStatus: ")
                + "("
                + "ID: "           + id + "; "
                + "Dial: "         + QString::number(dial_freq) + "; "
                + "Mode: "         + mode + "; "
                + "DXCall: "       + dx_call + "; "
                + "Report: "       + report + "; "
                + "TXMode: "       + tx_mode + "; "
                + "TXEnabled: "    + QString::number(tx_enabled) + "; "
                + "Transmitting: " + QString::number(transmitting) + "; "
                + "Decoding: "     + QString::number(decoding) + "; "
                + "RxDF: "         + QString::number(rx_df) + "; "
                + "TxDF: "         + QString::number(tx_df) + "; "
                + "DECall: "       + de_call + "; "
                + "DEGrid: "       + de_grid + "; "
                + "DXGrid: "       + dx_grid + "; "
                + "TXWatchdog: "   + QString::number(tx_watchdog) + "; "
                + "SubMode: "      + sub_mode + "; "
                + "FastMode: "     + QString::number(fast_mode) + "; "
                + "SpecOpMode: "   + QString::number(special_op_mode) + "; "
                + "FreqTolerance: "+ QString::number(freq_tolerance) + "; "
                + "TRPeriod: "     + QString::number(tr_period) + "; "
                + "ConfName: "     + conf_name + "; "
                + "TXMessage: "    + tx_message + "; "
                + ")";
    }
};

#endif // QLOG_DATA_WSJTXSTATUS_H
