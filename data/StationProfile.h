#ifndef QLOG_DATA_STATIONPROFILE_H
#define QLOG_DATA_STATIONPROFILE_H

#include <QMetaType>
#include <QString>
#include <QObject>
#include <QDataStream>
#include <QVariant>
#include <QMap>

#include "data/ProfileManager.h"

class StationProfile
{

public:
    StationProfile() : ituz(0), cqz(0), dxcc(0) {};

    QString profileName;
    QString callsign;
    QString locator;
    QString operatorName;
    QString qthName;
    QString iota;
    QString pota;
    QString sota;
    QString sig;
    QString sigInfo;
    QString vucc;
    QString wwff;
    int ituz;
    int cqz;
    int dxcc;
    QString country;

    bool operator== (const StationProfile &profile);
    bool operator!= (const StationProfile &profile);

    QString toHTMLString() const;

private:
    friend QDataStream& operator<<(QDataStream& out, const StationProfile& v);
    friend QDataStream& operator>>(QDataStream& in, StationProfile& v);
};

Q_DECLARE_METATYPE(StationProfile)

class StationProfilesManager : QObject, public ProfileManagerSQL<StationProfile>
{
    Q_OBJECT

public:

    explicit StationProfilesManager(QObject *parent = nullptr);
    ~StationProfilesManager() {};

    static StationProfilesManager* instance()
    {
        static StationProfilesManager instance;
        return &instance;
    };
    void save();
};


#endif // QLOG_DATA_STATIONPROFILE_H
