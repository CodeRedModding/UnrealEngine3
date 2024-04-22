/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SeqAct_UpdatePhysBonesFromAnim extends SequenceAction;

static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="Force Phys Bones To Anim Pose"
	ObjCategory="Physics"

	InputLinks(0)=(LinkDesc="Update")
	InputLinks(1)=(LinkDesc="Disable Physics")
	InputLinks(2)=(LinkDesc="Enable Physics")
}