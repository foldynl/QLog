#ifndef QLOG_DATA_ACTIVITYRUNTIMEPROVIDER_H
#define QLOG_DATA_ACTIVITYRUNTIMEPROVIDER_H

class ActivityProfilesManager;
class RuntimeProvider;

RuntimeProvider *createActivityRuntimeProvider(ActivityProfilesManager *manager);

#endif // QLOG_DATA_ACTIVITYRUNTIMEPROVIDER_H
