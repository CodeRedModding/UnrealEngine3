/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Allows clipping of BSP brushes against a plane.
 */
class GeomModifier_Clip
	extends GeomModifier_Edit
	native;

var(Settings)	bool	bFlipNormal;
var(Settings)	bool	bSplit;

/** The clip markers that the user has dropped down in the world so far. */
var array<vector>		ClipMarkers;

/** The mouse position, in world space, where the user currently is hovering. */
var	vector	SnappedMouseWorldSpacePos;

cpptext
{
	/**
	 * @return		TRUE if this modifier will work on the currently selected sub objects.
	 */
	virtual UBOOL Supports();

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
 	void ApplyClip( UBOOL InSplit, UBOOL InFlipNormal );
}
	
defaultproperties
{
	Description="BrushClip"
	bFlipNormal=FALSE
	bSplit=FALSE
}
