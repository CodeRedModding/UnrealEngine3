// Utility object for moving actors around.
// Note - it really doesn't care which actor its a component of - you can use it to pick its owner up or anything else.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

class RB_Handle extends ActorComponent
	collapsecategories
	hidecategories(Object)
	native(Physics);

cpptext
{
protected:
	// UActorComponent interface.
	virtual void Attach();
	virtual void Detach( UBOOL bWillReattach = FALSE );
	virtual void Tick(FLOAT DeltaTime);
public:
	virtual void TermComponentRBPhys(FRBPhysScene* InScene);

	// URB_Handle interface
}

var PrimitiveComponent		GrabbedComponent;
var name					GrabbedBoneName;

/** Physics scene index. */
var	transient native const int						SceneIndex;

/** Whether we are in the hardware or software scene. */
var	transient native const bool						bInHardware;

var transient native const pointer	HandleData{class NxJoint};
var	transient native const pointer	KinActorData{class NxActor};
var transient native const bool		bRotationConstrained;

// How strong handle is.
var()	FLOAT	LinearDamping;
var()	FLOAT	LinearStiffness;

/** Scales the handle spring stiffness along each axis (in local space of handle) */
var()	vector	LinearStiffnessScale3D;
/** Scales the handle spring damping along each axis (in local space of handle) */
var()	vector	LinearDampingScale3D;

var()	FLOAT	AngularDamping;
var()	FLOAT	AngularStiffness;

// for smooth linear interpolation of RB_Handle location
var		vector	Destination;
var		vector  StepSize;		// step size in units/sec
var		vector	Location;		// current location
var		bool	bInterpolating;

native function GrabComponent(PrimitiveComponent Component, Name InBoneName, vector GrabLocation, bool bConstrainRotation);
native function ReleaseComponent();

native function SetLocation(vector NewLocation);
native function SetSmoothLocation(vector NewLocation, float MoveTime);
/** Adjust interpolation goal location while respecting current interpolation timing.  Useful for interpolating to a moving target. */
native function UpdateSmoothLocation(const out vector NewLocation);
native function SetOrientation(const out quat NewOrientation);
native function Quat GetOrientation();

defaultproperties
{
	// Various physics related items need to be ticked pre physics update
	TickGroup=TG_PreAsyncWork

	LinearDamping=100.0
	LinearStiffness=1300.0

	AngularDamping=200.0
	AngularStiffness=1000.0

	LinearStiffnessScale3D=(X=1.0,Y=1.0,Z=1.0)
	LinearDampingScale3D=(X=1.0,Y=1.0,Z=1.0)
}
