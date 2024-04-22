
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimMetaData_SkelControl extends AnimMetaData
	native(Anim);

/** List of Bone Controllers Names to control. */
var() Array<Name> SkelControlNameList;

/** 
 * If TRUE, then it requires bControlledByAnimMetadata to be set as well on the BoneController.
 * It will then affect AnimMetadataWeight instead of ControlStrength.
 * And BoneController will only be turned on if there is such metadata present in the animation.
 * FALSE will set directly the BoneController's ControlStrength when that metadata is present.
 */
var() bool	bFullControlOverController;

// deprecated.
var deprecated name SkelControlName;

cpptext
{
	virtual void PostLoad();
	virtual void AnimSet(UAnimNodeSequence* SeqNode);
	virtual void AnimUnSet(UAnimNodeSequence* SeqNode);
	virtual void TickMetaData(UAnimNodeSequence* SeqNode);
	virtual UBOOL ShouldCallSkelControlTick(USkelControlBase* SkelControl, UAnimNodeSequence* SeqNode);
	virtual void SkelControlTick(USkelControlBase* SkelControl, UAnimNodeSequence* SeqNode);
}

defaultproperties
{
	bFullControlOverController=TRUE
}