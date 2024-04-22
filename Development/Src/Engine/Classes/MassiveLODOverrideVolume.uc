//=============================================================================
// MassiveLODOverrideVolume:  Forces lowest detail massive LOD's to be displayed when the viewer is inside the volume.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class MassiveLODOverrideVolume extends Volume
	native
	hidecategories(Collision,Brush,Attachment,Physics,Volume)
	placeable;

cpptext
{
	/**
	 * Removes the volume from world info's list of volumes.
	 */
	virtual void ClearComponents();

protected:
	/**
	 * Adds the volume to world info's list of volumes.
	 */
	virtual void UpdateComponentsInternal( UBOOL bCollisionUpdate = FALSE );
public:
}

defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=false
		BlockActors=false
		BlockZeroExtent=false
		BlockNonZeroExtent=false
		BlockRigidBody=false
		RBChannel=RBCC_Nothing
	End Object

	bColored=true
	BrushColor=(R=120,G=80,B=80,A=255)

	bWorldGeometry=false
	bCollideActors=false
	bBlockActors=false
}
