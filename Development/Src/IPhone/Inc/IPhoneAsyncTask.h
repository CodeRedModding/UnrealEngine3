/*=============================================================================
	IPhoneAppDelegate.h: IPhone application class / main loop
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#import <Foundation/Foundation.h>

@interface IPhoneAsyncTask : NSObject
{
@private
	/** Whether or not the task is ready to have GameThread callback called (set on iOS thread) */
	INT bIsReadyForGameThread;
}

/** Extra data for this async task */
@property (retain) id UserData;

/** Code to run on the game thread when the async task completes */
@property (copy) UBOOL (^GameThreadCallback)(void);

/**
 * Mark that the task is complete on the iOS thread, and now the
 * GameThread can be fired (the Task is unsafe to use after this call)
 */
- (void)FinishedTask;

/**
 * Tick all currently running tasks
 */
+ (void)TickAsyncTasks;

@end
