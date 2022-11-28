/** 
 * Blood Spray Emitter
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Emit_BloodSpray extends Emitter;

function simulated PostBeginPlay()
{
	super.PostBeginPlay();
	
	if( Level.NetMode != NM_DedicatedServer )
	{
		SetTemplate( ParticleSystem'AW-Particles.Bloodspray', true );
	}
}

defaultproperties
{
	bNetInitialRotation=true
}