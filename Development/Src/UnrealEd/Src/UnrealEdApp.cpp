/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "DlgActorFactory.h"
#include "DlgBuildProgress.h"
#include "DlgGeometryTools.h"
#include "DlgStaticMeshTools.h"
#include "DlgMapCheck.h"
#include "DlgLightingResults.h"
#include "StartupTipDialog.h"
#include "DlgTransform.h"
#include "DlgDensityRenderingOptions.h"
#if ENABLE_SIMPLYGON_MESH_PROXIES
#include "DlgCreateMeshProxy.h"
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
#include "DebugToolExec.h"
#include "UnTexAlignTools.h"
#include "SourceControl.h"
#include "..\..\Launch\Resources\resource.h"
#include "PropertyWindow.h"
#include "PropertyWindowManager.h"
#include "FileHelpers.h"
#include "DlgActorSearch.h"
#include "TerrainEditor.h"
#include "ScopedTransaction.h"
#include "EnginePhysicsClasses.h"
#include "EngineProcBuildingClasses.h"
#include "EngineSplineClasses.h"
#include "BSPOps.h"
#include "BusyCursor.h"
#include "LayerUtils.h"
#include "SplashScreen.h"
#include "EditorBuildUtils.h"

#if WITH_MANAGED_CODE
	#include "ContentBrowserShared.h"
	#include "GameAssetDatabaseShared.h"
	#include "InteropShared.h"
	#include "LightmapResRatioWindowShared.h"
	#include "ColorPickerShared.h"
	#include "ConsolidateWindowShared.h"
	#include "FileSystemNotificationShared.h"
	#include "WelcomeScreenShared.h"
	#include "NewMapShared.h"
#endif


#if WITH_EASYHOOK
	#include "EasyHook.h"

#if _WIN64
	// EasyHook is used to intercept calls to GetPenEventMultiple which causes crashes in
	// 64 bit Vista/Win7 when a tablet is present
	#pragma comment( lib, "EasyHook64.lib" )

	UBOOL GEnablePenIMC = FALSE;
#else
	// EasyHook is used to intercept calls to GetCursorPos which contains a bug in 32-bit XP/Vista in
	// large address aware applications
	#pragma comment( lib, "EasyHook32.lib" )
#endif

	#ifndef STATUS_SUCCESS
		#define STATUS_SUCCESS  ((NTSTATUS)0x00000000L)
	#endif
#endif




WxUnrealEdApp*		GApp;
UUnrealEdEngine*	GUnrealEd;
WxDlgAddSpecial*	GDlgAddSpecial;

/*-----------------------------------------------------------------------------
	FAutoConvexCollision
-----------------------------------------------------------------------------*/
FAutoConvexCollision::FAutoConvexCollision()
	: LastCreatedVolume(NULL)
{

}

void FAutoConvexCollision::DoDecomp(INT Depth, INT MaxVerts, FLOAT CollapseThresh)
{
	const FScopedTransaction Transaction( TEXT("Create Blocking Volume") );
	const FScopedBusyCursor BusyCursor;
	GWorld->GetBrush()->Modify();

	// If we have previously created a blocking volume, get rid of it.

	if( LastCreatedVolume != NULL )
	{
		GWorld->DestroyActor( LastCreatedVolume );
	}

	// Gather a list of selected polygons.

	TArray<FPoly*> SelectedPolys = GetSelectedPolygons();

	// Generate a bounding box for the selected polygons.  Will be used to compute a scaling factor below.

	FBox bbox(0);

	for( INT p = 0 ; p < SelectedPolys.Num() ; ++p )
	{
		FPoly* poly = SelectedPolys(p);

		for( INT v = 0 ; v < poly->Vertices.Num() ; ++v )
		{
			bbox += poly->Vertices(v);
		}
	}

	// Compute the scaling factor to get these polygons within a 256x256x256 cube.  The tools seem to work best if meshes aren't scaled to large values.

	FVector extent = bbox.GetExtent();
	FLOAT MaxAxis = Max( extent.Z, Max( extent.X, extent.Y ) );

	FLOAT ScaleFactor = MaxAxis / 128.0f;

	// Create vertex/index buffers from the selected polygon list

	INT NumVerts = SelectedPolys.Num(), idx = 0;
	TArray<FVector> Verts;
	TArray<INT> Indices;
	for( INT p = 0 ; p < SelectedPolys.Num() ; ++p )
	{
		FPoly* poly = SelectedPolys(p);

		for( INT v = 0 ; v < poly->Vertices.Num() ; ++v )
		{
			Verts.AddItem( poly->Vertices(v) / ScaleFactor );
			Indices.AddItem( idx );
			idx++;
		}
	}

	// Create a bodysetup to store the soon to be created collision primitives.  This will be thrown away at the end.

	URB_BodySetup* bs = ConstructObject<URB_BodySetup>(URB_BodySetup::StaticClass(), GWorld->CurrentLevel);

	// Run actual util to do the work
	DecomposeMeshToHulls( &(bs->AggGeom), Verts, Indices, Depth, 0.1f, CollapseThresh, MaxVerts );

	// Generate an offset vector.  This will be used to ensure that the newly created blocking volumes location matches the location
	// of the last selected static mesh.

	AActor* Actor = GEditor->GetSelectedActors()->GetTop<AActor>();
	FVector Offset(0,0,0);
	if( Actor )
	{
		Offset = Actor->Location;
	}

	// Create a new builder brush from the hull triangles.

	GEditor->ResetPivot();

	GWorld->GetBrush()->Location = FVector(0,0,0);
	GWorld->GetBrush()->Brush->Polys->Element.Empty();

	for( int ce = 0 ; ce < bs->AggGeom.ConvexElems.Num() ; ++ce )
	{
		FKConvexElem* convex = &bs->AggGeom.ConvexElems(ce);

		for( INT f = 0 ; f < convex->FaceTriData.Num() ; f += 3 )
		{
			FVector v0 = (convex->VertexData( convex->FaceTriData(f+0) ) * ScaleFactor ) - Offset;
			FVector v1 = (convex->VertexData( convex->FaceTriData(f+1) ) * ScaleFactor ) - Offset;
			FVector v2 = (convex->VertexData( convex->FaceTriData(f+2) ) * ScaleFactor ) - Offset;

			FPoly* Polygon = new FPoly();

			Polygon->Init();
			Polygon->PolyFlags = PF_DefaultFlags;

			Polygon->Vertices.AddItem( v0 );
			Polygon->Vertices.AddItem( v2 );
			Polygon->Vertices.AddItem( v1 );

			Polygon->CalcNormal(1);

			GWorld->GetBrush()->Brush->Polys->Element.AddItem( *Polygon );
		}
	}

	// Clean up the builder brush

	GWorld->GetBrush()->Location = Offset;

	GWorld->GetBrush()->ClearComponents();
	GWorld->GetBrush()->ConditionalUpdateComponents();

	// Create the blocking volume

	LastCreatedVolume = (ABlockingVolume*)GWorld->SpawnActor(ABlockingVolume::StaticClass(),NAME_None,GWorld->GetBrush()->Location);
	if( LastCreatedVolume )
	{
		LastCreatedVolume->PreEditChange(NULL);

		FBSPOps::csgCopyBrush
			(
			LastCreatedVolume,
			GWorld->GetBrush(),
			0,
			RF_Transactional,
			1,
			TRUE
			);

		// Set the texture on all polys to NULL.  This stops invisible texture
		// dependencies from being formed on volumes.

		if( LastCreatedVolume->Brush )
		{
			for( INT poly = 0 ; poly < LastCreatedVolume->Brush->Polys->Element.Num() ; ++poly )
			{
				FPoly* Poly = &(LastCreatedVolume->Brush->Polys->Element(poly));
				Poly->Material = NULL;
			}
		}

		LastCreatedVolume->PostEditChange();
	}

	// Set up collision on selected actors

	SetCollisionTypeOnSelectedActors( COLLIDE_BlockWeapons );

	// Clean up

	for( INT x = 0 ; x < SelectedPolys.Num() ; ++x )
	{
		delete SelectedPolys(x);
	}

	SelectedPolys.Empty();

	// Update screen.

	GEditor->RedrawLevelEditingViewports();
}

void FAutoConvexCollision::DecompOptionsClosed()
{
	DecompNewVolume();
}

void FAutoConvexCollision::DecompNewVolume()
{
	LastCreatedVolume = NULL;
}

/**
* Gathers up a list of selection FPolys from selected static meshes.
*
* @return	A TArray containing FPolys representing the triangles in the selected static meshes (note that these
*           triangles are transformed into world space before being added to the array.
*/

TArray<FPoly*> FAutoConvexCollision::GetSelectedPolygons()
{
	// Build a list of polygons from all selected static meshes

	TArray<FPoly*> SelectedPolys;

	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		for(INT j=0; j<Actor->AllComponents.Num(); j++)
		{
			// If its a static mesh component, with a static mesh
			UActorComponent* Comp = Actor->AllComponents(j);
			UStaticMeshComponent* SMComp = Cast<UStaticMeshComponent>(Comp);
			if(SMComp && SMComp->StaticMesh)
			{
				UStaticMesh* StaticMesh = SMComp->StaticMesh;
				const FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) StaticMesh->LODModels(0).RawTriangles.Lock(LOCK_READ_ONLY);

				if( SMComp->StaticMesh->LODModels(0).RawTriangles.GetElementCount() )
				{
					for( INT TriangleIndex = 0 ; TriangleIndex < StaticMesh->LODModels(0).RawTriangles.GetElementCount() ; TriangleIndex++ )
					{
						const FStaticMeshTriangle& Triangle = RawTriangleData[TriangleIndex];
						FPoly* Polygon = new FPoly;

						// Add the poly

						Polygon->Init();
						Polygon->PolyFlags = PF_DefaultFlags;

						new(Polygon->Vertices) FVector(Actor->LocalToWorld().TransformFVector( Triangle.Vertices[2] ) );
						new(Polygon->Vertices) FVector(Actor->LocalToWorld().TransformFVector( Triangle.Vertices[1] ) );
						new(Polygon->Vertices) FVector(Actor->LocalToWorld().TransformFVector( Triangle.Vertices[0] ) );

						Polygon->CalcNormal(1);
						Polygon->Fix();
						if( Polygon->Vertices.Num() > 2 )
						{
							if( !Polygon->Finalize( NULL, 1 ) )
							{
								SelectedPolys.AddItem( Polygon );
							}
						}

						// And add a flipped version of it to account for negative scaling

						Polygon = new FPoly;

						Polygon->Init();
						Polygon->PolyFlags = PF_DefaultFlags;

						new(Polygon->Vertices) FVector(Actor->LocalToWorld().TransformFVector( Triangle.Vertices[2] ) );
						new(Polygon->Vertices) FVector(Actor->LocalToWorld().TransformFVector( Triangle.Vertices[0] ) );
						new(Polygon->Vertices) FVector(Actor->LocalToWorld().TransformFVector( Triangle.Vertices[1] ) );

						Polygon->CalcNormal(1);
						Polygon->Fix();
						if( Polygon->Vertices.Num() > 2 )
						{
							if( !Polygon->Finalize( NULL, 1 ) )
							{
								SelectedPolys.AddItem( Polygon );
							}
						}
					}
				}
				SMComp->StaticMesh->LODModels(0).RawTriangles.Unlock();
			}
		}
	}

	return SelectedPolys;
}

/**
* Utility function to quickly set the collision type on selected static meshes.
*
* @param	InCollisionType		The collision type to use (COLLIDE_??)
*/

void FAutoConvexCollision::SetCollisionTypeOnSelectedActors( BYTE InCollisionType )
{
	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );
		AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor);

		if( StaticMeshActor )
		{
			StaticMeshActor->Modify();
			StaticMeshActor->CollisionType = InCollisionType;
			StaticMeshActor->SetCollisionFromCollisionType();
		}
	}
}

/*-----------------------------------------------------------------------------
	WxUnrealEdApp
-----------------------------------------------------------------------------*/

/**
 * Function for getting the editor frame window without knowing about it
 */
class wxWindow* GetEditorFrame(void)
{
	return GApp->EditorFrame;
}

BEGIN_EVENT_TABLE( WxUnrealEdApp, wxApp )
	EVT_KEY_DOWN( WxUnrealEdApp::KeyPressed )
	EVT_NAVIGATION_KEY( WxUnrealEdApp::NavKeyPressed )
END_EVENT_TABLE()

/**
 * Uses INI file configuration to determine which class to use to create
 * the editor's frame. Will fall back to Epic's hardcoded frame if it is
 * unable to create the configured one properly
 */
WxEditorFrame* WxUnrealEdApp::CreateEditorFrame(void)
{
	// Look up the name of the frame we are creating
	FString EditorFrameName;
	GConfig->GetString(TEXT("EditorFrame"),TEXT("FrameClassName"),
		EditorFrameName,GEditorIni);
	// In case the INI is messed up
	if (EditorFrameName.Len() == 0)
	{
		EditorFrameName = TEXT("WxEditorFrame");
	}
	// Use the wxWindows' RTTI system to create the window
	wxObject* NewObject = wxCreateDynamicObject(*EditorFrameName);
	if (NewObject == NULL)
	{
		debugf(TEXT("Failed to create the editor frame class %s"),
			*EditorFrameName);
		debugf(TEXT("Falling back to WxEditorFrame"));
		// Fallback to the default frame
		NewObject = new WxEditorFrame();
	}
	// Make sure it's the right type too
	if (wxDynamicCast(NewObject,WxEditorFrame) == NULL)
	{
		debugf(TEXT("Class %s is not derived from WxEditorFrame"),
			*EditorFrameName);
		debugf(TEXT("Falling back to WxEditorFrame"));
		delete NewObject;
		NewObject = new WxEditorFrame();
	}
	WxEditorFrame* Frame = wxDynamicCast(NewObject,WxEditorFrame);
	check(Frame);
	// Now do the window intialization
	Frame->Create();
	return Frame;
}

/**
 *  Updates text and value for various progress meters.
 *
 *	@param StatusText				New status text
 *	@param ProgressNumerator		Numerator for the progress meter (its current value).
 *	@param ProgressDenominitator	Denominiator for the progress meter (its range).
 */
void WxUnrealEdApp::StatusUpdateProgress( const TCHAR* StatusText, INT ProgressNumerator, INT ProgressDenominator, UBOOL bUpdateBuildDialog/*=TRUE*/ )
{
	// Clean up deferred cleanup objects from rendering thread every once in a while.
	static DOUBLE LastTimePendingCleanupObjectsWhereDeleted;
	if( appSeconds() - LastTimePendingCleanupObjectsWhereDeleted > 1 )
	{
		// Get list of objects that are pending cleanup.
		FPendingCleanupObjects* PendingCleanupObjects = GetPendingCleanupObjects();
		// Flush rendering commands in the queue.
		FlushRenderingCommands();
		// It is now save to delete the pending clean objects.
		delete PendingCleanupObjects;
		// Keep track of time this operation was performed so we don't do it too often.
		LastTimePendingCleanupObjectsWhereDeleted = appSeconds();
	}

	// Update build progress dialog if it is visible.
	const UBOOL bBuildProgressDialogVisible = DlgBuildProgress->IsShown();

	if( bBuildProgressDialogVisible && bUpdateBuildDialog )
	{
		if( StatusText != NULL )
		{
			DlgBuildProgress->SetBuildStatusText( StatusText );
		}

		DlgBuildProgress->SetBuildProgressPercent( ProgressNumerator, ProgressDenominator );
		::wxSafeYield(DlgBuildProgress,TRUE);
	}
}

/**
 * Returns whether or not the map build in progressed was cancelled by the user. 
 */
UBOOL WxUnrealEdApp::GetMapBuildCancelled() const
{
	return bCancelBuild;
}

/**
 * Sets the flag that states whether or not the map build was cancelled.
 *
 * @param InCancelled	New state for the cancelled flag.
 */
void WxUnrealEdApp::SetMapBuildCancelled( UBOOL InCancelled )
{
	bCancelBuild = InCancelled;
}


#if WITH_EASYHOOK

// Returns a handle to the installed hook.  This must remain resident for the application's lifetime, or until the hook is removed.
TRACED_HOOK_HANDLE WxUnrealEdApp::InstallHookHelper(HMODULE DllHandle, LPCSTR ProcedureName, void* ReplacementProcedurePtr, UBOOL bLimitToMainThread)
{
	TRACED_HOOK_HANDLE HookHandle = new HOOK_TRACE_INFO;
	HookHandle->Link = 0;

	if (DllHandle != NULL)
	{
		FARPROC ProcedurePtr = GetProcAddress(DllHandle, ProcedureName);
		if (ProcedurePtr != NULL)
		{
			// Install the hook!
			NTSTATUS HookResult = LhInstallHook(
				ProcedurePtr,					// Entry point
				ReplacementProcedurePtr,		// Hook function
				NULL,							// Callback
				HookHandle);					// (Out) Handle

			if (HookResult == STATUS_SUCCESS)
			{
				if (bLimitToMainThread)
				{
					// Set an inclusive list containing only the special thread ID 0, signifying the current thread
					ULONG ThreadsToHook = 0;
					HookResult = LhSetInclusiveACL(&ThreadsToHook, 1, HookHandle);
				}
				else
				{
					// Set an empty exclusive list, which means all threads will pass the test
					ULONG InvalidThread = -1;
					HookResult = LhSetExclusiveACL(&InvalidThread, 0, HookHandle);
				}

				if (HookResult == STATUS_SUCCESS)
				{
					// OK, we're good to go!
				}
				else
				{
					// Couldn't setup ACL
				}
			}
			else
			{
				// Couldn't install hook
			}
		}
	}

	return HookHandle;
}


#if _WIN64

/** Our replacement for GetPenEventMultiple which just ignores events, since the commHandles array can't be trusted */
INT32 UnrealEdHookedGetPenEventMultiple(
	INT32 cCommHandles,
	intptr_t* commHandles,
	intptr_t handleReset,
	INT32& iHandle,
	INT32& evt,
	INT32& stylusPointerId,
	INT32& cPackets,
	INT32& cbPacket,
	intptr_t& pPackets)
{
	static UBOOL HookedMethodCalled = FALSE;
	if (!HookedMethodCalled)
	{
		warnf(TEXT("The hooked GetPenEventMultiple() method was called; events are being ignored."));
		HookedMethodCalled = TRUE;
	}
	return 0;
}

#else

/** Our replacement for GetCursorPos which handles large addresses  */
BOOL WINAPI UnrealEdHookedGetCursorPos( LPPOINT OutPoint )
{
	// Use a static point so that it will likely reside below the 2 GB boundary in large-address-aware apps
    static POINT LowAddressPoint;
    BOOL ReturnValue = GetCursorPos( &LowAddressPoint );
	if( ReturnValue == TRUE && OutPoint != NULL )
	{
		appMemcpy( OutPoint, &LowAddressPoint, sizeof( LowAddressPoint ) );
	}

    return ReturnValue;
}

#endif // !_WIN64


/** Uses EasyHook to install hooks for broken WPF calls */
void WxUnrealEdApp::InstallHooksPreInit()
{
#if _WIN64
	GEnablePenIMC = ParseParam(appCmdLine(), TEXT("EnablePenIMC")) == TRUE;
#else
	// Workaround for WPF bug with Large Address Aware
	// 
	// Calls to GetCursorPos always fail with ERROR_NOACCESS when passed an
	// LPPOINT at a memory address higher than 2 GB. This is a common occurrence
	// when running a 32-bit process that is Large Address Aware.
	// 
	// Windows Presentation Foundation uses GetCursorPos when handling window
	// activation events and will throw a Win32Exception if GetCursorPos fails
	// (returns 0.) This exception prevents WPF applications from receiving mouse
	// focus! The WPF window will remain in an inactive state.
	//
	HMODULE DllHandle = LoadLibrary(TEXT("User32.dll"));
	checkf(DllHandle != NULL);

	static TRACED_HOOK_HANDLE GetCursorPosHookHandle =
		InstallHookHelper(DllHandle, "GetCursorPos", UnrealEdHookedGetCursorPos, TRUE);

	FreeLibrary(DllHandle);
#endif
}

void WxUnrealEdApp::InstallHooksWPF()
{
#if _WIN64
	// Workaround for WPF bug in 64 bit builds on machines with Pen / Tablet support
	//
	// Under some circumstances in a 64 bit build, the code either in PresentationFoundation or in
	// PenIMC.dll generates a bad pointer for comm handles, resulting in a bogus value in the commHandles
	// array passed to GetPenEventMultiple in PenIMC.dll, resulting in an access violation when it's dereferenced.
	//
	// The issue is intermittent but frequent on 64 bit machines with a Wacom tablet installed; and appears to be
	// a bug solely in WPF.  We hook GetPenEventMultiple and do no work in it, to prevent the actual crash.  Although
	// whatever WPF would have done with the pen input is thwarted, it doesn't stop the pen from being used as a
	// mouse to control the GUI of the editor, nor should it affect any other applications.
	//
	// Install the hook, but we must wait for WPF to load PenIMC itself; LoadLibrary isn't guaranteed to load the
	// same PenIMC as WPF does; this method is called from each site that creates a WPF windows
	static UBOOL HookedPenIMC = FALSE;
	if (!GEnablePenIMC && !HookedPenIMC)
	{
		HMODULE DllHandle = GetModuleHandle(TEXT("PenIMC.dll"));
		if (DllHandle != NULL)
		{
			static TRACED_HOOK_HANDLE GetPenEventMultipleHookHandle =
				InstallHookHelper(DllHandle, "GetPenEventMultiple", UnrealEdHookedGetPenEventMultiple, FALSE);
			HookedPenIMC = TRUE;
		}
	}
#endif
}

#endif		// WITH_EASYHOOK

bool WxUnrealEdApp::OnInit()
{
	GApp = this;
	GApp->EditorFrame = NULL;

	DOUBLE OnInitStartTime = appSeconds();

	AutosaveState = AUTOSAVE_Inactive;

#if WITH_EASYHOOK
	// EasyHook has a global list of threads and a per-hook local list
	// Each list can be set to must-be-one-of (the thread ID must be one on the list, they call it inclusive) or
	// to must-not-be (the thread ID cannot be in the list, called exclusive).
	//
	// We set the global list to exclusive with no entries, so the test is always down to the individual hook list

	// value of thread ID doesn't matter as count is 0
	ULONG InvalidThreadID = -1;
	verify(LhSetGlobalExclusiveACL(&InvalidThreadID, 0) == STATUS_SUCCESS);

	// Install hooks
	InstallHooksPreInit();
#endif

	// Start saving a backlog. This backlog will be read by the logwindow later on.
	GLog->EnableBacklog( TRUE );

	UBOOL bIsOk = WxLaunchApp::OnInit( );
	if ( !bIsOk )
	{
		appHideSplash();
		return 0;
	}


	// Register all callback notifications
	GCallbackEvent->Register(CALLBACK_SelChange,this);
	GCallbackEvent->Register(CALLBACK_SurfProps,this);
	GCallbackEvent->Register(CALLBACK_SelectedProps,this);
	GCallbackEvent->Register(CALLBACK_ActorPropertiesChange,this);
	GCallbackEvent->Register(CALLBACK_ObjectPropertyChanged,this);
	GCallbackEvent->Register(CALLBACK_ForcePropertyWindowRebuild, this);
	GCallbackEvent->Register(CALLBACK_RefreshPropertyWindows,this);
	GCallbackEvent->Register(CALLBACK_DeferredRefreshPropertyWindows,this);
	GCallbackEvent->Register(CALLBACK_EndPIE,this);
	GCallbackEvent->Register(CALLBACK_DisplayLoadErrors,this);
	GCallbackEvent->Register(CALLBACK_RedrawAllViewports,this);
	GCallbackEvent->Register(CALLBACK_UpdateUI,this);
	GCallbackEvent->Register(CALLBACK_Undo,this);
	GCallbackEvent->Register(CALLBACK_MapChange,this);
	GCallbackEvent->Register(CALLBACK_RefreshEditor,this);
	GCallbackEvent->Register(CALLBACK_ChangeEditorMode,this);
	GCallbackEvent->Register(CALLBACK_EditorModeEnter,this);
	GCallbackEvent->Register(CALLBACK_EditorModeExit,this);
	GCallbackEvent->Register(CALLBACK_PackageSaved,this);
	GCallbackEvent->Register(CALLBACK_LayersHaveChanged,this);
	GCallbackEvent->Register(CALLBACK_ProcBuildingUpdate,this);
	GCallbackEvent->Register(CALLBACK_ProcBuildingFixLODs,this);
	GCallbackEvent->Register(CALLBACK_BaseSMActorOnProcBuilding,this);
	GCallbackEvent->Register(CALLBACK_MobileFlattenedTextureUpdate,this);
	GCallbackEvent->Register(CALLBACK_PreUnitTesting,this);
	GCallbackEvent->Register(CALLBACK_PostUnitTesting,this);

	TerrainEditor = NULL;
	DlgActorSearch = NULL;
	DlgGeometryTools = NULL;
	DlgStaticMeshTools = NULL;
	SentinelTool = NULL;
	GameStatsVisualizer = NULL;

	// Get the editor
	EditorFrame = CreateEditorFrame();
	check(EditorFrame);
//	EditorFrame->Maximize();

	GEditorModeTools().Init();

	// Global dialogs.
	GDlgAddSpecial = new WxDlgAddSpecial();

	// Initialize "last dir" array.
	// NOTE: We append a "2" to the section name to enforce backwards compatibility.  "Directories" is deprecated.
	GApp->LastDir[LD_UNR]					= GConfig->GetStr( TEXT("Directories2"), TEXT("UNR"),				GEditorUserSettingsIni );
	GApp->LastDir[LD_BRUSH]					= GConfig->GetStr( TEXT("Directories2"), TEXT("BRUSH"),				GEditorUserSettingsIni );
	GApp->LastDir[LD_2DS]					= GConfig->GetStr( TEXT("Directories2"), TEXT("2DS"),				GEditorUserSettingsIni );
	GApp->LastDir[LD_PSA]					= GConfig->GetStr( TEXT("Directories2"), TEXT("PSA"),				GEditorUserSettingsIni );
#if WITH_FBX
	GApp->LastDir[LD_FBX_ANIM]				= GConfig->GetStr( TEXT("Directories2"), TEXT("FBXAnim"),			GEditorUserSettingsIni );
#endif // WITH_FBX
	GApp->LastDir[LD_GENERIC_IMPORT]		= GConfig->GetStr( TEXT("Directories2"), TEXT("GenericImport"),		GEditorUserSettingsIni );
	GApp->LastDir[LD_GENERIC_EXPORT]		= GConfig->GetStr( TEXT("Directories2"), TEXT("GenericExport"),		GEditorUserSettingsIni );
	GApp->LastDir[LD_GENERIC_OPEN]			= GConfig->GetStr( TEXT("Directories2"), TEXT("GenericOpen"),		GEditorUserSettingsIni );
	GApp->LastDir[LD_GENERIC_SAVE]			= GConfig->GetStr( TEXT("Directories2"), TEXT("GenericSave"),		GEditorUserSettingsIni );
	GApp->LastDir[LD_MESH_IMPORT_EXPORT]	= GConfig->GetStr( TEXT("Directories2"), TEXT("MeshImportExport"),	GEditorUserSettingsIni );

	// Remap editor directories to the user directory in shipped builds.
	// Also remapped \Content\ to \CookedPC\.
#if !SHIPPING_PC_GAME || UDK
	if ( ParseParam(appCmdLine(), TEXT("installed")) )
#endif
	{
		for ( INT PathIndex = 0 ; PathIndex < NUM_LAST_DIRECTORIES ; ++PathIndex )
		{
			GApp->LastDir[PathIndex] = GFileManager->ConvertAbsolutePathToUserPath( *GFileManager->ConvertToAbsolutePath(*GApp->LastDir[PathIndex]) );
			GApp->LastDir[PathIndex] = GApp->LastDir[PathIndex].Replace(TEXT("\\Content\\"),TEXT("\\CookedPC\\"),TRUE);
		}
	}

	if( !GConfig->GetString( TEXT("URL"), TEXT("MapExt"), GApp->DefaultMapExt, GEngineIni ) )
	{
		appErrorf(*LocalizeUnrealEd("Error_MapExtUndefined"));
	}

	
	GEditorModeTools().ActivateMode( EM_Default );
	GUnrealEd->Exec( *FString::Printf(TEXT("MODE MAPEXT=%s"), *GApp->DefaultMapExt ) );

	GApp->SupportedMapExtensions.Add( GApp->DefaultMapExt );
	FString AdditionalMapExt;
	if( GConfig->GetString( TEXT("URL"), TEXT("AdditionalMapExt"), AdditionalMapExt, GEngineIni ) )
	{
		GApp->SupportedMapExtensions.Add( AdditionalMapExt );
	}

#if WITH_MANAGED_CODE
	// Add a resolve handler to find the UnrealEdCSharp dll
	InteropTools::AddResolveHandler();

	// Initialize UnrealEdCSharp.dll backend interface
	InteropTools::InitUnrealEdCSharpBackend();

	// Load resource data for CLR.  Must be done before we initialize any WPF objects that depend
	// upon global resources.
	InteropTools::LoadResourceDictionaries();
#endif

#if HAVE_SCC
	// Init source control.
	FSourceControl::Init();
#endif

	//Moved to before GAD inits.  In case there is a "verify" kicked off and progress needs to display
	DlgBuildProgress = new WxDlgBuildProgress( EditorFrame );
	DlgBuildProgress->Show(false);

#if WITH_MANAGED_CODE

	// Initialize the game asset database
	if (FContentBrowser::ShouldUseContentBrowser())
	{
		GWarn->StatusUpdatef( -1, -1, *LocalizeUnrealEd( TEXT( "SplashScreen_StartingUpGameAssetDatabase" ) ) );

		//check to see if it's time to ensure assets are verified
		FGameAssetDatabase::CheckJournalAlarm();

		// We never want to update checkpoint files on editor startup.  Only commandlets run from a build
		// machine should be doing that.
		FGameAssetDatabaseStartupConfig StartupConfig;
		StartupConfig.bShouldCheckpoint = FALSE;

		// Unless the "-NoLiveTags" command-line option is used, we'll load unverified tag changes
		// from the SQL database from all users, not just the current user.
		StartupConfig.bOnlyLoadMyJournalEntries = ParseParam( appCmdLine(), TEXT( "NoLiveTags" ) );

		FString InitErrorMessageText;
		FGameAssetDatabase::Init(
				StartupConfig,
				InitErrorMessageText );	// Out

		if (InitErrorMessageText.Len() > 0)
		{
			warnf(TEXT("Asset Database errors: %s"), *InitErrorMessageText);
// jmarshall - disable GAD warning by default. 
/*
			if (!ParseParam(appCmdLine(), TEXT( "NoGADWarning" )))
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd( "UnrealEdStartup_GameAssetDatabaseStartupWarnings" ) );
			}
*/
// jmarshall end
		}

		GWarn->StatusUpdatef( -1, -1, *LocalizeUnrealEd( TEXT( "SplashScreen_DefaultLoading" ) ) );
	}
#endif	// #if WITH_MANAGED_CODE


	// Init the editor tools.
	GTexAlignTools.Init();

	EditorFrame->ButtonBar = new WxButtonBar;
	EditorFrame->ButtonBar->Create( (wxWindow*)EditorFrame, -1 );
	EditorFrame->ButtonBar->Show();

	DlgActorSearch = NULL;
	DlgMapCheck = NULL;
	DlgLightingResults = NULL;
	DlgLightingBuildInfo = NULL;
	DlgStaticMeshLightingInfo = NULL;
	DlgActorFactory = NULL;
	DlgTransform = NULL;

	AutoConvexHelper = new FAutoConvexCollision();

	DlgAutoConvexCollision = NULL;

	// Generate mapping from wx keys to unreal keys.
	GenerateWxToUnrealKeyMap();

	DlgBindHotkeys  = NULL;
	//temporarily fixing hot keys.  They are generated in the WxDlgBindHotkeys
	GetDlgBindHotkeys();
	
	//joegtemp -- Set the handle to use for GWarn->MapCheck_Xxx() calls
	GWarn->winEditorFrame	= (PTRINT)EditorFrame;
	GWarn->hWndEditorFrame	= (PTRINT)EditorFrame->GetHandle();

	DlgLoadErrors = NULL;
	DlgSurfaceProperties = NULL;
	
	DlgDensityRenderingOptions = NULL;

#if ENABLE_SIMPLYGON_MESH_PROXIES
	DlgCreateMeshProxy = NULL;
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES

	ObjectPropertyWindow = NULL;

	UClass* Parent = UMaterialExpression::StaticClass();
	INT ID = IDM_NEW_SHADER_EXPRESSION_START;

	if( Parent )
	{
		for(TObjectIterator<UClass> It;It;++It)
			if(It->IsChildOf(UMaterialExpression::StaticClass()) && !(It->ClassFlags & CLASS_Abstract))
			{
				ShaderExpressionMap.Set( ID, *It );
				ID++;
			}
	}


	// ToolTip preferences
	{
#if !PLATFORM_UNIX && !PLATFORM_MACOSX
		// ToolTip delay
		INT ToolTipDelayInMS = -1;
		GConfig->GetInt( TEXT( "ToolTips" ), TEXT( "DelayInMS" ), ToolTipDelayInMS, GEditorUserSettingsIni );

		// ToolTip duration
		INT ToolTipDurationInMS = 20000;
		GConfig->GetInt( TEXT( "ToolTips" ), TEXT( "DurationInMS" ), ToolTipDurationInMS, GEditorUserSettingsIni );

		// Send the tool tip settings to WxWidgets.  Note that values of -1 mean 'use defaults'.
		wxToolTip::SetDelay( ToolTipDelayInMS );
		wxToolTip::SetDisplayTime( ToolTipDurationInMS );

		// Save preferences
		GConfig->SetInt( TEXT( "ToolTips" ), TEXT( "DelayInMS" ), ToolTipDelayInMS, GEditorUserSettingsIni );
		GConfig->SetInt( TEXT( "ToolTips" ), TEXT( "DurationInMS" ), ToolTipDurationInMS, GEditorUserSettingsIni );
#endif
	}


	// Do final set up on the editor frame and show it
	{
		// Tear down rendering thread once instead of doing it for every window being resized.
		SCOPED_SUSPEND_RENDERING_THREAD(FSuspendRenderingThread::ST_RecreateThread);
		EditorFrame->SetUp();
		appHideSplash();
		EditorFrame->Show();
		GUnrealEd->GetBrowserManager()->RestoreBrowserWindows();
	}

	// Turn off and discard the startup backlog.
	GLog->EnableBacklog( FALSE );

	// Load the viewport configuration from the INI file.

	EditorFrame->ViewportConfigData = new FViewportConfig_Data;
	if( !EditorFrame->ViewportConfigData->LoadFromINI() )
	{
		UBOOL bHasMaximizedViewport = TRUE;
		EditorFrame->ViewportConfigData->SetTemplate( VC_2_2_Split );
		EditorFrame->ViewportConfigData->Apply( EditorFrame->ViewportContainer, bHasMaximizedViewport );

		// If no settings were found, default to the perspective viewport being maximized
		for ( INT ViewportIndex = 0; ViewportIndex < EditorFrame->ViewportConfigData->GetViewportCount(); ++ViewportIndex )
		{
			FVCD_Viewport& CurViewport = EditorFrame->ViewportConfigData->AccessViewport( ViewportIndex );
			if ( CurViewport.ViewportType == LVT_Perspective )
			{
				EditorFrame->ViewportConfigData->ToggleMaximize( CurViewport.ViewportWindow->Viewport );
				break;
			}
		}
	}

	GEditorModeTools().ActivateMode( EM_Default );

	// Show the startup tip dialog
	StartupTipDialog = new WxStartupTipDialog( EditorFrame);

	UBOOL bDoAutomatedMapBuild = FALSE;

	// see if the user specified an initial map to load on the commandline
	const TCHAR* ParsedCmdLine = appCmdLine();

	// If a map was loaded via command line, see if the user is attempting to do an automated build/submit
	bDoAutomatedMapBuild = ParseParam( ParsedCmdLine, TEXT("AutomatedMapBuild") );

	if( !bDoAutomatedMapBuild ) 
	{
		UBOOL bShowWelcomeScreen = FALSE;

		// See if the welcome screen should be displayed based on user settings, and if so, display it
		// We only show the welcome screen on UDK builds
#if WITH_MANAGED_CODE && UDK
		bShowWelcomeScreen = FWelcomeScreen::ShouldDisplayWelcomeScreenAtStartup();
		if ( bShowWelcomeScreen )
		{
			FWelcomeScreen::DisplayWelcomeScreen();
		}
#endif // #if WITH_MANAGED_CODE

		// Don't show the startup tip dialog if already displaying the welcome dialog screen
		if( !bShowWelcomeScreen && StartupTipDialog->GetShowAtStartup() )
		{
			StartupTipDialog->Show();
		}
	}



	// Tick the client so it can deliver deferred window messages.
	// This is needed to ensure the D3D RHI has been initialized when the map is loaded and its scene's static mesh draw lists created.
	// When the static mesh draw lists are created, it creates the RHI bound shader states, so the RHI needs to be initialized.
	check(GEngine);
	check(GEngine->Client);
	GEngine->Client->Tick(0.0f);

	UBOOL bMapLoaded = FALSE;
	
	FString ParsedMapName;
	if ( ParseToken(ParsedCmdLine, ParsedMapName, FALSE) )
	{
		FFilename InitialMapName;
		// if the specified package exists
		if ( GPackageFileCache->FindPackageFile(*ParsedMapName, NULL, (FString&)InitialMapName) &&

			// and it's a valid map file
			SupportedMapExtensions.Contains( InitialMapName.GetExtension()) )
		{
			FEditorFileUtils::LoadMap(InitialMapName);
			bMapLoaded = TRUE;

		}
	}

	if( !bDoAutomatedMapBuild )
	{
		if (FEditorFileUtils::GetSimpleMapName().Len() > 0 &&
			!bMapLoaded && GEditor && GEditor->GetUserSettings().bLoadSimpleLevelAtStartup)
		{
			FEditorFileUtils::LoadSimpleMapAtStartup();
		}
	}

	GWarn->MapCheck_ShowConditionally();

	DOUBLE OnInitEndTime = appSeconds();
	debugf(TEXT("TIMER ALL OF INIT : [%f]"), OnInitEndTime-OnInitStartTime);

#if HAVE_SCC && WITH_MANAGED_CODE
	if ( bDoAutomatedMapBuild )
	{
		// If the user is doing an automated build, configure the settings for the build appropriately
		FEditorBuildUtils::FEditorAutomatedBuildSettings AutomatedBuildSettings;
		
		// Assume the user doesn't want to add files not in the P4 depot to Perforce, they can specify that they
		// want to via commandline option
		AutomatedBuildSettings.bAutoAddNewFiles = FALSE;
		AutomatedBuildSettings.bCheckInPackages = FALSE;

		// Shut down the editor upon completion of the automated build
		AutomatedBuildSettings.bShutdownEditorOnCompletion = TRUE;

		if ( FContentBrowser::IsInitialized() )
		{
			AutomatedBuildSettings.SCCEventListener = &( FContentBrowser::GetActiveInstance() );
		}
		
		// Assume that save, SCC, and new map errors all result in failure and don't submit anything if any of those occur. If the user
		// wants, they can explicitly ignore each warning type via commandline option
		AutomatedBuildSettings.BuildErrorBehavior = FEditorBuildUtils::ABB_ProceedOnError;
		AutomatedBuildSettings.FailedToSaveBehavior = FEditorBuildUtils::ABB_FailOnError;
		AutomatedBuildSettings.NewMapBehavior = FEditorBuildUtils::ABB_FailOnError;
		AutomatedBuildSettings.UnableToCheckoutFilesBehavior = FEditorBuildUtils::ABB_FailOnError;

		// Attempt to parse the changelist description from the commandline
		FString ParsedString;
		if ( Parse( ParsedCmdLine, TEXT("CLDesc="), ParsedString ) )
		{
			AutomatedBuildSettings.ChangeDescription = ParsedString;
		}
		
		// See if the user has specified any additional commandline options and set the build setting appropriately if so
		UBOOL ParsedBool;
		if ( ParseUBOOL( ParsedCmdLine, TEXT("IgnoreBuildErrors="), ParsedBool ) )
		{
			AutomatedBuildSettings.BuildErrorBehavior = ParsedBool ? FEditorBuildUtils::ABB_ProceedOnError : FEditorBuildUtils::ABB_FailOnError;
		}
		if ( ParseUBOOL( ParsedCmdLine, TEXT("IgnoreSCCErrors="), ParsedBool ) )
		{
			AutomatedBuildSettings.UnableToCheckoutFilesBehavior = ParsedBool ? FEditorBuildUtils::ABB_ProceedOnError : FEditorBuildUtils::ABB_FailOnError;
		}
		if ( ParseUBOOL( ParsedCmdLine, TEXT("IgnoreMapSaveErrors="), ParsedBool ) )
		{
			AutomatedBuildSettings.FailedToSaveBehavior = ParsedBool ? FEditorBuildUtils::ABB_ProceedOnError : FEditorBuildUtils::ABB_FailOnError;
		}
		if ( ParseUBOOL( ParsedCmdLine, TEXT("AddFilesNotInDepot="), ParsedBool ) )
		{
			AutomatedBuildSettings.bAutoAddNewFiles = ParsedBool;
		}	

		// Kick off the automated build
		FString ErrorMessages;
		FEditorBuildUtils::EditorAutomatedBuildAndSubmit( AutomatedBuildSettings, ErrorMessages );
	}
#endif

	return 1;
}

int WxUnrealEdApp::OnExit()
{
	// Save out default file directories
	GConfig->SetString( TEXT("Directories2"), TEXT("UNR"),				*GApp->LastDir[LD_UNR],					GEditorUserSettingsIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("BRUSH"),			*GApp->LastDir[LD_BRUSH],				GEditorUserSettingsIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("2DS"),				*GApp->LastDir[LD_2DS],					GEditorUserSettingsIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("PSA"),				*GApp->LastDir[LD_PSA],					GEditorUserSettingsIni );
#if WITH_FBX
	GConfig->SetString( TEXT("Directories2"), TEXT("FBXAnim"),			*GApp->LastDir[LD_FBX_ANIM],			GEditorUserSettingsIni );
#endif // WITH_FBX
	GConfig->SetString( TEXT("Directories2"), TEXT("GenericImport"),	*GApp->LastDir[LD_GENERIC_IMPORT],		GEditorUserSettingsIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("GenericExport"),	*GApp->LastDir[LD_GENERIC_EXPORT],		GEditorUserSettingsIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("GenericOpen"),		*GApp->LastDir[LD_GENERIC_OPEN],		GEditorUserSettingsIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("GenericSave"),		*GApp->LastDir[LD_GENERIC_SAVE],		GEditorUserSettingsIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("MeshImportExport"),	*GApp->LastDir[LD_MESH_IMPORT_EXPORT],	GEditorUserSettingsIni );

	// Unregister all events
	GCallbackEvent->UnregisterAll(this);


#if WITH_MANAGED_CODE
	// Shutdown the game asset database
	FGameAssetDatabase::Destroy();
#endif


#if HAVE_SCC
	FSourceControl::Close();
#endif
#if WITH_MANAGED_CODE
	CloseColorPickers();
	FConsolidateWindow::Shutdown();
	FWelcomeScreen::Shutdown();
	FNewMapScreen::Shutdown();
	CloseFileSystemNotification();
#endif

	if( GLogConsole )
	{
		GLogConsole->Show( FALSE );
	}

	return WxLaunchApp::OnExit();
}

/**
 * Performs any required cleanup in the case of a fatal error.
 */
void WxUnrealEdApp::ShutdownAfterError()
{
#if WITH_MANAGED_CODE
	CloseColorPickers();
	FConsolidateWindow::Shutdown();
	FWelcomeScreen::Shutdown();
	FNewMapScreen::Shutdown();
	CloseFileSystemNotification();
#endif

	WxLaunchApp::ShutdownAfterError();

#if HAVE_SCC
	FSourceControl::Close();
#endif
}

/** Generate a mapping of wx keys to unreal key names */
void WxUnrealEdApp::GenerateWxToUnrealKeyMap()
{
	WxKeyToUnrealKeyMap.Set(WXK_LBUTTON,KEY_LeftMouseButton);
	WxKeyToUnrealKeyMap.Set(WXK_RBUTTON,KEY_RightMouseButton);
	WxKeyToUnrealKeyMap.Set(WXK_MBUTTON,KEY_MiddleMouseButton);

	WxKeyToUnrealKeyMap.Set(WXK_BACK,KEY_BackSpace);
	WxKeyToUnrealKeyMap.Set(WXK_TAB,KEY_Tab);
	WxKeyToUnrealKeyMap.Set(WXK_RETURN,KEY_Enter);
	WxKeyToUnrealKeyMap.Set(WXK_PAUSE,KEY_Pause);

	WxKeyToUnrealKeyMap.Set(WXK_CAPITAL,KEY_CapsLock);
	WxKeyToUnrealKeyMap.Set(WXK_ESCAPE,KEY_Escape);
	WxKeyToUnrealKeyMap.Set(WXK_SPACE,KEY_SpaceBar);
	WxKeyToUnrealKeyMap.Set(WXK_PRIOR,KEY_PageUp);
	WxKeyToUnrealKeyMap.Set(WXK_NEXT,KEY_PageDown);
	WxKeyToUnrealKeyMap.Set(WXK_END,KEY_End);
	WxKeyToUnrealKeyMap.Set(WXK_HOME,KEY_Home);

	WxKeyToUnrealKeyMap.Set(WXK_LEFT,KEY_Left);
	WxKeyToUnrealKeyMap.Set(WXK_UP,KEY_Up);
	WxKeyToUnrealKeyMap.Set(WXK_RIGHT,KEY_Right);
	WxKeyToUnrealKeyMap.Set(WXK_DOWN,KEY_Down);

	WxKeyToUnrealKeyMap.Set(WXK_INSERT,KEY_Insert);
	WxKeyToUnrealKeyMap.Set(WXK_DELETE,KEY_Delete);

	WxKeyToUnrealKeyMap.Set(0x30,KEY_Zero);
	WxKeyToUnrealKeyMap.Set(0x31,KEY_One);
	WxKeyToUnrealKeyMap.Set(0x32,KEY_Two);
	WxKeyToUnrealKeyMap.Set(0x33,KEY_Three);
	WxKeyToUnrealKeyMap.Set(0x34,KEY_Four);
	WxKeyToUnrealKeyMap.Set(0x35,KEY_Five);
	WxKeyToUnrealKeyMap.Set(0x36,KEY_Six);
	WxKeyToUnrealKeyMap.Set(0x37,KEY_Seven);
	WxKeyToUnrealKeyMap.Set(0x38,KEY_Eight);
	WxKeyToUnrealKeyMap.Set(0x39,KEY_Nine);

	WxKeyToUnrealKeyMap.Set(0x41,KEY_A);
	WxKeyToUnrealKeyMap.Set(0x42,KEY_B);
	WxKeyToUnrealKeyMap.Set(0x43,KEY_C);
	WxKeyToUnrealKeyMap.Set(0x44,KEY_D);
	WxKeyToUnrealKeyMap.Set(0x45,KEY_E);
	WxKeyToUnrealKeyMap.Set(0x46,KEY_F);
	WxKeyToUnrealKeyMap.Set(0x47,KEY_G);
	WxKeyToUnrealKeyMap.Set(0x48,KEY_H);
	WxKeyToUnrealKeyMap.Set(0x49,KEY_I);
	WxKeyToUnrealKeyMap.Set(0x4A,KEY_J);
	WxKeyToUnrealKeyMap.Set(0x4B,KEY_K);
	WxKeyToUnrealKeyMap.Set(0x4C,KEY_L);
	WxKeyToUnrealKeyMap.Set(0x4D,KEY_M);
	WxKeyToUnrealKeyMap.Set(0x4E,KEY_N);
	WxKeyToUnrealKeyMap.Set(0x4F,KEY_O);
	WxKeyToUnrealKeyMap.Set(0x50,KEY_P);
	WxKeyToUnrealKeyMap.Set(0x51,KEY_Q);
	WxKeyToUnrealKeyMap.Set(0x52,KEY_R);
	WxKeyToUnrealKeyMap.Set(0x53,KEY_S);
	WxKeyToUnrealKeyMap.Set(0x54,KEY_T);
	WxKeyToUnrealKeyMap.Set(0x55,KEY_U);
	WxKeyToUnrealKeyMap.Set(0x56,KEY_V);
	WxKeyToUnrealKeyMap.Set(0x57,KEY_W);
	WxKeyToUnrealKeyMap.Set(0x58,KEY_X);
	WxKeyToUnrealKeyMap.Set(0x59,KEY_Y);
	WxKeyToUnrealKeyMap.Set(0x5A,KEY_Z);

	WxKeyToUnrealKeyMap.Set(WXK_NUMPAD0,KEY_NumPadZero);
	WxKeyToUnrealKeyMap.Set(WXK_NUMPAD1,KEY_NumPadOne);
	WxKeyToUnrealKeyMap.Set(WXK_NUMPAD2,KEY_NumPadTwo);
	WxKeyToUnrealKeyMap.Set(WXK_NUMPAD3,KEY_NumPadThree);
	WxKeyToUnrealKeyMap.Set(WXK_NUMPAD4,KEY_NumPadFour);
	WxKeyToUnrealKeyMap.Set(WXK_NUMPAD5,KEY_NumPadFive);
	WxKeyToUnrealKeyMap.Set(WXK_NUMPAD6,KEY_NumPadSix);
	WxKeyToUnrealKeyMap.Set(WXK_NUMPAD7,KEY_NumPadSeven);
	WxKeyToUnrealKeyMap.Set(WXK_NUMPAD8,KEY_NumPadEight);
	WxKeyToUnrealKeyMap.Set(WXK_NUMPAD9,KEY_NumPadNine);

	WxKeyToUnrealKeyMap.Set(WXK_MULTIPLY,KEY_Multiply);

	WxKeyToUnrealKeyMap.Set(WXK_ADD,KEY_Add);
	WxKeyToUnrealKeyMap.Set(WXK_NUMPAD_ADD,KEY_Add);

	WxKeyToUnrealKeyMap.Set(WXK_SUBTRACT,KEY_Subtract);
	WxKeyToUnrealKeyMap.Set(WXK_NUMPAD_SUBTRACT,KEY_Subtract);

	WxKeyToUnrealKeyMap.Set(WXK_DECIMAL,KEY_Decimal);
	WxKeyToUnrealKeyMap.Set(WXK_DIVIDE,KEY_Divide);

	WxKeyToUnrealKeyMap.Set(WXK_F1,KEY_F1);
	WxKeyToUnrealKeyMap.Set(WXK_F2,KEY_F2);
	WxKeyToUnrealKeyMap.Set(WXK_F3,KEY_F3);
	WxKeyToUnrealKeyMap.Set(WXK_F4,KEY_F4);
	WxKeyToUnrealKeyMap.Set(WXK_F5,KEY_F5);
	WxKeyToUnrealKeyMap.Set(WXK_F6,KEY_F6);
	WxKeyToUnrealKeyMap.Set(WXK_F7,KEY_F7);
	WxKeyToUnrealKeyMap.Set(WXK_F8,KEY_F8);
	WxKeyToUnrealKeyMap.Set(WXK_F9,KEY_F9);
	WxKeyToUnrealKeyMap.Set(WXK_F10,KEY_F10);
	WxKeyToUnrealKeyMap.Set(WXK_F11,KEY_F11);
	WxKeyToUnrealKeyMap.Set(WXK_F12,KEY_F12);

	WxKeyToUnrealKeyMap.Set(WXK_NUMLOCK,KEY_NumLock);

	WxKeyToUnrealKeyMap.Set(WXK_SCROLL,KEY_ScrollLock);

	WxKeyToUnrealKeyMap.Set(WXK_SHIFT,KEY_LeftShift);
	//WxKeyToUnrealKeyMap.Set(WXK_SHIFT,KEY_RightShift);
	WxKeyToUnrealKeyMap.Set(WXK_CONTROL,KEY_LeftControl);
	//WxKeyToUnrealKeyMap.Set(WXK_RCONTROL,KEY_RightControl);
	WxKeyToUnrealKeyMap.Set(WXK_ALT,KEY_LeftAlt);
	//WxKeyToUnrealKeyMap.Set(WXK_RMENU,KEY_RightAlt);

	WxKeyToUnrealKeyMap.Set(0x3B,KEY_Semicolon);
	WxKeyToUnrealKeyMap.Set(0x2B,KEY_Equals);
	WxKeyToUnrealKeyMap.Set(0x2C,KEY_Comma);
	WxKeyToUnrealKeyMap.Set(0x2D,KEY_Underscore);	// NOTE: Really should be 'Subtract', but UE is weird.
	WxKeyToUnrealKeyMap.Set(0x5F,KEY_Underscore);
	WxKeyToUnrealKeyMap.Set(0x2E,KEY_Period);
	WxKeyToUnrealKeyMap.Set(0x2F,KEY_Slash);
	WxKeyToUnrealKeyMap.Set(0x7E,KEY_Tilde);
	WxKeyToUnrealKeyMap.Set(0x5B,KEY_LeftBracket);
	WxKeyToUnrealKeyMap.Set(0x5C,KEY_Backslash);
	WxKeyToUnrealKeyMap.Set(0x5D,KEY_RightBracket);
	WxKeyToUnrealKeyMap.Set(0x2F,KEY_Quote);
}

/**
 * @return Returns a unreal key name given a wx key event.
 */
FName WxUnrealEdApp::GetKeyName(wxKeyEvent &Event)
{
	FName* KeyName = WxKeyToUnrealKeyMap.Find(Event.GetKeyCode());

	if(KeyName)
	{
		return *KeyName;
	}
	else
	{
		return NAME_None;
	}
}

// Current selection changes (either actors or BSP surfaces).

void WxUnrealEdApp::CB_SelectionChanged()
{
	WxDlgSurfaceProperties*	DlgSurfProp = GetDlgSurfaceProperties();
	check(DlgSurfProp);
	DlgSurfProp->MarkDirty();

#if ENABLE_SIMPLYGON_MESH_PROXIES
	WxDlgCreateMeshProxy*	DlgCreateMeshProxy = GetDlgCreateMeshProxy();
	check(DlgCreateMeshProxy);
	DlgCreateMeshProxy->MarkDirty();
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES

	EditorFrame->UpdateUI();
}


// Shows the surface properties dialog

void WxUnrealEdApp::CB_SurfProps()
{
	WxDlgSurfaceProperties*	DlgSurfProp = GetDlgSurfaceProperties();
	check(DlgSurfProp);
	DlgSurfProp->Show( 1 );
	DlgSurfProp->Raise();
}

/**
 * Displays a property dialog based upon what is currently selected.
 * If no actors are selected, but one or more BSP surfaces are, the surface property dialog
 * is displayed. If any actors are selected, the actor property dialog is displayed. If no
 * actors or BSP surfaces are selected, no dialog is displayed.
 */
void WxUnrealEdApp::CB_SelectedProps()
{
	// Display the actor properties dialog if any actors are selected at all
	if ( GUnrealEd->GetSelectedActorCount() > 0 )
	{
		GUnrealEd->ShowActorProperties();
	}

	// If no actors are selected, find out if any BSP surfaces are selected, and if so, display the surface properties dialog
	else if ( GWorld )
	{
		UBOOL bFoundSelectedBSP = FALSE;
		for ( TArray<ULevel*>::TConstIterator LevelIterator( GWorld->Levels ); LevelIterator && !bFoundSelectedBSP; ++LevelIterator )
		{
			const UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TConstIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator && !bFoundSelectedBSP; ++SurfaceIterator )
			{
				const FBspSurf& CurSurface = *SurfaceIterator;
				if ( CurSurface.PolyFlags & PF_Selected )
				{
					bFoundSelectedBSP = TRUE;
					break;
				}
			}
		}
		
		// Found a BSP surface that was selected, so display the surface properties
		if ( bFoundSelectedBSP )
		{
			WxDlgSurfaceProperties*	DlgSurfProp = GetDlgSurfaceProperties();
			check(DlgSurfProp);

			DlgSurfProp->Show( 1 );
			DlgSurfProp->Raise();
		}
	}
}

// Called whenever the user changes the camera mode

void WxUnrealEdApp::CB_CameraModeChanged()
{
}

// Called whenever an actor has one of it's properties changed

void WxUnrealEdApp::CB_ActorPropertiesChanged()
{
}

// Called whenever an object has its properties changed

void WxUnrealEdApp::CB_ObjectPropertyChanged(UObject* Object)
{
	wxWindow* FocusWindow = wxWindow::FindFocus();

	// all non-active actor prop windows that have this actor need to refresh
	for (INT WindowIndex = 0; WindowIndex < GPropertyWindowManager->PropertyWindows.Num(); WindowIndex++ )
	{
		// CurrentPropertyWindow denotes the current property window, which we don't update
		// because we don't want to destroy the window out from under it.
		WxPropertyWindow* TempPropertyWindow = GPropertyWindowManager->PropertyWindows(WindowIndex);

		UBOOL bHasFocus = IsChildWindowOf(FocusWindow, TempPropertyWindow);
		UBOOL bInUse    = TempPropertyWindow->GetActiveCallbackCount() > 0;

		if (!(bHasFocus || bInUse))
		{
			TempPropertyWindow->Rebuild(Object);
		}
	}
}

void WxUnrealEdApp::CB_ForcePropertyWindowRebuild(UObject* Object)
{
	// all non-active actor prop windows that have this actor need to refresh
	for (INT WindowIndex = 0; WindowIndex < GPropertyWindowManager->PropertyWindows.Num(); WindowIndex++ )
	{
		// CurrentPropertyWindow denotes the current property window, which we don't update
		// because we don't want to destroy the window out from under it.
		WxPropertyWindow* TempPropertyWindow = GPropertyWindowManager->PropertyWindows(WindowIndex);

		if (TempPropertyWindow->ContainsObject(Object))
		{
			TempPropertyWindow->RequestReconnectToData();
		}
	}
}


void WxUnrealEdApp::CB_RefreshPropertyWindows()
{
	for (INT WindowIndex = 0; WindowIndex < GPropertyWindowManager->PropertyWindows.Num(); WindowIndex++ )
	{
		GPropertyWindowManager->PropertyWindows(WindowIndex)->Refresh();
	}
}

void WxUnrealEdApp::CB_DeferredRefreshPropertyWindows()
{
	for (INT WindowIndex = 0; WindowIndex < GPropertyWindowManager->PropertyWindows.Num(); WindowIndex++ )
	{
		GPropertyWindowManager->PropertyWindows(WindowIndex)->DeferredRefresh();
	}
}


void WxUnrealEdApp::CB_DisplayLoadErrors()
{
	if (!DlgLoadErrors)
	{
		DlgLoadErrors = new WxDlgLoadErrors( EditorFrame );
	}
	DlgLoadErrors->Update();
	DlgLoadErrors->Show();
}

void WxUnrealEdApp::CB_RefreshEditor()
{

	GCallbackEvent->Send( CALLBACK_RefreshEditor_AllBrowsers );

	if( DlgActorSearch )
	{
		DlgActorSearch->UpdateResults();
	}
}

// Tells the editor that something has been done to change the map.  Can be
// anything from loading a whole new map to changing the BSP.

void WxUnrealEdApp::CB_MapChange( DWORD InFlags )
{
	if (InFlags == MapChangeEventFlags::NewMap)
	{
		for( INT CurViewportIndex = 0; CurViewportIndex < EditorFrame->ViewportConfigData->GetViewportCount(); ++CurViewportIndex )
		{
			FEditorLevelViewportClient* CurLevelViewport =
				EditorFrame->ViewportConfigData->AccessViewport( CurViewportIndex ).ViewportWindow;
			if(CurLevelViewport)
			{
				CurLevelViewport->ResetCamera();
			}
		}
	}

	// Clear property coloration settings.
	const FString EmptyString;
	GEditor->SetPropertyColorationTarget( EmptyString, NULL, NULL, NULL );

	// Rebuild the collision hash if this map change is something major ("new", "open", etc).
	// Minor things like brush subtraction will set it to "0".

	if( InFlags != MapChangeEventFlags::Default )
	{	
		GEditor->ClearComponents();
		GWorld->CleanupWorld();
	}

	GEditor->UpdateComponents();

	GEditorModeTools().MapChangeNotify();

	if ( GEditor->LevelProperties && (InFlags&MapChangeEventFlags::MapRebuild) == 0 )
	{
		GEditor->LevelProperties->SetObject( NULL, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories  );
		GEditor->LevelProperties->Show( FALSE );
		GEditor->LevelProperties->ClearLastFocused();
	}

	GPropertyWindowManager->ClearReferencesAndHide();
	if ( (InFlags&MapChangeEventFlags::MapRebuild) != 0 )
	{
		GUnrealEd->UpdatePropertyWindows();
	}

	CB_RefreshEditor();

	// Only reset the auto save timer if we've created or loaded a new map
	if( InFlags & MapChangeEventFlags::NewMap )
	{
		GEditor->ResetAutosaveTimer();
	}
	else if( InFlags & MapChangeEventFlags::WorldTornDown )
	{
		// Purge map packages that are about to be unloaded from the checkout data
		GUnrealEd->PurgeOldPackageCheckoutData();
	}
}

void WxUnrealEdApp::CB_RedrawAllViewports()
{
	GUnrealEd->RedrawAllViewports();
}

void WxUnrealEdApp::CB_Undo()
{
	GEditorModeTools().PostUndo();
}

void WxUnrealEdApp::CB_EndPIE()
{
	if ( GDebugToolExec )
	{
		((FDebugToolExec*)GDebugToolExec)->CleanUpAfterPIE();
	}
}

void WxUnrealEdApp::CB_EditorModeEnter(const FEdMode& InEdMode)
{
	// Hide all dialogs.
	if ( TerrainEditor )
	{
		TerrainEditor->Show( false );
	}

	if ( DlgGeometryTools )
	{
		DlgGeometryTools->Show( false );
	}

	if( DlgStaticMeshTools )
	{
		DlgStaticMeshTools->Show( false );
	}

	
	TArray<FEdMode*> ActiveModes;
	GEditorModeTools().GetActiveModes( ActiveModes );

	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		INT CurrentModeID = ActiveModes(ModeIndex)->GetID();
	switch( CurrentModeID )
	{
	case EM_TerrainEdit:
		if ( !TerrainEditor )
		{
			TerrainEditor = new WxTerrainEditor( EditorFrame );
		}

		TerrainEditor->Show( true );
		break;
	case EM_Geometry:
		if ( !DlgGeometryTools )
		{
			DlgGeometryTools = new WxDlgGeometryTools( EditorFrame );
		}

		DlgGeometryTools->Show( true  );
		break;
	case EM_StaticMesh:
		if ( !DlgStaticMeshTools )
		{
			DlgStaticMeshTools = new WxDlgStaticMeshTools( EditorFrame );
		}

		DlgStaticMeshTools->Show( true  );
		break;
	default:
		break;
	}
	}


	wxStatusBar* StatusBar = EditorFrame->GetStatusBar();
	if ( StatusBar )
	{
		StatusBar->Refresh();
	}
}

void WxUnrealEdApp::CB_EditorModeExit(const FEdMode& InEdMode)
{

	INT CurrentModeID = InEdMode.GetID();
	switch( CurrentModeID )
	{
	case EM_TerrainEdit:
		check(TerrainEditor);
		TerrainEditor->Show( false );
		break;
	case EM_Geometry:
		check(DlgGeometryTools);
		DlgGeometryTools->Show( false  );
		break;
	case EM_StaticMesh:
		check(DlgStaticMeshTools);
		DlgStaticMeshTools->Show( false  );
		break;
	default:
		break;
	}

	wxStatusBar* StatusBar = EditorFrame->GetStatusBar();
	if ( StatusBar )
	{
		StatusBar->Refresh();
	}
}
/**
 * Update the given actor's visibility, since it's layer membership changed
 */
void WxUnrealEdApp::CB_LayersHaveChanged(AActor* Actor)
{
	FLayerUtils::UpdateActorAllViewsVisibility(Actor);
}

/** Called right before unit testing is about to begin */
void WxUnrealEdApp::CB_PreUnitTesting()
{
#if HAVE_SCC
	// Shut down SCC if it's enabled, as unit tests shouldn't be allowed to make any modifications to source control
	if ( FSourceControl::IsEnabled() )
	{
		FSourceControl::Close();
	}
#endif // #if HAVE_SCC
}

/** Called right after unit testing concludes */
void WxUnrealEdApp::CB_PostUnitTesting()
{
#if HAVE_SCC
	// Re-enable source control
	FSourceControl::Init();
#endif // #if HAVE_SCC
}


/**
 * Routes the event to the appropriate handlers
 *
 * @param InType the event that was fired
 */
void WxUnrealEdApp::Send(ECallbackEventType InType)
{
	switch( InType )
	{
		case CALLBACK_SelChange:
			CB_SelectionChanged();
			break;
		case CALLBACK_SurfProps:
			CB_SurfProps();
			break;
		case CALLBACK_SelectedProps:
			CB_SelectedProps();
			break;
		case CALLBACK_ActorPropertiesChange:
			CB_ActorPropertiesChanged();
			break;
		case CALLBACK_DisplayLoadErrors:
			CB_DisplayLoadErrors();
			break;
		case CALLBACK_RedrawAllViewports:
			CB_RedrawAllViewports();
			break;
		case CALLBACK_UpdateUI:
			EditorFrame->UpdateUI();
			break;
		case CALLBACK_Undo:
			GApp->CB_Undo();
			break;
		case CALLBACK_EndPIE:
			GApp->CB_EndPIE();
			break;
		case CALLBACK_RefreshPropertyWindows:
			CB_RefreshPropertyWindows();
			break;
		case CALLBACK_DeferredRefreshPropertyWindows:
			CB_DeferredRefreshPropertyWindows();
			break;
		case CALLBACK_RefreshEditor:
			CB_RefreshEditor();
			break;
		case CALLBACK_ProcBuildingFixLODs:
			CB_FixProcBuildingLODs();
			break;
#if USE_UNIT_TESTS
		case CALLBACK_PreUnitTesting:
			CB_PreUnitTesting();
			break;
		case CALLBACK_PostUnitTesting:
			CB_PostUnitTesting();
			break;
#endif // #if USE_UNIT_TESTS
	}
}

/**
 * Routes the event to the appropriate handlers
 *
 * @param InType the event that was fired
 * @param InFlags the flags for this event
 */
void WxUnrealEdApp::Send(ECallbackEventType InType,DWORD InFlag)
{
	switch( InType )
	{
		case CALLBACK_MapChange:
			CB_MapChange( InFlag );
			break;
		case CALLBACK_ChangeEditorMode:
			{
				const EEditorMode RequestedEditorMode = (EEditorMode)InFlag;
				GEditorModeTools().ActivateMode( RequestedEditorMode, TRUE );
			}
			break;
	}
}


/**
 * Routes the event to the appropriate handlers
 *
 * @param InObject the relevant object for this event
 */
void WxUnrealEdApp::Send(ECallbackEventType InType, UObject* InObject)
{
	switch( InType )
	{
		case CALLBACK_ObjectPropertyChanged:
			CB_ObjectPropertyChanged(InObject);
			break;

		case CALLBACK_ForcePropertyWindowRebuild:
			CB_ForcePropertyWindowRebuild(InObject);
			break;

		case CALLBACK_LayersHaveChanged:
			// update the actor's per-view visibility
			CB_LayersHaveChanged(Cast<AActor>(InObject));
			break;
			
		case CALLBACK_ProcBuildingUpdate:
			CB_ProcBuildingUpdate(Cast<AProcBuilding>(InObject));
			break;

		case CALLBACK_SelChange:
			// Handle selection changed passed in with UObjects
			CB_SelectionChanged();
			break;

		case CALLBACK_MobileFlattenedTextureUpdate:
			CB_UpdateMobileFlattenedTexture(CastChecked<UMaterialInterface>(InObject));
			break;		

		case CALLBACK_BaseSMActorOnProcBuilding:
			CB_AutoBaseSMActorToProcBuilding(CastChecked<AStaticMeshActor>(InObject));
			break;
	}
}

/**
 * Routes the event to the appropriate handlers
 *
 * @param InType the event that was fired
 * @param InEdMode the FEdMode that is changing
 */
void WxUnrealEdApp::Send(ECallbackEventType InType, FEdMode* InEdMode)
{
	switch( InType )
	{
		case CALLBACK_EditorModeEnter:
		{
			check( InEdMode );
			CB_EditorModeEnter( *InEdMode );
			break;
		}
		case CALLBACK_EditorModeExit:
		{
			check( InEdMode );
			CB_EditorModeExit( *InEdMode );
			break;
		}
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
void WxUnrealEdApp::Send(ECallbackEventType InType,const FString& InString, UObject* InObject)
{
	switch (InType)
	{
		// save the shader cache when we save a package
		case CALLBACK_PackageSaved:
		{
			SaveLocalShaderCaches();
			break;
		}
	}
}

/**
 * Accessor for WxDlgActorSearch Window
 */
WxDlgActorSearch* WxUnrealEdApp::GetDlgActorSearch(void)
{
	if (!DlgActorSearch)
	{
		DlgActorSearch = new WxDlgActorSearch( EditorFrame );
		DlgActorSearch->Show(false);
	}
	return DlgActorSearch;
}

/**
 * Accessor for WxDlgMapCheck Window
 */
WxDlgMapCheck* WxUnrealEdApp::GetDlgMapCheck(void)
{
	if (!DlgMapCheck)
	{
		DlgMapCheck = new WxDlgMapCheck( EditorFrame );
		DlgMapCheck->Initialize();
		DlgMapCheck->Show(false);
	}
	return DlgMapCheck;
}
/**
 * Accessor for WxDlgLightingResults Window
 */
WxDlgLightingResults* WxUnrealEdApp::GetDlgLightingResults(void)
{
	if (!DlgLightingResults)
	{
		DlgLightingResults = new WxDlgLightingResults(EditorFrame);
		DlgLightingResults->Initialize();
		DlgLightingResults->Show(false);
	}
	return DlgLightingResults;
}
/**
 * Accessor for WxDlgLightingBuildInfo Window
 */
WxDlgLightingBuildInfo* WxUnrealEdApp::GetDlgLightingBuildInfo(void)
{
	if (!DlgLightingBuildInfo)
	{
		DlgLightingBuildInfo = new WxDlgLightingBuildInfo(EditorFrame);
		DlgLightingBuildInfo->Show(false);
	}
	return DlgLightingBuildInfo;
}
/**
 * Accessor for WxDlgStaticMeshLightingInfo Window
 */
WxDlgStaticMeshLightingInfo* WxUnrealEdApp::GetDlgStaticMeshLightingInfo(void)
{
	if (!DlgStaticMeshLightingInfo)
	{
		DlgStaticMeshLightingInfo = new WxDlgStaticMeshLightingInfo(EditorFrame);
		// All initialization of this dialog is done in the constructor...
		DlgStaticMeshLightingInfo->Show(false);
	}
	return DlgStaticMeshLightingInfo;
}
/**
 * Accessor for WxDlgActorFactory Window
 */
WxDlgActorFactory* WxUnrealEdApp::GetDlgActorFactory(void)
{
	if (!DlgActorFactory)
	{
		DlgActorFactory = new WxDlgActorFactory( );
		DlgActorFactory->Show(false);
	}
	return DlgActorFactory;
}
/**
 * Accessor for WxDlgTransform Window
 */
WxDlgTransform* WxUnrealEdApp::GetDlgTransform(void)
{
	if (!DlgTransform)
	{
		DlgTransform = new WxDlgTransform( EditorFrame );
		DlgTransform->Show(false);
	}
	return DlgTransform;
}
/*
 * Accessor for WxConvexDecompOptions Window
 */
WxConvexDecompOptions* WxUnrealEdApp::GetDlgAutoConvexCollision(void)
{
	if (!DlgAutoConvexCollision)
	{
		DlgAutoConvexCollision = new WxConvexDecompOptions( AutoConvexHelper, EditorFrame, TRUE, TRUE );
		DlgAutoConvexCollision->Show(false);
	}
	return DlgAutoConvexCollision;
}
/**
 * Accessor for WxDlgBindHotkeys Window
 */
WxDlgBindHotkeys* WxUnrealEdApp::GetDlgBindHotkeys(void)
{
	if (!DlgBindHotkeys)
	{
		DlgBindHotkeys = new WxDlgBindHotkeys( EditorFrame );
		DlgBindHotkeys->Show(false);
	}
	return DlgBindHotkeys;
}
/**
 * Accessor for WxDlgSurfaceProperties Window
 */
WxDlgSurfaceProperties* WxUnrealEdApp::GetDlgSurfaceProperties(void)
{
	if (!DlgSurfaceProperties)
	{
		wxWindow* LastFocus = wxWindow::FindFocus();
		DlgSurfaceProperties = new WxDlgSurfaceProperties();
		//gets around issue of new window taking focus when it may have been created in the middle of important work (loading a map)
		DlgSurfaceProperties->Hide();
		if (LastFocus)
		{
			LastFocus->SetFocus();
		}
	}
	return DlgSurfaceProperties;
}
/**
 * Accessor for WxDlgDensityRenderingOptions Window
 */
WxDlgDensityRenderingOptions* WxUnrealEdApp::GetDlgDensityRenderingOptions(void)
{
	if (!DlgDensityRenderingOptions)
	{
		DlgDensityRenderingOptions = new WxDlgDensityRenderingOptions(EditorFrame);
		DlgDensityRenderingOptions->Show(false);
	}
	return DlgDensityRenderingOptions;
}
#if ENABLE_SIMPLYGON_MESH_PROXIES
/**
 * Accessor for WxDlgCreateMeshProxy Window
 */
WxDlgCreateMeshProxy* WxUnrealEdApp::GetDlgCreateMeshProxy(void)
{
	if (!DlgCreateMeshProxy)
	{
		DlgCreateMeshProxy = new WxDlgCreateMeshProxy(EditorFrame, *LocalizeUnrealEd( "MeshProxy_Create" ));
		DlgCreateMeshProxy->Show(false);
	}
	return DlgCreateMeshProxy;
}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES

/**
* Process key press events
*
* @param	The event to process
*/
void WxUnrealEdApp::KeyPressed(wxKeyEvent& Event)
{
	INT KeyCode = Event.GetKeyCode();
	UBOOL bCtrlPressed = Event.ControlDown();
	UBOOL bShiftPressed = Event.ShiftDown();
	UBOOL bAltPressed = Event.AltDown();
	UBOOL bHotKeyPressed = FALSE;
	FName KeyName(NAME_None);

	// This check fixes a bug when entering characters that are mapped to shortcut
	// keys into a text control in the property window by skipping all the hotkey code when the
	// event originates from a text control.
	if(wxDynamicCast(Event.GetEventObject(), wxTextCtrl) != 0 && !bCtrlPressed && !bAltPressed)
	{
		Event.Skip();
		return;
	}

	UBOOL bWasProcessed = FALSE;

	// Map the keycode to a name that GApp can recognize
	switch( KeyCode )
	{
	case 9:// Tab == 9
		{
			bHotKeyPressed = TRUE;
			KeyName = FName(TEXT("Tab"));
			break;
		}
	case 70:// F == 70
		{
			bHotKeyPressed = TRUE;
			KeyName = FName(TEXT("F"));
			break;
		}
	case 89:// Y == 89
		{
			bHotKeyPressed = TRUE;
			KeyName = FName(TEXT("Y"));
			break;
		}
	case 90:// Z == 90
		{
			bHotKeyPressed = TRUE;
			KeyName = FName(TEXT("Z"));
			break;
		}
	case 350:// F11 == 350
		{
			bHotKeyPressed = TRUE;
			KeyName = FName(TEXT("F11"));
			break;
		}
	case 13:// Enter == 13
	case 370:// Return == 370
		{
			bHotKeyPressed = TRUE;
			KeyName = FName(TEXT("Enter"));
			break;
		}
	case 27: // Escape 
		{
			bHotKeyPressed = TRUE;
			KeyName = FName(TEXT("Escape"));
			break;
		}
	default:
		break;
	}

	if( bHotKeyPressed )
	{// Process global hotkey
		bWasProcessed = GApp->CheckIfGlobalHotkey( KeyName, bCtrlPressed, bShiftPressed, bAltPressed );
	}

	if( !bWasProcessed && AutosaveState == AUTOSAVE_Saving && KeyName == TEXT("Escape"))
	{
		AutosaveState = AUTOSAVE_Cancelled;
		bWasProcessed = TRUE;
	}

	// Change editor modes 
	// If Matinee or Kismet is open do not switch editor modes as this could conflict with typing symbols into edit boxes in Matinee or Kismet using shift+num keys. Todo: need a better method
	if( !bWasProcessed && bShiftPressed && !GEditorModeTools().IsModeActive( EM_InterpEdit ) && GApp->KismetWindows.Num() == 0)
	{
		switch(KeyCode)
		{
		case '1': 
			{
				GEditor->Exec(TEXT("MODE CAMERAMOVE"));
				GEditor->RedrawLevelEditingViewports();
				bWasProcessed = TRUE;
				break;
			}
		case '2': 
			{
				GEditor->Exec(TEXT("MODE GEOMETRY"));
				GEditor->RedrawLevelEditingViewports();
				bWasProcessed = TRUE;
				break;
			}
		case '3': 
			{
				GEditor->Exec(TEXT("MODE TERRAINEDIT"));
				GEditor->RedrawLevelEditingViewports();
				bWasProcessed = TRUE;
				break;
			}
		case '4': 
			{
				GEditor->Exec(TEXT("MODE TEXTURE"));
				GEditor->RedrawLevelEditingViewports();
				bWasProcessed = TRUE;
				break;
			}
		case '5': 
			{
				GEditor->Exec(TEXT("MODE COVEREDIT"));
				GEditor->RedrawLevelEditingViewports();
				bWasProcessed = TRUE;
				break;
			}
		case '6': 
			{
				GEditor->Exec(TEXT("MODE MESHPAINT"));
				GEditor->RedrawLevelEditingViewports();
				bWasProcessed = TRUE;
				break;
			}
		case '7': 
			{
				GEditor->Exec(TEXT("MODE STATICMESH"));
				GEditor->RedrawLevelEditingViewports();
				bWasProcessed = TRUE;
				break;
			}
		}
	}

	Event.Skip(FALSE == bWasProcessed);
}

/**
* Process navigation key press events.  Useful for TAB.
*
* @param	The event to process
*/
void WxUnrealEdApp::NavKeyPressed(wxNavigationKeyEvent& Event)
{
	UBOOL SkipEvent = TRUE;

	if( Event.IsFromTab() )
	{
		FName TabName = FName(TEXT("Tab"));
		UBOOL bWindowChange = Event.IsWindowChange();
		UBOOL bForwardDirection = Event.GetDirection();
		// If Ctrl+Tab was processed, don't tab through the wxWindow input fields
		SkipEvent = !CheckIfGlobalHotkey(TabName, bWindowChange, !bForwardDirection, FALSE /* Don't care about ALT when handling CTRL+TAB */);
	}

	if( SkipEvent )
	{
		Event.Skip();
	}
}

/** 
* Check if the key pressed should process a global hotkey action
*
* @param Character		The key that was pressed
* @param bIsCtrlDown	Is either Ctrl button down
* @param bIsShiftDown	Is either Shift button down
* @param bIsALtDown		Is either Alt button down
*
* @return TRUE is a global hotkey was processed
*/
UBOOL WxUnrealEdApp::CheckIfGlobalHotkey( FName Key, UBOOL bIsCtrlDown, UBOOL bIsShiftDown, UBOOL bIsAltDown )
{
	FString KeyString;
	Key.ToString(KeyString);
	UBOOL returnVal = FALSE;

	if( ( bIsCtrlDown && bIsShiftDown ) && ( (Key == FName(TEXT("F"))) || (Key == FName(TEXT("f"))) ) )
	{// Display the objects in the content browser
#if WITH_MANAGED_CODE
		if( FContentBrowser::IsInitialized() )
		{
			FContentBrowser::GetActiveInstance().GoToSearch();
			returnVal = TRUE;
		}
#endif
	}
	else if ( (Key == FName(TEXT("F11"))) || ( bIsAltDown && ( (Key == FName(TEXT("Enter"))) || (Key == FName(TEXT("Return"))) ) ) )
	{// Fullscreen
		WxEditorFrame* Frame = static_cast<WxEditorFrame*>(GetEditorFrame());
		if( Frame )
		{
			Frame->ShowFullScreen( !(Frame->IsFullScreen()), wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOCAPTION );
			returnVal = TRUE;
		}
	}
	else if( bIsCtrlDown && (Key == FName(TEXT("Tab"))) )
	{// Cycle through content browser tabs
		WxTrackableWindowBase::HandleCtrlTab( GApp->EditorFrame, bIsShiftDown );
		returnVal = TRUE;
	}
	else if (bIsCtrlDown && (Key == FName(TEXT("Z"))))
	{
		WxPropertyWindow* FocusWindow = static_cast<WxPropertyWindow*>(wxWindow::FindFocus());
		if( FocusWindow )
		{
			WxPropertyWindowHost* FocusHost = FocusWindow->GetParentHostWindow();
			if(FocusHost && GPropertyWindowManager->PropertyWindows.ContainsItem(FocusHost->GetPropertyWindowForCallbacks()))
			{
				wxWindow* FocusParent = FocusHost->GetParent();
				if(FocusParent && FocusParent->IsKindOf(CLASSINFO(WxPropertyWindowFrame)))
				{
					GUnrealEd->Exec( TEXT("TRANSACTION UNDO") );
					returnVal = TRUE;
				}
			}
		}
	}
	else if (bIsCtrlDown && (Key == FName(TEXT("Y"))))
	{
		WxPropertyWindow* FocusWindow = static_cast<WxPropertyWindow*>(wxWindow::FindFocus());
		if( FocusWindow )
		{
			WxPropertyWindowHost* FocusHost = FocusWindow->GetParentHostWindow();
			if(FocusHost && GPropertyWindowManager->PropertyWindows.ContainsItem(FocusHost->GetPropertyWindowForCallbacks()))
			{
				wxWindow* FocusParent = FocusHost->GetParent();
				if(FocusParent && FocusParent->IsKindOf(CLASSINFO(WxPropertyWindowFrame)))
				{
					GUnrealEd->Exec( TEXT("TRANSACTION REDO") );
					returnVal = TRUE;
				}
			}
		}
	}
	else if(Key == FName(TEXT("Escape")))
	{// Close focused property window
		WxPropertyWindow* FocusWindow = static_cast<WxPropertyWindow*>(wxWindow::FindFocus());
		if( FocusWindow )
		{
			WxPropertyWindowHost* FocusHost = FocusWindow->GetParentHostWindow();
			if(FocusHost && GPropertyWindowManager->PropertyWindows.ContainsItem(FocusHost->GetPropertyWindowForCallbacks()))
			{
				wxWindow* FocusParent = FocusHost->GetParent();
				if(FocusParent && FocusParent->IsKindOf(CLASSINFO(WxPropertyWindowFrame)))
				{
					FocusParent->Hide();
					returnVal = TRUE;
				}

			}
		}
	}

	return returnVal;
}
