#include <QSqlError>
#include <QSqlQuery>

#include "AlertNotification.h"
#include "core/AlertEvaluator.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.notification.alertnotification");

AlertNotification::AlertNotification(QObject *parent) :
    QObject(parent),
    enabled(false),
    nextAlertCooldownCleanup(0)
{
    FCT_IDENTIFICATION;

    alertCooldownClock.start();
    loadAlertRules();
}

void AlertNotification::setEnabled(bool newEnabled)
{
    FCT_IDENTIFICATION;

    enabled = newEnabled;
}

void AlertNotification::loadAlertRules()
{
    FCT_IDENTIFICATION;

    QSqlQuery query;
    if ( !query.prepare("SELECT rule_name, notification_sound_enabled, "
                        "notification_repeat_minutes "
                        "FROM alert_rules "
                        "WHERE notification_enabled <> 0") )
    {
        qCWarning(runtime) << "Cannot prepare Alert Notification rules query"
                           << query.lastError();
        return;
    }

    if ( !query.exec() )
    {
        qCWarning(runtime) << "Cannot load Alert Notification rules"
                           << query.lastError();
        return;
    }

    QHash<QString, RuleSettings> loadedRuleSettings;
    while ( query.next() )
    {
        RuleSettings settings;
        settings.soundEnabled = query.value(1).toBool();
        settings.repeatMinutes = qBound<int>(AlertRule::MIN_NOTIFICATION_REPEAT_MINUTES,
                                             query.value(2).toInt(),
                                             AlertRule::MAX_NOTIFICATION_REPEAT_MINUTES);
        loadedRuleSettings.insert(query.value(0).toString(), settings);
    }

    if ( query.lastError().isValid() )
    {
        qCWarning(runtime) << "Cannot read Alert Notification rules"
                           << query.lastError();
        return;
    }

    alertRuleSettings.swap(loadedRuleSettings);
    nextAlertNotificationTimes.clear();
}

void AlertNotification::processSpotAlert(const SpotAlert &alert)
{
    FCT_IDENTIFICATION;

    if ( !enabled ) return;

    const qint64 now = alertCooldownClock.elapsed();
    if ( now >= nextAlertCooldownCleanup )
    {
        auto cooldown = nextAlertNotificationTimes.begin();
        while ( cooldown != nextAlertNotificationTimes.end() )
        {
            if ( cooldown.value() <= now )
                cooldown = nextAlertNotificationTimes.erase(cooldown);
            else
                ++cooldown;
        }
        nextAlertCooldownCleanup = now + 60 * 1000;
    }

    QStringList dueRules;
    bool soundEnabled = false;

    for ( const QString &ruleName : alert.ruleNameList )
    {
        const auto settings = alertRuleSettings.constFind(ruleName);
        if ( settings == alertRuleSettings.constEnd() ) continue;

        const QString key = alertCooldownKey(ruleName, alert);
        const auto nextNotification = nextAlertNotificationTimes.constFind(key);

        if ( nextNotification != nextAlertNotificationTimes.constEnd()
             && nextNotification.value() > now )
            continue;

        dueRules << ruleName;
        soundEnabled = soundEnabled || settings->soundEnabled;
        nextAlertNotificationTimes.insert(
            key, now + static_cast<qint64>(settings->repeatMinutes) * 60 * 1000);
    }

    if ( dueRules.isEmpty() ) return;

    emit notificationRequested(DesktopNotificationCategory::ALERT,
                               tr("%1 — Alert: %2").arg(alert.spot.callsign,
                                                       dueRules.join(", ")),
                               alertNotificationBody(alert),
                               soundEnabled);
}

void AlertNotification::clearAlertHistory()
{
    FCT_IDENTIFICATION;

    nextAlertNotificationTimes.clear();
    nextAlertCooldownCleanup = 0;
}

QString AlertNotification::alertCooldownKey(const QString &ruleName,
                                             const SpotAlert &alert) const
{
    FCT_IDENTIFICATION;

    const QChar separator(0x001f);
    return ruleName + separator
           + alert.spot.callsign.trimmed().toUpper() + separator
           + alert.spot.band.trimmed().toLower() + separator
           + alert.spot.modeGroupString.trimmed().toUpper();
}

QString AlertNotification::alertNotificationBody(const SpotAlert &alert) const
{
    FCT_IDENTIFICATION;

    QStringList details;
    if ( !alert.spot.dxcc.country.isEmpty() ) details << alert.spot.dxcc.country;
    if ( !alert.spot.band.isEmpty() ) details << alert.spot.band;
    if ( !alert.spot.modeGroupString.isEmpty() ) details << alert.spot.modeGroupString;
    if ( alert.spot.freq > 0.0 )
        details << tr("%1 MHz").arg(QSTRING_FREQ(alert.spot.freq));

    return details.join(QStringLiteral(" · "));
}
