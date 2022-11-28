//=============================================================================
// GeomModifier_Weld: Welds selected objects
//=============================================================================

class GeomModifier_Weld
	extends GeomModifier_Edit
	native;
	
cpptext
{
	virtual UBOOL Supports( int InSelType );
	virtual void Apply();
}
	
defaultproperties
{
	Description="Weld"
	bPushButton=True
}
