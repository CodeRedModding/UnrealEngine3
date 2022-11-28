/*=============================================================================
	UnController.cpp: AI implementation

  This contains both C++ methods (movement and reachability), as well as some
  AI related natives

	Copyright 2000-2002 Epic MegaGames, Inc. This software is a trade secret.

	Revision history:
		* Created by Steven Polge 4/00
=============================================================================*/
#include "EnginePrivate.h"
#include "UnNet.h"
#include "FConfigCacheIni.h"
#include "UnPath.h"
#include "EngineAIClasses.h"

IMPLEMENT_CLASS(UCheatManager);

enum EAIFunctions
{
	AI_PollMoveTo = 501,
	AI_PollMoveToward = 503,
	AI_PollStrafeTo = 505,
	AI_PollStrafeFacing = 507,
	AI_PollFinishRotation = 509,
	AI_PollWaitForLanding = 528,
};

void APlayerController::execCheckSpeedHack( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(NewDeltaTime);
	P_FINISH;

	*(DWORD*)Result = 1;
	if ( (ServerTimeStamp > 0.f) && (Level->MaxTimeMargin > 0.f) )
    {
 		if ( Level->TimeSeconds - ServerTimeStamp > 0.3f )
		{
			TimeMargin = 0.f;
			MaxTimeMargin = Level->MaxTimeMargin;
		}
		else if ( (TimeMargin > MaxTimeMargin) && (MaxTimeMargin < 0.2f) )
        {
			if ( MaxTimeMargin == 0.f )
				MaxTimeMargin = Level->MaxTimeMargin;
			else
			{
				// player is too far ahead - make them stand still for a while
				if ( Pawn )
					Pawn->Velocity = FVector(0.f,0.f,0.f);
				TimeMargin -= 0.1f * (Level->TimeSeconds - ServerTimeStamp);
				if ( TimeMargin < MaxTimeMargin )
					MaxTimeMargin = Level->MaxTimeMargin;
				else
					MaxTimeMargin = 0.1f;
				*(DWORD*)Result = 0;
			}
        }
		else
		{
			Level->TimeMarginSlack = 1.f;
			if ( TimeMargin < Level->MinTimeMargin )
				TimeMargin = 0.7f * Level->MinTimeMargin;
			else if ( TimeMargin < -0.3f )
				TimeMargin = TimeMargin + NewDeltaTime - (Level->TimeSeconds - ServerTimeStamp) - 0.002f;
			else
			{
				TimeMargin = TimeMargin + NewDeltaTime - Level->TimeMarginSlack * (Level->TimeSeconds - ServerTimeStamp);
				if ( TimeMargin > 0.f )
					TimeMargin -= 0.002f;
			}
            

			// if still same tick on server, don't trip detection
			if ( Level->TimeSeconds != ServerTimeStamp )
			{
				if ( TimeMargin > MaxTimeMargin )
				{
					MaxTimeMargin = 0.1f;
					// *(DWORD*)Result = 0; commented out to give him one tick of grace, in case it gets reset
				}
				else 
					MaxTimeMargin = Level->MaxTimeMargin;
			}
		}
    }
}

void AController::Spawned()
{
	Super::Spawned();
	PlayerNum = XLevel->PlayerNum++;
}

UBOOL AController::LocalPlayerController()
{
	return false;
}

UBOOL APlayerController::LocalPlayerController()
{
	return ( Player && Player->IsA(ULocalPlayer::StaticClass()) );

}

UBOOL AController::WantsLedgeCheck()
{
	if ( !Pawn )
		return false;
	if ( Pawn->bCanJump )
	{
		// check if moving toward a navigation point, and not messed with
		if ( MoveTarget && (GetStateFrame()->LatentAction == AI_PollMoveToward) )
		{
			// check if still on path
			if ( CurrentPath && (CurrentPath->End == MoveTarget) )
			{
				FVector LineDir = Pawn->Location - (CurrentPath->Start->Location + (CurrentPathDir | (Pawn->Location - CurrentPath->Start->Location)) * CurrentPathDir);
				if ( LineDir.SizeSquared() < 0.5f * Pawn->CylinderComponent->CollisionRadius )
				{
					//debugf(TEXT("%s skip ledge check because on path"), Pawn->GetName());
					return false;
				}
			}
			// check if could reach by jumping
			if ( MoveTarget->Physics != PHYS_Falling )
			{
				FVector NeededJump = Pawn->SuggestFallVelocity(MoveTarget->Location, Pawn->Location, Pawn->GroundSpeed, 0.f, 2 * Pawn->JumpZ, Pawn->GroundSpeed);
				if ( NeededJump.Z < Pawn->JumpZ )
				{
					//debugf(TEXT("%s skip ledge check because jump reachable"), Pawn->GetName());
					return false;
				}
			}
		}
	}
	//debugf(TEXT("%s do ledge check"), Pawn->GetName());
	return ( !Pawn->bCanFly );
}

UBOOL APlayerController::WantsLedgeCheck()
{
	return ( Pawn && (Pawn->bIsCrouched || Pawn->bIsWalking) );
}

UBOOL APlayerController::StopAtLedge()
{
	return false;
}

UBOOL AController::StopAtLedge()
{
	if ( !Pawn->bCanJump || Pawn->bStopAtLedges )
	{
		MoveTimer = -1.f;
		return true;
	}
	return false;
}
//-------------------------------------------------------------------------------------------------
/*
Node Evaluation functions, used with APawn::BreadthPathTo()
*/

// declare type for node evaluation functions
typedef FLOAT ( *NodeEvaluator ) (ANavigationPoint*, APawn*, FLOAT);


FLOAT FindRandomPath( ANavigationPoint* CurrentNode, APawn* seeker, FLOAT bestWeight )
{
	if ( CurrentNode->bEndPoint )
		return (1000.f + appFrand());
	return appFrand();
}
//----------------------------------------------------------------------------------
void APlayerController::execSetNetSpeed( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(NewSpeed);
	P_FINISH;

	UNetDriver* Driver = GetLevel()->NetDriver;
	if( Player && (NewSpeed>1800) && Driver && Driver->ServerConnection )
		Player->CurrentNetSpeed = Driver->ServerConnection->CurrentNetSpeed = Clamp( NewSpeed, 1800, Driver->MaxClientRate );
}

void APlayerController::execUpdateURL( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(NewOption);
	P_GET_STR(NewValue);
	P_GET_UBOOL(bSaveDefault);
	P_FINISH;

	UGameEngine* GameEngine = CastChecked<UGameEngine>( GetLevel()->Engine );
	GameEngine->LastURL.AddOption( *(NewOption + TEXT("=") + NewValue) );
	if( bSaveDefault )
		GameEngine->LastURL.SaveURLConfig( TEXT("DefaultPlayer"), *NewOption, TEXT("User") );
}

void APlayerController::execGetDefaultURL( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Option);
	P_FINISH;

	FURL URL;
	URL.LoadURLConfig( TEXT("DefaultPlayer"), TEXT("User") );

	*(FString*)Result = FString( URL.GetOption(*(Option + FString(TEXT("="))), TEXT("")) );
}

void APlayerController::execGetEntryLevel( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	check(XLevel);
	check(XLevel->Engine);
	check((UGameEngine*)(XLevel->Engine));
	check(((UGameEngine*)(XLevel->Engine))->GEntry);

	*(ALevelInfo**)Result = ((UGameEngine*)(XLevel->Engine))->GEntry->GetLevelInfo();

}

void APlayerController::execSetViewTarget( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR(NewViewTarget);
	P_FINISH;

	SetViewTarget(NewViewTarget);
}

void ADroppedPickup::execAddToNavigation( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	if ( !Inventory )
		return;

	if ( PickupCache )
	{
		if ( PickupCache->InventoryCache == this )
			PickupCache->InventoryCache = NULL;
		PickupCache = NULL;
	}

	// find searcher
	APawn *Searcher = NULL;
	for ( AController *C=Level->ControllerList; C!=NULL; C=C->NextController )
		if ( C->bIsPlayer && C->Pawn )
		{
			Searcher = C->Pawn;
			break;
		}
	if ( !Searcher )
		return;

	// find nearest path
	FSortedPathList EndPoints;
	for ( ANavigationPoint *N=Level->NavigationPointList; N!=NULL; N=N->nextNavigationPoint )
	{
		FLOAT dist = (Location - N->Location).SizeSquared();
		if ( (dist < MAXPATHDISTSQ) && (Location.Z - N->Location.Z < Searcher->MaxStepHeight + Searcher->MaxJumpHeight)
			&& (!N->InventoryCache || N->InventoryCache->bDeleteMe || !N->InventoryCache->Inventory || (N->InventoryCache->Inventory->MaxDesireability <= Inventory->MaxDesireability)) )
		{
			EndPoints.addPath(N, appRound(dist));
		}
	}

	if ( EndPoints.numPoints > 0 )
		PickupCache = EndPoints.findEndAnchor(Searcher,this,Location,false,false);

	if ( PickupCache )
	{
		PickupCache->InventoryCache = this;
		PickupCache->InventoryDist = (Location - PickupCache->Location).Size();
	}
}

void ADroppedPickup::execRemoveFromNavigation( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	if ( !PickupCache )
		return;
	if ( PickupCache->InventoryCache == this )
		PickupCache->InventoryCache = NULL;
}


void APlayerController::execConsoleCommand( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Command);
	P_GET_UBOOL_OPTX(bWriteToLog,0);
	P_FINISH;

	if ( Command.Len() > 1000 )
		Command = Command.Left(1000);

	if( bWriteToLog )
	{
		if(!Player || !Player->Exec( *Command, *GLog ) )
			GetLevel()->Engine->Exec( *Command, *GLog );
		*(FString*)Result = FString(TEXT(""));
	}
	else
	{
		FStringOutputDevice StrOut;
		if(!Player || !Player->Exec( *Command,StrOut ) )
			GetLevel()->Engine->Exec( *Command, StrOut );
		*(FString*)Result = *StrOut;
	}

}

/* CanSee()
returns true if LineOfSightto object and it is within creature's
peripheral vision
*/

void AController::execCanSee( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR(Other);
	P_FINISH;

	*(DWORD*)Result = SeePawn((APawn *)Other, false);
}

APawn* APawn::GetVehicleBase()
{
	if ( !Base || !Base->GetAPawn() )
		return NULL;

	return Cast<AVehicle>(Base);
}

/* PickTarget()
Find the best pawn target for this controller to aim at.  Used for autoaiming.
*/
void AController::execPickTarget( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT_REF(bestAim);
	P_GET_FLOAT_REF(bestDist);
	P_GET_VECTOR(FireDir);
	P_GET_VECTOR(projStart);
	P_GET_FLOAT(MaxRange);
	P_FINISH;
	APawn *pick = NULL;
	FLOAT VerticalAim = *bestAim * 3.f - 2.f;
	FCheckResult Hit(1.f);
    MaxRange *= MaxRange;

	for ( AController *next=GetLevel()->GetLevelInfo()->ControllerList; next!=NULL; next=next->NextController )
	{
		// look for best controlled pawn target which is not on same team
		if ( (next != this) && next->Pawn && (next->Pawn->Health > 0)
			&& (!PlayerReplicationInfo || !next->PlayerReplicationInfo
				|| !Level->Game->bTeamGame
				|| (PlayerReplicationInfo->Team != next->PlayerReplicationInfo->Team)) )
		{
			APawn* NewTarget = next->Pawn;
			if ( !NewTarget->bProjTarget )
			{
				NewTarget = NewTarget->GetVehicleBase();
				if ( !NewTarget || NewTarget->Controller )
					continue;
			}
			FVector AimDir = NewTarget->Location - projStart;
			FLOAT newAim = FireDir | AimDir;
			FVector FireDir2D = FireDir;
			FireDir2D.Z = 0;
			FireDir2D.Normalize();
			FLOAT newAim2D = FireDir2D | AimDir;
			if ( newAim > 0 )
			{
				FLOAT FireDist = AimDir.SizeSquared();
				// only find targets which are < MaxRange units away
				if ( FireDist < MaxRange )
				{
					FireDist = appSqrt(FireDist);
					newAim = newAim/FireDist;
					if ( newAim > *bestAim )
					{
						// target is more in line than current best - see if target is visible
						GetLevel()->SingleLineCheck( Hit, this, NewTarget->Location, Pawn->Location + FVector(0,0,Pawn->EyeHeight), TRACE_World|TRACE_StopAtFirstHit );
						if( Hit.Actor )
							GetLevel()->SingleLineCheck( Hit, this, NewTarget->Location + FVector(0,0,NewTarget->EyeHeight), Pawn->Location + FVector(0,0,Pawn->EyeHeight), TRACE_World|TRACE_StopAtFirstHit );
						if ( !Hit.Actor )
						{
							pick = NewTarget;
							*bestAim = newAim;
							*bestDist = FireDist;
						}
					}
					else if ( !pick )
					{
						// no target yet, so be more liberal about up/down error (more vertical autoaim help)
						newAim2D = newAim2D/FireDist;
						if ( (newAim2D > *bestAim) && (newAim > VerticalAim) )
						{
							GetLevel()->SingleLineCheck( Hit, this, NewTarget->Location, Pawn->Location + FVector(0,0,Pawn->EyeHeight), TRACE_World|TRACE_StopAtFirstHit );
							if( Hit.Actor )
								GetLevel()->SingleLineCheck( Hit, this, NewTarget->Location + FVector(0,0,NewTarget->EyeHeight), Pawn->Location + FVector(0,0,Pawn->EyeHeight), TRACE_World|TRACE_StopAtFirstHit );
							if ( !Hit.Actor )
							{
								pick = NewTarget;
								*bestDist = FireDist;
							}
						}
					}
				}
			}
		}
	}

	*(APawn**)Result = pick;
}

/* PickAnyTarget()
Find the best non-pawn target for this controller to aim at.  Used for autoaiming.
*/
void AController::execPickAnyTarget( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT_REF(bestAim);
	P_GET_FLOAT_REF(bestDist);
	P_GET_VECTOR(FireDir);
	P_GET_VECTOR(projStart);
	P_FINISH;
	AActor *pick = NULL;

	for( INT iActor=0; iActor<GetLevel()->Actors.Num(); iActor++ )
		if( GetLevel()->Actors(iActor) )
		{
			AActor* next = GetLevel()->Actors(iActor);
			if ( next->bProjTarget && !next->GetAPawn() )
			{
				FLOAT newAim = FireDir | (next->Location - projStart);
				if ( newAim > 0 )
				{
					FLOAT FireDist = (next->Location - projStart).SizeSquared();
					// only allow targets <2000 units away
					if ( FireDist < 4000000.f )
					{
						FireDist = appSqrt(FireDist);
						newAim = newAim/FireDist;
						// check if target is more in line and visible
						if ( (newAim > *bestAim) && LineOfSightTo(next) )
						{
							pick = next;
							*bestAim = newAim;
							*bestDist = FireDist;
						}
					}
				}
			}
		}

	*(AActor**)Result = pick;
}

/* execWaitForLanding()
wait until physics is not PHYS_Falling
*/
void AController::execWaitForLanding( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT_OPTX(waitDuration,4.f);
	P_FINISH;

	LatentFloat = waitDuration;
	if ( Pawn && (Pawn->Physics == PHYS_Falling) )
		GetStateFrame()->LatentAction = AI_PollWaitForLanding;
}

void AController::execPollWaitForLanding( FFrame& Stack, RESULT_DECL )
{
	if( Pawn && (Pawn->Physics != PHYS_Falling) )
	{
		GetStateFrame()->LatentAction = 0;
	}
	else
	{
		FLOAT DeltaSeconds = *(FLOAT*)Result;
		LatentFloat -= DeltaSeconds;
		if (LatentFloat <= 0.f)
		{
			eventLongFall();
		}
	}
}
IMPLEMENT_FUNCTION( AController, AI_PollWaitForLanding, execPollWaitForLanding);

void AController::execPickWallAdjust( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(HitNormal);
	P_FINISH;
	if ( !Pawn )
		return;
	*(DWORD*)Result = Pawn->PickWallAdjust(HitNormal, NULL);
}

/* FindStairRotation()
returns an integer to use as a pitch to orient player view along current ground (flat, up, or down)
*/
void APlayerController::execFindStairRotation( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(deltaTime);
	P_FINISH;

	// only recommend pitch if controller has a pawn, and frame rate isn't ridiculously low

	if ( !Pawn || (deltaTime > 0.33) )
	{
		*(DWORD*)Result = Rotation.Pitch;
		return;
	}

	if (Rotation.Pitch > 32768)
		Rotation.Pitch = (Rotation.Pitch & 65535) - 65536;
	
	FCheckResult Hit(1.f);
	FRotator LookRot = Rotation;
	LookRot.Pitch = 0;
	FVector Dir = LookRot.Vector();
	FVector EyeSpot = Pawn->Location + FVector(0,0,Pawn->BaseEyeHeight);
	FLOAT height = Pawn->CylinderComponent->CollisionHeight + Pawn->BaseEyeHeight;
	FVector CollisionSlice(Pawn->CylinderComponent->CollisionRadius,Pawn->CylinderComponent->CollisionRadius,1.f);

	GetLevel()->SingleLineCheck(Hit, this, EyeSpot + 2 * height * Dir, EyeSpot, TRACE_World, CollisionSlice);
	FLOAT Dist = 2 * height * Hit.Time;
	INT stairRot = 0;
	if (Dist > 0.8 * height)
	{
		FVector Spot = EyeSpot + 0.5 * Dist * Dir;
		FLOAT Down = 3 * height;
		GetLevel()->SingleLineCheck(Hit, this, Spot - FVector(0,0,Down), Spot, TRACE_World, CollisionSlice);
		if (Hit.Time < 1.f)
		{
			FLOAT firstDown = Down * Hit.Time;
			if (firstDown < 0.7f * height - 6.f) // then up or level
			{
				Spot = EyeSpot + Dist * Dir;
				GetLevel()->SingleLineCheck(Hit, this, Spot - FVector(0,0,Down), Spot, TRACE_World, CollisionSlice);
				stairRot = ::Max(0, Rotation.Pitch);
				if ( Down * Hit.Time < firstDown - 10 )
					stairRot = 3600;
			}
			else if  (firstDown > 0.7f * height + 6.f) // then down or level
			{
				GetLevel()->SingleLineCheck(Hit, this, Pawn->Location + 0.9*Dist*Dir, Pawn->Location, TRACE_World|TRACE_StopAtFirstHit);
				if (Hit.Time == 1.f)
				{
					Spot = EyeSpot + Dist * Dir;
					GetLevel()->SingleLineCheck(Hit, this, Spot - FVector(0,0,Down), Spot, TRACE_World, CollisionSlice);
					if (Down * Hit.Time > firstDown + 10)
						stairRot = -4000;
				}
			}
		}
	}
	INT Diff = Abs(Rotation.Pitch - stairRot);
	if( (Diff > 0) && (Level->TimeSeconds - GroundPitchTime > 0.25) )
	{
		FLOAT RotRate = 4;
		if( Diff < 1000 )
			RotRate = 4000/Diff;

		RotRate = ::Min(1.f, RotRate * deltaTime);
		stairRot = appRound(FLOAT(Rotation.Pitch) * (1 - RotRate) + FLOAT(stairRot) * RotRate);
	}
	else
	{
		if ( (Diff < 10) && (Abs(stairRot) < 10) )
			GroundPitchTime = Level->TimeSeconds;
		stairRot = Rotation.Pitch;
	}
	*(DWORD*)Result = stairRot;
}

void AActor::execSuggestFallVelocity( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Destination);
	P_GET_VECTOR(Start);
	P_GET_FLOAT(MaxZ);
	P_GET_FLOAT(MaxXYSpeed);
	P_FINISH;

	*(FVector*)Result = SuggestFallVelocity(Destination, Start, MaxXYSpeed, 0.f, MaxZ, MaxXYSpeed);
}

/* FIXMESTEVE
void AController::execEAdjustJump( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(BaseZ);
	P_GET_FLOAT(XYSpeed);
	P_FINISH;

	if ( Pawn )
		*(FVector*)Result = Pawn->SuggestJumpVelocity(Destination, XYSpeed,BaseZ);
	else
		*(FVector*)Result = FVector(0.f,0.f,0.f);
}
*/
void AController::execActorReachable( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR(actor);
	P_FINISH;

	if ( !actor || !Pawn )
	{
		debugf(NAME_DevPath,TEXT("Warning: No pawn or goal for ActorReachable by %s in %s"),GetName(), *GetStateFrame()->Describe() );
		*(DWORD*)Result = 0;
		return;
	}

	// check if cached failed reach
	if ( (LastFailedReach == actor) && (FailedReachTime == Level->TimeSeconds)
		&& (FailedReachLocation == Pawn->Location) )
	{
		*(DWORD*)Result = 0;
	}
	else
	{
		INT Reach = Pawn->actorReachable(actor);
		if ( !Reach )
		{
			LastFailedReach = actor;
			FailedReachTime = Level->TimeSeconds;
			FailedReachLocation = Pawn->Location;
		}
		*(DWORD*)Result = Reach;
	}
}

void AController::execPointReachable( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(point);
	P_FINISH;

	if ( !Pawn )
	{
		debugf(NAME_DevPath,TEXT("Warning: No pawn for pointReachable by %s in %s"),GetName(), *GetStateFrame()->Describe() );
		*(DWORD*)Result = 0;
		return;
	}

	*(DWORD*)Result = Pawn->pointReachable(point);
}

/* FindPathTo() and FindPathToward()
returns the best pathnode toward a point or actor - even if it is directly reachable
If there is no path, returns None
By default clears paths.  If script wants to preset some path weighting, etc., then
it can explicitly clear paths using execClearPaths before presetting the values and
calling FindPathTo with clearpath = 0
*/
AActor* AController::FindPath(FVector point, AActor* goal, UBOOL bWeightDetours)
{
	if ( !Pawn )
	{
		debugf(NAME_DevPath,TEXT("Warning: No pawn for FindPath by %s in %s"),GetName(), *GetStateFrame()->Describe() );
		return NULL;
	}
	LastRouteFind = Level->TimeSeconds;
	AActor * bestPath = NULL;
	bPreparingMove = false;
	if ( Pawn->findPathToward(goal,point,NULL,0.f, bWeightDetours) > 0.f )
		bestPath = SetPath();
	return bestPath;
}

void AController::execFindPathTo( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(point);
	P_FINISH;

	*(AActor**)Result = FindPath(point, NULL, false);
}

void AController::execFindPathToward( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR(goal);
	P_GET_UBOOL_OPTX(bWeightDetours, false);
	P_FINISH;

	if ( !goal )
	{
		debugf(NAME_DevPath,TEXT("Warning: No goal for FindPathToward by %s in %s"),GetName(), *GetStateFrame()->Describe() );
		*(AActor**)Result = NULL;
		return;
	}
	*(AActor**)Result = FindPath(FVector(0,0,0), goal, bWeightDetours);
}

void AController::execFindPathToIntercept( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR(goal);
	P_GET_ACTOR(OtherRouteGoal);
	P_GET_UBOOL_OPTX(bWeightDetours, false);
	P_FINISH;

    APawn *goalPawn = goal ? goal->GetAPawn() : NULL;
	if ( !goalPawn || !Pawn )
	{
		debugf(NAME_DevPath,TEXT("Warning: No goal for FindPathToIntercept by %s in %s"),GetName(), *GetStateFrame()->Describe() );
		*(AActor**)Result = NULL;
		return;
	}
	// debugf(TEXT("%s Find path to intercept %s going to %s"),Pawn->GetName(),goal->GetName(),OtherRouteGoal->GetName());
	if ( !Pawn->ValidAnchor() || !goalPawn->Controller || !OtherRouteGoal )
	{
		AActor *ResultPath = FindPath(FVector(0,0,0), goalPawn, bWeightDetours);
		*(AActor**)Result = ResultPath;
		return;
	}
	UBOOL bFindDirectPath = true;
	UBOOL bHumanPathed = false;
	if ( goalPawn->IsHumanControlled() )
	{
		APlayerController *GoalPC = Cast<APlayerController>(goalPawn->Controller);
		if ( GoalPC && (goalPawn->Location != GoalPC->FailedPathStart) )
		{	
			bHumanPathed = (goalPawn->Controller->FindPath(FVector(0.f,0.f,0.f), OtherRouteGoal, false)!= NULL);
			if ( !bHumanPathed )
				GoalPC->FailedPathStart = goalPawn->Location;
		}
	}

	if ( ((goalPawn->Controller->GetStateFrame()->LatentAction == AI_PollMoveToward) || (Level->TimeSeconds - goalPawn->Controller->LastRouteFind < 0.75f))
		|| bHumanPathed )
	{
		// if already on path, movetoward goalPawn
		for (INT i=0; i<goalPawn->Controller->RouteCache.Num(); i++ )
		{
			if ( !goalPawn->Controller->RouteCache(i) )
				break;
			else
			{	
				bFindDirectPath = false;
				if ( goalPawn->Controller->RouteCache(i) == Pawn->Anchor )
				{
//						debugf(TEXT("Already on path"));
					bFindDirectPath = true;
					break;
				}
			}
		}
	}
	AActor *ResultActor = NULL;

	if ( bFindDirectPath )
	{
		ResultActor = FindPath(FVector(0.f,0.f,0.f), goalPawn, bWeightDetours);
	}
	else
	{
		ANavigationPoint* Nav = Cast<ANavigationPoint>(goalPawn->Controller->MoveTarget);
		if ( Nav )
			Nav->bTransientEndPoint = true;
		for (INT i=0; i<16; i++ )
		{
			Nav = Cast<ANavigationPoint>(goalPawn->Controller->RouteCache(i));
			if ( Nav )
			{
				Nav->bTransientEndPoint = true;
//					debugf(TEXT("Mark %s"),Nav->GetName());
			}
			else if ( !goalPawn->Controller->RouteCache(i) )
				break;
		}
		ResultActor = FindPath(FVector(0.f,0.f,0.f), goalPawn, bWeightDetours);
	}
	*(AActor**)Result = ResultActor;
}

void AController::execFindPathTowardNearest( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UClass,GoalClass);
	P_GET_UBOOL_OPTX(bWeightDetours, false);
	P_FINISH;

	if ( !GoalClass || !Pawn )
	{
		debugf(NAME_DevPath,TEXT("Warning: No goal for FindPathTowardNearest by %s in %s"),GetName(), *GetStateFrame()->Describe() );
		*(AActor**)Result = NULL;
		return;
	}
	ANavigationPoint* Found = NULL;

	// mark appropriate Navigation points
	for ( ANavigationPoint* Nav=Level->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		if ( Nav->GetClass() == GoalClass )
		{
			Nav->bTransientEndPoint = true;
			Found = Nav;
		}
	if ( Found )
		*(AActor**)Result = FindPath(FVector(0,0,0), Found, bWeightDetours);
	else
		*(AActor**)Result = NULL;
}

/* FindRandomDest()
returns a random pathnode which is reachable from the creature's location.  Note that the path to
this destination is in the RouteCache.
*/
void AController::execFindRandomDest( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	if ( !Pawn )
		return;

	ANavigationPoint * bestPath = NULL;
	bPreparingMove = false;
	if ( Pawn->findPathToward(NULL,FVector(0,0,0),&FindRandomPath,0.f,false) > 0 )
		bestPath = Cast<ANavigationPoint>(RouteGoal);

	*(ANavigationPoint**)Result = bestPath;
}

void AController::execLineOfSightTo( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR(Other);
	P_GET_VECTOR_OPTX(chkLocation,FVector(0.f,0.f,0.f));
	P_FINISH;

	if (chkLocation.IsZero())
	{
		*(DWORD*)Result = LineOfSightTo(Other);
	}
	else
	{
		*(DWORD*)Result = LineOfSightTo(Other,0,&chkLocation);
	}
}

/* execMoveTo()
start moving to a point -does not use routing
Destination is set to a point
*/
void AController::execMoveTo( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(dest);
	P_GET_ACTOR_OPTX(viewfocus, NULL);
	P_GET_UBOOL_OPTX(bShouldWalk, false);
	P_FINISH;

	if ( !Pawn )
		return;

	if ( bShouldWalk != Pawn->bIsWalking )
		Pawn->eventSetWalking(bShouldWalk);
	FVector MoveDir = dest - Pawn->Location;
	MoveTarget = NULL;
	Pawn->bReducedSpeed = false;
	Pawn->DesiredSpeed = Pawn->MaxDesiredSpeed;
	Pawn->DestinationOffset = 0.f;
	Pawn->NextPathRadius = 0.f;
	Focus = viewfocus;
	Pawn->setMoveTimer(MoveDir); 
	GetStateFrame()->LatentAction = AI_PollMoveTo;
	Destination = dest;
	if ( !Focus )
		FocalPoint = Destination;
	bAdjusting = false;
	CurrentPath = NULL;
	NextRoutePath = NULL;
	Pawn->ClearSerpentine();
	AdjustLoc = Destination;
	bAdvancedTactics = false;
	Pawn->moveToward(Destination, NULL);
}

void AController::execPollMoveTo( FFrame& Stack, RESULT_DECL )
{
	if( !Pawn || ((MoveTimer < 0.f) && (Pawn->Physics != PHYS_Falling)) )
	{
		GetStateFrame()->LatentAction = 0;
		return;
	}
	if ( bAdjusting )
		bAdjusting = !Pawn->moveToward(AdjustLoc, NULL);
	if( !bAdjusting && (!Pawn || Pawn->moveToward(Destination, NULL)) )
		GetStateFrame()->LatentAction = 0;
	else
		CheckFears();
}
IMPLEMENT_FUNCTION( AController, AI_PollMoveTo, execPollMoveTo);

/* CheckFears()
Adjust pawn movement to avoid active FearSpots
*/
void AController::CheckFears()
{
/* FIXMESTEVE
	if ( Pawn->Acceleration.IsZero() )
		return;

	FVector FearAdjust(0.f,0.f,0.f);
	for ( INT i=0; i<2; i++ )
	{
		if ( FearSpots[i] )
		{
			if ( FearSpots[i]->bDeleteMe )
				FearSpots[i] = NULL;
			else if ( (Square(Pawn->Location.Z - FearSpots[i]->Location.Z) > Square(Pawn->CollisionHeight + FearSpots[i]->CollisionHeight))
					||	(Square(Pawn->Location.X - FearSpots[i]->Location.X) + Square(Pawn->Location.Y - FearSpots[i]->Location.Y)
						> Square(Pawn->CollisionRadius + FearSpots[i]->CollisionRadius)) )
				FearSpots[i] = NULL;
			else
				FearAdjust += (Pawn->Location - FearSpots[i]->Location)/FearSpots[i]->CollisionRadius;
		}
	}

	if ( FearAdjust.IsZero() )
		return;

	FearAdjust.Normalize();
	FLOAT PawnAccelRate = Pawn->Acceleration.Size();
	FVector PawnDir = Pawn->Acceleration/PawnAccelRate;

	if ( (FearAdjust | PawnDir) > 0.7f )
		return;

	if ( (FearAdjust | PawnDir) < -0.7f )
	{
		FVector LeftDir = PawnDir ^ FVector(0.f,0.f,1.f);	
		LeftDir = LeftDir.SafeNormal();
		FearAdjust = 2.f * LeftDir;
		if ( (LeftDir | FearAdjust) < 0.f )
			FearAdjust *= -1.f;
	}

	Pawn->Acceleration = (PawnDir + FearAdjust).SafeNormal();
	Pawn->Acceleration *= PawnAccelRate;
*/
}

void AController::execEndClimbLadder( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	if ( (GetStateFrame()->LatentAction == AI_PollMoveToward)
		&& Pawn && MoveTarget && MoveTarget->IsA(ALadder::StaticClass()) )
	{
		if ( Pawn->IsOverlapping(MoveTarget) )
			Pawn->SetAnchor(Cast<ANavigationPoint>(MoveTarget));
		GetStateFrame()->LatentAction = 0;
	}
}

/* execInLatentExecution()
returns true if controller currently performing latent execution with
passed in LatentAction value
*/
void AController::execInLatentExecution( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(LatentActionNumber);
	P_FINISH;

	*(DWORD*)Result = ( GetStateFrame()->LatentAction == LatentActionNumber );
}

void AController::execStopLatentExecution( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	GetStateFrame()->LatentAction = 0;
	LatentFloat = -1.f;
}

/* execMoveToward()
start moving toward a goal actor -does not use routing
MoveTarget is set to goal
*/
void AController::execMoveToward( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR(goal);
	P_GET_ACTOR_OPTX(viewfocus, goal);
	P_GET_FLOAT_OPTX(DesiredOffset,0.f);
	P_GET_UBOOL_OPTX(bStrafe, false);
	P_GET_UBOOL_OPTX(bShouldWalk, false);
	P_FINISH;

	if ( !goal || !Pawn )
	{
		//Stack.Log("MoveToward with no goal");
		return;
	}

	if ( bShouldWalk != Pawn->bIsWalking )
		Pawn->eventSetWalking(bShouldWalk);
	FVector Move = goal->Location - Pawn->Location;	
	Pawn->bReducedSpeed = false;
	Pawn->DesiredSpeed = Pawn->MaxDesiredSpeed;
	MoveTarget = goal;
	Focus = viewfocus;
	if ( goal->GetAPawn() )
		MoveTimer = 1.2f; //max before re-assess movetoward
	else
		Pawn->setMoveTimer(Move);

	Destination = MoveTarget->GetDestination(); 
	GetStateFrame()->LatentAction = AI_PollMoveToward;
	bAdjusting = false;
	AdjustLoc = Destination;
	Pawn->ClearSerpentine();
	bAdvancedTactics = bStrafe && ( (PlayerReplicationInfo && PlayerReplicationInfo->bHasFlag) || bForceStrafe || (Level->NetMode != NM_Standalone) || (Level->TimeSeconds - Pawn->LastRenderTime < 5.f) || bSoaking );

	// if necessary, allow the pawn to prepare for this move
	// give pawn the opportunity if its a navigation network move,
	// based on the reachspec
	ANavigationPoint *NavGoal = Cast<ANavigationPoint>(goal);

	FLOAT NewDestinationOffset = 0.f;
	CurrentPath = NULL;
	NextRoutePath = NULL;
	if ( NavGoal )
	{
		if ( NavGoal->bSpecialMove )
			NavGoal->eventSuggestMovePreparation(Pawn);			
		if ( Pawn->ValidAnchor() )
		{
			// if the reachspec isn't currently supported by the pawn
			// then give the pawn an opportunity to perform some latent preparation
			// (Controller will set its bPreparingMove=true if it needs latent preparation)
			CurrentPath = Pawn->Anchor->GetReachSpecTo(NavGoal);
			if ( CurrentPath )
			{
				if ( CurrentPath->bForced && NavGoal->bSpecialForced )
					NavGoal->eventSuggestMovePreparation(Pawn);
				else if ( !CurrentPath->supports(Pawn->CylinderComponent->CollisionRadius,Pawn->CylinderComponent->CollisionHeight,Pawn->calcMoveFlags(),Pawn->MaxFallSpeed) )
					eventPrepareForMove(NavGoal, CurrentPath);
/* FIXMESTEVE
				else if ( (Pawn->Physics == PHYS_Walking)
						&& (Pawn->Location.Z + UCONST_MAXSTEPHEIGHT >= CurrentPath->End->Location.Z)
						&& ((CurrentPath->reachFlags & (R_FORCED + R_WALK)) == CurrentPath->reachFlags) 
						&& (appFrand() < DodgeToGoalPct) ) 
				{
					eventMayDodgeToMoveTarget();
					if ( NavGoal != MoveTarget )
					{
						CurrentPath = NULL;
						NextRoutePath = NULL;
						NavGoal = Cast<ANavigationPoint>(MoveTarget);
					}
				}
*/
				if ( CurrentPath )
				{
					NextRoutePath = NULL;
					if ( RouteGoal && (NavGoal == CurrentPath->End) && (RouteGoal != NavGoal) )
					{
						for ( INT i=0; i < RouteCache.Num() - 1; i++ )
						{
							if ( !RouteCache(i) )
								break;
							else if ( RouteCache(i) == CurrentPath->End )
							{
								ANavigationPoint *NextNav = RouteCache(i+1) ? Cast<ANavigationPoint>(RouteCache(i+1)) : NULL;
								if ( NextNav )
									NextRoutePath = CurrentPath->End->GetReachSpecTo(NextNav);
								break;
							}
						}
					}
			    CurrentPathDir = CurrentPath->End->Location - CurrentPath->Start->Location;
			    CurrentPathDir = CurrentPathDir.SafeNormal();
/* FIXMESTEVE
					// handle leaving FlyingPathNodes (since don't go all the way to the center)
					if ( (Pawn->Physics == PHYS_Flying) || (Pawn->Physics == PHYS_Swimming) )
					{
						AFlyingPathNode *StartFPN = Cast<AFlyingPathNode>(CurrentPath->Start);
						// need to alter direction to get out of current FlyingPathNode region safely, if still in it
						if ( StartFPN && ((Abs(StartFPN->Location.Z - Destination.Z) > StartFPN->CollisionHeight)
									|| ((StartFPN->Location - Destination).Size2D() > StartFPN->CollisionRadius)) )
						{
							FCheckResult Hit(1.f);
							FVector Start = StartFPN->Location;
							if ( !Cast<AFlyingPathNode>(CurrentPath->End) && ((StartFPN->Location - Destination).Size2D() < StartFPN->CollisionRadius) 
								&& (Destination.Z < StartFPN->Location.Z) )
							{
								Start = Destination;
								Start.Z = StartFPN->Location.Z - StartFPN->CollisionHeight + 10.f;
							}
							if ( !StartFPN->GetPrimitive()->LineCheck(Hit,StartFPN,Start,Destination,FVector(0.f,0.f,0.f),0,0) )
							{
								bAdjusting = true;
								AdjustLoc = Hit.Location;
							}
						}
					}
*/
				}
			}
		}

		if ( !NavGoal->bNeverUseStrafing && !NavGoal->bForceNoStrafing )
		{
			if ( CurrentPath )
			{
				// round corners smoothly
				// start serpentine dir in current direction
				Pawn->SerpentineTime = 0.f;
				Pawn->SerpentineDir = Pawn->Velocity.SafeNormal();
				Pawn->SerpentineDist = Clamp(CurrentPath->CollisionRadius - Pawn->CylinderComponent->CollisionRadius,0.f,4.f * Pawn->CylinderComponent->CollisionRadius)
										* (0.5f + 1.f * appFrand());
			    FLOAT DP = CurrentPathDir | Pawn->SerpentineDir;
				FLOAT DistModifier = 1.f - DP*DP*DP*DP;
				if ( (DP < 0) && (DistModifier < 0.5f) )
					Pawn->SerpentineTime = 0.8f;
				else
					Pawn->SerpentineDist *= DistModifier;
			}
			if ( NavGoal != RouteGoal )
				NewDestinationOffset = (0.7f + 0.3f * appFrand()) * ::Max(0.f, Pawn->NextPathRadius - Pawn->CylinderComponent->CollisionRadius);
		}
	}
	Pawn->DestinationOffset = (DesiredOffset == 0.f) ? NewDestinationOffset : DesiredOffset;
	Pawn->NextPathRadius = 0.f;
	if ( !bPreparingMove )
		Pawn->moveToward(Destination, MoveTarget);
}

FVector AActor::GetDestination()
{
	return Location;
}

FVector AVehicle::GetDestination()
{
	return Location;
}

void AController::execPollMoveToward( FFrame& Stack, RESULT_DECL )
{
	if( !MoveTarget || !Pawn || ((MoveTimer < 0.f) && (Pawn->Physics != PHYS_Falling)) )
	{
/*		if ( (MoveTimer < 0.f) && MoveTarget && PlayerReplicationInfo )
		{
			debugf(TEXT("%s movetimer %f moving toward %s"),*(PlayerReplicationInfo->PlayerName),MoveTimer,MoveTarget->GetName());
			if ( Pawn )
				debugf(TEXT("distance remaining %f"),(Pawn->Location - MoveTarget->Location).Size());
		}
*/
		//Stack.Log("MoveTarget cleared during movetoward");
		GetStateFrame()->LatentAction = 0;
		return;
	}
	// check that pawn is ready to go
	if ( bPreparingMove )
		return;
	// check if adjusting around an obstacle
	if ( bAdjusting )
		bAdjusting = !Pawn->moveToward(AdjustLoc, MoveTarget);
	if ( !MoveTarget || !Pawn )
	{
		GetStateFrame()->LatentAction = 0;
		return;
	}
/* FIXMESTEVE
	if ( bAdjusting && Cast<AFlyingPathNode>(MoveTarget) )
	{
		if ( Pawn->ReachedDestination(Pawn->Location - MoveTarget->Location, MoveTarget) )
		{
			GetStateFrame()->LatentAction = 0;
			return;
		}
		else if ( (Pawn->Velocity | (AdjustLoc - Pawn->Location)) < 0.f )
			bAdjusting = false;
	}
*/
	if ( !bAdjusting )
	{
		// set destination to current movetarget location
		Destination = MoveTarget->GetDestination();
		// JAG_COLRADIUS_HACK
		FLOAT MoveTargetRadius, MoveTargetHeight;
		MoveTarget->GetBoundingCylinder(MoveTargetRadius, MoveTargetHeight);
/* FIXMESTEVE
		if( Pawn->Physics==PHYS_Flying )
		{
			if ( MoveTarget->GetAPawn() )
			{
				if ( MoveTarget->GetAPawn()->bStationary )
					Destination.Z += 2.f * ::Max(MoveTargetHeight, 2.5f * ColCyl->CollisionHeight);
				else
					Destination.Z += 0.7f * MoveTargetHeight;
			}
			else if ( MoveTarget->IsA(ANavigationPoint::StaticClass()) && !MoveTarget->IsA(AFlyingPathNode::StaticClass()) )
			{
				if ( MoveTarget->IsA(ALiftExit::StaticClass()) && CurrentPath && CurrentPath->Start->IsA(ALiftCenter::StaticClass()) )
				{
					Destination = Pawn->Location;
					Destination.Z = MoveTarget->Location.Z;
				}
				else 
				if ( Pawn->Location.Z < Destination.Z )
				{
					if ( Pawn->Location.Z > Destination.Z - MoveTargetHeight )
						Destination.Z += MoveTargetHeight;
					else
						Destination.Z += 500.f;
				}
			}
		}
		else
*/

		if( Pawn->Physics == PHYS_Spider )
			Destination = Destination - MoveTargetRadius * Pawn->Floor;

		FLOAT oldDesiredSpeed = Pawn->DesiredSpeed;
		FVector CurrentDest = Destination;

		// move to movetarget
		if( Pawn->moveToward(CurrentDest, MoveTarget) )
			GetStateFrame()->LatentAction = 0;
		else if ( MoveTarget && Pawn && (Pawn->Physics == PHYS_Walking) )
		{
			FVector Diff = Pawn->Location - Destination;
			FLOAT DiffZ = Diff.Z;
			Diff.Z = 0.f;
			// reduce timer if seem to be stuck above or below
			if ( Diff.SizeSquared() < Pawn->CylinderComponent->CollisionRadius * Pawn->CylinderComponent->CollisionRadius )
			{
				MoveTimer -= Pawn->AvgPhysicsTime;
				if ( DiffZ > Pawn->CylinderComponent->CollisionRadius + 2 * Pawn->MaxStepHeight )
				{
					// check if visible below
					FCheckResult Hit(1.f);
					GetLevel()->SingleLineCheck(Hit, Pawn, Destination, Pawn->Location, TRACE_World|TRACE_StopAtFirstHit);
					if ( (Hit.Time < 1.f) && (Hit.Actor != MoveTarget) )
						GetStateFrame()->LatentAction = 0;
				}
			}
		}
		if ( !MoveTarget || !Pawn )
		{
			GetStateFrame()->LatentAction = 0;
			return;
		}
		if ( GetStateFrame()->LatentAction != 0 )
			CheckFears();

		Destination = MoveTarget->Location;
		if( MoveTarget->GetAPawn() )
		{
			Pawn->DesiredSpeed = oldDesiredSpeed; //don't slow down when moving toward a pawn
			if ( !Pawn->bCanSwim && MoveTarget->PhysicsVolume->bWaterVolume )
				MoveTimer = -1.f; //give up
		}
	}
}
IMPLEMENT_FUNCTION( AController, AI_PollMoveToward, execPollMoveToward);

/* execTurnToward()
turn toward Focus
*/
void AController::execFinishRotation( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	GetStateFrame()->LatentAction = AI_PollFinishRotation;
}

void AController::execPollFinishRotation( FFrame& Stack, RESULT_DECL )
{

	if( !Pawn || Pawn->ReachedDesiredRotation() )
		GetStateFrame()->LatentAction = 0;

}
IMPLEMENT_FUNCTION( AController, AI_PollFinishRotation, execPollFinishRotation);

/*
SeePawn()

returns true if Other was seen by this controller's pawn.  Chance of seeing other pawn decreases with increasing
distance or angle in peripheral vision
*/
DWORD AController::SeePawn(APawn *Other, UBOOL bMaySkipChecks)
{
	if ( !Other || !Pawn )
		return 0;

	if (Other != Enemy)
		bLOSflag = !bLOSflag;
	else
		return LineOfSightTo(Other);

/* FIXMESTEVE
	if ( Pawn->Region.Zone->bDistanceFog 
		&& ((Pawn->Location - Other->Location).SizeSquared() > Square(500.f+Pawn->Region.Zone->DistanceFogEnd+Other->CollisionRadius)) )
		return 0;
*/
	FLOAT maxdist = Pawn->SightRadius * Min(1.f, (FLOAT)(Other->Visibility * 0.0078125f)); // * 1/128

	// fixed max sight distance
	if ( (Other->Location - Pawn->Location).SizeSquared() > maxdist * maxdist )
		return 0;

	FLOAT dist = (Other->Location - Pawn->Location).Size();

	// may skip if more than 1/5 of maxdist away (longer time to acquire)
	if ( bMaySkipChecks && (appFrand() * dist > 0.1f * maxdist) )
			return 0;

	// check field of view
	FVector SightDir = (Other->Location - Pawn->Location).SafeNormal();
	FVector LookDir = Rotation.Vector();
	FLOAT Stimulus = (SightDir | LookDir);
	if ( Stimulus < Pawn->PeripheralVision )
		return 0;

	// need to make this only have effect at edge of vision
	//if ( bMaySkipChecks && (appFrand() * (1.f - Pawn->PeripheralVision) < 1.f - Stimulus) )
	//	return 0;
	if ( bMaySkipChecks && bSlowerZAcquire && (appFrand() * dist > 0.1f * maxdist) )
	{
		// lower FOV vertically
		SightDir.Z *= 2.f;
		SightDir.Normalize();
		if ( (SightDir | LookDir) < Pawn->PeripheralVision )
			return 0;

		// notice other pawns at very different heights more slowly
		FLOAT heightMod = Abs(Other->Location.Z - Pawn->Location.Z);
		if ( appFrand() * dist < heightMod )
			return 0;
	}

	Stimulus = 1;
	return LineOfSightTo(Other, bMaySkipChecks);

}

AActor* AController::GetViewTarget()
{
	if ( Pawn )
		return Pawn;
	return this;
}

AActor* APlayerController::GetViewTarget()
{
	if ( PlayerCamera )
	{
		return PlayerCamera->GetViewTarget();
	}

	if ( RealViewTarget && !RealViewTarget->bDeleteMe )
	{
		AActor *OldViewTarget = ViewTarget;
		if ( RealViewTarget->Pawn )
		{
			if ( RealViewTarget->Pawn->bDeleteMe )
				ViewTarget = RealViewTarget;
			else
				ViewTarget = RealViewTarget->Pawn;
		}
		else if ( RealViewTarget->GetAPlayerController() && RealViewTarget->GetAPlayerController()->ViewTarget )
			ViewTarget = RealViewTarget->GetAPlayerController()->ViewTarget;
		else if ( !ViewTarget )
			ViewTarget = RealViewTarget;
		if ( (OldViewTarget != ViewTarget)
			&& !LocalPlayerController() )
		{
			eventClientSetViewTarget(ViewTarget);
		}
	}
	if ( !ViewTarget || ViewTarget->bDeleteMe )
	{
		if ( Pawn && !Pawn->bDeleteMe && !Pawn->bPendingDelete )
			ViewTarget = Pawn;
		else
			ViewTarget = this;
	}
	return ViewTarget;
}

/** Set the player controller to treat NewTarget as its new view target. */
void APlayerController::SetViewTarget(AActor* NewViewTarget)
{
	if ( PlayerCamera )
	{
		PlayerCamera->SetViewTarget( NewViewTarget );
		return;
	}

	ViewTarget = NewViewTarget;
	//FIXMESTEVE	LastPlayerCalcView = 0.f;
	if ( ViewTarget == this )
		RealViewTarget = NULL;
	else
		RealViewTarget = Cast<AController>(ViewTarget);
	GetViewTarget();
	if ( ViewTarget == this )
		RealViewTarget = NULL;
}

/*
LineOfSightTo()
returns true if controller's pawn can see Other actor.
Checks line to center of other actor, and possibly to head or box edges depending on distance
*/
DWORD AController::LineOfSightTo(AActor *Other, INT bUseLOSFlag, FVector *chkLocation)
{
	if ( !Other )
		return 0;

	AActor*		ViewTarg	= GetViewTarget();
	// FIXMESTEVE - Getviewpoint for 3rd person? - used by PCs?
// eventGetPlayerViewPoint( ViewPoint, ViewRot );
	FVector ViewPoint = ViewTarg->Location;
	if ( ViewTarg == Pawn )
	{
		ViewPoint.Z += Pawn->BaseEyeHeight; //look from eyes
	}
	// check for a viewpoint override
	if (chkLocation != NULL)
	{
		ViewPoint = *chkLocation;
	}

	// JAG_COLRADIUS_HACK
	FLOAT OtherRadius, OtherHeight;
	Other->GetBoundingCylinder(OtherRadius, OtherHeight);

	FCheckResult Hit(1.f);
	if ( Other == Enemy )
	{
/* FIXMESTEVE
		if ( ViewTarg->Region.Zone->bDistanceFog 
			&& ((ViewTarg->Location - Other->Location).SizeSquared() > Square(1000.f+ViewTarg->Region.Zone->DistanceFogEnd+Other->CollisionRadius)) )
			return 0;
*/
		GetLevel()->SingleLineCheck( Hit, this, Other->Location, ViewPoint, TRACE_World|TRACE_StopAtFirstHit );
		if ( Hit.Actor && (Hit.Actor != Other) )
			GetLevel()->SingleLineCheck( Hit, this, Enemy->Location + FVector(0,0,Enemy->BaseEyeHeight), ViewPoint, TRACE_World|TRACE_StopAtFirstHit );

		if ( !Hit.Actor || (Hit.Actor == Other) )
		{
			// update enemy info 
			// NOTE that controllers update this info even if the enemy is behind them
			// unless they check for this in UpdateEnemyInfo()
			UpdateEnemyInfo(Enemy);
			return 1;
		}
		// only check sides if width of other is significant compared to distance
		if ( OtherRadius * OtherRadius/(Other->Location - ViewPoint).SizeSquared() < 0.0001f )
			return 0;
	}
	else
	{
/* FIXMESTEVE
		if ( ViewTarg->Region.Zone->bDistanceFog 
			&& ((ViewTarg->Location - Other->Location).SizeSquared() > Square(500.f+ViewTarg->Region.Zone->DistanceFogEnd+Other->CollisionRadius)) )
			return 0;
*/
		GetLevel()->SingleLineCheck( Hit, this, Other->Location, ViewPoint, TRACE_World|TRACE_StopAtFirstHit );
		if ( !Hit.Actor || (Hit.Actor == Other) )
			return 1;

		FLOAT distSq = (Other->Location - ViewTarg->Location).SizeSquared();
		if ( distSq > 64000000.f )
			return 0;
		if ( (!bIsPlayer || !Other->GetAPawn()) && (distSq > 4000000.f) ) 
			return 0;
		
		//try viewpoint to head
		if ( !bUseLOSFlag || !bLOSflag )
		{
			GetLevel()->SingleLineCheck( Hit, this, Other->Location + FVector(0,0,OtherHeight * 0.8), ViewPoint, TRACE_World|TRACE_StopAtFirstHit );
			if ( !Hit.Actor || (Hit.Actor == Other) )
				return 1;
		}

		// bLOSFlag used by SeePawn to reduce visibility checks
		if ( bUseLOSFlag && !bLOSflag )
			return 0;
		// only check sides if width of other is significant compared to distance
		if ( OtherRadius * OtherRadius/distSq < 0.00015f )
			return 0;
	}

	//try checking sides - look at dist to four side points, and cull furthest and closest
	FVector Points[4];
	Points[0] = Other->Location - FVector(OtherRadius, -1 * OtherRadius, 0);
	Points[1] = Other->Location + FVector(OtherRadius, OtherRadius, 0);
	Points[2] = Other->Location - FVector(OtherRadius, OtherRadius, 0);
	Points[3] = Other->Location + FVector(OtherRadius, -1 * OtherRadius, 0);
	int imin = 0;
	int imax = 0;
	FLOAT currentmin = Points[0].SizeSquared();
	FLOAT currentmax = currentmin;
	for ( INT i=1; i<4; i++ )
	{
		FLOAT nextsize = Points[i].SizeSquared();
		if (nextsize > currentmax)
		{
			currentmax = nextsize;
			imax = i;
		}
		else if (nextsize < currentmin)
		{
			currentmin = nextsize;
			imin = i;
		}
	}

	for ( INT i=0; i<3; i++ )
		if	( (i != imin) && (i != imax) )
		{
			GetLevel()->SingleLineCheck( Hit, this, Points[i], ViewPoint, TRACE_World|TRACE_StopAtFirstHit );
			if ( !Hit.Actor || (Hit.Actor == Other) )
				return 1;
		}
	return 0;
}

/* CanHear()

Returns 1 if controller can hear this noise
Several types of hearing are supported

Noises must be perceptible (based on distance, loudness, and the alerntess of the controller

  Options for hearing are: (assuming the noise is perceptible

  bSameZoneHearing = Hear any perceptible noise made in the same zone
  bAdjacentZoneHearing = Hear any perceptible noise made in the same or an adjacent zone
  bLOSHearing = Hear any perceptible noise which is not blocked by geometry
  bAroundCornerHearing = Hear any noise around one corner (bLOSHearing must also be true)

*/
INT AController::CanHear(FVector NoiseLoc, FLOAT Loudness, AActor *Other)
{
	if ( bUsePlayerHearing || !Other->Instigator || !Other->Instigator->Controller || !Pawn )
		return 0; //ignore sounds from uncontrolled (dead) pawns, or if don't have a pawn to control

	FLOAT DistSq = (Pawn->Location - NoiseLoc).SizeSquared();
	FLOAT Perceived = Loudness * Pawn->HearingThreshold * Pawn->HearingThreshold;

	// take pawn alertness into account (it ranges from -1 to 1 normally)
	Perceived *= ::Max(0.f,(Pawn->Alertness + 1.f));

	// check if sound is too quiet to hear
	if ( Perceived < DistSq )
		return 0;

	// check if in same zone
	if ( (Pawn->bSameZoneHearing || Pawn->bAdjacentZoneHearing) && (Pawn->Region.Zone == Other->Region.Zone) )
		return 1;

	// check if in adjacent zone
	if ( Pawn->bAdjacentZoneHearing
		&& GetLevel()->Model->Zones[Pawn->Region.ZoneNumber].Connectivity.ContainsZone(Other->Region.ZoneNumber) )
		return 1;

	if ( !Pawn->bLOSHearing )
		return 0;

	// check if Line of Sight
	// NOTE - still using FastTrace, so terrain and static meshes won't block
	FVector ViewLoc = Pawn->Location + FVector(0,0,Pawn->BaseEyeHeight);
	FCheckResult Hit(1.f);
	GetLevel()->SingleLineCheck(Hit, this, NoiseLoc, ViewLoc, TRACE_Level);
	if ( Hit.Time == 1.f )
		return 1;

	if ( Pawn->bMuffledHearing )
	{
		// sound distance increased to double plus 4x the distance through BSP walls
		if ( Perceived > 4 * DistSq )
		{
			// check dist inside of walls
			FVector FirstHit = Hit.Location;
			GetLevel()->SingleLineCheck(Hit, this, ViewLoc, NoiseLoc, TRACE_Level);
			FLOAT WallDistSq = (FirstHit - Hit.Location).SizeSquared();

			if ( Perceived > 4 * DistSq + WallDistSq * WallDistSq )
				return 1;
		}
	}

	if ( !Pawn->bAroundCornerHearing )
		return 0;

	// check if around corner
	// using navigation network
	Perceived *= 0.125f; // distance to corner must be < 0.7 * max distance
	FSortedPathList SoundPoints;

	// find potential waypoints for sound propagation
	for ( ANavigationPoint *Nav=Level->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		if ( Nav->bPropagatesSound )
		{
			FLOAT D1 = (Nav->Location - Pawn->Location).SizeSquared();
			FLOAT D2 = (Nav->Location - Other->Location).SizeSquared();
			if ( (D1 < Perceived) && (D2 < Perceived) )
				SoundPoints.addPath(Nav, appRound(D1+D2));
		}

	if ( SoundPoints.numPoints == 0 )
		return 0;

	for ( INT i=0; i<SoundPoints.numPoints; i++ )
		if ( GetLevel()->Model->FastLineCheck(SoundPoints.Path[i]->Location, NoiseLoc)
			&& GetLevel()->Model->FastLineCheck(SoundPoints.Path[i]->Location, ViewLoc) )
			return 1;
	return 0;
}

/* Send a HearNoise() message to all Controllers which could possibly hear this noise
*/
void AActor::CheckNoiseHearing(FLOAT Loudness)
{
	if ( !Instigator || !Instigator->Controller )
		return;

	FLOAT CurrentTime = GetLevel()->TimeSeconds;

	// allow only one noise per 0.2 seconds from a given instigator & area (within 50 units) unless much louder
	// check the two sound slots
	if ( (Instigator->noise1time > CurrentTime - 0.2f)
		 && ((Instigator->noise1spot - Location).SizeSquared() < 2500.f)
		 && (Instigator->noise1loudness >= 0.9f * Loudness) )
	{
		return;
	}

	if ( (Instigator->noise2time > CurrentTime - 0.2f)
		 && ((Instigator->noise2spot - Location).SizeSquared() < 2500.f)
		 && (Instigator->noise2loudness >= 0.9f * Loudness) )
	{
		return;
	}

	// put this noise in a slot
	if ( Instigator->noise1time < CurrentTime - 0.18f )
	{
		Instigator->noise1time = CurrentTime;
		Instigator->noise1spot = Location;
		Instigator->noise1loudness = Loudness;
	}
	else if ( Instigator->noise2time < CurrentTime - 0.18f )
	{
		Instigator->noise2time = CurrentTime;
		Instigator->noise2spot = Location;
		Instigator->noise2loudness = Loudness;
	}
	else if ( ((Instigator->noise1spot - Location).SizeSquared() < 2500)
			  && (Instigator->noise1loudness <= Loudness) )
	{
		Instigator->noise1time = CurrentTime;
		Instigator->noise1spot = Location;
		Instigator->noise1loudness = Loudness;
	}
	else if ( Instigator->noise2loudness <= Loudness )
	{
		Instigator->noise1time = CurrentTime;
		Instigator->noise1spot = Location;
		Instigator->noise1loudness = Loudness;
	}

	// if the noise is not made by a player or an AI with a player as an enemy, then only send it to
	// other AIs with the same tag
	if ( !Instigator->IsPlayer()
		&& (!Instigator->Controller->Enemy || !Instigator->Controller->Enemy->IsPlayer()) )
	{
		for ( AController *next=Level->ControllerList; next!=NULL; next=next->NextController )
			if ( (next->Pawn != Instigator) && next->IsProbing(NAME_HearNoise)
				&& (next->Tag == Tag)
				&& next->CanHear(Location, Loudness, this) )
				next->eventHearNoise(Loudness, this);
		return;
	}

	// all pawns can hear this noise
	for ( AController *P=Level->ControllerList; P!=NULL; P=P->NextController )
		if ( (P->Pawn != Instigator) && P->IsProbing(NAME_HearNoise)
			 && P->CanHear(Location, Loudness, this) )
			 P->eventHearNoise(Loudness, this);

}

void AController::CheckEnemyVisible()
{
	if ( Enemy )
	{
		check(Enemy->IsValid());
		if ( !LineOfSightTo(Enemy) )
			eventEnemyNotVisible();
	}
}

/* Player shows self to pawns that are ready
*/
void AController::ShowSelf()
{

	if ( !Pawn )
		return;
	for ( AController *C=Level->ControllerList; C!=NULL; C=C->NextController )
	{
		if( C!=this  && (bIsPlayer || C->bIsPlayer) && C->SightCounter<0.f )
		{
			//check visibility
			if ( (bIsPlayer ? C->IsProbing(NAME_SeePlayer) : C->IsProbing(NAME_SeeMonster))
				&& C->SeePawn(Pawn) )
			{
				if ( bIsPlayer )
					C->eventSeePlayer(Pawn);
				else
					C->eventSeeMonster(Pawn);
			}
		}
	}
}

/*
SetPath()
Based on the results of the navigation network (which are stored in RouteCache[],
return the desired path.  Check if there are any intermediate goals (such as hitting a
switch) which must be completed before continuing toward the main goal
*/
AActor* AController::SetPath(INT bInitialPath)
{
	static AActor* ChosenPaths[4];

	AActor * bestPath = RouteCache(0);

	if ( !Pawn->ValidAnchor() )
		return bestPath;	// make sure on network before trying to find complex routes

	if ( bInitialPath )
	{
		for ( INT i=0; i<4; i++ )
			ChosenPaths[i] = NULL;
		// if this is setting the path toward the main (final) goal
		// make sure still same goal as before
		if ( RouteGoal == GoalList[0] )
		{
			// check for existing intermediate goals
			if ( GoalList[1] )
			{
				INT i = 1;
				while ( GoalList[i] )
					i++;
				AActor* RealGoal = GoalList[i-1];
				if ( Pawn->actorReachable(RealGoal) )
				{
					// I can reach the intermediate goal, so
					GoalList[i-1] = NULL;
					bPreparingMove = false;
					return RealGoal;
				}
				// find path to new goal
				UBOOL bOldPrep = bPreparingMove;
				bPreparingMove = false;
				if ( Pawn->findPathToward(RealGoal,RealGoal->Location,NULL, 0.f,false) > 0.f )
				{
					bestPath = SetPath(0);
				}
				else
				{
					bPreparingMove = bOldPrep;
				}
			}
		}
		else
		{
			GoalList[0] = RouteGoal;
			for ( INT i=1; i<4; i++ )
				GoalList[i] = NULL;
		}
	}
	else
	{
		// add new goal to goal list
		for ( INT i=0; i<4; i++ )
		{
			if ( GoalList[i] == RouteGoal )
				break;
			if ( !GoalList[i] )
			{
				GoalList[i] = RouteGoal;
				break;
			}
		}
	}
	for ( INT i=0; i<4; i++ )
	{
		if ( ChosenPaths[i] == NULL )
		{
			ChosenPaths[i] = bestPath;
			break;
		}
		else if ( ChosenPaths[i] == bestPath )
			return bestPath;
	}
	if ( bestPath && bestPath->IsProbing(NAME_SpecialHandling) )
		bestPath = HandleSpecial(bestPath);
	return bestPath;
}


AActor* AController::HandleSpecial(AActor *bestPath)
{
	if ( !bCanDoSpecial || GoalList[3] )
		return bestPath;	//limit AI intermediate goal depth to 4

	AActor * newGoal = bestPath->eventSpecialHandling(Pawn);

	if ( newGoal && (newGoal != bestPath) )
	{
		UBOOL bOldPrep = bPreparingMove;
		bPreparingMove = false;
		// if can reach intermediate goal directly, return it
		if ( Pawn->actorReachable(newGoal) )
			return newGoal;

		// find path to new goal
		if ( Pawn->findPathToward(newGoal,newGoal->Location,NULL, 0.f,false) > 0.f )
		{
			bestPath = SetPath(0);
		}
		else
		{
			bPreparingMove = bOldPrep;
		}
	}
	return bestPath;

}

/* AcceptNearbyPath() returns true if the controller will accept a path which gets close to
and withing sight of the destination if no reachable path can be found.
*/
INT AController::AcceptNearbyPath(AActor *goal)
{
	return 0;
}

INT AAIController::AcceptNearbyPath(AActor *goal)
{
	if ( Cast<AVehicle>(Pawn) )
		return true;
	return (goal && (goal->GetAPawn() || (goal->Physics == PHYS_Falling)) );
}

/* AdjustFromWall()
Gives controller a chance to adjust around an obstacle and keep moving
*/

void AController::AdjustFromWall(FVector HitNormal, AActor* HitActor)
{
}

void AAIController::AdjustFromWall(FVector HitNormal, AActor* HitActor)
{
	if ( bAdjustFromWalls
		&& ((GetStateFrame()->LatentAction == AI_PollMoveTo)
			|| (GetStateFrame()->LatentAction == AI_PollMoveToward)) )
	{
/* FIXMESTEVE
		if ( Pawn && MoveTarget )
		{
            AMover *HitMover = HitActor ? HitActor->GetAMover() : NULL;//Cast<AMover>(HitActor);
			if ( HitMover && MoveTarget->HasAssociatedLevelGeometry(HitMover) )
			{
				ANavigationPoint *Nav = Cast<ANavigationPoint>(MoveTarget);
				if ( !Nav || !Nav->bSpecialMove || !Nav->eventSuggestMovePreparation(Pawn) )
					eventNotifyHitMover(HitNormal,HitMover);
				return;
			}
		}
*/
		if ( bAdjusting )
		{
			MoveTimer = -1.f;
		}
		else
		{
			Pawn->SerpentineDir *= -1.f;
			if ( !Pawn->PickWallAdjust(HitNormal, HitActor) )
				MoveTimer = -1.f;
		}
	}
}

void AController::SetAdjustLocation(FVector NewLoc)
{
}

void AAIController::SetAdjustLocation(FVector NewLoc)
{
	bAdjusting = true;
	AdjustLoc = NewLoc;

}

void APlayerController::PostScriptDestroyed()
{
	// cheatmanager, adminmanager, and playerinput cleaned up in C++ PostScriptDestroyed()
	if ( PlayerInput )
		delete PlayerInput;
	PlayerInput = NULL;
	if ( CheatManager )
		delete CheatManager;
	CheatManager = NULL;
	Super::PostScriptDestroyed();
}

void AController::PostBeginPlay()
{
	Super::PostBeginPlay();
	// add to level controller list
	NextController = Level->ControllerList;
	Level->ControllerList = this;
}

void AController::PostScriptDestroyed()
{
	Super::PostScriptDestroyed();
	// remove from level controller list
	AController *next = Level->ControllerList;
	if (next == this)
	{
		Level->ControllerList = next->NextController;
	}
	else
	{
		while (next != NULL)
		{
			if (next->NextController == this)
			{
				next->NextController = NextController;
				break;
			}
			next = next->NextController;
		}
	}
}

