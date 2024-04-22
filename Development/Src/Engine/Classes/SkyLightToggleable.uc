/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SkyLightToggleable extends SkyLight
	native(Light)
	ClassGroup(Lights,SkyLights)
	placeable;

cpptext
{
public:
	/**
	 * Returns true if the light supports being toggled off and on on-the-fly
	 *
	 * @return For 'toggleable' lights, returns true
	 **/
	virtual UBOOL IsToggleable() const
	{
		// SkyLightToggleable supports being toggled on the fly!
		return TRUE;
	}
}


defaultproperties
{
	// Visual things should be ticked in parallel with physics
	TickGroup=TG_DuringAsyncWork

	bMovable=FALSE
	bStatic=FALSE
	bHardAttach=TRUE
}
