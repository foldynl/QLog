#include "RigCaps.h"

RigCaps::RigCaps(bool canGetFreq,
                 bool canGetMode,
                 bool canGetVFO,
                 bool canGetPWR,
                 bool canGetRIT,
                 bool canGetXIT,
                 bool canGetPTT,
                 bool canGetKeySpeed,
                 bool canSendMorse,
                 bool isNetworkOnly,
                 bool needPolling,
                 bool canProcessDXSpot,
                 int serial_data_bits,
                 int serial_stop_bits,
                 bool isCIVAddrSupported,
                 bool canGetSplit
                 ) :
    canGetFreq(canGetFreq),
    canGetMode(canGetMode),
    canGetVFO(canGetVFO),
    canGetPWR(canGetPWR),
    canGetRIT(canGetRIT),
    canGetXIT(canGetXIT),
    canGetPTT(canGetPTT),
    canGetKeySpeed(canGetKeySpeed),
    canSendMorse(canSendMorse),
    serialDataBits(serial_data_bits),
    serialStopBits(serial_stop_bits),
    isNetworkOnly(isNetworkOnly),
    needPolling(needPolling),
    canProcessDXSpot(canProcessDXSpot),
    isCIVAddrSupported(isCIVAddrSupported),
    canGetSplit(canGetSplit)
{

}
