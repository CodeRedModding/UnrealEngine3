/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_CastToFloat extends SeqAct_SetSequenceVariable
	native(Sequence);

cpptext
{
	void Activated()
	{
		OutputLinks(0).bHasImpulse = TRUE;
		
		FloatResult = (FLOAT)Value;
	}
};

var int Value;
var float FloatResult;

defaultproperties
{
	ObjName="Cast To Float"
	ObjCategory="Math"

	InputLinks(0)=(LinkDesc="In")
	
	OutputLinks(0)=(LinkDesc="Out")

	VariableLinks(0)=(ExpectedType=class'SeqVar_Int',LinkDesc="Int",PropertyName=Value)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="Result",bWriteable=true,PropertyName=FloatResult)
}
