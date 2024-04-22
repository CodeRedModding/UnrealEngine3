
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimMetaData_SkelControlKeyFrame extends AnimMetaData_SkelControl
	native(Anim);

/** Modifiers for what time and what strength for this skelcontrol **/
var()  editinline array<TimeModifier>  KeyFrames;

cpptext
{
	virtual void SkelControlTick(USkelControlBase* SkelControl, UAnimNodeSequence* SeqNode);
}

defaultproperties
{
	bFullControlOverController=FALSE
}