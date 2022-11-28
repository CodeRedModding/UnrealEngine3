class SeqVar_RandomFloat extends SeqVar_Float
	native(Sequence);

cpptext
{
	virtual FLOAT* GetFloatRef()
	{
		FloatValue = Min + appFrand() * Max;
		return &FloatValue;
	}

	virtual FString GetValueStr()
	{
		return FString::Printf(TEXT("%2.1f..%2.1f"),Min,Max);
	}
}

/** Min value for randomness */
var() float Min;

/** Max value for randomness */
var() float Max;

defaultproperties
{
	ObjName="Float - Random"

	Min=0.f
	Max=1.f
}
