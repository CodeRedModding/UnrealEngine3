/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqCond_GetServerType extends SequenceCondition
	native(Sequence);

cpptext
{
	virtual void Activated();
}

static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="Server Type"

	OutputLinks(0)=(LinkDesc="Standalone")
	OutputLinks(1)=(LinkDesc="Dedicated Server")
	OutputLinks(2)=(LinkDesc="Listen Server")
	OutputLinks(3)=(LinkDesc="Client")
}
