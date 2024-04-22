/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "UnSkeletalRender.h"
#include "UnTerrain.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#include "UnSoftBodySupport.h"
#endif // WITH_NOVODEX

void USkeletalMeshComponent::InitSoftBodySim(FRBPhysScene* Scene,  UBOOL bRunsInAnimSetViewer)
{
#if WITH_NOVODEX && !NX_DISABLE_SOFTBODY
	// Need a mesh and a scene. Also, do nothing if we already have a sim
	if( !SkeletalMesh || !Scene || SoftBodySim)
	{
		return;
	}

	// Get overall scaling factor
	FVector TotalScale = Scale * Scale3D;
	if (Owner != NULL)
	{
		TotalScale *= Owner->DrawScale3D * Owner->DrawScale;
	}

	if(!TotalScale.IsUniform())
	{
		debugf( TEXT("InitSoftBodySim : Only supported on uniformly scaled meshes.") );
		return;
	}

	
	NxSoftBodyMesh* SoftBodyMesh = SkeletalMesh->GetSoftBodyMeshForScale(TotalScale.X);
	if(SoftBodyMesh)
	{
		NxSoftBodyDesc SoftBodyDesc;
		SoftBodyDesc.softBodyMesh = SoftBodyMesh;

		// Set up buffers for where to put updated mesh data.
		FStaticLODModel* LODModel = &(SkeletalMesh->LODModels(0));

		NxSoftBodyMeshDesc MeshDesc;
		SoftBodyMesh->saveToDesc(MeshDesc);

		InitSoftBodySimBuffers();

		SoftBodyDesc.meshData.verticesPosBegin = SoftBodyTetraPosData.GetData();
		SoftBodyDesc.meshData.verticesPosByteStride = sizeof(FVector);
		SoftBodyDesc.meshData.maxVertices = MeshDesc.numVertices;
		SoftBodyDesc.meshData.numVerticesPtr = (NxU32 *) &NumSoftBodyTetraVerts;

		SoftBodyDesc.meshData.indicesBegin = SoftBodyTetraIndexData.GetData();
		SoftBodyDesc.meshData.indicesByteStride = sizeof(INT);
		SoftBodyDesc.meshData.maxIndices = MeshDesc.numTetrahedra * 4;
		SoftBodyDesc.meshData.numIndicesPtr = (NxU32 *) &NumSoftBodyTetraIndices;
		
		SoftBodyDesc.density = ::Max(0.001f, SkeletalMesh->SoftBodyDensity);

		check(SoftBodyDesc.isValid());

		// @JTODO: Expose this in a helpful way.
		NxGroupsMask SoftBodyMask = CreateGroupsMask(SoftBodyRBChannel, &SoftBodyRBCollideWithChannels);
		SoftBodyDesc.groupsMask = SoftBodyMask;

		// Set position of cloth in space.
		FMatrix SkelMeshCompTM = LocalToWorld;
		SkelMeshCompTM.RemoveScaling();

		SoftBodyDesc.globalPose = U2NTransform(SkelMeshCompTM);

#if !FINAL_RELEASE
		SoftBodyDesc.flags |= NX_SBF_VISUALIZATION;
#endif //!FINAL_RELEASE

		//SoftBodyDesc.flags |= NX_SBF_STATIC;

		// Get the physics scene.
		NxScene* NovodexScene = Scene->GetNovodexPrimaryScene();
		check(NovodexScene);

		if(bSoftBodyUseCompartment)
		{
			SoftBodyDesc.compartment = Scene->GetNovodexSoftBodyCompartment();
			if(IsPhysXHardwarePresent())
			{
				SoftBodyDesc.flags |= NX_SBF_HARDWARE;
			}
		}

		SoftBodyDesc.relativeGridSpacing = SkeletalMesh->SoftBodyRelativeGridSpacing;

		// Actually create cloth object.
		// just in case physics is still running... make sure we stall.
		WaitForNovodexScene(*NovodexScene);
		NxSoftBody* NewSoftBody = NovodexScene->createSoftBody( SoftBodyDesc );

		// Save pointer to cloth object
		SoftBodySim = NewSoftBody;
		SoftBodySceneIndex = Scene->NovodexSceneIndex;

		UpdateSoftBodyParams();
		SetSoftBodyFrozen(bSoftBodyFrozen);

		InitSoftBodyAttachments();

		if(!bSoftBodyAwakeOnStartup && !bRunsInAnimSetViewer)
		{
			NewSoftBody->putToSleep();
		}

		if(bRunsInAnimSetViewer)
		{
			// If we run in the ASV, add a plane to the physics-scene to prevent the soft-body from 
			// falling into oblivion on startup.
			NxBounds3 bounds;
			NewSoftBody->getWorldBounds(bounds);
			
			NxPlaneShapeDesc PlaneShapeDesc;
			PlaneShapeDesc.normal.set(0.0f, 0.0f, 1.0f);
			PlaneShapeDesc.d = bounds.min.z - NewSoftBody->getParticleRadius();
			
			FRBCollisionChannelContainer CollisionChannelContainer(0);
			CollisionChannelContainer.SetChannel(RBCC_SoftBody, TRUE);
			PlaneShapeDesc.groupsMask = CreateGroupsMask(RBCC_Default, &CollisionChannelContainer);		
			
			NxActorDesc ActorDesc;
			ActorDesc.shapes.pushBack(&PlaneShapeDesc);
			
			SoftBodyASVPlane = NovodexScene->createActor(ActorDesc);
		}
	}

#endif //WITH_NOVODEX && !NX_DISABLE_SOFTBODY
}

void USkeletalMeshComponent::InitSoftBodySimBuffers()
{
#if WITH_NOVODEX && !NX_DISABLE_SOFTBODY

	FVector TotalScale = Scale * Scale3D;
	AActor* Owner = GetOwner();
	if(Owner)
	{
		TotalScale *= Owner->DrawScale * Owner->DrawScale3D;
	}

	ScaleSoftBodyTetras(SkeletalMesh->SoftBodyTetraVertsUnscaled, SoftBodyTetraPosData, TotalScale.X);
	SoftBodyTetraIndexData = SkeletalMesh->SoftBodyTetraIndices;

	NumSoftBodyTetraVerts = SoftBodyTetraPosData.Num();
	NumSoftBodyTetraIndices = SoftBodyTetraIndexData.Num();

#endif //WITH_NOVODEX && !NX_DISABLE_SOFTBODY
}

/** Attach breakable soft-body vertex attachment to physics asset. */
void USkeletalMeshComponent::InitSoftBodyAttachments()
{  
#if WITH_NOVODEX && !NX_DISABLE_CLOTH

	if((SkeletalMesh == NULL) || (PhysicsAsset == NULL) || (PhysicsAssetInstance == NULL))
	{
		return;
	}

	const TArray<FSoftBodySpecialBoneInfo>& SpecialBones = SkeletalMesh->SoftBodySpecialBones;

	if((SoftBodySim == NULL) || (SpecialBones.Num() <= 0))
	{
		return;
	}

	NxSoftBody* nSoftBody = (NxSoftBody *)SoftBodySim;
	TArray<FSoftBodyTetraLink>& TetraLinks = SkeletalMesh->SoftBodyTetraLinks;
	TArray<INT>& SoftBodyTetraIndices = SkeletalMesh->SoftBodyTetraIndices;
	INT NumSoftBodyVerts = nSoftBody->getNumberOfParticles();
	NxVec3* nSoftBodyPositions = (NxVec3 *)appMalloc(sizeof(NxVec3) * NumSoftBodyVerts);

	//get the positions directly from the particles as we have not simulated yet(and the mesh data is not valid).
	nSoftBody->getPositions(nSoftBodyPositions, sizeof(NxVec3));

	for(INT i=0; i<SpecialBones.Num(); i++)
	{
		INT BoneIndex = MatchRefBone(SpecialBones(i).BoneName);

		//attach to the bones actor in the PhysicsAsset.
		if(BoneIndex == INDEX_NONE)
		{
			continue;
		}
		
		check(BoneIndex < 255);
		
		INT BodyIndex = PhysicsAsset->FindControllingBodyIndex(SkeletalMesh, BoneIndex);
		if(BodyIndex == INDEX_NONE)
		{
			continue;
		}

		NxActor* nActor = PhysicsAssetInstance->Bodies(BodyIndex)->GetNxActor();

		if(nActor == NULL || (nActor->getNbShapes() <= 0))
		{
			continue;
		}

		NxU32 AttachFlags = 0;

		switch(SpecialBones(i).BoneType)
		{
		case SOFTBODYBONE_Fixed:
			break;
		case SOFTBODYBONE_BreakableAttachment:
			AttachFlags |= NX_SOFTBODY_ATTACHMENT_TEARABLE;
			break;
		case SOFTBODYBONE_TwoWayAttachment:
			AttachFlags |= NX_SOFTBODY_ATTACHMENT_TWOWAY;
			break;
		default:
			break;
		}

		for(INT j=0; j<SpecialBones(i).AttachedVertexIndices.Num(); j++)
		{
			INT Idx = SpecialBones(i).AttachedVertexIndices(j);
			INT LinkIdx = TetraLinks(Idx).TetIndex;
			FVector LinkBary = TetraLinks(Idx).Bary;

			INT TetPtIdx[4];
			FVector TetPt[4];
			
			TetPtIdx[0] = SoftBodyTetraIndices(LinkIdx + 0);
			TetPtIdx[1] = SoftBodyTetraIndices(LinkIdx + 1);
			TetPtIdx[2] = SoftBodyTetraIndices(LinkIdx + 2);
			TetPtIdx[3] = SoftBodyTetraIndices(LinkIdx + 3);

			TetPt[0] = N2UVectorCopy(nSoftBodyPositions[TetPtIdx[0]]);
			TetPt[1] = N2UVectorCopy(nSoftBodyPositions[TetPtIdx[1]]);
			TetPt[2] = N2UVectorCopy(nSoftBodyPositions[TetPtIdx[2]]);
			TetPt[3] = N2UVectorCopy(nSoftBodyPositions[TetPtIdx[3]]);

			//find the graphics vertex position

			FVector GraphPos = 
					TetPt[0] * LinkBary.X + 
					TetPt[1] * LinkBary.Y + 
					TetPt[2] * LinkBary.Z + 
					TetPt[3] * (1.0f - LinkBary.X - LinkBary.Y - LinkBary.Z);

			for(INT k=0; k<4; k++)
			{
				FVector TetPtPos = TetPt[k];
				NxVec3 nTetPtPos = U2NVectorCopy(TetPt[k]);


				if((TetPtPos - GraphPos).Size() > SkeletalMesh->SoftBodyAttachmentThreshold)
				{
					continue;
				}

				NxShape* nShape = nActor->getShapes()[0];
				NxMat34 nShapePose = nShape->getGlobalPose();

				NxVec3 nLocalPos;
				nShapePose.multiplyByInverseRT(nTetPtPos, nLocalPos);

				// just in case physics is still running... make sure we stall.
				NxScene &nScene = nSoftBody->getScene();
				WaitForNovodexScene(nScene);

				nSoftBody->attachVertexToShape(TetPtIdx[k], nShape, nLocalPos, AttachFlags);
			}
		}
	}

	appFree(nSoftBodyPositions);
	nSoftBodyPositions = NULL;

#endif // WITH_NOVODEX && !NX_DISABLE_CLOTH
}
/** Stop cloth simulation and clean up simulation objects. */
void USkeletalMeshComponent::TermSoftBodySim(FRBPhysScene* Scene)
{
#if WITH_NOVODEX && !NX_DISABLE_SOFTBODY
	// Nothing to do if no simulation object
	if(!SoftBodySim)
	{
		return;
	}

	// If in right scene, or no scene supplied, clean up.
	if(Scene == NULL || SoftBodySceneIndex == Scene->NovodexSceneIndex)
	{
		// This will return NULL if this scene has already been shut down. 
		// If it has - do nothing - softbody will have been cleaned up with the scene.
		NxScene* NovodexScene = GetNovodexPrimarySceneFromIndex(SoftBodySceneIndex);
		if(NovodexScene)
		{
			NxSoftBody* nSoftBody = (NxSoftBody*)SoftBodySim;
			#if SUPPORT_DOUBLE_BUFFERING
			if (NovodexScene->isWritable())
			{
				NovodexScene->releaseSoftBody(*nSoftBody);
			}
			else 
			{
				appErrorf(TEXT("TRYING TO TERM SOFT BODY SIM WHILE SCENE IS RUNNING!"));
				//GNovodexPendingKillSoftBody.AddItem(nSoftBody);
			}
			#else
			NovodexScene->releaseSoftBody(*nSoftBody);
			#endif
			SoftBodySim = NULL;

			if(SoftBodyASVPlane)
			{
				NxActor* PlaneActor = (NxActor*)SoftBodyASVPlane;
				NovodexScene->releaseActor(*PlaneActor);
				SoftBodyASVPlane = NULL;
			}		
		}
	}
#endif
}

/** Update params of the this components internal soft-body sim from the SkeletalMesh properties. */
void USkeletalMeshComponent::UpdateSoftBodyParams()
{
#if WITH_NOVODEX && !NX_DISABLE_SOFTBODY
	if(!SoftBodySim || !SkeletalMesh)
	{
		return;
	}

	NxSoftBody* nSoftBody = (NxSoftBody*)SoftBodySim;

	nSoftBody->setVolumeStiffness( SkeletalMesh->SoftBodyVolumeStiffness );
	nSoftBody->setStretchingStiffness( SkeletalMesh->SoftBodyStretchingStiffness );
	nSoftBody->setParticleRadius( SkeletalMesh->SoftBodyParticleRadius );
	nSoftBody->setDampingCoefficient( SkeletalMesh->SoftBodyDamping );
	nSoftBody->setFriction( SkeletalMesh->SoftBodyFriction );
	nSoftBody->setSleepLinearVelocity( SkeletalMesh->SoftBodySleepLinearVelocity );
	nSoftBody->setSolverIterations( SkeletalMesh->SoftBodySolverIterations );
	nSoftBody->setAttachmentResponseCoefficient( SkeletalMesh->SoftBodyAttachmentResponse );
	nSoftBody->setCollisionResponseCoefficient( SkeletalMesh->SoftBodyCollisionResponse );
	nSoftBody->setAttachmentTearFactor( SkeletalMesh->SoftBodyAttachmentTearFactor );

	NxU32 Flags = nSoftBody->getFlags();

	if( SkeletalMesh->bEnableSoftBodyDamping )
	{
		Flags |= NX_SBF_DAMPING;
	}
	else
	{
		Flags &= ~NX_SBF_DAMPING;
	}

	if( SkeletalMesh->bUseSoftBodyCOMDamping )
	{
		Flags |= NX_SBF_COMDAMPING;
	}
	else
	{
		Flags &= ~NX_SBF_COMDAMPING;
	}

	if( SkeletalMesh->bEnableSoftBodyTwoWayCollision )
	{
		Flags |= NX_SBF_COLLISION_TWOWAY;
	}
	else
	{
		Flags &= ~NX_SBF_COLLISION_TWOWAY;
	}

	if( SkeletalMesh->bEnableSoftBodyTwoWayCollision )
	{
		Flags |= NX_SBF_SELFCOLLISION;
	}
	else
	{
		Flags &= ~NX_SBF_SELFCOLLISION;
	}

	nSoftBody->setFlags(Flags);

#endif
}

/** Script version of UpdateClothParams. */
void USkeletalMeshComponent::execUpdateSoftBodyParams( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	UpdateSoftBodyParams();
}

/** Toggle active simulation of SoftBodys. */
void USkeletalMeshComponent::SetSoftBodyFrozen(UBOOL bNewFrozen)
{
	/*
	// When unfreezing SoftBodys, first teleport vertices to ref pose
	if(bSoftBodyFrozen && !bNewFrozen)
	{
	//TODO
		ResetClothVertsToRefPose();
	}*/

	bSoftBodyFrozen = bNewFrozen;

#if WITH_NOVODEX && !NX_DISABLE_SOFTBODY
	if(SoftBodySim)
	{
		NxSoftBody* nSoftBody = (NxSoftBody*)SoftBodySim;
		NxU32 Flags = nSoftBody->getFlags();

		if(bSoftBodyFrozen)
		{
			Flags |= NX_SBF_STATIC;
		}
		else
		{
			Flags &= ~NX_SBF_STATIC;
		}

		nSoftBody->setFlags(Flags);
	}
#endif
}

/** Script version of SetSoftBodyFrozen. */
void USkeletalMeshComponent::execSetSoftBodyFrozen( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(bNewFrozen);
	P_FINISH;

	SetSoftBodyFrozen(bNewFrozen);
}

/** Force awake any soft body simulation on this component */
void USkeletalMeshComponent::WakeSoftBody()
{
	// Force any soft body physics awake
#if WITH_NOVODEX && !NX_DISABLE_SOFTBODY
	if(SoftBodySim && bEnableSoftBodySimulation)
	{
		NxSoftBody* SoftBody = (NxSoftBody *)SoftBodySim;
		SoftBody->wakeUp();
	}
#endif
}

/** Script version of WakeSoftBody. */
void USkeletalMeshComponent::execWakeSoftBody( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	WakeSoftBody();
}
