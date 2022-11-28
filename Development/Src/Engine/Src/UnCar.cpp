/*=============================================================================
	UnCar.cpp: Additional code to make wheeled SVehicles work (engine etc)
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Originally created by James Golding
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "UnStatChart.h"

IMPLEMENT_CLASS(ACar);

void ACar::UpdateVehicle(FLOAT DeltaTime)
{
	// Update ForwardVel, CarMPH and bIsInverted on both server and client.
	FMatrix CarTM = LocalToWorld();

	FVector WorldForward = CarTM.GetAxis(0);
	FVector WorldUp = CarTM.GetAxis(2);
  	
  	ForwardVel = Velocity | WorldForward;
	ChartData( FString(TEXT("VehicleVel")), ForwardVel );
	//debugf( TEXT("ForwardVel: %f"), ForwardVel );

	CarMPH = Abs( (ForwardVel * 3600.0f) / 140800.0f ); // Convert from units per sec to miles per hour.

	bIsInverted = WorldUp.Z < 0.2f;

	// If on the server - we work out OutputGas, OutputBrake etc, and pack them to be sent to the client.
	if(Role == ROLE_Authority)
	{
		ProcessCarInput();
	}

	//debugf( TEXT("OutputGas: %f OutputBrake: %f Steering: %f"), OutputGas, OutputBrake, Steering );


	/////////// STEERING ///////////

	FLOAT maxSteerAngle = MaxSteerAngleCurve.Eval(Velocity.Size(), 0.f);
	FLOAT maxSteer = DeltaTime * SteerSpeed;

	FLOAT deltaSteer;
	deltaSteer = (-Steering * maxSteerAngle) - ActualSteering; // Amount we want to move (target - current)
	deltaSteer = Clamp<FLOAT>(deltaSteer, -maxSteer, maxSteer);
	ActualSteering += deltaSteer;

	/////////// ENGINE ///////////

	// Calculate torque at output of engine. Combination of throttle, current RPM and engine braaking.
	FLOAT EngineTorque = OutputGas * TorqueCurve.Eval( EngineRPM, 0.f );
	FLOAT EngineBraking = (1.0f - OutputGas) * (EngineBrakeRPMScale*EngineRPM * EngineBrakeRPMScale*EngineRPM * EngineBrakeFactor);

	EngineTorque -= EngineBraking;

	// Total gear ratio between engine and differential (ie before being split between wheels).
	// A higher gear ratio and the torque at the wheels is reduced.
	FLOAT EngineWheelRatio = GearRatios[Gear] * TransRatio;

	// Reset engine RPM. We calculate this by adding the component of each wheel spinning.
	FLOAT NewTotalSpinVel=0.0f;
	EngineRPM = 0.0f;

	// Do model for each wheel.
	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);

		/////////// DRIVE ///////////

		// Heuristic to divide torque up so that the wheels that are spinning slower get more of it.
		// Sum of LSDFactor across all wheels should be 1.
		// JTODO: Do we need to handle the case of vehicles with different size wheels?
		FLOAT LSDSplit, EvenSplit, UseSplit;

		EvenSplit = 1.f/(FLOAT)NumPoweredWheels;

		// If no wheels are spinning, just do an even split.
		if(TotalSpinVel > 0.1f)
			LSDSplit = (TotalSpinVel - vw->SpinVel)/(((FLOAT)NumPoweredWheels-1.f) * TotalSpinVel);
		else
			LSDSplit = EvenSplit;

		UseSplit = ((1-LSDFactor) * EvenSplit) + (LSDFactor * LSDSplit);

		// Calculate Drive Torque : applied at wheels (ie after gearbox and differential)
		// This is an 'open differential' ie. equal torque to each wheel
		FLOAT DriveTorque = UseSplit * (EngineTorque / EngineWheelRatio);

		/////////// LONGITUDINAL ///////////

		// Calculate Grip Torque : longitudinal force against ground * distance of action (radius of tyre)
		// LongFrictionFunc is assumed to be reflected for negative Slip Ratio
		//FLOAT GripTorque = FTScale * vw->WheelRadius * vw->TireLoad * WheelLongFrictionScale * WheelLongFrictionFunc.Eval( Abs(vw->SlipVel), 0.f );
		FLOAT GripTorque = FTScale * vw->WheelRadius * WheelLongFrictionScale * WheelLongFrictionFunc.Eval( Abs(vw->SlipVel), 0.f );
		if(vw->SlipVel < 0.0f)
			GripTorque *= -1.0f;

		// GripTorque can't be more than the torque needed to invert slip ratio.
		FLOAT TransInertia = (EngineInertia / Abs(GearRatios[Gear] * TransRatio)) + vw->WheelInertia;
		//FLOAT SlipAngVel = vw->SlipVel/vw->WheelRadius;

		// Brake torque acts to stop wheels (ie against direction of motion)
		FLOAT BrakeTorque = 0.0f;

		if(vw->SpinVel > 0.0f)
			BrakeTorque = -OutputBrake * MaxBrakeTorque;
		else
			BrakeTorque = OutputBrake * MaxBrakeTorque;

		FLOAT LimitBrakeTorque = ( Abs(vw->SpinVel) * TransInertia ) / DeltaTime; // Size of torque needed to completely stop wheel spinning.
		BrakeTorque = Clamp(BrakeTorque, -LimitBrakeTorque, LimitBrakeTorque); // Never apply more than this!

		// Resultant torque at wheel : torque applied from engine + brakes + equal-and-opposite from tire-road interaction.
		FLOAT WheelTorque = DriveTorque + BrakeTorque - GripTorque;
	
		// Resultant linear force applied to car. (GripTorque applied at road)
		FLOAT VehicleForce = GripTorque / (FTScale * vw->WheelRadius);

		// If the wheel torque is opposing the direction of spin (ie braking) we use friction to apply it.
		//if( OutputBrake > 0.0f ||  (DriveTorque + BrakeTorque) * vw->SpinVel < 0.0f)
		//{
		//	vw->DriveForce = 0.0f;
		//	vw->LongFriction = Abs(VehicleForce) + (OutputBrake * MinBrakeFriction);
		//}
		//else
		//{
			vw->DriveForce = VehicleForce;
		//	vw->LongFriction = 0.0f;
		//}

		// Calculate torque applied back to chassis if wheel is on the ground
		if (vw->bWheelOnGround)
			vw->ChassisTorque = -1.0f * (DriveTorque + BrakeTorque) * ChassisTorqueScale;
		else
			vw->ChassisTorque = 0.0f;

		// Calculate new wheel speed. 
		// The lower the gear ratio, the harder it is to accelerate the engine.
		FLOAT TransAcc = WheelTorque / TransInertia;
		vw->SpinVel += TransAcc * DeltaTime;

		// Make sure the wheel can't spin in the wrong direction for the current gear.
		if(Gear == 0 && vw->SpinVel > 0.0f)
			vw->SpinVel = 0.0f;
		else if(Gear > 0 && vw->SpinVel < 0.0f)
			vw->SpinVel = 0.0f;

		// Accumulate wheel spin speeds to find engine RPM. 
		// The lower the gear ratio, the faster the engine spins for a given wheel speed.
		NewTotalSpinVel += vw->SpinVel;
		EngineRPM += vw->SpinVel / EngineWheelRatio;

		/////////// LATERAL ///////////

		// Nothing to do..

		/////////// STEERING  ///////////

		// Pass on steering to wheels that want it.
		if(vw->SteerType == VST_Steered)
			vw->Steer = ActualSteering;
		else if(vw->SteerType == VST_Inverted)
			vw->Steer = -ActualSteering;
		else
			vw->Steer = 0.0f;
	}

	// EngineRPM is in radians per second, want in revolutions per minute
	EngineRPM /= (FLOAT)NumPoweredWheels;
	EngineRPM /= 2.0f * (FLOAT)PI; // revs per sec
	EngineRPM *= 60;
	EngineRPM = Max( EngineRPM, 0.01f ); // ensure always positive!

	ChartData( FString(TEXT("EngineRPM")), EngineRPM );

	// Update total wheel spin vel
	TotalSpinVel = NewTotalSpinVel;
}

void ACar::ChangeGear(UBOOL bReverse)
{
	// If we want reverse, but aren't there already, and the engine is idling, change to it.
	if(bReverse && Gear != 0)// && EngineRPM < 100)
	{
		Gear = 0;
		return;
	}

	// If we want forwards, but are in reverse, and the engine is idling, change to it.
	if(!bReverse && Gear == 0)// && EngineRPM < 100)
	{
		Gear = 1;
		return;
	}

	// No gear changes in reverse!
	if(Gear == 0)
		return;

	// Forwards...
	if( EngineRPM > ChangeUpPoint && Gear < NumForwardGears )
	{
		Gear++;
	}
	else if( EngineRPM < ChangeDownPoint && Gear > 1 )
	{
		Gear--;
	}	
}

/**
 *  INPUT: Velocity, bIsInverted, StopThreshold, HandbrakeThresh, Throttle, Driver, ActualSteering, bIsDriving, OutputHandbrake, EngineRPM
 *	OUTPUT: OutputBrake, bIsDriving, OutputGas, Gear
 */
void ACar::ProcessCarInput()
{
	// 'ForwardVel' isn't very helpful if we are inverted, so we just pretend its positive.
	if(bIsInverted)
		ForwardVel = 2.0f * StopThreshold;

	//Log("F:"$ForwardVel$"IsI:"$bIsInverted);

	UBOOL bReverse = false;

	if( Driver == NULL )
	{
		OutputBrake = 1.0f;
		OutputGas = 0.0f;
		ChangeGear(false);
	}
	else
	{
		if(Throttle > 0.01f) // pressing forwards
		{
			if(ForwardVel < -StopThreshold) // going backwards - so brake first
			{
				//debugf(TEXT("F - Brake"));
				bReverse = true;
				OutputBrake = Abs(Throttle);
				bIsDriving = false;
			}
			else // stopped or going forwards, so drive
			{
				//debugf(TEXT("F - Drive"));
				OutputBrake = 0.0f;
				bIsDriving = true;
			}
		}
		else if(Throttle < -0.01f) // pressing backwards
		{
			// We have to release the brakes and then press reverse again to go into reverse
			// Also, we can only go into reverse once the engine has slowed down.
			if(ForwardVel < StopThreshold && bIsDriving == false)
			{
				//debugf(TEXT("B - Drive"));
				bReverse = true;
				OutputBrake = 0.0f;
			}
			else // otherwise, we are going forwards, or still holding brake, so just brake
			{
				//debugf(TEXT("B - Brake"));
				if ( (ForwardVel >= StopThreshold) || IsHumanControlled() )
					OutputBrake = Abs(Throttle);

				bIsDriving = false;
			}
		}
		else // not pressing either
		{
			// If stationary, stick brakes on
			if(Abs(ForwardVel) < StopThreshold)
			{
				//debugf(TEXT("B - Brake"));
				OutputBrake = 1.0;
				bIsDriving = false;
			}
			else if(ForwardVel < -StopThreshold) // otherwise, coast (but keep it in reverse if we're going backwards!)
			{
				//debugf(TEXT("Coast Backwards"));
				bReverse = true;
				OutputBrake = 0.0;
				bIsDriving = false;
			}
			else
			{
				//debugf(TEXT("Coast"));
				OutputBrake = 0.0;
				bIsDriving = false;
			}
		}

		// If there is any brake, dont throttle.
		if(OutputBrake > 0.0f)
			OutputGas = 0.0f;
		else
			OutputGas = Abs(Throttle);

		ChangeGear(bReverse);

		check(CollisionComponent);
		CollisionComponent->WakeRigidBody();
	}

#if 0
	// Force drive forwards (for testing)
	OutputBrake = 0.0f;
	OutputGas = Abs(0.1f);
	CollisionComponent->WakeRigidBody();
#endif
}

/** Handy function that synchronises all the wheels with the vehicles copy of the per-wheel paramters. */
void ACar::SyncWheelParams()
{
	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);

		vw->SuspensionStiffness = WheelSuspensionStiffness;
		vw->SuspensionDamping = WheelSuspensionDamping;
		vw->SuspensionBias = WheelSuspensionBias;

		vw->LatFrictionStatic = WheelLatFrictionStatic;
		vw->LatFrictionDynamic = WheelLatFrictionDynamic;
		vw->Restitution = WheelRestitution;
		vw->WheelInertia = WheelInertia;
	}
}

/** Do sync of wheel params when we edit car with EditActor. */
void ACar::PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);

	SyncWheelParams();
}

void ACar::PreInitVehicle()
{
	// Initially sync wheel parameters in vehicle class with each wheel.
	SyncWheelParams();

	// Count the number of powered wheels on the car
	NumPoweredWheels = 0;
	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);
		
		if(vw->bPoweredWheel)
		{
			NumPoweredWheels++;
		}
	}
}