//=============================================================================
// GeomModifier_Turn: Turns selected objects
//=============================================================================

class GeomModifier_Turn
	extends GeomModifier_Edit
	native;
	
cpptext
{
	virtual UBOOL Supports( int InSelType );
	virtual void Apply();
}
	
defaultproperties
{
	Description="Turn"
	bPushButton=True
}
