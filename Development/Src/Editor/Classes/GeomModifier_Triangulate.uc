//=============================================================================
// GeomModifier_Triangulate: Triangulates selected objects
//=============================================================================

class GeomModifier_Triangulate
	extends GeomModifier_Edit
	native;
	
cpptext
{
	virtual UBOOL Supports( int InSelType );
	virtual void Apply();
}
	
defaultproperties
{
	Description="Triangulate"
	bPushButton=True
}
