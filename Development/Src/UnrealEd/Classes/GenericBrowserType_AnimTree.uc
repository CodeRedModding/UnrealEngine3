//=============================================================================
// GenericBrowserType_AnimTree: Animation Blend Trees
//=============================================================================

class GenericBrowserType_AnimTree
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor( UObject* InObject );
}
	
defaultproperties
{
	Description="AnimTrees"
}
