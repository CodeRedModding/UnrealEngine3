/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleEventManager extends Actor
	abstract
	native(Particle)
	config(Game);


/** Needs to be overridden by game classes **/
event HandleParticleModuleEventSendToGame( ParticleModuleEventSendToGame InEvent, const out vector InCollideDirection, const out vector InHitLocation, const out vector InHitNormal, const out name InBoneName );


defaultproperties
{
}