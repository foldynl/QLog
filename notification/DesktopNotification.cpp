#include <QCoreApplication>
#include <QThread>

#include "DesktopNotification.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.notification.desktopnotification");

DesktopNotification::DesktopNotification(QObject *parent) :
    QObject(parent),
    platformContext(nullptr)
{
    FCT_IDENTIFICATION;

    qRegisterMetaType<DesktopNotificationCategory>("DesktopNotificationCategory");
    initializePlatform();
}

DesktopNotification::~DesktopNotification()
{
    FCT_IDENTIFICATION;

    shutdownPlatform();
}

DesktopNotification *DesktopNotification::instance()
{
    FCT_IDENTIFICATION;

    QCoreApplication *const application = QCoreApplication::instance();
    Q_ASSERT(application);
    Q_ASSERT(QThread::currentThread() == application->thread());

    // Let QCoreApplication own and delete this singleton. It must be destroyed
    // while Qt is still available because shutdownPlatform() uses Qt and native
    // notification APIs. A function-local static object would be destroyed too
    // late, after QCoreApplication.
    static DesktopNotification *const notification = new DesktopNotification(application);
    return notification;
}

bool DesktopNotification::show(DesktopNotificationCategory category,
                               const QString &title,
                               const QString &body,
                               bool soundEnabled)
{
    FCT_IDENTIFICATION;

    if ( title.isEmpty() ) return false;

    return showPlatform(category, title, body, soundEnabled);
}

void DesktopNotification::activate(DesktopNotificationCategory category)
{
    FCT_IDENTIFICATION;

    emit activated(category);
}

QString DesktopNotification::categoryIdentifier(DesktopNotificationCategory category)
{
    FCT_IDENTIFICATION;

    switch ( category )
    {
        case DesktopNotificationCategory::ALERT:
            return QStringLiteral("x-qlog.alert");
    }

    return QString();
}
