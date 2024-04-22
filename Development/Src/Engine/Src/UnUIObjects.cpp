/*=============================================================================
	UnUIObjects.cpp: UI widget class implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "EngineSequenceClasses.h"
#include "ScopedObjectStateChange.h"
#include "UnUIKeys.h"

// Registration
IMPLEMENT_CLASS(UCustomPropertyItemHandler);

//DECLARE_CYCLE_STAT(TEXT("RefreshStyles Time"),STAT_UIRefreshWidgetStyles,STATGROUP_UI);
DECLARE_CYCLE_STAT(TEXT("RebuildNavigationLinks Time"),STAT_UIRebuildNavigationLinks,STATGROUP_UI);
DECLARE_CYCLE_STAT(TEXT("Resolve Positions Time"),STAT_UIResolveScenePositions,STATGROUP_UI);
DECLARE_CYCLE_STAT(TEXT("RebuildDockingStack Time"),STAT_UIRebuildDockingStack,STATGROUP_UI);
DECLARE_CYCLE_STAT(TEXT("UpdateScene Time"),STAT_UISceneUpdateTime,STATGROUP_UI);

#if SUPPORTS_DEBUG_LOGGING
INT FScopedDebugLogger::DebugIndent	=	0;
INT FocusDebugIndent				=	1;

#define DEBUG_UIINPUT	1
#endif

#if DEBUG_UIINPUT
	#define debugInputResultf	if ( bResult ) debugf
	#define debugInputf			debugf
#else
	#define debugInputResultf	debugfSuppressed
	#define debugInputf			debugfSuppressed
#endif

/**
 * Returns the friendly name for the specified input event from the EInputEvent enum.
 *
 * @return	the textual representation of the enum member specified, or "Unknown" if the value is invalid.
 */
FString UUIRoot::GetInputEventText( BYTE InputEvent )
{
	static UEnum* InputEventEnum = FindObject<UEnum>(UObject::StaticClass(), TEXT("EInputEvent"), TRUE);
	if ( InputEventEnum != NULL && InputEvent <= IE_MAX )
	{
		return InputEventEnum->GetEnum(InputEvent).ToString();
	}

	return TEXT("Unknown");
}

/**
 * Returns the friendly name for the specified platform type from the EInputPlatformType enum.
 *
 * @return	the textual representation of the enum member specified, or "Unknown" if the value is invalid.
 */
FString UUIRoot::GetInputPlatformTypeText( BYTE PlatformType )
{
	static UEnum* InputPlatformTypeEnum = FindField<UEnum>(UUIRoot::StaticClass(), TEXT("EInputPlatformType"));
	if ( InputPlatformTypeEnum != NULL && PlatformType <= IPT_MAX )
	{
		return InputPlatformTypeEnum->GetEnum(PlatformType).ToString();
	}

	return TEXT("Unknown");
}

/**
 * Returns the UIController class set for this game.
 *
 * @return	a pointer to a UIInteraction class which is set as the value for GameViewportClient.UIControllerClass.
 */
UClass* UUIRoot::GetUIControllerClass()
{
	UClass* GameViewportClass = GEngine->GameViewportClientClass;
	check(GameViewportClass);

	// first, find the GameViewportClient class configured for this game
	UGameViewportClient* DefaultGameViewport = GameViewportClass->GetDefaultObject<UGameViewportClient>();
	if ( DefaultGameViewport == NULL )
	{
		// if the configured GameViewportClient class couldn't be loaded, fallback to the base class
		DefaultGameViewport = UGameViewportClient::StaticClass()->GetDefaultObject<UGameViewportClient>();
	}
	check(DefaultGameViewport);

	// now get the UIInteraction class from the configured GameViewportClient
	return DefaultGameViewport->UIControllerClass;
}

/**
 * Returns the default object for the UIController class set for this game.
 *
 * @return	a pointer to the CDO for UIInteraction class configured for this game.
 */
UUIInteraction* UUIRoot::GetDefaultUIController()
{
	UClass* UIControllerClass = GetUIControllerClass();
	check(UIControllerClass);

	UUIInteraction* DefaultUIController = UIControllerClass->GetDefaultObject<UUIInteraction>();
	if ( DefaultUIController == NULL )
	{
		DefaultUIController = UUIInteraction::StaticClass()->GetDefaultObject<UUIInteraction>();
	}
	check(DefaultUIController);

	return DefaultUIController;
}
/**
 * Returns the UIInteraction instance currently controlling the UI system, which is valid in game.
 *
 * @return	a pointer to the UIInteraction object currently controlling the UI system.
 */
UUIInteraction* UUIRoot::GetCurrentUIController()
{
	UUIInteraction* UIController = NULL;
	if ( GEngine != NULL && GEngine->GameViewport != NULL )
	{
		UIController = GEngine->GameViewport->UIController;
	}

	return UIController;
}

/**
 * Returns the game's scene client.
 *
 * @return 	a pointer to the UGameUISceneClient instance currently managing the scenes for the UI System.
 */
UGameUISceneClient* UUIRoot::GetSceneClient()
{
	UGameUISceneClient* SceneClient = NULL;

	UUIInteraction* CurrentUIController = GetCurrentUIController();
	if ( CurrentUIController != NULL )
	{
		SceneClient = CurrentUIController->SceneClient;
	}

	return SceneClient;
}

/**
 * Returns the platform type for the current input device.  This is not necessarily the platform the game is actually running
 * on; for example, if the game is running on a PC, but the player is using an Xbox controller, the current InputPlatformType
 * would be IPT_360.
 *
 * @param	OwningPlayer	if specified, the returned InputPlatformType will reflect the actual input device the player
 *							is using.  Otherwise, the returned InputPlatformType is always the platform the game is running on.
 *
 * @return	the platform type for the current input device (if a player is specified) or the host platform.
 */
EInputPlatformType UUIRoot::GetInputPlatformType( ULocalPlayer* OwningPlayer/*=NULL*/ )
{
#if XBOX
	EInputPlatformType Platform = IPT_360;
	if ( OwningPlayer != NULL && OwningPlayer->Actor != NULL && OwningPlayer->Actor->PlayerInput != NULL )
	{
		// if the player is using a keyboard / mouse, the input type is PC
		//@todo ronp - determine whether kb/m is active
	}
#elif PS3
	EInputPlatformType Platform = IPT_PS3;
	if ( OwningPlayer != NULL && OwningPlayer->Actor != NULL && OwningPlayer->Actor->PlayerInput != NULL )
	{
		// if the player is using a keyboard / mouse, the input type is PC
		//@todo ronp - determine whether kb/m is active
	}
#else
	EInputPlatformType Platform = IPT_PC;
	if ( OwningPlayer != NULL && OwningPlayer->Actor != NULL && OwningPlayer->Actor->PlayerInput != NULL )
	{
		//@todo ronp - what about using a ps3 gamepad?
		if ( OwningPlayer->Actor->PlayerInput->bUsingGamepad )
		{
			Platform = IPT_360;
		}
	}
#endif

	return Platform;
}


// exec wrappers
void UUIRoot::execGetCurrentUIController( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(UUIInteraction**)Result=GetCurrentUIController();
}
void UUIRoot::execGetSceneClient( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(UGameUISceneClient**)Result=GetSceneClient();
}

void UUIRoot::execGetInputPlatformType( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT_OPTX(ULocalPlayer,OwningPlayer,NULL);
	P_FINISH;
	*(BYTE*)Result=GetInputPlatformType(OwningPlayer);
}

// EOF




