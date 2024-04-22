/*=============================================================================
LandscapeEdit.cpp: Landscape editing
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "UnTerrain.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeRender.h"
#include "LandscapeRenderMobile.h"
#include "LevelUtils.h"
#include "MaterialInstance.h"

FString ULandscapeMaterialInstanceConstant::LandscapeVisibilitySwitchName = TEXT("___LandscapeVisSwitch");
FName ALandscape::DataWeightmapName = FName(TEXT("__DataLayer__"));

#if WITH_EDITOR

//
// ULandscapeComponent
//
void ULandscapeComponent::Init(INT InBaseX,INT InBaseY,INT InComponentSizeQuads, INT InNumSubsections,INT InSubsectionSizeQuads)
{
	SectionBaseX = InBaseX;
	SectionBaseY = InBaseY;
	ComponentSizeQuads = InComponentSizeQuads;
	NumSubsections = InNumSubsections;
	SubsectionSizeQuads = InSubsectionSizeQuads;
	check(NumSubsections * SubsectionSizeQuads == ComponentSizeQuads);
	ULandscapeInfo* Info = GetLandscapeInfo();
}

INT ULandscapeComponent::GetLODBias(FLOAT InHeightThreshold)
{
	// Calculate automatic LODBias based on height map error-calculation
	INT MaxLOD = appCeilLogTwo(SubsectionSizeQuads+1)-1;
	INT OutLODBias = 0;

	// Height Threshold...
	FLOAT HeightThreshold = InHeightThreshold * LANDSCAPE_INV_ZSCALE; //64.f;
	FLandscapeComponentDataInterface Mip0CDI(this);

	for (INT LODLevel = 1; LODLevel <= MaxLOD; ++LODLevel)
	{
		INT MipAlignedSize = 1 << LODLevel;
		FLOAT HeightError = 0.f;
		FLandscapeComponentDataInterface CDI(this, LODLevel);

		for( INT y=0; y < ComponentSizeQuads + 1; y++ )
		{
			for( INT x=0; x < ComponentSizeQuads + 1; x++ )
			{
				// Calculate error based on height map data
				FLOAT X = (FLOAT)x / MipAlignedSize;
				FLOAT Y = (FLOAT)y / MipAlignedSize;

				INT MaxX = ((ComponentSizeQuads + 1) >> LODLevel) - 1;

				INT XX = Min<INT>(MaxX, appFloor(X));
				INT YY = Min<INT>(MaxX, appFloor(Y));

				FLOAT Height = Lerp( 
								Lerp( CDI.GetHeight(XX, YY), CDI.GetHeight(Min<INT>(MaxX, XX+1), YY), appFractional(X) )
								, Lerp( CDI.GetHeight(XX, Min<INT>(MaxX, YY+1)), CDI.GetHeight(Min<INT>(MaxX, XX+1), Min<INT>(MaxX, YY+1)), appFractional(X) )
								, appFractional(Y) );

				HeightError += Abs(Height - (FLOAT)Mip0CDI.GetHeight(x, y));
			}
		}

		if (HeightError < HeightThreshold * Square(ComponentSizeQuads + 1))
		{
			OutLODBias = LODLevel;
		}
		else
		{
			break;
		}
	}

	return OutLODBias;
}

void ULandscapeComponent::UpdateCachedBounds()
{
	FLandscapeComponentDataInterface CDI(this);

	// Update local-space bounding box
	CachedLocalBox.Init();
	for( INT y=0;y<ComponentSizeQuads+1;y++ )
	{
		for( INT x=0;x<ComponentSizeQuads+1;x++ )
		{
			CachedLocalBox += CDI.GetLocalVertex(x,y);
		}	
	}

	// Convert to world space
	CachedBoxSphereBounds = FBoxSphereBounds(CachedLocalBox.TransformBy(LocalToWorld));
	if( CachedBoxSphereBounds.BoxExtent.Z < 32.f )
	{
		CachedBoxSphereBounds.BoxExtent.Z = 32.f;
	}

	// Update collision component bounds
	ULandscapeInfo* Info = GetLandscapeInfo();
	if (Info)
	{
		QWORD ComponentKey = ALandscape::MakeKey(SectionBaseX,SectionBaseY);
		ULandscapeHeightfieldCollisionComponent* CollisionComponent = Info->XYtoCollisionComponentMap.FindRef(ComponentKey);
		if( CollisionComponent )
		{
			CollisionComponent->Modify();
			CollisionComponent->CachedBoxSphereBounds = CachedBoxSphereBounds;	
			CollisionComponent->ConditionalUpdateTransform();
		}
	}
}

void ULandscapeComponent::UpdateMaterialInstances()
{
	check(GIsEditor);

	ALandscapeProxy* Proxy = GetLandscapeProxy();
	
	UMaterialInterface* LandscapeMaterial = GetLandscapeMaterial();

	if( LandscapeMaterial != NULL )
	{
		FString LayerKey = GetLayerAllocationKey();
		// debugf(TEXT("Looking for key %s"), *LayerKey);

		// Find or set a matching MIC in the Landscape's map.
		UMaterialInstanceConstant* CombinationMaterialInstance = Proxy->MaterialInstanceConstantMap.FindRef(*LayerKey);
		if( CombinationMaterialInstance == NULL || CombinationMaterialInstance->Parent != LandscapeMaterial || GetOutermost() != CombinationMaterialInstance->GetOutermost() )
		{
			CombinationMaterialInstance = ConstructObject<ULandscapeMaterialInstanceConstant>(ULandscapeMaterialInstanceConstant::StaticClass(), GetOutermost(), NAME_None, RF_Public);
			debugf(TEXT("Looking for key %s, making new combination %s"), *LayerKey, *CombinationMaterialInstance->GetName());
			Proxy->MaterialInstanceConstantMap.Set(*LayerKey,CombinationMaterialInstance);
			UBOOL bNeedsRecompile;
			CombinationMaterialInstance->SetParent(LandscapeMaterial);

			UMaterial* ParentUMaterial = CombinationMaterialInstance->GetMaterial();

			UBOOL bHasDataWeight = FALSE;
			INT WeightmapIndex = INDEX_NONE;
			// Find data weightmap
			for( INT AllocIdx=0;AllocIdx<WeightmapLayerAllocations.Num();AllocIdx++ )
			{
				FWeightmapLayerAllocationInfo& Allocation = WeightmapLayerAllocations(AllocIdx);
				if ( Allocation.LayerName == ALandscape::DataWeightmapName )
				{
					ULandscapeMaterialInstanceConstant* LandscapeMIC = Cast<ULandscapeMaterialInstanceConstant>(CombinationMaterialInstance);
					if (LandscapeMIC)
					{
						LandscapeMIC->DataWeightmapIndex = Allocation.WeightmapTextureIndex;
						LandscapeMIC->DataWeightmapSize = (SubsectionSizeQuads+1) * NumSubsections;
						bHasDataWeight = TRUE;
						WeightmapIndex = Allocation.WeightmapTextureIndex;
					}
					break;
				}
			}

			BYTE OriginalBlendMode = ParentUMaterial->BlendMode;
			if( ParentUMaterial && ParentUMaterial != GEngine->DefaultMaterial )
			{
				FlushRenderingCommands();
				ParentUMaterial->SetMaterialUsage(bNeedsRecompile,MATUSAGE_Landscape);
				ParentUMaterial->SetMaterialUsage(bNeedsRecompile,MATUSAGE_StaticLighting);
			}

			FStaticParameterSet StaticParameters;
			CombinationMaterialInstance->GetStaticParameterValues(&StaticParameters);

			// Find weightmap mapping for each layer parameter, or disable if the layer is not used in this component.
			for( INT LayerParameterIdx=0;LayerParameterIdx<StaticParameters.TerrainLayerWeightParameters.Num();LayerParameterIdx++ )
			{
				FStaticTerrainLayerWeightParameter& LayerParameter = StaticParameters.TerrainLayerWeightParameters(LayerParameterIdx);
				LayerParameter.WeightmapIndex = INDEX_NONE;
				// Look through our allocations to see if we need this layer.
				// If not found, this component doesn't use the layer, and WeightmapIndex remains as INDEX_NONE.
				for( INT AllocIdx=0;AllocIdx<WeightmapLayerAllocations.Num();AllocIdx++ )
				{
					FWeightmapLayerAllocationInfo& Allocation = WeightmapLayerAllocations(AllocIdx);
					if( Allocation.LayerName == LayerParameter.ParameterName )
					{
						LayerParameter.WeightmapIndex = Allocation.WeightmapTextureIndex;
						LayerParameter.bOverride = TRUE;
						// debugf(TEXT(" Layer %s channel %d"), *LayerParameter.ParameterName.ToString(), LayerParameter.WeightmapIndex);
						break;
					}
				}
			}

			if (bHasDataWeight) 
			{
				StaticParameters.StaticSwitchParameters.AddItem( FStaticSwitchParameter( FName(*FString::Printf(TEXT("%s_%d"), *ULandscapeMaterialInstanceConstant::LandscapeVisibilitySwitchName, WeightmapIndex)), TRUE, bHasDataWeight, FGuid() ) );
			}

			if (CombinationMaterialInstance->SetStaticParameterValues(&StaticParameters))
			{
				//mark the package dirty if a compile was needed
				CombinationMaterialInstance->MarkPackageDirty();
			}

			CombinationMaterialInstance->InitResources();
			if (bHasDataWeight) 
			{
				CombinationMaterialInstance->AllocateStaticPermutations();
			}
			else
			{
				CombinationMaterialInstance->UpdateStaticPermutation(); // Compilation time
			}

			if (bHasDataWeight) 
			{
				for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
				{
					if (CombinationMaterialInstance->StaticPermutationResources[QualityIndex])
					{
						CombinationMaterialInstance->StaticPermutationResources[QualityIndex]->BlendModeOverrideValue = BLEND_Masked;
						CombinationMaterialInstance->StaticPermutationResources[QualityIndex]->bIsBlendModeOverrided = TRUE;
						CombinationMaterialInstance->StaticPermutationResources[QualityIndex]->bIsMaskedOverrideValue = TRUE; //ParentUMaterial->bIsMasked;
					}
				}

				CombinationMaterialInstance->UpdateStaticPermutation(); // Compilation time
			}
		}

		// Create the instance for this component, that will use the layer combination instance.
		if( MaterialInstance == NULL || GetOutermost() != MaterialInstance->GetOutermost() )
		{
			MaterialInstance = ConstructObject<ULandscapeMaterialInstanceConstant>(ULandscapeMaterialInstanceConstant::StaticClass(), GetOutermost(), NAME_None, RF_Public);
		}

		ULandscapeMaterialInstanceConstant* LandscapeMIC = Cast<ULandscapeMaterialInstanceConstant>(CombinationMaterialInstance);
		if (LandscapeMIC)
		{
			ULandscapeMaterialInstanceConstant* ChildMIC = Cast<ULandscapeMaterialInstanceConstant>(MaterialInstance);
			if( ChildMIC )
			{
				ChildMIC->DataWeightmapIndex = LandscapeMIC->DataWeightmapIndex;
				ChildMIC->DataWeightmapSize = LandscapeMIC->DataWeightmapSize;
			}
		}

		// For undo
		MaterialInstance->SetFlags(RF_Transactional);
		MaterialInstance->Modify();

		MaterialInstance->SetParent(CombinationMaterialInstance);

		FLinearColor Masks[4];
		Masks[0] = FLinearColor(1.f,0.f,0.f,0.f);
		Masks[1] = FLinearColor(0.f,1.f,0.f,0.f);
		Masks[2] = FLinearColor(0.f,0.f,1.f,0.f);
		Masks[3] = FLinearColor(0.f,0.f,0.f,1.f);

		// Set the layer mask
		for( INT AllocIdx=0;AllocIdx<WeightmapLayerAllocations.Num();AllocIdx++ )
		{
			FWeightmapLayerAllocationInfo& Allocation = WeightmapLayerAllocations(AllocIdx);
			MaterialInstance->SetVectorParameterValue(FName(*FString::Printf(TEXT("LayerMask_%s"),*Allocation.LayerName.ToString())), Masks[Allocation.WeightmapTextureChannel]);
		}

		// Set the weightmaps
		for( INT i=0;i<WeightmapTextures.Num();i++ )
		{
			// debugf(TEXT("Setting Weightmap%d = %s"), i, *WeightmapTextures(i)->GetName());
			MaterialInstance->SetTextureParameterValue(FName(*FString::Printf(TEXT("Weightmap%d"),i)), WeightmapTextures(i));
		}
		// Set the heightmap, if needed.
		MaterialInstance->SetTextureParameterValue(FName(TEXT("Heightmap")), HeightmapTexture);
	}
}

/** Called after an Undo action occurs */
void ULandscapeComponent::PostEditUndo()
{
	Super::PostEditUndo();
	if (!bNeedPostUndo)
	{
		ULandscapeInfo* Info = GetLandscapeInfo(FALSE);
		if (Info && !Info->bIsValid)
		{
			bNeedPostUndo = TRUE;
			if (EditToolRenderData)
			{
				EditToolRenderData->DebugChannelR = INDEX_NONE;
				EditToolRenderData->DebugChannelG = INDEX_NONE;
				EditToolRenderData->DebugChannelB = INDEX_NONE;
			}
			return;
		}
	}

	UpdateMaterialInstances();
	if (EditToolRenderData)
	{
		EditToolRenderData->UpdateDebugColorMaterial();
		EditToolRenderData->UpdateSelectionMaterial(EditToolRenderData->SelectedType);
	}

	if (HeightmapTexture && HeightmapTexture->bHasBeenLoadedFromPersistentArchive)
	{
		HeightmapTexture->bHasBeenLoadedFromPersistentArchive = FALSE;
		HeightmapTexture->UpdateResource();
	}

	for (int i = 0; i < WeightmapTextures.Num(); ++i)
	{
		if (WeightmapTextures(i) && WeightmapTextures(i)->bHasBeenLoadedFromPersistentArchive)
		{
			WeightmapTextures(i)->bHasBeenLoadedFromPersistentArchive = FALSE;
			WeightmapTextures(i)->UpdateResource();
		}
	}
}

ELandscapeSetupErrors ULandscapeComponent::SetupActor(UBOOL bForce /*= FALSE*/)
{
	if( GIsEditor && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		ULandscapeInfo* Info = GetLandscapeInfo();
		ALandscapeProxy* Proxy = GetLandscapeProxy();

		if (Info)
		{
			QWORD LandscapeKey = ALandscape::MakeKey(SectionBaseX,SectionBaseY);
			ULandscapeComponent* Comp = Info->XYtoComponentMap.FindRef(LandscapeKey);
			if (Comp && Comp != this && !bForce)
			{
				return LSE_CollsionXY;
			}
			// Store the components in the map
			Info->XYtoComponentMap.Set(LandscapeKey, this);

			if (bForce) 
			{
				return LSE_None;
			}

			TArray<FName> DeletedLayers;
			UBOOL bFixedLayerDeletion = FALSE;

			if (Info->LayerInfoMap.Num() && Cast<ALandscape>(Info->LandscapeProxy))
			{
				// LayerName Validation check...
				for( INT LayerIdx=0;LayerIdx < WeightmapLayerAllocations.Num();LayerIdx++ )
				{
					if ( WeightmapLayerAllocations(LayerIdx).LayerName != ALandscape::DataWeightmapName && Info->LayerInfoMap.FindRef(WeightmapLayerAllocations(LayerIdx).LayerName) == NULL )
					{
						if(!bFixedLayerDeletion )
						{
							GWarn->MapCheck_Add( MCTYPE_WARNING, NULL, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_FixedUpDeletedLayerWeightmap" ), *GetName() ) ), TEXT( "FixedUpDeletedLayerWeightmap" ) );
							bFixedLayerDeletion = TRUE;
						}
						DeletedLayers.AddItem(WeightmapLayerAllocations(LayerIdx).LayerName);
					}
				}
			}

			if (bFixedLayerDeletion)
			{
				FLandscapeEditDataInterface LandscapeEdit(Info);
				for (INT Idx = 0; Idx < DeletedLayers.Num(); ++Idx)
				{
					DeleteLayer(DeletedLayers(Idx), &LandscapeEdit);
				}
			}

			UBOOL bFixedWeightmapTextureIndex=FALSE;

			// Store the weightmap allocations in WeightmapUsageMap
			for( INT LayerIdx=0;LayerIdx < WeightmapLayerAllocations.Num();LayerIdx++ )
			{
				FWeightmapLayerAllocationInfo& Allocation = WeightmapLayerAllocations(LayerIdx);

				// Fix up any problems caused by the layer deletion bug.
				if( Allocation.WeightmapTextureIndex >= WeightmapTextures.Num() )
				{
					Allocation.WeightmapTextureIndex = WeightmapTextures.Num()-1; 
					if( !bFixedWeightmapTextureIndex )
					{
						GWarn->MapCheck_Add( MCTYPE_WARNING, NULL, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_FixedUpIncorrectLayerWeightmap" ), *GetName() ) ), TEXT( "FixedUpIncorrectLayerWeightmap" ) );
						bFixedWeightmapTextureIndex = TRUE;
					}
				}

				UTexture2D* WeightmapTexture = WeightmapTextures(Allocation.WeightmapTextureIndex);
				FLandscapeWeightmapUsage& Usage = Proxy->WeightmapUsageMap.FindOrAdd(WeightmapTexture);

				// Detect a shared layer allocation, caused by a previous undo or layer deletion bugs
				if( Usage.ChannelUsage[Allocation.WeightmapTextureChannel] != NULL )
				{
					GWarn->MapCheck_Add( MCTYPE_WARNING, Proxy, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_FixedUpSharedLayerWeightmap" ), *Allocation.LayerName.ToString(), *GetName(), *Usage.ChannelUsage[Allocation.WeightmapTextureChannel]->GetName() ) ), TEXT( "FixedUpSharedLayerWeightmap" ) );
					WeightmapLayerAllocations.Remove(LayerIdx);
					LayerIdx--;
					continue;
				}
				else
				{
					Usage.ChannelUsage[Allocation.WeightmapTextureChannel] = this;
				}
			}

			RemoveInvalidWeightmaps();

			// Store the layer combination in the MaterialInstanceConstantMap
			if( MaterialInstance != NULL )
			{
				UMaterialInstanceConstant* CombinationMaterialInstance = Cast<UMaterialInstanceConstant>(MaterialInstance->Parent);
				if( CombinationMaterialInstance )
				{
					Proxy->MaterialInstanceConstantMap.Set(*GetLayerAllocationKey(),CombinationMaterialInstance);
				}
			}

			return LSE_None;
		}
		return LSE_NoLandscapeInfo;
	}
	return LSE_None;
}

void ULandscapeComponent::GeneratePlatformData( UE3::EPlatformType Platform, void*& NewPlatformData, INT& NewPlatformDataSize, UTexture2D*& NewWeightTexture ) const
{
	NewPlatformData = NULL;
	NewPlatformDataSize = 0;
	NewWeightTexture = NULL;

	if (Platform & (UE3::PLATFORM_IPhone|UE3::PLATFORM_Android) )
	{
		check(HeightmapTexture);
		check(HeightmapTexture->Format == PF_A8R8G8B8);

		INT MaxLOD = appCeilLogTwo(SubsectionSizeQuads+1)-1;
		INT MipBias = Min<INT>(GetLandscapeProxy()->MobileLODBias, MaxLOD);
		
		// Make sure the shaders are enabled for mobile landscape
		UMaterial* ParentUMaterial = GetLandscapeProxy()->LandscapeMaterial ? GetLandscapeProxy()->LandscapeMaterial->GetMaterial() : NULL;
		if( ParentUMaterial )
		{
			UBOOL bNeedsRecompile;
			ParentUMaterial->SetMaterialUsage(bNeedsRecompile,MATUSAGE_MobileLandscape);
		}

		FLOAT HeightmapSubsectionOffsetU = (FLOAT)(SubsectionSizeQuads+1) / (FLOAT)HeightmapTexture->SizeX;
		FLOAT HeightmapSubsectionOffsetV = (FLOAT)(SubsectionSizeQuads+1) / (FLOAT)HeightmapTexture->SizeY;

		// Create alphamap texture
		INT AlphaMapSizeX = 1<<appCeilLogTwo(SubsectionSizeQuads * NumSubsections + 1);
		NewWeightTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), GetOutermost(), NAME_None, RF_Public);
		NewWeightTexture->Init(AlphaMapSizeX,AlphaMapSizeX,PF_A8R8G8B8);
		NewWeightTexture->SRGB = FALSE;
		NewWeightTexture->CompressionNone = TRUE;
		NewWeightTexture->MipGenSettings = TMGS_SimpleAverage;
		NewWeightTexture->LODGroup = TEXTUREGROUP_World;
		NewWeightTexture->AddressX = TA_Clamp;
		NewWeightTexture->AddressY = TA_Clamp;
		NewWeightTexture->Mips.Empty();

	
		// Find weightmap and color channel mappings.
		INT ChannelMapping[4] = {-1,-1,-1,-1};
		INT ChannelWeightmap[4] = {0,0,0,0};

		TArray<FLandscapeLayerStruct>& LayerInfoLayers = GetLandscapeProxy()->LayerInfoObjs;
		for( INT LayerIdx=0; LayerIdx<4; LayerIdx++ )
		{
			FName LayerName = NAME_None;
			
			if( ParentUMaterial && ParentUMaterial->MobileLandscapeLayerNames[0] != NAME_None )
			{
				// Use the names specified in material's MobileLandscapeLayerNames setting.
				// Only checked if the first MobileLandscapeLayerNames array item has been set.
				LayerName = ParentUMaterial->MobileLandscapeLayerNames[LayerIdx];
			}
			else
			if( LayerInfoLayers.IsValidIndex(LayerIdx) && LayerInfoLayers(LayerIdx).LayerInfoObj != NULL )
			{
				// Use the layer name of the nth layer index specified
				// (ie, the order specified in the Landscape editor)
				LayerName = LayerInfoLayers(LayerIdx).LayerInfoObj->LayerName;
			}


			if( LayerName != NAME_None)
			{
				// Find the weightmap allocation for the layer
				for( INT AllocIdx=0;AllocIdx<WeightmapLayerAllocations.Num();AllocIdx++ )
				{
					if( LayerName == WeightmapLayerAllocations(AllocIdx).LayerName )
					{
						ChannelWeightmap[LayerIdx] = WeightmapLayerAllocations(AllocIdx).WeightmapTextureIndex;
						switch( WeightmapLayerAllocations(AllocIdx).WeightmapTextureChannel )
						{
						case 3:
							ChannelMapping[LayerIdx] = STRUCT_OFFSET(FColor,A);
							break;
						case 2:
							ChannelMapping[LayerIdx] = STRUCT_OFFSET(FColor,B);
							break;
						case 1:
							ChannelMapping[LayerIdx] = STRUCT_OFFSET(FColor,G);
							break;
						default:
							ChannelMapping[LayerIdx] = STRUCT_OFFSET(FColor,R);
							break;
						}
						break;
					}
				}
			}
		}
		FColor TempColor(255,0,0,0);

		NewPlatformDataSize = 0;
		TArray<FColor*> HeightmapMipData;

		// Struct to hold weightmap data
		struct FWeightmapInfo
		{
			TArray<FColor*> WeightMipData;
			UTexture2D* WeightmapTexture;

			FWeightmapInfo(UTexture2D* InWeightmapTexture)
			:	WeightmapTexture(InWeightmapTexture)
			{
				check(WeightmapTexture);

				for( INT MipIdx=0;MipIdx<WeightmapTexture->Mips.Num();MipIdx++ )
				{
					WeightMipData.AddItem( (FColor*)WeightmapTexture->Mips(MipIdx).Data.Lock(LOCK_READ_ONLY) );
				}
			}

			~FWeightmapInfo()
			{
				for( INT MipIdx=0;MipIdx<WeightmapTexture->Mips.Num();MipIdx++ )
				{
					WeightmapTexture->Mips(MipIdx).Data.Unlock();
				}
			}
		};

		TArray<FWeightmapInfo> WeightmapInfos;
		for( INT WeightmapIdx=0;WeightmapIdx<WeightmapTextures.Num();WeightmapIdx++ )
		{
			new(WeightmapInfos) FWeightmapInfo(WeightmapTextures(WeightmapIdx));
		}

		for( INT MipIdx=0;MipIdx<HeightmapTexture->Mips.Num();MipIdx++ )
		{
			HeightmapMipData.AddItem( (FColor*)HeightmapTexture->Mips(MipIdx).Data.Lock(LOCK_READ_ONLY) );

			INT MipSubsectionSizeVerts = (SubsectionSizeQuads+1) >> MipIdx;
			if( MipSubsectionSizeVerts > 1 && MipIdx >= MipBias )
			{
				INT MipComponentSizeQuads = (MipSubsectionSizeVerts-1) * NumSubsections;
				NewPlatformDataSize += sizeof(FLandscapeMobileVertex) * Square(MipComponentSizeQuads+1);
			}
		}
		NewPlatformData = appMalloc(NewPlatformDataSize);
		FLandscapeMobileVertex* DstVert = (FLandscapeMobileVertex*)NewPlatformData;

		INT Mip = 0;
		INT MipSizeX = HeightmapTexture->SizeX;
		INT MipSubsectionSizeVerts = SubsectionSizeQuads+1;
		while( MipSubsectionSizeVerts > 1 )
		{
			FTexture2DMipMap* NewWeightMip =NULL;	FColor* NewWeightData = NULL;
			if( Mip == 0 )
			{
				NewWeightMip = new(NewWeightTexture->Mips) FTexture2DMipMap;
				NewWeightMip->SizeX = AlphaMapSizeX;
				NewWeightMip->SizeY = AlphaMapSizeX;
				NewWeightMip->Data.Lock(LOCK_READ_WRITE);
				NewWeightData = (FColor*)NewWeightMip->Data.Realloc(Square(AlphaMapSizeX)*sizeof(FColor));
				appMemzero( NewWeightData, Square(AlphaMapSizeX)*sizeof(FColor) );
			}

			INT MipSubsectionSizeQuads = MipSubsectionSizeVerts-1;
			INT NextMipSubsectionSizeVerts = (MipSubsectionSizeVerts>>1);
			INT NextMipSubsectionSizeQuads = NextMipSubsectionSizeVerts-1;

			// Map current to next base and mip 
			FLOAT BaseMipRatio = (FLOAT)SubsectionSizeQuads / (FLOAT)MipSubsectionSizeQuads;
			FLOAT NextMipRatio = (FLOAT)NextMipSubsectionSizeQuads / (FLOAT)MipSubsectionSizeQuads;

			// Copy the data
			for( INT SubsectionY = 0;SubsectionY < NumSubsections;SubsectionY++ )
			{
				INT LastY = SubsectionY==NumSubsections-1 ? MipSubsectionSizeVerts : MipSubsectionSizeVerts - 1;
				for( INT Y=0;Y<LastY;Y++ )
				{
					for( INT SubsectionX = 0;SubsectionX < NumSubsections;SubsectionX++ )
					{
						FLOAT HeightmapScaleBiasZ = HeightmapScaleBias.Z + HeightmapSubsectionOffsetU * (FLOAT)SubsectionX;
						FLOAT HeightmapScaleBiasW = HeightmapScaleBias.W + HeightmapSubsectionOffsetV * (FLOAT)SubsectionY; 
						INT BaseMipOfsX = appRound(HeightmapScaleBiasZ * (FLOAT)HeightmapTexture->SizeX);
						INT BaseMipOfsY = appRound(HeightmapScaleBiasW * (FLOAT)HeightmapTexture->SizeY);
						INT CurrentMipOfsX = BaseMipOfsX >> Mip;
						INT CurrentMipOfsY = BaseMipOfsY >> Mip;
						INT NextMipOfsX = BaseMipOfsX >> (Mip+1);
						INT NextMipOfsY = BaseMipOfsY >> (Mip+1);				

						INT BaseMipY = appRound( (FLOAT)Y * BaseMipRatio );
						INT NextMipY = appRound( (FLOAT)Y * NextMipRatio );

						FColor* BaseMipSrcRow = HeightmapMipData(0) + (BaseMipOfsY + BaseMipY) * HeightmapTexture->SizeX + BaseMipOfsX;
						FColor* CurrentMipSrcRow = HeightmapMipData(Mip) + (CurrentMipOfsY + Y) * MipSizeX + CurrentMipOfsX;
						FColor* NextMipSrcRow = Mip+1 < HeightmapMipData.Num() ? HeightmapMipData(Mip+1) + (NextMipOfsY + NextMipY) * (MipSizeX>>1) + NextMipOfsX : NULL;

						TArray<FColor*> WeightMipSrcRows;
						for( INT WeightmapIdx=0;WeightmapIdx<WeightmapInfos.Num();WeightmapIdx++)
						{
							WeightMipSrcRows.AddItem(WeightmapInfos(WeightmapIdx).WeightMipData(Mip) + (Y + SubsectionY * MipSubsectionSizeVerts) * (WeightmapInfos(WeightmapIdx).WeightmapTexture->SizeX>>Mip) + (SubsectionX * MipSubsectionSizeVerts));
						}
						
						FColor* NewWeightRow = NewWeightData ? NewWeightData + (Y + SubsectionY * MipSubsectionSizeQuads) * (AlphaMapSizeX) + (SubsectionX * MipSubsectionSizeQuads) : NULL;

						INT LastX = SubsectionX==NumSubsections-1 ? MipSubsectionSizeVerts : MipSubsectionSizeVerts - 1;
						for( INT X=0;X<LastX;X++ )
						{
							if( Mip >= MipBias)
							{
								// Copy the height data
								DstVert->Position[0] = X + SubsectionX * (MipSubsectionSizeVerts-1);
								DstVert->Position[1] = Y + SubsectionY * (MipSubsectionSizeVerts-1);
								DstVert->Position[2] = CurrentMipSrcRow[X].G;
								DstVert->Position[3] = CurrentMipSrcRow[X].R;

								INT BaseMipX = appRound( (FLOAT)X * BaseMipRatio );
								FLOAT NormalX = ((FLOAT)(BaseMipSrcRow[BaseMipX].B) - 127.5f) / 127.5f;
								FLOAT NormalY = ((FLOAT)(BaseMipSrcRow[BaseMipX].A) - 127.5f) / 127.5f;
								DstVert->Normal = FVector(NormalX, NormalY, appSqrt( Max<FLOAT>(1.f-(Square(NormalX) + Square(NormalY)), 0.f)));					

								INT NextMipX = appRound( (FLOAT)X * NextMipRatio );

								// Morph position in next LOD.
								if( NextMipSrcRow )
								{
									DstVert->NextMipPosition[0] = NextMipX + SubsectionX * (NextMipSubsectionSizeVerts-1);
									DstVert->NextMipPosition[1] = NextMipY + SubsectionY * (NextMipSubsectionSizeVerts-1);
									DstVert->NextMipPosition[2] = NextMipSrcRow[NextMipX].G;
									DstVert->NextMipPosition[3] = NextMipSrcRow[NextMipX].R;
								}
								else
								{
									DstVert->NextMipPosition[0] = 0;
									DstVert->NextMipPosition[1] = 0;
									DstVert->NextMipPosition[2] = 0;
									DstVert->NextMipPosition[3] = 0;
								}

								DstVert++;
							}
							
							// Copy weight data
							if( NewWeightRow )
							{
								NewWeightRow[X].R = ChannelMapping[0] >= 0 ? ((BYTE*)(&WeightMipSrcRows(ChannelWeightmap[0])[X]))[ChannelMapping[0]] : 0;
								NewWeightRow[X].G = ChannelMapping[1] >= 0 ? ((BYTE*)(&WeightMipSrcRows(ChannelWeightmap[1])[X]))[ChannelMapping[1]] : 0;
								NewWeightRow[X].B = ChannelMapping[2] >= 0 ? ((BYTE*)(&WeightMipSrcRows(ChannelWeightmap[2])[X]))[ChannelMapping[2]] : 0;
								NewWeightRow[X].A = ChannelMapping[3] >= 0 ? ((BYTE*)(&WeightMipSrcRows(ChannelWeightmap[3])[X]))[ChannelMapping[3]] : 0;
								if( Platform != UE3::PLATFORM_IPhone && !GEmulateMobileRendering )
								{
									// Swap byte order for GL_RGBA - see comment in GES2PixelFormats.
									Exchange(NewWeightRow[X].R, NewWeightRow[X].B);
								}
							}
						}
					}
				}
			}

			if( Mip==0 )
			{
				INT MipSizeVerts = (MipSubsectionSizeVerts-1) * NumSubsections + 1;

				// tile out the last row and column for the new weight texture
				for( INT y=0;y<MipSizeVerts;y++ )
				{
					FColor WeightClampValue = NewWeightData[y * AlphaMapSizeX + MipSizeVerts-1];
					for( INT x=MipSizeVerts;x < AlphaMapSizeX; x++ )
					{
						NewWeightData[y * AlphaMapSizeX + x] = WeightClampValue;
					}
				}
				for( INT x=0;x<AlphaMapSizeX;x++ )
				{
					FColor WeightClampValue = NewWeightData[(MipSizeVerts-1) * AlphaMapSizeX + x];
					for( INT y=MipSizeVerts;y < AlphaMapSizeX; y++ )
					{
						NewWeightData[y * AlphaMapSizeX + x] = WeightClampValue;
					}
				}	
				NewWeightMip->Data.Unlock();
			}

			Mip++;
			MipSubsectionSizeVerts >>= 1;
			MipSizeX >>= 1;
			AlphaMapSizeX >>= 1;
		}

		
#if !MOBILE && WITH_MOBILE_RHI
		// Don't create mips when using the mobile RHI on PC as they render black for some unknown reason
		if( !GUsingMobileRHI )
#endif
		{
			NewWeightTexture->Compress();
		}

		for( INT MipIdx=0;MipIdx<HeightmapTexture->Mips.Num();MipIdx++ )
		{
			HeightmapTexture->Mips(MipIdx).Data.Unlock();
		}
	}
	else
	if (Platform == UE3::PLATFORM_PS3)
	{
		struct LodInfo
		{
			DWORD Offset;
			DWORD Stride;
		};

		TArray<LodInfo> LodInfos;
		TArray<WORD> Heights;
		check(HeightmapTexture);
		check(HeightmapTexture->Format == PF_A8R8G8B8);

		FLOAT HeightmapSubsectionOffsetU = (FLOAT)(SubsectionSizeQuads+1) / (FLOAT)HeightmapTexture->SizeX;
		FLOAT HeightmapSubsectionOffsetV = (FLOAT)(SubsectionSizeQuads+1) / (FLOAT)HeightmapTexture->SizeY;

		INT MipSizeX = HeightmapTexture->SizeX;
		INT Mip = 0;
		INT MipSubsectionSizeVerts = SubsectionSizeQuads+1;
		while( MipSubsectionSizeVerts > 1 )
		{
			// Calculate padding to ensure each row is a multiple of 16.
			INT Padding = 0;
			while( (((MipSubsectionSizeVerts + Padding) * sizeof(WORD)) & 0xF) )
			{
				Padding++;
			}

			// Update LODInfo;
			LodInfos.AddZeroed();
			LodInfos(Mip).Offset = Heights.Num() * sizeof(WORD);
			LodInfos(Mip).Stride = (MipSubsectionSizeVerts + Padding) * sizeof(WORD);

			FColor* SrcData = (FColor*)HeightmapTexture->Mips(Mip).Data.Lock(LOCK_READ_ONLY);

			// For each mip, copy the height data
			for( INT SubsectionY = 0;SubsectionY < NumSubsections;SubsectionY++ )
			{
				for( INT SubsectionX = 0;SubsectionX < NumSubsections;SubsectionX++ )
				{
					WORD* DstData = &Heights(Heights.AddZeroed((MipSubsectionSizeVerts + Padding) * MipSubsectionSizeVerts));

					FLOAT HeightmapScaleBiasZ = HeightmapScaleBias.Z + HeightmapSubsectionOffsetU * (FLOAT)SubsectionX;
					FLOAT HeightmapScaleBiasW = HeightmapScaleBias.W + HeightmapSubsectionOffsetV * (FLOAT)SubsectionY; 
					INT BaseMipOfsX = appRound(HeightmapScaleBiasZ * (FLOAT)HeightmapTexture->SizeX);
					INT BaseMipOfsY = appRound(HeightmapScaleBiasW * (FLOAT)HeightmapTexture->SizeY);
					INT CurrentMipOfsX = BaseMipOfsX >> Mip;
					INT CurrentMipOfsY = BaseMipOfsY >> Mip;

					FColor* SrcRow = SrcData + CurrentMipOfsY * MipSizeX + CurrentMipOfsX;
					for( INT Y=0;Y<MipSubsectionSizeVerts;Y++ )
					{
						for( INT X=0;X<MipSubsectionSizeVerts;X++ )
						{
							*DstData++ = (WORD)SrcRow[X].G << 8 | SrcRow[X].R;
						}
						DstData += Padding;
						SrcRow += MipSizeX;
					}

				}
			}

			HeightmapTexture->Mips(Mip).Data.Unlock();

			Mip++;
			MipSubsectionSizeVerts >>= 1;
			MipSizeX >>= 1;
		}

		INT LodInfoSize = (LodInfos.Num() * sizeof(LodInfo) + 15) & (~0xF);
		NewPlatformDataSize = Heights.Num() * sizeof(WORD) + LodInfoSize;
		NewPlatformData = appMalloc(NewPlatformDataSize);
		appMemzero(NewPlatformData, NewPlatformDataSize);

		DWORD* LodInfoData = (DWORD*)NewPlatformData;
		for( INT i=0;i<LodInfos.Num();i++ )
		{
			DWORD Offset = LodInfos(i).Offset + LodInfoSize;
			DWORD Stride = LodInfos(i).Stride;

			// byteswap offset
			*LodInfoData++ = ((Offset & 0x000000FF) << 24) |
				((Offset & 0x0000FF00) <<  8) |
				((Offset & 0x00FF0000) >>  8) |
				((Offset & 0xFF000000) >> 24) ;

			// byteswap stride
			*LodInfoData++ = ((Stride & 0x000000FF) << 24) |
				((Stride & 0x0000FF00) <<  8) |
				((Stride & 0x00FF0000) >>  8) |
				((Stride & 0xFF000000) >> 24) ;
		}

		appMemcpy((BYTE*)NewPlatformData + LodInfoSize, &Heights(0), Heights.Num() * sizeof(WORD));
	}
}

//
// LandscapeComponentAlphaInfo
//
struct FLandscapeComponentAlphaInfo
{
	INT LayerIndex;
	TArrayNoInit<BYTE> AlphaValues;

	// tor
	FLandscapeComponentAlphaInfo( ULandscapeComponent* InOwner, INT InLayerIndex )
	:	LayerIndex(InLayerIndex)
	,	AlphaValues(E_ForceInit)
	{
		AlphaValues.Empty(Square(InOwner->ComponentSizeQuads+1));
		AlphaValues.AddZeroed(Square(InOwner->ComponentSizeQuads+1));
	}

	UBOOL IsLayerAllZero() const
	{
		for( INT Index=0;Index<AlphaValues.Num();Index++ )
		{
			if( AlphaValues(Index) != 0 )
			{
				return FALSE;
			}
		}
		return TRUE;
	}
};

/**
 * Creates or updates collision component height data
 * @param HeightmapTextureMipData: heightmap data
 * @param ComponentX1, ComponentY1, ComponentX2, ComponentY2: region to update
 * @param Whether to update bounds from render component.
 */
void ULandscapeComponent::UpdateCollisionHeightData(FColor* HeightmapTextureMipData, INT ComponentX1, INT ComponentY1, INT ComponentX2, INT ComponentY2, UBOOL bUpdateBounds, UBOOL bRebuild )
{
	ULandscapeInfo* Info = GetLandscapeInfo();
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	QWORD ComponentKey = ALandscape::MakeKey(SectionBaseX,SectionBaseY);
	ULandscapeHeightfieldCollisionComponent* CollisionComponent = Info ? Info->XYtoCollisionComponentMap.FindRef(ComponentKey) : NULL;

	ALandscapeProxy* CollisionProxy = NULL;
	if (CollisionComponent && bRebuild)
	{
		// Remove existing component
		Info->XYtoCollisionComponentMap.Remove(ComponentKey);
		CollisionProxy = CollisionComponent->GetLandscapeProxy();
		if (CollisionProxy)
		{
			CollisionProxy->CollisionComponents.RemoveItem(CollisionComponent);
			CollisionComponent->ConditionalDetach();
			CollisionComponent = NULL;
		}
	}

	INT CollisionSubsectionSizeVerts = ((SubsectionSizeQuads+1)>>CollisionMipLevel);
	INT CollisionSubsectionSizeQuads = CollisionSubsectionSizeVerts-1;
	INT CollisionSizeVerts = NumSubsections*CollisionSubsectionSizeQuads+1;

	WORD* CollisionHeightData = NULL;
	UBOOL CreatedNew = FALSE;
	if( CollisionComponent )
	{
		CollisionComponent->Modify();
		if( bUpdateBounds )
		{
			CollisionComponent->CachedBoxSphereBounds = CachedBoxSphereBounds;	
			CollisionComponent->ConditionalUpdateTransform();
		}

		CollisionHeightData = (WORD*)CollisionComponent->CollisionHeightData.Lock(LOCK_READ_WRITE);
	}
	else
	{
		ComponentX1 = 0;
		ComponentY1 = 0;
		ComponentX2 = MAXINT;
		ComponentY2 = MAXINT;

		CollisionComponent = ConstructObject<ULandscapeHeightfieldCollisionComponent>(ULandscapeHeightfieldCollisionComponent::StaticClass(), Proxy,NAME_None,RF_Transactional);
		Proxy->CollisionComponents.AddItem(CollisionComponent);
		if (Info)
		{
			Info->XYtoCollisionComponentMap.Set(ComponentKey, CollisionComponent);
		}
		else
		{
			this->GetLandscapeProxy()->bResetup = TRUE;
			GEngine->DeferredCommands.AddUniqueItem(TEXT("UpdateLandscapeEditorData"));
		}

		CollisionComponent->SectionBaseX = SectionBaseX;
		CollisionComponent->SectionBaseY = SectionBaseY;
		CollisionComponent->CollisionSizeQuads = CollisionSubsectionSizeQuads * NumSubsections;
		CollisionComponent->CollisionScale = (FLOAT)(ComponentSizeQuads) / (FLOAT)(CollisionComponent->CollisionSizeQuads);
		CollisionComponent->CachedBoxSphereBounds = CachedBoxSphereBounds;
		CreatedNew = TRUE;

		// Reallocate raw collision data
		CollisionComponent->CollisionHeightData.Lock(LOCK_READ_WRITE);
		CollisionHeightData = (WORD*)CollisionComponent->CollisionHeightData.Realloc(Square(CollisionSizeVerts));
		appMemzero( CollisionHeightData,sizeof(WORD)*Square(CollisionSizeVerts));
	}

	INT HeightmapSizeU = HeightmapTexture->SizeX;
	INT HeightmapSizeV = HeightmapTexture->SizeY;
	INT MipSizeU = HeightmapSizeU >> CollisionMipLevel;
	INT MipSizeV = HeightmapSizeV >> CollisionMipLevel;

	// Ratio to convert update region coordinate to collision mip coordinates
	FLOAT CollisionQuadRatio = (FLOAT)CollisionSubsectionSizeQuads / (FLOAT)SubsectionSizeQuads;

	// XY offset into heightmap mip data
	INT HeightmapOffsetX = appRound(HeightmapScaleBias.Z * (FLOAT)HeightmapSizeU) >> CollisionMipLevel;
	INT HeightmapOffsetY = appRound(HeightmapScaleBias.W * (FLOAT)HeightmapSizeV) >> CollisionMipLevel;

	for( INT SubsectionY = 0;SubsectionY < NumSubsections;SubsectionY++ )
	{
		// Check if subsection is fully above or below the area we are interested in
		if( (ComponentY2 < SubsectionSizeQuads*SubsectionY) ||		// above
			(ComponentY1 > SubsectionSizeQuads*(SubsectionY+1)) )	// below
		{
			continue;
		}

		for( INT SubsectionX = 0;SubsectionX < NumSubsections;SubsectionX++ )
		{
			// Check if subsection is fully to the left or right of the area we are interested in
			if( (ComponentX2 < SubsectionSizeQuads*SubsectionX) ||		// left
				(ComponentX1 > SubsectionSizeQuads*(SubsectionX+1)) )	// right
			{
				continue;
			}

			// Area to update in subsection coordinates
			INT SubX1 = ComponentX1 - SubsectionSizeQuads*SubsectionX;
			INT SubY1 = ComponentY1 - SubsectionSizeQuads*SubsectionY;
			INT SubX2 = ComponentX2 - SubsectionSizeQuads*SubsectionX;
			INT SubY2 = ComponentY2 - SubsectionSizeQuads*SubsectionY;

			// Area to update in collision mip level coords
			INT CollisionSubX1 = appFloor( (FLOAT)SubX1 * CollisionQuadRatio );
			INT CollisionSubY1 = appFloor( (FLOAT)SubY1 * CollisionQuadRatio );
			INT CollisionSubX2 = appCeil(  (FLOAT)SubX2 * CollisionQuadRatio );
			INT CollisionSubY2 = appCeil(  (FLOAT)SubY2 * CollisionQuadRatio );

			// Clamp area to update
			INT VertX1 = Clamp<INT>(CollisionSubX1, 0, CollisionSubsectionSizeQuads);
			INT VertY1 = Clamp<INT>(CollisionSubY1, 0, CollisionSubsectionSizeQuads);
			INT VertX2 = Clamp<INT>(CollisionSubX2, 0, CollisionSubsectionSizeQuads);
			INT VertY2 = Clamp<INT>(CollisionSubY2, 0, CollisionSubsectionSizeQuads);

			for( INT VertY=VertY1;VertY<=VertY2;VertY++ )
			{
				for( INT VertX=VertX1;VertX<=VertX2;VertX++ )
				{
					// X/Y of the vertex we're looking indexed into the texture data
					INT TexX = HeightmapOffsetX + CollisionSubsectionSizeVerts * SubsectionX + VertX;
					INT TexY = HeightmapOffsetY + CollisionSubsectionSizeVerts * SubsectionY + VertY;
					FColor& TexData = HeightmapTextureMipData[ TexX + TexY * MipSizeU ];
			
					// this uses Quads as we don't want the duplicated vertices
					INT CompVertX = CollisionSubsectionSizeQuads * SubsectionX + VertX;
					INT CompVertY = CollisionSubsectionSizeQuads * SubsectionY + VertY;

					// Copy collision data
					WORD& CollisionHeight = CollisionHeightData[CompVertX+CompVertY*CollisionSizeVerts];
					WORD NewHeight = TexData.R<<8 | TexData.G;
					
					CollisionHeight = NewHeight;
				}
			}
		}
	}
	CollisionComponent->CollisionHeightData.Unlock();

	// If we updated an existing component, we need to reinitialize the physics
	if( !CreatedNew )
	{
		CollisionComponent->RecreateHeightfield();
	}

	if ( bRebuild && CollisionProxy )
	{
		CollisionProxy->ConditionalUpdateComponents();
		CollisionProxy->InitRBPhysEditor();
	}
}

/**
 * Updates collision component dominant layer data
 * @param WeightmapTextureMipData: weightmap data
 * @param ComponentX1, ComponentY1, ComponentX2, ComponentY2: region to update
 * @param Whether to update bounds from render component.
 */
void ULandscapeComponent::UpdateCollisionLayerData(TArray<FColor*>& WeightmapTextureMipData, INT ComponentX1, INT ComponentY1, INT ComponentX2, INT ComponentY2 )
{
	ULandscapeInfo* Info = GetLandscapeInfo();
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	QWORD ComponentKey = ALandscape::MakeKey(SectionBaseX,SectionBaseY);
	ULandscapeHeightfieldCollisionComponent* CollisionComponent = Info->XYtoCollisionComponentMap.FindRef(ComponentKey);

	if( CollisionComponent )
	{
		CollisionComponent->Modify();

		TArray<FName> CandidateLayers;
		TArray<BYTE*> CandidateDataPtrs;

		// Channel remapping
		INT ChannelOffsets[4] = {STRUCT_OFFSET(FColor,R),STRUCT_OFFSET(FColor,G),STRUCT_OFFSET(FColor,B),STRUCT_OFFSET(FColor,A)};

		UBOOL bExistingLayerMismatch = FALSE;
		INT DataLayerIdx = INDEX_NONE;

		// Find the layers we're interested in
		for( INT AllocIdx=0;AllocIdx<WeightmapLayerAllocations.Num();AllocIdx++ )
		{
			FWeightmapLayerAllocationInfo& AllocInfo = WeightmapLayerAllocations(AllocIdx);
			FLandscapeLayerStruct* LayerInfo = Info->LayerInfoMap.FindRef(AllocInfo.LayerName);
			if( AllocInfo.LayerName == ALandscape::DataWeightmapName || (LayerInfo && LayerInfo->LayerInfoObj && LayerInfo->LayerInfoObj->PhysMaterial) )
			{
				INT Idx = CandidateLayers.AddItem(AllocInfo.LayerName);
				CandidateDataPtrs.AddItem(((BYTE*)WeightmapTextureMipData(AllocInfo.WeightmapTextureIndex)) + ChannelOffsets[AllocInfo.WeightmapTextureChannel]);

				// Check if we still match the collision component.
				if( Idx >= CollisionComponent->ComponentLayers.Num() || CollisionComponent->ComponentLayers(Idx) != AllocInfo.LayerName )
				{
					bExistingLayerMismatch = TRUE;
				}

				if (AllocInfo.LayerName == ALandscape::DataWeightmapName)
				{
					DataLayerIdx = Idx;
					CollisionComponent->bIncludeHoles = TRUE;
					bExistingLayerMismatch = TRUE; // always rebuild whole component for hole
				}
			}	
		}

		if( CandidateLayers.Num() == 0 )
		{
			// No layers, so don't update any weights
			CollisionComponent->DominantLayerData.RemoveBulkData();
			CollisionComponent->ComponentLayers.Empty();
		}
		else
		{
			INT CollisionSubsectionSizeVerts = ((SubsectionSizeQuads+1)>>CollisionMipLevel);
			INT CollisionSubsectionSizeQuads = CollisionSubsectionSizeVerts-1;
			INT CollisionSizeVerts = NumSubsections*CollisionSubsectionSizeQuads+1;
			BYTE* DominantLayerData = NULL;

			// If there's no existing data, or the layer allocations have changed, we need to update the data for the whole component.
			if( bExistingLayerMismatch || CollisionComponent->DominantLayerData.GetElementCount() == 0  )
			{
				ComponentX1 = 0;
				ComponentY1 = 0;
				ComponentX2 = MAXINT;
				ComponentY2 = MAXINT;

				CollisionComponent->DominantLayerData.Lock(LOCK_READ_WRITE);
				DominantLayerData = (BYTE*)CollisionComponent->DominantLayerData.Realloc(Square(CollisionSizeVerts));
				appMemzero(DominantLayerData,Square(CollisionSizeVerts));

				CollisionComponent->ComponentLayers = CandidateLayers;
			}
			else
			{
				DominantLayerData = (BYTE*)CollisionComponent->DominantLayerData.Lock(LOCK_READ_WRITE);
			}

			INT MipSizeU = (WeightmapTextures(0)->SizeX) >> CollisionMipLevel;

			// Ratio to convert update region coordinate to collision mip coordinates
			FLOAT CollisionQuadRatio = (FLOAT)CollisionSubsectionSizeQuads / (FLOAT)SubsectionSizeQuads;

			for( INT SubsectionY = 0;SubsectionY < NumSubsections;SubsectionY++ )
			{
				// Check if subsection is fully above or below the area we are interested in
				if( (ComponentY2 < SubsectionSizeQuads*SubsectionY) ||		// above
					(ComponentY1 > SubsectionSizeQuads*(SubsectionY+1)) )	// below
				{
					continue;
				}

				for( INT SubsectionX = 0;SubsectionX < NumSubsections;SubsectionX++ )
				{
					// Check if subsection is fully to the left or right of the area we are interested in
					if( (ComponentX2 < SubsectionSizeQuads*SubsectionX) ||		// left
						(ComponentX1 > SubsectionSizeQuads*(SubsectionX+1)) )	// right
					{
						continue;
					}

					// Area to update in subsection coordinates
					INT SubX1 = ComponentX1 - SubsectionSizeQuads*SubsectionX;
					INT SubY1 = ComponentY1 - SubsectionSizeQuads*SubsectionY;
					INT SubX2 = ComponentX2 - SubsectionSizeQuads*SubsectionX;
					INT SubY2 = ComponentY2 - SubsectionSizeQuads*SubsectionY;

					// Area to update in collision mip level coords
					INT CollisionSubX1 = appFloor( (FLOAT)SubX1 * CollisionQuadRatio );
					INT CollisionSubY1 = appFloor( (FLOAT)SubY1 * CollisionQuadRatio );
					INT CollisionSubX2 = appCeil(  (FLOAT)SubX2 * CollisionQuadRatio );
					INT CollisionSubY2 = appCeil(  (FLOAT)SubY2 * CollisionQuadRatio );

					// Clamp area to update
					INT VertX1 = Clamp<INT>(CollisionSubX1, 0, CollisionSubsectionSizeQuads);
					INT VertY1 = Clamp<INT>(CollisionSubY1, 0, CollisionSubsectionSizeQuads);
					INT VertX2 = Clamp<INT>(CollisionSubX2, 0, CollisionSubsectionSizeQuads);
					INT VertY2 = Clamp<INT>(CollisionSubY2, 0, CollisionSubsectionSizeQuads);

					for( INT VertY=VertY1;VertY<=VertY2;VertY++ )
					{
						for( INT VertX=VertX1;VertX<=VertX2;VertX++ )
						{
							// X/Y of the vertex we're looking indexed into the texture data
							INT TexX = CollisionSubsectionSizeVerts * SubsectionX + VertX;
							INT TexY = CollisionSubsectionSizeVerts * SubsectionY + VertY;
							INT DataOffset = (TexX + TexY * MipSizeU) * sizeof(FColor);
					
							INT DominantLayer = 255; // 255 as invalid value
							INT DominantWeight = 0;
							for( INT LayerIdx=0;LayerIdx<CandidateDataPtrs.Num();LayerIdx++ )
							{
								BYTE LayerWeight = CandidateDataPtrs(LayerIdx)[DataOffset];

								if (LayerIdx == DataLayerIdx) // Override value for hole
								{
									if (LayerWeight > 170) // 255 * 0.66
									{
										DominantLayer = LayerIdx;
										DominantWeight = INT_MAX;
									}
									else
									{
										DominantLayer = 255;
										DominantWeight = 0;
									}
								}
								else if( LayerWeight > DominantWeight )
								{
									DominantLayer = LayerIdx;
									DominantWeight = LayerWeight;
								}
							}

							// this uses Quads as we don't want the duplicated vertices
							INT CompVertX = CollisionSubsectionSizeQuads * SubsectionX + VertX;
							INT CompVertY = CollisionSubsectionSizeQuads * SubsectionY + VertY;

							// Set collision data
							DominantLayerData[CompVertX+CompVertY*CollisionSizeVerts] = DominantLayer;
						}
					}
				}
			}
			CollisionComponent->DominantLayerData.Unlock();
		}

		// We do not force an update of the physics data here. We don't need the layer information in the editor and it
		// causes problems if we update it multiple times in a single frame.
	}
}

/**
 * Updates collision component dominant layer data for the whole component, locking and unlocking the weightmap textures.
 */
void ULandscapeComponent::UpdateCollisionLayerData()
{
	// Generate the dominant layer data
	TArray<FColor*> WeightmapTextureMipData;
	for( INT Idx=0;Idx<WeightmapTextures.Num();Idx++ )
	{
		WeightmapTextureMipData.AddItem( (FColor*)WeightmapTextures(Idx)->Mips(CollisionMipLevel).Data.Lock(LOCK_READ_ONLY) );
	}

	UpdateCollisionLayerData(WeightmapTextureMipData);

	for( INT Idx=0;Idx<WeightmapTextures.Num();Idx++ )
	{
		WeightmapTextures(Idx)->Mips(CollisionMipLevel).Data.Unlock();
	}
}



/**
* Generate mipmaps for height and tangent data.
* @param HeightmapTextureMipData - array of pointers to the locked mip data. 
*           This should only include the mips that are generated directly from this component's data
*           ie where each subsection has at least 2 vertices.
* @param ComponentX1 - region of texture to update in component space, MAXINT meant end of X component in ALandscape::Import()
* @param ComponentY1 - region of texture to update in component space, MAXINT meant end of Y component in ALandscape::Import()
* @param ComponentX2 (optional) - region of texture to update in component space
* @param ComponentY2 (optional) - region of texture to update in component space
* @param TextureDataInfo - FLandscapeTextureDataInfo pointer, to notify of the mip data region updated.
*/
void ULandscapeComponent::GenerateHeightmapMips( TArray<FColor*>& HeightmapTextureMipData, INT ComponentX1/*=0*/, INT ComponentY1/*=0*/, INT ComponentX2/*=MAXINT*/, INT ComponentY2/*=MAXINT*/, struct FLandscapeTextureDataInfo* TextureDataInfo/*=NULL*/ )
{
	UBOOL EndX = FALSE; 
	UBOOL EndY = FALSE;

	if (ComponentX1 == MAXINT)
	{
		EndX = TRUE;
		ComponentX1 = 0;
	}

	if (ComponentY1 == MAXINT)
	{
		EndY = TRUE;
		ComponentY1 = 0;
	}

	if( ComponentX2==MAXINT )
	{
		ComponentX2 = ComponentSizeQuads;
	}
	if( ComponentY2==MAXINT )
	{
		ComponentY2 = ComponentSizeQuads;
	}

	INT HeightmapSizeU = HeightmapTexture->SizeX;
	INT HeightmapSizeV = HeightmapTexture->SizeY;

	INT HeightmapOffsetX = appRound(HeightmapScaleBias.Z * (FLOAT)HeightmapSizeU);
	INT HeightmapOffsetY = appRound(HeightmapScaleBias.W * (FLOAT)HeightmapSizeV);

	for( INT SubsectionY = 0;SubsectionY < NumSubsections;SubsectionY++ )
	{
		// Check if subsection is fully above or below the area we are interested in
		if( (ComponentY2 < SubsectionSizeQuads*SubsectionY) ||		// above
			(ComponentY1 > SubsectionSizeQuads*(SubsectionY+1)) )	// below
		{
			continue;
		}

		for( INT SubsectionX = 0;SubsectionX < NumSubsections;SubsectionX++ )
		{
			// Check if subsection is fully to the left or right of the area we are interested in
			if( (ComponentX2 < SubsectionSizeQuads*SubsectionX) ||		// left
				(ComponentX1 > SubsectionSizeQuads*(SubsectionX+1)) )	// right
			{
				continue;
			}

			// Area to update in previous mip level coords
			INT PrevMipSubX1 = ComponentX1 - SubsectionSizeQuads*SubsectionX;
			INT PrevMipSubY1 = ComponentY1 - SubsectionSizeQuads*SubsectionY;
			INT PrevMipSubX2 = ComponentX2 - SubsectionSizeQuads*SubsectionX;
			INT PrevMipSubY2 = ComponentY2 - SubsectionSizeQuads*SubsectionY;

			INT PrevMipSubsectionSizeQuads = SubsectionSizeQuads;
			FLOAT InvPrevMipSubsectionSizeQuads = 1.f / (FLOAT)SubsectionSizeQuads;

			INT PrevMipSizeU = HeightmapSizeU;
			INT PrevMipSizeV = HeightmapSizeV;

			INT PrevMipHeightmapOffsetX = HeightmapOffsetX;
			INT PrevMipHeightmapOffsetY = HeightmapOffsetY;

			for( INT Mip=1;Mip<HeightmapTextureMipData.Num();Mip++ )
			{
				INT MipSizeU = HeightmapSizeU >> Mip;
				INT MipSizeV = HeightmapSizeV >> Mip;

				INT MipSubsectionSizeQuads = ((SubsectionSizeQuads+1)>>Mip)-1;
				FLOAT InvMipSubsectionSizeQuads = 1.f / (FLOAT)MipSubsectionSizeQuads;

				INT MipHeightmapOffsetX = HeightmapOffsetX>>Mip;
				INT MipHeightmapOffsetY = HeightmapOffsetY>>Mip;

				// Area to update in current mip level coords
				INT MipSubX1 = appFloor( (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubX1 * InvPrevMipSubsectionSizeQuads );
				INT MipSubY1 = appFloor( (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubY1 * InvPrevMipSubsectionSizeQuads );
				INT MipSubX2 = appCeil(  (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubX2 * InvPrevMipSubsectionSizeQuads );
				INT MipSubY2 = appCeil(  (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubY2 * InvPrevMipSubsectionSizeQuads );

				// Clamp area to update
				INT VertX1 = Clamp<INT>(MipSubX1, 0, MipSubsectionSizeQuads);
				INT VertY1 = Clamp<INT>(MipSubY1, 0, MipSubsectionSizeQuads);
				INT VertX2 = Clamp<INT>(MipSubX2, 0, MipSubsectionSizeQuads);
				INT VertY2 = Clamp<INT>(MipSubY2, 0, MipSubsectionSizeQuads);

				for( INT VertY=VertY1;VertY<=VertY2;VertY++ )
				{
					for( INT VertX=VertX1;VertX<=VertX2;VertX++ )
					{
						// Convert VertX/Y into previous mip's coords
						FLOAT PrevMipVertX = (FLOAT)PrevMipSubsectionSizeQuads * (FLOAT)VertX * InvMipSubsectionSizeQuads;
						FLOAT PrevMipVertY = (FLOAT)PrevMipSubsectionSizeQuads * (FLOAT)VertY * InvMipSubsectionSizeQuads;

#if 0
						// Validate that the vertex we skip wouldn't use the updated data in the parent mip.
						// Note this validation is doesn't do anything unless you change the VertY/VertX loops 
						// above to process all verts from 0 .. MipSubsectionSizeQuads.
						if( VertX < VertX1 || VertX > VertX2 )
						{
							check( appCeil(PrevMipVertX) < PrevMipSubX1 || appFloor(PrevMipVertX) > PrevMipSubX2 );
							continue;
						}

						if( VertY < VertY1 || VertY > VertY2 )
						{
							check( appCeil(PrevMipVertY) < PrevMipSubY1 || appFloor(PrevMipVertY) > PrevMipSubY2 );
							continue;
						}
#endif

						// X/Y of the vertex we're looking indexed into the texture data
						INT TexX = (MipHeightmapOffsetX) + (MipSubsectionSizeQuads+1) * SubsectionX + VertX;
						INT TexY = (MipHeightmapOffsetY) + (MipSubsectionSizeQuads+1) * SubsectionY + VertY;

						FLOAT fPrevMipTexX = (FLOAT)(PrevMipHeightmapOffsetX) + (FLOAT)((PrevMipSubsectionSizeQuads+1) * SubsectionX) + PrevMipVertX;
						FLOAT fPrevMipTexY = (FLOAT)(PrevMipHeightmapOffsetY) + (FLOAT)((PrevMipSubsectionSizeQuads+1) * SubsectionY) + PrevMipVertY;

						INT PrevMipTexX = appFloor(fPrevMipTexX);
						FLOAT fPrevMipTexFracX = appFractional(fPrevMipTexX);
						INT PrevMipTexY = appFloor(fPrevMipTexY);
						FLOAT fPrevMipTexFracY = appFractional(fPrevMipTexY);

						checkSlow( TexX >= 0 && TexX < MipSizeU );
						checkSlow( TexY >= 0 && TexY < MipSizeV );
						checkSlow( PrevMipTexX >= 0 && PrevMipTexX < PrevMipSizeU );
						checkSlow( PrevMipTexY >= 0 && PrevMipTexY < PrevMipSizeV );

						INT PrevMipTexX1 = Min<INT>( PrevMipTexX+1, PrevMipSizeU-1 );
						INT PrevMipTexY1 = Min<INT>( PrevMipTexY+1, PrevMipSizeV-1 );

						// Padding for missing data
						if (Mip == 1)
						{
							if (EndX && SubsectionX == NumSubsections-1 && VertX == VertX2)
							{
								for (INT PaddingIdx = PrevMipTexX + PrevMipTexY * PrevMipSizeU; PaddingIdx+1 < PrevMipTexY1 * PrevMipSizeU; ++PaddingIdx)
								{
									HeightmapTextureMipData(Mip-1)[ PaddingIdx+1 ] = HeightmapTextureMipData(Mip-1)[ PaddingIdx ];
								}
							}

							if (EndY && SubsectionX == NumSubsections-1 && SubsectionY == NumSubsections-1 && VertY == VertY2 && VertX == VertX2)
							{
								for (INT PaddingYIdx = PrevMipTexY; PaddingYIdx+1 < PrevMipSizeV; ++PaddingYIdx)
								{
									for (INT PaddingXIdx = 0; PaddingXIdx < PrevMipSizeU; ++PaddingXIdx)
									{
										HeightmapTextureMipData(Mip-1)[ PaddingXIdx + (PaddingYIdx+1) * PrevMipSizeU ] = HeightmapTextureMipData(Mip-1)[ PaddingXIdx + PaddingYIdx * PrevMipSizeU ];
									}
								}
							}
						}

						FColor* TexData = &(HeightmapTextureMipData(Mip))[ TexX + TexY * MipSizeU ];
						FColor *PreMipTexData00 = &(HeightmapTextureMipData(Mip-1))[ PrevMipTexX + PrevMipTexY * PrevMipSizeU ];
						FColor *PreMipTexData01 = &(HeightmapTextureMipData(Mip-1))[ PrevMipTexX + PrevMipTexY1 * PrevMipSizeU ];
						FColor *PreMipTexData10 = &(HeightmapTextureMipData(Mip-1))[ PrevMipTexX1 + PrevMipTexY * PrevMipSizeU ];
						FColor *PreMipTexData11 = &(HeightmapTextureMipData(Mip-1))[ PrevMipTexX1 + PrevMipTexY1 * PrevMipSizeU ];

						// Lerp height values
						WORD PrevMipHeightValue00 = PreMipTexData00->R << 8 | PreMipTexData00->G;
						WORD PrevMipHeightValue01 = PreMipTexData01->R << 8 | PreMipTexData01->G;
						WORD PrevMipHeightValue10 = PreMipTexData10->R << 8 | PreMipTexData10->G;
						WORD PrevMipHeightValue11 = PreMipTexData11->R << 8 | PreMipTexData11->G;

						WORD HeightValue = appRound(
							Lerp(
								Lerp( (FLOAT)PrevMipHeightValue00, (FLOAT)PrevMipHeightValue10, fPrevMipTexFracX),
								Lerp( (FLOAT)PrevMipHeightValue01, (FLOAT)PrevMipHeightValue11, fPrevMipTexFracX),
							fPrevMipTexFracY) );

						TexData->R = HeightValue >> 8;
						TexData->G = HeightValue & 255;

						// Lerp tangents
						TexData->B = appRound(
							Lerp(
							Lerp( (FLOAT)PreMipTexData00->B, (FLOAT)PreMipTexData10->B, fPrevMipTexFracX),
							Lerp( (FLOAT)PreMipTexData01->B, (FLOAT)PreMipTexData11->B, fPrevMipTexFracX),
							fPrevMipTexFracY) );

						TexData->A = appRound(
							Lerp(
							Lerp( (FLOAT)PreMipTexData00->A, (FLOAT)PreMipTexData10->A, fPrevMipTexFracX),
							Lerp( (FLOAT)PreMipTexData01->A, (FLOAT)PreMipTexData11->A, fPrevMipTexFracX),
							fPrevMipTexFracY) );


						// Padding for missing data
						if (EndX && SubsectionX == NumSubsections-1 && VertX == VertX2)
						{
							for (INT PaddingIdx = TexX + TexY * MipSizeU; PaddingIdx+1 < (TexY+1) * MipSizeU; ++PaddingIdx)
							{
								HeightmapTextureMipData(Mip)[ PaddingIdx+1 ] = HeightmapTextureMipData(Mip)[ PaddingIdx ];
							}
						}

						if (EndY && SubsectionX == NumSubsections-1 && SubsectionY == NumSubsections-1 && VertY == VertY2 && VertX == VertX2)
						{
							for (INT PaddingYIdx = TexY; PaddingYIdx+1 < MipSizeV; ++PaddingYIdx)
							{
								for (INT PaddingXIdx = 0; PaddingXIdx < MipSizeU; ++PaddingXIdx)
								{
									HeightmapTextureMipData(Mip)[ PaddingXIdx + (PaddingYIdx+1) * MipSizeU ] = HeightmapTextureMipData(Mip)[ PaddingXIdx + PaddingYIdx * MipSizeU ];
								}
							}
						}
					}
				}

				// Record the areas we updated
				if( TextureDataInfo )
				{
					INT TexX1 = (MipHeightmapOffsetX) + (MipSubsectionSizeQuads+1) * SubsectionX + VertX1;
					INT TexY1 = (MipHeightmapOffsetY) + (MipSubsectionSizeQuads+1) * SubsectionY + VertY1;
					INT TexX2 = (MipHeightmapOffsetX) + (MipSubsectionSizeQuads+1) * SubsectionX + VertX2;
					INT TexY2 = (MipHeightmapOffsetY) + (MipSubsectionSizeQuads+1) * SubsectionY + VertY2;
					TextureDataInfo->AddMipUpdateRegion(Mip,TexX1,TexY1,TexX2,TexY2);
				}

				// Copy current mip values to prev as we move to the next mip.
				PrevMipSubsectionSizeQuads = MipSubsectionSizeQuads;
				InvPrevMipSubsectionSizeQuads = InvMipSubsectionSizeQuads;

				PrevMipSizeU = MipSizeU;
				PrevMipSizeV = MipSizeV;

				PrevMipHeightmapOffsetX = MipHeightmapOffsetX;
				PrevMipHeightmapOffsetY = MipHeightmapOffsetY;

				// Use this mip's area as we move to the next mip
				PrevMipSubX1 = MipSubX1;
				PrevMipSubY1 = MipSubY1;
				PrevMipSubX2 = MipSubX2;
				PrevMipSubY2 = MipSubY2;
			}
		}
	}
}

void ULandscapeComponent::CreateEmptyTextureMips(UTexture2D* Texture, UBOOL bClear /*= FALSE*/)
{
	// Remove any existing mips.
	Texture->Mips.Remove(1, Texture->Mips.Num()-1);

	INT WeightmapSizeU = Texture->SizeX;
	INT WeightmapSizeV = Texture->SizeY;

	INT DataSize = sizeof(FColor);
	if (Texture->Format == PF_G8)
	{
		DataSize = sizeof(BYTE);
	}

	if (bClear)
	{
		FTexture2DMipMap* WeightMipMap = &Texture->Mips(0);
		WeightMipMap->SizeX = WeightmapSizeU;
		WeightMipMap->SizeY = WeightmapSizeV;
		appMemzero(WeightMipMap->Data.Lock(LOCK_READ_WRITE), WeightmapSizeU*WeightmapSizeV*DataSize);
		WeightMipMap->Data.Unlock();
	}

	INT Mip = 1;
	for(;;)
	{
		INT MipSizeU = Max<INT>(WeightmapSizeU >> Mip,1);
		INT MipSizeV = Max<INT>(WeightmapSizeV >> Mip,1);

		// Allocate the mipmap
		FTexture2DMipMap* WeightMipMap = new(Texture->Mips) FTexture2DMipMap;
		WeightMipMap->SizeX = MipSizeU;
		WeightMipMap->SizeY = MipSizeV;
		WeightMipMap->Data.Lock(LOCK_READ_WRITE);
		if (bClear)
		{
			appMemzero(WeightMipMap->Data.Realloc(MipSizeU*MipSizeV*DataSize), MipSizeU*MipSizeV*DataSize);
		}
		else
		{
			WeightMipMap->Data.Realloc(MipSizeU*MipSizeV*DataSize);
		}
		WeightMipMap->Data.Unlock();

		if( MipSizeU == 1 && MipSizeV == 1 )
		{
			break;
		}

		Mip++;
	}
}

template<typename DataType>
void ULandscapeComponent::GenerateMipsTempl(INT InNumSubsections, INT InSubsectionSizeQuads, UTexture2D* Texture, DataType* BaseMipData)
{
	// Remove any existing mips.
	Texture->Mips.Remove(1, Texture->Mips.Num()-1);

	// Stores pointers to the locked mip data
	TArray<DataType*> MipData;

	// Add the first mip's data
	MipData.AddItem( BaseMipData );

	INT WeightmapSizeU = Texture->SizeX;
	INT WeightmapSizeV = Texture->SizeY;

	INT Mip = 1;
	for(;;)
	{
		INT MipSizeU = Max<INT>(WeightmapSizeU >> Mip,1);
		INT MipSizeV = Max<INT>(WeightmapSizeV >> Mip,1);

		// Create the mipmap
		FTexture2DMipMap* WeightMipMap = new(Texture->Mips) FTexture2DMipMap;
		WeightMipMap->SizeX = MipSizeU;
		WeightMipMap->SizeY = MipSizeV;
		WeightMipMap->Data.Lock(LOCK_READ_WRITE);
		MipData.AddItem( (DataType*)WeightMipMap->Data.Realloc(MipSizeU*MipSizeV*sizeof(DataType)) );

		if( MipSizeU == 1 && MipSizeV == 1 )
		{
			break;
		}

		Mip++;
	}

	// Update the newly created mips
	UpdateMipsTempl<DataType>( InNumSubsections, InSubsectionSizeQuads, Texture, MipData );

	// Unlock all the new mips, but not the base mip's data
	for( INT i=1;i<MipData.Num();i++ )
	{
		Texture->Mips(i).Data.Unlock();
	}
}

void ULandscapeComponent::GenerateWeightmapMips(INT InNumSubsections, INT InSubsectionSizeQuads, UTexture2D* WeightmapTexture, FColor* BaseMipData)
{
	GenerateMipsTempl<FColor>(InNumSubsections, InSubsectionSizeQuads, WeightmapTexture, BaseMipData);
}

void ULandscapeComponent::GenerateDataMips(INT InNumSubsections, INT InSubsectionSizeQuads, UTexture2D* Texture, BYTE* BaseMipData)
{
	GenerateMipsTempl<BYTE>(InNumSubsections, InSubsectionSizeQuads, Texture, BaseMipData);
}

namespace
{
	template<typename DataType>
	void BiLerpTextureData(DataType* Output, const DataType* Data00, const DataType* Data10, const DataType* Data01, const DataType* Data11, FLOAT FracX, FLOAT FracY)
	{
		*Output = appRound(
			Lerp(
			Lerp( (FLOAT)*Data00, (FLOAT)*Data10, FracX),
			Lerp( (FLOAT)*Data01, (FLOAT)*Data11, FracX),
			FracY) );
	}

	template<>
	void BiLerpTextureData(FColor* Output, const FColor* Data00, const FColor* Data10, const FColor* Data01, const FColor* Data11, FLOAT FracX, FLOAT FracY)
	{
		Output->R = appRound(
			Lerp(
			Lerp( (FLOAT)Data00->R, (FLOAT)Data10->R, FracX),
			Lerp( (FLOAT)Data01->R, (FLOAT)Data11->R, FracX),
			FracY) );
		Output->G = appRound(
			Lerp(
			Lerp( (FLOAT)Data00->G, (FLOAT)Data10->G, FracX),
			Lerp( (FLOAT)Data01->G, (FLOAT)Data11->G, FracX),
			FracY) );
		Output->B = appRound(
			Lerp(
			Lerp( (FLOAT)Data00->B, (FLOAT)Data10->B, FracX),
			Lerp( (FLOAT)Data01->B, (FLOAT)Data11->B, FracX),
			FracY) );
		Output->A = appRound(
			Lerp(
			Lerp( (FLOAT)Data00->A, (FLOAT)Data10->A, FracX),
			Lerp( (FLOAT)Data01->A, (FLOAT)Data11->A, FracX),
			FracY) );
	}

	template<typename DataType>
	void AverageTexData(DataType* Output, const DataType* Data00, const DataType* Data10, const DataType* Data01, const DataType* Data11)
	{
		*Output = (((INT)(*Data00) + (INT)(*Data10) + (INT)(*Data01) + (INT)(*Data11)) >> 2);
	}

	template<>
	void AverageTexData(FColor* Output, const FColor* Data00, const FColor* Data10, const FColor* Data01, const FColor* Data11)
	{
		Output->R = (((INT)Data00->R + (INT)Data10->R + (INT)Data01->R + (INT)Data11->R) >> 2);
		Output->G = (((INT)Data00->G + (INT)Data10->G + (INT)Data01->G + (INT)Data11->G) >> 2);
		Output->B = (((INT)Data00->B + (INT)Data10->B + (INT)Data01->B + (INT)Data11->B) >> 2);
		Output->A = (((INT)Data00->A + (INT)Data10->A + (INT)Data01->A + (INT)Data11->A) >> 2);
	}

};

template<typename DataType>
void ULandscapeComponent::UpdateMipsTempl(INT InNumSubsections, INT InSubsectionSizeQuads, UTexture2D* Texture, TArray<DataType*>& TextureMipData, INT ComponentX1/*=0*/, INT ComponentY1/*=0*/, INT ComponentX2/*=MAXINT*/, INT ComponentY2/*=MAXINT*/, struct FLandscapeTextureDataInfo* TextureDataInfo/*=NULL*/)
{
	INT WeightmapSizeU = Texture->SizeX;
	INT WeightmapSizeV = Texture->SizeY;

	// Find the maximum mip where each texel's data comes from just one subsection.
	INT MaxWholeSubsectionMip = 1;
	INT Mip=1;
	for(;;)
	{
		INT MipSubsectionSizeQuads = ((InSubsectionSizeQuads+1)>>Mip)-1;

		INT MipSizeU = Max<INT>(WeightmapSizeU >> Mip,1);
		INT MipSizeV = Max<INT>(WeightmapSizeV >> Mip,1);

		// Mip must represent at least one quad to store valid weight data
		if( MipSubsectionSizeQuads > 0 )
		{
			MaxWholeSubsectionMip = Mip;
		}
		else
		{
			break;
		}

		Mip++;
	}

	// Update the mip where each texel's data comes from just one subsection.
	for( INT SubsectionY = 0;SubsectionY < InNumSubsections;SubsectionY++ )
	{
		// Check if subsection is fully above or below the area we are interested in
		if( (ComponentY2 < InSubsectionSizeQuads*SubsectionY) ||	// above
			(ComponentY1 > InSubsectionSizeQuads*(SubsectionY+1)) )	// below
		{
			continue;
		}

		for( INT SubsectionX = 0;SubsectionX < InNumSubsections;SubsectionX++ )
		{
			// Check if subsection is fully to the left or right of the area we are interested in
			if( (ComponentX2 < InSubsectionSizeQuads*SubsectionX) ||	// left
				(ComponentX1 > InSubsectionSizeQuads*(SubsectionX+1)) )	// right
			{
				continue;
			}

			// Area to update in previous mip level coords
			INT PrevMipSubX1 = ComponentX1 - InSubsectionSizeQuads*SubsectionX;
			INT PrevMipSubY1 = ComponentY1 - InSubsectionSizeQuads*SubsectionY;
			INT PrevMipSubX2 = ComponentX2 - InSubsectionSizeQuads*SubsectionX;
			INT PrevMipSubY2 = ComponentY2 - InSubsectionSizeQuads*SubsectionY;

			INT PrevMipSubsectionSizeQuads = InSubsectionSizeQuads;
			FLOAT InvPrevMipSubsectionSizeQuads = 1.f / (FLOAT)InSubsectionSizeQuads;

			INT PrevMipSizeU = WeightmapSizeU;
			INT PrevMipSizeV = WeightmapSizeV;

			for( INT Mip=1;Mip<=MaxWholeSubsectionMip;Mip++ )
			{
				INT MipSizeU = WeightmapSizeU >> Mip;
				INT MipSizeV = WeightmapSizeV >> Mip;

				INT MipSubsectionSizeQuads = ((InSubsectionSizeQuads+1)>>Mip)-1;
				FLOAT InvMipSubsectionSizeQuads = 1.f / (FLOAT)MipSubsectionSizeQuads;

				// Area to update in current mip level coords
				INT MipSubX1 = appFloor( (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubX1 * InvPrevMipSubsectionSizeQuads );
				INT MipSubY1 = appFloor( (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubY1 * InvPrevMipSubsectionSizeQuads );
				INT MipSubX2 = appCeil(  (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubX2 * InvPrevMipSubsectionSizeQuads );
				INT MipSubY2 = appCeil(  (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubY2 * InvPrevMipSubsectionSizeQuads );

				// Clamp area to update
				INT VertX1 = Clamp<INT>(MipSubX1, 0, MipSubsectionSizeQuads);
				INT VertY1 = Clamp<INT>(MipSubY1, 0, MipSubsectionSizeQuads);
				INT VertX2 = Clamp<INT>(MipSubX2, 0, MipSubsectionSizeQuads);
				INT VertY2 = Clamp<INT>(MipSubY2, 0, MipSubsectionSizeQuads);

				for( INT VertY=VertY1;VertY<=VertY2;VertY++ )
				{
					for( INT VertX=VertX1;VertX<=VertX2;VertX++ )
					{
						// Convert VertX/Y into previous mip's coords
						FLOAT PrevMipVertX = (FLOAT)PrevMipSubsectionSizeQuads * (FLOAT)VertX * InvMipSubsectionSizeQuads;
						FLOAT PrevMipVertY = (FLOAT)PrevMipSubsectionSizeQuads * (FLOAT)VertY * InvMipSubsectionSizeQuads;

						// X/Y of the vertex we're looking indexed into the texture data
						INT TexX = (MipSubsectionSizeQuads+1) * SubsectionX + VertX;
						INT TexY = (MipSubsectionSizeQuads+1) * SubsectionY + VertY;

						FLOAT fPrevMipTexX = (FLOAT)((PrevMipSubsectionSizeQuads+1) * SubsectionX) + PrevMipVertX;
						FLOAT fPrevMipTexY = (FLOAT)((PrevMipSubsectionSizeQuads+1) * SubsectionY) + PrevMipVertY;

						INT PrevMipTexX = appFloor(fPrevMipTexX);
						FLOAT fPrevMipTexFracX = appFractional(fPrevMipTexX);
						INT PrevMipTexY = appFloor(fPrevMipTexY);
						FLOAT fPrevMipTexFracY = appFractional(fPrevMipTexY);

						check( TexX >= 0 && TexX < MipSizeU );
						check( TexY >= 0 && TexY < MipSizeV );
						check( PrevMipTexX >= 0 && PrevMipTexX < PrevMipSizeU );
						check( PrevMipTexY >= 0 && PrevMipTexY < PrevMipSizeV );

						INT PrevMipTexX1 = Min<INT>( PrevMipTexX+1, PrevMipSizeU-1 );
						INT PrevMipTexY1 = Min<INT>( PrevMipTexY+1, PrevMipSizeV-1 );

						DataType* TexData = &(TextureMipData(Mip))[ TexX + TexY * MipSizeU ];
						DataType *PreMipTexData00 = &(TextureMipData(Mip-1))[ PrevMipTexX + PrevMipTexY * PrevMipSizeU ];
						DataType *PreMipTexData01 = &(TextureMipData(Mip-1))[ PrevMipTexX + PrevMipTexY1 * PrevMipSizeU ];
						DataType *PreMipTexData10 = &(TextureMipData(Mip-1))[ PrevMipTexX1 + PrevMipTexY * PrevMipSizeU ];
						DataType *PreMipTexData11 = &(TextureMipData(Mip-1))[ PrevMipTexX1 + PrevMipTexY1 * PrevMipSizeU ];

						// Lerp weightmap data
						BiLerpTextureData<DataType>(TexData, PreMipTexData00, PreMipTexData10, PreMipTexData01, PreMipTexData11, fPrevMipTexFracX, fPrevMipTexFracY);
					}
				}

				// Record the areas we updated
				if( TextureDataInfo )
				{
					INT TexX1 = (MipSubsectionSizeQuads+1) * SubsectionX + VertX1;
					INT TexY1 = (MipSubsectionSizeQuads+1) * SubsectionY + VertY1;
					INT TexX2 = (MipSubsectionSizeQuads+1) * SubsectionX + VertX2;
					INT TexY2 = (MipSubsectionSizeQuads+1) * SubsectionY + VertY2;
					TextureDataInfo->AddMipUpdateRegion(Mip,TexX1,TexY1,TexX2,TexY2);
				}

				// Copy current mip values to prev as we move to the next mip.
				PrevMipSubsectionSizeQuads = MipSubsectionSizeQuads;
				InvPrevMipSubsectionSizeQuads = InvMipSubsectionSizeQuads;

				PrevMipSizeU = MipSizeU;
				PrevMipSizeV = MipSizeV;

				// Use this mip's area as we move to the next mip
				PrevMipSubX1 = MipSubX1;
				PrevMipSubY1 = MipSubY1;
				PrevMipSubX2 = MipSubX2;
				PrevMipSubY2 = MipSubY2;
			}
		}
	}

	// Handle mips that have texels from multiple subsections
	Mip=1;
	for(;;)
	{
		INT MipSubsectionSizeQuads = ((InSubsectionSizeQuads+1)>>Mip)-1;

		INT MipSizeU = Max<INT>(WeightmapSizeU >> Mip,1);
		INT MipSizeV = Max<INT>(WeightmapSizeV >> Mip,1);

		// Mip must represent at least one quad to store valid weight data
		if( MipSubsectionSizeQuads <= 0 )
		{
			INT PrevMipSizeU = WeightmapSizeU >> (Mip-1);
			INT PrevMipSizeV = WeightmapSizeV >> (Mip-1);

			// not valid weight data, so just average the texels of the previous mip.
			for( INT Y = 0;Y < MipSizeV;Y++ )
			{
				for( INT X = 0;X < MipSizeU;X++ )
				{
					DataType* TexData = &(TextureMipData(Mip))[ X + Y * MipSizeU ];

					DataType *PreMipTexData00 = &(TextureMipData(Mip-1))[ (X*2+0) + (Y*2+0)  * PrevMipSizeU ];
					DataType *PreMipTexData01 = &(TextureMipData(Mip-1))[ (X*2+0) + (Y*2+1)  * PrevMipSizeU ];
					DataType *PreMipTexData10 = &(TextureMipData(Mip-1))[ (X*2+1) + (Y*2+0)  * PrevMipSizeU ];
					DataType *PreMipTexData11 = &(TextureMipData(Mip-1))[ (X*2+1) + (Y*2+1)  * PrevMipSizeU ];

					AverageTexData<DataType>(TexData, PreMipTexData00, PreMipTexData10, PreMipTexData01, PreMipTexData11);
				}
			}

			if( TextureDataInfo )
			{
				// These mip sizes are small enough that we may as well just update the whole mip.
				TextureDataInfo->AddMipUpdateRegion(Mip,0,0,MipSizeU-1,MipSizeV-1);
			}
		}

		if( MipSizeU == 1 && MipSizeV == 1 )
		{
			break;
		}

		Mip++;
	}
}

void ULandscapeComponent::UpdateWeightmapMips(INT InNumSubsections, INT InSubsectionSizeQuads, UTexture2D* WeightmapTexture, TArray<FColor*>& WeightmapTextureMipData, INT ComponentX1/*=0*/, INT ComponentY1/*=0*/, INT ComponentX2/*=MAXINT*/, INT ComponentY2/*=MAXINT*/, struct FLandscapeTextureDataInfo* TextureDataInfo/*=NULL*/)
{
	UpdateMipsTempl<FColor>(InNumSubsections, InSubsectionSizeQuads, WeightmapTexture, WeightmapTextureMipData, ComponentX1, ComponentY1, ComponentX2, ComponentY2, TextureDataInfo);
}

void ULandscapeComponent::UpdateDataMips(INT InNumSubsections, INT InSubsectionSizeQuads, UTexture2D* Texture, TArray<BYTE*>& TextureMipData, INT ComponentX1/*=0*/, INT ComponentY1/*=0*/, INT ComponentX2/*=MAXINT*/, INT ComponentY2/*=MAXINT*/, struct FLandscapeTextureDataInfo* TextureDataInfo/*=NULL*/)
{
	UpdateMipsTempl<BYTE>(InNumSubsections, InSubsectionSizeQuads, Texture, TextureMipData, ComponentX1, ComponentY1, ComponentX2, ComponentY2, TextureDataInfo);
}

FLOAT ULandscapeComponent::GetLayerWeightAtLocation( const FVector& InLocation, FName InLayerName, TArray<BYTE>* LayerCache )
{
	// Allocate and discard locally if no external cache is passed in.
	TArray<BYTE> LocalCache;
	if( LayerCache==NULL )
	{
		LayerCache = &LocalCache;
	}

	// Fill the cache if necessary
	if( LayerCache->Num() == 0 )
	{
		FLandscapeComponentDataInterface CDI(this);
		if( !CDI.GetWeightmapTextureData( InLayerName, *LayerCache ) )
		{
			// no data for this layer for this component.
			return 0.f;
		}
	}

	// Find location
	ALandscape* Landscape = GetLandscapeActor(); 
	FVector DrawScale = Landscape->DrawScale3D * Landscape->DrawScale;
	FLOAT TestX = (InLocation.X-Landscape->Location.X) / DrawScale.X - (FLOAT)SectionBaseX;
	FLOAT TestY = (InLocation.Y-Landscape->Location.Y) / DrawScale.Y - (FLOAT)SectionBaseY;

	// Find data
	INT X1 = appFloor(TestX);
	INT Y1 = appFloor(TestY);
	INT X2 = appCeil(TestX);
	INT Y2 = appCeil(TestY);

	INT Stride = (SubsectionSizeQuads+1) * NumSubsections;

	// Min is to prevent the sampling of the final column from overflowing
	INT IdxX1 = Min<INT>(((X1 / SubsectionSizeQuads) * (SubsectionSizeQuads+1)) + (X1 % SubsectionSizeQuads), Stride-1);
	INT IdxY1 = Min<INT>(((Y1 / SubsectionSizeQuads) * (SubsectionSizeQuads+1)) + (Y1 % SubsectionSizeQuads), Stride-1);
	INT IdxX2 = Min<INT>(((X2 / SubsectionSizeQuads) * (SubsectionSizeQuads+1)) + (X2 % SubsectionSizeQuads), Stride-1);
	INT IdxY2 = Min<INT>(((Y2 / SubsectionSizeQuads) * (SubsectionSizeQuads+1)) + (Y2 % SubsectionSizeQuads), Stride-1);

	// sample
	FLOAT Sample11 = (FLOAT)((*LayerCache)(IdxX1 + Stride*IdxY1)) / 255.f;
	FLOAT Sample21 = (FLOAT)((*LayerCache)(IdxX2 + Stride*IdxY1)) / 255.f;
	FLOAT Sample12 = (FLOAT)((*LayerCache)(IdxX1 + Stride*IdxY2)) / 255.f;
	FLOAT Sample22 = (FLOAT)((*LayerCache)(IdxX2 + Stride*IdxY2)) / 255.f;

	FLOAT LerpX = appFractional(TestX);
	FLOAT LerpY = appFractional(TestY);

	// Bilinear interpolate
	return Lerp(
		Lerp( Sample11, Sample21, LerpX),
		Lerp( Sample12, Sample22, LerpX),
		LerpY);
}

/** Return the LandscapeHeightfieldCollisionComponent matching this component */
ULandscapeHeightfieldCollisionComponent* ULandscapeComponent::GetCollisionComponent() const
{
	QWORD Key = ALandscape::MakeKey(SectionBaseX, SectionBaseY);
	return GetLandscapeInfo() ? GetLandscapeInfo()->XYtoCollisionComponentMap.FindRef(Key) : NULL;
}

//
// ULandscapeHeightfieldCollisionComponent

/** Return the LandscapeComponent matching this collision component */
ULandscapeComponent* ULandscapeHeightfieldCollisionComponent::GetLandscapeComponent() const
{
	QWORD Key = ALandscape::MakeKey(SectionBaseX, SectionBaseY);
	return GetLandscapeInfo() ? GetLandscapeInfo()->XYtoComponentMap.FindRef(Key) : NULL;
}


//
// ALandscape
//

#define MAX_LANDSCAPE_SUBSECTIONS 2

void ULandscapeInfo::GetComponentsInRegion(INT X1, INT Y1, INT X2, INT Y2, TSet<ULandscapeComponent*>& OutComponents)
{
	if (!LandscapeProxy || LandscapeProxy->ComponentSizeQuads <= 0)
	{
		return;
	}
	// Find component range for this block of data
	INT ComponentIndexX1 = (X1-1 >= 0) ? (X1-1) / LandscapeProxy->ComponentSizeQuads : (X1) / LandscapeProxy->ComponentSizeQuads - 1;	// -1 because we need to pick up vertices shared between components
	INT ComponentIndexY1 = (Y1-1 >= 0) ? (Y1-1) / LandscapeProxy->ComponentSizeQuads : (Y1) / LandscapeProxy->ComponentSizeQuads - 1;
	INT ComponentIndexX2 = (X2 >= 0) ? X2 / LandscapeProxy->ComponentSizeQuads : (X2+1) / LandscapeProxy->ComponentSizeQuads - 1;
	INT ComponentIndexY2 = (Y2 >= 0) ? Y2 / LandscapeProxy->ComponentSizeQuads : (Y2+1) / LandscapeProxy->ComponentSizeQuads - 1;

	for( INT ComponentIndexY=ComponentIndexY1;ComponentIndexY<=ComponentIndexY2;ComponentIndexY++ )
	{
		for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
		{		
			ULandscapeComponent* Component = XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*LandscapeProxy->ComponentSizeQuads,ComponentIndexY*LandscapeProxy->ComponentSizeQuads));
			if( Component && !FLevelUtils::IsLevelLocked(Component->GetLandscapeProxy()->GetLevel()) && FLevelUtils::IsLevelVisible(Component->GetLandscapeProxy()->GetLevel()))
			{
				OutComponents.Add(Component);
			}
		}
	}
}

UBOOL ALandscape::ImportFromOldTerrain(ATerrain* OldTerrain)
{
	if( !OldTerrain )
	{
		return FALSE;
	}

	// Landscape doesn't support rotation...
	if( !OldTerrain->Rotation.IsZero() )
	{
		appMsgf(AMT_OK,TEXT("The Terrain actor has Non-zero rotation, and Landscape doesn't support rotation"), *OldTerrain->GetName());
		return FALSE;
	}

	// Work out how many subsections we need.
	// Until we've got a SubsectionSizeQuads that's one less than a power of 2
	// Keep adding new sections, or until we run out of 
	ComponentSizeQuads = OldTerrain->MaxComponentSize;
	NumSubsections = 1;
	SubsectionSizeQuads = ComponentSizeQuads;
	while( ((SubsectionSizeQuads) & (SubsectionSizeQuads+1)) != 0 || NumSubsections*SubsectionSizeQuads != ComponentSizeQuads )
	{
		if( NumSubsections > MAX_LANDSCAPE_SUBSECTIONS || ComponentSizeQuads / NumSubsections < 1 )
		{
			appMsgf(AMT_OK,TEXT("The Terrain actor %s's MaxComponentSize must be an 1x or 2x multiple of one less than a power of two"), *OldTerrain->GetName());
			return FALSE;
		}

		// try adding another subsection.
		NumSubsections++;
		SubsectionSizeQuads = ComponentSizeQuads / NumSubsections;
	}

	// Should check after changing NumSubsections
	if( NumSubsections > MAX_LANDSCAPE_SUBSECTIONS || ComponentSizeQuads / NumSubsections < 1 )
	{
		appMsgf(AMT_OK,TEXT("The Terrain actor %s's MaxComponentSize must be an 1x or 2x multiple of one less than a power of two"), *OldTerrain->GetName());
		return FALSE;
	}

	debugf(TEXT("%s: using ComponentSizeQuads=%d, NumSubsections=%d, SubsectionSizeQuads=%d"), *OldTerrain->GetName(), ComponentSizeQuads, NumSubsections, SubsectionSizeQuads);

	// Validate old terrain.
	if( OldTerrain->NumPatchesX % OldTerrain->MaxComponentSize != 0 ||
		OldTerrain->NumPatchesX % OldTerrain->MaxComponentSize != 0 )
	{
		appMsgf(AMT_OK,TEXT("The Terrain actor %s's NumPatchesX/Y must be multiples of MaxComponentSize"), *OldTerrain->GetName());
		return FALSE;
	}
	if( OldTerrain->MaxTesselationLevel > 1 )
	{
		appMsgf(AMT_OK,TEXT("The Terrain actor %s's MaxTesselationLevel must be set to 1."), *OldTerrain->GetName());
		return FALSE;
	}

	GWarn->BeginSlowTask( *FString::Printf(TEXT("Converting terrain %s"), *OldTerrain->GetName()), TRUE);

	// Create and import terrain material
	UMaterial* LandscapeUMaterial = ConstructObject<UMaterial>(UMaterial::StaticClass(), GetOutermost(), FName(*FString::Printf(TEXT("%s_Material"),*GetName())), RF_Public|RF_Standalone);
	LandscapeMaterial = LandscapeUMaterial;

	TMap<EMaterialProperty, UMaterialExpressionTerrainLayerWeight*> MaterialPropertyLastLayerWeightExpressionMap;
	
	INT YOffset = 0;

	for( INT LayerIndex=0;LayerIndex<OldTerrain->Layers.Num();LayerIndex++ )
	{
		UTerrainLayerSetup*	Setup = OldTerrain->Layers(LayerIndex).Setup;
		if( Setup )
		{
			if( Setup->Materials.Num() == 1)
			{
				FTerrainFilteredMaterial& FilteredTerrainMaterial = Setup->Materials(0);
				if( FilteredTerrainMaterial.Material )
				{
					UMaterial* Material = Cast<UMaterial>(FilteredTerrainMaterial.Material->Material);
					if( Material == NULL )
					{
						debugf(TEXT("%s's Material is not a plain UMaterial, skipping..."), *FilteredTerrainMaterial.Material->GetName());
						continue;
					}

					TArray<UMaterialExpression*> NewExpressions;
					TArray<UMaterialExpression*> NewComments;

					// Copy all the expression from the material to our new material
					UMaterialExpression::CopyMaterialExpressions(Material->Expressions, Material->EditorComments, LandscapeUMaterial, NULL, NewExpressions, NewComments);

					for( INT CommentIndex=0;CommentIndex<NewComments.Num();CommentIndex++ )
					{
						// Move comments
						UMaterialExpression* Comment = NewComments(CommentIndex);
						Comment->MaterialExpressionEditorX += 200;
						Comment->MaterialExpressionEditorY += YOffset;
					}

					UMaterialExpressionTerrainLayerCoords* LayerTexCoords = NULL;

					for( INT ExpressionIndex=0;ExpressionIndex<NewExpressions.Num();ExpressionIndex++ )
					{
						UMaterialExpression* Expression = NewExpressions(ExpressionIndex);

						// Move expressions
						Expression->MaterialExpressionEditorX += 200;
						Expression->MaterialExpressionEditorY += YOffset;

						// Fix up texture coordinates for this layer
						UMaterialExpressionTextureSample* TextureSampleExpression = Cast<UMaterialExpressionTextureSample>(Expression);
						if( TextureSampleExpression != NULL) // && TextureSampleExpression->Coordinates.Expression == NULL)
						{
							if( LayerTexCoords == NULL )
							{
								LayerTexCoords = ConstructObject<UMaterialExpressionTerrainLayerCoords>( UMaterialExpressionTerrainLayerCoords::StaticClass(), LandscapeUMaterial, NAME_None, RF_Transactional );
								LandscapeUMaterial->Expressions.AddItem(LayerTexCoords);
								LayerTexCoords->MaterialExpressionEditorX = Expression->MaterialExpressionEditorX + 120;
								LayerTexCoords->MaterialExpressionEditorY = Expression->MaterialExpressionEditorY + 48;
								check((INT)TMT_MAX == (INT)TCMT_MAX);
								//!! TODO make these parameters
								LayerTexCoords->MappingType		= FilteredTerrainMaterial.Material->MappingType;
								LayerTexCoords->MappingScale	= FilteredTerrainMaterial.Material->MappingScale;
								LayerTexCoords->MappingRotation = FilteredTerrainMaterial.Material->MappingRotation;
								LayerTexCoords->MappingPanU		= FilteredTerrainMaterial.Material->MappingPanU;
								LayerTexCoords->MappingPanV		= FilteredTerrainMaterial.Material->MappingPanV;
							}

							TextureSampleExpression->Coordinates.Expression = LayerTexCoords;
						}
					}

					INT NumPropertiesWeighted = 0;
					for( INT PropertyIndex=0;PropertyIndex < MP_MAX;PropertyIndex++ )
					{
						FExpressionInput* PropertyInput = Material->GetExpressionInputForProperty((EMaterialProperty)PropertyIndex);
						if( PropertyInput->Expression )
						{
							// Need to construct a new UMaterialExpressionTerrainLayerWeight to blend in this layer for this input.
							UMaterialExpressionTerrainLayerWeight* WeightExpression = ConstructObject<UMaterialExpressionTerrainLayerWeight>( UMaterialExpressionTerrainLayerWeight::StaticClass(), LandscapeUMaterial, NAME_None, RF_Transactional );
							LandscapeUMaterial->Expressions.AddItem(WeightExpression);
							WeightExpression->ConditionallyGenerateGUID(TRUE);

							WeightExpression->MaterialExpressionEditorX = 200 + 32 * NumPropertiesWeighted++;
							WeightExpression->MaterialExpressionEditorY = YOffset;
							YOffset += 64;

							// Connect the previous layer's weight blend for this material property as the Base, or NULL if there was none.
							WeightExpression->Base.Expression = MaterialPropertyLastLayerWeightExpressionMap.FindRef((EMaterialProperty)PropertyIndex);

							// Connect this layer to the Layer input.
							WeightExpression->Layer = *PropertyInput;

							// Remap the expression it points to, so we're working on the copy.
							INT ExpressionIndex = Material->Expressions.FindItemIndex(PropertyInput->Expression);
							check(ExpressionIndex != INDEX_NONE);					
							WeightExpression->Layer.Expression = NewExpressions(ExpressionIndex);
							WeightExpression->ParameterName = FName(*FString::Printf(TEXT("Layer%d"),LayerIndex));
							
							// Remember this weight expression as the last layer for this material property
							MaterialPropertyLastLayerWeightExpressionMap.Set((EMaterialProperty)PropertyIndex, WeightExpression);
						}
					}

					for( INT ExpressionIndex=0;ExpressionIndex<NewExpressions.Num();ExpressionIndex++ )
					{
						UMaterialExpression* Expression = NewExpressions(ExpressionIndex);

						if( Expression->MaterialExpressionEditorY > YOffset )
						{
							YOffset = Expression->MaterialExpressionEditorY;
						}
					}

					YOffset += 64;
				}
			}
			else
			{
				debugf(TEXT("%s is a multi-layer filtered material, skipping..."), *Setup->GetName());
			}
		}
	}

	// Assign all the material inputs
	for( INT PropertyIndex=0;PropertyIndex < MP_MAX;PropertyIndex++ )
	{
		UMaterialExpressionTerrainLayerWeight* WeightExpression = MaterialPropertyLastLayerWeightExpressionMap.FindRef((EMaterialProperty)PropertyIndex);
		if( WeightExpression )
		{
			FExpressionInput* PropertyInput = LandscapeUMaterial->GetExpressionInputForProperty((EMaterialProperty)PropertyIndex);
			check(PropertyInput);
			PropertyInput->Expression = WeightExpression;
		}
	}

	LandscapeUMaterial->CacheResourceShaders(GRHIShaderPlatform, TRUE);

	DrawScale = OldTerrain->DrawScale;
	DrawScale3D = OldTerrain->DrawScale3D;

	// import!
	INT VertsX = OldTerrain->NumPatchesX+1;
	INT VertsY = OldTerrain->NumPatchesY+1;

	// Copy height data
	WORD* HeightData = new WORD[VertsX*VertsY];
	for( INT Y=0;Y<VertsY;Y++ )
	{
		for( INT X=0;X<VertsX;X++ )
		{
			HeightData[Y*VertsX + X] = OldTerrain->Height(X,Y);
		}
	}

	TArray<FLandscapeLayerInfo> ImportLayerInfos;
	TArray<BYTE*> ImportAlphaDataPointers;

	// Copy over Alphamap data from old terrain
	for( INT LayerIndex=0;LayerIndex<OldTerrain->Layers.Num();LayerIndex++ )
	{
		INT AlphaMapIndex = OldTerrain->Layers(LayerIndex).AlphaMapIndex;

		if( AlphaMapIndex != -1 || LayerIndex == 0 )
		{
			BYTE* AlphaData = new BYTE[VertsX*VertsY];
			ImportAlphaDataPointers.AddItem(AlphaData);
			new(ImportLayerInfos) FLandscapeLayerInfo(FName(*FString::Printf(TEXT("Layer%d"),LayerIndex)));
			//ImportLayerNames.AddItem(FName(*FString::Printf(TEXT("Layer%d"),LayerIndex)));

			if( AlphaMapIndex == -1 || LayerIndex == 0 )
			{
				// First layer doesn't have an alphamap, as it's completely opaque.
				appMemset( AlphaData, 255, VertsX*VertsY );
			}
			else
			{
				check( OldTerrain->AlphaMaps(AlphaMapIndex).Data.Num() == VertsX * VertsY );
				appMemcpy(AlphaData, &OldTerrain->AlphaMaps(AlphaMapIndex).Data(0), VertsX * VertsY);
			}
		}
	}

	// import heightmap and weightmap
	Import(VertsX, VertsY, ComponentSizeQuads, NumSubsections, SubsectionSizeQuads, HeightData, NULL, ImportLayerInfos, &ImportAlphaDataPointers(0));

	delete[] HeightData;
	for( INT i=0;i<ImportAlphaDataPointers.Num();i++)
	{
		delete []ImportAlphaDataPointers(i);
	}
	ImportAlphaDataPointers.Empty();

	GWarn->EndSlowTask();

	return TRUE;
}

// A struct to remember where we have spare texture channels.
struct FWeightmapTextureAllocation
{
	INT X;
	INT Y;
	INT ChannelsInUse;
	UTexture2D* Texture;
	FColor* TextureData;

	FWeightmapTextureAllocation( INT InX, INT InY, INT InChannels, UTexture2D* InTexture, FColor* InTextureData )
		:	X(InX)
		,	Y(InY)
		,	ChannelsInUse(InChannels)
		,	Texture(InTexture)
		,	TextureData(InTextureData)
	{}
};

// A struct to hold the info about each texture chunk of the total heightmap
struct FHeightmapInfo
{
	INT HeightmapSizeU;
	INT HeightmapSizeV;
	UTexture2D* HeightmapTexture;
	TArray<FColor*> HeightmapTextureMipData;
};

ULandscapeLayerInfoObject* ALandscapeProxy::GetLayerInfo(const TCHAR* LayerName, UPackage* Package/*= NULL*/, const TCHAR* SourceFilePath /*= NULL*/)
{
	// For now, don't allow same name in one package
	FString LayerObjectName = FString::Printf(TEXT("LayerInfoObject_%s"), LayerName);
	if (Package == NULL)
	{
		Package = GetOutermost();
	}
	ULandscapeLayerInfoObject* LayerInfo = LoadObject<ULandscapeLayerInfoObject>(Package, *LayerObjectName, NULL, LOAD_NoWarn|LOAD_Quiet, NULL );
	if (!LayerInfo)
	{
		LayerInfo = ConstructObject<ULandscapeLayerInfoObject>(ULandscapeLayerInfoObject::StaticClass(), Package, FName(*LayerObjectName), RF_Public | RF_Standalone);
		LayerInfo->MarkPackageDirty();
	}

	if (LayerInfo)
	{
		LayerInfo->LayerName = LayerName;
		ULandscapeInfo* LandscapeInfo = GetLandscapeInfo();
		if (LandscapeInfo)
		{
			LayerInfoObjs.AddItem(FLandscapeLayerStruct(LayerInfo, this, SourceFilePath));
			LandscapeInfo->UpdateLayerInfoMap(this);
			//LandscapeInfo->LayerInfoMap.Set(LayerInfo->LayerName, &LayerInfoObjs.Last() );
		}
	}
	return LayerInfo;
}

#define HEIGHTDATA(X,Y) (HeightData[ Clamp<INT>(Y,0,VertsY) * VertsX + Clamp<INT>(X,0,VertsX) ])
void ALandscape::Import(INT VertsX, INT VertsY, INT InComponentSizeQuads, INT InNumSubsections, INT InSubsectionSizeQuads, WORD* HeightData, const TCHAR* HeightmapFileName, TArray<FLandscapeLayerInfo> ImportLayerInfos, BYTE* AlphaDataPointers[] )
{
	GWarn->BeginSlowTask( TEXT("Importing Landscape"), TRUE);

	ComponentSizeQuads = InComponentSizeQuads;
	NumSubsections = InNumSubsections;
	SubsectionSizeQuads = InSubsectionSizeQuads;

	MarkPackageDirty();

	INT NumPatchesX = (VertsX-1);
	INT NumPatchesY = (VertsY-1);

	INT NumSectionsX = NumPatchesX / ComponentSizeQuads;
	INT NumSectionsY = NumPatchesY / ComponentSizeQuads;

	//LayerInfos = ImportLayerInfos;
	LandscapeGuid = appCreateGuid();

	ULandscapeInfo* LandscapeInfo = GetLandscapeInfo();
	check(LandscapeInfo);
	for (int i = 0; i < ImportLayerInfos.Num(); ++i )
	{
		ULandscapeLayerInfoObject* LayerInfo = GetLayerInfo(*ImportLayerInfos(i).LayerName.ToString(), NULL, *ImportLayerInfos(i).LayerSourceFile);
		if (LayerInfo)
		{
			LayerInfo->GetSharedProperties(&(ImportLayerInfos(i)));
		}
	}
	
	LandscapeComponents.Empty(NumSectionsX * NumSectionsY);

	for (INT Y = 0; Y < NumSectionsY; Y++)
	{
		for (INT X = 0; X < NumSectionsX; X++)
		{
			// The number of quads
			INT NumQuadsX = NumPatchesX;
			INT NumQuadsY = NumPatchesY;

			INT BaseX = X * ComponentSizeQuads;
			INT BaseY = Y * ComponentSizeQuads;

			ULandscapeComponent* LandscapeComponent = ConstructObject<ULandscapeComponent>(ULandscapeComponent::StaticClass(),this,NAME_None,RF_Transactional);
			LandscapeComponents.AddItem(LandscapeComponent);
			LandscapeComponent->Init(
				BaseX,BaseY,
				ComponentSizeQuads,
				NumSubsections,
				SubsectionSizeQuads
				);

		}
	}

#define MAX_HEIGHTMAP_TEXTURE_SIZE 512

	INT ComponentSizeVerts = NumSubsections * (SubsectionSizeQuads+1);
	INT ComponentsPerHeightmap = MAX_HEIGHTMAP_TEXTURE_SIZE / ComponentSizeVerts;

	// Count how many heightmaps we need and the X dimension of the final heightmap
	INT NumHeightmapsX = 1;
	INT FinalComponentsX = NumSectionsX;
	while( FinalComponentsX > ComponentsPerHeightmap )
	{
		FinalComponentsX -= ComponentsPerHeightmap;
		NumHeightmapsX++;
	}
	// Count how many heightmaps we need and the Y dimension of the final heightmap
	INT NumHeightmapsY = 1;
	INT FinalComponentsY = NumSectionsY;
	while( FinalComponentsY > ComponentsPerHeightmap )
	{
		FinalComponentsY -= ComponentsPerHeightmap;
		NumHeightmapsY++;
	}

	TArray<FHeightmapInfo> HeightmapInfos;

	for( INT HmY=0;HmY<NumHeightmapsY;HmY++ )
	{
		for( INT HmX=0;HmX<NumHeightmapsX;HmX++ )
		{
			FHeightmapInfo& HeightmapInfo = HeightmapInfos(HeightmapInfos.AddZeroed());

			// make sure the heightmap UVs are powers of two.
			HeightmapInfo.HeightmapSizeU = (1<<appCeilLogTwo( ((HmX==NumHeightmapsX-1) ? FinalComponentsX : ComponentsPerHeightmap)*ComponentSizeVerts ));
			HeightmapInfo.HeightmapSizeV = (1<<appCeilLogTwo( ((HmY==NumHeightmapsY-1) ? FinalComponentsY : ComponentsPerHeightmap)*ComponentSizeVerts ));

			// Construct the heightmap textures
			HeightmapInfo.HeightmapTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), GetOutermost(), NAME_None/*FName(TEXT("Heightmap"))*/, RF_Public);
			HeightmapInfo.HeightmapTexture->Init(HeightmapInfo.HeightmapSizeU,HeightmapInfo.HeightmapSizeV,PF_A8R8G8B8);
			HeightmapInfo.HeightmapTexture->SRGB = FALSE;
			HeightmapInfo.HeightmapTexture->CompressionNone = TRUE;
			HeightmapInfo.HeightmapTexture->MipGenSettings = TMGS_LeaveExistingMips;
			HeightmapInfo.HeightmapTexture->LODGroup = TEXTUREGROUP_Terrain_Heightmap;
			HeightmapInfo.HeightmapTexture->AddressX = TA_Clamp;
			HeightmapInfo.HeightmapTexture->AddressY = TA_Clamp;

			INT MipSubsectionSizeQuads = SubsectionSizeQuads;
			INT MipSizeU = HeightmapInfo.HeightmapSizeU;
			INT MipSizeV = HeightmapInfo.HeightmapSizeV;
			while( MipSizeU > 1 && MipSizeV > 1 && MipSubsectionSizeQuads >= 1 )
			{
				FColor* HeightmapTextureData;
				if( HeightmapInfo.HeightmapTextureMipData.Num() > 0 )	
				{
					// create subsequent mips
					FTexture2DMipMap* HeightMipMap = new(HeightmapInfo.HeightmapTexture->Mips) FTexture2DMipMap;
					HeightMipMap->SizeX = MipSizeU;
					HeightMipMap->SizeY = MipSizeV;
					HeightMipMap->Data.Lock(LOCK_READ_WRITE);
					HeightmapTextureData = (FColor*)HeightMipMap->Data.Realloc(MipSizeU*MipSizeV*sizeof(FColor));
				}
				else
				{
					HeightmapTextureData = (FColor*)HeightmapInfo.HeightmapTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);
				}

				appMemzero( HeightmapTextureData, MipSizeU*MipSizeV*sizeof(FColor) );
				HeightmapInfo.HeightmapTextureMipData.AddItem(HeightmapTextureData);

				MipSizeU >>= 1;
				MipSizeV >>= 1;

				MipSubsectionSizeQuads = ((MipSubsectionSizeQuads + 1) >> 1) - 1;
			}
		}
	}

	// Calculate the normals for each of the two triangles per quad.
	FVector* VertexNormals = new FVector[(NumPatchesX+1)*(NumPatchesY+1)];
	appMemzero(VertexNormals, (NumPatchesX+1)*(NumPatchesY+1)*sizeof(FVector));
	for( INT QuadY=0;QuadY<NumPatchesY;QuadY++ )
	{
		for( INT QuadX=0;QuadX<NumPatchesX;QuadX++ )
		{
			FVector Vert00 = FVector(0.f,0.f,((FLOAT)HEIGHTDATA(QuadX+0, QuadY+0) - 32768.f)*LANDSCAPE_ZSCALE) * DrawScale3D;
			FVector Vert01 = FVector(0.f,1.f,((FLOAT)HEIGHTDATA(QuadX+0, QuadY+1) - 32768.f)*LANDSCAPE_ZSCALE) * DrawScale3D;
			FVector Vert10 = FVector(1.f,0.f,((FLOAT)HEIGHTDATA(QuadX+1, QuadY+0) - 32768.f)*LANDSCAPE_ZSCALE) * DrawScale3D;
			FVector Vert11 = FVector(1.f,1.f,((FLOAT)HEIGHTDATA(QuadX+1, QuadY+1) - 32768.f)*LANDSCAPE_ZSCALE) * DrawScale3D;

			FVector FaceNormal1 = ((Vert00-Vert10) ^ (Vert10-Vert11)).SafeNormal();
			FVector FaceNormal2 = ((Vert11-Vert01) ^ (Vert01-Vert00)).SafeNormal(); 

			// contribute to the vertex normals.
			VertexNormals[(QuadX+1 + (NumPatchesX+1)*(QuadY+0))] += FaceNormal1;
			VertexNormals[(QuadX+0 + (NumPatchesX+1)*(QuadY+1))] += FaceNormal2;
			VertexNormals[(QuadX+0 + (NumPatchesX+1)*(QuadY+0))] += FaceNormal1 + FaceNormal2;
			VertexNormals[(QuadX+1 + (NumPatchesX+1)*(QuadY+1))] += FaceNormal1 + FaceNormal2;
		}
	}

	// Weight values for each layer for each component.
	TArray<TArray<TArray<BYTE> > > ComponentWeightValues;
	ComponentWeightValues.AddZeroed(NumSectionsX*NumSectionsY);

	for (INT ComponentY = 0; ComponentY < NumSectionsY; ComponentY++)
	{
		for (INT ComponentX = 0; ComponentX < NumSectionsX; ComponentX++)
		{
			ULandscapeComponent* LandscapeComponent = LandscapeComponents(ComponentX + ComponentY*NumSectionsX);
			TArray<TArray<BYTE> >& WeightValues = ComponentWeightValues(ComponentX + ComponentY*NumSectionsX);

			// Import alphamap data into local array and check for unused layers for this component.
			TArray<FLandscapeComponentAlphaInfo> EditingAlphaLayerData;
			for( INT LayerIndex=0;LayerIndex<ImportLayerInfos.Num();LayerIndex++ )
			{
				FLandscapeComponentAlphaInfo* NewAlphaInfo = new(EditingAlphaLayerData) FLandscapeComponentAlphaInfo(LandscapeComponent, LayerIndex);

				for( INT AlphaY=0;AlphaY<=LandscapeComponent->ComponentSizeQuads;AlphaY++ )
				{
					BYTE* OldAlphaRowStart = &AlphaDataPointers[LayerIndex][ (AlphaY+LandscapeComponent->SectionBaseY) * VertsX + (LandscapeComponent->SectionBaseX) ];
					BYTE* NewAlphaRowStart = &NewAlphaInfo->AlphaValues(AlphaY * (LandscapeComponent->ComponentSizeQuads+1));
					appMemcpy(NewAlphaRowStart, OldAlphaRowStart, LandscapeComponent->ComponentSizeQuads+1);
				}						
			}

			for( INT AlphaMapIndex=0; AlphaMapIndex<EditingAlphaLayerData.Num();AlphaMapIndex++ )
			{
				if( EditingAlphaLayerData(AlphaMapIndex).IsLayerAllZero() )
				{
					EditingAlphaLayerData.Remove(AlphaMapIndex);
					AlphaMapIndex--;
				}
			}

			
			debugf(TEXT("%s needs %d alphamaps"), *LandscapeComponent->GetName(),EditingAlphaLayerData.Num());

			// Calculate weightmap weights for this component
			WeightValues.Empty(EditingAlphaLayerData.Num());
			WeightValues.AddZeroed(EditingAlphaLayerData.Num());
			LandscapeComponent->WeightmapLayerAllocations.Empty(EditingAlphaLayerData.Num());

			TArray<UBOOL> IsNoBlendArray;
			IsNoBlendArray.Empty(EditingAlphaLayerData.Num());
			IsNoBlendArray.AddZeroed(EditingAlphaLayerData.Num());

			for( INT WeightLayerIndex=0; WeightLayerIndex<WeightValues.Num();WeightLayerIndex++ )
			{
				// Lookup the original layer name
				WeightValues(WeightLayerIndex) = EditingAlphaLayerData(WeightLayerIndex).AlphaValues;
				new(LandscapeComponent->WeightmapLayerAllocations) FWeightmapLayerAllocationInfo(ImportLayerInfos(EditingAlphaLayerData(WeightLayerIndex).LayerIndex).LayerName);
				IsNoBlendArray(WeightLayerIndex) = ImportLayerInfos(EditingAlphaLayerData(WeightLayerIndex).LayerIndex).bNoWeightBlend;
			}

			// Discard the temporary alpha data
			EditingAlphaLayerData.Empty();

			// For each layer...
			for( INT WeightLayerIndex=WeightValues.Num()-1; WeightLayerIndex>=0;WeightLayerIndex-- )
			{
				// ... multiply all lower layers'...
				for( INT BelowWeightLayerIndex=WeightLayerIndex-1; BelowWeightLayerIndex>=0;BelowWeightLayerIndex-- )
				{
					INT TotalWeight = 0;

					if (IsNoBlendArray(BelowWeightLayerIndex))
					{
						continue; // skip no blend
					}

					// ... values by...
					for( INT Idx=0;Idx<WeightValues(WeightLayerIndex).Num();Idx++ )
					{
						// ... one-minus the current layer's values
						INT NewValue = (INT)WeightValues(BelowWeightLayerIndex)(Idx) * (INT)(255 - WeightValues(WeightLayerIndex)(Idx)) / 255;
						WeightValues(BelowWeightLayerIndex)(Idx) = (BYTE)NewValue;
						TotalWeight += NewValue;
					}

					if( TotalWeight == 0 )
					{
						// Remove the layer as it has no contribution
						WeightValues.Remove(BelowWeightLayerIndex);
						LandscapeComponent->WeightmapLayerAllocations.Remove(BelowWeightLayerIndex);
						IsNoBlendArray.Remove(BelowWeightLayerIndex);

						// The current layer has been re-numbered
						WeightLayerIndex--;
					}
				}
			}

			// Weight normalization for total should be 255...
			if (WeightValues.Num())
			{
				for( INT Idx=0;Idx<WeightValues(0).Num();Idx++ )
				{
					INT TotalWeight = 0;
					INT MaxLayerIdx = -1;
					INT MaxWeight = INT_MIN;

					for( INT WeightLayerIndex = 0; WeightLayerIndex < WeightValues.Num(); WeightLayerIndex++ )
					{
						if (!IsNoBlendArray(WeightLayerIndex))
						{
							INT Weight = WeightValues(WeightLayerIndex)(Idx);
							TotalWeight += Weight;
							if (MaxWeight < Weight)
							{
								MaxWeight = Weight;
								MaxLayerIdx = WeightLayerIndex;
							}
						}
					}

					if (TotalWeight == 0)
					{
						if (MaxLayerIdx >= 0)
						{
							WeightValues(MaxLayerIdx)(Idx) = 255;
						}
					}
					else if (TotalWeight != 255)
					{
						// normalization...
						FLOAT Factor = 255.f/TotalWeight;
						TotalWeight = 0;
						for( INT WeightLayerIndex = 0; WeightLayerIndex < WeightValues.Num(); WeightLayerIndex++ )
						{
							if (!IsNoBlendArray(WeightLayerIndex))
							{
								WeightValues(WeightLayerIndex)(Idx) = (BYTE)(Factor * WeightValues(WeightLayerIndex)(Idx));
								TotalWeight += WeightValues(WeightLayerIndex)(Idx);
							}
						}

						if (255 - TotalWeight && MaxLayerIdx >= 0)
						{
							WeightValues(MaxLayerIdx)(Idx) += 255 - TotalWeight;
						}
					}
				}
			}
		}
	}

	// Remember where we have spare texture channels.
	TArray<FWeightmapTextureAllocation> TextureAllocations;
		
	for (INT ComponentY = 0; ComponentY < NumSectionsY; ComponentY++)
	{
		INT HmY = ComponentY / ComponentsPerHeightmap;
		INT HeightmapOffsetY = (ComponentY - ComponentsPerHeightmap*HmY) * NumSubsections * (SubsectionSizeQuads+1);

		for (INT ComponentX = 0; ComponentX < NumSectionsX; ComponentX++)
		{
			INT HmX = ComponentX / ComponentsPerHeightmap;
			FHeightmapInfo& HeightmapInfo = HeightmapInfos(HmX + HmY * NumHeightmapsX);

			ULandscapeComponent* LandscapeComponent = LandscapeComponents(ComponentX + ComponentY*NumSectionsX);

			// Lookup array of weight values for this component.
			TArray<TArray<BYTE> >& WeightValues = ComponentWeightValues(ComponentX + ComponentY*NumSectionsX);

			// Heightmap offsets
			INT HeightmapOffsetX = (ComponentX - ComponentsPerHeightmap*HmX) * NumSubsections * (SubsectionSizeQuads+1);

			LandscapeComponent->HeightmapScaleBias = FVector4( 1.f / (FLOAT)HeightmapInfo.HeightmapSizeU, 1.f / (FLOAT)HeightmapInfo.HeightmapSizeV, (FLOAT)((HeightmapOffsetX)) / (FLOAT)HeightmapInfo.HeightmapSizeU, ((FLOAT)(HeightmapOffsetY)) / (FLOAT)HeightmapInfo.HeightmapSizeV );
			LandscapeComponent->HeightmapTexture = HeightmapInfo.HeightmapTexture;

			// Weightmap is sized the same as the component
			INT WeightmapSize = (SubsectionSizeQuads+1) * NumSubsections;
			// Should be power of two
			check(((WeightmapSize-1) & WeightmapSize) == 0);

			LandscapeComponent->WeightmapScaleBias = FVector4( 1.f / (FLOAT)WeightmapSize, 1.f / (FLOAT)WeightmapSize, 0.5f / (FLOAT)WeightmapSize, 0.5f / (FLOAT)WeightmapSize);
			LandscapeComponent->WeightmapSubsectionOffset =  (FLOAT)(SubsectionSizeQuads+1) / (FLOAT)WeightmapSize;

			// Pointers to the texture data where we'll store each layer. Stride is 4 (FColor)
			TArray<BYTE*> WeightmapTextureDataPointers;

			debugf(TEXT("%s needs %d weightmap channels"), *LandscapeComponent->GetName(),WeightValues.Num());

			// Find texture channels to store each layer.
			INT LayerIndex = 0;
			while( LayerIndex < WeightValues.Num() )
			{
				INT RemainingLayers = WeightValues.Num()-LayerIndex;

				INT BestAllocationIndex = -1;

				// if we need less than 4 channels, try to find them somewhere to put all of them
				if( RemainingLayers < 4 )
				{
					INT BestDistSquared = MAXINT;
					for( INT TryAllocIdx=0;TryAllocIdx<TextureAllocations.Num();TryAllocIdx++ )
					{
						if( TextureAllocations(TryAllocIdx).ChannelsInUse + RemainingLayers <= 4 )
						{
							FWeightmapTextureAllocation& TryAllocation = TextureAllocations(TryAllocIdx);
							INT TryDistSquared = Square(TryAllocation.X-ComponentX) + Square(TryAllocation.Y-ComponentY);
							if( TryDistSquared < BestDistSquared )
							{
								BestDistSquared = TryDistSquared;
								BestAllocationIndex = TryAllocIdx;
							}
						}
					}
				}

				if( BestAllocationIndex != -1 )
				{
					FWeightmapTextureAllocation& Allocation = TextureAllocations(BestAllocationIndex);
					
					debugf(TEXT("  ==> Storing %d channels starting at %s[%d]"), RemainingLayers, *Allocation.Texture->GetName(), Allocation.ChannelsInUse );

					for( INT i=0;i<RemainingLayers;i++ )
					{
						LandscapeComponent->WeightmapLayerAllocations(LayerIndex+i).WeightmapTextureIndex = LandscapeComponent->WeightmapTextures.Num();
						LandscapeComponent->WeightmapLayerAllocations(LayerIndex+i).WeightmapTextureChannel = Allocation.ChannelsInUse;
						switch( Allocation.ChannelsInUse )
						{
						case 1:
							WeightmapTextureDataPointers.AddItem((BYTE*)&Allocation.TextureData->G);
							break;
						case 2:
							WeightmapTextureDataPointers.AddItem((BYTE*)&Allocation.TextureData->B);
							break;
						case 3:
							WeightmapTextureDataPointers.AddItem((BYTE*)&Allocation.TextureData->A);
							break;
						default:
							// this should not occur.
							check(0);

						}
						Allocation.ChannelsInUse++;
					}

					LayerIndex += RemainingLayers;
					LandscapeComponent->WeightmapTextures.AddItem(Allocation.Texture);
				}
				else
				{
					// We couldn't find a suitable place for these layers, so lets make a new one.
					UTexture2D* WeightmapTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), GetOutermost(), NAME_None, RF_Public);
					WeightmapTexture->Init(WeightmapSize,WeightmapSize,PF_A8R8G8B8);
					WeightmapTexture->SRGB = FALSE;
					WeightmapTexture->CompressionNone = TRUE;
					WeightmapTexture->MipGenSettings = TMGS_LeaveExistingMips;
					WeightmapTexture->AddressX = TA_Clamp;
					WeightmapTexture->AddressY = TA_Clamp;
					WeightmapTexture->LODGroup = TEXTUREGROUP_Terrain_Weightmap;
					FColor* MipData = (FColor*)WeightmapTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);

					INT ThisAllocationLayers = Min<INT>(RemainingLayers,4);
					new(TextureAllocations) FWeightmapTextureAllocation(ComponentX,ComponentY,ThisAllocationLayers,WeightmapTexture,MipData);

					debugf(TEXT("  ==> Storing %d channels in new texture %s"), ThisAllocationLayers, *WeightmapTexture->GetName());

					WeightmapTextureDataPointers.AddItem((BYTE*)&MipData->R);
					LandscapeComponent->WeightmapLayerAllocations(LayerIndex+0).WeightmapTextureIndex = LandscapeComponent->WeightmapTextures.Num();
					LandscapeComponent->WeightmapLayerAllocations(LayerIndex+0).WeightmapTextureChannel = 0;

					if( ThisAllocationLayers > 1 )
					{
						WeightmapTextureDataPointers.AddItem((BYTE*)&MipData->G);
						LandscapeComponent->WeightmapLayerAllocations(LayerIndex+1).WeightmapTextureIndex = LandscapeComponent->WeightmapTextures.Num();
						LandscapeComponent->WeightmapLayerAllocations(LayerIndex+1).WeightmapTextureChannel = 1;

						if( ThisAllocationLayers > 2 )
						{
							WeightmapTextureDataPointers.AddItem((BYTE*)&MipData->B);
							LandscapeComponent->WeightmapLayerAllocations(LayerIndex+2).WeightmapTextureIndex = LandscapeComponent->WeightmapTextures.Num();
							LandscapeComponent->WeightmapLayerAllocations(LayerIndex+2).WeightmapTextureChannel = 2;

							if( ThisAllocationLayers > 3 )
							{
								WeightmapTextureDataPointers.AddItem((BYTE*)&MipData->A);
								LandscapeComponent->WeightmapLayerAllocations(LayerIndex+3).WeightmapTextureIndex = LandscapeComponent->WeightmapTextures.Num();
								LandscapeComponent->WeightmapLayerAllocations(LayerIndex+3).WeightmapTextureChannel = 3;
							}
						}
					}
					LandscapeComponent->WeightmapTextures.AddItem(WeightmapTexture);

					LayerIndex += ThisAllocationLayers;
				}
			}
			check(WeightmapTextureDataPointers.Num() == WeightValues.Num());

			FVector* WorldVerts = new FVector[Square(ComponentSizeQuads+1)];
			FVector* LocalVerts = new FVector[Square(ComponentSizeQuads+1)];

			for( INT SubsectionY = 0;SubsectionY < NumSubsections;SubsectionY++ )
			{
				for( INT SubsectionX = 0;SubsectionX < NumSubsections;SubsectionX++ )
				{
					for( INT SubY=0;SubY<=SubsectionSizeQuads;SubY++ )
					{
						for( INT SubX=0;SubX<=SubsectionSizeQuads;SubX++ )
						{
							// X/Y of the vertex we're looking at in component's coordinates.
							INT CompX = SubsectionSizeQuads * SubsectionX + SubX;
							INT CompY = SubsectionSizeQuads * SubsectionY + SubY;

							// X/Y of the vertex we're looking indexed into the texture data
							INT TexX = (SubsectionSizeQuads+1) * SubsectionX + SubX;
							INT TexY = (SubsectionSizeQuads+1) * SubsectionY + SubY;

							INT WeightSrcDataIdx = CompY * (ComponentSizeQuads+1) + CompX;
							INT HeightTexDataIdx = (HeightmapOffsetX + TexX) + (HeightmapOffsetY + TexY) * (HeightmapInfo.HeightmapSizeU);

							INT WeightTexDataIdx = (TexX) + (TexY) * (WeightmapSize);

							// copy height and normal data
							WORD HeightValue = HEIGHTDATA(CompX + LandscapeComponent->SectionBaseX, CompY + LandscapeComponent->SectionBaseY);
							FVector Normal = VertexNormals[CompX+LandscapeComponent->SectionBaseX + (NumPatchesX+1)*(CompY+LandscapeComponent->SectionBaseY)].SafeNormal();

							HeightmapInfo.HeightmapTextureMipData(0)[HeightTexDataIdx].R = HeightValue >> 8;
							HeightmapInfo.HeightmapTextureMipData(0)[HeightTexDataIdx].G = HeightValue & 255;
							HeightmapInfo.HeightmapTextureMipData(0)[HeightTexDataIdx].B = appRound( 127.5f * (Normal.X + 1.f) );
							HeightmapInfo.HeightmapTextureMipData(0)[HeightTexDataIdx].A = appRound( 127.5f * (Normal.Y + 1.f) );

							for( INT WeightmapIndex=0;WeightmapIndex<WeightValues.Num(); WeightmapIndex++ )
							{
								WeightmapTextureDataPointers(WeightmapIndex)[WeightTexDataIdx*4] = WeightValues(WeightmapIndex)(WeightSrcDataIdx);
							}


							// Get local and world space verts
							FVector LocalVertex( CompX, CompY, LandscapeDataAccess::GetLocalHeight(HeightValue) );
							LocalVerts[(LandscapeComponent->ComponentSizeQuads+1) * CompY + CompX] = LocalVertex;

							FVector WorldVertex = LocalVertex + FVector(LandscapeComponent->SectionBaseX, LandscapeComponent->SectionBaseY, 0);
							WorldVertex *= DrawScale3D * DrawScale;
							WorldVertex += Location;
							WorldVerts[(LandscapeComponent->ComponentSizeQuads+1) * CompY + CompX] = WorldVertex;
						}
					}
				}
			}

			// This could give us a tighter sphere bounds than just adding the points one by one.
			LandscapeComponent->CachedLocalBox = FBox(LocalVerts, Square(ComponentSizeQuads+1));
			LandscapeComponent->CachedBoxSphereBounds = FBoxSphereBounds(WorldVerts, Square(ComponentSizeQuads+1));
			if( LandscapeComponent->CachedBoxSphereBounds.BoxExtent.Z < 32.f )
			{
				LandscapeComponent->CachedBoxSphereBounds.BoxExtent.Z = 32.f;
			}
			delete[] LocalVerts;
			delete[] WorldVerts;

			// Update MaterialInstance
			LandscapeComponent->UpdateMaterialInstances();

			// Register the new component with the landscape actor
			LandscapeComponent->SetupActor();
		}
	}


	// Unlock the weightmaps' base mips
	for( INT AllocationIndex=0;AllocationIndex<TextureAllocations.Num();AllocationIndex++ )
	{
		UTexture2D* WeightmapTexture = TextureAllocations(AllocationIndex).Texture;
		FColor* BaseMipData = TextureAllocations(AllocationIndex).TextureData;

		// Generate mips for weightmaps
		ULandscapeComponent::GenerateWeightmapMips(NumSubsections, SubsectionSizeQuads, WeightmapTexture, BaseMipData);

		WeightmapTexture->Mips(0).Data.Unlock();
		WeightmapTexture->UpdateResource();
	}


	delete[] VertexNormals;

	// Generate mipmaps for the components, and create the collision components
	for (INT ComponentY = 0; ComponentY < NumSectionsY; ComponentY++)
	{
		for (INT ComponentX = 0; ComponentX < NumSectionsX; ComponentX++)
		{
			INT HmX = ComponentX / ComponentsPerHeightmap;
			INT HmY = ComponentY / ComponentsPerHeightmap;
			FHeightmapInfo& HeightmapInfo = HeightmapInfos(HmX + HmY * NumHeightmapsX);

			ULandscapeComponent* LandscapeComponent = LandscapeComponents(ComponentX + ComponentY*NumSectionsX);
			LandscapeComponent->GenerateHeightmapMips(HeightmapInfo.HeightmapTextureMipData, ComponentX == NumSectionsX-1 ? MAXINT : 0, ComponentY == NumSectionsY-1 ? MAXINT : 0);
			LandscapeComponent->UpdateCollisionHeightData(HeightmapInfo.HeightmapTextureMipData(LandscapeComponent->CollisionMipLevel));
			LandscapeComponent->UpdateCollisionLayerData();
		}
	}

	for( INT HmIdx=0;HmIdx<HeightmapInfos.Num();HmIdx++ )
	{
		FHeightmapInfo& HeightmapInfo = HeightmapInfos(HmIdx);

		// Add remaining mips down to 1x1 to heightmap texture. These do not represent quads and are just a simple averages of the previous mipmaps. 
		// These mips are not used for sampling in the vertex shader but could be sampled in the pixel shader.
		INT Mip = HeightmapInfo.HeightmapTextureMipData.Num();
		INT MipSizeU = (HeightmapInfo.HeightmapTexture->SizeX) >> Mip;
		INT MipSizeV = (HeightmapInfo.HeightmapTexture->SizeY) >> Mip;
		while( MipSizeU > 1 && MipSizeV > 1 )
		{	
			// Create the mipmap
			FTexture2DMipMap* HeightmapMipMap = new(HeightmapInfo.HeightmapTexture->Mips) FTexture2DMipMap;
			HeightmapMipMap->SizeX = MipSizeU;
			HeightmapMipMap->SizeY = MipSizeV;
			HeightmapMipMap->Data.Lock(LOCK_READ_WRITE);
			HeightmapInfo.HeightmapTextureMipData.AddItem( (FColor*)HeightmapMipMap->Data.Realloc(MipSizeU*MipSizeV*sizeof(FColor)) );

			INT PrevMipSizeU = (HeightmapInfo.HeightmapTexture->SizeX) >> (Mip-1);
			INT PrevMipSizeV = (HeightmapInfo.HeightmapTexture->SizeY) >> (Mip-1);

			for( INT Y = 0;Y < MipSizeV;Y++ )
			{
				for( INT X = 0;X < MipSizeU;X++ )
				{
					FColor* TexData = &(HeightmapInfo.HeightmapTextureMipData(Mip))[ X + Y * MipSizeU ];

					FColor *PreMipTexData00 = &(HeightmapInfo.HeightmapTextureMipData(Mip-1))[ (X*2+0) + (Y*2+0)  * PrevMipSizeU ];
					FColor *PreMipTexData01 = &(HeightmapInfo.HeightmapTextureMipData(Mip-1))[ (X*2+0) + (Y*2+1)  * PrevMipSizeU ];
					FColor *PreMipTexData10 = &(HeightmapInfo.HeightmapTextureMipData(Mip-1))[ (X*2+1) + (Y*2+0)  * PrevMipSizeU ];
					FColor *PreMipTexData11 = &(HeightmapInfo.HeightmapTextureMipData(Mip-1))[ (X*2+1) + (Y*2+1)  * PrevMipSizeU ];

					TexData->R = (((INT)PreMipTexData00->R + (INT)PreMipTexData01->R + (INT)PreMipTexData10->R + (INT)PreMipTexData11->R) >> 2);
					TexData->G = (((INT)PreMipTexData00->G + (INT)PreMipTexData01->G + (INT)PreMipTexData10->G + (INT)PreMipTexData11->G) >> 2);
					TexData->B = (((INT)PreMipTexData00->B + (INT)PreMipTexData01->B + (INT)PreMipTexData10->B + (INT)PreMipTexData11->B) >> 2);
					TexData->A = (((INT)PreMipTexData00->A + (INT)PreMipTexData01->A + (INT)PreMipTexData10->A + (INT)PreMipTexData11->A) >> 2);
				}
			}

			Mip++;
			MipSizeU >>= 1;
			MipSizeV >>= 1;
		}

		for( INT i=0;i<HeightmapInfo.HeightmapTextureMipData.Num();i++ )
		{
			HeightmapInfo.HeightmapTexture->Mips(i).Data.Unlock();
		}
		HeightmapInfo.HeightmapTexture->UpdateResource();
	}

	// Update our new components
	ConditionalUpdateComponents();

	// Init RB physics for editor collision
	InitRBPhysEditor();

	// Add Collision data update
	LandscapeInfo->UpdateAllAddCollisions();
	LandscapeInfo->HeightmapFilePath = HeightmapFileName;

	bIsSetup = TRUE;

	GWarn->EndSlowTask();
}

namespace
{
	FORCEINLINE void GetComponentExtent(ULandscapeComponent* Comp, INT& MinX, INT& MinY, INT& MaxX, INT& MaxY)
	{
		if (Comp)
		{
			if( Comp->SectionBaseX < MinX )
			{
				MinX = Comp->SectionBaseX;
			}
			if( Comp->SectionBaseY < MinY )
			{
				MinY = Comp->SectionBaseY;
			}
			if( Comp->SectionBaseX + Comp->ComponentSizeQuads > MaxX )
			{
				MaxX = Comp->SectionBaseX + Comp->ComponentSizeQuads;
			}
			if( Comp->SectionBaseY + Comp->ComponentSizeQuads > MaxY )
			{
				MaxY = Comp->SectionBaseY + Comp->ComponentSizeQuads;
			}
		}
	}
};

UBOOL ALandscape::HasAllComponent()
{
	ULandscapeInfo* Info = GetLandscapeInfo();
	if (Info && Info->XYtoComponentMap.Num() == LandscapeComponents.Num())
	{
		// all components are owned by this Landscape actor (no Landscape Proxies)
		return TRUE;
	}
	return FALSE;
}

UBOOL ULandscapeInfo::GetLandscapeExtent(INT& MinX, INT& MinY, INT& MaxX, INT& MaxY)
{
	MinX=MAXINT;
	MinY=MAXINT;
	MaxX=MININT;
	MaxY=MININT;

	// Find range of entire landscape
	for (TMap<QWORD, ULandscapeComponent*>::TIterator It(XYtoComponentMap); It; ++It)
	{
		ULandscapeComponent* Comp = It.Value();
		GetComponentExtent(Comp, MinX, MinY, MaxX, MaxY);
	}
	return (MinX != MAXINT);
}

UBOOL ULandscapeInfo::GetSelectedExtent(INT& MinX, INT& MinY, INT& MaxX, INT& MaxY)
{
	MinX = MinY = MAXINT;
	MaxX = MaxY = MININT;
	for (TMap<QWORD, FLOAT>::TIterator It(SelectedRegion); It; ++It)
	{
		INT X, Y;
		ALandscape::UnpackKey(It.Key(), X, Y);
		if (MinX > X) MinX = X;
		if (MaxX < X) MaxX = X;
		if (MinY > Y) MinY = Y;
		if (MaxY < Y) MaxY = Y;
	}
	if (MinX != MAXINT)
	{
		return TRUE;
	}
	// if SelectedRegion is empty, try SelectedComponents
	for (TSet<ULandscapeComponent*>::TIterator It(SelectedComponents); It; ++It)
	{
		ULandscapeComponent* Comp = *It;
		GetComponentExtent(Comp, MinX, MinY, MaxX, MaxY);
	}
	return MinX != MAXINT;
}

FVector ULandscapeInfo::GetLandscapeCenterPos(FLOAT& LengthZ, INT MinX /*= MAX_INT*/, INT MinY /*= MAX_INT*/, INT MaxX /*= MIN_INT*/, INT MaxY /*= MIN_INT*/ )
{
	if (!LandscapeProxy)
	{
		return FVector::ZeroVector;
	}
	// MinZ, MaxZ is Local coordinate
	FLOAT MaxZ = -HALF_WORLD_MAX, MinZ = HALF_WORLD_MAX;
	FLOAT ScaleZ = LandscapeProxy->DrawScale3D.Z * LandscapeProxy->DrawScale;
	
	if (MinX == MAXINT)
	{
		// Find range of entire landscape
		for (TMap<QWORD, ULandscapeComponent*>::TIterator It(XYtoComponentMap); It; ++It)
		{
			ULandscapeComponent* Comp = It.Value();
			GetComponentExtent(Comp, MinX, MinY, MaxX, MaxY);
		}

		const INT Dist = (LandscapeProxy->ComponentSizeQuads+1) >> 1; // Should be same in ALandscapeGizmoActiveActor::SetTargetLandscape
		FVector2D MidPoint(((FLOAT)(MinX+MaxX))/2.f, ((FLOAT)(MinY+MaxY))/2.f);
		MinX = appFloor(MidPoint.X) - Dist;
		MaxX = appCeil(MidPoint.X) + Dist;
		MinY = appFloor(MidPoint.Y) - Dist;
		MaxY = appCeil(MidPoint.Y) + Dist;
		check(MidPoint.X == ((FLOAT)(MinX+MaxX))/2.f && MidPoint.Y == ((FLOAT)(MinY+MaxY))/2.f );
	}

	check(MinX != MAXINT);
	//if (MinX != MAXINT)
	{
		INT CompX1, CompX2, CompY1, CompY2;
		ALandscape::CalcComponentIndicesOverlap(MinX, MinY, MaxX, MaxY, LandscapeProxy->ComponentSizeQuads, CompX1, CompY1, CompX2, CompY2);
		for (INT IndexY = CompY1; IndexY <= CompY2; ++IndexY)
		{
			for (INT IndexX = CompX1; IndexX <= CompX2; ++IndexX)
			{
				ULandscapeHeightfieldCollisionComponent* Comp = XYtoCollisionComponentMap.FindRef(ALandscape::MakeKey(IndexX*LandscapeProxy->ComponentSizeQuads, IndexY*LandscapeProxy->ComponentSizeQuads));
				if (Comp)
				{
					WORD* Heights = (WORD*)Comp->CollisionHeightData.Lock(LOCK_READ_ONLY);
					INT CollisionSizeVerts = Comp->CollisionSizeQuads + 1;

					INT StartX = Max(0, MinX - Comp->SectionBaseX);
					INT StartY = Max(0, MinY - Comp->SectionBaseY);
					INT EndX = Min(CollisionSizeVerts, MaxX - Comp->SectionBaseX + 1);
					INT EndY = Min(CollisionSizeVerts, MaxY - Comp->SectionBaseY + 1);

					for (INT Y = StartY; Y < EndY; ++Y)
					{
						for (INT X = StartX; X < EndX; ++X)
						{
							FLOAT Height = LandscapeDataAccess::GetLocalHeight(Heights[X + Y*CollisionSizeVerts]);
							MaxZ = Max(Height, MaxZ);
							MinZ = Min(Height, MinZ);
						}
					}
					Comp->CollisionHeightData.Unlock();
				}
			}
		}
	}

	const FLOAT MarginZ = 3;
	if (MaxZ < MinZ)
	{
		MaxZ = +MarginZ;
		MinZ = -MarginZ;
	}
	LengthZ = (MaxZ - MinZ + 2*MarginZ) * ScaleZ;
	return LandscapeProxy->LocalToWorld().TransformFVector( FVector( ((FLOAT)(MinX+MaxX))/2.f, ((FLOAT)(MinY+MaxY))/2.f, MinZ - MarginZ) );
}

UBOOL ULandscapeInfo::IsValidPosition(INT X, INT Y)
{
	if (!LandscapeProxy)
	{
		return FALSE;
	}
	INT CompX1, CompX2, CompY1, CompY2;
	ALandscape::CalcComponentIndicesOverlap(X, Y, X, Y, LandscapeProxy->ComponentSizeQuads, CompX1, CompY1, CompX2, CompY2);
	if (XYtoComponentMap.FindRef(ALandscape::MakeKey(CompX1*LandscapeProxy->ComponentSizeQuads, CompY1*LandscapeProxy->ComponentSizeQuads)))
	{
		return TRUE;
	}
	if (XYtoComponentMap.FindRef(ALandscape::MakeKey(CompX2*LandscapeProxy->ComponentSizeQuads, CompY2*LandscapeProxy->ComponentSizeQuads)))
	{
		return TRUE;
	}
	return FALSE;
}

void ULandscapeInfo::Export(TArray<FName>& Layernames, TArray<FString>& Filenames)
{
	check( Filenames.Num() > 0 );

	INT MinX=MAXINT;
	INT MinY=MAXINT;
	INT MaxX=-MAXINT;
	INT MaxY=-MAXINT;

	if( !GetLandscapeExtent(MinX,MinY,MaxX,MaxY) )
	{
		return;
	}

	GWarn->BeginSlowTask( TEXT("Exporting Landscape"), TRUE);

	FLandscapeEditDataInterface LandscapeEdit(this);

	TArray<BYTE> HeightData;
	HeightData.AddZeroed((1+MaxX-MinX)*(1+MaxY-MinY)*sizeof(WORD));
	LandscapeEdit.GetHeightDataFast(MinX,MinY,MaxX,MaxY,(WORD*)&HeightData(0),0);
	appSaveArrayToFile(HeightData,*Filenames(0));

	for( INT i=1;i<Filenames.Num();i++ )
	{
		if ( i <= Layernames.Num())
		{
			TArray<BYTE> WeightData;
			WeightData.AddZeroed((1+MaxX-MinX)*(1+MaxY-MinY));
			LandscapeEdit.GetWeightDataFast(Layernames(i-1), MinX,MinY,MaxX,MaxY,&WeightData(0),0);
			appSaveArrayToFile(WeightData,*Filenames(i));
		}
	}

	GWarn->EndSlowTask();
}

void ULandscapeInfo::DeleteLayer(FName LayerName)
{
	GWarn->BeginSlowTask( TEXT("Deleting Layer"), TRUE);

	// Remove data from all components
	FLandscapeEditDataInterface LandscapeEdit(this);
	LandscapeEdit.DeleteLayer(LayerName);

	// Remove from array
	if (LandscapeProxy)
	{
		for (INT i = 0; i < LandscapeProxy->LayerInfoObjs.Num(); ++i )
		{
			if (LandscapeProxy->LayerInfoObjs(i).LayerInfoObj && LandscapeProxy->LayerInfoObjs(i).LayerInfoObj->LayerName == LayerName)
			{
				LandscapeProxy->LayerInfoObjs.Remove(i);
				break;
			}
		}
	}

	LayerInfoMap.RemoveKey(LayerName);
	UpdateLayerInfoMap();

	GWarn->EndSlowTask();
}

void ALandscapeProxy::InitRBPhysEditor()
{
	InitRBPhys();
}

void ALandscapeProxy::PostEditMove(UBOOL bFinished)
{
	// This point is only reached when Copy and Pasted
	Super::PostEditMove(bFinished);
	if( bFinished )
	{
		ULandscapeInfo* Info = GetLandscapeInfo();
		if (Info && Info->LandscapeProxy == this)
		{
			Info->GetSharedProperties(this);
		}

		UpdateLandscapeActor(NULL);
		WeightmapUsageMap.Empty();
		for(INT ComponentIndex = 0; ComponentIndex < LandscapeComponents.Num(); ComponentIndex++ )
		{
			ULandscapeComponent* Comp = LandscapeComponents(ComponentIndex);
			if( Comp )
			{
				Comp->SetupActor();
				Comp->UpdateMaterialInstances();
				if (Comp->EditToolRenderData)
				{
					Comp->EditToolRenderData->UpdateDebugColorMaterial();
				}

				Comp->UpdateCachedBounds();
				Comp->UpdateBounds();

				FComponentReattachContext ReattachContext(Comp);
			}
		}

		for(INT ComponentIndex = 0; ComponentIndex < CollisionComponents.Num(); ComponentIndex++ )
		{
			ULandscapeHeightfieldCollisionComponent* Comp = CollisionComponents(ComponentIndex);
			if( Comp )
			{
				Comp->RecreateHeightfield();
			}
		}

		ConditionalUpdateComponents();

		bIsSetup = TRUE;
	}
}

void ALandscapeProxy::PostEditImport()
{
	Super::PostEditImport();

	if (!bIsProxy && GWorld) // For Landscape
	{
		for (FActorIterator It; It; ++It)
		{
			ALandscape* Landscape = Cast<ALandscape>(*It);
			if (Landscape && Landscape != this && !Landscape->HasAnyFlags(RF_BeginDestroyed) && Landscape->LandscapeGuid == LandscapeGuid)
			{
				// Copy/Paste case, need to generate new GUID
				LandscapeGuid = appCreateGuid();
				Landscape->bResetup = TRUE;
			}
		}
	}

	if (GIsEditor)
	{
		bResetup = FALSE; // for Landscape Proxy, need to check component collision when pasting
		GEngine->DeferredCommands.AddUniqueItem(TEXT("UpdateLandscapeSetup"));
	}
}

void ALandscape::PostEditMove(UBOOL bFinished)
{
	Super::PostEditMove(bFinished);
	if( bFinished )
	{
		ULandscapeInfo* Info = GetLandscapeInfo();
		if (Info)
		{
			Info->GetSharedProperties(this);
			UBOOL bNewInfo = !Info->Proxies.Num();
			//Info->Proxies.Empty();

			// PostEditMove is called when Pasting a cut landscape.
			// In this situation we need to regenerate Proxies
			// TODO: move it to some function not related to movement, now we must support Widget movement.
			if (bNewInfo && GWorld)
			{
				for (FActorIterator It; It; ++It)
				{
					ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
					if (Proxy && Proxy->bIsProxy && Proxy->IsValidLandscapeActor(this))
					{
						Proxy->UpdateLandscapeActor(this);
						Proxy->WeightmapUsageMap.Empty();

						for(INT ComponentIndex = 0; ComponentIndex < Proxy->LandscapeComponents.Num(); ComponentIndex++ )
						{
							ULandscapeComponent* Comp = Proxy->LandscapeComponents(ComponentIndex);
							if( Comp )
							{
								Comp->SetupActor();
								Comp->UpdateMaterialInstances();
								if (Comp->EditToolRenderData)
								{
									Comp->EditToolRenderData->UpdateDebugColorMaterial();
								}

								FComponentReattachContext ReattachContext(Comp);
							}
						}
					}
				}
			}
			else
			{
				for (TSet<ALandscapeProxy*>::TIterator It(Info->Proxies); It; ++It)
				{
					ALandscapeProxy* Proxy = *It;
					Proxy->UpdateLandscapeActor(this);

					for(INT ComponentIndex = 0; ComponentIndex < Proxy->LandscapeComponents.Num(); ComponentIndex++ )
					{
						ULandscapeComponent* Comp = Proxy->LandscapeComponents(ComponentIndex);
						if( Comp )
						{
							Comp->UpdateCachedBounds();
							Comp->UpdateBounds();

							FComponentReattachContext ReattachContext(Comp);
						}
					}

					for(INT ComponentIndex = 0; ComponentIndex < Proxy->CollisionComponents.Num(); ComponentIndex++ )
					{
						ULandscapeHeightfieldCollisionComponent* Comp = Proxy->CollisionComponents(ComponentIndex);
						if( Comp )
						{
							Comp->RecreateHeightfield();
						}
					}

					Proxy->ConditionalForceUpdateComponents();
				}
			}
		}
	}
}

void ULandscapeComponent::SetLOD(UBOOL bForcedLODChanged, INT InLODValue)
{
	if (bForcedLODChanged)
	{
		ForcedLOD = InLODValue;
		if (ForcedLOD >= 0)
		{
			ForcedLOD = Clamp<INT>( ForcedLOD, 0, appCeilLogTwo(SubsectionSizeQuads+1)-1 );
		}
		else
		{
			ForcedLOD = -1;
		}
	}
	else
	{
		INT MaxLOD = appCeilLogTwo(SubsectionSizeQuads+1)-1;
		LODBias = Clamp<INT>(InLODValue, -MaxLOD, MaxLOD);
	}
	// Update neighbor components
	ULandscapeInfo* Info = GetLandscapeInfo(FALSE);
	if (Info)
	{
		QWORD LandscapeKey[8] = 
		{
			ALandscape::MakeKey(SectionBaseX-ComponentSizeQuads,	SectionBaseY-ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX,						SectionBaseY-ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX+ComponentSizeQuads,	SectionBaseY-ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX-ComponentSizeQuads,	SectionBaseY),
			ALandscape::MakeKey(SectionBaseX+ComponentSizeQuads,	SectionBaseY),
			ALandscape::MakeKey(SectionBaseX-ComponentSizeQuads,	SectionBaseY+ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX,						SectionBaseY+ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX+ComponentSizeQuads,	SectionBaseY+ComponentSizeQuads)
		};

		for (INT Idx = 0; Idx < 8; ++Idx)
		{
			ULandscapeComponent* Comp = Info->XYtoComponentMap.FindRef(LandscapeKey[Idx]);
			if (Comp)
			{
				Comp->Modify();
				if (bForcedLODChanged)
				{
					Comp->NeighborLOD[7-Idx] = ForcedLOD >= 0 ? ForcedLOD : 255; // Use 255 as unspecified value
				}
				else
				{
					Comp->NeighborLODBias[7-Idx] = LODBias + 128;
				}
				FComponentReattachContext ReattachContext(Comp);
			}
		}
	}
	FComponentReattachContext ReattachContext(this);
}

void ULandscapeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if( PropertyName == FName(TEXT("OverrideMaterial")) )
	{
		UpdateMaterialInstances();
		// Reattach all components
		FComponentReattachContext ReattachContext(this);
	}
	else if ( GIsEditor && (PropertyName == FName(TEXT("ForcedLOD")) || PropertyName == FName(TEXT("LODBias"))) )
	{
		UBOOL bForcedLODChanged = PropertyName == FName(TEXT("ForcedLOD"));
		SetLOD(bForcedLODChanged, bForcedLODChanged ? ForcedLOD : LODBias);
	}
}

void ULandscapeLayerInfoObject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if ( GIsEditor && PropertyName == FName(TEXT("Hardness")) )
	{
		Hardness = Clamp<FLOAT>(Hardness, 0.f, 1.f);
	}
}

void ULandscapeLayerInfoObject::PostLoad()
{
	Super::PostLoad();
	if (GIsEditor)
	{
		if (!HasAnyFlags(RF_Standalone))
		{
			SetFlags(RF_Standalone);
		}
		Hardness = Clamp<FLOAT>(Hardness, 0.f, 1.f);
	}
}

void ALandscapeProxy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (bIsProxy)
	{
		if( PropertyName == FName(TEXT("LandscapeActor")) )
		{
			if (LandscapeActor && IsValidLandscapeActor(LandscapeActor))
			{
				UpdateLandscapeActor(LandscapeActor);
				WeightmapUsageMap.Empty();

				for(INT ComponentIndex = 0; ComponentIndex < LandscapeComponents.Num(); ComponentIndex++ )
				{
					ULandscapeComponent* Comp = LandscapeComponents(ComponentIndex);
					if( Comp )
					{
						Comp->SetupActor();
						Comp->UpdateCachedBounds();
						Comp->UpdateBounds();
						// Update the MIC
						Comp->UpdateMaterialInstances();
						// Reattach all components
						FComponentReattachContext ReattachContext(Comp);
					}
				}

				// Update collision
				for(INT ComponentIndex = 0; ComponentIndex < CollisionComponents.Num(); ComponentIndex++ )
				{
					ULandscapeHeightfieldCollisionComponent* Comp = CollisionComponents(ComponentIndex);
					if( Comp )
					{
						Comp->SetupActor();
						Comp->RecreateHeightfield(FALSE);
					}
				}
				ConditionalUpdateComponents();
				GEngine->DeferredCommands.AddUniqueItem(TEXT("UpdateAddLandscapeComponents"));
			}
			else
			{
				LandscapeActor = NULL;
			}
		}
		else if( PropertyName == FName(TEXT("LandscapeMaterial")) )
		{
			// Clear the parents out of combination material instances
			for( TMap<FString ,UMaterialInstanceConstant*>::TIterator It(MaterialInstanceConstantMap); It; ++It )
			{
				It.Value()->SetParent(NULL);
			}

			// Remove our references to any material instances
			MaterialInstanceConstantMap.Empty();

			for(INT ComponentIndex = 0; ComponentIndex < LandscapeComponents.Num(); ComponentIndex++ )
			{
				ULandscapeComponent* Comp = LandscapeComponents(ComponentIndex);
				if( Comp )
				{
					// Update the MIC
					Comp->UpdateMaterialInstances();
					// Reattach all components
					FComponentReattachContext ReattachContext(Comp);
				}
			}
		}
	}

	if ( GIsEditor && PropertyName == FName(TEXT("StreamingDistanceMultiplier")) )
	{
		// Recalculate in a few seconds.
		ULevel::TriggerStreamingDataRebuild();
	}
	else if ( GIsEditor && PropertyName == FName(TEXT("DefaultPhysMaterial")) )
	{
		ChangedPhysMaterial();
	}
	else if ( GIsEditor && (PropertyName == FName(TEXT("CollisionMipLevel"))) )
	{
		CollisionMipLevel = Clamp<INT>( CollisionMipLevel, 0, appCeilLogTwo(SubsectionSizeQuads+1)-1 );
		for (INT i = 0; i < LandscapeComponents.Num(); ++i)
		{
			ULandscapeComponent* Comp = LandscapeComponents(i);
			if (Comp)
			{
				Comp->CollisionMipLevel = CollisionMipLevel;
				Comp->UpdateCollisionHeightData((FColor*)Comp->HeightmapTexture->Mips(CollisionMipLevel).Data.Lock(LOCK_READ_ONLY), 0, 0, MAXINT, MAXINT, TRUE, TRUE); // Rebuild for new CollisionMipLevel
				Comp->HeightmapTexture->Mips(CollisionMipLevel).Data.Unlock();
			}
		}
	}
}

void ALandscape::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	UBOOL ChangedMaterial = FALSE;
	UBOOL NeedsRecalcBoundingBox = FALSE;
	UBOOL NeedChangeLighting = FALSE;
	UBOOL NeedPropergateProxies = FALSE;

	ULandscapeInfo* Info = GetLandscapeInfo();

	if( PropertyName == FName(TEXT("LandscapeMaterial")) )
	{
		if (Info)
		{
			for (TMap<FName, FLandscapeLayerStruct*>::TIterator It(Info->LayerInfoMap); It; ++It )
			{
				It.Value()->ThumbnailMIC = NULL;
			}
		}

		ChangedMaterial = TRUE;

		// Clear the parents out of combination material instances
		for( TMap<FString ,UMaterialInstanceConstant*>::TIterator It(MaterialInstanceConstantMap); It; ++It )
		{
			It.Value()->SetParent(NULL);
		}
		
		// Remove our references to any material instances
		MaterialInstanceConstantMap.Empty();
	}
	else if( PropertyName == FName(TEXT("DrawScale")) ||
			 PropertyName == FName(TEXT("DrawScale3D")) ||
			 PropertyName == FName(TEXT("X")) ||
			 PropertyName == FName(TEXT("Y")) ||
			 PropertyName == FName(TEXT("Z")) ||
			 PropertyName == FName(TEXT("Pitch")) ||
			 PropertyName == FName(TEXT("Yaw")) ||
			 PropertyName == FName(TEXT("Roll")) )
	{
		NeedsRecalcBoundingBox = TRUE;
	}
	else if ( GIsEditor && PropertyName == FName(TEXT("MaxLODLevel")) )
	{
		NeedPropergateProxies = TRUE;
	}
	else if ( PropertyName == FName(TEXT("LODDistanceFactor")) )
	{
		LODDistanceFactor = Clamp<FLOAT>(LODDistanceFactor, 0.1f, 3.f); // limit because LOD transition became too popping...
		NeedPropergateProxies = TRUE;
	}
	else if ( PropertyName == FName(TEXT("CollisionMipLevel")) )
	{
		CollisionMipLevel = Clamp<INT>( CollisionMipLevel, 0, appCeilLogTwo(SubsectionSizeQuads+1)-1 );
		NeedPropergateProxies = TRUE;
	}
	else if ( GIsEditor && PropertyName == FName(TEXT("StaticLightingResolution")) )
	{
		// Change Lighting resolution to proper one...
		if (StaticLightingResolution > 1.f)
		{
			StaticLightingResolution = (INT)StaticLightingResolution;
		}
		else if (StaticLightingResolution < 1.f)
		{
			// Restrict to 1/16
			if (StaticLightingResolution < 0.0625)
			{
				StaticLightingResolution = 0.0625;
			}

			// Adjust to 1/2^n
			INT i = 2;
			INT LightmapSize = (NumSubsections * (SubsectionSizeQuads + 1)) >> 1;
			while ( StaticLightingResolution < (1.f/i) && LightmapSize > 4  )
			{
				i <<= 1;
				LightmapSize >>= 1;
			}
			StaticLightingResolution = 1.f / i;

			INT PixelPaddingX = GPixelFormats[PF_DXT1].BlockSizeX;

			INT DestSize = (INT)((2*PixelPaddingX + ComponentSizeQuads + 1) * StaticLightingResolution);
			StaticLightingResolution = (FLOAT)DestSize / (2*PixelPaddingX + ComponentSizeQuads + 1 );
		}
		NeedChangeLighting = TRUE;
	}

	NeedPropergateProxies = NeedPropergateProxies || NeedsRecalcBoundingBox || NeedChangeLighting;

	if (Info)
	{
		if (NeedsRecalcBoundingBox || NeedChangeLighting)
		{
			Info->GetSharedProperties(this);
		}

		if (NeedPropergateProxies)
		{
			// Propagate Event to Proxies...
			for (TSet<ALandscapeProxy*>::TIterator It(Info->Proxies); It; ++It )
			{
				(*It)->GetSharedProperties(this);
				(*It)->PostEditChangeProperty(PropertyChangedEvent);
			}
		}

		// Update normals if DrawScale3D is changed
		if( PropertyName == FName(TEXT("DrawScale3D")) )
		{
			FLandscapeEditDataInterface LandscapeEdit(Info);
			LandscapeEdit.RecalculateNormals();
		}

		for (TMap<QWORD, ULandscapeComponent*>::TIterator It(Info->XYtoComponentMap); It; ++It )
		{
			ULandscapeComponent* Comp = It.Value();
			if( Comp )
			{
				if( NeedsRecalcBoundingBox )
				{
					Comp->UpdateCachedBounds();
					Comp->UpdateBounds();
				}

				if( ChangedMaterial )
				{
					// Update the MIC
					Comp->UpdateMaterialInstances();
				}

				// Reattach all components
				FComponentReattachContext ReattachContext(Comp);
			}
		}

		// Update collision
		if( NeedsRecalcBoundingBox )
		{
			for (TMap<QWORD, ULandscapeHeightfieldCollisionComponent*>::TIterator It(Info->XYtoCollisionComponentMap); It; ++It )
			{
				ULandscapeHeightfieldCollisionComponent* Comp = It.Value();
				if( Comp )
				{
					Comp->RecreateHeightfield();
				}
			}
		}

		if (ChangedMaterial)
		{
			// Update all the proxies...
			for ( TSet<ALandscapeProxy*>::TIterator It(Info->Proxies); It; ++It )
			{
				(*It)->ConditionalUpdateComponents();
			}
		}
	}
}

void ALandscape::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	UBOOL NeedsRecalcBoundingBox = FALSE;
	UBOOL NeedsUpdateNormals = FALSE;
	ULandscapeInfo* Info = GetLandscapeInfo();

	if ( PropertyChangedEvent.PropertyChain.Num() > 0 )
	{
		UProperty* OutermostProperty = PropertyChangedEvent.PropertyChain.GetHead()->GetValue();
		if ( OutermostProperty != NULL )
		{
			const FName PropertyName = OutermostProperty->GetFName();
			if( PropertyName == FName(TEXT("DrawScale3D")) )
			{
				NeedsRecalcBoundingBox = TRUE;
				NeedsUpdateNormals = TRUE;
			}
		}
	}

	if( NeedsUpdateNormals )
	{
		if (Info)
		{
			FLandscapeEditDataInterface LandscapeEdit(Info);
			LandscapeEdit.RecalculateNormals();
		}
	}

	if( NeedsRecalcBoundingBox )
	{
		if (Info)
		{
			for (TMap<QWORD, ULandscapeComponent*>::TIterator It(Info->XYtoComponentMap); It; ++It )
			{
				ULandscapeComponent* Comp = It.Value();
				if( Comp )
				{
					Comp->UpdateCachedBounds();
					Comp->UpdateBounds();

					// Reattach all components
					FComponentReattachContext ReattachContext(Comp);
				}
			}

			for (TMap<QWORD, ULandscapeHeightfieldCollisionComponent*>::TIterator It(Info->XYtoCollisionComponentMap); It; ++It )
			{
				ULandscapeHeightfieldCollisionComponent* Comp = It.Value();
				if( Comp )
				{
					Comp->RecreateHeightfield();
				}
			}
		}
	}
}

void ALandscapeProxy::ChangedPhysMaterial()
{
	ULandscapeInfo* LandscapeInfo = GetLandscapeInfo();
	if (!LandscapeInfo) return;
	for( TMap<QWORD,ULandscapeComponent*>::TIterator It(LandscapeInfo->XYtoComponentMap); It; ++It )
	{
		ULandscapeComponent* Comp = It.Value();
		if( Comp )
		{
			ULandscapeHeightfieldCollisionComponent* CollisionComponent = Comp->GetCollisionComponent();
			if( CollisionComponent )
			{
				Comp->UpdateCollisionLayerData();
			}
		}
	}
}

void ULandscapeInfo::UpdateSelectedComponents(TSet<ULandscapeComponent*>& NewComponents, UBOOL bIsComponentwise /*=TRUE*/)
{
	INT InSelectType = bIsComponentwise ? FLandscapeEditToolRenderData::ST_COMPONENT : FLandscapeEditToolRenderData::ST_REGION;

	if (bIsComponentwise)
	{
		SelectedCollisionComponents.Empty();
		for( TSet<ULandscapeComponent*>::TIterator It(NewComponents); It; ++It )
		{
			ULandscapeComponent* Comp = *It;
			if( Comp->EditToolRenderData != NULL )
			{
				INT SelectedType = Comp->EditToolRenderData->SelectedType;
				SelectedType |= InSelectType;
				Comp->EditToolRenderData->UpdateSelectionMaterial(SelectedType);
			}

			// Update SelectedCollisionComponents
			ULandscapeHeightfieldCollisionComponent* CollisionComponent = XYtoCollisionComponentMap.FindRef(ALandscape::MakeKey((*It)->SectionBaseX, (*It)->SectionBaseY));
			if (CollisionComponent)
			{
				SelectedCollisionComponents.Add(CollisionComponent);
			}
		}

		// Remove the material from any old components that are no longer in the region
		TSet<ULandscapeComponent*> RemovedComponents = SelectedComponents.Difference(NewComponents);
		for ( TSet<ULandscapeComponent*>::TIterator It(RemovedComponents); It; ++It )
		{
			ULandscapeComponent* Comp = *It;
			if( Comp->EditToolRenderData != NULL )
			{
				INT SelectedType = Comp->EditToolRenderData->SelectedType;
				SelectedType &= ~InSelectType;
				Comp->EditToolRenderData->UpdateSelectionMaterial(SelectedType);
			}
		}
		SelectedComponents = NewComponents;	
	}
	else
	{
		// Only add components...
		if (NewComponents.Num())
		{
			for( TSet<ULandscapeComponent*>::TIterator It(NewComponents); It; ++It )
			{
				ULandscapeComponent* Comp = *It;
				if( Comp->EditToolRenderData != NULL )
				{
					INT SelectedType = Comp->EditToolRenderData->SelectedType;
					SelectedType |= InSelectType;
					Comp->EditToolRenderData->UpdateSelectionMaterial(SelectedType);
				}

				SelectedRegionComponents.Add(*It);
			}
		}
		else
		{
			// Remove the material from any old components that are no longer in the region
			for ( TSet<ULandscapeComponent*>::TIterator It(SelectedRegionComponents); It; ++It )
			{
				ULandscapeComponent* Comp = *It;
				if( Comp->EditToolRenderData != NULL )
				{
					INT SelectedType = Comp->EditToolRenderData->SelectedType;
					SelectedType &= ~InSelectType;
					Comp->EditToolRenderData->UpdateSelectionMaterial(SelectedType);
				}
			}
			SelectedRegionComponents = NewComponents;
		}
	}
}

IMPLEMENT_COMPARE_CONSTPOINTER( ULandscapeComponent, Landscape, { return (A->SectionBaseX == B->SectionBaseX) ? (A->SectionBaseY - B->SectionBaseY) : (A->SectionBaseX - B->SectionBaseX); } );
IMPLEMENT_COMPARE_CONSTPOINTER( ULandscapeHeightfieldCollisionComponent, Landscape, { return (A->SectionBaseX == B->SectionBaseX) ? (A->SectionBaseY - B->SectionBaseY) : (A->SectionBaseX - B->SectionBaseX); } );

void ULandscapeInfo::SortSelectedComponents()
{
	SelectedComponents.Sort<COMPARE_CONSTPOINTER_CLASS(ULandscapeComponent, Landscape)>();
	SelectedCollisionComponents.Sort<COMPARE_CONSTPOINTER_CLASS(ULandscapeHeightfieldCollisionComponent, Landscape)>();
}

void ULandscapeInfo::ClearSelectedRegion(UBOOL bIsComponentwise /*= TRUE*/)
{
	TSet<ULandscapeComponent*> NewComponents;
	UpdateSelectedComponents(NewComponents, bIsComponentwise);
	if (!bIsComponentwise)
	{
		SelectedRegion.Empty();
	}
}

struct FLandscapeDataInterface* ULandscapeInfo::GetDataInterface()
{
	if( DataInterface == NULL )
	{ 
		DataInterface = new FLandscapeDataInterface();
	}

	return DataInterface;
}

void ULandscapeComponent::ReallocateWeightmaps(FLandscapeEditDataInterface* DataInterface)
{
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	
	INT NeededNewChannels=0;
	for( INT LayerIdx=0;LayerIdx < WeightmapLayerAllocations.Num();LayerIdx++ )
	{
		if( WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex == 255 )
		{
			NeededNewChannels++;
		}
	}

	// All channels allocated!
	if( NeededNewChannels == 0 )
	{
		return;
	}

	Modify();
	//Landscape->Modify();
	Proxy->Modify();

	// debugf(TEXT("----------------------"));
	// debugf(TEXT("Component %s needs %d layers (%d new)"), *GetName(), WeightmapLayerAllocations.Num(), NeededNewChannels);

	// See if our existing textures have sufficient space
	INT ExistingTexAvailableChannels=0;
	for( INT TexIdx=0;TexIdx<WeightmapTextures.Num();TexIdx++ )
	{
		FLandscapeWeightmapUsage* Usage = Proxy->WeightmapUsageMap.Find(WeightmapTextures(TexIdx));
		check(Usage);

		ExistingTexAvailableChannels += Usage->FreeChannelCount();

		if( ExistingTexAvailableChannels >= NeededNewChannels )
		{
			break;
		}
	}
	
	if( ExistingTexAvailableChannels >= NeededNewChannels )
	{
		// debugf(TEXT("Existing texture has available channels"));

		// Allocate using our existing textures' spare channels.
		for( INT TexIdx=0;TexIdx<WeightmapTextures.Num();TexIdx++ )
		{
			FLandscapeWeightmapUsage* Usage = Proxy->WeightmapUsageMap.Find(WeightmapTextures(TexIdx));
			
			for( INT ChanIdx=0;ChanIdx<4;ChanIdx++ )
			{
				if( Usage->ChannelUsage[ChanIdx]==NULL )
				{
					for( INT LayerIdx=0;LayerIdx < WeightmapLayerAllocations.Num();LayerIdx++ )
					{
						FWeightmapLayerAllocationInfo& AllocInfo = WeightmapLayerAllocations(LayerIdx);
						if( AllocInfo.WeightmapTextureIndex == 255 )
						{
							// Zero out the data for this texture channel
							if( DataInterface )
							{
								DataInterface->ZeroTextureChannel( WeightmapTextures(TexIdx), ChanIdx );
							}

							AllocInfo.WeightmapTextureIndex = TexIdx;
							AllocInfo.WeightmapTextureChannel = ChanIdx;
							Usage->ChannelUsage[ChanIdx] = this;
							NeededNewChannels--;

							if( NeededNewChannels == 0 )
							{
								return;
							}
						}
					}
				}
			}
		}
		// we should never get here.
		check(FALSE);
	}

	// debugf(TEXT("Reallocating."));

	// We are totally reallocating the weightmap
	INT TotalNeededChannels = WeightmapLayerAllocations.Num();
	INT CurrentLayer = 0;
	TArray<UTexture2D*> NewWeightmapTextures;
	while( TotalNeededChannels > 0 )
	{
		// debugf(TEXT("Still need %d channels"), TotalNeededChannels);

		UTexture2D* CurrentWeightmapTexture = NULL;
		FLandscapeWeightmapUsage* CurrentWeightmapUsage = NULL;

		if( TotalNeededChannels < 4 )
		{
			// debugf(TEXT("Looking for nearest"));

			// see if we can find a suitable existing weightmap texture with sufficient channels
			INT BestDistanceSquared = MAXINT;
			for( TMap<UTexture2D*,struct FLandscapeWeightmapUsage>::TIterator It(Proxy->WeightmapUsageMap); It; ++It )
			{
				FLandscapeWeightmapUsage* TryWeightmapUsage = &It.Value();
				if( TryWeightmapUsage->FreeChannelCount() >= TotalNeededChannels )
				{
					// See if this candidate is closer than any others we've found
					for( INT ChanIdx=0;ChanIdx<4;ChanIdx++ )
					{
						if( TryWeightmapUsage->ChannelUsage[ChanIdx] != NULL  )
						{
							INT TryDistanceSquared = Square(TryWeightmapUsage->ChannelUsage[ChanIdx]->SectionBaseX - SectionBaseX) + Square(TryWeightmapUsage->ChannelUsage[ChanIdx]->SectionBaseX - SectionBaseX);
							if( TryDistanceSquared < BestDistanceSquared )
							{
								CurrentWeightmapTexture = It.Key();
								CurrentWeightmapUsage = TryWeightmapUsage;
								BestDistanceSquared = TryDistanceSquared;
							}
						}
					}
				}
			}
		}

		UBOOL NeedsUpdateResource=FALSE;
		// No suitable weightmap texture
		if( CurrentWeightmapTexture == NULL )
		{
			MarkPackageDirty();

			// Weightmap is sized the same as the component
			INT WeightmapSize = (SubsectionSizeQuads+1) * NumSubsections;

			// We need a new weightmap texture
			CurrentWeightmapTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), GetOutermost(), NAME_None, RF_Public);
			CurrentWeightmapTexture->Init(WeightmapSize,WeightmapSize,PF_A8R8G8B8);
			CurrentWeightmapTexture->SRGB = FALSE;
			CurrentWeightmapTexture->CompressionNone = TRUE;
			CurrentWeightmapTexture->MipGenSettings = TMGS_LeaveExistingMips;
			CurrentWeightmapTexture->AddressX = TA_Clamp;
			CurrentWeightmapTexture->AddressY = TA_Clamp;
			CurrentWeightmapTexture->LODGroup = TEXTUREGROUP_Terrain_Weightmap;
			// Alloc dummy mips
			CreateEmptyTextureMips(CurrentWeightmapTexture);
			CurrentWeightmapTexture->UpdateResource();

			// Store it in the usage map
			CurrentWeightmapUsage = &Proxy->WeightmapUsageMap.Set(CurrentWeightmapTexture, FLandscapeWeightmapUsage());

			// debugf(TEXT("Making a new texture %s"), *CurrentWeightmapTexture->GetName());
		}

		NewWeightmapTextures.AddItem(CurrentWeightmapTexture);

		for( INT ChanIdx=0;ChanIdx<4 && TotalNeededChannels > 0;ChanIdx++ )
		{
			// debugf(TEXT("Finding allocation for layer %d"), CurrentLayer);

			if( CurrentWeightmapUsage->ChannelUsage[ChanIdx] == NULL  )
			{
				// Use this allocation
				FWeightmapLayerAllocationInfo& AllocInfo = WeightmapLayerAllocations(CurrentLayer);

				if( AllocInfo.WeightmapTextureIndex == 255 )
				{
					// New layer - zero out the data for this texture channel
					if( DataInterface )
					{
						DataInterface->ZeroTextureChannel( CurrentWeightmapTexture, ChanIdx );
						// debugf(TEXT("Zeroing out channel %s.%d"), *CurrentWeightmapTexture->GetName(), ChanIdx);
					}
				}
				else
				{
					UTexture2D* OldWeightmapTexture = WeightmapTextures(AllocInfo.WeightmapTextureIndex);

					// Copy the data
					if( DataInterface )
					{
						DataInterface->CopyTextureChannel( CurrentWeightmapTexture, ChanIdx, OldWeightmapTexture, AllocInfo.WeightmapTextureChannel );
						DataInterface->ZeroTextureChannel( OldWeightmapTexture, AllocInfo.WeightmapTextureChannel );
						// debugf(TEXT("Copying old channel (%s).%d to new channel (%s).%d"), *OldWeightmapTexture->GetName(), AllocInfo.WeightmapTextureChannel, *CurrentWeightmapTexture->GetName(), ChanIdx);
					}

					// Remove the old allocation
					FLandscapeWeightmapUsage* OldWeightmapUsage = Proxy->WeightmapUsageMap.Find(OldWeightmapTexture);
					OldWeightmapUsage->ChannelUsage[AllocInfo.WeightmapTextureChannel] = NULL;
				}

				// Assign the new allocation
				CurrentWeightmapUsage->ChannelUsage[ChanIdx] = this;
				AllocInfo.WeightmapTextureIndex = NewWeightmapTextures.Num()-1;
				AllocInfo.WeightmapTextureChannel = ChanIdx;
				CurrentLayer++;
				TotalNeededChannels--;
			}
		}
	}

	// Replace the weightmap textures
	WeightmapTextures = NewWeightmapTextures;

	if (DataInterface)
	{
		// Update the mipmaps for the textures we edited
		for( INT Idx=0;Idx<WeightmapTextures.Num();Idx++)
		{
			UTexture2D* WeightmapTexture = WeightmapTextures(Idx);
			FLandscapeTextureDataInfo* WeightmapDataInfo = DataInterface->GetTextureDataInfo(WeightmapTexture);

			INT NumMips = WeightmapTexture->Mips.Num();
			TArray<FColor*> WeightmapTextureMipData(NumMips);
			for( INT MipIdx=0;MipIdx<NumMips;MipIdx++ )
			{
				WeightmapTextureMipData(MipIdx) = (FColor*)WeightmapDataInfo->GetMipData(MipIdx);
			}

			ULandscapeComponent::UpdateWeightmapMips(NumSubsections, SubsectionSizeQuads, WeightmapTexture, WeightmapTextureMipData, 0, 0, MAXINT, MAXINT, WeightmapDataInfo);
		}
	}
}

void ALandscapeProxy::RemoveInvalidWeightmaps()
{
	if (GIsEditor)
	{
		for( TMap< UTexture2D*,struct FLandscapeWeightmapUsage >::TIterator It(WeightmapUsageMap); It; ++It )
		{
			UTexture2D* Tex = It.Key();
			FLandscapeWeightmapUsage& Usage = It.Value();
			if (Usage.FreeChannelCount() == 4) // Invalid Weight-map
			{
				if (Tex)
				{
				Tex->SetFlags(RF_Transactional);
				Tex->Modify();
				Tex->MarkPackageDirty();
				Tex->ClearFlags(RF_Standalone);
			}
				WeightmapUsageMap.Remove(Tex);
			}
		}

		// Remove Unused Weightmaps...
		for (INT Idx=0; Idx < LandscapeComponents.Num(); ++Idx)
		{
			ULandscapeComponent* Component = LandscapeComponents(Idx);
			Component->RemoveInvalidWeightmaps();
		}
	}
}

void ULandscapeComponent::RemoveInvalidWeightmaps()
{
	// Adjust WeightmapTextureIndex index for other layers
	TSet<INT> UsedTextureIndices;
	TSet<INT> AllTextureIndices;
	for( INT LayerIdx=0;LayerIdx<WeightmapLayerAllocations.Num();LayerIdx++ )
	{
		UsedTextureIndices.Add( WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex );
	}

	for ( INT WeightIdx=0; WeightIdx < WeightmapTextures.Num(); ++WeightIdx )
	{
		AllTextureIndices.Add( WeightIdx );
	}

	TSet<INT> UnUsedTextureIndices = AllTextureIndices.Difference(UsedTextureIndices);

	INT DeletedLayers = 0;
	for (TSet<INT>::TIterator It(UnUsedTextureIndices); It; ++It)
	{
		INT DeleteLayerWeightmapTextureIndex = *It - DeletedLayers;
		WeightmapTextures(DeleteLayerWeightmapTextureIndex)->SetFlags(RF_Transactional);
		WeightmapTextures(DeleteLayerWeightmapTextureIndex)->Modify();
		WeightmapTextures(DeleteLayerWeightmapTextureIndex)->MarkPackageDirty();
		WeightmapTextures(DeleteLayerWeightmapTextureIndex)->ClearFlags(RF_Standalone);
		WeightmapTextures.Remove( DeleteLayerWeightmapTextureIndex );

		// Adjust WeightmapTextureIndex index for other layers
		for( INT LayerIdx=0;LayerIdx<WeightmapLayerAllocations.Num();LayerIdx++ )
		{
			FWeightmapLayerAllocationInfo& Allocation = WeightmapLayerAllocations(LayerIdx);

			if( Allocation.WeightmapTextureIndex > DeleteLayerWeightmapTextureIndex )
			{
				Allocation.WeightmapTextureIndex--;
			}

			check( Allocation.WeightmapTextureIndex < WeightmapTextures.Num() );
		}
		DeletedLayers++;
	}
}

void ULandscapeComponent::InitHeightmapData(TArray<FColor>& Heights, UBOOL bUpdateCollision)
{
	INT ComponentSizeVerts = NumSubsections * (SubsectionSizeQuads+1);

	if (Heights.Num() != Square(ComponentSizeVerts) )
	{
		return;
	}

	// Handling old Height map....
	if (HeightmapTexture && HeightmapTexture->GetOutermost() != UObject::GetTransientPackage() 
		&& HeightmapTexture->GetOutermost() == GetOutermost()
		&& HeightmapTexture->SizeX >= ComponentSizeVerts) // if Height map is not valid...
	{
		HeightmapTexture->SetFlags(RF_Transactional);
		HeightmapTexture->Modify();
		HeightmapTexture->MarkPackageDirty();
		HeightmapTexture->ClearFlags(RF_Standalone); // Delete if no reference...
	}

	// New Height map
	TArray<FColor*> HeightmapTextureMipData;
	// make sure the heightmap UVs are powers of two.
	INT HeightmapSizeU = (1<<appCeilLogTwo( ComponentSizeVerts ));
	INT HeightmapSizeV = (1<<appCeilLogTwo( ComponentSizeVerts ));

	// Height map construction
	HeightmapTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), GetOutermost(), NAME_None, RF_Public);
	HeightmapTexture->Init(HeightmapSizeU,HeightmapSizeV,PF_A8R8G8B8);
	HeightmapTexture->SRGB = FALSE;
	HeightmapTexture->CompressionNone = TRUE;
	HeightmapTexture->MipGenSettings = TMGS_LeaveExistingMips;
	HeightmapTexture->LODGroup = TEXTUREGROUP_Terrain_Heightmap;
	HeightmapTexture->AddressX = TA_Clamp;
	HeightmapTexture->AddressY = TA_Clamp;

	INT MipSubsectionSizeQuads = SubsectionSizeQuads;
	INT MipSizeU = HeightmapSizeU;
	INT MipSizeV = HeightmapSizeV;

	HeightmapScaleBias = FVector4( 1.f / (FLOAT)HeightmapSizeU, 1.f / (FLOAT)HeightmapSizeV, 0.f, 0.f);

	INT Mip = 0;
	while( MipSizeU > 1 && MipSizeV > 1 && MipSubsectionSizeQuads >= 1 )
	{
		FColor* HeightmapTextureData;
		if( HeightmapTextureMipData.Num() > 0 )	
		{
			// create subsequent mips
			FTexture2DMipMap* HeightMipMap = new(HeightmapTexture->Mips) FTexture2DMipMap;
			HeightMipMap->SizeX = MipSizeU;
			HeightMipMap->SizeY = MipSizeV;
			HeightMipMap->Data.Lock(LOCK_READ_WRITE);
			HeightmapTextureData = (FColor*)HeightMipMap->Data.Realloc(MipSizeU*MipSizeV*sizeof(FColor));
			appMemzero( HeightmapTextureData, MipSizeU*MipSizeV*sizeof(FColor) );
		}
		else
		{
			HeightmapTextureData = (FColor*)HeightmapTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);
			appMemcpy( HeightmapTextureData, &Heights(0), MipSizeU*MipSizeV*sizeof(FColor) );
		}

		HeightmapTextureMipData.AddItem(HeightmapTextureData);

		MipSizeU >>= 1;
		MipSizeV >>= 1;
		Mip++;

		MipSubsectionSizeQuads = ((MipSubsectionSizeQuads + 1) >> 1) - 1;
	}
	ULandscapeComponent::GenerateHeightmapMips( HeightmapTextureMipData );

	if( bUpdateCollision )
	{
		ULandscapeComponent::UpdateCollisionHeightData( HeightmapTextureMipData(CollisionMipLevel) );
	}

	for( INT i=0;i<HeightmapTextureMipData.Num();i++ )
	{
		HeightmapTexture->Mips(i).Data.Unlock();
	}
	HeightmapTexture->UpdateResource();
}

void ULandscapeComponent::InitWeightmapData(TArray<FName>& LayerNames, TArray<TArray<BYTE> >& WeightmapData)
{
	if (LayerNames.Num() != WeightmapData.Num() || LayerNames.Num() <= 0)
	{
		return;
	}

	INT ComponentSizeVerts = NumSubsections * (SubsectionSizeQuads+1);

	// Validation..
	for (INT Idx = 0; Idx < WeightmapData.Num(); ++Idx)
	{
		if ( WeightmapData(Idx).Num() != Square(ComponentSizeVerts) )
		{
			return;
		}
	}
	
	for (INT Idx = 0; Idx < WeightmapTextures.Num(); ++Idx)
	{
		if (WeightmapTextures(Idx) && WeightmapTextures(Idx)->GetOutermost() != UObject::GetTransientPackage() 
			&& WeightmapTextures(Idx)->GetOutermost() == GetOutermost()
			&& WeightmapTextures(Idx)->SizeX == ComponentSizeVerts) 
		{
			WeightmapTextures(Idx)->SetFlags(RF_Transactional);
			WeightmapTextures(Idx)->Modify();
			WeightmapTextures(Idx)->MarkPackageDirty();
			WeightmapTextures(Idx)->ClearFlags(RF_Standalone); // Delete if no reference...
		}
	}
	WeightmapTextures.Empty();

	WeightmapLayerAllocations.Empty(LayerNames.Num());
	for (INT Idx = 0; Idx < LayerNames.Num(); ++Idx)
	{
		new (WeightmapLayerAllocations) FWeightmapLayerAllocationInfo(LayerNames(Idx));
	}

	ReallocateWeightmaps(NULL);

	check(WeightmapLayerAllocations.Num() > 0 && WeightmapTextures.Num() > 0 );

	INT WeightmapSize = ComponentSizeVerts;
	WeightmapScaleBias = FVector4( 1.f / (FLOAT)WeightmapSize, 1.f / (FLOAT)WeightmapSize, 0.5f / (FLOAT)WeightmapSize, 0.5f / (FLOAT)WeightmapSize);
	WeightmapSubsectionOffset =  (FLOAT)(SubsectionSizeQuads+1) / (FLOAT)WeightmapSize;

	// Channel remapping
	INT ChannelOffsets[4] = {STRUCT_OFFSET(FColor,R),STRUCT_OFFSET(FColor,G),STRUCT_OFFSET(FColor,B),STRUCT_OFFSET(FColor,A)};

	TArray<void*> WeightmapDataPtrs(WeightmapTextures.Num());
	for (INT WeightmapIdx = 0; WeightmapIdx < WeightmapTextures.Num(); ++WeightmapIdx)
	{
		WeightmapDataPtrs(WeightmapIdx) = WeightmapTextures(WeightmapIdx)->Mips(0).Data.Lock(LOCK_READ_WRITE);
	}

	for (INT LayerIdx = 0; LayerIdx < WeightmapLayerAllocations.Num(); ++LayerIdx)
	{
		void* DestDataPtr = WeightmapDataPtrs(WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex);
		BYTE* DestTextureData = (BYTE*)DestDataPtr + ChannelOffsets[ WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel ];
		BYTE* SrcTextureData = (BYTE*)&WeightmapData(LayerIdx)(0);

		for( INT i=0;i<WeightmapData(LayerIdx).Num();i++ )
		{
			DestTextureData[i*4] = SrcTextureData[i];
		}
	}

	for( INT Idx=0;Idx<WeightmapTextures.Num();Idx++)
	{
		UTexture2D* WeightmapTexture = WeightmapTextures(Idx);
		WeightmapTexture->Mips(0).Data.Unlock();
	}

	for( INT Idx=0;Idx<WeightmapTextures.Num();Idx++)
	{
		UTexture2D* WeightmapTexture = WeightmapTextures(Idx);
		{
			FLandscapeTextureDataInfo WeightmapDataInfo(WeightmapTexture);

			INT NumMips = WeightmapTexture->Mips.Num();
			TArray<FColor*> WeightmapTextureMipData(NumMips);
			for( INT MipIdx=0;MipIdx<NumMips;MipIdx++ )
			{
				WeightmapTextureMipData(MipIdx) = (FColor*)WeightmapDataInfo.GetMipData(MipIdx);
			}

			ULandscapeComponent::UpdateWeightmapMips(NumSubsections, SubsectionSizeQuads, WeightmapTexture, WeightmapTextureMipData, 0, 0, MAXINT, MAXINT, &WeightmapDataInfo);
		}

		WeightmapTexture->UpdateResource();
	}

	FlushRenderingCommands();

	MaterialInstance = NULL;
}

#define MAX_LANDSCAPE_EXPORT_COMPONENTS_NUM		16
#define MAX_LANDSCAPE_PROP_TEXT_LENGTH			1024*1024*16

// Export/Import
UBOOL ALandscapeProxy::ShouldExport()
{
	if (!bIsMovingToLevel && LandscapeComponents.Num() > MAX_LANDSCAPE_EXPORT_COMPONENTS_NUM)
	{
		// Prompt to save startup packages
		INT PopupResult = appMsgf(AMT_YesNo, LocalizeSecure(LocalizeUnrealEd("LandscapeExport_Warning"), LandscapeComponents.Num()));

		switch( PopupResult )
		{
		case ART_No:
			return FALSE;
			break;
		case ART_Yes:
			return TRUE;
			break;
		}
	}
	return TRUE;
}

UBOOL ALandscapeProxy::ShouldImport(FString* ActorPropString, UBOOL IsMovingToLevel)
{
	bIsMovingToLevel = IsMovingToLevel;
	if (!bIsMovingToLevel && ActorPropString && ActorPropString->Len() > MAX_LANDSCAPE_PROP_TEXT_LENGTH)
	{
		// Prompt to save startup packages
		INT PopupResult = appMsgf(AMT_YesNo, LocalizeSecure(LocalizeUnrealEd("LandscapeImport_Warning"), ActorPropString->Len() >> 20 ));

		switch( PopupResult )
		{
		case ART_No:
			return FALSE;
			break;
		case ART_Yes:
			return TRUE;
			break;
		}
	}
	return TRUE;
}

void ULandscapeComponent::ExportCustomProperties(FOutputDevice& Out, UINT Indent)
{
	// Height map
	INT NumVertices = Square( NumSubsections*(SubsectionSizeQuads+1) );
	FLandscapeComponentDataInterface DataInterface(this);
	TArray<FColor> Heightmap;
	DataInterface.GetHeightmapTextureData(Heightmap);
	check(Heightmap.Num() == NumVertices);

	Out.Logf( TEXT("%sCustomProperties LandscapeHeightData "), appSpc(Indent));
	for( INT i=0;i<NumVertices;i++ )
	{
		Out.Logf( TEXT("%x "), Heightmap(i).DWColor() );
	}

	TArray<BYTE> Weightmap;
	// Weight map
	Out.Logf( TEXT("LayerNum=%d "), WeightmapLayerAllocations.Num());
	for (INT i=0; i < WeightmapLayerAllocations.Num(); i++)
	{
		if (DataInterface.GetWeightmapTextureData(WeightmapLayerAllocations(i).LayerName, Weightmap))
		{
			Out.Logf( TEXT("LayerName=%s "), *WeightmapLayerAllocations(i).LayerName.ToString());
			for( INT i=0;i<NumVertices;i++ )
			{
				Out.Logf( TEXT("%x "), Weightmap(i) );
			}
		}
	}

	Out.Logf( TEXT("\r\n") );
}

namespace
{
	inline UBOOL appIsHexDigit( TCHAR c )
	{
		return appIsDigit(c) || (c>=TEXT('a') && c<=TEXT('f'));
	}
};

void ULandscapeComponent::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	if(ParseCommand(&SourceText,TEXT("LandscapeHeightData")))
	{
		INT NumVertices = Square( NumSubsections*(SubsectionSizeQuads+1) );

		TArray<FColor> Heights;
		Heights.Empty(NumVertices);
		Heights.AddZeroed(NumVertices);

		ParseNext(&SourceText);
		INT i = 0;
		TCHAR* StopStr;
		while( appIsHexDigit(*SourceText) ) 
		{
			if( i < NumVertices )
			{
				Heights(i++).DWColor() = appStrtoi(SourceText, &StopStr, 16);
				while( appIsHexDigit(*SourceText) ) 
				{
					SourceText++;
				}
			}

			ParseNext(&SourceText);
		} 

		if( i != NumVertices )
		{
			Warn->Logf( *LocalizeError(TEXT("CustomProperties(LanscapeComponent Heightmap) Syntax Error"), TEXT("Core")));
		}

		INT ComponentSizeVerts = NumSubsections * (SubsectionSizeQuads+1);

		InitHeightmapData(Heights, FALSE);

		// Weight maps
		INT LayerNum = 0;
		if (Parse(SourceText, TEXT("LayerNum="), LayerNum))
		{
			while(*SourceText && (!appIsWhitespace(*SourceText)))
			{
				++SourceText;
			}
			ParseNext(&SourceText);
		}

		if (LayerNum <= 0)
		{
			return;
		}

		// Init memory
		TArray<FName> LayerNames;
		LayerNames.Empty(LayerNum);
		TArray<TArray<BYTE>> WeightmapData;
		for (INT i=0; i < LayerNum; ++i)
		{
			TArray<BYTE> Weights;
			Weights.Empty(NumVertices);
			Weights.Add(NumVertices);
			WeightmapData.AddItem(Weights);
		}

		INT LayerIdx = 0;
		FString LayerName;
		while ( *SourceText )
		{
			if (Parse(SourceText, TEXT("LayerName="), LayerName))
			{
				new (LayerNames) FName(*LayerName);

				while(*SourceText && (!appIsWhitespace(*SourceText)))
				{
					++SourceText;
				}
				ParseNext(&SourceText);
				check(*SourceText);

				i = 0;
				while( appIsHexDigit(*SourceText) ) 
				{
					if( i < NumVertices )
					{
						(WeightmapData(LayerIdx))(i++) = (BYTE)appStrtoi(SourceText, &StopStr, 16);
						while( appIsHexDigit(*SourceText) ) 
						{
							SourceText++;
						}
					}
					ParseNext(&SourceText);
				} 

				if( i != NumVertices )
				{
					Warn->Logf( *LocalizeError(TEXT("CustomProperties(LanscapeComponent Weightmap) Syntax Error"), TEXT("Core")));
				}
				LayerIdx++;
			}
			else
			{
				break;
			}
		}

		InitWeightmapData(LayerNames, WeightmapData);
	}
}

void ALandscapeProxy::UpdateLandscapeActor(ALandscape* Landscape, UBOOL bSearchForActor /*= TRUE*/)
{
	if (bIsProxy)
	{
		if (!Landscape || !IsValidLandscapeActor(Landscape))
		{
			LandscapeActor = NULL;
			if (bSearchForActor && GWorld)
			{
				for (FActorIterator It; It; ++It)
				{
					Landscape = Cast<ALandscape>(*It);
					if (Landscape && IsValidLandscapeActor(Landscape) )
					{
						LandscapeActor = Landscape;
						break;
					}
				}
			}
		}
		else
		{
			LandscapeActor = Landscape;
			GetSharedProperties(LandscapeActor);
		}

		if (LandscapeGuid.IsValid())
		{
			ULandscapeInfo* Info = GetLandscapeInfo();
			if (Info)
			{
				Info->Proxies.Add(this);
			}
		}
	}
}

void ALandscape::UpdateLandscapeActor(ALandscape* Landscape, UBOOL bSearchForActor /*= TRUE*/)
{
	// Find proxies
	if (!Landscape && GWorld)
	{
		for (FActorIterator It; It; ++It)
		{
			ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
			if (Proxy && Proxy->bIsProxy && !Proxy->LandscapeActor)
			{
				Proxy->UpdateLandscapeActor(this, FALSE);
			}
		}
	}
}

UBOOL ALandscapeProxy::IsValidLandscapeActor(ALandscape* Landscape)
{
	if (bIsProxy && Cast<ALandscape>(Landscape))
	{
		if (!Landscape->HasAnyFlags(RF_BeginDestroyed))
		{
			if (!LandscapeGuid.IsValid())
			{
				return TRUE; // always valid for newly created Proxy
			}
			if (LandscapeGuid == Landscape->LandscapeGuid
				&& ComponentSizeQuads == Landscape->ComponentSizeQuads 
				&& NumSubsections == Landscape->NumSubsections
				&& SubsectionSizeQuads == Landscape->SubsectionSizeQuads)
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

void ULandscapeInfo::CheckValidate()
{
	// Check for Validate for component number
	INT NumLandscapeComponent = 0;
	INT NumCollisionComponent = 0;
	TSet<ALandscapeProxy*> LandscapeSet;
	for (FActorIterator It; It; ++It)
	{
		ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
		if (Proxy && Proxy->LandscapeGuid == this->LandscapeGuid ) // Check only for this Guid cases...
		{
			LandscapeSet.Add(Proxy);
			NumLandscapeComponent += Proxy->LandscapeComponents.Num();
			NumCollisionComponent += Proxy->CollisionComponents.Num();
		}
	}

	// Only handle if there is more collision component than component
	// Which is legacy invalid state might occurs from old bug
	if (NumLandscapeComponent != NumCollisionComponent)
	{
		if (NumLandscapeComponent < NumCollisionComponent)
		{
			for (TSet<ALandscapeProxy*>::TIterator It(LandscapeSet); It; ++It)
			{
				// Remove invalid collision component
				ALandscapeProxy* Proxy = *It;
				for (INT i = 0; i < Proxy->CollisionComponents.Num(); ++i)
				{
					ULandscapeHeightfieldCollisionComponent* CollisionComp = Proxy->CollisionComponents(i);
					QWORD ComponentKey = ALandscape::MakeKey(CollisionComp->SectionBaseX, CollisionComp->SectionBaseY);
					if (XYtoCollisionComponentMap.FindRef(ComponentKey) != CollisionComp)
					{
						// Remove this component...
						Proxy->CollisionComponents.RemoveItem(CollisionComp);
						CollisionComp->ConditionalDetach();
					}
				}
			}
		}
	}
}

#endif //WITH_EDITOR