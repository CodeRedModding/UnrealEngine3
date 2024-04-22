/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleEventSendToGame extends Object
	native(Particle)
	abstract
	editinlinenew
	hidecategories(Object);


/** This is our function to allow subclasses to "do the event action" **/
function DoEvent( const out vector InCollideDirection, const out vector InHitLocation, const out vector InHitNormal, const out name InBoneName );


defaultproperties
{
}

