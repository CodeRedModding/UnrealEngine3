/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetMatInstScalarParam extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
}

var() MaterialInstanceConstant	MatInst;
var() Name						ParamName;

var() float ScalarValue;

defaultproperties
{
	ObjName="Set ScalarParam"
	ObjCategory="Material Instance"
	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Float',LinkDesc="ScalarValue",PropertyName=ScalarValue)
}
