/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactory extends Object
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew
	config(Editor)
	abstract;

cpptext
{
	/** Called to actual create an actor at the supplied location/rotation, using the properties in the ActorFactory */
	virtual AActor* CreateActor( const FVector* const Location, const FRotator* const Rotation, const class USeqAct_ActorFactory* const ActorFactoryData );

	/**
	 * If the ActorFactory thinks it could create an Actor with the current settings.
	 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
	 *
	 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
	 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
	 * @return	True if the actor can be created with this factory
	 */
	virtual UBOOL CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly = FALSE );

	/** Fill in parameters automatically, possibly using the specified selection set. */
	virtual void AutoFillFields(class USelection* Selection) {}

	/**
	 * Clears references to resources [usually set by the call to AutoFillFields] when the factory has done its work.  The default behavior
	 * (which is to call AutoFillFields() with an empty selection set) should be sufficient for most factories, but this method is provided
	 * to allow customized behavior.
	 */
	virtual void ClearFields();

	/** Name to put on context menu. */
	virtual FString GetMenuName() { return MenuName; }

	/** Initialize NewActorClass if necessary, and return default actor for that class. */
	virtual AActor* GetDefaultActor();

    protected:
	/**
		 * This will check whether there is enough space to spawn an character.
		 * Additionally it will check the ActorFactoryData to for any overrides
		 * ( e.g. bCheckSpawnCollision )
		 *
		 * @return if there is enough space to spawn character at this location
		 **/
		UBOOL IsEnoughRoomToSpawnPawn( const FVector* const Location, const class USeqAct_ActorFactory* const ActorFactoryData ) const;

}

/** class to spawn during gameplay; only used if NewActorClass is left at the default */
var class<Actor> GameplayActorClass;

/** Name used as basis for 'New Actor' menu. */
var string			MenuName;

/** Indicates how far up the menu item should be. The higher the number, the higher up the list.*/
var config int		MenuPriority;

/** DEPRECATED - Alternate value for menu priority; Used to allow things like modifier keys to access items in a different order. */
var deprecated int	AlternateMenuPriority;

/** name of actor subclass this actorfactory creates - dynamically loaded.  Overrides NewActorClass. */
var config string NewActorClassName;

/** Actor subclass this ActorFactory creates. */
var	class<Actor>	NewActorClass;

/** Whether to appear on menu (or this Factory only used through scripts etc.) */
var bool			bPlaceable;

/** Whether to appear in the editor add actor quick menu */
var bool			bShowInEditorQuickMenu;

/** Allows script to modify new actor */
simulated event PostCreateActor(Actor NewActor, optional const SeqAct_ActorFactory ActorFactoryData);

defaultproperties
{
	MenuName="Add Actor"
	bPlaceable=true
	bShowInEditorQuickMenu=false;
}
