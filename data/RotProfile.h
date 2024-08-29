#ifndef QLOG_DATA_ROTPROFILE_H
#define QLOG_DATA_ROTPROFILE_H

#include <QString>
#include <QObject>
#include <QDataStream>
#include <QVariant>

#include "data/ProfileManager.h"

#define DEFAULT_ROT_MODEL 1

class RotProfile
{
public:
    enum rotPortType
    {
        SERIAL_ATTACHED,
        NETWORK_ATTACHED
    };

    RotProfile() { model = DEFAULT_ROT_MODEL; netport = 0;
                   baudrate = 0; databits = 0; stopbits = 0.0;
                   driver = 0;};

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
    qint32 driver;

    bool operator== (const RotProfile &profile);
    bool operator!= (const RotProfile &profile);

    rotPortType getPortType() const;


private:
    friend QDataStream& operator<<(QDataStream& out, const RotProfile& v);
    friend QDataStream& operator>>(QDataStream& in, RotProfile& v);

};

Q_DECLARE_METATYPE(RotProfile);

class RotProfilesManager : QObject, public ProfileManagerSQL<RotProfile>
{
    Q_OBJECT

public:

    explicit RotProfilesManager(QObject *parent = nullptr);
    ~RotProfilesManager() { };

    static RotProfilesManager* instance();
    void save();

};

#endif // QLOG_DATA_ROTPROFILE_H
