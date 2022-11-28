//=============================================================================
// GeomModifier_Split: Splits selected objects
//=============================================================================

class GeomModifier_Split
	extends GeomModifier_Edit
	native;
	
cpptext
{
	virtual UBOOL Supports( int InSelType );
	virtual void Apply();
}
	
defaultproperties
{
	Description="Split"
	bPushButton=True
}
