/**
 * WarHUD
 * Warfare Heads Up Display
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class WarHUD extends HUD
	config(Game)
	native;

/**
 * Information used to draw an icon and/or text label
 * on the HUD.
 */
struct native HUDElementInfo
{
	/** Is this element enabled? */
	var() bool bEnabled;

	/** X position of the element */
	var() float DrawX;
	/** Y position of the element */
	var() float DrawY;
	/** Should DrawX be treated as the center point for drawing? */
	var() bool bCenterX;
	/** Should DrawY be treated as the center point for drawing? */
	var() bool bCenterY;
	/** Draw color to use for the element */
	var() color DrawColor;

	// Icon specific

	/** Width of the element */
	var() int DrawW;
	/** Height of the element */
	var() int DrawH;
	/** Texture to use in drawing */
	var() Texture2D DrawIcon;

	/** Emissive Material for drawing*/
	var()	Material	DrawMaterial;

	/** UV information for DrawIcon */
	var() float		DrawU;
	var() float		DrawV;
	/** If these are set to -1, the full UVs for DrawIcon are used */
	var() float		DrawUSize;
	var() float		DrawVSize;
	/** Overall scale to use on DrawW/DrawH */
	var() float		DrawScale;

	// Label specific
	
	/** Font to use for DrawLabel */
	var() Font DrawFont;
	/** String to draw */
	var() string DrawLabel;

	structdefaults
	{
		bEnabled=true
		DrawColor=(R=255,G=255,B=255,A=255)
		DrawScale=1.f
		DrawUSize=-1
		DrawVSize=-1
	}
};

/** List of static HUD elements to be rendered */
var() config editinline array<HUDElementInfo> HUDElements;

/** Global element scale */
var float GlobalElementScale;

var	const Texture2D	XboxA_Tex;

var const array<Texture2D> HealthSymbols;

/** If true then we should draw the melee context info */
var transient	bool	bDrawMeleeInfo;

//
// Cash / Bonus / Reward system
//

const NUM_CASH_ENTRIES = 4;

struct native CashDisplayStruct
{
	var	int		DeltaCash;
	var	float	Life;
};
var	transient CashDisplayStruct	CashEntries[NUM_CASH_ENTRIES];

/** Old Cash amount, to detect changes */
var	transient	int		OldCashAmount;

/** Time in seconds delta cash is displayed */
var	const		float	DeltaCashLifeTime;

/** cash indicator relative height position */
var	config		float	CashRelPosY;

/** Current fade color */
var transient color FadeColor;

/** Last fade alpha to interpolate from */
var transient float PreviousFadeAlpha;

/** Desired fade alpha to interpolate to */
var transient float DesiredFadeAlpha;

/** Current fade alpha */
var transient float FadeAlpha;

/** Current time for fade alpha interpolation */
var transient float FadeAlphaTime;

/** Total time for fade alpha interpolation */
var transient float DesiredFadeAlphaTime;

simulated function PostBeginPlay()
{
	super.PostBeginPlay();
    SetTimer(0.35, true,'UpdateContextInfo');
}

function UpdateContextInfo()
{
	bDrawMeleeInfo = (WarPawn(PlayerOwner.Pawn) != None &&
					  WarPawn(PlayerOwner.Pawn).SuggestMeleeAttack());
}

function DrawHUDElement(HUDElementInfo element)
{
	local int drawX, drawY;
	local float drawW, drawH;
	local float elementScale;
	if (element.bEnabled)
	{
		// draw the icon if specified
		if (element.DrawIcon != None || element.DrawMaterial != None )
		{
			// figure out the element scale
			elementScale = element.DrawScale * GlobalElementScale;
			// calculate the actual draw width/height (scaled)
			drawW = element.DrawW * elementScale;
			drawH = element.DrawH * elementScale;
			// calculate the draw coordinates, handling centering as necessary
			drawX = Canvas.ClipX * element.DrawX * GlobalElementScale;
			drawY = Canvas.ClipY * element.DrawY * GlobalElementScale;
			if (element.bCenterX)
			{
				drawX -= drawW/2.f;
			}
			if (element.bCenterY)
			{
				drawY -= drawH/2.f;
			}
			// set canvas params
			Canvas.SetPos(drawX,drawY);
			Canvas.DrawColor = element.DrawColor;
			// and draw the bastard
			if( element.DrawIcon != None )
			{
				Canvas.DrawTile(element.DrawIcon,
								drawW,drawH,
								element.DrawU,element.DrawV,
								element.DrawUSize,element.DrawVSize);
			}
			else if( element.DrawMaterial != None )
			{
				Canvas.DrawMaterialTile( element.DrawMaterial, drawW, drawH, element.DrawU, element.DrawV, element.DrawUSize, element.DrawVSize );
			}
		}
		// draw the label if specified
		if (element.DrawLabel != "")
		{
			Canvas.Font = element.DrawFont;
			Canvas.TextSize(element.DrawLabel,drawW,drawH);
			// calculate the draw coordinates, handling centering as necessary
			drawX = Canvas.ClipX * element.DrawX * GlobalElementScale;
			drawY = Canvas.ClipY * element.DrawY * GlobalElementScale;
			if (element.bCenterX)
			{
				drawX -= drawW/2.f;
			}
			if (element.bCenterY)
			{
				drawY -= drawH/2.f;
			}
			Canvas.DrawColor = element.DrawColor;
			Canvas.SetPos(drawX,drawY);
			Canvas.DrawText(element.DrawLabel,false);
		}
	}
}

/**
 * Renders the contents of HUDElements[].
 */
function DrawHUDElements()
{
}

/**
 * The Main Draw loop for the hud.  Get's called before any messaging.  Should be subclassed
 */
function DrawHUD()
{
	local int		idx;

	super.DrawHUD();

	if(	WarPC(PlayerOwner).bDebugCover )
	{
		DrawCoverDebug();
	}
	if ( WarPC(PlayerOwner).bDebugAI )
	{
		DrawAIDebug();
	}

	// iterate through each element
	for (idx = 0; idx < HUDElements.Length; idx++)
	{
		DrawHUDElement(HUDElements[idx]);
	}
	// check if assess mode is active, and if so handle all related drawing
	DrawAssessMode();
	if (PlayerOwner.Pawn != None)
	{
		DrawShield();
		DrawHealth();
		DrawMeleeInfo();
		DrawCash();
	}
}

/** Draw Cash status on HUD */
function DrawCash()
{
	local int		CurrentCash, Idx;
	local string	Text;
	local float		XL, YL, XOffset, YOffset, LeftMargin;

	LeftMargin	= 16 * RatioX;
	CurrentCash =  WarPC(PlayerOwner).Cash;
	// If cash amount is different, add a new entry with the delta
	if( OldCashAmount != CurrentCash )
	{
		AddCashUpdateEntry( CurrentCash - OldCashAmount );
		OldCashAmount = CurrentCash;
	}

	// update life time of first cash entry
	if( CashEntries[Idx].Life > 0.f )
	{
		CashEntries[Idx].Life -= RenderDelta;

		// shift entries if first one has expired
		if( CashEntries[Idx].Life < 0.f )
		{
			for (Idx=1; Idx<NUM_CASH_ENTRIES; Idx++)
			{
				CashEntries[Idx-1] = CashEntries[Idx];
			}
			CashEntries[NUM_CASH_ENTRIES-1].Life = 0.f;
		}
	}

	// Draw current cash amount
	Canvas.Font = class'Engine'.Default.MediumFont;
	SetCashColor( CurrentCash, DeltaCashLifeTime );
	Text = "Cash: $" $ CurrentCash;
	Canvas.StrLen( Text, XL, YL );
	XOffset = SizeX - XL - LeftMargin;
	YOffset = SizeY*CashRelPosY - YL;
	Canvas.SetPos( XOffset, YOffset );
	Canvas.DrawText( Text, false );

	// Draw list of delta cash entries
	for (Idx=0; Idx<NUM_CASH_ENTRIES; Idx++)
	{
		if( CashEntries[Idx].Life <= 0.f )
		{
			break;
		}

		if( CashEntries[Idx].DeltaCash < 0.f )
		{
			Text = "-";
		}
		else
		{
			Text = "+";
		}
		
		SetCashColor( CashEntries[Idx].DeltaCash, CashEntries[Idx].Life );
		Text = Text $ int(Abs(CashEntries[Idx].DeltaCash));
		Canvas.StrLen( Text, XL, YL );
		YOffset -= YL;
		XOffset = SizeX - XL - LeftMargin;
		Canvas.SetPos( XOffset, YOffset );
		Canvas.DrawText( Text, false );
	}
}

/** Add a cash reward entry in the display list */
function AddCashUpdateEntry( int DeltaCash )
{
	local int	NewEntryIdx, Idx;

	// If delta cash list is full, then force a shift and enter at last spot
	if( CashEntries[NUM_CASH_ENTRIES-1].Life > 0.f )
	{
		for (Idx=1; Idx<NUM_CASH_ENTRIES; Idx++)
		{
			CashEntries[Idx-1] = CashEntries[Idx];
		}
		NewEntryIdx = NUM_CASH_ENTRIES - 1;;
	}
	else
	{
		// other wise, find free spot and add our entry there
		for (Idx=0; Idx<NUM_CASH_ENTRIES; Idx++)
		{
			if( CashEntries[Idx].Life <= 0.f )
			{
				NewEntryIdx = Idx;
				break;
			}
		}
	}

	// add new entry at best spot.
	CashEntries[NewEntryIdx].DeltaCash	= DeltaCash;
	CashEntries[NewEntryIdx].Life		= DeltaCashLifeTime;
}

/** Set Cash drawing color, depending on sign and life of cash entry */
function SetCashColor( int CashAmount, float Life )
{
	// color depending on sign
	if( CashAmount > 0 )
	{
		Canvas.DrawColor = GreenColor;
	}
	else
	{
		Canvas.DrawColor = RedColor;
	}

	// Fade out based on life
	if( Life < 1.f )
	{
		Canvas.DrawColor.A = 255 * Life;
	}
}

/**
 * Suggest context sensitive action when player can perform a melee move to kill an enemy
 */
function DrawMeleeInfo()
{
	if (bDrawMeleeInfo)
	{
		DrawContextInfo( "Press 'USE' to melee" );
	}
}

/**
 * Draw Context sensitive action
 * Draws Xbox "A" button, with associated context sensitive hint.
 *
 * @param	Context Message to display
 */
function DrawContextInfo( String ContextMessage )
{
	local float	XL, YL, XO, YO, tmp;

	XO	= SizeX*0.55;
	YO	= SizeY*0.90;
	XL	= XboxA_Tex.SizeX * 0.15 * RatioX;
	YL	= XboxA_Tex.SizeY * 0.15 * RatioY;

	Canvas.SetPos( XO, YO - YL*0.5);
	Canvas.DrawTile( XboxA_Tex, XL, YL, 0, 0, XboxA_Tex.SizeX, XboxA_Tex.SizeY );

	Canvas.Font = class'Engine'.Default.SmallFont;
	Canvas.DrawColor = WhiteColor;

	Canvas.StrLen( ContextMessage, tmp, YL );
	Canvas.SetPos( XO + XL*1.5, YO - YL*0.5);
	Canvas.DrawText( ContextMessage );
}

/**
 * Special HUD for Engine demo
 */
function DrawEngineHUD();

function DrawCoverDebug()
{
	local WarPC		WPC;
	local string	Text;

	WPC = WarPC(PlayerOwner);

	Canvas.Font = class'Engine'.Default.TinyFont;
	Canvas.SetDrawColor(255,64,128,255);
	Text = "Pawn CoverType:" $ WPC.GetPawnCoverType() @ "CoverAction:" $ WPC.GetPawnCoverAction() @ "CoverDirection:" $ WPC.GetCoverDirection();
	Canvas.SetPos( 10, CenterY * 0.5 );
	Canvas.DrawText( Text );

	Text = "State:" $ WPC.GetStateName() @ "HoldCount:" $ WPC.CoverTransitionCountHold @ "CountDown:" $ WPC.CoverTransitionCountDown;
	Canvas.DrawText( Text );

	Text = "PrimaryCover:" $ WPC.PrimaryCover;
	if( WPC.PrimaryCover != None )
		Text = Text @ "CT:" $ WPC.PrimaryCover.CoverType;
	Canvas.DrawText( Text );

	Text = "SecondaryCover:" $ WPC.SecondaryCover;
	if( WPC.SecondaryCover != None )
		Text = Text @ "CT:" $ WPC.SecondaryCover.CoverType;
	Canvas.DrawText( Text );

	if( WPC.PrimaryCover != None )
	{
		Text = "LeftNode:" $ WPC.PrimaryCover.LeftNode;
		if( WPC.PrimaryCover.LeftNode != None )
			Text = Text @ "CT:" $ WPC.PrimaryCover.LeftNode.CoverType;
		Canvas.DrawText( Text );

		Text = "RightNode:" $ WPC.PrimaryCover.RightNode;
		if( WPC.PrimaryCover.RightNode != None )
			Text = Text @ "CT:" $ WPC.PrimaryCover.RightNode.CoverType;
		Canvas.DrawText( Text );
	}

	Text = "RawJoyUp:"$WarPlayerInput(WPC.PlayerInput).RawJoyUp;
	Canvas.DrawText( Text );

	Text = "RawJoyRight:"$WarPlayerInput(WPC.PlayerInput).RawJoyRight;
	Canvas.DrawText( Text );

	Text = "RawJoyLookRight:"$WarPlayerInput(WPC.PlayerInput).RawJoyLookRight;
	Canvas.DrawText( Text );

	Text = "RawJoyLookUp:"$WarPlayerInput(WPC.PlayerInput).RawJoyLookUp;
	Canvas.DrawText( Text );
}

function DrawAIDebug()
{
	local Controller player;
	local WarAIController ai;
	local vector cameraLoc;
	local rotator cameraRot;
	local WarPC pc;
	pc = WarPC(PlayerOwner);
	pc.GetPlayerViewPoint(cameraLoc,cameraRot);
	for (player = Level.ControllerList; player != None; player = player.NextController)
	{
		ai = WarAIController(player);
		if (ai != None &&
			ai.Pawn != None &&
			(ai.Pawn.Location - cameraLoc) dot vector(cameraRot) > 0.f)
		{
			ai.DrawDebug(self);
		}
	}
}

/**
 * Checks the owning player controller to see if assess mode is active,
 * and if so renders all objects in the interactable list.
 */
simulated function DrawAssessMode()
{
	local WarPC pc;
	local int idx;
	local Trigger trig;
	local vector screenLoc, screenLocTL, screenLocBR, X, Y, Z, cameraLoc;
	local rotator cameraRot;
	local Texture2D tex;
	local float texScale, colHeight, colRadius;
	local SeqEvent_Used evt;
	pc = WarPC(PlayerOwner);
	if (pc != None &&
		pc.bAssessMode)
	{
		pc.GetPlayerViewPoint(cameraLoc, cameraRot);
		// draw text to indicate assess mode is active
		Canvas.Font = class'Engine'.Default.LargeFont;
		Canvas.SetDrawColor(184,203,235,255);
		Canvas.SetPos(20, Canvas.ClipY-60);
		Canvas.DrawText("ASSESS MODE",false);
		Canvas.Font = class'Engine'.Default.SmallFont;
		// iterate through the list of interactables
		for (idx = 0; idx < pc.InteractableList.Length; idx++)
		{
			trig = pc.InteractableList[idx].InteractTrigger;
			evt = pc.InteractableList[idx].Event;
			if (trig != None &&
				evt != None)
			{
				// make sure this object is visible on screen
				screenLoc = Canvas.Project(trig.Location);
				if (screenLoc.X >= 0 &&
					screenLoc.X < Canvas.ClipX &&
					screenLoc.Y >= 0 &&
					screenLoc.Y < Canvas.ClipY)
				{
					// draw the object
					tex = evt.InteractIcon;
					colHeight = trig.CylinderComponent.CollisionHeight;
					colRadius = trig.CylinderComponent.COllisionRadius;
					// figure out the actor size so that we can get a useful scale
					//GetAxes(rotator(trig.Location - cameraLoc),X,Y,Z);
					GetAxes(cameraRot,X,Y,Z);
					screenLocTL = Canvas.Project(trig.Location + colHeight * Z - colRadius * Y);
					screenLocBR = Canvas.Project(trig.Location - colHeight * Z + colRadius * Y);
					texScale = abs(screenLocTL.X-screenLocBR.X)/(colRadius*2.3f);
					if (tex != None)
					{
						if (pc.InteractableList[idx].bUsuable)
						{
							Canvas.SetDrawColor(150,150,150,255);
						}
						else
						{
							Canvas.SetDrawColor(184,203,235,255);
						}
						Canvas.SetPos(screenLoc.X-(tex.SizeX/2.f*texScale),screenLoc.Y-(tex.SizeY/2.f*texScale));
						Canvas.DrawIcon(tex,texScale);
					}
					// draw the interact text
					if (evt.InteractText != "")
					{
						if (pc.InteractableList[idx].bUsuable)
						{
							Canvas.SetDrawColor(255,255,255,255);
						}
						else
						{
							Canvas.SetDrawColor(184,203,235,255);
						}
						if (tex != None)
						{
							Canvas.SetPos(screenLoc.X-(tex.SizeX/2.f*texScale),screenLoc.Y-(tex.SizeY/2.f*texScale) - 32.f);
						}
						else
						{
							Canvas.SetPos(screenLoc.X,screenLoc.Y);
						}
						Canvas.DrawText(evt.InteractText,false);
					}
				}
			}
		}
	}
}

/**
 * Draw player health
 */
simulated function DrawHealth()
{
	local int		drawX, drawY, barCnt, idx;
	local float		drawW, drawH, XL, YL;
	local float		healthPct;
	local string	HUDText;

	drawX = Canvas.ClipX - 32;
	drawY = 40;

	Canvas.Font = class'Engine'.Default.SmallFont;
	Canvas.DrawColor = MakeColor(255,255,255,255);
	HUDText = "Health";
	Canvas.TextSize( HUDText, XL, YL );
	Canvas.SetPos( DrawX - XL, DrawY );
	Canvas.DrawText( HUDText );

	drawX	= Canvas.ClipX - 64 - XL;
	drawW = 16 * 0.8f;
	drawH = 32 * 0.8f;
	barCnt = 20;

	healthPct = PlayerOwner.Pawn.Health/float(PlayerOwner.Pawn.default.Health);
	for (idx = 0; idx < barCnt; idx++)
	{
		Canvas.SetPos(drawX - idx * 6, drawY);
		if (idx/float(barCnt) > healthPct)
		{
			Canvas.SetDrawColor(238,23,35,255);
			Canvas.DrawTile(Texture'WarfareHudGFX.HUD_Weapon_T_Ammo_Off',
							drawW,drawH,
							0,0,
							16,32);
		}
		else
		{
			Canvas.SetDrawColor(50,243,15,255);
			Canvas.DrawTile(Texture'WarfareHudGFX.HUD_Weapon_T_Ammo_On',
							drawW,drawH,
							0,0,
							16,32);
		}
	}
}

/** Draw player shield status */
simulated function DrawShield()
{
	local int		drawX, drawY, barCnt, idx;
	local float		drawW, drawH, XL, YL;
	local float		ShieldPct;
	local string	HUDText;

	if( WarPawn(PlayerOwner.Pawn) == None ||
		WarPawn(PlayerOwner.Pawn).default.ShieldAmount == 0 )
	{
		return;
	}

	drawX	= Canvas.ClipX - 32;
	drawY	= 20;

	Canvas.Font = class'Engine'.Default.SmallFont;
	Canvas.DrawColor = MakeColor(255,255,255,255);
	HUDText = "Shield";
	Canvas.TextSize( HUDText, XL, YL );
	Canvas.SetPos( DrawX - XL, DrawY );
	Canvas.DrawText( HUDText );

	drawX	= Canvas.ClipX - 64 - XL;
	drawW	= 16 * 0.8f;
	drawH	= 32 * 0.8f;
	barCnt	= 20;
	
	ShieldPct = WarPawn(PlayerOwner.Pawn).ShieldAmount / WarPawn(PlayerOwner.Pawn).default.ShieldAmount;

	for( idx=0; idx<barCnt; idx++ )
	{
		Canvas.SetPos(drawX - idx * 6 , drawY);
		if( idx/float(barCnt) > ShieldPct )
		{
			Canvas.SetDrawColor(238,23,35,255);
			Canvas.DrawTile(Texture'WarfareHudGFX.HUD_Weapon_T_Ammo_Off',
							drawW,drawH,
							0,0,
							16,32);
		}
		else
		{
			Canvas.SetDrawColor(50,243,15,255);
			Canvas.DrawTile(Texture'WarfareHudGFX.HUD_Weapon_T_Ammo_On',
							drawW,drawH,
							0,0,
							16,32);
		}
	}
}

/**
 * Handles rendering our current fade if any.
 */
simulated event PostRender()
{
	Super.PostRender();
	if (FadeAlpha != 0.f)
	{
		Canvas.SetPos(0,0);
		Canvas.SetDrawColor(FadeColor.R,FadeColor.G,FadeColor.B,FadeAlpha);
		Canvas.DrawTile(Texture'WhiteSquareTexture',Canvas.ClipX,Canvas.ClipY,0,0,2,2);
	}
}

/**
 * Overridden to handle interpolating current fade if any.
 */
function Tick(float DeltaTime)
{
	if (FadeAlphaTime != DesiredFadeAlphaTime)
	{
		if (FadeAlphaTime > DesiredFadeAlphaTime)
		{
			FadeAlphaTime = FMax(FadeAlphaTime - DeltaTime, DesiredFadeAlphaTime);
		}
		else
		{
			FadeAlphaTime = FMin(FadeAlphaTime + DeltaTime, DesiredFadeAlphaTime);
		}
		FadeAlpha = lerp(FadeAlphaTime/DesiredFadeAlphaTime,PreviousFadeAlpha,DesiredFadeAlpha);
	}
	Super.Tick(DeltaTime);
}

function bool DrawLevelAction()
{
	if (Level.LevelAction == LEVACT_Loading)
	{
		Canvas.SetPos(16, Canvas.ClipY - 48);
		Canvas.SetDrawColor(200,200,200,180);
		Canvas.DrawIcon(Texture'WarfareHUDGfx.HUD_Health_T_MoraleGEAR',0.25f);
		return true;
	}
	else
	{
		return Super.DrawLevelAction();
	}
}

	/*
	HUDElements(0)=(bEnabled=True,DrawX=0.823000,DrawY=0.205000,bCenterX=True,bCenterY=True,DrawColor=(B=255,G=255,R=255,A=128),DrawW=128,DrawH=128,DrawIcon=Texture2D'WarfareHudGFX.HUD.HUD_Health_T_MoraleGEAR',DrawU=0,DrawV=0,DrawUSize=128,DrawVSize=128,DrawScale=0.400000,DrawFont=None,DrawLabel="")
	HUDElements(1)=(bEnabled=True,DrawX=0.820000,DrawY=0.200000,bCenterX=True,bCenterY=True,DrawColor=(B=255,G=255,R=255,A=255),DrawW=300,DrawH=256,DrawIcon=Texture2D'WarfareHudGFX.HUD.HUD_Health_T_MainLayer',DrawU=0,DrawV=0,DrawUSize=300,DrawVSize=256,DrawScale=0.600000,DrawFont=None,DrawLabel="")
	HUDElements(2)=(bEnabled=True,DrawX=0.710000,DrawY=0.123000,bCenterX=False,bCenterY=False,DrawColor=(B=255,G=255,R=255,A=255),DrawW=0,DrawH=0,DrawIcon=None,DrawU=0,DrawV=0,DrawUSize=-1,DrawVSize=-1,DrawScale=1.000000,DrawFont=Font'EngineFonts.SmallFont',DrawLabel="0")
	HUDElements(3)=(bEnabled=True,DrawX=0.814000,DrawY=0.133500,bCenterX=False,bCenterY=False,DrawColor=(B=235,G=203,R=184,A=200),DrawW=32,DrawH=64,DrawIcon=Texture2D'WarfareHudGFX.HUD.HUD_Health_T_On_fill',DrawU=0,DrawV=0,DrawUSize=32,DrawVSize=64,DrawScale=0.620000,DrawFont=None,DrawLabel="")
	HUDElements(4)=(bEnabled=True,DrawX=0.814000,DrawY=0.240000,bCenterX=False,bCenterY=False,DrawColor=(B=235,G=203,R=184,A=200),DrawW=32,DrawH=64,DrawIcon=Texture2D'WarfareHudGFX.HUD.HUD_Health_T_On_fill',DrawU=0,DrawV=0,DrawUSize=32,DrawVSize=64,DrawScale=0.620000,DrawFont=None,DrawLabel="")
	HUDElements(5)=(bEnabled=True,DrawX=0.765000,DrawY=0.198000,bCenterX=False,bCenterY=False,DrawColor=(B=255,G=255,R=255,A=255),DrawW=64,DrawH=32,DrawIcon=Texture2D'WarfareHudGFX.HUD.HUD_Health_T_On_fill_rot',DrawU=0,DrawV=0,DrawUSize=64,DrawVSize=32,DrawScale=0.640000,DrawFont=None,DrawLabel="")
	HUDElements(6)=(bEnabled=True,DrawX=0.821000,DrawY=0.198000,bCenterX=False,bCenterY=False,DrawColor=(B=235,G=203,R=184,A=200),DrawW=64,DrawH=32,DrawIcon=Texture2D'WarfareHudGFX.HUD.HUD_Health_T_On_fill_rot',DrawU=0,DrawV=0,DrawUSize=-64,DrawVSize=32,DrawScale=0.620000,DrawFont=None,DrawLabel="")
	*/

defaultproperties
{
	DeltaCashLifeTime=4
	CashRelPosY=0.98

	XboxA_Tex=Texture2D'HUD_Demo.HUD_XButtonTest_Tex'
	HealthSymbols(0)=Texture2D'HUD_Demo.new_medsymbol'
	HealthSymbols(1)=Texture2D'HUD_Demo.new_medsymbol_1'
	HealthSymbols(2)=Texture2D'HUD_Demo.new_medsymbol_2'
	HealthSymbols(3)=Texture2D'HUD_Demo.new_medsymbol_3'
	HealthSymbols(4)=Texture2D'HUD_Demo.new_medsymbol_4'
	HealthSymbols(5)=Texture2D'HUD_Demo.new_medsymbol_5'
	HealthSymbols(6)=Texture2D'HUD_Demo.new_medsymbol_6'
	HealthSymbols(7)=Texture2D'HUD_Demo.new_medsymbol_7'
	HealthSymbols(8)=Texture2D'HUD_Demo.new_medsymbol_8'

	GlobalElementScale=1.f;
}