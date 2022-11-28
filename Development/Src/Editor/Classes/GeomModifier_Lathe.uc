//=============================================================================
// GeomModifier_Lathe: Lathes selected objects around the widget
//=============================================================================

class GeomModifier_Lathe
	extends GeomModifier_Edit
	native;
	
var(Settings) int	TotalSegments;
var(Settings) int	Segments;
var(Settings) EAxis	Axis;

cpptext
{
	virtual UBOOL Supports( int InSelType );
	virtual void Apply();
	virtual void Initialize();
	
	void Apply( int InTotalSegments, int InSegments, EAxis InAxis );
}
	
defaultproperties
{
	Description="Lathe"
	TotalSegments=16
	Segments=4
	Axis=AXIS_Z
}
