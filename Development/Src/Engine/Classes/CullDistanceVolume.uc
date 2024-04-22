/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CullDistanceVolume extends Volume
	native
	hidecategories(Advanced,Attachment,Collision,Volume)
	placeable;

/**
 * Helper structure containing size and cull distance pair.
 */
struct native CullDistanceSizePair
{
	/** Size to associate with cull distance. */
	var() float Size;
	/** Cull distance associated with size. */
	var() float CullDistance;
};

/**
 * Array of size and cull distance pairs. The code will calculate the sphere diameter of a primitive's BB and look for a best
 * fit in this array to determine which cull distance to use.
 */
var() array<CullDistanceSizePair> CullDistances;

/**
 * Whether the volume is currently enabled or not.
 */
var() bool bEnabled;

cpptext
{
	/**
	 * Called after change has occured - used to force update of affected primitives.
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/**
	 * bFinished is FALSE while the actor is being continually moved, and becomes TRUE on the last call.
	 * This can be used to defer computationally intensive calculations to the final PostEditMove call of
	 * eg a drag operation.
	 */
	virtual void PostEditMove(UBOOL bFinished);

	/**
	 * Returns whether the passed in primitive can be affected by cull distance volumes.
	 *
	 * @param	PrimitiveComponent	Component to test
	 * @return	TRUE if tested component can be affected, FALSE otherwise
	 */
	static UBOOL CanBeAffectedByVolumes( UPrimitiveComponent* PrimitiveComponent );

	/**
	 * Get the set of primitives and new max draw distances defined by this volume.
	 */
	void GetPrimitiveMaxDrawDistances(TMap<UPrimitiveComponent*,FLOAT>& OutCullDistances);
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

	CullDistances(0)=(Size=0,CullDistance=0)
	CullDistances(1)=(Size=10000,CullDistance=0)
	bEnabled=TRUE

	bCollideActors=False
	bBlockActors=False
	bProjTarget=False
	SupportedEvents.Empty
}
