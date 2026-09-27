#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QHash>
#include <QUuid>

#include "notification/DesktopNotification.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.notification.linux.desktopnotification");

class LinuxNotificationContext : public QObject
{
    Q_OBJECT

public:
    explicit LinuxNotificationContext(DesktopNotification *owner);

    DesktopNotification *owner;
    QDBusInterface *interface;
    QHash<uint, DesktopNotificationCategory> notificationCategories;

public slots:
    void actionInvoked(uint id, const QString &actionKey);
    void notificationClosed(uint id, uint reason);
};

LinuxNotificationContext::LinuxNotificationContext(DesktopNotification *notificationOwner) :
    QObject(notificationOwner),
    owner(notificationOwner),
    interface(nullptr)
{
    FCT_IDENTIFICATION;
}

void LinuxNotificationContext::actionInvoked(uint id, const QString &actionKey)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << id << actionKey;

    const auto category = notificationCategories.constFind(id);
    if ( category != notificationCategories.constEnd() )
        owner->activate(category.value());
}

void LinuxNotificationContext::notificationClosed(uint id, uint reason)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << id << reason;

    notificationCategories.remove(id);
}

void DesktopNotification::initializePlatform()
{
    FCT_IDENTIFICATION;

    LinuxNotificationContext *context = new LinuxNotificationContext(this);
    platformContext = context;

#ifdef QLOG_FLATPAK
    context->interface = new QDBusInterface(QStringLiteral("org.freedesktop.portal.Desktop"),
                                            QStringLiteral("/org/freedesktop/portal/desktop"),
                                            QStringLiteral("org.freedesktop.portal.Notification"),
                                            QDBusConnection::sessionBus(),
                                            context);
#else
    context->interface = new QDBusInterface(QStringLiteral("org.freedesktop.Notifications"),
                                            QStringLiteral("/org/freedesktop/Notifications"),
                                            QStringLiteral("org.freedesktop.Notifications"),
                                            QDBusConnection::sessionBus(),
                                            context);

    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.Notifications"),
                                          QStringLiteral("/org/freedesktop/Notifications"),
                                          QStringLiteral("org.freedesktop.Notifications"),
                                          QStringLiteral("ActionInvoked"),
                                          context,
                                          SLOT(actionInvoked(uint,QString)));

    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.Notifications"),
                                          QStringLiteral("/org/freedesktop/Notifications"),
                                          QStringLiteral("org.freedesktop.Notifications"),
                                          QStringLiteral("NotificationClosed"),
                                          context,
                                          SLOT(notificationClosed(uint,uint)));
#endif
}

void DesktopNotification::shutdownPlatform()
{
    FCT_IDENTIFICATION;

    platformContext = nullptr;
}

bool DesktopNotification::showPlatform(DesktopNotificationCategory category,
                                       const QString &title,
                                       const QString &body,
                                       bool soundEnabled)
{
    FCT_IDENTIFICATION;

    LinuxNotificationContext *context = static_cast<LinuxNotificationContext *>(platformContext);
    if ( !context || !context->interface || !context->interface->isValid() )
    {
        qCWarning(runtime) << "Native notification service is not available";
        return false;
    }

#ifdef QLOG_FLATPAK
    QVariantMap notification;

    notification.insert(QStringLiteral("title"), title);
    notification.insert(QStringLiteral("body"), body);
    notification.insert(QStringLiteral("sound"), soundEnabled
                         ? QStringLiteral("default")
                         : QStringLiteral("silent"));
    notification.insert(QStringLiteral("priority"), QStringLiteral("normal"));
    notification.insert(QStringLiteral("icon"), QStringLiteral("io.github.foldynl.QLog"));
    notification.insert(QStringLiteral("category"), categoryIdentifier(category));

    const QString id = QStringLiteral("qlog-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    context->interface->call(QDBus::NoBlock, QStringLiteral("AddNotification"), id, notification);
#else
    QVariantMap hints;
    hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("qlog"));
    hints.insert(QStringLiteral("category"), categoryIdentifier(category));
    if ( soundEnabled )
        hints.insert(QStringLiteral("sound-name"), QStringLiteral("message-new-instant"));
    else
        hints.insert(QStringLiteral("suppress-sound"), true);

    const QStringList actions =
    {
        QStringLiteral("default"), tr("Open QLog")
    };

    QDBusPendingCall pendingCall = context->interface->asyncCall(QStringLiteral("Notify"),
                                                                 QStringLiteral("QLog"),
                                                                 uint(0),
                                                                 QStringLiteral("qlog"),
                                                                 title,
                                                                 body,
                                                                 actions,
                                                                 hints,
                                                                 -1);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pendingCall, context);

    connect(watcher, &QDBusPendingCallWatcher::finished, context,
            [context, category](QDBusPendingCallWatcher *finished)
    {
        QDBusPendingReply<uint> reply = *finished;
        if ( reply.isError() )
            qCWarning(runtime) << "Cannot send native notification" << reply.error();
        else
            context->notificationCategories.insert(reply.value(), category);
        finished->deleteLater();
    });
#endif

    return true;
}

#include "DesktopNotificationLinux.moc"
