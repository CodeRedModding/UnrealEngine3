//=============================================================================
// GenericBrowserType_StaticMesh: Static Meshes
//=============================================================================

class GenericBrowserType_StaticMesh
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor( UObject* InObject );
}
	
defaultproperties
{
	Description="Static Mesh"
}
