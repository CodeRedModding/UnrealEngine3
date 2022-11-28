class RB_ConstraintDrawComponent extends PrimitiveComponent
	native(Physics);

cpptext
{
	// Primitive Component interface
	virtual void Render(const FSceneContext& Context, struct FPrimitiveRenderInterface* PRI);
	virtual UBOOL Visible(FSceneView* View);
}

var()	MaterialInstance	LimitMaterial;

defaultproperties
{
}