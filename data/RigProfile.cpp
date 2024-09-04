#include <QSettings>
#include <QSqlQuery>
#include <QSqlError>

#include "RigProfile.h"
#include "core/debug.h"
#include "data/ProfileManager.h"
#include "rig/Rig.h"

MODULE_IDENTIFICATION("qlog.data.rigprofile");

QDataStream& operator<<(QDataStream& out, const RigProfile& v)
{
    out << v.profileName << v.model << v.portPath
        << v.hostname << v.netport << v.baudrate
        << v.databits << v.stopbits << v.flowcontrol
        << v.parity << v.pollInterval << v.txFreqStart
        << v.txFreqEnd << v.getFreqInfo << v.getModeInfo
        << v.getVFOInfo << v.getPWRInfo << v.ritOffset
        << v.xitOffset << v.getRITInfo << v.getXITInfo
        << v.defaultPWR << v.getPTTInfo << v.QSYWiping
        << v.getKeySpeed << v.assignedCWKey << v.keySpeedSync
        << v.driver << v.dxSpot2Rig << v.pttType << v.pttPortPath;

    return out;
}

QDataStream& operator>>(QDataStream& in, RigProfile& v)
{
    in >> v.profileName;
    in >> v.model;
    in >> v.portPath;
    in >> v.hostname;
    in >> v.netport;
    in >> v.baudrate;
    in >> v.databits;
    in >> v.stopbits;
    in >> v.flowcontrol;
    in >> v.parity;
    in >> v.pollInterval;
    in >> v.txFreqStart;
    in >> v.txFreqEnd;
    in >> v.getFreqInfo;
    in >> v.getModeInfo;
    in >> v.getVFOInfo;
    in >> v.getPWRInfo;
    in >> v.ritOffset;
    in >> v.xitOffset;
    in >> v.getRITInfo;
    in >> v.getXITInfo;
    in >> v.defaultPWR;
    in >> v.getPTTInfo;
    in >> v.QSYWiping;
    in >> v.getKeySpeed;
    in >> v.assignedCWKey;
    in >> v.keySpeedSync;
    in >> v.driver;
    in >> v.dxSpot2Rig;
    in >> v.pttType;
    in >> v.pttPortPath;

    return in;
}

RigProfilesManager::RigProfilesManager(QObject *parent) :
    QObject(parent),
    ProfileManagerSQL<RigProfile>("rig_profiles")
{
    FCT_IDENTIFICATION;

    QSettings settings;

    QSqlQuery profileQuery;

    if ( ! profileQuery.prepare("SELECT profile_name, model, port_pathname, hostname, "
                                "netport, baudrate, databits, stopbits, flowcontrol, parity, "
                                "pollinterval, txfreq_start, txfreq_end, get_freq, get_mode, "
                                "get_vfo, get_pwr, rit_offset, xit_offset, get_rit, get_xit, "
                                "default_pwr, get_ptt, qsy_wiping, get_key_speed, assigned_cw_key, "
                                "key_speed_sync, driver, dxspot2rig, ptt_type, ptt_port_pathname "
                                "FROM rig_profiles") )
    {
        qWarning()<< "Cannot prepare select";
    }

    if ( profileQuery.exec() )
    {
        while (profileQuery.next())
        {
            RigProfile profileDB;
            profileDB.profileName = profileQuery.value(0).toString();
            profileDB.model =  profileQuery.value(1).toInt();
            profileDB.portPath =  profileQuery.value(2).toString();
            profileDB.hostname =  profileQuery.value(3).toString();
            profileDB.netport =  profileQuery.value(4).toUInt();
            profileDB.baudrate =  profileQuery.value(5).toUInt();
            profileDB.databits =  profileQuery.value(6).toUInt();
            profileDB.stopbits =  profileQuery.value(7).toFloat();
            profileDB.flowcontrol =  profileQuery.value(8).toString();
            profileDB.parity =  profileQuery.value(9).toString();
            profileDB.pollInterval = profileQuery.value(10).toUInt();
            profileDB.txFreqStart = profileQuery.value(11).toFloat();
            profileDB.txFreqEnd = profileQuery.value(12).toFloat();
            profileDB.getFreqInfo = profileQuery.value(13).toBool();
            profileDB.getModeInfo = profileQuery.value(14).toBool();
            profileDB.getVFOInfo = profileQuery.value(15).toBool();
            profileDB.getPWRInfo = profileQuery.value(16).toBool();
            profileDB.ritOffset = profileQuery.value(17).toDouble();
            profileDB.xitOffset = profileQuery.value(18).toDouble();
            profileDB.getRITInfo = profileQuery.value(19).toBool();
            profileDB.getXITInfo = profileQuery.value(20).toBool();
            profileDB.defaultPWR = profileQuery.value(21).toDouble();
            profileDB.getPTTInfo = profileQuery.value(22).toBool();
            profileDB.QSYWiping = profileQuery.value(23).toBool();
            profileDB.getKeySpeed = profileQuery.value(24).toBool();
            profileDB.assignedCWKey = profileQuery.value(25).toString();
            profileDB.keySpeedSync = profileQuery.value(26).toBool();
            profileDB.driver = profileQuery.value(27).toInt();
            profileDB.dxSpot2Rig = profileQuery.value(28).toBool();
            profileDB.pttType = profileQuery.value(29).toString();
            profileDB.pttPortPath = profileQuery.value(30).toString();

            addProfile(profileDB.profileName, profileDB);
        }
    }
    else
    {
        qInfo() << "Station Profile DB select error " << profileQuery.lastError().text();
    }
}

void RigProfilesManager::save()
{
    FCT_IDENTIFICATION;

    QSqlQuery deleteQuery;
    QSqlQuery insertQuery;

    if ( ! deleteQuery.prepare("DELETE FROM rig_profiles") )
    {
        qWarning() << "cannot prepare Delete statement";
        return;
    }

    if ( ! insertQuery.prepare("INSERT INTO rig_profiles(profile_name, model, port_pathname, hostname, netport, "
                               "baudrate, databits, stopbits, flowcontrol, parity, pollinterval, txfreq_start, "
                               "txfreq_end, get_freq, get_mode, get_vfo, get_pwr, rit_offset, xit_offset, get_rit, "
                               "get_xit, default_pwr, get_ptt, qsy_wiping, get_key_speed, assigned_cw_key, key_speed_sync, "
                               "driver, dxSpot2Rig, ptt_type, ptt_port_pathname ) "
                        "VALUES (:profile_name, :model, :port_pathname, :hostname, :netport, "
                               ":baudrate, :databits, :stopbits, :flowcontrol, :parity, :pollinterval, :txfreq_start, "
                               ":txfreq_end, :get_freq, :get_mode, :get_vfo, :get_pwr, :rit_offset, :xit_offset, :get_rit, "
                               ":get_xit, :default_pwr, :get_ptt, :qsy_wiping, :get_key_speed, :assigned_cw_key, :key_speed_sync, "
                               ":driver, :dxSpot2Rig, :ptt_type, :ptt_port_pathname)") )
    {
        qWarning() << "cannot prepare Insert statement";
        return;
    }

    if ( deleteQuery.exec() )
    {
        auto keys = profileNameList();
        for ( auto &key: qAsConst(keys) )
        {
            RigProfile rigProfile = getProfile(key);

            insertQuery.bindValue(":profile_name", key);
            insertQuery.bindValue(":model", rigProfile.model);
            insertQuery.bindValue(":port_pathname", rigProfile.portPath);
            insertQuery.bindValue(":hostname", rigProfile.hostname);
            insertQuery.bindValue(":netport", rigProfile.netport);
            insertQuery.bindValue(":baudrate", rigProfile.baudrate);
            insertQuery.bindValue(":databits", rigProfile.databits);
            insertQuery.bindValue(":stopbits", rigProfile.stopbits);
            insertQuery.bindValue(":flowcontrol", rigProfile.flowcontrol);
            insertQuery.bindValue(":parity", rigProfile.parity);
            insertQuery.bindValue(":pollinterval", rigProfile.pollInterval);
            insertQuery.bindValue(":txfreq_start", rigProfile.txFreqStart);
            insertQuery.bindValue(":txfreq_end", rigProfile.txFreqEnd);
            insertQuery.bindValue(":get_freq", rigProfile.getFreqInfo);
            insertQuery.bindValue(":get_mode", rigProfile.getModeInfo);
            insertQuery.bindValue(":get_vfo", rigProfile.getVFOInfo);
            insertQuery.bindValue(":get_pwr", rigProfile.getPWRInfo);
            insertQuery.bindValue(":rit_offset", rigProfile.ritOffset);
            insertQuery.bindValue(":xit_offset", rigProfile.xitOffset);
            insertQuery.bindValue(":get_rit", rigProfile.getRITInfo);
            insertQuery.bindValue(":get_xit", rigProfile.getXITInfo);
            insertQuery.bindValue(":default_pwr", rigProfile.defaultPWR);
            insertQuery.bindValue(":get_ptt", rigProfile.getPTTInfo);
            insertQuery.bindValue(":qsy_wiping", rigProfile.QSYWiping);
            insertQuery.bindValue(":get_key_speed", rigProfile.getKeySpeed);
            insertQuery.bindValue(":assigned_cw_key", rigProfile.assignedCWKey);
            insertQuery.bindValue(":key_speed_sync", rigProfile.keySpeedSync);
            insertQuery.bindValue(":driver", rigProfile.driver);
            insertQuery.bindValue(":dxSpot2Rig", rigProfile.dxSpot2Rig);
            insertQuery.bindValue(":ptt_type", rigProfile.pttType);
            insertQuery.bindValue(":ptt_port_pathname", rigProfile.pttPortPath);

            if ( ! insertQuery.exec() )
            {
                qInfo() << "Station Profile DB insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
            }
        }
    }
    else
    {
        qInfo() << "Station Profile DB delete error " << deleteQuery.lastError().text();
    }

    saveCurProfile1();
}

bool RigProfile::operator==(const RigProfile &profile)
{
    return (profile.profileName == this->profileName
            && profile.model == this->model
            && profile.portPath == this->portPath
            && profile.hostname == this->hostname
            && profile.netport == this->netport
            && profile.baudrate == this->baudrate
            && profile.databits == this->databits
            && profile.stopbits == this->stopbits
            && profile.flowcontrol == this->flowcontrol
            && profile.parity == this->parity
            && profile.pollInterval == this->pollInterval
            && profile.txFreqStart == this->txFreqStart
            && profile.txFreqEnd == this->txFreqEnd
            && profile.getFreqInfo == this->getFreqInfo
            && profile.getModeInfo == this->getModeInfo
            && profile.getVFOInfo == this->getVFOInfo
            && profile.getPWRInfo == this->getPWRInfo
            && profile.ritOffset == this->ritOffset
            && profile.xitOffset == this->xitOffset
            && profile.getRITInfo == this->getRITInfo
            && profile.getXITInfo == this->getXITInfo
            && profile.defaultPWR == this->defaultPWR
            && profile.getPTTInfo == this->getPTTInfo
            && profile.QSYWiping == this->QSYWiping
            && profile.getKeySpeed == this->getKeySpeed
            && profile.assignedCWKey == this->assignedCWKey
            && profile.keySpeedSync == this->keySpeedSync
            && profile.driver == this->driver
            && profile.dxSpot2Rig == this->dxSpot2Rig
            && profile.pttType == this->pttType
            && profile.pttPortPath == this->pttPortPath
            );
}

bool RigProfile::operator!=(const RigProfile &profile)
{
    return !operator==(profile);
}

QString RigProfile::toHTMLString() const
{
    QString ret = "<b>" + QObject::tr("My Rig") + ":</b> " + profileName + "<br/>";

    return ret;
}

RigProfile::rigPortType RigProfile::getPortType() const
{
    FCT_IDENTIFICATION;

    if ( driver == Rig::OMNIRIG_DRIVER
        || driver == Rig::OMNIRIGV2_DRIVER )
        return RigProfile::SPECIAL_OMNIRIG_ATTACHED;

    if ( !hostname.isEmpty()
         && portPath.isEmpty() )
    {
        return RigProfile::NETWORK_ATTACHED;
    }
    return RigProfile::SERIAL_ATTACHED;
}
