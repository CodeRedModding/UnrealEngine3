class PhATSkeletalMeshComponent extends SkeletalMeshComponent
	native;

cpptext
{
	// FPrimitiveViewInterface interface.
	virtual DWORD GetLayerMask(const FSceneContext& Context) const;
	virtual void Render(const FSceneContext& Context, struct FPrimitiveRenderInterface* PRI);
	virtual void RenderForeground(const FSceneContext& Context, struct FPrimitiveRenderInterface* PRI);
	virtual void RenderHitTest(const FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void RenderForegroundHitTest(const FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void RenderShadowDepth(const struct FSceneContext& Context, struct FPrimitiveRenderInterface* PRI);

	// PhATSkeletalMeshComponent interface
	void RenderAssetTools(const FSceneContext& Context, struct FPrimitiveRenderInterface* PRI, UBOOL bHitTest, UBOOL bDrawShadows);
	void RenderForegroundAssetTools(const FSceneContext& Context, struct FPrimitiveRenderInterface* PRI, UBOOL bHitTest, UBOOL bDrawShadows);
	void DrawHierarchy(FPrimitiveRenderInterface* PRI);
}

defaultproperties
{
	ForcedLodModel=1
}