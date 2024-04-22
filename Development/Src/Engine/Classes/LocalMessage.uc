//=============================================================================
// LocalMessage
//
// LocalMessages are abstract classes which contain an array of localized text.
// The PlayerController function ReceiveLocalizedMessage() is used to send messages
// to a specific player by specifying the LocalMessage class and index.  This allows
// the message to be localized on the client side, and saves network bandwidth since
// the text is not sent.  Actors (such as the GameInfo) use one or more LocalMessage
// classes to send messages.  The BroadcastHandler function BroadcastLocalizedMessage()
// is used to broadcast localized messages to all the players.
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class LocalMessage extends Object
	abstract;

var bool   bIsSpecial;                                     // If true, don't add to normal queue.
var bool   bIsUnique;                                      // If true and special, only one can be in the HUD queue at a time.
var bool   bIsPartiallyUnique;                             // If true and special, only one can be in the HUD queue with the same switch value
var bool   bIsConsoleMessage;                              // If true, put a GetString on the console.
var bool   bBeep;                                          // If true, beep!
var	bool   bCountInstances;									// if true, if sent to HUD multiple times, count up instances (only if bIsUnique)

var float    Lifetime;                                       // # of seconds to stay in HUD message queue.
var Color  DrawColor;
var float PosY;
var int		FontSize;                                      // Tiny to Huge ( see HUD::GetFontSizeIndex )

static function ClientReceive(
	PlayerController P,
	optional int Switch,
	optional PlayerReplicationInfo RelatedPRI_1,
	optional PlayerReplicationInfo RelatedPRI_2,
	optional Object OptionalObject
	)
{
	local string MessageString;

	MessageString = static.GetString(Switch, (RelatedPRI_1 == P.PlayerReplicationInfo), RelatedPRI_1, RelatedPRI_2, OptionalObject);
	if ( MessageString != "" )
	{
		if ( P.myHud != None )
			P.myHUD.LocalizedMessage(
				Default.Class,
				RelatedPRI_1,
				RelatedPRI_2,
				MessageString,
				Switch,
				static.GetPos(Switch, P.myHUD),
				static.GetLifeTime(Switch),
				static.GetFontSize(Switch, RelatedPRI_1, RelatedPRI_2, P.PlayerReplicationInfo),
				static.GetColor(Switch, RelatedPRI_1, RelatedPRI_2, OptionalObject),
				OptionalObject );

		if(IsConsoleMessage(Switch) && LocalPlayer(P.Player) != None && LocalPlayer(P.Player).ViewportClient != None)
			LocalPlayer(P.Player).ViewportClient.ViewportConsole.OutputText( MessageString );
	}
}

static function string GetString(
	optional int Switch,
	optional bool bPRI1HUD,
	optional PlayerReplicationInfo RelatedPRI_1,
	optional PlayerReplicationInfo RelatedPRI_2,
	optional Object OptionalObject
	)
{
	if ( class<Actor>(OptionalObject) != None )
		return class<Actor>(OptionalObject).static.GetLocalString(Switch, RelatedPRI_1, RelatedPRI_2);
	return "";
}

static function color GetConsoleColor( PlayerReplicationInfo RelatedPRI_1 )
{
    return Default.DrawColor;
}

static function color GetColor(
	optional int Switch,
	optional PlayerReplicationInfo RelatedPRI_1,
	optional PlayerReplicationInfo RelatedPRI_2,
	optional Object OptionalObject
	)
{
	return Default.DrawColor;
}

static function float GetPos( int Switch, HUD myHUD )
{
    return default.PosY;
}

static function int GetFontSize( int Switch, PlayerReplicationInfo RelatedPRI1, PlayerReplicationInfo RelatedPRI2, PlayerReplicationInfo LocalPlayer )
{
    return default.FontSize;
}

static function float GetLifeTime(int Switch)
{
    return default.LifeTime;
}

static function bool IsConsoleMessage(int Switch)
{
    return default.bIsConsoleMessage;
}

/**
  * RETURNS true if messages are similar enough to trigger "partially unique" check for HUD display
  */
static function bool PartiallyDuplicates(INT Switch1, INT Switch2, object OptionalObject1, object OptionalObject2 )
{
	return (Switch1 == Switch2);
}

defaultproperties
{
    bIsSpecial=true
    bIsUnique=false
    bIsPartiallyUnique=false
	Lifetime=3
	bIsConsoleMessage=True

	DrawColor=(R=255,G=255,B=255,A=255)
    PosY=0.83
}
