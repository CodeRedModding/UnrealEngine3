class WarCameraMode extends CameraMode
	config(Camera);

/**
 * Contains all the information needed to define a camera
 * mode.
 */

/** View point offset for high player view pitch */
var()	config		Vector	ViewOffsetHigh;

/** View point offset for medium (horizon) player view pitch */
var()	config		Vector	ViewOffsetMid;

/** View point offset for low player view pitch */
var()	config		Vector	ViewOffsetLow;

/** Pawn relative offset. It's relative from Pawn's location, aligned to Pawn's rotation */
var()	config		vector	PawnRelativeOffset;

/** Blended relative offset */
var		transient	vector	BlendedRelativeOffset;

/** FOV for camera to use */
var()	float	FOVAngle;

/** Blend Time to and from this view mode */
var()	float	BlendTime;

/** Called when Camera mode becomes active */
function OnBecomeActive( Camera CameraOwner );

/** Get Pawn's relative offset (from location based on pawn's rotation */
function vector GetPawnRelativeOffset( WarPawn P )
{
	return PawnRelativeOffset;
}

/**
 * Calculates and returns the ideal view offset for the specified camera mode.
 * The offset is relative to the player origin and calculated by interpolating 2 ideal view points based on the player view pitch.
 * 
 * @param	DeltaTime			Delta time since last frame.
 * @param	out_Offset			Ideal offset from Pawn's origin.
 * @param	out_ViewRotation
 */
simulated function GetCamModeViewOffset
(
		WarPawn		ViewedPawn,
		float		DeltaTime,
	out	Vector		out_Offset,
	out Rotator		out_ViewRotation
)
{
	local vector	Mid, Low, High;
	local float		Pct, Pitch;

	GetViewOffsets( Low, Mid, High );

	Pitch = FNormalizedRotAxis( out_ViewRotation.Pitch );
	if( Pitch >=0 )
	{
		Pct					= Pitch / ViewedPawn.default.ViewPitchMax;
		out_Offset	= VLerp( Pct, Mid, Low );
	}
	else
	{
		Pct					= Pitch / ViewedPawn.default.ViewPitchMin;
		out_Offset	= VLerp( Pct, Mid, High );
	}
}

/** returns camera mode desired FOV */
function float GetDesiredFOV( WarPawn ViewedPawn )
{
	return FOVAngle;
}

/** returns View relative offsets */
simulated function GetViewOffsets
( 
	out Vector	out_Low, 
	out Vector	out_Mid, 
	out Vector	out_High 
)
{
	out_Low		= ViewOffsetLow;
	out_Mid		= ViewOffsetMid;
	out_High	= ViewOffsetHigh;
}

defaultproperties
{
	BlendTime=0.67
}