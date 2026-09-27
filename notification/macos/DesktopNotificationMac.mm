#import <UserNotifications/UserNotifications.h>

#include <QMetaObject>

#include "notification/DesktopNotification.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.notification.macos.desktopnotification");

@interface QLogNotificationDelegate : NSObject<UNUserNotificationCenterDelegate>
{
    DesktopNotification *notificationOwner;
}

- (instancetype)initWithOwner:(DesktopNotification *)owner;
- (void)clearOwner;

@end

@implementation QLogNotificationDelegate

- (instancetype)initWithOwner:(DesktopNotification *)owner
{
    FCT_IDENTIFICATION;

    self = [super init];
    if ( self ) notificationOwner = owner;
    return self;
}

- (void)clearOwner
{
    FCT_IDENTIFICATION;

    notificationOwner = nullptr;
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler
{
    FCT_IDENTIFICATION;

    Q_UNUSED(center);
    UNNotificationPresentationOptions options = UNNotificationPresentationOptionAlert;
    if ( notification.request.content.sound )
        options |= UNNotificationPresentationOptionSound;
    completionHandler(options);
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
didReceiveNotificationResponse:(UNNotificationResponse *)response
         withCompletionHandler:(void (^)(void))completionHandler
{
    FCT_IDENTIFICATION;

    Q_UNUSED(center);
    NSNumber *categoryValue =
        response.notification.request.content.userInfo[@"qlog-category"];
    if ( notificationOwner && categoryValue )
        QMetaObject::invokeMethod(
            notificationOwner, "activate", Qt::QueuedConnection,
            Q_ARG(DesktopNotificationCategory,
                  static_cast<DesktopNotificationCategory>(categoryValue.intValue)));
    completionHandler();
}

@end

void DesktopNotification::initializePlatform()
{
    FCT_IDENTIFICATION;

    if ( @available(macOS 10.14, *) )
    {
        QLogNotificationDelegate *notificationDelegate =
            [[QLogNotificationDelegate alloc] initWithOwner:this];
        [UNUserNotificationCenter currentNotificationCenter].delegate = notificationDelegate;
        platformContext = notificationDelegate;
    }
}

void DesktopNotification::shutdownPlatform()
{
    FCT_IDENTIFICATION;

    if ( !platformContext ) return;

    if ( @available(macOS 10.14, *) )
    {
        QLogNotificationDelegate *notificationDelegate =
            reinterpret_cast<QLogNotificationDelegate *>(platformContext);
        [notificationDelegate clearOwner];
        if ( [UNUserNotificationCenter currentNotificationCenter].delegate
             == notificationDelegate )
            [UNUserNotificationCenter currentNotificationCenter].delegate = nil;
#if !__has_feature(objc_arc)
        [notificationDelegate release];
#endif
    }
    platformContext = nullptr;
}

bool DesktopNotification::showPlatform(DesktopNotificationCategory category,
                                       const QString &title,
                                       const QString &body,
                                       bool soundEnabled)
{
    FCT_IDENTIFICATION;

    if ( !platformContext ) return false;

    if ( @available(macOS 10.14, *) )
    {
        NSString *nativeTitle = [NSString
            stringWithCharacters:reinterpret_cast<const unichar *>(title.utf16())
            length:title.length()];
        NSString *nativeBody = [NSString
            stringWithCharacters:reinterpret_cast<const unichar *>(body.utf16())
            length:body.length()];
        UNUserNotificationCenter *center =
            [UNUserNotificationCenter currentNotificationCenter];

        [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert
                                                  | UNAuthorizationOptionSound)
                               completionHandler:^(BOOL granted, NSError *error)
        {
            if ( error )
            {
                qCWarning(runtime) << "Cannot authorize native notifications"
                                   << QString::fromUtf8(error.localizedDescription.UTF8String);
                return;
            }
            if ( !granted ) return;

            UNMutableNotificationContent *content =
                [[UNMutableNotificationContent alloc] init];
            content.title = nativeTitle;
            content.body = nativeBody;
            content.userInfo =
                @{@"qlog-category": @(static_cast<int>(category))};
            if ( soundEnabled )
                content.sound = [UNNotificationSound defaultSound];

            NSString *identifier = [NSUUID UUID].UUIDString;
            UNNotificationRequest *request =
                [UNNotificationRequest requestWithIdentifier:identifier
                                                     content:content
                                                     trigger:nil];
            [center addNotificationRequest:request
                     withCompletionHandler:^(NSError *addError)
            {
                if ( addError )
                    qCWarning(runtime) << "Cannot send native notification"
                                       << QString::fromUtf8(
                                              addError.localizedDescription.UTF8String);
            }];
#if !__has_feature(objc_arc)
            [content release];
#endif
        }];
        return true;
    }

    return false;
}
