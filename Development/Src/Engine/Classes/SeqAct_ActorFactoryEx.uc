/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class SeqAct_ActorFactoryEx extends SeqAct_ActorFactory
	native(Sequence);

cpptext
{
	virtual void UpdateDynamicLinks();
	virtual void Spawned(UObject *NewSpawn);
}

defaultproperties
{
	ObjName="Actor Factory Ex"

	OutputLinks(2)=(LinkDesc="Spawned 1")

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Spawn Point",PropertyName=SpawnPoints)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Spawned 1",bWriteable=TRUE)
}
