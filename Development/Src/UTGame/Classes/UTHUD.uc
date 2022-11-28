/**
 * UTHUD
 * UT Heads Up Display
 *
 * Created by:	Steven Polge
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class UTHUD extends HUD
	config(Game);

var() globalconfig bool bMessageBeep;
var() globalconfig bool bShowWeaponInfo;
var() globalconfig bool bShowPersonalInfo;
var() globalconfig bool bShowPoints;
var() globalconfig bool bShowWeaponBar;
var() globalconfig bool bCrosshairShow;
var() globalconfig bool bShowPortrait;
var globalconfig bool bNoEnemyNames;
var globalconfig bool bSmallWeaponBar;

var	const	Texture2D	XboxA_Tex;

var const	Texture2D	HudTexture;

var float LastPickupTime, LastAmmoPickupTime, LastWeaponPickupTime, LastHealthPickupTime, LastArmorPickupTime;

struct DigitData
{
	var	int	left,top,width,height;
};

var DigitData 	DigitInfo[10];

var bool bShowTempScoreboard;

simulated function PostBeginPlay()
{
	super.PostBeginPlay();
    SetTimer(1.0, true);
}


exec function GrowHUD()
{
	if( !bShowWeaponInfo )
        bShowWeaponInfo = true;
    else if( !bShowPersonalInfo )
        bShowPersonalInfo = true;
    else if( !bShowPoints )
        bShowPoints = true;
    else if ( !bShowWeaponBar )
		bShowWeaponBar = true;
	else if ( bSmallWeaponBar )
		bSmallWeaponBar = false;
	SaveConfig();
}

exec function ShrinkHUD()
{
	if ( !bSmallWeaponBar )
		bSmallWeaponBar = true;
	else if ( bShowWeaponBar )
		bShowWeaponBar = false;
    else if( bShowPoints )
        bShowPoints = false;
    else if( bShowPersonalInfo )
        bShowPersonalInfo = false;
    else if( bShowWeaponInfo )
        bShowWeaponInfo = false;
	SaveConfig();
}

/* FIXMESTEVE
function DrawCustomBeacon(Canvas C, Pawn P, float ScreenLocX, float ScreenLocY)
{
	local texture BeaconTex;
	local float XL,YL;

	BeaconTex = UTPlayerOwner.TeamBeaconTexture;
	if ( (BeaconTex == None) || (P.PlayerReplicationInfo == None) )
		return;

	if ( P.PlayerReplicationInfo.Team != None )
		C.DrawColor = class'PlayerController'.Default.TeamBeaconTeamColors[P.PlayerReplicationInfo.Team.TeamIndex];
	else
		C.DrawColor = class'PlayerController'.Default.TeamBeaconTeamColors[0];

	C.StrLen(P.PlayerReplicationInfo.PlayerName, XL, YL);
	C.SetPos(ScreenLocX - 0.5*XL , ScreenLocY - 0.125 * BeaconTex.VSize - YL);
	C.DrawText(P.PlayerReplicationInfo.PlayerName,true);

	C.SetPos(ScreenLocX - 0.125 * BeaconTex.USize, ScreenLocY - 0.125 * BeaconTex.VSize);
	C.DrawTile(BeaconTex,
		0.25 * BeaconTex.USize,
		0.25 * BeaconTex.VSize,
		0.0,
		0.0,
		BeaconTex.USize,
		BeaconTex.VSize);
}
*/

function DisplayHit(vector HitDir, int Damage, class<DamageType> damageType)
{
}

function DrawHUD()
{
	super.DrawHUD();

	if ( PlayerOwner.Pawn != None )
	{
		DrawHealth();
	}
	DrawScore();
}

/**
 * Helper to Scale value based on HUD Scale
 */
final function float ScaleX( float X )
{
	return X * RatioX;
}

/**
 * Helper to Scale value based on HUD Scale
 */
final function float ScaleY( float Y )
{
	return Y * RatioY;
}

/**
 * Draw Context sensitive action
 *
 * @param	C, Canvas
 * @param	Context Message to display
 */
function DrawContextInfo( Canvas C, String ContextMessage )
{
	local float	XL, YL, XO, YO, tmp;

	XO	= C.ClipX*0.55;
	YO	= C.ClipY*0.90;
	XL	= ScaleX(XboxA_Tex.SizeX * 0.15);
	YL	= ScaleY(XboxA_Tex.SizeY * 0.15);

	C.SetPos( XO, YO - YL*0.5);
	C.DrawTile( XboxA_Tex, XL, YL, 0, 0, XboxA_Tex.SizeX, XboxA_Tex.SizeY );

	C.Font = class'Engine'.Default.MediumFont;
	C.DrawColor = WhiteColor;

	C.StrLen( ContextMessage, tmp, YL );
	C.SetPos( XO + XL*1.5, YO - YL*0.5);
	C.DrawText( ContextMessage );
}

/**
 * Special HUD for Engine demo
 */
function DrawEngineHUD()
{
	local	float	xl,yl,Y;
	local	String	myText;

	// Draw Copyright Notice
	Canvas.SetDrawColor(255, 255, 255, 255);
	myText = "UNREAL TOURNAMENT";
	Canvas.Font = class'Engine'.Default.SmallFont;
	Canvas.StrLen(myText, XL, YL);
	Y = YL*1.67;
	Canvas.SetPos( (Canvas.ClipX/2) - (XL/2), YL*0.5);
	Canvas.DrawText(myText, true);

	Canvas.SetDrawColor(200,200,200,255);
	myText = "Copyright (C) 2004, Epic Games Inc.";
	Canvas.Font = class'Engine'.Default.TinyFont;
	Canvas.StrLen(myText, XL, YL);
	Canvas.SetPos( (Canvas.ClipX/2) - (XL/2), Y);
	Canvas.DrawText(myText, true);
}

simulated function DrawHealth()
{
	local float scale, left, top, xl, yl, Width, Height;
	local int H;

	scale = Canvas.ClipX / 1024;

	H = PlayerOwner.Pawn.Health;

	SizeInt(H,xl,yl,Scale);
	Width = XL + (42*Scale) + (30*Scale);
	Height = (63*Scale);

	Left = 5;
	Top  = Canvas.ClipY - Height - 5;

	Canvas.SetPos(Left,Top);
	Canvas.SetDrawColor(180,64,64,255);
	Canvas.DrawTile(HudTexture,Width,Height,3,113,160,48);

	Top  += (10*Scale);
	Left += (10*Scale);

	Canvas.SetDrawColor(255,255,255,255);
	Canvas.SetPos(Left,Top);
	Canvas.DrawTile(HudTexture,42*Scale,43*Scale,78,169,42,43);

	if (H<30)
		Canvas.SetDrawColor(255,255,0,255);
	else
		Canvas.SetDrawColor(255,255,255,255);

	Left = Left + (55*Scale);
	Top  = Top += (10*Scale);

	DrawInt(H,Left,Top,Scale);

}

function string FormatTime()
{
	local int Minutes, Hours, Seconds;
	local string Result;

	if ( Level.GRI.TimeLimit != 0 )
		Seconds =  Level.GRI.RemainingTime;
	else
		Seconds =  Level.GRI.ElapsedTime;

	if( Seconds > 3600 )
    {
        Hours = Seconds / 3600;
        Seconds -= Hours * 3600;
		Result = string(Hours)$":";
	}
	Minutes = Seconds / 60;
    Seconds -= Minutes * 60;

	if ( Minutes < 10 )
	{
		if ( Hours == 0 )
			Result = Result$" ";
		else
			Result = Result$"0";
	}
	Result = Result$Minutes$":";

	if ( Seconds < 10 )
	{
		Result = Result$"0";
	}
	Result = Result$Seconds;
	return Result;
}

function DrawScore()
{
    local float		xl,yl, left, top;
    local int		i;

	if ( Level.GRI == None )
	{
		return;
	}

	Canvas.DrawColor = WhiteColor;
	Canvas.Font = class'Engine'.Default.SmallFont;
	Canvas.Strlen("Score", xl, yl);
	left	= 0.01 * Canvas.ClipX;
    Top		= 0.01 * Canvas.ClipY;

	Canvas.SetPos(Left, Top);

	Canvas.DrawColor = WhiteColor;
	Canvas.DrawTile(HudTexture,34,34,150,356,34,34);
	Canvas.SetPos(Left+40, Top+17-(YL/2));
	Canvas.DrawColor.R = 0;
	Canvas.DrawText(FormatTime());
	Top += 64;
	Canvas.DrawColor = WhiteColor;

	if (bShowTempScoreboard)
	{
		for ( i=0; i<Level.GRI.PRIArray.Length; i++ )
		{
			Canvas.SetPos(Left, Top);
			if ( Level.GRI.PRIArray[i] == PlayerOwner.PlayerReplicationInfo )
				Canvas.DrawColor.B = 0;
			else
				Canvas.DrawColor = WhiteColor;
			Canvas.DrawText(Level.GRI.PRIArray[i].PlayerName$": "$int(Level.GRI.PRIArray[i].Score));
			Top += yl;
		}
	}
}

exec function ShowScores()
{
	bShowTempScoreboard = !bShowTempScoreboard;
}

simulated function DrawInt(int Value,  float X, float Y, float Scale)
{
	local string s;
	local int i,v;
	local DigitData D;
	s = string(Value);

	for (i=0;i<len(s);i++)
	{
		v = int( Mid(s,i,1) );
		D = DigitInfo[v];

		Canvas.SetPos(X,Y);
		Canvas.DrawTile(HudTexture,d.Width*Scale,d.Height*Scale,D.Left,D.Top,d.Width,d.Height);
		X+=D.Width*Scale;
	}
}

simulated function SizeInt(int Value,  out float xl, out float yl, float Scale)
{
	local string s;
	local int i,v,m;
	local DigitData D;
	s = string(Value);

	m=0;
	for (i=0;i<len(s);i++)
	{
		v = int( Mid(s,i,1) );
		D = DigitInfo[v];
		xl+=D.Width*Scale;
		if (d.Height>m)
			m=d.height;
	}
	yl = m*Scale;
}


defaultproperties
{
	XboxA_Tex=Texture2D'HUD_Demo.HUD_XButtonTest_Tex'
	HudTexture=Texture2D'T_UTHudGraphics.Textures.UTOldHUD'

	DigitInfo(0)=(left=4,top=6,width=40,height=26)
	DigitInfo(1)=(left=47,top=6,width=28,height=26)
	DigitInfo(2)=(left=83,top=6,width=36,height=26)
	DigitInfo(3)=(left=122,top=6,width=36,height=26)
	DigitInfo(4)=(left=160,top=6,width=36,height=26)
	DigitInfo(5)=(left=200,top=6,width=36,height=26)
	DigitInfo(6)=(left=239,top=6,width=36,height=26)
	DigitInfo(7)=(left=277,top=6,width=36,height=26)
	DigitInfo(8)=(left=317,top=6,width=36,height=26)
	DigitInfo(9)=(left=357,top=6,width=36,height=26)

}
