/*=============================================================================
// LadderVolumes, when touched, cause ladder supporting actors to use Phys_Ladder.
// note that underwater ladders won't be waterzones (no breathing problems)
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
============================================================================= */

class LadderVolume extends PhysicsVolume
	native
	placeable;

var() rotator WallDir;
var vector LookDir;
var vector ClimbDir;	// pawn can move in this direction (or reverse)
var const Ladder LadderList;		// list of Ladder actors associated with this LadderVolume
var() bool	bNoPhysicalLadder;	// if true, won't push into/keep player against geometry in lookdir
var() bool	bAutoPath;			// add top and bottom ladders automatically
var() bool  bAllowLadderStrafing;  // if true, players on ladder can strafe sideways

var Pawn PendingClimber;

/** Editor visual cue for the direction of the wall */
var ArrowComponent WallDirArrow;

cpptext
{
	// Editor modification
#if WITH_EDITOR
	FVector FindTop(FVector V);
	FVector FindCenter();
	virtual INT AddMyMarker(AActor *S);
#endif
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent);
}

simulated event PostBeginPlay()
{
	local Ladder L, M;
	local vector Dir;

	Super.PostBeginPlay();
	LookDir = vector(WallDir);
	if ( !bAutoPath && (LookDir.Z != 0) )
	{
		ClimbDir = vect(0,0,1);
		for ( L=LadderList; L!=None; L=L.LadderList )
			for ( M=LadderList; M!=None; M=M.LadderList )
				if ( M != L )
				{
					Dir = Normal(M.Location - L.Location);
					if ( (Dir dot ClimbDir) < 0 )
						Dir *= -1;
					ClimbDir += Dir;
				}

		ClimbDir = Normal(ClimbDir);
		if ( (ClimbDir Dot vect(0,0,1)) < 0 )
			ClimbDir *= -1;
	}
}

function bool InUse(Pawn Ignored)
{
	local Pawn StillClimbing;

	ForEach TouchingActors(class'Pawn',StillClimbing)
	{
		if ( (StillClimbing != Ignored) && StillClimbing.bCollideActors && StillClimbing.bBlockActors )
			return true;
	}

	if ( PendingClimber != None )
	{
		if ( (PendingClimber.Controller == None)
			|| !PendingClimber.bCollideActors || !PendingClimber.bBlockActors
			|| (Ladder(PendingClimber.Controller.MoveTarget) == None)
			|| (Ladder(PendingClimber.Controller.MoveTarget).MyLadder != self) )
				PendingClimber = None;
	}
	return ( (PendingClimber != None) && (PendingClimber != Ignored) );
}

simulated event PawnEnteredVolume(Pawn P)
{
	local rotator PawnRot;

	Super.PawnEnteredVolume(P);
	if ( !P.CanGrabLadder() )
		return;

	PawnRot = P.Rotation;
	PawnRot.Pitch = 0;
	if ( (vector(PawnRot) Dot LookDir > 0.9)
		|| ((AIController(P.Controller) != None) && (Ladder(P.Controller.MoveTarget) != None)) )
		P.ClimbLadder(self);
	else if ( !P.bDeleteMe && (P.Controller != None) )
		spawn(class'PotentialClimbWatcher',P);
}

simulated event PawnLeavingVolume(Pawn P)
{
	local Controller C;

	if ( P.OnLadder != self )
		return;
	Super.PawnLeavingVolume(P);
	P.OnLadder = None;
	P.EndClimbLadder(self);
	if ( P == PendingClimber )
		PendingClimber = None;

	// tell all waiting pawns, if not in use
	if ( !InUse(P) )
	{
		foreach WorldInfo.AllControllers(class'Controller', C)
		{
			if (C.bPreparingMove && Ladder(C.MoveTarget) != None && Ladder(C.MoveTarget).MyLadder == self)
			{
				C.bPreparingMove = false;
				PendingClimber = C.Pawn;
				return;
			}
		}
	}
}

simulated event PhysicsChangedFor(Actor Other)
{
	if ( (Other.Physics == PHYS_Falling) || (Other.Physics == PHYS_Ladder) || Other.bDeleteMe || (Pawn(Other) == None) || (Pawn(Other).Controller == None) )
		return;
	spawn(class'PotentialClimbWatcher',Other);
}

defaultproperties
{
	Begin Object Class=ArrowComponent Name=Arrow
		ArrowColor=(R=150,G=100,B=150)
		ArrowSize=5.0
	End Object
	WallDirArrow = Arrow
	Components.Add(Arrow)

	Begin Object Name=BrushComponent0
		HiddenEditor=false
	End Object

	RemoteRole=ROLE_SimulatedProxy
	ClimbDir=(X=+0.0,Y=+0.0,Z=+1.0)
	bAutoPath=true
	bAllowLadderStrafing=true
}
