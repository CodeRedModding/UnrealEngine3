//=============================================================================
// GenericBrowserType_Material: Materials
//=============================================================================

class GenericBrowserType_Material
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor( UObject* InObject );
}
	
defaultproperties
{
	Description="Material"
}
