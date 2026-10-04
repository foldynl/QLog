#ifndef QLOG_DATA_STATIONRUNTIMEPROVIDER_H
#define QLOG_DATA_STATIONRUNTIMEPROVIDER_H

class RuntimeProvider;
class StationProfilesManager;

// The manager owns the provider; getters read its current profile in its thread.
RuntimeProvider *createStationRuntimeProvider(StationProfilesManager *manager);

#endif // QLOG_DATA_STATIONRUNTIMEPROVIDER_H
