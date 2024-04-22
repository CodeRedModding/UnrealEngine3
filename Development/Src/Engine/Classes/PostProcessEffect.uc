/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * A PostProcessEffect operates on an input render target and writes to an output target
 * These effects can be chained together in a PostProcessChain
 * Derive your own effects from this class
 */
class PostProcessEffect extends Object
	native
	dependson(Scene)
	hidecategories(Object);

/** Whether to apply the effect in the Editor */
var()	bool bShowInEditor;
/** Whether to apply the effect in the Game */
var()	bool bShowInGame;
/** Controls whether the effect should take its settings from the world's post process settings. */
var() bool bUseWorldSettings;
/** Name of the effect, used by e.g. FindEffectByName */
var() Name EffectName;

/** Variables for post process Editor support */
var		int		NodePosY;
var		int		NodePosX;
var		int		DrawWidth;
var		int		DrawHeight;
var		int		OutDrawY;
var		int		InDrawY;

/** controls which scene DPG to render this post-process effect in (mirrors ESceneDepthPriorityGroup) */
var() ESceneDepthPriorityGroup SceneDPG;

/** If true, the effect should be rendered after lighting and not at the end of the DPG. */
var bool bAffectsLightingOnly;

cpptext
{
	/**
	 * Creates a proxy to represent the render info for a post process effect
	 * @param WorldSettings - The world's post process settings for the view.
	 *			Will be NULL if the view didn't provide them, or the effect has bUseWorldSettings=False.
	 * @return The proxy object.
	 */
	virtual class FPostProcessSceneProxy* CreateSceneProxy(const FPostProcessSettings* WorldSettings) 
	{ 
		return NULL; 
	}

	/**
	 * @param View - current view
	 * @return TRUE if the effect should be rendered
	 */
	virtual UBOOL IsShown(const FSceneView* View) const;

	/**
	* @return TRUE if the effect requires the uber post process
	*/
	virtual UBOOL RequiresUberpostprocess() const 
	{ 
		return FALSE; 
	}

	/**
	* Tells the SceneRenderer is this effect includes the uber post process.
	*/
	virtual UBOOL IncludesUberpostprocess() const
	{
		return FALSE;
	}

	/**
	* This allows to print a warning when the effect is used.
	*/
	virtual void OnPostProcessWarning(FString& OutWarning) const
	{
	}
}

defaultproperties
{
	SceneDPG=SDPG_PostProcess
	bShowInEditor=TRUE
	bShowInGame=TRUE
	bAffectsLightingOnly=FALSE
}
