/*=============================================================================
	UnEdComponents.h: Scene components used by the editor modes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * Implements some basic functionality used by all editor viewports.
 */
class FEditorCommonDrawHelper
{
public:

	FEditorCommonDrawHelper();
	virtual ~FEditorCommonDrawHelper() {}

	/** Renders the grid, pivot, and base info. */
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI);


	/** Draws the viewport grid. */
	void DrawGrid(const FSceneView* View,FPrimitiveDrawInterface* PDI);

	/**
	 * Draws a section(vertical lines or horizontal lines) of a viewport's grid.
	 */
	static void DrawGridSection(INT ViewportLocX,INT ViewportGridY,FVector* A,FVector* B,FLOAT* AX,FLOAT* BX,INT Axis,INT AlphaCase,const FSceneView* View,FPrimitiveDrawInterface* PDI);
	
	/**
	 * Renders the editor's pivot.
	 */
	void DrawPivot(const FSceneView* View,FPrimitiveDrawInterface* PDI);

	/** Draw green lines to indicate what the selected actor(s) are based on. */
	void DrawBaseInfo(const FSceneView* View,FPrimitiveDrawInterface* PDI);

	UBOOL bDrawGrid:1;
	UBOOL bDrawPivot:1;
	UBOOL bDrawBaseInfo:1;
	UBOOL bDrawWorldBox:1;
	UBOOL bDrawColoredOrigin:1;
	UBOOL bDrawKillZ:1;

	FColor GridColorHi;
	FColor GridColorLo;
	FLOAT PerspectiveGridSize;

	FColor PivotColor;
	FLOAT PivotSize;

	FColor BaseBoxColor;

	ESceneDepthPriorityGroup DepthPriorityGroup;
};


/*------------------------------------------------------------------------------
    UEditorComponent
	@todo: This class is probably deprecated now, but will be removed later.
------------------------------------------------------------------------------*/

class UEditorComponent : public UPrimitiveComponent
{
	DECLARE_CLASS_NOEXPORT(UEditorComponent,UPrimitiveComponent,0,UnrealEd);

	UBOOL bDrawGrid:1;
	UBOOL bDrawPivot:1;
	UBOOL bDrawBaseInfo:1;
	UBOOL bDrawWorldBox:1;
	UBOOL bDrawColoredOrigin:1;
	UBOOL bDrawKillZ:1;

	FColor GridColorHi;
	FColor GridColorLo;
	FLOAT PerspectiveGridSize;

	FColor PivotColor;
	FLOAT PivotSize;

	FColor BaseBoxColor;

	void StaticConstructor();

	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI);

	virtual FViewport* GetViewport() { return NULL; }

	void DrawGrid(const FSceneView* View,FPrimitiveDrawInterface* PDI);
	static void DrawGridSection(INT ViewportLocX,INT ViewportGridY,FVector* A,FVector* B,FLOAT* AX,FLOAT* BX,INT Axis,INT AlphaCase,const FSceneView* View,FPrimitiveDrawInterface* PDI);

	void DrawPivot(const FSceneView* View,FPrimitiveDrawInterface* PDI);

	void DrawBaseInfo(const FSceneView* View,FPrimitiveDrawInterface* PDI);
};




/*------------------------------------------------------------------------------
    UEdModeComponent.
------------------------------------------------------------------------------*/

class UEdModeComponent : public UEditorComponent
{
	DECLARE_CLASS_NOEXPORT(UEdModeComponent,UEditorComponent,0,UnrealEd);

	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI);
};

namespace EditorActorSelectionDefs
{
	/** The maximum number of actors we should select before a performance warning message is displayed to the user */
	static const INT MaxActorsToSelectBeforeWarning = 500;
};
