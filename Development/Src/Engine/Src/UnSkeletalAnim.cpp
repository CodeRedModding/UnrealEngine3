/*=============================================================================
	UnSkeletalAnim.cpp: Skeletal mesh animation functions.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"


IMPLEMENT_CLASS(UAnimSequence);
IMPLEMENT_CLASS(UAnimSet);
IMPLEMENT_CLASS(UAnimNotify);

/*-----------------------------------------------------------------------------
	UAnimSequence
-----------------------------------------------------------------------------*/

void UAnimSequence::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << RawAnimData;
}

void UAnimSequence::PostLoad()
{
	Super::PostLoad();

	// Ensure notifies are sorted.
	SortNotifies();
}

#define MAXPOSDIFF		(0.0001f)
#define MAXANGLEDIFF	(0.0003f)

/**
 * 'Lossless' compression of raw animation data.
 * For the position and rotation arrays, we basically strip each down to just one frame if all frames are identical.
 * If both pos and rot go down to 1 frame, we strip time out as well.
 */
void UAnimSequence::CompressRawAnimData()
{
	RawAnimData.Load();

	for(INT i=0; i<RawAnimData.Num(); i++)
	{
		FRawAnimSequenceTrack& RawTrack = RawAnimData(i);

		// Only bother doing anything if we have some keys!
		if(RawTrack.KeyTimes.Num() == 0)
		{
			check( RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == RawTrack.KeyTimes.Num() );
			check( RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == RawTrack.KeyTimes.Num() );
			
			// Check variation of position keys
			if( RawTrack.PosKeys.Num() > 1 )
			{
				FVector FirstPos = RawTrack.PosKeys(0);
				UBOOL bFramesIdentical = true;
				for(INT j=1; j<RawTrack.PosKeys.Num() && bFramesIdentical; j++)
				{
					if( (FirstPos - RawTrack.PosKeys(j)).Size() > MAXPOSDIFF )
					{
						bFramesIdentical = false;
					}
				}

				// If all keys are the same, remove all but first frame
				if(bFramesIdentical)
				{
					RawTrack.PosKeys.Remove(1, RawTrack.PosKeys.Num()- 1);
					RawTrack.PosKeys.Shrink();
				}
			}

			// Check variation of rotational keys
			if(RawTrack.RotKeys.Num() > 1)
			{
				FQuat FirstRot = RawTrack.RotKeys(0);
				UBOOL bFramesIdentical = true;
				for(INT j=1; j<RawTrack.RotKeys.Num() && bFramesIdentical; j++)
				{
					if( FQuatError(FirstRot, RawTrack.RotKeys(j)) > MAXANGLEDIFF )
					{
						bFramesIdentical = false;
					}
				}

				if(bFramesIdentical)
				{
					RawTrack.RotKeys.Remove(1, RawTrack.RotKeys.Num()- 1);
					RawTrack.RotKeys.Shrink();
				}			
			}

			// If both pos and rot keys are down to just 1 key - clear out the time keys
			if(RawTrack.PosKeys.Num() == 1 && RawTrack.RotKeys.Num() == 1)
			{
				RawTrack.KeyTimes.Remove(1, RawTrack.KeyTimes.Num() - 1);
				RawTrack.KeyTimes.Shrink();
			}
		}
	}
}

/**
 * Interpolate keyframes in this sequence to find the bone transform (relative to parent).
 * 
 * @param OutAtom Output bone transform
 * @param TrackIndex Index of track to interpolat
 * @param Time Time on track to interpolate to
 */
void UAnimSequence::GetBoneAtom(FBoneAtom& OutAtom, INT TrackIndex, FLOAT Time, UBOOL bLooping)
{
	RawAnimData.Load();

	// Bail out (with rather whacky data) if data is empty for some reason.
	if( RawAnimData(TrackIndex).KeyTimes.Num() == 0 || 
		RawAnimData(TrackIndex).PosKeys.Num() == 0 ||
		RawAnimData(TrackIndex).RotKeys.Num() == 0 )
	{
		debugf( TEXT("UAnimSequence::GetBoneAtom : No anim data in AnimSequence!") );
		OutAtom.Rotation = FQuat::Identity;
		OutAtom.Translation = FVector(0.f, 0.f, 0.f);
		OutAtom.Scale = FVector(1.f);
	}

	FRawAnimSequenceTrack& RawTrack = RawAnimData(TrackIndex);	

	OutAtom.Scale = FVector(1.f);

	// This assumes that all keys are equally spaced (ie. won't work if we have dropped unimportant frames etc).
	FLOAT FrameInterval = RawTrack.KeyTimes(1) - RawTrack.KeyTimes(0);

    	// Check for 1-frame, before-first-frame and after-last-frame cases.
	
	if( Time < 0.f || RawTrack.KeyTimes.Num() == 1 )
	{
		OutAtom.Translation = RawTrack.PosKeys(0);
		OutAtom.Rotation = RawTrack.RotKeys(0);
		return;
	}

	INT LastIndex = RawTrack.KeyTimes.Num() - 1;
	if( Time > SequenceLength )
	{
		OutAtom.Translation = RawTrack.PosKeys(LastIndex);
		OutAtom.Rotation = RawTrack.RotKeys(LastIndex);
		return;
	}
	
	// Keyframe we want is somewhere in the actual data. 

	// Find key position as a float.
	FLOAT KeyPos = Time/FrameInterval;

	// Find the integer part (ensuring within range) and that gives us the 'starting' key index.
	INT KeyIndex1 = Clamp<INT>( appFloor(KeyPos), 0, RawTrack.KeyTimes.Num()-1 );

	// The alpha (fractional part) is then just the remainder.
	FLOAT Alpha = KeyPos - (FLOAT)KeyIndex1;

	INT KeyIndex2 = KeyIndex1 + 1;

	// If we have gone over the end, do different things in case of looping
	if(KeyIndex2 == RawTrack.KeyTimes.Num())
	{
		// If looping, interpolate between last and first frame
		if(bLooping)
		{
			KeyIndex2 = 0;
		}
		// If not looping - hold the last frame.
		else
		{
			KeyIndex2 = KeyIndex1;
		}
	}

	OutAtom.Translation = Lerp(RawTrack.PosKeys(KeyIndex1), RawTrack.PosKeys(KeyIndex2), Alpha);

	// Fast linear quaternion interpolation

	// To ensure the 'shortest route', we make sure the dot product between the two keys is positive.
	if( (RawTrack.RotKeys(KeyIndex1) | RawTrack.RotKeys(KeyIndex2)) < 0.f )
	{
		OutAtom.Rotation = (RawTrack.RotKeys(KeyIndex1) * (1.f-Alpha)) + (RawTrack.RotKeys(KeyIndex2) * -Alpha);
	}
	else
	{
		OutAtom.Rotation = (RawTrack.RotKeys(KeyIndex1) * (1.f-Alpha)) + (RawTrack.RotKeys(KeyIndex2) * Alpha);
	}
	OutAtom.Rotation.Normalize();
}

/**
 * Calculate memory footprint of this sequence.
 * 
 * @return Memory (in bytes) used by this sequence.
 */
INT UAnimSequence::GetAnimSequenceMemory()
{
	RawAnimData.Load();

	INT Total = 0;

	for(INT i=0; i<RawAnimData.Num(); i++)
	{
		FRawAnimSequenceTrack& RawTrack = RawAnimData(i);	
		
		Total += RawTrack.KeyTimes.Num() * sizeof(FLOAT);
		Total += RawTrack.PosKeys.Num() * sizeof(FVector);
		Total += RawTrack.RotKeys.Num() * sizeof(FQuat);
	}

	return Total;
}


IMPLEMENT_COMPARE_CONSTREF( FAnimNotifyEvent, UnSkeletalAnim, 
{
	if		(A.Time > B.Time)	return 1;
	else if	(A.Time < B.Time)	return -1;
	else						return 0;
} 
)

/**
 * Sort the Notifies array by time, earliest first.
 */
void UAnimSequence::SortNotifies()
{
	Sort<USE_COMPARE_CONSTREF(FAnimNotifyEvent,UnSkeletalAnim)>(&Notifies(0),Notifies.Num());
}

/*-----------------------------------------------------------------------------
	UAnimSet
-----------------------------------------------------------------------------*/

/**
 * See if we can play sequences from this AnimSet on the provided SkeletalMesh.
 * Returns true if there is a bone in SkelMesh for every track in the AnimSet, or there is a track of animation for every bone of the SkelMesh.
 * 
 * @param SkelMesh SkeletalMesh to check AnimSet against.
 * 
 * @return True if animation set can play on supplied SkeletalMesh, false if not.
 */
UBOOL UAnimSet::CanPlayOnSkeletalMesh(USkeletalMesh* SkelMesh)
{
	// Temporarily allow any animation to play on any AnimSet. 
	// We need a looser metric for matching animation to skeletons. Some 'overlap bone count'?
#if 0
	// First see if there is a bone for all tracks
	UBOOL bBoneForAllTracks = true;
	for(INT i=0; i<TrackBoneNames.Num() && bBoneForAllTracks; i++)
	{
		INT BoneIndex = SkelMesh->MatchRefBone( TrackBoneNames(i) );
		if(BoneIndex == INDEX_NONE)
		{
			bBoneForAllTracks = false;
		}
	}

	UBOOL bTrackForAllBones = true;
	for(INT i=0; i<SkelMesh->RefSkeleton.Num() && bTrackForAllBones; i++)
	{
		INT TrackIndex = FindTrackWithName( SkelMesh->RefSkeleton(i).Name );
		if(TrackIndex == INDEX_NONE)
		{
			bTrackForAllBones = false;
		}
	}

	return bBoneForAllTracks || bTrackForAllBones;
#else
	return true;
#endif
}

/** Find the track index for the bone with the supplied name. Returns INDEX_NONE if no track exists for that bone. */
INT UAnimSet::FindTrackWithName(FName BoneName)
{
	for(INT i=0; i<TrackBoneNames.Num(); i++)
	{
		if( TrackBoneNames(i) == BoneName )
		{
			return i;
		}
	}

	return INDEX_NONE;
}


/**
 * Find an AnimSequence with the given name in this set.
 * 
 * @param SequenceName Name of sequence to find.
 * 
 * @return Pointer to AnimSequence with desired name. NULL if sequence was not found.
 */
UAnimSequence* UAnimSet::FindAnimSequence(FName SequenceName)
{
	for(INT i=0; i<Sequences.Num(); i++)
	{
		if( Sequences(i)->SequenceName == SequenceName )
			return Sequences(i);
	}

	return NULL;
}

/**
 * Find a mesh linkup table (mapping of sequence tracks to bone indices) for a particular SkeletalMesh
 * If one does not already exist, create it now.
 *
 * @param InSkelMesh SkeletalMesh to look for linkup with.
 *
 * @return Linkup between mesh and animation set.
 */
FAnimSetMeshLinkup* UAnimSet::GetMeshLinkup(USkeletalMesh* InSkelMesh)
{
	// First, see if we have a cached link-up between this animation and the given skeletal mesh.
	for(INT i=0; i<LinkupCache.Num(); i++)
	{
		if( LinkupCache(i).SkelMesh == InSkelMesh )
			return &LinkupCache(i);
	}

	// No linkup found - so create one here and add to cache.
	INT NewLinkupIndex = LinkupCache.AddZeroed();
	FAnimSetMeshLinkup* NewLinkup = &LinkupCache(NewLinkupIndex);

	NewLinkup->SkelMesh = InSkelMesh;
	NewLinkup->BoneToTrackTable.Add( InSkelMesh->RefSkeleton.Num() );

	// For each bone in skeletal mesh, find which track to pull from in the AnimSet.
	for(INT i=0; i<NewLinkup->BoneToTrackTable.Num(); i++)
	{
		FName BoneName = InSkelMesh->RefSkeleton(i).Name;

		// FindTrackWithName will return INDEX_NONE if no track exists.
		NewLinkup->BoneToTrackTable(i) = FindTrackWithName(BoneName);
	}

	return NewLinkup;
}

INT UAnimSet::GetAnimSetMemory()
{
	INT Total = 0;

	for(INT i=0; i<Sequences.Num(); i++)
	{
		Total += Sequences(i)->GetAnimSequenceMemory();
	}

	return Total;
}

void UAnimSet::ResetAnimSet()
{
	Sequences.Empty();
	TrackBoneNames.Empty();
}

///////////////////////////////////////
// Thumbnail support

void UAnimSet::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	UTexture2D* Icon = LoadObject<UTexture2D>(NULL, TEXT("EngineMaterials.UnrealEdIcon_AnimSet"), NULL, LOAD_NoFail, NULL);
	InRI->DrawTile( InX, InY, Icon->SizeX*InZoom, Icon->SizeY*InZoom, 0.0f,	0.0f, 1.0f, 1.0f, FLinearColor::White, Icon );
}

FThumbnailDesc UAnimSet::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	UTexture2D* Icon = LoadObject<UTexture2D>(NULL, TEXT("EngineMaterials.UnrealEdIcon_AnimSet"), NULL, LOAD_NoFail, NULL);
	FThumbnailDesc	ThumbnailDesc;
	ThumbnailDesc.Width	= InFixedSz ? InFixedSz : Icon->SizeX*InZoom;
	ThumbnailDesc.Height = InFixedSz ? InFixedSz : Icon->SizeY*InZoom;
	return ThumbnailDesc;
}

INT UAnimSet::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();
	new( *InLabels )FString( GetName() );
	new( *InLabels )FString( FString::Printf(TEXT("%d Sequences"), Sequences.Num()) );
	return InLabels->Num();
}

/*-----------------------------------------------------------------------------
	AnimNotify subclasses
-----------------------------------------------------------------------------*/

//
// UAnimNotify
//
void UAnimNotify::PostEditChange(UProperty* PropertyThatChanged)
{
}

//
// UAnimNotify_Effect
//
void UAnimNotify_Effect::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = NodeSeq->SkelComponent->Owner;

	if( Owner && EffectClass )
	{
		FVector Location = Owner->Location;
		FRotator Rotation = Owner->Rotation;

		USkeletalMeshComponent* ThisMeshComponent = NodeSeq->SkelComponent;
		if( Bone!=NAME_None )
		{
			// Spawn at bone location but don't attach.
			if( ThisMeshComponent && !Attach )
			{
				FMatrix BoneMatrix = ThisMeshComponent->GetBoneMatrix( ThisMeshComponent->MatchRefBone(Bone) );
				FMatrix OffsetMatrix(OffsetLocation, OffsetRotation);
				
				FMatrix NotifyMatrix = OffsetMatrix * BoneMatrix;

				Location = NotifyMatrix.GetOrigin();
				Rotation = NotifyMatrix.Rotator();
			}
		}
		else
		{
			// Spawn relative to skel mesh component location/rotation.
			FMatrix OffsetMatrix(OffsetLocation, OffsetRotation);

			FMatrix NotifyMatrix = ThisMeshComponent->LocalToWorld * OffsetMatrix;

			Location = NotifyMatrix.GetOrigin();
			Rotation = NotifyMatrix.Rotator();
		}

		AActor* EffectActor = Owner->GetLevel()->SpawnActor( EffectClass, NAME_None, Location, Rotation, NULL, !Owner->Level->bBegunPlay, 0, Owner );
		if( !EffectActor )
			return;

		if( Tag != NAME_None )
			EffectActor->Tag = Tag;
		EffectActor->DrawScale = DrawScale;
		EffectActor->DrawScale3D = DrawScale3D;

		if( !Owner->Level->bBegunPlay )
			LastSpawnedEffect = EffectActor;
	}

}
IMPLEMENT_CLASS(UAnimNotify_Effect);

//
// UAnimNotify_DestroyEffect
//
void UAnimNotify_DestroyEffect::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = NodeSeq->SkelComponent->Owner;
	if( Owner && DestroyTag != NAME_None )
	{
		for( INT ActorNum=Owner->GetLevel()->Actors.Num()-1; ActorNum>=0; --ActorNum )
		{
			AActor* Actor = Owner->GetLevel()->Actors(ActorNum);
			if( Actor &&
				Actor->Owner == Owner &&
				Actor->Tag == DestroyTag )
			{
				Owner->GetLevel()->DestroyActor( Actor );
			}
		}
	}
}
IMPLEMENT_CLASS(UAnimNotify_DestroyEffect);

//
// UAnimNotify_Sound
//
void UAnimNotify_Sound::Notify( UAnimNodeSequence* NodeSeq )
{
	USkeletalMeshComponent* SkelComp = NodeSeq->SkelComponent;
	check( SkelComp );

	AActor* Owner = SkelComp->Owner;
	if(Owner)
	{
		UAudioComponent* AudioComponent = UAudioDevice::CreateComponent( SoundCue, SkelComp->Scene, Owner, 0 );
		if( AudioComponent )
		{
			if( !(bFollowActor && Owner) )
			{	
				AudioComponent->bUseOwnerLocation	= 0;
				AudioComponent->Location			= SkelComp->LocalToWorld.GetOrigin();
			}

			AudioComponent->bNonRealtime			= GIsEditor;
			AudioComponent->bAllowSpatialization	&= !GIsEditor;
			AudioComponent->bAutoDestroy			= 1;
			AudioComponent->Play();
		}
	}
}
IMPLEMENT_CLASS(UAnimNotify_Sound);

//
// UAnimNotify_Script
//
void UAnimNotify_Script::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = NodeSeq->SkelComponent->Owner;
	if( Owner && NotifyName != NAME_None )
	{
		if( !Owner->Level->bBegunPlay )
		{
			debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_Script %s"), *NotifyName );
		}
		else
		{
			UFunction* Function = Owner->FindFunction( NotifyName );
			if( Function )
			{
				if( Function->FunctionFlags & FUNC_Delegate )
				{
					UDelegateProperty* DelegateProperty = FindField<UDelegateProperty>( Owner->GetClass(), *FString::Printf(TEXT("__%s__Delegate"),*NotifyName) );
					FScriptDelegate* ScriptDelegate = (FScriptDelegate*)((BYTE*)Owner + DelegateProperty->Offset);
                    Owner->ProcessDelegate( NotifyName, ScriptDelegate, NULL );
				}
				else
				{
					Owner->ProcessEvent( Function, NULL );								
				}
			}
		}
	}

}
IMPLEMENT_CLASS(UAnimNotify_Script);

//
// UAnimNotify_Scripted
//
void UAnimNotify_Scripted::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = NodeSeq->SkelComponent->Owner;
	if( Owner )
	{
		if( !Owner->Level->bBegunPlay )
		{
			debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_Scripted %s"), GetName() );
		}
		else
		{
			eventNotify( Owner, NodeSeq );
		}
	}
}
IMPLEMENT_CLASS(UAnimNotify_Scripted);

//
// UAnimNotify_Footstep
//
void UAnimNotify_Footstep::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = NodeSeq->SkelComponent->Owner;

	if ( !Owner )
	{
		debugf(TEXT("FOOTSTEP no owner"));
	}
	else
	{
		//debugf(TEXT("FOOTSTEP for %s"),Owner->GetName());

		// Act on footstep...  FootDown siginfies which paw hits earth 0-left, 1=right   2=left hindleg etc.
		if ( Owner->GetAPawn() )
		{
			Owner->GetAPawn()->eventPlayFootStepSound(FootDown);
		}
	}
}
IMPLEMENT_CLASS(UAnimNotify_Footstep);
