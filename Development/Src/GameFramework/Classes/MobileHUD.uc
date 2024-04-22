/**
* MobileHUD
* Extra floating always on top HUD for touch screen devices
*
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class MobileHUD extends HUD
	native
	config(Game)
	dependson(MobilePlayerInput);

/** If true, we want to display the normal hud.  We need a third variable to support hiding the hud completly yet still supporting the ShowHud command */
var config bool bShowGameHud;	

/** If true, we want to display the mobile hud (ie: Input zones. etc) */
var config bool bShowMobileHud;

/** Allow for enabling/disabling the Mobile HUD stuff on non-mobile platforms */
var globalconfig bool bForceMobileHUD;

/** Texture to fill the zones with */

var Texture2D JoystickBackground;
var TextureUVs JoystickBackgroundUVs;
var Texture2D JoystickHat;
var TextureUVs JoystickHatUVs;

var Texture2D ButtonImages[2];
var TextureUVs ButtonUVs[2];
var font ButtonFont;
var color ButtonCaptionColor;

var Texture2D TrackballBackground;
var TextureUVs TrackballBackgroundUVs;
var Texture2D TrackballTouchIndicator;
var TextureUVs TrackballTouchIndicatorUVs;

var Texture2D SliderImages[4];
var TextureUVs SliderUVs[4];

/** If true, this hud will display the device tilt */
var config bool bShowMobileTilt;

/** Hold the position data for displaying the tilt */
var config float MobileTiltX, MobileTiltY, MobileTiltSize;

/** If true, display debug information regarding the touches */
var config bool bDebugTouches;

/** If true, debug info about the various mobile input zones will be displayed */
var config bool bDebugZones;

/** If true, debug info about a mobile input zone will be displayed, but only on presses */
var config bool bDebugZonePresses;

/** If this is true, we will display debug information regarding motion data */
var config bool bShowMotionDebug;

var array<SeqEvent_HudRender> KismetRenderEvents;

/**
* Create a list of actors needing post renders for.  Also Create the Hud Scene
*/
simulated function PostBeginPlay()
{

	super.PostBeginPlay();

	// If we are on the actual mobile platform or we are forcing the issue, then
	// figure out if we want to show the game hud
	if (WorldInfo.IsConsoleBuild(CONSOLE_Mobile) ||	bForceMobileHUD)
	{
	}
	else	// Not a mobile game so make sure we don't restrict the hud
	{
		bShowGameHud = true;
	}

	// Find any HudRender events that need to be tracked 
	RefreshKismetLinks();
}

/**
 * The start of the rendering chain.
 */
function PostRender()
{
	local MobilePlayerInput MPI;
	super.PostRender();

	// If no secondary screen is active, render input zones and mobile menus. Otherwise,
	// they'll be drawn as part of the secondary viewport client PostRender.
	if (class'GameEngine'.static.HasSecondaryScreenActive() == false)
	{
		if (ShowMobileHud())
		{
			DrawInputZoneOverlays();
		}
		RenderMobileMenu();
	}

	if (bShowMotionDebug)
	{
		MPI = MobilePlayerInput(PlayerOwner.PlayerInput);
		if (MPI != none)
		{
			Canvas.SetDrawColor(255,255,255,255);
			Canvas.SetPos(0,70);
			DrawMobileDebugString(0,90,"[Mobile Motion]");
			DrawMobileDebugString(0,110,"Attitude: Pitch=" $ MPI.aTilt.X @ "Yaw=" $ MPI.aTilt.Y @ "Roll=" $ MPI.aTilt.Z);
			DrawMobileDebugString(0,130,"Rotation:" @ MPI.aRotationRate.X @ MPI.aRotationRate.Y @ MPI.aRotationRate.Z);
			DrawMobileDebugString(0,150,"Gravity:" @ MPI.aGravity.X @ MPI.aGravity.Y @ MPI.aGravity.Z);
			DrawMobileDebugString(0,170,"Accleration:" @ MPI.aAcceleration.X @ MPI.aAcceleration.Y @ MPI.aAcceleration.Z);
		}
	}

	RenderKismetHud();

	// @DEBUG - Remove if you wish to see all touch events
	//MobilePlayerInput(PlayerOwner.PlayerInput).DrawTouchDebug(Canvas);

}

function DrawMobileDebugString(float XPos, float YPos,string Str)
{
	Canvas.SetDrawColor(0,0,0,255);
	Canvas.SetPos(XPos,YPos);
	Canvas.DrawText(Str);
	Canvas.SetPos(XPos+1,YPos+1);
	Canvas.DrawColor = WhiteColor;
	Canvas.DrawText(Str);
}

function bool ShowMobileHud()
{
	// Show the mobile HUD if we are allowed to and if we don't have the HUD disabled via cinematic mode.
	return bShowMobileHud && bShowHud;
}

/**
 * Draw the Mobile hud                                                                     
 */
function RenderMobileMenu()
{
	local MobilePlayerInput  MobileInput;

	local float y;
	local int i;

	// Get a reference to the mobile player input.  Quick out if it's not a mobile input

	MobileInput = MobilePlayerInput(PlayerOwner.PlayerInput);
	if (MobileInput == none)
	{
		return;
	}

	if (bDebugTouches)
	{
		Y=20;
		Canvas.SetDrawColor(255,255,255,255);
		for (i=0;i<5;i++)
		{
			Canvas.SetPos(0,Y);
			Canvas.DrawText("" $ i @ MobileInput.Touches[i].bInUse @ MobileInput.Touches[i].State @ MobileInput.Touches[i].Zone @ MobileInput.Touches[i].Handle);
			Y+=10;
		}
	}

	MobileInput.RenderMenus(Canvas, WorldInfo.DeltaSeconds);
}

/**
* Draws the input zones on top of everything else
*/
function DrawInputZoneOverlays()
{
	local int ZoneIndex;
	local MobileInputZone Zone;
	local float Fade;
	local MobilePlayerInput MobileInput;
	local array<MobileInputZone> Zones;

	// Get a reference to the mobile player input.  Quick out if it's not a mobile input

	if (!bShowHUD)
	{
		return;
	}
	MobileInput = MobilePlayerInput(PlayerOwner.PlayerInput);
	if (MobileInput == none)
	{
		return;
	}

	// reset the canvas state
	Canvas.Reset();
	Canvas.ClipX = Canvas.SizeX;
	Canvas.ClipY = Canvas.SizeY;

	Canvas.Font	 = class'Engine'.Static.GetSmallFont();

	if (MobileInput.HasZones())
	{
		Zones = MobileInput.GetCurrentZones();
	}

	// get the current zones from the game
	for (ZoneIndex = 0; ZoneIndex < Zones.Length; ZoneIndex++)
	{
		Zone = Zones[ZoneIndex];

		if ( !Zone.bIsInvisible )
		{
			// Setup the DrawColor, take the states in to consideration

			Canvas.DrawColor = Zone.RenderColor;

			// Apply opacity from animated transition fades
			Canvas.DrawColor.A *= Zone.AnimatingFadeOpacity;

			switch (Zone.State)
			{
				case ZoneState_Inactive:
					Canvas.DrawColor.A *= Zone.InactiveAlpha;
					break;

				case ZoneState_Activating:
					Fade = Lerp(Zone.InactiveAlpha, 1.0, Zone.TransitionTime / Zone.ActivateTime);
					Canvas.DrawColor.A *= Fade;
					break;

				case ZoneState_Deactivating:
					Fade = Lerp(1.0, Zone.InactiveAlpha, Zone.TransitionTime / Zone.DeactivateTime);
					Canvas.DrawColor.A *= Fade;
					break;
			}

			if (Canvas.DrawColor.A <= 0)
			{
				continue;
			}

			// Give script a chance to override the zone 
			if (!Zone.OnPreDrawZone(Zone,Canvas))
			{
				switch (Zone.Type)
				{
				case ZoneType_Button:
					DrawMobileZone_Button(Zone);
					break;

				case ZoneType_Joystick:
					DrawMobileZone_Joystick(Zone);
					break;

				case ZoneType_Trackball:
					DrawMobileZone_Trackball(Zone);
					break;

				case ZoneType_Slider:
					DrawMobileZone_Slider(Zone);
					break;

				}
				Zone.OnPostDrawZone(Zone,Canvas);
			}

		}

		if (bShowMobileTilt)
		{
			DrawMobileTilt(MobileInput);
		}

		if (bDebugZones || (bDebugZonePresses && (Zone.State == ZoneState_Active || Zone.State == ZoneState_Activating)))
		{
			Canvas.SetDrawColor(0,255,255,255);
			Canvas.SetPos(Zone.X, Zone.Y);
			Canvas.DrawBox(Zone.SizeX, Zone.SizeY);
		}
	}	
}
function DrawMobileZone_Button(MobileInputZone Zone)
{
	local int Pressed;
	local float X,Y,U,V,UL,VL,A;
	local Texture2D Tex;

	Pressed = int(Zone.State == ZoneState_Active);

	if (ButtonImages[Pressed] != none)
	{
		Canvas.SetPos(Zone.X, Zone.Y);

		// check for override textures
		if (Pressed == 0 && Zone.OverrideTexture1 != none)
		{
			Tex = Zone.OverrideTexture1;
			U   = Zone.OverrideUVs1.U;
			V   = Zone.OverrideUVs1.V;
			UL  = Zone.OverrideUVs1.UL;
			VL  = Zone.OverrideUVs1.VL;
		}
		else if (Pressed == 1 && Zone.OverrideTexture2 != none)
		{
			Tex = Zone.OverrideTexture2;
			U   = Zone.OverrideUVs2.U;
			V   = Zone.OverrideUVs2.V;
			UL  = Zone.OverrideUVs2.UL;
			VL  = Zone.OverrideUVs2.VL;
		}
		else
		{
			Tex = ButtonImages[Pressed];
			U   = ButtonUVs[Pressed].U;
			V   = ButtonUVs[Pressed].V;
			UL  = ButtonUVs[Pressed].UL;
			VL  = ButtonUVs[Pressed].VL;
		}
		
		Canvas.DrawTile(Tex,Zone.ActiveSizeX, Zone.ActiveSizeY, U,V,UL,VL);;

		// Draw the Caption

		if (Zone.Caption != "")
		{
			if (ButtonFont != none)
			{
				Canvas.Font = ButtonFont;
			}

			Canvas.StrLen(Zone.Caption,UL,VL);
			X = Zone.X + (Zone.SizeX /2) - (UL/2);
			Y = zone.Y + (Zone.SizeY /2) - (VL/2);
			Canvas.SetPos(X + Zone.CaptionXAdjustment,Y+Zone.CaptionYAdjustment);
			A = Canvas.DrawColor.A;
			Canvas.DrawColor = ButtonCaptionColor;
			Canvas.DrawColor.A = A;
			Canvas.DrawText(Zone.Caption);
		}

	}
}

function DrawMobileZone_Joystick(MobileInputZone Zone)
{
	local int X, Y, Width, Height;
	local Color LineColor;
	local float ClampedX, ClampedY, Scale;
	local Color TempColor;

	if (Zone.OverrideTexture1 != none || JoystickBackground != none)
	{
		Width = Zone.ActiveSizeX;
		Height = Zone.ActiveSizeY;

		X = Zone.CurrentCenter.X - (Width /2);
		Y = Zone.CurrentCenter.Y - (Height /2);

		Canvas.SetPos(X,Y);
		// check for override textures
		if (Zone.OverrideTexture1 != none)
		{
			Canvas.DrawTile(Zone.OverrideTexture1, Width, Height, Zone.OverrideUVs1.U, Zone.OverrideUVs1.V, Zone.OverrideUVs1.UL, Zone.OverrideUVs1.VL);
		}
		else
		{
			Canvas.DrawTile(JoystickBackground, Width, Height, JoystickBackgroundUVs.U, JoystickBackgroundUVs.V, JoystickBackgroundUVs.UL, JoystickBackgroundUVs.VL);
		}
	}

	// Draw the Hat

	if (Zone.OverrideTexture2 != none || JoystickHat != none)
	{
		// Compute X and Y clamped to the size of the zone for the joystick
		ClampedX = Zone.CurrentLocation.X - Zone.CurrentCenter.X;
		ClampedY = Zone.CurrentLocation.Y - Zone.CurrentCenter.Y;
		Scale = 1.0f;
		if ( ClampedX != 0 || ClampedY != 0 )
		{
			Scale = Min( Zone.ActiveSizeX, Zone.ActiveSizeY ) / ( 2.0 * Sqrt(ClampedX * ClampedX + ClampedY * ClampedY) );
			Scale = FMin( 1.0, Scale );
		}
		ClampedX = ClampedX * Scale + Zone.CurrentCenter.X;
		ClampedY = ClampedY * Scale + Zone.CurrentCenter.Y;

		if (Zone.bRenderGuides)
		{
			TempColor = Canvas.DrawColor;
			LineColor.R = 128;
			LineColor.G = 128;
			LineColor.B = 128;
			LineColor.A = 255;
			Canvas.Draw2DLine(Zone.CurrentCenter.X, Zone.CurrentCenter.Y, ClampedX, ClampedY, LineColor);
			Canvas.DrawColor = TempColor;

		}

		// The size of the indicator will be a fraction of the background's total size
		Width = Zone.ActiveSizeX * 0.65;
		Height = Zone.ActiveSizeY * 0.65;

		Canvas.SetPos( ClampedX - Width / 2, ClampedY - Height / 2);
		// check for override textures
		if (Zone.OverrideTexture2 != none)
		{
			Canvas.DrawTile(Zone.OverrideTexture2, Width, Height, Zone.OverrideUVs2.U, Zone.OverrideUVs2.V, Zone.OverrideUVs2.UL, Zone.OverrideUVs2.VL);
		}
		else
		{
			Canvas.DrawTile(JoystickHat, Width, Height, JoystickHatUVs.U, JoystickHatUVs.V, JoystickHatUVs.UL, JoystickHatUVs.VL);
		}
	}
}

function DrawMobileZone_Trackball(MobileInputZone Zone)
{
	local int Width, Height;
	if (Zone.OverrideTexture1 != none || TrackballBackground != none)
	{
		Canvas.SetPos( Zone.X, Zone.Y);
		// check for override textures
		if (Zone.OverrideTexture1 != none)
		{
			Canvas.DrawTile(Zone.OverrideTexture1, Zone.SizeX, Zone.SizeY, Zone.OverrideUVs1.U, Zone.OverrideUVs1.V, Zone.OverrideUVs1.UL, Zone.OverrideUVs1.VL);
		}
		else
		{
			Canvas.DrawTile(TrackballBackground, Zone.SizeX, Zone.SizeY, TrackballBackgroundUVs.U, TrackballBackgroundUVs.V, TrackballBackgroundUVs.UL, TrackballBackgroundUVs.VL);
		}
	}

	// Draw the Touch indicator

	if ((Zone.OverrideTexture2 != none || TrackballTouchIndicator != none) && (Zone.State == ZoneState_Active || Zone.State == ZoneState_Activating))
	{
		// The size of the indicator will be a fraction of the background's total size
		Width = Zone.ActiveSizeX * 0.65;
		Height = Zone.ActiveSizeY * 0.65;

		Canvas.SetPos(Zone.CurrentLocation.X - Width / 2, Zone.CurrentLocation.Y - Height / 2);
		// check for override textures
		if (Zone.OverrideTexture2 != none)
		{
			Canvas.DrawTile(Zone.OverrideTexture2, Width, Height, Zone.OverrideUVs2.U, Zone.OverrideUVs2.V, Zone.OverrideUVs2.UL, Zone.OverrideUVs2.VL);
		}
		else
		{
			Canvas.DrawTile(TrackballTouchIndicator, Width, Height, TrackballTouchIndicatorUVs.U, TrackballTouchIndicatorUVs.V, TrackballTouchIndicatorUVs.UL, TrackballTouchIndicatorUVs.VL);
		}
	}
}

function DrawMobileTilt(MobilePlayerInput MobileInput)
{
	local float X, Y, Scale;
	local float Yaw, Pitch;

	Yaw = 2.0 * FClamp(MobileInput.MobileYaw - MobileInput.MobileYawCenter,-0.5, 0.5) * MobileInput.MobileYawMultiplier;
	Pitch = 2.0 * FClamp(MobileInput.MobilePitch - MobileInput.MobilePitchCenter, -0.5, 0.5) * MobileInput.MobilePitchMultiplier;


	// Compute X and Y clamped to the size of the zone for the joystick
	X = (MobileTiltX +  Yaw * MobileTiltSize /2) - MobileTiltX;
	Y = (MobileTiltY +  Pitch * MobileTiltSize/2) - MobileTiltY;

	Scale = 1.0f;
	if ( X != 0 || Y != 0 )
	{
		Scale = MobileTiltSize  / ( 2.0 * Sqrt(X*X*Y*Y) );
		Scale = FMin( 1.0, Scale );
	}
	X = X * Scale + MobileTiltX;
	Y = Y * Scale + MobileTiltY;

	Canvas.DrawColor = WhiteColor;
	Canvas.Draw2DLine(MobileTiltX, MobileTiltY, X, Y, Canvas.DrawColor);
}

function DrawMobileZone_Slider(MobileInputZone Zone)
{
	local float X,Y;
	local TextureUVs UVs;
	local Texture2D Tex;

	// First, look up the Texture

	// check for override textures
	if (Zone.OverrideTexture1 != none)
	{
		Tex = Zone.OverrideTexture1;
		UVs = Zone.OverrideUVs1;
	}
	else
	{
		Tex = SliderImages[int(Zone.SlideType)];
		UVs = SliderUVs[int(Zone.SlideType)];
	}

	// Now, figure out where we have to draw.

	X = (int(Zone.SlideType) > 1) ? Zone.CurrentLocation.X - (Zone.ActiveSizeX * 0.5) : Zone.X;
	Y = (int(Zone.SlideType) > 1) ? Zone.Y : Zone.CurrentLocation.Y - (Zone.ActiveSizeY * 0.5);

	Canvas.SetPos(X,Y);
	Canvas.DrawTile(Tex,Zone.ActiveSizeX, Zone.ActiveSizeY, UVs.U, UVs.V, UVs.UL, UVs.VL);
}

/**
 * The SeqEvent's from the level's kismet will have their RegisterEvent function called before the inputzones are
 * configured.  So just this once, have all of them try again.
 */

function RefreshKismetLinks()
{
	local array<SequenceObject> HudEvents;
	local Sequence GameSeq;
	local int i;
	
	GameSeq = WorldInfo.GetGameSequence();
	if (GameSeq != None)
	{
		// Find all SeqEvent_HudRender objects anywhere and call RegisterEvent on them
		GameSeq.FindSeqObjectsByClass(class'SeqEvent_HudRender', TRUE, HudEvents);

		for (i=0;i< HudEvents.Length; i++)
		{
			AddKismetRenderEvent(SeqEvent_HudRender(HudEvents[i]));
		}
	}
}

/**
 * Adds a listen to the mobile handler list.
 *
 * @param Handler the MobileMotion sequence event to add to the handler list
 */
function AddKismetRenderEvent(SeqEvent_HudRender NewEvent)
{
	local int i;
	//`log("HUD: Adding Kismet Render Event" @ NewEvent.Name);

	// More sure this event handler isn't already in the array

	for (i=0;i<KismetRenderEvents.Length;i++)
	{
		if (KismetRenderEvents[i] == NewEvent)
		{
			return;	// Already Registered
		}
	}

	// Look though the array and see if there is an empty sport.  These empty sports
	// can occur when a kismet sequence is streamed out.

	for	(i=0;i<KismetRenderEvents.Length;i++)
	{
		if (KismetRenderEvents[i] == none)
		{
			KismetRenderEvents[i] = NewEvent;
			return;
		}
	}

	KismetRenderEvents.AddItem(NewEvent);
}


/**
 * Give all Kismet Render events a chance to render to the hud 
 */
function RenderKismetHud()
{
	local int i;
	local array<byte> boolVars;

	for	(i=0;i<KismetRenderEvents.Length;i++)
	{
		boolVars.Length = 0;
		KismetRenderEvents[i].GetBoolVars(BoolVars,"Active");
		if ((BoolVars.Length == 0 || BoolVars[0] != 0) && KismetRenderEvents[i].bIsActive)
		{
			if (KismetRenderEvents[i] != none && KismetRenderEvents[i].bIsActive)
			{
				KismetRenderEvents[i].Render(Canvas,self);
			}
		}
	}
}

defaultproperties
{
}
