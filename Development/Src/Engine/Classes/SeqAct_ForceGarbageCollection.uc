/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_ForceGarbageCollection extends SeqAct_Latent
	native(Sequence);

cpptext
{
	virtual void Activated();
	virtual UBOOL UpdateOp(FLOAT DeltaTime);
};

defaultproperties
{
	ObjName="Force Garbage Collection"
	ObjCategory="Misc"

	OutputLinks.Empty
	OutputLinks(0)=(LinkDesc="Finished")

	VariableLinks.Empty
}
