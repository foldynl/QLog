#ifndef QLOG_RIG_RIGRUNTIMEPROVIDER_H
#define QLOG_RIG_RIGRUNTIMEPROVIDER_H

#include "mcpbridge/RuntimeProvider.h"
#include "Rig.h"

// The status must belong to the owner and outlive its provider.
RuntimeProvider *createRigRuntimeProvider(QObject *owner, const Rig::Status &status);

#endif // QLOG_RIG_RIGRUNTIMEPROVIDER_H
