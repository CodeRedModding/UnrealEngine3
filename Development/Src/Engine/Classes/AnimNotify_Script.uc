/**
 * The implmenting class (usually a pawn) needs to have a function named the same as the <NotifyName> that is specified in the AnimNotify.
 *
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AnimNotify_Script extends AnimNotify
	native(Anim);

var() name NotifyName;

/** If this notify has a duration, name of the function to call each update */
var() Name NotifyTickName;

/** If this notify has a duration, name of the function to call at the end */
var() Name NotifyEndName;

cpptext
{
	// AnimNotify interface.
	virtual void Notify( class UAnimNodeSequence* NodeSeq );
	virtual void NotifyTick( class UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime, FLOAT AnimTimeStep, FLOAT InTotalDuration );
	virtual void NotifyEnd( class UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime );
	virtual FString GetEditorComment() { return (NotifyName == NAME_None) ? TEXT("Script") : NotifyName.ToString(); }
}
