/** 
 * Emit_FragGrenadeExplosion
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Emit_FragGrenadeExplosion extends Emitter
	config(Weapon);

/** Bullet Impact Sound */
var()	SoundCue				ExplosionSound;
/** transform component for light */
var()	TransformComponent		PointLightTransform;
/** Dynamic Light */
var()	PointLightComponent		ExploLight;
/** Dynamic Light fade out time */
var()	float					LightFadeOutTime;

function simulated PostBeginPlay()
{
	super.PostBeginPlay();

	if( Level.NetMode != NM_DedicatedServer )
	{
		SetTimer( LightFadeOutTime );
		SetTemplate( ParticleSystem'WarFare_Effects.Grandade_Explo', true );
		PlaySound( ExplosionSound );
	}
}

simulated function Tick( float DeltaTime )
{
	local float TimerCount, pct;

	if( Level.NetMode == NM_DedicatedServer )
	{
		ExploLight.bEnabled = false;
		Disable('Tick');
	}

	TimerCount = GetTimerCount();
	if( TimerCount >= 0 )
	{
		pct = (LightFadeOutTime-TimerCount) / LightFadeOutTime;
		ExploLight.Brightness = ExploLight.default.Brightness * pct * pct; 
	}
	else
	{
		ExploLight.bEnabled = false;
	}
	//log("TimerCount:" @ TimerCount @ "pct:" @ pct @ "ExploLight.Brightness:" @ ExploLight.Brightness );
}

defaultproperties
{
	LightFadeOutTime=0.33
	bNetInitialRotation=true
	ExplosionSound=SoundCue'LD_TempUTSounds.BExplosion3'

	//point light
    Begin Object Class=PointLightComponent Name=PointLightComponent0
        Radius=800.000000
        Brightness=1000.000000
        Color=(B=60,G=107,R=249,A=255)
    End Object
	ExploLight=PointLightComponent0

    Begin Object Class=TransformComponent Name=PointLightTransformComponent0
    	TransformedComponent=PointLightComponent0
		Translation=(X=16)
    End Object
	PointLightTransform=PointLightTransformComponent0
	Components.Add(PointLightTransformComponent0)
}