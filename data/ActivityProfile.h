#ifndef Q_LOG_DATA_ACTIVITYPROFILE_H
#define Q_LOG_DATA_ACTIVITYPROFILE_H

#include <QHash>
#include "data/ProfileManager.h"

class ActivityProfile
{
public:

    enum ProfileType
    {
        ANTENNA_PROFILE = 0,
        STATION_PROFILE = 1,
        RIG_PROFILE = 2,
        ROT_PROFILE = 3,
        MAIN_LAYOUT_PROFILE = 4,
        BANDMAP_GUIDE_PROFILE = 5
    };

    enum ProfileParamType
    {
        CONNECT = 0,
    };

    struct ProfileRecord
    {
        QString name;
        QHash<ProfileParamType, QVariant> params;

        bool operator==(const ProfileRecord &other) const
        {
            return name == other.name && params == other.params;
        }
    };

    ActivityProfile()  {};
    ActivityProfile(const QString &name, const QJsonDocument &config);
    QByteArray toJson() const;

    QString profileName;
    QHash<ProfileType, ProfileRecord> profiles;
    QHash<int, QVariant> fieldValues;

    QVariant getProfileParam(ProfileType profileType, ProfileParamType profileParam) const
    {
        return profiles.value(profileType).params.value(profileParam);
    };

    bool operator== (const ActivityProfile &profile);
    bool operator!= (const ActivityProfile &profile);

private:
    // friend QDataStream& operator<<(QDataStream& out, const ActivityProfile& v);
    // friend QDataStream& operator<<(QDataStream& out, const ActivityProfile::ProfileRecord& v);

    // friend QDataStream& operator>>(QDataStream& in, ActivityProfile& v);
    // friend QDataStream& operator>>(QDataStream& in, ActivityProfile::ProfileRecord& v);

    QString getParamName(ProfileParamType id) const  { return profileParamsNameMapping.value(id);} ;
    ProfileParamType getParamID(const QString &paramName) const { return profileParamsNameMapping.key(paramName);};
    QHash<ProfileParamType, QString> profileParamsNameMapping = {{CONNECT, "connect"}};
};

Q_DECLARE_METATYPE(ActivityProfile);

class ActivityProfilesManager : public ProfileManagerSQL<ActivityProfile>
{
    Q_OBJECT

signals:
    void changeFinished(const QString &name);

public slots:
    void setAllProfiles();

public:

    explicit ActivityProfilesManager();
    ~ActivityProfilesManager() { };

    static ActivityProfilesManager* instance()
    {
        static ActivityProfilesManager instance;
        return &instance;
    };
    void save();

private:
    void applyBandmapGuideProfile(const QString &profileId);
};

#endif // Q_LOG_DATA_ACTIVITYPROFILE_H
