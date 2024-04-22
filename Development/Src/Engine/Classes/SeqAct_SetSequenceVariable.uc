/**
 * Base class for all sequence actions that are capable of changing the value of a SequenceVariable
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetSequenceVariable extends SequenceAction
	native(Sequence)
	abstract;


DefaultProperties
{
	ObjName="Set Variable"
	ObjCategory="Set Variable"
}
