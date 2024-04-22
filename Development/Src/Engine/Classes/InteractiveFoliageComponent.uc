/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InteractiveFoliageComponent extends StaticMeshComponent
	native(Foliage)
	hidecategories(Object);

var protected{protected} const native duplicatetransient pointer FoliageSceneProxy{class FInteractiveFoliageSceneProxy};

cpptext
{
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	/**
	* Detach the component from the scene and remove its render proxy
	* @param bWillReattach TRUE if the detachment will be followed by an attachment
	*/
	virtual void Detach( UBOOL bWillReattach = FALSE );

	friend class AInteractiveFoliageActor;
}

defaultproperties
{
}
