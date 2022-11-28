//=============================================================================
// GenericBrowserType_Animation: Animations
//=============================================================================

class GenericBrowserType_Animation
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor( UObject* InObject );
}
	
defaultproperties
{
	Description="Animation"
}
