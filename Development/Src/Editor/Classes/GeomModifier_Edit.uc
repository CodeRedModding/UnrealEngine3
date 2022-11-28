//=============================================================================
// GeomModifier_Edit: Maniupalating selected objects with the widget
//=============================================================================

class GeomModifier_Edit
	extends GeomModifier
	native;

cpptext
{
	virtual UBOOL InputDelta(struct FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);
	
}
	
defaultproperties
{
	Description="Edit"
}
