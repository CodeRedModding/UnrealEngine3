/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __FILE_SYSTEM_NOTIFICATION_SHARED_H__
#define __FILE_SYSTEM_NOTIFICATION_SHARED_H__


/** Called from UUnrealEngine::Tick to send file notifications to avoid multiple events per file change */
void TickFileSystemNotifications();

/**
 * Ensures that the file system notifications are active
 * @param InExtension - The extension of file to watch for
 * @param InStart - TRUE if this is a "turn on" event, FALSE if this is a "turn off" event
 */
void SetFileSystemNotification(const FString& InExtension, UBOOL InStart);

/**
 * Helper function to enable and disable file listening used by the editor frame
 */
void SetFileSystemNotificationsForEditor(const UBOOL bTextureListenOnOff, const UBOOL bApexListenOnOff);

/**
 * Helper function to enable and disable file listening used by the anim set
 */
void SetFileSystemNotificationsForAnimSet(const UBOOL bAnimSetListenOnOff);

/**
 * Shuts down global pointers before shut down
 */
void CloseFileSystemNotification();

#endif // __FILE_SYSTEM_NOTIFICATION_SHARED_H__

