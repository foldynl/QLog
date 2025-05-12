#ifndef QLOG_DATA_MAINLAYOUTPROFILE_H
#define QLOG_DATA_MAINLAYOUTPROFILE_H

#include <QString>
#include <QObject>
#include <QDataStream>

#include "data/ProfileManager.h"

class MainLayoutProfile
{

public:
    MainLayoutProfile()
    {
        darkMode = 0;
        tabsexpanded = true;
    };

    QString profileName;
    QList<int> rowA;
    QList<int> rowB;
    QList<int> detailColA;
    QList<int> detailColB;
    QList<int> detailColC;
    QByteArray mainGeometry;
    QByteArray mainState;
    int darkMode;
    bool tabsexpanded;
    QList<QPair<QString, QString>> addlBandmaps;

    bool operator== (const MainLayoutProfile &profile);
    bool operator!= (const MainLayoutProfile &profile);
    static MainLayoutProfile getClassicLayout();

private:
    friend QDataStream& operator<<(QDataStream& out, const MainLayoutProfile& v);
    friend QDataStream& operator>>(QDataStream& in, MainLayoutProfile& v);

};

Q_DECLARE_METATYPE(MainLayoutProfile);

class MainLayoutProfilesManager : public ProfileManagerSQL<MainLayoutProfile>
{
    Q_OBJECT

public:

    explicit MainLayoutProfilesManager();
    ~MainLayoutProfilesManager() { };

    static MainLayoutProfilesManager* instance()
    {
        static MainLayoutProfilesManager instance;
        return &instance;
    };
    void save();

    static QString toDBStringList(const QList<int> &list);
    static QString toDBStringList(const QList<QPair<QString, QString>> &list);
    QList<int> toIntList(const QString &list) const;
    static QList<QPair<QString, QString>> toPairStringList(const QString &list);
};


#endif // QLOG_DATA_MAINLAYOUTPROFILE_H
