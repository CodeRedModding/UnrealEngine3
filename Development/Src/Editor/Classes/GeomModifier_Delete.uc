//=============================================================================
// GeomModifier_Delete: Deletes selected objects
//=============================================================================

class GeomModifier_Delete
	extends GeomModifier_Edit
	native;
	
cpptext
{
	virtual UBOOL Supports( int InSelType );
	virtual void Apply();
}
	
defaultproperties
{
	Description="Delete"
	bPushButton=True
}
