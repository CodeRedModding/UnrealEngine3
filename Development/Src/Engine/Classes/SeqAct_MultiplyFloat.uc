/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_MultiplyFloat extends SeqAct_SetSequenceVariable
	native(Sequence);

cpptext
{
	void Activated()
	{
		FloatResult = ValueA * ValueB;
		OutputLinks(0).bHasImpulse = TRUE;
		
		// Round the float result into the integer result
		IntResult = appRound( FloatResult );
	}
};

var() float ValueA;
var() float ValueB;
var float FloatResult;
var int IntResult;

defaultproperties
{
	ObjName="Multiply Float"
	ObjCategory="Math"

	InputLinks(0)=(LinkDesc="In")
	
	OutputLinks(0)=(LinkDesc="Out")

	VariableLinks(0)=(ExpectedType=class'SeqVar_Float',LinkDesc="A",PropertyName=ValueA)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="B",PropertyName=ValueB)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Float',LinkDesc="FloatResult",bWriteable=true,PropertyName=FloatResult)
	VariableLinks(3)=(ExpectedType=class'SeqVar_Int',LinkDesc="IntResult",bWriteable=true,PropertyName=IntResult)
}
