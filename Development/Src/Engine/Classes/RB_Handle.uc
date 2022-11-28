// Utility object for moving actors around.
// Note - it really doesn't care which actor its a component of - you can use it to pick its owner up or anything else.

class RB_Handle extends ActorComponent
	collapsecategories
	hidecategories(Object)
	native(Physics);

cpptext
{
	// UActorComponent interface
	virtual void Created();
	virtual void Destroyed();
	virtual void Tick(FLOAT DeltaTime);

	// URB_Handle interface
	void GrabComponent(UPrimitiveComponent* Component, FName InBoneName, const FVector& Location, UBOOL bConstrainRotation);
	void ReleaseComponent();

	void SetLocation(FVector NewLocation);
	void SetOrientation(FQuat NewOrientation);
}

var PrimitiveComponent		GrabbedComponent;
var name					GrabbedBoneName;

var native const pointer	HandleData;
var native const vector		GlobalHandlePos, LocalHandlePos;
var native const vector     HandleAxis1, HandleAxis2, HandleAxis3;
var native const bool		bRotationConstrained;

// How strong handle is.
var()	FLOAT	LinearDamping;
var()	FLOAT	LinearStiffness;
var()	FLOAT	LinearMaxDistance;

var()	FLOAT	AngularDamping;
var()	FLOAT	AngularStiffness;

native function GrabComponent(PrimitiveComponent Component, Name InBoneName, vector Location, bool bConstrainRotation);
native function ReleaseComponent();

native function SetLocation(vector NewLocation);
native function SetOrientation(quat NewOrientation);

defaultproperties
{
	LinearDamping=5000
	LinearStiffness=10000
	LinearMaxDistance=10000.0

	AngularDamping=10000
	AngularStiffness=15000
}