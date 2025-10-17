#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QDataStream>
#include <QSqlTableModel>
#include <QSqlRecord>
#include <QSqlError>
#include <QDateTime>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QSqlField>

#include "WsjtxUDPReceiver.h"
#include "data/Data.h"
#include "debug.h"
#include "data/HostsPortString.h"
#include "rig/macros.h"
#include "data/BandPlan.h"
#include "logformat/AdiFormat.h"
#include "core/LogParam.h"

MODULE_IDENTIFICATION("qlog.core.wsjtx");

WsjtxUDPReceiver::WsjtxUDPReceiver(QObject *parent) :
    QObject(parent),
    socket(nullptr)
{
    FCT_IDENTIFICATION;
    socket = new QUdpSocket(this);
    openPort();
    connect(socket, &QUdpSocket::readyRead, this, &WsjtxUDPReceiver::readPendingDatagrams);
    connect(&wsjtSQLRecord, &UpdatableSQLRecord::recordReady, this, &WsjtxUDPReceiver::contactReady);
}

void WsjtxUDPReceiver::openPort()
{
    FCT_IDENTIFICATION;


    if ( ! socket )
    {
        return;
    }

    if( socket->state() == QAbstractSocket::BoundState)
    {
        socket->close();
    }

    int newPort = getConfigPort();

    qCDebug(runtime) << "Listen port"<< newPort;

    bool multicastEnabled = getConfigMulticastJoin();

    qCDebug(runtime) << (( multicastEnabled ) ? "Multicast" : "Unicast") << "enabled";

    if ( multicastEnabled )
    {
        if ( ! socket->bind(QHostAddress::AnyIPv4, newPort, QUdpSocket::ShareAddress|QUdpSocket::ReuseAddressHint) )
        {
            qWarning() << "Cannot bind the Port for WSJTX";
        }
        else
        {
            QHostAddress multicastAddress = QHostAddress(getConfigMulticastAddress());

            const QList<QNetworkInterface> listNetworkIF = QNetworkInterface::allInterfaces();

            /* Join Multicast Group on all Multicast-capable interfaces */
            for ( const auto &networkIF : listNetworkIF )
            {
                auto flags = QNetworkInterface::IsUp | QNetworkInterface::CanMulticast;
                if ( networkIF.isValid()
                     && (networkIF.flags() & flags) )
                {
                    if ( ! socket->joinMulticastGroup(multicastAddress, networkIF) )
                    {
                        qWarning() << "Cannot join the Multicast address" << networkIF.humanReadableName() << "; Address" << multicastAddress;;
                    }
                    else
                    {
                        qCDebug(runtime) << "Joined interface: " << networkIF.humanReadableName() << "; Address" << multicastAddress;
                    }
                }
            }

            socket->setSocketOption(QAbstractSocket::MulticastTtlOption, getConfigMulticastTTL());
        }
    }
    else
    {
        if ( ! socket->bind(QHostAddress::Any, newPort) )
        {
            qWarning() << "Cannot bind the Port for WSJTX";
        }
        else
        {
            qCDebug(runtime) << "Listening on all interfaces";
        }
    }
}

void WsjtxUDPReceiver::forwardDatagram(const QNetworkDatagram &datagram)
{
    FCT_IDENTIFICATION;

    HostsPortString forwardAddresses(getConfigForwardAddresses());

    const QList<HostPortAddress> &addrList = forwardAddresses.getAddrList();

    for ( const HostPortAddress &addr : addrList )
    {
        QUdpSocket udpSocket;

        qCDebug(runtime) << "Sending to " << addr;
        udpSocket.writeDatagram(datagram.data(), addr, addr.getPort());
    }
}

float WsjtxUDPReceiver::modePeriodLength(const QString &mode)
{
    FCT_IDENTIFICATION;

    float ret = 60;

    qCDebug(function_parameters) << mode;

    if ( mode == "FST4"
         || mode == "FT8"
         || mode == "MSK144" )
    {
        ret = 15;
    }
    else if ( mode == "FT4" )
    {
        ret = 7.5;
    }
    else if ( mode == "JT4"
              || mode == "JT9"
              || mode.contains("JT65")
              || mode == "QRA64"
              || mode == "ISCAT" )
    {
        ret = 60;
    }
    else if ( mode == "FST4W"
              || mode == "WSPR" )
    {
        ret = 120;
    }

    qCDebug(runtime) << "Period: " << ret;

    return ret;
}

quint16 WsjtxUDPReceiver::getConfigPort()
{
    FCT_IDENTIFICATION;

    return LogParam::getNetworkWsjtxListenerPort(WsjtxUDPReceiver::DEFAULT_PORT);
}

void WsjtxUDPReceiver::saveConfigPort(quint16 port)
{
    FCT_IDENTIFICATION;

    LogParam::setNetworkNotifRigStateAddrs(port);
}

QString WsjtxUDPReceiver::getConfigForwardAddresses()
{
    FCT_IDENTIFICATION;

    return LogParam::getNetworkWsjtxForwardAddrs();
}

void WsjtxUDPReceiver::saveConfigForwardAddresses(const QString &addresses)
{
    FCT_IDENTIFICATION;

    LogParam::setNetworkWsjtxForwardAddrs(addresses);
}

void WsjtxUDPReceiver::saveConfigMulticastJoin(bool state)
{
    FCT_IDENTIFICATION;

    LogParam::setNetworkWsjtxListenerJoinMulticast(state);
}

bool WsjtxUDPReceiver::getConfigMulticastJoin()
{
    FCT_IDENTIFICATION;

    return LogParam::getNetworkWsjtxListenerJoinMulticast();
}

void WsjtxUDPReceiver::saveConfigMulticastAddress(const QString &addr)
{
    FCT_IDENTIFICATION;

    LogParam::setNetworkWsjtxListenerMulticastAddr(addr);
}

QString WsjtxUDPReceiver::getConfigMulticastAddress()
{
    FCT_IDENTIFICATION;    

    return LogParam::getNetworkWsjtxListenerMulticastAddr();
}

void WsjtxUDPReceiver::saveConfigMulticastTTL(int ttl)
{
    FCT_IDENTIFICATION;

    LogParam::setNetworkWsjtxListenerMulticastTTL(ttl);
}

int WsjtxUDPReceiver::getConfigMulticastTTL()
{
    FCT_IDENTIFICATION;

    return LogParam::getNetworkWsjtxListenerMulticastTTL();
}

void WsjtxUDPReceiver::readPendingDatagrams()
{
    FCT_IDENTIFICATION;

    while (socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = socket->receiveDatagram();

        /* remember WSJT receiving address becuase WSJT does not listen multicast address
         * but only UDP address. Therefore the command must be sent via UDP unicast */
        wsjtxAddress = datagram.senderAddress();
        wsjtxPort = datagram.senderPort();

        qCDebug(runtime) << "Received from" << wsjtxAddress;

        QDataStream stream(datagram.data());

        quint32 magic, schema, mtype;
        stream >> magic;
        stream >> schema;
        stream >> mtype;

        if (magic != 0xadbccbda) {
            qCDebug(runtime) << "Invalid wsjtx magic";
            continue;
        }

        qCDebug(runtime) << "WSJT mtype << "<< mtype << " schema " << schema;

        forwardDatagram(datagram);

        switch (mtype) {
        /* WSJTX Status message */
        case 1: {
            QByteArray id, mode, tx_mode, sub_mode, report, dx_call, dx_grid, de_call, de_grid, conf_name, tx_message;
            WsjtxStatus status;


            stream >> id >> status.dial_freq >> mode >> dx_call >> report >> tx_mode;
            stream >> status.tx_enabled >> status.transmitting >> status.decoding;
            stream >> status.rx_df >> status.tx_df >> de_call >> de_grid >> dx_grid;
            stream >> status.tx_watchdog >> sub_mode >> status.fast_mode >> status.special_op_mode >> status.freq_tolerance;
            stream >> status.tr_period >> conf_name >> tx_message;

            status.id = QString(id);
            status.mode = QString(mode);
            status.tx_mode = QString(mode);
            status.sub_mode = QString(sub_mode);
            status.report = QString(report);
            status.dx_call = QString(dx_call);
            status.dx_grid = QString(dx_grid);
            status.de_call = QString(de_call);
            status.de_grid = QString(de_grid);
            status.conf_name = QString(conf_name);
            status.tx_message = QString(tx_message);

            qCDebug(runtime)<<status;

            emit statusReceived(status);
            break;
        }
        /* WSJTX Decode message */
        case 2: {
            QByteArray id, mode, message;
            WsjtxDecode decode;

            stream >> id >> decode.is_new >> decode.time >> decode.snr >> decode.dt >> decode.df;
            stream >> mode >> message >> decode.low_confidence >> decode.off_air;

            decode.id = QString(id);
            decode.mode = QString(mode);
            decode.message = QString(message);

            if ( id.contains("JTDX") )
            {
                /* It's a workaround for JTDX only.
                 * JTDX sends the time without a time zone. Which is
                 * interpreted by QLog as time in the local zone and
                 * it is therefore recalculated, incorrectly, to UTC.
                 * Therefore it is needed to add timezone to date information
                 * received from JTDX
                 */
                // it is not needed to convert it here?
            }

            qCDebug(runtime) << decode;

            emit decodeReceived(decode);
            break;
        }
        /* WSJTX Log message */
        case 5: {
            QByteArray id, dx_call, dx_grid, mode, rprt_sent, rprt_rcvd, tx_pwr, comments;
            QByteArray name, op_call, my_call, my_grid, exch_sent, exch_rcvd, prop_mode;
            WsjtxLog log;

            stream >> id >> log.time_off >> dx_call >> dx_grid >> log.tx_freq >> mode >> rprt_sent;
            stream >> rprt_rcvd >> tx_pwr >> comments >> name >> log.time_on >> op_call;
            stream >> my_call >> my_grid >> exch_sent >> exch_rcvd >> prop_mode;

            log.id = QString(id);
            log.dx_call = QString(dx_call).toUpper();
            log.dx_grid = QString(dx_grid).toUpper();
            log.mode = QString(mode);
            log.rprt_sent = QString(rprt_sent);
            log.rprt_rcvd = QString(rprt_rcvd);
            log.tx_pwr = QString(tx_pwr);
            log.comments = QString(comments);
            log.name = QString(name);
            log.op_call = QString(op_call).toUpper();
            log.my_call = QString(my_call).toUpper();
            log.my_grid = QString(my_grid).toUpper();
            log.exch_sent = QString(exch_sent);
            log.exch_rcvd = QString(exch_rcvd);
            log.prop_mode = QString(prop_mode);

            if ( id.contains("JTDX") )
            {
                /* It's a workaround for JTDX only.
                 * JTDX sends the time without a time zone. Which is
                 * interpreted by QLog as time in the local zone and
                 * it is therefore recalculated, incorrectly, to UTC.
                 * Therefore it is needed to add timezone to date information
                 * received from JTDX
                 */
                qCDebug(runtime) << "JTDX detected - adding Timezone information";
                log.time_on.setTimeZone(QTimeZone::utc());
                log.time_off.setTimeZone(QTimeZone::utc());
            }
            qCDebug(runtime) << log;

            insertContact(log);
            break;
        }
        /* WSJTX LogADIF message */
        case 12: {
            QByteArray id, adif_text;
            WsjtxLogADIF log;

            const QString data = datagram.data();
            bool isCSNSat = data.contains("<programid:6>CSN Sat");

            if ( isCSNSat )
            {
                id = "CSN Sat";
                int index = data.indexOf("<adif_ver:");
                if ( index != -1 )
                    adif_text = data.mid(index).toUtf8();
                else
                    adif_text.clear();
            }
            else
                stream >> id >> adif_text;

            log.id = QString(id);
            log.log_adif = QString(adif_text);

            insertContact(log);
            break;
        }
        }
    }
}

void WsjtxUDPReceiver::insertContact(WsjtxLog log)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << log;

    QSqlTableModel model;
    model.setTable("contacts");
    model.removeColumn(model.fieldIndex("id"));

    QSqlRecord record = model.record(0);

    double freq = Hz2MHz(static_cast<double>(log.tx_freq));

    record.setValue("freq", freq);
    record.setValue("band", BandPlan::freq2Band(freq).name);

    /* if field is empty then do not initialize it, leave it NULL
     * for database */
    if ( !log.dx_call.isEmpty() )
    {
        record.setValue("callsign", log.dx_call);
    }

    if ( !log.rprt_rcvd.isEmpty() )
    {
        record.setValue("rst_rcvd", log.rprt_rcvd);
    }

    if ( !log.rprt_sent.isEmpty() )
    {
        record.setValue("rst_sent", log.rprt_sent);
    }

    if ( !log.name.isEmpty() )
    {
        record.setValue("name", Data::removeAccents(log.name));
        record.setValue("name_intl", log.name);
    }

    if ( !log.dx_grid.isEmpty() )
    {
        record.setValue("gridsquare", log.dx_grid);
    }

    if ( !log.mode.isEmpty() )
    {
        QString mode = log.mode.toUpper();
        QString submode;

        QPair<QString, QString> legacy = Data::instance()->legacyMode(mode);

        if ( !legacy.first.isEmpty() )
        {
            mode = legacy.first;
            submode = legacy.second;
        }

        record.setValue("mode", mode);
        record.setValue("submode", submode);
    }

    if ( log.time_on.isValid() )
    {
        record.setValue("start_time", log.time_on);
    }

    if ( log.time_off.isValid() )
    {
        record.setValue("end_time", log.time_off);
    }

    if ( !log.comments.isEmpty() )
    {
        record.setValue("comment", Data::removeAccents(log.comments));
        record.setValue("comment_intl", log.comments);
    }

    if ( !log.exch_sent.isEmpty() )
    {
        record.setValue("stx_string", log.exch_sent);
    }

    if ( !log.exch_rcvd.isEmpty() )
    {
        record.setValue("srx_string", log.exch_rcvd);
    }

    if ( !log.prop_mode.isEmpty() )
    {
        record.setValue("prop_mode", log.prop_mode);
    }

    if ( !log.tx_pwr.isEmpty() )
    {
        record.setValue("tx_pwr", log.tx_pwr);
    }

    if ( !log.op_call.isEmpty() )
    {
        record.setValue("operator", Data::removeAccents(log.op_call));
    }

    if ( !log.my_grid.isEmpty() )
    {
        record.setValue("my_gridsquare", log.my_grid);
    }

    if ( !log.my_call.isEmpty() )
    {
        record.setValue("station_callsign", log.my_call);
    }

    // The QSO record can be received in two formats from WSJTX (raw and ADIF).
    // Therefore, it is necessary to save the first record and possibly update it
    // with the second record and then emit the result.
    // For this we create an updatable SQLRecord
    wsjtSQLRecord.updateRecord(record);
    //emit addContact(record);
}

void WsjtxUDPReceiver::contactReady(QSqlRecord record)
{
    FCT_IDENTIFICATION;

    emit addContact(record);
}

void WsjtxUDPReceiver::insertContact(WsjtxLogADIF log)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << log;

    QSqlTableModel model;
    model.setTable("contacts");
    model.removeColumn(model.fieldIndex("id"));

    QSqlRecord record = model.record(0);

    QTextStream in(&log.log_adif);
    AdiFormat adif(in);

    adif.importNext(record);

    // the values ​​listed below have default values ​​defined in ADIF.
    // These are set to N. However, WSJTX and other apps do not send
    // these fields, which means that all records would be set to N - do not send.
    // It is unacceptable. So this piece of code deletes these defaults so that
    // in NewContact these values ​​are set from the values ​​in the GUI.
    // There is a risk that some clone will start sending these values ​​as well.
    // In this case, it will have to be handled differently. Currently it is OK
    record.remove(record.indexOf("qsl_sent"));
    record.remove(record.indexOf("lotw_qsl_sent"));
    record.remove(record.indexOf("eqsl_qsl_sent"));

    // The QSO record can be received in two formats from WSJTX (raw and ADIF).
    // Therefore, it is necessary to save the first record and possibly update it
    // with the second record and then emit the result.
    // For this we create an updatable SQLRecord
    wsjtSQLRecord.updateRecord(record);
    //emit addContact(record);
}

void WsjtxUDPReceiver::startReply(WsjtxDecode decode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << decode;

    /* sending to WSJT to UDP address, not multicast address because
     * WSJTX does not listen multicast address */
    qCDebug(runtime) << "Sending to" << wsjtxAddress;

    QByteArray data;
    QDataStream stream(&data, QIODevice::ReadWrite);
    stream << static_cast<quint32>(0xadbccbda);
    stream << static_cast<quint32>(3);
    stream << static_cast<quint32>(4);
    stream << decode.id.toUtf8();
    stream << decode.time;
    stream << decode.snr;
    stream << decode.dt;
    stream << decode.df;
    stream << decode.mode.toUtf8();
    stream << decode.message.toUtf8();
    stream << decode.low_confidence;
    stream << static_cast<quint8>(0);

    socket->writeDatagram(data, wsjtxAddress, wsjtxPort);
}

void WsjtxUDPReceiver::reloadSetting()
{
    FCT_IDENTIFICATION;
    openPort();
}

int     WsjtxUDPReceiver::DEFAULT_PORT = 2237;
QString WsjtxUDPReceiver::CONFIG_MULTICAST_TTL = "network/wsjtx_multicast_ttl";
