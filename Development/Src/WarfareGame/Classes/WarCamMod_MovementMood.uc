class WarCamMod_MovementMood extends WarCameraModifier
	config(Camera);

/**
 * This class applies bobbing movement to the camera as the player moves
 */

/* Flags for special changes to each component */
const	MOODFLAG_NONE		= 0;
const	MOODFLAG_ALWAYS_POS	= 1;
const	MOODFLAG_ALWAYS_NEG = 2;

/* Amplitude applied to sin wave */
var	config		Vector	Amplitude;
/* Frequency of sin wave */
var config		Vector	Frequency;
/* Shift applied to sin wave */
var config		Vector	Shift;
/* Vector to offset camera location by */
var config		Vector	Offset;
/* Vector of flags for each component of sin function */
var config		Byte	Flags[3];
/* Min speed of pawn needed to apply this modifier */
var config		float	MinPawnSpeed;

/* Time along sin wave */
var transient	float	SinTime;

function bool ModifyCamera
( 
		Camera	Camera, 
		float	DeltaTime,
	out Vector	out_CameraLocation, 
	out Rotator out_CameraRotation, 
	out float	out_FOV 
)
{
	local Vector	SinValue;
	local WarPawn	Pawn;
	
	// If camera isn't viewing a pawn - exit
	Pawn = WarPawn(Camera.primaryVT.Target);
	if( Pawn == None )
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

	// Update our time along sin wave
	SinTime += DeltaTime;
	
	// Compute sin wave value for each component
	if( Amplitude.X != 0.0 ) 
	{
		SinValue.X = Amplitude.X * sin( (Frequency.X * SinTime) + Shift.X );
	}
	if( Amplitude.Y != 0.0 ) 
	{
		SinValue.Y = Amplitude.Y * sin( (Frequency.Y * SinTime) + Shift.Y );
	}
	if( Amplitude.Z != 0.0 ) 
	{
		SinValue.Z = Amplitude.Z * sin( (Frequency.Z * SinTime) + Shift.Z );
	}

	// Evaluate flags for each component
	if( Flags[0] > 0 ) 
	{
		if( (Flags[0] & MOODFLAG_ALWAYS_POS) != 0 ) 
		{
			SinValue.X =  Abs(SinValue.X);
		}
		if( (Flags[1] & MOODFLAG_ALWAYS_NEG) != 0 ) 
		{
			SinValue.X = -Abs(SinValue.X);
		}
	}
	if( Flags[1] > 0 ) 
	{
		if( (Flags[1] & MOODFLAG_ALWAYS_POS) != 0 ) 
		{
			SinValue.Y =  Abs(SinValue.Y);
		}
		if( (Flags[1] & MOODFLAG_ALWAYS_NEG) != 0 ) 
		{
			SinValue.Y = -Abs(SinValue.Y);
		}
	}
	if( Flags[2] > 0 ) 
	{
		if( (Flags[2] & MOODFLAG_ALWAYS_POS) != 0 ) 
		{
			SinValue.Z =  Abs(SinValue.Z);
		}
		if( (Flags[2] & MOODFLAG_ALWAYS_NEG) != 0 ) 
		{
			SinValue.Z = -Abs(SinValue.Z);
		}
	}

	// Update output camera location by sin value along offset for each axis
	out_CameraLocation += (SinValue * Offset * Alpha);

	return false;
}

function float GetTargetAlpha( Camera Camera )
{
	local WarPawn	Pawn;
	local float		Target;

	Target = super.GetTargetAlpha( Camera );

	Pawn = WarPawn(Camera.primaryVT.Target);
	if( Target != 0.0 &&
		Pawn != None )
	{
		// If moving too slow - zero alpha
		if ( Pawn.bIsCrouched	||
			Pawn.CoverType != CT_None			||
			VSizeSq(Pawn.Velocity) < MinPawnSpeed * MinPawnSpeed ) 
		{
			Target = 0.0;
		}
	}

	return Target;
}

function UpdateAlpha( Camera Camera, float DeltaTime )
{
	super.UpdateAlpha( Camera, DeltaTime );

	// If alpha has zero'd reset timer too
	if( Alpha == 0.0 ) 
	{
		SinTime = 0.0;
	}
}

defaultproperties
{
	Priority=5

	Amplitude=(X=5.0,Y=5.0,Z=5.0)
	Frequency=(X=10.0,Y=10.0,Z=10.0)
	Shift=(X=0.0,Y=0.0,Z=0.0)
	Offset=(X=0.0,Y=1.0,Z=0.5)
	AlphaInTime=0.0
	AlphaOutTime=0.25
	MinPawnSpeed=50.0
	Flags[0]=0
	Flags[1]=0
	Flags[2]=2
}