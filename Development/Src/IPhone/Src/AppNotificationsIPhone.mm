/*=============================================================================
	AppNotificationsIPhone.mm: IPhone specific support for handling app notifications
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "EnginePlatformInterfaceClasses.h"

#import "IPhoneObjCWrapper.h"
#import "NSStringExtensions.h"
#import <UIKit/UIKit.h>
#include "AppNotificationsIPhone.h"

IMPLEMENT_CLASS(UAppNotificationsIPhone);
void AutoInitializeRegistrantsAppNotificationsIPhone( INT& Lookup )
{
	UAppNotificationsIPhone::StaticClass();
}

/**
 * Helper to get the message body string from the notification dictionary
 */
static NSString* GetApsNotificationMessageBodyStr(NSDictionary* notificationDict)
{
	NSString* Result = nil;
	NSDictionary* apsDict = [notificationDict objectForKey:@"aps"];
	if (apsDict != nil)
	{
		id Alert = [apsDict objectForKey:@"alert"];
		if ([Alert isKindOfClass:[NSDictionary class]])
		{
			Result = [Alert objectForKey:@"body"];
		}
		else
		{
			Result = Alert;
		}
	}
	return Result;
}
/**
 * Helper to get the badge number string from the notification dictionary
 */
static NSString* GetApsNotificationBadgeStr(NSDictionary* notificationDict)
{
	NSString* Result = nil;
	NSDictionary* apsDict = [notificationDict objectForKey:@"aps"];
	if (apsDict != nil)
	{
		Result = [apsDict objectForKey:@"badge"];
	}
	return Result;
}

/**
 * Helper to fill in notification data from the local notification object
 */
static void CaptureLocalNotificationInfo(FNotificationInfo& NotificationResult, UILocalNotification* localNotification)
{
	if (localNotification != nil)
	{
		if (localNotification.userInfo != nil)
		{
			// Add key/value strings from user payload of notification
			for (NSString* Key in [localNotification.userInfo allKeys])
			{
				NSString* Value = [localNotification.userInfo objectForKey:Key];
				if (Value != nil)
				{
					FNotificationMessageInfo MessageInfo(EC_EventParm);
					MessageInfo.Key = FString(Key);
					MessageInfo.Value = FString(Value);
					NotificationResult.MessageInfo.AddItem(MessageInfo);
				}
			}
		}
		// Badge number requested by the notification
		NotificationResult.BadgeNumber = localNotification.applicationIconBadgeNumber;
		// Message body
		if (localNotification.alertBody != nil)
		{
			NotificationResult.MessageBody = FString(localNotification.alertBody);
		}
	}
}

/**
 * Helper to fill in notification data from the remote notification object
 */
static void CaptureRemoteNotificationInfo(FNotificationInfo& NotificationResult, NSDictionary* notificationDict)
{
	if (notificationDict != nil)
	{
		// Add key/value strings from user payload of notification
		for (NSString* Key in [notificationDict allKeys])
		{
			id Value = [notificationDict objectForKey:Key];
			if ([Value isKindOfClass:[NSString class]])
			{
				FNotificationMessageInfo MessageInfo(EC_EventParm);
				MessageInfo.Key = FString(Key);
				MessageInfo.Value = FString((NSString*)Value);
				NotificationResult.MessageInfo.AddItem(MessageInfo);
			}
		}
		// Badge number requested by the notification
		NSString* badgeStr = GetApsNotificationBadgeStr(notificationDict);
		if (badgeStr != nil)
		{
			NotificationResult.BadgeNumber = [badgeStr integerValue];
		}
		// Message body
		NSString* msgBodyStr = GetApsNotificationMessageBodyStr(notificationDict);
		if (msgBodyStr != nil)
		{
			NotificationResult.MessageBody = FString(msgBodyStr);
		}
	}
}

/**
 * Perform any initialization. Called once after singleton instantiation
 */
void UAppNotificationsIPhone::Init()
{
	extern NSDictionary* GLaunchOptionsDict;
	
	// default to not launched via notification
	AppLaunchNotification.bWasLaunchedViaNotification = FALSE;
	// launch options dictionary only valid if launched via notification
	if (GLaunchOptionsDict != nil)
	{
		// local notification entry should be valid if launched via user accepting a local notification
		UILocalNotification* localNotif = [GLaunchOptionsDict objectForKey:UIApplicationLaunchOptionsLocalNotificationKey];
		// remote notification dict entry should be valid if launched via user accepting a remote notification
		NSDictionary* remoteNotifDict = [GLaunchOptionsDict objectForKey:UIApplicationLaunchOptionsRemoteNotificationKey];
		if (localNotif != nil)
		{
			// processing a local notification
			AppLaunchNotification.Notification.bIsLocal = TRUE;
			// copy to Notification data
			CaptureLocalNotificationInfo(AppLaunchNotification.Notification,localNotif);
			// mark as launched via notification
			AppLaunchNotification.bWasLaunchedViaNotification = TRUE;
		}
		else if (remoteNotifDict != nil)
		{
			// processing a remote notification
			AppLaunchNotification.Notification.bIsLocal = FALSE;
			// copy to Notification data
			CaptureRemoteNotificationInfo(AppLaunchNotification.Notification,remoteNotifDict);
			// mark as launched via notification
			AppLaunchNotification.bWasLaunchedViaNotification = TRUE;		
		}
	}
}

/**
 * Schedule a local notification to occur for the current app on the device
 *
 * @param Notification info needed to define the local notification
 * @param StartOffsetSeconds seconds to elapse before the notification is fired. 0 triggers immediately
 */
void UAppNotificationsIPhone::ScheduleLocalNotification(const struct FNotificationInfo& Notification,INT StartOffsetSeconds)
{
	UILocalNotification *localNotification = [[UILocalNotification alloc] init];
	if (localNotification != nil)
	{
		// Set the initial time for firing the notification
		if (StartOffsetSeconds > 0)
		{
			localNotification.fireDate = [NSDate dateWithTimeIntervalSinceNow:StartOffsetSeconds];
			localNotification.timeZone = [NSTimeZone defaultTimeZone];
		}
		// Body of the message to display to user
		localNotification.alertBody = [NSString stringWithFString:Notification.MessageBody];		
		// Badge icon to update 
		localNotification.applicationIconBadgeNumber = Notification.BadgeNumber;
		// Fill in custom message info
		if (Notification.MessageInfo.Num() > 0)
		{
			NSDictionary* infoDict = [NSMutableDictionary dictionaryWithCapacity:Notification.MessageInfo.Num()];
			for (INT Idx=0; Idx < Notification.MessageInfo.Num(); Idx++)
			{
				const FNotificationMessageInfo& MessageInfo = Notification.MessageInfo(Idx);
				[infoDict setValue:[NSString stringWithFString:MessageInfo.Value] forKey:[NSString stringWithFString:MessageInfo.Key]];				
			}
			localNotification.userInfo = infoDict;
		}
		// Schedule it
		[[UIApplication sharedApplication] scheduleLocalNotification:localNotification];
		[localNotification release];
	}
}

/**
 * Cancels all pending local notifications that have been scheduled previously
 */
void UAppNotificationsIPhone::CancelAllScheduledLocalNotifications()
{
	[[UIApplication sharedApplication] cancelAllLocalNotifications];
}

/**
 * Process a local notification
 *
 * @param localNotification contains the message, badge, and user payload for the incoming notification
 */
void UAppNotificationsIPhone::ProcessLocalNotification(UILocalNotification* localNotification,UBOOL bWasAppActive)
{
	FNotificationInfo NotificationResult(EC_EventParm);	
	// copy to Notification data
	CaptureLocalNotificationInfo(NotificationResult,localNotification);
	// Release ref to the notification
	[localNotification release];
	// Processing a local notification
	NotificationResult.bIsLocal = TRUE;
	// Trigger delegate
	delegateOnReceivedLocalNotification(NotificationResult, bWasAppActive);
}

/**
 * Process a remote notification
 *
 * @param notificationDict contains the message, badge, and user payload for the incoming notification
 */
void UAppNotificationsIPhone::ProcessRemoteNotification(NSDictionary* notificationDict,UBOOL bWasAppActive)
{
	FNotificationInfo NotificationResult(EC_EventParm);	
	// copy to Notification data
	CaptureRemoteNotificationInfo(NotificationResult,notificationDict);
	// Release ref to the dict
	[notificationDict release];
	// Processing a remote notification
	NotificationResult.bIsLocal = FALSE;
	// Trigger delegate
	delegateOnReceivedRemoteNotification(NotificationResult, bWasAppActive);
}