#ifndef QLOG_ROTATOR_ROTCAPS_H
#define QLOG_ROTATOR_ROTCAPS_H


class RotCaps
{
public:
    RotCaps(bool isNetworkOnly = false,
            int serial_data_bits = 8,
            int serial_stop_bits = 1);

    bool isNetworkOnly;
    int serialDataBits;
    int serialStopBits;
};

#endif // QLOG_ROTATOR_ROTCAPS_H
