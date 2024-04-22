/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_CameraLookAt extends SequenceAction
	native(Sequence);

/** Should this affect the camera? */
var()	bool		bAffectCamera;
/** If FALSE, focus only if point roughly in view; if TRUE, focus no matter where player is looking */
var()	bool		bAlwaysFocus;
/** DEPRECATED.  Functionally replaced by bTurnInPlace for clarity in version 4. */
var	deprecated bool	bAdjustCamera;
/** If TRUE, camera will rotate in place.  If FALSE, camera can move as necessary. */
var()	bool		bTurnInPlace;
/** If TRUE, ignore world trace to find a good spot */
var()	bool		bIgnoreTrace;
/** Speed range of interpolation to focus camera */
var()	Vector2d	InterpSpeedRange;
/** How tight the focus should be */
var()	Vector2d	InFocusFOV;
/** Name of bone to focus on if available */
var() 	Name		FocusBoneName;
/** Should this turn the character's head? */
var()	bool		bAffectHead;
/** Should this turn affect the player's rotation? */
var()   bool        bRotatePlayerWithCamera;
/** Set this player in god mode?  Only works if bAffectCamera == TRUE */
var() 	bool		bToggleGodMode;
/** Leave the camera focused on the actor? */
var()	bool		bLeaveCameraRotation;
/** Text to display while camera is focused */
var()	String		TextDisplay;
/** Don't allow input */
var()	bool		bDisableInput;
/** The total amount of time the camera lookat will happen */
var()	float		TotalTime<ClampMin=0.0>;
/** Whether this event used a timer or not */
var		bool		bUsedTimer;
/** TRUE to validate visibility of lookat target before doing any camera changes */
var()	bool		bCheckLineOfSight;
/** If >= 0, override camera fov to be this. */
var()	float		CameraFOV<ClampMin=1.0 | ClampMax=179.0>;

var transient float RemainingTime;

cpptext
{
	void Activated();
	UBOOL UpdateOp(FLOAT DeltaTime);
	void DeActivated();

	void UpdateObject();
};

/**
 * Return the version number for this class.  Child classes should increment this method by calling Super then adding
 * a individual class version to the result.  When a class is first created, the number should be 0; each time one of the
 * link arrays is modified (VariableLinks, OutputLinks, InputLinks, etc.), the number that is added to the result of
 * Super.GetObjClassVersion() should be incremented by 1.
 *
 * @return	the version number for this specific class.
 */
static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 3;
}

defaultproperties
{
	ObjName="Look At"
	ObjCategory="Camera"

	bAffectCamera=TRUE
	bTurnInPlace=TRUE
	InterpSpeedRange=(X=3,Y=3)
	InFocusFOV=(X=1,Y=1)
	CameraFOV=-1.f

	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Focus")

	OutputLinks(0)=(LinkDesc="Out")				// always fires on activation
	OutputLinks(1)=(LinkDesc="Finished")		// fires after lookat finishes, if using timer (i.e. TotalTime > 0)
	OutputLinks(2)=(LinkDesc="Succeeded")		// fires if lookat actually happens
	OutputLinks(3)=(LinkDesc="Failed")			// fires if lookat fails (eg bCheckLineOfSight=true but trace fails)

	bDisableInput=TRUE
	TotalTime=0.0f
	bLatentExecution=TRUE
	bUsedTimer=FALSE
}
