/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SVehicleSimTank extends SVehicleSimCar
	native(Physics);

var		float		LeftTrackVel;
var		float		RightTrackVel;

var     float       LeftTrackTorque;
var     float       RightTrackTorque;

// With full throttle applied, the tank engine will split up
// the MaxEngineToque across both tracks. This will be an even
// split unless the tank is being steered. When steered, the
// torque will be split based on the InsideTrackTorqueCurve
// according to its current speed.
//
// For example...
// When driving forward each track gets 50% of the total
// torque. However, if the tank is steering left and the
// InsideTrackTorqueCurve is 0.2, this will apply 20% of
// the engine torque to the left track and the remaining
// 80% will be applied to the right track. If the
// InsideTrackTorqueCurve is -0.2 it will still split the
// torque 20/80 however the inside torque will be applied
// in the reverse direction.
//
// Keep in mind that total torque is conserved across both
// tracks so if you were to direct 100% to the inside track
// there wouldn't be any torque left to send to the outside
// track.
//
// In addition, any track wheels with a positive SteerFactor 
// will have their lateral stiffness adjusted using 
// TurnMaxGripReduction/TurnGripScaleRate when tracks are at different speeds.
//
// TurnInPlaceThrottle allows you to set a reduced amount
// of throttle for turning in place.

var()   float               MaxEngineTorque;
var()   float               EngineDamping;
var()	float				InsideTrackTorqueFactor;
var()   float               SteeringLatStiffnessFactor;
var()   float               TurnInPlaceThrottle;

/** Maximum amount we will reduce lateral grip. */
var()	float				TurnMaxGripReduction;

/** How quickly grip drops off based on difference in track speed. */
var()	float				TurnGripScaleRate;

/** If true, tank will turn in place when just steering is applied. */
var()	bool				bTurnInPlaceOnSteer;

cpptext
{
	// SVehicleSimBase interface.
	virtual void ProcessCarInput(ASVehicle* Vehicle);
	virtual void UpdateVehicle(ASVehicle* Vehicle, FLOAT DeltaTime);
	virtual void ApplyWheels(FLOAT InLeftTrackVel, FLOAT InRightTrackVel, ASVehicle* Vehicle);
}

DefaultProperties
{
    bWheelSpeedOverride=true
	bTurnInPlaceOnSteer=true

	TurnMaxGripReduction=0.97
	TurnGripScaleRate=1.0

	MaxEngineTorque=500
}
