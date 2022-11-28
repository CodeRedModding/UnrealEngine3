class SeqVar_Bool extends SequenceVariable
	native(Sequence);

cpptext
{
	UBOOL* GetBoolRef()
	{
		return &((UBOOL)bValue);
	}

	FString GetValueStr()
	{
		return FString::Printf(TEXT("%s"),bValue?TEXT("True"):TEXT("False"));
	}
}

var() int			bValue;

// Red bool - gives you wings!
defaultproperties
{
	ObjName="Bool"
	ObjColor=(R=255,G=0,B=0,A=255)
}
