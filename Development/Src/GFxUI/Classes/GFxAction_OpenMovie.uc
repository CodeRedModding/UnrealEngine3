/**********************************************************************

Copyright   :   (c) 2006-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING 
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/
class GFxAction_OpenMovie extends SequenceAction
	native(UISequence)
	dependsOn(GFxMoviePlayer);

/** Swf Movie data to use */
var() SwfMovie              Movie;

/** Type of movie player to use */
var() class<GFxMoviePlayer>			MoviePlayerClass;

var GFxMoviePlayer MoviePlayer;

/** if true, focus on load */
var() bool                  bTakeFocus;

/** if true, capture input */
var() bool                  bCaptureInput;

/** if true, start paused */
var() bool                  bStartPaused;

/** Whether to gamma correct this movie before writing to the destination surface. */
var public bool bEnableGammaCorrection;

/** Whether to display the movie even if the HUD is turned off */
var() bool bDisplayWithHudOff;

/**  Use RTM_Alpha with BLEND_Translucent, doesn't support add.  Use RTM_AlphaComposite with BLEND_AlphaComposite. */
var() GFxRenderTextureMode RenderTextureMode;

/** If set, movie will be played on this RenderTexture */
var() TextureRenderTarget2D RenderTexture;

var() array<name> CaptureKeys;

var() array<name> FocusIgnoreKeys;

cpptext
{
	virtual void Activated();
}

event bool IsValidLevelSequenceObject() { return true; }

DefaultProperties
{
	bEnableGammaCorrection=FALSE
	bDisplayWithHudOff=TRUE
	RenderTextureMode=RTM_Opaque
	MoviePlayerClass=class'GFxMoviePlayer'
	ObjName="Open GFx Movie"
	ObjCategory="GFx UI"

	OutputLinks(0)=(LinkDesc="Success")
	OutputLinks(1)=(LinkDesc="Failed")

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="External Interface",bWriteable=false)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Player Owner",bWriteable=false)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Object',LinkDesc="Movie Player",bWriteable=true)
}
