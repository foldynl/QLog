#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include "PlatformParameterManager.h"
#include "core/LogParam.h"
#include "core/LogDatabase.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.platformparametermanager");

QList<PlatformParameterManager::KnownParamDef> PlatformParameterManager::knownParameters()
{
    FCT_IDENTIFICATION;

    // List of known platform-dependent parameters.
    // These are the parameters that are stored by LogParam class

    return
    {
        { "services/lotw/callbook/tqsl", QObject::tr("TQSL Path"), true }
    };
}

QList<PlatformParameter> PlatformParameterManager::getParameters(const QString &importDbPath,
                                                                 const QString &sourcePlatform)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << importDbPath << sourcePlatform;

    QList<PlatformParameter> result;

    const QString currentPlatform = LogDatabase::currentPlatformId();
    const bool platformDiffers = (sourcePlatform != currentPlatform);
    const bool targetIsFlatpak = (currentPlatform == LogDatabase::PLATFORM_LINUXFLATPAK);
    const bool sourceIsFlatpak = (sourcePlatform == LogDatabase::PLATFORM_LINUXFLATPAK);

    qCDebug(runtime) << "Source platform:" << sourcePlatform
                     << "Current platform:" << currentPlatform
                     << "Differs:" << platformDiffers;

    // Open the imported database
    const QString connectionName = QStringLiteral("PlatformParamImport");

    {
        QSqlDatabase importDb = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        importDb.setDatabaseName(importDbPath);

        if ( !importDb.open() )
        {
            qWarning() << "Cannot open import database:" << importDb.lastError().text();
            QSqlDatabase::removeDatabase(connectionName);
            return result;
        }

        // params defined in LogParam
        const QList<PlatformParameterManager::KnownParamDef> known = knownParameters();

        for ( const PlatformParameterManager::KnownParamDef &param : known )
        {
            // - If target is Flatpak: skip (path is fixed, e.g. /app/bin/tqsl)
            if ( param.isExecutablePath && targetIsFlatpak )
            {
                qCDebug(runtime) << "Skipping" << param.key << "- target is Flatpak (fixed path)";
                continue;
            }

            PlatformParameter p;
            p.key = param.key;
            p.displayName = param.displayName;
            p.requiresChange = platformDiffers;
            p.newValue = QString();

            // Read the value from the imported database
            // do not use LogParam function here because the class gets value from the main DB.
            QSqlQuery query(importDb);
            if ( query.prepare("SELECT value FROM log_param WHERE name = :name") )
            {
                query.bindValue(":name", p.key);
                if ( query.exec() && query.first() )
                {
                    // For executable paths from Flatpak source: path is not useful
                    if ( param.isExecutablePath && sourceIsFlatpak )
                        p.currentValue = QObject::tr("(Flatpak internal path)");
                    else
                        p.currentValue = query.value(0).toString();
                }
            }

            result.append(p);
        }

        importDb.close();
    }

    QSqlDatabase::removeDatabase(connectionName);

    return result;
}

bool PlatformParameterManager::applyParameters(const QList<PlatformParameter> &params)
{
    FCT_IDENTIFICATION;

    for ( const PlatformParameter &p : params )
    {
        if ( p.newValue.isEmpty() )
            continue;
        if ( !LogParam::isLoTWTQSLPathKey(p.key) || !LogParam::setLoTWTQSLPath(p.newValue) )
            return false;
    }
    return true;
}

bool PlatformParameterManager::applyFlatpakFixedPaths()
{
    FCT_IDENTIFICATION;

    if ( LogDatabase::currentPlatformId() != LogDatabase::PLATFORM_LINUXFLATPAK )
        return true;

    if ( !LogParam::setLoTWTQSLPath(QString()) )
        return false;

    QSqlQuery query;
    if ( !query.exec("UPDATE rig_profiles SET rigctld_path = NULL") )
    {
        qCWarning(runtime) << "Cannot clear rigctld paths:" << query.lastError();
        return false;
    }
    return true;
}

QList<ProfileParameter> PlatformParameterManager::getProfileParameters(const QString &importDbPath,
                                                                       const QString &sourcePlatform)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << importDbPath << sourcePlatform;

    QList<ProfileParameter> result;

    const QString currentPlatform = LogDatabase::currentPlatformId();
    const bool targetIsFlatpak = (currentPlatform == LogDatabase::PLATFORM_LINUXFLATPAK);
    const bool sourceIsFlatpak = (sourcePlatform == LogDatabase::PLATFORM_LINUXFLATPAK);
    const QString connectionName = QStringLiteral("ProfilePortParamImport");

    {
        QSqlDatabase importDb = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        importDb.setDatabaseName(importDbPath);

        if ( !importDb.open() )
        {
            qWarning() << "Cannot open import database:" << importDb.lastError().text();
            QSqlDatabase::removeDatabase(connectionName);
            return result;
        }

        // Define profile tables and their port columns
        struct ProfileDef
        {
            QString tableName;
            QString columnName;
            QString displayPrefix;
            bool isExecutablePath;  // true for paths to executables (like rigctld_path)
        };

        QList<ProfileDef> profileDefs =
        {
            { "rig_profiles",   "port_pathname",     QObject::tr("Rig"),         false },
            { "rig_profiles",   "ptt_port_pathname", QObject::tr("Rig PTT"),     false },
            { "rig_profiles",   "rigctld_path",      QObject::tr("Rig rigctld"), true },
            { "rot_profiles",   "port_pathname",     QObject::tr("Rotator"),     false },
            { "cwkey_profiles", "port_pathname",     QObject::tr("CW Keyer"),    false }
        };

        for ( const ProfileDef &def : profileDefs )
        {
            if ( def.isExecutablePath && targetIsFlatpak )
            {
                qCDebug(runtime) << "Skipping" << def.columnName << "- target is Flatpak (fixed path)";
                continue;
            }

            QSqlQuery query(importDb);
            QString sql = QString("SELECT profile_name, %1 FROM %2 WHERE %1 IS NOT NULL AND %1 != ''")
                              .arg(def.columnName, def.tableName);

            if ( !query.exec(sql) )
            {
                qCDebug(runtime) << "Query failed for" << def.tableName << ":" << query.lastError().text();
                continue;
            }

            while ( query.next() )
            {
                ProfileParameter p;
                p.tableName = def.tableName;
                p.profileName = query.value(0).toString();
                p.columnName = def.columnName;
                p.displayName = QString("%1: %2").arg(def.displayPrefix, p.profileName);
                p.newValue = QString();
                p.isExecutablePath = def.isExecutablePath;

                if ( def.isExecutablePath && sourceIsFlatpak )
                    p.currentValue = QObject::tr("(Flatpak internal path)");
                else
                    p.currentValue = query.value(1).toString();

                result.append(p);
            }
        }

        importDb.close();
    }

    QSqlDatabase::removeDatabase(connectionName);

    qCDebug(runtime) << "Found" << result.size() << "profile port parameters";
    return result;
}

bool PlatformParameterManager::applyProfileParameters(const QList<ProfileParameter> &params)
{
    FCT_IDENTIFICATION;

    for ( const ProfileParameter &p : params )
    {
        if ( p.newValue.isEmpty() )
            continue;

        const bool validColumn = p.columnName == "port_pathname"
                                 && (p.tableName == "rig_profiles"
                                     || p.tableName == "rot_profiles"
                                     || p.tableName == "cwkey_profiles");
        const bool validRigColumn = p.tableName == "rig_profiles"
                                    && (p.columnName == "ptt_port_pathname"
                                        || p.columnName == "rigctld_path");

        if ( !validColumn && !validRigColumn )
        {
            qCWarning(runtime) << "Invalid import profile field:" << p.tableName << p.columnName;
            return false;
        }

        QSqlQuery query;
        const QString sql = QString("UPDATE %1 SET %2 = :value WHERE profile_name = :profile")
                                .arg(p.tableName, p.columnName);

        if ( !query.prepare(sql) )
        {
            qCWarning(runtime) << "Cannot prepare profile update:" << query.lastError();
            return false;
        }

        query.bindValue(":value", p.newValue);
        query.bindValue(":profile", p.profileName);

        if ( !query.exec() || query.numRowsAffected() != 1 )
        {
            qCWarning(runtime) << "Cannot update import profile:" << p.profileName << query.lastError();
            return false;
        }
    }
    return true;
}
