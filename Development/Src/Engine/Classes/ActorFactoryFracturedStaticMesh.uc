/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactoryFracturedStaticMesh extends ActorFactory
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
	
	virtual void AutoFillFields(class USelection* Selection);
	virtual FString GetMenuName();
}

var()	FracturedStaticMesh		FracturedStaticMesh;
var()	vector			DrawScale3D;

defaultproperties
{
	DrawScale3D=(X=1,Y=1,Z=1)

	MenuName="Add FracturedStaticMesh"
	NewActorClass=class'Engine.FracturedStaticMeshActor'
}
