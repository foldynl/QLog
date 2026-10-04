#ifndef QLOG_ROTATOR_ROTATORRUNTIMEPROVIDER_H
#define QLOG_ROTATOR_ROTATORRUNTIMEPROVIDER_H

#include "mcpbridge/RuntimeProvider.h"
#include "Rotator.h"

// The status must belong to the owner and outlive its provider.
RuntimeProvider *createRotatorRuntimeProvider(QObject *owner, const Rotator::Status &status);

#endif // QLOG_ROTATOR_ROTATORRUNTIMEPROVIDER_H
