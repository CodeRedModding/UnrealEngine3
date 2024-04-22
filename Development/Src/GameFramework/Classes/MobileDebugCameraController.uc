//-----------------------------------------------------------
// Mobile Debug Camera Controller
//
// * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//-----------------------------------------------------------
class MobileDebugCameraController extends DebugCameraController
	DependsOn(MobilePlayerInput);

var int OldMobileGroup;

var MobilePlayerInput MPI;

/*
 *  Function called on activation debug camera controller
 */
function OnActivate( PlayerController PC )
{
	MPI = MobilePlayerInput(OriginalControllerRef.PlayerInput);

	if(MPI != none)
	{
		OldMobileGroup = MPI.CurrentMobileGroup;
		MPI.CurrentMobileGroup = -1;
	}

	MPI = new(self) class'MobileDebugCameraInput';

	super.OnActivate(PC);

	MPI.InitInputSystem();

	SetupDebugZones();

	MPI.ActivateInputGroup("DebugGroup");

	MobileDebugCameraHUD(myHUD).bDrawDebugText = bDrawDebugText;
}

/*
 * Does any controller/input necessary initialization.
 */
function InitDebugInputSystem()
{
	MPI.MobileInputGroups.Remove(0, MPI.MobileInputGroups.Length);
	MPI.MobileInputZones.Remove(0, MPI.MobileInputZones.Length);
}

/**
 *  Function called on deactivation debug camera controller
 */
function OnDeactivate( PlayerController PC )
{
	local MobilePlayerInput MobileInput;

	MPI.CurrentMobileGroup = -1;

	super.OnDeactivate(PC);

	MobileInput = MobilePlayerInput(OriginalControllerRef.PlayerInput);
	
	MobileInput.SwapZoneOwners();
	MobileInput.CurrentMobileGroup = OldMobileGroup;
}


/**
 * When we init the input system, find the TapToMove zone and hook up the delegate                                                                      
 */
event InitInputSystem()
{
	Super.InitInputSystem();
}

/**
 * The main purpose of this function is to size and reset zones.  There's a lot of specific code in
 * here to reposition zones based on if it's an phone vs pad.
 */
function SetupDebugZones()
{
	local float Ratio;
	local float Spacer;

	// Cache the MPI
	local MobileInputZone StickMoveZone;
	local MobileInputZone StickLookZone;

	local Vector2D ViewportSize;

	MPI.InitializeInputZones();

	StickMoveZone = MPI.FindZone("DebugStickMoveZone");
	StickLookZone = MPI.FindZone("DebugStickLookZone");

	LocalPlayer(OriginalPlayer).ViewportClient.GetViewportSize(ViewportSize);

	Ratio = ViewportSize.Y / ViewportSize.X;

	// The values here were picked after a long process of trail and error.  They basically
	// represent the collective "it feels right".  These work for EpicCitadel.  You will want to
	// choose values that work for you.

	Spacer = (Ratio == 0.75) ? 96 : 64;
	Spacer *= (ViewportSize.X / 1024);

	if (StickMoveZone != none)
	{
		if (Ratio == 0.75)
		{
			StickMoveZone.SizeX = ViewportSize.X * 0.12;
			StickMoveZone.SizeY = StickMoveZone.SizeX;

			StickMoveZone.ActiveSizeX = StickMoveZone.SizeX;
			StickMoveZone.ActiveSizeY = StickMoveZone.SizeY;
		}

		StickMoveZone.SizeX = Spacer + StickMoveZone.SizeX;
		StickMoveZone.SizeY = Spacer + StickMoveZone.SizeY;
		if (Ratio == 0.75) 
		{
			StickMoveZone.SizeY *= 1.5;
		}

		StickMoveZone.X = 0;
		StickMoveZone.Y = ViewportSize.Y - StickMoveZone.SizeY;

		StickMoveZone.CurrentCenter.X = StickMoveZone.X + StickMoveZone.SizeX - (StickMoveZone.ActiveSizeX*0.5); 
		if (Ratio == 0.75)
		{
			StickMoveZone.CurrentCenter.Y = ViewportSize.Y - StickMoveZone.SizeY * 0.33;
		}
		else
		{
			StickMoveZone.CurrentCenter.Y = StickMoveZone.Y + StickMoveZone.ActiveSizeY * 0.5;
		}
		StickMoveZone.CurrentLocation = StickMoveZone.CurrentCenter;
		StickMoveZone.InitialCenter = StickMoveZone.CurrentCenter;
		StickMoveZone.bCenterOnEvent = true;
	}

	if (StickLookZone != none)
	{
		if (Ratio == 0.75)
		{
			StickLookZone.SizeX = ViewportSize.X * 0.12;
			StickLookZone.SizeY = StickLookZone.SizeX;

			StickLookZone.ActiveSizeX = StickLookZone.SizeX;
			StickLookZone.ActiveSizeY = StickLookZone.SizeY;
		}

		StickLookZone.SizeX = Spacer + StickLookZone.SizeX;
		StickLookZone.SizeY = Spacer + StickLookZone.SizeY;
		if (Ratio == 0.75) 
		{
			StickLookZone.SizeY *= 1.5;
		}


		StickLookZone.X = ViewportSize.X - StickLookZone.SizeX;
		StickLookZone.Y = ViewportSize.Y - StickLookZone.SizeY;

		StickLookZone.CurrentCenter.X = StickLookZone.X + (StickLookZone.ActiveSizeX*0.5);
		if (Ratio == 0.75)
		{
			StickLookZone.CurrentCenter.Y = ViewportSize.Y - StickLookZone.SizeY * 0.33;
		}
		else
		{
			StickLookZone.CurrentCenter.Y = StickLookZone.Y + StickLookZone.ActiveSizeY * 0.5;
		}

		StickLookZone.CurrentLocation = StickLookZone.CurrentCenter;
		StickLookZone.InitialCenter = StickLookZone.CurrentCenter;
		StickLookZone.bCenterOnEvent = true;
	}

	
}

defaultproperties
{
	InputClass=class'GameFramework.MobileDebugCameraInput'
	HUDClass=class'GameFramework.MobileDebugCameraHUD'
}
