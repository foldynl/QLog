#ifndef QLOG_DATA_ANTPROFILE_H
#define QLOG_DATA_ANTPROFILE_H

#include <QString>
#include <QObject>
#include <QDataStream>

#include "data/ProfileManager.h"

class RuntimeProvider;

#define DEFAULT_ROT_MODEL 1

class AntProfile
{
public:
    AntProfile() : azimuthBeamWidth(0.0), azimuthOffset(0.0) {};
    QString profileName;
    QString description;
    double azimuthBeamWidth;
    double azimuthOffset;
    QStringList bands;
    QString rotProfileName;

    bool operator== (const AntProfile &profile);
    bool operator!= (const AntProfile &profile);

private:
    friend QDataStream& operator<<(QDataStream& out, const AntProfile& v);
    friend QDataStream& operator>>(QDataStream& in, AntProfile& v);
};

Q_DECLARE_METATYPE(AntProfile)

class AntProfilesManager : public ProfileManagerSQL<AntProfile>
{
    Q_OBJECT

public:

    explicit AntProfilesManager();
    ~AntProfilesManager() { };

    static AntProfilesManager *instance()
    {
        static AntProfilesManager instance;
        return &instance;
    };
    void save();
    RuntimeProvider *getRuntimeProvider() const;
    QStringList profileNamesForBand(const QString &bandName);
    QString profileNameForBand(const QString &bandName,
                               const QString &preferredProfileName = QString());

private:
    RuntimeProvider *statusProvider;
};


#endif // QLOG_DATA_ANTPROFILE_H
