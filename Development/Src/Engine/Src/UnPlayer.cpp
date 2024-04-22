/*=============================================================================
	UnPlayer.cpp: Unreal player implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
 
#include "EnginePrivate.h"
#include "EngineUserInterfaceClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "EngineParticleClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineAnimClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSoundClasses.h"
#include "SceneRenderTargets.h"

// needed for adding components when typing "show paths" in game
#include "EngineAIClasses.h"


#include "UnTerrain.h"
#include "UnSubtitleManager.h"
#include "UnNet.h"
#include "PerfMem.h"
#include "ProfilingHelpers.h"

#if WITH_REALD
	#include "RealD/RealD.h"
#endif

#include "GFxUIClasses.h"
#if WITH_GFx
#include "ScaleformEngine.h"
#endif // WITH_GFx

#if WITH_APEX
#include "NvApexScene.h"
#endif

#if DWTRIOVIZSDK
	#include "DwTrioviz/DwTriovizImpl.h"
#endif

IMPLEMENT_CLASS(UPlayer);
IMPLEMENT_CLASS(ULocalPlayer);
IMPLEMENT_CLASS(UScriptViewportClient);

/** This variable allows forcing full screen of the first player controller viewport, even if there are multiple controllers plugged in and no cinematic playing. */
UBOOL GForceFullscreen = FALSE;

/** High-res screenshot variables */
UBOOL		GIsTiledScreenshot = FALSE;
INT			GScreenshotResolutionMultiplier = 2;
INT			GScreenshotMargin = 64;	// In pixels
INT			GScreenshotTile = 0;
FIntRect	GScreenshotRect;		// Rendered tile
/** TRUE if we should grab a screenshot at the end of the frame */
UBOOL		GScreenShotRequest = FALSE;
FString     GScreenShotName = TEXT("");

/** Set to >0 when taking a screenshot, then counts down to 0.  When 0, do normal processing. */
INT			GGameScreenshotCounter = 0;

/** Whether to tick and render the UI. */
UBOOL		GTickAndRenderUI = TRUE;

#if WITH_GFx
/** Whether to render the Flash UI */
UBOOL		GRenderScaleform = TRUE;

/** Override to enable/disable Flash UI */
UBOOL		GScaleformEnabled = TRUE;
#endif // WITH_GFX

UBOOL		ULocalPlayer::bOverrideView = FALSE;
FVector		ULocalPlayer::OverrideLocation;
FRotator	ULocalPlayer::OverrideRotation;

IMPLEMENT_CLASS(UGameViewportClient);
IMPLEMENT_CLASS(UPlayerManagerInteraction);




UBOOL GShouldLogOutAFrameOfSkelCompTick = FALSE;
UBOOL GShouldLogOutAFrameOfLightEnvTick = FALSE;
UBOOL GShouldLogOutAFrameOfIsOverlapping = FALSE;
UBOOL GShouldLogOutAFrameOfMoveActor = FALSE;
UBOOL GShouldLogOutAFrameOfPhysAssetBoundsUpdate = FALSE;
UBOOL GShouldLogOutAFrameOfComponentUpdates = FALSE;

UBOOL GShouldTraceOutAFrameOfPain = FALSE;
UBOOL GShouldLogOutAFrameOfSkelCompLODs = FALSE;
UBOOL GShouldLogOutAFrameOfSkelMeshLODs = FALSE;
UBOOL GShouldLogOutAFrameOfFaceFXDebug = FALSE;
UBOOL GShouldLogOutAFrameOfFaceFXBones = FALSE;

UBOOL GShouldTraceFaceFX = FALSE;

/** Whether to visualize the lightmap selected by the Debug Camera. */
extern UBOOL GShowDebugSelectedLightmap;
/** The currently selected component in the actor. */
extern UPrimitiveComponent* GDebugSelectedComponent;
/** The lightmap used by the currently selected component, if it's a static mesh component. */
extern FLightMap2D* GDebugSelectedLightmap;

/**
 * Draw debug info on a game scene view.
 */
class FGameViewDrawer : public FViewElementDrawer
{
public:
	/**
	 * Draws debug info using the given draw interface.
	 */
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI)
	{
#if !FINAL_RELEASE
		// Draw a wireframe sphere around the selected lightmap, if requested.
		if ( GShowDebugSelectedLightmap && GDebugSelectedComponent && GDebugSelectedLightmap )
		{
			FLOAT Radius = GDebugSelectedComponent->Bounds.SphereRadius;
			INT Sides = Clamp<INT>( appTrunc(Radius*Radius*4.0f*PI/(80.0f*80.0f)), 8, 200 );
			DrawWireSphere( PDI, GDebugSelectedComponent->Bounds.Origin, FColor(255,130,0), GDebugSelectedComponent->Bounds.SphereRadius, Sides, SDPG_Foreground );
		}
#endif
	}
};


/** UPlayerManagerInteraction */
/**
 * Routes an input key event to the player's interactions array
 *
 * @param	Viewport - The viewport which the key event is from.
 * @param	ControllerId - The controller which the key event is from.
 * @param	Key - The name of the key which an event occured for.
 * @param	Event - The type of event which occured.
 * @param	AmountDepressed - For analog keys, the depression percent.
 * @param	bGamepad - input came from gamepad (ie xbox controller)
 *
 * @return	True to consume the key event, false to pass it on.
 */
UBOOL UPlayerManagerInteraction::InputKey(INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed/*=1.f*/,UBOOL bGamepad/*=FALSE*/)
{
	UBOOL bResult = FALSE;

	INT PlayerIndex = UUIInteraction::GetPlayerIndex(ControllerId);
	if ( GEngine->GamePlayers.IsValidIndex(PlayerIndex) )
	{
		ULocalPlayer* TargetPlayer = GEngine->GamePlayers(PlayerIndex);
		if ( TargetPlayer != NULL && TargetPlayer->Actor != NULL )
		{
			APlayerController* PC = TargetPlayer->Actor;
			for ( INT InteractionIndex = 0; !bResult && InteractionIndex < PC->Interactions.Num(); InteractionIndex++ )
			{
				UInteraction* PlayerInteraction = PC->Interactions(InteractionIndex);
				if ( OBJ_DELEGATE_IS_SET(PlayerInteraction,OnReceivedNativeInputKey) )
				{
					bResult = PlayerInteraction->delegateOnReceivedNativeInputKey(ControllerId, Key, Event, AmountDepressed, bGamepad);
				}

				bResult = bResult || PlayerInteraction->InputKey(ControllerId, Key, Event, AmountDepressed, bGamepad);
			}
		}
	}

	return bResult;
}

/**
 * Routes an axis input event to the player's interactions array.
 *
 * @param	Viewport - The viewport which the axis movement is from.
 * @param	ControllerId - The controller which the axis movement is from.
 * @param	Key - The name of the axis which moved.
 * @param	Delta - The axis movement delta.
 * @param	DeltaTime - The time since the last axis update.
 *
 * @return	True to consume the axis movement, false to pass it on.
 */
UBOOL UPlayerManagerInteraction::InputAxis( INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad/*=FALSE*/ )
{
	UBOOL bResult = FALSE;

	INT PlayerIndex = UUIInteraction::GetPlayerIndex(ControllerId);
	if ( GEngine->GamePlayers.IsValidIndex(PlayerIndex) )
	{
		ULocalPlayer* TargetPlayer = GEngine->GamePlayers(PlayerIndex);
		if ( TargetPlayer != NULL && TargetPlayer->Actor != NULL )
		{
			APlayerController* PC = TargetPlayer->Actor;
			for ( INT InteractionIndex = 0; !bResult && InteractionIndex < PC->Interactions.Num(); InteractionIndex++ )
			{
				UInteraction* PlayerInteraction = PC->Interactions(InteractionIndex);
				if ( OBJ_DELEGATE_IS_SET(PlayerInteraction,OnReceivedNativeInputAxis) )
				{
					bResult = PlayerInteraction->delegateOnReceivedNativeInputAxis(ControllerId, Key, Delta, DeltaTime, bGamepad);
				}

				bResult = bResult || PlayerInteraction->InputAxis(ControllerId, Key, Delta, DeltaTime, bGamepad);
			}
		}
	}

	return bResult;
}

/**
 * Routes a character input to the player's Interaction array.
 *
 * @param	Viewport - The viewport which the axis movement is from.
 * @param	ControllerId - The controller which the axis movement is from.
 * @param	Character - The character.
 *
 * @return	True to consume the character, false to pass it on.
 */
UBOOL UPlayerManagerInteraction::InputChar( INT ControllerId, TCHAR Character )
{
	UBOOL bResult = FALSE;

	INT PlayerIndex = UUIInteraction::GetPlayerIndex(ControllerId);
	if ( GEngine->GamePlayers.IsValidIndex(PlayerIndex) )
	{
		ULocalPlayer* TargetPlayer = GEngine->GamePlayers(PlayerIndex);
		if ( TargetPlayer != NULL && TargetPlayer->Actor != NULL )
		{
			APlayerController* PC = TargetPlayer->Actor;
			for ( INT InteractionIndex = 0; !bResult && InteractionIndex < PC->Interactions.Num(); InteractionIndex++ )
			{
				UInteraction* PlayerInteraction = PC->Interactions(InteractionIndex);
				if ( OBJ_DELEGATE_IS_SET(PlayerInteraction,OnReceivedNativeInputChar) )
				{
					TCHAR CharString[2] = { Character, 0 };
					bResult = PlayerInteraction->delegateOnReceivedNativeInputChar(ControllerId, CharString);
				}

				bResult = bResult || PlayerInteraction->InputChar(ControllerId, Character);
			}
		}
	}

	return bResult;
}

/**
 * Process a touchpad touch input event received from the viewport.
 *
 * @param	ControllerId - The controller which the key event is from.
 * @param	Handle - Identifier unique to this touch event
 * @param	TouchLocation - Screen position of the touch
 * @param	DeviceTimestamp - Timestamp of the event
 * @param	TouchpadIndex - For devices with multiple touchpads, this is the index of which one
 * @return	True to consume the key event, false to pass it on.
 */
UBOOL UPlayerManagerInteraction::InputTouch(INT ControllerId, UINT Handle, ETouchType Type, FVector2D TouchLocation, DOUBLE DeviceTimestamp, UINT TouchpadIndex)
{
	UBOOL bResult = FALSE;

	INT PlayerIndex = UUIInteraction::GetPlayerIndex(ControllerId);
	if ( GEngine->GamePlayers.IsValidIndex(PlayerIndex) )
	{
		ULocalPlayer* TargetPlayer = GEngine->GamePlayers(PlayerIndex);
		if ( TargetPlayer != NULL && TargetPlayer->Actor != NULL )
		{
			APlayerController* PC = TargetPlayer->Actor;
			for ( INT InteractionIndex = 0; !bResult && InteractionIndex < PC->Interactions.Num(); InteractionIndex++ )
			{
				UInteraction* PlayerInteraction = PC->Interactions(InteractionIndex);
				bResult = bResult || PlayerInteraction->InputTouch(ControllerId, Handle, Type, TouchLocation, DeviceTimestamp, TouchpadIndex);
			}
		}
	}

	return bResult;
}

/**
 * Process a motion event received from the viewport.
 *
 * @param ControllerId - The controller which the key event is from.
 * @param Tilt			The current orientation of the device
 * @param RotationRate	How fast the tilt is changing
 * @param Gravity		Describes the current gravity of the device
 * @param Acceleration  Describes the acceleration of the device
 * @return	True to consume the motion event, false to pass it on.
 */
UBOOL UPlayerManagerInteraction::InputMotion(INT ControllerId, const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration)
{
	UBOOL bResult = FALSE;

	INT PlayerIndex = UUIInteraction::GetPlayerIndex(ControllerId);
	if ( GEngine->GamePlayers.IsValidIndex(PlayerIndex) )
	{
		ULocalPlayer* TargetPlayer = GEngine->GamePlayers(PlayerIndex);
		if ( TargetPlayer != NULL && TargetPlayer->Actor != NULL )
		{
			APlayerController* PC = TargetPlayer->Actor;
			for ( INT InteractionIndex = 0; !bResult && InteractionIndex < PC->Interactions.Num(); InteractionIndex++ )
			{
				UInteraction* PlayerInteraction = PC->Interactions(InteractionIndex);
				bResult = bResult || PlayerInteraction->InputMotion(ControllerId, Tilt, RotationRate, Gravity, Acceleration);
			}
		}
	}

	return bResult;
}


UGameViewportClient::UGameViewportClient():
	ShowFlags(SHOW_ViewMode_Lit|(SHOW_DefaultGame&~SHOW_ViewMode_Mask))
{
	bDisplayHardwareMouseCursor = FALSE;
}

/**
 * Cleans up all rooted or referenced objects created or managed by the GameViewportClient.  This method is called
 * when this GameViewportClient has been disassociated with the game engine (i.e. is no longer the engine's GameViewport).
 */
void UGameViewportClient::DetachViewportClient()
{
	// notify all interactions to clean up their own references
	eventGameSessionEnded();

	// if we have a UIController, tear it down now
	if ( UIController != NULL )
	{
		UIController->TearDownUI();
	}

	UIController = NULL;
	ViewportConsole = NULL;
	RemoveFromRoot();
}

/**
 * Called every frame to allow the game viewport to update time based state.
 * @param	DeltaTime - The time since the last call to Tick.
 */
void UGameViewportClient::Tick( FLOAT DeltaTime )
{
	// first call the unrealscript tick
	eventTick(DeltaTime);

	// now tick all interactions
	for ( INT i = 0; i < GlobalInteractions.Num(); i++ )
	{
		UInteraction* Interaction = GlobalInteractions(i);
		Interaction->Tick(DeltaTime);
	}
}

FString UGameViewportClient::ConsoleCommand(const FString& Command)
{
	FString TruncatedCommand = Command.Left(1000);
	FConsoleOutputDevice ConsoleOut(ViewportConsole);
	Exec(*TruncatedCommand,ConsoleOut);
	return *ConsoleOut;
}

/**
 * Routes an input key event received from the viewport to the Interactions array for processing.
 *
 * @param	Viewport		the viewport the input event was received from
 * @param	ControllerId	gamepad/controller that generated this input event
 * @param	Key				the name of the key which an event occured for (KEY_Up, KEY_Down, etc.)
 * @param	EventType		the type of event which occured (pressed, released, etc.)
 * @param	AmountDepressed	(analog keys only) the depression percent.
 * @param	bGamepad - input came from gamepad (ie xbox controller)
 *
 * @return	TRUE to consume the key event, FALSE to pass it on.
 */
UBOOL UGameViewportClient::InputKey(FViewport* Viewport,INT ControllerId,FName Key,EInputEvent EventType,FLOAT AmountDepressed,UBOOL bGamepad)
{
	// if a movie is playing then handle input key
	if( GFullScreenMovie && 
		GFullScreenMovie->GameThreadIsMoviePlaying(TEXT("")) )
	{
#if !CONSOLE
		if ( GFullScreenMovie->InputKey(Viewport,ControllerId,Key,EventType,AmountDepressed,bGamepad) )
		{
#endif
			return TRUE;
#if !CONSOLE
		}
#endif
	}

#if DWTRIOVIZSDK
	// allow the Trioviz menu input
	if (DwTriovizImpl_ProcessMenuInput(ControllerId, Key, EventType))
	{
		return TRUE;
	}
#endif

	UBOOL bResult = FALSE;

	if ( DELEGATE_IS_SET(HandleInputKey) )
	{
		bResult = delegateHandleInputKey(ControllerId, Key, EventType, AmountDepressed, bGamepad);
	}

	// if it wasn't handled by script, route to the interactions array
	for ( INT InteractionIndex = 0; !bResult && InteractionIndex < GlobalInteractions.Num(); InteractionIndex++ )
	{
		UInteraction* Interaction = GlobalInteractions(InteractionIndex);
		if ( OBJ_DELEGATE_IS_SET(Interaction,OnReceivedNativeInputKey) )
		{
			bResult = Interaction->delegateOnReceivedNativeInputKey(ControllerId, Key, EventType, AmountDepressed, bGamepad);
		}

		bResult = bResult || Interaction->InputKey(ControllerId, Key, EventType, AmountDepressed, bGamepad);
	}

	return bResult;
}

/**
 * Routes an input axis (joystick, thumbstick, or mouse) event received from the viewport to the Interactions array for processing.
 *
 * @param	Viewport		the viewport the input event was received from
 * @param	ControllerId	the controller that generated this input axis event
 * @param	Key				the name of the axis that moved  (KEY_MouseX, KEY_XboxTypeS_LeftX, etc.)
 * @param	Delta			the movement delta for the axis
 * @param	DeltaTime		the time (in seconds) since the last axis update.
 *
 * @return	TRUE to consume the axis event, FALSE to pass it on.
 */
UBOOL UGameViewportClient::InputAxis(FViewport* Viewport,INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime, UBOOL bGamepad)
{
	UBOOL bResult = FALSE;

	// give script the chance to process this input first
	if ( DELEGATE_IS_SET(HandleInputAxis) )
	{
		bResult = delegateHandleInputAxis(ControllerId, Key, Delta, DeltaTime, bGamepad);
	}

	// if it wasn't handled by script, route to the interactions array
	for ( INT InteractionIndex = 0; !bResult && InteractionIndex < GlobalInteractions.Num(); InteractionIndex++ )
	{
		UInteraction* Interaction = GlobalInteractions(InteractionIndex);
		if ( OBJ_DELEGATE_IS_SET(Interaction,OnReceivedNativeInputAxis) )
		{
			bResult = Interaction->delegateOnReceivedNativeInputAxis(ControllerId, Key, Delta, DeltaTime, bGamepad);
		}

		bResult = bResult || Interaction->InputAxis(ControllerId, Key, Delta, DeltaTime, bGamepad);
	}

	return bResult;
}

/**
 * Routes a character input event (typing) received from the viewport to the Interactions array for processing.
 *
 * @param	Viewport		the viewport the input event was received from
 * @param	ControllerId	the controller that generated this character input event
 * @param	Character		the character that was typed
 *
 * @return	TRUE to consume the key event, FALSE to pass it on.
 */
UBOOL UGameViewportClient::InputChar(FViewport* Viewport,INT ControllerId,TCHAR Character)
{
	UBOOL bResult = FALSE;

	// should probably just add a ctor to FString that takes a TCHAR
	FString CharacterString;
	CharacterString += Character;

	if ( DELEGATE_IS_SET(HandleInputChar) )
	{
		bResult = delegateHandleInputChar(ControllerId, CharacterString);
	}

	// if it wasn't handled by script, route to the interactions array
	for ( INT InteractionIndex = 0; !bResult && InteractionIndex < GlobalInteractions.Num(); InteractionIndex++ )
	{
		UInteraction* Interaction = GlobalInteractions(InteractionIndex);
		if ( OBJ_DELEGATE_IS_SET(Interaction,OnReceivedNativeInputChar) )
		{
			bResult = Interaction->delegateOnReceivedNativeInputChar(ControllerId, CharacterString);
		}

		bResult = bResult || Interaction->InputChar(ControllerId, Character);
	}

	return bResult;
}


/**
 * Check a key event received by the viewport.
 * If the viewport client uses the event, it should return true to consume it.
 * @param	Viewport - The viewport which the event is from.
 * @param	ControllerId - The controller which the key event is from.
 * @param	Handle - Identifier unique to this touch event
 * @param	Type - What kind of touch event this is (see ETouchType)
 * @param	TouchLocation - Screen position of the touch
 * @param	DeviceTimestamp - Timestamp of the event
 * @param	TouchpadIndex - For devices with multiple touchpads, this is the index of which one
 * @return	True to consume the key event, false to pass it on.
 */
UBOOL UGameViewportClient::InputTouch(FViewport* Viewport, INT ControllerId, UINT Handle, BYTE Type, FVector2D TouchLocation, DOUBLE DeviceTimestamp, UINT TouchpadIndex)
{
	// allow each interaction to respond to the touch
	UBOOL bResult = FALSE;
	for ( INT InteractionIndex = 0; !bResult && InteractionIndex < GlobalInteractions.Num(); InteractionIndex++ )
	{
		UInteraction* Interaction = GlobalInteractions(InteractionIndex);

		bResult = bResult || Interaction->InputTouch(ControllerId, Handle, (ETouchType)Type, TouchLocation, DeviceTimestamp, TouchpadIndex);
	}

	return bResult;
}

/**
 * Each frame, the input system will update the motion data.
 *
 * @param Viewport - The viewport which the key event is from.
 * @param ControllerId - The controller which the key event is from.
 * @param Tilt			The current orientation of the device
 * @param RotationRate	How fast the tilt is changing
 * @param Gravity		Describes the current gravity of the device
 * @param Acceleration  Describes the acceleration of the device
 * @return	True to consume the motion event, false to pass it on.
 */
UBOOL UGameViewportClient::InputMotion(FViewport* Viewport, INT ControllerId, const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration) 
{ 
	// allow each interaction to respond to the touch
	UBOOL bResult = FALSE;
	for ( INT InteractionIndex = 0; !bResult && InteractionIndex < GlobalInteractions.Num(); InteractionIndex++ )
	{
		UInteraction* Interaction = GlobalInteractions(InteractionIndex);

		bResult = bResult || Interaction->InputMotion(ControllerId, Tilt, RotationRate, Gravity, Acceleration);
	}

	return bResult;
}


/**
 * Returns the forcefeedback manager associated with the PlayerController.
 */
class UForceFeedbackManager* UGameViewportClient::GetForceFeedbackManager(INT ControllerId)
{
	for(INT PlayerIndex = 0;PlayerIndex < GEngine->GamePlayers.Num();PlayerIndex++)
	{
		ULocalPlayer* Player = GEngine->GamePlayers(PlayerIndex);
		if(Player->ViewportClient == this && Player->ControllerId == ControllerId)
		{
			// Only play force feedback on gamepad
			if( Player->Actor && 
				Player->Actor->ForceFeedbackManager )
			{
				return Player->Actor->ForceFeedbackManager;
			}
			break;
		}
	}

	return NULL;
}

/** @return mouse position in game viewport coordinates (does not account for splitscreen) */
FVector2D UGameViewportClient::GetMousePosition()
{
	if (Viewport == NULL)
	{
		return FVector2D(0.f, 0.f);
	}
	else
	{
		FIntPoint MousePos;
		Viewport->GetMousePos(MousePos);
		return FVector2D(MousePos);
	}
}

/**
 * Determines whether this viewport client should receive calls to InputAxis() if the game's window is not currently capturing the mouse.
 * Used by the UI system to easily receive calls to InputAxis while the viewport's mouse capture is disabled.
 */
UBOOL UGameViewportClient::RequiresUncapturedAxisInput() const
{
	return Viewport != NULL && bDisplayHardwareMouseCursor == TRUE && Viewport->HasFocus();
}

/**
 * Retrieves the cursor that should be displayed by the OS
 *
 * @param	Viewport	the viewport that contains the cursor
 * @param	X			the client x position of the cursor
 * @param	Y			the client Y position of the cursor
 * 
 * @return	the cursor that the OS should display
 */
EMouseCursor UGameViewportClient::GetCursor( FViewport* Viewport, INT X, INT Y )
{
	UBOOL bIsPlayingMovie = GFullScreenMovie && GFullScreenMovie->GameThreadIsMoviePlaying( TEXT("") );

#if CONSOLE || PLATFORM_UNIX
	UBOOL bIsWithinTitleBar = FALSE;
#else
	POINT CursorPos = { X, Y };
	RECT WindowRect;
	ClientToScreen( (HWND)Viewport->GetWindow(), &CursorPos );
	GetWindowRect( (HWND)Viewport->GetWindow(), &WindowRect );
	UBOOL bIsWithinWindow = ( CursorPos.x >= WindowRect.left && CursorPos.x <= WindowRect.right &&
							  CursorPos.y >= WindowRect.top && CursorPos.y <= WindowRect.bottom );

	// The user is mousing over the title bar if Y is less than zero and within the window rect
	UBOOL bIsWithinTitleBar = Y < 0 && bIsWithinWindow;

	if( GIsEditor && bIsPlayInEditorViewport && bShowSystemMouseCursor )
	{
		return Super::GetCursor(Viewport, X, Y);
	}

#endif

	if ( bDisplayHardwareMouseCursor )
	{
		if ( !bIsPlayingMovie && (Viewport->IsFullscreen() || !bIsWithinTitleBar) )
		{
			return Super::GetCursor(Viewport, X, Y);
		}
		else
	{
		return MC_None;
	}
	}

	return MC_None;
}


void UGameViewportClient::SetDropDetail(FLOAT DeltaSeconds)
{
	if (GEngine->Client)
	{
#if CONSOLE
		// Externs to detailed frame stats, split by render/ game thread CPU time and GPU time.
		extern DWORD GRenderThreadTime;
		extern DWORD GGameThreadTime;

#if XBOX
		// GPU frame times is not supported in final release. We don't want to change how this works across configurations
		// so we simply disable looking at GPU frame time on Xbox 360.
		const DWORD GGPUFrameTime = 0;
#else
		extern DWORD GGPUFrameTime;
#endif // XBOX

		// Calculate the maximum time spent, EXCLUDING idle time. We don't use DeltaSeconds as it includes idle time waiting
		// for VSYNC so we don't know how much "buffer" we have.
		FLOAT FrameTime	= Max3<DWORD>( GRenderThreadTime, GGameThreadTime, GGPUFrameTime ) * GSecondsPerCycle;
		// If DeltaSeconds is bigger than 34 ms we can take it into account as we're not VSYNCing in that case.
		if( DeltaSeconds > 0.034 )
		{
			FrameTime = Max( FrameTime, DeltaSeconds );
		}
		const FLOAT FrameRate	= FrameTime > 0 ? 1 / FrameTime : 0;
#else
		FLOAT FrameTime = DeltaSeconds;
		const FLOAT FrameRate	= DeltaSeconds > 0 ? 1 / DeltaSeconds : 0;
#endif // CONSOLE

		// we check here to see that the local player is not in the interactive mode. If they are, we don't want to 
		// set "LOD" bools, as the cinematic team is in charge of the framerate and making it acceptable when when a 
		// cine or matinee is playing (i.e. bInteractiveMode == FALSE ) 
		UBOOL bInteractiveModeIsActive = FALSE;
		if( GEngine->GamePlayers.Num() > 0 )
		{
			ULocalPlayer* Player = GEngine->GamePlayers(0);
			if( ( Player != NULL ) && ( Player->Actor != NULL ) && ( Player->Actor->bInteractiveMode == FALSE ) )
			{
				bInteractiveModeIsActive = TRUE;
			}
		}

		// Drop detail if framerate is below threshold.
		AWorldInfo* WorldInfo		= GWorld->GetWorldInfo();
		WorldInfo->bInteractiveMode = bInteractiveModeIsActive;
		WorldInfo->bDropDetail		= FrameRate < Clamp(GEngine->Client->MinDesiredFrameRate, 1.f, 100.f) && !GIsBenchmarking && !GUseFixedTimeStep && !bInteractiveModeIsActive;
		WorldInfo->bAggressiveLOD	= FrameRate < Clamp(GEngine->Client->MinDesiredFrameRate - 5.f, 1.f, 100.f) && !GIsBenchmarking && !GUseFixedTimeStep  && !bInteractiveModeIsActive;

// this is slick way to be able to do something based on the frametime and whether we are bound by one thing or another
#if 0 
		// so where we check to see if we are above some threshold and below 150 ms (any thing above that is usually blocking loading of some sort)
		// also we don't want to do the auto trace when we are blocking on async loading
		if( ( 0.070 < FrameTime ) && ( FrameTime < 0.150 ) && GIsAsyncLoading == FALSE && GWorld->GetWorldInfo()->bRequestedBlockOnAsyncLoading == FALSE && (GWorld->GetTimeSeconds() > 30.0f )  )
		{
			// now check to see if we have done a trace in the last 30 seconds otherwise we will just trace ourselves to death
			static FLOAT LastTraceTime = -9999.0f;
			if( (LastTraceTime+30.0f < GWorld->GetTimeSeconds()))
			{
				LastTraceTime = GWorld->GetTimeSeconds();
				warnf(TEXT("Auto Trace initiated!! FrameTime: %f"), FrameTime );

				// do what ever action you want here (e.g. trace <type>, GShouldLogOutAFrameOfSkelCompTick = TRUE, c.f. UnLevTic.cpp for more)
				//GShouldLogOutAFrameOfSkelCompTick = TRUE;
				//GShouldLogOutAFrameOfIsOverlapping = TRUE;
				//GShouldLogOutAFrameOfMoveActor = TRUE;
				//GShouldLogOutAFrameOfPhysAssetBoundsUpdate = TRUE;
				//GShouldLogOutAFrameOfComponentUpdates = TRUE;
			
#if CONSOLE
				warnf(TEXT("    GGameThreadTime: %d GRenderThreadTime: %d "), GGameThreadTime, GRenderThreadTime );
				if( GGameThreadTime > GRenderThreadTime )
				{
					//GShouldTraceOutAFrameOfPain = TRUE;
					//appStartCPUTrace( NAME_Game, FALSE, FALSE, 40, NULL );
					//appStopCPUTrace( NAME_Game );
				}
				else
				{
					//appStartCPUTrace( TEXT("Render"), FALSE, FALSE, 40, NULL );
					//appStopCPUTrace( TEXT("Render") );
				}
#endif // CONSOLE
			}
		}
#endif // 0 
	}
}

/**
 * Set this GameViewportClient's viewport and viewport frame to the viewport specified
 */
void UGameViewportClient::SetViewportFrame( FViewportFrame* InViewportFrame )
{
	ViewportFrame = InViewportFrame;
	SetViewport( ViewportFrame ? ViewportFrame->GetViewport() : NULL );
}

/**
 * Set this GameViewportClient's viewport to the viewport specified
 */
void UGameViewportClient::SetViewport( FViewport* InViewport )
{
	FViewport* PreviousViewport = Viewport;
	Viewport = InViewport;

	if ( PreviousViewport == NULL && Viewport != NULL )
	{
		// ensure that the player's Origin and Size members are initialized the moment we get a viewport
		eventLayoutPlayers();
	}

	if ( UIController != NULL )
	{
		UIController->SceneClient->SetRenderViewport(Viewport);
	}

#if WITH_GFx
	if(ScaleformInteraction)
	{
		ScaleformInteraction->SetRenderViewport(InViewport);
	}
#endif
}


/**
 * Retrieve the size of the main viewport.
 *
 * @param	out_ViewportSize	[out] will be filled in with the size of the main viewport
 */
void UGameViewportClient::GetViewportSize( FVector2D& out_ViewportSize )
{
	if ( Viewport != NULL )
	{
		out_ViewportSize.X = Viewport->GetSizeX();
		out_ViewportSize.Y = Viewport->GetSizeY();
	}
}

/** @return Whether or not the main viewport is fullscreen or windowed. */
UBOOL UGameViewportClient::IsFullScreenViewport()
{
	return Viewport->IsFullscreen();
}


/**
 * Determine whether a fullscreen viewport should be used in cases where there are multiple players.
 *
 * @return	TRUE to use a fullscreen viewport; FALSE to allow each player to have their own area of the viewport.
 */
UBOOL UGameViewportClient::ShouldForceFullscreenViewport() const
{
	UBOOL bResult = FALSE;
	if ( GForceFullscreen )
	{
		bResult = TRUE;
	}
	else if ( GetOuterUEngine()->GamePlayers.Num() == 0 )
	{
		bResult = TRUE;
	}
	else if ( GWorld != NULL && GWorld->GetWorldInfo() != NULL && GWorld->GetWorldInfo()->IsMenuLevel() )
	{
		bResult = TRUE;
	}
	else
	{
		ULocalPlayer* FirstPlayer = GetOuterUEngine()->GamePlayers(0);
		if ( FirstPlayer != NULL && FirstPlayer->Actor != NULL && FirstPlayer->Actor->bCinematicMode )
		{
			bResult = TRUE;
		}
	}

	return bResult;
}

/**Used to allow custom UIInteractions when scaleform is used*/
INT UGameViewportClient::GetNumCustomInteractions()
{
#if WITH_GFx
	return 1;
#else
	return 0;
#endif
}

/**Used to allow custom UIInteractions when scaleform is used.  Provides enumerated interaction classes*/
class UClass* UGameViewportClient::GetCustomInteractionClass(INT InIndex)
{
#if WITH_GFx
	return UGFxInteraction::StaticClass();
#else
	//this should not be called if the above function returns 0
	check(FALSE);
	return NULL;
#endif
}

/**Used to allow custom UIInteractions when scaleform is used.  Gives the created object from script back to native code*/
void UGameViewportClient::SetCustomInteractionObject(UInteraction* InInteraction)
{
#if WITH_GFx
	UGFxInteraction* Interaction = Cast<UGFxInteraction>(InInteraction);
	if (Interaction)
	{
		//only should be called once
		check(ScaleformInteraction == NULL);
		ScaleformInteraction = Interaction;
	}
#endif
}

/** Used to notify GFx of a change in the splitscreen layout */
void UGameViewportClient::NotifySplitscreenLayoutChanged()
{
#if WITH_REALD
	RealD::NotifySplitscreenLayoutChanged();
#endif
#if WITH_GFx
	if (ScaleformInteraction != NULL)
	{
		ScaleformInteraction->NotifySplitscreenLayoutChanged();
	}
#endif
}

/** Whether we should precache during the next frame. */
UBOOL GPrecacheNextFrame = FALSE;
/** Whether we are currently precaching. */
UBOOL GIsCurrentlyPrecaching = FALSE;


class FCaptureRenderTarget : public FRenderTarget
{
public:
	FCaptureRenderTarget()
	{
	}

	virtual const FSurfaceRHIRef& GetRenderTargetSurface() const
	{
		return GSceneRenderTargets.GetRenderTargetSurface(CapturedSceneColor);
	}
	virtual UINT GetSizeX() const
	{
		return GSceneRenderTargets.GetBufferSizeX();
	}
	virtual UINT GetSizeY() const
	{
		return GSceneRenderTargets.GetBufferSizeY();
	}
};

void UGameViewportClient::Draw(FViewport* Viewport,FCanvas* Canvas)
{
#if WITH_GFx
	ENQUEUE_UNIQUE_RENDER_COMMAND(
		FGFxBeginFrame,
	{
		GGFxEngine->GetRenderer2D()->BeginFrame();
	});
#endif

    GSystemSettings.UpdateSplitScreenSettings();

#if !CONSOLE
	if(GPrecacheNextFrame)
	{
		GIsCurrentlyPrecaching = TRUE;
		FSceneViewFamilyContext PrecacheViewFamily(Viewport,GWorld->Scene,ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds());
		PrecacheViewFamily.Views.AddItem(
			new FSceneView(
			&PrecacheViewFamily,
			NULL,
			-1,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			1,
			1,
			FMatrix(
			FPlane(1.0f / WORLD_MAX,0.0f,0.0f,0.0f),
			FPlane(0.0f,1.0f / WORLD_MAX,0.0f,0.0f),
			FPlane(0.0f,0.0f,1.0f / WORLD_MAX,0.0f),
			FPlane(0.5f,0.5f,0.5f,1.0f)
			),
			FMatrix::Identity,
			FLinearColor::Black,
			FLinearColor::Black,
			FLinearColor::White,
			TSet<UPrimitiveComponent*>(),
			FRenderingPerformanceOverrides(E_ForceInit)
			)
			);
		BeginRenderingViewFamily(Canvas,&PrecacheViewFamily);
		// Flush rendering commands as RT is accessing GIsCurrentlyPrecaching
		FlushRenderingCommands();

		GPrecacheNextFrame = FALSE;
		GIsCurrentlyPrecaching = FALSE;
	}
#endif

	// update ScriptedTextures
	for (INT i = 0; i < UScriptedTexture::GScriptedTextures.Num(); i++)
	{
		UScriptedTexture::GScriptedTextures(i)->CheckUpdate();
	}

#if WITH_GFx
	UGameViewportClient* GameViewportClient = GEngine->GameViewport;
	if(GGFxEngine && GameViewportClient && GameViewportClient->ScaleformInteraction)
	{
		GGFxEngine->RenderTextures();
	}
#endif // WITH_GFx

	// Create a temporary canvas if there isn't already one.
	UCanvas* CanvasObject = FindObject<UCanvas>(UObject::GetTransientPackage(),TEXT("CanvasObject"));
	if( !CanvasObject )
	{
		CanvasObject = ConstructObject<UCanvas>(UCanvas::StaticClass(),UObject::GetTransientPackage(),TEXT("CanvasObject"));
		CanvasObject->AddToRoot();
	}
	CanvasObject->Canvas = Canvas;	

	UBOOL bUIDisableWorldRendering = FALSE;
	UBOOL bUICaptureWorldRendering = FALSE;

#if WITH_GFx
	if (GGFxEngine != NULL)
	{
		for (INT i = 0; i < GGFxEngine->OpenMovies.Num(); i++)
		{
			if (GGFxEngine->OpenMovies(i) != NULL && GGFxEngine->OpenMovies(i)->pUMovie != NULL)
			{
				if (GGFxEngine->OpenMovies(i)->pUMovie->bDisableWorldRendering)
				{
					bUIDisableWorldRendering = TRUE;
					break;
				}
				if (GGFxEngine->OpenMovies(i)->pUMovie->bCaptureWorldRendering)
				{
					bUICaptureWorldRendering = TRUE;					
					break;
				}
			}
		}
	}
#endif
	FGameViewDrawer GameViewDrawer;

	// create the view family for rendering the world scene to the viewport's render target
	FSceneViewFamilyContext ViewFamily(
		Viewport,
		GWorld->Scene,
		ShowFlags,
		GWorld->GetTimeSeconds(),
		GWorld->GetDeltaSeconds(),
		GWorld->GetRealTimeSeconds(), 
		TRUE,
		TRUE,
		FALSE,
		GEngine->bEnableColorClear,		// disable the initial scene color clear in game
		TRUE
		);
	TMap<INT,FSceneView*> PlayerViewMap;
#if WITH_REALD
	TMap<INT,FSceneView*> PlayerViewMap2;
#endif
	UBOOL bHaveAudioSettingsBeenSet = FALSE;
	for(INT PlayerIndex = 0;PlayerIndex < GEngine->GamePlayers.Num();PlayerIndex++)
	{
		ULocalPlayer* Player = GEngine->GamePlayers(PlayerIndex);
		if (Player->Actor)
		{
			// Calculate the player's view information.
			FVector		ViewLocation;
			FRotator	ViewRotation;
			FSceneView* View = NULL;
#if WITH_REALD
			FMatrix loffs, roffs;
			RealD::RealDStereo::ViewData RealDViewData;
			if (RealD::IsStereoEnabled())
			{
				RealDViewData.GetLeftViewOffset(loffs);
				RealDViewData.GetRightViewOffset(roffs);
				View = Player->CalcSceneViewOffs( &ViewFamily, ViewLocation, ViewRotation, Viewport, &loffs, &GameViewDrawer );
			}
			else
#endif
			{
				View = Player->CalcSceneView( &ViewFamily, ViewLocation, ViewRotation, Viewport, &GameViewDrawer );
			}
			if(View)
			{
#if WITH_REALD
				FSceneView* View2 = NULL;
				if (RealD::IsStereoEnabled())
				{
					RealDViewData.InitView1(View);
					View2 = Player->CalcSceneViewOffs( &ViewFamily, ViewLocation, ViewRotation, Viewport, &roffs );
					View2->State = Player->ViewState2;
					RealDViewData.InitView2(View2);
					PlayerViewMap2.Set(PlayerIndex,View2);
				}
#endif

				if(View->Family->ShowFlags & SHOW_Wireframe)
				{
					// Wireframe color is emissive-only, and mesh-modifying materials do not use material substitution, hence...
					View->DiffuseOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
					View->SpecularOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
				}
				else if (bOverrideDiffuseAndSpecular)
				{
					View->DiffuseOverrideParameter = FVector4(GEngine->LightingOnlyBrightness.R, GEngine->LightingOnlyBrightness.G, GEngine->LightingOnlyBrightness.B, 0.0f);
					View->SpecularOverrideParameter = FVector4(.1f, .1f, .1f, 0.0f);
				}

				// Save the location of the view.
				Player->LastViewLocation = ViewLocation;

				// Tiled rendering for high-res screenshots.
				if ( GIsTiledScreenshot )
				{
					Viewport->CalculateTiledScreenshotSettings(View);
				}

				PlayerViewMap.Set(PlayerIndex,View);

				// Update the listener.
				check(GEngine->Client);
				UAudioDevice* AudioDevice = GEngine->Client->GetAudioDevice();
				if( AudioDevice )
				{
					FMatrix CameraToWorld		= View->ViewMatrix.Inverse();

					FVector ProjUp				= CameraToWorld.TransformNormal(FVector(0,1000,0));
					FVector ProjRight			= CameraToWorld.TransformNormal(FVector(1000,0,0));
					FVector ProjFront			= ProjRight ^ ProjUp;

					ProjUp.Z = Abs( ProjUp.Z ); // Don't allow flipping "up".

					ProjUp.Normalize();
					ProjRight.Normalize();
					ProjFront.Normalize();

					AudioDevice->SetListener(PlayerIndex, GEngine->GamePlayers.Num(), ViewLocation, ProjUp, ProjRight, ProjFront, !View->bCameraCut);

					// Update reverb settings based on the view of the first player we encounter.
					if ( !bHaveAudioSettingsBeenSet )
					{
						bHaveAudioSettingsBeenSet = TRUE;

						FReverbSettings ReverbSettings;
						FInteriorSettings InteriorSettings;
						INT ReverbVolumeIndex = GWorld->GetWorldInfo()->GetAudioSettings( ViewLocation, &ReverbSettings, &InteriorSettings );
						AudioDevice->SetAudioSettings( ReverbVolumeIndex, ReverbSettings, InteriorSettings );
					}
				}

				if (!bDisableWorldRendering && !bUIDisableWorldRendering)
				{
					UBOOL bCanvasTransform = TRUE;
#if WITH_REALD
					if (RealD::IsStereoEnabled())
					{
						bCanvasTransform = FALSE;
						RealD::RealDStereo::PushLeftViewCanvas(Canvas, CanvasObject, View);
					}
#endif
					if (bCanvasTransform)
					{
						// Set the canvas transform for the player's view rectangle.
						CanvasObject->Init();
						CanvasObject->SizeX = appTrunc(View->SizeX);
						CanvasObject->SizeY = appTrunc(View->SizeY);
						CanvasObject->SceneView = View;
						CanvasObject->Update();
						Canvas->PushAbsoluteTransform(FTranslationMatrix(FVector(View->X,View->Y,0)));
					}

					// PreRender the player's view.
					Player->Actor->eventPreRender(CanvasObject);

					Canvas->PopTransform();

#if WITH_REALD
					if (RealD::IsStereoEnabled())
					{
						// REALD : second view
						RealD::RealDStereo::PushRightViewCanvas(Canvas, CanvasObject, View2);

						//PreRender the player's view.
						Player->Actor->eventPreRender(CanvasObject);

						Canvas->PopTransform();
					}
#endif
				}

				// Add view information for resource streaming.
				GStreamingManager->AddViewInformation( View->ViewOrigin, View->SizeX, View->SizeX * View->ProjectionMatrix.M[0][0] );

#if WITH_REALD
				if (RealD::IsStereoEnabled())
				{
					// cozinga : second view
					// Add view information for resource streaming.
					GStreamingManager->AddViewInformation( View2->ViewOrigin, View2->SizeX, View2->SizeX * View2->ProjectionMatrix.M[0][0] );
				}
#endif

				// Add scene captures - if their streaming is not disabled (SceneCaptureStreamingMultiplier == 0.0f)
				if (View->Family->Scene && (GSystemSettings.SceneCaptureStreamingMultiplier > 0.0f))
				{
					View->Family->Scene->AddSceneCaptureViewInformation(GStreamingManager, View);
				}

#if WITH_REALD
				if (RealD::IsStereoEnabled())
				{
					// cozinga : second view
					// Add scene captures - if their streaming is not disabled (SceneCaptureStreamingMultiplier == 0.0f)
					if (View2->Family->Scene && (GSystemSettings.SceneCaptureStreamingMultiplier > 0.0f))
					{
						View2->Family->Scene->AddSceneCaptureViewInformation(GStreamingManager, View2);
					}
				}
#endif
			}
		}
	}

	// Update level streaming.
	GWorld->UpdateLevelStreaming( &ViewFamily );
	
#if WITH_GFx
	if (!bDisableWorldRendering && bUICaptureWorldRendering && PlayerViewMap.Num() > 0)
	{
		if (!bCapturedWorldRendering)
		{
			FCaptureRenderTarget* CaptureRenderTarget = new FCaptureRenderTarget();

			FSceneViewFamily CaptureViewFamily = ViewFamily;
			CaptureViewFamily.bScreenCaptureRenderTarget = TRUE;
			CaptureViewFamily.RenderTarget = CaptureRenderTarget;

			BeginRenderingViewFamily(Canvas,&CaptureViewFamily);

			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				FFreeCapturedRenderTarget,
				FCaptureRenderTarget*,RenderTarget,CaptureRenderTarget,
			{
				delete RenderTarget;
			});

			bCapturedWorldRendering = TRUE;
		}

		RenderCapturedSceneColor(Canvas,&ViewFamily);
	}
	else
#endif
	{
		bCapturedWorldRendering = FALSE;

		// Draw the player views.
		if (!bDisableWorldRendering && !bUIDisableWorldRendering && PlayerViewMap.Num() > 0)
		{
			BeginRenderingViewFamily(Canvas,&ViewFamily);
		}
		else
		{
			Canvas->Flush();

			// On a tiled renderer, it's important for performance to clear a surface
			// after binding, if the intension is to overwrite all the pixels
			if (GMobileTiledRenderer)
			{	
				FLinearColor ClearColor = FLinearColor(0,0,0);
				ClearAll(Canvas, ClearColor);
			}
		}
	}

	// Trioviz camera hooks
#if DWTRIOVIZSDK
	//Check if player is cinematic to turn cinematic mode on
	UBOOL DwUseCinematicMode = FALSE;
	for(INT PlayerIndex = 0;PlayerIndex < GEngine->GamePlayers.Num() && !DwUseCinematicMode;PlayerIndex++)
	{
		ULocalPlayer* Player = GEngine->GamePlayers(PlayerIndex);
		if(Player->Actor && Player->Actor->bCinematicMode)
		{
			DwUseCinematicMode = TRUE;
		}
	}

	DwTriovizImpl_PrepareCapturing(CanvasObject, DwUseCinematicMode);

#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 || DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
	UINT UIDrawCount = ((DwTriovizImpl_IsTriovizActive() && (g_DwUseFramePacking || DwTriovizImpl_NeedsFinalize()))? 2 : 1);
#else
	UINT UIDrawCount = ((DwTriovizImpl_IsTriovizActive() && (DwTriovizImpl_NeedsFinalize()) && (!DwTriovizImpl_Is3dMovieHackeryNeeded()))? 2 : 1);
#endif
	for (UINT CurrentIndex = 0; CurrentIndex < UIDrawCount; ++CurrentIndex)
	{
		//tell trioviz which eye we are currently rendering
		DwTriovizImpl_SetWhichEyeRendering(CurrentIndex == 0);
		if (DwTriovizImpl_SetCanvasForUIRendering(Canvas, Viewport->GetSizeX(), Viewport->GetSizeY()))
		{
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
			static FDwSceneRenderTargetBackbufferStereoProxy BackbufferStereoLeft;
			static FDwSceneRenderTargetBackbufferStereoProxy BackbufferStereoRight;

			if(g_DwUseFramePacking)
			{
				BackbufferStereoLeft.SetStereo(0);
				BackbufferStereoRight.SetStereo(1);

				Canvas->SetRenderTargetDirty(FALSE);
				Canvas->SetRenderTarget(CurrentIndex == 0 ? &BackbufferStereoLeft : &BackbufferStereoRight);
				Canvas->Flush();
			}
#endif

			DwTriovizImpl_UseHDRSurface(FALSE);
#endif // DWTRIOVIZSDK

			// Clear areas of the rendertarget (backbuffer) that aren't drawn over by the views.
			// (No need to do this if we are already clearing the back buffer - doesn't work on PC?)
			if( !GUsingMobileRHI || !GEngine->bEnableColorClear )
			{
				// Find largest rectangle bounded by all rendered views.
				UINT MinX=Viewport->GetSizeX(),	MinY=Viewport->GetSizeY(), MaxX=0, MaxY=0;
				UINT TotalArea = 0;
				for( INT ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ++ViewIndex )
				{
					const FSceneView* View = ViewFamily.Views(ViewIndex);
					INT UnscaledViewX = 0;
					INT UnscaledViewY = 0;
					UINT UnscaledViewSizeX = 0;
					UINT UnscaledViewSizeY = 0;

					INT ScaledViewX = View->X;
					INT ScaledViewY = View->Y;
					INT ScaledViewSizeX = View->SizeX;
					INT ScaledViewSizeY = View->SizeY;

					// Unscale the view coordinates if needed
					GSystemSettings.UnScaleScreenCoords(
						UnscaledViewX, UnscaledViewY, 
						UnscaledViewSizeX, UnscaledViewSizeY, 
						ScaledViewX, ScaledViewY, 
						ScaledViewSizeX, ScaledViewSizeY);

					MinX = Min<UINT>(UnscaledViewX, MinX);
					MinY = Min<UINT>(UnscaledViewY, MinY);
					MaxX = Max<UINT>(UnscaledViewX + UnscaledViewSizeX, MaxX);
					MaxY = Max<UINT>(UnscaledViewY + UnscaledViewSizeY, MaxY);
					TotalArea += UnscaledViewSizeX * UnscaledViewSizeY;
				}

				// If the views don't cover the entire bounding rectangle, clear the entire buffer.
				if ( ViewFamily.Views.Num() == 0 || TotalArea != (MaxX-MinX)*(MaxY-MinY) || bDisableWorldRendering )
				{
					DrawTile(Canvas,0,0,Viewport->GetSizeX(),Viewport->GetSizeY(),0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,FALSE);
				}
				else
				{
					// clear left
					if( MinX > 0 )
					{
						DrawTile(Canvas,0,0,MinX,Viewport->GetSizeY(),0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,FALSE);
					}
					// clear right
					if( MaxX < Viewport->GetSizeX() )
					{
						DrawTile(Canvas,MaxX,0,Viewport->GetSizeX(),Viewport->GetSizeY(),0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,FALSE);
					}
					// clear top
					if( MinY > 0 )
					{
						DrawTile(Canvas,MinX,0,MaxX,MinY,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,FALSE);
					}
					// clear bottom
					if( MaxY < Viewport->GetSizeY() )
					{
						DrawTile(Canvas,MinX,MaxY,MaxX,Viewport->GetSizeY(),0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,FALSE);
					}
				}
			}

			// Remove temporary debug lines.
			if (GWorld->LineBatcher != NULL && GWorld->LineBatcher->BatchedLines.Num())
			{
				GWorld->LineBatcher->BatchedLines.Empty();
				GWorld->LineBatcher->BatchedPoints.Empty();
				GWorld->LineBatcher->BeginDeferredReattach();
			}

			// Render the UI if enabled.
#if WITH_GFx
			if( GTickAndRenderUI || (GRenderScaleform && GScaleformEnabled) )
#else
			if( GTickAndRenderUI )
#endif // WITH_GFx
			{
				SCOPE_CYCLE_COUNTER(STAT_UIDrawingTime);

				// render HUD
				for(INT PlayerIndex = 0;PlayerIndex < GEngine->GamePlayers.Num();PlayerIndex++)
				{
					ULocalPlayer* Player = GEngine->GamePlayers(PlayerIndex);
					if(Player->Actor)
					{
						FSceneView* View = PlayerViewMap.FindRef(PlayerIndex);
#if WITH_REALD
						FLOAT LastHUDRenderTime = Player->Actor->myHUD ? Player->Actor->myHUD->LastHUDRenderTime : -1.0f;
						FSceneView* View2 = NULL;
						while (View != NULL)
#else
						if (View != NULL)
#endif
						{
							UBOOL bScaleCanvas = TRUE;
#if WITH_REALD
							if (RealD::IsStereoEnabled())
							{
								bScaleCanvas = FALSE;
								// We render directly to the screen buffer if stereo is on
								// since it is assumed that it's for a single player (i.e. no split screen)
								if (View2 == NULL)
								{
									RealD::RealDStereo::PushLeftViewCanvas(Canvas, CanvasObject, View);
								}
								else
								{
									RealD::RealDStereo::PushRightViewCanvas(Canvas, CanvasObject, View);
								}
							}
#endif
							if (bScaleCanvas)
							{
								INT UnscaledViewX = 0;
								INT UnscaledViewY = 0;
								UINT UnscaledViewSizeX = 0;
								UINT UnscaledViewSizeY = 0;

								INT ScaledViewX = View->X;
								INT ScaledViewY = View->Y;
								INT ScaledViewSizeX = View->SizeX;
								INT ScaledViewSizeY = View->SizeY;

								// Allow the PlayerController to adjust the canvas size before rendering the HUD (e.g. useful for games that want to draw outside the splitscreen viewports)
								Player->Actor->eventAdjustHUDRenderSize(ScaledViewX, ScaledViewY, ScaledViewSizeX, ScaledViewSizeY, Viewport->GetSizeX(), Viewport->GetSizeY());

								// Unscale the view coordinates if needed
								GSystemSettings.UnScaleScreenCoords(
									UnscaledViewX, UnscaledViewY, 
									UnscaledViewSizeX, UnscaledViewSizeY, 
									ScaledViewX, ScaledViewY, 
									ScaledViewSizeX, ScaledViewSizeY);

								// Set the canvas transform for the player's view rectangle.
								CanvasObject->Init();
								CanvasObject->SizeX = UnscaledViewSizeX;
								CanvasObject->SizeY = UnscaledViewSizeY;
								CanvasObject->SceneView = View;
								CanvasObject->Update();
								{
									// rendering to directly to viewport target
									FVector CanvasOrigin(UnscaledViewX,UnscaledViewY, 0.f);
#if DWTRIOVIZSDK
									Canvas->PushRelativeTransform(FTranslationMatrix(CanvasOrigin));
#else
									Canvas->PushAbsoluteTransform(FTranslationMatrix(CanvasOrigin));
#endif
								}
							}

							// Render the player's HUD.
							if( Player->Actor->myHUD )
							{
								SCOPE_CYCLE_COUNTER(STAT_HudTime);

								Player->Actor->myHUD->Canvas = CanvasObject;
								Player->Actor->myHUD->eventPostRender();
								// A side effect of PostRender is that the playercontroller could be destroyed
								if (!Player->Actor->IsPendingKill())
								{
									Player->Actor->myHUD->Canvas = NULL;
								}
							}

							// A side effect of PostRender is that the playercontroller could be destroyed
							if (!Player->Actor->IsPendingKill())
							{
								// allow Interactions to PostRender
								for(INT InteractionIndex = 0;InteractionIndex < Player->Actor->Interactions.Num();InteractionIndex++)
								{
									if(Player->Actor->Interactions(InteractionIndex))
									{
										Player->Actor->Interactions(InteractionIndex)->eventPostRender(CanvasObject);
									}
								}
							}

							Canvas->PopTransform();

							// draw subtitles
							if (PlayerIndex == 0 && FSubtitleManager::GetSubtitleManager()->HasSubtitles())
							{
								FVector2D MinPos(0.f, 0.f);
								FVector2D MaxPos(1.f, 1.f);
								eventGetSubtitleRegion(MinPos, MaxPos);

								UINT SizeX = Canvas->GetRenderTarget()->GetSizeX();
								UINT SizeY = Canvas->GetRenderTarget()->GetSizeY();

#if WITH_REALD
								if (RealD::IsStereoEnabled())
								{
									if (View2 == NULL)
									{
										RealD::RealDStereo::PushLeftViewCanvas(Canvas, NULL, NULL);
									}
									else
									{
										RealD::RealDStereo::PushRightViewCanvas(Canvas, NULL, NULL);
									}
								}
#endif

								FIntRect SubtitleRegion(appTrunc(SizeX * MinPos.X), appTrunc(SizeY * MinPos.Y), appTrunc(SizeX * MaxPos.X), appTrunc(SizeY * MaxPos.Y));
								FSubtitleManager::GetSubtitleManager()->DisplaySubtitles( Canvas, SubtitleRegion );

#if WITH_REALD
								if (RealD::IsStereoEnabled())
								{
									Canvas->PopTransform();
								}
#endif
							}

#if WITH_REALD
							View = NULL;
							if ( RealD::IsStereoEnabled() )
							{
								if ( View2 == NULL)
								{
									View2 = PlayerViewMap2.FindRef(PlayerIndex);
									View = View2;
									if ( LastHUDRenderTime != -1.0f && Player->Actor->myHUD != NULL )
									{
										Player->Actor->myHUD->LastHUDRenderTime = LastHUDRenderTime;
									}
								}
							}
#endif
						}
					}
				}

				// Reset the canvas for rendering to the full viewport.
				CanvasObject->Init();
				CanvasObject->SizeX = Viewport->GetSizeX();
				CanvasObject->SizeY = Viewport->GetSizeY();
				CanvasObject->SceneView = NULL;
				CanvasObject->Update();		

				//ensure canvas has been flushed before rendering UI
				Canvas->Flush();

#if WITH_GFx
				// let Scaleform render
				// handle Trioviz/Scaleform interaction
#if DWTRIOVIZSDK
				if(GameViewportClient && GGFxEngine && (GRenderScaleform && GScaleformEnabled))
				{	
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
					GGFxEngine->RenderUI(FALSE, SDPG_Foreground, g_DwUseFramePacking ? Canvas->GetRenderTarget() : NULL);
					GGFxEngine->RenderUI(FALSE, SDPG_PostProcess, g_DwUseFramePacking ? Canvas->GetRenderTarget() : NULL);
#else
					GGFxEngine->RenderUI(FALSE, SDPG_Foreground);
					GGFxEngine->RenderUI(FALSE, SDPG_PostProcess);
#endif
				}
#else // DWTRIOVIZSDK
				if(GameViewportClient && GGFxEngine && (GRenderScaleform && GScaleformEnabled))
				{
					GGFxEngine->RenderUI(FALSE, SDPG_Foreground);
					GGFxEngine->RenderUI(FALSE, SDPG_PostProcess);
				}
#endif 
#endif // WITH_GFx

				UBOOL bPostRenderSingle = TRUE;
#if WITH_REALD
				if ( RealD::IsStereoEnabled() )
				{
					bPostRenderSingle = FALSE;
					RealD::RealDStereo::PushLeftViewCanvas(Canvas, CanvasObject, NULL);
					eventPostRender(CanvasObject);
					Canvas->PopTransform();

					RealD::RealDStereo::PushRightViewCanvas(Canvas, CanvasObject, NULL);
					eventPostRender(CanvasObject);
					Canvas->PopTransform();

					// Reset the canvas for rendering to the full viewport.
					CanvasObject->Init();
					CanvasObject->SizeX = Viewport->GetSizeX();
					CanvasObject->SizeY = Viewport->GetSizeY();
					CanvasObject->SceneView = NULL;
					CanvasObject->Update();       
				}
#endif
				if (bPostRenderSingle)
				{
					eventPostRender(CanvasObject);
				}

#if DWTRIOVIZSDK
				Canvas->PopTransform();
				Canvas->PopMaskRegion();
			}
#endif
		}

		// Grab the player camera location and orientation so we can pass that along to the stats drawing code.
		FVector PlayerCameraLocation = FVector::ZeroVector;
		FRotator PlayerCameraRotation = FRotator::ZeroRotator;
		{
			for(AController* Controller = GWorld->GetWorldInfo()->ControllerList; Controller != NULL; Controller = Controller->NextController)
			{
				APlayerController* PC = Cast<APlayerController>( Controller );
				if(PC)
				{
					PC->eventGetPlayerViewPoint( PlayerCameraLocation, PlayerCameraRotation );		
				}
			}
		}

		UBOOL bDrawStatsSingle = TRUE;
#if WITH_REALD
		if ( RealD::IsStereoEnabled() )
		{
			bDrawStatsSingle = FALSE;
			RealD::RealDStereo::PushLeftViewCanvas(Canvas, CanvasObject, NULL);
			DrawStatsHUD( Viewport, Canvas, CanvasObject, DebugProperties, PlayerCameraLocation, PlayerCameraRotation );
			Canvas->PopTransform();


			RealD::RealDStereo::PushRightViewCanvas(Canvas, CanvasObject, NULL);
			DrawStatsHUD( Viewport, Canvas, CanvasObject, DebugProperties, PlayerCameraLocation, PlayerCameraRotation );
			Canvas->PopTransform();

			// Reset the canvas for rendering to the full viewport.
			CanvasObject->Init();
			CanvasObject->SizeX = Viewport->GetSizeX();
			CanvasObject->SizeY = Viewport->GetSizeY();
			CanvasObject->SceneView = NULL;
			CanvasObject->Update();       
		}
#endif
#if DWTRIOVIZSDK
		// deal with multiple canvas renders
		if(DwTriovizImpl_SetCanvasForUIRendering(Canvas, Viewport->GetSizeX(), Viewport->GetSizeY()))
#endif
		{

			if (bDrawStatsSingle)
			{
				DrawStatsHUD( Viewport, Canvas, CanvasObject, DebugProperties, PlayerCameraLocation, PlayerCameraRotation );
#if DWTRIOVIZSDK
				Canvas->PopTransform();
				Canvas->PopMaskRegion();
#endif
			}
		}

#if WITH_REALD
		RealD::RealDStereo::UpdateDepthData(Viewport);
#endif

#if DWTRIOVIZSDK
		// allow the Trioviz menu to render
		if(DwTriovizImpl_SetCanvasForUIRendering(Canvas, Viewport->GetSizeX(), Viewport->GetSizeY()))
		{
			DwTriovizImpl_RenderMenu(Canvas, 0, 0);
			Canvas->PopTransform();
			Canvas->PopMaskRegion();
		}
#endif

		if( GIsDumpingMovie || GScreenShotRequest || GIsHighResScreenshot || GEngine->bScreenshotRequested )
		{
			Canvas->Flush();

			// Read the contents of the viewport into an array.
			TArray<FColor>	Bitmap;
			if(Viewport->ReadPixels(Bitmap))
			{
				check(Bitmap.Num() == Viewport->GetSizeX() * Viewport->GetSizeY());

				FString TypeName;

				if(GScreenShotRequest ||  GEngine->bScreenshotRequested)
				{
					TypeName = GScreenShotName.IsEmpty() ? TEXT("ScreenShot") : GScreenShotName;
				}
				if(GIsDumpingMovie)
				{
					TypeName = TEXT("MovieFrame");
				}
				if(GIsHighResScreenshot)
				{
					TypeName = TEXT("HighresScreenshot");
				}

				check(!TypeName.IsEmpty());
		
				const FString ScreenFileName = appScreenShotDir()  * TypeName;
		
				// Save the contents of the array to a bitmap file.
				appCreateBitmap(*ScreenFileName,Viewport->GetSizeX(),Viewport->GetSizeY(),&Bitmap(0),GFileManager);			
			}
			// reset any G vars that deal with screenshots.  We need to expressly reset the GScreenShotName here as there is code that will just keep
			// writing to the same .bmp if that GScreenShotName has an .bmp extension
			GScreenShotRequest=FALSE;
			GEngine->bScreenshotRequested = FALSE;
			// Reeanble screen messages - if we are NOT capturing a movie
			GAreScreenMessagesEnabled = GScreenMessagesRestoreState;
			GScreenShotName = TEXT("");
		}

#if DWTRIOVIZSDK
#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
		DwTriovizImpl_XboxFramePackingCopyBackbuffer(CurrentIndex);
#endif
		// this is the end of the UIDrawCount loop!
		DwTriovizImpl_fake3DRendering();
	}
#endif

#if WITH_GFx

	ENQUEUE_UNIQUE_RENDER_COMMAND(
		FGFxEndFrame,
	{
		GGFxEngine->GetRenderer2D()->EndFrame();
	});
#endif

#if DWTRIOVIZSDK

#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
	if(g_DwUseFramePacking)
	{
		Canvas->SetRenderTargetDirty(FALSE);
		Canvas->SetRenderTarget(Viewport);
	}
#endif

#endif

#if WITH_GFx
	// trigger to stop playing loading screen movies
	if (GGFxEngine)
	{
		GGFxEngine->GameHasRendered++;
	}
#endif // WITH_GFx
}

void UGameViewportClient::Precache()
{
	if(!GIsEditor)
	{
		// Precache sounds...
		UAudioDevice* AudioDevice = GEngine->Client ? GEngine->Client->GetAudioDevice() : NULL;
		if( AudioDevice )
		{
			debugf(TEXT("Precaching sounds..."));
			for(TObjectIterator<USoundNodeWave> It;It;++It)
			{
				USoundNodeWave* SoundNodeWave = *It;
				AudioDevice->Precache( SoundNodeWave );
			}
			debugf(TEXT("Precaching sounds completed..."));
		}
	}

	// Log time till first precache is finished.
	static UBOOL bIsFirstCallOfFunction = TRUE;
	if( bIsFirstCallOfFunction )
	{
		debugf(TEXT("%5.2f seconds passed since startup."),appSeconds()-GStartTime);
		bIsFirstCallOfFunction = FALSE;
	}
}

void UGameViewportClient::LostFocus(FViewport* Viewport)
{
}

void UGameViewportClient::ReceivedFocus(FViewport* Viewport)
{
}

UBOOL UGameViewportClient::IsFocused(FViewport* Viewport)
{
	return Viewport->HasFocus() || Viewport->HasMouseCapture();
}

void UGameViewportClient::CloseRequested(FViewport* Viewport)
{
	check(Viewport == this->Viewport);
	if( GFullScreenMovie )
	{
		// force movie playback to stop
		GFullScreenMovie->GameThreadStopMovie(0,FALSE,TRUE);
	}
	GEngine->Client->CloseViewport(this->Viewport);
	SetViewportFrame(NULL);
}

/**
 * Retrieves a reference to a LocalPlayer.
 *
 * @param	PlayerIndex		if specified, returns the player at this index in the GamePlayers array.  Otherwise, returns
 *							the player associated with the owner scene.
 *
 * @return	the player that owns this scene or is located in the specified index of the GamePlayers array.
 */
ULocalPlayer* UGameViewportClient::GetPlayerOwner(INT PlayerIndex)
{
	ULocalPlayer* Result = NULL;

	if (GEngine != NULL &&
		GEngine->GamePlayers.IsValidIndex(PlayerIndex))
	{
		Result = GEngine->GamePlayers(PlayerIndex);
	}

	return Result;
}

/** 
 * Called after the primary player has been changed so that the UI references to the owner are switched 
 *
 *	@param IDMappings - Mapping of old id (value) to new id (index into array)
 */
void UGameViewportClient::FixupOwnerReferences(const TArray<INT>& IDMappings)
{
#if WITH_GFx
	if (GGFxEngine)
	{
		const INT NumMovies = GGFxEngine->GetNumOpenMovies();
		for (INT MovieIdx = 0; MovieIdx < NumMovies; MovieIdx++)
		{
			FGFxMovie* Movie = GGFxEngine->GetOpenMovie(MovieIdx);
			if (Movie && Movie->pUMovie)
			{
				for (INT i = 0; i < IDMappings.Num(); i++)
				{
					if (Movie->pUMovie->LocalPlayerOwnerIndex == IDMappings(i))
					{
						Movie->pUMovie->LocalPlayerOwnerIndex = i;
						break;
					}
				}
			}
		}
	}
#endif
}

void UGameViewportClient::ForceUpdateMouseCursor(UBOOL bSetCursor)
{
	if (Viewport)
	{
		Viewport->UpdateMouseCursor(bSetCursor);
	}
}

void UGameViewportClient::SetMouse(INT X, INT Y)
{
	if (Viewport)
	{
		Viewport->SetMouse(X,Y);
	}
}

/** Function to allow script to turn on Scaleform processing/rendering */
void UGameViewportClient::EnableScaleform()
{
#if WITH_GFx
	GScaleformEnabled = TRUE;
#endif
}

/** Function to allow script to turn on Scaleform processing/rendering */
void UGameViewportClient::DisableScaleform()
{
#if WITH_GFx
	GScaleformEnabled = FALSE;
#endif
}

/** Function to allow script to find out if Scaleform is enabled/disabled */
UBOOL UGameViewportClient::IsScaleformEnabled()
{
#if WITH_GFx
	return GScaleformEnabled;
#else
	return FALSE;
#endif
}

/** Debug function to easily allow script to turn on / off the two UI systems for developing during the transition from the old UI to the new GFx UI */
void UGameViewportClient::DebugSetUISystemEnabled(UBOOL bOldUISystemActive, UBOOL bGFxUISystemActive)
{
	GTickAndRenderUI = bOldUISystemActive;
#if WITH_GFx
	GRenderScaleform = bGFxUISystemActive;
#endif
}

/**
 * Helper function to toggles, enable or disable the specified show flag. Called by Exec().
 *
 * @param Cmd		Exec command line, as passed on from Exec().
 * @param Ar		Output device used for reporting the result.
 * @param SetMode	Specifies whether the flag should be toggled, enabled or disabled.
 * @return			TRUE if the flag was modified.
 */
UBOOL UGameViewportClient::SetShowFlags(const TCHAR* Cmd,FOutputDevice& Ar, ESetMode SetMode )
{
	struct { const TCHAR* Name; EShowFlags Flag; }	Flags[] =
	{
		{ TEXT("Bounds"),				SHOW_Bounds					},
		{ TEXT("BSP"),					SHOW_BSP					},
		{ TEXT("BSPSplit"),				SHOW_BSPSplit				},
		{ TEXT("Camfrustums"),			SHOW_CamFrustums			},
		{ TEXT("Collision"),			SHOW_Collision				},
		{ TEXT("Constraints"),			SHOW_Constraints			},
		{ TEXT("Cover"),				SHOW_Cover					},
		{ TEXT("DecalInfo"),			SHOW_DecalInfo				},
		{ TEXT("Decals"),				SHOW_Decals					},
		{ TEXT("DynamicShadows"),		SHOW_DynamicShadows			},
		{ TEXT("Fog"),					SHOW_Fog					},
		{ TEXT("HitProxies"),			SHOW_HitProxies				},
		{ TEXT("LensFlares"),			SHOW_LensFlares				},
		{ TEXT("LevelColoration"),		SHOW_LevelColoration		},
		{ TEXT("MeshEdges"),			SHOW_MeshEdges				},
		{ TEXT("NavNodes"),				SHOW_NavigationNodes		},
		{ TEXT("NonZeroExtent"),		SHOW_CollisionNonZeroExtent	},
		{ TEXT("Particles"),			SHOW_Particles				},
		{ TEXT("Paths"),				SHOW_Paths					},
		{ TEXT("VertexColors"),			SHOW_VertexColors			},
		{ TEXT("PostProcess"),			SHOW_PostProcess			},
		{ TEXT("RigidBody"),			SHOW_CollisionRigidBody		},
		{ TEXT("SceneCapture"),			SHOW_SceneCaptureUpdates	},
		{ TEXT("ShadowFrustums"),		SHOW_ShadowFrustums			},
		{ TEXT("SkeletalMeshes"),		SHOW_SkeletalMeshes			},
		{ TEXT("SkelMeshes"),			SHOW_SkeletalMeshes			},
		{ TEXT("Speedtrees"),			SHOW_SpeedTrees				},
		{ TEXT("Splines"),				SHOW_Splines				},
		{ TEXT("Sprites"),				SHOW_Sprites				},
		{ TEXT("StaticMeshes"),			SHOW_StaticMeshes			},
		{ TEXT("InstancedStaticMeshes"),SHOW_InstancedStaticMeshes	},
		{ TEXT("Terrain"),				SHOW_Terrain				},
		{ TEXT("TerrainPatches"),		SHOW_TerrainPatches			},
		{ TEXT("Unlittranslucency"),	SHOW_UnlitTranslucency		},
		{ TEXT("TranslucencyDOF"),		SHOW_TranslucencyDoF		},
		{ TEXT("ZeroExtent"),			SHOW_CollisionZeroExtent	},
		{ TEXT("Volumes"),				SHOW_Volumes				},
		{ TEXT("MotionBlur"),			SHOW_MotionBlur				},
		{ TEXT("ImageGrain"),			SHOW_ImageGrain				},
		{ TEXT("DepthOfField"),			SHOW_DepthOfField			},
		{ TEXT("ImageReflections"),		SHOW_ImageReflections		},
		{ TEXT("SubsurfaceScattering"),	SHOW_SubsurfaceScattering	},
		{ TEXT("LightFunctions"),		SHOW_LightFunctions			},
		{ TEXT("Tessellation"),			SHOW_Tessellation			},
		{ TEXT("VisualizeDOFLayers"),	SHOW_VisualizeDOFLayers		},
		{ TEXT("PreShadowCasters"),		SHOW_PreShadowCasters		},
		{ TEXT("PreShadowFrustums"),	SHOW_PreShadowFrustums		},
		{ TEXT("SSAO"),					SHOW_SSAO					},
		{ TEXT("VisualizeSSAO"),		SHOW_VisualizeSSAO			},
		{ TEXT("LightShafts"),			SHOW_LightShafts			},
		{ TEXT("TemporalAA"),			SHOW_TemporalAA				},
		{ TEXT("PostProcessAA"),		SHOW_PostProcessAA			},
	};

	// First, look for skeletal mesh show commands

	UBOOL bUpdateSkelMeshCompDebugFlags = FALSE;
	static UBOOL bShowSkelBones = FALSE;
	static UBOOL bShowPrePhysSkelBones = FALSE;
	UBOOL bSetFlag = (SetMode == SetMode_Enable) ? TRUE : FALSE;

	if(ParseCommand(&Cmd,TEXT("BONES")))
	{
		bShowSkelBones = (SetMode == SetMode_Toggle) ? !bShowSkelBones : bSetFlag;
		bUpdateSkelMeshCompDebugFlags = TRUE;
	}
	else if(ParseCommand(&Cmd,TEXT("PREPHYSBONES")))
	{
		bShowPrePhysSkelBones = (SetMode == SetMode_Toggle) ? !bShowPrePhysSkelBones : bSetFlag;
		bUpdateSkelMeshCompDebugFlags = TRUE;
	}

	// If we changed one of the skel mesh debug show flags, set it on each of the components in the GWorld.
	if(bUpdateSkelMeshCompDebugFlags)
	{
		for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			USkeletalMeshComponent* SkelComp = *It;
			if( SkelComp->GetScene() == GWorld->Scene )
			{
				SkelComp->bDisplayBones = bShowSkelBones;
				SkelComp->bShowPrePhysBones = bShowPrePhysSkelBones;
				SkelComp->BeginDeferredReattach();
			}
		}

		// Now we are done.
		return TRUE;
	}

	// Search for a specific show flag and toggle it if found.
	for(UINT FlagIndex = 0;FlagIndex < ARRAY_COUNT(Flags);FlagIndex++)
	{
		if(ParseCommand(&Cmd,Flags[FlagIndex].Name))
		{
			// Don't let the user toggle editoronly showflags.
			const UBOOL bCanBeToggled = GIsEditor || !(Flags[FlagIndex].Flag & SHOW_EditorOnly_Mask);
			if ( !bCanBeToggled )
			{
				continue;
			}
			if ( Flags[FlagIndex].Flag == SHOW_RESERVED_FLAG || Flags[FlagIndex].Flag == 0 )
			{
				Ar.Logf(TEXT("%s cannot be modified!"), Flags[FlagIndex].Name);
				continue;
			}

			FString ExplicitValue = ParseToken(Cmd,0);
			if ( !ExplicitValue.IsEmpty() )
			{
				UBOOL bEnable = ExplicitValue.ToUBOOL();
				if ( bEnable )
				{
					ShowFlags |= Flags[FlagIndex].Flag;
				}
				else
				{
					ShowFlags &= ~Flags[FlagIndex].Flag;
				}
			}
			else if ( SetMode == SetMode_Toggle )
			{
				ShowFlags ^= Flags[FlagIndex].Flag;
			}
			else
			{
				ShowFlags &= ~Flags[FlagIndex].Flag;
				ShowFlags |= (SetMode == SetMode_Enable) ? Flags[FlagIndex].Flag : 0;
			}

			// special case: for the SHOW_Collision flag, we need to un-hide any primitive components that collide so their collision geometry gets rendered
			if (Flags[FlagIndex].Flag == SHOW_Collision ||
				Flags[FlagIndex].Flag == SHOW_CollisionNonZeroExtent || 
				Flags[FlagIndex].Flag == SHOW_CollisionZeroExtent || 
				Flags[FlagIndex].Flag == SHOW_CollisionRigidBody )
			{
				// Ensure that all flags, other than the one we just typed in, is off.
				ShowFlags &= ~(Flags[FlagIndex].Flag ^ (SHOW_Collision_Any | SHOW_Collision));

				for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
				{
					UPrimitiveComponent* PrimitiveComponent = *It;
					if( PrimitiveComponent->HiddenGame && PrimitiveComponent->ShouldCollide() && PrimitiveComponent->GetScene() == GWorld->Scene )
					{
						check( !GIsEditor || ( (PrimitiveComponent->GetOutermost()->PackageFlags & PKG_PlayInEditor) || (!GIsPlayInEditorWorld) ) );
						PrimitiveComponent->SetHiddenGame(false);
					}
				}
			}

#if !FINAL_RELEASE
			if( ShowFlags & SHOW_Collision )
			{
				for (FLocalPlayerIterator It((UEngine*)GetOuter()); It; ++It)
				{
					APlayerController* PC = It->Actor;
					if( PC != NULL && PC->Pawn != NULL )
					{
						PC->eventClientMessage( FString::Printf(TEXT("!!!! Player Pawn %s Collision Info !!!!"), *PC->Pawn->GetName()) );
						PC->eventClientMessage( FString::Printf(TEXT("Base %s"), *PC->Pawn->Base->GetName() ));
						for( INT i = 0; i < PC->Pawn->Touching.Num(); i++ )
						{
							PC->eventClientMessage( FString::Printf(TEXT("Touching %d: %s"), i, *PC->Pawn->Touching(i)->GetName() ));
						}
					}
				}
			}
#endif
			else if (Flags[FlagIndex].Flag == SHOW_Paths || (GIsGame && Flags[FlagIndex].Flag == SHOW_Cover))
			{
				UBOOL bShowPaths = (ShowFlags & SHOW_Paths) != 0;
				UBOOL bShowCover = (ShowFlags & SHOW_Cover) != 0;
				// make sure all nav points have path rendering components
				for (FActorIterator It; It; ++It)
				{
					ACoverLink *Link = Cast<ACoverLink>(*It);
					if (Link != NULL)
					{
						UBOOL bHasComponent = FALSE;
						for (INT Idx = 0; Idx < Link->Components.Num(); Idx++)
						{
							UCoverMeshComponent *PathRenderer = Cast<UCoverMeshComponent>(Link->Components(Idx));
							if (PathRenderer != NULL)
							{
								PathRenderer->SetHiddenGame(!(bShowPaths || bShowCover));
								bHasComponent = TRUE;
								break;
							}
						}
						if (!bHasComponent)
						{
							UClass *MeshCompClass = FindObject<UClass>(ANY_PACKAGE,*GEngine->DynamicCoverMeshComponentName);
							if (MeshCompClass == NULL)
							{
								MeshCompClass = UCoverMeshComponent::StaticClass();
							}
							UCoverMeshComponent *PathRenderer = ConstructObject<UCoverMeshComponent>(MeshCompClass,Link);
							PathRenderer->SetHiddenGame(!(bShowPaths || bShowCover));
							Link->AttachComponent(PathRenderer);
						}
					}
					else
					{
						ANavigationPoint *Nav = Cast<ANavigationPoint>(*It);
						if (Nav != NULL)
						{
							Nav->TogglePathRendering(bShowPaths);
						}
						else
						{
							APathTargetPoint* Targ = Cast<APathTargetPoint>(*It);
							if(Targ!=NULL)
							{
								UBOOL bHasComponent = FALSE;
								for (INT Idx = 0; Idx < Targ->Components.Num(); Idx++)
								{
									UDrawBoxComponent *BoxRenderer = Cast<UDrawBoxComponent>(Targ->Components(Idx));
									if (BoxRenderer != NULL)
									{
										bHasComponent = TRUE;
										BoxRenderer->SetHiddenGame(!bShowPaths);
										break;
									}
								}
								if (!bHasComponent)
								{
									UDrawBoxComponent *BoxRenderer = ConstructObject<UDrawBoxComponent>(UDrawBoxComponent::StaticClass(),Targ);
									BoxRenderer->SetHiddenGame(!bShowPaths);
									BoxRenderer->BoxColor=FColor(255,0,0,255);
									BoxRenderer->BoxExtent=FVector(10.f);
									BoxRenderer->bDrawWireBox=TRUE;
									Targ->AttachComponent(BoxRenderer);
								}
							}
						}
					}
				}
			}
			else if( Flags[FlagIndex].Flag == SHOW_Volumes )
			{
				if (AllowDebugViewmodes())
				{
					// Iterate over all brushes
					for( TObjectIterator<UBrushComponent> It; It; ++It )
					{
						UBrushComponent* BrushComponent = *It;
						AVolume* Owner = Cast<AVolume>( BrushComponent->GetOwner() );

						// Only bother with volume brushes that belong to the world's scene
						if( Owner && BrushComponent->GetScene() == GWorld->Scene )
						{
							// We're expecting this to be in the game at this point
							check( !GIsEditor || ( (BrushComponent->GetOutermost()->PackageFlags & PKG_PlayInEditor) || (!GIsPlayInEditorWorld) ) );

							// Toggle visibility of this volume
							if( BrushComponent->HiddenGame && (SetMode == SetMode_Toggle || SetMode == SetMode_Enable))
							{
								Owner->bHidden = false;
								BrushComponent->SetHiddenGame( false );
							}
							else if( !BrushComponent->HiddenGame && (SetMode == SetMode_Toggle || SetMode == SetMode_Disable))
							{
								Owner->bHidden = true;
								BrushComponent->SetHiddenGame( true );
							}
						}
					}
				}
				else
				{
					Ar.Logf(TEXT("Debug viewmodes not allowed on consoles by default.  See AllowDebugViewmodes()."));
				}
			}

			return TRUE;
		}
	}

	// The specified flag wasn't found -- list all flags and their current value.
	for(UINT FlagIndex = 0;FlagIndex < ARRAY_COUNT(Flags);FlagIndex++)
	{
		Ar.Logf(TEXT("%s : %s"),
			(ShowFlags & Flags[FlagIndex].Flag) ? TEXT(" TRUE") :TEXT("FALSE"),
			Flags[FlagIndex].Name);
	}

	return FALSE;
}

UBOOL UGameViewportClient::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	if ( ParseCommand(&Cmd,TEXT("FORCEFULLSCREEN")) )
	{
		GForceFullscreen = !GForceFullscreen;
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOW")) )
	{
#if (SHIPPING_PC_GAME || FINAL_RELEASE) && !FINAL_RELEASE_DEBUGCONSOLE
		// don't allow show flags in net games, but on con
		if ( GWorld->GetNetMode() != NM_Standalone || (Cast<UGameEngine>(GEngine) && Cast<UGameEngine>(GEngine)->GPendingLevel != NULL) )
		{
			return TRUE;
		}
		// the effects of this cannot be easily reversed, so prevent the user from playing network games without restarting to avoid potential exploits
		GDisallowNetworkTravel = TRUE;
#endif
		SetShowFlags( Cmd, Ar, SetMode_Toggle );
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWSET")) )
	{
		SetShowFlags( Cmd, Ar, SetMode_Enable );
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWCLEAR")) )
	{
		SetShowFlags( Cmd, Ar, SetMode_Disable );
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("VIEWMODE")))
	{
#ifndef _DEBUG
		// If there isn't a cheat manager, exit out
		UBOOL bCheatsEnabled = FALSE;
		for (FLocalPlayerIterator It((UEngine*)GetOuter()); It; ++It)
		{
			if (It->Actor != NULL && It->Actor->CheatManager != NULL)
			{
				bCheatsEnabled = TRUE;
				break;
			}
		}
		if (!bCheatsEnabled)
		{
			return TRUE;
		}
#endif

		UBOOL bDebugViewmode = TRUE;
		if( ParseCommand(&Cmd,TEXT("WIREFRAME")) )
		{
			ShowFlags &= ~SHOW_ViewMode_Mask;
			ShowFlags |= SHOW_ViewMode_Wireframe;
		}
		else if( ParseCommand(&Cmd,TEXT("BRUSHWIREFRAME")) )
		{
			ShowFlags &= ~SHOW_ViewMode_Mask;
			ShowFlags |= SHOW_ViewMode_BrushWireframe;
		}
		else if( ParseCommand(&Cmd,TEXT("UNLIT")) )
		{
#if CONSOLE
			Ar.Logf(TEXT("Unlit viewmode not currently supported on consoles."));
#else
			bOverrideDiffuseAndSpecular = FALSE;
			ShowFlags &= ~SHOW_ViewMode_Mask;
			ShowFlags |= SHOW_ViewMode_Unlit;
#endif
		}
		else if( ParseCommand(&Cmd,TEXT("LIGHTINGONLY")) )
		{
			ShowFlags &= ~SHOW_ViewMode_Mask;
			ShowFlags |= SHOW_ViewMode_LightingOnly;
		}			
		else if( ParseCommand(&Cmd,TEXT("LIGHTCOMPLEXITY")) )
		{
			ShowFlags &= ~SHOW_ViewMode_Mask;
			ShowFlags |= SHOW_ViewMode_LightComplexity;
		}
		else if( ParseCommand(&Cmd,TEXT("SHADERCOMPLEXITY")) )
		{
#if CONSOLE
			Ar.Logf(TEXT("Shader complexity not currently supported on consoles.  Use platform tools instead, for example PIX has an overdraw viewmode."));
#else
			ShowFlags &= ~SHOW_ViewMode_Mask;
			ShowFlags |= SHOW_ViewMode_ShaderComplexity;
#endif
		}
		else if( ParseCommand(&Cmd,TEXT("TEXTUREDENSITY")))
		{
			ShowFlags &= ~SHOW_ViewMode_Mask;
			ShowFlags |= SHOW_ViewMode_TextureDensity;
		}
		else if( ParseCommand(&Cmd,TEXT("LIGHTMAPDENSITY")))
		{
			ShowFlags &= ~SHOW_ViewMode_Mask;
			ShowFlags |= SHOW_ViewMode_LightMapDensity;
		}
		else if( ParseCommand(&Cmd,TEXT("LITLIGHTMAPDENSITY")) )
		{
			ShowFlags &= ~SHOW_ViewMode_Mask;
			ShowFlags |= SHOW_ViewMode_LitLightmapDensity;
		}	
		else if( ParseCommand(&Cmd,TEXT("DETAILLIGHTING")) )
		{
#if CONSOLE
			Ar.Logf(TEXT("Detail lighting not currently supported on consoles."));
#else
			bOverrideDiffuseAndSpecular = !bOverrideDiffuseAndSpecular;
			ShowFlags &= ~SHOW_ViewMode_Mask;
			ShowFlags |= SHOW_ViewMode_Lit;
#endif
		}
		else
		{
			bOverrideDiffuseAndSpecular = FALSE;
			bDebugViewmode = FALSE;
			ShowFlags &= ~SHOW_ViewMode_Mask;
			ShowFlags |= SHOW_ViewMode_Lit;
		}

		if (bDebugViewmode && !AllowDebugViewmodes())
		{
			Ar.Logf(TEXT("Debug viewmodes not allowed on consoles by default.  See AllowDebugViewmodes()."));
			ShowFlags &= ~SHOW_ViewMode_Mask;
			ShowFlags |= SHOW_ViewMode_Lit;
		}

		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("NEXTVIEWMODE")))
	{
#ifndef _DEBUG
		// If there isn't a cheat manager, exit out
		UBOOL bCheatsEnabled = FALSE;
		for (FLocalPlayerIterator It((UEngine*)GetOuter()); It; ++It)
		{
			if (It->Actor != NULL && It->Actor->CheatManager != NULL)
			{
				bCheatsEnabled = TRUE;
				break;
			}
		}
		if (!bCheatsEnabled)
		{
			return TRUE;
		}
#endif

		EShowFlags OldShowFlags = ShowFlags;
		ShowFlags &= ~SHOW_ViewMode_Mask;
		
		const EShowFlags OldViewModeMask = OldShowFlags & SHOW_ViewMode_Mask;
		if (OldViewModeMask == SHOW_ViewMode_Lit)
		{
			ShowFlags |= SHOW_ViewMode_LightingOnly;
		}
		else if (OldViewModeMask == SHOW_ViewMode_LightingOnly)
		{
			ShowFlags |= SHOW_ViewMode_LightComplexity;
		}
		else if (OldViewModeMask == SHOW_ViewMode_LightComplexity)
		{
			ShowFlags |= SHOW_ViewMode_Wireframe;
		}
		else if (OldViewModeMask == SHOW_ViewMode_Wireframe)
		{
			ShowFlags |= SHOW_ViewMode_BrushWireframe;
		}
		else if (OldViewModeMask == SHOW_ViewMode_BrushWireframe)
		{
			ShowFlags |= SHOW_ViewMode_Unlit;
		}
		else if (OldViewModeMask == SHOW_ViewMode_Unlit)
		{
			ShowFlags |= SHOW_ViewMode_TextureDensity;
		}
		else if (OldViewModeMask == SHOW_ViewMode_TextureDensity)
		{
			ShowFlags |= SHOW_ViewMode_Lit;
		}
		else
		{
			ShowFlags |= SHOW_ViewMode_Lit;
		}

		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PREVVIEWMODE")))
	{
#ifndef _DEBUG
		// If there isn't a cheat manager, exit out
		UBOOL bCheatsEnabled = FALSE;
		for (FLocalPlayerIterator It((UEngine*)GetOuter()); It; ++It)
		{
			if (It->Actor != NULL && It->Actor->CheatManager != NULL)
			{
				bCheatsEnabled = TRUE;
				break;
			}
		}
		if (!bCheatsEnabled)
		{
			return TRUE;
		}
#endif

		EShowFlags OldShowFlags = ShowFlags;
		ShowFlags &= ~SHOW_ViewMode_Mask;

		const EShowFlags OldViewModeMask = OldShowFlags & SHOW_ViewMode_Mask;
		if (OldViewModeMask == SHOW_ViewMode_Lit)
		{
			ShowFlags |= SHOW_ViewMode_TextureDensity;
		}
		else if (OldViewModeMask == SHOW_ViewMode_LightingOnly)
		{
			ShowFlags |= SHOW_ViewMode_Lit;
		}
		else if (OldViewModeMask == SHOW_ViewMode_LightComplexity)
		{
			ShowFlags |= SHOW_ViewMode_LightingOnly;
		}
		else if (OldViewModeMask == SHOW_ViewMode_Wireframe)
		{
			ShowFlags |= SHOW_ViewMode_LightComplexity;
		}
		else if (OldViewModeMask == SHOW_ViewMode_BrushWireframe)
		{
			ShowFlags |= SHOW_ViewMode_Wireframe;
		}
		else if (OldViewModeMask == SHOW_ViewMode_Unlit)
		{
			ShowFlags |= SHOW_ViewMode_BrushWireframe;
		}
		else if (OldViewModeMask == SHOW_ViewMode_TextureDensity)
		{
			ShowFlags |= SHOW_ViewMode_Unlit;
		}
		else
		{
			ShowFlags |= SHOW_ViewMode_Lit;
		}

		return TRUE;
	}
#if WITH_EDITOR
	else if( ParseCommand( &Cmd, TEXT("ShowMouseCursor") ) )
	{
		if( Viewport )
		{
		
			if( !bShowSystemMouseCursor )
			{
				// Toggle the cursor on and lock the mouse to the viewport
				Viewport->LockMouseToWindow( FALSE );
				Viewport->CaptureMouse( FALSE );
				bShowSystemMouseCursor = TRUE;
			}
			else
			{
				// Toggle the cursor off and unlock the mouse.
				Viewport->LockMouseToWindow( TRUE );
				Viewport->CaptureMouse( TRUE );
				bShowSystemMouseCursor = FALSE;
			}
			Viewport->UpdateMouseCursor(TRUE);
		}
		return TRUE;

	}
#endif
	else if( ParseCommand(&Cmd,TEXT("PRECACHE")) )
	{
		Precache();
		return TRUE;
	}
#if WITH_REALD
	else if( RealD::Exec(Cmd, Ar) )
	{
		return TRUE;
	}
#endif
	else if( ParseCommand(&Cmd,TEXT("SETRES")) )
	{
		if(Viewport && ViewportFrame)
		{
			INT X=appAtoi(Cmd);
			const TCHAR* CmdTemp = appStrchr(Cmd,'x') ? appStrchr(Cmd,'x')+1 : appStrchr(Cmd,'X') ? appStrchr(Cmd,'X')+1 : TEXT("");
			INT Y=appAtoi(CmdTemp);
			Cmd = CmdTemp;
			UBOOL	Fullscreen = Viewport->IsFullscreen();
			if(appStrchr(Cmd,'w') || appStrchr(Cmd,'W'))
				Fullscreen = 0;
			else if(appStrchr(Cmd,'f') || appStrchr(Cmd,'F'))
				Fullscreen = 1;
			if( X && Y )
			{
				ViewportFrame->Resize(X,Y,Fullscreen);
			}
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("TILEDSHOT")) )
	{
		FlushRenderingCommands();
		GIsTiledScreenshot = TRUE;
		GScreenMessagesRestoreState = GAreScreenMessagesEnabled;
		GAreScreenMessagesEnabled = FALSE;
		GScreenshotResolutionMultiplier = appAtoi(Cmd);
		GScreenshotResolutionMultiplier = Clamp<INT>( GScreenshotResolutionMultiplier, 2, 128 );
		const TCHAR* CmdTemp = appStrchr(Cmd, ' ');
		GScreenshotMargin = CmdTemp ? Clamp<INT>(appAtoi(CmdTemp), 0, 320) : 64;
		return TRUE;
	}	
	else if( ParseCommand(&Cmd,TEXT("HighResShot")) )
	{
		if(Viewport)
		{
			GIsHighResScreenshot = TRUE;
			GScreenshotResolutionMultiplier = 2;
			if(*Cmd)
			{
				GScreenshotResolutionMultiplier = appAtoi(Cmd);
				GScreenshotResolutionMultiplier = Clamp<INT>( GScreenshotResolutionMultiplier, 2, 20 );
			}
			GScreenMessagesRestoreState = GAreScreenMessagesEnabled;
			GAreScreenMessagesEnabled = FALSE;
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOT")) || ParseCommand(&Cmd,TEXT("SCREENSHOT")) )
	{
		if(Viewport)
		{
			GScreenShotRequest=TRUE;
			GScreenMessagesRestoreState = GAreScreenMessagesEnabled;
			GAreScreenMessagesEnabled = FALSE;
			Parse(Cmd, TEXT("NAME="), GScreenShotName);
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("LOGOUTSTATLEVELS")) )
	{
		TMap<FName,INT> StreamingLevels;	
		FString LevelPlayerIsInName;
		GetLevelStremingStatus( StreamingLevels, LevelPlayerIsInName );

		Ar.Logf( TEXT( "Level Streaming:" ) );

		// now draw the "map" name
		FString MapName	= GWorld->CurrentLevel->GetOutermost()->GetName();

		if( LevelPlayerIsInName == MapName )
		{
			MapName = *FString::Printf( TEXT("->  %s"), *MapName );
		}
		else
		{
			MapName = *FString::Printf( TEXT("    %s"), *MapName );
		}

		Ar.Logf( TEXT( "%s" ), *MapName );

		// now log the levels
		for( TMap<FName,INT>::TIterator It(StreamingLevels); It; ++It )
		{
			FString LevelName = It.Key().ToString();
			const INT Status = It.Value();
			FString StatusName;

			switch( Status )
			{
			case LEVEL_Visible:
				StatusName = TEXT( "red loaded and visible" );
				break;
			case LEVEL_MakingVisible:
				StatusName = TEXT( "orange, in process of being made visible" );
				break;
			case LEVEL_Loaded:
				StatusName = TEXT( "yellow loaded but not visible" );
				break;
			case LEVEL_UnloadedButStillAround:
				StatusName = TEXT( "blue  (GC needs to occur to remove this)" );
				break;
			case LEVEL_Unloaded:
				StatusName = TEXT( "green Unloaded" );
				break;
			case LEVEL_Preloading:
				StatusName = TEXT( "purple (preloading)" );
				break;
			default:
				break;
			};


			UPackage* LevelPackage = FindObject<UPackage>( NULL, *LevelName );

			if( LevelPackage 
				&& (LevelPackage->GetLoadTime() > 0) 
				&& (Status != LEVEL_Unloaded) )
			{
				LevelName += FString::Printf(TEXT(" - %4.1f sec"), LevelPackage->GetLoadTime());
			}
			else if( UObject::GetAsyncLoadPercentage( *LevelName ) >= 0 )
			{
				const INT Percentage = appTrunc( UObject::GetAsyncLoadPercentage( *LevelName ) );
				LevelName += FString::Printf(TEXT(" - %3i %%"), Percentage ); 
			}

			if( LevelPlayerIsInName == LevelName )
			{
				LevelName = *FString::Printf( TEXT("->  %s"), *LevelName );
			}
			else
			{
				LevelName = *FString::Printf( TEXT("    %s"), *LevelName );
			}

			LevelName = FString::Printf( TEXT("%s \t\t%s"), *LevelName, *StatusName );

			Ar.Logf( TEXT( "%s" ), *LevelName );

		}

		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("BUGSCREENSHOT")) )
	{
		// find where these are really defined
#if PS3
		const static INT MaxFilenameLen = 42;
#elif XBOX
		const static INT MaxFilenameLen = 42;
#else
		const static INT MaxFilenameLen = 100;
#endif

		if( Viewport != NULL )
		{
			TCHAR File[MAX_SPRINTF] = TEXT("");
			for( INT TestBitmapIndex = 0; TestBitmapIndex < 9; ++TestBitmapIndex )
			{ 
				const FString DescPlusExtension = FString::Printf( TEXT("%s%i.bmp"), Cmd, TestBitmapIndex );
				const FString SSFilename = CreateProfileFilename( DescPlusExtension, FALSE );

				const FString OutputDir = appScreenShotDir();

				//warnf( TEXT( "BugIt Looking: %s" ), *(OutputDir + SSFilename) );
				appSprintf( File, TEXT("%s"), *(OutputDir + SSFilename) );
				if( GFileManager->FileSize(File) == INDEX_NONE )
				{
					GScreenshotBitmapIndex = TestBitmapIndex; // this is safe as the UnMisc.cpp ScreenShot code will test each number before writing a file
					GScreenShotName = SSFilename; 
					GScreenShotRequest = TRUE;
					break;
				}
			}
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("KILLPARTICLES")) )
	{
		// Don't kill in the Editor to avoid potential content clobbering.
		if( !GIsEditor )
		{
			extern UBOOL GIsAllowingParticles;
			// Deactivate system and kill existing particles.
			for( TObjectIterator<UParticleSystemComponent> It; It; ++It )
			{
				UParticleSystemComponent* ParticleSystemComponent = *It;
				ParticleSystemComponent->DeactivateSystem();
				ParticleSystemComponent->KillParticlesForced();
			}
			// No longer initialize particles from here on out.
			GIsAllowingParticles = FALSE;
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("FORCESKELLOD")) )
	{
		INT ForceLod = 0;
		if(Parse(Cmd,TEXT("LOD="),ForceLod))
		{
			ForceLod++;
		}

		for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			USkeletalMeshComponent* SkelComp = *It;
			if( SkelComp->GetScene() == GWorld->Scene && !SkelComp->IsTemplate())
			{
				SkelComp->ForcedLodModel = ForceLod;
			}
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("TOGGLEUI")) )
	{
		GTickAndRenderUI = !GTickAndRenderUI;
		return TRUE;
	}
#if WITH_GFx
	else if( ParseCommand(&Cmd,TEXT("TOGGLEGFXUI")) )
	{
		GRenderScaleform = !GRenderScaleform;
		return TRUE;
	}
#endif // WITH_GFX
	else if (ParseCommand(&Cmd, TEXT("DISPLAY")))
	{
		TCHAR ObjectName[256];
		TCHAR PropStr[256];
		if ( ParseToken(Cmd, ObjectName, ARRAY_COUNT(ObjectName), TRUE) &&
			ParseToken(Cmd, PropStr, ARRAY_COUNT(PropStr), TRUE) )
		{
			UObject* Obj = FindObject<UObject>(ANY_PACKAGE, ObjectName);
			if (Obj != NULL)
			{
				FName PropertyName(PropStr, FNAME_Find);
				if (PropertyName != NAME_None && FindField<UProperty>(Obj->GetClass(), PropertyName) != NULL)
				{
					FDebugDisplayProperty& NewProp = DebugProperties(DebugProperties.AddZeroed());
					NewProp.Obj = Obj;
					NewProp.PropertyName = PropertyName;
				}
				else
				{
					Ar.Logf(TEXT("Property '%s' not found on object '%s'"), PropStr, *Obj->GetName());
				}
			}
			else
			{
				Ar.Logf(TEXT("Object not found"));
			}
		}

		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("DISPLAYALL")))
	{
		TCHAR ClassName[256];
		TCHAR PropStr[256];
		if ( ParseToken(Cmd, ClassName, ARRAY_COUNT(ClassName), TRUE) &&
			ParseToken(Cmd, PropStr, ARRAY_COUNT(PropStr), TRUE) )
		{
			UClass* Cls = FindObject<UClass>(ANY_PACKAGE, ClassName);
			if (Cls != NULL)
			{
				FName PropertyName(PropStr, FNAME_Find);
				if (PropertyName != NAME_None && FindField<UProperty>(Cls, PropertyName) != NULL)
				{
					// add all un-GCable things immediately as that list is static
					// so then we only have to iterate over dynamic things each frame
					for (TObjectIterator<UObject> It; It; ++It)
					{
						if (!It->HasAnyFlags(RF_DisregardForGC))
						{
							break;
						}
						else if (It->IsA(Cls) && !It->IsTemplate())
						{
							FDebugDisplayProperty& NewProp = DebugProperties(DebugProperties.AddZeroed());
							NewProp.Obj = *It;
							NewProp.PropertyName = PropertyName;
						}
					}
					FDebugDisplayProperty& NewProp = DebugProperties(DebugProperties.AddZeroed());
					NewProp.Obj = Cls;
					NewProp.PropertyName = PropertyName;
				}
				else
				{
					Ar.Logf(TEXT("Property '%s' not found on object '%s'"), PropStr, *Cls->GetName());
				}
			}
			else
			{
				Ar.Logf(TEXT("Object not found"));
			}
		}

		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("DISPLAYALLSTATE")))
	{
		TCHAR ClassName[256];
		if (ParseToken(Cmd, ClassName, ARRAY_COUNT(ClassName), TRUE))
		{
			UClass* Cls = FindObject<UClass>(ANY_PACKAGE, ClassName);
			if (Cls != NULL)
			{
				// add all un-GCable things immediately as that list is static
				// so then we only have to iterate over dynamic things each frame
				for (TObjectIterator<UObject> It; It; ++It)
				{
					if (!It->HasAnyFlags(RF_DisregardForGC))
					{
						break;
					}
					else if (It->IsA(Cls))
					{
						FDebugDisplayProperty& NewProp = DebugProperties(DebugProperties.AddZeroed());
						NewProp.Obj = *It;
						NewProp.PropertyName = NAME_State;
						NewProp.bSpecialProperty = TRUE;
					}
				}
				FDebugDisplayProperty& NewProp = DebugProperties(DebugProperties.AddZeroed());
				NewProp.Obj = Cls;
				NewProp.PropertyName = NAME_State;
				NewProp.bSpecialProperty = TRUE;
			}
			else
			{
				Ar.Logf(TEXT("Object not found"));
			}
		}

		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("DISPLAYCLEAR")))
	{
		DebugProperties.Empty();

		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("TOGGLEFLUIDS")))
	{
		extern UBOOL GForceFluidDeactivation;
		GForceFluidDeactivation = !GForceFluidDeactivation;
		Ar.Logf(TEXT("Forcing deactivation of all fluids: %s"), GForceFluidDeactivation ? TEXT("ON") : TEXT("OFF"));
		return TRUE;
	}
	else if(ParseCommand(&Cmd, TEXT("TEXTUREDEFRAG")))
	{
		extern void appDefragmentTexturePool();
		appDefragmentTexturePool();
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("TOGGLEMIPFADE")))
	{
		GEnableMipLevelFading = (GEnableMipLevelFading >= 0.0f) ? -1.0f : 1.0f;
		Ar.Logf(TEXT("Mip-fading is now: %s"), (GEnableMipLevelFading >= 0.0f) ? TEXT("ENABLED") : TEXT("DISABLED"));
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PAUSERENDERCLOCK")))
	{
		GPauseRenderingRealtimeClock = !GPauseRenderingRealtimeClock;
		Ar.Logf(TEXT("The global realtime rendering clock is now: %s"), GPauseRenderingRealtimeClock ? TEXT("PAUSED") : TEXT("RUNNING"));
		return TRUE;
	}
#if WITH_GFx
	else if (ScaleformInteraction && ScaleformInteraction->Exec(Cmd, Ar))
	{
		return TRUE;
	}
	else if( ParseCommand(&Cmd, TEXT("RECORDINPUT")) )
	{
		if(!GIsBenchmarking)
		{
			Ar.Log(TEXT("Input recording is only supported with -benchmark"));
		}
		else
		{
			if ( ParseCommand(&Cmd, TEXT("START")) )
			{
				return 1; 
			}
			else if ( ParseCommand(&Cmd, TEXT("STOP")) )
			{
				return 1;
			}
			else if (ParseCommand(&Cmd, TEXT("PLAYUNTILDONE")) )
			{
				return 1;
			}
			else if (ParseCommand(&Cmd, TEXT("PLAY")) )
			{
				return 1;
			}
		}
		return 0;
	}
	else if (ParseCommand(&Cmd, TEXT("RESETGFXVIEWPORT")))
	{
		SetViewport(ViewportFrame->GetViewport());
		return TRUE;
	}
#endif
	else if( ParseCommand(&Cmd,TEXT("KISMETLOG")) )
	{
		TArray<USequence*> AllSequences;

		GEngine->bEnableKismetLogging = TRUE;

		AllSequences = GEngine->GetCurrentWorldInfo()->GetAllRootSequences();
		for( INT SequenceIdx = 0; SequenceIdx < AllSequences.Num(); ++SequenceIdx )
		{
			AllSequences(SequenceIdx)->CreateKismetLog();
		}
		return TRUE;
	}
	else if ( UIController->Exec(Cmd,Ar) )
	{
		return TRUE;
	}
	else if(ScriptConsoleExec(Cmd,Ar,NULL))
	{
		return TRUE;
	}
	else if( GEngine->Exec(Cmd,Ar) )
	{
		return TRUE;
	}
	else if( GColorList.Exec(Cmd,Ar) )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


UBOOL UPlayer::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	if(Actor)
	{
		// Since UGameViewportClient calls Exec on UWorld, we only need to explicitly
		// call UWorld::Exec if we either have a null GEngine or a null ViewportClient
		UBOOL bWorldNeedsExec = GEngine == NULL || Cast<ULocalPlayer>(this) == NULL || static_cast<ULocalPlayer*>(this)->ViewportClient == NULL;
		if( bWorldNeedsExec && GWorld->Exec(Cmd,Ar) )
		{
			return TRUE;
		}
		else if( Actor->PlayerInput && Actor->PlayerInput->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
		{
			return TRUE;
		}
		else if( Actor->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
		{
			return TRUE;
		}
		else if( Actor->Pawn )
		{
			if( Actor->Pawn->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
			{
				return TRUE;
			}
			else if( Actor->Pawn->InvManager && Actor->Pawn->InvManager->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
			{
				return TRUE;
			}
			else if( Actor->Pawn->Weapon && Actor->Pawn->Weapon->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
			{
				return TRUE;
			}
		}
		if( Actor->myHUD && Actor->myHUD->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
		{
			return TRUE;
		}
		else if( GWorld->GetGameInfo() && GWorld->GetGameInfo()->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
		{
			return TRUE;
		}
		else if( Actor->CheatManager && Actor->CheatManager->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
		{
			return TRUE;
		}
		else
		{
			// allow Interactions to have exec functions
			for(INT InteractionIndex = 0;InteractionIndex < Actor->Interactions.Num();InteractionIndex++)
			{
				if (Actor->Interactions(InteractionIndex))
				{
					if (Actor->Interactions(InteractionIndex)->ScriptConsoleExec(Cmd, Ar, Actor->Pawn))
					{
						return TRUE;
					}
				}
			}
			return FALSE;
		}
	}
	return FALSE;
}


/**
 * Dynamically assign Controller to Player and set viewport.
 *
 * @param    PC - new player controller to assign to player
 **/
void UPlayer::SwitchController(class APlayerController* PC)
{
	// Detach old player.
	if( this->Actor )
	{
		this->Actor->Player = NULL;
	}

	// Set the viewport.
	PC->Player = this;
	this->Actor = PC;
}

ULocalPlayer::ULocalPlayer()
{
	if ( !IsTemplate() )
	{
		ViewState = AllocateViewState();
#if WITH_REALD
		ViewState2 = AllocateViewState();
#endif

		if( !PlayerPostProcess )
		{
			// initialize to global post process if one is not set
			if (InsertPostProcessingChain(GEngine->GetWorldPostProcessChain(), 0, TRUE) == FALSE)
			{
				warnf(TEXT("LocalPlayer %d - Failed to setup default post process..."), ControllerId);
			}
		}

		// Initialize the actor visibility history.
		ActorVisibilityHistory.Init();

		// Create a TranslationContext for this player.
		if (TagContext == NULL)
		{
			TagContext = Cast<UTranslationContext>( UTranslationContext::StaticConstructObject(UTranslationContext::StaticClass(), this) );
		}
	}

	bOverrideView = FALSE;
}

/**
 * Tick tasks required for auth code
 *
 * @param DelaTime	The time passed since the last tick
 */
void ULocalPlayer::Tick(FLOAT DeltaTime)
{
	// NOTE: This function is only called from UGameEngine, if bPendingServerAuth is True

	if (bPendingServerAuth && ServerAuthTimestamp > 0.0)
	{
		AWorldInfo* WI = (GWorld != NULL ? GWorld->GetWorldInfo() : NULL);

		if (WI != NULL)
		{
			if ((WI->RealTimeSeconds - ServerAuthTimestamp) > ServerAuthTimeout)
			{
				ServerAuthTimestamp = 0.0;
				eventServerAuthTimedOut();
			}
			// Reset the timestamp if the level has changed
			else if (ServerAuthTimestamp > WI->RealTimeSeconds)
			{
				ServerAuthTimestamp = WI->RealTimeSeconds;
			}
		}
	}
}

// DEPRECATED
UBOOL ULocalPlayer::GetActorVisibility(AActor* TestActor) const
{
	return TestActor->WorldInfo->TimeSeconds - TestActor->LastRenderTime < 0.1f;
}

UBOOL ULocalPlayer::SpawnPlayActor(const FString& URL,FString& OutError)
{
	if ( GWorld->IsServer() )
	{
		FURL PlayerURL(NULL, *URL, TRAVEL_Absolute);
		FString PlayerName = eventGetNickname();
		if (PlayerName.Len() > 0)
		{
			PlayerURL.AddOption(*FString::Printf(TEXT("Name=%s"), *PlayerName));
		}
		Actor = GWorld->SpawnPlayActor(this, ROLE_SimulatedProxy, PlayerURL, eventGetUniqueNetId(), OutError, GEngine->GamePlayers.FindItemIndex(this));
	}
	else
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		// Statically bind to the specified player controller
		UClass* PCClass = GameEngine != NULL ?
			LoadClass<APlayerController>(NULL, *GameEngine->PendingLevelPlayerControllerClassName, NULL, LOAD_None, NULL) :
			NULL;
		if (PCClass == NULL)
		{
			// This failed to load so use the engine one as default
			PCClass = APlayerController::StaticClass();
			debugf(TEXT("PlayerController class for the pending level is %s"),*PCClass->GetFName().ToString());
		}
		// The PlayerController gets replicated from the client though the engine assumes that every Player always has
		// a valid PlayerController so we spawn a dummy one that is going to be replaced later.
		Actor = CastChecked<APlayerController>(GWorld->SpawnActor(PCClass));
		const INT PlayerIndex=GEngine->GamePlayers.FindItemIndex(this);
		Actor->NetPlayerIndex = PlayerIndex;
	}
	// Add the spawned player as an observer
	AddObserver();
	return Actor != NULL;
}

void ULocalPlayer::SendSplitJoin()
{
	if (GWorld == NULL || GWorld->GetNetDriver() == NULL || GWorld->GetNetDriver()->ServerConnection == NULL || GWorld->GetNetDriver()->ServerConnection->State != USOCK_Open)
	{
		debugf(NAME_Warning, TEXT("SendSplitJoin(): Not connected to a server"));
	}
	else if (!bSentSplitJoin)
	{
		// make sure we don't already have a connection
		UBOOL bNeedToSendJoin = FALSE;
		if (Actor == NULL)
		{
			bNeedToSendJoin = TRUE;
		}
		else if (GWorld->GetNetDriver()->ServerConnection->Actor != Actor)
		{
			UNetDriver* NetDriver = GWorld->GetNetDriver();
			bNeedToSendJoin = TRUE;
			for (INT i = 0; i < NetDriver->ServerConnection->Children.Num(); i++)
			{
				if (NetDriver->ServerConnection->Children(i)->Actor == Actor)
				{
					bNeedToSendJoin = FALSE;
					break;
				}
			}
		}

		if (bNeedToSendJoin)
		{
			FUniqueNetId UniqueId = eventGetUniqueNetId();

			// use the default URL except for player name for splitscreen players
			FURL URL;
			URL.LoadURLConfig(TEXT("DefaultPlayer"), GGameIni);
			FString PlayerName = eventGetNickname();
			if (PlayerName.Len() > 0)
			{
				URL.AddOption(*FString::Printf(TEXT("Name=%s"), *PlayerName));
			}
			FString URLString = URL.String();

			FNetControlMessage<NMT_JoinSplit>::Send(GWorld->GetNetDriver()->ServerConnection, UniqueId, URLString);
			bSentSplitJoin = TRUE;
		}
	}
}

void ULocalPlayer::FinishDestroy()
{
	if ( !IsTemplate() )
	{
		ViewState->Destroy();
		ViewState = NULL;
#if WITH_REALD
		ViewState2->Destroy();
		ViewState2 = NULL;
#endif
	}
	// Remove player from observer list
	RemoveObserver();
	Super::FinishDestroy();
}

/** @return A Markup Context that can be used to translate localized text that may contain dynamic tags */
UTranslationContext* ULocalPlayer::GetTranslationContext()
{
	return this->TagContext;
}

/**
 * Add the given post process chain to the chain at the given index.
 *
 *	@param	InChain		The post process chain to insert.
 *	@param	InIndex		The position to insert the chain in the complete chain.
 *						If -1, insert it at the end of the chain.
 *	@param	bInClone	If TRUE, create a deep copy of the chains effects before insertion.
 */
UBOOL ULocalPlayer::InsertPostProcessingChain(class UPostProcessChain* InChain, INT InIndex, UBOOL bInClone)
{
	if (InChain == NULL)
	{
		return FALSE;
	}

	// Create a new chain...
	UPostProcessChain* ClonedChain = Cast<UPostProcessChain>(StaticDuplicateObject(InChain, InChain, UObject::GetTransientPackage(), TEXT("None"), RF_AllFlags & (~RF_Standalone)));
	if (ClonedChain)
	{
		INT InsertIndex = 0;
		if ((InIndex == -1) || (InIndex >= PlayerPostProcessChains.Num()))
		{
			InsertIndex = PlayerPostProcessChains.Num();
		}
		else
		{
			InsertIndex = InIndex;
		}

		PlayerPostProcessChains.InsertItem(ClonedChain, InsertIndex);

		RebuildPlayerPostProcessChain();

		return TRUE;
	}

	return FALSE;
}

/**
 * Remove the post process chain at the given index.
 *
 *	@param	InIndex		The position to insert the chain in the complete chain.
 */
UBOOL ULocalPlayer::RemovePostProcessingChain(INT InIndex)
{
	if ((InIndex >= 0) && (InIndex < PlayerPostProcessChains.Num()))
	{
		PlayerPostProcessChains.Remove(InIndex);
		RebuildPlayerPostProcessChain();
		return TRUE;
	}

	return FALSE;
}

/**
 * Remove all post process chains.
 *
 *	@return	boolean		TRUE if the chain array was cleared
 *						FALSE if not
 */
UBOOL ULocalPlayer::RemoveAllPostProcessingChains()
{
	PlayerPostProcessChains.Empty();
	RebuildPlayerPostProcessChain();
	return TRUE;
}

/**
 *	Get the PPChain at the given index.
 *
 *	@param	InIndex				The index of the chain to retrieve.
 *
 *	@return	PostProcessChain	The post process chain if found; NULL if not.
 */
class UPostProcessChain* ULocalPlayer::GetPostProcessChain(INT InIndex)
{
	if ((InIndex >= 0) && (InIndex < PlayerPostProcessChains.Num()))
	{
		return PlayerPostProcessChains(InIndex);
	}
	return NULL;
}

/**
 *	Forces the PlayerPostProcess chain to be rebuilt.
 *	This should be called if a PPChain is retrieved using the GetPostProcessChain,
 *	and is modified directly.
 */
void ULocalPlayer::TouchPlayerPostProcessChain()
{
	RebuildPlayerPostProcessChain();
}

/**
 *	Rebuilds the PlayerPostProcessChain.
 *	This should be called whenever the chain array has items inserted/removed.
 */
void ULocalPlayer::RebuildPlayerPostProcessChain()
{
	// Release the current PlayerPostProcessChain.
	if (PlayerPostProcessChains.Num() == 0)
	{
		PlayerPostProcess = NULL;
		return;
	}

	PlayerPostProcess = ConstructObject<UPostProcessChain>(UPostProcessChain::StaticClass(), UObject::GetTransientPackage());
	check(PlayerPostProcess);
	
#if DWTRIOVIZSDK
	UBOOL bDwFoundTriovizNode = FALSE;
#endif

	UBOOL bUberEffectInserted = FALSE;
	for (INT ChainIndex = 0; ChainIndex < PlayerPostProcessChains.Num(); ChainIndex++)
	{
		UPostProcessChain* PPChain = PlayerPostProcessChains(ChainIndex);
		if (PPChain)
		{
			for (INT EffectIndex = 0; EffectIndex < PPChain->Effects.Num(); EffectIndex++)
			{
				UPostProcessEffect* PPEffect = PPChain->Effects(EffectIndex);
				if (PPEffect)
				{
#if DWTRIOVIZSDK
					// track if there was already Trioviz PP effect in the chain
					if (PPEffect->IsA(UDwTriovizImplEffect::StaticClass()) == TRUE)
					{
						bDwFoundTriovizNode = TRUE;
					}
#endif
					if (PPEffect->IsA(UUberPostProcessEffect::StaticClass())== TRUE)
					{
						if (bUberEffectInserted == FALSE)
						{
							PlayerPostProcess->Effects.AddItem(PPEffect);
							bUberEffectInserted = TRUE;
						}
						else
						{
							warnf(TEXT("LocalPlayer %d - Multiple UberPostProcessEffects present..."), ControllerId);
						}
					}
					else
					{
						PlayerPostProcess->Effects.AddItem(PPEffect);
					}
				}
			}
		}
	}

#if DWTRIOVIZSDK
	// if there wasn't already a Trioviz in the chain, we need to add it for the stereoscopic effect to work
	if( bDwFoundTriovizNode == FALSE )
	{
		UDwTriovizImplEffect* Effect = ConstructObject<UDwTriovizImplEffect>(UDwTriovizImplEffect::StaticClass());
		if (Effect)
		{
			PlayerPostProcess->Effects.AddItem(Effect);
		}
	}
#endif
}


/**
 * Updates the post-process settings for the player's view.
 *
 *	There are four separate post process feeds that are combined in order to produce the final applied post process settings.
 *	Level				- The level's default post process values and PostProcessVolumes in the level. This set of post process settings is really a target, and is interpolated in.
 *	Actor(Controller)	- Post process applied by the controller itself, which in most cases is the camera animation's post process values. 
 *						  The way in which this set of settings is applied is determined by the controller, which means it can potentially be destructive to any prior applied settings.
 *	Camera Override		- Post process applied by the camera at a higher priority than the controller post process. Used by matinee.
 *						  These settings are blended in over the prior settings.
 *	Gameplay Override	- Post process settings applied by gameplay code. These post process settings are also an interpolation target.
 *						  Furthermore, these settings are faded out in order to 'recover' from a gameplay override.
 *
 * @param ViewLocation - The player's current view location.
 */
void ULocalPlayer::UpdatePostProcessSettings(const FVector& ViewLocation)
{
	const FLOAT CurrentWorldTime = GWorld->GetRealTimeSeconds();
	
	// Find the post-process settings for the view.
	FPostProcessSettings NewSettings;
	APostProcessVolume *NewVolume;

	//	LEVEL
	NewVolume = GWorld->GetWorldInfo()->GetPostProcessSettings(ViewLocation, TRUE, NewSettings);

	bForceDefaultPostProcessChain = FALSE;
	if (NewVolume && NewVolume->bOverrideWorldPostProcessChain)
	{
		bForceDefaultPostProcessChain = TRUE;
	}
	
	
	FString Map;
	if (Actor)
	{
		Map = Actor->GetURLMap();
	}

	//This is a new map!
	if (Map != LastMap)
	{
		if (bWantToResetToMapDefaultPP)
		{
			//Now set the interpolation durations to zero, so we will just go directly to the new settings
			NewSettings.Bloom_InterpolationDuration = 0;
			NewSettings.MotionBlur_InterpolationDuration = 0;
			NewSettings.DOF_InterpolationDuration = 0;
			NewSettings.Scene_InterpolationDuration = 0;
			NewSettings.RimShader_InterpolationDuration = 0;
			NewSettings.MobileColorGrading.TransitionTime = 0;
			NewSettings.MobilePostProcess.Mobile_TransitionTime = 0;
		}
		bWantToResetToMapDefaultPP = !GWorld->GetWorldInfo()->bPersistPostProcessToNextLevel;
		LastMap = Map;
	}
	// Update info for when a new volume goes into use
	if( LevelPPInfo.LastVolumeUsed != NewVolume )
	{
		LevelPPInfo.LastVolumeUsed = NewVolume;
		LevelPPInfo.BlendStartTime = CurrentWorldTime;
	}
	// Lerp the level settings. Use that to prime the CurrentPPInfo.
	UpdatePPSetting(LevelPPInfo, NewSettings, CurrentWorldTime);
	CurrentPPInfo.LastSettings = LevelPPInfo.LastSettings;
	// END LEVEL
	
	//	ACTOR (CONTROLLER)
	//	Give the controller an opportunity to do any modifications.
	//	NOTE: Camera anims work through this channel
	if (Actor != NULL)
	{
		Actor->ModifyPostProcessSettings(CurrentPPInfo.LastSettings);
	}
	//	END ACTOR (CONTROLLER)

	//	CAMERA OVERRIDE
	//	NOTE: Matinee works through this channel
	if(Actor && Actor->PlayerCamera && Actor->PlayerCamera->CamOverridePostProcessAlpha > 0.f)
	{
		//Blend the currently computed level settings with the camera's settings at the camera's alpha level
		Actor->PlayerCamera->CamPostProcessSettings.OverrideSettingsFor(CurrentPPInfo.LastSettings, Actor->PlayerCamera->CamOverridePostProcessAlpha);
	}
	//	END CAMERA OVERRIDE
	
	// GAMEPLAY OVERRIDE
	for (INT OverrideIdx=0; OverrideIdx<ActivePPOverrides.Num(); ++OverrideIdx)
	{
		FPostProcessSettingsOverride& PPSO = ActivePPOverrides(OverrideIdx);

		FLOAT const DeltaTime = GWorld->GetWorldInfo()->DeltaSeconds;
		UBOOL bJustFinished = FALSE;

		// update blends
		if ( PPSO.TimeAlphaCurve.Points.Num() > 0 )
		{
			// Curve based blending
			PPSO.CurrentBlendInTime += DeltaTime;
			FLOAT const CurrentBlendWeight = PPSO.TimeAlphaCurve.Eval( PPSO.CurrentBlendInTime, 0.f );
			PPSO.Settings.OverrideSettingsFor( CurrentPPInfo.LastSettings, CurrentBlendWeight );
			if ( PPSO.CurrentBlendInTime >= PPSO.BlendInDuration )
			{
				// this override is done, expire it
				ActivePPOverrides.Remove(OverrideIdx, 1);
				OverrideIdx--;
			}
		}
		else
		{
			// Non curve blending uses blendIn/blendOut times

			if (PPSO.bBlendingIn)
			{
				PPSO.CurrentBlendInTime += DeltaTime;
				if (PPSO.CurrentBlendInTime > PPSO.BlendInDuration)
				{
					// done blending in!
					PPSO.bBlendingIn = FALSE;
					ClearPostProcessSettingsOverride( PPSO.BlendInDuration );
				}
			}
			if (PPSO.bBlendingOut)
			{
				PPSO.CurrentBlendOutTime += DeltaTime;
				if (PPSO.CurrentBlendOutTime > PPSO.BlendOutDuration)
				{
					// done!
					bJustFinished = TRUE;
				}
			}

			if (bJustFinished)
			{
				// this override is done, expire it
				ActivePPOverrides.Remove(OverrideIdx, 1);
				OverrideIdx--;
			}
			else
			{
				// calculate blend weight. calculating separately and taking the minimum handles overlapping blends nicely.
				FLOAT const BlendInWeight = (PPSO.bBlendingIn) ? (PPSO.CurrentBlendInTime / PPSO.BlendInDuration) : 1.f;
				FLOAT const BlendOutWeight = (PPSO.bBlendingOut) ? (1.f - PPSO.CurrentBlendOutTime / PPSO.BlendOutDuration) : 1.f;
				FLOAT const CurrentBlendWeight = ::Min(BlendInWeight, BlendOutWeight);

				if (CurrentBlendWeight > 0.f)
				{
					// interp into a copy so it's not destructive
					FCurrentPostProcessVolumeInfo OverridePPInfo = CurrentPPInfo;
					OverridePPInfo.BlendStartTime = PPSO.BlendStartTime;

					// this will update all of the internal interpolations (e.g. Bloom_InterpolationDuration)
					// and store the result into OverridePPInfo
					UpdatePPSetting(OverridePPInfo, PPSO.Settings, CurrentWorldTime);
					
					PPSO.Settings.OverrideSettingsFor( CurrentPPInfo.LastSettings, CurrentBlendWeight );

					// now blend that result into the real output PP settings using the current opacity
					OverridePPInfo.LastSettings.OverrideSettingsFor(CurrentPPInfo.LastSettings, CurrentBlendWeight);
				}
			}
		}
	}
	// END GAMEPLAY OVERRIDE
	
	CurrentPPInfo.LastBlendTime = CurrentWorldTime;
}


// helper macro for UpdatePPSetting()
#define LERP_POSTPROCESS(Group, Name) \
	PPInfo.LastSettings.Group##_##Name = Lerp(PPInfo.LastSettings.Group##_##Name, NewSettings.Group##_##Name, LerpAmount); \
	PPInfo.LastSettings.bOverride_##Group##_##Name = NewSettings.bOverride_##Group##_##Name;

// helper macro for UpdatePPSetting()
#define SET_POSTPROCESS(Group, Name) \
	PPInfo.LastSettings.Group##_##Name = NewSettings.Group##_##Name; \
	PPInfo.LastSettings.bOverride_##Group##_##Name = NewSettings.bOverride_##Group##_##Name;

// helper macro for UpdatePPSetting()
#define LERP_POSTPROCESS_MUL(Group, Name, Multiplier) \
	PPInfo.LastSettings.Group##_##Name = Lerp(PPInfo.LastSettings.Group##_##Name, NewSettings.Group##_##Name * Multiplier, LerpAmount); \
	PPInfo.LastSettings.bOverride_##Group##_##Name = NewSettings.bOverride_##Group##_##Name;

/** Update a specific CurrentPostProcessVolumeInfo with the settings and volume specified 
 *
 *	@param PPInfo - The CurrentPostProcessVolumeInfo struct to update
 *	@param NewSettings - The PostProcessSettings to apply to PPInfo
 *	@param NewVolume - The PostProcessVolume to apply to PPInfo
 */
void ULocalPlayer::UpdatePPSetting(FCurrentPostProcessVolumeInfo& PPInfo, FPostProcessSettings& NewSettings, const FLOAT CurrentWorldTime)
{
	// Calculate the blend factors.
	const FLOAT DeltaTime = Max(CurrentWorldTime - PPInfo.LastBlendTime,0.f);
	const FLOAT ElapsedBlendTime = Max(PPInfo.LastBlendTime - PPInfo.BlendStartTime,0.f);

	// toggles
	PPInfo.LastSettings.bEnableBloom = NewSettings.bEnableBloom;
	PPInfo.LastSettings.bEnableDOF = NewSettings.bEnableDOF;
	PPInfo.LastSettings.bEnableMotionBlur = NewSettings.bEnableMotionBlur;
	PPInfo.LastSettings.bEnableSceneEffect = NewSettings.bEnableSceneEffect;
	PPInfo.LastSettings.bAllowAmbientOcclusion = NewSettings.bAllowAmbientOcclusion;

	// ?
	PPInfo.LastSettings.bOverrideRimShaderColor = NewSettings.bOverrideRimShaderColor;

	if (PPInfo.LastSettings.bEnableBloom)
	{
		// calc bloom lerp amount
		FLOAT LerpAmount = 1.f;
		const FLOAT RemainingBloomBlendTime = Max(NewSettings.Bloom_InterpolationDuration - ElapsedBlendTime,0.f);
		if(RemainingBloomBlendTime > DeltaTime)
		{
			LerpAmount = Clamp<FLOAT>(DeltaTime / RemainingBloomBlendTime,0.f,1.f);
		}
		// bloom values
		LERP_POSTPROCESS(Bloom, Scale)
		LERP_POSTPROCESS(Bloom, Threshold)
		LERP_POSTPROCESS(Bloom, ScreenBlendThreshold)

		// this one is in the wrong category (DOF, should be bloom)
		LERP_POSTPROCESS(DOF, BlurBloomKernelSize)

		PPInfo.LastSettings.Bloom_Tint = Lerp<FLinearColor>(FLinearColor(PPInfo.LastSettings.Bloom_Tint), FLinearColor(NewSettings.Bloom_Tint), LerpAmount).ToFColor(TRUE);
		PPInfo.LastSettings.bOverride_Bloom_Tint = NewSettings.bOverride_Bloom_Tint;
	}

	if (PPInfo.LastSettings.bEnableDOF)
	{
		// calc dof lerp amount
		FLOAT LerpAmount = 1.f;
		const FLOAT RemainingDOFBlendTime = Max(NewSettings.DOF_InterpolationDuration - ElapsedBlendTime,0.f);
		if(RemainingDOFBlendTime > DeltaTime)
		{
			LerpAmount = Clamp<FLOAT>(DeltaTime / RemainingDOFBlendTime,0.f,1.f);
		}
		// dof values
		LERP_POSTPROCESS(DOF, FalloffExponent)
		LERP_POSTPROCESS(DOF, BlurKernelSize)
		LERP_POSTPROCESS(DOF, MaxNearBlurAmount)
		LERP_POSTPROCESS(DOF, MinBlurAmount)
		LERP_POSTPROCESS(DOF, MaxFarBlurAmount)
		SET_POSTPROCESS(DOF, FocusType)
		LERP_POSTPROCESS(DOF, FocusInnerRadius)
		LERP_POSTPROCESS(DOF, FocusDistance)
		LERP_POSTPROCESS(DOF, FocusPosition)
		SET_POSTPROCESS(DOF, BokehTexture);
	}

	if (PPInfo.LastSettings.bEnableMotionBlur)
	{
		// calc motion blur lerp amount
		FLOAT LerpAmount = 1.f;
		const FLOAT RemainingMotionBlurBlendTime = Max(NewSettings.MotionBlur_InterpolationDuration - ElapsedBlendTime,0.f);
		if(RemainingMotionBlurBlendTime > DeltaTime)
		{
			LerpAmount = Clamp<FLOAT>(DeltaTime / RemainingMotionBlurBlendTime,0.f,1.f);
		}
		// motion blur values
		LERP_POSTPROCESS(MotionBlur, MaxVelocity)
		LERP_POSTPROCESS(MotionBlur, Amount)
		LERP_POSTPROCESS(MotionBlur, CameraRotationThreshold)
		LERP_POSTPROCESS(MotionBlur, CameraTranslationThreshold)
		LERP_POSTPROCESS(MotionBlur, FullMotionBlur)
	}

	if (PPInfo.LastSettings.bEnableSceneEffect)
	{
		// calc scene material lerp amount
		FLOAT LerpAmount = 1.f;
		const FLOAT RemainingSceneBlendTime = Max(NewSettings.Scene_InterpolationDuration - ElapsedBlendTime,0.f);
		if(RemainingSceneBlendTime > DeltaTime)
		{
			LerpAmount = Clamp<FLOAT>(DeltaTime / RemainingSceneBlendTime,0.f,1.f);
		}

		// scene material values
		LERP_POSTPROCESS_MUL(Scene, HighLights, PP_HighlightsMultiplier)
		LERP_POSTPROCESS_MUL(Scene, MidTones, PP_MidTonesMultiplier)
		LERP_POSTPROCESS_MUL(Scene, Shadows, PP_ShadowsMultiplier)
		LERP_POSTPROCESS_MUL(Scene, Desaturation, PP_DesaturationMultiplier)
		LERP_POSTPROCESS(Scene, Colorize)

		// Clamp desaturation to 0..1 range to allow desaturation multipliers > 1 without color shifts at high desaturation.
		PPInfo.LastSettings.Scene_Desaturation = Clamp( PPInfo.LastSettings.Scene_Desaturation, 0.f, 1.f );

		if(PPInfo.LastSettings.ColorGradingLUT.IsLUTEmpty())
		{
			PPInfo.LastSettings.ColorGradingLUT.Reset();
		}
		PPInfo.LastSettings.ColorGradingLUT.LerpTo(NewSettings.ColorGrading_LookupTable, LerpAmount);
		PPInfo.LastSettings.bOverride_Scene_ColorGradingLUT = NewSettings.bOverride_Scene_ColorGradingLUT;

		LERP_POSTPROCESS(Scene, TonemapperScale)
		LERP_POSTPROCESS(Scene, ImageGrainScale)
	}

	// rim shader color override
	if (PPInfo.LastSettings.bOverrideRimShaderColor)
	{
		// calc rim shader material lerp amount
		FLOAT LerpAmount = 1.f;
		const FLOAT RemainingRimShaderBlendTime = Max(NewSettings.RimShader_InterpolationDuration - ElapsedBlendTime,0.f);
		if(RemainingRimShaderBlendTime > DeltaTime)
		{
			LerpAmount = Clamp<FLOAT>(DeltaTime / RemainingRimShaderBlendTime,0.f,1.f);
		}
		PPInfo.LastSettings.RimShader_Color = Lerp<FLinearColor>(PPInfo.LastSettings.RimShader_Color,NewSettings.RimShader_Color,LerpAmount);
		PPInfo.LastSettings.bOverride_RimShader_Color = NewSettings.bOverride_RimShader_Color;
	}
	
	// mobile color grading
	{
		FLOAT LerpAmount = 1.0f;
		const FLOAT RemainingBlendTime = Max( NewSettings.MobileColorGrading.TransitionTime - ElapsedBlendTime, 0.0f );
		if ( RemainingBlendTime > DeltaTime )
		{
			LerpAmount = Clamp<FLOAT>( DeltaTime / RemainingBlendTime, 0.0f, 1.0f );
		}
		PPInfo.LastSettings.MobileColorGrading.Blend = Lerp( PPInfo.LastSettings.MobileColorGrading.Blend, NewSettings.MobileColorGrading.Blend, LerpAmount );
		PPInfo.LastSettings.MobileColorGrading.Desaturation = Lerp( PPInfo.LastSettings.MobileColorGrading.Desaturation, NewSettings.MobileColorGrading.Desaturation, LerpAmount );
		PPInfo.LastSettings.MobileColorGrading.HighLights = Lerp( PPInfo.LastSettings.MobileColorGrading.HighLights, NewSettings.MobileColorGrading.HighLights, LerpAmount );
		PPInfo.LastSettings.MobileColorGrading.MidTones = Lerp( PPInfo.LastSettings.MobileColorGrading.MidTones, NewSettings.MobileColorGrading.MidTones, LerpAmount );
		PPInfo.LastSettings.MobileColorGrading.Shadows = Lerp( PPInfo.LastSettings.MobileColorGrading.Shadows, NewSettings.MobileColorGrading.Shadows, LerpAmount );
	}

	// Mobile post-process settings
	if ( PPInfo.LastSettings.bEnableBloom || PPInfo.LastSettings.bEnableDOF )
	{
		FLOAT LerpAmount = 1.0f;
		const FLOAT RemainingBlendTime = Max( NewSettings.MobilePostProcess.Mobile_TransitionTime - ElapsedBlendTime, 0.0f );
		if ( RemainingBlendTime > DeltaTime )
		{
			LerpAmount = Clamp<FLOAT>( DeltaTime / RemainingBlendTime, 0.0f, 1.0f );
		}
		PPInfo.LastSettings.MobilePostProcess.Mobile_BlurAmount = Lerp( PPInfo.LastSettings.MobilePostProcess.Mobile_BlurAmount, NewSettings.MobilePostProcess.Mobile_BlurAmount, LerpAmount );
		PPInfo.LastSettings.MobilePostProcess.Mobile_Bloom_Scale = Lerp( PPInfo.LastSettings.MobilePostProcess.Mobile_Bloom_Scale, NewSettings.MobilePostProcess.Mobile_Bloom_Scale, LerpAmount );
		PPInfo.LastSettings.MobilePostProcess.Mobile_Bloom_Threshold = Lerp( PPInfo.LastSettings.MobilePostProcess.Mobile_Bloom_Threshold, NewSettings.MobilePostProcess.Mobile_Bloom_Threshold, LerpAmount );
		PPInfo.LastSettings.MobilePostProcess.Mobile_Bloom_Tint = Lerp( PPInfo.LastSettings.MobilePostProcess.Mobile_Bloom_Tint, NewSettings.MobilePostProcess.Mobile_Bloom_Tint, LerpAmount );
		PPInfo.LastSettings.MobilePostProcess.Mobile_DOF_Distance = Lerp( PPInfo.LastSettings.MobilePostProcess.Mobile_DOF_Distance, NewSettings.MobilePostProcess.Mobile_DOF_Distance, LerpAmount );
		PPInfo.LastSettings.MobilePostProcess.Mobile_DOF_MinRange = Lerp( PPInfo.LastSettings.MobilePostProcess.Mobile_DOF_MinRange, NewSettings.MobilePostProcess.Mobile_DOF_MinRange, LerpAmount );
		PPInfo.LastSettings.MobilePostProcess.Mobile_DOF_MaxRange = Lerp( PPInfo.LastSettings.MobilePostProcess.Mobile_DOF_MaxRange, NewSettings.MobilePostProcess.Mobile_DOF_MaxRange, LerpAmount );
		PPInfo.LastSettings.MobilePostProcess.Mobile_DOF_FarBlurFactor = Lerp( PPInfo.LastSettings.MobilePostProcess.Mobile_DOF_FarBlurFactor, NewSettings.MobilePostProcess.Mobile_DOF_FarBlurFactor, LerpAmount );
	}
	
	// Update the current settings and timer.
	PPInfo.LastBlendTime = CurrentWorldTime;
}

#undef LERP_POSTPROCESS
#undef SET_POSTPROCESS
#undef LERP_POSTPROCESS_MUL

/**
 * Begins an override of the current post process settings.
 */
void ULocalPlayer::OverridePostProcessSettings(struct FPostProcessSettings OverrideSettings,FLOAT BlendInTime)
{
// 	// check to see if this override is already active, and bail if so
// 	// struct compares here are ugly, but should be rare in all but the most pathological use cases
// 	for (INT OverrideIdx=0; OverrideIdx<ActivePPOverrides.Num(); ++OverrideIdx)
// 	{
// 		FPostProcessSettingsOverride& PPSO = ActivePPOverrides(OverrideIdx);
// 		if ( !PPSO.bBlendingOut && (OverrideSettings == PPSO.Settings) )
// 		{
// 			return;
// 		}
// 	}

	// crossfade any existing active override
	ClearPostProcessSettingsOverride(BlendInTime);

	// create new override
	FPostProcessSettingsOverride NewPPSO;
	appMemZero( NewPPSO.TimeAlphaCurve.Points );//, sizeof(NewPPSO.TimeAlphaCurve.Points) );
	NewPPSO.Settings = OverrideSettings;
	NewPPSO.bBlendingIn = (BlendInTime > 0) ? TRUE : FALSE;
	NewPPSO.BlendInDuration = BlendInTime;
	NewPPSO.bBlendingOut = FALSE;
	NewPPSO.CurrentBlendInTime = 0.f;
	
	NewPPSO.BlendStartTime = GWorld->GetWorldInfo()->RealTimeSeconds;

	// stick it on the end of the list
	ActivePPOverrides.AddItem(NewPPSO);
}

/**
 * Begins an override of the current post process settings.
 */
void ULocalPlayer::OverridePostProcessSettingsCurve(struct FPostProcessSettings OverrideSettings, const FInterpCurveFloat &Curve)
{
	// crossfade any existing active override
	ClearPostProcessSettingsOverride(0.f);

	// create new override
	FPostProcessSettingsOverride NewPPSO;
	appMemZero( NewPPSO.TimeAlphaCurve.Points );//, sizeof(NewPPSO.TimeAlphaCurve.Points) );
	NewPPSO.Settings = OverrideSettings;
	NewPPSO.bBlendingIn = FALSE;
	NewPPSO.bBlendingOut = FALSE;
	NewPPSO.TimeAlphaCurve = Curve;
	NewPPSO.BlendInDuration = (Curve.Points.Num() > 0 ? Curve.Points.Last().InVal : 0.f);
	NewPPSO.CurrentBlendInTime = 0.f;
	NewPPSO.BlendStartTime = GWorld->GetWorldInfo()->RealTimeSeconds;

	// stick it on the end of the list
	ActivePPOverrides.AddItem(NewPPSO);
}

/**
 * Stop overriding post process settings.
 * Will only affect active overrides -- in-progress blendouts are unaffected.
 * @param BlendOutTime - The amount of time you want to take to recover from the override you are clearing.
 */
void ULocalPlayer::ClearPostProcessSettingsOverride(FLOAT BlendOutTime)
{
	// foreach ActivePPOverride
	for (INT OverrideIdx=0; OverrideIdx<ActivePPOverrides.Num(); ++OverrideIdx)
	{
		FPostProcessSettingsOverride& PPSO = ActivePPOverrides(OverrideIdx);

		if (BlendOutTime > 0.f)
		{
			if (!PPSO.bBlendingOut)
			{
				PPSO.bBlendingOut = TRUE;
				PPSO.BlendOutDuration = BlendOutTime;
				PPSO.CurrentBlendOutTime = 0.f;
			}
		}
		else
		{
			// expire immediately
			ActivePPOverrides.Remove(OverrideIdx, 1);
			OverrideIdx--;
		}
	}
}


/**
 * Calculate the view settings for drawing from this view actor
 *
 * @param	View - output view struct
 * @param	ViewLocation - output actor location
 * @param	ViewRotation - output actor rotation
 * @param	Viewport - current client viewport
 * @param	ViewDrawer - optional drawing in the view
 */
#if WITH_REALD
FSceneView* ULocalPlayer::CalcSceneView( FSceneViewFamily* ViewFamily, FVector& ViewLocation, FRotator& ViewRotation, FViewport* Viewport, FViewElementDrawer* ViewDrawer/*=NULL*/ )
{
	return CalcSceneViewOffs(ViewFamily, ViewLocation, ViewRotation, Viewport, NULL, ViewDrawer);
}

FSceneView* ULocalPlayer::CalcSceneViewOffs( FSceneViewFamily* ViewFamily, FVector& ViewLocation, FRotator& ViewRotation, FViewport* Viewport, FMatrix* Offset/*=NULL*/, FViewElementDrawer* ViewDrawer/*=NULL*/ )
#else
FSceneView* ULocalPlayer::CalcSceneView( FSceneViewFamily* ViewFamily, FVector& ViewLocation, FRotator& ViewRotation, FViewport* Viewport, FViewElementDrawer* ViewDrawer/*=NULL*/ )
#endif
{
	if( !Actor )
	{
		return NULL;
	}

	// do nothing if the viewport size is zero - this allows the viewport client the capability to temporarily remove a viewport without actually destroying and recreating it
	if (Size.X <= 0.f || Size.Y <= 0.f)
	{
		return NULL;
	}

	check(Viewport);

	// Compute the view's screen rectangle.
	INT X = appTrunc(Origin.X * Viewport->GetSizeX());
	INT Y = appTrunc(Origin.Y * Viewport->GetSizeY());
	INT ClipX = X;
	INT ClipY = Y;
	UINT SizeX = appTrunc(Size.X * Viewport->GetSizeX());
	UINT SizeY = appTrunc(Size.Y * Viewport->GetSizeY());
	INT UnmodifiedSizeX = SizeX;
	INT UnmodifiedSizeY = SizeY;
	
	FLOAT fFOV = Actor->eventGetFOVAngle();

	// if the object propagtor is pushing us new values, use them instead of the player
	UBOOL bCameraCut = FALSE;
	if (bOverrideView)
	{
		ViewLocation = OverrideLocation;
		ViewRotation = OverrideRotation;
		fFOV = Actor->DefaultFOV;
	}
	else
	{
		Actor->eventGetPlayerViewPoint( ViewLocation, ViewRotation );
		bCameraCut = Actor->bCameraCut;
	}

	// scale distances for cull distance purposes by the ratio of our current FOV to the default FOV
	Actor->LODDistanceFactor = fFOV / Max<FLOAT>(0.01f, (Actor->PlayerCamera != NULL) ? Actor->PlayerCamera->DefaultFOV : Actor->DefaultFOV);
	
	FMatrix ViewMatrix = FTranslationMatrix(-ViewLocation);
	ViewMatrix = ViewMatrix * FInverseRotationMatrix(ViewRotation);
	ViewMatrix = ViewMatrix * FMatrix(
									  FPlane(0,	0,	1,	0),
									  FPlane(1,	0,	0,	0),
									  FPlane(0,	1,	0,	0),
									  FPlane(0,	0,	0,	1));
#if WITH_REALD
	if ( Offset != NULL ) 
	{
		ViewMatrix = ViewMatrix * (*Offset);
	}   
#endif

	UGameUISceneClient* SceneClient = UUIRoot::GetSceneClient();

	FMatrix ProjectionMatrix;
	if( Actor && Actor->PlayerCamera != NULL && Actor->PlayerCamera->bConstrainAspectRatio )
	{
		ProjectionMatrix = FPerspectiveMatrix(
			fFOV * (FLOAT)PI / 360.0f,
			Actor->PlayerCamera->ConstrainedAspectRatio,
			1.0f,
			GNearClippingPlane
			);

		// Enforce a particular aspect ratio for the render of the scene. 
		// Results in black bars at top/bottom etc.
		Viewport->CalculateViewExtents( 
				Actor->PlayerCamera->ConstrainedAspectRatio, 
				X, Y, SizeX, SizeY );
	}
	else 
	{
		FLOAT MatrixFOV = fFOV * (FLOAT)PI / 360.0f;
		FLOAT XAxisMultiplier;
		FLOAT YAxisMultiplier;

		//if x is bigger, and we're respecting x or major axis, AND mobile isn't forcing us to be Y axis aligned
		if (((SizeX > SizeY) && (AspectRatioAxisConstraint == AspectRatio_MajorAxisFOV)) || (AspectRatioAxisConstraint == AspectRatio_MaintainXFOV))
		{
			//if the viewport is wider than it is tall
			XAxisMultiplier = 1.0f;
			YAxisMultiplier = SizeX / (FLOAT)SizeY;
		}
		else
		{
			//if the viewport is taller than it is wide
			XAxisMultiplier = SizeY / (FLOAT)SizeX;
			YAxisMultiplier = 1.0f;
		}

		ProjectionMatrix = FPerspectiveMatrix (
			MatrixFOV,
			MatrixFOV,
			XAxisMultiplier,
			YAxisMultiplier,
			GNearClippingPlane,
			GNearClippingPlane
			);
	}

	// Set up the rendering overrides.
	FRenderingPerformanceOverrides RenderingOverrides = Actor->PlayerCamera ? Actor->PlayerCamera->RenderingOverrides : FRenderingPerformanceOverrides(E_ForceInit);	

	// If temporal AA is disabled in the WorldInfo, propagate that to the rendering overrides.
	if(!GWorld->GetWorldInfo()->GetAllowTemporalAA())
	{
		RenderingOverrides.bAllowTemporalAA = FALSE;
	}

	FTemporalAAParameters TemporalAAParameters = CalcTemporalAAParameters(
		(ViewFamily->ShowFlags & SHOW_TemporalAA) && RenderingOverrides.bAllowTemporalAA,
		SizeX,
		SizeY,
		(ViewLocation - LastViewLocation).Size());

	// Scale start depth according to FOV.
	TemporalAAParameters.StartDepth /= Actor->LODDistanceFactor;

#if WITH_APEX
	if ( GWorld && GWorld->RBPhysScene && GWorld->RBPhysScene->ApexScene )
	{
		FIApexScene* ApexScene = GWorld->RBPhysScene->ApexScene;
		ApexScene->UpdateProjection(
			ProjectionMatrix,
			fFOV,
			SizeX * Viewport->GetDesiredAspectRatio() * (FLOAT)Viewport->GetSizeY() / (FLOAT)Viewport->GetSizeX(),
			SizeY,
			GNearClippingPlane
			);
		ApexScene->UpdateView( ViewMatrix );
	}
#endif

	// Take screen percentage option into account if percentage != 100.
	// Note: this needs to be done after the view size and position are final
	GSystemSettings.ScaleScreenCoords(X,Y,SizeX,SizeY);

	FLinearColor OverlayColor(0,0,0,0);
	FLinearColor ColorScale(FLinearColor::White);

	if( Actor && Actor->PlayerCamera )
	{
		// Apply screen fade effect to screen.
		if(Actor->PlayerCamera->bEnableFading)
		{
			OverlayColor = Actor->PlayerCamera->FadeColor.ReinterpretAsLinear();
			OverlayColor.A = Clamp(Actor->PlayerCamera->FadeAmount,0.0f,1.0f);
		}

		// Do color scaling if desired.
		if(Actor->PlayerCamera->bEnableColorScaling)
		{
			ColorScale = FLinearColor(
				Actor->PlayerCamera->ColorScale.X,
				Actor->PlayerCamera->ColorScale.Y,
				Actor->PlayerCamera->ColorScale.Z
				);
		}

		if (Actor->PlayerCamera->bForceDisableTemporalAA)
		{
			RenderingOverrides.bAllowTemporalAA = FALSE;
		}
	}

	// Update the player's post process settings.
	UpdatePostProcessSettings(ViewLocation);

	// Update the pawn MICs to have proper rim light colors.
	FName static RimLightColorName = FName(TEXT("RimLightColor"));
	for (APawn *Pawn = GWorld->GetWorldInfo()->PawnList; Pawn != NULL; Pawn = Pawn->NextPawn)
	{
		if (Pawn->MIC_PawnMat)
		{
			Pawn->MIC_PawnMat->SetVectorParameterValue(RimLightColorName, CurrentPPInfo.LastSettings.RimShader_Color);
		}
		if (Pawn->MIC_PawnHair)
		{
			Pawn->MIC_PawnHair->SetVectorParameterValue(RimLightColorName, CurrentPPInfo.LastSettings.RimShader_Color);
		}
	}

	TSet<UPrimitiveComponent*> HiddenPrimitives;

	// Translate the camera's hidden actors list to a hidden primitive list.
	Actor->UpdateHiddenActors(ViewLocation);
	TArray<AActor*>& HiddenActors = Actor->HiddenActors;
	for(INT ActorIndex = 0; ActorIndex < HiddenActors.Num(); ActorIndex++)
	{
		AActor* HiddenActor = HiddenActors(ActorIndex);
		if (HiddenActor != NULL)
		{
			for(INT ComponentIndex = 0; ComponentIndex < HiddenActor->AllComponents.Num(); ComponentIndex++)
			{
				UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(HiddenActor->AllComponents(ComponentIndex));
				if(PrimitiveComponent && !PrimitiveComponent->bIgnoreHiddenActorsMembership)
				{
					HiddenPrimitives.Add(PrimitiveComponent);
				}
			}
		}
		else
		{
			HiddenActors.Remove(ActorIndex);
			ActorIndex--;
		}
	}
	// Allow the player controller a chance to operate on a per primitive basis
	Actor->UpdateHiddenComponents(ViewLocation,HiddenPrimitives);

	// Use the default post process instead of the players if it been requested (usually when entering a pp volume)
    UPostProcessChain *ActivePostProccessChain = (bForceDefaultPostProcessChain) ? GEngine->GetDefaultPostProcessChain() : PlayerPostProcess;
   
	FSceneView* View = new FSceneView(
		ViewFamily,
		ViewState,
		-1,
		NULL,
#if USE_ACTOR_VISIBILITY_HISTORY
		&ActorVisibilityHistory,
#else
		NULL,
#endif
		Actor->GetViewTarget(),
		ActivePostProccessChain,
		&CurrentPPInfo.LastSettings,
		ViewDrawer,
		X,
		Y,
		ClipX,
		ClipY,
		SizeX,
		SizeY,
		ViewMatrix,
		ProjectionMatrix,
		FLinearColor::Black,
		OverlayColor,
		ColorScale,
		HiddenPrimitives,
		RenderingOverrides,
		Actor->LODDistanceFactor,
		bCameraCut,
		TemporalAAParameters
		);

	View->bForceLowestMassiveLOD = GWorld->GetWorldInfo()->IsInsideMassiveLODVolume(ViewLocation);

	if ((X != ClipX) || (Y != ClipY) || (SizeX != UnmodifiedSizeX) || (SizeY != UnmodifiedSizeY))
	{
		View->bForceClear=TRUE;
	}

	ViewFamily->Views.AddItem(View);

	return View;
}

/** transforms 2D screen coordinates into a 3D world-space origin and direction
 * @note: use the Canvas version where possible as it already has the necessary information,
 *	whereas this function must gather it and is therefore slower
 * @param ScreenPos - screen coordinates in pixels
 * @param WorldOrigin (out) - world-space origin vector
 * @param WorldDirection (out) - world-space direction vector
 */
void ULocalPlayer::DeProject(FVector2D RelativeScreenPos, FVector& WorldOrigin, FVector& WorldDirection)
{
	if (ViewportClient != NULL && ViewportClient->Viewport != NULL && Actor != NULL)
	{
		// create the view family for rendering the world scene to the viewport's render target
		FSceneViewFamilyContext ViewFamily(
			ViewportClient->Viewport,
			GWorld->Scene,
			ViewportClient->ShowFlags,
			Actor->WorldInfo->TimeSeconds,
			Actor->WorldInfo->DeltaSeconds,
			Actor->WorldInfo->RealTimeSeconds, 
			FALSE,
			FALSE,
			FALSE,
			FALSE,
			TRUE
			);

		// Calculate the player's view information.
		FVector ViewLocation;
		FRotator ViewRotation;

		FSceneView* View = CalcSceneView(&ViewFamily, ViewLocation, ViewRotation, ViewportClient->Viewport, NULL);

		FVector2D ScreenPos(RelativeScreenPos.X * View->SizeX, RelativeScreenPos.Y * View->SizeY);
		View->DeprojectFVector2D(ScreenPos, WorldOrigin, WorldDirection);
	}
}

/** transforms 3D world coordinates into a 2D screen position (0-1, 0-1)
 * @note: use the Canvas version where possible as it already has the necessary information,
 *	whereas this function must gather it and is therefore slower
 * @param WorldLoc - world location to project
 * @return screen coordinates (0-1, 0-1)
 */
FVector2D ULocalPlayer::Project(FVector WorldLoc)
{
	if (ViewportClient != NULL && ViewportClient->Viewport != NULL && Actor != NULL)
	{
		// create the view family for rendering the world scene to the viewport's render target
		FSceneViewFamilyContext ViewFamily(
			ViewportClient->Viewport,
			GWorld->Scene,
			ViewportClient->ShowFlags,
			Actor->WorldInfo->TimeSeconds,
			Actor->WorldInfo->DeltaSeconds,
			Actor->WorldInfo->RealTimeSeconds, 
			FALSE,
			FALSE,
			FALSE,
			FALSE,
			TRUE
			);

		// Calculate the player's view information.
		FVector ViewLocation;
		FRotator ViewRotation;

		FSceneView* View = CalcSceneView(&ViewFamily, ViewLocation, ViewRotation, ViewportClient->Viewport, NULL);

		FPlane V = View->Project(WorldLoc);
		return FVector2D((1.0f + V.X) * 0.5f, 1.0f - (1.0f + V.Y) * 0.5f);
	}
	else
	{
		return FVector2D(0.f, 0.f);
	}
}


/** transforms 2D screen coordinates into a 3D world-space origin and direction
 * @note: use the Canvas version where possible as it already has the necessary information,
 *	whereas this function must gather it and is therefore slower
 * @param ScreenPos - screen coordinates in pixels
 * @param WorldOrigin (out) - world-space origin vector
 * @param WorldDirection (out) - world-space direction vector
 */
void ULocalPlayer::FastDeProject(FVector2D RelativeScreenPos, FVector& WorldOrigin, FVector& WorldDirection)
{
	if (ViewportClient != NULL && ViewportClient->Viewport != NULL && Actor != NULL)
	{
		// calculate view
		FVector ViewLocation;
		FRotator ViewRotation;

		// Compute the view's screen rectangle.
		INT X = appTrunc(Origin.X * ViewportClient->Viewport->GetSizeX());
		INT Y = appTrunc(Origin.Y * ViewportClient->Viewport->GetSizeY());
		INT ClipX = X;
		INT ClipY = Y;
		UINT SizeX = appTrunc(Size.X * ViewportClient->Viewport->GetSizeX());
		UINT SizeY = appTrunc(Size.Y * ViewportClient->Viewport->GetSizeY());

		FLOAT fFOV = Actor->eventGetFOVAngle();

		Actor->eventGetPlayerViewPoint( ViewLocation, ViewRotation );

		FMatrix ViewMatrix = FTranslationMatrix(-ViewLocation);
		ViewMatrix = ViewMatrix * FInverseRotationMatrix(ViewRotation);
		ViewMatrix = ViewMatrix * FMatrix(
										  FPlane(0,	0,	1,	0),
										  FPlane(1,	0,	0,	0),
										  FPlane(0,	1,	0,	0),
										  FPlane(0,	0,	0,	1));

		FMatrix ProjectionMatrix;
		if( Actor->PlayerCamera != NULL && Actor->PlayerCamera->bConstrainAspectRatio )
		{
			ProjectionMatrix = FPerspectiveMatrix(
				fFOV * (FLOAT)PI / 360.0f,
				Actor->PlayerCamera->ConstrainedAspectRatio,
				1.0f,
				GNearClippingPlane
				);
		}
		else
		{
			FLOAT MatrixFOV = fFOV * (FLOAT)PI / 360.0f;
			FLOAT XAxisMultiplier;
			FLOAT YAxisMultiplier;

			//if x is bigger, and we're respecting x or major axis, AND mobile isn't forcing us to be Y axis aligned
			if (((SizeX > SizeY) && (AspectRatioAxisConstraint == AspectRatio_MajorAxisFOV)) || (AspectRatioAxisConstraint == AspectRatio_MaintainXFOV))
			{
				//if the viewport is wider than it is tall
				XAxisMultiplier = 1.0f;
				YAxisMultiplier = SizeX / (FLOAT)SizeY;
			}
			else
			{
				//if the viewport is taller than it is wide
				XAxisMultiplier = SizeY / (FLOAT)SizeX;
				YAxisMultiplier = 1.0f;
			}

			ProjectionMatrix = FPerspectiveMatrix (
				MatrixFOV,
				MatrixFOV,
				XAxisMultiplier,
				YAxisMultiplier,
				GNearClippingPlane,
				GNearClippingPlane
				);
		}

		FVector2D ScreenPos(RelativeScreenPos.X * SizeX, RelativeScreenPos.Y * SizeY);
		FMatrix InvProjectionMatrix = ProjectionMatrix.Inverse();

		// deproject

		X = appTrunc(ScreenPos.X);
		Y = appTrunc(ScreenPos.Y);

		// Get the eye position and direction of the mouse cursor in two stages (inverse transform projection, then inverse transform view).
		// This avoids the numerical instability that occurs when a view matrix with large translation is composed with a projection matrix
		FMatrix InverseView = ViewMatrix.Inverse();

		// The start of the raytrace is defined to be at mousex,mousey,0 in projection space
		// The end of the raytrace is at mousex, mousey, 0.5 in projection space
		FLOAT ScreenSpaceX = (X - FLOAT(SizeX / 2)) / FLOAT(SizeX / 2);
		FLOAT ScreenSpaceY = (Y - FLOAT(SizeY / 2)) / -FLOAT(SizeY / 2);
		FVector4 RayStartProjectionSpace = FVector4(ScreenSpaceX, ScreenSpaceY,    0, 1.0f);
		FVector4 RayEndProjectionSpace   = FVector4(ScreenSpaceX, ScreenSpaceY, 0.5f, 1.0f);

		// Projection (changing the W coordinate) is not handled by the FMatrix transforms that work with vectors, so multiplications
		// by the projection matrix should use homogenous coordinates (i.e. FPlane).
		FVector4 HGRayStartViewSpace = InvProjectionMatrix.TransformFVector4(RayStartProjectionSpace);
		FVector4 HGRayEndViewSpace   = InvProjectionMatrix.TransformFVector4(RayEndProjectionSpace);
		FVector RayStartViewSpace(HGRayStartViewSpace.X, HGRayStartViewSpace.Y, HGRayStartViewSpace.Z);
		FVector   RayEndViewSpace(HGRayEndViewSpace.X,   HGRayEndViewSpace.Y,   HGRayEndViewSpace.Z);
		// divide vectors by W to undo any projection and get the 3-space coordinate 
		if (HGRayStartViewSpace.W != 0.0f)
		{
			RayStartViewSpace /= HGRayStartViewSpace.W;
		}
		if (HGRayEndViewSpace.W != 0.0f)
		{
			RayEndViewSpace /= HGRayEndViewSpace.W;
		}
		FVector RayDirViewSpace = RayEndViewSpace - RayStartViewSpace;
		RayDirViewSpace = RayDirViewSpace.SafeNormal();

		// The view transform does not have projection, so we can use the standard functions that deal with vectors and normals (normals
		// are vectors that do not use the translational part of a rotation/translation)
		FVector RayStartWorldSpace = InverseView.TransformFVector(RayStartViewSpace);
		FVector RayDirWorldSpace   = InverseView.TransformNormal(RayDirViewSpace);

		// Finally, store the results in the hitcheck inputs.  The start position is the eye, and the end position
		// is the eye plus a long distance in the direction the mouse is pointing.
		WorldOrigin = RayStartWorldSpace;
		WorldDirection = RayDirWorldSpace.SafeNormal();
	}
}

/** transforms 3D world coordinates into a 2D screen position (0-1, 0-1)
 * @note: use the Canvas version where possible as it already has the necessary information,
 *	whereas this function must gather it and is therefore slower
 * @param WorldLoc - world location to project
 * @return screen coordinates (0-1, 0-1)
 */
FVector2D ULocalPlayer::FastProject(FVector WorldLoc)
{
	if (ViewportClient != NULL && ViewportClient->Viewport != NULL && Actor != NULL)
	{
		// Calculate the player's view information.
		FVector ViewLocation;
		FRotator ViewRotation;

		// Compute the view's screen rectangle.
		INT X = appTrunc(Origin.X * ViewportClient->Viewport->GetSizeX());
		INT Y = appTrunc(Origin.Y * ViewportClient->Viewport->GetSizeY());
		UINT SizeX = appTrunc(Size.X * ViewportClient->Viewport->GetSizeX());
		UINT SizeY = appTrunc(Size.Y * ViewportClient->Viewport->GetSizeY());

		FLOAT fFOV = Actor->eventGetFOVAngle();

		Actor->eventGetPlayerViewPoint( ViewLocation, ViewRotation );

		FMatrix ViewMatrix = FTranslationMatrix(-ViewLocation);
		ViewMatrix = ViewMatrix * FInverseRotationMatrix(ViewRotation);
		ViewMatrix = ViewMatrix * FMatrix(
			FPlane(0,	0,	1,	0),
			FPlane(1,	0,	0,	0),
			FPlane(0,	1,	0,	0),
			FPlane(0,	0,	0,	1));

		FMatrix ProjectionMatrix;
		if( Actor->PlayerCamera != NULL && Actor->PlayerCamera->bConstrainAspectRatio )
		{
			ProjectionMatrix = FPerspectiveMatrix(
				fFOV * (FLOAT)PI / 360.0f,
				Actor->PlayerCamera->ConstrainedAspectRatio,
				1.0f,
				GNearClippingPlane
				);
		}
		else
		{
			FLOAT MatrixFOV = fFOV * (FLOAT)PI / 360.0f;
			FLOAT XAxisMultiplier;
			FLOAT YAxisMultiplier;

			//if x is bigger, and we're respecting x or major axis, AND mobile isn't forcing us to be Y axis aligned
			if (((SizeX > SizeY) && (AspectRatioAxisConstraint == AspectRatio_MajorAxisFOV)) || (AspectRatioAxisConstraint == AspectRatio_MaintainXFOV))
			{
				//if the viewport is wider than it is tall
				XAxisMultiplier = 1.0f;
				YAxisMultiplier = SizeX / (FLOAT)SizeY;
			}
			else
			{
				//if the viewport is taller than it is wide
				XAxisMultiplier = SizeY / (FLOAT)SizeX;
				YAxisMultiplier = 1.0f;
			}

			ProjectionMatrix = FPerspectiveMatrix (
				MatrixFOV,
				MatrixFOV,
				XAxisMultiplier,
				YAxisMultiplier,
				GNearClippingPlane,
				GNearClippingPlane
				);
		}

		FMatrix ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;

		FPlane Result = ViewProjectionMatrix.TransformFVector4(FVector4(WorldLoc,1));
		const FLOAT RHW = 1.0f / Result.W;

		FPlane V = FPlane(Result.X * RHW,Result.Y * RHW,Result.Z * RHW,Result.W);
		return FVector2D((1.0f + V.X) * 0.5f, 1.0f - (1.0f + V.Y) * 0.5f);
	}
	else
	{
		return FVector2D(0.f, 0.f);
	}
}


/**
 * set OverrideLocation and OverrideRotation
 * @return success (syntax is correct), the data is only set if the function returns TRUE
 */
UBOOL SetOverrideView(const TCHAR* InCmd)
{
	const TCHAR*& Cmd = InCmd;

	FString x = ParseToken(Cmd, 0);
	FString y = ParseToken(Cmd, 0);
	FString z = ParseToken(Cmd, 0);
	FString wx = ParseToken(Cmd, 0);
	FString wy = ParseToken(Cmd, 0);
	FString wz = ParseToken(Cmd, 0);

	if(x.IsEmpty() || y.IsEmpty() || z.IsEmpty() || wx.IsEmpty() || wy.IsEmpty() || wz.IsEmpty())
	{
		return FALSE;
	}

	ULocalPlayer::OverrideLocation.X = appAtof(*x);
	ULocalPlayer::OverrideLocation.Y = appAtof(*y);
	ULocalPlayer::OverrideLocation.Z = appAtof(*z);

	ULocalPlayer::OverrideRotation.Pitch = appAtoi(*wx);
	ULocalPlayer::OverrideRotation.Yaw   = appAtoi(*wy);
	ULocalPlayer::OverrideRotation.Roll  = appAtoi(*wz);

	return TRUE;
}

//
//	ULocalPlayer::Exec
//

UBOOL ULocalPlayer::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	// Create a pending Note actor (only in PIE)
	if( ParseCommand(&Cmd,TEXT("DN")) )
	{
#if !FINAL_RELEASE
		// Do nothing if not in editor
		if( GIsEditor && Actor )
		{
			FString Comment = FString(Cmd);
			INT NewNoteIndex = GEngine->PendingDroppedNotes.AddZeroed();
			FDropNoteInfo& NewNote = GEngine->PendingDroppedNotes(NewNoteIndex);
			
			
			// Use the pawn's location if we have one
			if( Actor->Pawn != NULL )
			{
				NewNote.Location = Actor->Pawn->Location;
			}
			else
			{
				// No pawn, so just use the camera's location
				NewNote.Location = Actor->Location;
			}

			NewNote.Rotation = Actor->Rotation;
			NewNote.Comment = Comment;
			debugf(TEXT("Note Dropped: (%3.2f,%3.2f,%3.2f) - '%s'"), NewNote.Location.X, NewNote.Location.Y, NewNote.Location.Z, *NewNote.Comment);
		}
#endif // !FINAL_RELEASE
		return TRUE;
	}


// NOTE: all of these can probably be #if !FINAL_RELEASE out

    // This will show all of the SkeletalMeshComponents that were ticked for one frame
	else if( ParseCommand(&Cmd,TEXT("SHOWSKELCOMPTICKTIME")) )
	{
		GShouldLogOutAFrameOfSkelCompTick = TRUE;
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWLIGHTENVS")) )
	{
		GShouldLogOutAFrameOfLightEnvTick = TRUE;
		return TRUE;
	}
	// This will show all IsOverlapping calls for one frame
	else if( ParseCommand(&Cmd,TEXT("SHOWISOVERLAPPING")) )
	{
		GShouldLogOutAFrameOfIsOverlapping = TRUE;
		return TRUE;
	}
	// This will list all awake rigid bodies
	else if( ParseCommand(&Cmd,TEXT("LISTAWAKEBODIES")) )
	{
		ListAwakeRigidBodies(TRUE);
		return TRUE;
	}
	// This will list all simulating rigid bodies
	else if( ParseCommand(&Cmd,TEXT("LISTSIMBODIES")) )
	{
		ListAwakeRigidBodies(FALSE);
		return TRUE;
	}
	// Allow crowds to be toggled on and off.
	else if( ParseCommand(&Cmd, TEXT("TOGGLECROWDS")) )
	{
		GWorld->bDisableCrowds = !GWorld->bDisableCrowds;
		return TRUE;
	}
	else if( ParseCommand(&Cmd, TEXT("MOVEACTORTIMES")) )
	{
		GShouldLogOutAFrameOfMoveActor = TRUE;
		return TRUE;
	}
	else if( ParseCommand(&Cmd, TEXT("PHYSASSETBOUNDS")) )
	{
		GShouldLogOutAFrameOfPhysAssetBoundsUpdate = TRUE;
		return TRUE;
	}
	else if( ParseCommand(&Cmd, TEXT("FRAMECOMPUPDATES")) )
	{
		GShouldLogOutAFrameOfComponentUpdates = TRUE;
		return TRUE;
	}
	else if( ParseCommand(&Cmd, TEXT("FRAMEOFPAIN")) )
	{
		GShouldTraceOutAFrameOfPain = TRUE;
		return TRUE;
	}
	else if( ParseCommand(&Cmd, TEXT("SHOWSKELCOMPLODS")) )
	{
		GShouldLogOutAFrameOfSkelCompLODs = TRUE;
		return TRUE;
	}
	else if( ParseCommand(&Cmd, TEXT("SHOWSKELMESHLODS")) )
	{
		debugf(TEXT("============================================================"));
		debugf(TEXT("Verifying SkeleltalMesh : STARTING"));
		debugf(TEXT("============================================================"));

		GShouldLogOutAFrameOfSkelMeshLODs = TRUE;
		return TRUE;
	}
	else if ( ParseCommand(&Cmd, TEXT("SHOWFACEFXBONES")) )
	{
		debugf(TEXT("============================================================"));
		debugf(TEXT("Verifying FaceFX Bones of SkeletalMeshComp : STARTING"));
		debugf(TEXT("============================================================"));

		GShouldLogOutAFrameOfFaceFXBones = TRUE;
		return TRUE;
	}
	else if ( ParseCommand(&Cmd, TEXT("SHOWFACEFXDEBUG")) )
	{
		debugf(TEXT("============================================================"));

		GShouldLogOutAFrameOfFaceFXDebug = TRUE;
		return TRUE;
	}
	else if ( ParseCommand(&Cmd, TEXT("TRACEFACEFX")) )
	{
		GShouldTraceFaceFX = TRUE;
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("LISTSKELMESHES")) )
	{
		// Iterate over all skeletal mesh components and create mapping from skeletal mesh to instance.
		TMultiMap<USkeletalMesh*,USkeletalMeshComponent*> SkeletalMeshToInstancesMultiMap;
		for( TObjectIterator<USkeletalMeshComponent> It; It; ++It )
		{
			USkeletalMeshComponent* SkeletalMeshComponent = *It;
			USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;

			if( !SkeletalMeshComponent->IsTemplate() )
			{
				SkeletalMeshToInstancesMultiMap.Add( SkeletalMesh, SkeletalMeshComponent );
			}
		}

		// Retrieve player location for distance checks.
		FVector PlayerLocation = FVector(0,0,0);
		if( Actor && Actor->Pawn )
		{
			PlayerLocation = Actor->Pawn->Location;
		}

		// Iterate over multi-map and dump information sorted by skeletal mesh.
		for( TObjectIterator<USkeletalMesh> It; It; ++It )
		{
			// Look up array of instances associated with this key/ skeletal mesh.
			USkeletalMesh* SkeletalMesh = *It;
			TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
			SkeletalMeshToInstancesMultiMap.MultiFind( SkeletalMesh, SkeletalMeshComponents );

			if( SkeletalMesh && SkeletalMeshComponents.Num() )
			{
				// Dump information about skeletal mesh.
				check(SkeletalMesh->LODModels.Num());
				debugf(TEXT("%5i Vertices for LOD 0 of %s"),SkeletalMesh->LODModels(0).NumVertices,*SkeletalMesh->GetFullName());

				// Dump all instances.
				for( INT InstanceIndex=0; InstanceIndex<SkeletalMeshComponents.Num(); InstanceIndex++ )
				{
					USkeletalMeshComponent* SkeletalMeshComponent = SkeletalMeshComponents(InstanceIndex);
					FLOAT TimeSinceLastRender = GWorld->GetTimeSeconds() - SkeletalMeshComponent->LastRenderTime;

					debugf(TEXT("%s%2i  Component    : %s"), 
						(TimeSinceLastRender > 0.5) ? TEXT(" ") : TEXT("*"), 
						InstanceIndex,
						*SkeletalMeshComponent->GetFullName() );
					if( SkeletalMeshComponent->GetOwner() )
					{
						debugf(TEXT("     Owner        : %s"),*SkeletalMeshComponent->GetOwner()->GetFullName());
					}
					debugf(TEXT("     LastRender   : %f"), TimeSinceLastRender);
					debugf(TEXT("     CullDistance : %f   Distance: %f   Location: (%7.1f,%7.1f,%7.1f)"), 
						SkeletalMeshComponent->CachedMaxDrawDistance,	
						FDist( PlayerLocation, SkeletalMeshComponent->Bounds.Origin ),
						SkeletalMeshComponent->Bounds.Origin.X,
						SkeletalMeshComponent->Bounds.Origin.Y,
						SkeletalMeshComponent->Bounds.Origin.Z );
				}
			}
		}
		return TRUE;
	}
	else if ( ParseCommand(&Cmd,TEXT("LISTPAWNCOMPONENTS")) )
	{
		for (APawn *Pawn = GWorld->GetWorldInfo()->PawnList; Pawn != NULL; Pawn = Pawn->NextPawn)
		{
			debugf(TEXT("Components for pawn: %s (collision component: %s)"),*Pawn->GetName(),*Pawn->CollisionComponent->GetName());
			for (INT CompIdx = 0; CompIdx < Pawn->Components.Num(); CompIdx++)
			{
				UActorComponent *Comp = Pawn->Components(CompIdx);
				if (Comp != NULL)
				{
					debugf(TEXT("  %d: %s"),CompIdx,*Comp->GetName());
				}
			}
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("EXEC")) )
	{
		TCHAR Filename[512];
		if( ParseToken( Cmd, Filename, ARRAY_COUNT(Filename), 0 ) )
		{
			ExecMacro( Filename, Ar );
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("RESETRHI")) )
	{		
		if (GAllowFullRHIReset )
		{
			// Flush rendering commands as RT is accessing GIsCurrentlyPrecaching
			FlushRenderingCommands();

			// list of resources to restore
			TArray<FRenderResource*> StoredResourceList;
		
			// clean up the ES2 Resources held onto outside of resourcelist
	#if WITH_ES2_RHI
			void ClearES2PendingResources();
			ClearES2PendingResources();
	#endif

			for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
			{
				StoredResourceList.AddItem( *ResourceIt );
			}
			for( INT ResourceIter = 0; ResourceIter < StoredResourceList.Num(); ResourceIter++ )
			{
				StoredResourceList( ResourceIter )->ReleaseResource();
			}
			debugf(TEXT("Freed %d GPU Resources"), StoredResourceList.Num());

			for( INT ResourceIter = 0; ResourceIter < StoredResourceList.Num(); ResourceIter++ )
			{
				StoredResourceList( ResourceIter )->InitResource();
			}
			debugf(TEXT("Restored %d GPU Resources"), StoredResourceList.Num());
			StoredResourceList.Empty();		
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("TOGGLEDRAWEVENTS")) )
	{
#if XBOX && !_DEBUG
		debugf(TEXT("Draw events are automatically enabled on Xbox 360 in Release when PIX is attached; no need to use TOGGLEDRAWEVENTS"));
#else
		if( GEmitDrawEvents )
		{
			GEmitDrawEvents = FALSE;
			warnf(TEXT("Draw events are now DISABLED"));
		}
		else
		{
			GEmitDrawEvents = TRUE;
			warnf(TEXT("Draw events are now ENABLED"));
		}
#endif
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("TOGGLESTREAMINGVOLUMES")) )
	{
		if (ParseCommand(&Cmd, TEXT("ON")))
		{
			GWorld->DelayStreamingVolumeUpdates( 0 );
		}
		else if (ParseCommand(&Cmd, TEXT("OFF")))
		{
			GWorld->DelayStreamingVolumeUpdates( INDEX_NONE );
		}
		else
		{
			if( GWorld->StreamingVolumeUpdateDelay == INDEX_NONE )
			{
				GWorld->DelayStreamingVolumeUpdates( 0 );
			}
			else
			{
				GWorld->DelayStreamingVolumeUpdates( INDEX_NONE );
			}
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("PUSHVIEW")) )
	{
		if (ParseCommand(&Cmd, TEXT("START")))
		{
			bOverrideView = TRUE;
		}
		else if (ParseCommand(&Cmd, TEXT("STOP")))
		{
			bOverrideView = FALSE;
		}
		else if (ParseCommand(&Cmd, TEXT("SYNC")))
		{
			if (bOverrideView)
			{
				// @todo: with PIE, this maybe be the wrong PlayWorld!
				GWorld->FarMoveActor(Actor->Pawn ? (AActor*)Actor->Pawn : Actor, OverrideLocation, FALSE, TRUE, TRUE);
				Actor->SetRotation(OverrideRotation);
			}
		}
		else
		{
			SetOverrideView(Cmd);
		}
		return TRUE;
	}
#if !FINAL_RELEASE
	else if( ParseCommand(&Cmd,TEXT("FreezeAt")) )
	{
		// e.g. FreezeAt 2819.5520 416.2633 75.1500 65378 -25879 0
		static IConsoleVariable* RenderTimeFrozenVar = GConsoleManager->FindConsoleVariable(TEXT("RenderTimeFrozen")); 
		static IConsoleVariable* FreezeAtPositionVar = GConsoleManager->FindConsoleVariable(TEXT("FreezeAtPosition")); 

		FString StartFreezeAtPosition;

		UBOOL bShowHelp = FALSE;

		if(ParseCommand(&Cmd, TEXT("?")))
		{
			bShowHelp = TRUE;
		}
		else if(*Cmd == 0)
		{
			// toggle on / off
			if(bOverrideView)
			{
				// stop
				bOverrideView = FALSE;
				RenderTimeFrozenVar->Set(0);

				// deactivated as pausing right at startup doesn't work
//				Exec(TEXT("Pause"), *GLog);

				Ar.Logf(TEXT("View and time is no longer frozen."));
			}
			else
			{
				// start
				StartFreezeAtPosition = *FreezeAtPositionVar->GetString();

				if(StartFreezeAtPosition.IsEmpty())
				{
					bShowHelp = TRUE;
				}
			}
		}
		else if(ParseCommand(&Cmd, TEXT("Here")))
		{
			if(bOverrideView)
			{
				Ar.Logf(TEXT("View is currently frozen at \"%s\""), *FreezeAtPositionVar->GetString());
			}
			else
			{
				// print out current pos
				FVector ViewLocation = FVector(0.0f, 0.0f, 0.0f);
				FRotator ViewRotation = FRotator(0, 0, 0);

				Actor->eventGetPlayerViewPoint(ViewLocation, ViewRotation);

				StartFreezeAtPosition = FString::Printf(
					TEXT("%g %g %g %d %d %d"),
					ViewLocation.X, ViewLocation.Y, ViewLocation.Z,
					ViewRotation.Pitch, ViewRotation.Yaw, ViewRotation.Roll);

				Ar.Logf(TEXT("\"FreezeAtPosition = %s\"    was copied to the clipboard"), *StartFreezeAtPosition);
				Ar.Logf(TEXT("This line can be put into the file ConsoleVariables.ini to get frozen on startup."), *StartFreezeAtPosition);

				appClipboardCopy(*(FString(TEXT("FreezeAtPosition = ")) + StartFreezeAtPosition));

				FreezeAtPositionVar->Set(*StartFreezeAtPosition);
			}
		}
		else
		{
			// See if the "FreezeAt <location_x> <location_y> <location_z> <orientation_x> <orientation_y> <orientation_z>" syntax was used
			FString FreezeAtParams = FString::Printf(TEXT("%s"), Cmd);
			if (!ParseToken(Cmd, 0).IsEmpty()		// x
				&& !ParseToken(Cmd, 0).IsEmpty()	// y
				&& !ParseToken(Cmd, 0).IsEmpty()	// z
				&& !ParseToken(Cmd, 0).IsEmpty()	// wx
				&& !ParseToken(Cmd, 0).IsEmpty()	// wy
				&& !ParseToken(Cmd, 0).IsEmpty())	// wz
			{
				StartFreezeAtPosition = FreezeAtParams;
				FreezeAtPositionVar->Set(*StartFreezeAtPosition);
			}
		}

		if(bShowHelp)
		{
			Ar.Logf(TEXT("This console command allows to lock the camera in order to provide more deterministic render profiling.\n"));
			Ar.Logf(TEXT("The view position and rotation is stored in the console variable \"FreezeAtPosition\"."));
			Ar.Logf(TEXT("\"FreezeAt\" without parameters toggle between frozen and not frozen."));
			Ar.Logf(TEXT("\"FreezeAt here\" returns the current position to allow to come back to this position.\n"));
			Ar.Logf(TEXT("\"FreezeAt x y z wx wy wz\" directly freezes at the given position.\n"));
			Ar.Logf(TEXT("See also \"FreezeAtPosition\" CONSOLE variable help.\n"));
			Ar.Logf(TEXT("See also \"RenderTimeFrozen\" CONSOLE variable help (this console variable is modified).\n"));
			Ar.Logf(TEXT("Examples:\n"));
			Ar.Logf(TEXT(" FreezeAt\n"));
			Ar.Logf(TEXT(" FreezeAt here\n"));
			Ar.Logf(TEXT(" FreezeAt 2819.5520 416.2633 75.1500 65378 -25879 0"));
		}

		if(!StartFreezeAtPosition.IsEmpty())
		{
			// start
			if(SetOverrideView(*StartFreezeAtPosition))
			{
				bOverrideView = TRUE;
				RenderTimeFrozenVar->Set(1);
				
				// deactivated as pausing right at startup doesn't work
//				Exec(TEXT("Pause"), *GLog);

				Ar.Logf(TEXT("View and time is now frozen at the position at \"%s\""), *StartFreezeAtPosition);
				Ar.Logf(TEXT("Use \"FreezeAt\" to toggle the feature on/off."));
			}
		}

		return TRUE;
	}
#endif
	// @hack: This is a test matinee skipping function, quick and dirty to see if it's good enough for
	// gameplay. Will fix up better when we have some testing done!
	else if (ParseCommand(&Cmd, TEXT("CANCELMATINEE")))
	{
		UBOOL bMatineeSkipped = FALSE;

		// allow optional parameter for initial time in the matinee that this won't work (ie, 
		// 'cancelmatinee 5' won't do anything in the first 5 seconds of the matinee)
		FLOAT InitialNoSkipTime = appAtof(Cmd);

		// is the player in cinematic mode?
		if (Actor->bCinematicMode)
		{
			// if so, look for all active matinees that has this Player in a director group
			for (TObjectIterator<USeqAct_Interp> It; It; ++It)
			{
				// isit currently playing (and skippable)?
				if (It->bIsPlaying && It->bIsSkippable && (It->bClientSideOnly || GWorld->IsServer()))
				{
					for (INT GroupIndex = 0; GroupIndex < It->GroupInst.Num(); GroupIndex++)
					{
						// is the PC the group actor?
						if (It->GroupInst(GroupIndex)->GetGroupActor() == Actor)
						{
							const FLOAT RightBeforeEndTime = 0.1f;
							// make sure we aren';t already at the end (or before the allowed skip time)
							if ((It->Position < It->InterpData->InterpLength - RightBeforeEndTime) && 
								(It->Position >= InitialNoSkipTime))
							{
								// skip to end
								It->SetPosition(It->InterpData->InterpLength - RightBeforeEndTime, TRUE);

								// send a callback that this was skipped
								GCallbackEvent->Send(CALLBACK_MatineeCanceled, *It);

								bMatineeSkipped = TRUE;

								extern FLOAT HACK_DelayAfterSkip;
								// for 2 seconds after actually skipping a matinee, don't allow savegame loadng
								HACK_DelayAfterSkip = 2.0f;
							}
						}
					}
				}
			}

			if(bMatineeSkipped && GWorld && GWorld->GetGameInfo())
			{
				GWorld->GetGameInfo()->eventMatineeCancelled();
			}
		}
		return TRUE;
	}
	else if(ViewportClient && ViewportClient->Exec(Cmd,Ar))
	{
		return TRUE;
	}
	else if ( Super::Exec( Cmd, Ar ) )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void ULocalPlayer::ExecMacro( const TCHAR* Filename, FOutputDevice& Ar )
{
	// make sure Binaries is specified in the filename
	FString FixedFilename;
	if (!appStristr(Filename, TEXT("Binaries")))
	{
		FixedFilename = FString(TEXT("..\\..\\Binaries\\")) + Filename;
		Filename = *FixedFilename;
	}

	FString Text;
	if (appLoadFileToString(Text, Filename))
	{
		debugf(TEXT("Execing %s"), Filename);
		const TCHAR* Data = *Text;
		FString Line;
		while( ParseLine(&Data, Line) )
		{
			Exec(*Line, Ar);
		}
	}
	else
	{
		Ar.Logf( NAME_ExecWarning, LocalizeSecure(LocalizeError("FileNotFound",TEXT("Core")), Filename) );
	}
}

void FConsoleOutputDevice::Serialize(const TCHAR* Text,EName Event)
{
	FStringOutputDevice::Serialize(Text,Event);
	FStringOutputDevice::Serialize(TEXT("\n"),Event);
	GLog->Serialize(Text,Event);

	if( Console != NULL )
	{
		Console->eventOutputText(Text);
	}
}

/** This will set the StreamingLevels TMap with the current Streaming Level Status and also set which level the player is in **/
void GetLevelStremingStatus( TMap<FName,INT>& StreamingLevels, FString& LevelPlayerIsInName )
{
	// Iterate over the world info's level streaming objects to find and see whether levels are loaded, visible or neither.
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

	for( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
	{
		ULevelStreaming* LevelStreaming = WorldInfo->StreamingLevels(LevelIndex);

		ULevelStreamingAlwaysLoaded* AlwaysLoadedLevel = Cast<ULevelStreamingAlwaysLoaded>(LevelStreaming);

		if( AlwaysLoadedLevel != NULL )
		{
			if( AlwaysLoadedLevel->bIsProceduralBuildingLODLevel == TRUE )
			{
				//warnf( TEXT( "GetLevelStremingStatus skipping %s" ), *AlwaysLoadedLevel->GetFullName() );
				continue;
			}
			else
			{
				//warnf( TEXT( "GetLevelStremingStatus NOT SKIPPING when it should %s" ), *AlwaysLoadedLevel->GetFullName() );
				//continue
			}	
		}


		if( LevelStreaming 
			&&  LevelStreaming->PackageName != NAME_None 
			&&	LevelStreaming->PackageName != GWorld->GetOutermost()->GetFName() )
		{
			if( LevelStreaming->LoadedLevel && !LevelStreaming->bHasUnloadRequestPending )
			{
				if( GWorld->Levels.FindItemIndex( LevelStreaming->LoadedLevel ) != INDEX_NONE )
				{
					if( LevelStreaming->LoadedLevel->bHasVisibilityRequestPending )
					{
						StreamingLevels.Set( LevelStreaming->PackageName, LEVEL_MakingVisible );
					}
					else
					{
						StreamingLevels.Set( LevelStreaming->PackageName, LEVEL_Visible );
					}
				}
				else
				{
					StreamingLevels.Set( LevelStreaming->PackageName, LEVEL_Loaded );
				}
			}
			else
			{
				// See whether the level's world object is still around.
				UPackage* LevelPackage	= Cast<UPackage>(UGameViewportClient::StaticFindObjectFast( UPackage::StaticClass(), NULL, LevelStreaming->PackageName ));
				UWorld*	  LevelWorld	= NULL;
				if( LevelPackage )
				{
					LevelWorld = Cast<UWorld>(UGameViewportClient::StaticFindObjectFast( UWorld::StaticClass(), LevelPackage, NAME_TheWorld ));
				}

				if( LevelWorld )
				{
					StreamingLevels.Set( LevelStreaming->PackageName, LEVEL_UnloadedButStillAround );
				}
				else if( UObject::GetAsyncLoadPercentage( *LevelStreaming->PackageName.ToString() ) >= 0 )
				{
					StreamingLevels.Set( LevelStreaming->PackageName, LEVEL_Loading );
				}
				else
				{
					StreamingLevels.Set( LevelStreaming->PackageName, LEVEL_Unloaded );
				}
			}
		}
	}

	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (GameEngine != NULL)
	{
		// toss in the levels being loaded by PrepareMapChange
		for( INT LevelIndex=0; LevelIndex < GameEngine->LevelsToLoadForPendingMapChange.Num(); LevelIndex++ )
		{
			const FName LevelName = GameEngine->LevelsToLoadForPendingMapChange(LevelIndex);
			StreamingLevels.Set(LevelName, LEVEL_Preloading);
		}
	}


	ULevel* LevelPlayerIsIn = NULL;

	for( AController* Controller = GWorld->GetWorldInfo()->ControllerList; 
		Controller != NULL; 
		Controller = Controller->NextController
		)
	{
		APlayerController* PC = Cast<APlayerController>( Controller );

		if( ( PC != NULL )
			&&( PC->Pawn != NULL )
			)
		{
			// need to do a trace down here
			//TraceActor = Trace( out_HitLocation, out_HitNormal, TraceDest, TraceStart, false, TraceExtent, HitInfo, true );
			FCheckResult Hit(1.0f);
			DWORD TraceFlags;
			TraceFlags = TRACE_World;

			FVector TraceExtent(0,0,0);

			// this will not work for flying around :-(
			GWorld->SingleLineCheck( Hit, PC->Pawn, (PC->Pawn->Location-FVector(0, 0, 256 )), PC->Pawn->Location, TraceFlags, TraceExtent );

			if( Hit.Level != NULL )
			{
				LevelPlayerIsIn = Hit.Level;
			}
			else if( Hit.Actor != NULL )
			{
				LevelPlayerIsIn = Hit.Actor->GetLevel();
			}
			else if( Hit.Component != NULL )
			{
				LevelPlayerIsIn = Hit.Component->GetOwner()->GetLevel();
			}
		}
	}

	// this no longer seems to be getting the correct level name :-(
	LevelPlayerIsInName = LevelPlayerIsIn != NULL ? LevelPlayerIsIn->GetOutermost()->GetName() : TEXT("None");
}

void APlayerController::LogOutBugItGoToLogFile( const FString& InScreenShotDesc, const FString& InGoString, const FString& InLocString )
{
#if ALLOW_DEBUG_FILES
	// Create folder if not already there

	const FString OutputDir = appScreenShotDir();

	GFileManager->MakeDirectory( *OutputDir );
	// Create archive for log data.
	// we have to +1 on the GScreenshotBitmapIndex as it will be incremented by the bugitscreenshot which is processed next tick

	const FString DescPlusExtension = FString::Printf( TEXT("%s%i.txt"), *InScreenShotDesc, GScreenshotBitmapIndex );
	const FString TxtFileName = CreateProfileFilename( DescPlusExtension, FALSE );

	//FString::Printf( TEXT("BugIt%i-%s%05i"), GBuiltFromChangeList, *InScreenShotDesc, GScreenshotBitmapIndex+1 ) + TEXT( ".txt" );
	const FString FullFileName = OutputDir + TxtFileName;

	FOutputDeviceFile OutputFile(*FullFileName);
	//FArchive* OutputFile = GFileManager->CreateDebugFileWriter( *(FullFileName), FILEWRITE_Append );


	OutputFile.Logf( TEXT("Dumping BugIt data chart at %s using build %i built from changelist %i"), *appSystemTimeString(), GEngineVersion, GetChangeListNumberForPerfTesting() );

	extern const FString GetMapNameStatic();
	const FString MapNameStr = GetMapNameStatic();

	OutputFile.Logf( TEXT("MapName: %s"), *MapNameStr );

	OutputFile.Logf( TEXT("Description: %s"), *InScreenShotDesc );
	OutputFile.Logf( TEXT("%s"), *InGoString );
	OutputFile.Logf( TEXT("%s"), *InLocString );

	OutputFile.Logf( TEXT(" ---=== GameSpecificData ===--- ") );
	// can add some other more detailed info here
	GEngine->Exec( TEXT( "GAMESPECIFIC_BUGIT" ), OutputFile );

	// Flush, close and delete.
	//delete OutputFile;
	OutputFile.TearDown();

	// so here we want to send this bad boy back to the PC
	SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!BUGIT:"), *(FullFileName) );


	

#endif // ALLOW_DEBUG_FILES
}

void APlayerController::LogOutBugItAIGoToLogFile( const FString& InScreenShotDesc, const FString& InGoString, const FString& InLocString )
{
#if ALLOW_DEBUG_FILES
	extern const FString GetMapNameStatic();
	const FString MapNameStr = GetMapNameStatic();
	const FString PlatformStr = FString(
#if PS3
		TEXT("PS3")
#elif XBOX
		TEXT("Xe")
#else
		TEXT("PC")
#endif // PS3
		);

	FString FinalDir = FString::Printf( TEXT("%s-%s-%i"), *MapNameStr, *PlatformStr, GetChangeListNumberForPerfTesting() );
	FString SubDir = 
	FinalDir = FinalDir.Right(42);
	FinalDir = appScreenShotDir() + FinalDir +  PATH_SEPARATOR + FString::Printf(TEXT("BugItAI-%s%i"),*InScreenShotDesc,GScreenshotBitmapIndex) + PATH_SEPARATOR;

	//debugf(TEXT("%s"),*FinalDir);

	// Create folder if not already there
	GFileManager->MakeDirectory( *FinalDir );


	const FString TxtFileName = FString::Printf( TEXT("BugitAI-%s%i.txt"), *InScreenShotDesc, GScreenshotBitmapIndex );


	TCHAR File[MAX_SPRINTF] = TEXT("");
	for( INT TestBitmapIndex = 0; TestBitmapIndex < 9; ++TestBitmapIndex )
	{ 
		FString BmpFileName = FString::Printf( TEXT("BugitAI-%s%i.bmp"), *InScreenShotDesc, TestBitmapIndex );
		BmpFileName = BmpFileName.Right(42);
		//warnf( TEXT( "BugIt Looking: %s" ), *(FinalDir + BmpFileName) );
		appSprintf( File, TEXT("%s"), *(FinalDir + BmpFileName) );
		if( GFileManager->FileSize(File) == INDEX_NONE )
		{
			GScreenshotBitmapIndex = TestBitmapIndex; // this is safe as the UnMisc.cpp ScreenShot code will test each number before writing a file
			FString SSFull = FinalDir + BmpFileName;
			warnf( TEXT( "BugItAI ScreenShot: %s" ), *SSFull );

			// make relative to SS dir
			SSFull = SSFull.Replace(*(appScreenShotDir()),TEXT(""),TRUE);
			GScreenShotName = SSFull; 
			GScreenShotRequest = TRUE;
			break;
		}
	}
	//FString::Printf( TEXT("BugIt%i-%s%05i"), GBuiltFromChangeList, *InScreenShotDesc, GScreenshotBitmapIndex+1 ) + TEXT( ".txt" );
	const FString FullFileName = FinalDir + TxtFileName;
	
	// Create archive for log data.
	FOutputDeviceFile OutputFile(*FullFileName);
	//FArchive* OutputFile = GFileManager->CreateDebugFileWriter( *(FullFileName), FILEWRITE_Append );


	OutputFile.Logf( TEXT("Dumping BugItAI data chart at %s using build %i built from changelist %i"), *appSystemTimeString(), GEngineVersion, GetChangeListNumberForPerfTesting() );
	OutputFile.Logf( TEXT("MapName: %s"), *MapNameStr );

	OutputFile.Logf( TEXT("Description: %s"), *InScreenShotDesc );
	OutputFile.Logf( TEXT("%s"), *InGoString );
	OutputFile.Logf( TEXT("%s"), *InLocString );

	OutputFile.Logf( TEXT(" ---=== GameSpecificData ===--- ") );
	// can add some other more detailed info here
	GEngine->Exec( *FString::Printf( TEXT("GAMESPECIFIC_BUGITAI %s" ), *FinalDir), OutputFile );

	// Flush, close and delete.
	//delete OutputFile;
	OutputFile.TearDown();

	// so here we want to send this bad boy back to the PC
	SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!BUGIT:"), *(FullFileName) );




#endif // ALLOW_DEBUG_FILES
}


