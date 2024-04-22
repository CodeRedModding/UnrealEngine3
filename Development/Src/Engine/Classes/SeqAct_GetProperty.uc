/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_GetProperty extends SequenceAction
	native(Sequence);

cpptext
{
	virtual void Activated();
}

/** Name of property to search for */
var() Name PropertyName;

defaultproperties
{
	ObjName="Get Property"
	ObjCategory="Object Property"

	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Object",bWriteable=TRUE)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Int',LinkDesc="Int",bWriteable=TRUE)
	VariableLinks(3)=(ExpectedType=class'SeqVar_Float',LinkDesc="Float",bWriteable=TRUE)
	VariableLinks(4)=(ExpectedType=class'SeqVar_String',LinkDesc="String",bWriteable=TRUE)
	VariableLinks(5)=(ExpectedType=class'SeqVar_Bool',LinkDesc="Bool",bWriteable=TRUE)
}
