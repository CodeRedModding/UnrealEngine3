class SeqVar_Float extends SequenceVariable
	native(Sequence);

cpptext
{
	virtual FLOAT* GetFloatRef()
	{
		return &FloatValue;
	}

	virtual FString GetValueStr()
	{
		return FString::Printf(TEXT("%2.3f"),FloatValue);
	}
}

var() float			FloatValue;

defaultproperties
{
	ObjName="Float"
	ObjColor=(R=0,G=0,B=255,A=255)
}
