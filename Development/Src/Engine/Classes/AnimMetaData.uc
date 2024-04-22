
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * Definition of AnimMetaData class
 * Warning: those are not instanced per AnimNodeSequence, they are solely attached to an AnimSequence.
 * Therefore they can be affecting multiple nodes at the same time!
 */

class AnimMetaData extends Object
	native(Anim)
	abstract
	editinlinenew
	hidecategories(Object)
	collapsecategories;

cpptext
{
	virtual void AnimSet(UAnimNodeSequence* SeqNode);
	virtual void AnimUnSet(UAnimNodeSequence* SeqNode);
	virtual void TickMetaData(UAnimNodeSequence* SeqNode);
}

