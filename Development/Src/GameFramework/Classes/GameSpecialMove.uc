/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameSpecialMove extends Object
	config(Pawn)
	native(SpecialMoves)
	abstract;

// C++ functions
cpptext
{
	virtual void PrePerformPhysics(FLOAT DeltaTime);
	virtual void PostProcessPhysics(FLOAT DeltaTime);
	virtual void TickSpecialMove(FLOAT DeltaTime);
}

/** Owner of this special move */
var	GamePawn PawnOwner;
/** Named handle of this special move */
var Name	 Handle;

/** Last time CanDoSpecialMove was called. */
var transient float LastCanDoSpecialMoveTime;
/** Can we do the current special move? */
var private bool bLastCanDoSpecialMove;

/** Flag used when moving Pawn to a precise location */
var const	bool	bReachPreciseDestination;
/** Flag set when Pawn reached precise location */
var	const	bool	bReachedPreciseDestination;
/** World location to reach */
var const	vector	PreciseDestination;
var const	Actor	PreciseDestBase;
var const	vector	PreciseDestRelOffset;

/** Flag used when rotating pawn to a precise rotation */
var const	bool	bReachPreciseRotation;
/** Flag set when pawn reached precise rotation */
var const	bool	bReachedPreciseRotation;
/** Time to interpolate Pawn's rotation */
var const	float	PreciseRotationInterpolationTime;
/** World rotation to face */
var const	Rotator	PreciseRotation;

/** PrecisePosition will not be enforced on non-owning instance unless this flag is set */
var bool	bForcePrecisePosition;

function InitSpecialMove( GamePawn InPawn, Name InHandle )
{
	PawnOwner = InPawn;
	Handle = InHandle;
}

/**
 *	Give special move a chance to set info flags
 */
function InitSpecialMoveFlags( out int out_Flags );

/**
 *	Give special move a chance to pull info out of flags
 */
function ExtractSpecialMoveFlags( int Flags );

/**
 * Can the special move be chained after the current one finishes?
 */
function bool CanChainMove( Name NextMove )
{
	return FALSE;
}

/**
 * Can a new special move override this one before it is finished?
 * This is only if CanDoSpecialMove() == TRUE && !bForce when starting it.
 */
function bool CanOverrideMoveWith( Name NewMove )
{
	return FALSE;
}

/**
 * Can this special move override InMove if it is currently playing?
 */
function bool CanOverrideSpecialMove( Name InMove )
{
	return FALSE;
}

/**
 * Public accessor to see if this special move can be done, handles caching the results for a single frame.
 * @param bForceCheck - Allows you to skip the single frame condition (which will be incorrect on clients since LastCanDoSpecialMoveTime isn't replicated)
 */
final function bool CanDoSpecialMove( optional bool bForceCheck )
{
	if( PawnOwner != None )
	{
		// update the cached value if outdated
		if( bForceCheck || PawnOwner.WorldInfo.TimeSeconds != LastCanDoSpecialMoveTime )
		{
			bLastCanDoSpecialMove		= InternalCanDoSpecialMove();
			LastCanDoSpecialMoveTime	= PawnOwner.WorldInfo.TimeSeconds;
		}
		// return the cached value
		return bLastCanDoSpecialMove;
	}

	return FALSE;
}

/**
 * Checks to see if this Special Move can be done.
 */
protected function bool InternalCanDoSpecialMove()
{
	return TRUE;
}

/**
 * Event called when Special Move is started.
 */
function SpecialMoveStarted( bool bForced, Name PrevMove );

/**
 * Event called when Special Move is finished.
 */
function SpecialMoveEnded( Name PrevMove, Name NextMove );

/** Script Tick function. */
function Tick( float DeltaTime );

/** called when DoSpecialMove() is called again with this special move, but the special move flags have changed */
function SpecialMoveFlagsUpdated();

/** Should this special move be replicated to non-owning clients? */
function bool ShouldReplicate()
{
	// by default all moves get replicated via GearPawn.ReplicatedSpecialMove
	return TRUE;
}

/**
 * Send Pawn to reach a precise destination.
 * ReachedPrecisePosition() event will be called when Pawn reaches destination.
 * This tries to get the Pawn as close as possible from DestinationToReach in 2D space (so Z is ignored).
 * This doesn't use the path network, so PawnOwner should be already fairly close to destination.
 * A TimeOut should be used to prevent the Pawn from being stuck.
 * @param	DestinationToReach	point in world space to reach. (Z ignored).
 * @param	bCancel				if TRUE, this will cancel any current PreciseDestination movement.
 */
native final function SetReachPreciseDestination(vector DestinationToReach, optional bool bCancel);

/**
 * Force Pawn to face a specific rotation.
 * @param	RotationToFace		Rotation for Pawn to face.
 * @param	InterpolationTime	Time it takes for Pawn to face given rotation.
 */
native final function SetFacePreciseRotation(rotator RotationToFace, float InterpolationTime);

/**
 * Event sent when Pawn has reached precise position.
 * PreciseRotation or PreciseLocation, or Both.
 * When both Rotation and Location are set, the event is fired just once,
 * after the Pawn has reached both.
 */
event ReachedPrecisePosition();

/** Reset FacePreciseRotation related vars
  * Otherwise, these vars will be carried over to next action
  * vars are const in script - so need native interface
  **/
native final function ResetFacePreciseRotation();

/**
 * Generic function to send message events to SpecialMoves.
 * Returns TRUE if message has been processed correctly.
 */
function bool MessageEvent(Name EventName, Object Sender)
{
	`log(PawnOwner.WorldInfo.TimeSeconds @ PawnOwner @ class @ GetFuncName() @ "Received unhandled event!" @ `showvar(EventName) @ "from:" @ `showvar(Sender) );
	ScriptTrace();

	return FALSE;
}

/** Forces Pawn's rotation to a given Rotator */
final native function ForcePawnRotation(Pawn P, Rotator NewRotation);

/**
 * Turn a World Space location into an Actor Space relative location.
 */
native final function vector WorldToRelativeOffset(Rotator InRotation, Vector WorldSpaceOffset) const;
native final function vector RelativeToWorldOffset(Rotator InRotation, Vector RelativeSpaceOffset) const;

defaultproperties
{
}