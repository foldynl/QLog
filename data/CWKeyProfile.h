#ifndef QLOG_DATA_CWKEYPROFILE_H
#define QLOG_DATA_CWKEYPROFILE_H

#include <QString>
#include <QObject>
#include <QDataStream>

#include "data/ProfileManager.h"
#include "cwkey/drivers/CWKey.h"

#define DEFAULT_CWKEY_MODEL 0

// this is a hack. We have non-empty text to set empty value in Setting dialog->Rig's Assigned CW Key combo
// therefore QLog use one space as a profile that meams no CW is assigned to Rig
#define EMPTY_PROFILE_NAME " "

class CWKeyProfile
{
public:
    CWKeyProfile() : model(CWKey::DUMMY_KEYER),
                     keyMode(CWKey::IAMBIC_B)
    {
        defaultSpeed = 0;
        baudrate = 0;
        netport = 0;
    };

    QString profileName;
    CWKey::CWKeyTypeID model;
    qint32 defaultSpeed;
    CWKey::CWKeyModeID keyMode;
    QString portPath;
    quint32 baudrate;
    QString hostname;
    quint16 netport;

    bool operator== (const CWKeyProfile &profile);
    bool operator!= (const CWKeyProfile &profile);

private:
    friend QDataStream& operator<<(QDataStream& out, const CWKeyProfile& v);
    friend QDataStream& operator>>(QDataStream& in, CWKeyProfile& v);

};

Q_DECLARE_METATYPE(CWKeyProfile);

class CWKeyProfilesManager : QObject, public ProfileManagerSQL<CWKeyProfile>
{
    Q_OBJECT

public:

    explicit CWKeyProfilesManager(QObject *parent = nullptr);
    ~CWKeyProfilesManager() { };

    static CWKeyProfilesManager* instance();
    void save();

};
#endif // QLOG_DATA_CWKEYPROFILE_H
