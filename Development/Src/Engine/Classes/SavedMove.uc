//=============================================================================
// SavedMove is used during network play to buffer recent client moves,
// for use when the server modifies the clients actual position, etc.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class SavedMove extends Object
	native;

// also stores info in Acceleration attribute
var SavedMove NextMove;		// Next move in linked list.
var float TimeStamp;		// Time of this move.
var float Delta;			// amount of time for this move
var bool	bRun;
var bool	bDuck;
var bool	bPressedJump;
var bool	bDoubleJump;
var bool	bPreciseDestination;
var bool	bForceRMVelocity;			// client-side only (for replaying moves) - not replicated
var bool bForceMaxAccel;
var EDoubleClickDir DoubleClickMove;	// Double click info.
var EPhysics SavedPhysics;
var vector StartLocation, StartRelativeLocation, StartVelocity, StartFloor, SavedLocation, SavedVelocity, SavedRelativeLocation, RMVelocity, Acceleration;
var rotator Rotation;
var Actor StartBase, EndBase;
var float CustomTimeDilation;
var float AccelDotThreshold;	// threshold for deciding this is an "important" move based on DP with last acked acceleration

// Root motion correction variables
var bool			bRootMotionFromInterpCurve;
var float			RootMotionInterpCurrentTime;
var Vector			RootMotionInterpCurveLastValue;
var ERootMotionMode	RootMotionMode;

function Clear()
{
	TimeStamp = 0;
	Delta = 0;
	DoubleClickMove = DCLICK_None;
	Acceleration = vect(0,0,0);
	StartVelocity = vect(0,0,0);
	bRun = false;
	bDuck = false;
	bPressedJump = false;
	bDoubleJump = false;
	bPreciseDestination = false;
	bForceRMVelocity = false;
	CustomTimeDilation = 1.0;
}

function PostUpdate(PlayerController P)
{
	bDoubleJump = P.bDoubleJump || bDoubleJump;
	if ( P.Pawn != None )
	{
		RMVelocity = P.Pawn.RMVelocity;
		SavedLocation = P.Pawn.Location;
		SavedVelocity = P.Pawn.Velocity;
		EndBase = P.Pawn.Base;
		if ( (EndBase != None) && !EndBase.bWorldGeometry )
			SavedRelativeLocation = P.Pawn.Location - EndBase.Location;
	}
	Rotation = P.Rotation;
}

function bool IsImportantMove(vector CompareAccel)
{
	local vector AccelNorm;

	// check if any important movement flags set
	if ( bPressedJump || 
		 bDoubleJump  || 
		 ((DoubleClickMove != DCLICK_None) && (DoubleClickMove != DCLICK_Active) && (DoubleClickMove != DCLICK_Done)) )
	{
		return true;
	}

	if( bRootMotionFromInterpCurve )
	{
		return TRUE;
	}

	// check if acceleration has changed significantly
	AccelNorm = Normal(Acceleration);
	return ( (CompareAccel != AccelNorm) && ((CompareAccel Dot AccelNorm) < AccelDotThreshold) );
}

function vector GetStartLocation()
{
	if( (StartBase != None) && !StartBase.bWorldGeometry )
	{
		return StartBase.Location + StartRelativeLocation;
	}

	return StartLocation;
}

function SetInitialPosition(Pawn P)
{
	SavedPhysics = P.Physics;
	StartLocation = P.Location;
	StartVelocity = P.Velocity;
	StartBase = P.Base;
	StartFloor = P.Floor;
	CustomTimeDilation = P.CustomTimeDilation;

	if( (StartBase != None) && !StartBase.bWorldGeometry )
	{
		StartRelativeLocation = P.Location - StartBase.Location;
	}

	// Store root motion information
	bRootMotionFromInterpCurve = P.bRootMotionFromInterpCurve;
	if( bRootMotionFromInterpCurve )
	{
		RootMotionInterpCurrentTime		= P.RootMotionInterpCurrentTime;
		RootMotionInterpCurveLastValue	= P.RootMotionInterpCurveLastValue;
		RootMotionMode					= P.Mesh.RootMotionMode;
	}
}

function bool CanCombineWith(SavedMove NewMove, Pawn InPawn, float MaxDelta)
{
	if( InPawn == None )
	{
		return FALSE;
	}

	if( bRootMotionFromInterpCurve )
	{
		return FALSE;
	}

	if ( NewMove.Acceleration == vect(0,0,0) )
	{
		return ( (Acceleration == vect(0,0,0))
			&& (StartVelocity == vect(0,0,0))
			&& (NewMove.StartVelocity == vect(0,0,0))
			&& (SavedPhysics == InPawn.Physics)
			&& !bPressedJump && !NewMove.bPressedJump
			&& (bRun == NewMove.bRun)
			&& (bDuck == NewMove.bDuck)
			&& (bPreciseDestination == NewMove.bPreciseDestination)
			&& (bDoubleJump == NewMove.bDoubleJump)
			&& ((DoubleClickMove == DCLICK_None) || (DoubleClickMove == DCLICK_Active))
			&& (NewMove.DoubleClickMove == DoubleClickMove)
			&& !bForceRMVelocity && !NewMove.bForceRMVelocity)
			&& (CustomTimeDilation == NewMove.CustomTimeDilation);
	}
	else
	{
		return ( (InPawn != None)
			&& (NewMove.Delta + Delta < MaxDelta)
			&& (SavedPhysics == InPawn.Physics)
			&& !bPressedJump && !NewMove.bPressedJump
			&& (bRun == NewMove.bRun)
			&& (bDuck == NewMove.bDuck)
			&& (bDoubleJump == NewMove.bDoubleJump)
			&& (bPreciseDestination == NewMove.bPreciseDestination)
			&& ((DoubleClickMove == DCLICK_None) || (DoubleClickMove == DCLICK_Active))
			&& (NewMove.DoubleClickMove == DoubleClickMove)
			&& ((Normal(Acceleration) Dot Normal(NewMove.Acceleration)) > 0.99)
			&& !bForceRMVelocity && !NewMove.bForceRMVelocity)
			&& (CustomTimeDilation == NewMove.CustomTimeDilation);
	}
}

function SetMoveFor(PlayerController P, float DeltaTime, vector NewAccel, EDoubleClickDir InDoubleClick)
{
	Delta = DeltaTime;

	// NOTE: max replicated vector component magnitude is 2^18, and acceleration is multiplied by 10 before replication
	// quick check to make sure we never cross this limit of 2^18/10
	if( VSize(NewAccel) > 26214 )
	{
		NewAccel = 26214 * Normal(NewAccel);
	}

	if( P.Pawn != None )
	{
		SetInitialPosition(P.Pawn);
	}
	Acceleration = NewAccel;
	DoubleClickMove = InDoubleClick;
	bRun = (P.bRun > 0);
	bDuck = (P.bDuck > 0);
	bPressedJump = P.bPressedJump;
	bDoubleJump = P.bDoubleJump;
	bPreciseDestination = P.bPreciseDestination;
	bForceRMVelocity = P.bPreciseDestination ||
						(P.Pawn != None && P.Pawn.Mesh != None && !P.Pawn.bRootMotionFromInterpCurve && 
							(P.Pawn.Mesh.RootMotionMode == RMM_Accel || P.Pawn.Mesh.RootMotionMode == RMM_Velocity));

	bForceMaxAccel = P.Pawn != None && P.Pawn.bForceMaxAccel;
	TimeStamp = P.WorldInfo.TimeSeconds;
}

/**
 * Called before PlayerController.ClientUpdatePosition uses this SavedMove to make a predictive correction
 */
function PrepMoveFor( Pawn P )
{
	if( P != None )
	{
		P.bForceRMVelocity = bForceRMVelocity;
		P.bForceMaxAccel = bForceMaxAccel;

		// Reset root motion information
		P.bRootMotionFromInterpCurve = bRootMotionFromInterpCurve;
		if( P.bRootMotionFromInterpCurve )
		{
			P.Mesh.RootMotionMode = RootMotionMode;
			P.RootMotionInterpCurveLastValue = RootMotionInterpCurveLastValue;
			P.SetRootMotionInterpCurrentTime( RootMotionInterpCurrentTime, Delta, TRUE );			
		}
	}
}


/**
 * Called after PlayerController.ClientUpdatePosition used this SavedMove to make a predictive correction
 */
function ResetMoveFor( Pawn P )
{
	if( P != None )
	{
		SavedLocation = P.Location;
		SavedVelocity = P.Velocity;
		EndBase = P.Base;
		if( EndBase != None && !EndBase.bWorldGeometry )
		{
			SavedRelativeLocation = P.Location - EndBase.Location;
		}	
		
		P.bForceRMVelocity = false;
	}
}

/* CompressedFlags()
returns a byte containing encoded special movement information (jumping, crouching, etc.)
SetFlags() and UnCompressFlags() should be overridden to allow game specific special movement information
*/
function byte CompressedFlags()
{
	local byte Result;

	Result = DoubleClickMove;

	if ( bRun )
		Result += 8;
	if ( bDuck )
		Result += 16;
	if ( bPressedJump )
		Result += 32;
	if ( bDoubleJump )
		Result += 64;
	if ( bPreciseDestination )
		Result += 128;

	return Result;
}

/* SetFlags()
Set the movement parameters for PC based on the passed in Flags
*/
static function EDoubleClickDir SetFlags(byte Flags, PlayerController PC)
{
		if ( (Flags & 8) != 0 )
			PC.bRun = 1;
		else
			PC.bRun = 0;
		if ( (Flags & 16) != 0 )
			PC.bDuck = 1;
		else
			PC.bDuck = 0;

		PC.bPreciseDestination = ( (Flags & 128) != 0 );
		PC.bDoubleJump = ( (Flags & 64) != 0 );
		PC.bPressedJump = ( (Flags & 32) != 0 );
		switch (Flags & 7)
		{
			case 0:
				return DCLICK_None;
				break;
			case 1:
				return DCLICK_Left;
				break;
			case 2:
				return DCLICK_Right;
				break;
			case 3:
				return DCLICK_Forward;
				break;
			case 4:
				return DCLICK_Back;
				break;
		}
		return DCLICK_None;
}

function String GetDebugString()
{
	local String Str;

	Str = self@`showvar(Delta)@`showvar(SavedPhysics)@`showvar(StartLocation)@`showvar(StartVelocity)@`showvar(SavedLocation)@`showvar(SavedVelocity)@`showvar(RMVelocity)@`showvar(Acceleration)@`showvar(bRootMotionFromInterpCurve)@`showvar(RootMotionInterpCurrentTime);
	return Str;
}

defaultproperties
{
	AccelDotThreshold=+0.9
}
