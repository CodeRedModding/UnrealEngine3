/*=============================================================================
	InstancedFoliage.cpp: Instanced foliage implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineMeshClasses.h"
#include "EngineFoliageClasses.h"
#include "UnTerrain.h"

IMPLEMENT_CLASS(AInstancedFoliageActor);
IMPLEMENT_CLASS(UInstancedFoliageSettings);

//
// Serializers for struct data
//

FArchive& operator<<( FArchive& Ar, FFoliageInstance& Instance )
{
	Ar << Instance.Base;
	Ar << Instance.Location;
	Ar << Instance.Rotation;
	Ar << Instance.DrawScale3D;

	if( Ar.Ver() >= VER_FOLIAGE_INSTANCE_SAVE_EDITOR_DATA )
	{
		Ar << Instance.ClusterIndex;
		Ar << Instance.PreAlignRotation;
		Ar << Instance.Flags;
	}
	else if( Ar.IsLoading() )
	{
		Instance.ClusterIndex = -1;
		Instance.PreAlignRotation = Instance.Rotation;
		Instance.Flags = FOLIAGE_AlignToNormal;
	}

	if( Ar.Ver() >= VER_FOLIAGE_ADDED_Z_OFFSET )
	{
		Ar << Instance.ZOffset;
	}
	else if( Ar.IsLoading() )
	{
		Instance.ZOffset = 0.f;
	}

	return Ar;
}

FArchive& operator<<( FArchive& Ar, FFoliageInstanceCluster& Cluster )
{
	Ar << Cluster.Bounds;
	Ar << Cluster.ClusterComponent;

	//!! editor only
	Ar << Cluster.InstanceIndices;

	return Ar;
}

FArchive& operator<<( FArchive& Ar, FFoliageMeshInfo& MeshInfo )
{
	Ar << MeshInfo.InstanceClusters;
	//!! editor only
	Ar << MeshInfo.Instances;

	// Serialize the transient data for undo.
	if( Ar.IsTransacting() )
	{
		Ar << *MeshInfo.InstanceHash;
		Ar << MeshInfo.ComponentHash;
		Ar << MeshInfo.FreeInstanceIndices;
		Ar << MeshInfo.SelectedIndices;
	}

	// serialize mesh settings.
	if( Ar.Ver() >= VER_FOLIAGE_SAVE_UI_DATA )
	{
		Ar << MeshInfo.Settings;
	}

	return Ar;
}

FArchive& operator<<( FArchive& Ar, FFoliageComponentHashInfo& ComponentHashInfo )
{
	return Ar << ComponentHashInfo.CachedLocation << ComponentHashInfo.CachedRotation << ComponentHashInfo.CachedDrawScale << ComponentHashInfo.Instances;
}

//
// FFoliageMeshInfo
//
FFoliageMeshInfo::FFoliageMeshInfo()
:	InstanceHash(NULL)
,	Settings(NULL)
{
	if( GIsEditor )
	{
		InstanceHash = new FFoliageInstanceHash();
	}
}

FFoliageMeshInfo::FFoliageMeshInfo(const FFoliageMeshInfo& Other)
:	InstanceHash(NULL)
,	Settings(NULL)
{
	if( GIsEditor )
	{
		InstanceHash = new FFoliageInstanceHash();
	}	
}

FFoliageMeshInfo::~FFoliageMeshInfo()
{
	if( GIsEditor )
	{
		delete InstanceHash;
	}
}

#if _DEBUG
// Debug function to asserts if there is some inconsistency in the data
void FFoliageMeshInfo::CheckValid()
{
	INT ClusterTotal=0;
	INT ComponentTotal=0;

	for( INT ClusterIdx=0;ClusterIdx<InstanceClusters.Num();ClusterIdx++ )
	{
		ClusterTotal += InstanceClusters(ClusterIdx).InstanceIndices.Num();
		ComponentTotal += InstanceClusters(ClusterIdx).ClusterComponent->PerInstanceSMData.Num();
	}

	check( ClusterTotal == ComponentTotal );

	INT FreeTotal = 0;
	INT InstanceTotal = 0;
	for( INT InstanceIdx=0; InstanceIdx<Instances.Num(); InstanceIdx++ )
	{
		if( Instances(InstanceIdx).ClusterIndex != -1 )
		{
			InstanceTotal++;
		}
		else
		{
			FreeTotal++;
		}
	}

	check( ClusterTotal == InstanceTotal );
	check( FreeInstanceIndices.Num() == FreeTotal );

	InstanceHash->CheckInstanceCount(InstanceTotal);

	INT ComponentHashTotal = 0;
	for( TMap<class UActorComponent*, FFoliageComponentHashInfo >::TConstIterator It(ComponentHash); It; ++It )
	{
		ComponentHashTotal += It.Value().Instances.Num();
	}
	check( ComponentHashTotal == InstanceTotal);

#if 0
	// Check transforms match up with editor data
	INT MismatchCount = 0;
	for( INT ClusterIdx=0;ClusterIdx<InstanceClusters.Num();ClusterIdx++ )
	{
		TArray<INT> Indices = InstanceClusters(ClusterIdx).InstanceIndices;
		UInstancedStaticMeshComponent* Comp = InstanceClusters(ClusterIdx).ClusterComponent;
		for( INT InstIdx=0;InstIdx<Indices.Num();InstIdx++ )
		{
			INT InstanceIdx = Indices(InstIdx);

			FMatrix InstanceTransform = Instances(InstanceIdx).GetInstanceTransform();
			FMatrix& CompTransform = Comp->PerInstanceSMData(InstIdx).Transform;
			
			if( InstanceTransform != CompTransform )
			{
				CompTransform = InstanceTransform;
				MismatchCount++;
			}
		}
	}

	if( MismatchCount != 0 )
	{
		debugf(TEXT("%s: transform mismatch: %d"), *InstanceClusters(0).ClusterComponent->StaticMesh->GetName(), MismatchCount);
	}
#endif
}
#endif

#if WITH_EDITOR

void FFoliageMeshInfo::AddInstance( AInstancedFoliageActor* InIFA, UStaticMesh* InMesh, const FFoliageInstance& InNewInstance )
{
	InIFA->Modify();

	// Add the instance taking either a free slot or adding a new item.
	INT InstanceIndex = FreeInstanceIndices.Num() > 0 ?  FreeInstanceIndices.Pop() : Instances.Add();

	FFoliageInstance& AddedInstance = Instances(InstanceIndex);
	AddedInstance = InNewInstance;

	// Add the instance to the hash
	InstanceHash->InsertInstance(InNewInstance.Location, InstanceIndex);
	FFoliageComponentHashInfo& ComponentHashInfo = ComponentHash.FindOrAddKey(InNewInstance.Base);
	ComponentHashInfo.Instances.Add(InstanceIndex);

	// Find the best cluster to allocate the instance to.
	FFoliageInstanceCluster* BestCluster = NULL;
	INT BestClusterIndex = INDEX_NONE;
	FLOAT BestClusterDistSq = FLT_MAX;

	INT MaxInstancesPerCluster = Settings->MaxInstancesPerCluster;
	FLOAT MaxClusterRadiusSq = Square(Settings->MaxClusterRadius);

	for( INT ClusterIdx=0;ClusterIdx<InstanceClusters.Num();ClusterIdx++ )
	{
		FFoliageInstanceCluster& Cluster = InstanceClusters(ClusterIdx);
		if( Cluster.InstanceIndices.Num() < MaxInstancesPerCluster )
		{
			FLOAT DistSq = (Cluster.Bounds.Origin - InNewInstance.Location).SizeSquared();
			if( DistSq < BestClusterDistSq && DistSq < MaxClusterRadiusSq )
			{
				BestCluster = &Cluster;
				BestClusterIndex = ClusterIdx;
				BestClusterDistSq = DistSq;
			}
		}
	}

	// Calculate transform for the instance
	FMatrix InstanceTransform = InNewInstance.GetInstanceTransform();

	if( BestCluster == NULL )
	{
		BestClusterIndex = InstanceClusters.Num();
		BestCluster = new(InstanceClusters) FFoliageInstanceCluster(
			ConstructObject<UInstancedStaticMeshComponent>(UInstancedStaticMeshComponent::StaticClass(),InIFA,NAME_None,RF_Transactional),
			InMesh->Bounds.TransformBy(InstanceTransform)
			);
			
		BestCluster->ClusterComponent->StaticMesh = InMesh;
		BestCluster->ClusterComponent->bForceDirectLightMap = TRUE;
		BestCluster->ClusterComponent->bSelectable = TRUE;
		BestCluster->ClusterComponent->bUsePerInstanceHitProxies = TRUE;
		BestCluster->ClusterComponent->bDontResolveInstancedLightmaps = TRUE;
		BestCluster->ClusterComponent->InstancingRandomSeed = appRand();
		ApplyInstancedFoliageSettings( BestCluster->ClusterComponent );
	}
	else
	{
		BestCluster->ClusterComponent->Modify();
		BestCluster->ClusterComponent->InvalidateLightingCache();
		BestCluster->Bounds = BestCluster->Bounds + InMesh->Bounds.TransformBy(InstanceTransform);
	}

	BestCluster->InstanceIndices.AddItem(InstanceIndex);

	// Save the cluster index
	AddedInstance.ClusterIndex = BestClusterIndex;
	
	// Add the instance to the component
	FInstancedStaticMeshInstanceData* NewInstanceData = new(BestCluster->ClusterComponent->PerInstanceSMData) FInstancedStaticMeshInstanceData();
	if( BestCluster->ClusterComponent->SelectedInstances.Num() > 0 )
	{
		BestCluster->ClusterComponent->SelectedInstances.AddItem(FALSE);
	}
	NewInstanceData->Transform = InstanceTransform;
	NewInstanceData->LightmapUVBias = FVector2D( -1.0f, -1.0f );
	NewInstanceData->ShadowmapUVBias = FVector2D( -1.0f, -1.0f );
	BestCluster->ClusterComponent->bNeedsReattach = TRUE;

#if _DEBUG
	CheckValid();
#endif

	InIFA->ConditionalUpdateComponents();
}

void FFoliageMeshInfo::ApplyInstancedFoliageSettings( UInstancedStaticMeshComponent* InClusterComponent )
{
	InClusterComponent->InstanceStartCullDistance			= Settings->StartCullDistance;
	InClusterComponent->InstanceEndCullDistance				= Settings->EndCullDistance;
	FLOAT CullDistance = Settings->EndCullDistance > 0 ? (FLOAT)Settings->EndCullDistance : 0.f;
	InClusterComponent->DetailMode							= Settings->DetailMode;
	InClusterComponent->LDMaxDrawDistance					= CullDistance;
	InClusterComponent->CachedMaxDrawDistance				= CullDistance;
	InClusterComponent->CastShadow							= Settings->CastShadow;
	InClusterComponent->bCastDynamicShadow					= Settings->bCastDynamicShadow;
	InClusterComponent->bCastStaticShadow					= Settings->bCastStaticShadow;
	InClusterComponent->bSelfShadowOnly						= Settings->bSelfShadowOnly;
	InClusterComponent->bNoModSelfShadow					= Settings->bNoModSelfShadow;
	InClusterComponent->bAcceptsDynamicDominantLightShadows	= Settings->bAcceptsDynamicDominantLightShadows;
	InClusterComponent->bCastHiddenShadow					= Settings->bCastHiddenShadow;
	InClusterComponent->bCastShadowAsTwoSided				= Settings->bCastShadowAsTwoSided;

	InClusterComponent->bAcceptsDynamicDominantLightShadows	= Settings->bAcceptsDynamicDominantLightShadows;
	InClusterComponent->bCastHiddenShadow					= Settings->bCastHiddenShadow;
	InClusterComponent->bCastShadowAsTwoSided				= Settings->bCastShadowAsTwoSided;

	InClusterComponent->AlwaysCheckCollision				= Settings->bCollideActors;
	InClusterComponent->CollideActors						= Settings->bCollideActors;
	InClusterComponent->BlockActors							= Settings->bBlockActors;
	InClusterComponent->BlockNonZeroExtent					= Settings->bBlockNonZeroExtent;
	InClusterComponent->BlockZeroExtent						= Settings->bBlockZeroExtent;

	if( InClusterComponent->bAcceptsLights != Settings->bAcceptsLights
		|| InClusterComponent->bUsePrecomputedShadows != Settings->bUsePrecomputedShadows )
	{
		InClusterComponent->InvalidateLightingCache();
	}

	InClusterComponent->bAcceptsLights						= Settings->bAcceptsLights;
	InClusterComponent->bUsePrecomputedShadows				= Settings->bUsePrecomputedShadows;
	InClusterComponent->bAcceptsDynamicLights				= Settings->bAcceptsDynamicLights;
	InClusterComponent->bUseOnePassLightingOnTranslucency	= Settings->bUseOnePassLightingOnTranslucency;
}

void FFoliageMeshInfo::RemoveInstances( AInstancedFoliageActor* InIFA, const TArray<INT>& InInstancesToRemove )
{
	UBOOL bRemoved = FALSE;

	InIFA->Modify();

	// Used to store a list of instances to remove sorted by cluster
	TMap<INT, TSet<INT> > ClusterInstanceSetMap;

	// Remove instances from the hash
	for( TArray<INT>::TConstIterator It(InInstancesToRemove); It; ++It )
	{
		INT InstanceIndex = *It;
		const FFoliageInstance& Instance = Instances(InstanceIndex);
		InstanceHash->RemoveInstance(Instance.Location, InstanceIndex);
		FFoliageComponentHashInfo* ComponentHashInfo = ComponentHash.Find(Instance.Base);
		if( ComponentHashInfo )
		{
			ComponentHashInfo->Instances.RemoveKey(InstanceIndex);
			if( ComponentHashInfo->Instances.Num() == 0 )
			{
				// Remove the component from the component hash if this is the last instance.
				ComponentHash.Remove(Instance.Base);
			}
		}

		// Store in ClusterInstanceSetMap
		INT ClusterIndex = Instances(InstanceIndex).ClusterIndex;
		TSet<INT>& ClusterInstances = ClusterInstanceSetMap.FindOrAdd(ClusterIndex);
		ClusterInstances.Add(InstanceIndex);
	}

	// Process each cluster in turn
	for( TMap<INT, TSet<INT> >::TIterator It(ClusterInstanceSetMap); It; ++It )
	{
		INT CurrentClusterIndex = It.Key();
		FFoliageInstanceCluster& Cluster = InstanceClusters(CurrentClusterIndex);
		UInstancedStaticMeshComponent* ClusterComponent = Cluster.ClusterComponent;
		UBOOL bRemovedFromComponent = FALSE;

		TSet<INT>& ClusterInstancesToRemove = It.Value();

		// Look through all indices in this cluster
		for( INT ClusterInstanceIdx=0;ClusterInstanceIdx<Cluster.InstanceIndices.Num();ClusterInstanceIdx++ )
		{
			INT InstanceIndex = Cluster.InstanceIndices(ClusterInstanceIdx);

			// Check if this instance is one we need to remove
			if( ClusterInstancesToRemove.Contains(InstanceIndex) )
			{
				// Remove the instance data and the index array entry
				ClusterComponent->Modify();
				ClusterComponent->PerInstanceSMData.RemoveSwap(ClusterInstanceIdx);
				if( ClusterInstanceIdx < ClusterComponent->SelectedInstances.Num() )
				{
					ClusterComponent->SelectedInstances.RemoveSwap(ClusterInstanceIdx);
				}
				Cluster.InstanceIndices.RemoveSwap(ClusterInstanceIdx);
				ClusterInstanceIdx--;
				bRemovedFromComponent = TRUE;

				// Invalidate this instance's editor data so we reuse the slot later
				FFoliageInstance& InstanceEditorData = Instances(InstanceIndex);
				InstanceEditorData.ClusterIndex = -1;
				InstanceEditorData.Base = NULL;

				// And remember the slot to reuse it.
				FreeInstanceIndices.AddItem(InstanceIndex);

				// Remove it from the selection.
				SelectedIndices.RemoveItem(InstanceIndex);
			}
		}

		if( bRemovedFromComponent )
		{
			//!! need to update bounds for this cluster
			ClusterComponent->bNeedsReattach = TRUE;
			bRemoved = TRUE;
		}
	}

	if( bRemoved )
	{
		UBOOL bDeleted = FALSE;
		// See if we can any clusters.
		for( INT ClusterIdx=0;ClusterIdx<InstanceClusters.Num();ClusterIdx++ )
		{
			if( InstanceClusters(ClusterIdx).InstanceIndices.Num() == 0 )
			{			
				if( !bDeleted )
				{
					InIFA->ClearComponents();
					bDeleted = TRUE;
				}

				// Remove cluster
				InstanceClusters.Remove(ClusterIdx);

				// update the ClusterIndex for remaining foliage instances
				for( INT InstanceIdx=0;InstanceIdx<Instances.Num();InstanceIdx++ )
				{
					if( Instances(InstanceIdx).ClusterIndex > ClusterIdx )
					{
						Instances(InstanceIdx).ClusterIndex--;
					}
				}

				ClusterIdx--;
			}

			InIFA->CheckSelection();
		}

#if _DEBUG
		CheckValid();
#endif

		InIFA->ConditionalUpdateComponents();
	}
}

void FFoliageMeshInfo::PreMoveInstances( class AInstancedFoliageActor* InIFA, const TArray<INT>& InInstancesToMove )
{
	// Remove instances from the hash
	for( TArray<INT>::TConstIterator It(InInstancesToMove); It; ++It )
	{
		INT InstanceIndex = *It;
		const FFoliageInstance& Instance = Instances(InstanceIndex);
		InstanceHash->RemoveInstance(Instance.Location, InstanceIndex);
	}
}


void FFoliageMeshInfo::PostUpdateInstances( class AInstancedFoliageActor* InIFA, const TArray<INT>& InInstancesUpdated, UBOOL bReAddToHash )
{
	for( TArray<INT>::TConstIterator It(InInstancesUpdated); It; ++It )
	{
		INT InstanceIndex = *It;
		const FFoliageInstance& Instance = Instances(InstanceIndex);

		// Update this instances' transform in the UInstancedStaticMeshComponent
		if( InstanceClusters.IsValidIndex(Instance.ClusterIndex) )
		{
			FFoliageInstanceCluster& Cluster = InstanceClusters(Instance.ClusterIndex);
			Cluster.ClusterComponent->Modify();
			INT ClusterInstanceDataIndex = Cluster.InstanceIndices.FindItemIndex(InstanceIndex);
			check(ClusterInstanceDataIndex != INDEX_NONE);					
			check(Cluster.ClusterComponent->PerInstanceSMData.IsValidIndex(ClusterInstanceDataIndex));
			Cluster.ClusterComponent->bNeedsReattach = TRUE;

			// Update bounds
			FMatrix InstanceTransform = Instance.GetInstanceTransform();
			Cluster.Bounds = Cluster.Bounds + Cluster.ClusterComponent->StaticMesh->Bounds.TransformBy(InstanceTransform);
			// Update transform in InstancedStaticMeshComponent
			Cluster.ClusterComponent->PerInstanceSMData(ClusterInstanceDataIndex).Transform = InstanceTransform;
			Cluster.ClusterComponent->InvalidateLightingCache();
		}
		
		if( bReAddToHash )
		{
			// Re-add instance to the hash
			InstanceHash->InsertInstance(Instance.Location, InstanceIndex);
		}
	}
}

void FFoliageMeshInfo::PostMoveInstances( class AInstancedFoliageActor* InIFA, const TArray<INT>& InInstancesMoved )
{
	PostUpdateInstances( InIFA, InInstancesMoved, TRUE );
}

void FFoliageMeshInfo::DuplicateInstances( class AInstancedFoliageActor* InIFA, class UStaticMesh* InMesh, const TArray<INT>& InInstancesToDuplicate )
{
	for( TArray<INT>::TConstIterator It(InInstancesToDuplicate); It; ++It )
	{
		INT InstanceIndex = *It;
		const FFoliageInstance TempInstance = Instances(InstanceIndex);
		AddInstance(InIFA, InMesh, TempInstance);
	}
}

// Destroy existing clusters and reassign all instances to new clusters
void FFoliageMeshInfo::ReallocateClusters( AInstancedFoliageActor* InIFA, UStaticMesh* InMesh )
{
	InIFA->ClearComponents();

	TArray<FFoliageInstance> OldInstances = Instances;
	for( INT Idx=0;Idx<OldInstances.Num();Idx++ )
	{
		if( OldInstances(Idx).ClusterIndex == -1 )
		{
			OldInstances.RemoveSwap(Idx);
			Idx--;
		}
		else
		{
			OldInstances(Idx).ClusterIndex = -1;
		}
	}

	// Remove everything
	InstanceClusters.Empty();
	Instances.Empty();
	InstanceHash->Empty();
	ComponentHash.Empty();
	FreeInstanceIndices.Empty();
	SelectedIndices.Empty();

	// Re-add
	for( INT Idx=0;Idx<OldInstances.Num();Idx++ )
	{
		AddInstance( InIFA, InMesh, OldInstances(Idx) );
	}

	InIFA->ConditionalUpdateComponents();
}

// Update settings in the clusters based on the current settings (eg culling distance, collision, ...)
void FFoliageMeshInfo::UpdateClusterSettings( AInstancedFoliageActor* InIFA )
{
	for( INT ClusterIdx=0;ClusterIdx<InstanceClusters.Num();ClusterIdx++ )
	{
		UInstancedStaticMeshComponent* ClusterComponent = InstanceClusters(ClusterIdx).ClusterComponent;
		ClusterComponent->Modify();
		ClusterComponent->bNeedsReattach = TRUE;

		// Copy settings
		ApplyInstancedFoliageSettings( ClusterComponent );
	}

	InIFA->ConditionalUpdateComponents();
}

void FFoliageMeshInfo::GetInstancesInsideSphere( const FSphere& Sphere, TArray<INT>& OutInstances )
{
	TSet<INT> TempInstances;
	InstanceHash->GetInstancesOverlappingBox(FBox::BuildAABB(Sphere.Center, FVector(Sphere.W,Sphere.W,Sphere.W)), TempInstances );

	for( TSet<INT>::TConstIterator It(TempInstances); It; ++It )
	{
		if( FSphere(Instances(*It).Location,0.f).IsInside(Sphere) )
		{
			OutInstances.AddItem(*It);
		}
	}	
}

// Returns whether or not there is are any instances overlapping the sphere specified
UBOOL FFoliageMeshInfo::CheckForOverlappingSphere( const FSphere& Sphere )
{
	TSet<INT> TempInstances;
	InstanceHash->GetInstancesOverlappingBox(FBox::BuildAABB(Sphere.Center, FVector(Sphere.W,Sphere.W,Sphere.W)), TempInstances );

	for( TSet<INT>::TConstIterator It(TempInstances); It; ++It )
	{
		if( FSphere(Instances(*It).Location,0.f).IsInside(Sphere) )
		{
			return TRUE;
		}
	}
	return FALSE;
}

// Returns whether or not there is are any instances overlapping the instance specified, excluding the set of instances provided
UBOOL FFoliageMeshInfo::CheckForOverlappingInstanceExcluding( INT TestInstanceIdx, FLOAT Radius, TSet<INT>& ExcludeInstances )
{
	FSphere Sphere( Instances(TestInstanceIdx).Location,Radius);
	TSet<INT> TempInstances;
	InstanceHash->GetInstancesOverlappingBox(FBox::BuildAABB(Sphere.Center, FVector(Sphere.W,Sphere.W,Sphere.W)), TempInstances );

	for( TSet<INT>::TConstIterator It(TempInstances); It; ++It )
	{
		if( *It != TestInstanceIdx && !ExcludeInstances.Contains(*It) && FSphere(Instances(*It).Location,0.f).IsInside(Sphere) )
		{
			return TRUE;
		}
	}
	return FALSE;
}

void FFoliageMeshInfo::SelectInstances( AInstancedFoliageActor* InIFA, UBOOL bSelect, TArray<INT>& Instances )
{
	InIFA->Modify();

	if( bSelect )
	{
		for( INT i=0;i<Instances.Num();i++ )
		{
			SelectedIndices.AddUniqueItem(Instances(i));
		}

		for( INT ClusterIdx=0;ClusterIdx<InstanceClusters.Num();ClusterIdx++ )
		{
			FFoliageInstanceCluster& Cluster = InstanceClusters(ClusterIdx);

			// Apply any selections in the component
			Cluster.ClusterComponent->Modify();
			Cluster.ClusterComponent->bNeedsReattach = TRUE;
			if( Cluster.ClusterComponent->SelectedInstances.Num() != Cluster.InstanceIndices.Num() )
			{
				Cluster.ClusterComponent->SelectedInstances.Init(FALSE, Cluster.InstanceIndices.Num());
			}

			for( INT ClusterInstanceIdx=0;ClusterInstanceIdx<Cluster.InstanceIndices.Num();ClusterInstanceIdx++ )
			{
				INT InstanceIdx = Cluster.InstanceIndices(ClusterInstanceIdx);
				if( Instances.FindItemIndex(InstanceIdx) != INDEX_NONE )
				{
					Cluster.ClusterComponent->SelectedInstances(ClusterInstanceIdx) = TRUE;
				}
			}
		}
	}
	else
	{
		for( INT i=0;i<Instances.Num();i++ )
		{
			SelectedIndices.RemoveSingleItemSwap(Instances(i));
		}

		for( INT ClusterIdx=0;ClusterIdx<InstanceClusters.Num();ClusterIdx++ )
		{
			FFoliageInstanceCluster& Cluster = InstanceClusters(ClusterIdx);

			if( Cluster.ClusterComponent->SelectedInstances.Num() != 0 )
			{
				// Remove any selections from the component
				Cluster.ClusterComponent->Modify();
				Cluster.ClusterComponent->bNeedsReattach = TRUE;

				for( INT ClusterInstanceIdx=0;ClusterInstanceIdx<Cluster.InstanceIndices.Num();ClusterInstanceIdx++ )
				{
					INT InstanceIdx = Cluster.InstanceIndices(ClusterInstanceIdx);
					if( Instances.FindItemIndex(InstanceIdx) != INDEX_NONE )
					{
						Cluster.ClusterComponent->SelectedInstances(ClusterInstanceIdx) = FALSE;
					}
				}
			}
		}
	}

	InIFA->ConditionalUpdateComponents();
}


//
// AInstancedFoliageActor
//

// Get the instanced foliage actor for the current streaming level.
AInstancedFoliageActor* AInstancedFoliageActor::GetInstancedFoliageActor(UBOOL bCreateIfNone)
{
	for( TObjectIterator<AInstancedFoliageActor> It;It;++It )
	{
		if( It->GetOuter() == GWorld->CurrentLevel && !It->bDeleteMe )
		{
			return *It;
		}
	}

	return bCreateIfNone ? Cast<AInstancedFoliageActor>(GWorld->SpawnActor( AInstancedFoliageActor::StaticClass() )) : NULL;
}

// Get the instanced foliage actor for the specified streaming level. Never creates a new IFA.
AInstancedFoliageActor* AInstancedFoliageActor::GetInstancedFoliageActorForLevel(ULevel* InLevel)
{
	for( TObjectIterator<AInstancedFoliageActor> It;It;++It )
	{
		if( It->GetOuter() == InLevel && !It->bDeleteMe )
		{
			return *It;
		}
	}
	return NULL;
}

void AInstancedFoliageActor::SnapInstancesForLandscape( class ULandscapeHeightfieldCollisionComponent* InLandscapeComponent, const FBox& InInstanceBox )
{
	for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TIterator MeshIt(FoliageMeshes); MeshIt; ++MeshIt )
	{
		// Find the per-mesh info matching the mesh.
		FFoliageMeshInfo& MeshInfo = MeshIt.Value();

		FFoliageComponentHashInfo* ComponentHashInfo = MeshInfo.ComponentHash.Find(InLandscapeComponent);
		if( ComponentHashInfo )
		{
			FLOAT TraceExtentSize = InLandscapeComponent->CachedBoxSphereBounds.SphereRadius * 2.f + 10.f; // extend a little
			FVector TraceVector = InLandscapeComponent->LocalToWorld.GetAxis(2).SafeNormal() * TraceExtentSize;

			// Debug for visualizing the instances snapped
			// GWorld->LineBatcher->DrawBox(InInstanceBox, FMatrix::Identity, FColor(0,128,0), SDPG_World);

			UBOOL bFirst = TRUE;
			UBOOL bUpdatedInstances = FALSE;
			TArray<INT> InstancesToRemove;
			for( TSet<INT>::TConstIterator InstIt(ComponentHashInfo->Instances); InstIt; ++InstIt )
			{
				INT InstanceIndex = *InstIt;
				FFoliageInstance& Instance = MeshInfo.Instances(InstanceIndex);

				// Test location should remove any Z offset
				FVector TestLocation = Abs(Instance.ZOffset) > KINDA_SMALL_NUMBER
					?	Instance.GetInstanceTransform().TransformFVector(FVector(0,0,-Instance.ZOffset))
					:	Instance.Location;

				if( InInstanceBox.IsInside(TestLocation) )
				{
					if( bFirst )
					{
						bFirst = FALSE;
						Modify();
					}

					FCheckResult Hit;
					FVector Start = TestLocation + TraceVector;
					FVector End   = TestLocation - TraceVector;

					if( !InLandscapeComponent->LineCheck(Hit, End, Start, FVector(0,0,0), 0 ) )
					{
						if( (TestLocation - Hit.Location).SizeSquared() > KINDA_SMALL_NUMBER )
						{
							// GWorld->LineBatcher->DrawLine(Start, End, FColor(128,0,0), SDPG_World);

							// Remove instance location from the hash. Do not need to update ComponentHash as we re-add below.
							MeshInfo.InstanceHash->RemoveInstance(Instance.Location, InstanceIndex);
							
							// Update the instance editor data
							Instance.Location = Hit.Location;

							if( Instance.Flags & FOLIAGE_AlignToNormal )
							{
								// Remove previous alignment and align to new normal.
								Instance.Rotation = Instance.PreAlignRotation;
								Instance.AlignToNormal(Hit.Normal);
							}

							// Reapply the Z offset in local space
							if( Abs(Instance.ZOffset) > KINDA_SMALL_NUMBER )
							{
								Instance.Location = Instance.GetInstanceTransform().TransformFVector(FVector(0,0,Instance.ZOffset));
							}

							// Todo: add do validation with other parameters such as max/min height etc.

							// Update this instances' transform in the UInstancedStaticMeshComponent
							if( MeshInfo.InstanceClusters.IsValidIndex(Instance.ClusterIndex) )
							{
								FFoliageInstanceCluster& Cluster = MeshInfo.InstanceClusters(Instance.ClusterIndex);
								Cluster.ClusterComponent->Modify();
								INT ClusterInstanceDataIndex = Cluster.InstanceIndices.FindItemIndex(InstanceIndex);
								check(ClusterInstanceDataIndex != INDEX_NONE);					
								check(Cluster.ClusterComponent->PerInstanceSMData.IsValidIndex(ClusterInstanceDataIndex));
								Cluster.ClusterComponent->bNeedsReattach = TRUE;
								bUpdatedInstances = TRUE;
								// Update bounds
								FMatrix InstanceTransform = Instance.GetInstanceTransform();
								Cluster.Bounds = Cluster.Bounds + Cluster.ClusterComponent->StaticMesh->Bounds.TransformBy(InstanceTransform);
								// Update transform in InstancedStaticMeshComponent
								Cluster.ClusterComponent->PerInstanceSMData(ClusterInstanceDataIndex).Transform = InstanceTransform;
								Cluster.ClusterComponent->InvalidateLightingCache();
							}

							// Re-add the new instance location to the hash
							MeshInfo.InstanceHash->InsertInstance(Instance.Location, InstanceIndex);
						}
					}
					else
					{
						// Couldn't find new spot - remove instance
						InstancesToRemove.AddItem(InstanceIndex);
					}
				}
			}

			// Remove any unused instances
			MeshInfo.RemoveInstances(this, InstancesToRemove);

			if( bUpdatedInstances )
			{
				ConditionalUpdateComponents();
			}
		}
	}
}

void AInstancedFoliageActor::MoveInstancesForMovedComponent( class UActorComponent* InComponent )
{
	UBOOL bUpdatedInstances = FALSE;
	UBOOL bFirst = TRUE;

	for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TIterator MeshIt(FoliageMeshes); MeshIt; ++MeshIt )
	{
		FFoliageMeshInfo& MeshInfo = MeshIt.Value();
		FFoliageComponentHashInfo* ComponentHashInfo = MeshInfo.ComponentHash.Find(InComponent);
		if( ComponentHashInfo )
		{
			if( bFirst )
			{
				bFirst = FALSE;
				Modify();
			}

			FVector OldLocation = ComponentHashInfo->CachedLocation;
			FRotator OldRotation = ComponentHashInfo->CachedRotation;
			FVector OldDrawScale = ComponentHashInfo->CachedDrawScale;
			ComponentHashInfo->UpdateLocationFromActor(InComponent);

			for( TSet<INT>::TConstIterator InstIt(ComponentHashInfo->Instances); InstIt; ++InstIt )
			{
				INT InstanceIndex = *InstIt;
				FFoliageInstance& Instance = MeshInfo.Instances(InstanceIndex);

				MeshInfo.InstanceHash->RemoveInstance(Instance.Location, InstanceIndex);
				
				// Apply change
				FMatrix DeltaTransform = 
					FRotationMatrix(Instance.Rotation) * 
					FTranslationMatrix(Instance.Location) *
					FTranslationMatrix(-OldLocation) *
					FInverseRotationMatrix(OldRotation) *
					FScaleMatrix(ComponentHashInfo->CachedDrawScale / OldDrawScale) *
					FRotationMatrix(ComponentHashInfo->CachedRotation) *
					FTranslationMatrix(ComponentHashInfo->CachedLocation);

				// Extract rotation and position
				Instance.Location = DeltaTransform.GetOrigin();
				Instance.Rotation = DeltaTransform.Rotator();

				// Update this instances' transform in the UInstancedStaticMeshComponent
				if( MeshInfo.InstanceClusters.IsValidIndex(Instance.ClusterIndex) )
				{
					FFoliageInstanceCluster& Cluster = MeshInfo.InstanceClusters(Instance.ClusterIndex);
					Cluster.ClusterComponent->Modify();
					INT ClusterInstanceDataIndex = Cluster.InstanceIndices.FindItemIndex(InstanceIndex);
					check(ClusterInstanceDataIndex != INDEX_NONE);					
					check(Cluster.ClusterComponent->PerInstanceSMData.IsValidIndex(ClusterInstanceDataIndex));
					Cluster.ClusterComponent->bNeedsReattach = TRUE;
					bUpdatedInstances = TRUE;
					// Update bounds
					FMatrix InstanceTransform = Instance.GetInstanceTransform();
					Cluster.Bounds = Cluster.Bounds + Cluster.ClusterComponent->StaticMesh->Bounds.TransformBy(InstanceTransform);
					// Update transform in InstancedStaticMeshComponent
					Cluster.ClusterComponent->PerInstanceSMData(ClusterInstanceDataIndex).Transform = InstanceTransform;
					Cluster.ClusterComponent->InvalidateLightingCache();
				}

				// Re-add the new instance location to the hash
				MeshInfo.InstanceHash->InsertInstance(Instance.Location, InstanceIndex);
			}	
		}
	}

	if( bUpdatedInstances )
	{
		ConditionalUpdateComponents();
	}
}

void AInstancedFoliageActor::DeleteInstancesForComponent( class UActorComponent* InComponent )
{
	for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TIterator MeshIt(FoliageMeshes); MeshIt; ++MeshIt )
	{
		FFoliageMeshInfo& MeshInfo = MeshIt.Value();
		const FFoliageComponentHashInfo* ComponentHashInfo = MeshInfo.ComponentHash.Find(InComponent);
		if( ComponentHashInfo )
		{
			MeshInfo.RemoveInstances(this, ComponentHashInfo->Instances.Array());
		}
	}
}

void AInstancedFoliageActor::MoveInstancesForComponentToCurrentLevel( class UActorComponent* InComponent )
{
	AInstancedFoliageActor* NewIFA = AInstancedFoliageActor::GetInstancedFoliageActor(TRUE);

	for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TIterator MeshIt(FoliageMeshes); MeshIt; ++MeshIt )
	{
		FFoliageMeshInfo& MeshInfo = MeshIt.Value();

		const FFoliageComponentHashInfo* ComponentHashInfo = MeshInfo.ComponentHash.Find(InComponent);
		if( ComponentHashInfo )
		{
			FFoliageMeshInfo* NewMeshInfo = NewIFA->FoliageMeshes.Find( MeshIt.Key() );
			if( !NewMeshInfo )
			{
				NewMeshInfo = NewIFA->AddMesh( MeshIt.Key() );
			}

			// Add the foliage to the new level
			for( TSet<INT>::TConstIterator InstIt(ComponentHashInfo->Instances); InstIt; ++InstIt )
			{
				INT InstanceIndex = *InstIt;
				NewMeshInfo->AddInstance( NewIFA, MeshIt.Key(), MeshInfo.Instances(InstanceIndex) );
			}

			// Remove from old level
			MeshInfo.RemoveInstances(this, ComponentHashInfo->Instances.Array());
		}
	}
}

TMap<class UStaticMesh*,TArray<const FFoliageInstancePlacementInfo*> > AInstancedFoliageActor::GetInstancesForComponent( class UActorComponent* InComponent )
{
	TMap<class UStaticMesh*,TArray<const struct FFoliageInstancePlacementInfo*> > Result;

	for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TConstIterator MeshIt(FoliageMeshes); MeshIt; ++MeshIt )
	{
		const FFoliageMeshInfo& MeshInfo = MeshIt.Value();

		const FFoliageComponentHashInfo* ComponentHashInfo = MeshInfo.ComponentHash.Find(InComponent);
		if( ComponentHashInfo )
		{
			TArray<const FFoliageInstancePlacementInfo*>& Array = Result.Set(MeshIt.Key(),TArray<const FFoliageInstancePlacementInfo*>() );
			Array.Empty(ComponentHashInfo->Instances.Num());

			for( TSet<INT>::TConstIterator InstIt(ComponentHashInfo->Instances); InstIt; ++InstIt )
			{
				INT InstanceIndex = *InstIt;
				const FFoliageInstancePlacementInfo* Instance = &MeshInfo.Instances(InstanceIndex);
				Array.AddItem(Instance);
			}
		}
	}

	return Result;
}

struct FFoliageMeshInfo* AInstancedFoliageActor::AddMesh( class UStaticMesh* InMesh )
{
	check( FoliageMeshes.Find(InMesh) == NULL );

	MarkPackageDirty();

	INT MaxDisplayOrder = 0;
	for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TIterator It(FoliageMeshes); It; ++It )
	{
		if( It.Value().Settings->DisplayOrder > MaxDisplayOrder )
		{
			MaxDisplayOrder = It.Value().Settings->DisplayOrder;
		}
	}

	FFoliageMeshInfo& MeshInfo = FoliageMeshes.Set(InMesh, FFoliageMeshInfo());
#if WITH_EDITORONLY_DATA
	UInstancedFoliageSettings* DefaultSettings = InMesh->FoliageDefaultSettings;
	if( DefaultSettings != NULL )
	{
		MeshInfo.Settings = Cast<UInstancedFoliageSettings>(StaticDuplicateObject(DefaultSettings,DefaultSettings,this,TEXT("None")));
	}
	else
#endif
	{
		MeshInfo.Settings = ConstructObject<UInstancedFoliageSettings>(UInstancedFoliageSettings::StaticClass(), this);
	}
	MeshInfo.Settings->IsSelected = TRUE;
	MeshInfo.Settings->DisplayOrder = MaxDisplayOrder+1;

	return &MeshInfo;
}

void AInstancedFoliageActor::RemoveMesh( class UStaticMesh* InMesh )
{
	Modify();
	MarkPackageDirty();
	ClearComponents();
	FoliageMeshes.Remove(InMesh);
	UpdateComponentsInternal();
	CheckSelection();
}

void AInstancedFoliageActor::SelectInstance( class UInstancedStaticMeshComponent* InComponent, INT InComponentInstanceIndex, UBOOL bToggle )
{
	UBOOL bNeedsUpdate = FALSE;

	Modify();

	// If we're not toggling, we need to first deselect everything else
	if( !bToggle )
	{
		for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TIterator MeshIt(FoliageMeshes); MeshIt; ++MeshIt )
		{
			FFoliageMeshInfo& Mesh = MeshIt.Value();

			if( Mesh.SelectedIndices.Num() > 0 )
			{
				for( INT ClusterIdx=0;ClusterIdx<Mesh.InstanceClusters.Num();ClusterIdx++ )
				{
					FFoliageInstanceCluster& Cluster = Mesh.InstanceClusters(ClusterIdx);
					if( Cluster.ClusterComponent->SelectedInstances.Num() > 0 )
					{
						Cluster.ClusterComponent->Modify();
						Cluster.ClusterComponent->SelectedInstances.Empty();
						Cluster.ClusterComponent->bNeedsReattach = TRUE;
					}
				}
				bNeedsUpdate = TRUE;
			}

			Mesh.SelectedIndices.Empty();
		}
	}

	if( InComponent )
	{
		FFoliageMeshInfo* Mesh = FoliageMeshes.Find(InComponent->StaticMesh);

		if( Mesh )
		{
			for( INT ClusterIdx=0;ClusterIdx<Mesh->InstanceClusters.Num();ClusterIdx++ )
			{
				FFoliageInstanceCluster& Cluster = Mesh->InstanceClusters(ClusterIdx);
				if( Cluster.ClusterComponent == InComponent )
				{
					InComponent->Modify();
					INT InstanceIndex = Cluster.InstanceIndices(InComponentInstanceIndex);
					INT SelectedIndex = Mesh->SelectedIndices.FindItemIndex(InstanceIndex);

					bNeedsUpdate = TRUE;

					// Deselect if it's already selected.
					if( InComponentInstanceIndex < InComponent->SelectedInstances.Num() )
					{
						InComponent->SelectedInstances(InComponentInstanceIndex) = FALSE;
						InComponent->bNeedsReattach = TRUE;
					}
					if( SelectedIndex != INDEX_NONE )
					{
						Mesh->SelectedIndices.Remove(SelectedIndex);
					}

					if( bToggle && SelectedIndex != INDEX_NONE)
					{
						if( SelectedMesh == InComponent->StaticMesh && Mesh->SelectedIndices.Num() == 0 )
						{
							SelectedMesh = NULL;
						}
					}
					else
					{
						// Add the selection
						if( InComponent->SelectedInstances.Num() < InComponent->PerInstanceSMData.Num() )
						{
							InComponent->SelectedInstances.Init(FALSE, Cluster.InstanceIndices.Num());
						}
						InComponent->SelectedInstances(InComponentInstanceIndex) = TRUE;
						InComponent->bNeedsReattach = TRUE;

						SelectedMesh = InComponent->StaticMesh;
						Mesh->SelectedIndices.InsertItem(InstanceIndex,0);
					}
					break;
				}
			}
		}
	}

	if( bNeedsUpdate )
	{
		// Update selection
		UpdateComponentsInternal();
	}

	CheckSelection();
}

void AInstancedFoliageActor::ApplySelectionToComponents( UBOOL bApply )
{
	UBOOL bNeedsUpdate = FALSE;

	for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TIterator MeshIt(FoliageMeshes); MeshIt; ++MeshIt )
	{
		FFoliageMeshInfo& Mesh = MeshIt.Value();

		if( bApply )
		{
			if( Mesh.SelectedIndices.Num() > 0 )
			{
				for( INT ClusterIdx=0;ClusterIdx<Mesh.InstanceClusters.Num();ClusterIdx++ )
				{
					FFoliageInstanceCluster& Cluster = Mesh.InstanceClusters(ClusterIdx);

					// Apply any selections in the component
					Cluster.ClusterComponent->SelectedInstances.Init(FALSE, Cluster.InstanceIndices.Num());
					Cluster.ClusterComponent->bNeedsReattach = TRUE;
					bNeedsUpdate = TRUE;

					for( INT ClusterInstanceIdx=0;ClusterInstanceIdx<Cluster.InstanceIndices.Num();ClusterInstanceIdx++ )
					{
						INT InstanceIdx = Cluster.InstanceIndices(ClusterInstanceIdx);
						if( Mesh.SelectedIndices.FindItemIndex(InstanceIdx) != INDEX_NONE )
						{
							Cluster.ClusterComponent->SelectedInstances(ClusterInstanceIdx) = TRUE;
						}
					}
				}
			}
		}
		else
		{
			for( INT ClusterIdx=0;ClusterIdx<Mesh.InstanceClusters.Num();ClusterIdx++ )
			{
				FFoliageInstanceCluster& Cluster = Mesh.InstanceClusters(ClusterIdx);

				if( Cluster.ClusterComponent->SelectedInstances.Num() > 0 )
				{
					// remove any selections in the component
					Cluster.ClusterComponent->SelectedInstances.Empty();
					Cluster.ClusterComponent->bNeedsReattach = TRUE;
					bNeedsUpdate = TRUE;
				}
			}
		}
	}

	if( bNeedsUpdate )
	{
		// Update selection
		UpdateComponentsInternal();
	}
}

void AInstancedFoliageActor::CheckSelection()
{
	// Check if we have to change the selection.
	if( SelectedMesh != NULL )
	{
		FFoliageMeshInfo* Mesh = FoliageMeshes.Find(SelectedMesh);
		if( Mesh && Mesh->SelectedIndices.Num() > 0 )
		{
			return;
		}
	}

	SelectedMesh = NULL;

	// Try to find a new selection
	for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TIterator MeshIt(FoliageMeshes); MeshIt; ++MeshIt )
	{
		FFoliageMeshInfo& Mesh = MeshIt.Value();
		if( Mesh.SelectedIndices.Num() > 0 )
		{
			SelectedMesh = MeshIt.Key();
			return;
		}
	}
}

FVector AInstancedFoliageActor::GetSelectionLocation()
{
	FVector Result(0,0,0);

	if( SelectedMesh != NULL )
	{
		FFoliageMeshInfo* Mesh = FoliageMeshes.Find(SelectedMesh);
		if( Mesh && Mesh->SelectedIndices.Num() > 0 )
		{
			Result = Mesh->Instances(Mesh->SelectedIndices(0)).Location;
		}
	}

	return Result;
}

#endif

void AInstancedFoliageActor::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << FoliageMeshes;
}

void AInstancedFoliageActor::PostLoad()
{
	Super::PostLoad();

	// Lookup ClusterIndex data for all existing instances
	if( GetLinker() && GetLinker()->Ver() < VER_FOLIAGE_INSTANCE_SAVE_EDITOR_DATA )
	{
		for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TIterator MeshIt(FoliageMeshes); MeshIt; ++MeshIt )
		{
			// Find the per-mesh info matching the mesh.
			FFoliageMeshInfo& MeshInfo = MeshIt.Value();

			// Lookup ClusterIndex data for each instance
			for( INT ClusterIdx=0;ClusterIdx<MeshInfo.InstanceClusters.Num();ClusterIdx++ )
			{
				TArray<INT>& InstanceIndices = MeshInfo.InstanceClusters(ClusterIdx).InstanceIndices;
				for( INT InstanceIndexIdx=0; InstanceIndexIdx<InstanceIndices.Num(); InstanceIndexIdx++ )
				{
					MeshInfo.Instances(InstanceIndices(InstanceIndexIdx)).ClusterIndex = ClusterIdx;
				}

				// Also disable selection highlight for meshes
				MeshInfo.InstanceClusters(ClusterIdx).ClusterComponent->bSelectable = FALSE;
			}
		}
	}

	// Create a UInstancedFoliageSettings object for each mesh.
	if( GetLinker() && GetLinker()->Ver() < VER_FOLIAGE_SAVE_UI_DATA )
	{
		INT i=0;
		for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TIterator MeshIt(FoliageMeshes); MeshIt; ++MeshIt )
		{
			FFoliageMeshInfo& MeshInfo = MeshIt.Value();
			// Remove any NULL entries.
			if( MeshIt.Key() == NULL )
			{
				MeshIt.RemoveCurrent();
				continue;
			}

#if WITH_EDITORONLY_DATA
			UInstancedFoliageSettings* DefaultSettings = MeshIt.Key()->FoliageDefaultSettings;
			if( DefaultSettings != NULL )
			{
				MeshInfo.Settings = Cast<UInstancedFoliageSettings>(StaticDuplicateObject(DefaultSettings,DefaultSettings,this,TEXT("None")));
			}
			else
#endif
			{
				MeshInfo.Settings = ConstructObject<UInstancedFoliageSettings>(UInstancedFoliageSettings::StaticClass(), this);
			}
			MeshInfo.Settings->DisplayOrder = i++;
		}
	}

	if( GIsEditor )
	{
		for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TIterator MeshIt(FoliageMeshes); MeshIt; ++MeshIt )
		{
			// Remove any NULL entries.
			if( MeshIt.Key() == NULL )
			{
				MeshIt.RemoveCurrent();
				continue;
			}

			// Find the per-mesh info matching the mesh.
			FFoliageMeshInfo& MeshInfo = MeshIt.Value();

			// Update the FreeInstanceIndices list and hash.
			for( INT InstanceIdx=0; InstanceIdx<MeshInfo.Instances.Num(); InstanceIdx++ )
			{
				FFoliageInstance& Instance = MeshInfo.Instances(InstanceIdx);
				if( Instance.ClusterIndex == -1 )
				{
					// Add invalid instances to the FreeInstanceIndices list.
					MeshInfo.FreeInstanceIndices.AddItem(InstanceIdx);
					Instance.Base = NULL;
				}
				else
				{
					// Add valid instances to the hash.
					MeshInfo.InstanceHash->InsertInstance(Instance.Location, InstanceIdx);
					FFoliageComponentHashInfo& ComponentHashInfo = MeshInfo.ComponentHash.FindOrAddKey(Instance.Base);
					ComponentHashInfo.Instances.Add(InstanceIdx);
				}
			}

			// Changes for selection
			if( GetLinker() && GetLinker()->Ver() < VER_FOLIAGE_LOD )
			{
				for( INT ClusterIdx=0;ClusterIdx<MeshInfo.InstanceClusters.Num();ClusterIdx++ )
				{
					// Set selection highlight and shadowing flags for meshes
					MeshInfo.InstanceClusters(ClusterIdx).ClusterComponent->bSelectable = TRUE;
					MeshInfo.InstanceClusters(ClusterIdx).ClusterComponent->bUsePerInstanceHitProxies = TRUE;
					MeshInfo.InstanceClusters(ClusterIdx).ClusterComponent->bDontResolveInstancedLightmaps = TRUE;
				}
			}

#if _DEBUG
			MeshInfo.CheckValid();
#endif
		}
	}

	// Store references to the cluster components for RTGC when loading for gameplay.
	if( GIsGame )
	{
		for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TIterator MeshIt(FoliageMeshes); MeshIt; ++MeshIt )
		{
			FFoliageMeshInfo& MeshInfo = MeshIt.Value();
			for( INT ClusterIdx=0;ClusterIdx<MeshInfo.InstanceClusters.Num();ClusterIdx++ )
			{
				InstancedStaticMeshComponents.AddItem(MeshInfo.InstanceClusters(ClusterIdx).ClusterComponent);
			}
		}
	}
}

void AInstancedFoliageActor::UpdateComponentsInternal(UBOOL bCollisionUpdate)
{
	Super::UpdateComponentsInternal(bCollisionUpdate);

	for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TIterator It(FoliageMeshes); It; ++It )
	{
		FFoliageMeshInfo& MeshInfo = It.Value();
		for( INT ClusterIndex=0;ClusterIndex < MeshInfo.InstanceClusters.Num(); ClusterIndex++ )
		{
			UInstancedStaticMeshComponent* Component = MeshInfo.InstanceClusters(ClusterIndex).ClusterComponent;
			if( Component )
			{
				Component->UpdateComponent(GWorld->Scene,this,FMatrix::Identity);
			}
		}
	}
}

void AInstancedFoliageActor::ClearComponents()
{
	// wait until resources are released
	FlushRenderingCommands();

	Super::ClearComponents();

	for( TMap<class UStaticMesh*, struct FFoliageMeshInfo>::TIterator It(FoliageMeshes); It; ++It )
	{
		FFoliageMeshInfo& MeshInfo = It.Value();
		for( INT ClusterIndex=0;ClusterIndex < MeshInfo.InstanceClusters.Num(); ClusterIndex++ )
		{
			UInstancedStaticMeshComponent* Component = MeshInfo.InstanceClusters(ClusterIndex).ClusterComponent;
			if( Component )
			{
				Component->ConditionalDetach();
			}
		}
	}
}

/** InstancedStaticMeshInstance hit proxy */
void HInstancedStaticMeshInstance::Serialize(FArchive& Ar)
{
	Ar << Component;
}
