#include "BandmapGuide.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QUuid>

#include "data/BandPlan.h"
#include "core/LogParam.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.bandmapguide");

BandmapGuide::BandmapGuide(QObject *parent) :
    QObject(parent)
{
}

BandmapGuide *BandmapGuide::instance()
{
    static BandmapGuide guide;

    return &guide;
}

void BandmapGuide::notifyChanged()
{
    FCT_IDENTIFICATION;

    emit instance()->changed();
}

BandmapGuide::Range::Range(double rangeFrom,
                           double rangeTo,
                           const QColor &rangeColor,
                           const QString &rangeLabel) :
    from(rangeFrom),
    to(rangeTo),
    color(rangeColor),
    label(rangeLabel)
{
}

bool BandmapGuide::Range::isValid() const
{
    return from < to && color.isValid();
}

bool BandmapGuide::Profile::isValid() const
{
    return !name.trimmed().isEmpty();
}

QList<BandmapGuide::Profile> BandmapGuide::profiles()
{
    FCT_IDENTIFICATION;

    const QString storedProfiles = LogParam::getBandmapGuideProfiles();
    QList<Profile> profileList = profilesFromJson(storedProfiles);

    if ( profileList.isEmpty() && storedProfiles.trimmed().isEmpty() )
        profileList.append(exampleProfile());

    return profileList;
}

void BandmapGuide::saveProfiles(const QList<Profile> &profiles)
{
    FCT_IDENTIFICATION;

    LogParam::setBandmapGuideProfiles(profilesToJson(profiles));
    notifyChanged();
}

QString BandmapGuide::currentProfileId()
{
    FCT_IDENTIFICATION;

    return LogParam::getBandmapGuideCurrentProfile();
}

void BandmapGuide::setCurrentProfileId(const QString &id)
{
    FCT_IDENTIFICATION;

    if ( LogParam::getBandmapGuideCurrentProfile() == id )
        return;

    LogParam::setBandmapGuideCurrentProfile(id);
    notifyChanged();
}

BandmapGuide::Profile BandmapGuide::currentProfile()
{
    FCT_IDENTIFICATION;

    const QString currentId = currentProfileId();
    const QList<Profile> profileList = profiles();

    for ( const Profile &profile : profileList )
        if ( profile.id == currentId )
            return profile;

    if ( !profileList.isEmpty() )
        return profileList.first();

    return Profile();
}

bool BandmapGuide::profileExists(const QString &id)
{
    FCT_IDENTIFICATION;

    if ( id.isEmpty() )
        return false;

    const QList<Profile> profileList = profiles();
    for ( const Profile &profile : profileList )
        if ( profile.id == id )
            return true;

    return false;
}

bool BandmapGuide::isEnabled()
{
    FCT_IDENTIFICATION;

    return LogParam::getBandmapGuideEnabled();
}

void BandmapGuide::setEnabled(bool state)
{
    FCT_IDENTIFICATION;

    if ( LogParam::getBandmapGuideEnabled() == state )
        return;

    LogParam::setBandmapGuideEnabled(state);
    notifyChanged();
}

QString BandmapGuide::newProfileId()
{
    FCT_IDENTIFICATION;

    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

BandmapGuide::Range BandmapGuide::defaultRange()
{
    FCT_IDENTIFICATION;

    return Range(14.000, 14.100, QColor::fromRgb(0x4d8ef7));
}

BandmapGuide::Profile BandmapGuide::exampleProfile()
{
    FCT_IDENTIFICATION;

    const QColor cwColor = QColor::fromRgb(0x4d8ef7);
    const QColor digitalColor = QColor::fromRgb(0xa6a435);
    const QColor phoneColor = QColor::fromRgb(0xd92f2f);

    Profile profile;
    profile.id = QStringLiteral("iaru-region-1");
    profile.name = QObject::tr("IARU Region 1");

    const QList<BandPlan::BandModeRange> bandRanges = BandPlan::r1BandModeRanges();
    for ( const BandPlan::BandModeRange &bandRange : bandRanges )
    {
        QColor color;
        QString label;

        switch ( bandRange.mode )
        {
        case BandPlan::BAND_MODE_CW:
            color = cwColor;
            label = BandPlan::MODE_GROUP_STRING_CW;
            break;

        case BandPlan::BAND_MODE_DIGITAL:
        case BandPlan::BAND_MODE_FT8:
        case BandPlan::BAND_MODE_FT4:
        case BandPlan::BAND_MODE_FT2:
            color = digitalColor;
            label = BandPlan::MODE_GROUP_STRING_DIGITAL;
            break;

        case BandPlan::BAND_MODE_PHONE:
        case BandPlan::BAND_MODE_LSB:
        case BandPlan::BAND_MODE_USB:
            color = phoneColor;
            label = BandPlan::MODE_GROUP_STRING_PHONE;
            break;

        case BandPlan::BAND_MODE_UNKNOWN:
            continue;
        }

        const Range range(bandRange.start, bandRange.end, color, label);
        if ( !profile.ranges.isEmpty() )
        {
            Range &lastRange = profile.ranges.last();
            if ( lastRange.to == range.from
                 && lastRange.color == range.color
                 && lastRange.label == range.label )
            {
                lastRange.to = range.to;
                continue;
            }
        }

        profile.ranges.append(range);
    }

    return profile;
}

bool BandmapGuide::writeProfileToFile(const Profile &profile,
                                      const QString &filePath,
                                      QString *error)
{
    FCT_IDENTIFICATION;

    QFile file(filePath);

    if ( !file.open(QIODevice::WriteOnly | QIODevice::Truncate) )
    {
        if ( error ) *error = QObject::tr("Failed to write file: %1").arg(filePath);
        return false;
    }

    const QJsonDocument document(profileToJsonObject(profile, false));
    if ( file.write(document.toJson(QJsonDocument::Indented)) < 0 )
    {
        if ( error ) *error = QObject::tr("Failed to write file: %1").arg(filePath);
        return false;
    }

    return true;
}

BandmapGuide::Profile BandmapGuide::readProfileFromFile(const QString &filePath,
                                                        QString *error)
{
    FCT_IDENTIFICATION;

    QFile file(filePath);

    if ( !file.open(QIODevice::ReadOnly) )
    {
        if ( error ) *error = QObject::tr("Cannot open file: %1").arg(filePath);
        return Profile();
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if ( parseError.error != QJsonParseError::NoError || !document.isObject() )
    {
        if ( error ) *error = QObject::tr("Invalid guide file: %1").arg(parseError.errorString());
        return Profile();
    }

    Profile profile;
    const QJsonObject root = document.object();

    if ( root.value("profiles").isArray() )
    {
        const QJsonArray profileArray = root.value("profiles").toArray();
        if ( !profileArray.isEmpty() )
            profile = profileFromJsonObject(profileArray.first().toObject());
    }
    else
    {
        profile = profileFromJsonObject(root);
    }

    if ( profile.name.trimmed().isEmpty() )
        profile.name = QFileInfo(filePath).completeBaseName();

    profile.id = newProfileId();

    if ( !profile.isValid() )
    {
        if ( error ) *error = QObject::tr("Invalid guide file: missing title");
        return Profile();
    }

    return profile;
}

QString BandmapGuide::profilesToJson(const QList<Profile> &profiles)
{
    FCT_IDENTIFICATION;

    QJsonArray profileArray;

    for ( const Profile &profile : profiles )
        if ( profile.isValid() )
            profileArray.append(profileToJsonObject(profile));

    QJsonObject root;
    root.insert("version", 1);
    root.insert("profiles", profileArray);

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QList<BandmapGuide::Profile> BandmapGuide::profilesFromJson(const QString &json)
{
    FCT_IDENTIFICATION;

    QList<Profile> result;
    if ( json.trimmed().isEmpty() )
        return result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if ( parseError.error != QJsonParseError::NoError || !document.isObject() )
    {
        qCWarning(runtime) << "Cannot parse bandmap guide profiles" << parseError.errorString();
        return result;
    }

    const QJsonArray profileArray = document.object().value("profiles").toArray();
    for ( const QJsonValue &value : profileArray )
    {
        Profile profile = profileFromJsonObject(value.toObject());
        if ( profile.id.isEmpty() )
            profile.id = newProfileId();
        if ( profile.isValid() )
            result.append(profile);
    }

    return result;
}

QJsonObject BandmapGuide::profileToJsonObject(const Profile &profile,
                                              bool includeId)
{
    FCT_IDENTIFICATION;

    QJsonArray rangeArray;
    for ( const Range &range : profile.ranges )
        if ( range.isValid() )
            rangeArray.append(rangeToJsonObject(range));

    QJsonObject object;
    object.insert("version", 1);
    if ( includeId )
        object.insert("id", profile.id);
    object.insert("title", profile.name);
    object.insert("ranges", rangeArray);
    return object;
}

BandmapGuide::Profile BandmapGuide::profileFromJsonObject(const QJsonObject &object)
{
    FCT_IDENTIFICATION;

    Profile profile;
    profile.id = object.value("id").toString();
    profile.name = object.value("title").toString(object.value("name").toString()).trimmed();

    const QJsonArray rangeArray = object.value("ranges").toArray();
    for ( const QJsonValue &value : rangeArray )
    {
        Range range = rangeFromJsonObject(value.toObject());
        if ( range.isValid() )
            profile.ranges.append(range);
    }

    return profile;
}

QJsonObject BandmapGuide::rangeToJsonObject(const Range &range)
{
    FCT_IDENTIFICATION;

    QJsonObject object;
    object.insert("from", range.from);
    object.insert("to", range.to);

    object.insert("color", range.color.name(QColor::HexRgb));
    object.insert("label", range.label);
    return object;
}

BandmapGuide::Range BandmapGuide::rangeFromJsonObject(const QJsonObject &object)
{
    FCT_IDENTIFICATION;

    Range range;
    range.from = object.value("from").toDouble();
    range.to = object.value("to").toDouble();
    range.color = QColor(object.value("color").toString());
    range.label = object.value("label").toString();
    return range;
}
