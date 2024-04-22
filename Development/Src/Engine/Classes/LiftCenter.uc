//=============================================================================
// LiftCenter.
// Used to support AI navigation on lifts.
// should be placed in the center of the navigable lift surface.
// Used in conjunction with LiftExits
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class LiftCenter extends NavigationPoint
	placeable
	native;

var		InterpActor		MyLift;
var		float			MaxDist2D;
var		vector			LiftOffset;		// starting vector between MyLift location and LiftCenter location
var		bool			bJumpLift;
var		float			CollisionHeight;
/** if specified, must touch this to start the lift */
var() Trigger LiftTrigger;

cpptext
{
	virtual UBOOL ShouldBeBased();
#if WITH_EDITOR
	void addReachSpecs(AScout *Scout, UBOOL bOnlyChanged);
	virtual void ReviewPath(APawn* Scout);
#endif
	void FindBase();
}

event PostBeginPlay()
{
	Super.PostBeginPlay();

	if (Base == MyLift && MyLift != None)
	{
		LiftOffset = Location - MyLift.Location;
		MyLift.bIsLift = true;
	}
}

/** SpecialHandling is called by the navigation code when the next path has been found.
It gives that path an opportunity to modify the result based on any special considerations
Here, we check if the mover needs to be triggered
*/
event Actor SpecialHandling(Pawn Other)
{
	// if no lift, no trigger, or trigger already hit, no special handling
	if (MyLift == None || LiftTrigger == None || LiftTrigger.bRecentlyTriggered)
	{
		return self;
	}
	else
	{
		return LiftTrigger;
	}
}

/*
Check if mover is positioned to allow Pawn to get on
*/
event bool SuggestMovePreparation(Pawn Other)
{
	// if already on lift, no problem
	if ( Other.base == MyLift )
		return false;

	// make sure LiftCenter is correctly positioned on the lift
	if ( (Base != MyLift) || (Location != MyLift.Location + LiftOffset) )
	{
		SetLocation(MyLift.Location + LiftOffset);
		SetBase(MyLift);
	}

	// if mover is moving, wait
	if (!IsZero(MyLift.velocity) || !ProceedWithMove(Other))
	{
		Other.Controller.WaitForMover(MyLift);
		return true;
	}

	return false;
}

function bool ProceedWithMove(Pawn Other)
{
	// see if mover is at appropriate location
	if ( Other.Controller == None )
		return false;
	else if ( (LiftExit(Other.Controller.MoveTarget) != None) && Other.ReachedDestination(self) )
		return LiftExit(Other.Controller.MoveTarget).CanBeReachedFromLiftBy(Other);
	else
	{
		//check distance directly - make sure close
		if ( (Location.Z - CollisionHeight < Other.Location.Z - Other.GetCollisionHeight() + Other.MAXSTEPHEIGHT + 2.0)
			&& (Location.Z - CollisionHeight > Other.Location.Z - Other.GetCollisionHeight() - 1200)
			&& (VSize2D(Location - Other.Location) < MaxDist2D || (IsZero(MyLift.Velocity) && Other.ValidAnchor() && LiftExit(Other.Anchor) != None)) )
		{
			return true;
		}
	}

	// if we need to hit the trigger, go do that
	if (LiftTrigger != None && !LiftTrigger.bRecentlyTriggered && IsZero(MyLift.Velocity))
	{
		Other.SetMoveTarget(LiftTrigger);
		return true;
	}

	return false;
}

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorResources.Lift_Center'
	End Object

	RemoteRole=ROLE_None
	bStatic=false
	bSpecialMove=true
	ExtraCost=400
	MaxDist2D=+400.000
	bNoAutoConnect=true
	bNeverUseStrafing=true
	bForceNoStrafing=true
	CollisionHeight=50
}
