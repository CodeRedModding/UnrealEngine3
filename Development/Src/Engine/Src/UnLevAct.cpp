/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "EngineAnimClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineMeshClasses.h"
#include "UnPhysicalMaterial.h"
#include "DemoRecording.h"
#include "EngineAIClasses.h"
#include "UnTerrain.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#if WITH_APEX
#include <NxModuleDestructible.h>
#include <NxApexDefs.h>
#endif
#endif

// LOOKING_FOR_PERF_ISSUES
#define PERF_MOVEACTOR_STATS (0)

#if LINE_CHECK_TRACING

/** Is tracking enabled */
INT LineCheckTracker::bIsTrackingEnabled = FALSE;
/** If this count is nonzero, dump the log when we exceed this number in any given frame */
INT LineCheckTracker::TraceCountForSpikeDump = 0;
/** Number of traces recorded this frame */
INT LineCheckTracker::CurrentCountForSpike = 0;

FStackTracker* LineCheckTracker::LineCheckStackTracker = NULL;
FScriptStackTracker* LineCheckTracker::LineCheckScriptStackTracker = NULL;

/** Updates an existing call stack trace with new data for this particular call*/
static void LineCheckUpdateFn(const FStackTracker::FCallStack& CallStack, void* UserData)
{
	if (UserData)
	{
		//Callstack has been called more than once, aggregate the data
		LineCheckTracker::FLineCheckData* NewLCData = static_cast<LineCheckTracker::FLineCheckData*>(UserData);
		LineCheckTracker::FLineCheckData* OldLCData = static_cast<LineCheckTracker::FLineCheckData*>(CallStack.UserData);

		OldLCData->Flags |= NewLCData->Flags;
		OldLCData->IsNonZeroExtent |= NewLCData->IsNonZeroExtent;

		if (NewLCData->LineCheckObjsMap.Num() > 0)
		{
			for (TMap<const FName, LineCheckTracker::FLineCheckData::LineCheckObj>::TConstIterator It(NewLCData->LineCheckObjsMap); It; ++It)
			{
				const LineCheckTracker::FLineCheckData::LineCheckObj &NewObj = It.Value();

				LineCheckTracker::FLineCheckData::LineCheckObj * OldObj = OldLCData->LineCheckObjsMap.Find(NewObj.ObjectName);
				if (OldObj)
				{
					OldObj->Count += NewObj.Count;
				}
				else
				{
					OldLCData->LineCheckObjsMap.Set(NewObj.ObjectName, NewObj);
				}
			}
		}
	}
}

/** After the stack tracker reports a given stack trace, it calls this function
*  which appends data particular to line checks
*/
static void LineCheckReportFn(const FStackTracker::FCallStack& CallStack, QWORD TotalStackCount, FOutputDevice& Ar)
{
	//Output to a csv file any relevant data
	LineCheckTracker::FLineCheckData* const LCData = static_cast<LineCheckTracker::FLineCheckData*>(CallStack.UserData);
	if (LCData)
	{
		FString UserOutput = LINE_TERMINATOR TEXT(",,,");
		UserOutput += (LCData->IsNonZeroExtent ? TEXT( "NonZeroExtent") : TEXT("ZeroExtent"));

		if (LCData->Flags & TRACE_StopAtAnyHit)
		{
			UserOutput += TEXT(" TRACE_StopAtAnyHit");
		}
		else if (LCData->Flags & TRACE_SingleResult)
		{
			UserOutput += TEXT(" TRACE_SingleResult");
		}
		else if (LCData->Flags & TRACE_ComplexCollision)
		{
			UserOutput += TEXT(" TRACE_ComplexCollision");
		}   
		else if (LCData->Flags & TRACE_AllComponents)
		{
			UserOutput += TEXT(" TRACE_AllComponents");
		}

		for (TMap<const FName, LineCheckTracker::FLineCheckData::LineCheckObj>::TConstIterator It(LCData->LineCheckObjsMap); It; ++It)
		{
			UserOutput += LINE_TERMINATOR TEXT(",,,");
			const LineCheckTracker::FLineCheckData::LineCheckObj &CurObj = It.Value();
			UserOutput += FString::Printf(TEXT("%s (%d) : %s"), *CurObj.ObjectName.ToString(), CurObj.Count, *CurObj.DetailedInfo);
		}

		UserOutput += LINE_TERMINATOR TEXT(",,,");
		
		Ar.Log(*UserOutput);
	}
}

/** Called at the beginning of each frame to check/reset spike count */
void LineCheckTracker::Tick()
{
	if(bIsTrackingEnabled && LineCheckStackTracker)
	{
		//Spike logging is enabled
		if (TraceCountForSpikeDump > 0)
		{
			//Dump if we exceeded the threshold this frame
			if (CurrentCountForSpike > TraceCountForSpikeDump)
			{
				DumpLineChecks(5);
			}
			//Reset for next frame
			ResetLineChecks();
		}

		CurrentCountForSpike = 0;
	}
}

/** Set the value which, if exceeded, will cause a dump of the line checks this frame */
void LineCheckTracker::SetSpikeMinTraceCount(INT MinTraceCount)
{
	TraceCountForSpikeDump = Max(0, MinTraceCount);
	debugf(TEXT("Line trace spike count is %d."), TraceCountForSpikeDump);
}

/** Dump out the results of all line checks called in the game since the last call to ResetLineChecks() */
void LineCheckTracker::DumpLineChecks(INT Threshold)
{
	if( LineCheckStackTracker )
	{
		const FString Filename = FString::Printf(TEXT("%sLineCheckLog-%s.csv"), *appGameLogDir(), *appSystemTimeString());
		FOutputDeviceFile OutputFile(*Filename);
		LineCheckStackTracker->DumpStackTraces( Threshold, OutputFile );
		OutputFile.TearDown();
	}

	if( LineCheckScriptStackTracker )
	{
		const FString Filename = FString::Printf(TEXT("%sScriptLineCheckLog-%s.csv"), *appGameLogDir(), *appSystemTimeString());
		FOutputDeviceFile OutputFile(*Filename);
		LineCheckScriptStackTracker->DumpStackTraces( Threshold, OutputFile );
		OutputFile.TearDown();
	}
}

/** Reset the line check stack tracker (calls appFree() on all user data pointers)*/
void LineCheckTracker::ResetLineChecks()
{
	if( LineCheckStackTracker )
	{
		LineCheckStackTracker->ResetTracking();
	}

	if( LineCheckScriptStackTracker )
	{
		LineCheckScriptStackTracker->ResetTracking();
	}
}

/** Turn line check stack traces on and off, does not reset the actual data */
void LineCheckTracker::ToggleLineChecks()
{
	bIsTrackingEnabled = !bIsTrackingEnabled;
	debugf(TEXT("Line tracing is now %s."), bIsTrackingEnabled ? TEXT("enabled") : TEXT("disabled"));
	
	CurrentCountForSpike = 0;
	if (LineCheckStackTracker == NULL)
	{
		appInitStackWalking();
		LineCheckStackTracker = new FStackTracker(LineCheckUpdateFn, LineCheckReportFn);
	}

	if (LineCheckScriptStackTracker == NULL)
	{
		LineCheckScriptStackTracker = new FScriptStackTracker();
	}

	LineCheckStackTracker->ToggleTracking();
	LineCheckScriptStackTracker->ToggleTracking();
}

/** Captures a single stack trace for a line check */
void LineCheckTracker::CaptureLineCheck(INT LineCheckFlags, const FVector* Extent, const FFrame* ScriptStackFrame, const UObject * Object)
{
	if (LineCheckStackTracker == NULL || LineCheckScriptStackTracker == NULL)
	{
		return;
	}

	if (ScriptStackFrame)
	{
		INT EntriesToIgnore = 0;
		LineCheckScriptStackTracker->CaptureStackTrace(ScriptStackFrame, EntriesToIgnore);
	}
	else
	{		   
		FLineCheckData* const LCData = static_cast<FLineCheckData*>(appMalloc(sizeof(FLineCheckData)));
		appMemset(LCData, 0, sizeof(FLineCheckData));
		LCData->Flags = LineCheckFlags;
		LCData->IsNonZeroExtent = (Extent && !Extent->IsZero()) ? TRUE : FALSE;
		FLineCheckData::LineCheckObj LCObj;
		if (Object)
		{
			LCObj = FLineCheckData::LineCheckObj(Object->GetFName(), 1, Object->GetDetailedInfo());
		}
		else
		{
			LCObj = FLineCheckData::LineCheckObj(NAME_None, 1, TEXT("Unknown"));
		}
		
		LCData->LineCheckObjsMap.Set(LCObj.ObjectName, LCObj);		

		INT EntriesToIgnore = 3;
		LineCheckStackTracker->CaptureStackTrace(EntriesToIgnore, static_cast<void*>(LCData));
		//Only increment here because execTrace() will lead to here also
		CurrentCountForSpike++;
	}
}
#endif //LINE_CHECK_TRACING

/*-----------------------------------------------------------------------------
	World/ Level/ Actor GC verification.
-----------------------------------------------------------------------------*/

/**
 * Verifies that there are no unreachable actor references. Registered as post GC callback.
 */
void VerifyNoUnreachableActorsReferenced()
{
#if VERIFY_NO_UNREACHABLE_OBJECTS_ARE_REFERENCED
	// Don't perform work in the Editor or during the exit purge.
	extern UBOOL GShouldVerifyGCAssumptions;
	if( !GIsEditor && !GExitPurge && GShouldVerifyGCAssumptions )
	{
		UBOOL bNoUnreachableReferences = TRUE;

		// Some operations can only be performed with a valid GWorld.
		if( GWorld )
		{
			// Iterate over all streaming levels and check them as well. This is a superset of the 
			// actor iterator used above as it contains levels that are loaded but not visible. We
			// can only do this if there actually is a world object.
			AWorldInfo* const WorldInfo = GWorld->GetWorldInfo();
			for( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
			{
				ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
				if( StreamingLevel && StreamingLevel->LoadedLevel )
				{
					ULevel* Level = StreamingLevel->LoadedLevel;
					// Iterate over all actors in the level and make sure neither of them references an
					// unreachable object or is unreachable itself.
					for( INT ActorIndex=0; ActorIndex<Level->Actors.Num(); ActorIndex++ )
					{
						AActor* Actor = Level->Actors(ActorIndex);
						if( Actor )
						{
							bNoUnreachableReferences = Actor->VerifyNoUnreachableReferences() && bNoUnreachableReferences;
						}
					}
				}
			}
		}
		
		// If there is no pending incremental purge it means that all ULevel objects are fair game.
		if( !UObject::IsIncrementalPurgePending() )
		{
			// Iterate over all levels in memory.
			for( TObjectIterator<ULevel> It; It; ++It )
			{
				ULevel* Level = *It;
				check( !Level->HasAnyFlags( RF_Unreachable ) );
				// Iterate over all actors in the level and make sure neither of them references an
				// unreachable object or is unreachable itself.
				for( INT ActorIndex=0; ActorIndex<Level->Actors.Num(); ActorIndex++ )
				{
					AActor* Actor = Level->Actors(ActorIndex);
					if( Actor )
					{
						bNoUnreachableReferences = Actor->VerifyNoUnreachableReferences() && bNoUnreachableReferences;
					}
				}
			}
		}
		check( bNoUnreachableReferences );
	}
#endif
}

IMPLEMENT_POST_GARBAGE_COLLECTION_CALLBACK( DUMMY_VerifyNoUnreachableActorsReferenced, VerifyNoUnreachableActorsReferenced, GCCB_POST_VerifyNoUnreachableActorsReferenced );

/*-----------------------------------------------------------------------------
	Level actor management.
-----------------------------------------------------------------------------*/


/**
 * PERF_ISSUE_FINDER
 *
 * Actors should not have more than one Collision Component. 
 *
 * Turn this on to check if actors have more than one collision component (most should not, 
 * but can easily happen accidentally)
 * Having more than one is going to cause extra collision checks.
 *
 **/
//#define PERF_DEBUG_CHECKCOLLISIONCOMPONENTS 1


/**
 * PERF_ISSUE_FINDER
 *
 * Move Actor should not take a long time to execute.  If it is then then there is probably something wrong.
 *
 * Turn this on to have the engine log out when a specific actor is taking longer than 
 * PERF_SHOW_MOVEACTOR_TAKING_LONG_TIME_AMOUNT to move.  This is a great way to catch cases where
 * collision has been enabled but it should not have been.  Or if a specific actor is doing something evil
 *
 **/
//#define PERF_SHOW_MOVEACTOR_TAKING_LONG_TIME 1
const static FLOAT PERF_SHOW_MOVEACTOR_TAKING_LONG_TIME_AMOUNT = 2.0f; // modify this value to look at larger or smaller sets of "bad" actors

// LOOKING_FOR_PERF_ISSUES
#define PERF_SHOW_MULTI_PAWN_SPAWN_FRAMES (TRUE && !FINAL_RELEASE)

#if !FINAL_RELEASE
/** Array showing names of pawns spawned this frame. */
TArray<FString>	ThisFramePawnSpawns;
#endif

//
// Create a new actor. Returns the new actor, or NULL if failure.
//
AActor* UWorld::SpawnActor
(
	UClass*			Class,
	FName			InName,
	const FVector&	Location,
	const FRotator&	Rotation,
	AActor*			Template,
	UBOOL			bNoCollisionFail,
	UBOOL			bRemoteOwned,
	AActor*			Owner,
	APawn*			Instigator,
	UBOOL			bNoFail,
	ULevel* OverrideLevel
)
{
#if ENABLE_DETAILED_SPAWNACTOR_STATS
	STAT(const DWORD SpawnActorStatID = GStatManager.FindStatIDForString(*FString::Printf(TEXT("SpawnActor: %s"), *Class->GetName())));
	SCOPE_CYCLE_COUNTER(SpawnActorStatID);
#else
	SCOPE_CYCLE_COUNTER(STAT_SpawnActorTime);
#endif

	check(CurrentLevel);
	check(GIsEditor || (CurrentLevel == PersistentLevel));
	check(GWorld == this || GIsCooking);

#if !CONSOLE
	if (!GIsGame && OverrideLevel != NULL)
	{
		appErrorfDebug(TEXT("SpawnActor failed - OverrideLevel specified outside of game (editor should set CurrentLevel)"));
		debugf(NAME_Warning, TEXT("SpawnActor failed - OverrideLevel specified outside of game (editor should set CurrentLevel)"));
		return NULL;
	}
#endif
	
	// It's not safe to call UWorld accessor functions till the world info has been spawned.
	const UBOOL bBegunPlay = HasBegunPlay();

	// Make sure this class is spawnable.
	if( !Class )
	{
		debugf( NAME_Warning, TEXT("SpawnActor failed because no class was specified") );
		return NULL;
	}
	if( Class->ClassFlags & CLASS_Deprecated )
	{
		debugf( NAME_Warning, TEXT("SpawnActor failed because class %s is deprecated"), *Class->GetName() );
		return NULL;
	}
	if( Class->ClassFlags & CLASS_Abstract )
	{
		debugf( NAME_Warning, TEXT("SpawnActor failed because class %s is abstract"), *Class->GetName() );
		return NULL;
	}
	else if( !Class->IsChildOf(AActor::StaticClass()) )
	{
		debugf( NAME_Warning, TEXT("SpawnActor failed because %s is not an actor class"), *Class->GetName() );
		return NULL;
	}
	else if( bBegunPlay && (Class->GetDefaultActor()->IsStatic() || Class->GetDefaultActor()->bNoDelete) )
	{
		debugf( NAME_Warning, TEXT("SpawnActor failed because class %s has bStatic or bNoDelete"), *Class->GetName() );
		if ( !bNoFail )
			return NULL;		
	}
	else if (Template != NULL && Template->GetClass() != Class)
	{
		debugf(NAME_Warning, TEXT("SpawnActor failed because template class (%s) does not match spawn class (%s)"), *Template->GetClass()->GetName(), *Class->GetName());
		if (!bNoFail)
		{
			return NULL;
		}
	}

#if !FINAL_RELEASE
	// Check to see if this move is illegal during this tick group
	if (InTick && TickGroup == TG_DuringAsyncWork && Class->GetDefaultActor()->bBlockActors)
	{
		debugf(NAME_Error,TEXT("Can't spawn collidable actor (%s) during async work!"),*Class->GetName());
	}
#endif

	// Use class's default actor as a template.
	if( !Template )
	{
		Template = Class->GetDefaultActor();
	}
	check(Template!=NULL);

	FVector NewLocation = Location;
	// Make sure actor will fit at desired location, and adjust location if necessary.
	if( (Template->bCollideWorld || (Template->bCollideWhenPlacing && (GetNetMode() != NM_Client))) && !bNoCollisionFail )
	{
		if (!FindSpot(Template->GetCylinderExtent(), NewLocation, Template->bCollideComplex, Template))
		{
			debugf( NAME_DevSpawn, TEXT("SpawnActor failed because of collision at the spawn location [%s] for [%s]"), *NewLocation.ToString(), *Class->GetName() );
			return NULL;
		}
	}

	ULevel* LevelToSpawnIn = OverrideLevel;
	if (LevelToSpawnIn == NULL)
	{
	// Spawn in the same level as the owner if we have one. @warning: this relies on the outer of an actor being the level.
		LevelToSpawnIn = (Owner != NULL) ? CastChecked<ULevel>(Owner->GetOuter()) : CurrentLevel;
	}
	AActor* Actor = ConstructObject<AActor>( Class, LevelToSpawnIn, InName, RF_Transactional, Template );
	check(Actor);
	if ( GUndo )
	{
		GWorld->ModifyLevel( LevelToSpawnIn );
	}
	LevelToSpawnIn->Actors.AddItem( Actor );
	if (Actor->WantsTick())
	{
		LevelToSpawnIn->TickableActors.AddItem(Actor);
	}

#if PERF_SHOW_MULTI_PAWN_SPAWN_FRAMES
	if(Actor->GetAPawn())
	{
		FString PawnName = FString::Printf(TEXT("%d: %s"), ThisFramePawnSpawns.Num(), *Actor->GetPathName());
		ThisFramePawnSpawns.AddItem(PawnName);
	}
#endif

#if defined(PERF_DEBUG_CHECKCOLLISIONCOMPONENTS) || LOOKING_FOR_PERF_ISSUES
	INT numcollisioncomponents = 0;
	for(INT ComponentIndex = 0;ComponentIndex < Actor->Components.Num();ComponentIndex++)
	{
		UActorComponent* ActorComponent = Actor->Components(ComponentIndex);
		if( ActorComponent )
		{
			UPrimitiveComponent *C = Cast<UPrimitiveComponent>( ActorComponent );
			if ( C && C->ShouldCollide() )
			{
				numcollisioncomponents++;
			}
		}

		// most pawns are going to have a CylinderComponent (Unreal Physics) and then they are also
		// going to have a SkeletalMeshComponent (per body hit detection).  So we really care about
		// cases of 3 or more  components with collision.  If we are still lots of spam then we will
		// want to increase the num we are looking for 
		if ( numcollisioncomponents > 2 )
		{
			debugf( NAME_PerfWarning, TEXT("Actor(%s) has more 3 or more components which collide: "), *Actor->GetName());

			for(INT ComponentIndex = 0;ComponentIndex < Actor->Components.Num();ComponentIndex++)
			{
				if(Actor->Components(ComponentIndex))
				{
					UPrimitiveComponent* C = Cast<UPrimitiveComponent>(Actor->Components(ComponentIndex));
					if ( C && C->ShouldCollide() )
					{
						UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(C);
						USkeletalMeshComponent* SK = Cast<USkeletalMeshComponent>(C);
						if( SM != NULL )
						{
							debugf( NAME_PerfWarning, TEXT("    -StaticMesh with collision:  %s"), *SM->StaticMesh->GetName() );
						}
						else if( SK != NULL )
						{
							debugf( NAME_PerfWarning, TEXT("    -SkeletalMesh with collision:  %s"), *SK->SkeletalMesh->GetName() );
						}
						else
						{
							debugf( NAME_PerfWarning, TEXT("    -component with collision:  %s"), *C->GetName() );
						}
					}
				}
			}
		}
	}	

	if( Actor->bCollideActors && !Actor->IsA(AProjectile::StaticClass()) && !Actor->IsA(ADroppedPickup::StaticClass()) && !Actor->IsA(APawn::StaticClass()) && !Actor->IsA(AFracturedStaticMeshPart::StaticClass()) )
	{
		debugf( NAME_PerfWarning, TEXT("Spawned: %s (%s)  it has bCollideActors set"),*Actor->GetFullName(),*Actor->GetDetailedInfo());
	}
#endif

	// Detect if the actor's collision component is not in the components array, which is invalid.
	if(Actor->CollisionComponent && Actor->Components.FindItemIndex(Actor->CollisionComponent) == INDEX_NONE)
	{
		if ( bBegunPlay )
		{
			appErrorf(TEXT("Spawned actor %s with a collision component %s that is not in the Components array."),*Actor->GetFullName(),*Actor->CollisionComponent->GetFullName());
		}
		else
		{
			debugf( NAME_Warning, TEXT("Spawned actor %s with a collision component %s that is not in the Components array."),*Actor->GetFullName(),*Actor->CollisionComponent->GetFullName() );
		}
	}

	// Set base actor properties.
	if (Actor->Tag == NAME_None)
	{
		Actor->Tag = Class->GetFName();
	}
	Actor->bTicked		= !Ticked;
	Actor->CreationTime = GetTimeSeconds();
	Actor->WorldInfo	= GetWorldInfo();

	// Set network role.
	check(Actor->Role==ROLE_Authority);
	if( bRemoteOwned )
	{
		Exchange( Actor->Role, Actor->RemoteRole );
	}

	// Set the actor's location and rotation.
	Actor->Location = NewLocation;
	Actor->Rotation = Rotation;

	// Initialize the actor's components.
	Actor->ConditionalForceUpdateComponents(FALSE,FALSE);

	// init actor's physics volume
	Actor->PhysicsVolume = GetWorldInfo()->PhysicsVolume;

	// Set owner.
	Actor->SetOwner( Owner );

	// Set instigator
	Actor->Instigator = Instigator;

	// Initialise physics if we are in the game.
	if (bBegunPlay)
	{
		Actor->InitRBPhys();
	}

	// Send messages.
	if ( !GIsCooking )
	{
		Actor->InitExecution();
		Actor->Spawned();
	}
	if(bBegunPlay)
	{
		Actor->PreBeginPlay();
		
		if( Actor->bDeleteMe && !bNoFail )
		{
			return NULL;
		}

		for(INT ComponentIndex = 0;ComponentIndex < Actor->Components.Num();ComponentIndex++)
		{
			if(Actor->Components(ComponentIndex))
			{
				Actor->Components(ComponentIndex)->ConditionalBeginPlay();
			}
		}
	}

	// Check for encroachment.
	if( !bNoCollisionFail )
	{
		if( CheckEncroachment( Actor, Actor->Location, Actor->Rotation, 1 ) )
		{
			debugf(NAME_DevSpawn, TEXT("SpawnActor destroyed [%s] after spawning because it was encroaching on another Actor"), *Actor->GetName());
			DestroyActor( Actor );
			return NULL;
		}
	}
	else if ( Actor->bCollideActors )
	{
		Actor->FindTouchingActors();
		
		if( Actor->bDeleteMe && !bNoFail )
		{
			return NULL;
		}
	}

	if(bBegunPlay)
	{
		Actor->PostBeginPlay();
		
		if( Actor->bDeleteMe && !bNoFail )
		{
			return NULL;
		}
	}

	// Success: Return the actor.
	if( InTick )
	{
		NewlySpawned.AddItem( Actor );
	}

	if ( !bBegunPlay )
	{
		// Set bDeleteMe to true so that when the initial undo record is made,
		// the actor will be treated as destroyed, in that undo an add will
		// actually work
		Actor->bDeleteMe = 1;
		Actor->Modify( FALSE );
		Actor->bDeleteMe = 0;
	}
#if WITH_FACEFX
	else
	{
		APawn* ActorPawn = Cast<APawn>(Actor);
		ASkeletalMeshActor *ActorSkelMesh = Cast<ASkeletalMeshActor>(Actor);
		if(ActorPawn || ActorSkelMesh)
		{
			GWorld->MountPersistentFaceFXAnimSetOnActor(Actor);
		}
	}
#endif	//#if WITH_FACEFX

	// Notify the texture streaming manager about the new actor.
	GStreamingManager->NotifyActorSpawned( Actor );

	return Actor;
}

//
// Spawn a brush.
//
ABrush* UWorld::SpawnBrush()
{
	ABrush* Result = (ABrush*)SpawnActor( ABrush::StaticClass() );
	check(Result);
	return Result;
}

/**
 * Wrapper for DestroyActor() that should be called in the editor.
 *
 * @param	bShouldModifyLevel		If TRUE, Modify() the level before removing the actor.
 */
UBOOL UWorld::EditorDestroyActor( AActor* ThisActor, UBOOL bShouldModifyLevel )
{
	check(ThisActor);
	check(ThisActor->IsValid());

	if ( ThisActor->IsA(ANavigationPoint::StaticClass()) )
	{
		if ( GetWorldInfo()->bPathsRebuilt )
		{
			debugf(TEXT("EditorDestroyActor Clear paths rebuilt"));
		}
		GetWorldInfo()->bPathsRebuilt = FALSE;
	}

#if USE_MASSIVE_LOD
	// request a reattach later (this is used with LOD parenting so that if we delete a parent, the children will be 
	// reattached properly back into the octree since a frame later the replacement primitive will be definitely NULL)
	// and by delaying it this way, if 1000 actors are deleted, it only reattaches once, and GC can heppen to make
	// sure the ReplacementPrimtive pointer is NULLed out (which the render thread needs to know that it shouldn't
	// be parented)
	if (bEditorHasMassiveLOD)
	{
		GEngine->bHasPendingGlobalReattach = TRUE;
	}
#endif

	return DestroyActor( ThisActor, FALSE, bShouldModifyLevel );
}

/**
 * Removes the actor from its level's actor list and generally cleans up the engine's internal state.
 * What this function does not do, but is handled via garbage collection instead, is remove references
 * to this actor from all other actors, and kill the actor's resources.  This function is set up so that
 * no problems occur even if the actor is being destroyed inside its recursion stack.
 *
 * @param	ThisActor				Actor to remove.
 * @param	bNetForce				[opt] Ignored unless called during play.  Default is FALSE.
 * @param	bShouldModifyLevel		[opt] If TRUE, Modify() the level before removing the actor.  Default is TRUE.
 * @return							TRUE if destroy, FALSE if actor couldn't be destroyed.
 */
UBOOL UWorld::DestroyActor( AActor* ThisActor, UBOOL bNetForce, UBOOL bShouldModifyLevel )
{
	check(ThisActor);
	check(ThisActor->IsValid());
	//debugf( NAME_Log, "Destroy %s", *ThisActor->GetClass()->GetName() );

#if !FINAL_RELEASE
	// Check to see if this move is illegal during this tick group
	if (InTick && TickGroup == TG_DuringAsyncWork && ThisActor->bBlockActors)
	{
		debugf(NAME_Error,TEXT("Can't destroy collidable actor (%s) during async work!"),*ThisActor->GetName());
	}
#endif

	// In-game deletion rules.
	if( HasBegunPlay() )
	{
		// Can't kill bStatic and bNoDelete actors during play.
		if( ThisActor->IsStatic() || ThisActor->bNoDelete )
		{
			return FALSE;
		}

		// If already on list to be deleted, pretend the call was successful.
		if( ThisActor->bDeleteMe )
		{
			return TRUE;
		}

		// Can't kill if wrong role.
		if( ThisActor->Role!=ROLE_Authority && !bNetForce && !ThisActor->bNetTemporary )
		{
			return FALSE;
		}

		// Don't destroy player actors.
		APlayerController* PC = ThisActor->GetAPlayerController();
		if ( PC )
		{
			UNetConnection* C = Cast<UNetConnection>(PC->Player);
			if( C )
			{	
				if( C->Channels[0] && C->State!=USOCK_Closed )
				{
					PC->bPendingDestroy = true;
					C->Channels[0]->Close();
				}
				return FALSE;
			}
		}
	}
	else
	{
		ThisActor->Modify();
	}
	ThisActor->bPendingDelete = true;

	// check for a destroyed sequence event
	for (INT Idx = 0; Idx < ThisActor->GeneratedEvents.Num(); Idx++)
	{
		USeqEvent_Destroyed *Evt = Cast<USeqEvent_Destroyed>(ThisActor->GeneratedEvents(Idx));
		if (Evt != NULL)
		{
			Evt->CheckActivate(ThisActor,ThisActor);
		}
	}

	// Notify the texture streaming manager about the destruction of this actor.
	GStreamingManager->NotifyActorDestroyed( ThisActor );

	// Terminate any physics engine stuff for this actor right away.
	ThisActor->TermRBPhys(NULL);

	// Send EndState notification.
	if( ThisActor->GetStateFrame() && ThisActor->GetStateFrame()->StateNode )
	{
		ThisActor->eventEndState(NAME_None);
		if( ThisActor->bDeleteMe )
		{
			return TRUE;
		}
	}
	// Tell this actor it's about to be destroyed.
	ThisActor->eventDestroyed();
	ThisActor->PostScriptDestroyed();
	// Remove from base.
	if( ThisActor->Base )
	{
		ThisActor->SetBase( NULL );
		if( ThisActor->bDeleteMe )
		{
			return TRUE;
		}
	}
	
	// Make a copy of the array, as calling SetBase might change the contents of the array.
	TArray<AActor*> AttachedCopy = ThisActor->Attached;
	for( INT AttachmentIndex=0; AttachmentIndex < AttachedCopy.Num(); AttachmentIndex++ )
	{
		AActor* AttachedActor = AttachedCopy(AttachmentIndex);
		if( AttachedActor && AttachedActor->Base == ThisActor && !AttachedActor->bDeleteMe )
		{
			AttachedActor->SetBase( NULL );
		}
	}
	// Then empty the array.
	ThisActor->Attached.Empty();

	if( ThisActor->bDeleteMe )
	{
		return TRUE;
	}

	// Clean up all touching actors.
	INT iTemp = 0;
	for ( INT i=0; i<ThisActor->Touching.Num(); i++ )
	{
		if ( ThisActor->Touching(i) && ThisActor->Touching(i)->Touching.FindItem(ThisActor, iTemp) )
		{
			ThisActor->EndTouch( ThisActor->Touching(i), 1 );
			i--;
			if( ThisActor->bDeleteMe )
			{
				return TRUE;
			}
		}
	}

	// If this actor has an owner, notify it that it has lost a child.
	if( ThisActor->Owner )
	{
		ThisActor->SetOwner(NULL);
		if( ThisActor->bDeleteMe )
		{
			return TRUE;
		}
	}
	// Notify net players that this guy has been destroyed.
	if( NetDriver )
	{
		NetDriver->NotifyActorDestroyed( ThisActor );
	}

	// If demo recording, notify the demo.
	if( DemoRecDriver && !DemoRecDriver->ServerConnection )
	{
		DemoRecDriver->NotifyActorDestroyed( ThisActor );
	}

	// Remove the actor from the actor list.
	RemoveActor( ThisActor, bShouldModifyLevel );
	
	// Mark the actor and its direct components as pending kill.
	ThisActor->bDeleteMe = 1;
	ThisActor->MarkPackageDirty();
	ThisActor->MarkComponentsAsPendingKill( TRUE );

	// Clean up the actor's components.
	ThisActor->ClearComponents();

	// Invalidate the lighting cache in the Editor.  We need to check for GIsEditor as play has not begun in network game and objects get destroyed on switching levels
	if( GIsEditor && !HasBegunPlay() )
	{
		ThisActor->InvalidateLightingCache();
	}

	// Return success.
	return TRUE;
}

/**
 * Cleans up anything that needs cleaning up before the Level Transition.
 *
 * Destroys actors marked as bKillDuringLevelTransition. 
 */
void UWorld::CleanUpBeforeLevelTransition()
{
	// Unmount the PersistentFaceFXAnimSet
	SetPersistentFaceFXAnimSet(NULL);

	// Kill actors we are supposed to remove reference to during e.g. seamless map transitions.
	for( INT ActorIndex=0; ActorIndex<PersistentLevel->Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = PersistentLevel->Actors(ActorIndex);
		if( Actor && Actor->bKillDuringLevelTransition )
		{
			DestroyActor( Actor );
		}
	}
}


/*-----------------------------------------------------------------------------
	Player spawning.
-----------------------------------------------------------------------------*/

/** spawns a PlayerController and binds it to the passed in Player with the specified RemoteRole and options
 * @param Player - the Player to set on the PlayerController
 * @param RemoteRole - the RemoteRole to set on the PlayerController
 * @param URL - URL containing player options (name, etc)
 * @param UniqueId - unique net ID of the player (may be zeroed if no online subsystem or not logged in, e.g. a local game or LAN match)
 * @param Error (out) - if set, indicates that there was an error - usually is set to a property from which the calling code can look up the actual message
 * @param InNetPlayerIndex (optional) - the NetPlayerIndex to set on the PlayerController
 * @return the PlayerController that was spawned (may fail and return NULL)
 */
APlayerController* UWorld::SpawnPlayActor(UPlayer* Player, ENetRole RemoteRole, const FURL& URL, const FUniqueNetId& UniqueId, FString& Error, BYTE InNetPlayerIndex)
{
	Error = TEXT("");

	// Make the option string.
	FString Options;
	for (INT i = 0; i < URL.Op.Num(); i++)
	{
		Options += TEXT('?');
		Options += URL.Op(i);
	}

	// Tell UnrealScript to log in.
	APlayerController* Actor = GetGameInfo()->eventLogin(*URL.Portal, Options, UniqueId, Error);
	if (Actor == NULL)
	{
		debugf( NAME_Warning, TEXT("Login failed: %s"), *Error);
		return NULL;
	}

	// Possess the newly-spawned player.
	Actor->NetPlayerIndex = InNetPlayerIndex;
	Actor->SetPlayer(Player);
	//debugf(TEXT("%s got player %s"), *Actor->GetName(), *Player->GetName());
	Actor->Role = ROLE_Authority;
	Actor->RemoteRole = RemoteRole;
	GetGameInfo()->eventPostLogin(Actor);

	return Actor;
}

/*-----------------------------------------------------------------------------
	Level actor moving/placing.
-----------------------------------------------------------------------------*/

/** CheckSlice() used by FindSpot() */
UBOOL UWorld::CheckSlice(FVector& Location, const FVector& Extent, INT& bKeepTrying, AActor* TestActor)
{
	FCheckResult Hit(1.f);
	FVector SliceExtent = Extent;
	SliceExtent.Z = 1.f;
	bKeepTrying = 0;

	if( !EncroachingWorldGeometry(Hit, Location, SliceExtent) )
	{
		// trace down to find floor
		const FVector Down = FVector(0.f,0.f,Extent.Z);
		
		SingleLineCheck(Hit, NULL, Location - 2.f*Down, Location, TRACE_World, SliceExtent);

		FVector FloorNormal = Hit.Normal;
		if( !Hit.Actor || (Hit.Time > 0.5f) )
		{
			// assume ceiling was causing problem
			if( !Hit.Actor )
			{
				Location = Location - Down;
			}
			else
			{
				Location = Location - (2.f*Hit.Time-1.f) * Down + FVector(0.f,0.f,1.f);
			}

			if( !EncroachingWorldGeometry(Hit,Location, Extent) )
			{
				// push back up to ceiling, and return
				SingleLineCheck( Hit, NULL, Location + Down, Location, TRACE_World, Extent );
				if( Hit.Actor )
				{
					Location = Hit.Location;
				}
				return TRUE;
			}
			else
			{
				// push out from floor, try to fit
				FloorNormal.Z = 0.f;
				Location = Location + FloorNormal * Extent.X;
				return ( !EncroachingWorldGeometry( Hit,Location, Extent ) );
			}
		}
		else
		{
			// assume Floor was causing problem
			Location = Location + (0.5f-Hit.Time) * 2.f*Down + FVector(0.f,0.f,1.f);
			if( !EncroachingWorldGeometry(Hit, Location, Extent) )
			{
				return TRUE;
			}
			else
			{
				// push out from floor, try to fit
				FloorNormal.Z = 0.f;
				Location = Location + FloorNormal * Extent.X;
				return ( !EncroachingWorldGeometry(Hit,Location, Extent) );
			}
		}
	}
	bKeepTrying = 1;
	return FALSE;
}


/**
 * Find a suitable nearby location to place a collision box.
 * No suitable location will ever be found if Location is not a valid point inside the level
 */
UBOOL UWorld::FindSpot(const FVector& Extent, FVector& Location, UBOOL bUseComplexCollision, AActor* TestActor)
{
	FCheckResult Hit(1.f);

	// check if fits at desired location
	if( !EncroachingWorldGeometry(Hit, Location, Extent, bUseComplexCollision,TestActor) )
	{
		return TRUE;
	}

	if( Extent.IsZero() )
	{
		return FALSE;
	}

	FVector StartLoc = Location;

	// Check if slice fits
	INT bKeepTrying = 1;

	if( CheckSlice(Location, Extent, bKeepTrying, TestActor) )
	{
		return TRUE;
	}
	else if( !bKeepTrying )
	{
		return FALSE;
	}

	// Try to fit half-slices
	Location = StartLoc;
	FVector SliceExtent = 0.5f * Extent;
	SliceExtent.Z = 1.f;
	INT NumFit = 0;
	for (INT i=-1; i<2; i+=2)
	{
		for (INT j=-1; j<2; j+=2)
		{
			if( NumFit < 2 )
			{
				const FVector SliceOffset = FVector(0.55f*Extent.X*i, 0.55f*Extent.Y*j, 0.f);
				if( !EncroachingWorldGeometry(Hit, StartLoc+SliceOffset, SliceExtent, bUseComplexCollision) )
				{
					NumFit++;
					Location += 1.1f * SliceOffset;
				}
			}
		}
	}

	if( NumFit == 0 )
	{
		return FALSE;
	}

	// find full-sized slice to check
	if( NumFit == 1 )
	{
		Location = 2.f * Location - StartLoc;
	}

	SingleLineCheck(Hit, NULL, Location, StartLoc, TRACE_World);
	if( Hit.Actor )
	{
		return FALSE;
	}

	if (!EncroachingWorldGeometry(Hit,Location, Extent, bUseComplexCollision) || CheckSlice(Location, Extent, bKeepTrying, TestActor))
	{
		// adjust toward center
		SingleLineCheck(Hit, NULL, StartLoc + 0.2f * (StartLoc - Location), Location, TRACE_World, Extent);
		if( Hit.Actor )
		{
			Location = Hit.Location;
		}
		return TRUE;
	}
	return FALSE;
}


//
// Try to place an actor that has moved a long way.  This is for
// moving actors through teleporters, adding them to levels, and
// starting them out in levels.  The results of this function is independent
// of the actor's current location and rotation.
//
// If the actor doesn't fit exactly in the location specified, tries
// to slightly move it out of walls and such.
//
// Returns 1 if the actor has been successfully moved, or 0 if it couldn't fit.
//
// Updates the actor's Zone and PhysicsVolume.
//
UBOOL UWorld::FarMoveActor( AActor* Actor, const FVector& DestLocation, UBOOL test, UBOOL bNoCheck, UBOOL bAttachedMove )
{
	SCOPE_CYCLE_COUNTER(STAT_FarMoveActorTime);

	check(Actor!=NULL);
	if( (Actor->IsStatic() || !Actor->bMovable) && HasBegunPlay() )
		return FALSE;
	if ( test && (Actor->Location == DestLocation) )
		return TRUE;

#if !FINAL_RELEASE
	// Check to see if this move is illegal during this tick group
	if (InTick && TickGroup == TG_DuringAsyncWork && Actor->bBlockActors && !test)
	{
		debugf(NAME_Error,TEXT("Can't move collidable actor (%s) during async work!"),*Actor->GetName());
	}
#endif

	FVector prevLocation = Actor->Location;
	FVector newLocation = DestLocation;
	INT result = 1;

	if (!bNoCheck && (Actor->bCollideWorld || (Actor->bCollideWhenPlacing && (GetNetMode() != NM_Client))) )
		result = FindSpot(Actor->GetCylinderExtent(), newLocation, Actor->bCollideComplex, Actor);

	if (result && !test && !bNoCheck && !Actor->bNoEncroachCheck)
		result = !CheckEncroachment( Actor, newLocation, Actor->Rotation, 0);
	
	if( prevLocation != Actor->Location && !test && !Actor->IsEncroacher() ) // CheckEncroachment moved this actor (teleported), we're done
	{
		// todo: ensure the actor was placed back into the collision hash
		//debugf(TEXT("CheckEncroachment moved this actor, we're done!"));
		//farMoveStackCnt--;
		return result;
	}
	
	if( result )
	{
		//move based actors and remove base unles this farmove was done as a test
		if ( !test )
		{
			Actor->bJustTeleported = true;
			if ( !bAttachedMove )
				Actor->SetBase(NULL);
			for ( INT i=0; i<Actor->Attached.Num(); i++ )
			{
				if ( Actor->Attached(i) )
				{
					FarMoveActor(Actor->Attached(i), newLocation + Actor->Attached(i)->Location - prevLocation, FALSE, bNoCheck, TRUE);
				}
			}
		}
		Actor->Location = newLocation;
	}

	if (!test)
	{
		// Update any collision components.  If we are in the Tick phase, only update components with collision.
		Actor->ForceUpdateComponents( GWorld->InTick );

		// update relative location (it can change since it's a world-space offset, not local space)
		if (bAttachedMove)
		{
			if( Actor->Base && !Actor->bHardAttach && Actor->Physics != PHYS_Interpolating && !Actor->BaseSkelComponent )
			{
				Actor->RelativeLocation = Actor->Location - Actor->Base->Location;
			}
	}

		if (Actor->bCollideActors)
	{
		// touch actors
		Actor->FindTouchingActors();
	}
	}

	// Set the zone after moving, so that if a PhysicsVolumeChange or ActorEntered/ActorLeaving message
	// tries to move the actor, the hashing will be correct.
	if( result )
	{
		Actor->SetZone( test,0 );
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// MOVEACTOR PROFILING CODE

#if !FINAL_RELEASE && PERF_MOVEACTOR_STATS
extern UBOOL GShouldLogOutAFrameOfMoveActor;

/** 
 *	Class to start/stop timer when it goes outside MoveActor scope.
 *	We keep all results from different MoveActor calls until we reach the top level, and then print them all out.
 *	That way we can show totals before breakdown, and not pollute timings with log time.
 */
class FScopedMoveActorTimer
{
	/** Struct to contain one temporary MoveActor timing, until we finish entire move and can print it out. */
	struct FMoveTimer
	{
		AActor* Actor;
		FVector Delta;
		DOUBLE Time;
		INT Depth;
		UBOOL bDidLineCheck;
		UBOOL bDidEncroachCheck;
	};

	/** Array of all moves within one 'outer' move. */
	static TArray<FMoveTimer> Moves;
	/** Current depth in movement hierarchy */
	static INT Depth;

	/** Time that this scoped move started. */
	DOUBLE	StartTime;
	/** Index into Moves array to put results of this MoveActor timing. */
	INT		MoveIndex;
public:

	/** If we did a line check during this MoveActor. */
	UBOOL	bDidLineCheck;
	/** If we did an encroach check during this MoveActor. */
	UBOOL	bDidEncroachCheck;

	FScopedMoveActorTimer(AActor* Actor, const FVector& Delta)
		: StartTime(0.0)
	    , MoveIndex(-1)
	    , bDidLineCheck(FALSE)
		, bDidEncroachCheck(FALSE)
	{
		if(GShouldLogOutAFrameOfMoveActor)
		{
			// Add new entry to temp results array, and save actor and current stack depth
			MoveIndex = Moves.AddZeroed(1);
			Moves(MoveIndex).Actor = Actor;
			Moves(MoveIndex).Depth = Depth;
			Moves(MoveIndex).Delta = Delta;
			Depth++;

			StartTime = appSeconds(); // Start timer.
		}
	}

	~FScopedMoveActorTimer()
	{
		if(GShouldLogOutAFrameOfMoveActor)
		{
			// Record total time MoveActor took
			const DOUBLE TakeTime = appSeconds() - StartTime;

			check(Depth > 0);
			check(MoveIndex < Moves.Num());

			// Update entry with timing results
			Moves(MoveIndex).Time = TakeTime;
			Moves(MoveIndex).bDidLineCheck = bDidLineCheck;
			Moves(MoveIndex).bDidEncroachCheck = bDidEncroachCheck;

			Depth--;

			// Reached the top of the move stack again - output what we accumulated!
			if(Depth == 0)
			{
				for(INT MoveIdx=0; MoveIdx<Moves.Num(); MoveIdx++)
				{
					const FMoveTimer& Move = Moves(MoveIdx);

					// Build indentation
					FString Indent;
					for(INT i=0; i<Move.Depth; i++)
					{
						Indent += TEXT("  ");
					}

					debugf(TEXT("MOVE%s - %s %5.2fms (%f %f %f) %d %d %s"), *Indent, *Move.Actor->GetName(), Move.Time * 1000.f, Move.Delta.X, Move.Delta.Y, Move.Delta.Z, Move.bDidLineCheck, Move.bDidEncroachCheck, *Move.Actor->GetDetailedInfo());
				}

				// Clear moves array
				Moves.Reset();
			}
		}
	}

};

// Init statics
TArray<FScopedMoveActorTimer::FMoveTimer>	FScopedMoveActorTimer::Moves;
INT											FScopedMoveActorTimer::Depth = 0;

#endif //  !FINAL_RELEASE && PERF_MOVEACTOR_STATS

//
// Tries to move the actor by a movement vector.  If no collision occurs, this function
// just does a Location+=Move.
//
// Assumes that the actor's Location is valid and that the actor
// does fit in its current Location. Assumes that the level's
// Dynamics member is locked, which will always be the case during
// a call to UWorld::Tick; if not locked, no actor-actor collision
// checking is performed.
//
// If bCollideWorld, checks collision with the world.
//
// For every actor-actor collision pair:
//
// If both have bCollideActors and bBlocksActors, performs collision
//    rebound, and dispatches Touch messages to touched-and-rebounded
//    actors.
//
// If both have bCollideActors but either one doesn't have bBlocksActors,
//    checks collision with other actors (but lets this actor
//    interpenetrate), and dispatches Touch and UnTouch messages.
//
// Returns 1 if some movement occured, 0 if no movement occured.
//
// Updates actor's Zone and PhysicsVolume.
//

UBOOL UWorld::MoveActor
(
	AActor*			Actor,
	const FVector&	Delta,
	const FRotator&	NewRotation,
	DWORD			MoveFlags,
	FCheckResult&	Hit
)
{
	SCOPE_CYCLE_COUNTER(STAT_MoveActorTime);

	check(Actor!=NULL);

#if !FINAL_RELEASE && PERF_MOVEACTOR_STATS
	FScopedMoveActorTimer MoveTimer(Actor, Delta);
#endif // !FINAL_RELEASE && PERF_MOVEACTOR_STATS

#if defined(PERF_SHOW_MOVEACTOR_TAKING_LONG_TIME) || LOOKING_FOR_PERF_ISSUES
	DWORD MoveActorTakingLongTime=0;
	CLOCK_CYCLES(MoveActorTakingLongTime);
#endif

	if ( Actor->bDeleteMe )
	{
		//debugf(TEXT("%s deleted move physics %d"),*Actor->GetName(),Actor->Physics);
		return FALSE;
	}
	if( (Actor->IsStatic() || !Actor->bMovable) && HasBegunPlay() )
		return FALSE;

	// Init CheckResult
	Hit.Actor = NULL;
	Hit.Time  = 1.f;

#if !FINAL_RELEASE
	// Check to see if this move is illegal during this tick group
	if (InTick && TickGroup == TG_DuringAsyncWork && Actor->bBlockActors)
	{
		debugf(NAME_Error,TEXT("Can't move collidable actor (%s) during async work!"),*Actor->GetName());
	}
#endif

	// Set up.
	FLOAT DeltaSize;
	FVector DeltaDir;
	if( Delta.IsZero() )
	{
		// Skip if no vector or rotation.
		if( NewRotation==Actor->Rotation && !Actor->bAlwaysEncroachCheck )
		{
			return TRUE;
		}
		DeltaSize = 0.f;
		DeltaDir = Delta;
	}
	else
	{
		DeltaSize = Delta.Size();
		DeltaDir = Delta/DeltaSize;
	}

	const UBOOL bNoFail = MoveFlags & MOVE_NoFail;
	const UBOOL bIgnoreBases = MoveFlags & MOVE_IgnoreBases;

	FMemMark Mark(GMainThreadMemStack);
	INT     MaybeTouched   = 0;
	FCheckResult* FirstHit = NULL;
	FVector FinalDelta = Delta;
	FRotator const OldRotation = Actor->Rotation;
	FVector const OldLocation = Actor->Location;
	DWORD TraceFlags = 0;

	if ( Actor->IsEncroacher() )
	{
		if( Actor->bNoEncroachCheck || !Actor->bCollideActors || bNoFail )
		{
			// Update the location.
			Actor->Location += FinalDelta;
			Actor->Rotation = NewRotation;

			// Update any collision components.  If we are in the Tick phase, only upgrade components with collision.
			// This is done before moving attached actors so they can test for encroachment based on the correct mover position.
			// It is done before touch so that components are correctly up-to-date when we call Touch events. Things like
			// Kismet's touch event do an IsOverlapping - which requires the component to be in the right location.
			// This will not fix all problems with components being out of date though - for example, attachments of an Actor are not 
			// updated at this point.
			Actor->ForceUpdateComponents( GWorld->InTick );
		}
		else 
		{
#if !FINAL_RELEASE && PERF_MOVEACTOR_STATS
			MoveTimer.bDidEncroachCheck = TRUE;
#endif // !FINAL_RELEASE && PERF_MOVEACTOR_STATS
			if( CheckEncroachment( Actor, Actor->Location + FinalDelta, NewRotation, 1 ) )
			{
				// Abort if encroachment declined.
				Mark.Pop();
				return FALSE;
			}
		}
		// if checkencroachment() doesn't fail, collision components will have been updated
	}
	else
	{
		// skip moveactor for non-blocking hard attached actors if bSkipAttachedMoves (optimization) - note this misses any touch updates
		UBOOL bSkipMove = bIgnoreBases && Actor->bHardAttach && Actor->Base && Actor->bSkipAttachedMoves && !Actor->bBlockActors;

		// Perform movement collision checking if needed for this actor.
		if( !bSkipMove &&
			(Actor->bCollideActors || Actor->bCollideWorld) &&
			Actor->CollisionComponent &&
			(DeltaSize != 0.f) )
		{
			// Check collision along the line.
			if( MoveFlags & MOVE_TraceHitMaterial )
			{
				TraceFlags |= TRACE_Material;
			}
			
			if( MoveFlags & MOVE_SingleBlocking )
			{
				TraceFlags |= TRACE_SingleResult | TRACE_Blocking;
			}
			
			if( Actor->bCollideActors )
			{
				TraceFlags |= TRACE_Pawns | TRACE_Others | TRACE_Volumes;
			}

			if( Actor->bCollideWorld )
			{
				TraceFlags |= TRACE_World;
			}

			if( Actor->bCollideComplex )
			{
				TraceFlags |= TRACE_ComplexCollision;
			}

			if( Actor->bMoveIgnoresDestruction )
			{
				TraceFlags |= TRACE_MoveIgnoresDestruction;
			}

			FVector ColCenter;

			if( Actor->CollisionComponent->IsValidComponent() )
			{
				if( !Actor->CollisionComponent->IsAttached() )
				{
					if(Actor->Base && Actor->AllComponents.FindItemIndex(Actor->CollisionComponent) == INDEX_NONE)
					{
						// This case means there was a weird cross-level attachment between two actors
						// and one is in the process of being unloaded so the collision component has been
						// detached, but not cleaned up.
						warnf(TEXT("%s is attached to %s which is in another level.  Using actor's location as collision center."), *Actor->GetName(), *Actor->Base->GetName());

						ColCenter = Actor->Location;
					}
					else
					{
					appErrorf(TEXT("%s collisioncomponent %s not initialized deleteme %d"),*Actor->GetName(), *Actor->CollisionComponent->GetName(), Actor->bDeleteMe);
				}
				}
				else
				{
				ColCenter = Actor->CollisionComponent->Bounds.Origin;
			}
			}
			else
			{
				ColCenter = Actor->Location;
			}

#if !FINAL_RELEASE && PERF_MOVEACTOR_STATS
			MoveTimer.bDidLineCheck = TRUE;
#endif // !FINAL_RELEASE && PERF_MOVEACTOR_STATS

			FirstHit = MultiLineCheck
			(
				GMainThreadMemStack,
				ColCenter + Delta,
				ColCenter,
				Actor->GetCylinderExtent()*FVector(1.001f,1.001f,1.0f), // scale up radius a bit to give us some buffer for floating point accuracy pain
				TraceFlags,
				Actor
			);

			// Handle first blocking actor.
			if( Actor->bCollideWorld || Actor->bBlockActors )
			{
				Hit = FCheckResult(1.f);
				for( FCheckResult* Test=FirstHit; Test; Test=Test->GetNext() )
				{
					if( (!bIgnoreBases || !Actor->IsBasedOn(Test->Actor)) && !Test->Actor->IsBasedOn(Actor) )
					{
						MaybeTouched = 1;
						if( Actor->IsBlockedBy(Test->Actor,Test->Component) )
						{
							Hit = *Test;
							break;
						}
					}
				}
				/* logging for stuck in collision
				if ( Hit.bStartPenetrating && Actor->GetAPawn() )
				{
					if ( Hit.Actor )
						debugf(TEXT("Started penetrating %s time %f dot %f"), *Hit.Actor->GetName(), Hit.Time, (Delta.SafeNormal() | Hit.Normal));
					else
						debugf(TEXT("Started penetrating no actor time %f dot %f"), Hit.Time, (Delta.SafeNormal() | Hit.Normal));
				}
				*/
				FinalDelta = Delta * Hit.Time;
			}
		}

		// Update the location.
		Actor->Location += FinalDelta;
		Actor->Rotation = NewRotation;

		// Update any collision components.  If we are in the Tick phase (and not in the final component update phase), only upgrade components with collision.
		// This is done before moving attached actors so they can test for encroachment based on the correct mover position.
		// It is done before touch so that components are correctly up-to-date when we call Touch events. Things like
		// Kismet's touch event do an IsOverlapping - which requires the component to be in the right location.
		// This will not fix all problems with components being out of date though - for example, attachments of an Actor are not 
		// updated at this point.
		Actor->ForceUpdateComponents( GWorld->InTick && !GWorld->bPostTickComponentUpdate );
	}

	// Move the based actors (after encroachment checking).
	if( Actor->Attached.Num() > 0 )
	{
		// Move base.
		FRotator ReducedRotation(0,0,0);

		const UBOOL bRotationChanged = (OldRotation != Actor->Rotation);
		if( bRotationChanged )
		{
			ReducedRotation = FRotator( ReduceAngle(Actor->Rotation.Pitch)	- ReduceAngle(OldRotation.Pitch),
										ReduceAngle(Actor->Rotation.Yaw)	- ReduceAngle(OldRotation.Yaw)	,
										ReduceAngle(Actor->Rotation.Roll)	- ReduceAngle(OldRotation.Roll) );
		}

		// Calculate new transform matrix of base actor (ignoring scale).
		const FRotationTranslationMatrix OldLocalToWorld(OldRotation, OldLocation);
		const FRotationTranslationMatrix NewLocalToWorld(NewRotation, Actor->Location);

		FSavedPosition* SavedPositions = NULL;

		for( INT i=0; i<Actor->Attached.Num(); i++ )
		{
			// For non-skeletal based actors. Skeletal-based actors are handled inside USkeletalMeshComponent::UpdateTransform
			AActor* Other = Actor->Attached(i);
			if ( Other && !Other->BaseSkelComponent )
			{
				SavedPositions = new(GMainThreadMemStack) FSavedPosition(Other, Other->Location, Other->Rotation, SavedPositions);

				FVector   RotMotion( 0, 0, 0 );
				FCheckResult OtherHit(1.f);
				UBOOL bMoveFailed = FALSE;
				if( Other->Physics == PHYS_Interpolating || (Other->bHardAttach && !Other->bBlockActors) )
				{
					FRotationTranslationMatrix HardRelMatrix(Other->RelativeRotation, Other->RelativeLocation);
					const FMatrix NewWorldTM = HardRelMatrix * NewLocalToWorld;
					const FVector NewWorldPos = NewWorldTM.GetOrigin();
					const FRotator NewWorldRot = Other->bIgnoreBaseRotation ? Other->Rotation : NewWorldTM.Rotator();
					MoveActor( Other, NewWorldPos - Other->Location, NewWorldRot, MOVE_IgnoreBases, OtherHit );
					bMoveFailed = (OtherHit.Time < 1.f) || (NewWorldRot != Other->Rotation);
				}
				else if ( Other->bIgnoreBaseRotation )
				{
					// move attached actor, ignoring effects of any changes in its base's rotation.
					MoveActor( Other, FinalDelta, Other->Rotation, MOVE_IgnoreBases, OtherHit );
				}
				else
				{
					FRotator finalRotation = Other->Rotation + ReducedRotation;

					UBOOL bFastAttachedMove = FALSE;

					// Pawns have special rules dictating their rotations, so we have to respect them even when based.
					APawn* OtherPawn = Cast<APawn>(Other);
					if( OtherPawn )
					{
						bFastAttachedMove = OtherPawn->bFastAttachedMove;

						FRotator PawnOldRotation = OtherPawn->Rotation;
						OtherPawn->eventUpdatePawnRotation(finalRotation);
						finalRotation = OtherPawn->Rotation;

						if( bRotationChanged )
						{
							FRotator PawnReducedRotation = 	FRotator(	ReduceAngle(finalRotation.Pitch)- ReduceAngle(PawnOldRotation.Pitch),
																		ReduceAngle(finalRotation.Yaw)	- ReduceAngle(PawnOldRotation.Yaw)	,
																		ReduceAngle(finalRotation.Roll)	- ReduceAngle(PawnOldRotation.Roll) );
							Other->UpdateBasedRotation(finalRotation, PawnReducedRotation);
						}
					}
					else
					{
						if( bRotationChanged )
						{
							Other->UpdateBasedRotation(finalRotation, ReducedRotation);
						}
					}

					if( bRotationChanged )
					{
						// Handle rotation-induced motion.
						FVector const LocalPos = OldLocalToWorld.InverseTransformFVector(Other->Location);
						FVector const NewWorldPos = NewLocalToWorld.TransformFVector(LocalPos);

						// move attached actor
						if (bFastAttachedMove)
						{
							// we're trusting no other obstacle can prevent the move here
							FarMoveActor( Other, NewWorldPos, FALSE, TRUE, TRUE );
							Other->SetRotation(finalRotation);
						}
						else
						{
							FVector const Delta = NewWorldPos - Other->Location;
							MoveActor( Other, Delta, finalRotation, MOVE_IgnoreBases, OtherHit );
					}
					}
					else
					{
					// move attached actor
						if (bFastAttachedMove)
						{
							// we're trusting no other obstacle can prevent the move here
							FarMoveActor( Other, Other->Location + FinalDelta, FALSE, TRUE, TRUE );
							Other->SetRotation(finalRotation);
						}
						else
						{
							MoveActor( Other, FinalDelta, finalRotation, MOVE_IgnoreBases, OtherHit );
						}
					}
				}

				if( !bNoFail && !bMoveFailed &&
					// If neither actor should check for encroachment, skip overlapping test
				   ((!Actor->bNoEncroachCheck || !Other->bNoEncroachCheck ) &&
					 Other->IsBlockedBy( Actor, Actor->CollisionComponent )) )
				{
					// check if encroached
					// IsOverlapping() returns false for based actors, so temporarily clear the base.
					const UBOOL bStillBased = (Other->Base == Actor);
					if ( bStillBased )
						Other->Base = NULL;
					FCheckResult OverlapHit(1.0f);
					UBOOL bStillEncroaching = Other->IsOverlapping(Actor, &OverlapHit);
					if ( bStillBased )
						Other->Base = Actor;
					if (bStillEncroaching)
					{
						bStillEncroaching = !Other->ResolveAttachedMoveEncroachment(Actor, OverlapHit);
					}

					// if encroachment declined, move back to old location
					if ( bStillEncroaching && Actor->eventEncroachingOn(Other) )
					{
						bMoveFailed = TRUE;
					}
				}
				if ( bMoveFailed )
				{
					Actor->Location -= FinalDelta;
					Actor->Rotation = OldRotation;
					Actor->ForceUpdateComponents( GWorld->InTick );
					for( FSavedPosition* Pos = SavedPositions; Pos!=NULL; Pos=Pos->GetNext() )
					{
						if ( Pos->Actor && !Pos->Actor->bDeleteMe )
						{
							MoveActor( Pos->Actor, Pos->OldLocation - Pos->Actor->Location, Pos->OldRotation, MOVE_IgnoreBases, OtherHit );
							if (bRotationChanged)
							{
								Pos->Actor->ReverseBasedRotation();
							}
						}
					}
					Mark.Pop();
					return FALSE;
				}
			}
		}
	}

	// update relative location of this actor
	if( Actor->Base && !Actor->bHardAttach && Actor->Physics != PHYS_Interpolating && !Actor->BaseSkelComponent )
	{
		Actor->RelativeLocation = Actor->Location - Actor->Base->Location;
		
		if( !Actor->Base->bWorldGeometry && (OldRotation != Actor->Rotation) )
		{
			Actor->UpdateRelativeRotation();
		}
	}

	// Handle bump and touch notifications.
	if( Hit.Actor )
	{
		// Notification that Actor has bumped against the level.
		if( Hit.Actor->bWorldGeometry )
		{
			Actor->NotifyBumpLevel(Hit.Location, Hit.Normal);
		}
		// Notify first bumped actor unless it's the level or the actor's base.
		else if( !Actor->IsBasedOn(Hit.Actor) )
		{
			// Notify both actors of the bump.
			Hit.Actor->NotifyBump(Actor, Actor->CollisionComponent, Hit.Normal);
			Actor->NotifyBump(Hit.Actor, Hit.Component, Hit.Normal);
		}
	}

	// Handle Touch notifications.
	// if there are extra components that want touch events, do more linechecks for them
	if(Actor->bCollideActors)
	{		
		for(UINT ComponentIndex = 0;ComponentIndex < (UINT)Actor->Components.Num();ComponentIndex++)
		{
			UPrimitiveComponent*	primComp = Cast<UPrimitiveComponent>(Actor->Components(ComponentIndex));

			FCheckResult* CompFirstHit = NULL;
			if(primComp && primComp->IsAttached() && primComp->CollideActors && primComp->AlwaysCheckCollision && Actor->CollisionComponent != primComp)
			{
					FVector Start,End;
					// at this point the actor's already been moved so offset from our delta
					Start = primComp->Bounds.Origin - FinalDelta;
					End = primComp->Bounds.Origin;
					TraceFlags = TRACE_Pawns | TRACE_Others | TRACE_Volumes;

					if( Actor->bCollideComplex )
					{
						TraceFlags |= TRACE_ComplexCollision;
					}

					// give primitive component a chance to have a say in its trace flags
					primComp->OverrideTraceFlagsForNonCollisionComponentChecks(TraceFlags);

#if !FINAL_RELEASE && PERF_MOVEACTOR_STATS
					MoveTimer.bDidLineCheck = TRUE;
#endif // !FINAL_RELEASE && PERF_MOVEACTOR_STATS

					CompFirstHit = MultiLineCheck
					(
						GMainThreadMemStack,
						End,
						Start,
						primComp->Bounds.BoxExtent,
						TraceFlags,
						Actor
					);

					// trigger the new touch events
					for(FCheckResult* LoopCurr=CompFirstHit;LoopCurr;LoopCurr=LoopCurr->GetNext())
					{
						if ( (!bIgnoreBases || !Actor->IsBasedOn(LoopCurr->Actor)) &&
							(!Actor->IsBlockedBy(LoopCurr->Actor,LoopCurr->Component)) && Actor != LoopCurr->Actor)
						{
							Actor->BeginTouch(LoopCurr->Actor, LoopCurr->Component, LoopCurr->Location, LoopCurr->Normal, primComp);
						}
					}
			}
		}	
	}
	if( MaybeTouched || (!Actor->bBlockActors && !Actor->bCollideWorld && Actor->bCollideActors) )
	{
		for( FCheckResult* Test=FirstHit; Test && Test->Time<Hit.Time; Test=Test->GetNext() )
		{
			if ( (!bIgnoreBases || !Actor->IsBasedOn(Test->Actor)) &&
				(!Actor->IsBlockedBy(Test->Actor,Test->Component)) && Actor != Test->Actor)
			{
				Actor->BeginTouch(Test->Actor, Test->Component, Test->Location, Test->Normal, Test->SourceComponent);
			}
		}
	}

	// UnTouch notifications (only need to do this if we're not calling FindTouchingActors).
	Actor->UnTouchActors();

	// Set actor zone.
	Actor->SetZone(FALSE,FALSE);
	Mark.Pop();

	// Update physics 'pushing' body.
	Actor->UpdatePushBody();

#if defined(PERF_SHOW_MOVEACTOR_TAKING_LONG_TIME) || LOOKING_FOR_PERF_ISSUES
	UNCLOCK_CYCLES(MoveActorTakingLongTime);
	const FLOAT MSec = MoveActorTakingLongTime * GSecondsPerCycle * 1000.f;
	if( MSec > PERF_SHOW_MOVEACTOR_TAKING_LONG_TIME_AMOUNT )
	{
		debugf( NAME_PerfWarning, TEXT("%10f executing MoveActor for %s"), MSec, *Actor->GetFullName() );
	}
#endif

	// Return whether we moved at all.
	return Hit.Time>0.f;
}

/** notification when actor has bumped against the level */
void AActor::NotifyBumpLevel(const FVector &HitLocation, const FVector &HitNormal)
{
}

void AActor::NotifyBump(AActor *Other, UPrimitiveComponent* OtherComp, const FVector &HitNormal)
{
	eventBump(Other, OtherComp, HitNormal);
}

void APawn::NotifyBump(AActor *Other, UPrimitiveComponent* OtherComp, const FVector &HitNormal)
{
	if( !Controller || !Controller->eventNotifyBump(Other, HitNormal) )
	{
		eventBump(Other, OtherComp, HitNormal);
	}
}

/**
  * Only call bump event if other is a pawn
  */
void AKActorFromStatic::NotifyBump(AActor *Other, UPrimitiveComponent* OtherComp, const FVector &HitNormal)
{
	if ( Other && Other->GetAPawn() )
	{
		eventBump(Other, OtherComp, HitNormal);
	}
}

/*-----------------------------------------------------------------------------
	Encroachment.
-----------------------------------------------------------------------------*/

//
// Check whether Actor is encroaching other actors after a move, and return
// 0 to ok the move, or 1 to abort it.  If OKed, move is actually completed if Actor->IsEncroacher()
// 
//
UBOOL UWorld::CheckEncroachment
(
	AActor*			Actor,
	FVector			TestLocation,
	FRotator		TestRotation,
	UBOOL			bTouchNotify
)
{
	check(Actor);

	// If this actor doesn't need encroachment checking, allow the move.
	if (!Actor->bCollideActors && !Actor->IsEncroacher())
	{
		return FALSE;
	}

	// set up matrices for calculating movement caused by mover rotation
	FMatrix WorldToLocal, TestLocalToWorld;
	FVector StartLocation = Actor->Location;
	FRotator StartRotation = Actor->Rotation;
	if ( Actor->IsEncroacher() )
	{
		// get old transform
		WorldToLocal = Actor->WorldToLocal();

		// move the actor 
		if ( (Actor->Location != TestLocation) || (Actor->Rotation != TestRotation) )
		{
			Actor->Location = TestLocation;
			Actor->Rotation = TestRotation;
			Actor->ForceUpdateComponents(GWorld->InTick);
		}
		TestLocalToWorld = Actor->LocalToWorld();
	}

	FLOAT ColRadius, ColHeight;
	Actor->GetBoundingCylinder(ColRadius, ColHeight);
	const UBOOL bIsZeroExtent = (ColRadius == 0.f) && (ColHeight == 0.f);

	// Query the mover about what he wants to do with the actors he is encroaching.
	FMemMark Mark(GMainThreadMemStack);
	FCheckResult* FirstHit = Hash ? Hash->ActorEncroachmentCheck(GMainThreadMemStack, Actor, TestLocation, TestRotation, TRACE_AllColliding & (~TRACE_LevelGeometry)) : NULL;	
	for( FCheckResult* Test = FirstHit; Test!=NULL; Test=Test->GetNext() )
	{
		if
		(	Test->Actor!=Actor
		&&	!Test->Actor->bWorldGeometry
		&&  !Test->Actor->IsBasedOn(Actor)
		&& (Test->Component == NULL || (bIsZeroExtent ? Test->Component->BlockZeroExtent : Test->Component->BlockNonZeroExtent))
		&&	Actor->IsBlockedBy( Test->Actor, Test->Component ) )
		{
			UBOOL bStillEncroaching = TRUE;
			// Actors can be pushed by movers or karma stuff.
			if (Actor->IsEncroacher() && !Test->Actor->IsEncroacher() && Test->Actor->bPushedByEncroachers)
			{
				// check if mover can safely push encroached actor
				// Move test actor away from mover
				FVector MoveDir = TestLocation - StartLocation;
				//FVector OldLoc = Test->Actor->Location;
				//FVector Dest = Test->Actor->Location + MoveDir;
				if ( TestRotation != StartRotation )
				{
					FVector TestLocalLoc = WorldToLocal.TransformFVector(Test->Actor->Location);
					// multiply X 1.5 to account for max bounding box center to colliding edge dist change
					MoveDir += 1.5f * (TestLocalToWorld.TransformFVector(TestLocalLoc) - Test->Actor->Location);
				}
				// temporarily turn off blocking for encroacher, so it won't affect the movement
				const UBOOL bRealBlockActors = Actor->bBlockActors;
				Actor->bBlockActors = FALSE;
				Test->Actor->moveSmooth(MoveDir);

				// see if mover still encroaches test actor
				FCheckResult TestHit(1.f);
				bStillEncroaching = Test->Actor->IsOverlapping(Actor, &TestHit);
				if ( bStillEncroaching && Test->Actor->GetAPawn() )
				{
					// try moving away
					FCheckResult Hit(1.f);
					Actor->ActorLineCheck(Hit, Actor->Location, Test->Actor->Location, FVector(0.f), TRACE_AllColliding);
					if ( Hit.Time < 1.f )
					{
						FVector PushOut = (Hit.Normal + FVector(0.f,0.f,0.1f)) * Test->Actor->GetAPawn()->CylinderComponent->CollisionRadius;
						Test->Actor->moveSmooth(PushOut);
						MoveDir += PushOut;
						bStillEncroaching = Test->Actor->IsOverlapping(Actor, &TestHit);
					}
				}
				Actor->bBlockActors = bRealBlockActors;
				if ( !bStillEncroaching ) //push test actor back toward brush
				{
					MoveActor( Test->Actor, -1.f * MoveDir, Test->Actor->Rotation, 0, TestHit );
				}
				Test->Actor->PushedBy(Actor);
			}
			if ( bStillEncroaching && Actor->eventEncroachingOn(Test->Actor) )
			{
				Mark.Pop();
			
				// move the encroacher back
				Actor->Location = StartLocation;
				Actor->Rotation = StartRotation;

				const UBOOL bTransformOnly = TRUE;
				Actor->ForceUpdateComponents(GWorld->InTick, bTransformOnly);
				return TRUE;
			}
			else 
			{
				Actor->eventRanInto(Test->Actor);
			}
		}
	}

	// If bTouchNotify, send Touch and UnTouch notifies.
	if( bTouchNotify )
	{
		// UnTouch notifications.
		Actor->UnTouchActors();
	}

	// Notify the encroached actors but not the level.
	for( FCheckResult* Test = FirstHit; Test; Test=Test->GetNext() )
		if
		(	Test->Actor!=Actor
		&&	!Test->Actor->bWorldGeometry
		&&  !Test->Actor->IsBasedOn(Actor)
		&&	Test->Actor!=GetWorldInfo()
		&& (Test->Component == NULL || (bIsZeroExtent ? Test->Component->BlockZeroExtent : Test->Component->BlockNonZeroExtent)) )
		{
			if( Actor->IsBlockedBy(Test->Actor,Test->Component) )
			{
				Test->Actor->eventEncroachedBy(Actor);
			}
			else if (bTouchNotify)
			{
				// Make sure Test->Location is not Zero, if that's the case, use TestLocation
				const FVector	HitLocation = Test->Location.IsZero() ? TestLocation : Test->Location;

				// Make sure we have a valid Normal
				FVector NormalDir = Test->Normal.IsZero() ? (TestLocation - Actor->Location) : Test->Normal;
				if( !NormalDir.IsZero() )
				{
					NormalDir.Normalize();
				}
				else
				{
					NormalDir = TestLocation - Test->Actor->Location;
					NormalDir.Normalize();
				}
				Actor->BeginTouch( Test->Actor, Test->Component, HitLocation, NormalDir, Test->SourceComponent );
			}
		}
							
	Mark.Pop();

	// Ok the move.
	return FALSE;
}

/*-----------------------------------------------------------------------------
	SinglePointCheck.
-----------------------------------------------------------------------------*/

//
// Check for nearest hit.
// Return 1 if no hit, 0 if hit.
//
UBOOL UWorld::SinglePointCheck( FCheckResult&	Hit, const FVector& Location, const FVector& Extent, DWORD TraceFlags )
{
	FMemMark Mark(GMainThreadMemStack);
	FCheckResult* Hits = MultiPointCheck( GMainThreadMemStack, Location, Extent, TraceFlags );
	if( !Hits )
	{
		Mark.Pop();
		return TRUE;
	}
	Hit = *Hits;
	for( Hits = Hits->GetNext(); Hits!=NULL; Hits = Hits->GetNext() )
		if( (Hits->Location-Location).SizeSquared() < (Hit.Location-Location).SizeSquared() )
			Hit = *Hits;
	Mark.Pop();
	return FALSE;
}

/*
  EncroachingWorldGeometry
  return true if Extent encroaches on level, terrain, or bWorldGeometry actors
*/
UBOOL UWorld::EncroachingWorldGeometry( FCheckResult& Hit, const FVector& Location, const FVector& Extent, UBOOL bUseComplexCollision, AActor* TestActor )
{
	FMemMark Mark(GMainThreadMemStack);
	const FCheckResult* const Hits = MultiPointCheck( GMainThreadMemStack, Location, Extent, TRACE_World | TRACE_Blocking | TRACE_StopAtAnyHit | (bUseComplexCollision ? TRACE_ComplexCollision : 0) );
	if ( !Hits )
	{
		Mark.Pop();
		return FALSE;
	}
	Hit = *Hits;
	Mark.Pop();

	if ( TestActor == NULL )
	{
		return TRUE;
	}

	// Make sure that hit component actually blocks TestActor
	for( FCheckResult* Test = &Hit; Test!=NULL; Test=Test->GetNext() )
	{
		if ( Test->Actor!=TestActor && TestActor->IsBlockedBy( Test->Actor, Test->Component ) )
		{
			return TRUE;
		}
	}
	return FALSE;
}


/*-----------------------------------------------------------------------------
	MultiPointCheck.
-----------------------------------------------------------------------------*/

FCheckResult* UWorld::MultiPointCheck( FMemStack& Mem, const FVector& Location, const FVector& Extent, DWORD TraceFlags )
{
	check(Hash);
	FCheckResult* Result=NULL;

	if(this->bShowPointChecks)
	{
		// Draw box showing extent of point check.
		DrawWireBox(LineBatcher,FBox(Location-Extent, Location+Extent), FColor(0, 128, 255), SDPG_World);
	}

	// Check with level.
	if( TraceFlags & TRACE_Level )
	{
		FCheckResult TestHit(1.f);
		if( BSPPointCheck( TestHit, NULL, Location, Extent )==0 )
		{
			// Hit.
			TestHit.GetNext() = Result;
			Result            = new(Mem)FCheckResult(TestHit);
			Result->Actor     = GetWorldInfo();
			if( TraceFlags & TRACE_StopAtAnyHit )
			{
				return Result;
			}
		}
	}

	// Check with actors.
	FCheckResult* ActorCheckResult = Hash->ActorPointCheck( Mem, Location, Extent, TraceFlags );
	if (Result != NULL)
	{
		// Link the actor hit in after the world
		Result->Next = ActorCheckResult;
	}
	else
	{
		Result = ActorCheckResult;
	}

	return Result;
}

/*-----------------------------------------------------------------------------
	SingleLineCheck.
-----------------------------------------------------------------------------*/

DECLARE_CYCLE_STAT(TEXT("Single Line Check"),	STAT_SingleLineCheck,	STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("Multi Line Check"),	STAT_MultiLineCheck,	STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("Check Level"),			STAT_Col_Level,			STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("Check Actors"),		STAT_Col_Actors,		STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("Check Sort"),			STAT_Col_Sort,			STATGROUP_Collision);

//
// Trace a line and return the first hit actor (Actor->bWorldGeometry means hit the world geomtry).
//
UBOOL UWorld::SingleLineCheck
(
	FCheckResult&		Hit,
	AActor*				SourceActor,
	const FVector&		End,
	const FVector&		Start,
	DWORD				TraceFlags,
	const FVector&		Extent,
	ULightComponent*	SourceLight
)
{
	SCOPE_CYCLE_COUNTER(STAT_SingleLineCheck);

	// Get list of hit actors.
	FMemMark Mark(GMainThreadMemStack);

	TraceFlags = TraceFlags | TRACE_SingleResult;
	FCheckResult* FirstHit = MultiLineCheck
	(
		GMainThreadMemStack,
		End,
		Start,
		Extent,
		TraceFlags,
		SourceActor,
		SourceLight
	);

	if( FirstHit != NULL )
	{
		Hit = *FirstHit;

		Hit.PhysMaterial = DetermineCorrectPhysicalMaterial( Hit );

		Hit.Material = Hit.Material ? Hit.Material->GetMaterial() : NULL;
	
	}
	else
	{
		Hit.Time = 1.f;
		Hit.Actor = NULL;
	}

	Mark.Pop();
	return FirstHit==NULL;
}

/*-----------------------------------------------------------------------------
	MultiLineCheck.
-----------------------------------------------------------------------------*/

FCheckResult* UWorld::MultiLineCheck
(
	FMemStack&			Mem,
	const FVector&		End,
	const FVector&		Start,
	const FVector&		Extent,
	DWORD				TraceFlags,
	AActor*				SourceActor,
	ULightComponent*	SourceLight
)
{
	SCOPE_CYCLE_COUNTER(STAT_MultiLineCheck);

	//If enabled, capture the callstack that triggered this linecheck
	LINE_CHECK_TRACE(TraceFlags, &Extent, SourceActor);

	INT NumHits=0;
	FCheckResult Hits[64];

#if !FINAL_RELEASE
	// Draw line that we are checking, and box showing extent at end of line, if non-zero
	if(this->bShowLineChecks && Extent.IsZero())
	{
		LineBatcher->DrawLine(Start, End, FColor(0, 255, 128), SDPG_World);
		
	}
	else if(this->bShowExtentLineChecks && !Extent.IsZero())
	{
		LineBatcher->DrawLine(Start, End, FColor(0, 255, 255), SDPG_World);
		DrawWireBox(LineBatcher,FBox(End-Extent, End+Extent), FColor(0, 255, 255), SDPG_World);
	}
#endif

	FLOAT Dilation = 1.f;
	FVector NewEnd = End;

	// Check for collision with the level, and cull by the end point for speed.
	
	{
		SCOPE_CYCLE_COUNTER(STAT_Col_Level);
		if( (TraceFlags & TRACE_Level) && BSPLineCheck( Hits[NumHits], NULL, End, Start, Extent, TraceFlags )==0 )
		{
			Hits[NumHits].Actor = GetWorldInfo();
			const FLOAT Dist = (Hits[NumHits].Location - Start).Size();
			Hits[NumHits].Time *= Dilation;
			Dilation = ::Min(1.f, Hits[NumHits].Time * (Dist + 5)/(Dist+0.0001f));
			NewEnd = Start + (End - Start) * Dilation;
			NumHits++;
		}
	}

	if(Dilation > SMALL_NUMBER)
	{
		{
			SCOPE_CYCLE_COUNTER(STAT_Col_Actors);
			if( !NumHits || !(TraceFlags & TRACE_StopAtAnyHit) )
			{
				// Check with actors.
				if( (TraceFlags & TRACE_Hash) && Hash )
				{
					for( FCheckResult* Link=Hash->ActorLineCheck( Mem, NewEnd, Start, Extent, TraceFlags, SourceActor, SourceLight ); Link && NumHits<ARRAY_COUNT(Hits); Link=Link->GetNext() )
					{
						Link->Time *= Dilation;
						Hits[NumHits++] = *Link;
					}
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


/*-----------------------------------------------------------------------------
	BSP line/ point checking
-----------------------------------------------------------------------------*/

UBOOL UWorld::BSPLineCheck(	FCheckResult& Hit, AActor* Owner, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags )
{
	UBOOL bHit = FALSE;
	FCheckResult TotalResult(1.f);

	// Iterate over each level in the World
	for( INT LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
	{
		ULevel* const Level = Levels(LevelIndex);
		FCheckResult TmpResult(1.f);

		// Test against this level's BSP
		Level->Model->LineCheck( TmpResult, Owner, NULL, End, Start, Extent, TraceFlags );
		
		// If this hit is the closest one so far, remember it.
		if(TmpResult.Time < TotalResult.Time)
		{
			TotalResult = TmpResult;
			TotalResult.Level = Level;
			TotalResult.LevelIndex = LevelIndex;
			bHit = TRUE;
		}
	}

	// If we got a valid hit- set the output CheckResult.
	if(bHit)
	{
		Hit = TotalResult;
	}

	// return hit condition
	return !bHit;
}

UBOOL UWorld::BSPFastLineCheck( const FVector& End, const FVector& Start )
{
	UBOOL bBlocked  = FALSE;

	for( INT LevelIndex=0; LevelIndex<Levels.Num() && !bBlocked ; LevelIndex++ )
	{
		ULevel* const Level = Levels(LevelIndex);
		bBlocked = !Level->Model->FastLineCheck( End, Start );
	}

	return !bBlocked;
}

UBOOL UWorld::BSPPointCheck( FCheckResult &Result, AActor *Owner, const FVector& Location, const FVector& Extent )
{
	UBOOL bBlocked = FALSE;

	for( INT LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
	{
		ULevel* const Level = Levels(LevelIndex);
		bBlocked = !Level->Model->PointCheck( Result, Owner, NULL, Location, Extent );		
		// if the check hit the level,
		if (bBlocked)
		{
			// update in the check result
			Result.Level = Level;
			Result.LevelIndex = LevelIndex;
			// and sotp furthing searching
			break;
		}
	}

	return !bBlocked;
}

/*-----------------------------------------------------------------------------
	ULevel zone functions.
-----------------------------------------------------------------------------*/

//
// Figure out which zone an actor is in, update the actor's iZone,
// and notify the actor of the zone change.  Skips the zone notification
// if the zone hasn't changed.
//

void AActor::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
	if( bDeleteMe )
	{
		return;
	}

	// update physics volume
	APhysicsVolume *NewVolume = GWorld->GetWorldInfo()->GetPhysicsVolume(Location,this,bCollideActors && !bTest && !bForceRefresh);
	if( !bTest )
	{
		if( NewVolume != PhysicsVolume )
		{
			if( PhysicsVolume )
			{
				PhysicsVolume->eventActorLeavingVolume(this);
				eventPhysicsVolumeChange(NewVolume);
			}
			PhysicsVolume = NewVolume;
			PhysicsVolume->eventActorEnteredVolume(this);
		}
	}
	else
	{
		PhysicsVolume = NewVolume;
	}
}

void APhysicsVolume::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
	if( bDeleteMe )
	{
		return;
	}

	PhysicsVolume = this;

	Register();
}

void APawn::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
	if( bDeleteMe )
	{
		return;
	}

	// update physics volume
	APhysicsVolume *NewVolume = GWorld->GetWorldInfo()->GetPhysicsVolume(Location,this,bCollideActors && !bTest && !bForceRefresh);
	APhysicsVolume *NewHeadVolume = GWorld->GetWorldInfo()->GetPhysicsVolume(Location + FVector(0,0,BaseEyeHeight),this,bCollideActors && !bTest && !bForceRefresh);
	if ( NewVolume != PhysicsVolume )
	{
		if ( !bTest )
		{
			if ( PhysicsVolume )
			{
				PhysicsVolume->eventPawnLeavingVolume(this);
				eventPhysicsVolumeChange(NewVolume);
			}
			if ( Controller )
				Controller->eventNotifyPhysicsVolumeChange( NewVolume );
		}
		PhysicsVolume = NewVolume;
		if ( !bTest )
			PhysicsVolume->eventPawnEnteredVolume(this);
	}
	if ( NewHeadVolume != HeadVolume )
	{
		if ( !bTest && (!Controller || !Controller->eventNotifyHeadVolumeChange(NewHeadVolume)) )
		{
			eventHeadVolumeChange(NewHeadVolume);
		}
		HeadVolume = NewHeadVolume;
	}
	checkSlow(PhysicsVolume);
}

void AWorldInfo::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
}

// init actor volumes
void AVolume::SetVolumes(const TArray<AVolume*>& Volumes)
{
}
void AWorldInfo::SetVolumes(const TArray<AVolume*>& Volumes)
{
}
void AActor::SetVolumes(const TArray<AVolume*>& Volumes)
{
	for( INT i=0; i<Volumes.Num(); i++ )
	{
		AVolume*		V = Volumes(i);

		// blocking volumes can't touch
		if( V && !V->bBlockActors )
		{
			APhysicsVolume*	P = Cast<APhysicsVolume>(V);

			if( ((bCollideActors && V->bCollideActors) || P || V->bProcessAllActors) && V->Encompasses(Location) )
			{
				if( bCollideActors && V->bCollideActors )
				{
					V->Touching.AddItem(this);
					Touching.AddItem(V);
				}
				if( P && (P->Priority > PhysicsVolume->Priority) )
				{
					PhysicsVolume = P;
				}
				if( V->bProcessAllActors )
				{
					V->eventProcessActorSetVolume( this );
				}
			}
		}
	}
}
void AActor::SetVolumes()
{
	for( FActorIterator It; It; ++ It )
	{
		AVolume*		V = (*It)->GetAVolume();

		// blocking volumes can't touch
		if( V && !V->bBlockActors )
		{
			APhysicsVolume*	P = Cast<APhysicsVolume>(V);
			if( V && ((bCollideActors && V->bCollideActors) || P || V->bProcessAllActors) && V->Encompasses(Location) )
			{
				if( bCollideActors && V->bCollideActors )
				{
					V->Touching.AddItem(this);
					Touching.AddItem(V);
				}
				if( P && (P->Priority > PhysicsVolume->Priority) )
				{
					PhysicsVolume = P;
				}
				if( V->bProcessAllActors )
				{
					V->eventProcessActorSetVolume( this );
				}
			}
		}
	}
}

/** 
 *	Calculates the relative transform between the actor and its base if it has one.
 *	This transform will then be enforced in the game.
 */
void AActor::EditorUpdateBase()
{
	if( Base )
	{
		// this will only happen for placed actors at level startup
		AActor* NewBase = Base;
		USkeletalMeshComponent* NewSkelComp = BaseSkelComponent;
		FName NewBaseBoneName = BaseBoneName;

		SetBase(NULL);

		// If a bone name is specified, attempt to attach to a skeletalmeshcomponent. 
		if(NewBaseBoneName != NAME_None)
		{
			// First see if we have a Base SkeletalMeshComponent specified
			USkeletalMeshComponent* BaseSkelComp = NewSkelComp;

			// Ignore BaseSkelComponent if its Owner is not the Base.
			if(BaseSkelComp && BaseSkelComp->GetOwner() != NewBase)
			{
				BaseSkelComp = NULL;
			}

			// If not, see if the CollisionComponent in the Base is a SkeletalMesh
			if(!BaseSkelComp)
			{									
				BaseSkelComp = Cast<USkeletalMeshComponent>( NewBase->CollisionComponent );
			}

			// If that failed, see if its a pawn, and use its Mesh pointer.
			APawn* Pawn = Cast<APawn>(NewBase);
			if(!BaseSkelComp && Pawn)
			{
				BaseSkelComp = Pawn->Mesh;
			}

			// If BaseSkelComp is still NULL at this point, SetBase will fail gracefully and just attach it relative to the Actor ref frame as usual.
			SetBase( NewBase, FVector(0,0,1), 0, BaseSkelComp, NewBaseBoneName );
		}
		else // Normal case - just attaching to actor.
		{
			SetBase( NewBase, FVector(0,0,1), 0 );
		}
	}

	// explicitly clear the base when referencing actors in other levels
	if ( Base && Base->GetOuter() != GetOuter() )
	{
		SetBase(NULL);
	}
}

/** 
 *	Utility for updating all actors attached to this one to satisfy their RelativeLocation/RelativeRotation. 
 *	Should only be used in the editor!
 */
void AActor::EditorUpdateAttachedActors(const TArray<AActor*>& IgnoreActors)
{
	FRotationTranslationMatrix ActorTM( Rotation, Location );

	for( INT i=0; i<Attached.Num(); i++ )
	{
		AActor* Other = Attached(i);

		// If we have an attached actor, which isn't deleted, and we are not ignoring, update its location.
		if(Other && !Other->bDeleteMe && !IgnoreActors.ContainsItem(Other))
		{
			// Matrix of parent of this Actor
			FMatrix BaseTM;

			// If its based on a SkeletalMeshComponent, use the bone matrix as the parent transform.
			if( Other->BaseSkelComponent )
			{
				const INT BoneIndex = Other->BaseSkelComponent->MatchRefBone(Other->BaseBoneName);
				if(BoneIndex != INDEX_NONE)
				{
					BaseTM = Other->BaseSkelComponent->GetBoneMatrix(BoneIndex);
					BaseTM.RemoveScaling();
				}
				// If we couldn't find the bone - just use Actor as base.
				else
				{
					BaseTM = ActorTM;
				}
			}
			// If its not attached to a bone, just use the Actor transform.
			else
			{
				BaseTM = ActorTM;
			}

			// CAlculate relative transform, and apply it
			const FRotationTranslationMatrix HardRelMatrix(Other->RelativeRotation, Other->RelativeLocation);
			const FMatrix NewWorldTM = HardRelMatrix * BaseTM;
			const FVector NewWorldPos = NewWorldTM.GetOrigin();
			const FRotator NewWorldRot = NewWorldTM.Rotator();

			// Update actors location and rotatation.
			Other->Location = NewWorldPos;
			Other->Rotation = NewWorldRot;

			Other->ForceUpdateComponents();

			// Now update anything which are based on _this_ thing.
			// @todo: Are we sure we can't get cycles?
			Other->EditorUpdateAttachedActors(IgnoreActors);
		}
	}
}

/** 
 *	Called before the Actor is saved. 
 */
void AActor::PreSave()
{
	Super::PreSave();
#if WITH_EDITORONLY_DATA
	// update the base of this actor
	EditorUpdateBase();
#endif // WITH_EDITORONLY_DATA
}

/**
 * Creates offsets for locations based on the editor grid size and active viewport.
 */
FVector AActor::CreateLocationOffset(UBOOL bDuplicate, UBOOL bOffsetLocations, INT ViewportType, FLOAT GridSize) const
{
	const FLOAT Offset = static_cast<FLOAT>( bOffsetLocations ? GridSize : 0 );
	FVector LocationOffset(Offset,Offset,Offset);
	if ( bDuplicate && ((ELevelViewportType)ViewportType != LVT_None) )
	{
		switch( (ELevelViewportType)ViewportType )
		{
		case LVT_OrthoXZ:
			LocationOffset = FVector(Offset,0.f,Offset);
			break;
		case LVT_OrthoYZ:
			LocationOffset = FVector(0.f,Offset,Offset);
			break;
		default:
			LocationOffset = FVector(Offset,Offset,0.f);
			break;
		}
	}
	return LocationOffset;
}

/**
 * Initializes this actor when play begins.  This version marks the actor as ready to execute script, but skips
 * the rest of the stuff that actors normally do in PostBeginPlay().
 */
void AStaticMeshActorBase::PostBeginPlay()
{
	bScriptInitialized = TRUE;
}




// Allow actors to initialize themselves on the C++ side
void AActor::PostBeginPlay()
{
	// Send PostBeginPlay.
	eventPostBeginPlay();

	if( bDeleteMe )
		return;

	// Init scripting.
	eventSetInitialState();

	// Find Base
	if( !Base && bCollideWorld && bShouldBaseAtStartup && ((Physics == PHYS_None) || (Physics == PHYS_Rotating)) )
	{
		FindBase();
	}
}

void AActor::PreBeginPlay()
{
	// Send PostBeginPlay.
	eventPreBeginPlay();

	if (!bDeleteMe)
	{
		// only call zone/volume change events when spawning actors during gameplay
		SetZone(!GWorld->HasBegunPlayAndNotAssociatingLevel(), TRUE);

		// Verify that the physics mode and ticking group are compatible
		if (Physics == PHYS_RigidBody && TickGroup != TG_PostAsyncWork)
		{
			debugf(TEXT("Physics mode for (%s) is not compatible with TickGroup(%d) adjusting"),
				*GetName(),TickGroup);
			SetTickGroup(TG_PostAsyncWork);
		}
	}
}

void AVolume::SetVolumes()
{
}

/** NavMesh PathConstraint and PathGoalEvaluator pools */
/**
 * Will retrieve a constraint of the given class from the cache, or create a new one and add it to the cache if none exists
 * @param ConstraintClass - class of constraint to retrieve
 * @param Requestor		  - the hanlde that needs the class
 * @return - the retrieved constraint
 */
UNavMeshPathConstraint* AWorldInfo::GetNavMeshPathConstraintFromCache(class UClass* ConstraintClass,class UNavigationHandle* Requestor)
{
	FNavMeshPathConstraintCacheDatum* Datum = NavMeshPathConstraintCache.Find(ConstraintClass);
	// if we found an existing datum, find a free instance
	if(Datum != NULL)
	{
	}
	else
	{
		FNavMeshPathConstraintCacheDatum TmpDatum(EC_EventParm);
		Datum = &NavMeshPathConstraintCache.Set(ConstraintClass,TmpDatum);

	}
	checkSlowish(Datum != NULL);

	if(Datum->ListIdx >= UCONST_MAX_INSTANCES_PER_CLASS)
	{
		checkSlowish(0 && "(CONSTRAINT) Hit path constraint constraint cache instance cap!");
		Datum->ListIdx=0;
	}

	if(Datum->List[Datum->ListIdx] == NULL)
	{
		Datum->List[Datum->ListIdx] = Cast<UNavMeshPathConstraint>(StaticConstructObject( ConstraintClass ));
	}

	UNavMeshPathConstraint* ChosenConstraint = Datum->List[Datum->ListIdx];
	Datum->ListIdx++;

	ChosenConstraint->eventRecycle();
	return ChosenConstraint;
}

/**
 * Will retrieve a path goal evaluator from the cache or create a new one if none yet exists
 * @param GoalEvalClass - class of goal to retrieve 
 * @param Requestor - handle requesting the goal evaluator
 * @return - the goal evaluator found in the cache
 */
UNavMeshPathGoalEvaluator* AWorldInfo::GetNavMeshPathGoalEvaluatorFromCache(class UClass* GoalEvalClass,class UNavigationHandle* Requestor)
{
	FNavMeshPathGoalEvaluatorCacheDatum* Datum = NavMeshPathGoalEvaluatorCache.Find(GoalEvalClass);

	// if we found an existing datum, find a free instance
	if(Datum != NULL)
	{
	}
	else
	{
		FNavMeshPathGoalEvaluatorCacheDatum TmpDatum(EC_EventParm);
		Datum = &NavMeshPathGoalEvaluatorCache.Set(GoalEvalClass,TmpDatum);
	}
	checkSlowish(Datum != NULL);

	if(Datum->ListIdx >= UCONST_MAX_INSTANCES_PER_CLASS)
	{
		checkSlowish(0 && "(GOAL) Hit path goal evaluator cache instance cap!");
		Datum->ListIdx=0;
	}

	if(Datum->List[Datum->ListIdx] == NULL)
	{
		Datum->List[Datum->ListIdx] = Cast<UNavMeshPathGoalEvaluator>(StaticConstructObject( GoalEvalClass ));
	}

	UNavMeshPathGoalEvaluator* ChosenConstraint = Datum->List[Datum->ListIdx];

	Datum->ListIdx++;

	ChosenConstraint->eventRecycle();
	return ChosenConstraint;
}

/**
 * This will release constraints/goal evals allocated to any handles back into the pool
 */
void AWorldInfo::ReleaseCachedConstraintsAndEvaluators()
{
	for(TMap< UClass*, FNavMeshPathConstraintCacheDatum >::TIterator It(NavMeshPathConstraintCache);It;++It)
	{
		FNavMeshPathConstraintCacheDatum& Datum = It.Value();
		Datum.ListIdx=0;
	}

	for(TMap< UClass*, FNavMeshPathGoalEvaluatorCacheDatum >::TIterator It(NavMeshPathGoalEvaluatorCache);It;++It)
	{
		FNavMeshPathGoalEvaluatorCacheDatum& Datum = It.Value();
		Datum.ListIdx=0;
	}
}


/**
 * Overidden to Track path constraint/goal evaluator references 
 */
void AWorldInfo::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects(ObjectArray);

	for(TMap< UClass*, FNavMeshPathConstraintCacheDatum >::TIterator It(NavMeshPathConstraintCache);It;++It)
	{
		FNavMeshPathConstraintCacheDatum& Datum = It.Value();
		for(INT Idx=0;Idx<UCONST_MAX_INSTANCES_PER_CLASS;++Idx)
		{
			if(Datum.List[Idx] != NULL)
			{
				AddReferencedObject(ObjectArray,Datum.List[Idx]);
			}
		}
	}

	for(TMap< UClass*, FNavMeshPathGoalEvaluatorCacheDatum >::TIterator It(NavMeshPathGoalEvaluatorCache);It;++It)
	{
		FNavMeshPathGoalEvaluatorCacheDatum& Datum = It.Value();
		for(INT Idx=0;Idx<UCONST_MAX_INSTANCES_PER_CLASS;++Idx)
		{
			if(Datum.List[Idx] != NULL)
			{
				AddReferencedObject(ObjectArray,Datum.List[Idx]);
			}
		}
	}
}

void AWorldInfo::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.Ver() < VER_INTEGRATED_LIGHTMASS)
	{
		bUseGlobalIllumination = FALSE;
	}

#if WITH_EDITORONLY_DATA
	if (Ar.Ver() < VER_INTEGRATED_LIGHTMASS)
	{
		// Copy the deprecated LightmassLevelSettings if they are present.
		if (LMLevelSettings_DEPRECATED != NULL)
		{
			LightmassSettings.NumIndirectLightingBounces = LMLevelSettings_DEPRECATED->NumIndirectLightingBounces;
			LightmassSettings.EnvironmentColor = LMLevelSettings_DEPRECATED->EnvironmentColor;
			LightmassSettings.EnvironmentIntensity = LMLevelSettings_DEPRECATED->EnvironmentIntensity;
			LightmassSettings.EmissiveBoost = LMLevelSettings_DEPRECATED->EmissiveBoost;
			LightmassSettings.DiffuseBoost = LMLevelSettings_DEPRECATED->DiffuseBoost;
			LightmassSettings.SpecularBoost = LMLevelSettings_DEPRECATED->SpecularBoost;
			LightmassSettings.bUseAmbientOcclusion = LMLevelSettings_DEPRECATED->bUseAmbientOcclusion;
			LightmassSettings.bVisualizeAmbientOcclusion = LMLevelSettings_DEPRECATED->bVisualizeAmbientOcclusion;
			LightmassSettings.DirectIlluminationOcclusionFraction = LMLevelSettings_DEPRECATED->DirectIlluminationOcclusionFraction;
			LightmassSettings.IndirectIlluminationOcclusionFraction = LMLevelSettings_DEPRECATED->IndirectIlluminationOcclusionFraction;
			LightmassSettings.OcclusionExponent = LMLevelSettings_DEPRECATED->OcclusionExponent;
			LightmassSettings.FullyOccludedSamplesFraction = LMLevelSettings_DEPRECATED->FullyOccludedSamplesFraction;
			LightmassSettings.MaxOcclusionDistance = LMLevelSettings_DEPRECATED->MaxOcclusionDistance;
		}
	}
#endif // WITH_EDITORONLY_DATA

	// add references to path constraints/goal evals in the pool
	if( Ar.IsObjectReferenceCollector() )
	{
		for(TMap< UClass*, FNavMeshPathConstraintCacheDatum >::TIterator It(NavMeshPathConstraintCache);It;++It)
		{
			FNavMeshPathConstraintCacheDatum& Datum = It.Value();
			for(INT Idx=0;Idx<UCONST_MAX_INSTANCES_PER_CLASS;++Idx)
			{
				if(Datum.List[Idx] != NULL)
				{
					Ar << Datum.List[Idx];
				}
			}
		}

		for(TMap< UClass*, FNavMeshPathGoalEvaluatorCacheDatum >::TIterator It(NavMeshPathGoalEvaluatorCache);It;++It)
		{
			FNavMeshPathGoalEvaluatorCacheDatum& Datum = It.Value();
			for(INT Idx=0;Idx<UCONST_MAX_INSTANCES_PER_CLASS;++Idx)
			{
				if(Datum.List[Idx] != NULL)
				{
					Ar << Datum.List[Idx];
				}
			}
		};
	}

#if WITH_EDITORONLY_DATA
	if (!Ar.IsLoading() && !Ar.IsSaving())
	{
		// To reference LandscapeInfo
		Ar << LandscapeInfoMap;
	}
#endif
}
// -- end Pathconstraint /Goaleval pooling

/**
* Called after this instance has been serialized.
*/
void AWorldInfo::PostLoad()
{
	Super::PostLoad();

	// Force to be blocking, needed for BSP.
	bBlockActors = TRUE;

	// clamp desaturation to 0..1 (fixup for old data)
	DefaultPostProcessSettings.Scene_Desaturation = Clamp(DefaultPostProcessSettings.Scene_Desaturation, 0.f, 1.f);
	ULinkerLoad* LODLinkerLoad = GetLinker();
	
	if (LODLinkerLoad && (LODLinkerLoad->Ver() < VER_COLORGRADING2))
	{
		// Before the override flag was introduced the override state was derived if the texture was actually defined.
		DefaultPostProcessSettings.bOverride_Scene_ColorGradingLUT = DefaultPostProcessSettings.ColorGrading_LookupTable != 0;
	}

	// Make sure that 'always loaded' maps are first in the array

	TArray<ULevelStreaming*> AlwaysLoadedLevels;
	// Iterate over each LevelStreaming object
	for( INT LevelIndex=StreamingLevels.Num()-1; LevelIndex>=0; LevelIndex-- )
	{
		// See if its an 'always loaded' one
		ULevelStreamingAlwaysLoaded* AlwaysLoadedLevel = Cast<ULevelStreamingAlwaysLoaded>( StreamingLevels(LevelIndex) );
		if(AlwaysLoadedLevel)
		{
			// If it is, add to out list (preserving order), and remove from main list
			AlwaysLoadedLevels.InsertItem(AlwaysLoadedLevel, 0);
			StreamingLevels.Remove(LevelIndex);
		}
	}

	// Now make new array that starts with 'always loaded' levels, followed by the rest
	TArray<ULevelStreaming*> NewStreamingLevels = AlwaysLoadedLevels;
	NewStreamingLevels.Append( StreamingLevels );
	// And use that for the new StreamingLevels array
	StreamingLevels = NewStreamingLevels;

#if WITH_APEX
	if ( GApexManager )
	{
		// NOTE: One WorldInfo will override another. Whichever WorldInfo gets loaded last, wins.
		physx::apex::NxModuleDestructible *DestructibleModule = GApexManager->GetModuleDestructible();
		if ( DestructibleModule  )
		{
			if ( DestructibleSettings.MaxChunkIslandCount >= 0 )
			{
				DestructibleModule->setMaxDynamicChunkIslandCount( DestructibleSettings.MaxChunkIslandCount );
			}
			else
			{
				DestructibleModule->setMaxDynamicChunkIslandCount( GSystemSettings.ApexDestructionMaxChunkIslandCount );
			}
			if ( DestructibleSettings.MaxShapeCount >= 0 )
			{
				DestructibleModule->setMaxChunkCount( DestructibleSettings.MaxShapeCount );
			}
			else
			{
				DestructibleModule->setMaxChunkCount( GSystemSettings.ApexDestructionMaxShapeCount );
			}

			if ( DestructibleSettings.bOverrideMaxChunkSeparationLOD )
			{
				DestructibleModule->setMaxChunkSeparationLOD( DestructibleSettings.MaxChunkSeparationLOD );
			}
			else
			{
				DestructibleModule->setMaxChunkSeparationLOD( GSystemSettings.ApexDestructionMaxChunkSeparationLOD );
			}
		}
	}
#endif

#if WITH_NOVODEX
	if( GNovodexSDK != NULL )
	{
		GNovodexSDK->setParameter(NX_SKIN_WIDTH, DefaultSkinWidth);
	}
#endif

#if WITH_EDITORONLY_DATA
	if (GetLinker() && (GetLinker()->Ver() < VER_RENAMED_GROUPS_TO_LAYERS))
	{
		VisibleLayers = VisibleGroups_DEPRECATED;
	}
#endif // WITH_EDITORONLY_DATA
}

/**
 * Called after GWorld has been set. Used to load, but not associate, all
 * levels in the world in the Editor and at least create linkers in the game.
 * Should only be called against GWorld::PersistentLevel's WorldInfo.
 */
void AWorldInfo::LoadSecondaryLevels()
{
	check( GIsEditor );

	// streamingServer
	// Only load secondary levels in the Editor, and not for commandlets.
	if( !GIsUCC
	// Don't do any work for world info actors that are part of secondary levels being streamed in! 
	&&	!GIsAsyncLoading )
	{
		for( INT LevelIndex=0; LevelIndex<StreamingLevels.Num(); LevelIndex++ )
		{
			UBOOL bLoadedLevelPackage = FALSE;
			ULevelStreaming* const StreamingLevel = StreamingLevels(LevelIndex);
			if( StreamingLevel )
			{
				// Load the package and find the world object.
				UPackage* const LevelPackage = UObject::LoadPackage( NULL, *StreamingLevel->PackageName.ToString(), LOAD_None );
				if( LevelPackage )
				{
					// Figure out whether main world is a PIE package.
					const UBOOL bIsMainWorldAPIEPackage = (GetOutermost()->PackageFlags & PKG_PlayInEditor) ? TRUE : FALSE;
					// Worlds can only refer to PIE packages if they ar a PIE package themselves.
					if( (LevelPackage->PackageFlags & PKG_PlayInEditor) && !bIsMainWorldAPIEPackage )
					{
						appErrorf( *LocalizeUnrealEd("CannotOpenPIEMapsForEditing") );
					}

					bLoadedLevelPackage = TRUE;

					// Find the world object in the loaded package.
					UWorld* const LoadedWorld	= FindObjectChecked<UWorld>( LevelPackage, TEXT("TheWorld") );

					// LoadedWorld won't be serialized as there's a BeginLoad on the stack so we manually serialize it here.
					check( LoadedWorld->GetLinker() );
					LoadedWorld->GetLinker()->Preload( LoadedWorld );

					// Keep reference to prevent garbage collection.
					check( LoadedWorld->PersistentLevel );
					StreamingLevel->LoadedLevel	= LoadedWorld->PersistentLevel;
				}
			}

			// Remove this level object if the file couldn't be found.
			if ( !bLoadedLevelPackage )
			{		
				StreamingLevels.Remove( LevelIndex-- );
				MarkPackageDirty();
			}
		}
	}
}

/** Utility for returning the ULevelStreaming object for a particular sub-level, specified by package name */
ULevelStreaming* AWorldInfo::GetLevelStreamingForPackageName(FName InPackageName)
{
	// iterate over each level streaming object
	for( INT LevelIndex=0; LevelIndex<StreamingLevels.Num(); LevelIndex++ )
	{
		ULevelStreaming* LevelStreaming = WorldInfo->StreamingLevels(LevelIndex);
		// see if name matches
		if(LevelStreaming && LevelStreaming->PackageName == InPackageName)
		{
			// it doesn, return this one
			return LevelStreaming;
		}
	}

	// failed to find one
	return NULL;
}

/**
 * Called when a property on this object has been modified externally
 *
 * @param PropertyThatChanged the property that was modified
 */
void AWorldInfo::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetName()==TEXT("bForceNoPrecomputedLighting") && bForceNoPrecomputedLighting)
		{
			appMsgf(AMT_OK, TEXT("bForceNoPrecomputedLighting is now enabled, build lighting once to propagate the change (will remove existing precomputed lighting data)."));
		}
		else if (PropertyThatChanged->GetName()==TEXT("CharacterLitIndirectBrightness")
			|| PropertyThatChanged->GetName()==TEXT("CharacterLitIndirectContrastFactor")
			|| PropertyThatChanged->GetName()==TEXT("CharacterShadowedIndirectBrightness")
			|| PropertyThatChanged->GetName()==TEXT("CharacterShadowedIndirectContrastFactor")
			|| PropertyThatChanged->GetName()==TEXT("CharacterLightingContrastFactor"))
		{
			// Reattach all light environments to propagate the change
			TComponentReattachContext<UDynamicLightEnvironmentComponent> LightEnvReattach;
		}
#if WITH_NOVODEX
		else if (PropertyThatChanged->GetName()==TEXT("DefaultSkinWidth"))
		{
			DefaultSkinWidth = Max( DefaultSkinWidth, 0.0f );
			if( GNovodexSDK != NULL )
			{
				GNovodexSDK->setParameter(NX_SKIN_WIDTH, DefaultSkinWidth);
			}
		}
#endif	// WITH_NOVODEX
	}

	// Reassociate levels in case we changed streaming behavior. Editor-only!
	if( GIsEditor )
	{
		// Load and associate levels if necessary.
		GWorld->FlushLevelStreaming();

		// Remove all currently visible levels.
		for( INT LevelIndex=0; LevelIndex<StreamingLevels.Num(); LevelIndex++ )
		{
			ULevelStreaming* StreamingLevel = StreamingLevels(LevelIndex);
			if( StreamingLevel 
			&&	StreamingLevel->LoadedLevel 
			&&	StreamingLevel->bIsVisible )
			{
				GWorld->RemoveFromWorld( StreamingLevel );
			}
		}

		// Load and associate levels if necessary.
		GWorld->FlushLevelStreaming();

		// Update the level browser so it always contains valid data
		if( GCallbackEvent )
		{
			GCallbackEvent->Send( CALLBACK_WorldChange );
		}
	}

	// clamp desaturation to 0..1
	DefaultPostProcessSettings.Scene_Desaturation = Clamp(DefaultPostProcessSettings.Scene_Desaturation, 0.f, 1.f);

	LightmassSettings.NumIndirectLightingBounces = Clamp(LightmassSettings.NumIndirectLightingBounces, 0, 100);
	LightmassSettings.StaticLightingLevelScale = Clamp(LightmassSettings.StaticLightingLevelScale, .001f, 1000.0f);
	LightmassSettings.EmissiveBoost = Max(LightmassSettings.EmissiveBoost, 0.0f);
	LightmassSettings.DiffuseBoost = Max(LightmassSettings.DiffuseBoost, 0.0f);
	LightmassSettings.SpecularBoost = Max(LightmassSettings.SpecularBoost, 0.0f);
	LightmassSettings.IndirectNormalInfluenceBoost = Clamp(LightmassSettings.IndirectNormalInfluenceBoost, 0.0f, 0.8f);
	LightmassSettings.DirectIlluminationOcclusionFraction = Clamp(LightmassSettings.DirectIlluminationOcclusionFraction, 0.0f, 1.0f);
	LightmassSettings.IndirectIlluminationOcclusionFraction = Clamp(LightmassSettings.IndirectIlluminationOcclusionFraction, 0.0f, 1.0f);
	LightmassSettings.OcclusionExponent = Max(LightmassSettings.OcclusionExponent, 0.0f);
	LightmassSettings.FullyOccludedSamplesFraction = Clamp(LightmassSettings.FullyOccludedSamplesFraction, 0.0f, 1.0f);
	LightmassSettings.MaxOcclusionDistance = Max(LightmassSettings.MaxOcclusionDistance, 0.0f);

	// Ensure texture size is power of two between 512 and 4096.
	PackedLightAndShadowMapTextureSize = Clamp<UINT>( appRoundUpToPowerOfTwo( PackedLightAndShadowMapTextureSize ), 512, 4096 );

	if (PropertyThatChanged)
	{
		UBOOL bRedraw = FALSE;
		if (PropertyThatChanged->GetName()==TEXT("bUseGlobalIllumination"))
		{
			FGlobalComponentReattachContext ReattachAll;
			bRedraw = TRUE;
		}
		if (bRedraw)
		{
			GCallbackEvent->Send(CALLBACK_RedrawAllViewports);
		}
	}

	if (GWorld->PersistentLevel->GetWorldInfo() == this)
	{
		const FLinearColor PreviewColor = bUseGlobalIllumination ? LightmassSettings.EnvironmentIntensity * FLinearColor(LightmassSettings.EnvironmentColor) : FLinearColor::Black;
		GWorld->Scene->UpdatePreviewSkyLightColor(PreviewColor);
		GWorld->Scene->SetImageReflectionEnvironmentTexture(ImageReflectionEnvironmentTexture, ImageReflectionEnvironmentColor, ImageReflectionEnvironmentRotation);

		if (GIsEditor)
		{
			GEngine->DeferredCommands.AddUniqueItem(TEXT("UpdateLandscapeSetup"));
		}
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void AWorldInfo::SetVolumes()
{
}

APhysicsVolume* AWorldInfo::GetDefaultPhysicsVolume()
{
	if ( !PhysicsVolume )
	{
		PhysicsVolume = CastChecked<APhysicsVolume>(GWorld->SpawnActor(ADefaultPhysicsVolume::StaticClass()));
		PhysicsVolume->Priority = -1000000;
		PhysicsVolume->bNoDelete = true;
	}
	return PhysicsVolume;
}

APhysicsVolume* AWorldInfo::GetPhysicsVolume(const FVector& Loc, AActor* A, UBOOL bUseTouch)
{
	APhysicsVolume *NewVolume = GWorld->GetDefaultPhysicsVolume();
	if (A != NULL)
	{
		// use the base physics volume if possible for skeletal attachments
		if (A->Base != NULL && A->BaseSkelComponent != NULL)
		{
			return A->Base->PhysicsVolume ? A->Base->PhysicsVolume : NewVolume;
		}
		// if this actor has no collision
		if ( !A->bCollideActors && !A->bCollideWorld && GIsGame )
		{
			// use base's physics volume if possible
			if (A->Base != NULL)
			{
				return A->Base->PhysicsVolume ? A->Base->PhysicsVolume : NewVolume;
			}
			else
			{
				// otherwise use the default one
				return NewVolume;
			}
		}
		// check touching array for volumes if possible
		if ( bUseTouch )
		{
			for ( INT Idx = 0; Idx < A->Touching.Num(); Idx++ )
			{
				APhysicsVolume* const V = Cast<APhysicsVolume>(A->Touching(Idx));
				if ( V && (V->Priority > NewVolume->Priority) && (V->bPhysicsOnContact || V->Encompasses(Loc)) )
				{
					NewVolume = V;
				}
			}
			return NewVolume;
		}
	}
	// check for all volumes at that point
	FMemMark Mark(GMainThreadMemStack);
	for( FCheckResult* Link=GWorld->Hash->ActorPointCheck( GMainThreadMemStack, Loc, FVector(0.f,0.f,0.f), TRACE_PhysicsVolumes); Link; Link=Link->GetNext() )
	{
		APhysicsVolume* const V = (APhysicsVolume*)(Link->Actor);
		if ( V && (V->Priority > NewVolume->Priority) )
		{
			NewVolume = V;
		}
	}
	Mark.Pop();

	return NewVolume;
}

/**
 * Activates LevelStartup and/or LevelBeginning events in the sequences in the world
 *
 * @param bShouldActivateLevelStartupEvents If TRUE, will activate all LevelStartup events
 * @param bShouldActivateLevelBeginningEvents If TRUE, will activate all LevelBeginning events
 * @param bShouldActivateLevelLoadedEvents If TRUE, will activate all LevelLoadedAndVisible events
 */
void AWorldInfo::NotifyMatchStarted(UBOOL bShouldActivateLevelStartupEvents, UBOOL bShouldActivateLevelBeginningEvents, UBOOL bShouldActivateLevelLoadedEvents)
{
	for (INT LevelIdx = 0; LevelIdx < GWorld->Levels.Num(); LevelIdx++)
	{
		ULevel* const Level = GWorld->Levels(LevelIdx);
		for (INT SeqIdx = 0; SeqIdx < Level->GameSequences.Num(); SeqIdx++)
		{
			USequence* Seq = Level->GameSequences(SeqIdx);
			if(Seq)
			{
				Seq->NotifyMatchStarted(bShouldActivateLevelStartupEvents, bShouldActivateLevelBeginningEvents, bShouldActivateLevelLoadedEvents);
			}
		}
	}
}

/**
 * Finds the post process settings to use for a given view location, taking into account the world's default
 * settings and the post process volumes in the world.
 * @param	ViewLocation			Current view location.
 * @param	bUseVolumes				Whether to use the world's post process volumes
 * @param	OutPostProcessSettings	Upon return, the post process settings for a camera at ViewLocation.
 * @return	If the settings came from a post process volume, the post process volume is returned.
 */
APostProcessVolume* AWorldInfo::GetPostProcessSettings(const FVector& ViewLocation,UBOOL bUseVolumes,FPostProcessSettings& OutPostProcessSettings)
{
	APostProcessVolume* Volume = NULL;
	if( bUseVolumes )
	{
		// Find the highest priority volume encompassing the current view location. This is made easier by the linked
		// list being sorted by priority. @todo: it remains to be seen whether we should trade off sorting for constant
		// time insertion/ removal time via e.g. TLinkedList.
		Volume = HighestPriorityPostProcessVolume;
		while( Volume )
		{
			// Volume encompasses, break out of loop.
			if( Volume->bEnabled && Volume->Encompasses( ViewLocation ) )
			{
				break;
			}
			// Volume doesn't encompass location, further traverse linked list.
			else
			{
				Volume = Volume->NextLowerPriorityVolume;
			}
		}
	}

	{
		// If first level is a FakePersistentLevel (see CommitMapChange for more info)
		// then use its world info for post process settings
		AWorldInfo* CurrentWorldInfo = this;
		if( StreamingLevels.Num() > 0 &&
			StreamingLevels(0) &&
			StreamingLevels(0)->LoadedLevel && 
			StreamingLevels(0)->IsA(ULevelStreamingPersistent::StaticClass()) )
		{
			CurrentWorldInfo = StreamingLevels(0)->LoadedLevel->GetWorldInfo();
		}

		OutPostProcessSettings = CurrentWorldInfo->DefaultPostProcessSettings;
	}

	if(Volume)
	{
		Volume->Settings.OverrideSettingsFor(OutPostProcessSettings);
	}

	return Volume;
}

/** Checks whether modulate-better shadows are allowed in this world. */
UBOOL AWorldInfo::GetAllowTemporalAA() const
{
	const AWorldInfo* CurrentWorldInfo = this;
	if( StreamingLevels.Num() > 0 &&
		StreamingLevels(0) &&
		StreamingLevels(0)->LoadedLevel && 
		StreamingLevels(0)->IsA(ULevelStreamingPersistent::StaticClass()) )
	{
		CurrentWorldInfo = StreamingLevels(0)->LoadedLevel->GetWorldInfo();
	}

	return CurrentWorldInfo->bAllowTemporalAA;
}

FLinearColor AWorldInfo::GetEnvironmentColor() const
{
	const AWorldInfo* CurrentWorldInfo = this;
	if( StreamingLevels.Num() > 0 &&
		StreamingLevels(0) &&
		StreamingLevels(0)->LoadedLevel && 
		StreamingLevels(0)->IsA(ULevelStreamingPersistent::StaticClass()) )
	{
		CurrentWorldInfo = StreamingLevels(0)->LoadedLevel->GetWorldInfo();
	}

	return FLinearColor(CurrentWorldInfo->LightmassSettings.EnvironmentColor) * CurrentWorldInfo->LightmassSettings.EnvironmentIntensity;
}

/**
 * Finds the reverb settings to use for a given view location, taking into account the world's default
 * settings and the reverb volumes in the world.
 *
 * @param	ViewLocation			Current view location.
 * @param	OutReverbSettings		[out] Upon return, the reverb settings for a camera at ViewLocation.
 * @param	OutInteriorSettings		[out] Upon return, the interior settings for a camera at ViewLocation.
 * @return							If the settings came from a reverb volume, the reverb volume's object index is returned.
 */
INT AWorldInfo::GetAudioSettings( const FVector& ViewLocation, FReverbSettings* OutReverbSettings, FInteriorSettings* OutInteriorSettings )
{
	INT Index = -1;
	// Find the highest priority volume encompassing the current view location. This is made easier by the linked
	// list being sorted by priority. @todo: it remains to be seen whether we should trade off sorting for constant
	// time insertion/ removal time via e.g. TLinkedList.
	AReverbVolume* Volume = HighestPriorityReverbVolume;
	while( Volume )
	{
		// Volume encompasses, break out of loop.
		if (Volume->bEnabled && Volume->Encompasses(ViewLocation))
		{
			break;
		}
		// Volume doesn't encompass location, further traverse linked list.
		else
		{
			Volume = Volume->NextLowerPriorityVolume;
		}
	}

	if( Volume )
	{
		if( OutReverbSettings )
		{
			*OutReverbSettings = Volume->Settings;
		}

		if( OutInteriorSettings )
		{
			*OutInteriorSettings = Volume->AmbientZoneSettings;
		}

		Index = Volume->GetIndex();
	}
	else
	{
		// If first level is a FakePersistentLevel (see CommitMapChange for more info)
		// then use its world info for reverb settings
		AWorldInfo* CurrentWorldInfo = this;
		if( StreamingLevels.Num() > 0 &&
			StreamingLevels( 0 ) &&
			StreamingLevels( 0 )->LoadedLevel && 
			StreamingLevels( 0 )->IsA( ULevelStreamingPersistent::StaticClass() ) )
		{
			CurrentWorldInfo = StreamingLevels( 0 )->LoadedLevel->GetWorldInfo();
		}

		if( OutReverbSettings )
		{
			*OutReverbSettings = CurrentWorldInfo->DefaultReverbSettings;
		}

		if( OutInteriorSettings )
		{
			*OutInteriorSettings = CurrentWorldInfo->DefaultAmbientZoneSettings;
		}
	}

	return( Index );
}

/**
 * Finds the portal volume actor at a given location
 */
APortalVolume* AWorldInfo::GetPortalVolume( const FVector& Location )
{
	for( INT i = 0; i < PortalVolumes.Num(); i++ )
	{
		APortalVolume* Volume = PortalVolumes( i );

		// Volume encompasses, break out of loop.
		if( Volume->Encompasses( Location ) )
		{
			return( Volume );
		}
	}

	return( NULL );
}

/** Returns TRUE if the given position is inside any AMassiveLODOverrideVolume in the world. */
UBOOL AWorldInfo::IsInsideMassiveLODVolume(const FVector& Location) const
{
	for (INT VolumeIndex = 0; VolumeIndex < MassiveLODOverrideVolumes.Num(); VolumeIndex++)
	{
		if (MassiveLODOverrideVolumes(VolumeIndex) && MassiveLODOverrideVolumes(VolumeIndex)->Encompasses(Location))
		{
			return TRUE;
		}
	}
	return FALSE;
}

/** 
 * Remap sound locations through portals
 */
FVector AWorldInfo::RemapLocationThroughPortals( const FVector& SourceLocation, const FVector& ListenerLocation )
{
	// Deconst the variable
	FVector ModifiedSourceLocation = SourceLocation;

	// Account for the fact we are in a different portal volume
	const APortalVolume* const SpeakerVolume = GetPortalVolume( SourceLocation );
	const APortalVolume* const ListenerVolume = GetPortalVolume( ListenerLocation );
	if( SpeakerVolume && ListenerVolume && ( ListenerVolume != SpeakerVolume ) )
	{
		// Find the destination portal (if any) that is in the same portal volume as the listener
		for( INT i = 0; i < ListenerVolume->Portals.Num(); i++ )
		{
			// Get the teleporter that the listener is next to
			const APortalTeleporter* const SourceTeleporter = ListenerVolume->Portals( i );
			for( INT j = 0; j < SpeakerVolume->Portals.Num(); j++ )
			{
				// Get the teleporter that is next to the sound
				const APortalTeleporter* const DestinationTeleporter = SpeakerVolume->Portals( j ); 

				// If they are the same - we can hear the sound through the portal
				if( SourceTeleporter->SisterPortal == DestinationTeleporter )
				{
					// If there is a portal link - map the sound location to the portal location
					ModifiedSourceLocation = SourceLocation - DestinationTeleporter->Location + SourceTeleporter->Location;
					return( ModifiedSourceLocation );
				}
			}
		}
	}

	return( ModifiedSourceLocation );
}

/**
 * Sets bMapNeedsLightingFullyRebuild to the specified value.  Marks the worldinfo package dirty if the value changed.
 *
 * @param	bInMapNeedsLightingFullyRebuild			The new value.
 */
void AWorldInfo::SetMapNeedsLightingFullyRebuilt(UBOOL bInMapNeedsLightingFullyRebuild)
{
	check(IsInGameThread());
	if ( bMapNeedsLightingFullyRebuilt != bInMapNeedsLightingFullyRebuild )
	{
		// Save the lighting invalidation for transactions.
		Modify();

		bMapNeedsLightingFullyRebuilt = bInMapNeedsLightingFullyRebuild;
	}
	// Update last time unbuilt lighting was encountered.
	if ( bMapNeedsLightingFullyRebuilt )
	{
		LastTimeUnbuiltLightingWasEncountered = GCurrentTime;
	}
}
