#include "AdifRecovery.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.adifrecovery");

QString AdifRecovery::normalizePath(const QString &path)
{
    QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}

QString AdifRecovery::fileKey(const QString &path, const QString &stationProfileName)
{
    const QString key = normalizePath(path) + QChar(0x1f) + stationProfileName.trimmed();
    return QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(),
                                                        QCryptographicHash::Sha256).toHex());
}

QString AdifRecovery::serializeConfigList(const QList<AdifRecoveryConfig> &configs)
{
    QJsonArray array;
    for ( const AdifRecoveryConfig &config : configs )
    {
        if ( config.path.trimmed().isEmpty() )
            continue;

        QJsonObject object;
        object.insert("enabled", config.enabled);
        object.insert("stationProfileName", config.stationProfileName);
        object.insert("qslSentStatusDefault", config.qslSentStatusDefault.isEmpty() ? QStringLiteral("Q")
                                                                                    : config.qslSentStatusDefault);
        object.insert("path", config.path);
        array.append(object);
    }

    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QList<AdifRecoveryConfig> AdifRecovery::deserializeConfigList(const QString &data)
{
    QList<AdifRecoveryConfig> configs;
    const QJsonDocument document = QJsonDocument::fromJson(data.toUtf8());

    if ( !document.isArray() )
        return configs;

    for ( const QJsonValue &value : static_cast<const QJsonArray &>(document.array()) )
    {
        const QJsonObject object = value.toObject();
        AdifRecoveryConfig config;
        config.enabled = object.value("enabled").toBool(true);
        config.stationProfileName = object.value("stationProfileName").toString();
        config.qslSentStatusDefault = object.value("qslSentStatusDefault").toString("Q");
        config.path = object.value("path").toString();
        if ( !config.path.trimmed().isEmpty() )
            configs.append(config);
    }

    return configs;
}

QString AdifRecovery::serializeState(const AdifRecoveryState &state)
{
    QJsonObject object;

    object.insert("path", state.path);
    object.insert("offset", state.offset);
    object.insert("lastRecoveryAt", state.lastRecoveryAt.toUTC().toString(Qt::ISODate));
    object.insert("lastMessage", state.lastMessage);

    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

AdifRecoveryState AdifRecovery::deserializeState(const QString &data)
{
    AdifRecoveryState state;
    const QJsonDocument document = QJsonDocument::fromJson(data.toUtf8());

    if ( !document.isObject() )
        return state;

    const QJsonObject object = document.object();
    state.path = object.value("path").toString();
    state.offset = static_cast<qint64>(object.value("offset").toDouble(-1));
    state.lastRecoveryAt = QDateTime::fromString(object.value("lastRecoveryAt").toString(), Qt::ISODate);
    state.lastMessage = object.value("lastMessage").toString();

    return state;
}

AdifRecoveryReaderWorker::AdifRecoveryReaderWorker(QObject *parent) :
    QObject(parent)
{
    FCT_IDENTIFICATION;
}

void AdifRecoveryReaderWorker::readTail(const AdifRecoveryConfig &config,
                                        const AdifRecoveryState &state,
                                        int maxContacts)
{
    FCT_IDENTIFICATION;

    AdifRecoveryScanResult result;

    result.path = config.path;
    result.fileKey = AdifRecovery::fileKey(config.path, config.stationProfileName);
    result.previousOffset = state.offset;
    result.nextOffset = state.offset;

    qCDebug(runtime) << "processing file" << config.path;

    if ( config.path.trimmed().isEmpty() )
    {
        result.message = tr("Startup ADI filename is empty");
        qCDebug(runtime) << "Startup ADI filename is empty";
        emit scanFinished(result);
        return;
    }

    QFileInfo fileInfo(config.path);
    if ( !fileInfo.exists() )
    {
        result.message = tr("Startup ADI file does not exist: %1").arg(config.path);
        qCDebug(runtime) << "Startup ADI file does not exist";
        emit scanFinished(result);
        return;
    }

    result.fileSize = fileInfo.size();

    if ( state.offset < 0 )
    {
        result.nextOffset = result.fileSize;
        result.message = tr("Startup ADI initialized at the end of file");
        qCDebug(runtime) << "Startup ADI initialized at the end of file";
        emit scanFinished(result);
        return;
    }

    if ( state.offset > result.fileSize )
    {
        result.reset = true;
        result.nextOffset = result.fileSize;
        result.message = tr("Startup ADI file was reset; load point moved to the end");
        qCDebug(runtime) << "Startup ADI file was reset; load point moved to the end";
        emit scanFinished(result);
        return;
    }

    if ( state.offset == result.fileSize )
    {
        qCDebug(runtime) << "The same size - nothing to do";
        emit scanFinished(result);
        return;
    }

    QFile file(config.path);
    if ( !file.open(QIODevice::ReadOnly) )
    {
        result.message = tr("Cannot open Startup ADI file: %1").arg(config.path);
        qCDebug(runtime) << "Cannot open Startup ADI file";
        emit scanFinished(result);
        return;
    }

    const qint64 readOffset = state.offset > 0 ? state.offset - 1 : state.offset;

    if ( !file.seek(readOffset) )
    {
        result.message = tr("Cannot seek Startup ADI file: %1").arg(config.path);
        qCDebug(runtime) << "Cannot seek Startup ADI file";
        emit scanFinished(result);
        return;
    }

    const QByteArray tail = file.readAll();
    if ( file.error() != QFile::NoError )
    {
        result.message = tr("Cannot read Startup ADI file: %1").arg(file.errorString());
        qCDebug(runtime) << "Cannot read Startup ADI file";
        emit scanFinished(result);
        return;
    }

    int adifStart = static_cast<int>(state.offset - readOffset);

    if ( adifStart > 0
         && adifStart < tail.size()
         && tail.at(adifStart - 1) == ADIF_TAG_START
         && tail.at(adifStart) != ADIF_TAG_START
         && !isAdifWhitespace(tail.at(adifStart)) )
    {
        adifStart--;
    }

    while ( adifStart < tail.size() && isAdifWhitespace(tail.at(adifStart)) )
        adifStart++;

    if ( adifStart >= tail.size() || tail.at(adifStart) != ADIF_TAG_START )
    {
        qCDebug(runtime) << "Maybe another file"; // TODO what should we do here
        emit scanFinished(result);
        return;
    }

    const QByteArray adifTail = tail.mid(adifStart);
    const qint64 adifTailOffset = readOffset + adifStart;

    const QByteArray lowerTail = adifTail.toLower();
    int searchFrom = 0;
    int lastRecordEnd = -1;

    while ( true )
    {
        const int eorIndex = lowerTail.indexOf(EOR_TAG, searchFrom);
        if ( eorIndex < 0 )
            break;

        result.contactCount++;
        lastRecordEnd = eorIndex + static_cast<int>(qstrlen(EOR_TAG));

        if ( maxContacts > 0 && result.contactCount > maxContacts )
        {
            result.tooMany = true;
            result.nextOffset = result.fileSize;
            result.message = tr("Too many ADIF records for automatic recovery");
            qCDebug(runtime) << "Too many ADIF records for automatic recovery";
            emit scanFinished(result);
            return;
        }

        searchFrom = lastRecordEnd;
    }

    if ( lastRecordEnd > 0 )
    {
        result.adifText = QString::fromLatin1(adifTail.left(lastRecordEnd));
        result.nextOffset = adifTailOffset + lastRecordEnd;
    }

    qCDebug(runtime) << "Finished";
    emit scanFinished(result);
}
