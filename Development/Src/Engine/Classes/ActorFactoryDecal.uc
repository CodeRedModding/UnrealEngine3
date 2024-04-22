/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactoryDecal extends ActorFactory
	config(Editor)
	native(Decal);

cpptext
{
	/**
	 * Called to create an actor at the supplied location/rotation
	 *
	 * @param	Location			Location to create the actor at
	 * @param	Rotation			Rotation to create the actor with
	 * @param	ActorFactoryData	Kismet object which spawns actors, could potentially have settings to use/override
	 *
	 * @return	The newly created actor, NULL if it could not be created
	 */
	virtual AActor* CreateActor( const FVector* const Location, const FRotator* const Rotation, const class USeqAct_ActorFactory* const ActorFactoryData );
	
	/**
	 * If the ActorFactory thinks it could create an Actor with the current settings.
	 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
	 *
	 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
	 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
	 * @return	True if the actor can be created with this factory
	 */
	virtual UBOOL CanCreateActor(FString& OutErrorMsg, UBOOL bFromAssetOnly = FALSE);
	
	/**
	 * Fill the data fields of this actor with the current selection
	 *
	 * @param	Selection	Selection to use to fill this actor's data fields with
	 */
	virtual void AutoFillFields(class USelection* Selection);
	
	/**
	 * Returns the name this factory should show up as in a context-sensitive menu
	 *
	 * @return	Name this factory should show up as in a menu
	 */
	virtual FString GetMenuName();

	/**
	 * Clears references to resources [usually set by the call to AutoFillFields] when the factory has done its work.  The default behavior
	 * (which is to call AutoFillFields() with an empty selection set) should be sufficient for most factories, but this method is provided
	 * to allow customized behavior.
	 */
	virtual void ClearFields();
}

var()	MaterialInterface	DecalMaterial;

defaultproperties
{
	MenuName="Add Decal"
	NewActorClass=class'Engine.DecalActor'
}
