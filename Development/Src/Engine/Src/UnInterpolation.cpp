/*=============================================================================
	UnInterpolation.cpp: Code for supporting interpolation of properties in-game.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineAnimClasses.h"
#include "EngineParticleClasses.h"
#include "EngineMaterialClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineMeshClasses.h"
#include "EngineSoundClasses.h"
#include "UnLinkedObjDrawUtils.h"
#include "UnInterpolationHitProxy.h"
#include "EngineAIClasses.h"
#include "EngineDecalClasses.h"
#include "AVIWriter.h"

#if WITH_FACEFX
using namespace OC3Ent;
using namespace Face;
#endif

// Priority with which to display sounds triggered by Matinee sound tracks.
#define SUBTITLE_PRIORITY_MATINEE	10000

IMPLEMENT_CLASS(AInterpActor);
IMPLEMENT_CLASS(AMatineeActor);

IMPLEMENT_CLASS(USeqAct_Interp);

IMPLEMENT_CLASS(UInterpData);

IMPLEMENT_CLASS(UInterpGroup);
IMPLEMENT_CLASS(UInterpGroupInst);

IMPLEMENT_CLASS(UInterpGroupDirector);
IMPLEMENT_CLASS(UInterpGroupInstDirector);

IMPLEMENT_CLASS(UInterpGroupAI);
IMPLEMENT_CLASS(UInterpGroupInstAI);

IMPLEMENT_CLASS(UInterpGroupCamera);
IMPLEMENT_CLASS(UInterpGroupInstCamera);

IMPLEMENT_CLASS(UInterpTrack);
IMPLEMENT_CLASS(UInterpTrackInst);

IMPLEMENT_CLASS(UInterpTrackInstProperty);

IMPLEMENT_CLASS(UInterpTrackMove);
IMPLEMENT_CLASS(UInterpTrackInstMove);

IMPLEMENT_CLASS(UInterpTrackMoveAxis);

IMPLEMENT_CLASS(UInterpTrackToggle);
IMPLEMENT_CLASS(UInterpTrackInstToggle);

IMPLEMENT_CLASS(UInterpTrackFloatBase);
IMPLEMENT_CLASS(UInterpTrackVectorBase);
IMPLEMENT_CLASS(UInterpTrackLinearColorBase);

IMPLEMENT_CLASS(UInterpTrackFloatProp);
IMPLEMENT_CLASS(UInterpTrackInstFloatProp);

IMPLEMENT_CLASS(UInterpTrackVectorProp);
IMPLEMENT_CLASS(UInterpTrackInstVectorProp);

IMPLEMENT_CLASS(UInterpTrackBoolProp);
IMPLEMENT_CLASS(UInterpTrackInstBoolProp);

IMPLEMENT_CLASS(UInterpTrackColorProp);
IMPLEMENT_CLASS(UInterpTrackInstColorProp);

IMPLEMENT_CLASS(UInterpTrackLinearColorProp);
IMPLEMENT_CLASS(UInterpTrackInstLinearColorProp);

IMPLEMENT_CLASS(UInterpTrackEvent);
IMPLEMENT_CLASS(UInterpTrackInstEvent);

IMPLEMENT_CLASS(UInterpTrackNotify);
IMPLEMENT_CLASS(UInterpTrackInstNotify);

IMPLEMENT_CLASS(UInterpTrackDirector);
IMPLEMENT_CLASS(UInterpTrackInstDirector);

IMPLEMENT_CLASS(UInterpTrackFade);
IMPLEMENT_CLASS(UInterpTrackInstFade);

IMPLEMENT_CLASS(UInterpTrackSlomo);
IMPLEMENT_CLASS(UInterpTrackInstSlomo);

IMPLEMENT_CLASS(UInterpTrackAnimControl);
IMPLEMENT_CLASS(UInterpTrackInstAnimControl);

IMPLEMENT_CLASS(UInterpTrackSound);
IMPLEMENT_CLASS(UInterpTrackInstSound);

IMPLEMENT_CLASS(UInterpTrackFloatParticleParam);
IMPLEMENT_CLASS(UInterpTrackInstFloatParticleParam);

IMPLEMENT_CLASS(UInterpTrackFloatMaterialParam);
IMPLEMENT_CLASS(UInterpTrackInstFloatMaterialParam);

IMPLEMENT_CLASS(UInterpTrackVectorMaterialParam);
IMPLEMENT_CLASS(UInterpTrackInstVectorMaterialParam);

IMPLEMENT_CLASS(UInterpTrackColorScale);
IMPLEMENT_CLASS(UInterpTrackInstColorScale);

IMPLEMENT_CLASS(UInterpTrackFaceFX);
IMPLEMENT_CLASS(UInterpTrackInstFaceFX);

IMPLEMENT_CLASS(UInterpTrackMorphWeight);
IMPLEMENT_CLASS(UInterpTrackInstMorphWeight);

IMPLEMENT_CLASS(UInterpTrackSkelControlScale);
IMPLEMENT_CLASS(UInterpTrackInstSkelControlScale);

IMPLEMENT_CLASS(UInterpTrackAudioMaster);
IMPLEMENT_CLASS(UInterpTrackInstAudioMaster);

IMPLEMENT_CLASS(UInterpTrackVisibility);
IMPLEMENT_CLASS(UInterpTrackInstVisibility);

IMPLEMENT_CLASS(UInterpTrackHeadTracking);
IMPLEMENT_CLASS(UInterpTrackInstHeadTracking);

IMPLEMENT_CLASS(UInterpTrackParticleReplay);
IMPLEMENT_CLASS(UInterpTrackInstParticleReplay);

IMPLEMENT_CLASS(UInterpTrackSkelControlStrength);
IMPLEMENT_CLASS(UInterpTrackInstSkelControlStrength);
/*-----------------------------------------------------------------------------
	Macros for making arrays-of-structs type tracks easier
-----------------------------------------------------------------------------*/

#define STRUCTTRACK_GETNUMKEYFRAMES( TrackClass, KeyArray ) \
INT TrackClass::GetNumKeyframes() const \
{ \
	return KeyArray.Num(); \
}

#define STRUCTTRACK_GETTIMERANGE( TrackClass, KeyArray, TimeVar ) \
void TrackClass::GetTimeRange(FLOAT& StartTime, FLOAT& EndTime) const \
{ \
	if(KeyArray.Num() == 0) \
	{ \
		StartTime = 0.f; \
		EndTime = 0.f; \
	} \
	else \
	{ \
		StartTime = KeyArray(0).TimeVar; \
		EndTime = KeyArray( KeyArray.Num()-1 ).TimeVar; \
	} \
}

// The default implementation returns the time of the last keyframe.
#define STRUCTTRACK_GETTRACKENDTIME( TrackClass, KeyArray, TimeVar ) \
FLOAT TrackClass::GetTrackEndTime() const \
{ \
	return KeyArray.Num() ? KeyArray(KeyArray.Num() - 1).TimeVar : 0.0f; \
} 

#define STRUCTTRACK_GETKEYFRAMETIME( TrackClass, KeyArray, TimeVar ) \
FLOAT TrackClass::GetKeyframeTime(INT KeyIndex) const \
{ \
	if( KeyIndex < 0 || KeyIndex >= KeyArray.Num() ) \
	{ \
		return 0.f; \
	} \
 	return KeyArray(KeyIndex).TimeVar; \
}

#define STRUCTTRACK_SETKEYFRAMETIME( TrackClass, KeyArray, TimeVar, KeyType ) \
INT TrackClass::SetKeyframeTime(INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder) \
{ \
	if( KeyIndex < 0 || KeyIndex >= KeyArray.Num() ) \
	{ \
		return KeyIndex; \
	} \
	if(bUpdateOrder) \
	{ \
		/* First, remove cut from track */ \
		KeyType MoveKey = KeyArray(KeyIndex); \
		KeyArray.Remove(KeyIndex); \
		/* Set its time to the new one. */ \
		MoveKey.TimeVar = NewKeyTime; \
		/* Find correct new position and insert. */ \
		INT i=0; \
		for( i=0; i<KeyArray.Num() && KeyArray(i).TimeVar < NewKeyTime; i++); \
		KeyArray.InsertZeroed(i); \
		KeyArray(i) = MoveKey; \
		return i; \
	} \
	else \
	{ \
		KeyArray(KeyIndex).TimeVar = NewKeyTime; \
		return KeyIndex; \
	} \
}

#define STRUCTTRACK_REMOVEKEYFRAME( TrackClass, KeyArray ) \
void TrackClass::RemoveKeyframe(INT KeyIndex) \
{ \
	if( KeyIndex < 0 || KeyIndex >= KeyArray.Num() ) \
	{ \
		return; \
	} \
	KeyArray.Remove(KeyIndex); \
}

#define STRUCTTRACK_DUPLICATEKEYFRAME( TrackClass, KeyArray, TimeVar, KeyType ) \
INT TrackClass::DuplicateKeyframe(INT KeyIndex, FLOAT NewKeyTime) \
{ \
	if( KeyIndex < 0 || KeyIndex >= KeyArray.Num() ) \
	{ \
		return INDEX_NONE; \
	} \
	KeyType NewKey = KeyArray(KeyIndex); \
	NewKey.TimeVar = NewKeyTime; \
	/* Find the correct index to insert this key. */ \
	INT i=0; for( i=0; i<KeyArray.Num() && KeyArray(i).TimeVar < NewKeyTime; i++); \
	KeyArray.InsertZeroed(i); \
	KeyArray(i) = NewKey; \
	return i; \
}

#define STRUCTTRACK_GETCLOSESTSNAPPOSITION( TrackClass, KeyArray, TimeVar ) \
UBOOL TrackClass::GetClosestSnapPosition(FLOAT InPosition, TArray<INT> &IgnoreKeys, FLOAT& OutPosition) \
{ \
	if(KeyArray.Num() == 0) \
	{ \
		return false; \
	} \
	UBOOL bFoundSnap = false; \
	FLOAT ClosestSnap = 0.f; \
	FLOAT ClosestDist = BIG_NUMBER; \
	for(INT i=0; i<KeyArray.Num(); i++) \
	{ \
		if(!IgnoreKeys.ContainsItem(i)) \
		{ \
			FLOAT Dist = Abs( KeyArray(i).TimeVar - InPosition ); \
			if(Dist < ClosestDist) \
			{ \
				ClosestSnap = KeyArray(i).TimeVar; \
				ClosestDist = Dist; \
				bFoundSnap = true; \
			} \
		} \
	} \
	OutPosition = ClosestSnap; \
	return bFoundSnap; \
}

/*-----------------------------------------------------------------------------
	InterpTools
-----------------------------------------------------------------------------*/

namespace InterpTools
{
	/**
	 * Removes any extraneous text that Matinee includes when storing 
	 * the property name, such as the owning struct or component.
	 *
	 * @param	PropertyName	The property name to prune. 
	 */
	FName PruneInterpPropertyName( const FName& PropertyName )
	{
		FString PropertyString = PropertyName.ToString();

		// Check to see if there is a period in the name, which is the case 
		// for structs and components that own interp variables. In these  
		// cases, we want to cut off the preceeding text up and the period.
		INT PeriodPosition = PropertyString.InStr(TEXT("."));

		if(PeriodPosition != INDEX_NONE)
		{
			// We found a period; Only capture the text after the 
			// period, which represents the actual property name.
			PropertyString = PropertyString.Mid( PeriodPosition + 1 );
		}

		return FName(*PropertyString);
	}

	/**
	 * Toggles the post process override flag on for the camera actor if the given actor 
	 * is a camera actor and the given property name is a post process setting.
	 *
	 * @param	Actor			The camera actor to set the post process setting.
	 * @param	PropertyName	The post process property name to override.
	 */
	void EnableCameraPostProcessFlag( AActor* Actor, const FName& PropertyName )
	{
		// The actor should at least be non-NULL.
		check(Actor);

		if( Actor->IsA(ACameraActor::StaticClass()) )
		{
			ACameraActor* CameraActor = CastChecked<ACameraActor>(Actor);
			CameraActor->CamOverridePostProcess.EnableOverrideSetting( PruneInterpPropertyName(PropertyName) );
		}
	}
	void DisableCameraPostProcessFlag( AActor* Actor, const FName& PropertyName )
	{
		// The actor should at least be non-NULL.
		check(Actor);

		if( Actor->IsA(ACameraActor::StaticClass()) )
		{
			ACameraActor* CameraActor = CastChecked<ACameraActor>(Actor);
			CameraActor->CamOverridePostProcess.DisableOverrideSetting( PruneInterpPropertyName(PropertyName) );
		}
	}
}

/*-----------------------------------------------------------------------------
	AActor
-----------------------------------------------------------------------------*/

UBOOL AActor::FindInterpMoveTrack(UInterpTrackMove** OutMoveTrack, UInterpTrackInstMove** OutMoveTrackInst, USeqAct_Interp** OutSeq)
{
	for(INT i=0; i<LatentActions.Num(); i++)
	{
		USeqAct_Interp* InterpAct = Cast<USeqAct_Interp>(LatentActions(i) );
		if(InterpAct)
		{
			UInterpGroupInst* GrInst = InterpAct->FindGroupInst(this);

			// now this function is called in multiple places - i.e. UpdateTransform in SkeletalMeshComponent for attached
			// where during replication, it can be out of order, so now we expect this can be NULL
			if (GrInst)
			{
				check(GrInst->Group); // Should be based on an InterpGroup
				check(GrInst->TrackInst.Num() == GrInst->Group->InterpTracks.Num()); // Check number of tracks match up.

				for(INT j=0; j<GrInst->Group->InterpTracks.Num(); j++)
				{
					UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>( GrInst->Group->InterpTracks(j) );
					if(MoveTrack && !MoveTrack->IsDisabled() )
					{
						*OutMoveTrack = MoveTrack;
						*OutMoveTrackInst = CastChecked<UInterpTrackInstMove>( GrInst->TrackInst(j) );
						*OutSeq = InterpAct;
						return true;
					}
				}
			}
		}
	}

	*OutMoveTrack = NULL;
	*OutMoveTrackInst = NULL;
	*OutSeq = NULL;
	return false;
}

INT AMatineeActor::GetGroupActor(FName GroupName)
{
	if ( GroupName!=NAME_None )
	{
		// this is hardcodede
		for (INT I=0; I<UCONST_MAX_AIGROUP_NUMBER; ++I)
		{
			if (AIGroupNames[I] == GroupName)
			{
				return I;
			}
		}
	}

	return -1;
}

UBOOL AMatineeActor::ClientInitializeAIGroupActors()
{
	// change this logic to return TRUE if ALL are initialized
	// that way we can allow later spawning actor in the matinee
	// ai group does not allow change actor after initialized,
 	UBOOL bAllDone=TRUE;

	// if all initialized, just check here and return TRUE
	for (INT I=0; I<UCONST_MAX_AIGROUP_NUMBER; ++I)
	{
		if (AIGroupNames[I] != NAME_None && AIGroupInitStage[I] != 2)
		{
			bAllDone=FALSE;
			break;
		}
	}

	if (bAllDone)
	{
		return TRUE;
	}
	
	// initialize below and set init stage to be 1, so that it can be second initialized after
	for (INT GroupID = 0; GroupID < InterpAction->GroupInst.Num(); ++GroupID)
	{
		UInterpGroupInstAI* AIGroupInst = Cast<UInterpGroupInstAI>(InterpAction->GroupInst(GroupID));
		if ( AIGroupInst && AIGroupInst->AIGroup)
		{
			INT Index = GetGroupActor(AIGroupInst->AIGroup->GroupName);
			// if index is valid and pawn exists and it's not initialized
			if ( Index!=-1 &&  AIGroupPawns[Index]!=NULL && AIGroupInitStage[Index]==0 )
			{
//				debugf(TEXT("Initializing AIGroup(%s) - Actor(%s)"), *AIGroupInst->AIGroup->GroupName.GetNameString(), *AIGroupPawns[Index]->GetName());
				//GroupMapToInit.Set(AIGroupInst, GroupPawn);
				AIGroupInst->InitGroupInst(AIGroupInst->AIGroup, AIGroupPawns[Index]);		
				// set init stage to be 1
				AIGroupInitStage[Index] = 1;

				for(INT j=0; j<AIGroupInst->TrackInst.Num(); j++)
				{
					UInterpTrackInst* TrInst = AIGroupInst->TrackInst(j);
					UInterpTrackInstMove* MoveInst = Cast<UInterpTrackInstMove>(TrInst);
					if(MoveInst)
					{
						// initialize to zero
						MoveInst->CalcInitialTransform( AIGroupInst->Group->InterpTracks(j), TRUE );
					}
				}
			}

		}
	}
	
	return FALSE;
}

void AMatineeActor::AddAIGroupActor(UInterpGroupInstAI * AIGroupInst)
{
	if (AIGroupInst->AIGroup)
	{
		FName GroupName = AIGroupInst->AIGroup->GroupName;
		APawn * GroupActor = GetPawn(AIGroupInst->GetGroupActor());
		if ( GroupName!=NAME_None ) 	
		{
//			debugf(TEXT("AIGroup: Adding Actor %s (group - %s)to the MatineeActor for replication "), *GroupActor->GetName(), *GroupName.GetNameString());

			INT FirstEmptyIndex=-1;
			// this is hardcodede
			for (INT I=0; I<10; ++I)
			{
				if (AIGroupNames[I] == GroupName)
				{
					AIGroupPawns[I] = GroupActor;
					return;
				}
				// set first empty index
				else if (AIGroupNames[I] == NAME_None)
				{
					// get out
					FirstEmptyIndex = I;
					break;
				}
			}

			if (FirstEmptyIndex != -1)
			{
				// set info - assume it will start from beginning
				AIGroupNames[FirstEmptyIndex] = GroupName;
				AIGroupPawns[FirstEmptyIndex] = GroupActor;
			}
			else
			{
				// all spots are occupied
				// print error
				debugf(NAME_Error, TEXT("Matinee Replication maxed out AI Groups - (%s, %s) will not be replicated."), *GroupName.GetNameString(), *GroupActor->GetName());
			}
		}
	}
}

void AMatineeActor::TickSpecial(FLOAT DeltaTime)
{
	Super::TickSpecial(DeltaTime);

	if (Role < ROLE_Authority && bIsPlaying && InterpAction != NULL)
	{
		InterpAction->StepInterp(DeltaTime, false);
	}
}

void AInterpActor::TickSpecial(FLOAT DeltaSeconds)
{
	Super::TickSpecial(DeltaSeconds);

	// handle AI notifications
	if (bMonitorMover)
	{
		if (Velocity.IsZero())
		{
			bMonitorMover = false;
			for (AController* C = GWorld->GetWorldInfo()->ControllerList; C != NULL; C = C->NextController)
			{
				if (C->PendingMover == this)
				{
					bMonitorMover = !C->eventMoverFinished() || bMonitorMover;
				}
			}
			MaxZVelocity = 0.f;
		}
		else
		{
			MaxZVelocity = ::Max(MaxZVelocity, Velocity.Z);
			if (bMonitorZVelocity && Velocity.Z > 0.f && MaxZVelocity > 2.f * Velocity.Z)
			{
				bMonitorMover = false;
				for (AController* C = GWorld->GetWorldInfo()->ControllerList; C != NULL; C = C->NextController)
				{
					if (C->PendingMover == this)
					{
						bMonitorMover = !C->eventMoverFinished() || bMonitorMover;
					}
				}
				MaxZVelocity = 0.f;
				bMonitorZVelocity = bMonitorMover;
			}
		}
	}
	else
	{
		MaxZVelocity = 0.f;
	}
}

/** Consider Controller if this gets called **/
UBOOL APawn::FindInterpMoveTrack(UInterpTrackMove** OutMoveTrack, UInterpTrackInstMove** OutMoveTrackInst, USeqAct_Interp** OutSeq)
{
	// Better efficient way?
	TArray<USeqAct_Latent*>  InLatentActions = LatentActions;
	if ( Controller )
	{
		InLatentActions.Append(Controller->LatentActions);
	}

	for(INT i=0; i<InLatentActions.Num(); i++)
	{
		USeqAct_Interp* InterpAct = Cast<USeqAct_Interp>(InLatentActions(i) );
		if(InterpAct)
		{
			UInterpGroupInst* GrInst = InterpAct->FindGroupInst(this);
			// now this function is called in multiple places - i.e. UpdateTransform in SkeletalMeshComponent for attached
			// where during replication, it can be out of order, so now we expect this can be NULL
			if (GrInst)
			{
				check(GrInst->Group); // Should be based on an InterpGroup
				check(GrInst->TrackInst.Num() == GrInst->Group->InterpTracks.Num()); // Check number of tracks match up.

				for(INT j=0; j<GrInst->Group->InterpTracks.Num(); j++)
				{
					UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>( GrInst->Group->InterpTracks(j) );
					if(MoveTrack && !MoveTrack->IsDisabled())
					{
						*OutMoveTrack = MoveTrack;
						*OutMoveTrackInst = CastChecked<UInterpTrackInstMove>( GrInst->TrackInst(j) );
						*OutSeq = InterpAct;
						return true;
					}
				}
			}
		}
	}

	*OutMoveTrack = NULL;
	*OutMoveTrackInst = NULL;
	*OutSeq = NULL;
	return false;
}

void USeqAct_Interp::Initialize()
{
	Super::Initialize();

	// if we started out playing (generally because this is in a level that was streamed out and back in without being GC'ed)
	// then we need to initialize playback immediately
	if (bIsPlaying)
	{
		InitInterp();
		for (INT i = 0; i < LatentActors.Num(); i++)
		{
			if (LatentActors(i) != NULL)
			{
				LatentActors(i)->LatentActions.AddItem(this);
			}
		}
		// if we are a server, create/update matinee actor for replication to clients
		if (!bClientSideOnly && GWorld->GetNetMode() != NM_Client)
		{
			if ((ReplicatedActor == NULL || ReplicatedActor->bDeleteMe) && ReplicatedActorClass != NULL)
			{
				ReplicatedActor = (AMatineeActor*)GWorld->SpawnActor(ReplicatedActorClass);
				check(ReplicatedActor != NULL);
				ReplicatedActor->InterpAction = this;
			}
			if (ReplicatedActor != NULL)
			{
				ReplicatedActor->eventUpdate();
			}
		}
	}
}

/** called when the level that contains this sequence object is being removed/unloaded */
void USeqAct_Interp::CleanUp()
{
	Super::CleanUp();

	TermInterp();
	if (ReplicatedActor != NULL)
	{
		GWorld->DestroyActor(ReplicatedActor);
	}
	// clear from all actors LatentActions list, so they don't try to reference us again
	//@todo: maybe all latent actions should do this?
	for (INT i = 0; i < LatentActors.Num(); i++)
	{
		if (LatentActors(i) != NULL)
		{
			LatentActors(i)->LatentActions.RemoveItem(this);
		}
	}
}

void USeqAct_Interp::InitGroupActorForGroup(class UInterpGroup* InGroup, class AActor* GroupActor)
{
	// Create a new variable connector on all Matinee's using this data.
	USequence* RootSeq = Cast<USequence>(GetOuter());
	if (RootSeq == NULL)
	{
		RootSeq = ParentSequence;
	}
	check(RootSeq);
	RootSeq->UpdateInterpActionConnectors();

	// Find the newly created connector on this SeqAct_Interp. Should always have one now!
	if(GroupActor)
	{
		USeqVar_Object* NewObjVar = ConstructObject<USeqVar_Object>( USeqVar_Object::StaticClass(), RootSeq, NAME_None, RF_Transactional );

#if WITH_EDITORONLY_DATA
		NewObjVar->ObjPosX = ObjPosX + 50 * VariableLinks.Num();
		NewObjVar->ObjPosY = ObjPosY + DrawHeight;
#endif // WITH_EDITORONLY_DATA
		NewObjVar->ObjValue = GroupActor;
		NewObjVar->OnCreated();

		RootSeq->SequenceObjects.AddItem(NewObjVar);

		InitSeqObjectForGroup(InGroup, NewObjVar);
	}
}

void USeqAct_Interp::InitSeqObjectForGroup(class UInterpGroup* InGroup, USequenceObject * SequenceObject)
{
	USequence* RootSeq = Cast<USequence>(GetOuter());
	if (RootSeq == NULL)
	{
		RootSeq = ParentSequence;
	}
	check(RootSeq);
	RootSeq->UpdateInterpActionConnectors();

	USeqVar_Object * SeqVar = Cast<USeqVar_Object>(SequenceObject);
	if ( SeqVar )
	{
		// Find the newly created connector on this SeqAct_Interp. Should always have one now!
		const INT NewLinkIndex = FindConnectorIndex(InGroup->GroupName.ToString(), LOC_VARIABLE );
		check(NewLinkIndex != INDEX_NONE);
		FSeqVarLink* NewLink = &(VariableLinks(NewLinkIndex));

		// Set the new variable connector to point to the new variable.
		NewLink->LinkedVariables.AddItem(SeqVar);
	}
}

/*-----------------------------------------------------------------------------
  UInterpData
-----------------------------------------------------------------------------*/
/**
 * This function is being called after all objects referenced by this object have been serialized.
 */
void UInterpData::PostLoad(void)
{
	Super::PostLoad();

	// Ensure the cached director group is emptied out
	CachedDirectorGroup = NULL;

#if WITH_EDITOR
	UpdateBakeAndPruneStatus();
#endif

	// If in the game, cache off the director group intentionally to avoid
	// frequent searches for it
	if ( GIsGame )
	{
		for( INT i = 0; i < InterpGroups.Num(); ++i )
		{
			UInterpGroupDirector* TestDirGroup = Cast<UInterpGroupDirector>( InterpGroups(i) );
			if( TestDirGroup )
			{
				check( !CachedDirectorGroup ); // Should only have 1 DirectorGroup at most!
				CachedDirectorGroup = TestDirGroup;
			}
		}
	}
}

FString UInterpData::GetValueStr()
{
	return FString::Printf( TEXT("Matinee Data (%3.1fs)"), InterpLength );
}

/** Search through all InterpGroups in this InterpData to find a group whose GroupName matches the given name. Returns NULL if not group found. */
INT UInterpData::FindGroupByName(FName InGroupName)
{
	if(InGroupName != NAME_None)
	{
		for(INT i=0; i<InterpGroups.Num(); i++)
		{
			const FName& GroupName = InterpGroups(i)->GroupName;
			if( GroupName == InGroupName )
			{
				return i;
			}
		}
	}

	return INDEX_NONE;
}

/** Search through all InterpGroups in this InterpData to find a group whose GroupName matches the given name. Returns NULL if not group found. */
INT UInterpData::FindGroupByName(const FString& InGroupName)
{
	for(INT i=0; i<InterpGroups.Num(); i++)
	{
		const FName& GroupName = InterpGroups(i)->GroupName;
		if( GroupName.ToString() == InGroupName )
		{
			return i;
		}
	}

	return INDEX_NONE;
}

/** Search through all groups to find all tracks of the given class. */
void UInterpData::FindTracksByClass(UClass* TrackClass, TArray<UInterpTrack*>& OutputTracks)
{
	for(INT i=0; i<InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = InterpGroups(i);
		Group->FindTracksByClass(TrackClass, OutputTracks);
	}
}

/** Find a DirectorGroup in the data. Should only ever be 0 or 1 of these! */
UInterpGroupDirector* UInterpData::FindDirectorGroup()
{
	UInterpGroupDirector* DirGroup = NULL;

	// If not in game, recheck all the interp groups to ensure there's either zero or one
	// director group and that it hasn't changed
	if ( !GIsGame )
	{
		for(INT i=0; i<InterpGroups.Num(); i++)
		{
			UInterpGroupDirector* TestDirGroup = Cast<UInterpGroupDirector>( InterpGroups(i) );
			if(TestDirGroup)
			{
				check(!DirGroup); // Should only have 1 DirectorGroup at most!
				DirGroup = TestDirGroup;
			}
		}
	}

	// If in game, just use the cached director group, as it cannot have changed
	else
	{
		DirGroup = CachedDirectorGroup;
	}

	return DirGroup;
}

/** Get all the names of events in EventTracks generated by this InterpData. */
void UInterpData::GetAllEventNames(TArray<FName>& OutEventNames)
{
	TArray<UInterpTrack*> Tracks;
	FindTracksByClass(UInterpTrackEvent::StaticClass(), Tracks);

	for(INT i=0; i<Tracks.Num(); i++)
	{
		UInterpTrackEvent* EventTrack = CastChecked<UInterpTrackEvent>( Tracks(i) );

		for(INT j=0; j<EventTrack->EventTrack.Num(); j++)
		{
			OutEventNames.AddUniqueItem( EventTrack->EventTrack(j).EventName );
		}
	}
}

#if WITH_EDITOR
void UInterpData::UpdateBakeAndPruneStatus()
{
	if (!GIsEditor)
	{
		return;
	}

	// Check for anim sets that are referenced
	TMap<FString,UBOOL> UsedAnimSetNames;
	TMap<FString,UBOOL> GroupAnimSetNames;
	TArray<FString> FoundAnimSetNames;
	for (INT GroupIdx = 0; GroupIdx < InterpGroups.Num(); GroupIdx++)
	{
		UInterpGroup* Group = InterpGroups(GroupIdx);
		if (Group != NULL)
		{
			if (Group->GroupAnimSets.Num() > 0)
			{
				for (INT DumpIdx = 0; DumpIdx < Group->GroupAnimSets.Num(); DumpIdx++)
				{
					UAnimSet* FoundSet = Group->GroupAnimSets(DumpIdx);
					if (FoundSet != NULL)
					{
						GroupAnimSetNames.Set(FoundSet->GetPathName(), TRUE);
						FoundAnimSetNames.AddUniqueItem(FoundSet->GetPathName());
					}
				}
			}
			// Iterate over all tracks to find anim control tracks and their anim sequences.
			// We only want to tag animsets that are acutally used.
			for (INT TrackIndex = 0; TrackIndex < Group->InterpTracks.Num(); TrackIndex++)
			{
				UInterpTrack* InterpTrack = Group->InterpTracks(TrackIndex);
				UInterpTrackAnimControl* AnimControl = Cast<UInterpTrackAnimControl>(InterpTrack);				
				if (AnimControl != NULL)
				{
					// Iterate over all track key/ sequences and find the associated sequence.
					for (INT TrackKeyIndex = 0; TrackKeyIndex < AnimControl->AnimSeqs.Num(); TrackKeyIndex++)
					{
						const FAnimControlTrackKey& TrackKey = AnimControl->AnimSeqs(TrackKeyIndex);
						UAnimSequence* AnimSequence = AnimControl->FindAnimSequenceFromName(TrackKey.AnimSeqName);
						if ((AnimSequence != NULL) && (AnimSequence->GetAnimSet() != NULL))
						{
							UsedAnimSetNames.Set(AnimSequence->GetAnimSet()->GetPathName(), TRUE);
							FoundAnimSetNames.AddUniqueItem(AnimSequence->GetAnimSet()->GetPathName());
						}
					}
				}
			}
		}
	}

	// Add any new ones found
	for (INT AnimSetIdx = 0; AnimSetIdx < FoundAnimSetNames.Num(); AnimSetIdx++)
	{
		FString AnimSetName = FoundAnimSetNames(AnimSetIdx);
		UBOOL bUsed = (UsedAnimSetNames.Find(AnimSetName) != NULL);
		UBOOL bInGroupList = (GroupAnimSetNames.Find(AnimSetName) != NULL);
		UBOOL bFound = FALSE;
		for (INT CheckIdx = 0; CheckIdx < BakeAndPruneStatus.Num(); CheckIdx++)
		{
			FAnimSetBakeAndPruneStatus& Status = BakeAndPruneStatus(CheckIdx);
			if (Status.AnimSetName == AnimSetName)
			{
				bFound = TRUE;
				Status.bReferencedButUnused = !bUsed;
				break;
			}
		}
		if (bFound == FALSE)
		{
			// Add it
			INT NewIdx = BakeAndPruneStatus.AddZeroed();
			FAnimSetBakeAndPruneStatus& Status = BakeAndPruneStatus(NewIdx);
			Status.AnimSetName = AnimSetName;
			Status.bSkipBakeAndPrune = FALSE;
			Status.bReferencedButUnused = !bUsed;
		}
	}
}
#endif

/*-----------------------------------------------------------------------------
  USeqAct_Interp
-----------------------------------------------------------------------------*/

void USeqAct_Interp::UpdateObject()
{
	Modify();

	// only update input links for matinee as everything else is generated based on the matinee data
	USequenceOp *DefaultOp = GetArchetype<USequenceOp>();
	checkSlow(DefaultOp);

	// check for changes to input links
	{
		if (InputLinks.Num() < DefaultOp->InputLinks.Num())
		{
			// add room for new entries
			InputLinks.AddZeroed(DefaultOp->InputLinks.Num()-InputLinks.Num());
		}
		else
		if (InputLinks.Num() > DefaultOp->InputLinks.Num())
		{
			if (DefaultOp->InputLinks.Num() == 0)
			{
				InputLinks.Empty();
			}
			else
			{
				// remove the extra entries
				InputLinks.Remove(DefaultOp->InputLinks.Num()-1,InputLinks.Num()-DefaultOp->InputLinks.Num());
			}
		}
		// match up the properties of all the input links
		for (INT Idx = 0; Idx < InputLinks.Num(); Idx++)
		{
			InputLinks(Idx).LinkDesc = DefaultOp->InputLinks(Idx).LinkDesc;
		}
	}
	// check for changes to output links
	{
		INT NumOfEventNames = 0;
		TArray<FName> EventNames;
		UInterpData* const Data = FindInterpDataFromVariable();
		if (Data)
		{
			Data->GetAllEventNames(EventNames);
			NumOfEventNames = EventNames.Num();
		}

		// Number of output links that were saved with our previous version
		INT NumberOfSavedOutputLinks = OutputLinks.Num() - NumOfEventNames;

		// if the number of output links that were saved with our previous version is less than
		// the number of output links that we have inside our current version, we need to make room
		// for the new links
		if (NumberOfSavedOutputLinks < DefaultOp->OutputLinks.Num())
		{
			// add room for new entries
			OutputLinks.InsertZeroed(NumberOfSavedOutputLinks, DefaultOp->OutputLinks.Num() - NumberOfSavedOutputLinks);
		}
		// if the number of output links that were saved with our previous version is greater than
		// the number of output links that we have inside our current version, we need to remove
		// some links
		else if (NumberOfSavedOutputLinks > DefaultOp->OutputLinks.Num())
		{
			// if we don't have any output links, clear just the output links and not the event links
			if (DefaultOp->OutputLinks.Num() == 0)
			{
				OutputLinks.Remove(0,NumberOfSavedOutputLinks);
			}
			else // remove only the changed default output links and leave the event links the same
			{	
				// index is the position right after the default number of links that we have
				INT index = DefaultOp->OutputLinks.Num()-1;
				// count is the total output links that we need to remove
				INT count = NumberOfSavedOutputLinks - DefaultOp->OutputLinks.Num();
				if (count > 0)
				{
					// remove the extra entries
					OutputLinks.Remove(index,count);
				}
			}
		}
		// match up the properties of all the output links
		for (INT Idx = 0; Idx < DefaultOp->OutputLinks.Num(); Idx++)
		{
			OutputLinks(Idx).LinkDesc = DefaultOp->OutputLinks(Idx).LinkDesc;
		}
	}
	USequenceObject::UpdateObject();
}

/** When you create a SeqAct_Interp (aka Matinee action) automatically create an InterpData as well and connect it. */
void USeqAct_Interp::OnCreated()
{
	Super::OnCreated();

#if WITH_EDITORONLY_DATA
	// Declared in default properties
	check(VariableLinks.Num() >= 1);
	check(VariableLinks(0).ExpectedType == UInterpData::StaticClass());
	check(VariableLinks(0).LinkedVariables.Num() == 0);

	// Create new InterpData to go along with the new Matinee
	USequence* ParentSeq = Cast<USequence>(GetOuter());;
	if (!ParentSeq)
	{
		ParentSeq = ParentSequence;
	}
	UInterpData* NewData = ConstructObject<UInterpData>( UInterpData::StaticClass(), GetOuter(), NAME_None, RF_Transactional);

	check(ParentSeq);

	// Add to sequence (this sets the ParentSequence pointer)
	if( ParentSeq->AddSequenceObject(NewData) )
	{
		NewData->ObjPosX = ObjPosX;
		NewData->ObjPosY = ObjPosY + 200;
		NewData->OnCreated();
		NewData->Modify();

		// Add to links of Matinee action.
		VariableLinks(0).LinkedVariables.AddItem(NewData);
	}
#endif // WITH_EDITORONLY_DATA
}

static USequenceVariable* GetFirstVar( const FSeqVarLink& VarLink )
{
	for (INT idx = 0; idx < VarLink.LinkedVariables.Num(); idx++)
	{
		if(VarLink.LinkedVariables(idx) != NULL)
		{
			return VarLink.LinkedVariables(idx);
		}
	}

	return NULL;
}

/** Resolves Named and External variables for the matinee preview */
void USeqAct_Interp::GetNamedObjVars(TArray<UObject**>& OutObjects, const TCHAR* InDesc)
{
	USequenceVariable* Var = NULL;
	USequence* RootSeq = GetRootSequence();

	if (RootSeq != NULL)
	{
		// search for all variables of the expected type
		for (INT VarLinkIdx = 0; VarLinkIdx < VariableLinks.Num(); ++VarLinkIdx)
		{
			FSeqVarLink const& VarLink = VariableLinks(VarLinkIdx);

			// Check for a valid InDesc
			if (InDesc == NULL || *InDesc == 0 || VarLink.LinkDesc == InDesc)
			{
				TArray<USequenceVariable*> NamedVars;

				// Attempt to resolve named and external variables
				for (INT VarIdx = 0; VarIdx < VarLink.LinkedVariables.Num(); ++VarIdx)
				{
					if (VarLink.LinkedVariables(VarIdx) != NULL)
					{
						USeqVar_Named* NamedVar = Cast<USeqVar_Named>(VarLink.LinkedVariables(VarIdx));
						USeqVar_External* ExtVar = Cast<USeqVar_External>(VarLink.LinkedVariables(VarIdx));

						if (NamedVar)
						{
							RootSeq->FindNamedVariables(NamedVar->FindVarName, false, NamedVars);

							// Look up to the persistent level base sequence, since it will be the parent sequence when actually streamed in
							if (GWorld->PersistentLevel->GameSequences.Num() > 0 && RootSeq != GWorld->PersistentLevel->GameSequences(0))
							{
								GWorld->PersistentLevel->GameSequences(0)->FindNamedVariables(NamedVar->FindVarName, false, NamedVars);
							}
						}
						else if (ExtVar)
						{
							USequence* ParentSeq = Cast<USequence>(ExtVar->GetOuter());
							if (ParentSeq != NULL)
							{
								for (INT ExtVarIdx = 0; ExtVarIdx < ParentSeq->VariableLinks.Num(); ExtVarIdx++)
								{
									if (ParentSeq->VariableLinks(ExtVarIdx).LinkVar == ExtVar->GetFName())
									{
										USequenceVariable* FirstVariable = GetFirstVar( ParentSeq->VariableLinks( ExtVarIdx ) );
										if( FirstVariable != NULL )
										{
											NamedVars.AddUniqueItem( FirstVariable );
										}
									}
								}
							}
						}

					}
				}

				// Now get the object reference for each named / external variable we resolved
				for (INT NamedVarIdx = 0; NamedVarIdx < NamedVars.Num(); ++NamedVarIdx)
				{
					UObject** ObjectRef = NamedVars(NamedVarIdx)->GetObjectRef(0);
					if (ObjectRef != NULL)
					{
						OutObjects.AddItem(ObjectRef);
					}
				}
			}
		}
	}
}

/** 
 *	Return the InterpData currently connected to this Matinee action. Returns NULL if none connected. 
 *	Should never allow more than 1 InterpData connected.
 */
UInterpData* USeqAct_Interp::FindInterpDataFromVariable()
{
	USequence* RootSeq = GetRootSequence();
	// First variable connector should always be the InterpData.
	if (RootSeq != NULL &&
		VariableLinks.Num() > 0 &&
		VariableLinks(0).ExpectedType == UInterpData::StaticClass() &&
		VariableLinks(0).LinkedVariables.Num() > 0)
	{
		// We need to handle the case where the InterpData is connected via an External or Named variable.
		// Here we keep traversing these until we either find an InterpData, or fail (returning NULL).
		USequenceVariable* Var = VariableLinks(0).LinkedVariables(0);
		while(Var)
		{
			UInterpData* Data = Cast<UInterpData>(Var);
			if( Data )
			{
				return Data;
			}

			USeqVar_External* ExtVar = Cast<USeqVar_External>(Var);
			USeqVar_Named* NamedVar = Cast<USeqVar_Named>(Var);
			Var = NULL;

			if(ExtVar)
			{
				USequence* ParentSeq = Cast<USequence>(ExtVar->GetOuter());
				if (ParentSeq != NULL)
				{
					for (INT varIdx = 0; varIdx < ParentSeq->VariableLinks.Num(); varIdx++)
					{
						if (ParentSeq->VariableLinks(varIdx).LinkVar == ExtVar->GetFName())
						{
							Var = GetFirstVar( ParentSeq->VariableLinks(varIdx) );
							if( Var != NULL )
							{
								break;
							}
						}
					}
				}
			}
			else if(NamedVar)
			{
				TArray<USequenceVariable*> Vars;
				RootSeq->FindNamedVariables(NamedVar->FindVarName, false, Vars);

				if( Vars.Num() == 1 )
				{
					Data = Cast<UInterpData>( Vars(0) );
					if(Data)
					{
						return Data;
					}
				}
			}
		}
	}

	return NULL;
}

/** Synchronize the variable connectors with the currently attached InterpData. */
void USeqAct_Interp::UpdateConnectorsFromData()
{
	UInterpData* const Data = FindInterpDataFromVariable();

	USeqAct_Interp* const ClassDefaultObject = (USeqAct_Interp*) GetClass()->GetDefaultObject();
	INT const NumDefaultVariableLinks = ClassDefaultObject ? ClassDefaultObject->VariableLinks.Num() : 0;
	INT const NumDefaultOutputLinks = ClassDefaultObject ? ClassDefaultObject->OutputLinks.Num() : 0;

	if (Data)
	{
		// Remove any connectors for which there is no Group (or group is a Director Group or Folder). Note, we don't
		// check the MatineeData connector!
		for(INT i=VariableLinks.Num()-1; i>=NumDefaultVariableLinks; i--)
		{
			// ignore exposed variable links
			if (VariableLinks(i).PropertyName != NAME_None)
			{
				continue;
			}
			const TCHAR* LinkDescription = *VariableLinks(i).LinkDesc;
			const FName LinkGroupName( LinkDescription );
			const INT GroupIndex = Data->FindGroupByName( LinkGroupName );
			if( GroupIndex == INDEX_NONE || Data->InterpGroups(GroupIndex)->IsA(UInterpGroupDirector::StaticClass()) || Data->InterpGroups(GroupIndex)->bIsFolder )
			{
				VariableLinks.Remove(i);
			}
		}

		// Ensure there is a connector for each InterpGroup.
		for(INT i=0; i<Data->InterpGroups.Num(); i++)
		{
			// Ignore director groups and folders
			if( !Data->InterpGroups(i)->IsA(UInterpGroupDirector::StaticClass()) && !Data->InterpGroups(i)->bIsFolder )
			{
				FName GroupName = Data->InterpGroups(i)->GroupName;
				if(FindConnectorIndex( GroupName.ToString(), LOC_VARIABLE ) == INDEX_NONE)
				{
					FSeqVarLink NewLink;
					appMemset(&NewLink, 0, sizeof(FSeqVarLink));
					NewLink.MinVars = 0;
					NewLink.MaxVars = 255;
					NewLink.ExpectedType = USeqVar_Object::StaticClass();
					NewLink.LinkDesc = Data->InterpGroups(i)->GroupName.ToString();

					VariableLinks.AddItem(NewLink);
				}
			}
		}

		// Take care of output connectors in a similar way.
		TArray<FName> EventNames;
		Data->GetAllEventNames(EventNames);

		for(INT i=OutputLinks.Num()-1; i>=NumDefaultOutputLinks; i--)
		{		
			const TCHAR* LinkDescription = *OutputLinks(i).LinkDesc;
			const FName LinkName( LinkDescription );
			if( !EventNames.ContainsItem(LinkName) )
			{
				OutputLinks.Remove(i);
			}
		}

		for(INT i=0; i<EventNames.Num(); i++)
		{
			const FString EventString = EventNames(i).ToString();
			const INT OutputIndex = FindConnectorIndex( EventString, LOC_OUTPUT );
			if(OutputIndex == INDEX_NONE)
			{
				const INT NewOutIndex = OutputLinks.AddZeroed();

				FSeqOpOutputLink NewOut;
				appMemset( &NewOut, 0, sizeof(FSeqOpOutputLink) );
				NewOut.LinkDesc = EventString;

				OutputLinks(NewOutIndex) = NewOut;
			}
		}
	}
	else
	{
		// First N variable links are always there  - for MatineeData
		if(VariableLinks.Num() > NumDefaultVariableLinks)
		{
			VariableLinks.Remove(NumDefaultVariableLinks, VariableLinks.Num() - NumDefaultVariableLinks);
		}

		// First 2 output links are always there - for Completed and Reversed outputs
		if(OutputLinks.Num() > NumDefaultOutputLinks)
		{
			OutputLinks.Remove(NumDefaultOutputLinks, OutputLinks.Num() - NumDefaultOutputLinks);
		}
	}

#if WITH_EDITOR
	// Recal connector positions as connectors may have changed
	SetPendingConnectorRecalc();
#endif
}

void USeqAct_Interp::Activated()
{
	USequenceOp::Activated();
	if( bIsPlaying )
	{
		// Don't think we should ever get here...
		return;
	}

	// See if the 'Play' or 'Reverse' inputs have got an impulse. If so, start up action and get it ticking.
	// do not start up if we're client side only and on a dedicated server
	if ((!bClientSideOnly || GWorld->GetNetMode() != NM_DedicatedServer) && (InputLinks(0).bHasImpulse || InputLinks(1).bHasImpulse || InputLinks(4).bHasImpulse))
	{
		InitInterp();

		if( InputLinks(0).bHasImpulse )
		{
			Play();
		}
		else if( InputLinks(1).bHasImpulse )
		{
			Reverse();
		}
		else if (InputLinks(4).bHasImpulse)
		{
			ChangeDirection();
		}

		// For each Actor being interpolated- add us to its LatentActions list, and add it to our LatentActors list.
		// Similar to USequenceAction::Activated - but we don't call a handler function on the Object. Should we?
		TArray<UObject**> objectVars;
		GetObjectVars(objectVars);

		for(INT i=0; i<objectVars.Num(); i++)
		{
			if( objectVars( i ) != NULL )
			{
				AActor* Actor = Cast<AActor>( *(objectVars(i)) );
				if( Actor )
				{
					UInterpGroupInst * GrInst = FindGroupInst(Actor);
					// ignore Actor if we failed to initialize a group for it. This can happen if the tracks in the matinee data are changed, but the connections are not.
					// This most often seems to happen if a prefab changes but the map instance hasn't been updated.
					if (GrInst != NULL)
					{
						PreActorHandle(Actor);
						// if actor has already been ticked, reupdate physics with updated position from track.
						// Fixes Matinee viewing through a camera at old position for 1 frame.
						if ( Actor->Physics == PHYS_Interpolating )
						{
							Actor->performPhysics(1.f);
						}
						Actor->eventInterpolationStarted(this, GrInst);
					}
					else
					{
						debugf(NAME_Warning, TEXT("%s has no groups that reference connected Actor '%s'"), *GetName(), *Actor->GetName());
					}
				}
			}
		}
		// if we are a server, create/update matinee actor for replication to clients
		if (!bClientSideOnly && GWorld->GetNetMode() != NM_Client)
		{
			if ((ReplicatedActor == NULL || ReplicatedActor->bDeleteMe) && ReplicatedActorClass != NULL)
			{
				ReplicatedActor = (AMatineeActor*)GWorld->SpawnActor(ReplicatedActorClass);
				check(ReplicatedActor != NULL);
				ReplicatedActor->InterpAction = this;
			}
			if (ReplicatedActor != NULL)
			{
				ReplicatedActor->eventUpdate();
			}
		}
	}
}

void USeqAct_Interp::NotifyActorsOfChange()
{
	for (INT i = 0; i < LatentActors.Num(); i++)
	{
		AActor* Actor = LatentActors(i);
		if (Actor != NULL && !Actor->IsPendingKill())
		{
			Actor->eventInterpolationChanged(this);
		}
	}
	if (ReplicatedActor != NULL)
	{
		ReplicatedActor->eventUpdate();
	}
}

/** Copies the values from all VariableLinks to the member variable [of this sequence op] associated with that VariableLink */
void USeqAct_Interp::PublishLinkedVariableValues()
{
	// detect PlayRate changes so we can update replication
	//@FIXME: better would be to just generally detect when this function changes something, but no checks or notifications for that exist
	FLOAT OldPlayRate = PlayRate;

	Super::PublishLinkedVariableValues();

	if (PlayRate != OldPlayRate && ReplicatedActor != NULL)
	{
		ReplicatedActor->eventUpdate();
	}
}

// Returning true from here indicated we are done.
UBOOL USeqAct_Interp::UpdateOp(FLOAT deltaTime)
{
	// First check inputs, to see if there is an input that might change direction etc.

	// NOTE: We check for Pause events first, so that a playing sequence can be paused in the same
	//		 stack frame that it started playing in.
	if( bIsPlaying && InputLinks(3).bHasImpulse )
	{
		Pause();
		NotifyActorsOfChange();
	}
	else if( InputLinks(0).bHasImpulse )
	{
		Play();
		NotifyActorsOfChange();
	}
	else if( InputLinks(1).bHasImpulse )
	{
		Reverse();
		NotifyActorsOfChange();
	}
	else if( InputLinks(2).bHasImpulse )
	{
		Stop();
	}
	else if (InputLinks(4).bHasImpulse)
	{
		ChangeDirection();
		NotifyActorsOfChange();
	}
	else if( !bIsPlaying )
	{
		// if we started a movie capture for a matinee, finish the capture
		if (GEngine->bStartWithMatineeCapture && 
			GetName() == GEngine->MatineeCaptureName) 
		{
			FString FullPackageName = ParentSequence->GetOutermost()->GetName();
			// Check for Play on PC.  That prefix is a bit special as it's only 5 characters. (All others are 6)
			if( FullPackageName.StartsWith( FString( PLAYWORLD_CONSOLE_BASE_PACKAGE_PREFIX ) + TEXT( "PC") ) )
			{
				FullPackageName = FullPackageName.Right(FullPackageName.Len() - 5);
			}
			else if( FullPackageName.StartsWith( PLAYWORLD_CONSOLE_BASE_PACKAGE_PREFIX ) )
			{
				// This is a Play on Console map package prefix. (6 characters)
				FullPackageName = FullPackageName.Right(FullPackageName.Len() - 6);
			}
			if (GEngine->MatineePackageCaptureName == FullPackageName)
			{
				FAVIWriter* AVIWriter = FAVIWriter::GetInstance();
				if (AVIWriter)
				{
					AVIWriter->SetCapturedMatineeFinished();
				}
			}
		}
		
		// If we have stopped playing - return 'true' to indicate so.
		return true;
	}


	// Clear all the inputs now.
	InputLinks(0).bHasImpulse = 0;
	InputLinks(1).bHasImpulse = 0;
	InputLinks(2).bHasImpulse = 0;
	InputLinks(3).bHasImpulse = 0;
	InputLinks(4).bHasImpulse = 0;

	// Update the current position and do any interpolation work.
	StepInterp(deltaTime, false);

	// This is a latent function. To indicate we are still doing something - return false.
	return false;

}

void USeqAct_Interp::DeActivated()
{
	// Never fire any outputs if no Matinee Data attached.
	if(InterpData)
	{
		// If we are at the start, fire off the 'Reversed' output.
		if(Position < KINDA_SMALL_NUMBER)
		{
			if( !OutputLinks(1).bDisabled && !(OutputLinks(1).bDisabled && GIsEditor))
			{
				OutputLinks(1).bHasImpulse = true;
			}
		}
		// If we reached the end, fire off the 'Complete' output.
		else if(Position > (InterpData->InterpLength - KINDA_SMALL_NUMBER))
		{
			if( !OutputLinks(0).bDisabled && !(OutputLinks(0).bDisabled && GIsEditor))
			{
				OutputLinks(0).bHasImpulse = true;
			}
		}
		// If we are in the middle (ie. stopped because the 'Stop' input was hit). Don't fire any output.
	}

	// Remove this latent action from all actors it was working on, and empty our list of actors.
	for(INT i=0; i<LatentActors.Num(); i++)
	{
		AActor* Actor = LatentActors(i);
		if(Actor && !Actor->IsPendingKill())
		{
			Actor->LatentActions.RemoveItem(this);
			Actor->eventInterpolationFinished(this);
		}
	}
	if (ReplicatedActor != NULL)
	{
		ReplicatedActor->eventUpdate();
	}

	LatentActors.Empty();

	// Do any interpolation sequence cleanup-  destroy track/group instances etc.
	TermInterp();
}

/** Disables the radio filter effect if "Disable Radio Filter" is checked. */
void USeqAct_Interp::DisableRadioFilterIfNeeded()
{
	UAudioDevice* AudioDevice = GEngine->GetAudioDevice();
	if( AudioDevice )
	{
		AudioDevice->EnableRadioEffect( !bDisableRadioFilter );
	}
}

/** Enables the radio filter */
void USeqAct_Interp::EnableRadioFilter()
{
	UAudioDevice* AudioDevice = GEngine->GetAudioDevice();
	if( AudioDevice )
	{
		AudioDevice->EnableRadioEffect( TRUE );
	}
}

void USeqAct_Interp::Play(UBOOL OnlyAIGroup)
{
	if( !bIsPlaying || bPaused )
	{
		// Disable the radio filter if we are just beginning to play
		DisableRadioFilterIfNeeded();
	}

	// add constant camera anims for matinee
	if(!bIsPlaying && ConstantCameraAnim != 0)
	{
		for (AController* C = GWorld->GetWorldInfo()->ControllerList; C != NULL; C = C->NextController)
		{
			APlayerController* PC = Cast<APlayerController>(C);
			if( PC )
			{
				PC->eventSetMatineeConstantCameraAnim(TRUE, ConstantCameraAnim, ConstantCameraAnimRate);
			}
		}
	}

	// Jump to specific location if desired.
	if(bForceStartPos && !bIsPlaying)
	{
		UpdateInterp(ForceStartPosition, FALSE, TRUE, OnlyAIGroup);
	}
	// See if we should rewind to beginning...
	else if(bRewindOnPlay && (!bIsPlaying || bRewindIfAlreadyPlaying))
	{
		if(bNoResetOnRewind)
		{
			ResetMovementInitialTransforms();
		}

		// 'Jump' interpolation to the start (ie. will not fire events between current position and start).
		UpdateInterp(0.f, FALSE, TRUE, OnlyAIGroup);
	}

	bReversePlayback = false;
	bIsPlaying = true;
	bPaused = false;
}

void USeqAct_Interp::Stop()
{
	// Re-enable the radio filter
	EnableRadioFilter();

	// add constant camera anims for matinee
	if(bIsPlaying && ConstantCameraAnim != 0)
	{
		for (AController* C = GWorld->GetWorldInfo()->ControllerList; C != NULL; C = C->NextController)
		{
			APlayerController* PC = Cast<APlayerController>(C);
			if( PC )
			{
				PC->eventSetMatineeConstantCameraAnim(FALSE, ConstantCameraAnim, 1.0f);
			}
		}
	}

	bIsPlaying = false;
	bPaused = false;
}

void USeqAct_Interp::Reverse()
{
	bReversePlayback = true;
	bIsPlaying = true;
	bPaused = false;
}

void USeqAct_Interp::Pause()
{
	// If we are playing - toggle the paused state.
	if(bIsPlaying)
	{
		// During pause, enable the radio filter.
		EnableRadioFilter();
		bPaused = !bPaused;
	}
}

void USeqAct_Interp::ChangeDirection()
{
	bReversePlayback = !bReversePlayback;
	bIsPlaying = true;
	bPaused = false;
}

/**
 *	Advance current position in sequence and call UpdateInterp to update each group/track and the actor it is working on.
 *
 *	@param	DeltaSeconds	Amount to step sequence on by.
 *	@param	bPreview		If we are previewing sequence (ie. viewing in editor without gameplay running)
 */
void USeqAct_Interp::StepInterp(FLOAT DeltaSeconds, UBOOL bPreview)
{
	// Do nothing if not playing.
	if(!bIsPlaying || bPaused || !InterpData)
		return;

	// do nothing if client side only and no affected Actors are recently visible
	UBOOL bSkipUpdate = FALSE;
	if (bClientSideOnly && bSkipUpdateIfNotVisible)
	{
		bSkipUpdate = TRUE;
		TArray<UObject**> ObjectVars;
		GetObjectVars(ObjectVars);
		for (INT i = 0; i < ObjectVars.Num() && bSkipUpdate; i++)
		{
			if( ObjectVars( i ) != NULL )
			{
				AActor* Actor = Cast<AActor>(*(ObjectVars(i)));
				if (Actor != NULL && Actor->LastRenderTime > Actor->WorldInfo->TimeSeconds - 1.f)
				{
					bSkipUpdate = FALSE;
				}
			}
		}
	}

	if (!bSkipUpdate)
	{
		FLOAT NewPosition;
		UBOOL bLooped = 0;
		UBOOL bShouldStopPlaying = FALSE;

		// Playing forwards
		if(!bReversePlayback)
		{
			NewPosition = Position + (DeltaSeconds * PlayRate);

			if(NewPosition > InterpData->InterpLength)
			{
				// If looping, play to end, jump to start, and set target to somewhere near the beginning.
				if(bLooping)
				{
					UpdateInterp(InterpData->InterpLength, bPreview);

					if(bNoResetOnRewind)
					{
						ResetMovementInitialTransforms();
					}

					UpdateInterp(0.f, bPreview, true);

					while(NewPosition > InterpData->InterpLength)
					{
						NewPosition -= InterpData->InterpLength;
					}

					bLooped = true;
				}
				// If not looping, snap to end and stop playing.
				else
				{
					NewPosition = InterpData->InterpLength;
					bShouldStopPlaying = TRUE;
				}
			}
		}
		// Playing backwards.
		else
		{
			NewPosition = Position - (DeltaSeconds * PlayRate);

			if(NewPosition < 0.f)
			{
				// If looping, play to start, jump to end, and set target to somewhere near the end.
				if(bLooping)
				{
					UpdateInterp(0.f, bPreview);
					UpdateInterp(InterpData->InterpLength, bPreview, true);

					while(NewPosition < 0.f)
					{
						NewPosition += InterpData->InterpLength;
					}

					bLooped = true;
				}
				// If not looping, snap to start and stop playing.
				else
				{
					NewPosition = 0.f;
					bShouldStopPlaying = TRUE;
				}
			}
		}

		UpdateInterp(NewPosition, bPreview);

		// We reached the end of the sequence (or the beginning, if playing backwards), so stop playback
		// now.  Note that we do that *after* calling UpdateInterp so that tracks that test bIsPlaying
		// will complete the full sequence before we stop them.
		if( bShouldStopPlaying )
		{
			Stop();
		}

		UpdateStreamingForCameraCuts(NewPosition, bPreview);

		
		if (ReplicatedActor != NULL)
		{
			// if we looped back to the start, notify the replicated actor so it can refresh any clients
			if (bLooped)
			{
				ReplicatedActor->eventUpdate();
			}
			else
			{
				// otherwise, just update position without notifying it
				// so that clients that join the game during movement will get the correct updated position
				// but nothing will get replicated to other clients that should be simulating the movement
				ReplicatedActor->Position = NewPosition;
			}
		}
	}
}

/** Number of seconds to look ahead for camera cuts (for notifying the streaming system). */
FLOAT GCameraCutLookAhead = 10.0f;

/**
 *	Updates the streaming system with the camera locations for the upcoming camera cuts, so
 *	that it can start streaming in textures for those locations now.
 *
 *	@param	CurrentTime		Current time within the matinee, in seconds
 *	@param	bPreview		If we are previewing sequence (ie. viewing in editor without gameplay running)
 */
void USeqAct_Interp::UpdateStreamingForCameraCuts(FLOAT CurrentTime, UBOOL bPreview/*=FALSE*/)
{
	// Only supports forward-playing non-looping matinees.
	if ( GIsGame && bIsPlaying && !bReversePlayback && !bLooping )
	{
		for ( INT CameraCutIndex=0; CameraCutIndex < CameraCuts.Num(); CameraCutIndex++ )
		{
			FCameraCutInfo& CutInfo = CameraCuts(CameraCutIndex);
			FLOAT TimeDifference = CutInfo.TimeStamp - CurrentTime;
			if ( TimeDifference > 0.0f && TimeDifference < GCameraCutLookAhead )
			{
				GStreamingManager->AddViewSlaveLocation( CutInfo.Location );
			}
			else if ( TimeDifference >= GCameraCutLookAhead )
			{
				break;
			}
		}
	}
}

/**
 * Checks to see if this Matinee should be associated with the specified player.  This is a relatively
 * quick test to perform.
 *
 * @param InPC The player controller to check
 *
 * @return TRUE if this Matinee sequence is compatible with the specified player
 */
UBOOL USeqAct_Interp::IsMatineeCompatibleWithPlayer( APlayerController* InPC ) const
{
	UBOOL bBindPlayerToMatinee = TRUE;

	// If the 'preferred split screen' value is non-zero, we'll only bind this Matinee to
	// player controllers that are associated with the specified game player index
	if( PreferredSplitScreenNum != 0 )
	{
		bBindPlayerToMatinee = FALSE;
		ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>( InPC->Player );
		if( LocalPlayer != NULL )
		{
			const INT GamePlayerIndex = GEngine->GamePlayers.FindItemIndex( LocalPlayer );
			if( ( GamePlayerIndex + 1 ) == PreferredSplitScreenNum )
			{
				bBindPlayerToMatinee = TRUE;
			}
		}
	}

	return bBindPlayerToMatinee;
}


/** Finds and returns the Director group, or NULL if not found. */
UInterpGroupDirector* USeqAct_Interp::FindDirectorGroup()
{
	if(InterpData)
	{
		for(INT i=0; i<InterpData->InterpGroups.Num(); i++)
		{
			UInterpGroup* Group = InterpData->InterpGroups(i);
			UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(Group);
			if (DirGroup)
			{
				return DirGroup;
			}
		}
	}
	return NULL;
}

/** Retrieve group linked variable but this goes through all of the index, and find unused actor **/
AActor * USeqAct_Interp::FindUnusedGroupLinkedVariable(FName GroupName)
{
	TArray<UObject**> ObjectVars;
	GetObjectVars(ObjectVars, *GroupName.ToString());
	GetNamedObjVars(ObjectVars, *GroupName.ToString());

	// We create at least one instance for every group - even if no Actor is attached.
	// Allows for tracks that dont require an Actor (eg. Event tracks).
	AActor* Actor = NULL;

	for(INT j=0; j<ObjectVars.Num(); j++)
	{
		if( ObjectVars( j ) != NULL )
		{
			UObject *Obj = *(ObjectVars(j));
			Actor = Cast<AActor>(Obj);

			// See if there is already a group working on this actor.
			UInterpGroupInst* TestInst = FindGroupInst(Actor);
			if(TestInst)
			{
				//debugf( NAME_Warning, TEXT("Skipping instancing group - an Actor may only be connected to one Group! [%s]"), *Actor->GetName()  );
				//Actor = NULL;
				continue;
			}
			else if (Actor)
			{
				return Actor;
			}
		}
	}

	return Actor;
}
/** Retrieve group linked variable 
**/

AActor * USeqAct_Interp::FindGroupLinkedVariable(INT Index, const TArray<UObject**> &ObjectVars)
{
	// We create at least one instance for every group - even if no Actor is attached.
	// Allows for tracks that dont require an Actor (eg. Event tracks).

	AActor* Actor = NULL;
	if( Index < ObjectVars.Num() )
	{
		if( ObjectVars( Index ) != NULL )
		{
			UObject *Obj = *(ObjectVars(Index));
			Actor = Cast<AActor>(Obj);

			// See if there is already a group working on this actor.
			UInterpGroupInst* TestInst = FindGroupInst(Actor);
			if(TestInst)
			{
				debugf( NAME_Warning, TEXT("Skipping instancing group - an Actor may only be connected to one Group! [%s]"), *Actor->GetName()  );
			}
		}
	}

	return Actor;
}

// For each Actor connected to a particular group, create an instance.
void USeqAct_Interp::InitInterp()
{
	// if groupinst still exists, that means it hasn't been propertly terminated, so terminate it
	// this happens in client, when persistent leve hasn't been unloaded, but restarted by server
	// it's not terminated, but reinitialized
	if (GroupInst.Num() != 0)
	{
		// group did not terminate, and it's trying to re-init, so will terminate here
		//checkMsg (FALSE, TEXT("Interp hasn't been terminated yet."));
		TermInterp();
	}

	// Get the InterpData that this SeqAct_Interp is linked to.
	InterpData = FindInterpDataFromVariable();

	// Only continue if we successfully found some data.
	if(InterpData)
	{
		// Cache whether or not we want to enable extreme content within this sequence
		bShouldShowGore = TRUE;
		if( GWorld != NULL && GWorld->GetWorldInfo() != NULL )
		{
			AGameReplicationInfo* GRI = GWorld->GetWorldInfo()->GRI;
			if( GRI != NULL )
			{
				bShouldShowGore = GRI->eventShouldShowGore();
			}
		}

		for(INT i=0; i<InterpData->InterpGroups.Num(); i++)
		{
			UInterpGroup* Group = InterpData->InterpGroups(i);

			// If this is a DirectorGroup, we find a player controller and pass it in instead of looking to a variable.
			UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(Group);
			UInterpGroupAI* AIGroup = Cast<UInterpGroupAI>(Group);
			UInterpGroupCamera* CameraGroup = Cast<UInterpGroupCamera>(Group);
			if(DirGroup)
			{
				// Need to do a game specific check here because there are no player controllers in the editor and matinee expects a group instance to be initialized.
				if(GIsGame)
				{
					UBOOL bCreatedGroup = FALSE;

					// iterate through the controller list
					for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
					{
						APlayerController *PC = Cast<APlayerController>(Controller);
						
						// If it's a player and this sequence is compatible with the player...
						if (PC != NULL && IsMatineeCompatibleWithPlayer( PC ) )
						{
							// create a new instance with this player
							UInterpGroupInstDirector* NewGroupInstDir = ConstructObject<UInterpGroupInstDirector>(UInterpGroupInstDirector::StaticClass(), this, NAME_None, RF_Transactional);
							GroupInst.AddItem(NewGroupInstDir);

							// and initialize the instance
							NewGroupInstDir->InitGroupInst(DirGroup, PC);
							bCreatedGroup = TRUE;
						}
					}

					// Sanity check to make sure that there is an instance for all groups.
					if(bCreatedGroup == FALSE)
					{
						UInterpGroupInstDirector* NewGroupInstDir = ConstructObject<UInterpGroupInstDirector>(UInterpGroupInstDirector::StaticClass(), this, NAME_None, RF_Transactional);
						GroupInst.AddItem(NewGroupInstDir);
						// and initialize the instance
						NewGroupInstDir->InitGroupInst(DirGroup, NULL);
					}
				}
				else
				{
					// In the editor always create a director group instance with a NULL group actor since there are no player controllers.
					UInterpGroupInstDirector* NewGroupInstDir = ConstructObject<UInterpGroupInstDirector>(UInterpGroupInstDirector::StaticClass(), this, NAME_None, RF_Transactional);
					GroupInst.AddItem(NewGroupInstDir);

					// and initialize the instance
					NewGroupInstDir->InitGroupInst(DirGroup, NULL);
				}
			}
			else
			{
				// Folder groups don't get variables
				if( !Group->bIsFolder )
				{
					TArray<UObject**> ObjectVars;
					GetObjectVars(ObjectVars, *Group->GroupName.ToString());
					GetNamedObjVars(ObjectVars, *Group->GroupName.ToString());

					for(INT j=0; j<ObjectVars.Num() || j==0; j++)
					{
						// clean up
						AActor * Actor = FindGroupLinkedVariable(j, ObjectVars);
						if (AIGroup)
						{
							UInterpGroupInstAI * GroupInstAI = ConstructObject<UInterpGroupInstAI>( UInterpGroupInstAI::StaticClass(), this, NAME_None, RF_Transactional );
							GroupInstAI->InitGroupInst(Group, Actor);
							GroupInst.AddItem(GroupInstAI);
						}
						else if (CameraGroup)
						{
							// Right now camera group is only used for CameraAnimations, and they're not saved with it. 
							UInterpGroupInstCamera * GroupInstCamera = ConstructObject<UInterpGroupInstCamera>( UInterpGroupInstCamera::StaticClass(), this, NAME_None, RF_Transient );
							GroupInstCamera->InitGroupInst(Group, Actor);
							GroupInst.AddItem(GroupInstCamera);
						}
						else
						{
							UInterpGroupInst* NewGroupInst = ConstructObject<UInterpGroupInst>(UInterpGroupInst::StaticClass(), this, NAME_None, RF_Transactional);
							GroupInst.AddItem(NewGroupInst);

							NewGroupInst->InitGroupInst(Group, Actor);
						}
					}
				}
			}
		}
	}

	/// Scan the matinee for camera cuts and sets up the CameraCut array.
	SetupCameraCuts();
}

/** Scans the matinee for camera cuts and sets up the CameraCut array. */
void USeqAct_Interp::SetupCameraCuts()
{
	UInterpGroupDirector* DirGroup = FindDirectorGroup();
	UInterpTrackDirector* DirTrack = DirGroup ? DirGroup->GetDirectorTrack() : NULL;
	if ( InterpData && DirTrack && DirTrack->CutTrack.Num() > 0 )
	{
		CameraCuts.Reserve( DirTrack->CutTrack.Num() );

		// Find the starting camera location for each cut.
		for( INT KeyFrameIndex=0; KeyFrameIndex < DirTrack->CutTrack.Num(); KeyFrameIndex++)
		{
			const FDirectorTrackCut& Cut = DirTrack->CutTrack(KeyFrameIndex);
			INT GroupIndex = InterpData->FindGroupByName( Cut.TargetCamGroup );
			UInterpGroupInst* ViewGroupInst = (GroupIndex != INDEX_NONE) ? FindFirstGroupInstByName( Cut.TargetCamGroup ) : NULL;
			if ( GroupIndex != INDEX_NONE && ViewGroupInst )
			{
				// Find a valid move track for this cut.
				UInterpGroup* Group = InterpData->InterpGroups(GroupIndex);
				for(INT TrackIndex=0; TrackIndex < Group->InterpTracks.Num(); TrackIndex++)
				{
					UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(Group->InterpTracks(TrackIndex));
					if ( MoveTrack && !MoveTrack->IsDisabled() && TrackIndex < ViewGroupInst->TrackInst.Num() )
					{
						FCameraCutInfo CameraCut;
						FRotator CameraRotation;

						UInterpTrackInst* TrackInst = ViewGroupInst->TrackInst( TrackIndex );
						UBOOL bSucceeded = MoveTrack->GetLocationAtTime( TrackInst, Cut.Time, CameraCut.Location, CameraRotation );

						// The first keyframe could be (0,0,0), try again slightly past it in time.
						if ( !bSucceeded || CameraCut.Location.IsNearlyZero() == TRUE )
						{
							bSucceeded = MoveTrack->GetLocationAtTime( TrackInst, Cut.Time+0.01f, CameraCut.Location, CameraRotation );
						}

						// Only add locations that aren't (0,0,0)
						if ( bSucceeded && CameraCut.Location.IsNearlyZero() == FALSE )
						{
							CameraCut.TimeStamp = Cut.Time;
							CameraCuts.AddItem( CameraCut );
							break;
						}
					}
				}
			}
		}
	}
}

void USeqAct_Interp::TermInterp()
{
	// Destroy each group instance.
	for(INT i=0; i<GroupInst.Num(); i++)
	{
		GroupInst(i)->TermGroupInst(true);
	}
	GroupInst.Empty();

	// Drop reference to interpolation data.
	InterpData = NULL;

	// remember last time this happened in game
	if (GIsGame && GWorld != NULL)
	{
		TerminationTime = GWorld->GetWorldInfo()->TimeSeconds;
	}
}

/** adds the passed in PlayerController to all running Director tracks so that its camera is controlled
 * all PCs that are available at playback start time are hooked up automatically, but this needs to be called to hook up
 * any that are created during playback (player joining a network game during a cinematic, for example)
 * @param PC the PlayerController to add
 */
void USeqAct_Interp::AddPlayerToDirectorTracks(APlayerController* PC)
{
	// if we aren't initialized (i.e. not currently running) then do nothing
	if (PC != NULL && InterpData != NULL && GroupInst.Num() > 0 && GIsGame)
	{
		for (INT i = 0; i < InterpData->InterpGroups.Num(); i++)
		{
			UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(InterpData->InterpGroups(i));
			if (DirGroup != NULL)
			{
				UBOOL bAlreadyHasGroup = FALSE;
				for (INT j = 0; j < GroupInst.Num(); j++)
				{
					if (GroupInst(j)->Group == DirGroup && GroupInst(j)->GroupActor == PC)
					{
						bAlreadyHasGroup = TRUE;
						break;
					}
				}
				if (!bAlreadyHasGroup)
				{
					// Make sure this sequence is compatible with the player
					if( IsMatineeCompatibleWithPlayer( PC ) )
					{
						// create a new instance with this player
						UInterpGroupInstDirector* NewGroupInstDir = ConstructObject<UInterpGroupInstDirector>(UInterpGroupInstDirector::StaticClass(), this, NAME_None, RF_Transactional);
						GroupInst.AddItem(NewGroupInstDir);

						// and initialize the instance
						NewGroupInstDir->InitGroupInst(DirGroup, PC);
					}
				}
			}
		}
	}
}


/** Defines the maximum depth of a group actor in relation to its Base chain. */
static INT MaxDepthBuckets = 10;

void USeqAct_Interp::UpdateInterp(FLOAT NewPosition, UBOOL bPreview, UBOOL bJump, UBOOL OnlyAIGroup)
{
	if(!InterpData)
	{
		return;
	}

	NewPosition = Clamp(NewPosition, 0.f, InterpData->InterpLength);

	// Initialize the "buckets" to sort group insts by Base depth. 
	TArray< TArray<UInterpGroupInst*> > SortedGroupInsts;
	SortedGroupInsts.AddZeroed(MaxDepthBuckets);

	for(INT i=0; i<GroupInst.Num(); i++)
	{
		UInterpGroupInst* GrInst = GroupInst(i);

		check(GrInst->Group);
		// if we do only AIGroups, and if not AIGroups, skip
		if (OnlyAIGroup && !GrInst->IsA(UInterpGroupInstAI::StaticClass()))
		{
			// skip
			continue;
		}

		// Determine the depth of group inst by the 
		// number of parents in the Base chain.
		INT ActorParentCount = 0;

		// A group inst may not have actor. In 
		// that case, the depth will be zero. 
		if( GrInst->GetGroupActor() )
		{
			AActor* CurrentBase = GrInst->GetGroupActor()->Base;

			// To figure out the update order, just walk up the 
			// Base tree to calculate the depth of this group. 
			while( CurrentBase != NULL  )
			{
				ActorParentCount++;
				CurrentBase = CurrentBase->Base;
			}
		}

		if( !SortedGroupInsts.IsValidIndex(ActorParentCount) )
		{
			// Increase the maximum bucket size to prevent resizing on next update. 
			MaxDepthBuckets = ActorParentCount + 1;

			// Add enough buckets to make the actor's parent depth valid. 
			const INT BucketsToAdd = MaxDepthBuckets - SortedGroupInsts.Num();
			SortedGroupInsts.AddZeroed(BucketsToAdd);

			// Hopefully, somebody will notice this alert. If so, increase MaxDepthBuckets to the logged max. 
			GLog->Logf( TEXT("WARNING: Reached maximum group actor depth in USeqAct_Interp::UpdateInterp()! Increase max to %d."), MaxDepthBuckets );
		}

		// Add the group inst into the corresponding bucket for its depth level.
		SortedGroupInsts(ActorParentCount).AddItem(GrInst);
	}

	// Update each group in order by the group inst's Base depth. 
	for( INT BaseDepthIndex = 0; BaseDepthIndex < SortedGroupInsts.Num(); ++BaseDepthIndex )
	{
		TArray<UInterpGroupInst*>& Groups = SortedGroupInsts(BaseDepthIndex);

		for( INT GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex )
		{
			Groups(GroupIndex)->Group->UpdateGroup( NewPosition, Groups(GroupIndex), bPreview, bJump );
		}
	}

	// check for any attached cover links that should be updated
	if (bInterpForPathBuilding &&
		Position <= InterpData->PathBuildTime &&
		NewPosition > InterpData->PathBuildTime)
	{
		for (INT Idx = 0; Idx < LinkedCover.Num(); Idx++)
		{
			ACoverLink *Link = LinkedCover(Idx);
			if (Link->IsEnabled())
			{
				for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
				{
					if (Link->Slots(SlotIdx).bEnabled)
					{
						Link->AutoAdjustSlot(SlotIdx,TRUE);
					}
				}
			}
		}
	}

	// after updating groups, if in editor, and if it has to clear the editor dirty property variables, 
	// do it here
#if WITH_EDITOR
	if ( GIsEditor )
	{
		// this is only to update preview meshes/stage mark groups for AI group
		// since I need AIGroup and AIGroupInst to separate, I'll need to clear variabel after update
		for ( INT I=0; I<InterpData->InterpGroups.Num(); ++I )
		{
			UInterpGroupAI * AIGroup = Cast<UInterpGroupAI>(InterpData->InterpGroups(I));
			if ( AIGroup )
			{
				if (AIGroup->bRecreatePreviewPawn)
				{
					AIGroup->bRecreatePreviewPawn = FALSE;
				}
				if (AIGroup->bRefreshStageMarkGroup)
				{
					AIGroup->bRefreshStageMarkGroup = FALSE;
				}
			}
		}
	}
#endif
	Position = NewPosition;
}

/** Reset any movement tracks so their 'initial position' is the current position. */
void USeqAct_Interp::ResetMovementInitialTransforms()
{
	if(!InterpData)
	{
		return;
	}

	for(INT i=0; i<GroupInst.Num(); i++)
	{
		UInterpGroupInst* GrInst = GroupInst(i);

		check(GrInst->Group);
		check(GrInst->Group->InterpTracks.Num() == GrInst->TrackInst.Num());

		for(INT j=0; j<GrInst->TrackInst.Num(); j++)
		{
			UInterpTrackInst* TrInst = GrInst->TrackInst(j);
			UInterpTrackInstMove* MoveInst = Cast<UInterpTrackInstMove>(TrInst);
			if(MoveInst)
			{
				MoveInst->CalcInitialTransform( GrInst->Group->InterpTracks(j), true );
			}
		}
	}
}

UInterpGroupInst* USeqAct_Interp::FindGroupInst(AActor* Actor)
{
	if(!Actor || Actor->bDeleteMe)
	{
		return NULL;
	}

	for(INT i=0; i<GroupInst.Num(); i++)
	{
		if( GroupInst(i)->HasActor(Actor) )
		{
			return GroupInst(i);
		}
	}

	return NULL;
}

UInterpGroupInst* USeqAct_Interp::FindFirstGroupInst(UInterpGroup* InGroup)
{
	if(!InGroup)
	{
		return NULL;
	}

	for(INT i=0; i<GroupInst.Num(); i++)
	{
		if( GroupInst(i)->Group == InGroup )
		{
			return GroupInst(i);
		}
	}

	return NULL;
}

UInterpGroupInst* USeqAct_Interp::FindFirstGroupInstByName(FName InGroupName)
{
	if(InGroupName == NAME_None)
	{
		return NULL;
	}

	for(INT i=0; i<GroupInst.Num(); i++)
	{
		if( GroupInst(i)->Group->GroupName == InGroupName )
		{
			return GroupInst(i);
		}
	}

	return NULL;
}

/** Find the first group instance based on the InterpGroup with the given name. */
UInterpGroupInst* USeqAct_Interp::FindFirstGroupInstByName( const FString& InGroupName )
{
	for(INT i=0; i<GroupInst.Num(); i++)
	{
		if( GroupInst(i)->Group->GroupName.ToString() == InGroupName )
		{
			return GroupInst(i);
		}
	}

	return NULL;
}


/** If we have a DirectorGroup, use it to find the viewed group at the give time, then the first instance of that group, and the Actor it is bound to. */
AActor* USeqAct_Interp::FindViewedActor()
{
	UInterpGroupDirector* DirGroup = InterpData->FindDirectorGroup();
	if(DirGroup)
	{
		UInterpTrackDirector* DirTrack = DirGroup->GetDirectorTrack();
		if(DirTrack)
		{
			FLOAT CutTime, CutTransitionTime;
			FName ViewGroupName = DirTrack->GetViewedGroupName(Position, CutTime, CutTransitionTime);
			UInterpGroupInst* ViewGroupInst = FindFirstGroupInstByName(ViewGroupName);
			if(ViewGroupInst)
			{
				return ViewGroupInst->GetGroupActor();
			}
		}
	}

	return NULL;
}

/** 
 *	Utility for getting all Actors currently being worked on by this Matinee action. 
 *	If bMovementTrackOnly is set, Actors must have a Movement track in their group to be included in the results.
 */
void USeqAct_Interp::GetAffectedActors(TArray<AActor*>& OutActors, UBOOL bMovementTrackOnly)
{
	for(INT i=0; i<GroupInst.Num(); i++)
	{
		if(GroupInst(i)->GetGroupActor())
		{
			UInterpGroup* Group = GroupInst(i)->Group;
			TArray<UInterpTrack*> MovementTracks;
			Group->FindTracksByClass(UInterpTrackMove::StaticClass(), MovementTracks);

			// If we either dont just want movement tracks, or we do and we have a movement track, add to array.
			if(!bMovementTrackOnly || MovementTracks.Num() > 0)
			{	
				OutActors.AddUniqueItem( GroupInst(i)->GetGroupActor() );
			}
		}
	}
}

void USeqAct_Interp::SetPosition(FLOAT NewPosition, UBOOL bJump)
{
	// if we aren't currently active, temporarily activate to change the position
	UBOOL bTempActivate = !bActive;
	if (bTempActivate)
	{
		InitInterp();
	}

	UpdateInterp(NewPosition, FALSE, bJump);
	// update interpolating actors for the new position
	TArray<UObject**> ObjectVars;
	GetObjectVars(ObjectVars);
	for (INT i = 0; i < ObjectVars.Num(); i++)
	{
		if( ObjectVars( i ) != NULL )
		{
			AActor* Actor = Cast<AActor>(*(ObjectVars(i)));
			if (Actor != NULL && !Actor->bDeleteMe && Actor->Physics == PHYS_Interpolating)
			{
				// temporarily add ourselves to the Actor's LatentActions list so it can find us
				const INT Index = Actor->LatentActions.AddItem(this);
				Actor->physInterpolating(Actor->WorldInfo->DeltaSeconds);
				Actor->LatentActions.Remove(Index);
			}
		}
	}

	if (bTempActivate)
	{
		TermInterp();
	}

	if (ReplicatedActor != NULL)
	{
		ReplicatedActor->eventUpdate();
	}
}



/**
 * Conditionally saves state for the specified actor and its children
 */
void USeqAct_Interp::ConditionallySaveActorState( UInterpGroupInst* GroupInst, AActor* Actor )
{
#if WITH_EDITORONLY_DATA
	UBOOL bShouldCaptureTransforms = FALSE;
	UBOOL bShouldCaptureChildTransforms = FALSE;
	UBOOL bShouldCaptureVisibility = FALSE;

	// Iterate over all of this group's tracks
	for( INT TrackIdx = 0; TrackIdx < GroupInst->Group->InterpTracks.Num(); ++TrackIdx )
	{
		const UInterpTrack* CurTrack = GroupInst->Group->InterpTracks( TrackIdx );

		if ( CurTrack->IsDisabled() )
		{
			continue;
		}

		// Is this is a 'movement' track?  If so, then we'll consider it worthy of our test
		if( CurTrack->IsA( UInterpTrackMove::StaticClass() ) )
		{
			bShouldCaptureTransforms = TRUE;
			bShouldCaptureChildTransforms = TRUE;
		}

		// Is this an 'anim control' track?  If so, we'll need to capture the object's transforms along
		// with all of it's attached objects.  As the object animates, any attached objects will wander
		// so we'll need to make sure to use this to restore their transform later.
		if( CurTrack->IsA( UInterpTrackAnimControl::StaticClass() ) )
		{
			// @todo: Consider not capturing parent actor transforms for anim control tracks
			bShouldCaptureTransforms = TRUE;
			bShouldCaptureChildTransforms = TRUE;
		}

		// Is this a 'visibility' track?  If so, we'll save the actor's original bHidden state
		if( CurTrack->IsA( UInterpTrackVisibility::StaticClass() ) )
		{
			bShouldCaptureVisibility = TRUE;
		}
	}

	if( bShouldCaptureTransforms || bShouldCaptureChildTransforms )
	{
		// Save off the transforms of the actor and anything directly or indirectly attached to it.
		const UBOOL bOnlyCaptureChildren = !bShouldCaptureTransforms;
		SaveActorTransforms( Actor, bOnlyCaptureChildren );
	}

	if( bShouldCaptureVisibility )
	{
		// Save visibility state
		SaveActorVisibility( Actor );
	}
#endif // WITH_EDITORONLY_DATA
}



/**
 * Adds the specified actor and any actors attached to it to the list
 * of saved actor transforms.  Does nothing if an actor has already
 * been saved.
 */
void USeqAct_Interp::SaveActorTransforms( AActor* Actor, UBOOL bOnlyChildren )
{
	check( GIsEditor );
#if WITH_EDITORONLY_DATA
	if( Actor != NULL )
	{
		if( bOnlyChildren )
		{
			// Only save transforms for child actors
			for( INT AttachedActorIndex = 0 ; AttachedActorIndex < Actor->Attached.Num() ; ++AttachedActorIndex )
			{
				AActor* Other = Actor->Attached(AttachedActorIndex);
				SaveActorTransforms( Other, TRUE );
			}
		}
		else if ( !Actor->bDeleteMe )
		{
			// Save transforms for the parent actor and all of its children
			const FSavedTransform* SavedTransform = SavedActorTransforms.Find( Actor );
			if ( !SavedTransform )
			{
				FSavedTransform NewSavedTransform;
				NewSavedTransform.Location = Actor->Location;
				NewSavedTransform.Rotation = Actor->Rotation;

				SavedActorTransforms.Set( Actor, NewSavedTransform );

				for( INT AttachedActorIndex = 0 ; AttachedActorIndex < Actor->Attached.Num() ; ++AttachedActorIndex )
				{
					AActor* Other = Actor->Attached(AttachedActorIndex);
					SaveActorTransforms( Other, FALSE );
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/**
 * Applies the saved locations and rotations to all saved actors.
 */
void USeqAct_Interp::RestoreActorTransforms()
{
	check( GIsEditor );

#if WITH_EDITORONLY_DATA
	for( TMap<AActor*, FSavedTransform>::TIterator It(SavedActorTransforms) ; It ; ++It )
	{
		AActor* SavedActor = It.Key();
		FSavedTransform& SavedTransform = It.Value();

		// only update actor position/rotation if the track changed its position/rotation
		if( !SavedActor->Location.Equals(SavedTransform.Location) || 
			!(SavedActor->Rotation == SavedTransform.Rotation) )
		{
			SavedActor->Location = SavedTransform.Location;
			SavedActor->Rotation = SavedTransform.Rotation;
			SavedActor->ConditionalForceUpdateComponents();
			SavedActor->PostEditMove( TRUE );
		}
	}
	SavedActorTransforms.Empty();
#endif // WITH_EDITORONLY_DATA
}

/** Saves whether or not this actor is hidden so we can restore it later */
void USeqAct_Interp::SaveActorVisibility( AActor* Actor )
{
	check( GIsEditor );

#if WITH_EDITORONLY_DATA
	if( Actor != NULL )
	{
		if ( !Actor->bDeleteMe )
		{
			const BYTE* SavedVisibility = SavedActorVisibilities.Find( Actor );
			if ( !SavedVisibility )
			{
				SavedActorVisibilities.Set( Actor, ( BYTE )Actor->bHidden );
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}



/** Applies the saved visibility state for all saved actors */
void USeqAct_Interp::RestoreActorVisibilities()
{
	check( GIsEditor );

#if WITH_EDITORONLY_DATA
	for( TMap<AActor*, BYTE>::TIterator It(SavedActorVisibilities) ; It ; ++It )
	{
		AActor* SavedActor = It.Key();
		UBOOL bSavedHidden = ( UBOOL )It.Value();

		// only update actor if something has actually changed
		if( SavedActor->bHidden != bSavedHidden )
		{
			SavedActor->SetHidden( bSavedHidden );
			SavedActor->ConditionalForceUpdateComponents();
			SavedActor->PostEditMove( TRUE );
		}
	}
	SavedActorVisibilities.Empty();
#endif // WITH_EDITORONLY_DATA
}


/**
 * Stores the current scrub position, restores all saved actor transforms,
 * then saves off the transforms for actors referenced (directly or indirectly)
 * by group instances, and finally restores the scrub position.
 */
void USeqAct_Interp::RecaptureActorState()
{
	check( GIsEditor );

#if WITH_EDITORONLY_DATA
	// We now need to remove from the saved actor transformation state any actors
	// that belonged to the removed group instances, along with actors rooted to
	// those group actors.  However, another group could be affecting an actor which
	// is an ancestor of the removed actor(S).  So, we store the current scrub position,
	// restore all actor transforms (including the ones assigned to the groups that were
	// removed), then save off the transforms for actors referenced (directly or indirectly)
	// by group instances, then restore the scrub position.

	const FLOAT SavedScrubPosition = Position;

	RestoreActorVisibilities();
	RestoreActorTransforms();
	for(INT i=0; i<GroupInst.Num(); i++)
	{
		UInterpGroupInst* GrInst = GroupInst(i);
		AActor* GroupActor = GrInst->GetGroupActor();
		if( GroupActor )
		{
			ConditionallySaveActorState( GrInst, GroupActor );
		}
	}
	UpdateInterp( SavedScrubPosition, TRUE );
#endif // WITH_EDITORONLY_DATA
}

/**
 * Serialize function.
 *
 * @param	Ar		The archive to serialize with.
 */
void USeqAct_Interp::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// @fixme: Why are we serializing these our to disk?
	Ar << SavedActorTransforms;

	if( Ar.IsObjectReferenceCollector() )
	{
		Ar << SavedActorVisibilities;
	}
}

/**
 * Activates the output for the named event.
 */
void USeqAct_Interp::NotifyEventTriggered(class UInterpTrackEvent const* EventTrack, INT EventIdx)
{
	if ( EventTrack && (EventIdx >= 0) && (EventIdx < EventTrack->EventTrack.Num()) )
	{
		FName const EventName = EventTrack->EventTrack(EventIdx).EventName;

		// Find output with give name 
		INT OutputIndex = FindConnectorIndex( EventName.ToString(), LOC_OUTPUT );
		if (OutputIndex != INDEX_NONE)
		{
			if( !OutputLinks(OutputIndex).bDisabled &&
				!(OutputLinks(OutputIndex).bDisabledPIE && GIsEditor) )
			{
				ActivateOutputLink(OutputIndex);
			}
		}
	}
}

#if WITH_EDITOR
void USeqAct_Interp::OnVariableConnect(USequenceVariable *Var, INT LinkIdx)
{
	UInterpGroupInst * LocalGroupInst = Cast<UInterpGroupInst>(FindFirstGroupInstByName(VariableLinks(LinkIdx).LinkDesc));
	UInterpGroupInstAI * AIGroupInst = Cast<UInterpGroupInstAI>(FindFirstGroupInstByName(VariableLinks(LinkIdx).LinkDesc));

	if ( AIGroupInst )
	{
		AIGroupInst->UpdatePreviewPawnFromSeqVarCharacter(LocalGroupInst->Group, Cast<USeqVar_Character>(Var));
	}
	else if ( LocalGroupInst )
	{
		if ( Var->GetObjectRef(0) )
		{
			LocalGroupInst->GroupActor = Cast<AActor>(*Var->GetObjectRef(0));

			// if you connect to stage mark group, it should refresh/recreate pawn
			// to find this out, I need to iterate through all groups to find any aigroup 
			for (INT I=0; I<GroupInst.Num(); ++I)
			{
				if (GroupInst(I)->IsA(UInterpGroupInstAI::StaticClass()))
				{
					UInterpGroupInstAI * IterAIGroupInst = CastChecked<UInterpGroupInstAI>(GroupInst(I));
					if (IterAIGroupInst)
					{
						// if this ai group connecting to this group		
						if (ensure(IterAIGroupInst->AIGroup) && 
							IterAIGroupInst->AIGroup->StageMarkGroup == LocalGroupInst->Group->GroupName)
						{
							// if this is stage mark group, maybe I should refresh this
							IterAIGroupInst->UpdateStageMarkGroupActor(this);
							IterAIGroupInst->CreatePreviewPawn();
							// refresh property
							IterAIGroupInst->AIGroup->Modify(TRUE);
							break;
						}
					}
				}
			}
		}
	}

	Super::OnVariableConnect(Var, LinkIdx);
}
#endif

/*-----------------------------------------------------------------------------
 UInterpGroup
-----------------------------------------------------------------------------*/

void UInterpGroup::PostLoad()
{
	Super::PostLoad();

	// Remove any NULLs in the InterpTracks array.
	INT TrackIndex = 0;
	while(TrackIndex < InterpTracks.Num())
	{
		if(InterpTracks(TrackIndex))
		{
			TrackIndex++;
		}
		else
		{
			InterpTracks.Remove(TrackIndex);
		}
	}

	// Now we have moved the AnimSets array into InterpGroup, we need to fix up old content.
	for(INT i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(InterpTracks(i));
		if(AnimTrack)
		{
			// Copy contents from that AnimTrack into GroupAnimSets..
			for(INT j=0; j<AnimTrack->AnimSets.Num(); j++)
			{
				GroupAnimSets.AddUniqueItem( AnimTrack->AnimSets(j) );
			}

			// ..and empty it
			AnimTrack->AnimSets.Empty();
		}
	}
}

void UInterpGroup::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
#if WITH_EDITOR
	if (PropertyChangedEvent.Property->GetName() == TEXT("GroupAnimSets"))
	{
		// Update the interp data bake and prune list
		UInterpData* InterpData = Cast<UInterpData>(GetOuter());
		if (InterpData != NULL)
		{
			InterpData->UpdateBakeAndPruneStatus();
		}
	}
#endif
}

/** Iterate over all InterpTracks in this InterpGroup, doing any actions to bring the state to the specified time. */
void UInterpGroup::UpdateGroup(FLOAT NewPosition, UInterpGroupInst* GrInst, UBOOL bPreview, UBOOL bJump)
{
	check( InterpTracks.Num() == GrInst->TrackInst.Num() );

	// Update animation state of Actor.
#if WITH_EDITORONLY_DATA
	// if in editor and preview and anim control exists, let them update weight before update track
	if ( GIsEditor && bPreview && HasAnimControlTrack() )
	{
		UpdateAnimWeights(NewPosition, GrInst, bPreview, bJump);
	}
#endif
	// We'll make sure to always update FaceFX tracks last, if we have any, since those will modify the skeletal
	// pose of the actor which can easily be blown away by VectorProperty tracks (DrawScale3D, etc) which force
	// an update of the actor's entire component set.
	for( INT CurUpdatePass = 1; CurUpdatePass <= 2; ++CurUpdatePass )
	{
		UBOOL bAnyFaceFXTracks = FALSE;
		for(INT i=0; i<InterpTracks.Num(); i++)
		{
			UInterpTrack* Track = InterpTracks(i);
			UInterpTrackInst* TrInst = GrInst->TrackInst(i);

			//Tracks that are disabled or are presently recording should NOT be updated
			if ( Track->IsDisabled() || Track->bIsRecording)
			{
				continue;
			}

			// Check to see if this is a FaceFX track; we'll always process those later.
			UInterpTrackFaceFX* FaceFXTrack = Cast< UInterpTrackFaceFX >( Track );
			if( FaceFXTrack != NULL )
			{
				bAnyFaceFXTracks = TRUE;
			}

			// Process FaceFX tracks on the second pass, all other tracks on the first pass
			if( ( CurUpdatePass == 2 ) == ( FaceFXTrack != NULL ) )
			{
				if(bPreview)
				{
					Track->ConditionalPreviewUpdateTrack(NewPosition, TrInst);
				}
				else
				{
					Track->ConditionalUpdateTrack(NewPosition, TrInst, bJump);
				}
			}
		}

		if( !bAnyFaceFXTracks )
		{
			// No FaceFX tracks found during first pass, so no need for more iteration!
			break;
		}
	}

	// Update animation state of Actor.
	UpdateAnimWeights(NewPosition, GrInst, bPreview, bJump);

	// Update stuff attached to bones (if group has anim control).
	if( bPreview && HasAnimControlTrack() )
	{
		GrInst->UpdateAttachedActors();
	}
}

/** Utility function for adding a weight entry to a slot with the given name. Creates a new entry in the array if there is not already one present. */
static void AddSlotInfo(TArray<FAnimSlotInfo>& SlotInfos, FName SlotName, FLOAT InChannelWeight)
{
	// Look for an existing entry with this name.
	for(INT i=0; i<SlotInfos.Num(); i++)
	{
		// If we find one, add weight to array and we are done.
		if(SlotInfos(i).SlotName == SlotName)
		{
			SlotInfos(i).ChannelWeights.AddItem(InChannelWeight);
			return;
		}
	}

	// If we didn't find one, add a new entry to the array now.
	const INT NewIndex = SlotInfos.AddZeroed();
	SlotInfos(NewIndex).SlotName = SlotName;
	SlotInfos(NewIndex).ChannelWeights.AddItem(InChannelWeight);
}

/** Returns whether this Group contains at least one AnimControl track. */
UBOOL UInterpGroup::HasAnimControlTrack() const
{
	UBOOL bHasAnimTrack = FALSE;
	for(INT i=0; i<InterpTracks.Num(); i++)
	{
		if( InterpTracks(i)->bIsAnimControlTrack )
		{
			bHasAnimTrack = TRUE;
		}
	}

	return bHasAnimTrack;
}

/** Returns whether this Group contains a movement track. */
UBOOL UInterpGroup::HasMoveTrack() const
{
	UBOOL bHasMoveTrack = FALSE;
	for(INT i=0; i<InterpTracks.Num(); i++)
	{
		if( InterpTracks(i)->IsA(UInterpTrackMove::StaticClass()) )
		{
			bHasMoveTrack = TRUE;
			break;
		}
	}

	return bHasMoveTrack;
}

/**
 * We keep this around here as otherwise we are constantly allocating/deallocating the FAnimSlotInfos that Matinee is using
 * NOTE:  We probably need to clear this out every N calls
 **/
namespace
{
	TArray<FAnimSlotInfo> UpdateAnimWeightsSlotInfos;
}

/** Iterate over AnimControl tracks in this Group, build the anim blend info structures, and pass to the Actor via (Preview)SetAnimWeights. */
void UInterpGroup::UpdateAnimWeights(FLOAT NewPosition, class UInterpGroupInst* GrInst, UBOOL bPreview, UBOOL bJump)
{
	// Get the Actor this group is working on.
	AActor* Actor = GrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	FLOAT TotalSlotNodeAnimWeight = 0.f;
	UBOOL UsingSlotNode = FALSE;
	FName SlotNodeNameUsed;

	// Now iterate over tracks looking for AnimControl ones.
	for(INT i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrack* Track = InterpTracks(i);
		check(Track);

		UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Track);
		if( AnimTrack && !AnimTrack->IsDisabled() )
		{
			// Add entry for this track to the SlotInfos array.
			const FLOAT TrackWeight = AnimTrack->GetWeightForTime(NewPosition);
			// if it's using slot node, then add weight
			if ( AnimTrack->SlotName != NAME_None )
			{
				TotalSlotNodeAnimWeight += TrackWeight;
				UsingSlotNode = TRUE;
				SlotNodeNameUsed = AnimTrack->SlotName;
			}

			AddSlotInfo(UpdateAnimWeightsSlotInfos, AnimTrack->SlotName, TrackWeight);
		}
	}

#if !FINAL_RELEASE
	// no weight is set and using slot node
	// sometimes effect artist put slot node name for non animtree
	// so I need to filter that out
	if ( (Actor->GetAPawn() || Actor->IsA(ASkeletalMeshActorMAT::StaticClass())) && UsingSlotNode && TotalSlotNodeAnimWeight <= ZERO_ANIMWEIGHT_THRESH )
	{
		debugf(NAME_DevAnim, TEXT("SlotName (%s) is set, but no weight is applied. Please add a key to curve editor and set weight."), *SlotNodeNameUsed.GetNameString());
	}
#endif
	// Finally, pass the array to the Actor. Does different things depending on whether we are in Matinee or not.
	if(bPreview)
	{
		Actor->PreviewSetAnimWeights(UpdateAnimWeightsSlotInfos);
	}
	else
	{
		Actor->SetAnimWeights(UpdateAnimWeightsSlotInfos);
	}

	UpdateAnimWeightsSlotInfos.Reset();
}

/** Ensure this group name is unique within this InterpData (its Outer). */
void UInterpGroup::EnsureUniqueName()
{
	UInterpData* IData = CastChecked<UInterpData>( GetOuter() );

	FName NameBase = GroupName;
	INT Suffix = 0;

	// Test all other groups apart from this one to see if name is already in use
	UBOOL bNameInUse = false;
	for(INT i=0; i<IData->InterpGroups.Num(); i++)
	{
		if( (IData->InterpGroups(i) != this) && (IData->InterpGroups(i)->GroupName == GroupName) )
		{
			bNameInUse = true;
		}
	}

	// If so - keep appending numbers until we find a name that isn't!
	while( bNameInUse )
	{
		FString GroupNameString = FString::Printf(TEXT("%s%d"), *NameBase.ToString(), Suffix);
		GroupName = FName( *GroupNameString );
		Suffix++;

		bNameInUse = false;
		for(INT i=0; i<IData->InterpGroups.Num(); i++)
		{
			if( (IData->InterpGroups(i) != this) && (IData->InterpGroups(i)->GroupName == GroupName) )
			{
				bNameInUse = true;
			}
		}
	}
}

/** 
 *	Find all the tracks in this group of a specific class.
 *	Tracks are in the output array in the order they appear in the group.
 */
void UInterpGroup::FindTracksByClass(UClass* TrackClass, TArray<UInterpTrack*>& OutputTracks)
{
	for(INT i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrack* Track = InterpTracks(i);
		if( Track->IsA(TrackClass) )
		{
			OutputTracks.AddItem(Track);
		}
	}
}

/** Util for determining how many AnimControl tracks within this group are using the Slot with the supplied name. */
INT UInterpGroup::GetAnimTracksUsingSlot(FName InSlotName)
{
	INT NumTracks = 0;
	for(INT i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>( InterpTracks(i) );
		if(AnimTrack && AnimTrack->SlotName == InSlotName)
		{
			NumTracks++;
		}
	}
	return NumTracks;
}


/*-----------------------------------------------------------------------------
	UInterpFilter
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UInterpFilter);

/** 
 * Given a interpdata object, updates visibility of groups and tracks based on the filter settings
 *
 * @param InData			Data to filter.
 */
void UInterpFilter::FilterData(class USeqAct_Interp* InData)
{
	// Mark our custom filtered groups as visible
	for( INT GroupIdx = 0; GroupIdx < InData->InterpData->InterpGroups.Num(); GroupIdx++)
	{
		UInterpGroup* CurGroup = InData->InterpData->InterpGroups( GroupIdx );
		CurGroup->bVisible = TRUE;

		for( INT CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
		{
			UInterpTrack* CurTrack = CurGroup->InterpTracks( CurTrackIndex );
			CurTrack->bVisible = TRUE;
		}
	}
}

/*-----------------------------------------------------------------------------
	UInterpFilter_Classes
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UInterpFilter_Classes);

/** 
 * Given a interpdata object, updates visibility of groups and tracks based on the filter settings
 *
 * @param InData			Data to filter.
 */
void UInterpFilter_Classes::FilterData(USeqAct_Interp* InData)
{
#if WITH_EDITORONLY_DATA
	for(INT GroupIdx=0; GroupIdx<InData->InterpData->InterpGroups.Num(); GroupIdx++)
	{
		UInterpGroup* Group = InData->InterpData->InterpGroups(GroupIdx);
		UInterpGroupInst* GroupInst = InData->FindFirstGroupInst(Group);

		UBOOL bIncludeThisGroup = TRUE;

		// Folder groups may not have a group instance
		if( GroupInst != NULL )
		{
			// We avoid filtering out director groups (unless all of the group's tracks are filtered out below)
			if( !Group->IsA( UInterpGroupDirector::StaticClass() ) )
			{
				// If we were set to filter specific classes, then do that! (Otherwise, the group will always
				// be included)
				if( ClassToFilterBy != NULL )
				{
					AActor* Actor = GroupInst->GetGroupActor();
					if( Actor != NULL )
					{
						if( !Actor->IsA( ClassToFilterBy ) )
						{
							bIncludeThisGroup = FALSE;
						}
					}
					else
					{
						// No actor bound but we have an actor filter set, so don't include it
						bIncludeThisGroup = FALSE;
					}
				}
			}

			// If we were set to only include the group if it contains specific types of
			// tracks, then do that now
			if( TrackClasses.Num() > 0 )
			{
				UBOOL bHasAppropriateTrack = FALSE;

				for( INT CurTrackIndex = 0; CurTrackIndex < Group->InterpTracks.Num(); ++CurTrackIndex )
				{
					UInterpTrack* CurTrack = Group->InterpTracks( CurTrackIndex );
					if( CurTrack != NULL )
					{
						for( INT TrackClassIndex = 0; TrackClassIndex < TrackClasses.Num(); ++TrackClassIndex )
						{
							if( CurTrack->IsA( TrackClasses( TrackClassIndex ) ) )
							{
								// We found a track that matches the filter!
								bHasAppropriateTrack = TRUE;
								break;
							}
						}
					}
				}

				if( !bHasAppropriateTrack )
				{
					// Group doesn't contain any tracks that matches the desired filter
					bIncludeThisGroup = FALSE;
				}
			}
		}
		else
		{
			// No group inst (probably a folder), so don't include it
			bIncludeThisGroup = FALSE;
		}

		if( bIncludeThisGroup )
		{
			// Mark the group as visible!
			Group->bVisible = TRUE;

			for( INT CurTrackIndex = 0; CurTrackIndex < Group->InterpTracks.Num(); ++CurTrackIndex )
			{
				UInterpTrack* CurTrack = Group->InterpTracks( CurTrackIndex );
				if( CurTrack != NULL )
				{
					// If we need to, go through and constrain which track types are visible using our
					// list of filtered track classes
					if( TrackClasses.Num() > 0 )
					{
						for( INT TrackClassIndex = 0; TrackClassIndex < TrackClasses.Num(); ++TrackClassIndex )
						{
							if( CurTrack->IsA( TrackClasses( TrackClassIndex ) ) )
							{
								// We found a track that matches the filter!
								CurTrack->bVisible = TRUE;
							}
						}
					}
					else
					{
						// No track filter set, so make sure they're all visible
						CurTrack->bVisible = TRUE;
					}
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/*-----------------------------------------------------------------------------
	UInterpFilter_Custom
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UInterpFilter_Custom);

/** 
 * Given a interpdata object, updates visibility of groups and tracks based on the filter settings
 *
 * @param InData			Data to filter.
 */
void UInterpFilter_Custom::FilterData(USeqAct_Interp* InData)
{
#if WITH_EDITORONLY_DATA
	// Mark our custom filtered groups as visible
	for(INT GroupIdx=0; GroupIdx<GroupsToInclude.Num(); GroupIdx++)
	{
		UInterpGroup* CurGroup = GroupsToInclude( GroupIdx );
		CurGroup->bVisible = TRUE;

		for( INT CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
		{
			UInterpTrack* CurTrack = CurGroup->InterpTracks( CurTrackIndex );
			CurTrack->bVisible = TRUE;
		}
	}
#endif // WITH_EDITORONLY_DATA
}


/*-----------------------------------------------------------------------------
 UInterpGroupInst
-----------------------------------------------------------------------------*/

/** 
 *	Returns the Actor that this GroupInstance is working on. 
 *	Should use this instead of just referencing GroupActor, as it check bDeleteMe for you.
 */
AActor* UInterpGroupInst::GetGroupActor()
{
	if(!GroupActor || GroupActor->bDeleteMe)
	{
		return NULL;
	}
	else 
	{
		return GroupActor;
	}
}

/** Called before Interp editing to save original state of Actor. @see UInterpTrackInst::SaveActorState */
void UInterpGroupInst::SaveGroupActorState()
{
	check(Group);
	for(INT i=0; i<TrackInst.Num(); i++)
	{
		TrackInst(i)->SaveActorState( Group->InterpTracks(i) );
	}
}

/** Called after Interp editing to put object back to its original state. @see UInterpTrackInst::RestoreActorState */
void UInterpGroupInst::RestoreGroupActorState()
{
	check(Group);
	for(INT i=0; i<TrackInst.Num(); i++)
	{
		TrackInst(i)->RestoreActorState( Group->InterpTracks(i) );
	}
}

/** 
*	Initialize this Group instance. Called from USeqAct_Interp::InitInterp before doing any interpolation.
*	Save the Actor for the group and creates any needed InterpTrackInsts
*/
void UInterpGroupInst::InitGroupInst(UInterpGroup* InGroup, AActor* InGroupActor)
{
	check(InGroup);

	// If this group has already been initialized, terminate it before reinitializing it
	// This can happen in networked games with placed pawns referenced by an InterpGroupAI
	if( TrackInst.Num() )
	{
		TermGroupInst(TRUE);
	}

	Group = InGroup;
	GroupActor = InGroupActor;
	CachedCamOverridePostProcess = NULL;

	for(INT i=0; i<InGroup->InterpTracks.Num(); i++)
	{
		// Construct Track instance object
		UInterpTrack* Track = InGroup->InterpTracks(i);
		UInterpTrackInst* TrInst = ConstructObject<UInterpTrackInst>( Track->TrackInstClass, this, NAME_None, RF_Transactional );
		TrackInst.AddItem(TrInst);

		TrInst->InitTrackInst( Track );
	}

	// If we have an anim control track, do startup for that.
	UBOOL bHasAnimTrack = Group->HasAnimControlTrack();
	if (GroupActor != NULL && !GroupActor->IsPendingKill())
	{
		// If in the editor and we haven't started playing, this should be Matinee! Bit yuck...
		if(GIsEditor && !GWorld->HasBegunPlay())
		{
			// Then set the ones specified by this Group.
			GroupActor->PreviewBeginAnimControl( Group );
		}
		else if (bHasAnimTrack)
		{
			// If in game - call script function that notifies us to that.
			GroupActor->eventBeginAnimControl( Group );
		}
	}
}

/** 
 *	Called when done with interpolation sequence. Cleans up InterpTrackInsts etc. 
 *	Do not do anything further with the Interpolation after this.
 */
void UInterpGroupInst::TermGroupInst(UBOOL bDeleteTrackInst)
{
	// If we have an anim control track, do startup for that.
	UBOOL bHasAnimTrack = Group->HasAnimControlTrack();
	if (GroupActor != NULL && !GroupActor->IsPendingKill())
	{
		// If in the editor and we haven't started playing, this should be Matinee!
		// We always call PreviewFinishAnimControl, even if we don't have an AnimTrack now, because we may have done at some point during editing in Matinee.
		if(GIsEditor && !GWorld->HasBegunPlay())
		{
			// Restore the AnimSets that was set on this actor when we entered Matinee.
			GroupActor->PreviewFinishAnimControl(Group);

			// DB: no longer needed, as WxInterpEd stores exact actor transformations.
			// Update any attachments to skeletal actors, so they are in the right place after the skeletal mesh has been reset.
			//UpdateAttachedActors();			
		}
		// Only call eventFinishAnimControl in the game if we have an anim track.
		else if(bHasAnimTrack)
		{
			// If in game - call script function to say we've finish with the anim control.
			GroupActor->eventFinishAnimControl(Group);
		}
	}

	for(INT i=0; i<TrackInst.Num(); i++)
	{
		// Do any track cleanup
		UInterpTrack* Track = Group->InterpTracks(i);
		TrackInst(i)->TermTrackInst( Track );
	}
	TrackInst.Empty();

	// Revert the cache any of the TrackInst's may have allocated
	FreePPS();
}

/** Force any actors attached to this group's actor to update their position using their relative location/rotation. */
void UInterpGroupInst::UpdateAttachedActors()
{
	AActor * GrActor = GetGroupActor();
	if(!GrActor)
	{
		return;
	}

	// We don't want to update any Actors that this Matinee is acting on, in case we mess up where the sequence has put it.
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GetOuter() );

	// Get a list of all Actors that this SeqAct_Interp is working on.
	TArray<AActor*> AffectedActors;	
	Seq->GetAffectedActors(AffectedActors, true);

	// Update all actors attached to Actor, expect those we specify.
	GrActor->EditorUpdateAttachedActors(AffectedActors);
}

/**
 * This function checks to see if it has the PPS allocated
 */
UBOOL UInterpGroupInst::HasPPS( void )
{
	return CachedCamOverridePostProcess ? TRUE : FALSE;
}

/**
 * This function creates the cached copy
 */
void UInterpGroupInst::CreatePPS( void )
{
	if ( !HasPPS() )
	{
		CachedCamOverridePostProcess = static_cast<FPostProcessSettings*>(appMalloc(sizeof( FPostProcessSettings )));
		CachedCamOverridePostProcess->DisableAllOverrides();
	}
}

/**
 * This function caches off a copy of the parsed PPS
 *
 * @PPSettings - The settings we want to make a copy of
 */
void UInterpGroupInst::CachePPS( const FPostProcessSettings& PPSettings )
{
	CreatePPS();
	if ( HasPPS() )
	{
		appMemCopy( *CachedCamOverridePostProcess, PPSettings );
	}
}

/**
 * This function restores the cached copy to the parsed PPS
 *
 * @PPSettings - The settings we want to restore to their original state
 */
void UInterpGroupInst::RestorePPS( FPostProcessSettings& PPSettings )
{
	if ( HasPPS() )
	{
		appMemCopy( PPSettings, *CachedCamOverridePostProcess );
	}
	DestroyPPS();
}

/**
 * This function releases the cached copy
 */
void UInterpGroupInst::DestroyPPS( void )
{
	if ( HasPPS() )
	{
		appFree(CachedCamOverridePostProcess);
		CachedCamOverridePostProcess = NULL;
	}
}

/**
 *  Check to see if the post process settings were cached and if so release them
 */
void UInterpGroupInst::FreePPS( void )
{
	if ( HasPPS() )	// If we've overridden the settings
	{
		ACameraActor* CamActor = Cast<ACameraActor>(GetGroupActor());
		if(CamActor)
		{
			RestorePPS( CamActor->CamOverridePostProcess );	// Restore the camera's original settings (if cached)
		}
		else
		{
			DestroyPPS();	// Free the memory (if allocated, just incase)
		}
	}
}

/*-----------------------------------------------------------------------------
 UInterpGroupDirector
-----------------------------------------------------------------------------*/

/** Iterate over all InterpTracks in this InterpGroup, doing any actions to bring the state to the specified time. */
void UInterpGroupDirector::UpdateGroup(FLOAT NewPosition, UInterpGroupInst* GrInst, UBOOL bPreview, UBOOL bJump)
{
	Super::UpdateGroup(NewPosition, GrInst, bPreview, bJump);
}

/** Returns the director track inside this Director group - if present. */
UInterpTrackDirector* UInterpGroupDirector::GetDirectorTrack()
{
	for(INT i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrackDirector* DirTrack = Cast<UInterpTrackDirector>( InterpTracks(i) );
		if( DirTrack && !DirTrack->IsDisabled() )
		{
			return DirTrack;
		}
	}

	return NULL;
}

/** Returns the fade track inside this Director group - if present. */
UInterpTrackFade* UInterpGroupDirector::GetFadeTrack()
{
	for(INT i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrackFade* FadeTrack = Cast<UInterpTrackFade>( InterpTracks(i) );
		if( FadeTrack && !FadeTrack->IsDisabled() )
		{
			return FadeTrack;
		}
	}

	return NULL;
}

/** Returns the slomo track inside this Director group - if present. */
UInterpTrackSlomo* UInterpGroupDirector::GetSlomoTrack()
{
	for(INT i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrackSlomo* SlomoTrack = Cast<UInterpTrackSlomo>( InterpTracks(i) );
		if( SlomoTrack && !SlomoTrack->IsDisabled() )
		{
			return SlomoTrack;
		}
	}

	return NULL;
}

UInterpTrackColorScale* UInterpGroupDirector::GetColorScaleTrack()
{
	for(INT i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrackColorScale* ColorTrack = Cast<UInterpTrackColorScale>( InterpTracks(i) );
		if( ColorTrack && !ColorTrack->IsDisabled() )
		{
			return ColorTrack;
		}
	}

	return NULL;
}


/** Returns this director group's Audio Master track, if it has one */
UInterpTrackAudioMaster* UInterpGroupDirector::GetAudioMasterTrack()
{
	for(INT i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrackAudioMaster* AudioMasterTrack = Cast<UInterpTrackAudioMaster>( InterpTracks(i) );
		if( AudioMasterTrack && !AudioMasterTrack->IsDisabled() )
		{
			return AudioMasterTrack;
		}
	}

	return NULL;
}

/*-----------------------------------------------------------------------------
 FInterpEdSelKey
-----------------------------------------------------------------------------*/

/** 
 * Recursive function used by GetOwningTrack();  to search through all subtracks
 */
UInterpTrack* FInterpEdSelKey::GetOwningTrack( UInterpTrack* pTrack )
{
	if ( pTrack )
	{
		// Loop through all the sub tracks trying to find the one that owns us
		for( INT iSubTrack = 0; iSubTrack < pTrack->SubTracks.Num(); iSubTrack++ )
		{
			UInterpTrack* pSubTrack = pTrack->SubTracks( iSubTrack );
			if ( pSubTrack )
			{
				UInterpTrack* pOwner = GetOwningTrack( pSubTrack );
				if ( pOwner )
				{
					return pOwner;
				}
				else if ( Track == pSubTrack )
				{
					return pTrack;
				}
			}
		}
	}
	return NULL;
}

/** 
 * Returns the parent track of this key.  If this track isn't a subtrack, Track is returned (it owns itself)
 */
UInterpTrack* FInterpEdSelKey::GetOwningTrack()
{
	if ( Group )
	{
		// Loop through all the interp tracks trying to find the one that owns us
		for( INT iInterpTrack = 0; iInterpTrack < Group->InterpTracks.Num(); iInterpTrack++ )
		{	
			UInterpTrack* pOwner = GetOwningTrack( Group->InterpTracks( iInterpTrack ) );
			if ( pOwner )
			{
				return pOwner;
			}
		}
	}
	return Track;
}

/** 
 * Returns the sub group name of the parent track of this key. If this track isn't a subtrack, nothing is returned
 */
FString FInterpEdSelKey::GetOwningTrackSubGroupName( INT* piSubTrack )
{
#if WITH_EDITORONLY_DATA
	// Get the owning track
	const UInterpTrack* pOwningTrack = GetOwningTrack();
	if ( pOwningTrack )
	{
		// Loop through all the sub tracks trying to find our index
		for( INT iSubTrack = 0; iSubTrack < pOwningTrack->SubTracks.Num(); iSubTrack++ )
		{
			const UInterpTrack* pSubTrack = pOwningTrack->SubTracks( iSubTrack );
			if ( pSubTrack && pSubTrack == Track )
			{
				// Loop through all the sub track groups trying to find a reference to our index
				for( INT iSubTrackGroup = 0; iSubTrackGroup < pOwningTrack->SubTrackGroups.Num(); iSubTrackGroup++ )
				{
					const FSubTrackGroup& rSubTrackGroup = pOwningTrack->SubTrackGroups( iSubTrackGroup );

					// Loop through all the track indices trying to find a reference to our index
					for( INT iTrackIndex = 0; iTrackIndex < rSubTrackGroup.TrackIndices.Num(); iTrackIndex++ )
					{
						const INT& rTrackIndex = rSubTrackGroup.TrackIndices( iTrackIndex );
						if ( iSubTrack == rTrackIndex )
						{
							// Send this back if requested
							if ( piSubTrack )
							{
								*piSubTrack = iSubTrack;
							}
							return rSubTrackGroup.GroupName;
						}
					}
				}
				break;
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
	return FString();
}

/*-----------------------------------------------------------------------------
 UInterpTrack
-----------------------------------------------------------------------------*/


/** 
 *	Conditionally calls PreviewUpdateTrack depending on whether or not the track is enabled.
 */
void UInterpTrack::ConditionalPreviewUpdateTrack(FLOAT NewPosition, class UInterpTrackInst* TrInst)
{
	// Is the track enabled?
	UBOOL bIsTrackEnabled = !bDisableTrack;
	UInterpGroupInst* GrInst = Cast<UInterpGroupInst>( TrInst->GetOuter() );
	if( GrInst != NULL )
	{
		USeqAct_Interp* Seq = Cast<USeqAct_Interp>( GrInst->GetOuter() );
		if( Seq != NULL )
		{
			if( ActiveCondition == ETAC_GoreEnabled && !Seq->bShouldShowGore ||
				ActiveCondition == ETAC_GoreDisabled && Seq->bShouldShowGore )
			{
				bIsTrackEnabled = FALSE;
			}
		}
	}

	if(bIsTrackEnabled)
	{
		PreviewUpdateTrack( NewPosition, TrInst );
	}
	else
	{
		TrInst->RestoreActorState(this);
	}
}

/** 
 *	Conditionally calls UpdateTrack depending on whether or not the track is enabled.
 */
void UInterpTrack::ConditionalUpdateTrack(FLOAT NewPosition, class UInterpTrackInst* TrInst, UBOOL bJump)
{
	// Is the track enabled?
	UBOOL bIsTrackEnabled = !bDisableTrack;
	UInterpGroupInst* GrInst = Cast<UInterpGroupInst>( TrInst->GetOuter() );
	if( GrInst != NULL )
	{
		USeqAct_Interp* Seq = Cast<USeqAct_Interp>( GrInst->GetOuter() );
		if( Seq != NULL )
		{
			if( ActiveCondition == ETAC_GoreEnabled && !Seq->bShouldShowGore ||
				ActiveCondition == ETAC_GoreDisabled && Seq->bShouldShowGore )
			{
				bIsTrackEnabled = FALSE;
			}
		}
	}

	if( bIsTrackEnabled )
	{
		UpdateTrack(NewPosition, TrInst, bJump);
	}
	else
	{
		TrInst->RestoreActorState(this);
	}
}


/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
* @return	String name of the helper class.*/
const FString	UInterpTrack::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackHelper") );
}

/** 
 * Returns the outer group of this track.  If this track is a subtrack, the group of its parent track is returned
 */
UInterpGroup* UInterpTrack::GetOwningGroup()
{
	UObject* Outer = NULL;
	for( Outer = GetOuter(); Outer && !Outer->IsA( UInterpGroup::StaticClass() ); Outer = Outer->GetOuter() );
	return CastChecked<UInterpGroup>(Outer);
}

/** 
 * Enables this track and optionally, all subtracks.
 * 
 * @param bInEnable				True if the track should be enabled, false to disable
 * @param bPropagateToSubTracks	True to propagate the state to all subtracks
 */
void UInterpTrack::EnableTrack( UBOOL bInEnable, UBOOL bPropagateToSubTracks )
{
	bDisableTrack = !bInEnable;

	if( bPropagateToSubTracks )
	{
		for( INT SubTrackIndex = 0; SubTrackIndex < SubTracks.Num(); ++SubTrackIndex )
		{
			SubTracks( SubTrackIndex )->EnableTrack( bInEnable, bPropagateToSubTracks );
		}
	}
}

/**
 * Toggle the override flags for a camera actor's post process setting if 
 * the given track is a property track that references a post process 
 * setting and the group actor is a camera actor.
 *
 * @param	GroupActor	The group actor for the interp track.
 */
void UInterpTrack::DisableCameraPostProcessFlags( AActor* GroupActor )
{
	check( GroupActor );

	if( GroupActor->IsA(ACameraActor::StaticClass()) )
	{
		FName PropertyName;
		if ( GetPropertyName( PropertyName ) )
		{
			InterpTools::DisableCameraPostProcessFlag(GroupActor, PropertyName);
		}
	}
}

/*-----------------------------------------------------------------------------
 UInterpTrackInst
-----------------------------------------------------------------------------*/

/** 
 *	Return the Actor associated with this instance of a Group. 
 *	Note that all Groups have at least 1 instance, even if no Actor variable is attached, so this may return NULL. 
 */
AActor* UInterpTrackInst::GetGroupActor()
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
	return GrInst->GetGroupActor();
}

/** Initialise this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInst::InitTrackInst(UInterpTrack* Track)
{
	// Check to see if this track is overriding a post process settings, and if so, backup the group actors
	// so it doesn't modify it's defaults which may be used elsewhere
	// Note: Each inst needs to perform this check when it's created, but the group is responsible for it's destruction
	check( Track );
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
	if ( !GrInst->HasPPS() )		// If we haven't already overridden the settings
	{
		ACameraActor* CamActor = Cast<ACameraActor>(GetGroupActor());
		if(CamActor)
		{
			// Check to see if this track is overriding any of the post process settings
			FName PropertyName;
			if ( Track->GetPropertyName( PropertyName ) && CamActor->CamOverridePostProcess.IsOverrideSetting( InterpTools::PruneInterpPropertyName(PropertyName) ) )
			{
				GrInst->CachePPS( CamActor->CamOverridePostProcess );	// Cache the camera's original settings
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstProperty
-----------------------------------------------------------------------------*/

/** 
 *	This utility finds a property reference by name within an actor.
 *
 * @param InActor				Actor to search within
 * @param InPropName			Name of the property to search for
 * @param OutOuterObjectInst	Out variable: Outer object instance that holds the property.
 */
static UProperty* FindPropertyByName(AActor* InActor, FName InPropName, UObject*& OutOuterObjectInst)
{
	FString CompString, PropString;

	if(InPropName.ToString().Split(TEXT("."), &CompString, &PropString))
	{
		// STRUCT
		// First look for a struct with first part of name.
		UStructProperty* StructProp = FindField<UStructProperty>( InActor->GetClass(), *CompString );
		if(StructProp)
		{
			// Look 
			UProperty* Prop = FindField<UProperty>( StructProp->Struct, *PropString );
			OutOuterObjectInst = NULL;
			return Prop;
		}

		// COMPONENT
		// If no struct property with that name, search for a matching component using name->component mapping table.
		FName CompName(*CompString);
		FName PropName(*PropString);
		UObject* OutObject = NULL;

		TArray<UComponent*> Components;
		InActor->CollectComponents(Components,FALSE);

		for ( INT ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++ )
		{
			UComponent* Component = Components(ComponentIndex);
			if ( Component->GetInstanceMapName() == CompName )
			{
				OutObject = Component;
				break;
			}
		}

		// If we found a component - look for the named property within it.
		if(OutObject)
		{
			UProperty* Prop = FindField<UProperty>( OutObject->GetClass(), *PropName.ToString() );
			OutOuterObjectInst = OutObject;
			return Prop;
		}
		// No component with that name found - return NULL;

		return NULL;
	}
	// No dot in name - just look for property in this actor.
	else
	{
		UProperty* Prop = FindField<UProperty>( InActor->GetClass(), *InPropName.ToString() );
		if(!Prop)
		{
			UStructProperty* StructProp = FindField<UStructProperty>( InActor->GetClass(), *PropString );
			if(StructProp)
			{
				UProperty* ContainerProp = FindField<UProperty>( StructProp->Struct, TEXT("MatineeValue"));

				if(ContainerProp)
				{
					OutOuterObjectInst = InActor;
					return ContainerProp;
				}
			}
		}
		OutOuterObjectInst = InActor;
		return Prop;
	}
}

/**
 * Retrieves the update callback from the interp property's metadata and stores it.
 *
 * @param InActor			Actor we are operating on.
 * @param TrackProperty		Property we are interpolating.
 */
void UInterpTrackInstProperty::SetupPropertyUpdateCallback(AActor* InActor, const FName& TrackPropertyName)
{
	// Try to find a custom callback to use when updating the property.  This callback will be called instead of UpdateComponents.
	UObject* PropertyOuterObject = NULL;
	UProperty* InterpProperty = FindPropertyByName( InActor, TrackPropertyName, PropertyOuterObject );
	if(InterpProperty != NULL && PropertyOuterObject != NULL)
	{
		FString UpdateCallbackName = FString(TEXT("OnUpdateProperty")) + InterpProperty->GetName();
		PropertyUpdateCallback = PropertyOuterObject->FindFunction(*UpdateCallbackName);

		if(PropertyUpdateCallback!=NULL)
		{
			PropertyOuterObjectInst=PropertyOuterObject;
		}
	}
}

/** 
 * Tries to call the property update callback.
 *
 * @return TRUE if the callback existed and was called, FALSE otherwise.
 */
UBOOL UInterpTrackInstProperty::CallPropertyUpdateCallback()
{
	UBOOL bResult = FALSE;

	// if we have a UFunction and object instance resolved, then call the update callback on it.  Otherwise, return FALSE so
	// the interp track can handle the updating. We only do this in the game as it relies on calling script code.
	if(GIsGame && PropertyUpdateCallback != NULL && PropertyOuterObjectInst != NULL)
	{
		void *FuncParms = appAlloca(PropertyUpdateCallback->ParmsSize);
		appMemzero(FuncParms, PropertyUpdateCallback->ParmsSize);
		PropertyOuterObjectInst->ProcessEvent(PropertyUpdateCallback, FuncParms);

		bResult = TRUE;
	}

	return bResult;
}

/** Called when interpolation is done. Should not do anything else with this TrackInst after this. */
void UInterpTrackInstProperty::TermTrackInst(UInterpTrack* Track)
{
	// Clear references
	PropertyUpdateCallback=NULL;
	PropertyOuterObjectInst=NULL;

	Super::TermTrackInst( Track );
}

/*-----------------------------------------------------------------------------
 UInterpTrackMoveAxis
-----------------------------------------------------------------------------*/
INT UInterpTrackMoveAxis::GetKeyframeIndex( FLOAT KeyTime ) const
{
	INT RetIndex = INDEX_NONE;
	if( FloatTrack.Points.Num() > 0 )
	{
		FLOAT CurTime = FloatTrack.Points(0).InVal;
		// Loop through every keyframe until we find a keyframe with the passed in time.
		// Stop searching once all the keyframes left to search have larger times than the passed in time.
		for( INT KeyIndex = 0; KeyIndex < FloatTrack.Points.Num() && CurTime <= KeyTime; ++KeyIndex )
		{
			if( KeyTime == FloatTrack.Points(KeyIndex).InVal )
			{
				RetIndex = KeyIndex;
				break;
			}
			CurTime = FloatTrack.Points(KeyIndex).InVal;	
		}
	}

	return RetIndex;
}

INT UInterpTrackMoveAxis::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	// We must be outered to a move track
	UInterpTrackMove* TrackParent = CastChecked<UInterpTrackMove>( GetOuter() );

	// Let the parent add keyframes to us based on its settings.
	INT NewKeyIndex = TrackParent->AddChildKeyframe( this, Time, TrInst, InitInterpMode );

	return NewKeyIndex;
}

void UInterpTrackMoveAxis::UpdateKeyframe(INT KeyIndex, class UInterpTrackInst* TrInst)
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );
	UInterpTrackMove* TrackParent = CastChecked<UInterpTrackMove>( GetOuter() );

	// Let our parent decide how to update us
	TrackParent->UpdateChildKeyframe( this, KeyIndex, TrInst );
}

INT UInterpTrackMoveAxis::SetKeyframeTime( INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );
	if( KeyIndex < 0 || KeyIndex >= FloatTrack.Points.Num() )
		return KeyIndex;

	INT NewKeyIndex = KeyIndex;
	if(bUpdateOrder)
	{
		NewKeyIndex = FloatTrack.MovePoint(KeyIndex, NewKeyTime);
		INT NewLookupKeyIndex = LookupTrack.MovePoint(KeyIndex, NewKeyTime);
		check( NewKeyIndex == NewLookupKeyIndex );
	}
	else
	{
		FloatTrack.Points(KeyIndex).InVal = NewKeyTime;
		LookupTrack.Points(KeyIndex).Time = NewKeyTime;
	}

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

void UInterpTrackMoveAxis::RemoveKeyframe( INT KeyIndex )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );
	Super::RemoveKeyframe( KeyIndex );
	LookupTrack.Points.Remove( KeyIndex );
}

INT UInterpTrackMoveAxis::DuplicateKeyframe( INT KeyIndex, FLOAT NewKeyTime )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );

	INT NewIndex = Super::DuplicateKeyframe( KeyIndex, NewKeyTime );
	FName& OldName = LookupTrack.Points(KeyIndex).GroupName;
	INT NewLookupKeyIndex = LookupTrack.AddPoint( NewKeyTime, OldName );
	
	check( NewIndex == NewLookupKeyIndex );

	return NewIndex;
}

FName UInterpTrackMoveAxis::GetLookupKeyGroupName( INT KeyIndex )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );
	check( KeyIndex < LookupTrack.Points.Num() );

	return LookupTrack.Points(KeyIndex).GroupName;
}

void UInterpTrackMoveAxis::SetLookupKeyGroupName( INT KeyIndex, const FName& NewGroupName )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );
	check( KeyIndex < LookupTrack.Points.Num() );

	LookupTrack.Points(KeyIndex).GroupName = NewGroupName;
}

void UInterpTrackMoveAxis::ClearLookupKeyGroupName( INT KeyIndex )
{
	SetLookupKeyGroupName( KeyIndex, NAME_None );
}

void UInterpTrackMoveAxis::GetKeyframeValue( UInterpTrackInst* TrInst, INT KeyIndex, FLOAT& OutTime, FLOAT &OutValue, FLOAT* OutArriveTangent, FLOAT* OutLeaveTangent )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );

	UBOOL bUseTrackKeyframe = TRUE;

	// If there is a valid group name in the lookup track at thsi index, use the lookup track to get transform information
	const FName& GroupName = LookupTrack.Points(KeyIndex).GroupName;

	if( GroupName != NAME_None && TrInst )
	{
		// Lookup position from the lookup track.
		AActor* Actor = TrInst->GetGroupActor();
		UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
		USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
		UInterpGroupInst* LookupGroupInst = Seq->FindFirstGroupInstByName(GroupName);

		if(Actor && LookupGroupInst && LookupGroupInst->GetGroupActor())
		{
			AActor* LookupActor = LookupGroupInst->GetGroupActor();

			// Slight hack here so that if we are trying to look at a Player variable, it looks at their Pawn.
			APlayerController* PC = Cast<APlayerController>(LookupActor);
			if(PC && PC->Pawn)
			{
				LookupActor = PC->Pawn;
			}

			// Find position
			if( MoveAxis == AXIS_TranslationX || MoveAxis == AXIS_TranslationY || MoveAxis == AXIS_TranslationZ )
			{
				OutValue = LookupActor->Location[MoveAxis];
			}
			else
			{
				OutValue = LookupActor->Rotation.Euler()[MoveAxis - 3];
			}

			OutTime = LookupTrack.Points( KeyIndex ).Time;
			// Find arrive and leave tangents.
			if(OutLeaveTangent != NULL || OutArriveTangent != NULL)
			{
				if(KeyIndex==0 || KeyIndex==(LookupTrack.Points.Num()-1))	// if we are an endpoint, set tangents to 0.
				{
					if(OutArriveTangent!=NULL)
					{
						appMemset( OutArriveTangent, 0, sizeof(FLOAT) );
					}

					if(OutLeaveTangent != NULL)
					{
						appMemset( OutLeaveTangent, 0, sizeof(FLOAT) );
					}
				}
				else
				{
					FLOAT PrevPos, NextPos;
					FLOAT PrevTime, NextTime;
					FLOAT AutoTangent;

					// Get previous and next positions for the tangents.
					GetKeyframeValue(TrInst, KeyIndex-1, PrevTime, PrevPos, NULL, NULL);
					GetKeyframeValue(TrInst, KeyIndex+1, NextTime, NextPos, NULL, NULL);

					if( FloatTrack.InterpMethod == IMT_UseFixedTangentEvalAndNewAutoTangents )
					{
						// @todo: Should this setting be exposed in some way to the Lookup track?
						const UBOOL bWantClamping = FALSE;

						ComputeCurveTangent(
							PrevTime, PrevPos,
							OutTime, OutValue,
							NextTime, NextPos,
							CurveTension,				// Tension
							bWantClamping,
							AutoTangent );					// Out
					}
					else
					{
						LegacyAutoCalcTangent( PrevPos, OutValue, NextPos, CurveTension, AutoTangent );
					}

					if(OutArriveTangent!=NULL)
					{
						*OutArriveTangent = AutoTangent;
					}

					if(OutLeaveTangent != NULL)
					{
						*OutLeaveTangent = AutoTangent;
					}
				}
			}

			bUseTrackKeyframe = FALSE;
		}
	}

	if( bUseTrackKeyframe )
	{
		OutTime = FloatTrack.Points(KeyIndex).InVal;
		OutValue = FloatTrack.Points(KeyIndex).OutVal;

		if(OutArriveTangent != NULL)
		{
			*OutArriveTangent = FloatTrack.Points(KeyIndex).ArriveTangent;
		}

		if(OutLeaveTangent != NULL)
		{
			*OutLeaveTangent = FloatTrack.Points(KeyIndex).LeaveTangent;
		}
	}
}

FLOAT UInterpTrackMoveAxis::EvalValueAtTime( UInterpTrackInst* TrInst, FLOAT Time )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );

	const INT NumPoints = FloatTrack.Points.Num();
	FLOAT KeyTime; // unused
	FLOAT OutValue;
	if( NumPoints == 0  )
	{
		return 0.0f;
	}
	else if( NumPoints < 2 || ( Time <= FloatTrack.Points(0).InVal ) )
	{
		GetKeyframeValue( TrInst, 0, KeyTime, OutValue, NULL, NULL );
		return OutValue;
	}
	else if( Time >= FloatTrack.Points(NumPoints-1).InVal )
	{
		GetKeyframeValue( TrInst, NumPoints - 1, KeyTime, OutValue, NULL, NULL );
		return OutValue;
	}
	else
	{
		for( INT i=1; i<NumPoints; i++ )
		{	
			if( Time < FloatTrack.Points(i).InVal )
			{
				const FLOAT Diff = FloatTrack.Points(i).InVal - FloatTrack.Points(i-1).InVal;

				if( Diff > 0.f && FloatTrack.Points(i-1).InterpMode != CIM_Constant )
				{
					const FLOAT Alpha = (Time - FloatTrack.Points(i-1).InVal) / Diff;

					if( FloatTrack.Points(i-1).InterpMode == CIM_Linear )	// Linear interpolation
					{
						FLOAT PrevPos, CurrentPos;
						GetKeyframeValue( TrInst, i-1, KeyTime, PrevPos, NULL, NULL);
						GetKeyframeValue( TrInst, i, KeyTime, CurrentPos, NULL, NULL);

						OutValue =  Lerp( PrevPos, CurrentPos, Alpha );
						return OutValue;
					}
					else	//Cubic Interpolation
					{
						// Get keyframe positions and tangents.
						FLOAT CurrentPos, CurrentArriveTangent, PrevPos, PrevLeaveTangent;
						GetKeyframeValue( TrInst, i-1, KeyTime, PrevPos, NULL, &PrevLeaveTangent);
						GetKeyframeValue( TrInst, i, KeyTime, CurrentPos, &CurrentArriveTangent, NULL);

						if(FloatTrack.InterpMethod == IMT_UseBrokenTangentEval)
						{
							OutValue = CubicInterp( PrevPos, PrevLeaveTangent, CurrentPos, CurrentArriveTangent, Alpha );
							return OutValue;
						}
						else
						{
							OutValue = CubicInterp( PrevPos, PrevLeaveTangent * Diff, CurrentPos, CurrentArriveTangent * Diff, Alpha );
							return OutValue;
						}
					}
				}
				else	// Constant Interpolation
				{
					GetKeyframeValue( TrInst, i-1, KeyTime, OutValue, NULL, NULL);
					return OutValue;
				}
			}
		}
	}

	// Shouldnt really reach here
	GetKeyframeValue( TrInst, NumPoints-1, KeyTime, OutValue, NULL, NULL);
	return OutValue;
}

/** 
 * Reduce Keys within Tolerance
 *
 * @param bIntervalStart	start of the key to reduce
 * @param bIntervalEnd		end of the key to reduce
 * @param Tolerance			tolerance
 */
void UInterpTrackMoveAxis::ReduceKeys( FLOAT IntervalStart, FLOAT IntervalEnd, FLOAT Tolerance )
{
	FInterpCurve<MatineeKeyReduction::SFLOAT>& OldCurve = (FInterpCurve<MatineeKeyReduction::SFLOAT>&) FloatTrack;

	// Create all the control points. They are six-dimensional, since
	// the Euler rotation key and the position key times must match.
	MatineeKeyReduction::MCurve<MatineeKeyReduction::SFLOAT, 1> Curve;
	Curve.RelativeTolerance = Tolerance / 100.0f;
	Curve.IntervalStart = IntervalStart - 0.0005f;  // 0.5ms pad to allow for floating-point precision.
	Curve.IntervalEnd = IntervalEnd + 0.0005f;  // 0.5ms pad to allow for floating-point precision.

	Curve.CreateControlPoints(OldCurve, 0);
	if (Curve.HasControlPoints())
	{
		Curve.FillControlPoints(OldCurve, 1, 0);

		// Reduce the curve.
		Curve.Reduce();

		// Copy the reduced keys over to the new curve.
		Curve.CopyCurvePoints(OldCurve.Points, 1, 0);
	}

	// Refer the look-up track to nothing.
	LookupTrack.Points.Empty();
	FName DefaultName(NAME_None);
	UINT PointCount = FloatTrack.Points.Num();
	for (UINT Index = 0; Index < PointCount; ++Index)
	{
		LookupTrack.AddPoint(FloatTrack.Points(Index).InVal, DefaultName );
	}
}
/*----------------------------------------------------------------------------
 UInterpTrackMove
-----------------------------------------------------------------------------*/

/** Set this track to sensible default values. Called when track is first created. */
void UInterpTrackMove::SetTrackToSensibleDefault()
{
	// Use 'relative to initial' as the default.
	MoveFrame = IMF_RelativeToInitial;
}


/** Total number of keyframes in this track. */
INT UInterpTrackMove::GetNumKeyframes() const
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	return PosTrack.Points.Num();
}

/** Get first and last time of keyframes in this track. */
void UInterpTrackMove::GetTimeRange(FLOAT& StartTime, FLOAT& EndTime) const
{
	// If there are no subtracks, this is an unsplit movemnt track.  Get timerange information directly from this track.
	if( SubTracks.Num() == 0 )
	{
		check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );

		if(PosTrack.Points.Num() == 0)
		{
			StartTime = 0.f;
			EndTime = 0.f;
		}
		else
		{
			// PosTrack and EulerTrack should have the same number of keys at the same times.
			check( (PosTrack.Points(0).InVal - EulerTrack.Points(0).InVal) < KINDA_SMALL_NUMBER );
			check( (PosTrack.Points(PosTrack.Points.Num()-1).InVal - EulerTrack.Points(EulerTrack.Points.Num()-1).InVal) < KINDA_SMALL_NUMBER );

			StartTime = PosTrack.Points(0).InVal;
			EndTime = PosTrack.Points( PosTrack.Points.Num()-1 ).InVal;
		}
	}
	else
	{
		// There are subtracks in this track. Find the min and max time by looking at all our subtracks.
		FLOAT SubStartTime = 0.0f, SubEndTime = 0.0f;
		SubTracks( 0 )->GetTimeRange( StartTime, EndTime );
		for( INT SubTrackIndex = 1; SubTrackIndex < SubTracks.Num(); ++SubTrackIndex )
		{
			SubTracks(SubTrackIndex)->GetTimeRange( SubStartTime, SubEndTime );
			StartTime = Min( SubStartTime, StartTime );
			EndTime = Max( SubEndTime, EndTime );
		}
	}
}

/**
 * @return	The ending time of the track. 
 */
FLOAT UInterpTrackMove::GetTrackEndTime() const
{
	FLOAT EndTime = 0.0f;

	if( PosTrack.Points.Num() )
	{
		EndTime = PosTrack.Points(PosTrack.Points.Num() - 1).InVal;
	}

	return EndTime;
}

/** Get the time of the keyframe with the given index. */
FLOAT UInterpTrackMove::GetKeyframeTime(INT KeyIndex) const
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	if( KeyIndex < 0 || KeyIndex >= PosTrack.Points.Num() )
		return 0.f;

	check( (PosTrack.Points(KeyIndex).InVal - EulerTrack.Points(KeyIndex).InVal) < KINDA_SMALL_NUMBER );

	return PosTrack.Points(KeyIndex).InVal;
}

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackMove::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	// If there are no subtracks, this track is not split, add a keyframe directly to this track.
	if( SubTracks.Num() == 0 )
	{
		check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );

		AActor* Actor = TrInst->GetGroupActor();
		if(!Actor)
		{
			return INDEX_NONE;
		}

		INT NewKeyIndex = PosTrack.AddPoint( Time, FVector(0,0,0) );
		PosTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

		INT NewRotKeyIndex = EulerTrack.AddPoint( Time, FVector(0.f) );
		EulerTrack.Points(NewRotKeyIndex).InterpMode = InitInterpMode;

		FName DefaultName(NAME_None);
		INT NewLookupKeyIndex = LookupTrack.AddPoint(Time, DefaultName);

		check((NewKeyIndex == NewRotKeyIndex) && (NewKeyIndex == NewLookupKeyIndex));

		// First key of a 'relative to initial' track must always be zero (unless we are using the raw actor location initially)
		if(MoveFrame == IMF_World || NewKeyIndex != 0 || bUseRawActorTMforRelativeToInitial)
		{
			UpdateKeyframe(NewKeyIndex, TrInst);
		}

		PosTrack.AutoSetTangents(LinCurveTension);
		EulerTrack.AutoSetTangents(AngCurveTension);

		return NewKeyIndex;
	}
	else
	{
		// This track has subtracks, add keyframe to each child.  
		AActor* Actor = TrInst->GetGroupActor();
		INT NewKeyIndex = INDEX_NONE;
		if( Actor )
		{
			for( INT SubTrackIndex = 0; SubTrackIndex < SubTracks.Num(); ++SubTrackIndex )
			{
				INT ReturnIndex = AddChildKeyframe( SubTracks(SubTrackIndex), Time, TrInst, InitInterpMode );
				check( ReturnIndex != INDEX_NONE );

				// Since each child track may add a keyframe at a different index, compute the min index where a keyframe was added.  
				// If a keyframe was added at index 0, we need  to update our initial transform.  The calling function checks for that.
				if( NewKeyIndex > ReturnIndex || NewKeyIndex == INDEX_NONE )
				{
					NewKeyIndex = ReturnIndex;
				}
			}
		}

		return NewKeyIndex;
	}
}

FVector WindNumToEuler(const FVector& WindNum)
{
	FVector OutEuler;
	OutEuler.X = appRound(WindNum.X) * 360.f;
	OutEuler.Y = appRound(WindNum.Y) * 360.f;
	OutEuler.Z = appRound(WindNum.Z) * 360.f;
	return OutEuler;
}

static FMatrix GetBaseMatrix(AActor* Actor)
{
	check(Actor);

	FMatrix BaseTM = FMatrix::Identity;
	if(Actor->Base)
	{
		// Look at bone we are attached to if attached to a skeletal mesh component
		if(Actor->BaseSkelComponent)
		{
			INT BoneIndex = Actor->BaseSkelComponent->MatchRefBone(Actor->BaseBoneName);
			// If we can't find the bone, just use actor ref frame.
			if(BoneIndex != INDEX_NONE)
			{
				BaseTM = Actor->BaseSkelComponent->GetBoneMatrix(BoneIndex);
			}
			else
			{
				BaseTM = FRotationTranslationMatrix(Actor->Base->Rotation, Actor->Base->Location);
			}
		}
		// Not skeletal case - just use actor transform.
		else
		{
			BaseTM = FRotationTranslationMatrix(Actor->Base->Rotation, Actor->Base->Location);
		}
	}
	else
	{
		BaseTM = FMatrix::Identity;
	}

	BaseTM.RemoveScaling();
	return BaseTM;
}

/** Clamps values between -DELTA and DELTA to 0 */
static FLOAT ClampValueNearZero(FLOAT Value)
{
	FLOAT Result = Value;

	if(Abs<FLOAT>(Value) < DELTA)
	{
		Result = 0;
	}

	return Result;
}

/** Creates a rotator given a source matrix, it checks for very small floating point errors being passed to atan2 and clamps them accordingly. */
static FRotator GetCleanedUpRotator(const FMatrix &Mat)
{
	const FVector		XAxis	= Mat.GetAxis( 0 );
	const FVector		YAxis	= Mat.GetAxis( 1 );
	const FVector		ZAxis	= Mat.GetAxis( 2 );

	FRotator	Rotator	= FRotator( 
		appRound(appAtan2( ClampValueNearZero(XAxis.Z), ClampValueNearZero(appSqrt(Square(XAxis.X)+Square(XAxis.Y))) ) * 32768.f / PI), 
		appRound(appAtan2( ClampValueNearZero(XAxis.Y), ClampValueNearZero(XAxis.X) ) * 32768.f / PI), 
		0 
		);

	const FVector		SYAxis	= FRotationMatrix( Rotator ).GetAxis(1);
	Rotator.Roll		= appRound(appAtan2( ClampValueNearZero(ZAxis | SYAxis), ClampValueNearZero(YAxis | SYAxis) ) * 32768.f / PI);
	return Rotator;
}

/** Change the value of an existing keyframe. */
void UInterpTrackMove::UpdateKeyframe(INT KeyIndex, UInterpTrackInst* TrInst)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	if( KeyIndex < 0 || KeyIndex >= EulerTrack.Points.Num() )
	{
		return;
	}

	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}


	// Don't want to record keyframes if track disabled.
	if(bDisableMovement)
	{
		return;
	}

	UInterpTrackInstMove* MoveTrackInst = CastChecked<UInterpTrackInstMove>(TrInst);


	AActor* BaseActor = Actor->GetBase();
	if( BaseActor == NULL && MoveFrame == IMF_World )
	{
		// World space coordinates, and no base actor.  This will be easy!
		FVector NewPos = Actor->Location;
		FRotator NewRot = Actor->Rotation;

		if ( APawn * Pawn = Actor->GetAPawn() )
		{
			if ( Pawn->CylinderComponent )
			{
				NewPos.Z -= Pawn->CylinderComponent->CollisionHeight;
			}
		}

		PosTrack.Points(KeyIndex).OutVal = NewPos;
		EulerTrack.Points(KeyIndex).OutVal = NewRot.Euler();
	}
	else if( MoveFrame == IMF_World || MoveFrame == IMF_RelativeToInitial )
	{
		// First, figure out our reference transformation
		FMatrix RelativeToWorldTransform;
		if( MoveFrame == IMF_World )
		{
			// Our reference transform is simply our base actor's transform
			check( BaseActor != NULL );
			RelativeToWorldTransform = GetBaseMatrix( Actor );
		}
		else if( MoveFrame == IMF_RelativeToInitial )
		{
			if( BaseActor != NULL )
			{
				// Our reference transform is our initial transform combined with the base actor's transform
				RelativeToWorldTransform = MoveTrackInst->InitialTM * GetBaseMatrix( Actor );
			}
			else
			{
				// No base actor, so our reference transform is simply our initial transform
				RelativeToWorldTransform = MoveTrackInst->InitialTM;
			}
		}

		// Compute matrix that transforms from world space back into reference space
		FMatrix WorldToRelativeTransform = RelativeToWorldTransform.Inverse();


		// Take the Actor Rotator and turn it into Winding and Remainder parts
		FRotator WorldSpaceWindRot, WorldSpaceUnwoundRot;
		Actor->Rotation.GetWindingAndRemainder( WorldSpaceWindRot, WorldSpaceUnwoundRot );

		FVector NewPos = Actor->Location;

		if ( APawn * Pawn = Actor->GetAPawn() )
		{
			if ( Pawn->CylinderComponent )
			{
				NewPos.Z -= Pawn->CylinderComponent->CollisionHeight;
			}
		}
		// Compute matrix that transforms the actor into the reference coordinate system
		FRotationTranslationMatrix ActorToWorldUnwoundTransform( WorldSpaceUnwoundRot, NewPos );

		// @todo: We are losing possible over-winding data (from rotation + unwound relative rotation) by
		//        using matrices to concatenate transforms here.  This can cause winding direction errors!
		FMatrix ActorToRelativeUnwoundTransform = ActorToWorldUnwoundTransform * WorldToRelativeTransform;
	
		// Position key
		{
			PosTrack.Points( KeyIndex ).OutVal = ActorToRelativeUnwoundTransform.GetOrigin();
		}

		// Rotation key
		{
			// Compute any extra rotation incurred by an over-wound world space rotation
			FVector WorldSpaceWindEulerNum = WorldSpaceWindRot.Euler() / 360.0f;
			FVector RelativeSpaceWindEulerNum = WorldToRelativeTransform.TransformNormal( WorldSpaceWindEulerNum );
			FVector RelativeSpaceWindEuler = WindNumToEuler( RelativeSpaceWindEulerNum );

			// Compute the rotation amount, relative to the initial transform
			FRotator RelativeSpaceUnwoundRot = GetCleanedUpRotator( ActorToRelativeUnwoundTransform );
			FVector RelativeSpaceUnwoundEuler = RelativeSpaceUnwoundRot.Euler();

			// Combine the relative space unwound euler with the transformed winding amount
			FVector RelativeSpaceEuler = RelativeSpaceUnwoundEuler + RelativeSpaceWindEuler;

			// To mitigate winding errors caused by using matrices for concatenation on actors that
			// relative to an initial transform or base actor (or both), we peek at an adjacent key
			// frame to attempt to keep rotation continuous
			if( EulerTrack.Points.Num() > 1 )
			{
				INT AdjacentKeyIndex = ( KeyIndex > 0 ) ? ( KeyIndex - 1 ) : ( KeyIndex + 1 );
				const FVector AdjacentEuler = EulerTrack.Points( AdjacentKeyIndex ).OutVal;

				// Try to minimize differences in curves
				const FVector EulerDiff = RelativeSpaceEuler - AdjacentEuler;
				if( EulerDiff.X > 180.0f )
				{
					RelativeSpaceEuler.X -= 360.0f;
				}
				else if( EulerDiff.X < -180.0f )
				{
					RelativeSpaceEuler.X += 360.0f;
				}
				if( EulerDiff.Y > 180.0f )
				{
					RelativeSpaceEuler.Y -= 360.0f;
				}
				else if( EulerDiff.Y < -180.0f )
				{
					RelativeSpaceEuler.Y += 360.0f;
				}
				if( EulerDiff.Z > 180.0f )
				{
					RelativeSpaceEuler.Z -= 360.0f;
				}
				else if( EulerDiff.Z < -180.0f )
				{
					RelativeSpaceEuler.Z += 360.0f;
				}
			}

			// Store the final rotation amount
			EulerTrack.Points( KeyIndex ).OutVal = RelativeSpaceEuler;
		}
	}
	else 
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_UnknownInterpolationType") );
		PosTrack.Points(KeyIndex).OutVal = FVector(0.f);
		EulerTrack.Points(KeyIndex).OutVal = FVector(0.f);
	}

	// Update the tangent vectors for the changed point, and its neighbours.
	PosTrack.AutoSetTangents(LinCurveTension);
	EulerTrack.AutoSetTangents(AngCurveTension);
}

/**
 * Adds a keyframe to a child track 
 *
 * @param ChildTrack		The child track where the keyframe should be added
 * @param Time				What time the keyframe is located at
 * @param TrackInst			The track instance of the parent track(this track)
 * @param InitInterpMode	The initial interp mode for the keyframe?	 
 */
INT UInterpTrackMove::AddChildKeyframe( UInterpTrack* ChildTrack, FLOAT Time, UInterpTrackInst* ChildTrackInst, EInterpCurveMode InitInterpMode )
{
	INT NewKeyIndex = INDEX_NONE;
	UInterpTrackMoveAxis* ChildMoveTrack = CastChecked<UInterpTrackMoveAxis>( ChildTrack );
	AActor* Actor = ChildTrackInst->GetGroupActor();
	if( Actor )
	{
		// Add a new key to our track.
		NewKeyIndex = ChildMoveTrack->FloatTrack.AddPoint( Time, 0.0f );
		ChildMoveTrack->FloatTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

		FName DefaultName(NAME_None);
		INT NewLookupKeyIndex = ChildMoveTrack->LookupTrack.AddPoint(Time, DefaultName);
	
		check( NewKeyIndex == NewLookupKeyIndex );

		// First key of a 'relative to initial' track must always be zero
		if( MoveFrame == IMF_World || NewKeyIndex != 0 )
		{
			UpdateChildKeyframe( ChildTrack, NewKeyIndex, ChildTrackInst );
		}
	}

	return NewKeyIndex;
}

/**
 * Updates a child track keyframe
 *
 * @param ChildTrack		The child track with keyframe to update
 * @param KeyIndex			The index of the key to be updated
 * @param TrackInst			The track instance of the parent track(this track)
 */
void UInterpTrackMove::UpdateChildKeyframe( UInterpTrack* ChildTrack, INT KeyIndex, UInterpTrackInst* TrackInst )
{
	check(ChildTrack);

	UInterpTrackMoveAxis* ChildMoveTrack = CastChecked<UInterpTrackMoveAxis>( ChildTrack );
	const BYTE MoveAxis = ChildMoveTrack->MoveAxis;

	FInterpCurveFloat& FloatTrack = ChildMoveTrack->FloatTrack;
	if( KeyIndex < 0 || KeyIndex >= ChildMoveTrack->FloatTrack.Points.Num() )
	{
		return;
	}

	UInterpTrackInstMove* MoveTrackInst = CastChecked<UInterpTrackInstMove>( TrackInst );
	AActor* Actor = MoveTrackInst->GetGroupActor();
	if( !Actor )
	{
		return;
	}

	if( MoveFrame == IMF_RelativeToInitial && KeyIndex == 0 )
	{
		return;
	}

	if( bDisableMovement )
	{
		return;
	}

	// New position of the actor
	FVector NewPos;
	// New rotation of the actor
	FVector NewRot(0, 0, 0);
	AActor* BaseActor = Actor->GetBase();
	if( BaseActor == NULL && MoveFrame == IMF_World )
	{
		NewPos = Actor->Location;
		NewRot = Actor->Rotation.Euler();

		APawn* Pawn = Actor->GetAPawn();
		if( Pawn && Pawn->CylinderComponent )
		{
			NewPos.Z -= Pawn->CylinderComponent->CollisionHeight;
		}
	}
	else if( MoveFrame == IMF_World || MoveFrame == IMF_RelativeToInitial )
	{
		// First, figure out our reference transformation
		FMatrix RelativeToWorldTransform;
		if( MoveFrame == IMF_World )
		{
			// Our reference transform is simply our base actor's transform
			check( BaseActor != NULL );
			RelativeToWorldTransform = GetBaseMatrix( Actor );
		}
		else if( MoveFrame == IMF_RelativeToInitial )
		{
			if( BaseActor != NULL )
			{
				// Our reference transform is our initial transform combined with the base actor's transform
				RelativeToWorldTransform = MoveTrackInst->InitialTM * GetBaseMatrix( Actor );
			}
			else
			{
				// No base actor, so our reference transform is simply our initial transform
				RelativeToWorldTransform = MoveTrackInst->InitialTM;
			}
		}

		// Compute matrix that transforms from world space back into reference space
		FMatrix WorldToRelativeTransform = RelativeToWorldTransform.Inverse();


		// Take the Actor Rotator and turn it into Winding and Remainder parts
		FRotator WorldSpaceWindRot, WorldSpaceUnwoundRot;
		Actor->Rotation.GetWindingAndRemainder( WorldSpaceWindRot, WorldSpaceUnwoundRot );

		NewPos = Actor->Location;

		APawn* Pawn = Actor->GetAPawn();
		if( Pawn && Pawn->CylinderComponent )
		{
			NewPos.Z -= Pawn->CylinderComponent->CollisionHeight;
		}

		// Compute matrix that transforms the actor into the reference coordinate system
		FRotationTranslationMatrix ActorToWorldUnwoundTransform( WorldSpaceUnwoundRot, NewPos );

		// @todo: We are losing possible over-winding data (from rotation + unwound relative rotation) by
		//        using matrices to concatenate transforms here.  This can cause winding direction errors!
		FMatrix ActorToRelativeUnwoundTransform = ActorToWorldUnwoundTransform * WorldToRelativeTransform;

		if( MoveAxis == AXIS_TranslationX || MoveAxis == AXIS_TranslationY || MoveAxis == AXIS_TranslationZ )
		{
			// This track modifies position
			NewPos = ActorToRelativeUnwoundTransform.GetOrigin();
		}
		else if( MoveAxis == AXIS_RotationX || MoveAxis == AXIS_RotationY || MoveAxis == AXIS_RotationZ )
		{
			//This track modifies rotation
			// Compute any extra rotation incurred by an over-wound world space rotation
			FVector WorldSpaceWindEulerNum = WorldSpaceWindRot.Euler() / 360.0f;
			FVector RelativeSpaceWindEulerNum = WorldToRelativeTransform.TransformNormal( WorldSpaceWindEulerNum );
			FVector RelativeSpaceWindEuler = WindNumToEuler( RelativeSpaceWindEulerNum );

			// Compute the rotation amount, relative to the initial transform
			FRotator RelativeSpaceUnwoundRot = GetCleanedUpRotator( ActorToRelativeUnwoundTransform );
			FVector RelativeSpaceUnwoundEuler = RelativeSpaceUnwoundRot.Euler();

			// Combine the relative space unwound euler with the transformed winding amount
			FVector RelativeSpaceEuler = RelativeSpaceUnwoundEuler + RelativeSpaceWindEuler;

			// To mitigate winding errors caused by using matrices for concatenation on actors that
			// relative to an initial transform or base actor (or both), we peek at an adjacent key
			// frame to attempt to keep rotation continuous
			if( FloatTrack.Points.Num() > 1 )
			{
				INT AdjacentKeyIndex = ( KeyIndex > 0 ) ? ( KeyIndex - 1 ) : ( KeyIndex + 1 );
				FVector AdjacentEuler;
				// Determine what our float track actually represents.
				switch( MoveAxis )
				{
				case AXIS_RotationX:
					AdjacentEuler = FVector( FloatTrack.Points( AdjacentKeyIndex ).OutVal, 0, 0 );
					break;
				case AXIS_RotationY:
					AdjacentEuler = FVector( 0, FloatTrack.Points( AdjacentKeyIndex ).OutVal, 0 );
					break;
				case AXIS_RotationZ:
				default:
					AdjacentEuler = FVector( 0, 0, FloatTrack.Points( AdjacentKeyIndex ).OutVal );
					break;
				}

				// Try to minimize differences in curves
				const FVector EulerDiff = RelativeSpaceEuler - AdjacentEuler;
				if( EulerDiff.X > 180.0f )
				{
					RelativeSpaceEuler.X -= 360.0f;
				}
				else if( EulerDiff.X < -180.0f )
				{
					RelativeSpaceEuler.X += 360.0f;
				}
				if( EulerDiff.Y > 180.0f )
				{
					RelativeSpaceEuler.Y -= 360.0f;
				}
				else if( EulerDiff.Y < -180.0f )
				{
					RelativeSpaceEuler.Y += 360.0f;
				}
				if( EulerDiff.Z > 180.0f )
				{
					RelativeSpaceEuler.Z -= 360.0f;
				}
				else if( EulerDiff.Z < -180.0f )
				{
					RelativeSpaceEuler.Z += 360.0f;
				}
			}

			// Store the final rotation amount
			NewRot = RelativeSpaceEuler;
		}
	}
	else 
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_UnknownInterpolationType") );
		FloatTrack.Points(KeyIndex).OutVal = 0.f;
	}


	// Now determine what value should be updated in the float track.
	switch( MoveAxis )
	{
	case AXIS_TranslationX:
		FloatTrack.Points(KeyIndex).OutVal = NewPos.X;
		break;
	case AXIS_TranslationY:
		FloatTrack.Points(KeyIndex).OutVal = NewPos.Y;
		break;
	case AXIS_TranslationZ:
		FloatTrack.Points(KeyIndex).OutVal = NewPos.Z;
		break;
	case AXIS_RotationX:
		FloatTrack.Points(KeyIndex).OutVal = NewRot.X;
		break;
	case AXIS_RotationY:
		FloatTrack.Points(KeyIndex).OutVal = NewRot.Y;
		break;
	case AXIS_RotationZ:
		FloatTrack.Points(KeyIndex).OutVal = NewRot.Z;
		break;
	default:
		checkf( FALSE, TEXT("Invalid Move axis") );
	}

	// Update the tangent vectors for the changed point, and its neighbors.
	FloatTrack.AutoSetTangents( ChildMoveTrack->CurveTension );
}

/** Change the time position of an existing keyframe. This can change the index of the keyframe - the new index is returned. */
INT UInterpTrackMove::SetKeyframeTime(INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	if( KeyIndex < 0 || KeyIndex >= PosTrack.Points.Num() )
		return KeyIndex;

	INT NewKeyIndex = KeyIndex;
	if(bUpdateOrder)
	{
		NewKeyIndex = PosTrack.MovePoint(KeyIndex, NewKeyTime);
		INT NewEulerKeyIndex = EulerTrack.MovePoint(KeyIndex, NewKeyTime);
		INT NewLookupKeyIndex = LookupTrack.MovePoint(KeyIndex, NewKeyTime);
		check( (NewKeyIndex == NewEulerKeyIndex) && (NewKeyIndex == NewLookupKeyIndex) );
	}
	else
	{
		PosTrack.Points(KeyIndex).InVal = NewKeyTime;
		EulerTrack.Points(KeyIndex).InVal = NewKeyTime;
		LookupTrack.Points(KeyIndex).Time = NewKeyTime;
	}

	PosTrack.AutoSetTangents(LinCurveTension);
	EulerTrack.AutoSetTangents(AngCurveTension);

	return NewKeyIndex;
}

/** Remove the keyframe with the given index. */
void UInterpTrackMove::RemoveKeyframe(INT KeyIndex)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	if( KeyIndex < 0 || KeyIndex >= PosTrack.Points.Num() )
		return;

	PosTrack.Points.Remove(KeyIndex);
	EulerTrack.Points.Remove(KeyIndex);
	LookupTrack.Points.Remove(KeyIndex);

	PosTrack.AutoSetTangents(LinCurveTension);
	EulerTrack.AutoSetTangents(AngCurveTension);
}

/** 
 *	Duplicate the keyframe with the given index to the specified time. 
 *	Returns the index of the newly created key.
 */
INT UInterpTrackMove::DuplicateKeyframe(INT KeyIndex, FLOAT NewKeyTime)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	if( KeyIndex < 0 || KeyIndex >= PosTrack.Points.Num() )
		return INDEX_NONE;

	FInterpCurvePoint<FVector> PosPoint = PosTrack.Points(KeyIndex);
	INT NewPosIndex = PosTrack.AddPoint(NewKeyTime, FVector(0.f));
	PosTrack.Points(NewPosIndex) = PosPoint; // Copy properties from source key.
	PosTrack.Points(NewPosIndex).InVal = NewKeyTime;

	FInterpCurvePoint<FVector> EulerPoint = EulerTrack.Points(KeyIndex);
	INT NewEulerIndex = EulerTrack.AddPoint(NewKeyTime, FVector(0.f));
	EulerTrack.Points(NewEulerIndex) = EulerPoint;
	EulerTrack.Points(NewEulerIndex).InVal = NewKeyTime;

	FName OldName = LookupTrack.Points(KeyIndex).GroupName;
	INT NewLookupKeyIndex = LookupTrack.AddPoint(NewKeyTime, OldName);

	check((NewPosIndex == NewEulerIndex) && (NewPosIndex == NewLookupKeyIndex));

	PosTrack.AutoSetTangents(LinCurveTension);
	EulerTrack.AutoSetTangents(AngCurveTension);

	return NewPosIndex;
}

/** Return the closest time to the time passed in that we might want to snap to. */
UBOOL UInterpTrackMove::GetClosestSnapPosition(FLOAT InPosition, TArray<INT> &IgnoreKeys, FLOAT& OutPosition)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );

	if(PosTrack.Points.Num() == 0)
	{
		return false;
	}

	UBOOL bFoundSnap = false;
	FLOAT ClosestSnap = 0.f;
	FLOAT ClosestDist = BIG_NUMBER;
	for(INT i=0; i<PosTrack.Points.Num(); i++)
	{
		if(!IgnoreKeys.ContainsItem(i))
		{
			FLOAT Dist = Abs( PosTrack.Points(i).InVal - InPosition );
			if(Dist < ClosestDist)
			{
				ClosestSnap = PosTrack.Points(i).InVal;
				ClosestDist = Dist;
				bFoundSnap = true;
			}
		}
	}

	OutPosition = ClosestSnap;
	return bFoundSnap;
}

/** 
 *	Conditionally calls PreviewUpdateTrack depending on whether or not the track is enabled.
 */
void UInterpTrackMove::ConditionalPreviewUpdateTrack(FLOAT NewPosition, class UInterpTrackInst* TrInst)
{
	// Is the track enabled?
	UBOOL bIsTrackEnabled = !IsDisabled();
	UInterpGroupInst* GrInst = Cast<UInterpGroupInst>( TrInst->GetOuter() );
	if( GrInst != NULL )
	{
		USeqAct_Interp* Seq = Cast<USeqAct_Interp>( GrInst->GetOuter() );
		if( Seq != NULL )
		{
			if( ActiveCondition == ETAC_GoreEnabled && !Seq->bShouldShowGore ||
				ActiveCondition == ETAC_GoreDisabled && Seq->bShouldShowGore )
			{
				bIsTrackEnabled = FALSE;
			}
		}
	}

	FLOAT CurTime = NewPosition;

	if( !bIsTrackEnabled )
	{
		CurTime = 0.0f;
	}

	PreviewUpdateTrack(CurTime, TrInst);
}

/** 
 *	Function which actually updates things based on the new position in the track. 
 *  This is called in the editor, when scrubbing/previewing etc.
 */
void UInterpTrackMove::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(bDisableMovement)
	{
		NewPosition = 0.0f;
	}

	FVector NewPos = Actor->Location;
	FRotator NewRot = Actor->Rotation;

	if (GetLocationAtTime(TrInst, NewPosition, NewPos, NewRot))
	{
		// Allow subclasses to adjust position
		Actor->AdjustInterpTrackMove(NewPos, NewRot, 0.f, RotMode == IMR_Ignore);

		Actor->Location = NewPos;
		Actor->Rotation = NewRot;

		// Don't force update all components unless we're in the editor.
		Actor->ConditionalForceUpdateComponents();

		// Update actors Based on this one once it moves.
		UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
		GrInst->UpdateAttachedActors();
	}
}

void UInterpTrackMove::PostLoad()
{
	Super::PostLoad();
}

void UInterpTrackMove::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );

	PosTrack.AutoSetTangents(LinCurveTension);
	EulerTrack.AutoSetTangents(AngCurveTension);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/** Called after copying/pasting this track.  Makes sure that the number of keys in the 3 keyframe arrays matchup properly. */
void UInterpTrackMove::PostEditImport()
{
	Super::PostEditImport();

	// Make sure that our array sizes match up.  If they don't, it is due to default struct keys not being exported. (Only happens for keys at Time=0).
	// @todo: This is a hack and can be removed once the struct properties are exported to text correctly for arrays of structs.
	if(PosTrack.Points.Num() > LookupTrack.Points.Num())	// Lookup track elements weren't imported.
	{
		INT Count = PosTrack.Points.Num()-LookupTrack.Points.Num();
		FName DefaultName(NAME_None);
		for(INT PointIdx=0; PointIdx<Count; PointIdx++)
		{
			LookupTrack.AddPoint(PosTrack.Points(PointIdx).InVal, DefaultName);
		}

		for(INT PointIdx=Count; PointIdx<PosTrack.Points.Num(); PointIdx++)
		{
			LookupTrack.Points(PointIdx).Time = PosTrack.Points(PointIdx).InVal;
		}
	}
	else if(PosTrack.Points.Num()==EulerTrack.Points.Num() && PosTrack.Points.Num() < LookupTrack.Points.Num())	// Pos/euler track elements weren't imported.
	{
		INT Count = LookupTrack.Points.Num()-PosTrack.Points.Num();

		for(INT PointIdx=0; PointIdx<Count; PointIdx++)
		{
			PosTrack.AddPoint( LookupTrack.Points(PointIdx).Time, FVector(0,0,0) );
			EulerTrack.AddPoint( LookupTrack.Points(PointIdx).Time, FVector(0.f) );
		}

		for(INT PointIdx=Count; PointIdx<LookupTrack.Points.Num(); PointIdx++)
		{
			PosTrack.Points(PointIdx).InVal = LookupTrack.Points(PointIdx).Time;
			EulerTrack.Points(PointIdx).InVal = LookupTrack.Points(PointIdx).Time;
		}

		PosTrack.AutoSetTangents(LinCurveTension);
		EulerTrack.AutoSetTangents(AngCurveTension);
	}

	check((PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()));
}

/**
 * @param KeyIndex	Index of the key to retrieve the lookup group name for.
 *
 * @return Returns the groupname for the keyindex specified.
 */
FName UInterpTrackMove::GetLookupKeyGroupName(INT KeyIndex)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	check( KeyIndex < LookupTrack.Points.Num() );

	return LookupTrack.Points(KeyIndex).GroupName;
}

/**
 * Sets the lookup group name for a movement track keyframe.
 *
 * @param KeyIndex			Index of the key to modify.
 * @param NewGroupName		Group name to set the keyframe's lookup group to.
 */
void UInterpTrackMove::SetLookupKeyGroupName(INT KeyIndex, const FName &NewGroupName)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	check( KeyIndex < LookupTrack.Points.Num() );

	LookupTrack.Points(KeyIndex).GroupName = NewGroupName;
}

/**
 * Clears the lookup group name for a movement track keyframe.
 *
 * @param KeyIndex			Index of the key to modify.
 */
void UInterpTrackMove::ClearLookupKeyGroupName(INT KeyIndex)
{
	FName DefaultName(NAME_None);
	SetLookupKeyGroupName(KeyIndex, DefaultName);
}

/**
 * Gets the position of a keyframe given its key index.  Also optionally retrieves the Arrive and Leave tangents for the key.
 * This function respects the LookupTrack.
 *
 * @param TrInst			TrackInst to use for lookup track positions.
 * @param KeyIndex			Index of the keyframe to get the position of.
 * @param OutTime			Final time of the keyframe.
 * @param OutPos			Final position of the keyframe.
 * @param OutArriveTangent	Pointer to a vector to store the arrive tangent in, can be NULL.
 * @param OutLeaveTangent	Pointer to a vector to store the leave tangent in, can be NULL.
 */
void UInterpTrackMove::GetKeyframePosition(UInterpTrackInst* TrInst, INT KeyIndex, FLOAT& OutTime, FVector &OutPos, FVector *OutArriveTangent, FVector *OutLeaveTangent)
{
	UBOOL bUsePosTrack = TRUE;

	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	check( KeyIndex < LookupTrack.Points.Num() );

	// See if this key is trying to get its position from another group.
	FName GroupName = LookupTrack.Points(KeyIndex).GroupName;
	if(GroupName != NAME_None && TrInst)
	{
		// Lookup position from the lookup track.
		AActor* Actor = TrInst->GetGroupActor();
		UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
		USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
		UInterpGroupInst* LookupGroupInst = Seq->FindFirstGroupInstByName(GroupName);

		if(Actor && LookupGroupInst && LookupGroupInst->GetGroupActor())
		{
			AActor* LookupActor = LookupGroupInst->GetGroupActor();

			// Slight hack here so that if we are trying to look at a Player variable, it looks at their Pawn.
			APlayerController* PC = Cast<APlayerController>(LookupActor);
			if(PC && PC->Pawn)
			{
				LookupActor = PC->Pawn;
			}

			// Find position
			OutPos = LookupActor->Location;
			OutTime = LookupTrack.Points( KeyIndex ).Time;

			// Find arrive and leave tangents.
			if(OutLeaveTangent != NULL || OutArriveTangent != NULL)
			{
				if(KeyIndex==0 || KeyIndex==(LookupTrack.Points.Num()-1))	// if we are an endpoint, set tangents to 0.
				{
					if(OutArriveTangent!=NULL)
					{
						appMemset( OutArriveTangent, 0, sizeof(FVector) );
					}

					if(OutLeaveTangent != NULL)
					{
						appMemset( OutLeaveTangent, 0, sizeof(FVector) );
					}
				}
				else
				{
					FVector PrevPos, NextPos;
					FLOAT PrevTime, NextTime;
					FVector AutoTangent;

					// Get previous and next positions for the tangents.
					GetKeyframePosition(TrInst, KeyIndex-1, PrevTime, PrevPos, NULL, NULL);
					GetKeyframePosition(TrInst, KeyIndex+1, NextTime, NextPos, NULL, NULL);

					if( PosTrack.InterpMethod == IMT_UseFixedTangentEvalAndNewAutoTangents )
					{
						// @todo: Should this setting be exposed in some way to the Lookup track?
						const UBOOL bWantClamping = FALSE;

						ComputeCurveTangent(
							PrevTime, PrevPos,
							OutTime, OutPos,
							NextTime, NextPos,
							LinCurveTension,				// Tension
							bWantClamping,
							AutoTangent );					// Out
					}
					else
					{
						LegacyAutoCalcTangent( PrevPos, OutPos, NextPos, LinCurveTension, AutoTangent );
					}

					if(OutArriveTangent!=NULL)
					{
						*OutArriveTangent = AutoTangent;
					}

					if(OutLeaveTangent != NULL)
					{
						*OutLeaveTangent = AutoTangent;
					}
				}
			}

			bUsePosTrack = FALSE;
		}
	}

	// We couldn't lookup a position from another group, so use the value stored in the pos track.
	if(bUsePosTrack)
	{
		OutTime = PosTrack.Points(KeyIndex).InVal;
		OutPos = PosTrack.Points(KeyIndex).OutVal;

		if(OutArriveTangent != NULL)
		{
			*OutArriveTangent = PosTrack.Points(KeyIndex).ArriveTangent;
		}

		if(OutLeaveTangent != NULL)
		{
			*OutLeaveTangent = PosTrack.Points(KeyIndex).LeaveTangent;
		}
	}
}


/**
 * Gets the rotation of a keyframe given its key index.  Also optionally retrieves the Arrive and Leave tangents for the key.
 * This function respects the LookupTrack.
 *
 * @param TrInst			TrackInst to use for lookup track rotations.
 * @param KeyIndex			Index of the keyframe to get the rotation of.
 * @param OutTime			Final time of the keyframe.
 * @param OutRot			Final rotation of the keyframe.
 * @param OutArriveTangent	Pointer to a vector to store the arrive tangent in, can be NULL.
 * @param OutLeaveTangent	Pointer to a vector to store the leave tangent in, can be NULL.
 */
void UInterpTrackMove::GetKeyframeRotation(UInterpTrackInst* TrInst, INT KeyIndex, FLOAT& OutTime, FVector &OutRot, FVector *OutArriveTangent, FVector *OutLeaveTangent)
{
	UBOOL bUseRotTrack = TRUE;

	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	check( KeyIndex < LookupTrack.Points.Num() );

	// See if this key is trying to get its rotation from another group.
	FName GroupName = LookupTrack.Points(KeyIndex).GroupName;
	if(GroupName != NAME_None && TrInst)
	{
		// Lookup rotation from the lookup track.
		AActor* Actor = TrInst->GetGroupActor();
		UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
		USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
		UInterpGroupInst* LookupGroupInst = Seq->FindFirstGroupInstByName(GroupName);

		if(Actor && LookupGroupInst && LookupGroupInst->GetGroupActor())
		{
			AActor* LookupActor = LookupGroupInst->GetGroupActor();

			// Slight hack here so that if we are trying to look at a Player variable, it looks at their Pawn.
			APlayerController* PC = Cast<APlayerController>(LookupActor);
			if(PC && PC->Pawn)
			{
				LookupActor = PC->Pawn;
			}

			// Find rotation
			OutRot = LookupActor->Rotation.Euler();
			OutTime = LookupTrack.Points( KeyIndex ).Time;

			// Find arrive and leave tangents.
			if(OutLeaveTangent != NULL || OutArriveTangent != NULL)
			{
				if(KeyIndex==0 || KeyIndex==(LookupTrack.Points.Num()-1))	// if we are an endpoint, set tangents to 0.
				{
					if(OutArriveTangent!=NULL)
					{
						appMemset( OutArriveTangent, 0, sizeof(FVector) );
					}

					if(OutLeaveTangent != NULL)
					{
						appMemset( OutLeaveTangent, 0, sizeof(FVector) );
					}
				}
				else
				{
					FVector PrevRot, NextRot;
					FLOAT PrevTime, NextTime;
					FVector AutoTangent;

					// Get previous and next positions for the tangents.
					GetKeyframeRotation(TrInst, KeyIndex-1, PrevTime, PrevRot, NULL, NULL);
					GetKeyframeRotation(TrInst, KeyIndex+1, NextTime, NextRot, NULL, NULL);

					if( EulerTrack.InterpMethod == IMT_UseFixedTangentEvalAndNewAutoTangents )
					{
						// @todo: Should this setting be exposed in some way to the Lookup track?
						const UBOOL bWantClamping = FALSE;

						ComputeCurveTangent(
							PrevTime, PrevRot,
							OutTime, OutRot,
							NextTime, NextRot,
							LinCurveTension,				// Tension
							bWantClamping,
							AutoTangent );					// Out
					}
					else
					{
						LegacyAutoCalcTangent( PrevRot, OutRot, NextRot, LinCurveTension, AutoTangent );
					}

					if(OutArriveTangent!=NULL)
					{
						*OutArriveTangent = AutoTangent;
					}

					if(OutLeaveTangent != NULL)
					{
						*OutLeaveTangent = AutoTangent;
					}
				}
			}

			bUseRotTrack = FALSE;
		}
	}

	// We couldn't lookup a position from another group, so use the value stored in the pos track.
	if(bUseRotTrack)
	{
		OutTime = EulerTrack.Points(KeyIndex).InVal;
		OutRot = EulerTrack.Points(KeyIndex).OutVal;

		if(OutArriveTangent != NULL)
		{
			*OutArriveTangent = EulerTrack.Points(KeyIndex).ArriveTangent;
		}

		if(OutLeaveTangent != NULL)
		{
			*OutLeaveTangent = EulerTrack.Points(KeyIndex).LeaveTangent;
		}
	}
}


/**
 * Replacement for the PosTrack eval function that uses GetKeyframePosition.  This is so we can replace keyframes that get their information from other tracks.
 *
 * @param TrInst	TrackInst to use for looking up groups.
 * @param Time		Time to evaluate position at.
 * @return			Final position at the specified time.
 */
FVector UInterpTrackMove::EvalPositionAtTime(UInterpTrackInst* TrInst, FLOAT Time)
{
	// If there are no subtracks, get the position directly from this track.
	if( SubTracks.Num() == 0 )
	{
		FVector OutPos;
		FLOAT KeyTime;	// Not used here
		const INT NumPoints = PosTrack.Points.Num();

		// If no point in curve, return the Default value we passed in.
		if( NumPoints == 0 )
		{
			return FVector(0.f);
		}

		// If only one point, or before the first point in the curve, return the first points value.
		if( NumPoints < 2 || (Time <= PosTrack.Points(0).InVal) )
		{
			GetKeyframePosition(TrInst, 0, KeyTime, OutPos, NULL, NULL);
			return OutPos;
		}

		// If beyond the last point in the curve, return its value.
		if( Time >= PosTrack.Points(NumPoints-1).InVal )
		{
			GetKeyframePosition(TrInst, NumPoints - 1, KeyTime, OutPos, NULL, NULL);
			return OutPos;
		}

		// Somewhere with curve range - linear search to find value.
		for( INT i=1; i<NumPoints; i++ )
		{	
			if( Time < PosTrack.Points(i).InVal )
			{
				const FLOAT Diff = PosTrack.Points(i).InVal - PosTrack.Points(i-1).InVal;

				if( Diff > 0.f && PosTrack.Points(i-1).InterpMode != CIM_Constant )
				{
					const FLOAT Alpha = (Time - PosTrack.Points(i-1).InVal) / Diff;

					if( PosTrack.Points(i-1).InterpMode == CIM_Linear )	// Linear interpolation
					{
						FVector PrevPos, CurrentPos;
						GetKeyframePosition(TrInst, i-1, KeyTime, PrevPos, NULL, NULL);
						GetKeyframePosition(TrInst, i, KeyTime, CurrentPos, NULL, NULL);

						OutPos =  Lerp( PrevPos, CurrentPos, Alpha );
						return OutPos;
					}
					else	//Cubic Interpolation
					{
						// Get keyframe positions and tangents.
						FVector CurrentPos, CurrentArriveTangent, PrevPos, PrevLeaveTangent;
						GetKeyframePosition(TrInst, i-1, KeyTime, PrevPos, NULL, &PrevLeaveTangent);
						GetKeyframePosition(TrInst, i, KeyTime, CurrentPos, &CurrentArriveTangent, NULL);

						if(PosTrack.InterpMethod == IMT_UseBrokenTangentEval)
						{
							OutPos = CubicInterp( PrevPos, PrevLeaveTangent, CurrentPos, CurrentArriveTangent, Alpha );
							return OutPos;
						}
						else
						{
							OutPos = CubicInterp( PrevPos, PrevLeaveTangent * Diff, CurrentPos, CurrentArriveTangent * Diff, Alpha );
							return OutPos;
						}
					}
				}
				else	// Constant Interpolation
				{
					GetKeyframePosition(TrInst, i-1, KeyTime, OutPos, NULL, NULL);
					return OutPos;
				}
			}
		}

		// Shouldn't really reach here.
		GetKeyframePosition(TrInst, NumPoints-1, KeyTime, OutPos, NULL, NULL);
		return OutPos;
	}
	else
	{
		// This track has subtracks, get position information from subtracks
		FVector OutPos;

		UInterpTrackMoveAxis* PosXTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks(AXIS_TranslationX) );
		UInterpTrackMoveAxis* PosYTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks(AXIS_TranslationY) );
		UInterpTrackMoveAxis* PosZTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks(AXIS_TranslationZ) );

		OutPos.X = PosXTrack->EvalValueAtTime( TrInst, Time );
		OutPos.Y = PosYTrack->EvalValueAtTime( TrInst, Time );
		OutPos.Z = PosZTrack->EvalValueAtTime( TrInst, Time );

		return OutPos;
	}
}


/**
 * Replacement for the RotTrack eval function that uses GetKeyframeRotation.  This is so we can replace keyframes that get their information from other tracks.
 *
 * @param TrInst	TrackInst to use for looking up groups.
 * @param Time		Time to evaluate rotation at.
 * @return			Final rotation at the specified time.
 */
FVector UInterpTrackMove::EvalRotationAtTime(UInterpTrackInst* TrInst, FLOAT Time)
{
	// IF the track has no subtracks, get rotation information directly from this track
	if( SubTracks.Num() == 0 )
	{
		FVector OutRot;
		FLOAT KeyTime;	// Not used here
		const INT NumPoints = EulerTrack.Points.Num();

		// If no point in curve, return the Default value we passed in.
		if( NumPoints == 0 )
		{
			return FVector(0.f);
		}

		// If only one point, or before the first point in the curve, return the first points value.
		if( NumPoints < 2 || (Time <= EulerTrack.Points(0).InVal) )
		{
			GetKeyframeRotation(TrInst, 0, KeyTime, OutRot, NULL, NULL);
			return OutRot;
		}

		// If beyond the last point in the curve, return its value.
		if( Time >= EulerTrack.Points(NumPoints-1).InVal )
		{
			GetKeyframeRotation(TrInst, NumPoints - 1, KeyTime, OutRot, NULL, NULL);
			return OutRot;
		}

		// Somewhere with curve range - linear search to find value.
		for( INT i=1; i<NumPoints; i++ )
		{	
			if( Time < EulerTrack.Points(i).InVal )
			{
				const FLOAT Diff = EulerTrack.Points(i).InVal - EulerTrack.Points(i-1).InVal;

				if( Diff > 0.f && EulerTrack.Points(i-1).InterpMode != CIM_Constant )
				{
					const FLOAT Alpha = (Time - EulerTrack.Points(i-1).InVal) / Diff;

					if( EulerTrack.Points(i-1).InterpMode == CIM_Linear )	// Linear interpolation
					{
						FVector PrevRot, CurrentRot;
						GetKeyframeRotation(TrInst, i-1, KeyTime, PrevRot, NULL, NULL);
						GetKeyframeRotation(TrInst, i, KeyTime, CurrentRot, NULL, NULL);

						OutRot =  Lerp( PrevRot, CurrentRot, Alpha );
						return OutRot;
					}
					else	//Cubic Interpolation
					{
						// Get keyframe rotations and tangents.
						FVector CurrentRot, CurrentArriveTangent, PrevRot, PrevLeaveTangent;
						GetKeyframeRotation(TrInst, i-1, KeyTime, PrevRot, NULL, &PrevLeaveTangent);
						GetKeyframeRotation(TrInst, i, KeyTime, CurrentRot, &CurrentArriveTangent, NULL);

						if(EulerTrack.InterpMethod == IMT_UseBrokenTangentEval)
						{
							OutRot = CubicInterp( PrevRot, PrevLeaveTangent, CurrentRot, CurrentArriveTangent, Alpha );
							return OutRot;
						}
						else
						{
							OutRot = CubicInterp( PrevRot, PrevLeaveTangent * Diff, CurrentRot, CurrentArriveTangent * Diff, Alpha );
							return OutRot;
						}
					}
				}
				else	// Constant Interpolation
				{
					GetKeyframeRotation(TrInst, i-1, KeyTime, OutRot, NULL, NULL);
					return OutRot;
				}
			}
		}

		// Shouldn't really reach here.
		GetKeyframeRotation(TrInst, NumPoints-1, KeyTime, OutRot, NULL, NULL);
		return OutRot;
	}
	else
	{
		// Track subtracks, find the rotation tracks and get the new rotation from them.
		FVector OutRot;

		UInterpTrackMoveAxis* RotXTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks(AXIS_RotationX) );
		UInterpTrackMoveAxis* RotYTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks(AXIS_RotationY) );
		UInterpTrackMoveAxis* RotZTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks(AXIS_RotationZ) );

		OutRot.X = RotXTrack->EvalValueAtTime( TrInst, Time );
		OutRot.Y = RotYTrack->EvalValueAtTime( TrInst, Time );
		OutRot.Z = RotZTrack->EvalValueAtTime( TrInst, Time );

		return OutRot;
	}
}


void UInterpTrackMove::GetKeyTransformAtTime(UInterpTrackInst* TrInst, FLOAT Time, FVector& OutPos, FRotator& OutRot)
{
	// If the tracks has no subtracks, get new transform directly from this track
	if( SubTracks.Num() == 0 )
	{
		FQuat KeyQuat;
		FLOAT KeyTime;	// Not used here
		if(bUseQuatInterpolation)
		{
			INT NumPoints = EulerTrack.Points.Num();

			// If no point in curve, return the Default value we passed in.
			if( NumPoints == 0 )
			{
				KeyQuat = FQuat::Identity;
			}
			// If only one point, or before the first point in the curve, return the first points value.
			else if( NumPoints < 2 || (Time <= EulerTrack.Points(0).InVal) )
			{
				FVector OutRot;
				GetKeyframeRotation(TrInst, 0, KeyTime, OutRot, NULL, NULL);
				KeyQuat = FQuat::MakeFromEuler(OutRot);
			}
			// If beyond the last point in the curve, return its value.
			else if( Time >= EulerTrack.Points(NumPoints-1).InVal )
			{
				FVector OutRot;
				GetKeyframeRotation(TrInst, NumPoints-1, KeyTime, OutRot, NULL, NULL);
				KeyQuat = FQuat::MakeFromEuler(OutRot);
			}
			// Somewhere with curve range - linear search to find value.
			else
			{			
				UBOOL bFoundPos = false;
				for( INT KeyIdx=1; KeyIdx<NumPoints && !bFoundPos; KeyIdx++ )
				{	
					if( Time < EulerTrack.Points(KeyIdx).InVal )
					{
						FLOAT Delta = EulerTrack.Points(KeyIdx).InVal - EulerTrack.Points(KeyIdx-1).InVal;
						FLOAT Alpha = Clamp( (Time - EulerTrack.Points(KeyIdx-1).InVal) / Delta, 0.f, 1.f );
						FVector CurrentRot, PrevRot;

						GetKeyframeRotation(TrInst, KeyIdx-1, KeyTime, PrevRot, NULL, NULL);
						GetKeyframeRotation(TrInst, KeyIdx, KeyTime, CurrentRot, NULL, NULL);

						FQuat Key1Quat = FQuat::MakeFromEuler(PrevRot);
						FQuat Key2Quat = FQuat::MakeFromEuler(CurrentRot);

						KeyQuat = SlerpQuat( Key1Quat, Key2Quat, Alpha );

						bFoundPos = true;
					}
				}
			}

			OutRot = FRotator(KeyQuat);
		}
		else
		{
			OutRot = FRotator::MakeFromEuler( EvalRotationAtTime(TrInst, Time) );
		}

		// Evaluate position
		OutPos = EvalPositionAtTime(TrInst, Time);
	}
	else
	{
		// Evaluate rotation from subtracks
		OutRot = FRotator::MakeFromEuler( EvalRotationAtTime(TrInst, Time) );

		// Evaluate position from subtracks
		OutPos = EvalPositionAtTime(TrInst, Time);
	}
}

FLOAT GetDistanceFromAxis(EAxis WeightAxis, const FVector & Eval, const FVector & Base)
{
	switch (WeightAxis)
	{
	case AXIS_X:
		return fabs(Eval.X-Base.X);
	case AXIS_Y:
		return fabs(Eval.Y-Base.Y);
	case AXIS_Z:
		return fabs(Eval.Z-Base.Z);
	case AXIS_XY:
		return appSqrt((Eval.X-Base.X)*(Eval.X-Base.X)+(Eval.Y-Base.Y)*(Eval.Y-Base.Y));
	case AXIS_XZ:
		return appSqrt((Eval.X-Base.X)*(Eval.X-Base.X)+(Eval.Z-Base.Z)*(Eval.Z-Base.Z));
	case AXIS_YZ:
		return appSqrt((Eval.Y-Base.Y)*(Eval.Y-Base.Y)+(Eval.Z-Base.Z)*(Eval.Z-Base.Z));
	case AXIS_XYZ:
		return (Eval-Base).Size();
	}

	return 0.f;
}
/** 
 * Find Best Matching Time From Position
 * This function simply try to find Time from input Position using simple Lerp
 * 
 * @param : Pos  - input position
 * @param : StartKeyIndex - optional
 *
 * @return : Interp Time
 */
 FLOAT UInterpTrackMove::FindBestMatchingTimefromPosition(UInterpTrackInst* TrInst, const FVector& Pos, INT StartKeyIndex,  EAxis WeightAxis )
{
	// If the tracks has no subtracks, get new transform directly from this track
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );

	FLOAT OutTime = -1.f, MaxError = BIG_NUMBER, CurrentError, CurrentTime;
	FVector CurrentPosition;

	// we're looking for key1, and key 2 that has this position between
	FLOAT KeyIndex1Time = 0, KeyIndex2Time = 0;
	FVector KeyIndex1Position, KeyIndex2Position;
	// need to interpolate, find the 2 keys this position is between
	INT KeyIndex1=-1, KeyIndex2=-1;

	// find first key - closest
	for (INT KeyIndex=StartKeyIndex; KeyIndex < PosTrack.Points.Num(); ++KeyIndex)
	{
		GetKeyframePosition(TrInst, KeyIndex, CurrentTime, CurrentPosition, NULL, NULL);

		CurrentError = GetDistanceFromAxis(WeightAxis, CurrentPosition, Pos);

		if (CurrentError < MaxError)
		{
			OutTime = CurrentTime;
			MaxError = CurrentError;
			KeyIndex1Time = CurrentTime;
			KeyIndex1=KeyIndex;
			KeyIndex1Position = CurrentPosition;
		}
		// if current error is getting bigger than maxerror
		// that means, it's going away from it. 
		else if (CurrentError > MaxError)
		{
			break;
		}
	}

	// if Error is less than 10, or we didn't find, we don't care - that should be it
	if (MaxError < 10 || KeyIndex1==-1)
	{
		return OutTime;
	}

	// otherwise, find the second key
	// it should be either KeyIndex1-1 or KeyIndex+1
	if (KeyIndex1-1 > 0)
	{
		GetKeyframePosition(TrInst, KeyIndex1-1, CurrentTime, CurrentPosition, NULL, NULL);
		KeyIndex2Time = CurrentTime;
		KeyIndex2Position = CurrentPosition;
		// save first key error
		FLOAT KeyIndex1Error = GetDistanceFromAxis(WeightAxis, CurrentPosition, Pos);

		// try to find later key
		if (KeyIndex1+1 < PosTrack.Points.Num())
		{
			GetKeyframePosition(TrInst, KeyIndex1+1, CurrentTime, CurrentPosition, NULL, NULL);			
			FLOAT KeyIndex2Error = GetDistanceFromAxis(WeightAxis, CurrentPosition, Pos);

			// if first key is lower, then use first key as second key
			if (KeyIndex1Error < KeyIndex2Error)
			{
				KeyIndex2=KeyIndex1-1;
			}
			else
			{
				// if not, it's later key that's closer, use that ase second key
				KeyIndex2=KeyIndex1+1;
				KeyIndex2Time = CurrentTime;
				KeyIndex2Position = CurrentPosition;
			}
		}
		else
		{
			KeyIndex2=KeyIndex1-1;
		}
	}
	else if (KeyIndex1+1 < PosTrack.Points.Num())
	{
		GetKeyframePosition(TrInst, KeyIndex1+1, CurrentTime, CurrentPosition, NULL, NULL);			
		KeyIndex2=KeyIndex1+1;
		KeyIndex2Time = CurrentTime;
		KeyIndex2Position = CurrentPosition;
	}

	// found second key
	if (KeyIndex2 != -1)
	{
		FLOAT Alpha = GetDistanceFromAxis(WeightAxis, KeyIndex1Position, Pos)/GetDistanceFromAxis(WeightAxis, KeyIndex2Position, KeyIndex1Position);
		OutTime = Lerp(KeyIndex1Time, KeyIndex2Time, Alpha);
#if 0
		// debug code if you'd like to compare the position 
		CurrentPosition = EvalPositionAtTime(TrInst, OutTime);
		debugf(TEXT("Current Error is %f"), (CurrentPosition-Pos).Size());
#endif
	}

	return OutTime;
}



/**
 * Computes the world space coordinates for a key; handles keys that use IMF_RelativeToInitial, basing, etc.
 *
 * @param MoveTrackInst		An instance of this movement track
 * @param RelativeSpacePos	Key position value from curve
 * @param RelativeSpaceRot	Key rotation value from curve
 * @param OutPos			Output world space position
 * @param OutRot			Output world space rotation
 */
void UInterpTrackMove::ComputeWorldSpaceKeyTransform( UInterpTrackInstMove* MoveTrackInst,
													  const FVector& RelativeSpacePos,
													  const FRotator& RelativeSpaceRot,
													  FVector& OutPos,
													  FRotator& OutRot )
{
	// Break into rotation and winding part.
	FRotator RelativeSpaceWindRot, RelativeSpaceUnwoundRot;
	RelativeSpaceRot.GetWindingAndRemainder( RelativeSpaceWindRot, RelativeSpaceUnwoundRot );

	// Find the reference frame the key is considered in.
	FMatrix RelativeToWorldTransform = GetMoveRefFrame( MoveTrackInst );

	// Use rotation part to form transformation matrix.
	FRotationTranslationMatrix ActorToRelativeUnwoundTransform( RelativeSpaceUnwoundRot, RelativeSpacePos );

	// Compute the rotation amount in world space
	// @todo: Doesn't take into account overwind introduced by transforming relative euler angles to world space
	FMatrix ActorToWorldUnwoundTransform = ActorToRelativeUnwoundTransform * RelativeToWorldTransform;


	// Position
	{
		// Apply keyframed position to base to find desired position in world frame.
		OutPos = ActorToWorldUnwoundTransform.GetOrigin();
	}

	// Rotation
	{
		// Transform winding into correct ref frame.
		FVector RelativeSpaceWindEulerNum = RelativeSpaceWindRot.Euler() / 360.f;
		FVector WorldSpaceWindEulerNum = RelativeToWorldTransform.TransformNormal( RelativeSpaceWindEulerNum );
		FVector WorldSpaceWindEuler = WindNumToEuler( WorldSpaceWindEulerNum );

		// Combine the world space unwound euler with the transformed winding amount
		OutRot = GetCleanedUpRotator( ActorToWorldUnwoundTransform ).GetNormalized() + FRotator::MakeFromEuler( WorldSpaceWindEuler );
	}
}



// The inputs here are treated as the 'default' output ie. if there is no track data.
UBOOL UInterpTrackMove::GetLocationAtTime(UInterpTrackInst* TrInst, FLOAT Time, FVector& OutPos, FRotator& OutRot)
{
	UInterpTrackInstMove* MoveTrackInst = CastChecked<UInterpTrackInstMove>(TrInst);

	check( SubTracks.Num() > 0 || (EulerTrack.Points.Num() == PosTrack.Points.Num()) && (EulerTrack.Points.Num() == LookupTrack.Points.Num()));

	// Do nothing if no data on this track.
	if( SubTracks.Num() == 0 && EulerTrack.Points.Num() == 0)
	{
		// would be nice to return error code, so that
		// if no point exists, 
		return FALSE;
	}

	// Find the transform for the given time.
	FVector RelativeSpacePos;
	FRotator RelativeSpaceRot;
	GetKeyTransformAtTime( TrInst, Time, RelativeSpacePos, RelativeSpaceRot );

	// Compute world space key transform
	ComputeWorldSpaceKeyTransform( MoveTrackInst, RelativeSpacePos, RelativeSpaceRot, OutPos, OutRot );
	
	// if ignore mode, do not apply rotation
	if(RotMode == IMR_Ignore)
	{
		AActor* Actor = TrInst->GetGroupActor();
		OutRot = Actor->Rotation;
	}
	// Replace rotation if using a special rotation mode.
	else if(RotMode == IMR_LookAtGroup)
	{		
		if(LookAtGroupName != NAME_None)
		{
			AActor* Actor = TrInst->GetGroupActor();

			UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
			USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
			UInterpGroupInst* LookAtGroupInst = Seq->FindFirstGroupInstByName(LookAtGroupName);

			if(Actor && LookAtGroupInst && LookAtGroupInst->GetGroupActor())
			{
				AActor* LookAtActor = LookAtGroupInst->GetGroupActor();

				// Slight hack here so that if we are trying to look at a Player variable, it looks at their Pawn.
				APlayerController* PC = Cast<APlayerController>(LookAtActor);
				if(PC && PC->Pawn)
				{
					LookAtActor = PC->Pawn;
				}

				// Find Rotator that points at LookAtActor
				FVector LookDir = (LookAtActor->Location - Actor->Location).SafeNormal();
				OutRot = LookDir.Rotation();
			}
		}
	}

	return TRUE;
}

/** 
 *	Return the reference frame that the animation is currently working within.
 *	Looks at the current MoveFrame setting and whether the Actor is based on something.
 */
FMatrix UInterpTrackMove::GetMoveRefFrame(UInterpTrackInstMove* MoveTrackInst)
{
	AActor* Actor = MoveTrackInst->GetGroupActor();
	FMatrix BaseTM = FMatrix::Identity;

	AActor* BaseActor = NULL;
	if(Actor)
	{
		BaseActor = Actor->GetBase();
		BaseTM = GetBaseMatrix(Actor);
	}

	FMatrix RefTM = FMatrix::Identity;
	if( MoveFrame == IMF_World )
	{
		RefTM = BaseTM;
	}
	else if( MoveFrame == IMF_RelativeToInitial )
	{
		RefTM = MoveTrackInst->InitialTM * BaseTM;
		RefTM.RemoveScaling();
	}

	return RefTM;
}

/*-----------------------------------------------------------------------------
 UInterpTrackInstMove
-----------------------------------------------------------------------------*/

/** 
 *	Calculate and store the InitialTM and InitialQuat values, that are used as the basis for RelativeToInitial movements. 
 *	If the bZeroFromHere parameter is true, 
 */
void UInterpTrackInstMove::CalcInitialTransform(UInterpTrack* Track, UBOOL bZeroFromHere)
{
	UInterpTrackMove* MoveTrack = CastChecked<UInterpTrackMove>( Track );
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
	USeqAct_Interp* Seq = Cast<USeqAct_Interp>( GrInst->GetOuter() );			// ok if this is NULL

	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	FMatrix ActorTM;

	UInterpGroupInstAI* AIGrInst = Cast<UInterpGroupInstAI>( GrInst );
	if (AIGrInst)
	{
		UInterpGroupAI* AIGroup = CastChecked<UInterpGroupAI>(AIGrInst->Group);
		// if AI group, use StageMark Location
		FRotator InitRot;
		FVector InitLoc = AIGrInst->GetStageMarkPosition(&InitRot);
		// add collision cylinder height
		// if you'd like to keep old legacy height adjust
		if (!AIGroup->bIgnoreLegacyHeightAdjust)
		{
			APawn * Pawn = GetPawn(Actor);
			if ( Pawn )
			{
				// Add collision cylinder
				if (Pawn->CylinderComponent)
				{
					InitLoc.Z += Pawn->CylinderComponent->CollisionHeight;
				}

				// Add mesh translation for editor
				// since we can't add adjustinterptrackmove to the point
				// in editor, setting point, getting point works without the delta
				if (GIsGame && Pawn->Mesh)
				{
					InitLoc.Z += Pawn->Mesh->Translation.Z;
				}

			}
		}
		ActorTM = FRotationTranslationMatrix( InitRot, InitLoc );

		// If this Actor has a base, transform its current position into the frame of its Base.
		AActor* BaseActor = Actor->GetBase();
		if(BaseActor)
		{
			FMatrix BaseTM = GetBaseMatrix(Actor);
			FMatrix InvBaseTM = BaseTM.Inverse();
			ActorTM = ActorTM * InvBaseTM;
		}
	}
	else
	{
		// The following is only used in the case of IMF_RelativeToInitial
		ActorTM = FRotationTranslationMatrix( Actor->Rotation, Actor->Location );

		// If this Actor has a base, transform its current position into the frame of its Base.
		AActor* BaseActor = Actor->GetBase();
		if(BaseActor)
		{
			FMatrix BaseTM = GetBaseMatrix(Actor);
			FMatrix InvBaseTM = BaseTM.Inverse();
			ActorTM = ActorTM * InvBaseTM;
		}
	}

	// Determine the InitialTM for this track. Either use the raw location of the Actor, or offset it by the first key frame.
	if(MoveTrack->bUseRawActorTMforRelativeToInitial)
	{
		InitialTM = ActorTM;
	}
	else
	{
		FVector RelPos;
		FRotator RelRot;

		// If flag is set, use this as the 'base' transform regardless.
		if(bZeroFromHere)
		{
			MoveTrack->GetKeyTransformAtTime(this, 0.f, RelPos, RelRot);
		}
		else
		{
			// Find the current relative position according to the interpolation.
			FLOAT const Pos = Seq ? Seq->Position : 0.f;
			MoveTrack->GetKeyTransformAtTime(this, Pos, RelPos, RelRot);
		}

		FRotationTranslationMatrix RelTM(RelRot,RelPos);

		// Initial position is the current position of the Actor, minus the transform from the beginning of the track.
		// This is so that we can stop a 'relative to initial' interpolation and restart it again etc.
		InitialTM = RelTM.Inverse() * ActorTM;
	}

	InitialTM.RemoveScaling();

	InitialQuat = FQuat(InitialTM);
}

/** Initialize this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstMove::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	// Initialize the 'initial' position for this track.
	CalcInitialTransform(Track, FALSE);
}

void UInterpTrackMove::CreateSubTracks( UBOOL bCopy )
{
#if WITH_EDITORONLY_DATA
	// Make a group for containing all translation subtracks
	FSubTrackGroup TranslateGroup;
	TranslateGroup.GroupName = TEXT("Translation");
	TranslateGroup.bIsCollapsed = FALSE;
	TranslateGroup.bIsSelected = FALSE;

	// Make a group for containing all rotation subtracks
	FSubTrackGroup RotateGroup;
	RotateGroup.GroupName = TEXT("Rotation");
	RotateGroup.bIsCollapsed = FALSE;
	RotateGroup.bIsSelected = FALSE;

	// Add the new subtracks
	SubTrackGroups.AddItem( TranslateGroup );
	SubTrackGroups.AddItem( RotateGroup );

	// For each supported subtrack, add a new track based on the supported subtrack parameters.
	for( INT SubClassIndex = 0; SubClassIndex < SupportedSubTracks.Num(); ++SubClassIndex )
	{
		FSupportedSubTrackInfo& SubTrackInfo = SupportedSubTracks( SubClassIndex );
		check( SubTrackInfo.SupportedClass );

		UInterpTrack* TrackDef = SubTrackInfo.SupportedClass->GetDefaultObject<UInterpTrack>();
		check( TrackDef && TrackDef->bSubTrackOnly );

		UInterpTrack* NewSubTrack = NULL;
		NewSubTrack = ConstructObject<UInterpTrack>( SubTrackInfo.SupportedClass, this, NAME_None, RF_Transactional );
		check( NewSubTrack );

		INT NewTrackIndex = SubTracks.AddItem( NewSubTrack );

		if( !bCopy )
		{
			NewSubTrack->SetTrackToSensibleDefault();
		}

		UInterpTrackMoveAxis* MoveSubTrack = Cast<UInterpTrackMoveAxis>( NewSubTrack );
		MoveSubTrack->TrackTitle = SubTrackInfo.SubTrackName;
		MoveSubTrack->MoveAxis = SubClassIndex;

		NewSubTrack->Modify();

		// Add the index to this track into the correct subtrack group.
		if( SubTrackInfo.GroupIndex != INDEX_NONE )
		{
			SubTrackGroups( SubTrackInfo.GroupIndex ).TrackIndices.AddItem( SubClassIndex );
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/**
 * Splits this movment track in to seperate tracks for translation and rotation
 */
void UInterpTrackMove::SplitTranslationAndRotation()
{
#if WITH_EDITORONLY_DATA
	check( SubTrackGroups.Num() == 0 && SubTracks.Num() == 0 );
	
	// First create the new subtracks
	CreateSubTracks( FALSE );

	UInterpTrackMoveAxis* MoveAxies[6];
	for( INT SubTrackIndex = 0; SubTrackIndex < 6; ++SubTrackIndex )
	{
		MoveAxies[ SubTrackIndex ] = Cast<UInterpTrackMoveAxis>( SubTracks( SubTrackIndex ) );
	}

	// Populate the translation tracks with data.
	for( INT KeyIndex = 0; KeyIndex < PosTrack.Points.Num(); ++KeyIndex )
	{
		// For each keyframe in the orginal position track, add one keyframe to each translation track at the same location and with the same options.
		FLOAT Time = PosTrack.Points( KeyIndex ).InVal;
		const FVector& Pos = PosTrack.Points( KeyIndex ).OutVal;
		MoveAxies[AXIS_TranslationX]->FloatTrack.AddPoint( Time, Pos.X );
		MoveAxies[AXIS_TranslationY]->FloatTrack.AddPoint( Time, Pos.Y );
		MoveAxies[AXIS_TranslationZ]->FloatTrack.AddPoint( Time, Pos.Z );
		MoveAxies[AXIS_TranslationX]->FloatTrack.Points( KeyIndex ).InterpMode = PosTrack.Points( KeyIndex ).InterpMode;
		MoveAxies[AXIS_TranslationY]->FloatTrack.Points( KeyIndex ).InterpMode = PosTrack.Points( KeyIndex ).InterpMode;
		MoveAxies[AXIS_TranslationZ]->FloatTrack.Points( KeyIndex ).InterpMode = PosTrack.Points( KeyIndex ).InterpMode;
		MoveAxies[AXIS_TranslationX]->FloatTrack.InterpMethod = PosTrack.InterpMethod;
		MoveAxies[AXIS_TranslationY]->FloatTrack.InterpMethod = PosTrack.InterpMethod;
		MoveAxies[AXIS_TranslationZ]->FloatTrack.InterpMethod = PosTrack.InterpMethod;
		MoveAxies[AXIS_TranslationX]->FloatTrack.Points( KeyIndex ).ArriveTangent = PosTrack.Points( KeyIndex ).ArriveTangent[ AXIS_TranslationX ];
		MoveAxies[AXIS_TranslationY]->FloatTrack.Points( KeyIndex ).ArriveTangent = PosTrack.Points( KeyIndex ).ArriveTangent[ AXIS_TranslationY ];
		MoveAxies[AXIS_TranslationZ]->FloatTrack.Points( KeyIndex ).ArriveTangent = PosTrack.Points( KeyIndex ).ArriveTangent[ AXIS_TranslationZ ];
		MoveAxies[AXIS_TranslationX]->FloatTrack.Points( KeyIndex ).LeaveTangent = PosTrack.Points( KeyIndex ).LeaveTangent[ AXIS_TranslationX ];
		MoveAxies[AXIS_TranslationY]->FloatTrack.Points( KeyIndex ).LeaveTangent = PosTrack.Points( KeyIndex ).LeaveTangent[ AXIS_TranslationY ];
		MoveAxies[AXIS_TranslationZ]->FloatTrack.Points( KeyIndex ).LeaveTangent = PosTrack.Points( KeyIndex ).LeaveTangent[ AXIS_TranslationZ ];

		// Copy lookup track info.
		MoveAxies[AXIS_TranslationX]->LookupTrack.Points.Add();
		MoveAxies[AXIS_TranslationX]->LookupTrack.Points( KeyIndex ) = LookupTrack.Points( KeyIndex );
		MoveAxies[AXIS_TranslationY]->LookupTrack.Points.Add();
		MoveAxies[AXIS_TranslationY]->LookupTrack.Points( KeyIndex ) = LookupTrack.Points( KeyIndex );
		MoveAxies[AXIS_TranslationZ]->LookupTrack.Points.Add();
		MoveAxies[AXIS_TranslationZ]->LookupTrack.Points( KeyIndex ) = LookupTrack.Points( KeyIndex );

	}

	// Populate the rotation tracks with data.
	for( INT KeyIndex = 0; KeyIndex < EulerTrack.Points.Num(); ++KeyIndex )
	{
		// For each keyframe in the orginal rotation track, add one keyframe to each rotation track at the same location and with the same options.
		FLOAT Time = EulerTrack.Points( KeyIndex ).InVal;
		const FVector& Rot = EulerTrack.Points( KeyIndex ).OutVal;
		MoveAxies[AXIS_RotationX]->FloatTrack.AddPoint( Time, Rot.X );
		MoveAxies[AXIS_RotationY]->FloatTrack.AddPoint( Time, Rot.Y );
		MoveAxies[AXIS_RotationZ]->FloatTrack.AddPoint( Time, Rot.Z );
		MoveAxies[AXIS_RotationX]->FloatTrack.Points( KeyIndex ).InterpMode = EulerTrack.Points( KeyIndex ).InterpMode;
		MoveAxies[AXIS_RotationY]->FloatTrack.Points( KeyIndex ).InterpMode = EulerTrack.Points( KeyIndex ).InterpMode;
		MoveAxies[AXIS_RotationZ]->FloatTrack.Points( KeyIndex ).InterpMode = EulerTrack.Points( KeyIndex ).InterpMode;
		MoveAxies[AXIS_RotationX]->FloatTrack.InterpMethod = EulerTrack.InterpMethod;
		MoveAxies[AXIS_RotationY]->FloatTrack.InterpMethod = EulerTrack.InterpMethod;
		MoveAxies[AXIS_RotationZ]->FloatTrack.InterpMethod = EulerTrack.InterpMethod;
		MoveAxies[AXIS_RotationX]->FloatTrack.Points( KeyIndex ).ArriveTangent = EulerTrack.Points( KeyIndex ).ArriveTangent[AXIS_RotationX-3];
		MoveAxies[AXIS_RotationY]->FloatTrack.Points( KeyIndex ).ArriveTangent = EulerTrack.Points( KeyIndex ).ArriveTangent[AXIS_RotationY-3];
		MoveAxies[AXIS_RotationZ]->FloatTrack.Points( KeyIndex ).ArriveTangent = EulerTrack.Points( KeyIndex ).ArriveTangent[AXIS_RotationZ-3];
		MoveAxies[AXIS_RotationX]->FloatTrack.Points( KeyIndex ).LeaveTangent = EulerTrack.Points( KeyIndex ).LeaveTangent[AXIS_RotationX-3];
		MoveAxies[AXIS_RotationY]->FloatTrack.Points( KeyIndex ).LeaveTangent = EulerTrack.Points( KeyIndex ).LeaveTangent[AXIS_RotationY-3];
		MoveAxies[AXIS_RotationZ]->FloatTrack.Points( KeyIndex ).LeaveTangent = EulerTrack.Points( KeyIndex ).LeaveTangent[AXIS_RotationZ-3];

		MoveAxies[AXIS_RotationX]->LookupTrack.Points.Add();
		MoveAxies[AXIS_RotationX]->LookupTrack.Points( KeyIndex ) = LookupTrack.Points( KeyIndex );
		MoveAxies[AXIS_RotationY]->LookupTrack.Points.Add();
		MoveAxies[AXIS_RotationY]->LookupTrack.Points( KeyIndex ) = LookupTrack.Points( KeyIndex );
		MoveAxies[AXIS_RotationZ]->LookupTrack.Points.Add();
		MoveAxies[AXIS_RotationZ]->LookupTrack.Points( KeyIndex ) = LookupTrack.Points( KeyIndex );
	}

	// Clear out old data.
	LookupTrack.Points.Empty();
	PosTrack.Points.Empty();
	EulerTrack.Points.Empty();
#endif // WITH_EDITORONLY_DATA
}

/** 
 * Reduce Keys within Tolerance
 *
 * @param bIntervalStart	start of the key to reduce
 * @param bIntervalEnd		end of the key to reduce
 * @param Tolerance			tolerance
 */
void UInterpTrackMove::ReduceKeys( FLOAT IntervalStart, FLOAT IntervalEnd, FLOAT Tolerance )
{
	if( SubTracks.Num() == 0 )
	{
		// Create all the control points. They are six-dimensional, since
		// the Euler rotation key and the position key times must match.
		MatineeKeyReduction::MCurve<FTwoVectors, 6> Curve;
		Curve.RelativeTolerance = Tolerance / 100.0f;
		Curve.IntervalStart = IntervalStart - 0.0005f; // 0.5ms pad to allow for floating-point precision.
		Curve.IntervalEnd = IntervalEnd + 0.0005f;  // 0.5ms pad to allow for floating-point precision.

		Curve.CreateControlPoints(PosTrack, 0);
		Curve.CreateControlPoints(EulerTrack, 3);
		if (Curve.HasControlPoints())
		{
			Curve.FillControlPoints(PosTrack, 3, 0);
			Curve.FillControlPoints(EulerTrack, 3, 3);

			// Reduce the 6D curve.
			Curve.Reduce();

			// Copy the reduced keys over to the new curve.
			Curve.CopyCurvePoints(PosTrack.Points, 3, 0);
			Curve.CopyCurvePoints(EulerTrack.Points, 3, 3);
		}

		// Refer the look-up track to nothing.
		LookupTrack.Points.Empty();
		FName Nothing(NAME_None);
		UINT PointCount = PosTrack.Points.Num();
		for (UINT Index = 0; Index < PointCount; ++Index)
		{
			LookupTrack.AddPoint(PosTrack.Points(Index).InVal, Nothing);
		}
	}
	else
	{
		// Reduce keys for all subtracks.
		for( INT SubTrackIndex = 0; SubTrackIndex < SubTracks.Num(); ++SubTrackIndex )
		{
			SubTracks( SubTrackIndex )->Modify();
			SubTracks( SubTrackIndex )->ReduceKeys(IntervalStart, IntervalEnd, Tolerance);
		}
	}
}
/*-----------------------------------------------------------------------------
  UInterpTrackFloatBase
-----------------------------------------------------------------------------*/

/** Total number of keyframes in this track. */
INT UInterpTrackFloatBase::GetNumKeyframes() const
{
	return FloatTrack.Points.Num();
}

/** Get first and last time of keyframes in this track. */
void UInterpTrackFloatBase::GetTimeRange(FLOAT& StartTime, FLOAT& EndTime) const
{
	if(FloatTrack.Points.Num() == 0)
	{
		StartTime = 0.f;
		EndTime = 0.f;
	}
	else
	{
		StartTime = FloatTrack.Points(0).InVal;
		EndTime = FloatTrack.Points( FloatTrack.Points.Num()-1 ).InVal;
	}
}

/** Get the time of the keyframe with the given index. */
FLOAT UInterpTrackFloatBase::GetKeyframeTime(INT KeyIndex) const
{
	if( KeyIndex < 0 || KeyIndex >= FloatTrack.Points.Num() )
		return 0.f;

	return FloatTrack.Points(KeyIndex).InVal;
}

/** Change the time position of an existing keyframe. This can change the index of the keyframe - the new index is returned. */
INT UInterpTrackFloatBase::SetKeyframeTime(INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder)
{
	if( KeyIndex < 0 || KeyIndex >= FloatTrack.Points.Num() )
		return KeyIndex;

	INT NewKeyIndex = KeyIndex;
	if(bUpdateOrder)
	{
		NewKeyIndex = FloatTrack.MovePoint(KeyIndex, NewKeyTime);
	}
	else
	{
		FloatTrack.Points(KeyIndex).InVal = NewKeyTime;
	}

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** Remove the keyframe with the given index. */
void UInterpTrackFloatBase::RemoveKeyframe(INT KeyIndex)
{
	if( KeyIndex < 0 || KeyIndex >= FloatTrack.Points.Num() )
		return;

	FloatTrack.Points.Remove(KeyIndex);

	FloatTrack.AutoSetTangents(CurveTension);
}


/** 
 *	Duplicate the keyframe with the given index to the specified time. 
 *	Returns the index of the newly created key.
 */
INT UInterpTrackFloatBase::DuplicateKeyframe(INT KeyIndex, FLOAT NewKeyTime)
{
	if( KeyIndex < 0 || KeyIndex >= FloatTrack.Points.Num() )
	{
		return INDEX_NONE;
	}

	FInterpCurvePoint<FLOAT> FloatPoint = FloatTrack.Points(KeyIndex);
	INT NewKeyIndex = FloatTrack.AddPoint(NewKeyTime, 0.f);
	FloatTrack.Points(NewKeyIndex) = FloatPoint; // Copy properties from source key.
	FloatTrack.Points(NewKeyIndex).InVal = NewKeyTime;

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** Return the closest time to the time passed in that we might want to snap to. */
UBOOL UInterpTrackFloatBase::GetClosestSnapPosition(FLOAT InPosition, TArray<INT> &IgnoreKeys, FLOAT& OutPosition)
{
	if(FloatTrack.Points.Num() == 0)
	{
		return false;
	}

	UBOOL bFoundSnap = false;
	FLOAT ClosestSnap = 0.f;
	FLOAT ClosestDist = BIG_NUMBER;
	for(INT i=0; i<FloatTrack.Points.Num(); i++)
	{
		if(!IgnoreKeys.ContainsItem(i))
		{
			FLOAT Dist = Abs( FloatTrack.Points(i).InVal - InPosition );
			if(Dist < ClosestDist)
			{
				ClosestSnap = FloatTrack.Points(i).InVal;
				ClosestDist = Dist;
				bFoundSnap = true;
			}
		}
	}

	OutPosition = ClosestSnap;
	return bFoundSnap;
}

void UInterpTrackFloatBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FloatTrack.AutoSetTangents(CurveTension);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/*-----------------------------------------------------------------------------
	UInterpTrackToggle
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackToggle, ToggleTrack)
STRUCTTRACK_GETTIMERANGE(UInterpTrackToggle, ToggleTrack, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackToggle, ToggleTrack, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackToggle, ToggleTrack, Time, FToggleTrackKey)
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackToggle, ToggleTrack)
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackToggle, ToggleTrack, Time, FToggleTrackKey)
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackToggle, ToggleTrack, Time)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackToggle, ToggleTrack, Time)

// InterpTrack interface
INT UInterpTrackToggle::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	UInterpTrackInstToggle* ToggleInst = CastChecked<UInterpTrackInstToggle>(TrInst);

	INT i = 0;
	for (i = 0; i < ToggleTrack.Num() && ToggleTrack(i).Time < Time; i++);
	ToggleTrack.Insert(i);
	ToggleTrack(i).Time = Time;
	ToggleTrack(i).ToggleAction = ToggleInst->Action;

	return i;
}

void UInterpTrackToggle::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if (Actor == NULL)
	{
		return;
	}

	UInterpTrackInstToggle* ToggleInst = CastChecked<UInterpTrackInstToggle>(TrInst);
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( ToggleInst->GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
	UInterpGroup* Group = CastChecked<UInterpGroup>( GetOuter() );
	UInterpData* IData = CastChecked<UInterpData>( Group->GetOuter() );



	AEmitter* EmitterActor = Cast<AEmitter>(Actor);
	if( EmitterActor && bActivateSystemEachUpdate )
	{
		// @todo: Deprecate this legacy particle track behavior!  It doesn't support playing skipped events,
		//        and it doesn't support network synchronization!
		if ((NewPosition > ToggleInst->LastUpdatePosition)  && !bJump)
		{
			for (INT KeyIndex = ToggleTrack.Num() - 1; KeyIndex >= 0; KeyIndex--)
			{
				FToggleTrackKey& ToggleKey = ToggleTrack(KeyIndex);
				// We have found the key to the left of the position
				if (ToggleKey.ToggleAction == ETTA_On)
				{
					EmitterActor->ParticleSystemComponent->ActivateSystem(bActivateWithJustAttachedFlag);
				}
				else
				if (ToggleKey.ToggleAction == ETTA_Trigger)
				{
					if (ToggleKey.Time >= ToggleInst->LastUpdatePosition)
					{
						EmitterActor->ParticleSystemComponent->SetActive(TRUE, bActivateWithJustAttachedFlag);
					}
				}
				else
				{
					EmitterActor->ParticleSystemComponent->DeactivateSystem();
				}
				break;
			}
		}
	}
	else
	{
		// This is the normal pathway for toggle tracks.  It supports firing toggle events
		// even when jummping forward in time (skipping a cutscene.)

		// NOTE: We don't fire events when jumping forwards in Matinee preview since that would
		//       fire off particles while scrubbing, which we currently don't want.
		const UBOOL bShouldActuallyFireEventsWhenJumpingForwards =
			bFireEventsWhenJumpingForwards && !( GIsEditor && !GWorld->HasBegunPlay() );

		// @todo: Make this configurable?
		const UBOOL bInvertBoolLogicWhenPlayingBackwards = TRUE;

		// Only allow triggers to play when jumping when scrubbing in editor's Matinee preview.  We
		// never want to allow this in game, since this could cause many particles to fire off
		// when a cinematic is skipped (as we "jump" to the end point)
		const UBOOL bPlayTriggersWhenJumping = GIsEditor && !GWorld->HasBegunPlay();


		// We'll consider playing events in reverse if we're either actively playing in reverse or if
		// we're in a paused state but forcing an update to an older position (scrubbing backwards in editor.)
		UBOOL bIsPlayingBackwards =
			( Seq->bIsPlaying && Seq->bReversePlayback ) ||
			( bJump && !Seq->bIsPlaying && NewPosition < ToggleInst->LastUpdatePosition );


		// Find the interval between last update and this to check events with.
		UBOOL bFireEvents = TRUE;


		if( bJump )
		{
			// If we are playing forwards, and the flag is set, fire events even if we are 'jumping'.
			if( bShouldActuallyFireEventsWhenJumpingForwards && !bIsPlayingBackwards )
			{
				bFireEvents = TRUE;
			}
			else
			{
				bFireEvents = FALSE;
			}
		}

		// If playing sequence forwards.
		FLOAT MinTime, MaxTime;
		if( !bIsPlayingBackwards )
		{
			MinTime = ToggleInst->LastUpdatePosition;
			MaxTime = NewPosition;

			// Slight hack here.. if playing forwards and reaching the end of the sequence, force it over a little to ensure we fire events actually on the end of the sequence.
			if( MaxTime == IData->InterpLength )
			{
				MaxTime += (FLOAT)KINDA_SMALL_NUMBER;
			}

			if( !bFireEventsWhenForwards )
			{
				bFireEvents = FALSE;
			}
		}
		// If playing sequence backwards.
		else
		{
			MinTime = NewPosition;
			MaxTime = ToggleInst->LastUpdatePosition;

			// Same small hack as above for backwards case.
			if( MinTime == 0.0f )
			{
				MinTime -= (FLOAT)KINDA_SMALL_NUMBER;
			}

			if( !bFireEventsWhenBackwards )
			{
				bFireEvents = FALSE;
			}
		}


		// If we should be firing events for this track...
		if( bFireEvents )
		{
			// See which events fall into traversed region.
			INT KeyIndexToPlay = INDEX_NONE;
			for(INT CurKeyIndex = 0; CurKeyIndex < ToggleTrack.Num(); ++CurKeyIndex )
			{
				FToggleTrackKey& ToggleKey = ToggleTrack( CurKeyIndex );

				FLOAT EventTime = ToggleKey.Time;

				// Need to be slightly careful here and make behavior for firing events symmetric when playing forwards of backwards.
				UBOOL bFireThisEvent = FALSE;
				if( !bIsPlayingBackwards )
				{
					if( EventTime >= MinTime && EventTime < MaxTime )
					{
						bFireThisEvent = TRUE;
					}
				}
				else
				{
					if( EventTime > MinTime && EventTime <= MaxTime )
					{
						bFireThisEvent = TRUE;
					}
				}

				if( bFireThisEvent )
				{
					// Check for "fire and forget" events that must always be played
					if (ToggleKey.ToggleAction == ETTA_Trigger && EmitterActor )
					{
						// Don't play triggers when jumping forward unless we're configured to do that
						if( bPlayTriggersWhenJumping || !bJump )
						{
							// Use ActivateSystem as multiple triggers should fire it multiple times.
							EmitterActor->ParticleSystemComponent->ActivateSystem(bActivateWithJustAttachedFlag);
							// don't set bCurrentlyActive (assume it's a one shot effect which the client will perform through its own matinee simulation)
						}
					}
					else
					{
						// The idea here is that there's no point in playing multiple bool-style events in a
						// single frame, so we skip over events to find the most relevant.
						if( KeyIndexToPlay == INDEX_NONE ||
							( !bIsPlayingBackwards && CurKeyIndex > KeyIndexToPlay ) ||
							( bIsPlayingBackwards && CurKeyIndex < KeyIndexToPlay ) )
						{
							// Found the key we want to play!  
							KeyIndexToPlay = CurKeyIndex;
						}
					}
				}
			}

			if( KeyIndexToPlay != INDEX_NONE )
			{
				FToggleTrackKey& ToggleKey = ToggleTrack( KeyIndexToPlay );


				ALensFlareSource* LensFlareActor = Cast<ALensFlareSource>(Actor);
				ALight* LightActor = Cast<ALight>(Actor);
				AImageReflection* ReflectionActor = Cast<AImageReflection>(Actor);
				AImageReflectionShadowPlane* ReflectionShadowActor = Cast<AImageReflectionShadowPlane>(Actor);

				if( EmitterActor )
				{
					// Trigger keys should have been handled earlier!
					check( ToggleKey.ToggleAction != ETTA_Trigger );

					UBOOL bShouldActivate = (ToggleKey.ToggleAction == ETTA_On);
					if( bInvertBoolLogicWhenPlayingBackwards && bIsPlayingBackwards )
					{
						// Playing in reverse, so invert bool logic
						bShouldActivate = !bShouldActivate;
					}

					EmitterActor->ParticleSystemComponent->SetActive( bShouldActivate, bActivateWithJustAttachedFlag );
					EmitterActor->bCurrentlyActive = bShouldActivate;
					if (!Seq->bClientSideOnly)
					{
						EmitterActor->bNetDirty = TRUE;
						EmitterActor->eventForceNetRelevant();
					}
				}
				else if( LensFlareActor && LensFlareActor->LensFlareComp )
				{
					UBOOL bShouldActivate = (ToggleKey.ToggleAction == ETTA_On);
					if( bInvertBoolLogicWhenPlayingBackwards && bIsPlayingBackwards )
					{
						// Playing in reverse, so invert bool logic
						bShouldActivate = !bShouldActivate;
					}

					LensFlareActor->LensFlareComp->SetIsActive( bShouldActivate );
				}
				else if( LightActor != NULL )
				{
					// We'll only allow *toggleable* lights to be toggled like this!  Static lights are ignored.
					if( LightActor->IsToggleable() )
					{
						UBOOL bShouldActivate = (ToggleKey.ToggleAction == ETTA_On);
						if( bInvertBoolLogicWhenPlayingBackwards && bIsPlayingBackwards )
						{
							// Playing in reverse, so invert bool logic
							bShouldActivate = !bShouldActivate;
						}

						LightActor->LightComponent->SetEnabled( bShouldActivate );
					}
				}
				else if( ReflectionActor != NULL )
				{
					UBOOL bShouldActivate = (ToggleKey.ToggleAction == ETTA_On);
					if( bInvertBoolLogicWhenPlayingBackwards && bIsPlayingBackwards )
					{
						// Playing in reverse, so invert bool logic
						bShouldActivate = !bShouldActivate;
					}

					ReflectionActor->ImageReflectionComponent->SetEnabled( bShouldActivate );
				}
				else if( ReflectionShadowActor != NULL )
				{
					UBOOL bShouldActivate = (ToggleKey.ToggleAction == ETTA_On);
					if( bInvertBoolLogicWhenPlayingBackwards && bIsPlayingBackwards )
					{
						// Playing in reverse, so invert bool logic
						bShouldActivate = !bShouldActivate;
					}

					ReflectionShadowActor->ReflectionShadowComponent->SetEnabled( bShouldActivate );
				}
				else
				{
					// Find the function to call on the actor
					FName FunctionName = TEXT("OnInterpToggle");
					UFunction* ToggleFunction = Actor->FindFunction( FunctionName );
					// Make sure we call the right function. It should have one param.
					if( ToggleFunction && ToggleFunction->NumParms == 1 )
					{		
						INT ShouldActivate = (ToggleKey.ToggleAction == ETTA_On || ToggleKey.ToggleAction == ETTA_Trigger);
						if( bInvertBoolLogicWhenPlayingBackwards && bIsPlayingBackwards )
						{
							// Playing in reverse, so invert bool logic
							ShouldActivate = !ShouldActivate;
						}

						// Call the function
						Actor->ProcessEvent( ToggleFunction, &ShouldActivate );	
					}
				}
			}
		}
	}

	ToggleInst->LastUpdatePosition = NewPosition;
}

/** 
 *	Function which actually updates things based on the new position in the track. 
 *  This is called in the editor, when scrubbing/previewing etc.
 */
void UInterpTrackToggle::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );

	// Dont play sounds unless we are preview playback (ie not scrubbing).
	UBOOL bJump = !(Seq->bIsPlaying);
	UpdateTrack(NewPosition, TrInst, bJump);
}

/** 
 *	Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
 *
 *	@return		String name of the helper class.
 */
const FString UInterpTrackToggle::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackToggleHelper") );
}

/*-----------------------------------------------------------------------------
	UInterpTrackVectorBase
-----------------------------------------------------------------------------*/

/** Total number of keyframes in this track. */
INT UInterpTrackVectorBase::GetNumKeyframes() const
{
	return VectorTrack.Points.Num();
}

/** Get first and last time of keyframes in this track. */
void UInterpTrackVectorBase::GetTimeRange(FLOAT& StartTime, FLOAT& EndTime) const
{
	if(VectorTrack.Points.Num() == 0)
	{
		StartTime = 0.f;
		EndTime = 0.f;
	}
	else
	{
		StartTime = VectorTrack.Points(0).InVal;
		EndTime = VectorTrack.Points( VectorTrack.Points.Num()-1 ).InVal;
	}
}

/** Get the time of the keyframe with the given index. */
FLOAT UInterpTrackVectorBase::GetKeyframeTime(INT KeyIndex) const
{
	if( KeyIndex < 0 || KeyIndex >= VectorTrack.Points.Num() )
		return 0.f;

	return VectorTrack.Points(KeyIndex).InVal;
}

/** Change the time position of an existing keyframe. This can change the index of the keyframe - the new index is returned. */
INT UInterpTrackVectorBase::SetKeyframeTime(INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder)
{
	if( KeyIndex < 0 || KeyIndex >= VectorTrack.Points.Num() )
		return KeyIndex;

	INT NewKeyIndex = KeyIndex;
	if(bUpdateOrder)
	{
		NewKeyIndex = VectorTrack.MovePoint(KeyIndex, NewKeyTime);
	}
	else
	{
		VectorTrack.Points(KeyIndex).InVal = NewKeyTime;
	}

	VectorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** Remove the keyframe with the given index. */
void UInterpTrackVectorBase::RemoveKeyframe(INT KeyIndex)
{
	if( KeyIndex < 0 || KeyIndex >= VectorTrack.Points.Num() )
		return;

	VectorTrack.Points.Remove(KeyIndex);

	VectorTrack.AutoSetTangents(CurveTension);
}


/** 
 *	Duplicate the keyframe with the given index to the specified time. 
 *	Returns the index of the newly created key.
 */
INT UInterpTrackVectorBase::DuplicateKeyframe(INT KeyIndex, FLOAT NewKeyTime)
{
	if( KeyIndex < 0 || KeyIndex >= VectorTrack.Points.Num() )
		return INDEX_NONE;

	FInterpCurvePoint<FVector> VectorPoint = VectorTrack.Points(KeyIndex);
	INT NewKeyIndex = VectorTrack.AddPoint(NewKeyTime, FVector(0.f));
	VectorTrack.Points(NewKeyIndex) = VectorPoint; // Copy properties from source key.
	VectorTrack.Points(NewKeyIndex).InVal = NewKeyTime;

	VectorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** Return the closest time to the time passed in that we might want to snap to. */
UBOOL UInterpTrackVectorBase::GetClosestSnapPosition(FLOAT InPosition, TArray<INT> &IgnoreKeys, FLOAT& OutPosition)
{
	if(VectorTrack.Points.Num() == 0)
	{
		return false;
	}

	UBOOL bFoundSnap = false;
	FLOAT ClosestSnap = 0.f;
	FLOAT ClosestDist = BIG_NUMBER;
	for(INT i=0; i<VectorTrack.Points.Num(); i++)
	{
		if(!IgnoreKeys.ContainsItem(i))
		{
			FLOAT Dist = Abs( VectorTrack.Points(i).InVal - InPosition );
			if(Dist < ClosestDist)
			{
				ClosestSnap = VectorTrack.Points(i).InVal;
				ClosestDist = Dist;
				bFoundSnap = true;
			}
		}
	}

	OutPosition = ClosestSnap;
	return bFoundSnap;
}

void UInterpTrackVectorBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	VectorTrack.AutoSetTangents(CurveTension);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}



/*-----------------------------------------------------------------------------
UInterpTrackLinearColorBase
-----------------------------------------------------------------------------*/

/** Total number of keyframes in this track. */
INT UInterpTrackLinearColorBase::GetNumKeyframes() const
{
	return LinearColorTrack.Points.Num();
}

/** Get first and last time of keyframes in this track. */
void UInterpTrackLinearColorBase::GetTimeRange(FLOAT& StartTime, FLOAT& EndTime) const
{
	if(LinearColorTrack.Points.Num() == 0)
	{
		StartTime = 0.f;
		EndTime = 0.f;
	}
	else
	{
		StartTime = LinearColorTrack.Points(0).InVal;
		EndTime = LinearColorTrack.Points( LinearColorTrack.Points.Num()-1 ).InVal;
	}
}

/**
 * @return	The ending time of the track. 
 */
FLOAT UInterpTrackLinearColorBase::GetTrackEndTime() const
{
	FLOAT EndTime = 0.0f;

	if( LinearColorTrack.Points.Num() )
	{
		EndTime = LinearColorTrack.Points(LinearColorTrack.Points.Num()-1).InVal;
	}

	return EndTime;
}

/** Get the time of the keyframe with the given index. */
FLOAT UInterpTrackLinearColorBase::GetKeyframeTime(INT KeyIndex) const
{
	if( KeyIndex < 0 || KeyIndex >= LinearColorTrack.Points.Num() )
		return 0.f;

	return LinearColorTrack.Points(KeyIndex).InVal;
}

/** Change the time position of an existing keyframe. This can change the index of the keyframe - the new index is returned. */
INT UInterpTrackLinearColorBase::SetKeyframeTime(INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder)
{
	if( KeyIndex < 0 || KeyIndex >= LinearColorTrack.Points.Num() )
		return KeyIndex;

	INT NewKeyIndex = KeyIndex;
	if(bUpdateOrder)
	{
		NewKeyIndex = LinearColorTrack.MovePoint(KeyIndex, NewKeyTime);
	}
	else
	{
		LinearColorTrack.Points(KeyIndex).InVal = NewKeyTime;
	}

	LinearColorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** Remove the keyframe with the given index. */
void UInterpTrackLinearColorBase::RemoveKeyframe(INT KeyIndex)
{
	if( KeyIndex < 0 || KeyIndex >= LinearColorTrack.Points.Num() )
		return;

	LinearColorTrack.Points.Remove(KeyIndex);

	LinearColorTrack.AutoSetTangents(CurveTension);
}


/** 
*	Duplicate the keyframe with the given index to the specified time. 
*	Returns the index of the newly created key.
*/
INT UInterpTrackLinearColorBase::DuplicateKeyframe(INT KeyIndex, FLOAT NewKeyTime)
{
	if( KeyIndex < 0 || KeyIndex >= LinearColorTrack.Points.Num() )
		return INDEX_NONE;

	FInterpCurvePoint<FLinearColor> VectorPoint = LinearColorTrack.Points(KeyIndex);
	INT NewKeyIndex = LinearColorTrack.AddPoint(NewKeyTime, FLinearColor(0.f, 0.f, 0.f, 0.f));
	LinearColorTrack.Points(NewKeyIndex) = VectorPoint; // Copy properties from source key.
	LinearColorTrack.Points(NewKeyIndex).InVal = NewKeyTime;

	LinearColorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** Return the closest time to the time passed in that we might want to snap to. */
UBOOL UInterpTrackLinearColorBase::GetClosestSnapPosition(FLOAT InPosition, TArray<INT> &IgnoreKeys, FLOAT& OutPosition)
{
	if(LinearColorTrack.Points.Num() == 0)
	{
		return false;
	}

	UBOOL bFoundSnap = false;
	FLOAT ClosestSnap = 0.f;
	FLOAT ClosestDist = BIG_NUMBER;
	for(INT i=0; i<LinearColorTrack.Points.Num(); i++)
	{
		if(!IgnoreKeys.ContainsItem(i))
		{
			FLOAT Dist = Abs( LinearColorTrack.Points(i).InVal - InPosition );
			if(Dist < ClosestDist)
			{
				ClosestSnap = LinearColorTrack.Points(i).InVal;
				ClosestDist = Dist;
				bFoundSnap = true;
			}
		}
	}

	OutPosition = ClosestSnap;
	return bFoundSnap;
}

void UInterpTrackLinearColorBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	LinearColorTrack.AutoSetTangents(CurveTension);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}


/*-----------------------------------------------------------------------------
	UInterpTrackFloatProp
-----------------------------------------------------------------------------*/

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackFloatProp::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	UInterpTrackInstFloatProp* PropInst = CastChecked<UInterpTrackInstFloatProp>(TrInst);
	if( !PropInst->FloatProp )
		return INDEX_NONE;

	INT NewKeyIndex = FloatTrack.AddPoint( Time, 0.f );
	FloatTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

	UpdateKeyframe(NewKeyIndex, TrInst);

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** Change the value of an existing keyframe. */
void UInterpTrackFloatProp::UpdateKeyframe(INT KeyIndex, UInterpTrackInst* TrInst)
{
	UInterpTrackInstFloatProp* PropInst = CastChecked<UInterpTrackInstFloatProp>(TrInst);
	if( !PropInst->FloatProp )
		return;

	if( KeyIndex < 0 || KeyIndex >= FloatTrack.Points.Num() )
		return;

	FloatTrack.Points(KeyIndex).OutVal = *((FLOAT*)(PropInst->FloatProp));

	FloatTrack.AutoSetTangents(CurveTension);
}


/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the editor, when scrubbing/previewing etc.
*/
void UInterpTrackFloatProp::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, FALSE);
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the game, when USeqAct_Interp is ticked
*  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
*/
void UInterpTrackFloatProp::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	UInterpTrackInstFloatProp* PropInst = CastChecked<UInterpTrackInstFloatProp>(TrInst);
	if(!PropInst->FloatProp)
		return;

	FLOAT NewFloatValue = FloatTrack.Eval( NewPosition, *((FLOAT*)(PropInst->FloatProp)) );
	*((FLOAT*)(PropInst->FloatProp)) = NewFloatValue;

	// If we have a custom callback for this property, call that, otherwise just force update components on the actor.
	if(PropInst->CallPropertyUpdateCallback()==FALSE)
	{
		// We update components, so things like draw scale take effect.
		Actor->ForceUpdateComponents(FALSE,FALSE);
	}
}

/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
* @return	String name of the helper class.*/
const FString	UInterpTrackFloatProp::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackFloatPropHelper") );
}


/** 
 * Reduce Keys within Tolerance
 *
 * @param bIntervalStart	start of the key to reduce
 * @param bIntervalEnd		end of the key to reduce
 * @param Tolerance			tolerance
 */
void UInterpTrackFloatProp::ReduceKeys( FLOAT IntervalStart, FLOAT IntervalEnd, FLOAT Tolerance )
{
	FInterpCurve<MatineeKeyReduction::SFLOAT>& OldCurve = (FInterpCurve<MatineeKeyReduction::SFLOAT>&) FloatTrack;

	// Create all the control points. They are six-dimensional, since
	// the Euler rotation key and the position key times must match.
	MatineeKeyReduction::MCurve<MatineeKeyReduction::SFLOAT, 1> Curve;
	Curve.RelativeTolerance = Tolerance / 100.0f;
	Curve.IntervalStart = IntervalStart - 0.0005f;  // 0.5ms pad to allow for floating-point precision.
	Curve.IntervalEnd = IntervalEnd + 0.0005f;  // 0.5ms pad to allow for floating-point precision.

	Curve.CreateControlPoints(OldCurve, 0);
	if (Curve.HasControlPoints())
	{
		Curve.FillControlPoints(OldCurve, 1, 0);

		// Reduce the curve.
		Curve.Reduce();

		// Copy the reduced keys over to the new curve.
		Curve.CopyCurvePoints(OldCurve.Points, 1, 0);
	}
}
/*-----------------------------------------------------------------------------
  UInterpTrackInstFloatProp
-----------------------------------------------------------------------------*/

/** Called before Interp editing to put object back to its original state. */
void UInterpTrackInstFloatProp::SaveActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!FloatProp)
		return;

	// Remember current value of property for when we quite Matinee
	ResetFloat = *((FLOAT*)(FloatProp));
}

/** Restore the saved state of this Actor. */
void UInterpTrackInstFloatProp::RestoreActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!FloatProp)
		return;

	// Restore original value of property
	*((FLOAT*)(FloatProp)) = ResetFloat;

	// We update components, so things like draw scale take effect.
	// Don't force update all components unless we're in the editor.
	Actor->ConditionalForceUpdateComponents(FALSE,FALSE);
}

/** Initialise this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstFloatProp::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	// Store a pointer to the float data for the property we will be interpolating.
	FName PropertyName;
	Track->GetPropertyName( PropertyName );
	FPointer RawFloatDistribution = NULL;
	FloatProp = Actor->GetInterpFloatPropertyRef(PropertyName, RawFloatDistribution);
	DistributionProp = RawFloatDistribution;

	if(DistributionProp)
	{
		((FMatineeRawDistributionFloat*)DistributionProp)->bInMatinee = TRUE;
	}

	SetupPropertyUpdateCallback(Actor, PropertyName);
	InterpTools::EnableCameraPostProcessFlag(Actor, PropertyName);
}

/** Terminate this Track instance. Called in-game after interpolation. */
void UInterpTrackInstFloatProp::TermTrackInst(UInterpTrack* Track)
{
	if(DistributionProp)
	{
		((FMatineeRawDistributionFloat*)DistributionProp)->bInMatinee = FALSE;
	}

	Super::TermTrackInst( Track );
}

/*-----------------------------------------------------------------------------
	UInterpTrackVectorProp
-----------------------------------------------------------------------------*/

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackVectorProp::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	UInterpTrackInstVectorProp* PropInst = CastChecked<UInterpTrackInstVectorProp>(TrInst);
	if( !PropInst->VectorProp )
		return INDEX_NONE;

	INT NewKeyIndex = VectorTrack.AddPoint( Time, FVector(0.f));
	VectorTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

	UpdateKeyframe(NewKeyIndex, TrInst);

	VectorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** Change the value of an existing keyframe. */
void UInterpTrackVectorProp::UpdateKeyframe(INT KeyIndex, UInterpTrackInst* TrInst)
{
	UInterpTrackInstVectorProp* PropInst = CastChecked<UInterpTrackInstVectorProp>(TrInst);
	if( !PropInst->VectorProp )
		return;

	if( KeyIndex < 0 || KeyIndex >= VectorTrack.Points.Num() )
		return;

	VectorTrack.Points(KeyIndex).OutVal = *((FVector*)(PropInst->VectorProp));

	VectorTrack.AutoSetTangents(CurveTension);
}


/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the editor, when scrubbing/previewing etc.
*/
void UInterpTrackVectorProp::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, FALSE);
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the game, when USeqAct_Interp is ticked
*  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
*/
void UInterpTrackVectorProp::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	UInterpTrackInstVectorProp* PropInst = CastChecked<UInterpTrackInstVectorProp>(TrInst);
	if(!PropInst->VectorProp)
		return;

	FVector NewVectorValue = VectorTrack.Eval( NewPosition, *((FVector*)(PropInst->VectorProp)) );
	*((FVector*)(PropInst->VectorProp)) = NewVectorValue;

	// If we have a custom callback for this property, call that, otherwise just force update components on the actor.
	if(PropInst->CallPropertyUpdateCallback()==FALSE)
	{
		// We update components, so things like draw scale take effect.
		Actor->ForceUpdateComponents(FALSE,FALSE);
	}
}

/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
* @return	String name of the helper class.*/
const FString	UInterpTrackVectorProp::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackVectorPropHelper") );
}


/*-----------------------------------------------------------------------------
	UInterpTrackInstVectorProp
-----------------------------------------------------------------------------*/

/** Called before Interp editing to put object back to its original state. */
void UInterpTrackInstVectorProp::SaveActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!VectorProp)
		return;

	// Remember current value of property for when we quite Matinee
	ResetVector = *((FVector*)(VectorProp));
}

/** Restore the saved state of this Actor. */
void UInterpTrackInstVectorProp::RestoreActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!VectorProp)
		return;

	// Restore original value of property
	*((FVector*)(VectorProp)) = ResetVector;

	// We update components, so things like draw scale take effect.
	// Don't force update all components unless we're in the editor.
	Actor->ConditionalForceUpdateComponents(FALSE,FALSE);
}

/** Initialise this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstVectorProp::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	FName PropertyName;
	Track->GetPropertyName( PropertyName );
	VectorProp = Actor->GetInterpVectorPropertyRef(PropertyName);

	SetupPropertyUpdateCallback(Actor, PropertyName);
	InterpTools::EnableCameraPostProcessFlag(Actor, PropertyName);
}

/*-----------------------------------------------------------------------------
	UInterpTrackBoolProp
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackBoolProp, BoolTrack)
STRUCTTRACK_GETTIMERANGE(UInterpTrackBoolProp, BoolTrack, Time)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackBoolProp, BoolTrack, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackBoolProp, BoolTrack, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackBoolProp, BoolTrack, Time, FBoolTrackKey )
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackBoolProp, BoolTrack )
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackBoolProp, BoolTrack, Time, FBoolTrackKey )
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackBoolProp, BoolTrack, Time)

/**
 * Adds a keyframe at the given time to the track.
 * 
 * @param	Time			The time to place the key in the track timeline.
 * @param	TrackInst		The instance of this track. 
 * @param	InitInterpMode	The interp mode of the newly-added keyframe.
 */
INT UInterpTrackBoolProp::AddKeyframe( FLOAT Time, UInterpTrackInst* TrackInst, EInterpCurveMode InitInterpMode )
{
	UInterpTrackInstBoolProp* BoolPropInst = CastChecked<UInterpTrackInstBoolProp>(TrackInst);

	if( !BoolPropInst->BoolProp )
		return INDEX_NONE;

	FBoolTrackKey BoolKey;
	BoolKey.Time = Time;
	BoolKey.Value = *((BITFIELD*)(BoolPropInst->BoolProp));

	INT NewKeyIndex = BoolTrack.AddItem( BoolKey );
	UpdateKeyframe( NewKeyIndex, TrackInst );

	return NewKeyIndex;
}

/**
 * Changes the value of an existing keyframe.
 *
 * @param	KeyIndex	The index of the key to update in the track's key array. 
 * @param	TrackInst	The instance of this track to update. 
 */
void UInterpTrackBoolProp::UpdateKeyframe( INT KeyIndex, UInterpTrackInst* TrackInst )
{
	UInterpTrackInstBoolProp* PropInst = CastChecked<UInterpTrackInstBoolProp>(TrackInst);

	// We must have a valid pointer to the boolean to modify
	if( !PropInst->BoolProp )
		return;

	// Must have a valid key index.
	if( !BoolTrack.IsValidIndex(KeyIndex) )
		return;

	BoolTrack(KeyIndex).Value = *(BITFIELD*)(PropInst->BoolProp);
}

/** 
 * Updates the instance of this track based on the new position. This is for editor preview.
 *
 * @param	NewPosition	The position of the track in the timeline. 
 * @param	TrackInst	The instance of this track to update. 
 */
void UInterpTrackBoolProp::PreviewUpdateTrack( FLOAT NewPosition, UInterpTrackInst* TrackInst )
{
	UpdateTrack( NewPosition, TrackInst, FALSE );
}

/** 
 * Updates the instance of this track based on the new position. This is called in the game, when USeqAct_Interp is ticked.
 *
 * @param	NewPosition	The position of the track in the timeline. 
 * @param	TrackInst	The instance of this track to update. 
 * @param	bJump		Indicates if this is a sudden jump instead of a smooth move to the new position.
 */
void UInterpTrackBoolProp::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrackInst, UBOOL bJump)
{
	AActor* Actor = TrackInst->GetGroupActor();

	// If we don't have a group actors, then we can't modify the boolean stored on the actor.
	if( !Actor )
	{
		return;
	}

	UInterpTrackInstBoolProp* PropInst = CastChecked<UInterpTrackInstBoolProp>(TrackInst);

	// We must have a valid pointer to the boolean to modify
	if( !PropInst->BoolProp )
		return;

	UBOOL NewBoolValue = FALSE;
	const INT NumOfKeys = BoolTrack.Num();

	// If we have zero keys, use the property's original value. 
	if( NumOfKeys == 0 )
	{
		NewBoolValue = (*(BITFIELD*)PropInst->BoolProp & (BITFIELD)PropInst->BitMask) != 0;
	}
	// If we only have one key or the position is before
	// the first key, use the value of the first key.
	else if( NumOfKeys == 1 || NewPosition <= BoolTrack(0).Time )
	{
		NewBoolValue = BoolTrack(0).Value;
	}
	// If the position is past the last key, use the value of the last key.
	else if( NewPosition >= BoolTrack(NumOfKeys - 1).Time )
	{
		NewBoolValue = BoolTrack(NumOfKeys - 1).Value;
	}
	// Else, search through all the keys, looking for the 
	// keys that encompass the new timeline position.
	else
	{
		// Start iterating from the second key because we already 
		// determined if the new position is less than the first key.
		for( INT KeyIndex = 1; KeyIndex < NumOfKeys; KeyIndex++ )
		{
			if( NewPosition < BoolTrack(KeyIndex).Time )
			{
				// We found the key that comes after the new position, 
				// use the value of the proceeding key. 
				NewBoolValue = BoolTrack(KeyIndex - 1).Value;
				break;
			}
		}
	}

	if (NewBoolValue)
	{
		*(BITFIELD*)PropInst->BoolProp |= (BITFIELD)PropInst->BitMask;
	}
	else
	{
		*(BITFIELD*)PropInst->BoolProp &= ~(BITFIELD)PropInst->BitMask;
	}

	// If we have a custom callback for this property, call that, otherwise just force update components on the actor.
	if( PropInst->CallPropertyUpdateCallback() == FALSE )
	{
		// We update components, so things like draw scale take effect.
		Actor->ForceUpdateComponents(FALSE,FALSE);
	}
}

/** 
 * Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
 * 
 * @return	String name of the helper class.
 */
const FString UInterpTrackBoolProp::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackBoolPropHelper") );
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstBoolProp
-----------------------------------------------------------------------------*/

/** 
 * Initialize the track instance.
 *
 * @param	Track	The track associated to this instance.
 */
void UInterpTrackInstBoolProp::InitTrackInst( UInterpTrack* Track )
{
	Super::InitTrackInst( Track );

	AActor* Actor = GetGroupActor();

	if( !Actor )
	{
		return;
	}

	// Store a pointer to the bitfield data for the property we will be interpolating.
	FName PropertyName;
	Track->GetPropertyName( PropertyName );
	BoolProp = Actor->GetInterpBoolPropertyRef(PropertyName, (BITFIELD&)BitMask);

	SetupPropertyUpdateCallback(Actor, PropertyName);
 	InterpTools::EnableCameraPostProcessFlag(Actor, PropertyName);
}

/** 
 * Save any variables from the actor that will be modified by this instance.
 *
 * @param	Track	The track associated to this instance.
 */
void UInterpTrackInstBoolProp::SaveActorState( UInterpTrack* Track )
{
	if( !GetGroupActor() || !BoolProp )
	{
		return;
	}

	// Remember current value of property for when we quit Matinee
	ResetBool = (*((BITFIELD*)(BoolProp)) & BitMask) != 0;
}

/** 
 * Restores any variables modified on the actor by this instance.
 *
 * @param	Track	The track associated to this instance.
 */
void UInterpTrackInstBoolProp::RestoreActorState( UInterpTrack* Track )
{
	AActor* Actor = GetGroupActor();

	if( !Actor || !BoolProp )
	{
		return;
	}

	// Restore original value of property
	if (ResetBool)
	{
		*((BITFIELD*)(BoolProp)) |= (BITFIELD)BitMask;
	}
	else
	{
		*((BITFIELD*)(BoolProp)) &= ~(BITFIELD)BitMask;
	}

	// We update components, so things like draw scale take effect.
	// Don't force update all components unless we're in the editor.
	Actor->ConditionalForceUpdateComponents(FALSE,FALSE);
}

/*-----------------------------------------------------------------------------
	UInterpTrackColorProp
-----------------------------------------------------------------------------*/

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackColorProp::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	UInterpTrackInstColorProp* PropInst = CastChecked<UInterpTrackInstColorProp>(TrInst);
	if( !PropInst->ColorProp )
		return INDEX_NONE;

	INT NewKeyIndex = VectorTrack.AddPoint( Time, FVector(0.f));
	VectorTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

	UpdateKeyframe(NewKeyIndex, TrInst);

	VectorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** Change the value of an existing keyframe. */
void UInterpTrackColorProp::UpdateKeyframe(INT KeyIndex, UInterpTrackInst* TrInst)
{
	UInterpTrackInstColorProp* PropInst = CastChecked<UInterpTrackInstColorProp>(TrInst);
	if( !PropInst->ColorProp )
	{
		return;
	}

	if( KeyIndex < 0 || KeyIndex >= VectorTrack.Points.Num() )
	{
		return;
	}

	FColor ColorValue = *((FColor*)(PropInst->ColorProp));
	FLinearColor LinearValue(ColorValue);
	VectorTrack.Points(KeyIndex).OutVal = FVector(LinearValue.R, LinearValue.G, LinearValue.B);

	VectorTrack.AutoSetTangents(CurveTension);
}


/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the editor, when scrubbing/previewing etc.
*/
void UInterpTrackColorProp::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, FALSE);
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the game, when USeqAct_Interp is ticked
*  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
*/
void UInterpTrackColorProp::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	UInterpTrackInstColorProp* PropInst = CastChecked<UInterpTrackInstColorProp>(TrInst);
	if(!PropInst->ColorProp)
	{
		return;
	}

	FColor DefaultColor = *((FColor*)(PropInst->ColorProp));
	FLinearColor DefaultLinearColor = DefaultColor;
	FVector DefaultColorAsVector(DefaultLinearColor.R, DefaultLinearColor.G, DefaultLinearColor.B);
	FVector NewVectorValue = VectorTrack.Eval( NewPosition, DefaultColorAsVector );
	FColor NewColorValue = FLinearColor(NewVectorValue.X, NewVectorValue.Y, NewVectorValue.Z);
	*((FColor*)(PropInst->ColorProp)) = NewColorValue;

	// If we have a custom callback for this property, call that, otherwise just force update components on the actor.
	if(PropInst->CallPropertyUpdateCallback()==FALSE)
	{
		// We update components, so things like draw scale take effect.
		Actor->ForceUpdateComponents(FALSE,FALSE);
	}
}

/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
* @return	String name of the helper class.*/
const FString	UInterpTrackColorProp::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackColorPropHelper") );
}



/*-----------------------------------------------------------------------------
UInterpTrackInstColorProp
-----------------------------------------------------------------------------*/

/** Called before Interp editing to put object back to its original state. */
void UInterpTrackInstColorProp::SaveActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!ColorProp)
		return;

	// Remember current value of property for when we quite Matinee
	ResetColor = *((FColor*)(ColorProp));
}

/** Restore the saved state of this Actor. */
void UInterpTrackInstColorProp::RestoreActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!ColorProp)
		return;

	// Restore original value of property
	*((FColor*)(ColorProp)) = ResetColor;

	// We update components, so things like draw scale take effect.
	// Don't force update all components unless we're in the editor.
	Actor->ConditionalForceUpdateComponents(FALSE,FALSE);
}

/** Initialize this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstColorProp::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	FName PropertyName;
	Track->GetPropertyName( PropertyName );
	ColorProp = Actor->GetInterpColorPropertyRef(PropertyName);

	SetupPropertyUpdateCallback(Actor, PropertyName);
}



/*-----------------------------------------------------------------------------
UInterpTrackLinearColorProp
-----------------------------------------------------------------------------*/

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackLinearColorProp::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	UInterpTrackInstLinearColorProp* PropInst = CastChecked<UInterpTrackInstLinearColorProp>(TrInst);
	if( !PropInst->ColorProp )
		return INDEX_NONE;

	INT NewKeyIndex = LinearColorTrack.AddPoint( Time, FLinearColor(0.f, 0.f, 0.f, 1.f));
	LinearColorTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

	UpdateKeyframe(NewKeyIndex, TrInst);

	LinearColorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}


/** Change the value of an existing keyframe. */
void UInterpTrackLinearColorProp::UpdateKeyframe(INT KeyIndex, UInterpTrackInst* TrInst)
{
	UInterpTrackInstLinearColorProp* PropInst = CastChecked<UInterpTrackInstLinearColorProp>(TrInst);
	if( !PropInst->ColorProp )
	{
		return;
	}

	if( KeyIndex < 0 || KeyIndex >= LinearColorTrack.Points.Num() )
	{
		return;
	}

	FLinearColor ColorValue = *((FLinearColor*)(PropInst->ColorProp));
	LinearColorTrack.Points(KeyIndex).OutVal = ColorValue;

	LinearColorTrack.AutoSetTangents(CurveTension);
}


/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the editor, when scrubbing/previewing etc.
*/
void UInterpTrackLinearColorProp::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, FALSE);
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the game, when USeqAct_Interp is ticked
*  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
*/
void UInterpTrackLinearColorProp::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	UInterpTrackInstLinearColorProp* PropInst = CastChecked<UInterpTrackInstLinearColorProp>(TrInst);
	if(!PropInst->ColorProp)
	{
		return;
	}

	FLinearColor DefaultValue = *((FLinearColor*)(PropInst->ColorProp));
	FLinearColor NewVectorValue = LinearColorTrack.Eval( NewPosition, DefaultValue);
	*((FLinearColor*)(PropInst->ColorProp)) = NewVectorValue;

	// If we have a custom callback for this property, call that, otherwise just force update components on the actor.
	if(PropInst->CallPropertyUpdateCallback()==FALSE)
	{
		// We update components, so things like draw scale take effect.
		Actor->ForceUpdateComponents(FALSE,FALSE);
	}
}

/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
* @return	String name of the helper class.*/
const FString	UInterpTrackLinearColorProp::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackLinearColorPropHelper") );
}



/*-----------------------------------------------------------------------------
UInterpTrackInstLinearColorProp
-----------------------------------------------------------------------------*/

/** Called before Interp editing to put object back to its original state. */
void UInterpTrackInstLinearColorProp::SaveActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!ColorProp)
		return;

	// Remember current value of property for when we quite Matinee
	 ResetColor = *((FLinearColor*)(ColorProp));
}

/** Restore the saved state of this Actor. */
void UInterpTrackInstLinearColorProp::RestoreActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!ColorProp)
		return;

	// Restore original value of property
	*((FLinearColor*)(ColorProp)) = ResetColor;

	// We update components, so things like draw scale take effect.
	// Don't force update all components unless we're in the editor.
	Actor->ConditionalForceUpdateComponents(FALSE,FALSE);
}

/** Initialize this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstLinearColorProp::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	FName PropertyName;
	Track->GetPropertyName( PropertyName );
	ColorProp = Actor->GetInterpLinearColorPropertyRef(PropertyName);

	SetupPropertyUpdateCallback(Actor, PropertyName);
	InterpTools::EnableCameraPostProcessFlag(Actor, PropertyName);
}


/*-----------------------------------------------------------------------------
	UInterpTrackEvent
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackEvent, EventTrack)
STRUCTTRACK_GETTIMERANGE(UInterpTrackEvent, EventTrack, Time)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackEvent, EventTrack, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackEvent, EventTrack, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackEvent, EventTrack, Time, FEventTrackKey )
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackEvent, EventTrack )
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackEvent, EventTrack, Time, FEventTrackKey )
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackEvent, EventTrack, Time)

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackEvent::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	FEventTrackKey NewEventKey;
	NewEventKey.EventName = NAME_None;
	NewEventKey.Time = Time;

	// Find the correct index to insert this key.
	INT i=0; for( i=0; i<EventTrack.Num() && EventTrack(i).Time < Time; i++);
	EventTrack.Insert(i);
	EventTrack(i) = NewEventKey;

	return i;
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the game, when USeqAct_Interp is ticked
*  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
*/
void UInterpTrackEvent::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	UInterpTrackInstEvent* EventInst = CastChecked<UInterpTrackInstEvent>(TrInst);
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( EventInst->GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
	UInterpGroup* Group = CastChecked<UInterpGroup>( GetOuter() );
	UInterpData* IData = CastChecked<UInterpData>( Group->GetOuter() );

	// We'll consider playing events in reverse if we're either actively playing in reverse or if
	// we're in a paused state but forcing an update to an older position (scrubbing backwards in editor.)
	UBOOL bIsPlayingBackwards =
		( Seq->bIsPlaying && Seq->bReversePlayback ) ||
		( bJump && !Seq->bIsPlaying && NewPosition < EventInst->LastUpdatePosition );

	// Find the interval between last update and this to check events with.
	UBOOL bFireEvents = true;

	if(bJump)
	{
		// If we are playing forwards, and the flag is set, fire events even if we are 'jumping'.
		if(bFireEventsWhenJumpingForwards && !bIsPlayingBackwards)
		{
			bFireEvents = true;
		}
		else
		{
			bFireEvents = false;
		}
	}

	// If playing sequence forwards.
	FLOAT MinTime, MaxTime;
	if(!bIsPlayingBackwards)
	{
		MinTime = EventInst->LastUpdatePosition;
		MaxTime = NewPosition;

		// Slight hack here.. if playing forwards and reaching the end of the sequence, force it over a little to ensure we fire events actually on the end of the sequence.
		if(MaxTime == IData->InterpLength)
		{
			MaxTime += (FLOAT)KINDA_SMALL_NUMBER;
		}

		if(!bFireEventsWhenForwards)
		{
			bFireEvents = false;
		}
	}
	// If playing sequence backwards.
	else
	{
		MinTime = NewPosition;
		MaxTime = EventInst->LastUpdatePosition;

		// Same small hack as above for backwards case.
		if(MinTime == 0.f)
		{
			MinTime -= (FLOAT)KINDA_SMALL_NUMBER;
		}

		if(!bFireEventsWhenBackwards)
		{
			bFireEvents = false;
		}
	}

	// If we should be firing events for this track...
	if(bFireEvents)
	{
		// See which events fall into traversed region.
		for(INT i=0; i<EventTrack.Num(); i++)
		{
			FLOAT EventTime = EventTrack(i).Time;

			// Need to be slightly careful here and make behavior for firing events symmetric when playing forwards of backwards.
			UBOOL bFireThisEvent = false;
			if(!bIsPlayingBackwards)
			{
				if( EventTime >= MinTime && EventTime < MaxTime )
				{
					bFireThisEvent = true;
				}
			}
			else
			{
				if( EventTime > MinTime && EventTime <= MaxTime )
				{
					bFireThisEvent = true;
				}
			}

			if( bFireThisEvent )
			{
				Seq->NotifyEventTriggered(this, i);
			}
		}
	}

	// Update LastUpdatePosition.
	EventInst->LastUpdatePosition = NewPosition;
}

/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
* @return	String name of the helper class.*/
const FString	UInterpTrackEvent::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackEventHelper") );
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstEvent
-----------------------------------------------------------------------------*/

/** Initialise this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstEvent::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );

	LastUpdatePosition = Seq->Position;
}

/*-----------------------------------------------------------------------------
	UInterpTrackNotify
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackNotify, NotifyTrack)
STRUCTTRACK_GETTIMERANGE(UInterpTrackNotify, NotifyTrack, Time)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackNotify, NotifyTrack, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackNotify, NotifyTrack, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackNotify, NotifyTrack, Time, FNotifyTrackKey )
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackNotify, NotifyTrack )
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackNotify, NotifyTrack, Time, FNotifyTrackKey )
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackNotify, NotifyTrack, Time)

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackNotify::AddKeyframe( FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode )
{
	FNotifyTrackKey NewNotifyKey;
	NewNotifyKey.Time = Time;

	// Find the correct index to insert this key
	INT i = 0; for (i = 0; i < NotifyTrack.Num() && NotifyTrack(i).Time < Time; i++);
	NotifyTrack.Insert(i);
	NotifyTrack(i) = NewNotifyKey;

	return i;
}

/** Activate the AnimNotify in the Editor */
void UInterpTrackNotify::PreviewUpdateTrack( FLOAT NewPosition, class UInterpTrackInst* TrInst )
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );

	// Dont play notifies unless we are previewing playback (ie not scrubbing).
	UBOOL bJump = !(Seq->bIsPlaying);
	UpdateTrack(NewPosition, TrInst, bJump);
}

/** Activate the AnimNotify in the game */
void UInterpTrackNotify::UpdateTrack( FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump )
{
	// Can't fire off a notify without an AnimNodeSequence to pass to the Notify function
	if(!Node)
	{
		return;
	}

	UInterpTrackInstNotify* NotifyInst = CastChecked<UInterpTrackInstNotify>(TrInst);
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>(NotifyInst->GetOuter());
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>(GrInst->GetOuter());

	UBOOL bIsPlayingBackwards = Seq->bIsPlaying && Seq->bReversePlayback;
	FLOAT MinTime = bIsPlayingBackwards ? NewPosition : NotifyInst->LastUpdatePosition;
	FLOAT MaxTime = bIsPlayingBackwards ? NotifyInst->LastUpdatePosition : NewPosition;
	UBOOL bFireNotifies = !bJump && !bIsPlayingBackwards;

	if (bFireNotifies)
	{
		// See which keys fall into traversed region.
		for(INT i = 0; i < NotifyTrack.Num(); i++)
		{
			FLOAT NotifyTime = NotifyTrack(i).Time;

			// Need to be slightly careful here and make behavior for firing notifies symmetric when playing forwards or backwards.
			UBOOL bFireThisNotify = bIsPlayingBackwards ? (NotifyTime >= MinTime && NotifyTime <= MaxTime) : (NotifyTime >= MinTime && NotifyTime <= MaxTime);

			if(bFireThisNotify)
			{
				NotifyTrack(i).Notify->Notify(Node);
			}
		}
	}

	// Update LastUpdatePosition.
	NotifyInst->LastUpdatePosition = NewPosition;
}

/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
* @return	String name of the helper class.*/
const FString UInterpTrackNotify::GetEdHelperClassName() const
{
	return FString(TEXT("UnrealEd.InterpTrackNotifyHelper"));
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstNotify
-----------------------------------------------------------------------------*/

void UInterpTrackInstNotify::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst(Track);

	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>(GetOuter());
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>(GrInst->GetOuter());

	UInterpTrackNotify* NotifyTrack = CastChecked<UInterpTrackNotify>(Track);

	if(NotifyTrack)
	{
		APawn* Pawn = CastChecked<APawn>(GrInst->GetGroupActor());

		/**
		 * Set up transient objects that get used in the Notify's functions for all the notify types.
		 * - OuterSet: Set as the Notify object's Outer->Outer and used to find the PreviewSkeletalMeshName
		 * - OuterSequence: Set as the Notify object's Outer
		 * - Node: The AnimNodeSequence object that gets passed into the Notify function
		 * - Node->ParentNodes(0): An AnimNodeBlend object used for it's NodeName property
		 */
		if (!NotifyTrack->OuterSet)
			NotifyTrack->OuterSet = CastChecked<UAnimSet>(GEngine->StaticConstructObject(UAnimSet::StaticClass(), NotifyTrack));

		if (!NotifyTrack->OuterSequence)
			NotifyTrack->OuterSequence = CastChecked<UAnimSequence>(GEngine->StaticConstructObject(UAnimSequence::StaticClass(), NotifyTrack->OuterSet));

		if (!NotifyTrack->Node)
			NotifyTrack->Node = CastChecked<UAnimNodeSequence>(GEngine->StaticConstructObject(UAnimNodeSequence::StaticClass(), NotifyTrack));

		if (NotifyTrack->Node->ParentNodes.Num() == 0)
			NotifyTrack->Node->ParentNodes.AddItem(CastChecked<UAnimNodeBlend>(GEngine->StaticConstructObject(UAnimNodeBlend::StaticClass(), NotifyTrack)));

		/**
		 * Assign values from the Pawn and the Track to the transient objects
		 * - OuterSet->PreviewSkelMeshName: Used to load the pawn for preview
		 * - Node->ParentNode(0)->NodeName: The name of the slot node to use when determining which sequence the Node is part of
		 * - Node->AnimSeq: A reference to the Track's OuterSequence object used to access the AnimSequence from the Node
		 * - Node->SkelComponent: The Pawn's mesh, used to access to the Node's Owner
		 */
		NotifyTrack->OuterSet->PreviewSkelMeshName = *Pawn->Mesh->SkeletalMesh->GetPathName();
		NotifyTrack->Node->ParentNodes(0)->NodeName = NotifyTrack->ParentNodeName;
		NotifyTrack->Node->AnimSeq = NotifyTrack->OuterSequence;
		NotifyTrack->Node->SkelComponent = Pawn->Mesh;

#if WITH_EDITORONLY_DATA
		// Hack to update the Outer for the notifies that were created before an AnimSequence object was being used
		// Once the package is saved, will never run again for those notify keys
		for (INT i = 0; i < NotifyTrack->NotifyTrack.Num(); i++)
		{
			UAnimNotify* Notify = NotifyTrack->NotifyTrack(i).Notify;

			if (!(Notify->GetOuter()->IsA(UAnimSequence::StaticClass())))
			{
				NotifyTrack->NotifyTrack(i).Notify = CastChecked<UAnimNotify>(UObject::StaticDuplicateObject(Notify, Notify, NotifyTrack->OuterSequence, *Notify->GetName()));
				NotifyTrack->MarkPackageDirty();
			}
		}
#endif
	}

	LastUpdatePosition = Seq->Position;
}

/*-----------------------------------------------------------------------------
	UInterpTrackDirector
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackDirector, CutTrack)
STRUCTTRACK_GETTIMERANGE(UInterpTrackDirector, CutTrack, Time)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackDirector, CutTrack, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackDirector, CutTrack, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackDirector, CutTrack, Time, FDirectorTrackCut )
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackDirector, CutTrack )
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackDirector, CutTrack, Time, FDirectorTrackCut )
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackDirector, CutTrack, Time)

/** Handle some specifics post load */
void UInterpTrackDirector::PostLoad()
{
    Super::PostLoad();

    //if shot names have not been assigned, do it now
    INT i=0;
    for (i=0; i < GetNumKeyframes(); i++)
    {
        INT ShotNum = CutTrack(i).ShotNumber;
        if (ShotNum == 0)
        {
            ShotNum = GenerateCameraShotNumber(i);
            CutTrack(i).ShotNumber = GenerateCameraShotNumber(i);
        } 
    }
}

/** Add a new keyframe at the specified time. Returns index of new keyframe. */
INT UInterpTrackDirector::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	FDirectorTrackCut NewCut;
	NewCut.TargetCamGroup = NAME_None;
	NewCut.TransitionTime = 0.f;
	NewCut.Time = Time;

	// Find the correct index to insert this cut.
	INT i=0; for( i=0; i<CutTrack.Num() && CutTrack(i).Time < Time; i++);
	CutTrack.Insert(i);
	CutTrack(i) = NewCut;
	
	//Generate a shot name
	INT ShotNum = GenerateCameraShotNumber(i);
    CutTrack(i).ShotNumber = ShotNum;
    
	return i;
}

void UInterpTrackDirector::DisplayShotNamesInHUD(UInterpGroupInst* GrInst, APlayerController* PC, FLOAT Time)
{
    if (PC && PC->myHUD)
    {
        if (CutTrack.Num() > 0)
        {
            if (PC->myHUD->bShowDirectorInfoDebug)
            {
                //use Debug Text
                FString GroupName = GrInst->Group->GroupName.ToString();
                FString ShotString = FString::Printf(TEXT("Director: %s - %s"), *GroupName,*GetViewedCameraShotName(Time));
                GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)((PTRINT)this), 1.0f, FColor(255,255,255), ShotString);
            }
            if (PC->myHUD->bShowDirectorInfoHUD)
            {
                //use HUD
                
                FString GroupName = GrInst->Group->GroupName.ToString();
                FString ShotString = GetViewedCameraShotName(Time); 
                FLOAT OffsetX = 400;
                FLOAT OffsetY = 300;
               
                FString DisplayStrings[] = {GroupName,ShotString};
                FVector2D DisplayOffsets[] = {FVector2D(-OffsetX,OffsetY), FVector2D(OffsetX,OffsetY)};
                
                //find the existing messages and remove them if they exist...
                TArrayNoInit<struct FKismetDrawTextInfo> &TextArray = PC->myHUD->KismetTextInfo;
                for (int i=0; i<(TextArray.Num()); i++)
                {
                    for (INT display=0; display<2; display++)
                    { 
                        if (TextArray(i).MessageOffset == DisplayOffsets[display])
                        {
                            TextArray.Remove(i);
                        }
                    }
                }
                //add a new one to the hud
                for (INT display=0; display<2; display++)
                { 
                    FKismetDrawTextInfo DrawTextInfo;
                    appMemzero(&DrawTextInfo, sizeof(FKismetDrawTextInfo));
                    DrawTextInfo.MessageText = DisplayStrings[display];
                    DrawTextInfo.AppendedText = TEXT(""); 
                    DrawTextInfo.MessageFont = GEngine->LargeFont;
                    DrawTextInfo.MessageEndTime = GWorld->GetWorldInfo()->TimeSeconds + 1; //-1;
                    DrawTextInfo.MessageFontScale = FVector2D(1,1);
                    DrawTextInfo.MessageOffset = DisplayOffsets[display]; 
                    DrawTextInfo.MessageColor = FColor(255,255,255,255);
                    TextArray.AddItem(DrawTextInfo);
                }
            }
        }
    }        
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the game, when USeqAct_Interp is ticked
*  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
*/
void UInterpTrackDirector::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	UInterpTrackInstDirector* DirInst = CastChecked<UInterpTrackInstDirector>(TrInst);
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );

	// Actor for a Director group should be a PlayerController.
	APlayerController* PC = Cast<APlayerController>(GrInst->GetGroupActor());
	if( PC )
	{
        
        #if !FINAL_RELEASE
        DisplayShotNamesInHUD(GrInst, PC, NewPosition);
        #endif
        
		USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
		// server is authoritative on viewtarget changes
		if (PC->Role == ROLE_Authority || Seq->bClientSideOnly || bSimulateCameraCutsOnClients)
		{
			FLOAT CutTime, CutTransitionTime;
			FName ViewGroupName = GetViewedGroupName(NewPosition, CutTime, CutTransitionTime);
			// if our group name was specified, make sure we use ourselves instead of any other instances with that name (there might be multiple in the multiplayer case)
			UInterpGroupInst* ViewGroupInst = (ViewGroupName == GrInst->Group->GroupName) ? GrInst : Seq->FindFirstGroupInstByName(ViewGroupName);
			
			AActor* ViewTarget = PC->GetViewTarget();
			if( ViewGroupInst && ViewGroupInst->GetGroupActor() && (ViewGroupInst->GetGroupActor() != PC) )
			{
				// If our desired view target is different from our current one...
				if( ViewTarget != ViewGroupInst->GroupActor )
				{
					// If we don't have a backed up ViewTarget, back up this one.
					if( !DirInst->OldViewTarget )
					{
						// If the actor's current view target is a director track camera, then we want to store 
						// the director track's 'old view target' in case the current Matinee sequence finishes
						// before our's does.
						UInterpTrackInstDirector* PreviousDirInst = PC->GetControllingDirector();
						if( PreviousDirInst != NULL && PreviousDirInst->OldViewTarget != NULL )
						{
							// Store the underlying director track's old view target so we can restore this later
							DirInst->OldViewTarget = PreviousDirInst->OldViewTarget;
						}
						else
						{
							DirInst->OldViewTarget = ViewTarget;
						}
					}

					PC->bClientSimulatingViewTarget = bSimulateCameraCutsOnClients;
					PC->SetControllingDirector( DirInst );
					PC->eventNotifyDirectorControl(TRUE, Seq);

					//debugf(TEXT("UInterpTrackDirector::UpdateTrack SetViewTarget ViewGroupInst->GroupActor Time:%f Name: %s"), GWorld->GetTimeSeconds(), *ViewGroupInst->GroupActor->GetFName());
					// Change view to desired view target.
					FViewTargetTransitionParams TransitionParams(EC_EventParm);
					TransitionParams.BlendTime = CutTransitionTime;

					// a bit ugly here, but we don't want this particular SetViewTarget to bash OldViewTarget
					AActor* const BackupViewTarget = DirInst->OldViewTarget;
					PC->SetViewTarget( ViewGroupInst->GroupActor, TransitionParams);
					PC->bCameraCut = TRUE;
					DirInst->OldViewTarget = BackupViewTarget;
				}
			}
			// If assigning to nothing or the PlayerController, restore any backed up viewtarget.
			else if (DirInst->OldViewTarget != NULL)
			{
				//debugf(TEXT("UInterpTrackDirector::UpdateTrack SetViewTarget DirInst->OldViewTarget Time:%f Name: %s"), GWorld->GetTimeSeconds(), *DirInst->OldViewTarget->GetFName());
				if (!DirInst->OldViewTarget->IsPendingKill())
				{
					FViewTargetTransitionParams TransitionParams(EC_EventParm);
					TransitionParams.BlendTime = CutTransitionTime;
					PC->SetViewTarget( DirInst->OldViewTarget, TransitionParams );
				}
				PC->eventNotifyDirectorControl(FALSE, Seq);
				PC->SetControllingDirector( NULL );
				PC->bClientSimulatingViewTarget = FALSE;
				DirInst->OldViewTarget = NULL;
			}
		}
	}
}

/** For the supplied time, get the specific key index */
INT UInterpTrackDirector::GetKeyframeIndex( FLOAT KeyTime ) const
{
    INT PrevKeyIndex = INDEX_NONE; // Index of key before current time.
    if(CutTrack.Num() > 0 && CutTrack(0).Time < KeyTime)
    {
        for( INT i=0; i < CutTrack.Num() && CutTrack(i).Time <= KeyTime; i++)
        {
            PrevKeyIndex = i;
        }
    }
    
    return PrevKeyIndex;
}

/** For the supplied time, find which group name we should be viewing from. */
FName UInterpTrackDirector::GetViewedGroupName(FLOAT CurrentTime, FLOAT& CutTime, FLOAT& CutTransitionTime)
{
	INT KeyIndex = GetKeyframeIndex(CurrentTime);
	// If no index found - we are before first frame (or no frames present), so use the director group name.
	if(KeyIndex == INDEX_NONE)
	{
		CutTime = 0.f;
		CutTransitionTime = 0.f;

		UInterpGroup* Group = CastChecked<UInterpGroup>( GetOuter() );
		return Group->GroupName;
	}
	else
	{
		CutTime = CutTrack(KeyIndex).Time;
		CutTransitionTime = CutTrack(KeyIndex).TransitionTime;

		return CutTrack(KeyIndex).TargetCamGroup;
	}
}

//** For the supplied time, get the name of our camera shot
const FString UInterpTrackDirector::GetViewedCameraShotName(FLOAT CurrentTime) const
{
    FString ShotName = TEXT("");
   
    INT KeyIndex = GetKeyframeIndex(CurrentTime);
    if (KeyIndex != INDEX_NONE)
    {
        ShotName = GetFormattedCameraShotName(KeyIndex);    
    }
    return ShotName;
}

/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
* @return	String name of the helper class.*/
const FString	UInterpTrackDirector::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackDirectorHelper") );
}

/** Get an autogenerated Camera Shot name for a given key frame index 
* @param KeyIndex   The camera cut key index
* @return   Sring of the newly geated shot name
*/
const INT UInterpTrackDirector::GenerateCameraShotNumber(INT KeyIndex) const
{
    //NOTE: this could give us an off by 1 error depending on when it is called.
    //The current implementation assumes the shot is already inserted int CutTrackArray
    
    const INT Interval = 10;
    INT ShotNum = Interval;
    INT LastKeyIndex = GetNumKeyframes() - 1;
    
    INT PrevShotNum = 0;
    //get the preceding shot number if any
    if (KeyIndex > 0)
    {
        PrevShotNum = CutTrack(KeyIndex - 1).ShotNumber; 
    }
   
    if (KeyIndex < LastKeyIndex)
    {
        //we're inserting before something before the first frame
        INT NextShotNum = CutTrack(KeyIndex + 1).ShotNumber;
        if (NextShotNum == 0)
        {
            NextShotNum = PrevShotNum + (Interval*2);        
        }
        
        if (NextShotNum > PrevShotNum)
        {
            //find a midpoint if we're in order
            
            //try to stick to the nearest interval if possible
            INT NearestInterval = PrevShotNum - (PrevShotNum % Interval) + Interval;
            if (NearestInterval > PrevShotNum && NearestInterval < NextShotNum)
            {
                ShotNum = NearestInterval;
            }
            //else find the exact mid point
            else
            {
                ShotNum = ((NextShotNum - PrevShotNum) / 2) + PrevShotNum;
            }
        }
        else
        {
            //Just use the previous shot number + 1 with we're out of order
            ShotNum = PrevShotNum + 1;
        }
    }
    else
    {   
        //we're adding to the end of the track
        ShotNum = PrevShotNum + Interval;
    }
    
    return ShotNum;
}

const FString UInterpTrackDirector::GetFormattedCameraShotName(INT KeyIndex) const
{
    INT ShotNum = CutTrack(KeyIndex).ShotNumber;
    FString NameString = TEXT("Shot_");
    FString NumString = FString::Printf(TEXT("%d"),ShotNum);
    INT LEN = NumString.Len();
    for (int i=0; i<(4 - LEN); i++)
    {
        NameString += TEXT("0");
    } 
    NameString += NumString;
    return NameString;
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstDirector
-----------------------------------------------------------------------------*/

void UInterpTrackInstDirector::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>(GetOuter());
	APlayerController* PC = Cast<APlayerController>(GrInst->GetGroupActor());
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
	if (PC && PC->PlayerCamera && Seq && !Track->IsDisabled())
	{
		UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(GrInst->Group);
		if (DirGroup)
		{
			UInterpTrackDirector* DirTrack = DirGroup->GetDirectorTrack();
			if (DirTrack && DirTrack->CutTrack.Num() > 0)
			{
				PC->bInteractiveMode = FALSE;
				// Backup existing player camera settings
				OldRenderingOverrides = PC->PlayerCamera->RenderingOverrides;
				// Apply the matinee's rendering overrides
				PC->PlayerCamera->RenderingOverrides = Seq->RenderingOverrides;
			}
		}
	}
}

/** Use this to ensure we always cut back to the players last view-target. Need this in case there was no explicit cut back to the Dir track. */
void UInterpTrackInstDirector::TermTrackInst(UInterpTrack* Track)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>(GetOuter());
	APlayerController* PC = Cast<APlayerController>(GrInst->GetGroupActor());
	if (PC != NULL)
	{
		USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
		if (OldViewTarget != NULL && !OldViewTarget->IsPendingKill())
		{
			// if we haven't already, restore original view target.
			AActor* ViewTarget = PC->GetViewTarget();
			if (ViewTarget != OldViewTarget)
			{
				PC->SetViewTarget(OldViewTarget);
			}
		}
		// this may be a duplicate call if it was already called in UpdateTrack(), but that's better than not at all and leaving code thinking we stayed in matinee forever
		PC->eventNotifyDirectorControl(FALSE, Seq);
		PC->SetControllingDirector( NULL );
		PC->bClientSimulatingViewTarget = FALSE;
		if (PC->PlayerCamera && !Track->IsDisabled())
		{
			UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(GrInst->Group);
			if (DirGroup)
			{
				UInterpTrackDirector* DirTrack = DirGroup->GetDirectorTrack();
				if (DirTrack && DirTrack->CutTrack.Num() > 0)
				{
					PC->bInteractiveMode = TRUE;
					// Restore the existing settings
					PC->PlayerCamera->RenderingOverrides = OldRenderingOverrides;
				}
			}
		}
	}
	
	OldViewTarget = NULL;

	Super::TermTrackInst( Track );
}


/*-----------------------------------------------------------------------------
	UInterpTrackFade
-----------------------------------------------------------------------------*/

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackFade::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	INT NewKeyIndex = FloatTrack.AddPoint( Time, 0.f );
	FloatTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** Change the value of an existing keyframe. */
void UInterpTrackFade::UpdateKeyframe(INT KeyIndex, UInterpTrackInst* TrInst)
{
	// Do nothing here - fading is all set up through curve editor.
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the editor, when scrubbing/previewing etc.
*/
void UInterpTrackFade::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	// Do nothing - in the editor Matinee itself handles updating the editor viewports.
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the game, when USeqAct_Interp is ticked
*  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
*/
void UInterpTrackFade::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	// when doing a skip in game, don't update fading - we only want it applied when actually running
	if (!bJump || !GIsGame)
	{
		UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );

		// Actor for a Director group should be a PlayerController.
		APlayerController* PC = Cast<APlayerController>(GrInst->GetGroupActor());
		if(PC && PC->PlayerCamera && !PC->PlayerCamera->bDeleteMe)
		{
			PC->PlayerCamera->bEnableFading = true;
			PC->PlayerCamera->FadeAmount = GetFadeAmountAtTime(NewPosition);
			// disable the Kismet fade control so that we don't thrash
			PC->PlayerCamera->FadeTimeRemaining = 0.0f;
		}
	}
}

/** Return the amount of fading we want at the given time. */
FLOAT UInterpTrackFade::GetFadeAmountAtTime(FLOAT Time)
{
	FLOAT Fade = FloatTrack.Eval(Time, 0.f);
	Fade = Clamp(Fade, 0.f, 1.f);
	return Fade;
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstFade
-----------------------------------------------------------------------------*/

/** Use this to turn off any fading that was applied by this track to this player controller. */
void UInterpTrackInstFade::TermTrackInst(UInterpTrack* Track)
{
	UInterpTrackFade *FadeTrack = Cast<UInterpTrackFade>(Track);
	if (FadeTrack == NULL || !FadeTrack->bPersistFade)
	{
		UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
		APlayerController* PC = Cast<APlayerController>(GrInst->GroupActor);
		if(PC && PC->PlayerCamera && !PC->PlayerCamera->bDeleteMe)
		{
			PC->PlayerCamera->bEnableFading = false;
			PC->PlayerCamera->FadeAmount = 0.f;
			// if the player is remote, ensure they got it
			// this handles cases where the LDs stream out this level immediately afterwards,
			// which can mean the client never gets the matinee replication if it was temporarily unresponsive
			if (!PC->IsLocalPlayerController())
			{
				PC->eventClientSetCameraFade(FALSE);
			}
		}
	}

	Super::TermTrackInst( Track );
}

/*-----------------------------------------------------------------------------
	UInterpTrackSlomo
-----------------------------------------------------------------------------*/

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackSlomo::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	INT NewKeyIndex = FloatTrack.AddPoint( Time, 1.0f );
	FloatTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** Change the value of an existing keyframe. */
void UInterpTrackSlomo::UpdateKeyframe(INT KeyIndex, UInterpTrackInst* TrInst)
{
	// Do nothing here - slomo is all set up through curve editor.
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the editor, when scrubbing/previewing etc.
*/
void UInterpTrackSlomo::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	// Do nothing - in the editor Matinee itself handles updating the editor viewports.
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the game, when USeqAct_Interp is ticked
*  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
*/
void UInterpTrackSlomo::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	// do nothing if we're the client, as the server will replicate TimeDilation
	if (CastChecked<UInterpTrackInstSlomo>(TrInst)->ShouldBeApplied())
	{
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		WorldInfo->TimeDilation = GetSlomoFactorAtTime(NewPosition);
		WorldInfo->bNetDirty = TRUE;
		WorldInfo->bForceNetUpdate = TRUE;
	}
}

/** Return the slomo factor we want at the given time. */
FLOAT UInterpTrackSlomo::GetSlomoFactorAtTime(FLOAT Time)
{
	FLOAT Slomo = FloatTrack.Eval(Time, 0.f);
	Slomo = ::Max(Slomo, 0.1f);
	return Slomo;
}

/** Set the slomo track to a default of one key at time zero and a slomo factor of 1.0 (ie no slomo) */
void UInterpTrackSlomo::SetTrackToSensibleDefault()
{
	FloatTrack.Points.Empty();
	FloatTrack.AddPoint(0.f, 1.f);
}	

/*-----------------------------------------------------------------------------
	UInterpTrackInstSlomo
-----------------------------------------------------------------------------*/

/** In editor, backup the LevelInfo->TimeDilation when opening Matinee. */
void UInterpTrackInstSlomo::SaveActorState(UInterpTrack* Track)
{
	OldTimeDilation = GWorld->GetWorldInfo()->TimeDilation;
}

/** In the editor, when we exit Matinee, restore levels TimeDilation to the backed-up value. */
void UInterpTrackInstSlomo::RestoreActorState(UInterpTrack* Track)
{
	GWorld->GetWorldInfo()->TimeDilation = OldTimeDilation;
}

UBOOL UInterpTrackInstSlomo::ShouldBeApplied()
{
	if (GIsEditor)
	{
		return TRUE;
	}
	else if (GWorld->GetWorldInfo()->NetMode == NM_Client)
	{
		return FALSE;
	}
	else
	{
		// if GroupActor is NULL, then this is the instance created on a dedicated server when no players were around
		// otherwise, check that GroupActor is the first player
		AActor* GroupActor = GetGroupActor();
		return (GroupActor == NULL || (GEngine != NULL && GEngine->GamePlayers.Num() > 0 && GEngine->GamePlayers(0) != NULL && GEngine->GamePlayers(0)->Actor == GroupActor));
	}
}

/** Remember the slomo factor applied when interpolation begins. */
void UInterpTrackInstSlomo::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	// do nothing if we're the client, as the server will replicate TimeDilation
	if (ShouldBeApplied())
	{
		OldTimeDilation = GWorld->GetWorldInfo()->TimeDilation;
	}
}

/** Ensure the slomo factor is restored to what it was when interpolation begins. */
void UInterpTrackInstSlomo::TermTrackInst(UInterpTrack* Track)
{
	// do nothing if we're the client, as the server will replicate TimeDilation
	if (ShouldBeApplied())
	{
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		if(OldTimeDilation <= 0.f)
		{
			warnf(TEXT("WARNING! OldTimeDilation was not initialized in %s!  Setting to 1.0f"),*GetPathName());
			OldTimeDilation = 1.0f;
		}
		WorldInfo->TimeDilation = OldTimeDilation;
		WorldInfo->bNetDirty = TRUE;
		WorldInfo->bForceNetUpdate = TRUE;
	}

	Super::TermTrackInst( Track );
}

/*-----------------------------------------------------------------------------
	UInterpTrackAnimControl
-----------------------------------------------------------------------------*/

void UInterpTrackAnimControl::PostLoad()
{
	Super::PostLoad();

	// Fix any anims with zero play rate.
	for(INT i=0; i<AnimSeqs.Num(); i++)
	{
		if(AnimSeqs(i).AnimPlayRate < 0.001f)
		{
			AnimSeqs(i).AnimPlayRate = 1.f;
		}
	}
}

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackAnimControl, AnimSeqs)
STRUCTTRACK_GETTIMERANGE(UInterpTrackAnimControl, AnimSeqs, StartTime)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackAnimControl, AnimSeqs, StartTime)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackAnimControl, AnimSeqs, StartTime, FAnimControlTrackKey )
STRUCTTRACK_REMOVEKEYFRAME( UInterpTrackAnimControl, AnimSeqs )
STRUCTTRACK_DUPLICATEKEYFRAME( UInterpTrackAnimControl, AnimSeqs, StartTime, FAnimControlTrackKey )

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackAnimControl::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	FAnimControlTrackKey NewSeq;
	NewSeq.AnimSeqName = NAME_None;
	NewSeq.bLooping = false;
	NewSeq.AnimStartOffset = 0.f;
	NewSeq.AnimEndOffset = 0.f;
	NewSeq.AnimPlayRate = 1.f;
	NewSeq.StartTime = Time;
	NewSeq.bReverse = FALSE;

	// Find the correct index to insert this cut.
	INT i=0; for( i=0; i<AnimSeqs.Num() && AnimSeqs(i).StartTime < Time; i++);
	AnimSeqs.Insert(i);
	AnimSeqs(i) = NewSeq;

	// FIXME: set weight to be 1 for AI group by default (curve editor value to be 1)

	return i;
}

/** Return the closest time to the time passed in that we might want to snap to. */
UBOOL UInterpTrackAnimControl::GetClosestSnapPosition(FLOAT InPosition, TArray<INT> &IgnoreKeys, FLOAT& OutPosition)
{
	if(AnimSeqs.Num() == 0)
	{
		return false;
	}

	UBOOL bFoundSnap = false;
	FLOAT ClosestSnap = 0.f;
	FLOAT ClosestDist = BIG_NUMBER;
	for(INT i=0; i<AnimSeqs.Num(); i++)
	{
		if(!IgnoreKeys.ContainsItem(i))
		{
			FLOAT SeqStartTime = AnimSeqs(i).StartTime;
			FLOAT SeqEndTime = SeqStartTime;

			FLOAT SeqLength = 0.f;
			UAnimSequence* Seq = FindAnimSequenceFromName(AnimSeqs(i).AnimSeqName);
			if(Seq)
			{
				SeqLength = ::Max((Seq->SequenceLength - (AnimSeqs(i).AnimStartOffset + AnimSeqs(i).AnimEndOffset)) / AnimSeqs(i).AnimPlayRate, 0.01f);
				SeqEndTime += SeqLength;
			}

			// If there is a sequence following this one - we stop drawing this block where the next one begins.
			if((i < AnimSeqs.Num()-1) && !IgnoreKeys.ContainsItem(i+1))
			{
				SeqEndTime = ::Min( AnimSeqs(i+1).StartTime, SeqEndTime );
			}

			FLOAT Dist = Abs( SeqStartTime - InPosition );
			if(Dist < ClosestDist)
			{
				ClosestSnap = SeqStartTime;
				ClosestDist = Dist;
				bFoundSnap = true;
			}

			Dist = Abs( SeqEndTime - InPosition );
			if(Dist < ClosestDist)
			{
				ClosestSnap = SeqEndTime;
				ClosestDist = Dist;
				bFoundSnap = true;
			}
		}
	}

	OutPosition = ClosestSnap;
	return bFoundSnap;
}

/** Return color to draw each keyframe in Matinee. */
FColor UInterpTrackAnimControl::GetKeyframeColor(INT KeyIndex) const
{
	return FColor(0,0,0);
}

/**
 * @return	The ending time of the track. 
 */
FLOAT UInterpTrackAnimControl::GetTrackEndTime() const
{
	FLOAT EndTime = 0.0f;

	if( AnimSeqs.Num() )
	{
		// Since the keys are sorted in chronological order, choose the 
		// last anim key on the track to find the track end time.
		const FAnimControlTrackKey& AnimKey = AnimSeqs( AnimSeqs.Num()-1 );

		// The end time should be no less than the 
		// timeline position of the anim key.
		EndTime = AnimKey.StartTime;

		// If there is a valid anim sequence, add the total time of the 
		// anim, accounting for factors, such as: offsets and play rate. 
		const UAnimSequence* AnimSequence = FindAnimSequenceFromName( AnimKey.AnimSeqName );
		if( AnimSequence )
		{
			// When calculating the end time, we do not consider the AnimStartOffset since we 
			// are not calculating the length of the anim key. We just want the time where it ends.
			EndTime += ::Max( (AnimSequence->SequenceLength - AnimKey.AnimEndOffset) / AnimKey.AnimPlayRate, 0.01f );
		}

	}

	return EndTime;
}

/** Find the AnimSequence with the given name in the set of AnimSets defined in this AnimControl track. */
UAnimSequence* UInterpTrackAnimControl::FindAnimSequenceFromName(FName InName)
{
	if(InName == NAME_None)
	{
		return NULL;
	}

	UInterpGroup* Group = CastChecked<UInterpGroup>(GetOuter());

	// Work from last element in list backwards, so you can replace a specific sequence by adding a set later in the array.
	for(INT i=Group->GroupAnimSets.Num()-1; i>=0; i--)
	{
		if( Group->GroupAnimSets(i) )
		{
			UAnimSequence* FoundSeq = Group->GroupAnimSets(i)->FindAnimSequence(InName);
			if(FoundSeq)
			{
				return FoundSeq;
			}
		}
	}

	return NULL;	
}

/** Find the AnimSequence with the given name in the set of AnimSets defined in this AnimControl track. */
const UAnimSequence* UInterpTrackAnimControl::FindAnimSequenceFromName(FName InName) const
{
	if(InName == NAME_None)
	{
		return NULL;
	}

	UInterpGroup* Group = CastChecked<UInterpGroup>(GetOuter());

	// Work from last element in list backwards, so you can replace a specific sequence by adding a set later in the array.
	for(INT i=Group->GroupAnimSets.Num()-1; i>=0; i--)
	{
		if( Group->GroupAnimSets(i) )
		{
			UAnimSequence* FoundSeq = Group->GroupAnimSets(i)->FindAnimSequence(InName);
			if(FoundSeq)
			{
				return FoundSeq;
			}
		}
	}

	return NULL;	
}
/** Find the animation name and position for the given point in the track timeline. 
 *  @return TRUE if it needs the animation to advance timer (from Previous to Current Time for Root Motion)
 */
UBOOL UInterpTrackAnimControl::GetAnimForTime(FLOAT InTime, FName& OutAnimSeqName, FLOAT& OutPosition, UBOOL& bOutLooping)
{
	UBOOL bResetTime = FALSE;

	if(AnimSeqs.Num() == 0)
	{
		OutAnimSeqName = NAME_None;
		OutPosition = 0.f;
	}
	else
	{
		if(InTime < AnimSeqs(0).StartTime)
		{
			OutAnimSeqName = AnimSeqs(0).AnimSeqName;
			OutPosition = AnimSeqs(0).AnimStartOffset;
			// Reverse position if the key is set to be reversed.
			if(AnimSeqs(0).bReverse)
			{
				UAnimSequence *Seq = FindAnimSequenceFromName(AnimSeqs(0).AnimSeqName);

				if(Seq)
				{
					OutPosition = ConditionallyReversePosition(AnimSeqs(0), Seq, OutPosition);
				}

				bOutLooping = AnimSeqs(0).bLooping;
			}

			// animation didn't start yet
			bResetTime = TRUE; 
		}
		else
		{
			INT i=0; for( i=0; i<AnimSeqs.Num()-1 && AnimSeqs(i+1).StartTime <= InTime; i++);

			OutAnimSeqName = AnimSeqs(i).AnimSeqName;
			OutPosition = ((InTime - AnimSeqs(i).StartTime) * AnimSeqs(i).AnimPlayRate);

			UAnimSequence *Seq = FindAnimSequenceFromName(AnimSeqs(i).AnimSeqName);
			if(Seq)
			{
				FLOAT SeqLength = ::Max(Seq->SequenceLength - (AnimSeqs(i).AnimStartOffset + AnimSeqs(i).AnimEndOffset), 0.01f);

				if(AnimSeqs(i).bLooping)
				{
					OutPosition = appFmod(OutPosition, SeqLength);
					OutPosition += AnimSeqs(i).AnimStartOffset;
				}
				else
				{
					OutPosition = ::Clamp(OutPosition + AnimSeqs(i).AnimStartOffset, 0.f, (Seq->SequenceLength - AnimSeqs(i).AnimEndOffset) + (FLOAT)KINDA_SMALL_NUMBER);
				}

				// Reverse position if the key is set to be reversed.
				if(AnimSeqs(i).bReverse)
				{
					OutPosition = ConditionallyReversePosition(AnimSeqs(i), Seq, OutPosition);
					bResetTime = (OutPosition == (Seq->SequenceLength - AnimSeqs(i).AnimEndOffset));
				}
				else
				{
					bResetTime = (OutPosition == AnimSeqs(i).AnimStartOffset);
				}

				bOutLooping = AnimSeqs(i).bLooping;
			}
		}
	}

	return bResetTime;
}

/** Get the strength that the animation from this track should be blended in with at the give time. */
FLOAT UInterpTrackAnimControl::GetWeightForTime(FLOAT InTime)
{
	return FloatTrack.Eval(InTime, 0.f);
}


/** 
 * Calculates the reversed time for a sequence key.
 *
 * @param SeqKey		Key that is reveresed and we are trying to find a position for.
 * @param Seq			Anim sequence the key represents.
 * @param InPosition	Timeline position that we are currently at.
 *
 * @return Returns the position in the specified seq key. 
 */
FLOAT UInterpTrackAnimControl::ConditionallyReversePosition(FAnimControlTrackKey &SeqKey, UAnimSequence* Seq, FLOAT InPosition)
{
	FLOAT Result = InPosition;

	// Reverse position if the key is set to be reversed.
	if(SeqKey.bReverse)
	{
		if(Seq == NULL)
		{
			Seq = FindAnimSequenceFromName(SeqKey.AnimSeqName);
		}

		// Reverse the clip.
		if(Seq)
		{
			const FLOAT RealLength = Seq->SequenceLength - (SeqKey.AnimStartOffset+SeqKey.AnimEndOffset);
			Result = (RealLength - (InPosition-SeqKey.AnimStartOffset))+SeqKey.AnimStartOffset;	// Mirror the cropped clip.
		}
	}

	return Result;
}

/** Update the Actors animation state, in the editor. */
void UInterpTrackAnimControl::PreviewUpdateTrack(FLOAT NewPosition, class UInterpTrackInst* TrInst)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	UInterpTrackInstAnimControl* AnimInst = CastChecked<UInterpTrackInstAnimControl>(TrInst);

	// Calculate this channels index within the named slot.
	INT ChannelIndex = CalcChannelIndex();

	FName NewAnimSeqName;
	FLOAT NewAnimPosition, TimeElapsed = 0.f;
	UBOOL bNewLooping;
	UBOOL bResetTime = GetAnimForTime(NewPosition, NewAnimSeqName, NewAnimPosition, bNewLooping);

	if(NewAnimSeqName != NAME_None)
	{
		if ( bEnableRootMotion )
		{
			// only save detaltime if root motion is enabled... Otherwise, no reason to do it. 
			TimeElapsed = NewPosition-AnimInst->LastUpdatePosition;
			debugf(TEXT("AnimTrack(EnableRootMotion) : %f"), TimeElapsed);
#if WITH_EDITORONLY_DATA
			// when root motion is enabled, precision error can kill the movement
			// so reset it whenever at the 0
			if (NewPosition == 0.f && TimeElapsed==0.f)
			{
				// then update to their original position
				Actor->SetLocation(AnimInst->InitPosition);
				Actor->SetRotation(AnimInst->InitRotation);
			}
#endif
		}

		// if we're going backward or if not @ the first frame of the animation
		UBOOL bFireNotifier = !bSkipAnimNotifiers && (TimeElapsed < 0.f || !bResetTime) ;
		Actor->PreviewSetAnimPosition(SlotName, ChannelIndex, NewAnimSeqName, NewAnimPosition, bNewLooping, bFireNotifier, bEnableRootMotion, TimeElapsed);
		AnimInst->LastUpdatePosition = NewPosition;
	}
}

/** Update the Actors animation state, in the game. */
void UInterpTrackAnimControl::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	//debugf(TEXT("UInterpTrackAnimControl::UpdateTrack, NewPosition: %3.2f"), NewPosition);

	UInterpTrackInstAnimControl* AnimInst = CastChecked<UInterpTrackInstAnimControl>(TrInst);

	// Calculate this channels index within the named slot.
	INT ChannelIndex = CalcChannelIndex();

	// Don't do complicated stuff for notifies if playing backwards, or not moving at all.
	if(AnimSeqs.Num() == 0 || NewPosition <= AnimInst->LastUpdatePosition || bJump)
	{
		FName NewAnimSeqName;
		FLOAT NewAnimPosition;
		UBOOL bNewLooping;
		GetAnimForTime(NewPosition, NewAnimSeqName, NewAnimPosition, bNewLooping);

		if( NewAnimSeqName != NAME_None )
		{
			Actor->eventSetAnimPosition(SlotName, ChannelIndex, NewAnimSeqName, NewAnimPosition, FALSE, bNewLooping, bEnableRootMotion);
		}
	}
	// Playing forwards - need to do painful notify stuff.
	else
	{
		// Find which anim we are starting in. -1 Means before first anim.
		INT StartSeqIndex = -1; 
		for( StartSeqIndex = -1; StartSeqIndex<AnimSeqs.Num()-1 && AnimSeqs(StartSeqIndex+1).StartTime <= AnimInst->LastUpdatePosition; StartSeqIndex++);

		// Find which anim we are ending in. -1 Means before first anim.
		INT EndSeqIndex = -1; 
		for( EndSeqIndex = -1; EndSeqIndex<AnimSeqs.Num()-1 && AnimSeqs(EndSeqIndex+1).StartTime <= NewPosition; EndSeqIndex++);

		// Now start walking from the first block.
		INT CurrentSeqIndex = StartSeqIndex;
		while(CurrentSeqIndex <= EndSeqIndex)
		{
			// If we are before the first anim - do nothing but set ourselves to the beginning of the first anim.
			if(CurrentSeqIndex == -1)
			{
				FAnimControlTrackKey &SeqKey = AnimSeqs(0);
				FLOAT Position = SeqKey.AnimStartOffset;

				// Reverse position if the key is set to be reversed.
				if( SeqKey.bReverse )
				{
					Position = ConditionallyReversePosition(SeqKey, NULL, Position);
				}

				if( SeqKey.AnimSeqName != NAME_None )
				{
					Actor->eventSetAnimPosition(SlotName, ChannelIndex, SeqKey.AnimSeqName, Position, FALSE, SeqKey.bLooping, bEnableRootMotion);
				}
			}
			// If we are within an anim.
			else
			{
				// Find the name and starting time
				FAnimControlTrackKey &AnimSeq = AnimSeqs(CurrentSeqIndex);
				FName CurrentAnimName = AnimSeq.AnimSeqName;
				FLOAT CurrentSeqStart = AnimSeq.StartTime;
				FLOAT CurrentStartOffset = AnimSeq.AnimStartOffset;
				FLOAT CurrentEndOffset = AnimSeq.AnimEndOffset;
				FLOAT CurrentRate = AnimSeq.AnimPlayRate;

				// Find the time we are currently at.
				// If this is the first start anim - its the 'current' position of the Matinee.
				FLOAT FromTime = (CurrentSeqIndex == StartSeqIndex) ? AnimInst->LastUpdatePosition : CurrentSeqStart;

				// Find the time we want to move to.
				// If this is the last anim - its the 'new' position of the Matinee. Otherwise, its the start of the next anim.
				// Safe to address AnimSeqs at CurrentSeqIndex+1 in the second case, as it must be <EndSeqIndex and EndSeqIndex<AnimSeqs.Num().
				FLOAT ToTime = (CurrentSeqIndex == EndSeqIndex) ? NewPosition : AnimSeqs(CurrentSeqIndex+1).StartTime; 
				
				// If looping, we need to play through the sequence multiple times, to ensure notifies are execute correctly.
				if( AnimSeq.bLooping )
				{
					UAnimSequence* Seq = FindAnimSequenceFromName(CurrentAnimName);
					if(Seq)
					{
						// Find position we should not play beyond in this sequence.
						FLOAT SeqEnd = (Seq->SequenceLength - CurrentEndOffset);

						// Find time this sequence will take to play
						FLOAT SeqLength = ::Max(Seq->SequenceLength - (CurrentStartOffset + CurrentEndOffset), 0.01f);

						// Find the number of loops we make. 
						// @todo: This will need to be updated if we decide to support notifies in reverse.
						if(AnimSeq.bReverse == FALSE)
						{
							INT FromLoopNum = appFloor( (((FromTime - CurrentSeqStart) * CurrentRate) + CurrentStartOffset)/SeqLength );
							INT ToLoopNum = appFloor( (((ToTime - CurrentSeqStart) * CurrentRate) + CurrentStartOffset)/SeqLength );
							INT NumLoopsToJump = ToLoopNum - FromLoopNum;

							for(INT i=0; i<NumLoopsToJump; i++)
							{
								Actor->eventSetAnimPosition(SlotName, ChannelIndex, CurrentAnimName, SeqEnd + KINDA_SMALL_NUMBER, TRUE, TRUE, bEnableRootMotion);
								Actor->eventSetAnimPosition(SlotName, ChannelIndex, CurrentAnimName, CurrentStartOffset, FALSE, TRUE, bEnableRootMotion);
							}
						}

						FLOAT AnimPos = appFmod((ToTime - CurrentSeqStart) * CurrentRate, SeqLength) + CurrentStartOffset;

						// Reverse position if the key is set to be reversed.
						if( AnimSeq.bReverse )
						{
							AnimPos = ConditionallyReversePosition(AnimSeq, Seq, AnimPos);
						}

						if( CurrentAnimName != NAME_None )
						{
							Actor->eventSetAnimPosition(SlotName, ChannelIndex, CurrentAnimName, AnimPos, !bSkipAnimNotifiers, TRUE, bEnableRootMotion);
						}
					}
					// If we failed to find the sequence, just use simpler method.
					else
					{
						if( CurrentAnimName != NAME_None )
						{
							Actor->eventSetAnimPosition(SlotName, ChannelIndex, CurrentAnimName, ((ToTime - CurrentSeqStart) * CurrentRate) + CurrentStartOffset, !bSkipAnimNotifiers, TRUE, bEnableRootMotion);
						}
					}
				}
				// No looping or reversed - its easy - wind to desired time.
				else
				{
					FLOAT AnimPos = ((ToTime - CurrentSeqStart) * CurrentRate) + CurrentStartOffset;

					UAnimSequence* Seq = FindAnimSequenceFromName(CurrentAnimName);
					if( Seq )
					{
						FLOAT SeqEnd = (Seq->SequenceLength - CurrentEndOffset);
						AnimPos = ::Clamp( AnimPos, 0.f, SeqEnd + (FLOAT)KINDA_SMALL_NUMBER );
					}

					// Conditionally reverse the position.
					AnimPos = ConditionallyReversePosition(AnimSeq, Seq, AnimPos);

					if( CurrentAnimName != NAME_None )
					{
						// if Current Animation Position == StartOffset, that means we clear all PreviousTime and new Time, 
						// jump there - bFireNotifier == FALSE will clear PreviousTime and CurrentTime to match
						Actor->eventSetAnimPosition(SlotName, ChannelIndex, CurrentAnimName, AnimPos, (AnimPos != CurrentStartOffset)?!bSkipAnimNotifiers:FALSE , FALSE, bEnableRootMotion);
					}
				}

				// If we are not yet at target anim, set position at start of next anim.
				if( CurrentSeqIndex < EndSeqIndex )
				{
					FAnimControlTrackKey &SeqKey = AnimSeqs(CurrentSeqIndex+1);
					FLOAT Position = SeqKey.AnimStartOffset;

					// Conditionally reverse the position.
					if( SeqKey.bReverse )
					{
						Position = ConditionallyReversePosition(SeqKey, NULL, Position);
					}

					if( SeqKey.AnimSeqName != NAME_None )
					{
						Actor->eventSetAnimPosition(SlotName, ChannelIndex, SeqKey.AnimSeqName, Position, FALSE, SeqKey.bLooping, bEnableRootMotion);
					}
				}
			}

			// Move on the CurrentSeqIndex counter.
			CurrentSeqIndex++;
		}
	}

	// Now remember the location we are at this frame, to use as the 'From' time next frame.
	AnimInst->LastUpdatePosition = NewPosition;
}

/** 
 *	Utility to split the animation we are currently over into two pieces at the current position. 
 *	InPosition is position in the entire Matinee sequence.
 *	Returns the index of the newly created key.
 */
INT UInterpTrackAnimControl::SplitKeyAtPosition(FLOAT InPosition)
{
	// Check we are over a valid animation
	INT SplitSeqIndex = -1; 
	for( SplitSeqIndex = -1; SplitSeqIndex < AnimSeqs.Num()-1 && AnimSeqs(SplitSeqIndex+1).StartTime <= InPosition; SplitSeqIndex++ );
	if(SplitSeqIndex == -1)
	{
		return INDEX_NONE;
	}

	// Check the sequence is valid.
	FAnimControlTrackKey& SplitKey = AnimSeqs(SplitSeqIndex);
	UAnimSequence* Seq = FindAnimSequenceFromName(SplitKey.AnimSeqName);
	if(!Seq)
	{
		return INDEX_NONE;
	}

	// Check we are over an actual chunk of sequence.
	FLOAT SplitAnimPos = ((InPosition - SplitKey.StartTime) * SplitKey.AnimPlayRate) + SplitKey.AnimStartOffset;
	if(SplitAnimPos <= SplitKey.AnimStartOffset || SplitAnimPos >= (Seq->SequenceLength - SplitKey.AnimEndOffset))
	{
		return INDEX_NONE;
	}

	// Create new Key.
	FAnimControlTrackKey NewKey;
	NewKey.AnimPlayRate = SplitKey.AnimPlayRate;
	NewKey.AnimSeqName = SplitKey.AnimSeqName;
	NewKey.StartTime = InPosition;
	NewKey.bLooping = SplitKey.bLooping;
	NewKey.AnimStartOffset = SplitAnimPos; // Start position in the new animation wants to be the place we are currently at.
	NewKey.AnimEndOffset = SplitKey.AnimEndOffset; // End place is the same as the one we are splitting.

	SplitKey.AnimEndOffset = Seq->SequenceLength - SplitAnimPos; // New end position is where we are.
	SplitKey.bLooping = false; // Disable looping for section before the cut.

	// Add new key to track.
	AnimSeqs.InsertZeroed(SplitSeqIndex+1);
	AnimSeqs(SplitSeqIndex+1) = NewKey;

	return SplitSeqIndex+1;
}

/**
 * Crops the key at the position specified, by deleting the area of the key before or after the position.
 *
 * @param InPosition				Position to use as a crop point.
 * @param bCutAreaBeforePosition	Whether we should delete the area of the key before the position specified or after.
 *
 * @return Returns the index of the key that was cropped.
 */
INT UInterpTrackAnimControl::CropKeyAtPosition(FLOAT InPosition, UBOOL bCutAreaBeforePosition)
{
	// Check we are over a valid animation
	INT SplitSeqIndex = -1; 
	for( SplitSeqIndex = -1; SplitSeqIndex < AnimSeqs.Num()-1 && AnimSeqs(SplitSeqIndex+1).StartTime <= InPosition; SplitSeqIndex++ );
	if(SplitSeqIndex == -1)
	{
		return INDEX_NONE;
	}

	// Check the sequence is valid.
	FAnimControlTrackKey& SplitKey = AnimSeqs(SplitSeqIndex);
	UAnimSequence* Seq = FindAnimSequenceFromName(SplitKey.AnimSeqName);
	if(!Seq)
	{
		return INDEX_NONE;
	}

	// Check we are over an actual chunk of sequence.
	FLOAT SplitAnimPos = ((InPosition - SplitKey.StartTime) * SplitKey.AnimPlayRate) + SplitKey.AnimStartOffset;
	if(SplitAnimPos <= SplitKey.AnimStartOffset || SplitAnimPos >= (Seq->SequenceLength - SplitKey.AnimEndOffset))
	{
		return INDEX_NONE;
	}

	// Crop either left or right depending on which way the user wants to crop.
	if(bCutAreaBeforePosition)
	{
		SplitKey.StartTime = InPosition;
		SplitKey.AnimStartOffset = SplitAnimPos; // New end position is where we are.
	}
	else
	{
		SplitKey.AnimEndOffset = Seq->SequenceLength - SplitAnimPos; // New end position is where we are.
	}

	return SplitSeqIndex;
}

/** Calculate the index of this Track within its Slot (for when multiple tracks are using same slot). */
INT UInterpTrackAnimControl::CalcChannelIndex()
{
	UInterpGroup* Group = CastChecked<UInterpGroup>(GetOuter());

	// Count number of tracks with same slot name before we reach this one
	INT ChannelIndex = 0;
	for(INT i=0; i<Group->InterpTracks.Num(); i++)
	{
		UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Group->InterpTracks(i));

		// If we have reached this track, return current ChannelIndex
		if(AnimTrack == this)
		{
			return ChannelIndex;
		}

		// If not this track, but has same slot name, increment ChannelIndex
		if(AnimTrack && !AnimTrack->IsDisabled() && AnimTrack->SlotName == SlotName)
		{
			ChannelIndex++;
		}
	}

	check(FALSE && "AnimControl Track Not Found In It's Group!"); // Should not reach here!
	return 0;
}

/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
* @return	String name of the helper class.*/
const FString UInterpTrackAnimControl::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackAnimControlHelper") );
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstAnimControl
-----------------------------------------------------------------------------*/

/** Initialise this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstAnimControl::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );

	LastUpdatePosition = Seq->Position;

#if WITH_EDITORONLY_DATA
	AActor * GrActor = GetGroupActor();
	if (GrActor)
	{
		InitPosition = GrActor->Location;
		InitRotation = GrActor->Rotation;
	}
#endif
}

/** Terminate this Track instance. Called in-game after interpolation. */
void UInterpTrackInstAnimControl::TermTrackInst(UInterpTrack* Track)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
	UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Track);
	AActor * Actor = GrInst->GetGroupActor();

	// This is to return to previous root motion mode if it was set
	if ( Actor && AnimTrack && AnimTrack->bEnableRootMotion )
	{
		USkeletalMeshComponent * SkeletalMeshComponent = NULL;
		BYTE DefaultRootMotionMode = RMM_Ignore;
		BYTE DefaultRootRotationMode = RMRM_Ignore;

		// for pawn, find default root motion mode
		if ( Actor->IsA(APawn::StaticClass()) )
		{
			APawn * Pawn = CastChecked<APawn>(Actor);
			if ( Pawn && Pawn->Mesh )
			{
				SkeletalMeshComponent = Pawn->Mesh;
				DefaultRootMotionMode = CastChecked<APawn>(Pawn->GetClass()->GetDefaultActor())->Mesh->RootMotionMode;
				DefaultRootRotationMode = CastChecked<APawn>(Pawn->GetClass()->GetDefaultActor())->Mesh->RootMotionRotationMode;				
			}
		}
		// for SkeletalMeshActor, by default it will be RMM_Ignore
		else if ( Actor->IsA(ASkeletalMeshActor::StaticClass()) )
		{
			ASkeletalMeshActor * SMA = CastChecked<ASkeletalMeshActor>(Actor);
			if ( SMA && SMA->SkeletalMeshComponent )
			{
				// just ignore after it's done
				SkeletalMeshComponent = SMA->SkeletalMeshComponent;
				DefaultRootMotionMode = RMM_Ignore;
				DefaultRootRotationMode = RMRM_Ignore;
			}
		}

		// skeletalmesh component
		if ( SkeletalMeshComponent )
		{
			// Go back to previous root motion
			SkeletalMeshComponent->RootMotionMode = DefaultRootMotionMode;
			SkeletalMeshComponent->RootMotionRotationMode = DefaultRootRotationMode; 
		}
	}

	Super::TermTrackInst( Track );
}

/*-----------------------------------------------------------------------------
	UInterpTrackSound
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackSound, Sounds)
STRUCTTRACK_GETTIMERANGE(UInterpTrackSound, Sounds, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackSound, Sounds, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackSound, Sounds, Time, FSoundTrackKey )
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackSound, Sounds )
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackSound, Sounds, Time, FSoundTrackKey )

/** Set this track to sensible default values. Called when track is first created. */
void UInterpTrackSound::SetTrackToSensibleDefault()
{
	VectorTrack.Points.Empty();

	const FLOAT DefaultSoundKeyVolume = 1.0f;
	const FLOAT DefaultSoundKeyPitch = 1.0f;

	VectorTrack.AddPoint( 0.0f, FVector( DefaultSoundKeyVolume, DefaultSoundKeyPitch, 1.f ) );
}

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackSound::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	FSoundTrackKey NewSound;
	NewSound.Sound = NULL;
	NewSound.Time = Time;
	NewSound.Volume = 1.0f;
	NewSound.Pitch = 1.0f;

	// Find the correct index to insert this cut.
	INT i=0; for( i=0; i<Sounds.Num() && Sounds(i).Time < Time; i++);
	Sounds.Insert(i);
	Sounds(i) = NewSound;

	return i;
}

/** Return the closest time to the time passed in that we might want to snap to. */
UBOOL UInterpTrackSound::GetClosestSnapPosition(FLOAT InPosition, TArray<INT> &IgnoreKeys, FLOAT& OutPosition)
{
	if(Sounds.Num() == 0)
	{
		return false;
	}

	UBOOL bFoundSnap = false;
	FLOAT ClosestSnap = 0.f;
	FLOAT ClosestDist = BIG_NUMBER;
	for(INT i=0; i<Sounds.Num(); i++)
	{
		if(!IgnoreKeys.ContainsItem(i))
		{
			FLOAT SoundStartTime = Sounds(i).Time;
			FLOAT SoundEndTime = SoundStartTime;

			// Make block as long as the SoundCue is.
			USoundCue* Cue = Sounds(i).Sound;
			if(Cue)
			{
				SoundEndTime += Cue->GetCueDuration();
			}

			// Truncate sound cue at next sound in the track.
			if((i < Sounds.Num()-1) && !IgnoreKeys.ContainsItem(i+1))
			{
				SoundEndTime = ::Min( Sounds(i+1).Time, SoundEndTime );
			}

			FLOAT Dist = Abs( SoundStartTime - InPosition );
			if(Dist < ClosestDist)
			{
				ClosestSnap = SoundStartTime;
				ClosestDist = Dist;
				bFoundSnap = true;
			}

			Dist = Abs( SoundEndTime - InPosition );
			if(Dist < ClosestDist)
			{
				ClosestSnap = SoundEndTime;
				ClosestDist = Dist;
				bFoundSnap = true;
			}
		}
	}

	OutPosition = ClosestSnap;
	return bFoundSnap;
}

/**
 * @return	The ending time of the track. 
 */
FLOAT UInterpTrackSound::GetTrackEndTime() const
{
	FLOAT EndTime = 0.0f; 

	if( Sounds.Num() )
	{
		const FSoundTrackKey& SoundKey = Sounds( Sounds.Num()-1 );
		EndTime = SoundKey.Time + SoundKey.Sound->Duration;
	}

	return EndTime;
}

void UInterpTrackSound::PostLoad()
{
	Super::PostLoad();
	if (VectorTrack.Points.Num() <= 0)
	{
		SetTrackToSensibleDefault();
	}
}

/**
 * Returns the key at the specified position in the track.
 */
FSoundTrackKey& UInterpTrackSound::GetSoundTrackKeyAtPosition(FLOAT InPosition)
{
	INT SoundIndex; 
	if (bPlayOnReverse)
	{
		for (SoundIndex = Sounds.Num(); SoundIndex > 0 && Sounds(SoundIndex - 1).Time > InPosition; SoundIndex--);
		if (SoundIndex == Sounds.Num())
		{
			SoundIndex = Sounds.Num() - 1;
		}
	}
	else
	{
		for (SoundIndex = -1; SoundIndex<Sounds.Num()-1 && Sounds(SoundIndex+1).Time < InPosition; SoundIndex++);
		if (SoundIndex == -1)
		{
			SoundIndex = 0;
		}
	}
	return Sounds(SoundIndex);
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the game, when USeqAct_Interp is ticked
*  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
*/
void UInterpTrackSound::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	if (Sounds.Num() <= 0)
	{
		//debugf(NAME_Warning,TEXT("No sounds for sound track %s"),*GetName());
		return;
	}

	UInterpGroup* Group = CastChecked<UInterpGroup>(GetOuter());
	UInterpTrackInstSound* SoundInst = CastChecked<UInterpTrackInstSound>(TrInst);
	AActor* Actor = TrInst->GetGroupActor();


	// If this is a director group and the associated actor is a player controller, we need to make sure that it's
	// the local client's player controller and not some other client's player.  In the case where we're a host
	// with other connected players, we don't want the audio to be played for each of the players -- once is fine!
	UBOOL bIsOkayToPlaySound = TRUE;
	if( Group->IsA( UInterpGroupDirector::StaticClass() ) && Actor != NULL )
	{
		APlayerController* PC = Cast< APlayerController >( Actor );
		if( PC != NULL )
		{
			USeqAct_Interp* Interp = CastChecked<USeqAct_Interp>(TrInst->GetOuter()->GetOuter());
			if (!PC->IsLocalPlayerController() || (Interp->PreferredSplitScreenNum == 0 && GEngine->GamePlayers.FindItemIndex(Cast<ULocalPlayer>(PC->Player)) > 0))
			{
				// The director track is trying to play audio for a non-local client's player, or a player beyond the first
				// of a splitscreen matinee that plays for all players.  This is probably not
				// what was intended, so we don't allow it!  This will be played only by the local player's
				// audio track instance.
				bIsOkayToPlaySound = FALSE;
			}
		}
	}

    
	// Only play sounds if we are playing Matinee forwards, we're not hopping around in time, and we're allowed to
	// play the sound.
	FVector VolumePitchValue( 1.0f, 1.0f, 1.0f );
	if ((bPlayOnReverse ? (NewPosition < SoundInst->LastUpdatePosition) : (NewPosition > SoundInst->LastUpdatePosition)) && !bJump && bIsOkayToPlaySound)
	{
		// Find which sound we are starting in. -1 Means before first sound.
		INT StartSoundIndex = -1; 
		// Find which sound we are ending in. -1 Means before first sound.
		INT EndSoundIndex = -1; 

		if (bPlayOnReverse)
		{
			for (StartSoundIndex = Sounds.Num(); StartSoundIndex > 0 && Sounds(StartSoundIndex - 1).Time > SoundInst->LastUpdatePosition; StartSoundIndex--);
			for (EndSoundIndex = Sounds.Num(); EndSoundIndex > 0 && Sounds(EndSoundIndex - 1).Time > NewPosition; EndSoundIndex--);
		}
		else
		{
			for (StartSoundIndex = -1; StartSoundIndex<Sounds.Num()-1 && Sounds(StartSoundIndex+1).Time < SoundInst->LastUpdatePosition; StartSoundIndex++);
			for (EndSoundIndex = -1; EndSoundIndex<Sounds.Num()-1 && Sounds(EndSoundIndex+1).Time < NewPosition; EndSoundIndex++);
		}

		//////
		FSoundTrackKey& SoundTrackKey = GetSoundTrackKeyAtPosition(NewPosition);
		VolumePitchValue *= FVector( SoundTrackKey.Volume, SoundTrackKey.Pitch, 1.0f );
		if (VectorTrack.Points.Num() > 0)
		{
			VolumePitchValue *= VectorTrack.Eval(NewPosition,VolumePitchValue);
		}

		// If we have moved into a new sound, we should start playing it now.
		if(StartSoundIndex != EndSoundIndex)
		{
			USoundCue* NewCue = SoundTrackKey.Sound;

			IInterface_Speaker* Speaker = NULL;
			if (bTreatAsDialogue && Actor)
			{
				Speaker = InterfaceCast<IInterface_Speaker>(Actor);
				if (Speaker == NULL)
				{
					// if we have a controller, see if it's controlling a speaker
					AController* const C = Actor->GetAController();
					Speaker = C ? InterfaceCast<IInterface_Speaker>(C->Pawn) : NULL;
				}
			}
			
			if (Speaker)
			{
				Speaker->eventSpeak(NewCue);
			}
			else if (!bTreatAsDialogue || !Actor) // Don't play at all if we had a dialogue actor but they are not available/dead now
			{
				// If we have a sound playing already (ie. an AudioComponent exists) stop it now.
				if(SoundInst->PlayAudioComp)
				{
					SoundInst->PlayAudioComp->Stop();
					SoundInst->PlayAudioComp->SetSoundCue(NewCue);
					SoundInst->PlayAudioComp->VolumeMultiplier = VolumePitchValue.X;
					SoundInst->PlayAudioComp->PitchMultiplier = VolumePitchValue.Y;
					SoundInst->PlayAudioComp->Play();
				}
				else
				{
					// If there is no AudioComponent - create one now.
					SoundInst->PlayAudioComp = UAudioDevice::CreateComponent(NewCue, GWorld->Scene, Actor, FALSE, FALSE);
					if(SoundInst->PlayAudioComp)
					{
						// If we have no actor to attach sound to - its location is meaningless, so we turn off spatialization.
						// Also if we are playing on a director group, disable spatialization.
						if(!Actor || Group->IsA(UInterpGroupDirector::StaticClass()))
						{
							SoundInst->PlayAudioComp->bAllowSpatialization = FALSE;
						}

						// Start the sound playing.
						SoundInst->PlayAudioComp->Play();				
					}
				}
			}
		}
	}
	// If Matinee is not being played forward, we're hopping around in time, or we're not allowed to
	// play the sound, then stop any already playing sounds
	else if( SoundInst->PlayAudioComp && SoundInst->PlayAudioComp->IsPlaying() )
	{
		FSoundTrackKey& SoundTrackKey = GetSoundTrackKeyAtPosition(NewPosition);
		USoundCue* NewCue = SoundTrackKey.Sound;
		if (NewCue != SoundInst->PlayAudioComp->SoundCue)
		{
			SoundInst->PlayAudioComp->Stop();
		}
	}


	// Apply master volume and pitch scale
	{
		UInterpData* IData = CastChecked<UInterpData>( Group->GetOuter() );
		UInterpGroupDirector* DirGroup = IData->FindDirectorGroup();
		if( DirGroup != NULL )
		{
			UInterpTrackAudioMaster* AudioMasterTrack = DirGroup->GetAudioMasterTrack();
			if( AudioMasterTrack != NULL )
			{
				VolumePitchValue.X *= AudioMasterTrack->GetVolumeScaleForTime( NewPosition );
				VolumePitchValue.Y *= AudioMasterTrack->GetPitchScaleForTime( NewPosition );
			}
		}
	}


	// Update the sound if its playing
	if (SoundInst->PlayAudioComp)
	{
		SoundInst->PlayAudioComp->VolumeMultiplier = VolumePitchValue.X;
		SoundInst->PlayAudioComp->PitchMultiplier = VolumePitchValue.Y;
		SoundInst->PlayAudioComp->SubtitlePriority = bSuppressSubtitles ? 0.f : SUBTITLE_PRIORITY_MATINEE;
	}

	// Finally update the current position as the last one.
	SoundInst->LastUpdatePosition = NewPosition;
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the editor, when scrubbing/previewing etc.
*/
void UInterpTrackSound::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
	UInterpTrackInstSound* SoundInst = CastChecked<UInterpTrackInstSound>( TrInst );
	UInterpGroup* Group = CastChecked<UInterpGroup>( GetOuter() );
	UInterpData* IData = CastChecked<UInterpData>( Group->GetOuter() );

	// If the new position for the track is past the end of the interp length, then the sound
	// should stop, unless the user has specified to continue playing the sound past matinee's end
	if ( NewPosition >= IData->InterpLength && !bContinueSoundOnMatineeEnd && SoundInst->PlayAudioComp && SoundInst->PlayAudioComp->IsPlaying() )
	{
		SoundInst->PlayAudioComp->Stop();
	}

	// Dont play sounds unless we are preview playback (ie not scrubbing).
	UBOOL bJump = !(Seq->bIsPlaying);
	UpdateTrack(NewPosition, TrInst, bJump);
}

/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
* @return	String name of the helper class.*/
const FString	UInterpTrackSound::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackSoundHelper") );
}

/** Stop sound playing when you press stop in Matinee. */
void UInterpTrackSound::PreviewStopPlayback(class UInterpTrackInst* TrInst)
{
	UInterpTrackInstSound* SoundTrInst = CastChecked<UInterpTrackInstSound>(TrInst);
	if(SoundTrInst->PlayAudioComp)
	{
		SoundTrInst->PlayAudioComp->Stop();
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstSound
-----------------------------------------------------------------------------*/

/** Initialise this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstSound::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
	LastUpdatePosition = Seq->Position;
}

/** Called when interpolation is done. Should not do anything else with this TrackInst after this. */
void UInterpTrackInstSound::TermTrackInst(UInterpTrack* Track)
{
	UInterpTrackSound* SoundTrack = CastChecked<UInterpTrackSound>(Track);

	// If we still have an audio component - deal with it.
	if(PlayAudioComp)
	{
		UBOOL bCompIsPlaying = (PlayAudioComp->bWasPlaying && !PlayAudioComp->bFinished);

		// If we are currently playing, and want to keep the sound playing, 
		// just flag it as 'auto destroy', and it will destroy itself when it finishes.
		if(bCompIsPlaying && SoundTrack->bContinueSoundOnMatineeEnd)
		{
			PlayAudioComp->bAutoDestroy = true;
			PlayAudioComp = NULL;
		}
		else
		{
			PlayAudioComp->Stop();
			PlayAudioComp->DetachFromAny();
			PlayAudioComp = NULL;
		}
	}

	Super::TermTrackInst( Track );
}

/*-----------------------------------------------------------------------------
	UInterpTrackFloatParticleParam
-----------------------------------------------------------------------------*/

/** Add a new keyframe at the specified time. Returns index of new keyframe. */
INT UInterpTrackFloatParticleParam::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	INT NewKeyIndex = FloatTrack.AddPoint( Time, 0.f );
	FloatTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the editor, when scrubbing/previewing etc.
*/
void UInterpTrackFloatParticleParam::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, FALSE);
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the game, when USeqAct_Interp is ticked
*  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
*/
void UInterpTrackFloatParticleParam::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	AEmitter* Emitter = Cast<AEmitter>(Actor);
	if(!Emitter)
	{
		return;
	}

	FLOAT NewFloatValue = FloatTrack.Eval(NewPosition, 0.f);
	Emitter->ParticleSystemComponent->SetFloatParameter( ParamName, NewFloatValue );
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstFloatParticleParam
-----------------------------------------------------------------------------*/

/** Called before Interp editing to put object back to its original state. */
void UInterpTrackInstFloatParticleParam::SaveActorState(UInterpTrack* Track)
{
	UInterpTrackFloatParticleParam* ParamTrack = CastChecked<UInterpTrackFloatParticleParam>(Track);
	AActor* Actor = GetGroupActor();
	AEmitter* Emitter = Cast<AEmitter>(Actor);
	if(!Emitter)
	{
		return;
	}

	UBOOL bFoundParam = Emitter->ParticleSystemComponent->GetFloatParameter( ParamTrack->ParamName, ResetFloat );
	if(!bFoundParam)
	{
		ResetFloat = 0.f;
	}
}

/** Restore the saved state of this Actor. */
void UInterpTrackInstFloatParticleParam::RestoreActorState(UInterpTrack* Track)
{
	UInterpTrackFloatParticleParam* ParamTrack = CastChecked<UInterpTrackFloatParticleParam>(Track);
	AActor* Actor = GetGroupActor();
	AEmitter* Emitter = Cast<AEmitter>(Actor);
	if(!Emitter)
	{
		return;
	}

	Emitter->ParticleSystemComponent->SetFloatParameter( ParamTrack->ParamName, ResetFloat );
}

/** helper for PreSave() of the material tracks, since there's no material track base class */
static void PreSaveMaterialParamTrack(UInterpTrack* Track, TArray<FMaterialReferenceList>& Materials)
{
	ULevel* MyLevel = Track->GetTypedOuter<ULevel>();
	UInterpData* OwnerData = Track->GetTypedOuter<UInterpData>();

	// first gather all known materials that are being touched through object variables in Kismet
	if (MyLevel != NULL)
	{
		USequence* BaseSequence = MyLevel->GetGameSequence();
		if (BaseSequence != NULL)
		{
			UInterpGroup* MyGroup = Track->GetTypedOuter<UInterpGroup>();

			TArray<USequenceObject*> Actions;
			BaseSequence->FindSeqObjectsByClass(USeqAct_Interp::StaticClass(), Actions, TRUE);
			for (INT i = 0; i < Actions.Num(); i++)
			{
				USeqAct_Interp* InterpAction = Cast<USeqAct_Interp>(Actions(i));
				if (InterpAction != NULL && InterpAction->FindInterpDataFromVariable() == OwnerData)
				{
					TArray<UObject**> ObjectVars;
					InterpAction->GetObjectVars(ObjectVars, *MyGroup->GroupName.ToString());
					InterpAction->GetNamedObjVars(ObjectVars, *MyGroup->GroupName.ToString());
					for (INT j = 0; j < ObjectVars.Num(); j++)
					{
						AMaterialInstanceActor* MatActor = Cast<AMaterialInstanceActor>(*ObjectVars(j));
						if (MatActor != NULL && MatActor->MatInst != NULL)
						{
							UBOOL bFound = FALSE;
							for (INT k = 0; k < Materials.Num(); k++)
							{
								if (MatActor->MatInst == Materials(k).TargetMaterial)
								{
									bFound = TRUE;
									break;
								}
							}
							if (!bFound)
							{
								INT Index = Materials.AddZeroed();
								Materials(Index).TargetMaterial = MatActor->MatInst;
							}
						}
					}
				}
			}
		}
	}

	// now find the relevant objects that use those materials
	for (INT i = 0; i < Materials.Num(); i++)
	{
		Materials(i).AffectedMaterialRefs.Reset();
		Materials(i).AffectedPPChainMaterialRefs.Reset();
		if (!Track->IsTemplate() && MyLevel != NULL && OwnerData != NULL)
		{
			MyLevel->GetMaterialRefs(Materials(i));
		}
	}
}
/** helper for InitTrackInst() of the material tracks, since there's no material track base class */
static void InitMaterialParamTrackInst(UInterpTrack* Track, TArray<FMaterialReferenceList>& Materials, AActor* Actor, UBOOL bNeedsMaterialRefsUpdate)
{
	// use attached MaterialInstanceActor to grab the material, if specified
	AMaterialInstanceActor* MatInstActor = Cast<AMaterialInstanceActor>(Actor);
	if (MatInstActor != NULL && MatInstActor->MatInst != NULL)
	{
		UBOOL bFound = FALSE;
		for (INT i = 0; i < Materials.Num(); i++)
		{
			if (Materials(i).TargetMaterial == MatInstActor->MatInst)
			{
				bFound = TRUE;
				break;
			}
		}
		if (!bFound)
		{
			//@warning: this will cause any other track instances hooked up to the same track to have a mismatched number of elements in MICInfos
			//@note: this doesn't fully support a single InterpData being hooked up to multiple Matinees which are connected to different MaterialInstanceActors
			//		(combined list of Materials will be modified by any individual Matinee)
			//		that seems to be pretty unlikely, though, so for now it doesn't appear to be worth fixing
			INT Index = Materials.AddZeroed();
			Materials(Index).TargetMaterial = MatInstActor->MatInst;

			ULevel* MyLevel = Track->GetTypedOuter<ULevel>();
			if (MyLevel != NULL)
			{
				MyLevel->GetMaterialRefs(Materials(Index));
			}
		}
	}

	// since AffectedMaterialRefs is normally filled on save, in the editor it might be out of date so do it here as well
	if (bNeedsMaterialRefsUpdate || (GIsEditor && !GIsGame))
	{
		ULevel* MyLevel = Track->GetTypedOuter<ULevel>();
		for (INT i = 0; i < Materials.Num(); i++)
		{
			Materials(i).AffectedMaterialRefs.Reset();
			Materials(i).AffectedPPChainMaterialRefs.Reset();
			if (MyLevel != NULL)
			{
				MyLevel->GetMaterialRefs(Materials(i));
			}
		}
	}

// 	see if the modification of post process MICs via Matinee has been enabled
 	UBOOL EnablePostProcessMaterialParam = FALSE;
 	GConfig->GetBool( TEXT("Engine.Engine"), TEXT("EnableMatineePostProcessMaterialParam"), EnablePostProcessMaterialParam, GEngineIni );

	// since post process materials are associated with local player, we need to find the refs in game
	if (GIsGame)
	{
		ULevel* MyLevel = Track->GetTypedOuter<ULevel>();
		for (INT i = 0; i < Materials.Num(); i++)
		{
			Materials(i).AffectedPPChainMaterialRefs.Reset();
			if (MyLevel != NULL)
			{
				MyLevel->GetMaterialRefs(Materials(i), TRUE);
			}
		}
	}
}

/** helper for PreEditChange() of the material tracks, since there's no material track base class */
static void PreEditChangeMaterialParamTrack()
{
	if (GIsEditor && !GIsGame)
	{
		// we need to reinitialize all material parameter tracks in the Matinee being edited so that changes are applied immediately
		// and so that Materials array modifications properly add/remove any instanced MICs from affected meshes
		// we can't reinit just the edited track because all active tracks that modify the same base Material share the instanced MIC
		for (TObjectIterator<UInterpTrackInstFloatMaterialParam> It; It; ++It)
		{
			UInterpGroupInst* GrInst = Cast<UInterpGroupInst>(It->GetOuter());
			if (GrInst != NULL && GrInst->TrackInst.ContainsItem(*It))
			{
				USeqAct_Interp* InterpAct = Cast<USeqAct_Interp>(GrInst->GetOuter());
				if (InterpAct != NULL && InterpAct->bIsBeingEdited && InterpAct->GroupInst.ContainsItem(GrInst))
				{
					It->RestoreActorState(It->InstancedTrack);
					It->TermTrackInst(It->InstancedTrack);
				}
			}
		}
		for (TObjectIterator<UInterpTrackInstVectorMaterialParam> It; It; ++It)
		{
			UInterpGroupInst* GrInst = Cast<UInterpGroupInst>(It->GetOuter());
			if (GrInst != NULL && GrInst->TrackInst.ContainsItem(*It))
			{
				USeqAct_Interp* InterpAct = Cast<USeqAct_Interp>(GrInst->GetOuter());
				if (InterpAct != NULL && InterpAct->bIsBeingEdited && InterpAct->GroupInst.ContainsItem(GrInst))
				{
					It->RestoreActorState(It->InstancedTrack);
					It->TermTrackInst(It->InstancedTrack);
				}
			}
		}
	}
}
/** helper for PostEditChange() of the material tracks, since there's no material track base class */
static void PostEditChangeMaterialParamTrack()
{
	if (GIsEditor && !GIsGame)
	{
		// we need to reinitialize all material parameter tracks so that changes are applied immediately
		// and so that Materials array modifications properly add/remove any instanced MICs from affected meshes
		// we can't reinit just the edited track because all active tracks that modify the same base Material share the instanced MIC
		for (TObjectIterator<UInterpTrackInstFloatMaterialParam> It; It; ++It)
		{
			UInterpGroupInst* GrInst = Cast<UInterpGroupInst>(It->GetOuter());
			if (GrInst != NULL && GrInst->TrackInst.ContainsItem(*It))
			{
				USeqAct_Interp* InterpAct = Cast<USeqAct_Interp>(GrInst->GetOuter());
				if (InterpAct != NULL && InterpAct->bIsBeingEdited && InterpAct->GroupInst.ContainsItem(GrInst))
				{
					It->InitTrackInst(It->InstancedTrack);
					It->SaveActorState(It->InstancedTrack);
					It->InstancedTrack->PreviewUpdateTrack(InterpAct->Position, *It);
				}
			}
		}
		for (TObjectIterator<UInterpTrackInstVectorMaterialParam> It; It; ++It)
		{
			UInterpGroupInst* GrInst = Cast<UInterpGroupInst>(It->GetOuter());
			if (GrInst != NULL && GrInst->TrackInst.ContainsItem(*It))
			{
				USeqAct_Interp* InterpAct = Cast<USeqAct_Interp>(GrInst->GetOuter());
				if (InterpAct != NULL && InterpAct->bIsBeingEdited && InterpAct->GroupInst.ContainsItem(GrInst))
				{
					It->InitTrackInst(It->InstancedTrack);
					It->SaveActorState(It->InstancedTrack);
					It->InstancedTrack->PreviewUpdateTrack(InterpAct->Position, *It);
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackFloatMaterialParam
-----------------------------------------------------------------------------*/

void UInterpTrackFloatMaterialParam::PreSave()
{
	Super::PreSave();

#if WITH_EDITORONLY_DATA
	PreSaveMaterialParamTrack(this, Materials);
#endif // WITH_EDITORONLY_DATA
}

void UInterpTrackFloatMaterialParam::PostLoad()
{
	Super::PostLoad();
	//@compatibility: update deprecated single Material property
	if (Material_DEPRECATED != NULL)
	{
		INT Index = Materials.AddZeroed();
		Materials(Index).TargetMaterial = Material_DEPRECATED;
	}
	//@compatibility: format of MaterialReferenceList has changed

	if (GetLinker() != NULL && GetLinker()->Ver() < VER_CHANGED_MATPARAMTRACK_MATERIAL_REFERENCES && !IsTemplate())
	{
		bNeedsMaterialRefsUpdate = TRUE;
	}
}

void UInterpTrackFloatMaterialParam::PreEditChange(UProperty* PropertyThatWillChange)
{
	PreEditChangeMaterialParamTrack();

	Super::PreEditChange(PropertyThatWillChange);
}

void UInterpTrackFloatMaterialParam::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ULevel* MyLevel = GetTypedOuter<ULevel>();
	UInterpData* OwnerData = GetTypedOuter<UInterpData>();
	for (INT i = 0; i < Materials.Num(); i++)
	{
		Materials(i).AffectedMaterialRefs.Reset();
		Materials(i).AffectedPPChainMaterialRefs.Reset();
		if (!IsTemplate() && MyLevel != NULL && OwnerData != NULL)
		{
			MyLevel->GetMaterialRefs(Materials(i));
		}
	}

	PostEditChangeMaterialParamTrack();
}

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackFloatMaterialParam::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	INT NewKeyIndex = FloatTrack.AddPoint( Time, 0.f );
	FloatTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the editor, when scrubbing/previewing etc.
*/
void UInterpTrackFloatMaterialParam::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, FALSE);
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the game, when USeqAct_Interp is ticked
*  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
*/
void UInterpTrackFloatMaterialParam::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	FLOAT NewFloatValue = FloatTrack.Eval(NewPosition, 0.f);
	UInterpTrackInstFloatMaterialParam* ParamTrackInst = Cast<UInterpTrackInstFloatMaterialParam>(TrInst);
	if (ParamTrackInst != NULL)
	{
		for (INT i = 0; i < ParamTrackInst->MICInfos.Num(); i++)
		{
			for (INT j = 0; j < ParamTrackInst->MICInfos(i).MICs.Num(); j++)
			{
				if (ParamTrackInst->MICInfos(i).MICs(j) != NULL)
				{
					ParamTrackInst->MICInfos(i).MICs(j)->SetScalarParameterValue(ParamName, NewFloatValue);
				}
			}
		}
	}
}

void UInterpTrackFloatMaterialParam::PostDuplicate()
{
	for (INT MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex )
	{
		Materials(MaterialIndex).AffectedMaterialRefs.Reset();
		Materials(MaterialIndex).AffectedPPChainMaterialRefs.Reset();
	}

	bNeedsMaterialRefsUpdate = TRUE;
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstFloatMaterialParam
-----------------------------------------------------------------------------*/

/**
 * Saves off the current float of the given parameter if one 
 * exists on the MIC. Otherwise, a zero float is saved.
 *
 * @param	ParameterName	The name of the MIC float parameter.
 * @param	MICInfo			The MIC data containing the reset float to set.
 * @param	MICIndex		The index of the MIC from the given MIC data for accessing the param float. 
 */
void SaveResetFloatForMIC( const FName& ParameterName, FFloatMaterialParamMICData& MICInfo, INT MICIndex )
{
	if (MICInfo.MICs(MICIndex) == NULL || !MICInfo.MICs(MICIndex)->GetScalarParameterValue(ParameterName, MICInfo.MICResetFloats(MICIndex)))
	{
		MICInfo.MICResetFloats(MICIndex) = 0.f;
	}
}

void UInterpTrackInstFloatMaterialParam::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	UInterpTrackFloatMaterialParam* ParamTrack = Cast<UInterpTrackFloatMaterialParam>(Track);
	if (ParamTrack != NULL)
	{
		if (GIsEditor && !GIsGame)
		{
			// remember track so we can be reinitialized if the track's material info changes
			InstancedTrack = ParamTrack;
		}

		InitMaterialParamTrackInst(ParamTrack, ParamTrack->Materials, GetGroupActor(), ParamTrack->bNeedsMaterialRefsUpdate);
		ParamTrack->bNeedsMaterialRefsUpdate = FALSE;
			
		MICInfos.Reset();
		MICInfos.AddZeroed(ParamTrack->Materials.Num());
		for (INT i = 0; i < ParamTrack->Materials.Num(); i++)
		{
			FMaterialReferenceList& CurrentMaterial = ParamTrack->Materials(i);
			MICInfos(i).MICs.AddZeroed(CurrentMaterial.AffectedMaterialRefs.Num() + CurrentMaterial.AffectedPPChainMaterialRefs.Num());
			MICInfos(i).MICResetFloats.AddZeroed(CurrentMaterial.AffectedMaterialRefs.Num() + CurrentMaterial.AffectedPPChainMaterialRefs.Num());
			for (INT j = 0; j < CurrentMaterial.AffectedMaterialRefs.Num(); j++)
			{
				if (CurrentMaterial.AffectedMaterialRefs(j).Primitive != NULL)
				{
					UMaterialInterface* Material = CurrentMaterial.AffectedMaterialRefs(j).Primitive->GetElementMaterial(CurrentMaterial.AffectedMaterialRefs(j).MaterialIndex);
					if (Material != NULL)
					{
						// if the material is already a runtime-generated MIC, don't make the chain larger
						//NOTE - the check for standalone is to catch new material/MICs that have not been saved yet.  Otherwise the removal of TermTrackInst function will set the meshes material to the base material
						if (Material->GetNetIndex() == INDEX_NONE && !Material->HasAnyFlags(RF_Standalone) && Material->IsA(UMaterialInstanceConstant::StaticClass()))
						{
							MICInfos(i).MICs(j) = (UMaterialInstanceConstant*)Material;
						}
						else
						{
							MICInfos(i).MICs(j) = ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass(), this);
							MICInfos(i).MICs(j)->SetParent(Material);
							CurrentMaterial.AffectedMaterialRefs(j).Primitive->SetElementMaterial(CurrentMaterial.AffectedMaterialRefs(j).MaterialIndex, MICInfos(i).MICs(j));
						}
					}

					SaveResetFloatForMIC( ParamTrack->ParamName, MICInfos(i), j );
				}
			}
			// evaluate the post process MIC refs... these refs should only exist in game
			INT ArrayOffset = CurrentMaterial.AffectedMaterialRefs.Num();
			for (INT k = 0; k < CurrentMaterial.AffectedPPChainMaterialRefs.Num(); k++)
			{
				if (CurrentMaterial.AffectedPPChainMaterialRefs(k).Effect != NULL)
				{
					UMaterialInterface* Material = CurrentMaterial.AffectedPPChainMaterialRefs(k).Effect->Material;
					if (Material != NULL)
					{
						// if the material is already a runtime-generated MIC, don't make the chain larger
						//NOTE - the check for standalone is to catch new material/MICs that have not been saved yet.  Otherwise the removal of TermTrackInst function will set the meshes material to the base material
						if (Material->GetNetIndex() == INDEX_NONE && !Material->HasAnyFlags(RF_Standalone) && Material->IsA(UMaterialInstanceConstant::StaticClass()))
						{
							MICInfos(i).MICs(ArrayOffset + k) = (UMaterialInstanceConstant*)Material;
						}
						else
						{
							MICInfos(i).MICs(ArrayOffset + k) = ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass(), this);
							MICInfos(i).MICs(ArrayOffset + k)->SetParent(Material);
							CurrentMaterial.AffectedPPChainMaterialRefs(k).Effect->Material = MICInfos(i).MICs(ArrayOffset + k);
						}
					}

					SaveResetFloatForMIC( ParamTrack->ParamName, MICInfos(i), ArrayOffset + k );
				}
			}
		}
	}
}

void UInterpTrackInstFloatMaterialParam::TermTrackInst(UInterpTrack* Track)
{
	// in the editor, we want to revert Actors to their original state
	// in game, leave the MIC around as Matinee changes persist when it is stopped
	if (GIsEditor && !GIsGame)
	{
		UInterpTrackFloatMaterialParam* ParamTrack = Cast<UInterpTrackFloatMaterialParam>(Track);
		if (ParamTrack != NULL)
		{
			for (INT i = 0; i < ParamTrack->Materials.Num() && i < MICInfos.Num(); i++)
			{
				for (INT j = 0; j < ParamTrack->Materials(i).AffectedMaterialRefs.Num(); j++)
				{
					if (MICInfos(i).MICs(j) != NULL && ParamTrack->Materials(i).AffectedMaterialRefs(j).Primitive != NULL)
					{
						ParamTrack->Materials(i).AffectedMaterialRefs(j).Primitive->SetElementMaterial(ParamTrack->Materials(i).AffectedMaterialRefs(j).MaterialIndex, MICInfos(i).MICs(j)->Parent);
					}
				}
			}
			for (INT i = 0; i < ParamTrack->Materials.Num() && i < MICInfos.Num(); i++)
			{
				INT ArrayOffset = ParamTrack->Materials(i).AffectedMaterialRefs.Num();
				for (INT j = 0; j < ParamTrack->Materials(i).AffectedPPChainMaterialRefs.Num(); j++)
				{
					if (MICInfos(i).MICs(ArrayOffset + j) != NULL && ParamTrack->Materials(i).AffectedPPChainMaterialRefs(j).Effect != NULL)
					{
						ParamTrack->Materials(i).AffectedPPChainMaterialRefs(j).Effect->Material = MICInfos(i).MICs(ArrayOffset + j)->Parent;
					}
				}
			}
		}
	}
	MICInfos.Empty();

	Super::TermTrackInst( Track );
}

/** Called before Interp editing to put object back to its original state. */
void UInterpTrackInstFloatMaterialParam::SaveActorState(UInterpTrack* Track)
{
	UInterpTrackFloatMaterialParam* ParamTrack = Cast<UInterpTrackFloatMaterialParam>(Track);
	if (ParamTrack != NULL)
	{
		for (INT i = 0; i < ParamTrack->Materials.Num() && i < MICInfos.Num(); i++)
		{
			MICInfos(i).MICResetFloats.Reset();
			MICInfos(i).MICResetFloats.Add(MICInfos(i).MICs.Num());
			for (INT j = 0; j < MICInfos(i).MICs.Num(); j++)
			{
				SaveResetFloatForMIC( ParamTrack->ParamName, MICInfos(i), j );
			}
		}
	}
}

/** Restore the saved state of this Actor. */
void UInterpTrackInstFloatMaterialParam::RestoreActorState(UInterpTrack* Track)
{
	UInterpTrackFloatMaterialParam* ParamTrack = Cast<UInterpTrackFloatMaterialParam>(Track);
	if (ParamTrack != NULL)
	{
		for (INT i = 0; i < ParamTrack->Materials.Num() && i < MICInfos.Num(); i++)
		{
			for (INT j = 0; j < MICInfos(i).MICs.Num(); j++)
			{
				if (MICInfos(i).MICs(j) != NULL)
				{
					check (MICInfos(i).MICResetFloats.Num() > j);
					MICInfos(i).MICs(j)->SetScalarParameterValue(ParamTrack->ParamName, MICInfos(i).MICResetFloats(j));
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackVectorMaterialParam
-----------------------------------------------------------------------------*/

void UInterpTrackVectorMaterialParam::PreSave()
{
	Super::PreSave();

#if WITH_EDITORONLY_DATA
	PreSaveMaterialParamTrack(this, Materials);
#endif // WITH_EDITORONLY_DATA
}

void UInterpTrackVectorMaterialParam::PostLoad()
{
	Super::PostLoad();
	//@compatibility: update deprecated single Material property
	if (Material_DEPRECATED != NULL)
	{
		INT Index = Materials.AddZeroed();
		Materials(Index).TargetMaterial = Material_DEPRECATED;
	}
	//@compatibility: format of MaterialReferenceList has changed

	if (GetLinker() != NULL && GetLinker()->Ver() < VER_CHANGED_MATPARAMTRACK_MATERIAL_REFERENCES && !IsTemplate())
	{
		bNeedsMaterialRefsUpdate = TRUE;
	}
}

void UInterpTrackVectorMaterialParam::PreEditChange(UProperty* PropertyThatWillChange)
{
	PreEditChangeMaterialParamTrack();

	Super::PreEditChange(PropertyThatWillChange);
}

void UInterpTrackVectorMaterialParam::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ULevel* MyLevel = GetTypedOuter<ULevel>();
	UInterpData* OwnerData = GetTypedOuter<UInterpData>();
	for (INT i = 0; i < Materials.Num(); i++)
	{
		Materials(i).AffectedMaterialRefs.Reset();
		Materials(i).AffectedPPChainMaterialRefs.Reset();
		if (!IsTemplate() && MyLevel != NULL && OwnerData != NULL)
		{
			MyLevel->GetMaterialRefs(Materials(i));
		}
	}

	PostEditChangeMaterialParamTrack();
}

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackVectorMaterialParam::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	INT NewKeyIndex = VectorTrack.AddPoint( Time, FVector(0.f) );
	VectorTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

	VectorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the editor, when scrubbing/previewing etc.
*/
void UInterpTrackVectorMaterialParam::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, FALSE);
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the game, when USeqAct_Interp is ticked
*  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
*/
void UInterpTrackVectorMaterialParam::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	FVector NewFloatValue = VectorTrack.Eval(NewPosition, FVector(0.f));
	FLinearColor NewLinearColor(NewFloatValue.X, NewFloatValue.Y, NewFloatValue.Z);
	UInterpTrackInstVectorMaterialParam* ParamTrackInst = Cast<UInterpTrackInstVectorMaterialParam>(TrInst);
	if (ParamTrackInst != NULL)
	{
		for (INT i = 0; i < ParamTrackInst->MICInfos.Num(); i++)
		{
			for (INT j = 0; j < ParamTrackInst->MICInfos(i).MICs.Num(); j++)
			{
				if (ParamTrackInst->MICInfos(i).MICs(j) != NULL)
				{
					ParamTrackInst->MICInfos(i).MICs(j)->SetVectorParameterValue(ParamName, NewLinearColor);
				}
			}
		}
	}
}

void UInterpTrackVectorMaterialParam::PostDuplicate()
{
	for (INT MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex )
	{
		Materials(MaterialIndex).AffectedMaterialRefs.Reset();
		Materials(MaterialIndex).AffectedPPChainMaterialRefs.Reset();
	}

	bNeedsMaterialRefsUpdate = TRUE;
}



/*-----------------------------------------------------------------------------
	UInterpTrackInstToggle
-----------------------------------------------------------------------------*/


/** Initialise this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstToggle::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	// Skip ahead if we shouldn't be firing any events
	UInterpTrackToggle* ToggleTrack = CastChecked<UInterpTrackToggle>( Track );
	if( !ToggleTrack->bFireEventsWhenJumpingForwards )
	{
		UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
		USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
		
		LastUpdatePosition = Seq->Position;
	}
}



/** Called before Interp editing to put object back to its original state. */
void UInterpTrackInstToggle::SaveActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();

	AEmitter* EmitterActor = Cast<AEmitter>(Actor);
	ALensFlareSource* LensFlareActor = Cast<ALensFlareSource>(Actor);
	ALight* LightActor = Cast<ALight>(Actor);

	bSavedActiveState = FALSE;

	if( EmitterActor )
	{
		bSavedActiveState = EmitterActor->bCurrentlyActive;
	}
	else if( LensFlareActor && LensFlareActor->LensFlareComp )
	{
		bSavedActiveState = LensFlareActor->LensFlareComp->bIsActive;
	}
	else if( LightActor != NULL )
	{
		bSavedActiveState = LightActor->LightComponent->bEnabled;
	}
}



/** Restore the saved state of this Actor. */
void UInterpTrackInstToggle::RestoreActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();

	AEmitter* EmitterActor = Cast<AEmitter>(Actor);
	ALensFlareSource* LensFlareActor = Cast<ALensFlareSource>(Actor);
	ALight* LightActor = Cast<ALight>(Actor);

	if( EmitterActor )
	{
		if( bSavedActiveState )
		{
			// Use SetActive to only activate a non-active system...
			EmitterActor->ParticleSystemComponent->SetActive(TRUE);
			EmitterActor->bCurrentlyActive = TRUE;
			EmitterActor->bNetDirty = TRUE;
			EmitterActor->eventForceNetRelevant();
		}
		else
		{
			EmitterActor->ParticleSystemComponent->SetActive(FALSE);
			EmitterActor->bCurrentlyActive = FALSE;
			EmitterActor->bNetDirty = TRUE;
			EmitterActor->eventForceNetRelevant();
		}
	}
	else if( LensFlareActor && LensFlareActor->LensFlareComp )
	{
		LensFlareActor->LensFlareComp->SetIsActive( bSavedActiveState );
	}
	else if( LightActor != NULL )
	{
		// We'll only allow *toggleable* lights to be toggled like this!  Static lights are ignored.
		if( LightActor->IsToggleable() )
		{
			LightActor->LightComponent->SetEnabled( bSavedActiveState );
		}
	}
}



/*-----------------------------------------------------------------------------
	UInterpTrackInstVectorMaterialParam
-----------------------------------------------------------------------------*/

/**
 * Saves off the current vector of the given parameter if one 
 * exists on the MIC. Otherwise, a zero vector is saved.
 *
 * @param	ParameterName	The name of the MIC vector parameter.
 * @param	MICInfo			The MIC data containing the reset vector to set.
 * @param	MICIndex		The index of the MIC from the given MIC data for accessing the param vector. 
 */
void SaveResetVectorForMIC( const FName& ParameterName, FVectorMaterialParamMICData& MICInfo, INT MICIndex )
{
	FLinearColor ResetLinearColor;
	if (MICInfo.MICs(MICIndex) == NULL || !MICInfo.MICs(MICIndex)->GetVectorParameterValue(ParameterName, ResetLinearColor))
	{
		MICInfo.MICResetVectors(MICIndex) = FVector(0.0f, 0.0f, 0.0f);
	}
	else
	{
		MICInfo.MICResetVectors(MICIndex) = FVector(ResetLinearColor.R, ResetLinearColor.G, ResetLinearColor.B);
	}
}

void UInterpTrackInstVectorMaterialParam::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	UInterpTrackVectorMaterialParam* ParamTrack = Cast<UInterpTrackVectorMaterialParam>(Track);
	if (ParamTrack != NULL)
	{
		if (GIsEditor && !GIsGame)
		{
			// remember track so we can be reinitialized if the track's material info changes
			InstancedTrack = ParamTrack;
		}

		InitMaterialParamTrackInst(ParamTrack, ParamTrack->Materials, GetGroupActor(), ParamTrack->bNeedsMaterialRefsUpdate);
		ParamTrack->bNeedsMaterialRefsUpdate = FALSE;
			
		MICInfos.Reset();
		MICInfos.AddZeroed(ParamTrack->Materials.Num());
		for (INT i = 0; i < ParamTrack->Materials.Num(); i++)
		{
			FMaterialReferenceList& CurrentMaterial = ParamTrack->Materials(i);
			MICInfos(i).MICs.AddZeroed(CurrentMaterial.AffectedMaterialRefs.Num() + CurrentMaterial.AffectedPPChainMaterialRefs.Num());
			MICInfos(i).MICResetVectors.AddZeroed(CurrentMaterial.AffectedMaterialRefs.Num() + CurrentMaterial.AffectedPPChainMaterialRefs.Num());
			for (INT j = 0; j < CurrentMaterial.AffectedMaterialRefs.Num(); j++)
			{
				if (CurrentMaterial.AffectedMaterialRefs(j).Primitive != NULL)
				{
					UMaterialInterface* Material = CurrentMaterial.AffectedMaterialRefs(j).Primitive->GetElementMaterial(CurrentMaterial.AffectedMaterialRefs(j).MaterialIndex);
					if (Material != NULL)
					{
						// if the material is already a runtime-generated MIC, don't make the chain larger
						//NOTE - the check for standalone is to catch new material/MICs that have not been saved yet.  Otherwise the removal of TermTrackInst function will set the meshes material to the base material
						if (Material->GetNetIndex() == INDEX_NONE && !Material->HasAnyFlags(RF_Standalone) && Material->IsA(UMaterialInstanceConstant::StaticClass()))
						{
							MICInfos(i).MICs(j) = (UMaterialInstanceConstant*)Material;
						}
						else
						{
							MICInfos(i).MICs(j) = ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass(), this);
							MICInfos(i).MICs(j)->SetParent(Material);
							CurrentMaterial.AffectedMaterialRefs(j).Primitive->SetElementMaterial(CurrentMaterial.AffectedMaterialRefs(j).MaterialIndex, MICInfos(i).MICs(j));
						}
					}

					SaveResetVectorForMIC( ParamTrack->ParamName, MICInfos(i), j );
				}
			}
			// evaluate the post process MIC refs... these refs should only exist in game
			INT ArrayOffset = CurrentMaterial.AffectedMaterialRefs.Num();
			for (INT k = 0; k < CurrentMaterial.AffectedPPChainMaterialRefs.Num(); k++)
			{
				if (CurrentMaterial.AffectedPPChainMaterialRefs(k).Effect != NULL)
				{
					UMaterialInterface* Material = CurrentMaterial.AffectedPPChainMaterialRefs(k).Effect->Material;
					if (Material != NULL)
					{
						// if the material is already a runtime-generated MIC, don't make the chain larger
						//NOTE - the check for standalone is to catch new material/MICs that have not been saved yet.  Otherwise the removal of TermTrackInst function will set the meshes material to the base material
						if (Material->GetNetIndex() == INDEX_NONE && !Material->HasAnyFlags(RF_Standalone) && Material->IsA(UMaterialInstanceConstant::StaticClass()))
						{
							MICInfos(i).MICs(ArrayOffset + k) = (UMaterialInstanceConstant*)Material;
						}
						else
						{
							MICInfos(i).MICs(ArrayOffset + k) = ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass(), this);
							MICInfos(i).MICs(ArrayOffset + k)->SetParent(Material);
							CurrentMaterial.AffectedPPChainMaterialRefs(k).Effect->Material = MICInfos(i).MICs(ArrayOffset + k);
						}
					}

					SaveResetVectorForMIC( ParamTrack->ParamName, MICInfos(i), ArrayOffset + k );
				}
			}
		}
	}
}

void UInterpTrackInstVectorMaterialParam::TermTrackInst(UInterpTrack* Track)
{
	// in the editor, we want to revert Actors to their original state
	// in game, leave the MIC around as Matinee changes persist when it is stopped
	if (GIsEditor && !GIsGame)
	{
		UInterpTrackVectorMaterialParam* ParamTrack = Cast<UInterpTrackVectorMaterialParam>(Track);
		if (ParamTrack != NULL)
		{
			for (INT i = 0; i < ParamTrack->Materials.Num() && i < MICInfos.Num(); i++)
			{
				for (INT j = 0; j < ParamTrack->Materials(i).AffectedMaterialRefs.Num(); j++)
				{
					if (MICInfos(i).MICs(j) != NULL && ParamTrack->Materials(i).AffectedMaterialRefs(j).Primitive != NULL)
					{
						ParamTrack->Materials(i).AffectedMaterialRefs(j).Primitive->SetElementMaterial(ParamTrack->Materials(i).AffectedMaterialRefs(j).MaterialIndex, MICInfos(i).MICs(j)->Parent);
					}
				}
			}
			for (INT i = 0; i < ParamTrack->Materials.Num() && i < MICInfos.Num(); i++)
			{
				INT ArrayOffset = ParamTrack->Materials(i).AffectedMaterialRefs.Num();
				for (INT j = 0; j < ParamTrack->Materials(i).AffectedPPChainMaterialRefs.Num(); j++)
				{
					if (MICInfos(i).MICs(ArrayOffset + j) != NULL && ParamTrack->Materials(i).AffectedPPChainMaterialRefs(j).Effect != NULL)
					{
						ParamTrack->Materials(i).AffectedPPChainMaterialRefs(j).Effect->Material = MICInfos(i).MICs(ArrayOffset + j)->Parent;
					}
				}
			}
		}
	}
	MICInfos.Empty();

	Super::TermTrackInst( Track );
}

/** Called before Interp editing to put object back to its original state. */
void UInterpTrackInstVectorMaterialParam::SaveActorState(UInterpTrack* Track)
{
	UInterpTrackVectorMaterialParam* ParamTrack = Cast<UInterpTrackVectorMaterialParam>(Track);
	if (ParamTrack != NULL)
	{
		for (INT i = 0; i < ParamTrack->Materials.Num() && i < MICInfos.Num(); i++)
		{
			MICInfos(i).MICResetVectors.Reset();
			MICInfos(i).MICResetVectors.Add(MICInfos(i).MICs.Num());
			for (INT j = 0; j < MICInfos(i).MICs.Num(); j++)
			{
				SaveResetVectorForMIC( ParamTrack->ParamName, MICInfos(i), j );
			}
		}
	}
}

/** Restore the saved state of this Actor. */
void UInterpTrackInstVectorMaterialParam::RestoreActorState(UInterpTrack* Track)
{
	UInterpTrackVectorMaterialParam* ParamTrack = Cast<UInterpTrackVectorMaterialParam>(Track);
	if (ParamTrack != NULL)
	{
		for (INT i = 0; i < ParamTrack->Materials.Num() && i < MICInfos.Num(); i++)
		{
			for (INT j = 0; j < MICInfos(i).MICs.Num(); j++)
			{
				if (MICInfos(i).MICs(j) != NULL)
				{
					check (MICInfos(i).MICResetVectors.Num() > j);
					FLinearColor ResetLinearColor(MICInfos(i).MICResetVectors(j).X, MICInfos(i).MICResetVectors(j).Y, MICInfos(i).MICResetVectors(j).Z);
					MICInfos(i).MICs(j)->SetVectorParameterValue(ParamTrack->ParamName, ResetLinearColor);
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackColorScale
-----------------------------------------------------------------------------*/

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackColorScale::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	INT NewKeyIndex = VectorTrack.AddPoint( Time, FVector(1.f,1.f,1.f) );
	VectorTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

	VectorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

/** Change the value of an existing keyframe. */
void UInterpTrackColorScale::UpdateKeyframe(INT KeyIndex, UInterpTrackInst* TrInst)
{
	
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the editor, when scrubbing/previewing etc.
*/
void UInterpTrackColorScale::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the game, when USeqAct_Interp is ticked
*  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
*/
void UInterpTrackColorScale::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );

	// Actor for a Director group should be a PlayerController.
	APlayerController* PC = Cast<APlayerController>(GrInst->GetGroupActor());
	if(PC && PC->PlayerCamera && !PC->PlayerCamera->bDeleteMe)
	{
		PC->PlayerCamera->bEnableColorScaling = TRUE;
		PC->PlayerCamera->ColorScale = GetColorScaleAtTime(NewPosition);

		// Disable the camera's "color scale interpolation" features since that would blow away our changes
		// when the camera's UpdateCamera function is called.  For the moment, we'll be the authority over color scale!
		PC->PlayerCamera->bEnableColorScaleInterp = FALSE;
	}
}

/** Return the blur alpha we want at the given time. */
FVector UInterpTrackColorScale::GetColorScaleAtTime(FLOAT Time)
{
	FVector ColorScale = VectorTrack.Eval(Time, FVector(1.f,1.f,1.f));
	return ColorScale;
}


/** Set the track to a default of one key at time zero with default blur settings. */
void UInterpTrackColorScale::SetTrackToSensibleDefault()
{
	VectorTrack.Points.Empty();
	VectorTrack.AddPoint(0.f, FVector(1.f,1.f,1.f));
}	

/*-----------------------------------------------------------------------------
	UInterpTrackInstColorScale
-----------------------------------------------------------------------------*/

/** Use this to turn off any blur changes that were applied by this track to this player controller. */
void UInterpTrackInstColorScale::TermTrackInst(UInterpTrack* Track)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
	APlayerController* PC = Cast<APlayerController>(GrInst->GroupActor);
	if(PC && PC->PlayerCamera && !PC->PlayerCamera->bDeleteMe)
	{
		PC->PlayerCamera->bEnableColorScaling = false;
		PC->PlayerCamera->ColorScale = FVector(1.f,1.f,1.f);
	}

	Super::TermTrackInst( Track );
}

/*-----------------------------------------------------------------------------
	UInterpTrackFaceFX
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackFaceFX, FaceFXSeqs)
STRUCTTRACK_GETTIMERANGE(UInterpTrackFaceFX, FaceFXSeqs, StartTime)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackFaceFX, FaceFXSeqs, StartTime)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackFaceFX, FaceFXSeqs, StartTime, FFaceFXTrackKey )
STRUCTTRACK_REMOVEKEYFRAME( UInterpTrackFaceFX, FaceFXSeqs )
STRUCTTRACK_DUPLICATEKEYFRAME( UInterpTrackFaceFX, FaceFXSeqs, StartTime, FFaceFXTrackKey )

/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
INT UInterpTrackFaceFX::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	FFaceFXTrackKey NewSeq;
	appMemzero(&NewSeq, sizeof(FFaceFXTrackKey));
	NewSeq.FaceFXGroupName = FString(TEXT(""));
	NewSeq.FaceFXSeqName = FString(TEXT(""));
	NewSeq.StartTime = Time;

	// Find the correct index to insert this sequence.
	INT i=0; for( i=0; i<FaceFXSeqs.Num() && FaceFXSeqs(i).StartTime < Time; i++);
	FaceFXSeqs.InsertZeroed(i);
	FaceFXSeqs(i) = NewSeq;

	return i;
}

/**
 * @return	The ending time of the track. 
 */
FLOAT UInterpTrackFaceFX::GetTrackEndTime() const
{
	FLOAT EndTime = 0.0f;

	if( FaceFXSeqs.Num() )
	{
		const FFaceFXTrackKey& Key = FaceFXSeqs(FaceFXSeqs.Num()-1);

#if WITH_FACEFX
		if(CachedActorFXAsset)
		{
			// Get the FxActor
			FxActor* fActor = CachedActorFXAsset->GetFxActor();

			// Find the Group by name
			FxSize GroupIndex = fActor->FindAnimGroup(TCHAR_TO_ANSI(*(Key.FaceFXGroupName)));

			if(FxInvalidIndex != GroupIndex)
			{
				FxAnimGroup& fGroup = fActor->GetAnimGroup(GroupIndex);

				// Find the animation by name
				FxSize SeqIndex = fGroup.FindAnim(TCHAR_TO_ANSI(*(Key.FaceFXSeqName)));
				if(FxInvalidIndex != SeqIndex)
				{
					const FxAnim& fAnim = fGroup.GetAnim(SeqIndex);

					// The track end time is the duration of the last Face FX animation 
					EndTime = Key.StartTime + fAnim.GetDuration();
				}
			}
		}
#endif
	}

	return EndTime;
}


void UInterpTrackFaceFX::GetSeqInfoForTime( FLOAT InTime, FString& OutGroupName, FString& OutSeqName, FLOAT& OutPosition, FLOAT& OutSeqStart, USoundCue*& OutSoundCue)
{
	// If no keys, or before first key, return no sequence.
	if(FaceFXSeqs.Num() == 0 || InTime <= FaceFXSeqs(0).StartTime)
	{
		OutGroupName = FString(TEXT(""));
		OutSeqName = FString(TEXT(""));
		OutPosition = 0.f;
		OutSeqStart = 0.f;
		OutSoundCue = NULL;
	}
	else
	{
		// Find index of sequence we are 'within' at the current time.
		INT i=0; for( i=0; i<FaceFXSeqs.Num()-1 && FaceFXSeqs(i+1).StartTime <= InTime; i++);

		OutGroupName = FaceFXSeqs(i).FaceFXGroupName;
		OutSeqName = FaceFXSeqs(i).FaceFXSeqName;
		OutSeqStart = FaceFXSeqs(i).StartTime;
		OutPosition = InTime - FaceFXSeqs(i).StartTime;

		// Grab the sound cue reference from the track instance key array
		OutSoundCue = NULL;
		if( FaceFXSoundCueKeys.IsValidIndex( i ) )
		{
			OutSoundCue = FaceFXSoundCueKeys( i ).FaceFXSoundCue;
		}
	}
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the editor, when scrubbing/previewing etc.
*/
void UInterpTrackFaceFX::PreviewUpdateTrack(FLOAT NewPosition, class UInterpTrackInst* TrInst)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	// If we are in proper playback, we use FaceFX to drive animation like in the game
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
	if(Seq->bIsPlaying)
	{
		UpdateTrack(NewPosition, TrInst, FALSE);
		Actor->PreviewUpdateFaceFX(FALSE, FString(TEXT("")), FString(TEXT("")), 0.f);
	}
	// If scrubbing - force to a particular frame
	else
	{
		FString GroupName, SeqName;
		FLOAT Position, StartPos;
		USoundCue* SoundCue = NULL;
		GetSeqInfoForTime( NewPosition, GroupName, SeqName, Position, StartPos, SoundCue );

		Actor->PreviewUpdateFaceFX(TRUE, GroupName, SeqName, Position);

		UInterpTrackInstFaceFX* FaceFXTrInst = CastChecked<UInterpTrackInstFaceFX>(TrInst);
		FaceFXTrInst->LastUpdatePosition = NewPosition;

		// If the user scrubbed, then it's technically no longer the "first update"
		FaceFXTrInst->bFirstUpdate = FALSE;
	}
}

/** 
*	Function which actually updates things based on the new position in the track. 
*  This is called in the game, when USeqAct_Interp is ticked
*  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
*/
void UInterpTrackFaceFX::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	UInterpTrackInstFaceFX* FaceFXTrInst = CastChecked<UInterpTrackInstFaceFX>(TrInst);

	// Only play facefx if we are playing Matinee forwards, and not jumping.
	if(NewPosition > FaceFXTrInst->LastUpdatePosition && !bJump)
	{
		FLOAT DummyPos, OldPos;
		FString OldGroupName, OldSeqName;
		USoundCue* OldSoundCue = NULL;
		GetSeqInfoForTime( FaceFXTrInst->LastUpdatePosition, OldGroupName, OldSeqName, DummyPos, OldPos, OldSoundCue );

		FLOAT NewPos;
		FString NewGroupName, NewSeqName;
		USoundCue* NewSoundCue = NULL;
		GetSeqInfoForTime( NewPosition, NewGroupName, NewSeqName, DummyPos, NewPos, NewSoundCue );

		// If this is the first update and we have a valid sequence name, or the sequence has changed, tell the FaceFX animation to play
		if((FaceFXTrInst->bFirstUpdate && NewSeqName != TEXT("") ) ||
			NewGroupName != OldGroupName ||
			NewSeqName != OldSeqName ||
			NewPos != OldPos ||
			NewSoundCue != OldSoundCue)
		{
			// Because we can't call script events from Matinee, if we are in matinee, use C++ version of function
			if(GIsEditor && !GWorld->HasBegunPlay())
			{
				Actor->PreviewActorPlayFaceFX(NewGroupName, NewSeqName, NewSoundCue);
			}
			else
			{
				Actor->eventPlayActorFaceFXAnim(NULL, NewGroupName, NewSeqName, NewSoundCue);
			}

			FaceFXTrInst->bFirstUpdate = FALSE;
		}
	}

	FaceFXTrInst->LastUpdatePosition = NewPosition;
}

/** Stop playing the FaceFX animation on the Actor when we hit stop in Matinee. */
void UInterpTrackFaceFX::PreviewStopPlayback(class UInterpTrackInst* TrInst)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(Actor)
	{
		Actor->PreviewActorStopFaceFX();
	}
}

const FString UInterpTrackFaceFX::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackFaceFXHelper") );
}

/** MountFaceFXAnimSets as soon as the user adds them to the array. */
void UInterpTrackFaceFX::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(CachedActorFXAsset)
	{
		for(INT i=0; i<FaceFXAnimSets.Num(); i++)
		{
			UFaceFXAnimSet* Set = FaceFXAnimSets(i);
			if(Set)
			{
				CachedActorFXAsset->MountFaceFXAnimSet(Set);
			}
		}
	}
}


/** Updates references to sound cues for all of this track's FaceFX animation keys.  Should be called at
    load time in the editor as well as whenever the track's data is changed. */
void UInterpTrackFaceFX::UpdateFaceFXSoundCueReferences( UFaceFXAsset* FaceFXAsset )
{
#if WITH_FACEFX
	using namespace OC3Ent;
	using namespace Face;

	if( FaceFXAsset != NULL )
	{
		// First, make sure that our track instance's key array is the same size as the track's key array
		if( FaceFXSoundCueKeys.Num() != FaceFXSeqs.Num() )
		{
			FaceFXSoundCueKeys.Empty( FaceFXSeqs.Num() );
			FaceFXSoundCueKeys.AddZeroed( FaceFXSeqs.Num() );

			// Make sure our changes are saved to disk
			MarkPackageDirty();
		}

		for( INT CurKeyIndex = 0; CurKeyIndex < FaceFXSeqs.Num(); ++CurKeyIndex )
		{
			FFaceFXTrackKey& CurKey = FaceFXSeqs( CurKeyIndex );
			FFaceFXSoundCueKey& SoundCueKey = FaceFXSoundCueKeys( CurKeyIndex );

			USoundCue* NewSoundCue = NULL;

			// Get the FxActor
			FxActor* fActor = FaceFXAsset->GetFxActor();

			if ( fActor )
			{
				// Find the Group by name
				FxSize GroupIndex = fActor->FindAnimGroup(TCHAR_TO_ANSI(*(CurKey.FaceFXGroupName)));
				if(FxInvalidIndex != GroupIndex)
				{
					FxAnimGroup& fGroup = fActor->GetAnimGroup(GroupIndex);

					// Find the animation by name
					FxSize SeqIndex = fGroup.FindAnim(TCHAR_TO_ANSI(*(CurKey.FaceFXSeqName)));
					if(FxInvalidIndex != SeqIndex)
					{
						const FxAnim& fAnim = fGroup.GetAnim(SeqIndex);

						FxString SoundCuePath = fAnim.GetSoundCuePath();
						if( SoundCuePath.Length() > 0 )
						{
							NewSoundCue = LoadObject<USoundCue>(NULL, ANSI_TO_TCHAR(SoundCuePath.GetData()), NULL, LOAD_NoWarn, NULL);
						}
					}
				}

			}
			// Has anything changed?
			if( SoundCueKey.FaceFXSoundCue != NewSoundCue )
			{
				SoundCueKey.FaceFXSoundCue = NewSoundCue;
				
				// Mark the package dirty so that the new sound cue reference will be resaved!
				MarkPackageDirty();
			}
		}
	}
#endif
}




/*-----------------------------------------------------------------------------
	UInterpTrackInstFaceFX
-----------------------------------------------------------------------------*/

/** Initialise this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstFaceFX::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
	UInterpTrackFaceFX* FaceFXTrack = CastChecked<UInterpTrackFaceFX>(Track);

	// Make sure all the FaceFXAnimSets that are wanted are mounted.
	AActor* Actor = GetGroupActor();
	if(Actor)
	{
		UFaceFXAsset* Asset = NULL;

		// Because we can't call script events from Matinee, if we are in matinee, use C++ version of function
		if(GIsEditor && !GWorld->HasBegunPlay())
		{
			Asset = Actor->PreviewGetActorFaceFXAsset();
		}
		else
		{
			Asset = Actor->eventGetActorFaceFXAsset();
		}

		// If we got a FaceFXAsset, mount AnimSets to it.
		if(Asset)
		{
			for(INT i=0; i<FaceFXTrack->FaceFXAnimSets.Num(); i++)
			{
				UFaceFXAnimSet* Set = FaceFXTrack->FaceFXAnimSets(i);
				if(Set)
				{
					Asset->MountFaceFXAnimSet(Set);
				}

				if( GIsEditor && !GWorld->HasBegunPlay() )
				{
					// Now that we've mounted our anim sets, we'll make sure sound cue references are up to date
					FaceFXTrack->UpdateFaceFXSoundCueReferences( Asset );
				}
			}
		}
	}

	// Init position, and flag so we catch first update.
	LastUpdatePosition = Seq->Position;
	bFirstUpdate = TRUE;
}

/** Stop playing FaceFX animations when exiting a cinematic. */
void UInterpTrackInstFaceFX::TermTrackInst(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(Actor)
	{
		// Because we can't call script events from Matinee, if we are in matinee, use C++ version of function
		if(GIsEditor && !GWorld->HasBegunPlay())
		{
			Actor->PreviewActorStopFaceFX();
		}
		else
		{
			Actor->eventStopActorFaceFXAnim();
		}
	}

	Super::TermTrackInst( Track );
}

/** Get a ref to the FaceFXAsset that is being used by the Actor. */
void UInterpTrackInstFaceFX::SaveActorState(UInterpTrack* Track)
{
	UInterpTrackFaceFX* FaceFXTrack = CastChecked<UInterpTrackFaceFX>(Track);

	AActor* Actor = GetGroupActor();
	if(Actor)
	{
		FaceFXTrack->CachedActorFXAsset = Actor->PreviewGetActorFaceFXAsset();
	}
}

/** On closing Matinee, clear any FaceFX animation on the Actor. */
void UInterpTrackInstFaceFX::RestoreActorState(UInterpTrack* Track)
{
	// Make sure sound cue references are up to date before exiting Matinee
	UInterpTrackFaceFX* FaceFXTrack = CastChecked<UInterpTrackFaceFX>(Track);
	FaceFXTrack->UpdateFaceFXSoundCueReferences( FaceFXTrack->CachedActorFXAsset );


	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	Actor->PreviewUpdateFaceFX(TRUE, TEXT(""), TEXT(""), 0.f);
}


/*-----------------------------------------------------------------------------
	UInterpTrackMorphWeight
-----------------------------------------------------------------------------*/

INT UInterpTrackMorphWeight::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	INT NewKeyIndex = FloatTrack.AddPoint( Time, 0.f );
	FloatTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

void UInterpTrackMorphWeight::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(Actor)
	{
		Actor->PreviewSetMorphWeight(MorphNodeName, FloatTrack.Eval(NewPosition, 0.f));
	}
}

void UInterpTrackMorphWeight::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(Actor)
	{
		Actor->eventSetMorphWeight(MorphNodeName, FloatTrack.Eval(NewPosition, 0.f));
	}
}


/*-----------------------------------------------------------------------------
	UInterpTrackInstMorphWeight
-----------------------------------------------------------------------------*/


void UInterpTrackInstMorphWeight::RestoreActorState(UInterpTrack* Track)
{
	UInterpTrackMorphWeight* MorphTrack = CastChecked<UInterpTrackMorphWeight>(Track);
	AActor* Actor = GetGroupActor();
	if(Actor)
	{
		Actor->PreviewSetMorphWeight(MorphTrack->MorphNodeName, 0.f);
	}
}


/*-----------------------------------------------------------------------------
	UInterpTrackSkelControlScale
-----------------------------------------------------------------------------*/

INT UInterpTrackSkelControlScale::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	INT NewKeyIndex = FloatTrack.AddPoint( Time, 0.f );
	FloatTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

void UInterpTrackSkelControlScale::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(Actor)
	{
		Actor->PreviewSetSkelControlScale(SkelControlName, FloatTrack.Eval(NewPosition, 0.f));
	}
}


void UInterpTrackSkelControlScale::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(Actor)
	{
		Actor->eventSetSkelControlScale(SkelControlName, FloatTrack.Eval(NewPosition, 0.f));
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstSkelControlScale
-----------------------------------------------------------------------------*/


void UInterpTrackInstSkelControlScale::RestoreActorState(UInterpTrack* Track)
{
	UInterpTrackSkelControlScale* ScaleTrack = CastChecked<UInterpTrackSkelControlScale>(Track);
	AActor* Actor = GetGroupActor();
	if(Actor)
	{
		Actor->PreviewSetSkelControlScale(ScaleTrack->SkelControlName, 1.f);
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackSkelControlStrength
-----------------------------------------------------------------------------*/

INT UInterpTrackSkelControlStrength::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	INT NewKeyIndex = FloatTrack.AddPoint( Time, 0.f );
	FloatTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

void UInterpTrackSkelControlStrength::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(Actor)
	{
		Actor->SetSkelControlStrength(SkelControlName, FloatTrack.Eval(NewPosition, 0.f));
	}
}


void UInterpTrackSkelControlStrength::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(Actor)
	{
		Actor->SetSkelControlStrength(SkelControlName, FloatTrack.Eval(NewPosition, 0.f));
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstSkelControlStrength
-----------------------------------------------------------------------------*/


void UInterpTrackInstSkelControlStrength::RestoreActorState(UInterpTrack* Track)
{
	UInterpTrackSkelControlStrength* StrengthTrack = CastChecked<UInterpTrackSkelControlStrength>(Track);
	AActor* Actor = GetGroupActor();
	if(Actor)
	{
		Actor->SetSkelControlStrength(StrengthTrack->SkelControlName, 1.f);
	}
}

/** Initialise this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstSkelControlStrength::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	AActor* Actor = GetGroupActor();
	if (Actor)
	{
		USkeletalMeshComponent * SkelComp = GetSkeletalMeshComp(Actor);
		if (SkelComp)
		{
			UInterpTrackSkelControlStrength * TrackSkelControlStrength = Cast<UInterpTrackSkelControlStrength>(Track);
			USkelControlBase * SkelControl = SkelComp->FindSkelControl(TrackSkelControlStrength->SkelControlName);
			if (SkelControl)
			{
				bSavedControlledByAnimMetaData = SkelControl->bControlledByAnimMetada;
				// I need to control strength during matinee, please turn this off
				SkelControl->bControlledByAnimMetada = FALSE;
			}
		}
	}
}

/** Called when interpolation is done. Should not do anything else with this TrackInst after this. */
void UInterpTrackInstSkelControlStrength::TermTrackInst(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if (Actor)
	{
		USkeletalMeshComponent * SkelComp = GetSkeletalMeshComp(Actor);
		if (SkelComp)
		{
			UInterpTrackSkelControlStrength * TrackSkelControlStrength = Cast<UInterpTrackSkelControlStrength>(Track);
			USkelControlBase * SkelControl = SkelComp->FindSkelControl(TrackSkelControlStrength->SkelControlName);
			if (SkelControl)
			{
				// restore value
				SkelControl->bControlledByAnimMetada = bSavedControlledByAnimMetaData;
			}
		}
	}

	Super::TermTrackInst( Track );
}


/*-----------------------------------------------------------------------------
	UInterpTrackAudioMaster
-----------------------------------------------------------------------------*/

/** Add a new keyframe at the specified time. Returns index of new keyframe. */
INT UInterpTrackAudioMaster::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	const FLOAT DefaultVolume = 1.0f;
	const FLOAT DefaultPitch = 1.0f;

	INT NewKeyIndex = VectorTrack.AddPoint( Time, FVector( DefaultVolume, DefaultPitch, 0.0f ) );
	VectorTrack.Points( NewKeyIndex ).InterpMode = InitInterpMode;

	VectorTrack.AutoSetTangents( CurveTension );

	return NewKeyIndex;
}


/** Change the value of an existing keyframe. */
void UInterpTrackAudioMaster::UpdateKeyframe(INT KeyIndex, UInterpTrackInst* TrInst)
{
}


/** This is called in the editor, when scrubbing/previewing etc. */
void UInterpTrackAudioMaster::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
}


/** This is called in the game, when USeqAct_Interp is ticked */
void UInterpTrackAudioMaster::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
}


/** Set the AudioMaster track to defaults */
void UInterpTrackAudioMaster::SetTrackToSensibleDefault()
{
	const FLOAT DefaultVolume = 1.0f;
	const FLOAT DefaultPitch = 1.0f;

	VectorTrack.Points.Empty();
	VectorTrack.AddPoint( 0.0f, FVector( DefaultVolume, DefaultPitch, 0.0f ) );
}	


/** Return the sound volume scale for the specified time */
FLOAT UInterpTrackAudioMaster::GetVolumeScaleForTime( FLOAT Time ) const
{
	const FLOAT DefaultVolume = 1.0f;
	const FLOAT DefaultPitch = 1.0f;
	FVector DefaultVolumePitch( DefaultVolume, DefaultPitch, 0.0f );

	return VectorTrack.Eval( Time, DefaultVolumePitch ).X;		// X = Volume
}


/** Return the sound pitch scale for the specified time */
FLOAT UInterpTrackAudioMaster::GetPitchScaleForTime( FLOAT Time ) const
{
	const FLOAT DefaultVolume = 1.0f;
	const FLOAT DefaultPitch = 1.0f;
	FVector DefaultVolumePitch( DefaultVolume, DefaultPitch, 0.0f );

	return VectorTrack.Eval( Time, DefaultVolumePitch ).Y;		// Y = Pitch
}


/*-----------------------------------------------------------------------------
	UInterpTrackInstAudioMaster
-----------------------------------------------------------------------------*/

void UInterpTrackInstAudioMaster::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );
}

void UInterpTrackInstAudioMaster::TermTrackInst(UInterpTrack* Track)
{
	Super::TermTrackInst( Track );
}




/*-----------------------------------------------------------------------------
	UInterpTrackVisibility
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackVisibility, VisibilityTrack)
STRUCTTRACK_GETTIMERANGE(UInterpTrackVisibility, VisibilityTrack, Time)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackVisibility,VisibilityTrack, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackVisibility, VisibilityTrack, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackVisibility, VisibilityTrack, Time, FVisibilityTrackKey)
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackVisibility, VisibilityTrack)
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackVisibility, VisibilityTrack, Time, FVisibilityTrackKey)
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackVisibility, VisibilityTrack, Time)

// InterpTrack interface
INT UInterpTrackVisibility::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	UInterpTrackInstVisibility* VisibilityInst = CastChecked<UInterpTrackInstVisibility>(TrInst);

	INT i = 0;
	for (i = 0; i < VisibilityTrack.Num() && VisibilityTrack(i).Time < Time; i++);
	VisibilityTrack.Insert(i);
	VisibilityTrack(i).Time = Time;
	VisibilityTrack(i).Action = VisibilityInst->Action;

	return i;
}

void UInterpTrackVisibility::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if (Actor == NULL)
	{
		return;
	}

	UInterpTrackInstVisibility* VisibilityInst = CastChecked<UInterpTrackInstVisibility>(TrInst);
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
	UInterpGroup* Group = CastChecked<UInterpGroup>( GetOuter() );
	UInterpData* IData = CastChecked<UInterpData>( Group->GetOuter() );


	// NOTE: We don't fire events when jumping forwards in Matinee preview since that would
	//       fire off particles while scrubbing, which we currently don't want.
	const UBOOL bShouldActuallyFireEventsWhenJumpingForwards = bFireEventsWhenJumpingForwards;

	// @todo: Make this configurable?
	const UBOOL bInvertBoolLogicWhenPlayingBackwards = TRUE;


	// We'll consider playing events in reverse if we're either actively playing in reverse or if
	// we're in a paused state but forcing an update to an older position (scrubbing backwards in editor.)
	UBOOL bIsPlayingBackwards =
		( Seq->bIsPlaying && Seq->bReversePlayback ) ||
		( bJump && !Seq->bIsPlaying && NewPosition < VisibilityInst->LastUpdatePosition );


	// Find the interval between last update and this to check events with.
	UBOOL bFireEvents = TRUE;


	if( bJump )
	{
		// If we are playing forwards, and the flag is set, fire events even if we are 'jumping'.
		if (bShouldActuallyFireEventsWhenJumpingForwards)
		{
			bFireEvents = TRUE;
		}
		else
		{
			bFireEvents = FALSE;
		}
	}

	// If playing sequence forwards.
	FLOAT MinTime, MaxTime;
	if( !bIsPlayingBackwards )
	{
		MinTime = VisibilityInst->LastUpdatePosition;
		MaxTime = NewPosition;

		// Slight hack here.. if playing forwards and reaching the end of the sequence, force it over a little to ensure we fire events actually on the end of the sequence.
		if( MaxTime == IData->InterpLength )
		{
			MaxTime += (FLOAT)KINDA_SMALL_NUMBER;
		}

		if( !bFireEventsWhenForwards )
		{
			bFireEvents = FALSE;
		}
	}
	// If playing sequence backwards.
	else
	{
		MinTime = NewPosition;
		MaxTime = VisibilityInst->LastUpdatePosition;

		// Same small hack as above for backwards case.
		if( MinTime == 0.0f )
		{
			MinTime -= (FLOAT)KINDA_SMALL_NUMBER;
		}

		if( !bFireEventsWhenBackwards )
		{
			bFireEvents = FALSE;
		}
	}


	// If we should be firing events for this track...
	if( bFireEvents )
	{
		// See which events fall into traversed region.
		for(INT CurKeyIndex = 0; CurKeyIndex < VisibilityTrack.Num(); ++CurKeyIndex )
		{
			// Iterate backwards if we're playing in reverse so that toggles are applied in the correct order
			INT ActualKeyIndex = CurKeyIndex;
			if( bIsPlayingBackwards )
			{
				ActualKeyIndex = ( VisibilityTrack.Num() - 1 ) - CurKeyIndex;
			}

			FVisibilityTrackKey& VisibilityKey = VisibilityTrack( ActualKeyIndex );

			FLOAT EventTime = VisibilityKey.Time;

			// Need to be slightly careful here and make behavior for firing events symmetric when playing forwards of backwards.
			UBOOL bFireThisEvent = FALSE;
			if( !bIsPlayingBackwards )
			{
				if( EventTime >= MinTime && EventTime <= MaxTime )
				{
					bFireThisEvent = TRUE;
				}
			}
			else
			{
				if( EventTime > MinTime && EventTime <= MaxTime )
				{
					bFireThisEvent = TRUE;
				}
			}

			if( bFireThisEvent )
			{
				// NOTE: Because of how Toggle keys work, we need to run every event in the range, not
				//       just the last event.

				if( Actor != NULL )
				{
					// Make sure the key's condition is satisfied
					if( !( VisibilityKey.ActiveCondition == EVTC_GoreEnabled && !Seq->bShouldShowGore ) &&
						!( VisibilityKey.ActiveCondition == EVTC_GoreDisabled && Seq->bShouldShowGore ) )
					{
						if( VisibilityKey.Action == EVTA_Show )
						{
							UBOOL bShouldHide = FALSE;
							if( bInvertBoolLogicWhenPlayingBackwards && bIsPlayingBackwards )
							{
								// Playing in reverse, so invert bool logic (Show -> Hide)
								bShouldHide = TRUE;
							}

							// Show the actor
							Actor->SetHidden( bShouldHide );
						}
						else if( VisibilityKey.Action == EVTA_Hide )
						{
							UBOOL bShouldHide = TRUE;
							if( bInvertBoolLogicWhenPlayingBackwards && bIsPlayingBackwards )
							{
								// Playing in reverse, so invert bool logic (Hide -> Show)
								bShouldHide = FALSE;
							}

							// Hide the actor
							Actor->SetHidden( bShouldHide );
						}
						else if( VisibilityKey.Action == EVTA_Toggle )
						{
							// Toggle the actor's visibility
							Actor->SetHidden( !Actor->bHidden );
						}
						if (!Seq->bClientSideOnly && VisibilityKey.ActiveCondition == EVTC_Always)
						{
							Actor->bNetDirty = TRUE;
							Actor->eventForceNetRelevant();
						}
					}
				}
			}	
		}
	}

	VisibilityInst->LastUpdatePosition = NewPosition;
}

/** 
 *	Function which actually updates things based on the new position in the track. 
 *  This is called in the editor, when scrubbing/previewing etc.
 */
void UInterpTrackVisibility::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );

	// Dont play sounds unless we are preview playback (ie not scrubbing).
	UBOOL bJump = !(Seq->bIsPlaying);
	UpdateTrack(NewPosition, TrInst, bJump);
}

/** 
 *	Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
 *
 *	@return		String name of the helper class.
 */
const FString UInterpTrackVisibility::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackVisibilityHelper") );
}


/*-----------------------------------------------------------------------------
	UInterpTrackInstVisibility
-----------------------------------------------------------------------------*/
/** Initialise this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstVisibility::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );
}





/*-----------------------------------------------------------------------------
	UInterpTrackParticleReplay
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackParticleReplay, TrackKeys)
STRUCTTRACK_GETTIMERANGE(UInterpTrackParticleReplay, TrackKeys, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackParticleReplay, TrackKeys, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackParticleReplay, TrackKeys, Time, FParticleReplayTrackKey)
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackParticleReplay, TrackKeys)
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackParticleReplay, TrackKeys, Time, FParticleReplayTrackKey)
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackParticleReplay, TrackKeys, Time)

// InterpTrack interface
INT UInterpTrackParticleReplay::AddKeyframe( FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode )
{
	UInterpTrackInstParticleReplay* ParticleReplayInst = CastChecked<UInterpTrackInstParticleReplay>(TrInst);

	// Figure out which key we should insert before by testing key time values
	INT InsertBeforeIndex = 0;
	while( InsertBeforeIndex < TrackKeys.Num() && TrackKeys( InsertBeforeIndex ).Time < Time )
	{
		++InsertBeforeIndex;
	}

	// Create new key frame
	FParticleReplayTrackKey NewKey;
	NewKey.Time = Time;
	NewKey.ClipIDNumber = 1;	// Default clip ID number
	NewKey.Duration = 1.0f;		// Default duration

	// Insert the new key
	TrackKeys.InsertItem( NewKey, InsertBeforeIndex );

	return InsertBeforeIndex;
}



void UInterpTrackParticleReplay::UpdateTrack( FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump )
{
	AActor* Actor = TrInst->GetGroupActor();
	if (Actor == NULL)
	{
		return;
	}

	UInterpTrackInstParticleReplay* ParticleReplayInst = CastChecked<UInterpTrackInstParticleReplay>(TrInst);

	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );

	// Particle replay tracks are expecting to be dealing with emitter actors
	AEmitter* EmitterActor = Cast< AEmitter >( Actor );
	if( EmitterActor != NULL && EmitterActor->ParticleSystemComponent != NULL )
	{
		if( ( NewPosition > ParticleReplayInst->LastUpdatePosition ) && !bJump )
		{
			for (INT KeyIndex = 0; KeyIndex < TrackKeys.Num(); KeyIndex++)
			{
				FParticleReplayTrackKey& ParticleReplayKey = TrackKeys(KeyIndex);

				// Check to see if we hit this key's start time
				if( ( ParticleReplayKey.Time < NewPosition ) && ( ParticleReplayKey.Time >= ParticleReplayInst->LastUpdatePosition ) )
				{
					if( bIsCapturingReplay )
					{
						// Do we already have data for this clip?
						UParticleSystemReplay* ExistingClipReplay =
							EmitterActor->ParticleSystemComponent->FindReplayClipForIDNumber( ParticleReplayKey.ClipIDNumber );
						if( ExistingClipReplay != NULL )
						{
							// Clear the existing clip's frame data.  We're re-recording the clip now!
							ExistingClipReplay->Frames.Empty();
						}

						// Start capturing!
						EmitterActor->ParticleSystemComponent->ReplayState = PRS_Capturing;
						EmitterActor->ParticleSystemComponent->ReplayClipIDNumber = ParticleReplayKey.ClipIDNumber;
						EmitterActor->ParticleSystemComponent->ReplayFrameIndex = 0;

						// Make sure we're alive and kicking
						EmitterActor->ParticleSystemComponent->SetActive( TRUE );
					}
					else
					{
						// Start playback!
						EmitterActor->ParticleSystemComponent->ReplayState = PRS_Replaying;
						EmitterActor->ParticleSystemComponent->ReplayClipIDNumber = ParticleReplayKey.ClipIDNumber;
						EmitterActor->ParticleSystemComponent->ReplayFrameIndex = 0;

						// Make sure we're alive and kicking
						EmitterActor->ParticleSystemComponent->SetActive( TRUE );
					}
				}

				// Check to see if we hit this key's end time
				const FLOAT KeyEndTime = ParticleReplayKey.Time + ParticleReplayKey.Duration;
				if( ( KeyEndTime < NewPosition ) && ( KeyEndTime >= ParticleReplayInst->LastUpdatePosition ) )
				{
					if( !bIsCapturingReplay )
					{
						// Done playing back replay sequence, so turn off the particle system
						EmitterActor->ParticleSystemComponent->SetActive( FALSE );

						// Stop playback/capture!  We'll still keep the particle system in 'replay mode' while
						// the replay track is bound so that the system doesn't start simulating/rendering
						EmitterActor->ParticleSystemComponent->ReplayState = PRS_Replaying;
						EmitterActor->ParticleSystemComponent->ReplayClipIDNumber = INDEX_NONE;
						EmitterActor->ParticleSystemComponent->ReplayFrameIndex = INDEX_NONE;
					}
				}
			}
		}


#if WITH_EDITORONLY_DATA
		// Are we 'jumping in time'? (scrubbing)
		if( bJump )
		{
			if( bIsCapturingReplay )
			{
				// Scrubbing while capturing will stop the capture
				EmitterActor->ParticleSystemComponent->ReplayState = PRS_Disabled;
			}
			else
			{
				// Scrubbing while replaying with render the specific frame of the particle system

				// Find the time that the last replay was started
				UBOOL bHaveReplayStartKey = FALSE;
				FParticleReplayTrackKey CurrentReplayStartKey;
				for( INT KeyIndex = TrackKeys.Num() - 1; KeyIndex >= 0; --KeyIndex )
				{
					FParticleReplayTrackKey& ParticleReplayKey = TrackKeys( KeyIndex );

					// Check to see if we hit this key's start time
					if( ParticleReplayKey.Time < NewPosition )
					{
						CurrentReplayStartKey = ParticleReplayKey;
						bHaveReplayStartKey = TRUE;
						break;
					}
				}

				UBOOL bIsReplayingSingleFrame = FALSE;
				if( bHaveReplayStartKey )
				{
					const FLOAT TimeWithinReplay = NewPosition - CurrentReplayStartKey.Time;
					const INT ReplayFrameIndex = appTrunc( TimeWithinReplay / Max( ( FLOAT )KINDA_SMALL_NUMBER, FixedTimeStep ) );


					// Check to see if we have a clip
					UParticleSystemReplay* ParticleSystemReplay =
						EmitterActor->ParticleSystemComponent->FindReplayClipForIDNumber( CurrentReplayStartKey.ClipIDNumber );
					if( ParticleSystemReplay != NULL )
					{
						if( ReplayFrameIndex < ParticleSystemReplay->Frames.Num() )
						{
							// Playback specific frame!
							bIsReplayingSingleFrame = TRUE;

							// Make sure replay mode is turned on
							EmitterActor->ParticleSystemComponent->ReplayState = PRS_Replaying;
							EmitterActor->ParticleSystemComponent->ReplayClipIDNumber = CurrentReplayStartKey.ClipIDNumber;
							EmitterActor->ParticleSystemComponent->ReplayFrameIndex = ReplayFrameIndex;

							// Make sure we're alive and kicking
							EmitterActor->ParticleSystemComponent->SetActive( TRUE );
						}
					}
				}

				if( !bIsReplayingSingleFrame )
				{
					// Stop playback!  We'll still keep the particle system in 'replay mode' while
					// the replay track is bound so that the system doesn't start simulating/rendering
					EmitterActor->ParticleSystemComponent->ReplayState = PRS_Replaying;
					EmitterActor->ParticleSystemComponent->ReplayClipIDNumber = INDEX_NONE;
					EmitterActor->ParticleSystemComponent->ReplayFrameIndex = INDEX_NONE;

					// We're not currently capturing and we're not in the middle of a replay frame,
					// so turn off the particle system
					EmitterActor->ParticleSystemComponent->SetActive( FALSE );
				}
			}
		}
		else
#endif // WITH_EDITORONLY_DATA
		{
			// Okay, we're not scrubbing, but are we replaying a particle system?
			if( EmitterActor->ParticleSystemComponent->ReplayState == PRS_Replaying )
			{
				// Advance to next frame (or reverse to the previous frame)
				if( Seq->bReversePlayback )
				{
					--EmitterActor->ParticleSystemComponent->ReplayFrameIndex;
				}
				else
				{
					++EmitterActor->ParticleSystemComponent->ReplayFrameIndex;
				}
			}
		}
	}


	ParticleReplayInst->LastUpdatePosition = NewPosition;
}


/** 
 *	Function which actually updates things based on the new position in the track. 
 *  This is called in the editor, when scrubbing/previewing etc.
 */
void UInterpTrackParticleReplay::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );

	UBOOL bJump = !(Seq->bIsPlaying);
	UpdateTrack(NewPosition, TrInst, bJump);
}

/**
 * @return	The ending time of the track. 
 */
FLOAT UInterpTrackParticleReplay::GetTrackEndTime() const
{
	FLOAT EndTime = 0.0f;

	if( TrackKeys.Num() )
	{
		const FParticleReplayTrackKey& ParticleReplayKey = TrackKeys( TrackKeys.Num()-1 );
		EndTime = ParticleReplayKey.Time + ParticleReplayKey.Duration;
	}

	return EndTime;
}

/** 
 *	Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
 *
 *	@return		String name of the helper class.
 */
const FString UInterpTrackParticleReplay::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackParticleReplayHelper") );
}


/*-----------------------------------------------------------------------------
	UInterpTrackInstParticleReplay
-----------------------------------------------------------------------------*/

/** Initialise this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstParticleReplay::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );
}


/** Restore the saved state of this Actor. */
void UInterpTrackInstParticleReplay::RestoreActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if( Actor != NULL )
	{
		// Particle replay tracks are expecting to be dealing with emitter actors
		AEmitter* EmitterActor = Cast< AEmitter >( Actor );
		if( EmitterActor != NULL && EmitterActor->ParticleSystemComponent != NULL )
		{
			// Make sure we don't leave the particle system in 'capture mode'
			
			// Stop playback/capture!  We'll still keep the particle system in 'replay mode' while
			// the replay track is bound so that the system doesn't start simulating/rendering
			EmitterActor->ParticleSystemComponent->ReplayState = PRS_Disabled;
			EmitterActor->ParticleSystemComponent->ReplayClipIDNumber = 0;
			EmitterActor->ParticleSystemComponent->ReplayFrameIndex = 0;
		}
	}
}

/** GetNetPriority()
Increase net priority for interpactor on which viewer is based.

@param Viewer		PlayerController owned by the client for whom net priority is being determined
@param InChannel	Channel on which this actor is being replicated.
@param Time			Time since actor was last replicated
@returns			Priority of this actor for replication
*/
FLOAT AInterpActor::GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, APlayerController* Viewer, UActorChannel* InChannel, FLOAT Time, UBOOL bLowBandwidth)
{
	if ( Viewer && Viewer->ViewTarget && Viewer->ViewTarget->Base == this )
	{
		return 4.f * Time * NetPriority;
	}
	else 
	{
		return Super::GetNetPriority(ViewPos, ViewDir, Viewer, InChannel, Time, bLowBandwidth);
	}
}


/****************************************/
/** InterpGroupAI : Group for AI		*/
/****************************************/

void UInterpGroupAI::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();
		if ( PropertyName == TEXT("PreviewPawnClass") )
		{
			// refresh variable, so that it can refresh info when next update
			bRecreatePreviewPawn = TRUE;
		}
		else if ( PropertyName == TEXT("StageMarkGroup") || PropertyName == TEXT("bNoEncroachmentCheck") || PropertyName == TEXT("bIgnoreLegacyHeightAdjust") )
		{
			// refresh variable, so that it can refresh info when next update

			static FLOAT LastTimeTriggered = 0.f;
 			if ( LastTimeTriggered == 0.f || TimeSince(LastTimeTriggered) > 180.f )
 			{
				bRefreshStageMarkGroup = TRUE;
 			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**********************************************/
/** InterpGroupInstAI : Group Instance for AI */
/**********************************************/

void UInterpGroupInstAI::CreatePreviewPawn()
{
#if WITH_EDITORONLY_DATA
	if ( !AIGroup )
	{
		return;
	}

	UClass * DefaultPreviewPawnClass=NULL;
	// if no preview pawn class is set, get default one
	FString PreviewPawnName = GConfig->GetStr(TEXT("MatineePreview"), TEXT("AIGroupPreviewPawnClassName"), GEditorIni);
	if ( PreviewPawnName!= TEXT("") )
	{
		DefaultPreviewPawnClass = LoadObject<UClass>(NULL, *PreviewPawnName, NULL, LOAD_None, NULL);
	}
	else
	{
		debugf(NAME_Warning, TEXT("Matinee Preview Default Mesh is missing."));
		return;
	}

	// Keep track of whether or not None was specified, so we know whether or not to create a mesh
	const UBOOL bNone = !AIGroup->PreviewPawnClass ? TRUE : FALSE;
	if ( !AIGroup->PreviewPawnClass )
	{
		AIGroup->PreviewPawnClass = DefaultPreviewPawnClass;
		if ( !AIGroup->PreviewPawnClass )
		{
			debugf(NAME_Warning, TEXT("Matinee Preview Default Pawn can't be loaded : %s."), *PreviewPawnName);
			return;
		}
	}

	// if stage mark actor exists
	if (AIGroup->PreviewPawnClass != NULL && StageMarkActor != NULL)
	{
		if (PreviewPawn == NULL)
		{
			// Spawn Editor Pawn
			// Find current location where view point is
			FRotator Rotation;
			FVector StageMarkPosition = GetStageMarkPosition(&Rotation);
			FVector Position = StageMarkPosition;
			// spawn high to avoid collision
			if (AIGroup->bNoEncroachmentCheck || AIGroup->bDisableWorldCollision)
			{
				PreviewPawn = Cast<APawn>(GWorld->SpawnActor(AIGroup->PreviewPawnClass, NAME_None, Position, Rotation, NULL, TRUE));
			}
			else
			{
				APawn * DefaultPawn = Cast<APawn>(AIGroup->PreviewPawnClass->GetDefaultActor());
				if (DefaultPawn && DefaultPawn->CylinderComponent)
				{
					Position.Z += DefaultPawn->CylinderComponent->CollisionHeight;
				}

				PreviewPawn = Cast<APawn>(GWorld->SpawnActor(AIGroup->PreviewPawnClass, NAME_None, Position, Rotation, NULL));
			}

			// if no preview pawn, most likely it's collision
			if (PreviewPawn == NULL)
			{
				debugf(NAME_Warning, TEXT("Creating Preview Pawn failed. Maybe collision? "));
			}
			else
			{
				// if no preview pawn mesh, then it hasn't had one setup in it's script, create one
				if (!bNone && PreviewPawn->Mesh == NULL)
				{
					PreviewPawn->Mesh = ConstructObject<USkeletalMeshComponent>( USkeletalMeshComponent::StaticClass(), PreviewPawn, NAME_None, RF_Transactional );
					if (PreviewPawn->Mesh == NULL)
					{
						debugf(NAME_Warning, TEXT("Preview Pawn has no mesh set in script and unable to create one. "));
					}
					else
					{
						PreviewPawn->Components.AddItem( PreviewPawn->Mesh );
					}
				}

				if (PreviewPawn->Mesh)
				{
					if (!bNone && PreviewPawn->Mesh->SkeletalMesh == NULL)
					{
						// set skeletal mesh
						FString PreviewSMName = GConfig->GetStr(TEXT("MatineePreview"), TEXT("AIGroupPreviewSkeletalMeshName"), GEditorIni);
						if (PreviewSMName!= TEXT(""))
						{
							USkeletalMesh * PreviewSM = LoadObject<USkeletalMesh>(NULL, *PreviewSMName, NULL, LOAD_None, NULL);
							if (PreviewSM)
							{
								// set skeletal mesh
								PreviewPawn->Mesh->SetSkeletalMesh( PreviewSM );
							}
							else
							{
								debugf(NAME_Warning, TEXT("Matinee Preview Preview skeletalmesh can't be loaded : %s."), *PreviewSMName);
							}
						}
					}
					if (!bNone && PreviewPawn->Mesh->AnimTreeTemplate == NULL)
					{
						// set animtree templates
						FString PreviewTreeName = GConfig->GetStr(TEXT("MatineePreview"), TEXT("AIGroupPreviewAnimTreeName"), GEditorIni);
						if (PreviewTreeName!= TEXT(""))
						{
							UAnimTree * PreviewAnimTreeTemplate = LoadObject<UAnimTree>(NULL, *PreviewTreeName, NULL, LOAD_None, NULL);
							if (PreviewAnimTreeTemplate)
							{
								// set animtreetemplate - do not use SetAnimTreeTemplate which will copy/init all that
								// we don't have to - we do that in PreviewBeginAnimControl
								// and it will clear up at the end of preview
								PreviewPawn->Mesh->AnimTreeTemplate = PreviewAnimTreeTemplate;
							}
							else
							{
								debugf(NAME_Warning, TEXT("Matinee Preview Preview animtree can't be loaded : %s."), *PreviewTreeName);
							}
						}
					}
				}

				// need to reinitialize animcontrol 
				PreviewPawn->PreviewBeginAnimControl(AIGroup);

				// need to recalc initial transform for movement track
				for(INT j=0; j<TrackInst.Num(); j++)
				{
					UInterpTrackInst* TrInst = TrackInst(j);
					UInterpTrackInstMove* MoveInst = Cast<UInterpTrackInstMove>(TrInst);
					if(MoveInst)
					{
						MoveInst->CalcInitialTransform( Group->InterpTracks(j), true );
					}
				}
			}
		}
	}
	else
	{
		debugf(NAME_Warning, TEXT("Stage Mark Actor is missing. Link a valid actor (start point) to Stage Mark Group"));
	}
#endif // WITH_EDITORONLY_DATA
}

void UInterpGroupInstAI::DestroyPreviewPawn()
{
#if WITH_EDITORONLY_DATA
	if ( PreviewPawn != NULL )
	{
		PreviewPawn->PreviewFinishAnimControl(AIGroup);
		GWorld->DestroyActor(PreviewPawn);
		PreviewPawn = NULL;

		check(GIsEditor);
		GCallbackEvent->Send( CALLBACK_SelectNone );
	}
#endif // WITH_EDITORONLY_DATA
}

/**
 * Get Stage Mark Actor ground position & rotation
 */
FVector UInterpGroupInstAI::GetStageMarkPosition(FRotator* Rotation)
{
	if (StageMarkActor)
	{
		*Rotation = StageMarkActor->Rotation;

		FCheckResult Hit;
		FVector vResult;

		if (AIGroup->bNoEncroachmentCheck  || AIGroup->bDisableWorldCollision || GWorld->SingleLineCheck( Hit, StageMarkActor, StageMarkActor->Location - FVector(0, 0, 100), StageMarkActor->Location, TRACE_World ))
		{
			vResult = StageMarkActor->Location;
		}
		else
		{
			vResult = Hit.Location;
		}

		return vResult;
	}

	*Rotation = FRotator::ZeroRotator;
	return FVector::ZeroVector;
}

/** 
 *  Update Stage Mark Group Actor
 */ 
void UInterpGroupInstAI::UpdateStageMarkGroupActor(USeqAct_Interp * Seq)
{
	if ( Seq && AIGroup )
	{
		// We don't want to update any Actors that this Matinee is acting on, in case we mess up where the sequence has put it.
		UInterpGroupInst * StageMarkLookupGroup = Seq->FindFirstGroupInstByName(AIGroup->StageMarkGroup);
		if ( StageMarkLookupGroup && StageMarkLookupGroup->GetGroupActor() )
		{
			StageMarkActor = StageMarkLookupGroup->GetGroupActor();
		}
		else
		{
			// print error
			debugf(TEXT("Matinee AI Group's Stage Mark is missing or valid actor isn't connected : %s (%s)"), *AIGroup->StageMarkGroup.GetNameString(), *Seq->GetFullName());
		}
	}
}

UBOOL UInterpGroupInstAI::HasActor(AActor * InActor)
{
	if (Super::HasActor(InActor))
	{
		return TRUE;
	}
	
	AActor * GrActor = GetGroupActor();

	if ( !GrActor )
	{
		return FALSE;
	}

	check (InActor);

	// if controller, check it's pawn
	if ( InActor->IsA(AController::StaticClass()) )
	{
		if (GrActor == CastChecked<AController>(InActor)->Pawn)
		{
			return TRUE;
		}
	}
	if ( GrActor->IsA(AController::StaticClass()) )
	{
		if (InActor == CastChecked<AController>(GrActor)->Pawn) 
		{
			return TRUE;
		}
	}

	return FALSE;
}
/** 
*	Returns the Actor that this GroupInstance is working on. 
*	Should use this instead of just referencing GroupActor, as it check bDeleteMe for you.
*/
AActor* UInterpGroupInstAI::GetGroupActor()
{
	check(AIGroup);

#if WITH_EDITORONLY_DATA
	// if Editor, but not in game
	// use Preview Matinee
	if ( GIsEditor && !GIsGame )
	{
		return PreviewPawn;
	}
#endif // WITH_EDITORONLY_DATA

	// if not just call parents
	return Super::GetGroupActor();
}

void UInterpGroupInstAI::InitGroupInst(UInterpGroup* InGroup, AActor* InGroupActor)
{
	Group = InGroup;
	AIGroup = Cast<UInterpGroupAI>(InGroup);

	if ( AIGroup )
	{
		// refresh stage mark group actor when initialized
		USeqAct_Interp * Seq = CastChecked<USeqAct_Interp>( GetOuter() );
		UpdateStageMarkGroupActor(Seq);

		// This is only for Preview
		// I need to do this so that MoveTrack/AnimTrack to initialize correctly
		if (GIsEditor && !GIsGame)
		{
			CreatePreviewPawn();
		}
	}

	// If allow deferred attach, wait for it  
	// if no actor, do not call init, which will create mess
	// this is only unique for AIGroups because
	// AI can spawn later or for clients, it won't have any actor first time. 
	UBOOL bShouldDelayInit = (GIsGame && AIGroup && InGroupActor == NULL);
	if ( bShouldDelayInit )
	{
		return;
	}

	// Don't set PreviewPawn here, it will save it
	Super::InitGroupInst( InGroup, InGroupActor );

	// now all initialization is done, update physics
	// this has to be called after initgroupinst
	// otherwise, tracks are not instantiated
	UpdatePhysics(TRUE);

	APawn * PawnActor = GetPawn(InGroupActor);

	if (PawnActor && StageMarkActor)
	{
		//debugf(TEXT("Name of marker (%s), Location(%s)"), *AIGroup->StageMarkActor->GetName(), *AIGroup->StageMarkActor->Location.ToString());
		// need to move first because otherwise, InitialTM in Movetrack might change due to losing base or not. 
		PawnActor->eventMAT_BeginAIGroup(StageMarkActor->Location, StageMarkActor->Rotation);
	}
}

/** 
*	Initialse this Group instance. Called from USeqAct_Interp::InitInterp before doing any interpolation.
*	Save the Actor for the group and creates any needed InterpTrackInsts
*/
void UInterpGroupInstAI::UpdatePreviewPawnFromSeqVarCharacter(UInterpGroup* InGroup, const USeqVar_Character* InGroupObject)
{
#if WITH_EDITORONLY_DATA
	if ( InGroupObject )
	{
		UInterpGroupAI * AIGroup = Cast<UInterpGroupAI>(InGroup);

		// see if it is USeqVar_Character - find the Pawn Class Mesh
		const USeqVar_Character* VarCharacter = InGroupObject;
		if ( VarCharacter && VarCharacter->PawnClass )
		{
			APawn * ReferencePawn = Cast<APawn>(VarCharacter->PawnClass->GetDefaultActor());
			AIGroup->PreviewPawnClass = ReferencePawn->GetClass();
			AIGroup->Modify();

			DestroyPreviewPawn();
			CreatePreviewPawn();
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/** 
*	Called when done with interpolation sequence. Cleans up InterpTrackInsts etc. 
*	Do not do anything further with the Interpolation after this.
*/
void UInterpGroupInstAI::TermGroupInst(UBOOL bDeleteTrackInst)
{
	APawn * PawnActor = GetPawn(GetGroupActor());

	if (PawnActor)
	{
		PawnActor->eventMAT_FinishAIGroup();
	}

	// revert physics
	UpdatePhysics(FALSE);

	DestroyPreviewPawn();

	Super::TermGroupInst( bDeleteTrackInst );
}

/** 
 *  Update Physics state if it includes Movement Track 
 *  Or terminate if bInit = FALSE
 */ 
void UInterpGroupInstAI::UpdatePhysics(UBOOL bInit)
{
	UBOOL MovementExists = FALSE;

	// check if it includes move track or not
	for ( INT I=0; I<TrackInst.Num(); ++I )
	{
		if (TrackInst(I)->IsA(UInterpTrackInstMove::StaticClass()))
		{
			MovementExists = TRUE;
		}
	}

	// since movement exists, set physics state required for movement
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	// If Pawn, change physics state to be interpolating, and unlock desiredrotation
	APawn * Pawn = GetPawn(Actor);
	if ( Pawn )
	{
		// when initialize tracks
		// make sure to save all values
		if (bInit)
		{
			if ( MovementExists )
			{
				SavedPhysics = Pawn->Physics;
				Pawn->setPhysics(PHYS_Interpolating);
			}
			if ( AIGroup->bNoEncroachmentCheck )
			{
				bSavedNoEncroachCheck = Pawn->bNoEncroachCheck;				
				bSavedCollideActors = Pawn->bCollideActors;
				bSavedBlockActors = Pawn->bBlockActors;
				Pawn->SetCollision(FALSE, FALSE, TRUE);
			}

			if ( AIGroup->bDisableWorldCollision )
			{
				Pawn->bCollideWorld = FALSE;
			}
		}
		// now exiting, recover all values to previous state
		else
		{
			if ( MovementExists )
			{
				// Need higher offset before switching physics to walk
				// otherwise, when character is on exactly floor, 
				// it doesn't find floor
				if ( SavedPhysics == PHYS_Walking )
				{
					Pawn->SetLocation(Pawn->Location + FVector(0, 0, 10));
				}

				Pawn->setPhysics(SavedPhysics);
			}
			
			if ( AIGroup->bDisableWorldCollision )
			{
				Pawn->bCollideWorld = TRUE;
			}

			if ( AIGroup->bNoEncroachmentCheck )
			{
				Pawn->SetCollision(bSavedCollideActors, bSavedBlockActors, bSavedNoEncroachCheck);			
			}

			if ( AIGroup->SnapToRootBoneLocationWhenFinished )
			{
				if ( Pawn->Mesh )
				{
					FMatrix RootTM = Pawn->Mesh->GetBoneMatrix(0);
					FVector NewLoc = RootTM.GetOrigin();
					if ( Pawn->CylinderComponent )
					{
						NewLoc.Z += Pawn->CylinderComponent->CollisionHeight;
					}
					
					Pawn->SetLocation(NewLoc);
				}
			}
		}


	}
}

// Customized UpdateGroup
void UInterpGroupAI::UpdateGroup(FLOAT NewPosition, UInterpGroupInst* GrInst, UBOOL bPreview, UBOOL bJump)
{
	UInterpGroupInstAI * AIGrInst = Cast<UInterpGroupInstAI>(GrInst);
	if (!AIGrInst)
	{
		return;
	}

	// if stage mark 
	if (!AIGrInst->StageMarkActor)
	{
		USeqAct_Interp * SeqInterp = Cast<USeqAct_Interp>(GrInst->GetOuter());
		if ( SeqInterp )
		{
			AIGrInst->UpdateStageMarkGroupActor(SeqInterp);

			if (AIGrInst->StageMarkActor)
			{
				// This is only for Preview
				// I need to do this so that MoveTrack/AnimTrack to initialize correctly
				if (GIsEditor && !GIsGame)
				{
					AIGrInst->CreatePreviewPawn();
				}
				else
				{
					// re calculate initial transform since StageMark Actor just has been found
					SeqInterp->ResetMovementInitialTransforms();
				}
			}
		}
	}

	// if deferred attach is set and no actor is found yet, check
	// remember this track is completed paused until track is attached
	UBOOL bTryAttachActor = (GIsGame && GrInst && GrInst->GetGroupActor() == NULL);
	if ( bTryAttachActor )
	{
		// search for it
		USeqAct_Interp * SeqInterp = Cast<USeqAct_Interp>(GrInst->GetOuter());
		if ( SeqInterp )
		{
			AActor * Actor = SeqInterp->FindUnusedGroupLinkedVariable(GroupName);
					
			// if actor exists, and it's not going to be deleted
			if (Actor!=NULL && Actor->bDeleteMe == FALSE)
			{
//				debugf(TEXT("AIGroup: Adding Actor %s to the group %s"), *Actor->GetName(), *GroupName.GetNameString());
				GrInst->InitGroupInst(this, Actor);
				// both has pointer to each other
				// so you need to add to both sides (SeqInterp and Actor)
				// otherwise, it won't be cleared
				SeqInterp->LatentActors.AddUniqueItem(Actor);
				Actor->LatentActions.AddUniqueItem(SeqInterp);

				SeqInterp->NotifyActorsOfChange();
			}
		}
	}

	if ( GrInst->GetGroupActor() == NULL )
	{
		return;
	}

#if WITH_EDITOR
	if (GIsEditor)
	{
		// if preview pawn exists, and preview pawn class isn't same as what's set
		// re-create
		if ( AIGrInst )
		{
			if ( bRefreshStageMarkGroup )
			{
				// search for it
				USeqAct_Interp * SeqInterp = Cast<USeqAct_Interp>(GrInst->GetOuter());
				AIGrInst->UpdateStageMarkGroupActor(SeqInterp);

				// recreate pawn
				AIGrInst->DestroyPreviewPawn();
				AIGrInst->CreatePreviewPawn();
			}

			if ( bRecreatePreviewPawn && AIGrInst->PreviewPawn && PreviewPawnClass != AIGrInst->PreviewPawn->GetClass() )
			{
				// for all inst, call this
				AIGrInst->DestroyPreviewPawn();
				AIGrInst->CreatePreviewPawn();
			}
		}
	}
#endif

	Super::UpdateGroup( NewPosition, GrInst, bPreview, bJump );
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstHeadTracking
-----------------------------------------------------------------------------*/
STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackHeadTracking, HeadTrackingTrack)
STRUCTTRACK_GETTIMERANGE(UInterpTrackHeadTracking, HeadTrackingTrack, Time)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackHeadTracking,HeadTrackingTrack, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackHeadTracking, HeadTrackingTrack, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackHeadTracking, HeadTrackingTrack, Time, FHeadTrackingKey)
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackHeadTracking, HeadTrackingTrack)
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackHeadTracking, HeadTrackingTrack, Time, FHeadTrackingKey)
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackHeadTracking, HeadTrackingTrack, Time)

#define DEBUG_HEADTRACKING 0

// InterpTrack interface
INT UInterpTrackHeadTracking::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	UInterpTrackInstHeadTracking* HeadTrackingInst = CastChecked<UInterpTrackInstHeadTracking>(TrInst);

	INT i = 0;
	for (i = 0; i < HeadTrackingTrack.Num() && HeadTrackingTrack(i).Time < Time; i++);
	HeadTrackingTrack.Insert(i);
	HeadTrackingTrack(i).Time = Time;
	HeadTrackingTrack(i).Action = HeadTrackingInst->Action;

	return i;
}

void UInterpTrackHeadTracking::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if (Actor == NULL)
	{
		return;
	}

 	UInterpTrackInstHeadTracking* HeadTrackingInst = CastChecked<UInterpTrackInstHeadTracking>(TrInst);

	// Remove any pending kill actors from list, so they will get properly GCd
	for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(HeadTrackingInst->CurrentActorMap); It; ++It )
	{
		FActorToLookAt * ActorToLookAt = It.Value();
		// Actor can never be NULL, as the map is exposed to GC
		if ( ActorToLookAt->Actor->ActorIsPendingKill())
		{
			delete It.Value();
			It.RemoveCurrent();
		}
	}

	const FHeadTrackingKey * LastTrackingKey=NULL;
	UBOOL bUpdatedActor = FALSE;
	// find key range that this position is in
	for (INT I=0; I<HeadTrackingTrack.Num(); ++I)
	{
		const FHeadTrackingKey * HeadTrackingKey = &HeadTrackingTrack(I);

		if ( NewPosition < HeadTrackingKey->Time && 
			LastTrackingKey && NewPosition > LastTrackingKey->Time && LastTrackingKey->Action == EHTA_EnableHeadTracking )
		{
			bUpdatedActor = TRUE;
			UpdateHeadTracking(Actor, TrInst, NewPosition - HeadTrackingInst->LastUpdatePosition);	
			break;
		}
		LastTrackingKey = HeadTrackingKey;
	}

	// if nothing else to update, turn that off, and clean up the list, it's not good to keep old list
	if (bUpdatedActor == FALSE)
	{
		for (INT I=0; I<HeadTrackingInst->TrackControls.Num(); ++I)
		{
			HeadTrackingInst->TrackControls(I)->SetSkelControlStrength(0.f, 0.25f);
		}

		// clean up map to make sure it's not deleted or anything like that
		for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(HeadTrackingInst->CurrentActorMap); It; ++It )
		{
			FActorToLookAt * ActorToLookAt = It.Value();
			delete It.Value();
		}

		// clear current actor map
		HeadTrackingInst->CurrentActorMap.Empty();
	}

	HeadTrackingInst->LastUpdatePosition = NewPosition;
}


/** Update Actor List for look at candidate **/
void    UInterpTrackHeadTracking::UpdateHeadTracking(AActor* Actor, UInterpTrackInst* TrInst, FLOAT DeltaTime)
{
	UInterpTrackInstHeadTracking* HeadTrackingInst = CastChecked<UInterpTrackInstHeadTracking>(TrInst);

	check (HeadTrackingInst);

	// look for lookat control
	if ( HeadTrackingInst->TrackControls.Num()==0 )
	{
		USkeletalMeshComponent * SkeletalMeshComp=GetSkeletalMeshComp(Actor);
		if ( SkeletalMeshComp && SkeletalMeshComp->SkeletalMesh && SkeletalMeshComp->Animations && SkeletalMeshComp->Animations->IsA(UAnimTree::StaticClass()) )
		{
			//now look for look at control
			UAnimTree * AnimTree = CastChecked<UAnimTree>(SkeletalMeshComp->Animations);

			if ( AnimTree )
			{
				for (INT I=0; I<TrackControllerName.Num(); ++I)
				{
					USkelControlLookAt* LookAtControl = Cast<USkelControlLookAt>(AnimTree->FindSkelControl(TrackControllerName(I)));
					if (LookAtControl)
					{
						HeadTrackingInst->TrackControls.AddItem(LookAtControl);
					}
				}
			}
		}

		if (HeadTrackingInst->TrackControls.Num() > 0)
		{
			for (INT I=0; I<HeadTrackingInst->TrackControls.Num(); ++I)
			{
				HeadTrackingInst->TrackControls(I)->bDisableBeyondLimit = bDisableBeyondLimit;
				// initialize as turn off
				HeadTrackingInst->TrackControls(I)->SetSkelControlStrength(0.f, 0.25f);
			}
		}
		else if (SkeletalMeshComp)
		{
			debugf(TEXT("Track control not found for mesh [%s]."), *SkeletalMeshComp->SkeletalMesh->GetName() );
		}
		else
		{
			debugf(TEXT("Head Track control is only supported for SkeletalMeshActorMAT or Pawn.") );
		}

		HeadTrackingInst->Mesh = SkeletalMeshComp;
	}

	FLOAT CurrentTime = GWorld->GetTimeSeconds();

	 // collect all actors - do this for every second
	if ( HeadTrackingInst->TrackControls.Num() > 0  && HeadTrackingInst->Mesh && HeadTrackingInst->Mesh->GetOwner() )
	{
		// find where is my position/rotation
		AActor * Owner = HeadTrackingInst->Mesh->GetOwner();
		FVector MeshLocation = Owner->Location;
		FRotator MeshRotation = Owner->Rotation;
		// we consider first one as base, and will calculate based on first one to target
		USkelControlLookAt * LookAtControl = HeadTrackingInst->TrackControls(0);
		if ( LookAtControl && LookAtControl->ControlBoneIndex!=INDEX_NONE )
		{
			// change MeshLocation to be that bone index
			FBoneAtom MeshRootBA = HeadTrackingInst->Mesh->GetBoneAtom(LookAtControl->ControlBoneIndex);
			MeshLocation = MeshRootBA.GetOrigin();
			if (HeadTrackingInst->Mesh->SkeletalMesh)
			{
				// apply local mesh transform to the baselookdir
				FMatrix RotMatrix = FRotationMatrix(HeadTrackingInst->Mesh->SkeletalMesh->RotOrigin);
				MeshRotation = RotMatrix.TransformNormal(LookAtControl->BaseLookDir).Rotation();
			}
			else
			{
				MeshRotation = LookAtControl->BaseLookDir.Rotation();
			}
		}

		// find actors around me
		FMemMark Mark( GMainThreadMemStack );
		FCheckResult* Link=GWorld->Hash->ActorRadiusCheck( GMainThreadMemStack, MeshLocation, LookAtActorRadius, TRACE_Actors );
		TArray<AActor *> ActorList;
		while ( Link )
		{
			if( Link->Actor &&
				Link->Actor->bCollideActors && 
				!Link->Actor->bDeleteMe )
			{
				if (bLookAtPawns && Link->Actor->IsA(APawn::StaticClass()))
				{
					ActorList.AddUniqueItem(Link->Actor);
				}
				else
				{
					// go through actor list to see if I have it. 
					for ( INT ActorID =0; ActorID < ActorClassesToLookAt.Num(); ++ActorID )
					{
						if (Link->Actor->IsA( ActorClassesToLookAt(ActorID) ))
						{
							ActorList.AddUniqueItem(Link->Actor);
							break;
						}
					}
				}

				Link = Link->GetNext();
			}
		}

		// add new items to the map
		for ( INT ActorID = 0; ActorID < ActorList.Num(); ++ActorID )
		{
			FActorToLookAt* ActorToLookAt = NULL;
			
			// if it's not in the list yet add
			if ( HeadTrackingInst->CurrentActorMap.HasKey(ActorList(ActorID)) == FALSE )
			{
				ActorToLookAt = new FActorToLookAt;
				ActorToLookAt->Actor = ActorList(ActorID);
				ActorToLookAt->EnteredTime = CurrentTime;
				ActorToLookAt->CurrentlyBeingLookedAt = FALSE;
				ActorToLookAt->LastKnownDistance = 0.f;
				ActorToLookAt->StartTimeBeingLookedAt = 0.f;
				ActorToLookAt->Rating = 0.f;
				HeadTrackingInst->CurrentActorMap.Set(ActorToLookAt->Actor, ActorToLookAt);
			}
		}

		// now run ratings
		FLOAT  LookAtActorRadiusSq =  LookAtActorRadius * LookAtActorRadius;
		FActorToLookAt * BestCandidate = NULL;
		FLOAT	BestRating = -99999.f;

		// now update their information
		for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(HeadTrackingInst->CurrentActorMap); It; ++It )
		{
			FActorToLookAt * ActorToLookAt = It.Value();
			ActorToLookAt->LastKnownDistance = (MeshLocation-ActorToLookAt->Actor->Location).SizeSquared();
			// outside of raius, do not care, delete them
			if (ActorToLookAt->LastKnownDistance > LookAtActorRadiusSq)
			{
				delete It.Value();
				It.RemoveCurrent();
			}
			else
			{
				// update rating
				// if closer, higher rating - 1 for distance, 1 for recently entered
				FLOAT DistanceRating = 1 - ActorToLookAt->LastKnownDistance/LookAtActorRadiusSq;
				// clamp time rating. Otherwise, you're never going to get second chance
				FLOAT TimeRating = Max(-1.f, (MaxInterestTime - (CurrentTime-ActorToLookAt->EnteredTime))/MaxInterestTime);
				FLOAT LookAtRating = 0.f;
				FLOAT LookAtTime = MinLookAtTime + appFrand()*(MaxLookAtTime-MinLookAtTime);

				if (ActorToLookAt->CurrentlyBeingLookedAt)
				{
					// if less than 1 second, give boost, don't like to switch every time
					LookAtRating = (LookAtTime - (CurrentTime-ActorToLookAt->StartTimeBeingLookedAt))/LookAtTime;
				}
				else if (CurrentTime-ActorToLookAt->StartTimeBeingLookedAt < LookAtTime*2.f)
				{
					// if he has been looked at before, 
					LookAtRating = (LookAtTime - (CurrentTime-ActorToLookAt->StartTimeBeingLookedAt))/LookAtTime;
				}
				else
				{
					// first time? Give boost
					LookAtRating = 0.8f;
				}

				// if it's in front of me, have more rating
				FLOAT AngleRating = (ActorToLookAt->Actor->Location-MeshLocation).SafeNormal() | MeshRotation.Vector();
				// give boost if target is moving. More interesting to see. 
				FLOAT MovingRating = (ActorToLookAt->Actor->Velocity.IsZero())? 0.f : 1.0f;

				ActorToLookAt->Rating = DistanceRating + TimeRating + LookAtRating + AngleRating + MovingRating;
#if DEBUG_HEADTRACKING
				debugf(TEXT("HeadTracking: [%s] Ratings(%0.2f), DistanceRating(%0.2f), TimeRaiting(%0.2f), LookAtRating(%0.2f), AngleRating(%0.2f), Moving Rating(%0.2f), CurrentLookAtTime(%0.2f)"), *ActorToLookAt->Actor->GetName(), ActorToLookAt->Rating, DistanceRating, TimeRating, LookAtRating, AngleRating, MovingRating, LookAtTime );
#endif
				if ( ActorToLookAt->Rating > BestRating && ActorToLookAt->Actor )
				{
					BestRating = ActorToLookAt->Rating;
					BestCandidate = ActorToLookAt;
				}
			}
		}

		// found the best candidate
		if (BestCandidate)
		{
			for (INT I=0; I<HeadTrackingInst->TrackControls.Num(); ++I)
			{
				HeadTrackingInst->TrackControls(I)->SetSkelControlStrength(1.f, 0.25f);
			}
			
#if DEBUG_HEADTRACKING
			debugf(TEXT("HeadTracking: Best Candidate [%s] "), *BestCandidate->Actor->GetName());
#endif
			if (BestCandidate->CurrentlyBeingLookedAt==FALSE)
			{
				BestCandidate->StartTimeBeingLookedAt = CurrentTime;
				for (INT I=0; I<HeadTrackingInst->TrackControls.Num(); ++I)
				{
					HeadTrackingInst->TrackControls(I)->SetLookAtAlpha(1.0f, 0.25f);
				}
			}

			BestCandidate->CurrentlyBeingLookedAt = TRUE;

			FVector TargetLoc = (BestCandidate->Actor->Location);
			if (TargetBoneNames.Num())
			{
				USkeletalMeshComponent * MeshComp = GetSkeletalMeshComp(BestCandidate->Actor);
				if (MeshComp)
				{
					for (INT TargetID=0; TargetID < TargetBoneNames.Num(); ++TargetID)
					{
						INT BoneIdx = MeshComp->MatchRefBone(TargetBoneNames(TargetID));
						if (BoneIdx != INDEX_NONE)
						{
							// found it, get out
							TargetLoc = MeshComp->GetBoneAtom(BoneIdx).GetOrigin();
							break;
						}
					}
				}
			}

			for (INT I=0; I<HeadTrackingInst->TrackControls.Num(); ++I)
			{
				HeadTrackingInst->TrackControls(I)->DesiredTargetLocation = TargetLoc;
				HeadTrackingInst->TrackControls(I)->InterpolateTargetLocation(DeltaTime);
			}

#if DEBUG_HEADTRACKING
			GWorld->GetWorldInfo()->FlushPersistentDebugLines();

			GWorld->GetWorldInfo()->DrawDebugCoordinateSystem(MeshLocation, MeshRotation, 10, TRUE);
			GWorld->GetWorldInfo()->DrawDebugLine(MeshLocation, BestCandidate->Actor->Location, 0, 0, 255, TRUE);
			GWorld->GetWorldInfo()->DrawDebugLine(MeshLocation, HeadTrackingInst->TrackControls(0)->DesiredTargetLocation, 255, 0, 0, TRUE);
			GWorld->GetWorldInfo()->DrawDebugLine(MeshLocation, HeadTrackingInst->TrackControls(0)->TargetLocation, 255, 255, 0, TRUE);
#endif
			for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(HeadTrackingInst->CurrentActorMap); It; ++It )
			{
				FActorToLookAt * ActorToLookAt = It.Value();
				if (ActorToLookAt != BestCandidate)
				{
					ActorToLookAt->CurrentlyBeingLookedAt = FALSE;
				}
			}
			return;
		}
	}
	
	// if nothing else turn that off
	if (HeadTrackingInst->TrackControls.Num() > 0 )
	{
#if DEBUG_HEADTRACKING
		debugf(TEXT("HeadTracking: Turning it off "));
#endif
		for (INT I=0; I<HeadTrackingInst->TrackControls.Num(); ++I)
		{
			HeadTrackingInst->TrackControls(I)->SetSkelControlStrength(0.f, 0.25f);
		}
	}
}

/** 
 *	Function which actually updates things based on the new position in the track. 
 *  This is called in the editor, when scrubbing/previewing etc.
 */
void UInterpTrackHeadTracking::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );

	// Dont play sounds unless we are preview playback (ie not scrubbing).
	UBOOL bJump = !(Seq->bIsPlaying);
	UpdateTrack(NewPosition, TrInst, bJump);
}

/** 
 *	Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
 *
 *	@return		String name of the helper class.
 */
const FString UInterpTrackHeadTracking::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackHeadTrackingHelper") );
}


/*-----------------------------------------------------------------------------
	UInterpTrackInstHeadTracking
-----------------------------------------------------------------------------*/
/** Initialise this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstHeadTracking::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst( Track );

	TrackControls.Empty();
	Mesh = NULL;
}

/** Called when interpolation is done. Should not do anything else with this TrackInst after this. */
void UInterpTrackInstHeadTracking::TermTrackInst(UInterpTrack* Track)
{
	// need to clear this up
	for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(CurrentActorMap); It; ++It )
	{
		FActorToLookAt * ActorToLookAt = It.Value();
		delete It.Value();
	}

	CurrentActorMap.Empty();

	for (INT I=0; I<TrackControls.Num(); ++I)
	{
		TrackControls(I)->SetSkelControlStrength(0.f, 0.25f);
	}

	TrackControls.Empty();
	Mesh = NULL;

	Super::TermTrackInst( Track );
}

/** Make sure CurrentActorMap is referenced */
void UInterpTrackInstHeadTracking::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects( ObjectArray );

	// Output reference for each actor in the map
	for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(CurrentActorMap); It; ++It )
	{
		// Key and Value->Actor are the same Actor
		FActorToLookAt * ActorToLookAt = It.Value();
		AddReferencedObject( ObjectArray, ActorToLookAt->Actor );
	}
}

void UInterpGroupCamera::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
#if WITH_EDITORONLY_DATA

	if (PropertyChangedEvent.Property->GetName() == TEXT("PreviewAnimSets"))
	{
		UCameraAnim * CameraAnim = CastChecked<UCameraAnim>(GetOuter());
		UInterpGroup* Group = CameraAnim->PreviewInterpGroup;
		// find first InterpGroup, not me
		if ( Group )
		{
			// add to groupanimsets
			for (INT J=0; J<Target.PreviewAnimSets.Num(); ++J)
			{
				if (Target.PreviewAnimSets(J))
				{
					Group->GroupAnimSets.AddUniqueItem(Target.PreviewAnimSets(J));
				}
			}

			Target.PawnInst->PreviewBeginAnimControl(Group);
		}
	}
	else if (PropertyChangedEvent.Property->GetName() == TEXT("AnimSeqName"))
	{
		UCameraAnim * CameraAnim = CastChecked<UCameraAnim>(GetOuter());
		UInterpGroup* Group = CameraAnim->PreviewInterpGroup;
		// find first InterpGroup, not me
		if ( Group )
		{
			TArray<UInterpTrack*> AnimTracks;
			Group->FindTracksByClass(UInterpTrackAnimControl::StaticClass(), AnimTracks);
			if (AnimTracks.Num() > 0)
			{
				UInterpTrackAnimControl* AnimTrack = CastChecked<UInterpTrackAnimControl>(AnimTracks(0));
				if (AnimTrack->AnimSeqs.Num() > 0)
				{
					FAnimControlTrackKey& SeqKey = AnimTrack->AnimSeqs( 0 );
					SeqKey.AnimSeqName = Target.AnimSeqName;	
				}
				else
				{
					INT KeyIndex = AnimTrack->AddKeyframe(0.0f, NULL, CIM_Linear);
					FAnimControlTrackKey& NewSeqKey = AnimTrack->AnimSeqs( KeyIndex );
					NewSeqKey.AnimSeqName = Target.AnimSeqName;	
				}
			}

			Target.PawnInst->PreviewBeginAnimControl(Group);
		}
	}
#endif

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
