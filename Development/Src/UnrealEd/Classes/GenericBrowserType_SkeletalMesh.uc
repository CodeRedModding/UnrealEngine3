//=============================================================================
// GenericBrowserType_SkeletalMesh: SkeletalMeshs
//=============================================================================

class GenericBrowserType_SkeletalMesh
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor( UObject* InObject );
	virtual void InvokeCustomCommand( INT InCommand, UObject* InObject );
}
	
defaultproperties
{
	Description="Skeletal Mesh"
}
