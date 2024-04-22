/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SVehicleWheel extends Component
	native(Physics);

enum EWheelSide
{
    SIDE_None,
    SIDE_Left,
    SIDE_Right
};

// INPUT TO CAR SIMULATION
var()		float				Steer; // degrees
var()		float				MotorTorque; //
var()		float				BrakeTorque; //
var()		float				ChassisTorque; // Torque applied back to the chassis (equal-and-opposite) from this wheel.

// PARAMS
var()		bool				bPoweredWheel;
var()		bool				bHoverWheel;  // Determines whether this wheel will collide with water

/** If true, this wheel will collide with other vehicles (add RBCC_Vehicle to its RBCollideWithChannels). */
var()		bool				bCollidesVehicles;

/** If true, this wheel will collide with pawns (add RBCC_Pawn to its RBCollideWithChannels). */
var()		bool				bCollidesPawns;


/** How steering affects this wheel. 0.0 means it is not steered. 1.0 means steered fully normally. -1.0 means reversed steering. */
var()		float				SteerFactor;

var()		name				SkelControlName;
var			SkelControlWheel	WheelControl;
var()		name				BoneName;
var()		vector				BoneOffset; // Offset from wheel bone to line check point (middle of tyre). NB: Not affected by scale.
var()		float				WheelRadius; // Length of line check. Usually 2x wheel radius.
var()		float				SuspensionTravel;
var()		float				SuspensionSpeed; // Max speed at which rendered wheel will move up or down (0 = instant)

var()		ParticleSystem		WheelParticleSystem;

var()		EWheelSide			Side; // What side of the vehicle the wheel is on (optional).

// Wheel slippyness factors - These factors scale the wheel slip curve defined in SVehicleSimBase
var()       float               LongSlipFactor;
var()       float               LatSlipFactor;
var()       float               HandbrakeLongSlipFactor;
var()       float               HandbrakeLatSlipFactor;
var()		float				ParkedSlipFactor;

// Internal sound variable
var         bool                bIsSquealing;

// OUTPUT FROM CAR SIMULATION

// Calculated on startup
var			vector				WheelPosition; // Wheel center in actor ref frame. Calculated using BoneOffset above.

// Calculated each frame
var			bool				bWheelOnGround;
var			float				SpinVel; // Radians per sec
var			float				LongSlipRatio;   // Either the difference in linear velocity between ground and wheel or the slip ratio
var			float				LatSlipAngle;    // Either the difference in linear velocity between ground and wheel or the slip angle
var         vector              ContactNormal;
var         vector              LongDirection;
var         vector              LatDirection;
var         float               ContactForce;
var         float               LongImpulse;
var         float               LatImpulse;

var			float				DesiredSuspensionPosition; // Desired vertical deflection position of suspension
var			float				SuspensionPosition; // Output vertical deflection position of suspension
var			float				CurrentRotation; // Output graphical rotation of the wheel. In degrees.

// Used internally for physics stuff - DO NOT CHANGE!
var	transient const pointer						WheelShape;
var	transient const int							WheelMaterialIndex;

/** the class to use for WheelParticleComp */
var class<ParticleSystemComponent> WheelPSCClass;

var ParticleSystemComponent WheelParticleComp;
/** parameter that should be set in WheelParticleComp to the wheel's slip velocity */
var name SlipParticleParamName;

cpptext
{
#if WITH_NOVODEX
	class NxWheelShape* GetNxWheelShape()
	{
		return (NxWheelShape*)WheelShape;
	}
#endif

	/** @return whether this wheel wants a particle component attached to it */
	virtual UBOOL WantsParticleComponent();
}

defaultproperties
{
	WheelRadius=35
	SuspensionTravel=30
	LongSlipFactor=4000
	LatSlipFactor=20000
	HandbrakeLongSlipFactor=4000
	HandbrakeLatSlipFactor=20000
	ParkedSlipFactor=20000
	WheelPSCClass=class'ParticleSystemComponent'
	SuspensionSpeed=50
	SlipParticleParamName=WheelSlip
	bCollidesVehicles=TRUE
}
