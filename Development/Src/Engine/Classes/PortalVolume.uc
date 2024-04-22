/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Used to define areas of a map by portal
 */
class PortalVolume extends Volume
	native
	placeable
	hidecategories( Advanced, Attachment, Collision, Volume );

/** List of teleporters residing in this volume */
var				array<PortalTeleporter>		Portals;

cpptext
{
	/**
	 * Removes the portal volume to world info's list of portal volumes.
	 */
	virtual void ClearComponents( void );

protected:
	/**
	 * Adds the portal volume to world info's list of portal volumes.
	 */
	virtual void UpdateComponentsInternal( UBOOL bCollisionUpdate = FALSE );
public:
}

defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=False
		BlockActors=False
		BlockZeroExtent=False
		BlockNonZeroExtent=False
		BlockRigidBody=False
	End Object

	bCollideActors=False
	bBlockActors=False
	bProjTarget=False
	SupportedEvents.Empty
	SupportedEvents(0)=class'SeqEvent_Touch'
}
