class SeqVar_Int extends SequenceVariable
	native(Sequence);

cpptext
{
	virtual INT* GetIntRef()
	{
		return &IntValue;
	}

	virtual FString GetValueStr()
	{
		return FString::Printf(TEXT("%d"),IntValue);
	}
}

var() int				IntValue;

defaultproperties
{
	ObjName="Int"
	ObjColor=(R=0,G=255,B=255,A=255)
}
