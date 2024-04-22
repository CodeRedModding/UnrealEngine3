/*=============================================================================
	UnCar.cpp: Additional code to make wheeled SVehicles work (engine etc)
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"


IMPLEMENT_CLASS(USVehicleSimCar);

/**
 *	Executed on the server to turn low-level user inputs into high-level car-specific inputs
 *  INPUT: Velocity, bIsInverted, StopThreshold, Throttle, Steering, Driver, bIsDriving, EngineRPM
 *	OUTPUT: OutputBrake, bIsDriving, OutputGas, OutputGear, OutputSteering
 */
void USVehicleSimCar::ProcessCarInput(ASVehicle* Vehicle)
{
	// 'ForwardVel' isn't very helpful if we are inverted, so we just pretend its positive.
	if(Vehicle->bIsInverted)
	{
		Vehicle->ForwardVel = 2.0f * StopThreshold;
	}

	UBOOL bReverse = false;

	if( bAutoDrive )
	{
		Vehicle->OutputBrake = 0.f;
		Vehicle->OutputGas = 1.0f;
		Vehicle->OutputSteering = AutoDriveSteer;
	}
	else if( Vehicle->Driver == NULL )
	{
		Vehicle->OutputBrake = 1.0f;
		Vehicle->OutputGas = 0.0f;
		Vehicle->bOutputHandbrake = FALSE;
	}
	else
	{
		if(Vehicle->Throttle > 0.01f) // pressing forwards
		{
			if (Vehicle->ForwardVel < -StopThreshold) // Vehicle is moving backwards so brake first
			{
				Vehicle->OutputBrake = 1.0f;
			}
			else
			{
				Vehicle->OutputBrake = 0.0f;
			}
			TimeSinceThrottle = GWorld->GetTimeSeconds();
		}
		else if(Vehicle->Throttle < -0.01f) // pressing backwards
		{
			// We have to release the brakes and then press reverse again to go into reverse
			// Also, we can only go into reverse once the engine has slowed down.
			if(Vehicle->ForwardVel < StopThreshold)
			{
				bReverse = true;
				Vehicle->OutputBrake = 0.0f;
			}
			else // otherwise, we are going forwards, or still holding brake, so just brake
			{
				if ( (Vehicle->ForwardVel >= StopThreshold) || Vehicle->IsHumanControlled() )
				{
					Vehicle->OutputBrake = Abs(Vehicle->Throttle);
				}
			}
		}
		else // not pressing either
		{
			// If stationary, stick brakes on
			if( Abs(Vehicle->ForwardVel) < StopThreshold )
			{
				//debugf(TEXT("B - Brake"));
				Vehicle->OutputBrake = 1.0;
			}
			else
			{
				//debugf(TEXT("Coast"));
				Vehicle->OutputBrake = 0.0;
				Vehicle->OutputGas = 0.0f;
			}
		}
		
		UpdateHandbrake(Vehicle);

		// If there is any brake, dont throttle.
		if(Vehicle->OutputBrake > 0.0f)
		{	
			Vehicle->OutputGas = 0.0f;
		}
		else
		{
			if (Vehicle->Throttle > 0.01f)
			{
				Vehicle->OutputGas = Vehicle->Throttle;
			}
			else if (Vehicle->Throttle < -0.01f)
			{	
				Vehicle->OutputGas = ReverseThrottle;
			}
			else
			{
				Vehicle->OutputGas = 0.0f;
			}
		}

		// Steering is easy - just pass steering control directly through.
		Vehicle->OutputSteering = Vehicle->Steering;

		// Keep away physics of any drive vehicle.
		check(Vehicle->CollisionComponent);
		Vehicle->CollisionComponent->WakeRigidBody();
	}
}

void USVehicleSimCar::UpdateHandbrake(ASVehicle* Vehicle)
{
	// If we are not forcibly holding down the handbrake for some reason (Scorpion boost),
	// pressing 'up' (ie making Rise positive) turns on the handbrake.
	if ( !Vehicle->bHoldingDownHandbrake )
		Vehicle->bOutputHandbrake = (Vehicle->Rise > 0.f);
}
