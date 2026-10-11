#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QSaveFile>

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

void PlatformParameterManager::applyParameters(const QList<PlatformParameter> &params)
{
    FCT_IDENTIFICATION;

    for ( const PlatformParameter &p : params )
    {
        // Only apply if user provided a new value
        if ( !p.newValue.isEmpty() )
        {
            qCDebug(runtime) << "Applying parameter:" << p.key << "=" << p.newValue;

            // Use LogParam to set the value in the current database
            // This method is called when the imported DB becomes the default DB.
            // Therefore, we can use the LogParam class.
            if ( LogParam::isLoTWTQSLPathKey(p.key) ) LogParam::setLoTWTQSLPath(p.newValue);
            // Add more parameter handlers here as needed
        }
    }
}

void PlatformParameterManager::applyFlatpakFixedPaths()
{
    FCT_IDENTIFICATION;

    const QString currentPlatform = LogDatabase::currentPlatformId();

    if ( currentPlatform != LogDatabase::PLATFORM_LINUXFLATPAK )
    {
        qCDebug(runtime) << "Not Flatpak target, skipping fixed paths";
        return;
    }

    qCDebug(runtime) << "Applying Flatpak fixed paths";

    // Set TQSL path to fixed Flatpak location - empty is OK because flatpak has built-in value.
    LogParam::setLoTWTQSLPath("");

    // Clear rigctld_path in all rig profiles (empty = autodetect will find /app/bin/rigctld)
    QSqlQuery query;
    if ( query.exec("UPDATE rig_profiles SET rigctld_path = NULL") )
        qCDebug(runtime) << "Cleared rigctld_path in all rig profiles";
    else
        qWarning() << "Failed to clear rigctld_path:" << query.lastError().text();
}

QString PlatformParameterManager::pendingParametersPath()
{
    return LogDatabase::dbDirectory().filePath("qlog.db.pending.params");
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

void PlatformParameterManager::applyProfileParameters(const QList<ProfileParameter> &params)
{
    FCT_IDENTIFICATION;

    for ( const ProfileParameter &p : params )
    {
        if ( p.newValue.isEmpty() )
            continue;

        qCDebug(runtime) << "Applying profile port parameter:" << p.tableName
                         << p.profileName << p.columnName << "=" << p.newValue;

        QSqlQuery query;
        QString sql = QString("UPDATE %1 SET %2 = :value WHERE profile_name = :profile")
                          .arg(p.tableName, p.columnName);

        if ( !query.prepare(sql) )
        {
            qWarning() << "Cannot prepare update query:" << query.lastError().text();
            continue;
        }

        query.bindValue(":value", p.newValue);
        query.bindValue(":profile", p.profileName);

        if ( !query.exec() )
            qWarning() << "Cannot update profile port:" << query.lastError().text();
    }
}

bool PlatformParameterManager::saveParametersToFile(const QList<PlatformParameter> &params,
                                                     const QList<ProfileParameter> &profileParams,
                                                     const QString &filePath)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filePath;

    QJsonObject root;

    // Save log_param parameters
    QJsonArray paramArray;
    for ( const PlatformParameter &p : params )
    {
        if ( !p.newValue.isEmpty() )
        {
            QJsonObject obj;
            obj["key"] = p.key;
            obj["newValue"] = p.newValue;
            paramArray.append(obj);
        }
    }
    root["parameters"] = paramArray;

    // Save profile port parameters
    QJsonArray profileArray;
    for ( const ProfileParameter &p : profileParams )
    {
        if ( !p.newValue.isEmpty() )
        {
            QJsonObject obj;
            obj["tableName"] = p.tableName;
            obj["profileName"] = p.profileName;
            obj["columnName"] = p.columnName;
            obj["newValue"] = p.newValue;
            obj["isExec"] = p.isExecutablePath;
            profileArray.append(obj);
        }
    }
    root["profilePorts"] = profileArray;

    QJsonDocument doc(root);
    QSaveFile file(filePath);
    file.setDirectWriteFallback(false);

    if ( !file.open(QIODevice::WriteOnly) )
    {
        qCWarning(runtime) << "Cannot open file for writing:" << filePath << file.errorString();
        return false;
    }

    const QByteArray data = doc.toJson(QJsonDocument::Compact);
    // Some Qt5 builds do not propagate buffered write failures from commit().
    if ( file.write(data) != data.size() || !file.flush() || !file.commit() )
    {
        qCWarning(runtime) << "Cannot save import parameters:" << filePath << file.errorString();
        return false;
    }

    qCDebug(runtime) << "Saved" << paramArray.size() << "params and"
                     << profileArray.size() << "profile ports to" << filePath;
    return true;
}

QList<PlatformParameter> PlatformParameterManager::loadParametersFromFile(const QString &filePath)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filePath;

    QList<PlatformParameter> result;

    QFile file(filePath);
    if ( !file.exists() )
    {
        qCDebug(runtime) << "No parameters file found";
        return result;
    }

    if ( !file.open(QIODevice::ReadOnly) )
    {
        qWarning() << "Cannot open parameters file:" << filePath;
        return result;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if ( error.error != QJsonParseError::NoError )
    {
        qWarning() << "JSON parse error:" << error.errorString();
        return result;
    }

    const QJsonObject root = doc.object();
    const QJsonArray array = root["parameters"].toArray();

    for ( const QJsonValue &val : array )
    {
        QJsonObject obj = val.toObject();
        PlatformParameter p;
        p.key = obj["key"].toString();
        p.newValue = obj["newValue"].toString();
        p.requiresChange = false;
        result.append(p);
    }

    qCDebug(runtime) << "Loaded" << result.size() << "parameters from" << filePath;
    return result;
}

QList<ProfileParameter> PlatformParameterManager::loadProfileParametersFromFile(const QString &filePath)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filePath;

    QList<ProfileParameter> result;

    QFile file(filePath);
    if ( !file.exists() )
    {
        qCDebug(runtime) << "No parameters file found";
        return result;
    }

    if ( !file.open(QIODevice::ReadOnly) )
    {
        qWarning() << "Cannot open parameters file:" << filePath;
        return result;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if ( error.error != QJsonParseError::NoError )
    {
        qWarning() << "JSON parse error:" << error.errorString();
        return result;
    }

    const QJsonObject root = doc.object();
    const QJsonArray array = root["profilePorts"].toArray();

    for ( const QJsonValue &val : array )
    {
        QJsonObject obj = val.toObject();
        ProfileParameter p;
        p.tableName = obj["tableName"].toString();
        p.profileName = obj["profileName"].toString();
        p.columnName = obj["columnName"].toString();
        p.newValue = obj["newValue"].toString();
        p.isExecutablePath = obj["isExec"].toBool();
        result.append(p);
    }

    qCDebug(runtime) << "Loaded" << result.size() << "profile port parameters from" << filePath;
    return result;
}
