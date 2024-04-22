/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "EnginePrivate.h"
#include "EngineClasses.h"
#include "EngineMeshClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineParticleClasses.h"
#include "EngineProcBuildingClasses.h"


#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif

#include "UserForceField.h"
#include "UserForceFieldLinearKernel.h"
#include "UserForceFieldShapeGroup.h"
#include "PrecomputedLightVolume.h"

#if WITH_APEX
#include <NxModuleClothing.h>
#include <NxModuleDestructible.h>
#include "NvApexManager.h"
#include "NvApexCommands.h"
#include "NvApexScene.h"
#include "PVDBinding.h"
#endif

#define FORCE_RRB_OFF				1



#if WITH_APEX
#include "NvApexRender.h"
#endif

// toggles pools of RB constraint,body instances
#ifndef DISABLE_POOLED_RB_INSTANCES
#define DISABLE_POOLED_RB_INSTANCES 0
#endif

/** Physics stats */
DECLARE_STATS_GROUP(TEXT("Physics"),STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Phys Events Time"),STAT_PhysicsEventTime,STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Phys Wait Time"),STAT_PhysicsWaitTime,STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Substep Time"),STAT_RBSubstepTime,STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Solver Time"),STAT_RBSolver,STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Nearphase Time"),STAT_RBNearphase,STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Broadphase Update Time"),STAT_RBBroadphaseUpdate,STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Broadphase GetPairs Time"),STAT_RBBroadphaseGetPairs,STATGROUP_Physics);

DECLARE_CYCLE_STAT(TEXT("Fluid Mesh Emitter Time"),STAT_PhysicsFluidMeshEmitterUpdate,STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Physics Stats Time"),STAT_PhysicsOutputStats,STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("GRB Including gfx Wait Time"),STAT_HWTimeIncludingGpuWait,STATGROUP_Physics);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Substeps"),STAT_NumSubsteps,STATGROUP_Physics);

DECLARE_DWORD_COUNTER_STAT(TEXT("Total SW Dynamic Bodies"),STAT_TotalSWDynamicBodies,STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Awake SW Dynamic Bodies"),STAT_AwakeSWDynamicBodies,STATGROUP_Physics);

DECLARE_DWORD_COUNTER_STAT(TEXT("Solver Bodies"),STAT_SWSolverBodies,STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Pairs"),STAT_SWNumPairs,STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Contacts"),STAT_SWNumContacts,STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Joints"),STAT_SWNumJoints,STATGROUP_Physics);

DECLARE_DWORD_COUNTER_STAT(TEXT("GRB Solver Bodies"),STAT_HWSolverBodies,STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("GRB Num Pairs"),STAT_HWNumPairs,STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("GRB Num Contacts"),STAT_HWNumContacts,STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("GRB Convex Meshes"),STAT_HWNumConvexMeshes,STATGROUP_Physics);

DECLARE_FLOAT_COUNTER_STAT(TEXT("GRB Scene Memory (MB)"),STAT_HWSceneMemoryUsed,STATGROUP_Physics);
DECLARE_FLOAT_COUNTER_STAT(TEXT("GRB Temp Memory (MB)"),STAT_HWTempMemoryUsed,STATGROUP_Physics);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num ConvexMesh"),STAT_NumConvexMeshes,STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num TriMesh"),STAT_NumTriMeshes,STATGROUP_Physics);

DECLARE_MEMORY_STAT(TEXT("Novodex Allocation Size"),STAT_NovodexTotalAllocationSize,STATGROUP_Physics);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Novodex Allocation Count"),STAT_NovodexNumAllocations,STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Novodex Allocator Time"),STAT_NovodexAllocatorTime,STATGROUP_Physics);

DECLARE_CYCLE_STAT(TEXT("Total Dynamics Time"),STAT_RBTotalDynamicsTime,STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Start Physics Time"),STAT_PhysicsKickOffDynamicsTime,STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Fetch Results Time"),STAT_PhysicsFetchDynamicsTime,STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("SkelMesh_SetMeshTime"),STAT_SkelMesh_SetMeshTime,STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("SkelMesh_SetPhysAssetTime"),STAT_SkelMesh_SetPhysAssetTime,STATGROUP_Physics);

/** Physics Fluid Stats */
DECLARE_STATS_GROUP(TEXT("PhysicsFluid"),STATGROUP_PhysicsFluid);

DECLARE_DWORD_COUNTER_STAT(TEXT("Total Fluids"),STAT_TotalFluids,STATGROUP_PhysicsFluid);
DECLARE_DWORD_COUNTER_STAT(TEXT("Total Fluid Emitters"),STAT_TotalFluidEmitters,STATGROUP_PhysicsFluid);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Fluid Particles"),STAT_ActiveFluidParticles,STATGROUP_PhysicsFluid);
DECLARE_DWORD_COUNTER_STAT(TEXT("Total Fluid Particles"),STAT_TotalFluidParticles,STATGROUP_PhysicsFluid);
DECLARE_DWORD_COUNTER_STAT(TEXT("Total Fluid Packets"),STAT_TotalFluidPackets,STATGROUP_PhysicsFluid);
DECLARE_CYCLE_STAT(TEXT("PhysXEmitterVertical Sync Time"),STAT_PhysXEmitterVerticalSync,STATGROUP_PhysicsFluid);
DECLARE_CYCLE_STAT(TEXT("PhysXEmitterVertical Tick Time"),STAT_PhysXEmitterVerticalTick,STATGROUP_PhysicsFluid);


/** Physics Cloth Stats */
DECLARE_STATS_GROUP(TEXT("PhysicsCloth"),STATGROUP_PhysicsCloth);

DECLARE_DWORD_COUNTER_STAT(TEXT("Total Cloths"),STAT_TotalCloths,STATGROUP_PhysicsCloth);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Cloths"),STAT_ActiveCloths,STATGROUP_PhysicsCloth);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Cloth Vertices"),STAT_ActiveClothVertices,STATGROUP_PhysicsCloth);
DECLARE_DWORD_COUNTER_STAT(TEXT("Total Cloth Vertices"),STAT_TotalClothVertices,STATGROUP_PhysicsCloth);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Attached Cloth Vertices"),STAT_ActiveAttachedClothVertices,STATGROUP_PhysicsCloth);
DECLARE_DWORD_COUNTER_STAT(TEXT("Total Attached Cloth Vertices"),STAT_TotalAttachedClothVertices,STATGROUP_PhysicsCloth);

/** Physics GPU Memory Stats */
DECLARE_STATS_GROUP(TEXT("PhysicsGpuMem"),STATGROUP_PhysicsGpuMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("Total"),STAT_PhysicsGpuMemTotal,STATGROUP_PhysicsGpuMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("Fluid"),STAT_PhysicsGpuMemFluid,STATGROUP_PhysicsGpuMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("Cloth/Soft Body"),STAT_PhysicsGpuMemClothSoftBody,STATGROUP_PhysicsGpuMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("Shared"),STAT_PhysicsGpuMemShared,STATGROUP_PhysicsGpuMem);


/** If set keeps track of memory routed through this allocator. */
#ifndef KEEP_TRACK_OF_NOVODEX_ALLOCATIONS
//#define KEEP_TRACK_OF_NOVODEX_ALLOCATIONS STATS
#define KEEP_TRACK_OF_NOVODEX_ALLOCATIONS 0
#endif


#if WITH_NOVODEX
#include "PhysXVerticalEmitter.h"

// On PC, add support for dumping scenes to XML.
//#if !CONSOLE
//#  include "NXU_helper.h"
//#endif

// This is moved to UnNovodexSupport.h
// #if SUPPORT_DOUBLE_BUFFERING
// #include "NxdScene.h"
// #endif

#if PS3
	#include "PS3/NxCellConfiguration.h"
#endif

#endif // WITH_NOVODEX


UBOOL FCollisionNotifyInfo::IsValidForNotify() const
{
	if(Info0.Actor && Info0.Actor->bDeleteMe)
	{
		return FALSE;
	}

	if(Info1.Actor && Info1.Actor->bDeleteMe)
	{
		return FALSE;
	}

	return TRUE;
}

/** Iterate over ContactInfos array and swap order of information */
void FCollisionImpactData::SwapContactOrders()
{
	for(INT i=0; i<ContactInfos.Num(); i++)
	{
		ContactInfos(i).SwapOrder();
	}
}

/** Swap the order of info in this info  */
void FRigidBodyContactInfo::SwapOrder()
{
	Swap(ContactVelocity[0], ContactVelocity[1]);
	Swap(PhysMaterial[0], PhysMaterial[1]);
	ContactNormal = -ContactNormal;
}

#if WITH_NOVODEX
static INT NxDumpIndex = -1;
static UBOOL bOutputAllStats = FALSE;

#if SUPPORT_DOUBLE_BUFFERING
static FNxdDoubleBufferReport nDoubleBufferReportObject;
#endif
static FNxContactReport		nContactReportObject;
static FNxNotify			nNotifyObject;
static FNxModifyContact		nContactModifyObject;


void FRBPhysScene::AddNovodexDebugLines(ULineBatchComponent* LineBatcherToUse)
{
	if(LineBatcherToUse)
	{
		NxScene* NovodexScene = GetNovodexPrimaryScene();
		const NxDebugRenderable* DebugData = NULL;
		INT DebugLineSource = 0;
		for(;;)
		{
			switch( DebugLineSource++ )
			{
			case 0: DebugData = NovodexScene ? NovodexScene->getDebugRenderable() : NULL; break;
#if WITH_APEX
			case 1: DebugData = ApexScene ? ApexScene->GetDebugRenderable() : NULL; break;
#endif
			default: return;
			}

			if(DebugData)
			{
				INT NumPoints = DebugData->getNbPoints();
				if(NumPoints > 0)
				{
					const NxDebugPoint* Points = DebugData->getPoints();
					for(INT i=0; i<NumPoints; i++)
					{
						DrawWireStar( LineBatcherToUse, N2UPosition(Points->p), 2.f, FColor((DWORD)Points->color), SDPG_World );
						Points++;
					}
				}

				// Build a list of all the lines we want to draw
				TArray<ULineBatchComponent::FLine> DebugLines;

				// Add all the 'lines' from Novodex
				INT NumLines = DebugData->getNbLines();
				if (NumLines > 0)
				{
					const NxDebugLine* Lines = DebugData->getLines();
					for(INT i = 0; i<NumLines; i++)
					{
						new(DebugLines) ULineBatchComponent::FLine(N2UPosition(Lines->p0), N2UPosition(Lines->p1), FColor((DWORD)Lines->color), 0.f, 0.0f, SDPG_World);
						Lines++;
					}
				}

				// Add all the 'triangles' from Novodex
				INT NumTris = DebugData->getNbTriangles();
				if(NumTris > 0)
				{
					const NxDebugTriangle* Triangles = DebugData->getTriangles();
					for(INT i=0; i<NumTris; i++)
					{
						new(DebugLines) ULineBatchComponent::FLine(N2UPosition(Triangles->p0), N2UPosition(Triangles->p1), FColor((DWORD)Triangles->color), 0.f, 0.0f,  SDPG_World);
						new(DebugLines) ULineBatchComponent::FLine(N2UPosition(Triangles->p1), N2UPosition(Triangles->p2), FColor((DWORD)Triangles->color), 0.f, 0.0f,  SDPG_World);
						new(DebugLines) ULineBatchComponent::FLine(N2UPosition(Triangles->p2), N2UPosition(Triangles->p0), FColor((DWORD)Triangles->color), 0.f, 0.0f,  SDPG_World);
						Triangles++;
					}
				}

				// Draw them all in one call.
				if( DebugLines.Num() > 0 )
				{
					LineBatcherToUse->DrawLines(DebugLines);
				}
			}
		}
	}
}

/** Called by Novodex when a constraint is broken. From here we call SeqAct_ConstraintBroken events. */
bool FNxNotify::onJointBreak(NxReal breakingForce, NxJoint& brokenJoint)
{
	URB_ConstraintInstance* Inst = (URB_ConstraintInstance*)(brokenJoint.userData);

	// Fire any events associated with this constraint actor.
	if(Inst && Inst->Owner)
	{
		for(INT Idx = 0; Idx < Inst->Owner->GeneratedEvents.Num(); Idx++)
		{
			USeqEvent_ConstraintBroken *BreakEvent = Cast<USeqEvent_ConstraintBroken>(Inst->Owner->GeneratedEvents(Idx));
			if (BreakEvent != NULL)
			{
				BreakEvent->CheckActivate(Inst->Owner, Inst->Owner);
			}
		}

		URB_ConstraintSetup* Setup = NULL;

		// Two cases supported here for finding the URB_ConstraintSetup
		USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Inst->OwnerComponent);
		ARB_ConstraintActor* ConAct = Cast<ARB_ConstraintActor>(Inst->Owner);

		// One is the case of a constraint Actor in  the level - just look to the RB_ConstraintActor
		if(ConAct)
		{
			check(ConAct->ConstraintInstance == Inst);
			Setup = ConAct->ConstraintSetup;
		}
		else if(SkelComp)
		{
			check(SkelComp->PhysicsAssetInstance);
			check(SkelComp->PhysicsAsset);
			check(SkelComp->PhysicsAssetInstance->Constraints.Num() == SkelComp->PhysicsAsset->ConstraintSetup.Num());
			check(Inst->ConstraintIndex < SkelComp->PhysicsAsset->ConstraintSetup.Num());
			Setup = SkelComp->PhysicsAsset->ConstraintSetup(Inst->ConstraintIndex);
		}

		// maybe the constraint name  (add to TTP)
		const FVector& ConstrainLocation = Inst->GetConstraintLocation();
		Inst->Owner->eventConstraintBrokenNotify( Inst->Owner, Setup, Inst );
	}

	// We still hold references to this joint, so do not want it released, so we return false here.
	return false;
}

// BSP triangle gathering.
struct FBSPTriIndices
{
	INT v0, v1, v2;
};

static void GatherBspTrisRecursive( UModel* model, INT nodeIndex, TArray<FBSPTriIndices>& tris, TArray<NxMaterialIndex>& materials )
{
	check(GEngine->DefaultPhysMaterial);

	while(nodeIndex != INDEX_NONE)
	{
		FBspNode* curBspNode   = &model->Nodes( nodeIndex );

		INT planeNodeIndex = nodeIndex;
		while(planeNodeIndex != INDEX_NONE)
		{
			FBspNode* curPlaneNode = &model->Nodes( planeNodeIndex );
			FBspSurf* curSurf = &model->Surfs( curPlaneNode->iSurf );

			UPhysicalMaterial* PhysMat = GEngine->DefaultPhysMaterial;
			if(curSurf->Material && curSurf->Material->GetPhysicalMaterial())
			{
				PhysMat = curSurf->Material->GetPhysicalMaterial();
			}

			int vertexOffset = curPlaneNode->iVertPool;

			if( (curPlaneNode->NumVertices > 0) && !(curSurf->PolyFlags & PF_NotSolid)) /* If there are any triangles to add. */
			{
				for( int i = 2; i < curPlaneNode->NumVertices; i++ )
				{
					// Verts, indices added as a fan from v0
					FBSPTriIndices ti;
					ti.v0 = model->Verts( vertexOffset ).pVertex;
					ti.v1 = model->Verts( vertexOffset + i - 1 ).pVertex;
					ti.v2 = model->Verts( vertexOffset + i ).pVertex;

					tris.AddItem( ti );
					materials.AddItem( 0 );
				}
			}

			planeNodeIndex = curPlaneNode->iPlane;
		}

		// recurse back and iterate to front.
		if( curBspNode->iBack != INDEX_NONE )
		{
			GatherBspTrisRecursive(model, curBspNode->iBack, tris, materials );
		}

		nodeIndex = curBspNode->iFront;
	}
}
#endif // WITH_NOVODEX

void ULevel::BuildPhysBSPData()
{
#if WITH_NOVODEX && WITH_PHYSX_COOKING
	if(Model->Nodes.Num() > 0)
	{
		if(!bUseConvexBSP)
		{
			// Generate a vertex buffer for all BSP verts at physics scale.
			TArray<FVector> PhysScaleTriVerts;
			PhysScaleTriVerts.Add( Model->Points.Num() );
			for(INT i=0; i<Model->Points.Num(); i++)
			{
				PhysScaleTriVerts(i) = Model->Points(i) * U2PScale;
			}

			// Harvest an overall index buffer for the level BSP.
			TArray<FBSPTriIndices> TriInidices;
			TArray<NxMaterialIndex> MaterialIndices;
			GatherBspTrisRecursive( Model, 0, TriInidices, MaterialIndices );
			check(TriInidices.Num() == MaterialIndices.Num());

			// Then create Novodex descriptor
			NxTriangleMeshDesc LevelBSPDesc;

			LevelBSPDesc.numVertices = PhysScaleTriVerts.Num();
			LevelBSPDesc.pointStrideBytes = sizeof(FVector);
			LevelBSPDesc.points = PhysScaleTriVerts.GetData();

			LevelBSPDesc.numTriangles = TriInidices.Num();
			LevelBSPDesc.triangleStrideBytes = sizeof(FBSPTriIndices);
			LevelBSPDesc.triangles = TriInidices.GetData();

			//LevelBSPDesc.materialIndexStride = sizeof(NxMaterialIndex);
			//LevelBSPDesc.materialIndices = MaterialIndices.GetData();

			LevelBSPDesc.flags = 0;

			// Clear out the cached data.
			CachedPhysBSPData.Empty();
			CachedPhysConvexBSPData.CachedConvexElements.Empty();

			FNxMemoryBuffer Buffer(&CachedPhysBSPData);
			if( GNovodexCooking->NxGetCookingParams().targetPlatform == PLATFORM_PC )
			{
				LevelBSPDesc.flags |= NX_MF_HARDWARE_MESH;
			}
			GNovodexCooking->NxCookTriangleMesh(LevelBSPDesc, Buffer);

			// Log cooked physics size.
			debugf( TEXT("COOKEDPHYSICS: BSP %3.2f KB"), ((FLOAT)CachedPhysBSPData.Num())/1024.f );

			// Update cooked data version number.
			CachedPhysBSPDataVersion = GCurrentCachedPhysDataVersion;
		}
		else
		{
			// Assign scale for model.
			FVector TotalScale3D = (FVector)1.0f;

			// Convert collision model into convex hulls.
			ConvexBSPAggGeom.~FKAggregateGeom();
			appMemzero(&ConvexBSPAggGeom,sizeof(ConvexBSPAggGeom));
			KModelToHulls( &ConvexBSPAggGeom, Model );

			// Clear out the cached data.
			CachedPhysBSPData.Empty();
			CachedPhysConvexBSPData.CachedConvexElements.Empty();

			// Do not cache PhysX geometry for volumes that don't need it
			MakeCachedConvexDataForAggGeom(&CachedPhysConvexBSPData, ConvexBSPAggGeom.ConvexElems, TotalScale3D, *GetName() );

			// Add to memory used total.
			INT HullCount=0;
			INT HullByteCount=0;
			for(INT HullIdx = 0; HullIdx < CachedPhysConvexBSPData.CachedConvexElements.Num(); HullIdx++)
			{
				FKCachedConvexDataElement& Hull = CachedPhysConvexBSPData.CachedConvexElements(HullIdx);
				HullByteCount += Hull.ConvexElementData.Num();
				HullCount++;
			}

			// Log cooked physics size.
			debugf( TEXT("COOKEDPHYSICS: CONVEX BSP %d Hulls (%3.2f KB)"), HullCount, ((FLOAT)HullByteCount)/1024.f);

			// Update cached data version
			CachedPhysConvexBSPVersion = GCurrentCachedPhysDataVersion;
		}
	}
#endif // WITH_NOVODEX && WITH_PHYSX_COOKING
}

void ULevel::InitLevelBSPPhysMesh()
{
#if WITH_NOVODEX
	if(!bUseConvexBSP)
	{
		// Do nothing if we already have an Actor for the level.
		// Just checking software version - that should always be around.
		if(LevelBSPActor)
		{
			return;
		}

		// If we have no physics mesh created yet - do it now
		// Again, just check software version of mesh
		if(!LevelBSPPhysMesh)
		{
			// Create the actor representing the level BSP. Do nothing if no BSP nodes.
			if( GWorld->RBPhysScene && Model->Nodes.Num() > 0 )
			{
				// If we don't have any cached data, or its out of date - cook it now and warn.
				if( CachedPhysBSPData.Num() == 0 || 
					CachedPhysBSPDataVersion != GCurrentCachedPhysDataVersion || 
					!bUsePrecookedPhysData)
				{
					debugf( TEXT("No Cached BSP Physics Data Found Or Out Of Date - Cooking Now.") );
					BuildPhysBSPData();
				}

#if XBOX || PS3
				check( GetCookedPhysDataEndianess(CachedPhysBSPData) != CPDE_LittleEndian );
#endif

				// Still may be no physics data if no structural brushes were present.
				if ( CachedPhysBSPData.Num() > 0 )
				{
					FNxMemoryBuffer Buffer(&CachedPhysBSPData);
					LevelBSPPhysMesh = GNovodexSDK->createTriangleMesh(Buffer);
					SetNxTriMeshRefCount(LevelBSPPhysMesh, DelayNxMeshDestruction);
					GNumPhysXTriMeshes++;
				}
			}

			// We don't need the cached physics data any more, so clear it
			CachedPhysBSPData.Empty();
		}

		// If we have a physics mesh - create NxActor.
		// Just check software version - should always be around.
		if(LevelBSPPhysMesh)
		{
			check(GEngine->DefaultPhysMaterial);

			NxTriangleMeshShapeDesc LevelBSPShapeDesc;
			LevelBSPShapeDesc.meshData = LevelBSPPhysMesh;
			LevelBSPShapeDesc.meshFlags = 0;
			LevelBSPShapeDesc.materialIndex = GWorld->RBPhysScene->FindPhysMaterialIndex( GEngine->DefaultPhysMaterial );
			LevelBSPShapeDesc.groupsMask = CreateGroupsMask(RBCC_Default, NULL);

			// Only use Mesh Paging on HW RB compartments
			FRBPhysScene* UseScene = GWorld->RBPhysScene;
			NxCompartment *RBCompartment = UseScene->GetNovodexRigidBodyCompartment();
			if(RBCompartment && RBCompartment->getDeviceCode() != NX_DC_CPU)
			{
				LevelBSPShapeDesc.meshPagingMode = NX_MESH_PAGING_AUTO;
			}

			// Create actor description and instance it.
			NxActorDesc LevelBSPActorDesc;
			LevelBSPActorDesc.shapes.pushBack(&LevelBSPShapeDesc);

			NxScene* NovodexScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
			check(NovodexScene);

			LevelBSPActor = NovodexScene->createActor(LevelBSPActorDesc);

			if( LevelBSPActor )
			{
				// No BodyInstance here.
				LevelBSPActor->userData = NULL;
			}
			else
			{
				// Log failure.
				debugf(TEXT("Couldn't create Novodex BSP actor"));
			}

			// Remember scene index.
			SceneIndex = GWorld->RBPhysScene->NovodexSceneIndex;
		}
	}
	else
	{
		// Do nothing if we already have an Actor for the level.
		// Just checking software version - that should always be around.
		if(LevelConvexBSPActor)
		{
			return;
		}

		// If we have no physics mesh created yet - do it now
		// Again, just check software version of mesh
		if(!LevelConvexBSPActor)
		{
			// Create the actor representing the level BSP. Do nothing if no BSP nodes.
			if( GWorld->RBPhysScene && Model->Nodes.Num() > 0 )
			{
				// If we don't have any cached data, or its out of date - cook it now and warn.
				if( CachedPhysConvexBSPData.CachedConvexElements.Num() == 0 || 
					CachedPhysConvexBSPVersion != GCurrentCachedPhysDataVersion || 
					!bUsePrecookedPhysData)
				{
					debugf( TEXT("No Cached Convex BSP Physics Data Found Or Out Of Date - Cooking Now.") );
					BuildPhysBSPData();
				}

#if XBOX || PS3
				//check( GetCookedPhysDataEndianess(CachedPhysConvexBSPData) != CPDE_LittleEndian );
#endif

				// Only continue if we got some valid hulls for this model.
				if(CachedPhysConvexBSPData.CachedConvexElements.Num() > 0)
				{
					NxActorDesc* ConvexBSPPhysDesc;
					FVector TotalScale3D = (FVector)1.0f;
					ConvexBSPPhysDesc = ConvexBSPAggGeom.InstanceNovodexGeom( TotalScale3D, &CachedPhysConvexBSPData, FALSE, *GetFullName() );

					// We don't need the cached physics data any more, so clear it
					CachedPhysConvexBSPData.CachedConvexElements.Empty();

					// Make transform for this static mesh component
					FMatrix CompTM = FMatrix::Identity;;
					CompTM.RemoveScaling();
					NxMat34 nCompTM = U2NTransform(CompTM);

					// Create actor description and instance it.
					NxActorDesc ConvexBSPActorDesc;
					ConvexBSPActorDesc.globalPose = nCompTM;
					ConvexBSPActorDesc.density = 1.f;

					// Get the physical material to use for the model.
					check(GEngine->DefaultPhysMaterial);
					UPhysicalMaterial* PhysMat = GEngine->DefaultPhysMaterial;
					NxMaterialIndex MatIndex = GWorld->RBPhysScene->FindPhysMaterialIndex( PhysMat );

					// Set to special group if its a water volume.
					NxGroupsMask GroupsMask = CreateGroupsMask(RBCC_Default, NULL);

					// Use the shapes descriptions from the cached actor desc.
					for(UINT i=0; i<ConvexBSPPhysDesc->shapes.size(); i++)
					{
						ConvexBSPActorDesc.shapes.push_back( ConvexBSPPhysDesc->shapes[i] );

						// Set the material to the one specified in the PhysicalMaterial before creating this NxActor instance.
						ConvexBSPActorDesc.shapes[i]->materialIndex = MatIndex;

						// Assign collision group to each shape.
						ConvexBSPActorDesc.shapes[i]->groupsMask = GroupsMask;
					}

					// Create the actual NxActor using the mesh collision shape.
					NxScene* NovodexScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
					check(NovodexScene);

					LevelConvexBSPActor = NovodexScene->createActor(ConvexBSPActorDesc);

					if(LevelConvexBSPActor)
					{
						LevelConvexBSPActor->userData = NULL;
					}
					else
					{
						debugf(TEXT("Couldn't create Novodex BSP Convex actor"));
					}
				}
			}
		}
	}
#endif // WITH_NOVODEX
}

#if PERF_SHOW_PHYS_INIT_COSTS
DOUBLE TotalInstanceGeomTime;
DOUBLE TotalCreateActorTime;
DOUBLE TotalTerrainTime;
DOUBLE TotalPerTriStaticMeshTime;
DOUBLE TotalInitArticulatedTime;
DOUBLE TotalConstructBodyInstanceTime;
DOUBLE TotalInitBodyTime;
INT TotalConvexGeomCount;
INT TotalFramesRun;
#endif

/** Reset stats used for seeing where time goes initializing physics. */
void ULevel::ResetInitRBPhysStats()
{
#if PERF_SHOW_PHYS_INIT_COSTS
	TotalInstanceGeomTime = 0;
	TotalCreateActorTime = 0;
	TotalTerrainTime = 0;
	TotalPerTriStaticMeshTime = 0;
	TotalInitArticulatedTime = 0;
	TotalConstructBodyInstanceTime = 0;
	TotalInitBodyTime = 0;
	TotalConvexGeomCount = 0;
	TotalFramesRun = 0;
#endif
}

/** Output stats for initializing physics. */
void ULevel::OutputInitRBPhysStats()
{
#if PERF_SHOW_PHYS_INIT_COSTS
	if( ((TotalInstanceGeomTime * 1000) > 10) 
	||  ((TotalCreateActorTime  * 1000) > 10) )
	{
		debugf( NAME_PerfWarning, TEXT("InstanceGeom: %f ms - %d Convex, Terrain %f ms, PerTriSM %f ms, InitArticulated %f ms, BodyInst Alloc %f ms, InitBody %f ms"), 
			TotalInstanceGeomTime * 1000.f,
			TotalConvexGeomCount,
			TotalTerrainTime * 1000.f,
			TotalPerTriStaticMeshTime * 1000.f,
			TotalInitArticulatedTime * 1000.f,
			TotalConstructBodyInstanceTime * 1000.f,
			TotalInitBodyTime * 1000.f);
		if ( TotalFramesRun > 0 )
		{
			debugf( NAME_PerfWarning, TEXT("InitBody Avg %f ms per frame over %d frames"), (TotalInitBodyTime * 1000.f) / (DOUBLE)TotalFramesRun, TotalFramesRun );
		}
		debugf( NAME_PerfWarning, TEXT("NxActor Creation: %f ms"), TotalCreateActorTime * 1000.f );
	}
#endif
}

/**
 *	Iterates over all actors calling InitRBPhys on them.
 */
void ULevel::IncrementalInitActorsRBPhys(INT NumActorsToInit)
{
#if PERF_SHOW_PHYS_INIT_COSTS
	TotalFramesRun++;
#endif
	// A value of 0 means that we want to update all components.
	UBOOL bForceUpdateAllActors = FALSE;
	if( NumActorsToInit == 0 )
	{
		NumActorsToInit = Actors.Num();
		bForceUpdateAllActors = TRUE;
	}
	// Only the game can use incremental update functionality.
	else
	{
		checkMsg(!GIsEditor && GIsGame,TEXT("Cannot call IncrementalInitActorsRBPhys with non 0 argument in the Editor/ commandlets."));
	}

	// Reset creation stats if this is the first time in here.
	if(CurrentActorIndexForInitActorsRBPhys == 0)
	{
		ResetInitRBPhysStats();
	}

	NumActorsToInit = Min( NumActorsToInit, Actors.Num() - CurrentActorIndexForInitActorsRBPhys );

	// Do as many Actor's as we were told, with the exception of 'collection' actors. They contain a variable number of 
	// RBPhys components that can take more time than we are anticipating at a higher level. Unless we do a force full update we
	// only do up to the first collection and only one collection at a time.
	UBOOL bShouldBailOutEarly = FALSE;
	for( INT i=0; i<NumActorsToInit && !bShouldBailOutEarly; i++ )
	{
		AActor* Actor = Actors(CurrentActorIndexForInitActorsRBPhys++);
		if( Actor )
		{
			// Request an early bail out if we encounter a SMCA... unless we force update all actors.
			UBOOL bIsCollectionActor = Actor->IsA(AStaticMeshCollectionActor::StaticClass()) || Actor->IsA(AProcBuilding::StaticClass());
			bShouldBailOutEarly = bIsCollectionActor ? !bForceUpdateAllActors : FALSE; 
			
			// Always do at least one and keep going as long as its not a SMCA
			if( !bShouldBailOutEarly || i == 0 )  
			{
				Actor->InitRBPhys();
			}
			else
			{			
				// Rollback since we didn't actually process the actor
				CurrentActorIndexForInitActorsRBPhys--;
				break;
			}	
		}
	}

	// See whether we are done.
	if( CurrentActorIndexForInitActorsRBPhys == Actors.Num() )
	{
		// Output stats for creation numbers.
		OutputInitRBPhysStats();

		// We only use the static-mesh physics data cache for startup - clear it now to free up more memory for run-time.
		ClearPhysStaticMeshCache();

		CurrentActorIndexForInitActorsRBPhys	= 0;

		// Set flag to indicate we have initialized all Actors.
		bAlreadyInitializedAllActorRBPhys		= TRUE;
	}
	// Only the game can use incremental update functionality.
	else
	{
		check(!GIsEditor && GIsGame);
	}
}


/**
 *	Destroys the physics engine BSP representation. 
 *	Does not iterate over actors - they are responsible for cleaning themselves up in AActor::Destroy.
 *	We don't free the actual physics mesh here, in case we want to InitLevelRBPhys again (ie unhide the level).
 */
void ULevel::TermLevelRBPhys(FRBPhysScene* Scene)
{
#if WITH_NOVODEX
	// hardware scene support
	if(Scene == NULL || SceneIndex == Scene->NovodexSceneIndex)
	{
		NxScene * NovodexScene = GetNovodexPrimarySceneFromIndex(SceneIndex);
		if( NovodexScene )
		{
			// Free the level BSP actor.
			if( LevelBSPActor )
			{
				NovodexScene->releaseActor(*(LevelBSPActor));
				LevelBSPActor = NULL;
			}

			// Free the level Convex BSP actor.
			if( LevelConvexBSPActor )
			{
				NovodexScene->releaseActor(*(LevelConvexBSPActor));
				LevelConvexBSPActor = NULL;
			}
		}
	}
#endif // WITH_NOVODEX
}

void ULevel::BeginDestroy()
{
	if ( GStreamingManager )
	{
		// At this time, referenced UTexture2Ds are still in memory.
		GStreamingManager->RemoveLevel( this );
	}

	Super::BeginDestroy();

	if (GWorld && GWorld->PersistentLevel == this && GWorld->Scene)
	{
		GWorld->Scene->SetPrecomputedVisibility(NULL);
		GWorld->Scene->SetPrecomputedVolumeDistanceField(NULL);
		GWorld->Scene->SetImageReflectionEnvironmentTexture(NULL, FLinearColor::Black, 0);
		RemoveFromSceneFence.BeginFence();
	}
}

UBOOL ULevel::IsReadyForFinishDestroy()
{
	const UBOOL bReady = Super::IsReadyForFinishDestroy();
	return bReady && !RemoveFromSceneFence.GetNumPendingFences();
}

void ULevel::FinishDestroy()
{
	TermLevelRBPhys(NULL);

#if WITH_NOVODEX

	// Add mesh to list to clean up.
	if(LevelBSPPhysMesh)
	{
		GNovodexPendingKillTriMesh.AddItem(LevelBSPPhysMesh);
		LevelBSPPhysMesh = NULL;
	}

#endif // WITH_NOVODEX

	delete PrecomputedLightVolume;
	PrecomputedLightVolume = NULL;

	Super::FinishDestroy();
}

/** Create the cache of cooked collision data for static meshes used in this level. */
void ULevel::BuildPhysStaticMeshCache()
{
	// Ensure cache is empty
	ClearPhysStaticMeshCache();
#if WITH_APEX
	if ( GApexManager )
	{
		GApexManager->GetApexSDK()->getCachedData().clear();
	}
#endif

	INT TriByteCount = 0;
	INT TriMeshCount = 0;
	INT HullByteCount = 0;
	INT HullCount = 0;
	DOUBLE BuildPhysCacheStart = appSeconds();

	GWarn->PushStatus();

	// Iterate over each actor in the level
	for(INT i=0; i<Actors.Num(); i++)
	{
		if( i % 20 == 0 )
		{
			GWarn->UpdateProgress( i, Actors.Num() );
		}

		AActor* Actor = Actors(i);
		if(Actor)
		{
            //Give actors a chance to do something unique with any static meshes they may have
			Actor->BuildPhysStaticMeshCache(this, TriByteCount, TriMeshCount, HullByteCount, HullCount);
		}
	}

	GWarn->UpdateProgress( Actors.Num(), Actors.Num() );
	GWarn->PopStatus();

#if WITH_NOVODEX
	// Update the version of this data to the current one.
	CachedPhysSMDataVersion = GCurrentCachedPhysDataVersion;
#endif

	debugf( TEXT("Built Phys StaticMesh Cache: %2.3f ms"), (appSeconds() - BuildPhysCacheStart) * 1000.f );
	debugf( TEXT("COOKEDPHYSICS: %d TriMeshes (%f KB), %d Convex Hulls (%f KB) - Total %f KB"), 
		TriMeshCount, ((FLOAT)TriByteCount)/1024.f, 
		HullCount, ((FLOAT)HullByteCount)/1024.f, 
		((FLOAT)(TriByteCount + HullByteCount))/1024.f);
}


/**  Clear the static mesh cooked collision data cache. */
void ULevel::ClearPhysStaticMeshCache()
{
	CachedPhysSMDataMap.Empty();
	CachedPhysSMDataStore.Empty();
	CachedPhysPerTriSMDataMap.Empty();
	CachedPhysPerTriSMDataStore.Empty();
}

/** 
 *	Utility for finding if we have cached data for a paricular static mesh at a particular scale.
 *	Returns NULL if there is no cached data. 
 *	The returned pointer will change if anything is added/removed from the cache, so should not be held.
 */
FKCachedConvexData* ULevel::FindPhysStaticMeshCachedData(UStaticMesh* InMesh, const FVector& InScale3D)
{
	// If we are intentionally not using precooked data, or its out of date, dont use it.
#if WITH_NOVODEX
	if( !bUsePrecookedPhysData || CachedPhysSMDataVersion != GCurrentCachedPhysDataVersion )
	{
		return NULL;
	}

	// First look up mesh in map to find all cached data for this mesh
	TArray<FCachedPhysSMData> OutData;
	CachedPhysSMDataMap.MultiFind(InMesh, OutData);

	// Then iterate over results to see if we have one at the right scale.
	for(INT i=0; i<OutData.Num(); i++)
	{
		if( (OutData(i).Scale3D - InScale3D).IsNearlyZero() )
		{
			check( OutData(i).CachedDataIndex < CachedPhysSMDataStore.Num() );
			return &CachedPhysSMDataStore( OutData(i).CachedDataIndex );
		}
	}
#endif // WITH_NOVODEX

	return NULL;
}

/** 
 *	Utility for finding if we have cached per-triangle data for a paricular static mesh at a particular scale.
 *	Returns NULL if there is no cached data. Pointer is to element of a map, so it invalid if anything is added/removed from map.
 */
FKCachedPerTriData* ULevel::FindPhysPerTriStaticMeshCachedData(UStaticMesh* InMesh, const FVector& InScale3D)
{
	// If we are intentionally not using precooked data, or its out of date, dont use it.
#if WITH_NOVODEX
	if( !bUsePrecookedPhysData || CachedPhysSMDataVersion != GCurrentCachedPhysDataVersion )
	{
		return NULL;
	}

	// First look up mesh in map to find all cached data for this mesh
	TArray<FCachedPerTriPhysSMData> OutData;
	CachedPhysPerTriSMDataMap.MultiFind(InMesh, OutData);

	// Then iterate over results to see if we have one at the right scale.
	for(INT i=0; i<OutData.Num(); i++)
	{
		if( (OutData(i).Scale3D - InScale3D).IsNearlyZero() )
		{
			check( OutData(i).CachedDataIndex < CachedPhysPerTriSMDataStore.Num() );
			return &CachedPhysPerTriSMDataStore( OutData(i).CachedDataIndex );
		}
	}
#endif // WITH_NOVODEX

	return NULL;
}


//////////////////////////////////////////////////////////////////////////
// UWORLD
//////////////////////////////////////////////////////////////////////////

/** Create the physics scene for the world. */
void UWorld::InitWorldRBPhys()
{
#if WITH_NOVODEX
	if( !RBPhysScene )
	{
		// JTODO: How do we handle changing gravity on the fly?
		FVector Gravity = FVector( 0.f, 0.f, GWorld->GetRBGravityZ() );

		RBPhysScene			= CreateRBPhysScene(Gravity);

#if WITH_APEX 
		if ( (RBPhysScene != NULL) && (RBPhysScene->ApexScene != NULL) )
		{
			FIApexScene* Scene = RBPhysScene->ApexScene;
			NxApexScene* ApexScene = Scene->GetApexScene();
			if ( ApexScene != NULL )
			{
				AWorldInfo* WorldInfo = GetWorldInfo();
				if(WorldInfo!=NULL)
				{
					if ( WorldInfo->ApexLODResourceBudget >= 0.0f )
					{
						ApexScene->setLODResourceBudget( WorldInfo->ApexLODResourceBudget );
					}
					else
					{
						ApexScene->setLODResourceBudget( GSystemSettings.ApexLODResourceBudget );
					}
					if(GApexManager)
					{
						if(GApexManager->GetModuleDestructible())
						{
							GApexManager->GetModuleDestructible()->setLODBenefitValue(WorldInfo->ApexDestructionLODResourceValue);
						}
						if(GApexManager->GetModuleClothing())
						{
							GApexManager->GetModuleClothing()->setLODBenefitValue(WorldInfo->ApexClothingLODResourceValue);
						}
					}
				}
			}
		}
#endif
	}
#endif // WITH_NOVODEX
}

/** Destroy the physics engine scene. */
void UWorld::TermWorldRBPhys()
{
#if WITH_NOVODEX
	if(RBPhysScene)
	{
		// Ensure all actors in the world are terminated before we clean up the scene.
		// If we don't do this, we'll have pointers to garbage NxActors and stuff.
		
		// We cannot iterate over the Levels array as we also need to deal with levels that are no longer associated
		// with the world but haven't been GC'ed before we change the world. The problem is TermWorldRBPhys destroying
		// the Novodex scene which is why we need to make sure all ULevel/ AActor objects have their physics terminated
		// before this happens.
		for( TObjectIterator<ULevel> It; It; ++It ) 
		{
			ULevel* Level = *It;
			
			for(INT j=0; j<Level->Actors.Num(); j++)
			{
				AActor* Actor = Level->Actors(j);
				if(Actor)
				{
					check(Actor->IsValid());
					Actor->TermRBPhys(RBPhysScene);
				}
			}
			
			// Ensure level is cleaned up too.
			Level->TermLevelRBPhys(RBPhysScene);
		}
		
		// Ensure that all primitive components had a chance to delete their BodyInstance data.
		for( TObjectIterator<UPrimitiveComponent> It; It; ++It )
		{
			UPrimitiveComponent* PrimitiveComponent = *It;
			PrimitiveComponent->TermComponentRBPhys(RBPhysScene);
		}

		// Make sure to release any deferred removals...
		DeferredRBResourceCleanup(RBPhysScene, FALSE);

		// Then release the scene itself.
		DestroyRBPhysScene(RBPhysScene);
		RBPhysScene = NULL;
	}
#endif // WITH_NOVODEX
}

/** Fire off physics engine thread. */
void UWorld::TickWorldRBPhys(FLOAT DeltaSeconds)
{

#if WITH_NOVODEX
	if (!RBPhysScene)
		return;

	FRBPhysScene* UseScene = RBPhysScene;

	// wait for any scenes that may still be running.
	WaitPhysCompartments(UseScene);

	// When ticking the main scene, clean up any physics engine resources (once a frame)
	DeferredRBResourceCleanup(GWorld->RBPhysScene, TRUE);

	FVector DefaultGravity( 0.f, 0.f, GWorld->GetRBGravityZ() );

	NxScene* NovodexScene = UseScene->GetNovodexPrimaryScene();
	if(NovodexScene)
	{
		NovodexScene->setGravity( U2NPosition(DefaultGravity) );
	}

	TickRBPhysScene(UseScene, DeltaSeconds);
#endif // WITH_NOVODEX
}

/**
 * Waits for the physics scene to be done processing - blocking the main game thread if necessary.
 */
void UWorld::WaitWorldRBPhys()
{
#if WITH_NOVODEX
	if (!RBPhysScene)
		return;


	WaitRBPhysScene(RBPhysScene);

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	if(!RBPhysScene->UsingBufferedScene)
	{
		RBPhysScene->AddNovodexDebugLines(GWorld->LineBatcher);
	}
#endif // !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE

#endif // WITH_NOVODEX
}

/** 
 *  Get a new BodyInstance from the pool, copying the values from the supplied BodyInstance. 
 *	TemplateBody can by NULL, in which case the resulting BodyInstance will have the class default properties.
 */
URB_BodyInstance* UWorld::InstanceRBBody(URB_BodyInstance const * TemplateBody)
{
	URB_BodyInstance* NewBI = NULL;
	if(BodyInstancePool.Num() > 0)
	{
		NewBI = BodyInstancePool.Pop();
	}
	else
	{
		NewBI = ConstructObject<URB_BodyInstance>(URB_BodyInstance::StaticClass(), this, NAME_None);

		// If we _want_ class default objects for this body, we can just return now.
		if(!TemplateBody)
		{
			return NewBI;
		}
	}

	// Use passed in template (if present) or class default object otherwise
	URB_BodyInstance const * UseTemplate = TemplateBody ? TemplateBody : (URB_BodyInstance*)(URB_BodyInstance::StaticClass()->GetDefaultObject());

	// Check various instance pointers are NULL in the template - don't handle these
	check(UseTemplate->OwnerComponent == NULL);
	check(UseTemplate->BodyData == NULL);
	check(UseTemplate->BoneSpring == NULL);
	check(UseTemplate->BoneSpringKinActor == NULL);

	// We use appMemcpy to copy all the per instance properties from the default object.
	void* DestStartAddr = (void*)&(NewBI->OwnerComponent);
	// The size of data to copy is the distance between last and first property, plus the size of the last property.
	INT Size = ((BYTE*)&(NewBI->PhysMaterialOverride) - (BYTE*)&(NewBI->OwnerComponent)) + sizeof(NewBI->PhysMaterialOverride);

	void* SrcStartAddr = (void*)&(UseTemplate->OwnerComponent);

	// Do the copy
	appMemcpy(DestStartAddr, SrcStartAddr, Size);

	return NewBI;
}

/** Return an RB_BodyInstance to the pool - must not keep a reference to it after this! */
void UWorld::ReturnRBBody(URB_BodyInstance* ReturnBody)
{
	check(ReturnBody);
	check(!BodyInstancePool.ContainsItem(ReturnBody));
	
	// Clear refs 
	ReturnBody->OwnerComponent = NULL;
	ReturnBody->BodyData = NULL;
	ReturnBody->BoneSpring = NULL;
	ReturnBody->BoneSpringKinActor = NULL;

#if !DISABLE_POOLED_RB_INSTANCES
	BodyInstancePool.Push(ReturnBody);
#endif
}

/** Get a new ConstraintInstance from the pool, copying the values from the supplied ConstraintInstance. */
URB_ConstraintInstance* UWorld::InstanceRBConstraint(URB_ConstraintInstance const * TemplateConstraint)
{
	URB_ConstraintInstance* NewCI = NULL;
	if(ConstraintInstancePool.Num() > 0)
	{
		NewCI = ConstraintInstancePool.Pop();
	}
	else
	{
		NewCI = ConstructObject<URB_ConstraintInstance>(URB_ConstraintInstance::StaticClass(), this, NAME_None);

		// If we _want_ class default objects for this constraint, we can just return now.
		if(!TemplateConstraint)
		{
			return NewCI;
		}
	}

	// Use passed in template (if present) or class default object otherwise
	URB_ConstraintInstance const * UseTemplate = TemplateConstraint ? TemplateConstraint : (URB_ConstraintInstance*)(URB_ConstraintInstance::StaticClass()->GetDefaultObject());

	// Check various instance pointers are NULL in the template - don't handle these
	check(UseTemplate->Owner == NULL);
	check(UseTemplate->OwnerComponent == NULL);
	check(UseTemplate->DummyKinActor == NULL);

	// We use appMemcpy to copy all the per instance properties from the default object.
	void* DestStartAddr = (void*)&(NewCI->Owner);
	// The size of data to copy is the distance between last and first property, plus the size of the last property.
	INT Size = ((BYTE*)&(NewCI->DummyKinActor) - (BYTE*)&(NewCI->Owner)) + sizeof(NewCI->DummyKinActor);

	void* SrcStartAddr = (void*)&(UseTemplate->Owner);

	// Do the copy
	appMemcpy(DestStartAddr, SrcStartAddr, Size);

	return NewCI;
}

/** Return an RB_ConstraintInstance to the pool - must not keep a reference to it after this! */
void UWorld::ReturnRBConstraint(URB_ConstraintInstance* ReturnConstraint)
{
	check(ReturnConstraint);
	check(!ConstraintInstancePool.ContainsItem(ReturnConstraint));

	// Clear refs
	ReturnConstraint->Owner = NULL;
	ReturnConstraint->OwnerComponent = NULL;
	ReturnConstraint->DummyKinActor = NULL;

#if !DISABLE_POOLED_RB_INSTANCES
	ConstraintInstancePool.Push(ReturnConstraint);
#endif
}

/**
* Tells the world to compact the world's object pools to return memory to general availability
*/
void AWorldInfo::ClearObjectPools()
{
	GWorld->ConstraintInstancePool.Empty();
	GWorld->BodyInstancePool.Empty();
	GWorld->AnimTreePool.Empty();
}

void AWorldInfo::SetLevelRBGravity(FVector NewGrav)
{
#if WITH_NOVODEX
	if(GWorld->RBPhysScene)
	{
		GWorld->RBPhysScene->SetGravity(NewGrav);
	}
#endif // WITH_NOVODEX
}

//////// GAME-LEVEL RIGID BODY PHYSICS STUFF ///////

#if WITH_NOVODEX

struct FPhysXAlloc
{
	INT Size;
#ifdef _DEBUG
	char Filename[100]; 
	INT	Count;
#endif
	INT Line;
};

class FNxAllocator : public NxUserAllocator
{
#if KEEP_TRACK_OF_NOVODEX_ALLOCATIONS
	/** Sync object for thread safety */
	FCriticalSection* SyncObject;
#endif

public:
	/** Create the synch object if we are tracking allocations */
	FNxAllocator(void)
	{
#if KEEP_TRACK_OF_NOVODEX_ALLOCATIONS
		SyncObject = GSynchronizeFactory->CreateCriticalSection();
		check(SyncObject);
#endif
	}
	/** Create the synch object if we are tracking allocations */
	virtual ~FNxAllocator(void)
	{
#if KEEP_TRACK_OF_NOVODEX_ALLOCATIONS
		GSynchronizeFactory->Destroy(SyncObject);
#endif
	}

	virtual void* mallocDEBUG(size_t Size, const char* Filename, int Line)
	{
		STAT(CallCount++);
		CLOCK_CYCLES(CallCycles);
		SCOPE_CYCLE_COUNTER(STAT_NovodexAllocatorTime);
		void* Pointer = appMalloc(Size);
		AddAllocation( Pointer, Size, Filename, Line );
		UNCLOCK_CYCLES(CallCycles);
		return Pointer;
	}

	virtual void* malloc(size_t Size)
	{
		STAT(CallCount++);
		CLOCK_CYCLES(CallCycles);
		void* Pointer = appMalloc(Size);
		AddAllocation( Pointer, Size, "MALLOC NON DEBUG", 0 );
		UNCLOCK_CYCLES(CallCycles);
		return Pointer;
	}

	virtual void* realloc(void* Memory, size_t Size)
	{
		STAT(CallCount++);
		CLOCK_CYCLES(CallCycles);
		RemoveAllocation( Memory );
		void* Pointer = appRealloc(Memory, Size);
		AddAllocation( Pointer, Size, "", 0 );
		UNCLOCK_CYCLES(CallCycles);
		return Pointer;
	}

	virtual void free(void* Memory)
	{
		STAT(CallCount++);
		CLOCK_CYCLES(CallCycles);
		RemoveAllocation( Memory );
		appFree(Memory);
		UNCLOCK_CYCLES(CallCycles);
	}

	/** Number of cycles spent in allocator this frame.			*/
	static DWORD					CallCycles;
	/** Number of times allocator has been invoked this frame.	*/
	static DWORD					CallCount;

#if KEEP_TRACK_OF_NOVODEX_ALLOCATIONS
	/** Dynamic map from allocation pointer to size.			*/
	static TMap<PTRINT,FPhysXAlloc>	AllocationToSizeMap;
	/** Total size of current allocations in bytes.				*/
	static SIZE_T					TotallAllocationSize;
	/** Number of allocations.									*/
	static SIZE_T					NumAllocations;

	/**
	 * Add allocation to keep track of.
	 *
	 * @param	Pointer		Allocation
	 * @param	Size		Allocation size in bytes
	 */
	void AddAllocation( void* Pointer, SIZE_T Size, const char* Filename, INT Line )
	{
		if(!GExitPurge)
		{
			FScopeLock sl(SyncObject);
			NumAllocations++;
			TotallAllocationSize += Size;

			FPhysXAlloc Alloc;
#ifdef _DEBUG
			appStrncpyANSI(Alloc.Filename, Filename, 100);
#endif
			Alloc.Size = Size;
			Alloc.Line = Line;

			AllocationToSizeMap.Set( (PTRINT) Pointer, Alloc );
			SET_DWORD_STAT(STAT_NovodexNumAllocations,NumAllocations);
			SET_DWORD_STAT(STAT_NovodexTotalAllocationSize,TotallAllocationSize);
			SET_DWORD_STAT(STAT_MemoryNovodexTotalAllocationSize,TotallAllocationSize);
		}
	}
	/**
	 * Remove allocation from list to track.
	 *
	 * @param	Pointer		Allocation
	 */
	void RemoveAllocation( void* Pointer )
	{
		if(!GExitPurge)
		{
			FScopeLock sl(SyncObject);
			NumAllocations--;
			FPhysXAlloc* AllocPtr = AllocationToSizeMap.Find( (PTRINT) Pointer );
			check(AllocPtr);
			TotallAllocationSize -= AllocPtr->Size;
			AllocationToSizeMap.Remove( (PTRINT) Pointer );
			SET_DWORD_STAT(STAT_NovodexNumAllocations,NumAllocations);
			SET_DWORD_STAT(STAT_NovodexTotalAllocationSize,TotallAllocationSize);
			SET_DWORD_STAT(STAT_MemoryNovodexTotalAllocationSize,TotallAllocationSize);
		}
	}
#elif STATS
	/**
	* Add allocation to keep track of.
	*
	* @param	Pointer		Allocation
	* @param	Size		Allocation size in bytes
	*/
	void AddAllocation( void* Pointer, SIZE_T Size, const char* Filename, INT Line )
	{
		if(!GExitPurge)
		{
			DWORD AllocSize = 0;

			if (GMalloc->GetAllocationSize(Pointer, AllocSize))
			{
				INC_DWORD_STAT(STAT_NovodexNumAllocations);
				INC_DWORD_STAT_BY(STAT_NovodexTotalAllocationSize,AllocSize);
				INC_DWORD_STAT_BY(STAT_MemoryNovodexTotalAllocationSize,AllocSize);
			}
		}
	}
	/**
	* Remove allocation from list to track.
	*
	* @param	Pointer		Allocation
	*/
	void RemoveAllocation( void* Pointer )
	{
		if(!GExitPurge)
		{
			DWORD AllocSize = 0;

			if (GMalloc->GetAllocationSize(Pointer, AllocSize))
			{
				DEC_DWORD_STAT(STAT_NovodexNumAllocations);
				DEC_DWORD_STAT_BY(STAT_NovodexTotalAllocationSize,AllocSize);
				DEC_DWORD_STAT_BY(STAT_MemoryNovodexTotalAllocationSize,AllocSize);
			}
		}
	}
#else	//KEEP_TRACK_OF_NOVODEX_ALLOCATIONS
	void AddAllocation( void* Pointer, SIZE_T Size, const char* Filename, INT Line ) {}
	void RemoveAllocation( void* Pointer ) {}
#endif	//KEEP_TRACK_OF_NOVODEX_ALLOCATIONS
};

#if KEEP_TRACK_OF_NOVODEX_ALLOCATIONS
/** Dynamic map from allocation pointer to size.			*/
TMap<PTRINT,FPhysXAlloc> FNxAllocator::AllocationToSizeMap;
/** Total size of current allocations in bytes.				*/
SIZE_T FNxAllocator::TotallAllocationSize;
/** Number of allocations.									*/
SIZE_T FNxAllocator::NumAllocations;
#endif	//KEEP_TRACK_OF_NOVODEX_ALLOCATIONS
/** Number of cycles spent in allocator this frame.			*/
DWORD FNxAllocator::CallCycles;
/** Number of times allocator has been invoked this frame.	*/
DWORD FNxAllocator::CallCount;

class FNxOutputStream : public NxUserOutputStream
{
public:
	virtual void reportError(NxErrorCode ErrorCode, const char* Message, const char* Filename, int Line)
	{
		// Hack to suppress warnings about open meshes (get this when cooking terrain) and static compounds.
		if( appStrstr( ANSI_TO_TCHAR(Message), TEXT("Mesh has a negative volume!") ) == NULL && 
			appStrstr( ANSI_TO_TCHAR(Message), TEXT("Creating static compound shape") ) == NULL )
		{
			debugf(NAME_DevPhysics, TEXT("Error (%d) in file %s, line %d: %s"), (INT)ErrorCode, ANSI_TO_TCHAR(Filename), Line, ANSI_TO_TCHAR(Message));
		}
	}

	virtual NxAssertResponse reportAssertViolation(const char* Message, const char* Filename, int Line)
	{
		debugf(NAME_DevPhysics, TEXT("Assert in file %s, line %d: %s"), ANSI_TO_TCHAR(Filename), Line, ANSI_TO_TCHAR(Message));
		return NX_AR_BREAKPOINT;
	}

	virtual void print(const char* Message)
	{
		debugf(NAME_DevPhysics, TEXT("%s"), ANSI_TO_TCHAR(Message));
	}
};

#endif // WITH_NOVODEX



#if WITH_NOVODEX
	/** Used to access the PhysX version without exposing the define */
	INT GCurrentPhysXVersion = NX_PHYSICS_SDK_VERSION;
	// Touch this value if something in UE3 related to physics changes
	BYTE GCurrentEpicPhysDataVersion = 17;
	// This value will auto-update as the SDK changes
	INT	GCurrentCachedPhysDataVersion =
#if USE_QUICKLOAD_CONVEX
		0x00000080 |
#endif
		GCurrentPhysXVersion | GCurrentEpicPhysDataVersion;
#endif // WITH_NOVODEX

#define MISSING_PHYSX_CAPTION TEXT( "Failed to Initialize PhysX Hardware Acceleration" )
#define OUTOFDATE_PHYSX_MESSAGE TEXT( "Using hardware accelerated PhysX has been requested, but the drivers were out of date.\r\n\r\nPlease download the latest drivers from http://www.nvidia.com/object/physx_system_software.html\r\n\r\nFATAL ERROR - EXITING" )

void InitGameRBPhys()
{
#if WITH_NOVODEX

#if _MSC_VER && !XBOX // Windows only hack to bypass PhysX installation req
	UBOOL bDisablePhysXHardware;
	verify( GConfig->GetBool(	TEXT("Engine.Engine"),	TEXT("bDisablePhysXHardwareSupport"	), bDisablePhysXHardware,		GEngineIni ) );
	HMODULE PhysXLoader = NULL;

	// Load local PhysXLoader dll
	#if defined(_WIN64) // WIN64

		#if defined( _DEBUG ) && defined(USE_DEBUG_NOVODEX)
			PhysXLoader = LoadLibrary( L"PhysXLoader64DEBUG.dll" );
		#else
			PhysXLoader = LoadLibrary( L"PhysXLoader64.dll" );
		#endif

	#else // WIN32

		#if defined( _DEBUG ) && defined(USE_DEBUG_NOVODEX)
			PhysXLoader = LoadLibrary( L"PhysXLoaderDEBUG.dll" );
		#else
			PhysXLoader = LoadLibrary( L"PhysXLoader.dll" );
		#endif

	#endif

	if(!PhysXLoader)
	{
		debugf(TEXT("ERROR: Failed to load PhysX Loader dll!"));
		check(0);
	}
#endif

	FNxAllocator* Allocator = new FNxAllocator();
	FNxOutputStream* Stream = new FNxOutputStream();

#if WITH_PHYSX_COOKING
	GNovodexCooking = NxGetCookingLib(NX_PHYSICS_SDK_VERSION);
	if( GNovodexCooking == NULL )
	{
#if _WINDOWS
		MessageBox( NULL, OUTOFDATE_PHYSX_MESSAGE, MISSING_PHYSX_CAPTION, MB_OK | MB_ICONERROR );
		appRequestExit( TRUE );
#else
		check(0);
#endif
	}
#endif  // WITH_PHYSX_COOKING

	// Optimize PhysX GPU memory usage
	NxPhysicsSDKDesc PhysXDesc;

#if _WINDOWS
	// PhysXDesc.gpuHeapSize is the total amount of GPU memory (in MB) set aside for PhysX.
	// PhysXDesc.meshCacheSize is the portion of the PhysX GPU heap dedicated to the mesh cache.
	if( !bDisablePhysXHardware ) 
	{
		debugf(TEXT("PhysX GPU Support: ENABLED"));

		INT PhysXGpuHeapSize;
		INT PhysXMeshCacheSize;

		verify( GConfig->GetInt( TEXT("Engine.Engine"), TEXT("PhysXGpuHeapSize"), PhysXGpuHeapSize, GEngineIni ) );
		verify( GConfig->GetInt( TEXT("Engine.Engine"), TEXT("PhysXMeshCacheSize"), PhysXMeshCacheSize, GEngineIni ) );

		if( PhysXGpuHeapSize >= 0 )
		{
			PhysXDesc.gpuHeapSize = PhysXGpuHeapSize;
		}
		if( PhysXMeshCacheSize >= 0 )
		{
			PhysXDesc.meshCacheSize = PhysXMeshCacheSize;
		}

		// Enable GPU physics (off by default)
		PhysXDesc.flags &= ~NX_SDKF_NO_HARDWARE;
	}
	else
	{
		debugf(TEXT("PhysX GPU Support: DISABLED"));

		PhysXDesc.gpuHeapSize = 0;
	}
	check(PhysXDesc.isValid());
#endif // _WINDOWS

#if USE_QUICKLOAD_CONVEX
	GNovodeXQuickLoad = static_cast<NxExtensionQuickLoad *>(NxCreateExtension(NX_EXT_QUICK_LOAD));
	check(GNovodeXQuickLoad);
	NxUserAllocator * QLAllocator = GNovodeXQuickLoad->wrapAllocator(*Allocator, NxExtensionQuickLoad::RemoveSDKReferencesToQLMesh);
	NxSDKCreateError OutError = NXCE_NO_ERROR;
	GNovodexSDK = NxCreatePhysicsSDK(NX_PHYSICS_SDK_VERSION, QLAllocator, Stream, PhysXDesc, &OutError);
	if(!GNovodexSDK)
	{
		debugf(TEXT("ERROR Creating GNovodexSDX (Error Code: %d)"), (INT)OutError);
	}
	check(GNovodexSDK);
	// Init the cooker. Sets params to defaults.
	#if WITH_PHYSX_COOKING
		GNovodexCooking->NxInitCooking(&GNovodexSDK->getFoundationSDK().getAllocator(), Stream);
	#endif
	GNovodeXQuickLoad->initialize(*GNovodexSDK);
#else
	GNovodexSDK = NxCreatePhysicsSDK(NX_PHYSICS_SDK_VERSION, Allocator, Stream, PhysXDesc);
	#if WITH_PHYSX_COOKING
		GNovodexCooking->NxInitCooking(Allocator, Stream);
	#endif
#endif

	// See "https://udn.epicgames.com/Three/DevelopingOnVista64bit if this check fires.
	check(GNovodexSDK);

#if PS3
	// Specify 1-15 per SPU (0 to not use that SPU).
	BYTE SPUPriorities[8] = { SPU_PRIO_PHYSX,SPU_PRIO_PHYSX,SPU_PRIO_PHYSX,SPU_PRIO_PHYSX,SPU_PRIO_PHYSX,SPU_PRIO_PHYSX,SPU_PRIO_PHYSX,SPU_PRIO_PHYSX };
	// don't create printf handler--it's in UnPS3.cpp
	NxCellSpursControl::initWithSpurs( GSPURS, SPU_NUM_PHYSX, SPUPriorities, FALSE );
#endif

	// Set parameters

	// Set the extra thickness we will use, to make more stable contact generation.
	GNovodexSDK->setParameter(NX_SKIN_WIDTH, PhysSkinWidth);

	// support creating meshes while scenes are running.
	GNovodexSDK->setParameter(NX_ASYNCHRONOUS_MESH_CREATION, 1);

#ifdef ENABLE_CCD
	// Turn on CCD.
	GNovodexSDK->setParameter(NX_CONTINUOUS_CD, 1);
#endif

	// Some flags for legacy behaviour
	GNovodexSDK->setParameter(NX_ADAPTIVE_FORCE, 0);
	GNovodexSDK->setParameter(NX_IMPROVED_SPRING_SOLVER, 0);
	GNovodexSDK->setParameter(NX_FAST_MASSIVE_BP_VOLUME_DELETION, 1);

#if XBOX
	SetPhysCookingXenon();
#elif PS3
	SetPhysCookingPS3();
#endif

#if WITH_PHYSX_COOKING
	// Set skin thickness for cooking to be the same as the one above.
	const NxCookingParams& Params = GNovodexCooking->NxGetCookingParams();
	NxCookingParams NewParams = Params;
	NewParams.skinWidth = PhysSkinWidth;
	GNovodexCooking->NxSetCookingParams(NewParams);
#endif

#if STATS
	// Make sure that these stats exist so we can write to them during the physics ticking.
	{
		FCycleStat* RootStat = GStatManager.GetCurrentStat();
		GStatManager.GetChildStat(RootStat, STAT_RBBroadphaseUpdate);
		GStatManager.GetChildStat(RootStat, STAT_RBBroadphaseGetPairs);
		GStatManager.GetChildStat(RootStat, STAT_RBNearphase);
		GStatManager.GetChildStat(RootStat, STAT_RBSolver);
		GStatManager.GetChildStat(RootStat, STAT_NovodexAllocatorTime);
		GStatManager.GetChildStat(RootStat, STAT_HWTimeIncludingGpuWait);
	}
#endif // STATS

#endif // WITH_NOVODEX
}

void DestroyGameRBPhys()
{
#if WITH_NOVODEX

	// Make sure everythig is cleaned up before releasing the SDK
	if(GWorld)
	{
		DeferredRBResourceCleanup(GWorld->RBPhysScene);
	}


#if WITH_APEX
	if ( GApexCommands )
	{
		ReleaseApexCommands(GApexCommands);
		ReleaseApexManager(GApexManager);
	}
#endif

#if WITH_PHYSX_COOKING
	if ( GNovodexCooking )
	{
		GNovodexCooking->NxCloseCooking();
	}
#endif

	if( GNovodexSDK )
	{
		NxReleasePhysicsSDK(GNovodexSDK);
		GNovodexSDK = NULL;
	}

#if !CONSOLE
	// Cleanup mem from XML dumping.
	//NXU::releasePersistentMemory();
#endif // CONSOLE

#endif
}

/** Change the global physics-data cooking mode to cook to Xenon target. */
void SetPhysCookingXenon()
{
#if WITH_NOVODEX && WITH_PHYSX_COOKING
	const NxCookingParams& Params = GNovodexCooking->NxGetCookingParams();
	NxCookingParams NewParams = Params;
	NewParams.targetPlatform = PLATFORM_XENON;
	GNovodexCooking->NxSetCookingParams(NewParams);
#endif
}

/** Change the global physics-data cooking mode to cook to PS3 target. */
void SetPhysCookingPS3()
{
#if WITH_NOVODEX && WITH_PHYSX_COOKING
	const NxCookingParams& Params = GNovodexCooking->NxGetCookingParams();
	NxCookingParams NewParams = Params;
	NewParams.targetPlatform = PLATFORM_PLAYSTATION3;
	GNovodexCooking->NxSetCookingParams(NewParams);
#endif
}

/** Utility to determine the endian-ness of a set of cooked physics data. */
ECookedPhysicsDataEndianess GetCookedPhysDataEndianess(const TArray<BYTE>& InData)
{
	if(InData.Num() < 4)
	{
		return CPDE_Unknown;
	}
	else
	{
		// In the Novodex format, the 4th byte is 1 if data is little endian.
		if(InData(3) & 1)
		{
			return CPDE_LittleEndian;
		}
		else
		{
			return CPDE_BigEndian;
		}
	}
}


///////////////// RBPhysScene /////////////////

static INT NovodexSceneCount = 1;

/** Exposes creation of physics-engine scene outside Engine (for use with PhAT for example). */
FRBPhysScene* CreateRBPhysScene(const FVector& Gravity)
{
#if WITH_NOVODEX
	NxVec3 nGravity = U2NPosition(Gravity);
	
	check( GWorld && GWorld->GetWorldInfo() );
	AWorldInfo * Info = GWorld->GetWorldInfo();
	check(Info);
	FPhysXSceneProperties & SceneProperties = Info->PhysicsProperties;
	const UBOOL bPhysXHWPresent = IsPhysXHardwarePresent();

	NxSceneDesc SceneDesc;
	SceneDesc.gravity = nGravity;
	SceneDesc.maxTimestep = SceneProperties.PrimaryScene.TimeStep;
	SceneDesc.maxIter = ::Min(SceneProperties.PrimaryScene.MaxSubSteps, Info->MaxPhysicsSubsteps);
	SceneDesc.timeStepMethod = NX_TIMESTEP_FIXED;

#if XBOX
	// Indicate we'll use our own thread
	SceneDesc.flags = NX_SF_SIMULATE_SEPARATE_THREAD;
	//On 360 simThreadMask chooses the HW thread to run on.
	SceneDesc.simThreadMask = 1 << PHYSICS_HWTHREAD; 
#elif PLATFORM_DESKTOP
	// On non-Xbox platforms allow PhysX to handle threading
	SceneDesc.flags = NX_SF_SIMULATE_SEPARATE_THREAD | NX_SF_DISABLE_SCENE_MUTEX;
#else
	SceneDesc.flags = NX_SF_SIMULATE_SEPARATE_THREAD;
#endif

	// PhysX on PS3 wants this setting.
#if PS3
	SceneDesc.flags |= NX_SF_SEQUENTIAL_PRIMARY;
#endif // PS3

	SceneDesc.userContactReport = &nContactReportObject; // See the GNovodexSDK->setActorGroupPairFlags call below for which object will cause notification.
	SceneDesc.userNotify = &nNotifyObject;
	SceneDesc.userContactModify = &nContactModifyObject; // See the GNovodexSDK->setActorGroupPairFlags
	SceneDesc.staticStructure = NX_PRUNING_DYNAMIC_AABB_TREE;
	SceneDesc.dynamicStructure = NX_PRUNING_NONE;
	SceneDesc.dynamicTreeRebuildRateHint = 100;

	if( SceneProperties.PrimaryScene.bUseHardware && bPhysXHWPresent )
   	{
 		SceneDesc.simType = NX_SIMULATION_HW;
 		debugf( TEXT("Primary PhysX scene will be in hardware.") );
   	}
   	else
   	{
 		SceneDesc.simType = NX_SIMULATION_SW;
 		debugf( TEXT("Primary PhysX scene will be in software.") );
   	}

	// Create FRBPhysScene container
	FRBPhysScene* NewRBPhysScene = new FRBPhysScene();	// moved this, now done before physics scene creation (so we may loop)
	
	NxScenePair ScenePair;
	
	// Create the primary scene.
	ScenePair.PrimaryScene = 0;
	NewRBPhysScene->UsingBufferedScene = FALSE;
	NewRBPhysScene->CompartmentsRunning = FALSE;

	//Wait for any simulations to finish, otherwise creation will fail.
	WaitForAllNovodexScenes();

	#if SUPPORT_DOUBLE_BUFFERING
	if(Info->bSupportDoubleBufferedPhysics && (GNumHardwareThreads >= 2 || IsPhysXHardwarePresent()))
	{
		debugf( TEXT("Creating Double Buffered Primary PhysX Scene.") );
		ScenePair.PrimaryScene = NxdScene::create(*GNovodexSDK, SceneDesc);
		check(ScenePair.PrimaryScene);
		if(ScenePair.PrimaryScene)
		{
			NewRBPhysScene->UsingBufferedScene = TRUE;
		}
		((NxdScene*)ScenePair.PrimaryScene)->setDoubleBufferReport(&nDoubleBufferReportObject);
	}
	#endif
	if(!ScenePair.PrimaryScene)
	{
		debugf( TEXT("Creating Primary PhysX Scene.") );
		ScenePair.PrimaryScene = GNovodexSDK->createScene(SceneDesc);
		check(ScenePair.PrimaryScene);
#if WITH_APEX
		NewRBPhysScene->ApexScene = NULL;
		if ( GApexManager && ScenePair.PrimaryScene )
		{
			NewRBPhysScene->ApexScene = CreateApexScene(ScenePair.PrimaryScene, GApexManager, true );
			check(NewRBPhysScene->ApexScene);
		}
#endif
	}
	
	// Create the async compartment.
	if(ScenePair.PrimaryScene)
	{
		NxCompartmentDesc CompartmentDesc;
		CompartmentDesc.deviceCode = NX_DC_PPU_AUTO_ASSIGN;
		// TODO: read this values in from an INI file or something.
		CompartmentDesc.gridHashCellSize = 20.0f;
		CompartmentDesc.gridHashTablePower = 8;
		CompartmentDesc.flags = NX_CF_INHERIT_SETTINGS;
		CompartmentDesc.timeScale = 1.0f;
		
#if defined(_WINDOWS)
		CompartmentDesc.type = NX_SCT_RIGIDBODY;
		CompartmentDesc.deviceCode =
			( SceneProperties.CompartmentRigidBody.bUseHardware && bPhysXHWPresent ) ? NX_DC_PPU_AUTO_ASSIGN : NX_DC_CPU;
		ScenePair.RigidBodyCompartment = ScenePair.PrimaryScene->createCompartment(CompartmentDesc);
		check(ScenePair.RigidBodyCompartment);
#endif
		
#if !defined(NX_DISABLE_FLUIDS)
		CompartmentDesc.type = NX_SCT_FLUID;
		CompartmentDesc.deviceCode =
			( SceneProperties.CompartmentFluid.bUseHardware && bPhysXHWPresent ) ? NX_DC_PPU_AUTO_ASSIGN : NX_DC_CPU;
		if (GIsEditor && !GIsGame)
		{
			CompartmentDesc.deviceCode = NX_DC_CPU;
		}

		ScenePair.FluidCompartment = ScenePair.PrimaryScene->createCompartment(CompartmentDesc);
		check(ScenePair.FluidCompartment);
#endif

#if !CONSOLE // compartments not supported on xenon yet
		CompartmentDesc.type = NX_SCT_CLOTH;
		CompartmentDesc.deviceCode = ( SceneProperties.CompartmentCloth.bUseHardware && bPhysXHWPresent ) ? NX_DC_PPU_AUTO_ASSIGN : NX_DC_CPU;
		if (GIsEditor && !GIsGame)
		{
			CompartmentDesc.deviceCode = NX_DC_CPU;
		}
		ScenePair.ClothCompartment = ScenePair.PrimaryScene->createCompartment(CompartmentDesc);
		check(ScenePair.ClothCompartment);
#endif	

#if 0 //!defined(NX_DISABLE_SOFTBODY)
		CompartmentDesc.type = NX_SCT_SOFTBODY;
		CompartmentDesc.deviceCode = NX_DC_PPU_AUTO_ASSIGN;
		if( !SceneProperties.CompartmentSoftBody.bUseHardware )
		{
			debugf( TEXT("Cannot create CPU soft body compartment.  Will be created in HW if available.") );
		}
		ScenePair.SoftBodyCompartment = ScenePair.PrimaryScene->createCompartment(CompartmentDesc);
#endif
	}

	NxScene *NovodexScene = ScenePair.PrimaryScene;
	
#if PS3
	// PS3 specific PhysX settings.

	// defaults that can be overridden
	UBOOL bUseSPUNarrow = TRUE;
	UBOOL bUseSPUMid = TRUE;
	UBOOL bUseSPUIslandGen = TRUE;
	UBOOL bUseSPUDynamics = TRUE;
	UBOOL bUseSPUCloth = TRUE;
	UBOOL bUseSPUHeightField = TRUE;
	UBOOL bUseSPURaycast = TRUE;

	// look for overrides on commandline
	ParseUBOOL(appCmdLine(), TEXT("spunarrow="), bUseSPUNarrow);
	ParseUBOOL(appCmdLine(), TEXT("spumid="), bUseSPUMid);
	ParseUBOOL(appCmdLine(), TEXT("spuisland="), bUseSPUIslandGen);
	ParseUBOOL(appCmdLine(), TEXT("spudynamics="), bUseSPUDynamics);
	ParseUBOOL(appCmdLine(), TEXT("spucloth="), bUseSPUCloth);
	ParseUBOOL(appCmdLine(), TEXT("spuheightfield="), bUseSPUHeightField);
	ParseUBOOL(appCmdLine(), TEXT("spuraycast="), bUseSPURaycast);

	if (ParseParam(appCmdLine(),TEXT("SPUNoPhysics")))
	{
		bUseSPUNarrow = bUseSPUMid = bUseSPUIslandGen = bUseSPUDynamics = bUseSPUCloth = bUseSPUHeightField = bUseSPURaycast = FALSE;
	}

	// disable SPU usage if desired
	if (!bUseSPUNarrow)
	{
		debugf(TEXT("PhysX: DISABLING Narrowphase SPU usage"));
		NxCellConfig::setSceneParamInt(NovodexScene, NxCellConfig::NX_CELL_SCENE_PARAM_SPU_NARROWPHASE, 0);
	}

	if (!bUseSPUMid)
	{
		debugf(TEXT("PhysX: DISABLING Midphase SPU usage"));
		//NxCellConfig::setSceneParamInt(NovodexScene, NxCellConfig::NX_CELL_SCENE_PARAM_SPU_MIDPHASE, 0);
	}

	if (!bUseSPUIslandGen)
	{
		debugf(TEXT("PhysX: DISABLING Island Gen SPU usage"));
		NxCellConfig::setSceneParamInt(NovodexScene, NxCellConfig::NX_CELL_SCENE_PARAM_SPU_ISLAND_GEN, 0);
	}

	if (!bUseSPUDynamics)
	{
		debugf(TEXT("PhysX: DISABLING Dynamics SPU usage"));
		NxCellConfig::setSceneParamInt(NovodexScene, NxCellConfig::NX_CELL_SCENE_PARAM_SPU_DYNAMICS, 0);
	}

	if (!bUseSPUCloth)
	{
//		debugf(TEXT("PhysX: DISABLING Cloth SPU usage"));
//		NxCellConfig::setSceneParamInt(NovodexScene, NxCellConfig::NX_CELL_SCENE_PARAM_SPU_CLOTH, 0);
	}

	if (!bUseSPUHeightField)
	{
		debugf(TEXT("PhysX: DISABLING HeightField SPU usage"));
		NxCellConfig::setSceneParamInt(NovodexScene, NxCellConfig::NX_CELL_SCENE_PARAM_SPU_HEIGHT_FIELD, 0);
	}

	if (!bUseSPURaycast)
	{
		debugf(TEXT("PhysX: DISABLING Raycast SPU usage"));
		NxCellConfig::setSceneParamInt(NovodexScene, NxCellConfig::NX_CELL_SCENE_PARAM_SPU_RAYCAST, 0);
	}

#endif // PS3

	// Use userData pointer in NovodexScene to store reference to this FRBPhysScene
	NovodexScene->userData = NewRBPhysScene;

	// Notify when anything in the 'notify collide' group touches anything.
	// Fire 'modify contact' callback when anything touches that group.
	NovodexScene->setActorGroupPairFlags(UNX_GROUP_DEFAULT,			UNX_GROUP_NOTIFYCOLLIDE,	NX_NOTIFY_FORCES | NX_NOTIFY_ON_TOUCH | NX_NOTIFY_ON_START_TOUCH);
	NovodexScene->setActorGroupPairFlags(UNX_GROUP_NOTIFYCOLLIDE,	UNX_GROUP_NOTIFYCOLLIDE,	NX_NOTIFY_FORCES | NX_NOTIFY_ON_TOUCH | NX_NOTIFY_ON_START_TOUCH);
	NovodexScene->setActorGroupPairFlags(UNX_GROUP_MODIFYCONTACT,	UNX_GROUP_NOTIFYCOLLIDE,	NX_NOTIFY_FORCES | NX_NOTIFY_ON_TOUCH | NX_NOTIFY_ON_START_TOUCH | NX_NOTIFY_CONTACT_MODIFICATION);
	NovodexScene->setActorGroupPairFlags(UNX_GROUP_MODIFYCONTACT,	UNX_GROUP_DEFAULT,			NX_NOTIFY_CONTACT_MODIFICATION);
	NovodexScene->setActorGroupPairFlags(UNX_GROUP_MODIFYCONTACT,	UNX_GROUP_MODIFYCONTACT,	NX_NOTIFY_CONTACT_MODIFICATION);

	NovodexScene->setActorGroupPairFlags(UNX_GROUP_THRESHOLD_NOTIFY,UNX_GROUP_DEFAULT,			NX_NOTIFY_FORCES | NX_NOTIFY_ON_TOUCH_FORCE_THRESHOLD | NX_NOTIFY_ON_START_TOUCH_FORCE_THRESHOLD);
	NovodexScene->setActorGroupPairFlags(UNX_GROUP_THRESHOLD_NOTIFY,UNX_GROUP_NOTIFYCOLLIDE,	NX_NOTIFY_FORCES | NX_NOTIFY_ON_TOUCH_FORCE_THRESHOLD | NX_NOTIFY_ON_START_TOUCH_FORCE_THRESHOLD);
	NovodexScene->setActorGroupPairFlags(UNX_GROUP_THRESHOLD_NOTIFY,UNX_GROUP_MODIFYCONTACT,	NX_NOTIFY_FORCES | NX_NOTIFY_ON_TOUCH_FORCE_THRESHOLD | NX_NOTIFY_ON_START_TOUCH_FORCE_THRESHOLD | NX_NOTIFY_CONTACT_MODIFICATION);
	NovodexScene->setActorGroupPairFlags(UNX_GROUP_THRESHOLD_NOTIFY,UNX_GROUP_THRESHOLD_NOTIFY,	NX_NOTIFY_FORCES | NX_NOTIFY_ON_TOUCH_FORCE_THRESHOLD | NX_NOTIFY_ON_START_TOUCH_FORCE_THRESHOLD);

	// Set up filtering
	NovodexScene->setFilterOps(NX_FILTEROP_OR, NX_FILTEROP_OR, NX_FILTEROP_SWAP_AND);
	NovodexScene->setFilterBool(true);  

	NxGroupsMask zeroMask;    
	zeroMask.bits0=zeroMask.bits1=zeroMask.bits2=zeroMask.bits3=0;    
	NovodexScene->setFilterConstant0(zeroMask);    
	NovodexScene->setFilterConstant1(zeroMask);

	// Add to map
	GNovodexSceneMap.Set(NovodexSceneCount, ScenePair);
	
#if !defined(NX_DISABLE_FLUIDS)
	NewRBPhysScene->PhysXEmitterManager = new FPhysXVerticalEmitter(*NewRBPhysScene);
#endif

	// Store index of NovodexScene in FRBPhysScene
	NewRBPhysScene->NovodexSceneIndex = NovodexSceneCount;

	// Increment scene count
	NovodexSceneCount++;

	return NewRBPhysScene;
#else
	return NULL;
#endif // WITH_NOVODEX
}

/** Exposes destruction of physics-engine scene outside Engine. */
void DestroyRBPhysScene(FRBPhysScene* Scene)
{
#if WITH_NOVODEX

	//Wait for any simulations to finish, otherwise release call will fail.
	WaitForAllNovodexScenes();

	if(Scene && Scene->CompartmentsRunning)
	{
		NxScene *NovodexScene = Scene->GetNovodexPrimaryScene();
		check(NovodexScene);
		if(NovodexScene)
		{
			WaitForNovodexScene(*NovodexScene);
		}
		Scene->CompartmentsRunning = FALSE;
	}
#if !defined(NX_DISABLE_FLUIDS)
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysicsFluidMeshEmitterUpdate)
		
		delete Scene->PhysXEmitterManager;
		Scene->PhysXEmitterManager = NULL;
	}
#endif

	NxScenePair *ScenePair = GetNovodexScenePairFromIndex(Scene->NovodexSceneIndex);
	if(ScenePair)
	{
		NxScene *PrimaryScene = ScenePair->PrimaryScene;

#if !NX_DISABLE_CLOTH
		for(INT i=GNovodexPendingKillCloths.Num()-1; i>=0; i--)
		{
			NxCloth* nCloth = GNovodexPendingKillCloths(i);
			check(nCloth);
			
			NxScene& nxScene = nCloth->getScene();

			if(&nxScene==PrimaryScene)
			{
				nxScene.releaseCloth(*nCloth);
				GNovodexPendingKillCloths.Remove(i);
			}
		}
#endif // !NX_DISABLE_CLOTH


		#if SUPPORT_DOUBLE_BUFFERING
		if(PrimaryScene && Scene->UsingBufferedScene)
		{
			NxdScene::release((NxdScene*)PrimaryScene);
			PrimaryScene = 0;
		}
		#endif
		if(PrimaryScene)
		{
#if WITH_APEX
			if ( Scene->ApexScene )
			{
				ReleaseApexScene(Scene->ApexScene);
				Scene->ApexScene = 0;
			}
#endif
			GNovodexSDK->releaseScene(*PrimaryScene);
		}
	}

	// Clear the entry to the scene in the map
	GNovodexSceneMap.Remove(Scene->NovodexSceneIndex);

	// Delete the FRBPhysScene container object.
	delete Scene;
#endif // WITH_NOVODEX
}

void DeferredReleaseNxJoint(NxJoint* InJoint, UBOOL bFreezeBody)
{
#if WITH_NOVODEX
	check(InJoint != NULL);
	GNovodexPendingKillJoint.AddItem(InJoint);
	if (bFreezeBody == TRUE)
	{
	}
#endif // WITH_NOVODEX
}

void DeferredReleaseNxActor(NxActor* InActor, UBOOL bFreezeBody)
{
#if WITH_NOVODEX
	check(InActor != NULL);
	GNovodexPendingKillActor.AddItem(InActor);
	if ((bFreezeBody == TRUE) && (InActor->isDynamic() == TRUE))
	{
		InActor->raiseBodyFlag(NX_BF_FROZEN);
	}
#endif // WITH_NOVODEX
}

/** 
*	Perform any cleanup of physics engine resources. 
*	This is deferred because when closing down the game, you want to make sure you are not destroying a mesh after the physics SDK has been shut down.
*/
void DeferredRBResourceCleanup(FRBPhysScene* PhysScene, UBOOL bCalledFromTick)
{
#if WITH_NOVODEX
	INT LastIndex = bCalledFromTick ? GNovodexPendingKillActor.Num() - 4 : 0;
	LastIndex = !GIsEditor ? Max<INT>(LastIndex, 0) : 0;
	// Clean up any deferred actors.
	for(INT i = GNovodexPendingKillActor.Num() - 1; i >= LastIndex; i--)
	{
		NxActor* nActor = GNovodexPendingKillActor(i);
		check(nActor);
		if (bCalledFromTick)
		{
			GNovodexPendingKillActor.RemoveSwap(i);
		}
		NxScene& nScene = nActor->getScene();
		nScene.releaseActor(*nActor);
	}
	if (!bCalledFromTick)
	{
		GNovodexPendingKillActor.Empty();
	}

	LastIndex = bCalledFromTick ? GNovodexPendingKillJoint.Num() - 1 : 0;
	LastIndex = Max<INT>(LastIndex, 0);
	// Clean up any deferred joints.
	for(INT i = GNovodexPendingKillJoint.Num() - 1; i >= LastIndex; i--)
	{
		NxJoint* Joint = GNovodexPendingKillJoint(i);
		check(Joint);
		if (bCalledFromTick)
		{
			// Remove it from the array... should be fast - it's the last one!
			GNovodexPendingKillJoint.RemoveSwap(i);
		}
		NxScene& Scene = Joint->getScene();
		Scene.releaseJoint(*Joint);
	}
	if (!bCalledFromTick)
	{
		GNovodexPendingKillJoint.Empty();
	}
#if !NX_DISABLE_CLOTH
	// Clean up any Cloths in the 'pending kill' array. (Note: to be done before deleting cloth meshes)
	NxScene* nxScene = PhysScene->GetNovodexPrimaryScene();
	// Work around PhysX cloth crash
	if (GNovodexPendingKillCloths.Num() > 0)
	{
		// crash in MirrorManager::onNewClientAABB if cloth is created & released in same simulation step
		// deleted id's need to be fetched before the next fetchResult, otherwise they get lost
#if !defined(NX_DISABLE_FLUIDS)
		PhysScene->PhysXEmitterManager->PreSyncPhysXData();
#endif
		nxScene->simulate(0);
		nxScene->fetchResults(NX_ALL_FINISHED, true);
#if !defined(NX_DISABLE_FLUIDS)
		PhysScene->PhysXEmitterManager->PostSyncPhysXData();
#endif
	}

	for(INT i=0; i<GNovodexPendingKillCloths.Num(); i++)
	{
		NxCloth* nCloth = GNovodexPendingKillCloths(i);
		check(nCloth);
		
		NxScene& nxScene = nCloth->getScene();

		nxScene.releaseCloth(*nCloth);
	}
	GNovodexPendingKillCloths.Empty();
#endif	// !NX_DISABLE_CLOTH

	// Clean up any force fields in the 'pending kill' array.
	for(INT i=0; i<GNovodexPendingKillForceFields.Num(); i++)
	{
		UserForceField* forceField = GNovodexPendingKillForceFields(i);
		check(forceField);
		forceField->Destroy();
	}
	GNovodexPendingKillForceFields.Empty();


	// Clean up any force field kernels in the 'pending kill' array.
	for(INT i=0; i<GNovodexPendingKillForceFieldLinearKernels.Num(); i++)
	{
		UserForceFieldLinearKernel* kernel = GNovodexPendingKillForceFieldLinearKernels(i);
		check(kernel);
		kernel->Destroy();
	}
	GNovodexPendingKillForceFieldLinearKernels.Empty();

	// Clean up any force field shape groups in the 'pending kill' array.
	for(INT i=0; i<GNovodexPendingKillForceFieldShapeGroups.Num(); i++)
	{
		UserForceFieldShapeGroup* group = GNovodexPendingKillForceFieldShapeGroups(i);
		check(group);
		group->Destroy();
	}
	GNovodexPendingKillForceFieldShapeGroups.Empty();

	if (GNovodexPendingKillActor.Num() || GNovodexPendingKillJoint.Num())
	{
		// We still have pending actors, so don't try to free shapes
		return;
	}

	// Clean up any convex meshes in the 'pending kill' array.
	INT ConvexIndex = 0;
	while(ConvexIndex<GNovodexPendingKillConvex.Num())
	{
		NxConvexMesh* ConvexMesh = GNovodexPendingKillConvex(ConvexIndex);
		check(ConvexMesh);

		// We check that nothing is still using this Convex Mesh.
		INT Refs = ConvexMesh->getReferenceCount();
		if(Refs > DelayNxMeshDestruction)
		{
			debugf( TEXT("WARNING: Release aborted - ConvexMesh still in use!") ); // Wish I could give more info...
			GNovodexPendingKillConvex.Remove(ConvexIndex);
		}
		// Not ready to destroy yet - decrement ref count
		else if(Refs > 0)
		{
			SetNxConvexMeshRefCount(ConvexMesh, Refs-1);
			ConvexIndex++;
		}
		// Ref count is zero - release the mesh.
		else
		{
#if USE_QUICKLOAD_CONVEX
			GNovodeXQuickLoad->releaseConvexMesh(*ConvexMesh);
#else
			GNovodexSDK->releaseConvexMesh(*ConvexMesh);
#endif
			GNumPhysXConvexMeshes--;
			GNovodexPendingKillConvex.Remove(ConvexIndex);
		}
	}

	// Clean up any triangle meshes in the 'pending kill' array.
	INT TriIndex = 0;
	while(TriIndex<GNovodexPendingKillTriMesh.Num())
	{
		NxTriangleMesh* TriMesh = GNovodexPendingKillTriMesh(TriIndex);
		check(TriMesh);

		// We check that nothing is still using this Triangle Mesh.
		INT Refs = TriMesh->getReferenceCount();
		if(Refs > DelayNxMeshDestruction)
		{
			debugf( TEXT("WARNING: Release aborted - TriangleMesh still in use!") ); // Wish I could give more info...
			GNovodexPendingKillTriMesh.Remove(TriIndex);
		}
		// Not ready to destroy yet - decrement ref count
		else if(Refs > 0)
		{
			SetNxTriMeshRefCount(TriMesh, Refs-1);
			TriIndex++;
		}
		// Ref count is zero - release the mesh.
		else
		{
			GNovodexSDK->releaseTriangleMesh(*TriMesh);
			GNumPhysXTriMeshes--;
			GNovodexPendingKillTriMesh.Remove(TriIndex);
		}
	}

	// Clean up any heightfields in the 'pending kill' array.
	for(INT i=0; i<GNovodexPendingKillHeightfield.Num(); i++)
	{
		NxHeightField* HF = GNovodexPendingKillHeightfield(i);
		check(HF);
		// During level transition, a stray reference can be still active due to the destruction order of
		// the BodyInstance and the CollisionComponent. In this case we skip the cleanup and we will do it
		// next time.
		if( HF->getReferenceCount() == 0 )
		{
			GNovodexSDK->releaseHeightField(*HF);
			GNovodexPendingKillHeightfield.RemoveSwap( i-- );
		}
	}

	// Clean up any CCD skeletons in the 'pending kill' array.
	for(INT i=0; i<GNovodexPendingKillCCDSkeletons.Num(); i++)
	{
		NxCCDSkeleton* CCDSkel = GNovodexPendingKillCCDSkeletons(i);
		check(CCDSkel);

		// Shouldn't need to check ref count - only Shapes use these.
		GNovodexSDK->releaseCCDSkeleton(*CCDSkel);
	}
	GNovodexPendingKillCCDSkeletons.Empty();

#if !NX_DISABLE_CLOTH
	// Clean up any cloth meshes in the 'pending kill' array.
	for(INT i=0; i<GNovodexPendingKillClothMesh.Num(); i++)
	{
		NxClothMesh* ClothMesh = GNovodexPendingKillClothMesh(i);
		check(ClothMesh);

		// We check that nothing is still using this Cloth Mesh.
		INT Refs = ClothMesh->getReferenceCount();
		if(Refs > 0)
		{
			debugf( TEXT("WARNING: Release aborted - ClothMesh still in use!") ); // Wish I could give more info...
		}

		GNovodexSDK->releaseClothMesh(*ClothMesh);		
	}
	GNovodexPendingKillClothMesh.Empty();
#endif // !NX_DISABLE_CLOTH

#if !NX_DISABLE_SOFTBODY
	// Clean up any SoftBody meshes in the 'pending kill' array.
	for(INT i=0; i<GNovodexPendingKillSoftBodyMesh.Num(); i++)
	{
		NxSoftBodyMesh* SoftBodyMesh = GNovodexPendingKillSoftBodyMesh(i);
		check(SoftBodyMesh);

		// We check that nothing is still using this Cloth Mesh.
		INT Refs = SoftBodyMesh->getReferenceCount();
		if(Refs > 0)
		{
			debugf( TEXT("WARNING: Release aborted - SoftBodyMesh still in use!") ); // Wish I could give more info...
		}

		GNovodexSDK->releaseSoftBodyMesh(*SoftBodyMesh);
	}
	GNovodexPendingKillSoftBodyMesh.Empty();
#endif // !NX_DISABLE_SOFTBODY


#endif // WITH_NOVODEX

}

/** Executes performanec statistics code for physics */
static void StatRBPhysScene(FRBPhysScene* Scene)
{
	SCOPE_CYCLE_COUNTER(STAT_PhysicsOutputStats);

#if WITH_NOVODEX
	NxScene *NovodexScene = 0;
	if( Scene )
	{
		NovodexScene = Scene->GetNovodexPrimaryScene();
	}
	if( !NovodexScene )
	{
		return;
   	}

#if STATS
	if( !NovodexScene )
	{
		return;
   	}

	// Only do stats when the game is actually running (this includes PIE though)
	if(GIsGame)
	{
		//GStatChart->AddDataPoint( FString(TEXT("PhysFetch")), (FLOAT)(GPhysicsStats.RBKickOffDynamicsTime.Value * GSecondsPerCycle * 1000.0) );

		// Show the average substep time as well.
		const FCycleStat* Total = GStatManager.GetCycleStat(STAT_RBTotalDynamicsTime);
		DWORD TotalCycles = Total ? Total->Cycles : 0;
		INT NumSubSteps = Scene->NumSubSteps > 0 ? Scene->NumSubSteps : 1;
		SET_CYCLE_COUNTER(STAT_RBSubstepTime, TotalCycles/NumSubSteps, 1);

#if LOOKING_FOR_PERF_ISSUES
		FLOAT PhysTimeMs = TotalCycles * GSecondsPerCycle * 1000;

		// Log detailed stats for any frame with physics times > 10 ms
		if( PhysTimeMs > 10.f )
		{
			debugf(TEXT("SLOW PHYSICS FRAME: %f ms"), PhysTimeMs);
			bOutputAllStats = TRUE;
		}
#endif

		// Get stats for software and, if present, hardware scene.
		NxSceneStats SWStats;
		NovodexScene->getStats(SWStats);

		if( bOutputAllStats )
		{
			debugf(NAME_PerfWarning,TEXT("---------------- Dumping all physics stats ----------------"));
			debugf(TEXT("Num Dynamic Actors: %d"), SWStats.numDynamicActors);
			debugf(TEXT("Num Awake Actors: %d"), SWStats.numDynamicActorsInAwakeGroups);
			debugf(TEXT("Solver Bodies: %d"), SWStats.numSolverBodies);
			debugf(TEXT("Num Pairs: %d"), SWStats.numPairs);
			debugf(TEXT("Num Contacts: %d"), SWStats.numContacts);
			debugf(TEXT("Num Joints: %d"), SWStats.numJoints);
		}

		SET_DWORD_STAT(STAT_TotalSWDynamicBodies,SWStats.numDynamicActors);
		SET_DWORD_STAT(STAT_AwakeSWDynamicBodies,SWStats.numDynamicActorsInAwakeGroups);
		SET_DWORD_STAT(STAT_SWSolverBodies,SWStats.numSolverBodies);
		SET_DWORD_STAT(STAT_SWNumPairs,SWStats.numPairs);
		SET_DWORD_STAT(STAT_SWNumContacts,SWStats.numContacts);
		SET_DWORD_STAT(STAT_SWNumJoints,SWStats.numJoints);
		SET_DWORD_STAT(STAT_NumConvexMeshes,GNumPhysXConvexMeshes);
		SET_DWORD_STAT(STAT_NumTriMeshes,GNumPhysXTriMeshes);
		SET_CYCLE_COUNTER(STAT_RBNearphase, 0, 0);
		SET_CYCLE_COUNTER(STAT_RBBroadphaseUpdate, 0, 0);
		SET_CYCLE_COUNTER(STAT_RBBroadphaseGetPairs, 0, 0);
		SET_CYCLE_COUNTER(STAT_RBSolver, 0, 0);
		SET_CYCLE_COUNTER(STAT_HWTimeIncludingGpuWait, 0, 0);
		SET_CYCLE_COUNTER(STAT_NovodexAllocatorTime, FNxAllocator::CallCycles, FNxAllocator::CallCount);
		FNxAllocator::CallCycles = 0;
		FNxAllocator::CallCount	= 0;

#if WITH_APEX_GRB
		const physx::apex::NxModuleDestructible *DestructibleModule = GApexManager ? GApexManager->GetModuleDestructible() : 0;
		if( DestructibleModule )
		{
			physx::PxI32	numActors	    = 0;
			physx::PxI32	numPairs	    = 0;
			physx::PxI32	numContacts	    = 0;
			physx::PxI32	numConvexes	    = 0;
			physx::PxF32	grbSceneMemUsed	= 0;
			physx::PxF32	grbTempMemUsed	= 0;
			physx::PxF32	grbTime	        = 0;
			if ( DestructibleModule->isGrbSimulationEnabled(*Scene->GetApexScene()) )
			{
				const physx::NxApexSceneStats* stats = Scene->GetApexScene()->getStats();		

				for (physx::PxU32 i = 0; i < stats->numApexStats; i++)
				{
					if ( strcmp(stats->ApexStatsInfoPtr[i].StatName, "NumberOfActors") == 0)
						numActors	= stats->ApexStatsInfoPtr[i].StatCurrentValue.Int;
					else if ( strcmp(stats->ApexStatsInfoPtr[i].StatName, "NumberOfGpuShapePairs") == 0)
						numPairs	= stats->ApexStatsInfoPtr[i].StatCurrentValue.Int;
					else if ( strcmp(stats->ApexStatsInfoPtr[i].StatName, "NumberOfGpuContacts") == 0)
						numContacts	= stats->ApexStatsInfoPtr[i].StatCurrentValue.Int;
					else if ( strcmp(stats->ApexStatsInfoPtr[i].StatName, "NumberOfShapes") == 0)
						numConvexes	= stats->ApexStatsInfoPtr[i].StatCurrentValue.Int;
					else if ( strcmp(stats->ApexStatsInfoPtr[i].StatName, "GpuSceneMemoryUsed (MB)") == 0)
						grbSceneMemUsed = stats->ApexStatsInfoPtr[i].StatCurrentValue.Float;
					else if ( strcmp(stats->ApexStatsInfoPtr[i].StatName, "GpuTempMemoryUsed (MB)") == 0)
						grbTempMemUsed = stats->ApexStatsInfoPtr[i].StatCurrentValue.Float;
					else if ( strcmp(stats->ApexStatsInfoPtr[i].StatName, "GpuRbSimulationTime") == 0)
						grbTime = stats->ApexStatsInfoPtr[i].StatCurrentValue.Float;
				}
			}

			SET_DWORD_STAT(STAT_HWSolverBodies,numActors);
			SET_DWORD_STAT(STAT_HWNumPairs,numPairs);
			SET_DWORD_STAT(STAT_HWNumContacts,numContacts);
			SET_DWORD_STAT(STAT_HWNumConvexMeshes,numConvexes);
			SET_FLOAT_STAT(STAT_HWSceneMemoryUsed,grbSceneMemUsed);
			SET_FLOAT_STAT(STAT_HWTempMemoryUsed,grbTempMemUsed);
			SET_CYCLE_COUNTER(STAT_HWTimeIncludingGpuWait,(grbTime/1000.0f)/GSecondsPerCycle,1);
		}
#endif

#ifdef NX_ENABLE_SCENE_STATS2		
		SET_DWORD_STAT( STAT_TotalFluids, FindNovodexSceneStat(NovodexScene, TEXT("TotalFluids"), FALSE) );
		SET_DWORD_STAT( STAT_TotalFluidEmitters, FindNovodexSceneStat(NovodexScene, TEXT("TotalFluidEmitters"), FALSE) );
		SET_DWORD_STAT( STAT_ActiveFluidParticles, FindNovodexSceneStat(NovodexScene, TEXT("ActiveFluidParticles"), FALSE) );
		SET_DWORD_STAT( STAT_TotalFluidParticles, FindNovodexSceneStat(NovodexScene, TEXT("TotalFluidParticles"), FALSE) );
		SET_DWORD_STAT( STAT_TotalFluidPackets, FindNovodexSceneStat(NovodexScene, TEXT("TotalFluidPackets"), FALSE) );

		SET_DWORD_STAT( STAT_TotalCloths, FindNovodexSceneStat(NovodexScene, TEXT("TotalCloths"), FALSE) );
		SET_DWORD_STAT( STAT_ActiveCloths, FindNovodexSceneStat(NovodexScene, TEXT("ActiveCloths"), FALSE) );
		SET_DWORD_STAT( STAT_ActiveClothVertices, FindNovodexSceneStat(NovodexScene, TEXT("ActiveClothVertices"), FALSE) );
		SET_DWORD_STAT( STAT_TotalClothVertices, FindNovodexSceneStat(NovodexScene, TEXT("TotalClothVertices"), FALSE) );
		SET_DWORD_STAT( STAT_ActiveAttachedClothVertices, FindNovodexSceneStat(NovodexScene, TEXT("ActiveAttachedClothVertices"), FALSE) );
		SET_DWORD_STAT( STAT_TotalAttachedClothVertices, FindNovodexSceneStat(NovodexScene, TEXT("TotalAttachedClothVertices"), FALSE) );

		SET_DWORD_STAT(STAT_PhysicsGpuMemTotal, FindNovodexSceneStat(NovodexScene, TEXT("GpuHeapUsageTotal"), TRUE) );
		SET_DWORD_STAT(STAT_PhysicsGpuMemFluid, FindNovodexSceneStat(NovodexScene, TEXT("GpuHeapUsageFluid"), TRUE) );
		SET_DWORD_STAT(STAT_PhysicsGpuMemClothSoftBody, FindNovodexSceneStat(NovodexScene, TEXT("GpuHeapUsageDeformable"), TRUE) );
		SET_DWORD_STAT(STAT_PhysicsGpuMemShared, FindNovodexSceneStat(NovodexScene, TEXT("GpuHeapUsageUtils"), TRUE) );

#endif //NX_ENABLE_SCENE_STATS2

#if !PS3
		const NxProfileData* nProfileData = NovodexScene->readProfileData(true);
		if (nProfileData != NULL)
		{		
			for(UINT i=0; i<nProfileData->numZones; i++)
			{
				NxProfileZone* nZone = nProfileData->profileZones + i;
				FANSIToTCHAR ZoneName(nZone->name);

				FCycleStat* Stat = NULL;
				if( appStrcmp(TEXT("PrBroadphase_NarrowPhase"), ZoneName) == 0 )
				{
					Stat = (FCycleStat*)GStatManager.GetCycleStat(STAT_RBNearphase);
				}
				else if( appStrcmp(TEXT("PrBroadphase_Update"), ZoneName) == 0 )
				{
					Stat = (FCycleStat*)GStatManager.GetCycleStat(STAT_RBBroadphaseUpdate);
				}
				else if( appStrcmp(TEXT("PrBroadphase_GetPairs"), ZoneName) == 0 )
				{
					Stat = (FCycleStat*)GStatManager.GetCycleStat(STAT_RBBroadphaseGetPairs);
				}
				else if( appStrcmp(TEXT("PrSolver"), ZoneName) == 0 )
				{
					Stat = (FCycleStat*)GStatManager.GetCycleStat(STAT_RBSolver);
				}

				if( Stat )
				{
					Stat->NumCallsPerFrame += nZone->callCount;
					Stat->Cycles += (DWORD) (((DOUBLE)nZone->hierTime) / (1000000.0 * GSecondsPerCycle));
				}

				if( bOutputAllStats && (nZone->hierTime > 1.f) )
				{
					// Don't output stats which are zero.
					if(nZone->hierTime > 1.f)
					{
						debugf(NAME_PerfWarning, TEXT("%d %d %s T: %6.3f ms"), i, nZone->recursionLevel, (TCHAR*)ZoneName, nZone->hierTime / 1000.f );
					}
				}
			}
		}
#endif // !CONSOLE

		bOutputAllStats = FALSE;
	}
#endif

#endif // WITH_NOVODEX
}

/** Exposes ticking of physics-engine scene outside Engine. */
void TickRBPhysScene(FRBPhysScene* Scene, FLOAT DeltaTime)
{
#if WITH_NOVODEX
	SCOPE_CYCLE_COUNTER(STAT_RBTotalDynamicsTime);


#if SUPPORT_DOUBLE_BUFFERING
	// put this here to make sure it is called from all codepaths (game, PhAT, ...)
	if (Scene->UsingBufferedScene)
	{
		WaitPhysCompartments(Scene);
	}
	#endif

#if STATS && !FINAL_RELEASE
	// Render debug info.
	if(Scene->UsingBufferedScene)
	{
		Scene->AddNovodexDebugLines(GWorld->LineBatcher);
	}

	if( GStatManager.GetGroup(STATGROUP_Physics)->bShowGroup ||
		GStatManager.GetGroup(STATGROUP_PhysicsCloth)->bShowGroup ||
		GStatManager.GetGroup(STATGROUP_PhysicsFluid)->bShowGroup ||
		GStatManager.GetGroup(STATGROUP_PhysicsGpuMem)->bShowGroup )
	{
		// do async safe statistics.
		StatRBPhysScene(Scene);
	}
#endif // !FINAL_RELEASE

#if !defined(NX_DISABLE_FLUIDS)
	Scene->PhysXEmitterManager->PreSyncPhysXData();
#endif

	check( GWorld && GWorld->GetWorldInfo() );
	AWorldInfo * Info = GWorld->GetWorldInfo();
	check(Info);
	FPhysXSceneProperties & SceneProperties = Info->PhysicsProperties;

	FLOAT UseDelta;
	if(Info->bPhysicsIgnoreDeltaTime)
	{
		UseDelta = 0.033f * Info->TimeDilation;
	}
	else
	{
		/** 
		* clamp down... if this happens we are simming physics slower than real-time, so be careful with it.
		* it can improve framerate dramatically (really, it is the same as scaling all velocities down and
		 * enlarging all timesteps) but at the same time, it will screw with networking (client and server will
		* diverge a lot more.)
		*/
		UseDelta = ::Min(DeltaTime, Info->MaxPhysicsDeltaTime);
	}

	FLOAT MaxSubstep = SceneProperties.PrimaryScene.TimeStep;
	INT MaxSubsteps = ::Min(SceneProperties.PrimaryScene.MaxSubSteps, Info->MaxPhysicsSubsteps);
	UBOOL bUseFixedTimestep = SceneProperties.PrimaryScene.bFixedTimeStep;

	INT NumSubSteps;
	if(bUseFixedTimestep)
	{
		NumSubSteps = MaxSubsteps;
		MaxSubstep *= Info->TimeDilation;
	}
	else
	{
		NumSubSteps = appCeil(UseDelta/MaxSubstep);
		NumSubSteps = Clamp<INT>(NumSubSteps, 1, MaxSubsteps);
		MaxSubstep = ::Clamp(UseDelta/NumSubSteps, 0.0025f, MaxSubstep);
	}

	Scene->NumSubSteps = NumSubSteps;
	SET_DWORD_STAT(STAT_NumSubsteps, NumSubSteps);

	// Make sure list of queued collision notifications is empty.
	Scene->PendingCollisionNotifies.Empty();

	NxScene *NovodexScene = Scene->GetNovodexPrimaryScene();;
	check(NovodexScene);
	if(NovodexScene)
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysicsKickOffDynamicsTime);

		// Debug printing of what timestep settings we are using
		if(FALSE)
		{
			debugf(TEXT("UseDelta: %f MaxSubstep: %f NumSubSteps: %d"), UseDelta, MaxSubstep, NumSubSteps);
		}

		NovodexScene->setTiming( MaxSubstep, NumSubSteps );
		NxScenePair *ScenePair = GetNovodexScenePairFromIndex(Scene->NovodexSceneIndex);
		INT RunRigidBody = 1;
		INT RunFluid = 1;
		INT RunCloth = 1;
		INT RunSoftBody = 1;
		if(Info->CompartmentRunFrames.Num() > 0)
		{
			check(Scene->CompartmentFrameNumber >= 0);
			if(Scene->CompartmentFrameNumber >= Info->CompartmentRunFrames.Num())
			{
				Scene->CompartmentFrameNumber = 0;
			}
			const FCompartmentRunList & RunList = Info->CompartmentRunFrames(Scene->CompartmentFrameNumber++);
			RunRigidBody = RunList.RigidBody ? 1 : 0;
			RunFluid = RunList.Fluid ? 1 : 0;
			RunCloth = RunList.Cloth ? 1 : 0;
			RunSoftBody = RunList.SoftBody ? 1 : 0;
		}
		if(ScenePair)
		{
			if(ScenePair->RigidBodyCompartment)
			{
				FLOAT MaxTimeStep	= SceneProperties.CompartmentRigidBody.TimeStep;
				INT	MaxAllowedIters = ::Min(SceneProperties.CompartmentRigidBody.MaxSubSteps, Info->MaxPhysicsSubsteps);
				INT MaxIters		= MaxAllowedIters;
				if(!SceneProperties.CompartmentRigidBody.bFixedTimeStep)
				{
					MaxIters    = appCeil(UseDelta/MaxTimeStep);
					MaxIters    = Clamp<INT>(MaxIters, 1, MaxAllowedIters);
					MaxTimeStep = Clamp(UseDelta/MaxIters, 0.0025f, MaxTimeStep);
				}
				ScenePair->RigidBodyCompartment->setTiming(MaxTimeStep, MaxIters*RunRigidBody);
			}
			if(ScenePair->FluidCompartment)
			{
				FLOAT MaxTimeStep	= SceneProperties.CompartmentFluid.TimeStep;
				INT	MaxAllowedIters = ::Min(SceneProperties.CompartmentFluid.MaxSubSteps, Info->MaxPhysicsSubsteps);
				INT MaxIters		= MaxAllowedIters;
				if(!SceneProperties.CompartmentFluid.bFixedTimeStep)
				{
					MaxIters    = appCeil(UseDelta/MaxTimeStep);
					MaxIters    = Clamp<INT>(MaxIters, 1, MaxAllowedIters);
					MaxTimeStep = Clamp(UseDelta/MaxIters, 0.0025f, MaxTimeStep);
				}
				ScenePair->FluidCompartment->setTiming(MaxTimeStep, MaxIters*RunFluid);
			}
			if(ScenePair->ClothCompartment)
			{
				FLOAT MaxTimeStep	= SceneProperties.CompartmentCloth.TimeStep;
				INT	MaxAllowedIters = ::Min(SceneProperties.CompartmentCloth.MaxSubSteps, Info->MaxPhysicsSubsteps);
				INT MaxIters		= MaxAllowedIters;
				if(!SceneProperties.CompartmentCloth.bFixedTimeStep)
				{
					MaxIters    = appCeil(UseDelta/MaxTimeStep);
					MaxIters    = Clamp<INT>(MaxIters, 1, MaxAllowedIters);
					MaxTimeStep = Clamp(UseDelta/MaxIters, 0.0025f, MaxTimeStep);
				}
				ScenePair->ClothCompartment->setTiming(MaxTimeStep, MaxIters*RunCloth);
			}
			if(ScenePair->SoftBodyCompartment && ScenePair->SoftBodyCompartment != ScenePair->ClothCompartment)
			{
				FLOAT MaxTimeStep	= SceneProperties.CompartmentSoftBody.TimeStep;
				INT	MaxAllowedIters = ::Min(SceneProperties.CompartmentSoftBody.MaxSubSteps, Info->MaxPhysicsSubsteps);
				INT MaxIters		= MaxAllowedIters;
				if(!SceneProperties.CompartmentSoftBody.bFixedTimeStep)
				{
					MaxIters    = appCeil(UseDelta/MaxTimeStep);
					MaxIters    = Clamp<INT>(MaxIters, 1, MaxAllowedIters);
					MaxTimeStep = Clamp(UseDelta/MaxIters, 0.0025f, MaxTimeStep);
				}
				ScenePair->SoftBodyCompartment->setTiming(MaxTimeStep, MaxIters*RunSoftBody);
			}
		}
#if WITH_APEX
		if ( Scene->ApexScene )
		{
#if STATS && !FINAL_RELEASE
			// Read updated stats from Apex scene
			if( GStatManager.GetGroup(STATGROUP_Apex)->bShowGroup )
			{
				Scene->ApexScene->UpdateStats();
			}
#endif
			
			// Provide unclamped DeltaTime to APEX. PhysX simulation will not be
			// affected, as its simulation time is defined by the setTiming settings,
			// it will simulate MaxSubstep*NumSubSteps at most.
			// However APEX Clothing needs the whole step to avoid wrong velocities.
			Scene->ApexScene->Simulate(DeltaTime);
		}
		else
#endif
		{
			NovodexScene->simulate(UseDelta);
		}
		Scene->CompartmentsRunning = TRUE;
	}

	//GStatChart->AddDataPoint( FString(TEXT("PhysKickoff")), (FLOAT)(GPhysicsStats.RBKickOffDynamicsTime.Value * GSecondsPerCycle * 1000.0) );
#endif // WITH_NOVODEX
}

#if WITH_NOVODEX
void FetchNovodexResults( NxScene* NovodexScene, NxSimulationStatus StatusCondition, DOUBLE Timeout )
{
#if DO_CHECK || FINAL_RELEASE_DEBUGCONSOLE
	DOUBLE StartTime = appSeconds();
	UBOOL bOk = NovodexScene->fetchResults( StatusCondition, FALSE );
	while ( !bOk )
	{
		DOUBLE CurrentTime = appSeconds();
		if ( (CurrentTime - StartTime) > Timeout )
		{
			// Restart timer
			Timeout = 10.0;
			StartTime = CurrentTime;
#if PS3
			printf( "--- PhysX crash/hang detected! ---\n" );
#endif
			// Report the timeout
			debugf( TEXT("Timed out while waiting for PhysX results!") );
		}
		appSleep( 0.0f );	// Yield for a tiny time
		bOk = NovodexScene->fetchResults( StatusCondition, FALSE );
	}
#else
	NovodexScene->fetchResults( StatusCondition, TRUE );
#endif
}
#endif // WITH_NOVODEX

/**
 * Waits for the scene to be done processing and fires off any physics events
 */
void WaitRBPhysScene(FRBPhysScene* Scene)
{
#if WITH_NOVODEX
	SCOPE_CYCLE_COUNTER(STAT_RBTotalDynamicsTime);
	SCOPE_CYCLE_COUNTER(STAT_PhysicsWaitTime);
	NxScene *NovodexScene = Scene->GetNovodexPrimaryScene();
	check(NovodexScene);
	if(NovodexScene)
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysicsFetchDynamicsTime);

		if(Scene->UsingBufferedScene)
		{
			FetchNovodexResults( NovodexScene, NX_PRIMARY_FINISHED, 5.0 );
		}
		else
		{
#if WITH_APEX
			if ( Scene->ApexScene )
			{
				Scene->ApexScene->FetchResults(TRUE,NULL);
			}
			else
#endif
			{
				FetchNovodexResults( NovodexScene, NX_ALL_FINISHED, 5.0 );
			}
			Scene->CompartmentsRunning = FALSE;
		}
#if !defined(NX_DISABLE_FLUIDS)
		Scene->PhysXEmitterManager->PostSyncPhysXData();
#endif
	}

#endif // WITH_NOVODEX
}


/**
 * Waits for the scene compartments to be done processing and fires off any physics events
 */
void WaitPhysCompartments(FRBPhysScene* Scene)
{
#if WITH_NOVODEX
	if(Scene->CompartmentsRunning)
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysicsWaitTime);
		NxScene *NovodexScene = Scene->GetNovodexPrimaryScene();
		check(NovodexScene);
		if(NovodexScene)
		{
			SCOPE_CYCLE_COUNTER(STAT_RBTotalDynamicsTime);
			SCOPE_CYCLE_COUNTER(STAT_PhysicsFetchDynamicsTime);
#if WITH_APEX
			if ( Scene->ApexScene )
			{
				Scene->ApexScene->FetchResults(TRUE,NULL);
			}
			else
#endif
			{
				NovodexScene->fetchResults(NX_ALL_FINISHED, true);
			}
		}
		Scene->CompartmentsRunning = FALSE;
	}
#endif // WITH_NOVODEX
}


/** 
 *	Call after WaitRBPhysScene to make deferred OnRigidBodyCollision calls. 
 *
 * @param RBPhysScene - the scene to process deferred collision events
 */
void DispatchRBCollisionNotifies(FRBPhysScene* RBPhysScene)
{
	SCOPE_CYCLE_COUNTER(STAT_PhysicsEventTime);

	// Fire any collision notifies in the queue.
	for(INT i=0; i<RBPhysScene->PendingCollisionNotifies.Num(); i++)
	{
		FCollisionNotifyInfo& NotifyInfo = RBPhysScene->PendingCollisionNotifies(i);
		if(NotifyInfo.RigidCollisionData.ContactInfos.Num() > 0)
		{
			if(NotifyInfo.bCallEvent0 && NotifyInfo.Info0.Actor && NotifyInfo.IsValidForNotify())
			{
				NotifyInfo.Info0.Actor->OnRigidBodyCollision(NotifyInfo.Info0, NotifyInfo.Info1, NotifyInfo.RigidCollisionData);
			}

			// Need to check IsValidForNotify again in case first call broke something.
			if(NotifyInfo.bCallEvent1 && NotifyInfo.Info1.Actor && NotifyInfo.IsValidForNotify())
			{
				NotifyInfo.RigidCollisionData.SwapContactOrders();
				NotifyInfo.Info1.Actor->OnRigidBodyCollision(NotifyInfo.Info1, NotifyInfo.Info0, NotifyInfo.RigidCollisionData);
			}
		}
	}
	RBPhysScene->PendingCollisionNotifies.Empty();

	// Fire any pushing notifications.
	for(INT i=0; i<RBPhysScene->PendingPushNotifies.Num(); i++)
	{
		FPushNotifyInfo& PushInfo = RBPhysScene->PendingPushNotifies(i);
		if(PushInfo.Pusher && !PushInfo.Pusher->bDeleteMe)
		{
			PushInfo.Pusher->ProcessPushNotify(PushInfo.PushedInfo, PushInfo.ContactInfos);
		}
	}
	RBPhysScene->PendingPushNotifies.Empty();
}

void FRBPhysScene::SetGravity(const FVector& NewGrav)
{
#if WITH_NOVODEX
	NxVec3 nGravity = U2NPosition(NewGrav);
	NxScene* NovodexScene = GetNovodexPrimaryScene();
	if(NovodexScene)
	{
		NovodexScene->setGravity(nGravity);
	}
#endif
}

UINT FRBPhysScene::FindPhysMaterialIndex(UPhysicalMaterial* PhysMat)
{
#if WITH_NOVODEX
	// Find the name of the PhysicalMaterial
	FName PhysMatName = PhysMat->GetFName();

	// Search for name in map.
	UINT* MatIndexPtr = MaterialMap.Find(PhysMatName);

	// If present, just return it.
	if(MatIndexPtr)
	{
		return *MatIndexPtr;
	}
	// If not, add to mapping.
	else
	{
		NxScene* PrimaryScene = GetNovodexPrimaryScene();
		if(!PrimaryScene)
		{
			return 0;
		}

		UINT NewMaterialIndex = 0;

		// Recycle NxMaterials
		if(UnusedMaterials.Num() > 0)
		{
			NewMaterialIndex = UnusedMaterials.Pop();
		}
		else
		{
			NxMaterialDesc MatDesc;
			NxMaterial * NewMaterial = PrimaryScene->createMaterial(MatDesc);
			NewMaterialIndex = NewMaterial->getMaterialIndex();
		}

		NxMaterial * NewMaterial = PrimaryScene->getMaterialFromIndex(NewMaterialIndex);

		NewMaterial->setRestitution(PhysMat->Restitution);
		NewMaterial->setStaticFriction(PhysMat->Friction);
		NewMaterial->setDynamicFriction(PhysMat->Friction);
		NewMaterial->setFrictionCombineMode(NX_CM_MULTIPLY);
		NewMaterial->setRestitutionCombineMode(NX_CM_MAX);

		// Anisotropic friction support
		if(PhysMat->bEnableAnisotropicFriction)
		{
			NewMaterial->setFlags( NewMaterial->getFlags() | NX_MF_ANISOTROPIC );
		}
		else
		{
			NewMaterial->setFlags( NewMaterial->getFlags() & ~NX_MF_ANISOTROPIC );
		}

		NewMaterial->setStaticFrictionV(PhysMat->FrictionV);
		NewMaterial->setDynamicFrictionV(PhysMat->FrictionV);
		NewMaterial->setDirOfAnisotropy(U2NVectorCopy(PhysMat->AnisoFrictionDir));

		NewMaterial->userData = PhysMat;

		// Add the newly created material to the table.
		MaterialMap.Set(PhysMatName, NewMaterialIndex);

		return NewMaterialIndex;
	}
#else
	return 0;
#endif
}

/** When a UPhysicalMaterial goes away, remove from mapping and discard NxMaterial. */
void FRBPhysScene::RemovePhysMaterial(UPhysicalMaterial* PhysMat)
{
	// Find the name of the PhysicalMaterial
	const FName PhysMatName = PhysMat->GetFName();

	// Search for name in map.
	const UINT* const MatIndexPtr = MaterialMap.Find(PhysMatName);
	if(MatIndexPtr)
	{

#if WITH_NOVODEX
		NxScene* PrimaryScene = GetNovodexPrimaryScene();
		if(PrimaryScene)
		{
			// Find the NxMaterial
			const INT MatIndex = *MatIndexPtr;
			NxMaterial* const nMaterial = PrimaryScene->getMaterialFromIndex(MatIndex);
			if(nMaterial)
			{
				// Clear pointer from NxMaterial to PhysicalMaterial
				nMaterial->userData = NULL;
				// And put material back into unused material array
				UnusedMaterials.AddItem(MatIndex);
			}
		}
#endif // WITH_NOVODEX

		// Remove from map
		MaterialMap.Remove(PhysMatName);
	}
}

IMPLEMENT_COMPARE_POINTER(UStaticMesh, UnPhysLevel, { return((B->BodySetup->CollisionGeom.Num() * B->BodySetup->AggGeom.ConvexElems.Num()) - (A->BodySetup->CollisionGeom.Num() * A->BodySetup->AggGeom.ConvexElems.Num())); } );
#if WITH_NOVODEX
#ifdef _DEBUG
IMPLEMENT_COMPARE_CONSTREF(FPhysXAlloc, UnPhysLevel, { return strcmp(A.Filename,B.Filename); } );
#endif
#endif	//#if WITH_NOVODEX

//// EXEC
UBOOL ExecRBCommands(const TCHAR* Cmd, FOutputDevice* Ar)
{
#if WITH_NOVODEX

#if WITH_APEX
	if ( GApexCommands && GApexCommands->IsApexCommand(TCHAR_TO_ANSI(Cmd)) )
	{
		// stall all scenes to make sure we don't simulate when calling setParameter
		NxU32 NbScenes = GNovodexSDK->getNbScenes();
		for (NxU32 i = 0; i < NbScenes; i++)
		{
			NxScene *nScene = GNovodexSDK->getScene(i);
			WaitForNovodexScene(*nScene);
		}
		if ( GApexCommands->ProcessCommand(TCHAR_TO_ANSI(Cmd), Ar) )
		{
			return TRUE;
		}
	}
#endif

	if( ParseCommand(&Cmd, TEXT("NXSTATS")) )
	{
		bOutputAllStats = TRUE;
		return TRUE;
	}
	else if(ParseCommand(&Cmd, TEXT("MESHSCALES")) )
	{
		// First make array of all USequenceOps
		TArray<UStaticMesh*> AllMeshes;
		for( TObjectIterator<UStaticMesh> It; It; ++It )
		{
			UStaticMesh* Mesh = *It;
			if(Mesh && Mesh->BodySetup)
			{
				AllMeshes.AddItem(Mesh);
			}
		}

		// Then sort them
		Sort<USE_COMPARE_POINTER(UStaticMesh, UnPhysLevel)>(&AllMeshes(0), AllMeshes.Num());

		Ar->Logf( TEXT("----- STATIC MESH SCALES ------") );
		for(INT i=0; i<AllMeshes.Num(); i++)
		{
			Ar->Logf( TEXT("%s (%d) (%d HULLS)"), *(AllMeshes(i)->GetPathName()), AllMeshes(i)->BodySetup->CollisionGeom.Num(), AllMeshes(i)->BodySetup->AggGeom.ConvexElems.Num() );
			for(INT j=0; j<AllMeshes(i)->BodySetup->CollisionGeom.Num(); j++)
			{
				const FVector& Scale3D = AllMeshes(i)->BodySetup->CollisionGeomScale3D(j);
				Ar->Logf( TEXT("  %f,%f,%f"), Scale3D.X, Scale3D.Y, Scale3D.Z);
			}
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd, TEXT("NXDUMPMEM")) )
	{
#if KEEP_TRACK_OF_NOVODEX_ALLOCATIONS && (defined _DEBUG)
		// Group allocations by file/line
		TArray<FPhysXAlloc> GroupedAllocs;

		// Iterate over all allocations
		for ( TMap<PTRINT,FPhysXAlloc>::TIterator AllocIt(FNxAllocator::AllocationToSizeMap); AllocIt; ++AllocIt )
		{
			FPhysXAlloc Alloc = AllocIt.Value();

			// See if we have this location already.
			INT EntryIndex = INDEX_NONE;
			for(INT i=0; i<GroupedAllocs.Num(); i++)
			{
				if((strcmp(GroupedAllocs(i).Filename, Alloc.Filename) == 0) && (GroupedAllocs(i).Line == Alloc.Line))
				{
					EntryIndex = i;
					break;
				}
			}

			// If we found it - update size
			if(EntryIndex != INDEX_NONE)
			{
				GroupedAllocs(EntryIndex).Count += 1;
				GroupedAllocs(EntryIndex).Size += Alloc.Size;
			}
			// If we didn't add to array.
			else
			{
				Alloc.Count = 1;
				GroupedAllocs.AddItem(Alloc);
			}
		}

		// sort by filename for easy diffing
		Sort<USE_COMPARE_CONSTREF(FPhysXAlloc, UnPhysLevel)>(&GroupedAllocs(0), GroupedAllocs.Num());

		// Now print out amount allocated for each allocating location.
		for(INT AllocIndex = 0; AllocIndex < GroupedAllocs.Num(); AllocIndex++)
		{
			FPhysXAlloc& Alloc = GroupedAllocs(AllocIndex);
			Ar->Logf( TEXT("%s:%d,%d,%d"), ANSI_TO_TCHAR(Alloc.Filename), Alloc.Line, Alloc.Size, Alloc.Count );
		}
#endif
		return TRUE;
	}
#if !CONSOLE // On PC, support dumping all physics info to XML.
	else if( ParseCommand(&Cmd, TEXT("NXDUMP")) )
	{
		TCHAR File[MAX_SPRINTF]=TEXT("");
		const TCHAR* FileNameBase = TEXT("UE3_PxCoreDump_");

		if( NxDumpIndex == -1 )
		{
			for( INT TestDumpIndex=0; TestDumpIndex<65536; TestDumpIndex++ )
			{
				appSprintf( File, TEXT("%s%03i.xml"), FileNameBase, TestDumpIndex );

				if( GFileManager->FileSize(File) < 0 )
				{
					NxDumpIndex = TestDumpIndex;
					break;
				}
			}
		}

		appSprintf( File, TEXT("%s%03i"), FileNameBase, NxDumpIndex++ );

		//NXU::coreDump(GNovodexSDK, TCHAR_TO_ANSI(File), NXU::FT_XML, true, false);

		return 1;
	}
#endif // CONSOLE
	else if(!GIsUCC && GNovodexSDK && (ParseCommand(&Cmd, TEXT("NXVRD")) || ParseCommand(&Cmd, TEXT("NXPVD"))))
	{
		#if SUPPORT_DOUBLE_BUFFERING
		NxU32 nbScenes = GNovodexSDK->getNbScenes();
		for (NxU32 i = 0; i < nbScenes; i++)
		{
			WaitForNovodexScene(*GNovodexSDK->getScene(i));
		}
		#endif
	
#if WITH_APEX
		// We need to use APEX's PVD wrapper to get APEX profiling events
		physx::apex::NxApexSDK* ApexSDK = GApexManager->GetApexSDK();
		PVD::PvdBinding* RemoteDebugger = NULL;
		if(NULL != ApexSDK)
		{
			RemoteDebugger = ApexSDK->getPvdBinding();
		}
		
		const UINT PVDTimeoutMS = 100;	// 100 ms
#else		
		NxFoundationSDK  &FoundationSDK  = GNovodexSDK->getFoundationSDK();
		NxRemoteDebugger *RemoteDebugger = FoundationSDK.getRemoteDebugger();
#endif

		if(RemoteDebugger && ParseCommand(&Cmd, TEXT("CONNECT_PROFILE")))
		{
#if WITH_APEX
			if(RemoteDebugger->isConnected())
			{
				RemoteDebugger->disconnect();
			}

			physx::debugger::TConnectionFlagsType PVDFlags = 
			physx::debugger::PvdConnectionType::Profile | 
			physx::debugger::PvdConnectionType::Memory;

			if(*Cmd)
			{
				FTCHARToANSI_Convert ToAnsi;
				ANSICHAR *AnsiCmd = ToAnsi.Convert(Cmd, 0, 0);
				check(AnsiCmd);
				if(AnsiCmd)
				{
					RemoteDebugger->connect(AnsiCmd, NX_DBG_DEFAULT_PORT, PVDTimeoutMS, PVDFlags);
				}
			}
			else
			{
				RemoteDebugger->connect("localhost", NX_DBG_DEFAULT_PORT, PVDTimeoutMS, PVDFlags);
			}
#endif
		}
		else if(RemoteDebugger && (ParseCommand(&Cmd, TEXT("CONNECT_OBJECT")) || ParseCommand(&Cmd, TEXT("CONNECT"))))
		{
			if(RemoteDebugger->isConnected())
			{
				RemoteDebugger->disconnect();
			}

#if WITH_APEX
			physx::debugger::TConnectionFlagsType PVDFlags = 
			physx::debugger::PvdConnectionType::Debug;
#endif
			if(*Cmd)
			{
				FTCHARToANSI_Convert ToAnsi;
				ANSICHAR *AnsiCmd = ToAnsi.Convert(Cmd, 0, 0);
				check(AnsiCmd);
				if(AnsiCmd)
				{
#if WITH_APEX
					RemoteDebugger->connect(AnsiCmd, NX_DBG_DEFAULT_PORT, PVDTimeoutMS, PVDFlags);
#else
					RemoteDebugger->connect(AnsiCmd);
#endif
				}
			}
			else
			{
#if WITH_APEX
				RemoteDebugger->connect("localhost", NX_DBG_DEFAULT_PORT, PVDTimeoutMS, PVDFlags);
#else
				RemoteDebugger->connect("localhost");
#endif
			}
		}
		else if(RemoteDebugger && ParseCommand(&Cmd, TEXT("DISCONNECT")))
		{
			RemoteDebugger->disconnect();
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd, TEXT("NXVIS")) )
	{
		struct { const TCHAR* Name; NxParameter Flag; FLOAT Size; } Flags[] =
		{
			// Axes
			{ TEXT("WORLDAXES"),			NX_VISUALIZE_WORLD_AXES,		1.f },
			{ TEXT("BODYAXES"),				NX_VISUALIZE_BODY_AXES,			1.f },
			{ TEXT("MASSAXES"),             NX_VISUALIZE_BODY_MASS_AXES,    1.f },

			// Contacts
			{ TEXT("CONTACTPOINT"),			NX_VISUALIZE_CONTACT_POINT,		1.f },
			{ TEXT("CONTACTS"),				NX_VISUALIZE_CONTACT_NORMAL,	1.f },
			{ TEXT("CONTACTERROR"),			NX_VISUALIZE_CONTACT_ERROR,		100.f },
			{ TEXT("CONTACTFORCE"),			NX_VISUALIZE_CONTACT_FORCE,		1.f },

			// Joints
			{ TEXT("JOINTLIMITS"),			NX_VISUALIZE_JOINT_LIMITS,		1.f },
			{ TEXT("JOINTLOCALAXES"),		NX_VISUALIZE_JOINT_LOCAL_AXES,	1.f },
			{ TEXT("JOINTWORLDAXES"),		NX_VISUALIZE_JOINT_WORLD_AXES,	1.f },

			// Collision
			{ TEXT("CCD"),					NX_VISUALIZE_COLLISION_SKELETONS,	1.f },
			{ TEXT("CCDTESTS"),				NX_VISUALIZE_COLLISION_CCD,			1.f },
			{ TEXT("COLLISION_AABBS"),		NX_VISUALIZE_COLLISION_AABBS,		1.f },
			{ TEXT("COLLISION"),			NX_VISUALIZE_COLLISION_SHAPES,		1.f },
			{ TEXT("COLLISION_AXES"),		NX_VISUALIZE_COLLISION_AXES,		1.f },
			{ TEXT("COLLISION_COMPOUNDS"),	NX_VISUALIZE_COLLISION_COMPOUNDS,	1.f },
			{ TEXT("COLLISION_VNORMALS"),	NX_VISUALIZE_COLLISION_VNORMALS,	1.f },
			{ TEXT("COLLISION_FNORMALS"),	NX_VISUALIZE_COLLISION_FNORMALS,	1.f },
			{ TEXT("COLLISION_EDGES"),		NX_VISUALIZE_COLLISION_EDGES,		1.f },
			{ TEXT("COLLISION_SPHERES"),	NX_VISUALIZE_COLLISION_SPHERES,		1.f },
			{ TEXT("COLLISION_STATIC"),		NX_VISUALIZE_COLLISION_STATIC,		1.f },
			{ TEXT("COLLISION_DYAMIC"),		NX_VISUALIZE_COLLISION_DYNAMIC,		1.f },
			{ TEXT("COLLISION_FREE"),		NX_VISUALIZE_COLLISION_FREE,		1.f },

			//Cloth
			{ TEXT("CLOTH_MESH"),			NX_VISUALIZE_CLOTH_MESH,			1.f },
			{ TEXT("CLOTH_COLLISIONS"),		NX_VISUALIZE_CLOTH_COLLISIONS,		1.f },
			{ TEXT("CLOTH_ATTACHMENT"),		NX_VISUALIZE_CLOTH_ATTACHMENT,		1.f },
			{ TEXT("CLOTH_SLEEP_VERTEX"),	NX_VISUALIZE_CLOTH_SLEEP_VERTEX,	1.f },
			{ TEXT("CLOTH_SLEEP"),			NX_VISUALIZE_CLOTH_SLEEP,			1.f },
			{ TEXT("CLOTH_VALIDBOUNDS"),	NX_VISUALIZE_CLOTH_VALIDBOUNDS,		1.f },
			{ TEXT("CLOTH_SELFCOLLISIONS"),	NX_VISUALIZE_CLOTH_SELFCOLLISIONS,	1.f },
			{ TEXT("CLOTH_WORKPACKETS"),	NX_VISUALIZE_CLOTH_WORKPACKETS,		1.f },
			{ TEXT("CLOTH_TEARING"),		NX_VISUALIZE_CLOTH_TEARING,	1.f },
			{ TEXT("CLOTH_WORKPACKETS"),	NX_VISUALIZE_CLOTH_WORKPACKETS,		1.f },
			{ TEXT("CLOTH_TEARABLE_VERTICES"),	NX_VISUALIZE_CLOTH_TEARABLE_VERTICES, 1.f },

			//Fluid
			{ TEXT("FLUID_PACKETS"),		NX_VISUALIZE_FLUID_PACKETS,			1.f },
			{ TEXT("FLUID_POSITION"),		NX_VISUALIZE_FLUID_POSITION,		1.f },
			{ TEXT("FLUID_MESH_PACKETS"),	NX_VISUALIZE_FLUID_MESH_PACKETS,	1.f },
			{ TEXT("FLUID_PACKET_DATA"),	NX_VISUALIZE_FLUID_PACKET_DATA,		1.f },
			{ TEXT("FLUID_BOUNDS"),			NX_VISUALIZE_FLUID_BOUNDS,			1.f },
			{ TEXT("FLUID_VELOCITY"),		NX_VISUALIZE_FLUID_VELOCITY,		10.f },
			{ TEXT("FLUID_KERNEL_RADIUS"),	NX_VISUALIZE_FLUID_KERNEL_RADIUS,	1.f },
			{ TEXT("FLUID_DRAINS"),			NX_VISUALIZE_FLUID_DRAINS,			1.f },
			{ TEXT("FLUID_EMITTERS"),		NX_VISUALIZE_FLUID_EMITTERS,		1.f },
			{ TEXT("FLUID_MOTION_LIMIT"),	NX_VISUALIZE_FLUID_MOTION_LIMIT ,	1.f },
			{ TEXT("FLUID_DYN_COLLISION"),	NX_VISUALIZE_FLUID_DYN_COLLISION,	1.f },
			{ TEXT("FLUID_STC_COLLISION"),	NX_VISUALIZE_FLUID_STC_COLLISION ,	1.f },
			{ TEXT("FLUID_DRAINS"),			NX_VISUALIZE_FLUID_DRAINS ,			1.f },

			// ForceFields
			{ TEXT("FORCEFIELDS"),			NX_VISUALIZE_FORCE_FIELDS,			1.f },

			{ TEXT("SOFTBODY_MESH"),			NX_VISUALIZE_SOFTBODY_MESH,			1.f },
			{ TEXT("SOFTBODY_ATTACHMENT"),		NX_VISUALIZE_SOFTBODY_ATTACHMENT,	1.f },
			{ TEXT("SOFTBODY_COLLISIONS"),		NX_VISUALIZE_SOFTBODY_COLLISIONS,	1.f },
			{ TEXT("SOFTBODY_SLEEP_VERTEX"),	NX_VISUALIZE_SOFTBODY_SLEEP_VERTEX,	1.f },
			{ TEXT("SOFTBODY_SLEEP"),			NX_VISUALIZE_SOFTBODY_SLEEP,		1.f },
			{ TEXT("SOFTBODY_VALIDBOUNDS"),		NX_VISUALIZE_SOFTBODY_VALIDBOUNDS,	1.f },
			{ TEXT("SOFTBODY_WORKPACKETS"),		NX_VISUALIZE_SOFTBODY_WORKPACKETS,	1.f },
			{ TEXT("SOFTBODY_TEARABLE_VERTICES"), NX_VISUALIZE_SOFTBODY_TEARABLE_VERTICES,	1.f },
			{ TEXT("SOFTBODY_TEARING"),			NX_VISUALIZE_SOFTBODY_TEARING,	1.f },
			{ TEXT("SOFTBODY_TEARABLE_VERTICES"), NX_VISUALIZE_SOFTBODY_TEARABLE_VERTICES,	1.f },

			// Body
			{ TEXT("BODY_LINEAR_VELOCITY"),		NX_VISUALIZE_BODY_LIN_VELOCITY,		1.f },
			{ TEXT("BODY_ANGULAR_VELOCITY"),	NX_VISUALIZE_BODY_ANG_VELOCITY,		1.f },
			{ TEXT("BODY_JOINT_GROUPS"),		NX_VISUALIZE_BODY_JOINT_GROUPS,		1.f },
			
			// Actor
			{ TEXT("ACTOR_AXES"),				NX_VISUALIZE_ACTOR_AXES,			1.f },
		};

		// stall all scenes to make sure we don't simulate when calling setParameter
		NxU32 NbScenes = GNovodexSDK->getNbScenes();
		for (NxU32 i = 0; i < NbScenes; i++)
		{
			NxScene *nScene = GNovodexSDK->getScene(i);
			WaitForNovodexScene(*nScene);
		}

		UBOOL bDebuggingActive = false;
		UBOOL bFoundFlag = false;
		if ( ParseCommand(&Cmd,TEXT("PHYSX_CLEAR_ALL")) )
		{
			Ar->Logf(TEXT("Clearing all PhysX Debug Flags."));
			for (int i = 0; i < ARRAY_COUNT(Flags); i++)
			{
				GNovodexSDK->setParameter(Flags[i].Flag, 0.0f);
				bFoundFlag = true;
			}
		}
		else
		{
			for (int i = 0; i < ARRAY_COUNT(Flags); i++)
			{
				// Parse out the command sent in and set only those flags
				if (ParseCommand(&Cmd, Flags[i].Name))
				{
					NxReal Result = GNovodexSDK->getParameter(Flags[i].Flag);
					if (Result == 0.0f)
					{
						GNovodexSDK->setParameter(Flags[i].Flag, Flags[i].Size);
						Ar->Logf(TEXT("Flag set."));
					}
					else
					{
						GNovodexSDK->setParameter(Flags[i].Flag, 0.0f);
						Ar->Logf(TEXT("Flag un-set."));
					}

					bFoundFlag = true;
				}

				// See if any flags are true
				NxReal Result = GNovodexSDK->getParameter(Flags[i].Flag);
				if(Result > 0.f)
				{
					bDebuggingActive = true;
				}
			}
		}

		// If no debugging going on - disable it using NX_VISUALIZATION_SCALE
		if(bDebuggingActive)
		{
			GNovodexSDK->setParameter(NX_VISUALIZATION_SCALE, 1.0f);
		}
		else
		{
			GNovodexSDK->setParameter(NX_VISUALIZATION_SCALE, 0.0f);
		}

		if(!bFoundFlag)
		{
			Ar->Logf(TEXT("Unknown Novodex visualization flag specified."));
		}
		return 1;
	}
	else if( ParseCommand(&Cmd, TEXT("DUMPAWAKE")) )
	{
		INT AwakeCount = 0;
		for( TObjectIterator<URB_BodyInstance> It; It; ++It ) 
		{
			URB_BodyInstance* BI = *It;
			if(BI && BI->GetNxActor() && !BI->GetNxActor()->isSleeping())
			{
				debugf(TEXT("-- %s:%d"), BI->OwnerComponent ? *BI->OwnerComponent->GetPathName() : TEXT("None"), BI->BodyIndex);
				AwakeCount++;
			}
		}
		debugf(TEXT("TOTAL: %d AWAKE BODIES FOUND"), AwakeCount);
		return 1;
	}
	else if( ParseCommand(&Cmd, TEXT("CLOTHINGTELEPORTANDRESET")) )
	{
#if WITH_APEX
		for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			USkeletalMeshComponent* SkelComp = *It;
			if( SkelComp->GetScene() == GWorld->Scene && SkelComp->GetApexClothing() != NULL)
			{
				SkelComp->ForceApexClothingTeleportAndReset();
			}
		}		
#endif
	}
	else if( ParseCommand(&Cmd, TEXT("CLOTHINGTELEPORT")) )
	{
#if WITH_APEX
		for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			USkeletalMeshComponent* SkelComp = *It;
			if( SkelComp->GetScene() == GWorld->Scene && SkelComp->GetApexClothing() != NULL)
			{
				SkelComp->ForceApexClothingTeleport();
			}
		}		
#endif
	}

#endif // WITH_NOVODEX

	return 0;
}

/** Util to list to log all currently awake rigid bodies */
void ListAwakeRigidBodies(UBOOL bIncludeKinematic)
{
#if WITH_NOVODEX
	INT BodyCount = 0;
	for( TObjectIterator<URB_BodyInstance> It; It; ++It )
	{
		URB_BodyInstance* BodyInst = *It;
		if(!BodyInst->IsTemplate() && BodyInst->IsValidBodyInstance())
		{
			NxActor* nActor = BodyInst->GetNxActor();
			if(!nActor->isSleeping() && (bIncludeKinematic || !nActor->readBodyFlag(NX_BF_KINEMATIC)))
			{
				debugf(TEXT("BI %s %d"), BodyInst->OwnerComponent ? *BodyInst->OwnerComponent->GetPathName() : TEXT("NONE"), BodyInst->BodyIndex);
				BodyCount++;
			}
		}
	}
	debugf(TEXT("TOTAL: %d awake bodies."), BodyCount);
#endif // WITH_NOVODEX
}


