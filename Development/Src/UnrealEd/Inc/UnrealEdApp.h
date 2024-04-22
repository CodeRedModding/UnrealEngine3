/*=============================================================================
	UnrealEdApp.h: The main application class

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNREALEDAPP_H__
#define __UNREALEDAPP_H__

// Forward declarations
class FSourceControl;
class UUnrealEdEngine;
class WxDlgActorFactory;
class WxDlgActorSearch;
class WxDlgBindHotkeys;
class WxDlgBuildProgress;
class WxDlgGeometryTools;
class WxDlgStaticMeshTools;
class WxDlgMapCheck;
class WxDlgLightingResults;
class WxDlgLightingBuildInfo;
class WxDlgStaticMeshLightingInfo;
class WxStartupTipDialog;
class WxDlgTransform;
class WxDlgAutoConvexCollision;
class WxPropertyWindow;
class WxPropertyWindowFrame;
class WxTerrainEditor;
class WxDlgDensityRenderingOptions;
#if ENABLE_SIMPLYGON_MESH_PROXIES
class WxDlgCreateMeshProxy;
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES

#if WITH_EASYHOOK
	struct _HOOK_TRACE_INFO_;
	typedef _HOOK_TRACE_INFO_* TRACED_HOOK_HANDLE;
#endif

#include "../../Unrealed/src/ConvexDecompTool.h"

/*-----------------------------------------------------------------------------
	FAutoConvexCollision

	Utility class that handles the actual creation of blocking volumes
	when the "Auto Convex Collision" dialog is used on selected static meshes.
-----------------------------------------------------------------------------*/

class FAutoConvexCollision : public FConvexDecompOptionHook
{
	/** Handler for the FConvexDecompOptionHook hook */
	virtual void DoDecomp(INT Depth, INT MaxVerts, FLOAT CollapseThresh);
	virtual void DecompOptionsClosed();
	virtual void DecompNewVolume();

	TArray<FPoly*> GetSelectedPolygons();
	void SetCollisionTypeOnSelectedActors( BYTE InCollisionType );

	/** The last blocking volume that was created.  This will be deleted when the user next hits the Apply button. */
	ABlockingVolume* LastCreatedVolume;
public:
	FAutoConvexCollision();
};

/*-----------------------------------------------------------------------------
	WxUnrealEdApp
-----------------------------------------------------------------------------*/

class WxUnrealEdApp :
	public WxLaunchApp,
	// The interface for receiving notifications
	public FCallbackEventDevice
{
	/**
	 * Uses INI file configuration to determine which class to use to create
	 * the editor's frame. Will fall back to Epic's hardcoded frame if it is
	 * unable to create the configured one properly
	 */
	WxEditorFrame* CreateEditorFrame(void);

public:

	enum EAutosaveState
	{
		AUTOSAVE_Inactive,
		AUTOSAVE_Saving,
		AUTOSAVE_Cancelled
	};

	EAutosaveState AutosaveState;

	WxEditorFrame* EditorFrame;							// The overall editor frame (houses everything)

	FString LastDir[NUM_LAST_DIRECTORIES];
	/** The default map extension when saving */
	FString DefaultMapExt;
	/** Supported map extensions for saving and loading **/
	TSet<FString> SupportedMapExtensions;

	/** A mapping from wx keys to unreal keys. */
	TMap<INT, FName>	WxKeyToUnrealKeyMap;

	WxTerrainEditor* TerrainEditor;
	WxDlgGeometryTools* DlgGeometryTools;
	WxDlgStaticMeshTools* DlgStaticMeshTools;
	WxDlgBuildProgress* DlgBuildProgress;
	WxStartupTipDialog* StartupTipDialog;
	FAutoConvexCollision* AutoConvexHelper;

	TMap<INT,UClass*> ShaderExpressionMap;		// For relating menu ID's to shader expression classes

	WxPropertyWindowFrame*	ObjectPropertyWindow;

	TArray<class WxKismet*>	KismetWindows;

	/** Pointer to Sentinel stats-viewing tool. */
	class WxSentinel* SentinelTool;

	/** Pointer to Game Stats Visualizer tool */
	class WxGameStatsVisualizer* GameStatsVisualizer;

	virtual bool OnInit();
	virtual int OnExit();

	/** Generate a mapping of wx keys to unreal key names */
	void GenerateWxToUnrealKeyMap();

	/**
	 * @return Returns a unreal key name given a wx key event.
	 */
	virtual FName GetKeyName(wxKeyEvent &Event);

	/**
	 * Performs any required cleanup in the case of a fatal error.
	 */
	virtual void ShutdownAfterError();

// FCallbackEventDevice interface

	/**
	 * Routes the event to the appropriate handlers
	 *
	 * @param InType the event that was fired
	 * @param InFlags the flags for this event
	 */
	virtual void Send(ECallbackEventType InType,DWORD InFlags);

	/**
	 * Routes the event to the appropriate handlers
	 *
	 * @param InType the event that was fired
	 */
	virtual void Send(ECallbackEventType InType);

	/**
	 * Routes the event to the appropriate handlers
	 *
	 * @param InObject the relevant object for this event
	 */
	virtual void Send(ECallbackEventType InType, UObject* InObject);

	/**
	 * Routes the event to the appropriate handlers
	 *
	 * @param InType the event that was fired
	 * @param InEdMode the FEdMode that is changing
	 */
	virtual void Send(ECallbackEventType InType,FEdMode* InEdMode);

	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired
	 *
	 * @param InType the event that was fired
	 * @param InString the string information associated with this event
	 * @param InObject the object associated with this event
	 */
	virtual void Send(ECallbackEventType InType,const FString& InString, UObject* InObject);

	/** Returns a handle to the current terrain editor. */
	WxTerrainEditor* GetTerrainEditor()
	{
		return TerrainEditor;
	};

	/**
	*  Updates text and value for various progress meters.
	*
	*	@param StatusText				New status text
	*	@param ProgressNumerator		Numerator for the progress meter (its current value).
	*	@param ProgressDenominitator	Denominiator for the progress meter (its range).
	*/
	void StatusUpdateProgress(const TCHAR* StatusText, INT ProgressNumerator, INT ProgressDenominator, UBOOL bUpdateBuildDialog=TRUE );

	/**
	* Returns whether or not the map build in progressed was cancelled by the user. 
	*/
	UBOOL GetMapBuildCancelled() const;

	/**
	* Sets the flag that states whether or not the map build was cancelled.
	*
	* @param InCancelled	New state for the cancelled flag.
	*/
	void SetMapBuildCancelled( UBOOL InCancelled );

	// Callback handlers.
	void CB_SelectionChanged();
	void CB_SurfProps();
	/**
	 * Displays a property dialog based upon what is currently selected.
	 * If no actors are selected, but one or more BSP surfaces are, the surface property dialog
	 * is displayed. If any actors are selected, the actor property dialog is displayed. If no
	 * actors or BSP surfaces are selected, no dialog is displayed.
	 */
	void CB_SelectedProps();
	void CB_CameraModeChanged();
	void CB_ActorPropertiesChanged();
	void CB_ObjectPropertyChanged(UObject* Object);
	void CB_ForcePropertyWindowRebuild(UObject* Object);
	void CB_RefreshPropertyWindows();
	void CB_DeferredRefreshPropertyWindows();
	void CB_DisplayLoadErrors();
	void CB_RefreshEditor();
	void CB_MapChange( DWORD InFlags );
	void CB_RedrawAllViewports();
	void CB_Undo();
	void CB_EndPIE();
	void CB_EditorModeEnter(const FEdMode& InEdMode);
	void CB_EditorModeExit(const FEdMode& InEdMode);
	void CB_LayersHaveChanged(AActor* Actor);
	
	void CB_ProcBuildingUpdate(class AProcBuilding* Building);

	/** Fix any shared or mis-leveled LOD actors */
	void CB_FixProcBuildingLODs();

	/** Called right before unit testing is about to begin */
	void CB_PreUnitTesting();

	/** Called right after unit testing concludes */
	void CB_PostUnitTesting();

	/** Attempts to set the base of this static mesh actor to any proc building directly below it. */
	void CB_AutoBaseSMActorToProcBuilding(AStaticMeshActor* SMActor);

	void CB_UpdateMobileFlattenedTexture(class UMaterialInterface* MaterialInterface);

	/**
	 * Accessor for WxDlgActorSearch Window
	 */
	WxDlgActorSearch* GetDlgActorSearch(void);
	/**
	 * Accessor for WxDlgMapCheck Window
	 */
	WxDlgMapCheck* GetDlgMapCheck(void);
	/**
	 * Accessor for WxDlgLightingResults Window
	 */
	WxDlgLightingResults* GetDlgLightingResults(void);
	/**
	 * Accessor for WxDlgLightingBuildInfo Window
	 */
	WxDlgLightingBuildInfo* GetDlgLightingBuildInfo(void);
	/**
	 * Accessor for WxDlgStaticMeshLightingInfo Window
	 */
	WxDlgStaticMeshLightingInfo* GetDlgStaticMeshLightingInfo(void);
	/**
	 * Accessor for WxDlgActorFactory Window
	 */
	WxDlgActorFactory* GetDlgActorFactory(void);
	/**
	 * Accessor for WxDlgTransform Window
	 */
	WxDlgTransform* GetDlgTransform(void);
	/*
	 * Accessor for WxConvexDecompOptions Window
	 */
	WxConvexDecompOptions* GetDlgAutoConvexCollision(void);
	/**
	 * Accessor for WxDlgBindHotkeys Window
	 */
	WxDlgBindHotkeys* GetDlgBindHotkeys(void);
	/**
	 * Accessor for WxDlgSurfaceProperties Window
	 */
	WxDlgSurfaceProperties* GetDlgSurfaceProperties(void);
	/**
	 * Accessor for WxDlgDensityRenderingOptions Window
	 */
	WxDlgDensityRenderingOptions* GetDlgDensityRenderingOptions(void);
#if ENABLE_SIMPLYGON_MESH_PROXIES
	/**
	 * Accessor for WxDlgCreateMeshProxy Window
	 */
	WxDlgCreateMeshProxy* GetDlgCreateMeshProxy(void);
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES

	/** 
	* Check if the key pressed should process a global hotkey action
	*
	* @param Key			The key that was pressed
	* @param bIsCtrlDown	Is either Ctrl button down
	* @param bIsShiftDown	Is either Shift button down
	* @param bIsALtDown		Is either Alt button down
	*
	* @return TRUE is a global hotkey was processed
	*/
	static UBOOL CheckIfGlobalHotkey( FName Key, UBOOL bIsCtrlDown, UBOOL bIsShiftDown, UBOOL bIsAltDown );

protected:

#if WITH_EASYHOOK
	/** Installs a hook for ProcedureName in the library DllHandle, to call ReplacementProcedurePtr */
	static TRACED_HOOK_HANDLE InstallHookHelper(HMODULE DllHandle, LPCSTR ProcedureName, void* ReplacementProcedurePtr, UBOOL bLimitToMainThread);

	/** Uses EasyHook to install hooks for broken API calls (before managed code & WPF init) */
	static void InstallHooksPreInit();

public:
	/** Uses EasyHook to install hooks for broken API calls (after WPF init; should be called once a WPF window
	    is created (after calling Interop::HwndSource constructor) */
	static void InstallHooksWPF();
#endif


private:
	WxDlgActorSearch*				DlgActorSearch;
	WxDlgMapCheck*					DlgMapCheck;
	WxDlgLightingResults*			DlgLightingResults;
	WxDlgLightingBuildInfo*			DlgLightingBuildInfo;
	WxDlgStaticMeshLightingInfo*	DlgStaticMeshLightingInfo;
	WxDlgActorFactory*				DlgActorFactory;
	WxDlgTransform*					DlgTransform;
	WxConvexDecompOptions*			DlgAutoConvexCollision;
	WxDlgBindHotkeys*				DlgBindHotkeys;
	WxDlgSurfaceProperties*			DlgSurfaceProperties;
	WxDlgDensityRenderingOptions*	DlgDensityRenderingOptions;
#if ENABLE_SIMPLYGON_MESH_PROXIES
	WxDlgCreateMeshProxy*			DlgCreateMeshProxy;
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES

	WxDlgLoadErrors* DlgLoadErrors;
	
	/** Stores whether or not the current map build was cancelled. */
	UBOOL bCancelBuild;

	/**
	* Process key press events
	*
	* @param	The event to process
	*/
	void KeyPressed(wxKeyEvent& Event);

	/**
	* Process navigation key press events.  Useful for TAB.
	*
	* @param	The event to process
	*/
	void NavKeyPressed(wxNavigationKeyEvent& Event);

	DECLARE_EVENT_TABLE()
};

#endif // __UNREALEDAPP_H__
