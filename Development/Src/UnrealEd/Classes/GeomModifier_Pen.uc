/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Allows the user to place verts in an orthographic viewport and create a brush afterwards.
 */
class GeomModifier_Pen
	extends GeomModifier_Edit
	native;

/** If TRUE, the shape will be automatically extruded into a brush upon completion. */
var(Settings)	bool	bAutoExtrude;

/** If TRUE, the tool will try and optimize the resulting triangles into convex polygons before creating the brush. */
var(Settings)	bool	bCreateConvexPolygons;

/** If TRUE, the resulting shape will be turned into an ABrushShape actor. */
var(Settings)	bool	bCreateBrushShape;

/** How far to extrude the newly created brush if bAutoExtrude is set to TRUE. */
var(Settings)	INT		ExtrudeDepth;

var transient editoronly native pointer UsingViewportClient {FEditorLevelViewportClient};

/** The vertices that the user has dropped down in the world so far. */
var array<vector>		ShapeVertices;

/** The mouse position, in world space, where the user currently is hovering (snapped to grid if that setting is enabled). */
var	transient vector	MouseWorldSpacePos;

cpptext
{
	/**
	 * @return		TRUE if the key was handled by this editor mode tool.
	 */
	virtual UBOOL InputKey(struct FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event);

	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI);
	virtual void DrawHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas);

	virtual void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime);

	/**
	 * Gives the modifier a chance to initialize it's internal state when activated.
	 */
	virtual void WasActivated();

protected:
	/**
	 * Implements the modifier application.
	 */
 	virtual UBOOL OnApply();
 	
private:
 	void Apply();
}
	
defaultproperties
{
	Description="Pen"
	bCreateBrushShape=FALSE
	bAutoExtrude=TRUE
	ExtrudeDepth=256
	bCreateConvexPolygons=TRUE
}
