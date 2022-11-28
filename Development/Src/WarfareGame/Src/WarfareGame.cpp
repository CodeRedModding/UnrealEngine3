//=============================================================================
// Copyright 2003 Epic Games - All Rights Reserved.
// Confidential.
//=============================================================================

#include "WarfareGame.h"

#define STATIC_LINKING_MOJO 1
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName WARFAREGAME_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "WarfareGameClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "WarfareGameClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVES_ONLY
#undef NAMES_ONLY

IMPLEMENT_CLASS(AWarHUD)
IMPLEMENT_CLASS(AWarPawn)
IMPLEMENT_CLASS(AWarPC)
IMPLEMENT_CLASS(AWarPRI)
IMPLEMENT_CLASS(AWarWeapon)
IMPLEMENT_CLASS(AProj_Grenade)
IMPLEMENT_CLASS(AWarScout)
IMPLEMENT_CLASS(AWarTeamInfo)

IMPLEMENT_CLASS(USeqAct_AIShootAtTarget);
IMPLEMENT_CLASS(USeqAct_GetTeammate);
IMPLEMENT_CLASS(USeqAct_GetCash);

IMPLEMENT_CLASS(UWarCheatManager);

IMPLEMENT_CLASS(UWarActorFactoryAI);

//IMPLEMENT_CLASS(UUIContainerFactory);

FLOAT AWarPawn::MaxSpeedModifier()
{
	FLOAT Result = MovementPct;

	if ( !IsHumanControlled() )
		Result *= DesiredSpeed;

	if ( bIsCrouched )
		Result *= CrouchedPct;
	else if ( bIsWalking )
		Result *= WalkingPct;
	else if ( bIsSprinting )
		Result *= SprintingPct;

	return Result;
}

void AWarPawn::execJumpDownLedge(FFrame &Stack, RESULT_DECL)
{
	P_FINISH;
	if (Physics == PHYS_Walking &&
		Controller != NULL)
	{
		// trace in front of the pawn, looking for the ledge
		FCheckResult chkResult;
		UBOOL bFoundLedge = 0;
		const INT MaxChecks = 4;
		const FLOAT StepDistance = 32.f;
		FVector chkLoc = Location;
		FLOAT lastDistance = CylinderComponent->CollisionHeight;
		for (INT idx = 0; idx < MaxChecks && !bFoundLedge; idx++)
		{
			chkLoc = Location - FVector(0.f,0.f,CylinderComponent->CollisionHeight) + (Rotation.Vector() * StepDistance * idx);
			if (!GetLevel()->SingleLineCheck(chkResult,this,chkLoc - FVector(0.f,0.f,MaxJumpDownDistance),chkLoc,TRACE_World,FVector(1.f,1.f,1.f)))
			{
				FLOAT dist = (chkLoc - chkResult.Location).Size();
				if (dist - lastDistance >= MinJumpDownDistance)
				{
					bFoundLedge = 1;
				}
				lastDistance = dist;
			}
		}
		// if we found an acceptable ledge,
		if (bFoundLedge)
		{
			// walk the pawn off of it
			bJumpingDownLedge = 1;
			bCanJump = 1;
			Controller->bPreciseDestination = 1;
			Controller->Destination = chkResult.Location + Rotation.Vector() * StepDistance * 2.f + FVector(0,0,CylinderComponent->CollisionHeight);
			*(UBOOL*)Result = 1;
		}
	}
}

FVector AWarPawn::CheckForLedges(FVector AccelDir, FVector Delta, FVector GravDir, int &bCheckedFall, int &bMustJump )
{
	// if jumping down a ledge then don't check,
	if (bJumpingDownLedge ||
		JumpPoint != NULL)
	{
		return Delta;
	}
	else
	if (Base != NULL)
	{
		FCheckResult chkResult;
		FLOAT TestDown = MinJumpDownDistance + 4.f;
		FVector chkLoc = Location;
		chkLoc.Z -= CylinderComponent->CollisionHeight;
		// first check distance to the surface we're standing on to grab the ignore offset
		if (!GetLevel()->SingleLineCheck(chkResult,this,chkLoc + GravDir * TestDown,chkLoc,TRACE_AllBlocking))
		{
			TestDown += (chkResult.Location - chkLoc).Size();
		}
		// otherwise, check that the edge of our cylinder in the accel direction is sitting on something
		chkLoc = Location + Delta + AccelDir * CylinderComponent->CollisionRadius;
		// adjust for collision height
		chkLoc.Z -= CylinderComponent->CollisionHeight;
		// check down enough to catch either step or slope
		if (GetLevel()->SingleLineCheck(chkResult,this,chkLoc + GravDir * TestDown,chkLoc,TRACE_AllBlocking|TRACE_StopAtFirstHit))
		{
			// don't allow a move in this direction
			return FVector(0.f,0.f,0.f);
		}
	}
	return Super::CheckForLedges(AccelDir,Delta,GravDir,bCheckedFall,bMustJump);
}

/**
 * Special physProjectile for Grenades
 * (in order to help with simulation rendering on HUD)
 */
void AProj_Grenade::physProjectile(FLOAT deltaTime, INT Iterations)
{
	//debugf( TEXT("AProjectile::physProjectile Velocity X:%f, Y:%f, Z:%f"), Velocity.X, Velocity.Y, Velocity.Z);

	// if freezing grenade, do not update physics
	if ( bFreeze )
		return;

	if ( Location.Z < (Region.Zone->bSoftKillZ ? Region.Zone->KillZ - Region.Zone->SoftKill : Region.Zone->KillZ) )
	{
		eventFellOutOfWorld(Region.Zone->KillZDamageType);
		return;
	}

	if ( (Region.ZoneNumber == 0) && !bIgnoreOutOfWorld )
	{
		GetLevel()->DestroyActor( this );
		return;
	}

	FLOAT	remainingTime	= deltaTime;
	INT		numBounces		= 0;
	FVector	OldLocation		= Location;
	
	FCheckResult Hit(1.f);
	bJustTeleported = 0;
	
	// Simulate gravity
	if ( bPerformGravity )
		Velocity += fGravityScale * PhysicsVolume->Gravity * deltaTime;

	while ( remainingTime > 0.f )
	{
		Iterations++;

		/*
		if ( !Acceleration.IsZero() )
		{
			debugf(TEXT("%s has acceleration!"),GetName());
			Velocity = Velocity	+ Acceleration * remainingTime;
			BoundProjectileVelocity();
		}
		*/
		FLOAT	timeTick	= remainingTime;
		FVector Adjusted	= Velocity * remainingTime;
		remainingTime		= 0.f;
		Hit.Time			= 1.f;
		
		// Move projectile
		//debugf( TEXT("AProjectile::physProjectile perform move Velocity X:%f, Y:%f, Z:%f"), Velocity.X, Velocity.Y, Velocity.Z);
		GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
		
		// If there was a collision...
		if ( Hit.Time<1.f && !bDeleteMe && !bJustTeleported )
		{
			//debugf( TEXT("native collision") );
			/*if ( ShrinkCollision(Hit.Actor) )
				remainingTime = timeTick * (1.f - Hit.Time);
			else */
			if ( Hit.Actor->bWorldGeometry )
			{
				eventHitWall(Hit.Normal, Hit.Actor);
				if ( bBounce )
				{
					if ( numBounces < 2 )
						remainingTime = timeTick * (1.f - Hit.Time);
					numBounces++;
					if ( Physics == PHYS_Falling )
						physFalling(remainingTime, Iterations);
				}
			}
		}
	}

	//debugf( TEXT("AProjectile::physProjectile finished Velocity X:%f, Y:%f, Z:%f"), Velocity.X, Velocity.Y, Velocity.Z);
	if ( !bBounce && !bJustTeleported )
		Velocity = (Location - OldLocation) / deltaTime;
}

//
// WarWeapon
//
static UBOOL	bSavedIsFiring;
static INT		SavedFireMode;

void AWarWeapon::PreNetReceive()
{
	SavedFireMode	= CurrentFireMode;
	bSavedIsFiring	= bIsFiring;
	AWeapon::PreNetReceive();
}

void AWarWeapon::PostNetReceive()
{
	if( bSavedIsFiring != bIsFiring )
	{
		if( bIsFiring )
		{
			eventStartFiringEvent( CurrentFireMode );
		}
		else
		{
			eventEndFiringEvent( CurrentFireMode );
		}
	}

	if( SavedFireMode != CurrentFireMode )
	{
		eventFireModeChanged( SavedFireMode, CurrentFireMode );
	}

	AWeapon::PostNetReceive();
}

/**
 * Overridden to get the closest teammates to the given player.
 */
void USeqAct_GetTeammate::Activated()
{
	UBOOL bAssignedMember = 0;
	TArray<UObject**> playerVars;
	GetObjectVars(playerVars,TEXT("Player"));
	TArray<UObject**> teamVars;
	GetObjectVars(teamVars,TEXT("Teammate"));
	if (playerVars.Num() > 0 &&
		teamVars.Num() > 0)
	{
		// get the player's team
		AController *player = Cast<AController>(*playerVars(0));
		if (player == NULL)
		{
			APawn *pawn = Cast<APawn>(*playerVars(0));
			if (pawn != NULL)
			{
				player = pawn->Controller;
			}
		}
		ScriptLog(FString::Printf(TEXT("Getting teammates for %s"),player!=NULL?player->GetName():TEXT("NULL")));
		if (player != NULL &&
			player->Pawn != NULL &&
			player->PlayerReplicationInfo != NULL)
		{
			AWarTeamInfo *team = Cast<AWarTeamInfo>(player->PlayerReplicationInfo->Team);
			if (team != NULL)
			{
				// build a list of team members sorted by distance
				TArray<AController*> teamMembers;
				for (INT idx = 0; idx < team->TeamMembers.Num(); idx++)
				{
					AController *member = team->TeamMembers(idx);
					if (member != NULL &&
						member != player &&
						member->Pawn != NULL)
					{
						// filter this member by required inventory, if any
						UBOOL bValidMember = 1;
						if (RequiredInventory.Num() > 0)
						{
							// grab the inventory manager
							AInventoryManager *invMgr = member->Pawn->InvManager;
							if (invMgr != NULL)
							{
								// and search for each required inventory type on the manager
								for (INT invIdx = 0; invIdx < RequiredInventory.Num() && bValidMember; invIdx++)
								{
									// only search if it's a valid item,
									if (RequiredInventory(invIdx) != NULL)
									{
										AInventory *inv = invMgr->eventFindInventoryType(RequiredInventory(invIdx));
										if (inv == NULL)
										{
											// *DENIED*
											bValidMember = 0;
										}
									}
								}
							}
						}
						// if they are still a valid member, add them to the distance sorted list
						if (bValidMember)
						{
							UBOOL bInserted = 0;
							for (INT memberIdx = 0; memberIdx < teamMembers.Num() && !bInserted; memberIdx++)
							{
								if ((member->Pawn->Location - player->Pawn->Location).Size() <
									(teamMembers(memberIdx)->Pawn->Location - player->Pawn->Location).Size())
								{
									bInserted = 1;
									teamMembers.Insert(memberIdx,1);
									teamMembers(memberIdx) = member;
								}
							}
							if (!bInserted)
							{
								teamMembers.AddItem(member);
							}
						}
					}
				}
				// for each variable set a team member
				for (INT idx = 0; idx < teamVars.Num() && teamMembers.Num() > 0; idx++)
				{
					bAssignedMember = 1;
					*(teamVars(idx)) = teamMembers(0);
					teamMembers.Remove(0,1);
				}
			}
		}
	}
	// activate the proper output based on whether or not we assigned a teammate
	OutputLinks(bAssignedMember?0:1).bHasImpulse = 1;
}

/**
 * Overridden to check for an activation of the stop input,
 * so as to notify target actors.
 */
UBOOL USeqAct_AIShootAtTarget::UpdateOp(FLOAT deltaTime)
{
	// if the stop input has been activated, then
	// call the actor handler so that they can properly
	// stop firing
	if (InputLinks(0).bHasImpulse ||
		InputLinks(1).bHasImpulse)
	{
		Activated();
	}
	return Super::UpdateOp(deltaTime);
}

AActor* UWarActorFactoryAI::CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation)
{
	APawn* newPawn = NULL;
	INT spawnSetIdx = (INT)SpawnType;
	if (spawnSetIdx >= 0 &&
		spawnSetIdx < SpawnSets.Num())
	{
		// first create the pawn
		if (SpawnSets(spawnSetIdx).PawnClass != NULL)
		{
			// check that the area around the location is clear of other characters
			UBOOL bHitPawn = false;
			FMemMark Mark(GMem);
			FCheckResult* checkResult = Level->MultiPointCheck(GMem, *Location, FVector(36,36,78), NULL, true, false, false);
			for (FCheckResult* testResult = checkResult; testResult != NULL && !bHitPawn; testResult = testResult->GetNext())
			{
				if (testResult->Actor != NULL &&
					testResult->Actor->IsA(APawn::StaticClass()))
				{
					bHitPawn = true;
				}
			}
			Mark.Pop();
			if (!bHitPawn)
			{
				NewActorClass = SpawnSets(spawnSetIdx).PawnClass;
				newPawn = (APawn*)Super::CreateActor(Level,Location,Rotation);
				if (newPawn != NULL)
				{
					// create any inventory
					TArray<UClass*> *invList = &InventoryList;
					if (InventoryList.Num() == 0)
					{
						invList = &(SpawnSets(spawnSetIdx).InventoryList);
					}
					for (INT idx = 0; idx < invList->Num(); idx++)
					{
						newPawn->eventCreateInventory( (*invList)(idx) );
					}
					// create the controller
					if (SpawnSets(spawnSetIdx).ControllerClass != NULL)
					{
						NewActorClass = SpawnSets(spawnSetIdx).ControllerClass;
						AAIController* newController = (AAIController*)Super::CreateActor(Level,Location,Rotation);
						if (newController != NULL)
						{
							AWarPRI *pri = Cast<AWarPRI>(newController->PlayerReplicationInfo);
							if (pri != NULL)
							{
								pri->SquadName = SquadName;
							}
							// handle the team assignment
							newController->eventSetTeam(SpawnSets(spawnSetIdx).TeamIndex);
							// force the controller to possess, etc
							newController->eventPossess(newPawn);
						}
					}
				}
			}
		}
	}
	return newPawn;
}
