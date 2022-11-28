/*=============================================================================
	UnAnimPlay.cpp: Skeletal mesh animation  - UAnimNodeSequence implementation.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Erik de Neve	
		* Re-written by James Golding	
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"


IMPLEMENT_CLASS(UAnimNodeSequence);

void UAnimNodeSequence::PostEditChange(UProperty* PropertyThatChanged)
{
	SetAnim( AnimSeqName );

	if(SkelComponent)
	{
		SkelComponent->UpdateSpaceBases();
		SkelComponent->Update();
	}

	Super::PostEditChange(PropertyThatChanged);
}

void UAnimNodeSequence::InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent )
{
	Super::InitAnim( meshComp, Parent );

	SetAnim( AnimSeqName );
}

void UAnimNodeSequence::TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight )
{
	check(!ParentNode || ParentNode->SkelComponent == SkelComponent);

	UBOOL bHasBegunPlay = false;
	if(SkelComponent->Owner)
	{
		bHasBegunPlay = SkelComponent->Owner->GetLevel()->GetLevelInfo()->bBegunPlay;
	}

	// Can only do something if we are currently playing a valid AnimSequence
	if(bPlaying && AnimSeq && bHasBegunPlay)
	{
		// Time to move forwards by - DeltaSeconds scaled by various factors.
		FLOAT MoveDelta = Rate * AnimSeq->RateScale * DeltaSeconds;

		// Before we actually advance the time, issue any notifies (if desired).
		if( !bNoNotifies && (TotalWeight >= NotifyWeightThreshold) )
		{
			IssueNotifies(MoveDelta);
		}

		// Then update internal time.
		CurrentTime += MoveDelta;

		// See if we passed the end of the animtion.
		if(CurrentTime > AnimSeq->SequenceLength)
		{
			// If we are looping, wrap over.
			if(bLooping)
			{
				CurrentTime = appFmod( CurrentTime, AnimSeq->SequenceLength );
			}
			// If not, snap time to end of sequence and stop playing.
			else 
			{
				CurrentTime = AnimSeq->SequenceLength;
				bPlaying = false;

				// When we hit the end and stop, issue notifies to parent AnimNodeBlendBase
				if (ParentNode != NULL)
				{
					ParentNode->OnChildAnimEnd(this);
				}

				if(bCauseActorAnimEnd && SkelComponent->Owner)
				{
					SkelComponent->Owner->eventOnAnimEnd(this);
				}
			}
		}
	}
}

void UAnimNodeSequence::GetBoneAtoms( TArray<FBoneAtom>& Atoms )
{
	check(SkelComponent);
	check(SkelComponent->SkeletalMesh);

	if(!AnimSeq || !AnimLinkup)
	{
		FillWithRefPose( Atoms, SkelComponent->SkeletalMesh->RefSkeleton );
		return;
	}

	TArray<FMeshBone>& RefSkel = SkelComponent->SkeletalMesh->RefSkeleton;
	INT NumBones = RefSkel.Num();
	check( NumBones == Atoms.Num() );

	for(INT i=0; i<NumBones; i++)
	{
		// Find which track in the sequence we look in for this bones data
		INT TrackIndex = ((FAnimSetMeshLinkup*)AnimLinkup)->BoneToTrackTable(i);

		// If there is no track for this bone, we just use the reference pose.
		if(TrackIndex == INDEX_NONE)
		{
			Atoms(i) = FBoneAtom( RefSkel(i).BonePos.Orientation, RefSkel(i).BonePos.Position, FVector(1.f) );					
		}
		else // Otherwise read it from the sequence.
		{
			AnimSeq->GetBoneAtom( Atoms(i), TrackIndex, CurrentTime, bLooping);

			if(i==0)
			{
				if(bZeroRootTranslationX || bZeroRootTranslationY || bZeroRootTranslationZ)
				{
					FVector WorldTranslation = SkelComponent->LocalToWorld.TransformNormal( Atoms(0).Translation );

					if(bZeroRootTranslationX)
					{
						WorldTranslation.X = 0.f;
					}

					if(bZeroRootTranslationY)
					{
						WorldTranslation.Y = 0.f;
					}

					if(bZeroRootTranslationZ)
					{
						WorldTranslation.Z = 0.f;
					}

					Atoms(0).Translation = SkelComponent->LocalToWorld.InverseTransformNormal(WorldTranslation);
				}
			}

			// Apply quaternion fix for ActorX-exported quaternions.
			if( i > 0 ) 
			{
				Atoms(i).Rotation.W *= -1.0f;
			}
		}
	}
}

void UAnimNodeSequence::DrawDebug(FRenderInterface* RI, FLOAT ParentPosX, FLOAT ParentPosY, FLOAT PosX, FLOAT PosY, FLOAT Weight)
{
	Super::DrawDebug(RI, ParentPosX, ParentPosY, PosX, PosY, Weight);

	FColor NodeColor = AnimDebugColorForWeight(Weight);

	// Draw little animation track
	RI->DrawTile(PosX + 16, PosY, 64, 16, 0.f, 0.f, 1.f, 1.f, NodeColor, DebugTrack);
	if(AnimSeq)
	{
		RI->DrawTile(PosX + 16 + ((CurrentTime/AnimSeq->SequenceLength) * 64), PosY + 8, 16, 16, 0.f, 0.f, 1.f, 1.f, NodeColor, DebugCarat);
	}

	// Print name of animation
	FString AnimText = FString::Printf( TEXT("%s (%1.2f)"), *AnimSeqName, Weight );
	RI->DrawString(PosX + 16, PosY - 8, *AnimText, GEngine->SmallFont, NodeColor );
}

void UAnimNodeSequence::IssueNotifies(FLOAT DeltaTime)
{
	// Do nothing if there are no notifies to play.
	INT NumNotifies = AnimSeq->Notifies.Num();
	if(NumNotifies == 0)
		return;

	// Set current time to current animation position.
	FLOAT WorkTime = CurrentTime;

	// Total interval we're about to proceed CurrentTime forward  (right after this IssueNotifies routine)
	FLOAT TimeToGo = DeltaTime; 

	// First, find the next notify we are going to hit.
	INT NextNotifyIndex = INDEX_NONE;
	FLOAT TimeToNextNotify = BIG_NUMBER;

	// Note - if there are 2 notifies with the same time, it starts with the lower index one.
	for(INT i=0; i<NumNotifies; i++)
	{
		FLOAT TryTimeToNotify = AnimSeq->Notifies(i).Time - WorkTime;
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
	TimeToGo -= TimeToNextNotify;
	WorkTime += TimeToNextNotify;

	// Handle looping...
	if(WorkTime >= AnimSeq->SequenceLength)
	{
		WorkTime -= AnimSeq->SequenceLength;
	}

	// Then keep walking forwards until we run out of time.
	while(TimeToGo > 0.0f)
	{
		//debugf( TEXT("NOTIFY: %d %s"), NextNotifyIndex, *(AnimSeq->Notifies(NextNotifyIndex).Comment) );

		// Execute this notify. NextNotifyIndex will be the soonest notify inside the current TimeToGo interval.
		UAnimNotify* AnimNotify = AnimSeq->Notifies(NextNotifyIndex).Notify;
		if( AnimNotify )
		{
			// Call Notify function
			AnimNotify->Notify( this );
		}
		
		// Then find the next one.
		NextNotifyIndex = (NextNotifyIndex + 1)%NumNotifies; // Assumes notifies are ordered.
		TimeToNextNotify = AnimSeq->Notifies(NextNotifyIndex).Time - WorkTime;

		// Wrap if necessary.
		if( NextNotifyIndex == 0 )
		{
			if(!bLooping)
			{
				// If not looping, nothing more to do if notify is before current working time.
				return;
			}
			else
			{
				// Correct TimeToNextNotify as it lies before WorkTime.
				TimeToNextNotify += AnimSeq->SequenceLength; 
			}
		}

		// Wind on to next notify.
		TimeToGo -= TimeToNextNotify;
		WorkTime = AnimSeq->Notifies(NextNotifyIndex).Time;
	}
}

/** 
 * Set a new animation by name.
 * This will find the UAnimationSequence by name, from the list of AnimSets specified in the SkeletalMeshComponent and cache it
 * Will also store a pointer to the anim track <-> 
 */
void UAnimNodeSequence::SetAnim(FName InSequenceName)
{
	AnimSeqName = InSequenceName;
	AnimSeq = NULL;
	AnimLinkup = NULL;

	if(InSequenceName == NAME_None || !SkelComponent || !SkelComponent->SkeletalMesh)
		return;

	AnimSeq = SkelComponent->FindAnimSequence(AnimSeqName);
	if (AnimSeq != NULL)
	{
		UAnimSet* AnimSet = CastChecked<UAnimSet>( AnimSeq->GetOuter() );
		AnimLinkup = AnimSet->GetMeshLinkup( SkelComponent->SkeletalMesh );
		check( ((FAnimSetMeshLinkup*)AnimLinkup)->SkelMesh == SkelComponent->SkeletalMesh );
		check( ((FAnimSetMeshLinkup*)AnimLinkup)->BoneToTrackTable.Num() == SkelComponent->SkeletalMesh->RefSkeleton.Num() );
	}
	else
	{
		debugf(NAME_Warning,TEXT("%s - Failed to find animsequence '%s'"),
			   GetName(),
			   *InSequenceName);
	}
}

/** Start the current animation playing. This just sets the bPlaying flag to true, so that TickAnim will move CurrentTime forwards. */
void UAnimNodeSequence::PlayAnim(UBOOL bInLoop, FLOAT InRate, FLOAT StartTime)
{
	CurrentTime = StartTime;
	Rate = InRate;
	bLooping = bInLoop;

	bPlaying = true;

	if(bCauseActorAnimPlay && SkelComponent->Owner)
	{
		SkelComponent->Owner->eventOnAnimPlay(this);
	}
}

/** Stop playing current animation. Will set bPlaying to false. */
void UAnimNodeSequence::StopAnim()
{
	bPlaying = false;
}

/** Set the CurrentTime to the supplied position. */
void UAnimNodeSequence::SetPosition( FLOAT NewTime )
{
	CurrentTime = Clamp<FLOAT>( NewTime, 0.f, GetAnimLength() );
}


FLOAT UAnimNodeSequence::GetAnimLength()
{
	if(AnimSeq)
	{
		return AnimSeq->SequenceLength;
	}
	else
	{
		return 0.f;
	}
}

////////////////////////////////
// Script versions
void UAnimNodeSequence::execSetAnim( FFrame& Stack, RESULT_DECL )
{	
	P_GET_NAME( SequenceName );
	P_FINISH;

	SetAnim( SequenceName );
};

void UAnimNodeSequence::execPlayAnim( FFrame& Stack, RESULT_DECL )
{	
	P_GET_UBOOL_OPTX( bLoop, false );
	P_GET_FLOAT_OPTX( InRate, 1.f);	
	P_GET_FLOAT_OPTX( StartTime, 0.f);	
	P_FINISH;

	PlayAnim( bLoop, InRate, StartTime );
};

void UAnimNodeSequence::execStopAnim( FFrame& Stack, RESULT_DECL )
{	
	P_FINISH;

	StopAnim();
};

void UAnimNodeSequence::execSetPosition( FFrame& Stack, RESULT_DECL )
{	
	P_GET_FLOAT( NewTime );
	P_FINISH;

	SetPosition( NewTime );
};

void UAnimNodeSequence::execGetAnimLength( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	*(FLOAT*)Result = GetAnimLength();
}


////////////////////////////////
// Actor latent function

void AActor::execFinishAnim( FFrame &Stack, RESULT_DECL )
{
	P_GET_OBJECT(UAnimNodeSequence, SeqNode);
	P_FINISH;

	GetStateFrame()->LatentAction = EPOLL_FinishAnim;
	LatentSeqNode  = SeqNode;
}

void AActor::execPollFinishAnim( FFrame& Stack, RESULT_DECL )
{
	// Block as long as LatentSeqNode is present and playing.
	if( !LatentSeqNode || !LatentSeqNode->bPlaying )
	{
		GetStateFrame()->LatentAction = 0;
		LatentSeqNode = NULL;
	}
}
IMPLEMENT_FUNCTION( AActor, EPOLL_FinishAnim, execPollFinishAnim );