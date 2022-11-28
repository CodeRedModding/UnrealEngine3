class PathRenderingComponent extends PrimitiveComponent
	native(AI)
	hidecategories(Object)
	editinlinenew;

cpptext
{
	virtual void UpdateBounds();
	virtual void Render(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
};
