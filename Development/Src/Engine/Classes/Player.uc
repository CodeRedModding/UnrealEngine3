//=============================================================================
// Player: Corresponds to a real player (a local camera or remote net player).
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class Player extends Object
	native
	transient
	config(Engine)
	Inherits(FExec);

// The actor this player controls.
var transient const playercontroller		Actor;

// Net variables.
var const int CurrentNetSpeed;
var globalconfig int ConfiguredInternetSpeed, ConfiguredLanSpeed;

/** Global multiplier for scene desaturation PP effect.					*/
var config float							PP_DesaturationMultiplier;
/** Global multiplier for scene highlights PP effect.					*/
var config float							PP_HighlightsMultiplier;
/** Global multiplier for scene midtones PP effect.						*/
var config float							PP_MidTonesMultiplier;
/** Global multiplier for scene shadows PP effect.						*/
var config float							PP_ShadowsMultiplier;

/**
 * Dynamically assign Controller to Player and set viewport.
 *
 * @param    PC - new player controller to assign to player
 **/
native function SwitchController( PlayerController PC );

cpptext
{
	// FExec interface.
	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);
}
