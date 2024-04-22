#import <UIKit/UIKit.h>
#import <CoreMotion/CoreMotion.h>

#include "Engine.h"
#include "IPhoneDrv.h"
#include "IPhoneInput.h"

/** Variables that can't be defined in IPhoneInput.h since they are Obj-C classes */
static CMMotionManager* MotionManager = nil;
static CMAttitude* ReferenceAttitude = nil;

/**
 * Allows main thread to push events into our queue for the game thread to pull from 
 *
 * @param Locations Location of all touches in this one event
 * @param Type Type of the event
 */ 
void FIPhoneInputManager::AddTouchEvents(const TArray<FIPhoneTouchEvent>& Events)
{
	// wait on critical section
	FScopeLock Lock(&QueueCriticalSection);
	
	// add the entry
	TouchEventsQueue.AddItem(Events);
}

/**
 * Safely pull all queued input from the queue
 */
void FIPhoneInputManager::GetAllEvents(QueueType& OutEvents)
{
	// wait on critical section
	FScopeLock Lock(&QueueCriticalSection);

	// copy the queue
	OutEvents = TouchEventsQueue;
	
	// clear out the events
	TouchEventsQueue.Empty();
}

/**
 * returns the current Movement data from the device
 *
 * @param Attitude The current Roll/Pitch/Yaw of the device
 * @param RotationRate The current rate of change of the attitude
 * @param Gravity A vector that describes the direction of gravity for the device
 * @param Acceleration returns the current acceleration of the device
 */
void FIPhoneInputManager::GetMovementData(FVector& Attitude, FVector& RotationRate, FVector& Gravity, FVector& Acceleration)
{
	/** Define a static reference to the motion manager */

	static FLOAT LastPitch;
	static FLOAT LastRoll;

	FLOAT Pitch;
	FLOAT Roll;

	// initialize on first use
	if (MotionManager == nil)
	{
		// Look to see if we can create the motion manager
		MotionManager = [[CMMotionManager alloc] init];

		// Check to see if the device supports full motion (gyro + accelerometer)
		if (MotionManager.deviceMotionAvailable)
		{
			MotionManager.deviceMotionUpdateInterval = 0.02;
			// Start the Device updating motion
			[MotionManager startDeviceMotionUpdates];
		}
		else
		{
			[MotionManager startAccelerometerUpdates];
			CenterPitch = CenterPitch = 0;
			bIsCalibrationRequested = FALSE;
		}
	}

	// do we have full motion data?
	if (MotionManager.deviceMotionActive)
	{
		// Grab the values
		CMAttitude* CurrentAttitude = MotionManager.deviceMotion.attitude;
		CMRotationRate CurrentRotationRate = MotionManager.deviceMotion.rotationRate;
		CMAcceleration CurrentGravity = MotionManager.deviceMotion.gravity;
		CMAcceleration CurrentUserAcceleration = MotionManager.deviceMotion.userAcceleration;

		// apply a reference attitude if we have been calibrated away from default
		if (ReferenceAttitude)
		{
			[CurrentAttitude multiplyByInverseOfAttitude: ReferenceAttitude];
		}

		// convert to UE3
		Attitude = FVector( FLOAT(CurrentAttitude.pitch), FLOAT(CurrentAttitude.yaw), FLOAT(CurrentAttitude.roll));
		RotationRate = FVector( FLOAT(CurrentRotationRate.x), FLOAT(CurrentRotationRate.y), FLOAT(CurrentRotationRate.z));
		Gravity = FVector( FLOAT(CurrentGravity.x), FLOAT(CurrentGravity.y), FLOAT(CurrentGravity.z));
		Acceleration = FVector( FLOAT(CurrentUserAcceleration.x), FLOAT(CurrentUserAcceleration.y), FLOAT(CurrentUserAcceleration.z));
	}
	else
	{
		// get the plain accleration
		CMAcceleration RawAcceleration = [MotionManager accelerometerData].acceleration;
		FVector NewAcceleration(RawAcceleration.x, RawAcceleration.y, RawAcceleration.z);

		// storage for keeping the accelerometer values over time (for filtering)
		static UBOOL bFirstAccel = TRUE;

		// how much of the previous frame's acceleration to keep
		const FLOAT VectorFilter = bFirstAccel ? 0.0f : 0.85f;
		bFirstAccel = FALSE;

		// apply new accelerometer values to last frames
		FilteredAccelerometer = FilteredAccelerometer * VectorFilter + (1.0f - VectorFilter) * NewAcceleration;
		
		// create an normalized acceleration vector
		FVector FinalAcceleration = -FilteredAccelerometer.SafeNormal();
		
		// calculate Roll/Pitch
		FLOAT CurrentPitch = appAtan2(FinalAcceleration.Y, FinalAcceleration.Z);
		FLOAT CurrentRoll = -appAtan2(FinalAcceleration.X, FinalAcceleration.Z);
		
		// if we want to calibrate, use the current values as center
		if (bIsCalibrationRequested)
		{
			CenterPitch = CurrentPitch;
			CenterRoll = CurrentRoll;
			bIsCalibrationRequested = FALSE;
		}

		CurrentPitch -= CenterPitch;
		CurrentRoll -= CenterRoll;

		Attitude = FVector(CurrentPitch, 0, CurrentRoll);
		RotationRate = FVector(LastPitch - CurrentPitch, 0, LastRoll - CurrentRoll);
		Gravity = FVector(0, 0, 0);
		// use the raw acceleration for acceleration
		Acceleration = NewAcceleration;
		
		// remember for next time (for rotation rate)
		LastPitch = CurrentPitch;
		LastRoll = CurrentRoll;
	}
}

/**
 * Call this function to performa any calibration of the motion controls                                                                     
 */
void FIPhoneInputManager::CalibrateMotion()
{
	// If we are using the motion manager, grab a reference frame.  Note, once you set the Attitude Reference frame
	// all additional reference information will come from it
	if (MotionManager.deviceMotionActive)
	{
		ReferenceAttitude = [MotionManager.deviceMotion.attitude retain];
	}
	else
	{
		bIsCalibrationRequested = TRUE;
	}
}
