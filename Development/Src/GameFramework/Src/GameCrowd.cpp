/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/
#include "GameFramework.h"

#include "EngineSequenceClasses.h"
#include "EngineAnimClasses.h"
#include "DebugRenderSceneProxy.h"

#include "GameCrowdUtilities.h"

IMPLEMENT_CLASS(AGameCrowdAgent);
IMPLEMENT_CLASS(AGameCrowdAgentSkeletal);
IMPLEMENT_CLASS(AGameCrowdAgentSM);
IMPLEMENT_CLASS(AGameCrowdInteractionPoint);
IMPLEMENT_CLASS(AGameCrowdDestination);
IMPLEMENT_CLASS(AGameCrowdBehaviorPoint);
IMPLEMENT_CLASS(USeqAct_GameCrowdSpawner);
IMPLEMENT_CLASS(AGameCrowdReplicationActor);
IMPLEMENT_CLASS(USeqEvent_CrowdAgentReachedDestination);
IMPLEMENT_CLASS(USeqAct_PlayAgentAnimation);
IMPLEMENT_CLASS(UGameDestinationConnRenderingComponent);
IMPLEMENT_CLASS(UGameCrowdAgentBehavior);
IMPLEMENT_CLASS(AGameCrowdDestinationQueuePoint);
IMPLEMENT_CLASS(UGameCrowdBehavior_PlayAnimation);
IMPLEMENT_CLASS(UGameCrowdBehavior_WaitInQueue);
IMPLEMENT_CLASS(UGameCrowdBehavior_RunFromPanic);
IMPLEMENT_CLASS(UGameCrowdBehavior_WaitForGroup);
IMPLEMENT_CLASS(UGameCrowdGroup);
IMPLEMENT_CLASS(USeqAct_GameCrowdPopulationManagerToggle);
IMPLEMENT_CLASS(AGameCrowdPopulationManager);
IMPLEMENT_CLASS(AGameCrowdSpawnRelativeActor);
IMPLEMENT_CLASS(AGameCrowdInfoVolume);

// Crowd stats
DECLARE_STATS_GROUP(TEXT("Crowd"), STATGROUP_Crowd);

// Nav mesh stat enums. This could be in UnPath.h if we need stats in 
// UnNavigationMesh.cpp, etc (but then more rebuilds needed when changing)
enum ECrowdStats
{
	STAT_CrowdTotal = STAT_CrowdFirstStat,
	STAT_AgentTick,
	STAT_AgentPhysics,
	STAT_CrowdPopMgr,
	STAT_CrowdForce,
};
DECLARE_CYCLE_STAT(TEXT("Crowd Total"), STAT_CrowdTotal, STATGROUP_Crowd);
DECLARE_CYCLE_STAT(TEXT("Force Points"), STAT_CrowdForce, STATGROUP_Crowd);
DECLARE_CYCLE_STAT(TEXT("Pop Manager"), STAT_CrowdPopMgr, STATGROUP_Crowd);
DECLARE_CYCLE_STAT(TEXT("..Agent Physics"), STAT_AgentPhysics, STATGROUP_Crowd);
DECLARE_CYCLE_STAT(TEXT("Agent Full Tick"), STAT_AgentTick, STATGROUP_Crowd);


void AGameCrowdAgent::PreBeginPlay()
{
	GetLevel()->CrossLevelActors.AddItem(this);

	Super::PreBeginPlay();
}

void AGameCrowdAgent::PostBeginPlay()
{
	Super::PostBeginPlay();

	PendingVelocity = (IntermediatePoint-Location).SafeNormal2D() * MaxSpeed;
}

void AGameCrowdAgent::PostScriptDestroyed()
{
	//@note: this won't be called if the agent is simply GC'ed due to level change/removal,
	// but in that case the level must be being GC'ed as well, making this unnecessary
	GetLevel()->CrossLevelActors.RemoveItem(this);

	Super::PostScriptDestroyed();
}

void AGameCrowdAgent::GetActorReferences(TArray<FActorReference*>& ActorRefs, UBOOL bIsRemovingLevel)
{
	if (bIsRemovingLevel)
	{
		// simply clear our references regardless as we can easily regenerate them from the remaining levels
		// FIXMESTEVE - clear navigation handle?
	}
	Super::GetActorReferences(ActorRefs, bIsRemovingLevel);
}

FLOAT AGameCrowdAgent::GetAvoidRadius()
{
	return AvoidOtherRadius;
}
INT AGameCrowdAgent::GetInfluencePriority()
{
	return 0;
}
FColor AGameCrowdAgent::GetDebugAgentColor()
{
	return DebugAgentColor;
}

INT CalcDeltaYaw(const FRotator& A, const FRotator& B)
{
	INT DeltaYaw = (A.Yaw & 65535) - (B.Yaw & 65535);
	if(DeltaYaw > 32768)
		DeltaYaw -= 65536;
	else if(DeltaYaw < -32768)
		DeltaYaw += 65536;

	return DeltaYaw;
}

/** 
  * Clamp velocity to reach destination exactly
  *  Assumes valid CurrentDestination
  */
void AGameCrowdAgent::ExactVelocity(FLOAT DeltaTime)
{
	if ( (CurrentDestination->Location - Location).SizeSquared() <= Velocity.SizeSquared() * DeltaTime*DeltaTime )
	{
		Velocity = (CurrentDestination->Location - Location)/DeltaTime;
	}
	else
	{
		FVector ExactDir = CurrentDestination->Location - Location;
		ExactDir.Z = 0.f;
		Velocity = Velocity.Size() * ExactDir.SafeNormal();
	}
}

/** Override physics update to do our own movement stuff */
void AGameCrowdAgent::performPhysics(FLOAT DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_AgentPhysics);
	// Don't update if crowds disabled or this guy disabled
	if( GWorld->bDisableCrowds )
	{
		return;
	}

	if( Location.Z < WorldInfo->KillZ )
	{
		eventFellOutOfWorld(WorldInfo->KillZDamageType);
		return;
	}

	// Force velocity to zero while idling.
	if( !bPaused )
	{
		Velocity = PendingVelocity;

		// Clamp velocity to not overshoot destination
		if( CurrentDestination && (CurrentDestination->Location == IntermediatePoint) && (DeltaTime > 0.f) )
		{
			FVector ClosestPoint(0.f);
			FVector EndPoint = Location+Velocity*DeltaTime;
			FLOAT NearestDist = PointDistToSegment(IntermediatePoint, Location+Velocity*DeltaTime, Location, ClosestPoint);
			if ( CurrentDestination->ReachedByAgent(this, EndPoint, FALSE) )
			{
				if ( CurrentDestination->bMustReachExactly )
				{
					ExactVelocity(DeltaTime);
				}
			}
			else if ( CurrentDestination->ReachedByAgent(this, ClosestPoint, FALSE) )
			{
				if ( CurrentDestination->bMustReachExactly )
				{
					ExactVelocity(DeltaTime);
				}
				else
				{
					// FIXMESTEVE - to smooth more, could just clamp to a velocity that keeps from overshooting too much
					Velocity = (ClosestPoint - Location)/DeltaTime;
				}
			}
			else if ( ((Velocity | (CurrentDestination->Location - EndPoint)) < 0.f) )
			{
				// Force toward
				if ( NearestDist < AwareRadius )
				{
					ExactVelocity(DeltaTime);
				}
			}
		}

		// Integrate velocity to get new position.
		FVector NewLocation = Location + (Velocity * DeltaTime);

		// Adjust velocity to avoid obstacles (as defined by NavMesh)
		if ( bCheckForObstacles && CurrentDestination )
		{
			bHitObstacle = FALSE;
			bBadHitNormal = FALSE;
			ObstacleCheckCount++;
			if ( !NavigationHandle )
			{
				eventInitNavigationHandle();
			}

			if ( NavigationHandle )
			{
				FCheckResult Hit(1.f);
				FVector NewPosition = NewLocation;
				NewPosition.Z += EyeZOffset;
				FVector TestVelocity = 0.5f * MaxWalkingSpeed * Velocity.SafeNormal();
				TestVelocity.Z = ::Max(0.f, TestVelocity.Z); // Not a good assumption if using crowd system for flying/swimming creatures, but neither is navmesh

				if ( !NavigationHandle->StaticObstacleLineCheck( GetOuter(), Hit, LastKnownGoodPosition, NewPosition + TestVelocity, FVector(0.f)) )
				{
					bHitObstacle = TRUE;
					//DrawDebugLine(NewPosition + TestVelocity, LastKnownGoodPosition,255,0,0,0);

					if ( (Velocity.SizeSquared() < 50.f) && (WorldInfo->TimeSeconds - LastPathingAttempt > 1.f) && (WorldInfo->TimeSeconds - LastRenderTime < 5.f) )
					{
						eventUpdateIntermediatePoint();
					}

					// FIXMESTEVE - maybe just kill if not visible to player
					// slide along wall
					FLOAT AgentSpeed = Velocity.Size();
					if ( Hit.Normal.IsZero() )
					{
						bBadHitNormal = TRUE;
						// FIXMESTEVE - need to figure out why this happens!
						//debugf(TEXT("%s NO VALID HITNORMAL"), *GetName());
						Velocity *= -1.f;
					}
					else if ( Hit.Normal.Z < WalkableFloorZ )
					{
						//debugf(TEXT("%s hit %f"), *GetName(), Hit.Normal.Z);
						Velocity = (Velocity - Hit.Normal * (Velocity | Hit.Normal)) * (1.f - Hit.Time);
						Velocity = AgentSpeed * Velocity.SafeNormal();
					}
					NewLocation = Location + (Velocity * DeltaTime);
 				}
				else
				{
					//DrawDebugLine(NewPosition + TestVelocity, LastKnownGoodPosition ,255,255,255,0);

					LastKnownGoodPosition = NewPosition;
				}
			}
		}

		// If desired, use ZELC to follow ground
		ConformTraceFrameCount++;
		if( ConformType != CFM_None )
		{
			// See if enough frames have passed to update target Z again
			if(ConformTraceFrameCount >= CurrentConformTraceInterval)
			{
				ConformTraceFrameCount = 0;

				UBOOL bSuccess = UpdateInterpZTranslation(NewLocation);
				if(!bSuccess)
				{
					return;
				}
			}

			// Move location towards Z target, at ConformZVel speed
			NewLocation.Z += InterpZTranslation;
		}


		// Point in direction of travel
		FRotator NewRotation = Rotation;
		if( !Velocity.IsNearlyZero() )
		{
			NewRotation = Velocity.Rotation();
		}

		// Cap the maximum yaw rate
		INT DeltaYaw = CalcDeltaYaw(NewRotation, Rotation);
		INT MaxYaw = appRound(DeltaTime * MaxYawRate);
		DeltaYaw = ::Clamp(DeltaYaw, -MaxYaw, MaxYaw);
		NewRotation.Yaw = Rotation.Yaw + DeltaYaw;
		
		if( !bAllowPitching )
		{
			NewRotation.Pitch = 0;
			NewRotation.Roll = 0;
		}

		// Actually move the Actor
		FCheckResult Hit(1.f);
		GWorld->MoveActor(this, NewLocation - Location, NewRotation, 0, Hit);

		if( GDrawCrowdPath )
		{
			DrawDebugCylinder( Location, Location, AvoidOtherRadius, 20, DebugAgentColor.R, DebugAgentColor.G, DebugAgentColor.B, TRUE );
		}
	}
}

UBOOL AGameCrowdAgent::IsIdle()
{
	return CurrentBehavior && CurrentBehavior->bIdleBehavior;
}

UBOOL AGameCrowdAgent::IsPanicked()
{
	return bIsPanicked;
}

FVector AGameCrowdAgent::GetCollisionExtent()
{
	return FVector( AvoidOtherRadius, AvoidOtherRadius, GroundOffset );
}

/** Stop agent moving and play death anim */
void AGameCrowdAgent::PlayDeath(FVector KillMomentum)
{
	if ( CurrentBehavior )
	{
		eventStopBehavior();
	}
	SetCollision(FALSE, FALSE, FALSE); // Turn off all collision when dead.
	LifeSpan = DeadBodyDuration;
	eventFireDeathEvent();
}

/** Stop agent moving and play death anim */
void AGameCrowdAgentSkeletal::PlayDeath(FVector KillMomentum)
{
	Super::PlayDeath(KillMomentum);
	if( (DeathAnimNames.Num() > 0) && FullBodySlot )
	{
		const INT AnimIndex = appRand() % DeathAnimNames.Num();
		FullBodySlot->PlayCustomAnim(DeathAnimNames(AnimIndex), 1.f, 0.2f, -1.f, FALSE, TRUE);
		FullBodySlot->SetActorAnimEndNotification(TRUE);
		bIsPlayingDeathAnimation = TRUE;
	}
}

UBOOL AGameCrowdAgent::ShouldTrace(UPrimitiveComponent* Primitive, AActor *SourceActor, DWORD TraceFlags)
{
	if( SourceActor && !SourceActor->Instigator && SourceActor->IsA(AWeapon::StaticClass()) )
	{
		return FALSE;
	}

	return Super::ShouldTrace(Primitive, SourceActor, TraceFlags);
}

void AGameCrowdAgentSkeletal::SetRootMotion(UBOOL bRootMotionEnabled)
{
	if ( ActionSeqNode && SkeletalMeshComponent )
	{
		if ( bRootMotionEnabled )
		{
			ActionSeqNode->SetRootBoneAxisOption(RBA_Translate, RBA_Translate, RBA_Translate);
			SkeletalMeshComponent->RootMotionMode = RMM_Translate;
		}
		else
		{
			ActionSeqNode->SetRootBoneAxisOption(RBA_Default, RBA_Default, RBA_Default);
			SkeletalMeshComponent->RootMotionMode = bUseRootMotionVelocity? RMM_Accel : RMM_Ignore;
		}
	}
}

/** 
  * Update position of an agent in a crowd, using properties in the action. 
  */
void AGameCrowdAgentSkeletal::TickSpecial(FLOAT DeltaTime)
{
	Super::TickSpecial(DeltaTime);

	UBOOL bResult = ShouldPerformCrowdSimulation(DeltaTime);

	// Evaluate if we should continue to tick, when not visible
	if (!bTickWhenNotVisible)
	{
		if (WorldInfo->TimeSeconds - LastRenderTime >= Max<FLOAT>(NotVisibleDisableTickTime, 0.01f))
		{
			bResult = FALSE;
		}
	}
	
	if (bResult && MaxAnimationDistanceSq > 0.f)
	{
		FVector PlayerLocation(0.f, 0.f, 0.f);
		FRotator PlayerRotation(0, 0, 0);
		for (INT Idx = 0; Idx < GEngine->GamePlayers.Num(); ++Idx)
		{
			if (GEngine->GamePlayers(Idx) && GEngine->GamePlayers(Idx)->Actor && GEngine->GamePlayers(Idx)->Actor->IsLocalPlayerController())
			{
				GEngine->GamePlayers(Idx)->Actor->eventGetPlayerViewPoint(PlayerLocation, PlayerRotation);
				break;
			}
		}

		// Don't animate if far away from viewer(s)
		bResult = ((Location - PlayerLocation).SizeSquared() < MaxAnimationDistanceSq);
	}

	// turn off skeleton as well, unless ragdoll, playing death anim or playing important animation
	if (bAllowSkeletonUpdateChangeBasedOnTickResult && SkeletalMeshComponent && Physics != PHYS_RigidBody && !bIsPlayingDeathAnimation && !bIsPlayingImportantAnimation)
	{
		SkeletalMeshComponent->bNoSkeletonUpdate = !bResult;
	}

	if ( bDeleteMe || !bResult )
	{
		return;
	}

	if ( !bUseRootMotionVelocity )
	{
		FLOAT VMag = Velocity.Size();

		// Blend between running and walking anim
		if(SpeedBlendNode)
		{
			FLOAT CurrentWeight = SpeedBlendNode->Child2Weight;

			FLOAT TargetWeight = ((VMag - SpeedBlendStart)/(SpeedBlendEnd - SpeedBlendStart));
			TargetWeight = ::Clamp<FLOAT>(TargetWeight, 0.f, 1.f);

			// limit how quickly anim rate can change
			FLOAT DeltaWeight = (TargetWeight - CurrentWeight);
			FLOAT MaxScaleChange = MaxSpeedBlendChangeSpeed * DeltaTime;
			DeltaWeight = Clamp(DeltaWeight, -MaxScaleChange, MaxScaleChange);

			SpeedBlendNode->SetBlendTarget(CurrentWeight + DeltaWeight, 0.f);
		}

		// Change anim rate based on speed
		if( AgentTree )
		{
			AgentTree->SetGroupRateScale(MoveSyncGroupName, VMag * AnimVelRate);
		}
	}
}

UBOOL AGameCrowdAgent::WantsOverlapCheckWith(AActor* TestActor)
{
	// FIXMESTEVE - GetACrowdAgent() or actor bool?
	return TestActor && (TestActor->GetAPawn() || Cast<AGameCrowdAgent>(TestActor) || (TestActor->GetAVolume() && Cast<APhysicsVolume>(TestActor)));
}

/** 
  * Update NearbyDynamics lists used for avoidance purposes. 
  */
void AGameCrowdAgent::UpdateProximityInfo()
{
	SCOPE_CYCLE_COUNTER(STAT_AgentPhysics);

	NearbyDynamics.Reset();

	if( !IsIdle() && CurrentDestination != NULL )
	{
		if( NavigationHandle != NULL && 
			!NavigationHandle->ObstacleLineCheck( Location, IntermediatePoint, FVector(AvoidOtherRadius,AvoidOtherRadius,GroundOffset*0.5f)) )
		{
			eventUpdateIntermediatePoint();
		}
	}

	FLOAT CheckDistSq = (WorldInfo->TimeSeconds - LastRenderTime > 2.0) ? ProximityLODDist*ProximityLODDist : VisibleProximityLODDist*VisibleProximityLODDist;
	UBOOL bFoundNearbyPlayer = FALSE;
	for( INT Idx = 0; Idx < GEngine->GamePlayers.Num(); ++Idx )   
	{  
		if (GEngine->GamePlayers(Idx) &&  
			GEngine->GamePlayers(Idx)->Actor &&  
			GEngine->GamePlayers(Idx)->Actor->IsLocalPlayerController())  
		{  
			APlayerController* PC = GEngine->GamePlayers(Idx)->Actor;  
			// FIXMESTEVE - base on camera position, not viewtarget position
			PC->GetViewTarget();
			if ( PC->ViewTarget && ((PC->ViewTarget->Location - Location).SizeSquared() < CheckDistSq) )
			{
				bFoundNearbyPlayer = TRUE;
				break;
			}
		}
	}

	// Agent-killing stuff
	UBOOL bKillAgent = FALSE;
	UBOOL bPlayDeathAnim = FALSE;

	// Query main collision octree for other agents or pawns nearby
	FMemMark Mark(GMainThreadMemStack);

	// Set up to check for possible encounter behaviors
	UBOOL bCheckForAgentEncounters = (WorldInfo->TimeSeconds - LastRenderTime < 0.1f) && !CurrentBehavior && (EncounterAgentBehaviors.Num() > 0);
	UBOOL bHavePotentialEncounter = FALSE;

	const FVector OverlapLocation = Location;// + 0.5f * AwareRadius * Velocity.SafeNormal();
	
	if( bDebug )
	{
		DrawDebugSphere( OverlapLocation, AwareRadius, 20, 255, 255, 255 );
		DrawDebugLine( Location, Location + FVector(0,0,10000), 255, 255, 255 );
	}

	// Test location is out in front of agent
	FLOAT CheckRadius = AwareRadius;
	FLOAT ViralRadiusSq = 0.f;
	if( CurrentBehavior != NULL && CurrentBehavior->bIsViralBehavior )
	{
		ViralRadiusSq = CurrentBehavior->ViralRadius * CurrentBehavior->ViralRadius;
		CheckRadius = ::Max( CheckRadius, CurrentBehavior->ViralRadius );
	}
	const FLOAT AwareRadiusSq = AwareRadius * AwareRadius;

	FCheckResult* Link = GWorld->Hash->RestrictedOverlapCheck(GMainThreadMemStack, this, OverlapLocation, AwareRadius);
	for( FCheckResult* result=Link; result; result=result->GetNext())
	{
		checkSlow(result->Actor);

		// Look for nearby agents
		IInterface_RVO* InterRVO = InterfaceCast<IInterface_RVO>(result->Actor);
		if( InterRVO != NULL && IsValidNearbyDynamic( result->Actor ) )
		{
			FLOAT DistSq = (result->Actor->Location - Location).SizeSquared();
			if( DistSq <= AwareRadiusSq )
			{
				FNearbyDynamicItem Item;
				Item.Dynamic = result->Actor;
				NearbyDynamics.AddItem(Item);
			}

			AGameCrowdAgent* OtherAgent = Cast<AGameCrowdAgent>(result->Actor);
			if( OtherAgent != NULL )
			{
				// trade viral behaviors
				if( CurrentBehavior != NULL && CurrentBehavior->bIsViralBehavior && DistSq <= ViralRadiusSq )
				{
					CurrentBehavior->eventPropagateViralBehaviorTo(OtherAgent);
				}

				// if don't have current behavior, and have EncounterAgentBehaviors, and other agent in front of me and facing me, then try initiating behavior
				OtherAgent->bPotentialEncounter = bCheckForAgentEncounters && ((Rotation.Vector() | (OtherAgent->Location - Location)) > 0.f) && ((OtherAgent->Rotation.Vector() | (Location - OtherAgent->Location)) > 0.f);
				bHavePotentialEncounter = bHavePotentialEncounter || OtherAgent->bPotentialEncounter;
			}
		}
		else if( result->Actor->GetAVolume() )
		{
			// If volume, look for a pain volume that would kill agent
			APhysicsVolume* PhysVol = Cast<APhysicsVolume>(result->Actor);
			if(PhysVol && PhysVol->bPainCausing)
			{
				// verify that am currently overlapping this pain volume (real radius + actual location)
				FRadiusOverlapCheck CheckInfo(Location,AvoidOtherRadius);
				if (CheckInfo.SphereBoundsTest(PhysVol->BrushComponent->Bounds))
				{
					// See if we want to destroy actor, or play the death anim
					if(PhysVol->bCrowdAgentsPlayDeathAnim)
					{
						bPlayDeathAnim = TRUE;
					}
					else
					{
						bKillAgent = TRUE;
					}
					break;
				}
			}
		}
	}
	Mark.Pop();

	AGameCrowdPopulationManager* PopMgr = Cast<AGameCrowdPopulationManager>(WorldInfo->PopulationManager);
	if( PopMgr != NULL )
	{
		PopMgr->GetAlwaysRelevantDynamics( this );
	}

	// First handle dying cases
	if(bKillAgent)
	{
		eventKillAgent();
	}
	else if(bPlayDeathAnim)
	{
		PlayDeath(FVector(0.f));
	}
	else if ( bHavePotentialEncounter )
	{
		eventHandlePotentialAgentEncounter();
	}
}

UBOOL AGameCrowdAgent::IsValidNearbyDynamic( AActor* A )
{
	return TRUE;
}
FLOAT AGameCrowdAgent::GetInfluencePct( INT PriB )
{
	if( PriB > GetInfluencePriority() )
	{
		return 1.f;
	}
	if( PriB < GetInfluencePriority() )
	{
		return 0.f;
	}
	return 0.5f;
}

UBOOL AGameCrowdAgent::IsDestinationObstructed( const FVector& Dest )
{
	for( INT ObstacleIdx = 0; ObstacleIdx < NearbyDynamics.Num(); ObstacleIdx++ )
	{
		IInterface_RVO* Interface = InterfaceCast<IInterface_RVO>(NearbyDynamics(ObstacleIdx).Dynamic);
		if( Interface == NULL )
		{
			continue;
		}

		const FVector Loc = Interface->GetLocation();
		const FLOAT	  Rad = Interface->GetAvoidRadius() + AvoidOtherRadius;
		const FLOAT   DistToDestSq = (Dest-Loc).SizeSquared2D();
		const FVector VectToDest = Dest - Location;
		const FLOAT	  DestDotObsVel = Interface->GetVelocity() | VectToDest;
		if( DistToDestSq < Rad*Rad && DestDotObsVel < 0.f )
		{
			return TRUE;
		}
	}

	return FALSE;
}

UBOOL AGameCrowdAgent::VerifyDestinationIsClear()
{
	FLOAT DistToDestSq = (IntermediatePoint - Location).SizeSquared2D();
	if( DistToDestSq < (AwareRadius*AwareRadius) && IsDestinationObstructed( IntermediatePoint ) )
	{
		if( bDebug && bPaused )
		{
			DrawDebugBox( IntermediatePoint, FVector(AvoidOtherRadius,AvoidOtherRadius,GroundOffset), 255, 0, 0 );
		}

		if( IntermediatePoint != CurrentDestination->Location && 
			NavigationHandle && 
			NavigationHandle->CurrentEdge != NULL )
		{
			const FVector	V0 = NavigationHandle->CurrentEdge->GetVertLocation(0);
			const FVector	V1 = NavigationHandle->CurrentEdge->GetVertLocation(1);
			const FLOAT		D0 = (Location - V0).Size();
			const FLOAT		D1 = (Location - V1).Size();
			const FVector	ClosestPt = (D0<D1) ? V0 : V1;
			const FVector	EdgeDir = (V1-V0).SafeNormal() * ((D1 < D0) ? -1.f : 1.f);

			UBOOL bClearDestFound = FALSE;
			FVector ClearDest(0);
			const FLOAT EdgeStep = AvoidOtherRadius * 0.5f;
			const INT StepCnt = appTrunc( NavigationHandle->CurrentEdge->GetEdgeLength() / EdgeStep );
			for( INT StepIdx = 0; StepIdx < StepCnt; StepIdx++ )
			{
				const FVector TestDest = ClosestPt + (EdgeStep * StepIdx * EdgeDir);
				if( !IsDestinationObstructed( TestDest ) )
				{
					ClearDest = TestDest;
					bClearDestFound = TRUE;
					break;
				}
				else if( bDebug && bPaused )
				{
					DrawDebugBox( TestDest, FVector(5), 255, 0, 0 );
				}
			}

			if( bClearDestFound )
			{
				if( bDebug && bPaused )
				{
					DrawDebugBox( ClearDest, FVector(5), 0, 255, 0 );
				}

				if( !bPaused )
				{
					IntermediatePoint = ClearDest;
				}				
			}

			return bClearDestFound;
		}		
	}
	return TRUE;
}

void IInterface_RVO::GetVelocityObstacleStats( TArray<FVelocityObstacleStat>& out_Array, AActor* RelActor )
{
	FVelocityObstacleStat Item;
	Item.Position = GetLocation();
	Item.Velocity = GetVelocity();
	Item.Radius   = GetAvoidRadius();
	Item.Priority = GetInfluencePriority();

	out_Array.AddItem(Item);
}

void AGameCrowdAgent::UpdatePendingVelocity( FLOAT DeltaTime )
{
	SCOPE_CYCLE_COUNTER(STAT_AgentPhysics);
	// Don't update if crowds disabled or this guy disabled
	if( GWorld->bDisableCrowds )
	{
		return;
	}

	if( Location.Z < WorldInfo->KillZ )
	{
		eventFellOutOfWorld(WorldInfo->KillZDamageType);
		return;
	}

	// Force velocity to zero while idling.
	if( CurrentDestination == NULL || IsIdle() )
	{
		PendingVelocity = FVector(0.f);
	}
	else
	{
		if( (WorldInfo->TimeSeconds - LastRenderTime) > 1.f )
		{
			PendingVelocity = (IntermediatePoint-Location).SafeNormal2D() * MaxSpeed;
			return;
		}

		const UBOOL bDestIsClear = VerifyDestinationIsClear();
		if( !bDestIsClear && IntermediatePoint == CurrentDestination->Location )
		{
			PendingVelocity = FVector(0);
			return;
		}		

		FVector OldVelocity = Velocity;
		FVector VelDir = Velocity.SafeNormal2D();
		FVector Destination = IntermediatePoint;
		FVector VectToDest = (Destination-Location);
		VectToDest.Z = 0;
		FLOAT DistToDest = VectToDest.Size2D();
		FVector DirToDest = VectToDest / DistToDest;


		TArray<FRVOAgentPair> RVO_Pairs;
		UBOOL bUseFallbackOptions = FALSE;
		for( INT ObstacleIdx = 0; ObstacleIdx < NearbyDynamics.Num(); ObstacleIdx++ )
		{
			IInterface_RVO* Interface = InterfaceCast<IInterface_RVO>(NearbyDynamics(ObstacleIdx).Dynamic);
			if( Interface == NULL || !Interface->IsActiveObstacle() )
			{
				continue;
			}

			AActor* NearbyActor = Interface->GetActor();
			const FVector VectToActor = (Interface->GetLocation() - Location).SafeNormal2D();
			const FLOAT ActorDotVel = -1.f;
			const FLOAT ActorDotDest = VectToActor | DirToDest;
			if( ActorDotVel < 0.f && ActorDotDest < -0.2f )
			{
				continue;
			}

			TArray<FVelocityObstacleStat> ObsList;
			Interface->GetVelocityObstacleStats( ObsList, this );
			for( INT ListIdx = 0; ListIdx < ObsList.Num(); ListIdx++ )
			{
				FRVOAgentPair Pair;
				Pair.OtherAgent = Interface;
				FVector LocB = ObsList(ListIdx).Position;
				FVector VelB = ObsList(ListIdx).Velocity;
				FLOAT	RadB = ObsList(ListIdx).Radius;
				FLOAT	Inf  = GetInfluencePct( ObsList(ListIdx).Priority );
				Pair.RVO.CalcRVO( Location, Velocity, AvoidOtherRadius, LocB, VelB, RadB, Inf );
				if( Pair.RVO.bValid )
				{
					RVO_Pairs.AddItem(Pair);
					bUseFallbackOptions = bUseFallbackOptions || Pair.RVO.bOverlap;
				}
			}
		}
		bUseFallbackOptions = bUseFallbackOptions || ((WorldInfo->TimeSeconds - LastProgressTime) > 2.f);
		if( bUseFallbackOptions )
		{
			LastFallbackActiveTime = WorldInfo->TimeSeconds;
		}
		bUseFallbackOptions = bUseFallbackOptions || ((WorldInfo->TimeSeconds - LastFallbackActiveTime) < 1.f);


		if( bDebug )
		{
			DrawDebugLine( Location, IntermediatePoint, 0, 0, 255 );
			DrawDebugLine( Location, Destination+FVector(0,0,5), 0, 0, 128 );
			DrawDebugLine( Location, CurrentDestination->Location+FVector(0,0,10), 255, 164, 200 );
			FColor C = GetDebugAgentColor();
			DrawDebugCylinder( Location, Location, GetAvoidRadius(), 20, C.R, C.G, C.B );
						
			for( INT PairIdx = 0; PairIdx < RVO_Pairs.Num(); PairIdx++ )
			{
				FRVOAgentPair& Pair = RVO_Pairs(PairIdx);
				Pair.RVO.DebugDrawVelocityObstacle( Pair.OtherAgent->GetDebugAgentColor() );
			}
		}

		

		FRotator DestDir = DirToDest.Rotation();
		FRotator Dir = Rotation;
		FLOAT	 BestPenality = MAXINT;
		FVector  BestVelocity(0);

		INT MaxYaw = appRound(DeltaTime * MaxYawRate);
		INT YawStart = DestDir.Yaw;
		for( INT RotSampleIdx = 0; RotSampleIdx < AvoidOtherSampleList.Num(); RotSampleIdx++ )
		{
			FAvoidOtherSampleItem& SampleItem  = AvoidOtherSampleList(RotSampleIdx);
			if( !bUseFallbackOptions && SampleItem.bFallbackOnly )
			{
				continue;
			}		

			Dir.Yaw = YawStart + SampleItem.RotOffset;

			FLOAT DirPenalty  = 1.f + (Dir.Vector() | -DirToDest);
			FLOAT VelPenalty  = 1.f + (Dir.Vector() | -VelDir);
			FLOAT MagPenalty  = 0.f;

			UBOOL bRotSuccess = FALSE;
			FLOAT SpeedPct = 1.f;
			FLOAT Speed = MaxSpeed;
			FLOAT MinCollisionPct = 1.f;
			FLOAT MinCollisionDistSq = 99999.f;

			for( INT MagSampleIdx = 0; MagSampleIdx < SampleItem.NumMagSamples; MagSampleIdx++ )
			{
				SpeedPct = ((SampleItem.NumMagSamples - MagSampleIdx)/(FLOAT)SampleItem.NumMagSamples);
				if( SpeedPct > MinCollisionPct )
				{
					continue;
				}
				Speed = MaxSpeed * SpeedPct;
				
				const FVector TestVel = Dir.Vector() * Speed;
				UBOOL bMagSuccess = TRUE;
				for( INT PairIdx = 0; PairIdx < RVO_Pairs.Num(); PairIdx++ )
				{
					FRVOAgentPair& Pair = RVO_Pairs(PairIdx);
					if( !Pair.RVO.bValid )
					{
						continue;
					}

					if( Pair.RVO.IsVelocityWithinObstacleBounds( Location, TestVel ) )
					{
						Pair.RVO.ComputePlanes();
						const FLOAT LtPlaneDotVel = Pair.RVO.LeftPlane.PlaneDot( Location );
//						const FLOAT RtPlaneDotVel = Pair.RVO.RightPlane.PlaneDot( Location );
						const FPlane& P = LtPlaneDotVel > 0 ? Pair.RVO.LeftPlane : Pair.RVO.RightPlane;
						const FVector Pt = FLinePlaneIntersection( Location, Location + TestVel, P );
						const FLOAT MinDistSq = (Pt-Location).SizeSquared2D();
						if( MinDistSq < MinCollisionDistSq )
						{
							MinCollisionDistSq = MinDistSq;
							MinCollisionPct = appSqrt(MinDistSq) / MaxSpeed;
						}
						
						if( bDebug && bPaused )
						{
							FColor C = Pair.OtherAgent->GetDebugAgentColor();
							DrawDebugBox(Pt, FVector(2), C.R, C.G, C.B );
						}

						bMagSuccess = FALSE;
						break;
					}
					else if( NavigationHandle != NULL && !NavigationHandle->ObstacleLineCheck( Location, Location + TestVel, FVector(0)) )
					{
						if( bPaused && bDebug )
						{
							DrawDebugLine( Location, Location + TestVel, 64 + BYTE(190 * SpeedPct), 0, 0 );
						}						

						bMagSuccess = FALSE;
						break;
					}
				}

				if( bMagSuccess )
				{
					MagPenalty = 1.f - SpeedPct;
					bRotSuccess = TRUE;
					break;
				}
				else
				{
// 					if( bPaused && bDebug )
// 					{
// 						DrawDebugLine( Location, Location + TestVel, 64 + (190 * SpeedPct), 0, 0 );
// 						DrawDebugBox( Location + TestVel, FVector(2), 64 + (190 * SpeedPct), 0, 0 );
// 					}
				}
			}

			if( bRotSuccess )
			{
				FLOAT Penalty = Max<FLOAT>(	DirPenalty  * PENALTY_COEFF_ANGLETOGOAL + 
											VelPenalty  * PENALTY_COEFF_ANGLETOVEL + 
											MagPenalty  * PENALTY_COEFF_MAG,KINDA_SMALL_NUMBER);
				if( Penalty < BestPenality )
				{
					BestPenality = Penalty;
					BestVelocity = Dir.Vector() * Speed;

					if( BestPenality < MIN_PENALTY_THRESHOLD && !bPaused )
					{
						break;
					}
				}

				if( bPaused && bDebug )
				{
					DrawDebugLine( Location, Location + Dir.Vector() * Speed, 0, 255, 0 );
				}
			}
		}

		PendingVelocity = BestVelocity;
		if( !PendingVelocity.IsNearlyZero() )
		{
			LastProgressTime = WorldInfo->TimeSeconds;
		}

		if( bDebug )
		{
			DrawDebugLine( Location, Location + PendingVelocity, 255, 255, 255 );
			DrawDebugLine( Location + FVector(0,0,20), Location + FVector(0,0,20) + Velocity, 128, 128, 128 );
		}
	}	
}

void AGameCrowdAgentSkeletal::UpdatePendingVelocity( FLOAT DeltaTime )
{

	// note: if not recently rendered, can't use root motion for velocity if skeleton doesn't update when not rendered
	if( bUseRootMotionVelocity && !SkeletalMeshComponent->bNoSkeletonUpdate && (SkeletalMeshComponent->bUpdateSkelWhenNotRendered || (WorldInfo->TimeSeconds - SkeletalMeshComponent->LastRenderTime < 1.f)) )
	{
		PendingVelocity = FVector(0.f);

		// clamp velocity to root motion magnitude
		FLOAT RootMotionTranslationSizeSquared = SkeletalMeshComponent->RootMotionDelta.GetTranslation().SizeSquared();
		if( Velocity.SizeSquared() > RootMotionTranslationSizeSquared/(DeltaTime*DeltaTime) )
		{
			PendingVelocity = Velocity.SafeNormal() * appSqrt(RootMotionTranslationSizeSquared)/DeltaTime;
		}

		if( !PendingVelocity.IsNearlyZero() )
		{
			LastProgressTime = WorldInfo->TimeSeconds;
		}
		
		// if we're using root motion, then clear accumulated root motion
		// See USkeletalMeshComponent::UpdateSkelPose() for details.
		SkeletalMeshComponent->RootMotionDelta.SetTranslation(FVector::ZeroVector);
	}
	else
	{
		Super::UpdatePendingVelocity( DeltaTime );
	}
}

UBOOL AGameCrowdAgent::IsVelocityWithinConstraints( const FRotator& Dir, FLOAT Speed, FLOAT DeltaTime )
{
/*	// Cap the maximum yaw rate
	INT DeltaYaw = CalcDeltaYaw(Dir, Rotation);
	INT MaxYaw = appRound(DeltaTime * MaxYawRate);
	if( Abs<INT>(DeltaYaw) > MaxYaw )
	{
		return FALSE;
	}
*/	
	return TRUE;
}


/** 
  * If desired, take the current location, and using a line check, update its Z to match the ground better. 
  */
UBOOL AGameCrowdAgent::UpdateInterpZTranslation(const FVector& NewLocation)
{
	FVector LineStart = NewLocation + FVector(0.f,0.f,ConformTraceDist);
	FVector LineEnd = NewLocation - FVector(0.f,0.f,GroundOffset+ConformTraceDist);

	// Ground-conforming stuff
	UBOOL bHitGround = FALSE;
	FLOAT GroundHitTime = 1.f;
	FVector GroundHitLocation(0.f), GroundHitNormal(0.f);

	FMemMark LineMark(GMainThreadMemStack);
	if ( (ConformType == CFM_NavMesh) && NavigationHandle )
	{
		// trace against NavMesh
		FCheckResult Hit(1.f);
		NavigationHandle->StaticLineCheck(Hit,LineStart,LineEnd, FVector(0.f));
		if(Hit.Time < GroundHitTime)
		{
			bHitGround = TRUE;
			GroundHitTime = Hit.Time;
			GroundHitLocation = Hit.Location;
			GroundHitNormal = Hit.Normal;
		}
		else
		{
			//FIXMESTEVE - temp workaround for nav mesh trace issues when get too close to obstacle edge
			FCheckResult Result(1.f);
			GWorld->SingleLineCheck(Result, this, LineEnd, LineStart, TRACE_World, FVector(0.f));
			// If we hit something
			if(Result.Time < GroundHitTime)
			{
/*				if ( GroundHitTime == 1.f ) 
				{
					debugf(TEXT("SingleLineCheck hit when conform didn't"));
					DrawDebugLine(LineStart,LineEnd,255,0,0,TRUE);
				}
*/				bHitGround = TRUE;
				GroundHitTime = Result.Time;
				GroundHitLocation = Result.Location;
				GroundHitNormal = Hit.Normal;
			}
		}
		//DrawDebugLine(LineStart,LineEnd,255,0,0,0);
	}
	else
	{
		// Line trace down and look through results
		FCheckResult Result(1.f);
		GWorld->SingleLineCheck(Result, this, LineEnd, LineStart, TRACE_World, FVector(0.f));
		// If we hit something
		if(Result.Time < GroundHitTime)
		{
			bHitGround = TRUE;
			GroundHitTime = Result.Time;
			GroundHitLocation = Result.Location;
			GroundHitNormal = Result.Normal;
		}
	}
	LineMark.Pop();

	FLOAT TargetZ = NewLocation.Z;
	// If we hit something - move to that point
	if(bHitGround)
	{
		// If you end up embedded in the world - kill the crowd member
		if(GroundHitTime < KINDA_SMALL_NUMBER)
		{
			LifeSpan = -0.1f;
		}
		// Otherwise just position at end of line trace.
		else
		{
			TargetZ = GroundHitLocation.Z + GroundOffset;
		}
	}
	// If we didn't move to bottom of line check
	else
	{
		TargetZ = LineEnd.Z + GroundOffset;
	}

//	debugf(TEXT("%s Translate from %f to %f"), *GetName(), NewLocation.Z, TargetZ);
	InterpZTranslation = (TargetZ - NewLocation.Z)/((FLOAT)CurrentConformTraceInterval);
	
	// FIXMESTEVE TEMP DEBUG
	LastGroundZ = GroundHitNormal.Z;

	if ( Square(GroundHitNormal.Z) < 0.9f )
	{
		// Predict slope will continue, so interpolate ahead  FIXMESTEVE IMPROVE
		InterpZTranslation *= 1.5f;

		// update CurrentConformTraceInterval based on distance to player and whether currently on slope
		// FIXMESTEVE = use distance as well
		CurrentConformTraceInterval = ConformTraceInterval/3;
	}
	else
	{
		CurrentConformTraceInterval = ConformTraceInterval;
	}

	return TRUE;
}






///////////////////////////////////
//////////// BEHAVIORS //////////// 
///////////////////////////////////
AGameCrowdBehaviorPoint* UGameCrowdAgentBehavior::TriggerCrowdBehavior(BYTE EventType,class AActor* Instigator,FVector AtLocation,FLOAT InRange,FLOAT InDuration,AActor* BaseActor,UBOOL bRequireLOS)
{
	AGameCrowdBehaviorPoint* BP = NULL;
	if( InDuration < 0.f )
	{
		// Query main collision octree for agents or pawns nearby
		FMemMark Mark(GMainThreadMemStack);
		FCheckResult* Link = GWorld->Hash->RestrictedOverlapCheck(GMainThreadMemStack, GWorld->GetWorldInfo(), AtLocation, InRange );
		for( FCheckResult* result=Link; result; result=result->GetNext())
		{
			checkSlow(result->Actor);
			AGameCrowdAgent* Agent = Cast<AGameCrowdAgent>(result->Actor);
			if( Agent == NULL )
			{
				continue;
			}

			FCheckResult Hit(1.f);
			if( bRequireLOS && !GWorld->SingleLineCheck( Hit, Agent->WorldInfo, Agent->Location, AtLocation,  TRACE_World | TRACE_StopAtAnyHit ) )
			{
				continue;
			}

			Agent->eventHandleBehaviorEvent( EventType, Instigator, FALSE, FALSE );
		}
	}
	else
	{
		BP = Cast<AGameCrowdBehaviorPoint>(GWorld->SpawnActor( AGameCrowdBehaviorPoint::StaticClass(), NAME_None, AtLocation, FRotator(0,0,0), NULL, TRUE ));
		if( BP != NULL )
		{
			BP->SetCollisionSize( InRange, 200.f );
			BP->LifeSpan = InDuration;
			BP->bRequireLOS = bRequireLOS;
			BP->SetBase( BaseActor );
			BP->Initiator = Instigator;
			BP->EventType = EventType;
		}
	}
	return BP;
}

void UGameCrowdAgentBehavior::Tick(FLOAT DeltaTime) 
{
	AWorldInfo* Info = GWorld->GetWorldInfo();
	if( Info != NULL )
	{
		AGameCrowdPopulationManager* PopMgr = Cast<AGameCrowdPopulationManager>(Info->PopulationManager);
		if( PopMgr != NULL && PopMgr->bPauseCrowd )
		{
			return;
		}
	}

	if( ActionTarget )
	{
		// If desired, rotate pawn to look at target when performing action.
		FRotator ToTargetRot = (ActionTarget->Location - MyAgent->Location).Rotation();
		ToTargetRot.Pitch = 0;
		INT DeltaYaw = CalcDeltaYaw(ToTargetRot, MyAgent->Rotation);
		FRotator NewRotation = MyAgent->Rotation;
		FLOAT MaxTurn = DeltaTime * MyAgent->RotateToTargetSpeed;
		if ( Abs(DeltaYaw) < MaxTurn )
		{
			NewRotation.Yaw = ToTargetRot.Yaw;
		}
		else
		{
			if ( DeltaYaw < 0 )
			{
				MaxTurn *= -1.f;
			}		
			NewRotation.Yaw += appRound(MaxTurn);
		}
		MyAgent->SetRotation(NewRotation);

		if ( bFaceActionTargetFirst && (Abs(DeltaYaw) < 400) )
		{
			eventFinishedTargetRotation();
		}
	}

	if( !bIsViralBehavior && 
		DurationBeforeBecomesViral > 0.f &&
		TimeToBecomeViral < GWorld->GetWorldInfo()->TimeSeconds )
	{
//		warnf( TEXT( "Start Viral Behavior %s/%s" ), *MyAgent->GetFullName(), *GetName() );
		bIsViralBehavior = TRUE;
	}
	// check to see if we are going to turn off the "viralness" of the current behavior
	if( bIsViralBehavior && 
		DurationOfViralBehaviorPropagation > 0.0f && 
		TimeToStopPropagatingViralBehavior < GWorld->GetWorldInfo()->TimeSeconds ) 
	{
//		warnf( TEXT( "Stopping Viral Behavior %s/%s" ), *MyAgent->GetFullName(), *GetName() );
		bIsViralBehavior = FALSE;
	}

	// check to see if we are going to stop our behavior due to Duration running out
	if( DurationOfBehavior > 0.0f )
	{
		TimeUntilStopBehavior -= DeltaTime;
		if( TimeUntilStopBehavior <= 0.f )
		{
			MyAgent->eventStopBehavior();
		}
	}
}

/**
  * Set the "Out Agent" output of the current sequence to be MyAgent.
  */
void UGameCrowdBehavior_PlayAnimation::SetSequenceOutput()
{
	TArray<UObject**> AgentVars;
	AnimSequence->GetObjectVars(AgentVars,TEXT("Out Agent"));
	for (INT Idx = 0; Idx < AgentVars.Num(); Idx++)
	{
		*(AgentVars(Idx)) = MyAgent;
	}
}


UBOOL AGameCrowdDestination::ReachedByAgent(AGameCrowdAgent *Agent, FVector TestPosition, UBOOL bTestExactly)
{
	return ((Location - TestPosition).SizeSquared2D() < Square((bTestExactly && bMustReachExactly) ? ExactReachTolerance : Agent->ReachThreshold*CylinderComponent->CollisionRadius)) && Abs(Location.Z - TestPosition.Z) < CylinderComponent->CollisionHeight + Agent->GroundOffset/2.f;
}

UBOOL AGameCrowdDestinationQueuePoint::QueueReachedBy(AGameCrowdAgent *Agent, FVector TestPosition)
{
	return ((Location - TestPosition).SizeSquared2D() < Square(CylinderComponent->CollisionRadius)) && Abs(Location.Z - TestPosition.Z) < CylinderComponent->CollisionHeight + Agent->GroundOffset/2.f;
}

UBOOL AGameCrowdAgent::ShouldEndIdle()
{
	return !CurrentBehavior || CurrentBehavior->ShouldEndIdle();
}

UBOOL UGameCrowdAgentBehavior::ShouldEndIdle()
{
	return !bIdleBehavior;
}

UBOOL UGameCrowdBehavior_WaitInQueue::ShouldEndIdle()
{
	if ( QueuePosition )
	{
		// if waiting in line and have reached queue position, wait till line moves
		bIdleBehavior = QueuePosition->QueueReachedBy(MyAgent, MyAgent->Location);
		return !bIdleBehavior;
	}
	else
	{
		debugf(TEXT("Queue behavior with no queue position!"));
		bIdleBehavior = FALSE;
		return TRUE;
	}
}

UBOOL UGameCrowdBehavior_WaitForGroup::ShouldEndIdle()
{
	UBOOL bShouldEndIdle = TRUE;

	// see if laggards have caught up or are no longer closing in.
	if ( MyAgent->MyGroup )
	{
		for ( INT i=0; i<MyAgent->MyGroup->Members.Num(); i++ )
		{
			if ( MyAgent->MyGroup->Members(i) && !MyAgent->MyGroup->Members(i)->bDeleteMe && ((MyAgent->MyGroup->Members(i)->Location - MyAgent->Location).SizeSquared() > MyAgent->DesiredGroupRadiusSq) 
				&& ((MyAgent->MyGroup->Members(i)->Velocity | (MyAgent->Location - MyAgent->MyGroup->Members(i)->Location)) > 0.f))
			{
				bShouldEndIdle = FALSE;
				break;
			}
		}
	}
	if ( bShouldEndIdle )
	{
		MyAgent->eventStopBehavior();
	}
	return bShouldEndIdle;
}

/** 
  *  Creates a new behavior object based on the passed in archetype ,
  *  and assigns it to be the agent's CurrentBehavior
  *
  * @PARAM BehaviorArchetype is the archetype for the new behavior
  */
void AGameCrowdAgent::SetCurrentBehavior( UGameCrowdAgentBehavior* BehaviorArchetype )
{
	CurrentBehavior = ConstructObject<UGameCrowdAgentBehavior>( BehaviorArchetype->GetClass(), this, NAME_None, 0, BehaviorArchetype );
	if( CurrentBehavior != NULL )
	{
		CurrentBehaviorActivationTime = WorldInfo->TimeSeconds;
	}
}

/**
  * Check if reached intermediate point in route to destination
  */
UBOOL AGameCrowdAgent::ReachedIntermediatePoint()
{
	// check if close enough
	FVector Dist = Location - IntermediatePoint;
	if ( Abs(Dist.Z) < 2.f*SearchExtent.Z )
	{
		Dist.Z = 0.f;
		if ( Dist.SizeSquared() < Square(2.f*SearchExtent.X) )
		{
			return TRUE;
		}
	}

	if ( NavigationHandle && NavigationHandle->CurrentEdge )
	{
		// if following navmesh path, see if on destination poly
		// FIXMESTEVE - move to navigationhandle after discussin with MattT
		if ( NavigationHandle->AnchorPoly )
		{
			FNavMeshPolyBase* OtherPoly = NavigationHandle->CurrentEdge->GetOtherPoly(NavigationHandle->AnchorPoly); 
			if ( OtherPoly->ContainsPoint(Location,TRUE) )
			{
				//debugf(TEXT("%s on destination poly at %f"), *GetName(), WorldInfo->TimeSeconds);
				return TRUE;
			}
		}
	}
	return FALSE;
}

/**
  * Handles movement destination updating for agent.
  *
  * @RETURNS true if destination updating was handled
  */ 
UBOOL UGameCrowdAgentBehavior::HandleMovement()
{
	return FALSE;
}

/**
  * Handles movement destination updating for agent.
  *
  * @RETURNS true if destination updating was handled
  */ 
UBOOL UGameCrowdBehavior_WaitInQueue::HandleMovement()
{
	if (QueuePosition )
	{
		// moving toward QueuePosition
		if ( QueuePosition->QueueReachedBy(MyAgent, MyAgent->Location) )
		{
			QueuePosition->eventReachedDestination(MyAgent);
		}
		else if ( (MyAgent->IntermediatePoint != QueuePosition->Location) && MyAgent->ReachedIntermediatePoint() )
		{
			MyAgent->eventUpdateIntermediatePoint(QueuePosition);
		}
	}
	return TRUE;
}

/** Update position of an 'agent' in a crowd, using properties in the action. */
void AGameCrowdAgent::TickSpecial(FLOAT DeltaTime)
{
	Super::TickSpecial(DeltaTime);

	if( bDeleteMe )
	{
		return;
	}

	if( CurrentDestination )
	{
		// FIXMESTEVE - merge IsIdle() and HandleMovement() here?
		if ( IsIdle() )
		{
			if( ShouldEndIdle() )
			{
				if ( CurrentDestination->ReachedByAgent(this, Location, FALSE) )
				{
					CurrentDestination->eventReachedDestination(this);
				}
			}
		}
		else if ( !CurrentBehavior || !CurrentBehavior->HandleMovement() )
		{
			if ( bWantsGroupIdle && !CurrentBehavior && (GroupWaitingBehaviors.Num() > 0) )
			{
				// Too far ahead of group, wait if necessary
				eventWaitForGroupMembers();
			}
			// check if have reached destination, if so perform appropriate action
			else if ( CurrentDestination->ReachedByAgent(this, Location, TRUE) )
			{
				CurrentDestination->eventReachedDestination(this);
			}
			else if ( IntermediatePoint != CurrentDestination->Location && ReachedIntermediatePoint() )
			{
				eventUpdateIntermediatePoint(CurrentDestination);
			}
		}
	}

	// tick behavior
	if( CurrentBehavior )
	{
		CurrentBehavior->eventTick(DeltaTime);
	}
}

#if WITH_EDITOR
void AGameCrowdInteractionPoint::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	const FVector ModifiedScale = DeltaScale * 500.0f;

	if ( bCtrlDown )
	{
		// CTRL+Scaling modifies trigger collision height.  This is for convenience, so that height
		// can be changed without having to use the non-uniform scaling widget (which is
		// inaccessable with spacebar widget cycling).
		CylinderComponent->CollisionHeight += ModifiedScale.X;
		CylinderComponent->CollisionHeight = Max( 0.0f, CylinderComponent->CollisionHeight );
	}
	else
	{
		CylinderComponent->CollisionRadius += ModifiedScale.X;
		CylinderComponent->CollisionRadius = Max( 0.0f, CylinderComponent->CollisionRadius );

		// If non-uniformly scaling, Z scale affects height and Y can affect radius too.
		if ( !ModifiedScale.AllComponentsEqual() )
		{
			CylinderComponent->CollisionHeight += -ModifiedScale.Z;
			CylinderComponent->CollisionHeight = Max( 0.0f, CylinderComponent->CollisionHeight );

			CylinderComponent->CollisionRadius += ModifiedScale.Y;
			CylinderComponent->CollisionRadius = Max( 0.0f, CylinderComponent->CollisionRadius );
		}
	}
}
#endif

void AGameCrowdBehaviorPoint::TickSpecial( FLOAT DeltaTime )
{
	SCOPE_CYCLE_COUNTER(STAT_CrowdTotal);
	SCOPE_CYCLE_COUNTER(STAT_CrowdForce);

	Super::TickSpecial(DeltaTime);

	// check for overlapping crowd agents
	if( bIsEnabled )
	{
		//FindTouchingActors();
		FMemMark Mark(GMainThreadMemStack);
		
		TLookupMap<AActor*> NewTouching;
		
		FLOAT ColRadius, ColHeight;
		GetBoundingCylinder(ColRadius, ColHeight);
		UBOOL bIsZeroExtent = (ColRadius == 0.f) && (ColHeight == 0.f);
		FCheckResult* FirstHit = GWorld->Hash ? GWorld->Hash->ActorEncroachmentCheck( GMainThreadMemStack, this, Location, Rotation, TRACE_AllColliding ) : NULL;	
		for( FCheckResult* Test = FirstHit; Test; Test=Test->GetNext() )
		{
			if(	Test->Actor!=this && !Test->Actor->IsBasedOn(this) && Test->Actor != GWorld->GetWorldInfo() )
			{
				if( !IsBlockedBy(Test->Actor,Test->Component)
					&& (!Test->Component || (bIsZeroExtent ? Test->Component->BlockZeroExtent : Test->Component->BlockNonZeroExtent)) )
				{
					// Make sure Test->Location is not Zero, if that's the case, use Location
					FVector	HitLocation = Test->Location.IsZero() ? Location : Test->Location;

					// Make sure we have a valid Normal
					FVector NormalDir = Test->Normal.IsZero() ? (Location - HitLocation) : Test->Normal;
					if( !NormalDir.IsZero() )
					{
						NormalDir.Normalize();
					}
					else
					{
						NormalDir = FVector(0,0,1.f);
					}

					NewTouching.AddItem(Test->Actor);
					BeginTouch( Test->Actor, Test->Component, HitLocation, NormalDir, Test->SourceComponent );
				}
			}						
		}

		for(INT Idx=0;Idx<Touching.Num();)
		{
			if(Touching(Idx) && !NewTouching.Find(Touching(Idx)))		
			{
				EndTouch(Touching(Idx),0);
			}
			else
			{
				Idx++;
			}
		}

		Mark.Pop();	
	}
}

/** 
  * Special version of IsOverlapping to handle touching crowd agents
  */
UBOOL AGameCrowdBehaviorPoint::IsOverlapping( AActor* Other, FCheckResult* Hit, UPrimitiveComponent* OtherPrimitiveComponent, UPrimitiveComponent* MyPrimitiveComponent )
{
	checkSlow(Other!=NULL);

	SCOPE_CYCLE_COUNTER(STAT_CrowdForce);
	if( !bCollideActors || !CollisionComponent || !CollisionComponent->CollideActors )
	{
		return FALSE;
	}

	// FIXMESTEVE - not just for skeletal
	AGameCrowdAgent* Agent = Cast<AGameCrowdAgent>(Other);
	if( !Agent )
	{
		return Super::IsOverlapping(Other, Hit, OtherPrimitiveComponent, MyPrimitiveComponent);
	}

	// Can't overlap actors with collision turned off.
	if( !Agent->bCollideActors )
	{
		return FALSE;
	}

	// Now have 2 actors we want to overlap, so we have to pick which one to treat as a box and test against the PrimitiveComponents of the other.
	AActor* PrimitiveActor = this;
	AActor* BoxActor = Other;

	// If we were not supplied an FCheckResult, use a temp one.
	FCheckResult TestHit;
	if(Hit==NULL)
	{
		Hit = &TestHit;
	}

	// Check bounding box of collision component against each colliding PrimitiveComponent.
	FBox BoxActorBox;
	AGameCrowdAgentSkeletal* AgentSkel = Cast<AGameCrowdAgentSkeletal>(Other);
	AGameCrowdAgentSM*		 AgentSM = Cast<AGameCrowdAgentSM>(Other);
	if( AgentSkel != NULL )
	{
		BoxActorBox = AgentSkel->SkeletalMeshComponent->Bounds.GetBox();
	}
	else if( AgentSM != NULL )
	{
		BoxActorBox = AgentSM->Mesh->Bounds.GetBox();
	}

	if(BoxActorBox.IsValid)
	{
		// adjust box position since overlap check is for different location than actor's current location
		BoxActorBox.Min -= OverlapAdjust;
		BoxActorBox.Max -= OverlapAdjust;
	}

	// If we failed to get a valid bounding box from the Box actor, we can't get an overlap.
	if(!BoxActorBox.IsValid)
	{
		return FALSE;
	}

	FVector BoxCenter, BoxExtent;
	BoxActorBox.GetCenterAndExtents(BoxCenter, BoxExtent);

	// DEBUGGING: Time how long the point checks take, and print if its more than PERF_SHOW_SLOW_OVERLAPS_TAKING_LONG_TIME_AMOUNT.
#if defined(PERF_SHOW_SLOW_OVERLAPS) || LOOKING_FOR_PERF_ISSUES
	DWORD Time=0;
	CLOCK_CYCLES(Time);
#endif

	// Check agent box against my collisioncomponent.
	if( CollisionComponent->PointCheck(*Hit, BoxCenter, BoxExtent, 0) == 0 )
	{
		Hit->Component = CollisionComponent;
	}

#if defined(PERF_SHOW_SLOW_OVERLAPS) || LOOKING_FOR_PERF_ISSUES
	const static FLOAT PERF_SHOW_SLOW_OVERLAPS_TAKING_LONG_TIME_AMOUNT = 0.05f; // modify this value to look at larger or smaller sets of "bad" actors
	UNCLOCK_CYCLES(Time);
	FLOAT MSec = Time * GSecondsPerCycle * 1000.f;
	if( MSec > PERF_SHOW_SLOW_OVERLAPS_TAKING_LONG_TIME_AMOUNT )
	{
		debugf( NAME_PerfWarning, TEXT("IsOverLapping: Testing: P:%s - B:%s Time: %f"), *(PrimitiveActor->GetPathName()), *(BoxActor->GetPathName()), MSec );
	}
#endif


	return Hit->Component != NULL;
}


UBOOL USeqEvent_CrowdAgentReachedDestination::CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest, TArray<INT>* ActivateIndices, UBOOL bPushTop)
{
	UBOOL bResult = Super::CheckActivate(InOriginator, InInstigator, bTest, ActivateIndices, bPushTop);

	if( bResult && !bTest )
	{
		AGameCrowdAgent* Agent = Cast<AGameCrowdAgent>(InInstigator);
		if( Agent )
		{
			// see if any Agent variables are attached
			TArray<UObject**> AgentVars;
			GetObjectVars(AgentVars,TEXT("Agent"));
			for (INT Idx = 0; Idx < AgentVars.Num(); Idx++)
			{
				*(AgentVars(Idx)) = Agent;
			}
		}
	}

	return bResult;
}

void USeqAct_PlayAgentAnimation::Activated()
{
	// START
	if( InputLinks(0).bHasImpulse )
	{
		// cache ActionTarget
		TArray<UObject**> Objs;
		GetObjectVars(Objs,TEXT("Action Focus"));
		for(INT Idx = 0; Idx < Objs.Num(); Idx++)
		{
			AActor* TestActor = Cast<AActor>( *(Objs(Idx)) );
			if (TestActor != NULL)
			{
				// use the pawn instead of the controller
				if ( TestActor->GetAController() && TestActor->GetAController()->Pawn )
				{
					TestActor = TestActor->GetAController()->Pawn;
				}

				ActionTarget = TestActor;
				break;
			}
		}
	}

	Super::Activated();
}

UBOOL USeqAct_PlayAgentAnimation::UpdateOp(FLOAT DeltaTime)
{
	// call the script handler if we catch a stop while active
	if (InputLinks.Num() > 1 && InputLinks(1).bHasImpulse)
	{
		USequenceAction::Activated();
	}
	return Super::UpdateOp(DeltaTime);
}

/** AGameCrowdPopulationManager IMPLEMENT Interface_NavigationHandle */
void AGameCrowdPopulationManager::SetupPathfindingParams( FNavMeshPathParams& out_ParamCache )
{
	VERIFY_NAVMESH_PARAMS(9)
	if(QueryingAgent != NULL)
	{
		out_ParamCache.bAbleToSearch = TRUE;
		out_ParamCache.SearchExtent = QueryingAgent->SearchExtent;
		out_ParamCache.SearchStart = QueryingAgent->Location;
	}
	else
	{
		out_ParamCache.bAbleToSearch = FALSE;
		out_ParamCache.SearchExtent = FVector(0.f);
		out_ParamCache.SearchStart = Location;
	}
	out_ParamCache.SearchLaneMultiplier = 0.f;
	out_ParamCache.bCanMantle = FALSE;
	out_ParamCache.bNeedsMantleValidityTest = FALSE;
	out_ParamCache.MaxDropHeight = 0.f;
	out_ParamCache.MinWalkableZ = 0.7f;
	out_ParamCache.MaxHoverDistance = -1.f;
}

FVector AGameCrowdPopulationManager::GetEdgeZAdjust(FNavMeshEdgeBase* Edge)
{
	if(QueryingAgent != NULL)
	{
		return FVector(0.f,0.f,QueryingAgent->SearchExtent.Z);
	}
	else
	{
		return FVector(0.f);
	}
}

void AGameCrowdAgent::InitForPathfinding()
{
	CurrentPathLaneValue = appFrand() * MaxPathLaneValue;
}

void AGameCrowdAgent::SetupPathfindingParams( FNavMeshPathParams& out_ParamCache )
{
	VERIFY_NAVMESH_PARAMS(9)
	out_ParamCache.bAbleToSearch = TRUE;
	out_ParamCache.SearchExtent = SearchExtent;
	out_ParamCache.SearchLaneMultiplier = CurrentPathLaneValue; 
	out_ParamCache.SearchStart = Location;
	out_ParamCache.bCanMantle = FALSE;
	out_ParamCache.bNeedsMantleValidityTest = FALSE;
	out_ParamCache.MaxDropHeight = 0.f;
	out_ParamCache.MinWalkableZ = 0.7f;
	out_ParamCache.MaxHoverDistance = -1.f;
}

INT AGameCrowdAgent::ExtraEdgeCostToAddWhenActive(FNavMeshEdgeBase* Edge)
{
	return ExtraPathCost;
}

FVector AGameCrowdAgent::GetEdgeZAdjust(FNavMeshEdgeBase* Edge)
{
	return FVector(0.f,0.f,SearchExtent.Z);
}

UBOOL AGameCrowdAgent::Tick( FLOAT DeltaTime, ELevelTick TickType )
{
	SCOPE_CYCLE_COUNTER(STAT_CrowdTotal);
	SCOPE_CYCLE_COUNTER(STAT_AgentTick);
	
	return Super::Tick(DeltaTime, TickType);
}

/** 
 * This will allow subclasses to implement specialized behavior for whether or not to actually simulate. 
 * Base behavior is to reduce update frequency based on how long since an agent has been visible.
 **/
UBOOL AGameCrowdAgent::ShouldPerformCrowdSimulation(FLOAT DeltaTime) 
{
	if( GWorld->bDisableCrowds )
	{
		return FALSE;
	}
	// don't perform simulation if dead crowd member
	if ( Health < 0 )
	{
		return FALSE;
	}

	ForceUpdateTime = ::Max(LastRenderTime, ForceUpdateTime);
	if( NotVisibleLifeSpan > 0.f && WorldInfo->TimeSeconds - ForceUpdateTime > NotVisibleLifeSpan )
	{
		AGameCrowdPopulationManager* PopMgr = Cast<AGameCrowdPopulationManager>(WorldInfo->PopulationManager);
		if( PopMgr != NULL )
		{
			// don't destroy if still in line of sight and not too far away
			UBOOL bLineOfSight = FALSE;
			PopMgr->GetPlayerInfo();
			for( INT Idx = 0; Idx < PopMgr->PlayerInfo.Num(); Idx++ )
			{
				FCrowdSpawnerPlayerInfo& Info = PopMgr->PlayerInfo(Idx);
				const FLOAT DistSq = (Location - Info.ViewLocation).SizeSquared();
				if( DistSq < MaxLOSLifeDistanceSq )
				{
					// if heading for visible or soon to be visible destination, and not too far away, keep alive
					if( CurrentDestination != NULL && (CurrentDestination->bIsVisible || CurrentDestination->bWillBeVisible) )
					{
						ForceUpdateTime = WorldInfo->TimeSeconds;
						bLineOfSight = TRUE;
						break;
					}
					else 
					{
						// check if line of sight
						FCheckResult Hit(1.f);
						if( GWorld->SingleLineCheck( Hit, this, Location, Info.ViewLocation, TRACE_World | TRACE_StopAtAnyHit ) )
						{
							bLineOfSight = TRUE;
							ForceUpdateTime = WorldInfo->TimeSeconds + 3.f;
							break;
						}
					}
				}
			}

			// if not in L.O.S. and not visible for a while, kill agent
			if( !bLineOfSight )
			{
				eventKillAgent();
				return FALSE;
			}
		}
	}

	return TRUE;
}

//=============================================================================
// FConnectionRenderingSceneProxy

/** Represents a GameDestinationConnRenderingComponent to the scene manager. */
class FConnectionRenderingSceneProxy : public FDebugRenderSceneProxy
{
public:

	FConnectionRenderingSceneProxy(const UGameDestinationConnRenderingComponent* InComponent):
	FDebugRenderSceneProxy(InComponent)
	{
		// only on selected GameCrowdDestination
			// draw destination connections
			AGameCrowdDestination* Dest = Cast<AGameCrowdDestination>(InComponent->GetOwner());

			if( Dest )
			{
				if( Dest->NextDestinations.Num() > 0 )
				{
					for (INT Idx = 0; Idx < Dest->NextDestinations.Num(); Idx++)
					{
						AGameCrowdDestination* NextDest = Dest->NextDestinations(Idx);
						if( NextDest )
						{
							FLinearColor Color = FLinearColor(1.f,1.f,0.f,1.f);
							new(Lines) FDebugRenderSceneProxy::FDebugLine(Dest->Location, NextDest->Location,Color);
						}
					}
				}
				AActor* Prev = Dest;
				for ( AGameCrowdDestinationQueuePoint* QueuePoint=Dest->QueueHead; QueuePoint!=NULL; QueuePoint=QueuePoint->NextQueuePosition )
				{
					FLinearColor Color = FLinearColor(1.f,0.3f,1.f,1.f);
					new(Lines) FDebugRenderSceneProxy::FDebugLine(Prev->Location, QueuePoint->Location,Color);
					Prev = QueuePoint;
				}
			}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Result;
		Result.bDynamicRelevance = IsShown(View) && bSelected;
		Result.SetDPG(SDPG_World,TRUE);
		if (IsShadowCast(View))
		{
			Result.bShadowRelevance = TRUE;
		}
		return Result;
	}

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FDebugRenderSceneProxy::GetAllocatedSize() ); }
};


//=============================================================================
// UGameDestinationConnRenderingComponent

/**
 * Creates a new scene proxy for the path rendering component.
 * @return	Pointer to the FPathRenderingSceneProxy
 */
FPrimitiveSceneProxy* UGameDestinationConnRenderingComponent::CreateSceneProxy()
{
	return new FConnectionRenderingSceneProxy(this);
}

void UGameDestinationConnRenderingComponent::UpdateBounds()
{
	FBox BoundingBox(0);

	AGameCrowdDestination* Dest = Cast<AGameCrowdDestination>(Owner);
	if ( Dest )
	{
		if ( Dest->NextDestinations.Num() > 0)
		{
			for (INT Idx = 0; Idx < Dest->NextDestinations.Num(); Idx++)
			{
				AGameCrowdDestination* End = Dest->NextDestinations(Idx);
				if( End )
				{
					BoundingBox += Dest->Location;
					BoundingBox += End->Location;
				}
			}
		}
		for ( AGameCrowdDestinationQueuePoint* QueuePoint=Dest->QueueHead; QueuePoint!=NULL; QueuePoint=QueuePoint->NextQueuePosition )
		{
			BoundingBox += Dest->Location;
			BoundingBox += QueuePoint->Location;
		}
	}
	Bounds = FBoxSphereBounds(BoundingBox);
}

/**
Hook to allow actors to render HUD overlays for themselves.
Assumes that appropriate font has already been set
*/
void AGameCrowdAgent::NativePostRenderFor(APlayerController *PC, UCanvas *Canvas, FVector CameraPosition, FVector CameraDir)
{
	if ( (WorldInfo->TimeSeconds - LastRenderTime < 0.1f)
		&& ((CameraDir | (Location - CameraPosition)) > 0.f) 
		&& ((CameraPosition - Location).SizeSquared() * Square(PC->LODDistanceFactor) < Square(BeaconMaxDist)) )
	{
		eventPostRenderFor(PC, Canvas, CameraPosition, CameraDir);
	}
}

/** 
  * EditorLinkSelectionInterface 
  */
void AGameCrowdDestination::LinkSelection(USelection* SelectedActors)
{
	UBOOL bFoundOtherDest = FALSE;
	for( INT SelectedIdx=0; SelectedIdx<SelectedActors->Num(); SelectedIdx++ )
	{
		AGameCrowdDestination* CurDest = Cast<AGameCrowdDestination>((*SelectedActors)(SelectedIdx));
		if( CurDest )
		{
			if ( CurDest != this )
			{
				// if not in the list yet, add it
				NextDestinations.AddUniqueItem(CurDest);
				bFoundOtherDest = TRUE;
			}
		}
	}

	if ( !bFoundOtherDest )
	{
		// connect any queue points to this destination
		for( INT SelectedIdx=0; SelectedIdx<SelectedActors->Num(); SelectedIdx++ )
		{
			AGameCrowdDestinationQueuePoint* NewQueuePt = Cast<AGameCrowdDestinationQueuePoint>((*SelectedActors)(SelectedIdx));
			if( NewQueuePt )
			{
				// check if already in the list
				UBOOL bFoundInList = FALSE;

				if ( NewQueuePt->NextQueuePosition )
				{
					//already part of a list
					bFoundInList = TRUE;
				}
				else
				{
					for ( AGameCrowdDestinationQueuePoint* QueuePt=QueueHead; QueuePt!=NULL; QueuePt=QueuePt->NextQueuePosition )
					{
						if ( QueuePt == NewQueuePt )
						{
							bFoundInList = TRUE;
							break;
						}
					}
				}
				// NOTE: if if QueuePt is in another list as tail  (this could also be done by hand changing NextQueuePosition)
				// This is checked for in Check For Errors and when actually using a Queue

				// not in list, so insert it at appropriate position, based on distance
				if ( !bFoundInList )
				{
					if ( !QueueHead )
					{
						QueueHead = NewQueuePt;
					}
					else if ( (Location - NewQueuePt->Location).SizeSquared() < (Location - QueueHead->Location).SizeSquared() )
					{
						NewQueuePt->NextQueuePosition = QueueHead;
						QueueHead = NewQueuePt;
					}
					else
					{
						for ( AGameCrowdDestinationQueuePoint* QueuePt=QueueHead; QueuePt!=NULL; QueuePt=QueuePt->NextQueuePosition )
						{
							if ( QueuePt->NextQueuePosition )
							{
								if ( (Location - NewQueuePt->Location).SizeSquared() < (Location - QueuePt->NextQueuePosition->Location).SizeSquared() )
								{
									NewQueuePt->NextQueuePosition = QueuePt->NextQueuePosition;
									QueuePt->NextQueuePosition = NewQueuePt;
								}											
							}
							else
							{
								QueuePt->NextQueuePosition = NewQueuePt;
								break;
							}
						}
					}
				}
			}
		}
	}

	// update drawn connections
	UGameDestinationConnRenderingComponent *Comp = NULL;
	if ( Components.FindItemByClass<UGameDestinationConnRenderingComponent>(&Comp) )
	{
		FComponentReattachContext Context(Comp);
	}	
}

/** 
  * EditorLinkSelectionInterface 
  */
void AGameCrowdDestination::UnLinkSelection(USelection* SelectedActors)
{
	UBOOL bFoundOtherDest = FALSE;
	for( INT SelectedIdx=0; SelectedIdx<SelectedActors->Num(); SelectedIdx++ )
	{
		AGameCrowdDestination* CurDest = Cast<AGameCrowdDestination>((*SelectedActors)(SelectedIdx));
		if( CurDest )
		{
			if ( CurDest != this )
			{
				// if not in the list yet, add it
				NextDestinations.RemoveItem(CurDest);
				bFoundOtherDest = TRUE;
			}
		}
	}

	// update drawn connections
	UGameDestinationConnRenderingComponent *Comp = NULL;
	if ( Components.FindItemByClass<UGameDestinationConnRenderingComponent>(&Comp) )
	{
		FComponentReattachContext Context(Comp);
	}	
}

#if WITH_EDITOR
void AGameCrowdDestination::CheckForErrors()
{
	// make sure this destination doesn't have a queue with shared queue points
	for ( AGameCrowdDestinationQueuePoint* QueuePt=QueueHead; QueuePt!=NULL; QueuePt=QueuePt->NextQueuePosition )
	{
		if ( QueuePt->QueueDestination && (QueuePt->QueueDestination != this) )
		{
			// maybe shared - check if still attached to other destination
			UBOOL bFoundShared = FALSE;
			for ( AGameCrowdDestinationQueuePoint* SharedQueuePt=QueuePt->QueueDestination->QueueHead; SharedQueuePt!=NULL; SharedQueuePt=SharedQueuePt->NextQueuePosition )
			{
				if ( SharedQueuePt == QueuePt )
				{
					bFoundShared = TRUE;
					break;
				}
			}
			if ( bFoundShared )
			{
				GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_SharesQueuePoint" ), *QueuePt->GetName(), *QueuePt->QueueDestination->GetName() ) ), TEXT( "SharesQueuePoint" ) );
			}
		}
		QueuePt->QueueDestination = this;
	}
}
#endif


FString AGameCrowdAgent::GetDetailedInfoInternal() const
{
	FString Result;  

	if( MyArchetype != NULL )
	{
		Result = MyArchetype->GetName();
	}
	else
	{
		Result = TEXT("No_MyArchetype");
	}

	return Result;  
}


/*
void USeqAct_GameCrowdSpawner::KillAgents()
{
	// Iterate over list of dudes
	for(INT i=0; i<SpawnedList.Num(); i++)
	{
		AGameCrowdAgent* Agent = SpawnedList(i);
		if(Agent)
		{
			GWorld->DestroyActor(Agent);
		}
	}
	SpawnedList.Empty();

	// Tell clients if necessary
	if(GWorld->GetNetMode() != NM_Client && RepActor)
	{
		RepActor->DestroyAllCount++;
		RepActor->bNetDirty = TRUE;
	}
}
*/

UBOOL AGameCrowdPopulationManager::GetSpawnInfoItem( USeqAct_GameCrowdPopulationManagerToggle* inAction, FCrowdSpawnInfoItem*& out_Spawner, UBOOL bCreateIfNotFound )
{
	if( !inAction->bIndividualSpawner )
	{
		out_Spawner = &CloudSpawnInfo;
		return TRUE;
	}

	INT FoundIdx = -1;
	for( INT Idx = 0; Idx < ScriptedSpawnInfo.Num(); Idx++ )
	{
		FCrowdSpawnInfoItem& Item = ScriptedSpawnInfo(Idx);
		if( Item.SeqSpawner == inAction )
		{
			FoundIdx = Idx;
			break;
		}
	}
		
	if( FoundIdx < 0 )
	{
		if( !bCreateIfNotFound )
		{
			return FALSE;
		}
		FoundIdx = eventCreateSpawner( inAction );
	}

	out_Spawner = &ScriptedSpawnInfo(FoundIdx);
	return TRUE;
}

void USeqAct_GameCrowdPopulationManagerToggle::Activated()
{
	Super::Activated();
}

UBOOL USeqAct_GameCrowdPopulationManagerToggle::UpdateOp(FLOAT DeltaTime)
{
	AWorldInfo* Info = GWorld->GetWorldInfo();
	if( Info != NULL )
	{
		AGameCrowdPopulationManager* PM = Cast<AGameCrowdPopulationManager>(Info->PopulationManager);
		if( PM != NULL )
		{
			FCrowdSpawnInfoItem* SpawnerInfo = NULL;
			if( PM->GetSpawnInfoItem( this, SpawnerInfo, TRUE ) )
			{
				if( (InputLinks.IsValidIndex(0) && InputLinks(0).bHasImpulse) || (InputLinks.IsValidIndex(2) && InputLinks(2).bHasImpulse) )
				{
					if( !SpawnerInfo->bSpawningActive )
					{
						eventFillCrowdSpawnInfoItem( *SpawnerInfo, PM );
						SpawnerInfo->SeqSpawner = this;
						SpawnerInfo->bSpawningActive = TRUE;
					}					
				}

				if( (InputLinks.IsValidIndex(1) && InputLinks(1).bHasImpulse) || (InputLinks.IsValidIndex(4) && InputLinks(4).bHasImpulse) )
				{
					SpawnerInfo->bSpawningActive = FALSE;
				}

				if( InputLinks.IsValidIndex(2) && InputLinks(2).bHasImpulse )
				{
					if( WarmupPopulationPct > 0.f )
					{
						INT WarmupNum = Min<FLOAT>(WarmupPopulationPct, 1.f) * SpawnerInfo->SpawnNum;
						if( PM->Warmup( *SpawnerInfo, WarmupNum ) )
						{
							USeqAct_GameCrowdPopulationManagerToggle* SeqSpawner = SpawnerInfo->SeqSpawner;
							if( SeqSpawner != NULL )
							{
								USeqVar_ObjectList* AList = Cast<USeqVar_ObjectList>((SeqSpawner->VariableLinks(0).LinkedVariables(0)));
								if( AList != NULL )
								{
									AList->ObjList.Empty();
									for( INT AgentIdx = 0; AgentIdx < SeqSpawner->LastSpawnedList.Num(); AgentIdx++ )
									{
										AList->ObjList.AddUniqueItem(SeqSpawner->LastSpawnedList(AgentIdx));
									}
								}
								SeqSpawner->ActivateOutputLink(0);
							}
						}
					}
				}

				if( (InputLinks.IsValidIndex(3) && InputLinks(3).bHasImpulse) || (InputLinks.IsValidIndex(4) && InputLinks(4).bHasImpulse) )
				{
					PM->eventFlushAgents( *SpawnerInfo );
				}

				return !SpawnerInfo->bSpawningActive;
			}
		}
	}

	return TRUE;
}


void AGameCrowdPopulationManager::UpdateAllSpawners( FLOAT DeltaTime )
{
	if( eventUpdateSpawner( CloudSpawnInfo, DeltaTime ) )
	{
		USeqAct_GameCrowdPopulationManagerToggle* SeqSpawner = CloudSpawnInfo.SeqSpawner;
		if( SeqSpawner != NULL )
		{
			USeqVar_ObjectList* AList = Cast<USeqVar_ObjectList>((SeqSpawner->VariableLinks(0).LinkedVariables(0)));
			if( AList != NULL )
			{
				AList->ObjList.Empty();
				for( INT AgentIdx = 0; AgentIdx < SeqSpawner->LastSpawnedList.Num(); AgentIdx++ )
				{
					AList->ObjList.AddUniqueItem(SeqSpawner->LastSpawnedList(AgentIdx));
				}
			}
			SeqSpawner->ActivateOutputLink(0);
		}
	}

	for( INT Idx = 0; Idx < ScriptedSpawnInfo.Num(); Idx++ )
	{
		if( eventUpdateSpawner( ScriptedSpawnInfo(Idx), DeltaTime ) )
		{
			USeqAct_GameCrowdPopulationManagerToggle* SeqSpawner = ScriptedSpawnInfo(Idx).SeqSpawner;
			if( SeqSpawner != NULL )
			{
				USeqVar_ObjectList* AList = Cast<USeqVar_ObjectList>((SeqSpawner->VariableLinks(0).LinkedVariables(0)));
				if( AList != NULL )
				{
					AList->ObjList.Empty();
					for( INT AgentIdx = 0; AgentIdx < SeqSpawner->LastSpawnedList.Num(); AgentIdx++ )
					{
						AList->ObjList.AddUniqueItem(SeqSpawner->LastSpawnedList(AgentIdx));
					}
				}
				SeqSpawner->ActivateOutputLink(0);
			}
		}
	}
}


/**
  *  Implementation just for stats gathering
  */
UBOOL AGameCrowdPopulationManager::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	SCOPE_CYCLE_COUNTER(STAT_CrowdTotal);
	SCOPE_CYCLE_COUNTER(STAT_CrowdPopMgr);

	if( TickType == LEVELTICK_All )
	{
		TickSpawnInfo( CloudSpawnInfo, DeltaTime );
		for( INT ItemIdx = 0; ItemIdx < ScriptedSpawnInfo.Num(); ItemIdx++ )
		{
			TickSpawnInfo( ScriptedSpawnInfo(ItemIdx), DeltaTime );
		}
	}

	return Super::Tick(DeltaTime, TickType);
}

void AGameCrowdPopulationManager::TickSpawnInfo( FCrowdSpawnInfoItem& Item, FLOAT DeltaTime )
{
	if( Item.NumAgentsToTickPerFrame >= 0 )
	{
		TArray<AGameCrowdAgent*> AgentsToTick;
		INT NumToTick = Min<INT>(Item.ActiveAgents.Num(), Item.NumAgentsToTickPerFrame);
		INT TickAttempts = 0;
		while( NumToTick > 0 && TickAttempts < Item.ActiveAgents.Num() )
		{
			Item.LastAgentTickedIndex = (Item.LastAgentTickedIndex + 1) % Item.ActiveAgents.Num();
			AGameCrowdAgent* Agent = Item.ActiveAgents(Item.LastAgentTickedIndex);
			if( Agent == NULL )
			{
				Item.ActiveAgents.Remove(Item.LastAgentTickedIndex,1);
				Item.LastAgentTickedIndex = (Item.LastAgentTickedIndex <= 0) ? (Item.ActiveAgents.Num() - 1) : (Item.LastAgentTickedIndex - 1);
				continue;
			}

			Agent->bSimulateThisTick = Agent->ShouldPerformCrowdSimulation(DeltaTime);
			if( Agent->bSimulateThisTick )
			{
				AgentsToTick.AddItem(Agent);
				NumToTick--;
			}

			TickAttempts++;
		}

		//DEBUG
#if !FINAL_RELEASE
		for( INT AgentIdx = 0; AgentIdx < Item.ActiveAgents.Num(); AgentIdx++ )
		{
			AGameCrowdAgent* Agent = Item.ActiveAgents(AgentIdx);
			if( Agent != NULL && Agent->bDebug && Agent->bPaused )
			{
				AgentsToTick.AddUniqueItem(Agent);
			}
		}
#endif

		for( INT AgentIdx = 0; AgentIdx < AgentsToTick.Num(); AgentIdx++ )
		{
			AGameCrowdAgent* Agent = AgentsToTick(AgentIdx);
			Agent->UpdateProximityInfo();
			Agent->UpdatePendingVelocity( DeltaTime );
			Agent->CheckSeePlayer();
		}
	}
}

UBOOL GStopIfHaveSRA = TRUE;
UBOOL AGameCrowdPopulationManager::StaticGetPlayerInfo( TArray<struct FCrowdSpawnerPlayerInfo>& out_PlayerInfo )
{
	out_PlayerInfo.Empty();

#if !FINAL_RELEASE
	for( FActorIterator It; It; ++It )
	{
		AActor* SRA = *It;
		if(	 SRA && 
			!SRA->bDeleteMe &&
			SRA->IsA(AGameCrowdSpawnRelativeActor::StaticClass()) )
		{
			FCrowdSpawnerPlayerInfo Info;
			Info.ViewLocation	 = SRA->Location;
			Info.ViewRotation	 = SRA->Rotation;
			Info.PredictLocation = SRA->Location + PlayerPositionPredictionTime * SRA->Velocity;
			Info.PC				 = NULL;
			out_PlayerInfo.AddItem( Info );
			break;
		}
	}
	if( GStopIfHaveSRA && out_PlayerInfo.Num() > 0 )
	{
		return TRUE;
	}
#endif

	// look for spawn points which are either one link away from a visible GameCrowdDestination, or which will be visible in PlayerPositionPredictionTime
	for( INT Idx = 0; Idx < GEngine->GamePlayers.Num(); ++Idx )   
	{  
		ULocalPlayer* LP = GEngine->GamePlayers(Idx);
		if( LP &&  
			LP->Actor &&  
			LP->Actor->IsLocalPlayerController() )
		{  
			APlayerController* PC = LP->Actor;
			if( PC->Pawn == NULL )
			{
				continue;
			}

			FCrowdSpawnerPlayerInfo Info;
			PC->eventGetPlayerViewPoint( Info.ViewLocation, Info.ViewRotation );
			Info.PredictLocation = Info.ViewLocation + PlayerPositionPredictionTime * PC->ViewTarget->Velocity;
			Info.PC = PC;

			// clamp predicted camera position inside world geometry
			FCheckResult Hit(1.f);
			if( !GWorld->SingleLineCheck( Hit, this, Info.PredictLocation, Info.ViewLocation, TRACE_World ) )
			{
				Info.PredictLocation = (7.f * Hit.Location + 3.f * Info.ViewLocation)/10.f;
			}

			out_PlayerInfo.AddItem( Info );
		}
	}

	return (out_PlayerInfo.Num()>0);
}

UBOOL AGameCrowdPopulationManager::GetPlayerInfo()
{
	if( LastPlayerInfoUpdateTime == WorldInfo->TimeSeconds )
	{
		return (PlayerInfo.Num()>0);
	}
	LastPlayerInfoUpdateTime = WorldInfo->TimeSeconds;
	return StaticGetPlayerInfo(PlayerInfo);
}

AGameCrowdAgent* AGameCrowdPopulationManager::SpawnAgentByIdx( INT SpawnerIdx, AGameCrowdDestination* SpawnLoc )
{
	FCrowdSpawnInfoItem& Item = (SpawnerIdx < 0) ? CloudSpawnInfo : ScriptedSpawnInfo(SpawnerIdx);

	return SpawnAgent( Item, SpawnLoc );
}

AGameCrowdAgent* AGameCrowdPopulationManager::SpawnAgent( FCrowdSpawnInfoItem& Item, AGameCrowdDestination* SpawnLoc )
{
	// pick agent class
	if( Item.AgentFrequencySum == 0.0 )
	{
		// make sure initialized
		for( INT i = 0;  i < Item.AgentArchetypes.Num(); i++ )
		{
			if( Cast<AGameCrowdAgent>(Item.AgentArchetypes(i).AgentArchetype) != NULL )
			{
				Item.AgentFrequencySum = Item.AgentFrequencySum + Max<FLOAT>(0.f,Item.AgentArchetypes(i).FrequencyModifier);
			}
		}
	}

	FLOAT AgentPickValue = Item.AgentFrequencySum * appFrand();
	INT PickedInfo = -1;
	FLOAT PickSum = 0.f;
	AGameCrowdAgent* AgentTemplate = NULL;
	for( INT i = 0; i < Item.AgentArchetypes.Num(); i++ )
	{
		AgentTemplate = Cast<AGameCrowdAgent>(Item.AgentArchetypes(i).AgentArchetype);
		if( AgentTemplate != NULL )
		{
			// here we can check this here and have a max allowed
			//`log( GetFuncName() @ `showvar(AgentArchetypes[i].CurrSpawned) @ `showvar(AgentArchetypes[i].GroupMembers.Length) );
			// native struct properties don't get properly propagated so we need to hack this in.
			if( Item.AgentArchetypes(i).CurrSpawned < Item.AgentArchetypes(i).MaxAllowed || 
				Item.AgentArchetypes(i).MaxAllowed == 0 )
			{
				PickSum = PickSum + Max<FLOAT>(0.f,Item.AgentArchetypes(i).FrequencyModifier);
				if( PickSum > AgentPickValue )
				{
					PickedInfo = i;
					break;
				}
			}
		} 
	}	

	if( PickedInfo == -1 )
	{
		// failed to find valid archetype
		return NULL;
	}

	UGameCrowdGroup* NewGroup = NULL;
	if( Item.AgentArchetypes(PickedInfo).GroupMembers.Num() > 0 )
	{
		NewGroup = ConstructObject<UGameCrowdGroup>( UGameCrowdGroup::StaticClass(), GWorld->PersistentLevel, NAME_None, RF_Transactional, NULL );
	}

	AGameCrowdAgent* Agent = eventCreateNewAgent( Item, SpawnLoc, AgentTemplate, NewGroup );

	// spawn other agents in group
	for( INT i = 0; i < Item.AgentArchetypes(PickedInfo).GroupMembers.Num(); i++ )
	{
		if( Cast<AGameCrowdAgent>(Item.AgentArchetypes(PickedInfo).GroupMembers(i)) != NULL )
		{
			eventCreateNewAgent(Item, SpawnLoc, Cast<AGameCrowdAgent>(Item.AgentArchetypes(PickedInfo).GroupMembers(i)), NewGroup);
		}
	}
	return Agent;
}

UBOOL AGameCrowdPopulationManager::Warmup( FCrowdSpawnInfoItem& Item, INT WarmupNum )
{
	INT NumSpawned = 0;
	if( Item.SeqSpawner != NULL )
	{
		Item.SeqSpawner->LastSpawnedList.Empty();
	}

	while( WarmupNum > 0 )
	{
		eventPrioritizeSpawnPoints( Item, 0.05f );

		AGameCrowdDestination* PickedSpawnPoint = eventPickSpawnPoint( Item );
		if( PickedSpawnPoint != NULL )
		{
			PickedSpawnPoint->LastSpawnTime = WorldInfo->TimeSeconds;
			AGameCrowdAgent* A = SpawnAgent( Item, PickedSpawnPoint );
			if( A != NULL )
			{
				NumSpawned++;
				if( Item.SeqSpawner != NULL )
				{
					Item.SeqSpawner->LastSpawnedList.AddItem( A );
				}
			}
		}

		WarmupNum--;
	}

	return (NumSpawned>0);
}

void AGameCrowdAgent::CheckSeePlayer()
{
	// check if can see local player
	if( bWantsSeePlayerNotification && (WorldInfo->TimeSeconds - LastRenderTime < 0.1f) )
	{
		AGameCrowdPopulationManager* PopMgr = Cast<AGameCrowdPopulationManager>(WorldInfo->PopulationManager);
		if( PopMgr != NULL && PopMgr->GetPlayerInfo() )
		{
			for( INT PlayerIdx = 0; PlayerIdx < PopMgr->PlayerInfo.Num(); ++PlayerIdx )   
			{  
				FCrowdSpawnerPlayerInfo& Info = PopMgr->PlayerInfo(PlayerIdx);
				if( Info.PC == NULL || Info.PC->Pawn == NULL )
				{
					continue;
				}
	
				// If facing player, do a trace to verify line of sight
				const FLOAT DistFromPlayer	= (Info.PC->Pawn->Location - Location).SizeSquared();
				const FLOAT RotDotPlayer	= (Info.PC->Pawn->Location - Location) | Rotation.Vector();
				if( DistFromPlayer < MaxSeePlayerDistSq && RotDotPlayer > 0.f )  
				{  
					FCheckResult Hit(1.f);
					GWorld->SingleLineCheck( Hit, this, Info.PC->Pawn->Location + FVector(0.f, 0.f, Info.PC->Pawn->BaseEyeHeight), Location + FVector(0.f,0.f,EyeZOffset), TRACE_World|TRACE_StopAtAnyHit|TRACE_ComplexCollision );
					if ( Hit.Time == 1.f )
					{
						eventNotifySeePlayer(Info.PC);
						break;
					}
				}  
			}
		}
	}
}

void AGameCrowdPopulationManager::GetAlwaysRelevantDynamics( AGameCrowdAgent* Agent )
{
	if( Agent != NULL )
	{
		for( INT Idx = 0; Idx < GEngine->GamePlayers.Num(); ++Idx )   
		{  
			if( GEngine->GamePlayers(Idx) &&  
				GEngine->GamePlayers(Idx)->Actor &&  
				GEngine->GamePlayers(Idx)->Actor->IsLocalPlayerController())  
			{  
				APlayerController* PC = GEngine->GamePlayers(Idx)->Actor;  
				if( PC != NULL && PC->Pawn != NULL )
				{
					IInterface_RVO* Interface = InterfaceCast<IInterface_RVO>(PC->Pawn);
					if( Interface != NULL )
					{
						UBOOL bFound = FALSE;
						for( INT ItemIdx = 0; ItemIdx < Agent->NearbyDynamics.Num(); ItemIdx++ )
						{
							if( Agent->NearbyDynamics(ItemIdx) == PC->Pawn )
							{
								bFound = TRUE;
								break;
							}
						}

						if( !bFound )
						{
							FNearbyDynamicItem Item;
							Item.Dynamic = PC->Pawn;
							Agent->NearbyDynamics.AddItem( Item );
						}
					}
				}
			}
		}
	}
}
