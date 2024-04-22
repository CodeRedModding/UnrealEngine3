/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_ConvertToString extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
	void AppendVariables(TArray<USequenceVariable*> &LinkedVariables, FString &CombinedString, INT &VarCount);
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
};

var() bool bIncludeVarComment;
var() string VarSeparator;
var() int NumberOfInputs;

static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="Convert To String"
	ObjCategory="Misc"

	bIncludeVarComment=TRUE
	VarSeparator=", "

	VariableLinks(0)=(LinkDesc="Inputs",bAllowAnyType=TRUE)
	VariableLinks(1)=(ExpectedType=class'SeqVar_String',LinkDesc="Output",bWriteable=TRUE)

	NumberOfInputs=1
}
