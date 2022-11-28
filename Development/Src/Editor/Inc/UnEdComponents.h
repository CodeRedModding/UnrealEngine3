/*=============================================================================
	UnEdComponents.h: Scene components used by the editor modes.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall.
=============================================================================*/

/*------------------------------------------------------------------------------
    UEditorComponent.
------------------------------------------------------------------------------*/

class UEditorComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UEditorComponent,UPrimitiveComponent,0,Editor);

	UBOOL bDrawGrid;
	UBOOL bDrawPivot;
	UBOOL bDrawBaseInfo;

	FColor GridColorHi;
	FColor GridColorLo;
	FLOAT PerspectiveGridSize;
	UBOOL bDrawWorldBox;
	UBOOL bDrawColoredOrigin;

	FColor PivotColor;
	FLOAT PivotSize;

	FColor BaseBoxColor;

	void StaticConstructor();

	virtual void GetZoneMask(UModel* Model);
	virtual DWORD GetLayerMask(const FSceneContext& Context) const;
	virtual void RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
	virtual void RenderBackground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
	virtual void Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
	virtual FViewport* GetViewport() { return NULL; }

	void RenderGrid(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
	static void DrawGridSection(INT ViewportLocX,INT ViewportGridY,FVector* A,FVector* B,FLOAT* AX,FLOAT* BX,INT Axis,INT AlphaCase,FSceneView* View,FPrimitiveRenderInterface* PRI);

	void RenderPivot(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);

	void RenderBaseInfo(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
};

/*------------------------------------------------------------------------------
    UEdModeComponent.
------------------------------------------------------------------------------*/

class UEdModeComponent : public UEditorComponent
{
	DECLARE_CLASS(UEdModeComponent,UEditorComponent,0,Editor);

	virtual DWORD GetLayerMask(const FSceneContext& Context) const;
	virtual void RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
	virtual void RenderBackground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
	virtual void Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
};
