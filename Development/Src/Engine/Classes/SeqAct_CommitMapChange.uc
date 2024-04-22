/**
 * SeqAct_CommitMapChange
 *
 * Kismet action commiting pending map change
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_CommitMapChange extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
};

defaultproperties
{
	ObjName="Commit Map Change"

	ObjCategory="Level"
	VariableLinks.Empty
	InputLinks(0)=(LinkDesc="Commit")
}
