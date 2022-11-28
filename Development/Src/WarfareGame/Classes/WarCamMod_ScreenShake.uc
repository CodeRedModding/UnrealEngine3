class WarCamMod_ScreenShake extends WarCameraModifier
	config(Camera);

/** 
 * WarCamMod_ScreenShake
 * Screen Shake Camera modifier
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

struct ScreenShakeStruct
{
	/** Time in seconds to go until current screen shake is finished */
	var()	float	TimeToGo;
	/** Duration in seconds of current screen shake */
	var()	float	TimeDuration;

	/** view rotation amplitude */
	var()	vector	RotAmplitude;
	/** view rotation frequency */
	var()	vector	RotFrequency;
	/** view rotation Sine offset */
	var		vector	RotSinOffset;

	/** view offset amplitude */
	var()	vector	LocAmplitude;
	/** view offset frequency */
	var()	vector	LocFrequency;
	/** view offset Sine offset */
	var		vector	LocSinOffset;

	/** FOV amplitude */
	var()	float	FOVAmplitude;
	/** FOV frequency */
	var()	float	FOVFrequency;
	/** FOV Sine offset */
	var		float	FOVSinOffset;

	structdefaults
	{
		TimeDuration=1.f
		RotAmplitude=(X=100,Y=100,Z=200)
		RotFrequency=(X=10,Y=10,Z=25)
		LocAmplitude=(X=0,Y=3,Z=5)
		LocFrequency=(X=1,Y=10,Z=20)
		FOVAmplitude=2
		FOVFrequency=5
	}
};

/** Active ScreenShakes array */
var		Array<ScreenShakeStruct>	Shakes;

/** Always active ScreenShake for testing purposes */
var()	ScreenShakeStruct			TestShake;

/** Add a new screen shake to the list */
final function AddScreenShake( ScreenShakeStruct NewShake )
{
	Shakes[Shakes.Length] = NewShake;
}

/**
 * ComposeNewShake
 * Take Screen Shake parameters and create a new ScreenShakeStruct variable
 *
 * @param	Duration			Duration in seconds of shake
 * @param	newRotAmplitude		view rotation amplitude (pitch,yaw,roll)
 * @param	newRotFrequency		frequency of rotation shake
 * @param	newLocAmplitude		relative view offset amplitude (x,y,z)
 * @param	newLocFrequency		frequency of view offset shake
 * @param	newFOVAmplitude		fov shake amplitude
 * @param	newFOVFrequency		fov shake frequency
 */
final function ScreenShakeStruct ComposeNewShake
(
	float	Duration, 
	vector	newRotAmplitude, 
	vector	newRotFrequency,
	vector	newLocAmplitude, 
	vector	newLocFrequency,
	float	newFOVAmplitude,
	float	newFOVFrequency
)
{
	local ScreenShakeStruct	NewShake;

	NewShake.TimeDuration		= Duration;
	NewShake.TimeToGo			= Duration;

	NewShake.RotAmplitude		= newRotAmplitude;
	NewShake.RotFrequency		= newRotFrequency;
	NewShake.RotSinOffset.X		= FRand() * 2*Pi;
	NewShake.RotSinOffset.Y		= FRand() * 2*Pi;
	NewShake.RotSinOffset.Z		= FRand() * 2*Pi;

	NewShake.LocAmplitude		= newLocAmplitude;
	NewShake.LocFrequency		= newLocFrequency;
	NewShake.LocSinOffset.X		= FRand() * 2*Pi;
	NewShake.LocSinOffset.Y		= FRand() * 2*Pi;
	NewShake.LocSinOffset.Z		= FRand() * 2*Pi;

	NewShake.FOVAmplitude		= newFOVAmplitude;
	NewShake.FOVFrequency		= newFOVFrequency;
	NewShake.FOVSinOffset		= FRand() * 2*Pi;

	return NewShake;
}

/**
 * StartNewShake
 *
 * @param	Duration			Duration in seconds of shake
 * @param	newRotAmplitude		view rotation amplitude (pitch,yaw,roll)
 * @param	newRotFrequency		frequency of rotation shake
 * @param	newLocAmplitude		relative view offset amplitude (x,y,z)
 * @param	newLocFrequency		frequency of view offset shake
 * @param	newFOVAmplitude		fov shake amplitude
 * @param	newFOVFrequency		fov shake frequency
 */
function StartNewShake
( 
	float	Duration, 
	vector	newRotAmplitude, 
	vector	newRotFrequency,
	vector	newLocAmplitude, 
	vector	newLocFrequency,
	float	newFOVAmplitude,
	float	newFOVFrequency
)
{
	local ScreenShakeStruct	NewShake;

	NewShake = ComposeNewShake
	( 
		Duration, 
		newRotAmplitude, 
		newRotFrequency,
		newLocAmplitude, 
		newLocFrequency,
		newFOVAmplitude,
		newFOVFrequency
	);

	AddScreenShake( NewShake );
}

/** Update a ScreenShake */
function UpdateScreenShake
( 
		float				DeltaTime, 
	out	ScreenShakeStruct	Shake,
		float				GlobalScale,
	out Vector				out_CamLoc, 
	out Rotator				out_CamRot, 
	out float				out_FOV 
)
{
	local	float	FOVOffset, ShakePct;
	local	vector	LocOffset;
	local	vector	RotOffset;

	Shake.TimeToGo -= DeltaTime;

	// Do not update screen shake if not needed
	if( Shake.TimeToGo <= 0.f )
	{
		return;
	}

	// Smooth fade out
	ShakePct = FClamp(Shake.TimeToGo / Shake.TimeDuration, 0.f, 1.f);
	ShakePct = GlobalScale * Alpha * ShakePct*ShakePct*(3.f - 2.f*ShakePct);

	// do not update if percentage is null
	if( ShakePct == 0 )
	{
		return;
	}

	// View Rotation, compute sin wave value for each component
	if( !IsZero(Shake.RotAmplitude) )
	{
		if( Shake.RotAmplitude.X != 0.0 ) 
		{
			Shake.RotSinOffset.X += DeltaTime * Shake.RotFrequency.X * ShakePct;
			RotOffset.X = Shake.RotAmplitude.X * Sin( Shake.RotSinOffset.X );
		}
		if( Shake.RotAmplitude.Y != 0.0 ) 
		{
			Shake.RotSinOffset.Y += DeltaTime * Shake.RotFrequency.Y * ShakePct;
			RotOffset.Y = Shake.RotAmplitude.Y * Sin( Shake.RotSinOffset.Y );
		}
		if( Shake.RotAmplitude.Z != 0.0 ) 
		{
			Shake.RotSinOffset.Z += DeltaTime * Shake.RotFrequency.Z * ShakePct;
			RotOffset.Z = Shake.RotAmplitude.Z * Sin( Shake.RotSinOffset.Z );
		}
		RotOffset			*= ShakePct;
		out_CamRot.Pitch	+= RotOffset.X;
		out_CamRot.Yaw		+= RotOffset.Y;
		out_CamRot.Roll		+= RotOffset.Z;
	}

	// View Offset, Compute sin wave value for each component
	if( !IsZero(Shake.LocAmplitude) )
	{
		if( Shake.LocAmplitude.X != 0.0 ) 
		{
			Shake.LocSinOffset.X += DeltaTime * Shake.LocFrequency.X * ShakePct;
			LocOffset.X = Shake.LocAmplitude.X * Sin( Shake.LocSinOffset.X );
		}
		if( Shake.LocAmplitude.Y != 0.0 ) 
		{
			Shake.LocSinOffset.Y += DeltaTime * Shake.LocFrequency.Y * ShakePct;
			LocOffset.Y = Shake.LocAmplitude.Y * Sin( Shake.LocSinOffset.Y );
		}
		if( Shake.LocAmplitude.Z != 0.0 ) 
		{
			Shake.LocSinOffset.Z += DeltaTime * Shake.LocFrequency.Z * ShakePct;
			LocOffset.Z = Shake.LocAmplitude.Z * Sin( Shake.LocSinOffset.Z );
		}
		out_CamLoc += LocOffset * ShakePct;
	}

	// Compute FOV change
	if( Shake.FOVAmplitude != 0.0 ) 
	{
		Shake.FOVSinOffset += DeltaTime * Shake.FOVFrequency * ShakePct;
		FOVOffset = ShakePct * Shake.FOVAmplitude * Sin( Shake.FOVSinOffset );
		out_FOV += FOVOffset;
	}
}

/** @see CameraModifer::ModifyCamera */
function bool ModifyCamera
( 
		Camera	Camera, 
		float	DeltaTime,
	out Vector	out_CameraLocation, 
	out Rotator out_CameraRotation, 
	out float	out_FOV 
)
{
	local int					i;
	local AmbientShakeVolume	ShakeVolume;

	// Update the alpha
	UpdateAlpha( Camera, DeltaTime );

	// Call super where modifier may be disabled
	super.ModifyCamera( Camera, DeltaTime, out_CameraLocation, out_CameraRotation, out_FOV );

	// If no alpha, exit early
	if( Alpha <= 0.f ) 
	{
		return false;
	}

	// Update Screen Shakes array
	if( Shakes.Length > 0 )
	{
		for(i=0; i<Shakes.Length; i++)
		{
			UpdateScreenShake( DeltaTime, Shakes[i], 1.f, out_CameraLocation, out_CameraRotation, out_FOV );
		}

		// Delete any obsolete shakes
		for(i=Shakes.Length-1; i>=0; i--)
		{
			if( Shakes[i].TimeToGo <= 0 )
			{
				Shakes.Remove(i,1);
			}
		}
	}
	// Update Test Shake
	UpdateScreenShake( DeltaTime, TestShake, 1.f, out_CameraLocation, out_CameraRotation, out_FOV );

	// Update Ambient Shake Volumes
	ForEach Camera.primaryVT.Target.TouchingActors(class'AmbientShakeVolume', ShakeVolume)
	{
		if( ShakeVolume.bEnableShake )
		{
			// Set TimeToGo, so it ends up scaling shake by 1x
			ShakeVolume.AmbientShake.TimeToGo = ShakeVolume.AmbientShake.TimeDuration + DeltaTime;
			// Update ambient shake
			UpdateScreenShake( DeltaTime, ShakeVolume.AmbientShake, 1.f, out_CameraLocation, out_CameraRotation, out_FOV );
		}
	}
}

defaultproperties
{
}