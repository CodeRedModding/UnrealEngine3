/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ArrowComponent extends PrimitiveComponent
	native
	noexport
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var() color					ArrowColor;
var() float					ArrowSize;
/** If TRUE, don't show the arrow when SHOW_Sprites is disabled. */
var() bool					bTreatAsASprite;

/** Sprite category that the arrow component belongs to, if being treated as a sprite. Value serves as a key into the localization file. */
var editoronly name SpriteCategoryName;

defaultproperties
{
	ArrowColor=(R=255,G=0,B=0,A=255)
	ArrowSize=1.0
	// the arrow is generally for the editor, so by default do not load them in the game
	HiddenGame=True
	AlwaysLoadOnServer=false
	AlwaysLoadOnClient=false
	SpriteCategoryName="Misc"
}
