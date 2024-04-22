/*=============================================================================
Landscape.cpp: New terrain rendering
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnTerrain.h"
#include "LandscapeDataAccess.h"
#include "LandscapeRender.h"
#include "LandscapeRenderMobile.h"

IMPLEMENT_CLASS(ALandscape);
IMPLEMENT_CLASS(ALandscapeProxy);
IMPLEMENT_CLASS(ULandscapeComponent);
IMPLEMENT_CLASS(ULandscapeMaterialInstanceConstant);
IMPLEMENT_CLASS(ULandscapeLayerInfoObject);
IMPLEMENT_CLASS(ULandscapeInfo);

//
// ULandscapeComponent
//
void ULandscapeComponent::AddReferencedObjects(TArray<UObject*>& ObjectArray)
{
	Super::AddReferencedObjects(ObjectArray);
	if(LightMap != NULL)
	{
		LightMap->AddReferencedObjects(ObjectArray);
	}
}

void ULandscapeComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.Ver() >= VER_LANDSCAPECOMPONENT_LIGHTMAPS)
	{
		Ar << LightMap;
	}

#if PS3 || MOBILE
	if( Ar.IsLoading() || Ar.IsCountingMemory() )
	{
		Ar << PlatformDataSize;
		if( PlatformDataSize )
		{
#if WITH_MOBILE_RHI
			// Discard data not needed due to LOD bias.
			if( Ar.IsLoading() )
			{
				// Calculate the amount of vertex buffer data to discard
				INT SkipSize = 0;	
				INT MipSubsectionSizeVerts = (SubsectionSizeQuads+1);
				for( INT MipIdx=0;MipIdx < GSystemSettings.MobileLandscapeLodBias && MipSubsectionSizeVerts > 1; MipIdx++ )
				{
					INT MipComponentSizeQuads = (MipSubsectionSizeVerts-1) * NumSubsections;
					INT MipComponentSizeVerts = MipComponentSizeQuads + 1;
					SkipSize += Square(MipComponentSizeVerts) * sizeof(FLandscapeMobileVertex);
					MipSubsectionSizeVerts >>= 1;
				}

				if( SkipSize > 0 )
				{
					void* SkipData = appMalloc(SkipSize);
					Ar.Serialize(SkipData, SkipSize);
					appFree(SkipData);
					PlatformDataSize -= SkipSize;
				}
			}
#endif
#if PS3
			PlatformData = appMalloc(PlatformDataSize, 16);
#else
			PlatformData = appMalloc(PlatformDataSize);
#endif
			Ar.Serialize(PlatformData, PlatformDataSize);
		}
	}
#endif

#if WITH_EDITOR
	if( Ar.IsSaving() && (GCookingTarget & (UE3::PLATFORM_PS3 | UE3::PLATFORM_IPhone | UE3::PLATFORM_Android)) )
	{
		Ar << PlatformDataSize;
		if( PlatformDataSize )
		{
			Ar.Serialize(PlatformData, PlatformDataSize);
		}
	}

	if ( Ar.IsTransacting() )
	{
		if (EditToolRenderData)
		{
			Ar << EditToolRenderData->SelectedType;
		}
		else
		{
			INT TempV = 0;
			Ar << TempV;
		}
	}
#endif
}

void ULandscapeComponent::SetElementMaterial(INT ElementIndex, UMaterialInterface* InMaterial)
{
	MaterialInstance = Cast<UMaterialInstanceConstant>(InMaterial);
	BeginDeferredReattach();
}


#if WITH_EDITOR
/**
 * Generate a key for this component's layer allocations to use with MaterialInstanceConstantMap.
 */

IMPLEMENT_COMPARE_CONSTREF( FString, Landscape, { return A < B ? 1 : -1; } );

UMaterialInterface* ULandscapeComponent::GetLandscapeMaterial() const
{
	if (OverrideMaterial)
	{
		return OverrideMaterial;
	}
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	if (Proxy)
	{
		return Proxy->GetLandscapeMaterial();
	}
	return GEngine->DefaultMaterial;
}

FString ULandscapeComponent::GetLayerAllocationKey() const
{
	UMaterialInterface* LandscapeMaterial = GetLandscapeMaterial();
	if (!LandscapeMaterial)
	{
		return FString("");
	}

	FString Result(*FString::Printf(TEXT("%d_"), LandscapeMaterial->GetIndex()) );

	// Sort the allocations
	TArray<FString> LayerStrings;
	for( INT LayerIdx=0;LayerIdx < WeightmapLayerAllocations.Num();LayerIdx++ )
	{
		new(LayerStrings) FString( *FString::Printf(TEXT("%s_%d_"), *WeightmapLayerAllocations(LayerIdx).LayerName.ToString(), WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex) );
	}
	Sort<USE_COMPARE_CONSTREF(FString,Landscape)>( &LayerStrings(0), LayerStrings.Num() );

	for( INT LayerIdx=0;LayerIdx < LayerStrings.Num();LayerIdx++ )
	{
		Result += LayerStrings(LayerIdx);
	}
	return Result;
}

void ULandscapeComponent::GetLayerDebugColorKey(INT& R, INT& G, INT& B) const
{
	ULandscapeInfo* Info = GetLandscapeInfo(FALSE);
	if (Info)
	{
		R = INDEX_NONE, G = INDEX_NONE, B = INDEX_NONE;

		for (TMap< FName, struct FLandscapeLayerStruct* >::TIterator It(Info->LayerInfoMap); It; ++It )
		{
			FLandscapeLayerStruct* LayerStruct = It.Value();
			if (LayerStruct && LayerStruct->DebugColorChannel > 0)
			{
				ULandscapeLayerInfoObject* LayerInfo = LayerStruct->LayerInfoObj;
				if (LayerInfo)
				{
					for( INT LayerIdx=0;LayerIdx < WeightmapLayerAllocations.Num();LayerIdx++ )	
					{
						if (WeightmapLayerAllocations(LayerIdx).LayerName == LayerInfo->LayerName)
						{
							if ( LayerStruct->DebugColorChannel & 1 ) // R
							{
								R = (WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex*4+WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel);
							}
							if ( LayerStruct->DebugColorChannel & 2 ) // G
							{
								G = (WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex*4+WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel);
							}
							if ( LayerStruct->DebugColorChannel & 4 ) // B
							{
								B = (WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex*4+WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel);
							}
							break;
						}
					}		
				}
			}
		}
	}
}

void ULandscapeInfo::UpdateDebugColorMaterial()
{
	FlushRenderingCommands();
	//GWarn->BeginSlowTask( *FString::Printf(TEXT("Compiling layer color combinations for %s"), *GetName()), TRUE);
	
	for (TMap<QWORD, ULandscapeComponent*>::TIterator It(XYtoComponentMap); It; ++It )
	{
		ULandscapeComponent* Comp = It.Value();
		if (Comp && Comp->EditToolRenderData)
		{
			Comp->EditToolRenderData->UpdateDebugColorMaterial();
		}
	}
	FlushRenderingCommands();
	//GWarn->EndSlowTask();
}

void ULandscapeComponent::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	//SetupActor();
	if( GIsEditor && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Remove standalone flags from data textures to ensure data is unloaded in the editor when reverting an unsaved level.
		// Previous version of landscape set these flags on creation.
		if( HeightmapTexture && HeightmapTexture->HasAnyFlags(RF_Standalone) )
		{
			HeightmapTexture->ClearFlags(RF_Standalone);
		}
		for( INT Idx=0;Idx<WeightmapTextures.Num();Idx++ )
		{
			if( WeightmapTextures(Idx) && WeightmapTextures(Idx)->HasAnyFlags(RF_Standalone) )
			{
				WeightmapTextures(Idx)->ClearFlags(RF_Standalone);
			}
		}

		if( !CachedLocalBox.IsValid )
		{
			ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();
			if( LandscapeProxy )
			{
				// Component isn't attached so we can't use its LocalToWorld
				FMatrix ComponentLtWTransform = FTranslationMatrix(FVector(SectionBaseX,SectionBaseY,0)) * LandscapeProxy->LocalToWorld();
				// This is good enough. The correct box will be calculated during painting.
				CachedLocalBox = CachedBoxSphereBounds.GetBox().TransformBy(ComponentLtWTransform.Inverse());
			}
		}
	}
#endif
}

void ULandscapeComponent::PostRename()
{
	Super::PostRename();
}

void ULandscapeComponent::PostEditImport()
{
	Super::PostEditImport();
}

#endif // WITH_EDITOR

ALandscape* ALandscape::GetLandscapeActor()
{
	return this;
}

ALandscape* ALandscapeProxy::GetLandscapeActor()
{
#if WITH_EDITORONLY_DATA
	return LandscapeActor;
#else
	return NULL;
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
ULandscapeInfo* ALandscapeProxy::GetLandscapeInfo(UBOOL bSpawnNewActor /*= TRUE*/)
{
	// LandscapeInfo generate
	if (GIsEditor && !GIsCooking && GWorld && GWorld->GetWorldInfo() && LandscapeGuid.IsValid() )
	{
		ULandscapeInfo* LandscapeInfo = GWorld->GetWorldInfo()->LandscapeInfoMap.FindRef(LandscapeGuid);

		if (!LandscapeInfo && bSpawnNewActor && !HasAnyFlags(RF_BeginDestroyed))
		{
			LandscapeInfo = ConstructObject<ULandscapeInfo>(ULandscapeInfo::StaticClass(), GWorld);
			if (LandscapeInfo)
			{
				LandscapeInfo->SetFlags(RF_Transactional);
				LandscapeInfo->GetSharedProperties(this);
				GWorld->GetWorldInfo()->LandscapeInfoMap.Set(LandscapeGuid, LandscapeInfo);
			}
		}
		return LandscapeInfo;
	}

	return NULL;
}
#endif

ALandscape* ULandscapeComponent::GetLandscapeActor() const
{
	ALandscapeProxy* Landscape = CastChecked<ALandscapeProxy>(GetOuter());
	if (Landscape)
	{
		return Landscape->GetLandscapeActor();
	}
	return NULL;
}

ALandscapeProxy* ULandscapeComponent::GetLandscapeProxy() const
{
	return CastChecked<ALandscapeProxy>(GetOuter());
}

#if WITH_EDITOR
ULandscapeInfo* ULandscapeComponent::GetLandscapeInfo(UBOOL bSpawnNewActor /*= TRUE*/) const
{
	if (GetLandscapeProxy())
	{
		return GetLandscapeProxy()->GetLandscapeInfo(bSpawnNewActor);
	}
	return NULL;
}
#endif

ALandscape* ULandscapeHeightfieldCollisionComponent::GetLandscapeActor() const
{
	ALandscapeProxy* Landscape = CastChecked<ALandscapeProxy>(GetOuter());
	if (Landscape)
	{
		return Landscape->GetLandscapeActor();
	}
	return NULL;
}

ALandscapeProxy* ULandscapeHeightfieldCollisionComponent::GetLandscapeProxy() const
{
	return CastChecked<ALandscapeProxy>(GetOuter());
}

#if WITH_EDITOR
ULandscapeInfo* ULandscapeHeightfieldCollisionComponent::GetLandscapeInfo(UBOOL bSpawnNewActor /*= TRUE*/) const
{
	if (GetLandscapeProxy())
	{
		return GetLandscapeProxy()->GetLandscapeInfo(bSpawnNewActor);
	}
	return NULL;
}
#endif

void ULandscapeComponent::BeginDestroy()
{
	Super::BeginDestroy();

#if PS3 || WITH_EDITOR
	if( PlatformData )
	{
		appFree(PlatformData);
		PlatformDataSize = 0;
	}
#endif

#if WITH_EDITOR
	if( GIsEditor && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		ULandscapeInfo* Info = GetLandscapeInfo(FALSE);
		ALandscapeProxy* Proxy = GetLandscapeProxy();
		
		if (Info)
		{
			Info->XYtoComponentMap.Remove(ALandscape::MakeKey(SectionBaseX,SectionBaseY));
			if (Info->SelectedComponents.Contains(this))
			{
				Info->SelectedComponents.Remove(Info->SelectedComponents.FindId(this));
			}

			// Remove any weightmap allocations from the Landscape Actor's map
			for( INT LayerIdx=0;LayerIdx < WeightmapLayerAllocations.Num();LayerIdx++ )
			{
				UTexture2D* WeightmapTexture = WeightmapTextures(WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex);
				FLandscapeWeightmapUsage* Usage = Proxy->WeightmapUsageMap.Find(WeightmapTexture);
				if( Usage != NULL )
				{
					Usage->ChannelUsage[WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel] = NULL;

					if( Usage->FreeChannelCount()==4 )
					{
						Proxy->WeightmapUsageMap.Remove(WeightmapTexture);
					}
				}
			}
		}
	}

	if( EditToolRenderData != NULL )
	{
		// Ask render thread to destroy EditToolRenderData
		EditToolRenderData->Cleanup();
		EditToolRenderData = NULL;
	}
#endif
}

FPrimitiveSceneProxy* ULandscapeComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;

	if( GUsingMobileRHI || GEmulateMobileRendering )
	{
		Proxy = new FLandscapeComponentSceneProxyMobile(this);
	}
	else
	{
	#if WITH_EDITOR
		if( EditToolRenderData == NULL )
		{
			EditToolRenderData = new FLandscapeEditToolRenderData(this);
		}
		Proxy = new FLandscapeComponentSceneProxy(this, EditToolRenderData);
	#else
		Proxy = new FLandscapeComponentSceneProxy(this, NULL);
	#endif
	}
	return Proxy;
}

void ULandscapeComponent::UpdateBounds()
{
	Bounds = CachedBoxSphereBounds;
#if !CONSOLE
	if ( !GIsGame && Scene->GetWorld() == GWorld )
	{
		ULevel::TriggerStreamingDataRebuild();
	}
#endif
}

void ULandscapeComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	Super::SetParentToWorld(FTranslationMatrix(FVector(SectionBaseX,SectionBaseY,0)) * ParentToWorld);
}

/** 
* Retrieves the materials used in this component 
* 
* @param OutMaterials	The list of used materials.
*/
void ULandscapeComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	if( MaterialInstance != NULL )
	{
		OutMaterials.AddItem(MaterialInstance);
	}
}


//
// ALandscape
//
void ALandscapeProxy::InitRBPhys()
{
#if WITH_NOVODEX
	if (!GWorld->RBPhysScene)
	{
		return;
	}
#endif // WITH_NOVODEX

	for(INT ComponentIndex = 0; ComponentIndex < CollisionComponents.Num(); ComponentIndex++ )
	{
		ULandscapeHeightfieldCollisionComponent* Comp = CollisionComponents(ComponentIndex);
		if( Comp && Comp->IsAttached() )
		{
			Comp->InitComponentRBPhys(TRUE);
		}
	}
}

void ALandscapeProxy::UpdateComponentsInternal(UBOOL bCollisionUpdate)
{
	Super::UpdateComponentsInternal(bCollisionUpdate);

	const FMatrix&	ActorToWorld = LocalToWorld();

	// Render components
	for(INT ComponentIndex = 0; ComponentIndex < LandscapeComponents.Num(); ComponentIndex++ )
	{
		ULandscapeComponent* Comp = LandscapeComponents(ComponentIndex);
		if( Comp )
		{
			Comp->UpdateComponent(GWorld->Scene,this,ActorToWorld);
		}
	}

	// Collision components
	for(INT ComponentIndex = 0; ComponentIndex < CollisionComponents.Num(); ComponentIndex++ )
	{
		ULandscapeHeightfieldCollisionComponent* Comp = CollisionComponents(ComponentIndex);
		if( Comp )
		{
			Comp->UpdateComponent(GWorld->Scene,this,ActorToWorld);
		}
	}
}

#if WITH_EDITOR
void ALandscape::UpdateComponentsInternal(UBOOL bCollisionUpdate)
{
	Super::UpdateComponentsInternal(bCollisionUpdate);
}
#endif

// FLandscapeWeightmapUsage serializer
FArchive& operator<<( FArchive& Ar, FLandscapeWeightmapUsage& U )
{
	return Ar << U.ChannelUsage[0] << U.ChannelUsage[1] << U.ChannelUsage[2] << U.ChannelUsage[3];
}

FArchive& operator<<( FArchive& Ar, FLandscapeAddCollision& U )
{
#if WITH_EDITORONLY_DATA
	return Ar << U.Corners[0] << U.Corners[1] << U.Corners[2] << U.Corners[3];
#else
	return Ar;
#endif // WITH_EDITORONLY_DATA
}

void ALandscape::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

FArchive& operator<<( FArchive& Ar, FLandscapeLayerStruct*& L )
{
	if (L)
	{
		Ar << L->LayerInfoObj;
#if WITH_EDITORONLY_DATA
		return Ar << L->ThumbnailMIC;
#else
		return Ar;
#endif // WITH_EDITORONLY_DATA
	}
	return Ar;
}

void ULandscapeInfo::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// We do not serialize XYtoComponentMap as we don't want these references to hold a component in memory.
	// The references are automatically cleaned up by the components' BeginDestroy method.
	if (Ar.IsTransacting())
	{
		Ar << XYtoComponentMap;
		Ar << XYtoCollisionComponentMap;
		Ar << XYtoAddCollisionMap;
		Ar << SelectedComponents;
		Ar << SelectedCollisionComponents;
		Ar << SelectedRegion;
		Ar << SelectedRegionComponents;
	}
}

void ALandscape::PostLoad()
{
	Super::PostLoad();
	if (!LandscapeGuid.IsValid())
	{
		LandscapeGuid = appCreateGuid();
	}

#if WITH_EDITOR
	if( GIsEditor )
	{
		if (GetLinker() && (GetLinker()->Ver() < VER_CHANGED_LANDSCAPE_MATERIAL_PARAMS))
		{
			GWarn->BeginSlowTask( TEXT("Updating Landscape material combinations"), TRUE);

			// Clear any RF_Standalone flag for material instance constants in the level package
			// So it can be GC'd when it's no longer used.
			UObject* Pkg = GetOutermost();
			for ( TObjectIterator<UMaterialInstanceConstant> It; It; ++It )
			{
				if( (*It)->GetOutermost() == Pkg )
				{
					(*It)->ClearFlags(RF_Standalone);

					// Clear out the parent for any old MIC's
					(*It)->SetParent(NULL);
				}
			}

			// Recreate MIC's for all components
			for(INT ComponentIndex = 0; ComponentIndex < LandscapeComponents.Num(); ComponentIndex++ )
			{
				GWarn->StatusUpdatef( ComponentIndex, LandscapeComponents.Num(), TEXT("Updating Landscape material combinations") );
				ULandscapeComponent* Comp = LandscapeComponents(ComponentIndex);
				if( Comp )
				{
					Comp->UpdateMaterialInstances();
				}
			}

#if WITH_EDITORONLY_DATA
			// Clear out the thumbnail MICs so they're recreated
			for (INT LayerIdx = 0; LayerIdx < LayerInfoObjs.Num(); LayerIdx++)
			{
				LayerInfoObjs(LayerIdx).ThumbnailMIC = NULL;
			}
#endif

			GWarn->EndSlowTask();
		}

		if (GetLinker() && (GetLinker()->Ver() < VER_LANDSCAPEDECALVERTEXFACTORY))
		{
			// Fixed up WeightmapScaleBias, etc...
			INT ComponentVerts = (SubsectionSizeQuads+1) * NumSubsections;
			for(INT ComponentIndex = 0; ComponentIndex < LandscapeComponents.Num(); ComponentIndex++ )
			{
				ULandscapeComponent* LandscapeComponent = LandscapeComponents(ComponentIndex);
				if( LandscapeComponent )
				{
					LandscapeComponent->WeightmapScaleBias = FVector4( 1.f / (FLOAT)ComponentVerts , 1.f / (FLOAT)ComponentVerts, 0.5f / (FLOAT)ComponentVerts , 0.5f / (FLOAT)ComponentVerts );
					LandscapeComponent->WeightmapSubsectionOffset =  (FLOAT)(LandscapeComponent->SubsectionSizeQuads+1) / (FLOAT)ComponentVerts ;
				}
			}
		}
	}
#endif
}

#if WITH_EDITOR
void ALandscape::UpdateOldLayerInfo()
{
	// For the LayerName legacy
	if (LayerInfos_DEPRECATED.Num())
	{
		for (INT Idx = 0; Idx < LayerInfos_DEPRECATED.Num(); Idx++)
		{
			ULandscapeLayerInfoObject* LayerInfo = GetLayerInfo(*LayerInfos_DEPRECATED(Idx).LayerName.ToString());
			if (LayerInfo)
			{
				LayerInfo->GetSharedProperties(&LayerInfos_DEPRECATED(Idx));
				LayerInfoObjs.Last().ThumbnailMIC = LayerInfos_DEPRECATED(Idx).ThumbnailMIC;
			}
		}
	}
	else if (LayerNames_DEPRECATED.Num())
	{
		for (INT Idx = 0; Idx < LayerNames_DEPRECATED.Num(); Idx++)
		{
			ULandscapeLayerInfoObject* LayerInfo = GetLayerInfo(*LayerNames_DEPRECATED(Idx).ToString());
		}
	}
}
#endif

void ALandscape::ClearComponents()
{
	Super::ClearComponents();
}

void ALandscapeProxy::ClearCrossLevelReferences()
{
	LandscapeActor = NULL;
#if WITH_EDITOR
	if (GIsEditor && !HasAnyFlags(RF_ClassDefaultObject))
	{
		// delay the reset of the material until after the save has finished
		//GEngine->DeferredCommands.AddUniqueItem(TEXT("RestoreLandscapeAfterSave"));
		GEngine->DeferredCommands.AddUniqueItem(TEXT("RestoreLandscapeLayerInfos"));
	}
#endif
}

#if WITH_EDITOR
void ALandscapeProxy::RestoreLandscapeAfterSave()
{
	if (GIsEditor && GWorld)
	{
		for (FActorIterator It; It; ++It)
		{
			ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
			if (Proxy && Proxy->bIsProxy)
			{
				Proxy->UpdateLandscapeActor(NULL);
			}
		}
	}
}
#endif

void ALandscape::BeginDestroy()
{
	Super::BeginDestroy();
}

void ULandscapeInfo::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	if( DataInterface )
	{
		delete DataInterface;
		DataInterface = NULL;
	}
#endif
}

#if WITH_EDITOR
/**
 * Function that gets called from within Map_Check to allow this actor to check itself
 * for any potential errors and register them with map check dialog.
 */
void ALandscape::CheckForErrors()
{
	if( !GWorld->GetWorldInfo()->bNoMobileMapWarnings )
	{
//		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString( LocalizeUnrealEd( TEXT( "MapCheck_Message_Landscape_Warning_MobileSupport" ) ) ), TEXT( "Landscape_Warning_MobileSupport" ), MCGROUP_MOBILEPLATFORM );
	}
}

#endif

void ALandscapeProxy::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if( !(Ar.IsLoading() || Ar.IsSaving()) )
	{
		Ar << MaterialInstanceConstantMap;
	}

#if WITH_EDITOR
	if( Ar.IsTransacting() )
	{
		Ar << WeightmapUsageMap;
	}
#endif
}

#if WITH_EDITOR
void ALandscapeProxy::PreEditUndo()
{
	Super::PreEditUndo();
	if (GIsEditor)
	{
		// Remove all layer info for this Proxy
		ULandscapeInfo* LandscapeInfo = GetLandscapeInfo(FALSE);
		if (LandscapeInfo)
		{
			LandscapeInfo->UpdateLayerInfoMap(this, TRUE);
		}
	}
}

void ALandscapeProxy::PostEditUndo()
{
	Super::PostEditUndo();
	if (GIsEditor)
	{
		GEngine->DeferredCommands.AddUniqueItem(TEXT("RestoreLandscapeLayerInfos"));
	}
}

void ULandscapeInfo::PostEditUndo()
{
	Super::PostEditUndo();
	if( GCallbackEvent )
	{
		// For Landscape List Update
		GCallbackEvent->Send( CALLBACK_WorldChange );
	}
}

void ULandscapeInfo::UpdateLODBias(FLOAT Threshold)
{
	for (TMap< QWORD, ULandscapeComponent* >::TIterator It(XYtoComponentMap); It; ++It)
	{
		ULandscapeComponent* Comp = It.Value();
		Comp->SetLOD(FALSE, Comp->GetLODBias(Threshold));
	}
}

UBOOL ULandscapeInfo::UpdateLayerInfoMap(ALandscapeProxy* Proxy /*= NULL*/, UBOOL bInvalidate /*= FALSE*/)
{
	UBOOL bHasCollision = FALSE;
	if (GIsEditor)
	{
		bIsValid = !bInvalidate;
		if( GCallbackEvent )
		{
			GCallbackEvent->Send( CALLBACK_EditorPreModal );
		}

		if (!Proxy)
		{
			LayerInfoMap.Empty();
		}

		if (Proxy)
		{
			for (int i = 0; i < Proxy->LayerInfoObjs.Num(); ++i)
			{
				ULandscapeLayerInfoObject* LayerInfo = Proxy->LayerInfoObjs(i).LayerInfoObj;
				if (LayerInfo)
				{
					// remove old LayerInfoStruct first...
					FLandscapeLayerStruct* LayerStruct = LayerInfoMap.FindRef(LayerInfo->LayerName);
					if ( LayerStruct && LayerStruct->Owner == Proxy )
					{
						LayerInfoMap.Remove(LayerInfo->LayerName);
					}

					if (!bInvalidate)
					{
						if ( !Proxy->bIsProxy || !LayerInfoMap.FindRef(LayerInfo->LayerName) )
						{
							LayerInfoMap.Set(LayerInfo->LayerName, &Proxy->LayerInfoObjs(i));
						}
					}
				}
			}
		}
		else if (!bInvalidate)
		{
			for (FActorIterator It; It; ++It)
			{
				ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
				if ( Proxy && LandscapeGuid == Proxy->LandscapeGuid )
				{
					// Merge Landscape Info
					for (int i = 0; i < Proxy->LayerInfoObjs.Num(); ++i)
					{
						ULandscapeLayerInfoObject* LayerInfo = Proxy->LayerInfoObjs(i).LayerInfoObj;
						if (LayerInfo)
						{
							FLandscapeLayerStruct* OldInfo = LayerInfoMap.FindRef(LayerInfo->LayerName);
							if (OldInfo && OldInfo->LayerInfoObj != LayerInfo)
							{
								// Collide with other LayerInfo
								bHasCollision = TRUE;
								GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("MapCheck_Message_LandscapeLayerInfo_CollisionWarning") ), *LayerInfo->GetName(), *LayerInfo->LayerName.ToString() )), TEXT( "LandscapeLayerInfo_CollisionWarning" ), MCGROUP_DEFAULT );
								// Landscape wins proxies... -0-;
								if (!Proxy->bIsProxy)
								{
									LayerInfoMap.Set(LayerInfo->LayerName, &Proxy->LayerInfoObjs(i));
								}
							}
							else
							{
								LayerInfoMap.Set(LayerInfo->LayerName, &Proxy->LayerInfoObjs(i));
							}
						}
					}
				}
			}
		}

		if (GCallbackEvent)
		{
			if (!bInvalidate)
			{
				GCallbackEvent->Send( CALLBACK_PostLandscapeLayerUpdated );
			}
			GCallbackEvent->Send( CALLBACK_EditorPostModal );
		}
	}
	return bHasCollision;
}
#endif

void ULandscapeInfo::PostLoad()
{
	Super::PostLoad();
}

void ALandscapeProxy::PostLoad()
{
	Super::PostLoad();

	// Temporary
	if( ComponentSizeQuads == 0 && LandscapeComponents.Num() > 0 )
	{
		ULandscapeComponent* Comp = LandscapeComponents(0);
		if( Comp )
		{
			ComponentSizeQuads = Comp->ComponentSizeQuads;
			SubsectionSizeQuads = Comp->SubsectionSizeQuads;	
			NumSubsections = Comp->NumSubsections;
		}
	}

	//UpdateLandscapeActor(LandscapeActor);
	//GetLandscapeInfo(); // Generate LandscapeInfo
#if WITH_EDITOR
	if (GIsEditor && !bIsSetup)
	{
		GEngine->DeferredCommands.AddUniqueItem(TEXT("UpdateLandscapeEditorData"));
	}
#endif
}

void ALandscapeProxy::BeginDestroy()
{
	Super::BeginDestroy();
}

void ALandscapeProxy::ClearComponents()
{
	// wait until resources are released
	FlushRenderingCommands();

	Super::ClearComponents();

	// Render components
	for(INT ComponentIndex = 0;ComponentIndex < LandscapeComponents.Num();ComponentIndex++)
	{
		ULandscapeComponent* Comp = LandscapeComponents(ComponentIndex);
		if (Comp)
		{
			Comp->ConditionalDetach();
		}
	}

	// Collision components
	for(INT ComponentIndex = 0; ComponentIndex < CollisionComponents.Num(); ComponentIndex++ )
	{
		ULandscapeHeightfieldCollisionComponent* Comp = CollisionComponents(ComponentIndex);
		if( Comp )
		{
			Comp->ConditionalDetach();
		}
	}
}

#if WITH_EDITOR
void ALandscapeProxy::PostScriptDestroyed()
{
	if (GIsEditor)
	{
		ULandscapeInfo* Info = GetLandscapeInfo(FALSE);
		if ( Info )
		{
			if (bIsProxy)
			{
				for (TSet<ALandscapeProxy*>::TIterator It(Info->Proxies); It; ++It )
				{
					if ((*It) == this)
					{
						It.RemoveCurrent();
						break;
					}
				}
			}

			if (Info->LandscapeProxy == this)
			{
				if (Info->Proxies.Num())
				{
					TSet<ALandscapeProxy*>::TIterator It(Info->Proxies);
					Info->LandscapeProxy = *It;
				}
				else if (GWorld && GWorld->GetWorldInfo()) // remove Info
				{
					GWorld->GetWorldInfo()->LandscapeInfoMap.RemoveKey(LandscapeGuid);
					if( GCallbackEvent )
					{
						// For Landscape List Update
						GCallbackEvent->Send( CALLBACK_WorldChange );
					}
				}
			}

			// Unregister for LayerInfoMap
			UBOOL bHasChanged = FALSE;
			for (int i = 0; i < LayerInfoObjs.Num(); ++i)
			{
				if (LayerInfoObjs(i).LayerInfoObj)
				{
					FLandscapeLayerStruct* Struct = Info->LayerInfoMap.FindRef(LayerInfoObjs(i).LayerInfoObj->LayerName);
					if (Struct && Struct->Owner == this)
					{
						Info->LayerInfoMap.RemoveKey(LayerInfoObjs(i).LayerInfoObj->LayerName);
						bHasChanged = TRUE;
					}
				}
			}

			Info->UpdateLayerInfoMap();
			if( bHasChanged && GCallbackEvent )
			{
				// For Landscape List Update
				GCallbackEvent->Send( CALLBACK_WorldChange );
			}
		}
	}
}
#endif

UBOOL ULandscapeLayerInfoObject::GetSharedProperties(FLandscapeLayerInfo* Info)
{
	if (Info)
	{
		LayerName = Info->LayerName;
		PhysMaterial = Info->PhysMaterial;
		Hardness = Clamp<FLOAT>(Info->Hardness, 0.f, 1.f);
		bNoWeightBlend = Info->bNoWeightBlend;
		return TRUE;
	}
	return FALSE;
}

#if WITH_EDITOR
void ALandscapeProxy::GetSharedProperties(ALandscape* Landscape)
{
	if (GIsEditor && Landscape)
	{
		if (!LandscapeGuid.IsValid())
		{
			LandscapeGuid = Landscape->LandscapeGuid;
		}
		Location = Landscape->Location;
		Rotation = Landscape->Rotation;
		PrePivot = Landscape->PrePivot;
		DrawScale = Landscape->DrawScale;
		DrawScale3D = Landscape->DrawScale3D;
		StaticLightingResolution = Landscape->StaticLightingResolution;
		ComponentSizeQuads = Landscape->ComponentSizeQuads;
		NumSubsections = Landscape->NumSubsections;
		SubsectionSizeQuads = Landscape->SubsectionSizeQuads;
		MaxLODLevel = Landscape->MaxLODLevel;
		if (!LandscapeMaterial)
		{
			LandscapeMaterial = Landscape->LandscapeMaterial;
		}
		if (!DefaultPhysMaterial)
		{
			DefaultPhysMaterial = Landscape->DefaultPhysMaterial;
		}
		CollisionMipLevel = Landscape->CollisionMipLevel;
		LightmassSettings = Landscape->LightmassSettings;
	}
}
#endif

void ULandscapeInfo::GetSharedProperties(ALandscapeProxy* Landscape)
{
	if (Landscape)
	{
		LandscapeGuid = Landscape->LandscapeGuid;
		LandscapeProxy = Landscape;
	}
}

#if WITH_EDITOR

UMaterialInterface* ALandscapeProxy::GetLandscapeMaterial() const
{
	if (LandscapeMaterial)
	{
		return LandscapeMaterial;
	}
	else if (LandscapeActor)
	{
		return LandscapeActor->GetLandscapeMaterial();
	}
	return GEngine->DefaultMaterial;
}

UMaterialInterface* ALandscape::GetLandscapeMaterial() const
{
	if (LandscapeMaterial)
	{
		return LandscapeMaterial;
	}
	return GEngine->DefaultMaterial;
}

void ULandscapeInfo::PreSave()
{
	Super::PreSave();
}

void ALandscapeProxy::PreSave()
{
	Super::PreSave();
	if (GIsEditor && bIsProxy)
	{
		if (!LandscapeActor)
		{
			UpdateLandscapeActor(NULL);
		}
		if (LandscapeActor)
		{
			GetSharedProperties(LandscapeActor);
			if (LandscapeMaterial == LandscapeActor->LandscapeMaterial)
			{
				UBOOL bNeedCopyLayerObject = FALSE;
				TArray<ULandscapeLayerInfoObject*> CopyLayerInfos;
				for (int i = 0; i < LandscapeActor->LayerInfoObjs.Num(); ++i)
				{
					UBOOL bFoundInLandscapeActor = FALSE;
					for (int j = 0; j < LayerInfoObjs.Num(); ++j)
					{
						if ( LandscapeActor->LayerInfoObjs(i).LayerInfoObj == LayerInfoObjs(j).LayerInfoObj )
						{
							bFoundInLandscapeActor = TRUE;
							break;
						}
					}
					if (!bFoundInLandscapeActor)
					{
						CopyLayerInfos.AddItem(LandscapeActor->LayerInfoObjs(i).LayerInfoObj);
					}
				}

				if (CopyLayerInfos.Num())
				{
					if (LandscapeActor != this)
					{
						for (int i = 0; i < CopyLayerInfos.Num(); ++i )
						{
							LayerInfoObjs.AddItem(FLandscapeLayerStruct(CopyLayerInfos(i), this, NULL));
							if (CopyLayerInfos(i) && CopyLayerInfos(i)->GetOutermost() == LandscapeActor->GetOutermost() && CopyLayerInfos(i)->GetOutermost() != LandscapeMaterial->GetOutermost())
							{
								CopyLayerInfos(i)->Rename(NULL, LandscapeMaterial->GetOutermost());
							}
						}
					}
				}
			}
		}
	}
}

void ALandscape::PreSave()
{
	Super::PreSave();
	ULandscapeInfo* Info = GetLandscapeInfo();
	if (GIsEditor && Info)
	{
		for (TSet<ALandscapeProxy*>::TIterator It(Info->Proxies); It; ++It)
		{
			ALandscapeProxy* Proxy = *It;
			Proxy->LandscapeActor = this;
			Proxy->GetSharedProperties(this);
		}
	}
}
#endif

