#ifndef QLOG_NOTIFICATION_DESKTOPNOTIFICATION_H
#define QLOG_NOTIFICATION_DESKTOPNOTIFICATION_H

#include <QObject>
#include <QString>

#include "notification/DesktopNotificationCategory.h"

class DesktopNotification : public QObject
{
    Q_OBJECT

public:
    static DesktopNotification *instance();

public slots:
    bool show(DesktopNotificationCategory category,
              const QString &title,
              const QString &body,
              bool soundEnabled = true);
    void activate(DesktopNotificationCategory category);

signals:
    void activated(DesktopNotificationCategory category);

private:
    explicit DesktopNotification(QObject *parent);
    ~DesktopNotification();

    static QString categoryIdentifier(DesktopNotificationCategory category);

    void initializePlatform();
    void shutdownPlatform();
    bool showPlatform(DesktopNotificationCategory category,
                      const QString &title,
                      const QString &body,
                      bool soundEnabled);

    void *platformContext;
};

#endif // QLOG_NOTIFICATION_DESKTOPNOTIFICATION_H
