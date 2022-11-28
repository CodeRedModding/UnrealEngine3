class LandShark extends Car
	placeable;

var()	SoundCue	TestCue;
var()	AnimTree	TestTree;

defaultproperties
{
	Begin Object Name=CollisionCylinder
		CollisionRadius=+0256.000000
		BlockZeroExtent=false
		HiddenGame=false
	End Object
	CylinderComponent=CollisionCylinder

	Begin Object Name=SVehicleMesh
		SkeletalMesh=SkeletalMesh'TestVehicles.LandShark'
		PhysicsAsset=PhysicsAsset'TestVehicles.LandShark_Physics'
	End Object

	Components.Add(SVehicleMesh)

	WheelSuspensionStiffness=300.0
	WheelSuspensionDamping=20.0
	WheelSuspensionBias=0.0

	WheelLongFrictionFunc=(Points=((InVal=0,OutVal=0.0),(InVal=100.0,OutVal=1.0),(InVal=200.0,OutVal=0.9),(InVal=10000000000.0,OutVal=0.9)))
	WheelLongFrictionScale=100.0
	WheelLatFrictionStatic = 0.8
	WheelLatFrictionDynamic = 0.7
	WheelRestitution=0.0
	WheelSuspensionMaxRenderTravel=15.0

	FTScale=0.0001
	ChassisTorqueScale=0.0

	MinBrakeFriction=4.0
	MaxBrakeTorque=20.0
	MaxSteerAngleCurve=(Points=((InVal=0,OutVal=25.0)))//,(InVal=1500.0,OutVal=11.0),(InVal=1000000000.0,OutVal=11.0)))
	SteerSpeed=160
	StopThreshold=100
	TorqueCurve=(Points=((InVal=0,OutVal=9.0),(InVal=200,OutVal=10.0),(InVal=1500,OutVal=11.0),(InVal=2800,OutVal=0.0)))
	EngineBrakeFactor=0.0001
	EngineBrakeRPMScale=0.1
	EngineInertia=0.1
	WheelInertia=0.1

	TransRatio=0.2
	GearRatios[0]=-0.5
	GearRatios[1]=0.4
	GearRatios[2]=0.65
	GearRatios[3]=0.85
	GearRatios[4]=1.1
	ChangeUpPoint=2000
	ChangeDownPoint=1000
	LSDFactor=0.0

	ExitPositions(0)=(Y=-400,Z=-20)
	ExitPositions(1)=(Y=400,Z=-20)

	bAttachDriver=true

	Begin Object Class=SVehicleWheel Name=RRWheel
		BoneName="Wheel_01"
		BoneRollAxis=AXIS_Y
		BoneSteerAxis=AXIS_Z
		BoneOffset=(X=0.0,Y=0.0,Z=0.0)
		WheelRadius=50
		bPoweredWheel=True
		SteerType=VST_Fixed
	End Object
	Wheels(0)=RRWheel

	Begin Object Class=SVehicleWheel Name=LRWheel
		BoneName="Wheel_04"
		BoneRollAxis=AXIS_Y
		BoneSteerAxis=AXIS_Z
		BoneOffset=(X=0.0,Y=0.0,Z=0.0)
		WheelRadius=50
		bPoweredWheel=True
		SteerType=VST_Fixed
	End Object
	Wheels(1)=LRWheel

	Begin Object Class=SVehicleWheel Name=RFWheel
		BoneName="Wheel_02"
		BoneRollAxis=AXIS_Y
		BoneSteerAxis=AXIS_Z
		BoneOffset=(X=0.0,Y=0.0,Z=0.0)
		WheelRadius=50
		bPoweredWheel=True
		SteerType=VST_Steered
	End Object
	Wheels(2)=RFWheel

	Begin Object Class=SVehicleWheel Name=LFWheel
		BoneName="Wheel_03"
		BoneRollAxis=AXIS_Y
		BoneSteerAxis=AXIS_Z
		BoneOffset=(X=0.0,Y=0.0,Z=0.0)
		WheelRadius=50
		bPoweredWheel=True
		SteerType=VST_Steered
	End Object
	Wheels(3)=LFWheel
}