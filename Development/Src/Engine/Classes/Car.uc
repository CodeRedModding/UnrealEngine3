//-----------------------------------------------------------
// Basic car simulation
//-----------------------------------------------------------
class Car extends SVehicle
	abstract
	native(Physics);

cpptext
{
	// UObject interface
	virtual void PostEditChange(UProperty* PropertyThatChanged);

	// SVehicle interface.
	virtual void PreInitVehicle();
	virtual void UpdateVehicle(FLOAT DeltaTime);

	// SCar interface.
	void ProcessCarInput();
	void ChangeGear(UBOOL bReverse);
	void SyncWheelParams();
}

// Wheel params
var()	float				WheelSuspensionStiffness;
var()	float				WheelSuspensionDamping;
var()	float				WheelSuspensionBias;

var()	InterpCurveFloat	WheelLongFrictionFunc;
var()	float				WheelLongFrictionScale;
var()	float				WheelLatFrictionStatic;
var()	float				WheelLatFrictionDynamic;
var()	float				WheelRestitution;
var()	float				WheelInertia;
var()	float				WheelSuspensionMaxRenderTravel;

// Vehicle params
var()	float				FTScale;
var()	float				ChassisTorqueScale;
var()	float				MinBrakeFriction;

var()	InterpCurveFloat	MaxSteerAngleCurve; // degrees based on velocity
var()	InterpCurveFloat	TorqueCurve; // Engine output torque
var()	float				GearRatios[5]; // 0 is reverse, 1-4 are forward
var()	int					NumForwardGears;
var()	float				TransRatio; // Other (constant) gearing
var()	float				ChangeUpPoint;
var()	float				ChangeDownPoint;
var()	float				LSDFactor;

var()	float				EngineBrakeFactor;
var()	float				EngineBrakeRPMScale;

var()	float				MaxBrakeTorque;
var()	float				SteerSpeed; // degrees per second

var()	float				StopThreshold;

var()	float				EngineInertia; // Pre-gear box engine inertia (racing flywheel etc.)

var()	float				IdleRPM;

// Internal
var		float				OutputBrake;
var		float				OutputGas;
var		int					Gear;

var		float				ForwardVel;
var		bool				bIsInverted;
var		bool				bIsDriving;
var		int					NumPoweredWheels;

var		float				TotalSpinVel;
var		float				EngineRPM;
var		float				CarMPH;
var		float				ActualSteering;

// camera
var()	vector		BaseOffset;
var()	float		CamDist;

/**
 *	Calculate camera view point, when viewing this pawn.
 *
 * @param	fDeltaTime	delta time seconds since last update
 * @param	out_CamLoc	Camera Location
 * @param	out_CamRot	Camera Rotation
 * @param	out_FOV		Field of View
 *
 * @return	true if Pawn should provide the camera point of view.
 */
simulated function bool PawnCalcCamera( float fDeltaTime, out vector out_CamLoc, out rotator out_CamRot, out float out_FOV )
{
	local vector	Pos, HitLocation, HitNormal;
	
	// Simple third person view implementation
	GetActorEyesViewPoint( out_CamLoc, out_CamRot );
	
	out_CamLoc += BaseOffset;
	Pos = out_CamLoc - Vector(out_CamRot) * CamDist;
	if( Trace(HitLocation, HitNormal, Pos, out_CamLoc, false, vect(0,0,0)) != None )
	{
		out_CamLoc = HitLocation + HitNormal*2;
	}
	else
	{
		out_CamLoc = Pos;
	}

	return true;
}

simulated function name GetDefaultCameraMode( PlayerController RequestedBy )
{
	return 'Default';
}

///////////////////////////////////////////
/////////////// CREATION //////////////////
///////////////////////////////////////////

defaultproperties
{
	BaseOffset=(Z=128)
	CamDist=512

	NumForwardGears=4
}
