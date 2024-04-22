/*=============================================================================
LandscapeCollision.cpp: Landscape collision
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "UnTerrain.h"
#include "LandscapeDataAccess.h"
#include "LandscapeRender.h"
#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

UBOOL ALandscapeProxy::ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	return (TraceFlags & TRACE_Terrain);
}

void ULandscapeHeightfieldCollisionComponent::UpdateBounds()
{
	Bounds = CachedBoxSphereBounds;
}

void ULandscapeHeightfieldCollisionComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	Super::SetParentToWorld(FTranslationMatrix(FVector(SectionBaseX,SectionBaseY,0)) * ParentToWorld);
}

/** Create a PhysX Heightfield shape and NxActor for Unreal and rigid body collision. */
void ULandscapeHeightfieldCollisionComponent::InitComponentRBPhys(UBOOL bFixed)
{
	// If no physics scene, or we already have a BodyInstance, do nothing.
	if( !BlockRigidBody || !GWorld->RBPhysScene || BodyInstance || bDisableAllRigidBody )
	{
		return;
	}

#if WITH_NOVODEX
	INT CollisionSizeVerts = CollisionSizeQuads+1;

	check(bFixed);

	// Make transform for this landscape component NxActor
	FMatrix LandscapeCompTM = LocalToWorld;
	FVector LandscapeScale = LandscapeCompTM.ExtractScaling();
	FVector TerrainY = LandscapeCompTM.GetAxis(1);
	FVector TerrainZ = LandscapeCompTM.GetAxis(2);
	LandscapeCompTM.SetAxis(2, -TerrainY);
	LandscapeCompTM.SetAxis(1, TerrainZ);
	NxMat34 PhysXLandscapeCompTM = U2NTransform(LandscapeCompTM);

	// Create an RB_BodyInstance for this terrain component and store a pointer to the NxActor in it.
	BodyInstance = GWorld->InstanceRBBody(NULL);
	BodyInstance->BodyData = NULL;
	BodyInstance->OwnerComponent = this;
	BodyInstance->SceneIndex = GWorld->RBPhysScene->NovodexSceneIndex;

	check(GEngine->DefaultPhysMaterial);
	check(GEngine->LandscapeHolePhysMaterial);

	// The properties of this physical material are not important. It must just be unique.
	INT HoleMaterial = GWorld->RBPhysScene->FindPhysMaterialIndex( GEngine->LandscapeHolePhysMaterial );

	// If we have not created a heightfield yet - do it now.
	if(!RBHeightfield)
	{
		ALandscapeProxy* Proxy = GetLandscapeProxy();

		// Default physical material used for regions where the dominant layer has no PhysicalMaterial set.
		INT DefPhysMaterialIndex = GWorld->RBPhysScene->FindPhysMaterialIndex( Proxy && Proxy->DefaultPhysMaterial!=NULL ? Proxy->DefaultPhysMaterial : GEngine->DefaultPhysMaterial );

		TArray<INT> LayerPhysMaterialIndexes(ComponentLayers.Num());
		for( INT Idx=0;Idx<ComponentLayers.Num();Idx++ )
		{
			if (ComponentLayers(Idx) == ALandscape::DataWeightmapName)
			{
				bIncludeHoles = TRUE;
				LayerPhysMaterialIndexes(Idx) = (!GIsEditor || bHeightFieldDataHasHole) ? HoleMaterial : DefPhysMaterialIndex;
			}
			else
			{
				ULandscapeLayerInfoObject* LayerInfo = NULL;
				if (Proxy)
				{
					for (int i = 0; i < Proxy->LayerInfoObjs.Num(); ++i)
					{
						if ( Proxy->LayerInfoObjs(i).LayerInfoObj && Proxy->LayerInfoObjs(i).LayerInfoObj->LayerName == ComponentLayers(Idx) )
						{
							LayerInfo = Proxy->LayerInfoObjs(i).LayerInfoObj;
							break;
						}
					}
				}
				//ULandscapeLayerInfoObject* LayerInfo = Proxy ? *Proxy->LayerInfoObjs.FindItemByKey<FName>(ComponentLayers(Idx)) : NULL;
				LayerPhysMaterialIndexes(Idx) = (LayerInfo && LayerInfo->PhysMaterial) ? GWorld->RBPhysScene->FindPhysMaterialIndex( LayerInfo->PhysMaterial ) : DefPhysMaterialIndex;
			}
		}

		WORD* Heights = (WORD*)CollisionHeightData.Lock(LOCK_READ_ONLY);
		check(CollisionHeightData.GetElementCount()==Square(CollisionSizeVerts));

		BYTE* DominantLayers = DominantLayerData.GetElementCount() > 0 ? (BYTE*)DominantLayerData.Lock(LOCK_READ_ONLY) : NULL;

		TArray<NxHeightFieldSample> Samples;
		Samples.AddZeroed(Square(CollisionSizeVerts));

		for(INT RowIndex = 0; RowIndex < CollisionSizeVerts; RowIndex++)
		{
			for(INT ColIndex = 0; ColIndex < CollisionSizeVerts; ColIndex++)
			{
				INT SrcSampleIndex = (ColIndex * CollisionSizeVerts) + RowIndex;
				INT DstSampleIndex = (RowIndex * CollisionSizeVerts) + ColIndex;

				NxHeightFieldSample& Sample = Samples(DstSampleIndex);
				Sample.height = Clamp<SWORD>(((INT)Heights[SrcSampleIndex]-32768), -32678, 32767);
				
				INT MaterialIndex = (DominantLayers && DominantLayers[SrcSampleIndex] != 255) ? LayerPhysMaterialIndexes(DominantLayers[SrcSampleIndex]) : DefPhysMaterialIndex;
				// TODO: edge turning, holes.
				Sample.tessFlag = 0;
				Sample.materialIndex0 = MaterialIndex;
				Sample.materialIndex1 = MaterialIndex;
			}
		}

		NxHeightFieldDesc HFDesc;
		HFDesc.nbColumns		= CollisionSizeVerts;
		HFDesc.nbRows			= CollisionSizeVerts;
		HFDesc.samples			= Samples.GetData();
		HFDesc.sampleStride		= sizeof(NxU32);
		HFDesc.flags			= NX_HF_NO_BOUNDARY_EDGES;
		// it is necessary to specify a thickness, otherwise the verticalExtent value of 0 will be used, meaning PointCheck will collide underneath landscape.
		HFDesc.thickness		= -1.f;	

		RBHeightfield = GNovodexSDK->createHeightField(HFDesc);

		CollisionHeightData.Unlock();
		if( DominantLayers )
		{
			DominantLayerData.Unlock();
		}
	}
	check(RBHeightfield);

	NxHeightFieldShapeDesc LandscapeComponentShapeDesc;
	LandscapeComponentShapeDesc.heightField		= RBHeightfield;
	LandscapeComponentShapeDesc.shapeFlags		= NX_SF_FEATURE_INDICES | NX_SF_VISUALIZATION;
	LandscapeComponentShapeDesc.heightScale		= LandscapeScale.Z * TERRAIN_ZSCALE * U2PScale;
	LandscapeComponentShapeDesc.rowScale		= LandscapeScale.Y * CollisionScale * U2PScale;
	LandscapeComponentShapeDesc.columnScale		= -LandscapeScale.X * CollisionScale * U2PScale;
	LandscapeComponentShapeDesc.meshFlags		= 0;
	LandscapeComponentShapeDesc.materialIndexHighBits = 0;
	LandscapeComponentShapeDesc.holeMaterial	= HoleMaterial;
	LandscapeComponentShapeDesc.groupsMask		= CreateGroupsMask(RBCC_Default, NULL);
	LandscapeComponentShapeDesc.group			= SHAPE_GROUP_LANDSCAPE;

	// Create actor description and instance it.
	NxActorDesc LandscapeActorDesc;
	LandscapeActorDesc.shapes.pushBack(&LandscapeComponentShapeDesc);
	LandscapeActorDesc.globalPose = PhysXLandscapeCompTM;

	// Create the actual NxActor using the mesh collision shape.
	NxScene* NovodexScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
	check(NovodexScene);

	NxActor* LandscapeNxActor = NovodexScene->createActor(LandscapeActorDesc);

	if(LandscapeNxActor)
	{
		BodyInstance->BodyData = (FPointer)LandscapeNxActor;
		LandscapeNxActor->userData = BodyInstance;
	}
	else
	{
		debugf(TEXT("ULandscapeHeightfieldCollisionComponent::InitComponentRBPhys : Could not create NxActor"));
	}
#endif
}

void ULandscapeHeightfieldCollisionComponent::BeginDestroy()
{
#if WITH_NOVODEX
	// Free the heightfield data.
	if(RBHeightfield)
	{
		GNovodexPendingKillHeightfield.AddItem(RBHeightfield);
		RBHeightfield = NULL;
	}
#endif
#if WITH_EDITOR
	if( GIsEditor && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		ULandscapeInfo* Info = GetLandscapeInfo(FALSE);
		if (Info)
		{
			Info->XYtoCollisionComponentMap.Remove(ALandscape::MakeKey(SectionBaseX,SectionBaseY));
			if (Info->SelectedCollisionComponents.Contains(this))
			{
				Info->SelectedCollisionComponents.Remove(Info->SelectedCollisionComponents.FindId(this));
			}
		}
	}
#endif

	Super::BeginDestroy();
}

void ULandscapeHeightfieldCollisionComponent::RecreateHeightfield(UBOOL bUpdateAddCollision/*= TRUE*/)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		TermComponentRBPhys(NULL);

#if WITH_NOVODEX
		// Free the existing heightfield data.
		if(RBHeightfield)
		{
			GNovodexPendingKillHeightfield.AddItem(RBHeightfield);
			RBHeightfield = NULL;
		}
#endif
#if WITH_EDITOR
		if (bUpdateAddCollision)
		{
			UpdateAddCollisions(TRUE);
		}
#endif

		InitComponentRBPhys(TRUE);
	}
}

void ULandscapeHeightfieldCollisionComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	CollisionHeightData.Serialize(Ar,this);

	if( Ar.Ver() >= VER_LANDSCAPE_PHYS_MATERIALS )
	{
		DominantLayerData.Serialize(Ar,this);
	}
}

#if WITH_EDITOR
void ULandscapeHeightfieldCollisionComponent::PostEditImport()
{
	Super::PostEditImport();
	// Reinitialize physics after paste
	if (CollisionSizeQuads > 0)
	{
		RecreateHeightfield(FALSE);
	}
}

void ULandscapeHeightfieldCollisionComponent::PostEditUndo()
{
	Super::PostEditUndo();
	// Reinitialize physics after undo
	if (CollisionSizeQuads > 0)
	{
		RecreateHeightfield(FALSE);
	}
}

void ULandscapeHeightfieldCollisionComponent::PreSave()
{
	Super::PreSave();
	if( bIncludeHoles && GIsEditor && !GIsPlayInEditorWorld && CollisionSizeQuads > 0 )
	{
		if( !bHeightFieldDataHasHole )
		{
			bHeightFieldDataHasHole = TRUE;
			RecreateHeightfield(FALSE);
		}
	}
};
#endif

/*-----------------------------------------------------------------------------
	Landscape line / point checking
-----------------------------------------------------------------------------*/

#define EXTRA_TRACE_DIST 5.f
#define NX_EXTRA_TRACE_DIST (EXTRA_TRACE_DIST * U2PScale)

UBOOL ULandscapeHeightfieldCollisionComponent::LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags)
{
#if WITH_NOVODEX
	if( BodyInstance )
	{
		// For LineCheck in editor
		if( bIncludeHoles && GIsEditor && !GIsPlayInEditorWorld && CollisionSizeQuads > 0 )
		{
			// Prevent DuringAsyncWork PhysX work
			if ( !(GWorld && GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork) )
			{
				if( TraceFlags & TRACE_TerrainIgnoreHoles )
				{
					if( bHeightFieldDataHasHole )
					{
						bHeightFieldDataHasHole = FALSE;
						RecreateHeightfield(FALSE);
					}
				}
				else
				{
					if( !bHeightFieldDataHasHole )
					{
						bHeightFieldDataHasHole = TRUE;
						RecreateHeightfield(FALSE);
					}
				}
			}
		}

		if( Extent.IsZero() )
		{
			FVector StartEnd = (End-Start);
			FLOAT Distance = StartEnd.Size();
			if( Distance > SMALL_NUMBER )
			{
				FVector UnitDirection = StartEnd / Distance;

				FLOAT NxDistance = Distance * U2PScale;
				FVector NxExtent = Extent * U2PScale;

				// We actually start our trace EXTRA_TRACE_DIST from the requested Start location
				// This ensures we don't go through the terrain should we start the trace
				// infinitesimally close to the surface of the terrain.
				FVector ModifiedStart = (Start - EXTRA_TRACE_DIST * UnitDirection);						
				FLOAT ModifiedDistance = Distance + EXTRA_TRACE_DIST;
				FLOAT NxModifiedDistance = ModifiedDistance * U2PScale;

				NxRay Ray;
				Ray.orig = U2NVectorCopy(ModifiedStart * U2PScale);
				Ray.dir = U2NVectorCopy(UnitDirection);

				NxActor* HeightfieldActor = (NxActor*)BodyInstance->BodyData;
				check(HeightfieldActor->getNbShapes() == 1);
				NxShape* HeightfieldShape = (HeightfieldActor->getShapes())[0];
				check(HeightfieldShape);

				NxU32 hintFlags = NX_RAYCAST_NORMAL | NX_RAYCAST_IMPACT | NX_RAYCAST_DISTANCE;
				if( TraceFlags & TRACE_Material )
				{
					hintFlags |= NX_RAYCAST_MATERIAL;
				}

				NxRaycastHit RaycastHit;
				if( HeightfieldShape->raycast(Ray, NxModifiedDistance, hintFlags, RaycastHit, (TraceFlags & TRACE_StopAtAnyHit) ? TRUE : FALSE ) )
				{
					if( (N2UVectorCopy(RaycastHit.worldNormal) | UnitDirection) > 0.f )
					{
						// Ignore hits to backfaces
						return TRUE;
					}

					// Check for hit before the start of the trace. 
					if( RaycastHit.distance <= NX_EXTRA_TRACE_DIST )
					{				
						// Return a hit time of 0.
						Result.Time = 0.f;
						Result.Location = Start;
					}
					else
					{
						Result.Time			= (RaycastHit.distance - NX_EXTRA_TRACE_DIST) / NxDistance;
						Result.Location		= N2UVectorCopy(RaycastHit.worldImpact) * P2UScale;
					}
					
					Result.Normal		= N2UVectorCopy(RaycastHit.worldNormal).SafeNormal();
					Result.Actor		= Owner;
					Result.Component	= this;

					if( TraceFlags & TRACE_Material )
					{
						NxMaterial* HitNxMaterial = GWorld->RBPhysScene->GetNovodexPrimaryScene()->getMaterialFromIndex(RaycastHit.materialIndex);
						Result.PhysMaterial	= (UPhysicalMaterial*)HitNxMaterial->userData;
					}
					return FALSE;
				}
			}
		}
		else
		if( GWorld->RBPhysScene )
		{
			FVector StartEnd = (End-Start);
			FLOAT Distance = StartEnd.Size();
			if( Distance > SMALL_NUMBER )
			{
				FVector NxExtent = Extent * U2PScale;
				FLOAT InvDistance = 1.f / Distance;
				FVector Direction = StartEnd * InvDistance;

				// We actually start our trace EXTRA_TRACE_DIST from the requested Start location
				// This ensures we don't fall through the terrain should Unreal walking code start the trace
				// infinitesimally close to the surface of the terrain.
				FVector ModifiedStart = (Start - EXTRA_TRACE_DIST * Direction);

				FVector ModifiedStartEnd = (End-ModifiedStart);
				FVector NxStartEnd = ModifiedStartEnd * U2PScale;
				FVector NxModifiedStart = ModifiedStart * U2PScale;

				NxVec3 NxModifiedStartEnd = U2NVectorCopy(ModifiedStartEnd * U2PScale);

				NxBox testBox;
				testBox.setEmpty();
				testBox.center	= U2NVectorCopy(NxModifiedStart);
				testBox.extents	= U2NVectorCopy(NxExtent);

				NxSweepQueryHit SweepHit;

				NxActor* HeightfieldActor = (NxActor*)BodyInstance->BodyData;
				check(HeightfieldActor->getNbShapes() == 1);
				NxShape* HeightfieldShape = (HeightfieldActor->getShapes())[0];
				check(HeightfieldShape);

				if( GWorld->RBPhysScene->GetNovodexPrimaryScene()->linearOBBSpecificShapeSweep(testBox, HeightfieldShape, NxModifiedStartEnd, SweepHit) )
				{
					if( SweepHit.t <= 1.f )
					{
						if( (EXTRA_TRACE_DIST+Distance) * SweepHit.t <= EXTRA_TRACE_DIST )
						{
							// Hit before the start of the trace. Return a hit time of 0.
							Result.Time = 0.f;
							Result.Location = Start;						
						}
						else
						{
							Result.Location = ModifiedStart + ModifiedStartEnd * SweepHit.t;
							Result.Time = (Result.Location - Start).Size() * InvDistance;
						}

						Result.Component	= this;
						Result.Actor = this->GetOwner();
						Result.Normal = N2UVectorCopy(SweepHit.normal).SafeNormal();
						return FALSE;
					}
				}
			}
		}
	}
#endif
	return TRUE;
}

UBOOL ULandscapeHeightfieldCollisionComponent::PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags)
{
#if WITH_NOVODEX
	if( BodyInstance )
	{
		NxActor* HeightfieldActor = (NxActor*)BodyInstance->BodyData;
		check(HeightfieldActor->getNbShapes() == 1);
		NxShape* HeightfieldShape = (HeightfieldActor->getShapes())[0];
		check(HeightfieldShape);

		NxBounds3 WorldBounds;
		WorldBounds.set(U2NVectorCopy((Location-Extent) * U2PScale), U2NVectorCopy((Location+Extent) * U2PScale));

		if( HeightfieldShape->checkOverlapAABB(WorldBounds) )
		{
			Result.Actor		= Owner;
			Result.Component	= this;
			Result.Time			= 0.f;
			Result.Location		= Location;
			return FALSE;
		}
	}
#endif
	return TRUE;
}

#if WITH_EDITOR
ELandscapeSetupErrors ULandscapeHeightfieldCollisionComponent::SetupActor(UBOOL bForce /*= FALSE*/ )
{
	if( GIsEditor && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		ULandscapeInfo* Info = GetLandscapeInfo();
		if (Info)
		{
			QWORD LandscapeKey = ALandscape::MakeKey(SectionBaseX,SectionBaseY);
			if (Info->XYtoCollisionComponentMap.FindRef(LandscapeKey) && !bForce)
			{
				return LSE_CollsionXY;
			}
			Info->XYtoCollisionComponentMap.Set(LandscapeKey, this);
			return LSE_None;
		}
		return LSE_NoLandscapeInfo;
	}
	return LSE_None;
}

void ULandscapeHeightfieldCollisionComponent::PostLoad()
{
	Super::PostLoad();
	//SetupActor();
}

void ULandscapeInfo::UpdateAllAddCollisions()
{
	//XYtoAddCollisionMap.Empty();
	for (TMap<QWORD, ULandscapeHeightfieldCollisionComponent*>::TIterator It(XYtoCollisionComponentMap); It; ++It )
	{
		ULandscapeHeightfieldCollisionComponent* Comp = It.Value();
		if (Comp)
		{
			Comp->UpdateAddCollisions();
		}
	}
}

void ULandscapeHeightfieldCollisionComponent::UpdateAddCollisions(UBOOL bForceUpdate /*= FALSE*/)
{
	ULandscapeInfo* Info = GetLandscapeInfo();
	if (Info)
	{
		ALandscapeProxy* Proxy = GetLandscapeProxy();
		QWORD NeighborsKeys[8] = 
		{
			ALandscape::MakeKey(SectionBaseX-Proxy->ComponentSizeQuads,	SectionBaseY-Proxy->ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX,							SectionBaseY-Proxy->ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX+Proxy->ComponentSizeQuads,	SectionBaseY-Proxy->ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX-Proxy->ComponentSizeQuads,	SectionBaseY),
			ALandscape::MakeKey(SectionBaseX+Proxy->ComponentSizeQuads,	SectionBaseY),
			ALandscape::MakeKey(SectionBaseX-Proxy->ComponentSizeQuads,	SectionBaseY+Proxy->ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX,							SectionBaseY+Proxy->ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX+Proxy->ComponentSizeQuads,	SectionBaseY+Proxy->ComponentSizeQuads)
		};

		// Search for Neighbors...
		for (INT i = 0; i < 8; ++i)
		{
			if (!Info->XYtoCollisionComponentMap.FindRef(NeighborsKeys[i]))
			{
				Info->UpdateAddCollision(NeighborsKeys[i], bForceUpdate);
			}
			else
			{
				Info->XYtoAddCollisionMap.Remove(NeighborsKeys[i]);
			}
		}
	}
}

void ULandscapeInfo::UpdateAddCollision(QWORD LandscapeKey, UBOOL bForceUpdate /*= FALSE*/ )
{
	if (!LandscapeProxy)
	{
		return;
	}
	INT SectionBaseX, SectionBaseY;
	ALandscape::UnpackKey(LandscapeKey, SectionBaseX, SectionBaseY);

	FLandscapeAddCollision* AddCollision = XYtoAddCollisionMap.Find(LandscapeKey);
	if (!AddCollision)
	{
		AddCollision = &XYtoAddCollisionMap.Set(LandscapeKey, FLandscapeAddCollision());
	}
	else if (!bForceUpdate)
	{
		return;
	}
	check(AddCollision);

	// 8 Neighbors... 
	// 0 1 2 
	// 3   4
	// 5 6 7
	QWORD NeighborsKeys[8] = 
	{
		ALandscape::MakeKey(SectionBaseX-LandscapeProxy->ComponentSizeQuads,	SectionBaseY-LandscapeProxy->ComponentSizeQuads),
		ALandscape::MakeKey(SectionBaseX,										SectionBaseY-LandscapeProxy->ComponentSizeQuads),
		ALandscape::MakeKey(SectionBaseX+LandscapeProxy->ComponentSizeQuads,	SectionBaseY-LandscapeProxy->ComponentSizeQuads),
		ALandscape::MakeKey(SectionBaseX-LandscapeProxy->ComponentSizeQuads,	SectionBaseY),
		ALandscape::MakeKey(SectionBaseX+LandscapeProxy->ComponentSizeQuads,	SectionBaseY),
		ALandscape::MakeKey(SectionBaseX-LandscapeProxy->ComponentSizeQuads,	SectionBaseY+LandscapeProxy->ComponentSizeQuads),
		ALandscape::MakeKey(SectionBaseX,										SectionBaseY+LandscapeProxy->ComponentSizeQuads),
		ALandscape::MakeKey(SectionBaseX+LandscapeProxy->ComponentSizeQuads,	SectionBaseY+LandscapeProxy->ComponentSizeQuads)
	};

	ULandscapeHeightfieldCollisionComponent* NeighborCollisions[8];
	// Search for Neighbors...
	for (INT i = 0; i < 8; ++i)
	{
		NeighborCollisions[i] = XYtoCollisionComponentMap.FindRef(NeighborsKeys[i]);
	}

	BYTE CornerSet = 0;
	BYTE OriginalSet = 0;
	WORD HeightCorner[4];

	// Corner Cases...
	if (NeighborCollisions[0])
	{
		WORD* Heights = (WORD*)NeighborCollisions[0]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[0]->CollisionSizeQuads + 1;
		HeightCorner[0] = Heights[ CollisionSizeVerts-1 + (CollisionSizeVerts-1)*CollisionSizeVerts ];
		CornerSet |= 1;
		NeighborCollisions[0]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[2])
	{
		WORD* Heights = (WORD*)NeighborCollisions[2]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[2]->CollisionSizeQuads + 1;
		HeightCorner[1] = Heights[ (CollisionSizeVerts-1)*CollisionSizeVerts ];
		CornerSet |= 1 << 1;
		NeighborCollisions[2]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[5])
	{
		WORD* Heights = (WORD*)NeighborCollisions[5]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[5]->CollisionSizeQuads + 1;
		HeightCorner[2] = Heights[ (CollisionSizeVerts-1) ];
		CornerSet |= 1 << 2;
		NeighborCollisions[5]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[7])
	{
		WORD* Heights = (WORD*)NeighborCollisions[7]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[7]->CollisionSizeQuads + 1;
		HeightCorner[3] = Heights[ 0 ];
		CornerSet |= 1 << 3;
		NeighborCollisions[7]->CollisionHeightData.Unlock();
	}

	// Other cases...
	if (NeighborCollisions[1])
	{
		WORD* Heights = (WORD*)NeighborCollisions[1]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[1]->CollisionSizeQuads + 1;
		HeightCorner[0] = Heights[ (CollisionSizeVerts-1)*CollisionSizeVerts ];
		CornerSet |= 1;
		HeightCorner[1] = Heights[ CollisionSizeVerts-1 + (CollisionSizeVerts-1)*CollisionSizeVerts ];
		CornerSet |= 1 << 1;
		NeighborCollisions[1]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[3])
	{
		WORD* Heights = (WORD*)NeighborCollisions[3]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[3]->CollisionSizeQuads + 1;
		HeightCorner[0] = Heights[ (CollisionSizeVerts-1) ];
		CornerSet |= 1;
		HeightCorner[2] = Heights[ CollisionSizeVerts-1 + (CollisionSizeVerts-1)*CollisionSizeVerts ];
		CornerSet |= 1 << 2;
		NeighborCollisions[3]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[4])
	{
		WORD* Heights = (WORD*)NeighborCollisions[4]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[4]->CollisionSizeQuads + 1;
		HeightCorner[1] = Heights[ 0 ];
		CornerSet |= 1 << 1;
		HeightCorner[3] = Heights[ (CollisionSizeVerts-1)*CollisionSizeVerts ];
		CornerSet |= 1 << 3;
		NeighborCollisions[4]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[6])
	{
		WORD* Heights = (WORD*)NeighborCollisions[6]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[6]->CollisionSizeQuads + 1;
		HeightCorner[2] = Heights[ 0 ];
		CornerSet |= 1 << 2;
		HeightCorner[3] = Heights[ (CollisionSizeVerts-1) ];
		CornerSet |= 1 << 3;
		NeighborCollisions[6]->CollisionHeightData.Unlock();
	}

	OriginalSet = CornerSet;

	// Fill unset values
	// First iteration only for valid values distance 1 propagation
	// Second iteration fills left ones...
	FillCornerValues(CornerSet, HeightCorner);
	//check(CornerSet == 15);

	// Transform Height to Vectors...
	FMatrix LtoW = LandscapeProxy->LocalToWorld();
	AddCollision->Corners[0] = LtoW.TransformFVector( FVector(SectionBaseX,									   SectionBaseY,									LandscapeDataAccess::GetLocalHeight(HeightCorner[0])) );
	AddCollision->Corners[1] = LtoW.TransformFVector( FVector(SectionBaseX+LandscapeProxy->ComponentSizeQuads, SectionBaseY,									LandscapeDataAccess::GetLocalHeight(HeightCorner[1])) );
	AddCollision->Corners[2] = LtoW.TransformFVector( FVector(SectionBaseX,									   SectionBaseY+LandscapeProxy->ComponentSizeQuads, LandscapeDataAccess::GetLocalHeight(HeightCorner[2])) );
	AddCollision->Corners[3] = LtoW.TransformFVector( FVector(SectionBaseX+LandscapeProxy->ComponentSizeQuads, SectionBaseY+LandscapeProxy->ComponentSizeQuads, LandscapeDataAccess::GetLocalHeight(HeightCorner[3])) );
}

/** Return a list of collision triangles in world space */
void ULandscapeHeightfieldCollisionComponent::GetCollisionTriangles( TArray<FVector>& OutTriangleVerts )
{
	WORD* Heights = (WORD*)CollisionHeightData.Lock(LOCK_READ_ONLY);
	INT NumHeights = Square(CollisionSizeQuads+1);
	check(CollisionHeightData.GetElementCount()==NumHeights);

	OutTriangleVerts.Empty(Square(CollisionSizeQuads) * 6);

	for( INT Y=0;Y<CollisionSizeQuads;Y++ )
	{
		FLOAT Y0 = (FLOAT)(Y+0) * CollisionScale;
		FLOAT Y1 = (FLOAT)(Y+1) * CollisionScale;
		for( INT X=0;X<CollisionSizeQuads;X++ )
		{
			FLOAT X0 = (FLOAT)(X+0) * CollisionScale;
			FLOAT X1 = (FLOAT)(X+1) * CollisionScale;
			FVector V00 = LocalToWorld.TransformFVector( FVector( X0, Y0, ((FLOAT)Heights[X+0 + (Y+0)*(CollisionSizeQuads+1)] - 32768.f) * LANDSCAPE_ZSCALE) );
			FVector V01 = LocalToWorld.TransformFVector( FVector( X0, Y1, ((FLOAT)Heights[X+0 + (Y+1)*(CollisionSizeQuads+1)] - 32768.f) * LANDSCAPE_ZSCALE) );
			FVector V10 = LocalToWorld.TransformFVector( FVector( X1, Y0, ((FLOAT)Heights[X+1 + (Y+0)*(CollisionSizeQuads+1)] - 32768.f) * LANDSCAPE_ZSCALE) );
			FVector V11 = LocalToWorld.TransformFVector( FVector( X1, Y1, ((FLOAT)Heights[X+1 + (Y+1)*(CollisionSizeQuads+1)] - 32768.f) * LANDSCAPE_ZSCALE) );

			OutTriangleVerts.AddItem(V00);
			OutTriangleVerts.AddItem(V10);
			OutTriangleVerts.AddItem(V11);

			OutTriangleVerts.AddItem(V00);
			OutTriangleVerts.AddItem(V11);
			OutTriangleVerts.AddItem(V01);
		}
	}

	CollisionHeightData.Unlock();
}

void ULandscapeHeightfieldCollisionComponent::ExportCustomProperties(FOutputDevice& Out, UINT Indent)
{
	WORD* Heights = (WORD*)CollisionHeightData.Lock(LOCK_READ_ONLY);
	INT NumHeights = Square(CollisionSizeQuads+1);
	check(CollisionHeightData.GetElementCount()==NumHeights);

	Out.Logf( TEXT("%sCustomProperties CollisionHeightData "), appSpc(Indent));
	for( INT i=0;i<NumHeights;i++ )
	{
		Out.Logf( TEXT("%d "), Heights[i] );
	}

	CollisionHeightData.Unlock();
	Out.Logf( TEXT("\r\n") );

	INT NumDominantLayerSamples = DominantLayerData.GetElementCount();
	check(NumDominantLayerSamples == 0 || NumDominantLayerSamples==NumHeights);

	if( NumDominantLayerSamples > 0 )
	{
		BYTE* DominantLayerSamples = (BYTE*)DominantLayerData.Lock(LOCK_READ_ONLY);

		Out.Logf( TEXT("%sCustomProperties DominantLayerData "), appSpc(Indent));
		for( INT i=0;i<NumDominantLayerSamples;i++ )
		{
			Out.Logf( TEXT("%02x"), DominantLayerSamples[i] );
		}

		DominantLayerData.Unlock();
		Out.Logf( TEXT("\r\n") );
	}
}

void ULandscapeHeightfieldCollisionComponent::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	if(ParseCommand(&SourceText,TEXT("CollisionHeightData")))
	{
		INT NumHeights = Square(CollisionSizeQuads+1);

		CollisionHeightData.Lock(LOCK_READ_WRITE);
		WORD* Heights = (WORD*)CollisionHeightData.Realloc(NumHeights);
		appMemzero(Heights, sizeof(WORD)*NumHeights);

		ParseNext(&SourceText);
		INT i = 0;
		while( appIsDigit(*SourceText) ) 
		{
			if( i < NumHeights )
			{
				Heights[i++] = appAtoi(SourceText);
				while( appIsDigit(*SourceText) ) 
				{
					SourceText++;
				}
			}

			ParseNext(&SourceText);
		} 

		CollisionHeightData.Unlock();

		if( i != NumHeights )
		{
			Warn->Logf( *LocalizeError(TEXT("CustomProperties Syntax Error"), TEXT("Core")));
		}
	}
	else
	if(ParseCommand(&SourceText,TEXT("DominantLayerData")))
	{
		INT NumDominantLayerSamples = Square(CollisionSizeQuads+1);

		DominantLayerData.Lock(LOCK_READ_WRITE);
		BYTE* DominantLayerSamples = (BYTE*)DominantLayerData.Realloc(NumDominantLayerSamples);
		appMemzero(DominantLayerSamples, NumDominantLayerSamples);

		ParseNext(&SourceText);
		INT i = 0;
		while( SourceText[0] && SourceText[1] )
		{
			if( i < NumDominantLayerSamples )
			{
				DominantLayerSamples[i++] = ParseHexDigit(SourceText[0]) * 16 + ParseHexDigit(SourceText[1]);
			}
			SourceText += 2;
		} 

		DominantLayerData.Unlock();

		if( i != NumDominantLayerSamples )
		{
			Warn->Logf( *LocalizeError(TEXT("CustomProperties Syntax Error"), TEXT("Core")));
		}
	}
}

#endif


IMPLEMENT_CLASS(ULandscapeHeightfieldCollisionComponent);
