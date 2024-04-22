/*=============================================================================
	UnPhysAsset.cpp: Physics Asset - code for managing articulated assemblies of rigid bodies.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"

IMPLEMENT_CLASS(UPhysicsAsset);
IMPLEMENT_CLASS(URB_BodySetup);

IMPLEMENT_CLASS(UPhysicsAssetInstance);
IMPLEMENT_CLASS(URB_BodyInstance);

IMPLEMENT_CLASS(UPhysicalMaterial);
IMPLEMENT_CLASS(UPhysicalMaterialPropertyBase);


#if WITH_NOVODEX
	#include "UnNovodexSupport.h"

#if PERF_SHOW_PHYS_INIT_COSTS
extern DOUBLE TotalCreateActorTime;
#endif

#define PERF_SHOW_SLOW_RELEASE_TIMES 0
#define PERF_SHOW_SLOW_RELEASE_TIMES_AMOUNT 0.1f

#endif // WITH_NOVODEX


///////////////////////////////////////	
//////////// UPhysicsAsset ////////////
///////////////////////////////////////

UBOOL UPhysicsAsset::LineCheck(FCheckResult& Result, class USkeletalMeshComponent* SkelComp, const FVector& Start, const FVector& End, const FVector& Extent, UBOOL bPerPolyShapes)
{
	FVector Scale3D = SkelComp->Scale * SkelComp->Scale3D;
	AActor* SkelOwner = SkelComp->GetOwner();
	if( SkelOwner != NULL )
	{
		Scale3D *= SkelOwner ->DrawScale * SkelOwner->DrawScale3D;
	}

	if( !Scale3D.IsUniform() )
	{
		debugf( NAME_DevCollision, TEXT("UPhysicsAsset::LineCheck : Non-uniform scale factor. You will not be able to collide with it.  Turn off collision and wrap it with a blocking volume. SkelComp %s (%s) Scale %f Scale3D %s Owner %s Scale %f Scale3D %s"), 
			*SkelComp->GetName(),
			*SkelComp->GetDetailedInfo(),
			 SkelComp->Scale,
			*SkelComp->Scale3D.ToString(),
			 SkelOwner ? *SkelOwner->GetName() : TEXT("NULL"),
			 SkelOwner ? SkelOwner->DrawScale : 0.f,
			 SkelOwner ? *SkelOwner->DrawScale3D.ToString() : TEXT("N/A") );
		return 1;
	}

	UBOOL bIsZeroExtent = Extent.IsZero();

	Result.Item = INDEX_NONE;
	Result.LevelIndex = INDEX_NONE;
	Result.Time = 1.0f;
	Result.BoneName = NAME_None;
	Result.Component = NULL;
	Result.Material = NULL;
	Result.PhysMaterial = NULL;

	FCheckResult TempResult;

	for(INT i=0; i<BodySetup.Num(); i++)
	{
		URB_BodySetup* bs = BodySetup(i);

		if( (bIsZeroExtent && !bs->bBlockZeroExtent) || (!bIsZeroExtent && !bs->bBlockNonZeroExtent) )
		{
			continue;
		}

		// Find the index for the bone that matches this body setup
		INT BoneIndex = SkelComp->MatchRefBone(bs->BoneName);
		if(BoneIndex != INDEX_NONE)
		{
			FMatrix WorldBoneTM = SkelComp->GetBoneMatrix(BoneIndex);

			// Ignore bones that have been scaled to zero.
			FLOAT Det = WorldBoneTM.RotDeterminant();
			if(Abs(Det) > KINDA_SMALL_NUMBER)
			{
				WorldBoneTM.RemoveScaling();

				TempResult.Time = 1.0f;

				bs->AggGeom.LineCheck(TempResult, WorldBoneTM, Scale3D, End, Start, Extent, FALSE, bPerPolyShapes);

				if(TempResult.Time < Result.Time)
				{
					Result = TempResult;
					Result.Item = i;
					Result.BoneName = bs->BoneName;
					Result.Component = SkelComp;
					Result.Actor = SkelComp->GetOwner();


					if( SkelComp->PhysicsAssetInstance != NULL )
					{
						check(SkelComp->PhysicsAssetInstance->Bodies.Num() == BodySetup.Num());
						Result.PhysMaterial = SkelComp->PhysicsAssetInstance->Bodies(Result.Item)->GetPhysicalMaterial();
					}
					else
					{
						// We can not know which material we hit without doing a per poly line check and that is SLOW
						Result.PhysMaterial = BodySetup(Result.Item)->PhysMaterial;
						if( SkelComp->PhysMaterialOverride != NULL )
						{
							Result.PhysMaterial = SkelComp->PhysMaterialOverride;
						}
					}					
				}
			}
		}
	}

	if(Result.Time < 1.0f)
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

FCheckResult* UPhysicsAsset::LineCheckAllInteractions( FMemStack& Mem, class USkeletalMeshComponent* SkelComp, const FVector& Start, const FVector& End, const FVector& Extent, UBOOL bPerPolyShapes )
{
	FVector Scale3D = SkelComp->Scale * SkelComp->Scale3D;
	AActor* SkelOwner = SkelComp->GetOwner();
	if( SkelOwner != NULL )
	{
		Scale3D *= SkelOwner ->DrawScale * SkelOwner->DrawScale3D;
	}

	if( !Scale3D.IsUniform() )
	{
		debugf( NAME_DevCollision, TEXT( "UPhysicsAsset::LineCheck : Non-uniform scale factor. You will not be able to collide with it.  Turn off collision and wrap it with a blocking volume. SkelComp %s (%s) Scale %f Scale3D %s Owner %s Scale %f Scale3D %s"), 
			*SkelComp->GetName(),
			*SkelComp->GetDetailedInfo(),
			SkelComp->Scale,
			*SkelComp->Scale3D.ToString(),
			SkelOwner ? *SkelOwner->GetName() : TEXT("NULL"),
			SkelOwner ? SkelOwner->DrawScale : 0.f,
			SkelOwner ? *SkelOwner->DrawScale3D.ToString() : TEXT("N/A") );

		return NULL;
	}

	UBOOL bIsZeroExtent = Extent.IsZero();

	FCheckResult Hits[32];
	INT			 NumHits = 0;
	FCheckResult TempResult;

	for( INT i = 0; i < BodySetup.Num() && NumHits < ARRAY_COUNT(Hits); i++ )
	{
		URB_BodySetup* bs = BodySetup(i);

		if( (bIsZeroExtent && !bs->bBlockZeroExtent) || (!bIsZeroExtent && !bs->bBlockNonZeroExtent) )
		{
			continue;
		}

		// Find the index for the bone that matches this body setup
		INT BoneIndex = SkelComp->MatchRefBone(bs->BoneName);
		if( BoneIndex != INDEX_NONE )
		{
			FMatrix WorldBoneTM = SkelComp->GetBoneMatrix(BoneIndex);

			// Ignore bones that have been scaled to zero.
			FLOAT Det = WorldBoneTM.RotDeterminant();
			if(Abs(Det) > KINDA_SMALL_NUMBER)
			{
				WorldBoneTM.RemoveScaling();


				TempResult.Time = 1.0f;

				bs->AggGeom.LineCheck(TempResult, WorldBoneTM, Scale3D, End, Start, Extent, FALSE, bPerPolyShapes);

				if( TempResult.Time < 1.f )
				{
					Hits[NumHits] = TempResult;
					Hits[NumHits].Item = i;
					Hits[NumHits].BoneName = bs->BoneName;
					Hits[NumHits].Component = SkelComp;
					Hits[NumHits].Actor = SkelComp->GetOwner();

					if( SkelComp->PhysicsAssetInstance != NULL )
					{
						check(SkelComp->PhysicsAssetInstance->Bodies.Num() == BodySetup.Num());
						Hits[NumHits].PhysMaterial = SkelComp->PhysicsAssetInstance->Bodies(Hits[NumHits].Item)->GetPhysicalMaterial();
					}
					else
					{
						// We can not know which material we hit without doing a per poly line check and that is SLOW
						Hits[NumHits].PhysMaterial = BodySetup(Hits[NumHits].Item)->PhysMaterial;
						if( SkelComp->PhysMaterialOverride != NULL )
						{
							Hits[NumHits].PhysMaterial = SkelComp->PhysMaterialOverride;
						}
					}

					NumHits++;
				}
			}
		}
	}

	// Sort the list.
	FCheckResult* Result = NULL;
	if( NumHits )
	{
		SCOPE_CYCLE_COUNTER(STAT_Col_Sort);
		appQsort( Hits, NumHits, sizeof(Hits[0]), (QSORT_COMPARE)FCheckResult::CompareHits );
		Result = new(Mem,NumHits)FCheckResult;
		for( INT i=0; i<NumHits; i++ )
		{
			Result[i]      = Hits[i];
			Result[i].Next = (i+1<NumHits) ? &Result[i+1] : NULL;
		}
	}

	return Result;
}

UBOOL UPhysicsAsset::PointCheck(FCheckResult& Result, class USkeletalMeshComponent* SkelComp, const FVector& Location, const FVector& Extent)
{
	FVector Scale3D = SkelComp->Scale * SkelComp->Scale3D;
	if (SkelComp->GetOwner() != NULL)
	{
		Scale3D *= SkelComp->GetOwner()->DrawScale * SkelComp->GetOwner()->DrawScale3D;
	}

	Result.Time = 1.0f;

	FCheckResult TempResult;
	UBOOL bHit = FALSE;

	for(INT i=0; i<BodySetup.Num(); i++)
	{
		URB_BodySetup* bs = BodySetup(i);

		// Find the index for the bone that matches this body setup
		INT BoneIndex = SkelComp->MatchRefBone(bs->BoneName);
		if(bs->bBlockNonZeroExtent && BoneIndex != INDEX_NONE)
		{
			FMatrix WorldBoneTM = SkelComp->GetBoneMatrix(BoneIndex);

			// Ignore bones that have been scaled to zero.
			FLOAT Det = WorldBoneTM.RotDeterminant();
			if(Abs(Det) > KINDA_SMALL_NUMBER)
			{
				WorldBoneTM.RemoveScaling();

				bHit = !bs->AggGeom.PointCheck(TempResult, WorldBoneTM, Scale3D, Location, Extent);

				// If we got a hit, fill in the result.
				if(bHit)
				{
					Result = TempResult;
					Result.Item = i;
					Result.BoneName = bs->BoneName;
					Result.Component = SkelComp;
					Result.Actor = SkelComp->GetOwner();
					// Grab physics material from BodySetup.
					Result.PhysMaterial = BodySetup(Result.Item)->PhysMaterial;

					// Currently we just return the first hit we find. Is this bad?
					break;
				}
			}
		}
	}

	return !bHit;
}


FBox UPhysicsAsset::CalcAABB(USkeletalMeshComponent* SkelComp)
{
	FBox Box(0);

	FVector Scale3D = SkelComp->Scale * SkelComp->Scale3D;
	if (SkelComp->GetOwner() != NULL)
	{
		Scale3D *= SkelComp->GetOwner()->DrawScale * SkelComp->GetOwner()->DrawScale3D;
	}

	if( Scale3D.IsUniform() )
	{
		TArray<INT>* BodyIndexRefs = NULL;
		TArray<INT> AllBodies;
		// If we want to consider all bodies, make array with all body indices in
		if(SkelComp->bConsiderAllBodiesForBounds)
		{
			AllBodies.Add(BodySetup.Num());
			for(INT i=0; i<BodySetup.Num();i ++)
			{
				AllBodies(i) = i;
			}
			BodyIndexRefs = &AllBodies;
		}
		// Otherwise, use the cached shortlist of bodies to consider
		else
		{
			BodyIndexRefs = &BoundsBodies;
		}

		// Then iterate over bodies we want to consider, calculating bounding box for each
		const INT BodySetupNum = (*BodyIndexRefs).Num();

		for(INT i=0; i<BodySetupNum; i++)
		{
			const INT BodyIndex = (*BodyIndexRefs)(i);
			URB_BodySetup* bs = BodySetup(BodyIndex);

			if (i+1<BodySetupNum)
			{
				INT NextIndex = (*BodyIndexRefs)(i+1);
				CONSOLE_PREFETCH( BodySetup(NextIndex) );
				CONSOLE_PREFETCH_NEXT_CACHE_LINE( BodySetup(NextIndex) );
			}

			INT BoneIndex = SkelComp->MatchRefBone(bs->BoneName);
			if(BoneIndex != INDEX_NONE)
			{
				FMatrix WorldBoneTM = SkelComp->GetBoneMatrix(BoneIndex);
				if(Abs(WorldBoneTM.RotDeterminant()) > (FLOAT)KINDA_SMALL_NUMBER)
				{
					WorldBoneTM.RemoveScaling();
					Box += bs->AggGeom.CalcAABB( WorldBoneTM, Scale3D );
				}
			}
		}
	}
	else
	{
		debugf( TEXT("UPhysicsAsset::CalcAABB : Non-uniform scale factor. You will not be able to collide with it.  Turn off collision and wrap it with a blocking volume.  SkelComp: %s  SkelMesh: %s"), *SkelComp->GetFullName(), *SkelComp->SkeletalMesh->GetFullName() );
	}

	if(!Box.IsValid)
	{
		Box = FBox( SkelComp->LocalToWorld.GetOrigin(), SkelComp->LocalToWorld.GetOrigin() );
	}

	return Box;
}

// Find the index of the physics bone that is controlling this graphics bone.
INT	UPhysicsAsset::FindControllingBodyIndex(class USkeletalMesh* skelMesh, INT StartBoneIndex)
{
	INT BoneIndex = StartBoneIndex;
	while(1)
	{
		FName BoneName = skelMesh->RefSkeleton(BoneIndex).Name;
		INT BodyIndex = FindBodyIndex(BoneName);

		if(BodyIndex != INDEX_NONE)
			return BodyIndex;

		INT ParentBoneIndex = skelMesh->RefSkeleton(BoneIndex).ParentIndex;

		if(ParentBoneIndex == BoneIndex)
			return INDEX_NONE;

		BoneIndex = ParentBoneIndex;
	}
}


INT UPhysicsAsset::FindBodyIndex(FName bodyName)
{
	check( BodySetup.Num() == DefaultInstance->Bodies.Num() );

	INT * IdxData = BodySetupIndexMap.Find(bodyName);
	if (IdxData)
	{
		return *IdxData;
	}
	
	return INDEX_NONE;
}

INT UPhysicsAsset::FindConstraintIndex(FName constraintName)
{
	check( ConstraintSetup.Num() == DefaultInstance->Constraints.Num() );

	for(INT i=0; i<ConstraintSetup.Num(); i++)
	{
		if( ConstraintSetup(i)->JointName == constraintName )
			return i;
	}

	return INDEX_NONE;
}

FName UPhysicsAsset::FindConstraintBoneName(INT ConstraintIndex)
{
	check( ConstraintSetup.Num() == DefaultInstance->Constraints.Num() );

	if ( (ConstraintIndex < 0) || (ConstraintIndex >= ConstraintSetup.Num()) )
		return NAME_None;

	return ConstraintSetup(ConstraintIndex)->JointName;
}

/** Utility for getting indices of all bodies below (and including) the one with the supplied name. */
void UPhysicsAsset::GetBodyIndicesBelow(TArray<INT>& OutBodyIndices, FName InBoneName, USkeletalMesh* SkelMesh)
{
	INT BaseIndex = SkelMesh->MatchRefBone(InBoneName);

	// Iterate over all other bodies, looking for 'children' of this one
	for(INT i=0; i<BodySetup.Num(); i++)
	{
		URB_BodySetup* BS = BodySetup(i);
		FName TestName = BS->BoneName;
		INT TestIndex = SkelMesh->MatchRefBone(TestName);

		// We want to return this body as well.
		if(TestIndex == BaseIndex || SkelMesh->BoneIsChildOf(TestIndex, BaseIndex))
		{
			OutBodyIndices.AddItem(i);
		}
	}
}

void UPhysicsAsset::UpdateBodyIndices()
{
	for(INT i=0; i<DefaultInstance->Bodies.Num(); i++)
	{
		DefaultInstance->Bodies(i)->BodyIndex = i;
	}

	// Also update list of bounds bodies
	UpdateBoundsBodiesArray();
}


// Find all the constraints that are connected to a particular body.
void UPhysicsAsset::BodyFindConstraints(INT bodyIndex, TArray<INT>& constraints)
{
	constraints.Empty();
	FName bodyName = BodySetup(bodyIndex)->BoneName;

	for(INT i=0; i<ConstraintSetup.Num(); i++)
	{
		if( ConstraintSetup(i)->ConstraintBone1 == bodyName || ConstraintSetup(i)->ConstraintBone2 == bodyName )
			constraints.AddItem(i);
	}
}

void UPhysicsAsset::ClearShapeCaches()
{
	for(INT i=0; i<BodySetup.Num(); i++)
	{
		BodySetup(i)->ClearShapeCache();
	}
}



///////////////////////////////////////
/////////// URB_BodySetup /////////////
///////////////////////////////////////

void URB_BodySetup::CopyBodyPropertiesFrom(class URB_BodySetup* FromSetup)
{
	CopyMeshPropsFrom(FromSetup);

	PhysMaterial = FromSetup->PhysMaterial;
	COMNudge = FromSetup->COMNudge;
	bFixed = FromSetup->bFixed;
	bNoCollision = FromSetup->bNoCollision;

	bBlockNonZeroExtent = FromSetup->bBlockNonZeroExtent;
	bBlockZeroExtent = FromSetup->bBlockZeroExtent;

	bEnableContinuousCollisionDetection = FromSetup->bEnableContinuousCollisionDetection;
	bAlwaysFullAnimWeight = FromSetup->bAlwaysFullAnimWeight;
	MassScale = FromSetup->MassScale;
	SleepFamily = FromSetup->SleepFamily;

}

// Cleanup all collision geometries currently in this StaticMesh. Must be called before the StaticMesh is destroyed.
void URB_BodySetup::ClearShapeCache()
{
#if WITH_NOVODEX
	// Clear pre-cooked data.
	PreCachedPhysData.Empty();


	// Clear created shapes.
	for (INT i = 0; i < CollisionGeom.Num(); i++)
	{
		NxActorDesc* ActorDesc = (NxActorDesc*) CollisionGeom(i);
		if (ActorDesc)
		{
			// Add any convex shapes being used to the 'pending destroy' list.
			// We can't destroy these now, because during GC we might hit this while some things are still using it.
			for(UINT j=0; j<ActorDesc->shapes.size(); j++)
			{
				NxShapeDesc* ShapeDesc = ActorDesc->shapes[j];

				// If this shape has a CCD skeleton - add it to the list to kill.
				if(ShapeDesc->ccdSkeleton)
				{
					GNovodexPendingKillCCDSkeletons.AddItem(ShapeDesc->ccdSkeleton);
				}
				
				// If its a convex, add its mesh data to the list to kill.
				if(ShapeDesc->getType() == NX_SHAPE_CONVEX)
				{
					NxConvexShapeDesc* ConvexDesc = (NxConvexShapeDesc*)ShapeDesc;
					GNovodexPendingKillConvex.AddItem( ConvexDesc->meshData );
				}

				// Free memory for the ShapeDesc
				delete ShapeDesc;
			}

			// Delete ActorDesc itself
			delete ActorDesc;
			CollisionGeom(i) = NULL;
		}
	}

	CollisionGeom.Empty();
	CollisionGeomScale3D.Empty();
#endif // WITH_NOVODEX
}

/** Pre-cache this mesh at all desired scales. */
void URB_BodySetup::PreCachePhysicsData()
{
	PreCachedPhysData.Empty();

#if WITH_NOVODEX
	// Go over each scale we want to pre-cache data for.
	for(INT i=0; i<PreCachedPhysScale.Num(); i++)
	{
		INT NewDataIndex = PreCachedPhysData.AddZeroed();
		FKCachedConvexData& NewCachedData = PreCachedPhysData(NewDataIndex);

		FVector Scale3D = PreCachedPhysScale(i);
		if(abs(Scale3D.GetMin()) > KINDA_SMALL_NUMBER)
		{
			MakeCachedConvexDataForAggGeom( &NewCachedData, AggGeom.ConvexElems, Scale3D, *GetName() );
		}
	}

	// Save version number
	PreCachedPhysDataVersion = GCurrentCachedPhysDataVersion;
#endif // WITH_NOVODEX
}

void URB_BodySetup::BeginDestroy()
{
	Super::BeginDestroy();

	AggGeom.FreeRenderInfo();
}	

void URB_BodySetup::FinishDestroy()
{
	ClearShapeCache();
	Super::FinishDestroy();
}

void URB_BodySetup::PreSave()
{
	Super::PreSave();

#if WITH_EDITORONLY_DATA
	if(!IsTemplate())
	{
		// Pre-cache physics data at particular scales for this mesh.
		PreCachePhysicsData();
	}
#endif // WITH_EDITORONLY_DATA
}

void URB_BodySetup::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << PreCachedPhysData;
}


void URB_BodySetup::PostLoad()
{
	Super::PostLoad();
}

void URB_BodySetup::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// If we change the CCD flag, we need to create shapes - so we have to flush the shape cache.
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged && (PropertyThatChanged->GetFName()  == FName(TEXT("bEnableContinuousCollisionDetection"))) )
	{
		// This is yucky because we are not actually free'ing the memory for the shapes, but if the game is running,
		// we will crash if we try and destroy the meshes while things are actually using them.
		CollisionGeom.Empty();
		CollisionGeomScale3D.Empty();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/*
 * Create and add a new physics collision representation to this body setup
 * @param Scale3D    - Scale at which this geometry should be created at
 * @param CachedData - Unreal description of the collision geometry to create
 * @param DebugName  - debug name to output
 * returns TRUE if successful
 */
UBOOL URB_BodySetup::AddCollisionFromCachedData(const FVector& Scale3D, FKCachedConvexData* CachedData, const FString& DebugName)
{
#if WITH_NOVODEX
	if(Scale3D.IsNearlyZero())
	{
		debugf(TEXT("URB_BodySetup::AddCollisionFromCachedData : Scale3D is (nearly) zero: %s"), *DebugName);
		return FALSE;
	}

	// Try to find a shape with the matching scale
	for (INT i=0; i < CollisionGeomScale3D.Num(); i++)
	{
		// Found a shape with the right scale
		if ((CollisionGeomScale3D(i) - Scale3D).IsNearlyZero())
		{
			//debugf(TEXT("URB_BodySetup::AddCollisionFromCachedData : Shape: %s already exists at scale [%f %f %f]"), *DebugName, Scale3D.X, Scale3D.Y, Scale3D.Z);
			return FALSE;
		}
	}

#ifdef ENABLE_CCD
	UBOOL bEnableCCD = bEnableContinuousCollisionDetection;
#else
	UBOOL bEnableCCD = FALSE;
#endif

	UBOOL bSuccess = FALSE;

	// Actually instance the novodex geometry from the Unreal description.
	NxActorDesc* GeomActorDesc = AggGeom.InstanceNovodexGeom(Scale3D, CachedData, bEnableCCD, *DebugName);
	if (GeomActorDesc)
	{
		CollisionGeomScale3D.AddItem(Scale3D);
		CollisionGeom.AddItem(GeomActorDesc);
		bSuccess = TRUE;

		//debugf(TEXT("URB_BodySetup::AddCollisionFromCachedData : created new Shape: %s at scale [%f %f %f]"), *DebugName, Scale3D.X, Scale3D.Y, Scale3D.Z);
	}
	else
	{
		debugf(TEXT("URB_BodySetup::AddCollisionFromCachedData : Could not create new Shape: %s at scale [%f %f %f]"), *DebugName, Scale3D.X, Scale3D.Y, Scale3D.Z);
	}

	return bSuccess;
#else
	return FALSE;
#endif //WITH_NOVODEX
}


///////////////////////////////////////
/////////// URB_BodyInstance //////////
///////////////////////////////////////

// Transform is in Unreal scale.
void URB_BodyInstance::InitBody(URB_BodySetup* setup, const FMatrix& transform, const FVector& Scale3D, UBOOL bFixed, UPrimitiveComponent* PrimComp, FRBPhysScene* InRBScene)
{
	check(PrimComp);
	check(setup);

	OwnerComponent = PrimComp;
	AActor* Owner = OwnerComponent->GetOwner();

#if WITH_NOVODEX
	// If there is already a body instanced, or there is no scene to create it into, do nothing.
	if (BodyData || !InRBScene)	// hardware scene support
	{
		return;
	}

	// Make the debug name for this geometry...
	FString DebugName(TEXT(""));
#if !FINAL_RELEASE && !NO_LOGGING
  #if (!CONSOLE || _DEBUG || LOOKING_FOR_PERF_ISSUES)
	if(Owner)
	{
		DebugName += FString::Printf( TEXT("Actor: %s "), *Owner->GetPathName() );
	}

	DebugName += FString::Printf( TEXT("Component: %s "), *PrimComp->GetName() );

	UStaticMeshComponent* SMComp = Cast<UStaticMeshComponent>(PrimComp);
	if(SMComp)
	{
		DebugName += FString::Printf( TEXT("StaticMesh: %s"), *SMComp->StaticMesh->GetPathName() );
	}

	if(setup->BoneName != NAME_None)
	{
		DebugName += FString::Printf( TEXT("Bone: %s "), *setup->BoneName.ToString() );
	}
  #else
	DebugName = FString::Printf( TEXT("%s %s"), *PrimComp->GetPathName(), *PrimComp->GetDetailedInfo());
  #endif
#endif

	if(Scale3D.IsNearlyZero())
	{
		debugf(TEXT("URB_BodyInstance::InitBody : Scale3D is (nearly) zero: %s"), *DebugName);
		return;
	}

	if(transform.ContainsNaN())
	{
		FMatrix TM = transform;
		debugf(TEXT("InitBody: Bad transform - %s %s"), *DebugName, *setup->BoneName.ToString());
		debugf(TEXT("%f %f %f %f"), TM.M[0][0], TM.M[0][1], TM.M[0][2], TM.M[0][3]);
		debugf(TEXT("%f %f %f %f"), TM.M[1][0], TM.M[1][1], TM.M[1][2], TM.M[1][3]);
		debugf(TEXT("%f %f %f %f"), TM.M[2][0], TM.M[2][1], TM.M[2][2], TM.M[2][3]);
		debugf(TEXT("%f %f %f %f"), TM.M[3][0], TM.M[3][1], TM.M[3][2], TM.M[3][3]);
		return;
	}

	// Create ActorDesc description, and copy list of primitives from stored geometry.
	NxActorDesc ActorDesc;

	// Find the PhysicalMaterial to use for this instance.
	UPhysicalMaterial*	PhysMat  = GetPhysicalMaterial();
	NxMaterialIndex		MatIndex = InRBScene->FindPhysMaterialIndex( PhysMat );

	// Make the collision mask for this instance.
	NxGroupsMask		GroupsMask = CreateGroupsMask(OwnerComponent->RBChannel, &(OwnerComponent->RBCollideWithChannels));

	// See if we want to skip geometry creation step
	if(!PrimComp->bSkipRBGeomCreation)
	{
		// Try to find a shape with the matching scale
		NxArray<NxShapeDesc*>* Aggregate = NULL;
		for (INT i=0; i < setup->CollisionGeomScale3D.Num(); i++)
		{
			// Found a shape with the right scale
			if ((setup->CollisionGeomScale3D(i) - Scale3D).IsNearlyZero())
			{
				Aggregate = &((NxActorDesc*) setup->CollisionGeom(i))->shapes;
				break;
			}
		}

		// If no shape was found, create a new one and place it in the list
		if (!Aggregate)
		{
			// Instance a Novodex collision shape based on the stored unreal collision data.
			NxActorDesc* GeomActorDesc = NULL;

			// If this is a static mesh, check the cache in the ULevel to see if we have cooked data for it already
			FKCachedConvexData* CachedData = PrimComp->GetBoneCachedPhysConvexData(Scale3D, setup->BoneName);

#ifdef ENABLE_CCD
			UBOOL bEnableCCD = setup->bEnableContinuousCollisionDetection;
#else
			UBOOL bEnableCCD = FALSE;
#endif

			// Actually instance the novodex geometry from the Unreal description.
			GeomActorDesc = setup->AggGeom.InstanceNovodexGeom(Scale3D, CachedData, bEnableCCD, *DebugName);
			if (GeomActorDesc)
			{
				Aggregate = &GeomActorDesc->shapes;
			}

			if (Aggregate)
			{
				setup->CollisionGeomScale3D.AddItem(Scale3D);
				setup->CollisionGeom.AddItem(GeomActorDesc);
			}
			else
			{
				debugf(TEXT("URB_BodyInstance::InitBody : Could not create new Shape: %s"), *DebugName);
				return;
			}
		}


		for(UINT i=0; i<Aggregate->size(); i++)
		{
#ifndef NX_DISABLE_FLUIDS
			// Novodex fluids
			if( PrimComp->bFluidDrain )
			{
				((*Aggregate)[i])->shapeFlags |= NX_SF_FLUID_DRAIN;
			}
			else
			{
				((*Aggregate)[i])->shapeFlags &= ~(NxU32)NX_SF_FLUID_DRAIN;
			}
			if( PrimComp->bFluidTwoWay )
			{
				((*Aggregate)[i])->shapeFlags |= NX_SF_FLUID_TWOWAY;
			}
			else
			{
				((*Aggregate)[i])->shapeFlags &= ~(NxU32)NX_SF_FLUID_TWOWAY;
			}
#endif

			ActorDesc.shapes.push_back( (*Aggregate)[i] );

			// Set the material to the one specified in the PhysicalMaterial before creating this NxActor instance.
			ActorDesc.shapes[i]->materialIndex = MatIndex;

			// Set the collision filtering mask for this shape.
			ActorDesc.shapes[i]->groupsMask = GroupsMask;
		}
	}

	NxMat34 nTM = U2NTransform(transform);

	ActorDesc.density = PhysMat->Density;
	ActorDesc.globalPose = nTM;

	if(setup->bNoCollision || !PrimComp->BlockRigidBody)
	{
		ActorDesc.flags = NX_AF_DISABLE_COLLISION;
	}

	// force more expensive cone friction that doesn't converge to axis aligned velocity
	if (PhysMat->bForceConeFriction)
	{
		ActorDesc.flags |= NX_AF_FORCE_CONE_FRICTION;
	}

	if (!bEnableCollisionResponse || bPushBody)
	{
		ActorDesc.flags |= NX_AF_DISABLE_RESPONSE;
	}

	// caching this expression
	UBOOL bStatic = (Owner != NULL && (Owner->IsStatic() || !Owner->bMovable));

	// Now fill in dynamics parameters.
	// If an owner and is static, don't create any dynamics properties for this Actor.
	NxBodyDesc BodyDesc;
	if(!bStatic)
	{
		// Set the damping properties from the PhysicalMaterial
		BodyDesc.angularDamping = PhysMat->AngularDamping;
		BodyDesc.linearDamping = PhysMat->LinearDamping;

		// Set the parameters for determining when to put the object to sleep.
		switch( setup->SleepFamily )
		{
		case SF_Sensitive: 
			{
				// Values with a lower sleep threshold; good for slower pendulum-like physics.
				BodyDesc.sleepEnergyThreshold = 0.01f;
				BodyDesc.sleepDamping = 0.2f;
			}
			break;
		case SF_Normal:
		default:
			{
				// Engine defaults.
				BodyDesc.sleepEnergyThreshold = 0.5f;
				BodyDesc.sleepDamping = 0.2f;
			}
			break;
		}
		BodyDesc.solverIterationCount = 8;

		// Inherit linear velocity from Unreal Actor.
		FVector uLinVel(0.f);
		if(Owner)
		{	
			uLinVel = Velocity = PreviousVelocity = Owner->Velocity;
		}

		// Set kinematic flag if body is not currently dynamics.
		if(setup->bFixed || bFixed)
		{
			BodyDesc.flags |= NX_BF_KINEMATIC;
		}

#ifdef ENABLE_CCD
		// If we don't want CCD, set the threshold to something really high.
		if(!setup->bEnableContinuousCollisionDetection)
		{
			BodyDesc.CCDMotionThreshold = BIG_NUMBER;
		}
		// If we do want it on, set a threshold velocity for it to work.
		else
		{
			// @todo at the moment its always on - maybe look at bounding box to figure out a good threshold.
			BodyDesc.CCDMotionThreshold = 0.f * U2PScale;
		}
#endif

		// Set linear velocity of body.
		BodyDesc.linearVelocity = U2NPosition(uLinVel);

		// Set the dominance group
		ActorDesc.dominanceGroup = Clamp<BYTE>(PrimComp->RBDominanceGroup, 0, 31);

		// Assign Body Description to Actor Description
		ActorDesc.body = &BodyDesc;
	}

	// Give the actor a chance to modify the NxActorDesc before we create the NxActor with it
	if(Owner)
	{
		Owner->ModifyNxActorDesc(ActorDesc, PrimComp, GroupsMask, MatIndex);
	}

	if (!ActorDesc.isValid())
	{
		// check to make certain you have a correct Physic Asset and that your bone
		// names have not been changed
		debugf(TEXT("URB_BodyInstance::InitBody - Error, rigid body description invalid, %s"), *DebugName);
		return;
	}

	BodyData                   = NULL;
	NxScene *NovodexScene      = InRBScene->GetNovodexPrimaryScene();
	NxCompartment *Compartment = InRBScene->GetNovodexRigidBodyCompartment();
	UBOOL bUsingCompartment    = PrimComp->bUseCompartment && !bStatic && Compartment;
	if(bUsingCompartment)
	{
		ActorDesc.compartment = Compartment;
	}
	
#if PERF_SHOW_PHYS_INIT_COSTS
	DOUBLE StartCreate = appSeconds();
#endif
	NxActor* Actor = NovodexScene->createActor(ActorDesc);
#if PERF_SHOW_PHYS_INIT_COSTS
	TotalCreateActorTime += (appSeconds() - StartCreate);
#endif
	// removed scene loop....

	// If we made the NxActor successfully.
	if(Actor)
	{
		// Store pointer to Novodex data in RB_BodyInstance
		BodyData = (FPointer)Actor;

		// Store pointer to owning bodyinstance.
		Actor->userData = this;

		// Store scene index
		SceneIndex = InRBScene->NovodexSceneIndex;

		if(!bStatic)
		{
			check( Actor->isDynamic() );

			UpdateMassProperties(setup);

			if( Owner && Owner->Velocity.Size() > KINDA_SMALL_NUMBER )
			{
				// Wake up bodies that are part of a moving actor.
				Actor->wakeUp();
			}
			else
			{
				// Bodies should start out sleeping.
				Actor->putToSleep();
			}
		}

		// Put the NxActor into the 'notify on collision' group if desired.
		if(PrimComp->bNotifyRigidBodyCollision)
		{
			// Set the force-based contact report threshold, if enabled (value > 0)
			if( !bStatic && ContactReportForceThreshold >= 0 )
			{
				Actor->setContactReportThreshold( ContactReportForceThreshold );
				Actor->setGroup(UNX_GROUP_THRESHOLD_NOTIFY);
			}
			else
			{
				Actor->setGroup(UNX_GROUP_NOTIFYCOLLIDE);
			}
		}

		// After starting up the physics, let the Actor do anything else it might want.
		if(Owner)
		{
			Owner->PostInitRigidBody(Actor, ActorDesc,PrimComp);
		}
	}
	else
	{
		debugf(TEXT("URB_BodyInstance::InitBody : Could not create NxActor: %s"), *DebugName);
	}
#endif // WITH_NOVODEX
}

/**
 *	Clean up the physics engine info for this instance.
 *	If Scene is NULL, it will always shut down physics. If an RBPhysScene is passed in, it will only shut down physics it the physics is in that scene. 
 *	Returns TRUE if physics was shut down, and FALSE otherwise.
 */
UBOOL URB_BodyInstance::TermBody(FRBPhysScene* Scene)
{
#if WITH_NOVODEX
	// If this body is in the scene we want, (or NULL was specified) kill it.
	if(Scene == NULL || SceneIndex == Scene->NovodexSceneIndex)
	{
		AActor* Owner = NULL;
		if(OwnerComponent)
		{
			Owner = OwnerComponent->GetOwner();
		}

#if !FINAL_RELEASE
		// Check to see if this physics call is illegal during this tick group
		if (GWorld && GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
		{
			debugf(NAME_Error,TEXT("Can't call TermBody() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		}
#endif

		// Check the software scene is still around. If it isn't - both have been torn down already so 
		// its unsafe to touch any NxActors etc
		NxScene *NovodexScene = GetNovodexPrimarySceneFromIndex( SceneIndex );
		if( NovodexScene )
		{
			NxActor * BoneSpringActor = (NxActor*)BodyData;
			if( BoneSpringActor )
			{
				NxScene * BoneSpringScene = &BoneSpringActor->getScene();
				check(BoneSpringScene);
				NxJoint* Spring = (NxJoint*)BoneSpring;
				if (Spring)
				{
					if (!GIsEditor)
					{
						DeferredReleaseNxJoint(Spring, TRUE);
					}
					else
					{
						BoneSpringScene->releaseJoint(*Spring);
					}
				}
			}

			// Clean up kinematic actor for bone spring if there is one.
			NxActor* KinActor = (NxActor*)BoneSpringKinActor;
			if(KinActor)
			{
				DestroyDummyKinActor(KinActor);
				BoneSpringKinActor = NULL;
			}
		}

		BoneSpring = NULL;

		if( NovodexScene )
		{
			NxActor* nActor = (NxActor*)BodyData;
			if( nActor )
			{
				if(Owner && !Owner->IsPendingKill())
				{
					Owner->PreTermRigidBody(nActor);
				}

#if PERF_SHOW_SLOW_RELEASE_TIMES || LOOKING_FOR_PERF_ISSUES
				DOUBLE ReleaseStart = appSeconds();
				INT NumShapes  = nActor->getNbShapes();
#endif

				// If physics is running, defer destruction of NxActor
				nActor->userData = 0;
				if (!GIsEditor || (GWorld && GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork))
				{
					DeferredReleaseNxActor(nActor, TRUE);					
				}
				else
				{
					NovodexScene->releaseActor(*nActor);
				}

#if PERF_SHOW_SLOW_RELEASE_TIMES || LOOKING_FOR_PERF_ISSUES
					DOUBLE ReleaseTimeMs = (appSeconds() - ReleaseStart) * 1000.f;
					if(ReleaseTimeMs > PERF_SHOW_SLOW_RELEASE_TIMES_AMOUNT)
					{
						debugf(NAME_PerfWarning, TEXT("Novodex releaseActor (%s - %d shapes) took: %f ms"), *Owner->GetName(), NumShapes, ReleaseTimeMs );
					}
#endif
			}
		}

		BodyData = NULL;

		return TRUE;
	}
#endif // WITH_NOVODEX

	return FALSE;
}

void URB_BodyInstance::FinishDestroy()
{
	// Clean up physics engine stuff when the BodyInstance gets GC'd
	TermBody(NULL);

	Super::FinishDestroy();
}

void URB_BodyInstance::SetFixed(UBOOL bNewFixed)
{
#if WITH_NOVODEX

#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		FString SpecificName;
		USkeletalMeshComponent* Comp = Cast<USkeletalMeshComponent>(OwnerComponent);
		if( Comp != NULL )
		{
			SpecificName = Comp->SkeletalMesh->GetFullName();
		}
		else
		{
			SpecificName = OwnerComponent ? *OwnerComponent->GetName() : TEXT("No Owner");
		}

		debugf(NAME_Error,TEXT("Can't call URB_BodyInstance::SetFixed() in '%s' during async work!"), *SpecificName );
	}
#endif

	if( bForceUnfixed == TRUE )
	{
		if( bNewFixed == TRUE ) // we do not want to have bNewFixed == TRUE set on this bForcedToBeKinematic body
		{
			// do not allow this SetFixed
			//warnf( TEXT ("SetAllBodiesFixed %d But we are bForcedToBeKinematic == TRUE"), bNewFixed );
			return;
		}
	}

	NxActor* Actor = (NxActor*)BodyData;
	if(Actor && Actor->isDynamic())
	{
		// If we want it fixed, and it is currently not kinematic
		if(bNewFixed && !Actor->readBodyFlag(NX_BF_KINEMATIC))
		{
			Actor->raiseBodyFlag(NX_BF_KINEMATIC);
		}
		// If want to stop it being fixed, and it is currently kinematic
		else if(!bNewFixed && Actor->readBodyFlag(NX_BF_KINEMATIC))
		{
			Actor->clearBodyFlag(NX_BF_KINEMATIC);

#if 0
			// Should not need to do this, but moveGlobalPose does not currently update velocity correctly so we are not using it.
			if(OwnerComponent)
			{
				AActor* Owner = OwnerComponent->GetOwner();
				if(Owner)
				{
					AActor* Owner = OwnerComponent->GetOwner();
					if(Owner)
					{
						setLinearVelocity(Actor, U2NPosition(Owner->Velocity) );
					}
				}
			}
#endif
		}
	}
#endif // WITH_NOVODEX
}

UBOOL URB_BodyInstance::IsFixed()
{
#if WITH_NOVODEX
	
	NxActor* Actor = (NxActor*)BodyData;
	if( Actor && Actor->isDynamic() )
	{
		if( !Actor->readBodyFlag(NX_BF_KINEMATIC) )
		{
			return FALSE;
		}
	}

	return TRUE;
#else
	return FALSE;
#endif
}

/** Used to disable rigid body collisions for this body. Overrides the bNoCollision flag in the BodySetup for this body. */
void URB_BodyInstance::SetBlockRigidBody(UBOOL bNewBlockRigidBody)
{
#if WITH_NOVODEX

#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call URB_BodyInstance::SetBlockRigidBody() in '%s' during async work!"), OwnerComponent ? *OwnerComponent->GetName() : TEXT("No Owner"));
	}
#endif

	NxActor* Actor = (NxActor*)BodyData;
	if(Actor)
	{
		if(bNewBlockRigidBody)
		{
			Actor->clearActorFlag(NX_AF_DISABLE_COLLISION);
		}
		else
		{
			Actor->raiseActorFlag(NX_AF_DISABLE_COLLISION);
		}
	}
#endif // WITH_NOVODEX
}

UBOOL URB_BodyInstance::IsValidBodyInstance()
{
	UBOOL Retval = false;
#if WITH_NOVODEX
	NxActor* Actor = GetNxActor();

	if(Actor)
	{
		Retval = true;
	}
#endif // WITH_NOVODEX

	return Retval;
}

FMatrix URB_BodyInstance::GetUnrealWorldTM()
{
#if WITH_NOVODEX
	NxActor* Actor = GetNxActor();
	check(Actor);
	check(Actor->getNbShapes() > 0);

	NxMat34 nTM = Actor->getGlobalPose();
	FMatrix uTM = N2UTransform(nTM);

	return uTM;
#endif // WITH_NOVODEX

	return FMatrix::Identity;
}


FVector URB_BodyInstance::GetUnrealWorldVelocity()
{
#if WITH_NOVODEX
	NxActor* Actor = GetNxActor();
	check(Actor);

	FVector uVelocity(0.f);
	if(Actor->isDynamic())
	{
		NxVec3 nVelocity = Actor->getLinearVelocity();
		uVelocity = N2UPosition(nVelocity);
	}

	return uVelocity;
#endif // WITH_NOVODEX

	return FVector(0.f);
}

FVector URB_BodyInstance::GetUnrealWorldAngularVelocity()
{
#if WITH_NOVODEX
	NxActor* Actor = GetNxActor();
	check(Actor);

	FVector uAngVelocity(0.f);
	if(Actor->isDynamic())
	{
		NxVec3 nAngVelocity = Actor->getAngularVelocity();
		uAngVelocity = N2UVectorCopy(nAngVelocity);
	}

	return uAngVelocity;
#endif // WITH_NOVODEX

	return FVector(0.f);
}

FVector URB_BodyInstance::GetUnrealWorldVelocityAtPoint(FVector Point)
{
#if WITH_NOVODEX
	NxActor* Actor = GetNxActor();
	check(Actor);

	FVector uVelocity(0.f);
	if(Actor->isDynamic())
	{
		NxVec3 nPoint = U2NPosition(Point);
		NxVec3 nVelocity = Actor->getPointVelocity(nPoint);
		uVelocity = N2UPosition(nVelocity);
	}

	return uVelocity;
#endif // WITH_NOVODEX

	return FVector(0.f);
}


FVector URB_BodyInstance::GetCOMPosition()
{
#if WITH_NOVODEX
	NxActor* Actor = GetNxActor();
	if(Actor)
	{
		NxVec3 nCOMPos = Actor->getCMassGlobalPosition();
		return N2UPosition(nCOMPos);
	}
	else
	{
		return FVector(0,0,0);
	}
#else
	return FVector(0,0,0);
#endif
}

FLOAT URB_BodyInstance::GetBodyMass()
{
	FLOAT Retval = 0.f;

#if WITH_NOVODEX
	NxActor* Actor = GetNxActor();
	if(Actor)
	{
		Retval = Actor->getMass();
	}
	else
	{
		Retval = 0.f;
	}
#endif

	return Retval;
}

void URB_BodyInstance::DrawCOMPosition( FPrimitiveDrawInterface* PDI, FLOAT COMRenderSize, const FColor& COMRenderColor )
{
#if WITH_NOVODEX
	NxActor* Actor = GetNxActor();
	if(Actor)
	{
		NxVec3 nCOMPos = Actor->getCMassGlobalPosition();
		FVector COMPos = N2UPosition(nCOMPos);

		DrawWireStar(PDI, COMPos, COMRenderSize, COMRenderColor, SDPG_World);
	}
#endif
}

/** Utility for copying properties from one BodyInstance to another. */
void URB_BodyInstance::CopyBodyInstancePropertiesFrom(URB_BodyInstance* FromInst)
{
	bEnableBoneSpringLinear = FromInst->bEnableBoneSpringLinear;
	bEnableBoneSpringAngular = FromInst->bEnableBoneSpringAngular;
	bDisableOnOverextension = FromInst->bDisableOnOverextension;
	BoneLinearSpring = FromInst->BoneLinearSpring;
	BoneLinearDamping = FromInst->BoneLinearDamping;
	BoneAngularSpring = FromInst->BoneAngularSpring;
	BoneAngularDamping = FromInst->BoneAngularDamping;
	OverextensionThreshold = FromInst->OverextensionThreshold;

	bEnableCollisionResponse = FromInst->bEnableCollisionResponse;
	bPushBody = FromInst->bPushBody;

	bTeleportOnOverextension = FromInst->bTeleportOnOverextension;
	bUseKinActorForBoneSpring = FromInst->bUseKinActorForBoneSpring;
	bMakeSpringToBaseCollisionComponent = FromInst->bMakeSpringToBaseCollisionComponent;
	bOnlyCollideWithPawns = FromInst->bOnlyCollideWithPawns;
	PhysMaterialOverride = FromInst->PhysMaterialOverride;

	ContactReportForceThreshold = FromInst->ContactReportForceThreshold;

	InstanceMassScale= FromInst->InstanceMassScale;
	InstanceDampingScale= FromInst->InstanceDampingScale;
}

#if WITH_NOVODEX
class NxActor* URB_BodyInstance::GetNxActor()
{
	return (NxActor*)BodyData;
}
#endif // WITH_NOVODEX


/** Used to turn the angular/linear bone spring on and off. */
void URB_BodyInstance::EnableBoneSpring(UBOOL bInEnableLinear, UBOOL bInEnableAngular, const FMatrix& InBoneTarget)
{
#if WITH_NOVODEX
	// If we don't want any spring, but we have one - get rid of it.
	if(!bInEnableLinear && !bInEnableAngular && BoneSpring)
	{
		// If there is a kinematic actor for this spring, release it.
		NxActor* KinActor = (NxActor*)BoneSpringKinActor;
		if(KinActor)
		{
			DestroyDummyKinActor(KinActor);
		}
		BoneSpringKinActor = NULL;


		NxJoint* Spring = (NxJoint*)BoneSpring;
		// if bone spring was made, this is the scene it was put in.
		NxActor * BoneSpringActor = GetNxActor();
		if( BoneSpringActor )
		{
			BoneSpringActor->getScene().releaseJoint(*Spring);
		}
		BoneSpring = NULL;
	}
	// If we want a spring, but we don't have one, create here.
	else if((bInEnableLinear || bInEnableAngular) && !BoneSpring)
	{
		NxActor* BoneActor = GetNxActor();
		if(BoneActor)
		{
			// hardware scene support - get scene from actor
			NxScene * NovodexScene = &BoneActor->getScene();

			NxD6JointDesc Desc;
			Desc.actor[0] = BoneActor;

			if(bUseKinActorForBoneSpring)
			{
				NxActor* KinActor = CreateDummyKinActor(NovodexScene, InBoneTarget);
				BoneSpringKinActor = KinActor;
				Desc.actor[1] = KinActor;
			}
			// If we want to make spring to base body, find it here.
			else if(bMakeSpringToBaseCollisionComponent)
			{
				if( OwnerComponent && 
					OwnerComponent->GetOwner() && 
					OwnerComponent->GetOwner()->Base && 
					OwnerComponent->GetOwner()->Base->CollisionComponent )
				{
					NxActor* nSpringActor = OwnerComponent->GetOwner()->Base->CollisionComponent->GetNxActor();
					if(nSpringActor)
					{
						Desc.actor[1] = nSpringActor;
					}
				}
			}

			Desc.flags = NX_D6JOINT_SLERP_DRIVE;

			Desc.localAxis[0].set(1,0,0);
			Desc.localNormal[0].set(0,1,0);

			Desc.localAxis[1].set(1,0,0);
			Desc.localNormal[1].set(0,1,0);

			if( NovodexScene )
			{
				NxJoint* Spring = NovodexScene->createJoint(Desc);
				BoneSpring = Spring;
			}

			// Push drive params into the novodex joint now.
			SetBoneSpringParams(BoneLinearSpring, BoneLinearDamping, BoneAngularSpring, BoneAngularDamping);

			// Set the initial bone target to be correct.
			SetBoneSpringTarget(InBoneTarget, TRUE);
		}
	}

	// Turn drives on as required.
	if(BoneSpring)
	{
		NxJoint* Spring = (NxJoint*)BoneSpring;
		NxD6Joint* D6Joint = Spring->isD6Joint();
		check(D6Joint);

		NxD6JointDesc Desc;
		D6Joint->saveToDesc(Desc);

		if(bInEnableAngular)
		{
			Desc.slerpDrive.driveType = NX_D6JOINT_DRIVE_POSITION;
		}

		if(bInEnableLinear)
		{
			Desc.xDrive.driveType = NX_D6JOINT_DRIVE_POSITION;
			Desc.yDrive.driveType = NX_D6JOINT_DRIVE_POSITION;
			Desc.zDrive.driveType = NX_D6JOINT_DRIVE_POSITION;
		}

		D6Joint->loadFromDesc(Desc);
	}
#endif

	bEnableBoneSpringLinear = bInEnableLinear;
	bEnableBoneSpringAngular = bInEnableAngular;
}

/** Used to set the spring stiffness and  damping parameters for the bone spring. */
void URB_BodyInstance::SetBoneSpringParams(FLOAT InLinearSpring, FLOAT InLinearDamping, FLOAT InAngularSpring, FLOAT InAngularDamping)
{
#if WITH_NOVODEX
	if(BoneSpring)
	{
		NxJoint* Spring = (NxJoint*)BoneSpring;
		NxD6Joint* D6Joint = Spring->isD6Joint();
		check(D6Joint);

		NxD6JointDesc Desc;
		D6Joint->saveToDesc(Desc);

		Desc.xDrive.spring = InLinearSpring;
		Desc.xDrive.damping = InLinearDamping;

		Desc.yDrive.spring = InLinearSpring;
		Desc.yDrive.damping = InLinearDamping;

		Desc.zDrive.spring = InLinearSpring;
		Desc.zDrive.damping = InLinearDamping;

		Desc.slerpDrive.spring = InAngularSpring;
		Desc.slerpDrive.damping = InAngularDamping;

		D6Joint->loadFromDesc(Desc);
	}
#endif

	BoneLinearSpring = InLinearSpring;
	BoneLinearDamping = InLinearDamping;
	BoneAngularSpring = InAngularSpring;
	BoneAngularDamping = InAngularDamping;
}

/** Used to set desired target location of this spring. Usually called by UpdateRBBoneKinematics. */
void URB_BodyInstance::SetBoneSpringTarget(const FMatrix& InBoneTarget, UBOOL bTeleport)
{
#if WITH_NOVODEX
	if(BoneSpring)
	{
		FMatrix UseTarget = InBoneTarget;
		UseTarget.RemoveScaling();
		check(!UseTarget.ContainsNaN());

		NxJoint* Spring = (NxJoint*)BoneSpring;
		NxD6Joint* D6Joint = Spring->isD6Joint();
		NxActor* KinActor = (NxActor*)BoneSpringKinActor;
		check(D6Joint);

		// If using a kinematic actor, just update its transform.
		if(KinActor)
		{
			NxMat34 nPose = U2NTransform(UseTarget);
			NxMat34 nCurrentPose = KinActor->getGlobalPose();

			// If bone is scaled to zero - break the bone spring
			if(	nPose.M.determinant() < (FLOAT)KINDA_SMALL_NUMBER )
			{
				FMatrix Dummy = FMatrix::Identity;
				EnableBoneSpring(FALSE, FALSE, Dummy);
				return;
			}
			// Otherwise, if matrices are different, .
			else if( !MatricesAreEqual(nPose, nCurrentPose, (FLOAT)KINDA_SMALL_NUMBER) )
			{
				if(bTeleport)
				{
					KinActor->setGlobalPose(nPose);
				}
				else
				{
					KinActor->moveGlobalPose(nPose);
				}
			}
		}
		// If not, update joint attachment to world.
		else
		{
			NxD6JointDesc Desc;
			D6Joint->saveToDesc(Desc);

			FVector LocalAxis = UseTarget.GetAxis(0);
			FVector LocalNormal = UseTarget.GetAxis(1);
			if (LocalAxis.IsNearlyZero() || LocalNormal.IsNearlyZero())
			{
				LocalAxis = FVector(1.f,0.f,0.f);
				LocalNormal = FVector(0.f,1.f,0.f);
			}

			Desc.localAnchor[1] = U2NPosition(UseTarget.GetOrigin());
			Desc.localAxis[1] = U2NVectorCopy(LocalAxis);
			Desc.localNormal[1] = U2NVectorCopy(LocalNormal);

			D6Joint->loadFromDesc(Desc);
		}

		// @todo Hook to a toggleable parameter.
		// Draw bone springs, if desired.
		static bool bShowBoneSprings = FALSE;
		if (bShowBoneSprings)
		{
			NxActor *nActor0, *nActor1;
			Spring->getActors(&nActor0, &nActor1);
			if (nActor0)
			{
				FLinearColor LineColor(1.f, 1.f, 1.f);
				if( nActor0->readBodyFlag(NX_BF_KINEMATIC) )
				{
					// color red
					LineColor.G = 0.2f; 
					LineColor.B = 0.2f;
				}

				GWorld->LineBatcher->DrawLine(UseTarget.GetOrigin(), N2UPosition(nActor0->getGlobalPosition()), LineColor, SDPG_Foreground);
			}
		}

		// If desired - see if spring length has exceeded OverextensionThreshold
		if(bDisableOnOverextension || bTeleportOnOverextension)
		{
			NxActor *nActor0, *nActor1;
			Spring->getActors(&nActor0, &nActor1);
			check(nActor0);

			// Calculate distance from body to target (ie length of spring)
			FVector BodyPosition = N2UPosition( nActor0->getGlobalPosition() );
			FVector SpringError = UseTarget.GetOrigin() - BodyPosition;
			
			if( SpringError.Size() > OverextensionThreshold )
			{
				// If desired - disable spring.
				if(bDisableOnOverextension)
				{
					FMatrix Dummy = FMatrix::Identity;
					EnableBoneSpring(FALSE, FALSE, Dummy);

					// Notify Actor that this spring is now broken.
					if( bNotifyOwnerOnOverextension )
					{
						OwnerComponent->GetOwner()->eventOnRigidBodySpringOverextension(this);
					}
				}
				// Teleport entire asset.
				else if(bTeleportOnOverextension)
				{
					UPhysicsAssetInstance* Inst = GetPhysicsAssetInstance();
					if(Inst)
					{
						// Apply the spring delta to all the bodies (including this one).
						for (INT i=0; i<Inst->Bodies.Num(); i++)
						{
							check(Inst->Bodies(i));
							NxActor* UpdateActor = Inst->Bodies(i)->GetNxActor();
							if (UpdateActor != NULL)
							{
								FVector UpdatePos = N2UPosition(UpdateActor->getGlobalPosition());
								UpdateActor->setGlobalPosition( U2NPosition(UpdatePos + SpringError) );
							}
						}
					}
				}
			}
		}
	}
#endif
}

/** 
*	Changes the current PhysMaterialOverride for this body. 
*	Note that if physics is already running on this component, this will _not_ alter its mass/inertia etc, it will only change its 
*	surface properties like friction and the damping.
*/
void URB_BodyInstance::SetPhysMaterialOverride( UPhysicalMaterial* NewPhysMaterial )
{
#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call SetPhysMaterialOverride() on (%s)->(%s) during async work!"), *OwnerComponent->GetOwner()->GetName(), *GetName());
		return;
	}
#endif

	// Save ref to PhysicalMaterial
	PhysMaterialOverride = NewPhysMaterial;

#if WITH_NOVODEX
	// Go through the chain of physical materials and update the NxActor
	UpdatePhysMaterialOverride();
#endif // WITH_NOVODEX
}

#if WITH_NOVODEX
/**
 *	UpdatePhysMaterialOverride
 *		Update Nx with the physical material that should be applied to this body
 *		Author: superville
 */
void URB_BodyInstance::UpdatePhysMaterialOverride()
{
	NxActor* nActor = GetNxActor();

	// If there is no physics object, nothing to do.
	if( nActor )
	{
		UPhysicalMaterial* PhysMat = GetPhysicalMaterial();

		// Turn PhysicalMaterial into PhysX material index using RBScene.
		NxScene* nScene = &nActor->getScene();
		check(nScene);
		FRBPhysScene* RBScene = (FRBPhysScene*)(nScene->userData);
		check(RBScene);
		NxMaterialIndex MatIndex = RBScene->FindPhysMaterialIndex( PhysMat );

		// Finally assign to all shapes in the physics object.
		SetNxActorMaterial( nActor, MatIndex, PhysMat );	
	}
}
#endif // WITH_NOVODEX

/**
*	GetPhysicalMaterial
*		Figure out which physical material to apply to the NxActor for this body.
*		Need to walk the chain of potential materials/overrides
*		Author: superville
*/
UPhysicalMaterial* URB_BodyInstance::GetPhysicalMaterial()
{
	check( GEngine->DefaultPhysMaterial != NULL );

	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(OwnerComponent);
	UStaticMeshComponent*	StatComp = Cast<UStaticMeshComponent>(OwnerComponent);
	URB_BodySetup*			Setup	 = NULL;
	UPhysicalMaterial*		PMaterialFromMaterialInstance = NULL;
	if( SkelComp != NULL && SkelComp->PhysicsAsset != NULL )
	{
		Setup = SkelComp->PhysicsAsset->BodySetup(BodyIndex);
	}
	if (StatComp != NULL)
	{
		UMaterialInterface* Material = StatComp->GetMaterial(0);
		if(Material != NULL)
		{
			PMaterialFromMaterialInstance = Material->GetPhysicalMaterial();
		}
		if(StatComp->StaticMesh != NULL)
		{
			Setup = StatComp->StaticMesh->BodySetup;
		}
	}

	// Find the PhysicalMaterial we need to apply to the physics bodies.
	// (LOW priority) Engine Mat, MaterialInstance Mat, Setup Mat, Component Override, Body Override (HIGH priority)
	UPhysicalMaterial* PhysMat = GEngine->DefaultPhysMaterial;					 // Fallback is engine default.
	
	if(PMaterialFromMaterialInstance != NULL)									// Normal Mat
	{
		PhysMat = PMaterialFromMaterialInstance;
	}
	if( Setup != NULL && Setup->PhysMaterial != NULL )							 // Next use setup material
	{
		PhysMat = Setup->PhysMaterial;
	}
	if( OwnerComponent != NULL && OwnerComponent->PhysMaterialOverride != NULL ) // Next use component override
	{
		PhysMat = OwnerComponent->PhysMaterialOverride;
	}
	if( PhysMaterialOverride != NULL )											 // Always use body override if it exists
	{
		PhysMat = PhysMaterialOverride;
	}

	return PhysMat;
}

/** Returns the PhysicsAssetInstance that owns this RB_BodyInstance (if there is one) */
UPhysicsAssetInstance* URB_BodyInstance::GetPhysicsAssetInstance()
{
	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(OwnerComponent);
	if(SkelComp)
	{
		return SkelComp->PhysicsAssetInstance;
	}

	return NULL;
}

/** Used to turn collision response on/off. */
void URB_BodyInstance::EnableCollisionResponse(UBOOL bInEnableResponse)
{
#if WITH_NOVODEX
	NxActor* const BoneActor = GetNxActor();

	if (BoneActor)
	{
		// set appropriate response flag on the NxActor
		if (bInEnableResponse && BoneActor->readActorFlag(NX_AF_DISABLE_RESPONSE))
		{
			BoneActor->clearActorFlag(NX_AF_DISABLE_RESPONSE);
		}
		else if (!bInEnableResponse && !BoneActor->readActorFlag(NX_AF_DISABLE_RESPONSE))
		{
			BoneActor->raiseActorFlag(NX_AF_DISABLE_RESPONSE);
		}
	}
#endif

	bEnableCollisionResponse = bInEnableResponse;
}

/**
 *	Set a new contact report force threhold.  Threshold < 0 disables this feature.
 */
void URB_BodyInstance::SetContactReportForceThreshold( FLOAT Threshold )
{
#if WITH_NOVODEX
	NxActor* const BoneActor = GetNxActor();

	if (BoneActor)
	{
		if( Threshold < 0 )
		{
			if( BoneActor->getGroup() == UNX_GROUP_THRESHOLD_NOTIFY )
			{
				BoneActor->setGroup( UNX_GROUP_NOTIFYCOLLIDE );
			}
			BoneActor->setContactReportThreshold( FLT_MAX );
		}
		else
		{
			if( BoneActor->getGroup() == UNX_GROUP_NOTIFYCOLLIDE )
			{
				BoneActor->setGroup( UNX_GROUP_THRESHOLD_NOTIFY );
			}
			BoneActor->setContactReportThreshold( Threshold );
		}
	}
#endif

	ContactReportForceThreshold = Threshold;
}

/** Update instance's mass properties (mass, inertia and center-of-mass offset) based on MassScale, InstanceMassScale and COMNudge. */
void URB_BodyInstance::UpdateMassProperties(URB_BodySetup* Setup)
{
#if WITH_NOVODEX
	check(Setup);
	NxActor* nActor = (NxActor*)BodyData;
	if(nActor)
	{
		// First, reset mass to default
		UPhysicalMaterial* PhysMat  = GetPhysicalMaterial();
		nActor->updateMassFromShapes(PhysMat->Density, 0.f);

		// Then scale mass to avoid big differences between big and small objects.
		FLOAT OldMass = nActor->getMass();
		FLOAT NewMass = appPow(OldMass, 0.75f);
		//debugf( TEXT("OldMass: %f NewMass: %f"), OldMass, NewMass );

		// Apply user-defined mass scaling.
		NewMass *= Clamp<FLOAT>(Setup->MassScale * InstanceMassScale, 0.01f, 100.0f);

		FLOAT MassRatio = NewMass/OldMass;
		NxVec3 InertiaTensor = nActor->getMassSpaceInertiaTensor();
		nActor->setMassSpaceInertiaTensor(InertiaTensor * MassRatio);
		nActor->setMass(NewMass);

		// Apply the COMNudge
		if(!Setup->COMNudge.IsZero())
		{
			NxVec3 nCOMNudge = U2NPosition(Setup->COMNudge);
			NxVec3 nCOMPos = nActor->getCMassLocalPosition();
			nActor->setCMassOffsetLocalPosition(nCOMPos + nCOMNudge);
		}
	}
#endif
}

/** Update instance's linear and angular damping */
void URB_BodyInstance::UpdateDampingProperties()
{
#if WITH_NOVODEX
	NxActor* nActor = (NxActor*)BodyData;
	if(nActor)
	{
		UPhysicalMaterial*	PhysMat  = GetPhysicalMaterial();

		nActor->setLinearDamping(PhysMat->LinearDamping * InstanceDampingScale);
		nActor->setAngularDamping(PhysMat->AngularDamping * InstanceDampingScale);
	}
#endif
}

///////////////////////////////////////
//////// UPhysicsAssetInstance ////////
///////////////////////////////////////

void UPhysicsAssetInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << CollisionDisableTable;
}

void UPhysicsAssetInstance::EnableCollision(class URB_BodyInstance* BodyA, class URB_BodyInstance* BodyB)
{
	if(BodyA == BodyB)
		return;

	FRigidBodyIndexPair Key(BodyA->BodyIndex, BodyB->BodyIndex);

	// If its not in table - do nothing
	if( !CollisionDisableTable.Find(Key) )
		return;

	CollisionDisableTable.Remove(Key);

#if WITH_NOVODEX
	NxActor* ActorA = BodyA->GetNxActor();
	NxActor* ActorB = BodyB->GetNxActor();

	if (ActorA && ActorB)
	{
		NxScene* NovodexScene = &(ActorA->getScene());
		NxU32 CurrentFlags = NovodexScene->getActorPairFlags(*ActorA, *ActorB);
		NovodexScene->setActorPairFlags(*ActorA, *ActorB, CurrentFlags & ~NX_IGNORE_PAIR);
	}
#endif // WITH_NOVODEX
}

void UPhysicsAssetInstance::DisableCollision(class URB_BodyInstance* BodyA, class URB_BodyInstance* BodyB)
{
	if(BodyA == BodyB)
		return;

	FRigidBodyIndexPair Key(BodyA->BodyIndex, BodyB->BodyIndex);

	// If its already in the disable table - do nothing
	if( CollisionDisableTable.Find(Key) )
		return;

	CollisionDisableTable.Set(Key, 0);

#if WITH_NOVODEX
	NxActor* ActorA = BodyA->GetNxActor();
	NxActor* ActorB = BodyB->GetNxActor();

	if (ActorA && ActorB)
	{
		NxScene* NovodexScene = &(ActorA->getScene());
		NxU32 CurrentFlags = NovodexScene->getActorPairFlags(*ActorA, *ActorB);
		NovodexScene->setActorPairFlags(*ActorA, *ActorB, CurrentFlags | NX_IGNORE_PAIR);
	}
#endif // WITH_NOVODEX
}

// Called to actually start up the physics of
void UPhysicsAssetInstance::InitInstance(USkeletalMeshComponent* SkelComp, class UPhysicsAsset* PhysAsset, UBOOL bFixed, FRBPhysScene* InRBScene)
{
	Owner = SkelComp->GetOwner();
	
	FString OwnerName( TEXT("None") );
	if(Owner)
	{
		OwnerName = Owner->GetName();
	}

	if(!InRBScene)
	{
		debugf(TEXT("UPhysicsAssetInstance::InitInstance : No RBPhysScene: %s"), *OwnerName);
		return;
	}

	if(!SkelComp->SkeletalMesh)
	{
		debugf(TEXT("UPhysicsAssetInstance::InitInstance : SkeletalMeshComponent has no SkeletalMesh: %s"), *OwnerName);
		return;
	}

	FVector Scale3D = SkelComp->Scale * SkelComp->Scale3D;
	if (Owner != NULL)
	{
		Scale3D *= Owner->DrawScale * Owner->DrawScale3D;
	}

	if( !Scale3D.IsUniform() )
	{
		debugf(TEXT("UPhysicsAssetInstance::InitInstance : Actor has non-uniform scaling: %s"), *OwnerName);
		return;
	}
	FLOAT Scale = Scale3D.X;

	if( Bodies.Num() != PhysAsset->BodySetup.Num() )
	{
		debugf(TEXT("UPhysicsAssetInstance::InitInstance : Asset/AssetInstance Body Count Mismatch (%d/%d) : %s"), PhysAsset->BodySetup.Num(), Bodies.Num(), *OwnerName);
		return;
	}

	if( Constraints.Num() != PhysAsset->ConstraintSetup.Num() )
	{
		debugf(TEXT("UPhysicsAssetInstance::InitInstance : Asset/AssetInstance Counstraint Count Mismatch (%d/%d) : %s"), PhysAsset->ConstraintSetup.Num(), Constraints.Num(), *OwnerName);
		return;
	}

	// Find root physics body
	USkeletalMesh* SkelMesh = SkelComp->SkeletalMesh;
	RootBodyIndex = INDEX_NONE;
	for(INT i=0; i<SkelMesh->RefSkeleton.Num(); i++)
	{
		INT BodyInstIndex = PhysAsset->FindBodyIndex( SkelMesh->RefSkeleton(i).Name );
		if(BodyInstIndex != INDEX_NONE)
		{
			RootBodyIndex = BodyInstIndex;
			break;
		}
	}

	if(RootBodyIndex == INDEX_NONE)
	{
		debugf(TEXT("UPhysicsAssetInstance::InitInstance : Could not find root physics body: %s; Skeletal Mesh: %s, Physics Asset: %s."),
			Owner ? *Owner->GetName() : TEXT("NONE"),
			(SkelComp && SkelComp->SkeletalMesh) ? *SkelComp->SkeletalMesh->GetName() : TEXT("NONE"),
			PhysAsset ? *PhysAsset->GetName() : TEXT("NONE")
			  );
		return;
	}

	// Create all the bodies.
	for(INT i=0; i<Bodies.Num(); i++)
	{
		URB_BodyInstance* BodyInst = Bodies(i);
		check(BodyInst);

		// Set the BodyIndex property in the BodyInstance.
		BodyInst->BodyIndex = i;

		// Get transform of bone by name.
		INT BoneIndex = SkelComp->MatchRefBone( PhysAsset->BodySetup(i)->BoneName );
		if(BoneIndex == INDEX_NONE)
		{
			continue;
		}

		FMatrix BoneTM = SkelComp->GetBoneMatrix( BoneIndex );
		BoneTM.RemoveScaling();

		// Create physics body instance.
		if ( bInitBodies )
		{
			BodyInst->InitBody( PhysAsset->BodySetup(i), BoneTM, Scale3D, bFixed, SkelComp, InRBScene);
		}

		// Enable bone springs if desired.
		if(BodyInst->bEnableBoneSpringLinear || BodyInst->bEnableBoneSpringAngular)
		{
			// If bMakeSpringToBaseCollisionComponent is TRUE, transform needs to be relative to base component body.
			if(BodyInst->bMakeSpringToBaseCollisionComponent)
			{
				if( SkelComp->GetOwner() &&
					SkelComp->GetOwner()->Base &&
					SkelComp->GetOwner()->Base->CollisionComponent )
				{
					FMatrix BaseL2W = SkelComp->GetOwner()->Base->CollisionComponent->LocalToWorld;
					FMatrix InvBaseL2W = BaseL2W.Inverse();
					BoneTM = BoneTM * InvBaseL2W;
				}
			}

			BodyInst->EnableBoneSpring(BodyInst->bEnableBoneSpringLinear, BodyInst->bEnableBoneSpringAngular, BoneTM);
		}
	}

	// Create all the constraints.
	for(INT i=0; i<Constraints.Num(); i++)
	{
		URB_ConstraintInstance* ConInst = Constraints(i);
		check( ConInst );

		// Set the ConstraintIndex property in the ConstraintInstance.
		ConInst->ConstraintIndex = i;

		// See if both bodies needed for this constraint actually exist in the skeletal mesh. Don't create constraint if this is not the case.
		UBOOL bConstraintOK = TRUE;

		FName ConBone1 = PhysAsset->ConstraintSetup(i)->ConstraintBone1;
		if( ConBone1 != NAME_None )
		{
			if( SkelComp->MatchRefBone(ConBone1) == INDEX_NONE )
			{
				bConstraintOK = FALSE;
			}
		}

		FName ConBone2 = PhysAsset->ConstraintSetup(i)->ConstraintBone2;
		if( ConBone2 != NAME_None )
		{
			if( SkelComp->MatchRefBone(ConBone2) == INDEX_NONE )
			{
				bConstraintOK = FALSE;
			}
		}

		// Constraint is OK - create physics joint
		if(bConstraintOK)
		{
			Constraints(i)->InitConstraint( SkelComp, SkelComp, PhysAsset->ConstraintSetup(i), Scale, Owner, SkelComp, FALSE );
		}
	}

	// Fill in collision disable table information.
	for(INT i=1; i<Bodies.Num(); i++)
	{
		for(INT j=0; j<i; j++)
		{
			FRigidBodyIndexPair Key(j,i);
			if( CollisionDisableTable.Find(Key) )
			{
#if WITH_NOVODEX
				NxActor* ActorA = Bodies(i)->GetNxActor();
				NxActor* ActorB = Bodies(j)->GetNxActor();

				if (ActorA && ActorB)
				{
					// hardware scene support
					NxScene * NovodexScene = &ActorA->getScene();
					check( &ActorB->getScene() == NovodexScene );

					NxU32 CurrentFlags = NovodexScene->getActorPairFlags(*ActorA, *ActorB);
					NovodexScene->setActorPairFlags(*ActorA, *ActorB, CurrentFlags | NX_IGNORE_PAIR);
				}
#endif // WITH_NOVODEX
			}
		}
	}	

}

/**
 *	Clean up all the physics engine info for this asset instance.
 *	If Scene is NULL, it will always shut down physics. If an RBPhysScene is passed in, it will only shut down physics if the asset is in that scene. 
 *	Returns TRUE if physics was shut down, and FALSE otherwise.
 */
UBOOL UPhysicsAssetInstance::TermInstance(FRBPhysScene* Scene)
{
	UBOOL bTerminating = FALSE;

	// We shut down the physics for each body and constraint here. 
	// The actual UObjects will get GC'd

	for(INT i=0; i<Constraints.Num(); i++)
	{
		check( Constraints(i) );
		UBOOL bTerminated = Constraints(i)->TermConstraint(Scene, FALSE);
		if(bTerminated)
		{
			GWorld->ReturnRBConstraint( Constraints(i) );
			Constraints(i) = NULL;
			bTerminating = TRUE;
		}
	}

	// In the destructible we intentionally remove bone physics.
	// In those cases bInitBodies will be false and we don't want an error.
	if ( bInitBodies == FALSE )
	{
		bTerminating = TRUE;
	}

	for(INT i=0; i<Bodies.Num(); i++)
	{
		check( Bodies(i) );
		UBOOL bTerminated = Bodies(i)->TermBody(Scene);
		if(bTerminated)
		{
			GWorld->ReturnRBBody( Bodies(i) );
			Bodies(i) = NULL;
			bTerminating = TRUE;
		}
	}

	return bTerminating;
}

/** Terminate physics on all bodies below the named bone */
void UPhysicsAssetInstance::TermBodiesBelow(FName ParentBoneName, class USkeletalMeshComponent* SkelComp)
{
	// Check we have an asset and mesh
	if(SkelComp->PhysicsAsset && SkelComp->SkeletalMesh)
	{
		check(Bodies.Num() == SkelComp->PhysicsAsset->BodySetup.Num());

		// Get index of parent bone
		INT ParentBoneIndex = SkelComp->MatchRefBone(ParentBoneName);
		if(ParentBoneIndex == INDEX_NONE)
		{
			debugf(TEXT("TermBodiesBelow: ParentBoneName '%s' is invalid"), *ParentBoneName.ToString());
			return;
		}

		// First terminate any constraints at below this bone
		for(INT i=0; i<SkelComp->PhysicsAsset->ConstraintSetup.Num(); i++)
		{
			// Get bone index of constraint
			FName JointName = SkelComp->PhysicsAsset->ConstraintSetup(i)->JointName;
			INT JointBoneIndex = SkelComp->MatchRefBone(JointName);

			// If constraint has bone in mesh, and is either the parent or child of it, term it
			if(	JointBoneIndex != INDEX_NONE && (JointName == ParentBoneName ||	SkelComp->SkeletalMesh->BoneIsChildOf(JointBoneIndex, ParentBoneIndex)) )
			{
				Constraints(i)->TermConstraint(NULL, FALSE);
			}
		}

		// Then iterate over bodies looking for any which are children of supplied parent
		for(INT i=0; i<SkelComp->PhysicsAsset->BodySetup.Num(); i++)
		{
			// Get bone index of body
			FName BodyName = SkelComp->PhysicsAsset->BodySetup(i)->BoneName;
			INT BodyBoneIndex = SkelComp->MatchRefBone(BodyName);

			// If body has bone in mesh, and is either the parent or child of it, term it
			if(	BodyBoneIndex != INDEX_NONE && (BodyName == ParentBoneName ||	SkelComp->SkeletalMesh->BoneIsChildOf(BodyBoneIndex, ParentBoneIndex)) )
			{
				Bodies(i)->TermBody(NULL);
			}
		}


	}
}

/** 
 * Enable/Disable collision on all bodies below the named bone 
 * @param bEnable - Enable collision if true. Disable it otherwise. 
 * @param ParentBoneName - ParentBoneName to disable/enable collision of
 * @param SkelComp - SkeletalComponent of the Physics Asset Instance
 */
void UPhysicsAssetInstance::EnableCollisionBodiesBelow(UBOOL bEnable, FName ParentBoneName, class USkeletalMeshComponent* SkelComp)
{
	// Check we have an asset and mesh
	if(SkelComp->PhysicsAsset && SkelComp->SkeletalMesh)
	{
		check(Bodies.Num() == SkelComp->PhysicsAsset->BodySetup.Num());

		// Get index of parent bone
		INT ParentBoneIndex = SkelComp->MatchRefBone(ParentBoneName);
		if(ParentBoneIndex == INDEX_NONE)
		{
			debugf(TEXT("TermBodiesBelow: ParentBoneName '%s' is invalid"), *ParentBoneName.ToString());
			return;
		}

		// Then iterate over bodies looking for any which are children of supplied parent
		for(INT i=0; i<SkelComp->PhysicsAsset->BodySetup.Num(); i++)
		{
			// Get bone index of body
			FName BodyName = SkelComp->PhysicsAsset->BodySetup(i)->BoneName;
			INT BodyBoneIndex = SkelComp->MatchRefBone(BodyName);

			// If body has bone in mesh, and is either the parent or child of it, term it
			if(	BodyBoneIndex != INDEX_NONE && (BodyName == ParentBoneName ||	SkelComp->SkeletalMesh->BoneIsChildOf(BodyBoneIndex, ParentBoneIndex)) )
			{
				if ( bEnable )
				{
					Bodies(i)->EnableCollisionResponse(TRUE);
				}
				else
				{
					Bodies(i)->EnableCollisionResponse(FALSE);
				}
			}
		}
	}
}
/** Function to scale strength of all linear motors in the constraint instance. */
void UPhysicsAssetInstance::SetLinearDriveScale(FLOAT InLinearSpringScale, FLOAT InLinearDampingScale, FLOAT InLinearForceLimitScale)
{
	// Update params
	LinearSpringScale = InLinearSpringScale;
	LinearDampingScale = InLinearDampingScale;
	LinearForceLimitScale = InLinearForceLimitScale;

	// Iterate over each joint calling SetLinearDriveParams. This will then update motors taking into account the new drive scaling in the owning instance.
	for(INT i=0; i<Constraints.Num(); i++)
	{
		URB_ConstraintInstance* ConInst = Constraints(i);
		check(ConInst);

		ConInst->SetLinearDriveParams(ConInst->LinearDriveSpring, ConInst->LinearDriveDamping, ConInst->LinearDriveForceLimit);
	}
}

/** Function to scale strength of all angular motors in the constraint instance. */
void UPhysicsAssetInstance::SetAngularDriveScale(FLOAT InAngularSpringScale, FLOAT InAngularDampingScale, FLOAT InAngularForceLimitScale)
{
	// Update params
	AngularSpringScale		= InAngularSpringScale;
	AngularDampingScale		= InAngularDampingScale;
	AngularForceLimitScale	= InAngularForceLimitScale;

	// Iterate over each joint calling SetAngularDriveParams. This will then update motors taking into account the new drive scaling in the owning instance.
	for(INT i=0; i<Constraints.Num(); i++)
	{
		URB_ConstraintInstance* ConInst = Constraints(i);
		check(ConInst);

		ConInst->SetAngularDriveParams(ConInst->AngularDriveSpring, ConInst->AngularDriveDamping, ConInst->AngularDriveForceLimit);
	}
}

/** Utility which returns total mass of all bones below the supplied one in the hierarchy (including this one). */
FLOAT UPhysicsAssetInstance::GetTotalMassBelowBone(FName InBoneName, UPhysicsAsset* InAsset, USkeletalMesh* InSkelMesh)
{
	if(!InAsset || !InSkelMesh)
	{
		return 0.f;
	}

	TArray<INT> BodyIndices;
	InAsset->GetBodyIndicesBelow(BodyIndices, InBoneName, InSkelMesh);

	FLOAT TotalMass = 0.f;
	for(INT i=0; i<BodyIndices.Num(); i++)
	{
		TotalMass += Bodies(BodyIndices(i))->GetBodyMass();
	}

	return TotalMass;
}


/** Fix or unfix all bodies */
void UPhysicsAssetInstance::SetAllBodiesFixed(UBOOL bNewFixed)
{
	for(INT i=0; i<Bodies.Num(); i++)
	{
		Bodies(i)->SetFixed(bNewFixed);
	}
}

/** Fix or unfix a list of bodies, by name */
void UPhysicsAssetInstance::SetNamedBodiesFixed(UBOOL bNewFixed, const TArray<FName>& BoneNames, USkeletalMeshComponent* SkelMesh, UBOOL bSetOtherBodiesToComplement, UBOOL bSkipFullAnimWeightBodies)
{
	if( !SkelMesh || !SkelMesh->PhysicsAsset || !SkelMesh->PhysicsAssetInstance )
	{
		debugf(TEXT("UPhysicsAssetInstance::SetNamedBodiesFixed No SkeletalMesh or PhysicsAssetInstance for %s"), *SkelMesh->GetName());
		return;
	}

	// Fix / Unfix bones
	for(INT i=0; i<SkelMesh->PhysicsAsset->BodySetup.Num(); i++)
	{
		URB_BodyInstance*	BodyInst	= SkelMesh->PhysicsAssetInstance->Bodies(i);
		URB_BodySetup*		BodySetup	= SkelMesh->PhysicsAsset->BodySetup(i);

		if( !bSkipFullAnimWeightBodies || !BodySetup->bAlwaysFullAnimWeight )
		{
			// Update Bodies contained within given list
			if( BoneNames.ContainsItem(BodySetup->BoneName) )
			{
				BodyInst->SetFixed(bNewFixed);
			}
			else if( bSetOtherBodiesToComplement )
			// Set others to complement if bSetOtherBodiesToComplement is TRUE
			{
				BodyInst->SetFixed(!bNewFixed);
			}
		}
	}
}


void UPhysicsAssetInstance::ForceAllBodiesBelowUnfixed( const FName& InBoneName, UPhysicsAsset* InAsset, USkeletalMeshComponent* InSkelMesh, UBOOL InbInstanceAlwaysFullAnimWeight  )
{
	TArray<INT> BodyIndices;
	InAsset->GetBodyIndicesBelow(BodyIndices, InBoneName, InSkelMesh->SkeletalMesh);

	for(INT i=0; i<BodyIndices.Num(); i++)
	{
		//warnf( TEXT( "ForceAllBodiesBelowUnfixed %s" ), *InAsset->BodySetup(BodyIndices(i))->BoneName.ToString() );
		Bodies(BodyIndices(i))->SetFixed( FALSE );
		Bodies(BodyIndices(i))->bForceUnfixed = TRUE;
		Bodies(BodyIndices(i))->bInstanceAlwaysFullAnimWeight = InbInstanceAlwaysFullAnimWeight;
	}
	
	// update FullAnimWeight Flag
	InSkelMesh->UpdateFullAnimWeightBodiesFlag();
}


/** Enable or Disable AngularPositionDrive */
void UPhysicsAssetInstance::SetAllMotorsAngularPositionDrive(UBOOL bEnableSwingDrive, UBOOL bEnableTwistDrive, USkeletalMeshComponent* SkelMeshComp, UBOOL bSkipFullAnimWeightBodies)
{
	for(INT i=0; i<Constraints.Num(); i++)
	{
		if( bSkipFullAnimWeightBodies && SkelMeshComp )
		{
			INT BodyIndex = SkelMeshComp->PhysicsAsset->FindBodyIndex(SkelMeshComp->PhysicsAsset->ConstraintSetup(i)->JointName);
			if( SkelMeshComp->PhysicsAsset->BodySetup(BodyIndex)->bAlwaysFullAnimWeight )
			{
				continue;
			}
		}

		Constraints(i)->SetAngularPositionDrive(bEnableSwingDrive, bEnableTwistDrive);
	}
}

void UPhysicsAssetInstance::SetNamedMotorsAngularPositionDrive(UBOOL bEnableSwingDrive, UBOOL bEnableTwistDrive, const TArray<FName>& BoneNames, USkeletalMeshComponent* SkelMeshComp, UBOOL bSetOtherBodiesToComplement)
{
	if( !SkelMeshComp || !SkelMeshComp->PhysicsAsset || SkelMeshComp->PhysicsAssetInstance != this )
	{
		return;
	}

	for(INT i=0; i<Constraints.Num(); i++)
	{
		URB_ConstraintSetup* CS = SkelMeshComp->PhysicsAsset->ConstraintSetup(Constraints(i)->ConstraintIndex);
		if( CS )
		{
			if( BoneNames.ContainsItem(CS->JointName) )
			{
				Constraints(i)->SetAngularPositionDrive(bEnableSwingDrive, bEnableTwistDrive);
			}
			else if( bSetOtherBodiesToComplement )
			{
				Constraints(i)->SetAngularPositionDrive(!bEnableSwingDrive, !bEnableTwistDrive);
			}
		}
	}
}

void UPhysicsAssetInstance::SetNamedMotorsAngularVelocityDrive(UBOOL bEnableSwingDrive, UBOOL bEnableTwistDrive, const TArray<FName>& BoneNames, USkeletalMeshComponent* SkelMeshComp, UBOOL bSetOtherBodiesToComplement)
{
	if( !SkelMeshComp || !SkelMeshComp->PhysicsAsset || SkelMeshComp->PhysicsAssetInstance != this )
	{
		return;
	}

	for(INT i=0; i<Constraints.Num(); i++)
	{
		URB_ConstraintSetup* CS = SkelMeshComp->PhysicsAsset->ConstraintSetup(Constraints(i)->ConstraintIndex);
		if( CS )
		{
			if( BoneNames.ContainsItem(CS->JointName) )
			{
				Constraints(i)->SetAngularVelocityDrive(bEnableSwingDrive, bEnableTwistDrive);
			}
			else if( bSetOtherBodiesToComplement )
			{
				Constraints(i)->SetAngularVelocityDrive(!bEnableSwingDrive, !bEnableTwistDrive);
			}
		}
	}
}

void UPhysicsAssetInstance::SetAllMotorsAngularVelocityDrive(UBOOL bEnableSwingDrive, UBOOL bEnableTwistDrive, USkeletalMeshComponent* SkelMeshComp, UBOOL bSkipFullAnimWeightBodies)
{
	if( !SkelMeshComp || !SkelMeshComp->PhysicsAsset || SkelMeshComp->PhysicsAssetInstance != this )
	{
		return;
	}

	for(INT i=0; i<Constraints.Num(); i++)
	{
		if( bSkipFullAnimWeightBodies && SkelMeshComp )
		{
			INT BodyIndex = SkelMeshComp->PhysicsAsset->FindBodyIndex(SkelMeshComp->PhysicsAsset->ConstraintSetup(i)->JointName);
			if( SkelMeshComp->PhysicsAsset->BodySetup(BodyIndex)->bAlwaysFullAnimWeight )
			{
				continue;
			}
		}

		Constraints(i)->SetAngularVelocityDrive(bEnableSwingDrive, bEnableTwistDrive);
	}
}


/** Set Angular Drive motors params for all constraint instance */
void UPhysicsAssetInstance::SetAllMotorsAngularDriveParams(FLOAT InSpring, FLOAT InDamping, FLOAT InForceLimit, USkeletalMeshComponent* SkelMeshComp, UBOOL bSkipFullAnimWeightBodies)
{
	for(INT i=0; i<Constraints.Num(); i++)
	{
		if( bSkipFullAnimWeightBodies && SkelMeshComp )
		{
			INT BodyIndex = SkelMeshComp->PhysicsAsset->FindBodyIndex(SkelMeshComp->PhysicsAsset->ConstraintSetup(i)->JointName);
			if( SkelMeshComp->PhysicsAsset->BodySetup(BodyIndex)->bAlwaysFullAnimWeight )
			{
				continue;
			}
		}
		Constraints(i)->SetAngularDriveParams(InSpring, InDamping, InForceLimit);
	}
}


/** Use to toggle and set RigidBody angular and linear bone springs (see RB_BodyInstance). */
void UPhysicsAssetInstance::SetNamedRBBoneSprings(UBOOL bEnable, const TArray<FName>& BoneNames, FLOAT InBoneLinearSpring, FLOAT InBoneAngularSpring, USkeletalMeshComponent* SkelMeshComp)
{
	if( !SkelMeshComp )
	{
		return;
	}

	// Set up springs
	for(INT i=0; i<BoneNames.Num(); i++)
	{
		// Find the body instance to turn on spring for
		URB_BodyInstance* BoneBI	= SkelMeshComp->FindBodyInstanceNamed( BoneNames(i) );
		if( BoneBI && BoneBI->IsValidBodyInstance())
		{
			// Find current matrix of bone
			FMatrix BoneMatrix	= BoneBI->GetUnrealWorldTM();
			if( bEnable )
			{
				// If making bone spring to base body, transform needs to be relative to that body.
				if(BoneBI->bMakeSpringToBaseCollisionComponent)
				{
					if( BoneBI->OwnerComponent && 
						BoneBI->OwnerComponent->GetOwner() && 
						BoneBI->OwnerComponent->GetOwner()->Base && 
						BoneBI->OwnerComponent->GetOwner()->Base->CollisionComponent )
					{
						URB_BodyInstance* SpringBI = BoneBI->OwnerComponent->GetOwner()->Base->CollisionComponent->BodyInstance;
						if(SpringBI && SpringBI->IsValidBodyInstance())
						{
							FMatrix InvSpringBodyTM = SpringBI->GetUnrealWorldTM().Inverse();
							BoneMatrix = BoneMatrix * InvSpringBodyTM;
						}
					}
				}

				BoneBI->BoneLinearSpring	= InBoneLinearSpring;
				BoneBI->BoneAngularSpring	= InBoneAngularSpring;
			}
			BoneBI->EnableBoneSpring(bEnable, bEnable, BoneMatrix);
		}
	}
}

/** Use to toggle collision on particular bodies in the asset. */
void UPhysicsAssetInstance::SetNamedBodiesBlockRigidBody(UBOOL bNewBlockRigidBody, const TArray<FName>& BoneNames, USkeletalMeshComponent* SkelMesh)
{
	if( !SkelMesh || !SkelMesh->PhysicsAsset || !SkelMesh->PhysicsAssetInstance )
	{
		debugf(TEXT("UPhysicsAssetInstance::SetNamedBodiesBlockRigidBody No SkeletalMesh or PhysicsAssetInstance for %s"), *SkelMesh->GetName());
		return;
	}

	// Fix / Unfix bones
	for(INT i=0; i<SkelMesh->PhysicsAsset->BodySetup.Num(); i++)
	{
		URB_BodyInstance*	BodyInst	= SkelMesh->PhysicsAssetInstance->Bodies(i);
		URB_BodySetup*		BodySetup	= SkelMesh->PhysicsAsset->BodySetup(i);

		// Update Bodies contained within given list
		if( BoneNames.ContainsItem(BodySetup->BoneName) )
		{
			BodyInst->SetBlockRigidBody(bNewBlockRigidBody);
		}
	}
}

/** Use to toggle collision on particular bodies in the asset. */
void UPhysicsAssetInstance::SetFullAnimWeightBlockRigidBody(UBOOL bNewBlockRigidBody, USkeletalMeshComponent* SkelMesh)
{
	if( !SkelMesh || !SkelMesh->PhysicsAsset || !SkelMesh->PhysicsAssetInstance )
	{
		debugf(TEXT("UPhysicsAssetInstance::SetFullAnimWeightBlockRigidBody No SkeletalMesh or PhysicsAssetInstance for %s"), *SkelMesh->GetName());
		return;
	}

	// Fix / Unfix bones
	for(INT i=0; i<SkelMesh->PhysicsAsset->BodySetup.Num(); i++)
	{
		URB_BodyInstance*	BodyInst	= SkelMesh->PhysicsAssetInstance->Bodies(i);
		URB_BodySetup*		BodySetup	= SkelMesh->PhysicsAsset->BodySetup(i);

		// Update Bodies contained within given list
		if( BodySetup->bAlwaysFullAnimWeight )
		{
			BodyInst->SetBlockRigidBody(bNewBlockRigidBody);
		}
	}
}

/** Allows you to fix/unfix bodies where bAlwaysFullAnimWeight is set to TRUE in the BodySetup. */
void UPhysicsAssetInstance::SetFullAnimWeightBonesFixed(UBOOL bNewFixed, USkeletalMeshComponent* SkelMesh)
{
	if( !SkelMesh || !SkelMesh->PhysicsAsset || !SkelMesh->PhysicsAssetInstance )
	{
		debugf(TEXT("UPhysicsAssetInstance::SetFullAnimWeightBonesFixed No SkeletalMesh or PhysicsAssetInstance for %s"), SkelMesh ? *SkelMesh->GetName() : TEXT("None"));
		return;
	}

	// Fix / Unfix bones
	for(INT i=0; i<SkelMesh->PhysicsAsset->BodySetup.Num(); i++)
	{
		URB_BodyInstance*	BodyInst	= SkelMesh->PhysicsAssetInstance->Bodies(i);
		URB_BodySetup*		BodySetup	= SkelMesh->PhysicsAsset->BodySetup(i);

		// Set fixed on any bodies with bAlwaysFullAnimWeight set to TRUE
		if( BodySetup->bAlwaysFullAnimWeight )
		{
			BodyInst->SetFixed(bNewFixed);
		}
	}
}

/** Find instance of the body that matches the name supplied. */
URB_BodyInstance* UPhysicsAssetInstance::FindBodyInstance(FName BodyName, UPhysicsAsset* InAsset)
{
	if(InAsset && InAsset->BodySetup.Num() == Bodies.Num())
	{
		INT BodyIndex = InAsset->FindBodyIndex(BodyName);
		if(BodyIndex != INDEX_NONE)
		{
			return Bodies(BodyIndex);
		}
	}

	return NULL;
}

/** Find instance of the constraint that matches the name supplied. */
URB_ConstraintInstance* UPhysicsAssetInstance::FindConstraintInstance(FName ConName, UPhysicsAsset* InAsset)
{
	if(InAsset && InAsset->ConstraintSetup.Num() == Constraints.Num())
	{
		INT ConIndex = InAsset->FindConstraintIndex(ConName);
		if(ConIndex != INDEX_NONE)
		{
			return Constraints(ConIndex);
		}
	}

	return NULL;
}


///////////////////////////////////////
////////// UPhysicalMaterial //////////
///////////////////////////////////////

void UPhysicalMaterial::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
#if WITH_NOVODEX
	if(GWorld->RBPhysScene)
	{
		UINT NovodexMatIndex = GWorld->RBPhysScene->FindPhysMaterialIndex(this);
		NxScene* NovodexScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
		if(NovodexScene)
		{
			// We need to update the Novodex material corresponding to this Unreal one.
			NxMaterial* Material = NovodexScene->getMaterialFromIndex(NovodexMatIndex);
			check(Material);

			Material->setDynamicFriction(Friction);
			Material->setStaticFriction(Friction);
			Material->setRestitution(Restitution);
		}
	}
#endif

	//here we need to check to see if setting the parent to this PhysicalMaterial will
	// make a circular linked list of physical materials

	// we use the age old tortoise and the hare solution to finding a cycle in a linked list
	UPhysicalMaterial* Hare = this;
	UPhysicalMaterial* Turtoise = this;


	// move the tortoise one link forward and the hare two links forward
	// if they ever are the same we have a cycle
	// if the hare makes it to the end of the linked list (NULL) then no cycle
	do
	{	
		// move the tortoise once
		Turtoise = Turtoise->Parent;

		// move the hare forward twice
		Hare = Hare->Parent;
		if( NULL != Hare )
		{
			Hare = Hare->Parent;
		}

	}
	while( ( NULL != Hare )
		&& ( Hare != Turtoise ) 
		);

	// we have reached an end condition need to check which one
	if( NULL == Hare )
	{
		// all good
	}
	else if( Hare == Turtoise )
	{
		// need to send a warning and not allow the setting of the parent node
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_PhysicalMaterialCycleInHierarchy") );
		Parent = NULL;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/** Called when PhysicalMaterial is GC'd to remove it from world's material map. */
void UPhysicalMaterial::FinishDestroy()
{
	if(GWorld && GWorld->RBPhysScene)
	{
		GWorld->RBPhysScene->RemovePhysMaterial(this);
	}

	Super::FinishDestroy();
}


/**
 * This will fix any old PhysicalMaterials that were created in the PhysMaterial's outer instead
 * of correctly inside the PhysMaterial.  This will allow "broken" PhysMaterials to be renamed.
 **/
UBOOL UPhysicalMaterial::Rename( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags )
{
	// A long time ago in a galaxy far away, the properties were created in the physmaterial's outer, not in the physmaterial, so Rename won't work (since it's not in
	// what is being renamed... So, if this is the case, then we move the property into the physmaterial (this)
	if( PhysicalMaterialProperty && PhysicalMaterialProperty->GetOuter() == GetOuter() )
	{
		// the physmatproperty probably has other properties inside it that are also outside of the physmaterial, so we need to fix those up,
		// but they aren't native, so we have to loop through all objectprops looking for the evil ones
		// note: this will only happen for broken physmaterials, so this can't make it any worse
		for (TFieldIterator<UObjectProperty> It(PhysicalMaterialProperty->GetClass()); It; ++It)
		{
			UObject* PropVal = *((UObject**)((BYTE*)PhysicalMaterialProperty + It->Offset));

			if( PropVal != NULL )
			{
				check(PropVal->HasAnyFlags(RF_Public) == FALSE && TEXT("A PhysicalMaterial has properties that are marked public.  Run Fixup redirects to correct this issue."));

				if( PropVal->GetOuter() == GetOuter() )
				{
					PropVal->Rename(NULL, PhysicalMaterialProperty);
				}
			}
		}

		if( PhysicalMaterialProperty->Rename( *MakeUniqueObjectName(this, PhysicalMaterialProperty->GetClass()).ToString(), this ) == FALSE )
		{
			return FALSE;
		}
	}

	// Rename the physical material
	return Super::Rename( InName, NewOuter, Flags );
}

/** Walk up the PhysMat heirarchy to fill in the supplied PhysEffectInfo struct. */
FPhysEffectInfo UPhysicalMaterial::FindPhysEffectInfo(BYTE Type)
{
	// Start by zeroing
	FPhysEffectInfo Info;
	appMemzero(&Info, sizeof(FPhysEffectInfo));

	// Start here..
	UPhysicalMaterial* TestMat = this;

	// ..keep looking until we find all source data or run out of materials
	while( (!Info.Effect || !Info.Sound || Info.Threshold == 0.f || Info.ReFireDelay == 0.f) && TestMat )
	{
		// Attempt to replace any empty slots in the info

		if(!Info.Effect)
		{
			Info.Effect = (Type == EPMET_Impact) ? TestMat->ImpactEffect : TestMat->SlideEffect;
		}

		if(!Info.Sound)
		{
			Info.Sound = (Type == EPMET_Impact) ? TestMat->ImpactSound : TestMat->SlideSound;
		}

		if(Info.Threshold == 0.f)
		{
			Info.Threshold = (Type == EPMET_Impact) ? TestMat->ImpactThreshold : TestMat->SlideThreshold;
		}

		if(Info.ReFireDelay == 0.f)
		{
			Info.ReFireDelay = (Type == EPMET_Impact) ? TestMat->ImpactReFireDelay : TestMat->SlideReFireDelay;
		}

		// Move on to parent
		TestMat = TestMat->Parent;
	}

	return Info;
}

///////////////////////////////////////
////////// AKAsset //////////
///////////////////////////////////////

#if WITH_EDITOR
void AKAsset::CheckForErrors()
{
	Super::CheckForErrors();
	if( SkeletalMeshComponent == NULL )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_KAssetSkeletalComponentNull" ), *GetName() ) ), TEXT( "KAssetSkeletalComponentNull" ) );
	}
	else
	{
		if( SkeletalMeshComponent->SkeletalMesh == NULL )
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_KAssetSkeletalMeshNull" ), *GetName() ) ), TEXT( "KAssetSkeletalMeshNull" ) );
		}
		if( SkeletalMeshComponent->PhysicsAsset == NULL )
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_KAssetPhysicsAssetNull" ), *GetName() ) ), TEXT( "KAssetPhysicsAssetNull" ) );
		}
	}
}
#endif

/** Make KAssets be hit when using the TRACE_Mover flag. */
UBOOL AKAsset::ShouldTrace(UPrimitiveComponent *Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	if( TraceFlags & TRACE_Movers )
	{
		if( TraceFlags & TRACE_OnlyProjActor )
		{
			return (bProjTarget || (bBlockActors && Primitive->BlockActors));
		}
		else
		{
			return (!(TraceFlags & TRACE_Blocking) || (SourceActor && SourceActor->IsBlockedBy(this,Primitive)));
		}
	}
	return FALSE;
}

/** Allow specific control control over pawns being blocked by this KAsset. */
UBOOL AKAsset::IgnoreBlockingBy(const AActor *Other) const
{
	if(!bBlockPawns && Other->GetAPawn())
		return TRUE;

	return Super::IgnoreBlockingBy(Other);
}
