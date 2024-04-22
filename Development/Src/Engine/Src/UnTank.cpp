/*=============================================================================
	UnTank.cpp: Simulation of tracked vehicles such as tanks.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(USVehicleSimTank);

/** Simulate the engine model of the treaded vehicle */
void USVehicleSimTank::UpdateVehicle(ASVehicle* Vehicle, FLOAT DeltaTime)
{
#if WITH_NOVODEX
	// Determine how much torque we are getting from the engine
	FLOAT EngineTorque = 0.f;
	if(bTurnInPlaceOnSteer)
	{
		EngineTorque = Clamp<FLOAT>(Abs(Vehicle->OutputGas) + Abs(TurnInPlaceThrottle * Vehicle->OutputSteering), -1.0, 1.0) * MaxEngineTorque;
	}
	else
	{
		EngineTorque = Clamp<FLOAT>(Abs(Vehicle->OutputGas), -1.0, 1.0) * MaxEngineTorque;
	}

	UBOOL bInvertTorque = (Vehicle->OutputGas < 0.f);

	// Lose torque when climbing too steep
	FRotationMatrix R(Vehicle->Rotation);
	if ( R.GetAxis(2).Z < Vehicle->WalkableFloorZ )
	{
		if ( (Vehicle->OutputGas > 0.f) == (R.GetAxis(0).Z > 0.f) )
		{
			// Kill torque if trying to go up
			EngineTorque = 0.f;
		}
	}

	if (Vehicle->OutputSteering != 0.f)
	{
		FLOAT InsideTrackFactor;
		if( Abs(Vehicle->OutputGas) > 0.f )
		{
			InsideTrackFactor = InsideTrackTorqueFactor;
		}
		else
		{
			InsideTrackFactor = -0.5f;
		}

		//FLOAT InsideTrackFactor = Clamp(InsideTrackTorqueCurve.Eval(Vehicle->ForwardVel, 0.0f), -1.0f, 1.0f);
		
		// Determine how to split up the torque based on the InsideTrackTorqueCurve
		FLOAT InsideTrackTorque = EngineTorque * InsideTrackFactor;
		FLOAT OutsideTrackTorque = EngineTorque * (1.0f - Abs(InsideTrackFactor)); 

		if (Vehicle->OutputSteering < 0.f) // Turn Right
		{
			LeftTrackTorque = OutsideTrackTorque; 
			RightTrackTorque = InsideTrackTorque;
		}
		else // Turn Left
		{	
			LeftTrackTorque = InsideTrackTorque;
			RightTrackTorque = OutsideTrackTorque;
		}
	}
	else
	{
		// If not steering just split up the torque equally between the two tracks
		LeftTrackTorque = EngineTorque * 0.5f;
		RightTrackTorque = EngineTorque * 0.5f;
	}

	// Invert torques when you want to drive backwards.
	if(bInvertTorque)
	{
		LeftTrackTorque *= -1.f;
		RightTrackTorque *= -1.f;
	}

	LeftTrackVel += (LeftTrackTorque - (LeftTrackVel * EngineDamping)) * DeltaTime;
	RightTrackVel += (RightTrackTorque - (RightTrackVel * EngineDamping)) * DeltaTime;

	// Do the simulation for each wheel.
	ApplyWheels(LeftTrackVel,RightTrackVel,Vehicle);
	
#endif // WITH_NOVODEX
}
void USVehicleSimTank::ApplyWheels(FLOAT InLeftTrackVel, FLOAT InRightTrackVel, ASVehicle* Vehicle)
{
#if WITH_NOVODEX

#if !FINAL_RELEASE
	// Checking for bad velocities
	if(Abs(InLeftTrackVel) > 10000.f || appIsNaN(InLeftTrackVel))
	{
		debugf(TEXT("Large/Invalid InLeftTrackVel! %f (in %s)"), InLeftTrackVel, *Vehicle->GetName());
	}

	if(Abs(InRightTrackVel) > 10000.f || appIsNaN(InRightTrackVel))
	{
		debugf(TEXT("Large/Invalid InRightTrackVel! %f (in %s)"), InRightTrackVel, *Vehicle->GetName());
	}
#endif // FINAL_RELEASE

	FLOAT TrackDifference = Abs(LeftTrackVel - RightTrackVel);

	for (INT i = 0; i < Vehicle->Wheels.Num(); i++)
	{
		USVehicleWheel* Wheel = Vehicle->Wheels(i);	
		NxWheelShape* WheelShape = Wheel->GetNxWheelShape();
		check(WheelShape);		

		// Calculate the drive torque and steer braking applied at the wheel
		if (Wheel->Side == SIDE_Left)
			WheelShape->setAxleSpeed(U2PScale * InLeftTrackVel);
		else
			WheelShape->setAxleSpeed(U2PScale * InRightTrackVel);
		
		// Increase slip on wheels with positive SteerFactor when turning.
		FLOAT LatGripScale = 1.f;
		if(Wheel->SteerFactor > 0.0f)
		{
			FLOAT ReduceGrip = Min(TrackDifference * TurnGripScaleRate, TurnMaxGripReduction);
			LatGripScale -= ReduceGrip;
			// Ensure we are not making grip larger or negative.
			LatGripScale = Clamp(LatGripScale, 0.f, 1.f);
		}

		SetNxWheelShapeTireForceFunctions(WheelShape, Wheel, 1.f, LatGripScale);
	}
#endif
}

void USVehicleSimTank::ProcessCarInput(ASVehicle* Vehicle)
{
	UBOOL bReverse = false;

	// If the vehicle has no driver, apply the brakes.
	if(Vehicle->Driver == NULL)
	{
		Vehicle->OutputGas = 0.0f;
		Vehicle->OutputSteering = 0.f;
		Vehicle->OutputRise = 0.f;
		Vehicle->bOutputHandbrake = false;
	}
	// Otherwise process the user input.
	else
	{	
		Vehicle->OutputGas = Vehicle->Throttle;
		Vehicle->OutputRise = Vehicle->Rise;
			
		// Just pass steering control directly through.
		Vehicle->OutputSteering = Vehicle->Steering;

		// Keep the rigid body for the vehicle awake.
		check(Vehicle->CollisionComponent);
		Vehicle->CollisionComponent->WakeRigidBody();
	}

	// Grab the view direction as well - in case we want it.
	if ( Vehicle->IsHumanControlled() )
	{			
		Vehicle->DriverViewPitch = Vehicle->Controller->Rotation.Pitch;
		Vehicle->DriverViewYaw = Vehicle->Controller->Rotation.Yaw;
	}
	else
	{
		Vehicle->DriverViewPitch = Vehicle->Rotation.Pitch;
		Vehicle->DriverViewYaw = Vehicle->Rotation.Yaw;
	}
}
