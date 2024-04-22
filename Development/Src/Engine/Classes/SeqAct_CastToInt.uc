/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_CastToInt extends SeqAct_SetSequenceVariable
	native(Sequence);

cpptext
{
	void Activated()
	{
		OutputLinks(0).bHasImpulse = TRUE;
		
		if( bTruncate )
		{
			IntResult = (INT)Value;
		}
		else
		{
			IntResult = appRound( Value );
		}
	}
};

/** If TRUE, the float value will be truncated into the integer rather than rounded into it. */
var() bool bTruncate;

var float Value;
var int IntResult;

defaultproperties
{
	ObjName="Cast To Int"
	ObjCategory="Math"

	InputLinks(0)=(LinkDesc="In")
	
	OutputLinks(0)=(LinkDesc="Out")

	VariableLinks(0)=(ExpectedType=class'SeqVar_Float',LinkDesc="Int",PropertyName=Value)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Int',LinkDesc="Result",bWriteable=true,PropertyName=IntResult)
	
	bTruncate=False
}
