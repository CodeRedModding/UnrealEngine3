//=============================================================================
//	AUTBot.cpp: UT AI implementation
// Copyright 2004 Epic Games - All Rights Reserved.
// Confidential.
//
//	Revision history:
//		* Created by Steven Polge
//
//=============================================================================

#include "UTGame.h"
#include "UnPath.h"

IMPLEMENT_CLASS(AUTBot);

enum EBotAIFunctions
{
	BotAI_PollWaitToSeeEnemy = 511
};


void AUTBot::UpdateEnemyInfo(APawn* AcquiredEnemy)
{
		LastSeenTime = Level->TimeSeconds;
		LastSeeingPos = GetViewTarget()->Location;
		LastSeenPos = Enemy->Location;
		bEnemyInfoValid = true;
}

/* FIXMESTEVE
void AUTBot::execFindBestSuperPickup( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(MaxDist);
	P_FINISH;

	if ( !Pawn )
	{
		*(AActor**)Result = NULL; 
		return;
	}
	AActor * bestPath = NULL;
	PendingMover = NULL;
	bPreparingMove = false;

	// Set super pickups as endpoints
	APickupFactory *GOAL = NULL;
	for ( ANavigationPoint *N=Level->NavigationPointList; N!=NULL; N=N->nextNavigationPoint )
	{
		APickupFactory *IS = Cast<APickupFactory>(N);
		if ( IS && IS->bDelayedSpawn
			&& IS->IsProbing(NAME_Touch) 
			&& !IS->BlockedByVehicle()
			&& (eventSuperDesireability(IS) > 0.f) )
		{
			IS->bTransientEndPoint = true;
			GOAL = IS;
		}
	}
	bestPath = FindPath(FVector(0.f,0.f,0.f), GOAL, true);
	if ( RouteDist > MaxDist )
		bestPath = NULL;

	*(AActor**)Result = bestPath; 
}
*/

void AUTBot::execWaitToSeeEnemy( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(dest);
	P_GET_ACTOR_OPTX(viewfocus, NULL);
	P_GET_FLOAT_OPTX(speed, 1.f);
	P_FINISH;

	if ( !Pawn || !Enemy )
		return;
	Focus = Enemy;
	GetStateFrame()->LatentAction = BotAI_PollWaitToSeeEnemy;
}

void AUTBot::execPollWaitToSeeEnemy( FFrame& Stack, RESULT_DECL )
{
	if( !Pawn || !Enemy )
	{
		GetStateFrame()->LatentAction = 0; 
		return;
	}
	if ( Level->TimeSeconds - LastSeenTime > 0.1f )
		return;

	//check if facing enemy 
	if ( Pawn->ReachedDesiredRotation() )
		GetStateFrame()->LatentAction = 0; 
}
IMPLEMENT_FUNCTION( AUTBot, BotAI_PollWaitToSeeEnemy, execPollWaitToSeeEnemy);

/* CanMakePathTo()
// assumes valid CurrentPath, tries to see if CurrentPath can be combined with a path to N
*/
void AUTBot::execCanMakePathTo( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR(A);
	P_FINISH;

	ANavigationPoint *N = Cast<ANavigationPoint>(A);
	INT Success = 0;

	if ( N && Pawn->ValidAnchor() && CurrentPath 
		&& ((CurrentPath->reachFlags & (R_FORCED + R_WALK)) == CurrentPath->reachFlags) )
	{
		UReachSpec *NextPath = 	CurrentPath->End->GetReachSpecTo(N);
		if ( NextPath &&  ((NextPath->reachFlags & (R_FORCED + R_WALK)) == NextPath->reachFlags) 
			&& NextPath->supports(Pawn->CylinderComponent->CollisionRadius,Pawn->CylinderComponent->CollisionHeight,Pawn->calcMoveFlags(),Pawn->MaxFallSpeed) )
		{
			FCheckResult Hit(1.f);
			GetLevel()->SingleLineCheck( Hit, this, N->Location, Pawn->Location + FVector(0,0,Pawn->EyeHeight), TRACE_World|TRACE_StopAtFirstHit );
			if ( !Hit.Actor )
			{
				// check in relatively straight line ( within path radii)
				FLOAT MaxDist = ::Min<FLOAT>(CurrentPath->CollisionRadius,NextPath->CollisionRadius);
				FVector Dir = (N->Location - Pawn->Location).SafeNormal();
				FVector LineDist = CurrentPath->End->Location - (Pawn->Location + (Dir | (CurrentPath->End->Location - Pawn->Location)) * Dir);
				//debugf(TEXT("Path dist is %f max %f"),LineDist.Size(),MaxDist);
				Success = ( LineDist.SizeSquared() < MaxDist * MaxDist );
			}
		}
	}

	*(DWORD*)Result = Success;
}

/* FIXMESTEVE
FLOAT FindBestInventory( ANavigationPoint* CurrentNode, APawn* seeker, FLOAT bestWeight )
{
	FLOAT CacheWeight = 0.f;
	if ( CurrentNode->InventoryCache && (CurrentNode->visitedWeight < (8.f - CurrentNode->InventoryCache->TimerCounter) * seeker->GroundSpeed) )
	{
		FLOAT BaseWeight = 0.f;
		FLOAT CacheDist = ::Max(1.f,CurrentNode->InventoryDist + CurrentNode->visitedWeight);
		if ( CurrentNode->InventoryCache->bDeleteMe )
			CurrentNode->InventoryCache = NULL;
		else if ( CurrentNode->InventoryCache->MaxDesireability/CacheDist > bestWeight )
			BaseWeight = seeker->Controller->eventRatePickup(CurrentNode->InventoryCache);
		CacheWeight = BaseWeight/CacheDist;
		if ( (CacheWeight > bestWeight) && !CurrentNode->InventoryCache->BlockedByVehicle() )
		{
			if ( BaseWeight >= 1.f )
				return 2.f;
			bestWeight = CacheWeight;
		}
	}
	
	if ( !CurrentNode->GetAPickupFactory() || !seeker->Controller )
		return CacheWeight;

	APickupFactory* item = ((APickupFactory *)CurrentNode);
	FLOAT AdjustedWeight = ::Max(1,CurrentNode->visitedWeight);
	if ( item && !item->bDeleteMe && (item->IsProbing(NAME_Touch) || (item->bPredictRespawns && (item->LatentFloat < seeker->Controller->RespawnPredictionTime))) 
			&& (item->MaxDesireability/AdjustedWeight > bestWeight) )
	{
		FLOAT BaseWeight = seeker->Controller->eventRatePickup(item);
		if ( !item->IsProbing(NAME_Touch) )
			AdjustedWeight += seeker->GroundSpeed * item->LatentFloat;
		if ( (CacheWeight * AdjustedWeight > BaseWeight) || (bestWeight * AdjustedWeight > BaseWeight) || item->BlockedByVehicle() )
			return CacheWeight;

		if ( (BaseWeight >= 1.f) && (BaseWeight > AdjustedWeight * bestWeight) )
			return 2.f;

		return BaseWeight/AdjustedWeight;
	}
	return CacheWeight;
}

void AUTBot::execFindBestInventoryPath( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT_REF(Weight);
	P_FINISH;

	if ( !Pawn )
	{
		*(AActor**)Result = NULL;
		return;
	}
	AActor * bestPath = NULL;
	bPreparingMove = false;

	// first, look for nearby dropped inventory
	if ( Pawn->ValidAnchor() )
	{
		if ( Pawn->Anchor->InventoryCache )
		{
			if ( Pawn->Anchor->InventoryCache->bDeleteMe )
				Pawn->Anchor->InventoryCache = NULL;
			else if ( Pawn->actorReachable(Pawn->Anchor->InventoryCache) )
			{
				*(AActor**)Result = Pawn->Anchor->InventoryCache;
				return;
			}
			else
				Pawn->Anchor->InventoryCache = NULL;
		}
	}

	*Weight = Pawn->findPathToward(NULL,FVector(0,0,0),&FindBestInventory, *Weight,false);
	if ( *Weight > 0.f )
		bestPath = SetPath();

	*(AActor**)Result = bestPath;
}


UBOOL APickupFactory::BlockedByVehicle()
{
	if ( !Level->Game || !Level->Game->bAllowVehicles )
		return false;

	for ( INT i=0; i<Touching.Num(); i++ )
		if ( Touching(i) && Touching(i)->GetAPawn() && Cast<AVehicle>(Touching(i)) )
			return true;

	return false;
}


void AUTBot::AirSteering(float DeltaTime)
{
	if ( !Pawn )
		return;

	if ( !bPlannedJump || (Skill < 2.f) )
	{
		Pawn->ImpactVelocity = FVector(0.f,0.f,0.f);
		return;
	}

	// no steering here if already doing low grav steering
	if ( (Pawn->Velocity.Z < 0.f) && (Pawn->PhysicsVolume->Gravity.Z > 0.9f * ((APhysicsVolume *)(Pawn->PhysicsVolume->GetClass()->GetDefaultObject()))->Gravity.Z) )
		return;

	Pawn->Acceleration = -1.f * Pawn->ImpactVelocity * Pawn->AccelRate;
	Pawn->Acceleration.Z = 0.f;
}
*/

/* UTBot Tick
*/
UBOOL AUTBot::Tick( FLOAT DeltaSeconds, ELevelTick TickType )
{
	UBOOL Ticked = Super::Tick(DeltaSeconds, TickType);

	if ( Ticked )
	{
		if( TickType==LEVELTICK_All )
		{
			if ( WarningProjectile && !WarningProjectile->bDeleteMe && (Level->TimeSeconds > WarningDelay) )
			{
				eventDelayedWarning();
				WarningProjectile = NULL;
			}
			if ( MonitoredPawn )
			{
				if ( !Pawn || MonitoredPawn->bDeleteMe || !MonitoredPawn->Controller )
					eventMonitoredPawnAlert();
				else if ( !Pawn->SharingVehicleWith(MonitoredPawn) )
				{
					// quit if further than max dist, moving away fast, or has moved far enough
					if ( ((MonitoredPawn->Location - Pawn->Location).SizeSquared() > MonitorMaxDistSq)
						|| ((MonitoredPawn->Location - MonitorStartLoc).SizeSquared() > 0.25f * MonitorMaxDistSq) )
						eventMonitoredPawnAlert();
					else if ( (MonitoredPawn->Velocity.SizeSquared() > 0.6f * MonitoredPawn->GroundSpeed)
						&& ((MonitoredPawn->Velocity | (MonitorStartLoc - Pawn->Location)) > 0.f)
						&& ((MonitoredPawn->Location - Pawn->Location).SizeSquared() > 0.25f * MonitorMaxDistSq) )
						eventMonitoredPawnAlert();
				}
			}
		}		
	}

	return Ticked;
}