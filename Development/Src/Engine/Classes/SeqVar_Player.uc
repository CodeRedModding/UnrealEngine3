class SeqVar_Player extends SeqVar_Object
	native(Sequence);

cpptext
{
	UObject** GetObjectRef();

	virtual FString GetValueStr()
	{
		return FString(TEXT("Player"));
	}

	void OnCreated()
	{
		// do nothing
	}
};

var() int		PlayerIdx;		// which player to pick (for coop support)

defaultproperties
{
	ObjName="Player"
}
