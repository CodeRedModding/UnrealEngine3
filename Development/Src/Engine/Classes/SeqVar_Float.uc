/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqVar_Float extends SequenceVariable
	native(Sequence);

cpptext
{
	virtual FLOAT* GetRef()
	{
		return &FloatValue;
	}

	virtual FString GetValueStr()
	{
		return FString::Printf(TEXT("%2.3f"),FloatValue);
	}

	virtual UBOOL SupportsProperty(UProperty *Property)
	{
		return (Property->IsA(UFloatProperty::StaticClass()) ||
				(Property->IsA(UArrayProperty::StaticClass()) && ((UArrayProperty*)Property)->Inner->IsA(UFloatProperty::StaticClass())));
	}

	virtual void PublishValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink);
	virtual void PopulateValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink);
}

var() float			FloatValue;

defaultproperties
{
	ObjName="Float"
	ObjCategory="Float"
	ObjColor=(R=0,G=0,B=255,A=255)			// blue
}
