/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetBool extends SeqAct_SetSequenceVariable
	native(Sequence);

cpptext
{
	void Activated();
};

var() bool DefaultValue<autocomment=true>;

defaultproperties
{
	ObjName="Bool"

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Bool',LinkDesc="Value",MinVars=0)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Bool',LinkDesc="Target",bWriteable=true)
}
