//=============================================================================
// HUD: Superclass of the heads-up display.
//
//Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class HUD extends Actor
	native
	config(Game)
	transient
	dependson(Canvas);

//=============================================================================
// Variables.

var	const	color	WhiteColor, GreenColor, RedColor;

var PlayerController 	PlayerOwner; // always the actual owner

/** Tells whether the game was paused due to lost focus */
var transient bool bLostFocusPaused;

// Visibility flags

var config	bool 	bShowHUD;					// Is the hud visible
var			bool	bShowScores;				// Is the Scoreboard visible
var			bool	bShowDebugInfo;				// If true, show properties of current ViewTarget
var()		bool	bShowBadConnectionAlert;	// Display indication of bad connection (set in C++ based on lag and packetloss).

var config  bool    bShowDirectorInfoDebug;     // If true matinee/director information will be visible in the HUD in DebugText
var config  bool    bShowDirectorInfoHUD;       // If true matinee/director information will be visible in the HUD (using KismetTextInfo)

var globalconfig bool	bMessageBeep;				// If true, any console messages will make a beep

var globalconfig float HudCanvasScale;    	// Specifies amount of screen-space to use (for TV's).

/** Use the full screen extents for the canvas. Ignores splitscreen and cinematic mode scaling. */
var bool bRenderFullScreen;

/** Scale the canvas in with the cinematic black bars. Default behavior. */
var bool bScaleCanvasForCinematicMode;

/** If true, render actor overlays */
var bool bShowOverlays;

/** Holds a list of Actors that need PostRender calls */
var array<Actor> PostRenderedActors;

// Console Messages

struct native ConsoleMessage
{
	var string Text;
	var color TextColor;
	var float MessageLife;
	var PlayerReplicationInfo PRI;
};

var array<ConsoleMessage> ConsoleMessages;
var const Color 		ConsoleColor;

var config int 	ConsoleMessageCount;
var globalconfig int 	ConsoleFontSize;
var globalconfig int 	MessageFontOffset;

var int MaxHUDAreaMessageCount;

// Localized Messages
struct native HudLocalizedMessage
{
    // The following block of variables are set when the message is entered;
    // (Message being set indicates that a message is in the list).

	var class<LocalMessage> Message;
	var String StringMessage;
	var int Switch;
	var float EndOfLife;
	var float Lifetime;
	var float PosY;
	var Color DrawColor;
	var int FontSize;

    // The following block of variables are cached on first render;
    // (StringFont being set indicates that they've been rendered).

	var Font StringFont;
	var float DX, DY;
	var bool Drawn;
	var int Count;
	var object OptionalObject;
};
var() transient HudLocalizedMessage LocalMessages[8];

var() float ConsoleMessagePosX, ConsoleMessagePosY; // DP_LowerLeft

/**
 * Canvas to Draw HUD on.
 * NOTE: a new Canvas is given every frame, only draw on it from the HUD::PostRender() event */
var	/*const*/ Canvas	Canvas;

//
// Useful variables
//

/** Used to create DeltaTime */
var transient	float	LastHUDRenderTime;
/** Time since last render */
var	transient	float	RenderDelta;
/** Size of ViewPort in pixels */
var transient	float	SizeX, SizeY;
/** Center of Viewport */
var transient	float	CenterX, CenterY;
/** Ratio of viewport compared to native resolution 1024x768 */
var	transient	float	RatioX, RatioY;

var globalconfig array<name> DebugDisplay;		// array of strings specifying what debug info to display for viewtarget actor
									// base engine types include "AI", "physics", "weapon", "net", "camera", and "collision"

struct native KismetDrawTextInfo
{
	var() string	MessageText;
	var   string    AppendedText;
	var() Font		MessageFont;
	var() vector2d	MessageFontScale;
	var() vector2d	MessageOffset;
	var() Color		MessageColor;
	var	  float		MessageEndTime;
};
var array<KismetDrawTextInfo> KismetTextInfo;

//=============================================================================
// Utils
//=============================================================================

// Draw3DLine  - draw line in world space. 
native final function Draw3DLine(vector Start, vector End, color LineColor);
native final function Draw2DLine(int X1, int Y1, int X2, int Y2, color LineColor);

event PostBeginPlay()
{
	super.PostBeginPlay();

	PlayerOwner = PlayerController(Owner);

	// e.g. getting material pointers to control effects for gameplay
	NotifyBindPostProcessEffects();
}

/* DrawActorOverlays()
draw overlays for actors that were rendered this tick and have added themselves to the PostRenderedActors array
*/
native function DrawActorOverlays(vector Viewpoint, rotator ViewRotation);

/************************************************************************************************************
 Actor Render - These functions allow for actors in the world to gain access to the hud and render
 information on it.
************************************************************************************************************/

/** RemovePostRenderedActor()
remove an actor from the PostRenderedActors array
*/
function RemovePostRenderedActor(Actor A)
{
	local int i;

	for ( i=0; i<PostRenderedActors.Length; i++ )
	{
		if ( PostRenderedActors[i] == A )
		{
			PostRenderedActors[i] = None;
			return;
		}
	}
}

/** AddPostRenderedActor()
add an actor to the PostRenderedActors array
*/
function AddPostRenderedActor(Actor A)
{
	local int i;

	// make sure that A is not already in list
	for ( i=0; i<PostRenderedActors.Length; i++ )
	{
		if ( PostRenderedActors[i] == A )
		{
			return;
		}
	}

	// add A at first empty slot
	for ( i=0; i<PostRenderedActors.Length; i++ )
	{
		if ( PostRenderedActors[i] == None )
		{
			PostRenderedActors[i] = A;
			return;
		}
	}

	// no empty slot found, so grow array
	PostRenderedActors[PostRenderedActors.Length] = A;
}

//=============================================================================
// Execs
//=============================================================================

/* hides or shows HUD */
exec function ToggleHUD()
{
	bShowHUD = !bShowHUD;
}

exec function ShowHUD()
{
	ToggleHUD();
}

/* toggles displaying scoreboard
*/
exec function ShowScores()
{
	SetShowScores(!bShowScores);
}

/** sets bShowScores to a specific value (not toggle) */
exec function SetShowScores(bool bNewValue)
{
	bShowScores = bNewValue;
}

/**
 * Toggles displaying properties of player's current ViewTarget
 * DebugType input values supported by base engine include "AI", "physics", "weapon", "net", "camera", and "collision"
 */
exec function ShowDebug(optional name DebugType)
{
	//local int i;
	local bool bRemoved;

	if (DebugType == 'None')
	{
		bShowDebugInfo = !bShowDebugInfo;
	}
	else
	{
		if (bShowDebugInfo)
		{
			// remove debugtype if already in array
			if (INDEX_NONE != DebugDisplay.RemoveItem(DebugType))
			{
				bRemoved = true;
			}
		}
		if (!bRemoved)
		{
			DebugDisplay[DebugDisplay.Length] = DebugType;
		}

		bShowDebugInfo = true;

		SaveConfig();
	}
}

function bool ShouldDisplayDebug(name DebugType)
{
	local int i;

	for ( i=0; i<DebugDisplay.Length; i++ )
	{
		if ( DebugDisplay[i] == DebugType )
		{
			return true;
		}
	}
	return false;
}

exec function ToggleDirectorInfoHUD()
{
    bShowDirectorInfoHUD = !bShowDirectorInfoHUD;
}

exec function ToggleDirectorInfoDebug()
{
    bShowDirectorInfoDebug = !bShowDirectorInfoDebug;
}

/** Entry point for basic debug rendering on the HUD.  Activated and controlled via the "showdebug" console command.  Can be overridden to display custom debug per-game. */
function ShowDebugInfo(out float out_YL, out float out_YPos)
{
	PlayerOwner.ViewTarget.DisplayDebug(self, out_YL, out_YPos);

	if (ShouldDisplayDebug('Game'))
	{
		WorldInfo.Game.DisplayDebug(self, out_YL, out_YPos);
	}

	if (ShouldDisplayDebug('AI') && (Pawn(PlayerOwner.ViewTarget) != None))
	{
		DrawRoute(Pawn(PlayerOwner.ViewTarget));
	}
}

//=============================================================================
// Debugging
//=============================================================================

// DrawRoute - Bot Debugging

function DrawRoute(Pawn Target)
{
	local int			i;
	local Controller	C;
	local vector		Start, RealStart, Dest;
	local bool			bPath;
	local Actor			FirstRouteCache;
	local Pylon FoundPylon;

	C = Target.Controller;
	if ( C == None )
		return;
	if ( C.CurrentPath != None )
		Start = C.CurrentPath.Start.Location;
	else
		Start = Target.Location;
	RealStart = Start;

	if ( C.bAdjusting )
	{
		Draw3DLine(C.Pawn.Location, C.GetAdjustLocation(), MakeColor(255,0,255,255));
		Start = C.GetAdjustLocation();
	}

	Dest = C.GetDestinationPosition();

	// check if using navmesh
	Foreach WorldInfo.AllNavigationPoints(class'Pylon', FoundPylon)
		break;

	if ( FoundPylon != None )
	{
		if( C.NavigationHandle == none )
		{
			C.NavigationHandle = new(C) class'NavigationHandle';
		}

		class'NavmeshPath_Toward'.static.TowardPoint(C.NavigationHandle,Dest);
		class'NavMeshGoal_At'.static.AtLocation(C.NavigationHandle,Dest);
		C.NavigationHandle.bVisualPathDebugging = TRUE;
		if(C.NavigationHandle.FindPath())
		{
			DrawDebugCoordinateSystem(Dest,rot(0,0,0),25.f,TRUE);
			C.NavigationHandle.DrawPathCache(,true);
		}
		else
		{
			DrawDebugCoordinateSystem(Dest,rot(0,0,0),25.f,TRUE);
			DrawDebugBox(Target.Location,Target.GetCollisionExtent(),255,0,0,TRUE);
		}
		return;
	}

	if( C.RouteCache.Length > 0 )
	{
		FirstRouteCache = C.RouteCache[0];
	}

	// show where pawn is going
	if ( (C == PlayerOwner)
		|| (C.MoveTarget == FirstRouteCache) && (C.MoveTarget != None) )
	{
		if ( (C == PlayerOwner) && (Dest != vect(0,0,0)) )
		{
			if ( C.PointReachable(Dest) )
			{
				Draw3DLine(C.Pawn.Location, Dest, MakeColor(255,255,255,255));
				return;
			}
			C.FindPathTo(Dest);
		}
		if( C.RouteCache.Length > 0 )
		{
			for ( i=0; i<C.RouteCache.Length; i++ )
			{
				if ( C.RouteCache[i] == None )
					break;
				bPath = true;
				Draw3DLine(Start,C.RouteCache[i].Location,MakeColor(0,255,0,255));
				Start = C.RouteCache[i].Location;
			}
			if ( bPath )
				Draw3DLine(RealStart,Dest,MakeColor(255,255,255,255));
		}
	}
	else if (Target.Velocity != vect(0,0,0))
		Draw3DLine(RealStart,Dest,MakeColor(255,255,255,255));

	if ( C == PlayerOwner )
		return;

	// show where pawn is looking
	Draw3DLine(Target.Location + Target.BaseEyeHeight * vect(0,0,1), C.GetFocalPoint(), MakeColor(255,0,0,255));
}

/**
 * Pre-Calculate most common values, to avoid doing 1200 times the same operations
 */
function PreCalcValues()
{
	// Size of Viewport
	SizeX	= Canvas.SizeX;
	SizeY	= Canvas.SizeY;

	// Center of Viewport
	CenterX = SizeX * 0.5;
	CenterY = SizeY * 0.5;

	// ratio of viewport compared to native resolution
	RatioX	= SizeX / 1024.f;
	RatioY	= SizeY / 768.f;
}

/**
 * PostRender is the main draw loop.
 */
event PostRender()
{
	local float		XL, YL, YPos;

	// Set up delta time
	RenderDelta = WorldInfo.TimeSeconds - LastHUDRenderTime;

	// Pre calculate most common variables
	if ( SizeX != Canvas.SizeX || SizeY != Canvas.SizeY )
	{
		PreCalcValues();
	}

	// Set PRI of view target
	if ( PlayerOwner != None )
	{
		// draw any debug text in real-time
		PlayerOwner.DrawDebugTextList(Canvas,RenderDelta);
	}

	if ( bShowDebugInfo )
	{
		Canvas.Font = class'Engine'.Static.GetTinyFont();
		Canvas.DrawColor = ConsoleColor;
		Canvas.StrLen("X", XL, YL);
		YPos = 0;

		ShowDebugInfo(YL, YPos);
	}
	else if ( bShowHud )
	{
		if ( !bShowScores )
		{
			DrawHud();

			DisplayConsoleMessages();
			DisplayLocalMessages();
			DisplayKismetMessages();
		}
	}

	if ( bShowBadConnectionAlert )
	{
		DisplayBadConnectionAlert();
	}

	LastHUDRenderTime = WorldInfo.TimeSeconds;
}

/**
 * The Main Draw loop for the hud.  Gets called before any messaging.  Should be subclassed
 */
function DrawHUD()
{
	local vector ViewPoint;
	local rotator ViewRotation;

	if ( bShowOverlays && (PlayerOwner != None) )
	{
		Canvas.Font = GetFontSizeIndex(0);
		PlayerOwner.GetPlayerViewPoint(ViewPoint, ViewRotation);
		DrawActorOverlays(Viewpoint, ViewRotation);
	}
	PlayerOwner.DrawHud( Self );
}

// DisplayBadConnectionAlert() - Warn user that net connection is bad
function DisplayBadConnectionAlert();	// Subclass Me

//=============================================================================
// Messaging.
//=============================================================================

function ClearMessage( out HudLocalizedMessage M )
{
	M.Message = None;
    M.StringFont = None;
}

// Console Messages

function Message( PlayerReplicationInfo PRI, coerce string Msg, name MsgType, optional float LifeTime )
{
	local string ThePlayerName;

	if ( bMessageBeep )
	{
		PlayerOwner.PlayBeepSound();
	}


	if ( (MsgType == 'Say') || (MsgType == 'TeamSay') )
	{
		ThePlayerName = PRI != None ? PRI.PlayerName : "";
		Msg = ThePlayerName $ ": "$Msg;
	}

	AddConsoleMessage(Msg,class'LocalMessage',PRI,LifeTime);
}

/**
 * Display current messages
 */
function DisplayConsoleMessages()
{
    local int Idx, XPos, YPos;
    local float XL, YL;

	if ( ConsoleMessages.Length == 0 )
		return;

    for (Idx = 0; Idx < ConsoleMessages.Length; Idx++)
    {
		if ( ConsoleMessages[Idx].Text == "" || ConsoleMessages[Idx].MessageLife < WorldInfo.TimeSeconds )
		{
			ConsoleMessages.Remove(Idx--,1);
		}
    }

    XPos = (ConsoleMessagePosX * HudCanvasScale * Canvas.SizeX) + (((1.0 - HudCanvasScale) / 2.0) * Canvas.SizeX);
    YPos = (ConsoleMessagePosY * HudCanvasScale * Canvas.SizeY) + (((1.0 - HudCanvasScale) / 2.0) * Canvas.SizeY);

    Canvas.Font = class'Engine'.Static.GetSmallFont();
    Canvas.DrawColor = ConsoleColor;

    Canvas.TextSize ("A", XL, YL);

    YPos -= YL * ConsoleMessages.Length; // DP_LowerLeft
    YPos -= YL; // Room for typing prompt

    for (Idx = 0; Idx < ConsoleMessages.Length; Idx++)
    {
		if (ConsoleMessages[Idx].Text == "")
		{
			continue;
		}

		if( ShouldShowConsoleMessage(ConsoleMessages[Idx]) == FALSE )
		{
			continue;
		}

		Canvas.StrLen( ConsoleMessages[Idx].Text, XL, YL );
		Canvas.SetPos( XPos, YPos );
		Canvas.DrawColor = ConsoleMessages[Idx].TextColor;
		Canvas.DrawText( ConsoleMessages[Idx].Text, false );
		YPos += YL;
    }
}

/** Determines whether or not the speciific console message should be displayed **/
function bool ShouldShowConsoleMessage( const  ConsoleMessage InConsoleMessage )
{
	return TRUE;
}



/**
 * Add a new console message to display.
 */
function AddConsoleMessage(string M, class<LocalMessage> InMessageClass, PlayerReplicationInfo PRI, optional float LifeTime)
{
	local int Idx, MsgIdx;
	MsgIdx = -1;
	// check for beep on message receipt
	if( bMessageBeep && InMessageClass.default.bBeep )
	{
		PlayerOwner.PlayBeepSound();
	}
	// find the first available entry
	if (ConsoleMessages.Length < ConsoleMessageCount)
	{
		MsgIdx = ConsoleMessages.Length;
	}
	else
	{
		// look for an empty entry
		for (Idx = 0; Idx < ConsoleMessages.Length && MsgIdx == -1; Idx++)
		{
			if (ConsoleMessages[Idx].Text == "")
			{
				MsgIdx = Idx;
			}
		}
	}
    if( MsgIdx == ConsoleMessageCount || MsgIdx == -1)
    {
		// push up the array
		for(Idx = 0; Idx < ConsoleMessageCount-1; Idx++ )
		{
			ConsoleMessages[Idx] = ConsoleMessages[Idx+1];
		}
		MsgIdx = ConsoleMessageCount - 1;
    }
	// fill in the message entry
	if (MsgIdx >= ConsoleMessages.Length)
	{
		ConsoleMessages.Length = MsgIdx + 1;
	}

    ConsoleMessages[MsgIdx].Text = M;
	if (LifeTime != 0.f)
	{
		ConsoleMessages[MsgIdx].MessageLife = WorldInfo.TimeSeconds + LifeTime;
	}
	else
	{
		ConsoleMessages[MsgIdx].MessageLife = WorldInfo.TimeSeconds + InMessageClass.default.LifeTime;
	}

    ConsoleMessages[MsgIdx].TextColor = InMessageClass.static.GetConsoleColor(PRI);
    ConsoleMessages[MsgIdx].PRI = PRI;
}

//===============================================
// Localized Message rendering

function LocalizedMessage
(
	class<LocalMessage>		InMessageClass,
	PlayerReplicationInfo	RelatedPRI_1,
	PlayerReplicationInfo	RelatedPRI_2,
	string					CriticalString,
	int						Switch,
	float					Position,
	float					LifeTime,
	int						FontSize,
	color					DrawColor,
	optional object			OptionalObject
)
{
	local int i, LocalMessagesArrayCount, MessageCount;

    if( InMessageClass == None || CriticalString == "" )
	{
		return;
	}

	if( bMessageBeep && InMessageClass.default.bBeep )
		PlayerOwner.PlayBeepSound();

    if( !InMessageClass.default.bIsSpecial )
    {
	    AddConsoleMessage( CriticalString, InMessageClass, RelatedPRI_1 );
		return;
    }

	LocalMessagesArrayCount = ArrayCount(LocalMessages);
    i = LocalMessagesArrayCount;
	if( InMessageClass.default.bIsUnique )
	{
		for( i = 0; i < LocalMessagesArrayCount; i++ )
		{
		    if( LocalMessages[i].Message == InMessageClass )
			{
				if ( InMessageClass.default.bCountInstances && (LocalMessages[i].StringMessage ~= CriticalString) )
				{
					MessageCount = (LocalMessages[i].Count == 0) ? 2 : LocalMessages[i].Count + 1;
				}
				break;
			}
		}
	}
	else if ( InMessageClass.default.bIsPartiallyUnique )
	{
		for( i = 0; i < LocalMessagesArrayCount; i++ )
		{
		    if( ( LocalMessages[i].Message == InMessageClass )
				&& InMessageClass.static.PartiallyDuplicates(Switch, LocalMessages[i].Switch, OptionalObject, LocalMessages[i].OptionalObject) )
				break;
		}
	}

    if( i == LocalMessagesArrayCount )
    {
	    for( i = 0; i < LocalMessagesArrayCount; i++ )
	    {
		    if( LocalMessages[i].Message == None )
				break;
	    }
    }

    if( i == LocalMessagesArrayCount )
    {
	    for( i = 0; i < LocalMessagesArrayCount - 1; i++ )
		    LocalMessages[i] = LocalMessages[i+1];
    }

    ClearMessage( LocalMessages[i] );

	// Add the local message to the spot.
	AddLocalizedMessage(i, InMessageClass, CriticalString, Switch, Position, LifeTime, FontSize, DrawColor, MessageCount, OptionalObject);

}

/**
 * Add the actual message to the array.  Made easier to tweak in a subclass
 *
 * @Param	Index				The index in to the LocalMessages array to place it.
 * @Param	InMessageClass		Class of the message
 * @Param	CriticialString		String of the message
 * @Param	Switch				The message switch
 * @Param	Position			Where on the screen is the message
 * @Param	LifeTime			How long does this message live
 * @Param	FontSize			How big is the message
 * @Param	DrawColor			The Color of the message
 */
function AddLocalizedMessage
(
	int						Index,
	class<LocalMessage>		InMessageClass,
	string					CriticalString,
	int						Switch,
	float					Position,
	float					LifeTime,
	int						FontSize,
	color					DrawColor,
	optional int			MessageCount,
	optional object			OptionalObject
)
{
	LocalMessages[Index].Message		= InMessageClass;
	LocalMessages[Index].Switch			= Switch;
	LocalMessages[Index].EndOfLife		= LifeTime + WorldInfo.TimeSeconds;
	LocalMessages[Index].StringMessage	= CriticalString;
	LocalMessages[Index].LifeTime		= LifeTime;
	LocalMessages[Index].PosY			= Position;
	LocalMessages[Index].DrawColor		= DrawColor;
	LocalMessages[Index].FontSize		= FontSize;
	LocalMessages[Index].Count			= MessageCount;
	LocalMessages[Index].OptionalObject	= OptionalObject;
}


function GetScreenCoords(float PosY, out float ScreenX, out float ScreenY, out HudLocalizedMessage InMessage )
{
    ScreenX = 0.5 * Canvas.ClipX;
    ScreenY = (PosY * HudCanvasScale * Canvas.ClipY) + (((1.0f - HudCanvasScale) * 0.5f) * Canvas.ClipY);

    ScreenX -= InMessage.DX * 0.5;
    ScreenY -= InMessage.DY * 0.5;
}

function DrawMessage(int i, float PosY, out float DX, out float DY )
{
    local float FadeValue;
    local float ScreenX, ScreenY;

	FadeValue = FMin(1.0, LocalMessages[i].EndOfLife - WorldInfo.TimeSeconds);

	Canvas.DrawColor = LocalMessages[i].DrawColor;
	Canvas.DrawColor.A = FadeValue * Canvas.DrawColor.A;
	Canvas.Font = LocalMessages[i].StringFont;
	GetScreenCoords( PosY, ScreenX, ScreenY, LocalMessages[i] );
	DX = LocalMessages[i].DX / Canvas.ClipX;
    DY = LocalMessages[i].DY / Canvas.ClipY;

	DrawMessageText(LocalMessages[i], ScreenX, ScreenY);
    LocalMessages[i].Drawn = true;
}

function DrawMessageText(HudLocalizedMessage LocalMessage, float ScreenX, float ScreenY)
{
	local FontRenderInfo FontInfo;

	Canvas.SetPos(ScreenX, ScreenY);
	FontInfo.bClipText = true;
	Canvas.DrawText(LocalMessage.StringMessage, FALSE,,, FontInfo);
}

function DisplayLocalMessages()
{
	local float PosY, DY, DX;
    local int i, j, LocalMessagesArrayCount, AreaMessageCount;
    local float FadeValue;
    local int FontSize;

	// early out
	if ( LocalMessages[0].Message == None )
		return;

 	Canvas.Reset(true);
	LocalMessagesArrayCount = ArrayCount(LocalMessages);

    // Pass 1: Layout anything that needs it and cull dead stuff.
    for( i = 0; i < LocalMessagesArrayCount; i++ )
    {
		if( LocalMessages[i].Message == None )
		{
			break;
		}

		LocalMessages[i].Drawn = false;

		if( LocalMessages[i].StringFont == None )
		{
			FontSize = LocalMessages[i].FontSize + MessageFontOffset;
			LocalMessages[i].StringFont = GetFontSizeIndex(FontSize);
			Canvas.Font = LocalMessages[i].StringFont;
			Canvas.TextSize( LocalMessages[i].StringMessage, DX, DY );
			LocalMessages[i].DX = DX;
			LocalMessages[i].DY = DY;

			if( LocalMessages[i].StringFont == None )
			{
//				`warn( "LayoutMessage("$LocalMessages[i].Message$") failed!" );

				for( j = i; j < LocalMessagesArrayCount - 1; j++ )
					LocalMessages[j] = LocalMessages[j+1];
				ClearMessage( LocalMessages[j] );
				i--;
				continue;
			}
		}

		FadeValue = (LocalMessages[i].EndOfLife - WorldInfo.TimeSeconds);

		if( FadeValue <= 0.0 )
		{
			for( j = i; j < LocalMessagesArrayCount - 1; j++ )
				LocalMessages[j] = LocalMessages[j+1];
			ClearMessage( LocalMessages[j] );
			i--;
			continue;
		}
     }

    // Pass 2: Go through the list and draw each stack:
    for( i = 0; i < LocalMessagesArrayCount; i++ )
	{
		if( LocalMessages[i].Message == None )
			break;

		if( LocalMessages[i].Drawn )
			continue;

		PosY = LocalMessages[i].PosY;
		AreaMessageCount = 0;

		for( j = i; j < LocalMessagesArrayCount; j++ )
		{
			if( LocalMessages[j].Drawn || (LocalMessages[i].PosY != LocalMessages[j].PosY) )
			{
				continue;
			}

			DrawMessage( j, PosY, DX, DY );

			PosY += DY;
			AreaMessageCount++;
		}
		if ( AreaMessageCount > MaxHUDAreaMessageCount )
		{
			LocalMessages[i].EndOfLife = WorldInfo.TimeSeconds;
		}
    }
}

function DisplayKismetMessages()
{
	local int KismetTextIdx;

	KismetTextIdx = 0;
	while( KismetTextIdx < KismetTextInfo.length )
	{
		if( KismetTextInfo[KismetTextIdx].MessageEndTime > 0 && KismetTextInfo[KismetTextIdx].MessageEndTime <= WorldInfo.TimeSeconds)
		{
			KismetTextInfo.Remove(KismetTextIdx,1);
		}
		else
		{
			DrawText(KismetTextInfo[KismetTextIdx].MessageText$KismetTextInfo[KismetTextIdx].AppendedText, 
					 KismetTextInfo[KismetTextIdx].MessageOffset, KismetTextInfo[KismetTextIdx].MessageFont, KismetTextInfo[KismetTextIdx].MessageFontScale, KismetTextInfo[KismetTextIdx].MessageColor);
			++KismetTextIdx;
		}
	}
}


function DrawText(string Text, vector2d Position, Font TextFont, vector2d FontScale, Color TextColor, optional const out FontRenderInfo RenderInfo)
{
	local float XL, YL;

	Canvas.Font = TextFont;
	Canvas.TextSize(Text, XL, YL);
	Canvas.SetPos(Canvas.ClipX/2 - XL/2 + Position.X, Canvas.ClipY/3 - YL/2 + Position.Y);
	Canvas.SetDrawColor(TextColor.R, TextColor.G, TextColor.B, TextColor.A);
	Canvas.DrawText(Text, FALSE, FontScale.X, FontScale.Y, RenderInfo);
}

static function Font GetFontSizeIndex(int FontSize)
{
	if ( FontSize == 0 )
	{
		return class'Engine'.Static.GetTinyFont();
	}
	else if ( FontSize == 1 )
	{
		return class'Engine'.Static.GetSmallFont();
	}
	else if ( FontSize == 2 )
	{
		return class'Engine'.Static.GetMediumFont();
	}
	else if ( FontSize == 3 )
	{
		return class'Engine'.Static.GetLargeFont();
	}
	else
	{
		return class'Engine'.Static.GetLargeFont();
	}
}

/**
 * Called when the player owner has died.
 */
function PlayerOwnerDied()
{
}

/**
 * Called in PostBeginPlay or postprocessing chain has changed (happens because of the worldproperties can define it's own chain and this one is set late).
 */
function NotifyBindPostProcessEffects()
{
	// overload with custom code e.g. getting material pointers to control effects for gameplay.
}

/**
 *	Pauses or unpauses the game due to main window's focus being lost.
 *	@param Enable tells whether to enable or disable the pause state
 */
event OnLostFocusPause(bool bEnable)
{
	if ( bLostFocusPaused == bEnable )
		return;

	if ( WorldInfo.NetMode != NM_Client )
	{
		bLostFocusPaused = bEnable;
		PlayerOwner.SetPause(bEnable);
	}
}

defaultproperties
{
	TickGroup=TG_DuringAsyncWork

	bHidden=true
	RemoteRole=ROLE_None

	WhiteColor=(R=255,G=255,B=255,A=255)
	ConsoleColor=(R=153,G=216,B=253,A=255)
	GreenColor=(R=0,G=255,B=0,A=255)
	RedColor=(R=255,G=0,B=0,A=255)

	ConsoleMessagePosY=0.8
	MaxHUDAreaMessageCount=3

	bLostFocusPaused=false

	bScaleCanvasForCinematicMode=true
	bRenderFullScreen=false
}
