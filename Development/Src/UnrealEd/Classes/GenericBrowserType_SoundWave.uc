//=============================================================================
// GenericBrowserType_SoundWave: SoundWaves
//=============================================================================

class GenericBrowserType_SoundWave
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual void InvokeCustomCommand( INT InCommand, UObject* InObject );
	virtual void DoubleClick( UObject* InObject );
}
	
defaultproperties
{
	Description="Sound Wave"
}
