/**
 * Example 
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SecondaryViewportClient extends ScriptViewportClient
	native;

cpptext
{
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas);
	virtual UBOOL RequiresHitProxyStorage() { return 0; }

protected:
	
	virtual UCanvas* InitCanvas(FViewport* Viewport, FCanvas* Canvas);
	virtual void DrawSecondaryHUD(UCanvas* CanvasObject);
}

/**
 * Called after rendering the player views and HUDs to render menus, the console, etc.
 * This is the last rendering cal in the render loop
 * @param Canvas - The canvas to use for rendering.
 */
event PostRender(Canvas Canvas)
{
}

defaultproperties
{

}
