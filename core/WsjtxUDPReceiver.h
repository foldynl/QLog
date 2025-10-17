#ifndef QLOG_CORE_WSJTXUDPRECEIVER_H
#define QLOG_CORE_WSJTXUDPRECEIVER_H

#include <QObject>
#include <QDateTime>
#include <QHostAddress>
#include <QSqlRecord>
#include <QNetworkDatagram>

#include "data/UpdatableSQLRecord.h"

class Data;
class QUdpSocket;

class WsjtxStatus {
public:
    QString id, mode, tx_mode, sub_mode;
    QString dx_call, dx_grid, de_call, de_grid;
    QString report;
    quint64 dial_freq;
    qint32 rx_df, tx_df;
    bool tx_enabled, transmitting, decoding;
    bool tx_watchdog, fast_mode;
    quint8 special_op_mode;
    quint32 freq_tolerance, tr_period;
    QString conf_name, tx_message;

    operator QString() const {
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
                + ")";}
};


class WsjtxDecode {
public:
    QString id, mode, message;
    bool is_new, low_confidence, off_air;
    QTime time;
    qint32 snr;
    quint32 df;
    double dt;
    operator QString() const {
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
                + ")";}
};

class WsjtxLog {
public:
    QString id, dx_call, dx_grid, mode, rprt_sent, rprt_rcvd;
    QString tx_pwr, comments, name, op_call, my_call, my_grid, prop_mode;
    QString exch_sent, exch_rcvd;
    QDateTime time_on, time_off;
    quint64 tx_freq;
    operator QString() const {
        return QString("WsjtxLog: ")
                + "("
                + "ID: "           + id + "; "
                + "DateTimeOff: "  + time_off.toString() + "; "
                + "DXCall: "       + dx_call + "; "
                + "DXGrid: "       + dx_grid + "; "
                + "TXFreq: "       + QString::number(tx_freq) + "; "
                + "Mode: "         + mode + "; "
                + "RrpSent: "      + rprt_sent + "; "
                + "RrpRcvd: "      + rprt_rcvd + "; "
                + "TxPower: "      + tx_pwr + "; "
                + "Comments: "     + comments + "; "
                + "Name: "         + name + "; "
                + "DateTimeOn: "   + time_on.toString() + "; "
                + "OpCall: "       + op_call + "; "
                + "MyCall: "       + my_call + "; "
                + "MyGrid: "       + my_grid + "; "
                + "ExchSent: "     + exch_sent + "; "
                + "ExchRcvd: "     + exch_rcvd + "; "
                + "ADIFPropMode: " + prop_mode + "; "
                + ")";}
};

class WsjtxLogADIF {
public:
    QString id, log_adif;

    operator QString() const {
        return QString("WsjtxLogADIF")
                 + "("
                 + "ID: "  + id + ";"
                 + "ADIF: " + log_adif + ";"
                 + ")";}
};

class WsjtxUDPReceiver : public QObject
{
    Q_OBJECT
public:
    explicit WsjtxUDPReceiver(QObject *parent = nullptr);
    static float modePeriodLength(const QString &);

    static quint16 getConfigPort();
    static void saveConfigPort(quint16);
    static QString getConfigForwardAddresses();
    static void saveConfigForwardAddresses(const QString &);
    static void saveConfigMulticastJoin(bool);
    static bool getConfigMulticastJoin();
    static void saveConfigMulticastAddress(const QString &);
    static QString getConfigMulticastAddress();
    static void saveConfigMulticastTTL(int);
    static int getConfigMulticastTTL();

signals:
    void statusReceived(WsjtxStatus);
    void decodeReceived(WsjtxDecode);
    void addContact(QSqlRecord);

public slots:
    void startReply(WsjtxDecode);
    void reloadSetting();

private slots:
    void readPendingDatagrams();
    void insertContact(WsjtxLog log);
    void contactReady(QSqlRecord record);
    void insertContact(WsjtxLogADIF log);

private:
    QUdpSocket* socket;
    QHostAddress wsjtxAddress;
    quint16 wsjtxPort;

    UpdatableSQLRecord wsjtSQLRecord;

    static int DEFAULT_PORT;
    static QString CONFIG_MULTICAST_TTL;

    void openPort();
    void forwardDatagram(const QNetworkDatagram &);
};

#endif // QLOG_CORE_WSJTXUDPRECEIVER_H
