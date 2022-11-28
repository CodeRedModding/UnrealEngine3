/** 
 * Bullet Impact Emitter
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Emit_BulletImpact extends Emitter;

/** Bullet Impact Sound */
var()	SoundCue	BulletImpactSound;

function simulated PostBeginPlay()
{
	super.PostBeginPlay();

	if( Level.NetMode != NM_DedicatedServer )
	{
		SetTemplate( ParticleSystem'WarFare_Effects.COG_AssaultRifle_Impact', true );
		PlaySound( BulletImpactSound );
	}
}

defaultproperties
{
	bNetInitialRotation=true
	BulletImpactSound=SoundCue'COG_AssaultRifle.COG_AssaultRifle_BulletImpacts'
}