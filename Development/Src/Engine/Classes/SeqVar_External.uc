/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqVar_External extends SequenceVariable
	within Sequence
	native(Sequence);

cpptext
{
	// UObject interface
	virtual void PostLoad();

	// SequenceObject interface
	virtual void OnConnect(USequenceObject* connObj, INT connIdx);

	// SequenceVariable interface
	virtual FString GetValueStr();

	/**
	 * Returns whether this SequenceObject can exist in a sequence without being linked to anything else (i.e. does not require
	 * another sequence object to activate it)
	 */
	virtual UBOOL IsStandalone() const { return TRUE; }
}

/** */
var() class<SequenceVariable> ExpectedType;

/** Name of the variable link to create on the parent sequence */
var() string VariableLabel;

defaultproperties
{
	ObjName="External Variable"
	VariableLabel="Default Var"
}
