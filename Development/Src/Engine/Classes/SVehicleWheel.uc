class SVehicleWheel extends Component
	native(Physics);

// INPUT TO CAR SIMULATION
var()		float				Steer; // degrees
var()		float				DriveForce; // resultant linear driving force at wheel center
var()		float				ChassisTorque; // Torque applied back to the chassis (equal-and-opposite) from this wheel.

// PARAMS
var()		bool				bPoweredWheel;
var() enum EVehicleSteerType
{
	VST_Fixed,
	VST_Steered,
	VST_Inverted
} SteerType; // How steering affects this wheel.

var()		name				BoneName;
var()		EAxis				BoneRollAxis; // Local axis to rotate the wheel around for normal rolling movement.
var()		EAxis				BoneSteerAxis; // Local axis to rotate the wheel around for steering.
var()		vector				BoneOffset; // Offset from wheel bone to line check point (middle of tyre). NB: Not affected by scale.
var()		float				WheelRadius; // Length of line check. Usually 2x wheel radius.

var()		float				SuspensionStiffness;
var()		float				SuspensionDamping;
var()		float				SuspensionBias;

var()		float				LatFrictionStatic;
var()		float				LatFrictionDynamic;
var()		float				Restitution;

var()		float				WheelInertia;
var()		float				SuspensionMaxRenderTravel;

var()		name				SupportBoneName; // Name of strut etc. that will be rotated around local X as wheel goes up and down.
var()		EAxis				SupportBoneAxis; // Local axis to rotate support bone around.


// OUTPUT FROM CAR SIMULATION

// Calculated on startup
var			vector				WheelPosition; // Wheel center in actor ref frame. Calculated using BoneOffset above.
var			float				SupportPivotDistance; // If a SupportBoneName is specified, this is the distance used to calculate the anglular displacement.

// Calculated each frame
var			bool				bWheelOnGround;
var			float				TireLoad; // Load on tire
var			float				SpinVel; // Radians per sec
var			float				SlipAngle; // Angle between wheel facing direction and wheel travelling direction. In degrees.
var			float				SlipVel;   // Difference in linear velocity between ground and wheel at contact.

var			float				SuspensionPosition; // Output vertical deflection position of suspension
var			float				CurrentRotation; // Output graphical rotation of the wheel.


// Used internally for physics stuff - DO NOT CHANGE!
var		transient const pointer	RayShape;
var		transient const int		WheelMaterialIndex;


defaultproperties
{
	WheelInertia=1.0
	SteerType=VST_Fixed
	SuspensionMaxRenderTravel=50.0
	WheelRadius=35
	BoneRollAxis=AXIS_X
	BoneSteerAxis=AXIS_Z
	SupportBoneAxis=AXIS_Y
}
