#ifndef QLOG_DATA_CWSHORTCUTPROFILE_H
#define QLOG_DATA_CWSHORTCUTPROFILE_H

#include <QString>
#include <QObject>
#include <QDataStream>

#include "data/ProfileManager.h"

#define MAX_SHORTCUT_KEYS 7

class CWShortcutProfile
{
public:
    CWShortcutProfile()
    {
        shortDescs.resize(MAX_SHORTCUT_KEYS);
        macros.resize(MAX_SHORTCUT_KEYS);
    };

    QString profileName;
    QVector<QString> shortDescs;
    QVector<QString> macros;

    bool operator== (const CWShortcutProfile &profile);
    bool operator!= (const CWShortcutProfile &profile);

private:
    friend QDataStream& operator<<(QDataStream& out, const CWShortcutProfile& v);
    friend QDataStream& operator>>(QDataStream& in, CWShortcutProfile& v);
};

Q_DECLARE_METATYPE(CWShortcutProfile);

class CWShortcutProfilesManager : QObject, public ProfileManagerSQL<CWShortcutProfile>
{
    Q_OBJECT

public:

    explicit CWShortcutProfilesManager(QObject *parent = nullptr);
    ~CWShortcutProfilesManager() { };

    static CWShortcutProfilesManager* instance();
    void save();

};

#endif // QLOG_DATA_CWSHORTCUTPROFILE_H
