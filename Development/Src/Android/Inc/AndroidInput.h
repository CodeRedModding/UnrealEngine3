/*=============================================================================
	AndroidInput.h: Unreal Android user input interface functionality
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ANDROIDINPUT_H__
#define __ANDROIDINPUT_H__

#include "EngineUserInterfaceClasses.h"

/**
 * Helper struct for a set of touch locations and type
 */
struct FAndroidTouchEvent
{
	FAndroidTouchEvent()
	{
	}

	FAndroidTouchEvent(const FAndroidTouchEvent& Other)
	{
		Handle = Other.Handle;
		Data = Other.Data;
		Type = Other.Type;
		AndroidTimestamp = Other.AndroidTimestamp;
	}

	FAndroidTouchEvent(INT InHandle, const FIntPoint& InData, ETouchType InType, DOUBLE InAndroidTimestamp)
		: Handle(InHandle)
		, Data(InData)
		, Type(InType)
		, AndroidTimestamp( InAndroidTimestamp )
	{
	}

	// Holds the handle of this touch.  It's used to line up touch events
	INT Handle;

	// Location of the event (or rotation for tilt pitch/yaw)
	FIntPoint Data;

	// Type of event
	ETouchType Type;

	// Time in seconds since application startup that input event occurred.  Note that this value is not
	// necessarily in the same space as our application's "real-time seconds", but the deltas between
	// two AndroidTimestamps are still valid.
	DOUBLE AndroidTimestamp;
};

struct FAndroidKeyEvent
{
public:
	FAndroidKeyEvent()
	{
	}

	FAndroidKeyEvent(UBOOL InKeyDown, FName InKeyName, INT InUnichar) 
		: KeyDown(InKeyDown)
		, KeyName(InKeyName)
		, Unichar(InUnichar)
	{
	}

	FAndroidKeyEvent(const FAndroidKeyEvent& Other)
	{
		KeyDown = Other.KeyDown;
		KeyName = Other.KeyName;
		Unichar = Other.Unichar;
	}

	FAndroidKeyEvent& operator=(const FAndroidKeyEvent& Other)
	{
		KeyDown = Other.KeyDown;
		KeyName = Other.KeyName;
		Unichar = Other.Unichar;
		return *this;
	}

	UBOOL operator==(const FAndroidKeyEvent& Other) const
	{
		return (
			KeyDown == Other.KeyDown &&
			KeyName == Other.KeyName &&
			Unichar == Other.Unichar
		);
	}

	UBOOL KeyDown;
	FName KeyName;
	INT Unichar;
};

/**
 * Manager class to help marshal input from main thread to the UE3 thread
 */
class FAndroidInputManager
{
private:
	// Queues, and critical sections for protection, for the game thread to pull from
	TArray<FAndroidTouchEvent> TouchEventsQueue;
	FCriticalSection TouchQueueCriticalSection;

	TArray<FAndroidKeyEvent> KeyEventsQueue;
	FCriticalSection KeyQueueCriticalSection;

	// Current tilt values
	FLOAT CurrentYaw;
	FLOAT CurrentPitch;

public:
	/**
	 * Allows main thread to push events into our queue for the game thread to pull from 
	 */ 
	void AddTouchEvent(const FAndroidTouchEvent& Event)
	{
		FScopeLock Lock(&TouchQueueCriticalSection);
		TouchEventsQueue.AddItem(Event);
	}
	void AddKeyEvent(const FAndroidKeyEvent& Event)
	{
		FScopeLock Lock(&KeyQueueCriticalSection);
		KeyEventsQueue.AddItem(Event);
	}
	
	/**
	 * Safely pull all queued input from the queue
	 */
	void GetAllTouchEvents(TArray<FAndroidTouchEvent>& OutTouchEvents)
	{
		FScopeLock Lock(&TouchQueueCriticalSection);
		OutTouchEvents = TouchEventsQueue;
		TouchEventsQueue.Empty();
	}
	void GetAllKeyEvents(TArray<FAndroidKeyEvent>& OutKeyEvents)
	{
		FScopeLock Lock(&KeyQueueCriticalSection);
		OutKeyEvents = KeyEventsQueue;
		KeyEventsQueue.Empty();
	}
	
	/**
	 * Sets the current tilt values from the main thread
	 * 
	 * @param Yaw Current yaw (side to side)
	 * @param Pitch Current pitch (up and down)
	 */
	void SetTilt(FLOAT Yaw, FLOAT Pitch)
	{
		CurrentYaw = Yaw;
		CurrentPitch = Pitch;
	}
	/**
	 * Gets the current tilt values on the game thread
	 *
	 * @param Yaw Current yaw (side to side)
	 * @param Pitch Current pitch (up and down)
	 */
	void GetTilt(FLOAT& Yaw, FLOAT& Pitch)
	{
		Yaw = CurrentYaw;
		Pitch = CurrentPitch;
	}
};

/** Global singleton */
extern FAndroidInputManager GAndroidInputManager;

#endif
