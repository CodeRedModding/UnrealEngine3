//=============================================================================
// PlayerInput
// Object within playercontroller that manages player input.
// only spawned on client
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class PlayerInput extends Input within PlayerController
	config(Input)
	transient
	native(UserInterface);

cpptext
{
	/**
	 * Generates an IE_Released event for each key in the PressedKeys array, then clears the array.  Should be called when another
	 * interaction which swallows some (but perhaps not all) input is activated.
	 */
	virtual void FlushPressedKeys();

	/** Override to detect input from a gamepad */
	virtual UBOOL InputKey(INT ControllerId, FName Key, enum EInputEvent Event, FLOAT AmountDepressed = 1.f, UBOOL bGamepad = FALSE );
	virtual UBOOL InputAxis(INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad=FALSE);
	virtual UBOOL InputTouch(INT ControllerId, UINT Handle, ETouchType Type, FVector2D TouchLocation, DOUBLE DeviceTimestamp, UINT TouchpadIndex=0);
	virtual UBOOL InputMotion(INT ControllerId, const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration);
	virtual void  UpdateAxisValue( FLOAT* Axis, FLOAT Delta );
	virtual UBOOL IsGamepadKey(FName Name) const;
}

/** Player is giving input through a gamepad */
var	const bool	bUsingGamepad;
var const Name	LastAxisKeyName;

var globalconfig	bool		bInvertMouse;							/** if true, mouse y axis is inverted from normal FPS mode */
var globalconfig	bool		bInvertTurn;							/** if true, mouse x axis is inverted from normal FPS mode */

// Double click move flags
var					bool		bWasForward;
var					bool		bWasBack;
var					bool		bWasLeft;
var					bool		bWasRight;
var					bool		bEdgeForward;
var					bool		bEdgeBack;
var					bool		bEdgeLeft;
var					bool 		bEdgeRight;

var					float		DoubleClickTimer;						/** max double click interval for double click move */
var globalconfig	float		DoubleClickTime;						/** stores time of first click for potential double click */

var globalconfig	float		MouseSensitivity;

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

// analog trigger axes
var input			float		aRightAnalogTrigger;
var input			float		aLeftAnalogTrigger;

// PS3 SIXAXIS axes
var input			float		aPS3AccelX;
var input			float		aPS3AccelY;
var input			float		aPS3AccelZ;
var input			float		aPS3Gyro;

// WiiU IR Pointer
var input			float		aWiiUPointerX;
var input			float		aWiiUPointerY;


// device tilt - NOTE: These are set directly, they DO NOT go through the .ini Bindings
var	input			vector		aTilt;
var	input			vector		aRotationRate;
var	input			vector		aGravity;
var input			vector		aAcceleration;
// touch locations, the Z component is > 0 if the touch is currently held down
var	input			vector		aTouch[5];
var	input			vector		aBackTouch[5];

//
// Joy Raw Input
//
/** Joypad left thumbstick, vertical axis. Range [-1,+1] */
var		transient	float	RawJoyUp;
/** Joypad left thumbstick, horizontal axis. Range [-1,+1] */
var		transient	float	RawJoyRight;
/** Joypad right thumbstick, horizontal axis. Range [-1,+1] */
var		transient	float	RawJoyLookRight;
/** Joypad right thumbstick, vertical axis. Range [-1,+1] */
var		transient	float	RawJoyLookUp;

/** move forward speed scaling */
var()	config		float	MoveForwardSpeed;
/** strafe speed scaling */
var()	config		float	MoveStrafeSpeed;
/** Yaw turn speed scaling */
var()	config		float	LookRightScale;
/** pitch turn speed scaling */
var()	config		float	LookUpScale;


// Input buttons.
var input			byte		bStrafe;
var input			byte		bXAxis;
var input			byte		bYAxis;

// Mouse smoothing control
var globalconfig bool		bEnableMouseSmoothing;			/** if true, mouse smoothing is enabled */

// Zoom Scaling
var bool bEnableFOVScaling;

// Mouse smoothing sample data
var float ZeroTime[2];							/** How long received mouse movement has been zero. */
var float SmoothedMouse[2];						/** Current average mouse movement/sample */
var int MouseSamples;							/** Number of mouse samples since mouse movement has been zero */
var float  MouseSamplingTotal;					/** DirectInput's mouse sampling total time */

/** If TRUE turn input will be ignored until the stick is released */
var transient bool bLockTurnUntilRelease;
/** Time remaining to disable bLockTurnUntilRelease */
var transient float AutoUnlockTurnTime;

/** 
 * Function to cancel all mobile input before a Scaleform movie confuses the state
 */
native function CancelMobileInput();

//=============================================================================
// Input related functions.

exec function bool InvertMouse()
{
	bInvertMouse = !bInvertMouse;
	SaveConfig();
	return bInvertMouse;
}

exec function bool InvertTurn()
{
	bInvertTurn = !bInvertTurn;
	SaveConfig();
	return bInvertTurn;
}

exec function SetSensitivity(Float F)
{
	MouseSensitivity = F;
}

/** Hook called from HUD actor. Gives access to HUD and Canvas */
function DrawHUD( HUD H );

function PreProcessInput(float DeltaTime);
function PostProcessInput(float DeltaTime);

function AdjustMouseSensitivity(float FOVScale)
{
	// Apply mouse sensitivity.
	aMouseX			*= MouseSensitivity * FOVScale;
	aMouseY			*= MouseSensitivity * FOVScale;
}

// Postprocess the player's input.
event PlayerInput( float DeltaTime )
{
	local float FOVScale, TimeScale;

	// Save Raw values
	RawJoyUp		= aBaseY;
	RawJoyRight		= aStrafe;
	RawJoyLookRight	= aTurn;
	RawJoyLookUp	= aLookUp;

	// PlayerInput shouldn't take timedilation into account
	DeltaTime /= WorldInfo.TimeDilation;
	if (Outer.bDemoOwner && WorldInfo.NetMode == NM_Client)
	{
		DeltaTime /= WorldInfo.DemoPlayTimeDilation;
	}

	PreProcessInput( DeltaTime );

	// Scale to game speed
	TimeScale = 100.f*DeltaTime;
	aBaseY		*= TimeScale * MoveForwardSpeed;
	aStrafe		*= TimeScale * MoveStrafeSpeed;
	aUp			*= TimeScale * MoveStrafeSpeed;
	aTurn		*= TimeScale * LookRightScale;
	aLookUp		*= TimeScale * LookUpScale;

	PostProcessInput( DeltaTime );

	ProcessInputMatching(DeltaTime);

	// Check for Double click movement.
	CatchDoubleClickInput();

	// Take FOV into account (lower FOV == less sensitivity).

	if ( bEnableFOVScaling )
	{
		FOVScale = GetFOVAngle() * 0.01111; // 0.01111 = 1 / 90.0
	}
	else
	{
		FOVScale = 1.0;
	}

	AdjustMouseSensitivity(FOVScale);

	// mouse smoothing
	if ( bEnableMouseSmoothing )
	{
		aMouseX = SmoothMouse(aMouseX, DeltaTime,bXAxis,0);
		aMouseY = SmoothMouse(aMouseY, DeltaTime,bYAxis,1);
	}

	aLookUp			*= FOVScale;
	aTurn			*= FOVScale;

	// Turning and strafing share the same axis.
	if( bStrafe > 0 )
		aStrafe		+= aBaseX + aMouseX;
	else
		aTurn		+= aBaseX + aMouseX;

	// Look up/down.
	aLookup += aMouseY;
	if (bInvertMouse)
	{
		aLookup *= -1.f;
	}

	if (bInvertTurn)
	{
		aTurn *= -1.f;
	}

	// Forward/ backward movement
	aForward		+= aBaseY;

	// Handle walking.
	HandleWalking();

	// check for turn locking
	if (bLockTurnUntilRelease)
	{
		if (RawJoyLookRight != 0)
		{
			aTurn = 0.f;
			if (AutoUnlockTurnTime > 0.f)
			{
				AutoUnlockTurnTime -= DeltaTime;
				if (AutoUnlockTurnTime < 0.f)
				{
					bLockTurnUntilRelease = FALSE;
				}
			}
		}
		else
		{
			bLockTurnUntilRelease = FALSE;
		}
	}

	// ignore move input
	// Do not clear RawJoy flags, as we still want to be able to read input.
	if( IsMoveInputIgnored() )
	{
		aForward	= 0.f;
		aStrafe		= 0.f;
		aUp			= 0.f;
	}

	// ignore look input
	// Do not clear RawJoy flags, as we still want to be able to read input.
	if( IsLookInputIgnored() )
	{
		aTurn		= 0.f;
		aLookup		= 0.f;
	}
}

function CatchDoubleClickInput()
{
	if (!IsMoveInputIgnored())
	{
		bEdgeForward	= (bWasForward	^^ (aBaseY	> 0));
		bEdgeBack		= (bWasBack		^^ (aBaseY	< 0));
		bEdgeLeft		= (bWasLeft		^^ (aStrafe < 0));
		bEdgeRight		= (bWasRight	^^ (aStrafe > 0));
		bWasForward		= (aBaseY	> 0);
		bWasBack		= (aBaseY	< 0);
		bWasLeft		= (aStrafe	< 0);
		bWasRight		= (aStrafe	> 0);
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
		if (InputRequests[i].MatchIdx >= 0 &&
			InputRequests[i].MatchIdx < InputRequests[i].Inputs.Length)
		{
			if (InputRequests[i].MatchActor == None)
			{
				InputRequests[i].MatchActor = Outer;
			}
			MatchIdx = InputRequests[i].MatchIdx;
			// if we've exceeded the delta,
			// ignore the delta for the first match
			if (MatchIdx != 0 &&
				InputRequests[i].Inputs[MatchIdx].TimeDelta > 0.f && 
				WorldInfo.TimeSeconds - InputRequests[i].LastMatchTime >= InputRequests[i].Inputs[MatchIdx].TimeDelta)
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
					InputRequests[i].LastMatchTime = WorldInfo.TimeSeconds;
					InputRequests[i].MatchIdx++;
					// check to see if we've matched all inputs
					if (InputRequests[i].MatchIdx >= InputRequests[i].Inputs.Length)
					{
						if (InputRequests[i].MatchDelegate != None)
						{
							InputMatchDelegate = InputRequests[i].MatchDelegate;
							InputMatchDelegate();
						}
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

//*************************************************************************************
// Normal gameplay execs
// Type the name of the exec function at the console to execute it

exec function Jump()
{
	if ( WorldInfo.Pauser == PlayerReplicationInfo )
		SetPause( False );
	else
		bPressedJump = true;
}

exec function SmartJump()
{
	Jump();
}

//*************************************************************************************
// Mouse smoothing

exec function ClearSmoothing()
{
	local int i;

	for ( i=0; i<2; i++ )
	{
		//`Log(i$" zerotime "$zerotime[i]$" smoothedmouse "$SmoothedMouse[i]);
		ZeroTime[i] = 0;
		SmoothedMouse[i] = 0;
	}
	//`Log("MouseSamplingTotal "$MouseSamplingTotal$" MouseSamples "$MouseSamples);
    	MouseSamplingTotal = Default.MouseSamplingTotal;
	MouseSamples = Default.MouseSamples;
}

/** SmoothMouse()
Smooth mouse movement, because mouse sampling doesn't match up with tick time.
 * @note: if we got sample event for zero mouse samples (so we
			didn't have to guess whether a 0 was caused by no sample occuring during the tick (at high frame rates) or because the mouse actually stopped)
 * @param: aMouse is the mouse axis movement received from DirectInput
 * @param: DeltaTime is the tick time
 * @param: SampleCount is the number of mouse samples received from DirectInput
 * @param: Index is 0 for X axis, 1 for Y axis
 * @return the smoothed mouse axis movement
 */
function float SmoothMouse(float aMouse, float DeltaTime, out byte SampleCount, int Index)
{
	local float MouseSamplingTime;

	if (DeltaTime < 0.25)
	{
		MouseSamplingTime = MouseSamplingTotal/MouseSamples;

		if ( aMouse == 0 )
		{
			// no mouse movement received
			ZeroTime[Index] += DeltaTime;
			if ( ZeroTime[Index] < MouseSamplingTime )
			{
				// zero mouse movement is possibly because less than the mouse sampling interval has passed
				aMouse = SmoothedMouse[Index] * DeltaTime/MouseSamplingTime;
			}
			else
			{
				SmoothedMouse[Index] = 0;
			}
		}
		else
		{
			ZeroTime[Index] = 0;
			if ( SmoothedMouse[Index] != 0 )
			{
				// this isn't the first tick with non-zero mouse movement
				if ( DeltaTime < MouseSamplingTime * (SampleCount + 1) )
				{
					// smooth mouse movement so samples/tick is constant
					aMouse = aMouse * DeltaTime/(MouseSamplingTime * SampleCount);
				}
				else
				{
					// fewer samples, so going slow
					// use number of samples we should have had for sample count
					SampleCount = DeltaTime/MouseSamplingTime;
				}
			}
			SmoothedMouse[Index] = aMouse/SampleCount;
		}
	}
	else
	{
		// if we had an abnormally long frame, clear everything so it doesn't distort the results
		ClearSmoothing();
	}
	SampleCount = 0;
	return aMouse;
}

/**
 * The player controller will call this function directly after creating the input system
 */
native function InitInputSystem();

/**
 * Called via replication to tell the Client to Init it's input system                                                                     
 */
function ClientInitInputSystem();

/**
 * The player controll will call this function directly before traveling                                                                     
 */

function PreClientTravel( string PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
}

defaultproperties
{
    MouseSamplingTotal=+0.0083
	MouseSamples=1
}

