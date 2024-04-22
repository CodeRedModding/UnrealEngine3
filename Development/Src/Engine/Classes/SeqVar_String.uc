/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqVar_String extends SequenceVariable
	native(Sequence);

cpptext
{
	virtual FString* GetRef()
	{
		return &StrValue;
	}

	FString GetValueStr()
	{
		return StrValue;
	}

	virtual UBOOL SupportsProperty(UProperty *Property)
	{
		if (Cast<UStrProperty>(Property))
		{
			return TRUE;
		}

		UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
		if (ArrayProp)
		{
			if (Cast<UStrProperty>(ArrayProp->Inner))
			{
				return TRUE;
			}
		}

		return FALSE;
	}

	virtual void PublishValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink);
	virtual void PopulateValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink);
}

var() string			StrValue;

defaultproperties
{
	ObjName="String"
	ObjColor=(R=0,G=255,B=0,A=255)			// green
}
