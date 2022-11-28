//=============================================================================
// GenericBrowserType_MaterialInstance: Material Instances
//=============================================================================

class GenericBrowserType_MaterialInstance
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor( UObject* InObject );
}
	
defaultproperties
{
	Description="Material Instance"
}
