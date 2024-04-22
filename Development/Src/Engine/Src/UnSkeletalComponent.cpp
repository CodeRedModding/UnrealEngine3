/*=============================================================================
	UnSkeletalComponent.cpp: Actor component implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "EngineDecalClasses.h"
#include "EnginePhysicsClasses.h"
#include "UnSkeletalRenderCPUSkin.h"
#include "UnSkeletalRenderGPUSkin.h"
#include "UnDecalRenderData.h"
#include "NvApexManager.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "AnimationEncodingFormat.h"

#if WITH_APEX
#include "NvApexScene.h"
#endif

#if WITH_FACEFX
	#include "UnFaceFXSupport.h"
	#include "UnFaceFXRegMap.h"
	#include "UnFaceFXMaterialParameterProxy.h"
	#include "UnFaceFXMorphTargetProxy.h"

	#include "../FaceFX/UnFaceFXMaterialNode.h"
	#include "../FaceFX/UnFaceFXMorphNode.h"
	#include "../../../External/FaceFX/FxSDK/Inc/FxActorInstance.h"

	using namespace OC3Ent;
	using namespace Face;

	/** FaceFX stats objects */
	DECLARE_STATS_GROUP(TEXT("FaceFX"),STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX Tick Time"),STAT_FaceFX_TickTime,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX Begin Frame Time"),STAT_FaceFX_BeginFrameTime,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX Morph Pass Time"),STAT_FaceFX_MorphPassTime,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX Material Pass Time"),STAT_FaceFX_MaterialPassTime,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX Bone Blending Time"),STAT_FaceFX_BoneBlendingPassTime,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX End Frame Time"),STAT_FaceFX_EndFrameTime,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX PlayAnim Time"),STAT_FaceFX_PlayAnim,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX PlayAnim1 Time"),STAT_FaceFX_PlayAnim1,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX PlayAnim2 Time"),STAT_FaceFX_PlayAnim2,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX PlayAnim3 Time"),STAT_FaceFX_PlayAnim3,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX PlayAnim4 Time"),STAT_FaceFX_PlayAnim4,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX PlayAnim5 Time"),STAT_FaceFX_PlayAnim5,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX PlayAnim6 Time"),STAT_FaceFX_PlayAnim6,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX PlayAnim7 Time"),STAT_FaceFX_PlayAnim7,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX Mount Time"),STAT_FaceFX_Mount,STATGROUP_FaceFX);
	DECLARE_CYCLE_STAT(TEXT("FaceFX Unmount Time"),STAT_FaceFX_UnMount,STATGROUP_FaceFX);

	DECLARE_MEMORY_STAT(TEXT("FaceFX Peak Mem"),STAT_FaceFXPeakAllocSize,STATGROUP_Memory);
	DECLARE_MEMORY_STAT(TEXT("FaceFX Cur Mem"),STAT_FaceFXCurrentAllocSize,STATGROUP_Memory);

	// Priority with which to display sounds triggered by FaceFX.
	#define SUBTITLE_PRIORITY_FACEFX	10000

#endif // WITH_FACEFX

IMPLEMENT_CLASS(USkeletalMeshComponent);

/** Whether to show drop rates for skeletal mesh components using colored glow. */
UBOOL GVisualizeSkeletalMeshTickOptimization = FALSE;

// LOOKING_FOR_PERF_ISSUES
extern UBOOL GShouldLogOutAFrameOfSkelCompTick;
#define PERF_SHOW_SKELETAL_MESH_COMPONENT_TICK_TIME	!(FINAL_RELEASE)

// LOOKING_FOR_PERF_ISSUES
#define PERF_SHOW_ANIMNODE_TICK_TIMES	(0)

// This will do a profile trace when SetSkeletalMesh is called.
#define TRACE_SETSKELMESH			(0)
// This will do a profile trace when SetPhysAsset is called.
#define TRACE_SETPHYSASSET			(0)

IMPLEMENT_COMPARE_CONSTREF( BYTE, UnSkeletalComponent, { return (A - B); } )

/** This holds all of our SkelMesh LOD info **/
struct SkelMeshLODDatum
{
	// this is implied by the key in the map FString SkelMeshName;
	INT NumLODs;
	INT NumBones;

	TMap<INT, INT> NumOccurrences;

	FString ToString() const
	{
		FString Retval;

		Retval += FString::Printf( TEXT("NumLODs: %d NumBones %d"), NumLODs, NumBones );

		FLOAT TotalOccurrences = 0;
		for( TMap<INT, INT>::TConstIterator NumItr(NumOccurrences); NumItr; ++NumItr )
		{
			TotalOccurrences += NumItr.Value();
		}

		for( TMap<INT, INT>::TConstIterator NumItr(NumOccurrences); NumItr; ++NumItr )
		{
			Retval += LINE_TERMINATOR;
			Retval += FString::Printf( TEXT("    LOD: %d Occurrence %d  Percent: %f"), 
				NumItr.Key()
				, NumItr.Value()
				, TotalOccurrences ? static_cast<FLOAT>(NumItr.Value())/TotalOccurrences : -1.0f 
				);
		}

		return Retval;
	}
};

TMap<FString, SkelMeshLODDatum> SkelMeshLODData;
UBOOL bPrintedSnazzyPrint = FALSE;

/** This clears out the data from the SkelMeshLODData list**/
void ClearSkelMeshLODsList()
{
	SkelMeshLODData.Reset();
}

/** This is our printer function for printing out the list of skel meshes and their LODs**/
void PrintOutSkelMeshLODs()
{
	warnf( TEXT( "Size of list: %d %d" ), SkelMeshLODData.Num(), GFrameCounter );
	for( TMap<FString, SkelMeshLODDatum>::TConstIterator SkelItr(SkelMeshLODData); SkelItr; ++SkelItr )
	{
		warnf( TEXT( "%s %s" ), *SkelItr.Key(), *SkelItr.Value().ToString() );
	}
}


/*-----------------------------------------------------------------------------
	USkeletalMeshComponent.
-----------------------------------------------------------------------------*/

void USkeletalMeshComponent::PostLoad()
{
	// Skeletal mesh components do not support using precomputed shadows. Disable them
	// here before we call up to the super classes, which disable other properties,
	// in part, based on this flag.
	bUsePrecomputedShadows = FALSE;

	Super::PostLoad();
}

void USkeletalMeshComponent::DeleteAnimTree()
{
	// make sure any playing camera anims are cleaned up
	UINT const NumAnimNodes = AnimTickArray.Num();
	for(UINT i=0; i<NumAnimNodes; i++)
	{
		UAnimNodeSequence* const SeqNode = Cast<UAnimNodeSequence>(AnimTickArray(i));
		if (SeqNode && SeqNode->ActiveCameraAnimInstance)
		{
			SeqNode->StopCameraAnim();
		}
	}
	
	// make sure skel component to be released from pool
	UAnimNodeSlot::ReleaseSequenceNodes(this);

	UAnimTree* RESTRICT Tree			= Cast<UAnimTree>(Animations);
	if (Tree)
	{	
		// Return it to the object pool if needed
		Tree->ReturnToPool();
	}

	// Just release the reference to the existing AnimTree. GC will take care of actually freeing the AnimNodes, 
	// as this should have been the only reference to them!
	Animations = NULL;

	// Clear flag
	bAnimTreeInitialised = FALSE;

	// Also clear refs to nodes in tree.
	AnimTickArray.Empty();
	AnimAlwaysTickArray.Empty();
	SkelControlTickArray.Empty();

	// clear morph target index map
	MorphTargetIndexMap.Empty();
	ActiveMorphs.Empty();
	ActiveCurveMorphs.Empty();
}

void USkeletalMeshComponent::Attach()
{
	if( SkeletalMesh )
	{
		// Initialize the alternate weight tracks if present BEFORE creating the new mesh object
		InitLODInfos();

#if WITH_EDITOR
		// if it's editor, and preview morphset is available, 
		// make sure you load them to morphsets 
		// this is to support animset viewer
		if ( GIsEditor && SkeletalMesh->PreviewMorphSets.Num() >  0)
		{
			// make sure this isn't same set already in morphset
			for (INT I=0; I<SkeletalMesh->PreviewMorphSets.Num(); ++I)
			{
				MorphSets.AddUniqueItem(SkeletalMesh->PreviewMorphSets(I));
			}
		}
#endif

		// No need to create the mesh object if we aren't actually rendering anything (see UPrimitiveComponent::Attach)
		if (!GIsUCC && ShouldComponentAddToScene() &&
			(appGetPlatformType() & UE3::PLATFORM_WindowsServer) == 0)
		{
			// If the component has morph targets, and the GPU doesn't support morphing, use CPU skinning.
			const UBOOL bHasMorphTargets = MorphSets.Num();
			const UBOOL bSupportsGPUMorphing = TRUE;

			// Also check if skeletal mesh has too many bones/chunk for GPU skinning.
			const UBOOL bIsCPUSkinned = SkeletalMesh->IsCPUSkinned() || (bHasMorphTargets && !bSupportsGPUMorphing) || (GIsEditor && ShouldCPUSkin());
			if(bIsCPUSkinned)
			{

				warnf(TEXT("!!!!!!!!!!!!!!!!!!!!!!!!!!   CPU skinned %s %s %u %u %u %u"), *GetPathName(), *SkeletalMesh->GetPathName(), SkeletalMesh->IsCPUSkinned(), bHasMorphTargets, bSupportsGPUMorphing, ShouldCPUSkin());
				MeshObject = ::new FSkeletalMeshObjectCPUSkin(this);
			}
			else
			{
				MeshObject = ::new FSkeletalMeshObjectGPUSkin(this);
			}

			//Allow the editor a chance to manipulate it before its added to the scene
			PostInitMeshObject(MeshObject);
		}

		// Initialize/release clothing on pawns
#if WITH_APEX
		if ( !bSkipInitClothing && GIsGame )
		{
			InitApexClothing(GWorld->RBPhysScene);
		}
#endif
	}

	// Update bHasValidBodies flag
	UpdateHasValidBodies();

	Super::Attach();

	// if it already has hit mask, send update notification to render thread
	// so that render thread refreshes scene info of the mesh component
	if ( bNeedsToDeleteHitMask )
	{
		// Send a command to the rendering thread to update the primitive from hit mask list
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FUpdateHitMaskComponentCommand,
			FSceneInterface*,Scene,Scene,
			USkeletalMeshComponent*,SkeletalMeshComp,this,
		{
			Scene->UpdateHitMask( SkeletalMeshComp );
		});
	}

	if( GWorld->HasBegunPlay() && !Animations && AnimTreeTemplate )
	{
		// make sure anim tree is instanced from template.
		SetAnimTreeTemplate( AnimTreeTemplate );
	}
	else
	{
		// This re-attach may be due to property editing - want to force a re-init here, as skelmesh may have changed
		UBOOL bForceInit = (GIsEditor && !GIsGame);
		InitAnimTree(bForceInit);
	}

#if WITH_FACEFX
	// Are we using FaceFX features?
	if( !bDisableFaceFX && SkeletalMesh != NULL && SkeletalMesh->FaceFXAsset != NULL && SkeletalMesh->FaceFXAsset->FaceFXActor != NULL )
	{
		// Create FaceFX actor instance if we don't have one already.  It may have survived a prior detachment, so
		// in which case we won't need to do anything here
		if( FaceFXActorInstance == NULL )
		{
			FaceFXActorInstance = new FxActorInstance();
			FaceFXActorInstance->SetActor( SkeletalMesh->FaceFXAsset->GetFxActor() );
		}

		// Make sure the actors match between the FaceFX instance and the FaceFX asset.
		else if( FaceFXActorInstance->GetActor() != SkeletalMesh->FaceFXAsset->GetFxActor() )
		{
			// OK, we have a FaceFX actor instance but the actor doesn't match the skeletal mesh's FaceFX asset's
			// associated actor.  We'll update our instance object to point to the correct actor.
			FaceFXActorInstance->SetActor( SkeletalMesh->FaceFXAsset->GetFxActor() );
		}
	}
	else
	{
		// FaceFX features are disabled, so destroy our FaceFX instance if we have one
		if( FaceFXActorInstance != NULL )
		{
			delete FaceFXActorInstance;
			FaceFXActorInstance = NULL;
		}
	}
#endif // WITH_FACEFX

	bRequiredBonesUpToDate = FALSE;
	bUpdatedFixedClothVerts = FALSE;

	UpdateParentBoneMap();
	UpdateLODStatus();
	UpdateSkelPose();

	// re-update instance vertex influences after re-attaching
	for (INT LODIdx=0; LODIdx<LODInfo.Num(); LODIdx++)
	{								
		if( InstanceVertexWeightBones.Num() > 0 || LODInfo(LODIdx).bAlwaysUseInstanceWeights )
		{
			UpdateInstanceVertexWeights(LODIdx);
		}	
	}

	bForceMeshObjectUpdate = TRUE;
	ConditionalUpdateTransform();
	bForceMeshObjectUpdate = FALSE;
}

#if USE_GAMEPLAY_PROFILER
/** 
 * This function actually does the work for the GetProfilerAssetObject and is virtual.  
 * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
 **/
UObject* USkeletalMeshComponent::GetProfilerAssetObjectInternal() const
{
	return SkeletalMesh;
}
#endif

/**
 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
 * you have a component of interest but what you really want is some characteristic that you can use to track
 * down where it came from.  
 *
 */
FString USkeletalMeshComponent::GetDetailedInfoInternal() const
{
	FString Result;  

	if( SkeletalMesh != NULL )
	{
		Result = SkeletalMesh->GetDetailedInfoInternal();
	}
	else
	{
		Result = TEXT("No_SkeletalMesh");
	}

	return Result;  
}

/**
* This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
* ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
* you have a component of interest but what you really want is some characteristic that you can use to track
* down where it came from.  
*
*/
FString USkeletalMesh::GetDetailedInfoInternal() const
{
	return GetPathName( NULL );
}


void USkeletalMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	if( !Animations && AnimTreeTemplate )
	{
		SetAnimTreeTemplate(AnimTreeTemplate);
		UpdateSkelPose();
		ConditionalUpdateTransform();
	}

	// Call BeginPlay on any attached components.
	for(UINT AttachmentIndex = 0;AttachmentIndex < (UINT)Attachments.Num();AttachmentIndex++)
	{
		FAttachment& Attachment = Attachments(AttachmentIndex);
		if(Attachment.Component)
		{
			Attachment.Component->ConditionalBeginPlay();
		}
	}
}

/** Calculate the up-to-date transform of the supplied SkeletalMeshComponent, which should be attached to this component. */
FMatrix USkeletalMeshComponent::CalcAttachedSkelCompMatrix(const USkeletalMeshComponent* AttachedComp)
{
	// First, find what our up-to-date transform should be
	FMatrix ParentLocalToWorld;
	if(AttachedToSkelComponent)
	{
		ParentLocalToWorld = AttachedToSkelComponent->CalcAttachedSkelCompMatrix(this);
	}
	else
	{
		ParentLocalToWorld = LocalToWorld;
	}

	// Then find the attachment in the array
	INT AttachmentIndex = INDEX_NONE;
	for(UINT Idx = 0; Idx < (UINT)Attachments.Num(); Idx++)
	{
		FAttachment& Attachment = Attachments(Idx);
		if(Attachment.Component == AttachedComp)
		{
			AttachmentIndex = Idx;
			break;
		}
	}

	// If attachment is not in the array - generate a warning here and return SkelComp's current matrix
	if(AttachmentIndex == INDEX_NONE)
	{
		debugf(TEXT("ERROR: Component '%s' not found as attachment of '%s'"), *AttachedComp->GetPathName(), *GetPathName());
		return AttachedComp->LocalToWorld;
	}

	// Use the attachment info to calculate the new transform.
	FAttachment& Attachment = Attachments(AttachmentIndex);
	INT	BoneIndex = MatchRefBone(Attachment.BoneName);
	if(BoneIndex != INDEX_NONE)
	{
		FVector RelativeScale = Attachment.RelativeScale == FVector(0,0,0) ? FVector(1,1,1) : Attachment.RelativeScale;
		const FMatrix& AttachmentToWorld = FScaleRotationTranslationMatrix( RelativeScale, Attachment.RelativeRotation, Attachment.RelativeLocation ) * SpaceBases(BoneIndex).ToMatrix() * ParentLocalToWorld;

		return AttachmentToWorld;
	}
	// If bone not found, return current transform.
	else
	{
		debugf(TEXT("CalcAttachedSkelCompMatrix: Bone '%s' not found in '%s' for attached component '%s'"), *Attachment.BoneName.ToString(), *this->GetPathName(), *AttachedComp->GetPathName());
		return AttachedComp->LocalToWorld;
	}
}

#if CHART_DISTANCE_FACTORS
static void AddDistanceFactorToChart(FLOAT DistanceFactor)
{
	if(DistanceFactor < SMALL_NUMBER)
	{
		return;
	}

	if(DistanceFactor >= GDistanceFactorDivision[NUM_DISTANCEFACTOR_BUCKETS-2])
	{
		GDistanceFactorChart[NUM_DISTANCEFACTOR_BUCKETS-1]++;
	}
	else if(DistanceFactor < GDistanceFactorDivision[0])
	{
		GDistanceFactorChart[0]++;
	}
	else
	{
		for(INT i=1; i<NUM_DISTANCEFACTOR_BUCKETS-2; i++)
		{
			if(DistanceFactor < GDistanceFactorDivision[i])
			{
				GDistanceFactorChart[i]++;
				break;
			}
		}
	}
}
#endif // CHART_DISTANCE_FACTORS

void USkeletalMeshComponent::UpdateTransform()
{
	SCOPE_CYCLE_COUNTER(STAT_SkelCompUpdateTransform);

	// for characters we are not trying to animate we don't need to BlendInPhysics!
	bRecentlyRendered = (LastRenderTime > GWorld->GetWorldInfo()->TimeSeconds - 1.0f);

	if(ParentAnimComponent && bTransformFromAnimParent)
	{		
		Super::SetParentToWorld(ParentAnimComponent->LocalToWorld);
	}

	// If we have physics, and physics is done, blend in any physics results.
	// We use the bHasHadPhysicsBlendedIn to ensure we don't try and blend it in more than once!
	// We need to do this before Super::UpdateTransform so the bounding box takes into account bone movement due to physics.
	if( PhysicsAssetInstance && !bHasHadPhysicsBlendedIn && GWorld->InTick && GWorld->bPostTickComponentUpdate )
	{
		if( bRecentlyRendered || bUpdateSkelWhenNotRendered )
		{
			BlendInPhysics();
		}

		bHasHadPhysicsBlendedIn = TRUE;
	}
	Super::UpdateTransform();

#if WITH_APEX
	FIApexClothing *Clothing = GetApexClothing();
	if ( Clothing != NULL )
	{
		// Optimization that stops skinning invisible clothing, but causes it to take a frame to appear, so disabled for now
		if(FALSE)
		{
			UBOOL bVisible = !HiddenGame;
			if(!bIgnoreOwnerHidden && GetOwner() && GetOwner()->bHidden)
			{
				bVisible = FALSE;
			}

			Clothing->SetVisible(bVisible);
		}
		
		bool bParentIsRagdoll = ParentAnimComponent && ParentAnimComponent->PhysicsWeight > 0.0f;
		bool bIsRagdoll = bParentIsRagdoll || PhysicsWeight > 0.0f;
		if((Clothing->IsVisible(TRUE) || !bAutoFreezeApexClothingWhenNotRendered) && ((!GWorld->bPostTickComponentUpdate && !bIsRagdoll) || (GWorld->bPostTickComponentUpdate && bIsRagdoll)))
		{
			TArray<FMatrix> WorldBoneTMs;
			physx::PxU32 BoneCount;
			const physx::PxU32 *BonesUsed = Clothing->GetBonesUsed(BoneCount);
			WorldBoneTMs.Add(BoneCount);
			for(physx::PxU32 Index=0; Index<BoneCount; Index++)
			{
				INT BoneIdx = (INT)BonesUsed[Index];
				WorldBoneTMs(Index) = GetBoneMatrix(BoneIdx);
				WorldBoneTMs(Index).ScaleTranslation( FVector(U2PScale) );
			}

			FMatrix LocalToWorldPhysX = LocalToWorld;
			LocalToWorldPhysX.ScaleTranslation(FVector(U2PScale));
			Clothing->SyncTransforms(WorldBoneTMs.Num(), (const physx::PxF32*)WorldBoneTMs.GetData(), sizeof(FMatrix), LocalToWorldPhysX);
			//debugf(TEXT("SyncTransforms '%s' (UpdateTransform)"), *GetPathName());
		}
	}
#endif

	// Start assuming this is the last time we'll call UpdateTransform.
	UBOOL bFinalUpdate = TRUE;

	// if we have physics to blend in (or our parent does), and we haven't received physics update yet, this is not final update. 
	if((PhysicsAssetInstance || (ParentAnimComponent && ParentAnimComponent->PhysicsAssetInstance)) && GWorld->InTick && !GWorld->bPostTickComponentUpdate)
	{
		bFinalUpdate = FALSE;
	}
#if WITH_APEX
	if(Clothing && Clothing->IsVisible(TRUE) && GWorld->InTick && !GWorld->bPostTickComponentUpdate)
	{
		bFinalUpdate = FALSE;
	}
#endif
	
	// We mark ourself as dirty, so we revisit it afterwards. That later visit will update attachments.
	if( !bFinalUpdate )
	{
		bNeedsUpdateTransform = TRUE;
	}

	if(bFinalUpdate || bForceUpdateAttachmentsInTick)
	{
		//debugf(TEXT("%2.3f: %s full update (%s)"),GWorld->GetTimeSeconds(),*GetPathName(),*Owner->GetName());
		// Mark all the attachments' transforms as dirty.
		for(UINT AttachmentIndex = 0;AttachmentIndex < (UINT)Attachments.Num();AttachmentIndex++)
		{
			FAttachment& Attachment = Attachments(AttachmentIndex);
			if(Attachment.Component)
			{
				Attachment.Component->BeginDeferredUpdateTransform();
			}
		}

		// Update the attachments.
		//@note - testing whether we can survive without this since UActorComponent::UpdateComponent() will call it after this function returns
		//        it might cause issues where UpdateTransform is called directly though, not sure if that is a valid case or not
		// @laurent - restored for Editor, since it breaks attachment updates in viewport. (animset editor / animtree).
		if( GIsEditor )
		{
			UpdateChildComponents();
		}

		if(Owner && GWorld->HasBegunPlay())
		{
			for(INT i=0; i<Owner->Attached.Num(); i++)
			{
				AActor* Other = Owner->Attached(i);
				if(Other && Other->BaseSkelComponent == this)
				{
					//debugf(TEXT("- %2.3f: moving %s as attached"),GWorld->GetTimeSeconds(),*Other->GetName());
					// This UpdateTransform might be part of the initial association of components.
					// In that case, we don't want to start calling MoveActor on things, as they might not be fully associated.
					// So we check that the level is not in the process of being made visible, and do nothing if that is the case.
					ULevel* Level = Other->GetLevel();
					if(Level && !Level->bHasVisibilityRequestPending && (FGlobalComponentReattachContext::ActiveGlobalReattachContextCount == 0))
					{
						const INT BoneIndex = MatchRefBone(Other->BaseBoneName);
						if(BoneIndex != INDEX_NONE)
						{
							// If there's a movement track on the attached actor, that movement track will take into account the base transform.
							// In that case, there's no reason to update the attached actor's transform here.
							UInterpTrackMove* MoveTrack;
							UInterpTrackInstMove* MoveInst;
							USeqAct_Interp* Seq;
							UBOOL bHasMovementTrack = Other->FindInterpMoveTrack(&MoveTrack, &MoveInst, &Seq);
							if  (!bHasMovementTrack)
							{
								FMatrix BaseTM = GetBoneMatrix(BoneIndex);
								BaseTM.RemoveScaling();

								FRotationTranslationMatrix HardRelMatrix(Other->RelativeRotation,Other->RelativeLocation);

								const FMatrix& NewWorldTM = HardRelMatrix * BaseTM;

								const FVector& NewWorldPos = NewWorldTM.GetOrigin();
								const FRotator& NewWorldRot = NewWorldTM.Rotator();

#if !FINAL_RELEASE
								extern UBOOL GShouldLogOutAFrameOfMoveActor;

								if(GShouldLogOutAFrameOfMoveActor)
								{
									debugf(TEXT("UpdateTransform (%s %s) ->"), *Owner->GetPathName(), *Owner->GetDetailedInfo());
								}
#endif // !FINAL_RELEASE

								FCheckResult Hit(1.f);
								GWorld->MoveActor( Other, NewWorldPos - Other->Location, Other->bIgnoreBaseRotation ? Other->Rotation : NewWorldRot, MOVE_IgnoreBases, Hit );
								if (Owner == NULL || Owner->bDeleteMe)
								{
									// MoveActor() resulted in us being detached or Owner's destruction
									break;
								}
								else if(bForceUpdateAttachmentsInTick)
								{
									Other->ForceUpdateComponents(FALSE, TRUE);
								}
							}
						}
						else
						{
							debugf(TEXT("USkeletalMeshComponent::UpdateTransform for %s: BaseBoneName (%s) not found for attached Actor %s!"), *Owner->GetName(), *Other->BaseBoneName.ToString(), *Other->GetName());
						}
					}
				}
			}
		}
	}

	// if we have not updated the transforms then no need to send them to the rendering thread
	// @todo GIsEditor used to be bUpdateSkelWhenNotRendered. Look into it further to find out why it doesn't update animations in the AnimSetViewer, when a level is loaded in UED (like POC_Cover.gear).
	if( MeshObject && (bForceMeshObjectUpdate || (bFinalUpdate && (bRecentlyRendered || bUpdateSkelWhenNotRendered || GIsEditor || MeshObject->bHasBeenUpdatedAtLeastOnce == FALSE))) )
	{
		SCOPE_CYCLE_COUNTER(STAT_MeshObjectUpdate);

		INT UseLOD = PredictedLODLevel;
		// If we have a ParentAnimComponent - force this component to render at that LOD, so all bones are present for it.
		// Note that this currently relies on the behaviour where this mesh is rendered at the LOD we pass in here for all viewports. We should
		// be able to render it at lower LOD on viewports where it is further away. That will make the ParentAnimComponent case a bit harder to solve.
		if(ParentAnimComponent)
		{
			UseLOD = ::Clamp(ParentAnimComponent->PredictedLODLevel, 0, SkeletalMesh->LODModels.Num()-1);
		}

#if WITH_APEX
		if ( ApexClothing )
		{
			ApexClothing->UpdateRenderResources();
		}
#endif

		// Are morph targets disabled for this LOD?
		if ( SkeletalMesh->LODInfo( UseLOD ).bHasBeenSimplified)
		{
			ActiveMorphs.Empty();
		}
		if (GEmulateMobileRendering && ActiveMorphs.Num())
		{
			ActiveMorphs.Empty();
			warnf(NAME_Warning, TEXT("Skeletal Mesh Morphs are not supported on mobile"));
		}

		if( BoneVisibilityStates.Num() == SpaceBases.Num() )
		{
			// for invisible bones, I'll still need to update the transform, so that rendering can use it for skinning
			BYTE const * BoneVisibilityState = BoneVisibilityStates.GetData();
			FBoneAtom * SpaceBase = SpaceBases.GetData();
			for (INT BoneIndex=0; BoneIndex<BoneVisibilityStates.Num(); ++BoneIndex, ++BoneVisibilityState, ++SpaceBase)
			{
				if (*BoneVisibilityState!=BVS_Visible)
				{
					if (BoneIndex != 0 )
					{
						// since they're invisible, copy parent transform to itself and scale set to be 0
						const INT ParentIndex = SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;
						*SpaceBase = SpaceBases(ParentIndex);
					}
					SpaceBase->SetScale(ScalarZero);
				}
			}
		}

		MeshObject->Update(UseLOD,this,ActiveMorphs);  // send to rendering thread
		MeshObject->bHasBeenUpdatedAtLeastOnce = TRUE;
		
		// scene proxy update of material usage based on active morphs
		UpdateMorphMaterialUsageOnProxy();

#if !FINAL_RELEASE
		extern UBOOL GShouldLogOutAFrameOfSkelCompLODs;
		if( GShouldLogOutAFrameOfSkelCompLODs == TRUE )
		{
			// so we have sent the SkelMesh over to the render thread with UseLOD.  So let's record that info.
			const FString SkelName = SkeletalMesh->GetName();
			SkelMeshLODDatum* Datum = SkelMeshLODData.Find( SkelName );

			if( Datum != NULL )
			{
				INT* CurrCount = Datum->NumOccurrences.Find( UseLOD );
				if( CurrCount != NULL )
				{
					(*CurrCount)++; 
				}
				else
				{
					Datum->NumOccurrences.Set( UseLOD, 1 );
				}
			}
			// if this is not in the Map yet.
			else
			{
				//warnf( TEXT( "Adding: %s" ), *SkelName );
				SkelMeshLODDatum NewDatum;
				NewDatum.NumLODs = SkeletalMesh->LODModels.Num();
				NewDatum.NumBones = SkeletalMesh->RefSkeleton.Num();
				SkelMeshLODData.Set( SkelName, NewDatum );
			}

// 			if( !( GFrameCounter % 1000 )  && ( bPrintedSnazzyPrint == FALSE ) )
// 			{
// 				PrintOutSkelMeshLODs();
// 				bPrintedSnazzyPrint = TRUE;
// 			}
// 
// 			if( GFrameCounter % 1000 )
// 			{
// 				bPrintedSnazzyPrint = FALSE;
// 			}
		}
#endif // !FINAL_RELEASE
	}

}


void USkeletalMeshComponent::UpdateChildComponents()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateChildComponents);
	for(UINT AttachmentIndex = 0;AttachmentIndex < (UINT)Attachments.Num();AttachmentIndex++)
	{
		FAttachment&	Attachment = Attachments(AttachmentIndex);
		const INT		BoneIndex = MatchRefBone(Attachment.BoneName);

		if( Attachment.Component && BoneIndex != INDEX_NONE && BoneIndex < SpaceBases.Num() )
		{
			FVector RelativeScale = (Attachment.RelativeScale == FVector(0)) ? FVector(1) : Attachment.RelativeScale;
			const FMatrix& AttachmentToWorld = FScaleRotationTranslationMatrix( RelativeScale, Attachment.RelativeRotation, Attachment.RelativeLocation ) * SpaceBases(BoneIndex).ToMatrix() * LocalToWorld;
			SetAttachmentOwnerVisibility(Attachment.Component);
			Attachment.Component->UpdateComponent(Scene, GetOwner(), AttachmentToWorld);
		}
	}
}

void USkeletalMeshComponent::Detach( UBOOL bWillReattach )
{
#if WITH_APEX
	// force a reattach for any clothing component that uses this component as a parent animcomponent to ensure that the clothing is always ticked last
	if(Owner && bWillReattach)
	{
		USkeletalMeshComponent*	ApexClothingComp = NULL;
		for(INT ComponentIndex = 0;ComponentIndex < Owner->Components.Num();ComponentIndex++)
		{
			ApexClothingComp = Cast<USkeletalMeshComponent>(Owner->Components(ComponentIndex));
			if(ApexClothingComp && ApexClothingComp->IsAttached() && ApexClothingComp->GetApexClothing() && ApexClothingComp->ParentAnimComponent == this
				&& ApexClothingComp != this)
			{
				ApexClothingComp->bNeedsReattach = TRUE;
			}
		}
	}
#endif
	if (bNeedsToDeleteHitMask && !bWillReattach)
	{
		// Do not clear if it will reattach
		// Send a command to the rendering thread to remove the primitive from hit mask list
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FRemoveHitMaskComponentCommand,
			FSceneInterface*,Scene,Scene,
			USkeletalMeshComponent*,SkeletalMeshComp,this,
		{
			Scene->ClearHitMask( SkeletalMeshComp );
		});

		bNeedsToDeleteHitMask = FALSE;
	}

	// if it won't reattach, make sure you delete the animnodesequences used by this
	if ( !bWillReattach )
	{
		AnimAlwaysTickArray.Empty();
		UAnimNodeSlot::ReleaseSequenceNodes(this);

		UAnimTree* RESTRICT Tree			= Cast<UAnimTree>(Animations);

		if (Tree && AnimTreeTemplate && AnimTreeTemplate->bEnablePooling && !GIsEditor)
		{	
			// Return it to the object pool if needed
			Tree->ReturnToPool();
			Animations = NULL;
		}		
	}

	Super::Detach( bWillReattach );

	for(INT AttachmentIndex = 0;AttachmentIndex < Attachments.Num();AttachmentIndex++)
	{
		if(Attachments(AttachmentIndex).Component)
		{
			Attachments(AttachmentIndex).Component->ConditionalDetach(bWillReattach);
		}
	}

#if WITH_FACEFX
	// NOTE: Our FaceFX actor instance persists during reattachments, because we want the actor to retain it's current
	//   animation state even during lighting, material and interp property changes!
  	if( !bWillReattach )
  	{
		// Destroy our FaceFX instance if we have one
		if( FaceFXActorInstance != NULL )
		{
			delete FaceFXActorInstance;
			FaceFXActorInstance = NULL;
		}
  	}
#endif // WITH_FACEFX

#if WITH_APEX
  	if( !bWillReattach && (ApexClothing != NULL) )
  	{
		ReleaseApexClothing(); // release any previously allocated apex clothing
	}
#endif

	if(MeshObject)
	{
		// Begin releasing the RHI resources used by this skeletal mesh component.
		// This doesn't immediately destroy anything, since the rendering thread may still be using the resources.
		MeshObject->ReleaseResources();

		// Begin a deferred delete of MeshObject.  BeginCleanup will call MeshObject->FinishDestroy after the above release resource
		// commands execute in the rendering thread.
		BeginCleanup(MeshObject);
		MeshObject = NULL;
	}
}

/** Check if this actor is used by Matinee or not 
 * @param : ActorToCheck 
 * @return : TRUE if so, FALSE otherwise
 * This only works in editor : do not attempt to use this in game
 */
UBOOL IsMatineeBeingOpenedAndUsing( AActor * ActorToCheck )
{
	/* Cached variable for protecting Animsets during Matinee Editing 
	 * CanEditChange gets called multiple times when focus changes or not
	 * Iterating through all objects via that slows down editor a lot - cache at least for 1 second
	 * Doing more than that might be dangerous 
	 */
	// if none, do not check
	if ( !ActorToCheck )
	{
		return FALSE;
	}

	check(GPropertyWindowDataCache);

	const TArray<UObject*>& CachedEditedSeqActInterps = GPropertyWindowDataCache->GetEditedSeqActInterps();

	for( INT InterpIndex = 0; InterpIndex < CachedEditedSeqActInterps.Num(); ++InterpIndex )
	{
		USeqAct_Interp *Interp = Cast<USeqAct_Interp>(CachedEditedSeqActInterps(InterpIndex));
		check ( Interp && Interp->bIsBeingEdited );

		// Iterate over all groups to find used actors
		for( INT GroupIndex=0; GroupIndex<Interp->GroupInst.Num(); GroupIndex++ )
		{
			UInterpGroupInst * GroupInst = Interp->GroupInst(GroupIndex);
			// if same, return TRUE;
			if (GroupInst->GetGroupActor() == ActorToCheck)
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**
 * Called by the editor to query whether a property of this object is allowed to be modified.
 * The property editor uses this to disable controls for properties that should not be changed.
 * When overriding this function you should always call the parent implementation first.
 *
 * @param	InProperty	The property to query
 *
 * @return	TRUE if the property can be modified in the editor, otherwise FALSE
 */
UBOOL USkeletalMeshComponent::CanEditChange( const UProperty* InProperty ) const
{
	UBOOL bIsEditable = Super::CanEditChange( InProperty );
	if( bIsEditable && InProperty != NULL )
	{
		if ( InProperty->GetFName() == TEXT( "AnimSets" ) )
		{
			// If the mesh source data has been stripped then we'll disable certain property
			// window controls as these settings are now locked in place with the built mesh.
			if( IsMatineeBeingOpenedAndUsing(GetOwner()) )
			{
				bIsEditable = FALSE;
			}
		}
		else if( InProperty->GetFName() == TEXT( "bUsePrecomputedShadows" ) )
		{
			// Skeletal Mesh Components do not support using precomputed shadows
			bIsEditable = FALSE;
		}
	}

	return bIsEditable;
}


void USkeletalMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	if ( PropertyChangedEvent.Property != NULL )
	{
		if ( PropertyChangedEvent.Property->GetFName() == TEXT( "AnimSets" ) )
		{
			// Saved default animsets. 
			// If it's not used by Matinee, overwrite cached value
			if ( IsMatineeBeingOpenedAndUsing(GetOwner()) == FALSE )
			{
				// You can't call SaveAnimSets - that will modify memory being modified via this call stack causing memory corruption
				TemporarySavedAnimSets = AnimSets;
				bValidTemporarySavedAnimSets = TRUE;
			}
			else
			{
				appMsgf(AMT_OK, TEXT("This actor is being used by Matinee. Close Matinee to modify AnimSets."));
			}
		}
		// if animtree template, and if that's null, create new animnodesequence
		if ( PropertyChangedEvent.Property->GetFName() == TEXT( "AnimTreeTemplate" ) )
		{
			if (AnimTreeTemplate == NULL)
			{
				Animations = CastChecked<UAnimNodeSequence>( UObject::StaticConstructObject(UAnimNodeSequence::StaticClass(), this) );
			}
		}

		if ( GIsEditor && PropertyThatChanged->GetName() == TEXT("StreamingDistanceMultiplier") )
		{
			// Recalculate in a few seconds.
			ULevel::TriggerStreamingDataRebuild();
		}
	}

	// Push cloth params to cloth sim
	UpdateClothParams();
}

/**
 * Called to finish destroying the object.  After UObject::FinishDestroy is called, the object's memory should no longer be accessed.
 */
void USkeletalMeshComponent::FinishDestroy()
{
#if WITH_FACEFX
	// Destroy our FaceFX instance if we have one.  We can't always rely on Detach() cleaning this up, because of
	// cases where a 'Reattach' was enqueued but we never had a chance to get around to doing that
	if( FaceFXActorInstance != NULL )
	{
		delete FaceFXActorInstance;
		FaceFXActorInstance = NULL;
	}
#endif // WITH_FACEFX

	// make sure cloth is always cleaned up before we destroy the arrays that the cloth sim
	// has pointers into
	if (ClothSim)
	{
		TermClothSim(NULL);
	}

	// clean up the 
	if (SoftBodySim)
	{
		TermSoftBodySim(NULL);
	}

	// Call parent implementation
	Super::FinishDestroy();
}

void USkeletalMeshComponent::TickAnimNodes(FLOAT DeltaTime)
{
	// before start ticking, clear unused for released node from last tick
	// I can't do after tick since other component is still referencing - i.e. sound notifier
	UAnimNodeSlot::FlushReleasedSequenceNodes(this);

	// Get AnimTree, and if we are using saved pose, skip ticking altogether
	UAnimTree* RESTRICT AnimTree = Cast<UAnimTree>(Animations);
	if( AnimTree && AnimTree->bUseSavedPose && ParentAnimComponent )
	{
		return;
	}

	// SyncGroup pre tick update
	if( AnimTree )
	{
		AnimTree->SyncGroupPreTickUpdate();
	}

	check( AnimTickArray.Num() == AnimTickWeightsArray.Num() && AnimTickRelevancyArray.Num() == AnimTickArray.Num() );
	const INT AnimNodeCount = AnimTickArray.Num();

	// Reset weights
	appMemzero((BYTE*)AnimTickWeightsArray.GetData(), AnimNodeCount * sizeof(FLOAT));

	TickTag++;
	check(Animations->SkelComponent == this);

	AnimTickWeightsArray(0) = 1.f;

	for(INT i=0; i<AnimNodeCount; ++i)
	{
		INT& bRelevant = AnimTickRelevancyArray(i);
		const FLOAT& NodeWeight = AnimTickWeightsArray(i);

		// Call final blend relevancy notifications
		if( !bRelevant )
		{
			// Node not relevant, skip to next one
			if( NodeWeight <= ZERO_ANIMWEIGHT_THRESH )
			{
				continue;
			}
			// node becoming relevant this frame.
			else
			{
				bRelevant = TRUE;

				UAnimNode* Node = AnimTickArray(i);
				Node->bRelevant = TRUE;
				Node->bJustBecameRelevant = TRUE;
				Node->OnBecomeRelevant();
			}
		}
		else
		{
			UAnimNode* Node = AnimTickArray(i);

			if( NodeWeight <= ZERO_ANIMWEIGHT_THRESH )
			{
				bRelevant = FALSE;

				// Node is not going to be ticked, but still update NodeTickTag, if we do things in OnCeaseRelevant that rely on that.
				Node->NodeTickTag = TickTag;
				Node->OnCeaseRelevant();
				Node->bRelevant = FALSE;
				Node->bJustBecameRelevant = FALSE;
				// Update the node's new weight too.
				Node->NodeTotalWeight = NodeWeight;

				// not relevant, not going to be ticked, go to next one.
				continue;
			}

			Node->bJustBecameRelevant = FALSE;
		}

		// Clear just became relevant flag
		UAnimNode* Node = AnimTickArray(i);
		// Set proper weight on the node.
		Node->NodeTotalWeight = NodeWeight;

		// Call Deferred InitAnim if we have to.
		if( Node->NodeInitTag != InitTag )
		{
			Node->NodeInitTag = InitTag;
			Node->DeferredInitAnim();
		}

		// If we are not skipping because of zero weight, call the Tick function.
		// Also check if all anims are paused, or this is ticked anyway
		if( !bPauseAnims || Node->bTickDuringPausedAnims )
		{
			Node->NodeTickTag = TickTag;

#if !FINAL_RELEASE && PERF_SHOW_ANIMNODE_TICK_TIMES
			DOUBLE Start = 0.f;
			if( GShouldLogOutAFrameOfSkelCompTick )
			{
				Start = appSeconds();
			}
#endif

			// Call Tick() on node, to update child weights.
			Node->TickAnim(DeltaTime);

#if !FINAL_RELEASE && PERF_SHOW_ANIMNODE_TICK_TIMES
			if( GShouldLogOutAFrameOfSkelCompTick )
			{
				DOUBLE End = appSeconds();
				debugf(TEXT("-- %s - %s:\t%fms"), SkeletalMesh?*SkeletalMesh->GetName():TEXT("None"), *Node->GetName(), (End-Start)*1000.f);
			}
#endif
		}
	}

	// Handle Nodes that should always be ticked even when not relevant.
	INT NumAlwaysTickNode = AnimAlwaysTickArray.Num();
	for(INT i=0; i<AnimAlwaysTickArray.Num(); i++)
	{
		UAnimNode* Node = AnimAlwaysTickArray(i);

		// Call Deferred InitAnim if we have to.
		if( Node->NodeInitTag != InitTag )
		{
			Node->NodeInitTag = InitTag;
			Node->DeferredInitAnim();
		}

		// Only tick nodes which haven't been ticked previously.
		if( Node->NodeTickTag != TickTag )
		{
			Node->NodeTickTag = TickTag;
			Node->TickAnim(DeltaTime);

			// If NodeCount changed, one or more nodes have been removed.
			// Restart from beginning in that case.
			if( NumAlwaysTickNode != AnimAlwaysTickArray.Num() )
			{
				i = 0;
				// Set new size
				NumAlwaysTickNode = AnimAlwaysTickArray.Num();
			}
		}
	}

	// After all nodes have been ticked, and weights have been updated, take another pass for AnimNodeSequence groups.
	// (Anim Synchronization, and notification groups).
	if( AnimTree && !bPauseAnims)
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimSyncGroupTime);
		AnimTree->UpdateAnimNodeSeqGroups(DeltaTime);
	}
}

/** Update the PredictedLODLevel and MaxDistanceFactor in the component from its MeshObject. */
UBOOL USkeletalMeshComponent::UpdateLODStatus()
{
	// Predict the best (min) LOD level we are going to need. Basically we use the Min (best) LOD the renderer desired last frame.
	// Because we update bones based on this LOD level, we have to update bones to this LOD before we can allow rendering at it.

	// Support forcing to a particular LOD.
	if(ForcedLodModel > 0)
	{
		PredictedLODLevel = ::Clamp(ForcedLodModel - 1, 0, SkeletalMesh->LODModels.Num()-1);
	}
	else
	{
		// If no MeshObject - just assume lowest LOD.
		if(MeshObject)
		{
			PredictedLODLevel = ::Clamp(MeshObject->MinDesiredLODLevel + GSystemSettings.SkeletalMeshLODBias, 0, SkeletalMesh->LODModels.Num()-1);
		}
		else
		{
			PredictedLODLevel = SkeletalMesh->LODModels.Num()-1;
		}
	}

	// now check to see if we have a MinLODLevel
	if( ( MinLodModel > 0 ) && ( MinLodModel <= SkeletalMesh->LODModels.Num()-1 ) )
	{
		PredictedLODLevel = ::Clamp(PredictedLODLevel, MinLodModel, SkeletalMesh->LODModels.Num()-1);
	}

	// See if LOD has changed. 
	UBOOL bLODChanged = (PredictedLODLevel != OldPredictedLODLevel);
	OldPredictedLODLevel = PredictedLODLevel;

	//If so, we need to recalc required bones.	
	if( bLODChanged )
	{
		bRequiredBonesUpToDate = FALSE;
	}

	// Read back MaxDistanceFactor from the render object.
	if(MeshObject)
	{
		MaxDistanceFactor = MeshObject->MaxDistanceFactor;

#if CHART_DISTANCE_FACTORS
		// Only chart DistanceFactor if it was actually rendered recently
		if(bChartDistanceFactor && ((LastRenderTime > GWorld->GetWorldInfo()->TimeSeconds - 1.0f) || bUpdateSkelWhenNotRendered))
		{
			AddDistanceFactorToChart(MaxDistanceFactor);
		}
#endif // CHART_DISTANCE_FACTORS

		// Cloth LOD
		if ( ClothSim )
		{
			ClothDynamicBlendWeight = ClothBlendWeight;

			// If using cloth distance factor
			if(ClothBlendMinDistanceFactor >= 0.0f)
			{
				if ( MaxDistanceFactor < ClothBlendMinDistanceFactor )
				{
					ClothDynamicBlendWeight = 0.0f;
				}
				else if ( MaxDistanceFactor < ClothBlendMaxDistanceFactor )
				{
					ClothDynamicBlendWeight = (MaxDistanceFactor - ClothBlendMinDistanceFactor) / (ClothBlendMaxDistanceFactor - ClothBlendMinDistanceFactor);
					ClothDynamicBlendWeight *= ClothBlendWeight;
				}
			}
		}

#if WITH_APEX
		if(bLODChanged)
		{
			FIApexClothing *Clothing = GetApexClothing();
			if ( Clothing )
			{
				Clothing->SetGraphicalLod(PredictedLODLevel);
			}
		}
#endif

	}

	return bLODChanged;
}

/** Initialize the LOD entries for the component */
void USkeletalMeshComponent::InitLODInfos()
{
	if (SkeletalMesh != NULL)
	{
		if (SkeletalMesh->LODInfo.Num() != LODInfo.Num())
		{
			LODInfo.Empty(SkeletalMesh->LODInfo.Num());
			for (INT Idx=0; Idx < SkeletalMesh->LODInfo.Num(); Idx++)
			{
				new(LODInfo) FSkelMeshComponentLODInfo();
			}
		}
		
		for (INT Idx=0; Idx < SkeletalMesh->LODInfo.Num(); Idx++)
		{
			const FStaticLODModel& LODModel = SkeletalMesh->LODModels(Idx);
			FSkelMeshComponentLODInfo& MeshLODInfo = LODInfo(Idx);
			if (LODModel.VertexInfluences.Num() > 0)
			{
				const INT VertInfIdx = 0;
				const FSkeletalMeshVertexInfluences& VertexInfluences = LODModel.VertexInfluences(VertInfIdx);
				MeshLODInfo.InstanceWeightUsage = VertexInfluences.Usage;
				MeshLODInfo.InstanceWeightIdx = VertInfIdx;
			}
			else
			{
				MeshLODInfo.bNeedsInstanceWeightUpdate = FALSE;
				MeshLODInfo.bAlwaysUseInstanceWeights = FALSE;
				MeshLODInfo.InstanceWeightUsage = IWU_PartialSwap;
				MeshLODInfo.InstanceWeightIdx = INDEX_NONE;
			}
		}
	}	
}

void USkeletalMeshComponent::Tick(FLOAT DeltaTime)
{
	INC_DWORD_STAT(STAT_SkelComponentTickCount);

	SCOPE_CYCLE_COUNTER(STAT_SkelComponentTickTime);

	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

	// See if this mesh was rendered recently.
	bRecentlyRendered = !WorldInfo || (LastRenderTime > WorldInfo->TimeSeconds - 1.0f);
	// SkelMeshComp to use for optimizations. For attachments, use our base skeletalmeshcomponent. So everything is in sync.
	USkeletalMeshComponent* BaseSkelMeshComp = AttachedToSkelComponent ? AttachedToSkelComponent : this;

    // Adjust this based on LOD, distance, etc
    SkipRateForTickAnimNodesAndGetBoneAtoms = 0;
	// Do not perform distance based optimizations during cinematics. Art team is responsible for performance.
	if( WorldInfo && !WorldInfo->bInteractiveMode && bUseTickOptimization )
	{
		if( !bRecentlyRendered )
		{
			SkipRateForTickAnimNodesAndGetBoneAtoms = 4;
		}
		else if( MaxDistanceFactor > .3 )
		{
			SkipRateForTickAnimNodesAndGetBoneAtoms = 0;
		}
		else if( MaxDistanceFactor > .15 )
		{
			SkipRateForTickAnimNodesAndGetBoneAtoms = 1;
		}
		else if( MaxDistanceFactor > .075 )
		{
			SkipRateForTickAnimNodesAndGetBoneAtoms = 2;
		}
		else 
		{
			SkipRateForTickAnimNodesAndGetBoneAtoms = 3;
		}
		
		// Be more aggressive if in split screen
		const UBOOL bInSplitScreen = GEngine->IsSplitScreen();
		if( bInSplitScreen )
		{
			SkipRateForTickAnimNodesAndGetBoneAtoms++;
		}

		// we will only adjust LastDropRate once a second (with random variation), then keep using that for a second
		if( WorldInfo->TimeSeconds - LastDropRateChange > 1.0f )
		{
			LastDropRateChange = WorldInfo->TimeSeconds + appFrand()* 0.5f - 0.25f; // randomize this a bit
			LastDropRate = WorldInfo->bDropDetail ? 1 : 0;
		}
		SkipRateForTickAnimNodesAndGetBoneAtoms += LastDropRate;

		APawn* PawnOwner = BaseSkelMeshComp->Owner ? BaseSkelMeshComp->Owner->GetAPawn() : NULL;
		if( PawnOwner )
		{
			// Locally controlled human player - never skip a beat there.
			if( PawnOwner->IsHumanControlled() && PawnOwner->IsLocallyControlled() )
			{
				SkipRateForTickAnimNodesAndGetBoneAtoms = 0;
			}
			//  We can be more aggressive with dead Pawns.
			else if( !PawnOwner->IsAliveAndWell() )
			{
				SkipRateForTickAnimNodesAndGetBoneAtoms++;
			}
		}

		// 4 is max right now for non rendered and 3 for rendered, we're losing too much beyond that point @ 30FPS.
		SkipRateForTickAnimNodesAndGetBoneAtoms = Min<FLOAT>(SkipRateForTickAnimNodesAndGetBoneAtoms, bRecentlyRendered ? 3 : 4);
	}

#if !FINAL_RELEASE
	// Visualize skeletal meshes using colored glow.
	if( GVisualizeSkeletalMeshTickOptimization )
	{
		UDynamicLightEnvironmentComponent* DynLightEnv = Cast<UDynamicLightEnvironmentComponent>( LightEnvironment );
		if( DynLightEnv )
		{
			// If we are sharing this light environment don't override the color. We'll just let the master SkelMesh drive the color for its attachments.
			if( !AttachedToSkelComponent || (AttachedToSkelComponent->LightEnvironment != DynLightEnv) )
			{
				FLinearColor NewColor = FLinearColor(0,0,0,1);
				switch( SkipRateForTickAnimNodesAndGetBoneAtoms )
				{
					case 2 : // Green
						NewColor.G = 10;
						break;

					case 3 : // Yellow
						NewColor.R = 10;
						NewColor.G = 10;
						break;

					case 4 : // Red
						NewColor.R = 10;
						break;

					default:
						// no glow
						break;
				}

				if( DynLightEnv->AmbientGlow != NewColor )
				{
					DynLightEnv->AmbientGlow = NewColor;
					DynLightEnv->ResetEnvironment();
				}
			}
		}
	}
#endif // !FINAL_RELEASE

	INT EffectiveSkipRateForTickAnimNodesAndGetBoneAtoms = SkipRateForTickAnimNodesAndGetBoneAtoms;
	// Leave this at one for the existing tweakable optimizations, LowUpdateFrameRate and AnimationLODFrameRate
	INT EffectiveSkipRateForGetBoneAtoms = 1;
	{
		if (!BaseSkelMeshComp->bRecentlyRendered)
		{
			EffectiveSkipRateForGetBoneAtoms = Max<FLOAT>( EffectiveSkipRateForGetBoneAtoms, BaseSkelMeshComp->LowUpdateFrameRate);
		}

		if (BaseSkelMeshComp->MaxDistanceFactor < BaseSkelMeshComp->AnimationLODDistanceFactor)
		{
			EffectiveSkipRateForGetBoneAtoms = Max<FLOAT>( EffectiveSkipRateForGetBoneAtoms, BaseSkelMeshComp->AnimationLODFrameRate);
		}
	}

	// don't skip if we are doing root motion
	if (RootMotionMode != RMM_Ignore || PreviousRMM != RMM_Ignore || RootMotionRotationMode != RMRM_Ignore)
	{
		EffectiveSkipRateForTickAnimNodesAndGetBoneAtoms = 0;
		EffectiveSkipRateForGetBoneAtoms = 0;
	}

	// Update FrameCount.
	TickCount++;
	INT FrameCount = TickCount;
	{
		AActor* ActorOwner = BaseSkelMeshComp->Owner;
		if( ActorOwner )
		{
			// Spread the load per Owner.
			if( (ActorOwner->SkelMeshCompTickTag == 0) && WorldInfo )
			{
				ActorOwner->SkelMeshCompTickTag = (++WorldInfo->SkelMeshCompTickTagCount);
			}

			FrameCount += ActorOwner->SkelMeshCompTickTag;
		}
	}

	// periodic frame skip
	bSkipTickAnimNodes = FALSE;
	bSkipGetBoneAtoms = FALSE;
	{
		if ( EffectiveSkipRateForTickAnimNodesAndGetBoneAtoms > 1 )
		{
			if ((FrameCount % EffectiveSkipRateForTickAnimNodesAndGetBoneAtoms) > 0)
			{
				bSkipTickAnimNodes = TRUE;
				bSkipGetBoneAtoms = TRUE;
			}
		}
		else if ( EffectiveSkipRateForGetBoneAtoms > 1 )
		{
			if ((FrameCount % EffectiveSkipRateForGetBoneAtoms) > 0)
			{
				bSkipGetBoneAtoms = TRUE;
			}
		}

		// Never skip ticking AnimNodes for human controlled pawns on server.
		// Gameplay depends on animations for events to do special moves, to be able to fire, etc.
		// So we don't want to delay those and keep network play and replication to client as responsive as possible.
		// This only matters to this mesh, not BaseSkelMeshComp.
		APawn* PawnOwner = Owner ? Owner->GetAPawn() : NULL;
		if( bSkipTickAnimNodes && PawnOwner && PawnOwner->IsHumanControlled() )
		{
			bSkipTickAnimNodes = FALSE;
		}
	}

	// Interpolate if we are skipping and we are visible
	bInterpolateBoneAtoms = bRecentlyRendered && ((EffectiveSkipRateForTickAnimNodesAndGetBoneAtoms > 1) || (EffectiveSkipRateForGetBoneAtoms > 1));

	// Skip component ticking altogether if not visible and skipping TickAnimNodes.
	if( bSkipTickAnimNodes && bSkipGetBoneAtoms && !bRecentlyRendered &&
		bRequiredBonesUpToDate && // and we have the right set of bones
		SkeletalMesh && SkeletalMesh->RefSkeleton.Num() == LocalAtoms.Num() && SkeletalMesh->RefSkeleton.Num() == SpaceBases.Num() ) // and the arrays are the right size
	{
		ComponentDroppedDeltaTime += DeltaTime;
		return;
	}
	// Otherwise tick...
	DeltaTime += ComponentDroppedDeltaTime;
	ComponentDroppedDeltaTime = 0.f;

	FLOAT TimeDilation = GetOwner() ? GetOwner()->CustomTimeDilation : 1.f;

#if PERF_SHOW_SKELETAL_MESH_COMPONENT_TICK_TIME || LOOKING_FOR_PERF_ISSUES
	const DOUBLE TickStart = appSeconds();	
	DOUBLE UpdatePoseTotal = 0.0;
	DOUBLE TickNodesTotal = 0.0;
	DOUBLE UpdateTransformTotal = 0.0;
	DOUBLE UpdateRBTotal = 0.0;
#endif

	// If in-game, tick all animation channels in our anim nodes tree. Dont want to play animation in level editor.
	const UBOOL bHasBegunPlay = GWorld->HasBegunPlay();

	if( !bSkipTickAnimNodes  )
	{
		if( Animations && bHasBegunPlay && !bNoSkeletonUpdate && IsAttached() )
		{
			SCOPE_CYCLE_COUNTER(STAT_AnimTickTime);
#if PERF_SHOW_SKELETAL_MESH_COMPONENT_TICK_TIME || LOOKING_FOR_PERF_ISSUES
			TickNodesTotal -= appSeconds();
#endif

			// we should not tick anim nodes if we are have not been rendered recently and are not updating when skel is not rendered (aka bUpdateSkelWhenNotRendered = FALSE )
			const UBOOL bShouldTickAnimNodes = (bRecentlyRendered || bTickAnimNodesWhenNotRendered);
			
			if( bShouldTickAnimNodes == TRUE )
			{
				INC_DWORD_STAT(STAT_SkelComponentTickNodesCount);
				TickAnimNodes((DeltaTime + AccumulatedDroppedDeltaTime) * TimeDilation);
			}

			// skel controls always need to be ticked.  And then on the specific skel control there is a flag to not update
			// if it has not been recently rendered.  We still pay the iteration cost.
			TickSkelControls((DeltaTime + AccumulatedDroppedDeltaTime) * TimeDilation);

#if PERF_SHOW_SKELETAL_MESH_COMPONENT_TICK_TIME || LOOKING_FOR_PERF_ISSUES
			TickNodesTotal += appSeconds();
			TickNodesTotal *= 1000.f;
#endif
		}
		AccumulatedDroppedDeltaTime = 0.0f;
	}
	else
	{
		AccumulatedDroppedDeltaTime += DeltaTime;
	}
	DeltaTime *= TimeDilation;

	// If we have cloth, apply wind forces to make it flutter, each frame.
	if(ClothSim)
	{
		// Do 'auto freeezing'
		if( bAutoFreezeClothWhenNotRendered )
		{
			// If we have not been rendered for a while, or we are fully animated, freeze cloth
			if ( !bRecentlyRendered || (ClothDynamicBlendWeight <= 0.0f) )
			{
				if ( !bClothFrozen )
				{
					SetClothFrozen(TRUE);
				}
			}
			else if ( bClothFrozen )
			{
				SetClothFrozen(FALSE);
			}
		}

		// If not frozen - update wind forces.
		if(!bClothFrozen)
		{
			UpdateClothWindForces(DeltaTime);
		}
	}

	if(SoftBodySim)
	{
		if(bAutoFreezeSoftBodyWhenNotRendered)
		{
			// If we have not been rendered for a while, and are not yet frozen, do it now
			if(!bRecentlyRendered && !bSoftBodyFrozen)
			{
				SetSoftBodyFrozen(TRUE);
			}
			// If we have been rendered recently, and are still frozen, unfreeze.
			else if(bRecentlyRendered && bSoftBodyFrozen)
			{
				SetSoftBodyFrozen(FALSE);
			}
		}
	}

	// Save this off before we call BeginDeferredUpdateTransform.
	const UBOOL bNeedsUpdateTransform = NeedsUpdateTransform();

	// See if we are going to need to update kinematics
	const UBOOL bUpdateKinematics = (!bUseSingleBodyPhysics && PhysicsAssetInstance && bUpdateKinematicBonesFromAnimation && !bNotUpdatingKinematicDueToDistance);

	// If we need it, find the up-to-date transform for this component. 
	FMatrix ParentTransform = FMatrix::Identity;
	// Work with an updated LocalToWorld matrix, as we might need one for bone controllers
	if( bNeedsUpdateTransform && Owner )
	{
		// We have a special case for when its attached to another SkelComp.
		if( AttachedToSkelComponent )
		{
			ParentTransform = AttachedToSkelComponent->CalcAttachedSkelCompMatrix(this);
		}
		else
		{
			ParentTransform = Owner->LocalToWorld();
		}
		
		// Update CachedParentToWorld
		SetParentToWorld(ParentTransform);
		// Updates LocalToWorld matrix based on CachedParentToWorld
		SetTransformedToWorld();
	}

	// Update component's LOD settings
	const UBOOL bLODHasChanged = UpdateLODStatus();

	// We can skip doing some work when using PHYS_RigidBody and we have physics that are asleep
	if(Owner && Owner->Physics == PHYS_RigidBody && (BodyInstance || PhysicsAssetInstance))
	{
		// Update count of how long physics for this actor has been asleep.
		const UBOOL bAsleep = !RigidBodyIsAwake();
		if(bAsleep)
		{
			FramesPhysicsAsleep++;
		}
		else
		{
			FramesPhysicsAsleep = 0;
		}
	}
	else
	{
		FramesPhysicsAsleep = 0;
	}

	// update the instanced influence weights if needed
	for (INT LODIdx=0; LODIdx<LODInfo.Num(); LODIdx++)
	{
		if( LODInfo(LODIdx).bNeedsInstanceWeightUpdate )
		{
			UpdateInstanceVertexWeights(LODIdx);
		}
	}

	// If we have been recently rendered, and bForceRefPose has been on for at least a frame, or the LOD changed, update bone matrices.
	if(((bRecentlyRendered || bUpdateSkelWhenNotRendered) && !(bForceRefpose && bOldForceRefPose)) || bLODHasChanged)
	{
		// Do not update bones if we are taking bone transforms from another SkelMeshComp
		if(!ParentAnimComponent && !bNoSkeletonUpdate)
		{
#if PERF_SHOW_SKELETAL_MESH_COMPONENT_TICK_TIME || LOOKING_FOR_PERF_ISSUES
			DOUBLE UpdatePoseStart = appSeconds();
#endif
			// Update the mesh-space bone transforms held in SpaceBases array from animation data.
			UpdateSkelPose( DeltaTime ); 

#if PERF_SHOW_SKELETAL_MESH_COMPONENT_TICK_TIME || LOOKING_FOR_PERF_ISSUES
			UpdatePoseTotal = (appSeconds() - UpdatePoseStart) * 1000.f;
#endif
		}
		else if (bUpdateMorphWhenParentAnimComponentExists)
		{
			UpdateMorph( DeltaTime );
		}


#if PERF_SHOW_SKELETAL_MESH_COMPONENT_TICK_TIME || LOOKING_FOR_PERF_ISSUES
		UpdateTransformTotal -= appSeconds();
#endif

		// If desired, force attachments into the correct position now.
		if( bForceUpdateAttachmentsInTick )
		{
			if( bNeedsUpdateTransform && Owner )
			{
				ConditionalUpdateTransform(ParentTransform);
			}
			else
			{
				ConditionalUpdateTransform();
			}
			UpdateChildComponents();
		}
		// Otherwise, make sure that we do call UpdateTransform later this frame, as that is where transforms are sent to rendering thread
		else
		{
			BeginDeferredUpdateTransform();
		}

#if PERF_SHOW_SKELETAL_MESH_COMPONENT_TICK_TIME || LOOKING_FOR_PERF_ISSUES		
		UpdateTransformTotal += appSeconds();
		UpdateTransformTotal *= 1000.f;
#endif
	}

	// Update bOldForceRefPose
	bOldForceRefPose = bForceRefpose;

	// If desired, update physics bodies associated with skeletal mesh component to match.
	// should not be in the above bUpdateSkelWhenNotRendered block so that physics gets properly updated if the bodies are moving due to other sources (e.g. actor movement)
	if( bUpdateKinematics )
	{
#if PERF_SHOW_SKELETAL_MESH_COMPONENT_TICK_TIME || LOOKING_FOR_PERF_ISSUES
		UpdateRBTotal -= appSeconds();
#endif

		UpdateRBBonesFromSpaceBases(LocalToWorld, FALSE, FALSE);

#if PERF_SHOW_SKELETAL_MESH_COMPONENT_TICK_TIME || LOOKING_FOR_PERF_ISSUES
		UpdateRBTotal += appSeconds();
		UpdateRBTotal *= 1000.f;
#endif
	}

	// Update fixed vertices in cloth to match graphics location.
	UpdateFixedClothVerts();

#if PERF_SHOW_SKELETAL_MESH_COMPONENT_TICK_TIME || LOOKING_FOR_PERF_ISSUES
	const DOUBLE TickTotal = (appSeconds() - TickStart) * 1000.f;
	if( GShouldLogOutAFrameOfSkelCompTick == TRUE )
	{
		static bool bShouldLogColumnHeadings = TRUE;
		if( bShouldLogColumnHeadings == TRUE )
		{
			bShouldLogColumnHeadings = FALSE;
			debugf(NAME_DevAnim,TEXT( "SkelMeshComp: Name SkelMeshName Owner TickTotal UpdatePoseTotal TickNodesTotal UpdateTransformTotal UpdateRBTotal" ) );
		}

		debugf(NAME_DevAnim,TEXT( "SkelMeshComp: %s %s %s %f %f %f %f %f" ), *this->GetPathName(), *SkeletalMesh->GetPathName(), *GetOwner()->GetName(), TickTotal, UpdatePoseTotal, TickNodesTotal, UpdateTransformTotal, UpdateRBTotal );
	}
#endif
#if WITH_APEX
	TickApexClothing(DeltaTime);
#endif

#if !FINAL_RELEASE
	extern UBOOL GShouldLogOutAFrameOfSkelMeshLODs;
	if( GShouldLogOutAFrameOfSkelMeshLODs == TRUE )
	{
		if (SkeletalMesh)
		{		
			SkeletalMesh->DebugVerifySkeletalMeshLOD();
		}
	}

	extern UBOOL GShouldLogOutAFrameOfFaceFXDebug;
	extern UBOOL GShouldTraceFaceFX;
	if ( GShouldLogOutAFrameOfFaceFXDebug == TRUE )
	{
		if ( GShouldTraceFaceFX == TRUE )
		{
			// Once trace, please turn the log off since this does not need to be done for every skeletalmesh
			TraceFaceFX( TRUE );

			debugf(TEXT("============================================================"));
			GShouldLogOutAFrameOfFaceFXDebug = FALSE;
		}
	}

	extern UBOOL GShouldLogOutAFrameOfFaceFXBones;
	if ( GShouldLogOutAFrameOfFaceFXBones == TRUE )
	{
		DebugVerifyFaceFXBoneList();
	}

#endif // !FINAL_RELEASE
}

INT USkeletalMeshComponent::GetNumElements() const
{
	return SkeletalMesh ? SkeletalMesh->Materials.Num() : 0;
}

//
//	USkeletalMeshComponent::GetMaterial
//

UMaterialInterface* USkeletalMeshComponent::GetMaterial(INT MaterialIndex) const
{
	if(MaterialIndex < Materials.Num() && Materials(MaterialIndex))
	{
		return Materials(MaterialIndex);
	}
	else if(SkeletalMesh && MaterialIndex < SkeletalMesh->Materials.Num() && SkeletalMesh->Materials(MaterialIndex))
	{
		return SkeletalMesh->Materials(MaterialIndex);
	}
	else
	{
		return NULL;
	}
}

/**
 *	Attach a Component to the bone of this SkeletalMeshComponent at the supplied offset.
 *	If you are a attaching to a SkeletalMeshComponent that is using another SkeletalMeshComponent for its bone transforms (via the ParentAnimComponent pointer)
 *	you should attach to that component instead.
 */
void USkeletalMeshComponent::AttachComponent(UActorComponent* Component,FName BoneName,FVector RelativeLocation,FRotator RelativeRotation,FVector RelativeScale)
{
	if( IsPendingKill() )
	{
		debugf(NAME_DevAnim,TEXT("USkeletalMeshComponent::AttachComponent: Trying to attach '%s' to '%s' which IsPendingKill. Aborting"), *(Component->GetDetailedInfo()), *(this->GetDetailedInfo()) );
		return;
	}

	Component->DetachFromAny();

	if(ParentAnimComponent)
	{
		debugf(NAME_DevAnim,
			TEXT("SkeletalMeshComponent %s in Actor %s has a ParentAnimComponent - should attach Component %s to that instead."), 
			*GetPathName(),
			Owner ? *Owner->GetPathName() : TEXT("None"),
			*Component->GetPathName()
			);
		return;
	}

	// Add the component to the attachments array.
	new(Attachments) FAttachment(Component,BoneName,RelativeLocation,RelativeRotation,RelativeScale);

	// Set pointer is a skeletal mesh component
	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Component);
	if(SkelComp)
	{
		SkelComp->AttachedToSkelComponent = this;
	}

	if(IsAttached())
	{
		// Attach the component to the scene.
		INT BoneIndex = MatchRefBone(BoneName);
		if( BoneIndex != INDEX_NONE && BoneIndex < SpaceBases.Num() )
		{
			const FMatrix& AttachmentToWorld = FScaleRotationTranslationMatrix(RelativeScale,RelativeRotation,RelativeLocation) * SpaceBases(BoneIndex).ToMatrix() * LocalToWorld;
			SetAttachmentOwnerVisibility(Component);
			Component->ConditionalAttach(Scene,Owner,AttachmentToWorld);
		}
		else
		{
#if !PS3 // hopefully will be found on PC
			debugf(NAME_DevAnim,TEXT("USkeletalMeshComponent::AttachComponent : Could not find bone '%s' index: %d SpaceBases: %d in %s attaching: %s (%s %s)"), *BoneName.ToString(), BoneIndex, SpaceBases.Num(), *GetOwner()->GetName(), *Component->TemplateName.ToString(), *Component->GetName(), *Component->GetDetailedInfo() );
#endif
		}
	}

	// Notify the texture streaming system about the new component
	const UPrimitiveComponent* Primitive = ConstCast<UPrimitiveComponent>(Component);
	if ( Primitive )
	{
		GStreamingManager->NotifyPrimitiveAttached( Primitive, DPT_Spawned );
	}
}

/**
 * Function that returns whether or not CPU skinning should be applied
 * Allows the editor to override the skinning state for editor tools
 */
UBOOL USkeletalMeshComponent::ShouldCPUSkin()
{
  return FALSE;
}

/** Version of AttachComponent that uses Socket data stored in the Skeletal mesh to attach a component to a particular bone and offset. */
void USkeletalMeshComponent::AttachComponentToSocket(UActorComponent* Component,FName SocketName)
{
	if( SkeletalMesh != NULL )
	{
		USkeletalMeshSocket* Socket = SkeletalMesh->FindSocket(SocketName);
		if( Socket )
		{
			AttachComponent(Component, Socket->BoneName, Socket->RelativeLocation, Socket->RelativeRotation, Socket->RelativeScale);
		}
		else
		{
			//use as a bone for attaching if this fails
			// @todo ib2merge: Chair added this, and removed a message. Is it bad to always do this? Seems like an okay change
			AttachComponent(Component, SocketName);
		}
	}
	else
	{
#if !PS3 // hopefully will be found on PC
		debugf(NAME_DevAnim,TEXT("AttachComponentToSocket : no SkeletalMesh could not attach socket '%s'  attaching: %s[%s]"), *SocketName.ToString(), *Component->GetName(), *Component->GetDetailedInfo() );
#endif
	}
}

//
//	USkeletalMeshComponent::DetachComponent
//

void USkeletalMeshComponent::DetachComponent(UActorComponent* Component)
{
	if (Component != NULL)
	{
		// Find the specified component in the Attachments array.
		for(INT AttachmentIndex = 0;AttachmentIndex < Attachments.Num();AttachmentIndex++)
		{
			if(Attachments(AttachmentIndex).Component == Component)
			{
				// Notify the texture streaming system
				const UPrimitiveComponent* Primitive = ConstCast<UPrimitiveComponent>(Component);
				if ( Primitive )
				{
					GStreamingManager->NotifyPrimitiveDetached( Primitive );
				}

				// This attachment is the specified component, detach it from the scene and remove it from the attachments array.
				Component->ConditionalDetach();
				Attachments.Remove(AttachmentIndex--);

				// Unset pointer to a skeletal mesh component
				USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Component);
				if(SkelComp)
				{
					SkelComp->AttachedToSkelComponent = NULL;
				}

				break;
			}
		}
	}
}

/**
* Detach any component that's attached if class of the component == ClassOfComponentToDetach or child
*/
void USkeletalMeshComponent::DetachAnyOf(UClass * ClassOfComponentToDetach)
{
	// only if actorcomponent
	if ( ClassOfComponentToDetach && ClassOfComponentToDetach->IsChildOf(UActorComponent::StaticClass()) )
	{
		for (INT ComponentIndex = 0; ComponentIndex < Attachments.Num(); ComponentIndex++)
		{
			UActorComponent* ComponentToDetach = Cast<UActorComponent>(Attachments(ComponentIndex).Component);
			if ( ComponentToDetach )
			{
				UClass * ComponentClass = Attachments(ComponentIndex).Component->GetClass();
				if ( ComponentClass->IsChildOf(ClassOfComponentToDetach) )
				{
					DetachComponent(ComponentToDetach);
					// reduce index, so that it can start from current location
					ComponentIndex--;
				}
			}
		}
	}
}
/** if bOverrideAttachmentOwnerVisibility is true, overrides the owner visibility values in the specified attachment with our own
 * @param Component the attached primitive whose settings to override
 */
void USkeletalMeshComponent::SetAttachmentOwnerVisibility(UActorComponent* Component)
{
	if (bOverrideAttachmentOwnerVisibility)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
		if (Primitive != NULL)
		{
			Primitive->SetOwnerNoSee(bOwnerNoSee);
			Primitive->SetOnlyOwnerSee(bOnlyOwnerSee);
		}
	}
}

//
//	USkeletalMeshComponent::SetParentToWorld
//

void USkeletalMeshComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	// If getting transform from ParentAnimComponent - ignore any other calls to modify the transform.
	if(bTransformFromAnimParent && ParentAnimComponent)
	{
		return;
	}
	else
	{
		Super::SetParentToWorld(ParentToWorld);
	}
}

void USkeletalMeshComponent::GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	if( SkeletalMesh )
	{
		const FLOAT LocalTexelFactor = SkeletalMesh->GetStreamingTextureFactor(0) * StreamingDistanceMultiplier;
		const FLOAT WorldTexelFactor = LocalTexelFactor * LocalToWorld.GetMaximumAxisScale();
		const INT NumMaterials = Max(SkeletalMesh->Materials.Num(), Materials.Num());
		for( INT MatIndex = 0; MatIndex < NumMaterials; MatIndex++ )
		{
			UMaterialInterface* const MaterialInterface = GetMaterial(MatIndex);
			if(MaterialInterface)
			{
				TArray<UTexture*> Textures;
				
				MaterialInterface->GetUsedTextures(Textures, MSQ_UNSPECIFIED, TRUE);
				for(INT TextureIndex = 0; TextureIndex < Textures.Num(); TextureIndex++ )
				{
					FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
					StreamingTexture.Bounds = Bounds.GetSphere();
					StreamingTexture.TexelFactor = WorldTexelFactor;
					StreamingTexture.Texture = Textures(TextureIndex);
				}
			}
		}
	}
}

/** Utility for calculating the current LocalToWorld matrix of this SkelMeshComp, given its parent transform. */
FMatrix USkeletalMeshComponent::CalcCurrentLocalToWorld(const FMatrix& ParentMatrix)
{
	FMatrix ResultMatrix = ParentMatrix;

	if(AbsoluteTranslation)
	{
		ResultMatrix.M[3][0] = ResultMatrix.M[3][1] = ResultMatrix.M[3][2] = 0.0f;
	}

	if(AbsoluteRotation || AbsoluteScale)
	{
		FVector	X(ResultMatrix.M[0][0],ResultMatrix.M[1][0],ResultMatrix.M[2][0]),
			Y(ResultMatrix.M[0][1],ResultMatrix.M[1][1],ResultMatrix.M[2][1]),
			Z(ResultMatrix.M[0][2],ResultMatrix.M[1][2],ResultMatrix.M[2][2]);

		if(AbsoluteScale)
		{
			X.Normalize();
			Y.Normalize();
			Z.Normalize();
		}

		if(AbsoluteRotation)
		{
			X = FVector(X.Size(),0,0);
			Y = FVector(0,Y.Size(),0);
			Z = FVector(0,0,Z.Size());
		}

		ResultMatrix.M[0][0] = X.X;
		ResultMatrix.M[1][0] = X.Y;
		ResultMatrix.M[2][0] = X.Z;
		ResultMatrix.M[0][1] = Y.X;
		ResultMatrix.M[1][1] = Y.Y;
		ResultMatrix.M[2][1] = Y.Z;
		ResultMatrix.M[0][2] = Z.X;
		ResultMatrix.M[1][2] = Z.Y;
		ResultMatrix.M[2][2] = Z.Z;
	}

	ResultMatrix = FScaleRotationTranslationMatrix( Scale * Scale3D, Rotation, Translation ) * ResultMatrix;

	// If desired, take into account the transform from the Origin/RotOrigin in the SkeletalMesh (if there is one).
	// We don't do this if bTransformFromAnimParent is true and we have a parent - in that case both ResultMatrixs should be the same, including skeletal offset.
	if( SkeletalMesh && !bForceRawOffset && !(ParentAnimComponent && bTransformFromAnimParent) )
	{
		ResultMatrix = FTranslationMatrix( SkeletalMesh->Origin ) * FRotationMatrix(SkeletalMesh->RotOrigin) * ResultMatrix;
	}

	return ResultMatrix;
}

void USkeletalMeshComponent::execGetPosition(FFrame& Stack,RESULT_DECL)
{
	P_FINISH;
	// LocalToWorld takes into account RotOrigin and Origin, so it's not technically the Component's LocalToWorld, but the SkelMesh's LocalToWorld
	// However here we really do want the component. So we have to take out that ComponentToSkelMesh transform
	FMatrix CompToSkelMeshTM = FTranslationMatrix( SkeletalMesh->Origin ) * FRotationMatrix( SkeletalMesh->RotOrigin );
	FMatrix CompToWorldTM = CompToSkelMeshTM.Inverse() * LocalToWorld;
	*(FVector*)Result = CompToWorldTM.GetOrigin();
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execGetPosition);

void USkeletalMeshComponent::execGetRotation(FFrame& Stack,RESULT_DECL)
{
	P_FINISH;
	// LocalToWorld takes into account RotOrigin and Origin, so it's not technically the Component's LocalToWorld, but the SkelMesh's LocalToWorld
	// However here we really do want the component. So we have to take out that ComponentToSkelMesh transform
	FMatrix CompToSkelMeshTM = FTranslationMatrix( SkeletalMesh->Origin ) * FRotationMatrix( SkeletalMesh->RotOrigin );
	FMatrix CompToWorldTM = CompToSkelMeshTM.Inverse() * LocalToWorld;
	*(FRotator*)Result = CompToWorldTM.Rotator();
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execGetRotation);

//
//	USkeletalMeshComponent::SetTransformedToWorld
//
void USkeletalMeshComponent::SetTransformedToWorld()
{
	LocalToWorld = CalcCurrentLocalToWorld(CachedParentToWorld);
	// cache localToWorldBoneAtom
	LocalToWorldBoneAtom.SetMatrixWithScale(LocalToWorld);
	LocalToWorldDeterminant = LocalToWorld.Determinant();
}

UBOOL USkeletalMeshComponent::IsValidComponent() const
{
	 return (SkeletalMesh != NULL) && Super::IsValidComponent(); 
}

/** 
 *	Utility for taking two arrays of bytes, which must be strictly increasing, and finding the intersection between them.
 *	That is - any item in the output should be present in both A and B. Output is strictly increasing as well.
 */
// An identical function was defined in UnAnimTree.cpp, so use that (can break Unity builds if it's defined twice)
extern void IntersectByteArrays(TArray<BYTE>& Output, const TArray<BYTE>& A, const TArray<BYTE>& B);

/** 
 * Separated RequiredBones Array in three Arrays, 
 * to "compose" skeleton, and apply controllers in three different passes.
 * Ex:
 * - PRE-PASS: IK Bones. (so they can be used by IK controllers without a frame of lag)
 * - MAIN-PASS: Most bones and controllers. Incl. IK Controllers.
 * - POST-PASS: Roll & Twist Bone controllers, that need current bone position, POST IK transformation.
 */
void USkeletalMeshComponent::BuildComposeSkeletonPasses()
{
	// Max number of bones for this LOD
	const INT NumRequiredBones = RequiredBones.Num();

	UAnimTree* Tree = Cast<UAnimTree>(Animations);

	// We at least need a SkeletalMesh and an AnimTree to progress further.
	// If the first node of the Animation Tree if not a UAnimTree, then skip.
	// This can happen in the AnimTree editor when previewing a node different than the root.
	if( !SkeletalMesh || !Animations || !Tree 
		// If we're not doing any pre pass or post pass, just copy and move on.
		|| (Tree->ComposePrePassBoneNames.Num() == 0 && Tree->ComposePostPassBoneNames.Num() == 0) )
	{
		// Just process all the bones in normal pass.
		ComposeOrderedRequiredBones = RequiredBones;
		return;
	}

	// reset our array.
	ComposeOrderedRequiredBones.Empty(NumRequiredBones);

	// Intermediate arrays.
	TArray<BYTE> ComposePass1RequiredBones, ComposePass2RequiredBones, ComposePass3RequiredBones;

	TArray<BYTE> TempByteArray;
	TempByteArray.Reserve(NumRequiredBones);
	const INT MaxNumBones = SkeletalMesh->RefSkeleton.Num();

	if( Tree->ComposePrePassBoneNames.Num() > 0 )
	{
		// Build Pre-Pass
		// Go through bone selection, and include their children.
		for(INT i=0; i<Tree->ComposePrePassBoneNames.Num(); i++)
		{
			const FName BoneName = Tree->ComposePrePassBoneNames(i);
			const INT BoneIndex = SkeletalMesh->MatchRefBone(BoneName);
			if( BoneIndex != INDEX_NONE )
			{
				// Add this bone
				TempByteArray.AddUniqueItem(BoneIndex);

				// Add child bones
				// We rely on the fact that they are in strictly increasing order
				// so we start at the bone after BoneIndex, up until we reach the end of the list
				// or we find another bone that has a parent before BoneIndex.
				for( INT Index=BoneIndex + 1; Index<MaxNumBones && SkeletalMesh->RefSkeleton(Index).ParentIndex >= BoneIndex; Index++)
				{
					TempByteArray.AddUniqueItem(Index);
				}
			}
		}

		// Sort to ensure strictly increasing order.
		Sort<USE_COMPARE_CONSTREF(BYTE, UnSkeletalComponent)>(&TempByteArray(0), TempByteArray.Num());
		// First pass also needs to include parent bones, as we build from local space, we need parent information for those bones.
		UAnimNode::EnsureParentsPresent(TempByteArray, SkeletalMesh);
		// Fill in first pass array, intersect with LOD RequiredBones array, to make sure we don't process bones that are not needed
		ComposePass1RequiredBones.Reserve(TempByteArray.Num());
		IntersectByteArrays(ComposePass1RequiredBones, TempByteArray, RequiredBones);
	}

	if( Tree->ComposePostPassBoneNames.Num() > 0 )
	{
		// Build Post-Pass
		// Go through bone selection, and include their children.
		TempByteArray.Reset();
		for(INT i=0; i<Tree->ComposePostPassBoneNames.Num(); i++)
		{
			const FName BoneName = Tree->ComposePostPassBoneNames(i);
			const INT BoneIndex = SkeletalMesh->MatchRefBone(BoneName);
			// Make sure this bone is not contained in first pass array
			if( BoneIndex != INDEX_NONE && !ComposePass1RequiredBones.ContainsItem(BoneIndex) )
			{
				// Add this bone
				TempByteArray.AddUniqueItem(BoneIndex);

				// Add child bones
				// We rely on the fact that they are in strictly increasing order
				// so we start at the bone after BoneIndex, up until we reach the end of the list
				// or we find another bone that has a parent before BoneIndex.
				for( INT Index=BoneIndex + 1; Index<MaxNumBones && SkeletalMesh->RefSkeleton(Index).ParentIndex >= BoneIndex; Index++)
				{
					TempByteArray.AddUniqueItem(Index);
				}
			}
#if !FINAL_RELEASE
			else if( BoneIndex == INDEX_NONE && BoneName != NAME_None && GIsEditor && !GIsGame )
			{
				// That might not always be a problem, we can get missing bones from LODs.
				// So we'll only report them in the editor, where we'll most likely work only with the highest LOD.
				warnf(TEXT("BuildComposeSkeletonPasses Bone %s not found for POST-PASS. Mesh: %s, Tree: %s"), *BoneName.ToString(), *SkeletalMesh->GetFName().ToString(), (AnimTreeTemplate)?*AnimTreeTemplate->GetFName().ToString():TEXT("None"));
			}
#endif
		}

		// Sort to ensure strictly increasing order.
		Sort<USE_COMPARE_CONSTREF(BYTE, UnSkeletalComponent)>(&TempByteArray(0), TempByteArray.Num());
		// Fill in last pass array, intersect with LOD RequiredBones array, to make sure we don't process bones that are not needed
		ComposePass3RequiredBones.Reserve(TempByteArray.Num());
		IntersectByteArrays(ComposePass3RequiredBones, TempByteArray, RequiredBones);
	}

	// Build Main Pass
	// Bones not included in either pre-pass or post-pass
	if( ComposePass1RequiredBones.Num() == 0 && ComposePass3RequiredBones.Num() == 0 )
	{
		// Not really any work to do here...
		ComposeOrderedRequiredBones = RequiredBones;
	}
	else
	{
		INT Pass1Index = 0;
		INT Pass3Index = 0;
		for(INT RequiredBoneIndex=0; RequiredBoneIndex<RequiredBones.Num(); RequiredBoneIndex++)
		{
			const INT BoneIndex = RequiredBones(RequiredBoneIndex);

			if( Pass1Index < ComposePass1RequiredBones.Num() && ComposePass1RequiredBones(Pass1Index) == BoneIndex )
			{
				Pass1Index++;
			}
			else if( Pass3Index < ComposePass3RequiredBones.Num() && ComposePass3RequiredBones(Pass3Index) == BoneIndex )
			{
				Pass3Index++;
			}
			else
			{
				ComposePass2RequiredBones.AddItem(BoneIndex);
			}
		}

		ComposeOrderedRequiredBones = ComposePass1RequiredBones;
		ComposeOrderedRequiredBones.Append(ComposePass2RequiredBones);
		ComposeOrderedRequiredBones.Append(ComposePass3RequiredBones);
	}

#if !FINAL_RELEASE
	// Make sure we correctly split RequiredBones into three arrays
	check( RequiredBones.Num() == ComposeOrderedRequiredBones.Num());
#endif

#if 0	// debug how passes are built.
	debugf(TEXT("USkeletalMeshComponent::BuildComposeSkeletonPasses"));
	for(INT Pass=0; Pass<3; Pass++)
	{
		TArray<BYTE> *TempByteArray = NULL;
		switch( Pass )
		{
			case 0 : TempByteArray = &ComposePass1RequiredBones;	debugf(NAME_DevAnim,TEXT("PRE-PASS"));	break;
			case 1 : TempByteArray = &ComposePass2RequiredBones;	debugf(NAME_DevAnim,TEXT("MAIN PASS"));	break;
			case 2 : TempByteArray = &ComposePass3RequiredBones;	debugf(NAME_DevAnim,TEXT("POST-PASS"));	break;
		}

		for(INT i=0; i<TempByteArray->Num(); i++)
		{
			const INT BoneIndex = (*TempByteArray)(i);
			FMeshBone& MeshBone = SkeletalMesh->RefSkeleton(BoneIndex);

			// Find out bone depth
			INT BoneDepth = 0;
			INT TmpBoneIndex = BoneIndex;
			while( TmpBoneIndex != 0 )
			{
				TmpBoneIndex = SkeletalMesh->RefSkeleton(TmpBoneIndex).ParentIndex;
				BoneDepth++;
			}

			FString LeadingSpace;
			for(INT j=0; j<BoneDepth; j++)
			{
				LeadingSpace += TEXT(" ");
			}

			if( BoneIndex == 0 )
			{
				debugf(NAME_DevAnim,TEXT("%3d: %s%s"), BoneIndex, *LeadingSpace, *MeshBone.Name.ToString());
			}
			else
			{
				debugf(NAME_DevAnim,TEXT("%3d: %s%s (ParentBoneID: %d)"), BoneIndex, *LeadingSpace, *MeshBone.Name.ToString(), MeshBone.ParentIndex );
			}
		}
	}
#endif

}

TArray<INT>				AffectedBones;
TArray<FBoneAtom>	NewBoneTransforms;
TArray<FLOAT>			NewBoneScales;

void USkeletalMeshComponent::ApplyControllersForBoneIndex(INT BoneIndex, UBOOL bPrePhysControls, UBOOL bPostPhysControls, const UAnimTree* Tree, UBOOL bRenderedRecently, const BYTE* BoneProcessed)
{
	checkSlow(Tree);

	TArray<BYTE>& ControlIndexMap = (!bPrePhysControls && bPostPhysControls) ? PostPhysSkelControlIndex : SkelControlIndex;

	// Temp relative transform
	FBoneAtom RelativeTransform;

	// If the ControlIndex array is not empty, and we have controllers for this bone, apply it now.
	if( (ControlIndexMap.Num() > 0) && (ControlIndexMap(BoneIndex) != 255) )
	{
		const INT ControlIndex = ControlIndexMap(BoneIndex);
		check( ControlIndex < Tree->SkelControlLists.Num() );

		// Iterate over linked list of controls, calculate desired transforms for each.
		USkelControlBase* Control = Tree->SkelControlLists(ControlIndex).ControlHead;
		while( Control )
		{
			FLOAT const ControlWeight = Control->bControlledByAnimMetada ? Control->ControlStrength * Control->GetControlMetadataWeight() :  Control->ControlStrength;
			UBOOL bCorrectPhysPhase = (Control->bPostPhysicsController == bPostPhysControls) || (!Control->bPostPhysicsController == bPrePhysControls);

			// If rendered recently (or we don't care), at a high enough LOD level, have some weight, and are correctly pre/post physics - process control!
			if( (bRenderedRecently || !Control->bIgnoreWhenNotRendered) && (PredictedLODLevel < Control->IgnoreAtOrAboveLOD) && (ControlWeight > ZERO_ANIMWEIGHT_THRESH) && bCorrectPhysPhase )
			{
				SCOPE_CYCLE_COUNTER(STAT_SkelControl);

				AffectedBones.Reset();
				Control->GetAffectedBones(BoneIndex, this, AffectedBones);

				// Do nothing if we are not going to affect any bones.
				if( AffectedBones.Num() > 0 )
				{
					NewBoneTransforms.Reset();
					Control->CalculateNewBoneTransforms(BoneIndex, this, NewBoneTransforms);
					UBOOL const bTransformingAffectedBones = (NewBoneTransforms.Num() > 0);

#if !FINAL_RELEASE && !SHIPPING_PC_GAME
					if (bTransformingAffectedBones)
					{
						for(INT AffectedIdx=0; AffectedIdx<AffectedBones.Num(); AffectedIdx++)
						{
							const INT AffectedBoneIndex	= AffectedBones(AffectedIdx);
							// Verify that bone controllers are not propagating NaNs.
							if( NewBoneTransforms.IsValidIndex(AffectedIdx) && NewBoneTransforms(AffectedIdx).ContainsNaN() )
							{
								FBoneAtom& TM = NewBoneTransforms(AffectedIdx);

								debugf(TEXT("(ComposeSkeleton) Bad NewBoneTransforms - BoneController: %s (%s), SkelMesh: %s, AffectedIdx: %d, BoneIndex: %d, BoneName: %s"), 
									*Control->GetClass()->GetFName().ToString(), *Control->ControlName.ToString(), *SkeletalMesh->GetName(), 
									AffectedIdx, AffectedBoneIndex, *SkeletalMesh->RefSkeleton(AffectedBoneIndex).Name.ToString());
								TM.DebugPrint();
#if !CONSOLE
								ensure(!TM.ContainsNaN());
#endif

								TM.SetIdentity();
							}
							else if(!NewBoneTransforms.IsValidIndex(AffectedIdx))
							{
								debugf(NAME_Warning,TEXT("Invalid NewBoneTransforms index: %d, BoneController: %s (%s), SkelMesh: %s, BoneIndex: %d, BoneName: %s"),AffectedIdx,*Control->GetClass()->GetFName().ToString(), *Control->ControlName.ToString(),*SkeletalMesh->GetName(),AffectedBoneIndex,*SkeletalMesh->RefSkeleton(AffectedBoneIndex).Name.ToString());
							}
						}
					}
#endif

					NewBoneScales.Reset();
					Control->CalculateNewBoneScales(BoneIndex, this, NewBoneScales);
					UBOOL const bScalingAffectedBones = (NewBoneScales.Num() > 0);

					// Get Alpha for this control. CalculateNewBoneTransforms() may have changed it.
					FLOAT const ControlAlpha = AlphaToBlendType(Control->GetControlAlpha(), Control->BlendType);

					// This allows the SkelControl to do nothing, by returning empty arrays.
					// ControlAlpha can also return 0 to skip applying the controller.
					if( ControlAlpha > ZERO_ANIMWEIGHT_THRESH && (bTransformingAffectedBones || bScalingAffectedBones) )
					{
						// handle skelcontrol pos/rot modifications to bones
						if (bTransformingAffectedBones)
						{
							check( AffectedBones.Num() == NewBoneTransforms.Num() );

							FBoneAtom Parent;
							// Now handle blending control output into skeleton.
							// We basically have to turn each transform back into a local-space FBoneAtom, interpolate between the current FBoneAtom
							// for this bone, then do the regular 'relative to parent' blend maths.

							for(INT AffectedIdx=0; AffectedIdx<AffectedBones.Num(); AffectedIdx++)
							{
								const INT AffectedBoneIndex	= AffectedBones(AffectedIdx);
								const INT ParentIndex		= SkeletalMesh->RefSkeleton(AffectedBoneIndex).ParentIndex;

								// Calculate transform of parent bone
								if( AffectedBoneIndex > 0 )
								{
									// If the parent of this bone is another one affected by this controller,
									// we want to use the new parent transform from the controller as the basis for the relative-space animation atom.
									// If not, use the current SpaceBase matrix for the parent.
									const INT NewBoneTransformIndex = AffectedBones.FindItemIndex(ParentIndex);
									if( NewBoneTransformIndex == INDEX_NONE )
									{
										Parent = SpaceBases(ParentIndex);
									}
									else
									{
										Parent = NewBoneTransforms(NewBoneTransformIndex);
									}
								}
								else
								{
									Parent = FBoneAtom::Identity;
								}


								// Then work out relative transform, and convert to FBoneAtom.
								RelativeTransform = NewBoneTransforms(AffectedIdx) * Parent.InverseSafe();
								// The above code basically inverts any scale parent has
								// For example if parent had scale of 0.5, this will add this bone to have 2, so that the result stays 1. 
								// so copy back original LocalAtoms value to preserve previous scale
								// this section of code is only to get translation, so reserving previous LocalAtoms scale should be fine
								RelativeTransform.SetScale(LocalAtoms(AffectedBoneIndex).GetScale());
								checkSlow(RelativeTransform.IsRotationNormalized());
								checkSlow(!RelativeTransform.ContainsNaN());

								// faster version when we don't need to blend. Copy results directly
								if( ControlAlpha >= (1.f - ZERO_ANIMWEIGHT_THRESH) )
								{
									// We can't just assign NewBoneTransforms to SpaceBases, because we want to inherit scaling from parents.
									if( AffectedBoneIndex > 0 )
									{
										SpaceBases(AffectedBoneIndex) = RelativeTransform * SpaceBases(ParentIndex);
										checkSlow(SpaceBases(AffectedBoneIndex).IsRotationNormalized());
									}
									else
									{
										SpaceBases(AffectedBoneIndex) = RelativeTransform;
									}

									LocalAtoms(AffectedBoneIndex) = RelativeTransform;
								}
								else
								{
									// Set the new FBoneAtom to be a blend between the current one, and the one from the controller.
									LocalAtoms(AffectedBoneIndex).Blend(LocalAtoms(AffectedBoneIndex), RelativeTransform, ControlAlpha);
#ifdef _DEBUG
									// Check that all bone atoms coming from animation are normalized
									check( LocalAtoms(AffectedBoneIndex).IsRotationNormalized() );
									check( !LocalAtoms(AffectedBoneIndex).ContainsNaN() );
#endif

									// Then do usual hierarchical blending stuff (just like in ComposeSkeleton).
									if( AffectedBoneIndex > 0 )
									{
										SpaceBases(AffectedBoneIndex) = LocalAtoms(AffectedBoneIndex) * SpaceBases(ParentIndex);
									}
									else
									{
										SpaceBases(0) = LocalAtoms(0);
									}
								}
#ifdef _DEBUG
								// Check that all bone atoms coming from animation are normalized
								checkSlow( LocalAtoms(AffectedBoneIndex).IsRotationNormalized() );
								checkSlow( SpaceBases(AffectedBoneIndex).IsRotationNormalized() );

								checkSlow( !LocalAtoms(AffectedBoneIndex).ContainsNaN() );
								checkSlow( !SpaceBases(AffectedBoneIndex).ContainsNaN() );
#endif
							}
						}

						// handle scaling separately for now.  loops through affected bones again, but
						// should be a rarely used code path
						if (bScalingAffectedBones)
						{
							check( AffectedBones.Num() == NewBoneScales.Num() );

							for(INT AffectedIdx=0; AffectedIdx<AffectedBones.Num(); AffectedIdx++)
							{
								const INT AffectedBoneIndex	= AffectedBones(AffectedIdx);
								const INT ParentIndex		= SkeletalMesh->RefSkeleton(AffectedBoneIndex).ParentIndex;

								FLOAT ParentScale = 1.f;
								{
									if( AffectedBoneIndex > 0 )
									{
										// If the parent of this bone is another one affected by this controller,
										// we want to use the new parent transform from the controller as the basis for the relative-space animation atom.
										// If not, use the current SpaceBase matrix for the parent.
										const INT ParentBoneIndex = AffectedBones.FindItemIndex(ParentIndex);
										if( ParentBoneIndex != INDEX_NONE )
										{
											ParentScale = NewBoneScales(ParentBoneIndex);
										}
									}
								}

								FLOAT FinalScale, FinalRelScale;
								if( ControlAlpha >= (1.f - ZERO_ANIMWEIGHT_THRESH) )
								{
									// faster version when we don't need to blend. Copy results directly
									FinalScale = NewBoneScales(AffectedIdx);
									FinalRelScale = (ParentScale == 0.f) ? 1.f : FinalScale / ParentScale;
								}
								else
								{
									// else, need to blend.  Just doing lerp here for now, instead of pushing through 
									// the FBoneAtom.Blend() call.
									FLOAT const RelScale = (ParentScale == 0.f) ? 1.f : NewBoneScales(AffectedIdx) / ParentScale;
									FinalRelScale = Lerp(LocalAtoms(AffectedBoneIndex).GetScale(), RelScale, ControlAlpha);
									FinalScale = FinalRelScale * ParentScale;
								}

								// apply the scaling
								LocalAtoms(AffectedBoneIndex).MultiplyScale(FinalRelScale);
								SpaceBases(AffectedBoneIndex).MultiplyScale(FinalRelScale);
							}
						}

						FLOAT const BoneScale = Lerp(1.f, Control->GetBoneScale(BoneIndex, this), ControlAlpha);

						// Apply bone scaling.
						if( BoneScale != 1.f )
						{
							LocalAtoms(BoneIndex).MultiplyScale(BoneScale);
							SpaceBases(BoneIndex).MultiplyScale(BoneScale);
						}

						// Find the earliest bone in bones array affected by this controller. 
						// Because parents are always before children in AffectedBones, this will always be the first element.
						if ((BoneProcessed != NULL) && (AffectedBones.Num() > 1))
						{
							// For any bones between that bone and the one we updated, that was not affected by the bone controller, we need to refresh it.
							for(INT UpdateBoneIndex = AffectedBones(0) + 1; UpdateBoneIndex < BoneIndex; UpdateBoneIndex++)
							{
								if( BoneProcessed[UpdateBoneIndex] && !AffectedBones.ContainsItem(UpdateBoneIndex) )
								{
									SpaceBases(UpdateBoneIndex) = LocalAtoms(UpdateBoneIndex);
									const INT UpdateParentIndex = SkeletalMesh->RefSkeleton(UpdateBoneIndex).ParentIndex;
									SpaceBases(UpdateBoneIndex) *= SpaceBases(UpdateParentIndex);
									checkSlow( SpaceBases(UpdateBoneIndex).IsRotationNormalized() );
									checkSlow( !SpaceBases(UpdateBoneIndex).ContainsNaN() );
								}
							}
						}
					}
				} // if ((BoneProcessed != NULL) && (AffectedBones.Num() > 1))
			} // if( Control->ControlStrength > KINDA_SMALL_NUMBER )

			Control = Control->NextControl;
		}
	}
}


/**
 * Take the LocalAtoms array (translation vector, rotation quaternion and scale vector) and update the array of component-space bone transformation matrices (SpaceBases).
 * It will work down hierarchy multiplying the component-space transform of the parent by the relative transform of the child.
 * This code also applies any per-bone rotators etc. as part of the composition process
 */
void USkeletalMeshComponent::ComposeSkeleton()
{
	SCOPE_CYCLE_COUNTER(STAT_SkelComposeTime);

	if( !SkeletalMesh )
	{
		return;
	}

	PREFETCH(SkeletalMesh->RefSkeleton.GetTypedData());

	// See if we need to refresh required bones array for multi-pass compose.
	if( bUpdateComposeSkeletonPasses )
	{
		BuildComposeSkeletonPasses();
		bUpdateComposeSkeletonPasses = FALSE;
	}

	check( SkeletalMesh->RefSkeleton.Num() == LocalAtoms.Num() );
	check( SkeletalMesh->RefSkeleton.Num() == SpaceBases.Num() );
	check( SkeletalMesh->RefSkeleton.Num() == BoneVisibilityStates.Num() );

	const UAnimTree* Tree = Cast<UAnimTree>(Animations);

	const INT NumBones = LocalAtoms.Num();

	/** Keep track of which bones have been processed for fast look up */
	BYTE BoneProcessed[MAX_BONES];
	appMemzero(BoneProcessed, NumBones * sizeof(BYTE));

	// Cache this once
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

	// If bIgnoreControllersWhenNotRendered is true, set bForceIgnore if the Owner has not been drawn recently.
	const UBOOL bRenderedRecently	= (GIsGame && bRecentlyRendered) || GIsEditor;
	const UBOOL bForceIgnore		= GIsGame && bIgnoreControllersWhenNotRendered && !bRenderedRecently;

// 	debugf(TEXT("(ComposeSkeleton) [%3.3f] SkelMesh: %s"), GWorld->GetWorldInfo()->TimeSeconds, *SkeletalMesh->GetName());

	// Special case here - if we have no physics, or are in editor (not PIE), update pre _and_ post phys controls now, because BlendInPhysics won't be run
	UBOOL bDoPostPhysControls = ( !DoesBlendPhysics() ) || (GIsEditor && !GIsGame);

	UBOOL bApplyControllers = Tree && !bIgnoreControllers && !bForceIgnore;

	// Build in 3 passes.
	FBoneAtom* LocalTransformsData = LocalAtoms.GetTypedData(); 
	FBoneAtom* SpaceBasesData = SpaceBases.GetTypedData();
	for(INT i=0; i<ComposeOrderedRequiredBones.Num(); i++)
	{
		const INT BoneIndex = ComposeOrderedRequiredBones(i);
		PREFETCH(SpaceBasesData + BoneIndex);

		// Mark bone as processed
		BoneProcessed[BoneIndex] = 1;

		if( BoneIndex != 0 )
		{
			// For all bones below the root, final component-space transform is relative transform * component-space transform of parent.
			const INT ParentIndex = SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;
			PREFETCH(SpaceBasesData + ParentIndex);

			// Check the precondition that Parents occur before Children in the RequiredBones array.
			checkSlow(BoneProcessed[ParentIndex] == 1);

			FBoneAtom::Multiply(SpaceBasesData + BoneIndex, LocalTransformsData + BoneIndex, SpaceBasesData + ParentIndex);
		}
		else
		{
			// For root bone, just read bone atom as component-space transform.
			SpaceBases(0) = LocalAtoms(0);
		}

		// If we have an AnimTree, and we are not ignoring controllers, apply any SkelControls in the tree now.
		if( bApplyControllers )
		{
			ApplyControllersForBoneIndex(BoneIndex, TRUE, bDoPostPhysControls, Tree, bRenderedRecently, BoneProcessed);
		}

		checkSlow( SpaceBases(BoneIndex).IsRotationNormalized() );
		checkSlow( !SpaceBases(BoneIndex).ContainsNaN() );
	}
}

/** 
 *	Utility for iterating over all SkelControls in the Animations AnimTree of this SkeletalMeshComponent.
 *	Assumes that bTickStatus has already been toggled outside of this function.
 */
void USkeletalMeshComponent::TickSkelControls(FLOAT DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_SkelControlTickTime);

	UAnimTree* RESTRICT AnimTree = Cast<UAnimTree>(Animations);
	if( AnimTree )
	{
		INT const NumSkelControls = SkelControlTickArray.Num();
#if CONSOLE
		// prefetch the initial iteration
		if( NumSkelControls > 0 )
		{
			BYTE* RESTRICT Address = (BYTE* RESTRICT)SkelControlTickArray(0);
			CONSOLE_PREFETCH(Address);
			CONSOLE_PREFETCH_NEXT_CACHE_LINE(Address);
		}
#endif

		for(INT i=0; i<NumSkelControls; i++)
		{
			USkelControlBase* Control = SkelControlTickArray(i);

#if CONSOLE
			// prefetch the next iteration
			if( i+1 < NumSkelControls )
			{
				BYTE* RESTRICT Address = (BYTE* RESTRICT)SkelControlTickArray(i+1);
				CONSOLE_PREFETCH(Address);
				CONSOLE_PREFETCH_NEXT_CACHE_LINE(Address);
			}
#endif

			Control->TickSkelControl(DeltaSeconds, this);
		}
	}
}

/** Uses the current AnimTree to update the ActiveMorphs array of morph targets to apply to the SkeletalMesh. */
void USkeletalMeshComponent::UpdateActiveMorphs()
{
	ActiveMorphs.Empty();

	// update tree first
	UAnimTree* AnimTree = Cast<UAnimTree>(Animations);
	if(AnimTree)
	{
		AnimTree->GetTreeActiveMorphs( ActiveMorphs );
	}

	// if refpose, do not include animation morph data
	if ( !bForceRefpose )
	{
		// add activecurve morphs to active morphs
		// do not overwrite anything that exists in tree morphs
		if ( ActiveCurveMorphs.Num() > 0 )
		{
			// update material here 
			// temporarily doing this until we consolidate the tree morphs
			// for now, I wouldn't like to over
			// some of tree morphs
			UBOOL bSameFound;
			for ( INT I=0; I<ActiveCurveMorphs.Num(); ++I )
			{
				bSameFound = FALSE;
				// I don't want to overwrite target info if tree already uses this
				// So check if Target is same or not, if same target, then do not add to the list
				for ( INT J=0;J<ActiveMorphs.Num(); ++J )
				{
					if ( ActiveCurveMorphs(I).Target == ActiveMorphs(J).Target )
					{
						debugf( NAME_DevAnim, TEXT("ERROR: Same Morph Target (%s) is used in the morph node. Ignoring animation morph data."), *ActiveCurveMorphs(I).Target->GetFName().GetNameString());
						bSameFound = TRUE;
						break;
					}
				}

				// if not used by tree, then update material and add to ActiveMorphs
				if ( bSameFound == FALSE )
				{
					if ( ActiveCurveMorphs(I).Target )
					{
						UpdateMorphTargetMaterial(ActiveCurveMorphs(I).Target, ActiveCurveMorphs(I).Weight );
						ActiveMorphs.AddItem(ActiveCurveMorphs(I));
					}
				}
			}
		}
	}
}

/** Performs per-frame FaceFX processing. */
void USkeletalMeshComponent::UpdateFaceFX( TArray<FBoneAtom>& LocalTransforms, UBOOL bTickFaceFX )
{
#if WITH_FACEFX
	// Note: SkeletalMesh and SkeletalMesh->FaceFXAsset cannot be NULL when 
	// this is called.
	if( FaceFXActorInstance )
	{
		FxActor* FaceFXActor = FaceFXActorInstance->GetActor();
		if( FaceFXActor )
		{
			// First, tick the current animation.
			// The only time we should be setting bTickFaceFX to FALSE is when we are ForceTicking somewhere else to a specific location.
			// In that case, IsPlaying will not be true, but we do want to evaluate FaceFX to update the face - hence the !bTickFaceFX check here.
			if( FaceFXActorInstance->IsPlayingAnim() || 
				FaceFXActorInstance->GetAllowNonAnimTick() || 
				FaceFXActorInstance->IsOpenInStudio() || 
				!bTickFaceFX ) 
			{

				// Do not do this if we are ForceTicking outside of this function.
				if(bTickFaceFX)
				{
					FxReal AudioOffset = -1.0f;
					if( CachedFaceFXAudioComp )
					{
						//@todo Is there a better way to determine if an audio component
						// is currently playing?
						if( CachedFaceFXAudioComp->SoundCue &&
							CachedFaceFXAudioComp->PlaybackTime != 0.0f &&
							!CachedFaceFXAudioComp->bFinished &&
							!GIsBenchmarking )
						{
							AudioOffset = CachedFaceFXAudioComp->PlaybackTime;

							// note, this assumes a constant pitch for the duration of the cue
							// to support variable pitch, AudioComponent could track and store a current time
							// within the audio.
							AudioOffset *= CachedFaceFXAudioComp->CurrentPitchMultiplier;
						}
					}

					FxAnimPlaybackState PlaybackState = APS_Stopped;
					{
						SCOPE_CYCLE_COUNTER(STAT_FaceFX_TickTime);
						FxDReal AppTime = 0.0;
						if( Owner && Owner->WorldInfo )
						{
							AppTime = Owner->WorldInfo->AudioTimeSeconds;
						}
						else
						{
							AppTime = appSeconds();
						}

						// In FaceFX 1.7+ the compiled face graph has already been ticked from the FaceFX 
						// Studio code.  Ticking it again here causes the face graph results to be cleared
						// before the skeleton has been updated, so if the FaceFX actor instance is open 
						// inside of FaceFX Studio the actor instance isn't ticked again here.
						if( !FaceFXActorInstance->IsOpenInStudio() )
						{
							PlaybackState = FaceFXActorInstance->Tick(AppTime, AudioOffset);
						}
					}

					if( APS_StartAudio == PlaybackState )
					{
						if( CachedFaceFXAudioComp )
						{
							if( CachedFaceFXAudioComp->SoundCue )
							{
								CachedFaceFXAudioComp->SubtitlePriority = SUBTITLE_PRIORITY_FACEFX;
								CachedFaceFXAudioComp->Play();
							}
						}
					}

					//@todo What about non-anim tick events?
					if( APS_Stopped == PlaybackState )
					{
						// Don't stop the anim if the audio has not finished as this will cause an audio artifact
						if( CachedFaceFXAudioComp == NULL || CachedFaceFXAudioComp->bFinished )
						{
							StopFaceFXAnim();
						}
					}
				}

				// Next update through the face graph and relink anything requesting a relink.
				FxBool bShouldClientRelink = FaceFXActor->ShouldClientRelink();
				if( bShouldClientRelink )
				{
					// Link the bones.
					FxMasterBoneList& MasterBoneList = FaceFXActor->GetMasterBoneList();
					for( FxSize i = 0; i < MasterBoneList.GetNumRefBones(); ++i )
					{
						FxBool bFoundBone = FxFalse;
						FName BoneName = FName(*FString(MasterBoneList.GetRefBone(i).GetNameAsCstr()), FNAME_Find, TRUE); 
						if ( BoneName!=NAME_None )
						{
							for( INT j = 0; j < SkeletalMesh->RefSkeleton.Num(); ++j )
							{
								if( BoneName == SkeletalMesh->RefSkeleton(j).Name )
								{
									MasterBoneList.SetRefBoneClientIndex(i, j);
									bFoundBone = FxTrue;
								}
							}
						}

						if( !bFoundBone )
						{
							debugf(TEXT("FaceFX: WARNING Reference bone %s could not be found in the skeleton '%s'."), 
								ANSI_TO_TCHAR(MasterBoneList.GetRefBone(i).GetNameAsCstr())
								,*GetDetailedInfo()
								);
							// Make sure any bones not linked up to the skeleton do not try to update
							// the skeleton.
							MasterBoneList.SetRefBoneClientIndex(i, FX_INT32_MAX);
						}
					}
					FaceFXActor->SetShouldClientRelink(FxFalse);
				}

				{
					SCOPE_CYCLE_COUNTER(STAT_FaceFX_BeginFrameTime);
					FaceFXActorInstance->BeginFrame();
				}

				FxCompiledFaceGraph& cg = FaceFXActor->GetCompiledFaceGraph();
				FxSize numNodes = cg.nodes.Length();
				for( FxSize node = 0; node < numNodes; ++node )
				{
					switch( cg.nodes[node].nodeType )
					{
					case NT_MorphTargetUE3:
						{
							SCOPE_CYCLE_COUNTER(STAT_FaceFX_MorphPassTime);

							// Update any morph nodes that may be in the Face Graph.
							FFaceFXMorphTargetProxy* Proxy = reinterpret_cast<FFaceFXMorphTargetProxy*>(cg.nodes[node].pUserData);
							// If the proxy is invalid create one.
							FxBool bShouldLink = FxFalse;
							if( !Proxy )
							{
								FFaceFXMorphTargetProxy* NewMorphTargetProxy = new FFaceFXMorphTargetProxy();
								cg.nodes[node].pUserData = NewMorphTargetProxy;
								bShouldLink = FxTrue;
								Proxy = NewMorphTargetProxy;
							}

							if( Proxy )
							{
								// If the proxy requests to be re-linked, link it up.
								if( bShouldLink || bShouldClientRelink )
								{
									const FxFaceGraphNodeUserProperty& MorphTargetNameProperty = 
										cg.nodes[node].userProperties[FX_MORPH_NODE_TARGET_NAME_INDEX];
									Proxy->Link(FName(ANSI_TO_TCHAR(MorphTargetNameProperty.GetStringProperty().GetData())));
								}
								Proxy->SetSkeletalMeshComponent(this);
								Proxy->Update(cg.nodes[node].finalValue);
							}
						}
						break;
					case NT_MaterialParameterUE3:
						{
							SCOPE_CYCLE_COUNTER(STAT_FaceFX_MaterialPassTime);

							// Update any material parameter nodes that may be in the Face Graph.
							FFaceFXMaterialParameterProxy* Proxy = reinterpret_cast<FFaceFXMaterialParameterProxy*>(cg.nodes[node].pUserData);
							// If the proxy is invalid create one.
							FxBool bShouldLink = FxFalse;
							if( !Proxy )
							{
								FFaceFXMaterialParameterProxy* NewMaterialParameterProxy = new FFaceFXMaterialParameterProxy();
								cg.nodes[node].pUserData = NewMaterialParameterProxy;
								bShouldLink = FxTrue;
								Proxy = NewMaterialParameterProxy;
							}

							if( Proxy )
							{
								// If the proxy requests to be re-linked, link it up.
								if( bShouldLink || bShouldClientRelink )
								{
									const FxFaceGraphNodeUserProperty& MaterialSlotProperty = 
										cg.nodes[node].userProperties[FX_MATERIAL_PARAMETER_NODE_MATERIAL_SLOT_ID_INDEX];
									const FxFaceGraphNodeUserProperty& ParameterNameProperty =
										cg.nodes[node].userProperties[FX_MATERIAL_PARAMETER_NODE_PARAMETER_NAME_INDEX];
									Proxy->Link(MaterialSlotProperty.GetIntegerProperty(), FName(ANSI_TO_TCHAR(ParameterNameProperty.GetStringProperty().GetData())));
								}
								Proxy->SetSkeletalMeshComponent(this);
								Proxy->Update(cg.nodes[node].finalValue);
							}
						}
						break;
					default:
						break;
					}
				}

				{
					SCOPE_CYCLE_COUNTER(STAT_FaceFX_BoneBlendingPassTime);
					FxMasterBoneList& MasterBoneList = FaceFXActor->GetMasterBoneList();
					const FxSize NumBones = FaceFXActorInstance->GetNumBones();
					
					if( FaceFXBlendMode == FXBM_Additive )
					{
						// Additive blending of Face FX transforms in parent bone space

						for( FxSize i = 0; i < NumBones; ++i )
						{
							const FxInt32 ClientIndex = MasterBoneList.GetRefBoneClientIndex(i);
							if( FX_INT32_MAX != ClientIndex )
							{
								if( ClientIndex >= 0 && ClientIndex < LocalTransforms.Num())
								{
									// Face FX local bone transform
									FxVec3 pos, scale;
									FxQuat rot;
									FxReal weight;
									FaceFXActorInstance->GetBone(i, pos, rot, scale, weight);
									const FVector		Position(pos.x, -pos.y, pos.z);
									const FQuat			Rotation(rot.x, -rot.y, rot.z, rot.w);

									FBoneAtom			FaceFXAtom	= FBoneAtom(Rotation, Position);
									//FaceFXAtom.NormalizeRotation();

									// Face FX reference pose bone transform
									const FxBone&		FFXRefBoneTransform = MasterBoneList.GetRefBone(i);
									const FxVec3&		FFXRefBonePos		= FFXRefBoneTransform.GetPos();
									const FxQuat&		FFXRefBoneRot		= FFXRefBoneTransform.GetRot();

									const FVector		RefPosition(FFXRefBonePos.x, -FFXRefBonePos.y, FFXRefBonePos.z);
									const FQuat			RefRotation(FFXRefBoneRot.x, -FFXRefBoneRot.y, FFXRefBoneRot.z, FFXRefBoneRot.w);

									FBoneAtom	RefSkelAtom	= FBoneAtom(RefRotation, RefPosition);
									//RefSkelAtom.NormalizeRotation();

									//RefSkelAtom.NormalizeRotation();

									LocalTransforms(ClientIndex).AccumulateWithAdditiveScale(FaceFXAtom - RefSkelAtom);
									
									// make sure rotation is normalized
									//LocalTransforms(ClientIndex).NormalizeRotation();
								}
								else
								{
#if CONSOLE
									warnf( TEXT("Bad FaceFX Animation Data for Skeleton '%s', ClientIndex: %d, NumLocalTransforms: %d"), *GetDetailedInfo(), ClientIndex, LocalTransforms.Num() );									
#else
									ensureMsgf(ClientIndex >= 0 && ClientIndex < LocalTransforms.Num(), TEXT("Bad FaceFX Animation Data for Skeleton '%s', ClientIndex: %d, NumLocalTransforms: %d"), *GetDetailedInfo(), ClientIndex, LocalTransforms.Num());
#endif
								}
							}
						}
					}
					else
					{
						// Original path for Face FX, overwrite skeleton local transforms.
						for( FxSize i = 0; i < NumBones; ++i )
						{
							const FxInt32 ClientIndex = MasterBoneList.GetRefBoneClientIndex(i);
							if( FX_INT32_MAX != ClientIndex )
							{
								if( ClientIndex >= 0 && ClientIndex < LocalTransforms.Num() )
								{
									FxVec3 pos, scale;
									FxQuat rot;
									FxReal weight;
									FaceFXActorInstance->GetBone(i, pos, rot, scale, weight);
									const FVector		Position(pos.x, -pos.y, pos.z);
									const FQuat			Rotation(rot.x, -rot.y, rot.z, rot.w);
									LocalTransforms(ClientIndex).SetComponents(Rotation, Position);
								}
								else
								{
#if CONSOLE
									warnf( TEXT("Bad FaceFX Animation Data for Skeleton '%s', ClientIndex: %d, NumLocalTransforms: %d"), *GetDetailedInfo(), ClientIndex, LocalTransforms.Num() );									
#else
									ensureMsgf(ClientIndex >= 0 && ClientIndex < LocalTransforms.Num(), TEXT("Bad FaceFX Animation Data for Skeleton '%s', ClientIndex: %d, NumLocalTransforms: %d"), *GetDetailedInfo(), ClientIndex, LocalTransforms.Num());
#endif
								}
							}
						}
					}
				}

				{
					SCOPE_CYCLE_COUNTER(STAT_FaceFX_EndFrameTime);
					FaceFXActorInstance->EndFrame();
				}
			}
		}
	}
#endif
}

UBOOL USkeletalMeshComponent::PlayFaceFXAnim(UFaceFXAnimSet* FaceFXAnimSetRef, const FString& AnimName, const FString& GroupName,class USoundCue* SoundCueToPlay)
{
#if WITH_FACEFX
	//debugf(TEXT("PlayFaceFXAnim on: %s, AnimSet: %s, GroupName: %s, AnimName: %s SoundCueToPlay: %s"), *Owner->GetName(), *FaceFXAnimSetRef->GetFullName(), *GroupName, *AnimName, *SoundCueToPlay->GetFullName() );
	SCOPE_CYCLE_COUNTER(STAT_FaceFX_PlayAnim);

#if !FINAL_RELEASE
	extern UBOOL GShouldTraceFaceFX;
	if (GShouldTraceFaceFX)
		TraceFaceFX();
#endif

	if( !bDisableFaceFX && FaceFXActorInstance )
	{
		// See if AnimSet needs to be mounted
		if( FaceFXAnimSetRef )
		{
			if( SkeletalMesh && SkeletalMesh->FaceFXAsset )
			{
				// Ensure AnimSet is mounted - this should be fine to call if it is already mounted (will do nothing).
				SkeletalMesh->FaceFXAsset->MountFaceFXAnimSet(FaceFXAnimSetRef);
			}
		}

// 		warnf( TEXT( "NUM MountedAnimSet: %d" ) ,SkeletalMesh->FaceFXAsset->MountedFaceFXAnimSets.Num() );
// 
// 		// log out the set of mounted AnimSets!
// 		for( INT i = 0; i < SkeletalMesh->FaceFXAsset->MountedFaceFXAnimSets.Num(); ++i )
// 		{
// 			//const FxAnimGroup& TempFFXAnimGroup = SkeletalMesh->FaceFXAsset->MountedFaceFXAnimSets(i)->GetFullName();
// 			warnf( TEXT( "MountedAnimSet: %s %s %s" )
// 				,*SkeletalMesh->FaceFXAsset->GetFullName()
// 				, *SkeletalMesh->FaceFXAsset->MountedFaceFXAnimSets(i)->GetFullName()
// 				//,ANSI_TO_TCHAR(TempFFXAnimGroup.GetNameAsCstr()), 
// 				);
// 		}dddddsfsfsdf


		// Start the FaceFX animation playing
		UBOOL bPlaySuccess = FaceFXActorInstance->PlayAnim(TCHAR_TO_ANSI(*AnimName), TCHAR_TO_ANSI(*GroupName));

		// If that worked, we want to start some audio
		if( bPlaySuccess )
		{
			// If we have an animation playing, and an Owner Actor
			const FxAnim* Anim = FaceFXActorInstance->GetCurrentAnim();
			if( Anim && Owner )
			{
				// Get the audio component even if we do not have a sound to play, so we can clear any existing sound on it.
				// Use the virtual function in Actor to do this.

				// Hackette here - in Matinee we want to use the C++ function, because we can't run events in the editor :(
				if(GIsEditor && !GWorld->HasBegunPlay())
				{
					CachedFaceFXAudioComp = Owner->PreviewGetFaceFXAudioComponent();
				}
				// In real game, use script event.
				else
				{
					CachedFaceFXAudioComp = Owner->eventGetFaceFXAudioComponent();
				}


				// we here can use the passed in soundcue to play the sound if they passed one in
				USoundCue* Sound = NULL;

				if( SoundCueToPlay != NULL )
				{
					Sound = SoundCueToPlay;
				}
				else
				{
					Sound = reinterpret_cast<USoundCue*>(Anim->GetSoundCuePointer());
				}

				
				// If we got the sound, get an AudioComponent to play it on.
				if( CachedFaceFXAudioComp )
				{
					CachedFaceFXAudioComp->Stop();
					CachedFaceFXAudioComp->SoundCue = Sound;
				}
			}

			return TRUE;
		}
		else
		{
#if !FINAL_RELEASE && !NO_LOGGING
			FString AnimSetName = FaceFXAnimSetRef ? FaceFXAnimSetRef->GetFullName() : TEXT("None");
			warnf(TEXT("Failed to play FaceFX Animation on: %s (%s), AnimSet: %s, GroupName: %s, AnimName: %s, SoundCue: %s (%s%s%s)"), 
				*Owner->GetName(), 
				*SkeletalMesh->GetPathName(),
				*AnimSetName, *GroupName, *AnimName, 
				SoundCueToPlay ? *(SoundCueToPlay->GetPathName()) : TEXT("NULL"),
				SoundCueToPlay ? *(SoundCueToPlay->FaceFXGroupName) : TEXT(""),
				SoundCueToPlay ? TEXT(".") : TEXT(""),
				SoundCueToPlay ? *(SoundCueToPlay->FaceFXAnimName) : TEXT("")
				);
#if 0 //Make this '1' to have a failed case list all animations on the FxActor
			{
				FxActor* FFXPActor = FaceFXActorInstance->GetActor();
				if (FFXPActor)
				{
					for (FxSize AGIndex = 0; AGIndex < FFXPActor->GetNumAnimGroups(); AGIndex++)
					{
						const FxAnimGroup& AnimGroup = FFXPActor->GetAnimGroup(AGIndex);
						debugf(TEXT("\tAnimGroup %d - %s"), (INT)AGIndex, ANSI_TO_TCHAR(AnimGroup.GetNameAsCstr()));

						FxSize AnimCount = AnimGroup.GetNumAnims();
						for (FxSize AnimIndex = 0; AnimIndex < AnimCount; AnimIndex++)
						{
							const FxAnim& Anim = AnimGroup.GetAnim(AnimIndex);
							debugf(TEXT("\t\tAnim %4d - %s"), (INT)AnimIndex, ANSI_TO_TCHAR(Anim.GetNameAsCstr()));
						}
					}
				}
			}
#endif
#endif
			StopFaceFXAnim();
		}
	}
#endif // WITH_FACEFX

	return FALSE;
}

void USkeletalMeshComponent::StopFaceFXAnim( void )
{
#if WITH_FACEFX
	if( FaceFXActorInstance )
	{
		//debugf(TEXT("StopFaceFXAnim on: %s AnimName: %s"), *Owner->GetName(), FaceFXActorInstance->GetAnimPlayer().GetCurrentAnimName().GetAsCstr());
		// If there is an AudioComponent for this animation, stop it
		if( CachedFaceFXAudioComp )
		{
			CachedFaceFXAudioComp->Stop();
			CachedFaceFXAudioComp->SoundCue = NULL;

			// ..and release ref to it.
			CachedFaceFXAudioComp = NULL;
		}

		// Stop the animation playback.
		FaceFXActorInstance->StopAnim();
	}
#endif // WITH_FACEFX
}


/** Returns TRUE if Face FX is currently playing an animation */
UBOOL USkeletalMeshComponent::IsPlayingFaceFXAnim()
{
#if WITH_FACEFX
	if( !FaceFXActorInstance )
	{
		return FALSE;
	}

	return (FaceFXActorInstance->IsPlayingAnim() == FxTrue);
#else
	return FALSE;
#endif
}


void USkeletalMeshComponent::DeclareFaceFXRegister( const FString& RegName )
{
#if WITH_FACEFX
	// This will not add duplicate entries.
	FFaceFXRegMap::AddRegisterMapping(*RegName);
#endif // WITH_FACEFX
}

FLOAT USkeletalMeshComponent::GetFaceFXRegister( const FString& RegName )
{
#if WITH_FACEFX
	if( FaceFXActorInstance )
	{
		FFaceFXRegMapEntry* pRegMapEntry = FFaceFXRegMap::GetRegisterMapping(*RegName);
		if( pRegMapEntry )
		{
			return FaceFXActorInstance->GetRegister(pRegMapEntry->FaceFXRegName);
		}
		else
		{
			debugf(TEXT("FaceFX: WARNING Attempt to read from undeclared register %s"), *RegName);
		}
	}
#endif // WITH_FACEFX
	return 0.0f;
}

void USkeletalMeshComponent::SetFaceFXRegister( const FString& RegName, FLOAT RegVal, BYTE RegOp, FLOAT InterpDuration )
{
#if WITH_FACEFX
	if( FaceFXActorInstance )
	{
		FFaceFXRegMapEntry* pRegMapEntry = FFaceFXRegMap::GetRegisterMapping(*RegName);
		if( pRegMapEntry )
		{
			FxRegisterOp RegisterOp = RO_Add;
			switch( RegOp )
			{
			case FXRO_Add:
				RegisterOp = RO_Add;
				break;
			case FXRO_Multiply:
				RegisterOp = RO_Multiply;
				break;
			case FXRO_Replace:
				RegisterOp = RO_Replace;
				break;
			default:
				debugf(TEXT("FaceFX: Invalid RegOp in USkeletalMeshComponent::SetFaceFXRegister()!"));
				break;
			}
			FaceFXActorInstance->SetRegister(pRegMapEntry->FaceFXRegName, RegVal, RegisterOp, InterpDuration);
		}
		else
		{
			debugf(TEXT("FaceFX: WARNING Attempt to write to undeclared register %s"), *RegName);
		}
	}
#endif // WITH_FACEFX
}

void USkeletalMeshComponent::SetFaceFXRegisterEx( const FString& RegName, BYTE RegOp, FLOAT FirstValue, FLOAT FirstInterpDuration, FLOAT NextValue, FLOAT NextInterpDuration )
{
#if WITH_FACEFX
	if( FaceFXActorInstance )
	{
		FFaceFXRegMapEntry* pRegMapEntry = FFaceFXRegMap::GetRegisterMapping(*RegName);
		if( pRegMapEntry )
		{
			FxRegisterOp RegisterOp = RO_Add;
			switch( RegOp )
			{
			case FXRO_Add:
				RegisterOp = RO_Add;
				break;
			case FXRO_Multiply:
				RegisterOp = RO_Multiply;
				break;
			case FXRO_Replace:
				RegisterOp = RO_Replace;
				break;
			default:
				debugf(TEXT("FaceFX: Invalid RegOp in USkeletalMeshComponent::SetFaceFXRegisterEx()!"));
				break;
			}
			//@todo FaceFX The second RegisterOp should be the actual NextRegisterOp, so expose it through script and properly use it here.
			FaceFXActorInstance->SetRegisterEx(pRegMapEntry->FaceFXRegName, RegisterOp, FirstValue, FirstInterpDuration, RegisterOp, NextValue, NextInterpDuration);
		}
		else
		{
			debugf(TEXT("FaceFX: WARNING Attempt to write to undeclared register %s"), *RegName);
		}
	}
#endif // WITH_FACEFX
}

/** Takes sorted array Base and then adds any elements from sorted array Insert which is missing from it, preserving order. */
static void MergeInByteArray(TArray<BYTE>& BaseArray, TArray<BYTE>& InsertArray)
{
	// Then we merge them into the array of required bones.
	INT BaseBonePos = 0;
	INT InsertBonePos = 0;

	// Iterate over each of the bones we need.
	while( InsertBonePos < InsertArray.Num() )
	{
		// Find index of physics bone
		BYTE InsertByte = InsertArray(InsertBonePos);

		// If at end of BaseArray array - just append.
		if( BaseBonePos == BaseArray.Num() )
		{
			BaseArray.AddItem(InsertByte);
			BaseBonePos++;
			InsertBonePos++;
		}
		// If in the middle of BaseArray, merge together.
		else
		{
			// Check that the BaseArray array is strictly increasing, otherwise merge code does not work.
			check( BaseBonePos == 0 || BaseArray(BaseBonePos-1) < BaseArray(BaseBonePos) );

			// Get next required bone index.
			BYTE BaseByte = BaseArray(BaseBonePos);

			// We have a bone in BaseArray not required by Insert. Thats ok - skip.
			if( BaseByte < InsertByte )
			{
				BaseBonePos++;
			}
			// Bone required by Insert is in 
			else if(BaseByte == InsertByte )
			{
				BaseBonePos++;
				InsertBonePos++;
			}
			// Bone required by Insert is missing - insert it now.
			else // BaseByte > InsertByte
			{
				BaseArray.Insert(BaseBonePos);
				BaseArray(BaseBonePos) = InsertByte;

				BaseBonePos++;
				InsertBonePos++;
			}
		}
	}
}

void USkeletalMeshComponent::RebuildVisibilityArray()
{
	// If the BoneVisibilityStates array has a 0 for a parent bone, all children bones are meant to be hidden as well
	// (as the concatenated matrix will have scale 0).  This code propagates explicitly hidden parents to children.

	// On the first read of any cell of BoneVisibilityStates, BVS_HiddenByParent and BVS_Visible are treated as visible.
	// If it starts out visible, the value written back will be BVS_Visible if the parent is visible; otherwise BVS_HiddenByParent.
	// If it starts out hidden, the BVS_ExplicitlyHidden value stays in place

	// The following code relies on a complete hierarchy sorted from parent to children
	check(BoneVisibilityStates.Num() == SkeletalMesh->RefSkeleton.Num());
	for (INT BoneId=0; BoneId < BoneVisibilityStates.Num(); ++BoneId)
	{
		BYTE VisState = BoneVisibilityStates(BoneId);

		// if not exclusively hidden, consider if parent is hidden
		if (VisState != BVS_ExplicitlyHidden)
		{
			// Check direct parent (only need to do one deep, since we have already processed the parent and written to BoneVisibilityStates previously)
			const INT ParentIndex = SkeletalMesh->RefSkeleton(BoneId).ParentIndex;
			if ((ParentIndex == 0) || (BoneVisibilityStates(ParentIndex) == BVS_Visible))
			{
				BoneVisibilityStates(BoneId) = BVS_Visible;
			}
			else
			{
				BoneVisibilityStates(BoneId) = BVS_HiddenByParent;
			}
		}
	}
}

/** Recalculates the RequiredBones array in this SkeletalMeshComponent based on current SkeletalMesh, LOD and PhysicsAsset. */
void USkeletalMeshComponent::RecalcRequiredBones(INT LODIndex)
{
	// The list of bones we want is taken from the predicted LOD level.
	FStaticLODModel& LODModel = SkeletalMesh->LODModels(LODIndex);
	if (LODInfo.IsValidIndex(LODIndex))
	{
		const FSkelMeshComponentLODInfo& MeshCompLODInfo = LODInfo(LODIndex);

		// The LODModel.RequiredBones array only includes bones that are desired for that LOD level.
		// They are also in strictly increasing order, which also infers parents-before-children.
		if (MeshCompLODInfo.bAlwaysUseInstanceWeights && MeshCompLODInfo.InstanceWeightUsage == IWU_FullSwap)
		{
			check(MeshCompLODInfo.InstanceWeightIdx < LODModel.VertexInfluences.Num());
			RequiredBones = LODModel.VertexInfluences(MeshCompLODInfo.InstanceWeightIdx).RequiredBones;
		}
		else
		{
			RequiredBones = LODModel.RequiredBones;
		}
	}
	else
	{
		RequiredBones = LODModel.RequiredBones;
	}

	// If we have a PhysicsAsset, we also need to make sure that all the bones used by it are always updated, as its used
	// by line checks etc. We might also want to kick in the physics, which means having valid bone transforms.
	if(PhysicsAsset)
	{
		TArray<BYTE> PhysAssetBones;
		for(INT i=0; i<PhysicsAsset->BodySetup.Num(); i++ )
		{
			INT PhysBoneIndex = SkeletalMesh->MatchRefBone( PhysicsAsset->BodySetup(i)->BoneName );
			if(PhysBoneIndex != INDEX_NONE)
			{
				PhysAssetBones.AddItem(PhysBoneIndex);
			}	
		}

		// Then sort array of required bones in hierarchy order
		Sort<USE_COMPARE_CONSTREF(BYTE, UnSkeletalComponent)>( &PhysAssetBones(0), PhysAssetBones.Num() );

		// Make sure all of these are in RequiredBones.
		MergeInByteArray(RequiredBones, PhysAssetBones);
	}

	// Make sure that bones with per-poly collision are also always updated.
	if(SkeletalMesh->PerPolyCollisionBones.Num() > 0)
	{
		TArray<BYTE> PerPolyCollisionBones;
		for(INT i=0; i<SkeletalMesh->PerPolyCollisionBones.Num(); i++ )
		{
			INT PerPolyBoneIndex = SkeletalMesh->MatchRefBone( SkeletalMesh->PerPolyCollisionBones(i) );
			if(PerPolyBoneIndex != INDEX_NONE)
			{
				PerPolyCollisionBones.AddItem(PerPolyBoneIndex);
			}	
		}

		// Once again, sort and merge.
		Sort<USE_COMPARE_CONSTREF(BYTE, UnSkeletalComponent)>( &PerPolyCollisionBones(0), PerPolyCollisionBones.Num() );
		MergeInByteArray(RequiredBones, PerPolyCollisionBones);
	}

	// Purge invisible bones and their children
	// this has to be done before mirror table check/phsysics body checks
	// mirror table/phys body ones has to be calculated
	{
		check(BoneVisibilityStates.Num() == SkeletalMesh->RefSkeleton.Num());

		INT VisibleBoneWriteIndex = 0;
		for (INT i = 0; i < RequiredBones.Num(); ++i)
		{
			BYTE CurBoneIndex = RequiredBones(i);

			// Current bone visible?
			if (BoneVisibilityStates(CurBoneIndex) == BVS_Visible)
			{
				RequiredBones(VisibleBoneWriteIndex++) = CurBoneIndex;
			}
		}

		// Remove any trailing junk in the RequiredBones array
		const INT NumBonesHidden = RequiredBones.Num() - VisibleBoneWriteIndex;
		if (NumBonesHidden > 0)
		{
			RequiredBones.Remove(VisibleBoneWriteIndex, NumBonesHidden);
		}
	}

	// Add in any bones that may be required when mirroring.
	// JTODO: This is only required if there are mirroring nodes in the tree, but hard to know...
	if(SkeletalMesh->SkelMirrorTable.Num() == LocalAtoms.Num())
	{
		TArray<BYTE> MirroredDesiredBones;
		MirroredDesiredBones.Add(RequiredBones.Num());

		// Look up each bone in the mirroring table.
		for(INT i=0; i<RequiredBones.Num(); i++)
		{
			MirroredDesiredBones(i) = SkeletalMesh->SkelMirrorTable(RequiredBones(i)).SourceIndex;
		}

		// Sort to ensure strictly increasing order.
		Sort<USE_COMPARE_CONSTREF(BYTE, UnSkeletalComponent)>(&MirroredDesiredBones(0), MirroredDesiredBones.Num());

		// Make sure all of these are in RequiredBones, and 
		MergeInByteArray(RequiredBones, MirroredDesiredBones);
	}

	// Ensure that we have a complete hierarchy down to those bones.
	UAnimNode::EnsureParentsPresent(RequiredBones, SkeletalMesh);

	// Required Bones array updated, we need to update the ones for 3 pass composing.
	bUpdateComposeSkeletonPasses = TRUE;
}

#if PERF_ENABLE_GETBONEATOM_STATS || PERF_ENABLE_INITANIM_STATS
IMPLEMENT_COMPARE_CONSTREF( FAnimNodeTimeStat, UnSkeletalComponent, { return (B.NodeExclusiveTime > A.NodeExclusiveTime) ? 1 : -1; } )
#endif

// Track root motion rotation accumulation errors
#define ROOT_MOTION_ROTATION_ERROR 0

#if ROOT_MOTION_ROTATION_ERROR
static FQuat AccumRMQuat;
static FRotator AccumRMRotation;
#endif


void USkeletalMeshComponent::ProcessRootMotion( FLOAT DeltaTime, FBoneAtom& ExtractedRootMotionDelta, INT& bHasRootMotion )
{
	// If an interp curve has been setup to use, use that instead of the extracted root motion from the bone atoms
	// Only usable for pawns mesh right now
	APawn* PawnOwner = Cast<APawn>(GetOwner());
	if( PawnOwner != NULL && PawnOwner->bRootMotionFromInterpCurve && PawnOwner->Mesh == this )
	{
		//const FVector OrigTrans = ExtractedRootMotionDelta.GetTranslation();

		PawnOwner->RootMotionInterpCurrentTime += (PawnOwner->RootMotionInterpRate * DeltaTime);

		FInterpCurveInitVector VC;
		VC.Points = PawnOwner->RootMotionInterpCurve.Curve.Points;
		ExtractedRootMotionDelta.SetTranslation(VC.Eval( PawnOwner->RootMotionInterpCurrentTime, FVector::ZeroVector ));
		PawnOwner->RootMotionInterpCurveLastValue = ExtractedRootMotionDelta.GetTranslation();

		bHasRootMotion = !ExtractedRootMotionDelta.GetTranslation().IsNearlyZero();

		//warnf(TEXT("Orig Trans %s Interp Curve Trans %s -- Time: %f"), *OrigTrans.ToString(), *ExtractedRootMotionDelta.GetTranslation().ToString(), PawnOwner->RootMotionInterpCurrentTime );

#if 0 // DEBUG (draw the curve in game so you can see it)
			PawnOwner->FlushPersistentDebugLines();
			FInterpCurveInitVector VCDebug;
			VCDebug.Points = PawnOwner->RootMotionInterpCurve.Curve.Points;
			FLOAT CurrentTime = 0.f;
			FVector PrevPos = PawnOwner->Location;
			FVector CurrPos(0.f);
			while( CurrentTime <= PawnOwner->RootMotionInterpCurve.MaxCurveTime )
			{
				CurrPos = LocalToWorld.TransformNormal(VCDebug.Eval( CurrentTime, FVector(0,0,0) ));
				PawnOwner->DrawDebugLine( PrevPos, CurrPos + PrevPos, 255, 0, 0, TRUE );
				if( CurrentTime == 0.f )
				{
					PawnOwner->DrawDebugBox( CurrPos + PrevPos, FVector(5.f), 255, 0, 0, TRUE );
				}

				PrevPos += CurrPos;
				CurrentTime += DeltaTime; // this will give us the curve as "played" in the game and not the original value
			}
			PawnOwner->DrawDebugBox( PrevPos, FVector(5.f), 0, 0, 255, TRUE );	
#endif

	}

	// Root motion movement is done only once per tick. 
	// Because RootMotionDelta is relative to the last time the animation was ticked. 
	// So we can't just arbitrarily call that function and have root motion work, the same offsets would be applied every time.
	// UPDATE: Adding support for Matineee root motion. The DeltaTime can be minus as it can go backward.
	// DeltaTime will the elapsed time since last updated. 
	// Second condition make sure it allows for Matinee. 
	if( DeltaTime > 0.f || (GIsEditor && !GIsGame) )
	{
		// If PendingRMM has changed, set it
		if( PendingRMM != OldPendingRMM )
		{
			// Already set, do nothing
			if( RootMotionMode == PendingRMM )
			{
				OldPendingRMM = PendingRMM;
			}
			// delay by a frame if setting to RMM_Ignore AND animation extracted root motion on this frame.
			// This is to force physics to process the entire root motion.
			else if( PendingRMM != RMM_Ignore || !bHasRootMotion || bRMMOneFrameDelay == 1 )
			{
				RootMotionMode		= PendingRMM;
				OldPendingRMM		= PendingRMM;
				bRMMOneFrameDelay	= 0;
			}
			else
			{
				bRMMOneFrameDelay	= 1;
			}
		}

		// if root motion is requested, then transform it from mesh space to world space so it can be used.
		if( bHasRootMotion && RootMotionMode != RMM_Ignore )
		{
#if 0 // DEBUG
			warnf(TEXT("%3.2f [%s] [%0.5f] Extracted RM, PreProcessing, Translation: %3.3f, vect: %s, RootMotionAccelScale: %s"), 
				GWorld->GetTimeSeconds(), *Owner->GetName(), DeltaTime, ExtractedRootMotionDelta.GetTranslation().Size(), *ExtractedRootMotionDelta.GetTranslation().ToString(), *RootMotionAccelScale.ToString());
#endif
			// Transform mesh space root delta translation to world space
			ExtractedRootMotionDelta.SetTranslation(LocalToWorld.TransformNormal(ExtractedRootMotionDelta.GetTranslation()));

			// Scale RootMotion translation in Mesh Space.
			if( RootMotionAccelScale != FVector(1.f) )
			{
				ExtractedRootMotionDelta.SetTranslation(ExtractedRootMotionDelta.GetTranslation() * RootMotionAccelScale);
			}

			// If Owner required a Script event forwarded when root motion has been extracted, forward it
			if( Owner && bRootMotionExtractedNotify )
			{
				Owner->eventRootMotionExtracted(this, ExtractedRootMotionDelta);
			}

			// Root Motion delta is accumulated every time it is extracted.
			// This is because on servers using autonomous physics, physics updates and ticking are out of synch.
			// So 2 physics updates can happen in a row, or 2 animation updates, effectively
			// making client and server out of synch.
			// So root motion is accumulated, and reset when used by physics.
			RootMotionDelta.AddToTranslation(ExtractedRootMotionDelta.GetTranslation());
			RootMotionVelocity = ExtractedRootMotionDelta.GetTranslation() / DeltaTime;
		}
		else
		{
			RootMotionDelta.SetTranslation(FVector::ZeroVector);
			RootMotionVelocity			= FVector::ZeroVector;
		}

#if 0 // DEBUG
		static FVector	AccumulatedRMTranslation = FVector(0.f);
		{
			if( RootMotionMode != RMM_Ignore )
			{
				AccumulatedRMTranslation	+= ExtractedRootMotionDelta.GetTranslation();
			}
			else
			{
				AccumulatedRMTranslation	= FVector(0.f);
			}

			if( RootMotionMode != RMM_Ignore )
			{
				debugf(TEXT("%3.2f [%s] RM Translation: %3.3f, vect: %s"), GWorld->GetTimeSeconds(), *Owner->GetName(), RootMotionDelta.GetTranslation().Size(), *RootMotionDelta.GetTranslation().ToString());
				debugf(TEXT("%3.2f [%s] RM Velocity: %3.3f, vect: %s"), GWorld->GetTimeSeconds(), *Owner->GetName(), RootMotionVelocity.Size(), *RootMotionVelocity.ToString());
				debugf(TEXT("%3.2f [%s] RM AccumulatedRMTranslation: %3.3f, vect: %s"), GWorld->GetTimeSeconds(), *Owner->GetName(), AccumulatedRMTranslation.Size(), *AccumulatedRMTranslation.ToString());
			}
		}
#endif

		if( bHasRootMotion && RootMotionRotationMode != RMRM_Ignore )
		{
			FMatrix	TM = LocalToWorld;
			TM.RemoveScaling();
			FQuat	MeshToWorldQuat(TM);

			// Transform mesh space delta rotation to world space.
			RootMotionDelta.SetRotation(MeshToWorldQuat * ExtractedRootMotionDelta.GetRotation() * (-MeshToWorldQuat));
			RootMotionDelta.NormalizeRotation();
		}
		else
		{
			RootMotionDelta.SetRotation(FQuat::Identity);
		}

#if 0 // DEBUG ROOT ROTATION
		if( RootMotionRotationMode != RMRM_Ignore )
		{
			const FRotator DeltaRotation = FQuatRotationTranslationMatrix(RootMotionDelta.GetRotation(), FVector(0.f)).Rotator();
			debugf(TEXT("%3.2f Root Rotation: %s"), GWorld->GetTimeSeconds(), *DeltaRotation.ToString());
		}
#endif
		
#if ROOT_MOTION_ROTATION_ERROR
		// Clear up rotation error compensation code
		if( RootMotionRotationMode == RMRM_Ignore )
		{
			AccumRMQuat = FQuat::Identity;
			AccumRMRotation = FRotator(0,0,0); 
		}
#endif
		// Motion applied right away
		if( bHasRootMotion )
		{
			if( (RootMotionMode == RMM_Translate || RootMotionRotationMode == RMRM_RotateActor || 
				(RootMotionMode == RMM_Ignore && PreviousRMM == RMM_Translate)) )	// If root motion was just turned off, forward remaining root motion.
			{
				/** 
				* Delay applying instant translation for one frame
				* So we check for PreviousRMM to be up to date with the current root motion mode.
				* We need to do this because in-game physics have already been applied for this tick.
				* So we want root motion to kick in for next frame.
				*/
				const UBOOL		bCanDoTranslation	= (RootMotionMode == RMM_Translate && PreviousRMM == RMM_Translate);
				const FVector	InstantTranslation	= bCanDoTranslation ? RootMotionDelta.GetTranslation() : FVector::ZeroVector;

				const UBOOL		bCanDoRotation		= (RootMotionRotationMode == RMRM_RotateActor);
				const FMatrix	DeltaRotM			= FQuatRotationTranslationMatrix(RootMotionDelta.GetRotation(), FVector::ZeroVector);
				const UBOOL		bShouldDoRotation	= bCanDoRotation && !DeltaRotM.Rotator().IsZero();

				if( Owner && (bShouldDoRotation || InstantTranslation.SizeSquared() > SMALL_NUMBER) )
				{
					FRotator DesiredRotation = Owner->Rotation;
					if( bShouldDoRotation )
					{
						FRotator const DeltaRotation = RootMotionDelta.GetRotation().Rotator();
						DesiredRotation = (Owner->Rotation + DeltaRotation).GetNormalized();

#if 0 // DEBUG ROOT MOTION
						debugf(TEXT("%3.2f Root Motion Instant. DesiredRotation: %s"), GWorld->GetTimeSeconds(), *DesiredRotation.ToString());
#endif

#if ROOT_MOTION_ROTATION_ERROR
						// Accumulate quaternion rotation
						AccumRMQuat = AccumRMQuat * RootMotionDelta.GetRotation();
						// Accumulate the actual applied FRotator rotation
						AccumRMRotation = (AccumRMRotation + DeltaRotation).GetNormalized();
						// Compute accumulated error
						FRotator const AccumErrorRot = (AccumRMQuat.Rotator() - AccumRMRotation).GetNormalized();
// 						DesiredRotation += AccumErrorRot;  //Comment out these lines to debug FQuat vs FRotator error
// 						AccumRMRotation += AccumErrorRot;  //Comment out these lines to debug FQuat vs FRotator error 
	#if 1 // DEBUG ROOT MOTION
						debugf(TEXT("Accum RMRot: %s Accum RMQuat: %s Error: %s"),
							*AccumRMRotation.ToString(),
							*(AccumRMQuat.Rotator().ToString()),
							*(AccumErrorRot.ToString()) ); 
	#endif 
#endif
					}

					// Transform mesh directly. Doesn't take in-game physics into account.
					FCheckResult Hit(1.f);
					GWorld->MoveActor(Owner, InstantTranslation, DesiredRotation, 0, Hit);

#if !FINAL_RELEASE
					// @fixme: delete this - this is for temporary debugging matinee
					if (GIsEditor && !GIsGame)
					{
						debugf(TEXT("Matinee(EnableRootMotion) : Translation (%s), Rotation (%s), RootMotionVelocity(%s), RootMotionDelta(%s)"), *InstantTranslation.ToString(), *DesiredRotation.ToString(), *RootMotionVelocity.ToString(), *RootMotionDelta.ToString());
					}
#endif
					// If we have used translation, reset the accumulator.
					if( bCanDoTranslation )
					{
						RootMotionDelta.SetTranslation(FVector::ZeroVector);
					}

					if( bCanDoRotation )
					{
						// Update DesiredRotation for AI Controlled Pawns
						if( PawnOwner )
						{
							PawnOwner->SetDesiredRotation(Owner->Rotation);
						}
					}
				}
			}

			// RMM Relative
			if( RootMotionMode == RMM_Relative || (RootMotionMode == RMM_Ignore && PreviousRMM == RMM_Relative) )
			{
				const UBOOL		bCanDoTranslation	= (RootMotionMode == RMM_Relative && PreviousRMM == RMM_Relative);
				const FVector	InstantTranslation	= bCanDoTranslation ? RootMotionDelta.GetTranslation() : FVector::ZeroVector;
				if( Owner && Owner->Base && InstantTranslation.SizeSquared() > SMALL_NUMBER )
				{
					if( Owner->bHardAttach )
					{
						// Remove actor from base
						AActor* OldBase = Owner->Base;
						USkeletalMeshComponent* OldBaseSkelComponent = Owner->BaseSkelComponent;
						FName OldBaseBoneName = Owner->BaseBoneName;

						// do a stat here to see if ths taking some time
						Owner->SetBase(NULL, FVector(0.f,0.f,1.f), 0, NULL, NAME_None);

#if 0 // DEBUG ROOT ROTATION
						{
							debugf(TEXT("RMM_Relative : %3.2f Before Move Current Location : %s, Instant Translation Value : %s, RelativeLocation, %s"), GWorld->GetTimeSeconds(), *Owner->Location.ToString(), *InstantTranslation.ToString(), *Owner->RelativeLocation.ToString());
						}
#endif
						// Move to new location
						FCheckResult Hit(1.f);
						GWorld->MoveActor(Owner, InstantTranslation, Owner->Rotation, 0, Hit);

#if 0 // DEBUG ROOT ROTATION
						{
							debugf(TEXT("RMM_Relative : %3.2f After Move Current Location : %s, RelativeLocation: %s"), GWorld->GetTimeSeconds(), *Owner->Location.ToString() , *Owner->RelativeLocation.ToString());
						}
#endif

						// Rebase Actor				
						Owner->SetBase(OldBase, FVector(0.f,0.f,1.f), 0, OldBaseSkelComponent, OldBaseBoneName);

						// If we have used translation, reset the accumulator.
						if( bCanDoTranslation )
						{
							RootMotionDelta.SetTranslation(FVector::ZeroVector);
						}
					}
				}
			}
		}

		// Tick Physics right away for RMM_Accel, so there is no frame of lag.
		// Checking GIsGame because in Editor Matinee can allow Root Motion to be extracted with DeltaTime <= 0.
		if( RootMotionMode == RMM_Accel && GIsGame && Owner )
		{
			APawn* P = Owner->GetAPawn();
			if( Owner->Role != ROLE_SimulatedProxy || (P && P->ShouldBypassSimulatedClientPhysics()) )
			{
				if( P != NULL )
				{
					APlayerController* PC = P->Controller ? P->Controller->GetAPlayerController() : NULL;
					if( PC != NULL && PC->RemoteRole == ROLE_AutonomousProxy && !PC->IsLocalPlayerController() )
					{
						PC->ServerTimeStamp = GWorld->GetWorldInfo()->TimeSeconds;
					}
				}

				check(DeltaTime > 0.f);
				bProcessingRootMotion = TRUE;
				Owner->performPhysics(DeltaTime);
				bProcessingRootMotion = FALSE;
				// Physics can kill owner :(
				if( !Owner || Owner->bDeleteMe )
				{
					return;
				}
			}
		}

		// Track root motion mode changes
		if( RootMotionMode != PreviousRMM )
		{
			// notify owner that root motion mode changed. 
			// if RootMotionMode != RMM_Ignore, then on next frame root motion will kick in.
			if( bRootMotionModeChangeNotify && Owner )
			{
				Owner->eventRootMotionModeChanged(this);
			}
			PreviousRMM = RootMotionMode;

			// If switching from ShouldBypassSimulatedClientPhysics to not, force a location & velocity update for replication.
			if( PawnOwner && PawnOwner->bPrevBypassSimulatedClientPhysics && !PawnOwner->ShouldBypassSimulatedClientPhysics())
			{
				if (Owner->Role == ROLE_SimulatedProxy )
				{
					PawnOwner->bSimulateGravity = ((PawnOwner->Physics == PHYS_Falling) || (PawnOwner->Physics == PHYS_Walking));
				}
				else
				{
					AActor* OldBase = Owner->Base;
					USkeletalMeshComponent* OldBaseSkelComponent = Owner->BaseSkelComponent;
					FName OldBaseBoneName = Owner->BaseBoneName;
					FVector OldFloor = Owner->GetAPawn() ? Owner->GetAPawn()->Floor : FVector(0.f,0.f,1.f);

					FVector const Nudge(0.f, 0.f, 0.1f);
					Owner->SetLocation( Owner->Location + Nudge );
					Owner->Velocity += Nudge;

					// Restore Base we lost by doing a set location.
					Owner->SetBase(OldBase, OldFloor, 0, OldBaseSkelComponent, OldBaseBoneName);
				}
			}
		}

		if( PawnOwner )
		{
			PawnOwner->bPrevBypassSimulatedClientPhysics = PawnOwner->ShouldBypassSimulatedClientPhysics();
		}

		// Notify Owner, if requested, that we have processed Root Motion.
		if( bNotifyRootMotionProcessed && Owner )
		{
			Owner->eventRootMotionProcessed(this);
		}
	}

#if 0 // DEBUG root Rotation
	if( Owner )
	{
		Owner->DrawDebugLine(Owner->Location, Owner->Location + Owner->Rotation.Vector() * 200.f, 255, 0, 0, FALSE);
	}
#endif
}


void USkeletalMeshComponent::UpdateMorph( FLOAT DeltaTime, UBOOL bTickFaceFX)
{
	// Can't do anything without a SkeletalMesh
	if( !SkeletalMesh )
	{
		return;
	}

	if ( !ParentAnimComponent )
	{
		return;
	}

	// Update bones transform from animations (if present)
	bRecentlyRendered = (LastRenderTime > GWorld->GetWorldInfo()->TimeSeconds - 1.0f);

	// Update the ActiveMorphs array.
	if( GIsEditor || bRecentlyRendered || bUpdateSkelWhenNotRendered )
	{
		UpdateActiveMorphs();
	}
	else
	{
		// clear it, so that it doesn't send to renderer
		ActiveMorphs.Empty();
		ActiveCurveMorphs.Empty();
	}
}
/**
 * Update the SpaceBases array of component-space bone transforms.
 * This will evaluate any animation blend tree if present (or use the reference pose if not).
 * It will then blend in any physics information from a PhysicsAssetInstance based on the PhysicsWeight value.
 * Then evaluates any root bone options to apply translation to the owning Actor if desired.
 * Finally it composes all the relative transforms to calculate component-space transforms for each bone. 
 * Applying per-bone controllers is done as part of multiplying the relative transforms down the heirarchy.
 *
 * NOTE: DeltaTime is optional and can be zero!!!
 *
 *	@param	bTickFaceFX		Passed to FaceFX to tell it to update facial state based on global clock or wave position.
 *							Set to false if you are forcing a pose outside of this function and do not want it over-ridden.
 */
void USkeletalMeshComponent::UpdateSkelPose( FLOAT DeltaTime, UBOOL bTickFaceFX )
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateSkelPose);

	// Can't do anything without a SkeletalMesh
	if( !SkeletalMesh )
	{
		return;
	}

	// Update bones transform from animations (if present)
	bRecentlyRendered = (LastRenderTime > GWorld->GetWorldInfo()->TimeSeconds - 1.0f);

	const UBOOL bOldIgnoreControllers = bIgnoreControllers;

	// Allocate transforms if not present.
	if ( ParentAnimComponent == NULL )
	{
		if( SpaceBases.Num() != SkeletalMesh->RefSkeleton.Num() )
		{
			SpaceBases.Empty( SkeletalMesh->RefSkeleton.Num() );
			SpaceBases.Add( SkeletalMesh->RefSkeleton.Num() );

			// This used to be fine until I get latest. There might have been code added
			// for this to be used before initialized
			for(INT I=0; I<SkeletalMesh->RefSkeleton.Num(); ++I)
			{
				SpaceBases(I).SetIdentity();
			}

			// Controls sometimes use last frames position of a bone. But if that is not valid (ie array is freshly allocated)
			// we need to turn them off.
			bIgnoreControllers = TRUE;
		}

		if( LocalAtoms.Num() != SkeletalMesh->RefSkeleton.Num() )
		{
			LocalAtoms.Empty( SkeletalMesh->RefSkeleton.Num() );
			LocalAtoms.Add( SkeletalMesh->RefSkeleton.Num() );
		}

		if( BoneVisibilityStates.Num() != SkeletalMesh->RefSkeleton.Num())
		{
			BoneVisibilityStates.Empty( SkeletalMesh->RefSkeleton.Num() );
			if( SkeletalMesh->RefSkeleton.Num() )
			{
				BoneVisibilityStates.Add( SkeletalMesh->RefSkeleton.Num() );
				for (INT BoneIndex = 0; BoneIndex < SkeletalMesh->RefSkeleton.Num(); BoneIndex++)
				{
					BoneVisibilityStates( BoneIndex ) = BVS_Visible;
				}
			}
		}
	}

	// Do nothing more if no bones in skeleton.
	if( SpaceBases.Num() == 0 )
	{
		bIgnoreControllers = bOldIgnoreControllers;
		return;
	}

	// See if this mesh is far enough from the viewer we should stop updating kinematic rig
	UBOOL bNewNotUpdateKinematic = FALSE;
	if(	MinDistFactorForKinematicUpdate > 0.f 
		&& (MaxDistanceFactor < MinDistFactorForKinematicUpdate || (!bRecentlyRendered && !bUpdateSkelWhenNotRendered) ) )
	{
		bNewNotUpdateKinematic = TRUE;

#if !FINAL_RELEASE
		if( ( Owner != NULL ) && ( Owner->Physics == PHYS_RigidBody ) )
		{
			warnf( TEXT( "SkelMesh has: MinDistFactorForKinematicUpdate is > 0.0f and in PHYS_RigidBody.  This will probably result in it falling out of the world.   Component: %s  SkeletalMesh: %s"), *GetPathName(), *SkeletalMesh->GetName() );
		}
#endif // !FINAL_RELEASE
	}

	// Flag to indicate this is a frame where we have just turned on kinematic updating of bodies again.  
	UBOOL bJustEnabledKinematicUpdate = FALSE;  
	UBOOL bKinematicUpdateStateChanged = FALSE;  

	// Turn off BlockRigidBody when we stop updating kinematics, and turn it back on when we start again.  
	if(bNotUpdatingKinematicDueToDistance && !bNewNotUpdateKinematic)   
	{   
		bKinematicUpdateStateChanged = TRUE;  
		bJustEnabledKinematicUpdate = TRUE;   
	}  
	else if(!bNotUpdatingKinematicDueToDistance && bNewNotUpdateKinematic)  
	{  
		bKinematicUpdateStateChanged = TRUE;
		PutRigidBodyToSleep(); // Don't need to simulate these any more
	}  
	bNotUpdatingKinematicDueToDistance = bNewNotUpdateKinematic;  

	// This also looks at bNotUpdatingKinematicDueToDistance and sets the collision state accordingly  
	if(bKinematicUpdateStateChanged)
	{
		SetBlockRigidBody(BlockRigidBody);
	}

// 	// Log if this mesh has no PhysicsAsset (collision)
// 	if( !bHasPhysicsAssetInstance )
// 	{
// 		debugf(TEXT("%s has no PhysicsAsset. SkelMesh: %s, AnimTree: %s, Owner: %s"), *GetFName().ToString(), *SkeletalMesh->GetFName().ToString(), AnimTreeTemplate ? *AnimTreeTemplate->GetFName().ToString() : TEXT("None"), *GetOwner()->GetFName().ToString());
// 	}

	UBOOL bCanPreserveOldAtoms = bRequiredBonesUpToDate &&
		SkeletalMesh->RefSkeleton.Num() == CachedLocalAtoms.Num() && SkeletalMesh->RefSkeleton.Num() == CachedSpaceBases.Num() && LocalAtoms.Num() == CachedLocalAtoms.Num() && SpaceBases.Num() == CachedSpaceBases.Num();
	if (bSkipGetBoneAtoms)
	{
		// Make sure our cached data is up to date.
		if( bRequiredBonesUpToDate
			&& SkeletalMesh->RefSkeleton.Num() == CachedLocalAtoms.Num() 
			&& SkeletalMesh->RefSkeleton.Num() == CachedSpaceBases.Num() 
			// Root Motion affects physics, needs to be updated every frame.
			&& RootMotionMode == RMM_Ignore 
			&& PreviousRMM == RMM_Ignore
			&& RootMotionRotationMode == RMRM_Ignore )
		{
			if (bInterpolateBoneAtoms && bCanPreserveOldAtoms)
			{
				FLOAT alpha = 0.25f + 1.0f / FLOAT(Max<INT>(SkipRateForTickAnimNodesAndGetBoneAtoms, 2) * 2);
				for(INT Index=0; Index<RequiredBones.Num(); Index++ )
				{
					INT const BoneIndex = RequiredBones(Index);
					LocalAtoms(BoneIndex).BlendWith(CachedLocalAtoms(BoneIndex), alpha);
					SpaceBases(BoneIndex).BlendWith(CachedSpaceBases(BoneIndex), alpha);
				}
			}
			else
			{
				LocalAtoms = CachedLocalAtoms;
				SpaceBases = CachedSpaceBases;
			}

			// When we re-enable kinematic update, we teleport bodies to the right place now (handles flappy bits not getting jerked when updated).
			if( bJustEnabledKinematicUpdate )
			{
				UpdateRBBonesFromSpaceBases(LocalToWorld, TRUE, TRUE);
			}

			// If desired, pass the animation data to the physics joints so they can be used by motors.
			if(PhysicsAssetInstance && bUpdateJointsFromAnimation)
			{
				UpdateRBJointMotors();
			}

			bIgnoreControllers = bOldIgnoreControllers;
			bHasHadPhysicsBlendedIn = FALSE;

			return;
		}
	}

	// Recalculate the RequiredBones array, if necessary
	if( !bRequiredBonesUpToDate )
	{
		RecalcRequiredBones(PredictedLODLevel);
		bRequiredBonesUpToDate = TRUE;
	}

	// We can skip doing some work when using PHYS_RigidBody and physics bodies asleep
	if(bSkipAllUpdateWhenPhysicsAsleep && FramesPhysicsAsleep > 5)
	{
		CachedLocalAtoms.Empty();
		CachedSpaceBases.Empty();
		return;
	}
	if (bCanPreserveOldAtoms)
	{
		Exchange(LocalAtoms, CachedLocalAtoms);
		Exchange(SpaceBases, CachedSpaceBases);
	}
	INC_DWORD_STAT(STAT_SkelComponentTickGBACount);

	// Add a mark to mem stack allocator.
	FMemMark Mark(GMainThreadMemStack);

	// Root motion extracted for this call
	FBoneAtom	ExtractedRootMotionDelta	= FBoneAtom::Identity;
	INT			bHasRootMotion				= 0;
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimBlendTime);
		if( Animations && !bForceRefpose )
		{
			// Before we can get the animation data, we have to make sure all the nodes have been properly initialized.
			// This is because during ticking and during script, Nodes' weight can be changed after they have been ticked.
			// Also during Spawn, Attach() can call UpdateSkelPose(), in which case the nodes haven't been ticked yet.
			// So some nodes might be relevant now, without having DeferredInitAnim() called on them. So ensure this now.
			UAnimNode::CurrentSearchTag++;
			Animations->CallDeferredInitAnim();

			// Check its been initialized
			checkf(Animations->SkelComponent, TEXT("! Component: %s  SkeletalMesh: %s"), *GetPathName(), *SkeletalMesh->GetName());
			// Check we don't have any nasty AnimTree sharing going on or anything.
			checkf(Animations->SkelComponent == this, TEXT("!! Component: %s  SkeletalMesh: %s"), *GetPathName(), *SkeletalMesh->GetName());

			FBoneAtomArray OutAtoms;
			OutAtoms.Add(SkeletalMesh->RefSkeleton.Num());

			// Increment the cache tag, so caches are invalidated on the nodes.
			CachedAtomsTag++;

#if PERF_ENABLE_GETBONEATOM_STATS
			BoneAtomBlendStats.Empty();
			BoneAtomBlendStatsTMap.Empty();
#endif
			//debugf(TEXT("%2.3f: %s GetBoneAtoms(), owner: %s"),GWorld->GetTimeSeconds(),*GetPathName(),*Owner->GetName());
			FCurveKeyArray CurveKeys;

			UAnimNode::NodesRequiringCacheClear.Empty();

			Animations->GetBoneAtoms(OutAtoms, RequiredBones, ExtractedRootMotionDelta, bHasRootMotion, CurveKeys);

			const INT AnimNodeCount = UAnimNode::NodesRequiringCacheClear.Num();
			for(INT i=0; i<AnimNodeCount; ++i)
			{
				UAnimNode::NodesRequiringCacheClear(i)->ClearCachedResult();
			}
			UAnimNode::NodesRequiringCacheClear.Empty();

			if (GIsEditor || bRecentlyRendered)
			{
				ApplyCurveKeys(CurveKeys);
			}

#if PERF_ENABLE_GETBONEATOM_STATS
			if(GShouldLogOutAFrameOfSkelCompTick)
			{
				// Sort results (slowest first)
				Sort<USE_COMPARE_CONSTREF(FAnimNodeTimeStat,UnSkeletalComponent)>( &BoneAtomBlendStats(0), BoneAtomBlendStats.Num() );

				debugf(TEXT(" ======= GetBoneAtom - TIMING - %s %s"), *GetPathName(), SkeletalMesh?*SkeletalMesh->GetName():TEXT("NONE"));
				FLOAT TotalBlendTime = 0.f;
				for(INT i=0; i<BoneAtomBlendStats.Num(); i++)
				{
					debugf(TEXT("%fms\t%s"), BoneAtomBlendStats(i).NodeExclusiveTime * 1000.f, *BoneAtomBlendStats(i).NodeName.ToString());
					TotalBlendTime += BoneAtomBlendStats(i).NodeExclusiveTime;
				}
				debugf(TEXT(" ======= Total Exclusive Time: %fms"), TotalBlendTime * 1000.f);
			}
#endif

			// Finally copy the result of GetBoneAtoms into the output array
			LocalAtoms = OutAtoms;

#ifdef _DEBUG
			// Check that all bone atoms coming from animation are normalized
			for( INT ChckBoneIdx=0; ChckBoneIdx<RequiredBones.Num(); ChckBoneIdx++ )
			{
				const INT	BoneIndex = RequiredBones(ChckBoneIdx);
				check( LocalAtoms(BoneIndex).IsRotationNormalized() );
				check( !LocalAtoms(BoneIndex).ContainsNaN() );
			}
#endif
		}
		else
		{
			UAnimNode::FillWithRefPose(LocalAtoms, RequiredBones, SkeletalMesh->RefSkeleton);
		}
	}

	// Root Motion 
	ProcessRootMotion( DeltaTime, ExtractedRootMotionDelta, bHasRootMotion );

	if( bForceDiscardRootMotion )
	{
		LocalAtoms(0).SetIdentity();
	}

	// Remember the root bone's translation so we can move the bounds.
	RootBoneTranslation = LocalAtoms(0).GetTranslation() - SkeletalMesh->RefSkeleton(0).BonePos.Position;

	// Update the ActiveMorphs array.
	if( GIsEditor || bRecentlyRendered || bUpdateSkelWhenNotRendered )
	{
		UpdateActiveMorphs();
	}
	else
	{
		// clear it, so that it doesn't send to renderer
		ActiveMorphs.Empty();
		ActiveCurveMorphs.Empty();
	}

	if( !bDisableFaceFX && SkeletalMesh->FaceFXAsset )
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateFaceFX);

#if FBONEATOM_TRACK_NAN_ISSUES
		// Initialize all LocalAtoms entries that are not 'required' and therefore not initialized
		// before FaceFX potentially tries to accumulate into one of them
		
		// RequiredBones is a monotonically increasing list of bone indices that are valid
		for(INT RBoneIndex = 0; RBoneIndex < RequiredBones.Num(); RBoneIndex++ )
		{
			// These two represent the two endpoints of deff. required bones
			const INT BoneIndex1 = RequiredBones(RBoneIndex);
			const INT BoneIndex2 = (RBoneIndex+1 < RequiredBones.Num()) ? RequiredBones(RBoneIndex+1) : LocalAtoms.Num();

			// Initialize the ones in between
			for (INT BadBoneIndex = BoneIndex1 + 1; BadBoneIndex < BoneIndex2; ++BadBoneIndex)
			{
				LocalAtoms(BadBoneIndex).SetIdentity();
			}
		}
#endif

		// Do FaceFX processing.
		UpdateFaceFX(LocalAtoms, bTickFaceFX);

#ifdef _DEBUG
		// Check that all bone atoms coming from animation are normalized
		for( INT ChckBoneIdx=0; ChckBoneIdx<RequiredBones.Num(); ChckBoneIdx++ )
		{
			const INT	BoneIndex = RequiredBones(ChckBoneIdx);
			check( LocalAtoms(BoneIndex).IsRotationNormalized() );
		}
#endif
	}

	// We need the world space bone transforms now for two reasons:
	// 1) we don't have physics, so we will not be revisiting this skeletal mesh in UpdateTransform.
	// 2) we do have physics, and want to update the physics state from the animation.
	// This will do controllers and the like.

#if PERF_SHOW_SKELETAL_MESH_COMPONENT_TICK_TIME || LOOKING_FOR_PERF_ISSUES
	const DOUBLE ComposeSkeleton_Start = appSeconds();
#endif
	ComposeSkeleton();

#ifdef _DEBUG
	// Check that all bone atoms coming from animation are normalized
	for( INT ChckBoneIdx=0; ChckBoneIdx<RequiredBones.Num(); ChckBoneIdx++ )
	{
		const INT	BoneIndex = RequiredBones(ChckBoneIdx);
		check( LocalAtoms(BoneIndex).IsRotationNormalized() );
		check( !LocalAtoms(BoneIndex).ContainsNaN() );
	}
#endif

#if PERF_SHOW_SKELETAL_MESH_COMPONENT_TICK_TIME || LOOKING_FOR_PERF_ISSUES
 	if( GShouldLogOutAFrameOfSkelCompTick == TRUE )
 	{
 		const DOUBLE ComposeSkeleton_Time = (appSeconds() - ComposeSkeleton_Start) * 1000.f;
 		debugf(NAME_DevAnim,TEXT( "   ComposeSkeleton_Time: %f" ), ComposeSkeleton_Time );
 	}
#endif

	// UpdateRBBonesFromSpaceBases, updating the physical skeleton from the animated one, is done inside UpdateTransform.

	// When we re-enable kinematic update, we teleport bodies to the right place now (handles flappy bits not getting jerked when updated).
	if(bJustEnabledKinematicUpdate)
	{
		UpdateRBBonesFromSpaceBases(LocalToWorld, TRUE, TRUE);
	}

	// If desired, pass the animation data to the physics joints so they can be used by motors.
	if(PhysicsAssetInstance && bUpdateJointsFromAnimation)
	{
		UpdateRBJointMotors();
	}

	bIgnoreControllers = bOldIgnoreControllers;
	bHasHadPhysicsBlendedIn = FALSE;

	if (bInterpolateBoneAtoms)
	{
		if (bCanPreserveOldAtoms)
		{
			Exchange(LocalAtoms, CachedLocalAtoms);
			Exchange(SpaceBases, CachedSpaceBases);
			FLOAT alpha = 0.25f + 1.0f / FLOAT(Max<INT>(SkipRateForTickAnimNodesAndGetBoneAtoms, 2) * 2);
			for(INT Index=0; Index<RequiredBones.Num(); Index++ )
			{
				INT const BoneIndex = RequiredBones(Index);
				LocalAtoms(BoneIndex).BlendWith(CachedLocalAtoms(BoneIndex), alpha);
				SpaceBases(BoneIndex).BlendWith(CachedSpaceBases(BoneIndex), alpha);
			}
		}
		else
		{
			CachedLocalAtoms = LocalAtoms;
			CachedSpaceBases = SpaceBases;
		}
	}
	else
	{
		CachedLocalAtoms.Empty();
		CachedSpaceBases.Empty();
	}
}

/** Used by the SkelControlFootPlacement to line-check against the world and find the point to place the foot bone. */
UBOOL USkeletalMeshComponent::LegLineCheck(const FVector& Start, const FVector& End, FVector& HitLocation, FVector& HitNormal, const FVector& Extent)
{
	if( Owner )
	{
		DWORD TraceFlags = TRACE_AllBlocking|TRACE_ComplexCollision|TRACE_Accurate;
		FCheckResult const* const pHitList = GWorld->MultiLineCheck(GMainThreadMemStack, End, Start, Extent, TraceFlags, Owner);

		// Filter results, and take first valid hit.
		for( FCheckResult const* Hit = pHitList; Hit != NULL; Hit = Hit->GetNext() )
		{
			// If Primitive Component should skip foot placement line checks, do so.
			if( Hit->Component && !Hit->Component->bBlockFootPlacement )
			{
				continue;
			}
			// Valid hit!
			else
			{
				HitLocation = Hit->Location;
				HitNormal = Hit->Normal;
				return TRUE;
			}
		}
	}

	return FALSE;
}

//
//	USkeletalMeshComponent::UpdateBounds
//

#if !FINAL_RELEASE
extern UBOOL GShouldLogOutAFrameOfPhysAssetBoundsUpdate;
#endif // !FINAL_RELEASE

void USkeletalMeshComponent::UpdateBounds()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateSkelMeshBounds);

	// If physics are asleep, and actor is using physics to move, skip updating the bounds.
	if(GWorld->HasBegunPlay() && FramesPhysicsAsleep > 5 && Owner && Owner->Physics == PHYS_RigidBody)
	{
		return;
	}

	FVector DrawScale = Scale * Scale3D;
	if (Owner != NULL)
	{
		DrawScale *= Owner->DrawScale * Owner->DrawScale3D;
	}

	// See if component is visible - we don't need accurate bounds if it is not
	const UBOOL bShowInEditor = !HiddenEditor && (!Owner || !Owner->IsHiddenEd());
	const UBOOL bShowInGame = !HiddenGame && (!Owner || !Owner->bHidden || bIgnoreOwnerHidden);
	const UBOOL bDetailModeAllowsRendering = (DetailMode <= GSystemSettings.DetailMode);
	const UBOOL bVisible = ( bDetailModeAllowsRendering && ((GIsGame && bShowInGame) || (!GIsGame && bShowInEditor) || bCastHiddenShadow));

	// Can only use the PhysicsAsset to calculate the bounding box if we are not non-uniformly scaling the mesh.
	const UBOOL bCanUsePhysicsAsset = DrawScale.IsUniform() && (SkeletalMesh != NULL)
		// either space base exists or child component
		&& ( (SpaceBases.Num() == SkeletalMesh->RefSkeleton.Num()) || (ParentAnimComponent != NULL && ParentAnimComponent->PhysicsAsset) );

	// if not visible, or we were told to use fixed bounds, use skelmesh bounds
	if(!bVisible || bComponentUseFixedSkelBounds)
	{
#if !FINAL_RELEASE
		const DOUBLE Start = appSeconds();
#endif
		FBoxSphereBounds RootAdjustedBounds = SkeletalMesh->Bounds;
		RootAdjustedBounds.Origin += RootBoneTranslation; // Adjust bounds by root bone translation
		Bounds = RootAdjustedBounds.TransformBy(LocalToWorld);
#if !FINAL_RELEASE
		if(GShouldLogOutAFrameOfPhysAssetBoundsUpdate)
		{
			const DOUBLE Time = appSeconds() - Start;
			debugf(TEXT("SkelMesh_Fixed_BOUNDS: %s %s %s %f ms"), *GetPathName(), *GetDetailedInfo(), *PhysicsAsset->GetName(), Time * 1000.f);
		}
#endif // !FINAL_RELEASE
	}
	else if( ParentAnimComponent && ParentAnimComponent->SkeletalMesh && ParentAnimComponent->bComponentUseFixedSkelBounds )
	{
#if !FINAL_RELEASE
		const DOUBLE Start = appSeconds();
#endif
		FBoxSphereBounds RootAdjustedBounds = ParentAnimComponent->SkeletalMesh->Bounds;
		RootAdjustedBounds.Origin += ParentAnimComponent->RootBoneTranslation; // Adjust bounds by root bone translation
		Bounds = RootAdjustedBounds.TransformBy(LocalToWorld);
#if !FINAL_RELEASE
		if(GShouldLogOutAFrameOfPhysAssetBoundsUpdate)
		{
			const DOUBLE Time = appSeconds() - Start;
			debugf(TEXT("SkelMesh_Parent_Fixed_BOUNDS: %s %s %s %f ms"), *GetPathName(), *GetDetailedInfo(), *PhysicsAsset->GetName(), Time * 1000.f);
		}
#endif // !FINAL_RELEASE
	}
#if WITH_EDITOR
	// For AnimSet Viewer, use 'bounds preview' physics asset if present.
	else if(SkeletalMesh && SkeletalMesh->BoundsPreviewAsset && bCanUsePhysicsAsset)
	{
		Bounds = FBoxSphereBounds(SkeletalMesh->BoundsPreviewAsset->CalcAABB(this));
	}
#endif // WITH_EDITOR
	// If we have a PhysicsAsset (with at least one matching bone), and we can use it, do so to calc bounds.
	else if( PhysicsAsset && bCanUsePhysicsAsset && bHasValidBodies )
	{
#if !FINAL_RELEASE
		const DOUBLE Start = appSeconds();
#endif
		Bounds = FBoxSphereBounds(PhysicsAsset->CalcAABB(this));
#if !FINAL_RELEASE
		if(GShouldLogOutAFrameOfPhysAssetBoundsUpdate)
		{
			const DOUBLE Time = appSeconds() - Start;
			INT NumConsiderForBounds = PhysicsAsset->BoundsBodies.Num();		
			debugf(TEXT("PA_BOUNDS: %s %s %s %f ms  Bodies: %u  NumConsiderForBounds: %u"), *GetPathName(), *GetDetailedInfo(), *PhysicsAsset->GetName(), Time * 1000.f, PhysicsAsset->BodySetup.Num(), NumConsiderForBounds);
		}
#endif // !FINAL_RELEASE
	}
	// Use ParentAnimComponent's PhysicsAsset, if we don't have one and it does
	else if( ParentAnimComponent && ParentAnimComponent->PhysicsAsset && bCanUsePhysicsAsset )
	{
#if !FINAL_RELEASE
		const DOUBLE Start = appSeconds();
#endif
		// if we can just use our parent's bounds do so
		if( bUseBoundsFromParentAnimComponent == TRUE )
		{
			Bounds = ParentAnimComponent->Bounds;
		}
		// otherwise we need to calculate 
		else
		{
			Bounds = FBoxSphereBounds(ParentAnimComponent->PhysicsAsset->CalcAABB(this));
		}

#if !FINAL_RELEASE
		if(GShouldLogOutAFrameOfPhysAssetBoundsUpdate)
		{
			const DOUBLE Time = appSeconds() - Start;
			debugf(TEXT("PARENT_PA_BOUNDS: %s %s %s %f ms"), *GetPathName(), *GetDetailedInfo(), *PhysicsAsset->GetName(), Time * 1000.f );
		}
#endif // !FINAL_RELEASE
	}
	// Fallback is to use the one from the skeletal mesh. Usually pretty bad in terms of Accuracy of where the SkelMesh Bounds are located (i.e. usually bigger than it needs to be)
	else if( SkeletalMesh )
	{
#if !FINAL_RELEASE
		const DOUBLE Start = appSeconds();
#endif
		FBoxSphereBounds RootAdjustedBounds = SkeletalMesh->Bounds;
		RootAdjustedBounds.Origin += RootBoneTranslation; // Adjust bounds by root bone translation
		Bounds = RootAdjustedBounds.TransformBy(LocalToWorld);
#if !FINAL_RELEASE
		if(GShouldLogOutAFrameOfPhysAssetBoundsUpdate)
		{
			const DOUBLE Time = appSeconds() - Start;
			debugf(TEXT("SkelMesh_Fixed_BOUNDS: %s %s %s %f ms"), *GetPathName(), *GetDetailedInfo(), *PhysicsAsset->GetName(), Time * 1000.f);
		}
#endif // !FINAL_RELEASE
	}
	else
	{
		Super::UpdateBounds();
		return;
	}

	// Add bounds of any per-poly collision data.
	if(SkeletalMesh && SpaceBases.Num() > 0)
	{
#if !FINAL_RELEASE
		const DOUBLE Start = appSeconds();
#endif
		check(SkeletalMesh->PerPolyCollisionBones.Num() == SkeletalMesh->PerPolyBoneKDOPs.Num());
		for(INT i=0; i<SkeletalMesh->PerPolyBoneKDOPs.Num(); i++)
		{
			FPerPolyBoneCollisionData& Data = SkeletalMesh->PerPolyBoneKDOPs(i);
			INT BoneIndex = SkeletalMesh->MatchRefBone(SkeletalMesh->PerPolyCollisionBones(i));
			FBox PerPolyBoneBox;
			if(BoneIndex != INDEX_NONE && Data.KDOPTree.GetRootBound(PerPolyBoneBox))
			{
				// Get bone matrix and check its not zero
				FMatrix BoneMatrix = GetBoneMatrix(BoneIndex);
				if(Abs(BoneMatrix.RotDeterminant()) > (FLOAT)KINDA_SMALL_NUMBER)
				{
					Bounds = Bounds + PerPolyBoneBox.TransformBy(GetBoneMatrix(BoneIndex));
				}
			}
		}
#if !FINAL_RELEASE
		if(GShouldLogOutAFrameOfPhysAssetBoundsUpdate && SkeletalMesh->PerPolyBoneKDOPs.Num() > 0 )
		{
			const DOUBLE Time = appSeconds() - Start;
			debugf(TEXT("PerPoly BOUNDS: %s %s %f ms"), *GetPathName(), *GetDetailedInfo(), *PhysicsAsset->GetName(), Time * 1000.f);
		}
#endif
	}

	Bounds.BoxExtent *= BoundsScale;
	Bounds.SphereRadius *= BoundsScale;

#if !FINAL_RELEASE
	const DOUBLE Start = appSeconds();
#endif
	UpdateClothBounds();
#if !FINAL_RELEASE
	if(GShouldLogOutAFrameOfPhysAssetBoundsUpdate && (ClothSim != NULL) )
	{
		const DOUBLE Time = appSeconds() - Start;
		debugf(TEXT("Cloth BOUNDS: %s %s %s %f ms"), *GetPathName(), *GetDetailedInfo(), *PhysicsAsset->GetName(), Time * 1000.f);
	}
#endif
#if WITH_APEX_CLOTHING
	UpdateApexClothingBounds();
#endif
}

FMatrix USkeletalMeshComponent::GetBoneMatrix(DWORD BoneIdx) const
{
	// Handle case of use a ParentAnimComponent - get bone matrix from there.
	if(ParentAnimComponent)
	{
		if(BoneIdx < (DWORD)ParentBoneMap.Num())
		{
			INT ParentBoneIndex = ParentBoneMap(BoneIdx);

			// If ParentBoneIndex is valid, grab matrix from ParentAnimComponent.
			if(	ParentBoneIndex != INDEX_NONE && 
				ParentBoneIndex < ParentAnimComponent->SpaceBases.Num())
			{
				return ParentAnimComponent->SpaceBases(ParentBoneIndex).ToMatrix() * LocalToWorld;
			}
			else
			{
#if !PS3 // will be caught on PC, hopefully
				debugf(NAME_Warning, TEXT("GetBoneMatrix : ParentBoneIndex(%d) out of range of ParentAnimComponent->SpaceBases for %s owned by %s (%s)"), BoneIdx, *GetName(), *Owner->GetName(), *SkeletalMesh->GetFName().ToString());
#endif
				return FMatrix::Identity;
			}
		}
		else
		{
#if !PS3 // will be caught on PC, hopefully
			debugf( NAME_Warning, TEXT("GetBoneMatrix : BoneIndex(%d) out of range of ParentBoneMap for %s (%s)"), BoneIdx, *this->GetFName().ToString(), *SkeletalMesh->GetFName().ToString() );
#endif
			return FMatrix::Identity;
		}
	}
	else
	{
		if( SpaceBases.Num() && BoneIdx < (DWORD)SpaceBases.Num() )
		{
			return SpaceBases(BoneIdx).ToMatrix() * LocalToWorld;
		}
		else
		{
#if !PS3 // will be caught on PC, hopefully
			debugf( NAME_Warning, TEXT("GetBoneMatrix : BoneIndex(%d) out of range of SpaceBases for %s (%s) owned by %s"), BoneIdx, *this->GetFName().ToString(), this->SkeletalMesh?*this->SkeletalMesh->GetFullName():TEXT("NULL"), this->Owner?*this->Owner->GetFName().ToString():TEXT("NULL") );
#endif
			return FMatrix::Identity;
		}
	}
}

void USkeletalMeshComponent::execGetBoneMatrix( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(BoneIdx);
	P_FINISH;

	*(FMatrix*)Result = GetBoneMatrix(BoneIdx);
}

FBoneAtom USkeletalMeshComponent::GetBoneAtom(DWORD BoneIdx) const
{
	// Handle case of use a ParentAnimComponent - get bone matrix from there.
	if(ParentAnimComponent)
	{
		if(BoneIdx < (DWORD)ParentBoneMap.Num())
		{
			INT ParentBoneIndex = ParentBoneMap(BoneIdx);

			// If ParentBoneIndex is valid, grab matrix from ParentAnimComponent.
			if(	ParentBoneIndex != INDEX_NONE && 
				ParentBoneIndex < ParentAnimComponent->SpaceBases.Num())
			{
#if DO_GUARD_SLOWISH && !FINAL_RELEASE
				//ensureMsgf(LocalToWorldBoneAtom==GetLocalToWorldBoneAtom(), TEXT("%s LocalToWorldBoneAtom is stale."), *SkeletalMesh->GetName());

				// warning for non uniform scaling
				FVector ScaleVector = LocalToWorld.GetScaleVector();
				if ( ScaleVector.IsUniform() == FALSE )
				{
					debugf(NAME_Warning, TEXT("LocalToWorld contains non uniform scale %s (%s) (Scale %s)"), *GetName(), *GetDetailedInfo(), *ScaleVector.ToString());
				}
#endif
				return ParentAnimComponent->SpaceBases(ParentBoneIndex) * LocalToWorldBoneAtom;
			}
			else
			{
#if !PS3 // will be caught on PC, hopefully
				debugf( NAME_Warning, TEXT("GetBoneAtom : ParentBoneIndex(%d) out of range of ParentAnimComponent->SpaceBases for %s"), BoneIdx, *this->GetFName().ToString() );
#endif
				return FBoneAtom::Identity;
			}
		}
		else
		{
#if !PS3 // will be caught on PC, hopefully
			debugf( NAME_Warning, TEXT("GetBoneAtom : BoneIndex(%d) out of range of ParentBoneMap for %s"), BoneIdx, *this->GetFName().ToString() );
#endif
			return FBoneAtom::Identity;
		}
	}
	else
	{
		if( SpaceBases.Num() && BoneIdx < (DWORD)SpaceBases.Num() )
		{
#if DO_GUARD_SLOWISH && !FINAL_RELEASE
			//ensureMsgf(LocalToWorldBoneAtom==GetLocalToWorldBoneAtom(), TEXT("%s LocalToWorldBoneAtom is stale."), *SkeletalMesh->GetName());

			FVector ScaleVector = LocalToWorld.GetScaleVector();
			if ( ScaleVector.IsUniform() == FALSE )
			{
				debugf(NAME_Warning, TEXT("LocalToWorld contains non uniform scale %s (%s) (Scale %s)"), *GetName(), *GetDetailedInfo(), *ScaleVector.ToString());
			}
#endif
			return SpaceBases(BoneIdx) * LocalToWorldBoneAtom;
		}
		else
		{
#if !PS3 // will be caught on PC, hopefully
			debugf( NAME_Warning, TEXT("GetBoneMatrix : BoneIndex(%d) out of range of SpaceBases for %s (%s) owned by %s"), BoneIdx, *this->GetFName().ToString(), this->SkeletalMesh?*this->SkeletalMesh->GetFullName():TEXT("NULL"), this->Owner?*this->Owner->GetFName().ToString():TEXT("NULL") );
#endif
			return FBoneAtom::Identity;
		}
	}
}

/**
 * Find the index of bone by name. Looks in the current SkeletalMesh being used by this SkeletalMeshComponent.
 * 
 * @param BoneName Name of bone to look up
 * 
 * @return Index of the named bone in the current SkeletalMesh. Will return INDEX_NONE if bone not found.
 *
 * @see USkeletalMesh::MatchRefBone.
 */
INT USkeletalMeshComponent::MatchRefBone( FName BoneName) const
{
	INT BoneIndex = INDEX_NONE;
	if ( BoneName != NAME_None && SkeletalMesh )
	{
		BoneIndex = SkeletalMesh->MatchRefBone( BoneName );
	}

	return BoneIndex;
}

void USkeletalMeshComponent::execMatchRefBone( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(BoneName);
	P_FINISH;

	*(INT*)Result = MatchRefBone(BoneName);
}

void USkeletalMeshComponent::execGetBoneName(FFrame& Stack, RESULT_DECL)
{
	P_GET_INT(BoneIndex);
	P_FINISH;

	*(FName*)Result = (SkeletalMesh != NULL && SkeletalMesh->RefSkeleton.IsValidIndex(BoneIndex)) ? SkeletalMesh->RefSkeleton(BoneIndex).Name : NAME_None;
}

FName USkeletalMeshComponent::GetParentBone( FName BoneName ) const
{
	FName Result = NAME_None;

	INT BoneIndex = MatchRefBone(BoneName);
	if ((BoneIndex != INDEX_NONE) && (BoneIndex > 0)) // This checks that this bone is not the root (ie no parent), and that BoneIndex != INDEX_NONE (ie bone name was found)
	{
		Result = SkeletalMesh->RefSkeleton(SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex).Name;
	}
	return Result;
}

void USkeletalMeshComponent::execGetParentBone(FFrame& Stack, RESULT_DECL)
{
	P_GET_NAME(BoneName);
	P_FINISH;

	*(FName*)Result = GetParentBone(BoneName);
}

void USkeletalMeshComponent::execGetBoneNames(FFrame& Stack, RESULT_DECL)
{
	P_GET_TARRAY_REF(FName, BoneNames);
	P_FINISH;

	if (SkeletalMesh == NULL)
	{
		// no mesh, so no bones
		BoneNames.Empty();
	}
	else
	{
		// pre-size the array to avoid unnecessary reallocation
		BoneNames.Empty(SkeletalMesh->RefSkeleton.Num());
		BoneNames.Add(SkeletalMesh->RefSkeleton.Num());
		for (INT i = 0; i < SkeletalMesh->RefSkeleton.Num(); i++)
		{
			BoneNames(i) = SkeletalMesh->RefSkeleton(i).Name;
		}
	}
}

/** 
 * Tests if BoneName is child of (or equal to) ParentBoneName. 
 * Note - will return FALSE if ChildBoneIndex is the same as ParentBoneIndex ie. must be strictly a child.
 */
void USkeletalMeshComponent::execBoneIsChildOf(FFrame& Stack, RESULT_DECL)
{
	P_GET_NAME(BoneName);
	P_GET_NAME(ParentBoneName);
	P_FINISH;

	*(UBOOL*)Result = FALSE;

	if(SkeletalMesh)
	{
		INT BoneIndex = SkeletalMesh->MatchRefBone(BoneName);
		if(BoneIndex == INDEX_NONE)
		{
			debugf(TEXT("execBoneIsChildOf: BoneName '%s' not found in SkeletalMesh '%s'"), *BoneName.ToString(), *SkeletalMesh->GetName());
			return;
		}

		INT ParentBoneIndex = SkeletalMesh->MatchRefBone(ParentBoneName);
		if(ParentBoneIndex == INDEX_NONE)
		{
			debugf(TEXT("execBoneIsChildOf: ParentBoneName '%s' not found in SkeletalMesh '%s'"), *ParentBoneName.ToString(), *SkeletalMesh->GetName());
			return;
		}

		*(UBOOL*)Result = SkeletalMesh->BoneIsChildOf(BoneIndex, ParentBoneIndex);
	}
}

/** Gets the local-space position of a bone in the reference pose. */
void USkeletalMeshComponent::execGetRefPosePosition(FFrame& Stack, RESULT_DECL)
{
	P_GET_INT(BoneIndex);
	P_FINISH;

	if(SkeletalMesh && (BoneIndex >= 0) && (BoneIndex < SkeletalMesh->RefSkeleton.Num()))
	{
		*(FVector*)Result = SkeletalMesh->RefSkeleton(BoneIndex).BonePos.Position;
	}
	else
	{
		*(FVector*)Result = FVector(0,0,0);
	}
}

/**
 *	Sets the SkeletalMesh used by this SkeletalMeshComponent.
 *	Will also update ParentBoneMap, if we are using a ParentAnimComponent. 
 *	NB: If other things are using THIS component as a ParenAnimComponent, you must manually call UpdateParentBoneMap on them after making this call.
 * 
 * @param InSkelMesh		New SkeletalMesh to use for SkeletalMeshComponent.
 * @param bKeepSpaceBases	If true, when changing the skeletal mesh, keep the SpaceBases array around.
 * @param InbAlwaysUseInstanceWeights If TRUE, always use instanced vertex influences for this mesh
 */
void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkelMesh, UBOOL bKeepSpaceBases)
{
	SCOPE_CYCLE_COUNTER(STAT_SkelMesh_SetMeshTime);
	// NOTE: InSkelMesh may be NULL (useful in the editor for removing the skeletal mesh associated with
	//   this component on-the-fly)

	if (InSkelMesh == SkeletalMesh)
	{
		// do nothing if the input mesh is the same mesh we're already using.
		return;
	}

#if _XBOX && TRACE_SETSKELMESH
	XTraceSetBufferSize( 40 * 1024 * 1024 );
	FString FileName = FString::Printf(TEXT("GAME:\\trace-setskelmesh-%s.bin"), *appSystemTimeString());
	FString MeshName = ( InSkelMesh != NULL ) ? InSkelMesh->GetName() : TEXT( "Unknown" );
	debugf(TEXT("SetSkeletalMesh: %s -> %s --- %s"), *GetPathName(), *MeshName, *FileName);
	XTraceStartRecording( TCHAR_TO_ANSI(*FileName) );
#endif

	USkeletalMesh* OldSkelMesh = SkeletalMesh;

	// If desired - back up SpaceBases array now
	TArray<FBoneAtom> OldSpaceBases;
	if( bKeepSpaceBases && SkeletalMesh)
	{
		OldSpaceBases = SpaceBases;
	}

#if WITH_FACEFX
	// Wipe out our existing FaceFX instance if we have one, since our skeletal mesh is changing.  It will be
	// created again in the reattachment phase below (FComponentReattachContext)
	if( FaceFXActorInstance != NULL )
	{
		delete FaceFXActorInstance;
		FaceFXActorInstance = NULL;
	}
#endif //WITH_FACEFX

	{
		FMatrix OldSkelMatrix;
		if( SkeletalMesh )
		{
			OldSkelMatrix = ( bForceRawOffset ?  FMatrix::Identity : FTranslationMatrix( SkeletalMesh->Origin ) ) * FRotationMatrix(SkeletalMesh->RotOrigin);
		}
		else
		{
			OldSkelMatrix = FMatrix::Identity;
		}

		// This will force InitAnimTree() to be called when the Component is reattached.
		// We need to do this so nodes can recache the new SkelMesh bone indices if needed.
		bAnimTreeInitialised = FALSE;

		// Use recreate context to update the scene proxy for the new SkeletalMesh
		{
			FComponentReattachContext	ReattachContext(this);

			// NOTE: InSkelMesh may be NULL!
			SkeletalMesh = InSkelMesh;

			// Reset the animation stuff when changing mesh.
			SpaceBases.Empty();	

			// Either turn off or remap saved pose if we are using one.
			if(Animations)
			{
				UAnimTree* Tree = Animations->GetAnimTree();
				if(Tree)
				{
					// If we want to keep space bases, and we have a pose to use, and we are changing to a good mesh
					if(bKeepSpaceBases && (Tree->SavedPose.Num() == OldSkelMesh->RefSkeleton.Num()) && SkeletalMesh)
					{
						// Back up old pose
						TArray<FBoneAtom> OldSavedPose = Tree->SavedPose;

						// Then reset space for updated pose
						Tree->SavedPose.Empty();
						Tree->SavedPose.Add(SkeletalMesh->RefSkeleton.Num());

						// Then iterate over each bone of new mesh, looking for pose from old mesh
						for(INT i=0; i<SkeletalMesh->RefSkeleton.Num(); i++)
						{
							// Use names to match up
							FName BoneName = SkeletalMesh->RefSkeleton(i).Name;
							INT OldBoneIndex = OldSkelMesh->MatchRefBone(BoneName);

							// If it exists - just copy over
							if(OldBoneIndex != INDEX_NONE)
							{
								Tree->SavedPose(i) = OldSavedPose(OldBoneIndex);
							}
							// If it doesn't, use ref pose
							else
							{
								Tree->SavedPose(i).SetComponents(
									SkeletalMesh->RefSkeleton(i).BonePos.Orientation,
									SkeletalMesh->RefSkeleton(i).BonePos.Position);
							}
						}
					}
					// If we could not update the pose, stop using the saved pose
					else
					{
						Tree->SetUseSavedPose(FALSE);
					}
				}
			}

			// We want to just replace the bit of the transform due to the actual skeletal mesh.
			FMatrix OldOffset = OldSkelMatrix.Inverse() * LocalToWorld;

			if( SkeletalMesh )
			{
				LocalToWorld = ( bForceRawOffset ? FMatrix::Identity : FTranslationMatrix(SkeletalMesh->Origin) )  * FRotationMatrix(SkeletalMesh->RotOrigin) * OldOffset;
			}
			else
			{
				LocalToWorld = OldOffset;
			}

			// If this component refers to some parent, make sure it is up to date.
			// No way to tell if other things refer to this - that has to be done manually by user unfortuntalely.
			UpdateParentBoneMap();

			// Update bHasValidBodies flag
			UpdateHasValidBodies();

			// Indicate that 'required bones' array will need to be recalculated.
			bRequiredBonesUpToDate = FALSE;
		}
	}

	// If we backed up the SpaceBases array - find bones by name that are in both meshes and copy matrix.
	// TODO: This leaves any _new_ bones in their animated pose - is that bad? What else should we do?
	if( OldSpaceBases.Num() > 0 && SpaceBases.Num() > 0 )
	{
		check(OldSpaceBases.Num() == OldSkelMesh->RefSkeleton.Num());
		check(SpaceBases.Num() == SkeletalMesh->RefSkeleton.Num());

		for(INT i=0; i<OldSkelMesh->RefSkeleton.Num(); i++)
		{
			FName BoneName = OldSkelMesh->RefSkeleton(i).Name;
			INT BoneIndex = SkeletalMesh->MatchRefBone(BoneName);
			if(BoneIndex != INDEX_NONE)
			{
				SpaceBases(BoneIndex) = OldSpaceBases(i);
			}
		}
	}

	InitMorphTargets();
	InitLODInfos();

	// Notify the streaming system. Don't use Update(), because this may be the first time the mesh has been set
	// and the component may have to be added to the streaming system for the first time.
	GStreamingManager->NotifyPrimitiveAttached( this, DPT_Spawned );

#if _XBOX && TRACE_SETSKELMESH
	XTraceStopRecording();
#endif
}


/**
 *	Sets the PhysicsAsset used by this SkeletalMeshComponent.
 * 
 * @param InPhysicsAsset	New PhysicsAsset to use for SkeletalMeshComponent.
 * @param bForceReInit		Force asset to be re-initialised.
 */
void USkeletalMeshComponent::SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset, UBOOL bForceReInit)
{
#if _XBOX && TRACE_SETPHYSASSET
	XTraceSetBufferSize( 40 * 1024 * 1024 );
	FString FileName = FString::Printf(TEXT("GAME:\\trace-setphys-%s.bin"), *appSystemTimeString());
	debugf(TEXT("SetPhysicsAsset: %s -> %s --- %s"), *GetPathName(), *InPhysicsAsset->GetName(), *FileName);
	XTraceStartRecording( TCHAR_TO_ANSI(*FileName) );
#endif
	SCOPE_CYCLE_COUNTER(STAT_SkelMesh_SetPhysAssetTime);
	// If this is different from what we have now, or we should have an instance but for whatever reason it failed last time, teardown/recreate now.
	if(bForceReInit || (InPhysicsAsset != PhysicsAsset) || (bHasPhysicsAssetInstance && !PhysicsAssetInstance) )
	{
		// SkelComp had a physics instance, then terminate it.
		if( bHasPhysicsAssetInstance )
		{
			TermArticulated(NULL);

			{
				// Need to update scene proxy, because it keeps a ref to the PhysicsAsset.
				FPrimitiveSceneAttachmentContext ReattachContext(this);
				PhysicsAsset = InPhysicsAsset;
			}

			// Update bHasValidBodies flag
			UpdateHasValidBodies();

			// Component should be re-attached here, so create physics.
			if( PhysicsAsset && SkeletalMesh )
			{
				// Because we don't know what bones the new PhysicsAsset might want, we have to force an update to _all_ bones in the skeleton.
				RequiredBones.Reset(SkeletalMesh->RefSkeleton.Num());
				RequiredBones.Add( SkeletalMesh->RefSkeleton.Num() );
				for(INT i=0; i<SkeletalMesh->RefSkeleton.Num(); i++)
				{
					RequiredBones(i) = (BYTE)i;
				}

				CachedLocalAtoms.Reset();
				CachedSpaceBases.Reset();

				UpdateSkelPose();

				// Initialize new Physics Asset
				InitArticulated(bSkelCompFixed);
			}
		}
		else
		{
			// If PhysicsAsset hasn't been instanced yet, just update the template.
			PhysicsAsset = InPhysicsAsset;

			// Update bHasValidBodies flag
			UpdateHasValidBodies();
		}

		// Indicate that 'required bones' array will need to be recalculated.
		bRequiredBonesUpToDate = FALSE;
	}

#if _XBOX && TRACE_SETPHYSASSET
	XTraceStopRecording();
#endif
}

/** Change whether to force mesh into ref pose (and use cheaper vertex shader) */
void USkeletalMeshComponent::SetForceRefPose(UBOOL bNewForceRefPose)
{
	bForceRefpose = bNewForceRefPose;
	BeginDeferredReattach();
}

/** Turn on and off cloth simulation for this skeletal mesh. */
void USkeletalMeshComponent::SetEnableClothSimulation(UBOOL bInEnable)
{
	if(!ClothSim && bInEnable)
	{
		FRBPhysScene* UseScene = GWorld->RBPhysScene;
		InitClothSim(UseScene);
	}
	else if(ClothSim && !bInEnable)
	{
		TermClothSim(NULL);
	}

	bEnableClothSimulation = bInEnable;
}

void USkeletalMeshComponent::execSetEnableClothSimulation( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(bInEnable);
	P_FINISH;

	SetEnableClothSimulation(bInEnable);
}
void USkeletalMeshComponent::SetMaterial(INT ElementIndex, UMaterialInterface* InMaterial)
{
	Super::SetMaterial(ElementIndex, InMaterial);
#if WITH_APEX
	FIApexClothing *Clothing = GetApexClothing();
	if ( Clothing != NULL )
	{
		Clothing->SetMaterial(ElementIndex, InMaterial);
	}
#endif

}

IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execSetMaterial);

void USkeletalMeshComponent::execSetMaterial( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(ElementIndex);
	P_GET_OBJECT(UMaterialInterface, Material);
	P_FINISH;

	SetMaterial(ElementIndex, Material);
}
/** Change the ParentAnimComponent that this component is using as the source for its bone transforms. */
void USkeletalMeshComponent::SetParentAnimComponent(USkeletalMeshComponent* NewParentAnimComp)
{
	ParentAnimComponent = NewParentAnimComp;

	UpdateParentBoneMap();
}

/**
 *	Sets the value of the bForceWireframe flag and reattaches the component as necessary.
 *
 *	@param	InForceWireframe		New value of bForceWireframe.
 */
void USkeletalMeshComponent::SetForceWireframe(UBOOL InForceWireframe)
{
	if(bForceWireframe != InForceWireframe)
	{
		bForceWireframe = InForceWireframe;
		FComponentReattachContext ReattachContext(this);
	}
}

/**
 * Find a named AnimSequence from the AnimSets array in the SkeletalMeshComponent. 
 * This searches array from end to start, so specific sequence can be replaced by putting a set containing a sequence with the same name later in the array.
 * 
 * @param AnimSeqName Name of AnimSequence to look for.
 * 
 * @return Pointer to found AnimSequence. Returns NULL if could not find sequence with that name.
 */
UAnimSequence* USkeletalMeshComponent::FindAnimSequence(FName AnimSeqName) const
{
	if( AnimSeqName == NAME_None )
	{
		return NULL;
	}

	// Work from last element in list backwards, so you can replace a specific sequence by adding a set later in the array.
	for(INT i=AnimSets.Num()-1; i>=0; i--)
	{
		if( AnimSets(i) )
		{
			UAnimSequence* FoundSeq = AnimSets(i)->FindAnimSequence(AnimSeqName);
			if( FoundSeq )
			{
#if 0 // DEBUG
				debugf(TEXT("Found %s in AnimSet %s"), *AnimSeqName, AnimSets(i)->GetName());
#endif
				return FoundSeq;
			}
		}
	}

	return NULL;
}

/** 
* Add Curve Keys to ActiveMorph Sets 
*/
void USkeletalMeshComponent::ApplyCurveKeys(FCurveKeyArray& CurveKeys)
{
	TArray<FActiveMorph>	NewActiveCurveMorphs;
	
	if ( CurveKeys.Num() > 0 )
	{
		// Purge all the curves that has 0 weight
		// I need to do this first before checking name
		for (INT I=0; I<CurveKeys.Num(); ++I)
		{
			if (CurveKeys(I).Weight <= MinMorphBlendWeight)
			{
				CurveKeys.Remove(I, 1);
				--I;
			}
		}

		// see if there is multiple weights for the same target 
		// then later one overwrites it
		for (INT I=0; I<CurveKeys.Num(); ++I)
		{
			for (INT J=I+1; J<CurveKeys.Num(); ++J)
			{
				// if same target is found, print erorr message, and delete old one
				if ( CurveKeys(I).CurveName == CurveKeys(J).CurveName )
				{
#if !FINAL_RELEASE
					// if same target
					if (CurveKeys(I).Weight != CurveKeys(J).Weight)
					{
						debugf(TEXT("ERROR: Same curve with different weight. (%s : new[%0.2f], old[%0.2f]) Overwriting with new..."), *CurveKeys(I).CurveName.GetNameString(), CurveKeys(I).Weight, CurveKeys(J).Weight);
					}
#endif
					CurveKeys.Remove(I, 1);
					--I;
					--J;
					break;
				}
			}
		}

		// now add to active morphs
		for (INT I=0; I<CurveKeys.Num(); ++I)
		{
			UMorphTarget * Target = FindMorphTarget(CurveKeys(I).CurveName);
			if (Target)
			{
//				debugf(TEXT("NewActiveCurveMorphs: Target (%s), Weight (%0.5f)"), *CurveKeys(I).CurveName.GetNameString(), CurveKeys(I).Weight);
				NewActiveCurveMorphs.AddItem(FActiveMorph(Target, CurveKeys(I).Weight));
			}
		}
	}

	if (ActiveCurveMorphs.Num())
	{
		for (INT I=0; I<ActiveCurveMorphs.Num(); ++I)
		{
			if ( !NewActiveCurveMorphs.ContainsItem(ActiveCurveMorphs(I)) )
			{
				if (ActiveCurveMorphs(I).Target)
				{
					// clear material parameter
					UpdateMorphTargetMaterial( ActiveCurveMorphs(I).Target, 0.f );
				}
			}
		}
	}

	ActiveCurveMorphs = NewActiveCurveMorphs;
}
/**
 * Find a named MorphTarget from the MorphSets array in the SkeletalMeshComponent.
 * This searches the array in the same way as FindAnimSequence
 *
 * @param AnimSeqName Name of MorphTarget to look for.
 *
 * @return Pointer to found MorphTarget. Returns NULL if could not find target with that name.
 */
UMorphTarget* USkeletalMeshComponent::FindMorphTarget( FName MorphTargetName )
{
	if(MorphTargetName == NAME_None)
	{
		return NULL;
	}

	return MorphTargetIndexMap.FindRef(MorphTargetName);
}

/**
*	Initialize MorphSets look up table : MorphTargetIndexMap
*/
void USkeletalMeshComponent::InitMorphTargets()
{
	MorphTargetIndexMap.Empty();

	// Work from last element in list backwards, so you can replace a specific target by adding a set later in the array.
	for(INT I=MorphSets.Num()-1; I>=0; --I)
	{
		UMorphTargetSet* MorphSet = MorphSets(I);
		if( MorphSet )
		{
			if ( MorphSet->BaseSkelMesh == NULL )
			{
				if ( GIsEditor && SkeletalMesh )
				{
					if (appMsgf(AMT_YesNo, TEXT("MorphTargetSet (%s) does not have BaseSkelMesh yet. Would you like to set this up to (%s)"), *MorphSet->GetName(), *SkeletalMesh->GetName()) > 0)
					{
						MorphSet->BaseSkelMesh = SkeletalMesh;
					}
				}
			}

			// if BaseSkelMesh isn't set, just let it cache. It hasn't been set up correctly yet
			if ( MorphSet->BaseSkelMesh == SkeletalMesh || MorphSet->BaseSkelMesh == NULL )
			{
				for (INT J=0; J<MorphSet->Targets.Num(); ++J)
				{
					UMorphTarget* Target = MorphSet->Targets(J);

					if( Target )
					{
						// make sure the vert counts in the LOD match
						if ( SkeletalMesh )
						{
							for ( INT K=0; K<Target->MorphLODModels.Num(); ++K)
							{
								// if this morph model index is outside the LOD range, or has more verts that the LOD at the same index, it can't be used
								FMorphTargetLODModel& TargetLODModel = Target->MorphLODModels(K);
								if ( !SkeletalMesh->LODModels.IsValidIndex(K) || SkeletalMesh->LODModels(K).NumVertices < (UINT)TargetLODModel.NumBaseMeshVerts )
								{
									// If it's not the editor, just remove
									if ( !GIsEditor )
									{
										// Remove the LOD, this will mark the asset as dirty but won't be saved
										Target->MorphLODModels.Remove( K-- );
									}
									// If it's not PIE, and we haven't already, ask if it should be removed
									else if ( !GIsPlayInEditorWorld && !MorphTargetsQueried.ContainsItem( Target->GetFName() ) )
									{
										if ( appMsgf(AMT_YesNo, TEXT("MorphTargetSet (%s) MorphTarget (%s) had invalid LOD (%d) that's not compatible with mesh (%s) for (%s).\nDelete LOD? Not doing so will result in visible corruption at that LOD level."), 
											*MorphSet->GetName(), *Target->GetName(), K, *SkeletalMesh->GetName(), *GetName()) )
										{
											// Remove the LOD, this will mark the asset as dirty and could be saved by the user
											Target->MorphLODModels.Remove( K-- );
										}
										else
										{
											// Don't ask again for this Target (prevent excess spam - still some due to the way morph targets are initialized all the time)
											MorphTargetsQueried.AddUniqueItem( Target->GetFName() );
										}
									}
								}
							}
						}

						// if I don't find it, then overwrite
						// to keep the way the previous version works
						// when it checks from last to first and if found
						// it returns, I need to not overwrite the version 
						// if exists
						FName const TargetName = Target->GetFName();
						if( MorphTargetIndexMap.Find(TargetName) == NULL )
						{ 
							MorphTargetIndexMap.Set(TargetName, Target);
						}
					}
				}
			}
			else
			{
				// have extra morph target sets that doesn't work with skeletalmesh
				warnf(TEXT("MorphTargetSet (%s) is added to the mesh (%s) that's not compatible. BaseSkelMesh has to be (%s)"), 
					*MorphSet->GetName(), (SkeletalMesh)?*SkeletalMesh->GetName():TEXT("None"), (MorphSet->BaseSkelMesh)?*MorphSet->BaseSkelMesh->GetName():TEXT("None") );
			}
		}
	}
}

/**
* Update Material Mapping if exists, and update scalara paramter value
* This feature is almost identical to the MorphNodeWeightByBoneAngle
* @param MorphTarget kelComponent - SkelComponent to update material for
* @param Weight - Weight of Scalar Parameter
*/
void USkeletalMeshComponent::UpdateMorphTargetMaterial(const UMorphTarget* MorphTarget, const FLOAT Weight)
{
	if (MorphTarget->ScalarParameterName!=NAME_None)
	{
		UMaterialInterface* MaterialInterface = GetMaterial(MorphTarget->MaterialSlotId);
		UMaterialInstanceConstant * MaterialInstanceConstant = NULL;

		// See if we need to update the MaterialInstanceConstant reference
		if( MaterialInterface && MaterialInterface->IsA(UMaterialInstanceConstant::StaticClass()) )
		{
			MaterialInstanceConstant = Cast<UMaterialInstanceConstant>(MaterialInterface);
		}

		if( !MaterialInstanceConstant && SkeletalMesh )
		{
			if( MorphTarget->MaterialSlotId < SkeletalMesh->Materials.Num() && SkeletalMesh->Materials(MorphTarget->MaterialSlotId) )
			{
				if( bDisableFaceFXMaterialInstanceCreation )
				{
					debugf(TEXT("UGearMorph_WeightByBoneAngle: WARNING Unable to create MaterialInstanceConstant because bDisableFaceFXMaterialInstanceCreation is true!"));
				}
				else
				{
					UMaterialInstanceConstant* NewMaterialInstanceConstant = CastChecked<UMaterialInstanceConstant>( UObject::StaticConstructObject(UMaterialInstanceConstant::StaticClass(), this) );
					NewMaterialInstanceConstant->SetParent(SkeletalMesh->Materials(MorphTarget->MaterialSlotId));
					SetMaterial(MorphTarget->MaterialSlotId, NewMaterialInstanceConstant);
					MaterialInstanceConstant = NewMaterialInstanceConstant;
				}
			}
		}

		// Set Scalar parameter value
		if( MaterialInstanceConstant )
		{
			MaterialInstanceConstant->SetScalarParameterValue(MorphTarget->ScalarParameterName, Weight);
		}
	}
}
/**
 *	Set the AnimTree, residing in a package, to use as the template for this SkeletalMeshComponent.
 *	The AnimTree that is passed in is copied and assigned to the Animations pointer in the SkeletalMeshComponent.
 *	NB. This will destroy the currently instanced AnimTree, so it important you don't have any references to it or nodes within it!
 */
void USkeletalMeshComponent::SetAnimTreeTemplate(UAnimTree* NewTemplate)
{
	// If there is a tree instanced at the moment - destroy it.
	DeleteAnimTree();
	checkSlow(Animations == NULL);

	if (NewTemplate != NULL)
	{
		// Copy template and assign to Animations pointer.
		if (NewTemplate->bEnablePooling)
		{	
			// If we're pooling, set owner to world instead of self
			Animations = NewTemplate->CopyAnimTree(GWorld, TRUE);
		}
		else
		{
			Animations = NewTemplate->CopyAnimTree(this);
		}

		if( Animations != NULL )
		{
			// Remember the new template
			AnimTreeTemplate = NewTemplate;
			// If successful, initialize the new tree.
			InitAnimTree(TRUE);
		}
		else
		{
			debugf(NAME_DevAnim,TEXT("Failed to instance AnimTree Template: %s"), *NewTemplate->GetName() );
			AnimTreeTemplate = NULL;
		}
	}
	else
	{
		AnimTreeTemplate = NULL;
	}

	// Let our own know that we've updated our AnimTree, if it needs to update any node caching information
	AActor* Owner = GetOwner();
	if( Owner )
	{
		Owner->eventAnimTreeUpdated(this);
	}
}

void USkeletalMeshComponent::UpdateHasValidBodies()
{
	// First clear out old data
	bHasValidBodies = FALSE;

	// If we have a physics asset..
	if(PhysicsAsset != NULL)
	{
		// For each body in physics asset..
		for( INT BodyIndex = 0; BodyIndex < PhysicsAsset->BodySetup.Num(); BodyIndex++ )
		{
			// .. find the matching graphics bone index
			INT BoneIndex = MatchRefBone( PhysicsAsset->BodySetup( BodyIndex )->BoneName );

			// If we found a valid graphics bone, set the 'valid' flag
			if(BoneIndex != INDEX_NONE)
			{
				bHasValidBodies = TRUE;
				break;
			}
		}
	}
}


/** Update mapping table between. Call whenever you change this or the ParentAnimComponent skeletal mesh. */
void USkeletalMeshComponent::UpdateParentBoneMap()
{
	ParentBoneMap.Empty();

	if(SkeletalMesh && ParentAnimComponent && ParentAnimComponent->SkeletalMesh)
	{
		USkeletalMesh* ParentMesh = ParentAnimComponent->SkeletalMesh;

		ParentBoneMap.Empty( SkeletalMesh->RefSkeleton.Num() );
		ParentBoneMap.Add( SkeletalMesh->RefSkeleton.Num() );
		if (SkeletalMesh == ParentMesh)
		{
			// if the meshes are the same, the indices must match exactly so we don't need to look them up
			for (INT i = 0; i < ParentBoneMap.Num(); i++)
			{
				ParentBoneMap(i) = i;
			}
		}
		else
		{
			for(INT i=0; i<ParentBoneMap.Num(); i++)
			{
				FName BoneName = SkeletalMesh->RefSkeleton(i).Name;
				ParentBoneMap(i) = ParentMesh->MatchRefBone( BoneName );
			}
		}
	}
}

#if PERF_ENABLE_INITANIM_STATS
static FName NAME_BuildParentNodesArray = FName(TEXT("InitAnimTree_BuildParentNodesArray"));
static FName NAME_InitMorphTargets = FName(TEXT("InitAnimTree_InitMorphTargets"));
static FName NAME_InitSkelControls = FName(TEXT("InitAnimTree_InitSkelControls"));
static FName NAME_PostInitAnimTree = FName(TEXT("InitAnimTree_PostInitAnimTree"));
static FName NAME_BuildTickArray_Setup = FName(TEXT("InitAnimTree_BuildTickArray_Setup"));
static FName NAME_InitMorphNodes = FName(TEXT("InitAnimTree_InitMorphNodes"));
#endif

void USkeletalMeshComponent::InitAnimTree(UBOOL bForceReInit)
{
	// If not already initialized (or we are forcing a re-init), and we have an AnimTree
	if( (bForceReInit || !bAnimTreeInitialised) && Animations && (ParentAnimComponent == NULL || bUpdateMorphWhenParentAnimComponentExists))
	{
#if PERF_ENABLE_INITANIM_STATS
		InitAnimStats.Empty();
		InitAnimStatsTMap.Empty();

		DOUBLE Start = appSeconds();
#endif
#define CAPTURE_INITANIMTREE_PERF 0
#if CAPTURE_INITANIMTREE_PERF
		FString AnimTreeName = *AnimTreeTemplate->GetPathName();
		FString CheckName = FString::Printf(TEXT("AT_MarcusLayered"));
		debugf(TEXT("CAPTURE_INITANIMTREE_PERF AnimTreeName: %s, CheckName: %s"), *AnimTreeName, *CheckName);
		UBOOL bDoIt = AnimTreeName.InStr(CheckName) != INDEX_NONE;
		if( bDoIt == TRUE )
		{
			GCurrentTraceName = NAME_Game;
		}
		else
		{
			GCurrentTraceName = NAME_None;
		}

		appStartCPUTrace( NAME_Game, FALSE, TRUE, 40, NULL );
#endif
		// If its an AnimTree, initialize the MorphNodes.
		UAnimTree* Tree = Cast<UAnimTree>(Animations);

		// See if we need to rebuild animtree tick
		UBOOL const bRebuildAnimTickArray = (Tree) ? Tree->bRebuildAnimTickArray: FALSE;

		if ( bRebuildAnimTickArray && Tree )
		{
			TArray<UAnimNode*> OldAnimNodes;
			// go around clear all child links
			Tree->GetNodes(OldAnimNodes, TRUE);
			for ( INT I=0; I<OldAnimNodes.Num(); ++I )
			{
				// if slot node
				if ( OldAnimNodes(I)->IsA(UAnimNodeSlot::StaticClass()) )
				{
					UAnimNodeSlot * SlotNode = Cast<UAnimNodeSlot>(OldAnimNodes(I));
					if ( SlotNode )
					{
						// clear child
						for (INT I=1; I<SlotNode->Children.Num() ; ++I)
						{
							SlotNode->Children(I).Anim = NULL;
						}
					}
				}
			}

			Tree->bRebuildAnimTickArray = FALSE;
		}

		// See if our template already has built the parent node array. Otherwise we have to do it here.
		UBOOL const bParentNodeArrayBuilt = Tree ? (Tree->bParentNodeArrayBuilt && !bRebuildAnimTickArray) : FALSE;

		// Estimate number of nodes for our allocations
		INT NodeCount = AnimTickArray.Num();
		UBOOL const bAlreadyHasAnimTickArray = (GIsGame && Tree && Tree->AnimTickArray.Num() > 0 && !bRebuildAnimTickArray);

		if( bAlreadyHasAnimTickArray )
		{	
			// Copy the AnimTickArray from our Tree...
			AnimTickArray = Tree->AnimTickArray;
			NodeCount = AnimTickArray.Num();
		}

		// If Parent Node array hasn't been already built, then build it now.
		// In the editor, always rebuild it, as we maybe we adding/removing nodes.
		if( !bParentNodeArrayBuilt || (GIsEditor && !GIsGame) )
		{
			INITANIM_CUSTOM(NAME_BuildParentNodesArray);
			// Traverse Tree a first time to build the ParentNodes array in each node.
			// Hopefully we can serialize this in the future.
			UAnimNode::CurrentSearchTag++;
			Animations->BuildParentNodesArray();

			// Keep track that ParentNodes array is up to date, so we don't have to do it in-game at run time.
			if( Tree )
			{
				Tree->bParentNodeArrayBuilt = TRUE;
			}
		}

		// Trigger DeferredAnimInit call on relevant nodes
		InitTag = Animations->NodeInitTag + 1;

		// Build array in tick order. Start by adding root node and call from there.
		// Only do this in editor, or if we don't have a template to get this data from.
		if( (GIsEditor && !GIsGame) || !bAlreadyHasAnimTickArray )
		{
			INITANIM_CUSTOM(NAME_BuildTickArray_Setup)

			TickTag++;

			AnimTickArray.Empty(NodeCount);
			Animations->TickArrayIndex = AnimTickArray.AddItem(Animations);

			// Do initialization on Root Node
			Animations->SkelComponent = this;
			Animations->NodeTickTag = TickTag;
			{
				EXCLUDE_PARENT_TIME
				// Traverse the tree a second time, build tick order array, and call InitAnim on all the nodes
				Animations->BuildTickArray(AnimTickArray);
			}

			// Update our nodecount here, in case we didn't have it before.
			NodeCount = AnimTickArray.Num();

			// Back up our AnimTickArray in our Tree node, so we don't have to keep building it.
			if( Tree )
			{
				Tree->AnimTickArray = AnimTickArray;
			}
		}

		// Reset those arrays as well.
		AnimTickRelevancyArray.Empty(NodeCount);
		AnimTickRelevancyArray.AddZeroed(NodeCount);
		AnimTickWeightsArray.Empty(NodeCount);
		AnimTickWeightsArray.Add(NodeCount);

		// Call InitAnim on all of the nodes
		// This needs to be done once the ParentNodes array is setup, as some initialization code calls IsChildOf().
		{
			for(INT i=0; i<NodeCount; i++)
			{
				AnimTickArray(i)->SkelComponent = this;
				AnimTickArray(i)->NodeTickTag = TickTag;
				AnimTickArray(i)->InitAnim(this, NULL);
			}
		}

		// init morph targets
		{
			INITANIM_CUSTOM(NAME_InitMorphTargets);
			InitMorphTargets();

			if( Tree )
			{
				EXCLUDE_PARENT_TIME
				INITANIM_CUSTOM(NAME_InitMorphNodes);
				// Keep track of how many nodes we have
				Tree->InitTreeMorphNodes(this);
			}
		}
		
		// Initialize the skeletal controls on the tree.
		{
			INITANIM_CUSTOM(NAME_InitSkelControls);
			InitSkelControls();
		}

		// if there's an Owner, notify it that our AnimTree was initialized so it can cache references to controllers and such
		if( Tree != NULL && Owner != NULL )
		{
			INITANIM_CUSTOM(NAME_PostInitAnimTree);
			Owner->eventPostInitAnimTree(this);
		}

		bAnimTreeInitialised = TRUE;

#if CAPTURE_INITANIMTREE_PERF
		appStopCPUTrace( NAME_Game );
#endif
#if PERF_ENABLE_INITANIM_STATS
		debugf(TEXT("InitAnimTree: %f (%s using %s)"), (appSeconds() - Start) * 1000.f, *GetPathName(), *SkeletalMesh->GetPathName());

		// Sort results (slowest first)
		Sort<USE_COMPARE_CONSTREF(FAnimNodeTimeStat,UnSkeletalComponent)>( &InitAnimStats(0), InitAnimStats.Num() );

		debugf(TEXT(" ======= InitAnim - TIMING - %s %s"), *GetPathName(), SkeletalMesh?*SkeletalMesh->GetName():TEXT("NONE"));
		FLOAT TotalBlendTime = 0.f;
		for(INT i=0; i<InitAnimStats.Num(); i++)
		{
			debugf(TEXT("%fms\t%s"), InitAnimStats(i).NodeExclusiveTime * 1000.f, *InitAnimStats(i).NodeName.ToString());
			TotalBlendTime += InitAnimStats(i).NodeExclusiveTime;
		}
		debugf(TEXT(" ======= Total Exclusive Time: %fms"), TotalBlendTime * 1000.f);
#endif
	}
}

/** 
 *	Iterate over all SkelControls in the AnimTree (Animations) calling InitSkelControl. 
 *	Also sets up the SkelControlIndex array indicating which SkelControl to apply when we reach certain bones.
 */
void USkeletalMeshComponent::InitSkelControls()
{
	// Initialize the SkelControls and the SkelControlIndex array.
	SkelControlIndex.Reset();
	PostPhysSkelControlIndex.Reset();
	SkelControlTickArray.Reset();

	UAnimTree* Tree = Cast<UAnimTree>(Animations);
	if(SkeletalMesh && Tree && Tree->SkelControlLists.Num() > 0)
	{
		INT NumBones = SkeletalMesh->RefSkeleton.Num();

		// Allocate SkelControlIndex array and initialize all elements to '255' - which indicates 'no control'.
		SkelControlIndex.Add(NumBones);
		appMemset( &SkelControlIndex(0), 0xFF, sizeof(BYTE) * NumBones );

		INT NumControls = Tree->SkelControlLists.Num();
		check(NumControls < 255);

		// For each list, store index of head struct at the bone where it should be applied.
		TickTag++;
		for(INT ControlIndex = 0; ControlIndex < NumControls; ControlIndex++)
		{
			INT BoneIndex = SkeletalMesh->MatchRefBone(Tree->SkelControlLists(ControlIndex).BoneName);
			if(BoneIndex != INDEX_NONE)
			{
				if(SkelControlIndex(BoneIndex) != 255)
				{
					debugf(NAME_DevAnim,TEXT("SkelControl: Trying To Control Bone Which Already Has Control List.") );
				}
				else
				{
					// Save index of SkelControl.
					SkelControlIndex(BoneIndex) = ControlIndex;

					// Now look for controls flagged as 'post physics'
					UBOOL bContainsPostPhysControl = FALSE;
					USkelControlBase* Control = Tree->SkelControlLists(ControlIndex).ControlHead;
					while( Control )
					{
						// Keep track of the controllers. Make sure they're not added twice!
						// Because one controller can be applied to different bones!
						if( Control->ControlTickTag != TickTag )
						{
							Control->ControlTickTag = TickTag;
							SkelControlTickArray.AddItem(Control);
						}

						if(Control->bPostPhysicsController)
						{
							bContainsPostPhysControl = TRUE;
						}

						Control = Control->NextControl;
					}

					// If we found one, add it to the 'post physics controller' map
					if(bContainsPostPhysControl)
					{
						// If mapping has not been allocated yet, do it now
						if(PostPhysSkelControlIndex.Num() == 0)
						{
							PostPhysSkelControlIndex.Add(NumBones);
							appMemset( &PostPhysSkelControlIndex(0), 0xFF, sizeof(BYTE) * NumBones );
						}

						PostPhysSkelControlIndex(BoneIndex) = ControlIndex;
					}
				}
			}
		}
	}
}

/** 
 *	Utility for find a USkelControl by name from the AnimTree currently being used by this SkelelalMeshComponent.
 *	Do not hold on to pointer- will become invalid when tree changes.
 */
USkelControlBase* USkeletalMeshComponent::FindSkelControl(FName InControlName)
{
	UAnimTree* AnimTree = Cast<UAnimTree>(Animations);
	if(AnimTree)
	{
		return AnimTree->FindSkelControl(InControlName);
	}

	return NULL;
}


/** 
 * Find an Animation Node in the Animation Tree whose NodeName matches InNodeName. 
 * Warning: The search is O(n), so for large AnimTrees, cache result.
 */
UAnimNode* USkeletalMeshComponent::FindAnimNode(FName InNodeName)
{
	if( Animations )
	{
		return Animations->FindAnimNode(InNodeName);
	}

	return NULL;
}


/** 
 *	Utility for find a UMorphNode by name from the AnimTree currently being used by this SkelelalMeshComponent.
 *	Do not hold on to pointer- will become invalid when tree changes.
 */
UMorphNodeBase* USkeletalMeshComponent::FindMorphNode(FName InNodeName)
{
	UAnimTree* AnimTree = Cast<UAnimTree>(Animations);
	if(AnimTree)
	{
		return AnimTree->FindMorphNode(InNodeName);
	}

	return NULL;
}


/** 
 *	Find the current world space location and rotation of a named socket on the skeletal mesh component.
 *	If the socket is not found, then it returns false and the inputs are initialized to (0,0,0)
 *	@param InSocketName the name of the socket to find
 *	@param OutLocation (out) set to the world space location of the socket
 *	@param OutRotation (out) if not NULL, the rotator pointed to is set to the world space rotation of the socket
 *	@return whether or not the socket was found
 */
UBOOL USkeletalMeshComponent::GetSocketWorldLocationAndRotation(FName InSocketName, FVector& OutLocation, FRotator* OutRotation, INT Space)
{
	if( SkeletalMesh )
	{
		USkeletalMeshSocket* Socket = SkeletalMesh->FindSocket( InSocketName );
		if( Socket )
		{
			INT BoneIndex = MatchRefBone(Socket->BoneName);
			if( BoneIndex != INDEX_NONE )
			{
				FMatrix BoneMatrix = GetBoneMatrix(BoneIndex);
				FRotationTranslationMatrix SocketMatrix(Socket->RelativeRotation,Socket->RelativeLocation);
				FMatrix WorldSocketMatrix = SocketMatrix * BoneMatrix;
				// if local, apply worldtoLocal
				if ( Space == 1 )
				{
					WorldSocketMatrix *= LocalToWorld.InverseSafe();
				}

				OutLocation = WorldSocketMatrix.GetOrigin();
				if (OutRotation != NULL)
				{
					*OutRotation = WorldSocketMatrix.Rotator();
				}

				return true;
			}
			else
			{
#if !PS3 // hopefully will be found on PC
				debugf(NAME_Warning, TEXT("GetSocketWorldLocationAndRotation : Could not find bone '%s'"), *Socket->BoneName.ToString() );
#endif
			}
		}
		else
		{
#if !PS3 // hopefully will be found on PC
			debugf(NAME_Warning, TEXT("GetSocketWorldLocationAndRotation : Could not find socket '%s' in '%s'"), *InSocketName.ToString(), SkeletalMesh?*SkeletalMesh->GetName():TEXT("None") );
#endif
		}
	}
	else
	{
#if !PS3 // hopefully will be found on PC
		debugf(NAME_Warning, TEXT("GetSocketWorldLocationAndRotation : Could not find SkeletalMesh (SkeletalMesh == NULL :-( )") );
#endif
	}

	OutLocation = FVector(0,0,0);
	if(OutRotation != NULL)
	{
		*OutRotation = FRotator(0,0,0);
	}

	return false;
}


/**
 * Returns the LocalToWorld Transformation for the given attachment
 * @param	Attachment		Attachment we want our transform from
 * @return					Calculated transform from our attachment
 */
FMatrix USkeletalMeshComponent::GetAttachmentLocalToWorld(const FAttachment& Attachment)
{
	const INT BoneIndex = MatchRefBone(Attachment.BoneName);
	check (BoneIndex != INDEX_NONE && BoneIndex < SpaceBases.Num());
	
	FVector RelativeScale = (Attachment.RelativeScale == FVector(0)) ? FVector(1) : Attachment.RelativeScale;
	FMatrix AttachmentToWorld = FScaleRotationTranslationMatrix( RelativeScale, Attachment.RelativeRotation, Attachment.RelativeLocation ) * SpaceBases(BoneIndex).ToMatrix() * LocalToWorld;	
	return AttachmentToWorld;
}

///////////////////////////////////////////////////////
// Script function implementations


/** @see USkeletalMeshComponent::FindAnimSequence */
void USkeletalMeshComponent::execFindAnimSequence( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(AnimSeqName);
	P_FINISH;

	*(UAnimSequence**)Result = FindAnimSequence( AnimSeqName );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execFindAnimSequence);

/** @see USkeletalMeshComponent::FindMorphTarget */
void USkeletalMeshComponent::execFindMorphTarget( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(MorphTargetName);
	P_FINISH;

	*(UMorphTarget**)Result = FindMorphTarget( MorphTargetName );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execFindMorphTarget);

void USkeletalMeshComponent::execAttachComponent(FFrame& Stack,RESULT_DECL)
{
	P_GET_OBJECT(UActorComponent,Component);
	P_GET_NAME(BoneName);
	P_GET_VECTOR_OPTX(RelativeLocation,FVector(0,0,0));
	P_GET_ROTATOR_OPTX(RelativeRotation,FRotator(0,0,0));
	P_GET_VECTOR_OPTX(RelativeScale,FVector(1,1,1));
	P_FINISH;

	if (Component == NULL)
	{
		debugf(NAME_Warning,TEXT("Attempting to attach NULL component to %s"),*GetName());
	}
	else
	{
		AttachComponent(Component,BoneName,RelativeLocation,RelativeRotation,RelativeScale);
	}
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execAttachComponent);

void USkeletalMeshComponent::execDetachComponent(FFrame& Stack,RESULT_DECL)
{
	P_GET_OBJECT(UActorComponent,Component);
	P_FINISH;

	if (Component == NULL)
	{
		debugf(NAME_Warning,TEXT("Attempting to detach NULL component from %s"),*GetName());
	}
	else
	{
		DetachComponent(Component);
	}
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execDetachComponent);

void USkeletalMeshComponent::execAttachComponentToSocket(FFrame& Stack, RESULT_DECL)
{
	P_GET_OBJECT(UActorComponent,Component);
	P_GET_NAME(SocketName);
	P_FINISH;

	if (Component == NULL)
	{
		debugf(NAME_Warning,TEXT("Attempting to attach NULL component to %s, socket %s"),*GetName(),*SocketName.ToString());
	}
	else
	{
		AttachComponentToSocket(Component, SocketName);
	}
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execAttachComponentToSocket);

void USkeletalMeshComponent::execGetSocketWorldLocationAndRotation(FFrame& Stack, RESULT_DECL)
{
	P_GET_NAME(InSocketName);
	P_GET_VECTOR_REF(OutLocation);
	P_GET_ROTATOR_REF(OutRotation); // optional
	P_GET_INT_OPTX(Space, 0);
	P_FINISH;

	*(UBOOL*)Result = GetSocketWorldLocationAndRotation(InSocketName, OutLocation, pOutRotation, Space);
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execGetSocketWorldLocationAndRotation);

void USkeletalMeshComponent::execGetSocketByName(FFrame& Stack, RESULT_DECL)
{
	P_GET_NAME(InSocketName);
	P_FINISH;

	USkeletalMeshSocket* Socket = NULL;

	if( SkeletalMesh )
	{
		Socket = SkeletalMesh->FindSocket( InSocketName );
	}
	else
	{
		debugf(NAME_Warning,TEXT("GetSocketByName(): No SkeletalMesh for %s"), *GetName());
	}

	*(USkeletalMeshSocket**)Result = Socket;
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execGetSocketByName);

/** 
 * Returns bone name linked to a given named socket on the skeletal mesh component.
 * If you're unsure to deal with sockets or bones names, you can use this function to filter through, and always return the bone name.
 * @input	bone name or socket name
 * @output	bone name
 */
FName USkeletalMeshComponent::GetSocketBoneName(FName InSocketName)
{
	if(!SkeletalMesh)
	{
		return NAME_None;
	}

	// First check for a socket
	USkeletalMeshSocket* TmpSocket = SkeletalMesh->FindSocket(InSocketName);
	if( TmpSocket )
	{
		return TmpSocket->BoneName;
	}

	// If socket is not found, maybe it was just a bone name.
	if( MatchRefBone(InSocketName) != INDEX_NONE )
	{
		return InSocketName;
	}

	// Doesn't exist.
	return NAME_None;
}

/** Script handler for GetSocketBoneName */
void USkeletalMeshComponent::execGetSocketBoneName(FFrame& Stack, RESULT_DECL)
{
	P_GET_NAME(InSocketName);
	P_FINISH;

	*(FName*)Result = GetSocketBoneName(InSocketName);
}


void USkeletalMeshComponent::execFindComponentAttachedToBone(FFrame& Stack, RESULT_DECL)
{
	P_GET_NAME(InBoneName);
	P_FINISH;

	UActorComponent*	FoundComponent = NULL;

	if( InBoneName != NAME_None )
	{
		for(INT idx=0; idx<Attachments.Num(); idx++)
		{
			if( Attachments(idx).BoneName == InBoneName )
			{
				FoundComponent = Attachments(idx).Component;
				break;
			}
		}
	}
	*(UActorComponent**)Result = FoundComponent;
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execFindComponentAttachedToBone);

void USkeletalMeshComponent::execIsComponentAttached(FFrame& Stack, RESULT_DECL)
{
	P_GET_OBJECT(UActorComponent,Component)
	P_GET_NAME_OPTX(BoneName,NAME_None)
	P_FINISH;

	UBOOL	bFound = false;

	for(INT idx=0; idx<Attachments.Num(); idx++)
	{
		if( Attachments(idx).Component == Component &&
			(BoneName == NAME_None || Attachments(idx).BoneName == BoneName) )
		{
			bFound = true;
			break;
		}
	}

	*(UBOOL*)Result = bFound;
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execIsComponentAttached);

void USkeletalMeshComponent::execAttachedComponents(FFrame& Stack, RESULT_DECL)
{
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_OBJECT_REF(UActorComponent, OutComponent);
	P_FINISH;

	if (BaseClass == NULL)
	{
		debugf(NAME_Error, TEXT("(%s:%04X) AttachedComponents() called with no class"), *Stack.Node->GetFullName(), Stack.Code - &Stack.Node->Script(0));
		SKIP_ITERATOR;
	}
	else
	{
		INT Index = 0;
		PRE_ITERATOR;
			// Fetch next component in the iteration.
			OutComponent = NULL;
			while (Index < Attachments.Num() && OutComponent == NULL)
			{
				UActorComponent* TestComponent = Attachments(Index).Component;
				Index++;
				if (TestComponent != NULL && !TestComponent->IsPendingKill() && TestComponent->IsA(BaseClass))
				{
					OutComponent = TestComponent;
				}
			}
			if (OutComponent == NULL)
			{
				EXIT_ITERATOR;
				break;
			}
		POST_ITERATOR;
	}
}

void USkeletalMeshComponent::execGetTransformMatrix(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	*(FMatrix*)Result = GetTransformMatrix();
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execGetTransformMatrix);


/**
 * Return Transform Matrix for SkeletalMeshComponent considering root motion setups
 * 
 * @param SkelComp SkeletalMeshComponent to get transform matrix from
 */
FMatrix USkeletalMeshComponent::GetTransformMatrix()
{
	FMatrix RootTransform = GetBoneMatrix(0);
	FVector Translation;
	FQuat Rotation;
	
	// if in editor, it should always use localToWorld
	// if root motion is ignored, use root transform 
	if( GIsGame && RootMotionMode == RMM_Ignore)
	{
		// add root translation info
		Translation = RootTransform.GetOrigin();
	}
	else
	{
		Translation = LocalToWorld.TransformFVector(SkeletalMesh->RefSkeleton(0).BonePos.Position);
	}

	// if root rotation is ignored, use root transform rotation
	if( RootMotionRotationMode == RMRM_Ignore )
	{
		Rotation = FQuat(RootTransform.GetMatrixWithoutScale());
	}
	else
	{
		Rotation = SkeletalMesh->RefSkeleton(0).BonePos.Orientation*FQuat(LocalToWorld.GetMatrixWithoutScale());
	}

	// now I need to get scale
	// only LocalToWorld will have scale
	FVector ScaleVector = LocalToWorld.GetScaleVector();

	Rotation.Normalize();
	return FScaleMatrix(ScaleVector)*FQuatRotationTranslationMatrix(Rotation, Translation);
}

void USkeletalMeshComponent::execSetSkeletalMesh( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(USkeletalMesh, NewMesh);
	P_GET_UBOOL_OPTX(bKeepSpaceBases, FALSE);
	P_FINISH;	

#if WITH_APEX
	if( NewMesh != SkeletalMesh)
	{
		ReleaseApexClothing(); // release previously allocated apex clothing
	}
#endif

	SetSkeletalMesh( NewMesh, bKeepSpaceBases);
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execSetSkeletalMesh);


void USkeletalMeshComponent::execSetPhysicsAsset( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UPhysicsAsset, NewPhysicsAsset);
	P_GET_UBOOL_OPTX(bForceReInit, FALSE);
	P_FINISH;	

	SetPhysicsAsset(NewPhysicsAsset, bForceReInit); 
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execSetPhysicsAsset);

void USkeletalMeshComponent::execSetForceRefPose( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(bNewForceRefPose);
	P_FINISH;	

	SetForceRefPose(bNewForceRefPose); 
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execSetForceRefPose);

//
//	USkeletalMeshComponent::execSetParentAnimComponent
//

void USkeletalMeshComponent::execSetParentAnimComponent( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(USkeletalMeshComponent, NewParentAnimComp);
	P_FINISH;

	SetParentAnimComponent( NewParentAnimComp );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execSetParentAnimComponent);

//
//	USkeletalMeshComponent::execGetBoneQuaternion
//

void USkeletalMeshComponent::execGetBoneQuaternion( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(BoneName);
	P_GET_INT_OPTX(Space,0);
	P_FINISH;

	*(FQuat*)Result = GetBoneQuaternion(BoneName, Space);
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execGetBoneQuaternion);

FQuat USkeletalMeshComponent::GetBoneQuaternion(FName BoneName, INT Space) const
{
	INT BoneIndex = MatchRefBone(BoneName);

	if( BoneIndex == INDEX_NONE )
	{
		debugf(NAME_Warning, TEXT("USkeletalMeshComponent::execGetBoneQuaternion : Could not find bone: %s"), *BoneName.ToString());
		return FQuat::Identity;
	}

	FBoneAtom BoneTransform;
	if( Space == 1 )
	{
		if(ParentAnimComponent)
		{
			if(BoneIndex < ParentBoneMap.Num())
			{
				INT ParentBoneIndex = ParentBoneMap(BoneIndex);
				// If ParentBoneIndex is valid, grab matrix from ParentAnimComponent.
				if(	ParentBoneIndex != INDEX_NONE && 
					ParentBoneIndex < ParentAnimComponent->SpaceBases.Num())
				{
					BoneTransform = ParentAnimComponent->SpaceBases(ParentBoneIndex);
				}
				else
				{
					BoneTransform = FBoneAtom::Identity;
				}
			}
			else
			{
				BoneTransform = FBoneAtom::Identity;
			}
		}
		else
		{
			BoneTransform = SpaceBases(BoneIndex);
		}
	}
	else
	{
		BoneTransform = GetBoneAtom(BoneIndex);
	}

	BoneTransform.RemoveScaling();
	return BoneTransform.GetRotation();
}

//
//	USkeletalMeshComponent::execGetBoneLocation
//

void USkeletalMeshComponent::execGetBoneLocation( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(BoneName);
	P_GET_INT_OPTX(Space,0);
	P_FINISH;

	*(FVector*)Result = GetBoneLocation(BoneName,Space);
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execGetBoneLocation);

FVector USkeletalMeshComponent::GetBoneLocation( FName BoneName, INT Space ) const
{
	INT BoneIndex = MatchRefBone(BoneName);
	if( BoneIndex == INDEX_NONE )
	{
		debugf(NAME_DevAnim,TEXT("USkeletalMeshComponent::GetBoneLocation (%s %s): Could not find bone: %s"), *GetFullName(), *GetDetailedInfo(), *BoneName.ToString() );
		return FVector(0,0,0);
	}

	// If space == Local
	if( Space == 1 )
	{
		if(ParentAnimComponent)
		{
			if(BoneIndex < ParentBoneMap.Num())
			{
				INT ParentBoneIndex = ParentBoneMap(BoneIndex);
				// If ParentBoneIndex is valid, grab transform from ParentAnimComponent.
				if(	ParentBoneIndex != INDEX_NONE && 
					ParentBoneIndex < ParentAnimComponent->SpaceBases.Num())
				{
					return ParentAnimComponent->SpaceBases(ParentBoneIndex).GetOrigin();
				}
			}
			
			// return empty vector
			return FVector( 0.f, 0.f, 0.f );			
		}
		else
		{
			return SpaceBases(BoneIndex).GetOrigin();
		}
	}
	else
	{
		// To support non-uniform scale (via LocalToWorld), use GetBoneMatrix
		return GetBoneMatrix(BoneIndex).GetOrigin();
	}
}

void USkeletalMeshComponent::execGetBoneAxis(FFrame& Stack, RESULT_DECL)
{
	P_GET_NAME(BoneName);
	P_GET_BYTE(Axis);
	P_FINISH;

	*(FVector*)Result = GetBoneAxis(BoneName,Axis);
}

FVector USkeletalMeshComponent::GetBoneAxis( FName BoneName, BYTE Axis ) const
{
	INT BoneIndex = MatchRefBone(BoneName);
	if (BoneIndex == INDEX_NONE)
	{
		debugf(NAME_Warning, TEXT("USkeletalMeshComponent::execGetBoneAxis : Could not find bone: %s"), *BoneName.ToString());
		return FVector(0.f, 0.f, 0.f);
	}
	else if (Axis == AXIS_None || Axis == 3 || Axis > 4)
	{
		debugf(NAME_Warning, TEXT("USkeletalMeshComponent::execGetBoneAxis: Invalid axis specified"));
		return FVector(0.f, 0.f, 0.f);
	}
	else
	{
		INT MatrixAxis;
		if (Axis == AXIS_X)
		{
			MatrixAxis = 0;
		}
		else if (Axis == AXIS_Y)
		{
			MatrixAxis = 1;
		}
		else
		{
			MatrixAxis = 2;
		}
		return GetBoneMatrix(BoneIndex).GetAxis(MatrixAxis).SafeNormal();
	}
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent, INDEX_NONE, execGetBoneAxis);

/** 
 *	Transform a location/rotation from world space to bone relative space.
 *	This is handy if you know the location in world space for a bone attachment, as AttachComponent takes location/rotation in bone-relative space.
 */
void USkeletalMeshComponent::TransformToBoneSpace(FName BoneName, const FVector & InPosition, const FRotator & InRotation, FVector & OutPosition, FRotator & OutRotation)
{
	INT BoneIndex = MatchRefBone(BoneName);
	if(BoneIndex != INDEX_NONE)
	{
		FMatrix BoneToWorldTM = GetBoneMatrix(BoneIndex);
		FMatrix WorldTM = FRotationTranslationMatrix(InRotation, InPosition);
		FMatrix LocalTM = WorldTM * BoneToWorldTM.InverseSafe();

		OutPosition = LocalTM.GetOrigin();
		OutRotation = LocalTM.Rotator();
	}
}
void USkeletalMeshComponent::execTransformToBoneSpace(FFrame& Stack, RESULT_DECL)
{
	P_GET_NAME(BoneName);
	P_GET_VECTOR(InPosition);
	P_GET_ROTATOR(InRotation);
	P_GET_VECTOR_REF(OutPosition);
	P_GET_ROTATOR_REF(OutRotation);
	P_FINISH;

	TransformToBoneSpace(BoneName, InPosition, InRotation, OutPosition, OutRotation );
}

IMPLEMENT_FUNCTION(USkeletalMeshComponent, INDEX_NONE, execTransformToBoneSpace);

/**
 *	Transfor a location/rotation in bone relative space to world space.
 */
void USkeletalMeshComponent::TransformFromBoneSpace(FName BoneName, const FVector & InPosition, const FRotator & InRotation, FVector & OutPosition, FRotator & OutRotation)
{
	INT BoneIndex = MatchRefBone(BoneName);
	if(BoneIndex != INDEX_NONE)
	{
		FMatrix BoneToWorldTM = GetBoneMatrix(BoneIndex);

		FMatrix LocalTM = FRotationTranslationMatrix(InRotation, InPosition);
		FMatrix WorldTM = LocalTM * BoneToWorldTM;

		OutPosition = WorldTM.GetOrigin();
		OutRotation = WorldTM.Rotator();
	}
}

void USkeletalMeshComponent::execTransformFromBoneSpace(FFrame& Stack, RESULT_DECL)
{
	P_GET_NAME(BoneName);
	P_GET_VECTOR(InPosition);
	P_GET_ROTATOR(InRotation);
	P_GET_VECTOR_REF(OutPosition);
	P_GET_ROTATOR_REF(OutRotation);
	P_FINISH;

	TransformFromBoneSpace(BoneName, InPosition, InRotation, OutPosition, OutRotation);
}

IMPLEMENT_FUNCTION(USkeletalMeshComponent, INDEX_NONE, execTransformFromBoneSpace);

/** finds the closest bone to the given location
 * @param TestLocation the location to test against
 * @param BoneLocation (optional, out) if specified, set to the world space location of the bone that was found, or (0,0,0) if no bone was found
 * @param IgnoreScale (optional) if specified, only bones with scaling larger than the specified factor are considered
 * @return the name of the bone that was found, or 'None' if no bone was found
 */
FName USkeletalMeshComponent::FindClosestBone(FVector TestLocation, FVector* BoneLocation, FLOAT IgnoreScale)
{
	if (SkeletalMesh == NULL)
	{
		if (BoneLocation != NULL)
		{
			*BoneLocation = FVector(0.f, 0.f, 0.f);
		}
		return NAME_None;
	}
	else
	{
		// transform the TestLocation into mesh local space so we don't have to transform the (mesh local) bone locations
		TestLocation = LocalToWorld.Inverse().TransformFVector(TestLocation);
		
		FLOAT IgnoreScaleSquared = Square(IgnoreScale);
		FLOAT BestDistSquared = BIG_NUMBER;
		INT BestIndex = -1;
		for (INT i = 0; i < SpaceBases.Num(); i++)
		{
			if (IgnoreScale < 0.f || SpaceBases(i).GetAxis(0).SizeSquared() > IgnoreScaleSquared)
			{
				FLOAT DistSquared = (TestLocation - SpaceBases(i).GetOrigin()).SizeSquared();
				if (DistSquared < BestDistSquared)
				{
					BestIndex = i;
					BestDistSquared = DistSquared;
				}
			}
		}

		if (BestIndex == -1)
		{
			if (BoneLocation != NULL)
			{
				*BoneLocation = FVector(0.f, 0.f, 0.f);
			}
			return NAME_None;
		}
		else
		{
			// transform the bone location into world space
			if (BoneLocation != NULL)
			{
				*BoneLocation = (SpaceBases(BestIndex) * LocalToWorldBoneAtom).GetOrigin();
			}
			return SkeletalMesh->RefSkeleton(BestIndex).Name;
		}
	}
}

void USkeletalMeshComponent::execFindClosestBone(FFrame& Stack, RESULT_DECL)
{
	P_GET_VECTOR(TestLocation);
	P_GET_VECTOR_REF(BoneLocation); // optional
	P_GET_FLOAT_OPTX(IgnoreScale, -1.0f);
	P_FINISH;

	*(FName*)Result = FindClosestBone(TestLocation, pBoneLocation, IgnoreScale);
}

void USkeletalMeshComponent::execGetClosestCollidingBoneLocation(FFrame& Stack, RESULT_DECL)
{
	P_GET_VECTOR(TestLocation);
	P_GET_UBOOL(bCheckZeroExtent);
	P_GET_UBOOL(bCheckNonZeroExtent);
	P_FINISH;

	FLOAT BestDistSq = BIG_NUMBER;
	FVector Best = FVector(0.0f, 0.0f, 0.0f);
	if (PhysicsAsset != NULL)
	{
		for (INT i = 0; i < PhysicsAsset->BodySetup.Num(); i++)
		{
			if ( (bCheckZeroExtent && PhysicsAsset->BodySetup(i)->bBlockZeroExtent) ||
				(bCheckNonZeroExtent && PhysicsAsset->BodySetup(i)->bBlockNonZeroExtent) )
			{
				FVector BoneLoc = GetBoneLocation(PhysicsAsset->BodySetup(i)->BoneName);
				FLOAT DistSq = (BoneLoc - TestLocation).SizeSquared();
				if (DistSq < BestDistSq)
				{
					Best = BoneLoc;
					BestDistSq = DistSq;
				}
			}
		}
	}

	*(FVector*)Result = Best;
}

/** Script-exposed SetAnimTreeTemplate function. */
void USkeletalMeshComponent::execSetAnimTreeTemplate( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UAnimTree, NewTemplate);
	P_FINISH;

	SetAnimTreeTemplate(NewTemplate);
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execSetAnimTreeTemplate);

/** Script-exposed UpdateParentBoneMap function. */
void USkeletalMeshComponent::execUpdateParentBoneMap( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	UpdateParentBoneMap();
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execUpdateParentBoneMap);

/** Script-exposed InitSkelControls function. */
void USkeletalMeshComponent::execInitSkelControls( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	InitSkelControls();
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execInitSkelControls);

/** Script-exposed InitSkelControls function. */
void USkeletalMeshComponent::execInitMorphTargets( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	//InitSkelControls();
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execInitMorphTargets);

void USkeletalMeshComponent::execFindSkelControl( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(InControlName);
	P_FINISH;

	*(USkelControlBase**)Result = FindSkelControl(InControlName);
}

void USkeletalMeshComponent::execFindAnimNode( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(InNodeName);
	P_FINISH;

	*(UAnimNode**)Result = FindAnimNode(InNodeName);
}

/** returns all AnimNodes in the animation tree that are the specfied class or a subclass
 * @param BaseClass base class to return
 * @param Node (out) the returned AnimNode for each iteration
 */
void USkeletalMeshComponent::execAllAnimNodes(FFrame& Stack, RESULT_DECL)
{
	P_GET_OBJECT(UClass, BaseClass);
	P_GET_OBJECT_REF(UAnimNode, Node);
	P_FINISH;

	if (Animations == NULL)
	{
		debugf(NAME_ScriptWarning, TEXT("(%s:%04X) AllAnimNodes() called with no Animations"), *Stack.Node->GetFullName(), Stack.Code - &Stack.Node->Script(0));
		SKIP_ITERATOR;
	}
	else
	{
		TArray<UAnimNode*> AllNodes;
		// if we have a valid subclass of AnimNode
		if (BaseClass != NULL && BaseClass != UAnimNode::StaticClass())
		{
			// get only nodes of that class
			Animations->GetNodesByClass(AllNodes, BaseClass);
		}
		else
		{
			// otherwise get all of them
			Animations->GetNodes(AllNodes);
		}
		INT CurrentIndex = 0;
		PRE_ITERATOR;
			// get the next node in the iteration
			if (CurrentIndex < AllNodes.Num())
			{
				Node = AllNodes(CurrentIndex++);
			}
			else
			{
				// we're out of nodes
				Node = NULL;
				EXIT_ITERATOR;
				break;
			}
		POST_ITERATOR;
	}
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent, INDEX_NONE, execAllAnimNodes);

void USkeletalMeshComponent::execFindMorphNode( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(InNodeName);
	P_FINISH;

	*(UMorphNodeBase**)Result = FindMorphNode(InNodeName);
}

/** 
 *	Utility function for calculating transform from the component reference frame to the desired reference frame specified by the enum. 
 */
FBoneAtom USkeletalMeshComponent::CalcComponentToFrameMatrix(INT BoneIndex, BYTE Space, FName OtherBoneName)
{
	if( Space == BCS_WorldSpace )
	{
		return LocalToWorldBoneAtom;
	}
	else if( Space == BCS_ActorSpace )
	{
		if( Owner )
		{
			FBoneAtom ComponentToFrame;
			ComponentToFrame.SetMatrix(LocalToWorld * Owner->LocalToWorld().Inverse());
			return ComponentToFrame;
		}
		else
		{
			return LocalToWorldBoneAtom;
		}
	}
	else if( Space == BCS_ComponentSpace )
	{
		return FBoneAtom::Identity;
	}
	else if( Space == BCS_ParentBoneSpace )
	{
		if( BoneIndex == 0 )
		{
			return FBoneAtom::Identity;
		}
		else
		{
			const INT ParentIndex = SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;
			return SpaceBases(ParentIndex).InverseSafe();
		}
	}
	else if( Space == BCS_BoneSpace )
	{
		return SpaceBases(BoneIndex).InverseSafe();
	}
	else if( Space == BCS_OtherBoneSpace )
	{
		INT const OtherBoneIndex = MatchRefBone(OtherBoneName);
		if( OtherBoneIndex != INDEX_NONE )
		{
			return SpaceBases(OtherBoneIndex).InverseSafe();
		}
		else
		{
			debugf(NAME_DevAnim,TEXT("GetFrameMatrix: Invalid BoneName: %s  for Mesh: %s"), *OtherBoneName.ToString(), *SkeletalMesh->GetFName().ToString() );
			return FBoneAtom::Identity;
		}
	}
	// if based on base mesh, look for EffectorSpaceBoneName in basemesh's bones or sockets
	else if( Space == BCS_BaseMeshSpace && OtherBoneName != NAME_None )
	{
		// get base's skel mesh - support Pawn, SkeletalmeshActor
		AActor* BaseActor = Owner ? Owner->GetBase(): NULL;
		if( BaseActor )
		{
			USkeletalMeshComponent* BaseSkelMesh = NULL;
			// If it's a pawn, query for the proper skeletal mesh component
			APawn* BasePawn = BaseActor->GetAPawn();
			if( BasePawn )
			{
				BaseSkelMesh = BasePawn->GetMeshForSkelControlLimbTransform(this);
			}
			else if( BaseActor->IsA(ASkeletalMeshActor::StaticClass()) )
			{
				BaseSkelMesh = CastChecked<ASkeletalMeshActor>(BaseActor)->SkeletalMeshComponent;
			}

			if( BaseSkelMesh && BaseSkelMesh->SkeletalMesh )
			{
				FRotator BaseLocalTargetRotation;
				FVector BaseLocalTargetLocation;
				FBoneAtom BaseTargetComponentToFrame;

				// Check if the socket exists and grab the world location if possible
				if( BaseSkelMesh->SkeletalMesh->FindSocket(OtherBoneName) == NULL || BaseSkelMesh->GetSocketWorldLocationAndRotation(OtherBoneName, BaseLocalTargetLocation, &BaseLocalTargetRotation) == FALSE )
				{
					// No socket, try to get the bone transform
					INT const BoneIndex = BaseSkelMesh->MatchRefBone(OtherBoneName);
					if( BoneIndex != INDEX_NONE )
					{
						BaseTargetComponentToFrame = BaseSkelMesh->GetBoneAtom(BoneIndex);
					}
					else
					{
						BaseTargetComponentToFrame = FBoneAtom::Identity;
					}
				}
				else
				{
					BaseTargetComponentToFrame = FBoneAtom(BaseLocalTargetRotation, BaseLocalTargetLocation);
				}

				return (BaseTargetComponentToFrame * LocalToWorldBoneAtom.InverseSafe()).InverseSafe();
			}
		}
	}
	else
	{
		debugf(NAME_DevAnim,TEXT("GetFrameMatrix: Unknown Frame %d  for Mesh: %s"), Space, *SkeletalMesh->GetFName().ToString() );
		return FBoneAtom::Identity;
	}

	// Fallback/default
	return FBoneAtom::Identity;
}

/** Utility function to get both Component<->Frame transforms. To avoid incurring Double Inverse in a row. */
void USkeletalMeshComponent::CalcBothComponentFrameMatrix(const INT BoneIndex, const BYTE Space, const FName OtherBoneName, FBoneAtom &ComponentToFrame, FBoneAtom& FrameToComponent) const
{
	if( Space == BCS_WorldSpace )
	{
		ComponentToFrame = LocalToWorldBoneAtom;
		FrameToComponent = ComponentToFrame.InverseSafe();
	}
	else if( Space == BCS_ActorSpace )
	{
		if( Owner )
		{
			ComponentToFrame.SetMatrix(LocalToWorld * Owner->LocalToWorld().Inverse());
			FrameToComponent = ComponentToFrame.InverseSafe();
		}
		else
		{
			//ActorToWorld = FMatrix::Identity;
			ComponentToFrame = LocalToWorldBoneAtom;
			FrameToComponent = ComponentToFrame.InverseSafe();
		}
	}
	else if( Space == BCS_ComponentSpace )
	{
		ComponentToFrame = FBoneAtom::Identity;
		FrameToComponent = FBoneAtom::Identity;
	}
	else if( Space == BCS_ParentBoneSpace )
	{
		if( BoneIndex == 0 )
		{
			ComponentToFrame = FBoneAtom::Identity;
			FrameToComponent = FBoneAtom::Identity;
		}
		else
		{
			const INT ParentIndex = SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;
			FrameToComponent = SpaceBases(ParentIndex);
			ComponentToFrame = FrameToComponent.InverseSafe();
		}
	}
	else if( Space == BCS_BoneSpace )
	{
		FrameToComponent = SpaceBases(BoneIndex);
		ComponentToFrame = FrameToComponent.InverseSafe();
	}
	else if( Space == BCS_OtherBoneSpace )
	{
		const INT OtherBoneIndex = MatchRefBone(OtherBoneName);
		if( OtherBoneIndex != INDEX_NONE )
		{
			FrameToComponent = SpaceBases(OtherBoneIndex);
			ComponentToFrame = FrameToComponent.InverseSafe();
		}
		else
		{
			debugf(NAME_DevAnim,TEXT("GetFrameMatrix: Invalid BoneName: %s  for Mesh: %s"), *OtherBoneName.ToString(), *SkeletalMesh->GetFName().ToString() );
			ComponentToFrame = FBoneAtom::Identity;
			FrameToComponent = FBoneAtom::Identity;
		}
	}
	// if based on base mesh, look for EffectorSpaceBoneName in basemesh's bones or sockets
	else if( Space == BCS_BaseMeshSpace && OtherBoneName != NAME_None )
	{
		// get base's skel mesh - support Pawn, SkeletalmeshActor
		AActor* BaseActor = Owner ? Owner->GetBase(): NULL;
		if( BaseActor )
		{
			USkeletalMeshComponent* BaseSkelMesh = NULL;
			// If it's a pawn, query for the proper skeletal mesh component
			APawn* BasePawn = BaseActor->GetAPawn();
			if( BasePawn )
			{
				BaseSkelMesh = BasePawn->GetMeshForSkelControlLimbTransform(this);
			}
			else if( BaseActor->IsA(ASkeletalMeshActor::StaticClass()) )
			{
				BaseSkelMesh = CastChecked<ASkeletalMeshActor>(BaseActor)->SkeletalMeshComponent;
			}

			if( BaseSkelMesh && BaseSkelMesh->SkeletalMesh )
			{
				FRotator BaseLocalTargetRotation;
				FVector BaseLocalTargetLocation;
				FBoneAtom BaseTargetComponentToFrame;

				// Check if the socket exists and grab the world location if possible
				if( BaseSkelMesh->SkeletalMesh->FindSocket(OtherBoneName) == NULL || BaseSkelMesh->GetSocketWorldLocationAndRotation(OtherBoneName, BaseLocalTargetLocation, &BaseLocalTargetRotation) == FALSE )
				{
					// No socket, try to get the bone transform
					INT const BoneIndex = BaseSkelMesh->MatchRefBone(OtherBoneName);
					if( BoneIndex != INDEX_NONE )
					{
						BaseTargetComponentToFrame = BaseSkelMesh->GetBoneAtom(BoneIndex);
					}
					else
					{
						BaseTargetComponentToFrame = FBoneAtom::Identity;
					}
				}
				else
				{
					BaseTargetComponentToFrame = FBoneAtom(BaseLocalTargetRotation, BaseLocalTargetLocation);
				}

				FrameToComponent = BaseTargetComponentToFrame * LocalToWorldBoneAtom.InverseSafe();
				ComponentToFrame = FrameToComponent.InverseSafe();
			}
			else
			{
				ComponentToFrame = FBoneAtom::Identity;
				FrameToComponent = FBoneAtom::Identity;
			}
		}
		else
		{
			ComponentToFrame = FBoneAtom::Identity;
			FrameToComponent = FBoneAtom::Identity;
		}
	}
	else
	{
		debugf(NAME_DevAnim,TEXT("GetFrameMatrix: Unknown Frame %d  for Mesh: %s"), Space, *SkeletalMesh->GetFName().ToString() );
		ComponentToFrame = FBoneAtom::Identity;
		FrameToComponent = FBoneAtom::Identity;
	}
}

/** Script-exposed execFindConstraintBoneName function. */
void USkeletalMeshComponent::execFindConstraintIndex( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(constraintName);
	P_FINISH;

	*(INT*)Result = PhysicsAsset ? PhysicsAsset->FindConstraintIndex(constraintName) : -1;
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execFindConstraintIndex);

/** Script-exposed execFindConstraintBoneName function. */
void USkeletalMeshComponent::execFindConstraintBoneName( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(ConstraintIndex);
	P_FINISH;

	*(FName*)Result = PhysicsAsset ? PhysicsAsset->FindConstraintBoneName(ConstraintIndex) : NAME_None;
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execFindConstraintBoneName);


/** Script-exposed execFindBodyInstanceNamed function. */
void USkeletalMeshComponent::execFindBodyInstanceNamed( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(BoneName);
	P_FINISH;

	*(URB_BodyInstance**)Result = FindBodyInstanceNamed(BoneName);
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent, INDEX_NONE, execFindBodyInstanceNamed);


/** Find a BodyInstance by BoneName */
URB_BodyInstance* USkeletalMeshComponent::FindBodyInstanceNamed(FName BoneName)
{
	URB_BodyInstance* BodyInst = NULL;

	if( !PhysicsAsset || !PhysicsAssetInstance )
	{
		return BodyInst;
	}

	INT BodyIndex = PhysicsAsset->FindBodyIndex(BoneName);
	if(BodyIndex != INDEX_NONE)
	{
		BodyInst = PhysicsAssetInstance->Bodies(BodyIndex);
	}

	return BodyInst;
}



void USkeletalMeshComponent::ForceSkelUpdate()
{
	if( IsAttached() )
	{
		// easiest way to make sure everything works is to temporarily pretend we've been recently rendered
		FLOAT OldRenderTime = LastRenderTime;
		LastRenderTime = GWorld->GetWorldInfo()->TimeSeconds;

		// Tick nodes to update things like MetaData.
		// If in-game, tick all animation channels in our anim nodes tree. Dont want to play animation in level editor.
		const UBOOL bHasBegunPlay = GWorld->HasBegunPlay();
		if( IsAttached() && Animations && bHasBegunPlay && !bNoSkeletonUpdate )
		{
			TickAnimNodes(0.f);
			TickSkelControls(0.f);
		}

		UpdateLODStatus();
		UpdateSkelPose();
		ConditionalUpdateTransform();

		LastRenderTime = OldRenderTime;
	}
}

void USkeletalMeshComponent::execForceSkelUpdate(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;
	
	ForceSkelUpdate();
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execForceSkelUpdate);


/** 
 * Force AnimTree to recache all animations.
 * Call this when the AnimSets array has been changed.
 */
void USkeletalMeshComponent::UpdateAnimations()
{
	// Call AnimSetsUpdated() on individual nodes. We'll try to do deferred work as much as we can
	if( Animations )
	{
		// Increase InitTag to trigger DeferredInitAnim()
		InitTag = Animations->NodeInitTag + 1;

		const INT AnimNodeCount = AnimTickArray.Num();
		for(INT i=0; i<AnimNodeCount; i++)
		{
			UAnimNode* Node = AnimTickArray(i);
			Node->AnimSetsUpdated();
		}
	}
}


void USkeletalMeshComponent::execUpdateAnimations(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	UpdateAnimations();
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execUpdateAnimations);


void USkeletalMeshComponent::execPlayFaceFXAnim( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT_REF(UFaceFXAnimSet, FaceFXAnimSetRef);
	P_GET_STR(AnimName);
	P_GET_STR(GroupName);
	P_GET_OBJECT_REF(USoundCue, SoundCueToPlay);
	P_FINISH;

	*(UBOOL*)Result = PlayFaceFXAnim(FaceFXAnimSetRef, AnimName, GroupName, SoundCueToPlay);
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent, INDEX_NONE, execPlayFaceFXAnim);

void USkeletalMeshComponent::execStopFaceFXAnim( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	StopFaceFXAnim();
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent, INDEX_NONE, execStopFaceFXAnim);


void USkeletalMeshComponent::execIsPlayingFaceFXAnim( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	*(UBOOL*)Result = IsPlayingFaceFXAnim();
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent, INDEX_NONE, execIsPlayingFaceFXAnim);


void USkeletalMeshComponent::execDeclareFaceFXRegister( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(RegName);
	P_FINISH;

	DeclareFaceFXRegister(RegName);
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent, INDEX_NONE, execDeclareFaceFXRegister);

void USkeletalMeshComponent::execGetFaceFXRegister( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(RegName);
	P_FINISH;

	*(FLOAT*)Result = GetFaceFXRegister(RegName);
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent, INDEX_NONE, execGetFaceFXRegister);

void USkeletalMeshComponent::execSetFaceFXRegister( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(RegName);
	P_GET_FLOAT(RegVal);
	P_GET_BYTE(RegOp);
	P_GET_FLOAT_OPTX(InterpDuration,0.0f);
	P_FINISH;

	SetFaceFXRegister(RegName, RegVal, RegOp, InterpDuration);
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent, INDEX_NONE, execSetFaceFXRegister);

void USkeletalMeshComponent::execSetFaceFXRegisterEx( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(RegName);
	P_GET_BYTE(RegOp);
	P_GET_FLOAT(FirstValue);
	P_GET_FLOAT(FirstInterpDuration);
	P_GET_FLOAT(NextValue);
	P_GET_FLOAT(NextInterpDuration);
	P_FINISH;

	SetFaceFXRegisterEx(RegName, RegOp, FirstValue, FirstInterpDuration, NextValue, NextInterpDuration);
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent, INDEX_NONE, execSetFaceFXRegisterEx);

void USkeletalMeshComponent::execGetBonesWithinRadius(FFrame& Stack, RESULT_DECL)
{
	P_GET_VECTOR(Origin);
	P_GET_FLOAT(Radius);
	P_GET_INT(TraceFlags);
	P_GET_TARRAY_REF(FName,out_Bones);
	P_FINISH;

	*(UBOOL*)Result = GetBonesWithinRadius( Origin, Radius, TraceFlags, out_Bones );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execGetBonesWithinRadius);

void USkeletalMeshComponent::execHideBone( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT( BoneIndex );
	P_GET_BYTE( PhysBodyOption );
	P_FINISH;

	HideBone( BoneIndex, (EPhysBodyOp) PhysBodyOption );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execHideBone);

void USkeletalMeshComponent::execUnHideBone( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT( BoneIndex );
	P_FINISH;

	UnHideBone( BoneIndex );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execUnHideBone);

void USkeletalMeshComponent::execIsBoneHidden( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT( BoneIndex );
	P_FINISH;

	*(UBOOL*)Result = IsBoneHidden( BoneIndex ); 
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execIsBoneHidden);

void USkeletalMeshComponent::execHideBoneByName( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME( BoneName );
	P_GET_BYTE( PhysBodyOption );
	P_FINISH;

	HideBoneByName( BoneName, (EPhysBodyOp) PhysBodyOption );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execHideBoneByName);

void USkeletalMeshComponent::execUnHideBoneByName( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME( BoneName );
	P_FINISH;

	UnHideBoneByName( BoneName );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execUnHideBoneByName);

/** 
 * Update the instanced vertex influences (weights/bones) 
 * Uses the cached list of bones to find vertices that need to use instanced influences
 * instead of the defaults from the skeletal mesh 
 */
void USkeletalMeshComponent::UpdateInstanceVertexWeights(INT LODIdx)
{
	if( MeshObject && LODInfo.IsValidIndex(LODIdx) )
	{
		FSkelMeshComponentLODInfo& MeshLODInfo = LODInfo(LODIdx);

		if(MeshLODInfo.InstanceWeightUsage == IWU_PartialSwap)
		{
			// The next if() would prevent a call that wants to reset the VertexInflucences.
			// TODO: Investigate if it should be there
			if(InstanceVertexWeightBones.Num() > 0)
			{
				TArray<FBoneIndexPair> BoneIndexPairs;
				BoneIndexPairs.Add(InstanceVertexWeightBones.Num());
				for( INT Idx=0; Idx < InstanceVertexWeightBones.Num(); Idx++ )
				{
					const FBonePair& BoneNames = InstanceVertexWeightBones(Idx);
					FBoneIndexPair& BoneIdxPair = BoneIndexPairs(Idx);
					BoneIdxPair.BoneIdx[0] = MatchRefBone(BoneNames.Bones[0]);
					BoneIdxPair.BoneIdx[1] = MatchRefBone(BoneNames.Bones[1]);
				}
				
				// force instance weights on so that resources get re-initialized
				MeshObject->ToggleVertexInfluences(TRUE,LODIdx);
				// update the skel mesh object instance 
				// this will lock/update the vertex buffer instance for the weights
				// for now just reset the vertex weights on every update
				MeshObject->UpdateVertexInfluences(LODIdx,BoneIndexPairs,TRUE);
			}
		}
		else
		{
			// Toggle instance weight usage on the mesh object
			// This will reinitialize the vertex factories
			const UBOOL bEnabled = MeshLODInfo.bAlwaysUseInstanceWeights;
			MeshObject->ToggleVertexInfluences(bEnabled,LODIdx);
		}
		
		// mark as updated
		MeshLODInfo.bNeedsInstanceWeightUpdate = FALSE;
	}
}

/** 
 * Add a new bone to the list of instance vertex weight bones
 *
 * @param BoneNames - set of bones (implicitly parented) to use for finding vertices
 */
void USkeletalMeshComponent::AddInstanceVertexWeightBoneParented(FName BoneName, UBOOL bPairWithParent)
{
	FBonePair BonePair;

	BonePair.Bones[0] = BoneName;
	// pair up with parent if desired
	if(bPairWithParent)
	{
		BonePair.Bones[1] = GetParentBone(BoneName);
	}
	else
	{
		BonePair.Bones[1] = NAME_None;
	}

	// find an existing bone name
	INT FoundIdx = FindInstanceVertexweightBonePair(BonePair);
	if( FoundIdx == INDEX_NONE )
	{		
		// add if not found
		InstanceVertexWeightBones.AddItem(BonePair);

		for (INT LODIdx = 0; LODIdx<LODInfo.Num(); LODIdx++)
		{
			FSkelMeshComponentLODInfo& MeshLODInfo = LODInfo(LODIdx);
			if (MeshLODInfo.InstanceWeightUsage == IWU_PartialSwap)
			{
				// mark for update
				MeshLODInfo.bNeedsInstanceWeightUpdate = TRUE;
			}
		}
	}
}

/** @see USkeletalMeshComponent::AddInstanceVertexWeightBoneParented */
void USkeletalMeshComponent::execAddInstanceVertexWeightBoneParented( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(BoneName);
	P_GET_UBOOL_OPTX(bPairParent, TRUE);
	P_FINISH;

	AddInstanceVertexWeightBoneParented( BoneName, bPairParent );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execAddInstanceVertexWeightBoneParented);

/** 
 * Remove a new bone to the list of instance vertex weight bones
 *
 * @param BoneNames - set of bones (implicitly parented) to use for finding vertices
 */
void USkeletalMeshComponent::RemoveInstanceVertexWeightBoneParented(FName BoneName)
{
	FBonePair BonePair;

	// pair up with parent
	BonePair.Bones[0] = BoneName;
	BonePair.Bones[1] = GetParentBone(BoneName);

	// find an existing bone name
	INT FoundIdx = FindInstanceVertexweightBonePair(BonePair);
	if( FoundIdx != INDEX_NONE )
	{
		// remove if found
		InstanceVertexWeightBones.Remove(FoundIdx);

		for (INT LODIdx = 0; LODIdx<LODInfo.Num(); LODIdx++)
		{
			FSkelMeshComponentLODInfo& MeshLODInfo = LODInfo(LODIdx);
			if (MeshLODInfo.InstanceWeightUsage == IWU_PartialSwap)
			{
				// mark for update
				MeshLODInfo.bNeedsInstanceWeightUpdate = TRUE;
			}
		}
	}
}

/** @see USkeletalMeshComponent::RemoveInstanceVertexWeightBoneParented */
void USkeletalMeshComponent::execRemoveInstanceVertexWeightBoneParented( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(BoneName);
	P_FINISH;

	RemoveInstanceVertexWeightBoneParented( BoneName );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execRemoveInstanceVertexWeightBoneParented);

/** 
 * Find an existing bone pair entry in the list of InstanceVertexWeightBones
 *
 * @param BonePair - pair of bones to search for
 * @return index of entry found or INDEX_NONE if not found
 */
INT USkeletalMeshComponent::FindInstanceVertexweightBonePair(const FBonePair& BonePair) const
{
	INT Result = INDEX_NONE;
	for( INT Idx=0; Idx < InstanceVertexWeightBones.Num(); Idx++ )
	{
		if( BonePair.IsMatch(InstanceVertexWeightBones(Idx)) )
		{
			Result=Idx;
			break;
		}
	}
	return Result;
}

/** @see USkeletalMeshComponent::FindInstanceVertexweightBonePair */
void USkeletalMeshComponent::execFindInstanceVertexweightBonePair( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT_REF(FBonePair, BonePair);
	P_FINISH;

	*(INT*)Result = FindInstanceVertexweightBonePair( BonePair );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execFindInstanceVertexweightBonePair);

/** 
 * Update the bones that specify which vertices will use instanced influences
 * This will also trigger an update of the vertex weights.
 *
 * @param BonePairs - set of bone pairs to use for finding vertices. 
 * A bone can be paired with None bone name to only match up a single bone.
 */
void USkeletalMeshComponent::UpdateInstanceVertexWeightBones( const TArray<FBonePair>& BonePairs )
{
	// check to see if matches current bone array and only update if it doesn't
	if( BonePairs != InstanceVertexWeightBones )
	{
		// copy to cached bone array
		InstanceVertexWeightBones = BonePairs;

		for (INT LODIdx = 0; LODIdx<LODInfo.Num(); LODIdx++)
		{
			FSkelMeshComponentLODInfo& MeshLODInfo = LODInfo(LODIdx);
			if (MeshLODInfo.InstanceWeightUsage == IWU_PartialSwap)
			{
				// mark for update
				MeshLODInfo.bNeedsInstanceWeightUpdate = TRUE;
			}
		}
	}
}

/** @see USkeletalMeshComponent::UpdateInstanceVertexWeightBones */
void USkeletalMeshComponent::execUpdateInstanceVertexWeightBones( FFrame& Stack, RESULT_DECL )
{
	P_GET_TARRAY_REF(FBonePair, BonePairs);
	P_FINISH;

	UpdateInstanceVertexWeightBones( BonePairs );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execUpdateInstanceVertexWeightBones);

/**
 * Enabled or disable the instanced vertex weights buffer for the skeletal mesh object
 *
 * @param bEnable - TRUE to enable, FALSE to disable
 * @param LODIdx - LOD to enable 
 */
void USkeletalMeshComponent::ToggleInstanceVertexWeights( UBOOL bEnabled, INT LODIdx)
{
	if (LODInfo.IsValidIndex(LODIdx))
	{
		FSkelMeshComponentLODInfo& MeshLODInfo = LODInfo(LODIdx);
		if (MeshLODInfo.bAlwaysUseInstanceWeights != bEnabled)
		{
			// mark for update
			MeshLODInfo.bNeedsInstanceWeightUpdate = TRUE;	

			// If we are doing full swaps, the required bones need updating
			if (MeshLODInfo.InstanceWeightUsage == IWU_FullSwap)
			{
				bRequiredBonesUpToDate = FALSE;
			}

			MeshLODInfo.bAlwaysUseInstanceWeights = bEnabled;

			// if disabled then clear out the cached bones
			if( !bEnabled )
			{
				InstanceVertexWeightBones.Empty();
			}
		}
	}
	else
	{
		debugf(*FString::Printf(TEXT("USkeletalMeshComponent: ToggleInstanceVertexWeights FAILED %d/%d"), LODIdx, LODInfo.Num()));
	}
}

/**
 * Enabled or disable the instanced vertex weights buffer for the skeletal mesh object
 *
 * @param bEnable - TRUE to enable, FALSE to disable
 * @param InInstanceWeightIdx - index of influence array to swap to
 * @param InInstanceWeightUsage - EInstanceWeightUsage type to specify usage case when toggling weights
 */
void USkeletalMeshComponent::execToggleInstanceVertexWeights( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(bEnabled);
	P_GET_INT(LODIdx);
	P_FINISH;

	ToggleInstanceVertexWeights( bEnabled,LODIdx );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execToggleInstanceVertexWeights);

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Decals on skeletal meshes.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Transforms the specified decal info into reference pose space.
 *
 * @param	Decal			Info of decal to transform.
 * @param	BoneIndex		The index of the bone hit by the decal.
 * @return					A reference to the transformed decal info, or NULL if the operation failed.
 */
FDecalState* USkeletalMeshComponent::TransformDecalToRefPoseSpace(FDecalState* Decal, INT BoneIndex) const
{
	FDecalState* NewDecalState = NULL;

	FBoneAtom ComponentSpaceToWorldTM = GetBoneAtom( BoneIndex );
	ComponentSpaceToWorldTM.RemoveScaling();
	const FBoneAtom RefToWorld = SkeletalMesh->RefBasesInvMatrix(BoneIndex) * ComponentSpaceToWorldTM;
	const FBoneAtom WorldToRef = RefToWorld.Inverse();

	NewDecalState = Decal;

	NewDecalState->HitLocation = WorldToRef.TransformFVector4( Decal->HitLocation );
	NewDecalState->HitTangent = WorldToRef.TransformNormal( Decal->HitTangent );
	NewDecalState->HitBinormal = WorldToRef.TransformNormal( Decal->HitBinormal );
	NewDecalState->HitNormal = WorldToRef.TransformNormal( Decal->HitNormal );
	NewDecalState->WorldTexCoordMtx = FMatrix( /*TileX**/NewDecalState->HitTangent/Decal->Width,
									/*TileY**/NewDecalState->HitBinormal/Decal->Height,
									NewDecalState->HitNormal,
									FVector(0.f,0.f,0.f) ).Transpose();

	NewDecalState->TransformFrustumVerts( WorldToRef );

	return NewDecalState;
}

/** 
* @return TRUE if the primitive component can render decals
*/
UBOOL USkeletalMeshComponent::SupportsDecalRendering() const
{
	return MeshObject && MeshObject->SupportsDecalRendering();
}

namespace{
class FIndexRemap
{
public:
	INT NewIndex;
	UBOOL bIsRigid;
	FIndexRemap(INT InNewIndex,UBOOL bInIsRigid)
		: NewIndex( InNewIndex )
		, bIsRigid( bInIsRigid )
	{}
};
} // namespace

void USkeletalMeshComponent::GenerateDecalRenderData(FDecalState* Decal, TArray< FDecalRenderData* >& OutDecalRenderDatas) const
{
	SCOPE_CYCLE_COUNTER(STAT_DecalSkeletalMeshAttachTime);

	OutDecalRenderDatas.Reset();

	// Can't do anything without a SkeletalMesh or the specified decal doesn't
	// project on skeletal meshes
	if( !SkeletalMesh || !Decal->bProjectOnSkeletalMeshes )
	{
		return;
	}

	// Check to see if the decal was manually attached, which is permitted
	const AActor* DecalOwner = Decal->DecalComponent->GetOwner();
	const UBOOL bDecalManuallyAttached =
		DecalOwner &&
		DecalOwner->Base == Owner &&
		DecalOwner->BaseSkelComponent == this &&
		DecalOwner->BaseBoneName != NAME_None;

	// If the decal is static, do nothing unless the decal is manually attached
	if( Decal->DecalComponent->bStaticDecal && !bDecalManuallyAttached )
	{
		return;
	}

	if( bDecalManuallyAttached )
	{
		// Find user-specified bone index
		Decal->HitBoneIndex = MatchRefBone( DecalOwner->BaseBoneName );
	}
	else
	{
		// Find the named bone index
		Decal->HitBoneIndex = MatchRefBone( Decal->HitBone );
	}

	if ( Decal->HitBoneIndex != INDEX_NONE )
	{
		// Transform the decal to reference pose space.
		TransformDecalToRefPoseSpace( Decal, Decal->HitBoneIndex );

		FDecalRenderData* DecalRenderData = new FDecalRenderData( NULL, FALSE, FALSE );
		DecalRenderData->NumTriangles = DecalRenderData->GetNumIndices()/3;
		DecalRenderData->DecalBlendRange = Decal->DecalComponent->CalcDecalDotProductBlendRange();

		// Record that we need to generate a RefToLocal transform for the bone this decal is attached to.
		if( MeshObject )
		{
			MeshObject->DecalRequiredBoneIndices.AddUniqueItem(Decal->HitBoneIndex);
		}

		OutDecalRenderDatas.AddItem( DecalRenderData );
	}
}

void USkeletalMeshComponent::DebugVerifyFaceFXBoneList()
{
#if !FINAL_RELEASE
#if WITH_FACEFX
	if ( FaceFXActorInstance )
	{
		FxActor* FaceFXActor = FaceFXActorInstance->GetActor();
		if ( FaceFXActor )
		{
			debugf(NAME_DevAnim,TEXT( "FACEFX: Checking %s - %s" ), *SkeletalMesh->GetPathName(), ANSI_TO_TCHAR(FaceFXActor->GetNameAsCstr()));
			FxMasterBoneList& MasterBoneList = FaceFXActor->GetMasterBoneList();
			// go through master bone list
			for( FxSize i = 0; i < MasterBoneList.GetNumRefBones(); ++i )
			{
				FxBool bFoundBone = FxFalse;
				FName BoneName = FName(*FString(MasterBoneList.GetRefBone(i).GetNameAsCstr()), FNAME_Find, TRUE); 
				if ( BoneName!=NAME_None )
				{
					for( INT j = 0; j < SkeletalMesh->RefSkeleton.Num(); ++j )
					{
						// if name is same, check if index is same
						if( BoneName == SkeletalMesh->RefSkeleton(j).Name )
						{
							bFoundBone = FxTrue;

							// name if same, but bone index does not match, then
							if (MasterBoneList.GetRefBoneClientIndex(i)!=j)
							{
								const FxInt32 FXIndex = MasterBoneList.GetRefBoneClientIndex(i);

								// print
								if (FXIndex < SkeletalMesh->RefSkeleton.Num())
								{
									debugf(NAME_DevAnim,TEXT( "MISMATCH: Name of bone [%s] - FX Index(%d) | Unreal Index(%d) | Unreal Bone name of FX Index (%s)" ), 
										ANSI_TO_TCHAR(MasterBoneList.GetRefBone(i).GetNameAsCstr()), FXIndex, j, *SkeletalMesh->RefSkeleton(FXIndex).Name.GetNameString());
								}
								else
								{
									// this is just in case FXIndex is outside of RefSkeleton indices.
									debugf(NAME_DevAnim,TEXT( "MISMATCH: Name of bone [%s] - FX Index(%d) | Unreal Index(%d)" ), 
										MasterBoneList.GetRefBone(i).GetNameAsCstr(), FXIndex, j);
								}
							}					
						}
					}

				}

				if( !bFoundBone )
				{
					debugf(TEXT("FaceFX: WARNING Reference bone %s could not be found in the skeleton '%s'."), 
						ANSI_TO_TCHAR(MasterBoneList.GetRefBone(i).GetNameAsCstr())
						,*GetDetailedInfo()
						);
				}
			}
		}
	}
#endif // #if WITH_FACEFX
#endif // #if !FINAL_RELEASE
}

typedef struct 
{
	FString FaceFXActorName; // FXActor Name
	TArray<FString> SkeletalMeshPaths; // SkeletalMesh Path
} FaceFXActorInfo;

void USkeletalMeshComponent::TraceFaceFX( UBOOL bOutput )
{
#if !FINAL_RELEASE
#if WITH_FACEFX
#if !PS3
	static TMap<FxActor*, FaceFXActorInfo> FaceFXActorInfoList;

	if ( bOutput )
	{
		for( TMap<FxActor*, FaceFXActorInfo>::TConstIterator Iter(FaceFXActorInfoList); Iter; ++Iter )
		{
			const FaceFXActorInfo & Info = Iter.Value();
			debugf(NAME_DevAnim,TEXT( "TRACE FACEFX: [%s] - Number of SkeletalMesh [%d] " ), *Info.FaceFXActorName, Info.SkeletalMeshPaths.Num());

			for ( INT I=0; I<Info.SkeletalMeshPaths.Num(); ++I )
			{
				debugf(NAME_DevAnim,TEXT( "TRACE FACEFX: [%d] - [%s] " ), I+1, *Info.SkeletalMeshPaths(I) );
			}
		}
	}
	else
	{
		if ( FaceFXActorInstance && SkeletalMesh )
		{
			FxActor* FaceFXActor = FaceFXActorInstance->GetActor();
			if ( FaceFXActor )
			{
				FaceFXActorInfo * Info = FaceFXActorInfoList.Find(FaceFXActor);
				// same actor exists, 
				if ( Info )
				{
					UBOOL bFoundSame=FALSE;
					// verify if the skeletalmeshpaths have this
					for (INT I=0; I<Info->SkeletalMeshPaths.Num(); ++I)
					{
						if (Info->SkeletalMeshPaths(I) == SkeletalMesh->GetPathName())
						{
							bFoundSame=TRUE;
							break;
						}
					}

					if (!bFoundSame)
					{
						// add new one
						Info->SkeletalMeshPaths.AddItem(SkeletalMesh->GetPathName());
					}
				}
				else
				{
					FaceFXActorInfo NewInfo;
					NewInfo.FaceFXActorName = ANSI_TO_TCHAR(FaceFXActor->GetNameAsCstr()); 
					NewInfo.SkeletalMeshPaths.AddItem(SkeletalMesh->GetPathName());

					FaceFXActorInfoList.Set(FaceFXActor, NewInfo);
				}
			}
		}
	}
#endif //#if !PS3
#endif //#if WITH_FACEFX
#endif //#if !FINAL_RELEASE
}

/** Enables or disables Alternative Bone Weighting Edit Mode 
 *  Disable only works in editor
 */
void USkeletalMeshComponent::EnableAltBoneWeighting(UBOOL bEnable, INT LOD)
{
	if (bEnable)
	{
		// lock current LOD - ForcedLodModel is 1 base
		ForcedLodModel = ::Clamp(LOD, MinLodModel, SkeletalMesh->LODModels.Num()-1)+1;

		ToggleInstanceVertexWeights(TRUE, ForcedLodModel-1);
	}
	// you can revert back only in editor
	else if ( GIsEditor )
	{
		// do not go back to 0, that's very confusing in the menu
		// lock current LOD - ForcedLodModel is 1 base
		if ( SkeletalMesh )
		{
			ForcedLodModel = ::Clamp(LOD, MinLodModel, SkeletalMesh->LODModels.Num()-1)+1;
		}
		ToggleInstanceVertexWeights(FALSE, ForcedLodModel-1);
	}
}


/** 
 *  Show/Hide Material - technical correct name for this is Section, but seems Material is mostly used
 *  This disable rendering of certain Material ID (Section)
 *
 * @param MaterialID - id of the material to match a section on and to show/hide
 * @param bShow - TRUE to show the section, otherwise hide it
 * @param LODIndex - index of the lod entry since material mapping is unique to each LOD
 */
void USkeletalMeshComponent::execShowMaterialSection( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(MaterialID)
	P_GET_UBOOL(ShowFlag)
	P_GET_INT(LODIndex)
	P_FINISH;

	ShowMaterialSection(MaterialID, ShowFlag,LODIndex);
}

IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execShowMaterialSection);

/** 
 *  Show/Hide Material - technical correct name for this is Section, but seems Material is mostly used
 *  This disable rendering of certain Material ID (Section)
 *
 * @param MaterialID - id of the material to match a section on and to show/hide
 * @param bShow - TRUE to show the section, otherwise hide it
 * @param LODIndex - index of the lod entry since material mapping is unique to each LOD
 */
void USkeletalMeshComponent::ShowMaterialSection(INT MaterialID, UBOOL bShow, INT LODIndex)
{
	if (!SkeletalMesh || LODIndex >= SkeletalMesh->LODModels.Num())
	{
		// no skeletalmesh, then nothing to do. 
		return;
	}
	// Make sure LOD info for this component has been initialized
	InitLODInfos();
	const FSkeletalMeshLODInfo& SkelLODInfo = SkeletalMesh->LODInfo(LODIndex);
	FSkelMeshComponentLODInfo& SkelCompLODInfo = LODInfo(LODIndex);
	TArray<UBOOL>& HiddenMaterials = SkelCompLODInfo.HiddenMaterials;
	
	// allocate if not allocated yet
	if ( HiddenMaterials.Num() != SkeletalMesh->Materials.Num() )
	{
		// Using skeletalmesh component because Materials.Num() should be <= SkeletalMesh->Materials.Num()		
		HiddenMaterials.Empty(SkeletalMesh->Materials.Num());
		HiddenMaterials.AddZeroed(SkeletalMesh->Materials.Num());
	}
	// If we are at a dropped LOD, route material index through the LODMaterialMap in the LODInfo struct.
	INT UseMaterialIndex = MaterialID;			
	if(LODIndex > 0)
	{
		if(SkelLODInfo.LODMaterialMap.IsValidIndex(MaterialID))
		{
			UseMaterialIndex = SkelLODInfo.LODMaterialMap(MaterialID);
			UseMaterialIndex = ::Clamp( UseMaterialIndex, 0, HiddenMaterials.Num() );
		}
	}
	// Mark the mapped section material entry as visible/hidden
	if (HiddenMaterials.IsValidIndex(UseMaterialIndex))
	{
		HiddenMaterials(UseMaterialIndex) = !bShow;
	}

	if ( MeshObject )
	{
		// need to send render thread for updated hidden section
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			FUpdateHiddenSectionCommand, 
			FSkeletalMeshObject*, MeshObject, MeshObject, 
			TArray<UBOOL>, HiddenMaterials, HiddenMaterials, 
			INT, LODIndex, LODIndex,
		{
			MeshObject->SetHiddenMaterials(LODIndex,HiddenMaterials);
		});
	}
}

/** 
 * Retrieves the materials used in this component 
 * 
 * @param OutMaterials	The list of used materials.
 */
void USkeletalMeshComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	if( SkeletalMesh )
	{
		// The max number of materials used is the max of the materials on the skeletal mesh and the materials on the mesh component
		const INT NumMaterials = Max( SkeletalMesh->Materials.Num(), Materials.Num() );
		for( INT MatIdx = 0; MatIdx < NumMaterials; ++MatIdx )
		{
			// GetMaterial will determine the correct material to use for this index.  
			UMaterialInterface* MaterialInterface = GetMaterial( MatIdx );
			OutMaterials.AddItem( MaterialInterface );
		}
	}
}
