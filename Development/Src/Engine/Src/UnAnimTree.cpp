/*=============================================================================
	UnAnimTree.cpp: Blend tree implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "UnLinkedObjDrawUtils.h"

IMPLEMENT_CLASS(UAnimObject);
IMPLEMENT_CLASS(UAnimNode);

IMPLEMENT_CLASS(UAnimNodeBlendBase);
IMPLEMENT_CLASS(UAnimNodeBlend);
IMPLEMENT_CLASS(UAnimNodeCrossfader);
IMPLEMENT_CLASS(UAnimNodeBlendPerBone);
IMPLEMENT_CLASS(UAnimNode_MultiBlendPerBone);
IMPLEMENT_CLASS(UAnimNodeBlendList);
IMPLEMENT_CLASS(UAnimNodeRandom);
IMPLEMENT_CLASS(UAnimNodeBlendDirectional);
IMPLEMENT_CLASS(UAnimNodeBlendByPosture);
IMPLEMENT_CLASS(UAnimNodeBlendByPhysics);
IMPLEMENT_CLASS(UAnimNodeBlendBySpeed);
IMPLEMENT_CLASS(UAnimNodeBlendByBase);
IMPLEMENT_CLASS(UAnimNodeMirror);
IMPLEMENT_CLASS(UAnimNodeBlendMultiBone);
IMPLEMENT_CLASS(UAnimNodeAimOffset);
IMPLEMENT_CLASS(UAnimNodeSynch);
IMPLEMENT_CLASS(UAnimNodeScalePlayRate);
IMPLEMENT_CLASS(UAnimNodeScaleRateBySpeed);
IMPLEMENT_CLASS(UAnimNodePlayCustomAnim);
IMPLEMENT_CLASS(UAnimNodeSlot);
IMPLEMENT_CLASS(UAnimTree);
IMPLEMENT_CLASS(UAnimNodeAdditiveBlending);
IMPLEMENT_CLASS(UAnimNodeBlendByProperty);

IMPLEMENT_CLASS(UAnimNodeFrame);

UBOOL UAnimNode::bNodeSearching = FALSE;
INT UAnimNode::CurrentSearchTag = 0;
/** Array to keep track of those nodes requiring an actual clear of the cache */
TArray<UAnimNode*> UAnimNode::NodesRequiringCacheClear;

static const FLOAT	AnimDebugHozSpace = 40.0f;
static const FLOAT  AnimDebugVertSpace = 20.0f;

/** Anim stats */
DECLARE_STATS_GROUP(TEXT("Anim"),STATGROUP_Anim);

/****
SeklCompTick

   Anim Tick

   UpdatSkelPose
      GetBoneAtoms
          Anim Decompression
          Blend (the remainder)  (aim offsets)
      ComposeSkeleton  (local to component space)  
          Skel Control

   Update RBBones

   -->  Physics Engine here <--

   Update Transform (for anything that is dirty (e.g. it was moved above))
       BlendinPhysics 
       MeshObject Update  (vertex math and copying!)  (send all bones to render thread)
****/
DECLARE_CYCLE_STAT(TEXT("SkelCtrl_FootPlacement"),STAT_SkelCtrl_FootPlacement,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("  GetFloorConformNormal"),STAT_GetFloorConformNormal,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("UpdateFloorConform"),STAT_UpdateFloorConform,STATGROUP_Anim);
// Foot Placement
DECLARE_CYCLE_STAT(TEXT("SkelComp UpdateChildComponents"),STAT_UpdateChildComponents,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("  Update SkelMesh Bounds"),STAT_UpdateSkelMeshBounds,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("  MeshObject Update"),STAT_MeshObjectUpdate,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("  BlendInPhysics"),STAT_BlendInPhysics,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("SkelComp UpdateTransform"),STAT_SkelCompUpdateTransform,STATGROUP_Anim);
//                         -->  Physics Engine here <--
DECLARE_CYCLE_STAT(TEXT("  Update RBBones"),STAT_UpdateRBBones,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("      SkelControl"),STAT_SkelControl,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("    Compose Skeleton"),STAT_SkelComposeTime,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("    UpdateFaceFX"),STAT_UpdateFaceFX,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("      Anim Decompression"),STAT_GetAnimationPose,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("      Mirror BoneAtoms"),STAT_MirrorBoneAtoms,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("    GetBoneAtoms"),STAT_AnimBlendTime,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("  UpdateSkelPose"),STAT_UpdateSkelPose,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("    Tick SkelControls"),STAT_SkelControlTickTime,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("    Sync Groups"),STAT_AnimSyncGroupTime,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("  Anim Tick Time"),STAT_AnimTickTime,STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("SkelComp Tick Time"),STAT_SkelComponentTickTime,STATGROUP_Anim);

DECLARE_DWORD_COUNTER_STAT(TEXT("SkelComp Tick Cnt"),STAT_SkelComponentTickCount,STATGROUP_Anim);
DECLARE_DWORD_COUNTER_STAT(TEXT("  TickNodes Cnt"),STAT_SkelComponentTickNodesCount,STATGROUP_Anim);
DECLARE_DWORD_COUNTER_STAT(TEXT("  GetBoneAtoms Cnt"),STAT_SkelComponentTickGBACount,STATGROUP_Anim);


IMPLEMENT_COMPARE_CONSTREF( BYTE, UnAnimTree, { return (A - B); } )

#if PERF_ENABLE_GETBONEATOM_STATS
TArray<FAnimNodeTimeStat> BoneAtomBlendStats;
TMap<FName, INT> BoneAtomBlendStatsTMap;
#endif

#if PERF_ENABLE_INITANIM_STATS
TArray<FAnimNodeTimeStat> InitAnimStats;
TMap<FName, INT> InitAnimStatsTMap;
#endif

#if PERF_SHOW_COPYANIMTREE_TIMES
TArray<FAnimNodeTimeStat> PostAnimNodeInstanceStats;
TMap<FName, INT> PostAnimNodeInstanceStatsTMap;
IMPLEMENT_COMPARE_CONSTREF( FAnimNodeTimeStat, UnAnimTree, { return (B.NodeExclusiveTime > A.NodeExclusiveTime) ? 1 : -1; } )
#endif

/** Template to use optimized bigblock memcpy to copy arrays of POD data */
template<typename T, typename A1, typename A2>
static void FastBoneArrayCopy(TArray<T,A1>& Dest, const TArray<T,A2>& Src)
{
	INT Num = Src.Num();
	Dest.Empty(Num);
	Dest.Add(Num);
	appBigBlockMemcpy(Dest.GetData(), Src.GetData(), sizeof(T) * Num);
}


// This define will be obsolete once content is saved with this
// if you did not save content with this yet, and if you'd like to see if this caused any issue
// undefine this should work
// if you'd like to go back to prior to this, and if you already saved content with this, 
// you will need to re-link all slot nodes children. 
// TODO: remove thie define!
#define USE_SLOTNODE_ANIMSEQPOOL 1

#define DEBUG_SLOTNODE_ANIMSEQPOOL 0

#if USE_SLOTNODE_ANIMSEQPOOL

/** AnimNodeSlot : AnimNodeSequence pool 
	
	If you use this system, it will remove current AnimNodeSequence of (child 1-(N-1) of N), and use one on demand from pool
	That way, we don't have to keep many AnimNodeSequence in memory but use it when it only needed 
	This pool is shared between any pawn **/
class FSlotNodeAnimSequencePool
{
	enum ECacheStatus
	{
		Unused = 0,
		Claimed = 1,
		Used = 2,
		Released = 3
	};

	/** Each element contains this information 
		@param SeqNode: AnimNodeSequence
		@param bUsed: Used or not **/
	struct FSlotNodeSeqCache
	{
		UAnimNodeSequence * SeqNode;
		/** Cache Status **/
		ECacheStatus Status;
	};

	// Array of elements
	TArray<FSlotNodeSeqCache> AnimSlotNodeSequencePool;
public:
	FSlotNodeAnimSequencePool() {}

	/*
	 * GetSlotNodeSequence	: Return AnimNodeSequence from pool
	 *
	 * @param SkelComponent	: The owner skelcomponent
	 * @param ParentNode	: Parent Node of the current AnimNode
	 * @param bClaimOnly	: Since sometimes, return/release can be mixed up due to OnAimEnd, and PlayCustomAnim also calling SetActiveChild
								If you claim only, you need to commit later, so that it doesn't try to remove that just has been claimed
								By default, in Matinee, it will use bClaimOnly = FALSE, in game, we will use ClaimOnly
								To make sure it's not trying to release that has been claimed and ready to use
	  */
	UAnimNodeSequence * GetAnimNodeSequence( USkeletalMeshComponent * SkelComponent, UAnimNodeSlot * ParentNode, UBOOL bClaimOnly=FALSE );

	/*
	 * MarkAsClaimed	:	When it reuses same child, it's not getting marked as claimed
	 *						This function re-marks as claimed
	 */
	void MarkAsClaimed( USkeletalMeshComponent * SkelComponent, UAnimNodeSequence * Node );

	/*
	 * CommitToUse	:	If you only claimed (by GetSlotNodeSequence) , you need to commit to use
	 *					If not it won't be released when you release
	 */
	UBOOL CommitToUse( USkeletalMeshComponent * SkelComponent, UAnimNodeSequence * Node );
	/*
	 * ReleaseSlotNodeSequence	: Relase Slot Node to pool
	 * 
	 * @param Node		: AnimNodeSequence to release
	 */
	void ReleaseSlotNodeSequence(UAnimNodeSequence * Node);
	/** 
	 * FlushReleasedSlots : Clear Released slots for the input SkeletalMeshComponent 
	 * 
	 * You can just clear when released since there are references to the node from notifiers, or even scripts or so
	 * So I need to hold it to be released and clear it later
	 * Whenever new tick starts, it will clear for released nodes from previous tick
	 */
	void FlushReleasedSlots(const USkeletalMeshComponent * SkelComponent);

	/**
	 * ReleaseAllSlotNodeSequences : Release All AnimNodeSequences that belongs to SkelComponent
	 * 
	 * This is called when DeleteAnimTree or when component gets detached
	 * To make sure it's all released before it gets removed
	 */
	void ReleaseAllSlotNodeSequences(const USkeletalMeshComponent * SkelComponent);

	/** ResetAnimNodeSequencePool : Clear all of items in the pool
	 * 
	 * Called when world is cleaned up 
	 */
	void ResetAnimNodeSequencePool();

	/** PrintAnimNodeSequencePool : Print AnimNodeSequence pool 
	 *
	 * List out all items in the pool that are used/claimed/release
	 */
	void PrintAnimNodeSequencePool();

#if DEBUG_SLOTNODE_ANIMSEQPOOL
	/** Debug purpose : calls PrintAnimNodeSequencePool with Output info **/
	void DebugOutputAnimNodeSequencePool(const FString & Output);
#endif 

	/** Debug purpose : verify if this sequence is in pool and marked as used **/
	UBOOL VerifySequenceNodeIsBeingUsed(UAnimNodeSequence * Node);

private: 
	/** Create Slot Node Sequence **/
	UAnimNodeSequence * CreateAnimNodeSequence()
	{
		UAnimNodeSequence * NewSeqNode = ConstructObject<UAnimNodeSequence>( UAnimNodeSequence::StaticClass() );
		// otherwise, OOM
		check (NewSeqNode);
		// add to root so that it doesn't get GCed
		NewSeqNode->AddToRoot();

		return NewSeqNode;
	}

	/** Grow pool to the size of chunk **/
	INT Grow()
	{
		// grow chunk size
		const INT GrowChunkSize = 10;

		INT NumOfSlotNode = AnimSlotNodeSequencePool.Num();
		AnimSlotNodeSequencePool.AddZeroed(GrowChunkSize);
		return NumOfSlotNode;
	}

	/** Find Index from Sequence Node and return the Index **/
	INT FindIndexFromSeqNode(const UAnimNodeSequence * Node)
	{
		for ( INT I=0; I<AnimSlotNodeSequencePool.Num(); ++I )	
		{
			if ( AnimSlotNodeSequencePool(I).SeqNode == Node )
			{
				return I;
			}
		}

		return INDEX_NONE;
	}

	/** Release Anim Node Sequence 
	 * If Index is given, use that index 
	 */
	void ReleaseAnimNodeSequence( UAnimNodeSequence * Node, INT Index=INDEX_NONE )
	{
		// if node is null, it didn't belong here
		if (!Node)
		{
			return;
		}

#if DEBUG_SLOTNODE_ANIMSEQPOOL
		USkeletalMeshComponent * SkelComponent = Node->SkelComponent;
		DebugOutputAnimNodeSequencePool(FString::Printf(TEXT("Releasing SlotNode %x (%s) for SkeletalMesh %s"), Node, *Node->AnimSeqName.GetNameString(), (SkelComponent)?*SkelComponent->SkeletalMesh->GetName(): TEXT("NONE")));
#endif 

		INT IndexToRelease = Index;

		// for now brute forse
		if ( AnimSlotNodeSequencePool.IsValidIndex(IndexToRelease) == FALSE )
		{
			IndexToRelease = FindIndexFromSeqNode(Node);
		}

		if ( IndexToRelease != INDEX_NONE )
		{
			// if find animnode sequence and make sure it's used. Do not release that's marked as Claimed or anything else
			// this can be called more than once 1. from deleteanimtree, 2. from tick or when anim is deactivated
			// when onanimend gets called, it can call deleteanimtree so after onanimend, if you release, it should already have been deactivated
			// TODO: we might need better system to reduce complexity of this all issue
			// onanimend triggers so many things that it's hard to predict state of animnode after onanimend. 
			if ( AnimSlotNodeSequencePool(IndexToRelease).Status == Used )
			{
				// if parent has links to me, remove it here
				for ( INT ParentID=0; ParentID<Node->ParentNodes.Num(); ++ParentID )
				{
					UAnimNodeSlot * SlotNode = Cast<UAnimNodeSlot>(Node->ParentNodes(ParentID));
					if (SlotNode)
					{
						// now go through find which one is me
						for ( INT ChildID=0; ChildID<SlotNode->Children.Num(); ++ChildID )
						{
							if ( SlotNode->Children(ChildID).Anim == Node )
							{
								// clear link to it
								SlotNode->Children(ChildID).Anim = NULL;
								SlotNode->Children(ChildID).bIsAdditive = FALSE;
								SlotNode->Children(ChildID).bMirrorSkeleton = FALSE;

								// as for current system, you can't have more than one same child
								break;
							}
						}
					}
				}

				// Mark as released
				AnimSlotNodeSequencePool(IndexToRelease).Status = Released;
			}
#if DEBUG_SLOTNODE_ANIMSEQPOOL
			else 
			{
				debugf(TEXT("[%s] Trying to release that has been claimed only [%x]"), (SkelComponent && SkelComponent->SkeletalMesh)? *SkelComponent->SkeletalMesh->GetName(): TEXT("None"));
			}
#endif
		}
	}

	/** Clear Given Index Seq Node Information **/
	void ClearAnimNodeSequence(INT Index)
	{
		AnimSlotNodeSequencePool(Index).Status = Unused;
		if (AnimSlotNodeSequencePool(Index).SeqNode)
		{
			UAnimNodeSequence * SeqNodeToClear = AnimSlotNodeSequencePool(Index).SeqNode; 
			// remove any reference you can find -otherwise during streaming it can be still around
			// calling InitializeProperties sound great idea, but that defeats purpose of using pool since it cleans up all properties. 
			// this is fast way of removing all reference you'd need
			SeqNodeToClear->SetAnim(NAME_None);
			SeqNodeToClear->SkelComponent = NULL;
			SeqNodeToClear->ParentNodes.Empty();
			SeqNodeToClear->ClearCachedResult();
			SeqNodeToClear->MetaDataSkelControlList.Empty();
			SeqNodeToClear->CameraAnim = NULL;
			SeqNodeToClear->CurrentTime = 0.f;
			SeqNodeToClear->PreviousTime = 0.f;
			SeqNodeToClear->bPlaying = FALSE;
			SeqNodeToClear->bRelevant = FALSE;
			SeqNodeToClear->bJustBecameRelevant = FALSE;
			SeqNodeToClear->bTickDuringPausedAnims = FALSE;
			SeqNodeToClear->bDisableCaching = FALSE;
			SeqNodeToClear->NodeTotalWeight = 0.f;
		}
	}
};

/** Global variable for pool **/
static FSlotNodeAnimSequencePool GAnimSlotNodeSequencePool;

/*
 * GetSlotNodeSequence	: Return AnimNodeSequence from pool
 *
 * @param SkelComponent	: The owner skelcomponent
 * @param ParentNode	: Parent Node of the current AnimNode
 * @param bClaimOnly	: Since sometimes, return/release can be mixed up due to OnAimEnd, and PlayCustomAnim also calling SetActiveChild
							If you claim only, you need to commit later, so that it doesn't try to remove that just has been claimed
							By default, in Matinee, it will use bClaimOnly = FALSE, in game, we will use ClaimOnly
							To make sure it's not trying to release that has been claimed and ready to use
  */
UAnimNodeSequence * FSlotNodeAnimSequencePool::GetAnimNodeSequence( USkeletalMeshComponent * SkelComponent, UAnimNodeSlot * ParentNode, UBOOL bClaimOnly )
{
	UAnimNodeSequence* NewSeqNode = NULL;

	// don't accept if pending kill
	if (!SkelComponent || SkelComponent->IsPendingKill())
	{
		return NULL;
	}

	// if Pool isn't full
	// for now brute forse
	for ( INT I=0; I<AnimSlotNodeSequencePool.Num(); ++I )	
	{
		if ( AnimSlotNodeSequencePool(I).Status == Unused )
		{
			if ( bClaimOnly )
			{
				AnimSlotNodeSequencePool(I).Status = Claimed;
			}
			else
			{
				AnimSlotNodeSequencePool(I).Status = Used;
			}

			// if sequence isn't found yet, create new one
			if (AnimSlotNodeSequencePool(I).SeqNode==NULL)
			{
				NewSeqNode = CreateAnimNodeSequence();
				AnimSlotNodeSequencePool(I).SeqNode = NewSeqNode;
			}
			else
			{
				// if already assigned use one
				NewSeqNode = AnimSlotNodeSequencePool(I).SeqNode;
			}

			// found one, get out
			break;
		}
 		else 
 		{
 			// make sure it's still there - remove this 
 			//check (AnimSlotNodeSequencePool(I).SeqNode && AnimSlotNodeSequencePool(I).SeqNode->SkelComponent!=NULL);
 		}
	}

	// iterated through all list, but didn't find any, pool is full
	if (NewSeqNode == NULL)
	{
		// grow the list
		INT NumOfSlotNode = Grow();

		// construct first element
		NewSeqNode = CreateAnimNodeSequence();
		AnimSlotNodeSequencePool(NumOfSlotNode).SeqNode = NewSeqNode;
		AnimSlotNodeSequencePool(NumOfSlotNode).Status = Used;

	}

	// now we have new Seq Node, initialize
	if (NewSeqNode)
	{
#if DEBUG_SLOTNODE_ANIMSEQPOOL
		// This function already has it, but I'd like to avoid Printf if possible
		DebugOutputAnimNodeSequencePool(FString::Printf(TEXT("Returning SlotNode %x for SkeletalMesh %s"), NewSeqNode, (SkelComponent)?*SkelComponent->SkeletalMesh->GetName(): TEXT("NONE")));
#endif

		// Set Skelcomponent, and initialize and add ParentNodes
		// These all happens in InitTree
		NewSeqNode->SkelComponent = SkelComponent;
		NewSeqNode->InitAnim(SkelComponent, ParentNode);
		NewSeqNode->SynchGroupName = NAME_None;
		NewSeqNode->ParentNodes.Empty();
		NewSeqNode->ParentNodes.AddUniqueItem(ParentNode);
	}

	return NewSeqNode;
}

/*
 * MarkAsClaimed	:	When it reuses same child, it's not getting marked as claimed
 *						This function re-marks as claimed
 */
void FSlotNodeAnimSequencePool::MarkAsClaimed( USkeletalMeshComponent * SkelComponent, UAnimNodeSequence * Node )
{
	// if node is null, it didn't belong here
	if (!Node || !SkelComponent)
	{
		return;
	}

	// for now brute force search 
	INT I = FindIndexFromSeqNode(Node);

	// if index is valid
	if ( I != INDEX_NONE )
	{
		// make sure skelcomponent matches - just in case somebody else is trying to commit that doesn't belong to it
		check ( Node->SkelComponent == SkelComponent );

		// if find animnode sequence
		if ( AnimSlotNodeSequencePool(I).Status == Used )
		{
			AnimSlotNodeSequencePool(I).Status = Claimed;
		}
	}

}

/*
 * CommitToUse	:	If you only claimed (by GetSlotNodeSequence) , you need to commit to use
 *					If not it won't be released when you release
 */
UBOOL FSlotNodeAnimSequencePool::CommitToUse( USkeletalMeshComponent * SkelComponent, UAnimNodeSequence * Node )
{
	// if node is null, it didn't belong here
	if (!Node || !SkelComponent)
	{
		return FALSE;
	}

	// for now brute force search 
	INT I = FindIndexFromSeqNode(Node);

	// if index is valid
	if ( I != INDEX_NONE )
	{
		// make sure skelcomponent matches - just in case somebody else is trying to commit that doesn't belong to it
		check ( Node->SkelComponent == SkelComponent );

		// if find animnode sequence
		if ( AnimSlotNodeSequencePool(I).Status == Claimed )
		{
			AnimSlotNodeSequencePool(I).Status = Used;
		}

		return TRUE;
	}

	return FALSE;
}

/** Debug purpose : verify if this sequence is in pool and marked as used **/
UBOOL FSlotNodeAnimSequencePool::VerifySequenceNodeIsBeingUsed(UAnimNodeSequence * Node)
{
	// if node is null, it didn't belong here
	if (!Node)
	{
		return FALSE;
	}
	// for now brute force search 
	INT I = FindIndexFromSeqNode(Node);

	// if index is valid
	if ( I != INDEX_NONE )
	{
		// if find animnode sequence
		if ( AnimSlotNodeSequencePool(I).Status == Used && AnimSlotNodeSequencePool(I).SeqNode == Node )
		{
			return TRUE;
		}		
	}

	return FALSE;
}

/*
 * ReleaseSlotNodeSequence	: Relase Slot Node to pool
 * 
 * @param Node		: AnimNodeSequence to release
 */
void FSlotNodeAnimSequencePool::ReleaseSlotNodeSequence(UAnimNodeSequence * Node)
{
	ReleaseAnimNodeSequence( Node );
}

/** PrintAnimNodeSequencePool : Print AnimNodeSequence pool 
 *
 * List out all items in the pool that are used/claimed/release
 */
void FSlotNodeAnimSequencePool::PrintAnimNodeSequencePool()
{
	UAnimNodeSequence * NodeSeq=NULL;
	INT	TotalUsedNumber = 0;
	INT TotalReleaseNumber = 0;
	INT TotalEverUsedNumber = 0;

	debugf(TEXT("[Used List]"));
	for ( INT I=0; I<AnimSlotNodeSequencePool.Num(); ++I )	
	{
		if (AnimSlotNodeSequencePool(I).Status == Used)
		{
			NodeSeq = AnimSlotNodeSequencePool(I).SeqNode;
			debugf( TEXT("[%s](%d) Anim Sequence (%s) (%x)"), 
				(NodeSeq->SkelComponent && NodeSeq->SkelComponent->SkeletalMesh)? *NodeSeq->SkelComponent->SkeletalMesh->GetName():TEXT("NONE"), 
				I+1,
				*NodeSeq->AnimSeqName.GetNameString(), 
				NodeSeq);
			TotalUsedNumber++;
		}

		if (AnimSlotNodeSequencePool(I).SeqNode != 0)
		{
			// ever used?
			TotalEverUsedNumber++;
		}
	}

	debugf(TEXT("[Claimed List]"));
	for ( INT I=0; I<AnimSlotNodeSequencePool.Num(); ++I )	
	{
		if (AnimSlotNodeSequencePool(I).Status == Claimed)
		{
			NodeSeq = AnimSlotNodeSequencePool(I).SeqNode;
			debugf( TEXT("[%s](%d) Anim Sequence (%s) (%x)"), 
				(NodeSeq->SkelComponent && NodeSeq->SkelComponent->SkeletalMesh)? *NodeSeq->SkelComponent->SkeletalMesh->GetName():TEXT("NONE"), 
				I+1,
				*NodeSeq->AnimSeqName.GetNameString(), 
				NodeSeq);
		}
	}

	debugf(TEXT("[Released List]"));

	for ( INT I=0; I<AnimSlotNodeSequencePool.Num(); ++I )	
	{
		if (AnimSlotNodeSequencePool(I).Status == Released)
		{
			NodeSeq = AnimSlotNodeSequencePool(I).SeqNode;
			debugf( TEXT("[%s](%d) Anim Sequence (%s) "), 
				(NodeSeq->SkelComponent && NodeSeq->SkelComponent->SkeletalMesh)? *NodeSeq->SkelComponent->SkeletalMesh->GetName():TEXT("NONE"), 
				I+1,
				*NodeSeq->AnimSeqName.GetNameString());
		}
	}

	INT TotalPoolSize = AnimSlotNodeSequencePool.Num();

	debugf(TEXT("Total Node Number [%d], Used Node Number [%d](%0.2f), Max Used Node Number [%d](%0.2f)"), 
			TotalPoolSize,  TotalUsedNumber, (FLOAT)TotalUsedNumber/(FLOAT)TotalPoolSize, TotalEverUsedNumber, (FLOAT)TotalEverUsedNumber/(FLOAT)TotalPoolSize);
}
/** 
 * FlushReleasedSlots : Clear Released slots for the input SkeletalMeshComponent 
 * 
 * You can just clear when released since there are references to the node from notifiers, or even scripts or so
 * So I need to hold it to be released and clear it later
 * Whenever new tick starts, it will clear for released nodes from previous tick
 */
void FSlotNodeAnimSequencePool::FlushReleasedSlots(const USkeletalMeshComponent * SkelComponent)
{
	// for now brute forse
	for ( INT I=0; I<AnimSlotNodeSequencePool.Num(); ++I )	
	{
		// if Released and SkelComponent matches to the inputSkelComponent
		if ( AnimSlotNodeSequencePool(I).Status == Released && 
			AnimSlotNodeSequencePool(I).SeqNode->SkelComponent == SkelComponent )
		{
#if DEBUG_SLOTNODE_ANIMSEQPOOL
			debugf(TEXT("%s mesh is clearing slotnode(%x)"), (SkelComponent)?*SkelComponent->SkeletalMesh->GetName(): TEXT("NONE"), AnimSlotNodeSequencePool(I).SeqNode);
#endif
			ClearAnimNodeSequence(I);

			// If we have SkelComponent, make sure it removes from SyncGroup
			if (SkelComponent)
			{
				UAnimTree * AnimTree = Cast<UAnimTree>(SkelComponent->Animations);
				if (AnimTree)
				{
					AnimTree->RemoveFromSyncGroup(AnimSlotNodeSequencePool(I).SeqNode);
				}
			}
		}
	}
}

/**
 * ReleaseAllSlotNodeSequences : Release All AnimNodeSequences that belongs to SkelComponent
 * 
 * This is called when DeleteAnimTree or when component gets detached
 * To make sure it's all released before it gets removed
 */
void FSlotNodeAnimSequencePool::ReleaseAllSlotNodeSequences(const USkeletalMeshComponent * SkelComponent)
{
#if DEBUG_SLOTNODE_ANIMSEQPOOL
	DebugOutputAnimNodeSequencePool(FString::Printf(TEXT("%s mesh is releasing all slotnodes"), (SkelComponent)?*SkelComponent->SkeletalMesh->GetName(): TEXT("NONE")));
#endif

	// for now brute forse
	for ( INT I=0; I<AnimSlotNodeSequencePool.Num(); ++I )	
	{
		// if used, release it
		if ( AnimSlotNodeSequencePool(I).Status == Used && 
			(AnimSlotNodeSequencePool(I).SeqNode->SkelComponent == SkelComponent) )
		{
			UAnimNodeSequence * Node = AnimSlotNodeSequencePool(I).SeqNode;
			ReleaseAnimNodeSequence(Node, I);
		}
	}
}

#if DEBUG_SLOTNODE_ANIMSEQPOOL
/** Debug purpose : calls PrintAnimNodeSequencePool with Output info **/
void FSlotNodeAnimSequencePool::DebugOutputAnimNodeSequencePool(const FString & Output)
{
	debugf(TEXT("==========================================="));
	debugf(*Output);
	debugf(TEXT("==========================================="));
	PrintAnimNodeSequencePool();
	debugf(TEXT("==========================================="));
}
#endif 
/** ResetAnimNodeSequencePool : Clear all of items in the pool
 * 
 * Called when world is cleaned up 
 */
void FSlotNodeAnimSequencePool::ResetAnimNodeSequencePool()
{
#if DEBUG_SLOTNODE_ANIMSEQPOOL
	DebugOutputAnimNodeSequencePool(TEXT("Clear Pool"));
#endif

	// for now brute forse
	for ( INT I=0; I<AnimSlotNodeSequencePool.Num(); ++I )	
	{
		// in this only case, we release 
		if (AnimSlotNodeSequencePool(I).Status == Used)
		{
			ReleaseAnimNodeSequence(AnimSlotNodeSequencePool(I).SeqNode, I);
		}

		// if seqnode exists, remove from root, so that it can be GCed
		if ( AnimSlotNodeSequencePool(I).SeqNode )
		{
			AnimSlotNodeSequencePool(I).SeqNode->RemoveFromRoot();
		}

		// clear
		ClearAnimNodeSequence(I);
	}

	// empty the pool
	AnimSlotNodeSequencePool.Empty();
}

#endif //USE_SLOTNODE_ANIMSEQPOOL

#if 0
static void CheckAtomOutput(FBoneAtomArray& Output, UAnimNode* Node)
{
	for(INT i=0; i<Output.Num(); i++)
	{
		FVector Translation(Output(i).GetTranslation());
		if(Translation.Size() > WORLD_MAX)
		{
			debugf(TEXT("CHILD: %s - T %s"), *Node->GetName(), *Translation.ToString() );
		}
	}
}
#endif

///////////////////////////////////////
//////////// FBoneAtom ////////////////
///////////////////////////////////////

// BoneAtom identity
const FBoneAtom FBoneAtom::Identity(FQuat(0.f,0.f,0.f,1.f), FVector(0.f, 0.f, 0.f), 1.f);

/**
* Does a debugf of the contents of this BoneAtom.
*/
void FBoneAtom::DebugPrint() const
{
	debugf(TEXT("%s"), *ToString());
}

FString FBoneAtom::ToString() const
{
	FQuat R(GetRotation());
	FVector T(GetTranslation());
	FLOAT S(GetScale());

	FString Output= FString::Printf(TEXT("Rotation: %f %f %f %f\r\n"), R.X, R.Y, R.Z, R.W);
	Output += FString::Printf(TEXT("Translation: %f %f %f\r\n"), T.X, T.Y, T.Z);
	Output += FString::Printf(TEXT("Scale: %f\r\n"), S);

	return Output;
}

///////////////////////////////////////
//////////// UAnimNode ////////////////
///////////////////////////////////////

/** Get notification that this node has become relevant for the final blend. ie TotalWeight is now > 0 */
void UAnimNode::OnBecomeRelevant()
{
	if( bCallScriptEventOnBecomeRelevant )
	{
		eventOnBecomeRelevant();
	}
}

/** Get notification that this node is no longer relevant for the final blend. ie TotalWeight is now == 0 */
void UAnimNode::OnCeaseRelevant()
{
	if( bCallScriptEventOnCeaseRelevant )
	{
		eventOnCeaseRelevant();
	}
}

/** 
 * Clear Cached Result 
 **/
void UAnimNode::ClearCachedResult()
{
	// clear it 

	CachedBoneAtoms.Empty();
	CachedCurveKeys.Empty();
	CachedNumDesiredBones = 0;
}

/**
 * Do any initialization. Should not reset any state of the animation tree so should be safe to call multiple times.
 * However, the SkeletalMesh may have changed on the SkeletalMeshComponent, so should update any data that relates to that.
 * 
 * @param meshComp SkeletalMeshComponent to which this node of the tree belongs.
 * @param Parent Parent blend node (will be NULL for root note).
 */
void UAnimNode::InitAnim(USkeletalMeshComponent* MeshComp, UAnimNodeBlendBase* Parent)
{
	// Reset ticking information
	NodeTickTag = MeshComp->TickTag;
	NodeTotalWeight = 0.f;
	bRelevant = FALSE;
	bJustBecameRelevant = FALSE;

	if( bCallScriptEventOnInit )
	{
		eventOnInit();
	}
}

/**
 * Fills the Atoms array with the specified skeletal mesh reference pose.
 * 
 * @param OutAtoms			[out] Output array of relative bone transforms. Must be the same length as RefSkel when calling function.
 * @param DesiredBones		Indices of bones we want to modify. Parents must occur before children.
 * @param RefSkel			Input reference skeleton to create atoms from.
 */
void UAnimNode::FillWithRefPose(TArray<FBoneAtom>& OutAtoms, const TArray<BYTE>& DesiredBones, const TArray<struct FMeshBone>& RefSkel)
{
	check( OutAtoms.Num() == RefSkel.Num() );

	for(INT i=0; i<DesiredBones.Num(); i++)
	{
		const INT BoneIndex				= DesiredBones(i);
		const FMeshBone& RefSkelBone	= RefSkel(BoneIndex);
		FBoneAtom& OutAtom				= OutAtoms(BoneIndex);

		OutAtom.SetComponents(
			RefSkelBone.BonePos.Orientation,
			RefSkelBone.BonePos.Position);
	}
}

void UAnimNode::FillWithRefPose(FBoneAtomArray& OutAtoms, const TArray<BYTE>& DesiredBones, const TArray<struct FMeshBone>& RefSkel)
{
	check( OutAtoms.Num() == RefSkel.Num() );

	for(INT i=0; i<DesiredBones.Num(); i++)
	{
		const INT BoneIndex				= DesiredBones(i);
		const FMeshBone& RefSkelBone	= RefSkel(BoneIndex);
		FBoneAtom& OutAtom				= OutAtoms(BoneIndex);

		OutAtom.SetComponents(
			RefSkelBone.BonePos.Orientation,
			RefSkelBone.BonePos.Position);
	}
}

/** 
 *	Utility for taking an array of bone indices and ensuring that all parents are present 
 *	(ie. all bones between those in the array and the root are present). 
 *	Note that this must ensure the invariant that parent occur before children in BoneIndices.
 */
void UAnimNode::EnsureParentsPresent(TArray<BYTE>& BoneIndices, USkeletalMesh* SkelMesh)
{
	// Iterate through existing array.
	INT i=0;
	while( i<BoneIndices.Num() )
	{
		const BYTE BoneIndex = BoneIndices(i);

		// For the root bone, just move on.
		if( BoneIndex > 0 )
		{
#if	!FINAL_RELEASE
			// Warn if we're getting bad data.
			// Bones are matched as INT, and a non found bone will be set to INDEX_NONE == -1
			// This will be turned into a 255 BYTE
			// This should never happen, so if it does, something is wrong!
			if( BoneIndex >= SkelMesh->RefSkeleton.Num() )
			{
				debugf(TEXT("UAnimNode::EnsureParentsPresent, BoneIndex >= SkelMesh->RefSkeleton.Num()."));
				i++;
				continue;
			}
#endif
			const BYTE ParentIndex = SkelMesh->RefSkeleton(BoneIndex).ParentIndex;

			// If we do not have this parent in the array, we add it in this location, and leave 'i' where it is.
			if( !BoneIndices.ContainsItem(ParentIndex) )
			{
				BoneIndices.Insert(i);
				BoneIndices(i) = ParentIndex;
			}
			// If parent was in array, just move on.
			else
			{
				i++;
			}
		}
		else
		{
			i++;
		}
	}
}


/**
 * Get the set of bone 'atoms' (ie. transform of bone relative to parent bone) generated by the blend subtree starting at this node.
 * 
 * @param	Atoms			Output array of bone transforms. Must be correct size when calling function - that is one entry for each bone. Contents will be erased by function though.
 * @param	DesiredBones	Indices of bones that we want to return. Note that bones not in this array will not be modified, so are not safe to access! Parents must occur before children.
 */
void UAnimNode::GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	START_GETBONEATOM_TIMER

	// No root motion here, move along, nothing to see...
	RootMotionDelta.SetIdentity();
	bHasRootMotion	= 0;

	const INT NumAtoms = SkelComponent->SkeletalMesh->RefSkeleton.Num();
	check(NumAtoms == Atoms.Num());
	FillWithRefPose(Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
}

/** 
 *	Will copy the cached results into the OutAtoms array if they are up to date and return TRUE 
 *	If cache is not up to date, does nothing and returns FALSE.
 */
UBOOL UAnimNode::GetCachedResults(FBoneAtomArray& OutAtoms, FBoneAtom& OutRootMotionDelta, INT& bOutHasRootMotion, FCurveKeyArray& OutCurveKeys, INT NumDesiredBones)
{
	// See if results are cached, and cached array is the same size as the target array.
	if( !bDisableCaching 
		&& NodeCachedAtomsTag == SkelComponent->CachedAtomsTag 
		&& CachedBoneAtoms.Num() == OutAtoms.Num() 
		&& CachedNumDesiredBones == NumDesiredBones )
	{
		FastBoneArrayCopy(OutAtoms,CachedBoneAtoms);
		OutCurveKeys += CachedCurveKeys;
		OutRootMotionDelta = CachedRootMotionDelta;
		bOutHasRootMotion = bCachedHasRootMotion;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

UBOOL UAnimNode::ShouldSaveCachedResults()
{
	// If we have two or more relevant parents, cache results
	if( ParentNodes.Num() > 1 )
	{
		INT NumRelevantParents = 0;
		UAnimNode::CurrentSearchTag++;
		for(INT i=0; i<ParentNodes.Num() && NumRelevantParents<2; i++)
		{
			UAnimNodeBlendBase* ParentNode = ParentNodes(i);

			// Touch parent nodes only once...
			if( ParentNode->SearchTag == UAnimNode::CurrentSearchTag )
			{
				continue;
			}

			ParentNode->SearchTag = UAnimNode::CurrentSearchTag;

			if( ParentNode->bRelevant )
			{
				// Find the matching child weight... Because the parent could be relevant, but not that child.
				for(INT ChildIdx=0; ChildIdx<ParentNode->Children.Num(); ChildIdx++)
				{
					if( ParentNode->Children(ChildIdx).Anim == this )
					{
						if( ParentNode->Children(ChildIdx).Weight * ParentNode->NodeTotalWeight > ZERO_ANIMWEIGHT_THRESH )
						{
							NumRelevantParents++;
						}
						break;
					}
				}
			}
		}
		return (NumRelevantParents >= 2);
	}

	// Do not cache results for nodes which only have one parent- it is unnecessary.
	return FALSE;
}

/** Save the supplied array of BoneAtoms in the CachedBoneAtoms. */
void UAnimNode::SaveCachedResults(const FBoneAtomArray& NewAtoms, const FBoneAtom& NewRootMotionDelta, INT bNewHasRootMotion, const FCurveKeyArray& NewCurveKeys, INT NumDesiredBones)
{
	check(SkelComponent);

	// If Caching is disabled, don't bother
	if( bDisableCaching )
	{
		return;
	}

	// We make sure the cache is empty.
	if( !ShouldSaveCachedResults() )
	{
		CachedBoneAtoms.Empty();
		CachedCurveKeys.Empty();
		CachedNumDesiredBones = 0;
	}
	else
	{
		FastBoneArrayCopy(CachedBoneAtoms,NewAtoms);
		FastBoneArrayCopy(CachedCurveKeys,NewCurveKeys);
		CachedRootMotionDelta = NewRootMotionDelta;
		bCachedHasRootMotion = bNewHasRootMotion;
		CachedNumDesiredBones = NumDesiredBones;
		if (!ShouldKeepCachedResult())
		{
			NodesRequiringCacheClear.AddItem(this);
		}
	}

	// Change flag to indicate cache is up to date
	NodeCachedAtomsTag = SkelComponent->CachedAtomsTag;
}

void UAnimNode::GetNodes(TArray<UAnimNode*>& Nodes, bool bForceTraversal/*=FALSE*/)
{
	if( SkelComponent && SkelComponent->AnimTickArray.Num() > 0 )
	{
		// If we're at the root, then we can just directly use the AnimTickArray, without having to traverse the tree.
		if( !bForceTraversal && SkelComponent->Animations == this )
		{
			Nodes = SkelComponent->AnimTickArray;
			return;
		}
		else
		{
			// Make sure we have reserved enough, so we don't pay allocation costs.
			Nodes.Empty( SkelComponent->AnimTickArray.Num() );
		}
	}

	// we can't start another search while we're already in one as it would invalidate SearchTags on the original search
	check(!UAnimNode::bNodeSearching);

	// set flag allowing GetNodesInternal()
	UAnimNode::bNodeSearching = TRUE;
	// increment search tag so all nodes in the tree will consider themselves once
	UAnimNode::CurrentSearchTag++;
	GetNodesInternal(Nodes);
	// reset flag
	UAnimNode::bNodeSearching = FALSE;
}

/**
 * Find all AnimNodes including and below this one in the tree. Results are arranged so that parents are before children in the array.
 * 
 * @param Nodes Output array of AnimNode pointers.
 */
void UAnimNode::GetNodesInternal(TArray<UAnimNode*>& Nodes)
{
	// make sure we're only called from inside GetNodes() or another GetNodesInternal()
	check(UAnimNode::bNodeSearching);

	if (SearchTag != UAnimNode::CurrentSearchTag)
	{
		SearchTag = UAnimNode::CurrentSearchTag;
		Nodes.AddItem(this);
	}
}

/** Add this node and all children of the specified class to array. Node are added so a parent is always before its children in the array. */
void UAnimNode::GetNodesByClass(TArray<UAnimNode*>& Nodes, UClass* BaseClass)
{
	TArray<UAnimNode*>* AllNodes;
	TArray<UAnimNode*> AlternateArray;

	// Directly use AnimTickArray if starting from the root
	if( SkelComponent && SkelComponent->Animations == this && SkelComponent->AnimTickArray.Num() > 0 )
	{
		AllNodes = &SkelComponent->AnimTickArray;
	}
	else
	{
		AllNodes = &AlternateArray;
		// Get Nodes.
		GetNodes( *AllNodes );
	}

	INT const NodeCount = AllNodes->Num();
	// preallocate enough for all nodes to avoid repeated realloc
	Nodes.Reset(NodeCount);
	for(INT i=0; i<NodeCount; i++)
	{
#if CONSOLE
		// prefetch the next iteration
		if( i+1 < NodeCount )
		{
			BYTE* RESTRICT Address = (BYTE* RESTRICT)(*AllNodes)(i+1);
			CONSOLE_PREFETCH(Address);
			CONSOLE_PREFETCH_NEXT_CACHE_LINE(Address);
		}
#endif
		UAnimNode* AnimNode =(*AllNodes)(i);
		if( AnimNode->IsA(BaseClass) )
		{
			Nodes.AddItem( AnimNode );
		}
	}
}

/** Return an array with all UAnimNodeSequence childs, including this node. */
void UAnimNode::GetAnimSeqNodes(TArray<UAnimNodeSequence*>& Nodes, FName InSynchGroupName)
{
	TArray<UAnimNode*> AllNodes;
	GetNodes(AllNodes);

	Nodes.Reserve(AllNodes.Num());
	for (INT i = 0; i < AllNodes.Num(); i++)
	{
		UAnimNodeSequence* SeqNode = Cast<UAnimNodeSequence>(AllNodes(i));
		if (SeqNode != NULL && (InSynchGroupName == NAME_None || InSynchGroupName == SeqNode->SynchGroupName))
		{
			Nodes.AddItem(SeqNode);
		}
	}
}

/** Find a node whose NodeName matches InNodeName. Will search this node and all below it. */
UAnimNode* UAnimNode::FindAnimNode(FName InNodeName)
{
	TArray<UAnimNode*> Nodes;
	this->GetNodes( Nodes );

	for(INT i=0; i<Nodes.Num(); i++)
	{
		checkSlow( Nodes(i) );
		if( Nodes(i)->NodeName == InNodeName )
		{
			return Nodes(i);
		}
	}

	return NULL;
}

/** Utility for counting the number of parents of this node that have been ticked. */
UBOOL UAnimNode::WereAllParentsTicked() const
{
	for(INT i=0; i<ParentNodes.Num(); i++)
	{
		if( ParentNodes(i)->NodeTickTag != SkelComponent->TickTag )
		{
			return FALSE;
		}
	}
	return TRUE;
}


/** Returns TRUE if this node is a child of given Node */
UBOOL UAnimNode::IsChildOf(UAnimNode* Node)
{
	check(Node);

	// Only touch nodes once
	UAnimNode::CurrentSearchTag++;
	return IsChildOf_Internal(Node);
}

/** Returns TRUE if this node is a child of Node */
UBOOL UAnimNode::IsChildOf_Internal(UAnimNode* Node)
{
	if( this == Node )
	{
		return TRUE;
	}

	INT const NumParentNodes = ParentNodes.Num();
	for(INT i=0; i<NumParentNodes; i++)
	{
		UAnimNodeBlendBase* ParentNode = ParentNodes(i);

		// Can be connected to multiple parent nodes, so make sure we touch them only once.
		if( ParentNode->SearchTag != UAnimNode::CurrentSearchTag )
		{
			ParentNode->SearchTag = UAnimNode::CurrentSearchTag;
			if( ParentNodes(i)->IsChildOf_Internal(Node) )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/** Don't load editor only anim nodes */
UBOOL UAnimNode::NeedsLoadForClient() const
{
	if( bEditorOnly )
	{
		return 0;
	}
	else
	{
		return Super::NeedsLoadForClient();
	}
}

/** Don't load editor only anim nodes */
UBOOL UAnimNode::NeedsLoadForServer() const
{
	if( bEditorOnly )
	{
		return 0;
	}
	else
	{
		return Super::NeedsLoadForServer();
	}
}

void UAnimNode::PlayAnim(UBOOL bLoop, FLOAT Rate, FLOAT StartTime)
{}

void UAnimNode::StopAnim()
{}

void UAnimNode::ReplayAnim()
{}

void UAnimNode::OnPaste()
{
	NodeTotalWeight = 0.f;

	// FIXME
	//PostAnimNodeInstance();

	Super::OnPaste();
}

void UAnimNode::ResetAnimNodeToSource(UAnimNode* SourceNode)
{
	bRelevant = bJustBecameRelevant = bDisableCaching = bCachedHasRootMotion = FALSE;
	NodeTickTag = NodeInitTag = NodeCachedAtomsTag = SearchTag = NodeEndEventTick = 0;

	CachedNumDesiredBones = 0;
	CachedBoneAtoms.Empty();
	CachedCurveKeys.Empty();
}

///////////////////////////////////////
///////// UAnimNodeBlendBase //////////
///////////////////////////////////////

void UAnimNode::CallDeferredInitAnim()
{
	// If node needs to be initialized, call DeferredInitAnim()
	if( NodeInitTag != SkelComponent->InitTag )
	{
		NodeInitTag = SkelComponent->InitTag;
		DeferredInitAnim();
	}

	// Update search tag, so node is touched only once.
	SearchTag = UAnimNode::CurrentSearchTag;
}

void UAnimNodeBlendBase::CallDeferredInitAnim()
{
	// If node needs to be initialized, call DeferredInitAnim()
	if( NodeInitTag != SkelComponent->InitTag )
	{
		NodeInitTag = SkelComponent->InitTag;
		DeferredInitAnim();
	}

	// Update search tag, so node is touched only once.
	SearchTag = UAnimNode::CurrentSearchTag;

	// Call on active children
	INT const ChildCount = Children.Num();
	for(INT i=0; i<ChildCount; i++)
	{
		// We have to look at weight, and not bRelevant flag, as Weight could have been changed by SetActiveChild() or similar function
		// After the node has been ticked.
		if( Children(i).Weight > ZERO_ANIMWEIGHT_THRESH )
		{
			UAnimNode* ChildNode = Children(i).Anim;
			// Make sure we touch each node only once.
			if( ChildNode && ChildNode->SearchTag != UAnimNode::CurrentSearchTag )
			{
				ChildNode->CallDeferredInitAnim();
			}
		}
	}
}


void UAnimNode::BuildParentNodesArray()
{
	// Touch each node only once
	SearchTag = UAnimNode::CurrentSearchTag;
	ParentNodes.Empty();
}

/** Traverse the Tree and build ParentNodesArray */
void UAnimNodeBlendBase::BuildParentNodesArray()
{
	// Touch each node only once
	SearchTag = UAnimNode::CurrentSearchTag;
	ParentNodes.Empty();

	// Then iterate over calling BuildParentNodesArray on its children
	INT const ChildCount = Children.Num();
	for(INT i=0; i<ChildCount; i++)
	{
		UAnimNode* ChildNode = Children(i).Anim;
		if( ChildNode )
		{
			// Traverse Tree through child
			// Do check here, to avoid calling virtual function.
			if( ChildNode->SearchTag != UAnimNode::CurrentSearchTag )
			{
				ChildNode->BuildParentNodesArray();
			}

			// Add ourselves to its parent nodes list.
			ChildNode->ParentNodes.AddUniqueItem(this);
		}
	}
}

#if PERF_ENABLE_INITANIM_STATS
static FName NAME_BuildTickArray = FName(TEXT("InitAnimTree_BuildTickArray"));
#endif

/** Used for building array of AnimNodes in 'tick' order - that is, all parents of a node are added to array before it. */
void UAnimNodeBlendBase::BuildTickArray(TArray<UAnimNode*>& OutTickArray)
{
	INITANIM_CUSTOM(NAME_BuildTickArray)	

	// Then iterate over calling BuildTickArray on any children who have had all parents updated.
	for(INT i=0; i<Children.Num(); i++)
	{
		UAnimNode* ChildNode = Children(i).Anim;
		if( ChildNode && ChildNode->NodeTickTag != SkelComponent->TickTag )
		{
			// Set SkelComponent Pointer - required by WereAllParentsTicked()
			ChildNode->SkelComponent = SkelComponent;

			if( ChildNode->WereAllParentsTicked() )
			{
				// Add to array
				ChildNode->TickArrayIndex = OutTickArray.AddItem(ChildNode);
				// Use tick tag to indicate it was added
				ChildNode->NodeTickTag = SkelComponent->TickTag;
				
				{
					EXCLUDE_PARENT_TIME
					// Call to add its children.
					ChildNode->BuildTickArray(OutTickArray);
				}
			}
		}
	}
}

/**
 * Set final Children weight, to be used during the GetBoneAtoms phase for blending.
 * @param DeltaSeconds Size of timestep we are advancing animation tree by. In seconds.
 */
void UAnimNodeBlendBase::TickAnim(FLOAT DeltaSeconds)
{
	INT const ChildNum = Children.Num();
	for(INT ChildIndex=0; ChildIndex<ChildNum && ChildIndex < 8; ChildIndex++)
	{
		CONSOLE_PREFETCH(Children(ChildIndex).Anim);
	}
	for(INT ChildIndex=0; ChildIndex<ChildNum; ChildIndex++)
	{
		// update children weight
		UpdateChildWeight(ChildIndex);
	}
}

/** 
 * Update Child Weight : Make sure childIndex isn't OOB
 */
void UAnimNodeBlendBase::UpdateChildWeight(INT ChildIndex)
{
	// Iterate over each child, updating this nodes contribution to its weight.
	// Only if node is relevant, otherwise it's always going to be zero.
	check (Children.IsValidIndex(ChildIndex));

	UAnimNode* ChildNode = Children(ChildIndex).Anim;
	if( ChildNode )
	{
		// Calculate the 'global weight' of the connection to this child.
		FLOAT& ChildNodeWeight = SkelComponent->AnimTickWeightsArray( ChildNode->TickArrayIndex );
		ChildNodeWeight = ::Min(ChildNodeWeight + NodeTotalWeight * Children(ChildIndex).Weight, 1.f);
	}
}

/**
 * Find all AnimNodes including and below this one in the tree. Results are arranged so that parents are before children in the array.
 * For a blend node, will recursively call GetNodes on all child nodes.
 * 
 * @param Nodes Output array of AnimNode pointers.
 */
void UAnimNodeBlendBase::GetNodesInternal(TArray<UAnimNode*>& Nodes)
{
	// make sure we're only called from inside GetNodes() or another GetNodesInternal()
	checkSlow(UAnimNode::bNodeSearching);

	if (SearchTag != UAnimNode::CurrentSearchTag)
	{
		SearchTag = UAnimNode::CurrentSearchTag;
		Nodes.AddItem(this);
		for (INT i = 0; i < Children.Num(); i++)
		{
			if (Children(i).Anim != NULL)
			{
				Children(i).Anim->GetNodesInternal(Nodes);
			}
		}
	}
}

// Use AnimBlendTypes (see Enum definition in AnimNodeBlendBase.uc)
#define USEANIMBLENDTYPES	1

#if USEANIMBLENDTYPES	
	#define GET_CHILD_BLENDWEIGHT(i)	Children(i).BlendWeight	
#else	// Revert to linear blends (old way).
	#define GET_CHILD_BLENDWEIGHT(i)	Children(i).Weight
#endif

#if 0 && !CONSOLE
	#define DEBUGBLENDTYPEWEIGHTS(x)	{ ##x }
#else
	#define DEBUGBLENDTYPEWEIGHTS(x)
#endif

FORCEINLINE FLOAT UAnimNodeBlendBase::GetBlendWeight(FLOAT ChildWeight)
{
#if USEANIMBLENDTYPES
	return AlphaToBlendType(ChildWeight, BlendType);
#else
	return ChildWeight;
#endif
}

FORCEINLINE void UAnimNodeBlendBase::SetBlendTypeWeights()
{
#if USEANIMBLENDTYPES
	DEBUGBLENDTYPEWEIGHTS( debugf(TEXT("SetBlendTypeWeights %s, %d"), *NodeName.ToString(), BlendType); )
	FLOAT AccumulatedWeight = 0.f;
	for(INT i=0; i<Children.Num(); i++)
	{
		Children(i).BlendWeight = 0.f;

		// Don't consider additive nodes as their weight doesn't sum up to 1.f
		if( !Children(i).bIsAdditive && Children(i).Weight > ZERO_ANIMWEIGHT_THRESH )
		{
			Children(i).BlendWeight = GetBlendWeight(Children(i).Weight);
			check(Children(i).BlendWeight >= 0 && Children(i).BlendWeight <= 1.f)
			DEBUGBLENDTYPEWEIGHTS( debugf(TEXT(" %d, Weight: %f BlendWeight: %f"), i, Children(i).Weight, Children(i).BlendWeight); );
			AccumulatedWeight += Children(i).BlendWeight;
		}
	}

	DEBUGBLENDTYPEWEIGHTS( debugf(TEXT(" AccumulatedWeight: %f"), AccumulatedWeight); )

	// See if we need to renormalize weights.
	if( BlendType != ABT_Linear && AccumulatedWeight > ZERO_ANIMWEIGHT_THRESH && Abs(AccumulatedWeight - 1.f) > ZERO_ANIMWEIGHT_THRESH )
	{
		const FLOAT Normalizer = 1.f / AccumulatedWeight;
		DEBUGBLENDTYPEWEIGHTS( debugf(TEXT(" Normalizer: %f"), Normalizer); )
		for(INT i=0; i<Children.Num(); i++)
		{
			if( !Children(i).bIsAdditive && GET_CHILD_BLENDWEIGHT(i) > ZERO_ANIMWEIGHT_THRESH )
			{
				Children(i).BlendWeight *= Normalizer;
			}
		}
	}
#endif
}

#if 0
static void CheckForLongBones(TArray<FBoneAtom>& Atoms, USkeletalMeshComponent* SkelComp, UAnimNode* Node)
{
	check(Atoms.Num() == SkelComp->SkeletalMesh->RefSkeleton.Num());
	for(INT i=1; i<SkelComp->SkeletalMesh->RefSkeleton.Num(); i++)
	{
		if(	appStristr(*SkelComp->SkeletalMesh->RefSkeleton(i).Name.ToString(), TEXT("b_MF_IK_")) ||
			appStristr(*SkelComp->SkeletalMesh->RefSkeleton(i).Name.ToString(), TEXT("b_MF_Weapon_")) ||
			appStristr(*SkelComp->SkeletalMesh->RefSkeleton(i).Name.ToString(), TEXT("b_AR_Receiver_Spin")) )
		{
			continue;
		}

		FLOAT AtomLength = Atoms(i).GetTranslation().Size();
		FLOAT RefLength = SkelComp->SkeletalMesh->RefSkeleton(i).BonePos.Position.Size();

		if(AtomLength > RefLength * 1.5f)
		{
			//debugf(TEXT("ATOM TOO LONG! %f %s %s"), AtomLength, *SkelComp->SkeletalMesh->RefSkeleton(i).Name.ToString(), *Node->GetPathName());
		}
	}
}
#endif

/**
 * Blends together the Children AnimNodes of this blend based on the Weight in each element of the Children array.
 * Instead of using SLERPs, the blend is done by taking a weighted sum of each atom, and renormalising the quaternion part at the end.
 * This allows n-way blends, and makes the code much faster, though the angular velocity will not be constant across the blend.
 *
 * @param	Atoms			Output array of relative bone transforms.
 * @param	DesiredBones	Indices of bones that we want to return. Note that bones not in this array will not be modified, so are not safe to access! 
 *							This array must be in strictly increasing order.
 */
void UAnimNodeBlendBase::GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	START_GETBONEATOM_TIMER

	// See if results are cached.
	if( GetCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num()) )
	{
		return;
	}

	// Handle case of a blend with no children.
	if (Children.Num())
	{
	}
	else
	{
		RootMotionDelta.SetIdentity();
		bHasRootMotion	= 0;
		FillWithRefPose(Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
		return;
	}

#ifdef _DEBUG
	// Check all children weights sum to 1.0
	if( Abs(GetChildWeightTotal() - 1.f) > ZERO_ANIMWEIGHT_THRESH )
	{
		AActor* Owner = SkelComponent->GetOwner();
		warnf(TEXT("WARNING: AnimNodeBlendBase has Children weights which do not sum to 1 - Node: %s, NodeName: %s, Owner: %s"), 
			*GetFName().ToString(), *NodeName.ToString(), Owner ? *Owner->GetFName().ToString() : TEXT("NULL"));	

		FLOAT TotalWeight = 0.f;
		for(INT i=0; i<Children.Num(); i++)
		{
			UAnimNode* ANode = Children(i).Anim;
			warnf(TEXT("[%i] %s, Weight: %f, Children: %s (%s), bIsAdditive: %d"), 
				i, 
				*Children(i).Name.ToString(), 
				Children(i).Weight, 
				(ANode ? *ANode->GetFName().ToString() : TEXT("NULL")), 
				(ANode ? *ANode->NodeName.ToString() : TEXT("None")), 
				Children(i).bIsAdditive );	

			if( !Children(i).bIsAdditive )
			{
				TotalWeight += Children(i).Weight;
			}
		}

		debugf( TEXT("Total Weight: %f"), TotalWeight );
		check( Abs(GetChildWeightTotal() - 1.f) <= ZERO_ANIMWEIGHT_THRESH );
	}
#endif

	const INT NumAtoms = SkelComponent->SkeletalMesh->RefSkeleton.Num();
	check( NumAtoms == Atoms.Num() );

	// Get information on our relevant children.
	INT		LastChildAddIndex		= INDEX_NONE;
	INT		LastChildNonAddIndex	= INDEX_NONE;
	INT		NumRelevantAddAnims		= 0;
	INT		NumRelevantNonAddAnims	= 0;
	UBOOL	bHasAdditiveAnimations	= FALSE;

	// Children Curve Keys for blending later
	const INT PresizingChildCount = 6;

	TArray< FCurveKeyArray, TInlineAllocator<PresizingChildCount, TMemStackAllocator<GMainThreadMemStack> > > ChildrenCurveKeys;
	ChildrenCurveKeys.Empty(Children.Num());
	ChildrenCurveKeys.AddZeroed(Children.Num());

	TArray<INT, TInlineAllocator<PresizingChildCount, TMemStackAllocator<GMainThreadMemStack> > > ChildrenHasRootMotion;
	ChildrenHasRootMotion.Empty(Children.Num());
	ChildrenHasRootMotion.AddZeroed(Children.Num());

	TArray<FBoneAtom, TInlineAllocator<PresizingChildCount, TMemStackAllocator<GMainThreadMemStack> > > ChildrenRootMotion;
	ChildrenRootMotion.Empty(Children.Num());
	ChildrenRootMotion.Add(Children.Num());

	for(INT i=0; i<Children.Num(); i++)
	{
		if( Children(i).Weight > ZERO_ANIMWEIGHT_THRESH )
		{
			if( !Children(i).bIsAdditive )
			{
				LastChildNonAddIndex = i;
				NumRelevantNonAddAnims++;
			}
			else
			{
				bHasAdditiveAnimations = TRUE;
				LastChildAddIndex = i;
				NumRelevantAddAnims++;
			}
		}
	}

	if( LastChildNonAddIndex == INDEX_NONE )
	{
		AActor* Owner = SkelComponent->GetOwner();
		warnf(TEXT("LastChildNonAddIndex != INDEX_NONE - Node: %s, NodeName: %s, Owner: %s"), *GetFName().ToString(), *NodeName.ToString(), Owner ? *Owner->GetFName().ToString() : TEXT("NULL"));	
		for(INT i=0; i<Children.Num(); i++)
		{
			UAnimNode* ANode = Children(i).Anim;
			warnf(TEXT("[%i] %s, Weight: %f, Children: %s (%s), bIsAdditive: %d"), 
				i, 
				*Children(i).Name.ToString(), 
				Children(i).Weight, 
				(ANode ? *ANode->GetFName().ToString() : TEXT("NULL")), 
				(ANode ? *ANode->NodeName.ToString() : TEXT("None")), 
				Children(i).bIsAdditive );	
		}
	}
 	check(LastChildNonAddIndex != INDEX_NONE);

	// If only one child is relevant, just pass through this one.
	if( NumRelevantNonAddAnims == 1 )
	{
		if( Children(LastChildNonAddIndex).Anim )
		{
			EXCLUDE_CHILD_TIME
			if( !Children(LastChildNonAddIndex).bMirrorSkeleton )
			{
				Children(LastChildNonAddIndex).Anim->GetBoneAtoms(Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, ChildrenCurveKeys(LastChildNonAddIndex));
			}
			else
			{
				GetMirroredBoneAtoms(Atoms, LastChildNonAddIndex, DesiredBones, RootMotionDelta, bHasRootMotion, ChildrenCurveKeys(LastChildNonAddIndex));
			}
#ifdef _DEBUG
			// Check that all bone atoms coming from animation are normalized
			for( INT ChckBoneIdx=0; ChckBoneIdx<DesiredBones.Num(); ChckBoneIdx++ )
			{
				const INT	BoneIndex = DesiredBones(ChckBoneIdx);
				check( Atoms(BoneIndex).IsRotationNormalized() );
			}

			if( bHasRootMotion )
			{
				check( RootMotionDelta.IsRotationNormalized() );
			}
#endif
		}
		else
		{
			RootMotionDelta.SetIdentity();
			bHasRootMotion	= 0;
			FillWithRefPose(Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
		}
	}
	else
	{
#if USEANIMBLENDTYPES
		// Translate weights into blend weights.
		SetBlendTypeWeights();
#endif
		FBoneAtomArray ChildAtoms;
		ChildAtoms.Add(NumAtoms);
		UBOOL bNoChildrenYet = TRUE;

		bHasRootMotion						= 0;
		INT		LastRootMotionChildIndex	= INDEX_NONE;
		ScalarRegister AccumulatedRootMotionWeight(ScalarZero);
		
		FBoneAtom* AtomsData = Atoms.GetTypedData();
		FBoneAtom* ChildAtomsData = ChildAtoms.GetTypedData();
		const unsigned char* DesiredBonesData = DesiredBones.GetTypedData();
		INT NumDesiredBones = DesiredBones.Num();
		check(NumDesiredBones > 0);

		// Iterate over each child getting its atoms, scaling them and adding them to output (Atoms array)
		// First pass non additive animations.
		for(INT i=0; i<=LastChildNonAddIndex; i++)
		{
			ScalarRegister VChildBlendWeight(GET_CHILD_BLENDWEIGHT(i));

			CONSOLE_PREFETCH(AtomsData + DesiredBonesData[0]);

			// If this child has non-zero weight, blend it into accumulator.
			if( !Children(i).bIsAdditive && GET_CHILD_BLENDWEIGHT(i) > ZERO_ANIMWEIGHT_THRESH )
			{
				// Get bone atoms from child node (if no child - use ref pose).
				if( Children(i).Anim )
				{
					EXCLUDE_CHILD_TIME
					if( !Children(i).bMirrorSkeleton )
					{
						GetChildBoneAtoms( i, ChildAtoms, DesiredBones, ChildrenRootMotion(i), ChildrenHasRootMotion(i), ChildrenCurveKeys(i));
					}
					else
					{
						GetMirroredBoneAtoms(ChildAtoms, i, DesiredBones, ChildrenRootMotion(i), ChildrenHasRootMotion(i), ChildrenCurveKeys(i));
					}

#ifdef _DEBUG
					// Check that all bone atoms coming from animation are normalized
					for( INT ChckBoneIdx=0; ChckBoneIdx<DesiredBones.Num(); ChckBoneIdx++ )
					{
						const INT BoneIndex = DesiredBones(ChckBoneIdx);
						check( ChildAtoms(BoneIndex).IsRotationNormalized() );
					}
#endif
					// If this children received root motion information, accumulate its weight
					if( ChildrenHasRootMotion(i) )
					{
						bHasRootMotion				= 1;
						LastRootMotionChildIndex	= i;
						AccumulatedRootMotionWeight += ScalarRegister(GET_CHILD_BLENDWEIGHT(i));
					}
				}
				else
				{	
					ChildrenRootMotion(i).SetIdentity();
					ChildrenHasRootMotion(i)	= 0;
					FillWithRefPose(ChildAtoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
				}

				if( bNoChildrenYet )
				{
					// We just write the first childrens atoms into the output array. Avoids zero-ing it out.
					for(INT j=0; j<DesiredBones.Num(); j++)
					{
						const INT BoneIndex = DesiredBones(j);
						Atoms(BoneIndex) = ChildAtoms(BoneIndex) * VChildBlendWeight;
					}

					bNoChildrenYet = FALSE;
				}
				else
				{
					for(INT j=0; j<DesiredBones.Num(); j++)
					{
						const INT BoneIndex = DesiredBones(j);
						Atoms(BoneIndex).AccumulateWithShortestRotation(ChildAtoms(BoneIndex), VChildBlendWeight);
					}
				}

				bNoChildrenYet = FALSE;
			}
		}

		// Normalize rotations
		for(INT j=0; j<DesiredBones.Num(); j++)
		{
			const INT BoneIndex = DesiredBones(j);
			Atoms(BoneIndex).NormalizeRotation();
		}

		//@TODO: Seems dead to me, since we normalize above...
#ifdef _DEBUG
		// Check that all bone atoms coming from animation are normalized
		for( INT ChckBoneIdx=0; ChckBoneIdx<DesiredBones.Num(); ChckBoneIdx++ )
		{
			const INT BoneIndex = DesiredBones(ChckBoneIdx);
			check( Atoms(BoneIndex).IsRotationNormalized() );
		}
#endif
		// 2nd pass, iterate over all children who received root motion
		// And blend root motion only between these children.
		if( bHasRootMotion )
		{
			bNoChildrenYet = TRUE;
			ScalarRegister InvAccumulatedRootMotionWeight = ScalarReciprocal(AccumulatedRootMotionWeight);
			for(INT i=0; i<=LastRootMotionChildIndex; i++)
			{
				if( !Children(i).bIsAdditive && GET_CHILD_BLENDWEIGHT(i) > ZERO_ANIMWEIGHT_THRESH && ChildrenHasRootMotion(i) )
				{
					const ScalarRegister Weight = ScalarRegister(GET_CHILD_BLENDWEIGHT(i)) * InvAccumulatedRootMotionWeight;

					// Accumulate Root Motion
					if( bNoChildrenYet )
					{
						RootMotionDelta = ChildrenRootMotion(i) * Weight;
						bNoChildrenYet	= FALSE;
					}
					else
					{
						RootMotionDelta.AccumulateWithShortestRotation(ChildrenRootMotion(i), Weight);
					}
				}
			}

			// Normalize rotation quaternion at the end.
			RootMotionDelta.NormalizeRotation();
		}
	}

	// If we have additive animations, do another pass for those
	if( bHasAdditiveAnimations )
	{
		// We don't allocate this array until we need it.
		FBoneAtomArray ChildAtoms;
		ChildAtoms.Add(NumAtoms);

		// Iterate over each child getting its atoms, scaling them and adding them to output (Atoms array)
		// First pass non additive animations.
		for(INT i=0; i<=LastChildAddIndex; i++)
		{
			if( Children(i).bIsAdditive )
			{
#if USEANIMBLENDTYPES
				// Compute blend weight for additive animations.
				// This is done separately as additive animations don't need to sum up to 1.
				Children(i).BlendWeight = 0.f;
				if( Children(i).Weight > ZERO_ANIMWEIGHT_THRESH )
				{
					Children(i).BlendWeight = GetBlendWeight(Children(i).Weight);
					check(Children(i).BlendWeight >= 0 && Children(i).BlendWeight <= 1.f)
					DEBUGBLENDTYPEWEIGHTS( debugf(TEXT(" ADDITIVE PASS %d, Weight: %f BlendWeight: %f"), i, Children(i).Weight, Children(i).BlendWeight); );
				}
#endif
				// Read the blend weight (note: do not merge these two reads)
				const FLOAT BlendWeight = GET_CHILD_BLENDWEIGHT(i);
				const ScalarRegister VBlendWeight(GET_CHILD_BLENDWEIGHT(i));

				// If this child has non-zero weight, blend it into accumulator.
				if( BlendWeight > ZERO_ANIMWEIGHT_THRESH )
				{
					// Get bone atoms from child node (if no child - skip).
					if( Children(i).Anim )
					{
						EXCLUDE_CHILD_TIME
						if( !Children(i).bMirrorSkeleton )
						{
							GetChildBoneAtoms( i, ChildAtoms, DesiredBones, ChildrenRootMotion(i), ChildrenHasRootMotion(i), ChildrenCurveKeys(i));
						}
						else
						{
							GetMirroredBoneAtoms(ChildAtoms, i, DesiredBones, ChildrenRootMotion(i), ChildrenHasRootMotion(i), ChildrenCurveKeys(i));
						}
#ifdef _DEBUG
						// Check that all bone atoms coming from animation are normalized
						for( INT ChckBoneIdx=0; ChckBoneIdx<DesiredBones.Num(); ChckBoneIdx++ )
						{
							const INT BoneIndex = DesiredBones(ChckBoneIdx);
							check( ChildAtoms(BoneIndex).IsRotationNormalized() );
						}
#endif
					}
					else
					{	
						continue;
					}

					for(INT j=0; j<DesiredBones.Num(); j++)
					{
						const INT BoneIndex = DesiredBones(j);
						FBoneAtom::BlendFromIdentityAndAccumulate(Atoms(BoneIndex), ChildAtoms(BoneIndex), VBlendWeight);
					}

					// If additive animation has root motion, then add it!
					if( ChildrenHasRootMotion(i) )
					{
						// If we didn't have non additive root motion, then it's all coming from additive.
						// So we need to set that up
						if( !bHasRootMotion )
						{
							bHasRootMotion = 1;
							RootMotionDelta.SetIdentity();
						}

						// Normalize rotation quaternion at the end.
						RootMotionDelta.Accumulate(ChildrenRootMotion(i), VBlendWeight);
						RootMotionDelta.NormalizeRotation();
					}
				}
			}
		}
	}

#ifdef _DEBUG
	// Check that all bone atoms coming from animation are normalized
	for( INT ChckBoneIdx=0; ChckBoneIdx<DesiredBones.Num(); ChckBoneIdx++ )
	{
		const INT BoneIndex = DesiredBones(ChckBoneIdx);
		check( Atoms(BoneIndex).IsRotationNormalized() );
	}

	if( bHasRootMotion )
	{
		check( RootMotionDelta.IsRotationNormalized() );
	}
#endif

	// returns result of blending
	if (GIsEditor || SkelComponent->bRecentlyRendered)
	{
		FCurveKeyArray NewCurveKeys;
		if ( ChildrenCurveKeys.Num() > 1 && BlendCurveWeights(ChildrenCurveKeys, NewCurveKeys)  > 0 )
		{
			CurveKeys += NewCurveKeys;
		}
		else if (ChildrenCurveKeys.Num() == 1)
		{
			CurveKeys += ChildrenCurveKeys(0);
		}

#if WITH_EDITORONLY_DATA
#if !FINAL_RELEASE
		if ( GIsEditor )
		{
			if (ChildrenCurveKeys.Num() == 1)
			{
				LastUpdatedAnimMorphKeys = ChildrenCurveKeys(0);
			}
			else
			{
				// editor only variable for morph debugging
				LastUpdatedAnimMorphKeys = NewCurveKeys;
			}
		}
#endif
#endif // WITH_EDITORONLY_DATA
	}

	SaveCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num());
}

void UAnimNodeBlendBase::GetChildBoneAtoms( INT ChildIdx, FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys )
{
	Children(ChildIdx).Anim->GetBoneAtoms( Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys );
}

// Struct to hold Curve Key Weight and Child Blend Weight
typedef struct 
{
	FLOAT CurveKeyWeight;
	FLOAT ChildWeight;
} ChildCurveKeyWeight;

/** 
 * Resolve conflicts for blend curve weights if same morph target exists 
 *
 * @param	InChildrenCurveKeys	Array of curve keys for children. The index should match up with Children.
 * @param	OutCurveKeys		Result output after blending is resolved
 * 
 * @return	Number of new addition to OutCurveKeys
 */
INT UAnimNodeBlendBase::BlendCurveWeights(const FArrayCurveKeyArray& InChildrenCurveKeys, FCurveKeyArray& OutCurveKeys)
{
	check(InChildrenCurveKeys.Num() == Children.Num());

	// Make local version so that I can edit
	FArrayCurveKeyArray ChildrenCurveKeys = InChildrenCurveKeys;
	extern const FLOAT MinMorphBlendWeight;

	TMap<FName, TArray<ChildCurveKeyWeight> > CurveKeyMap;

	// if same target found, accumulate 
	// technically you can have one TArray for holding all childrens' key, but 
	// just in case in future we need to know which children has which information
	for (INT I=0; I<ChildrenCurveKeys.Num(); ++I)
	{
		const FCurveKeyArray& LocalCurveKeyArray = ChildrenCurveKeys(I);

		if ( Children(I).Weight > MinMorphBlendWeight )
		{
			const FLOAT ChildBlendWeight = Children(I).Weight; 

			for (INT J=0; J<LocalCurveKeyArray.Num(); ++J)
			{
				if ( LocalCurveKeyArray(J).Weight > MinMorphBlendWeight )
				{
					ChildCurveKeyWeight NewChildCurve;
					NewChildCurve.ChildWeight = ChildBlendWeight;
					NewChildCurve.CurveKeyWeight = LocalCurveKeyArray(J).Weight;

					// collect child weight and curve weight separate for all curves 
					// need to normalize later using the information
					TArray<ChildCurveKeyWeight> * ChildCurves = CurveKeyMap.Find(LocalCurveKeyArray(J).CurveName);

					// if the weight is found, add
					if ( ChildCurves ) 
					{
						ChildCurves->AddItem(NewChildCurve);
					}
					else // otherwise, add to the map
					{
						TArray<ChildCurveKeyWeight> NewChildCurves;
						NewChildCurves.AddItem(NewChildCurve);
						CurveKeyMap.Set(LocalCurveKeyArray(J).CurveName, NewChildCurves);
					}
				}
			}
		}
	}

	// save sum
	ChildCurveKeyWeight SumOfChildWeights;
	// now iterate through and add to the array after normalizing to sumf of child weight
	for (TMap<FName, TArray<ChildCurveKeyWeight> >::TConstIterator Iter(CurveKeyMap); Iter; ++Iter)
	{
		TArray<ChildCurveKeyWeight> ChildCurves = Iter.Value();
		SumOfChildWeights.ChildWeight = 0.f;
		SumOfChildWeights.CurveKeyWeight = 0.f;
		for ( INT I=0; I<ChildCurves.Num(); ++I )
		{
			SumOfChildWeights.ChildWeight +=ChildCurves(I).ChildWeight;
			// please note, now curvekeyweight is saving with child weight multiplied - 
			// now curvekeyweight is normalized to each child
			SumOfChildWeights.CurveKeyWeight +=ChildCurves(I).CurveKeyWeight*ChildCurves(I).ChildWeight;
		}

		if ( SumOfChildWeights.CurveKeyWeight > MinMorphBlendWeight && SumOfChildWeights.ChildWeight > ZERO_ANIMWEIGHT_THRESH )
		{
			// now normalize with all children that affecting it
			OutCurveKeys.AddItem(FCurveKey(Iter.Key(), SumOfChildWeights.CurveKeyWeight/SumOfChildWeights.ChildWeight));
		//	debugf(TEXT("NodeName:%s, CurveName:%s, Value:%0.2f"), *NodeName.GetNameString(), *Iter.Key().GetNameString(), SumOfChildWeights.CurveKeyWeight/SumOfChildWeights.ChildWeight);
		}
	}

	return OutCurveKeys.Num();
}

/** 
* Get mirrored bone atoms from desired child index. 
 * Bones are mirrored using the SkelMirrorTable.
 */
void UAnimNodeBlendBase::GetMirroredBoneAtoms(FBoneAtomArray& Atoms, INT ChildIndex, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	USkeletalMesh* SkelMesh = SkelComponent->SkeletalMesh;
	check(SkelMesh);

	// If mirroring is enabled, and the mirror info array is initialized correctly.
	if( SkelMesh->SkelMirrorTable.Num() == Atoms.Num() )
	{
		if( Children(ChildIndex).Anim )
		{
			GetChildBoneAtoms(ChildIndex, Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
		}
		else
		{
			RootMotionDelta	= FBoneAtom::Identity;
			bHasRootMotion	= 0;
			FillWithRefPose(Atoms, DesiredBones, SkelMesh->RefSkeleton);
		}

#ifdef _DEBUG
		// Check that all bone atoms coming from animation are normalized
		for( INT ChckBoneIdx=0; ChckBoneIdx<DesiredBones.Num(); ChckBoneIdx++ )
		{
			const INT	BoneIndex = DesiredBones(ChckBoneIdx);
			check( Atoms(BoneIndex).IsRotationNormalized() );
		}
#endif
		{
			SCOPE_CYCLE_COUNTER(STAT_MirrorBoneAtoms);


			// We build the mesh-space matrices of the source bones.
			FBoneAtomArray BoneTM;
			BoneTM.Add(SkelMesh->RefSkeleton.Num());

			FBoneAtom* AtomsData = Atoms.GetTypedData();
			FBoneAtom* BoneTMData = BoneTM.GetTypedData();

			for(INT i=0; i<DesiredBones.Num(); i++)
			{	
				INT const BoneIndex = DesiredBones(i);

				if( BoneIndex != 0 )
				{
					const INT ParentIndex = SkelMesh->RefSkeleton(BoneIndex).ParentIndex;
					FBoneAtom::Multiply(BoneTMData + BoneIndex, AtomsData + BoneIndex, BoneTMData + ParentIndex);
				}
				else
				{
					BoneTM(0) =  Atoms(0);
				}
			}

			// Then we do the mirroring.

			// Make array of flags to track which bones have already been mirrored.
			TArray<UBOOL> BoneMirrored;
			BoneMirrored.InsertZeroed(0, Atoms.Num());

			for(INT i=0; i<DesiredBones.Num(); i++)
			{
				INT const BoneIndex = DesiredBones(i);
				if( BoneMirrored(BoneIndex) )
				{
					continue;
				}

				BYTE const BoneFlipAxis = SkelMesh->SkelMirrorTable(BoneIndex).BoneFlipAxis;

				// In Editor AXIS_BLANK can be selected.
				// This will result in no mirroring done for the selected bone.
				if( BoneFlipAxis != 3 )
				{
					// Get 'flip axis' from SkeletalMesh, unless we have specified an override for that bone.
					BYTE const FlipAxis = (BoneFlipAxis != AXIS_None) ? BoneFlipAxis : SkelMesh->SkelMirrorFlipAxis;

					// Mirror the root motion delta
					// @fixme laurent -- add support for root rotation and mirroring
					if( BoneIndex == 0 && bHasRootMotion )
					{
						FQuatRotationTranslationMatrix RootTM(RootMotionDelta.GetRotation(), RootMotionDelta.GetTranslation());
						RootTM.Mirror(SkelMesh->SkelMirrorAxis, FlipAxis);
						RootMotionDelta.SetTranslation(RootTM.GetOrigin());
						RootMotionDelta.SetRotation(RootTM.Rotator().Quaternion());
					}

					INT const SourceIndex = SkelMesh->SkelMirrorTable(BoneIndex).SourceIndex;

					if( BoneIndex == SourceIndex )
					{
						BoneTM(BoneIndex).Mirror(SkelMesh->SkelMirrorAxis, FlipAxis);
						BoneMirrored(BoneIndex) = TRUE;
					}
					else
					{
						// get source flip axis
						BYTE const SourceBoneFlipAxis = SkelMesh->SkelMirrorTable(SourceIndex).BoneFlipAxis;
						BYTE const SourceFlipAxis = (SourceBoneFlipAxis != AXIS_None) ? SourceBoneFlipAxis : SkelMesh->SkelMirrorFlipAxis;

						FBoneAtom BoneTransform0 = BoneTM(BoneIndex);
						FBoneAtom BoneTransform1 = BoneTM(SourceIndex);
						BoneTransform0.Mirror(SkelMesh->SkelMirrorAxis, SourceFlipAxis);
						BoneTransform1.Mirror(SkelMesh->SkelMirrorAxis, FlipAxis);
						BoneTM(BoneIndex)			= BoneTransform1;
						BoneTM(SourceIndex)			= BoneTransform0;
						BoneMirrored(BoneIndex)		= TRUE;
						BoneMirrored(SourceIndex)	= TRUE;
					}
				}
			}

			// Now we need to convert this back into local space transforms.
			for(INT i=0; i<DesiredBones.Num(); i++)
			{
				const INT BoneIndex = DesiredBones(i);

				BYTE const BoneFlipAxis = SkelMesh->SkelMirrorTable(BoneIndex).BoneFlipAxis;
				// In Editor AXIS_BLANK can be selected.
				// This will result in no mirroring done for the selected bone.
				// In that case no need to transform to local space transform, since that hasn't changed.
				if( BoneFlipAxis != 3 )
				{
					if( BoneIndex == 0 )
					{
						Atoms(BoneIndex) = BoneTM(BoneIndex);
					}
					else
					{
						const FBoneAtom ParentTM = BoneTM(SkelMesh->RefSkeleton(BoneIndex).ParentIndex);
						Atoms(BoneIndex) = BoneTM(BoneIndex) * ParentTM.InverseSafe();
					}
					// Normalize rotation quaternion after the mirroring has been done.
					Atoms(BoneIndex).NormalizeRotation();
				}
				// However update world transform, in case a child needs to be updated from world to local.
				else
				{
					if( BoneIndex != 0 )
					{
						INT const ParentIndex = SkelMesh->RefSkeleton(BoneIndex).ParentIndex;

						FBoneAtom::Multiply(BoneTMData + BoneIndex, AtomsData + BoneIndex, BoneTMData + ParentIndex);
					}
					else
					{
						BoneTM(0) = Atoms(0);
					}
				}

#ifdef _DEBUG
				check( Atoms(BoneIndex).IsRotationNormalized() );
#endif
			}
		}
	}
	// Otherwise, just pass right through.
	else
	{
		if( Children(ChildIndex).Anim )
		{
			GetChildBoneAtoms(ChildIndex, Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
		}
		else
		{
			RootMotionDelta	= FBoneAtom::Identity;
			bHasRootMotion	= 0;
			FillWithRefPose(Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
		}
	}
}

/**
 * For debugging.
 * 
 * @return Sum weight of all children weights. Should always be 1.0
 */
FLOAT UAnimNodeBlendBase::GetChildWeightTotal()
{
	FLOAT TotalWeight = 0.f;

	for(INT i=0; i<Children.Num(); i++)
	{
		// Skip additive animation children, their weight doesn't need to sum up to 1.f
		if( !Children(i).bIsAdditive )
		{
			TotalWeight += Children(i).Weight;
		}
	}

	return TotalWeight;
}

/**
 * Make sure to relay OnChildAnimEnd to Parent AnimNodeBlendBase as long as it exists 
 */ 
void UAnimNodeBlendBase::OnChildAnimEnd(UAnimNodeSequence* Child, FLOAT PlayedTime, FLOAT ExcessTime) 
{ 
	for(INT i=0; i<ParentNodes.Num(); i++)
	{
		if (ParentNodes(i)->NodeEndEventTick!=SkelComponent->TickTag)
		{
			ParentNodes(i)->OnChildAnimEnd(Child, PlayedTime, ExcessTime); 
			ParentNodes(i)->NodeEndEventTick = SkelComponent->TickTag;
		}
	}
} 

void UAnimNodeBlendBase::PlayAnim(UBOOL bLoop, FLOAT Rate, FLOAT StartTime)
{
	// pass on the call to our children
	for (INT i = 0; i < Children.Num(); i++)
	{
		if (Children(i).Anim != NULL)
		{
			Children(i).Anim->PlayAnim(bLoop, Rate, StartTime);
		}
	}
}

void UAnimNodeBlendBase::ReplayAnim()	
{
	// pass on the call to our children
	for (INT i = 0; i < Children.Num(); i++)
	{
		if (Children(i).Anim != NULL)
		{
			Children(i).Anim->ReplayAnim();
		}
	}
}

void UAnimNodeBlendBase::StopAnim()
{
	// pass on the call to our children
	for (INT i = 0; i < Children.Num(); i++)
	{
		if (Children(i).Anim != NULL)
		{
			Children(i).Anim->StopAnim();
		}
	}
}

/** Rename all child nodes upon Add/Remove, so they match their position in the array. */
void UAnimNodeBlendBase::RenameChildConnectors()
{
	for(INT i=0; i<Children.Num(); i++)
	{
		FName OldFName = Children(i).Name;
		FString OldStringName = Children(i).Name.ToString();
		//if it contains "child" as the first string, it more than likely isn't custom named.
		if ((OldStringName.InStr("Child")==0) || (OldFName == NAME_None))
		{
			FString NewChildName = FString::Printf( TEXT("Child%d"), i + 1 );
			Children(i).Name = FName( *NewChildName );
		}
	}
}

/** A child connector has been added */
void UAnimNodeBlendBase::OnAddChild(INT ChildNum)
{
	// Make sure name matches position in array.
	RenameChildConnectors();
}

/** A child connector has been removed */
void UAnimNodeBlendBase::OnRemoveChild(INT ChildNum)
{
	// Make sure name matches position in array.
	RenameChildConnectors();
}

void UAnimNodeBlendBase::OnPaste()
{
	for (INT j=0; j<Children.Num(); ++j)
	{
		if (Children(j).Anim)
		{
			Children(j).Anim = FindObject<UAnimNode>(GetOuter(), *Children(j).Anim->GetName());
		}
	}

	Super::OnPaste();
}

///////////////////////////////////////
//////////// UAnimNodeBlend ///////////
///////////////////////////////////////

/** @see UAnimNode::TickAnim */
void UAnimNodeBlend::TickAnim(FLOAT DeltaSeconds)
{
	if( BlendTimeToGo > 0.f )
	{
		if( BlendTimeToGo > DeltaSeconds )
		{
			// Amount we want to change Child2Weight by.
			FLOAT const BlendDelta = Child2WeightTarget - Child2Weight; 

			Child2Weight	+= (BlendDelta / BlendTimeToGo) * DeltaSeconds;
			BlendTimeToGo	-= DeltaSeconds;
		}
		else
		{
			Child2Weight	= Child2WeightTarget;
			BlendTimeToGo	= 0.f;
		}

		// debugf(TEXT("Blender: %s (%x) Child2Weight: %f BlendTimeToGo: %f"), *GetPathName(), (INT)this, Child2BoneWeights.ChannelWeight, BlendTimeToGo);
	}

	Children(0).Weight = 1.f - Child2Weight;
	Children(1).Weight = Child2Weight;

	// Call Super::TickAnim last, to set proper weights on children.
	Super::TickAnim(DeltaSeconds);
}


/**
 * Set desired balance of this blend.
 * 
 * @param BlendTarget Target amount of weight to put on Children(1) (second child). Between 0.0 and 1.0. 1.0 means take all animation from second child.
 * @param BlendTime How long to take to get to BlendTarget.
 */
void UAnimNodeBlend::SetBlendTarget(FLOAT BlendTarget, FLOAT BlendTime)
{
	Child2WeightTarget = Clamp<FLOAT>(BlendTarget, 0.f, 1.f);
	
	if( bSkipBlendWhenNotRendered && !SkelComponent->bRecentlyRendered && !GIsEditor )
	{
		BlendTime = 0.f;
	}

	// If we want this weight NOW - update weights straight away (don't wait for TickAnim).
	if( BlendTime <= 0.0f )
	{
		Child2Weight		= Child2WeightTarget;
		Children(0).Weight	= 1.f - Child2Weight;
		Children(1).Weight	= Child2Weight;
	}

	BlendTimeToGo = BlendTime;
}


///////////////////////////////////////
///////////// AnimNodeCrossfader //////
///////////////////////////////////////

/** @see UAnimNode::InitAnim */
void UAnimNodeCrossfader::InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent )
{	
	START_INITANIM_TIMER
	{
		EXCLUDE_PARENT_TIME
		Super::InitAnim( meshComp, Parent );
	}

	UAnimNodeSequence	*ActiveChild = GetActiveChild();
	if( ActiveChild && ActiveChild->AnimSeqName == NAME_None )
	{
		SetAnim( DefaultAnimSeqName );
	}
}

/** @see UAnimNode::TickAnim */
void UAnimNodeCrossfader::TickAnim(FLOAT DeltaSeconds)
{
	if( !bDontBlendOutOneShot && PendingBlendOutTimeOneShot > 0.f )
	{
		UAnimNodeSequence	*ActiveChild = GetActiveChild();

		if( ActiveChild && ActiveChild->AnimSeq )
		{
			FLOAT	fCountDown = ActiveChild->AnimSeq->SequenceLength - ActiveChild->CurrentTime;
			
			// if playing a one shot anim, and it's time to blend back to previous animation, do so.
			if( fCountDown <= PendingBlendOutTimeOneShot )
			{
				SetBlendTarget( 1.f - Child2WeightTarget, PendingBlendOutTimeOneShot );
				PendingBlendOutTimeOneShot = 0.f;
			}
		}
	}

	// Call Super::TickAnim last to set proper children weights.
	Super::TickAnim(DeltaSeconds);
}

/** @see AnimCrossfader::GetAnimName */
FName UAnimNodeCrossfader::GetAnimName()
{
	UAnimNodeSequence	*ActiveChild = GetActiveChild();
	if( ActiveChild )
	{
		return ActiveChild->AnimSeqName;
	}
	else
	{
		return NAME_None;
	}
}

/**
 * Get active AnimNodeSequence child. To access animation properties and control functions.
 *
 * @return	AnimNodeSequence currently playing.
 */
UAnimNodeSequence *UAnimNodeCrossfader::GetActiveChild()
{
	// requirements for the crossfader. Just exit if not met, do not crash.
	if( Children.Num() != 2 ||	// needs 2 childs
		!Children(0).Anim ||	// properly connected
		!Children(1).Anim )
	{
		return NULL;
	}

	return Cast<UAnimNodeSequence>(Child2WeightTarget < 0.5f ? Children(0).Anim : Children(1).Anim);
}

/** @see AnimNodeCrossFader::PlayOneShotAnim */
void UAnimNodeCrossfader::execPlayOneShotAnim( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(AnimSeqName);
	P_GET_FLOAT_OPTX(BlendInTime,0.f); 
	P_GET_FLOAT_OPTX(BlendOutTime,0.f);
	P_GET_UBOOL_OPTX(bDontBlendOut,false);
	P_GET_FLOAT_OPTX(Rate,1.f);
	P_FINISH;

	// requirements for the crossfader. Just exit if not met, do not crash.
	if( Children.Num() != 2 ||	// needs 2 childs
		!Children(0).Anim ||	// properly connected
		!Children(1).Anim ||
		SkelComponent == NULL )
	{
		return;
	}

	// Make sure AnimSeqName exists
	if( SkelComponent->FindAnimSequence( AnimSeqName ) == NULL )
	{
		debugf( NAME_Warning,TEXT("%s - Failed to find animsequence '%s' on SkeletalMeshComponent: '%s' whose owner is: '%s' and is of type %s" ),
			*GetName(),
			*AnimSeqName.ToString(),
			*SkelComponent->GetName(), 
			*SkelComponent->GetOwner()->GetName(),
			*SkelComponent->TemplateName.ToString()
			);
		return;
	}

	UAnimNodeSequence*	Child = Cast<UAnimNodeSequence>(Child2WeightTarget < 0.5f ? Children(1).Anim : Children(0).Anim);

	if( Child )
	{
		FLOAT	BlendTarget			= Child2WeightTarget < 0.5f ? 1.f : 0.f;

		bDontBlendOutOneShot		= bDontBlendOut;
		PendingBlendOutTimeOneShot	= BlendOutTime;

		Child->SetAnim(AnimSeqName);
		Child->PlayAnim(false, Rate);
		SetBlendTarget( BlendTarget, BlendInTime );
	}
}

/** @see AnimNodeCrossFader::BlendToLoopingAnim */
void UAnimNodeCrossfader::execBlendToLoopingAnim( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(AnimSeqName);
	P_GET_FLOAT_OPTX(BlendInTime,0.f);
	P_GET_FLOAT_OPTX(Rate,1.f);
	P_FINISH;

	// requirements for the crossfader. Just exit if not met, do not crash.
	if( Children.Num() != 2 ||	// needs 2 childs
		!Children(0).Anim ||	// properly connected
		!Children(1).Anim ||
		SkelComponent == NULL )
	{
		return;
	}

	// Make sure AnimSeqName exists
	if( SkelComponent->FindAnimSequence( AnimSeqName ) == NULL )
	{
		debugf( NAME_Warning,TEXT("%s - Failed to find animsequence '%s' on SkeletalMeshComponent: '%s' whose owner is: '%s' and is of type %s" ),
			*GetName(),
			*AnimSeqName.ToString(),
			*SkelComponent->GetName(), 
			*SkelComponent->GetOwner()->GetName(),
			*SkelComponent->TemplateName.ToString()
			);
		return;
	}

	UAnimNodeSequence*	Child = Cast<UAnimNodeSequence>(Child2WeightTarget < 0.5f ? Children(1).Anim : Children(0).Anim);

	if( Child )
	{
		FLOAT	BlendTarget			= Child2WeightTarget < 0.5f ? 1.f : 0.f;

		// One shot variables..
		bDontBlendOutOneShot		= true;
		PendingBlendOutTimeOneShot	= 0.f;

		Child->SetAnim(AnimSeqName);
		Child->PlayAnim(true, Rate);
		SetBlendTarget( BlendTarget, BlendInTime );
	}
}


/************************************************************************************
 * AnimNodeBlendPerBone
 ***********************************************************************************/

void UAnimNodeBlendPerBone::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged && 
		(PropertyThatChanged->GetFName() == FName(TEXT("BranchStartBoneName"))) )
	{
		BuildWeightList();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UAnimNodeBlendPerBone::BuildWeightList()
{
	if( !SkelComponent || !SkelComponent->SkeletalMesh )
	{
		return;
	}

	TArray<FMeshBone>& RefSkel	= SkelComponent->SkeletalMesh->RefSkeleton;
	const INT NumAtoms			= RefSkel.Num();

	Child2PerBoneWeight.Reset();
	Child2PerBoneWeight.AddZeroed(NumAtoms);

	TArray<INT> BranchStartBoneIndex;
	BranchStartBoneIndex.Add( BranchStartBoneName.Num() );

	for(INT NameIndex=0; NameIndex<BranchStartBoneName.Num(); NameIndex++)
	{
		BranchStartBoneIndex(NameIndex) = SkelComponent->MatchRefBone( BranchStartBoneName(NameIndex) );
	}

	for(INT i=0; i<NumAtoms; i++)
	{
		const INT BoneIndex	= i;
		UBOOL bBranchBone = BranchStartBoneIndex.ContainsItem(BoneIndex);

		if( bBranchBone )
		{
			Child2PerBoneWeight(BoneIndex) = 1.f;
		}
		else
		{
			if( BoneIndex > 0 )
			{
				Child2PerBoneWeight(BoneIndex) = Child2PerBoneWeight(RefSkel(BoneIndex).ParentIndex);
			}
		}
	}

	// build list of required bones
	LocalToCompReqBones.Empty();

	for(INT i=0; i<NumAtoms; i++)
	{
		// if weight different than previous one, then this bone needs to be blended in component space
		if( Child2PerBoneWeight(i) != Child2PerBoneWeight(RefSkel(i).ParentIndex) )
		{
			LocalToCompReqBones.AddItem(i);
		}
	}
	EnsureParentsPresent(LocalToCompReqBones, SkelComponent->SkeletalMesh);
}


/** 
 *	Utility for taking two arrays of bytes, which must be strictly increasing, and finding the intersection between them.
 *	That is - any item in the output should be present in both A and B. Output is strictly increasing as well.
 */
void IntersectByteArrays(TArray<BYTE>& Output, const TArray<BYTE>& A, const TArray<BYTE>& B)
{
	INT APos = 0;
	INT BPos = 0;
	while(	APos < A.Num() && BPos < B.Num() )
	{
		// If value at APos is lower, increment APos.
		if( A(APos) < B(BPos) )
		{
			APos++;
		}
		// If value at BPos is lower, increment APos.
		else if( B(BPos) < A(APos) )
		{
			BPos++;
		}
		// If they are the same, put value into output, and increment both.
		else
		{
			Output.AddItem( A(APos) );
			APos++;
			BPos++;
		}
	}
}



void UAnimNodeBlendPerBone::InitAnim(USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent)
{
	START_INITANIM_TIMER
	{
		EXCLUDE_PARENT_TIME
		Super::InitAnim(meshComp, Parent);
	}

	BuildWeightList();

	// Force child 0 to weight 1.
	Children(0).Weight = 1.f;
}

// AnimNode interface
void UAnimNodeBlendPerBone::TickAnim(FLOAT DeltaSeconds)
{
	if( BlendTimeToGo > 0.f )
	{
		if( BlendTimeToGo > DeltaSeconds )
		{
			// Amount we want to change Child2Weight by.
			FLOAT const BlendDelta = Child2WeightTarget - Child2Weight; 

			Child2Weight	+= (BlendDelta / BlendTimeToGo) * DeltaSeconds;
			BlendTimeToGo	-= DeltaSeconds;
		}
		else
		{
			Child2Weight	= Child2WeightTarget;
			BlendTimeToGo	= 0.f;
		}

		// debugf(TEXT("Blender: %s (%x) Child2Weight: %f BlendTimeToGo: %f"), *GetPathName(), (INT)this, Child2BoneWeights.ChannelWeight, BlendTimeToGo);
	}

	Children(0).Weight = 1.f;
	Children(1).Weight = Child2Weight;

	// Skip super call, as we want to force child 1 weight to 1.f
	UAnimNodeBlendBase::TickAnim(DeltaSeconds);
}

/**
 * Set desired balance of this blend.
 * 
 * @param BlendTarget Target amount of weight to put on Children(1) (second child). Between 0.0 and 1.0. 1.0 means take all animation from second child.
 * @param BlendTime How long to take to get to BlendTarget.
 */
void UAnimNodeBlendPerBone::SetBlendTarget(FLOAT BlendTarget, FLOAT BlendTime)
{
	Super::SetBlendTarget(BlendTarget, BlendTime);

	// Make sure that no matter what, child zero weight is 1.
	Children(0).Weight = 1.f;
}

/** @see UAnimNode::GetBoneAtoms. */
void UAnimNodeBlendPerBone::GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	START_GETBONEATOM_TIMER

	// See if results are cached.
	if( GetCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num()) )
	{
		return;
	}

	// If drawing all from Children(0), no need to look at Children(1). Pass Atoms array directly to Children(0).
	if( Children(1).Weight <= ZERO_ANIMWEIGHT_THRESH )
	{
		if( Children(0).Anim )
		{
			EXCLUDE_CHILD_TIME
			Children(0).Anim->GetBoneAtoms(Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
		}
		else
		{
			RootMotionDelta.SetIdentity();
			bHasRootMotion	= 0;
			FillWithRefPose(Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
		}

		// Pass-through, no caching needed.
		return;
	}

	TArray<FMeshBone>& RefSkel = SkelComponent->SkeletalMesh->RefSkeleton;
	const INT NumAtoms = RefSkel.Num();

	FBoneAtomArray Child1Atoms, Child2Atoms;

	// Get bone atoms from each child (if no child - use ref pose).
	Child1Atoms.Add(NumAtoms);
	FBoneAtom	Child1RMD				= FBoneAtom::Identity;
	INT  		bChild1HasRootMotion	= FALSE;
	if( Children(0).Anim )
	{
		EXCLUDE_CHILD_TIME
		Children(0).Anim->GetBoneAtoms(Child1Atoms, DesiredBones, Child1RMD, bChild1HasRootMotion, CurveKeys);
	}
	else
	{
		FillWithRefPose(Child1Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
	}

	// Get only the necessary bones from child2. The ones that have a Child2PerBoneWeight(BoneIndex) > 0
	Child2Atoms.Add(NumAtoms);
	FBoneAtom Child2RMD = FBoneAtom::Identity;
	INT bChild2HasRootMotion = FALSE;

	//debugf(TEXT("child2 went from %d bones to %d bones."), DesiredBones.Num(), Child2DesiredBones.Num() );
	if( Children(1).Anim )
	{
		EXCLUDE_CHILD_TIME
		Children(1).Anim->GetBoneAtoms(Child2Atoms, DesiredBones, Child2RMD, bChild2HasRootMotion, CurveKeys);
	}
	else
	{
		FillWithRefPose(Child2Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
	}

	FBoneAtomArray GChild1CompSpace, GChild2CompSpace, GResultCompSpace;
	// If we are doing component-space blend, ensure working buffers are big enough
	if( !bForceLocalSpaceBlend )
	{
		GChild1CompSpace.Add(NumAtoms);
		GChild2CompSpace.Add(NumAtoms);
		GResultCompSpace.Add(NumAtoms);
	}

	INT LocalToCompReqIndex = 0;

	// do blend
	for(INT i=0; i<DesiredBones.Num(); i++)
	{
		const INT	BoneIndex			= DesiredBones(i);
		const FLOAT Child2BoneWeight	= Children(1).Weight * Child2PerBoneWeight(BoneIndex);

		//debugf(TEXT("  (%2d) %1.1f %s"), BoneIndex, Child2PerBoneWeight(BoneIndex), *RefSkel(BoneIndex).Name);
		const UBOOL bDoComponentSpaceBlend =	LocalToCompReqIndex < LocalToCompReqBones.Num() && 
												BoneIndex == LocalToCompReqBones(LocalToCompReqIndex);

		if( !bForceLocalSpaceBlend && bDoComponentSpaceBlend )
		{
			//debugf(TEXT("  (%2d) %1.1f %s"), BoneIndex, Child2PerBoneWeight(BoneIndex), *RefSkel(BoneIndex).Name);
			LocalToCompReqIndex++;

			const INT ParentIndex	= RefSkel(BoneIndex).ParentIndex;

			// turn bone atoms to matrices
			GChild1CompSpace(BoneIndex) = Child1Atoms(BoneIndex);
			GChild2CompSpace(BoneIndex) = Child2Atoms(BoneIndex);

			// transform to component space
			if( BoneIndex > 0 )
			{
				GChild1CompSpace(BoneIndex) *= GChild1CompSpace(ParentIndex);
				GChild2CompSpace(BoneIndex) *= GChild2CompSpace(ParentIndex);
			}

			ScalarRegister VChild2BoneWeight(Child2BoneWeight);

			// everything comes from child1 copy directly
			if( Child2BoneWeight <= ZERO_ANIMWEIGHT_THRESH )
			{
				GResultCompSpace(BoneIndex) = GChild1CompSpace(BoneIndex);
			}
			// everything comes from child2, copy directly
			else if( Child2BoneWeight >= (1.f - ZERO_ANIMWEIGHT_THRESH) )
			{
				GResultCompSpace(BoneIndex) = GChild2CompSpace(BoneIndex);
			}
			// blend rotation in component space of both childs
			else
			{
				// convert matrices to FBoneAtoms
				FBoneAtom Child1CompSpaceAtom(GChild1CompSpace(BoneIndex));
				FBoneAtom Child2CompSpaceAtom(GChild2CompSpace(BoneIndex));

				// blend FBoneAtom rotation. We store everything in Child1

				// We use a linear interpolation and a re-normalize for the rotation.
				// Treating Rotation as an accumulator, initialize to a scaled version of Atom1.
				Child1CompSpaceAtom.SetRotation(Child1CompSpaceAtom.GetRotation() * (1.0f - Child2BoneWeight));

				// Then add on the second rotation..
				Child1CompSpaceAtom.AccumulateWithShortestRotation(Child2CompSpaceAtom, VChild2BoneWeight);

				// ..and renormalize
				Child1CompSpaceAtom.NormalizeRotation();

				GResultCompSpace(BoneIndex)= Child1CompSpaceAtom;
			}

			// Blend Translation and Scale in local space
			Atoms(BoneIndex).LerpTranslationScale(Child1Atoms(BoneIndex), Child2Atoms(BoneIndex), VChild2BoneWeight);

			// and rotation was blended in component space...
			// convert bone atom back to local space
			FBoneAtom ParentTM = FBoneAtom::Identity;
			if( BoneIndex > 0 )
			{
				ParentTM = GResultCompSpace(ParentIndex);
			}

			// Then work out relative transform, and convert to FBoneAtom.
			const FBoneAtom RelTM = GResultCompSpace(BoneIndex) * ParentTM.Inverse();
			Atoms(BoneIndex).SetRotation( FBoneAtom(RelTM).GetRotation() );
		}	
		else
		{
			// otherwise do faster local space blending.
			Atoms(BoneIndex).Blend(Child1Atoms(BoneIndex), Child2Atoms(BoneIndex), Child2BoneWeight);
		}

		// Blend root motion
		if( BoneIndex == 0 )
		{
			bHasRootMotion = bChild1HasRootMotion || bChild2HasRootMotion;

			if( bChild1HasRootMotion && bChild2HasRootMotion )
			{
				RootMotionDelta.Blend(Child1RMD, Child2RMD, Child2BoneWeight);
			}
			else if( bChild1HasRootMotion )
			{
				RootMotionDelta = Child1RMD;
			}
			else if( bChild2HasRootMotion )
			{
				RootMotionDelta = Child2RMD;
			}
		}
	}
	
	SaveCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num());
}


///////////////////////////////////////
/////// AnimNodeBlendDirectional //////
///////////////////////////////////////


/**
 * Updates weight of the 4 directional animation children by looking at the current velocity and heading of actor.
 * 
 * @see UAnimNode::TickAnim
 */
void UAnimNodeBlendDirectional::TickAnim(FLOAT DeltaSeconds)
{
	check(Children.Num() == 4);

	// Calculate DirAngle based on player velocity.
	AActor* actor = SkelComponent->GetOwner(); // Get the actor to use for acceleration/look direction etc.
	if( actor )
	{
		FLOAT TargetDirAngle = 0.f;
		FVector	VelDir = bUseAcceleration ? actor->Acceleration : actor->Velocity;
		VelDir.Z = 0.0f;

		if( VelDir.IsNearlyZero() )
		{
			TargetDirAngle = 0.f;
		}
		else
		{
			VelDir = VelDir.SafeNormal();

			FVector LookDir = (actor->Rotation+RotationOffset).Vector();
			LookDir.Z = 0.f;
			LookDir = LookDir.SafeNormal();

			FVector LeftDir = LookDir ^ FVector(0.f,0.f,1.f);
			LeftDir = LeftDir.SafeNormal();

			FLOAT ForwardPct = (LookDir | VelDir);
			FLOAT LeftPct = (LeftDir | VelDir);

			TargetDirAngle = appAcos(ForwardPct);
			if( LeftPct > 0.f )
			{
				TargetDirAngle *= -1.f;
			}
		}
		// Move DirAngle towards TargetDirAngle as fast as DirRadsPerSecond allows
		FLOAT DeltaDir = FindDeltaAngle(DirAngle, TargetDirAngle);
		if( DeltaDir != 0.f )
		{
			FLOAT MaxDelta = DeltaSeconds * DirDegreesPerSecond * (PI/180.f);
			DeltaDir = Clamp<FLOAT>(DeltaDir, -MaxDelta, MaxDelta);
			DirAngle = UnwindHeading( DirAngle + DeltaDir );
		}
	}

	// Option to only choose one animation
	if(SkelComponent->PredictedLODLevel < SingleAnimAtOrAboveLOD)
	{
		// Work out child weights based on DirAngle.
		if(DirAngle < -0.5f*PI) // Back and left
		{
			Children(2).Weight = (DirAngle/(0.5f*PI)) + 2.f;
			Children(3).Weight = 0.f;

			Children(0).Weight = 0.f;
			Children(1).Weight = 1.f - Children(2).Weight;
		}
		else if(DirAngle < 0.f) // Forward and left
		{
			Children(2).Weight = -DirAngle/(0.5f*PI);
			Children(3).Weight = 0.f;

			Children(0).Weight = 1.f - Children(2).Weight;
			Children(1).Weight = 0.f;
		}
		else if(DirAngle < 0.5f*PI) // Forward and right
		{
			Children(2).Weight = 0.f;
			Children(3).Weight = DirAngle/(0.5f*PI);

			Children(0).Weight = 1.f - Children(3).Weight;
			Children(1).Weight = 0.f;
		}
		else // Back and right
		{
			Children(2).Weight = 0.f;
			Children(3).Weight = (-DirAngle/(0.5f*PI)) + 2.f;

			Children(0).Weight = 0.f;
			Children(1).Weight = 1.f - Children(3).Weight;
		}
	}
	else
	{
		Children(0).Weight = 0.f;
		Children(1).Weight = 0.f;
		Children(2).Weight = 0.f;
		Children(3).Weight = 0.f;

		if(DirAngle < -0.75f*PI) // Back
		{
			Children(1).Weight = 1.f;
		}
		else if(DirAngle < -0.25f*PI) // Left
		{
			Children(2).Weight = 1.f;
		}
		else if(DirAngle < 0.25f*PI) // Forward
		{
			Children(0).Weight = 1.f;
		}
		else if(DirAngle < 0.75f*PI) // Right
		{
			Children(3).Weight = 1.f;
		}
		else // Back
		{
			Children(1).Weight = 1.f;
		}
	}

	// Call Super::TickAnim last to set proper children weights.
	Super::TickAnim(DeltaSeconds);
}


///////////////////////////////////////
/////////// AnimNodeBlendList /////////
///////////////////////////////////////

/**
 * Will ensure TargetWeight array is right size. If it has to resize it, will set Chidlren(0) to have a target of 1.0.
 * Also, if all Children weights are zero, will set Children(0) as the active child.
 * 
 * @see UAnimNode::InitAnim
 */
void UAnimNodeBlendList::InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent )
{
	START_INITANIM_TIMER
	{
		EXCLUDE_PARENT_TIME
		Super::InitAnim(meshComp, Parent);
	}

	if( TargetWeight.Num() != Children.Num() )
	{
		TargetWeight.Empty( Children.Num() );
		TargetWeight.AddZeroed( Children.Num() );

		if( TargetWeight.Num() > 0 )
		{
			TargetWeight(0) = 1.f;
		}
	}

	// If all child weights are zero - set the first one to the active child.
	const FLOAT ChildWeightSum = GetChildWeightTotal();
	if( ChildWeightSum <= ZERO_ANIMWEIGHT_THRESH )
	{
		SetActiveChild(ActiveChildIndex, 0.f);
	}
}

// UObject interface
void UAnimNodeBlendList::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

#if WITH_EDITORONLY_DATA
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("EditorActiveChildIndex")) )
	{
		// Update slider instead of directly setting the child index, as some nodes use that for editor previewing
		// and triggering transitions in TickAnim().
		FLOAT const NewSliderPosition = Clamp<FLOAT>(FLOAT(EditorActiveChildIndex) / FLOAT(Children.Num() - 1), 0, 1.f);
		HandleSliderMove(0, 0, NewSliderPosition);
	}
#endif // WITH_EDITORONLY_DATA
}

/** @see UAnimNode::TickAnim */
void UAnimNodeBlendList::TickAnim(FLOAT DeltaSeconds)
{
	check(Children.Num() == TargetWeight.Num());

	// When this node becomes relevant to the tree and the bForceChildFullWeightWhenBecomingRelevant
	// flag is set, then make sure the node is not blending between multiple children.
	// Start full weight on our active child to minimize max number of blends and animation decompression.
	// Note, it is done here instead of OnBecomeRelevant(), so the node gets a chance to update its state first.
	// And it's done here instead of SetActiveChild(), so we can catch nodes that are already being blended.
	if( bJustBecameRelevant && bForceChildFullWeightWhenBecomingRelevant &&
		ActiveChildIndex >= 0 && ActiveChildIndex < Children.Num() && 
		Children(ActiveChildIndex).Weight != 1.f )
	{
		SetActiveChild(ActiveChildIndex, 0.f);
	}

	// Do nothing if BlendTimeToGo is zero.
	if( BlendTimeToGo > 0.f )
	{
		// So we don't overshoot.
		if( BlendTimeToGo <= DeltaSeconds )
		{
			BlendTimeToGo = 0.f; 

			const INT ChildNum = Children.Num();
			for(INT i=0; i<ChildNum; i++)
			{
				Children(i).Weight = TargetWeight(i);
			}
		}
		else
		{
			const INT ChildNum = Children.Num();
			for(INT i=0; i<ChildNum; i++)
			{
				// Amount left to blend.
				const FLOAT BlendDelta = TargetWeight(i) - Children(i).Weight;
				Children(i).Weight += (BlendDelta / BlendTimeToGo) * DeltaSeconds;
			}

			BlendTimeToGo -= DeltaSeconds;
		}
	}

	// Call Super::TickAnim last to set proper children weights.
	Super::TickAnim(DeltaSeconds);
}


/**
 * The the child to blend up to full Weight (1.0)
 * 
 * @param ChildIndex Index of child node to ramp in. If outside range of children, will set child 0 to active.
 * @param BlendTime How long for child to take to reach weight of 1.0.
 */
void UAnimNodeBlendList::SetActiveChild( INT ChildIndex, FLOAT BlendTime )
{
	check(Children.Num() == TargetWeight.Num());
	
	if( ChildIndex < 0 || ChildIndex >= Children.Num() )
	{
		debugf( TEXT("UAnimNodeBlendList::SetActiveChild : %s ChildIndex (%d) outside number of Children (%d)."), *GetName(), ChildIndex, Children.Num() );
		ChildIndex = 0;
	}

	if( BlendTime > 0.f )
	{
		// Clamp ActiveChildIndex to make sure it is valid.
		ActiveChildIndex = Clamp<INT>(ActiveChildIndex, 0, Children.Num() - 1);

		// If going full weight when node becomes relevant, then set instant blend
		if( (bForceChildFullWeightWhenBecomingRelevant && bJustBecameRelevant) || (bSkipBlendWhenNotRendered && SkelComponent && !SkelComponent->bRecentlyRendered && !GIsEditor) )
		{
			BlendTime = 0.f;
		}
		// If switching to same node, then just transfer current weight. Do a simple switch instead of blending.
		else if( Children(ActiveChildIndex).Anim == Children(ChildIndex).Anim )
		{
			BlendTime *= (1.f - Children(ActiveChildIndex).Weight);
		}
		// Otherwise just scale by destination weight.
		else
		{
			BlendTime *= (1.f - Children(ChildIndex).Weight);
		}
	}

	for(INT i=0; i<Children.Num(); i++)
	{
		if(i == ChildIndex)
		{
			TargetWeight(i) = 1.0f;

			// If we basically want this straight away - dont wait until a tick to update weights.
			if(BlendTime == 0.0f)
			{
				Children(i).Weight = 1.0f;
			}
		}
		else
		{
			TargetWeight(i) = 0.0f;

			if(BlendTime == 0.0f)
			{
				Children(i).Weight = 0.0f;
			}
		}
	}

	BlendTimeToGo = BlendTime;
	ActiveChildIndex = ChildIndex;

	if( bPlayActiveChild )
	{
		// Play the animation if this child is a sequence
		UAnimNodeSequence* AnimSeq = Cast<UAnimNodeSequence>(Children(ActiveChildIndex).Anim);
		if( AnimSeq )
		{
			AnimSeq->ReplayAnim();
		}
	}
}

FLOAT UAnimNodeBlendList::GetSliderPosition(INT SliderIndex, INT ValueIndex)
{
	check(0 == SliderIndex && 0 == ValueIndex);
	return SliderPosition;
}

void UAnimNodeBlendList::HandleSliderMove(INT SliderIndex, INT ValueIndex, FLOAT NewSliderValue)
{
	check(0 == SliderIndex && 0 == ValueIndex);
	SliderPosition = NewSliderValue;

	if( Children.Num() > 0 )
	{
		const INT TargetChannel = appRound(SliderPosition*(Children.Num() - 1));
		if( ActiveChildIndex != TargetChannel )
		{
			SetActiveChild(TargetChannel, 0.1f);
		}
	}
}

FString UAnimNodeBlendList::GetSliderDrawValue(INT SliderIndex)
{
	check(0 == SliderIndex);
	INT TargetChild = appRound( SliderPosition*(Children.Num() - 1) );
	if( Children.Num() > 0 && TargetChild < Children.Num() )
	{
		return FString::Printf( TEXT("%3.2f %s"), SliderPosition, *Children(TargetChild).Name.ToString() );
	}

	return FString::Printf( TEXT("%3.2f"), SliderPosition );
}

/**
 * Reset the specified TargetWeight array to the given number of children.
 */
static void ResetTargetWeightArray(TArrayNoInit<FLOAT>& TargetWeight, INT ChildNum)
{
	TargetWeight.Empty();
	if( ChildNum > 0 )
	{
		TargetWeight.AddZeroed( ChildNum );
		TargetWeight(0) = 1.f;
	}
}

/** Called when we add a child to this node. We reset the TargetWeight array when this happens. */
void UAnimNodeBlendList::OnAddChild(INT ChildNum)
{
	Super::OnAddChild(ChildNum);

	ResetTargetWeightArray( TargetWeight, Children.Num() );
}

/** Called when we remove a child to this node. We reset the TargetWeight array when this happens. */
void UAnimNodeBlendList::OnRemoveChild(INT ChildNum)
{
	Super::OnRemoveChild(ChildNum);

	ResetTargetWeightArray( TargetWeight, Children.Num() );
}

void UAnimNodeBlendList::OnPaste()
{
	Super::OnPaste();

	ResetTargetWeightArray( TargetWeight, Children.Num() );
}

void UAnimNodeBlendList::ResetAnimNodeToSource(UAnimNode *SourceNode)
{
	Super::ResetAnimNodeToSource(SourceNode);

	ResetTargetWeightArray( TargetWeight, Children.Num() );

	UAnimNodeBlendList* SourceSequence = Cast<UAnimNodeBlendList>(SourceNode);
	if (SourceSequence)
	{
		ActiveChildIndex = SourceSequence->ActiveChildIndex;
	}

}
/**
 * Blend animations based on an Owner's posture.
 *
 * @param DeltaSeconds	Time since last tick in seconds.
 */
void UAnimNodeBlendByPosture::TickAnim(FLOAT DeltaSeconds)
{
	APawn* Owner = SkelComponent ? Cast<APawn>(SkelComponent->GetOwner()) : NULL;

	if ( Owner )
	{
		if ( Owner->bIsCrouched )
		{
			if ( ActiveChildIndex != 1 )
			{
				SetActiveChild( 1, 0.1f );
			}
		}
		else if ( ActiveChildIndex != 0 )
		{
			SetActiveChild( 0 , 0.1f );
		}
	}

	// Call Super::TickAnim last to set proper children weights.
	Super::TickAnim(DeltaSeconds);
}



/************************************************************************************
 * UAnimNodeBlendBySpeed
 ***********************************************************************************/

/**
 * Resets the last channel on becoming active.	
 */
void UAnimNodeBlendBySpeed::OnBecomeRelevant()
{
	Super::OnBecomeRelevant();
	LastChannel = -1;
}

/**
 * Blend animations based on an Owner's velocity.
 *
 * @param DeltaSeconds	Time since last tick in seconds.
 */
void UAnimNodeBlendBySpeed::TickAnim(FLOAT DeltaSeconds)
{
	INT		NumChannels				= Children.Num();
	UBOOL	SufficientChannels		= NumChannels >= 2,
			SufficientConstraints	= Constraints.Num() >= NumChannels;

	if( SufficientChannels && SufficientConstraints )
	{
		INT		TargetChannel	= 0;

		// Get the speed we should use for the blend.
		Speed = CalcSpeed();

		// Find appropriate channel for current speed with "Constraints" containing an upper speed bound.
		while( (TargetChannel < NumChannels-1) && (Speed > Constraints(TargetChannel)) )
		{
			TargetChannel++;
		}

		// See if we need to blend down.
		if( TargetChannel > 0 )
		{
			FLOAT SpeedRatio = (Speed - Constraints(TargetChannel-1)) / (Constraints(TargetChannel) - Constraints(TargetChannel-1));
			if( SpeedRatio <= BlendDownPerc )
			{
				TargetChannel--;
			}
		}

		if( TargetChannel != LastChannel )
		{
			UBOOL bChangeChannel = TRUE;
			if (BlendUpDelay > 0.f || BlendDownDelay > 0.f)
			{
				if (BlendDelayRemaining == 0.f)
				{
					BlendDelayRemaining = TargetChannel > LastChannel ? BlendUpDelay : BlendDownDelay;
				}
				if (BlendDelayRemaining > 0.f)
				{
					BlendDelayRemaining -= DeltaSeconds;
					if(BlendDelayRemaining <= 0.f)
					{
						BlendDelayRemaining = 0.f;
						bChangeChannel = TRUE;
					}
					else
					{
						bChangeChannel = FALSE;
					}
				}
			}
			if (bChangeChannel)
			{
				if( TargetChannel < LastChannel )
				{
					SetActiveChild( TargetChannel, BlendDownTime );
				}
				else
				{
					SetActiveChild( TargetChannel, BlendUpTime );
				}
				LastChannel = TargetChannel;
			}
		}
		else
		{
			BlendDelayRemaining = 0.f;
		}
	}
	else if( !SufficientChannels )
	{
		debugf(TEXT("UAnimNodeBlendBySpeed::TickAnim - Need at least two children"));
	}
	else if( !SufficientConstraints )
	{
		debugf(TEXT("UAnimNodeBlendBySpeed::TickAnim - Number of constraints (%i) is lower than number of children! (%i)"), Constraints.Num(), NumChannels);
	}
	
	// Call Super::TickAnim last to set proper children weights.
	Super::TickAnim(DeltaSeconds);				
}

/** 
 *	Function called to calculate the speed that should be used for this node. 
 *	Allows subclasses to easily modify the speed used.
 */
FLOAT UAnimNodeBlendBySpeed::CalcSpeed()
{
	AActor* Owner = SkelComponent ? SkelComponent->GetOwner() : NULL;
	if(Owner)
	{
		if( bUseAcceleration )
		{
			return Owner->Acceleration.Size();
		}
		else
		{
			return Owner->Velocity.Size();
		}
	}
	else
	{
		return Speed;
	}
}

///////////////////////////////////////
////////// UAnimNodeMirror ////////////
///////////////////////////////////////


void UAnimNodeMirror::GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	START_GETBONEATOM_TIMER

	if( GetCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num()) )
	{
		return;
	}

	check(Children.Num() == 1);

	// If mirroring is enabled, and the mirror info array is initialized correctly.
	if( bEnableMirroring )
	{
		EXCLUDE_CHILD_TIME
		GetMirroredBoneAtoms(Atoms, 0, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);

		// Save cached results if input is modified. Pass-through otherwise.
		SaveCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num());
	}
	// If no mirroring is going on, just pass right through.
	else
	{
		EXCLUDE_CHILD_TIME
		if( Children(0).Anim )
		{
			Children(0).Anim->GetBoneAtoms(Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
		}
		else
		{
			RootMotionDelta.SetIdentity();
			bHasRootMotion	= 0;
			FillWithRefPose(Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
		}
	}
}


///////////////////////////////////////
//////////// UAnimTree ////////////////
///////////////////////////////////////

void UAnimTree::PostLoad()
{
	Super::PostLoad();

	UBOOL bMarkDirty = FALSE;
#if WITH_EDITORONLY_DATA
	// Convert Previewing data to new profile system.
	if( GetLinkerVersion() < VER_ANIMTREE_PREVIEW_PROFILES )
	{
		bMarkDirty = TRUE;

		// Preview Mesh list
		PreviewMeshList.Empty(1);
		PreviewMeshList.AddZeroed(1);
		PreviewMeshIndex = 0;

		PreviewMeshList(0).DisplayName = FName(TEXT("Default"));
		PreviewMeshList(0).PreviewSkelMesh = PreviewSkelMesh_DEPRECATED;
		PreviewMeshList(0).PreviewMorphSets = PreviewMorphSets_DEPRECATED;

		PreviewMorphSets_DEPRECATED.Empty();
		PreviewSkelMesh_DEPRECATED = NULL;

		// Preview Socket list
		PreviewSocketList.Empty(1);
		PreviewSocketList.AddZeroed(1);
		PreviewSocketIndex = 0;

		PreviewSocketList(0).DisplayName = FName(TEXT("Default"));
		PreviewSocketList(0).SocketName = SocketName_DEPRECATED;
		PreviewSocketList(0).PreviewSkelMesh = SocketSkelMesh_DEPRECATED;
		PreviewSocketList(0).PreviewStaticMesh = SocketStaticMesh_DEPRECATED;

		// Preview AnimSets list
		PreviewAnimSetList.Empty(1);
		PreviewAnimSetList.AddZeroed(1);
		PreviewAnimSetList(0).DisplayName = FName(TEXT("Default"));
		PreviewAnimSetList(0).PreviewAnimSets = PreviewAnimSets_DEPRECATED;
		PreviewAnimSetListIndex = 0;
		PreviewAnimSetIndex = 0;

		PreviewAnimSets_DEPRECATED.Empty();
	}
#endif // WITH_EDITORONLY_DATA

	// 3 Pass Skeletal Mesh Compose
	if( GetLinkerVersion() < VER_THREE_PASS_SKELMESH_COMPOSE )
	{
		ComposePrePassBoneNames = PrioritizedSkelBranches_DEPRECATED;
		PrioritizedSkelBranches_DEPRECATED.Empty();
	}

	if( bMarkDirty && (GIsRunning || GIsUCC) )
	{
		MarkPackageDirty();
	}
}

void UAnimTree::InitAnim(USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent)
{
	START_INITANIM_TIMER
	{
		EXCLUDE_PARENT_TIME
		Super::InitAnim(meshComp, Parent);
	}

	if( meshComp )
	{
		// Calling this here isn't ideal and should be fixed.
		// This should be called when a new AnimTree is set, and provides new ComposePrePassBoneNames and ComposePostPassBoneNames.
		// @note when fixed, special care should be taken as some code calls InitAnimTree() directly without calling SetAnimTree();
		meshComp->bUpdateComposeSkeletonPasses = TRUE;
	}
}

void UAnimTree::ResetAnimNodeToSource(UAnimNode *SourceNode)
{
	Super::ResetAnimNodeToSource(SourceNode);

	// Disable saved pose
	SetUseSavedPose(FALSE);

	// Clear references to our Master Nodes. Force them to be reselected.
	INT const GroupsCount = AnimGroups.Num();
	for(INT GroupIdx=0; GroupIdx<GroupsCount; GroupIdx++)
	{
		FAnimGroup& AnimGroup = AnimGroups(GroupIdx);

		// Make sure we hold no reference to nodes from the Template
		AnimGroup.SynchMaster = NULL;
		AnimGroup.NotifyMaster = NULL;
		AnimGroup.SeqNodes.Empty();
	}
}

void UAnimTree::PostAnimNodeInstance(UAnimNode* SourceNode, TMap<UAnimNode*,UAnimNode*>& SrcToDestNodeMap)
{
	START_POSTANIMNODEINSTANCE_TIMER
	{
		EXCLUDE_POSTANIMNODEINSTANCE_PARENT_TIME
		Super::PostAnimNodeInstance(SourceNode, SrcToDestNodeMap);
	}

	// Clear references to our Master Nodes. Force them to be reselected.
	INT const GroupsCount = AnimGroups.Num();
	for(INT GroupIdx=0; GroupIdx<GroupsCount; GroupIdx++)
	{
		FAnimGroup& AnimGroup = AnimGroups(GroupIdx);

		// Make sure we hold no reference to nodes from the Template
		AnimGroup.SynchMaster = NULL;
		AnimGroup.NotifyMaster = NULL;
		AnimGroup.SeqNodes.Empty();
	}

	UAnimTree* SrcAnimTree = CastChecked<UAnimTree>(SourceNode);

	// Fix up AnimTickArray array
	if (bRebuildAnimTickArray)
	{
		AnimTickArray.Empty();
	}
	else
	{
		INT const NumNodes = SrcAnimTree->AnimTickArray.Num();
		AnimTickArray.Empty(NumNodes);
		AnimTickArray.Add(NumNodes);
		for(INT NodeIndex=0; NodeIndex<NumNodes; NodeIndex++)
		{
			UAnimNode** NewNode = SrcToDestNodeMap.Find( SrcAnimTree->AnimTickArray(NodeIndex) );
			if( NewNode == NULL )
			{
				warnf( TEXT("AnimTree was unable to instance a node:  %s (%s)"), *GetFullName(), *GetDetailedInfo() );
			}
			checkSlow( NewNode );
			AnimTickArray(NodeIndex) = *NewNode;
		}
	}

	// let our instance know if our Template had its ParentNodeArray already built, so we can skip that.
	bParentNodeArrayBuilt = SrcAnimTree->bParentNodeArrayBuilt;
}

void UAnimTree::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged 
		&& (PropertyThatChanged->GetFName() == FName(TEXT("ComposePrePassBoneNames")) || PropertyThatChanged->GetFName() == FName(TEXT("ComposePostPassBoneNames"))) )
	{
		if( SkelComponent )
		{
			SkelComponent->bUpdateComposeSkeletonPasses = TRUE;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/** Utility function, to check if a node is relevant for synchronization group. */
static inline UBOOL IsAnimNodeRelevantForSynchGroup(const UAnimNodeSequence* SeqNode)
{
	return SeqNode && SeqNode->AnimSeq && SeqNode->bSynchronize;
}

/** Utility function to check if a node is relevant for notification group. */
static inline UBOOL IsAnimNodeRelevantForNotifyGroup(const UAnimNodeSequence* SeqNode)
{
	return SeqNode && SeqNode->AnimSeq && !SeqNode->bNoNotifies;
}

// LOOKING_FOR_PERF_ISSUES
#if 0 && !FINAL_RELEASE
	#define PERF_DOINGANIMGROUPSDEBUG	1
	#define PERF_DEBUGANIMGROUPS(x) { ##x }
#else
	#define PERF_DEBUGANIMGROUPS(x)
	#define PERF_DOINGANIMGROUPSDEBUG	0
#endif
/** Remove from Sync Group **/
void UAnimTree::RemoveFromSyncGroup(UAnimNodeSequence * SeqNode)
{
	// Go through all groups, and clear our SeqNode cache
	INT const GroupsCount = AnimGroups.Num();
	for(INT GroupIdx=0; GroupIdx<GroupsCount; GroupIdx++)
	{
		FAnimGroup& AnimGroup = AnimGroups(GroupIdx);
		if (AnimGroup.SynchMaster == SeqNode)
		{
			AnimGroup.SynchMaster = NULL;
		}
		if (AnimGroup.NotifyMaster == SeqNode)
		{
			AnimGroup.NotifyMaster = NULL;
		}

		AnimGroup.SynchPctPosition = 0.f;
	}
}

void UAnimTree::SyncGroupPreTickUpdate()
{
	// Go through all groups, and clear our SeqNode cache
	INT const GroupsCount = AnimGroups.Num();
	for(INT GroupIdx=0; GroupIdx<GroupsCount; GroupIdx++)
	{
		FAnimGroup& AnimGroup = AnimGroups(GroupIdx);
		AnimGroup.SeqNodes.Empty();
	}
}

/** The main synchronization code... */
void UAnimTree::UpdateAnimNodeSeqGroups(FLOAT DeltaSeconds)
{
#ifdef PERF_DOINGANIMGROUPSDEBUG
	INT NumTotalUpdatedNodes = 0;
#endif

	// Go through all groups, and update all nodes.
	const INT GroupsCount = AnimGroups.Num();
	for(INT GroupIdx=0; GroupIdx<GroupsCount; GroupIdx++)
	{
		FAnimGroup& AnimGroup = AnimGroups(GroupIdx);

		// If we have no work to do, skip
		if( AnimGroup.SeqNodes.Num() == 0 )
		{
			continue;
		}

#ifdef PERF_DOINGANIMGROUPSDEBUG
		INT NumNodesUpdatedNoMaster = 0; 
		INT NumNodesUpdatedSync = 0;
		INT NumNodesUpdatedNoSync = 0;
#endif
		// If we have a SynchMaster and it is not relevant anymore, clear it.
		if( AnimGroup.SynchMaster && !IsAnimNodeRelevantForSynchGroup(AnimGroup.SynchMaster) )
		{
			AnimGroup.SynchMaster = NULL;
		}

		// Again, make sure our notification master is still relevant. Otherwise, clear it.
		if( AnimGroup.NotifyMaster && !IsAnimNodeRelevantForNotifyGroup(AnimGroup.NotifyMaster) )
		{
			AnimGroup.NotifyMaster = NULL;
		}

		// Calculate GroupDelta here, as this is common to all nodes in this group.
		FLOAT const GroupDelta = AnimGroup.RateScale * SkelComponent->GlobalAnimRateScale * DeltaSeconds;

		// If we don't have a valid synchronization master for this group, update all nodes without synchronization.
		if( !IsAnimNodeRelevantForSynchGroup(AnimGroup.SynchMaster) )
		{
			INT const SeqNodesCount = AnimGroup.SeqNodes.Num();
			for(INT i=0; i<SeqNodesCount; i++)
			{
				UAnimNodeSequence* SeqNode = AnimGroup.SeqNodes(i);
				if( SeqNode )
				{
					// Keep track of PreviousTime before any update. This is used by Root Motion.
					SeqNode->PreviousTime = SeqNode->CurrentTime;
					if( SeqNode->AnimSeq && SeqNode->bPlaying )
					{
						FLOAT const MoveDelta = GroupDelta * SeqNode->Rate * SeqNode->AnimSeq->RateScale;
						// Fire notifies if node is notification master
						UBOOL const bFireNotifies = (SeqNode == AnimGroup.NotifyMaster);
						// Advance animation.
						SeqNode->AdvanceBy(MoveDelta, DeltaSeconds, bFireNotifies);

						PERF_DEBUGANIMGROUPS( NumNodesUpdatedNoMaster++; )
					}
				}
			}
		}
		// Now that we have the master node, have it update all the others
		else 
		{
			UAnimNodeSequence* SynchMaster = AnimGroup.SynchMaster;
			FLOAT const MasterMoveDelta = GroupDelta * SynchMaster->Rate * SynchMaster->AnimSeq->RateScale;
			FLOAT const MasterPrevSynchPct = SynchMaster->GetGroupRelativePosition();

			{
				// Keep track of PreviousTime before any update. This is used by Root Motion.
				SynchMaster->PreviousTime = SynchMaster->CurrentTime;

				if( SynchMaster->bPlaying )
				{
					// Fire notifies if node is notification master
					UBOOL const bFireNotifies = (SynchMaster == AnimGroup.NotifyMaster);
					// Advance Synch Master Node
					SynchMaster->AdvanceBy(MasterMoveDelta, DeltaSeconds, bFireNotifies);

					PERF_DEBUGANIMGROUPS( NumNodesUpdatedSync++; )
				}
			}

			// SynchMaster node was changed during the tick?
			// Skip this round of updates...
			if( AnimGroup.SynchMaster != SynchMaster )
			{
				continue;
			}

			// Find it's relative position on its time line.
			AnimGroup.SynchPctPosition = SynchMaster->GetGroupRelativePosition();

			// Update slaves to match relative position of master node.
			INT const SeqNodesCount = AnimGroup.SeqNodes.Num();
			for(INT i=0; i<SeqNodesCount; i++)
			{
				UAnimNodeSequence* SeqNode = AnimGroup.SeqNodes(i);
				if( SeqNode && SeqNode != SynchMaster )
				{
					// If node should be synchronized
					if( IsAnimNodeRelevantForSynchGroup(SeqNode) && SeqNode->AnimSeq->SequenceLength > 0.f )
					{
						// Set the previous time to what it would've been had this node been relevant the whole time.
						SeqNode->SetPosition(SeqNode->FindGroupPosition(MasterPrevSynchPct), FALSE);

						// Slave's new time
						const FLOAT NewTime		= SeqNode->FindGroupPosition(AnimGroup.SynchPctPosition);
						FLOAT SlaveMoveDelta	= appFmod(NewTime - SeqNode->CurrentTime, SeqNode->AnimSeq->SequenceLength);

						// Make sure SlaveMoveDelta And MasterMoveDelta are the same sign, so they both move in the same direction.
						if( SlaveMoveDelta * MasterMoveDelta < 0.f )
						{
							if( SlaveMoveDelta >= 0.f )
							{
								SlaveMoveDelta -= SeqNode->AnimSeq->SequenceLength;
							}
							else
							{
								SlaveMoveDelta += SeqNode->AnimSeq->SequenceLength;
							}
						}

						// Fire notifies if node is master of notification group
						const UBOOL	bFireNotifies = (SeqNode == AnimGroup.NotifyMaster);
						// Move slave node to correct position
						SeqNode->AdvanceBy(SlaveMoveDelta, DeltaSeconds, bFireNotifies);

						PERF_DEBUGANIMGROUPS( NumNodesUpdatedSync++; )
					}
					// If node shouldn't be synchronized, update it normally
					else if( SeqNode->AnimSeq && SeqNode->bPlaying )
					{
						// Keep track of PreviousTime before any update. This is used by Root Motion.
						SeqNode->PreviousTime		= SeqNode->CurrentTime;
						const FLOAT MoveDelta		= GroupDelta * SeqNode->Rate * SeqNode->AnimSeq->RateScale;
						// Fire notifies if node is notification master
						const UBOOL	bFireNotifies	= (SeqNode == AnimGroup.NotifyMaster);
						// Advance animation.
						SeqNode->AdvanceBy(MoveDelta, DeltaSeconds, bFireNotifies);

						PERF_DEBUGANIMGROUPS( NumNodesUpdatedNoSync++; )
					}
				}
			}
		}

		PERF_DEBUGANIMGROUPS( 
			debugf(TEXT("UpdateAnimNodeSeqGroups. GroupName: %s, NumNodesUpdatedNoMaster: %d, NumNodesUpdatedSync: %d, NumNodesUpdatedNoSync: %d"), *AnimGroup.GroupName.ToString(), NumNodesUpdatedNoMaster, NumNodesUpdatedSync, NumNodesUpdatedNoSync); 
		NumTotalUpdatedNodes += (NumNodesUpdatedNoMaster + NumNodesUpdatedSync + NumNodesUpdatedNoSync);
		)
	}

	PERF_DEBUGANIMGROUPS( debugf(TEXT("UpdateAnimNodeSeqGroups. NumTotalUpdatedNodes: %d"), NumTotalUpdatedNodes); )
}

/** 
 *  Custom Serialize function
 *  This function will save some information we can use in InitAnimTree
 */
void UAnimTree::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

#if USE_SLOTNODE_ANIMSEQPOOL
	if ( Ar.Ver() < VER_ADDED_ANIMNODESLOTPOOL )
	{
		// mark as rebuild animtick array
		bRebuildAnimTickArray = TRUE;
	}
#endif
}

/** Add a node to an existing group */
UBOOL UAnimTree::SetAnimGroupForNode(class UAnimNodeSequence* SeqNode, FName GroupName, UBOOL bCreateIfNotFound)
{
	if( !SeqNode )
	{	
		return FALSE;
	}

	// If node is already in this group, then do nothing
	if( SeqNode->SynchGroupName == GroupName )
	{
		return TRUE;
	}

	// If node is already part of a group, remove it from there first.
	if( SeqNode->SynchGroupName != NAME_None )
	{
		INT const GroupIndex = GetGroupIndex(SeqNode->SynchGroupName);
		if( GroupIndex != INDEX_NONE )
		{
			FAnimGroup& AnimGroup = AnimGroups(GroupIndex);
			SeqNode->SynchGroupName = NAME_None;

			// If we're removing a Master Node, clear reference to it.
			if( AnimGroup.SynchMaster == SeqNode )
			{
				AnimGroup.SynchMaster = NULL;
			}
			// If we're removing a Master Node, clear reference to it.
			if( AnimGroup.NotifyMaster == SeqNode )
			{
				AnimGroup.NotifyMaster = NULL;
			}
		}
	}

	// See if we can move the node to the new group
	if( GroupName != NAME_None )
	{
		INT GroupIndex = GetGroupIndex(GroupName);
		
		// If group wasn't found, maybe we should create it
		if( GroupIndex == INDEX_NONE && bCreateIfNotFound )
		{
			GroupIndex = AnimGroups.AddZeroed();
			AnimGroups(GroupIndex).RateScale = 1.f;
			AnimGroups(GroupIndex).GroupName = GroupName;
		}

		if( GroupIndex != INDEX_NONE )
		{
			// Set group name
			SeqNode->SynchGroupName = GroupName;
		}
	}

	// return TRUE if change was successful
	return (SeqNode->SynchGroupName == GroupName);
}

/** Force a group at a relative position. */
void UAnimTree::ForceGroupRelativePosition(FName GroupName, FLOAT RelativePosition)
{
	INT const GroupIndex = GetGroupIndex(GroupName);
	if( GroupIndex != INDEX_NONE )
	{
		FAnimGroup& AnimGroup = AnimGroups(GroupIndex);
		// Update group position
		AnimGroup.SynchPctPosition = RelativePosition;

		if( AnimGroup.SynchMaster )
		{
			AnimGroup.SynchMaster->SetPosition(AnimGroup.SynchMaster->FindGroupPosition(RelativePosition), FALSE);
		}

		PERF_DEBUGANIMGROUPS( debugf(TEXT("ForceGroupRelativePosition. GroupName: %s, RelativePosition: %f"), *AnimGroup.GroupName.ToString(), RelativePosition); )
		for( INT i=0; i<AnimGroup.SeqNodes.Num(); i++ )
		{
			UAnimNodeSequence* SeqNode = AnimGroup.SeqNodes(i);
			if( IsAnimNodeRelevantForSynchGroup(SeqNode) && SeqNode != AnimGroup.SynchMaster )
			{
				PERF_DEBUGANIMGROUPS( debugf(TEXT("  Setting position on SeqNode: %s, AnimSeqName: %s"), *SeqNode->NodeName.ToString(), *SeqNode->AnimSeqName.ToString()); )
				SeqNode->SetPosition(SeqNode->FindGroupPosition(RelativePosition), FALSE);
			}
		}
	}
	else
	{
		PERF_DEBUGANIMGROUPS( debugf(TEXT("ForceGroupRelativePosition. GroupName: %s Not found!!"), *GroupName.ToString(), RelativePosition); )
	}
}

/** Get the relative position of a group. */
FLOAT UAnimTree::GetGroupRelativePosition(FName GroupName)
{
	INT const GroupIndex = GetGroupIndex(GroupName);
	if( GroupIndex != INDEX_NONE )
	{
		PERF_DEBUGANIMGROUPS( debugf(TEXT("GetGroupRelativePosition. GroupName: %s, RelativePosition: %f"), *AnimGroups(GroupIndex).GroupName.ToString(), AnimGroups(GroupIndex).SynchPctPosition); )
		return AnimGroups(GroupIndex).SynchPctPosition;
	}

	PERF_DEBUGANIMGROUPS( debugf(TEXT("GetGroupRelativePosition. GroupName: %s not found (GroupIndex: %d)!!!"), *GroupName.ToString(), GroupIndex); )
	return 0.f;
}

/** Returns the master node driving synchronization for this group. */
UAnimNodeSequence* UAnimTree::GetGroupSynchMaster(FName GroupName)
{
	INT const GroupIndex = GetGroupIndex(GroupName);
	if( GroupIndex != INDEX_NONE )
	{
		return AnimGroups(GroupIndex).SynchMaster;
	}

	return NULL;
}

/** Returns the master node driving notifications for this group. */
UAnimNodeSequence* UAnimTree::GetGroupNotifyMaster(FName GroupName)
{
	INT const GroupIndex = GetGroupIndex(GroupName);
	if( GroupIndex != INDEX_NONE )
	{
		return AnimGroups(GroupIndex).NotifyMaster;
	}

	return NULL;
}

/** Adjust the Rate Scale of a group */
void UAnimTree::SetGroupRateScale(FName GroupName, FLOAT NewRateScale)
{
	INT const GroupIndex = GetGroupIndex(GroupName);
	if( GroupIndex != INDEX_NONE )
	{
		AnimGroups(GroupIndex).RateScale = NewRateScale;
	}
}

/** Get the Rate Scale of a group */
FLOAT UAnimTree::GetGroupRateScale(FName GroupName)
{
	INT const GroupIndex = GetGroupIndex(GroupName);
	if( GroupIndex != INDEX_NONE )
	{
		return AnimGroups(GroupIndex).RateScale;
	}

	return 0.f;
}

/** 
 * Returns the index in the AnimGroups list of a given GroupName.
 * If group cannot be found, then INDEX_NONE will be returned.
 */
INT UAnimTree::GetGroupIndex(FName GroupName)
{
	if( GroupName != NAME_None )
	{
		for(INT GroupIdx=0; GroupIdx<AnimGroups.Num(); GroupIdx++ )
		{
			if( AnimGroups(GroupIdx).GroupName == GroupName )
			{
				return GroupIdx;
			}
		}
	}

	return INDEX_NONE;
}

/** Get all SkelControls within this AnimTree. */
void UAnimTree::GetSkelControls(TArray<USkelControlBase*>& OutControls)
{
	OutControls.Empty();

	// Iterate over array of list-head structs.
	for(INT i=0; i<SkelControlLists.Num(); i++)
	{
		// Then walk down each list, adding the SkelControl if its not already in the array.
		USkelControlBase* Control = SkelControlLists(i).ControlHead;
		while(Control)
		{
			OutControls.AddUniqueItem(Control);
			Control = Control->NextControl;
		}
	}
}

/** Get all MorphNodes within this AnimTree. */
void UAnimTree::GetMorphNodes(TArray<UMorphNodeBase*>& OutNodes)
{
	// Firest empty the array we will put nodes into.
	OutNodes.Empty();

	// Iterate over each node connected to the root.
	for(INT i=0; i<RootMorphNodes.Num(); i++)
	{
		// If non-NULL, call GetNodes. This will add itself and any children.
		if( RootMorphNodes(i) )
		{
			RootMorphNodes(i)->GetNodes(OutNodes);
		}
	}
}

/** Calls GetActiveMorphs on each element of the RootMorphNodes array. */
void UAnimTree::GetTreeActiveMorphs(TArray<FActiveMorph>& OutMorphs)
{
	// Iterate over each node connected to the root.
	for(INT i=0; i<RootMorphNodes.Num(); i++)
	{
		// If non-NULL, call GetNodes. This will add itself and any children.
		if( RootMorphNodes(i) )
		{
			RootMorphNodes(i)->GetActiveMorphs(OutMorphs);
		}
	}
}

/** Call InitMorph on all morph nodes attached to the tree. */
void UAnimTree::InitTreeMorphNodes(USkeletalMeshComponent* InSkelComp)
{
	TArray<UMorphNodeBase*>	AllNodes;
	GetMorphNodes(AllNodes);

	for(INT i=0; i<AllNodes.Num(); i++)
	{
		if(AllNodes(i))
		{
			AllNodes(i)->InitMorphNode(InSkelComp);
		}
	}
}

/** Utility for find a SkelControl in an AnimTree by name. */
USkelControlBase* UAnimTree::FindSkelControl(FName InControlName)
{
	// Always return NULL if we did not pass in a name.
	if(InControlName == NAME_None)
	{
		return NULL;
	}

	// Iterate over array of list-head structs.
	for(INT i=0; i<SkelControlLists.Num(); i++)
	{
		// Then walk down each list, adding the SkelControl if its not already in the array.
		USkelControlBase* Control = SkelControlLists(i).ControlHead;
		while(Control)
		{
			if( Control->ControlName == InControlName )
			{
				return Control;
			}
			Control = Control->NextControl;
		}
	}

	return NULL;
}

/** Utility for find a MorphNode in an AnimTree by name. */
UMorphNodeBase* UAnimTree::FindMorphNode(FName InNodeName)
{
	// Always return NULL if we did not pass in a name.
	if(InNodeName == NAME_None)
	{
		return NULL;
	}

	TArray<UMorphNodeBase*>	MorphNodes;
	GetMorphNodes(MorphNodes);

	for(INT i=0; i<MorphNodes.Num(); i++)
	{
		if(MorphNodes(i)->NodeName == InNodeName)
		{
			return MorphNodes(i);
		}
	}

	return NULL;
}

/** Used to return saved pose if desired.  */
void UAnimTree::GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	if(bUseSavedPose)
	{
		check(SavedPose.Num() == Atoms.Num());

		Atoms = SavedPose; // This will fill in all bones, not just DEsiredBones, but that should be fine - faster than looping and copying one at a time
		RootMotionDelta = FBoneAtom::Identity;
		bHasRootMotion = 0;
	}
	else
	{
		Super::GetBoneAtoms(Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
	}
}

/** 
 *	When passing in TRUE, will cause tree to evaluate and then save resulting pose. From then on will continue to use that saved pose instead of re-evaluating the tree 
 *	This feature is turned off when the SkeletalMesh changes
 */
void UAnimTree::SetUseSavedPose(UBOOL bSavePose)
{
	// Eval tree and save pose if necessary
	if(bSavePose)
	{
		if(SkelComponent && SkelComponent->SkeletalMesh)
		{
			INT NumBones = SkelComponent->SkeletalMesh->RefSkeleton.Num();

			// Build array to request all bones
			TArray<BYTE> RequiredBones;
			RequiredBones.Add(NumBones);
			for(INT i=0; i<NumBones; i++)
			{
				RequiredBones(i) = i;
			}

			// Make sure cached bone atoms array is sized correctly
			SavedPose.Empty();
			SavedPose.Add(NumBones);			

			FBoneAtom DummyRootMotionDelta = FBoneAtom::Identity;
			INT bDummyHasRootMotion = 0;

			if(Children.Num() < 1 || Children(0).Anim == NULL)
			{
				FillWithRefPose(SavedPose, RequiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
			}
			else
			{
				// Before we call GetBoneAtoms, we need to make sure all the nodes have been properly initialized
				// With deferred initialization, if someone changed some weights since last tick, we may face unintialized nodes
				UAnimNode::CurrentSearchTag++;
				check(SkelComponent->Animations == this);
				SkelComponent->Animations->CallDeferredInitAnim();

				FMemMark Mark(GMainThreadMemStack);

				FBoneAtomArray OutAtoms;
				OutAtoms.Add(NumBones);

				FCurveKeyArray DummyCurveKeys;
				Children(0).Anim->GetBoneAtoms(OutAtoms, RequiredBones, DummyRootMotionDelta, bDummyHasRootMotion, DummyCurveKeys);

				SavedPose = OutAtoms;

				Mark.Pop();
			}

			bUseSavedPose = TRUE;
		}
		else
		{
			debugf(TEXT("SetUseSavedPose : Unable to generate pose to save. (%s)"), *GetPathName());

			SavedPose.Empty();
			bUseSavedPose = FALSE;
		}
	}
	else
	{
		// Clear cached array if no longer needed
		SavedPose.Empty();
		bUseSavedPose = FALSE;
	}
}

#if PERF_SHOW_COPYANIMTREE_TIMES
static FName NAME_CopyAnimTree_CopyAnimNodes_ConstructObjects = FName(TEXT("CopyAnimTree_CopyAnimNodes_ConstructObjects"));
static FName NAME_CopyAnimTree_CopyAnimNodes_FixUpPointers = FName(TEXT("CopyAnimTree_CopyAnimNodes_FixUpPointers"));
#endif

void UAnimTree::CopyAnimNodes(const TArray<UAnimNode*>& SrcNodes, UObject* NewOuter, TArray<UAnimNode*>& DestNodes, TMap<UAnimNode*,UAnimNode*>& SrcToDestNodeMap)
{
	DWORD OldHackFlags = GUglyHackFlags;
	GUglyHackFlags |= HACK_DisableSubobjectInstancing | HACK_FastPathUniqueNameGeneration; // Disable subobject instancing. Will not be needed when we can remove 'editinline export' from AnimTree pointers.

	// Duplicate each AnimNode in tree.
	{
		POSTANIMNODEINSTANCE_CUSTOM(NAME_CopyAnimTree_CopyAnimNodes_ConstructObjects)
		for(INT i=0; i<SrcNodes.Num(); i++)
		{
			UAnimNode* NewNode = ConstructObject<UAnimNode>( SrcNodes(i)->GetClass(), NewOuter, NAME_None, 0, SrcNodes(i) );
			NewNode->SetArchetype( SrcNodes(i)->GetClass()->GetDefaultObject() );
			DestNodes.AddItem(NewNode);
			SrcToDestNodeMap.Set( SrcNodes(i), NewNode );
		}
	}

	// Then fix up pointers.
	{
		POSTANIMNODEINSTANCE_CUSTOM(NAME_CopyAnimTree_CopyAnimNodes_FixUpPointers)
		for(INT i=0; i<DestNodes.Num(); i++)
		{
			// Only UAnimNodeBlendBase classes have references to other AnimNodes.
			UAnimNodeBlendBase* NewBlend = Cast<UAnimNodeBlendBase>( DestNodes(i) );
			if( NewBlend )
			{
				for(INT j=0; j<NewBlend->Children.Num(); j++)
				{
					if( NewBlend->Children(j).Anim )
					{
						UAnimNode** NewNode = SrcToDestNodeMap.Find(NewBlend->Children(j).Anim);
						if( NewNode )
						{
							check(*NewNode);
							NewBlend->Children(j).Anim = *NewNode;
						}

						checkSlow( NewBlend->Children(j).Anim->GetOuter() == NewOuter );
					}
				}
			}
		}
	}

	GUglyHackFlags = OldHackFlags;
}

void UAnimTree::CopySkelControls(const TArray<USkelControlBase*>& SrcControls, UObject* NewOuter, TArray<USkelControlBase*>& DestControls, TMap<USkelControlBase*,USkelControlBase*>& SrcToDestControlMap)
{
	DWORD OldHackFlags = GUglyHackFlags;
	GUglyHackFlags |= HACK_DisableSubobjectInstancing | HACK_FastPathUniqueNameGeneration; // Disable subobject instancing. Will not be needed when we can remove 'editinline export' from AnimTree pointers.

	for(INT i=0; i<SrcControls.Num(); i++)
	{
		USkelControlBase* NewControl = ConstructObject<USkelControlBase>( SrcControls(i)->GetClass(), NewOuter, NAME_None, 0, SrcControls(i) );
		NewControl->SetArchetype( SrcControls(i)->GetClass()->GetDefaultObject() );
		DestControls.AddItem(NewControl);
		SrcToDestControlMap.Set( SrcControls(i), NewControl );
	}

	// Then we fix up 'NextControl' pointers.
	for(INT i=0; i<DestControls.Num(); i++)
	{
		if(DestControls(i)->NextControl)
		{
			USkelControlBase** NewControl = SrcToDestControlMap.Find(DestControls(i)->NextControl);
			if(NewControl)
			{
				check(*NewControl);
				DestControls(i)->NextControl = *NewControl;
			}
		}
	}

	GUglyHackFlags = OldHackFlags;
}

void UAnimTree::CopyMorphNodes(const TArray<class UMorphNodeBase*>& SrcNodes, UObject* NewOuter, TArray<class UMorphNodeBase*>& DestNodes, TMap<class UMorphNodeBase*,class UMorphNodeBase*>& SrcToDestNodeMap)
{
	DWORD OldHackFlags = GUglyHackFlags;
	GUglyHackFlags |= HACK_DisableSubobjectInstancing | HACK_FastPathUniqueNameGeneration; // Disable subobject instancing. Will not be needed when we can remove 'editinline export' from AnimTree pointers.

	for(INT i=0; i<SrcNodes.Num(); i++)
	{
		UMorphNodeBase* NewNode = ConstructObject<UMorphNodeBase>( SrcNodes(i)->GetClass(), NewOuter, NAME_None, 0, SrcNodes(i) );
		NewNode->SetArchetype( SrcNodes(i)->GetClass()->GetDefaultObject() );
		DestNodes.AddItem(NewNode);
		SrcToDestNodeMap.Set( SrcNodes(i), NewNode );
	}

	// Then we fix up links in the NodeConns array.
	for(INT i=0; i<DestNodes.Num(); i++)
	{
		UMorphNodeWeightBase* WeightNode = Cast<UMorphNodeWeightBase>( DestNodes(i) );
		if(WeightNode)
		{
			// Iterate over each connector
			for(INT j=0; j<WeightNode->NodeConns.Num(); j++)
			{
				FMorphNodeConn& Conn = WeightNode->NodeConns(j);

				// Iterate over each link from this connector.
				for(INT k=0; k<Conn.ChildNodes.Num(); k++)
				{
					// If this is a pointer to another node, look it up in the SrcToDestNodeMap and replace the reference.
					if( Conn.ChildNodes(k) )
					{
						UMorphNodeBase** NewNode = SrcToDestNodeMap.Find( Conn.ChildNodes(k) );
						if(NewNode)
						{
							check(*NewNode);
							Conn.ChildNodes(k) = *NewNode;
						}
					}
				}
			}
		}
	}

	GUglyHackFlags = OldHackFlags;
}

INT UAnimTree::GetPoolSize(void)
{
	AWorldInfo *WorldInfo = GWorld->GetWorldInfo();
	if (WorldInfo && WorldInfo->GRI)
	{
		AGameInfo *GameInfo = WorldInfo->GRI->GetDefaultGameInfo();
		if (GameInfo)
		{
			return GameInfo->AnimTreePoolSize;
		}
	}
	return 0;
}

void UAnimTree::ReturnToPool(void)
{	
	if (GWorld && AnimTreeTemplate && AnimTreeTemplate->bEnablePooling)
	{	
		INT FoundTemplate = 0;
		check(this);

		for(INT i=0; i<GWorld->AnimTreePool.Num(); i++)
		{
			UAnimTree *CheckTree = GWorld->AnimTreePool(i);
			check (CheckTree != this);
			if (GWorld->AnimTreePool(i)->AnimTreeTemplate == this->AnimTreeTemplate)
			{
				FoundTemplate++;
			}
		}

		if (FoundTemplate >= GetPoolSize())
		{
			// If we have too many of this type, don't pool it and just let it get GCd
			return;
		}

		GWorld->AnimTreePool.Push(this);
	}
}

UAnimTree* UAnimTree::CopyAnimTree(UObject* NewTreeOuter, UBOOL bAcceptPooled)
{
#if PERF_SHOW_COPYANIMTREE_TIMES
	static FName NAME_CopyAnimTree_GetNodes = FName(TEXT("CopyAnimTree_GetNodes"));
	static FName NAME_CopyAnimTree_GetSkelControls = FName(TEXT("CopyAnimTree_GetSkelControls"));
	static FName NAME_CopyAnimTree_CopySkelControls = FName(TEXT("CopyAnimTree_CopySkelControls"));
	static FName NAME_CopyAnimTree_GetMorphNodes = FName(TEXT("CopyAnimTree_GetMorphNodes"));
	static FName NAME_CopyAnimTree_CopyMorphNodes = FName(TEXT("CopyAnimTree_CopyMorphNodes"));
	static FName NAME_CopyAnimTree_FixUpAnimTree = FName(TEXT("CopyAnimTree_FixUpAnimTree"));

	PostAnimNodeInstanceStats.Empty();
	PostAnimNodeInstanceStatsTMap.Empty();

	DOUBLE Start = appSeconds();	
#endif
#define CAPTURE_COPYANIMTREE_PERF 0
#if CAPTURE_COPYANIMTREE_PERF
	FString AnimTreeName = *GetPathName();
	FString CheckName = FString::Printf(TEXT("AT_MarcusLayered"));
	debugf(TEXT("CAPTURE_COPYANIMTREE_PERF AnimTreeName: %s, CheckName: %s"), *AnimTreeName, *CheckName);
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

	UAnimTree* NewTree = NULL;

	if (GWorld && bAcceptPooled)
	{
		// Let's see if we can find one in the pool
		
		for(INT i=0; i<GWorld->AnimTreePool.Num(); i++)
		{				
			if (GWorld->AnimTreePool(i)->AnimTreeTemplate == this)
			{
				NewTree = GWorld->AnimTreePool(i);
				GWorld->AnimTreePool.Remove(i);

				// Do some fixup on the pooled nodes
				TArray<UAnimNode*> DestNodes;
				NewTree->GetNodes(DestNodes, TRUE);

				TArray<UAnimNode*> SrcNodes;
				GetNodes(SrcNodes, TRUE);								

				TArray<USkelControlBase*> DestControls;
				NewTree->GetSkelControls(DestControls);

				TArray<USkelControlBase*> SrcControls;				
				GetSkelControls(SrcControls);

				TArray<UMorphNodeBase*> DestMorphNodes;
				NewTree->GetMorphNodes(DestMorphNodes);

				TArray<UMorphNodeBase*> SrcMorphNodes;
				GetMorphNodes(SrcMorphNodes);

				if (SrcNodes.Num() != DestNodes.Num() || SrcControls.Num() != DestControls.Num() || SrcMorphNodes.Num() != DestMorphNodes.Num())
				{
					// This can happen if runtime data is modified to not match source data. We should not be pooling such trees
					warnf(TEXT("AnimTree pooling failed for AnimTreeName: %s, needs to be resaved or is modified at runtime"), *GetPathName());
					NewTree = NULL;
					break;
				}
			
				for(INT i=0; i<DestNodes.Num(); i++)
				{
					UAnimNode* DestNode = DestNodes(i);
					UAnimNode* SrcNode = SrcNodes(i);

					DestNode->ResetAnimNodeToSource(SrcNode);
				}

				break;
			}
		}		
	}

	if (!NewTree)
	{	
		// Construct the new AnimTree object.
		DWORD OldHackFlags = GUglyHackFlags;
		GUglyHackFlags |= HACK_DisableSubobjectInstancing; // Disable sub-object instancing. Will not be needed when we can remove 'editinline export' from AnimTree pointers.
		NewTree = ConstructObject<UAnimTree>( GetClass(), NewTreeOuter, NAME_None, 0, this, INVALID_OBJECT );
		GUglyHackFlags = OldHackFlags;

		if (AnimTreeTemplate)
		{
			NewTree->AnimTreeTemplate = AnimTreeTemplate;
		}
		else
		{
			NewTree->AnimTreeTemplate = this;
		}

		// Then get all the nodes in the source tree, excluding the tree itself (ie this object).
		TArray<UAnimNode*> SrcNodes;
		{
			POSTANIMNODEINSTANCE_CUSTOM(NAME_CopyAnimTree_GetNodes)
			GetNodes(SrcNodes, TRUE);
			verify(SrcNodes.RemoveItem(this) > 0);
		}

		// Duplicate all the AnimNodes	
		TArray<UAnimNode*> DestNodes; // Array of newly created nodes.
		DestNodes.Empty(SrcNodes.Num());
		TMap<UAnimNode*,UAnimNode*> SrcToDestNodeMap; // Mapping table from src node to newly created node.
		SrcToDestNodeMap.Empty(SrcNodes.Num());
		{
			CopyAnimNodes(SrcNodes, NewTree, DestNodes, SrcToDestNodeMap);
		}


		// Finally we fix up references in the new AnimTree itself (root AnimNode, head of each SkelControl chain, and root MorphNodes).
	
		POSTANIMNODEINSTANCE_CUSTOM(NAME_CopyAnimTree_FixUpAnimTree)
		check(NewTree->Children.Num() == 1);
		if(NewTree->Children(0).Anim)
		{
			UAnimNode** NewNode = SrcToDestNodeMap.Find(NewTree->Children(0).Anim);
			check(NewNode && *NewNode); // When we copy the entire tree, we should always find the node we want.
			NewTree->Children(0).Anim = *NewNode;
		}
		
		// Add AnimTree to SrcToDestNodeMap to nodes can do proper fix up.
		SrcToDestNodeMap.Set(this, NewTree);

		// more node pointer fix up
		for(INT i=0; i<DestNodes.Num(); i++)
		{
			UAnimNode* DestNode = DestNodes(i);
			UAnimNode* SrcNode = SrcNodes(i);

			INT const NumParentNodes = SrcNode->ParentNodes.Num();
			DestNode->ParentNodes.Empty(NumParentNodes);
			DestNode->ParentNodes.Add(NumParentNodes);

			// Fix up ParentNode array
			for(INT ParentNodeIndex=0; ParentNodeIndex<NumParentNodes; ParentNodeIndex++)
			{
				checkSlow( SrcNode->ParentNodes(ParentNodeIndex) );
				UAnimNode** NewNode = SrcToDestNodeMap.Find( SrcNode->ParentNodes(ParentNodeIndex) );
				checkSlow( NewNode );
				DestNode->ParentNodes(ParentNodeIndex) = CastChecked<UAnimNodeBlendBase>(*NewNode);
			}

			// Allow node to do custom work post-instance
			{
				EXCLUDE_POSTANIMNODEINSTANCE_PARENT_TIME
				DestNodes(i)->PostAnimNodeInstance( SrcNodes(i), SrcToDestNodeMap );
			}
		}

		// call PostAnimNodeInstance on animtree node as well - allows subclasses of AnimTree to do things after instantiation
		NewTree->PostAnimNodeInstance(this, SrcToDestNodeMap);

		// need to copy variable so that new tree can use this
		NewTree->bRebuildAnimTickArray = bRebuildAnimTickArray;
	}

	// Copy over the morph targets and skelcontrols in either case

	// Now we get all the SkelControls in this tree
	TArray<USkelControlBase*> SrcControls;
	{
		POSTANIMNODEINSTANCE_CUSTOM(NAME_CopyAnimTree_GetSkelControls)
		GetSkelControls(SrcControls);
	}

	// Duplicate all the SkelControls
	TArray<USkelControlBase*> DestControls; // Array of new skel controls.
	DestControls.Empty(SrcControls.Num());
	TMap<USkelControlBase*, USkelControlBase*> SrcToDestControlMap; // Map from src control to newly created one.
	SrcToDestControlMap.Empty(SrcControls.Num());
	{
		POSTANIMNODEINSTANCE_CUSTOM(NAME_CopyAnimTree_CopySkelControls)
		CopySkelControls(SrcControls, NewTree, DestControls, SrcToDestControlMap);
	}

	// Now we get all the MorphNodes in this tree
	TArray<UMorphNodeBase*> SrcMorphNodes;
	{
		POSTANIMNODEINSTANCE_CUSTOM(NAME_CopyAnimTree_GetMorphNodes)
		GetMorphNodes(SrcMorphNodes);
	}

	// Duplicate all the MorphNodes
	TArray<UMorphNodeBase*> DestMorphNodes; // Array of new morph nodes.
	DestMorphNodes.Empty(SrcMorphNodes.Num());
	TMap<UMorphNodeBase*, UMorphNodeBase*> SrcToDestMorphNodeMap; // Map from src node to newly created one.
	SrcToDestMorphNodeMap.Empty(DestMorphNodes.Num());
	{
		POSTANIMNODEINSTANCE_CUSTOM(NAME_CopyAnimTree_CopyMorphNodes)
		CopyMorphNodes(SrcMorphNodes, NewTree, DestMorphNodes, SrcToDestMorphNodeMap);
	}

	for(INT i=0; i<NewTree->SkelControlLists.Num(); i++)
	{
		if(NewTree->SkelControlLists(i).ControlHead)
		{
			USkelControlBase** NewControl = SrcToDestControlMap.Find(SkelControlLists(i).ControlHead);
			check(NewControl && *NewControl);
			NewTree->SkelControlLists(i).ControlHead = *NewControl;
		}
	}

	for(INT i=0; i<NewTree->RootMorphNodes.Num(); i++)
	{
		if( NewTree->RootMorphNodes(i) )
		{
			UMorphNodeBase** NewNode = SrcToDestMorphNodeMap.Find(RootMorphNodes(i) );
			check(*NewNode);
			NewTree->RootMorphNodes(i) = *NewNode;
		}
	}

	
#if CAPTURE_COPYANIMTREE_PERF
	appStopCPUTrace( NAME_Game );
#endif

#if PERF_SHOW_COPYANIMTREE_TIMES
	debugf(TEXT("CopyAnimTree: %f ms"), (appSeconds() - Start) * 1000.f);

	// Sort results (slowest first)
	Sort<USE_COMPARE_CONSTREF(FAnimNodeTimeStat,UnAnimTree)>( &PostAnimNodeInstanceStats(0), PostAnimNodeInstanceStats.Num() );

	debugf(TEXT(" ======= PostAnimNodeInstance - TIMING - %s %s"), *GetPathName(), SkelComponent && SkelComponent->SkeletalMesh?*SkelComponent->SkeletalMesh->GetName():TEXT("NONE"));
	FLOAT TotalBlendTime = 0.f;
	for(INT i=0; i<PostAnimNodeInstanceStats.Num(); i++)
	{
		debugf(TEXT("%fms\t%s"), PostAnimNodeInstanceStats(i).NodeExclusiveTime * 1000.f, *PostAnimNodeInstanceStats(i).NodeName.ToString());
		TotalBlendTime += PostAnimNodeInstanceStats(i).NodeExclusiveTime;
	}
	debugf(TEXT(" ======= Total Exclusive Time: %fms"), TotalBlendTime * 1000.f);
#endif

	return NewTree;
}

/** 
* This returns total size for this animtree including animnodes, skelcontrols, morphnodes
* This is only for ListAnimTress to get idea of how much size each tree cost in run-time
* Do not use this in GetResourceSize as it will be calculate multiple times of same data
*/
INT UAnimTree::GetTotalNodeBytes()
{
	INT nTotalSize=0;

	// Get all animnodes in this tree, and add the size
	// this function adds itself as well
	TArray<UAnimNode*> AnimNodes;
	GetNodes(AnimNodes);

	for (INT I=0; I<AnimNodes.Num(); ++I)
	{
		FArchiveCountMem Count( AnimNodes(I) );
		nTotalSize += Count.GetNum();
	}

	// Now we get all the SkelControls in this tree
	TArray<USkelControlBase*> SkelControls;
	GetSkelControls(SkelControls);

	for (INT I=0; I<SkelControls.Num(); ++I)
	{
		FArchiveCountMem Count( SkelControls(I) );
		nTotalSize += Count.GetNum();
	}

	// Now we get all the MorphNodes in this tree
	TArray<UMorphNodeBase*> MorphNodes;
	GetMorphNodes(MorphNodes);

	for (INT I=0; I<MorphNodes.Num(); ++I)
	{
		FArchiveCountMem Count( MorphNodes(I) );
		nTotalSize += Count.GetNum();
	}

	return nTotalSize;
}

///////////////////////
// UAnimNodeBlendMultiBone
//////////////////////////

/** @see UAnimNode::InitAnim */
void UAnimNodeBlendMultiBone::InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent )
{
	START_INITANIM_TIMER
	{
		EXCLUDE_PARENT_TIME
		Super::InitAnim(meshComp, Parent);
	}

	for( INT Idx = 0; Idx < BlendTargetList.Num(); Idx++ )
	{
		if( BlendTargetList(Idx).InitTargetStartBone != NAME_None )
		{
			SetTargetStartBone( Idx, BlendTargetList(Idx).InitTargetStartBone, BlendTargetList(Idx).InitPerBoneIncrease );
		}
	}
}

void UAnimNodeBlendMultiBone::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	for( INT Idx = 0; Idx < BlendTargetList.Num(); Idx++ )
	{
		UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
		if( PropertyThatChanged && 
			(PropertyThatChanged->GetFName() == FName(TEXT("InitTargetStartBone")) || 
			 PropertyThatChanged->GetFName() == FName(TEXT("InitPerBoneIncrease"))) )
		{
			SetTargetStartBone( Idx, BlendTargetList(Idx).InitTargetStartBone, BlendTargetList(Idx).InitPerBoneIncrease );
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


/**
 * Utility for creating the Child2PerBoneWeight array. Walks down the heirarchy increasing the weight by PerBoneIncrease each step.
 * The per-bone weight is multiplied by the overall blend weight to calculate how much animation data to pull from each child.
 * 
 * @param StartBoneName Start blending in animation from Children(1) (ie second child) from this bone down.
 * @param PerBoneIncrease Amount to increase bone weight by for each bone down heirarchy.
 */
void UAnimNodeBlendMultiBone::SetTargetStartBone( INT TargetIdx, FName StartBoneName, FLOAT PerBoneIncrease )
{
	FChildBoneBlendInfo& Info = BlendTargetList(TargetIdx);

	if( !SkelComponent || 
		(Info.OldStartBone	  == StartBoneName && 
		 Info.OldBoneIncrease == PerBoneIncrease &&
		 Info.TargetRequiredBones.Num() > 0 &&
		 SourceRequiredBones.Num() > 0) )
	{
		return;
	}

	Info.OldBoneIncrease		= PerBoneIncrease;
	Info.InitPerBoneIncrease	= PerBoneIncrease;
	Info.OldStartBone			= StartBoneName;
	Info.InitTargetStartBone	= StartBoneName;

	if( StartBoneName == NAME_None )
	{
		Info.TargetPerBoneWeight.Empty();
	}
	else
	{
		INT StartBoneIndex = SkelComponent->MatchRefBone( StartBoneName );
		if( StartBoneIndex == INDEX_NONE )
		{
			debugf( TEXT("UAnimNodeBlendPerBone::SetTargetStartBone : StartBoneName (%s) not found."), *StartBoneName.ToString() );
			return;
		}

		TArray<FMeshBone>& RefSkel = SkelComponent->SkeletalMesh->RefSkeleton;
		Info.TargetRequiredBones.Empty();
		Info.TargetPerBoneWeight.Empty();
		Info.TargetPerBoneWeight.AddZeroed( RefSkel.Num() );
		SourceRequiredBones.Empty();

		check(PerBoneIncrease >= 0.0f && PerBoneIncrease <= 1.0f); // rather aggressive...

		// Allocate bone weights by walking heirarchy, increasing bone weight for bones below the start bone.
		Info.TargetPerBoneWeight(StartBoneIndex) = PerBoneIncrease;
		for( INT i = 0; i < Info.TargetPerBoneWeight.Num(); i++ )
		{
			if( i != StartBoneIndex )
			{
				FLOAT ParentWeight = Info.TargetPerBoneWeight( RefSkel(i).ParentIndex );
				FLOAT BoneWeight   = (ParentWeight == 0.0f) ? 0.0f : Min( ParentWeight + PerBoneIncrease, 1.0f );

				Info.TargetPerBoneWeight(i) = BoneWeight;
			}

			if( Info.TargetPerBoneWeight(i) > ZERO_ANIMWEIGHT_THRESH )
			{
				Info.TargetRequiredBones.AddItem(i);
			}
			else if( Info.TargetPerBoneWeight(i) <=(1.f - ZERO_ANIMWEIGHT_THRESH) )
			{
				SourceRequiredBones.AddItem( i );
			}
		}
	}
}

void UAnimNodeBlendMultiBone::execSetTargetStartBone( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(TargetIdx);
	P_GET_NAME(StartBoneName);
	P_GET_FLOAT_OPTX(PerBoneIncrease, 1.0f);
	P_FINISH;
	
	SetTargetStartBone( TargetIdx, StartBoneName, PerBoneIncrease );
}


/** @see UAnimNode::GetBoneAtoms. */
void UAnimNodeBlendMultiBone::GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	START_GETBONEATOM_TIMER

	if( GetCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num()) )
	{
		return;
	}

	// Handle case of a blend with no children.
	if( Children.Num() == 0 )
	{
		RootMotionDelta = FBoneAtom::Identity;
		bHasRootMotion	= 0;
		FillWithRefPose(Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
		return;
	}

	INT NumAtoms = SkelComponent->SkeletalMesh->RefSkeleton.Num();
	check( NumAtoms == Atoms.Num() );

	// Find index of the last child with a non-zero weight.
	INT LastChildIndex = INDEX_NONE;
	for(INT i=0; i<Children.Num(); i++)
	{
		if( Children(i).Weight > ZERO_ANIMWEIGHT_THRESH )
		{
			LastChildIndex = i;
		}
	}
	check(LastChildIndex != INDEX_NONE);

	// We don't allocate this array until we need it.
	FBoneAtomArray ChildAtoms;
	if( LastChildIndex == 0 )
	{
		if( Children(0).Anim )
		{
			EXCLUDE_CHILD_TIME
			Children(0).Anim->GetBoneAtoms(Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
		}
		else
		{
			RootMotionDelta.SetIdentity();
			bHasRootMotion	= 0;
			FillWithRefPose(Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
		}

		// We're acting as a pass-through, no need to cache results.
		return;
	}

	TArray<INT>	ChildrenHasRootMotion;
	ChildrenHasRootMotion.Empty(Children.Num());
	ChildrenHasRootMotion.AddZeroed(Children.Num());
	FBoneAtomArray ChildrenRootMotion;
	ChildrenRootMotion.Empty(Children.Num());
	ChildrenRootMotion.AddZeroed(Children.Num());
	for( INT j = 0; j < DesiredBones.Num(); j++ )
	{
		ScalarRegister VAccWeight(ScalarZero);

		UBOOL bNoChildrenYet = TRUE;
		
		// Iterate over each child getting its atoms, scaling them and adding them to output (Atoms array)
		for( INT i = LastChildIndex; i >= 0; i-- )
		{
			if( Children(i).Weight > ZERO_ANIMWEIGHT_THRESH )
			{
				INT BoneIndex = DesiredBones(j);
				ScalarRegister VBoneWeight;
				ScalarRegister VChildWeight(Children(i).Weight);
				if( i > 0 )
				{
					VBoneWeight	= VChildWeight * ScalarRegister(BlendTargetList(i).TargetPerBoneWeight(BoneIndex));
				}
				else
				{
					VBoneWeight = ScalarOne - VAccWeight;
				}

				// Do need to request atoms, so allocate array here.
				if( ChildAtoms.Num() == 0 )
				{
					ChildAtoms.Add(NumAtoms);
				}

				// Get bone atoms from child node (if no child - use ref pose).
				if( Children(i).Anim )
				{
					EXCLUDE_CHILD_TIME
					Children(i).Anim->GetBoneAtoms(ChildAtoms, DesiredBones, ChildrenRootMotion(i), ChildrenHasRootMotion(i), CurveKeys);

					bHasRootMotion = bHasRootMotion || ChildrenHasRootMotion(i);

					if( bNoChildrenYet )
					{
						RootMotionDelta = ChildrenRootMotion(i) * VChildWeight;
					}
					else
					{
						RootMotionDelta += ChildrenRootMotion(i) * VChildWeight;
					}
				}
				else
				{
					FillWithRefPose(ChildAtoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
				}


				// We just write the first childrens atoms into the output array. Avoids zero-ing it out.
				if( bNoChildrenYet )
				{
					Atoms(BoneIndex) = ChildAtoms(BoneIndex) * VBoneWeight;
				}
				else
				{
					Atoms(BoneIndex).AccumulateWithShortestRotation(ChildAtoms(BoneIndex), VBoneWeight);
				}

				// If last child - normalize the rotation quaternion now.
				if( i == 0 )
				{
					Atoms(BoneIndex).NormalizeRotation();
				}

				if( i > 0 )
				{
					VAccWeight = VAccWeight + VBoneWeight;
				}
				
				bNoChildrenYet = FALSE;
			}
		}
	}

	SaveCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num());
}


/************************************************************************************
 * UAnimNodeAimOffset
 ***********************************************************************************/

/** Used to save pointer to AimOffset node in package, to avoid duplicating profile data. */
void UAnimNodeAimOffset::PostAnimNodeInstance(UAnimNode* SourceNode, TMap<UAnimNode*,UAnimNode*>& SrcToDestNodeMap)
{
	START_POSTANIMNODEINSTANCE_TIMER
	{
		EXCLUDE_POSTANIMNODEINSTANCE_PARENT_TIME
		Super::PostAnimNodeInstance(SourceNode, SrcToDestNodeMap);
	}

	// We are going to use data from the TemplateNode, rather than keeping a copy for each instance of the node.
	// Do this only when we instance that tree, not when we duplicate nodes within the same AnimTree!
	if( SourceNode->GetOuter() != GetOuter() )
	{
		TemplateNode = CastChecked<UAnimNodeAimOffset>(SourceNode);
		Profiles.Empty();
	}
}

void UAnimNodeAimOffset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged != NULL )
	{
		if( PropertyThatChanged->GetFName() == FName(TEXT("bBakeFromAnimations")) )
		{
			bBakeFromAnimations = FALSE;
			BakeOffsetsFromAnimations();
		}
	}

	SynchronizeNodesInEditor();
}

/** Deferred Initialization, called only when the node is relevant in the tree. */
void UAnimNodeAimOffset::DeferredInitAnim()
{
	Super::DeferredInitAnim();

	// Update list of required bones
	// this is the list of bones needed for transformation and their parents.
	UpdateListOfRequiredBones();
}

void UAnimNodeAimOffset::UpdateListOfRequiredBones()
{
	// Empty required bones list
	RequiredBones.Reset();
	AimCpntIndexLUT.Reset();

	FAimOffsetProfile* P = GetCurrentProfile();
	if( !P || !SkelComponent || !SkelComponent->SkeletalMesh )
	{
		return;
	}

	const TArray<FMeshBone>&	RefSkel		= SkelComponent->SkeletalMesh->RefSkeleton;
	const INT					NumBones	= RefSkel.Num();

	// Make sure bones don't exceed max byte value.
	check( NumBones <= 255 );
	check( P->AimComponents.Num() <= 255 );

	// Size look up table
	RequiredBones.Empty(NumBones);
	AimCpntIndexLUT.Add(NumBones);
	appMemset(AimCpntIndexLUT.GetData(), 255, AimCpntIndexLUT.Num() * sizeof(BYTE));

	for(INT i=0; i<P->AimComponents.Num(); i++)
	{
		INT const BoneIndex = SkelComponent->SkeletalMesh->MatchRefBone(P->AimComponents(i).BoneName);
		if( BoneIndex != INDEX_NONE )
		{
			RequiredBones.AddItem(BoneIndex);
			AimCpntIndexLUT(BoneIndex) = i;
		}
	}

	// Sort to ensure strictly increasing order.
	Sort<USE_COMPARE_CONSTREF(BYTE, UnAnimTree)>(&RequiredBones(0), RequiredBones.Num());

	// Make sure parents are present in the array. Since we need to get the relevant bones in component space.
	// And that require the parent bones...
	UAnimNode::EnsureParentsPresent(RequiredBones, SkelComponent->SkeletalMesh);
	RequiredBones.Shrink();
	AimCpntIndexLUT.Shrink();
}

FAimOffsetProfile* UAnimNodeAimOffset::GetCurrentProfile()
{
	// Check profile index is not outside range.
	FAimOffsetProfile* P = NULL;
	if(TemplateNode)
	{
		if(CurrentProfileIndex < TemplateNode->Profiles.Num())
		{
			P = &TemplateNode->Profiles(CurrentProfileIndex);
		}
	}
	else
	{
		if(CurrentProfileIndex < Profiles.Num())
		{
			P = &Profiles(CurrentProfileIndex);
		}
	}
	return P;
}

#if DEBUG_QUATERNION
UBOOL DebugCompare(const FBoneAtom & BoneAtom, const FMatrix & InMatrix)
{
	FMatrix MBone = BoneAtom.ToMatrix();

	if (MBone.Equals(InMatrix, 0.1f)==FALSE)
	{
		// problem, don't match up
 		debugf(TEXT("MBone %s"), *MBone.ToString());
		debugf(TEXT("InMatrix %s"), *InMatrix.ToString());

		return FALSE;
	}

	return TRUE;
}
#endif
void UAnimNodeAimOffset::GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	START_GETBONEATOM_TIMER

	if( GetCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num()) )
	{
		return;
	}

	// Get local space atoms from child
	if( Children(0).Anim )
	{
		EXCLUDE_CHILD_TIME
		Children(0).Anim->GetBoneAtoms(Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
	}
	else
	{
		RootMotionDelta.SetIdentity();
		bHasRootMotion	= 0;
		FillWithRefPose(Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
	}

	// Have the option of doing nothing if at a low LOD.
	if( !SkelComponent || RequiredBones.Num() == 0 || SkelComponent->PredictedLODLevel >= PassThroughAtOrAboveLOD || (bPassThroughWhenNotRendered && !SkelComponent->bRecentlyRendered && !GIsEditor) )
	{
		return;
	}

	const USkeletalMesh*	SkelMesh = SkelComponent->SkeletalMesh;
	const INT				NumBones = SkelMesh->RefSkeleton.Num();

	// Make sure we have a valid setup
	FAimOffsetProfile* P = GetCurrentProfile();
	if( !P )
	{
		return;
	}

	FVector2D SafeAim = GetAim();
	
	// Add in rotation offset, but not in the editor
	if( !GIsEditor || GIsGame )
	{
		if( AngleOffset.X != 0.f )
		{
			SafeAim.X = UnWindNormalizedAimAngle(SafeAim.X - AngleOffset.X);
		}
		if( AngleOffset.Y != 0.f )
		{
			SafeAim.Y = UnWindNormalizedAimAngle(SafeAim.Y - AngleOffset.Y);
		}
	}

	// Scale by range
	if( SafeAim.X < 0.f )
	{
		if( P->HorizontalRange.X != 0.f )
		{
			SafeAim.X = SafeAim.X / Abs(P->HorizontalRange.X);
		}
		else
		{
			SafeAim.X = 0.f;
		}
	}
	else
	{
		if( P->HorizontalRange.Y != 0.f )
		{
			SafeAim.X = SafeAim.X / P->HorizontalRange.Y;
		}
		else
		{
			SafeAim.X = 0.f;
		}
	}

	if( SafeAim.Y < 0.f )
	{
		if( P->VerticalRange.X != 0.f )
		{
			SafeAim.Y = SafeAim.Y / Abs(P->VerticalRange.X);
		}
		else
		{
			SafeAim.Y = 0.f;
		}
	}
	else
	{
		if( P->VerticalRange.Y != 0.f )
		{
			SafeAim.Y = SafeAim.Y / P->VerticalRange.Y;
		}
		else
		{
			SafeAim.Y = 0.f;
		}
	}

	// Make sure we're using correct values within legal range.
	SafeAim.X = Clamp<FLOAT>(SafeAim.X, -1.f, +1.f);
	SafeAim.Y = Clamp<FLOAT>(SafeAim.Y, -1.f, +1.f);

	// Post process final Aim.
	PostAimProcessing(SafeAim);

	// Figure out which AimID we're going to use.
	BYTE AimID = EAID_CellCC;
	if( bForceAimDir )
	{
		// If bForceAimDir - just use whatever ForcedAimDir is set to - ignore Aim.
		switch( ForcedAimDir )
		{
			case ANIMAIM_LEFTUP			:	AimID = EAID_CellLU;
											break;
			case ANIMAIM_CENTERUP		:	AimID = EAID_CellCU;
											break;
			case ANIMAIM_RIGHTUP		:	AimID = EAID_CellRU;
											break;
			case ANIMAIM_LEFTCENTER		:	AimID = EAID_CellLC;
											break;
			case ANIMAIM_CENTERCENTER	:	AimID = EAID_CellCC;
											break;
			case ANIMAIM_RIGHTCENTER	:	AimID = EAID_CellRC;
											break;
			case ANIMAIM_LEFTDOWN		:	AimID = EAID_CellLD;
											break;
			case ANIMAIM_CENTERDOWN		:	AimID = EAID_CellCD;
											break;
			case ANIMAIM_RIGHTDOWN		:	AimID = EAID_CellRD;
											break;
		}
	}
	// If horizontal axis is zero, then do 1 lerp instead of 3.
	else if( Abs(SafeAim.X) < KINDA_SMALL_NUMBER )
	{
		if( SafeAim.Y > 0.f ) // Up
		{
			AimID = EAID_ZeroUp;
		}
		else // Down
		{
			AimID = EAID_ZeroDown;
			SafeAim.Y += 1.f;
		}
	}
	// If vertical axis is zero, then do 1 lerp instead of 3.
	else if( Abs(SafeAim.Y) < KINDA_SMALL_NUMBER )
	{
		if( SafeAim.X > 0.f ) // Right
		{
			AimID = EAID_ZeroRight;
		}
		else // Left
		{
			AimID = EAID_ZeroLeft;
			SafeAim.X += 1.f;
		}
	}
	else if( SafeAim.X > 0.f && SafeAim.Y > 0.f ) // Up Right
	{
		AimID = EAID_RightUp;
	}
	else if( SafeAim.X > 0.f && SafeAim.Y < 0.f ) // Bottom Right
	{
		AimID = EAID_RightDown;
		SafeAim.Y += 1.f;
	}
	else if( SafeAim.X < 0.f && SafeAim.Y > 0.f ) // Up Left
	{
		AimID = EAID_LeftUp;
		SafeAim.X += 1.f;
		
	}
	else if( SafeAim.X < 0.f && SafeAim.Y < 0.f ) // Bottom Left
	{
		AimID = EAID_LeftDown;
		SafeAim.X += 1.f;
		SafeAim.Y += 1.f;
	}

#if ENABLE_VECTORIZED_FBONEATOM
	const VectorRegister LV_SafeAim_X = VectorLoadFloat1(&(SafeAim.X));
	const VectorRegister LV_SafeAim_Y = VectorLoadFloat1(&(SafeAim.Y));
#else
	const float LV_SafeAim_X = SafeAim.X;
	const float LV_SafeAim_Y = SafeAim.Y;
#endif

	// We build the mesh-space matrices of the source bones.
	FBoneAtomArray AimOffsetBoneTM;
	AimOffsetBoneTM.Add(NumBones);

	FBoneAtom* AtomsData = Atoms.GetTypedData();
	FBoneAtom* AimOffsetBoneTMData = AimOffsetBoneTM.GetTypedData();

	const INT NumAimComp = P->AimComponents.Num();
	const INT RequiredNum = RequiredBones.Num();
	const INT DesiredNum = DesiredBones.Num();
	INT DesiredPos = 0;
	INT RequiredPos = 0;
	INT BoneIndex = 0;
	while( DesiredPos < DesiredNum && RequiredPos < RequiredNum )
	{
		INT const DesiredBoneIndex = DesiredBones(DesiredPos);
		INT const RequiredBoneIndex = RequiredBones(RequiredPos);

		// Perform intersection of RequiredBones and DesiredBones array.
		// If they are the same, BoneIndex is relevant and we should process it.
		if( DesiredBoneIndex == RequiredBoneIndex )
		{
			BoneIndex = DesiredBoneIndex;
			DesiredPos++;
			RequiredPos++;
		}
		// If value at DesiredPos is lower, increment DesiredPos.
		else if( DesiredBoneIndex < RequiredBoneIndex )
		{
			DesiredPos++;
			continue;
		}
		// If value at RequiredPos is lower, increment RequiredPos.
		else
		{
			RequiredPos++;
			continue;
		}

		INT const ParentIndex = SkelMesh->RefSkeleton(BoneIndex).ParentIndex;
		// Transform required bones into component space
		if( BoneIndex > 0 )
		{
			AimOffsetBoneTM(BoneIndex) = Atoms(BoneIndex) * AimOffsetBoneTM(ParentIndex);
		}
		else
		{
			AimOffsetBoneTM(0) = Atoms(0);
		}

		BYTE& AimCompIndex = AimCpntIndexLUT(BoneIndex);
		if( AimCompIndex != 255 && AimCompIndex < NumAimComp )
		{
#if ENABLE_VECTORIZED_FBONEATOM
			VectorRegister QuaternionOffset;
			VectorRegister TranslationOffset;

#define LV_Load3(x) VectorLoadFloat3_W0(&(x))
#define LV_Load4(x) VectorLoadAligned(&(x))

#define LV_Lerp(a, b, alpha)                    Lerp(LV_Load3(a), LV_Load3(b), alpha)
#define LV_BiLerp(p00, p10, p01, p11, x, y)     BiLerp(LV_Load3(p00), LV_Load3(p10), LV_Load3(p01), LV_Load3(p11), x, y)
#define LV_LerpQuat(a, b, alpha)                VectorLerpQuat(LV_Load4(a), LV_Load4(b), alpha)
#define LV_BiLerpQuat(p00, p10, p01, p11, x, y) VectorBiLerpQuat(LV_Load4(p00), LV_Load4(p10), LV_Load4(p01), LV_Load4(p11), x, y)
#define LV_NormalizeQuat(a)                     a = VectorNormalizeQuaternion(a)

#else
			FQuat QuaternionOffset;
			FVector	TranslationOffset;

#define LV_Lerp(a, b, alpha)                    Lerp(a, b, alpha)
#define LV_BiLerp(p00, p10, p01, p11, x, y)     BiLerp(p00, p10, p01, p11, x, y)
#define LV_LerpQuat(a, b, alpha)                LerpQuat(a, b, alpha)
#define LV_BiLerpQuat(p00, p10, p01, p11, x, y) BiLerpQuat(p00, p10, p01, p11, x, y)
#define LV_NormalizeQuat(a)                     a.Normalize()

#define LV_Load3(x) x
#define LV_Load4(x) x

#endif
			const FAimComponent& AimCpnt = P->AimComponents(AimCompIndex);

			switch( AimID )
			{
				case EAID_ZeroUp	: 	TranslationOffset	= LV_Lerp(AimCpnt.CC.Translation, AimCpnt.CU.Translation, LV_SafeAim_Y);
										QuaternionOffset	= LV_LerpQuat(AimCpnt.CC.Quaternion, AimCpnt.CU.Quaternion, LV_SafeAim_Y);
										LV_NormalizeQuat(QuaternionOffset);
										break;
				case EAID_ZeroDown	: 	TranslationOffset	= LV_Lerp(AimCpnt.CD.Translation, AimCpnt.CC.Translation, LV_SafeAim_Y);
										QuaternionOffset	= LV_LerpQuat(AimCpnt.CD.Quaternion, AimCpnt.CC.Quaternion, LV_SafeAim_Y);
										LV_NormalizeQuat(QuaternionOffset);
										break;
				case EAID_ZeroRight	: 	TranslationOffset	= LV_Lerp(AimCpnt.CC.Translation, AimCpnt.RC.Translation, LV_SafeAim_X);
										QuaternionOffset	= LV_LerpQuat(AimCpnt.CC.Quaternion, AimCpnt.RC.Quaternion, LV_SafeAim_X);
										LV_NormalizeQuat(QuaternionOffset);
										break;
				case EAID_ZeroLeft	: 	TranslationOffset	= LV_Lerp(AimCpnt.LC.Translation, AimCpnt.CC.Translation, LV_SafeAim_X);
										QuaternionOffset	= LV_LerpQuat(AimCpnt.LC.Quaternion, AimCpnt.CC.Quaternion, LV_SafeAim_X);
										LV_NormalizeQuat(QuaternionOffset);
										break;
				case EAID_RightUp	: 	TranslationOffset	= LV_BiLerp(AimCpnt.CC.Translation, AimCpnt.RC.Translation, AimCpnt.CU.Translation, AimCpnt.RU.Translation, LV_SafeAim_X, LV_SafeAim_Y);
										QuaternionOffset	= LV_BiLerpQuat(AimCpnt.CC.Quaternion, AimCpnt.RC.Quaternion, AimCpnt.CU.Quaternion, AimCpnt.RU.Quaternion, LV_SafeAim_X, LV_SafeAim_Y);
										LV_NormalizeQuat(QuaternionOffset);
										break;
				case EAID_RightDown	: 	TranslationOffset	= LV_BiLerp(AimCpnt.CD.Translation, AimCpnt.RD.Translation, AimCpnt.CC.Translation, AimCpnt.RC.Translation, LV_SafeAim_X, LV_SafeAim_Y);
										QuaternionOffset	= LV_BiLerpQuat(AimCpnt.CD.Quaternion, AimCpnt.RD.Quaternion, AimCpnt.CC.Quaternion, AimCpnt.RC.Quaternion, LV_SafeAim_X, LV_SafeAim_Y);
										LV_NormalizeQuat(QuaternionOffset);
										break;
				case EAID_LeftUp	: 	TranslationOffset	= LV_BiLerp(AimCpnt.LC.Translation, AimCpnt.CC.Translation, AimCpnt.LU.Translation, AimCpnt.CU.Translation, LV_SafeAim_X, LV_SafeAim_Y);
										QuaternionOffset	= LV_BiLerpQuat(AimCpnt.LC.Quaternion, AimCpnt.CC.Quaternion, AimCpnt.LU.Quaternion, AimCpnt.CU.Quaternion, LV_SafeAim_X, LV_SafeAim_Y);
										LV_NormalizeQuat(QuaternionOffset);
										break;
				case EAID_LeftDown	: 	TranslationOffset	= LV_BiLerp(AimCpnt.LD.Translation, AimCpnt.CD.Translation, AimCpnt.LC.Translation, AimCpnt.CC.Translation, LV_SafeAim_X, LV_SafeAim_Y);
										QuaternionOffset	= LV_BiLerpQuat(AimCpnt.LD.Quaternion, AimCpnt.CD.Quaternion, AimCpnt.LC.Quaternion, AimCpnt.CC.Quaternion, LV_SafeAim_X, LV_SafeAim_Y);
										LV_NormalizeQuat(QuaternionOffset);
										break;
				case EAID_CellLU	:	TranslationOffset	= LV_Load3(AimCpnt.LU.Translation);
										QuaternionOffset	= LV_Load4(AimCpnt.LU.Quaternion); 
										break;
				case EAID_CellCU	:	TranslationOffset	= LV_Load3(AimCpnt.CU.Translation);
										QuaternionOffset	= LV_Load4(AimCpnt.CU.Quaternion);
										break;
				case EAID_CellRU	:	TranslationOffset	= LV_Load3(AimCpnt.RU.Translation);
										QuaternionOffset	= LV_Load4(AimCpnt.RU.Quaternion);
										break;
				case EAID_CellLC	:	TranslationOffset	= LV_Load3(AimCpnt.LC.Translation);
										QuaternionOffset	= LV_Load4(AimCpnt.LC.Quaternion);
										break;
				case EAID_CellCC	:	TranslationOffset	= LV_Load3(AimCpnt.CC.Translation);
										QuaternionOffset	= LV_Load4(AimCpnt.CC.Quaternion);
										break;
				case EAID_CellRC	:	TranslationOffset	= LV_Load3(AimCpnt.RC.Translation);
										QuaternionOffset	= LV_Load4(AimCpnt.RC.Quaternion);
										break;
				case EAID_CellLD	:	TranslationOffset	= LV_Load3(AimCpnt.LD.Translation);
										QuaternionOffset	= LV_Load4(AimCpnt.LD.Quaternion);
										break;
				case EAID_CellCD	:	TranslationOffset	= LV_Load3(AimCpnt.CD.Translation);
										QuaternionOffset	= LV_Load4(AimCpnt.CD.Quaternion);
										break;
				case EAID_CellRD	:	TranslationOffset	= LV_Load3(AimCpnt.RD.Translation);
										QuaternionOffset	= LV_Load4(AimCpnt.RD.Quaternion);
										break;
				default:
#if ENABLE_VECTORIZED_FBONEATOM
					TranslationOffset = VectorZero();
					QuaternionOffset = GlobalVectorConstants::Float0001;
#else
					TranslationOffset = FVector::ZeroVector;
					QuaternionOffset = FQuat::Identity;
#endif
					break;
			}

#undef LV_Lerp
#undef LV_BiLerp
#undef LV_LerpQuat
#undef LV_BiLerpQuat
#undef LV_NormalizeQuat

#undef LV_Load3
#undef LV_Load4

			// Simpler path if using FBoneAtoms for our transforms.
#if ENABLE_VECTORIZED_FBONEATOM
			FBoneAtom& NewBoneTransform = AimOffsetBoneTM(BoneIndex);
			NewBoneTransform.SetRotation(VectorQuaternionMultiply2(QuaternionOffset, NewBoneTransform.GetRotationV()));
			NewBoneTransform.AddToTranslation(TranslationOffset);
#else
			FBoneAtom NewBoneTransform(AimOffsetBoneTM(BoneIndex));
			NewBoneTransform.SetRotation(QuaternionOffset * NewBoneTransform.GetRotation());
			NewBoneTransform.AddToTranslation(TranslationOffset);
			AimOffsetBoneTM(BoneIndex) = NewBoneTransform;
#endif

			// Transform back to parent bone space
			if( BoneIndex > 0 )
			{
				FBoneAtom const InverseParentTransform = AimOffsetBoneTM(ParentIndex).Inverse();
				Atoms(BoneIndex) = NewBoneTransform * InverseParentTransform;
			}
			else
			{
				Atoms(BoneIndex) = NewBoneTransform;
			}
		}
	}

	SaveCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num());
}


FLOAT UAnimNodeAimOffset::GetSliderPosition(INT SliderIndex, INT ValueIndex)
{
	check(SliderIndex == 0);
	check(ValueIndex == 0 || ValueIndex == 1);

	if( ValueIndex == 0 )
	{
		return (0.5f * Aim.X) + 0.5f;
	}
	else
	{
		return ((-0.5f * Aim.Y) + 0.5f);
	}
}

void UAnimNodeAimOffset::HandleSliderMove(INT SliderIndex, INT ValueIndex, FLOAT NewSliderValue)
{
	check(SliderIndex == 0);
	check(ValueIndex == 0 || ValueIndex == 1);

	if( ValueIndex == 0 )
	{
		Aim.X = (NewSliderValue - 0.5f) * 2.f;
	}
	else
	{
		Aim.Y = -1.f * (NewSliderValue - 0.5f) * 2.f;
	}

	SynchronizeNodesInEditor();
}

void UAnimNodeAimOffset::SynchronizeNodesInEditor()
{
	if( bSynchronizeNodesInEditor && SkelComponent )
	{
		FVector2D AdjustedAim = Aim;
		if( AngleOffset.X != 0.f )
		{
			AdjustedAim.X = UnWindNormalizedAimAngle(AdjustedAim.X + AngleOffset.X);
		}
		if( AngleOffset.Y != 0.f )
		{
			AdjustedAim.Y = UnWindNormalizedAimAngle(AdjustedAim.Y + AngleOffset.Y);
		}

		TArray<UAnimNode*> NodesList;
		if ( SkelComponent && SkelComponent->Animations )
		{
			SkelComponent->Animations->GetNodes(NodesList);

			for(INT i=0; i<NodesList.Num(); i++)
			{
				UAnimNodeAimOffset* AimOffsetNode = Cast<UAnimNodeAimOffset>(NodesList(i));
				if( AimOffsetNode && AimOffsetNode != this && AimOffsetNode->bSynchronizeNodesInEditor )
				{
					// Sync up Aim value.
					AimOffsetNode->Aim = AdjustedAim;

					if( AimOffsetNode->AngleOffset.X != 0.f )
					{
						AimOffsetNode->Aim.X = UnWindNormalizedAimAngle(AimOffsetNode->Aim.X + AimOffsetNode->AngleOffset.X);
					}
					if( AimOffsetNode->AngleOffset.Y != 0.f )
					{
						AimOffsetNode->Aim.Y = UnWindNormalizedAimAngle(AimOffsetNode->Aim.Y + AimOffsetNode->AngleOffset.Y);
					}
				}
			}
		}
	}
}

FString UAnimNodeAimOffset::GetSliderDrawValue(INT SliderIndex)
{
	check(SliderIndex == 0);
	return FString::Printf( TEXT("%0.2f,%0.2f"), Aim.X, Aim.Y );
}

/** Util for finding the quaternion that corresponds to a particular direction. */
static FQuat* GetAimQuatPtr(FAimOffsetProfile* P, INT CompIndex, EAnimAimDir InAimDir)
{
	if( CompIndex < 0 || CompIndex >= P->AimComponents.Num() )
	{
		return NULL;
	}

	FQuat			*QuatPtr	= NULL;
	FAimComponent	&AimCpnt	= P->AimComponents(CompIndex);

	switch( InAimDir )
	{
		case ANIMAIM_LEFTUP			: QuatPtr = &(AimCpnt.LU.Quaternion); break;
		case ANIMAIM_CENTERUP		: QuatPtr = &(AimCpnt.CU.Quaternion); break;
		case ANIMAIM_RIGHTUP		: QuatPtr = &(AimCpnt.RU.Quaternion); break;

		case ANIMAIM_LEFTCENTER		: QuatPtr = &(AimCpnt.LC.Quaternion); break;
		case ANIMAIM_CENTERCENTER	: QuatPtr = &(AimCpnt.CC.Quaternion); break;
		case ANIMAIM_RIGHTCENTER	: QuatPtr = &(AimCpnt.RC.Quaternion); break;

		case ANIMAIM_LEFTDOWN		: QuatPtr = &(AimCpnt.LD.Quaternion); break;
		case ANIMAIM_CENTERDOWN		: QuatPtr = &(AimCpnt.CD.Quaternion); break;
		case ANIMAIM_RIGHTDOWN		: QuatPtr = &(AimCpnt.RD.Quaternion); break;
	}

	return QuatPtr;
}


/** Util for finding the translation that corresponds to a particular direction. */
static FVector* GetAimTransPtr(FAimOffsetProfile* P, INT CompIndex, EAnimAimDir InAimDir)
{
	if( CompIndex < 0 || CompIndex >= P->AimComponents.Num() )
	{
		return NULL;
	}

	FVector			*TransPtr	= NULL;
	FAimComponent	&AimCpnt	= P->AimComponents(CompIndex);

	switch( InAimDir )
	{
		case ANIMAIM_LEFTUP			: TransPtr = &(AimCpnt.LU.Translation); break;
		case ANIMAIM_CENTERUP		: TransPtr = &(AimCpnt.CU.Translation); break;
		case ANIMAIM_RIGHTUP		: TransPtr = &(AimCpnt.RU.Translation); break;

		case ANIMAIM_LEFTCENTER		: TransPtr = &(AimCpnt.LC.Translation); break;
		case ANIMAIM_CENTERCENTER	: TransPtr = &(AimCpnt.CC.Translation); break;
		case ANIMAIM_RIGHTCENTER	: TransPtr = &(AimCpnt.RC.Translation); break;

		case ANIMAIM_LEFTDOWN		: TransPtr = &(AimCpnt.LD.Translation); break;
		case ANIMAIM_CENTERDOWN		: TransPtr = &(AimCpnt.CD.Translation); break;
		case ANIMAIM_RIGHTDOWN		: TransPtr = &(AimCpnt.RD.Translation); break;
	}

	return TransPtr;
}


/** Util for grabbing the quaternion on a specific bone in a specific direction. */
FQuat UAnimNodeAimOffset::GetBoneAimQuaternion(INT CompIndex, EAnimAimDir InAimDir)
{
	FAimOffsetProfile* P = GetCurrentProfile();
	if(!P)
	{
		return FQuat::Identity;
	}

	// Get the array for this pose.
	FQuat *QuatPtr = GetAimQuatPtr(P, CompIndex, InAimDir);

	// And return the Quaternion (if its in range).
	if( QuatPtr )
	{
		return (*QuatPtr);
	}
	else
	{
		return FQuat::Identity;
	}
}


/** Util for grabbing the translation on a specific bone in a specific direction. */
FVector UAnimNodeAimOffset::GetBoneAimTranslation(INT CompIndex, EAnimAimDir InAimDir)
{
	FAimOffsetProfile* P = GetCurrentProfile();
	if(!P)
	{
		return FVector(0,0,0);
	}

	// Get the translation for this pose.
	FVector *TransPtr = GetAimTransPtr(P, CompIndex, InAimDir);

	// And return the Rotator (if its in range).
	if( TransPtr )
	{
		return (*TransPtr);
	}
	else
	{
		return FVector(0,0,0);
	}
}

/** Util for setting the quaternion on a specific bone in a specific direction. */
void UAnimNodeAimOffset::SetBoneAimQuaternion(INT CompIndex, EAnimAimDir InAimDir, const FQuat & InQuat)
{
	FAimOffsetProfile* P = GetCurrentProfile();
	if(!P)
	{
		return;
	}

	// Get the array for this pose.
	FQuat *QuatPtr = GetAimQuatPtr(P, CompIndex, InAimDir);

	// Set the Rotator (if BoneIndex is in range).
	if( QuatPtr )
	{
		(*QuatPtr) = InQuat;
	}
}


/** Util for setting the translation on a specific bone in a specific direction. */
void UAnimNodeAimOffset::SetBoneAimTranslation(INT CompIndex, EAnimAimDir InAimDir, FVector InTrans)
{
	FAimOffsetProfile* P = GetCurrentProfile();
	if(!P)
	{
		return;
	}

	// Get the array for this pose.
	FVector *TransPtr = GetAimTransPtr(P, CompIndex, InAimDir);

	// Set the Rotator (if BoneIndex is in range).
	if( TransPtr )
	{
		(*TransPtr) = InTrans;
	}
}

/** Returns TRUE if AimComponents contains specified bone */
UBOOL UAnimNodeAimOffset::ContainsBone(const FName &BoneName)
{
	FAimOffsetProfile* P = GetCurrentProfile();
	if(!P)
	{
		return FALSE;
	}

	for( INT i=0; i<P->AimComponents.Num(); i++ )
	{
		if( P->AimComponents(i).BoneName == BoneName )
		{
			return TRUE;
		}
	}
	return FALSE;
}


/** Bake in Offsets from supplied Animations. */
void UAnimNodeAimOffset::BakeOffsetsFromAnimations()
{
	if( !SkelComponent || !SkelComponent->SkeletalMesh )
	{
		appMsgf(AMT_OK, TEXT(" No SkeletalMesh to import animations from. Aborting."));
		return;
	}

	// Check profile index is not outside range.
	FAimOffsetProfile* P = GetCurrentProfile();
	if( !P )
	{
		return;
	}

	// Clear current setup
	P->AimComponents.Empty();
	AimCpntIndexLUT.Empty();

	// AnimNodeSequence used to extract animation data.
	UAnimNodeSequence*	SeqNode = ConstructObject<UAnimNodeSequence>(UAnimNodeSequence::StaticClass());
	SeqNode->SkelComponent = SkelComponent;

	// Extract Center/Center (reference pose)
	TArray<FBoneAtom>	BoneAtoms_CC;
	if( ExtractAnimationData(SeqNode, P->AnimName_CC, BoneAtoms_CC) == FALSE )
	{
		appMsgf(AMT_OK, TEXT(" Couldn't get CenterCenter pose, this is necessary. Aborting."));
		return;;
	}

	TArray<FBoneAtom>	BoneAtoms;

	// Extract Left/Up
	if( ExtractAnimationData(SeqNode, P->AnimName_LU, BoneAtoms) == TRUE )
	{
		debugf(TEXT(" Converting Animation: %s"), *P->AnimName_LU.ToString());
		ExtractOffsets(BoneAtoms_CC, BoneAtoms, ANIMAIM_LEFTUP);
	}

	// Extract Left/Center
	if( ExtractAnimationData(SeqNode, P->AnimName_LC, BoneAtoms) == TRUE )
	{
		debugf(TEXT(" Converting Animation: %s"), *P->AnimName_LC.ToString());
		ExtractOffsets(BoneAtoms_CC, BoneAtoms, ANIMAIM_LEFTCENTER);
	}

	// Extract Left/Down
	if( ExtractAnimationData(SeqNode, P->AnimName_LD, BoneAtoms) == TRUE )
	{
		debugf(TEXT(" Converting Animation: %s"), *P->AnimName_LD.ToString());
		ExtractOffsets(BoneAtoms_CC, BoneAtoms, ANIMAIM_LEFTDOWN);
	}

	// Extract Center/Up
	if( ExtractAnimationData(SeqNode, P->AnimName_CU, BoneAtoms) == TRUE )
	{
		debugf(TEXT(" Converting Animation: %s"), *P->AnimName_CU.ToString());
		ExtractOffsets(BoneAtoms_CC, BoneAtoms, ANIMAIM_CENTERUP);
	}

	// Extract Center/Down
	if( ExtractAnimationData(SeqNode, P->AnimName_CD, BoneAtoms) == TRUE )
	{
		debugf(TEXT(" Converting Animation: %s"), *P->AnimName_CD.ToString());
		ExtractOffsets(BoneAtoms_CC, BoneAtoms, ANIMAIM_CENTERDOWN);
	}

	// Extract Right/Up
	if( ExtractAnimationData(SeqNode, P->AnimName_RU, BoneAtoms) == TRUE )
	{
		debugf(TEXT(" Converting Animation: %s"), *P->AnimName_RU.ToString());
		ExtractOffsets(BoneAtoms_CC, BoneAtoms, ANIMAIM_RIGHTUP);
	}

	// Extract Right/Center
	if( ExtractAnimationData(SeqNode, P->AnimName_RC, BoneAtoms) == TRUE )
	{
		debugf(TEXT(" Converting Animation: %s"), *P->AnimName_RC.ToString());
		ExtractOffsets(BoneAtoms_CC, BoneAtoms, ANIMAIM_RIGHTCENTER);
	}

	// Extract Right/Down
	if( ExtractAnimationData(SeqNode, P->AnimName_RD, BoneAtoms) == TRUE )
	{
		debugf(TEXT(" Converting Animation: %s"), *P->AnimName_RD.ToString());
		ExtractOffsets(BoneAtoms_CC, BoneAtoms, ANIMAIM_RIGHTDOWN);
	}

	// Done, cache required bones
	UpdateListOfRequiredBones();

	// Clean up.
	SeqNode->SkelComponent	= NULL;
	SeqNode					= NULL;

	appMsgf(AMT_OK, TEXT(" Export finished, check log for details."));
}


void UAnimNodeAimOffset::ExtractOffsets(TArray<FBoneAtom>& RefBoneAtoms, TArray<FBoneAtom>& BoneAtoms, EAnimAimDir InAimDir)
{
	TArray<FBoneAtom>	TargetTM;
	TargetTM.Add(BoneAtoms.Num());

	for(INT i=0; i<BoneAtoms.Num(); i++)
	{
		// Transform target pose into mesh space
		TargetTM(i) = BoneAtoms(i);
		if( i > 0 )
		{
			TargetTM(i) *= TargetTM(SkelComponent->SkeletalMesh->RefSkeleton(i).ParentIndex);
		}

		// Now get Source transform on this bone
		// But from Target parent bone in mesh space.
		FBoneAtom SourceTM = RefBoneAtoms(i);
		if( i > 0 )
		{
			// Transform Target Skeleton by reference transform for this bone
			SourceTM *= TargetTM(SkelComponent->SkeletalMesh->RefSkeleton(i).ParentIndex);
		}

		FBoneAtom TargetTranslationM = FBoneAtom::Identity;
		TargetTranslationM.SetOrigin( TargetTM(i).GetOrigin() );

		FBoneAtom SourceTranslationM = FBoneAtom::Identity;
		SourceTranslationM.SetOrigin( SourceTM.GetOrigin() );
		
		const FBoneAtom	TranslationTM		= SourceTranslationM.Inverse() * TargetTranslationM;
		const FVector	TranslationOffset	= TranslationTM.GetOrigin();

		if( !TranslationOffset.IsNearlyZero() )
		{
			const INT CompIdx = GetComponentIdxFromBoneIdx(i, TRUE);

			if( CompIdx != INDEX_NONE )
			{
				SetBoneAimTranslation(CompIdx, InAimDir, TranslationOffset);
			}
		}

		FBoneAtom TargetRotationM = TargetTM(i);
		TargetRotationM.RemoveScaling();
		TargetRotationM.SetOrigin(FVector(0.f));

		FBoneAtom SourceRotationM = SourceTM;
		SourceRotationM.RemoveScaling();
		SourceRotationM.SetOrigin(FVector(0.f));

		// Convert delta rotation to FRotator.
		const FBoneAtom 		RotationTM		= SourceRotationM.Inverse() * TargetRotationM;
		const FQuat				QuaterionOffset = RotationTM.GetRotation(); 
		const FRotator			RotationOffset	= FRotator(QuaterionOffset);

		if( !RotationOffset.IsZero() )
		{
			const INT CompIdx = GetComponentIdxFromBoneIdx(i, TRUE);

			if( CompIdx != INDEX_NONE )
			{
				SetBoneAimQuaternion(CompIdx, InAimDir, QuaterionOffset);
			}
		}

	}
}


INT UAnimNodeAimOffset::GetComponentIdxFromBoneIdx(const INT BoneIndex, UBOOL bCreateIfNotFound)
{
	if( BoneIndex == INDEX_NONE )
	{
		return INDEX_NONE;
	}

	FAimOffsetProfile* P = GetCurrentProfile();
	if( !P )
	{
		return INDEX_NONE;
	}

	// If AimComponent exists, return it's index from the look up table
	INT AimCpntIndex = INDEX_NONE;
	if( AimCpntIndexLUT.Num() > BoneIndex && AimCpntIndexLUT(BoneIndex) != 255 )
	{
		return AimCpntIndexLUT(BoneIndex);
	}

	if( bCreateIfNotFound )
	{
		const FName	BoneName = SkelComponent->SkeletalMesh->RefSkeleton(BoneIndex).Name;

		// If its a valid bone we want to add..
		if( BoneName != NAME_None && BoneIndex != INDEX_NONE )
		{
			INT InsertPos = INDEX_NONE;

			// Iterate through current array, to find place to insert this new Bone so they stay in Bone index order.
			for(INT i=0; i<P->AimComponents.Num() && InsertPos == INDEX_NONE; i++)
			{
				const FName	TestName	= P->AimComponents(i).BoneName;
				const INT	TestIndex	= SkelComponent->SkeletalMesh->MatchRefBone(TestName);

				if( TestIndex != INDEX_NONE && TestIndex > BoneIndex )
				{
					InsertPos = i;
				}
			}

			// If we didn't find insert position - insert at end.
			// This also handles case of and empty BoneNames array.
			if( InsertPos == INDEX_NONE )
			{
				InsertPos = P->AimComponents.Num();
			}

			// Add a new component.
			P->AimComponents.InsertZeroed(InsertPos);

			// Set correct name and index.
			P->AimComponents(InsertPos).BoneName = BoneName;

			// Initialize Quaternions - InsertZeroed doesn't set them to Identity
			SetBoneAimQuaternion(InsertPos, ANIMAIM_LEFTUP,			FQuat::Identity);
			SetBoneAimQuaternion(InsertPos, ANIMAIM_CENTERUP,		FQuat::Identity);
			SetBoneAimQuaternion(InsertPos, ANIMAIM_RIGHTUP,		FQuat::Identity);

			SetBoneAimQuaternion(InsertPos, ANIMAIM_LEFTCENTER,		FQuat::Identity);
			SetBoneAimQuaternion(InsertPos, ANIMAIM_CENTERCENTER,	FQuat::Identity);
			SetBoneAimQuaternion(InsertPos, ANIMAIM_RIGHTCENTER,	FQuat::Identity);

			SetBoneAimQuaternion(InsertPos, ANIMAIM_LEFTDOWN,		FQuat::Identity);
			SetBoneAimQuaternion(InsertPos, ANIMAIM_CENTERDOWN,		FQuat::Identity);
			SetBoneAimQuaternion(InsertPos, ANIMAIM_RIGHTDOWN,		FQuat::Identity);

			// Update RequiredBoneIndex, this will update the LookUp Table
			UpdateListOfRequiredBones();

			return InsertPos;
		}
	}

	return INDEX_NONE;
}


/** 
 * Extract Parent Space Bone Atoms from Animation Data specified by Name. 
 * Returns TRUE if successful.
 */
UBOOL UAnimNodeAimOffset::ExtractAnimationData(UAnimNodeSequence *SeqNode, FName AnimationName, TArray<FBoneAtom>& BoneAtoms)
{
	//Set Animation
	SeqNode->SetAnim(AnimationName);

	if( SeqNode->AnimSeq == NULL )
	{
		debugf(TEXT(" ExtractAnimationData: Animation not found: %s, Skipping..."), *AnimationName.ToString());
		return FALSE;
	}

	const USkeletalMesh*	SkelMesh = SkelComponent->SkeletalMesh;
	const INT				NumBones = SkelMesh->RefSkeleton.Num();

	// initialize Bone Atoms array
	if( BoneAtoms.Num() != NumBones )
	{
		BoneAtoms.Empty();
		BoneAtoms.Add(NumBones);
	}

	// Initialize Desired bones array. We take all.
	TArray<BYTE> DesiredBones;
	DesiredBones.Empty();
	DesiredBones.Add(NumBones);
	for(INT i=0; i<DesiredBones.Num(); i++)
	{
		DesiredBones(i) = i;
	}

	// Extract bone atoms from animation data
	FBoneAtom	RootMotionDelta;
	INT			bHasRootMotion;

	FMemMark Mark(GMainThreadMemStack);
	FBoneAtomArray OutAtoms;
	OutAtoms.Add(NumBones);
	FCurveKeyArray DummyCurveKeys;
	SeqNode->GetBoneAtoms(OutAtoms, DesiredBones, RootMotionDelta, bHasRootMotion, DummyCurveKeys);
	BoneAtoms = OutAtoms;
	Mark.Pop();

	return TRUE;
}

/** 
 *	Change the currently active profile to the one with the supplied name. 
 *	If a profile with that name does not exist, this does nothing.
 */
void UAnimNodeAimOffset::SetActiveProfileByName(FName ProfileName)
{
	if(TemplateNode)
	{
		// Look through profiles to find a name that matches.
		for(INT i=0; i<TemplateNode->Profiles.Num(); i++)
		{
			// Found it - change to it.
			if(TemplateNode->Profiles(i).ProfileName == ProfileName)
			{
				SetActiveProfileByIndex(i);
				return;
			}
		}
	}
	else
	{
		// Look through profiles to find a name that matches.
		for(INT i=0; i<Profiles.Num(); i++)
		{
			// Found it - change to it.
			if(Profiles(i).ProfileName == ProfileName)
			{
				SetActiveProfileByIndex(i);
				return;
			}
		}
	}
}

/** 
 *	Change the currently active profile to the one with the supplied index. 
 *	If ProfileIndex is outside range, this does nothing.
 */
void UAnimNodeAimOffset::SetActiveProfileByIndex(INT ProfileIndex)
{
	// Check new index is in range.
	// Don't update if selecting the current profile again.
	if( TemplateNode )
	{
		if( ProfileIndex == CurrentProfileIndex || ProfileIndex < 0 || ProfileIndex >= TemplateNode->Profiles.Num() )
		{
			return;
		}
	}
	else
	{
		if( ProfileIndex == CurrentProfileIndex || ProfileIndex < 0 || ProfileIndex >= Profiles.Num() )
		{
			return;
		}
	}

	// Update profile index
	CurrentProfileIndex = ProfileIndex;

	// We need to recalculate the bone indices modified by the new profile.
	UpdateListOfRequiredBones();
}

/************************************************************************************
 * UAnimNodeSynch
 ***********************************************************************************/

/** Add a node to an existing group */
void UAnimNodeSynch::AddNodeToGroup(class UAnimNodeSequence* SeqNode, FName GroupName)
{
	if( !SeqNode || GroupName == NAME_None )
	{
		return;
	}

	for( INT GroupIdx=0; GroupIdx<Groups.Num(); GroupIdx++ )
	{
		FSynchGroup& SynchGroup = Groups(GroupIdx);
		if( SynchGroup.GroupName == GroupName )
		{
			// Set group name
			SeqNode->SynchGroupName = GroupName;
			SynchGroup.SeqNodes.AddUniqueItem(SeqNode);

			break;
		}
	}
}


/** Remove a node from an existing group */
void UAnimNodeSynch::RemoveNodeFromGroup(class UAnimNodeSequence* SeqNode, FName GroupName)
{
	if( !SeqNode || GroupName == NAME_None )
	{
		return;
	}

	for( INT GroupIdx=0; GroupIdx<Groups.Num(); GroupIdx++ )
	{
		FSynchGroup& SynchGroup = Groups(GroupIdx);
		if( SynchGroup.GroupName == GroupName )
		{
			SeqNode->SynchGroupName = NAME_None;
			SynchGroup.SeqNodes.RemoveItem(SeqNode);

			// If we're removing the Master Node, clear reference to it.
			if( SynchGroup.MasterNode == SeqNode )
			{
				SynchGroup.MasterNode = NULL;
				UpdateMasterNodeForGroup(SynchGroup);
			}

			break;
		}
	}
}


/** Force a group at a relative position. */
void UAnimNodeSynch::ForceRelativePosition(FName GroupName, FLOAT RelativePosition)
{
	for( INT GroupIdx=0; GroupIdx<Groups.Num(); GroupIdx++ )
	{
		FSynchGroup& SynchGroup = Groups(GroupIdx);
		if( SynchGroup.GroupName == GroupName )
		{
			for( INT i=0; i < SynchGroup.SeqNodes.Num(); i++ )
			{
				UAnimNodeSequence* SeqNode = SynchGroup.SeqNodes(i);
				if( SeqNode && SeqNode->AnimSeq )
				{
					SeqNode->SetPosition(SeqNode->FindGroupPosition(RelativePosition), FALSE);
				}
			}
		}
	}
}


/** Get the relative position of a group. */
FLOAT UAnimNodeSynch::GetRelativePosition(FName GroupName)
{
	for( INT GroupIdx=0; GroupIdx<Groups.Num(); GroupIdx++ )
	{
		FSynchGroup& SynchGroup = Groups(GroupIdx);
		if( SynchGroup.GroupName == GroupName )
		{
			if( SynchGroup.MasterNode )
			{
				return SynchGroup.MasterNode->GetGroupRelativePosition();
			}
			else
			{
				debugf(TEXT("UAnimNodeSynch::GetRelativePosition, no master node for group %s"), *SynchGroup.GroupName.ToString());
			}
		}
	}

	return 0.f;
}

/** Accesses the Master Node driving a given group */
UAnimNodeSequence* UAnimNodeSynch::GetMasterNodeOfGroup(FName GroupName)
{
	for(INT GroupIdx=0; GroupIdx<Groups.Num(); GroupIdx++ )
	{
		if( Groups(GroupIdx).GroupName == GroupName )
		{
			return Groups(GroupIdx).MasterNode;
		}
	}

	return NULL;
}

/** Force a group at a relative position. */
void UAnimNodeSynch::SetGroupRateScale(FName GroupName, FLOAT NewRateScale)
{
	for(INT GroupIdx=0; GroupIdx<Groups.Num(); GroupIdx++ )
	{
		if( Groups(GroupIdx).GroupName == GroupName )
		{
			Groups(GroupIdx).RateScale = NewRateScale;
		}
	}
}

void UAnimNodeSynch::InitAnim(USkeletalMeshComponent* MeshComp, UAnimNodeBlendBase* Parent)
{
	START_INITANIM_TIMER
	{
		EXCLUDE_PARENT_TIME
		Super::InitAnim(MeshComp, Parent);
	}

	// Rebuild cached list of nodes belonging to each group
	RepopulateGroups();
}

/** Look up AnimNodeSequences and cache Group lists */
void UAnimNodeSynch::RepopulateGroups()
{
	if( Children(0).Anim )
	{
		// get all UAnimNodeSequence children and cache them to avoid over casting
		TArray<UAnimNodeSequence*> Nodes;
		Children(0).Anim->GetAnimSeqNodes(Nodes);

		for( INT GroupIdx=0; GroupIdx<Groups.Num(); GroupIdx++ )
		{
			FSynchGroup& SynchGroup = Groups(GroupIdx);

			// Clear cached nodes
			SynchGroup.SeqNodes.Empty();

			// Caches nodes belonging to group
			if( SynchGroup.GroupName != NAME_None )
			{
				for( INT i=0; i < Nodes.Num(); i++ )
				{
					UAnimNodeSequence* SeqNode = Nodes(i);
					if( SeqNode->SynchGroupName == SynchGroup.GroupName )	// need to be from the same group name
					{
						SynchGroup.SeqNodes.AddItem(SeqNode);
					}
				}
			}

			// Clear Master Node
			SynchGroup.MasterNode = NULL;

			// Update Master Node
			UpdateMasterNodeForGroup(SynchGroup);
		}
	}
}

/** Updates the Master Node of a given group */
void UAnimNodeSynch::UpdateMasterNodeForGroup(FSynchGroup& SynchGroup)
{
	// start with our previous node. see if we can find better!
	UAnimNodeSequence*	MasterNode		= SynchGroup.MasterNode;	
	// Find the node which has the highest weight... that's our master node
	FLOAT				HighestWeight	= MasterNode ? MasterNode->NodeTotalWeight : 0.f;

		// if the previous master node has a full weight, then don't bother looking for another one.
	if( !SynchGroup.MasterNode || (SynchGroup.MasterNode->NodeTotalWeight < (1.f - ZERO_ANIMWEIGHT_THRESH)) )
	{
		for(INT i=0; i < SynchGroup.SeqNodes.Num(); i++)
		{
			UAnimNodeSequence* SeqNode = SynchGroup.SeqNodes(i);
			if( SeqNode &&									
				!SeqNode->bForceAlwaysSlave && 
				SeqNode->NodeTotalWeight >= HighestWeight )  // Must be the most relevant to the final blend
			{
				MasterNode		= SeqNode;
				HighestWeight	= SeqNode->NodeTotalWeight;
			}
		}

		SynchGroup.MasterNode = MasterNode;
	}
}

/** The main synchronization code... */
void UAnimNodeSynch::TickAnim(FLOAT DeltaSeconds)
{
	// Call Super::TickAnim first to set proper children weights.
	Super::TickAnim(DeltaSeconds);

	// Force continuous update if within the editor (because we can add or remove nodes)
	// This can be improved by only doing that when a node has been added a removed from the tree.
	if( GIsEditor && !GIsGame )
	{
		RepopulateGroups();
	}

	// Update groups
	for(INT GroupIdx=0; GroupIdx<Groups.Num(); GroupIdx++)
	{
		FSynchGroup& SynchGroup = Groups(GroupIdx);

		// Update Master Node
		UpdateMasterNodeForGroup(SynchGroup);

		// Now that we have the master node, have it update all the others
		if( SynchGroup.MasterNode && SynchGroup.MasterNode->AnimSeq )
		{
			UAnimNodeSequence* MasterNode	= SynchGroup.MasterNode;
			const FLOAT	OldPosition			= MasterNode->CurrentTime;
			const FLOAT MasterMoveDelta		= SynchGroup.RateScale * MasterNode->Rate * MasterNode->AnimSeq->RateScale * DeltaSeconds;

			if( MasterNode->bPlaying == TRUE )
			{
				// Keep track of PreviousTime before any update. This is used by Root Motion.
				MasterNode->PreviousTime = MasterNode->CurrentTime;

				// Update Master Node
				// Master Node has already been ticked, so now we advance its CurrentTime position...
				// Time to move forward by - DeltaSeconds scaled by various factors.
				MasterNode->AdvanceBy(MasterMoveDelta, DeltaSeconds, TRUE);
			}

			// Master node was changed during the tick?
			// Skip this round of updates...
			if( SynchGroup.MasterNode != MasterNode )
			{
				continue;
			}

			// Update slave nodes only if master node has changed.
			if( MasterNode->CurrentTime != OldPosition &&
				MasterNode->AnimSeq &&
				MasterNode->AnimSeq->SequenceLength > 0.f )
			{
				// Find it's relative position on its time line.
				const FLOAT MasterRelativePosition = MasterNode->GetGroupRelativePosition();

				// Update slaves to match relative position of master node.
				for(INT i=0; i<SynchGroup.SeqNodes.Num(); i++)
				{
					UAnimNodeSequence *SlaveNode = SynchGroup.SeqNodes(i);

					if( SlaveNode && 
						SlaveNode != MasterNode && 
						SlaveNode->AnimSeq &&
						SlaveNode->AnimSeq->SequenceLength > 0.f )
					{
						// Slave's new time
						const FLOAT NewTime		= SlaveNode->FindGroupPosition(MasterRelativePosition);
						FLOAT SlaveMoveDelta	= appFmod(NewTime - SlaveNode->CurrentTime, SlaveNode->AnimSeq->SequenceLength);

						// Make sure SlaveMoveDelta And MasterMoveDelta are the same sign, so they both move in the same direction.
						if( SlaveMoveDelta * MasterMoveDelta < 0.f )
						{
							if( SlaveMoveDelta >= 0.f )
							{
								SlaveMoveDelta -= SlaveNode->AnimSeq->SequenceLength;
							}
							else
							{
								SlaveMoveDelta += SlaveNode->AnimSeq->SequenceLength;
							}
						}

						// Keep track of PreviousTime before any update. This is used by Root Motion.
						SlaveNode->PreviousTime = SlaveNode->CurrentTime;

						// Move slave node to correct position
						SlaveNode->AdvanceBy(SlaveMoveDelta, DeltaSeconds, SynchGroup.bFireSlaveNotifies);
					}
				}
			}
		}
	}
}


/************************************************************************************
 * UAnimNodeRandom
 ***********************************************************************************/

#define DEBUG_ANIMNODERANDOM 0

void UAnimNodeRandom::InitAnim(USkeletalMeshComponent* MeshComp, UAnimNodeBlendBase* Parent)
{
#if DEBUG_ANIMNODERANDOM
	debugf(TEXT("%3.2f UAnimNodeRandom::InitAnim"), GWorld->GetTimeSeconds());
#endif

	START_INITANIM_TIMER
	{
		EXCLUDE_PARENT_TIME
		Super::InitAnim(MeshComp, Parent);
	}

	// Verify that Info array is in synch with child number.
	if( RandomInfo.Num() != Children.Num() )
	{
		const INT Diff = Children.Num() - RandomInfo.Num();
		if( Diff > 0 )
		{
			RandomInfo.AddZeroed(Diff);
		}
		else
		{
			RandomInfo.Remove(RandomInfo.Num() + Diff, -Diff);
		}
	}

	// Only trigger animation is none is playing currently.
	// We don't want to override an animation that is currently playing.
	UBOOL bTriggerAnim = ActiveChildIndex < 0 || ActiveChildIndex >= Children.Num() || !Children(ActiveChildIndex).Anim;

	if( !bTriggerAnim )
	{
		bTriggerAnim = !PlayingSeqNode || !PlayingSeqNode->IsChildOf(Children(ActiveChildIndex).Anim);

		if( !bTriggerAnim )
		{
			FRandomAnimInfo& Info = RandomInfo(ActiveChildIndex);
			bTriggerAnim = !PlayingSeqNode->bPlaying && !Info.bStillFrame;
		}
	}

	if( bTriggerAnim )
	{
		PlayPendingAnimation(0.f);
	}
}

/** A child has been added, update RandomInfo accordingly */
void UAnimNodeRandom::OnAddChild(INT ChildNum)
{
	Super::OnAddChild(ChildNum);

	// Update RandomInfo to match Children array
	if( ChildNum >= 0 )
	{
		if( ChildNum < RandomInfo.Num() )
		{
			RandomInfo.InsertZeroed(ChildNum, 1);
		}
		else
		{
			RandomInfo.AddZeroed(ChildNum + 1 - RandomInfo.Num());
		}

		// Set up new addition w/ defaults
		FRandomAnimInfo& Info = RandomInfo(ChildNum);
		Info.Chance			= 1.f;
		Info.BlendInTime	= 0.25f;
		Info.PlayRateRange	= FVector2D(1.f,1.f);
	}
}


/** A child has been removed, update RandomInfo accordingly */
void UAnimNodeRandom::OnRemoveChild(INT ChildNum)
{
	Super::OnRemoveChild(ChildNum);

	if( ChildNum < RandomInfo.Num() )
	{
		// Update Mask to match Children array
		RandomInfo.Remove(ChildNum);
	}
}

void UAnimNodeRandom::OnChildAnimEnd(UAnimNodeSequence* Child, FLOAT PlayedTime, FLOAT ExcessTime)
{
#if DEBUG_ANIMNODERANDOM
	debugf(TEXT("%3.2f UAnimNodeRandom::OnChildAnimEnd %s"), GWorld->GetTimeSeconds(), *Child->AnimSeqName.ToString());
#endif

	Super::OnChildAnimEnd(Child, PlayedTime, ExcessTime);

	// Node playing current animation is done playing? For a loop certainly... So transition to pending animation!
	if( Child && Child == PlayingSeqNode && bPickedPendingChildIndex )
	{
		PlayPendingAnimation(0.f, ExcessTime);
	}
}

INT UAnimNodeRandom::PickNextAnimIndex()
{
#if DEBUG_ANIMNODERANDOM
	debugf(TEXT("%3.2f UAnimNodeRandom::PickNextAnimIndex"), GWorld->GetTimeSeconds());
#endif

	// Monitor that we went in there.
	bPickedPendingChildIndex = TRUE;

	if( !Children.Num() )
	{
		return INDEX_NONE;
	}

	// If active child is in the list
	if( PlayingSeqNode && ActiveChildIndex >= 0 && ActiveChildIndex < RandomInfo.Num() )
	{
		FRandomAnimInfo& Info = RandomInfo(ActiveChildIndex);

		// If we need to loop, loop!
		if( Info.LoopCount > 0 )
		{
			Info.LoopCount--;
			return ActiveChildIndex;
		}
	}

	// Build a list of valid indices to choose from
	TArray<INT> IndexList;
	FLOAT TotalWeight = 0.f;
	for(INT Idx=0; Idx<Children.Num(); Idx++)
	{
		if( Idx != ActiveChildIndex && Idx < RandomInfo.Num() && RandomInfo(Idx).Chance > 0.f && Children(Idx).Anim )
		{
			IndexList.AddItem(Idx);
			TotalWeight += RandomInfo(Idx).Chance;
		}
	}

	// If we have valid children to choose from
	// (Fall through will just replay current active child)
	if( IndexList.Num() > 0 && TotalWeight > 0.f )
	{
		TArray<FLOAT> Weights;
		Weights.Add(IndexList.Num());
	
		/** Value used to normalize weights so all childs add up to 1.f */
		for(INT i=0; i<IndexList.Num(); i++)
		{
			Weights(i) = RandomInfo(IndexList(i)).Chance / TotalWeight;
		}	

		FLOAT	RandomWeight	= appFrand();
		INT		Index			= 0;
		INT		DesiredChildIdx	= IndexList(Index);

		// This child has too much weight, so skip to next.
		while( Index < IndexList.Num() - 1 && RandomWeight > Weights(Index) )
		{
			RandomWeight -= Weights(Index);
			Index++;
			DesiredChildIdx	= IndexList(Index);
		}

		FRandomAnimInfo& Info = RandomInfo(DesiredChildIdx);

		// Reset loop counter
		if( Info.LoopCountMax > Info.LoopCountMin )
		{
			Info.LoopCount = Info.LoopCountMin + appRand() % (Info.LoopCountMax - Info.LoopCountMin + 1);
		}
		else
		{
			Info.LoopCount = Max( Info.LoopCountMin, Info.LoopCountMax );
		}

		return DesiredChildIdx;
	}

	// Fall back to using current one again.
	return ActiveChildIndex;
}

void UAnimNodeRandom::PlayPendingAnimation(FLOAT BlendTime, FLOAT StartTime)
{
#if DEBUG_ANIMNODERANDOM
	debugf(TEXT("%3.2f UAnimNodeRandom::PlayPendingAnimation. PendingChildIndex: %d, BlendTime: %f, StartTime: %f"), GWorld->GetTimeSeconds(), PendingChildIndex, BlendTime, StartTime);
#endif

	// if our pending child index is not valid, pick one
	if( !(PendingChildIndex >= 0 && PendingChildIndex < Children.Num() && PendingChildIndex < RandomInfo.Num() && Children(PendingChildIndex).Anim) )
	{
		PendingChildIndex = PickNextAnimIndex();
#if DEBUG_ANIMNODERANDOM
		debugf(TEXT("%3.2f UAnimNodeRandom::PlayPendingAnimation. PendingChildIndex not valid, picked: %d"), GWorld->GetTimeSeconds(), PendingChildIndex);
#endif
		// if our pending child index is STILL not valid, abort
		if( !(PendingChildIndex >= 0 && PendingChildIndex < Children.Num() && PendingChildIndex < RandomInfo.Num() && Children(PendingChildIndex).Anim) )
		{
#if DEBUG_ANIMNODERANDOM
			debugf(TEXT("%3.2f UAnimNodeRandom::PlayPendingAnimation. PendingChildIndex still not valid, abort"), GWorld->GetTimeSeconds());
#endif
			return;
		}
	}

	bPickedPendingChildIndex = FALSE;

	// Set new active child w/ blend. Don't override blend time if same channel, we might still be blending.
	if( ActiveChildIndex != PendingChildIndex )
	{
		SetActiveChild(PendingChildIndex, BlendTime);
	}

	// Play the animation if this child is a sequence
	PlayingSeqNode = Cast<UAnimNodeSequence>(Children(ActiveChildIndex).Anim);
	UBOOL bPickPendingChild = TRUE;
	if( PlayingSeqNode )
	{
		FRandomAnimInfo& Info = RandomInfo(ActiveChildIndex);

		// If Animation is synced, don't mess with its position and rate. (MoveCycles?)
		if( PlayingSeqNode->SynchGroupName != NAME_None && PlayingSeqNode->bLooping )
		{
			PlayingSeqNode->bPlaying = TRUE;
			// Don't pick pending
			bPickPendingChild = FALSE;

			UAnimTree* RootNode = Cast<UAnimTree>(SkelComponent->Animations);
			if( RootNode )
			{
				INT const GroupIndex = RootNode->GetGroupIndex(PlayingSeqNode->SynchGroupName);
				if( GroupIndex != INDEX_NONE )
				{
					FAnimGroup& AnimGroup = RootNode->AnimGroups(GroupIndex);
					// Set current time, so we can start tracking from there on.
					Info.LastPosition = PlayingSeqNode->FindGroupPosition(AnimGroup.SynchPctPosition);
				}
			}
		}
		else if( !Info.bStillFrame )
		{
			FLOAT PlayRate = Lerp(Info.PlayRateRange.X, Info.PlayRateRange.Y, appFrand());
			if( PlayRate < KINDA_SMALL_NUMBER )
			{
				PlayRate = 1.f;
			}
			PlayingSeqNode->PlayAnim(FALSE, PlayRate, 0.f);

			// Advance to proper position if needed, useful for looping animations.
			if( StartTime > 0.f )
			{
				PlayingSeqNode->SetPosition(StartTime * PlayingSeqNode->GetGlobalPlayRate(), TRUE);
			}
		}
		else
		{
			// Still frame, don't play animation.
			if( PlayingSeqNode->bPlaying )
			{
				PlayingSeqNode->StopAnim();
			}

			// For still frames, we need to pick the pending child
			bPickPendingChild = TRUE;
		}
	}

	// Pick PendingChildIndex for next time...
	if( bPickPendingChild )
	{
		PendingChildIndex = PickNextAnimIndex();
#if DEBUG_ANIMNODERANDOM
		debugf(TEXT("%3.2f UAnimNodeRandom::PlayPendingAnimation. Picked PendingChildIndex: %d"), GWorld->GetTimeSeconds(), PendingChildIndex);
#endif
	}
}

void UAnimNodeRandom::OnBecomeRelevant()
{
	Super::OnBecomeRelevant();

	// Node becoming relevant, make sure we can start animation properly.
	if( ActiveChildIndex >=0 && ActiveChildIndex < RandomInfo.Num() )
	{
		FRandomAnimInfo& Info = RandomInfo(ActiveChildIndex);
		if( !PlayingSeqNode || !PlayingSeqNode->AnimSeq || !PlayingSeqNode->bPlaying || Info.bStillFrame )
		{
#if DEBUG_ANIMNODERANDOM
			debugf(TEXT("%3.2f UAnimNodeRandom::OnBecomeRelevant. Starting a new animation."), GWorld->GetTimeSeconds());
#endif
			PlayPendingAnimation();
		}
	}
	else
	{
		// we're not doing anything??
		PlayPendingAnimation();
	}
}


/** Ticking, updates weights... */
void UAnimNodeRandom::TickAnim(FLOAT DeltaSeconds)
{
	// Check for transition when a new animation is playing
	// in order to trigger blend early for smooth transitions.
	if( ActiveChildIndex >=0 && ActiveChildIndex < RandomInfo.Num() )
	{
		FRandomAnimInfo& Info = RandomInfo(ActiveChildIndex);
		// Track when animation loops over here to pick next child.
		if( PlayingSeqNode && PlayingSeqNode->SynchGroupName != NAME_None && PlayingSeqNode->bLooping )
		{
			// if we wrapped over, then pick next child for next time.
			if( (PlayingSeqNode->CurrentTime - Info.LastPosition) * PlayingSeqNode->GetGlobalPlayRate() < 0 )
			{
				PendingChildIndex = PickNextAnimIndex();
#if DEBUG_ANIMNODERANDOM
				debugf(TEXT("%3.2f UAnimNodeRandom::TickAnim. Animation Looped. Picked PendingChildIndex: %d"), GWorld->GetTimeSeconds(), PendingChildIndex);
#endif
			}
		}

		if( PlayingSeqNode )
		{
#if DEBUG_ANIMNODERANDOM
			debugf(TEXT("%3.2f UAnimNodeRandom::TickAnim. LastPosition: %f, CurrentTime: %f"), GWorld->GetTimeSeconds(), Info.LastPosition, PlayingSeqNode->CurrentTime);
#endif
			Info.LastPosition = PlayingSeqNode->CurrentTime;
		}

		// Do this only when transitioning from one animation to another. not looping.
		if( ActiveChildIndex != PendingChildIndex )
		{
			if( Info.BlendInTime > 0.f && PlayingSeqNode && PlayingSeqNode->AnimSeq )
			{
				const FLOAT TimeLeft = PlayingSeqNode->GetTimeLeft();

				if( TimeLeft <= Info.BlendInTime )
				{
#if DEBUG_ANIMNODERANDOM
					debugf(TEXT("%3.2f UAnimNodeRandom::TickAnim. Blending to pending animation. TimeLeft: %f"), GWorld->GetTimeSeconds(), TimeLeft);
#endif
					PlayPendingAnimation(TimeLeft);
				}
			}
		}
	}
	else
	{
		// we're not doing anything??
		PlayPendingAnimation();
	}

	// Call Super::TickAnim last to set proper children weights.
	Super::TickAnim(DeltaSeconds);
}


///////////////////////////////////////
/////// AnimNodeBlendByPhysics ////////
///////////////////////////////////////

/**
 * Blend animations based on an Owner's physics.
 *
 * @param DeltaSeconds	Time since last tick in seconds.
 */
void UAnimNodeBlendByPhysics::TickAnim(FLOAT DeltaSeconds)
{
	AActor* Owner = SkelComponent ? Cast<AActor>(SkelComponent->GetOwner()) : NULL;

	if ( Owner )
	{
		if( ActiveChildIndex != Owner->Physics )
		{
			SetActiveChild( Owner->Physics , 0.1f );
		}
	}

	// Call Super::TickAnim last to set proper children weights.
	Super::TickAnim(DeltaSeconds);
}


/************************************************************************************
 * UAnimNodeBlendByBase
 ***********************************************************************************/

void UAnimNodeBlendByBase::TickAnim(FLOAT DeltaSeconds)
{
	AActor* AOwner = SkelComponent ? SkelComponent->GetOwner() : NULL;

	if( AOwner && AOwner->Base != CachedBase )
	{
		INT DesiredChildIdx = 0;

		CachedBase = AOwner->Base;
		if( CachedBase )
		{
			switch( Type )
			{
				case BBT_ByActorTag:
					if( CachedBase->Tag == ActorTag )
					{
						DesiredChildIdx = 1;
					}
					break;

				case BBT_ByActorClass:
					if( CachedBase->GetClass() == ActorClass )
					{
						DesiredChildIdx = 1;
					}
					break;

				default:
					break;
			}
		}
		
		if( DesiredChildIdx != ActiveChildIndex )
		{
			SetActiveChild(DesiredChildIdx, BlendTime);
		}
	}

	// Call Super::TickAnim last to set proper children weights.
	Super::TickAnim(DeltaSeconds);
}


/************************************************************************************
 * UAnimNodeScalePlayRate
 ***********************************************************************************/

void UAnimNodeScalePlayRate::TickAnim(FLOAT DeltaSeconds)
{
	if( Children(0).Anim )
	{
		TArray<UAnimNodeSequence*> SeqNodes;
		Children(0).Anim->GetAnimSeqNodes(SeqNodes);

		const FLOAT Rate = GetScaleValue();

		for( INT i=0; i<SeqNodes.Num(); i++ )
		{
			SeqNodes(i)->Rate = Rate;
		}
	}

	// Call Super::TickAnim last to set proper children weights.
	Super::TickAnim(DeltaSeconds);
}


FLOAT UAnimNodeScalePlayRate::GetScaleValue()
{
	return ScaleByValue;
}


/************************************************************************************
 * UAnimNodeScaleRateBySpeed
 ***********************************************************************************/

FLOAT UAnimNodeScaleRateBySpeed::GetScaleValue()
{
	AActor* Owner = SkelComponent ? SkelComponent->GetOwner() : NULL;
	if( Owner && BaseSpeed > KINDA_SMALL_NUMBER )
	{
		return Owner->Velocity.Size() / BaseSpeed;
	}
	else
	{
		return ScaleByValue;
	}
}


/************************************************************************************
 * UAnimNodePlayCustomAnim
 ***********************************************************************************/

/**
 * Main tick
 */
void UAnimNodePlayCustomAnim::TickAnim(FLOAT DeltaSeconds)
{
	// check for custom animation timing, to blend out before it is over.
	if( bIsPlayingCustomAnim && CustomPendingBlendOutTime >= 0.f )
	{
		UAnimNodeSequence *ActiveChild = Cast<UAnimNodeSequence>(Children(1).Anim);
		if( ActiveChild && ActiveChild->AnimSeq )
		{
			const FLOAT TimeLeft = ActiveChild->AnimSeq->SequenceLength - ActiveChild->CurrentTime;

			// if it's time to blend back to previous animation, do so.
			if( TimeLeft <= CustomPendingBlendOutTime )
			{
				bIsPlayingCustomAnim = FALSE;
			}
		}
	}

	const FLOAT DesiredChild2Weight = bIsPlayingCustomAnim ? 1.f : 0.f;

	// Child needs to be updated
	if( DesiredChild2Weight != Child2WeightTarget )
	{
		FLOAT BlendInTime = 0.f;

		// if blending out from Custom animation node, use CustomPendingBlendOutTime
		if( Child2WeightTarget == 1.f && CustomPendingBlendOutTime >= 0 )
		{
			BlendInTime					= CustomPendingBlendOutTime;
			CustomPendingBlendOutTime	= -1;
		}
		SetBlendTarget(DesiredChild2Weight, BlendInTime);
	}

	// Call Super::TickAnim last to set proper children weights.
	Super::TickAnim(DeltaSeconds);
}


/**
 * Play a custom animation.
 * Supports many features, including blending in and out. *
 * @param	AnimName		Name of animation to play.
 * @param	Rate			Rate the animation should be played at.
 * @param	BlendInTime		Blend duration to play anim.
 * @param	BlendOutTime	Time before animation ends (in seconds) to blend out.
 *							-1.f means no blend out. 
 *							0.f = instant switch, no blend. 
 *							otherwise it's starting to blend out at AnimDuration - BlendOutTime seconds.
 * @param	bLooping		Should the anim loop? (and play forever until told to stop)
 * @param	bOverride		play same animation over again only if bOverride is set to true.
 */
FLOAT UAnimNodePlayCustomAnim::PlayCustomAnim(FName AnimName, FLOAT Rate, FLOAT BlendInTime, FLOAT BlendOutTime, UBOOL bLooping, UBOOL bOverride)
{
	// Pre requisites
	if( AnimName == NAME_None || Rate <= 0.f )
	{
		return 0.f;
	}

	UAnimNodeSequence* Child = Cast<UAnimNodeSequence>(Children(1).Anim);
	if( Child )
	{
		SetBlendTarget(1.f, BlendInTime);
		bIsPlayingCustomAnim = TRUE;

		// when looping an animation, blend out time is not supported
		CustomPendingBlendOutTime = bLooping ? -1.f : BlendOutTime;

		// if already playing the same anim, replay it again only if bOverride is set to true.
		if( Child->AnimSeqName == AnimName && Child->bPlaying && !bOverride && Child->bLooping == bLooping )
		{
			return 0.f;
		}

		if( Child->AnimSeqName != AnimName )
		{
			Child->SetAnim(AnimName);
		}
		Child->PlayAnim(bLooping, Rate, 0.f);
		return Child->GetAnimPlaybackLength();
	}
	return 0.f;
}


/**
 * Play a custom animation. 
 * Auto adjusts the animation's rate to match a given duration in seconds.
 * Supports many features, including blending in and out.
 *
 * @param	AnimName		Name of animation to play.
 * @param	Duration		duration in seconds the animation should be played.
 * @param	BlendInTime		Blend duration to play anim.
 * @param	BlendOutTime	Time before animation ends (in seconds) to blend out.
 *							-1.f means no blend out. 
 *							0.f = instant switch, no blend. 
 *							otherwise it's starting to blend out at AnimDuration - BlendOutTime seconds.
 * @param	bLooping		Should the anim loop? (and play forever until told to stop)
 * @param	bOverride		play same animation over again only if bOverride is set to true.
 */
void UAnimNodePlayCustomAnim::PlayCustomAnimByDuration(FName AnimName, FLOAT Duration, FLOAT BlendInTime, FLOAT BlendOutTime, UBOOL bLooping, UBOOL bOverride)
{
	// Pre requisites
	if( AnimName == NAME_None || Duration <= 0.f )
	{
		return;
	}

	UAnimSequence* AnimSeq = SkelComponent->FindAnimSequence(AnimName);
	if( AnimSeq )
	{
		const FLOAT NewRate = AnimSeq->SequenceLength / (Duration * AnimSeq->RateScale);
		PlayCustomAnim(AnimName, NewRate, BlendInTime, BlendOutTime, bLooping, bOverride);
	}
	else
	{
		debugf(TEXT("UWarAnim_PlayCustomAnim::PlayAnim - AnimSequence for %s not found"), *AnimName.ToString());
	}
}


/** 
 * Stop playing a custom animation. 
 * Used for blending out of a looping custom animation.
 */
void UAnimNodePlayCustomAnim::StopCustomAnim(FLOAT BlendOutTime)
{
	if( bIsPlayingCustomAnim )
	{
		bIsPlayingCustomAnim		= FALSE;
		CustomPendingBlendOutTime	= BlendOutTime;
	}
}


/************************************************************************************
 * UAnimNodeSlot
 ***********************************************************************************/

void UAnimNodeSlot::UpdateWeightsForAdditiveAnimations()
{
	// Make sure non additive channels + source weights add up to 1.
	FLOAT NonAdditiveWeight = 0.f;
	for(INT i=1; i<Children.Num(); i++)
	{
		if( !Children(i).bIsAdditive )
		{
			NonAdditiveWeight += Children(i).Weight;
		}
	}

	// source input has remainder.
	Children(0).Weight = 1.f - Clamp<FLOAT>(NonAdditiveWeight, 0.f, 1.f);
}

/**
 * Will ensure TargetWeight array is right size. If it has to resize it, will set Chidlren(0) to have a target of 1.0.
 * Also, if all Children weights are zero, will set Children(0) as the active child.
 * 
 * @see UAnimNode::InitAnim
 */
void UAnimNodeSlot::InitAnim(USkeletalMeshComponent* MeshComp, UAnimNodeBlendBase* Parent)
{
	START_INITANIM_TIMER
	{
		EXCLUDE_PARENT_TIME
		Super::InitAnim(MeshComp, Parent);
	}

	// if re-initialized after we have pooled sequence nodes, 
	// then we need to reinitialize them as well
	// i.e. when skeletalmesh is replaced via SetSkeletalMesh
	for (INT I=1; I<Children.Num(); ++I)
	{
		if ( Children(I).Anim!=NULL )
		{
			Children(I).Anim->SkelComponent = MeshComp;
			Children(I).Anim->InitAnim(MeshComp, NULL);
		}
	}

	if( TargetWeight.Num() != Children.Num() )
	{
		TargetWeight.Empty();
		TargetWeight.AddZeroed(Children.Num());

		if( TargetWeight.Num() > 0 )
		{
			TargetWeight(0) = 1.f;
		}
	}

	// Make sure weights are up to date.
	UpdateWeightsForAdditiveAnimations();

	// If all child weights are zero - set the first one to the active child.
	const FLOAT ChildWeightSum = GetChildWeightTotal();
	if( ChildWeightSum <= ZERO_ANIMWEIGHT_THRESH )
	{
		SetActiveChild(TargetChildIndex, 0.f);
	}
}

/** Update position of given channel */
void UAnimNodeSlot::MAT_SetAnimPosition(INT ChannelIndex, FName InAnimSeqName, FLOAT InPosition, UBOOL bFireNotifies, UBOOL bLooping, UBOOL bEnableRootMotion)
{
	const INT ChildNum = ChannelIndex + 1;

	if( ChildNum >= Children.Num() )
	{
		debugf(TEXT("UAnimNodeSlot::MAT_SetAnimPosition, invalid ChannelIndex: %d"), ChannelIndex);
		return;
	}

	// for matinee, we need to make
	EnsureChildExists(ChildNum);

	UAnimNodeSequence* SeqNode = Cast<UAnimNodeSequence>(Children(ChildNum).Anim);
	if( SeqNode )
	{
		// make sure they don't exists in tickarray
		// these seqNodes gets updated here manually, so if ticknode gets called
		// it will change previoustime/currenttime messing up root motion
		if ( SkelComponent && SkelComponent->AnimAlwaysTickArray.ContainsItem(SeqNode) )
		{
 			SkelComponent->AnimAlwaysTickArray.RemoveItem(SeqNode);
		}

		// Update Animation if needed
		if( SeqNode->AnimSeqName != InAnimSeqName || SeqNode->AnimSeq == NULL )
		{
			SeqNode->SetAnim(InAnimSeqName);
			// first time just clear prev/currenttime
			SeqNode->SetPosition(InPosition, FALSE);
		}

		// need to clear mirror skeleton - otherwise previous set of mirror skeleton will be used for this animation
		Children(ChildNum).bMirrorSkeleton = FALSE;
		Children(ChildNum).bIsAdditive = SeqNode->AnimSeq ? SeqNode->AnimSeq->bIsAdditive : FALSE;

		// if root motion is used, set proper value
		if (SkelComponent)
		{
			if ( bEnableRootMotion )
			{
				SkelComponent->RootMotionMode = RMM_Translate;
				SeqNode->SetRootBoneAxisOption(RBA_Translate, RBA_Translate, RBA_Translate);
				SkelComponent->RootMotionRotationMode = RMRM_RotateActor;
				SeqNode->SetRootBoneRotationOption(RRO_Extract, RRO_Extract, RRO_Extract);
			}
			else
			{
				SkelComponent->RootMotionMode = RMM_Ignore;
				SeqNode->SetRootBoneAxisOption(RBA_Default, RBA_Default, RBA_Default);
				SkelComponent->RootMotionRotationMode = RMRM_Ignore;
				SeqNode->SetRootBoneRotationOption(RRO_Default, RRO_Default, RRO_Default);
			}
		}

		// set rate to be default. If we would like to change rate, we need option parameter
		SeqNode->Rate = 1.f;
		SeqNode->bLooping = bLooping;
		// anticipate the fact the SkelComp TickTag will be increased later this frame when ticking AnimNodes.
		// Ugly, but needed for AnimMetadata :(
		SeqNode->NodeTickTag = SkelComponent->TickTag + 1; 

		// set previous time here
		FLOAT PreviousTime = SeqNode->CurrentTime;
		// Set new position
		SeqNode->SetPosition(InPosition, bFireNotifies);
		// Set correct PreviousTime if root motion is enabled since otherwise it won't get applied
		if (bEnableRootMotion)
		{
			SeqNode->PreviousTime = PreviousTime;
		}

		// AnimMetadata Update -- copy from UAnimSequence::TickAnim()
		if( SeqNode->AnimSeq )
		{
			for(INT Index=0; Index<SeqNode->AnimSeq->MetaData.Num(); Index++)
			{
				UAnimMetaData* AnimMetadata = SeqNode->AnimSeq->MetaData(Index);
				if( AnimMetadata )
				{
					AnimMetadata->TickMetaData(SeqNode);
				}
			}
		}

#if 0 
		if ( bEnableRootMotion && SkelComponent->GetOwner() )
		{
			AActor * SkelOwner = SkelComponent->GetOwner();
			debugf(TEXT("(%s) (%s) playing animation (Loc:%s, Rot:%s) - PreviousTime (%0.2f), CurrentTime (%0.2f)"), *SkelOwner->GetName(), *InAnimSeqName.GetNameString(), *SkelOwner->Location.ToString(), *SkelOwner->Rotation.ToString(), SeqNode->PreviousTime, SeqNode->CurrentTime);
		}
#endif
	}
}

/** Update weight of channels */
void UAnimNodeSlot::MAT_SetAnimWeights(const FAnimSlotInfo& SlotInfo)
{
	const INT NumChilds = Children.Num();

	if( NumChilds == 1 )
	{
		// Only one child, not much choice here!
		Children(0).Weight = 1.f;
	}
	else if( NumChilds >= 2 )
	{
		// number of channels from Matinee
		const INT NumChannels	= SlotInfo.ChannelWeights.Num();
		FLOAT AccumulatedWeight = 0.f;

		// Set blend weight to each child, from Matinee channels alpha.
		// Start from last to first, as we want bottom channels to have precedence over top ones.
		for(INT i=Children.Num()-1; i>0; i--)
		{
			const INT	ChannelIndex	= i - 1;
			const FLOAT ChannelWeight	= ChannelIndex < NumChannels ? Clamp<FLOAT>(SlotInfo.ChannelWeights(ChannelIndex), 0.f, 1.f) : 0.f;
			UAnimNodeSequence * AnimSeq = Cast<UAnimNodeSequence>(Children(i).Anim);
			if (AnimSeq && AnimSeq->AnimSeq && AnimSeq->AnimSeq->bIsAdditive)
			{
				// additive doesn't have to be at 1
				Children(i).Weight			= ChannelWeight;
			}
			else
			{
				Children(i).Weight			= ChannelWeight * (1.f - AccumulatedWeight);
				AccumulatedWeight			+= Children(i).Weight;
			}
		}
		
		UAnimNodeSequence * AnimSeq = Cast<UAnimNodeSequence>(Children(0).Anim);
		if (!AnimSeq || !AnimSeq->AnimSeq || !AnimSeq->AnimSeq->bIsAdditive)
		{
			// Set remaining weight to "normal" / animtree animation.
			Children(0).Weight = 1.f - AccumulatedWeight;
		}
	}
}


/** Rename all child nodes upon Add/Remove, so they match their position in the array. */
void UAnimNodeSlot::RenameChildConnectors()
{
	const INT NumChildren = Children.Num();

	if( NumChildren > 0 )
	{
		//First pin has to be source.
		//@TODO, remove rename/delete pin options from this entry
		Children(0).Name = FName(TEXT("Source"));

		for(INT i=1; i<NumChildren; i++)
		{
			FName OldFName = Children(i).Name;
			FString OldStringName = Children(i).Name.ToString();
			//if it contains "Channel " as the first string, it more than likely isn't custom named.
			if ((OldStringName.InStr("Channel ")==0) || (OldFName == NAME_None))
			{
				Children(i).Name = FName(*FString::Printf(TEXT("Channel %2d"), i-1));
			}
		}
	}
}

/**
 * When requested to play a new animation, we need to find a new child.
 * We'd like to find one that is unused for smooth blending, 
 * but that may be a luxury that is not available.
 */
INT UAnimNodeSlot::FindBestChildToPlayAnim(FName AnimToPlay, UBOOL bOverride)
{
	// If not overriding, see if we can find the animation we're playing
	// Because in that case, we'll reuse this one, instead of playing a new one.
	if( !bOverride && bIsPlayingCustomAnim )
	{
		UAnimNodeSequence* SeqNode = GetCustomAnimNodeSeq();
		if( SeqNode && SeqNode->AnimSeqName == AnimToPlay )
		{
			return CustomChildIndex;
		}
	}
		
	FLOAT	BestWeight	= 1.f;
	INT		BestIndex	= INDEX_NONE;

	for(INT i=1; i<Children.Num(); i++)
	{
		if( BestIndex == INDEX_NONE || Children(i).Weight < BestWeight )
		{
			BestIndex	= i;
			BestWeight	= Children(i).Weight;

			// If we've found a perfect candidate, no need to look further!
			if( BestWeight <= ZERO_ANIMWEIGHT_THRESH )
			{
				break;
			}
		}
	}

	// Send a warning if node we've picked is relevant. This is going to result in a visible pop.
	if( BestWeight > ZERO_ANIMWEIGHT_THRESH )
	{
		AActor* AOwner = SkelComponent ? SkelComponent->GetOwner() : NULL;
		debugf(NAME_DevAnim, TEXT("UAnimNodeSlot::FindBestChildToPlayAnim - Best Index %d with a weight of %f, for Anim: %s and Owner: %s"), 
			BestIndex, BestWeight, *AnimToPlay.ToString(), *AOwner->GetName());
	}

	return BestIndex;
}

/**
 * Play a custom animation.
 * Supports many features, including blending in and out.
 *
 * @param	AnimName		Name of animation to play.
 * @param	Rate			Rate the animation should be played at.
 * @param	BlendInTime		Blend duration to play anim.
 * @param	BlendOutTime	Time before animation ends (in seconds) to blend out.
 *							-1.f means no blend out. 
 *							0.f = instant switch, no blend. 
 *							otherwise it's starting to blend out at AnimDuration - BlendOutTime seconds.
 * @param	bLooping		Should the anim loop? (and play forever until told to stop)
 * @param	bOverride		play same animation over again only if bOverride is set to true.
 * @param	StartTime		When to start the anim (e.g. start at 2 seconds into the anim)
 * @param	EndTime		    When to end the anim (e.g. end at 4 second into the anim)
 */
FLOAT UAnimNodeSlot::PlayCustomAnim(FName AnimName, FLOAT Rate, FLOAT BlendInTime, FLOAT BlendOutTime, UBOOL bLooping, UBOOL bOverride, FLOAT StartTime, FLOAT EndTime)
{
	// Pre requisites
	if( AnimName == NAME_None || Rate == 0.f )
	{
		return 0.f;
	}

	if ( bIsBeingUsedByInterpGroup )
	{
		debugf(TEXT("UAnimNodeSlot::PlayCustomAnim, AnimName (%s) can't be played because being used by Matinee."), *AnimName.GetNameString());
		return 0.f;
	}

	// Figure out on which child to the play the animation on.
	CustomChildIndex = FindBestChildToPlayAnim(AnimName, bOverride);
	

	if( CustomChildIndex < 1 || CustomChildIndex >= Children.Num() )
	{
		debugf(TEXT("UAnimNodeSlot::PlayCustomAnim, CustomChildIndex %d is out of bounds."), CustomChildIndex);
		return 0.f;
	}

	// make sure you get as claimed, and commit later
	EnsureChildExists(CustomChildIndex, TRUE);

	UAnimNodeSequence* SeqNode = Cast<UAnimNodeSequence>(Children(CustomChildIndex).Anim);
	if( SeqNode )
	{
		SelectedActiveSequence( SeqNode );
		UBOOL bSetAnim = TRUE;

		// if already playing the same anim, replay it again only if bOverride is set to true.
		if( !bOverride && 
			SeqNode->bPlaying && SeqNode->bLooping == bLooping && 
			SeqNode->AnimSeqName == AnimName && SeqNode->AnimSeq != NULL )
		{
			bSetAnim = FALSE;
		}

		if( bSetAnim )
		{
			if( SeqNode->AnimSeqName != AnimName || SeqNode->AnimSeq == NULL )
			{
				SeqNode->SetAnim(AnimName);
				if( SeqNode->AnimSeq == NULL )
				{
					// Animation was not found, so abort. because we wouldn't be able to blend out.
					debugf(TEXT("PlayCustomAnim %s, CustomChildIndex: %d, Animation Not Found!!"), *AnimName.ToString(), CustomChildIndex);
					// still need to mark it as use, otherwise it won't be released when weight goes 0
#if USE_SLOTNODE_ANIMSEQPOOL
					GAnimSlotNodeSequencePool.CommitToUse(SkelComponent, SeqNode);
#endif
					return 0.f;
				}

				// Set additive flag on this node.
				if( !bAdditiveAnimationsOverrideSource )
				{
					Children(CustomChildIndex).bIsAdditive = SeqNode->AnimSeq->bIsAdditive;
				}
			}

			// Play the animation
			SeqNode->EndTime = EndTime;
			SeqNode->PlayAnim(bLooping, Rate, StartTime);
		}

		SetActiveChild(CustomChildIndex, BlendInTime);
		bIsPlayingCustomAnim = TRUE;

		// when looping an animation, blend out time is not supported
		PendingBlendOutTime = bLooping ? -1.f : BlendOutTime;

		// Disable Actor AnimEnd notification.
		SetActorAnimEndNotification(FALSE);
		
#if USE_SLOTNODE_ANIMSEQPOOL
		// Since we're now adding to tick array
		// we need to commit to make sure we're ready and safe to commit
		GAnimSlotNodeSequencePool.CommitToUse(SkelComponent, SeqNode);
#endif
		// allow bPauseAnims to be supported for this node type if we want */
		if(!bDontAddToAlwaysTickArray)
		{
			// Force the AnimNodeSlot and AnimNodeSequence to be always ticked.
			SkelComponent->AnimAlwaysTickArray.AddUniqueItem(this);
			SkelComponent->AnimAlwaysTickArray.AddUniqueItem(SeqNode);
		}

#if 0 // DEBUG
		UAnimNodeSequence* SeqNode = GetCustomAnimNodeSeq();
		debugf(TEXT("PlayCustomAnim %s, CustomChildIndex: %d"), *AnimName.ToString(), CustomChildIndex);
#endif
		return SeqNode->GetAnimPlaybackLength();
	}
	else
	{
		debugf(TEXT("UAnimNodeSlot::PlayCustomAnim, Child %d, is not hooked up to a AnimNodeSequence."), CustomChildIndex);
	}

	return 0.f;
}


/**
 * Play a custom animation. 
 * Auto adjusts the animation's rate to match a given duration in seconds.
 * Supports many features, including blending in and out.
 *
 * @param	AnimName		Name of animation to play.
 * @param	Duration		duration in seconds the animation should be played.
 * @param	BlendInTime		Blend duration to play anim.
 * @param	BlendOutTime	Time before animation ends (in seconds) to blend out.
 *							-1.f means no blend out. 
 *							0.f = instant switch, no blend. 
 *							otherwise it's starting to blend out at AnimDuration - BlendOutTime seconds.
 * @param	bLooping		Should the anim loop? (and play forever until told to stop)
 * @param	bOverride		play same animation over again only if bOverride is set to true.
 */
UBOOL UAnimNodeSlot::PlayCustomAnimByDuration(FName AnimName, FLOAT Duration, FLOAT BlendInTime, FLOAT BlendOutTime, UBOOL bLooping, UBOOL bOverride)
{
	// Pre requisites
	if( AnimName == NAME_None || Duration <= 0.f )
	{
		return FALSE;
	}

	if ( bIsBeingUsedByInterpGroup )
	{
		debugf(TEXT("UAnimNodeSlot::PlayCustomAnim, AnimName (%s) can't be played because being used by Matinee."), *AnimName.GetNameString());
		return FALSE;
	}

	UAnimSequence* AnimSeq = SkelComponent->FindAnimSequence(AnimName);
	if( AnimSeq )
	{
		FLOAT NewRate = AnimSeq->SequenceLength / Duration;
		if( AnimSeq->RateScale > 0.f )
		{
			NewRate /= AnimSeq->RateScale;
		}

		return (PlayCustomAnim(AnimName, NewRate, BlendInTime, BlendOutTime, bLooping, bOverride) > 0.f);
	}
	else
	{
		debugf(TEXT("UAnimNodeSlot::PlayAnim - AnimSequence for %s not found"), *AnimName.ToString());
	}

	return FALSE;
}

/** Returns the Name of the currently played animation or NAME_None otherwise. */
FName UAnimNodeSlot::GetPlayedAnimation()
{
	UAnimNodeSequence* SeqNode = GetCustomAnimNodeSeq();
	if( SeqNode )
	{	
		return SeqNode->AnimSeqName;
	}

	return NAME_None;
}


/** 
 * Stop playing a custom animation. 
 * Used for blending out of a looping custom animation.
 */
void UAnimNodeSlot::StopCustomAnim(FLOAT BlendOutTime)
{
	if( bIsPlayingCustomAnim )
	{
		UAnimNodeSequence* SeqNode = GetCustomAnimNodeSeq();

#if 0 // DEBUG
		debugf(TEXT("StopCustomAnim %s, CustomChildIndex: %d %s %f"), *SeqNode->AnimSeqName.ToString(), CustomChildIndex, *SkelComponent->GetOwner()->GetFName().ToString(), BlendOutTime);
#endif
		
		// this allows up to return from FinishAnim on blendout when wanted */
		if (SeqNode)
		{
			SeqNode->bBlendingOut = TRUE;
		}

		// If we don't trigger AnimEnd notifies we can consider the animation is done playing.
		if( !SeqNode || !SeqNode->bCauseActorAnimEnd )
		{
			bIsPlayingCustomAnim = FALSE;
		}
		SetActiveChild(0, BlendOutTime);
	}
}

/**
 * Removes nodes from the AnimAlwaysTickArray so bPauseAnims will work 
 */
void UAnimNodeSlot::SetAllowPauseAnims(UBOOL bSet)
{
	if(bSet)
	{
		bDontAddToAlwaysTickArray = true;
		// Remove any nodes that might be in the always tick array
		SkelComponent->AnimAlwaysTickArray.RemoveItem(this);
		for(INT i = 0; i < Children.Num(); ++i)
		{
			UAnimNodeSequence* SeqNode = Cast<UAnimNodeSequence>(Children(i).Anim);
			if(SeqNode)
				SkelComponent->AnimAlwaysTickArray.RemoveItem(SeqNode);	
		}
	}
	else
	{
		bDontAddToAlwaysTickArray = false;
		// Add back the nodes to the always tick array
		SkelComponent->AnimAlwaysTickArray.AddUniqueItem(this);
		for(INT i = 0; i < Children.Num(); ++i)
		{
			UAnimNodeSequence* SeqNode = Cast<UAnimNodeSequence>(Children(i).Anim);
			if(SeqNode)
				SkelComponent->AnimAlwaysTickArray.AddUniqueItem(SeqNode);
		}
	}
}

/** 
 * Stop playing a custom animation. 
 * Used for blending out of a looping custom animation.
 */
void UAnimNodeSlot::SetCustomAnim(FName AnimName)
{
	if( bIsPlayingCustomAnim )
	{
		UAnimNodeSequence* SeqNode = GetCustomAnimNodeSeq();
		if( SeqNode && SeqNode->AnimSeqName != AnimName )
		{
			SeqNode->SetAnim(AnimName);

			// Set additive flag on this node.
			if( !bAdditiveAnimationsOverrideSource )
			{
				Children(CustomChildIndex).bIsAdditive = SeqNode->AnimSeq ? SeqNode->AnimSeq->bIsAdditive : FALSE;
			}
		}
	}
}

/** 
 * Set bCauseActorAnimEnd flag to receive AnimEnd() notification.
 */
void UAnimNodeSlot::SetActorAnimEndNotification(UBOOL bNewStatus)
{
	// Set all childs but the active one to FALSE. 
	// Active one is set to bNewStatus
	for(INT i=1; i<Children.Num(); i++)
	{
		UAnimNodeSequence* SeqNode = Cast<UAnimNodeSequence>(Children(i).Anim);

		if( SeqNode )
		{
			SeqNode->bCauseActorAnimEnd = (bIsPlayingCustomAnim && i == CustomChildIndex ? bNewStatus : FALSE);
		}
	}
}

/** 
 * Returns AnimNodeSequence currently selected for playing animations.
 * Note that calling PlayCustomAnim *may* change which node plays the animation.
 * (Depending on the blend in time, and how many nodes are available, to provide smooth transitions.
 */
UAnimNodeSequence* UAnimNodeSlot::GetCustomAnimNodeSeq()
{
	if( CustomChildIndex > 0 )
	{
		return Cast<UAnimNodeSequence>(Children(CustomChildIndex).Anim);
	}

	return NULL;
}


/**
 * Set custom animation root bone options.
 */
void UAnimNodeSlot::SetRootBoneAxisOption(BYTE AxisX, BYTE AxisY, BYTE AxisZ)
{
	UAnimNodeSequence* SeqNode = GetCustomAnimNodeSeq();

	if( SeqNode )
	{
		SeqNode->SetRootBoneAxisOption(AxisX, AxisY, AxisZ);
	}
}

/**
 * Set custom animation root bone options.
 */
void UAnimNodeSlot::SetRootBoneRotationOption(BYTE AxisX, BYTE AxisY, BYTE AxisZ)
{
	UAnimNodeSequence* SeqNode = GetCustomAnimNodeSeq();

	if( SeqNode )
	{
		SeqNode->SetRootBoneRotationOption(AxisX, AxisY, AxisZ); 
	}
}


/**  */
void UAnimNodeSlot::TickAnim(FLOAT DeltaSeconds)
{
	UAnimNodeSequence *SeqNode = GetCustomAnimNodeSeq();

	if( bIsPlayingCustomAnim )
	{
		// Hmm there's no animations to play, so we shouldn't be here
		if( !SeqNode || !SeqNode->AnimSeq )
		{
// 			warnf(TEXT("UAnimNodeSlot::TickAnim - Should not be here! Animation was lost while playing it!!!"));
			StopCustomAnim(0.f);
			bIsPlayingCustomAnim = FALSE;

#if USE_SLOTNODE_ANIMSEQPOOL
 			if ( SeqNode )
 			{
 				if (CustomChildIndex!=0)
				{
	 				// just in case animseq disappear
#if DEBUG_SLOTNODE_ANIMSEQPOOL
					debugf(TEXT("TickAnim(1): Releasing SlotNode Sequence (%x)."), SeqNode);
#endif
					// whenever release it, make sure it removes from tick array
 					GAnimSlotNodeSequencePool.ReleaseSlotNodeSequence(SeqNode);
 					SkelComponent->AnimAlwaysTickArray.RemoveItem(SeqNode);
				}
 			}
			// no need to remove from always tick array?
#endif
		}

		// check for custom animation timing, to blend out before it is over.
		if( PendingBlendOutTime >= 0.f )
		{
			if( SeqNode && SeqNode->AnimSeq )
			{
				const FLOAT TimeLeft = SeqNode->GetTimeLeft();

				// if it's time to blend back to previous animation, do so.
				if( TimeLeft <= PendingBlendOutTime )
				{
// 					debugf(TEXT("[%3.3f] UAnimNodeSlot::TickAnim [%s], PendingBlendOutTimer: %3.3f, TimeLeft: %3.3f, AnimLength: %3.3f, GlobalRate: %3.3f"), GWorld->GetWorldInfo()->TimeSeconds, *SeqNode->AnimSeqName.ToString(), PendingBlendOutTime, TimeLeft, SeqNode->AnimSeq->SequenceLength, SeqNode->GetGlobalPlayRate());

					// Blend out, and stop tracking this animation.
					StopCustomAnim(TimeLeft);

					// Force an AnimEnd notification earlier when we start blending out. Can improve transitions.
					if( bEarlyAnimEndNotify && SeqNode->bCauseActorAnimEnd && SkelComponent->GetOwner() )
					{
						SeqNode->bCauseActorAnimEnd = FALSE;
						bIsPlayingCustomAnim = FALSE;
						SkelComponent->GetOwner()->eventOnAnimEnd(SeqNode, DeltaSeconds, 0.f);
					}
				}
			}
		}
	}
#if USE_SLOTNODE_ANIMSEQPOOL
	else
	{
		// If it's not playing, and we don't have a valid AnimSeq - make sure the resource is freed
		if( SeqNode && !SeqNode->AnimSeq && CustomChildIndex != 0)
		{
			// just in case animseq disappear
#if DEBUG_SLOTNODE_ANIMSEQPOOL
			debugf(TEXT("TickAnim(2): Releasing SlotNode Sequence (%x)."), SeqNode);
#endif
			// whenever release it
			GAnimSlotNodeSequencePool.ReleaseSlotNodeSequence(SeqNode);
		}
		// no need to remove from always tick array?
	}
#endif

	TickChildWeights(DeltaSeconds);

	// Call Super::TickAnim last to set proper children weights.
	Super::TickAnim(DeltaSeconds);

#if USE_SLOTNODE_ANIMSEQPOOL
#if 0 
	// if it has weight, but it doesn't have child, then break
	if (NodeTotalWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		FLOAT ChildrenWeight = 0.f;
		for (INT I=0; I<Children.Num(); ++I)
		{
			ChildrenWeight += Children(I).Weight;
			// first child nodetotal weight won't be applied here
			if (I!=0 && Children(I).Weight > ZERO_ANIMWEIGHT_THRESH)
			{
				check (Children(I).Anim != NULL);
				check (Children(I).Anim->NodeTotalWeight > ZERO_ANIMWEIGHT_THRESH);
			}
		}

		check (ChildrenWeight >  ZERO_ANIMWEIGHT_THRESH);
	}
#endif // debug only
#endif
}

/* Advance time regarding child weights */
void UAnimNodeSlot::TickChildWeights(FLOAT DeltaSeconds)
{
	check(Children.Num() == TargetWeight.Num());

	// Update children weights based on their target weights
	// Do nothing if BlendTimeToGo is zero.
	if( BlendTimeToGo > 0.f )
	{
		// So we don't overshoot.
		if( BlendTimeToGo <= DeltaSeconds )
		{
			BlendTimeToGo = 0.f; 

			for(INT i=0; i<Children.Num(); i++)
			{
				// If this node is about to be stopped, have him trigger AnimEnd notifies if he needs to.
				// So special moves don't get stuck
				if( Children(i).Weight > ZERO_ANIMWEIGHT_THRESH && TargetWeight(i) <= ZERO_ANIMWEIGHT_THRESH )
				{
					UAnimNodeSequence* SeqNode = Cast<UAnimNodeSequence>(Children(i).Anim);
					if( SeqNode )
					{
						// Remove this sequence from the AnimAlwaysTickArray list.
						SkelComponent->AnimAlwaysTickArray.RemoveItem(SeqNode);
						if( SeqNode->bCauseActorAnimEnd )
						{
							SeqNode->bCauseActorAnimEnd = FALSE;
							SkelComponent->GetOwner()->eventOnAnimEnd(SeqNode, DeltaSeconds, 0.f);
						}

#if USE_SLOTNODE_ANIMSEQPOOL
						if (i!=0)
						{
							// sometimes in the event of anim end, it asks to play it again. 
							// so that means, it will get added again to always tick array
							// so make sure it's not added yet. 
							// if I do this before on anim end, some on animend expects you to have 
							// child.anim exists
							if (SkelComponent->AnimAlwaysTickArray.ContainsItem(SeqNode) == FALSE)
							{
#if DEBUG_SLOTNODE_ANIMSEQPOOL
								debugf(TEXT("TickAnim(3): Releasing SlotNode Sequence (%x)."), SeqNode);
#endif
								GAnimSlotNodeSequencePool.ReleaseSlotNodeSequence(SeqNode);
							}
						}
#endif
					}
				}
				Children(i).Weight = TargetWeight(i);
			}

			// If we blended back to default, then we're not playing a custom anim anymore
			if( TargetChildIndex == 0 )
			{
				// make sure all children are removed from the	always tick array if we're removing
				// ourselves, and reset their weights to zero (this	avoids cases where sometimes
				// the animnodeslot is no longer ticked, but its children still are, resulting in
				// animations playing forever) 
				for(INT i = 0; i < Children.Num(); i++)
				{
					Children(i).Weight = 0.0f;

					UAnimNodeSequence* SeqNode = Cast<UAnimNodeSequence>(Children(i).Anim);

					if (SkelComponent->AnimAlwaysTickArray.ContainsItem(SeqNode))
					{
						if (i!=0)
						{
							GAnimSlotNodeSequencePool.ReleaseSlotNodeSequence(SeqNode);
						}

						SkelComponent->AnimAlwaysTickArray.RemoveItem(SeqNode);
					}
				}

				bIsPlayingCustomAnim = FALSE;
				// Remove us from the AnimAlwaysTickArray list
				SkelComponent->AnimAlwaysTickArray.RemoveItem(this);
			}
		}
		else
		{
			for(INT i=0; i<Children.Num(); i++)
			{
				// Amount left to blend.
				const FLOAT BlendDelta = TargetWeight(i) - Children(i).Weight;
				Children(i).Weight += (BlendDelta / BlendTimeToGo) * DeltaSeconds;
			}

			BlendTimeToGo -= DeltaSeconds;
		}
	}

	// Make sure non additive channels + source weights add up to 1.
	UpdateWeightsForAdditiveAnimations();
}

/** Child AnimNodeSequence hit end of animation */
void UAnimNodeSlot::OnChildAnimEnd(UAnimNodeSequence* Child, FLOAT PlayedTime, FLOAT ExcessTime)
{
	// Turn off bCauseActorAnimEnd flag, because event has been called from UAnimNodeSequence::AdvanceBy.
	// prevent event from being fired again from inside UAnimSlot code.
	// Make sure its our first generation child, so we don't turn that flag off for Nodes that don't directly belong to us.
	if( Child->bCauseActorAnimEnd && SkelComponent && SkelComponent->GetOwner() && Child->ParentNodes.ContainsItem(this) )
	{
		Child->bCauseActorAnimEnd = FALSE;
		SkelComponent->GetOwner()->eventOnAnimEnd(Child, PlayedTime, ExcessTime);
	}

	Super::OnChildAnimEnd(Child, PlayedTime, ExcessTime);
}


/** When use sequence pool system, make sure if child exists before doing anything **/
void UAnimNodeSlot::EnsureChildExists(INT ChildIndex, UBOOL bClaimOnly)
{
#if USE_SLOTNODE_ANIMSEQPOOL
	if ( ChildIndex > 0 )
	{
		check(Children.IsValidIndex(ChildIndex));

		// make sure BestIndex has the children weight
		if ( Children(ChildIndex).Anim == NULL )
		{
			Children(ChildIndex).Anim = GAnimSlotNodeSequencePool.GetAnimNodeSequence(SkelComponent, this, bClaimOnly);
		}
		// if child already exists, it already has been marked as USED, which now you need to revert to claim
		else if ( bClaimOnly )
		{
			// if bClaimOnly and if the Anim still exits and mark as Used, 
			// Mark as Claimed
			GAnimSlotNodeSequencePool.MarkAsClaimed(SkelComponent, Cast<UAnimNodeSequence>(Children(ChildIndex).Anim) );
		}
	}
#endif	
}
/** 
 * Update All Children Weights
 */
void UAnimNodeSlot::UpdateChildWeight(INT ChildIndex)
{
#if USE_SLOTNODE_ANIMSEQPOOL
	check (Children.IsValidIndex(ChildIndex));

	if (ChildIndex == 0)
	{
		Super::UpdateChildWeight(ChildIndex);
	}
	else
	{
		UAnimNode* ChildNode = Children(ChildIndex).Anim;
		if( ChildNode )
		{
			// otherwise, update NodeTotalWeight of the child and bRelevant
			// if you don't update bRelevant, sync Group won't work 
			// as it relies on that boolean
			ChildNode->NodeTotalWeight = NodeTotalWeight * Children(ChildIndex).Weight;
			// Call final blend relevancy notifications
			if( !ChildNode->bRelevant )
			{
				// Node not relevant, skip to next one
				if( ChildNode->NodeTotalWeight <= ZERO_ANIMWEIGHT_THRESH )
				{
					return;
				}
				// node becoming relevant this frame.
				else
				{
					ChildNode->bRelevant = TRUE;
					ChildNode->bJustBecameRelevant = TRUE;
					ChildNode->OnBecomeRelevant();
				}
			}
			else
			{
				if( ChildNode->NodeTotalWeight <= ZERO_ANIMWEIGHT_THRESH )
				{
					ChildNode->bRelevant = FALSE;

					// Node is not going to be ticked, but still update NodeTickTag, if we do things in OnCeaseRelevant that rely on that.
					ChildNode->NodeTickTag = NodeTickTag;
					ChildNode->OnCeaseRelevant();
					ChildNode->bJustBecameRelevant = FALSE;
				}

				ChildNode->bJustBecameRelevant = FALSE;
			}
		}
	}
#else
	Super::UpdateChildWeight(ChildIndex);
#endif // USE_SLOTNODE_ANIMSEQPOOL
}

/** 
 * Clean up Slot Node Sequence Pool if used 
 * Called when world is cleaned up
 */
void UAnimNodeSlot::CleanUpSlotNodeSequencePool()
{
#if USE_SLOTNODE_ANIMSEQPOOL
	GAnimSlotNodeSequencePool.ResetAnimNodeSequencePool();
#endif 
}

/** 
 * Clean up Slot Node Sequence Pool if used 
 * Called when world is cleaned up
 */
void UAnimNodeSlot::PrintSlotNodeSequencePool()
{
#if USE_SLOTNODE_ANIMSEQPOOL
	GAnimSlotNodeSequencePool.PrintAnimNodeSequencePool();
#endif 
}

// Release all sequencenodes that belong to this skelcomponent
// mark as released and clear the slots
void UAnimNodeSlot::ReleaseSequenceNodes(const USkeletalMeshComponent * SkelComp)
{
#if USE_SLOTNODE_ANIMSEQPOOL
	GAnimSlotNodeSequencePool.ReleaseAllSlotNodeSequences(SkelComp);
	GAnimSlotNodeSequencePool.FlushReleasedSlots(SkelComp);
#endif 
}

/** 
 * Release unused Sequence nodes if released
 * Called before Tick is called, so that it doesn't leave any reference during tick
 */
void UAnimNodeSlot::FlushReleasedSequenceNodes(const USkeletalMeshComponent * SkelComp)
{
#if USE_SLOTNODE_ANIMSEQPOOL
	GAnimSlotNodeSequencePool.FlushReleasedSlots(SkelComp);
#endif 
}
/**
 * The the child to blend up to full Weight (1.0)
 * 
 * @param ChildIndex Index of child node to ramp in. If outside range of children, will set child 0 to active.
 * @param BlendTime How long for child to take to reach weight of 1.0.
 */
void UAnimNodeSlot::SetActiveChild(INT ChildIndex, FLOAT BlendTime)
{
	check(Children.Num() == TargetWeight.Num());

	if( ChildIndex < 0 || ChildIndex >= Children.Num() )
	{
		debugf( TEXT("UAnimNodeBlendList::SetActiveChild : %s ChildIndex (%d) outside number of Children (%d)."), *GetName(), ChildIndex, Children.Num() );
		ChildIndex = 0;
	}

	if( bSkipBlendWhenNotRendered && !SkelComponent->bRecentlyRendered && !GIsEditor )
	{
		BlendTime = 0.f;
	}
	else
	{
		// Scale blend time by weight of target child.
		if( TargetChildIndex != INDEX_NONE && TargetChildIndex < Children.Num() )
		{
			// Take the max time between previous child to reach zero, and new child to reach one.
			// This is because of additive animations... Source will be always 1.f and so it triggers instant blend outs.
			BlendTime *= Max<FLOAT>(Children(TargetChildIndex).Weight, (1.f - Children(ChildIndex).Weight));
		}
		else
		{
			BlendTime *= (1.f - Children(ChildIndex).Weight);
		}
	}

	// If time is too small, consider instant blend
	if( BlendTime < ZERO_ANIMWEIGHT_THRESH )
	{
		BlendTime = 0.f;
	}

	for(INT i=0; i<Children.Num(); i++)
	{
		// Child becoming active
		if( i == ChildIndex )
		{
			TargetWeight(i) = 1.0f;

			// If we basically want this straight away - don't wait until a tick to update weights.
			if( BlendTime == 0.0f || (Children(i).Weight == TargetWeight(i)) )
			{
				Children(i).Weight = 1.0f;

				// If we blended back to default, then we're not playing a custom anim anymore
				// Only handle BlendTime == 0.f here, otherwise handled in tick()
				if( ChildIndex == 0 )
				{
					bIsPlayingCustomAnim = FALSE;
					// Remove us from the AnimAlwaysTickArray list
					SkelComponent->AnimAlwaysTickArray.RemoveItem(this);
				}
			}
		}
		// Others, blending to zero weight
		else
		{
			TargetWeight(i) = 0.f;

			if( BlendTime == 0.0f || (Children(i).Weight == TargetWeight(i)) )
			{
				if( Children(i).Weight > ZERO_ANIMWEIGHT_THRESH || (i == TargetChildIndex) )
				{
					Children(i).Weight = 0.0f;

					// If this node is about to be stopped, have him trigger AnimEnd notifies if he needs to.
					// So special moves don't get stuck
					// Only handle BlendTime == 0.f here, otherwise handled in tick()
					UAnimNodeSequence* SeqNode = Cast<UAnimNodeSequence>(Children(i).Anim);
					if( SeqNode )
					{
						// Remove this sequence from the AnimAlwaysTickArray list.
						SkelComponent->AnimAlwaysTickArray.RemoveItem(SeqNode);
						if( SeqNode->bCauseActorAnimEnd )
						{
							SeqNode->bCauseActorAnimEnd = FALSE;
							SkelComponent->GetOwner()->eventOnAnimEnd(SeqNode, 0.f, 0.f);
						}

#if USE_SLOTNODE_ANIMSEQPOOL
						if (i != 0)
						{
#if DEBUG_SLOTNODE_ANIMSEQPOOL
							debugf(TEXT("SetActiveChild: Releasing SlotNode Sequence (%x)."), SeqNode);
#endif
							// sometimes in the event of anim end, it asks to play it again. 
							// so that means, it will get added again to always tick array
							// so make sure it's not added yet. 
							// if I do this before on anim end, some onanimend expects you to have 
							// child.anim exists
							if (SkelComponent->AnimAlwaysTickArray.ContainsItem(SeqNode) == FALSE)
							{
								GAnimSlotNodeSequencePool.ReleaseSlotNodeSequence(SeqNode);
							}
						}
#endif
					}
				}
			}
		}
	}

	// Update weights for additive animations.
	UpdateWeightsForAdditiveAnimations();

	BlendTimeToGo		= BlendTime;
	TargetChildIndex	= ChildIndex;
}

void UAnimNodeSlot::GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	// Update weights for additive animations.
	UpdateWeightsForAdditiveAnimations();

	Super::GetBoneAtoms(Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
}

/** Called when we add a child to this node. We reset the TargetWeight array when this happens. */
void UAnimNodeSlot::OnAddChild(INT ChildNum)
{
	Super::OnAddChild(ChildNum);

	ResetTargetWeightArray(TargetWeight, Children.Num());
}

/** Called when we remove a child to this node. We reset the TargetWeight array when this happens. */
void UAnimNodeSlot::OnRemoveChild(INT ChildNum)
{
	Super::OnRemoveChild(ChildNum);

	ResetTargetWeightArray(TargetWeight, Children.Num());
}

void UAnimNodeSlot::OnPaste()
{
	Super::OnPaste();

	ResetTargetWeightArray( TargetWeight, Children.Num() );
}

/** When child is changed, make sure it doesn't connect to 1-[n] child, which will use pool system **/	
void UAnimNodeSlot::OnChildAnimChange(INT ChildNum)
{
	Super::OnChildAnimChange(ChildNum);

	if ( ChildNum > 0 )
	{
		if ( Children(ChildNum).Anim != NULL )
		{
			Children(ChildNum).Anim = NULL;
			appMsgf(AMT_OK, TEXT("SlotNode does not need child node connected in order to play."));
		}
	}
}

/************************************************************************************
 * UAnimNodeAdditiveBlending
 ***********************************************************************************/

void UAnimNodeAdditiveBlending::InitAnim(USkeletalMeshComponent* MeshComp, UAnimNodeBlendBase* Parent)
{
	START_INITANIM_TIMER
	{
		EXCLUDE_PARENT_TIME
		Super::InitAnim(MeshComp, Parent);
	}

	// Make sure Children 0's weight is 1.f. This didn't use to be the case, so enforce it here.
	if( Children(0).Weight != 1.f )
	{
		Children(0).Weight = 1.f;
	}
}

/**
 * Set desired balance of this blend.
 * 
 * @param BlendTarget Target amount of weight to put on Children(1) (second child). Between 0.0 and 1.0. 1.0 means take all animation from second child.
 * @param BlendTime How long to take to get to BlendTarget.
 */
void UAnimNodeAdditiveBlending::SetBlendTarget(FLOAT BlendTarget, FLOAT BlendTime)
{
	Super::SetBlendTarget(BlendTarget, BlendTime);

	// Make sure that no matter what, child zero weight is 1.
	Children(0).Weight = 1.f;
}

/** Overridden to force child zero's weight to 1.f */
void UAnimNodeAdditiveBlending::TickAnim(FLOAT DeltaSeconds)
{
	if( BlendTimeToGo > 0.f )
	{
		if( BlendTimeToGo > DeltaSeconds )
		{
			// Amount we want to change Child2Weight by.
			FLOAT const BlendDelta = Child2WeightTarget - Child2Weight; 

			Child2Weight	+= (BlendDelta / BlendTimeToGo) * DeltaSeconds;
			BlendTimeToGo	-= DeltaSeconds;
		}
		else
		{
			Child2Weight	= Child2WeightTarget;
			BlendTimeToGo	= 0.f;
		}

		// debugf(TEXT("Blender: %s (%x) Child2Weight: %f BlendTimeToGo: %f"), *GetPathName(), (INT)this, Child2BoneWeights.ChannelWeight, BlendTimeToGo);
	}
	
	Children(0).Weight = 1.f;
	Children(1).Weight = Child2Weight;

	// Skip super call, as we want to force child 1 weight to 1.f
	UAnimNodeBlendBase::TickAnim(DeltaSeconds);
}

void UAnimNodeAdditiveBlending::GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	START_GETBONEATOM_TIMER

	// See if results are cached.
	if( GetCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num()) )
	{
		return;
	}

	const INT NumAtoms = SkelComponent->SkeletalMesh->RefSkeleton.Num();
	check( NumAtoms == Atoms.Num() );

	// Pass through if out additive anim input doesn't have an animation (otherwise we add the ref pose on top of animated pose!)
	UBOOL bNoAddInput = FALSE;
	UAnimNodeSequence* SeqNode = Cast<UAnimNodeSequence>(Children(1).Anim);
	if(SeqNode && !SeqNode->AnimSeq)
	{
		bNoAddInput = TRUE;
	}

	// Act as a pass through if no weight on child 1
	if( Children(1).Weight < ZERO_ANIMWEIGHT_THRESH || !Children(1).Anim || bNoAddInput || (bPassThroughWhenNotRendered && !SkelComponent->bRecentlyRendered && !GIsEditor) )
	{
		if( Children(0).Anim )
		{
			EXCLUDE_CHILD_TIME
			if( !Children(0).bMirrorSkeleton )
			{
				Children(0).Anim->GetBoneAtoms(Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
			}
			else
			{
				GetMirroredBoneAtoms(Atoms, 0, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
			}
		}
		else
		{
			RootMotionDelta.SetIdentity();
			bHasRootMotion	= 0;
			FillWithRefPose(Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
		}

		// Save cached results only if we're modifying input. Otherwise we're just a pass-through.
		if( Children(0).bMirrorSkeleton )
		{
			SaveCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num());
		}

#if WITH_EDITORONLY_DATA
#if !FINAL_RELEASE
		if ( GIsEditor )
		{
			// editor only variable for morph debugging
			LastUpdatedAnimMorphKeys = CurveKeys;
		}
#endif
#endif // WITH_EDITORONLY_DATA
		return;
	}

	FBoneAtomArray DeltaAtoms;
	DeltaAtoms.Empty(NumAtoms);
	DeltaAtoms.Add(NumAtoms);

	FBoneAtom	TmpBoneAtom;
	INT			TmpINT;
	FCurveKeyArray AdditiveKeys, BaseKeys;
	{
		EXCLUDE_CHILD_TIME
		GetChildAtoms(0, Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, BaseKeys);
		GetChildAtoms(1, DeltaAtoms, DesiredBones, TmpBoneAtom, TmpINT, AdditiveKeys);
	}

	const FLOAT BlendWeight = GetBlendWeight(Child2Weight);
	UBOOL bBlendWeight = BlendWeight < (1.f - ZERO_ANIMWEIGHT_THRESH);

	if (bBlendWeight)
	{
		// Need to blend and accumulate
		const ScalarRegister VBlendWeight(BlendWeight);
		for(INT j=0; j<DesiredBones.Num(); j++)
		{
			const INT BoneIndex = DesiredBones(j);
			FBoneAtom::BlendFromIdentityAndAccumulate(Atoms(BoneIndex), DeltaAtoms(BoneIndex), VBlendWeight);
		}
	}
	else
	{
		// Just accumulate
		for(INT j=0; j<DesiredBones.Num(); j++)
		{
			const INT BoneIndex = DesiredBones(j);
			Atoms(BoneIndex).Accumulate(DeltaAtoms(BoneIndex));
			Atoms(BoneIndex).NormalizeRotation();
		}
	}

	// blend additive curve keys
	if (GIsEditor || SkelComponent->bRecentlyRendered)
	{
		FArrayCurveKeyArray ChildArray;
		FCurveKeyArray	NewCurveKeys;
		ChildArray.AddItem(BaseKeys);
		ChildArray.AddItem(AdditiveKeys);
		if ( ChildArray.Num() > 1 && BlendCurveWeights(ChildArray, NewCurveKeys) > 0 )
		{
			CurveKeys += NewCurveKeys;
		}
		else if (ChildArray.Num() == 1)
		{
			CurveKeys += ChildArray(0);
		}
#if WITH_EDITORONLY_DATA
#if !FINAL_RELEASE
		if ( GIsEditor )
		{
			if (ChildArray.Num() == 1)
			{
				LastUpdatedAnimMorphKeys = ChildArray(0);
			}
			else
			{
				// editor only variable for morph debugging
				LastUpdatedAnimMorphKeys = NewCurveKeys;
			}
		}
#endif
#endif // WITH_EDITORONLY_DATA
	}

	SaveCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num());
}

/** Get bone atoms from child node (if no child - use ref pose). */
void UAnimNodeAdditiveBlending::GetChildAtoms(INT ChildIndex, FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	if( Children(ChildIndex).Anim )
	{
		if( !Children(ChildIndex).bMirrorSkeleton )
		{
			Children(ChildIndex).Anim->GetBoneAtoms(Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
		}
		else
		{
			GetMirroredBoneAtoms(Atoms, ChildIndex, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
		}
	}
	else
	{	
		RootMotionDelta	= FBoneAtom::Identity;
		bHasRootMotion = 0;
		FillWithRefPose(Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
	}
}

/************************************************************************************
* UAnimNodeBlendByProperty
***********************************************************************************/

void UAnimNodeBlendByProperty::InitAnim(USkeletalMeshComponent* MeshComp, UAnimNodeBlendBase* Parent)
{
	START_INITANIM_TIMER
	{
		EXCLUDE_PARENT_TIME
		Super::InitAnim(MeshComp, Parent);
	}

	AActor* Owner = SkelComponent->GetOwner();

	// See if we should use Owner's Base instead
	if( bUseOwnersBase )
	{
		Owner = Owner ? Owner->Base : NULL;
	}

	if( Owner != NULL )
	{
		UProperty* CachedProperty = FindField<UProperty>( Owner->GetClass(), *PropertyName.ToString() );
		if( CachedProperty == NULL )
		{
			APawn* P = Cast<APawn>(Owner);
			if( P != NULL && P->Controller != NULL )
			{
				Owner = P->Controller;	
			}
		}
	}

	if( CachedOwner != Owner )
	{
		CachedOwner = Owner;
		bForceUpdate = TRUE;
	}
}

void UAnimNodeBlendByProperty::TickAnim(FLOAT DeltaSeconds)
{
	if( SkelComponent && CachedOwner )
	{
		INT DesiredChildIndex = INDEX_NONE;

		// Cache the property and detect changes in name.
		if( CachedPropertyName != PropertyName || bForceUpdate )
		{
			AActor* Owner = SkelComponent->GetOwner();

			// See if we should use Owner's Base instead
			if( bUseOwnersBase )
			{
				Owner = Owner ? Owner->Base : NULL;
			}

			if( Owner != NULL )
			{
				UProperty* CachedProperty = FindField<UProperty>( Owner->GetClass(), *PropertyName.ToString() );
				if( CachedProperty == NULL )
				{
					APawn* P = Cast<APawn>(Owner);
					if( P != NULL && P->Controller != NULL )
					{
						Owner = P->Controller;	
						CachedProperty = FindField<UProperty>( CachedOwner->GetClass(), *PropertyName.ToString() );
					}
				}

				// Cache its type
				if( CachedProperty )
				{
					CachedFloatProperty = Cast<UFloatProperty>(CachedProperty);
					CachedBoolProperty = Cast<UBoolProperty>(CachedProperty);
					CachedByteProperty = Cast<UByteProperty>(CachedProperty);
				}
			}

			CachedOwner = Owner;
			CachedPropertyName = PropertyName;
			bForceUpdate = FALSE;
		}

		// See if its a float property
		if( CachedFloatProperty )
		{
			FLOAT CurrentFloatVal = *(FLOAT*)((BYTE*)CachedOwner + CachedFloatProperty->Offset);
			FLOAT UseVal = (CurrentFloatVal - FloatPropMin)/(FloatPropMax - FloatPropMin);
			UseVal = Clamp<FLOAT>(UseVal, 0.f, 1.f);

			if( Children.Num() >= 2 )
			{
				check(Children.Num() == TargetWeight.Num());

				TargetWeight(0) = Children(0).Weight = (1.f - UseVal);
				TargetWeight(1) = Children(1).Weight = UseVal;

				for(INT i=2; i<Children.Num(); i++)
				{
					TargetWeight(i) = Children(i).Weight = 0.f;
				}
			}

			// Call Super::TickAnim last to set proper children weights.
			Super::TickAnim(DeltaSeconds);

			// Skip code that sets one child to be active
			return;
		}
		// See if its a bool property
		else if( CachedBoolProperty )
		{
			UBOOL bCurrentVal = *(BITFIELD*)((BYTE*)CachedOwner + CachedBoolProperty->Offset) & CachedBoolProperty->BitMask;

			// TRUE is output 1, FALSE is 0
			DesiredChildIndex = (bCurrentVal) ? 1 : 0;
		}
		// See if its a byte/enum property
		else if( CachedByteProperty )
		{
			BYTE CurrentByteVal = *((BYTE*)CachedOwner + CachedByteProperty->Offset);

			DesiredChildIndex = CurrentByteVal;
		}

		// See if we got a desired output, and we are not already on it, and its withing the bounds of our outputs
		if((DesiredChildIndex != INDEX_NONE) && (DesiredChildIndex != ActiveChildIndex) && (DesiredChildIndex < Children.Num()))
		{
			if (bUseSpecificBlendTimes)
			{
				BlendTime = DesiredChildIndex == 0 ? BlendToChild1Time : BlendToChild2Time;
			}
			// See if we are allowed to do the blend, or if we are delayed by transitions.
			UBOOL bCanDoBlend = TRUE;
			if( ActiveChildIndex != INDEX_NONE && (ActiveChildIndex < Children.Num()) && Children(ActiveChildIndex).Anim )
			{
				bCanDoBlend = Children(ActiveChildIndex).Anim->CanBlendOutFrom();
			}
			if( bCanDoBlend && Children(DesiredChildIndex).Anim )
			{
				bCanDoBlend = Children(DesiredChildIndex).Anim->CanBlendTo();
			}
			if( bCanDoBlend )
			{
				SetActiveChild(DesiredChildIndex, BlendTime);
			}
		}
	}

	// Call Super::TickAnim last to set proper children weights.
	Super::TickAnim(DeltaSeconds);
}

/** Parent node is requesting a blend out. Give node a chance to delay that. */
UBOOL UAnimNodeBlendByProperty::CanBlendOutFrom()
{
	// See if any of our relevant children is requesting a delay.
	if( bRelevant )
	{
		for(INT i=0; i<Children.Num(); i++)
		{
			if( Children(i).Anim && Children(i).Anim->bRelevant && !Children(i).Anim->CanBlendOutFrom() )
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}


/** parent node is requesting a blend in. Give node a chance to delay that. */
UBOOL UAnimNodeBlendByProperty::CanBlendTo()
{
	// See if any of our relevant children is requesting a delay.
	if( bRelevant )
	{
		for(INT i=0; i<Children.Num(); i++)
		{
			if( Children(i).Anim && Children(i).Anim->bRelevant && !Children(i).Anim->CanBlendTo() )
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

void UAnimNodeBlendByProperty::HandleSliderMove(INT SliderIndex, INT ValueIndex, FLOAT NewSliderValue)
{
	Super::HandleSliderMove(SliderIndex, ValueIndex, NewSliderValue);

	if( bSynchronizeNodesInEditor && SkelComponent )
	{
		TArray<UAnimNode*> NodesList;
		SkelComponent->Animations->GetNodes(NodesList);

		for(INT i=0; i<NodesList.Num(); i++)
		{
			UAnimNodeBlendByProperty* BlendByPropertyNode = Cast<UAnimNodeBlendByProperty>(NodesList(i));
			if( BlendByPropertyNode && BlendByPropertyNode->bSynchronizeNodesInEditor && BlendByPropertyNode->PropertyName == PropertyName && BlendByPropertyNode->Children.Num() == Children.Num() )
			{
				// Call HandleSliderMove directly to sync up. But parent version, so we don't create a forever loop.
				BlendByPropertyNode->Super::HandleSliderMove(SliderIndex, ValueIndex, NewSliderValue);
			}
		}
	}
}

FString UAnimNodeBlendByProperty::GetNodeTitle()
{
	return PropertyName.ToString();
}



/************************************************************************************
* UAnimNode_MultiBlendPerBone
***********************************************************************************/

#define DEBUG_BLENDPERBONE_CALCMASKWEIGHT 0

/** Do any initialisation, and then call InitAnim on all children. Should not discard any existing anim state though. */
void UAnimNode_MultiBlendPerBone::InitAnim(USkeletalMeshComponent* MeshComp, UAnimNodeBlendBase* Parent)
{
	START_INITANIM_TIMER
	{
		EXCLUDE_PARENT_TIME
		Super::InitAnim(MeshComp, Parent);
	}

	// Cache Pawn owner to avoid casting every frame.
	if( PawnOwner != MeshComp->GetOwner() )
	{
		PawnOwner = Cast<APawn>(MeshComp->GetOwner());
	}

	// Masks and Children arrays do not match, argh!
	if( (MaskList.Num() + 1) != Children.Num() )
	{
		MaskList.Reset();
		if( Children.Num() > 1 )
		{
			MaskList.AddZeroed(Children.Num() - 1);
		}
	}

	// Update weights on all Mask
	// Skeletal Mesh might have changed, and may have been exported differently
	// (ie BoneIndices do not match, so we need to rexport everything based on bone names)
	for(INT i=0; i<MaskList.Num(); i++)
	{
		CalcMaskWeight(i);
	}

	// Update rules
	UpdateRules();
}


void UAnimNode_MultiBlendPerBone::UpdateRules()
{
	//debugf(TEXT("%3.2f UpdateRules"), GWorld->GetTimeSeconds());

	// Cache nodes for weighting rules
	for(INT MaskIndex=0; MaskIndex<MaskList.Num(); MaskIndex++)
	{
		FPerBoneMaskInfo& Mask = MaskList(MaskIndex);

		if( Mask.WeightRuleList.Num() == 0 )
		{
			continue;
		}

		for(INT RuleIndex=0; RuleIndex<Mask.WeightRuleList.Num(); RuleIndex++)
		{
			FWeightRule& Rule = Mask.WeightRuleList(RuleIndex);

			// Find first node
			if( Rule.FirstNode.NodeName != NAME_None )
			{
				Rule.FirstNode.CachedSlotNode = Cast<UAnimNodeSlot>(FindAnimNode(Rule.FirstNode.NodeName));
			}
			else
			{
				Rule.FirstNode.CachedNode = NULL;
			}

			// Find second node
			if( Rule.SecondNode.NodeName != NAME_None )
			{
				Rule.SecondNode.CachedSlotNode = Cast<UAnimNodeSlot>(FindAnimNode(Rule.SecondNode.NodeName));
			}
			else
			{
				Rule.SecondNode.CachedNode = NULL;
			}
		}
	}
}


/** Track Changes, and trigger updates */
void UAnimNode_MultiBlendPerBone::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// If nothing has changed, skip
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( !PropertyThatChanged )
	{
		return;
	}

#if 0 // DEBUG
	debugf(TEXT("PropertyThatChanged: %s"), *PropertyThatChanged->GetFName());
#endif

	// Update weights on all Masks
	for(INT MaskIndex=0; MaskIndex<MaskList.Num(); MaskIndex++)
	{
		FPerBoneMaskInfo& Mask = MaskList(MaskIndex);

		if( PropertyThatChanged->GetFName() == FName(TEXT("PerBoneWeightIncrease")) )
		{
			for(INT BranchIndex=0; BranchIndex<Mask.BranchList.Num(); BranchIndex++)
			{
				FBranchInfo& Branch = Mask.BranchList(BranchIndex);

				// Ensure parameters are within reasonable range
				Branch.PerBoneWeightIncrease = Clamp<FLOAT>(Branch.PerBoneWeightIncrease, -1.f, 1.f);
			}
		}

		Mask.DesiredWeight = Clamp<FLOAT>(Mask.DesiredWeight, 0.f, 1.f);

		if( PropertyThatChanged->GetFName() == FName(TEXT("BlendTimeToGo")) )
		{
			Mask.bPendingBlend = TRUE;
			Mask.BlendTimeToGo = Clamp<FLOAT>(Mask.BlendTimeToGo, 0.f, 1.f);
		}

		if( PropertyThatChanged->GetFName() == FName(TEXT("BoneName")) ||
			PropertyThatChanged->GetFName() == FName(TEXT("PerBoneWeightIncrease")) ||
			PropertyThatChanged->GetFName() == FName(TEXT("RotationBlendType")) )
		{
			// Update weights
			CalcMaskWeight(MaskIndex);
		}

		if( PropertyThatChanged->GetFName() == FName(TEXT("NodeName")) )
		{
			UpdateRules();
		}
	}
}


/** Rename all child nodes upon Add/Remove, so they match their position in the array. */
void UAnimNode_MultiBlendPerBone::RenameChildConnectors()
{
	const INT NumChildren = Children.Num();

	if( NumChildren > 0 )
	{
		//First pin has to be source.
		//@TODO, remove rename/delete pin options from this entry
		Children(0).Name = FName(TEXT("Source"));

		for(INT i=1; i<NumChildren; i++)
		{
			FName OldFName = Children(i).Name;
			FString OldStringName = Children(i).Name.ToString();
			//if it contains "Mask " as the first string, it more than likely isn't custom named.
			if ((OldStringName.InStr("Mask ")==0) || (OldFName == NAME_None))
			{
				Children(i).Name = FName(*FString::Printf(TEXT("Mask %2d"), i-1));
			}
		}
	}
}


/** A child has been added, update Mask accordingly */
void UAnimNode_MultiBlendPerBone::OnAddChild(INT ChildNum)
{
	Super::OnAddChild(ChildNum);

	// Update Mask to match Children array
	if( ChildNum > 0 )
	{
		INT MaskIndex = ChildNum - 1;

		if( MaskIndex < MaskList.Num() )
		{
			MaskList.InsertZeroed(MaskIndex, 1);
			// initialize with defaults
			CalcMaskWeight(MaskIndex);
		}
		else
		{
			MaskIndex = MaskList.AddZeroed(1);
			// initialize with defaults
			CalcMaskWeight(MaskIndex);
		}
	}
}


/** A child has been removed, update Mask accordingly */
void UAnimNode_MultiBlendPerBone::OnRemoveChild(INT ChildNum)
{
	Super::OnRemoveChild(ChildNum);

	INT MaskIndex = ChildNum > 0 ? ChildNum-1 : 0;
	if( MaskIndex < MaskList.Num() )
	{
		// Update Mask to match Children array
		MaskList.Remove(MaskIndex);
	}
}


/**
* Utility for creating the Mask PerBoneWeights array. 
* Walks down the hierarchy increasing the weight by PerBoneWeightIncrease each step.
*/
void UAnimNode_MultiBlendPerBone::CalcMaskWeight(INT MaskIndex)
{
	FPerBoneMaskInfo& Mask = MaskList(MaskIndex);

	// Clean per bone weights array
	Mask.PerBoneWeights.Reset();
	// Clear transform required bone indices array
	Mask.TransformReqBone.Reset();

	if( !SkelComponent || !SkelComponent->SkeletalMesh )
	{
		return;
	}

	TArray<FMeshBone>&	RefSkel =	SkelComponent->SkeletalMesh->RefSkeleton;
	const INT			NumBones =	RefSkel.Num();

	Mask.PerBoneWeights.AddZeroed(NumBones);
	Mask.PerBoneWeights.Shrink();
	Mask.TransformReqBone.Reserve(NumBones);

	/** Should transform parent space bone atoms to mesh space? */
	const UBOOL	bDoMeshSpaceTransform = (RotationBlendType == EBT_MeshSpace);

#if DEBUG_BLENDPERBONE_CALCMASKWEIGHT
	debugf(TEXT("UAnimNode_MultiBlendPerBone::CalcMaskWeight %s, bDoMeshSpaceTransform: %d"), *NodeName.ToString(), bDoMeshSpaceTransform);
#endif

	/** Mapping between BoneIndex and PerBoneWeightIncrease */
	TMap<INT,FLOAT>	BoneToWeightIncreaseMap;
	for(INT BranchIdx=0; BranchIdx<Mask.BranchList.Num(); BranchIdx++)
	{
		FBranchInfo& Branch = Mask.BranchList(BranchIdx);
		// If no bone name supplied abort. Assume it wanted to be cleared.
		if( Branch.BoneName == NAME_None )
		{
			continue;
		}

		const INT BoneIndex = SkelComponent->MatchRefBone(Branch.BoneName);
		BoneToWeightIncreaseMap.Set(BoneIndex, Branch.PerBoneWeightIncrease);
	}

	FLOAT LastWeight = 0.f;
	for(INT i=0; i<NumBones; i++)
	{
		// Find bone weight increase
		// Start with us and walk up our parent bone index hierarchy
		// Stop at first find
		INT	BoneIndex = i;
		FLOAT* BoneIncreasePtr = BoneToWeightIncreaseMap.Find(BoneIndex);
		while( !BoneIncreasePtr && BoneIndex > 0 )
		{
			BoneIndex = RefSkel(BoneIndex).ParentIndex;
			BoneIncreasePtr = BoneToWeightIncreaseMap.Find(BoneIndex);
		}

		if( BoneIncreasePtr )
		{
			const FLOAT ParentWeight	= Mask.PerBoneWeights(RefSkel(i).ParentIndex);
			const FLOAT WeightIncrease	= ParentWeight + *BoneIncreasePtr;
			Mask.PerBoneWeights(i)		= Clamp<FLOAT>(Mask.PerBoneWeights(i) + WeightIncrease, 0.f, 1.f);
		}

#if DEBUG_BLENDPERBONE_CALCMASKWEIGHT
		// Log per bone weights
		debugf(TEXT("  Bone: %3d Weight: %1.2f (%s)"), i, Mask.PerBoneWeights(i), *RefSkel(i).Name.ToString());
#endif
		// If rotation blending is done in mesh space, then fill up the TransformReqBone array.
		if( bDoMeshSpaceTransform )
		{
			if( i == 0 )
			{
				LastWeight = Mask.PerBoneWeights(i);
			}
			else
			{
				// if weight different than previous one, then this bone needs to be blended in mesh space
				if( Mask.PerBoneWeights(i) != LastWeight )
				{
					Mask.TransformReqBone.AddItem(i);
					LastWeight = Mask.PerBoneWeights(i);

#if DEBUG_BLENDPERBONE_CALCMASKWEIGHT 
					debugf(TEXT("    Bone added to TransformReqBone."));
#endif
				}
			}
		}
	}

	// Make sure parents are present
	EnsureParentsPresent(Mask.TransformReqBone, SkelComponent->SkeletalMesh);
	Mask.TransformReqBone.Shrink();

#if DEBUG_BLENDPERBONE_CALCMASKWEIGHT 
	for(INT i=0; i<Mask.TransformReqBone.Num(); i++)
	{
		debugf(TEXT("+ TransformReqBone: %d"), Mask.TransformReqBone(i));
	}
#endif	
}


static inline UBOOL CheckNodeRule(const FWeightNodeRule &NodeRule)
{
	return (!NodeRule.CachedSlotNode || !(NodeRule.CachedSlotNode->bIsPlayingCustomAnim || NodeRule.CachedSlotNode->BlendTimeToGo > 0.f));
}


/** Ticking, updates weights... */
void UAnimNode_MultiBlendPerBone::TickAnim(FLOAT DeltaSeconds)
{
	// Update weights for each branch
	const INT NumMasks = MaskList.Num();
	for(INT i=0; i<NumMasks; i++)
	{
		FPerBoneMaskInfo&	Mask		= MaskList(i);
		const INT			ChildIdx	= i + 1;
		FAnimBlendChild&	Child		= Children(ChildIdx);

		// See if this mask should be disabled for non local human players
		// (ie AI, other players in network...)
		if( Mask.bDisableForNonLocalHumanPlayers )
		{
			AActor* Owner		= SkelComponent->GetOwner();
			APawn*	PawnOwner	= Owner ? Owner->GetAPawn() : NULL;

			if( !PawnOwner || !PawnOwner->Controller || !PawnOwner->Controller->IsLocalPlayerController() )
			{
				Child.Weight = 0.f;
				continue;
			}
		}

		// Check rules, if they're all TRUE, then we deactivate the mask
		// This will turn the node into a pass through for optimization
		if( Mask.bWeightBasedOnNodeRules )
		{
			UBOOL	bRuleFailed = FALSE;

			// Within one rule it's an AND between the two nodes. The list is an OR between list items.
			for(INT RuleIndex=0; RuleIndex<Mask.WeightRuleList.Num(); RuleIndex++)
			{
				FWeightRule& Rule = Mask.WeightRuleList(RuleIndex);
				UBOOL bANDRuleFailed = FALSE;

				// Check rule on first node
				if( Rule.FirstNode.CachedSlotNode )
				{
					bANDRuleFailed = !CheckNodeRule(Rule.FirstNode);
				}

				// Check rule on second node
				if( Rule.SecondNode.CachedSlotNode )
				{
					bANDRuleFailed = bANDRuleFailed && !CheckNodeRule(Rule.SecondNode);
				}

				bRuleFailed = bRuleFailed || bANDRuleFailed;
			}

			// If none of the rules have failed, then we can lock the mask to a zero 0 weight (== pass through)
			if( !bRuleFailed )
			{
				Child.Weight = 0.f;
				continue;
			}
			// Otherwise, this child can be relevant.
			else if( Mask.BlendTimeToGo == 0.f )
			{
				Child.Weight = Mask.DesiredWeight;
			}
		}

		if( MaskList(i).BlendTimeToGo != 0.f )
		{
			const FLOAT	BlendDelta = Mask.DesiredWeight - Child.Weight; 

			if( Mask.bPendingBlend && Child.Anim )
			{
				// See if blend in is authorized by child
				if( BlendDelta > 0.f  && !Child.Anim->CanBlendTo() )
				{
					continue;
				}

				// See if blend out is authorized by child
				if( BlendDelta < 0.f && !Child.Anim->CanBlendOutFrom() )
				{
					continue;
				}
			}

			// Blend approved!
			Mask.bPendingBlend = FALSE;

			if( Abs(BlendDelta) > KINDA_SMALL_NUMBER && Mask.BlendTimeToGo > DeltaSeconds )
			{
				Child.Weight		+= (BlendDelta / Mask.BlendTimeToGo) * DeltaSeconds;
				Mask.BlendTimeToGo	-= DeltaSeconds;
			}
			else
			{
				Child.Weight		= Mask.DesiredWeight;
				Mask.BlendTimeToGo	= 0.f;
			}
		}	
		else
		{
			Child.Weight = Mask.DesiredWeight;
		}
	}

	// Call Super::TickAnim last to set proper children weights.
	Super::TickAnim(DeltaSeconds);
}


/** 
* Control the weight of a given Mask.
*/
void UAnimNode_MultiBlendPerBone::SetMaskWeight(INT MaskIndex, FLOAT DesiredWeight, FLOAT BlendTime)
{
	if( MaskIndex >= MaskList.Num() )
	{
		debugf(TEXT("SetMaskWeight, MaskIndex: %d out of bounds."), MaskIndex);
		return;
	}

	FPerBoneMaskInfo& Mask = MaskList(MaskIndex);

	// Set desired weight
	Mask.DesiredWeight = Clamp<FLOAT>(DesiredWeight, 0.f, 1.f);

	const INT ChildIdx		= MaskIndex + 1;
	FAnimBlendChild& Child	= Children(ChildIdx);

	const FLOAT BlendDelta	= Mask.DesiredWeight - Child.Weight; 
	UBOOL bCanDoBlend		= TRUE;

	// Scale blend time by weight left to blend. So it gives a constant blend time
	// No matter what state the blend is in already
	BlendTime *= Abs(BlendDelta);

	if( Child.Anim )
	{
		// See if blend in is authorized by child
		if( BlendDelta > 0.f && !Child.Anim->CanBlendTo() )
		{
			bCanDoBlend = FALSE;
		}

		// See if blend out is authorized by child
		if( BlendDelta < 0.f && !Child.Anim->CanBlendOutFrom() )
		{
			bCanDoBlend = FALSE;
		}
	}

	// If no time, then set instantly
	if( BlendTime < KINDA_SMALL_NUMBER )
	{
		if( !bCanDoBlend )
		{
			// If can't blend yet, delay it...
			Mask.BlendTimeToGo = (FLOAT)KINDA_SMALL_NUMBER;
		}
		else
		{
			Mask.BlendTimeToGo	= 0.f;
			Child.Weight		= Mask.DesiredWeight;
		}
	}
	// Blend over time
	else
	{
		Mask.bPendingBlend = TRUE;
		Mask.BlendTimeToGo = BlendTime;
	}
}


/** Parent node is requesting a blend out. Give node a chance to delay that. */
UBOOL UAnimNode_MultiBlendPerBone::CanBlendOutFrom()
{
	// See if any of our relevant children is requesting a delay.
	if( bRelevant )
	{
		for(INT i=0; i<Children.Num(); i++)
		{
			if( Children(i).Anim && Children(i).Anim->bRelevant && !Children(i).Anim->CanBlendOutFrom() )
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}


/** parent node is requesting a blend in. Give node a chance to delay that. */
UBOOL UAnimNode_MultiBlendPerBone::CanBlendTo()
{
	// See if any of our relevant children is requesting a delay.
	if( bRelevant )
	{
		for(INT i=0; i<Children.Num(); i++)
		{
			if( Children(i).Anim && Children(i).Anim->bRelevant && !Children(i).Anim->CanBlendTo() )
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

/** @see UAnimNode::GetBoneAtoms. */
void UAnimNode_MultiBlendPerBone::GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	START_GETBONEATOM_TIMER

	if( GetCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num()) )
	{
		return;
	}

	const TArray<FMeshBone>& RefSkel = SkelComponent->SkeletalMesh->RefSkeleton;

	// Handle case with no child connectors
	if( Children.Num() == 0 )
	{
		RootMotionDelta.SetIdentity();
		bHasRootMotion	= 0;
		FillWithRefPose(Atoms, DesiredBones, RefSkel);
		return;
	}

	// Build array of relevant children to scan
	TArray<INT>	RelevantChildren;
	// Find index of the last child with a non-zero weight.
	INT LastChildIndex = INDEX_NONE;
	const INT NumChildren = Children.Num();

	RelevantChildren.Reserve(NumChildren);
	for(INT i=0; i<NumChildren; i++)
	{
		if( Children(i).Weight > ZERO_ANIMWEIGHT_THRESH )
		{
			LastChildIndex = i;
			RelevantChildren.AddItem(i);
		}
	}

	// If only the Source is relevant, then pass right through
	if( LastChildIndex == 0 )
	{
		if( Children(0).Anim )
		{
			EXCLUDE_CHILD_TIME
				Children(0).Anim->GetBoneAtoms(Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
		}
		else
		{
			RootMotionDelta.SetIdentity();
			bHasRootMotion	= 0;
			FillWithRefPose(Atoms, DesiredBones, RefSkel);
		}

		// No need to save cached results here, nothing is done.
		return;
	}

	TArray<INT>	ChildrenHasRootMotion;
	ChildrenHasRootMotion.Empty(Children.Num());
	ChildrenHasRootMotion.AddZeroed(Children.Num());
	FBoneAtomArray ChildrenRootMotion;
	ChildrenRootMotion.Empty(Children.Num());
	ChildrenRootMotion.AddZeroed(Children.Num());

	// Now, extract atoms for source animation
	{
		// Get bone atoms from child node (if no child - use ref pose).
		if( Children(0).Anim )
		{
			EXCLUDE_CHILD_TIME
				Children(0).Anim->GetBoneAtoms(Atoms, DesiredBones, ChildrenRootMotion(0), ChildrenHasRootMotion(0), CurveKeys);

			// Update our Root Motion flag.
			bHasRootMotion |= ChildrenHasRootMotion(0);
		}
		else
		{
			ChildrenRootMotion(0)		= FBoneAtom::Identity;
			ChildrenHasRootMotion(0)	= 0;
			FillWithRefPose(Atoms, DesiredBones, RefSkel);
		}
	}

	// If this local space blend of mesh space blend?
	const UBOOL bDoMeshSpaceTransform = (RotationBlendType == EBT_MeshSpace);
	const INT NumBones = RefSkel.Num();
	const INT NumRelevantChildren = RelevantChildren.Num();

	// Allocate memory for Children Bone Atoms
	FMemMark Mark(GMainThreadMemStack);
	FArrayBoneAtomArray ChildAtomsArray;
	ChildAtomsArray.AddZeroed(NumRelevantChildren-1);

	// Local space blend, multiple masks
	if( !bDoMeshSpaceTransform )
	{
		// Iterate over each relevant child getting its atoms
		for(INT RelevantChildIndex=1; RelevantChildIndex<NumRelevantChildren; RelevantChildIndex++)
		{
			const INT			ChildIndex	= RelevantChildren(RelevantChildIndex);
			const INT			MaskIndex	= ChildIndex - 1;
			FAnimBlendChild&	Child		= Children(ChildIndex);
			FPerBoneMaskInfo&	Mask		= MaskList(MaskIndex);
			FBoneAtomArray&		ChildAtoms	= ChildAtomsArray(RelevantChildIndex-1);
			FBoneAtom&	ChildRootMotion		= ChildrenRootMotion(ChildIndex);
			INT&		ChildHasRootMotion 	= ChildrenHasRootMotion(ChildIndex);		
			ChildAtoms.Add(NumBones);

			// Get bone atoms from child node (if no child - use ref pose).
			if( Child.Anim )
			{
				EXCLUDE_CHILD_TIME
					Child.Anim->GetBoneAtoms(ChildAtoms, DesiredBones, ChildRootMotion, ChildHasRootMotion, CurveKeys);
			}
			else
			{
				ChildRootMotion	= FBoneAtom::Identity;
				ChildHasRootMotion	= 0;
				FillWithRefPose(ChildAtoms, DesiredBones, RefSkel);
			}
		}

		if( NumRelevantChildren == 2 )
		{
			LocalBlendSingleMask(Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, RelevantChildren, ChildAtomsArray, ChildrenHasRootMotion, ChildrenRootMotion);
		}
		else
		{
			LocalBlendMultipleMasks(Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, RelevantChildren, ChildAtomsArray, ChildrenHasRootMotion, ChildrenRootMotion);
		}
	}
	else
	{
		// Array of Array of Matrices for mesh space blending.
		// @fixme = try doing this with quaternion directly. It would save quite of maths by not doing Quaterion <=> FMatrix conversions.
		FArrayMatrixArray MaskTMArray;
		MaskTMArray.AddZeroed(NumRelevantChildren-1);

		// Iterate over each relevant child getting its atoms
		for(INT RelevantChildIndex=1; RelevantChildIndex<NumRelevantChildren; RelevantChildIndex++)
		{
			const INT			ChildIndex	= RelevantChildren(RelevantChildIndex);
			const INT			MaskIndex	= ChildIndex - 1;
			FAnimBlendChild&	Child		= Children(ChildIndex);
			FPerBoneMaskInfo&	Mask		= MaskList(MaskIndex);
			FBoneAtomArray&		ChildAtoms	= ChildAtomsArray(RelevantChildIndex-1);
			FMatrixArray&		MaskTM		= MaskTMArray(RelevantChildIndex-1);
			FBoneAtom&	ChildRootMotion		= ChildrenRootMotion(ChildIndex);
			INT&		ChildHasRootMotion 	= ChildrenHasRootMotion(ChildIndex);		
			ChildAtoms.Add(NumBones);
			MaskTM.Add(NumBones);

			// Transforming to mesh space, allocate when needed
			// Clear index
			Mask.TransformReqBoneIndex = 0;

			// Get bone atoms from child node (if no child - use ref pose).
			if( Child.Anim )
			{
				EXCLUDE_CHILD_TIME
					Child.Anim->GetBoneAtoms(ChildAtoms, DesiredBones, ChildRootMotion, ChildHasRootMotion, CurveKeys);
			}
			else
			{
				ChildRootMotion		= FBoneAtom::Identity;
				ChildHasRootMotion	= 0;
				FillWithRefPose(ChildAtoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
			}
		}

		if( NumRelevantChildren == 2 )
		{
			MeshSpaceBlendSingleMask(Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, RelevantChildren, ChildAtomsArray, MaskTMArray, ChildrenHasRootMotion, ChildrenRootMotion);
		}
		else
		{
			MeshSpaceBlendMultipleMasks(Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, RelevantChildren, ChildAtomsArray, MaskTMArray, ChildrenHasRootMotion, ChildrenRootMotion);
		}
	}

	SaveCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num());
}

/** 
* Special path for local blend w/ 1 mask.
* We can simplify code a lot in this case. Fastest path.
*/
FORCEINLINE void UAnimNode_MultiBlendPerBone::LocalBlendSingleMask(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, TArray<INT>& RelevantChildren, FArrayBoneAtomArray& ChildAtomsArray, const TArray<INT> & ChildrenHasRootMotion, const FBoneAtomArray & ChildrenRootMotion)
{
	const TArray<FMeshBone>& RefSkel = SkelComponent->SkeletalMesh->RefSkeleton;
	const INT NumBones = RefSkel.Num();
	const INT NumRelevantChildren = RelevantChildren.Num();

	// Make sure do indeed have 1 mask + source
	check(NumRelevantChildren == 2);

	const INT			MaskChildIndex	= RelevantChildren(1);
	FBoneAtomArray&		ChildAtoms		= ChildAtomsArray(0);
	const INT			MaskIndex		= MaskChildIndex - 1;
	FAnimBlendChild&	MaskChild		= Children(MaskChildIndex);

	const FBoneAtom&	ChildRootMotion	= ChildrenRootMotion(0);
	const INT&			ChildHasRootMotion = ChildrenHasRootMotion(0);
	const FBoneAtom&	MaskChildRootMotion = ChildrenRootMotion(MaskIndex);	
	const INT&			MaskChildHasRootMotion = ChildrenHasRootMotion(MaskIndex);

	// Iterate over each bone, and iterate over each branch to figure out the bone's weight
	// And perform blending.
	const INT NumDesiredBones = DesiredBones.Num();
	for(INT i=0; i<NumDesiredBones; i++)
	{
		const INT	BoneIndex	= DesiredBones(i);
		const UBOOL	bIsRootBone = (BoneIndex == 0);

		// Figure weight for this bone giving priority to the Mask
		const FLOAT	MaskBoneWeight = MaskChild.Weight * MaskList(MaskIndex).PerBoneWeights(BoneIndex);

		// If MaskBoneWeight is relevant, then do a blend.
		// Otherwise no need to do anything, Atoms() already has all Source bones.
		if( MaskBoneWeight > ZERO_ANIMWEIGHT_THRESH )
		{
			Atoms(BoneIndex).BlendWith(ChildAtoms(BoneIndex), MaskBoneWeight);
		}

		// Take care of root motion if we have to.
		if( bIsRootBone && (ChildHasRootMotion || MaskChildHasRootMotion) )
		{
			if( ChildHasRootMotion && MaskChildHasRootMotion )
			{
				RootMotionDelta.Blend(ChildRootMotion, MaskChildRootMotion, MaskBoneWeight);
			}
			else if( ChildHasRootMotion )
			{
				RootMotionDelta = ChildRootMotion;
			}
			else if( MaskChildHasRootMotion )
			{
				RootMotionDelta = MaskChildRootMotion;
			}
		}
	}
}

/** 
* Special path for local blend w/ multiple masks.
* As opposed to MeshSpace blends, we can get rid OF a lot OF branches and make that code faster.
*/
FORCEINLINE void UAnimNode_MultiBlendPerBone::LocalBlendMultipleMasks(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, TArray<INT>& RelevantChildren, FArrayBoneAtomArray& ChildAtomsArray, const TArray<INT> & ChildrenHasRootMotion, const FBoneAtomArray & ChildrenRootMotion)
{
	const TArray<FMeshBone>& RefSkel = SkelComponent->SkeletalMesh->RefSkeleton;
	const INT NumBones = RefSkel.Num();
	const INT NumRelevantChildren = RelevantChildren.Num();

	bHasRootMotion = 0;
	ScalarRegister AccumulatedRootMotionWeight(ScalarZero);

	// Temp Bone Atom used for blending.
	FBoneAtom BlendedBoneAtom = FBoneAtom::Identity;

	// Iterate over each bone, and iterate over each branch to figure out the bone's weight
	// And perform blending.
	const INT NumDesiredBones = DesiredBones.Num();
	for(INT i=0; i<NumDesiredBones; i++)
	{
		const INT	BoneIndex	= DesiredBones(i);
		const UBOOL	bIsRootBone = (BoneIndex == 0);

		ScalarRegister AccumulatedWeight(ScalarZero);
		UBOOL bDidInitAtom = FALSE;

		// Iterate over each child getting its atoms, scaling them and adding them to output (Atoms array)
		for(INT RelevantChildIndex=NumRelevantChildren-1; RelevantChildIndex>0; RelevantChildIndex--)
		{
			const INT			ChildIndex	= RelevantChildren(RelevantChildIndex);
			const INT			MaskIndex	= ChildIndex - 1;
			FAnimBlendChild&	Child		= Children(ChildIndex);

			const FBoneAtom&	ChildRootMotion		= ChildrenRootMotion(ChildIndex);
			const INT	&		ChildHasRootMotion 	= ChildrenHasRootMotion(ChildIndex);
			
			// Figure weight for this bone giving priority to the highest branch
			const ScalarRegister BoneWeight = (ScalarOne - AccumulatedWeight) * ScalarRegister(Child.Weight) * ScalarRegister(MaskList(MaskIndex).PerBoneWeights(BoneIndex));

			// And accumulate it, as the sum needs to be 1.f
			AccumulatedWeight = ScalarMin(AccumulatedWeight + BoneWeight, ScalarOne);

			// If weight is significant, do the blend...
			if( NonZeroAnimWeight(BoneWeight) )
			{
				FBoneAtomArray&	ChildAtoms = ChildAtomsArray(RelevantChildIndex-1);

				// If this is the root bone and child has root motion, then accumulate it
				// And set flag saying the animation we'll forward from here will contain root motion.
				if( bIsRootBone && ChildHasRootMotion )
				{
					bHasRootMotion				= 1;
					AccumulatedRootMotionWeight = ScalarMin(AccumulatedRootMotionWeight + BoneWeight, ScalarOne);
				}

				if( NonOneAnimWeight(BoneWeight) )
				{
					// We just write the first childrens atoms into the output array. Avoids zero-ing it out.
					if( !bDidInitAtom )
					{
						// Parent bone space bone atom weighting
						BlendedBoneAtom	= ChildAtoms(BoneIndex) * BoneWeight;
						bDidInitAtom	= TRUE;
					}
					else
					{
						BlendedBoneAtom.AccumulateWithShortestRotation(ChildAtoms(BoneIndex), BoneWeight);
					}
				}
				else
				{
					// If full weight, just do a straight copy, no blending - and no need to go further.
					BlendedBoneAtom = ChildAtoms(BoneIndex);
					break;
				}
			}
		} //for(INT RelevantChildIndex=NumRelevantChildren-1; RelevantChildIndex>0; RelevantChildIndex--)

		// Source gets remainder
		const ScalarRegister SourceBoneWeight = ScalarOne - AccumulatedWeight;

		// If Source is a little relevant, then we need to either copy or blend
		if( NonZeroAnimWeight(SourceBoneWeight) )
		{
			// If this is the root bone and child has root motion, then accumulate it
			// And set flag saying the animation we'll forward from here will contain root motion.
			if( bIsRootBone && ChildrenHasRootMotion(0) )
			{
				bHasRootMotion				= 1;
				AccumulatedRootMotionWeight = ScalarMin(AccumulatedRootMotionWeight + SourceBoneWeight, ScalarOne);
			}

			// We only need to touch up something if we have to blend. If source is full weight, we've already got the animation data in there.
			if( NonOneAnimWeight(SourceBoneWeight) )
			{
				Atoms(BoneIndex) *= SourceBoneWeight;
				Atoms(BoneIndex).AccumulateWithShortestRotation(BlendedBoneAtom);

				// Normalize Rotation
				Atoms(BoneIndex).NormalizeRotation();
			}
		}
		// Source not relevant. Everything comes from the mask(s)
		else
		{
			Atoms(BoneIndex) = BlendedBoneAtom;
		}

		ScalarRegister InvAccumulatedRootMotionWeight = ScalarReciprocal(AccumulatedRootMotionWeight);

		// Do another pass for root motion
		if( bIsRootBone && bHasRootMotion )
		{
			AccumulatedWeight	= ScalarZero;
			bDidInitAtom		= FALSE;
			UBOOL bDidBlendAtom	= FALSE;

			// Iterate over each child getting its atoms, scaling them and adding them to output (Atoms array)
			for(INT RelevantChildIndex=NumRelevantChildren-1; RelevantChildIndex>=0; RelevantChildIndex--)
			{
				const INT			ChildIndex	= RelevantChildren(RelevantChildIndex);
				FAnimBlendChild&	Child		= Children(ChildIndex);
				const FBoneAtom&	ChildRootMotion		= ChildrenRootMotion(ChildIndex);
				const INT&			ChildHasRootMotion	= ChildrenHasRootMotion(ChildIndex);

				if( ChildHasRootMotion )
				{
					const INT MaskIndex = ChildIndex - 1;
					ScalarRegister BoneWeight;

					if( ChildIndex > 0 )
					{
						// Figure weight for this bone giving priority to the highest branch
						BoneWeight			= (ScalarOne - AccumulatedWeight) * ScalarRegister(Child.Weight) * ScalarRegister(MaskList(MaskIndex).PerBoneWeights(BoneIndex));
						// And accumulate it, as the sum needs to be 1.f
						AccumulatedWeight	= ScalarMin(AccumulatedWeight + BoneWeight, ScalarOne);
					}
					else
					{
						// Source gets remainder
						BoneWeight = ScalarOne - AccumulatedWeight;
					}

					ScalarRegister FinalWeight = BoneWeight * InvAccumulatedRootMotionWeight;

#if 0 // Debug Root Motion
					if( !WeightedRootMotion.Translation.IsZero() )
					{
						debugf(TEXT("%3.2f [%s]  Adding weighted (%3.2f) root motion trans: %3.2f, vect: %s. ChildWeight: %3.3f"), GWorld->GetTimeSeconds(), SkelComponent->GetOwner()->GetName(), BoneWeight, WeightedRootMotion.Translation.Size(), *WeightedRootMotion.Translation.ToString(), Children(i).Weight);
					}
#endif
					// Accumulate Root Motion
					if( !bDidInitAtom )
					{
						bDidInitAtom	= TRUE;
						RootMotionDelta = ChildRootMotion * FinalWeight;
					}
					else
					{
						RootMotionDelta.AccumulateWithShortestRotation(ChildRootMotion, FinalWeight);
						bDidBlendAtom	= TRUE;
					}
				} // if( ChildHasRootMotion )

			} // for(INT RelevantChildIndex=NumRelevantChildren-1; RelevantChildIndex>=0; RelevantChildIndex--)

			// If we did blend root motion, normalize rotation quaternion
			if( bDidBlendAtom )
			{
				RootMotionDelta.NormalizeRotation();
			}
		}

	}
}

/** 
* Special path for mesh space blend w/ 1 mask.
* We can simplify code a lot in this case. Faster path. Less branches and loops.
*/
FORCEINLINE void UAnimNode_MultiBlendPerBone::MeshSpaceBlendSingleMask(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, TArray<INT>& RelevantChildren, FArrayBoneAtomArray& ChildAtomsArray, FArrayMatrixArray& MaskTMArray, const TArray<INT> & ChildrenHasRootMotion, const FBoneAtomArray & ChildrenRootMotion)
{
	const TArray<FMeshBone>& RefSkel = SkelComponent->SkeletalMesh->RefSkeleton;
	const INT NumBones = RefSkel.Num();
	const INT NumRelevantChildren = RelevantChildren.Num();

	// Make sure do indeed have 1 mask + source
	check(NumRelevantChildren == 2);

	const INT			MaskChildIndex	= RelevantChildren(1);
	const INT			MaskIndex		= MaskChildIndex - 1;
	FPerBoneMaskInfo&	Mask			= MaskList(MaskIndex); 
	FAnimBlendChild&	MaskChild		= Children(MaskChildIndex);

	const FBoneAtom&	MaskChildRootMotion	= ChildrenRootMotion(MaskChildIndex);
	const INT	&		MaskChildHasRootMotion = ChildrenHasRootMotion(MaskChildIndex);

	FBoneAtomArray&		MaskAtoms		= ChildAtomsArray(0);
	FMatrixArray&		MaskTM			= MaskTMArray(0);
	FMatrixArray		SourceTM, ResultTM;
	SourceTM.Add(NumBones);
	ResultTM.Add(NumBones);

	const INT			NumMaskTransformReqBones = Mask.TransformReqBone.Num();

	// Iterate over each bone, and iterate over each branch to figure out the bone's weight
	// And perform blending.
	const INT NumDesiredBones = DesiredBones.Num();
	UBOOL bTransformBone;
	for(INT i=0; i<NumDesiredBones; i++)
	{
		const INT	BoneIndex	= DesiredBones(i);
		const UBOOL	bIsRootBone = (BoneIndex == 0);

		// Figure out if this bone needs to be transformed to mesh space
		// It's an expensive operation, so prefer parent bone space blending whenever possible
		bTransformBone = (Mask.TransformReqBoneIndex < NumMaskTransformReqBones && BoneIndex == Mask.TransformReqBone(Mask.TransformReqBoneIndex));
		// Figure weight for this bone giving priority to the Mask
		const FLOAT	MaskBoneWeight = MaskChild.Weight * MaskList(MaskIndex).PerBoneWeights(BoneIndex);

		// Transform parent bone space BoneAtoms into mesh space matrices
		if( bTransformBone )
		{
			Mask.TransformReqBoneIndex++;

			// Transform mesh space rotation FBoneAtom to FMatrix
			MaskTM(BoneIndex) = FQuatRotationTranslationMatrix(MaskAtoms(BoneIndex).GetRotation(), FVector(0.f));
			SourceTM(BoneIndex) = FQuatRotationTranslationMatrix(Atoms(BoneIndex).GetRotation(), FVector(0.f));

			// Figure out if we're going to blend something, faster path otherwise
			// Because we basically need both bones in mesh space to blend them. Otherwise we can just copy the right one.
			const UBOOL	bWeAreBlending = (MaskBoneWeight > ZERO_ANIMWEIGHT_THRESH) && (MaskBoneWeight < 1.f - ZERO_ANIMWEIGHT_THRESH);

			if( !bIsRootBone )
			{
				const INT	ParentIndex	= RefSkel(BoneIndex).ParentIndex;

				// Transform to mesh space
				MaskTM(BoneIndex) *= MaskTM(ParentIndex);
				SourceTM(BoneIndex) *= SourceTM(ParentIndex);

				if( bWeAreBlending )
				{
					// If transforming to mesh space, then overwrite local rotation with mesh space rotation for weighting
					MaskAtoms(BoneIndex).SetRotation( FQuat(MaskTM(BoneIndex)) );
					Atoms(BoneIndex).SetRotation( FQuat(SourceTM(BoneIndex)) );

					// Do blend
					Atoms(BoneIndex).BlendWith(MaskAtoms(BoneIndex), MaskBoneWeight);

					// If blending in mesh space, we now need to turn the rotation back into parent bone space
					// Transform mesh space rotation FBoneAtom to FMatrix
					ResultTM(BoneIndex)	= FQuatRotationTranslationMatrix(Atoms(BoneIndex).GetRotation(), FVector(0.f));
				}
				// No need to blend, can do direct copy
				else 
				{
					ResultTM(BoneIndex)	= (MaskBoneWeight <= ZERO_ANIMWEIGHT_THRESH) ? SourceTM(BoneIndex) : MaskTM(BoneIndex);
				}

				// Transform mesh space rotation back to parent bone space
				const FMatrix RelativeTM	= ResultTM(BoneIndex) * ResultTM(ParentIndex).Inverse();
				Atoms(BoneIndex).SetRotation( FQuat(RelativeTM) );
			}
			else
			{
				if( bWeAreBlending )
				{
					// Do blend
					Atoms(BoneIndex).BlendWith(MaskAtoms(BoneIndex), MaskBoneWeight);

					// If blending in mesh space, we now need to turn the rotation back into parent bone space
					// Transform mesh space rotation FBoneAtom to FMatrix
					ResultTM(BoneIndex)	= FQuatRotationTranslationMatrix(Atoms(BoneIndex).GetRotation(), FVector(0.f));
				}
				else 
				{
					// Source
					if( MaskBoneWeight <= ZERO_ANIMWEIGHT_THRESH )
					{
						ResultTM(BoneIndex)	= SourceTM(BoneIndex);
					}
					// Mask
					else
					{
						// Direct copy
						Atoms(BoneIndex) = MaskAtoms(BoneIndex);
						ResultTM(BoneIndex)	= MaskTM(BoneIndex);
					}
				}
			}
		}
		// Just do fast path for local space blend
		else
		{
			Atoms(BoneIndex).BlendWith(MaskAtoms(BoneIndex), MaskBoneWeight);
		}

		// Take care of root motion if we have to.
		if( bIsRootBone && (ChildrenHasRootMotion(0) || MaskChildHasRootMotion) )
		{
			if( ChildrenHasRootMotion(0) && MaskChildHasRootMotion )
			{
				RootMotionDelta.Blend(ChildrenRootMotion(0), MaskChildRootMotion, MaskBoneWeight);
			}
			else if( ChildrenHasRootMotion(0) )
			{
				RootMotionDelta = ChildrenRootMotion(0);
			}
			else if( MaskChildHasRootMotion )
			{
				RootMotionDelta = MaskChildRootMotion;
			}
		}
	}
}

/** 
* Hardcore path. Multiple masks and mesh space transform. This is the slowest path.
*/
FORCEINLINE void UAnimNode_MultiBlendPerBone::MeshSpaceBlendMultipleMasks(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, TArray<INT>& RelevantChildren, FArrayBoneAtomArray& ChildAtomsArray, FArrayMatrixArray& MaskTMArray, const TArray<INT> & ChildrenHasRootMotion, const FBoneAtomArray & ChildrenRootMotion)
{
	const TArray<FMeshBone>& RefSkel = SkelComponent->SkeletalMesh->RefSkeleton;
	const INT NumBones = RefSkel.Num();
	const INT NumRelevantChildren = RelevantChildren.Num();

	bHasRootMotion = 0;
	ScalarRegister AccumulatedRootMotionWeight = ScalarZero;

	// Temp Bone Atom used for blending.
	FBoneAtom BlendedBoneAtom = FBoneAtom::Identity;
	FMatrixArray SourceTM, ResultTM;
	SourceTM.Add(NumBones);
	ResultTM.Add(NumBones);

	// Iterate over each bone, and iterate over each branch to figure out the bone's weight
	// And perform blending.
	const INT NumDesiredBones = DesiredBones.Num();
	for(INT i=0; i<NumDesiredBones; i++)
	{
		const INT	BoneIndex	= DesiredBones(i);
		const UBOOL	bIsRootBone = (BoneIndex == 0);

		ScalarRegister AccumulatedWeight(ScalarZero);
		UBOOL	bDidInitAtom		= FALSE;
		UBOOL	bTransformBone		= FALSE;

		// Figure out if this bone needs to be transformed to mesh space
		// It's an expensive operation, so prefer parent bone space blending whenever possible
		for(INT RelevantChildIndex=NumRelevantChildren-1; RelevantChildIndex>0; RelevantChildIndex--)
		{
			const INT			ChildIndex	= RelevantChildren(RelevantChildIndex);
			const INT			MaskIndex	= ChildIndex - 1;
			FPerBoneMaskInfo&	Mask		= MaskList(MaskIndex); 

			if( Mask.TransformReqBoneIndex < Mask.TransformReqBone.Num() &&	BoneIndex == Mask.TransformReqBone(Mask.TransformReqBoneIndex) )
			{
				Mask.TransformReqBoneIndex++;
				bTransformBone = TRUE;
			}
		}

		// Iterate over each child getting its atoms, scaling them and adding them to output (Atoms array)
		for(INT RelevantChildIndex=NumRelevantChildren-1; RelevantChildIndex>0; RelevantChildIndex--)
		{
			const INT			ChildIndex	= RelevantChildren(RelevantChildIndex);
			const INT			MaskIndex	= ChildIndex - 1;
			FAnimBlendChild&	Child		= Children(ChildIndex);
			FBoneAtomArray&		MaskAtoms	= ChildAtomsArray(RelevantChildIndex-1);
			FBoneAtom	ChildRootMotion		= ChildrenRootMotion(ChildIndex);
			INT			ChildHasRootMotion	= ChildrenHasRootMotion(ChildIndex);
			// Get current child's mesh space transform matrix
			FMatrixArray&		TM = MaskTMArray(RelevantChildIndex-1);

			// Figure weight for this bone giving priority to the highest branch
			const ScalarRegister BoneWeight = (ScalarOne - AccumulatedWeight) * ScalarRegister(Child.Weight) * ScalarRegister(MaskList(MaskIndex).PerBoneWeights(BoneIndex));

			// And accumulate it, as the sum needs to be 1.f
			AccumulatedWeight = ScalarMin(AccumulatedWeight + BoneWeight, ScalarOne);

			// Transform parent bone space BoneAtoms into mesh space matrices
			if( bTransformBone )
			{
				// Transform mesh space rotation FBoneAtom to FMatrix
				TM(BoneIndex) = FQuatRotationTranslationMatrix(MaskAtoms(BoneIndex).GetRotation(), FVector(0.f));

				// Transform to mesh space
				if( !bIsRootBone )
				{
					TM(BoneIndex) *= TM(RefSkel(BoneIndex).ParentIndex);
				}
			}

			// If weight is significant, do the blend...
			if( NonZeroAnimWeight(BoneWeight) )
			{
				// If transforming to mesh space, then overwrite local rotation with mesh space rotation for weighting
				// No need to transform rotation for root bone, as it has no parent bone.
				if( bTransformBone && !bIsRootBone )
				{
					// Turn transform matrix rotation into quaternion.
					MaskAtoms(BoneIndex).SetRotation( FQuat(TM(BoneIndex)) );
				}

				// If this is the root bone and child has root motion, then accumulate it
				// And set flag saying the animation we'll forward from here will contain root motion.
				if( bIsRootBone && ChildHasRootMotion )
				{
					bHasRootMotion				= 1;
					AccumulatedRootMotionWeight = ScalarMin(AccumulatedRootMotionWeight + BoneWeight, ScalarOne);
				}

				if( NonOneAnimWeight(BoneWeight) )
				{
					// We just write the first childrens atoms into the output array. Avoids zero-ing it out.
					if( !bDidInitAtom )
					{
						// Parent bone space bone atom weighting
						BlendedBoneAtom	= MaskAtoms(BoneIndex) * BoneWeight;
						bDidInitAtom	= TRUE;
					}
					else
					{
						// To ensure the 'shortest route', we make sure the dot product between the accumulator and the incoming child atom is positive.
						BlendedBoneAtom.AccumulateWithShortestRotation(MaskAtoms(BoneIndex), BoneWeight);
					}
				}
				else
				{
					// If full weight, just do a straight copy, no blending - and no need to go further.
					BlendedBoneAtom = MaskAtoms(BoneIndex);
					break;
				}
			}
		} //for(INT RelevantChildIndex=NumRelevantChildren-1; RelevantChildIndex>0; RelevantChildIndex--)

		// Transform parent bone space BoneAtoms into mesh space matrices
		if( bTransformBone )
		{
			// Transform mesh space rotation FBoneAtom to FMatrix
			SourceTM(BoneIndex) = FQuatRotationTranslationMatrix(Atoms(BoneIndex).GetRotation(), FVector(0.f));

			// Transform to mesh space
			if( !bIsRootBone )
			{
				SourceTM(BoneIndex) *= SourceTM(RefSkel(BoneIndex).ParentIndex);
			}
		}

		// Source gets remainder
		const ScalarRegister SourceBoneWeight = ScalarOne - AccumulatedWeight;

		// If Source is a little relevant, then we need to either copy or blend
		if( NonZeroAnimWeight(SourceBoneWeight) )
		{
			// If this is the root bone and child has root motion, then accumulate it
			// And set flag saying the animation we'll forward from here will contain root motion.
			if( bIsRootBone && ChildrenHasRootMotion(0) )
			{
				bHasRootMotion				= 1;
				AccumulatedRootMotionWeight = ScalarMin(AccumulatedRootMotionWeight + SourceBoneWeight, ScalarOne);
			}

			// If transforming to mesh space, then overwrite local rotation with mesh space rotation for weighting
			// No need to transform rotation for root bone, as it has no parent bone.
			if( bTransformBone && !bIsRootBone )
			{
				// Turn transform matrix rotation into quaternion.
				Atoms(BoneIndex).SetRotation( FQuat(SourceTM(BoneIndex)) );
			}

			// We only need to touch up something if we have to blend. If source is full weight, we've already got the animation data in there.
			if( NonOneAnimWeight(SourceBoneWeight) )
			{
				// To ensure the 'shortest route', we make sure the dot product between the accumulator and the incoming child atom is positive.
				Atoms(BoneIndex) *= SourceBoneWeight;
				Atoms(BoneIndex).AccumulateWithShortestRotation(BlendedBoneAtom);

				// Normalize Rotation
				Atoms(BoneIndex).NormalizeRotation();
			}
		}
		// Source not relevant. Everything comes from the mask(s)
		else
		{
			Atoms(BoneIndex) = BlendedBoneAtom;
		}

		// If blending in mesh space, we now need to turn the rotation back into parent bone space
		if( bTransformBone )
		{
			// Transform mesh space rotation FBoneAtom to FMatrix
			ResultTM(BoneIndex)	= FQuatRotationTranslationMatrix(Atoms(BoneIndex).GetRotation(), FVector(0.f));

			// Transform mesh space rotation back to parent bone space
			if( !bIsRootBone )
			{
				const FMatrix RelativeTM	= ResultTM(BoneIndex) * ResultTM(RefSkel(BoneIndex).ParentIndex).Inverse();
				Atoms(BoneIndex).SetRotation( FQuat(RelativeTM) );
			}
		}

		// Do another pass for root motion
		if( bIsRootBone && bHasRootMotion )
		{
			AccumulatedWeight	= ScalarZero;
			bDidInitAtom		= FALSE;
			UBOOL bDidBlendAtom	= FALSE;

			// Iterate over each child getting its atoms, scaling them and adding them to output (Atoms array)
			for(INT RelevantChildIndex=NumRelevantChildren-1; RelevantChildIndex>=0; RelevantChildIndex--)
			{
				const INT			ChildIndex	= RelevantChildren(RelevantChildIndex);
				FAnimBlendChild&	Child		= Children(ChildIndex);
				const FBoneAtom&	ChildRootMotion	= ChildrenRootMotion(ChildIndex);
				const INT&		ChildHasRootMotion	= ChildrenHasRootMotion(ChildIndex);

				if( ChildHasRootMotion )
				{
					const INT MaskIndex = ChildIndex - 1;
					ScalarRegister BoneWeight;

					if( ChildIndex > 0 )
					{
						// Figure weight for this bone giving priority to the highest branch
						BoneWeight			= (ScalarOne - AccumulatedWeight) * ScalarRegister(Child.Weight) * ScalarRegister(MaskList(MaskIndex).PerBoneWeights(BoneIndex));
						// And accumulate it, as the sum needs to be 1.f
						AccumulatedWeight	= ScalarMin(AccumulatedWeight + BoneWeight, ScalarOne);
					}
					else
					{
						// Source gets remainder
						BoneWeight = ScalarOne - AccumulatedWeight;
					}

					FBoneAtom WeightedRootMotion = ChildRootMotion * (BoneWeight * ScalarReciprocal(AccumulatedRootMotionWeight));

#if 0 // Debug Root Motion
					if( !WeightedRootMotion.Translation.IsZero() )
					{
						debugf(TEXT("%3.2f [%s]  Adding weighted (%3.2f) root motion trans: %3.2f, vect: %s. ChildWeight: %3.3f"), GWorld->GetTimeSeconds(), SkelComponent->GetOwner()->GetName(), BoneWeight, WeightedRootMotion.Translation.Size(), *WeightedRootMotion.Translation.ToString(), Children(i).Weight);
					}
#endif
					// Accumulate Root Motion
					if( !bDidInitAtom )
					{
						bDidInitAtom	= TRUE;
						RootMotionDelta = WeightedRootMotion;
					}
					else
					{
						RootMotionDelta.AccumulateWithShortestRotation(WeightedRootMotion);
						bDidBlendAtom	= TRUE;
					}
				} // if( ChildHasRootMotion )

			} // for(INT RelevantChildIndex=NumRelevantChildren-1; RelevantChildIndex>=0; RelevantChildIndex--)

			// If we did blend root motion, normalize rotation quaternion
			if( bDidBlendAtom )
			{
				RootMotionDelta.NormalizeRotation();
			}
		}

	}
}

#if WITH_EDITOR

void UAnimNodeFrame::DrawNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes, UBOOL bShowWeight)
{
	DrawFrameBox(Canvas, SelectedNodes.ContainsItem(this));
	// draw comment
	// Draw comment text

	// Check there are some valid chars in string. If not - we can't select it! So we force it back to default.
	INT NumProperChars = 0;
	for(INT i=0; i<ObjComment.Len(); i++)
	{
		if(ObjComment[i] != ' ')
		{
			NumProperChars++;
		}
	}

	if(NumProperChars == 0)
	{
		ObjComment = TEXT("Comment");
	}

	const FLOAT OldZoom2D = FLinkedObjDrawUtils::GetUniformScaleFromMatrix(Canvas->GetTransform());
	UFont* FontToUse = FLinkedObjDrawUtils::GetFont();

	FTextSizingParameters Parameters(FontToUse, 1.f, 1.f);
	FLOAT& XL = Parameters.DrawXL, &YL = Parameters.DrawYL;

	UCanvas::CanvasStringSize( Parameters, *ObjComment );

	// We always draw comment-box text at normal size (don't scale it as we zoom in and out.)
	const INT x = appTrunc(NodePosX*OldZoom2D + 2);
	const INT y = appTrunc(NodePosY*OldZoom2D - YL - 2);


	// Viewport cull at a zoom of 1.0, because that's what we'll be drawing with.
	if ( FLinkedObjDrawUtils::AABBLiesWithinViewport( Canvas, NodePosX, NodePosY-YL-2, SizeX * OldZoom2D, YL ) )
	{
		Canvas->PushRelativeTransform(FScaleMatrix(1.0f / OldZoom2D));
		{
			const UBOOL bHitTesting = Canvas->IsHitTesting();

			DrawString(Canvas, x, y, *ObjComment, FontToUse, FColor(64,64,192) );

			// We only set the hit proxy for the area above the comment box with the height of the comment text
			if(bHitTesting)
			{
				Canvas->SetHitProxy(new HLinkedObjProxy(this));

				// account for the +2 when x was assigned
				DrawTile(Canvas, x - 2, y, SizeX * OldZoom2D, YL, 0.f, 0.f, 1.f, 1.f, FLinearColor(1.0f, 0.0f, 0.0f));

				Canvas->SetHitProxy(NULL);
			}
		}
		Canvas->PopTransform();
	}

	// Fill in base SequenceObject rendering info (used by bounding box calculation).
	DrawWidth = SizeX;
	DrawHeight = SizeY;
}

void UAnimNodeFrame::DrawFrameBox(FCanvas* Canvas, UBOOL bSelected)
{
	// Draw filled center if desired.
	if(bFilled)
	{
		// If texture, use it...
		if(FillMaterial)
		{
			// Tiling is every 64 pixels.
			if(bTileFill)
			{
				DrawTile(Canvas, NodePosX, NodePosY, SizeX, SizeY, 0.f, 0.f, (FLOAT)SizeX/64.f, (FLOAT)SizeY/64.f, FillMaterial->GetRenderProxy(0) );
			}
			else
			{
				DrawTile(Canvas, NodePosX, NodePosY, SizeX, SizeY, 0.f, 0.f, 1.f, 1.f, FillMaterial->GetRenderProxy(0) );
			}
		}
		else if(FillTexture)
		{
			if(bTileFill)
			{
				DrawTile(Canvas, NodePosX, NodePosY, SizeX, SizeY, 0.f, 0.f, (FLOAT)SizeX/64.f, (FLOAT)SizeY/64.f, FillColor, FillTexture->Resource );
			}
			else
			{
				DrawTile(Canvas, NodePosX, NodePosY, SizeX, SizeY, 0.f, 0.f, 1.f, 1.f, FillColor, FillTexture->Resource );
			}
		}
		// .. otherwise just use a solid color.
		else
		{
			DrawTile(Canvas, NodePosX, NodePosY, SizeX, SizeY, 0.f, 0.f, 1.f, 1.f, FillColor );
		}
	}

	// Draw frame
	const FColor FrameColor = bSelected ? FColor(255,255,0) : BorderColor;

	const INT MinDim = Min(SizeX, SizeY);
	const INT UseBorderWidth = Clamp( BorderWidth, 0, (MinDim/2)-3 );

	for(INT i=0; i<UseBorderWidth; i++)
	{
		DrawLine2D(Canvas, FVector2D(NodePosX,				NodePosY + i),			FVector2D(NodePosX + SizeX,		NodePosY + i),			FrameColor );
		DrawLine2D(Canvas, FVector2D(NodePosX + SizeX - i,	NodePosY),				FVector2D(NodePosX + SizeX - i,	NodePosY + SizeY),		FrameColor );
		DrawLine2D(Canvas, FVector2D(NodePosX + SizeX,		NodePosY + SizeY - i),	FVector2D(NodePosX,				NodePosY + SizeY - i),	FrameColor );
		DrawLine2D(Canvas, FVector2D(NodePosX + i,			NodePosY + SizeY),		FVector2D(NodePosX + i,			NodePosY - 1),			FrameColor );
	}

	// Draw little sizing triangle in bottom left.
	const INT HandleSize = 16;
	const FIntPoint A(NodePosX + SizeX,				NodePosY + SizeY);
	const FIntPoint B(NodePosX + SizeX,				NodePosY + SizeY - HandleSize);
	const FIntPoint C(NodePosX + SizeX - HandleSize,	NodePosY + SizeY);
	const BYTE TriangleAlpha = (bSelected) ? 255 : 32; // Make it more transparent if comment is not selected.

	const UBOOL bHitTesting = Canvas->IsHitTesting();

	if(bHitTesting)  Canvas->SetHitProxy( new HLinkedObjProxySpecial(this, 1) );
	DrawTriangle2D(Canvas, A, FVector2D(0,0), B, FVector2D(0,0), C, FVector2D(0,0), FColor(0,0,0,TriangleAlpha) );
	if(bHitTesting)  Canvas->SetHitProxy( NULL );
}
#endif // WITH_EDITOR