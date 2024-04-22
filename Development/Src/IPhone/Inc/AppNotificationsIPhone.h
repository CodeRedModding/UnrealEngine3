/*=============================================================================
	AppNotificationsIPhone.h: IPhone specific handling of iOS notifications
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _APPNOTIFICATIONSIPHONE_H_
#define _APPNOTIFICATIONSIPHONE_H_

/**
 * IPhone implementation for app notifications
 */
class UAppNotificationsIPhone : public UAppNotificationsBase
{
	DECLARE_CLASS_INTRINSIC(UAppNotificationsIPhone, UAppNotificationsBase, 0, IPhoneDrv)

	// UAnalyticEventsBase interface
	virtual void Init();
	virtual void ScheduleLocalNotification(const struct FNotificationInfo& Notification,INT StartOffsetSeconds);
    virtual void CancelAllScheduledLocalNotifications();

	/**
	 * Process a local notification
	 *
	 * @param localNotification contains the message, badge, and user payload for the incoming notification
	 */
	void ProcessLocalNotification(UILocalNotification* localNotification,UBOOL bWasAppActive);
	/**
	 * Process a remote notification
	 *
	 * @param notificationDict contains the message, badge, and user payload for the incoming notification
	 */
	void ProcessRemoteNotification(NSDictionary* notificationDict,UBOOL bWasAppActive);
};

#endif //_APPNOTIFICATIONSIPHONE_H_

