#ifndef QLOG_DATA_RIGPROFILE_H
#define QLOG_DATA_RIGPROFILE_H

#include <QString>
#include <QObject>
#include <QMap>
#include <QDataStream>
#include <QVariant>

#include "data/ProfileManager.h"

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
                   model = 1; netport = 0; baudrate = 0;
                   databits = 0; stopbits = 0.0; pollInterval = 0;
                   txFreqStart = 0.0; txFreqEnd = 0.0; getFreqInfo = false;
                   getModeInfo = false; getVFOInfo = false; getPWRInfo = false;
                   ritOffset = 0.0; xitOffset = 0.0, getRITInfo = false;
                   getXITInfo = true; defaultPWR = 0.0, getPTTInfo = false;
                   QSYWiping = false, getKeySpeed = false, keySpeedSync = false;
                   driver = 0, dxSpot2Rig = false, civAddr = -1;
                   shareRigctld = false; rigctldPort = 4532; getSplitInfo = false;
                 };

    QString profileName;
    QString portPath;
    QString hostname;
    QString flowcontrol;
    QString parity;
    double ritOffset;
    double xitOffset;
    double defaultPWR;
    QString assignedCWKey;
    QString pttType;
    QString pttPortPath;
    QString rts;
    QString dtr;
    QString rigctldPath;  // empty = autodetect
    QString rigctldArgs;  // additional arguments

    qint32 model;
    quint32 baudrate;
    float stopbits;
    quint32 pollInterval;
    float txFreqStart;
    float txFreqEnd;
    qint32 driver;

    quint16 netport;
    qint16 civAddr; // -1 = AUTO; otherwise address
    quint16 rigctldPort;

    quint8 databits;
    bool getFreqInfo;
    bool getModeInfo;
    bool getVFOInfo;
    bool getPWRInfo;
    bool getRITInfo;
    bool getXITInfo;
    bool getPTTInfo;
    bool QSYWiping;
    bool getKeySpeed;
    bool keySpeedSync;
    bool dxSpot2Rig;
    bool shareRigctld;
    bool getSplitInfo;

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
