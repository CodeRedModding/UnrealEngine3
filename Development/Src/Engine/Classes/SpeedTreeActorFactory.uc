/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
 
class SpeedTreeActorFactory extends ActorFactory
	config(Editor)
	native(SpeedTree);

cpptext
{
	virtual AActor*	CreateActor(const FVector* const Location, const FRotator* const Rotation, const class USeqAct_ActorFactory* const ActorFactoryData);
	
	/**
	 * If the ActorFactory thinks it could create an Actor with the current settings.
	 * Used to determine if we should add to context menu for example.
	 *
	 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
	 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
	 */
	virtual UBOOL CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly = FALSE );
	virtual void AutoFillFields(class USelection* Selection);
	virtual FString	GetMenuName();
}

var() SpeedTree	SpeedTree;

defaultproperties
{
	MenuName		= "Add SpeedTree"
	NewActorClass	= class'Engine.SpeedTreeActor'
}