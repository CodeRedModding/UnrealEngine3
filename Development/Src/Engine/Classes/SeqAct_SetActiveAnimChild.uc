/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetActiveAnimChild extends SequenceAction 
	native(Sequence);

/** Node name in the AnimTree - the node should always be active **/
var()	name		NodeName;
/** start with 1-N **/
var()	int			ChildIndex;
/** float blend time **/
var()	float		BlendTime;

cpptext
{
	virtual void Activated();
}

defaultproperties
{
	ObjName="Set Active Anim Child"
	ObjCategory="Anim"

	InputLinks(0)=(LinkDesc="Activate")

	BlendTime = 0.25
}