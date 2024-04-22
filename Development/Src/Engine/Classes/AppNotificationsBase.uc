/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is the base class for per-platform support for handling application notifications
 */
 
class AppNotificationsBase extends PlatformInterfaceBase
	native(PlatformInterface);

/** Key/Value pairing for passing custom message strings to notifications */
struct native NotificationMessageInfo
{
	/** Custom message info key identifier */
	var string Key;
	/** Custom message info value */
	var string Value;
};

/** All the info relevant to a notification */
struct native NotificationInfo
{
	/** True if the notification was scheduled locally */
	var bool bIsLocal;
	/** Message to show for the body of the notification */
	var string MessageBody;
	/** Badge # to show next to the app icon */
	var int BadgeNumber;
	/** Custom messages to pass with the notification (key/value pairs) */
	var array<NotificationMessageInfo> MessageInfo;
};

/** All the info relevant to a notification */
struct native LaunchNotificationInfo
{
	/** True if the app was launched because the user accepted a notification */
	var bool bWasLaunchedViaNotification;
	/** Custom messages to pass with the notification (key/value pairs) */
	var NotificationInfo Notification;
};

/** Info filled in if the app was launched via a notification. */
var const LaunchNotificationInfo AppLaunchNotification;

/**
 * Perform any initialization. Called once after singleton instantiation
 */
native event Init();

/**
 * @return True if the app was launched via a notification
 */
function bool WasLaunchedViaNotification()
{
	return AppLaunchNotification.bWasLaunchedViaNotification;
}

/**
 * Schedule a local notification to occur for the current app on the device
 *
 * @param Notification info needed to define the local notification
 * @param StartOffsetSeconds seconds to elapse before the notification is fired. 0 triggers immediately
 */
native function ScheduleLocalNotification(const out NotificationInfo Notification, int StartOffsetSeconds);

/**
 * Cancels all pending local notifications that have been scheduled previously
 */
native function CancelAllScheduledLocalNotifications();

/**
 * delegate triggered when the app processes a local notification
 *
 * @param Notification info from the local notification
 * @param bWasAppInactive TRUE if the app was in the foreground before the notification was processed
 */
delegate OnReceivedLocalNotification(const out NotificationInfo Notification, bool bWasAppActive);

/**
 * delegate triggered when the app processes a remote notification
 *
 * @param Notification info from the remote notification
 * @param bWasAppInactive TRUE if the app was in the foreground before the notification was processed
 */
delegate OnReceivedRemoteNotification(const out NotificationInfo Notification, bool bWasAppActive);

/** Debug loggin of a notification entry */
function DebugLogNotification(const out NotificationInfo Notification)
{
	local int Idx;

	`log("Notification:"
		@"bIsLocal="$Notification.bIsLocal
		@"BadgeNumber="$Notification.BadgeNumber
		@"MessageBody="$Notification.MessageBody);

	for (Idx=0; Idx < Notification.MessageInfo.Length; Idx++)
	{
		`log("Notification:"
			@"key="$Notification.MessageInfo[Idx].Key
			@"val="$Notification.MessageInfo[Idx].Value);
	}
}