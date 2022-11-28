//=============================================================================
// PlayerInput
// Object within playercontroller that manages player input.
// only spawned on client
//=============================================================================

class PlayerInput extends Input within PlayerController
	config(Input)
	transient
	native;

var globalconfig	bool		bInvertMouse;

var					bool		bWasForward;							// used for doubleclick move
var					bool		bWasBack;
var					bool		bWasLeft;
var					bool		bWasRight;
var					bool		bEdgeForward;
var					bool		bEdgeBack;
var					bool		bEdgeLeft;
var					bool 		bEdgeRight;
var					bool		bAdjustSampling;

var globalconfig	float		MouseSensitivity;

var					float		DoubleClickTimer;						// max double click interval for double click move
var globalconfig	float		DoubleClickTime;

// Input axes.
var input			float		aBaseX;
var input			float		aBaseY;
var input			float		aBaseZ;
var input			float		aMouseX;
var input			float		aMouseY;
var input			float		aForward;
var input			float		aTurn;
var input			float		aStrafe;
var input			float		aUp;
var input			float		aLookUp;

// Input buttons.
var input			byte		bStrafe;
var input			byte		bTurn180;
var input			byte		bTurnToNearest;
var input			byte		bXAxis;
var input			byte		bYAxis;

/** Caches of the last values for aStrafe and aBaseY */
var transient float LastStrafe, LastBaseY;

//=============================================================================
// Input related functions.

function bool InvertLook();

/** Hook called from HUD actor. Gives access to HUD and Canvas */
function DrawHUD( HUD H );

// Postprocess the player's input.
event PlayerInput( float DeltaTime )
{
	local float FOVScale;

	ProcessInputMatching(DeltaTime);

	if (bMovementInputEnabled)
	{
		// Check for Double click movement.
		bEdgeForward	= (bWasForward	^^ (aBaseY	> 0));
		bEdgeBack		= (bWasBack		^^ (aBaseY	< 0));
		bEdgeLeft		= (bWasLeft		^^ (aStrafe < 0));
		bEdgeRight		= (bWasRight	^^ (aStrafe > 0));
		bWasForward		= (aBaseY	> 0);
		bWasBack		= (aBaseY	< 0);
		bWasLeft		= (aStrafe	< 0);
		bWasRight		= (aStrafe	> 0);

		// Apply mouse sensitivity.
		aMouseX			*= MouseSensitivity;
		aMouseY			*= MouseSensitivity;

		// Turning and strafing share the same axis.
		if( bStrafe > 0 )
			aStrafe		+= aBaseX + aMouseX;
		else
			aTurn		+= aBaseX + aMouseX;

		// Look up/down.
		if ( bInvertMouse )
			aLookUp		-= aMouseY;
		else
			aLookUp		+= aMouseY;

		// Forward/ backward movement
		aForward		+= aBaseY;

		// Take FOV into account (lower FOV == less sensitivity).	
		FOVScale		= GetFOVAngle() * 0.01111; // 0.01111 = 1 / 90.0
		aLookUp			*= FOVScale;
		aTurn			*= FOVScale;

		LastStrafe = aStrafe;
		LastBaseY = aBaseY;

		// Handle walking.
		HandleWalking();
	}
	else
	{
		aForward = 0.f;
		aStrafe = 0.f;
	}
	if (!bTurningInputEnabled)
	{
		aTurn = 0.f;
		aLookup = 0.f;
	}
}

// check for double click move
function Actor.EDoubleClickDir CheckForDoubleClickMove(float DeltaTime)
{
	local Actor.EDoubleClickDir DoubleClickMove, OldDoubleClick;

	if ( DoubleClickDir == DCLICK_Active )
		DoubleClickMove = DCLICK_Active;
	else
		DoubleClickMove = DCLICK_None;
	if (DoubleClickTime > 0.0)
	{
		if ( DoubleClickDir == DCLICK_Active )
		{
			if ( (Pawn != None) && (Pawn.Physics == PHYS_Walking) )
			{
				DoubleClickTimer = 0;
				DoubleClickDir = DCLICK_Done;
			}
		}
		else if ( DoubleClickDir != DCLICK_Done )
		{
			OldDoubleClick = DoubleClickDir;
			DoubleClickDir = DCLICK_None;

			if (bEdgeForward && bWasForward)
				DoubleClickDir = DCLICK_Forward;
			else if (bEdgeBack && bWasBack)
				DoubleClickDir = DCLICK_Back;
			else if (bEdgeLeft && bWasLeft)
				DoubleClickDir = DCLICK_Left;
			else if (bEdgeRight && bWasRight)
				DoubleClickDir = DCLICK_Right;

			if ( DoubleClickDir == DCLICK_None)
				DoubleClickDir = OldDoubleClick;
			else if ( DoubleClickDir != OldDoubleClick )
				DoubleClickTimer = DoubleClickTime + 0.5 * DeltaTime;
			else
				DoubleClickMove = DoubleClickDir;
		}

		if (DoubleClickDir == DCLICK_Done)
		{
			DoubleClickTimer = FMin(DoubleClickTimer-DeltaTime,0);
			if (DoubleClickTimer < -0.35)
			{
				DoubleClickDir = DCLICK_None;
				DoubleClickTimer = DoubleClickTime;
			}
		}		
		else if ((DoubleClickDir != DCLICK_None) && (DoubleClickDir != DCLICK_Active))
		{
			DoubleClickTimer -= DeltaTime;			
			if (DoubleClickTimer < 0)
			{
				DoubleClickDir = DCLICK_None;
				DoubleClickTimer = DoubleClickTime;
			}
		}
	}
	return DoubleClickMove;
}

/**
 * Iterates through all InputRequests on the PlayerController and
 * checks to see if a new input has been matched, or if the entire
 * match sequence should be reset.
 * 
 * @param	DeltaTime - time since last tick
 */
final function ProcessInputMatching(float DeltaTime)
{
	local float Value;
	local int i,MatchIdx;
	local bool bMatch;
	// iterate through each request,
	for (i = 0; i < InputRequests.Length; i++)
	{
		// if we have a valid match idx
		if (InputRequests[i].MatchActor != None &&
			InputRequests[i].MatchIdx >= 0 &&
			InputRequests[i].MatchIdx < InputRequests[i].Inputs.Length)
		{
			MatchIdx = InputRequests[i].MatchIdx;
			// if we've exceeded the delta,
			// ignore the delta for the first match
			if (MatchIdx != 0 &&
				Level.TimeSeconds - InputRequests[i].LastMatchTime >= InputRequests[i].Inputs[MatchIdx].TimeDelta)
			{
				// reset this match
				InputRequests[i].LastMatchTime = 0.f;
				InputRequests[i].MatchIdx = 0;

				// fire off the cancel event
				if (InputRequests[i].FailedFuncName != 'None')
				{
					InputRequests[i].MatchActor.SetTimer(0.01f, false, InputRequests[i].FailedFuncName );
				}
			}
			else
			{
				// grab the current input value of the matching type
				Value = 0.f;
				switch (InputRequests[i].Inputs[MatchIdx].Type)
				{
				case IT_XAxis:
					Value = aStrafe;
					break;
				case IT_YAxis:
					Value = aBaseY;
					break;
				}
				// check to see if this matches
				switch (InputRequests[i].Inputs[MatchIdx].Action)
				{
				case IMA_GreaterThan:
					bMatch = Value >= InputRequests[i].Inputs[MatchIdx].Value;
					break;
				case IMA_LessThan:
					bMatch = Value <= InputRequests[i].Inputs[MatchIdx].Value;
					break;
				}
				if (bMatch)
				{
					// mark it as matched
					InputRequests[i].LastMatchTime = Level.TimeSeconds;
					InputRequests[i].MatchIdx++;
					// check to see if we've matched all inputs
					if (InputRequests[i].MatchIdx >= InputRequests[i].Inputs.Length)
					{
						// fire off the event
						if (InputRequests[i].MatchFuncName != 'None')
						{
							InputRequests[i].MatchActor.SetTimer(0.01f,false,InputRequests[i].MatchFuncName);
						}
						// reset this match
						InputRequests[i].LastMatchTime = 0.f;
						InputRequests[i].MatchIdx = 0;
						// as well as all others
					}
				}
			}
		}
	}
}

defaultproperties
{
	bAdjustSampling=true
	MouseSensitivity=60.0
	DoubleClickTime=0.250000
}