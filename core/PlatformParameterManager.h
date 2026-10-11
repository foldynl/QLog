#ifndef QLOG_CORE_PLATFORMPARAMETERMANAGER_H
#define QLOG_CORE_PLATFORMPARAMETERMANAGER_H

#include <QString>
#include <QList>
#include <QPair>

struct PlatformParameter
{
    QString key;           // LogParam key (e.g. "services/lotw/callbook/tqsl")
    QString displayName;   // Human-readable name (e.g. "TQSL Path")
    QString currentValue;  // Value from imported DB
    QString newValue;      // User-provided value for current platform
    bool requiresChange;   // true if platform differs
};

// Structure for profile-based port paths (rig, rotator, cwkey)
struct ProfileParameter
{
    QString tableName;     // e.g. "rig_profiles"
    QString profileName;   // e.g. "My Rig"
    QString columnName;    // e.g. "port_pathname"
    QString displayName;   // e.g. "Rig: My Rig - Serial Port"
    QString currentValue;  // Value from imported DB
    QString newValue;      // User-provided value for current platform
    bool isExecutablePath; // is exec - browse should be displayer
};

class PlatformParameterManager
{
public:
    // Get list of platform-dependent parameters from imported DB
    // importDbPath: path to the imported database file
    // sourcePlatform: platform from which the DB was exported
    // Returns list of parameters that may need adjustment
    static QList<PlatformParameter> getParameters(const QString &importDbPath,
                                                  const QString &sourcePlatform);

    // Apply user-provided values to the current database
    // params: list of parameters with newValue set by user
    static bool applyParameters(const QList<PlatformParameter> &params);

    // Apply fixed paths for Flatpak target (called during import)
    // Sets TQSL path to /app/bin/tqsl and clears rigctld_path in all profiles
    static bool applyFlatpakFixedPaths();

    // Get list of profile port parameters from imported DB
    // sourcePlatform: platform from which the DB was exported
    static QList<ProfileParameter> getProfileParameters(const QString &importDbPath,
                                                                 const QString &sourcePlatform);

    // Apply profile port parameters to the current database
    static bool applyProfileParameters(const QList<ProfileParameter> &params);

private:
    // Structure for known platform-dependent parameters
    struct KnownParamDef
    {
        QString key;            // LogParam key
        QString displayName;    // Human-readable name
        bool isExecutablePath;  // true for paths to executables
    };

    // Template list of known platform-dependent parameters
    static QList<KnownParamDef> knownParameters();
};

#endif // QLOG_CORE_PLATFORMPARAMETERMANAGER_H
