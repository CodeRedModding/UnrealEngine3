/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_GetVectorComponents extends SequenceAction
	native(Sequence);

var vector InVector;
var float X, Y, Z;

cpptext
{
	void Activated()
	{
		X = InVector.X;
		Y = InVector.Y;
		Z = InVector.Z;
		OutputLinks(0).bHasImpulse = TRUE;
	}
};

defaultproperties
{
	ObjName="Get Vector Components"
	ObjCategory="Math"

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Input Vector", PropertyName=InVector)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="X",PropertyName=X)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Float',LinkDesc="Y",PropertyName=Y)
	VariableLinks(3)=(ExpectedType=class'SeqVar_Float',LinkDesc="Z",PropertyName=Z)
}
