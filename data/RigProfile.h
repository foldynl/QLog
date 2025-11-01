#ifndef QLOG_DATA_RIGPROFILE_H
#define QLOG_DATA_RIGPROFILE_H

#include <QString>
#include <QObject>
#include <QMap>
#include <QDataStream>
#include <QVariant>

#include "data/ProfileManager.h"

#define DEFAULT_HAMLIB_RIG_MODEL 1

class RigProfile
{
public:
    enum rigPortType
    {
        SERIAL_ATTACHED,
        NETWORK_ATTACHED,
        SPECIAL_OMNIRIG_ATTACHED
    };

    RigProfile() {
                   model = DEFAULT_HAMLIB_RIG_MODEL; netport = 0; baudrate = 0;
                   databits = 0; stopbits = 0.0; pollInterval = 0;
                   txFreqStart = 0.0; txFreqEnd = 0.0; getFreqInfo = false;
                   getModeInfo = false; getVFOInfo = false; getPWRInfo = false;
                   ritOffset = 0.0; xitOffset = 0.0, getRITInfo = false;
                   getXITInfo = true; defaultPWR = 0.0, getPTTInfo = false;
                   QSYWiping = false, getKeySpeed = false, keySpeedSync = false;
                   driver = 0, dxSpot2Rig = false;
                 };

    QString profileName;
    qint32 model;
    QString portPath;
    QString hostname;
    quint16 netport;
    quint32 baudrate;
    quint8 databits;
    float stopbits;
    QString flowcontrol;
    QString parity;
    quint32 pollInterval;
    double ritOffset;
    double xitOffset;
    float txFreqStart;
    float txFreqEnd;
    bool getFreqInfo;
    bool getModeInfo;
    bool getVFOInfo;
    bool getPWRInfo;
    bool getRITInfo;
    bool getXITInfo;
    double defaultPWR;
    bool getPTTInfo;
    bool QSYWiping;
    bool getKeySpeed;
    QString assignedCWKey;
    bool keySpeedSync;
    qint32 driver;
    bool dxSpot2Rig;
    QString pttType;
    QString pttPortPath;
    QString rts;
    QString dtr;

    bool operator== (const RigProfile &profile);
    bool operator!= (const RigProfile &profile);

    QString toHTMLString() const;
    rigPortType getPortType() const;

private:
    friend QDataStream& operator<<(QDataStream& out, const RigProfile& v);
    friend QDataStream& operator>>(QDataStream& in, RigProfile& v);
};

Q_DECLARE_METATYPE(RigProfile)

class RigProfilesManager : public ProfileManagerSQL<RigProfile>
{
    Q_OBJECT

public:

    explicit RigProfilesManager();
    ~RigProfilesManager() { };

    static RigProfilesManager* instance()
    {
        static RigProfilesManager instance;
        return &instance;
    };
    void save();

};


#endif // QLOG_DATA_RIGPROFILE_H
