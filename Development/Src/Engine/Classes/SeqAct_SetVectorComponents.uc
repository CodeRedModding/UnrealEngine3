/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetVectorComponents extends SequenceAction
	native(Sequence);

var vector OutVector;
var float X, Y, Z;

cpptext
{
	void Activated()
	{
		OutVector.X = X;
		OutVector.Y = Y;
		OutVector.Z = Z;
		OutputLinks(0).bHasImpulse = TRUE;
	}
};

defaultproperties
{
	ObjName="Set Vector Components"
	ObjCategory="Math"

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Output Vector",bWriteable=TRUE,PropertyName=OutVector)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="X",PropertyName=X)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Float',LinkDesc="Y",PropertyName=Y)
	VariableLinks(3)=(ExpectedType=class'SeqVar_Float',LinkDesc="Z",PropertyName=Z)
}
