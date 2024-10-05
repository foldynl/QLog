#ifndef QLOG_DATA_MAINLAYOUTPROFILE_H
#define QLOG_DATA_MAINLAYOUTPROFILE_H

#include <QString>
#include <QObject>
#include <QDataStream>

#include "data/ProfileManager.h"

class MainLayoutProfile
{
public:
    MainLayoutProfile(){darkMode = false;};

    QString profileName;
    QList<int> rowA;
    QList<int> rowB;
    QList<int> detailColA;
    QList<int> detailColB;
    QList<int> detailColC;
    QByteArray mainGeometry;
    QByteArray mainState;
    bool darkMode;

    bool operator== (const MainLayoutProfile &profile);
    bool operator!= (const MainLayoutProfile &profile);
    static MainLayoutProfile getClassicLayout();

private:
    friend QDataStream& operator<<(QDataStream& out, const MainLayoutProfile& v);
    friend QDataStream& operator>>(QDataStream& in, MainLayoutProfile& v);

};

Q_DECLARE_METATYPE(MainLayoutProfile);

class MainLayoutProfilesManager : QObject, public ProfileManagerSQL<MainLayoutProfile>
{
    Q_OBJECT

public:

    explicit MainLayoutProfilesManager(QObject *parent = nullptr);
    ~MainLayoutProfilesManager() { };

    static MainLayoutProfilesManager* instance()
    {
        static MainLayoutProfilesManager instance;
        return &instance;
    };
    void save();

    QString toDBStringList(const QList<int> &list) const;
    QList<int> toIntList(const QString &list) const;
};


#endif // QLOG_DATA_MAINLAYOUTPROFILE_H
