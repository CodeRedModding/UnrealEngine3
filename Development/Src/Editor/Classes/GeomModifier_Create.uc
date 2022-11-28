//=============================================================================
// GeomModifier_Create: Creates selected objects
//=============================================================================

class GeomModifier_Create
	extends GeomModifier_Edit
	native;
	
cpptext
{
	virtual UBOOL Supports( int InSelType );
	virtual void Apply();
}
	
defaultproperties
{
	Description="Create"
	bPushButton=True
}
