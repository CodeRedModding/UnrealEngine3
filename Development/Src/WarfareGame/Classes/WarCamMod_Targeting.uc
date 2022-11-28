class WarCamMod_Targeting extends WarCameraModifier
	config(Camera);

/**
 * This class applies a small modifier for different targeting modes
 */

/* Offset to adjust camera for standing targeting */
var	config	Vector	TargetOffset;
/* Offset to adjust camera for crouching targeting */
var config	Vector	CrouchTargetOffset;
/* Offset to adjust camera for grenade targeting */
var config	Vector	GrenTargetOffset;
	
function bool ModifyCamera
( 
		Camera	Camera, 
		float	DeltaTime,
	out Vector	out_CameraLocation, 
	out Rotator out_CameraRotation, 
	out float	out_FOV 
)
{
	local WarPawn	Pawn;
	local Vector	Offset;
	local Weapon	ActiveWeapon;

	
	// If camera isn't viewing a pawn - exit
	// or currently at cover
	Pawn = WarPawn(Camera.primaryVT.Target);
	if ( Pawn == None || Pawn.CoverType != CT_None )
	{
		return false;
	}

	// Update the alpha
	UpdateAlpha( Camera, DeltaTime );

	// Call super where modifier may be disabled
	super.ModifyCamera( Camera, DeltaTime, out_CameraLocation, out_CameraRotation, out_FOV );
	
	// If no alpha, exit early
	if( Alpha <= 0.0 ) 
	{
		return false;
	}

	// Pick correct offset by weapon or posture
	ActiveWeapon = Pawn.Weapon;
	if( ActiveWeapon != None && 
		ActiveWeapon.IsA('Weap_Grenade') && 
		ActiveWeapon.IsInState('Ready') )
	{
		Offset = GrenTargetOffset;
	}

	if ( Pawn.bIsCrouched ) 
	{
		Offset = CrouchTargetOffset;
	}
	else
	{
		Offset = TargetOffset;
	}

	// Apply offset by alpha
	out_CameraLocation += Offset * Alpha;

	return false;
}

defaultproperties
{
	bDisabled=true

	Priority=4

	AlphaInTime=0.15
	AlphaOutTime=0.15

	TargetOffset=(X=88,Y=0,Z=0)
	CrouchTargetOffset=(X=100,Y=0,Z=-20)
	GrenTargetOffset=(X=88,Y=0,Z=-20)
}
