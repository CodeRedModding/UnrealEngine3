/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AnimNotify_Scripted extends AnimNotify
	native(Anim)
	abstract;

event Notify( Actor Owner, AnimNodeSequence AnimSeqInstigator );
event NotifyEnd( Actor Owner, AnimNodeSequence AnimSeqInstigator );

cpptext
{
	// AnimNotify interface.
	virtual void Notify( class UAnimNodeSequence* NodeSeq );
	virtual void NotifyEnd( class UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime );
}
