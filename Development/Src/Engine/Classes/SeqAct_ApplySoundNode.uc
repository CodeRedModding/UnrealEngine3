/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_ApplySoundNode extends SequenceAction
	native(Sequence);

cpptext
{
	virtual void Activated();
}

var() SoundCue PlaySound;
var() editinline SoundNode ApplyNode;

defaultproperties
{
	ObjName="Apply Sound Node"
	ObjCategory="Sound"
}
