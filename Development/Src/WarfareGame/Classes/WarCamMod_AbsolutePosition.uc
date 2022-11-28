class WarCamMod_AbsolutePosition extends WarCameraModifier
	config(Camera);

/**
 * This class sets camera location/rotation to an absolute world space coord while it's active
 * Pointer resides in WarPawn and trigger should call enable/disable on this modifier and set the 
 * location/rotation by hand
 */

/* World space camera location */
var Vector	CameraLocation;
/* World space camera rotation */
var Rotator CameraRotation;

function bool ModifyCamera
( 
		Camera	Camera, 
		float	DeltaTime,
	out Vector	out_CameraLocation, 
	out Rotator out_CameraRotation, 
	out float	out_FOV 
)
{
	// Update alpha
	UpdateAlpha( Camera, DeltaTime );

	// Call super where modifier may be disabled
	super.ModifyCamera( Camera, DeltaTime, out_CameraLocation, out_CameraRotation, out_FOV );

	// If alpha is zero, no need to continue
	if( Alpha <= 0.0 )
	{
		return false;
	}

	// Set location/rotation
	out_CameraLocation = VLerp( Alpha, out_CameraLocation, CameraLocation );
	out_CameraRotation = RLerp( Alpha, out_CameraRotation, CameraRotation );

	// Prevent any further changes
	return true;
}

defaultproperties
{
	bDisabled=true
}