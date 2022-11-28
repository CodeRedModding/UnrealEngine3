//=============================================================================
// GeomModifier_Extrude: Extrudes selected objects
//=============================================================================

class GeomModifier_Extrude
	extends GeomModifier_Edit
	native;
	
var(Settings)	int		Length;
var(Settings)	int		Segments;

cpptext
{
	virtual UBOOL Supports( int InSelType );
	virtual void Apply();
	virtual void Initialize();
	
	void Apply( int InLength, int InSegments );
}
	
defaultproperties
{
	Description="Extrude"
	Length=16
	Segments=1
}
