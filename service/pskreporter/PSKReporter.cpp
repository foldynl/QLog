#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
#include <QTimeZone>
#endif
#include "PSKReporter.h"
#include "core/MqttClient.h"
#include "core/debug.h"
#include "rig/macros.h"
#include "data/BandPlan.h"
#include "data/Data.h"

MODULE_IDENTIFICATION("qlog.service.pskreporter.pskreporter");

PSKReporter::PSKReporter(QObject *parent)
    : QObject(parent),
      mqttClient(new MqttClient(QStringLiteral("mqtt.pskreporter.info"),
                                1884,
                                this))
{
    FCT_IDENTIFICATION;

    connect(mqttClient, &MqttClient::messageReceived,
            this, &PSKReporter::mqttMessageReceived);
}

void PSKReporter::setSubscription(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    const QString newCallsign = callsign.trimmed().toUpper();

    if ( subscriptionCallsign == newCallsign )
        return;

    subscriptionCallsign = newCallsign;
    QStringList topics;

    if ( !subscriptionCallsign.isEmpty() )
    {
        topics << QStringLiteral("pskr/filter/v2/+/+/%1/+/#").arg(subscriptionCallsign);
        // Uncomment together with the test operation in processMessage() to
        // receive stations heard by this callsign.
        // topics << QStringLiteral("pskr/filter/v2/+/+/+/%1/#").arg(subscriptionCallsign);
    }

    mqttClient->setSubscriptions(topics);
    emit subscriptionChanged();
}

void PSKReporter::mqttMessageReceived(const QString &topic,
                                      const QByteArray &payload)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << topic << payload.size();

    if ( subscriptionCallsign.isEmpty() )
        return;

    const QStringList topicParts = topic.split(QLatin1Char('/'));
    if ( topicParts.size() < 7 )
    {
        qCDebug(runtime) << "Ignoring unexpected PSK Reporter MQTT topic" << topic;
        return;
    }

    if ( topicParts.at(5).compare(subscriptionCallsign, Qt::CaseInsensitive) == 0 )
    {
        processMessage(Direction::SentBy, topic, payload);
    }
    else if ( topicParts.at(6).compare(subscriptionCallsign, Qt::CaseInsensitive) == 0 )
    {
        processMessage(Direction::ReceivedBy, topic, payload);
    }
    else
    {
        qCDebug(runtime) << "Ignoring PSK Reporter MQTT topic for another callsign" << topic;
    }
}

void PSKReporter::processMessage(PSKReporter::Direction direction,
                                 const QString &topic,
                                 const QByteArray &payload)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << ((direction == Direction::ReceivedBy) ? "ReceivedBy" : "SentBy")
                                 << topic
                                 << payload;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if ( parseError.error != QJsonParseError::NoError
         || !document.isObject() )
    {
        qCWarning(runtime) << "Invalid PSK Reporter message:" << parseError.errorString();
        return;
    }

    const QJsonObject message = document.object();
    PskDecode decode;
    decode.sequenceNumber = message.value(QStringLiteral("sq")).toVariant().toULongLong();
    decode.frequency = Hz2MHz(message.value(QStringLiteral("f")).toVariant().toULongLong());
    decode.mode = message.value(QStringLiteral("md")).toString();
    decode.report = message.value(QStringLiteral("rp")).toInt();    
    decode.timestamp = QDateTime::fromSecsSinceEpoch(message.value(QStringLiteral("t")).toVariant().toLongLong(),
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
                                                     QTimeZone::UTC);
#else
                                                     Qt::UTC);
#endif

    decode.transmitterTimestamp = QDateTime::fromSecsSinceEpoch(message.value(QStringLiteral("t_tx")).toVariant().toLongLong(),
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
                                                                QTimeZone::UTC);
#else
                                                                Qt::UTC);
#endif

    decode.senderCallsign = message.value(QStringLiteral("sc")).toString();
    decode.senderLocator = message.value(QStringLiteral("sl")).toString();
    decode.receiverCallsign = message.value(QStringLiteral("rc")).toString();
    decode.receiverLocator = message.value(QStringLiteral("rl")).toString();
    decode.senderDxcc = message.value(QStringLiteral("sa")).toInt();
    decode.receiverDxcc = message.value(QStringLiteral("ra")).toInt();
    decode.band = message.value(QStringLiteral("b")).toString();

#if 1 // Normal operation: show stations which heard this callsign.
    if ( direction != Direction::SentBy )
        return;
#else // Test operation: show stations heard by this callsign.
    if ( direction != Direction::ReceivedBy )
        return;
#endif

    const Band bandDecoded = BandPlan::freq2Band(decode.frequency);
    const bool sentBy = (direction == Direction::SentBy);
    const QString &remoteCallsign = sentBy ? decode.receiverCallsign
                                           : decode.senderCallsign;
    const qint32 remoteDxcc = sentBy ? decode.receiverDxcc
                                     : decode.senderDxcc;

    if ( bandDecoded.name.isEmpty()
         || remoteCallsign.isEmpty() )
        return;

    decode.dupeCount = Data::countDupe(remoteCallsign,
                                       bandDecoded.name,
                                       BandPlan::MODE_GROUP_STRING_DIGITAL);
    decode.status = Data::instance()->currentDxccStatus(remoteDxcc,
                                                        bandDecoded.name,
                                                        BandPlan::MODE_GROUP_STRING_DIGITAL);

    qCDebug(runtime) << decode;
    emit heardMePointRequested(decode, direction);
}
