/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Factories.h"
#include "UnPath.h"
#include "BusyCursor.h"
#include "EnginePhysicsClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineAnimClasses.h"
#include "EnginePrefabClasses.h"
#include "EngineSoundClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineParticleClasses.h"
#include "EngineFluidClasses.h"
#include "EngineProcBuildingClasses.h"
#include "AnimationUtils.h"
#include "LevelUtils.h"
#include "UnTerrain.h"
#include "LocalizationExport.h"
#include "ScopedTransaction.h"
#include "SurfaceIterators.h"
#include "BSPOps.h"
#include "UnEdTran.h"
#include "UnConsoleSupportContainer.h"
#include "FileHelpers.h"
#include "EditorLevelUtils.h"
#include "LevelBrowser.h"
#if ENABLE_SIMPLYGON_MESH_PROXIES
#include "DlgCreateMeshProxy.h"
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES

namespace 
{
	/**
	 * A stat group use to track memory usage.
	 */
	class FWaveCluster
	{
	public:
		FWaveCluster() {}
		FWaveCluster(const TCHAR* InName)
			:	Name( InName )
			,	Num( 0 )
			,	Size( 0 )
		{}

		FString Name;
		INT Num;
		INT Size;
	};

	struct FAnimSequenceUsageInfo
	{
		FAnimSequenceUsageInfo( FLOAT InStartOffset, FLOAT InEndOffset, UInterpTrackAnimControl* InAnimControl, INT InTrackKeyIndex )
		: 	StartOffset( InStartOffset )
		,	EndOffset( InEndOffset )
		,	AnimControl( InAnimControl )
		,	TrackKeyIndex( InTrackKeyIndex )
		{}

		FLOAT						StartOffset;
		FLOAT						EndOffset;
		UInterpTrackAnimControl*	AnimControl;
		INT							TrackKeyIndex;
	};
}

/**
* @param		bPreviewOnly		If TRUE, don't actually clear material references.  Useful for e.g. map error checking.
* @param		bLogReferences		If TRUE, write to the log any references that were cleared (brush name and material name).
* @return							The number of surfaces that need cleaning or that were cleaned
*/
static INT CleanBSPMaterials(UBOOL bPreviewOnly, UBOOL bLogBrushes)
{
	// Clear the mark flag the polys of all non-volume, non-builder brushes.
	// Make a list of all brushes that were encountered.
	TArray<ABrush*> Brushes;
	for ( FActorIterator It ; It ; ++It )
	{
		if ( It->IsBrush() && !It->IsVolumeBrush() && !It->IsABuilderBrush() && !It->IsABrushShape() )
		{
			ABrush* Actor = static_cast<ABrush*>( *It );
			if( Actor->Brush && Actor->Brush->Polys )
			{
				for ( INT PolyIndex = 0 ; PolyIndex < Actor->Brush->Polys->Element.Num() ; ++PolyIndex )
				{
					Actor->Brush->Polys->Element(PolyIndex).PolyFlags &= ~PF_EdProcessed;
				}
				Brushes.AddItem( Actor );
			}
		}
	}													

	// Iterate over all surfaces and mark the corresponding brush polys.
	for ( TSurfaceIterator<> It ; It ; ++It )
	{
		if ( It->Actor && It->iBrushPoly != INDEX_NONE )
		{										
			It->Actor->Brush->Polys->Element( It->iBrushPoly ).PolyFlags |= PF_EdProcessed;
		}
	}

	// Go back over all brushes and clear material references on all unmarked polys.
	INT NumRefrencesCleared = 0;
	for ( INT BrushIndex = 0 ; BrushIndex < Brushes.Num() ; ++BrushIndex )
	{
		ABrush* Actor = Brushes(BrushIndex);
		for ( INT PolyIndex = 0 ; PolyIndex < Actor->Brush->Polys->Element.Num() ; ++PolyIndex )
		{
			// If the poly was marked . . .
			if ( (Actor->Brush->Polys->Element(PolyIndex).PolyFlags & PF_EdProcessed) != 0 )
			{
				// . . . simply clear the mark flag.
				Actor->Brush->Polys->Element(PolyIndex).PolyFlags &= ~PF_EdProcessed;
			}
			else
			{
				// This poly wasn't marked, so clear its material reference if one exists.
				UMaterialInterface*& ReferencedMaterial = Actor->Brush->Polys->Element(PolyIndex).Material;
				if ( ReferencedMaterial && ReferencedMaterial != GEngine->DefaultMaterial )
				{
					NumRefrencesCleared++;
					if ( bLogBrushes )
					{
						debugf(TEXT("Cleared %s:%s"), *Actor->GetPathName(), *ReferencedMaterial->GetPathName() );
					}
					if ( !bPreviewOnly )
					{
						Actor->Brush->Polys->Element.ModifyItem(PolyIndex);
						ReferencedMaterial = GEngine->DefaultMaterial;
					}
				}
			}
		}
	}

	return NumRefrencesCleared;
}

/*-----------------------------------------------------------------------------
	UnrealEd safe command line.
-----------------------------------------------------------------------------*/

/**
 * Redraws all editor viewport clients.
 *
 * @param	bInvalidateHitProxies		[opt] If TRUE (the default), invalidates cached hit proxies too.
 */
void UEditorEngine::RedrawAllViewports(UBOOL bInvalidateHitProxies)
{
	for( INT ViewportIndex = 0 ; ViewportIndex < ViewportClients.Num() ; ++ViewportIndex )
	{
		FEditorLevelViewportClient* ViewportClient = ViewportClients(ViewportIndex);
		if ( ViewportClient && ViewportClient->Viewport )
		{
			if ( bInvalidateHitProxies )
			{
				// Invalidate hit proxies and display pixels.
				ViewportClient->Viewport->Invalidate();
			}
			else
			{
				// Invalidate only display pixels.
				ViewportClient->Viewport->InvalidateDisplay();
			}
		}
	}
}

/**
 * Invalidates all viewports parented to the specified view.
 *
 * @param	InParentView				The parent view whose child views should be invalidated.
 * @param	bInvalidateHitProxies		[opt] If TRUE (the default), invalidates cached hit proxies too.
 */
void UEditorEngine::InvalidateChildViewports(FSceneViewStateInterface* InParentView, UBOOL bInvalidateHitProxies)
{
	if ( InParentView )
	{
		// Iterate over viewports and redraw those that have the specified view as a parent.
		for( INT ViewportIndex = 0 ; ViewportIndex < ViewportClients.Num() ; ++ViewportIndex )
		{
			FEditorLevelViewportClient* ViewportClient = ViewportClients(ViewportIndex);
			if ( ViewportClient && ViewportClient->ViewState )
			{
				if ( ViewportClient->ViewState->HasViewParent() &&
					ViewportClient->ViewState->GetViewParent() == InParentView &&
					!ViewportClient->ViewState->IsViewParent() )
				{
					if ( bInvalidateHitProxies )
					{
						// Invalidate hit proxies and display pixels.
						ViewportClient->Viewport->Invalidate();
					}
					else
					{
						// Invalidate only display pixels.
						ViewportClient->Viewport->InvalidateDisplay();
					}
				}
			}
		}
	}
}

//
// Execute a command that is safe for rebuilds.
//
UBOOL UEditorEngine::SafeExec( const TCHAR* InStr, FOutputDevice& Ar )
{
	const TCHAR* Str = InStr;

	// Keep a pointer to the beginning of the string to use for message displaying purposes
	const TCHAR* const FullStr = InStr;

	if( ParseCommand(&Str,TEXT("MACRO")) || ParseCommand(&Str,TEXT("EXEC")) )//oldver (exec)
	{
		appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"),FullStr));
	}
	else if( ParseCommand( &Str, TEXT( "EXECFILE" ) ) )
	{
		// Executes a file that contains a list of commands
		TCHAR FilenameString[ MAX_EDCMD ];
		if( ParseToken( Str, FilenameString, ARRAY_COUNT( FilenameString ), 0 ) )
		{
			ExecFile( FilenameString, Ar );
		}

		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("NEW")) )
	{
		// Generalized object importing.
		EObjectFlags Flags = RF_Public|RF_Standalone;
		if( ParseCommand(&Str,TEXT("STANDALONE")) )
		{
			Flags = RF_Public|RF_Standalone;
		}
		else if( ParseCommand(&Str,TEXT("PUBLIC")) )
		{
			Flags = RF_Public;
		}
		else if( ParseCommand(&Str,TEXT("PRIVATE")) )
		{
			Flags = 0;
		}

		const FString ClassName     = ParseToken(Str,0);
		UClass* Class         = FindObject<UClass>( ANY_PACKAGE, *ClassName );
		if( !Class )
		{
			Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized or missing factor class %s"), *ClassName );
			return TRUE;
		}

		FString  PackageName  = ParentContext ? ParentContext->GetName() : TEXT("");
		FString	 GroupName	  = TEXT("");
		FString  FileName     = TEXT("");
		FString  ObjectName   = TEXT("");
		UClass*  ContextClass = NULL;
		UObject* Context      = NULL;

		Parse( Str, TEXT("Package="), PackageName );
		Parse( Str, TEXT("Group="), GroupName );
		Parse( Str, TEXT("File="), FileName );

		ParseObject( Str, TEXT("ContextClass="), UClass::StaticClass(), *(UObject**)&ContextClass, NULL );
		ParseObject( Str, TEXT("Context="), ContextClass, Context, NULL );

		if ( !Parse( Str, TEXT("Name="), ObjectName ) && FileName != TEXT("") )
		{
			// Deduce object name from filename.
			ObjectName = FileName;
			for( ; ; )
			{
				INT i=ObjectName.InStr(PATH_SEPARATOR);
				if( i==-1 )
				{
					i=ObjectName.InStr(TEXT("/"));
				}
				if( i==-1 )
				{
					break;
				}
				ObjectName = ObjectName.Mid( i+1 );
			}
			if( ObjectName.InStr(TEXT("."))>=0 )
			{
				ObjectName = ObjectName.Left( ObjectName.InStr(TEXT(".")) );
			}
		}

		UFactory* Factory = NULL;
		if( Class->IsChildOf(UFactory::StaticClass()) )
		{
			Factory = ConstructObject<UFactory>( Class );
		}

		UObject* NewObject = NULL;

		// Make sure the user isn't trying to create a class with a factory that doesn't
		// advertise its supported type.
		UClass* FactoryClass = Factory ? Factory->GetSupportedClass() : Class;
		if ( FactoryClass )
		{
			NewObject = UFactory::StaticImportObject
			(
				FactoryClass,
				CreatePackage(NULL,*(GroupName != TEXT("") ? (PackageName+TEXT(".")+GroupName) : PackageName)),
				*ObjectName,
				Flags,
				*FileName,
				Context,
				Factory,
				Str,
				GWarn
			);
		}

		if( !NewObject )
		{
			Ar.Logf( NAME_ExecWarning, TEXT("Failed factoring: %s"), InStr );
		}

		return TRUE;
	}
	else if( ParseCommand( &Str, TEXT("LOAD") ) )
	{
		appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"),FullStr));
	}
	else if( ParseCommand( &Str, TEXT("MESHMAP")) )
	{
		appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"),FullStr));
	}
	else if( ParseCommand(&Str,TEXT("ANIM")) )
	{
		appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"),FullStr));
	}
	else if( ParseCommand(&Str,TEXT("MESH")) )
	{
		appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"),FullStr));
	}
	else if( ParseCommand( &Str, TEXT("AUDIO")) )
	{
		appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"),FullStr));
	}
	else if ( ParseCommand( &Str, TEXT("DumpThumbnailStats") ) )
	{
		UBOOL bShowImageData = ParseCommand(&Str, TEXT("ShowImageData"));
		FArchiveCountMem UncompressedArc(NULL), CompressedArc(NULL);
		INT TotalThumbnailCount=0, UncompressedThumbnailCount=0;
		INT PackagesWithUncompressedThumbnails=0;
		SIZE_T SizeOfNames=0;

		SIZE_T TotalKB = 0;
		for ( TObjectIterator<UPackage> It; It; ++It )
		{

			UPackage* Pkg = *It;
			if ( Pkg->HasThumbnailMap() )
			{
				
				FThumbnailMap& Thumbs = Pkg->AccessThumbnailMap();
				FArchiveCountMem MemArc(NULL);
				MemArc << Thumbs;


				SIZE_T PkgThumbnailFootprint = MemArc.GetMax() / 1024;
				Ar.Logf(TEXT("Pkg %s has %i thumbnails (%i KB)"), *Pkg->GetName(), Thumbs.Num(), PkgThumbnailFootprint);
				
				TotalThumbnailCount += Thumbs.Num();
				TotalKB += PkgThumbnailFootprint;

				if ( bShowImageData )
				{
					UBOOL bHasUncompressedImageData = FALSE;
					for ( TMap<FName,FObjectThumbnail>::TIterator It(Thumbs); It; ++It )
					{
						FName& ThumbName = It.Key();

						FObjectThumbnail& ThumbData = It.Value();
						ThumbData.CountImageBytes_Uncompressed(UncompressedArc);
						ThumbData.CountImageBytes_Compressed(CompressedArc);

						TArray<BYTE>& UncompressedData = ThumbData.AccessImageData();
						if ( UncompressedData.Num() > 0 )
						{
							bHasUncompressedImageData = TRUE;
							UncompressedThumbnailCount++;
						}
					}

					if ( bHasUncompressedImageData )
					{
						PackagesWithUncompressedThumbnails++;
					}
				}
			}
		}

		if ( bShowImageData )
		{
			SIZE_T UncompressedImageSize = UncompressedArc.GetMax() / 1024;
			SIZE_T CompressedImageSize = CompressedArc.GetMax() / 1024;

			Ar.Log(TEXT("Total size of image data:"));
			Ar.Logf(TEXT("%i total thumbnails (%i uncompressed) across %i packages"), TotalThumbnailCount, UncompressedThumbnailCount, PackagesWithUncompressedThumbnails);
			Ar.Logf(TEXT("Total size of compressed image data: %i KB"), CompressedImageSize);
			Ar.Logf(TEXT("Total size of UNcompressed image data: %i KB"), UncompressedImageSize);
		}
		Ar.Logf(TEXT("Total memory required for all package thumbnails: %i KB"), TotalKB);
		return TRUE;
	}
	return FALSE;
}

/*-----------------------------------------------------------------------------
	UnrealEd command line.
-----------------------------------------------------------------------------*/

//@hack: this needs to be cleaned up!
static const TCHAR* GStream = NULL;
static TCHAR TempStr[MAX_SPRINTF], TempFname[MAX_EDCMD], TempName[MAX_EDCMD], Temp[MAX_EDCMD];
static WORD Word2;

UBOOL UEditorEngine::Exec_StaticMesh( const TCHAR* Str, FOutputDevice& Ar )
{
	if(ParseCommand(&Str,TEXT("FROM")))
	{
		if(ParseCommand(&Str,TEXT("SELECTION")))	// STATICMESH FROM SELECTION PACKAGE=<name> NAME=<name>
		{
			FinishAllSnaps();

			FName PkgName = NAME_None;
			Parse( Str, TEXT("PACKAGE="), PkgName );

			FName Name = NAME_None;
			Parse( Str, TEXT("NAME="), Name );

			if( PkgName != NAME_None  )
			{
				UPackage* Pkg = CreatePackage(NULL, *PkgName.ToString());

				FName GroupName = NAME_None;
				if( Parse( Str, TEXT("GROUP="), GroupName ) && GroupName != NAME_None )
				{
					Pkg = CreatePackage(Pkg, *GroupName.ToString());
				}

				UINT NumberOfBrushes = RebuildModelFromBrushes(ConversionTempModel, TRUE, FALSE, FALSE, TRUE );
				bspBuildFPolys(ConversionTempModel, TRUE, 0);

				if (0 < ConversionTempModel->Polys->Element.Num())
				{
					// because we aren't passing in a brush we have to specify whether to move it into local space or 
					// the origin based on whether we used multiple brushes.
					const UBOOL MoveToLocalSpace = (NumberOfBrushes < 2) ? TRUE : FALSE;
					UStaticMesh* NewMesh = CreateStaticMeshFromBrush(Pkg, Name, NULL, ConversionTempModel, MoveToLocalSpace);

					// Refresh
					const DWORD UpdateMask = CBR_UpdatePackageList|CBR_UpdateAssetList;
					GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, UpdateMask, NewMesh));
				}

				// Empty temp model to clear any references to the current level
				ConversionTempModel->EmptyModel(1, 1);
			}

			RedrawLevelEditingViewports();

			return TRUE;
		}
	}
#if !SHIPPING_PC_GAME
	// Not supported on shipped builds because PC cooking strips raw mesh data.
	else if(ParseCommand(&Str,TEXT("TO")))
	{
		if(ParseCommand(&Str,TEXT("BRUSH")))
		{
			const FScopedTransaction Transaction( TEXT("StaticMesh to Brush") );
			GWorld->GetBrush()->Brush->Modify();

			// Find the first selected static mesh actor.
			AStaticMeshActor* SelectedActor = NULL;
			for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>( Actor );
				if( StaticMeshActor )
				{
					SelectedActor = StaticMeshActor;
					break;
				}
			}

			if(SelectedActor)
			{
				GWorld->GetBrush()->Location = SelectedActor->Location;
				SelectedActor->Location = FVector(0,0,0);

				CreateModelFromStaticMesh(GWorld->GetBrush()->Brush,SelectedActor);

				SelectedActor->Location = GWorld->GetBrush()->Location;
			}
			else
			{
				Ar.Logf(TEXT("No suitable actors found."));
			}

			RedrawLevelEditingViewports();
			return TRUE;
		}
	}
	else if(ParseCommand(&Str,TEXT("REBUILD")))	// STATICMESH REBUILD
	{
		// Forces a rebuild of the selected static mesh.
		const FScopedBusyCursor BusyCursor;
		const FString LocalizedRebuildingStaticMeshes( LocalizeUnrealEd(TEXT("RebuildingStaticMeshes")) );

		const FScopedTransaction Transaction( *LocalizedRebuildingStaticMeshes );
		GWarn->BeginSlowTask( *LocalizedRebuildingStaticMeshes, TRUE );

		UStaticMesh* sm = GetSelectedObjects()->GetTop<UStaticMesh>();
		if( sm )
		{
			sm->Modify();
			sm->Build();
		}

		GWarn->EndSlowTask();
	}
	else if(ParseCommand(&Str,TEXT("SMOOTH")))	// STATICMESH SMOOTH
	{
		// Hack to set the smoothing mask of the triangles in the selected static meshes to 1.
		const FScopedBusyCursor BusyCursor;
		const FString LocalizedSmoothStaticMeshes( LocalizeUnrealEd(TEXT("SmoothStaticMeshes")) );

		const FScopedTransaction Transaction( *LocalizedSmoothStaticMeshes );
		GWarn->BeginSlowTask( *LocalizedSmoothStaticMeshes, TRUE );

		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>( Actor );
			if( StaticMeshActor )
			{
				UStaticMesh* StaticMesh = StaticMeshActor->StaticMeshComponent->StaticMesh;
				if ( StaticMesh )
				{
					StaticMesh->Modify();

					// Generate smooth normals.

					for(int k=0;k<StaticMesh->LODModels.Num();k++)
					{
						FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) StaticMesh->LODModels(k).RawTriangles.Lock(LOCK_READ_WRITE);
						for(INT i = 0;i < StaticMesh->LODModels(k).RawTriangles.GetElementCount();i++)
						{
							RawTriangleData[i].SmoothingMask = 1;
						}
						StaticMesh->LODModels(k).RawTriangles.Unlock();
					}

					StaticMesh->Build();
				}
			}
		}

		GWarn->EndSlowTask();
	}
	else if(ParseCommand(&Str,TEXT("UNSMOOTH")))	// STATICMESH UNSMOOTH
	{
		// Hack to set the smoothing mask of the triangles in the selected static meshes to 0.
		const FScopedBusyCursor BusyCursor;
		const FString LocalizedUnsmoothStaticMeshes( LocalizeUnrealEd(TEXT("UnsmoothStaticMeshes")) );

		const FScopedTransaction Transaction( *LocalizedUnsmoothStaticMeshes );
		GWarn->BeginSlowTask( *LocalizedUnsmoothStaticMeshes, TRUE );

		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>( Actor );
			if( StaticMeshActor )
			{
				UStaticMesh* StaticMesh = StaticMeshActor->StaticMeshComponent->StaticMesh;
				if ( StaticMesh )
				{
					StaticMesh->Modify();

					// Generate smooth normals.

					for(int k=0;k<StaticMesh->LODModels.Num();k++)
					{
						FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) StaticMesh->LODModels(k).RawTriangles.Lock(LOCK_READ_WRITE);
						for(INT i = 0;i < StaticMesh->LODModels(k).RawTriangles.GetElementCount();i++)
						{
							RawTriangleData[i].SmoothingMask = 0;
						}
						StaticMesh->LODModels(k).RawTriangles.Unlock();
					}

					StaticMesh->Build();
				}
			}
		}

		GWarn->EndSlowTask();
	}
	else if( ParseCommand(&Str,TEXT("DEFAULT")) )	// STATICMESH DEFAULT NAME=<name>
	{
		GetSelectedObjects()->SelectNone( UStaticMesh::StaticClass() );
		UStaticMesh* sm;
		ParseObject<UStaticMesh>(Str,TEXT("NAME="),sm,ANY_PACKAGE);
		GetSelectedObjects()->Select( sm );
		return TRUE;
	}
#endif // SHIPPING_PC_GAME
	// Take the currently selected static mesh, and save the builder brush as its
	// low poly collision model.
	else if( ParseCommand(&Str,TEXT("SAVEBRUSHASCOLLISION")) )
	{
		// First, find the currently selected actor with a static mesh.
		// Fail if more than one actor with staticmesh is selected.
		UStaticMesh* StaticMesh = NULL;
		FMatrix MeshToWorld;

		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			UStaticMeshComponent* FoundStaticMeshComponent = NULL;
			if( Actor->IsA(AStaticMeshActor::StaticClass()) )
			{
				FoundStaticMeshComponent = static_cast<AStaticMeshActor*>(Actor)->StaticMeshComponent;
			}
			else if( Actor->IsA(ADynamicSMActor::StaticClass()) )
			{
				FoundStaticMeshComponent = static_cast<ADynamicSMActor*>(Actor)->StaticMeshComponent;
			}

			UStaticMesh* FoundMesh = FoundStaticMeshComponent ? FoundStaticMeshComponent->StaticMesh : NULL;
			if( FoundMesh )
			{
				// If we find multiple actors with static meshes, warn and do nothing.
				if( StaticMesh )
				{
					appMsgf( AMT_OK, *LocalizeUnrealEd("Error_SelectOneActor") );
					return TRUE;
				}
				StaticMesh = FoundMesh;
				MeshToWorld = FoundStaticMeshComponent->LocalToWorld;
			}
		}

		// If no static-mesh-toting actor found, warn and do nothing.
		if(!StaticMesh)
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Error_NoActorWithStaticMesh") );
			return TRUE;
		}

		// If we already have a collision model for this staticmesh, ask if we want to replace it.
		if( StaticMesh->BodySetup )
		{
			const UBOOL bDoReplace = appMsgf(AMT_YesNo, *LocalizeUnrealEd("Prompt_24"));
			if( !bDoReplace )
			{
				return TRUE;
			}
		}

		// Now get the builder brush.
		UModel* builderModel = GWorld->GetBrush()->Brush;

		// Need the transform between builder brush space and static mesh actor space.
		const FMatrix BrushL2W = GWorld->GetBrush()->LocalToWorld();
		const FMatrix MeshW2L = MeshToWorld.Inverse();
		const FMatrix SMToBB = BrushL2W * MeshW2L;
		const FMatrix SMToBB_AT = SMToBB.TransposeAdjoint();

		// Copy the current builder brush into a temp model.
		// We keep no reference to this, so it will be GC'd at some point.
		UModel* TempModel = new UModel(NULL,1);
		TempModel->Polys->Element = builderModel->Polys->Element;

		// Now transform each poly into local space for the selected static mesh.
		for(INT i=0; i<TempModel->Polys->Element.Num(); i++)
		{
			FPoly* Poly = &TempModel->Polys->Element(i);

			for(INT j=0; j<Poly->Vertices.Num(); j++ )
			{
				Poly->Vertices(j)  = SMToBB.TransformFVector(Poly->Vertices(j));
			}

			Poly->Normal = SMToBB_AT.TransformNormal(Poly->Normal);
			Poly->Normal.Normalize(); // SmToBB might have scaling in it.
		}

		// Build bounding box.
		TempModel->BuildBound();

		// Build BSP for the brush.
		FBSPOps::bspBuild(TempModel,FBSPOps::BSP_Good,15,70,1,0);
		FBSPOps::bspRefresh(TempModel,1);
		FBSPOps::bspBuildBounds(TempModel);

		// Now - use this as the Rigid Body collision for this static mesh as well.

		// Make sure rendering is done - so we are not changing data being used by collision drawing.
		FlushRenderingCommands();

		// If we already have a BodySetup - clear it.
		if( StaticMesh->BodySetup )
		{
			StaticMesh->BodySetup->AggGeom.EmptyElements();
			StaticMesh->BodySetup->ClearShapeCache();
		}
		// If we don't already have physics props, construct them here.
		else
		{
			 StaticMesh->BodySetup = ConstructObject<URB_BodySetup>(URB_BodySetup::StaticClass(), StaticMesh);
		}

		// Convert collision model into a collection of convex hulls.
		// NB: This removes any convex hulls that were already part of the collision data.
		KModelToHulls(&StaticMesh->BodySetup->AggGeom, TempModel);

		// Finally mark the parent package as 'dirty', so user will be prompted if they want to save it etc.
		StaticMesh->MarkPackageDirty();

		Ar.Logf(TEXT("Added collision model to StaticMesh %s."), *StaticMesh->GetName() );
	}

	return FALSE;

}

UBOOL UEditorEngine::Exec_Brush( const TCHAR* Str, FOutputDevice& Ar )
{
	// Keep a pointer to the beginning of the string to use for message displaying purposes
	const TCHAR* const FullStr = Str;

	if( ParseCommand(&Str,TEXT("APPLYTRANSFORM")) )
	{
		appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"),FullStr));
	}
	else if( ParseCommand(&Str,TEXT("SET")) )
	{
		{
			const FScopedTransaction Transaction( TEXT("Brush Set") );
			GWorld->GetBrush()->Brush->Modify();
			FRotator Temp(0.0f,0.0f,0.0f);
			Constraints.Snap( GWorld->GetBrush()->Location, FVector(0.f,0.f,0.f), Temp );
			GWorld->GetBrush()->Location -= GWorld->GetBrush()->PrePivot;
			GWorld->GetBrush()->PrePivot = FVector(0.f,0.f,0.f);
			GWorld->GetBrush()->Brush->Polys->Element.Empty();
			UPolysFactory* It = new UPolysFactory;
			It->FactoryCreateText( UPolys::StaticClass(), GWorld->GetBrush()->Brush->Polys->GetOuter(), *GWorld->GetBrush()->Brush->Polys->GetName(), 0, GWorld->GetBrush()->Brush->Polys, TEXT("t3d"), GStream, GStream+appStrlen(GStream), GWarn );
			// Do NOT merge faces.
			FBSPOps::bspValidateBrush( GWorld->GetBrush()->Brush, 0, 1 );
			GWorld->GetBrush()->Brush->BuildBound();
		}
		NoteSelectionChange();
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("RESET")) )
	{
		const FScopedTransaction Transaction( TEXT("Brush Reset") );
		GWorld->GetBrush()->Modify();
		GWorld->GetBrush()->InitPosRotScale();
		RedrawLevelEditingViewports();
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("SCALE")) )
	{
		const FScopedTransaction Transaction( TEXT("Brush Scale") );

		FVector Scale;
		GetFVECTOR( Str, Scale );
		if( !Scale.X ) Scale.X = 1.f;
		if( !Scale.Y ) Scale.Y = 1.f;
		if( !Scale.Z ) Scale.Z = 1.f;

		const FVector InvScale( 1.f / Scale.X, 1.f / Scale.Y, 1.f / Scale.Z );

		// Fire CALLBACK_LevelDirtied when falling out of scope.
		FScopedLevelDirtied		LevelDirtyCallback;

		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if( Actor->IsABrush() )
			{
				ABrush* Brush = static_cast<ABrush*>( Actor );
				if ( Brush->Brush )
				{
					Brush->Brush->Modify();
					for( INT poly = 0 ; poly < Brush->Brush->Polys->Element.Num() ; poly++ )
					{
						FPoly* Poly = &(Brush->Brush->Polys->Element(poly));

						Poly->TextureU *= InvScale;
						Poly->TextureV *= InvScale;
						Poly->Base = ((Poly->Base - Brush->PrePivot) * Scale) + Brush->PrePivot;

						for( INT vtx = 0 ; vtx < Poly->Vertices.Num() ; vtx++ )
						{
							Poly->Vertices(vtx) = ((Poly->Vertices(vtx) - Brush->PrePivot) * Scale) + Brush->PrePivot;
						}

						Poly->CalcNormal();
					}

					Brush->Brush->BuildBound();

					Brush->MarkPackageDirty();
					LevelDirtyCallback.Request();
				}
			}
		}

		RedrawLevelEditingViewports();
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("MOVETO")) )
	{
		const FScopedTransaction Transaction( TEXT("Brush MoveTo") );
		GWorld->GetBrush()->Modify();
		GetFVECTOR( Str, GWorld->GetBrush()->Location );
		RedrawLevelEditingViewports();
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("MOVEREL")) )
	{
		const FScopedTransaction Transaction( TEXT("Brush MoveRel") );
		GWorld->GetBrush()->Modify();
		FVector TempVector( 0, 0, 0 );
		GetFVECTOR( Str, TempVector );
		GWorld->GetBrush()->Location.AddBounded( TempVector, HALF_WORLD_MAX1 );
		RedrawLevelEditingViewports();
		return TRUE;
	}
	else if (ParseCommand(&Str,TEXT("ADD")))
	{
		ABrush* NewBrush = NULL;
		{
			const FScopedTransaction Transaction( TEXT("Brush Add") );
			FinishAllSnaps();
			INT DWord1=0;
			Parse( Str, TEXT("FLAGS="), DWord1 );
			NewBrush = FBSPOps::csgAddOperation( GWorld->GetBrush(), DWord1, CSG_Add );
			if( NewBrush )
			{
				bspBrushCSG( NewBrush, GWorld->GetModel(), DWord1, CSG_Add, TRUE, TRUE, TRUE );
				NewBrush->MarkPackageDirty();
			}
			GWorld->InvalidateModelGeometry( TRUE );
		}

		GWorld->UpdateComponents( TRUE );
		RedrawLevelEditingViewports();
		if ( NewBrush )
		{
			GCallbackEvent->Send( CALLBACK_LevelDirtied );
			GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );
		}
		return TRUE;
	}
	else if (ParseCommand(&Str,TEXT("ADDVOLUME"))) // BRUSH ADDVOLUME
	{
		AVolume* Actor = NULL;
		{
			const FScopedTransaction Transaction( TEXT("Brush AddVolume") );
			FinishAllSnaps();

			UClass* VolumeClass = NULL;
			ParseObject<UClass>( Str, TEXT("CLASS="), VolumeClass, ANY_PACKAGE );
			if( !VolumeClass || !VolumeClass->IsChildOf(AVolume::StaticClass()) )
			{
				VolumeClass = AVolume::StaticClass();
			}

			Actor = (AVolume*)GWorld->SpawnActor(VolumeClass,NAME_None,GWorld->GetBrush()->Location);
			if( Actor )
			{
				Actor->PreEditChange(NULL);

				FBSPOps::csgCopyBrush
				(
					Actor,
					GWorld->GetBrush(),
					0,
					RF_Transactional,
					1,
					TRUE
				);

				// Set the texture on all polys to NULL.  This stops invisible texture
				// dependencies from being formed on volumes.
				if( Actor->Brush )
				{
					for( INT poly = 0 ; poly < Actor->Brush->Polys->Element.Num() ; ++poly )
					{
						FPoly* Poly = &(Actor->Brush->Polys->Element(poly));
						Poly->Material = NULL;
					}
				}
				Actor->PostEditChange();
			}
		}

		RedrawLevelEditingViewports();
		if ( Actor )
		{
			GCallbackEvent->Send( CALLBACK_LevelDirtied );
			GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );
		}
		return TRUE;
	}
	else if (ParseCommand(&Str,TEXT("SUBTRACT"))) // BRUSH SUBTRACT
	{
		ABrush* NewBrush = NULL;
		{
			const FScopedTransaction Transaction( TEXT("Brush Subtract") );
			FinishAllSnaps();
			NewBrush = FBSPOps::csgAddOperation(GWorld->GetBrush(),0,CSG_Subtract); // Layer
			if( NewBrush )
			{
				bspBrushCSG( NewBrush, GWorld->GetModel(), 0, CSG_Subtract, TRUE, TRUE, TRUE );
				NewBrush->MarkPackageDirty();
			}
			GWorld->InvalidateModelGeometry( TRUE );
		}
		GWorld->UpdateComponents( TRUE );
		//@todo seamless: this shouldn't be needed: GWorld->UpdateComponents();
		RedrawLevelEditingViewports();
		if ( NewBrush )
		{
			GCallbackEvent->Send( CALLBACK_LevelDirtied );
			GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );
		}
		return TRUE;
	}
	else if (ParseCommand(&Str,TEXT("FROM"))) // BRUSH FROM INTERSECTION/DEINTERSECTION
	{
		if( ParseCommand(&Str,TEXT("INTERSECTION")) )
		{
			Ar.Log( TEXT("Brush from intersection") );
			{
				const FScopedTransaction Transaction( TEXT("Brush From Intersection") );
				GWorld->GetBrush()->Brush->Modify();
				FinishAllSnaps();
				bspBrushCSG( GWorld->GetBrush(), GWorld->GetModel(), 0, CSG_Intersect, FALSE, TRUE, TRUE );
			}
			GWorld->GetBrush()->ClearComponents();
			GWorld->GetBrush()->ConditionalUpdateComponents();	
			GEditorModeTools().MapChangeNotify();
			RedrawLevelEditingViewports();
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("DEINTERSECTION")) )
		{
			Ar.Log( TEXT("Brush from deintersection") );
			{
				const FScopedTransaction Transaction( TEXT("Brush From Deintersection") );
				GWorld->GetBrush()->Brush->Modify();
				FinishAllSnaps();
				bspBrushCSG( GWorld->GetBrush(), GWorld->GetModel(), 0, CSG_Deintersect, FALSE, TRUE, TRUE );
			}
			GWorld->GetBrush()->ClearComponents();
			GWorld->GetBrush()->ConditionalUpdateComponents();
			GEditorModeTools().MapChangeNotify();
			RedrawLevelEditingViewports();
			return TRUE;
		}
	}
	else if( ParseCommand (&Str,TEXT("NEW")) )
	{
		const FScopedTransaction Transaction( TEXT("Brush New") );
		GWorld->GetBrush()->Brush->Modify();
		GWorld->GetBrush()->Brush->Polys->Element.Empty();
		RedrawLevelEditingViewports();
		return TRUE;
	}
	else if( ParseCommand (&Str,TEXT("LOAD")) ) // BRUSH LOAD
	{
		if( Parse( Str, TEXT("FILE="), TempFname, 256 ) )
		{
			const FScopedBusyCursor BusyCursor;

			ResetTransaction( TEXT("loading brush") );
			const FVector TempVector = GWorld->GetBrush()->Location;
			LoadPackage( GWorld->GetOutermost(), TempFname, 0 );
			GWorld->GetBrush()->Location = TempVector;
			FBSPOps::bspValidateBrush( GWorld->GetBrush()->Brush, 0, 1 );
			Cleanse( FALSE, 1, TEXT("loading brush") );
			return TRUE;
		}
	}
	else if( ParseCommand( &Str, TEXT("SAVE") ) )
	{
		if( Parse(Str,TEXT("FILE="),TempFname, 256) )
		{
			Ar.Logf( TEXT("Saving %s"), TempFname );
			check(GWorld);
			SavePackage( GWorld->GetBrush()->Brush->GetOutermost(), GWorld->GetBrush()->Brush, 0, TempFname, GWarn );
		}
		else
		{
			Ar.Log( NAME_ExecWarning, *LocalizeUnrealEd(TEXT("MissingFilename")) );
		}
		return TRUE;
	}
	else if( ParseCommand( &Str, TEXT("IMPORT")) )
	{
		if( Parse(Str,TEXT("FILE="),TempFname, 256) )
		{
			const FScopedBusyCursor BusyCursor;
			const FScopedTransaction Transaction( TEXT("Brush Import") );

			GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("ImportingBrush")), TRUE );

			GWorld->GetBrush()->Brush->Polys->Modify();
			GWorld->GetBrush()->Brush->Polys->Element.Empty();
			DWORD Flags=0;
			UBOOL Merge=0;
			ParseUBOOL( Str, TEXT("MERGE="), Merge );
			Parse( Str, TEXT("FLAGS="), Flags );
			GWorld->GetBrush()->Brush->Linked = 0;
			ImportObject<UPolys>( GWorld->GetBrush()->Brush->Polys->GetOuter(), *GWorld->GetBrush()->Brush->Polys->GetName(), 0, TempFname );
			if( Flags )
			{
				for( Word2=0; Word2<TempModel->Polys->Element.Num(); Word2++ )
				{
					GWorld->GetBrush()->Brush->Polys->Element(Word2).PolyFlags |= Flags;
				}
			}
			for( INT i=0; i<GWorld->GetBrush()->Brush->Polys->Element.Num(); i++ )
			{
				GWorld->GetBrush()->Brush->Polys->Element(i).iLink = i;
			}
			if( Merge )
			{
				bspMergeCoplanars( GWorld->GetBrush()->Brush, 0, 1 );
				FBSPOps::bspValidateBrush( GWorld->GetBrush()->Brush, 0, 1 );
			}
			GWorld->GetBrush()->ClearComponents();
			GWorld->GetBrush()->ConditionalUpdateComponents();
			GWarn->EndSlowTask();
		}
		else
		{
			Ar.Log( NAME_ExecWarning, *LocalizeUnrealEd(TEXT("MissingFilename")) );
		}
		return TRUE;
	}
	else if (ParseCommand(&Str,TEXT("EXPORT")))
	{
		if( Parse(Str,TEXT("FILE="),TempFname, 256) )
		{
			const FScopedBusyCursor BusyCursor;

			GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("ExportingBrush")), TRUE );
			UExporter::ExportToFile( GWorld->GetBrush()->Brush->Polys, NULL, TempFname, 0 );
			GWarn->EndSlowTask();
		}
		else
		{
			Ar.Log( NAME_ExecWarning, *LocalizeUnrealEd(TEXT("MissingFilename")) );
		}
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("MERGEPOLYS")) ) // BRUSH MERGEPOLYS
	{
		const FScopedBusyCursor BusyCursor;

		// Merges the polys on all selected brushes
		GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("MergePolys")), TRUE );
		const INT ProgressDenominator = FActorIteratorBase::GetProgressDenominator();

		// Fire CALLBACK_LevelDirtied when falling out of scope.
		FScopedLevelDirtied		LevelDirtyCallback;

		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if ( Actor->IsABrush() )
			{
				ABrush* Brush = static_cast<ABrush*>( Actor );
				FBSPOps::bspValidateBrush( Brush->Brush, 1, 1 );
				Brush->MarkPackageDirty();
				LevelDirtyCallback.Request();
			}
		}
		RedrawLevelEditingViewports();
		GWarn->EndSlowTask();
	}
	else if( ParseCommand(&Str,TEXT("SEPARATEPOLYS")) ) // BRUSH SEPARATEPOLYS
	{
		const FScopedBusyCursor BusyCursor;

		GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("SeparatePolys")),  TRUE );
		const INT ProgressDenominator = FActorIteratorBase::GetProgressDenominator();

		// Fire CALLBACK_LevelDirtied when falling out of scope.
		FScopedLevelDirtied		LevelDirtyCallback;

		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if ( Actor->IsABrush() )
			{
				ABrush* Brush = static_cast<ABrush*>( Actor );
				FBSPOps::bspUnlinkPolys( Brush->Brush );
				Brush->MarkPackageDirty();
				LevelDirtyCallback.Request();
			}
		}
		RedrawLevelEditingViewports();
		GWarn->EndSlowTask();
	}

	return FALSE;
}

INT UEditorEngine::BeginTransaction(const TCHAR* SessionName)
{
	return Trans->Begin( SessionName );
}

INT UEditorEngine::EndTransaction()
{
	return Trans->End();
}

void UEditorEngine::ResetTransaction(const TCHAR* Action)
{
	if(!GIsUCC)
	{
		Trans->Reset( Action );
	}
}

void UEditorEngine::CancelTransaction(INT Index)
{
	Trans->Cancel( Index );
}

UBOOL UEditorEngine::UndoTransaction()
{
	// Begin transacting
	GIsTransacting = TRUE;
	UBOOL bResult = Trans->Undo();
	NoteSelectionChange();
	// End transacting
	GIsTransacting = FALSE;
	return bResult;
}

UBOOL UEditorEngine::RedoTransaction()
{
	// Begin transacting
	GIsTransacting = TRUE;
	UBOOL bResult = Trans->Redo();
	NoteSelectionChange();
	// End transacting
	GIsTransacting = FALSE;
	return bResult;
}

UBOOL UEditorEngine::IsTransactionActive()
{
	return Trans->IsActive();
}

void UEditorEngine::BuildFluidSurfaces()
{
	for(TObjectIterator<UFluidSurfaceComponent> FluidSurfaceIt; FluidSurfaceIt; ++FluidSurfaceIt)
	{
		FluidSurfaceIt->RebuildClampMap();
	}
}

INT UEditorEngine::GetFreeTransactionBufferSpace()
{
	return Trans->GetBufferFreeSpace();
}

INT UEditorEngine::GetLastTransactionSize()
{
	return Trans->GetLastTransactionSize();
}

UBOOL UEditorEngine::IsTransactionBufferBreeched()
{
	return Trans->IsTransactionBufferBreeched();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Map execs.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

UBOOL UEditorEngine::Map_Rotgrid(const TCHAR* Str, FOutputDevice& Ar)
{
	FinishAllSnaps();
	if( GetFROTATOR( Str, Constraints.RotGridSize, 1 ) )
	{
		RedrawLevelEditingViewports();
	}
	GCallbackEvent->Send( CALLBACK_UpdateUI );

	return TRUE;
}


UBOOL UEditorEngine::Map_Select(const TCHAR* Str, FOutputDevice& Ar)
{
	const FScopedTransaction Transaction( TEXT("Select Brushes") );

	GetSelectedActors()->Modify();

	if( ParseCommand(&Str,TEXT("ADDS")) )
	{
		mapSelectOperation( CSG_Add );
	}
	else if( ParseCommand(&Str,TEXT("SUBTRACTS")) )
	{
		mapSelectOperation( CSG_Subtract );
	}
	else if( ParseCommand(&Str,TEXT("SEMISOLIDS")) )
	{
		mapSelectFlags( PF_Semisolid );
	}
	else if( ParseCommand(&Str,TEXT("NONSOLIDS")) )
	{
		mapSelectFlags( PF_NotSolid );
	}

	RedrawLevelEditingViewports();

	return TRUE;
}

UBOOL UEditorEngine::Map_Brush(const TCHAR* Str, FOutputDevice& Ar)
{
	UBOOL bSuccess = FALSE;

	if( ParseCommand (&Str,TEXT("GET")) )
	{
		const FScopedTransaction Transaction( TEXT("Brush Get") );
		GetSelectedActors()->Modify();
		mapBrushGet();
		RedrawLevelEditingViewports();
		bSuccess = TRUE;
	}
	else if( ParseCommand (&Str,TEXT("PUT")) )
	{
		const FScopedTransaction Transaction( TEXT("Brush Put") );
		mapBrushPut();
		RedrawLevelEditingViewports();
		bSuccess = TRUE;
	}

	return bSuccess;
}

UBOOL UEditorEngine::Map_Sendto(const TCHAR* Str, FOutputDevice& Ar)
{
	UBOOL bSuccess = FALSE;

	if( ParseCommand(&Str,TEXT("FIRST")) )
	{
		const FScopedTransaction Transaction( TEXT("Map SendTo Front") );
		mapSendToFirst();
		RedrawLevelEditingViewports();
		RebuildAlteredBSP(); // Update the Bsp of any levels containing a modified brush
		bSuccess = TRUE;
	}
	else if( ParseCommand(&Str,TEXT("LAST")) )
	{
		const FScopedTransaction Transaction( TEXT("Map SendTo Back") );
		mapSendToLast();
		RedrawLevelEditingViewports();
		RebuildAlteredBSP(); // Update the Bsp of any levels containing a modified brush
		bSuccess = TRUE;
	}
	else if( ParseCommand(&Str,TEXT("SWAP")) )
	{
		const FScopedTransaction Transaction( TEXT("Map SendTo Swap") );
		mapSendToSwap();
		RedrawLevelEditingViewports();
		RebuildAlteredBSP(); // Update the Bsp of any levels containing a modified brush
		bSuccess = TRUE;
	}

	return bSuccess;
}

UBOOL UEditorEngine::Map_Rebuild(const TCHAR* Str, FOutputDevice& Ar)
{
	EMapRebuildType RebuildType = EMapRebuildType::MRT_Current;

	if( ParseCommand(&Str,TEXT("ALLVISIBLE")) )
	{
		RebuildType = EMapRebuildType::MRT_AllVisible;
	}
	else if( ParseCommand(&Str,TEXT("ALLDIRTYFORLIGHTING")) )
	{
		RebuildType = EMapRebuildType::MRT_AllDirtyForLighting;
	}

	RebuildMap(RebuildType);

	//Clean BSP references afterward (artist request)
	const INT NumReferences = CleanBSPMaterials(FALSE, FALSE);
	if (NumReferences > 0)
	{
		debugf(TEXT("Cleared %d NULL BSP materials after rebuild."), NumReferences);
	}

	return TRUE;
}

/**
 * Rebuilds the map.
 *
 * @param bBuildAllVisibleMaps	Whether or not to rebuild all visible maps, if FALSE only the current level will be built.
 */
void UEditorEngine::RebuildMap(EMapRebuildType RebuildType)
{
	FlushRenderingCommands();

	ResetTransaction( *LocalizeUnrealEd(TEXT("RebuildingMap")) );
	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("RebuildingGeometry")), FALSE);

	AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
	if ( WorldInfo->bPathsRebuilt )
	{
		debugf(TEXT("Rebuildmap Clear paths rebuilt"));
	}
	WorldInfo->bPathsRebuilt = FALSE;

	switch (RebuildType)
	{
		case EMapRebuildType::MRT_AllVisible:
		{
			// Store old current level
			ULevel* OldCurrent = GWorld->CurrentLevel;

			// Build CSG for the persistent level
			GWorld->CurrentLevel = GWorld->PersistentLevel;
			if ( FLevelUtils::IsLevelVisible( GWorld->CurrentLevel ) )
			{
				csgRebuild();
				GWorld->InvalidateModelGeometry( TRUE );
				GWorld->CurrentLevel->bGeometryDirtyForLighting = FALSE;
			}

			// Build CSG for all visible streaming levels
			for( INT LevelIndex = 0; LevelIndex < WorldInfo->StreamingLevels.Num() && !GEngine->GetMapBuildCancelled(); ++LevelIndex )
			{
				ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels( LevelIndex );
				if( StreamingLevel && StreamingLevel->LoadedLevel != NULL && FLevelUtils::IsLevelVisible( StreamingLevel ) )
				{
					GWorld->CurrentLevel = StreamingLevel->LoadedLevel;
					csgRebuild();
					GWorld->InvalidateModelGeometry( TRUE );
					GWorld->CurrentLevel->bGeometryDirtyForLighting = FALSE;
				}
			}
			// Restore the current level
			GWorld->CurrentLevel = OldCurrent;
		}
		break;

		case EMapRebuildType::MRT_AllDirtyForLighting:
		{
			// Store old current level
			ULevel* OldCurrent = GWorld->CurrentLevel;
			{
				// Build CSG for the persistent level if it's out of date
				if (GWorld->PersistentLevel->bGeometryDirtyForLighting)
				{
					GWorld->CurrentLevel = GWorld->PersistentLevel;
					csgRebuild();
					GWorld->InvalidateModelGeometry( TRUE );
					GWorld->CurrentLevel->bGeometryDirtyForLighting = FALSE;
				}

				// Build CSG for each streaming level that is out of date
				for( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() && !GEngine->GetMapBuildCancelled(); ++LevelIndex )
				{
					ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels( LevelIndex );
					if( StreamingLevel && StreamingLevel->LoadedLevel && StreamingLevel->LoadedLevel->bGeometryDirtyForLighting)
					{
						GWorld->CurrentLevel = StreamingLevel->LoadedLevel;
						csgRebuild();
						GWorld->InvalidateModelGeometry( TRUE );
						GWorld->CurrentLevel->bGeometryDirtyForLighting = FALSE;
					}
				}
			}
		// Restore the current level.
		GWorld->CurrentLevel = OldCurrent;
	}
		break;

		case EMapRebuildType::MRT_Current:
	{
			// Just build the current level
		csgRebuild();
		GWorld->InvalidateModelGeometry( TRUE );
			GWorld->CurrentLevel->bGeometryDirtyForLighting = FALSE;
		}
		break;
	}

	GWarn->StatusUpdatef( -1, -1, *LocalizeUnrealEd(TEXT("CleaningUpE")) );

	RedrawLevelEditingViewports();
	GCallbackEvent->Send( CALLBACK_MapChange, MapChangeEventFlags::MapRebuild );
	
	GWarn->EndSlowTask();
}

/**
 * Quickly rebuilds a single level (no bounds build, visibility testing or Bsp tree optimization).
 *
 * @param Level	The level to be rebuilt.
 */
void UEditorEngine::RebuildLevel(ULevel& Level)
{
	// Early out if BSP auto-updating is disabled
	if (!GEditorModeTools().GetBSPAutoUpdate())
	{
		return;
	}

	// Note: most of the following code was taken from UEditorEngine::csgRebuild()
	FinishAllSnaps();
	FBSPOps::GFastRebuild = 1;
	
	// Store old current level
	AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
	ULevel* OldCurrent = GWorld->CurrentLevel;

	// Build CSG for the persistent level
	GWorld->CurrentLevel = &Level;

	RebuildModelFromBrushes(GWorld->GetModel(), false);

	GWorld->InvalidateModelGeometry(TRUE);
	GWorld->CurrentLevel->MarkPackageDirty();
	GCallbackEvent->Send(CALLBACK_LevelDirtied);
	GCallbackEvent->Send(CALLBACK_RefreshEditor_LevelBrowser);

	// Restore the current level
	GWorld->CurrentLevel = OldCurrent;

	FBSPOps::GFastRebuild = 1;
}

/**
 * Builds up a model from a set of brushes. Used by RebuildLevel.
 *
 * @param Model					The model to be rebuilt.
 * @param bSelectedBrushesOnly	Use all brushes in the current level or just the selected ones?.
 * @param bExcludeBuilderBrush	Ignore the current builder brush? Defaults to ignore it.
 * @param bStaticBrushesOnly	Use only static brushes? Dynamic brushes like volumes are ignored. Defaults to ignore dynamic brushes.
 * @param bAddActiveBrushes		Treat brushes of type CSG_Active as CGS_Add for model building purposes? Defaults to FALSE. Used by the 'convert to mesh' code.
 *
 * @return Number of brushes used.
 */
UINT UEditorEngine::RebuildModelFromBrushes(UModel* Model, UBOOL bSelectedBrushesOnly, UBOOL bExcludeBuilderBrush, UBOOL bStaticBrushesOnly, UBOOL bAddActiveBrushes)
{
	// Empty the model out.
	Model->EmptyModel(1, 1);
	// Rebuild dynamic brush BSP's.
	for (FActorIterator It; It; ++It)
	{
		ABrush* DynamicBrush = Cast<ABrush>(*It);
		if (DynamicBrush && DynamicBrush->Brush && !DynamicBrush->IsStaticBrush() &&
			(!bSelectedBrushesOnly || DynamicBrush->IsSelected()))
		{
			FBSPOps::csgPrepMovingBrush(DynamicBrush);
		}
	}

	// Record number of Brushes used
	UINT NumberOfBrushesUsed = 0;

	// Compose all structural brushes and portals.
	for (FActorIterator It; It; ++It)
	{	
		ABrush* Brush = Cast<ABrush>(*It);

		if (Brush &&
			(!bStaticBrushesOnly || Brush->IsStaticBrush()) &&
			(!bExcludeBuilderBrush || !Brush->IsABuilderBrush()) &&
			(!bSelectedBrushesOnly || Brush->IsSelected()) &&
			(!(Brush->PolyFlags & PF_Semisolid) || (Brush->CsgOper != CSG_Add && Brush->CsgOper != CSG_Active) || (Brush->PolyFlags & PF_Portal)))
		{
			// Treat portals as solids for cutting.
			if (Brush->PolyFlags & PF_Portal)
			{
				Brush->PolyFlags = (Brush->PolyFlags & ~PF_Semisolid) | PF_NotSolid;
			}
			ECsgOper CsgOp = (bAddActiveBrushes && Brush->CsgOper == CSG_Active) ? CSG_Add : (ECsgOper)Brush->CsgOper;
			bspBrushCSG(Brush, Model, Brush->PolyFlags, CsgOp, FALSE, TRUE, FALSE, FALSE);
			NumberOfBrushesUsed++;
		}
	}
	
	// Compose all detail brushes.
	for (FActorIterator It; It; ++It)
	{
		ABrush* Brush = Cast<ABrush>(*It);
		if (Brush &&
			(!bStaticBrushesOnly || Brush->IsStaticBrush()) &&
			(!bExcludeBuilderBrush || !Brush->IsABuilderBrush()) &&
			(!bSelectedBrushesOnly || Brush->IsSelected()) &&
			(Brush->PolyFlags & PF_Semisolid) && !(Brush->PolyFlags & PF_Portal) && (Brush->CsgOper == CSG_Add || Brush->CsgOper == CSG_Active))
		{
			ECsgOper CsgOp = (bAddActiveBrushes && Brush->CsgOper == CSG_Active) ? CSG_Add : (ECsgOper)Brush->CsgOper;
			bspBrushCSG(Brush, Model, Brush->PolyFlags, CsgOp, FALSE, TRUE, FALSE, FALSE);
			NumberOfBrushesUsed++;
		}
	}
	return NumberOfBrushesUsed;
}

/**
 * Rebuilds levels containing currently selected brushes and should be invoked after a brush has been modified
 */
void UEditorEngine::RebuildAlteredBSP()
{
	// Early out if BSP auto-updating is disabled
	if (!GEditorModeTools().GetBSPAutoUpdate())
	{
		return;
	}

	// Flush rendering commands so the renderer doesn't attempt to render the BSP while it's being built
	FlushRenderingCommands();

	// A list of all the levels that need to be rebuilt
	TArray<ULevel*> LevelsToRebuild;

	// Determine which levels need to be rebuilt
	for (FSelectionIterator It(GetSelectedActorIterator()); It; ++It)
	{
		AActor* Actor = static_cast<AActor*>(*It);
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if (Actor->IsABrush() && !Actor->IsABuilderBrush())
		{
			ABrush* SelectedBrush = static_cast<ABrush*>( Actor );
			ULevel* Level = SelectedBrush->GetLevel();
			if (Level)
			{
				LevelsToRebuild.AddUniqueItem(Level);
			}
		}
	}

	// Rebuild the levels
	for (INT LevelIdx = 0; LevelIdx < LevelsToRebuild.Num(); ++LevelIdx)
	{
		RebuildLevel(*(LevelsToRebuild(LevelIdx)));
	}
	
	if (LevelsToRebuild.Num() > 0)
	{
		GWorld->UpdateComponents(TRUE);
		RedrawLevelEditingViewports();
	}
}

namespace {

static void TearDownGWorld(const TCHAR* CleanseText)
{
	GUnrealEd->CurrentLODParentActor = NULL;
	GUnrealEd->GetThumbnailManager()->ClearComponents();
	GEditor->SelectNone( TRUE, TRUE );
	GEditor->ClearComponents();
	GEditor->ClearPreviewAudioComponents();
	// Remove all active groups, they belong to a map being unloaded
	GEditor->ActiveGroupActors.Empty();

	// Stop all audio and remove references to level.
	if( GEditor->Client && GEditor->Client->GetAudioDevice() )
	{
		GEditor->Client->GetAudioDevice()->Flush( NULL );
	}

	// Create dummy intermediate world.
	UWorld::CreateNew();
	// Keep track of it as this should be the only UWorld object still around after a map change.
	UWorld* IntermediateDummyWorld = GWorld;

	// Route map change.
	GCallbackEvent->Send( CALLBACK_MapChange, MapChangeEventFlags::WorldTornDown );
	GEditor->NoteSelectionChange();

	// Tear it down again
	GEditor->ClearComponents();
	GWorld->TermWorldRBPhys();
	GWorld->CleanupWorld();
	GWorld->RemoveFromRoot();
	// Set GWorld to NULL so accessing it via PostLoad crashes.		
	GWorld = NULL;

	// And finally cleanse which should remove the old world which we are going to verify.
	GEditor->Cleanse( TRUE, 0, CleanseText );

	// Ensure that previous world is fully cleaned up at this point.
	for( TObjectIterator<UWorld> It; It; ++It )
	{
		UWorld* RemainingWorld = *It;
		if( RemainingWorld != IntermediateDummyWorld )
		{
			UObject::StaticExec(*FString::Printf(TEXT("OBJ REFS CLASS=WORLD NAME=%s"), *RemainingWorld->GetPathName()));

			TMap<UObject*,UProperty*>	Route		= FArchiveTraceRoute::FindShortestRootPath( RemainingWorld, TRUE, GARBAGE_COLLECTION_KEEPFLAGS );
			FString						ErrorString	= FArchiveTraceRoute::PrintRootPath( Route, RemainingWorld );

			appErrorf(TEXT("%s still around trying to load %s") LINE_TERMINATOR TEXT("%s"),*RemainingWorld->GetPathName(),TempFname,*ErrorString);
		}
	}
}

} // namespace 

void UEditorEngine::NewMap()
{
	const FScopedBusyCursor BusyCursor;

	// Pause propagation.
	GObjectPropagator->Pause();

	// Clear the lighting build results
	GWarn->LightingBuild_Clear();
	GWarn->LightingBuildInfo_Clear();

	LastCameraAlignTarget = NULL;

	// Clear out the actor list for moving actors between levels
	BufferLevelActors.Empty();

	ResetTransaction( TEXT("clearing map") );

	TearDownGWorld( TEXT("starting new map") );

	// Create a new world.
	UWorld::CreateNew();
	NoteSelectionChange();
	GCallbackEvent->Send( CALLBACK_MapChange, MapChangeEventFlags::NewMap );

	// Move the brush to the origin.
	GWorld->GetBrush()->Location = FVector(0,0,0);

	// Make the builder brush a small 256x256x256 cube so its visible.
	InitBuilderBrush();

	// Start up the PhysX scene for Landscape collision.
	GWorld->InitWorldRBPhys();

	// Resume propagation.
	GObjectPropagator->Unpause();
}

/**
 *	Check whether the specified package file is a map
 */
UBOOL UEditorEngine::PackageIsAMapFile( const TCHAR* PackageFilename )
{
	// make sure that the file is a map
	FArchive* CheckMapPackageFile = GFileManager->CreateFileReader( PackageFilename );
	if( CheckMapPackageFile )
	{
		FPackageFileSummary Summary;
		( *CheckMapPackageFile ) << Summary;
		delete CheckMapPackageFile;

		if( ( Summary.PackageFlags & PKG_ContainsMap ) == 0 )
		{
			return FALSE;
		}
	}
	return TRUE;
}

UBOOL UEditorEngine::Map_Load(const TCHAR* Str, FOutputDevice& Ar)
{
	// Pause propagation.
	GObjectPropagator->Pause();

	// Clear the lighting build results
	GWarn->LightingBuild_Clear();
	GWarn->LightingBuildInfo_Clear();

	LastCameraAlignTarget = NULL;

	// Clear out the actor list for moving actors between levels
	BufferLevelActors.Empty();

	// We are beginning a map load
	GIsEditorLoadingMap = TRUE;

	INT IsPlayWorld = 0;
	if( Parse( Str, TEXT("FILE="), TempFname, 256 ) )
	{
		FString AlteredPath;
		if ( GPackageFileCache->FindPackageFile( TempFname, NULL, AlteredPath) )
		{
			if( !PackageIsAMapFile( TempFname ) )
			{
				appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd( TEXT( "FileIsNotAMap" ) ), TempFname ) );
				return FALSE;
			}

			const FScopedBusyCursor BusyCursor;

			// Are we loading up a playworld to play in inside the editor?
			Parse(Str, TEXT("PLAYWORLD="), IsPlayWorld);

			// Are we loading a template map that should be loaded into an untitled package?
			INT bIsLoadingMapTemplate = 0;
			Parse(Str, TEXT("TEMPLATE="), bIsLoadingMapTemplate);

			// We cannot open a play world package directly in the Editor as we munge some data for e.g. level streaming
			// and also set PKG_PlayInEditor to disallow undo/ redo.
			if( !IsPlayWorld && FFilename(TempFname).GetBaseFilename().StartsWith( PLAYWORLD_PACKAGE_PREFIX ) )
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("CannotOpenPIEMapsForEditing") );
				// Map load failed 
				GIsEditorLoadingMap = FALSE;
				return FALSE;
			}

			UObject* OldOuter = NULL;

			// If we are loading the playworld, then we don't want to clean up the existing GWorld, because it needs to live side-by-side.
			if (!IsPlayWorld)
			{
				if( !GEditorModeTools().IsModeActive( EM_Default ) )
				{
					GEditorModeTools().ActivateMode( EM_Default );
				}

				OldOuter = GWorld->GetOuter();

				//@todo seamless, when clicking on Play From Here, there is a click in the TransBuffer, so this Reset barfs. ???

				FString MapFileName = FFilename( TempFname ).GetCleanFilename();
				const FString LocalizedLoadingMap(
					*FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("LoadingMap_F") ), *MapFileName ) ) );

				ResetTransaction( *LocalizedLoadingMap );
				GWarn->BeginSlowTask( *FString( LocalizedLoadingMap + TEXT( " " ) + LocalizeUnrealEd( TEXT( "LoadingMapStatus_CleaningUp" ) ) ), TRUE );

				EdClearLoadErrors();
				TearDownGWorld( *LocalizedLoadingMap );


				GWarn->StatusUpdatef( -1, -1, *LocalizedLoadingMap );
			}

			// Record the name of this file to make sure we load objects in this package on top of in-memory objects in this package.
			UserOpenedFile				= TempFname;

			UINT LoadFlags = LOAD_None;
			// if we're loading a PIE map, attempt to find objects that aren't saved out to disk yet
			if (IsPlayWorld)
			{
				LoadFlags |= LOAD_FindIfFail;
			}

			const INT MAX_STREAMLVL_SIZE = 16384;  // max cmd line size (16kb)
			TCHAR StreamLvlBuf[MAX_STREAMLVL_SIZE]; //There can be a lot of streaming levels with very large path names

			if(Parse(Str, TEXT("STREAMLVL="), StreamLvlBuf, ARRAY_COUNT(StreamLvlBuf)))
			{
				TCHAR *Context = NULL;
				TCHAR *CurStreamMap = _tcstok_s(StreamLvlBuf, TEXT(";"), &Context);

				while(CurStreamMap)
				{
					LoadPackage(NULL, CurStreamMap, LoadFlags);
					CurStreamMap = _tcstok_s(NULL, TEXT(";"), &Context);
				}
			}

			FFilename NewFileName = TempFname;
			FFilename SimpleFileName = FEditorFileUtils::GetSimpleMapName();
			UBOOL bIsLoadingSimpleStartupMap = FEditorFileUtils::IsLoadingSimpleStartupMap() && (SimpleFileName.GetBaseFilename() == NewFileName.GetBaseFilename());
			
			UPackage* WorldPackage;
			// Load startup maps and templates into new outermost packages so that the Save function in the editor won't overwrite the original
			if (bIsLoadingSimpleStartupMap || bIsLoadingMapTemplate)
			{
				//create a package with the proper name
				WorldPackage = CreatePackage(NULL, *(MakeUniqueObjectName(NULL, UPackage::StaticClass()).ToString()));
				//now load the map into the package created above
				WorldPackage = LoadPackage( WorldPackage, TempFname, LoadFlags );
			}
			else
			{
				//Load the map normally into a new package
				WorldPackage = LoadPackage( NULL, TempFname, LoadFlags );
			}

			// Reset the opened package to nothing.
			UserOpenedFile				= FString("");

			GWorld						= FindObjectChecked<UWorld>( WorldPackage, TEXT("TheWorld") );
			GWorld->Init();

			FBSPOps::bspValidateBrush( GWorld->GetBrush()->Brush, 0, 1 );

			// In the playworld case, we don't want to do crazy cleanup since the main level is still running.
			if (!IsPlayWorld)
			{
				// Make sure PIE maps are not openable for editing even after they have been renamed.
				if( WorldPackage->PackageFlags & PKG_PlayInEditor )
				{
					appErrorf( *LocalizeUnrealEd("CannotOpenPIEMapsForEditing") );
					GIsEditorLoadingMap = FALSE;
					return FALSE;
				}

				FString MapFileName = FFilename( TempFname ).GetCleanFilename();
				const FString LocalizedLoadingMap(
					*FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("LoadingMap_F") ), *MapFileName ) ) );
				GWarn->StatusUpdatef( -1, -1, *FString( LocalizedLoadingMap + TEXT( " " ) + LocalizeUnrealEd( TEXT( "LoadingMapStatus_Initializing" ) ) ) );

				GWorld->AddToRoot();

				GCallbackEvent->Send( CALLBACK_MapChange, MapChangeEventFlags::NewMap );
				NoteSelectionChange();
				GWorld->PersistentLevel->SetFlags( RF_Transactional );
				GWorld->GetModel()->SetFlags( RF_Transactional );
				if( GWorld->GetModel()->Polys ) 
				{
					GWorld->GetModel()->Polys->SetFlags( RF_Transactional );
				}

				// Make sure secondary levels are loaded & visible.
				GWorld->UpdateLevelStreaming();

				// Update any actors that can be affected by CullDistanceVolumes
				GWorld->UpdateCullDistanceVolumes();

				// Check for any PrefabInstances which are out of date.
				UpdatePrefabs();

				// Fix Kismet ParentSequence pointers
				FixKismetParentSequences();
				
				// Update / Convert out-of-date kismet objects
				UpdateKismetObjects();

				GWarn->EndSlowTask();

				// Update out-of-date proc buildings
				GUnrealEd->CleanupOldBuildings(FALSE, TRUE);

				// Start up the PhysX scene for Landscape collision.
				GWorld->InitWorldRBPhys();
			}

			// Look for 'orphan' actors - that is, actors which are in the Package of the level we just loaded, but not in the Actors array.
			// If we find any, set bDeleteMe to 'true', so that PendingKill will return 'true' for them. We can NOT use FActorIterator here
			// as it just traverses the Actors list.
			const DOUBLE StartTime = appSeconds();
			for( TObjectIterator<AActor> It; It; ++It )
			{
				AActor* Actor = *It;

				// If Actor is part of the world we are loading's package, but not in Actor list, clear it
				if( Actor->GetOutermost() == WorldPackage && !GWorld->ContainsActor(Actor) && !Actor->bDeleteMe
					&&	!Actor->IsAPrefabArchetype() && !Actor->HasAnyFlags(RF_ArchetypeObject) )
				{
					debugf( TEXT("Destroying orphan Actor: %s"), *Actor->GetName() );					
					Actor->bDeleteMe = true;
				}
			}
			debugf( TEXT("Finished looking for orphan Actors (%3.3lf secs)"), appSeconds() - StartTime );

			// Set Transactional flag.
			for( FActorIterator It; It; ++It )
			{
				AActor* Actor = *It;
				Actor->SetFlags( RF_Transactional );
			}

			// We don't really need the Load Error dialog when we are playing in the editor.
			if( !IsPlayWorld && GEdLoadErrors.Num() )
			{
				GCallbackEvent->Send( CALLBACK_DisplayLoadErrors );
			}

			GUnrealEd->PromptToSaveChangedDependentMaterialPackages();
		}
		else
		{
			warnf( LocalizeSecure(LocalizeError(TEXT("FileNotFound"),TEXT("Core")), TempFname) );
		}
	}
	else
	{
		Ar.Log( NAME_ExecWarning, *LocalizeUnrealEd(TEXT("MissingFilename")) );
	}

	// Done loading a map
	GIsEditorLoadingMap = FALSE;

	// Resume propagation.
	GObjectPropagator->Unpause();

	return TRUE;
}

UBOOL UEditorEngine::Map_Import(const TCHAR* Str, FOutputDevice& Ar, UBOOL bNewMap)
{
	// Pause propagation.
	GObjectPropagator->Pause();

	if( Parse( Str, TEXT("FILE="), TempFname, 256) )
	{
		const FScopedBusyCursor BusyCursor;

		FString MapFileName = FFilename( TempFname ).GetCleanFilename();
		const FString LocalizedImportingMap(
			*FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("ImportingMap_F") ), *MapFileName ) ) );

		LastCameraAlignTarget = NULL;

		// Clear out the actor list for moving actors between levels
		BufferLevelActors.Empty();

		ResetTransaction( *LocalizedImportingMap );
		GWarn->BeginSlowTask( *LocalizedImportingMap, TRUE );
		ClearComponents();
		GWorld->CleanupWorld();
		// If we are importing into a new map, we toss the old, and make a new.
		if (bNewMap)
		{
			Cleanse( TRUE, 1, *LocalizedImportingMap );
			UWorld::CreateNew();

			// Start up the PhysX scene for Landscape collision.
			GWorld->InitWorldRBPhys();
		}
		ImportObject<UWorld>(GWorld->GetOuter(), GWorld->GetFName(), RF_Transactional, TempFname );
		GWarn->EndSlowTask();
		GCallbackEvent->Send( CALLBACK_MapChange, MapChangeEventFlags::NewMap );
		NoteSelectionChange();
		Cleanse( FALSE, 1, *LocalizeUnrealEd(TEXT("ImportingActors")) );
	}
	else
	{
		Ar.Log( NAME_ExecWarning, *LocalizeUnrealEd(TEXT("MissingFilename")) );
	}

	// Resume propagation.
	GObjectPropagator->Unpause();

	return TRUE;
}

/**
 * Exports the current map to the specified filename.
 *
 * @param	InFilename					Filename to export the map to.
 * @param	bExportSelectedActorsOnly	If TRUE, export only the selected actors.
 */
void UEditorEngine::ExportMap(const TCHAR* InFilename, UBOOL bExportSelectedActorsOnly)
{
	const FScopedBusyCursor BusyCursor;

	FString MapFileName = FFilename( InFilename ).GetCleanFilename();
	const FString LocalizedExportingMap(
		*FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("ExportingMap_F") ), *MapFileName ) ) );
	GWarn->BeginSlowTask( *LocalizedExportingMap, TRUE);

	UExporter::FExportToFileParams Params;
	Params.Object = GWorld;
	Params.Exporter = NULL;
	Params.Filename = InFilename;
	Params.InSelectedOnly = bExportSelectedActorsOnly;
	Params.NoReplaceIdentical = FALSE;
	Params.Prompt = FALSE;
	Params.bUseFileArchive = FALSE;
	Params.WriteEmptyFiles = FALSE;

	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	if (WorldInfo)
	{
		for (INT ClipPadIndex = 0; ClipPadIndex < WorldInfo->ClipPadEntries.Num(); ClipPadIndex++)
		{
			UObject* ClipPadEntry = WorldInfo->ClipPadEntries(ClipPadIndex);
			if (ClipPadEntry)
			{
				Params.IgnoreObjectList.AddItem(ClipPadEntry);
			}
		}
	}
	UExporter::ExportToFileEx(Params);

	GWarn->EndSlowTask();
}

/**
 * Helper structure for finding meshes at the same point in space.
 */
struct FGridBounds
{
	/**
	 * Constructor, intializing grid bounds based on passed in center and extent.
	 * 
	 * @param InCenter	Center location of bounds.
	 * @param InExtent	Extent of bounds.
	 */
	FGridBounds( const FVector& InCenter, const FVector& InExtent )
	{
		const INT GRID_SIZE_XYZ = 16;
		CenterX = InCenter.X / GRID_SIZE_XYZ;
		CenterY = InCenter.Y / GRID_SIZE_XYZ;
		CenterZ = InCenter.Z / GRID_SIZE_XYZ;
		ExtentX = InExtent.X / GRID_SIZE_XYZ;
		ExtentY = InExtent.Y / GRID_SIZE_XYZ;
		ExtentZ = InExtent.Z / GRID_SIZE_XYZ;
	}

	/** Center integer coordinates */
	INT	CenterX, CenterY, CenterZ;

	/** Extent integer coordinates */
	INT	ExtentX, ExtentY, ExtentZ;

	/**
	 * Equals operator.
	 *
	 * @param Other	Other gridpoint to compare agains
	 * @return TRUE if equal, FALSE otherwise
	 */
	UBOOL operator == ( const FGridBounds& Other ) const
	{
		return CenterX == Other.CenterX 
			&& CenterY == Other.CenterY 
			&& CenterZ == Other.CenterZ
			&& ExtentX == Other.ExtentX
			&& ExtentY == Other.ExtentY
			&& ExtentZ == Other.ExtentZ;
	}
	
	/**
	 * Helper function for TMap support, generating a hash value for the passed in 
	 * grid bounds.
	 *
	 * @param GridBounds Bounds to calculated hash value for
	 * @return Hash value of passed in grid bounds.
	 */
	friend inline DWORD GetTypeHash( const FGridBounds& GridBounds )
	{
		return appMemCrc( &GridBounds,sizeof(FGridBounds) );
	}
};

/**
 * Iterates over objects belonging to the specified packages and reports
 * direct references to objects in the trashcan packages.  Only looks
 * at loaded objects.  Output is to the specified arrays -- the i'th
 * element of OutObjects refers to the i'th element of OutTrashcanObjects.
 *
 * @param	Packages			Only objects in these packages are considered when looking for trashcan references.
 * @param	OutObjects			[out] Receives objects that directly reference objects in the trashcan.
 * @param	OutTrashcanObject	[out] Receives the list of referenced trashcan objects.
 */
void UEditorEngine::CheckForTrashcanReferences(const TArray<UPackage*>& Packages, TArray<UObject*>& OutObjects, TArray<UObject*>& OutTrashcanObjects)
{
	OutObjects.Empty();
	OutTrashcanObjects.Empty();

	INT NumLoadedPackages = 0;
	for ( INT PackageIdx = 0; PackageIdx < Packages.Num(); PackageIdx++ )
	{
		if ( Packages(PackageIdx) != NULL )
		{
			NumLoadedPackages++;
		}
	}

	// Do nothing if no packages are specified.
	if ( NumLoadedPackages == 0 )
	{
		return;
	}

	// Assemble a list of all trashcan packages.
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();
	TArray<FString> TrashcanPackageFilenames;
	for ( INT PackageIndex = 0 ; PackageIndex < PackageList.Num() ; ++PackageIndex )
	{
		const FString& Filename = PackageList( PackageIndex );
		if ( Filename.InStr(TRASHCAN_DIRECTORY_NAME) != -1 )
		{
			TrashcanPackageFilenames.AddItem( Filename );
		}
	}

	// Assemble a list of loaded trashcan packages.
	TArray<UPackage*> LoadedTrashcanPackages;
	for ( INT TrashcanPackageIndex = 0 ; TrashcanPackageIndex < TrashcanPackageFilenames.Num() ; ++TrashcanPackageIndex )
	{
		const FString& PackageFilename = TrashcanPackageFilenames(TrashcanPackageIndex);
		const FString PackageName( FFilename(PackageFilename).GetBaseFilename() );

		// Add the package to the list if loaded.
		UPackage* Package = FindObject<UPackage>( ANY_PACKAGE, *PackageName );
		if ( Package )
		{
			LoadedTrashcanPackages.AddItem( Package );
		}
	}

	// Do nothing if no trashcan packages are loaded.
	if ( LoadedTrashcanPackages.Num() == 0 )
	{
		return;
	}

	// Iterate over all objects in the selected packages and look for references to trashcan objects.
	for ( TObjectIterator<UObject> It ; It ; ++It )
	{
		UObject* Object = *It;

		// See if the object belongs to one of the input packages.
		UBOOL bBelongsToSelectedPackage = FALSE;
		for ( INT PackageIndex = 0 ; PackageIndex < Packages.Num() ; ++PackageIndex )
		{
			UPackage* SelectedPackage = Packages(PackageIndex);
			if ( SelectedPackage != NULL && Object->IsIn( SelectedPackage ) )
			{
				bBelongsToSelectedPackage = TRUE;
				break;
			}
		}

		if ( bBelongsToSelectedPackage )
		{
			// Collect a list of direct references.
			TArray<UObject*> DirectReferences;
			FArchiveObjectReferenceCollector ObjectReferenceCollector( &DirectReferences, NULL, TRUE, FALSE, FALSE, TRUE );
			Object->Serialize( ObjectReferenceCollector );

			// For all references . . . 
			for ( INT ReferenceIndex = 0 ; ReferenceIndex < DirectReferences.Num() ; ++ReferenceIndex )
			{
				UObject* Ref = DirectReferences(ReferenceIndex);

				// . . . note references to objects in the loaded trashcan packages.
				for ( INT TrashcanPackageIndex = 0 ; TrashcanPackageIndex < LoadedTrashcanPackages.Num() ; ++TrashcanPackageIndex )
				{
					UPackage* TrashcanPackage = LoadedTrashcanPackages(TrashcanPackageIndex);
					if ( Ref->IsIn(TrashcanPackage) )
					{
						OutObjects.AddItem( Object );
						OutTrashcanObjects.AddItem( Ref );
					}
				}
			}
		}
	}
}

/**
 * Checks loaded levels for references to objects in the trashcan and
 * reports to the Map Check dialog.
 */
void UEditorEngine::CheckLoadedLevelsForTrashcanReferences()
{
	// Clean up any old worlds.
	UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

	// Get the set of all referenced world packages.
	TArray<UWorld*> Worlds;
	FLevelUtils::GetWorlds( Worlds, TRUE );

	TArray<UPackage*> WorldPackages;
	for ( INT WorldIndex = 0 ; WorldIndex < Worlds.Num() ; ++WorldIndex )
	{
		UWorld* World = Worlds(WorldIndex);
		UPackage* WorldPackage = World->GetOutermost();
		WorldPackages.AddItem( WorldPackage );
	}

	// Find references to objects in the trashcan.
	TArray<UObject*> Objects;
	TArray<UObject*> TrashcanObjects;
	GEditor->CheckForTrashcanReferences( WorldPackages, Objects, TrashcanObjects );

	check( Objects.Num() == TrashcanObjects.Num() );

	// Output to the map check dialog.
	for ( INT ObjectIndex = 0 ; ObjectIndex < Objects.Num() ; ++ObjectIndex )
	{
		const UObject* Object = Objects( ObjectIndex );
		const UObject* TrashcanObject = TrashcanObjects( ObjectIndex );
		GWarn->MapCheck_Add( MCTYPE_ERROR, NULL, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_TrashcanReference" ), *Object->GetName(), *TrashcanObject->GetName(), *Object->GetFullName(), *TrashcanObject->GetFullName() ) ), TEXT( "TrashcanReference" ) );
	}
}

/**
 * Deselects all selected prefab instances or actors belonging to prefab instances.  If a prefab
 * instance is selected, or if all actors in the prefab are selected, record the prefab.
 *
 * @param	OutPrefabInstances		[out] The set of prefab instances that were selected.
 * @param	bNotify					If TRUE, call NoteSelectionChange if any actors were deselected.
 */
void UEditorEngine::DeselectActorsBelongingToPrefabs(TArray<APrefabInstance*>& OutPrefabInstances, UBOOL bNotify)
{
	OutPrefabInstances.Empty();

	TArray<AActor*> ActorsToDeselect;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		const UBOOL bIsAPrefabInstance = Actor->IsA( APrefabInstance::StaticClass() );
		if( bIsAPrefabInstance )
		{
			// The selected actor is a prefab instance.
			APrefabInstance* PrefabInstance = static_cast<APrefabInstance*>(Actor);
			OutPrefabInstances.AddUniqueItem( PrefabInstance );
			ActorsToDeselect.AddItem( Actor );
		}
		else
		{
			// Check if the selected actor is part of a prefab instance.
			APrefabInstance* PrefabInstance = Actor->FindOwningPrefabInstance();
			if ( PrefabInstance )
			{
				const UBOOL bAllPrefabActorsSelected = PrefabInstance->GetActorSelectionStatus( TRUE );
				if ( bAllPrefabActorsSelected )
				{
					OutPrefabInstances.AddUniqueItem( PrefabInstance );
				}
				ActorsToDeselect.AddItem( Actor );
			}
		}
	}

	// Deselect marked actors.
	if ( ActorsToDeselect.Num() )
	{
		GetSelectedActors()->Modify();
		for( INT ActorIndex = 0 ; ActorIndex < ActorsToDeselect.Num() ; ++ActorIndex )
		{
			AActor* Actor = ActorsToDeselect(ActorIndex);
			SelectActor( Actor, FALSE, NULL, FALSE );
		}

		if ( bNotify )
		{
			NoteSelectionChange();
		}
	}
}

namespace MoveSelectedActors {
/**
 * A collection of actors and prefabs to move that all belong to the same level.
 */
class FCopyJob
{
public:
	/** A list of actors to move. */
	TArray<AActor*>	Actors;

	/** A list of prefabs to move. */
	TArray<APrefabInstance*> PrefabInstances;

	/** The index of the selected surface to copy. */
	INT SurfaceIndex;

	/** The source level that all actors in the Actors array and/or selected BSP surface come from. */
	ULevel*			SrcLevel;

	explicit FCopyJob( ULevel* SourceLevel )
		:	SurfaceIndex(INDEX_NONE)
		,	SrcLevel(SourceLevel)
	{
		check(SrcLevel);
	}

	/**
	* Moves the job's actors to the destination level.  The move happens via the
	* buffer level if one is specified; this is so that references are cleared
	* when the source actors refer to objects whose names also exist in the destination
	* level.  By serializing through a temporary level, the references are cleanly
	* severed.
	*
	* @param	OutNewActors			[out] Newly created actors are appended to this list.
	* @param	OutNewPrefabInstances	[out] Newly created prefab instances are appended to this list.
	* @param	bIgnoreKismetReferenced	If TRUE, don't move actors referenced by Kismet.
	* @param	DestLevel				The level to duplicate the actors in this job to.
	*/
	void MoveActorsToLevel(TArray<AActor*>& OutNewActors, TArray<APrefabInstance*>& OutNewPrefabInstances, ULevel* DestLevel, ULevel* BufferLevel, UBOOL bIgnoreKismetReferenced, UBOOL bCopyOnly, FString* OutClipboardContents )
	{
		GWorld->CurrentLevel = SrcLevel;

		// Set the selection set to be precisely the actors belonging to this job,
		// but make sure not to deselect selected BSP surfaces. 
		GEditor->SelectNone( FALSE, TRUE );
		for ( INT ActorIndex = 0 ; ActorIndex < Actors.Num() ; ++ActorIndex )
		{
			AActor* Actor = Actors( ActorIndex );
			const UBOOL bRejectBecauseOfKismetReference = bIgnoreKismetReferenced && Actor->IsReferencedByKismet();
			if ( !bRejectBecauseOfKismetReference )
			{
				GEditor->SelectActor( Actor, TRUE, NULL, FALSE );
			}

			// Groups cannot contain actors in different levels.  If the current actor is in a group but not being moved to the same level as the group
			// then remove the actor from the group
			AGroupActor* GroupActor = AGroupActor::GetParentForActor( Actor );
			if( GroupActor && GroupActor->GetLevel() != DestLevel )
			{
				GroupActor->Remove( *Actor );
			}
		}

		FString ScratchData;

		// Cut actors from src level.
		// edactCopySelected deselects prefab actors before exporting.  Pass FALSE edactCopySelected so that prefab
		// actors are not reselected, so that edactDeleteSelected won't delete them (as they weren't exported).
		GEditor->edactCopySelected( FALSE, FALSE, &ScratchData );

		if( !bCopyOnly )
		{
			const UBOOL bSuccess = GEditor->edactDeleteSelected( FALSE, bIgnoreKismetReferenced );
			if ( !bSuccess )
			{
				// The deletion was aborted.
				GWorld->CurrentLevel = SrcLevel;
				GEditor->SelectNone( FALSE, TRUE );
				return;
			}
		}

		if ( BufferLevel )
		{
			// Paste to the buffer level.
			GWorld->CurrentLevel = BufferLevel;
			GEditor->edactPasteSelected( TRUE, FALSE, FALSE, &ScratchData );

			const UBOOL bCopySurfaceToBuffer = (SurfaceIndex != INDEX_NONE);
			UModel* OldModel = BufferLevel->Model;

			if( bCopySurfaceToBuffer )
			{
				// When copying surfaces, we need to override the level's UModel to 
				// point to the existing UModel containing the BSP surface. This is 
				// because a buffer level is setup with an empty UModel.
				BufferLevel->Model = SrcLevel->Model;

				// Select the surface because we deselected everything earlier because 
				// we wanted to deselect all but the first selected BSP surface.
				GEditor->SelectBSPSurf( BufferLevel->Model, SurfaceIndex, TRUE, FALSE );
			}

			// Cut Actors from the buffer level.
			GWorld->CurrentLevel = BufferLevel;
			GEditor->edactCopySelected( FALSE, FALSE, &ScratchData );

			if( bCopySurfaceToBuffer )
			{
				// Deselect the surface.
				GEditor->SelectBSPSurf( BufferLevel->Model, SurfaceIndex, FALSE, FALSE );

				// Restore buffer level's original empty UModel
				BufferLevel->Model = OldModel;
			}
			
			if( OutClipboardContents != NULL )
			{
				*OutClipboardContents = *ScratchData;
			}

			GEditor->edactDeleteSelected( FALSE, bIgnoreKismetReferenced );
		}

		if( DestLevel )
		{
			// Paste to the dest level.
			GWorld->CurrentLevel = DestLevel;
			//A hidden level must be shown first, otherwise the paste will fail (it will not properly import the properties because that is based on selection)
			UBOOL bReHideLevel = !FLevelUtils::IsLevelVisible(DestLevel);
			if (bReHideLevel)
			{
				const UBOOL bShouldBeVisible = TRUE;
				const UBOOL bForceGroupsVisible = FALSE;
				ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( DestLevel );
				FLevelUtils::SetLevelVisibility(LevelStreaming, DestLevel, bShouldBeVisible, bForceGroupsVisible);
			}

			GEditor->edactPasteSelected( TRUE, FALSE, FALSE, &ScratchData );

			//if the level was hidden, hide it again
			if (bReHideLevel)
			{
				//empty selection
				GEditor->SelectNone( FALSE, TRUE );

				const UBOOL bShouldBeVisible = FALSE;
				const UBOOL bForceGroupsVisible = FALSE;
				ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( DestLevel );
				FLevelUtils::SetLevelVisibility(LevelStreaming, DestLevel, bShouldBeVisible, bForceGroupsVisible);
			}
		}

		// The current selection set is the actors that were moved during this job; copy them over to the output array.
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );
			OutNewActors.AddItem( Actor );
		}

		/////////////////
		// Move prefabs
		TArray<APrefabInstance*> PrefabsToDelete;
		for ( INT PrefabIndex = 0 ; PrefabIndex < PrefabInstances.Num() ; ++PrefabIndex )
		{
			APrefabInstance* SrcPrefabInstance = PrefabInstances(PrefabIndex);
			UPrefab* TemplatePrefab = SrcPrefabInstance->TemplatePrefab;

			APrefabInstance* NewPrefabInstance = TemplatePrefab
				? GEditor->Prefab_InstancePrefab( TemplatePrefab, SrcPrefabInstance->Location, SrcPrefabInstance->Rotation )
				: NULL;

			if ( NewPrefabInstance )
			{
				OutNewPrefabInstances.AddItem( NewPrefabInstance );
				PrefabsToDelete.AddItem( SrcPrefabInstance );
			}
			else
			{
				debugf( TEXT("Failed to move prefab %s into level %s"), *SrcPrefabInstance->GetPathName(), *GWorld->CurrentLevel->GetName() );
			}
		}

		if( !bCopyOnly )
		{
			// Delete prefabs that were instanced into the new level.
			GWorld->CurrentLevel = SrcLevel;
			GEditor->SelectNone( FALSE, TRUE );
			if (PrefabInstances.Num())
			{
				for ( INT PrefabIndex = 0 ; PrefabIndex < PrefabInstances.Num() ; ++PrefabIndex )
				{
					APrefabInstance* SrcPrefabInstance = PrefabInstances(PrefabIndex);
					GEditor->SelectActor( SrcPrefabInstance, TRUE, NULL, FALSE );
				}
				GEditor->edactDeleteSelected( FALSE, bIgnoreKismetReferenced );
			}
		}
	}

};
} // namespace MoveSelectedActors

/**
 * Moves selected actors to the current level.
 *
 * @param	bUseCurrentLevelGridVolume	If true, moves actors to an appropriately level associated with the current level grid volume (if there is one.)  If there is no current level grid volume, the actors will be moved to the current level.
 */
void UEditorEngine::MoveSelectedActorsToCurrentLevel( const UBOOL bUseCurrentLevelGridVolume )
{
	// suppress updating of the level browser window - we'll do this ONCE when we've finished updating things (otherwise, the update, which can be slow on large worlds, will be fired several times)
	WxLevelBrowser* LevelBrowser = GUnrealEd->GetBrowser<WxLevelBrowser>( TEXT("LevelBrowser") );
	LevelBrowser->SuppressUpdates( TRUE );

	// do the actual work...
	DoMoveSelectedActorsToCurrentLevel( bUseCurrentLevelGridVolume );

	// restore updates
	LevelBrowser->SuppressUpdates( FALSE );

	// Queue a level browser update because the actor counts may have changed
	GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );
}

void UEditorEngine::DoMoveSelectedActorsToCurrentLevel( const UBOOL bUseCurrentLevelGridVolume )
{
	using namespace MoveSelectedActors;

	// Make sure there is at least one actor selected
	if( GetSelectedActorCount() == 0 )
	{
		return;
	}

	// Grab the location of the first selected actor.  Even though there may be many actors selected
	// we'll make sure we find an appropriately destination level for the first actor.  The other
	// actors will be moved to their appropriate levels automatically afterwards.
	const FVector& FirstSelectedActorLocation = CastChecked< AActor >( *GetSelectedActorIterator() )->Location;


	// Select a destination level for the actors.  Usually this will be the CurrentLevel, but 
	// if the level grid volume is set as 'current', the we'll choose a level to place the
	// actors in right here, based on the location of the source actors.
	ULevel* DestLevel = EditorLevelUtils::GetLevelForPlacingNewActor( FirstSelectedActorLocation, bUseCurrentLevelGridVolume );
	checkSlow( DestLevel != NULL );


	// Make sure the desired level is current
	ULevel* OldCurrentLevel = GWorld->CurrentLevel;
	ALevelGridVolume* OldCurrentLevelGridVolume = GWorld->CurrentLevelGridVolume;
	GWorld->CurrentLevel = DestLevel;


	// Make sure the target level isn't locked
	if( FLevelUtils::IsLevelLocked( DestLevel ) )
	{
		// Destination level is locked.  We can't move an actor to this level right now.
		appMsgf(AMT_OK, TEXT("MoveActorsToCurrentLevel: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
		return;
	}

	// Make sure none of the selected actors are in a locked level, as we're about to delete
	// the actors from those levels!
	{
		for( FSelectionIterator CurActorIt( GetSelectedActorIterator() ); CurActorIt; ++CurActorIt )
		{
			AActor* CurActor = static_cast<AActor*>( *CurActorIt );
			if( ensure( CurActor->IsA( AActor::StaticClass() ) ) )
			{
				// Is the actor's level locked?
				if( FLevelUtils::IsLevelLocked( CurActor ) )
				{
					// Actor's level is locked.  We can't move an actor from this level right now.
					appMsgf(AMT_OK, TEXT("MoveActorsToCurrentLevel: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
					return;
				}
			}

			ALandscape* Landscape = Cast<ALandscape>( CurActor );
			if ( Landscape && !Landscape->HasAllComponent() )
			{
				appMsgf(AMT_OK, TEXT("MoveActorsToCurrentLevel: %s"), *FString::Printf(LocalizeSecure(LocalizeUnrealEd(TEXT("Error_LandscapeMoveToLevel")), *Landscape->GetName())) );
				return;
			}

			ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(CurActor);
			if ( Proxy )
			{
				Proxy->bIsMovingToLevel = TRUE;
			}
		}
	}

	// Provide the option to abort up-front.
	UBOOL bIgnoreKismetReferenced = FALSE;
	if ( ShouldAbortActorDeletion( bIgnoreKismetReferenced ) )
	{
		return;
	}

	const FScopedBusyCursor BusyCursor;

	// Get the list of selected prefabs.
	TArray<APrefabInstance*> SelectedPrefabInstances;
	DeselectActorsBelongingToPrefabs( SelectedPrefabInstances, FALSE );

	// Create per-level job lists.
	typedef TMap<ULevel*, FCopyJob*>	CopyJobMap;
	CopyJobMap							CopyJobs;

	// Keep track of groups that need to be added
	TArray<AGroupActor*> GroupsToAdd;

	// Add selected actors to the per-level job lists.
	UBOOL bProxiesRemoved = FALSE;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// See if a group selected to be added into the new group
		AGroupActor* GroupActor = Cast<AGroupActor>(Actor);
		if(GroupActor == NULL) // Aren't directly selecting a group, see if the actor has a locked parent
		{
			GroupActor = AGroupActor::GetParentForActor(Actor);
			if(GroupActor)
			{
				// If the grouping active and the actor has a locked parent, add it.
				if (GUnrealEd->bGroupingActive && GroupActor->IsLocked())
				{
					GroupsToAdd.AddUniqueItem(GroupActor);

					// Add the root group as well if its locked
					AGroupActor* RootGroupActor = AGroupActor::GetRootForActor(Actor, FALSE, TRUE);
					if (RootGroupActor && RootGroupActor->IsLocked())
					{
						GroupsToAdd.AddUniqueItem(RootGroupActor);
					}

					continue;	// Don't add this to the job list if it's locked
				}
				else
				{
					// If this actor is part of the proxy, and we're not moving the group it belongs to, don't move the proxy either!
					AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(Actor);
					if (SMActor && (SMActor->IsProxy() || SMActor->IsHiddenByProxy()))
					{
						if ( !bProxiesRemoved )	// Only inform the user once
						{
							bProxiesRemoved = TRUE;
							appMsgf( AMT_OK, *LocalizeUnrealEd("Error_CannotMoveProxy") );
						}

						continue;
					}
				}
			}
		}

		ULevel* OldLevel = Actor->GetLevel();
		FCopyJob** Job = CopyJobs.Find( OldLevel );
		if ( Job )
		{
			(*Job)->Actors.AddItem( Actor );
		}
		else
		{
			// Allocate a new job for the level.
			FCopyJob* NewJob = new FCopyJob(OldLevel);
			NewJob->Actors.AddItem( Actor );
			CopyJobs.Set( OldLevel, NewJob );
		}
	}

	// Remove sub groups to avoid loosing hierarchy when doing our adds below.
	AGroupActor::RemoveSubGroupsFromArray(GroupsToAdd);
	for(INT GroupIdx=0; GroupIdx < GroupsToAdd.Num(); ++GroupIdx)
	{
		AGroupActor* GroupToAdd = GroupsToAdd(GroupIdx);
#if ENABLE_SIMPLYGON_MESH_PROXIES
		// Revert any proxies that belong to the group, they can't be moved across levels
		WxDlgCreateMeshProxy*	DlgCreateMeshProxy = GApp->GetDlgCreateMeshProxy();
		check(DlgCreateMeshProxy);
		if ( DlgCreateMeshProxy->UpdateActorGroup( NULL, GroupToAdd, NULL, FALSE, TRUE, FALSE ) )	// Ungroup the proxy, but maintain the hiddenflags
		{
			GroupToAdd->SetRemergeProxy( TRUE );	// Flag that this should be remade into a proxy, post-move
		}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
		ULevel* OldLevel = GroupToAdd->GetLevel();
		FCopyJob** Job = CopyJobs.Find( OldLevel );
		if ( Job )
		{
			(*Job)->Actors.AddItem( GroupToAdd );
		}
		else
		{
			// Allocate a new job for the level.
			FCopyJob* NewJob = new FCopyJob(OldLevel);
			NewJob->Actors.AddItem( GroupToAdd );
			CopyJobs.Set( OldLevel, NewJob );
		}
	}

	// Add prefab instances to the per-level job lists.
	for ( INT Index = 0 ; Index < SelectedPrefabInstances.Num() ; ++Index )
	{
		APrefabInstance* PrefabInstance = SelectedPrefabInstances(Index);
		ULevel* PrefabLevel = PrefabInstance->GetLevel();
		FCopyJob** Job = CopyJobs.Find( PrefabLevel );
		if ( Job )
		{
			(*Job)->PrefabInstances.AddItem( PrefabInstance );
		}
		else
		{
			// Allocate a new job for the level.
			FCopyJob* NewJob = new FCopyJob(PrefabLevel);
			NewJob->PrefabInstances.AddItem( PrefabInstance );
			CopyJobs.Set( PrefabLevel, NewJob );
		}
	}

	if ( CopyJobs.Num() > 0 )
	{
		// Display a progress dialog
		const UBOOL bShowProgressDialog = TRUE;
		GWarn->BeginSlowTask( *LocalizeUnrealEd( TEXT( "LevelOperationProgress_MoveActorsToLevel" ) ), bShowProgressDialog );

		ULevel* BufferLevel = new(GWorld, TEXT("TransLevelMoveBuffer")) ULevel(FURL(NULL));
		BufferLevel->AddToRoot();

		if (BufferLevel)
		{
			BufferLevel->Model = new(BufferLevel) UModel(NULL, TRUE);
			BufferLevel->SetFlags( RF_Transactional );
			BufferLevel->Model->SetFlags( RF_Transactional );

			// The buffer needs to be the current level to spawn actors into.
			GWorld->CurrentLevel = BufferLevel;

			if (BufferLevelActors.Num() == 0)
			{
				// Spawn worldinfo.
				GWorld->SpawnActor( AWorldInfo::StaticClass() );
				check( Cast<AWorldInfo>( BufferLevel->Actors(0) ) );

				//////////////////////////// Based on UWorld::Init()
				// Spawn builder brush for the buffer level.
				ABrush* BufferDefaultBrush = CastChecked<ABrush>( GWorld->SpawnActor( ABrush::StaticClass() ) );
				check( BufferDefaultBrush->BrushComponent );
				BufferDefaultBrush->Brush = new( GWorld->GetOuter(), TEXT("Brush") )UModel( BufferDefaultBrush, 1 );
				BufferDefaultBrush->BrushComponent->Brush = BufferDefaultBrush->Brush;
				BufferDefaultBrush->SetFlags( RF_NotForClient | RF_NotForServer | RF_Transactional );
				BufferDefaultBrush->Brush->SetFlags( RF_NotForClient | RF_NotForServer | RF_Transactional );

				// Find the index in the array the default brush has been spawned at. Not necessarily
				// the last index as the code might spawn the default physics volume afterwards.
				const INT DefaultBrushActorIndex = BufferLevel->Actors.FindItemIndex( BufferDefaultBrush );

				// The default brush needs to reside at index 1.
				Exchange(BufferLevel->Actors(1),BufferLevel->Actors(DefaultBrushActorIndex));

				// Re-sort actor list as we just shuffled things around.
				BufferLevel->SortActorList();
				////////////////////////////

				GWorld->Levels.AddUniqueItem( BufferLevel );

				// Associate buffer level's actors with persistent world info actor and update zoning.
				AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
				for( INT ActorIndex = 0 ; ActorIndex < BufferLevel->Actors.Num() ; ++ActorIndex )
				{
					AActor* Actor = BufferLevel->Actors(ActorIndex);
					if( Actor )
					{
						Actor->WorldInfo = WorldInfo;
						Actor->SetZone( 0, 1 );
					}
				}

				BufferLevel->UpdateComponents();
			}
			else
			{
				// Restore the buffer level's actor list
				BufferLevel->Actors.Append(BufferLevelActors);
				GWorld->Levels.AddUniqueItem( BufferLevel );
			}
		}

		{
			const FScopedTransaction Transaction( TEXT("Move Actors Across Levels") );
			GetSelectedActors()->Modify();

			// For each level, select the actors in that level and copy-paste into the destination level.
			TArray<AActor*>	NewActors;
			TArray<APrefabInstance*> NewPrefabInstances;
			for ( CopyJobMap::TIterator It( CopyJobs ) ; It ; ++It )
			{
				FCopyJob* Job = It.Value();
				check( Job );

				// Do nothing if the source and destination levels match.
				if ( Job->SrcLevel != DestLevel )
				{
					const UBOOL bCopyOnly = FALSE;
					Job->MoveActorsToLevel( NewActors, NewPrefabInstances, DestLevel, BufferLevel, bIgnoreKismetReferenced, bCopyOnly, NULL );
				}
			}

			// Select any moved actors and prefabs.
			SelectNone( FALSE, TRUE );
			for ( INT NewActorIndex = 0 ; NewActorIndex < NewActors.Num() ; ++NewActorIndex )
			{
				AActor* Actor = NewActors(NewActorIndex);
				SelectActor( Actor, TRUE, NULL, FALSE, TRUE );
			}
			for ( INT PrefabIndex = 0 ; PrefabIndex < NewPrefabInstances.Num() ; ++PrefabIndex )
			{
				APrefabInstance* PrefabInstance = NewPrefabInstances( PrefabIndex );
				SelectActor( PrefabInstance, TRUE, NULL, FALSE, TRUE );
			}
		}

		// Cleanup.
		for ( CopyJobMap::TIterator It( CopyJobs ) ; It ; ++It )
		{
			FCopyJob* Job = It.Value();
			delete Job;
		}

		// Clean-up flag for Landscape Proxy cases...
		for( FActorIterator CurActorIt; CurActorIt; ++CurActorIt )
		{
			ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*CurActorIt);
			if (Proxy)
			{
				Proxy->bIsMovingToLevel = FALSE;
			}
		}

		// Save off the buffer level's actor list
		BufferLevelActors.Empty();
		BufferLevelActors.Append(BufferLevel->Actors);

		BufferLevel->ClearComponents();
		GWorld->Levels.RemoveItem( BufferLevel );
		BufferLevel->RemoveFromRoot();


		// End progress display
		GWarn->EndSlowTask();
	}

	// Check to see if any of the groups need their proxy remerging
	AGroupActor::RemergeActiveGroups();

	// Return the current world to the destination package.
	GWorld->CurrentLevel = OldCurrentLevel;
	GWorld->CurrentLevelGridVolume = OldCurrentLevelGridVolume;

	GEditor->NoteSelectionChange();
}



/**
 * Copies selected actors to the clipboard.  Supports copying actors from multiple levels.
 * NOTE: Doesn't support copying prefab instance actors!
 *
 * @param bShouldCut If TRUE, deletes the selected actors after copying them to the clipboard
 * @param bShouldClipPadCanBeUsed If TRUE and the SHIFT key is held down, the actors will be copied to the clip pad
 */
void UEditorEngine::CopySelectedActorsToClipboard( UBOOL bShouldCut, UBOOL bClipPadCanBeUsed )
{
	using namespace MoveSelectedActors;

	// For faster performance, if all actors belong to the same level then we can just go ahead and copy normally
	UBOOL bAllActorsInSameLevel = TRUE;
	ULevel* LevelAllActorsAreIn = NULL;
	{
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if( LevelAllActorsAreIn == NULL )
			{
				LevelAllActorsAreIn = Actor->GetLevel();
			}

			if( Actor->GetLevel() != LevelAllActorsAreIn )
			{
				bAllActorsInSameLevel = FALSE;
				LevelAllActorsAreIn = NULL;
				break;
			}
		}
	}

	// Next, check for selected BSP surfaces. 
	ULevel* LevelWithSelectedSurface = NULL;
	UBOOL bHasSelectedSurfaces = FALSE;

	for( TSelectedSurfaceIterator<> SurfaceIter; SurfaceIter; ++SurfaceIter )
	{
		ULevel* OwningLevel = GWorld->Levels(SurfaceIter.GetLevelIndex());

		if( LevelWithSelectedSurface == NULL )
		{
			LevelWithSelectedSurface = OwningLevel;
			bHasSelectedSurfaces = TRUE;
		}

		if( OwningLevel != LevelWithSelectedSurface )
		{
			LevelWithSelectedSurface = NULL;
			break;
		}
	}

	UBOOL bCanPerformQuickCopy = FALSE;

	// If there are selected actors and BSP surfaces AND all selected actors 
	// and surfaces are in the same level, we can do a quick copy.
	if( LevelAllActorsAreIn && LevelWithSelectedSurface )
	{
		bCanPerformQuickCopy = ( LevelWithSelectedSurface == LevelAllActorsAreIn );
	}
	// Else, if either we have selected actors all in one level OR we have 
	// selected surfaces all in one level, then we can perform a quick copy. 
	else
	{
		bCanPerformQuickCopy = (LevelWithSelectedSurface != NULL) || (LevelAllActorsAreIn != NULL);
	}

	// Perform a quick copy if all the conditions are right. 
	if( bCanPerformQuickCopy )
	{
		// Keep track of the previous 'current level' so we can restore it later
		ULevel* OldCurrentLevel = GWorld->CurrentLevel;

		// It's possible that no actors are actually selected right now, in which case our Level
		// pointer will be NULL.  We still want to run through the regular Copy code though.
		if( LevelAllActorsAreIn != NULL )
		{
			// The buffer needs to be the current level to spawn actors into.
			GWorld->CurrentLevel = LevelAllActorsAreIn;
		}
		// Otherwise, we might just have a selected BSP surface. 
		else if( LevelWithSelectedSurface != NULL )
		{
			GWorld->CurrentLevel = LevelWithSelectedSurface;
		}

		if( bShouldCut )
		{
			// Cut!
			const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("Cut")) );
			edactCopySelected( TRUE, TRUE );
			edactDeleteSelected();
		}
		else
		{
			// Copy!
			edactCopySelected( TRUE, TRUE );
		}

		// Restore current level in case we needed to change it
		GWorld->CurrentLevel = OldCurrentLevel;
	}
	else
	{
		// OK, we'll use a copy method that supports cleaning up references for actors in multiple levels

		UBOOL bIgnoreKismetReferenced = FALSE;
		if( bShouldCut )
		{
			// Provide the option to abort up-front.
			if ( ShouldAbortActorDeletion( bIgnoreKismetReferenced ) )
			{
				return;
			}
		}

		const FScopedBusyCursor BusyCursor;

		// We don't support clipboard copies of prefab instances, so we just deselect those.
		TArray<APrefabInstance*> DeselectedPrefabInstances;
		DeselectActorsBelongingToPrefabs( DeselectedPrefabInstances, FALSE );

		// If we have selected actors and/or selected BSP surfaces, we need to setup some copy jobs. 
		if( GetSelectedActorCount() > 0 || bHasSelectedSurfaces )
		{
			// Create per-level job lists.
			typedef TMap<ULevel*, FCopyJob*>	CopyJobMap;
			CopyJobMap							CopyJobs;

			// First, create new copy jobs for BSP surfaces if we have selected surfaces. 
			if( bHasSelectedSurfaces )
			{
				// Create copy job for the selected surfaces that need copying.
				for( TSelectedSurfaceIterator<> SurfaceIter; SurfaceIter; ++SurfaceIter )
				{
					ULevel* LevelWithSelectedSurface = GWorld->Levels(SurfaceIter.GetLevelIndex());

					// Currently, we only support one selected surface per level. So, If the 
					// level is already in the map, we don't need to copy this surface. 
					if( !CopyJobs.Find(LevelWithSelectedSurface) )
					{
						FCopyJob* NewJob = new FCopyJob( LevelWithSelectedSurface );
						NewJob->SurfaceIndex = SurfaceIter.GetSurfaceIndex();

						check( NewJob->SurfaceIndex != INDEX_NONE );

						CopyJobs.Set( NewJob->SrcLevel, NewJob );
					}
				}
			}

			// Add selected actors to the per-level job lists.
			for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				ULevel* OldLevel = Actor->GetLevel();
				FCopyJob** Job = CopyJobs.Find( OldLevel );
				if ( Job )
				{
					(*Job)->Actors.AddItem( Actor );
				}
				else
				{
					// Allocate a new job for the level.
					FCopyJob* NewJob = new FCopyJob(OldLevel);
					NewJob->Actors.AddItem( Actor );
					CopyJobs.Set( OldLevel, NewJob );
				}
			}


			if ( CopyJobs.Num() > 0 )
			{
				// Keep track of the previous 'current level' so we can restore it later
				ULevel* OldCurrentLevel = GWorld->CurrentLevel;

				// Create a buffer level that actors will be moved through to cleanly break references.
				// Create a new UWorld, ULevel and UModel.
				UPackage* BufferPackage	= UObject::GetTransientPackage();

				ULevel* BufferLevel		= new( GWorld, TEXT("TransLevelMoveBuffer")	) ULevel( FURL(NULL) );
				BufferLevel->AddToRoot();

				BufferLevel->Model = new( BufferLevel ) UModel( NULL, TRUE );

				if ( BufferLevel )
				{
					BufferLevel->SetFlags( RF_Transactional );
					BufferLevel->Model->SetFlags( RF_Transactional );

					// The buffer needs to be the current level to spawn actors into.
					GWorld->CurrentLevel = BufferLevel;

					// Spawn worldinfo.
					GWorld->SpawnActor( AWorldInfo::StaticClass() );
					check( Cast<AWorldInfo>( BufferLevel->Actors(0) ) );

					//////////////////////////// Based on UWorld::Init()
					// Spawn builder brush for the buffer level.
					ABrush* BufferDefaultBrush = CastChecked<ABrush>( GWorld->SpawnActor( ABrush::StaticClass() ) );
					check( BufferDefaultBrush->BrushComponent );
					BufferDefaultBrush->Brush = new( GWorld->GetOuter(), TEXT("Brush") )UModel( BufferDefaultBrush, 1 );
					BufferDefaultBrush->BrushComponent->Brush = BufferDefaultBrush->Brush;
					BufferDefaultBrush->SetFlags( RF_NotForClient | RF_NotForServer | RF_Transactional );
					BufferDefaultBrush->Brush->SetFlags( RF_NotForClient | RF_NotForServer | RF_Transactional );

					// Find the index in the array the default brush has been spawned at. Not necessarily
					// the last index as the code might spawn the default physics volume afterwards.
					const INT DefaultBrushActorIndex = BufferLevel->Actors.FindItemIndex( BufferDefaultBrush );

					// The default brush needs to reside at index 1.
					Exchange(BufferLevel->Actors(1),BufferLevel->Actors(DefaultBrushActorIndex));

					// Re-sort actor list as we just shuffled things around.
					BufferLevel->SortActorList();
					////////////////////////////

					GWorld->Levels.AddUniqueItem( BufferLevel );

					// Associate buffer level's actors with persistent world info actor and update zoning.
					AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
					for( INT ActorIndex = 0 ; ActorIndex < BufferLevel->Actors.Num() ; ++ActorIndex )
					{
						AActor* Actor = BufferLevel->Actors(ActorIndex);
						if( Actor )
						{
							Actor->WorldInfo = WorldInfo;
							Actor->SetZone( 0, 1 );
						}
					}

					BufferLevel->UpdateComponents();
				}

				{
					// We'll build up our final clipboard string with the result of each copy
					FString ClipboardString;

					if( bShouldCut )
					{
						GEditor->Trans->Begin( TEXT("Cut") );
						GetSelectedActors()->Modify();
					}

					// For each level, select the actors in that level and copy-paste into the destination level.
					TArray<AActor*>	NewActors;
					TArray<APrefabInstance*> NewPrefabInstances;
					for ( CopyJobMap::TIterator It( CopyJobs ) ; It ; ++It )
					{
						FCopyJob* Job = It.Value();
						check( Job );

						FString CopiedActorsString;
						const UBOOL bCopyOnly = !bShouldCut;
						Job->MoveActorsToLevel( NewActors, NewPrefabInstances, NULL, BufferLevel, bIgnoreKismetReferenced, bCopyOnly, &CopiedActorsString );

						// Append our copied actors to our final clipboard string
						ClipboardString += CopiedActorsString;
					}

					if( bShouldCut )
					{
						GEditor->Trans->End();
					}

					// Update the clipboard with the final string
					appClipboardCopy( *ClipboardString );

					// If the clip pad is being used...
					if( bClipPadCanBeUsed && (GetAsyncKeyState(VK_SHIFT) & 0x8000) )
					{	
						GEditor->PasteClipboardIntoClipPad();
					}

					// Restore current level in case we needed to change it
					GWorld->CurrentLevel = OldCurrentLevel;
				}

				// Cleanup.
				for ( CopyJobMap::TIterator It( CopyJobs ) ; It ; ++It )
				{
					FCopyJob* Job = It.Value();
					delete Job;
				}

				// Clean-up flag for Landscape Proxy cases...
				for( FActorIterator CurActorIt; CurActorIt; ++CurActorIt )
				{
					ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*CurActorIt);
					if (Proxy)
					{
						Proxy->bIsMovingToLevel = FALSE;
					}
				}

				BufferLevel->ClearComponents();
				GWorld->Levels.RemoveItem( BufferLevel );
				BufferLevel->RemoveFromRoot();
			}
		}

		if( DeselectedPrefabInstances.Num() > 0 || bShouldCut )
		{
			// Reselect actors that were deselected for being or belonging to prefabs.
			for( INT ActorIndex = 0 ; ActorIndex < DeselectedPrefabInstances.Num() ; ++ActorIndex )
			{
				AActor* Actor = DeselectedPrefabInstances(ActorIndex);
				GetSelectedActors()->Select( Actor );
			}

			GEditor->NoteSelectionChange();
		}
	}
}


namespace
{
	/** Property value used for property-based coloration. */
	static FString				GPropertyColorationValue;

	/** Property used for property-based coloration. */
	static UProperty*			GPropertyColorationProperty = NULL;

	/** Class of object to which property-based coloration is applied. */
	static UClass*				GPropertyColorationClass = NULL;

	/** TRUE if GPropertyColorationClass is an actor class. */
	static UBOOL				GbColorationClassIsActor = FALSE;

	/** TRUE if GPropertyColorationProperty is an object property. */
	static UBOOL				GbColorationPropertyIsObjectProperty = FALSE;

	/** The chain of properties from member to lowest priority*/
	static FEditPropertyChain*	GPropertyColorationChain = NULL;

	/** Used to collect references to actors that match the property coloration settings. */
	static TArray<AActor*>*		GPropertyColorationActorCollector = NULL;
}

/**
 * Sets property value and property chain to be used for property-based coloration.
 *
 * @param	PropertyValue		The property value to color.
 * @param	Property			The property to color.
 * @param	CommonBaseClass		The class of object to color.
 * @param	PropertyChain		The chain of properties from member to lowest property.
 */
void UEditorEngine::SetPropertyColorationTarget(const FString& PropertyValue, UProperty* Property, UClass* CommonBaseClass, FEditPropertyChain* PropertyChain)
{
	if ( GPropertyColorationProperty != Property || 
		GPropertyColorationClass != CommonBaseClass ||
		GPropertyColorationChain != PropertyChain ||
		GPropertyColorationValue != PropertyValue )
	{
		const FScopedBusyCursor BusyCursor;
		delete GPropertyColorationChain;

		GPropertyColorationValue = PropertyValue;
		GPropertyColorationProperty = Property;
		GPropertyColorationClass = CommonBaseClass;
		GPropertyColorationChain = PropertyChain;

		GbColorationClassIsActor = GPropertyColorationClass->IsChildOf( AActor::StaticClass() );
		GbColorationPropertyIsObjectProperty = Cast<UObjectProperty>(GPropertyColorationProperty,CLASS_IsAUObjectProperty) != NULL;

		GWorld->UpdateComponents( FALSE );
		RedrawLevelEditingViewports();
	}
}

/**
 * Accessor for current property-based coloration settings.
 *
 * @param	OutPropertyValue	[out] The property value to color.
 * @param	OutProperty			[out] The property to color.
 * @param	OutCommonBaseClass	[out] The class of object to color.
 * @param	OutPropertyChain	[out] The chain of properties from member to lowest property.
 */
void UEditorEngine::GetPropertyColorationTarget(FString& OutPropertyValue, UProperty*& OutProperty, UClass*& OutCommonBaseClass, FEditPropertyChain*& OutPropertyChain)
{
	OutPropertyValue	= GPropertyColorationValue;
	OutProperty			= GPropertyColorationProperty;
	OutCommonBaseClass	= GPropertyColorationClass;
	OutPropertyChain	= GPropertyColorationChain;
}

/**
 * Computes a color to use for property coloration for the given object.
 *
 * @param	Object		The object for which to compute a property color.
 * @param	OutColor	[out] The returned color.
 * @return				TRUE if a color was successfully set on OutColor, FALSE otherwise.
 */
UBOOL UEditorEngine::GetPropertyColorationColor(UObject* Object, FColor& OutColor)
{
	UBOOL bResult = FALSE;
	if ( GPropertyColorationClass && GPropertyColorationChain && GPropertyColorationChain->Num() > 0 )
	{
		UObject* MatchingBase = NULL;
		AActor* Owner = NULL;
		if ( Object->IsA(GPropertyColorationClass) )
		{
			// The querying object matches the coloration class.
			MatchingBase = Object;
		}
		else
		{
			// If the coloration class is an actor, check if the querying object is a component.
			// If so, compare the class of the component's owner against the coloration class.
			if ( GbColorationClassIsActor )
			{
				UActorComponent* ActorComponent = Cast<UActorComponent>( Object );
				if ( ActorComponent )
				{
					Owner = ActorComponent->GetOwner();
					if ( Owner && Owner->IsA( GPropertyColorationClass ) )
					{
						MatchingBase = Owner;
					}
				}
			}
		}

		// Do we have a matching object?
		if ( MatchingBase )
		{
			UBOOL bDontCompareProps = FALSE;

			BYTE* Base = (BYTE*) MatchingBase;
			INT TotalChainLength = GPropertyColorationChain->Num();
			INT ChainIndex = 0;
			for ( FEditPropertyChain::TIterator It(GPropertyColorationChain->GetHead()); It; ++It )
			{
				UProperty* Prop = *It;
				if( Cast<UArrayProperty>(Prop) )
				{
					// @todo DB: property coloration -- add support for array properties.
					bDontCompareProps = TRUE;
					break;
				}
				else if ( Cast<UObjectProperty>(Prop,CLASS_IsAUObjectProperty) && (ChainIndex != TotalChainLength-1))
				{
					BYTE* ObjAddr = Base + Prop->Offset;
					UObject* ReferencedObject = *(UObject**) ObjAddr;
					Base = (BYTE*) ReferencedObject;
				}
				else
				{
					Base += Prop->Offset;
				}
				ChainIndex++;
			}

			// Export the property value.  We don't want to exactly compare component properties.
			if ( !bDontCompareProps ) 
			{
				BYTE*	Data = Base - GPropertyColorationProperty->Offset;
				if ( Data )
				{
					FString PropertyValue;
					GPropertyColorationProperty->ExportText( 0, PropertyValue, Data, Data, NULL, PPF_Localized );
					if ( PropertyValue == GPropertyColorationValue )
					{
						bResult  = TRUE;
						OutColor = FColor(255,0,0);

						// Collect actor references.
						if ( GPropertyColorationActorCollector && Owner )
						{
							GPropertyColorationActorCollector->AddUniqueItem( Owner );
						}
					}
				}
			}
		}
	}
	return bResult;
}

/**
 * Selects actors that match the property coloration settings.
 */
void UEditorEngine::SelectByPropertyColoration()
{
	TArray<AActor*> Actors;
	GPropertyColorationActorCollector = &Actors;
	GWorld->UpdateComponents( FALSE );
	GPropertyColorationActorCollector = NULL;

	if ( Actors.Num() > 0 )
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("SelectByProperty")) );
		GetSelectedActors()->Modify();
		SelectNone( FALSE, TRUE );
		for ( INT ActorIndex = 0 ; ActorIndex < Actors.Num() ; ++ActorIndex )
		{
			AActor* Actor = Actors(ActorIndex);
			SelectActor( Actor, TRUE, NULL, FALSE );
		}
		NoteSelectionChange();
	}
}

/**
 * Checks map for common errors.
 */
UBOOL UEditorEngine::Map_Check(const TCHAR* Str, FOutputDevice& Ar, UBOOL bCheckDeprecatedOnly, UBOOL bClearExistingMessages, UBOOL bDisplayResultDialog/*=TRUE*/)
{
	const FString CheckMapLocString(LocalizeUnrealEd(TEXT("CheckingMap")));
	GWarn->BeginSlowTask( *CheckMapLocString, FALSE);
	
	GWarn->MapCheck_BeginBulkAdd();
	if ( bClearExistingMessages )
	{
		GWarn->MapCheck_Clear();
	}

	TMap<FGridBounds,AActor*>	GridBoundsToActorMap;
	TMap<FGuid,AActor*>			LightGuidToActorMap;
	const INT ProgressDenominator = FActorIteratorBase::GetProgressDenominator();

	if ( !bCheckDeprecatedOnly )
	{
		CheckLoadedLevelsForTrashcanReferences();

		// Report if any brush material references could be cleaned by running 'Clean BSP Materials'.
		const INT NumRefrencesCleared = CleanBSPMaterials( TRUE, FALSE );
		if ( NumRefrencesCleared > 0 )
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, NULL, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_CleanBSPMaterials" ), NumRefrencesCleared ) ), TEXT( "CleanBSPMaterials" ) );
		}
	}

	// Check to see if any of the streaming levels have streaming levels of their own
	// Grab the world info, and loop through the streaming levels
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	if ( WorldInfo )
	{
		for (INT LevelIndex = 0; LevelIndex < WorldInfo->StreamingLevels.Num(); LevelIndex++)
		{
			ULevelStreaming* LevelStreaming = WorldInfo->StreamingLevels(LevelIndex);
			if (LevelStreaming != NULL && LevelStreaming->LoadedLevel != NULL)
			{
				// Grab the world info of the streaming level, and loop through it's streaming levels
				AWorldInfo* SubLevelWorldInfo = CastChecked<AWorldInfo>(LevelStreaming->LoadedLevel->Actors(0));
				if (SubLevelWorldInfo)
				{
					for (INT SubLevelIndex=0; SubLevelIndex < SubLevelWorldInfo->StreamingLevels.Num(); SubLevelIndex++ )
					{
						// If it has any and they aren't loaded flag a warning to the user
						ULevelStreaming* SubLevelStreaming = SubLevelWorldInfo->StreamingLevels(SubLevelIndex);
						if (SubLevelStreaming != NULL && SubLevelStreaming->LoadedLevel == NULL)
						{							
							GWarn->MapCheck_Add( MCTYPE_WARNING, SubLevelWorldInfo, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_ContainsUnloadedStreamingLevel" ), *SubLevelStreaming->PackageName.ToString() ) ), TEXT( "ContainsUnloadedStreamingLevel" ) );
						}
					}
				}
			}
		}
	}

	// Make sure all levels in the world have a filename length less than the max limit
	// Filenames over the max limit interfere with cooking for consoles.
	const INT MaxFilenameLen = FEditorFileUtils::MAX_UNREAL_FILENAME_LENGTH;
	for ( INT LevelIndex = 0; LevelIndex < GWorld->Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = GWorld->Levels( LevelIndex );
		UPackage* LevelPackage = Level->GetOutermost();
		FFilename PackageFilename;
		if( GPackageFileCache->FindPackageFile( *LevelPackage->GetName(), NULL, PackageFilename ) && 
			PackageFilename.GetBaseFilename().Len() > MaxFilenameLen )
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, NULL, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_FilenameIsTooLongForCooking" ), *PackageFilename.GetBaseFilename(), MaxFilenameLen ) ), TEXT( "FilenameIsTooLongForCooking" ) );
		}
	}

	Game_Map_Check(Str, Ar, bCheckDeprecatedOnly);

	GWarn->StatusUpdatef( 0, ProgressDenominator, *CheckMapLocString );

	INT LastUpdateCount = 0;
	INT UpdateGranularity = ProgressDenominator / 5;
	for( FActorIterator It; It; ++It ) 
	{
		if(It.GetProgressNumerator() >= LastUpdateCount + UpdateGranularity)
		{
			GWarn->UpdateProgress( It.GetProgressNumerator(), ProgressDenominator );
			LastUpdateCount=It.GetProgressNumerator();
		}
		
		AActor* Actor = *It;
		if(bCheckDeprecatedOnly)
		{
			Actor->CheckForDeprecated();
		}
		else
		{
			Actor->CheckForErrors();
			
			// Determine actor location and bounds, falling back to actor location property and 0 extent
			FVector Center = Actor->Location;
			FVector Extent = FVector(0,0,0);
			AStaticMeshActor*	StaticMeshActor		= Cast<AStaticMeshActor>(Actor);
			ASkeletalMeshActor*	SkeletalMeshActor	= Cast<ASkeletalMeshActor>(Actor);
			ADynamicSMActor*	DynamicSMActor		= Cast<ADynamicSMActor>(Actor);
			ALight*				LightActor			= Cast<ALight>(Actor);
			AProcBuilding*		BuildingActor		= Cast<AProcBuilding>(Actor);
			UMeshComponent*		MeshComponent		= NULL;
			if( StaticMeshActor )
			{
				MeshComponent = StaticMeshActor->StaticMeshComponent;
			}
			else if( SkeletalMeshActor )
			{
				MeshComponent = SkeletalMeshActor->SkeletalMeshComponent;
			}
			else if( DynamicSMActor )
			{
				MeshComponent = DynamicSMActor->StaticMeshComponent;
			}

			// See whether there are lights that ended up with the same component. This was possible in earlier versions of the engine.
			if( LightActor )
			{
				ULightComponent* LightComponent = LightActor->LightComponent;
				AActor* ExistingLightActor = LightGuidToActorMap.FindRef( LightComponent->LightGuid );
				if( ExistingLightActor )
				{
					GWarn->MapCheck_Add( MCTYPE_WARNING, LightActor, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_MatchingLightGUID" ), *LightActor->GetName(), *ExistingLightActor->GetName() ) ), TEXT( "MatchingLightGUID" ) );
					GWarn->MapCheck_Add( MCTYPE_WARNING, ExistingLightActor, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_MatchingLightGUID" ), *ExistingLightActor->GetName(), *LightActor->GetName() ) ), TEXT( "MatchingLightGUID" ) );
				}
				else
				{
					LightGuidToActorMap.Set( LightComponent->LightGuid, LightActor );
				}
			}
			
			// Check buildings for non-approved rulesets
			if(BuildingActor && BuildingActor->Ruleset)
			{
				if(!GUnrealEd->CheckPBRulesetIsApproved(BuildingActor->Ruleset))
				{
					GWarn->MapCheck_Add( MCTYPE_WARNING, BuildingActor, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NonApprovedRuleset" ), *BuildingActor->Ruleset->GetPathName() ) ), TEXT( "NonApprovedRuleset" ) );
				}
			}

			// Use center of bounding box for location.
			if( MeshComponent )
			{
				Center = MeshComponent->Bounds.GetBox().GetCenter();
				Extent = MeshComponent->Bounds.GetBox().GetExtent();
			}

			// Check for two actors being in the same location.
			FGridBounds	GridBounds( Center, Extent );
			AActor*		ExistingActorInSameLocation = GridBoundsToActorMap.FindRef( GridBounds );		
			if( ExistingActorInSameLocation )
			{
				// We emit two warnings to allow easy double click selection.
//superville
// Disable same location warnings for now
//				GWarn->MapCheck_Add( MCTYPE_WARNING, Actor, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_ActorInSameLocation" ), *Actor->GetName(), *ExistingActorInSameLocation->GetName() ) ), TEXT( "ActorInSameLocation" ) );
//				GWarn->MapCheck_Add( MCTYPE_WARNING, ExistingActorInSameLocation, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_ActorInSameLocation" ), *ExistingActorInSameLocation->GetName(), *Actor->GetName() ) ), TEXT( "ActorInSameLocation" ) );
			}
			// We only care about placeable classes.
			else if( Actor->GetClass()->HasAllClassFlags( CLASS_Placeable ) )
			{
				GridBoundsToActorMap.Set( GridBounds, Actor );
			}
		}

		Game_Map_Check_Actor(Str, Ar, bCheckDeprecatedOnly, Actor);
	}

	// Check for navigation point GUID collisions
	{
		TMap< FGuid, const ANavigationPoint* > NavGuids;
		NavGuids.Reset();
		for( FActorIterator It; It; ++It )
		{
			const ANavigationPoint *Nav = Cast<ANavigationPoint>( *It );
			if( Nav != NULL )
			{
				if( Nav->NavGuid.IsValid() )
				{
					const ANavigationPoint** ExistingHashElement = NavGuids.Find( Nav->NavGuid );
					if( ExistingHashElement == NULL )
					{
						NavGuids.Set( Nav->NavGuid, Nav );
					}
					else
					{
						const ANavigationPoint* ExistingNavWithGUID = *ExistingHashElement;
						check( ExistingNavWithGUID != NULL );
						if( ExistingNavWithGUID != Nav )		// Should never happen
						{
							GWarn->MapCheck_Add( MCTYPE_ERROR, const_cast<ANavigationPoint*>( Nav ), *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_SamePathNodeGUID" ), *Nav->GetName(), *ExistingNavWithGUID->GetName() ) ), TEXT( "SamePathNodeGUID" ) );
						}
					}
				}
				else
				{
					GWarn->MapCheck_Add( MCTYPE_WARNING, const_cast<ANavigationPoint*>( Nav ), *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_PathNodeInvalidGUID" ), *Nav->GetName() ) ), TEXT( "PathNodeInvalidGUID" ) );
				}
			}
		}
	}
	
	if (GEditor && GEditor->bDoReferenceChecks)
	{
		// Check for externally reference actors and add them to the map check
		PackageUsingExternalObjects(GWorld->PersistentLevel, TRUE);
	}

	GWarn->MapCheck_EndBulkAdd();
	GWarn->EndSlowTask();

	if ( bDisplayResultDialog )
	{
		if(bCheckDeprecatedOnly)
		{
			GWarn->MapCheck_ShowConditionally();
		}
		else
		{
			GWarn->MapCheck_Show();
		}
	}
	return TRUE;
}

UBOOL UEditorEngine::Map_Scale(const TCHAR* Str, FOutputDevice& Ar)
{
	FLOAT Factor = 1.f;
	if( Parse(Str,TEXT("FACTOR="),Factor) )
	{
		UBOOL bAdjustLights=0;
		ParseUBOOL( Str, TEXT("ADJUSTLIGHTS="), bAdjustLights );
		UBOOL bScaleSprites=0;
		ParseUBOOL( Str, TEXT("SCALESPRITES="), bScaleSprites );
		UBOOL bScaleLocations=0;
		ParseUBOOL( Str, TEXT("SCALELOCATIONS="), bScaleLocations );
		UBOOL bScaleCollision=0;
		ParseUBOOL( Str, TEXT("SCALECOLLISION="), bScaleCollision );

		const FScopedBusyCursor BusyCursor;

		const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MapScaling")) );
		const FString LocalizeScaling( LocalizeUnrealEd(TEXT("Scaling")) );
		GWarn->BeginSlowTask( *LocalizeScaling, TRUE );

		NoteActorMovement();
		const INT ProgressDenominator = FActorIteratorBase::GetProgressDenominator();

		// Fire CALLBACK_LevelDirtied and CALLBACK_RefreshEditor_LevelBrowser when falling out of scope.
		FScopedLevelDirtied		LevelDirtyCallback;

		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			GWarn->StatusUpdatef( It->GetIndex(), GetSelectedActors()->Num(), *LocalizeScaling );
			Actor->PreEditChange(NULL);
			Actor->Modify();

			LevelDirtyCallback.Request();

			if( Actor->IsABrush() )
			{
				ABrush* Brush = (ABrush*)Actor;

				Brush->Brush->Polys->Element.ModifyAllItems();
				for( INT poly = 0 ; poly < Brush->Brush->Polys->Element.Num() ; poly++ )
				{
					FPoly* Poly = &(Brush->Brush->Polys->Element(poly));

					Poly->TextureU /= Factor;
					Poly->TextureV /= Factor;
					Poly->Base = ((Poly->Base - Brush->PrePivot) * Factor) + Brush->PrePivot;

					for( INT vtx = 0 ; vtx < Poly->Vertices.Num() ; vtx++ )
					{
						Poly->Vertices(vtx) = ((Poly->Vertices(vtx) - Brush->PrePivot) * Factor) + Brush->PrePivot;
					}

					Poly->CalcNormal();
				}

				Brush->Brush->BuildBound();
			}
			else
			{
				Actor->DrawScale *= Factor;
			}

			if( bScaleLocations )
			{
				Actor->Location.X *= Factor;
				Actor->Location.Y *= Factor;
				Actor->Location.Z *= Factor;
			}

			Actor->PostEditChange();
		}
		GWarn->EndSlowTask();
	}
	else
	{
		Ar.Log(NAME_ExecWarning,*LocalizeUnrealEd(TEXT("MissingScaleFactor")));
	}

	return TRUE;
}

UBOOL UEditorEngine::Map_Setbrush(const TCHAR* Str, FOutputDevice& Ar)
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("SetBrushProperties")) );

	WORD PropertiesMask = 0;

	INT CSGOperation = 0;
	if (Parse(Str,TEXT("CSGOPER="),CSGOperation))	PropertiesMask |= MSB_CSGOper;

	WORD BrushColor = 0;
	if (Parse(Str,TEXT("COLOR="),BrushColor))		PropertiesMask |= MSB_BrushColor;

	FName GroupName = NAME_None;
	if (Parse(Str,TEXT("GROUP="),GroupName))		PropertiesMask |= MSB_Group;

	INT SetFlags = 0;
	if (Parse(Str,TEXT("SETFLAGS="),SetFlags))		PropertiesMask |= MSB_PolyFlags;

	INT ClearFlags = 0;
	if (Parse(Str,TEXT("CLEARFLAGS="),ClearFlags))	PropertiesMask |= MSB_PolyFlags;

	mapSetBrush( static_cast<EMapSetBrushFlags>( PropertiesMask ),
				 BrushColor,
				 GroupName,
				 SetFlags,
				 ClearFlags,
				 CSGOperation,
				 0 // Draw type
				 );

	RedrawLevelEditingViewports();
	RebuildAlteredBSP(); // Update the Bsp of any levels containing a modified brush

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {
	/** Implements texmult and texpan*/
	static void ScaleTexCoords(const TCHAR* Str)
	{
		// Ensure each polygon has unique texture vector indices.
		for ( TSelectedSurfaceIterator<> It ; It ; ++It )
		{
			FBspSurf* Surf = *It;
			UModel* Model = It.GetModel();
			const FVector TextureU( Model->Vectors(Surf->vTextureU) );
			const FVector TextureV( Model->Vectors(Surf->vTextureV) );
			Surf->vTextureU = Model->Vectors.AddItem(TextureU);
			Surf->vTextureV = Model->Vectors.AddItem(TextureV);
		}

		FLOAT UU,UV,VU,VV;
		UU=1.0; Parse (Str,TEXT("UU="),UU);
		UV=0.0; Parse (Str,TEXT("UV="),UV);
		VU=0.0; Parse (Str,TEXT("VU="),VU);
		VV=1.0; Parse (Str,TEXT("VV="),VV);

		FOR_EACH_UMODEL;
			GEditor->polyTexScale( Model, UU, UV, VU, VV, Word2 );
		END_FOR_EACH_UMODEL;
	}
} // namespace

UBOOL UEditorEngine::Exec_Poly( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("SELECT")) ) // POLY SELECT [ALL/NONE/INVERSE] FROM [LEVEL/SOLID/GROUP/ITEM/ADJACENT/MATCHING]
	{
		appSprintf( TempStr, TEXT("POLY SELECT %s"), Str );
		if( ParseCommand(&Str,TEXT("NONE")) )
		{
			return Exec( TEXT("SELECT NONE") );
		}
		else if( ParseCommand(&Str,TEXT("ALL")) )
		{
			const FScopedTransaction Transaction( TempStr );
			GetSelectedActors()->Modify();
			SelectNone( FALSE, TRUE );
			FOR_EACH_UMODEL;
				polySelectAll( Model );
			END_FOR_EACH_UMODEL;
			NoteSelectionChange();
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("REVERSE")) )
		{
			const FScopedTransaction Transaction( TempStr );
			FOR_EACH_UMODEL;
				polySelectReverse( Model );
			END_FOR_EACH_UMODEL;
			GCallbackEvent->Send( CALLBACK_SelChange );
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("MATCHING")) )
		{
			const FScopedTransaction Transaction( TempStr );
			if 		(ParseCommand(&Str,TEXT("GROUPS")))
			{
				FOR_EACH_UMODEL;
					polySelectMatchingGroups( Model );
				END_FOR_EACH_UMODEL;
			}
			else if (ParseCommand(&Str,TEXT("ITEMS")))
			{
				FOR_EACH_UMODEL;
					polySelectMatchingItems( Model );
				END_FOR_EACH_UMODEL;
			}
			else if (ParseCommand(&Str,TEXT("BRUSH")))
			{
				FOR_EACH_UMODEL;
					polySelectMatchingBrush( Model );
				END_FOR_EACH_UMODEL;
			}
			else if (ParseCommand(&Str,TEXT("TEXTURE")))
			{
				polySelectMatchingMaterial( FALSE );
			}
			else if (ParseCommand(&Str,TEXT("RESOLUTION")))
			{
				if (ParseCommand(&Str,TEXT("CURRENT")))
				{
					polySelectMatchingResolution(TRUE);
				}
				else
				{
					polySelectMatchingResolution(FALSE);
				}
			}
			GCallbackEvent->Send( CALLBACK_SelChange );
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("ADJACENT")) )
		{
			const FScopedTransaction Transaction( TempStr );
			if 	  (ParseCommand(&Str,TEXT("ALL")))			{FOR_EACH_UMODEL;polySelectAdjacents(  Model  );END_FOR_EACH_UMODEL;}
			else if (ParseCommand(&Str,TEXT("COPLANARS")))	{FOR_EACH_UMODEL;polySelectCoplanars(  Model  );END_FOR_EACH_UMODEL;}
			else if (ParseCommand(&Str,TEXT("WALLS")))		{FOR_EACH_UMODEL;polySelectAdjacentWalls(  Model  );END_FOR_EACH_UMODEL;}
			else if (ParseCommand(&Str,TEXT("FLOORS")))		{FOR_EACH_UMODEL;polySelectAdjacentFloors(  Model  );END_FOR_EACH_UMODEL;}
			else if (ParseCommand(&Str,TEXT("CEILINGS")))	{FOR_EACH_UMODEL;polySelectAdjacentFloors(  Model  );END_FOR_EACH_UMODEL;}
			else if (ParseCommand(&Str,TEXT("SLANTS")))		{FOR_EACH_UMODEL;polySelectAdjacentSlants(  Model  );END_FOR_EACH_UMODEL;}
			GCallbackEvent->Send( CALLBACK_SelChange );
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("MEMORY")) )
		{
			const FScopedTransaction Transaction( TempStr );
			if 		(ParseCommand(&Str,TEXT("SET")))		{FOR_EACH_UMODEL;polyMemorizeSet(  Model  );END_FOR_EACH_UMODEL;}
			else if (ParseCommand(&Str,TEXT("RECALL")))		{FOR_EACH_UMODEL;polyRememberSet(  Model  );END_FOR_EACH_UMODEL;}
			else if (ParseCommand(&Str,TEXT("UNION")))		{FOR_EACH_UMODEL;polyUnionSet(  Model  );END_FOR_EACH_UMODEL;}
			else if (ParseCommand(&Str,TEXT("INTERSECT")))	{FOR_EACH_UMODEL;polyIntersectSet(  Model  );END_FOR_EACH_UMODEL;}
			else if (ParseCommand(&Str,TEXT("XOR")))		{FOR_EACH_UMODEL;polyXorSet(  Model  );END_FOR_EACH_UMODEL;}
			GCallbackEvent->Send( CALLBACK_SelChange );
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("ZONE")) )
		{
			const FScopedTransaction Transaction( TempStr );
			FOR_EACH_UMODEL;
				polySelectZone( Model );
			END_FOR_EACH_UMODEL;
			GCallbackEvent->Send( CALLBACK_SelChange );
			return TRUE;
		}
		RedrawLevelEditingViewports();
	}
	else if( ParseCommand(&Str,TEXT("DEFAULT")) ) // POLY DEFAULT <variable>=<value>...
	{
		//CurrentMaterial=NULL;
		//ParseObject<UMaterial>(Str,TEXT("TEXTURE="),CurrentMaterial,ANY_PACKAGE);
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("SETMATERIAL")) )
	{
		UBOOL bModelDirtied = FALSE;
		{
			const FScopedTransaction Transaction( TEXT("Poly SetMaterial") );
			FOR_EACH_UMODEL;
				Model->ModifySelectedSurfs( 1 );
			END_FOR_EACH_UMODEL;

			UMaterialInterface* SelectedMaterialInstance = GetSelectedObjects()->GetTop<UMaterialInterface>();

			for ( TSelectedSurfaceIterator<> It ; It ; ++It )
			{
				UModel* Model = It.GetModel();
				const INT SurfaceIndex = It.GetSurfaceIndex();

				Model->Surfs(SurfaceIndex).Material = SelectedMaterialInstance;
				polyUpdateMaster( Model, SurfaceIndex, 0 );
				Model->MarkPackageDirty();

				bModelDirtied = TRUE;
			}
		}
		RedrawLevelEditingViewports();
		if ( bModelDirtied )
		{
			GCallbackEvent->Send( CALLBACK_LevelDirtied );
		}
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("SET")) ) // POLY SET <variable>=<value>...
	{
		{
			const FScopedTransaction Transaction( TEXT("Poly Set") );
			FOR_EACH_UMODEL;
				Model->ModifySelectedSurfs( 1 );
			END_FOR_EACH_UMODEL;
			DWORD Ptr;
			if( !Parse(Str,TEXT("TEXTURE="),Ptr) )
			{
				Ptr = 0;
			}

			UMaterialInterface*	Material = (UMaterialInterface*)Ptr;
			if( Material )
			{
				for ( TSelectedSurfaceIterator<> It ; It ; ++It )
				{
					const INT SurfaceIndex = It.GetSurfaceIndex();
					It.GetModel()->Surfs(SurfaceIndex).Material = Material;
					polyUpdateMaster( It.GetModel(), SurfaceIndex, 0 );
				}
			}

			INT SetBits = 0;
			INT ClearBits = 0;

			Parse(Str,TEXT("SETFLAGS="),SetBits);
			Parse(Str,TEXT("CLEARFLAGS="),ClearBits);

			// Update selected polys' flags.
			if ( SetBits != 0 || ClearBits != 0 )
			{
				FOR_EACH_UMODEL;
					polySetAndClearPolyFlags( Model,SetBits,ClearBits,1,1 );
				END_FOR_EACH_UMODEL;
			}
		}
		RedrawLevelEditingViewports();
		GCallbackEvent->Send( CALLBACK_LevelDirtied );
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("TEXSCALE")) ) // POLY TEXSCALE [U=..] [V=..] [UV=..] [VU=..]
	{
		{
			const FScopedTransaction Transaction( TEXT("Poly Texscale") );

			FOR_EACH_UMODEL;
				Model->ModifySelectedSurfs( 1 );
			END_FOR_EACH_UMODEL;

			Word2 = 1; // Scale absolute
			if( ParseCommand(&Str,TEXT("RELATIVE")) )
			{
				Word2=0;
			}
			ScaleTexCoords( Str );
		}
		RedrawLevelEditingViewports();
		GCallbackEvent->Send( CALLBACK_LevelDirtied );
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("TEXMULT")) ) // POLY TEXMULT [U=..] [V=..]
	{
		{
			const FScopedTransaction Transaction( TEXT("Poly Texmult") );
			FOR_EACH_UMODEL;
				Model->ModifySelectedSurfs( 1 );
			END_FOR_EACH_UMODEL;
			Word2 = 0; // Scale relative;
			ScaleTexCoords( Str );
		}
		RedrawLevelEditingViewports();
		GCallbackEvent->Send( CALLBACK_LevelDirtied );
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("TEXPAN")) ) // POLY TEXPAN [RESET] [U=..] [V=..]
	{
		{
			const FScopedTransaction Transaction( TEXT("Poly Texpan") );
			FOR_EACH_UMODEL;
				Model->ModifySelectedSurfs( 1 );
			END_FOR_EACH_UMODEL;

			// Ensure each polygon has a unique base point index.
			for ( TSelectedSurfaceIterator<> It ; It ; ++It )
			{
				FBspSurf* Surf = *It;
				UModel* Model = It.GetModel();
				const FVector Base( Model->Points(Surf->pBase) );
				Surf->pBase = Model->Points.AddItem(Base);
			}

			if( ParseCommand (&Str,TEXT("RESET")) )
			{
				FOR_EACH_UMODEL;
					polyTexPan( Model, 0, 0, 1 );
				END_FOR_EACH_UMODEL;
			}

			INT PanU = 0; Parse (Str,TEXT("U="),PanU);
			INT PanV = 0; Parse (Str,TEXT("V="),PanV);
			FOR_EACH_UMODEL;
				polyTexPan( Model, PanU, PanV, 0 );
			END_FOR_EACH_UMODEL;
		}

		RedrawLevelEditingViewports();
		GCallbackEvent->Send( CALLBACK_LevelDirtied );
		return TRUE;
	}

	return FALSE;
}

UBOOL UEditorEngine::Exec_Obj( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("EXPORT")) )//oldver
	{
		FName Package=NAME_None;
		UClass* Type;
		UObject* Res;
		Parse( Str, TEXT("PACKAGE="), Package );
		if
		(	ParseObject<UClass>( Str, TEXT("TYPE="), Type, ANY_PACKAGE )
		&&	Parse( Str, TEXT("FILE="), TempFname, 256 )
		&&	ParseObject( Str, TEXT("NAME="), Type, Res, ANY_PACKAGE ) )
		{
			for( FObjectIterator It; It; ++It )
				It->ClearFlags( RF_TagImp | RF_TagExp );
			UExporter* Exporter = UExporter::FindExporter( Res, *FFilename(TempFname).GetExtension() );
			if( Exporter )
			{
				Exporter->ParseParms( Str );
				UExporter::ExportToFile( Res, Exporter, TempFname, 0 );
			}
		}
		else
		{
			Ar.Log( NAME_ExecWarning, TEXT("Missing file, name, or type") );
		}
		return TRUE;
	}
	else if( ParseCommand( &Str, TEXT( "SavePackage" ) ) )
	{
		UPackage* Pkg;
		UBOOL bSilent = FALSE;
		UBOOL bWasSuccessful = TRUE;

		if( Parse( Str, TEXT( "FILE=" ), TempFname, 256 ) && ParseObject<UPackage>( Str, TEXT( "Package=" ), Pkg, NULL ) )
		{
			//Check if this is a package we should warn about.
			if( !GIsUCC												&&	// Don't prompt about saving startup packages when running UCC
				!GIsCooking											&&	// ......or when cooking
				FString(TempFname).EndsWith(TEXT(".upk"))		&& 		// Maps, even startup maps, are ok
				IsStartupPackage( Pkg->GetName(), GEngineIni ) &&	// Check if the package is an engine startup package.
				(!StartupPackageToWarnState.Find(Pkg) || (*(StartupPackageToWarnState.Find(Pkg)) == FALSE)) ) // Check if the user has previously saved over this package this session.
			{

				// Display the warning window.
				FString Title = *FString::Printf(LocalizeSecure(LocalizeUnrealEd("Prompt_AboutToEditStartupPackage_Title"), *(Pkg->GetName())));
				FString Message = *FString::Printf(LocalizeSecure(LocalizeUnrealEd("Prompt_AboutToEditStartupPackage"), *(Pkg->GetName())));
				WxSuppressableWarningDialog SuppressDialog(Message, Title, "bSuppressSaveStartUpPackagesWarning", TRUE);
				if( SuppressDialog.ShowModal() == wxID_CANCEL )
				{
					// The user did not want to save over the package.
					StartupPackageToWarnState.Set( Pkg, FALSE );

					GWarn->Logf( NAME_Warning, LocalizeSecure(LocalizeError(TEXT("CannotSaveStartupPackage"),TEXT("Core")), TempFname) );
					return FALSE;
				}
				else
				{
					// The user is okay with saving over the package.
					StartupPackageToWarnState.Set( Pkg, TRUE );
				}
			}

			// allow aborting this package save
			if( !GCallbackQuery->Query( CALLBACK_AllowPackageSave, Pkg ) )
			{
				return FALSE;
			}

			const FScopedBusyCursor BusyCursor;

			ParseUBOOL( Str, TEXT( "SILENT=" ), bSilent );

			// Allow the game specific editor a chance to modify the package before saving
			GEditor->PreparePackageForSaving(Pkg, TempFname, bSilent);

			// Make a list of waves to cook for PC, 360 and PS3
			TArray<USoundNodeWave*> WavesToCook;
			for( TObjectIterator<USoundNodeWave> It; It; ++It )
			{
				USoundNodeWave* Wave = *It;
				if( Wave->IsIn( Pkg ) )
				{
					WavesToCook.AddItem( Wave );
				}
			}

			// Cook the waves.
			if( WavesToCook.Num() > 0 && !bSilent )
			{
				GWarn->BeginSlowTask( *LocalizeUnrealEd( TEXT( "CookingSound" ) ), TRUE );
			}

			for( INT WaveIndex = 0 ; WaveIndex < WavesToCook.Num() ; ++WaveIndex )
			{
				USoundNodeWave* Wave = WavesToCook( WaveIndex );
				GWarn->StatusUpdatef( WaveIndex, WavesToCook.Num(), *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "CookingSoundf" ) ), WaveIndex, WavesToCook.Num(), *Wave->GetName() ) ) );
				CookSoundNodeWave( Wave );
			}

			if( WavesToCook.Num() && !bSilent )
			{
				GWarn->EndSlowTask();
			}

			// make sure any resources that need preparing for mobile devices are up to date
			::PreparePackageForMobile(Pkg, bSilent);

			// Save the package.
			if( !bSilent )
			{
				GWarn->BeginSlowTask( *LocalizeUnrealEd( TEXT( "SavingPackage" ) ), TRUE );
				GWarn->StatusUpdatef( 1, 1, *LocalizeUnrealEd( TEXT( "SavingPackageE" ) ) );
			}

			bWasSuccessful = SavePackage( Pkg, NULL, RF_Standalone, TempFname, GWarn );
			if( !bSilent )
			{
				GWarn->EndSlowTask();
			}
		}
		else
		{
			Ar.Log( NAME_ExecWarning, *LocalizeUnrealEd( TEXT( "MissingFilename" ) ) );
		}

		return bWasSuccessful;
	}
	else if( ParseCommand(&Str,TEXT("Rename")) )
	{
		UObject* Object=NULL;
		UObject* OldPackage=NULL, *OldGroup=NULL;
		FString NewName, NewGroup, NewPackage;
		ParseObject<UObject>( Str, TEXT("OLDPACKAGE="), OldPackage, NULL );
		ParseObject<UObject>( Str, TEXT("OLDGROUP="), OldGroup, OldPackage );
		Cast<UPackage>(OldPackage)->SetDirtyFlag(TRUE);
		if( OldGroup )
		{
			OldPackage = OldGroup;
		}
		ParseObject<UObject>( Str, TEXT("OLDNAME="), Object, OldPackage );
		Parse( Str, TEXT("NEWPACKAGE="), NewPackage );
		UPackage* Pkg = CreatePackage(NULL,*NewPackage);
		Pkg->SetDirtyFlag(TRUE);
		if( Parse(Str,TEXT("NEWGROUP="),NewGroup) && appStricmp(*NewGroup,TEXT("None"))!= 0)
		{
			Pkg = CreatePackage( Pkg, *NewGroup );
		}
		Parse( Str, TEXT("NEWNAME="), NewName );
		if( Object )
		{
			Object->Rename( *NewName, Pkg );
			Object->SetFlags(RF_Public|RF_Standalone);
		}

		// Refresh
		const DWORD UpdateMask = CBR_UpdatePackageList|CBR_UpdateAssetList;
		GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, UpdateMask, Object));

		return TRUE;
	}

	return FALSE;

}

UBOOL UEditorEngine::Exec_Class( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("SPEW")) )
	{
		const FScopedBusyCursor BusyCursor;

		GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("ExportingScripts")), FALSE);

		UObject* Package = NULL;
		ParseObject( Str, TEXT("PACKAGE="), Package, ANY_PACKAGE );

		for( TObjectIterator<UClass> It; It; ++It )
		{
			if( It->ScriptText )
			{
				// Check package
				if( Package )
				{
					UObject* Outer = It->GetOuter();
					while( Outer && Outer->GetOuter() )
						Outer = Outer->GetOuter();
					if( Outer != Package )
						continue;
				}

				// Create directory.
				FString Directory		= appGameDir() + TEXT("ExportedScript\\") + *It->GetOuter()->GetName() + TEXT("\\Classes\\");
				GFileManager->MakeDirectory( *Directory, 1 );

				// Save file.
				FString Filename		= Directory + It->GetName() + TEXT(".uc");
				debugf( NAME_Log, TEXT("Spewing: %s"), *Filename );
				UExporter::ExportToFile( *It, NULL, *Filename, 0 );
			}
		}

		GWarn->EndSlowTask();
		return TRUE;
	}
	return FALSE;
}

// A pointer to the named actor or NULL if not found.
AActor* UEditorEngine::SelectNamedActor(const TCHAR* TargetActorName)
{
	AActor* Actor = FindObject<AActor>( ANY_PACKAGE, TargetActorName, FALSE );
	if( Actor && !Actor->IsA(AWorldInfo::StaticClass()) )
	{
		SelectActor( Actor, TRUE, NULL, TRUE );
		return Actor;
	}
	return NULL;
}

/** 
 * Updates the selected so that its view fits the passed in BoundingBox.
 * 
 * @param Viewport			Viewport to update.
 * @param bZoomOutOrtho		Whether or not to adjust zoom on ortho viewports, if false, the viewport just centers on the bounding box.
 * @param BoundingBox		The bounding box to fit into view.
 */
static void PrivateAlignViewport(FEditorLevelViewportClient* Viewport,
								 const UBOOL bZoomOutOrtho,
								 const FBox& BoundingBox)
{
	const FVector Position = BoundingBox.GetCenter();
	FLOAT Radius = BoundingBox.GetExtent().Size();

	if(Viewport->bAllowAlignViewport)
	{
		if(!Viewport->IsOrtho())
		{
		   /** 
		    * We need to make sure we are fitting the sphere into the viewport completely, so if the height of the viewport is less
		    * than the width of the viewport, we scale the radius by the aspect ratio in order to compensate for the fact that we have
		    * less visible vertically than horizontally.
		    */
		    if(Viewport->AspectRatio > 1.0f)
		    {
			    Radius *= Viewport->AspectRatio;
		    }

			/** 
			 * Now that we have a adjusted radius, we are taking half of the viewport's FOV,
			 * converting it to radians, and then figuring out the camera's distance from the center
			 * of the bounding sphere using some simple trig.  Once we have the distance, we back up
			 * along the camera's forward vector from the center of the sphere, and set our new view location.
			 */

			const FLOAT HalfFOVRadians = (Viewport->ViewFOV / 2.0f) * PI / 180.0f;
			const FLOAT DistanceFromSphere = Radius / appTan( HalfFOVRadians );
			FVector CameraOffsetVector = Viewport->ViewRotation.Vector() * -DistanceFromSphere;

			Viewport->ViewLocation = Position + CameraOffsetVector;
		}
		else
		{
			// For ortho viewports just set the camera position to the center of the bounding volume.
			Viewport->ViewLocation = Position;

			if(bZoomOutOrtho && !(Viewport->Input->IsPressed(KEY_LeftControl) || Viewport->Input->IsPressed(KEY_RightControl)))
			{
				/** 			
				* We also need to zoom out till the entire volume is in view.  The following block of code first finds the minimum dimension
				* size of the viewport.  It then calculates backwards from what the view size should be (The radius of the bounding volume),
				* to find the new OrthoZoom value for the viewport.  The 15.0f is a fudge factor that is used by the Editor.
				*/
				FViewport* ViewportClient = Viewport->Viewport;
				FLOAT OrthoZoom;
				UINT MinAxisSize = (Viewport->AspectRatio > 1.0f) ? ViewportClient->GetSizeY() : ViewportClient->GetSizeX();
				FLOAT Zoom = Radius / (MinAxisSize / 2);

				OrthoZoom = Zoom * (ViewportClient->GetSizeX() * 15.0f);
				OrthoZoom = Clamp<FLOAT>( OrthoZoom, MIN_ORTHOZOOM, MAX_ORTHOZOOM );
				Viewport->OrthoZoom = OrthoZoom;
			}
		}
	}

	// Tell the viewport to redraw itself.
	Viewport->Invalidate();
}

/** 
 * Handy util to tell us if Obj is 'within' a ULevel.
 * 
 * @return Returns whether or not an object is 'within' a ULevel.
 */
static UBOOL IsInALevel(UObject* Obj)
{
	UObject* Outer = Obj->GetOuter();

	// Keep looping while we walk up Outer chain.
	while(Outer)
	{
		if(Outer->IsA(ULevel::StaticClass()))
		{
			return TRUE;
		}

		Outer = Outer->GetOuter();
	}

	return FALSE;
}

/**
* Moves all viewport cameras to the target actor.
* @param	Actor					Target actor.
* @param	bActiveViewportOnly		If TRUE, move/reorient only the active viewport.
*/
void UEditorEngine::MoveViewportCamerasToActor(AActor& Actor,  UBOOL bActiveViewportOnly)
{
	// Pack the provided actor into a array and call the more robust version of this function.
	TArray<AActor*> Actors;

	Actors.AddItem( &Actor );

	MoveViewportCamerasToActor( Actors, bActiveViewportOnly );
}

/**
 * Moves all viewport cameras to the target actor.
 * @param	Actors					Target actors.
 * @param	bActiveViewportOnly		If TRUE, move/reorient only the active viewport.
 */
void UEditorEngine::MoveViewportCamerasToActor(const TArray<AActor*> &Actors, UBOOL bActiveViewportOnly)
{
	if( Actors.Num() == 0 )
	{
		return;
	}
	
	TArray<AActor*> InvisLevelActors;

	// Create a bounding volume of all of the selected actors.
	FBox BoundingBox( 0 );
	INT NumActiveActors = 0;
	for( INT ActorIdx = 0; ActorIdx < Actors.Num(); ActorIdx++ )
	{
		AActor* Actor = Actors(ActorIdx);

		if( Actor )
		{

			// Don't allow moving the viewport cameras to actors in invisible levels
			if ( !FLevelUtils::IsLevelVisible( Actor->GetLevel() ) )
			{
				InvisLevelActors.AddItem( Actor );
				continue;
			}

			/**
			 * We need to generate an accurate bounding box.  In order to do this, we first see if the actor is a brush,
			 * if it is, we generate its bounding box based on all of its components.  Otherwise, if the actor is not a brush,
			 * we generate the bounding box only from components that are marked AlwaysLoadOnClient and AlwaysLoadOnServer,
			 * this ensures that the components we use for the bounding box have proper bounds associated with them.
			 */
			const UBOOL bActorIsBrush = Actor->IsBrush();
			const UBOOL bActorIsEmitter = (Cast<AEmitter>(Actor) != NULL);

			if( bActorIsBrush )
			{
				for(UINT ComponentIndex = 0;ComponentIndex < (UINT)Actor->Components.Num();ComponentIndex++)
				{
					UPrimitiveComponent*	primComp = Cast<UPrimitiveComponent>(Actor->Components(ComponentIndex));

					// Only use collidable components to find collision bounding box.
					if( primComp && primComp->IsAttached() )
					{
						BoundingBox += primComp->Bounds.GetBox();
					}
				}
			}
			else if (bActorIsEmitter && bCustomCameraAlignEmitter)
			{
				const FVector DefaultExtent(CustomCameraAlignEmitterDistance,CustomCameraAlignEmitterDistance,CustomCameraAlignEmitterDistance);
				const FBox DefaultSizeBox(Actor->Location - DefaultExtent, Actor->Location + DefaultExtent);
				BoundingBox += DefaultSizeBox;
			}
			else
			{
				// Create a default box so all actors have atleast some contribution to the final volume.
				const FVector DefaultExtent(64,64,64);
				const FBox DefaultSizeBox(Actor->Location - DefaultExtent, Actor->Location + DefaultExtent);
				

				BoundingBox += DefaultSizeBox;

				// Loop through all components and add their contribution to the bounding volume.
				for(UINT ComponentIndex = 0;ComponentIndex < (UINT)Actor->Components.Num();ComponentIndex++)
				{
					UPrimitiveComponent*	primComp = Cast<UPrimitiveComponent>(Actor->Components(ComponentIndex));

					if( primComp && primComp->IsAttached() && primComp->AlwaysLoadOnClient && primComp->AlwaysLoadOnServer )
					{
						BoundingBox += primComp->Bounds.GetBox();
					}
				}
			}

			NumActiveActors++;
		}
	}

	// Make sure we had atleast one non-null actor in the array passed in.
	if( NumActiveActors > 0 )
	{
		if ( bActiveViewportOnly )
		{
			if ( GCurrentLevelEditingViewportClient )
			{
				PrivateAlignViewport( GCurrentLevelEditingViewportClient, TRUE, BoundingBox );
			}
		}
		else
		{
			// Update all unlocked viewports.
			for( INT i = 0; i < ViewportClients.Num(); i++ )
			{
				FEditorLevelViewportClient* ViewportClient = ViewportClients(i);
				if ( !ViewportClient->bViewportLocked )
				{
					PrivateAlignViewport( ViewportClient, TRUE, BoundingBox );
				}
			}
		}
	}

	// Warn the user with a suppressable dialog if they attempted to zoom to actors that are in an invisible level
	if ( InvisLevelActors.Num() > 0 )
	{
		FString InvisLevelActorString;
		for ( TArray<AActor*>::TConstIterator InvisLevelActorIter( InvisLevelActors ); InvisLevelActorIter; ++InvisLevelActorIter )
		{
			const AActor* CurActor = *InvisLevelActorIter;
			InvisLevelActorString += FString::Printf( TEXT("%s\n"), *CurActor->GetName() );
		}
		FString WarningMessage = FString::Printf( LocalizeSecure( LocalizeUnrealEd("MoveCameraToInvisLevelActor_Message"), *InvisLevelActorString ) );
		WxSuppressableWarningDialog InvisLevelActorWarning( WarningMessage, LocalizeUnrealEd("MoveCameraToInvisLevelActor_Title"), TEXT("MoveViewportCamerasToActorsInInvisLevel") );
		InvisLevelActorWarning.ShowModal();
	}
}

/** 
 * Moves an actor to the floor.  Optionally will align with the trace normal.
 * @param InActor			Actor to move to the floor.
 * @param InAlign			Whether or not to rotate the actor to align with the trace normal.
 * @return					Whether or not the actor was moved.
 */
UBOOL UEditorEngine::MoveActorToFloor( AActor* InActor, UBOOL InAlign, UBOOL UsePivot )
{
	check( InActor );

	FVector BestLoc = FVector(0,0,-HALF_WORLD_MAX);
	FRotator SaveRot = FRotator(0,0,0);

	FVector Direction = FVector(0,0,-1);

	FVector	StartLocation = InActor->Location,
		LocationOffset = FVector(0,0,0),
		Extent = FVector(1,1,1); // collide with BlockAllButWeapons BVs, etc

	if( UsePivot )
	{
		Extent = FVector(0,0,0);
	}
	else if(InActor->CollisionComponent && InActor->CollisionComponent->IsValidComponent())
	{
		check(InActor->CollisionComponent->IsAttached());
		StartLocation = InActor->CollisionComponent->Bounds.Origin;
		Extent = InActor->CollisionComponent->Bounds.BoxExtent;
	}

	LocationOffset = StartLocation - InActor->Location;

	// The following bit of code checks to see if the actor has a chance of hitting a terrain component.
	// if it does, we need to rebuild collision for that component in-case the user has changed the terrain but has not yet saved the map.
	// @todo Add a dirty collision flag so we do not need to do a collision test/rebuild of a terrain component.
	{
		FCheckResult Hit(1.f);

		if( !GWorld->SingleLineCheck( Hit, InActor, StartLocation + Direction*WORLD_MAX, StartLocation, TRACE_Terrain ) )
		{

			const UBOOL bIsTerrainComponent = Hit.Component->IsA(UTerrainComponent::StaticClass());

			if(bIsTerrainComponent == TRUE)
			{
				UTerrainComponent* Component = Cast<UTerrainComponent>(Hit.Component);

				// Only rebuild if the collision data is dirty and needs to be rebuilt.
				if(Component->IsCollisionDataDirty())
				{
					Component->BuildCollisionData();
				}
			}
		}
	}



	// Do the actual actor->world check.  We try to collide against the world, straight down from our current position.
	// If we hit anything, we will move the actor to a position that lets it rest on the floor.
	FCheckResult Hit(1.f);
	if( !GWorld->SingleLineCheck( Hit, InActor, StartLocation + Direction*WORLD_MAX, StartLocation, TRACE_World, Extent ) )
	{
		const FVector NewLocation = Hit.Location - LocationOffset;

		GWorld->FarMoveActor( InActor, NewLocation, FALSE, FALSE, TRUE );

		if( InAlign )
		{
			//@todo: This doesn't take into account that rotating the actor changes LocationOffset.
			FRotator NewRotation( Hit.Normal.Rotation() );
			NewRotation.Pitch -= 16384;
			if( InActor->IsBrush() )
			{
				FBSPOps::RotateBrushVerts( (ABrush*)InActor, NewRotation, FALSE );
			}
			else
			{
				InActor->Rotation = NewRotation;
			}
		}

		InActor->PostEditMove( TRUE );
		return TRUE;
	}

	return FALSE;
}

/**
 * Moves an actor in front of a camera specified by the camera's origin and direction.
 * The distance the actor is in front of the camera depends on the actors bounding cylinder
 * or a default value if no bounding cylinder exists.
 * 
 * @param InActor			The actor to move
 * @param InCameraOrigin	The location of the camera in world space
 * @param InCameraDirection	The normalized direction vector of the camera
 */
void UEditorEngine::MoveActorInFrontOfCamera( AActor& InActor, const FVector& InCameraOrigin, const FVector& InCameraDirection )
{
	// Get the  radius of the actors bounding cylinder.  Height is not needed.
	FLOAT CylRadius, CylHeight;
	InActor.GetBoundingCylinder(CylRadius, CylHeight);

	// a default cylinder radius if no bounding cylinder exists.  
	const FLOAT	DefaultCylinderRadius = 50.0f;

	if( CylRadius == 0.0f )
	{
		// If the actor does not have a bounding cylinder, use a default value.
		CylRadius = DefaultCylinderRadius;
	}

	// The new location the cameras origin offset by the actors bounding cylinder radius down the direction of the cameras view. 
	FVector NewLocation = InCameraOrigin + InCameraDirection * CylRadius;

	if( InActor.bEdShouldSnap )
	{
		// Snap the new location if this actor wants to be snapped
		Constraints.Snap( NewLocation, FVector( 0, 0, 0 ) );
	}

	// Move the actor to its new location.  Not checking for collisions
	GWorld->FarMoveActor( &InActor, NewLocation, FALSE, TRUE, FALSE );

	if( InActor.IsSelected() )
	{
		// If the actor was selected, reselect it so the widget is set in the correct location
		SelectNone( FALSE, TRUE );
		SelectActor( &InActor, TRUE, NULL, TRUE );
	}

	InActor.InvalidateLightingCache();
	InActor.PostEditMove( TRUE );
}

/**
 * Snaps the view of the camera to that of the provided actor.
 *
 * @param	Actor	The actor the camera is going to be snapped to.
 */
void UEditorEngine::SnapViewToActor(const AActor &Actor)
{
	for(INT ViewportIndex = 0; ViewportIndex < ViewportClients.Num(); ++ViewportIndex)
	{
		FEditorLevelViewportClient* ViewportClient = ViewportClients(ViewportIndex);

		if (ViewportClient->IsPerspective() && !ViewportClient->bViewportLocked)
		{
			ViewportClient->ViewLocation = Actor.Location;
			ViewportClient->ViewRotation = Actor.Rotation;
			ViewportClient->Invalidate();
		}
	}
}

UBOOL UEditorEngine::Exec_Camera( const TCHAR* Str, FOutputDevice& Ar )
{
	const UBOOL bAlign = ParseCommand( &Str,TEXT("ALIGN") );
	const UBOOL bSnap = !bAlign && ParseCommand( &Str, TEXT("SNAP") );

	if ( !bAlign && !bSnap )
	{
		return FALSE;
	}

	AActor* TargetSelectedActor = NULL;

	if( bAlign )
	{
		USelection* SelectedActors = GetSelectedActors();
		const INT iNumSelectedActors = SelectedActors->Num();
		INT LastCameraAlignActorIdx = -1;
		if (LastCameraAlignTarget)
		{
			for (INT ActorIdx = 0; ActorIdx < iNumSelectedActors; ActorIdx++)
			{
				if (LastCameraAlignTarget == (*SelectedActors)(ActorIdx))
				{
					LastCameraAlignActorIdx = ActorIdx;
					break;
				}
			}
		}

		// Try to select the named actor if specified.
		if( Parse( Str, TEXT("NAME="), TempStr, NAME_SIZE ) )
		{
			TargetSelectedActor = SelectNamedActor( TempStr );
			if ( TargetSelectedActor ) 
			{
				NoteSelectionChange();
			}
		}
		else if( ParseCommand( &Str,TEXT("NEXT") ) )
		{
			if ( iNumSelectedActors )
			{
				TargetSelectedActor = Cast<AActor>((*SelectedActors)((LastCameraAlignActorIdx + 1) % iNumSelectedActors));
			}
		}
		else if( ParseCommand( &Str,TEXT("PREVIOUS") ) )
		{
			if ( iNumSelectedActors )
			{
				if (1 > LastCameraAlignActorIdx)
				{
					LastCameraAlignActorIdx = iNumSelectedActors;
				}
				TargetSelectedActor = Cast<AActor>((*SelectedActors)((LastCameraAlignActorIdx - 1) % iNumSelectedActors));
			}
		}

		// Position/orient viewports to look at the selected actor.
		const UBOOL bActiveViewportOnly = ParseCommand( &Str,TEXT("ACTIVEVIEWPORTONLY") );

		// If they specifed a specific Actor to align to, then align to that actor only.
		// Otherwise, build a list of all selected actors and fit the camera to them.
		// If there are no actors selected, give an error message and return false.
		if ( TargetSelectedActor )
		{
			MoveViewportCamerasToActor( *TargetSelectedActor, bActiveViewportOnly );
			LastCameraAlignTarget = TargetSelectedActor;
			Ar.Log( TEXT("Aligned camera to the specified actor.") );
		}
		else 
		{
			TArray<AActor*> Actors;
			for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );
				Actors.AddItem( Actor );
			}

			if( Actors.Num() )
			{
				MoveViewportCamerasToActor( Actors, bActiveViewportOnly );
				if (1 == Actors.Num())
				{
					LastCameraAlignTarget = Actors(0);
				}
				Ar.Log( TEXT("Aligned camera to fit all selected actors.") );
				return TRUE;
			}
			else
			{
				Ar.Log( TEXT("Can't find target actor.") );
				return FALSE;
			}
		}
	}
	else if ( bSnap )
	{
		TargetSelectedActor = GEditor->GetSelectedActors()->GetTop<AActor>();

		if (TargetSelectedActor)
		{
			// Set perspective viewport camera parameters to that of the selected camera.
			SnapViewToActor(*TargetSelectedActor);
			Ar.Log( TEXT("Snapped camera to the first selected actor.") );
		}
	}

	return TRUE;
}

UBOOL UEditorEngine::Exec_Transaction(const TCHAR* Str, FOutputDevice& Ar)
{
	// Was an undo requested?
	const UBOOL bShouldUndo = ParseCommand(&Str,TEXT("UNDO"));

	// Was a redo requested?
	const UBOOL bShouldRedo = ParseCommand(&Str,TEXT("REDO"));

	// If something was requested . . .
	if( bShouldUndo || bShouldRedo )
	{
		//Get the list of all selected actors before the undo/redo is performed
		TArray<AActor*> OldSelectedActors;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = CastChecked<AActor>( *It );
			OldSelectedActors.AddItem( Actor);
		}

		// Perform the operation.
		UBOOL bOpWasSuccessful = FALSE;
		if ( bShouldUndo )
		{
			bOpWasSuccessful = Trans->Undo( );
		}
		else if ( bShouldRedo )
		{
			bOpWasSuccessful = Trans->Redo( );
		}

		//Make sure that the proper objects display as selected

		//Get the list of all selected actors after the operation
		TArray<AActor*> SelectedActors;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = CastChecked<AActor>( *It );
			SelectedActors.AddItem( Actor);
		}

		//Deselect all of the actors that were selected prior to the operation
		for ( INT OldSelectedActorIndex = OldSelectedActors.Num()-1 ; OldSelectedActorIndex >= 0 ; --OldSelectedActorIndex )
		{
			AActor* Actor = OldSelectedActors(OldSelectedActorIndex);

			//To stop us from unselecting and then reselecting again (causing two force update components, we will remove (from both lists) any object that was selected and should continue to be selected
			INT FoundIndex;
			if (SelectedActors.FindItem(Actor, FoundIndex))
			{
				OldSelectedActors.Remove(OldSelectedActorIndex);
				SelectedActors.Remove(FoundIndex);
			}
			else
			{
				SelectActor( Actor, FALSE, NULL, FALSE );//First FALSE is to deselect, 2nd is to notify
				Actor->ForceUpdateComponents();
			}
		}

		//Select all of the actors in SelectedActors
		for ( INT SelectedActorIndex = 0 ; SelectedActorIndex < SelectedActors.Num() ; ++SelectedActorIndex )
		{
			AActor* Actor = SelectedActors(SelectedActorIndex);
			SelectActor( Actor, TRUE, NULL, FALSE );	//FALSE is to stop notify which is done below if bOpWasSuccessful
			Actor->ForceUpdateComponents();
		}

		//if something changed as a result of undo redo
		if (bOpWasSuccessful)
		{
			GCallbackEvent->Send( CALLBACK_Undo );
			NoteSelectionChange();
		}
	}
	else if( ParseCommand(&Str,TEXT("CANCEL")) )
	{
		CancelTransaction(0);
	}
	else if( ParseCommand(&Str,TEXT("RESET")) )
	{
		FString Reason;
		ParseLine(&Str, Reason);
		ResetTransaction(*Reason);
	}

	return TRUE;
}

UBOOL UEditorEngine::Exec_Particle(const TCHAR* Str, FOutputDevice& Ar)
{
	UBOOL bHandled = FALSE;
	debugf(TEXT("Exec Particle!"));
	if (ParseCommand(&Str,TEXT("RESET")))
	{
		TArray<AEmitter*> EmittersToReset;
		if (ParseCommand(&Str,TEXT("SELECTED")))
		{
			// Reset any selected emitters in the level
			for (FSelectionIterator It(GEditor->GetSelectedActorIterator()) ; It ; ++It)
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow(Actor->IsA(AActor::StaticClass()));

				AEmitter* Emitter = Cast<AEmitter>(Actor);
				if (Emitter)
				{
					Emitter->ResetInLevel();
				}
			}
		}
		else if (ParseCommand(&Str,TEXT("ALL")))
		{
			// Reset ALL emitters in the level
			for (TObjectIterator<AEmitter> It;It;++It)
			{
				AEmitter* Emitter = *It;
				Emitter->ResetInLevel();
			}
		}
	}
	return bHandled;
}

/** 
 *	Gather the status of BakeAndPrune settings for the referenced AnimSets in the given level
 *	
 *	@param	InLevel							The source level
 *	@param	OutAnimSetSkipBakeAndPruneMap	List of anim sets to not touch during the operation
 *											This is filled in during the bake, and used during the prune
 *											If NULL, bake all possible animsets
 *
 *	@return	UBOOL							TRUE if successful, FALSE if not
 */
UBOOL UEditorEngine::GatherBakeAndPruneStatus(ULevel* InLevel, TMap<FString,UBOOL>* OutAnimSetSkipBakeAndPruneMap)
{
	if (OutAnimSetSkipBakeAndPruneMap != NULL)
	{
		for (TObjectIterator<UInterpData> IterpDataIt; IterpDataIt; ++IterpDataIt)
		{
			UInterpData* InterpData = *IterpDataIt;
			if (InterpData->IsIn(InLevel))
			{
				if (GIsCooking)
				{
					if (InterpData->HasAnyFlags(RF_MarkedByCooker))
					{
						continue;
					}
				}
				else
				{
					// Can't crop cooked animations.
					const UPackage* Package = InterpData->GetOutermost();
					if (Package->PackageFlags & PKG_Cooked)
					{
						continue;
					}
				}

				// Gather the animsets from this InterpData
				if (InterpData->bShouldBakeAndPrune == FALSE)
				{
					// None of the anim sets in this data should be bake and pruned.
					for (INT AnimSetIdx = 0; AnimSetIdx < InterpData->BakeAndPruneStatus.Num(); AnimSetIdx++)
					{
						// If the anim set is referenced in *any* interp data that should not bake & prune,
						// then it should not be bake and pruned in any of them...
						FAnimSetBakeAndPruneStatus& Status = InterpData->BakeAndPruneStatus(AnimSetIdx);
						OutAnimSetSkipBakeAndPruneMap->Set(Status.AnimSetName, TRUE);
					}
				}
				else
				{
					for (INT AnimSetIdx = 0; AnimSetIdx < InterpData->BakeAndPruneStatus.Num(); AnimSetIdx++)
					{
						// If the anim set is referenced in *any* interp data that should not bake & prune,
						// then it should not be bake and pruned in any of them...
						FAnimSetBakeAndPruneStatus& Status = InterpData->BakeAndPruneStatus(AnimSetIdx);
						UBOOL* pShouldBakeAndPrune = OutAnimSetSkipBakeAndPruneMap->Find(Status.AnimSetName);
						if (pShouldBakeAndPrune == NULL)
						{
							// First instance of the AnimSet
							OutAnimSetSkipBakeAndPruneMap->Set(Status.AnimSetName, Status.bSkipBakeAndPrune);
						}
						else
						{
							// If found, SkipBakeAndPrune == TRUE trumps any previous setting
							if (*pShouldBakeAndPrune == FALSE)
							{
								OutAnimSetSkipBakeAndPruneMap->Set(Status.AnimSetName, Status.bSkipBakeAndPrune);
							}
						}
					}
				}
			}
		}
	}

	return TRUE;
}

/** 
 *	Clear the BakeAndPrune status arrays in the given level
 *	
 *	@param	InLevel							The source level
 *
 *	@return	UBOOL							TRUE if successful, FALSE if not
 */
UBOOL UEditorEngine::ClearBakeAndPruneStatus(ULevel* InLevel)
{
	for (TObjectIterator<UInterpData> IterpDataIt; IterpDataIt; ++IterpDataIt)
	{
		UInterpData* InterpData = *IterpDataIt;
		if (InterpData->IsIn(InLevel))
		{
			if (GIsCooking)
			{
				if (InterpData->HasAnyFlags(RF_MarkedByCooker))
				{
					continue;
				}
			}
			else
			{
				// Can't crop cooked animations.
				const UPackage* Package = InterpData->GetOutermost();
				if (Package->PackageFlags & PKG_Cooked)
				{
					continue;
				}
			}
			InterpData->BakeAndPruneStatus.Empty();
		}
	}

	return TRUE;
}

/** 
 *	Bake the anim sets in the given level
 *	
 *	@param	InLevel							The source level
 *	@param	InAnimSetSkipBakeAndPruneMap	List of anim sets to not touch during the operation
 *											If NULL, bake all possible animsets
 *
 *	@return	UBOOL							TRUE if successful, FALSE if not
 */
UBOOL UEditorEngine::BakeAnimSetsInLevel(ULevel* InLevel, TMap<FString,UBOOL>* InAnimSetSkipBakeAndPruneMap)
{
	// Anim set and sequences referenced by this level's matinee.
	TArray<UAnimSequence*>		UsedAnimSequences;
	TArray<UAnimSet*>			AnimSets;
	TMap<UAnimSet*,UAnimSet*>	OldToNewMap;

	// Iterate over all interp groups in the current level and gather anim set/ sequence usage.
	for( TObjectIterator<UInterpGroup> It; It; ++It )
	{
		UInterpGroup* InterpGroup = *It;
		// Gather all anim sequences and sets referenced by this interp group.
		if( InterpGroup->IsIn( InLevel ) )
		{
			UInterpData* OuterInterpData = Cast<UInterpData>(InterpGroup->GetOuter());
			if ((OuterInterpData == NULL) || (OuterInterpData->bShouldBakeAndPrune == FALSE))
			{
				continue;
			}

			if (GIsCooking)
			{
				if (OuterInterpData->HasAnyFlags(RF_MarkedByCooker))
				{
					continue;
				}
			}
			else
			{
				// Can't crop cooked animations.
				const UPackage* Package = OuterInterpData->GetOutermost();
				if (Package->PackageFlags & PKG_Cooked)
				{
					continue;
				}
			}
			// Iterate over all tracks to find anim control tracks and their anim sequences.
			for( INT TrackIndex=0; TrackIndex<InterpGroup->InterpTracks.Num(); TrackIndex++ )
			{
				UInterpTrack*				InterpTrack = InterpGroup->InterpTracks(TrackIndex);
				UInterpTrackAnimControl*	AnimControl	= Cast<UInterpTrackAnimControl>(InterpTrack);				
				if( AnimControl )
				{
					// Iterate over all track key/ sequences and find the associated sequence.
					for( INT TrackKeyIndex=0; TrackKeyIndex<AnimControl->AnimSeqs.Num(); TrackKeyIndex++ )
					{
						const FAnimControlTrackKey& TrackKey = AnimControl->AnimSeqs(TrackKeyIndex);
						UAnimSequence* AnimSequence			 = AnimControl->FindAnimSequenceFromName( TrackKey.AnimSeqName );
						if( AnimSequence )
						{
							UBOOL bShouldSkip = FALSE;
							if (InAnimSetSkipBakeAndPruneMap != NULL)
							{
								UBOOL* pShouldSkip = InAnimSetSkipBakeAndPruneMap->Find(AnimSequence->GetAnimSet()->GetPathName());
								if (pShouldSkip != NULL)
								{
									bShouldSkip = *pShouldSkip;
								}
							}

							if (bShouldSkip == FALSE)
							{
								// We've found a soft reference, add it to the list.
								UsedAnimSequences.AddUniqueItem( AnimSequence );
								// Also kee track of sets used.
								AnimSets.AddUniqueItem( AnimSequence->GetAnimSet() );
							}
							else
							{
								warnf(NAME_Log, TEXT("Skipping Baking on %s (%s)"), 
									*(AnimSequence->GetName()),
									*(AnimSequence->GetAnimSet()->GetPathName()));
							}
						}
					}
				}	 
			}
		}
	}

	// Iterate over all referenced anim sets, duplicate them and prune unused sequences.
	for( INT AnimSetIndex=0; AnimSetIndex<AnimSets.Num(); AnimSetIndex++ )
	{
		// Duplicate anim set - this will perform a deep duplication as the anim sequences have the set as their outers.
		UAnimSet*	OldAnimSet		= AnimSets(AnimSetIndex);
		FName		NewAnimSetName	= OldAnimSet->GetFName();
					
		// Prefix name with Baked_ ...
		if( !NewAnimSetName.ToString().StartsWith(TEXT("Baked_")) )
		{
			NewAnimSetName = FName( *(FString("Baked_") + NewAnimSetName.ToString()) );	
		}
		// ... unless it's already prefixed in which case we bump the instance number on the name.
		else
		{
			NewAnimSetName = FName( *NewAnimSetName.GetNameString(), NewAnimSetName.GetNumber() + 1 );

			// Make sure baked anim sets and sequences are not standalone! This is to work around bug in earlier baking code.
			OldAnimSet->ClearFlags( RF_Standalone );
			for( TObjectIterator<UObject> It; It; ++It )
			{
				UObject* Object = *It;
				if( Object->IsIn( OldAnimSet ) )
				{
					Object->ClearFlags( RF_Standalone );
				}
			}
		}

		// Make sure there is not a set with that name already!
		do
		{
			NewAnimSetName = FName(*NewAnimSetName.GetNameString(), NewAnimSetName.GetNumber() + 1);
		} while( FindObject<UAnimSet>( InLevel, *NewAnimSetName.ToString() ) );

		// Duplicate with that name
		UAnimSet*	NewAnimSet		= CastChecked<UAnimSet>(UObject::StaticDuplicateObject( OldAnimSet, OldAnimSet, InLevel, *NewAnimSetName.ToString(), ~RF_Standalone ));
		
		// Make sure anim sets and sequences are not standalone!
		NewAnimSet->ClearFlags( RF_Standalone );
		for( TObjectIterator<UObject> It; It; ++It )
		{
			UObject* Object = *It;
			if( Object->IsIn( NewAnimSet ) )
			{
				Object->ClearFlags( RF_Standalone );
			}
		}

		// Iterate over all sequences and remove the ones not referenced by the InterpGroup. Please note that we iterate
		// over the old anim set but remove from the new one, which is why we need to iterate in reverse order.
		for( INT SequenceIndex=OldAnimSet->Sequences.Num()-1; SequenceIndex>=0; SequenceIndex-- )
		{
			UAnimSequence* AnimSequence = OldAnimSet->Sequences(SequenceIndex);
			if( UsedAnimSequences.FindItemIndex( AnimSequence ) == INDEX_NONE )
			{
 				// The NewAnimSet sequences will be duplicates of the OldAnimSets
 				// So the order will be the same.
 				UAnimSequence* RemoveSequence = NewAnimSet->Sequences(SequenceIndex);
 				if (RemoveSequence != NULL)
 				{
 					NewAnimSet->RemoveAnimSequenceFromAnimSet(RemoveSequence);
 				}
			}
		}

		// Keep track of upgrade path.
		OldToNewMap.Set( OldAnimSet, NewAnimSet );
	}

	// Iterate over all interp groups again and replace references with the baked version.
	for( TObjectIterator<UInterpGroup> It; It; ++It )
	{
		UInterpGroup* InterpGroup = *It;
		// Only iterate over interp groups in the current level.
		if( InterpGroup->IsIn( InLevel ) )
		{
			UInterpData* OuterInterpData = Cast<UInterpData>(InterpGroup->GetOuter());
			if (OuterInterpData && ((OuterInterpData->bShouldBakeAndPrune == FALSE) || (OuterInterpData->HasAnyFlags(RF_MarkedByCooker))))
			{
				continue;
			}
			else if (OuterInterpData == NULL)
			{
				continue;
			}
			// Replace anim sets with new baked version.
			for( INT AnimSetIndex=InterpGroup->GroupAnimSets.Num()-1; AnimSetIndex>=0; AnimSetIndex-- )
			{
				UAnimSet* OldAnimSet = InterpGroup->GroupAnimSets(AnimSetIndex);
				UAnimSet* NewAnimSet = OldToNewMap.FindRef( OldAnimSet );
			
				// Replace old with new.
				if( NewAnimSet )
				{
					InterpGroup->GroupAnimSets(AnimSetIndex) = NewAnimSet;
				}
				// Purge unused.
				else
				{
					UBOOL bWasSkipped = FALSE;
					if ((OldAnimSet != NULL) && (InAnimSetSkipBakeAndPruneMap != NULL))
					{
						UBOOL* pShouldSkip = InAnimSetSkipBakeAndPruneMap->Find(OldAnimSet->GetPathName());
						if (pShouldSkip != NULL)
						{
							bWasSkipped = *pShouldSkip;
						}
					}
					if (bWasSkipped == FALSE)
					{
						InterpGroup->GroupAnimSets.Remove(AnimSetIndex);
					}
				}
			}
		}
	}

	// Iterate over all skeletal mesh components and replace references with the baked version.
	for (TObjectIterator<USkeletalMeshComponent> SkelMeshIt; SkelMeshIt; ++SkelMeshIt)
	{
		USkeletalMeshComponent* SkelMeshComp = *SkelMeshIt;
		// Only iterate over interp groups in the current level.
		if (SkelMeshComp->IsIn(InLevel))
		{
			// Replace anim sets with new baked version.
			for (INT AnimSetIndex = SkelMeshComp->AnimSets.Num() - 1; AnimSetIndex >= 0; AnimSetIndex--)
			{
				UAnimSet* OldAnimSet = SkelMeshComp->AnimSets(AnimSetIndex);
				UAnimSet* NewAnimSet = OldToNewMap.FindRef(OldAnimSet);
			
				// Replace old with new.
				if (NewAnimSet)
				{
					SkelMeshComp->AnimSets(AnimSetIndex) = NewAnimSet;
				}
			}
		}
	}

	return TRUE;
}

/** 
 *	Prune the anim sets in the given level
 *	
 *	@param	InLevel							The source level
 *	@param	InAnimSetSkipBakeAndPruneMap	List of anim sets to not touch during the operation
 *											Filled in during the bake, and used during the prune
 *											If NULL, prine all possible animsets
 *
 *	@return	UBOOL							TRUE if successful, FALSE if not
 */
UBOOL UEditorEngine::PruneAnimSetsInLevel(ULevel* InLevel, TMap<FString,UBOOL>* InAnimSetSkipBakeAndPruneMap)
{
	TMultiMap<UAnimSequence*,FAnimSequenceUsageInfo> SequenceToUsageMap;
	TArray<UAnimSequence*> UsedAnimSequences;

	// Iterate over all interp groups in the current level and gather anim sequence usage.
	for( TObjectIterator<UInterpGroup> It; It; ++It )
	{
		UInterpGroup* InterpGroup = *It;
		// Gather usage stats for all anim sequences referenced by this interp group.
		if( InterpGroup->IsIn( InLevel ) )
		{
			UInterpData* OuterInterpData = Cast<UInterpData>(InterpGroup->GetOuter());
			if ((OuterInterpData == NULL) || (OuterInterpData->bShouldBakeAndPrune == FALSE))
			{
				continue;
			}

			if (GIsCooking)
			{
				if (OuterInterpData->HasAnyFlags(RF_MarkedByCooker))
				{
					continue;
				}
			}
			else
			{
				// Can't crop cooked animations.
				const UPackage* Package = OuterInterpData->GetOutermost();
				if (Package->PackageFlags & PKG_Cooked)
				{
					continue;
				}
			}

			// Iterate over all tracks to find anim control tracks and their anim sequences.
			for( INT TrackIndex=0; TrackIndex<InterpGroup->InterpTracks.Num(); TrackIndex++ )
			{
				UInterpTrack*				InterpTrack = InterpGroup->InterpTracks(TrackIndex);
				UInterpTrackAnimControl*	AnimControl	= Cast<UInterpTrackAnimControl>(InterpTrack);
				if( AnimControl )
				{
					// Iterate over all track key/ sequences and find the associated sequence.
					for( INT TrackKeyIndex=0; TrackKeyIndex<AnimControl->AnimSeqs.Num(); TrackKeyIndex++ )
					{
						const FAnimControlTrackKey& TrackKey = AnimControl->AnimSeqs(TrackKeyIndex);
						UAnimSequence* AnimSequence			 = AnimControl->FindAnimSequenceFromName( TrackKey.AnimSeqName );

						// Don't modify uses of any AnimSequence that is not in this level.
						// You should only use this util if you have already used BAKEANIMSETS
						if( AnimSequence && AnimSequence->IsIn(InLevel) )
						{
							UBOOL bShouldSkip = FALSE;
							if (InAnimSetSkipBakeAndPruneMap != NULL)
							{
								UBOOL* pShouldSkip = InAnimSetSkipBakeAndPruneMap->Find(AnimSequence->GetAnimSet()->GetPathName());
								if (pShouldSkip != NULL)
								{
									bShouldSkip = *pShouldSkip;
								}
							}

							if (bShouldSkip == FALSE)
							{
								FLOAT AnimStartOffset = TrackKey.AnimStartOffset;
								FLOAT AnimEndOffset = TrackKey.AnimEndOffset;
								FLOAT SeqLength = AnimSequence->SequenceLength;

								// If we are looping - take all of the animation.
								if(TrackKey.bLooping)
								{
									AnimStartOffset=0.f;
									AnimEndOffset=0.f;
								}
								else
								{
									// If key is before start of cinematic, we can discard part of the animation before zero
									if(TrackKey.StartTime < 0.f)
									{
										AnimStartOffset = ((0.f - TrackKey.StartTime) * TrackKey.AnimPlayRate) + TrackKey.AnimStartOffset;
										AnimStartOffset = ::Min(AnimStartOffset, SeqLength - TrackKey.AnimEndOffset);
									}

									// Find time that this animation ends.
									UInterpData* IData = CastChecked<UInterpData>(InterpGroup->GetOuter());
									FLOAT AnimEndTime = IData->InterpLength;

									// If there is a key following this one, use its start time as the end of this key
									if(TrackKeyIndex < AnimControl->AnimSeqs.Num()-1)
									{
										const FAnimControlTrackKey& NextTrackKey = AnimControl->AnimSeqs(TrackKeyIndex+1);
										AnimEndTime = NextTrackKey.StartTime;
									}

									FLOAT AnimEndPos = ((AnimEndTime - TrackKey.StartTime) * TrackKey.AnimPlayRate) + TrackKey.AnimStartOffset;
									if(AnimEndPos < SeqLength)
									{
										AnimEndOffset = SeqLength - AnimEndPos;
									}
								}


								// Add usage to map.
								SequenceToUsageMap.Add( AnimSequence, FAnimSequenceUsageInfo( 
																			AnimStartOffset, 
																			AnimEndOffset, 
																			AnimControl, 
																			TrackKeyIndex ) );
								// Keep track of anim sequences separately as TMultiMap doesn't have a nice iterator.
								UsedAnimSequences.AddUniqueItem( AnimSequence );
							}
							else
							{
								warnf(NAME_Log, TEXT("Skipping Pruning on %s (%s)"), 
									*(AnimSequence->GetName()),
									*(AnimSequence->GetAnimSet()->GetPathName()));
							}
						}
					}
				}	 
			}
		}
	}

	// Iterate over all used anim sequences and trim them.
	for( INT AnimSequenceIndex=0; AnimSequenceIndex<UsedAnimSequences.Num(); AnimSequenceIndex++ )
	{
		// Get usage info for anim sequence.
		UAnimSequence* AnimSequence = UsedAnimSequences(AnimSequenceIndex);

		// Can't modify cooked animations.
		const UPackage* Package = AnimSequence->GetOutermost();
 		if ((GIsCooking == FALSE) && (Package->PackageFlags & PKG_Cooked))
 		{
 			continue;
 		}

		TArray<FAnimSequenceUsageInfo> UsageInfos;
		SequenceToUsageMap.MultiFind( AnimSequence, UsageInfos );

		// Only cut from the beginning/ end and not in between. Figure out what to cut.
		FLOAT MinStartOffset	= FLT_MAX;
		FLOAT MinEndOffset		= FLT_MAX;
		for( INT UsageInfoIndex=0; UsageInfoIndex<UsageInfos.Num(); UsageInfoIndex++ )
		{
			const FAnimSequenceUsageInfo& UsageInfo = UsageInfos(UsageInfoIndex);
			MinStartOffset	= Min( MinStartOffset, UsageInfo.StartOffset );
			MinEndOffset	= Min( MinEndOffset, UsageInfo.EndOffset );
		}

		// Update track infos.
		for( INT UsageInfoIndex=0; UsageInfoIndex<UsageInfos.Num(); UsageInfoIndex++ )
		{
			const FAnimSequenceUsageInfo&	UsageInfo	= UsageInfos(UsageInfoIndex);
			FAnimControlTrackKey&			TrackKey	= UsageInfo.AnimControl->AnimSeqs( UsageInfo.TrackKeyIndex );

			// First - we make sure the key is at the start of the matinee, updating AnimStartPosition
			// if looping, do not cut to split pos since it will crop the animation
			if(!TrackKey.bLooping && TrackKey.StartTime < 0.f)
			{
				FLOAT SplitAnimPos = ((0.f - TrackKey.StartTime) * TrackKey.AnimPlayRate) + TrackKey.AnimStartOffset;

				TrackKey.AnimStartOffset = SplitAnimPos;
				TrackKey.StartTime = 0.f;
			}

			// Then update both to take into account cropping of animation
			TrackKey.AnimStartOffset -= MinStartOffset;
			TrackKey.AnimEndOffset = ::Max(0.f, TrackKey.AnimEndOffset - MinEndOffset);
		}

		UBOOL bRequiresRecompression = FALSE;

		// Crop from start.
		if( MinStartOffset > KINDA_SMALL_NUMBER )
		{
			AnimSequence->CropRawAnimData( MinStartOffset, TRUE );
			bRequiresRecompression = TRUE;
		}
		// Crop from end.
		if( MinEndOffset > KINDA_SMALL_NUMBER )
		{
			AnimSequence->CropRawAnimData( AnimSequence->SequenceLength - MinEndOffset, FALSE );
			bRequiresRecompression = TRUE;
		}

		if( bRequiresRecompression )
		{
			debugf(TEXT("%s cropped [%5.2f,%5.2f] to [%5.2f,%5.2f] "), 
				*AnimSequence->GetPathName(), 
				0.f, 
				AnimSequence->SequenceLength, 
				MinStartOffset, 
				AnimSequence->SequenceLength - MinEndOffset );
		}
		else
		{
			debugf(TEXT("%s is used in its entirety - no cropping possible"), *AnimSequence->GetPathName());
		}

		// Recompress sequence.
		if( bRequiresRecompression )
		{
			FAnimationUtils::CompressAnimSequence(AnimSequence, NULL, FALSE, FALSE);
		}
	}

	return TRUE;
}

/**
 *	Clear unreferenced AnimSets from InterpData Groups
 *
 *	@param	InLevel		The source level to clean up
 *
 *	@return	UBOOL		TRUE if successful, FALSE if not
 */
UBOOL UEditorEngine::ClearUnreferenceAnimSetsFromGroups(ULevel* InLevel)
{
	// Iterate over all interp groups in the current level and remove the unreferenced anim sets
	for (TObjectIterator<UInterpGroup> It; It; ++It)
	{
		UInterpGroup* InterpGroup = *It;
		if (InterpGroup->IsIn(InLevel))
		{
			UInterpData* OuterInterpData = Cast<UInterpData>(InterpGroup->GetOuter());
			if (OuterInterpData == NULL)
			{
				continue;
			}

			if (GIsCooking)
			{
				if (OuterInterpData->HasAnyFlags(RF_MarkedByCooker))
				{
					continue;
				}
			}
			else
			{
				// Can't crop cooked animations.
				const UPackage* Package = OuterInterpData->GetOutermost();
				if (Package->PackageFlags & PKG_Cooked)
				{
					continue;
				}
			}

			// 
			for (INT CheckIdx = InterpGroup->GroupAnimSets.Num() - 1; CheckIdx >= 0; CheckIdx--)
			{
				UAnimSet* AnimSet = InterpGroup->GroupAnimSets(CheckIdx);
				if (AnimSet != NULL)
				{
					for (INT BnPIdx = 0; BnPIdx < OuterInterpData->BakeAndPruneStatus.Num(); BnPIdx++)
					{
						FAnimSetBakeAndPruneStatus& BAndPStatus = OuterInterpData->BakeAndPruneStatus(BnPIdx);
						if (AnimSet->GetPathName() == BAndPStatus.AnimSetName)
						{
							if (BAndPStatus.bReferencedButUnused == TRUE)
							{
								debugf(TEXT("Removing animset %s from %s"), *(AnimSet->GetPathName()), *(InterpGroup->GetPathName()));
								InterpGroup->GroupAnimSets.Remove(CheckIdx);
								break;
							}
							else if (BAndPStatus.bSkipCooking == TRUE && GIsCooking)
							{
								debugf(TEXT("Skipping cook for animset %s from %s"), *(AnimSet->GetPathName()), *(InterpGroup->GetPathName()));
								InterpGroup->GroupAnimSets.Remove(CheckIdx);
								break;
							}
						}
					}
				}
			}
		}
	}
	return TRUE;
}

IMPLEMENT_COMPARE_POINTER( USoundNodeWave, UnEdSrv, { return appStricmp(*A->GetPathName(), *B->GetPathName()); } )

/**
 * Executes each line of text in a file sequentially, as if each were a separate command
 *
 * @param InFilename The name of the file to load and execute
 * @param Ar Output device
 */
void UEditorEngine::ExecFile( const TCHAR* InFilename, FOutputDevice& Ar )
{
	FString FileTextContents;
	if( appLoadFileToString( FileTextContents, InFilename ) )
	{
		debugf( TEXT( "Execing file: %s..." ), InFilename );

		const TCHAR* FileString = *FileTextContents;
		FString LineString;
		while( ParseLine( &FileString, LineString ) )
		{
			Exec( *LineString, Ar );
		}
	}
	else
	{
		Ar.Logf( NAME_ExecWarning, LocalizeSecure( LocalizeError("FileNotFound",TEXT("Core")), InFilename ) );
	}
}


/*---------------------------------------------------------------------------------------
	Component replacing for LOD
---------------------------------------------------------------------------------------*/

/**
 * Replaces the components in ActorsToReplace with an primitive component in Replacement
 *
 * @param ActorsToReplace Primitive components in the actors in this array will have their ReplacementPrimitive set to a component in Replacement
 * @param Replacement The first usable component in Replacement will be the ReplacementPrimitive for the actors
 * @param ClassToReplace If this is set, only components will of this class will be used/replaced
 */
void UEditorEngine::AssignReplacementComponentsByActors(TArray<AActor*>& ActorsToReplace, AActor* Replacement, UClass* ClassToReplace)
{
	// the code will use this to find the best possible component, in the priority listed here
	// (ie it will first look for a mesh component, then a particle, and finally a sprite)
	UClass* PossibleReplacementClass[] = 
	{
		UMeshComponent::StaticClass(),
		UParticleSystemComponent::StaticClass(),
		USpriteComponent::StaticClass(),
	};

	// look for a mesh component to replace with
	UPrimitiveComponent* ReplacementComponent = NULL;

	// loop over the clases until a component is found
	for (INT ClassIndex = 0; ClassIndex < ARRAY_COUNT(PossibleReplacementClass) && ReplacementComponent == NULL; ClassIndex++)
	{
		// use ClassToReplace of UMeshComponent if not specified
		UClass* ReplacementComponentClass = ClassToReplace ? ClassToReplace : PossibleReplacementClass[ClassIndex];

		// if we are clearing the replacement, then we don't need to find a component
		if (Replacement)
		{
			for (INT ComponentIndex = 0; ComponentIndex < Replacement->Components.Num(); ComponentIndex++)
			{
				UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Replacement->Components(ComponentIndex));
				if (PrimitiveComponent && PrimitiveComponent->IsA(ReplacementComponentClass))
				{
					ReplacementComponent = PrimitiveComponent;
					break;
				}
			}
		}
	}

	// attempt to set replacement component for all selected actors
	for (INT ActorIndex = 0; ActorIndex < ActorsToReplace.Num(); ActorIndex++)
	{
		AActor* Actor = ActorsToReplace(ActorIndex);
		for (INT ComponentIndex = 0; ComponentIndex < Actor->Components.Num(); ComponentIndex++)
		{
			UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Actor->Components(ComponentIndex));
			// if the primitive component matches the class we are looking for (if specified)
			// then set it's replacement component
			if (PrimitiveComponent && (ClassToReplace == NULL || PrimitiveComponent->IsA(ClassToReplace)))
			{
				// need to reattach the component
				FComponentReattachContext ComponentReattch(PrimitiveComponent);

				// set the replacement
				PrimitiveComponent->ReplacementPrimitive = ReplacementComponent;

				// makr the package as dirty now that we've modified it
				Actor->MarkPackageDirty();
			}
		}
	}
}

/**
 *	Fix up bad animnotifiers that has wrong outers
 *	It uses all loaded animsets
 */
UBOOL FixUpBadAnimNotifiers()
{
	// Iterate over all interp groups in the current level and remove the unreferenced anim sets
	for (TObjectIterator<UAnimSet> It; It; ++It)
	{
		UAnimSet* AnimSet = *It;

		for (INT J=0; J<AnimSet->Sequences.Num(); ++J)
		{
			UAnimSequence * AnimSeq = AnimSet->Sequences(J);
			// iterate over all animnotifiers
			// if any animnotifier outer != current animsequence
			// then add to map
			for (INT I=0; I<AnimSeq->Notifies.Num(); ++I)
			{
				if (AnimSeq->Notifies(I).Notify)
				{
					if ( AnimSeq->Notifies(I).Notify->GetOuter()!=AnimSeq )
					{
						// fix animnotifiers
						debugf(TEXT("Animation[%s] Notifier[%s:%d] is being fixed (Current Outer:%s)"), *AnimSeq->SequenceName.GetNameString(), *AnimSeq->Notifies(I).Notify->GetName(), I, *AnimSeq->Notifies(I).Notify->GetOuter()->GetName());
						AnimSeq->Notifies(I).Notify = CastChecked<UAnimNotify>( UObject::StaticConstructObject(AnimSeq->Notifies(I).Notify->GetClass(), AnimSeq,NAME_None,0,AnimSeq->Notifies(I).Notify) );
						debugf(TEXT("After fixed (Current Outer:%s)"), *AnimSeq->Notifies(I).Notify->GetOuter()->GetName());
						AnimSeq->MarkPackageDirty();
					}
					if (AnimSeq->Notifies(I).Notify->GetArchetype()!=AnimSeq->Notifies(I).Notify->GetClass()->GetDefaultObject())
					{
						AnimSeq->Notifies(I).Notify->SetArchetype(AnimSeq->Notifies(I).Notify->GetClass()->GetDefaultObject());
						AnimSeq->MarkPackageDirty();
					}
				}
			}
		}
	}

	return TRUE;
}

/**
 *	Helper function for listing package dependencies
 *
 *	@param	bInMapDependencies		If TRUE, find the dependencies of the loaded levels
 *									If FALSE, find the dependencies of script packages
 *	@param	InStr					The EXEC command string
 */
void EditorExecHelper_ListPkgDependencies(UBOOL bInMapDependencies, const TCHAR* InStr)
{
	TArray<UPackage*> PackagesToProcess;
	TMap<FString,UBOOL> ReferencedPackages;
	TMap<FString,UBOOL> ReferencedPackagesWithTextures;
	UBOOL bTexturesOnly = FALSE;
	UBOOL bResave = FALSE;

	// Check the 'command line'
	if (ParseCommand(&InStr,TEXT("TEXTURES"))) // LISTMAPPKGDEPENDENCIES TEXTURE
	{
		bTexturesOnly = TRUE;
		//@todo. Implement resave option!
		if (ParseCommand(&InStr,TEXT("RESAVE"))) // LISTMAPPKGDEPENDENCIES TEXTURE RESAVE
		{
			bResave = TRUE;
		}
	}
	warnf(NAME_Log, TEXT("Listing %s package dependencies%s%s"),
		bInMapDependencies ? TEXT("MAP") : TEXT("SCRIPT"),
		bTexturesOnly ? TEXT(" with TEXTURES") : TEXT(""),
		bResave ? TEXT(" RESAVE") : TEXT(""));

	if (bInMapDependencies == TRUE)
	{
		// For each loaded level, list out it's dependency map
		for (TObjectIterator<ULevel> LevelIt; LevelIt; ++LevelIt)
		{
			ULevel* Level = *LevelIt;
			UPackage* LevelPackage = Level->GetOutermost();
			FString LevelPackageName = LevelPackage->GetName();
			warnf(NAME_Log, TEXT("\tFound level %s - %s"), *Level->GetPathName(), *LevelPackageName);

			if (LevelPackageName.StartsWith(TEXT("Untitled")) == FALSE)
			{
				PackagesToProcess.AddUniqueItem(LevelPackage);
			}
		}
	}
	else
	{
		// Get a list of all script package names split by type, excluding editor- only ones.
		TArray<FString>				EngineOnlyNativeScriptPackageNames;
		TArray<FString>				NativeScriptPackageNames;
		TArray<FString>				NonNativeScriptPackageNames;
		// get just the engine native so we can calculate the first game script
		appGetScriptPackageNames(EngineOnlyNativeScriptPackageNames, SPT_EngineNative, GEngineIni);
		// we want to cook native editor packages when cooking for Windows
		appGetScriptPackageNames(NativeScriptPackageNames, SPT_Native, GEngineIni);
		appGetScriptPackageNames(NonNativeScriptPackageNames, SPT_NonNative, GEngineIni);

		TArray<FString> AllScriptPackages = EngineOnlyNativeScriptPackageNames;
		AllScriptPackages += NativeScriptPackageNames;
		AllScriptPackages += NonNativeScriptPackageNames;

		TMap<FString,UBOOL> ProcessedList;
		FString DummyFilename;

		for (INT PkgIdx = 0; PkgIdx < AllScriptPackages.Num(); PkgIdx++)
		{
			FString PkgName = AllScriptPackages(PkgIdx);
			if (ProcessedList.Find(PkgName) == NULL)
			{
				UPackage* ScriptPackage = UObject::LoadPackage(NULL, *PkgName, LOAD_None);
				if (ScriptPackage != NULL)
				{
					PackagesToProcess.AddUniqueItem(ScriptPackage);
				}
			}
		}
	}

	// For each package in the list, generate the appropriate package dependency list
	for (INT PkgIdx = 0; PkgIdx < PackagesToProcess.Num(); PkgIdx++)
	{
		UPackage* ProcessingPackage = PackagesToProcess(PkgIdx);
		FString ProcessingPackageName = ProcessingPackage->GetName();
		warnf(NAME_Log, TEXT("Processing package %s..."), *ProcessingPackageName);
		if (ProcessingPackage->IsDirty() == TRUE)
		{
			warnf(NAME_Log, TEXT("\tPackage is dirty so results may not contain all references!"));
			warnf(NAME_Log, TEXT("\tResave packages and run again to ensure accurate results."));
		}

		ULinkerLoad* Linker = ProcessingPackage->GetLinker();
		if (Linker == NULL)
		{
			// Create a new linker object which goes off and tries load the file.
			Linker = UObject::GetPackageLinker(NULL, *(ProcessingPackage->GetName()), LOAD_Throw, NULL, NULL );
		}
		if (Linker != NULL)
		{
			for (INT ImportIdx = 0; ImportIdx < Linker->ImportMap.Num(); ImportIdx++)
			{
				// don't bother outputting package references, just the objects
				if (Linker->ImportMap(ImportIdx).ClassName != NAME_Package)
				{
					// get package name of the import
					FString ImportPackage = FFilename(Linker->GetImportPathName(ImportIdx)).GetBaseFilename();
					INT PeriodIdx = ImportPackage.InStr(TEXT("."));
					if (PeriodIdx != INDEX_NONE)
					{
						ImportPackage = ImportPackage.Left(PeriodIdx);
					}
					ReferencedPackages.Set(ImportPackage, TRUE);
				}
			}
		}
		else
		{
			warnf(NAME_Log, TEXT("\t\tCouldn't get package linker. Skipping..."));
		}
	}

	if (bTexturesOnly == TRUE)
	{
		FName CheckTexture2DName(TEXT("Texture2D"));
		FName CheckCubeTextureName(TEXT("TextureCube"));
		FName CheckLightmap2DName(TEXT("Lightmap2D"));
		FName CheckShadowmap2DName(TEXT("Shadowmap2D"));
		
		for (TMap<FString,UBOOL>::TIterator PkgIt(ReferencedPackages); PkgIt; ++PkgIt)
		{
			FString RefdPkgName = PkgIt.Key();
			UPackage* RefdPackage = UObject::LoadPackage(NULL, *RefdPkgName, LOAD_None);
			if (RefdPackage != NULL)
			{
				ULinkerLoad* Linker = RefdPackage->GetLinker();
				if (Linker == NULL)
				{
					// Create a new linker object which goes off and tries load the file.
					Linker = UObject::GetPackageLinker(NULL, *RefdPkgName, LOAD_Throw, NULL, NULL );
				}
				if (Linker != NULL)
				{
					for (INT ExportIdx = 0; ExportIdx < Linker->ExportMap.Num(); ExportIdx++)
					{
						FName CheckClassName = Linker->GetExportClassName(ExportIdx);
						UClass* CheckClass = (UClass*)(UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *(CheckClassName.ToString()), TRUE));
						if (
							(CheckClass != NULL) &&
							(CheckClass->IsChildOf(UTexture::StaticClass()) == TRUE)
							)
						{
							ReferencedPackagesWithTextures.Set(RefdPkgName, TRUE);
							break;
						}
					}
				}
			}
		}
		ReferencedPackages.Empty();
		ReferencedPackages = ReferencedPackagesWithTextures;
	}

	warnf(NAME_Log, TEXT("--------------------------------------------------------------------------------"));
	warnf(NAME_Log, TEXT("Referenced packages%s..."), 
		bTexturesOnly ? TEXT(" (containing Textures)") : TEXT(""));
	for (TMap<FString,UBOOL>::TIterator PkgIt(ReferencedPackages); PkgIt; ++PkgIt)
	{
		warnf(NAME_Log, TEXT("\t%s"), *(PkgIt.Key()));
	}
}

void EditorExecHelper_DumpMobileFlattenSettings()
{
	warnf(NAME_Log, TEXT("--------------------------------------------------------------------------------"));
	warnf(NAME_Log, TEXT("Dumping mobile flatten settings...."));
	warnf(NAME_Log, TEXT("Material,AutoFlatten,DirLightDirX,DirLightDirY,DirLightDirZ,DirLightBright,DirLightColorR,DirLightColorG,DirLightColorB,BounceLight,BLDirX,BLDirY,BLDirZ,BLBright,BLColorR,BLColorG,BLColorB,SLBright,SLColorR,SLColorG,SLColorB"));
	for (TObjectIterator<UMaterial> MatIt; MatIt; ++MatIt)
	{
		UMaterial* Material = *MatIt;
		warnf(NAME_Log, TEXT("%s,%s,%5.3f,%5.3f,%5.3f,%5.3f,%5.3f,%5.3f,%5.3f,%s,%5.3f,%5.3f,%5.3f,%5.3f,%5.3f,%5.3f,%5.3f,%5.3f,%5.3f,%5.3f,%5.3f"),
			*(Material->GetPathName()),
			Material->bAutoFlattenMobile ? TEXT("AUTO") : TEXT(""),
			Material->MobileDirectionalLightDirection.X,
			Material->MobileDirectionalLightDirection.Y,
			Material->MobileDirectionalLightDirection.Z,
			Material->MobileDirectionalLightBrightness,
			Material->MobileDirectionalLightColor.R,
			Material->MobileDirectionalLightColor.G,
			Material->MobileDirectionalLightColor.B,
			Material->bMobileEnableBounceLight ? TEXT("BOUNCE") : TEXT("NO BOUNCE"),
			Material->MobileBounceLightDirection.X,
			Material->MobileBounceLightDirection.Y,
			Material->MobileBounceLightDirection.Z,
			Material->MobileBounceLightBrightness,
			Material->MobileBounceLightColor.R,
			Material->MobileBounceLightColor.G,
			Material->MobileBounceLightColor.B,
			Material->MobileSkyLightBrightness,
			Material->MobileSkyLightColor.R,
			Material->MobileSkyLightColor.G,
			Material->MobileSkyLightColor.B);
	}
	warnf(NAME_Log, TEXT("--------------------------------------------------------------------------------"));
}

//
// Process an incoming network message meant for the editor server
//
UBOOL UEditorEngine::Exec( const TCHAR* Stream, FOutputDevice& Ar )
{
	TCHAR CommandTemp[MAX_EDCMD];
	TCHAR ErrorTemp[256]=TEXT("Setup: ");
	UBOOL bProcessed=FALSE;

	// Echo the command to the log window
	if( appStrlen(Stream)<200 )
	{
		appStrcat( ErrorTemp, Stream );
		debugf( NAME_Cmd, Stream );
	}

	GStream = Stream;

	appStrncpy( CommandTemp, Stream, ARRAY_COUNT(CommandTemp) );
	const TCHAR* Str = &CommandTemp[0];

	appStrncpy( ErrorTemp, Str, 79 );
	ErrorTemp[79]=0;

	if( SafeExec( Stream, Ar ) )
	{
		return TRUE;
	}

	//------------------------------------------------------------------------------------
	// MISC
	//
	else if( ParseCommand(&Str,TEXT("EDCALLBACK")) )
	{
		if( ParseCommand(&Str,TEXT("SURFPROPS")) )
		{
			GCallbackEvent->Send( CALLBACK_SurfProps );
		}
		else if ( ParseCommand(&Str,TEXT("SELECTEDPROPS")) )
		{
			GCallbackEvent->Send( CALLBACK_SelectedProps );
		}
		else if( ParseCommand( &Str, TEXT( "FITTEXTURETOSURFACE" ) ) )
		{
			GCallbackEvent->Send( CALLBACK_FitTextureToSurface );
		}
	}
	else if(ParseCommand(&Str,TEXT("STATICMESH")))
	{
		if( Exec_StaticMesh( Str, Ar ) )
		{
			return TRUE;
		}
	}
	else if(ParseCommand(&Str,TEXT("OPENKISMETDEBUGGER")))
	{
		TCHAR SeqName[512];
		if (Parse(Str, TEXT("SEQUENCE="), SeqName, 512))
		{
			RequestKismetDebuggerOpen(SeqName);
			return TRUE;
		}
	}
	//------------------------------------------------------------------------------------
	// BRUSH
	//
	else if( ParseCommand(&Str,TEXT("BRUSH")) )
	{
		if( Exec_Brush( Str, Ar ) )
		{
			return TRUE;
		}
	}
	//------------------------------------------------------------------------------------
	// BSP
	//
	else if( ParseCommand( &Str, TEXT("BSP") ) )
	{
		appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"), CommandTemp ) );
	}
	//------------------------------------------------------------------------------------
	// LIGHT
	//
	else if( ParseCommand( &Str, TEXT("LIGHT") ) )
	{
		appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"), CommandTemp ) );
	}
	//------------------------------------------------------------------------------------
	// MAP
	//
	else if (ParseCommand(&Str,TEXT("MAP")))
	{
		if (ParseCommand(&Str,TEXT("ROTGRID"))) // MAP ROTGRID [PITCH=..] [YAW=..] [ROLL=..]
		{
			return Map_Rotgrid( Str, Ar );
		}
		else if (ParseCommand(&Str,TEXT("ANGLESNAPTYPE")))
		{
			INT AngleSnapType = EST_ANGLE;
			if ( Parse( Str, TEXT( "TYPE=" ), AngleSnapType ) )
			{
				FinishAllSnaps();
				Constraints.AngleSnapType = AngleSnapType;
				RedrawLevelEditingViewports();
				GCallbackEvent->Send( CALLBACK_UpdateUI );
			}
		}
		else if (ParseCommand(&Str,TEXT("SELECT")))
		{
			return Map_Select( Str, Ar );
		}
		else if( ParseCommand(&Str,TEXT("BRUSH")) )
		{
			return Map_Brush( Str, Ar );
		}
		else if (ParseCommand(&Str,TEXT("SENDTO")))
		{
			return Map_Sendto( Str, Ar );
		}
		else if( ParseCommand(&Str,TEXT("REBUILD")) )
		{
			return Map_Rebuild( Str, Ar );
		}
		else if( ParseCommand (&Str,TEXT("NEW")) )
		{
			appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"), CommandTemp ) );
		}
		else if( ParseCommand( &Str, TEXT("LOAD") ) )
		{
			return Map_Load( Str, Ar );
		}
		else if( ParseCommand( &Str, TEXT("IMPORT") ) )
		{
			return Map_Import( Str, Ar, TRUE );
		}
		else if( ParseCommand( &Str, TEXT("IMPORTADD") ) )
		{
			SelectNone( FALSE, TRUE );
			return Map_Import( Str, Ar, FALSE );
		}
		else if (ParseCommand (&Str,TEXT("EXPORT")))
		{
			appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"), CommandTemp ) );
		}
		else if (ParseCommand (&Str,TEXT("SETBRUSH"))) // MAP SETBRUSH (set properties of all selected brushes)
		{
			return Map_Setbrush( Str, Ar );
		}
		else if (ParseCommand (&Str,TEXT("CHECK")))
		{
			const UBOOL bClearMessages = !ParseCommand(&Str,TEXT("DONTCLEARMESSAGES"));
			const UBOOL bDisplayDialog = !ParseCommand(&Str,TEXT("DONTDISPLAYDIALOG"));
			bDoReferenceChecks = !ParseCommand(&Str,TEXT("DONTDOSLOWREFCHECK"));
			return Map_Check( Str, Ar, FALSE, bClearMessages, bDisplayDialog );
		}
		else if (ParseCommand (&Str,TEXT("CHECKDEP")))
		{
			const UBOOL bClearMessages = !ParseCommand(&Str,TEXT("DONTCLEARMESSAGES"));
			const UBOOL bDisplayDialog = !ParseCommand(&Str,TEXT("DONTDISPLAYDIALOG"));
			bDoReferenceChecks = !ParseCommand(&Str,TEXT("DONTDOSLOWREFCHECK"));
			return Map_Check( Str, Ar, TRUE, bClearMessages, bDisplayDialog );
		}
		else if (ParseCommand (&Str,TEXT("SCALE")))
		{
			return Map_Scale( Str, Ar );
		}
	}
	//------------------------------------------------------------------------------------
	// SELECT: Rerouted to mode-specific command
	//
	else if( ParseCommand(&Str,TEXT("SELECT")) )
	{
		if( ParseCommand(&Str,TEXT("NONE")) )
		{
			const FScopedTransaction Transaction( TEXT("Select None") );
			SelectNone( TRUE, TRUE );
			RedrawLevelEditingViewports();
			return TRUE;
		}
		else if ( ParseCommand(&Str,TEXT("BUILDERBRUSH")) )
		{
			// Deselect everything else in the editor and select the builder brush instead
			const FScopedTransaction Trans( *LocalizeUnrealEd("SelectBuilderBrush") );
			GetSelectedActors()->Modify();
			SelectNone( FALSE, TRUE );
			GetSelectedActors()->Select( GWorld->GetBrush() );	
			NoteSelectionChange();
		}
	}
	//------------------------------------------------------------------------------------
	// DELETE: Rerouted to mode-specific command
	//
	else if (ParseCommand(&Str,TEXT("DELETE")))
	{
		// If geometry mode is active, give it a chance to handle this command.  If it does not, use the default handler
		if( !GEditorModeTools().IsModeActive( EM_Geometry ) || !( (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry) )->ExecDelete() )
		{
			return Exec( TEXT("ACTOR DELETE") );
		}

		return TRUE;
	}
	//------------------------------------------------------------------------------------
	// DUPLICATE: Rerouted to mode-specific command
	//
	else if (ParseCommand(&Str,TEXT("DUPLICATE")))
	{
		return Exec( TEXT("ACTOR DUPLICATE") );
	}
	//------------------------------------------------------------------------------------
	// POLY: Polygon adjustment and mapping
	//
	else if( ParseCommand(&Str,TEXT("POLY")) )
	{
		if( Exec_Poly( Str, Ar ) )
		{
			return TRUE;
		}
	}
	//------------------------------------------------------------------------------------
	// ANIM: All mesh/animation management.
	//
	else if( ParseCommand(&Str,TEXT("NEWANIM")) )
	{
		appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"), CommandTemp ) );
	}
	//------------------------------------------------------------------------------------
	// Transaction tracking and control
	//
	else if( ParseCommand(&Str,TEXT("TRANSACTION")) )
	{
		if ( Exec_Transaction( Str, Ar ) )
		{
			return TRUE;
		}
	}
	//------------------------------------------------------------------------------------
	// General objects
	//
	else if( ParseCommand(&Str,TEXT("OBJ")) )
	{
		if( Exec_Obj( Str, Ar ) )
		{
			return TRUE;
		}
	}
	//------------------------------------------------------------------------------------
	// CLASS functions
	//
	else if( ParseCommand(&Str,TEXT("CLASS")) )
	{
		if( Exec_Class( Str, Ar ) )
		{
			return TRUE;
		}
	}
	//------------------------------------------------------------------------------------
	// CAMERA: cameras
	//
	else if( ParseCommand(&Str,TEXT("CAMERA")) )
	{
		if( Exec_Camera( Str, Ar ) )
		{
			return TRUE;
		}
	}
	//------------------------------------------------------------------------------------
	// LEVEL
	//
	if( ParseCommand(&Str,TEXT("LEVEL")) )
	{
		appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"), CommandTemp ) );
		return FALSE;
	}
	//------------------------------------------------------------------------------------
	// LEVEL
	//
	if( ParseCommand(&Str,TEXT("PREFAB")) )
	{
		if( ParseCommand(&Str,TEXT("SELECTACTORSINPREFABS")) )
		{
			edactSelectPrefabActors();
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
	//------------------------------------------------------------------------------------
	// PARTICLE: Particle system-related commands
	//
	else if (ParseCommand(&Str,TEXT("PARTICLE")))
	{
		if (Exec_Particle(Str, Ar))
		{
			return TRUE;
		}
	}
	//----------------------------------------------------------------------------------
	// QUIT_EDITOR - Closes the main editor frame
	//
	else if( ParseCommand(&Str,TEXT("QUIT_EDITOR")) )
	{
		CloseEditor();
		return TRUE;
	}
	//----------------------------------------------------------------------------------
	// SETSHADOWPARENT - Forcibly sets shadow parents of DynamicSMActors.
	//
	else if( ParseCommand(&Str,TEXT("SETSHADOWPARENT")) )
	{
		ADynamicSMActor*			ShadowParent = NULL;
		TArray<ADynamicSMActor*>	ChildActors;

		for( FSelectedActorIterator It; It; ++It )
		{
			ADynamicSMActor* Actor = Cast<ADynamicSMActor>(*It);
			if ( Actor && Actor->StaticMeshComponent )
			{
				// The first found actor is the shadow parent.
				if ( !ShadowParent )
				{
					ShadowParent = Actor;
				}
				else
				{
					ChildActors.AddItem( Actor );
				}
			}
		}

		if ( ShadowParent && ChildActors.Num() > 0 )
		{
			Ar.Logf( TEXT("Shadow parent is %s"), *ShadowParent->GetName() );
			const FScopedTransaction Transaction( TEXT("SetShadowParent") );
			// Make sure the parent object itself is not parented.
			ShadowParent->StaticMeshComponent->SetShadowParent( NULL );
			// Parent child actors to parent.
			for ( INT i = 0 ; i < ChildActors.Num() ; ++i )
			{
				ADynamicSMActor* Actor = ChildActors(i);
				Actor->StaticMeshComponent->Modify();
				Actor->StaticMeshComponent->MarkPackageDirty();
				Actor->StaticMeshComponent->SetShadowParent( ShadowParent->StaticMeshComponent );
				Ar.Logf( TEXT("Parenting %s to %s"), *Actor->GetName(), *ShadowParent->GetName() );
			}
		}
		else
		{
			Ar.Logf( TEXT("couldn't find at least 2 DynamicSMActors") );
		}

		return TRUE;
	}
	//----------------------------------------------------------------------------------
	// LIGHTMASSDEBUG - Toggles whether UnrealLightmass.exe is launched automatically (default),
	// or must be launched manually (e.g. through a debugger) with the -debug command line parameter.
	//
	else if( ParseCommand(&Str,TEXT("LIGHTMASSDEBUG")) )
	{
		extern UBOOL GLightmassDebugMode;
		GLightmassDebugMode = !GLightmassDebugMode;
		Ar.Logf( TEXT("Lightmass Debug Mode: %s"), GLightmassDebugMode ? TEXT("TRUE (launch UnrealLightmass.exe manually)") : TEXT("FALSE") );
		return TRUE;
	}
	//----------------------------------------------------------------------------------
	// LIGHTMASSSTATS - Toggles whether all participating Lightmass agents will report
	// back detailed stats to the log.
	//
	else if( ParseCommand(&Str,TEXT("LIGHTMASSSTATS")) )
	{
		extern UBOOL GLightmassStatsMode;
		GLightmassStatsMode = !GLightmassStatsMode;
		Ar.Logf( TEXT("Show detailed Lightmass statistics: %s"), GLightmassStatsMode ? TEXT("ENABLED") : TEXT("DISABLED") );
		return TRUE;
	}
	//----------------------------------------------------------------------------------
	// SWARMDISTRIBUTION - Toggles whether to enable Swarm distribution for Jobs.
	// Default is off (local builds only).
	//
	else if( ParseCommand(&Str,TEXT("SWARMDISTRIBUTION")) )
	{
		extern FSwarmDebugOptions GSwarmDebugOptions;
		GSwarmDebugOptions.bDistributionEnabled = !GSwarmDebugOptions.bDistributionEnabled;
		debugf(TEXT("Swarm Distribution Mode: %s"), GSwarmDebugOptions.bDistributionEnabled ? TEXT("TRUE (Jobs will be distributed)") : TEXT("FALSE (Jobs will be local only)"));
		return TRUE;
	}
	//----------------------------------------------------------------------------------
	// DETLIGHT - Toggles deterministic lighting mode for Lightmass builds.
	//	Default value is TRUE
	//
	else if (
		( ParseCommand(&Str,TEXT("TOGGLEDETERMINISTICLIGHTING")) ) ||
		( ParseCommand(&Str,TEXT("DETLIGHT")) ))
	{
		GLightmassDebugOptions.bUseDeterministicLighting = !GLightmassDebugOptions.bUseDeterministicLighting;
		debugf(TEXT("Deterministic lighting will be %s"), GLightmassDebugOptions.bUseDeterministicLighting ? TEXT("ENABLED") : TEXT("DISABLED"));
		return TRUE;
	}
	//----------------------------------------------------------------------------------
	// LMIMM - Toggles Lightmass ImmediateImport mode.
	//	If TRUE, Lightmass will import mappings immediately as they complete.
	//	It will not process them, however.
	//	Default value is FALSE
	//
	else if (
		( ParseCommand(&Str,TEXT("LMIMMEDIATE")) ) ||
		( ParseCommand(&Str,TEXT("LMIMM")) ))
	{
		GLightmassDebugOptions.bUseImmediateImport = !GLightmassDebugOptions.bUseImmediateImport;
		debugf(TEXT("Lightmass Immediate Import will be %s"), GLightmassDebugOptions.bUseImmediateImport ? TEXT("ENABLED") : TEXT("DISABLED"));
		return TRUE;
	}
	//----------------------------------------------------------------------------------
	// LMIMP - Toggles Lightmass ImmediateProcess mode.
	//	If TRUE, Lightmass will process appropriate mappings as they are imported.
	//	NOTE: Requires ImmediateMode be enabled to actually work.
	//	Default value is FALSE
	//
	else if ( ParseCommand(&Str,TEXT("LMIMP")) )
	{
		GLightmassDebugOptions.bImmediateProcessMappings = !GLightmassDebugOptions.bImmediateProcessMappings;
		debugf(TEXT("Lightmass Immediate Process will be %s"), GLightmassDebugOptions.bImmediateProcessMappings ? TEXT("ENABLED") : TEXT("DISABLED"));
		if ((GLightmassDebugOptions.bImmediateProcessMappings == TRUE) && (GLightmassDebugOptions.bUseImmediateImport == FALSE))
		{
			debugf(TEXT("\tLightmass Immediate Import needs to be enabled for this to matter..."));
		}
		return TRUE;
	}
	//----------------------------------------------------------------------------------
	// LMSORT - Toggles Lightmass sorting mode.
	//	If TRUE, Lightmass will sort mappings by texel cost.
	//
	else if ( ParseCommand(&Str,TEXT("LMSORT")) )
	{
		GLightmassDebugOptions.bSortMappings = !GLightmassDebugOptions.bSortMappings;
		debugf(TEXT("Lightmass Sorting is now %s"), GLightmassDebugOptions.bSortMappings ? TEXT("ENABLED") : TEXT("DISABLED"));
		return TRUE;
	}
	//----------------------------------------------------------------------------------
	// LMDEBUGMAT - Toggles Lightmass dumping of exported material samples.
	//	If TRUE, Lightmass will write out BMPs for each generated material property 
	//	sample to <GAME>\ScreenShots\Materials.
	//
	else if ( ParseCommand(&Str,TEXT("LMDEBUGMAT")) )
	{
		GLightmassDebugOptions.bDebugMaterials = !GLightmassDebugOptions.bDebugMaterials;
		debugf(TEXT("Lightmass Dump Materials is now %s"), GLightmassDebugOptions.bDebugMaterials ? TEXT("ENABLED") : TEXT("DISABLED"));
		return TRUE;
	}
	//----------------------------------------------------------------------------------
	// LMPADDING - Toggles Lightmass padding of mappings.
	//
	else if ( ParseCommand(&Str,TEXT("LMPADDING")) )
	{
		GLightmassDebugOptions.bPadMappings = !GLightmassDebugOptions.bPadMappings;
		debugf(TEXT("Lightmass Mapping Padding is now %s"), GLightmassDebugOptions.bPadMappings ? TEXT("ENABLED") : TEXT("DISABLED"));
		return TRUE;
	}
	//----------------------------------------------------------------------------------
	// LMDEBUGPAD - Toggles Lightmass debug padding of mappings.
	// Means nothing if LightmassPadMappings is not enabled...
	//
	else if ( ParseCommand(&Str,TEXT("LMDEBUGPAD")) )
	{
		GLightmassDebugOptions.bDebugPaddings = !GLightmassDebugOptions.bDebugPaddings;
		debugf(TEXT("Lightmass Mapping Debug Padding is now %s"), GLightmassDebugOptions.bDebugPaddings ? TEXT("ENABLED") : TEXT("DISABLED"));
		return TRUE;
	}
	//----------------------------------------------------------------------------------
	// LMPROFILE - Switched settings for Lightmass to a mode suited for profiling.
	// Specifically, it disabled ImmediateImport and ImmediateProcess of completed mappings.
	//
	else if( ParseCommand(&Str,TEXT("LMPROFILE")) )
	{
		GLightmassDebugOptions.bUseImmediateImport = FALSE;
		GLightmassDebugOptions.bImmediateProcessMappings = FALSE;
		debugf(TEXT("Lightmass Profiling mode is ENABLED"));
		debugf(TEXT("\tLightmass ImmediateImport mode is DISABLED"));
		debugf(TEXT("\tLightmass ImmediateProcess mode is DISABLED"));
		return TRUE;
	}
	//----------------------------------------------------------------------------------
	// SETREPLACEMENT - Sets the replacement primitive for selected actors
	//
	else if( ParseCommand(&Str,TEXT("SETREPLACEMENT")) )
	{
		UPrimitiveComponent* ReplacementComponent;
		if (!ParseObject<UPrimitiveComponent>(Str, TEXT("COMPONENT="), ReplacementComponent, ANY_PACKAGE))
		{
			Ar.Logf(TEXT("Replacement component was not specified (COMPONENT=)"));
			return TRUE;
		}

		// filter which types of component to set to the ReplacementComponent
		UClass* ClassToReplace;
		if (!ParseObject<UClass>(Str, TEXT("CLASS="), ClassToReplace, ANY_PACKAGE))
		{
			ClassToReplace = NULL;
		}

		// attempt to set replacement component for all selected actors
		for( FSelectedActorIterator It; It; ++It )
		{
			for (INT ComponentIndex = 0; ComponentIndex < It->Components.Num(); ComponentIndex++)
			{
				UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(It->Components(ComponentIndex));
				// if the primitive component matches the class we are looking for (if specified)
				// then set it's replacement component
				if (PrimitiveComponent && (ClassToReplace == NULL || PrimitiveComponent->IsA(ClassToReplace)))
				{
					PrimitiveComponent->ReplacementPrimitive = ReplacementComponent;
				}
			}
		}
	}
	//------------------------------------------------------------------------------------
	// Other handlers.
	//
	else if( GWorld->Exec(Stream,Ar) )
	{
		// The level handled it.
		bProcessed = TRUE;
	}
	else if( UEngine::Exec(Stream,Ar) )
	{
		// The engine handled it.
		bProcessed = TRUE;
	}
	else if( ParseCommand(&Str,TEXT("SELECTNAME")) )
	{
		FName FindName=NAME_None;
		Parse( Str, TEXT("NAME="), FindName );

		for( FActorIterator It; It; ++It ) 
		{
			AActor* Actor = *It;
			SelectActor( Actor, Actor->GetFName()==FindName, NULL, 0 );
		}
		bProcessed = TRUE;
	}
	// Dump a list of all public UObjects in the level
	else if( ParseCommand(&Str,TEXT("DUMPPUBLIC")) )
	{
		for( FObjectIterator It; It; ++It )
		{
			UObject* Obj = *It;
			if(Obj && IsInALevel(Obj) && Obj->HasAnyFlags(RF_Public))
			{
				debugf( TEXT("--%s"), *(Obj->GetFullName()) );
			}
		}
	}
	else if( ParseCommand(&Str,TEXT("EXPORTLOC")) )
	{
		while( *Str==' ' )
		{
			Str++;
		}
		UPackage* Package = LoadPackage( NULL, Str, LOAD_None );
		if( Package )
		{
			FString IntName;
			FLocalizationExport::GenerateIntNameFromPackageName( Str, IntName );
			FLocalizationExport::ExportPackage( Package, *IntName, FALSE, TRUE );
		}
	}
	else if( ParseCommand(&Str,TEXT("JUMPTO")) )
	{
		FVector Loc;
		if( GetFVECTOR( Str, Loc ) )
		{
			for( INT i=0; i<ViewportClients.Num(); i++ )
			{
				ViewportClients(i)->ViewLocation = Loc;
			}
		}
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("BugItGo")) )
	{
		const TCHAR* Stream = Str;
		FVector Loc;
		Stream = GetFVECTORSpaceDelimited( Stream, Loc );
		if( Stream != NULL )
		{
			for( INT i=0; i<ViewportClients.Num(); i++ )
			{
				ViewportClients(i)->ViewLocation = Loc;
			}
		}

		// so here we need to do move the string forward by a ' ' to get to the Rotator data
		if( Stream != NULL )
		{
			Stream = appStrchr(Stream,' ');
			if( Stream != NULL )
			{
				++Stream;
			}
		}


		FRotator Rot;
		Stream = GetFROTATORSpaceDelimited( Stream, Rot, 1.0f );
		if( Stream != NULL )
		{
			for( INT i=0; i<ViewportClients.Num(); i++ )
			{
				ViewportClients(i)->ViewRotation = Rot;
			}
		}

		RedrawLevelEditingViewports();

		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("DUMPCOVERSTATS")))
	{
		GWorld->DumpCoverStats();
	}
	else if( ParseCommand(&Str,TEXT("SELECTDYNAMIC")) )
	{	
		// Select actors that have terrain/ staticmesh or skeletal mesh components that are not set up to
		// receive static/ precomputed lighting and are visible in the game.
		for( TObjectIterator<UPrimitiveComponent> It; It; ++It )
		{
			UPrimitiveComponent* PrimitiveComponent = *It;
			if( !PrimitiveComponent->HiddenEditor
			&&	!PrimitiveComponent->HiddenGame
			&&	!PrimitiveComponent->HasStaticShadowing() )
			{
				if( PrimitiveComponent->IsA(UStaticMeshComponent::StaticClass())
				||	PrimitiveComponent->IsA(USkeletalMeshComponent::StaticClass())
				||	PrimitiveComponent->IsA(UTerrainComponent::StaticClass()) )
				{
					AActor* Owner = PrimitiveComponent->GetOwner();
					if( Owner )
					{
						GEditor->SelectActor( Owner, TRUE, NULL, FALSE );
					}
				}
			}
		}
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("DUMPPRIMITIVESTATS")) )
	{
		extern UBOOL GDumpPrimitiveStatsToCSVDuringNextUpdate;
		GDumpPrimitiveStatsToCSVDuringNextUpdate = TRUE;
		return TRUE;
	}
	else if ( ParseCommand(&Str,TEXT("TAGSOUNDS")) )
	{
		INT NumObjects = 0;
		INT TotalSize = 0;
		for( FObjectIterator It(USoundNodeWave::StaticClass()); It; ++It )
		{
			++NumObjects;
			It->SetFlags( RF_Marked );

			USoundNodeWave* Wave = static_cast<USoundNodeWave*>(*It);
			const INT Size = Wave->GetResourceSize();
			TotalSize += Size;
		}
		debugf( TEXT("Marked %i sounds %10.2fMB"), NumObjects, ((FLOAT)TotalSize) /(1024.f*1024.f) );
		return TRUE;
	}
	else if ( ParseCommand(&Str,TEXT("CHECKSOUNDS")) )
	{
		TArray<USoundNodeWave*> WaveList;
		for( FObjectIterator It(USoundNodeWave::StaticClass()); It; ++It )
		{
			USoundNodeWave* Wave = static_cast<USoundNodeWave*>(*It);
			if ( !Wave->HasAnyFlags( RF_Marked ) )
			{
				WaveList.AddItem( Wave );
			}
			Wave->ClearFlags( RF_Marked );
		}

		// Sort based on full path name.
		Sort<USE_COMPARE_POINTER(USoundNodeWave,UnEdSrv)>( &WaveList(0), WaveList.Num() );

		TArray<FWaveCluster> Clusters;
		Clusters.AddItem( FWaveCluster(TEXT("Total")) );
		Clusters.AddItem( FWaveCluster(TEXT("Ambient")) );
		Clusters.AddItem( FWaveCluster(TEXT("Foley")) );
		Clusters.AddItem( FWaveCluster(TEXT("Chatter")) );
		Clusters.AddItem( FWaveCluster(TEXT("Dialog")) );
		Clusters.AddItem( FWaveCluster(TEXT("Efforts")) );
		const INT NumCoreClusters = Clusters.Num();

		// Output information.
		INT TotalSize = 0;
		debugf( TEXT("=================================================================================") );
		debugf( TEXT("%60s %10s"), TEXT("Wave Name"), TEXT("Size") );
		for ( INT WaveIndex = 0 ; WaveIndex < WaveList.Num() ; ++WaveIndex )
		{
			USoundNodeWave* Wave = WaveList(WaveIndex);
			const INT WaveSize = Wave->GetResourceSize();
			UPackage* WavePackage = Wave->GetOutermost();
			const FString PackageName( WavePackage->GetName() );

			// Totals.
			Clusters(0).Num++;
			Clusters(0).Size += WaveSize;

			// Core clusters
			for ( INT ClusterIndex = 1 ; ClusterIndex < NumCoreClusters ; ++ClusterIndex )
			{
				FWaveCluster& Cluster = Clusters(ClusterIndex);
				if ( PackageName.InStr( Cluster.Name ) != -1 )
				{
					Cluster.Num++;
					Cluster.Size += WaveSize;
				}
			}

			// Package
			UBOOL bFoundMatch = FALSE;
			for ( INT ClusterIndex = NumCoreClusters ; ClusterIndex < Clusters.Num() ; ++ClusterIndex )
			{
				FWaveCluster& Cluster = Clusters(ClusterIndex);
				if ( PackageName == Cluster.Name )
				{
					// Found a cluster with this package name.
					Cluster.Num++;
					Cluster.Size += WaveSize;
					bFoundMatch = TRUE;
					break;
				}
			}
			if ( !bFoundMatch )
			{
				// Create a new cluster with the package name.
				FWaveCluster NewCluster( *PackageName );
				NewCluster.Num = 1;
				NewCluster.Size = WaveSize;
				Clusters.AddItem( NewCluster );
			}

			// Dump bulk sound list.
			debugf( TEXT("%70s %10.2fk"), *Wave->GetPathName(), ((FLOAT)WaveSize)/1024.f );
		}
		debugf( TEXT("=================================================================================") );
		debugf( TEXT("%60s %10s %10s"), TEXT("Cluster Name"), TEXT("Num"), TEXT("Size") );
		debugf( TEXT("=================================================================================") );
		INT TotalClusteredSize = 0;
		for ( INT ClusterIndex = 0 ; ClusterIndex < Clusters.Num() ; ++ClusterIndex )
		{
			const FWaveCluster& Cluster = Clusters(ClusterIndex);
			if ( ClusterIndex == NumCoreClusters )
			{
				debugf( TEXT("---------------------------------------------------------------------------------") );
				TotalClusteredSize += Cluster.Size;
			}
			debugf( TEXT("%60s %10i %10.2fMB"), *Cluster.Name, Cluster.Num, ((FLOAT)Cluster.Size)/(1024.f*1024.f) );
		}
		debugf( TEXT("=================================================================================") );
		debugf( TEXT("Total Clusterd: %10.2fMB"), ((FLOAT)TotalClusteredSize)/(1024.f*1024.f) );
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("PURGEPHYSMATREFS")) )
	{
		debugf(TEXT("Purging physical material references. Do NOT save packages unless you know EXACTLY what you are doing"));
		for( TObjectIterator<UMaterial> It; It; ++It )
		{
			It->PhysMaterial = NULL;
			It->PhysicalMaterial = NULL;
			It->PhysMaterialMask = NULL;
			It->BlackPhysicalMaterial = NULL;
			It->WhitePhysicalMaterial = NULL;
		}
		for( TObjectIterator<UMaterialInstance> It; It; ++It )
		{
			It->PhysMaterial = NULL;
			It->PhysMaterialMask = NULL;
			It->BlackPhysicalMaterial = NULL;
			It->WhitePhysicalMaterial = NULL;
		}
		for( TObjectIterator<UPrimitiveComponent> It; It; ++It )
		{
			It->PhysMaterialOverride = NULL;
		}
		for( TObjectIterator<URB_BodyInstance> It; It; ++It )
		{
			It->PhysMaterialOverride = NULL;
		}
	}
	else if( ParseCommand(&Str,TEXT("PRUNEANIMSETS")) )
	{
		TMap<FString,UBOOL> AnimSetSkipBakeAndPruneMap;
		GatherBakeAndPruneStatus(GWorld->CurrentLevel, &AnimSetSkipBakeAndPruneMap);
		PruneAnimSetsInLevel(GWorld->CurrentLevel, &AnimSetSkipBakeAndPruneMap);
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("BAKEANIMSETS")) )
	{
		TMap<FString,UBOOL> AnimSetSkipBakeAndPruneMap;
		GatherBakeAndPruneStatus(GWorld->CurrentLevel, &AnimSetSkipBakeAndPruneMap);
		BakeAnimSetsInLevel(GWorld->CurrentLevel, &AnimSetSkipBakeAndPruneMap);
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("CLEANMATINEEANIMSETS")) )
	{
		// Clear out unreferenced animsets from groups...
		ClearUnreferenceAnimSetsFromGroups(GWorld->CurrentLevel);
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("FIXUPBADANIMNOTIFIERS")) )
	{
		// Clear out unreferenced animsets from groups...
		FixUpBadAnimNotifiers();
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("SETDETAILMODE")) )
	{
		TArray<AActor*> ActorsToDeselect;

		BYTE DetailMode = DM_High;
		if ( Parse( Str, TEXT("MODE="), DetailMode ) )
		{
			for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				for(UINT ComponentIndex = 0;ComponentIndex < (UINT)Actor->Components.Num();ComponentIndex++)
				{
					UPrimitiveComponent*	primComp = Cast<UPrimitiveComponent>(Actor->Components(ComponentIndex));

					if( primComp )
					{
						if( primComp->DetailMode != DetailMode )
						{
							primComp->Modify();
							primComp->DetailMode = DetailMode;
							primComp->BeginDeferredReattach();

							// If the actor will not be visible after changing the detail mode, deselect it
							if( primComp->DetailMode > GSystemSettings.DetailMode )
							{
								ActorsToDeselect.AddUniqueItem( Actor );
							}
						}
					}
				}
			}

			for( int x = 0 ; x < ActorsToDeselect.Num() ; ++x )
			{
				GEditor->SelectActor( ActorsToDeselect(x), FALSE, NULL, FALSE );
			}
		}

		GCallbackEvent->Send( CALLBACK_LevelDirtied );
		GCallbackEvent->Send( CALLBACK_RefreshPropertyWindows );
		GCallbackEvent->Send( CALLBACK_RefreshEditor );

		RedrawLevelEditingViewports( TRUE );
		bProcessed = TRUE;
	}
	else if( ParseCommand(&Str,TEXT("SETDETAILMODEVIEW")) )
	{
		BYTE DM = DM_High;
		if ( Parse( Str, TEXT("MODE="), DM ) )
		{
			DetailMode = (EDetailMode)DM;
			GSystemSettings.DetailMode = DetailMode;

			// Reattach all primitive components so the view will respect the new detail mode filter
			FGlobalComponentReattachContext ReattachContext;
		}

		RedrawLevelEditingViewports( TRUE );
		bProcessed = TRUE;
	}
	else if( ParseCommand(&Str,TEXT("REMOVECOOKEDPS3AUDIO")) )
	{
		UPackage* InsidePackage = NULL;
		ParseObject<UPackage>(Str, TEXT("PACKAGE="), InsidePackage, NULL);
		if( InsidePackage )
		{
			debugf(TEXT("Removing cooked PS3 audio data from waves in %s"), *InsidePackage->GetName());

			// Iterate over all wave instances inside package and remove cooked PS3 data.
			for( TObjectIterator<USoundNodeWave> It; It; ++It )
			{
				USoundNodeWave* SoundNodeWave = *It;
				if( SoundNodeWave->IsIn( InsidePackage ) )
				{
					SoundNodeWave->CompressedPS3Data.RemoveBulkData();
				}
			}
		}
	}
	else if( ParseCommand(&Str,TEXT("FARPLANE")) )
	{
		if ( Parse( Str, TEXT("DIST="), FarClippingPlane ) )
		{
			RedrawLevelEditingViewports( TRUE );
		}
		bProcessed = TRUE;
	}
	else if( ParseCommand(&Str,TEXT("FORCEREALTIMECOMPRESSION")) )
	{
		UPackage* InsidePackage = NULL;
		ParseObject<UPackage>(Str, TEXT("PACKAGE="), InsidePackage, NULL);
		if( InsidePackage )
		{
			// Iterate over all wave instances inside package and force realtime compression.
			for( TObjectIterator<USoundNodeWave> It; It; ++It )
			{
				USoundNodeWave* SoundNodeWave = *It;
				if( SoundNodeWave->IsIn( InsidePackage ) )
				{
					SoundNodeWave->bForceRealTimeDecompression = TRUE;
					SoundNodeWave->MarkPackageDirty();
				}
			}
		}
		bProcessed = TRUE;
	}
	else if( ParseCommand(&Str,TEXT("CLEANBSPMATERIALS")) )
	{
		const FScopedBusyCursor BusyCursor;
		const FScopedTransaction Transaction( TEXT("Clean BSP materials") );
		const INT NumRefrencesCleared = CleanBSPMaterials( FALSE, TRUE );
		// Prompt the user that the operation is complete.
		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("CleanBSPMaterialsReportF"), NumRefrencesCleared) );
		bProcessed = TRUE;
	}
	else if( ParseCommand(&Str,TEXT("CREATESMFROMBSP")) )
	{
		// Convert the current level's BSP to a static mesh placed into the package of the persitent level.
		UModel* Model = GWorld->GetModel();
		FString BSPMeshName = *(FString(TEXT("BSP-")) + Model->GetOutermost()->GetName());

		// Construct the mesh. The mesh is neither RF_Public nor RF_Standalone. This means it can only be used in the same level and will go away if unreferenced.
		UStaticMesh* StaticMesh = new( GWorld->GetOutermost(), *BSPMeshName, 0 ) UStaticMesh;
		new(StaticMesh->LODModels) FStaticMeshRenderData();
		StaticMesh->LODInfo.AddItem(FStaticMeshLODInfo());

		// Temp data
		TArray<UMaterialInterface*> Materials;
		TArray<FStaticMeshTriangle> Triangles;	

		// Convert the model's nodes into triangles.
		for( INT NodeIndex=0; NodeIndex<Model->Nodes.Num(); NodeIndex++ )
		{
			FBspNode& Node = Model->Nodes(NodeIndex);
			FBspSurf& Surf = Model->Surfs(Node.iSurf);
			const FVector& TextureBase = Model->Points(Surf.pBase);
			const FVector& TextureX = Model->Vectors(Surf.vTextureU);
			const FVector& TextureY = Model->Vectors(Surf.vTextureV);

			// Skip surfaces with unwanted materials.
			if( !Surf.Material 
			||	Surf.Material->GetMaterial() == GEditor->RemoveSurfaceMaterial
			||	Surf.Material->GetMaterial() == GEngine->DefaultMaterial )
			{
				continue;
			}

			// Find section of used material or create new one on first encounter.
			INT MaterialIndex = Materials.FindItemIndex( Surf.Material );
			if( MaterialIndex == INDEX_NONE )
			{
				MaterialIndex = Materials.AddItem(Surf.Material);
				FStaticMeshElement* NewStaticMeshElement = new(StaticMesh->LODModels(0).Elements) FStaticMeshElement(Surf.Material, MaterialIndex);
			}

			for( UINT VertexIndex=2; VertexIndex<Node.NumVertices; VertexIndex++ )
			{
				const FVert& Vert0 = Model->Verts(Node.iVertPool + 0 );
				const FVert& Vert1 = Model->Verts(Node.iVertPool + VertexIndex );
				const FVert& Vert2 = Model->Verts(Node.iVertPool + VertexIndex - 1 );
				
				const FVector& P0 = Model->Points(Vert0.pVertex);
				const FVector& P1 = Model->Points(Vert1.pVertex);
				const FVector& P2 = Model->Points(Vert2.pVertex);

				FStaticMeshTriangle NewTri;
				appMemzero(&NewTri, sizeof(FStaticMeshTriangle));

				NewTri.Vertices[0] = P0;
				NewTri.UVs[0][0] = FVector2D( ((P0 - TextureBase) | TextureX) / 128.0f, ((P0 - TextureBase) | TextureY) / 128.0f );
				NewTri.Vertices[1] = P1;
				NewTri.UVs[1][0] = FVector2D( ((P1 - TextureBase) | TextureX) / 128.0f, ((P1 - TextureBase) | TextureY) / 128.0f );
				NewTri.Vertices[2] = P2;		
				NewTri.UVs[2][0] = FVector2D( ((P2 - TextureBase) | TextureX) / 128.0f, ((P2 - TextureBase) | TextureY) / 128.0f );

				NewTri.MaterialIndex = MaterialIndex;
				// Lighting UVs can be generated via the static mesh viewer.
				NewTri.NumUVs = 1;
				
				Triangles.AddItem(NewTri);
			}
		}

		// Replace the static meshes' raw triangles with new ones.
		StaticMesh->LODModels(0).RawTriangles.RemoveBulkData();	
		StaticMesh->LODModels(0).RawTriangles.Lock(LOCK_READ_WRITE);
		void* RawTriangleData = StaticMesh->LODModels(0).RawTriangles.Realloc(Triangles.Num());
		check( StaticMesh->LODModels(0).RawTriangles.GetBulkDataSize() == Triangles.Num() * Triangles.GetTypeSize() );
		appMemcpy( RawTriangleData, Triangles.GetData(), StaticMesh->LODModels(0).RawTriangles.GetBulkDataSize() );
		StaticMesh->LODModels(0).RawTriangles.Unlock();

		// Last, but not least, build it!
		StaticMesh->Build();

		bProcessed = TRUE;
	}
	else if( ParseCommand(&Str,TEXT("AUTOMERGESM")) )
	{
		AutoMergeStaticMeshes();

		bProcessed = TRUE;
	}
	else if (ParseCommand(&Str, TEXT("ADDSELECTED")))
	{
		UBOOL bVisible = TRUE;
		FString OverrideGroup;
		FString VolumeName;
		if (Parse(Str, TEXT("GROUP="), OverrideGroup))
		{
			if (OverrideGroup.ToUpper() == TEXT("INVISIBLE"))
			{
				bVisible = FALSE;
			}
		}

		if (Parse(Str, TEXT("VOLUME="), VolumeName))
		{
			debugf(TEXT("Adding selected actors to %s group of PrecomputedVisibiltyOverrideVolume %s"), 
				bVisible ? TEXT(" VISIBLE ") : TEXT("INVISIBLE"), *VolumeName);

			APrecomputedVisibilityOverrideVolume* PrecompOverride = NULL;
			// Find the selected volume
			for (TObjectIterator<APrecomputedVisibilityOverrideVolume> VolumeIt; VolumeIt; ++VolumeIt)
			{
				APrecomputedVisibilityOverrideVolume* CheckPrecompOverride = *VolumeIt;
				if (CheckPrecompOverride->GetName() == VolumeName)
				{
					// Found the volume
					PrecompOverride = CheckPrecompOverride;
					break;
				}
			}

			if (PrecompOverride != NULL)
			{
				TArrayNoInit<class AActor*>* OverrideActorList = 
					bVisible ? &(PrecompOverride->OverrideVisibleActors) : &(PrecompOverride->OverrideInvisibleActors);
				// Grab a list of selected actors...
				for (FSelectionIterator ActorIt(GetSelectedActorIterator()) ; ActorIt; ++ActorIt)
				{
					AActor* Actor = static_cast<AActor*>(*ActorIt);
					checkSlow(Actor->IsA(AActor::StaticClass()));
					OverrideActorList->AddUniqueItem(Actor);
				}
			}
			else
			{
				warnf(NAME_Warning, TEXT("Unable to find PrecomputedVisibilityOverrideVolume %s"), *VolumeName);
			}
		}
		else
		{
			warnf(NAME_Warning, TEXT("Usage: ADDSELECTED GROUP=<VISIBLE/INVISIBLE> VOLUME=<Name of volume actor>"));
		}
	}
	else if (ParseCommand(&Str, TEXT("TOGGLESOCKETGMODE")))
	{
		GEditor->bDrawSocketsInGMode = !GEditor->bDrawSocketsInGMode;
		warnf(NAME_Log, TEXT("Draw sockets in 'G' mode is now %s"), GEditor->bDrawSocketsInGMode ? TEXT("ENABLED") : TEXT("DISABLED"));
	}
	else if (ParseCommand(&Str, TEXT("LISTMAPPKGDEPENDENCIES")))
	{
		EditorExecHelper_ListPkgDependencies(TRUE, Str);
	}
	else if (ParseCommand(&Str, TEXT("LISTSCRIPTPKGDEPENDENCIES")))
	{
		EditorExecHelper_ListPkgDependencies(FALSE, Str);
	}
	else if (ParseCommand(&Str, TEXT("DUMPMOBILEFLATTEN")))
	{
		EditorExecHelper_DumpMobileFlattenSettings();
	}

	return bProcessed;
}

UClass* UEditorEngine::GetClassFromPairMap( FString ClassName )
{
	UClass* ClassToUse = FindObject<UClass>(ANY_PACKAGE,*ClassName);
	for( INT i = 0; i < ClassMapPair.Num(); i++ )
	{
		const FString& Str = ClassMapPair(i);
		INT DelimIdx = Str.InStr(TEXT("/"));
		if( DelimIdx >= 0 )
		{
			const FString Key = Str.Left(DelimIdx);
			const FString Value = Str.Mid(DelimIdx+1);
			if( appStricmp( *ClassName, *Key ) == 0 )
			{
				ClassToUse = FindObject<UClass>(ANY_PACKAGE,*Value);
				break;
			}
		}
	}	
	return ClassToUse;
}

/**
 * @return TRUE if the given component's StaticMesh can be merged with other StaticMeshes
 */
UBOOL IsComponentMergable(UStaticMeshComponent* Component)
{
	// we need a component to work
	if (Component == NULL)
	{
		return FALSE;
	}

	// we need a static mesh to work
	if (Component->StaticMesh == NULL)
	{
		return FALSE;
	}

	// only components with a single LOD can be merged
	if (Component->StaticMesh->LODModels.Num() != 1)
	{
		return FALSE;
	}

	// only components with a single material can be merged
	INT NumSetElements = 0;
	for (INT ElementIndex = 0; ElementIndex < Component->GetNumElements(); ElementIndex++)
	{
		if (Component->GetMaterial(ElementIndex) != NULL)
		{
			NumSetElements++;
		}
	}

	if (NumSetElements > 1)
	{
		return FALSE;
	}

	return TRUE;
}

/**
 * Auto merge all staticmeshes that are able to be merged
 */
void UEditorEngine::AutoMergeStaticMeshes()
{
	TArray<AStaticMeshActor*> SMAs;
	for (FActorIterator It; It; ++It)
	{
		if (It->GetClass() == AStaticMeshActor::StaticClass())
		{
			SMAs.AddItem((AStaticMeshActor*)*It);
		}
	}

	// keep a mapping of actors and the other components that will be merged in to them
	TMap<AStaticMeshActor*, TArray<UStaticMeshComponent*> > ActorsToComponentForMergingMap;

	for (INT SMAIndex = 0; SMAIndex < SMAs.Num(); SMAIndex++)
	{
		AStaticMeshActor* SMA = SMAs(SMAIndex);
		UStaticMeshComponent* Component = SMAs(SMAIndex)->StaticMeshComponent;

		// can this component merge with others?
		UBOOL bCanBeMerged = IsComponentMergable(Component);

		// look for an already collected component to merge in to if I can be merged
		if (bCanBeMerged)
		{
			UMaterialInterface* Material = Component->GetMaterial(0);
			UObject* Outermost = SMA->GetOutermost();

			for (INT OtherSMAIndex = 0; OtherSMAIndex < SMAIndex; OtherSMAIndex++)
			{
				AStaticMeshActor* OtherSMA = SMAs(OtherSMAIndex);
				UStaticMeshComponent* OtherComponent = OtherSMA->StaticMeshComponent;

				// is this other mesh mergable?
				UBOOL bCanOtherBeMerged = IsComponentMergable(OtherComponent);

				// has this other mesh already been merged into another one? (after merging, DestroyActor
				// is called on it, setting bPendingDelete)
				UBOOL bHasAlreadyBeenMerged = OtherSMA->bPendingDelete == TRUE;

				// only look at this mesh if it can be merged and the actor hasn't already been merged
				if (bCanOtherBeMerged && !bHasAlreadyBeenMerged)
				{
					// do materials match?
					UBOOL bHasMatchingMaterials = Material == OtherComponent->GetMaterial(0);

					// we shouldn't go over 65535 verts so the index buffer can use 16 bit indices
					UBOOL bWouldResultingMeshBeSmallEnough = 
						(Component->StaticMesh->LODModels(0).NumVertices + 
						OtherComponent->StaticMesh->LODModels(0).NumVertices) < 65535;

					// make sure they are in the same level
					UBOOL bHasMatchingOutermost = Outermost == OtherSMA->GetOutermost();

					// now, determine compatibility between components/meshes
					if (bHasMatchingMaterials && bHasMatchingOutermost && bWouldResultingMeshBeSmallEnough)
					{
						// if these two can go together, collect the information for later merging
						TArray<UStaticMeshComponent*>* ComponentsForMerging = ActorsToComponentForMergingMap.Find(OtherSMA);
						if (ComponentsForMerging == NULL)
						{
							ComponentsForMerging = &ActorsToComponentForMergingMap.Set(OtherSMA, TArray<UStaticMeshComponent*>());
						}

						// @todo: Remove this limitation, and improve the lightmap UV packing below
						if (ComponentsForMerging->Num() == 16)
						{
							continue;
						}

						// add my component as a component to merge in to the other actor
						ComponentsForMerging->AddItem(Component);

						// and remove this actor from the world, it is no longer needed (it won't be deleted
						// until after this function returns, so it's safe to use it's components below)
						GWorld->DestroyActor(SMA);

						break;
					}
				}
			}
		}
	}

	// now that everything has been gathered, we can build some meshes!
	for (TMap<AStaticMeshActor*, TArray<UStaticMeshComponent*> >::TIterator It(ActorsToComponentForMergingMap); It; ++It)
	{
		AStaticMeshActor* OwnerActor = It.Key();
		TArray<UStaticMeshComponent*>& MergeComponents = It.Value();

		// get the component for the owner actor (its component is not in the TArray)
		UStaticMeshComponent* OwnerComponent = OwnerActor->StaticMeshComponent;

		// all lightmap UVs will go in to channel 1
		// @todo: This needs to look at the material and look for the smallest UV not used by the material
		INT LightmapUVChannel = 1;

		// first, create an empty mesh
		TArray<FStaticMeshTriangle> EmptyTris;
		UStaticMesh* NewStaticMesh = CreateStaticMesh(EmptyTris, OwnerComponent->StaticMesh->LODModels(0).Elements, OwnerActor->GetOutermost(), NAME_None);

		// set where the lightmap UVs come from
		NewStaticMesh->LightMapCoordinateIndex = LightmapUVChannel;

		// figure out how much to grow the lightmap resolution by, since it needs to be square, start by sqrt'ing the number
		INT LightmapMultiplier = appTrunc(appSqrt(MergeComponents.Num()));

		// increase the sqrt by 1 unless it was a perfect square
		if (LightmapMultiplier * LightmapMultiplier != MergeComponents.Num())
		{
			LightmapMultiplier++;
		}

		// cache the 1 over
		FLOAT InvLightmapMultiplier = 1.0f / (FLOAT)LightmapMultiplier;

		// look for the largest lightmap resolution
		INT MaxLightMapResolution = OwnerComponent->bOverrideLightMapRes ? OwnerComponent->OverriddenLightMapRes : OwnerComponent->StaticMesh->LightMapResolution;
		for (INT ComponentIndex = 0; ComponentIndex < MergeComponents.Num(); ComponentIndex++)
		{
			UStaticMeshComponent* Component = MergeComponents(ComponentIndex);
			MaxLightMapResolution = Max(MaxLightMapResolution,
				Component->bOverrideLightMapRes ? Component->OverriddenLightMapRes : Component->StaticMesh->LightMapResolution);
		}

		// clamp the multiplied res to 1024
		// @todo: maybe 2048? 
		INT LightmapRes = Min(1024, MaxLightMapResolution * LightmapMultiplier);

		// now, use the max resolution in the new mesh
		if (OwnerComponent->bOverrideLightMapRes)
		{
			OwnerComponent->OverriddenLightMapRes = LightmapRes;
		}
		else
		{
			NewStaticMesh->LightMapResolution = LightmapRes;
		}

		// set up the merge parameters
		FMergeStaticMeshParams Params;
		Params.bDeferBuild = TRUE;
		Params.OverrideElement = 0;
		Params.bUseUVChannelRemapping = TRUE;
		Params.UVChannelRemap[LightmapUVChannel] = OwnerComponent->StaticMesh->LightMapCoordinateIndex;
		Params.bUseUVScaleBias = TRUE;
		Params.UVScaleBias[LightmapUVChannel] = FVector4(InvLightmapMultiplier, InvLightmapMultiplier, 0.0f, 0.0f);

		// merge in to the empty mesh
		MergeStaticMesh(NewStaticMesh, OwnerComponent->StaticMesh, Params);

		// the component now uses this mesh
		// @todo: Is this needed? I think the Merge handles this
		{
			FComponentReattachContext ReattachContext(OwnerComponent);
			OwnerComponent->StaticMesh = NewStaticMesh;
		}

		// now merge all of the other component's meshes in to me
		for (INT ComponentIndex = 0; ComponentIndex < MergeComponents.Num(); ComponentIndex++)
		{
			UStaticMeshComponent* Component = MergeComponents(ComponentIndex);

			// calculate a matrix to go from my component space to the owner's component's space
			FMatrix TransformToOwnerSpace = Component->LocalToWorld * OwnerComponent->LocalToWorld.Inverse();

			// if we have negative scale, we need to munge the matrix and scaling
			if (TransformToOwnerSpace.Determinant() < 0.0f)
			{
				// get and remove the scale vector from the matrix
				Params.ScaleFactor3D = TransformToOwnerSpace.ExtractScaling();

				// negate X scale and top row of the matrix (will result in same transform, but then
				// MergeStaticMesh will fix the poly winding)
				Params.ScaleFactor3D.X = -Params.ScaleFactor3D.X;
				TransformToOwnerSpace.SetAxis(0, -TransformToOwnerSpace.GetAxis(0));
			}
			else
			{
				Params.ScaleFactor3D = TransformToOwnerSpace.GetScaleVector();
			}

			// now get the offset and rotation from the transform
			Params.Offset = TransformToOwnerSpace.GetOrigin();
			Params.Rotation = TransformToOwnerSpace.Rotator();

			// set the UV offset 
			INT XSlot = (ComponentIndex + 1) % LightmapMultiplier;
			INT YSlot = (ComponentIndex + 1) / LightmapMultiplier;
			Params.UVScaleBias[LightmapUVChannel].Z = (FLOAT)XSlot * InvLightmapMultiplier;
			Params.UVScaleBias[LightmapUVChannel].W = (FLOAT)YSlot * InvLightmapMultiplier;

			// route our lightmap UVs to the final lightmap channel
			Params.UVChannelRemap[LightmapUVChannel] = Component->StaticMesh->LightMapCoordinateIndex;

			// if compatible, merge them
			MergeStaticMesh(OwnerComponent->StaticMesh, Component->StaticMesh, Params);
		}

		// now that everything has been merged in, perform the slow build operation
		OwnerComponent->StaticMesh->Build();
	}
}
