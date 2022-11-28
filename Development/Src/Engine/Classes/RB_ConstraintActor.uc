//=============================================================================
// The Basic constraint actor class.
//=============================================================================

class RB_ConstraintActor extends Actor
    abstract
	placeable
	native(Physics);

cpptext
{
	virtual void physRigidBody(FLOAT DeltaTime) {};
	virtual void PostEditMove();
	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual void CheckForErrors(); // used for checking that this constraint is valid buring map build

	virtual void InitRBPhys();
	virtual void TermRBPhys();

	void SetDisableCollision(UBOOL NewDisableCollision);
}

// Actors joined effected by this constraint (could be NULL for 'World')
var() edfindable Actor					ConstraintActor1;
var() edfindable Actor					ConstraintActor2;

var() editinline export RB_ConstraintSetup		ConstraintSetup;
var() editinline export RB_ConstraintInstance	ConstraintInstance;

// Disable collision between actors joined by this constraint.
var() const bool						bDisableCollision;

native final function SetDisableCollision(bool NewDisableCollision);

defaultproperties
{
	Begin Object Class=RB_ConstraintInstance Name=MyConstraintInstance
	End Object
	ConstraintInstance=MyConstraintInstance

	Begin Object Class=RB_ConstraintDrawComponent Name=MyConDrawComponent
	End Object
	Components.Add(MyConDrawComponent)

    bDisableCollision=false

	SupportedEvents.Add(class'SeqEvent_ConstraintBroken')

	bCollideActors=false
    bHidden=True
	DrawScale=0.5
	bEdShouldSnap=true
 
    Physics=PHYS_RigidBody
	bStatic=false
}