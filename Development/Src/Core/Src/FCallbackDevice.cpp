/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Core includes.
#include "CorePrivate.h"

/**
 * Macro for wrapping implmentations of TScopedCallback'd ECallbackEventType
 */
#define IMPLEMENT_SCOPED_CALLBACK( CallbackName )						\
	void FScoped##CallbackName##Impl::FireCallback()					\
	{																	\
		GCallbackEvent->Send( CALLBACK_##CallbackName );				\
	}

IMPLEMENT_SCOPED_CALLBACK( None );

IMPLEMENT_SCOPED_CALLBACK( SelChange );
IMPLEMENT_SCOPED_CALLBACK( MapChange );
IMPLEMENT_SCOPED_CALLBACK( LayerChange );
IMPLEMENT_SCOPED_CALLBACK( WorldChange );
IMPLEMENT_SCOPED_CALLBACK( LevelDirtied );
IMPLEMENT_SCOPED_CALLBACK( SurfProps );
IMPLEMENT_SCOPED_CALLBACK( FitTextureToSurface );
IMPLEMENT_SCOPED_CALLBACK( ChangeEditorMode );
IMPLEMENT_SCOPED_CALLBACK( RedrawAllViewports );
IMPLEMENT_SCOPED_CALLBACK( ActorPropertiesChange );

IMPLEMENT_SCOPED_CALLBACK( RefreshEditor );
IMPLEMENT_SCOPED_CALLBACK( LoadSelectedAssetsIfNeeded );

IMPLEMENT_SCOPED_CALLBACK( RefreshEditor_AllBrowsers );
IMPLEMENT_SCOPED_CALLBACK( RefreshEditor_LevelBrowser );
IMPLEMENT_SCOPED_CALLBACK( RefreshEditor_LayerBrowser );
IMPLEMENT_SCOPED_CALLBACK( RefreshEditor_ActorBrowser );
IMPLEMENT_SCOPED_CALLBACK( RefreshEditor_TerrainBrowser );
IMPLEMENT_SCOPED_CALLBACK( RefreshEditor_SceneManager );
IMPLEMENT_SCOPED_CALLBACK( RefreshEditor_Kismet );

IMPLEMENT_SCOPED_CALLBACK( DisplayLoadErrors );
IMPLEMENT_SCOPED_CALLBACK( EditorModeEnter );
IMPLEMENT_SCOPED_CALLBACK( EditorModeExit );
IMPLEMENT_SCOPED_CALLBACK( UpdateUI );
IMPLEMENT_SCOPED_CALLBACK( SelectObject );
IMPLEMENT_SCOPED_CALLBACK( ShowDockableWindow );
IMPLEMENT_SCOPED_CALLBACK( Undo );
IMPLEMENT_SCOPED_CALLBACK( PreWindowsMessage );
IMPLEMENT_SCOPED_CALLBACK( PostWindowsMessage );
IMPLEMENT_SCOPED_CALLBACK( RedirectorFollowed );

IMPLEMENT_SCOPED_CALLBACK( ObjectPropertyChanged );
IMPLEMENT_SCOPED_CALLBACK( EndPIE );
IMPLEMENT_SCOPED_CALLBACK( RefreshPropertyWindows );

IMPLEMENT_SCOPED_CALLBACK( UIEditor_StyleModified );
IMPLEMENT_SCOPED_CALLBACK( UIEditor_ModifiedRenderOrder );
IMPLEMENT_SCOPED_CALLBACK( UIEditor_RefreshScenePositions );

const FString DefaultRefString;
const FVector DefaultRefVector(0);

/**
 * Constructor - callers wishing to use additional parameters should set those directly prior to calling Send()
 */
FCallbackEventParameters::FCallbackEventParameters( FCallbackEventDevice* InSender, ECallbackEventType InType )
: Sender(InSender), EventType(InType), EventFlag(0), EventMessage(0), EventString(DefaultRefString)
, EventVector(DefaultRefVector), EventEditorMode(NULL), EventObject(NULL), EventViewport(NULL)
{
}
FCallbackEventParameters::FCallbackEventParameters( FCallbackEventDevice* InSender, ECallbackEventType InType, const FString& InString )
: Sender(InSender), EventType(InType), EventFlag(0), EventMessage(0), EventString(InString)
, EventVector(DefaultRefVector), EventEditorMode(NULL), EventObject(NULL), EventViewport(NULL)
{
}
FCallbackEventParameters::FCallbackEventParameters( FCallbackEventDevice* InSender, ECallbackEventType InType, const FVector& InVector )
: Sender(InSender), EventType(InType), EventFlag(0), EventMessage(0), EventString(DefaultRefString)
, EventVector(InVector), EventEditorMode(NULL), EventObject(NULL), EventViewport(NULL)
{
}
FCallbackEventParameters::FCallbackEventParameters( FCallbackEventDevice* InSender, ECallbackEventType InType, UObject* InEventObj )
: Sender(InSender), EventType(InType), EventFlag(0), EventMessage(0), EventString(DefaultRefString)
, EventVector(DefaultRefVector), EventEditorMode(NULL), EventObject(InEventObj), EventViewport(NULL)
{
}
FCallbackEventParameters::FCallbackEventParameters( FCallbackEventDevice* InSender, ECallbackEventType InType, DWORD InEventFlag )
: Sender(InSender), EventType(InType), EventFlag(InEventFlag), EventMessage(0), EventString(DefaultRefString)
, EventVector(DefaultRefVector), EventEditorMode(NULL), EventObject(NULL), EventViewport(NULL)
{
}
FCallbackEventParameters::FCallbackEventParameters( FCallbackEventDevice* InSender, ECallbackEventType InType, DWORD InEventFlag, UObject* InEventObj )
: Sender(InSender), EventType(InType), EventFlag(InEventFlag), EventMessage(0), EventString(DefaultRefString)
, EventVector(DefaultRefVector), EventEditorMode(NULL), EventObject(InEventObj), EventViewport(NULL)
{
}
FCallbackEventParameters::FCallbackEventParameters( FCallbackEventDevice* InSender, ECallbackEventType InType, DWORD InEventFlag, UObject* InEventObj, const FString& InEventString )
: Sender(InSender), EventType(InType), EventFlag(InEventFlag), EventMessage(0), EventString(InEventString)
, EventVector(DefaultRefVector), EventEditorMode(NULL), EventObject(InEventObj), EventViewport(NULL)
{
}

/**
 * Constructor - callers wishing to use additional parameters should set those directly prior to calling Query()
 */
FCallbackQueryParameters::FCallbackQueryParameters( FCallbackQueryDevice* InSender, ECallbackQueryType InType )
: Sender(InSender), QueryType(InType), QueryObject(NULL), QueryString(DefaultRefString)
{
}

FCallbackQueryParameters::FCallbackQueryParameters( FCallbackQueryDevice* InSender, ECallbackQueryType InType, const FString& InString )
: Sender(InSender), QueryType(InType), QueryObject(NULL), QueryString(InString)
{
}

/**
 * Destructor - Unregister self from all events upon destruction
 */
FCallbackEventDevice::~FCallbackEventDevice()
{
	if( !GIsRequestingExit && GCallbackEvent )
	{
		GCallbackEvent->UnregisterAll(this);
	}
}


