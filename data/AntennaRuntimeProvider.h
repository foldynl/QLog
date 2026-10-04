#ifndef QLOG_DATA_ANTENNARUNTIMEPROVIDER_H
#define QLOG_DATA_ANTENNARUNTIMEPROVIDER_H

class RuntimeProvider;
class AntProfilesManager;

// The manager owns the provider; getters read its current profile in its thread.
RuntimeProvider *createAntennaRuntimeProvider(AntProfilesManager *manager);

#endif // QLOG_DATA_ANTENNARUNTIMEPROVIDER_H
