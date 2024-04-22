/*=============================================================================
	CameraController.h: Implements controls for a camera with pseudo-physics
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if _MSC_VER
#pragma once
#endif

#ifndef __CameraController_h__
#define __CameraController_h__


/**
 * FCameraControllerUserImpulseData
 *
 * Wrapper structure for all of the various user input parameters for camera movement
 */
class FCameraControllerUserImpulseData
{

public:

	/** Scalar user input for moving forwards (positive) or backwards (negative) */
	FLOAT MoveForwardBackwardImpulse;

	/** Scalar user input for moving right (positive) or left (negative) */
	FLOAT MoveRightLeftImpulse;

	/** Scalar user input for moving up (positive) or down (negative) */
	FLOAT MoveUpDownImpulse;

	/** Scalar user input for turning right (positive) or left (negative) */
	FLOAT RotateYawImpulse;

	/** Scalar user input for pitching up (positive) or down (negative) */
	FLOAT RotatePitchImpulse;

	/** Scalar user input for rolling clockwise (positive) or counter-clockwise (negative) */
	FLOAT RotateRollImpulse;

	/** Velocity modifier for turning right (positive) or left (negative) */
	FLOAT RotateYawVelocityModifier;

	/** Velocity modifier for pitching up (positive) or down (negative) */
	FLOAT RotatePitchVelocityModifier;

	/** Velocity modifier for rolling clockwise (positive) or counter-clockwise (negative) */
	FLOAT RotateRollVelocityModifier;

	/** Scalar user input for increasing FOV (zoom out, positive) or decreasing FOV (zoom in, negative) */
	FLOAT ZoomOutInImpulse;


	/** Constructor */
	FCameraControllerUserImpulseData()
		: MoveForwardBackwardImpulse( 0.0f ),
		  MoveRightLeftImpulse( 0.0f ),
		  MoveUpDownImpulse( 0.0f ),
		  RotateYawImpulse( 0.0f ),
		  RotatePitchImpulse( 0.0f ),
		  RotateRollImpulse( 0.0f ),
		  RotateYawVelocityModifier( 0.0f ),
		  RotatePitchVelocityModifier( 0.0f ),
		  RotateRollVelocityModifier( 0.0f ),
		  ZoomOutInImpulse( 0.0f )
	{
	}
};



/**
 * FCameraControllerConfig
 *
 * Configuration data for the camera controller object
 */
class FCameraControllerConfig
{

public:

	/**
	 * General configuration
	 */

	/** Impulses below this amount will be ignored */
	FLOAT ImpulseDeadZoneAmount;


	/**
	 * Movement configuration
	 */

	/** True if camera movement (forward/backward/left/right) should use a physics model with velocity */
	UBOOL bUsePhysicsBasedMovement;

	/** Movement acceleration rate in units per second per second */
	FLOAT MovementAccelerationRate;

	/** Movement velocity damping amount in 'velocities' per second */
	FLOAT MovementVelocityDampingAmount;

	/** Maximum movement speed in units per second */
	FLOAT MaximumMovementSpeed;


	/**
	 * Rotation configuration
	 */

	/** True if camera rotation (yaw/pitch/roll) should use a physics model with velocity */
	UBOOL bUsePhysicsBasedRotation;

	/**Allows xbox controller to temporarily force rotational physics on*/
	UBOOL bForceRotationalPhysics;

	/** Rotation acceleration rate in degrees per second per second */
	FLOAT RotationAccelerationRate;

	/** Rotation velocity damping amount in 'velocities' per second */
	FLOAT RotationVelocityDampingAmount;

	/** Maximum rotation speed in degrees per second */
	FLOAT MaximumRotationSpeed;

	/** Minimum allowed camera pitch rotation in degrees */
	FLOAT MinimumAllowedPitchRotation;

	/** Maximum allowed camera pitch rotation in degrees */
	FLOAT MaximumAllowedPitchRotation;


	/**
	 * FOV zooming configuration
	 */

	/** True if FOV should snap back when flight controls are released */
	UBOOL bEnableFOVRecoil;

	/** True if FOV zooming should use a physics model with velocity */
	UBOOL bUsePhysicsBasedFOV;

	/** FOV acceleration rate in degrees per second per second */
	FLOAT FOVAccelerationRate;

	/** FOV velocity damping amount in 'velocities' per second */
	FLOAT FOVVelocityDampingAmount;

	/** Maximum FOV change speed in degrees per second */
	FLOAT MaximumFOVSpeed;

	/** Minimum allowed camera FOV in degrees */
	FLOAT MinimumAllowedFOV;

	/** Maximum allowed camera FOV in degrees */
	FLOAT MaximumAllowedFOV;

	/**Multiplier for translation movement*/
	FLOAT TranslationMultiplier;
	/**Multiplier for rotation movement*/
	FLOAT RotationMultiplier;
	/**Multiplier for zoom movement*/
	FLOAT ZoomMultiplier;
	/**Camera Trim (pitch offset) */
	FLOAT PitchTrim;

	/**Invert the impulses on the x axis*/
	UBOOL bInvertX;
	/**Invert the impulses on the y axis*/
	UBOOL bInvertY;
	/**Whether the camera is planar or free flying*/
	UBOOL bPlanarCamera;

	/** Constructor */
	FCameraControllerConfig()
		: ImpulseDeadZoneAmount( 0.2f ),
		  bUsePhysicsBasedMovement( TRUE ),	  
		  MovementAccelerationRate( 20000.0f ),
		  MovementVelocityDampingAmount( 10.0f ),
		  MaximumMovementSpeed( MAX_FLT ),
		  bUsePhysicsBasedRotation( FALSE ),
		  bForceRotationalPhysics( FALSE ),
		  RotationAccelerationRate( 1600.0f ),
		  RotationVelocityDampingAmount( 12.0f ),
		  MaximumRotationSpeed( MAX_FLT ),
		  MinimumAllowedPitchRotation( -90.0f ),
		  MaximumAllowedPitchRotation( 90.0f ),
		  bEnableFOVRecoil( TRUE ),
		  bUsePhysicsBasedFOV( TRUE ),
		  FOVAccelerationRate( 1200.0f ),
		  FOVVelocityDampingAmount( 10.0f ),
		  MaximumFOVSpeed( MAX_FLT ),
		  MinimumAllowedFOV( 5.0f ),
		  MaximumAllowedFOV( 170.0f ),
		  TranslationMultiplier(1.0f),
		  RotationMultiplier(1.0f),
		  ZoomMultiplier(1.0f),
		  PitchTrim(0.0f),
		  bInvertX(FALSE),
		  bInvertY(FALSE),
		  bPlanarCamera(FALSE)
	{
	}

};



/**
 * FEditorCameraController
 *
 * An interactive camera movement system.  Supports simple physics-based animation.
 */
class FEditorCameraController
{

public:

	/** Constructor */
	FEditorCameraController();


	/** Sets the configuration for this camera controller */
	void SetConfig( const FCameraControllerConfig& InConfig )
	{
		Config = InConfig;
	}


	/** Returns the configuration of this camera controller */
	FCameraControllerConfig& GetConfig()
	{
		return Config;
	}


	/** Access the configuration for this camera.  Making changes is allowed. */
	FCameraControllerConfig& AccessConfig()
	{
		return Config;
	}


	/**
	 * Updates the position and orientation of the camera as well as other state (like velocity.)  Should be
	 * called every frame.
	 *
	 * @param	UserImpulseData			Input data from the user this frame
	 * @param	DeltaTime				Time interval since last update
	 * @param	bAllowRecoilIfNoImpulse	True if we should recoil FOV if needed
	 * @param	MovementSpeedScale		Scales the speed of movement
	 * @param	InOutCameraPosition		[in, out] Camera position
	 * @param	InOutCameraEuler		[in, out] Camera orientation
	 * @param	InOutCameraFOV			[in, out] Camera field of view
	 */
	void UpdateSimulation(
		const FCameraControllerUserImpulseData& UserImpulseData,
		const FLOAT DeltaTime,
		const UBOOL bAllowRecoilIfNoImpulse,
		const FLOAT MovementSpeedScale,
		FVector& InOutCameraPosition,
		FVector& InOutCameraEuler,
		FLOAT& InOutCameraFOV );

	/**TRUE if this camera currently has rotational velocity*/
	UBOOL IsRotating (void) const;
private:

	/**
	 * Applies the dead zone setting to the incoming user impulse data
	 *
	 * @param	InUserImpulse	Input user impulse data
	 * @param	OutUserImpulse	[out] Output user impulse data with dead zone applied
	 * @param	bOutAnyImpulseData	[out] True if there was any user impulse this frame
	 */
	void ApplyImpulseDeadZone( const FCameraControllerUserImpulseData& InUserImpulse,
							   FCameraControllerUserImpulseData& OutUserImpulse,
							   UBOOL& bOutAnyImpulseData );

	/**
	 * Updates the camera position.  Called every frame by UpdateSimulation.
	 *
	 * @param	UserImpulse				User impulse data for the current frame
	 * @param	DeltaTime				Time interval
	 * @param	MovementSpeedScale		Additional movement accel/speed scale
	 * @param	CameraEuler				Current camera rotation
	 * @param	InOutCameraPosition		[in, out] Camera position
	 */
	void UpdatePosition( const FCameraControllerUserImpulseData& UserImpulse, const FLOAT DeltaTime, const FLOAT MovementSpeedScale, const FVector& CameraEuler, FVector& InOutCameraPosition );


	/**
	 * Update the field of view.  Called every frame by UpdateSimulation.
	 *
	 * @param	UserImpulse		User impulse data for this frame
	 * @param	DeltaTime		Time interval
	 * @param	InOutCameraFOV	[in, out] Camera field of view
	 */
	void UpdateFOV( const FCameraControllerUserImpulseData& UserImpulse, const FLOAT DeltaTime, FLOAT& InOutCameraFOV );


	/**
	 * Applies FOV recoil (if appropriate)
	 *
	 * @param	DeltaTime					Time interval
	 * @param	bAllowRecoilIfNoImpulse		Whether recoil should be allowed if there wasn't any user impulse
	 * @param	bAnyUserImpulse				True if there was user impulse data this iteration
	 * @param	InOutCameraFOV				[in, out] Camera field of view
	 */
	void ApplyRecoil( const FLOAT DeltaTime, const UBOOL bAllowRecoilIfNoImpulse, UBOOL bAnyUserImpulse, FLOAT& InOutCameraFOV );


	/**
	 * Updates the camera rotation.  Called every frame by UpdateSimulation.
	 *
	 * @param	UserImpulse			User impulse data for this frame
	 * @param	DeltaTime			Time interval
	 * @param	InOutCameraEuler	[in, out] Camera rotation
	 */
	void UpdateRotation( const FCameraControllerUserImpulseData& UserImpulse, const FLOAT DeltaTime, FVector &InOutCameraEuler );



private:

	/** Configuration */
	FCameraControllerConfig Config;

	/** World space movement velocity */
	FVector MovementVelocity;

	/** FOV velocity in degrees per second */
	FLOAT FOVVelocity;

	/** Rotation velocity euler (yaw, pitch and roll) in degrees per second */
	FVector RotationVelocityEuler;

	/** Cached FOV angle, for recoiling back to the original FOV */
	FLOAT OriginalFOVForRecoil;

};




#endif // __CameraController_h__