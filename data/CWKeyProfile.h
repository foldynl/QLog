#ifndef CWKEYPROFILE_H
#define CWKEYPROFILE_H

#include <QString>
#include <QObject>
#include <QDataStream>

#include "data/ProfileManager.h"

#define DEFAULT_CWKEY_MODEL 0

class CWKeyProfile
{
public:
    CWKeyProfile() { model = DEFAULT_CWKEY_MODEL;
                     defaultSpeed = 0;
                     baudrate = 0;
                     keyMode = 0;
                   };

    QString profileName;
    qint32 model;
    qint32 defaultSpeed;
    qint32 keyMode;
    QString portPath;
    quint32 baudrate;

    bool operator== (const CWKeyProfile &profile);
    bool operator!= (const CWKeyProfile &profile);

private:
    friend QDataStream& operator<<(QDataStream& out, const CWKeyProfile& v);
    friend QDataStream& operator>>(QDataStream& in, CWKeyProfile& v);

};

Q_DECLARE_METATYPE(CWKeyProfile);

class CWKeyProfilesManager : QObject, public ProfileManager<CWKeyProfile>
{
    Q_OBJECT

public:

    explicit CWKeyProfilesManager(QObject *parent = nullptr);
    ~CWKeyProfilesManager() { };

    static CWKeyProfilesManager* instance();
    void save();

};
#endif // CWKEYPROFILE_H
