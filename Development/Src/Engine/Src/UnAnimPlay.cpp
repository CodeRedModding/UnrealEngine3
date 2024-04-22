/*=============================================================================
	UnAnimPlay.cpp: Skeletal mesh animation  - UAnimNodeSequence implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "AnimationEncodingFormat.h"

IMPLEMENT_CLASS(UAnimNodeSequence);
IMPLEMENT_CLASS(UAnimNodeSequenceBlendBase);
IMPLEMENT_CLASS(UAnimNodeSequenceBlendByAim);


UBOOL UAnimNodeSequence::GetCachedResults(FBoneAtomArray& OutAtoms, FBoneAtom& OutRootMotionDelta, INT& bOutHasRootMotion, FCurveKeyArray& OutCurveKeys, INT NumDesiredBones)
{
	check(SkelComponent);

	// See if cached array is the same size as the target array.
	// Don't check for CachedAtomsTag as if the animation is paused, we don't want to recache it.
	if( !bDisableCaching && CachedNumDesiredBones == NumDesiredBones && CachedBoneAtoms.Num() == OutAtoms.Num() )
	{
		OutAtoms = CachedBoneAtoms;
		OutCurveKeys += CachedCurveKeys;

		// Only use cached root motion if animation has been cached this frame.
		// We don't want a paused animation to send our mesh drifting away.
		if( NodeCachedAtomsTag == SkelComponent->CachedAtomsTag )
		{
			OutRootMotionDelta = CachedRootMotionDelta;
			bOutHasRootMotion = bCachedHasRootMotion;
		}
		else
		{
			OutRootMotionDelta.SetIdentity();
			bOutHasRootMotion = 0;
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/** 
 * Cache results always if bCacheAnimSequenceNodes == TRUE. (Optimization if animation is not frequently updated).
 * or if node has more than a single parent.
 */
UBOOL UAnimNodeSequence::ShouldSaveCachedResults()
{
	return ( !bPlaying || GetGlobalPlayRate() <= KINDA_SMALL_NUMBER 
		|| (SkelComponent->bPauseAnims && !bTickDuringPausedAnims) 
		|| (AnimSeq && AnimSeq->NumFrames < 2) 
		|| Super::ShouldSaveCachedResults() );
}

/** 
	Whether we should keep the cached result for the next frame or not 
	This is to avoid keeping cached result once it ticks. 
	It will release cache result if this returns FALSE
*/
UBOOL UAnimNodeSequence::ShouldKeepCachedResult()
{
	// keep cached result if playing and if pause
	return (!bPlaying ||  GetGlobalPlayRate() <= KINDA_SMALL_NUMBER 
		|| (SkelComponent->bPauseAnims && !bTickDuringPausedAnims)
		|| (AnimSeq && AnimSeq->NumFrames < 2));
}

void UAnimNodeSequence::ConditionalClearCachedData()
{
	CachedBoneAtoms.Empty();
	CachedCurveKeys.Empty();
	CachedNumDesiredBones = 0;
}

/**
 * Set custom animation root bone options.
 */
void UAnimNodeSequence::SetRootBoneAxisOption(BYTE AxisX, BYTE AxisY, BYTE AxisZ)
{
	RootBoneOption[0] = AxisX;
	RootBoneOption[1] = AxisY;
	RootBoneOption[2] = AxisZ;

	// Force an update on cached animation, because that affects root bone atom.
	ConditionalClearCachedData();
}

/**
 * Set custom animation root bone options.
 */
void UAnimNodeSequence::SetRootBoneRotationOption(BYTE AxisX, BYTE AxisY, BYTE AxisZ)
{
	RootRotationOption[0] = AxisX; // Roll
	RootRotationOption[1] = AxisY; // Pitch
	RootRotationOption[2] = AxisZ; // Yaw

	// Force an update on cached animation, because that affects root bone atom.
	ConditionalClearCachedData();
}

FLOAT UAnimNodeSequence::GetSliderPosition(INT SliderIndex, INT ValueIndex)
{
	check(0 == SliderIndex && 0 == ValueIndex);

	if( AnimSeq && AnimSeq->SequenceLength > 0.f )
	{
		return (CurrentTime / AnimSeq->SequenceLength);
	}
	return 0.f;
}

void UAnimNodeSequence::HandleSliderMove(INT SliderIndex, INT ValueIndex, FLOAT NewSliderValue)
{
	check(0 == SliderIndex && 0 == ValueIndex);

	if( !AnimSeq || AnimSeq->SequenceLength == 0.f )
	{
		return;
	}

	const FLOAT NewTime = NewSliderValue * AnimSeq->SequenceLength;
	SetPosition(NewTime, FALSE);
}

FString UAnimNodeSequence::GetSliderDrawValue(INT SliderIndex)
{
	check(0 == SliderIndex);

	if( !AnimSeq || AnimSeq->SequenceLength == 0.f )
	{
		return FString::Printf(TEXT("N/A"));
	}

	return FString::Printf(TEXT("Pos: %3.2f%%, Time: %3.2fs"), (CurrentTime/AnimSeq->SequenceLength)*100.f, CurrentTime);
}


void UAnimNodeSequence::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SetAnim( AnimSeqName );

	if(SkelComponent && SkelComponent->IsAttached())
	{
		SkelComponent->UpdateSkelPose();
		SkelComponent->ConditionalUpdateTransform();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UAnimNodeSequence::BeginDestroy()
{
	// make sure we're not still trying to play a camera anim
	if (ActiveCameraAnimInstance)
	{
		StopCameraAnim();
	}

	Super::BeginDestroy();
}

void UAnimNodeSequence::InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent )
{
	START_INITANIM_TIMER
	{
		EXCLUDE_PARENT_TIME
		Super::InitAnim( meshComp, Parent );
	}

	// Clear reference to Animation data instantly, but keep AnimSeqName.
	// DeferredInitAnim will re-set references if needed
	FName OldName = AnimSeqName;
	SetAnim(NAME_None);
	// Make sure cached data is cleared too.
	CachedNumDesiredBones = 0;
	AnimSeqName = OldName;

	if (ActiveCameraAnimInstance != NULL)
	{
		StopCameraAnim();
	}
}

void UAnimNodeSequence::DeferredInitAnim()
{
	Super::DeferredInitAnim();

	SetAnim( AnimSeqName );
}


/** AnimSets have been updated, update all animations */
void UAnimNodeSequence::AnimSetsUpdated()
{
	// Clear reference to Animation data instantly, but keep AnimSeqName.
	// DeferredInitAnim will re-set references if needed
	FName OldName = AnimSeqName;
	SetAnim(NAME_None);
	// Make sure cached data is cleared too.
	CachedNumDesiredBones = 0;
	AnimSeqName = OldName;
}

/** 
 * Set a new animation by name.
 * This will find the UAnimationSequence by name, from the list of AnimSets specified in the SkeletalMeshComponent and cache it
 * Will also store a pointer to the anim track <-> 
 */
void UAnimNodeSequence::SetAnim(FName InSequenceName)
{
	if (0)
	{
		debugf(TEXT("** SetAnim %s, on %s"), *InSequenceName.ToString(), *GetFName().ToString());
	}

	// Abort if we are in the process of firing notifies, as this can cause a crash.
	//
	//	Unless the animation is the same. This can happen if a new skeletal mesh is set, then it forces all
	// animations to be recached. If the animation is the same, then it's safe to update it.
	// Note that it can be set to NAME_None if the AnimSet has been removed as well.
// jmarshall - we won't be changing the skeleton on animation updates.
/*
	if( bIsIssuingNotifies && AnimSeqName != InSequenceName )
	{
		debugf( TEXT("UAnimNodeSequence::SetAnim : Not safe to call SetAnim from inside a Notify. AnimName: %s, Owner: %s"), *InSequenceName.ToString(), *SkelComponent->GetOwner()->GetName() );
		return;
	}
*/
// jmarshall end
	// Track AnimSeq changes, so we can update the metadata classes only when really needed.
	UAnimSequence* OldAnimSeq = AnimSeq;

	AnimSeqName		= InSequenceName;
	AnimSeq			= NULL;
	AnimLinkupIndex = INDEX_NONE;

	bool bSkipAnimUpdate = FALSE;
	if( InSequenceName == NAME_None || !SkelComponent || !SkelComponent->SkeletalMesh )
	{
		// Skip update, we don't have everything we need.
		bSkipAnimUpdate = TRUE;
	}

	if( !bSkipAnimUpdate )
	{
		AnimSeq = SkelComponent->FindAnimSequence(AnimSeqName);
		if( AnimSeq != NULL )
		{
			UAnimSet* AnimSet = AnimSeq->GetAnimSet();
			AnimLinkupIndex = AnimSet->GetMeshLinkupIndex( SkelComponent->SkeletalMesh );
			
			check(AnimLinkupIndex != INDEX_NONE);
			check(AnimLinkupIndex < AnimSet->LinkupCache.Num());

			FAnimSetMeshLinkup* AnimLinkup = &AnimSet->LinkupCache(AnimLinkupIndex);

			check( AnimLinkup->BoneToTrackTable.Num() == SkelComponent->SkeletalMesh->RefSkeleton.Num() );

#if !FINAL_RELEASE
			// If we're tracing animation usage, set bHasBeenUsed to be TRUE
			extern UBOOL GShouldTraceAnimationUsage;
			if ( GShouldTraceAnimationUsage )
			{
				if ( AnimSeq->bHasBeenUsed == FALSE )
				{
					AnimSeq->bHasBeenUsed = TRUE;
				}
			}
#endif
		}
		else if( !bDisableWarningWhenAnimNotFound && !SkelComponent->bDisableWarningWhenAnimNotFound )
		{
			debugf( NAME_DevAnim, TEXT("%s - Failed to find animsequence '%s' on SkeletalMeshComponent: %s whose owner is: %s using mesh: %s" ),
				   *GetName(),
				   *InSequenceName.ToString(),
				   *SkelComponent->GetName(),
				   *SkelComponent->GetOwner()->GetName(),
				   *SkelComponent->SkeletalMesh->GetPathName()
				   );
		}
	}

	// If we have changed AnimSeq, update MetaData
	if( OldAnimSeq != AnimSeq )
	{
		// Animation updated, clear cached data
		ConditionalClearCachedData();

		// AnimMetadata AnimUnSet
		if( OldAnimSeq )
		{
			for(INT Index=0; Index<OldAnimSeq->MetaData.Num(); Index++)
			{
				UAnimMetaData* AnimMetadata = OldAnimSeq->MetaData(Index);
				if( AnimMetadata )
				{
					AnimMetadata->AnimUnSet(this);
				}
			}
		}

		// AnimMetadata AnimSet
		if( AnimSeq )
		{
			for(INT Index=0; Index<AnimSeq->MetaData.Num(); Index++)
			{
				UAnimMetaData* AnimMetadata = AnimSeq->MetaData(Index);
				if( AnimMetadata )
				{
					AnimMetadata->AnimSet(this);
				}
			}
		}
	}
}

void UAnimNodeSequence::TickAnim(FLOAT DeltaSeconds)
{

	// If this node is part of a group, animation is going to be advanced in a second pass, once all weights have been calculated in the tree.
	if( SynchGroupName == NAME_None )
	{
		// Keep track of PreviousTime before any update. This is used by Root Motion.
		PreviousTime = CurrentTime;

		// Can only do something if we are currently playing a valid AnimSequence
		if( bPlaying && AnimSeq )
		{
			// Time to move forwards by - DeltaSeconds scaled by various factors.
			const FLOAT MoveDelta = Rate * AnimSeq->RateScale * SkelComponent->GlobalAnimRateScale * DeltaSeconds;
			AdvanceBy(MoveDelta, DeltaSeconds, (SkelComponent->bUseRawData) ? FALSE : TRUE);
		}
	}
	else if( AnimSeq )
	{
		UAnimTree* RootNode = Cast<UAnimTree>(SkelComponent->Animations);
		if( RootNode )
		{
			INT const GroupIndex = RootNode->GetGroupIndex(SynchGroupName);
			if( GroupIndex != INDEX_NONE )
			{
				FAnimGroup& AnimGroup = RootNode->AnimGroups(GroupIndex);
				
				// Update SyncMaster node
				if( bSynchronize && !bForceAlwaysSlave &&
					(!AnimGroup.SynchMaster || (AnimGroup.SynchMaster->NodeTotalWeight < NodeTotalWeight)) )
				{
					UBOOL bAlreadyHadMasterNode = (AnimGroup.SynchMaster != NULL);
					AnimGroup.SynchMaster = this;

					// If this node just became relevant, then it hasn't been sync'd yet. 
					// So make sure it is sync'd with the groups' position now.
					if( bAlreadyHadMasterNode && bJustBecameRelevant )
					{
						FLOAT const NewTime = AnimGroup.SynchMaster->FindGroupPosition(AnimGroup.SynchPctPosition);
						SetPosition(NewTime, FALSE);
					}
				}

				// Update NotifyMaster node
				if( !bNoNotifies &&
					(!AnimGroup.NotifyMaster || (AnimGroup.NotifyMaster->NodeTotalWeight < NodeTotalWeight)) )
				{
					AnimGroup.NotifyMaster = this;
				}

				// Add this node to our Update Queue for this frame
				AnimGroup.SeqNodes.AddItem(this);
			}
		}
	}

	// AnimMetadata Update
	if( AnimSeq )
	{
		for(INT Index=0; Index<AnimSeq->MetaData.Num(); Index++)
		{
			UAnimMetaData* AnimMetadata = AnimSeq->MetaData(Index);
			if( AnimMetadata )
			{
				AnimMetadata->TickMetaData(this);
			}
		}
	}

	if (ActiveCameraAnimInstance != NULL)
	{
		// check and see if we still have a camera associated with our skeletal mesh
		ACamera* const Cam = GetPlayerCamera();
		if ( Cam && bPlaying && bRelevant )
		{
			// associated camera anims are weighted the same as this node
			ActiveCameraAnimInstance->ApplyTransientScaling(NodeTotalWeight);
		}
		else
		{
			StopCameraAnim();
		}
	}
	else if ( CameraAnim && bPlaying && bLoopCameraAnim && bRelevant )
	{
		// looping camera anims should always be playing, so make sure it's on whenever 
		// note: nonlooping camera anims will play when the real anim starts
		StartCameraAnim();
	}
}

#if !FINAL_RELEASE
void UAnimNodeSequence::UpdateAnimationUsage( FLOAT DeltaSeconds )
{
	extern UBOOL GShouldTraceAnimationUsage;
	if ( GShouldTraceAnimationUsage )
	{
		if ( bRelevant && SkelComponent && SkelComponent->bRecentlyRendered && AnimSeq )
		{
			AnimSeq->UseScore += NodeTotalWeight*DeltaSeconds*Min(SkelComponent->MaxDistanceFactor, 1.0f);
//			debugf(TEXT("%s: %s(%s) - Score(%0.6f), DisplayFactor(%0.2f)"), *SkelComponent->GetOwner()->GetName(), *AnimSeq->GetAnimSet()->GetName(), *AnimSeq->SequenceName.GetNameString(), AnimSeq->UseScore, SkelComponent->MaxDistanceFactor);
		}
	}
}
#endif

/** Advance animation time. Take care of issuing notifies, looping and so on */
void UAnimNodeSequence::AdvanceBy(FLOAT MoveDelta, FLOAT DeltaSeconds, UBOOL bFireNotifies)
{
	if( !AnimSeq || MoveDelta == 0.f || DeltaSeconds == 0.f )
	{
		return;
	}

#if !FINAL_RELEASE
	// update animation usage
	UpdateAnimationUsage( DeltaSeconds );
#endif

	// If animation has only 1 frame, then no need to clear it.
	if( AnimSeq->NumFrames > 1 )
	{
		// Animation updated, clear cached data.
		ConditionalClearCachedData();
	}
	
	// Before we actually advance the time, issue any notifies (if desired).
	if( !bNoNotifies && bFireNotifies && MoveDelta != 0.f )
	{
		// Can fire notifies if part of a synchronization group and node is relevant.
		// then bFireNotifies tells us if we should actually fire notifies or not.
		const UBOOL bCanFireNotifyGroup		= SynchGroupName != NAME_None && bRelevant;
	
		// If not part of a group then we check for the weight threshold.
		const UBOOL	bCanFireNotifyNoGroup	= (NodeTotalWeight >= NotifyWeightThreshold);

		if( bCanFireNotifyGroup || bCanFireNotifyNoGroup )
		{
			IssueNotifies(MoveDelta);

			// If a notification cleared the animation, stop here, don't crash.
			if( !AnimSeq )
			{
				return;
			}
		}
	}

	// Then update internal time.
	CurrentTime	+= MoveDelta;

	// so we need to check to see what the actual EndTime of the Anim is (e.g. if we passed one in)
	const FLOAT RealEndTime = EndTime > 0.0f ? EndTime : AnimSeq->SequenceLength;

	// See if we passed the end of the animation.
	if( CurrentTime > RealEndTime )
	{
		// Find Rate of this movement.
		const FLOAT MoveRate = MoveDelta / DeltaSeconds;

		// figure out by how much we've reached beyond end of animation.
		// This is useful for transitions. It is made independent from play rate
		// So we can transition properly between animations of different play rates
		const FLOAT ExcessTime = (CurrentTime - RealEndTime) / MoveRate;
		const FLOAT PlayedTime = DeltaSeconds - ExcessTime;

		OnAnimComplete( PlayedTime, ExcessTime );
		// If we are looping, wrap over.
		if( bLooping )
		{
			CurrentTime	= appFmod(CurrentTime, RealEndTime);
		}
		// If not, snap time to end of sequence and stop playing.
		else 
		{
			CurrentTime = RealEndTime;
			StopAnim();

			// Notify that animation finished playing
			OnAnimEnd( PlayedTime, ExcessTime);
		}
	}
	// See if we passed before the beginning of the animation
	else if( CurrentTime < 0.f )
	{
		// If we are looping, wrap over.
		if( bLooping )
		{
			CurrentTime	= appFmod(CurrentTime, RealEndTime);
			if( CurrentTime < 0.f )
			{
				CurrentTime += RealEndTime;
			}
		}
		// If not, snap time to beginning of sequence and stop playing.
		else 
		{
			// Find Rate of this movement.
			const FLOAT MoveRate = MoveDelta / DeltaSeconds;

			// figure out by how much we've reached beyond beginning of animation.
			// This is useful for transitions.
			const FLOAT ExcessTime = CurrentTime / Abs(MoveRate);
			CurrentTime = 0.f;
			StopAnim();

			// Notify that animation finished playing
			OnAnimEnd(DeltaSeconds + ExcessTime, ExcessTime);
		}
	}
}

/** 
 * notification that current animation finished playing. 
 * @param	PlayedTime	Time in seconds of animation played. (play rate independent).
 * @param	ExcessTime	Time in seconds beyond end of animation. (play rate independent).
 */
void UAnimNodeSequence::OnAnimEnd(FLOAT PlayedTime, FLOAT ExcessTime)
{
	// When we hit the end and stop, issue notifies to parent AnimNodeBlendBase
	for(INT i=0; i<ParentNodes.Num(); i++)
	{
		if (ParentNodes(i)->NodeEndEventTick!=SkelComponent->TickTag)
		{
			ParentNodes(i)->OnChildAnimEnd(this, PlayedTime, ExcessTime); 
			ParentNodes(i)->NodeEndEventTick=SkelComponent->TickTag;
		}
	}

	if( bForceRefposeWhenNotPlaying && !SkelComponent->bForceRefpose)
	{
		SkelComponent->SetForceRefPose(TRUE);
	}

	if( bCauseActorAnimEnd && SkelComponent->GetOwner() )
	{
		if( GIsEditor && !GIsPlayInEditorWorld )
		{
			SkelComponent->GetOwner()->OnEditorAnimEnd( this, PlayedTime, ExcessTime );
		}		

		SkelComponent->GetOwner()->eventOnAnimEnd(this, PlayedTime, ExcessTime);
	}
}


void UAnimNodeSequence::GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	START_GETBONEATOM_TIMER

	if( GetCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num()) )
	{
		return;
	}

	GetAnimationPose(AnimSeq, AnimLinkupIndex, Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
	SaveCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num());
}

void UAnimNodeSequence::GetAnimationPose(UAnimSequence* InAnimSeq, INT& InAnimLinkupIndex, FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	SCOPE_CYCLE_COUNTER(STAT_GetAnimationPose);

	check(SkelComponent);
	check(SkelComponent->SkeletalMesh);

	// Set root motion delta to identity, so it's always initialized, even when not extracted.
	RootMotionDelta.SetIdentity();
	bHasRootMotion	= 0;

	if( !InAnimSeq || InAnimLinkupIndex == INDEX_NONE )
	{
#if 0
		debugf(TEXT("UAnimNodeSequence::GetAnimationPose - %s - No animation data!"), *GetFName());
#endif
		FillWithRefPose(Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
		return;
	}

	// Get the reference skeleton
	TArray<FMeshBone>& RefSkel = SkelComponent->SkeletalMesh->RefSkeleton;
	const INT NumBones = RefSkel.Num();
	check(NumBones == Atoms.Num());

	UAnimSet* AnimSet = InAnimSeq->GetAnimSet();
	check(InAnimLinkupIndex < AnimSet->LinkupCache.Num());

	FAnimSetMeshLinkup* AnimLinkup = &AnimSet->LinkupCache(InAnimLinkupIndex);

	// @remove me, trying to figure out why this is failing
	if( AnimLinkup->BoneToTrackTable.Num() != NumBones )
	{
		debugf(TEXT("AnimLinkup->BoneToTrackTable.Num() != NumBones, BoneToTrackTable.Num(): %d, NumBones: %d, AnimName: %s, Owner: %s, Mesh: %s"), AnimLinkup->BoneToTrackTable.Num(), NumBones, *InAnimSeq->SequenceName.ToString(), *SkelComponent->GetOwner()->GetName(), *SkelComponent->SkeletalMesh->GetName());
	}
	check(AnimLinkup->BoneToTrackTable.Num() == NumBones);

	// bAnimRotationOnly settings.
	const UBOOL bAnimRotationOnly = (SkelComponent->AnimRotationOnly == EARO_AnimSet) ? AnimSet->bAnimRotationOnly : (SkelComponent->AnimRotationOnly == EARO_ForceEnabled ? TRUE : FALSE); 
	
	// Are we doing root motion for this node?
	const UBOOL bDoRootTranslation	= (RootBoneOption[0] != RBA_Default) || (RootBoneOption[1] != RBA_Default) || (RootBoneOption[2] != RBA_Default);
	const UBOOL bDoRootRotation		= (RootRotationOption[0] != RRO_Default) || (RootRotationOption[1] != RRO_Default) || (RootRotationOption[2] != RRO_Default);
	const UBOOL	bDoingRootMotion	= bDoRootTranslation || bDoRootRotation;

	// Is the skeletal mesh component requesting that raw animation data be used?
	UBOOL bUseRawData = SkelComponent->bUseRawData;
#if WITH_EDITORONLY_DATA
	bUseRawData |= GIsEditor && (AnimSet->bAnimRotationOnly != InAnimSeq->bWasCompressedWithoutTranslations);
#endif // WITH_EDITORONLY_DATA

	// Handle Last Frame to First Frame interpolation when looping.
	// It is however disabled for the Root Bone.
	// And the 'bNoLoopingInterpolation' flag on the animation can also disable that behavior.
	const UBOOL bLoopingInterpolation = bLooping && !InAnimSeq->bNoLoopingInterpolation;

	// Never process bone indices higher than our static array size to avoid crashes for malformed assets
	const INT DesiredBoneCount = Min( MAX_BONES, DesiredBones.Num() );

#if (USE_ANIMATION_CODEC_BATCH_SOLVER)
	if (!bUseRawData && DesiredBones.Num()>0 && InAnimSeq->CompressedTrackOffsets.Num() > 0)
	{
		// Note: These are static to avoid allocation. This makes it non-threadsafe.
		static BoneTrackArray RotationPairs;
		static BoneTrackArray TranslationPairs;
		check( IsInGameThread() );

		// build a list of desired bones
		RotationPairs.Empty();
		TranslationPairs.Empty();

		UBOOL NonRootEncountered = FALSE;
		for( INT DesiredIndex=0; DesiredIndex < DesiredBoneCount; DesiredIndex++ )
		{
			// Find which track in the sequence we look in for this bones data
			const INT BoneIndex = DesiredBones(DesiredIndex);
			checkSlow( BoneIndex < MAX_BONES );

			const INT	TrackIndex = AnimLinkup->BoneToTrackTable(BoneIndex);

			if( TrackIndex == INDEX_NONE )
			{
				// use the default rotation and translation for this bone
				if( InAnimSeq->bIsAdditive )
				{
					Atoms(BoneIndex).SetIdentity();
				}
				else
				{
					Atoms(BoneIndex).SetComponents(RefSkel(BoneIndex).BonePos.Orientation, RefSkel(BoneIndex).BonePos.Position);
				}
			}
			else if (BoneIndex > 0)
			{
				NonRootEncountered = TRUE;
				RotationPairs.AddItem(BoneTrackPair(BoneIndex, TrackIndex));

				// If doing 'rotation only' case, use ref pose for all non-root bones that are not in the BoneUseAnimTranslation array.
				if(	!InAnimSeq->bIsAdditive && ((bAnimRotationOnly && !AnimSet->BoneUseAnimTranslation(TrackIndex)) || AnimSet->ForceUseMeshTranslation(TrackIndex)) )
				{
					// use the default translation for this bone
					Atoms(BoneIndex).SetComponents(FQuat::Identity, RefSkel(BoneIndex).BonePos.Position);  //@TODO: mnoland - Altered behavior to avoid relying on uninitialized memory
				}
				else
				{
					Atoms(BoneIndex).SetIdentity(); //@TODO: mnoland - Altered behavior to avoid relying on uninitialized memory
					TranslationPairs.AddItem(BoneTrackPair(BoneIndex, TrackIndex));
				}
			}
			else
			{
				// Otherwise read it from the sequence.
				FBoneAtom& RootAtom = Atoms(0);

				AnimationFormat_GetBoneAtom(	
					RootAtom,
					*InAnimSeq,
					TrackIndex,
					CurrentTime,
					bLoopingInterpolation && !bDoingRootMotion);

				InAnimSeq->GetCurveData(CurrentTime, bLooping, CurveKeys);

#if WITH_EDITORONLY_DATA
#if !FINAL_RELEASE
				if ( GIsEditor )
				{
					LastUpdatedAnimMorphKeys = CurveKeys;
				}
#endif
#endif // WITH_EDITORONLY_DATA
				// If doing root motion for this animation, extract it!
				if( bDoingRootMotion )
				{
					ExtractRootMotion(InAnimSeq, TrackIndex, RootAtom, RootMotionDelta, bHasRootMotion);
				}

				// If desired, zero out Root Bone rotation.
				if( bZeroRootRotation )
				{
					RootAtom.SetRotation(FQuat::Identity);
				}

				// If desired, zero out Root Bone translation.
				if( bZeroRootTranslation )
				{
					RootAtom.SetTranslation(FVector::ZeroVector);
				}
			}
		}

		// get the remaining bone atoms
		if (NonRootEncountered)
		{
			AnimationFormat_GetAnimationPose(	
				Atoms, 
				RotationPairs,
				TranslationPairs,
				*InAnimSeq,
				CurrentTime,
				bLoopingInterpolation);
		}
	}
	else
	{
#endif
	// For each desired bone...
	for( INT i=0; i < DesiredBoneCount; i++ )
	{
		const INT BoneIndex = DesiredBones(i);
		checkSlow( BoneIndex < MAX_BONES );

		// Find which track in the sequence we look in for this bones data
		const INT	TrackIndex = AnimLinkup->BoneToTrackTable(BoneIndex);

		// If there is no track for this bone, we just use the reference pose.
		if( TrackIndex == INDEX_NONE )
		{
			if( InAnimSeq->bIsAdditive )
			{
				Atoms(BoneIndex).SetIdentity();
			}
			else
			{
				Atoms(BoneIndex).SetComponents(RefSkel(BoneIndex).BonePos.Orientation, RefSkel(BoneIndex).BonePos.Position);
			}
		}
		else 
		{
			// Non Root Bone
			if( BoneIndex > 0 )
			{
				// Otherwise read it from the sequence.
				InAnimSeq->GetBoneAtom(Atoms(BoneIndex), TrackIndex, CurrentTime, bLoopingInterpolation, bUseRawData);

				// If doing 'rotation only' case, use ref pose for all non-root bones that are not in the BoneUseAnimTranslation array.
				if(	!InAnimSeq->bIsAdditive && ((bAnimRotationOnly && !AnimSet->BoneUseAnimTranslation(TrackIndex)) || AnimSet->ForceUseMeshTranslation(TrackIndex)) )
				{
 					Atoms(BoneIndex).SetTranslation(RefSkel(BoneIndex).BonePos.Position);
				}

				// Apply quaternion fix for ActorX-exported quaternions.
				Atoms(BoneIndex).FlipSignOfRotationW();
			}
			// Root Bone
			else
			{
				// Otherwise read it from the sequence.
				InAnimSeq->GetBoneAtom(Atoms(BoneIndex), TrackIndex, CurrentTime, bLoopingInterpolation && !bDoingRootMotion, bUseRawData, &CurveKeys);

#if WITH_EDITORONLY_DATA
#if !FINAL_RELEASE
				if ( GIsEditor )
				{
					LastUpdatedAnimMorphKeys = CurveKeys;
				}
#endif
#endif // WITH_EDITORONLY_DATA
				// If doing root motion for this animation, extract it!
				if( bDoingRootMotion )
				{
					ExtractRootMotion(InAnimSeq, TrackIndex, Atoms(0), RootMotionDelta, bHasRootMotion);
				}

				// If desired, zero out Root Bone rotation.
				if( bZeroRootRotation )
				{
					Atoms(0).SetRotation(FQuat::Identity);
				}

				// If desired, zero out Root Bone translation.
				if( bZeroRootTranslation )
				{
					Atoms(0).SetTranslation(FVector::ZeroVector);
				}
			}
		}
	}
#if (USE_ANIMATION_CODEC_BATCH_SOLVER)
	}
#endif

	// Check that all bone atoms coming from animation are normalized
#if !FINAL_RELEASE && !CONSOLE
	for( INT i=0; i<DesiredBoneCount; i++ )
	{
		const INT	BoneIndex = DesiredBones(i);
		check( Atoms(BoneIndex).IsRotationNormalized() );
	}
#endif

// In Editor, show additive animation added to its ref pose
// Made into a separate branch, so we don't add a branch test for each bone when we only care about this in the editor.
#if 1 && !CONSOLE
	if( bEditorOnlyAddRefPoseToAdditiveAnimation && InAnimSeq->bIsAdditive )
	{
		for( INT i=0; i<DesiredBoneCount; i++ )
		{
			const INT	BoneIndex = DesiredBones(i);
			// Find which track in the sequence we look in for this bones data
			const INT	TrackIndex = AnimLinkup->BoneToTrackTable(BoneIndex);

			if( TrackIndex == INDEX_NONE )
			{
				Atoms(BoneIndex).SetComponents(RefSkel(BoneIndex).BonePos.Orientation, RefSkel(BoneIndex).BonePos.Position);				
			}
			else
			{
				FBoneAtom	RefBoneAtom;
				InAnimSeq->GetAdditiveBasePoseBoneAtom(RefBoneAtom, TrackIndex, CurrentTime, bLoopingInterpolation);

				// If doing 'rotation only' case, use ref pose for all non-root bones that are not in the BoneUseAnimTranslation array.
				if(	BoneIndex > 0 && ((bAnimRotationOnly && !AnimSet->BoneUseAnimTranslation(TrackIndex)) || AnimSet->ForceUseMeshTranslation(TrackIndex)) )
				{
					Atoms(BoneIndex).AddToTranslation(RefSkel(BoneIndex).BonePos.Position);
				}
				else
				{
					Atoms(BoneIndex).AddToTranslation(RefBoneAtom.GetTranslation());
				}

				// Apply quaternion fix for ActorX-exported quaternions.
				FQuat RefRot = RefBoneAtom.GetRotation();
				if( BoneIndex > 0 )
				{
					RefRot.W *= -1.0f;
				}

				// Add ref pose relative animation to base animation, only if rotation is significant.
				if( Square(Atoms(BoneIndex).GetRotation().W) < 1.f - DELTA * DELTA )
				{
					Atoms(BoneIndex).ConcatenateRotation(RefRot);
					Atoms(BoneIndex).NormalizeRotation();
				}
				else
				{
					Atoms(BoneIndex).SetRotation(RefRot);
				}
			}
		}
	}
#endif

}

/**
 *  Extract root motion for animation using a specified start and end time
 */
void UAnimNodeSequence::ExtractRootMotionUsingSpecifiedTimespan (UAnimSequence* InAnimSeq, const INT &TrackIndex, FBoneAtom& CurrentFrameAtom, FBoneAtom& DeltaMotionAtom, INT& bHasRootMotion, FLOAT StartTime, FLOAT EndTime) const
{
	// SkeletalMesh has a transformation that is applied between the component and the actor, 
	// instead of being between mesh and component. 
	// So we have to take that into account when doing operations happening in component space (such as per Axis masking/locking).
	const FMatrix MeshToCompTM = FRotationMatrix(SkelComponent->SkeletalMesh->RotOrigin);
	// Inverse transform, from component space to mesh space.
	const FMatrix CompToMeshTM = MeshToCompTM.Inverse();

	// Get the exact translation of the root bone on the first frame of the animation
	FBoneAtom FirstFrameAtom;
	// technically you don't need to get curve keys here, so giving dummy variable 
	InAnimSeq->GetBoneAtom(FirstFrameAtom, TrackIndex, 0.f, FALSE, SkelComponent->bUseRawData);

	// Do we need to extract root motion?
	const UBOOL bExtractRootTranslation	= (RootBoneOption[0] == RBA_Translate) || (RootBoneOption[1] == RBA_Translate) || (RootBoneOption[2] == RBA_Translate);
	const UBOOL bExtractRootRotation	= (RootRotationOption[0] == RRO_Extract) || (RootRotationOption[1] == RRO_Extract) || (RootRotationOption[2] == RRO_Extract);
	const UBOOL	bExtractRootMotion		= bExtractRootTranslation || bExtractRootRotation;

	// Calculate bone motion
	if( bExtractRootMotion )
	{
		// We are extracting root motion, so set the flag to TRUE
		bHasRootMotion	= 1;		

		/** 
		 * If FromTime == ToTime, then we can't give a root delta for this frame.
		 * To avoid delaying it to next frame, because physics may need it right away,
		 * see if we can make up one by simulating forward.
		 */
		if( StartTime == EndTime )
		{
			// Only try to make up movement if animation is allowed to play
			if( bPlaying && InAnimSeq )
			{
				// See how much time would have passed on this frame
				const FLOAT DeltaTime = Rate * InAnimSeq->RateScale * SkelComponent->GlobalAnimRateScale * GWorld->GetDeltaSeconds();

				// If we can push back FromTime, then do so.
				if( StartTime > DeltaTime )
				{
					StartTime -= DeltaTime;
				}
				else
				{
					// otherwise, advance in time, to predict the movement
					EndTime += DeltaTime;

					// See if we passed the end of the animation.
					if( EndTime > InAnimSeq->SequenceLength )
					{
						// If we are looping, wrap over. If not, snap time to end of sequence.
						EndTime = bLooping ? appFmod(EndTime, InAnimSeq->SequenceLength) : InAnimSeq->SequenceLength;
					}
				}
			}
			else
			{
				// If animation is done playing we're not extracting root motion anymore.
				bHasRootMotion = 0;
			}
		}

		// If time has passed, compute root delta movement
		if( StartTime != EndTime )
		{
			UBOOL bReversePlaying = ((Rate * InAnimSeq->RateScale * SkelComponent->GlobalAnimRateScale) < 0.f);

			// Get Root Bone Position of start of movement
			FBoneAtom StartAtom;
			if( StartTime != CurrentTime )
			{
				InAnimSeq->GetBoneAtom(StartAtom, TrackIndex, StartTime, FALSE, SkelComponent->bUseRawData);
			}
			else
			{
				StartAtom = CurrentFrameAtom;
			}

			// Get Root Bone Position of end of movement
			FBoneAtom EndAtom;
			if( EndTime != CurrentTime )
			{
				InAnimSeq->GetBoneAtom(EndAtom, TrackIndex, EndTime, FALSE, SkelComponent->bUseRawData);
			}
			else
			{
				EndAtom = CurrentFrameAtom;
			}

			// Get position on last frame if we extract translation and/or rotation
			// if looping, we need last frame
			FBoneAtom LastFrameAtom;
			if( bLooping && (bExtractRootTranslation || bExtractRootRotation) )
			{
				// Get the exact root position of the root bone on the last frame of the animation
				InAnimSeq->GetBoneAtom(LastFrameAtom, TrackIndex, InAnimSeq->SequenceLength, FALSE, SkelComponent->bUseRawData);
			}

			// We don't support scale
			DeltaMotionAtom.SetScale(0.f);

			// If extracting root translation, filter out any axis
			if( bExtractRootTranslation )
			{
				// Handle case if animation looped
				if( bLooping )
				{
					// if not backward, and start time > end time, looped
					if (!bReversePlaying && StartTime > EndTime)
					{
						// Handle proper translation wrapping. We don't want the mesh to translate back to origin. So split that in 2 moves.
						DeltaMotionAtom.SetTranslation((LastFrameAtom.GetTranslation() - StartAtom.GetTranslation()) + (EndAtom.GetTranslation() - FirstFrameAtom.GetTranslation()));
					}
					// it's backward, and starttime is smaller than end time, looped
					else if (bReversePlaying && StartTime < EndTime)
					{
						// Handle proper translation wrapping. We don't want the mesh to translate back to origin. So split that in 2 moves.
						DeltaMotionAtom.SetTranslation((EndAtom.GetTranslation() - LastFrameAtom.GetTranslation()) + (FirstFrameAtom.GetTranslation() - StartAtom.GetTranslation()));

					}
					else
					{
						// Delta motion of the root bone in mesh space
						DeltaMotionAtom.SetTranslation(EndAtom.GetTranslation() - StartAtom.GetTranslation());
					}
				}
				else
				{
					// Delta motion of the root bone in mesh space
					DeltaMotionAtom.SetTranslation(EndAtom.GetTranslation() - StartAtom.GetTranslation());
				}

				// Only do that if an axis needs to be filtered out.
				if( RootBoneOption[0] != RBA_Translate || RootBoneOption[1] != RBA_Translate || RootBoneOption[2] != RBA_Translate )
				{
					// Convert Delta translation from mesh space to component space
					// We do this for axis filtering
					FVector CompDeltaTranslation = MeshToCompTM.TransformNormal(DeltaMotionAtom.GetTranslation());

					// Filter out any of the X, Y, Z channels in mesh space
					if( RootBoneOption[0] != RBA_Translate )
					{
						CompDeltaTranslation.X = 0.f;
					}
					if( RootBoneOption[1] != RBA_Translate )
					{
						CompDeltaTranslation.Y = 0.f;
					}
					if( RootBoneOption[2] != RBA_Translate )
					{
						CompDeltaTranslation.Z = 0.f;
					}

					// Convert back to mesh space.
					DeltaMotionAtom.SetTranslation(MeshToCompTM.InverseTransformNormal(CompDeltaTranslation));
				}
#if 0
				debugf(TEXT("%3.2f [%s] [%s] Extracted Root Motion Trans: %3.3f, Vect: %s, StartTime: %3.3f, EndTime: %3.3f, Weight : %0.3f"), GWorld->GetTimeSeconds(), *SkelComponent->GetOwner()->GetName(), *AnimSeqName.ToString(), DeltaMotionAtom.GetTranslation().Size(), *DeltaMotionAtom.GetTranslation().ToString(), StartTime, EndTime, NodeTotalWeight);
#endif
			}
			else
			{
				// Otherwise, don't extract any translation
				DeltaMotionAtom.SetTranslation(FVector::ZeroVector);
			}

			// If extracting root translation, filter out any axis
			if( bExtractRootRotation )
			{
				// Delta rotation
				// Handle case if animation looped
				if( bLooping )
				{
					if ( !bReversePlaying && StartTime > EndTime )
					{
						// Handle proper Rotation wrapping. We don't want the mesh to rotate back to origin. So split that in 2 turns.
						DeltaMotionAtom.SetRotation((LastFrameAtom.GetRotation() * (-StartAtom.GetRotation())) * (EndAtom.GetRotation() * (-FirstFrameAtom.GetRotation())));
					}
					// it's backward, and starttime is smaller than end time, looped
					else if (bReversePlaying && StartTime < EndTime)
					{
						// Handle proper Rotation wrapping. We don't want the mesh to rotate back to origin. So split that in 2 turns.
						DeltaMotionAtom.SetRotation((LastFrameAtom.GetRotation() * (-EndAtom.GetRotation())) * (StartAtom.GetRotation() * (-FirstFrameAtom.GetRotation())));
					}
					else
					{
						// Delta motion of the root bone in mesh space
						DeltaMotionAtom.SetRotation(EndAtom.GetRotation() * (-StartAtom.GetRotation()));
					}
				}
				else
				{
					// Delta motion of the root bone in mesh space
					DeltaMotionAtom.SetRotation(EndAtom.GetRotation() * (-StartAtom.GetRotation()));
				}
				DeltaMotionAtom.NormalizeRotation();

#if 0 // DEBUG ROOT ROTATION
				debugf(TEXT("%3.2f Root Rotation StartAtom: %s, EndAtom: %s, DeltaMotionAtom: %s"), GWorld->GetTimeSeconds(), 
					*(FQuatRotationTranslationMatrix(StartAtom.GetRotation(), FVector(0.f)).Rotator()).ToString(), 
					*(FQuatRotationTranslationMatrix(EndAtom.GetRotation(), FVector(0.f)).Rotator()).ToString(), 
					*(FQuatRotationTranslationMatrix(DeltaMotionAtom.GetRotation(), FVector(0.f)).Rotator()).ToString());
#endif

				// Only do that if an axis needs to be filtered out.
				if( RootRotationOption[0] != RRO_Extract || RootRotationOption[1] != RRO_Extract || RootRotationOption[2] != RRO_Extract )
				{
					FQuat	MeshToCompQuat(MeshToCompTM);

					// Turn delta rotation from mesh space to component space
					FQuat	CompDeltaQuat = MeshToCompQuat * DeltaMotionAtom.GetRotation() * (-MeshToCompQuat);
					CompDeltaQuat.Normalize();

#if 0 // DEBUG ROOT ROTATION
					debugf(TEXT("%3.2f Mesh To Comp Delta: %s"), GWorld->GetTimeSeconds(), *(FQuatRotationTranslationMatrix(CompDeltaQuat, FVector(0.f)).Rotator()).ToString());
#endif

					// Turn component space delta rotation to FRotator
					// @note going through rotators introduces some errors. See if this can be done using quaterions instead.
					FRotator CompDeltaRot = FQuatRotationTranslationMatrix(CompDeltaQuat, FVector(0.f)).Rotator();

					// Filter out any of the Roll (X), Pitch (Y), Yaw (Z) channels in mesh space
					if( RootRotationOption[0] != RRO_Extract )
					{
						CompDeltaRot.Roll	= 0;
					}
					if( RootRotationOption[1] != RRO_Extract )
					{
						CompDeltaRot.Pitch	= 0;
					}
					if( RootRotationOption[2] != RRO_Extract )
					{
						CompDeltaRot.Yaw	= 0;
					}

					// Turn back filtered component space delta rotator to quaternion
					FQuat CompNewDeltaQuat	= CompDeltaRot.Quaternion();

					// Turn component space delta to mesh space.
					DeltaMotionAtom.SetRotation((-MeshToCompQuat) * CompNewDeltaQuat * MeshToCompQuat);
					DeltaMotionAtom.NormalizeRotation();

#if 0 // DEBUG ROOT ROTATION
					debugf(TEXT("%3.2f Post Comp Filter. CompDelta: %s, MeshDelta: %s"), GWorld->GetTimeSeconds(), 
						*(FQuatRotationTranslationMatrix(CompNewDeltaQuat, FVector(0.f)).Rotator()).ToString(), 
						*(FQuatRotationTranslationMatrix(DeltaMotionAtom.GetRotation(), FVector(0.f)).Rotator()).ToString());
#endif

				}

#if 0 // DEBUG ROOT ROTATION
				FQuat	MeshToCompQuat(MeshToCompTM);

				// Transform mesh space delta rotation to component space.
				FQuat	CompDeltaQuat	= MeshToCompQuat * DeltaMotionAtom.GetRotation() * (-MeshToCompQuat);
				CompDeltaQuat.Normalize();

				debugf(TEXT("%3.2f Extracted Root Rotation: %s"), GWorld->GetTimeSeconds(), *(FQuatRotationTranslationMatrix(CompDeltaQuat, FVector(0.f)).Rotator()).ToString());
#endif

				// Transform delta translation by this delta rotation.
				// This is to compensate the fact that the rotation will rotate the actor, and affect the translation.
				// This assumes that root rotation won't be weighted down the tree, and that Actor will actually use it...
				// Also we don't filter rotation per axis here.. what is done for delta root rotation, should be done here as well.
				if( !DeltaMotionAtom.GetTranslation().IsZero() )
				{
					// Delta rotation since first frame
					// Remove delta we just extracted, because translation is going to be applied with current rotation, not new one.
					FQuat	MeshDeltaRotQuat = (CurrentFrameAtom.GetRotation() * (-FirstFrameAtom.GetRotation())) * (-DeltaMotionAtom.GetRotation());
					MeshDeltaRotQuat.Normalize();

					// Only do that if an axis needs to be filtered out.
					if( RootRotationOption[0] != RRO_Extract || RootRotationOption[1] != RRO_Extract || RootRotationOption[2] != RRO_Extract )
					{
						FQuat	MeshToCompQuat(MeshToCompTM);

						// Turn delta rotation from mesh space to component space
						FQuat	CompDeltaQuat = MeshToCompQuat * MeshDeltaRotQuat * (-MeshToCompQuat);
						CompDeltaQuat.Normalize();

						// Turn component space delta rotation to FRotator
						// @note going through rotators introduces some errors. See if this can be done using quaterions instead.
						FRotator CompDeltaRot = FQuatRotationTranslationMatrix(CompDeltaQuat, FVector(0.f)).Rotator();

						// Filter out any of the Roll (X), Pitch (Y), Yaw (Z) channels in mesh space
						if( RootRotationOption[0] != RRO_Extract )
						{
							CompDeltaRot.Roll	= 0;
						}
						if( RootRotationOption[1] != RRO_Extract )
						{
							CompDeltaRot.Pitch	= 0;
						}
						if( RootRotationOption[2] != RRO_Extract )
						{
							CompDeltaRot.Yaw	= 0;
						}

						// Turn back filtered component space delta rotator to quaternion
						FQuat CompNewDeltaQuat = CompDeltaRot.Quaternion();

						// Turn component space delta to mesh space.
						MeshDeltaRotQuat = (-MeshToCompQuat) * CompNewDeltaQuat * MeshToCompQuat;
						MeshDeltaRotQuat.Normalize();
					}

					FMatrix	MeshDeltaRotTM		= FQuatRotationTranslationMatrix(MeshDeltaRotQuat, FVector(0.f));
					DeltaMotionAtom.SetTranslation(MeshDeltaRotTM.InverseTransformNormal( DeltaMotionAtom.GetTranslation() ));
				}
			}
			else
			{			
				// If we're not extracting rotation, then set to identity
				DeltaMotionAtom.SetRotation(FQuat::Identity);
			}
		}
		else // if( StartTime != EndTime )
		{
			// Root Motion cannot be extracted.
			DeltaMotionAtom.SetIdentity();
		}
	}

	// Apply bone locking, with axis filtering (in component space)
	// Bone is locked to its position on the first frame of animation.
	{
		// Lock full rotation to first frame.
		if( RootRotationOption[0] != RRO_Default && RootRotationOption[1] != RRO_Default && RootRotationOption[2] != RRO_Default)
		{
			CurrentFrameAtom.SetRotation(FirstFrameAtom.GetRotation());
		}
		// Do we need to lock at least one axis of the bone's rotation to the first frame's value?
		else if( RootRotationOption[0] != RRO_Default || RootRotationOption[1] != RRO_Default || RootRotationOption[2] != RRO_Default )
		{
			FQuat	MeshToCompQuat(MeshToCompTM);

			// Find delta between current frame and first frame
			FQuat	CompFirstQuat	= MeshToCompQuat * FirstFrameAtom.GetRotation();
			FQuat	CompCurrentQuat	= MeshToCompQuat * CurrentFrameAtom.GetRotation();
			FQuat	CompDeltaQuat	= CompCurrentQuat * (-CompFirstQuat);
			CompDeltaQuat.Normalize();

			FRotator CompDeltaRot = FQuatRotationTranslationMatrix(CompDeltaQuat, FVector(0.f)).Rotator();

			// Filter out any of the Roll (X), Pitch (Y), Yaw (Z) channels in mesh space
			if( RootRotationOption[0] != RRO_Default )
			{
				CompDeltaRot.Roll	= 0;
			}
			if( RootRotationOption[1] != RRO_Default )
			{
				CompDeltaRot.Pitch	= 0;
			}
			if( RootRotationOption[2] != RRO_Default )
			{
				CompDeltaRot.Yaw	= 0;
			}

			// Use new delta and first frame to find out new current rotation.
			FQuat	CompNewDeltaQuat	= CompDeltaRot.Quaternion();
			FQuat	CompNewCurrentQuat	= CompNewDeltaQuat * CompFirstQuat;
			CompNewCurrentQuat.Normalize();

			CurrentFrameAtom.SetRotation((-MeshToCompQuat) * CompNewCurrentQuat);
			CurrentFrameAtom.NormalizeRotation();
		}

		// Lock full bone translation to first frame
		if( RootBoneOption[0] != RBA_Default && RootBoneOption[1] != RBA_Default && RootBoneOption[2] != RBA_Default )
		{
			CurrentFrameAtom.SetTranslation(FirstFrameAtom.GetTranslation());

#if 0
			debugf(TEXT("%3.2f Lock Root Bone to first frame translation: %s"), GWorld->GetTimeSeconds(), *FirstFrameAtom.GetTranslation().ToString());
#endif
		}
		// Do we need to lock at least one axis of the bone's translation to the first frame's value?
		else if( RootBoneOption[0] != RBA_Default || RootBoneOption[1] != RBA_Default || RootBoneOption[2] != RBA_Default )
		{
			FVector CompCurrentFrameTranslation			= MeshToCompTM.TransformNormal(CurrentFrameAtom.GetTranslation());
			const	FVector	CompFirstFrameTranslation	= MeshToCompTM.TransformNormal(FirstFrameAtom.GetTranslation());

			// Lock back to first frame position any of the X, Y, Z axis
			if( RootBoneOption[0] != RBA_Default  )
			{
				CompCurrentFrameTranslation.X = CompFirstFrameTranslation.X;
			}
			if( RootBoneOption[1] != RBA_Default  )
			{
				CompCurrentFrameTranslation.Y = CompFirstFrameTranslation.Y;
			}
			if( RootBoneOption[2] != RBA_Default  )
			{
				CompCurrentFrameTranslation.Z = CompFirstFrameTranslation.Z;
			}

			// convert back to mesh space
			CurrentFrameAtom.SetTranslation(MeshToCompTM.InverseTransformNormal(CompCurrentFrameTranslation));
		}
	}				
}

void UAnimNodeSequence::IssueNotifies(FLOAT DeltaTime)
{
	// If no sequence - do nothing!
	if(!AnimSeq)
	{
		return;
	}

	// If the owning actor is tagged to NOT do anim notifies, then don't!
	if (SkelComponent != NULL)
	{
		ASkeletalMeshActor* const Owner = Cast<ASkeletalMeshActor>(SkelComponent->GetOwner());
		if (Owner != NULL)
		{
			if (Owner->bShouldDoAnimNotifies == FALSE)
			{
				return;
			}
		}
	}

	// Do nothing if there are no notifies to play.
	const INT NumNotifies = AnimSeq->Notifies.Num();
	if(NumNotifies == 0)
	{
		return;
	}

	if(DeltaTime < 0.0f)
	{
		IssueNegativeRateNotifies(DeltaTime);
		return;
	}

	// Total interval we're about to proceed CurrentTime forward  (right after this IssueNotifies routine)
	FLOAT TimeToGo = DeltaTime; 

	// First, find the next notify we are going to hit.
	INT NextNotifyIndex = INDEX_NONE;
	FLOAT TimeToNextNotify = BIG_NUMBER;
	FLOAT WorkTime = BIG_NUMBER;
	// Note - if there are 2 notifies with the same time, it starts with the lower index one.
	for(INT i=0; i<NumNotifies; i++)
	{
		FLOAT NotifyEventTime = AnimSeq->Notifies(i).Time;
		FLOAT TryTimeToNotify = NotifyEventTime - CurrentTime;
		if(TryTimeToNotify < 0.0f)
		{
			if(!bLooping)
			{
				// Not interested in notifies before current time if not looping.
				continue; 
			}
			else
			{
				// Correct TimeToNextNotify as it lies before WorkTime.
				TryTimeToNotify += AnimSeq->SequenceLength; 
			}
		}

		// Check to find soonest one.
		if(TryTimeToNotify < TimeToNextNotify)
		{
			TimeToNextNotify = TryTimeToNotify;
			NextNotifyIndex = i;
			WorkTime = NotifyEventTime;
		}
	}

	// Backup SeqNode, in case it gets changed during a notify, so we can process them all.
	UAnimSequence* AnimSeqNotify = AnimSeq;

	// Set flag to show we are firing notifies.
	bIsIssuingNotifies = TRUE;

	// Separate pass to look for any notifies with an active duration
	// NOTE: durations are clamped to the sequence length, so we can opt out of the looping edge cases
	for(INT i=0; i<NumNotifies; i++)
	{
		// Check to see if this is a notify with a duration
		if (AnimSeq->Notifies(i).Duration > 0.f)
		{
			const FLOAT AnimNotifyStartTime = AnimSeq->Notifies(i).Time;
			const FLOAT AnimNotifyEndTime = AnimNotifyStartTime + AnimSeq->Notifies(i).Duration;
			//debugf(TEXT("notify: current %.4f, delta %.4f, end %.4f"),CurrentTime,DeltaTime,AnimNotifyEndTime);
			if ((CurrentTime < AnimNotifyEndTime) && (CurrentTime > AnimNotifyStartTime))
			{
				UAnimNotify* AnimNotify = AnimSeq->Notifies(i).Notify;
				if (AnimNotify != NULL)
				{
					AnimNotify->NotifyTick(this,CurrentTime,Min<FLOAT>(DeltaTime,AnimSeq->Notifies(i).Duration), AnimSeq->Notifies(i).Duration);
					if (CurrentTime + DeltaTime >= AnimNotifyEndTime)
					{
						AnimNotify->NotifyEnd(this,CurrentTime);
					}
					if (AnimSeq != AnimSeqNotify)
					{
						//debugf(NAME_Warning,TEXT("Animation sequence changed from notify, aborting further duration notifies"));
						break;
					}
				}
			}
		}
	}

	// If there is no potential next notify, do nothing.
	// This can only happen if there are no notifies (and we would have returned at start) or the anim is not looping.
	if(NextNotifyIndex == INDEX_NONE)
	{
		bIsIssuingNotifies = FALSE;
		check(!bLooping);
		return;
	}

	// Wind current time to first notify.
	TimeToGo -= TimeToNextNotify;

	// Then keep walking forwards until we run out of time.
	while( TimeToGo > 0.0f )
	{
		//debugf( TEXT("NOTIFY: %d %s %f"), NextNotifyIndex, *(AnimSeqNotify->Notifies(NextNotifyIndex).Comment.ToString()), TimeToGo );

		// Execute this notify. NextNotifyIndex will be the soonest notify inside the current TimeToGo interval.
		UAnimNotify* AnimNotify = AnimSeqNotify->Notifies(NextNotifyIndex).Notify;
		if( AnimNotify )
		{
			// Call Notify function
			AnimNotify->Notify( this );
		}
		
		// Then find the next one.
		NextNotifyIndex = (NextNotifyIndex + 1) % NumNotifies; // Assumes notifies are ordered.
		TimeToNextNotify = AnimSeqNotify->Notifies(NextNotifyIndex).Time - WorkTime;

		// Wrap if necessary.
		if( NextNotifyIndex == 0 )
		{
			if( !bLooping )
			{
				// If not looping, nothing more to do if notify is before current working time.
				bIsIssuingNotifies = FALSE;
				return;
			}
			else
			{
				// Correct TimeToNextNotify as it lies before WorkTime.
				TimeToNextNotify += AnimSeqNotify->SequenceLength; 
			}
		}

		// Wind on to next notify.
		TimeToGo -= TimeToNextNotify;
		WorkTime = AnimSeqNotify->Notifies(NextNotifyIndex).Time;
	}

	bIsIssuingNotifies = FALSE;
}

/** 
  * Issue notifies for animations playing in reverse. 
  **/
void UAnimNodeSequence::IssueNegativeRateNotifies(FLOAT DeltaTime)
{
	// If no sequence - do nothing!
	if(!AnimSeq)
	{
		return;
	}

	// Do nothing if there are no notifies to play.
	const INT NumNotifies = AnimSeq->Notifies.Num();
	if(NumNotifies == 0)
	{
		return;
	}

	//shouldn't be here if this hits, should be using the normal IssueNotifies
	check(DeltaTime<=0.0f);

	// Total interval we're about to proceed CurrentTime forward  (right after this IssueNotifies routine)
	FLOAT TimeToGo = DeltaTime; 

	// First, find the next notify we are going to hit.
	INT NextNotifyIndex = INDEX_NONE;
	FLOAT TimeToNextNotify = BIG_NUMBER;
	FLOAT WorkTime = BIG_NUMBER;

	// Note - if there are 2 notifies with the same time, it starts with the lower index one.
	for(INT i=NumNotifies-1; i>=0; i--)
	{
		FLOAT NotifyEventTime = AnimSeq->Notifies(i).Time;
		FLOAT TryTimeToNotify = CurrentTime - NotifyEventTime;
		if(TryTimeToNotify < 0.0f)
		{
			if(!bLooping)
			{
				// Not interested in notifies before current time if not looping.
				continue; 
			}
			else
			{
				// Correct TimeToNextNotify as it lies before WorkTime.
				TryTimeToNotify += AnimSeq->SequenceLength; 
			}
		}

		// Check to find soonest one.
		if(TryTimeToNotify < TimeToNextNotify)
		{
			TimeToNextNotify = TryTimeToNotify;
			NextNotifyIndex = i;
			WorkTime = NotifyEventTime;
		}
	}

	// If there is no potential next notify, do nothing.
	// This can only happen if there are no notifies (and we would have returned at start) or the anim is not looping.
	if(NextNotifyIndex == INDEX_NONE)
	{
		check(!bLooping);
		return;
	}

	// Wind current time to first notify.
	TimeToGo += TimeToNextNotify;

	// Set flag to show we are firing notifies.
	bIsIssuingNotifies = TRUE;
	// Backup SeqNode, in case it gets changed during a notify, so we can process them all.
	UAnimSequence* AnimSeqNotify = AnimSeq;

	// Then keep walking forwards until we run out of time.
	while( TimeToGo < 0.0f )
	{
		//debugf( TEXT("NOTIFY: %d %s %f"), NextNotifyIndex, *(AnimSeqNotify->Notifies(NextNotifyIndex).Comment.ToString()), TimeToGo );

		// Execute this notify. NextNotifyIndex will be the soonest notify inside the current TimeToGo interval.
		UAnimNotify* AnimNotify = AnimSeqNotify->Notifies(NextNotifyIndex).Notify;
		if( AnimNotify )
		{
			// Call Notify function
			AnimNotify->Notify( this );
		}
		
		// Then find the next one.
		NextNotifyIndex--; // Assumes notifies are ordered.
		if(NextNotifyIndex < 0)
			NextNotifyIndex = NumNotifies - 1;
		TimeToNextNotify = WorkTime - AnimSeqNotify->Notifies(NextNotifyIndex).Time;

		// Wrap if necessary.
		if( NextNotifyIndex == NumNotifies - 1 )
		{
			if( !bLooping )
			{
				// If not looping, nothing more to do if notify is before current working time.
				bIsIssuingNotifies = FALSE;
				return;
			}
			else
			{
				// Correct TimeToNextNotify as it lies before WorkTime.
				TimeToNextNotify += AnimSeqNotify->SequenceLength; 
			}
		}

		// Wind on to next notify.
		TimeToGo += TimeToNextNotify;
		WorkTime = AnimSeqNotify->Notifies(NextNotifyIndex).Time;
	}

	bIsIssuingNotifies = FALSE;
}

/** Returns the camera associated with the skelmesh's owner, if any. */
ACamera* UAnimNodeSequence::GetPlayerCamera() const
{
	if (SkelComponent != NULL)
	{
		AActor* const Owner = SkelComponent->GetOwner();

		if (Owner != NULL)
		{
			APawn* const Pawn = Owner->GetAPawn();
			if ( (Pawn != NULL) && (Pawn->Controller != NULL) )
			{
				APlayerController* const PC = Pawn->Controller->GetAPlayerController();

				if (PC != NULL)
				{
					return PC->PlayerCamera;
				}
			}
		}
	}

	return NULL;
}


/** Start the current animation playing. This just sets the bPlaying flag to true, so that TickAnim will move CurrentTime forwards. */
void UAnimNodeSequence::PlayAnim(UBOOL bInLoop, FLOAT InRate, FLOAT StartTime)
{
	CurrentTime		= (InRate < 0.f && AnimSeq != NULL) ? AnimSeq->SequenceLength - StartTime : StartTime;
	PreviousTime	= CurrentTime;
	Rate			= InRate;
	bLooping		= bInLoop;
	bPlaying		= TRUE;

	// Clear Cached data.
	ConditionalClearCachedData();

	if( bForceRefposeWhenNotPlaying && SkelComponent->bForceRefpose)
	{
		SkelComponent->SetForceRefPose(FALSE);
	}

	if( bCauseActorAnimPlay && SkelComponent->GetOwner() )
	{
		SkelComponent->GetOwner()->eventOnAnimPlay(this);
	}

	// start any nonlooping camera anim  looping camera anims will start in Tick().
	if (CameraAnim && !bLooping)
	{
		StartCameraAnim();
	}
}

/** Restart the current animation with the current settings */
void UAnimNodeSequence::ReplayAnim()
{
	PlayAnim(bLooping, Rate, 0.f);
}

/** Stop playing current animation. Will set bPlaying to false. */
void UAnimNodeSequence::StopAnim()
{
	bPlaying = false;

	if (ActiveCameraAnimInstance != NULL)
	{
		StopCameraAnim();
	}
}

/** Restart camera animations */
void UAnimNodeSequence::OnBecomeRelevant()
{
	Super::OnBecomeRelevant();

	if (ActiveCameraAnimInstance != NULL)
	{
		ActiveCameraAnimInstance->CurTime = CurrentTime;
		ActiveCameraAnimInstance->PlayRate = Rate;
	}
}

/** Pause camera animations */
void UAnimNodeSequence::OnCeaseRelevant()
{
	if( ActiveCameraAnimInstance != NULL )
	{
		ActiveCameraAnimInstance->PlayRate = 0;
	}

	Super::OnCeaseRelevant();
	
}

/** Starts playing any camera anim we want to play in conjunction with this anim. */
void UAnimNodeSequence::StartCameraAnim()
{
	if (CameraAnim != NULL)
	{
		ACamera* Cam = GetPlayerCamera();
		if (Cam)
		{
			// let's be sure we aren't somehow stomping our reference to a playing anim
			if (ActiveCameraAnimInstance != NULL)
			{
				StopCameraAnim();
			}

			// start our desired anim
			ActiveCameraAnimInstance = Cam->PlayCameraAnim(CameraAnim, CameraAnimPlayRate, CameraAnimScale, CameraAnimBlendInTime, CameraAnimBlendOutTime, bLoopCameraAnim, bRandomizeCameraAnimLoopStartTime);
			if (ActiveCameraAnimInstance != NULL)
			{
				// we don't want this deallocated behind our backs.  necessary since we're pooling the instances.
				ActiveCameraAnimInstance->bAutoReleaseWhenFinished = FALSE;
				ActiveCameraAnimInstance->RegisterAnimNode(this);
			}
		}
	}
}

/** Stops playing any active camera anim playing in conjunction with this anim. */
void UAnimNodeSequence::StopCameraAnim()
{
	if (ActiveCameraAnimInstance != NULL)
	{
		ActiveCameraAnimInstance->Stop(TRUE);
		ActiveCameraAnimInstance->bAutoReleaseWhenFinished = TRUE;
		ActiveCameraAnimInstance = NULL;
	}
}


/** 
 *	Set the CurrentTime to the supplied position. 
 *	If bFireNotifies is true, this will fire any notifies between current and new time.
 */
void UAnimNodeSequence::SetPosition(FLOAT NewTime, UBOOL bFireNotifies)
{
	// Ensure NewTime lies within sequence length.
	const FLOAT AnimLength = AnimSeq ? AnimSeq->SequenceLength : 0.f;
	NewTime = ::Clamp<FLOAT>(NewTime, 0.f, AnimLength+KINDA_SMALL_NUMBER);

	// Find the amount we are moving.
	const FLOAT DeltaTime = NewTime - CurrentTime;
	// This is to support Matinee root motion in preview

	// If moving forwards, and this node generates notifies, and is sufficiently 'in the mix', fire notifies now.
	if( bFireNotifies && 
		DeltaTime != 0.f && 
		!bNoNotifies && 
		(NodeTotalWeight >= NotifyWeightThreshold) )
	{
		IssueNotifies(DeltaTime);		
	}
	
	// Then update the time.
	CurrentTime = NewTime;

	// if not used by editor root motion 
	// If we don't fire notifies, it means we jump to that new position instantly, 
	// so reset previous time
 	if( !bFireNotifies )
 	{
 		PreviousTime = CurrentTime;
 	}
	
	// If animation has only one frame then no need to clear it.
	if( !AnimSeq || AnimSeq->NumFrames > 1 )
	{
		// Animation updated, clear cached data
		ConditionalClearCachedData();
	}
}


/** 
 * Get normalized position, from 0.f to 1.f.
 */
FLOAT UAnimNodeSequence::GetNormalizedPosition() const
{
	if( AnimSeq && AnimSeq->SequenceLength > 0.f )
	{
		return Clamp<FLOAT>(CurrentTime / AnimSeq->SequenceLength, 0.f, 1.f);
	}

	return 0.f;
}

/** 
 * Finds out normalized position of a synchronized node given a relative position of a group. 
 * Takes into account node's relative SynchPosOffset.
 */
FLOAT UAnimNodeSequence::FindGroupRelativePosition(FLOAT GroupRelativePosition) const
{
	FLOAT NormalizedPosition = appFmod(GroupRelativePosition + SynchPosOffset, 1.f);
	if( NormalizedPosition < 0.f )
	{
		NormalizedPosition += 1.f;
	}

	return bReverseSync ? 1.f - NormalizedPosition : NormalizedPosition;
}

/** 
 * Finds out position of a synchronized node given a relative position of a group. 
 * Takes into account node's relative SynchPosOffset.
 */
FLOAT UAnimNodeSequence::FindGroupPosition(FLOAT GroupRelativePosition) const
{
	if( AnimSeq )
	{
		return FindGroupRelativePosition(GroupRelativePosition) * AnimSeq->SequenceLength;
	}
	return 0.f;
}

/** 
 * Get relative position of a synchronized node. 
 * Taking into account node's relative offset.
 */
FLOAT UAnimNodeSequence::GetGroupRelativePosition() const
{
	if( AnimSeq && AnimSeq->SequenceLength > 0.f )
	{
		// Find it's relative position on its time line.
		FLOAT RelativePosition = appFmod((CurrentTime / AnimSeq->SequenceLength) - SynchPosOffset, 1.f);
		if( RelativePosition < 0.f )
		{
			RelativePosition += 1.f;
		}

		return bReverseSync ? 1.f - RelativePosition : RelativePosition;
	}

	return 0.f;
}

/** Returns the global play rate of this animation. Taking into account all Rate Scales */
FLOAT UAnimNodeSequence::GetGlobalPlayRate()
{
	// AnimNodeSequence play rate
	FLOAT GlobalRate = Rate * SkelComponent->GlobalAnimRateScale;

	// AnimSequence play rate
	if( AnimSeq )
	{
		GlobalRate *= AnimSeq->RateScale;
	}

	// AnimGroup play rate
	if( SynchGroupName != NAME_None && SkelComponent )
	{
		UAnimTree* RootNode = Cast<UAnimTree>(SkelComponent->Animations);
		if( RootNode )
		{
			const INT GroupIndex = RootNode->GetGroupIndex(SynchGroupName);
			if( GroupIndex != INDEX_NONE )
			{
				GlobalRate *= RootNode->AnimGroups(GroupIndex).RateScale;
			}
		}
	}

	return GlobalRate;
}


FLOAT UAnimNodeSequence::GetAnimPlaybackLength()
{
	if( AnimSeq )
	{
		const FLOAT GlobalPlayRate = GetGlobalPlayRate();
		if( GlobalPlayRate != 0.f )
		{
			const FLOAT ActualLength = EndTime > 0.0f ? EndTime : AnimSeq->SequenceLength;

			return ActualLength / GlobalPlayRate;
		}
	}

	return 0.f;
}

/** 
 * Returns in seconds the time left until the animation is done playing. 
 * This is assuming the play rate is not going to change.
 */
FLOAT UAnimNodeSequence::GetTimeLeft()
{
	if( AnimSeq )
	{
		const FLOAT ActualLength = EndTime > 0.0f ? EndTime : AnimSeq->SequenceLength;

		const FLOAT GlobalPlayRate = GetGlobalPlayRate();
		if( GlobalPlayRate > 0.f )
		{
			return Max(ActualLength - CurrentTime, 0.f) / GlobalPlayRate;
		}
		// StartTime and EndTime don't work with playing anims that are in reverse
		else if( GlobalPlayRate < 0.f )
		{
			return Max(CurrentTime, 0.f) / -GlobalPlayRate;
		}
	}

	return 0.f;
}

void UAnimNodeSequence::ResetAnimNodeToSource(UAnimNode *SourceNode)
{
	Super::ResetAnimNodeToSource(SourceNode);

	UAnimNodeSequence* SourceSequence = Cast<UAnimNodeSequence>(SourceNode);
	if (SourceSequence)
	{
		bPlaying = SourceSequence->bPlaying;
	}
}

////////////////////////////////
// Actor latent function

void AActor::FinishAnim(class UAnimNodeSequence* SeqNode, UBOOL bFinishOnBlendOut)
{
	GetStateFrame()->LatentAction = EPOLL_FinishAnim;
	LatentSeqNode  = SeqNode;

	if( LatentSeqNode )
	{
		// this allows up to return from FinishAnim on blendout when wanted
		LatentSeqNode->bCheckForFinishAnimEarly = bFinishOnBlendOut;
		LatentSeqNode->bBlendingOut = FALSE;
	}
}

void AActor::execPollFinishAnim( FFrame& Stack, RESULT_DECL )
{
	// Block as long as LatentSeqNode is present and playing and relevant.

	// check for early exit
	if( !LatentSeqNode || !LatentSeqNode->bPlaying || !LatentSeqNode->bRelevant || 
		(LatentSeqNode->bCheckForFinishAnimEarly && LatentSeqNode->bBlendingOut) )
	{
		GetStateFrame()->LatentAction = 0;
		LatentSeqNode = NULL;
	}
}
IMPLEMENT_FUNCTION( AActor, EPOLL_FinishAnim, execPollFinishAnim );


/************************************************************************************
 * UAnimNodeSequenceBlendBase
 ***********************************************************************************/

void UAnimNodeSequenceBlendBase::CheckAnimsUpToDate()
{
	// Make sure animations are up to date
	const	INT		NumAnims		= Anims.Num();
			UBOOL	bUpdatedSeqNode = FALSE;

	for(INT i=0; i<NumAnims; i++)
	{
		SetAnimInfo(Anims(i).AnimName, Anims(i).AnimInfo);

		if( !bUpdatedSeqNode && Anims(i).AnimInfo.AnimSeq != NULL )
		{
			// Ensure AnimNodeSequence playback compatibility by setting one animation
			// The node will use this animation for timing and notifies.
			SetAnim(Anims(i).AnimName);
			bUpdatedSeqNode = (AnimSeq != NULL);
		}
	}
}

/** Get AnimInfo total weight */
FLOAT UAnimNodeSequenceBlendBase::GetAnimInfoTotalWeight()
{
	FLOAT TotalWeight = 0.f;
	const INT NumAnims = Anims.Num();
	for(INT i=0; i<NumAnims; i++)
	{
		TotalWeight += Anims(i).Weight;
	}
	return TotalWeight;
}

/**
 * Init anim tree.
 * Make sure animation references are up to date.
 */
void UAnimNodeSequenceBlendBase::InitAnim(USkeletalMeshComponent* MeshComp, UAnimNodeBlendBase* Parent)
{
	START_INITANIM_TIMER
	{
		EXCLUDE_PARENT_TIME
		// Call Super version first, because that's where SkeletalMeshComponent reference is set (see UAnimNode::InitAnim()).
		Super::InitAnim(MeshComp, Parent);
	}

	// Clear references instantly, re-cache in DeferredInitAnim()
	INT const NumAnims = Anims.Num();
	for(INT i=0; i<NumAnims; i++)
	{
		FName OldName = Anims(i).AnimName;
		SetAnimInfo(NAME_None, Anims(i).AnimInfo);
		Anims(i).AnimName = OldName;
	}	

	// Make sure weights sum up to one
	// If all child weights are zero - set the first one to the active child.
	FLOAT const ChildWeightSum = GetAnimInfoTotalWeight();
	if( ChildWeightSum <= ZERO_ANIMWEIGHT_THRESH )
	{
		Anims(0).Weight = 1.f;
	}
}

/** Deferred Initialization, called only when the node is relevant in the tree. */
void UAnimNodeSequenceBlendBase::DeferredInitAnim()
{
	Super::DeferredInitAnim();

	// Make sure animations are up to date
	CheckAnimsUpToDate();
}

/** AnimSets have been updated, update all animations */
void UAnimNodeSequenceBlendBase::AnimSetsUpdated()
{
	Super::AnimSetsUpdated();

	// Clear references instantly, re-cache in DeferredInitAnim()
	INT const NumAnims = Anims.Num();
	for(INT i=0; i<NumAnims; i++)
	{
		FName OldName = Anims(i).AnimName;
		SetAnimInfo(NAME_None, Anims(i).AnimInfo);
		Anims(i).AnimName = OldName;
	}	
}


/**
 * A property has been changed from the editor
 * Make sure animation references are up to date.
 */
void UAnimNodeSequenceBlendBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
#if 0
	debugf(TEXT("* PostEditChange on %s"), *GetFName());
#endif

	// Make sure animations are up to date
	CheckAnimsUpToDate();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


/** 
 * Set a new animation by name.
 * This will find the UAnimationSequence by name, from the list of AnimSets specified in the SkeletalMeshComponent and cache it.
 */
void UAnimNodeSequenceBlendBase::SetAnimInfo(FName InSequenceName, FAnimInfo& InAnimInfo)
{
	UBOOL bFailed = FALSE;

#if 0
	debugf(TEXT("** SetAnimInfo %s on %s"), *InSequenceName, *GetFName());
#endif

	// if not valid, reset
	if( InSequenceName == NAME_None || !SkelComponent || !SkelComponent->SkeletalMesh )
	{
		bFailed = TRUE;
	}

	if( !bFailed )
	{
		InAnimInfo.AnimSeq = SkelComponent->FindAnimSequence(InSequenceName);
		if( InAnimInfo.AnimSeq != NULL )
		{
			InAnimInfo.AnimSeqName = InSequenceName;
			
			UAnimSet* AnimSet = InAnimInfo.AnimSeq->GetAnimSet();

			InAnimInfo.AnimLinkupIndex = AnimSet->GetMeshLinkupIndex(SkelComponent->SkeletalMesh);

			check(InAnimInfo.AnimLinkupIndex != INDEX_NONE);
			check(InAnimInfo.AnimLinkupIndex < AnimSet->LinkupCache.Num());

			FAnimSetMeshLinkup* AnimLinkup = &AnimSet->LinkupCache(InAnimInfo.AnimLinkupIndex);

			check(AnimLinkup->BoneToTrackTable.Num() == SkelComponent->SkeletalMesh->RefSkeleton.Num());

#if !FINAL_RELEASE
			// If we're tracing animation usage, set bHasBeenUsed to be TRUE
			extern UBOOL GShouldTraceAnimationUsage;
			if ( GShouldTraceAnimationUsage )
			{
				if ( InAnimInfo.AnimSeq->bHasBeenUsed == FALSE )
				{
					InAnimInfo.AnimSeq->bHasBeenUsed = TRUE;
				}
			}
#endif
		}
		else
		{
			bFailed = TRUE;
		}
	}

	if( bFailed )
	{
		// See if we should bypass the error message
		const UBOOL bDisableWarning = bDisableWarningWhenAnimNotFound || (SkelComponent && SkelComponent->bDisableWarningWhenAnimNotFound);

		// If InSequenceName == NAME_None, it's not really error... it was intentional! :)
		if( InSequenceName != NAME_None && !bDisableWarning )
		{
			AActor* Owner = SkelComponent ? SkelComponent->GetOwner() : NULL;

			debugf(NAME_Warning, TEXT("%s - Failed, with animsequence '%s' on SkeletalMeshComponent: '%s' whose owner is: '%s' and is of type '%s'"), 
				*GetName(), 
				*InSequenceName.ToString(), 
				SkelComponent ? *SkelComponent->GetName() : TEXT("None"),
				Owner ? *Owner->GetName() : TEXT("None"),
				SkelComponent ? *SkelComponent->TemplateName.ToString() : TEXT("None")
				);
		}

		InAnimInfo.AnimSeqName		= NAME_None;
		InAnimInfo.AnimSeq			= NULL;
		InAnimInfo.AnimLinkupIndex	= INDEX_NONE;
	}
}


/** Update animation usage **/
void UAnimNodeSequenceBlendBase::UpdateAnimationUsage( FLOAT DeltaSeconds )
{
#if !FINAL_RELEASE
	extern UBOOL GShouldTraceAnimationUsage;
	if ( GShouldTraceAnimationUsage )
	{
		if ( SkelComponent && SkelComponent->bRecentlyRendered && AnimSeq )
		{
			for(INT i=0; i<Anims.Num(); i++)
			{
				if ( Anims(i).AnimInfo.AnimSeq )
				{
					Anims(i).AnimInfo.AnimSeq->UseScore += NodeTotalWeight*Anims(i).Weight*DeltaSeconds*Min(SkelComponent->MaxDistanceFactor, 1.f);
//					debugf(TEXT("%s : %s(%s) - Score(%0.6f), DisplayFactor(%0.2f)"), *SkelComponent->GetOwner()->GetName(), *Anims(i).AnimInfo.AnimSeq->GetAnimSet()->GetName(), *Anims(i).AnimInfo.AnimSeq->SequenceName.GetNameString(), Anims(i).AnimInfo.AnimSeq->UseScore, SkelComponent->MaxDistanceFactor);
				}
			}
		}
	}
#endif
}

/**
 * Blends together the animations of this node based on the Weight in each element of the Anims array.
 *
 * @param	Atoms			Output array of relative bone transforms.
 * @param	DesiredBones	Indices of bones that we want to return. Note that bones not in this array will not be modified, so are not safe to access! 
 *							This array must be in strictly increasing order.
 */
void UAnimNodeSequenceBlendBase::GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys)
{
	START_GETBONEATOM_TIMER

	// See if results are cached.
	if( GetCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num()) )
	{
		return;
	}

	const INT NumAnims = Anims.Num();

#if !FINAL_RELEASE
	if( NumAnims == 0 )
	{
		debugf(TEXT("UAnimNodeSequenceBlendBase::GetBoneAtoms - %s - Anims array is empty!"), *GetName());
		RootMotionDelta.SetIdentity();
		bHasRootMotion	= 0;
		FillWithRefPose(Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
		return;
	}
#endif

#if !FINAL_RELEASE
	FLOAT TotalWeight = GetAnimInfoTotalWeight();

	// Check all children weights sum to 1.0
	if( Abs(TotalWeight - 1.f) > ZERO_ANIMWEIGHT_THRESH )
	{
		check( FALSE );
		debugf(TEXT("WARNING: UAnimNodeSequenceBlendBase (%s) has Children weights which do not sum to 1.0."), *GetName());

		for(INT i=0; i<NumAnims; i++)
		{
			debugf(TEXT("Pose: %d Weight: %f"), i, Anims(i).Weight);
		}

		debugf(TEXT("Total Weight: %f"), TotalWeight);
		//@todo - adjust first node weight to 

		RootMotionDelta.SetIdentity();
		bHasRootMotion	= 0;
		FillWithRefPose(Atoms, DesiredBones, SkelComponent->SkeletalMesh->RefSkeleton);
		return;
	}
#endif

	// Find index of the last child with a non-zero weight.
	INT LastChildIndex = INDEX_NONE;
	for(INT i=0; i<NumAnims; i++)
	{
		if( Anims(i).Weight > ZERO_ANIMWEIGHT_THRESH )
		{
			// If this is the only child with any weight, pass Atoms array into it directly.
			if( Anims(i).Weight >= (1.f - ZERO_ANIMWEIGHT_THRESH) )
			{
				GetAnimationPose(Anims(i).AnimInfo.AnimSeq, Anims(i).AnimInfo.AnimLinkupIndex, Atoms, DesiredBones, RootMotionDelta, bHasRootMotion, CurveKeys);
				SaveCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num());
				return;
			}
			LastChildIndex = i;
		}
	}
	check(LastChildIndex != INDEX_NONE);

	// We don't allocate this array until we need it.
	FBoneAtomArray ChildAtoms;
	UBOOL bNoChildrenYet = TRUE;

	// Children Curve Keys for blending later
	FArrayCurveKeyArray	ChildrenCurveKeys;
	ChildrenCurveKeys.AddZeroed(Anims.Num());

	// Root Motion
	FBoneAtom ExtractedRootMotion;

	// Iterate over each child getting its atoms, scaling them and adding them to output (Atoms array)
	for(INT i=0; i<=LastChildIndex; i++)
	{
		ScalarRegister VAnimWeight(Anims(i).Weight);

		// If this child has non-zero weight, blend it into accumulator.
		if( Anims(i).Weight > ZERO_ANIMWEIGHT_THRESH )
		{
			// Do need to request atoms, so allocate array here.
			if( ChildAtoms.Num() == 0 )
			{
				const INT NumAtoms = SkelComponent->SkeletalMesh->RefSkeleton.Num();
				check(NumAtoms == Atoms.Num());
				ChildAtoms.Add(NumAtoms);
			}

			check(ChildAtoms.Num() == Atoms.Num());

			// Get Animation pose
			GetAnimationPose(Anims(i).AnimInfo.AnimSeq, Anims(i).AnimInfo.AnimLinkupIndex, ChildAtoms, DesiredBones, ExtractedRootMotion, bHasRootMotion, ChildrenCurveKeys(i));

			if( bHasRootMotion )
			{
				// Accumulate weighted Root Motion
				if( bNoChildrenYet )
				{
					RootMotionDelta = ExtractedRootMotion * VAnimWeight;
				}
				else
				{
					RootMotionDelta.AccumulateWithShortestRotation(ExtractedRootMotion, VAnimWeight);
				}

				// If Last Child, normalize rotation quaternion
				if( i == LastChildIndex )
				{
					RootMotionDelta.NormalizeRotation();
				}
		
#if 0 // Debug Root Motion
				if( !RMD.GetTranslation().IsZero() )
				{
					const FVector RDTW = RMD.GetTranslation() * Anims(i).Weight;
					debugf(TEXT("%3.2f %s: Added weighted root motion translation: %3.2f, vect: %s"), GWorld->GetTimeSeconds(), *GetFName(), RDTW.Size(), *RDTW.ToString());
				}
#endif
			}

			for(INT j=0; j<DesiredBones.Num(); j++)
			{
				const INT BoneIndex = DesiredBones(j);

				// We just write the first childrens' atoms into the output array. Avoids zero-ing it out.
				if( bNoChildrenYet )
				{
					Atoms(BoneIndex) = ChildAtoms(BoneIndex) * VAnimWeight;
				}
				else
				{
					// To ensure the 'shortest route', we make sure the dot product between the accumulator and the incoming child atom is positive.
					Atoms(BoneIndex).AccumulateWithShortestRotation(ChildAtoms(BoneIndex), VAnimWeight);
				}

				// If last child - normalize the rotation quaternion now.
				if( i == LastChildIndex )
				{
					Atoms(BoneIndex).NormalizeRotation();
				}
			}

			bNoChildrenYet = FALSE;
		}
	}

	if (GIsEditor || SkelComponent->bRecentlyRendered)
	{
		FCurveKeyArray NewCurveKeys;
		// returns result of blending
		if ( ChildrenCurveKeys.Num() > 1 &&  BlendCurveWeights(ChildrenCurveKeys, NewCurveKeys)  > 0 )
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
				LastUpdatedAnimMorphKeys = NewCurveKeys;
			}
		}
	#endif
#endif // WITH_EDITORONLY_DATA
	}

	SaveCachedResults(Atoms, RootMotionDelta, bHasRootMotion, CurveKeys, DesiredBones.Num());
}

/** 
* Resolve conflicts for blend curve weights if same morph target exists 
*
* @param	InChildrenCurveKeys	Array of curve keys for children. The index should match up with Children.
* @param	OutCurveKeys		Result output after blending is resolved
* 
* @return	Number of new addition to OutCurveKeys
*/
INT UAnimNodeSequenceBlendBase::BlendCurveWeights(const FArrayCurveKeyArray& InChildrenCurveKeys, FCurveKeyArray& OutCurveKeys)
{
	check(InChildrenCurveKeys.Num() == Anims.Num());

	// Make local version so that I can edit
	FArrayCurveKeyArray ChildrenCurveKeys = InChildrenCurveKeys;
	extern const FLOAT MinMorphBlendWeight;

	TMap<FName, FLOAT> CurveKeyMap;

	// if same target found, accumulate 
	// technically you can have one TArray for holding all childrens' key, but 
	// just in case in future we need to know which children has which information
	for (INT I=0; I<ChildrenCurveKeys.Num(); ++I)
	{
		const FCurveKeyArray& LocalCurveKeyArray = ChildrenCurveKeys(I);

		if ( Anims(I).Weight > MinMorphBlendWeight )
		{
			const FLOAT ChildBlendWeight = Anims(I).Weight; 

			for (INT J=0; J<LocalCurveKeyArray.Num(); ++J)
			{
				if ( LocalCurveKeyArray(J).Weight > MinMorphBlendWeight )
				{
					FLOAT * Weight = CurveKeyMap.Find(LocalCurveKeyArray(J).CurveName);
					// if the weight is found, add
					if ( Weight ) 
					{
						*Weight += LocalCurveKeyArray(J).Weight*ChildBlendWeight;
					}
					else // otherwise, add to the map
					{
						CurveKeyMap.Set(LocalCurveKeyArray(J).CurveName, LocalCurveKeyArray(J).Weight*ChildBlendWeight);
					}
				}
			}
		}
	}

	INT TotalKeys=0;
	// now iterate through and add to the array
	for (TMap<FName, FLOAT>::TConstIterator Iter(CurveKeyMap); Iter; ++Iter)
	{
		if ( Iter.Value() > MinMorphBlendWeight )
		{
//			debugf(TEXT("NodeName:%s, CurveName:%s, Value:%0.2f"), *NodeName.GetNameString(), *Iter.Key().GetNameString(), Iter.Value());
			OutCurveKeys.AddItem(FCurveKey(Iter.Key(), Iter.Value()));
			++TotalKeys;
		}
	}

	return TotalKeys;
}


/************************************************************************************
 * UAnimNodeSequenceBlendByAim
 ***********************************************************************************/

void UAnimNodeSequenceBlendByAim::CheckAnimsUpToDate()
{
	// Make sure animations are properly set
	Anims(0).AnimName = AnimName_LU;
	Anims(1).AnimName = AnimName_LC;
	Anims(2).AnimName = AnimName_LD;
	Anims(3).AnimName = AnimName_CU;
	Anims(4).AnimName = AnimName_CC;
	Anims(5).AnimName = AnimName_CD;
	Anims(6).AnimName = AnimName_RU;
	Anims(7).AnimName = AnimName_RC;
	Anims(8).AnimName = AnimName_RD;

	Super::CheckAnimsUpToDate();
}

/** Override this function in a subclass, and return normalized Aim from Pawn. */
FVector2D UAnimNodeSequenceBlendByAim::GetAim() 
{ 
	return Aim;
}

void UAnimNodeSequenceBlendByAim::TickAnim(FLOAT DeltaSeconds)
{
	// Get Normalized Aim.
	FVector2D SafeAim = GetAim();

	const UBOOL bAimChanged = (SafeAim != PreviousAim);
	if( bAimChanged )
	{
		// Reset cached bone atoms to force an update of the node.
		ConditionalClearCachedData();
		PreviousAim = SafeAim;
	}

	// Update weights to blend animations together
	if( bAimChanged || bJustBecameRelevant )
	{
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
			if( HorizontalRange.X != 0.f )
			{
				SafeAim.X = SafeAim.X / Abs(HorizontalRange.X);
			}
			else
			{
				SafeAim.X = 0.f;
			}
		}
		else
		{
			if( HorizontalRange.Y != 0.f )
			{
				SafeAim.X = SafeAim.X / HorizontalRange.Y;
			}
			else
			{
				SafeAim.X = 0.f;
			}
		}

		if( SafeAim.Y < 0.f )
		{
			if( VerticalRange.X != 0.f )
			{
				SafeAim.Y = SafeAim.Y / Abs(VerticalRange.X);
			}
			else
			{
				SafeAim.Y = 0.f;
			}
		}
		else
		{
			if( VerticalRange.Y != 0.f )
			{
				SafeAim.Y = SafeAim.Y / VerticalRange.Y;
			}
			else
			{
				SafeAim.Y = 0.f;
			}
		}

		// Make sure we're using values within legal range.
		SafeAim.X = Clamp<FLOAT>(SafeAim.X, -1.f, +1.f);
		SafeAim.Y = Clamp<FLOAT>(SafeAim.Y, -1.f, +1.f);

		if( SafeAim.X >= 0.f && SafeAim.Y >= 0.f ) // Up Right
		{
			// Calculate weight of each relevant animation
			Anims(4).Weight = BiLerp(1.f, 0.f, 0.f, 0.f, SafeAim.X, SafeAim.Y);
			Anims(7).Weight = BiLerp(0.f, 1.f, 0.f, 0.f, SafeAim.X, SafeAim.Y);
			Anims(3).Weight = BiLerp(0.f, 0.f, 1.f, 0.f, SafeAim.X, SafeAim.Y);
			Anims(6).Weight = BiLerp(0.f, 0.f, 0.f, 1.f, SafeAim.X, SafeAim.Y);

			// The rest is set to zero
			Anims(0).Weight = 0.f;
			Anims(1).Weight = 0.f;
			Anims(2).Weight = 0.f;
			Anims(5).Weight = 0.f;
			Anims(8).Weight = 0.f;
		}
		else if( SafeAim.X >= 0.f && SafeAim.Y < 0.f ) // Bottom Right
		{
			SafeAim.Y += 1.f;

			// Calculate weight of each relevant animation
			Anims(5).Weight = BiLerp(1.f, 0.f, 0.f, 0.f, SafeAim.X, SafeAim.Y);
			Anims(8).Weight = BiLerp(0.f, 1.f, 0.f, 0.f, SafeAim.X, SafeAim.Y);
			Anims(4).Weight = BiLerp(0.f, 0.f, 1.f, 0.f, SafeAim.X, SafeAim.Y);
			Anims(7).Weight = BiLerp(0.f, 0.f, 0.f, 1.f, SafeAim.X, SafeAim.Y);

			// The rest is set to zero
			Anims(0).Weight = 0.f;
			Anims(1).Weight = 0.f;
			Anims(2).Weight = 0.f;
			Anims(3).Weight = 0.f;
			Anims(6).Weight = 0.f;
		}
		else if( SafeAim.X < 0.f && SafeAim.Y >= 0.f ) // Up Left
		{
			SafeAim.X += 1.f;

			// Calculate weight of each relevant animation
			Anims(1).Weight = BiLerp(1.f, 0.f, 0.f, 0.f, SafeAim.X, SafeAim.Y);
			Anims(4).Weight = BiLerp(0.f, 1.f, 0.f, 0.f, SafeAim.X, SafeAim.Y);
			Anims(0).Weight = BiLerp(0.f, 0.f, 1.f, 0.f, SafeAim.X, SafeAim.Y);
			Anims(3).Weight = BiLerp(0.f, 0.f, 0.f, 1.f, SafeAim.X, SafeAim.Y);

			// The rest is set to zero
			Anims(2).Weight = 0.f;
			Anims(5).Weight = 0.f;
			Anims(6).Weight = 0.f;
			Anims(7).Weight = 0.f;
			Anims(8).Weight = 0.f;
		}
		else if( SafeAim.X < 0.f && SafeAim.Y < 0.f ) // Bottom Left
		{
			SafeAim.X += 1.f;
			SafeAim.Y += 1.f;

			// Calculate weight of each relevant animation
			Anims(2).Weight = BiLerp(1.f, 0.f, 0.f, 0.f, SafeAim.X, SafeAim.Y);
			Anims(5).Weight = BiLerp(0.f, 1.f, 0.f, 0.f, SafeAim.X, SafeAim.Y);
			Anims(1).Weight = BiLerp(0.f, 0.f, 1.f, 0.f, SafeAim.X, SafeAim.Y);
			Anims(4).Weight = BiLerp(0.f, 0.f, 0.f, 1.f, SafeAim.X, SafeAim.Y);

			// The rest is set to zero
			Anims(0).Weight = 0.f;
			Anims(3).Weight = 0.f;
			Anims(6).Weight = 0.f;
			Anims(7).Weight = 0.f;
			Anims(8).Weight = 0.f;
		}
	}

	Super::TickAnim(DeltaSeconds);
}


FLOAT UAnimNodeSequenceBlendByAim::GetSliderPosition(INT SliderIndex, INT ValueIndex)
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

void UAnimNodeSequenceBlendByAim::HandleSliderMove(INT SliderIndex, INT ValueIndex, FLOAT NewSliderValue)
{
	check(SliderIndex == 0);
	check(ValueIndex == 0 || ValueIndex == 1);

	if( ValueIndex == 0 )
	{
		Aim.X = (NewSliderValue - 0.5f) * 2.f;
	}
	else
	{
		Aim.Y = (NewSliderValue - 0.5f) * -2.f;
	}
}

FString UAnimNodeSequenceBlendByAim::GetSliderDrawValue(INT SliderIndex)
{
	check(SliderIndex == 0);
	return FString::Printf(TEXT("%0.2f,%0.2f"), Aim.X, Aim.Y);
}
