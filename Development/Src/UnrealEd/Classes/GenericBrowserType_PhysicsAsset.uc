//=============================================================================
// GenericBrowserType_PhysicsAsset: PhysicsAssets
//=============================================================================

class GenericBrowserType_PhysicsAsset
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor( UObject* InObject );
}
	
defaultproperties
{
	Description="Physics Asset"
}
