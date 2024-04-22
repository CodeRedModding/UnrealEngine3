/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
/*=============================================================================
	FCallbackDevice.h:  Allows the engine to do callbacks into the editor
=============================================================================*/

#ifndef _FCALLBACKDEVICE_H_
#define _FCALLBACKDEVICE_H_

#include "ScopedCallback.h"

/**
 * The list of events that can be fired from the engine to unknown listeners
 */
enum ECallbackEventType
{
	CALLBACK_None,
	CALLBACK_SelChange,
	CALLBACK_MapChange,
	// Called when an actor is added to a layer
	CALLBACK_LayerChange,
	// Called when the world is modified
	CALLBACK_WorldChange,
	/** Called when a level package has been dirtied. */
	CALLBACK_LevelDirtied,

	/** Called when the CurrentLevel is switched to a new level.  Note that this event won't be fired for temporary
	    changes to the current level, such as when copying/pasting actors. */
	CALLBACK_NewCurrentLevel,

	CALLBACK_SurfProps,
	CALLBACK_SelectedProps,									// Sent when requesting to display the properties of selected actors or BSP surfaces
	
	/** Fits the currently assigned texture to the selected surfaces */
	CALLBACK_FitTextureToSurface,

	CALLBACK_PreEditorClose,								// Sent when the editor is about to close down

	CALLBACK_ChangeEditorMode,
	CALLBACK_RedrawAllViewports,
	CALLBACK_ActorPropertiesChange,
	CALLBACK_RefreshEditor,
	//@todo -- Add a better way of doing these
	CALLBACK_RefreshEditor_AllBrowsers,
	CALLBACK_RefreshEditor_LevelBrowser,
	CALLBACK_RefreshEditor_LayerBrowser,
	CALLBACK_RefreshEditor_ActorBrowser,
	CALLBACK_RefreshEditor_TerrainBrowser,
	CALLBACK_RefreshEditor_PrimitiveStatsBrowser,
	CALLBACK_RefreshEditor_SceneManager,
	CALLBACK_RefreshEditor_Kismet,
	//@todo end
	CALLBACK_RefreshContentBrowser,							// Sent when events occur which invalidates some portion of the content browser's data
	CALLBACK_LoadSelectedAssetsIfNeeded,					// Called when an action is performed which interacts with the content browser; load any selected assets which aren't already loaded
	CALLBACK_DisplayLoadErrors,
	CALLBACK_EditorModeEnter,
	CALLBACK_EditorModeExit,
	CALLBACK_UpdateUI,										// Sent when events happen that affect how the editors UI looks (mode changes, grid size changes, etc)
	CALLBACK_SelectObject,									// An object has been selected (generally an actor)
	CALLBACK_SelectNone,									// deselect all objects
	CALLBACK_ShowDockableWindow,							// Brings a specific dockable window to the front
	CALLBACK_Undo,											// Sent after an undo/redo operation takes place
	CALLBACK_PreWindowsMessage,								// Sent before the given windows message is handled in the given viewport
	CALLBACK_PostWindowsMessage,							// Sent afer the given windows message is handled in the given viewport
	CALLBACK_RedirectorFollowed,							// Sent when a UObjectRedirector was followed to find the destination object
	CALLBACK_ObjectPropertyChanged,							// Sent when a property value has changed
	CALLBACK_ForcePropertyWindowRebuild,					// Sent to f
	CALLBACK_EndPIE,										// Sent when a PIE session is ending (may not be needed when PlayLvel.cpp is moved to UnrealEd)
	CALLBACK_RefreshPropertyWindows,						// Refresh property windows w/o creating/destroying controls
	CALLBACK_DeferredRefreshPropertyWindows,				// Refresh property windows that have been marked for deferred refreshing
	CALLBACK_UIEditor_StyleModified,						// Sent when a style has been modified, used to communicate changes in style editor to  widgets in a scene and also the UISkin
	CALLBACK_UIEditor_PropertyModified,						// Sent when individual property of a style has been modified, used to communicate changes between property panels and style editor 
	CALLBACK_UIEditor_ModifiedRenderOrder,					// Send when the render order or widget hierarchy has been modified
	CALLBACK_UIEditor_RefreshScenePositions,				// Sent whenever the scene updates the positions of its widgets
	CALLBACK_UIEditor_SceneRenamed,							// Sent when the scene has been renamed
	CALLBACK_UIEditor_WidgetUIKeyBindingChanged,			// Sent when a widget's input alias mapping has been updated
	CALLBACK_UIEditor_SkinLoaded,							// Send when a UISkin is loaded from an archive.
	CALLBACK_UIEditor_PreviewPlatformChanged,				// Send when the user changes the platform used for previewing UI scenes
	CALLBACK_PreViewportResized,							// Send when the viewport is about to resize, allowing custom systems to release their references
	CALLBACK_ViewportResized,								// Send when a viewport is resized
	CALLBACK_PackageSaved,									// Sent at the end of SavePackage (ie, only on successful save)
	CALLBACK_LevelAddedToWorld,								// Sent when a ULevel is added to the world via UWorld::AddToWorld
	CALLBACK_LevelRemovedFromWorld,							// Sent when a ULevel is removed from the world via UWorld::RemoveFromWorld or LoadMap (a NULL object means the LoadMap case, because all levels will be removed from the world without a RemoveFromWorld call for each)
	CALLBACK_PreLoadMap,									// Sent at the very beginning of LoadMap
	CALLBACK_PostLoadMap,									// Sent at the _successful_ end of LoadMap
	CALLBACK_MatineeCanceled,								// Sent when CANCELMATINEE exec is processed and actually skips a matinee
	CALLBACK_WorldTickFinished,								// Sent when InTick is set to FALSE at the end of UWorld::Tick
	CALLBACK_TexturePreSave,								// Sent before we attempt to save a texture

	/** Called just after engine shutdown is initiated, before anything gets purged
	 *  No parameters.
	 */
	CALLBACK_PreEngineShutdown,

	/** Called before garbage is collected.  No parameters. */
	CALLBACK_PreGarbageCollection,

	/** Called when the editor is cleansing of transient references before a map change event */
	CALLBACK_CleanseEditor,

	/** Called when actor layers have been modified */
	CALLBACK_LayersHaveChanged,

	/** Called when a procuderal building is updated, to allow code in editor to do things to it */
	CALLBACK_ProcBuildingUpdate,
	
	/** Look for and fix problems with building LOD associations */
	CALLBACK_ProcBuildingFixLODs,

	/** Called before the editor displays a modal window, allowing other windows the opportunity to
		disable themselves to avoid reentrant calls */
	CALLBACK_EditorPreModal,

	/** Called after the editor dismisses a modal window, allowing other windows the opportunity to
		disable themselves to avoid reentrant calls */
	CALLBACK_EditorPostModal,

	/** Called for a material after the user has change a texture's compression settings.
		Needed to notify the material editors that the need to reattach their preview objects */
	CALLBACK_MaterialTextureSettingsChanged,

	/** Called when a material/MaterialInstance is updated, and a new flattened texture needs to be rendered out */
	CALLBACK_MobileFlattenedTextureUpdate,
	
	/** Called when a package has just been marked dirty.
		Allows the editor to register the modified package as one that should be prompted for source control checkout. */
	CALLBACK_PackageModified,

	/** Called in the editor after an actor has been created or moved.  Passes the AActor object.  */
	CALLBACK_OnActorMoved,

	/**
	 * Called to force the editor to update levels for all actors in level grid volumes (next tick.)
	 * No Parameters.
	 */
	CALLBACK_UpdateLevelsForAllActors,

	/** Called when the supplied StaticMeshActor should be tested to see if it can find and attach to the ProcBuilding */
	CALLBACK_BaseSMActorOnProcBuilding,

	/** Called when a file change has been detected */
	CALLBACK_FileChanged,

	/**Color picker color has changed, please refresh as needed*/
	CALLBACK_ColorPickerChanged,

	/**Within a property window, the currently selected item was changed.*/
	CALLBACK_PropertySelectionChange,

	/** Save package ShaderCache cleanup callback. */
	CALLBACK_PackageSaveCleanupShaderCache, 

	/**ShowFlags changed*/
	CALLBACK_ShowFlagsChanged,

	/**ViewportClient was invalidated*/
	CALLBACK_ViewportClientInvalidated,

	/** Called right before unit testing is about to begin */
	CALLBACK_PreUnitTesting,

	/** Called after all unit tests have completed */
	CALLBACK_PostUnitTesting,

	/** Called after Landscape layer infomap update have completed */
	CALLBACK_PostLandscapeLayerUpdated,

	/** Called before and after SaveWorld is processed */
	CALLBACK_PreSaveWorld,
	CALLBACK_PostSaveWorld,

	/**Called after a texture has been modified*/
	CALLBACK_TextureModified,

	/** Called when stereo 3D rendering is toggled on or off */
	CALLBACK_Stereo3DToggled,
	
	// The following MUST be the last one!
	CALLBACK_EventCount,
};

/**
 * Macro for wrapping ECallbackEventType in TScopedCallback
 */
#define DECLARE_SCOPED_CALLBACK( CallbackName )										\
	class FScoped##CallbackName##Impl												\
	{																				\
	public:																			\
		static void FireCallback();													\
	};																				\
																					\
	typedef TScopedCallback<FScoped##CallbackName##Impl> FScoped##CallbackName;

DECLARE_SCOPED_CALLBACK( None );

DECLARE_SCOPED_CALLBACK( SelChange );
DECLARE_SCOPED_CALLBACK( MapChange );
DECLARE_SCOPED_CALLBACK( LayerChange );
DECLARE_SCOPED_CALLBACK( WorldChange );
DECLARE_SCOPED_CALLBACK( LevelDirtied );
DECLARE_SCOPED_CALLBACK( SurfProps );
DECLARE_SCOPED_CALLBACK( ActorProps );
DECLARE_SCOPED_CALLBACK( SelectedProps );
DECLARE_SCOPED_CALLBACK( FitTextureToSurface );
DECLARE_SCOPED_CALLBACK( ChangeEditorMode );
DECLARE_SCOPED_CALLBACK( RedrawAllViewports );
DECLARE_SCOPED_CALLBACK( ActorPropertiesChange );

DECLARE_SCOPED_CALLBACK( RefreshEditor );

DECLARE_SCOPED_CALLBACK( RefreshEditor_AllBrowsers );
DECLARE_SCOPED_CALLBACK( RefreshEditor_LevelBrowser );
DECLARE_SCOPED_CALLBACK( RefreshEditor_LayerBrowser );
DECLARE_SCOPED_CALLBACK( RefreshEditor_ActorBrowser );
DECLARE_SCOPED_CALLBACK( RefreshEditor_TerrainBrowser );
DECLARE_SCOPED_CALLBACK( RefreshEditor_PrimitiveStatsBrowser );
DECLARE_SCOPED_CALLBACK( RefreshEditor_SceneManager );
DECLARE_SCOPED_CALLBACK( RefreshEditor_Kismet );

DECLARE_SCOPED_CALLBACK( RefreshContentBrowser );
DECLARE_SCOPED_CALLBACK( LoadSelectedAssetsIfNeeded );

DECLARE_SCOPED_CALLBACK( DisplayLoadErrors );
DECLARE_SCOPED_CALLBACK( EditorModeEnter );
DECLARE_SCOPED_CALLBACK( EditorModeExit );
DECLARE_SCOPED_CALLBACK( ModalErrorMessage );

DECLARE_SCOPED_CALLBACK( UpdateUI );
DECLARE_SCOPED_CALLBACK( SelectObject );
DECLARE_SCOPED_CALLBACK( ShowDockableWindow );
DECLARE_SCOPED_CALLBACK( Undo );
DECLARE_SCOPED_CALLBACK( PreWindowsMessage );
DECLARE_SCOPED_CALLBACK( PostWindowsMessage );
DECLARE_SCOPED_CALLBACK( RedirectorFollowed );

DECLARE_SCOPED_CALLBACK( ObjectPropertyChanged );
DECLARE_SCOPED_CALLBACK( EndPIE );
DECLARE_SCOPED_CALLBACK( RefreshPropertyWindows );

DECLARE_SCOPED_CALLBACK( UIEditor_StyleModified );
DECLARE_SCOPED_CALLBACK( UIEditor_ModifiedRenderOrder );
DECLARE_SCOPED_CALLBACK( UIEditor_RefreshScenePositions );

/**
 * The list of events that can be fired from the engine to unknown listeners
 */
enum ECallbackQueryType
{
	CALLBACK_LoadObjectsOnTop,				// A query sent to find out if objects in a the given filename should have loaded object replace them or not
	CALLBACK_AllowPackageSave,				// A query sent to determine whether saving a package is allowed.
	CALLBACK_QuerySelectedAssets,			// A query sent to retrieve a delimited string containing class and pathnames of assets selected in the generic/content browser
	CALLBACK_ModalErrorMessage,				//Sent when appMsgf or appDebugMesgf need to display a modal
	CALLBACK_LocalizationExportFilter,		// A query sent to find out if the provided object passes the localization export filter
	CALLBACK_DoReferenceChecks,				// A query sent to find out if during map checks slow reference checks should be done
	CALLBACK_QueryCount,
};

/**
 * Container for callback parameters.
 */
struct FCallbackEventParameters
{
	/** the consumer that generated the event */
	class FCallbackEventDevice*		Sender;

	/** the type of event sent */
	ECallbackEventType				EventType;

	/** @name additional parameters (optional) */
	//@{
	// the following members are optional and only used by certain events
	DWORD							EventFlag;
	UINT							EventMessage;
	const FString&					EventString;
	const FVector&					EventVector;
	class FEdMode*					EventEditorMode;
	class UObject*					EventObject;
	class FViewport*				EventViewport;
	//@}

	/**
	 * Constructor - callers wishing to use additional parameters should set those directly prior to calling Send()
	 */
	FCallbackEventParameters( FCallbackEventDevice* InSender, ECallbackEventType InType );
	FCallbackEventParameters( FCallbackEventDevice* InSender, ECallbackEventType InType, const FString& InString );
	FCallbackEventParameters( FCallbackEventDevice* InSender, ECallbackEventType InType, const FVector& InVector );
	FCallbackEventParameters( FCallbackEventDevice* InSender, ECallbackEventType InType, UObject* InEventObj );
	FCallbackEventParameters( FCallbackEventDevice* InSender, ECallbackEventType InType, DWORD InEventFlag );
	FCallbackEventParameters( FCallbackEventDevice* InSender, ECallbackEventType InType, DWORD InEventFlag, UObject* InEventObj );
	FCallbackEventParameters( FCallbackEventDevice* InSender, ECallbackEventType InType, DWORD InEventFlag, UObject* InEventObj, const FString& InEventString );
};

/**
 * Container for callback query parameters.
 */
struct FCallbackQueryParameters
{
	/** the consumer that generated the query */
	class FCallbackQueryDevice*	Sender;

	/** the type of query sent */
	ECallbackQueryType			QueryType;

	/** @name additional parameters (optional) */
	//@{
	class UObject*				QueryObject;
	const FString&				QueryString;
	//@}

	/** @name holds result data (optional) */
	FString						ResultString;

	/**
	 * Constructor - callers wishing to use additional parameters should set those directly prior to calling Query()
	 */
	FCallbackQueryParameters( class FCallbackQueryDevice* InSender, ECallbackQueryType InType );
	FCallbackQueryParameters( class FCallbackQueryDevice* InSender, ECallbackQueryType InType, const FString& InString );
};

/**
 * Base interface for firing events from the engine to an unknown set of
 * listeners.
 */
class FCallbackEventDevice
{
public:
	FCallbackEventDevice()
	{
	}
	virtual ~FCallbackEventDevice();

	virtual void Send( ECallbackEventType InType )	{}
	virtual void Send( ECallbackEventType InType, DWORD InFlag )	{}
	virtual void Send( ECallbackEventType InType, const FVector& InVector ) {}
	virtual void Send( ECallbackEventType InType, class FEdMode* InMode ) {}
	virtual void Send( ECallbackEventType InType, UObject* InObject ) {}
	virtual void Send( ECallbackEventType InType, class FViewport* InViewport, UINT InMessage) {}
	virtual void Send( ECallbackEventType InType, const FString& InString, UObject* InObject) {}
	virtual void Send( ECallbackEventType InType, const FString& InString, UPackage* InPackage, UObject* InObject) {}
	virtual void Send( const FCallbackEventParameters& Parms ) {}
};

/**
 * Base interface for querying from the engine to an unknown responder.
 */
class FCallbackQueryDevice
{
public:
	FCallbackQueryDevice()
	{
	}
	virtual ~FCallbackQueryDevice()
	{
	}

	virtual UBOOL Query( ECallbackQueryType InType, const FString& InString )	{ return FALSE; }
	virtual UBOOL Query( ECallbackQueryType InType, UObject* QueryObject )		{ return FALSE; }
	virtual INT Query( ECallbackQueryType InType, const FString& InString, UINT InMessage ) { return 0; }
	virtual UBOOL Query( FCallbackQueryParameters& Parms )						{ return FALSE; }
};

/**
 * This is a shared implementation of the FCallbackEventDevice that adheres to the
 * observer pattern. Consumers register with this class to be notified when a
 * particular ECallbackEventType event is fired.
 */
class FCallbackEventObserver :
	public FCallbackEventDevice
{
protected:
	typedef TLookupMap <FCallbackEventDevice*> FObserverArray;

	/**
	 * This is the array of observers that are registered for a particular event
	 */
	FObserverArray RegisteredObservers[CALLBACK_EventCount];

public:
	/**
	 * Default constructor. Does nothing special.
	 */
	FCallbackEventObserver(void)
	{
	}

	/**
	 * Destructor. Does nothing special.
	 */
	virtual ~FCallbackEventObserver(void)
	{
	}

// FCallbackDeviceObserver interface

	/**
	 * Registers an observer with the even they are interested in.
	 *
	 * @param InType the event that they wish to be notified of
	 * @param InObserver the object that wants to be notified of the change
	 */
	virtual void Register(ECallbackEventType InType,FCallbackEventDevice* InObserver)
	{
		checkf(InType < CALLBACK_EventCount, TEXT("Value is out of range"));
		// Add them to the list
		RegisteredObservers[InType].AddItem(InObserver);
	}

	/**
	 * Unregisters an observer from a single event type
	 *
	 * @param InType the event that they no longer wish to be notified of
	 * @param InObserver the object that wants to be removed from the list
	 */
	virtual void Unregister(ECallbackEventType InType,FCallbackEventDevice* InObserver)
	{
		checkf(InType < CALLBACK_EventCount, TEXT("Value is out of range"));
		// Remove them from the list
		RegisteredObservers[InType].RemoveItem(InObserver);
	}

	/**
	 * Unregisters an observer from all of their registered events
	 *
	 * @param InObserver the object that wants to be removed from the list
	 */
	virtual void UnregisterAll(FCallbackEventDevice* InObserver)
	{
		// Go through all events removing registered observers
		for (INT Index = 0; Index < CALLBACK_EventCount; Index++)
		{
			// Remove them from the list
			RegisteredObservers[Index].RemoveItem(InObserver);
		}
	}

// FCallbackEventDevice interface

	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired
	 *
	 * @param InType the event that was fired
	 */
	virtual void Send(ECallbackEventType InType)
	{
		check(InType < CALLBACK_EventCount && "Value is out of range");
		// Loop through each registered observer and notify them
		for (INT Index = 0; Index < RegisteredObservers[InType].Num(); Index++)
		{
			RegisteredObservers[InType](Index)->Send(InType);
		}
	}

	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired
	 *
	 * @param InType the event that was fired
	 * @param InFlags the special flags associated with this event
	 */
	virtual void Send(ECallbackEventType InType,DWORD InFlags)
	{
		check(InType < CALLBACK_EventCount && "Value is out of range");
		// Loop through each registered observer and notify them
		for (INT Index = 0; Index < RegisteredObservers[InType].Num(); Index++)
		{
			RegisteredObservers[InType](Index)->Send(InType,InFlags);
		}
	}

	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired
	 *
	 * @param InType the event that was fired
	 * @param InVector the vector associated with this event
	 */
	virtual void Send(ECallbackEventType InType,const FVector& InVector)
	{
		check(InType < CALLBACK_EventCount && "Value is out of range");
		// Loop through each registered observer and notify them
		for (INT Index = 0; Index < RegisteredObservers[InType].Num(); Index++)
		{
			RegisteredObservers[InType](Index)->Send(InType,InVector);
		}
	}

	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired
	 *
	 * @param InType the event that was fired
	 * @param InMode the editor associated with this event
	 */
	virtual void Send(ECallbackEventType InType,class FEdMode* InMode)
	{
		check(InType < CALLBACK_EventCount && "Value is out of range");
		// Loop through each registered observer and notify them
		for (INT Index = 0; Index < RegisteredObservers[InType].Num(); Index++)
		{
			RegisteredObservers[InType](Index)->Send(InType,InMode);
		}
	}

	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired
	 *
	 * @param InType the event that was fired
	 * @param InObject the object associated with this event
	 */
	virtual void Send(ECallbackEventType InType,UObject* InObject)
	{
		check(InType < CALLBACK_EventCount && "Value is out of range");
		// Loop through each registered observer and notify them
		for (INT Index = 0; Index < RegisteredObservers[InType].Num(); Index++)
		{
			RegisteredObservers[InType](Index)->Send(InType,InObject);
		}
	}
	
	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired
	 *
	 * @param InType the event that was fired
	 * @param InViewport the viewport associated with this event
	 * @param InMessage the message for this event
	 */
	virtual void Send(ECallbackEventType InType,class FViewport* InViewport,
		UINT InMessage)
	{
		check(InType < CALLBACK_EventCount && "Value is out of range");
		// Loop through each registered observer and notify them
		for (INT Index = 0; Index < RegisteredObservers[InType].Num(); Index++)
		{
			RegisteredObservers[InType](Index)->Send(InType,InViewport,InMessage);
		}
	}
	
	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired
	 *
	 * @param InType the event that was fired
	 * @param InString the string information associated with this event
	 * @param InObject the object associated with this event
	 */
	virtual void Send(ECallbackEventType InType,const FString& InString,
		UObject* InObject)
	{
		check(InType < CALLBACK_EventCount && "Value is out of range");
		// Loop through each registered observer and notify them
		for (INT Index = 0; Index < RegisteredObservers[InType].Num(); Index++)
		{
			RegisteredObservers[InType](Index)->Send(InType,InString,InObject);
		}
	}

	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired
	 *
	 * @param InType	the event that was fired
	 * @param InString	the string information associated with this event
	 * @param InPackage	the package associated with this event
	 * @param InObject	the object associated with this event
	 */
	virtual void Send(ECallbackEventType InType,const FString& InString, UPackage* InPackage, UObject* InObject)
	{
		check(InType < CALLBACK_EventCount && "Value is out of range");
		// Loop through each registered observer and notify them
		for (INT Index = 0; Index < RegisteredObservers[InType].Num(); Index++)
		{
			RegisteredObservers[InType](Index)->Send(InType, InString, InPackage, InObject);
		}
	}

	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired
	 *
	 * @param	Parms	the parameters for the event
	 */
	virtual void Send( const FCallbackEventParameters& Parms )
	{
		checkf(Parms.EventType<CALLBACK_EventCount,TEXT("Value is out of range (%i, max=%i)"), Parms.EventType, CALLBACK_EventCount);
		// Loop through each registered observer and notify them
		for (INT Index = 0; Index < RegisteredObservers[Parms.EventType].Num(); Index++)
		{
			RegisteredObservers[Parms.EventType](Index)->Send(Parms);
		}
	}
};

#endif
