//=============================================================================
// GenericBrowserType_Texture: Textures
//=============================================================================

class GenericBrowserType_Texture
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor( UObject* InObject );
}
	
defaultproperties
{
	Description="Texture"
}
