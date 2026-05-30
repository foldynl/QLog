#ifndef QLOG_DATA_BANDMAPGUIDE_H
#define QLOG_DATA_BANDMAPGUIDE_H

#include <QColor>
#include <QList>
#include <QObject>
#include <QString>

class QJsonObject;

class BandmapGuide : public QObject
{
    Q_OBJECT

public:
    static BandmapGuide *instance();

    struct Range
    {
        double from = 0.0;
        double to = 0.0;
        QColor color;
        QString label;

        Range() = default;
        Range(double rangeFrom,
              double rangeTo,
              const QColor &rangeColor,
              const QString &rangeLabel = QString());

        bool isValid() const;
    };

    struct Profile
    {
        QString id;
        QString name;
        QList<Range> ranges;

        bool isValid() const;
    };

    static QList<Profile> profiles();
    static void saveProfiles(const QList<Profile> &profiles);

    static QString currentProfileId();
    static void setCurrentProfileId(const QString &id);
    static Profile currentProfile();
    static bool profileExists(const QString &id);

    static bool isEnabled();
    static void setEnabled(bool state);

    static QString newProfileId();
    static Range defaultRange();
    static Profile exampleProfile();

    static bool writeProfileToFile(const Profile &profile,
                                   const QString &filePath,
                                   QString *error = nullptr);
    static Profile readProfileFromFile(const QString &filePath,
                                       QString *error = nullptr);

signals:
    void changed();

private:
    explicit BandmapGuide(QObject *parent = nullptr);
    static void notifyChanged();

    static QString profilesToJson(const QList<Profile> &profiles);
    static QList<Profile> profilesFromJson(const QString &json);
    static QJsonObject profileToJsonObject(const Profile &profile,
                                           bool includeId = true);
    static Profile profileFromJsonObject(const QJsonObject &object);

    static QJsonObject rangeToJsonObject(const Range &range);
    static Range rangeFromJsonObject(const QJsonObject &object);
};

#endif // QLOG_DATA_BANDMAPGUIDE_H
