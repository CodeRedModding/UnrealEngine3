class SeqVar_RandomInt extends SeqVar_Int
	native(Sequence);

cpptext
{
	virtual INT* GetIntRef()
	{
		IntValue = Min + (appRand() % (Max - Min));
		return &IntValue;
	}

	virtual FString GetValueStr()
	{
		return FString::Printf(TEXT("%d..%d"),Min,Max);
	}
}

/** Min value for randomness */
var() int Min;

/** Max value for randomness */
var() int Max;

defaultproperties
{
	ObjName="Int - Random"

	Min=0
	Max=100
}
