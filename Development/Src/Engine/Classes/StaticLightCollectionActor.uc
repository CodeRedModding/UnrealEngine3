/**
 * This is a special type of actor used as the container for a large number of LightComponents on the console.  This
 * actor is created only during the console cooking process so cannot be placed by designers in the editor.  It replaces
 * multiple normal static Light actors in content which has been cooked for the a console platform, becoming the owner for
 * those Light's LightComponent.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class StaticLightCollectionActor extends Light
	native(Light)
	config(Engine);

/**
 * Since the components array is only serialized during make, we need to store the components we contain in a separate array.
 */
var	const	array<LightComponent>	LightComponents;

/**
 * The maximum number of LightComponents that can be attached to this actor.  Once this number has been reached, a
 * new StaticLightCollectionActor will be created.
 */
var	config	int						MaxLightComponents;

cpptext
{
	/* === AActor interface === */
	/**
	 * Updates the CachedLocalToWorld transform for all attached components.
	 */
	virtual void UpdateComponentsInternal( UBOOL bCollisionUpdate=FALSE );


	/* === UObject interface === */
	/**
	 * Serializes the LocalToWorld transforms for the StaticMeshComponents contained in this actor.
	 */
	virtual void Serialize( FArchive& Ar );
}

DefaultProperties
{
	Components.Empty
}


