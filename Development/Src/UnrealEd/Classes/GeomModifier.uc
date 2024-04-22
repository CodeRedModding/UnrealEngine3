/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Base class of all geometry mode modifiers.
 */
class GeomModifier
	extends Object
	abstract
	hidecategories(Object,GeomModifier)
	native;

/** A human readable name for this modifier (appears on buttons, menus, etc) */
var(GeomModifier) string Description;

/** If true, this modifier should be displayed as a push button instead of a radio button */
var(GeomModifier) bool bPushButton;

/**
 * TRUE if the modifier has been initialized.
 * This is useful for interpreting user input and mouse drags correctly.
 */
var(GeomModifier) bool bInitialized;

/** Stored state of polys in case the brush state needs to be restroed */
var private Polys CachedPolys;

cpptext
{
	/**
	 * @return		The modifier's description string.
	 */
	const FString& GetModifierDescription() const;

	/**
	 * @return		TRUE if the key was handled by this editor mode tool.
	 */
	virtual UBOOL InputKey(struct FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event);

	/**
	 * @return		TRUE if the delta was handled by this editor mode tool.
	 */
	virtual UBOOL InputDelta(struct FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);

	/*
	 * Drawing functions to allow modifiers to have better control over the screen.
	 */
	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI);
	virtual void DrawHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas);

	/**
	 * Applies the modifier.  Does nothing if the editor is not in geometry mode.
	 *
	 * @return		TRUE if something happened.
	 */
 	UBOOL Apply();

	/**
	 * @return		TRUE if this modifier will work on the currently selected sub objects.
	 */
	virtual UBOOL Supports();

	/**
	 * Gives the individual modifiers a chance to do something the first time they are activated.
	 */
	virtual void Initialize();
	
	/**
	 * Starts the modification of geometry data.
	 */
	UBOOL StartModify();

	/**
	 * Ends the modification of geometry data.
	 */
	UBOOL EndModify();

	/**
	 * Handles the starting of transactions against the selected ABrushes.
	 */
	void StartTrans();
	
	/**
	 * Handles the stopping of transactions against the selected ABrushes.
	 */
	void EndTrans();

	virtual void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime) {}

	/**
	 * Gives the modifier a chance to initialize it's internal state when activated.
	 */
	virtual void WasActivated() {}

	/**
	* Gives the modifier a chance to clean up when the user is switching away from it.
	*/
	virtual void WasDeactivated() {}
 	
 	/**
 	 * Stores the current state of the brush so that upon faulty operations, the
 	 * brush may be restored to its previous state
 	 */
 	 void CacheBrushState();
 	 
 	/**
 	 * Restores the brush to its cached state
 	 */
 	 void RestoreBrushState();
 	 
 	/**
	 * @return		TRUE if two edges in the shape overlap not at a vertex
	 */
	UBOOL DoEdgesOverlap();

protected:
	/**
	 * Interface for displaying error messages.
	 *
	 * @param	InErrorMsg		The error message to display.
	 */
	void GeomError(const FString& InErrorMsg);
	
	/**
	 * Implements the modifier application.
	 */
 	virtual UBOOL OnApply();
}

defaultproperties
{
	Description="None"
	bPushButton=False
	bInitialized=False
	CachedPolys=None
}
