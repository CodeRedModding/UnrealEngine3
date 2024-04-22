/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SVehicleSimCar extends SVehicleSimBase
	native(Physics);

// Vehicle params
var()	float				ChassisTorqueScale;  // How much torque to apply to chassis based on acceleration

var()	InterpCurveFloat	MaxSteerAngleCurve;  // degrees based on velocity
var()	float				SteerSpeed;          // degrees per second

var()   float               ReverseThrottle;     // How much throttle when in reverse

var()	float				EngineBrakeFactor;   // How much the engine slows down when not applying throttle

var()	float				MaxBrakeTorque;      // Amount of stopping torque applied when applying the brakes

var()	float				StopThreshold;       // Speed at which the vehicle will stop simulating

// Internal
var		bool				bIsDriving;
var		float				ActualSteering;
var     float               TimeSinceThrottle;

cpptext
{
	// SVehicleSimBase interface.
	virtual void ProcessCarInput(ASVehicle* Vehicle);
	virtual void UpdateHandbrake(ASVehicle* Vehicle);
}

defaultproperties
{
	ReverseThrottle=-1.0
}
