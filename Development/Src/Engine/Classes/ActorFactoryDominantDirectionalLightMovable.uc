/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactoryDominantDirectionalLightMovable extends ActorFactory
	config(Editor)
	collapsecategories
	hidecategories(Object)
	native;

cpptext
{
	/**
	 * Returns whether the ActorFactory thinks it could create an Actor with the current settings.
	 * Can be used to determine if we should add to context menu or if the factory can be used for drag and drop.
	 *
	 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
	 * @param	bFromAssetOnly	If TRUE, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
	 * @return	TRUE if the actor can be created with this factory
	 */
	virtual UBOOL CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly = FALSE );
}

defaultproperties
{
	MenuName="Add Light (DominantDirectionalLightMovable)"
	NewActorClass=class'Engine.DominantDirectionalLightMovable'
}
