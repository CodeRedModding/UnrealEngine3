/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqVar_Int extends SequenceVariable
	native(Sequence);

cpptext
{
	virtual INT* GetRef()
	{
		return &IntValue;
	}

	virtual FString GetValueStr()
	{
		return FString::Printf(TEXT("%d"),IntValue);
	}

	virtual UBOOL SupportsProperty(UProperty *Property)
	{
		return (Property->IsA(UIntProperty::StaticClass()) ||
				(Property->IsA(UArrayProperty::StaticClass()) && ((UArrayProperty*)Property)->Inner->IsA(UIntProperty::StaticClass())));
	}

	virtual void PublishValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink);
	virtual void PopulateValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink);
}

var() int				IntValue;

defaultproperties
{
	ObjName="Int"
	ObjCategory="Int"
	ObjColor=(R=0,G=255,B=255,A=255)		// bright aqua / teal
}
