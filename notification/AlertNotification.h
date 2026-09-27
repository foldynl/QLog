#ifndef QLOG_NOTIFICATION_ALERTNOTIFICATION_H
#define QLOG_NOTIFICATION_ALERTNOTIFICATION_H

#include <QElapsedTimer>
#include <QHash>
#include <QObject>

#include "data/SpotAlert.h"
#include "notification/DesktopNotificationCategory.h"

class AlertNotification : public QObject
{
    Q_OBJECT

public:
    explicit AlertNotification(QObject *parent = nullptr);

    void setEnabled(bool enabled);

public slots:
    void loadAlertRules();
    void processSpotAlert(const SpotAlert &alert);
    void clearAlertHistory();

signals:
    void notificationRequested(DesktopNotificationCategory category,
                               const QString &title,
                               const QString &body,
                               bool soundEnabled);

private:
    struct RuleSettings
    {
        bool soundEnabled;
        int repeatMinutes;
    };

    QString alertCooldownKey(const QString &ruleName, const SpotAlert &alert) const;
    QString alertNotificationBody(const SpotAlert &alert) const;

    bool enabled;
    QHash<QString, RuleSettings> alertRuleSettings;
    QHash<QString, qint64> nextAlertNotificationTimes;
    QElapsedTimer alertCooldownClock;
    qint64 nextAlertCooldownCleanup;
};

#endif // QLOG_NOTIFICATION_ALERTNOTIFICATION_H
