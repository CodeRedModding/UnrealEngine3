class SeqVar_String extends SequenceVariable
	native(Sequence);

cpptext
{
	FString* GetStringRef()
	{
		return &StrValue;
	}
	
	FString GetValueStr()
	{
		return StrValue;
	}
}

var() string			StrValue;

defaultproperties
{
	ObjName="String"
	ObjColor=(R=0,G=255,B=0,A=255)
}
