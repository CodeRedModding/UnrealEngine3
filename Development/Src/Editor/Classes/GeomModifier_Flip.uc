//=============================================================================
// GeomModifier_Flip: Flips selected objects
//=============================================================================

class GeomModifier_Flip
	extends GeomModifier_Edit
	native;
	
cpptext
{
	virtual UBOOL Supports( int InSelType );
	virtual void Apply();
}
	
defaultproperties
{
	Description="Flip"
	bPushButton=True
}
