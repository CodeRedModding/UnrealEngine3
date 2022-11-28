class SeqVar_Object extends SequenceVariable
	native(Sequence);

cpptext
{
	virtual UObject** GetObjectRef()
	{
		return &ObjValue;
	}

	virtual FString GetValueStr()
	{
		return FString::Printf(TEXT("%s"),ObjValue!=NULL?ObjValue->GetName():TEXT("???"));
	}

	virtual void OnCreated();

	virtual void OnExport()
	{
		ObjValue = NULL;
	}

	// USequenceVariable interface
	virtual void DrawExtraInfo(FRenderInterface* RI, const FVector& CircleCenter);
}

var Object			ObjValue;

defaultproperties
{
	ObjName="Object"
	ObjColor=(R=255,G=0,B=255,A=255)
}
