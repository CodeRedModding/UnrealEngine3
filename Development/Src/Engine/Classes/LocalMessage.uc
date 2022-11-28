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
//=============================================================================
class LocalMessage extends Info;

var bool   bIsSpecial;                                     // If true, don't add to normal queue.
var bool   bIsUnique;                                      // If true and special, only one can be in the HUD queue at a time.
var bool   bIsPartiallyUnique;                             // If true and special, only one can be in the HUD queue with the same switch value
var bool   bIsConsoleMessage;                              // If true, put a GetString on the console.
var bool   bBeep;                                          // If true, beep!

var int    Lifetime;                                       // # of seconds to stay in HUD message queue.
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
	if ( P.myHud != None )
		P.myHUD.LocalizedMessage( 
			Default.Class, 
			RelatedPRI_1,
			MessageString,
			Switch, 
			static.GetPos(Switch),
			static.GetLifeTime(Switch),
			static.GetFontSize(Switch, RelatedPRI_1, RelatedPRI_2, P.PlayerReplicationInfo),
			static.GetColor(Switch, RelatedPRI_1, RelatedPRI_2) );

	// fixmesteve - only get string once
	if ( default.bIsConsoleMessage && (P.Console != None) )
		P.Console.Message( MessageString, 0 );

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
	optional PlayerReplicationInfo RelatedPRI_2
	)
{
	return Default.DrawColor;
}

static function float GetPos( int Switch )
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