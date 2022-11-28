//=============================================================================
// GenericBrowserType_Sounds: Sounds
//=============================================================================

class GenericBrowserType_Sounds
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor( UObject* InObject );
	virtual void InvokeCustomCommand( INT InCommand, UObject* InObject );
	virtual void DoubleClick( UObject* InObject );

	static void Play( USoundCue* InSound );
	static void Play( USoundNode* InSound );	
	static void Stop();
}
	
defaultproperties
{
	Description="Sounds"
}
