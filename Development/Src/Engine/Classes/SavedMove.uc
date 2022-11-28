//=============================================================================
// SavedMove is used during network play to buffer recent client moves,
// for use when the server modifies the clients actual position, etc.
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class SavedMove extends Info;

// also stores info in Acceleration attribute
var SavedMove NextMove;		// Next move in linked list.
var float TimeStamp;		// Time of this move.
var float Delta;			// amount of time for this move
var bool	bRun;
var bool	bDuck;
var bool	bPressedJump;
var bool	bDoubleJump;
var EDoubleClickDir DoubleClickMove;	// Double click info.
var EPhysics SavedPhysics;
var vector StartLocation, StartRelativeLocation, StartVelocity, StartFloor, SavedLocation, SavedVelocity, SavedRelativeLocation;
var Actor StartBase, EndBase;

var float AccelDotThreshold;	// threshold for deciding this is an "important" move based on DP with last acked acceleration

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
}

function PostUpdate(PlayerController P)
{
	bDoubleJump = P.bDoubleJump || bDoubleJump;
	if ( P.Pawn != None )
	{
		SavedLocation = P.Pawn.Location;
		SavedVelocity = P.Pawn.Velocity;
		EndBase = P.Pawn.Base;
		if ( (EndBase != None) && !EndBase.bWorldGeometry )
			SavedRelativeLocation = P.Pawn.Location - EndBase.Location;
	}
	SetRotation(P.Rotation);
}

function bool IsImportantMove(vector CompareAccel)
{
	local vector AccelNorm;

	// check if any important movement flags set
	if ( bPressedJump || bDoubleJump 
		|| ((DoubleClickMove != DCLICK_None) && (DoubleClickMove != DCLICK_Active) && (DoubleClickMove != DCLICK_Done)) )
		return true;

	// check if acceleration has changed significantly
	AccelNorm = Normal(Acceleration);
	return ( (CompareAccel != AccelNorm) && ((CompareAccel Dot AccelNorm) < AccelDotThreshold) );
}

function vector GetStartLocation()
{
	if ( (Vehicle(StartBase) != None) ) //FIXMESTEVE || (Mover(StartBase) != None) )
		return StartBase.Location + StartRelativeLocation;
	
	return StartLocation;

}
function SetInitialPosition(Pawn P)
{
	SavedPhysics = P.Physics;
	StartLocation = P.Location;
	StartVelocity = P.Velocity;
	StartBase = P.Base;
	StartFloor = P.Floor;
	if ( (StartBase != None) && !StartBase.bWorldGeometry )
		StartRelativeLocation = P.Location - StartBase.Location;
}

function bool CanCombineWith(SavedMove NewMove, Pawn InPawn, float MaxDelta)
{
	// FIXMESTEVE - combine moves for spectators!!!
	// FIXMESTEVE - does combining really need to be limited to PHYS_Walking?
	// FIXMESTEVE - combine moves which share DCLICK_Active also?

	return ( (InPawn != None) && (InPawn.Physics == PHYS_Walking)
		&& (NewMove.Delta + Delta < MaxDelta)
		&& (NewMove.Acceleration != vect(0,0,0))
		&& (SavedPhysics == PHYS_Walking)
		&& !bPressedJump && !NewMove.bPressedJump
		&& (bRun == NewMove.bRun)
		&& (bDuck == NewMove.bDuck)
		&& (bDoubleJump == NewMove.bDoubleJump)
		&& (DoubleClickMove == DCLICK_None)
		&& (NewMove.DoubleClickMove == DCLICK_None)
		&& ((Normal(Acceleration) Dot Normal(NewMove.Acceleration)) > 0.99) );
}

function SetMoveFor(PlayerController P, float DeltaTime, vector NewAccel, EDoubleClickDir InDoubleClick)
{
	Delta = DeltaTime;
	if ( VSize(NewAccel) > 3072 )
		NewAccel = 3072 * Normal(NewAccel);
	if ( P.Pawn != None )
		SetInitialPosition(P.Pawn);
	Acceleration = NewAccel;
	DoubleClickMove = InDoubleClick;
	bRun = (P.bRun > 0);
	bDuck = (P.bDuck > 0);
	bPressedJump = P.bPressedJump;
	bDoubleJump = P.bDoubleJump;
	TimeStamp = Level.TimeSeconds;
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

		PC.bDoubleJump = ( (Flags & 64) != 0 );
		PC.bPressedJump = ( (Flags & 32) != 0 );
		if ( PC.bPressedJump )
			PC.bJumpStatus = true; // FIXMESTEVE
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

defaultproperties
{
     bHidden=True
	AccelDotThreshold=+0.9
}
