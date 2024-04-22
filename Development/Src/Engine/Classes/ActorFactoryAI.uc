/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactoryAI extends ActorFactory
	config(Editor)
	native;

cpptext
{
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

	virtual AActor* GetDefaultActor();
};

var() class<AIController>			ControllerClass;
var() class<Pawn>					PawnClass;
var() string						PawnName;

/** whether or not to give the spawned Pawn the default inventory for the gametype being played */
var() bool bGiveDefaultInventory;
/** additional inventory to give the Pawn */
var() array< class<Inventory> > InventoryList;
/** what team to put the AI on */
var() int TeamIndex;

defaultproperties
{
	ControllerClass=class'AIController'

	TeamIndex=255
	bPlaceable=false
}
