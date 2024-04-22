/*=============================================================================
	IPhoneInput.h: Unreal IPhone user input interface functionality
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __IPHONEINPUT_H__
#define __IPHONEINPUT_H__

#include "EngineUserInterfaceClasses.h"

/**
 * Helper struct for a set of touch locations and type
 */
struct FIPhoneTouchEvent
{
	FIPhoneTouchEvent()
	{
	}
	
	FIPhoneTouchEvent(const FIPhoneTouchEvent& Other)
	{
		Handle = Other.Handle;
		Data = Other.Data;
		Type = Other.Type;
		IPhoneTimestamp = Other.IPhoneTimestamp;
	}
	
	
	FIPhoneTouchEvent(INT InHandle, const FIntPoint& InData, ETouchType InType, DOUBLE InIPhoneTimestamp)
		: Handle(InHandle)
		, Data(InData)
		, Type(InType)
		, IPhoneTimestamp( InIPhoneTimestamp )
	{
	}

	/** Holds the handle of this touch.  It's used to line up touch events */
	INT Handle;

	/** Location of the event (or rotation for tilt pitch/yaw) */
	FIntPoint Data;
	
	/** Type of event */
	ETouchType Type;

	/** Time in seconds since application startup that input event occurred.  Note that this value is not
	    neccessarily in the same space as our application's "real-time seconds", but the deltas between
		two IPhoneTimestamps are still valid. */
	DOUBLE IPhoneTimestamp;
};

/**
 * Manager class to help marshall input from main thread to the UE3 thread
 */
class FIPhoneInputManager
{
public:
	/** Inline allocator to reduce reallocations with arrays of arrays */
	typedef TArray< TArray<FIPhoneTouchEvent>, TInlineAllocator<8> > QueueType;

	/**
	 * Allows main thread to push events into our queue for the game thread to pull from 
	 *
	 * @param Locations Location of all touches in this one event
	 * @param Type Type of the event
	 */ 
	void AddTouchEvents(const TArray<FIPhoneTouchEvent>& Events);

	/**
	 * Safely pull all queued input from the queue
	 */
	void GetAllEvents(QueueType& OutEvents);
	
	/**
	 * returns the current Movement data from the device
	 *
	 * @param Attitude The current Roll/Pitch/Yaw of the device
	 * @param RotationRate The current rate of change of the attitude
	 * @param Gravity A vector that describes the direction of gravity for the device
	 * @param Acceleration returns the current acceleration of the device
	 */
	void GetMovementData(FVector& Attitude, FVector& RotationRate, FVector& Gravity, FVector& Acceleration);

	/**
	 * Call this function to performa any calibration of the motion controls                                                                     
	 */
	void CalibrateMotion();

private:
	
	/** Queue for the game thread to pull from */
	QueueType TouchEventsQueue;
	
	/** Critical section to protect the TouchEventQueue */
	FCriticalSection QueueCriticalSection;
	
	/** TRUE if a calibration is requested */
	UBOOL bIsCalibrationRequested;

	/** Last frames roll/pitch, for calculating rate */
	FLOAT LastRoll;
	FLOAT LastPitch;

	/** The center values for tilt calibration */
	FLOAT CenterRoll;
	FLOAT CenterPitch;

	/** When using just acceleration (without full motion) we store a frame of accel data to filter by */
	FVector FilteredAccelerometer;

};

/** Global singleton */
extern FIPhoneInputManager GIPhoneInputManager;

#endif
