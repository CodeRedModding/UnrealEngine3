/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqVar_Vector extends SequenceVariable
	native(Sequence);

cpptext
{
	FVector* GetRef()
	{
		return &VectValue;
	}

	virtual FString GetValueStr()
	{
		return VectValue.ToString();
	}

	virtual UBOOL SupportsProperty(UProperty *Property)
	{
		UStructProperty* StructProp = Cast<UStructProperty>(Property);
		if (StructProp)
		{
			if (StructProp->Struct)
			{
				if (appStricmp(*(StructProp->Struct->GetName()), TEXT("Vector")) == 0)
				{
					return TRUE;
				}
			}
		}

		UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
		if (ArrayProp)
		{
			UStructProperty* StructProp = Cast<UStructProperty>(ArrayProp->Inner);
			if (StructProp && StructProp->Struct)
			{
				if (appStricmp(*(StructProp->Struct->GetName()), TEXT("Vector")) == 0)
				{
					return TRUE;
				}
			}
		}

		return FALSE;
	}

	virtual void PublishValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink);
	virtual void PopulateValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink);
}

var() vector VectValue;

defaultproperties
{
	ObjName="Vector"
	ObjColor=(R=128,G=128,B=0,A=255)		// dark yellow
}
