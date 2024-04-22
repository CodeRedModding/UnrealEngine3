/**
 * If they are bShowInGame they are still evaluated and will take up GPU time even if their effects are not seen.
 * So for MaterialEffects that are in your Post Process Chain you will want to manage them by toggling bShowInGame
 * for when you see them if they are not always evident.  (e.g. you press a button to go into "see invis things" mode 
 * which has some expensive and cool looking material.  You want to toggle that on the button press and not have it on
 * all the time)
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialEffect extends PostProcessEffect
	native;

var() MaterialInterface			Material;

cpptext
{
    // UPostProcessEffect interface

	/**
	 * Creates a proxy to represent the render info for a post process effect
	 * @param WorldSettings - The world's post process settings for the view.
	 * @return The proxy object.
	 */
	virtual class FPostProcessSceneProxy* CreateSceneProxy(const FPostProcessSettings* WorldSettings);
}