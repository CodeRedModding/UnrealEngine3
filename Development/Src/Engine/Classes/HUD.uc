//=============================================================================
// HUD: Superclass of the heads-up display.
//=============================================================================
class HUD extends Actor
	native
	config(Game)
	transient;

//=============================================================================
// Variables.

var	const	color	WhiteColor, GreenColor, RedColor;

var PlayerController 	PlayerOwner; // always the actual owner
var HUD 				HudOwner;	 // Used for sub-huds like the scoreboard

var PlayerReplicationInfo ViewedInfo;	// The PRI of the current view target

var float ProgressFadeTime;
var Color MOTDColor;

var actor AnimDebugThis;

var ScoreBoard 	Scoreboard;

// Visibility flags

var bool 	bShowHUD;				// Is the hud visible
var bool	bShowScores;			// Is the Scoreboard visible
var bool	bShowDebugInfo;			// if true, show properties of current ViewTarget
var config bool	bShowAIDebug;
var config bool	bShowPhysicsDebug;
var config bool	bShowWeaponDebug;
var config bool	bShowNetworkDebug;
var config bool	bShowCollisionDebug;
var bool	bShowAnimDebug;			// If true, the animation system will show it's debug data

var bool	bHideCenterMessages;			// don't draw centered messages (screen center being used)
var() bool bShowBadConnectionAlert;     // Display indication of bad connection

var config bool bMessageBeep;				// If true, any console messages will make a beep

var globalconfig float HudCanvasScale;    	// Specifies amount of screen-space to use (for TV's).

// Level Action Messages

var localized string LoadingMessage;
var localized string SavingMessage;
var localized string ConnectingMessage;
var localized string PausedMessage;
var localized string PrecachingMessage;

// Console Messages

struct native ConsoleMessage
{
	var string Text;
	var color TextColor;
	var float MessageLife;
	var PlayerReplicationInfo PRI;
};

var ConsoleMessage 		ConsoleMessages[8];
var const Color 		ConsoleColor;

var globalconfig int 	ConsoleMessageCount;
var globalconfig int 	ConsoleFontSize;
var globalconfig int 	MessageFontOffset;

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
};
var() transient HudLocalizedMessage LocalMessages[8];


var() float ConsoleMessagePosX, ConsoleMessagePosY; // DP_LowerLeft


/** 
 * Canvas to Draw HUD on. 
 * NOTE: a new Canvas is given every frame, only draw on it from the HUD::PostRender() event */
var	const Canvas	Canvas;

//
// Useful variables
//

/** Used to create DeltaTime */
var transient	float	LastRenderTime;
/** Time since last render */
var	transient	float	RenderDelta;
/** Size of ViewPort in pixels */
var transient	float	SizeX, SizeY;
/** Center of Viewport */
var transient	float	CenterX, CenterY;
/** Ratio of viewport compared to native resolution 1024x768 */
var	transient	float	RatioX, RatioY;


//=============================================================================
// Utils
//=============================================================================

// Draw3DLine  - draw line in world space. Should be used when engine calls RenderWorldOverlays() event.

native final function Draw3DLine(vector Start, vector End, color LineColor);

simulated event PostBeginPlay()
{
	super.PostBeginPlay();

	PlayerOwner = PlayerController(Owner);
}

// SpawnScoreBoard - Sets the scoreboard

function SpawnScoreBoard(class<Scoreboard> ScoringType)
{
	if ( ScoringType != None )
		Scoreboard = Spawn(ScoringType, PlayerOwner);

	if (ScoreBoard!=None)
    	ScoreBoard.HudOwner = self;
}

// Clean up

simulated event Destroyed()
{
    if( ScoreBoard != None )
    {
        ScoreBoard.Destroy();
        ScoreBoard = None;
    }
	Super.Destroyed();
}

//=============================================================================
// Execs
//=============================================================================

/* hides or shows HUD */
exec function ToggleHUD()
{
	bShowHUD = !bShowHUD;
}

/* toggles displaying scoreboard
*/
exec function ShowScores()
{
	bShowScores = !bShowScores;
}

/* toggles displaying properties of player's current viewtarget
*/
exec function ShowDebug()
{
	bShowDebugInfo = !bShowDebugInfo;
}

exec function ShowAIDebug()
{
	bShowAIDebug = !bShowAIDebug;
}

exec function ShowPhysicsDebug()
{
	bShowPhysicsDebug = !bShowPhysicsDebug;
}

exec function ShowWeaponDebug()
{
	bShowWeaponDebug = !bShowWeaponDebug;
}

exec function ShowNetworkDebug()
{
	bShowNetworkDebug = !bShowNetworkDebug;
}

exec function ShowCollisionDebug()
{
	bShowCollisionDebug = !bShowCollisionDebug;
}

// Show debug information for owner's pawn's skeletal mesh
exec function ShowAnimDebug()
{
	bShowAnimDebug = !bShowAnimDebug;

	if ( bShowAnimDebug )
		AnimDebugThis = PlayerOwner.Pawn;
}
// Show debug information for owner's pawn's weapon's skeletal mesh
exec function ShowWeapAnimDebug()
{
	bShowAnimDebug = !bShowAnimDebug;
    if ( bShowAnimDebug )
    	AnimDebugThis = PlayerOwner.Pawn.Weapon;
}


// Show debug information for another pawns skeletal mesh. Finds nearest of given class (excluding owner's pawn).
exec function ShowPawnAnimDebug(class<Pawn> aClass)
{
	local Pawn P, ClosestPawn;
	local float ThisDistance, ClosestPawnDistance;

	bShowAnimDebug = !bShowAnimDebug;

	if(bShowAnimDebug)
	{
		ClosestPawn = None;
		ClosestPawnDistance = 10000000.0;
		ForEach DynamicActors(class'Pawn', P)
		{
			if ( ClassIsChildOf(P.class, aClass) && (P != PlayerOwner.Pawn) )
			{
				ThisDistance = VSize(P.Location - PlayerOwner.Pawn.Location);
				if(ThisDistance < ClosestPawnDistance)
				{
					ClosestPawn = P;
					ClosestPawnDistance = ThisDistance;
				}
			}
		}

		AnimDebugThis = ClosestPawn;
	}
}

//=============================================================================
// Debugging
//=============================================================================

// DrawRoute - Bot Debugging

simulated function DrawRoute()
{
	local int			i;
	local Controller	C;
	local vector		Start, End, RealStart;;
	local bool			bPath;
	local Actor			FirstRouteCache;

	C = Pawn(PlayerOwner.ViewTarget).Controller;
	if ( C == None )
		return;
	if ( C.CurrentPath != None )
		Start = C.CurrentPath.Start.Location;
	else
		Start = PlayerOwner.ViewTarget.Location;
	RealStart = Start;

	if ( C.bAdjusting )
	{
		Draw3DLine(C.Pawn.Location, C.AdjustLoc, MakeColor(255,0,255,255));
		Start = C.AdjustLoc;
	}

	if( C.RouteCache.Length > 0 )
	{
		FirstRouteCache = C.RouteCache[0];
	}

	// show where pawn is going
	if ( (C == PlayerOwner)
		|| (C.MoveTarget == FirstRouteCache) && (C.MoveTarget != None) )
	{
		if ( (C == PlayerOwner) && (C.Destination != vect(0,0,0)) )
		{
			if ( C.PointReachable(C.Destination) )
			{
				Draw3DLine(C.Pawn.Location, C.Destination, MakeColor(255,255,255,255));
				return;
			}
			C.FindPathTo(C.Destination);
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
				Draw3DLine(RealStart,C.Destination,MakeColor(255,255,255,255));
		}
	}
	else if ( PlayerOwner.ViewTarget.Velocity != vect(0,0,0) )
		Draw3DLine(RealStart,C.Destination,MakeColor(255,255,255,255));

	if ( C == PlayerOwner )
		return;

	// show where pawn is looking
	if ( C.Focus != None )
		End = C.Focus.Location;
	else
		End = C.FocalPoint;
	Draw3DLine(PlayerOwner.ViewTarget.Location + Pawn(PlayerOwner.ViewTarget).BaseEyeHeight * vect(0,0,1),End,MakeColor(255,0,0,255));
}

/**
 * Pre-Calculate most common values, to avoid doing 1200 times the same operations
 */
simulated function PreCalcValues()
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
simulated event PostRender()
{
	local float		XL, YL, YPos;

	// Set up delta time
    RenderDelta = Level.TimeSeconds - LastRenderTime;

	// Pre calculate most common variables
	if ( SizeX != Canvas.SizeX || SizeY != Canvas.SizeY )
		PreCalcValues();

	// Set PRI of view target
    if ( PlayerOwner != None )
    {
    	if ( PlayerOwner.ViewTarget != None )
	    {
	        if ( Pawn(PlayerOwner.ViewTarget) != None )
				ViewedInfo = Pawn(PlayerOwner.ViewTarget).PlayerReplicationInfo;
        	else
            	ViewedInfo = PlayerOwner.PlayerReplicationInfo;
        }
        else if ( PlayerOwner.Pawn != None )
        	ViewedInfo = PlayerOwner.Pawn.PlayerReplicationInfo;
    	else
        	ViewedInfo = PlayerOwner.PlayerReplicationInfo;
    }
	else
   		ViewedInfo = PlayerOwner.PlayerReplicationInfo;


    if ( bShowDebugInfo )
    {
        Canvas.Font			= class'Engine'.Default.TinyFont;
        Canvas.DrawColor	= ConsoleColor;
		Canvas.StrLen("X", XL, YL);
		YPos = 0;
        PlayerOwner.ViewTarget.DisplayDebug (self, YL, YPos);

		if ( bShowAIDebug && (Pawn(PlayerOwner.ViewTarget) != None) )
			DrawRoute();
	}
	else if ( bShowHud )
    {
        if ( bShowScores )
        {
            if ( ScoreBoard != None )
            {
                ScoreBoard.DrawHud();
				if ( Scoreboard.bDisplayMessages )
					DisplayConsoleMessages();
			}
        }
        else
        {
            DrawHud();

            if ( !DrawLevelAction() )
            {
                if ((PlayerOwner != None) && (PlayerOwner.ProgressTimeOut > Level.TimeSeconds))
                    DisplayProgressMessage();
            }

            DisplayConsoleMessages();
			DisplayLocalMessages();

		    PlayerOwner.DrawHud( Self );
        }
    }

	bHideCenterMessages = DrawLevelAction();

	if ( !bHideCenterMessages && (PlayerOwner.ProgressTimeOut > Level.TimeSeconds) )
		DisplayProgressMessage();


	if ( bShowBadConnectionAlert )
		DisplayBadConnectionAlert();

    if ( bShowAnimDebug && AnimDebugThis != None )
	{
		// Draw animation Debug
    	if ( Pawn(AnimDebugThis) != None && Pawn(AnimDebugThis).Mesh != None )
        	Pawn(AnimDebugThis).Mesh.DrawAnimDebug( Canvas );
	}

	LastRenderTime = Level.TimeSeconds;
}


/**
 * The Main Draw loop for the hud.  Gets called before any messaging.  Should be subclassed
 */
function DrawHUD()
{
	DrawEngineHUD();
}

/**
 * Special HUD for Engine demo
 */
function DrawEngineHUD()
{
	local	float	CrosshairSize;
	local	float	xl,yl,Y;
	local	String	myText;

	// Draw Copyright Notice
	Canvas.SetDrawColor(255, 255, 255, 255);
	myText = "UnrealEngine3";
	Canvas.Font = class'Engine'.Default.SmallFont;
	Canvas.StrLen(myText, XL, YL);
	Y = YL*1.67;
	Canvas.SetPos( CenterX - (XL/2), YL*0.5);
	Canvas.DrawText(myText, true);

	Canvas.SetDrawColor(200,200,200,255);
	myText = "Copyright (C) 2004, Epic Games";
	Canvas.Font = class'Engine'.Default.TinyFont;
	Canvas.StrLen(myText, XL, YL);
	Canvas.SetPos( CenterX - (XL/2), Y);
	Canvas.DrawText(myText, true);

	// Draw Temporary Crosshair
	CrosshairSize = 4;
	Canvas.SetPos(CenterX - CrosshairSize, CenterY);
	Canvas.DrawRect(2*CrosshairSize + 1, 1);

	Canvas.SetPos(CenterX, CenterY - CrosshairSize);
	Canvas.DrawRect(1, 2*CrosshairSize + 1);
}


/**
 * display progress messages in center of screen
 */
simulated function DisplayProgressMessage()
{
    local int i, LineCount;
    local float FontDX, FontDY;
    local float X, Y;
    local int Alpha;
    local float TimeLeft;

    TimeLeft = PlayerOwner.ProgressTimeOut - Level.TimeSeconds;

    if ( TimeLeft >= ProgressFadeTime )
        Alpha = 255;
    else
        Alpha = (255 * TimeLeft) / ProgressFadeTime;

    LineCount = 0;

    for (i = 0; i < ArrayCount (PlayerOwner.ProgressMessage); i++)
    {
        if (PlayerOwner.ProgressMessage[i] == "")
            continue;

        LineCount++;
    }

    if ( (Level.GRI != None) && (Level.GRI.MessageOfTheDay != "") )
    {
        LineCount++;
    }

    Canvas.Font = class'Engine'.Default.LargeFont;
    Canvas.TextSize ("A", FontDX, FontDY);

    X = (0.5 * HudCanvasScale * Canvas.SizeX) + (((1.0 - HudCanvasScale) / 2.0) * Canvas.SizeX);
    Y = (0.5 * HudCanvasScale * Canvas.SizeY) + (((1.0 - HudCanvasScale) / 2.0) * Canvas.SizeY);

    Y -= FontDY * (float (LineCount) / 2.0);

    for (i = 0; i < ArrayCount (PlayerOwner.ProgressMessage); i++)
    {
        if (PlayerOwner.ProgressMessage[i] == "")
            continue;

        Canvas.DrawColor = PlayerOwner.ProgressColor[i];
        Canvas.DrawColor.A = Alpha;

        Canvas.TextSize (PlayerOwner.ProgressMessage[i], FontDX, FontDY);
        Canvas.SetPos (X - (FontDX / 2.0), Y);
        Canvas.DrawText (PlayerOwner.ProgressMessage[i]);

        Y += FontDY;
    }

    if( (Level.GRI != None) && (Level.NetMode != NM_StandAlone) )
    {
        Canvas.DrawColor = MOTDColor;
        Canvas.DrawColor.A = Alpha;

        if( Level.GRI.MessageOfTheDay != "" )
        {
            Canvas.TextSize (Level.GRI.MessageOfTheDay, FontDX, FontDY);
            Canvas.SetPos (X - (FontDX / 2.0), Y);
            Canvas.DrawText (Level.GRI.MessageOfTheDay);
            Y += FontDY;
        }
    }
}


/**
 * Displays the current action message for the level
 */
function bool DrawLevelAction()
{
	local string BigMessage;

	if (Level.LevelAction == LEVACT_None )
	{
		if ( (Level.Pauser != None) && (Level.TimeSeconds > Level.PauseDelay + 0.2) )
			BigMessage = PausedMessage; // Add pauser name?
		else
		{
			BigMessage = "";
			return false;
		}
	}
	else if ( Level.LevelAction == LEVACT_Loading )
		BigMessage = LoadingMessage;
	else if ( Level.LevelAction == LEVACT_Saving )
		BigMessage = SavingMessage;
	else if ( Level.LevelAction == LEVACT_Connecting )
		BigMessage = ConnectingMessage;
	else if ( Level.LevelAction == LEVACT_Precaching )
		BigMessage = PrecachingMessage;

	if ( BigMessage != "" )
	{
		PrintActionMessage( BigMessage );
		return true;
	}
	return false;
}

/**
 * Print a centered level action message with a drop shadow.
 */
function PrintActionMessage( string BigMessage )
{
	local float XL, YL;

	Canvas.Font = class'Engine'.Default.LargeFont;
	Canvas.bCenter = false;
	Canvas.StrLen( BigMessage, XL, YL );
	Canvas.SetPos(0.5 * (Canvas.ClipX - XL) + 1, 0.66 * Canvas.ClipY - YL * 0.5 + 1);
	Canvas.SetDrawColor(0,0,0);
	Canvas.DrawText( BigMessage, false );
	Canvas.SetPos(0.5 * (Canvas.ClipX - XL), 0.66 * Canvas.ClipY - YL * 0.5);
	Canvas.SetDrawColor(0,0,255);;
	Canvas.DrawText( BigMessage, false );
}

// DisplayBadConnectionAlert() - Warn user that net connection is bad

function DisplayBadConnectionAlert();	// Subclass Me

//=============================================================================
// Messaging.
//=============================================================================

event ConnectFailure(string FailCode, string URL)
{
	PlayerOwner.ReceiveLocalizedMessage(class'FailedConnect', class'FailedConnect'.Static.GetFailSwitch(FailCode));
}

function PlayStartupMessage(byte Stage);


// Manipulation


simulated function ClearMessage( out HudLocalizedMessage M )
{
	M.Message = None;
    M.StringFont = None;
}

simulated function PlayReceivedMessage( string S, string PName, ZoneInfo PZone )
{
	PlayerOwner.ClientMessage(S);

	if ( bMessageBeep )
		PlayerOwner.PlayBeepSound();
}

// Console Messages

simulated function Message( PlayerReplicationInfo PRI, coerce string Msg, name MsgType )
{
	if ( bMessageBeep )
		PlayerOwner.PlayBeepSound();

	if ( (MsgType == 'Say') || (MsgType == 'TeamSay') )
		Msg = PRI.PlayerName$": "$Msg;

	AddConsoleMessage(Msg,class'LocalMessage',PRI);
}



/**
 * Display current messages
 */
function DisplayConsoleMessages()
{
    local int i, j, XPos, YPos,MessageCount;
    local float XL, YL;

    for( i = 0; i < ConsoleMessageCount; i++ )
    {
        if ( ConsoleMessages[i].Text == "" )
            break;
        else if( ConsoleMessages[i].MessageLife < Level.TimeSeconds )
        {
            ConsoleMessages[i].Text = "";

            if( i < ConsoleMessageCount - 1 )
            {
                for( j=i; j<ConsoleMessageCount-1; j++ )
                    ConsoleMessages[j] = ConsoleMessages[j+1];
            }
            ConsoleMessages[j].Text = "";
            break;
        }
        else
			MessageCount++;
    }

    XPos = (ConsoleMessagePosX * HudCanvasScale * Canvas.SizeX) + (((1.0 - HudCanvasScale) / 2.0) * Canvas.SizeX);
    YPos = (ConsoleMessagePosY * HudCanvasScale * Canvas.SizeY) + (((1.0 - HudCanvasScale) / 2.0) * Canvas.SizeY);

    Canvas.Font = class'Engine'.Default.SmallFont;
    Canvas.DrawColor = ConsoleColor;

    Canvas.TextSize ("A", XL, YL);

    YPos -= YL * MessageCount+1; // DP_LowerLeft
    YPos -= YL; // Room for typing prompt

    for( i=0; i<MessageCount; i++ )
    {
        if ( ConsoleMessages[i].Text == "" )
            break;

        Canvas.StrLen( ConsoleMessages[i].Text, XL, YL );
        Canvas.SetPos( XPos, YPos );
        Canvas.DrawColor = ConsoleMessages[i].TextColor;
        Canvas.DrawText( ConsoleMessages[i].Text, false );
        YPos += YL;
    }
}

function AddConsoleMessage(string M, class<LocalMessage> MessageClass, PlayerReplicationInfo PRI)
{
	local int i;


	if( bMessageBeep && MessageClass.Default.bBeep )
		PlayerOwner.PlayBeepSound();

    for( i=0; i<ConsoleMessageCount; i++ )
    {
        if ( ConsoleMessages[i].Text == "" )
            break;
    }

    if( i == ConsoleMessageCount )
    {
        for( i=0; i<ConsoleMessageCount-1; i++ )
            ConsoleMessages[i] = ConsoleMessages[i+1];
    }

    ConsoleMessages[i].Text = M;
    ConsoleMessages[i].MessageLife = Level.TimeSeconds + MessageClass.Default.LifeTime;
    ConsoleMessages[i].TextColor = MessageClass.static.GetConsoleColor(PRI);
    ConsoleMessages[i].PRI = PRI;
}

//===============================================
// Localized Message rendering

function LocalizedMessage
( 
	class<LocalMessage>		Message, 
	PlayerReplicationInfo	RelatedPRI_1, 
	string					CriticalString, 
	int						Switch, 
	float					Position, 
	float					LifeTime, 
	int						FontSize, 
	color					DrawColor
)
{
	local int i, LocalMessagesArrayCount;

    if( Message == None )
	{
        return;
	}

	//FIXMESTEVE - move to LocalMessage class
	if( bMessageBeep && Message.default.bBeep )
		PlayerOwner.PlayBeepSound();

    if( !Message.default.bIsSpecial )
    {
	    AddConsoleMessage( CriticalString, Message,RelatedPRI_1 );
        return;
    }
	LocalMessagesArrayCount = ArrayCount(LocalMessages);
    i = LocalMessagesArrayCount;
	if( Message.default.bIsUnique )
	{
		for( i = 0; i < LocalMessagesArrayCount; i++ )
		{
		    if( LocalMessages[i].Message == Message )
                break;
		}
	}
	else if ( Message.default.bIsPartiallyUnique )
	{
		for( i = 0; i < LocalMessagesArrayCount; i++ )
		{
		    if( ( LocalMessages[i].Message == Message ) && ( LocalMessages[i].Switch == Switch ) )
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

	LocalMessages[i].Message		= Message;
	LocalMessages[i].Switch			= Switch;
	LocalMessages[i].EndOfLife		= LifeTime + Level.TimeSeconds;
	LocalMessages[i].StringMessage	= CriticalString;
	LocalMessages[i].LifeTime		= LifeTime;
	LocalMessages[i].PosY			= Position;
	LocalMessages[i].DrawColor		= DrawColor;
	LocalMessages[i].FontSize		= FontSize;
}


simulated function GetScreenCoords(float PosY, out float ScreenX, out float ScreenY, out HudLocalizedMessage Message )
{
    ScreenX = 0.5 * Canvas.ClipX;
    ScreenY = (PosY * HudCanvasScale * Canvas.ClipY) + (((1.0f - HudCanvasScale) * 0.5f) * Canvas.ClipY);

    ScreenX -= Message.DX * 0.5;
    ScreenY -= Message.DY * 0.5;
}

simulated function DrawMessage(int i, float PosY, out float DX, out float DY )
{
    local float FadeValue;
    local float ScreenX, ScreenY;

	FadeValue = (LocalMessages[i].EndOfLife - Level.TimeSeconds);
	Canvas.DrawColor = LocalMessages[i].DrawColor;
	Canvas.DrawColor.A = LocalMessages[i].DrawColor.A * (FadeValue/LocalMessages[i].LifeTime);

	Canvas.Font = LocalMessages[i].StringFont;
	GetScreenCoords( PosY, ScreenX, ScreenY, LocalMessages[i] );
	Canvas.SetPos( ScreenX, ScreenY );
	DX = LocalMessages[i].DX / Canvas.ClipX;
    DY = LocalMessages[i].DY / Canvas.ClipY;

	Canvas.DrawTextClipped( LocalMessages[i].StringMessage, false );

    LocalMessages[i].Drawn = true;
}

simulated function DisplayLocalMessages()
{
	local float PosY, DY, DX;
    local int i, j;
    local float FadeValue;
    local int FontSize;

 	Canvas.Reset();

    // Pass 1: Layout anything that needs it and cull dead stuff.

    for( i = 0; i < ArrayCount(LocalMessages); i++ )
    {
		if( LocalMessages[i].Message == None )
            break;

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
				log( "LayoutMessage("$LocalMessages[i].Message$") failed!", 'Error' );

				for( j = i; j < ArrayCount(LocalMessages) - 1; j++ )
					LocalMessages[j] = LocalMessages[j+1];
				ClearMessage( LocalMessages[j] );
				i--;
				continue;
			}
		}

		FadeValue = (LocalMessages[i].EndOfLife - Level.TimeSeconds);

		if( FadeValue <= 0.0 )
        {
	        for( j = i; j < ArrayCount(LocalMessages) - 1; j++ )
		        LocalMessages[j] = LocalMessages[j+1];
            ClearMessage( LocalMessages[j] );
            i--;
            continue;
        }
     }

    // Pass 2: Go through the list and draw each stack:

    for( i = 0; i < ArrayCount(LocalMessages); i++ )
	{
		if( LocalMessages[i].Message == None )
            break;

        if( LocalMessages[i].Drawn )
            continue;
	    PosY = LocalMessages[i].PosY;

        for( j = i; j < ArrayCount(LocalMessages); j++ )
        {
            if( LocalMessages[j].Drawn )
                continue;

            if( LocalMessages[i].PosY != LocalMessages[j].PosY )
                continue;

            DrawMessage( j, PosY, DX, DY );

            PosY += DY;
        }
    }
}

function Font GetFontSizeIndex(int FontSize)
{
	if ( FontSize == 0 )
	{
		return class'Engine'.Default.TinyFont;
	}
	else if ( FontSize == 1 )
	{
		return class'Engine'.Default.SmallFont;
	}
	else if ( FontSize == 2 )
	{
		return class'Engine'.Default.MediumFont;
	}
	else if ( FontSize == 3 )
	{
		return class'Engine'.Default.LargeFont;
	}
	else
	{
		return class'Engine'.Default.LargeFont;
	}
}

defaultproperties
{
	CollisionComponent=None
	Components.Remove(CollisionCylinder)
	Components.Remove(Sprite)
	bMessageBeep=true
	bHidden=true
	RemoteRole=ROLE_None

	WhiteColor=(R=255,G=255,B=255,A=255)
    ConsoleColor=(R=153,G=216,B=253,A=255)
	GreenColor=(R=0,G=255,B=0,A=255)
	RedColor=(R=255,G=0,B=0,A=255)

    MOTDColor=(R=255,G=255,B=255,A=255)
    ProgressFadeTime=1.0

	ConsoleMessagePosY=0.8
    HudCanvasScale=0.95

	LoadingMessage="LOADING"
	SavingMessage="SAVING"
	ConnectingMessage="CONNECTING"
	PausedMessage="PAUSED"
	PrecachingMessage=""

    ConsoleMessageCount=4
    ConsoleFontSize=5
    MessageFontOffset=0

 	bShowHud=true

	bShowAIDebug=true
	bShowPhysicsDebug=false
	bShowWeaponDebug=false
	bShowNetworkDebug=false
	bShowCollisionDebug=false
}