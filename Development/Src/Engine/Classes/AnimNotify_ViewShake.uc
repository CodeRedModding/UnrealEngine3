class AnimNotify_ViewShake extends AnimNotify_Scripted;

/** radius within which to shake player views */
var()	float	ShakeRadius;
/** Duration in seconds of shake */
var()	float	Duration;
/** view rotation amplitude (pitch,yaw,roll) */
var()	vector	RotAmplitude;
/** frequency of rotation shake */
var()	vector	RotFrequency;
/** relative view offset amplitude (x,y,z) */
var()	vector	LocAmplitude;
/** frequency of view offset shake */
var()	vector	LocFrequency;
/** fov shake amplitude */
var()	float	FOVAmplitude;
/** fov shake frequency */
var()	float	FOVFrequency;

/** Should use a bone location as shake's epicentre? */
var()	bool	bUseBoneLocation;
/** if so, bone name to use */
var()	name	BoneName;

event Notify( Actor Owner, AnimNodeSequence AnimSeqInstigator )
{
	local Controller	C;
	local float			Pct, DistToOrigin;
	local vector		ViewShakeOrigin, CamLoc;
	local rotator		CamRot;

	// Figure out world origin of view shake
	if( bUseBoneLocation && 
		AnimSeqInstigator != None &&
		AnimSeqInstigator.SkelComponent != None )
	{
		ViewShakeOrigin = AnimSeqInstigator.SkelComponent.GetBoneLocation( BoneName );
	}
	else
	{
		ViewShakeOrigin = Owner.Location;
	}

	// propagate to all player controllers
	for ( C=Owner.Level.ControllerList; C!=None; C=C.NextController )
	{
		if( C.IsA('PlayerController') )
		{
			C.GetPlayerViewPoint(CamLoc, CamRot);
			DistToOrigin = VSize(ViewShakeOrigin - CamLoc);
			if( DistToOrigin < ShakeRadius )		
			{
				Pct = 1.f - (DistToOrigin / ShakeRadius);
				PlayerController(C).CameraShake
				(
					Duration*Pct, 
					RotAmplitude*Pct, 
					RotFrequency*Pct, 
					LocAmplitude*Pct, 
					LocFrequency*Pct, 
					FOVAmplitude*Pct, 
					FOVFrequency*Pct
				);
			}
		}
	}
}

defaultproperties
{
	ShakeRadius=4096.0
	Duration=1.f
	RotAmplitude=(X=100,Y=100,Z=200)
	RotFrequency=(X=10,Y=10,Z=25)
	LocAmplitude=(X=0,Y=3,Z=6)
	LocFrequency=(X=1,Y=10,Z=20)
	FOVAmplitude=2
	FOVFrequency=5
}