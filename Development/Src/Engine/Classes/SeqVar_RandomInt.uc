/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqVar_RandomInt extends SeqVar_Int
	native(Sequence);

cpptext
{
	virtual INT* GetRef()
	{
		if( Min < Max )
		{
			IntValue = Min + (appRand() % (Max - Min + 1));
		}
		else
		{
			IntValue = Max + (appRand() % (Min - Max + 1));
		}
		return &IntValue;
	}

	virtual FString GetValueStr()
	{
		return FString::Printf(TEXT("%d..%d"),Min,Max);
	}

	virtual UBOOL SupportsProperty(UProperty *Property)
	{
		return FALSE;
	}
}

/** Min value for randomness */
var() int Min;

/** Max value for randomness */
var() int Max;

defaultproperties
{
	ObjName="Random Int"
	ObjCategory="Int"

	Min=0
	Max=100
}
