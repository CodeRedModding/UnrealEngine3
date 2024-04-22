//=============================================================================
// Player start location.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class PlayerStart extends NavigationPoint
	placeable
	native
	ClassGroup(Common)
	hidecategories(Collision);

cpptext
{
#if WITH_EDITOR
	void addReachSpecs(AScout *Scout, UBOOL bOnlyChanged=0);
#endif
}

var() bool bEnabled;
var() bool bPrimaryStart;		// None primary starts used only if no primary start available

/** Team specific player start, 255 for any team */
var() int TeamIndex;

// Properties used only by the PlayerStart scoring postrender system for visualizing playerstart scoring in multiplayer games.  Set these in
// your RatePlayerStart() or equivalent method.  Will be displayed if PlayerStart's bPostRenderIfNotVisible=true and the PlayerStart is added to 
// the HUD's PostRenderedActors array.
var int Score;
var int SelectionIndex;
var bool bBestStart;

/* epic ===============================================
* ::OnToggle
*
* Scripted support for toggling a playerstart, checks which
* operation to perform by looking at the action input.
*
* Input 1: turn on
* Input 2: turn off
* Input 3: toggle
*
* =====================================================
*/
simulated function OnToggle(SeqAct_Toggle action)
{
	if (action.InputLinks[0].bHasImpulse)
	{
		// turn on
		bEnabled = true;
	}
	else
	if (action.InputLinks[1].bHasImpulse)
	{
		// turn off
		bEnabled = false;
	}
	else
	if (action.InputLinks[2].bHasImpulse)
	{
		// toggle
		bEnabled = !bEnabled;
	}
}

/**
Hook to allow agents to render HUD overlays for themselves.
Called only if the agent was rendered this tick.  Assumes that appropriate font has already been set
Will be displayed if PlayerStart's bPostRenderIfNotVisible=true and the PlayerStart is added to the HUD's PostRenderedActors array.
*/
simulated event PostRenderFor(PlayerController PC, Canvas Canvas, vector CameraPosition, vector CameraDir)
{
	local float NameXL, TextYL, YL, XL, textscale;
	local vector ScreenLoc, ViewLoc;
	local rotator ViewRot;
	local string ScreenName;
	local FontRenderInfo FontInfo;

	PC.GetPlayerViewPoint(ViewLoc, ViewRot);
	if ( (vector(ViewRot) dot (Location - ViewLoc)) < 0.5 )
		return;

	// make sure not clipped out
	screenLoc = Canvas.Project(Location);
	if (screenLoc.X < 0 ||
		screenLoc.X >= Canvas.ClipX ||
		screenLoc.Y < 0 ||
		screenLoc.Y >= Canvas.ClipY)
	{
		return;
	}

	ScreenName = "("$SelectionIndex$")"@Score;
	Canvas.StrLen(ScreenName, NameXL, TextYL);
	XL = FMax(XL, NameXL);
	YL += TextYL;

	textscale = 1.0;
	if ( bBestStart )
	{
		Canvas.DrawColor = class'HUD'.default.GreenColor;
		textscale = 4.0;
	}
	else if ( Score == 10000000.0 )
	{
		Canvas.DrawColor = class'HUD'.default.WhiteColor;
		Canvas.DrawColor.B = 0;
	}
	else if ( Score == 0.0 )
	{
		Canvas.DrawColor.R = 200;
		Canvas.DrawColor.G = 50;
		Canvas.DrawColor.B = 255;
		textscale = 0.5;
	}		
	else
	{
		Canvas.DrawColor = class'HUD'.default.RedColor;
	}

	Canvas.SetPos(ScreenLoc.X-0.5*NameXL,ScreenLoc.Y-1.7*YL);
	FontInfo.bClipText = true;
	Canvas.DrawText(ScreenName, true, textscale, textscale, FontInfo);
}

defaultproperties
{
	Begin Object NAME=CollisionCylinder
		CollisionRadius=+00040.000000
		CollisionHeight=+00080.000000
	End Object

	Begin Object NAME=Sprite LegacyClassName=PlayerStart_PlayerStartSprite_Class
		Sprite=Texture2D'EditorResources.S_Player'
		SpriteCategoryName="PlayerStart"
	End Object

	bPrimaryStart=true
 	bEnabled=true
 	bCollideWhenPlacing=false

	TeamIndex=0

	bEdShouldSnap=true
}
