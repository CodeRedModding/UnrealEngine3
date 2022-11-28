//=============================================================================
// GenericBrowserType_SoundCue: SoundCues
//=============================================================================

class GenericBrowserType_SoundCue
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor( UObject* InObject );
	virtual void InvokeCustomCommand( INT InCommand, UObject* InObject );
	virtual void DoubleClick( UObject* InObject );
}
	
defaultproperties
{
	Description="Sound Cue"
}
