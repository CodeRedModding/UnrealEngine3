//-----------------------------------------------------------
// Debug Camera Controller
//
// To turn it on, please press Alt+C or both (left and right) analogs on xbox pad
// After turning:
//   WASD  | Left Analog - moving
//   Mouse | Right Analog - rotating
//   Shift | XBOX_KeyB - move faster
//   Q/E   | LT/RT - move Up/Down
//   Enter | XBOX_KeyA - to call "FreezeRendering" console command
//   Alt+C | LeftThumbstick - to toggle debug camera
//
// * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//-----------------------------------------------------------
class DebugCameraController extends GamePlayerController
	config(Input)
	native;

/** The key that triggers PrimarySelect(). */
var globalconfig name PrimaryKey;
/** The key that triggers SecondarySelect(). */
var globalconfig name SecondaryKey;
/** The key that triggers Unselect(). */
var globalconfig name UnselectKey;
/** Whether to show information about the selected actor on the debug camera HUD. */
var globalconfig bool bShowSelectedInfo;
/** When true the debug text is drawn.  This is passed through from CheatManager's ToggleDebugCamera exec function. */
var bool bDrawDebugText;
/** HUD class to spawn. */
var class<HUD> HUDClass;

var PlayerController        OriginalControllerRef;
var Player                  OriginalPlayer;
var bool                    bIsFrozenRendering;
var	DrawFrustumComponent	DrawFrustum;
var Actor					SelectedActor;
var PrimitiveComponent		SelectedComponent;

/**
 * Called when an actor has been selected with the primary key (e.g. left mouse button).
 *
 * @param HitLoc	World-space position of the selection point.
 * @param HitNormal	World-space normal of the selection point.
 * @param HitInfo	Info struct for the selection point.
 */
native function PrimarySelect( vector HitLoc, vector HitNormal, TraceHitInfo HitInfo );

/**
 * Called when an actor has been selected with the secondary key (e.g. right mouse button).
 *
 * @param HitLoc	World-space position of the selection point.
 * @param HitNormal	World-space normal of the selection point.
 * @param HitInfo	Info struct for the selection point.
 */
native function SecondarySelect( vector HitLoc, vector HitNormal, TraceHitInfo HitInfo );

/**
 * Called when the user pressed the unselect key, just before the selected actor is cleared.
 */
native function Unselect();

simulated event PostBeginPlay()
{
	super.PostBeginPlay();

	// if hud is existing, delete it and create new hud for debug camera
	if ( myHUD != None )
		myHUD.Destroy();
	myHUD = Spawn( HUDClass, self);
}

/*
 *  Function called on activation debug camera controller
 */
function OnActivate( PlayerController PC )
{
	if ( DebugCameraHUD(myHUD) != None )
	{
		DebugCameraHUD(myHUD).bDrawDebugText = bDrawDebugText;
	}

	if(DrawFrustum==None)
	{
		DrawFrustum = new(PC.PlayerCamera) class'DrawFrustumComponent';
	}

	if ( bDrawDebugText )
	{
		DrawFrustum.SetHidden( false );
		ConsoleCommand("show camfrustums true");
	}
	else
	{
		DrawFrustum.SetHidden( true );
		ConsoleCommand("show camfrustums false");
	}

	PC.SetHidden(false);
	PC.PlayerCamera.SetHidden(false);

	DrawFrustum.FrustumAngle = PC.PlayerCamera.CameraCache.POV.FOV;
	DrawFrustum.SetAbsolute(true, true, false);
	DrawFrustum.SetTranslation(PC.PlayerCamera.CameraCache.POV.Location);
	DrawFrustum.SetRotation(PC.PlayerCamera.CameraCache.POV.Rotation);

	PC.PlayerCamera.AttachComponent(DrawFrustum);
}

/**
 *  Function called on deactivation debug camera controller
 */
function OnDeactivate( PlayerController PC )
{
	DrawFrustum.SetHidden( true );
	PC.PlayerCamera.DetachComponent(DrawFrustum);
	PC.SetHidden(true);
	PC.PlayerCamera.SetHidden(true);
}

//function called from key bindings command to save information about
// turning on/off FreezeRendering command.
exec function SetFreezeRendering()
{
	 ConsoleCommand("FreezeRendering");
	 bIsFrozenRendering = !bIsFrozenRendering;
}

//function called from key bindings command
exec function MoreSpeed()
{
	bRun = 2;
}

//function called from key bindings command
exec function NormalSpeed()
{
	bRun = 0;
}

/*
 * Switch from debug camera controller to local player controller
 */
function DisableDebugCamera()
{
	if( OriginalControllerRef != none )
	{
		// restore FreezeRendering command state before quite
		if( bIsFrozenRendering==true ) {
			ConsoleCommand("FreezeRendering");
			bIsFrozenRendering = false;
		}
		if( OriginalPlayer != none )
		{
			OriginalPlayer.SwitchController( OriginalControllerRef );
			OriginalControllerRef.InitInputSystem();
			OnDeactivate( self );
		}
	}
}

/*
 * Does any controller/input necessary initialization.
 */
function InitDebugInputSystem()
{
	OriginalControllerRef.PlayerInput.InitInputSystem();
	OriginalControllerRef.InitInputSystem();
}

auto state PlayerWaiting
{
	function PlayerMove(float DeltaTime)
	{
		local float UndilatedDeltaTime;
		
		UndilatedDeltaTime = DeltaTime / WorldInfo.TimeDilation;

		super.PlayerMove(UndilatedDeltaTime);

		if (WorldInfo.Pauser != None)
		{
			PlayerCamera.UpdateCamera(DeltaTime);
		}
	}
};

/**
 * Called from DebugCameraInput
 * Process an input key event routed through unrealscript from another object. This method is assigned as the value for the
 * OnRecievedNativeInputKey delegate so that native input events are routed to this unrealscript function.
 *
 * @param	ControllerId	the controller that generated this input key event
 * @param	Key				the name of the key which an event occured for (KEY_Up, KEY_Down, etc.)
 * @param	EventType		the type of event which occured (pressed, released, etc.)
 * @param	AmountDepressed	for analog keys, the depression percent.
 *
 * @return	true to consume the key event, false to pass it on.
 */
function bool NativeInputKey( int ControllerId, name Key, EInputEvent Event, float AmountDepressed = 1.f, bool bGamepad = FALSE )
{
	local vector		CamLoc, ZeroVec;
	local rotator		CamRot;
	local TraceHitInfo	HitInfo;
	local Actor			HitActor;
	local vector		HitLoc, HitNormal;

	CamLoc = PlayerCamera.CameraCache.POV.Location;
	CamRot = PlayerCamera.CameraCache.POV.Rotation;

	if ( Event == IE_Pressed )
	{
		if ( Key == UnselectKey )
		{
			Unselect();
			SelectedActor = None;
			SelectedComponent = None;
			return true;
		}
		if ( Key == PrimaryKey )
		{
			HitActor = Trace(HitLoc, HitNormal, vector(camRot) * 5000 * 20 + CamLoc, CamLoc, true, ZeroVec, HitInfo);
			if( HitActor != None)
			{
				SelectedActor = HitActor;
				SelectedComponent = HitInfo.HitComponent;
				PrimarySelect( HitLoc, HitNormal, HitInfo );
			}
			return true;
		}
		if ( Key == SecondaryKey )
		{
			HitActor = Trace(HitLoc, HitNormal, vector(camRot) * 5000 * 20 + CamLoc, CamLoc, true, ZeroVec, HitInfo);
			if( HitActor != None)
			{
				SelectedActor = HitActor;
				SelectedComponent = HitInfo.HitComponent;
				SecondarySelect( HitLoc, HitNormal, HitInfo );
			}
			return true;
		}
	}
	return false;
}

exec function ShowDebugSelectedInfo()
{
	bShowSelectedInfo = !bShowSelectedInfo;
}

/** Overridden to potentially pipe commands to regular PlayerController */
native function string ConsoleCommand(string Command, optional bool bWriteToLog = true);

cpptext
{
	/**
	 * Builds a list of components that are hidden based upon gameplay
	 *
	 * @param ViewLocation the view point to hide/unhide from
	 * @param HiddenComponents the list to add to/remove from
	 */
	virtual void UpdateHiddenComponents(const FVector& ViewLocation,TSet<UPrimitiveComponent*>& HiddenComponents);
}

defaultproperties
{
	InputClass=class'GameFramework.DebugCameraInput'
	OriginalControllerRef=None
	OriginalPlayer=None
	bIsFrozenRendering=false

	DrawFrustum=none
	bHidden=FALSE
	bHiddenEd=FALSE
	bDrawDebugText=true
	HUDClass=class'DebugCameraHUD'

	bAlwaysTick=TRUE
}
