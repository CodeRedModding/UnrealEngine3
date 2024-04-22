/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CameraModifier extends Object
	native(Camera);

/* Interface class for all camera modifiers applied to
	a player camera actor. Sub classes responsible for
	implementing declared functions.
*/

/* Do not apply this modifier */
var	protected	bool	bDisabled;
/* This modifier is still being applied and will disable itself */
var				bool	bPendingDisable;

/** Camera this object is attached to */
var Camera	CameraOwner;

/**
 * Priority of this modifier - determines where it is added in the modifier list.
 * 0 = highest priority, 255 = lowest 
 */
var	protected byte Priority;
/** This modifier can only be used exclusively - no modifiers of same priority allowed */
var protected bool bExclusive;
/** When blending in, alpha proceeds from 0 to 1 over this time */
var protected float AlphaInTime;
/** When blending out, alpha proceeds from 1 to 0 over this time */
var protected float AlphaOutTime;

/** Current blend alpha */
var protected transient float Alpha;
/** Desired alpha we are interpolating towards. */
var protected transient float TargetAlpha;


//debug
var(Debug) bool bDebug;

cpptext
{
protected:
	virtual FLOAT GetTargetAlpha(class ACamera* Camera);
public:
	virtual void GetDebugText(TArray<FString>& Lines) {};
};

/** Allow anything to happen right after creation */
function Init();


/**
 * Directly modifies variables in the camera actor
 *
 * @param	Camera		reference to camera actor we are modifying
 * @param	DeltaTime	Change in time since last update
 * @param	OutPOV		current Point of View, to be updated.
 * @return	bool		TRUE if should STOP looping the chain, FALSE otherwise
 */
native function bool ModifyCamera
(
		Camera	Camera,
		float	DeltaTime,
	out TPOV	OutPOV
);

/** Accessor function to check if modifier is inactive */
native function bool IsDisabled() const;

/**
 * Camera modifier evaluates itself vs the given camera's modifier list
 * and decides whether to add itself or not. Handles adding by priority and avoiding 
 * adding the same modifier twice.
 *
 * @param	Camera - reference to camera actor we want add this modifier to
 * @return	bool   - TRUE if modifier added to camera's modifier list, FALSE otherwise
 */
function bool AddCameraModifier( Camera Camera )
{
	local int BestIdx, ModifierIdx;
	local CameraModifier Modifier;

	// Make sure we don't already have this modifier in the list
	for( ModifierIdx = 0; ModifierIdx < Camera.ModifierList.Length; ModifierIdx++ )
	{
		if ( Camera.ModifierList[ModifierIdx] == Self )
		{
			return false;
		}
	}

	// Make sure we don't already have a modifier of this type
	for( ModifierIdx = 0; ModifierIdx < Camera.ModifierList.Length; ModifierIdx++ )
	{
		if ( Camera.ModifierList[ModifierIdx].Class == Class )
		{
			`log("AddCameraModifier found existing modifier in list, replacing with new one" @ self);
			
			// hack replace old by new (delete??)
			Camera.ModifierList[ModifierIdx] = Self;
			
			// Save camera
			CameraOwner = Camera;
			
			return true;
		}
	}

	// Look through current modifier list and find slot for this priority
	BestIdx = 0;

	for( ModifierIdx = 0; ModifierIdx < Camera.ModifierList.Length; ModifierIdx++ )
	{
		Modifier = Camera.ModifierList[ModifierIdx];
		if( Modifier == None ) {
			continue;
		}

		// If priority of current index has passed or equaled ours - we have the insert location
		if( Priority <= Modifier.Priority )
		{
			// Disallow addition of exclusive modifier if priority is already occupied
			if( bExclusive && Priority == Modifier.Priority )
			{
				return false;
			}

			break;
		}

		// Update best index
		BestIdx++;
	}

	// Insert self into best index
	Camera.ModifierList.Insert( BestIdx, 1 );
	Camera.ModifierList[BestIdx] = self;

	// Save camera
	CameraOwner = Camera;

`if(`notdefined(FINAL_RELEASE))
	//debug
	if( bDebug )
	{
		`log( "AddModifier"@BestIdx@self );
		for( ModifierIdx = 0; ModifierIdx < Camera.ModifierList.Length; ModifierIdx++ )
		{
			`log( Camera.ModifierList[ModifierIdx]@"Idx"@ModifierIdx@"Pri"@Camera.ModifierList[ModifierIdx].Priority);
		}
		`log( "****************" );
	}
`endif

	return true;
}


/**
 * Camera modifier removes itself from given camera's modifier list
 *
 * @param	Camera	- reference to camara actor we want to remove this modifier from
 * @return	bool	- TRUE if modifier removed successfully, FALSE otherwise
 */
function bool RemoveCameraModifier( Camera Camera )
{
	local int ModifierIdx;

	//debug
	`Log( self@"RemoveModifier", bDebug );

	// Loop through each modifier in camera
	for( ModifierIdx = 0; ModifierIdx < Camera.ModifierList.Length; ModifierIdx++ )
	{
		// If we found ourselves, remove ourselves from the list and return
		if( Camera.ModifierList[ModifierIdx] == self )
		{
			Camera.ModifierList.Remove(ModifierIdx, 1);
			return true;
		}
	}

	// Didn't find ourselves in the list
	return false;
}

/** 
 *  Accessor functions for changing disable flag
 *  
 *  @param  bImmediate  - TRUE to disable with no blend out, FALSE (default) to allow blend out
 *  */
event DisableModifier(optional bool bImmediate)
{
	//debug
	`Log( self@"DisableModifier"@bImmediate, bDebug );

	if (bImmediate)
	{
		bDisabled = true;
		bPendingDisable = false;
	}
	else if (!bDisabled)
	{
		bPendingDisable = true;
	}

}
function EnableModifier()
{
	//debug
	`Log( self@"EnableModifier", bDebug );

	bDisabled = false;
	bPendingDisable = false;
}
function ToggleModifier()
{
	//debug
	`Log( self@"ToggleModifier", bDebug );

	if( bDisabled )
	{
		EnableModifier();
	}
	else
	{
		DisableModifier();
	}
}
/**
 * Allow this modifier a chance to change view rotation and deltarot
 * Default just returns ViewRotation unchanged
 * @return	bool - TRUE if should stop looping modifiers to adjust rotation, FALSE otherwise
 */
simulated function bool ProcessViewRotation( Actor ViewTarget, float DeltaTime, out Rotator out_ViewRotation, out Rotator out_DeltaRot );

/**
 * Responsible for updating alpha blend value.
 *
 * @param	Camera		- Camera that is being updated
 * @param	DeltaTime	- Amount of time since last update
 */
native function UpdateAlpha( Camera Camera, float DeltaTime );


defaultproperties
{
	Priority=127
}
