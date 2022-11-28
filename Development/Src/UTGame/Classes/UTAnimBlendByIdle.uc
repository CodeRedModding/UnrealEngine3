class UTAnimBlendByIdle extends UTAnimBlendBase
	native;

cpptext
{
	// AnimNode interface
	virtual	void TickAnim( float DeltaSeconds, FLOAT TotalWeight  );
}

defaultproperties
{
	Children(0)=(Name="Idle",Weight=1.0)
	Children(1)=(Name="Moving")
	bFixNumChildren=true

}
