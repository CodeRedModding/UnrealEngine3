class CameraModifier extends Object;

/* Interface class for all camera modifiers applied to 
	a player camera actor. Sub classes responsible for 
	implementing declared functions.

	Author: superville (08/03/04)
*/

/* Do not apply this modifier */
var	protected	bool	bDisabled;
/* This modifier is still being applied and will disable itself */
var				bool	bPendingDisable;


//debug
var(Debug) bool bDebug;

/** Allow anything to happen right after creation */
function Init();

/**
 * Directly modifies variables in the camera actor
 * 
 * @param	Camera - 
				reference to camera actor we are modifiying
 * @param	DeltaTime - 
				Change in time since last update
 * @param	out_CameraLocation - 
				input variable for current camera location
				output variable for new camera location
 * @param	out_CameraRotation - 
				input variable for current camera rotation
				output variable for new camera rotation
 * @return	bool - TRUE if should STOP looping the chain, FALSE otherwise
 */

function bool ModifyCamera
( 
		Camera	Camera, 
		float	DeltaTime,
	out Vector	out_CameraLocation, 
	out Rotator out_CameraRotation, 
	out float	out_FOV 
);

/** 
 * Camera modifier evaluates itself vs the given camera's modifier list
 *	and decides whether to add itself or not
 *
 * @parma	Camera - reference to camera actor we want add this modifier to
 * @output	BOOL   - TRUE if modifier added to camera's modifier list, FALSE otherwise
 */
function bool AddCameraModifier( Camera Camera ) 
{
	//debug
	if( bDebug )
	{
		Log( self@"AddModifier"@Camera.ModifierList.Length );
	}

	// Default implementation adds self and always returns true
	Camera.ModifierList[Camera.ModifierList.Length] = self;
	return true;
}

/**
 * Camera modifier removes itself from given camera's modifier list
 *
 * @param	Camera	- reference to camara actor we want to remove this modifier from
 * @output	BOOL	- TRUE if modifier removed successfully, FALSE otherwise
 */
function bool RemoveCameraModifier( Camera Camera )
{
	local int ModifierIdx;

	//debug
	if( bDebug )
	{
		Log( self@"RemoveModifier" );
	}

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

/** Accessor function to check if modifier is inactive */
simulated function bool IsDisabled()
{
	return bDisabled;
}

/** Accessor functions for changing disable flag */
function DisableModifier()
{
	//debug
	if( bDebug ) {
		Log( self@"DisableModifier" );
	}

	bDisabled = true;
	bPendingDisable = false;
}
function EnableModifier()
{
	//debug
	if( bDebug ) {
		Log( self@"EnableModifier" );
	}

	bDisabled = false;
	bPendingDisable = false;
}
function ToggleModifier()
{
	//debug
	if( bDebug ) {
		Log( self@"ToggleModifier" );
	}

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

defaultproperties
{
}