/*=============================================================================
	UnPawn.cpp: APawn AI implementation

  This contains both C++ methods (movement and reachability), as well as some
  AI related natives

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "FConfigCacheIni.h"
#include "UnPath.h"
#include "EngineAnimClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineAIClasses.h"

#if WITH_FACEFX
using namespace OC3Ent;
using namespace Face;
#endif

DECLARE_STATS_GROUP(TEXT("Pathfinding"),STATGROUP_PathFinding);
DECLARE_CYCLE_STAT(TEXT("Various Reachable"),STAT_PathFinding_Reachable,STATGROUP_PathFinding);
DECLARE_CYCLE_STAT(TEXT("FindPathToward"),STAT_PathFinding_FindPathToward,STATGROUP_PathFinding);
DECLARE_CYCLE_STAT(TEXT("BestPathTo"),STAT_PathFinding_BestPathTo,STATGROUP_PathFinding);

#if LOG_DETAILED_PATHFINDING_STATS
/** Global detailed pathfinding stats. */
FDetailedTickStats GDetailedPathFindingStats( 30, 10, 1, 20, TEXT("pathfinding") );
#endif

/*-----------------------------------------------------------------------------
	APawn object implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(APawn);
IMPLEMENT_CLASS(AMatineePawn);

void APawn::PostBeginPlay()
{
	Super::PostBeginPlay();
	if ( !bDeleteMe )
	{
		GWorld->AddPawn( this );
	}
}

void APawn::PostScriptDestroyed()
{
	Super::PostScriptDestroyed();
	GWorld->RemovePawn( this );
}

APawn* APawn::GetPlayerPawn() const
{
	if ( !Controller || !Controller->GetAPlayerController() )
		return NULL;

	return ((APawn *)this);
}

/** IsPlayerPawn()
return TRUE if controlled by a Player (AI or human) on local machine (any controller on server, localclient's pawn on client)
*/
UBOOL APawn::IsPlayerPawn() const
{
	return ( Controller && Controller->bIsPlayer );
}

/**
 * IsHumanControlled()
 8 TODO - remove the parameter, just cast to playercontroller otherwise!!!
 * @param PawnController - optional parameter so you can pass a controller that is associated with this pawn but is not attached to it
 * @return - TRUE if controlled by a real live human on the local machine.  On client, only local player's pawn returns TRUE
 */
UBOOL APawn::IsHumanControlled(class AController* PawnController)
{
	AController *PController = PawnController ? PawnController : Controller;

	return ( PController && PController->GetAPlayerController() ); 
}

/**
 * @return - TRUE if controlled by local (not network) player
 */
UBOOL APawn::IsLocallyControlled()
{
    return ( Controller && Controller->IsLocalController() );
}


/**
@RETURN TRUE if pawn is invisible to AI
*/
UBOOL APawn::IsInvisible()
{
	return FALSE;
}

UBOOL APawn::IsAliveAndWell() const
{
	return (Health > 0 && !bHidden && !bDeleteMe && !bPlayedDeath);
}

/** SetDesiredRotation function
* @param TargetDesiredRotation: DesiredRotation you want
* @param InLockDesiredRotation: I'd like to lock up DesiredRotation, please nobody else can touch it until I say it's done 
* @param InUnlockWhenReached: When you lock, set this to TRUE if you want it to be auto Unlock when reached desired rotation
* @param InterpolationTime: Give interpolation time to get to the desired rotation - Ignore RotationRate, but use this to get there
* @return TRUE if properly set, otherwise, return FALSE
**/
UBOOL APawn::SetDesiredRotation(FRotator TargetDesiredRotation,UBOOL InLockDesiredRotation,UBOOL InUnlockWhenReached,FLOAT InterpolationTime, UBOOL bResetRotationRate)
{
	if ( bLockDesiredRotation==FALSE )
	{
		DesiredRotation = TargetDesiredRotation.GetDenormalized();
		bLockDesiredRotation = InLockDesiredRotation;

		UBOOL bNewDesiredRotationSet = (bLockDesiredRotation || (DesiredRotation != Rotation.GetDenormalized()));
		// if DesiredRotation is set, but about to turn off since it's same, 
		// make rue reset DesiredRotation
		if (bDesiredRotationSet && !bNewDesiredRotationSet)
		{
			ResetDesiredRotation();
		}

		bDesiredRotationSet = bNewDesiredRotationSet;
		if (bDesiredRotationSet)
		{
			bUnlockWhenReached = InUnlockWhenReached;

			if (InterpolationTime >= 0.f)
			{
				FRotator DiffRotator = (TargetDesiredRotation.Clamp() - Rotation.Clamp());
				// Make a shortest route - otherwise you'll get unexpected fast turn since interpolation time is using longer route
				DiffRotator.MakeShortestRoute();
				if (InterpolationTime > 0.f)
				{
					// Note that this could be a lot bigger than 360 degree
					RotationRate = DiffRotator * (1.f/InterpolationTime);
				}
				else
				{
					RotationRate = DiffRotator * 1000.f; // so we can turn instantly - 
				}
			}
			else if( bResetRotationRate )
			{
				RotationRate = CastChecked<APawn>(GetArchetype())->RotationRate;
			}
		}

		return TRUE;
	}

	//debugf(TEXT("%s:Failed to SetDesiredRotation"), *GetName());

	//eventDebugFreezeGame();

	return FALSE;
}

/** LockDesiredRotation function
* @param Lock: Lock or Unlock CurrentDesiredRotation 
* @param InUnlockWhenReached: Unlock when reached desired rotation. This is only valid when Lock = TRUE 
*/
void APawn::LockDesiredRotation(UBOOL Lock, UBOOL InUnlockWhenReached)
{
	bLockDesiredRotation = Lock;

	if ( bLockDesiredRotation )
	{
		bUnlockWhenReached = InUnlockWhenReached;
		bDesiredRotationSet = TRUE;
	}
	else
	{
		// set rotation rate to be default again
		bUnlockWhenReached = FALSE;
		ResetDesiredRotation();
	}
}

/** ResetDesiredRotation function
* Clear RotationRate/Flag to go back to default behavior
* Unless it's locked. 
*/
void APawn::ResetDesiredRotation()
{
	if ( !bLockDesiredRotation )
	{
		bDesiredRotationSet = FALSE;
	}

	RotationRate = CastChecked<APawn>(GetArchetype())->RotationRate;
}

/** CheckDesiredRotation function
* Check to see if DesiredRotation is met, and it need to be clear or not
* This is called by physicsRotation to make sure it needs to be cleared
*/
void APawn::CheckDesiredRotation()
{
	// if desiredRotation is set
	//@note: SetDesiredRotation() should have already denormalized DesiredRotation
	if (bDesiredRotationSet && Rotation.GetDenormalized() == DesiredRotation)
	{
		// if auto unlocks is wanted,
		if (bUnlockWhenReached)
		{
			// it should reset the bUnlockWhenREached flag too
			LockDesiredRotation(FALSE);
		}
		else
		{
			// otherwise, Reset all values
			ResetDesiredRotation();
		}
	}
}

/** IsDesiredRotationInUse()
* See if DesiredRotation is used by somebody 
*/
UBOOL APawn::IsDesiredRotationInUse()
{
	return bDesiredRotationSet;
}

/** IsDesiredRotationLocked()
* See if DesiredRotation is used by somebody 
*/
UBOOL APawn::IsDesiredRotationLocked()
{
	return bLockDesiredRotation;
}

UBOOL APawn::ReachedDesiredRotation()
{
	// Only base success on Yaw 
	INT YawDiff = Abs((DesiredRotation.Yaw & 65535) - (Rotation.Yaw & 65535));
	return ( (YawDiff < AllowedYawError) || (YawDiff > 65535 - AllowedYawError) );
}

UBOOL APawn::ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	if( (TraceFlags & TRACE_Pawns) || (bStationary && (TraceFlags & TRACE_Others)) )
	{
		return (!(TraceFlags & TRACE_Blocking) || (SourceActor && SourceActor->IsBlockedBy(this,Primitive)));
	}

	return FALSE;
}

/** Save off commonly used nodes so the tree doesn't need to be iterated over often */
void APawn::CacheAnimNodes()
{
	for (INT i = 0; i < Mesh->AnimTickArray.Num(); i++)
	{
		if (Mesh->AnimTickArray(i)->IsA(UAnimNodeSlot::StaticClass()))
		{
			SlotNodes.AddItem(Cast<UAnimNodeSlot>(Mesh->AnimTickArray(i)));
		}
	}
}

void APawn::SetAnchor(ANavigationPoint *NewAnchor)
{
	//debug
//	debugf(TEXT("%s [%s] SET ANCHOR from %s to %s"), *GetName(), (GetStateFrame() && GetStateFrame()->StateNode) ? *GetStateFrame()->StateNode->GetName() : TEXT("NONE"), Anchor ? *Anchor->GetName() : TEXT("NONE"), NewAnchor ? *NewAnchor->GetName() : TEXT("NONE") );

	// clear the previous anchor reference
	if (Anchor != NULL &&
		Anchor->AnchoredPawn == this)
	{
		Anchor->AnchoredPawn = NULL;
		Anchor->LastAnchoredPawnTime = GWorld->GetTimeSeconds();
	}

	Anchor = NewAnchor;
	if ( Anchor )
	{
		LastValidAnchorTime = GWorld->GetTimeSeconds();
		LastAnchor = Anchor;
		// set the anchor reference for path finding purposes
		// only for AI pawns as PlayerControllers using pathfinding is usually sporadic, so this might not get updated for a long time
		if (!IsHumanControlled())
		{
			Anchor->AnchoredPawn = this;
		}
	}
}

#if 0
#define SCOPE_QUICK_TIMER(TIMERNAME) \
	static DOUBLE AverageTime##TIMERNAME = -1.0; \
	static INT NumItts##TIMERNAME = 0; \
	static UObject* OuterThis;\
	OuterThis=this;\
class TimerClassName##TIMERNAME \
{ \
public:\
	TimerClassName##TIMERNAME() \
	{ \
		StartTimerTime = 0; \
		CLOCK_CYCLES(StartTimerTime);\
	} \
	\
	~TimerClassName##TIMERNAME() \
	{\
		UNCLOCK_CYCLES(StartTimerTime);\
		FLOAT Duration = (FLOAT)(StartTimerTime*GSecondsPerCycle);\
		if(AverageTime##TIMERNAME < 0.f)\
		{\
			AverageTime##TIMERNAME = Duration;\
		}\
		else\
		{\
			AverageTime##TIMERNAME += (Duration - AverageTime##TIMERNAME)/++NumItts##TIMERNAME;\
		}\
		\
		debugf(TEXT("(%s) Task %s took %.2f(ms) Avg %.2f(ms)"),(OuterThis)?*OuterThis->GetName():TEXT("NULL"),TEXT(#TIMERNAME),Duration*1000.f,AverageTime##TIMERNAME*1000.f);\
	}\
	DWORD StartTimerTime;\
};\
	TimerClassName##TIMERNAME TIMERNAME = TimerClassName##TIMERNAME();
#else
#define SCOPE_QUICK_TIMER(blah) {}
#endif

ANavigationPoint* APawn::GetBestAnchor( AActor* TestActor, FVector TestLocation, UBOOL bStartPoint, UBOOL bOnlyCheckVisible, FLOAT& out_Dist )
{
	//debugf(TEXT("----------------->> PHYS: %i"),Physics);
	SCOPE_QUICK_TIMER(GetBestAnchor)

	if (Controller != NULL)
	{
		return FindAnchor( TestActor, TestLocation, bStartPoint, bOnlyCheckVisible, out_Dist );
	}
	else
	{
		debugf(NAME_Warning, TEXT("GetBestAnchor(): No Controller! (%s)"),*GetName());
		return NULL;
	}
}

/** @see Pawn::SetRemoteViewPitch */
void APawn::SetRemoteViewPitch(INT NewRemoteViewPitch)
{
	RemoteViewPitch = (NewRemoteViewPitch & 65535) >> 8;
}

/* PlayerCanSeeMe returns true if any player (server) or the local player (standalone
* or client) has a line of sight to actor's location.
* @param bForceLOSCheck (opt) - If set, force a line of sight check instead of relying on the occlusion checks
*/
UBOOL AActor::PlayerCanSeeMe(UBOOL bForceLOSCheck)
{
	if (!bForceLOSCheck && (WorldInfo->NetMode == NM_Standalone || WorldInfo->NetMode == NM_Client || 
							(WorldInfo->NetMode == NM_ListenServer && (bTearOff || (WorldInfo->Game && (WorldInfo->Game->NumPlayers + WorldInfo->Game->NumSpectators < 2)))) ))
	{
		// just check local player visibility
		return (WorldInfo->TimeSeconds - LastRenderTime < 1.f);
	}
	else
	{
		for (AController *next = GWorld->GetFirstController(); next != NULL; next = next->NextController)
		{
			if (TestCanSeeMe(next->GetAPlayerController()))
			{
				return TRUE;
			}
		}

		return FALSE;
	}
}

INT AActor::TestCanSeeMe( APlayerController *Viewer )
{
	if ( !Viewer )
		return 0;
	if ( Viewer->GetViewTarget() == this )
		return 1;

	FLOAT distSq = (Location - Viewer->ViewTarget->Location).SizeSquared();

	FLOAT CollisionRadius, CollisionHeight;
	GetBoundingCylinder(CollisionRadius, CollisionHeight);

	return ( (distSq < 100000.f * ( Max(CollisionRadius, CollisionHeight) + 3.6 ))
		&& ( Viewer->PlayerCamera != NULL
			|| (Square(Viewer->Rotation.Vector() | (Location - Viewer->ViewTarget->Location)) >= 0.25f * distSq) )
		&& Viewer->LineOfSightTo(this) );

}

/*-----------------------------------------------------------------------------
	Pawn related functions.
-----------------------------------------------------------------------------*/

void APawn::ForceCrouch()
{
	Crouch(FALSE);
}

/*MakeNoise
- check to see if other creatures can hear this noise
*/
void AActor::MakeNoise( FLOAT Loudness, FName NoiseType )
{
	if ( (GWorld->GetNetMode() != NM_Client) && Instigator )
	{
		Instigator->CheckNoiseHearing( this, Loudness, NoiseType );
	}
}

/* Dampen loudness of a noise instigated by this pawn
@PARAM NoiseMaker	is the actual actor who made the noise
@PARAM Loudness		is the loudness passed in (1.0 is typical value)
@PARAM NoiseType	is additional optional information for game specific implementations
@RETURN		dampening factor
*/
FLOAT APawn::DampenNoise( AActor* NoiseMaker, FLOAT Loudness, FName NoiseType )
{
	FLOAT DampeningFactor = 1.f;
	if ( (NoiseMaker == this) ||(NoiseMaker->Owner == this) )
		DampeningFactor *= Instigator->SoundDampening;

	return DampeningFactor;
}


/* Send a HearNoise() message instigated by this pawn to all Controllers which could possibly hear this noise
@PARAM NoiseMaker	is the actual actor who made the noise
@PARAM Loudness		is the loudness passed in (1.0 is typical value)
@PARAM NoiseType	is additional optional information for game specific implementations
*/
void APawn::CheckNoiseHearing( AActor* NoiseMaker, FLOAT Loudness, FName NoiseType )
{
	// only hear sounds from pawns with controllers, and not when playersonly
	if ( !Controller || WorldInfo->bPlayersOnly )
	{
		return;
	}

	Loudness *= DampenNoise(NoiseMaker, Loudness, NoiseType);

	FLOAT CurrentTime = WorldInfo->TimeSeconds;

	// allow only one noise per MAXSOUNDOVERLAPTIME seconds from a given instigator & area (within PROXIMATESOUNDTHRESHOLD units) unless much louder
	// check the two sound slots
	if ( (noise1time > CurrentTime - MAXSOUNDOVERLAPTIME)
		 && ((noise1spot - NoiseMaker->Location).SizeSquared() < PROXIMATESOUNDTHRESHOLDSQUARED)
		 && (noise1loudness >= 0.9f * Loudness) )
	{
		return;
	}

	if ( (noise2time > CurrentTime - MAXSOUNDOVERLAPTIME)
		 && ((noise2spot - NoiseMaker->Location).SizeSquared() < PROXIMATESOUNDTHRESHOLDSQUARED)
		 && (noise2loudness >= 0.9f * Loudness) )
	{
		return;
	}

	// put this noise in a slot
	if ( noise1time < CurrentTime - MINSOUNDOVERLAPTIME )
	{
		noise1time = CurrentTime;
		noise1spot = NoiseMaker->Location;
		noise1loudness = Loudness;
	}
	else if ( noise2time < CurrentTime - MINSOUNDOVERLAPTIME )
	{
		noise2time = CurrentTime;
		noise2spot = NoiseMaker->Location;
		noise2loudness = Loudness;
	}
	else if ( ((noise1spot - NoiseMaker->Location).SizeSquared() < PROXIMATESOUNDTHRESHOLDSQUARED)
			  && (noise1loudness <= Loudness) )
	{
		noise1time = CurrentTime;
		noise1spot = NoiseMaker->Location;
		noise1loudness = Loudness;
	}
	else if ( noise2loudness <= Loudness )
	{
		noise1time = CurrentTime;
		noise1spot = NoiseMaker->Location;
		noise1loudness = Loudness;
	}
//debugf(TEXT("Hear noise %s %f at %f"), *NoiseMaker->GetName(), Loudness, WorldInfo->TimeSeconds);
	// all controllers can hear this noise
	for ( AController *C=GWorld->GetFirstController(); C!=NULL; C=C->NextController )
	{
		if ( C->Pawn && C->Pawn != this )
		{
			C->HearNoise( NoiseMaker, Loudness, NoiseType);
		}
	}
}

FVector APawn::AdjustDestination(AActor* GoalActor, FVector Dest)
{
	if (GoalActor != NULL && CylinderComponent != NULL)
	{
		FLOAT MyHeight = CylinderComponent->CollisionHeight;
		// use as destination Pawn's height from the bottom of the object
		// prefer using cylinder if available
		UCylinderComponent* GoalCyl = Cast<UCylinderComponent>(GoalActor->CollisionComponent);
		if (GoalCyl != NULL)
		{
			return -FVector(0.f, 0.f, GoalCyl->CollisionHeight - MyHeight);
		}
		// otherwise use center of bounds of all colliding components
		// if there is no collision, than the bottom cannot be determined, so just leave Dest as the Location of GoalActor
		else if (GoalActor->bCollideActors)
		{
			FBox BoundingBox = GoalActor->GetComponentsBoundingBox(FALSE);
			if (BoundingBox.IsValid)
			{
				FVector NewDest = BoundingBox.GetCenter();
				NewDest.Z = BoundingBox.Min.Z + MyHeight;
				return NewDest - Dest;
			}
			else
			{
				return FVector(0.f);
			}
		}
		else
		{
			return FVector(0.f);
		}
	}
	else
	{
		return FVector(0.f);
	}
}

//=================================================================================
void APawn::setMoveTimer(FVector MoveDir)
{
	if ( !Controller )
		return;

	if ( DesiredSpeed == 0.f )
		Controller->MoveTimer = 0.5f;
	else
	{
		FLOAT Extra = 2.f;
		if ( bIsCrouched )
			Extra = ::Max(Extra, 1.f/CrouchedPct);
		else if ( bIsWalking )
			Extra = ::Max(Extra, 1.f/WalkingPct);
		FLOAT MoveSize = MoveDir.Size();
		Controller->MoveTimer = 0.5f + Extra * MoveSize/(DesiredSpeed * 0.6f * GetMaxSpeed());
	}
	if ( Controller->bPreparingMove && Controller->PendingMover )
		Controller->MoveTimer += 2.f;
}

FLOAT APawn::GetMaxSpeed()
{
	if (Physics == PHYS_Flying)
		return AirSpeed;
	else if (Physics == PHYS_Swimming)
		return WaterSpeed;
	return GroundSpeed;
}

/* StartNewSerpentine()
pawn is using serpentine motion while moving to avoid being hit (while staying within the reachspec
its using.  At this point change direction (either reverse, or go straight for a while)
*/
void APawn::StartNewSerpentine(const FVector& Dir, const FVector& Start)
{
	FVector NewDir(Dir.Y, -1.f * Dir.X, Dir.Z);
	if ( (NewDir | (Location - Start)) > 0.f )
	{
		NewDir *= -1.f;
	}
	SerpentineDir = NewDir;

	if (!Controller->bAdvancedTactics || Controller->bUsingPathLanes)
	{
		ClearSerpentine();
	}
	else if (appFrand() < 0.2f)
	{
		SerpentineTime = 0.1f + 0.4f * appFrand();
	}
	else
	{
		SerpentineTime = 0.f;

		FLOAT ForcedStrafe = ::Min(1.f, 4.f * CylinderComponent->CollisionRadius/Controller->CurrentPath->CollisionRadius);
		SerpentineDist = (ForcedStrafe + (1.f - ForcedStrafe) * appFrand());
		SerpentineDist *= (Controller->CurrentPath->CollisionRadius - CylinderComponent->CollisionRadius);
	}
}

/* ClearSerpentine()
completely clear all serpentine related attributes
*/
void APawn::ClearSerpentine()
{
	SerpentineTime = 999.f;
	SerpentineDist = 0.f;
}

/* moveToward()
move Actor toward a point.  Returns 1 if Actor reached point
(Set Acceleration, let physics do actual move)
*/
UBOOL APawn::moveToward(const FVector &Dest, AActor *GoalActor )
{
	if ( !Controller )
		return FALSE;

	if ( Controller->bAdjusting )
	{
		GoalActor = NULL;
	}
	FVector Direction = Dest - Location;
	FLOAT ZDiff = Direction.Z;

	if( Physics == PHYS_Walking )
	{
		Direction.Z = 0.f;
	}
	else if (Physics == PHYS_Falling)
	{
		// use air control if low grav or above destination and falling towards it
		if (Velocity.Z < 0.f && (ZDiff < 0.f || GetGravityZ() > 0.9f * GWorld->GetDefaultGravityZ()))
		{
			if ( ZDiff > 0.f )
			{
				if ( ZDiff > 2.f * MaxJumpHeight )
				{
					Controller->FailMove();
					Controller->eventNotifyMissedJump();
				}
			}
			else
			{
				if ( (Velocity.X == 0.f) && (Velocity.Y == 0.f) )
					Acceleration = FVector(0.f,0.f,0.f);
				else
				{
					FLOAT Dist2D = Direction.Size2D();
					Direction.Z = 0.f;
					Acceleration = Direction;
					Acceleration = Acceleration.SafeNormal();
					Acceleration *= AccelRate;
					if ( (Dist2D < 0.5f * Abs(Direction.Z)) && ((Velocity | Direction) > 0.5f*Dist2D*Dist2D) )
						Acceleration *= -1.f;

					if ( Dist2D < 1.5f*CylinderComponent->CollisionRadius )
					{
						Velocity.X = 0.f;
						Velocity.Y = 0.f;
						Acceleration = FVector(0.f,0.f,0.f);
					}
					else if ( (Velocity | Direction) < 0.f )
					{
						FLOAT M = ::Max(0.f, 0.2f - AvgPhysicsTime);
						Velocity.X *= M;
						Velocity.Y *= M;
					}
				}
			}
		}
		return FALSE; // don't end move until have landed
	}
	else if ( (Physics == PHYS_Ladder) && OnLadder )
	{
		if ( ReachedDestination(Location, Dest, GoalActor) )
		{
			Acceleration = FVector(0.f,0.f,0.f);

			// if Pawn just reached a navigation point, set a new anchor
			ANavigationPoint *Nav = Cast<ANavigationPoint>(GoalActor);
			if ( Nav )
				SetAnchor(Nav);
			return TRUE;
		}
		Acceleration = Direction.SafeNormal();
		if ( GoalActor && (OnLadder != GoalActor->PhysicsVolume)
			&& ((Acceleration | (OnLadder->ClimbDir + OnLadder->LookDir)) > 0.f)
			&& (GoalActor->Location.Z < Location.Z) )
			setPhysics(PHYS_Falling);
		Acceleration *= LadderSpeed;
		return FALSE;
	}
	if ( Controller->MoveTarget && Controller->MoveTarget->IsA(APickupFactory::StaticClass()) 
		 && (Abs(Location.Z - Controller->MoveTarget->Location.Z) < CylinderComponent->CollisionHeight)
		 && (Square(Location.X - Controller->MoveTarget->Location.X) + Square(Location.Y - Controller->MoveTarget->Location.Y) < Square(CylinderComponent->CollisionRadius)) )
	{
		 Controller->MoveTarget->eventTouch(this, this->CollisionComponent, Location, (Controller->MoveTarget->Location - Location) );
	}
	
	FLOAT Distance = Direction.Size();
	const UBOOL bGlider = IsGlider();
	FCheckResult Hit(1.f);

	if ( ReachedDestination(Location, Dest, GoalActor, TRUE) )
	{
		if ( !bGlider )
		{
			Acceleration = FVector(0.f,0.f,0.f);
		}

		// if Pawn just reached a navigation point, set a new anchor
		ANavigationPoint *Nav = Cast<ANavigationPoint>(GoalActor);
		if ( Nav )
			SetAnchor(Nav);
		return TRUE;
	}
	else 
	// if walking, and within radius, and goal is null or
	// the vertical distance is greater than collision + step height and trace hit something to our destination
	if (Physics == PHYS_Walking 
		&& Distance < (CylinderComponent->CollisionRadius+DestinationOffset) &&
		(GoalActor == NULL ||
		 (ZDiff > CylinderComponent->CollisionHeight + 2.f * MaxStepHeight && 
		  !GWorld->SingleLineCheck(Hit, this, Dest, Location, TRACE_World))))
	{
		Controller->eventMoveUnreachable(Dest,GoalActor);
		return TRUE;
	}
	else if ( bGlider )
	{
		Direction = Rotation.Vector();
	}
	else if ( Distance > 0.f )
	{
		Direction = Direction/Distance;
		if (Controller != NULL && Controller->CurrentPath != NULL && Controller->CurrentPath->Start != NULL)
		{
			HandleSerpentineMovement(Direction, Distance, Dest);
		}
	}

	Acceleration = Direction * AccelRate;

	if ( !Controller->bAdjusting && Controller->MoveTarget && Controller->MoveTarget->GetAPawn() )
	{
		return (Distance < CylinderComponent->CollisionRadius + Controller->MoveTarget->GetAPawn()->CylinderComponent->CollisionRadius + 0.8f * MeleeRange);
	}

	FLOAT speed = Velocity.Size();

	if ( !bGlider && (speed > FASTWALKSPEED) )
	{
//		FVector VelDir = Velocity/speed;
//		Acceleration -= 0.2f * (1 - (Direction | VelDir)) * speed * (VelDir - Direction);
	}
	if ( Distance < 1.4f * AvgPhysicsTime * speed )
	{
		// slow pawn as it nears its destination to prevent overshooting
		if ( !bReducedSpeed ) 
		{
			//haven't reduced speed yet
			DesiredSpeed = 0.51f * DesiredSpeed;
			bReducedSpeed = 1;
		}
		if ( speed > 0.f )
			DesiredSpeed = Min(DesiredSpeed, (2.f*FASTWALKSPEED)/speed);
		if ( bGlider )
			return TRUE;
	}
	return FALSE;
}

void APawn::InitSerpentine()
{
	if( Controller->CurrentPath != NULL )
	{
		// round corners smoothly
		// start serpentine dir in current direction
		SerpentineTime = 0.f;
		SerpentineDir = Velocity.SafeNormal();
		SerpentineDist = Clamp(Controller->CurrentPath->CollisionRadius - CylinderComponent->CollisionRadius,0.f,4.f * CylinderComponent->CollisionRadius)
								* (0.5f + 1.f * appFrand());
		FLOAT DP = Controller->CurrentPathDir | SerpentineDir;
		FLOAT DistModifier = 1.f - DP*DP*DP*DP;
		if( (DP < 0) && (DistModifier < 0.5f) )
		{
			SerpentineTime = 0.8f;
		}
		else
		{
			SerpentineDist *= DistModifier;
		}
	}
}

void APawn::HandleSerpentineMovement(FVector& out_Direction, FLOAT Distance, const FVector& Dest)
{
	if( SerpentineTime > 0.f )
	{
		SerpentineTime -= AvgPhysicsTime;
		if( SerpentineTime <= 0.f )
		{
			StartNewSerpentine( Controller->CurrentPathDir, Controller->CurrentPath->Start->Location );
		}
		else if( SerpentineDist > 0.f )
		{
			if( Distance < 2.f * SerpentineDist )
			{
				ClearSerpentine();
			}
			else
			{
				// keep within SerpentineDist units of both the ReachSpec we're using and the line to our destination point
				const FVector& Start = Controller->CurrentPath->Start->Location;
				FVector PathLineDir = Location - (Start + (Controller->CurrentPathDir | (Location - Start)) * Controller->CurrentPathDir);
				FVector DestLine = (Dest - Start).SafeNormal();
				FVector DestLineDir = Location - (Start + (DestLine | (Location - Start)) * DestLine);
				if ( PathLineDir.SizeSquared() >= SerpentineDist * SerpentineDist && (PathLineDir.SafeNormal() | SerpentineDir) > 0.f ||
					 DestLineDir.SizeSquared() >= SerpentineDist * SerpentineDist && (DestLineDir.SafeNormal() | SerpentineDir) > 0.f )
				{
					out_Direction = (Dest - Location + SerpentineDir*SerpentineDist).SafeNormal();
				}
				else
				{
					out_Direction = (out_Direction + 0.2f * SerpentineDir).SafeNormal();
				}
			}
		}
	}
	if( SerpentineTime <= 0.f )
	{
		if( Distance < 2.f * SerpentineDist )
		{
			ClearSerpentine();
		}
		else
		{
			// keep within SerpentineDist units of both the ReachSpec we're using and the line to our destination point
			const FVector& Start = Controller->CurrentPath->Start->Location;
			FVector PathLineDir = Location - (Start + (Controller->CurrentPathDir | (Location - Start)) * Controller->CurrentPathDir);
			FVector DestLine = (Dest - Start).SafeNormal();
			FVector DestLineDir = Location - (Start + (DestLine | (Location - Start)) * DestLine);
			if ( PathLineDir.SizeSquared() >= SerpentineDist * SerpentineDist && (PathLineDir.SafeNormal() | SerpentineDir) > 0.f ||
				 DestLineDir.SizeSquared() >= SerpentineDist * SerpentineDist && (DestLineDir.SafeNormal() | SerpentineDir) > 0.f )
			{
				StartNewSerpentine(Controller->CurrentPathDir,Start);
			}
			else
			{
				out_Direction = (out_Direction + SerpentineDir).SafeNormal();
			}
		}
	}
}

UBOOL APawn::IsGlider()
{
	return !bCanStrafe && (Physics == PHYS_Flying || Physics == PHYS_Swimming);
}

/* rotateToward()
rotate Actor toward a point.  Returns 1 if target rotation achieved.
(Set DesiredRotation, let physics do actual move)
*/
void APawn::rotateToward(FVector FocalPoint)
{
	if( bRollToDesired || Physics == PHYS_Spider )
	{
		return;
	}

	if( IsGlider() )
	{
		Acceleration = Rotation.Vector() * AccelRate;
	}

	FVector Direction = FocalPoint - Location;
	if ( (Physics == PHYS_Flying) 
		&& Controller && Controller->MoveTarget && (Controller->MoveTarget != Controller->Focus) )
	{
		FVector MoveDir = (Controller->MoveTarget->Location - Location);
		FLOAT Dist = MoveDir.Size();
		if ( Dist < MAXPATHDIST )
		{
			Direction = Direction/Dist;
			MoveDir = MoveDir.SafeNormal();
			if ( (Direction | MoveDir) < 0.9f )
			{
				Direction = MoveDir;
				Controller->Focus = Controller->MoveTarget;
			}
		}
	}

	// Rotate toward destination
	// Do not overwrite if DesiredRotation is set by somebody else
	if ( !bDesiredRotationSet )
	{
		// if we're walking on the navmesh calculate a rotation such that our base is on the mesh
		UNavigationHandle* Handle = (Controller != NULL) ? Controller->NavigationHandle : NULL;
		if(Physics == PHYS_NavMeshWalking && Handle != NULL && Handle->AnchorPoly != NULL)
		{
			FVector Up = Handle->AnchorPoly->GetPolyNormal();
			
			FMatrix OrientMat = FMatrix::Identity; 
			
			FVector DirNorm = Direction.SafeNormal();
			OrientMat.SetAxis(0,DirNorm);
			OrientMat.SetAxis(1,(Up^DirNorm).SafeNormal());
			OrientMat.SetAxis(2,Up);

			DesiredRotation = OrientMat.Rotator();
			DesiredRotation = DesiredRotation.GetNormalized();
		}
		else
		{
			DesiredRotation = Direction.Rotation();

			if ( (Physics == PHYS_Walking) && (!Controller || !Controller->MoveTarget || !Controller->MoveTarget->GetAPawn()) )
			{
				DesiredRotation.Pitch = 0;
			}
		}

		DesiredRotation.Yaw = DesiredRotation.Yaw & 65535;
	}
}

UBOOL APhysicsVolume::WillHurt(APawn *P)
{
	if ( !bPainCausing || (DamagePerSec <= 0) || bAIShouldIgnorePain )
		return FALSE;

	return P->HurtByDamageType(DamageType);
}

UBOOL APawn::HurtByDamageType(class UClass* DamageType)
{
	return TRUE;
}

INT APawn::actorReachable(AActor *Other, UBOOL bKnowVisible, UBOOL bNoAnchorCheck)
{
	SCOPE_CYCLE_COUNTER(STAT_PathFinding_Reachable);
	TRACK_DETAILED_PATHFINDING_STATS(this);
	if ( !Other || Other->bDeleteMe )
		return 0;

	if ( (Other->Physics == PHYS_Flying) && !bCanFly )
		return 0;

	if ( bCanFly && ValidAnchor() && Cast<AVolumePathNode>(Anchor) )
	{
		FVector Dir = Other->Location - Anchor->Location;
		if ( Abs(Dir.Z) < Anchor->CylinderComponent->CollisionHeight )
		{
			Dir.Z = 0.f;
			if ( Dir.SizeSquared() < Anchor->CylinderComponent->CollisionRadius * Anchor->CylinderComponent->CollisionRadius )
				return TRUE;
		}
	}

	// If goal is on the navigation network, check if it will give me reachability
	ANavigationPoint *Nav = Cast<ANavigationPoint>(Other);
	if (Nav != NULL)
	{
		if (!bNoAnchorCheck)
		{
			if (ReachedDestination(Location, Other->Location,Nav))
			{
				SetAnchor( Nav );
				return 1;
			}
			else if( ValidAnchor() )
			{
				UReachSpec* Path = Anchor->GetReachSpecTo(Nav);
				if (Path == NULL)
				{
					// if it's a replacement PickupFactory, try redirecting to original
					APickupFactory* Pickup = Nav->GetAPickupFactory();
					if (Pickup != NULL)
					{
						while (Pickup->OriginalFactory != NULL && Path == NULL)
						{
							Path = Anchor->GetReachSpecTo(Pickup->OriginalFactory);
							Pickup = Pickup->OriginalFactory;
						}
					}
				}
				return ( Path != NULL && 
						!Path->End.Nav()->IsProbing(NAME_SpecialHandling) && // nodes requiring special handling might require extra pathfinding so force code through that path
						Path->supports(appTrunc(CylinderComponent->CollisionRadius), appTrunc(CylinderComponent->CollisionHeight), calcMoveFlags(), appTrunc(GetAIMaxFallSpeed())) && 
						Path->CostFor(this) < UCONST_BLOCKEDPATHCOST );
			}
			else if (Nav->bBlocked || (Nav->bBlockedForVehicles && bPathfindsAsVehicle))
			{
				return FALSE;
			}
		}
	}
	else if (!bNoAnchorCheck && ValidAnchor())
	{
		// find out if a path connected to our Anchor intersects with the Actor we want to reach
		FBox ActorBox = Other->GetComponentsBoundingBox(FALSE);
		INT Radius = appTrunc(CylinderComponent->CollisionRadius);
		INT Height = appTrunc(CylinderComponent->CollisionHeight);
		INT MoveFlags = calcMoveFlags();
		INT MaxFallSpeedInt = appTrunc(GetAIMaxFallSpeed());
		for (INT i = 0; i < Anchor->PathList.Num(); i++)
		{
			UReachSpec* Path = Anchor->PathList(i);
			// if the path is in the navigation octree, is usable by this pawn, and intersects with Other, then Other is reachable
			if(  Path != NULL && 
				 Path->NavOctreeObject != NULL && 
				 *Path->End != NULL &&
				!Path->End.Nav()->bSpecialMove &&	// Don't do for special move paths, it breaks special movement code
				 Path->supports(Radius, Height, MoveFlags, MaxFallSpeedInt) && 
			 	 Path->CostFor(this) < UCONST_BLOCKEDPATHCOST &&
				 Path->NavOctreeObject->BoundingBox.Intersect(ActorBox) && 
				 !Path->NavigationOverlapCheck(ActorBox) )
			{
				return TRUE;
			}
		}
	}

	FVector Dir = Other->Location - Location;

	if ( GWorld->HasBegunPlay() )
	{
		// prevent long reach tests during gameplay as they are expensive
		// Use the navigation network if more than MAXPATHDIST units to goal
		if (Dir.SizeSquared2D() > MAXPATHDISTSQ)
		{
			return 0;
		}
		if ( Other->PhysicsVolume )
		{
			if ( Other->PhysicsVolume->bWaterVolume )
			{
				if ( !bCanSwim )
					return 0;
			}
			else if ( !bCanFly && !bCanWalk )
				return 0;
			if ( Other->PhysicsVolume->WillHurt(this) )
				return 0;
		}
	}

	FVector aPoint = Other->GetDestination(Controller); //adjust destination

	//check other visible
	if ( !bKnowVisible )
	{
		FCheckResult Hit(1.f);
		FVector	ViewPoint = Location;
		ViewPoint.Z += BaseEyeHeight; //look from eyes
		GWorld->SingleLineCheck(Hit, this, aPoint, ViewPoint, TRACE_World|TRACE_StopAtAnyHit);
		if( Hit.Time!=1.f && Hit.Actor!=Other )
			return 0;
	}

	if ( Other->GetAPawn() )
	{
		APawn* OtherPawn = (APawn*)Other;

		FLOAT Threshold = CylinderComponent->CollisionRadius + ::Min(1.5f * CylinderComponent->CollisionRadius, MeleeRange) + OtherPawn->CylinderComponent->CollisionRadius;
		if (Dir.SizeSquared() <= Square(Threshold))
			return 1;
	}
	FVector realLoc = Location;
	if ( Other->Physics == PHYS_Falling )
	{
		// check if ground below it
		FCheckResult Hit(1.f);
		GWorld->SingleLineCheck(Hit, this, Other->Location - FVector(0.f,0.f,4.f*SHORTTRACETESTDIST), Other->Location, TRACE_World);
		if ( Hit.Time == 1.f )
			return FALSE;
		aPoint = Hit.Location + FVector(0.f,0.f,CylinderComponent->CollisionRadius + MaxStepHeight);
		if ( GWorld->FarMoveActor(this, aPoint, 1) )
		{
			aPoint = Location;
			GWorld->FarMoveActor(this, realLoc,1,1);
			FVector	ViewPoint = Location;
			ViewPoint.Z += BaseEyeHeight; //look from eyes
			GWorld->SingleLineCheck(Hit, this, aPoint, ViewPoint, TRACE_World);
			if( Hit.Time!=1.f && Hit.Actor!=Other )
				return 0;
		}
		else
			return 0;
	}
	else
	{
		FLOAT OtherRadius, OtherHeight;
		Other->GetBoundingCylinder(OtherRadius, OtherHeight);

		if ( ((CylinderComponent->CollisionRadius > OtherRadius) || (CylinderComponent->CollisionHeight > OtherHeight))
			&& GWorld->FarMoveActor(this, aPoint, 1) )
		{
			aPoint = Location;
			GWorld->FarMoveActor(this, realLoc,1,1);
		}
	}
	return Reachable(aPoint, Other);
}

INT APawn::pointReachable(FVector aPoint, INT bKnowVisible)
{
	SCOPE_CYCLE_COUNTER(STAT_PathFinding_Reachable);
	if (GWorld->HasBegunPlay())
	{
		FVector Dir2D = aPoint - Location;
		if (Dir2D.SizeSquared2D() > MAXPATHDISTSQ)
			return 0;
	}

	//check aPoint visible
	if ( !bKnowVisible )
	{
		FVector	ViewPoint = Location;
		ViewPoint.Z += BaseEyeHeight; //look from eyes
		FCheckResult Hit(1.f);
		GWorld->SingleLineCheck( Hit, this, aPoint, ViewPoint, TRACE_World|TRACE_StopAtAnyHit );
		if ( Hit.Actor )
			return 0;
	}

	FVector realLoc = Location;
	if ( GWorld->FarMoveActor(this, aPoint, 1) )
	{
		aPoint = Location; //adjust destination
		GWorld->FarMoveActor(this, realLoc,1,1);
	}
	return Reachable(aPoint, NULL);

}

/**
 * Pawn crouches.
 * Checks if new cylinder size fits (no encroachment), and trigger Pawn->eventStartCrouch() in script if succesful.
 *
 * @param	bClientSimulation	TRUE when called when bIsCrouched is replicated to non owned clients, to update collision cylinder and offset.
 */
void APawn::Crouch(INT bClientSimulation)
{
	// Do not perform if collision is already at desired size.
	if( (CylinderComponent->CollisionHeight == CrouchHeight) && (CylinderComponent->CollisionRadius == CrouchRadius) )
	{
		return;
	}

	// Change collision size to crouching dimensions
	FLOAT OldHeight = CylinderComponent->CollisionHeight;
	FLOAT OldRadius = CylinderComponent->CollisionRadius;
	SetCollisionSize(CrouchRadius, CrouchHeight);
	FLOAT HeightAdjust	= OldHeight - CrouchHeight;

	if( !bClientSimulation )
	{
		if ( (CrouchRadius > OldRadius) || (CrouchHeight > OldHeight) )
		{
			FMemMark Mark(GMainThreadMemStack);
			FCheckResult* FirstHit = GWorld->Hash->ActorEncroachmentCheck
				(	GMainThreadMemStack,
					this, 
					Location - FVector(0,0,HeightAdjust), 
					Rotation, 
					TRACE_Pawns | TRACE_Movers | TRACE_Others 
				);

			UBOOL bEncroached	= FALSE;
			for( FCheckResult* Test = FirstHit; Test!=NULL; Test=Test->GetNext() )
			{
				if ( (Test->Actor != this) && IsBlockedBy(Test->Actor,Test->Component) )
				{
					bEncroached = TRUE;
					break;
				}
			}
			Mark.Pop();
			// If encroached, cancel

			if( bEncroached )
			{
				SetCollisionSize(OldRadius, OldHeight);
				return;
			}
		}

		bNetDirty = TRUE;	// bIsCrouched replication controlled by bNetDirty
		bIsCrouched = TRUE;
	}
	bForceFloorCheck = TRUE;
	eventStartCrouch( HeightAdjust );

}

/**
 * Pawn uncrouches.
 * Checks if new cylinder size fits (no encroachment), and trigger Pawn->eventEndCrouch() in script if succesful.
 *
 * @param	bClientSimulation	TRUE when called when bIsCrouched is replicated to non owned clients, to update collision cylinder and offset.
 */
void APawn::UnCrouch(INT bClientSimulation)
{
	// see if space to uncrouch
	FCheckResult Hit(1.f);
	APawn* DefaultPawn = Cast<APawn>(GetClass()->GetDefaultObject());

	FLOAT	HeightAdjust	= DefaultPawn->CylinderComponent->CollisionHeight - CylinderComponent->CollisionHeight;
	FVector	NewLoc = Location + FVector(0.f,0.f,HeightAdjust);

	UBOOL bEncroached = FALSE;

	// change cylinder directly rather than calling setcollisionsize(), since we don't want to cause touch/untouch notifications unless uncrouch succeeds
	check(CylinderComponent);
	CylinderComponent->SetCylinderSize(DefaultPawn->CylinderComponent->CollisionRadius, DefaultPawn->CylinderComponent->CollisionHeight);
	CylinderComponent->UpdateBounds(); // Force an update of the bounds with the new dimensions

	if( !bClientSimulation )
	{
		AActor* OldBase = Base;
		FVector OldFloor = Floor;
		SetBase(NULL,OldFloor,0);
		FMemMark Mark(GMainThreadMemStack);
		FCheckResult* FirstHit = GWorld->Hash->ActorEncroachmentCheck
			(	GMainThreadMemStack, 
				this, 
				NewLoc, 
				Rotation, 
				TRACE_Pawns | TRACE_Movers | TRACE_Others 
			);

		for( FCheckResult* Test = FirstHit; Test!=NULL; Test=Test->GetNext() )
		{
			if ( (Test->Actor != this) && IsBlockedBy(Test->Actor,Test->Component) )
			{
				bEncroached = TRUE;
				break;
			}
		}
		Mark.Pop();
		// Attempt to move to the adjusted location
		if ( !bEncroached && !GWorld->FarMoveActor(this, NewLoc, 0, FALSE, TRUE) )
		{
			bEncroached = TRUE;
		}

		// if encroached  then abort.
		if( bEncroached )
		{
			CylinderComponent->SetCylinderSize(CrouchRadius, CrouchHeight);
			CylinderComponent->UpdateBounds(); // Update bounds again back to old value
			SetBase(OldBase,OldFloor,0);
			return;
		}
	}	

	// now call setcollisionsize() to cause touch/untouch events
	SetCollisionSize(DefaultPawn->CylinderComponent->CollisionRadius, DefaultPawn->CylinderComponent->CollisionHeight);

	// space enough to uncrouch, so stand up
	if( !bClientSimulation )
	{
		bNetDirty = TRUE;			// bIsCrouched replication controlled by bNetDirty
		bIsCrouched = FALSE;
	}
	bForceFloorCheck = TRUE;
	eventEndCrouch( HeightAdjust );
}

INT APawn::Reachable(FVector aPoint, AActor* GoalActor)
{
	SCOPE_CYCLE_COUNTER(STAT_PathFinding_Reachable);
	TRACK_DETAILED_PATHFINDING_STATS(this);
	
	INT Result = 0;

	if(PhysicsVolume == NULL)
	{
		warnf(TEXT("WARNING! NULL PhysicsVolume in APawn::Reachable for %s"),*GetName());
		return 0;
	}
	if( PhysicsVolume->bWaterVolume )
	{
		Result = swimReachable( aPoint, Location, 0, GoalActor );
	}
	else if( PhysicsVolume->IsA(ALadderVolume::StaticClass()) )
	{
		Result = ladderReachable(aPoint, Location, 0, GoalActor);
	}
	else if( (Physics == PHYS_Walking)	|| 
			 (Physics == PHYS_Swimming) || 
			 (Physics == PHYS_Ladder)	|| 
			 (Physics == PHYS_Falling)	)
	{
		Result = walkReachable( aPoint, Location, 0, GoalActor );
	}
	else if( Physics == PHYS_Flying )
	{
		Result = flyReachable( aPoint, Location, 0, GoalActor );
	}
	else if( Physics == PHYS_Spider )
	{
		Result = spiderReachable( aPoint, Location, 0, GoalActor );
	}
	else
	{
		FCheckResult Hit(1.f);
		FVector Slice = GetDefaultCollisionSize();
		Slice.Z = 1.f;
		FVector Dest = aPoint + Slice.X * (Location - aPoint).SafeNormal();
		if (GWorld->SingleLineCheck(Hit, this, Dest, Location, TRACE_World | TRACE_StopAtAnyHit, Slice))
		{
			if (bCanFly)
			{
				Result = TRUE;
			}
			else
			{
				// if not a flyer, trace down and make sure there's a valid landing
				FLOAT Down = CylinderComponent->CollisionHeight;
				if (GoalActor != NULL)
				{
					FLOAT Radius, Height;
					GoalActor->GetBoundingCylinder(Radius, Height);
					Down += Height;
				}
				Result = (!GWorld->SingleLineCheck(Hit, this, Dest - FVector(0.f, 0.f, Down), Dest, TRACE_World | TRACE_StopAtAnyHit, Slice) && Hit.Normal.Z >= WalkableFloorZ);
			}
		}
		else
		{
			Result = FALSE;
		}
	}

	return Result;
}

UBOOL APawn::ReachedBy(APawn *P, const FVector &TestPosition, const FVector& Dest)
{
	// get the pawn's normal height (might be crouching or a Scout, so use the max of current/default)
	FLOAT PawnHeight = Max<FLOAT>(P->CylinderComponent->CollisionHeight, ((APawn *)(P->GetClass()->GetDefaultObject()))->CylinderComponent->CollisionHeight);
	FLOAT UpThresholdAdjust = ::Max(0.f, CylinderComponent->CollisionHeight - PawnHeight);
	if ( Physics == PHYS_Falling )
		UpThresholdAdjust += 2.f * P->MaxJumpHeight;

	return P->ReachThresholdTest(TestPosition, Dest, this, 
		UpThresholdAdjust, 
		::Max(0.f,CylinderComponent->CollisionHeight-P->CylinderComponent->CollisionHeight), 
		::Min(1.5f * P->CylinderComponent->CollisionRadius, P->MeleeRange) + CylinderComponent->CollisionRadius);	
}

UBOOL AVehicle::ReachedBy(APawn *P, const FVector &TestPosition, const FVector& Dest)
{
	// if enemy vehicle, use normal pawn check
	if (!bCollideActors || (P->Controller != NULL && P->Controller->Enemy == this))
	{
		return Super::ReachedBy(P, TestPosition, Dest);
	}

	FRadiusOverlapCheck CheckInfo(TestPosition, P->VehicleCheckRadius);
	for (INT i = 0; i < Components.Num(); i++)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Components(i));

		// Only use collidable components to find collision bounding box.
		if (Primitive != NULL && Primitive->IsAttached() && Primitive->CollideActors && CheckInfo.SphereBoundsTest(Primitive->Bounds))
		{
			return TRUE;
		}
	}

	return FALSE;
}

UBOOL AActor::ReachedBy(APawn *P, const FVector &TestPosition, const FVector& Dest)
{
	if ( TouchReachSucceeded(P, TestPosition) )
		return TRUE;

	FLOAT ColHeight, ColRadius;
	GetBoundingCylinder(ColRadius, ColHeight);
	if ( !bBlockActors && GWorld->HasBegunPlay() )
		ColRadius = 0.f;
	return P->ReachThresholdTest(TestPosition, Dest, this, ColHeight, ColHeight, ColRadius);	
}

UBOOL AActor::TouchReachSucceeded(APawn *P, const FVector &TestPosition)
{
	UBOOL bTouchTest = bCollideActors && P->bCollideActors && !GIsEditor;
	if ( bTouchTest )
	{
		if ( TestPosition == P->Location )
		{
			for (INT i = 0; i < Touching.Num(); i++)
			{
				if ( Touching(i) == P )
				{
					// succeed if touching
					return TRUE;
				}
			}
			return FALSE;
		}

		// if TestPosition != P->Location, can't use touching array
		UCylinderComponent* CylComp1 = Cast<UCylinderComponent>(this->CollisionComponent);
		if (CylComp1 != NULL && (!bBlockActors || !CylComp1->BlockActors))
		{
			return
				( (Square(Location.Z - TestPosition.Z) < Square(CylComp1->CollisionHeight + P->CylinderComponent->CollisionHeight))
					&&	( Square(Location.X - TestPosition.X) + Square(Location.Y - TestPosition.Y) <
								Square(CylComp1->CollisionRadius + P->CylinderComponent->CollisionRadius) ) );
		}
	}
	return FALSE;
}

UBOOL APawn::ReachedDestination(class AActor* GoalActor)
{
	if( GoalActor != NULL )
	{
		return ReachedDestination(Location, GoalActor->GetDestination( Controller ), GoalActor);
	}

	return FALSE;
}

/*
	ReachedDestination()
	Return TRUE if sufficiently close to destination.
*/
UBOOL APawn::ReachedDestination(const FVector &TestPosition, const FVector &Dest, AActor* GoalActor, UBOOL bCheckHandle)
{
	if ( GoalActor && (!Controller || !Controller->bAdjusting) )
	{
		return GoalActor->ReachedBy(this, TestPosition, Dest);
	}
	else if( bCheckHandle && Controller != NULL && Controller->NavigationHandle != NULL )
	{
		UBOOL bRet = FALSE;
		// if the navhandle successfuly determined if we're there, return what it decided.. otherwise go to thresholdtest
		if(Controller->NavigationHandle->ReachedDestination(Dest,Controller,CylinderComponent->CollisionRadius + DestinationOffset,bRet))
		{
			return bRet;
		}
		else
		{
			// navmesh destinations are usually near the floor so adjust for that
			return ReachThresholdTest(TestPosition, Controller->NavigationHandle->MoveToDesiredHeightAboveMesh(Dest, CylinderComponent->CollisionHeight), NULL, 0.f, 0.f, 0.f);
		}
	}
	else
	{
		return ReachThresholdTest(TestPosition, Dest, NULL, 0.f, 0.f, 0.f);
	}
}

UBOOL APawn::ReachedPoint( FVector Point, AActor* NewAnchor )
{
	if (ReachedDestination(Location, Point, NULL, TRUE))
	{
		ANavigationPoint* Nav = Cast<ANavigationPoint>(NewAnchor);
		if( Nav )
		{
			SetAnchor( Nav );
		}
		return TRUE;
	}

	return FALSE;
}


UBOOL APawn::ReachThresholdTest(const FVector &TestPosition, const FVector &Dest, AActor* GoalActor, FLOAT UpThresholdAdjust, FLOAT DownThresholdAdjust, FLOAT ThresholdAdjust)
{
	// get the pawn's normal height (might be crouching or a Scout, so use the max of current/default)
	const FLOAT PawnFullHeight = Max<FLOAT>(CylinderComponent->CollisionHeight, ((APawn*)(GetClass()->GetDefaultObject()))->CylinderComponent->CollisionHeight);
	FLOAT UpThreshold = UpThresholdAdjust + PawnFullHeight + PawnFullHeight - CylinderComponent->CollisionHeight;
	FLOAT DownThreshold = DownThresholdAdjust + CylinderComponent->CollisionHeight;
	FLOAT Threshold = ThresholdAdjust + CylinderComponent->CollisionRadius + DestinationOffset;

	FVector Dir = Dest - TestPosition;

	// give gliding pawns more leeway
	if ( !bCanStrafe && ((Physics == PHYS_Flying) || (Physics == PHYS_Swimming)) 
		&& ((Velocity | Dir) < 0.f) )
	{
		UpThreshold = 2.f * UpThreshold;
		DownThreshold = 2.f * DownThreshold;
		Threshold = 2.f * Threshold;
	}
	else if ( Physics == PHYS_RigidBody )
	{
		if ( GoalActor )
		{
			FLOAT GoalRadius, GoalHeight;
			GoalActor->GetBoundingCylinder(GoalRadius, GoalHeight);
			UpThreshold = ::Max(UpThreshold, GoalHeight);
		}
		UpThreshold = ::Max(UpThreshold, CylinderComponent->CollisionHeight);
		DownThreshold = ::Max(3.f * CylinderComponent->CollisionHeight, DownThreshold);
	}

	//debug
/*	if( Physics == PHYS_Flying )
	{
		FlushPersistentDebugLines();
		DrawDebugCylinder( Location+FVector(0,0,1)*UpThreshold, Location-FVector(0,0,1)*DownThreshold, Threshold, 20, 255, 0, 0, TRUE );
		DrawDebugBox( Dest, FVector(5,5,5), 255, 128, 128, TRUE );
		debugf(TEXT("Reach test UpT %f DownT %f T %f DestOffset %f"), UpThreshold, DownThreshold, Threshold, DestinationOffset );
	}*/

	FLOAT Zdiff = Dir.Z;
	Dir.Z = 0.f;
	if ( Dir.SizeSquared() > Threshold * Threshold )
		return FALSE;

	if ( (Zdiff > 0.f) ? (Abs(Zdiff) > UpThreshold) : (Abs(Zdiff) > DownThreshold) )
	{
		if ( (Zdiff > 0.f) ? (Abs(Zdiff) > 2.f * UpThreshold) : (Abs(Zdiff) > 2.f * DownThreshold) )
			return FALSE;

		// Check if above or below because of slope
		FCheckResult Hit(1.f);
		UBOOL bCheckSlope = FALSE;
		if ( (Zdiff < 0.f) && (CylinderComponent->CollisionRadius > CylinderComponent->CollisionHeight) )
		{
			// if wide pawn, use a smaller trace down and check for overlap 
			GWorld->SingleLineCheck( Hit, this, TestPosition - FVector(0.f,0.f,CylinderComponent->CollisionHeight), TestPosition, TRACE_World, FVector(CylinderComponent->CollisionHeight,CylinderComponent->CollisionHeight,CylinderComponent->CollisionHeight));
			bCheckSlope = ( Hit.Time < 1.f );
			Zdiff = Dest.Z - Hit.Location.Z;
		}
		else
		{
			// see if on slope
			GWorld->SingleLineCheck( Hit, this, TestPosition - FVector(0.f,0.f,LedgeCheckThreshold+MAXSTEPHEIGHTFUDGE), TestPosition, TRACE_World, FVector(CylinderComponent->CollisionRadius,CylinderComponent->CollisionRadius,CylinderComponent->CollisionHeight));
			bCheckSlope = ( (Hit.Normal.Z < 0.95f) && (Hit.Normal.Z >= WalkableFloorZ) );
		}
		if ( bCheckSlope )
		{
			// check if above because of slope
			if ( (Zdiff < 0.f)
				&& (Zdiff * -1.f < PawnFullHeight + CylinderComponent->CollisionRadius * appSqrt(1.f/(Hit.Normal.Z * Hit.Normal.Z) - 1.f)) )
			{
				return TRUE;
			}
			else
			{
				// might be below because on slope
				FLOAT adjRad = 0.f;
				if ( GoalActor )
				{
					FLOAT GoalHeight;
					GoalActor->GetBoundingCylinder(adjRad, GoalHeight);
				}
				else
				{
					ANavigationPoint* DefaultNavPoint = (ANavigationPoint*)ANavigationPoint::StaticClass()->GetDefaultActor();
					adjRad = DefaultNavPoint->CylinderComponent->CollisionRadius;
				}
				if ( (CylinderComponent->CollisionRadius < adjRad)
					&& (Zdiff < PawnFullHeight + (adjRad + 15.f - CylinderComponent->CollisionRadius) * appSqrt(1.f/(Hit.Normal.Z * Hit.Normal.Z) - 1.f)) )
				{
					return TRUE;
				}
			}
		}
		return FALSE;
	}
	return TRUE;
}

/* ladderReachable()
Pawn is on ladder. Return FALSE if no GoalActor, else TRUE if GoalActor is on the same ladder
*/
INT APawn::ladderReachable(const FVector &Dest, const FVector &Start, INT reachFlags, AActor* GoalActor)
{
	SCOPE_CYCLE_COUNTER(STAT_PathFinding_Reachable);
	TRACK_DETAILED_PATHFINDING_STATS(this);

	if ( !OnLadder || !GoalActor || ((GoalActor->Physics != PHYS_Ladder) && !GoalActor->IsA(ALadder::StaticClass())) )
		return walkReachable(Dest, Start, reachFlags, GoalActor);
	
	ALadder *L = Cast<ALadder>(GoalActor);
	ALadderVolume *GoalLadder = NULL;
	if ( L )
		GoalLadder = L->MyLadder;
	else
	{
		APawn *GoalPawn = GoalActor->GetAPawn(); 
		if ( GoalPawn && GoalPawn->OnLadder )
			GoalLadder = GoalPawn->OnLadder;
		else
			return walkReachable(Dest, Start, reachFlags, GoalActor);
	}

	if ( GoalLadder == OnLadder )
		return bCanClimbLadders;
	return walkReachable(Dest, Start, reachFlags, GoalActor);
}

INT APawn::flyReachable(const FVector &Dest, const FVector &Start, INT reachFlags, AActor* GoalActor)
{
	SCOPE_CYCLE_COUNTER(STAT_PathFinding_Reachable);
	TRACK_DETAILED_PATHFINDING_STATS(this);

	reachFlags = reachFlags | R_FLY;
	INT success = 0;
	FVector CurrentPosition = Start;
	ETestMoveResult stillmoving = TESTMOVE_Moved;
	FVector Direction = Dest - CurrentPosition;
	FLOAT Movesize = ::Max(MAXTESTMOVESIZE, CylinderComponent->CollisionRadius);
	FLOAT MoveSizeSquared = Movesize * Movesize;
	INT ticks = 100; 
	if ( !GWorld->HasBegunPlay() )
		ticks = 10000;

	while (stillmoving != TESTMOVE_Stopped )
	{
		Direction = Dest - CurrentPosition;
		if ( !ReachedDestination(CurrentPosition, Dest, GoalActor) )
		{
			if ( Direction.SizeSquared() < MoveSizeSquared )
				stillmoving = flyMove(Direction, CurrentPosition, GoalActor, 2.f*MINMOVETHRESHOLD);
			else
			{
				Direction = Direction.SafeNormal();
				stillmoving = flyMove(Direction * Movesize, CurrentPosition, GoalActor, MINMOVETHRESHOLD);
			}
			if ( stillmoving == TESTMOVE_HitGoal ) //bumped into goal
			{
				stillmoving = TESTMOVE_Stopped;
				success = 1;
			}
			else if ( (stillmoving != TESTMOVE_Stopped) )
			{
				APhysicsVolume *NewVolume = GWorld->GetWorldInfo()->GetPhysicsVolume(CurrentPosition,this,FALSE);
				if ( NewVolume->bWaterVolume )
				{
					stillmoving = TESTMOVE_Stopped;
					if ( bCanSwim && !NewVolume->WillHurt(this) )
					{
						reachFlags = swimReachable(Dest, CurrentPosition, reachFlags, GoalActor);
						success = reachFlags;
					}
				}
			}
		}
		else
		{
			stillmoving = TESTMOVE_Stopped;
			success = 1;
		}
		ticks--;
		if (ticks < 0)
			stillmoving = TESTMOVE_Stopped;
	}

	if (success)
		return reachFlags;
	else
		return 0;
}

INT APawn::swimReachable(const FVector &Dest, const FVector &Start, INT reachFlags, AActor* GoalActor)
{
	SCOPE_CYCLE_COUNTER(STAT_PathFinding_Reachable);
	TRACK_DETAILED_PATHFINDING_STATS(this);
	reachFlags = reachFlags | R_SWIM;
	INT success = 0;
	FVector CurrentPosition = Start;
	ETestMoveResult stillmoving = TESTMOVE_Moved;
	FVector CollisionExtent = GetDefaultCollisionSize();
	FLOAT Movesize = ::Max(MAXTESTMOVESIZE, CollisionExtent.X);
	FLOAT MoveSizeSquared = Movesize * Movesize;
	INT ticks = 100; 
	if ( !GWorld->HasBegunPlay() )
		ticks = 1000;

	while ( stillmoving != TESTMOVE_Stopped )
	{
		FVector Direction = Dest - CurrentPosition;
		if ( !ReachedDestination(CurrentPosition, Dest, GoalActor) )
		{
			if ( Direction.SizeSquared() < MoveSizeSquared )
				stillmoving = swimMove(Direction, CurrentPosition, GoalActor, 2.f*MINMOVETHRESHOLD);
			else
			{
				Direction = Direction.SafeNormal();
				stillmoving = swimMove(Direction * Movesize, CurrentPosition, GoalActor, MINMOVETHRESHOLD);
			}
			if ( stillmoving == TESTMOVE_HitGoal ) //bumped into goal
			{
				stillmoving = TESTMOVE_Stopped;
				success = 1;
			}

			APhysicsVolume *NewVolume = GWorld->GetWorldInfo()->GetPhysicsVolume(CurrentPosition,this,FALSE);

			if ( NewVolume->bWaterVolume && (stillmoving == TESTMOVE_Stopped) && bCanWalk )
			{
				// allow move up in case close to edge of wall, if still in water
				FCheckResult Hit(1.f);
				TestMove(FVector(0,0,MaxStepHeight), CurrentPosition, Hit, CollisionExtent);
				NewVolume = GWorld->GetWorldInfo()->GetPhysicsVolume(CurrentPosition,this,FALSE);
			}
			if ( !NewVolume->bWaterVolume )
			{
				stillmoving = TESTMOVE_Stopped;
				if (bCanFly)
				{
					reachFlags = flyReachable(Dest, CurrentPosition, reachFlags, GoalActor);
					success = reachFlags;
				}
				else if ( bCanWalk && (Dest.Z < CurrentPosition.Z + CollisionExtent.Z + MaxStepHeight) )
				{
					FCheckResult Hit(1.f);
					TestMove(FVector(0,0,::Max(CollisionExtent.Z + MaxStepHeight,Dest.Z - CurrentPosition.Z)), CurrentPosition, Hit, CollisionExtent);
					if (Hit.Time == 1.f)
					{
						success = flyReachable(Dest, CurrentPosition, reachFlags, GoalActor);
						reachFlags = success & !R_FLY;
						reachFlags |= R_WALK;
					}
				}
			}
			else if ( NewVolume->WillHurt(this) )
			{
				stillmoving = TESTMOVE_Stopped;
				success = 0;
			}
		}
		else
		{
			stillmoving = TESTMOVE_Stopped;
			success = 1;
		}
		ticks--;
		if (ticks < 0)
		{
			stillmoving = TESTMOVE_Stopped;
		}
	}

	if (success)
		return reachFlags;
	else
		return 0;
}

INT APawn::spiderReachable( const FVector &Dest, const FVector &Start, INT reachFlags, AActor* GoalActor)
{
	SCOPE_CYCLE_COUNTER(STAT_PathFinding_Reachable);
	TRACK_DETAILED_PATHFINDING_STATS(this);
	FVector CollisionExtent = GetDefaultCollisionSize();
	reachFlags = reachFlags | R_WALK;
	UBOOL bSuccess = 0;

	FVector CurrentPosition = Start;
	ETestMoveResult stillmoving = TESTMOVE_Moved;
	FVector Direction;

	FLOAT Movesize = CollisionExtent.X;
	INT ticks = 100; 
	if( GWorld->HasBegunPlay() )
	{
		// use longer steps if have begun play and can jump
		if( bJumpCapable )
		{
			Movesize = ::Max(TESTMINJUMPDIST, Movesize);
		}
	}
	else
	{
		// allow longer walks when path building in editor
		ticks = 1000;
	}
	
	FLOAT MoveSizeSquared = Movesize * Movesize;
	FCheckResult Hit(1.f);
	APhysicsVolume *CurrentVolume = GWorld->GetWorldInfo()->GetPhysicsVolume( CurrentPosition, this, FALSE );
	FVector CheckDown = GetGravityDirection();
	CheckDown.Z *= (0.5f * CollisionExtent.Z + MaxStepHeight + 2.f * MAXSTEPHEIGHTFUDGE);

	while( stillmoving == TESTMOVE_Moved )
	{
		if( ReachedDestination( CurrentPosition, Dest, GoalActor ) )
		{
			// Success!
			stillmoving = TESTMOVE_Stopped;
			bSuccess = TRUE;
		}
		else
		{
			Direction = Dest - CurrentPosition;
			Direction.Z = 0; //this is a 2D move
			if( Direction.SizeSquared() < MoveSizeSquared )
			{
				// shorten this step since near end
				stillmoving = walkMove( Direction, CurrentPosition, CollisionExtent, Hit, GoalActor, 2.f*MINMOVETHRESHOLD );
			}
			else
			{
				// step toward destination
				Direction   = Direction.SafeNormal();
				Direction  *= Movesize;
				stillmoving = walkMove( Direction, CurrentPosition, CollisionExtent, Hit, GoalActor, MINMOVETHRESHOLD );
			}

			if( stillmoving != TESTMOVE_Moved )
			{
				if( stillmoving == TESTMOVE_HitGoal ) 
				{
					// bumped into goal - success!
					stillmoving = TESTMOVE_Stopped;
					bSuccess = TRUE;
				}
				else 
				if( bCanFly )
				{
					// no longer walking - see if can fly to destination
					stillmoving = TESTMOVE_Stopped;
					reachFlags	= flyReachable( Dest, CurrentPosition, reachFlags, GoalActor );
					bSuccess	= reachFlags;
				}
				else 
				if( bJumpCapable )
				{
					// try to jump to destination
					reachFlags = reachFlags | R_JUMP;

					if( stillmoving == TESTMOVE_Fell )
					{
						// went off ledge
						FVector Landing = Dest;
						if( GoalActor )
						{
							FLOAT GoalRadius, GoalHeight;
							GoalActor->GetBoundingCylinder(GoalRadius, GoalHeight);
							Landing.Z = Landing.Z - GoalHeight + CollisionExtent.Z;
						}

						stillmoving = FindBestJump(Landing, CurrentPosition);
					}
					else if (stillmoving == TESTMOVE_Stopped)
					{
						// try to jump up over barrier
						stillmoving = FindJumpUp(Direction, CurrentPosition);
						if ( stillmoving == TESTMOVE_HitGoal ) 
						{
							// bumped into goal - success!
							stillmoving = TESTMOVE_Stopped;
							bSuccess = TRUE;
						}
					}
					if ( SetHighJumpFlag() )
						reachFlags = reachFlags | R_HIGHJUMP;
				}
				else 
				if( (stillmoving == TESTMOVE_Fell) && (Movesize > MaxStepHeight) ) 
				{
					// try smaller step
					stillmoving = TESTMOVE_Moved;
					Movesize = MaxStepHeight;
				}
			}
			else
			if ( !GWorld->HasBegunPlay() )
			{
				// FIXME - make sure fully on path
				GWorld->SingleLineCheck(Hit, this, CurrentPosition + CheckDown, CurrentPosition, TRACE_World|TRACE_StopAtAnyHit, 0.5f * CollisionExtent);
				if( Hit.Time == 1.f )
				{
					reachFlags = reachFlags | R_JUMP;	
				}
			}

			// check if entered new physics volume
			APhysicsVolume *NewVolume = GWorld->GetWorldInfo()->GetPhysicsVolume(CurrentPosition,this,FALSE);
			if( NewVolume != CurrentVolume )
			{
				if( NewVolume->WillHurt(this) )
				{
					// failed because entered pain volume
					stillmoving = TESTMOVE_Stopped;
					bSuccess = FALSE;
				}
				else 
				if( NewVolume->bWaterVolume )
				{
					// entered water - try to swim to destination
					stillmoving = TESTMOVE_Stopped;
					if( bCanSwim )
					{
						reachFlags = swimReachable( Dest, CurrentPosition, reachFlags, GoalActor );
						bSuccess = reachFlags;
					}
				}
				else 
				if( bCanClimbLadders && 
					GoalActor && 
					(GoalActor->PhysicsVolume == NewVolume) && 
					NewVolume->IsA(ALadderVolume::StaticClass()) )
				{
					// entered ladder, and destination is on same ladder - success!
					stillmoving = TESTMOVE_Stopped;
					bSuccess = TRUE;
				}
			}

			CurrentVolume = NewVolume;
			if( ticks-- < 0 )
			{
				//debugf(TEXT("OUT OF TICKS"));
				stillmoving = TESTMOVE_Stopped;
			}
		}
	}
	return (bSuccess ? reachFlags : 0);

//	return 1;
}

void APawn::GetBoundingCylinder(FLOAT& CollisionRadius, FLOAT& CollisionHeight) const
{
	// if we are a template, no components will be attached and the default implementation of calling GetComponentsBoundingBox() will do nothing
	// so use our CylinderComponent instead if possible
	if (CylinderComponent != CollisionComponent && IsTemplate() && CylinderComponent != NULL)
	{
		CollisionRadius = CylinderComponent->CollisionRadius;
		CollisionHeight = CylinderComponent->CollisionHeight;
	}
	else
	{
		Super::GetBoundingCylinder(CollisionRadius, CollisionHeight);
	}
}

FVector APawn::GetDefaultCollisionSize()
{
	UCylinderComponent* Cylinder = GWorld->HasBegunPlay() ? GetClass()->GetDefaultObject<APawn>()->CylinderComponent : CylinderComponent;
	return (Cylinder != NULL) ? FVector(Cylinder->CollisionRadius, Cylinder->CollisionRadius, Cylinder->CollisionHeight) : FVector(0.f, 0.f, 0.f);
}

FVector APawn::GetCrouchSize()
{
	return FVector(CrouchRadius, CrouchRadius, CrouchHeight);
}

/*walkReachable() -
walkReachable returns 0 if Actor cannot reach dest, and 1 if it can reach dest by moving in
 straight line
 actor must remain on ground at all times
 Note that Actor is not actually moved
*/
INT APawn::walkReachable(const FVector &Dest, const FVector &Start, INT reachFlags, AActor* GoalActor)
{
	SCOPE_CYCLE_COUNTER(STAT_PathFinding_Reachable);
	TRACK_DETAILED_PATHFINDING_STATS(this);
	FVector CollisionExtent = bCanCrouch ? GetCrouchSize() : GetDefaultCollisionSize();
	reachFlags = reachFlags | R_WALK;
	INT success = 0;
	FVector CurrentPosition = Start;
	ETestMoveResult stillmoving = TESTMOVE_Moved;
	FVector Direction;

	FLOAT Movesize = CollisionExtent.X;
	INT ticks = 100; 
	if (GWorld->HasBegunPlay())
	{
		// use longer steps if have begun play and can jump
		if ( bJumpCapable )
			Movesize = ::Max(TESTMINJUMPDIST, Movesize);
	}
	else
	{
		// allow longer walks when path building in editor
		ticks = 1000;
	}
	
	FLOAT MoveSizeSquared = Movesize * Movesize;
	FCheckResult Hit(1.f);
	APhysicsVolume *CurrentVolume = GWorld->GetWorldInfo()->GetPhysicsVolume(CurrentPosition,this,FALSE);
	FVector CheckDown = FVector(0,0,-1 * (0.5f * CollisionExtent.Z + MaxStepHeight + 2*MAXSTEPHEIGHTFUDGE));

	// make sure pawn starts on ground
	TestMove(CheckDown, CurrentPosition, Hit, CollisionExtent);

	while ( stillmoving == TESTMOVE_Moved )
	{
		if ( ReachedDestination(CurrentPosition, Dest, GoalActor) )
		{
			// Success!
			stillmoving = TESTMOVE_Stopped;
			success = 1;
		}
		else
		{
			FVector LastPosition = CurrentPosition;
			Direction = Dest - CurrentPosition;
			Direction.Z = 0; //this is a 2D move
			if ( Direction.SizeSquared() < MoveSizeSquared )
			{
				// shorten this step since near end
				stillmoving = walkMove(Direction, CurrentPosition, CollisionExtent, Hit, GoalActor, 2.f*MINMOVETHRESHOLD);
			}
			else
			{
				// step toward destination
				Direction = Direction.SafeNormal();
				Direction *= Movesize;
				stillmoving = walkMove(Direction, CurrentPosition, CollisionExtent, Hit, GoalActor, MINMOVETHRESHOLD);
			}
			if (stillmoving != TESTMOVE_Moved)
			{
				if ( stillmoving == TESTMOVE_HitGoal ) 
				{
					// bumped into goal - success!
					stillmoving = TESTMOVE_Stopped;
					success = 1;
				}
				else if ( bCanFly )
				{
					// no longer walking - see if can fly to destination
					stillmoving = TESTMOVE_Stopped;
					reachFlags = flyReachable(Dest, CurrentPosition, reachFlags, GoalActor);
					success = reachFlags;
				}
				else if ( bJumpCapable )
				{
					// try to jump to destination
					reachFlags = reachFlags | R_JUMP;	
					if (stillmoving == TESTMOVE_Fell)
					{
						// went off ledge
						FVector Landing = Dest;
						if ( GoalActor )
						{
							FLOAT GoalRadius, GoalHeight;
							GoalActor->GetBoundingCylinder(GoalRadius, GoalHeight);
							Landing.Z = Landing.Z - GoalHeight + CollisionExtent.Z;
						}
						stillmoving = FindBestJump(Landing, CurrentPosition);
					}
					else if (stillmoving == TESTMOVE_Stopped)
					{
						// try to jump up over barrier
						stillmoving = FindJumpUp(Direction, CurrentPosition);
						if ( stillmoving == TESTMOVE_HitGoal ) 
						{
							// bumped into goal - success!
							stillmoving = TESTMOVE_Stopped;
							success = 1;
						}
					}
					if ( SetHighJumpFlag() )
						reachFlags = reachFlags | R_HIGHJUMP;
				}
				else if ( (stillmoving == TESTMOVE_Fell) && (Movesize > MaxStepHeight) ) 
				{
					// try smaller step
					CurrentPosition = LastPosition;
					stillmoving = TESTMOVE_Moved;
					Movesize = MaxStepHeight;
					MoveSizeSquared = Square(Movesize);
				}
			}
			else if ( !GWorld->HasBegunPlay() )
			{
				// During path building only, if the pawn ends a move mostly hanging over a ledge, then the path is marked R_JUMP 
				// to prevent pawns which can't jump from pathing into a situation which isn't allowed by the ledge checking code (APawn::CheckForLedges()) in physwalking.
				GWorld->SingleLineCheck(Hit, this, CurrentPosition + CheckDown, CurrentPosition, TRACE_World|TRACE_StopAtAnyHit, 0.5f * CollisionExtent);
				if ( Hit.Time == 1.f )
					reachFlags = reachFlags | R_JUMP;	
			}

			// check if entered new physics volume
			APhysicsVolume *NewVolume = GWorld->GetWorldInfo()->GetPhysicsVolume(CurrentPosition,this,FALSE);
			if ( NewVolume != CurrentVolume )
			{
				if ( NewVolume->WillHurt(this) )
				{
					// failed because entered pain volume
					stillmoving = TESTMOVE_Stopped;
					success = 0;
				}
				else if ( NewVolume->bWaterVolume )
				{
					// entered water - try to swim to destination
					stillmoving = TESTMOVE_Stopped;
					if (bCanSwim )
					{
						reachFlags = swimReachable(Dest, CurrentPosition, reachFlags, GoalActor);
						success = reachFlags;
					}
				}
				else if ( bCanClimbLadders && GoalActor && (GoalActor->PhysicsVolume == NewVolume)
						&& NewVolume->IsA(ALadderVolume::StaticClass()) )
				{
					// entered ladder, and destination is on same ladder - success!
					stillmoving = TESTMOVE_Stopped;
					success = 1;
				}
			}
			CurrentVolume = NewVolume;
			if (ticks-- < 0)
			{
				//debugf(TEXT("OUT OF TICKS"));
				stillmoving = TESTMOVE_Stopped;
			}
		}
	}
	return (success ? reachFlags : 0);
}

ETestMoveResult APawn::FindJumpUp(FVector Direction, FVector &CurrentPosition)
{
	FCheckResult Hit(1.f);
	FVector StartLocation = CurrentPosition;
	FVector CollisionExtent = GetDefaultCollisionSize();

	TestMove(FVector(0,0,MaxJumpHeight - MaxStepHeight), CurrentPosition, Hit, CollisionExtent);
	ETestMoveResult success = walkMove(Direction, CurrentPosition, CollisionExtent, Hit, NULL, MINMOVETHRESHOLD);

	StartLocation.Z = CurrentPosition.Z;
	if ( success != TESTMOVE_Stopped )
	{
		TestMove(-1.f*FVector(0,0,MaxJumpHeight), CurrentPosition, Hit, CollisionExtent);
		// verify walkmove didn't just step down
		StartLocation.Z = CurrentPosition.Z;
		if ((StartLocation - CurrentPosition).SizeSquared() < MINMOVETHRESHOLD * MINMOVETHRESHOLD)
			return TESTMOVE_Stopped;
	}
	else
		CurrentPosition = StartLocation;

	return success;

}

/** SuggestTossVelocity()
 * returns a recommended Toss velocity vector, given a destination and a Toss speed magnitude
 * @param TossVelocity - out param stuffed with the computed velocity to use
 * @param End - desired end point of arc
 * @param Start - desired start point of arc
 * @param TossSpeed - in the magnitude of the toss - assumed to only change due to gravity for the entire lifetime of the projectile
 * @param BaseTossZ - is an additional Z direction force added to the toss (which will not be included in the returned TossVelocity) - (defaults to 0)
 * @param DesiredZPct (optional) - is the requested pct of the toss in the z direction (0=toss horizontally, 0.5 = toss at 45 degrees).  This is the starting point for finding a toss.  (Defaults to 0.05).
 *		the purpose of this is to bias the test in cases where there is more than one solution
 * @param CollisionSize (optional) - is the size of bunding box of the tossed actor (defaults to (0,0,0)
 * @param TerminalVelocity (optional) - terminal velocity of the projectile
 * @param OverrideGravityZ (optional) - gravity inflicted upon the projectile in the z direction
 * @param bOnlyTraceUp  (optional) - when TRUE collision checks verifying the arc will only be done along the upward portion of the arc
 * @return - TRUE/FALSE depending on whether a valid arc was computed
*/
UBOOL AActor::SuggestTossVelocity(FVector* TossVelocity, const FVector& End, const FVector& Start, FLOAT TossSpeed, FLOAT BaseTossZ, FLOAT DesiredZPct, const FVector& CollisionSize, FLOAT TerminalVelocity, FLOAT OverrideGravityZ, UBOOL bOnlyTraceUp)
{
	const FLOAT StartXYPct = Max<FLOAT>(1.f - DesiredZPct,0.05f); 
	FLOAT XYPct = StartXYPct;
	FVector Flight = End - Start;
	const FLOAT FlightZ = Flight.Z;
	Flight.Z = 0.f;
	const FLOAT FlightSize = Flight.Size();

	if ( (FlightSize == 0.f) || (TossSpeed == 0.f) )
	{
		*TossVelocity = FVector(0.f, 0.f, TossSpeed);
		return FALSE;
	}

	FLOAT Gravity;
	if (OverrideGravityZ != 0.f)
	{
		Gravity = OverrideGravityZ;
	}
	else
	{
		Gravity = (PhysicsVolume != NULL) ? PhysicsVolume->GetGravityZ() : GWorld->GetGravityZ();
	}
	FLOAT XYSpeed = XYPct*TossSpeed;
	FLOAT FlightTime = FlightSize/XYSpeed;
	FLOAT ZSpeed = FlightZ/FlightTime - Gravity * FlightTime - BaseTossZ;
	FLOAT TossSpeedSq = TossSpeed*TossSpeed;
	if ( TerminalVelocity == 0.f )
	{
		TerminalVelocity = GetTerminalVelocity();
	}
	TossSpeedSq = ::Min(TossSpeedSq, Square(TerminalVelocity));

	// iteratively find a working toss velocity with magnitude <= TossSpeed
	FLOAT TestSpeedSq = Square(ZSpeed) + Square(XYSpeed);
	if ( StartXYPct == 1.f )
	{
		while (XYPct > 0.f && (TestSpeedSq > TossSpeedSq || TestSpeedSq < TossSpeedSq * 0.9f))
		{
			// pick an XYSpeed
			XYPct -= 0.05f;
			XYSpeed = XYPct*TossSpeed;
			FlightTime = FlightSize/XYSpeed;
			ZSpeed = FlightZ/FlightTime - Gravity * FlightTime - BaseTossZ;
			TestSpeedSq = Square(ZSpeed) + Square(XYSpeed);
		}
	}
	else
	{
		while (XYPct < 1.f && (TestSpeedSq > TossSpeedSq || TestSpeedSq < TossSpeedSq * 0.9f))
		{
			// pick an XYSpeed
			XYPct += 0.05f;
			XYSpeed = XYPct*TossSpeed;
			FlightTime = FlightSize/XYSpeed;
			ZSpeed = FlightZ/FlightTime - Gravity * FlightTime - BaseTossZ;
			TestSpeedSq = Square(ZSpeed) + Square(XYSpeed);
		}
	}

	const FVector FlightDir = Flight/FlightSize;

	// make sure we ended with a valid toss velocity
	if (TestSpeedSq > TossSpeedSq * 1.15f || TestSpeedSq < TossSpeedSq * 0.85f)
	{
		*TossVelocity = TossSpeed * (XYSpeed*FlightDir + FVector(0.f,0.f,ZSpeed)).SafeNormal();
		return FALSE;
	}

	// trace check trajectory
	UBOOL bFailed = TRUE;
	FCheckResult Hit(1.f);

	while ( bFailed && (XYPct > 0.f) )
	{
		const FVector StartVel = XYSpeed*FlightDir + FVector(0.f,0.f,ZSpeed+BaseTossZ);
		const FLOAT StepSize = 0.125f;
		FVector TraceStart = Start;
		bFailed = FALSE;

		if ( GWorld->HasBegunPlay() )
		{
			// if game in progress, do fast line traces, moved down by collision box size (assumes that obstacles more likely to be below)
			for ( FLOAT Step=0.f; Step<1.f; Step+=StepSize )
			{
				FlightTime = (Step+StepSize) * FlightSize/XYSpeed;
				if (bOnlyTraceUp && StartVel.Z + Gravity * FlightTime <= 0.0f)
				{
					break;
				}
				const FVector TraceEnd = Start + StartVel*FlightTime + FVector(0.f, 0.f, Gravity * FlightTime * FlightTime - CollisionSize.Z);
				// Uncomment this for debugging the arc for this toss.  Visualizing the arc is really key for debugging why things are not working correctly
				//DrawDebugLine( TraceStart, TraceEnd, 255, 0, 0, TRUE );
				if ( !GWorld->SingleLineCheck( Hit, this, TraceEnd, TraceStart, TRACE_World|TRACE_StopAtAnyHit ) )
				{
					bFailed = TRUE;
					break;
				}
				TraceStart = TraceEnd;
			}
		}
		else
		{
			// if no game in progress, do slower but more accurate box traces
			for ( FLOAT Step=0.f; Step<1.f; Step+=StepSize )
			{
				FlightTime = (Step+StepSize) * FlightSize/XYSpeed;
				if (bOnlyTraceUp && StartVel.Z + Gravity * FlightTime <= 0.0f)
				{
					break;
				}
				const FVector TraceEnd = Start + StartVel*FlightTime + FVector(0.f, 0.f, Gravity * FlightTime * FlightTime);
				if ( !GWorld->SingleLineCheck( Hit, this, TraceEnd, TraceStart, TRACE_World|TRACE_StopAtAnyHit, CollisionSize ) )
				{
					bFailed = TRUE;
					break;
				}
				TraceStart = TraceEnd;
			}
		}

		if ( bFailed )
		{
			if ( XYPct >= StartXYPct )
			{
				// no valid toss possible - return an average arc
				XYSpeed = 0.7f*TossSpeed;
				FlightTime = FlightSize/XYSpeed;
				ZSpeed = FlightZ/FlightTime - Gravity * FlightTime - BaseTossZ;
				*TossVelocity = XYSpeed*FlightDir + FVector(0.f,0.f,ZSpeed);
				return FALSE;
			}
			else
			{
				// raise trajectory
				XYPct -= 0.1f;
				XYSpeed = XYPct*TossSpeed;
				FlightTime = FlightSize/XYSpeed;
				ZSpeed = FlightZ/FlightTime - Gravity * FlightTime - BaseTossZ;
				if ( ZSpeed*ZSpeed + XYSpeed * XYSpeed > TossSpeed * TossSpeed )
				{
					// no valid toss possible - return an average arc
					XYSpeed = 0.7f*TossSpeed;
					FlightTime = FlightSize/XYSpeed;
					ZSpeed = FlightZ/FlightTime - Gravity * FlightTime - BaseTossZ;
					*TossVelocity = XYSpeed*FlightDir + FVector(0.f,0.f,ZSpeed);
					return FALSE;
				}
			}
		}
	}
	*TossVelocity = XYSpeed*FlightDir + FVector(0.f,0.f,ZSpeed);
	return TRUE;
}
/** CalculateMinSpeedTrajectory()
 * returns a velocity that will result in a trajectory that minimizes the speed of the projectile within the given range
 * @param out_Velocity - out param stuffed with the computed velocity to use
 * @param End - desired end point of arc
 * @param Start - desired start point of arc
 * @param MaxTossSpeed - Max acceptable speed of projectile
 * @param MinTossSpeed - Min Acceptable speed of projectile
 * @param CollisionSize (optional) - is the size of bunding box of the tossed actor (defaults to (0,0,0)
 * @param TerminalVelocity (optional) - terminal velocity of the projectile
 * @param GravityZ (optional) - gravity inflicted upon the projectile in the z direction
 * @param bOnlyTraceUp  (optional) - when TRUE collision checks verifying the arc will only be done along the upward portion of the arc
 * @return - TRUE/FALSE depending on whether a valid arc was computed
*/
UBOOL AActor::CalculateMinSpeedTrajectory(FVector& out_Velocity,
										  FVector End,
										  FVector Start,
										  FLOAT MaxTossSpeed,
										  FLOAT MinTossSpeed,
										  FVector CollisionSize,
										  FLOAT TerminalVelocity/*=0*/,
										  FLOAT Gravity/*=0*/,
										  UBOOL bOnlyTraceUp/*=0*/)
{
	if( appIsNearlyZero(Gravity) )
	{
		Gravity = (PhysicsVolume != NULL) ? PhysicsVolume->GetGravityZ() : GWorld->GetGravityZ();
	}
	if( appIsNearlyZero(TerminalVelocity) )
	{
		TerminalVelocity = GetTerminalVelocity();
	}

	if( appIsNearlyZero(MinTossSpeed) )
	{
		MinTossSpeed = 1.0f;
	}



	const FLOAT StartXYPct = 1.f;
	FLOAT XYPct = StartXYPct;
	FVector Flight = End - Start;
	const FLOAT FlightZ = Flight.Z;
	Flight.Z = 0.f;
	const FLOAT FlightSize = Flight.Size();

	if ( (FlightSize == 0.f) || (MaxTossSpeed == 0.f) )
	{
		out_Velocity= FVector(0.f, 0.f, MaxTossSpeed);
		return FALSE;
	}

	FLOAT XYSpeed = XYPct*MinTossSpeed;
	FLOAT FlightTime = FlightSize/XYSpeed;
	FLOAT ZSpeed = FlightZ/FlightTime - Gravity * FlightTime;
	FLOAT MinTossSpeedSq = MinTossSpeed*MinTossSpeed;
	FLOAT MaxTossSpeedSq = MaxTossSpeed*MaxTossSpeed;

	MaxTossSpeedSq = ::Min(MaxTossSpeedSq, Square(TerminalVelocity));

	// iteratively find a working toss velocity with MinTossSpeed <= magnitude <= MaxTossSpeed
	FLOAT TestSpeedSq = Square(ZSpeed) + Square(XYSpeed);
	FLOAT LastTestSpeedSq = TestSpeedSq;
	while (XYPct > 0.f)
	{
		if (TestSpeedSq > MaxTossSpeedSq && LastTestSpeedSq > TestSpeedSq)
		{
			XYPct += 0.05f;
		}
		else if (TestSpeedSq < MinTossSpeedSq && LastTestSpeedSq < TestSpeedSq)
		{
			XYPct -= 0.05f;
		}
		else
		{
			break;
		}
		LastTestSpeedSq = TestSpeedSq;
		XYSpeed = XYPct*MinTossSpeed;
		FlightTime = FlightSize/XYSpeed;
		ZSpeed = FlightZ/FlightTime - Gravity * FlightTime;
		TestSpeedSq = Square(ZSpeed) + Square(XYSpeed);
	}

	const FVector FlightDir = Flight/FlightSize;

	// make sure we ended with a valid toss velocity
	if (TestSpeedSq > MaxTossSpeedSq || TestSpeedSq < MinTossSpeedSq)
	{
		out_Velocity = MinTossSpeed * (XYSpeed*FlightDir + FVector(0.f,0.f,ZSpeed)).SafeNormal();
		return FALSE;
	}

	// trace check trajectory
	UBOOL bFailed = TRUE;
	FCheckResult Hit(1.f);

	while ( bFailed && (XYPct > 0.f) )
	{
		const FVector StartVel = XYSpeed*FlightDir + FVector(0.f,0.f,ZSpeed);
		const FLOAT StepSize = 0.125f;
		FVector TraceStart = Start;
		bFailed = FALSE;

		// if game in progress, do fast line traces, moved down by collision box size (assumes that obstacles more likely to be below)
		for ( FLOAT Step=0.f; Step<1.f; Step+=StepSize )
		{
			FlightTime = (Step+StepSize) * FlightSize/XYSpeed;
			if (bOnlyTraceUp && StartVel.Z + Gravity * FlightTime <= 0.0f)
			{
				break;
			}
			const FVector TraceEnd = Start + StartVel*FlightTime + FVector(0.f, 0.f, Gravity * FlightTime * FlightTime - CollisionSize.Z);
			// Uncomment this for debugging the arc for this toss.  Visualizing the arc is really key for debugging why things are not working correctly
			//DrawDebugLine( TraceStart, TraceEnd, 255, 0, 0, TRUE );
			if ( !GWorld->SingleLineCheck( Hit, this, TraceEnd, TraceStart, TRACE_World|TRACE_StopAtAnyHit ) )
			{
				//debugf(TEXT("CalculateMinSpeedTrajectory failed %s"), *Hit.Actor->GetName() );

				bFailed = TRUE;
				break;
			}
			TraceStart = TraceEnd;
		}

		if ( bFailed )
		{
			// raise trajectory
			XYPct -= 0.1f;
			XYSpeed = XYPct*MaxTossSpeed;
			FlightTime = FlightSize/XYSpeed;
			ZSpeed = FlightZ/FlightTime - Gravity * FlightTime;
			if ( ZSpeed*ZSpeed + XYSpeed * XYSpeed > MaxTossSpeedSq )
			{
				// no valid toss possible - return an average arc
				XYSpeed = Lerp<FLOAT>(MaxTossSpeed,MinTossSpeed,0.5f);
				FlightTime = FlightSize/XYSpeed;
				ZSpeed = FlightZ/FlightTime - Gravity * FlightTime;
				out_Velocity = XYSpeed*FlightDir + FVector(0.f,0.f,ZSpeed);
				return FALSE;
			}
		}
	}
	out_Velocity = XYSpeed*FlightDir + FVector(0.f,0.f,ZSpeed);
	return TRUE;
}

FLOAT APawn::GetFallDuration()
{
	//D = V0*t + 0.5*g*t^2
	//solve for t
	//t = (-V0 +/- Sqrt(V0^2 + 2*g*D)) / g

	FCheckResult Hit(1.f);
	UBOOL bHit = !GWorld->SingleLineCheck( Hit, this, Location - FVector(0,0,1024), Location, TRACE_World, GetCylinderExtent() );
	if( bHit )
	{
		const FLOAT D = (Hit.Location.Z - Location.Z);
		const FLOAT g = GetGravityZ();

		FLOAT TimeToImpact = (-Velocity.Z - appSqrt((Velocity.Z*Velocity.Z) + (2.f * g * D))) / g;
		return TimeToImpact;
	}
	return 0.f;
}

/**
 * SuggestJumpVelocity()
 * returns true if succesful jump from start to destination is possible
 * returns a suggested initial falling velocity in JumpVelocity
 * Uses GroundSpeed and JumpZ as limits
 * 
 * @param	JumpVelocity        The vector to fill with the calculated jump velocity
 * @param   Destination         The destination location of the jump
 * @param   Start               The start location of the jump
 * @param   bRequireFallLanding If true, the jump calculated will have a velocity in the negative Z at the destination
*/
UBOOL APawn::SuggestJumpVelocity(FVector& JumpVelocity, FVector End, FVector Start, UBOOL bRequireFallLanding)
{
	FVector JumpDir = End - Start;
	FLOAT JumpDirZ = JumpDir.Z;
	JumpDir.Z = 0.f;
	FLOAT JumpDist = JumpDir.Size();

	if ( (JumpDist == 0.f) || (JumpZ <= 0.f) )
	{
		JumpVelocity = FVector(0.f, 0.f, JumpZ);
		return FALSE;
	}

	FLOAT Gravity = GetGravityZ();
	FLOAT XYSpeed = GroundSpeed;
	check(XYSpeed > 0.f);
	FLOAT JumpTime = JumpDist/XYSpeed;
	FLOAT ZSpeed = JumpDirZ/JumpTime - Gravity * JumpTime;
	FLOAT LandingZSpeed = ZSpeed + 2.f * Gravity * JumpTime;	// Account for double gravity in PHYS_Falling :(

	if ( (ZSpeed < 0.25f * JumpZ) && (JumpDirZ < 0.f) )
	{
		ZSpeed = 0.25f * JumpZ;

		// calculate XYSpeed by solving this quadratic equation for JumpTime
		// Gravity*JumpTime*JumpTime + ZSpeed*JumpTime - JumpDirZ = 0;
		JumpTime = (-1.f*ZSpeed - appSqrt(ZSpeed*ZSpeed + 4.f*Gravity*JumpDirZ))/(2.f*Gravity);
		XYSpeed = JumpDist/JumpTime;
	}
	else if ( ZSpeed > JumpZ || (bRequireFallLanding && LandingZSpeed > 0.f))
	{
		// see if could make jump if we gave more time
		const FLOAT TimeSlice = 0.1f;
		FLOAT LastZSpeed = ZSpeed;
		do
		{
			JumpTime += TimeSlice;
			FLOAT NewZSpeed = JumpDirZ/JumpTime - Gravity * JumpTime;
			if ( bRequireFallLanding && 
				 ( (NewZSpeed <= LastZSpeed && NewZSpeed < 0.f)		||
				   (  (NewZSpeed > ZSpeed) ) ) )
			{
				JumpVelocity = XYSpeed * JumpDir/JumpDist + FVector(0.f,0.f,JumpZ);
				return FALSE;
			}
			else 
			if ( !bRequireFallLanding && NewZSpeed > LastZSpeed )
			{
				JumpVelocity = XYSpeed * JumpDir/JumpDist + FVector(0.f,0.f,JumpZ);
				return FALSE;
			}
			LastZSpeed = NewZSpeed;
			LandingZSpeed = ZSpeed + 2.f * Gravity * JumpTime;
		} while ( (LastZSpeed > JumpZ) || (bRequireFallLanding && LandingZSpeed > 0.f) );
		// found usable jump speed
		ZSpeed = LastZSpeed;
		XYSpeed = JumpDist/JumpTime;
	}
   
	JumpVelocity = XYSpeed * JumpDir/JumpDist + FVector(0.f,0.f,ZSpeed);
	return TRUE;
}

/* Find best jump from current position toward destination.  Assumes that there is no immediate
barrier.  
*/
ETestMoveResult APawn::FindBestJump(FVector Dest, FVector &CurrentPosition)
{
	// Calculate jump velocity to get as close as possible to Dest
	FVector StartVel(0.f,0.f,0.f); 
	SuggestJumpVelocity(StartVel, Dest, CurrentPosition);

	// trace jump to validate that it is not obstructed
	FVector StartPos = CurrentPosition;
	FVector TraceStart = CurrentPosition;
	FVector CollisionSize = GetDefaultCollisionSize();
	UBOOL bFailed = FALSE;
	FLOAT FlightSize = (CurrentPosition - Dest).Size();
	if ( FlightSize < CollisionSize.X )
	{
		CurrentPosition = Dest;
		return TESTMOVE_Moved;
	}
	FLOAT FlightTotalTime = FlightSize/GroundSpeed;
	FLOAT StepSize = ::Max(0.03f,CylinderComponent->CollisionRadius/FlightSize);
	FCheckResult Hit(1.f);
	FLOAT Step = 0.f;
	FVector TraceEnd = CurrentPosition;

	FLOAT GravityNet;
	do
	{
		APhysicsVolume *CurrentPhysicsVolume = GWorld->GetWorldInfo()->GetPhysicsVolume(TraceStart,NULL,FALSE);
		FLOAT FlightTime = (Step+StepSize) * FlightTotalTime;
		FLOAT BaseGravity = CurrentPhysicsVolume->GetGravityZ();
		GravityNet = BaseGravity * FlightTime * FlightTime;
		TraceEnd = CurrentPosition + StartVel*FlightTime + FVector(0.f, 0.f, GravityNet);
		if ( GravityNet < -1.f * GetAIMaxFallSpeed() )
		{
			bFailed = TRUE;
		}
		else if ( CurrentPhysicsVolume->bWaterVolume )
		{
			break;
		}
		else if ( !GWorld->SingleLineCheck( Hit, this, TraceEnd, TraceStart, TRACE_AllBlocking, CollisionSize ) )
		{
			if ( Hit.Normal.Z < WalkableFloorZ )
			{
				// hit a wall
				// if we're still moving upward, see if we can get over the wall
				UBOOL bMustFall = TRUE;
				if (TraceEnd.Z > TraceStart.Z)
				{
					// trace upwards as high as our jump will get us
					FLOAT CurrentVelocityZ = StartVel.Z + BaseGravity * FlightTime;
					FlightTime = (-StartVel.Z / BaseGravity) - FlightTime;
					TraceEnd = Hit.Location + FVector(0.f, 0.f, (CurrentVelocityZ * FlightTime) + (0.5f * BaseGravity * FlightTime * FlightTime));
					if (!GWorld->SingleLineCheck(Hit, this, TraceEnd, Hit.Location, TRACE_AllBlocking, CollisionSize))
					{
						TraceEnd = Hit.Location;
					}
					// now trace one step in the direction we want to go to see if we got over the wall
					TraceStart = TraceEnd;
					TraceEnd += FVector(StartVel.X, StartVel.Y, 0.f) * StepSize;
					bMustFall = !GWorld->SingleLineCheck(Hit, this, TraceEnd, TraceStart, TRACE_AllBlocking, CollisionSize);
					// add the time it took to reach the jump apex
					Step += FlightTime / FlightTotalTime;
					// adjust CurrentPosition to account for only moving upward during this part of the jump
					CurrentPosition -= FVector(StartVel.X, StartVel.Y, 0.f) * FlightTime;
				}
				if (bMustFall)
				{
					// see if acceptable landing below
					// limit trace to point at which would reach MaxFallSpeed
					FlightTime = appSqrt(Abs(GetAIMaxFallSpeed()/BaseGravity)) - FlightTime;
					TraceEnd = Hit.Location + FVector(0.f, 0.f, BaseGravity*FlightTime*FlightTime);
					GWorld->SingleLineCheck( Hit, this, TraceEnd, Hit.Location, TRACE_AllBlocking, CollisionSize );

					bFailed = ( (Hit.Time == 1.f) || (Hit.Normal.Z < WalkableFloorZ) );
					if ( bFailed )
					{
						// check if entered water
						APhysicsVolume* PossibleWaterPhysicsVolume = GWorld->GetWorldInfo()->GetPhysicsVolume(Hit.Location,NULL,FALSE);
						if ( PossibleWaterPhysicsVolume->bWaterVolume )
						{
							bFailed = FALSE;
						}
					}
					TraceEnd = Hit.Location;
					break;
				}
			}
			else
			{
				TraceEnd = Hit.Location;
				break;
			}
		}
		TraceStart = TraceEnd;
		Step += StepSize;
	} while (!bFailed);

	if ( bFailed )
	{
		CurrentPosition = StartPos;
		return TESTMOVE_Stopped;
	}

	// if we're building paths, set the Scout's MaxLandingVelocity to its largest fall
	SetMaxLandingVelocity(GravityNet);
	CurrentPosition = TraceEnd;
	return TESTMOVE_Moved;
}

FVector APawn::GetGravityDirection()
{
	if( Physics == PHYS_Spider )
	{
		return -Floor;
	}

	return FVector(0,0,-1);
}

ETestMoveResult APawn::HitGoal(AActor *GoalActor)
{
	if ( GoalActor->IsA(ANavigationPoint::StaticClass()) && !GoalActor->bBlockActors )
		return TESTMOVE_Stopped;

	return TESTMOVE_HitGoal;
}

void APawn::TestMove(const FVector &Delta, FVector &CurrentPosition, FCheckResult& Hit, const FVector &CollisionExtent)
{
	GWorld->SingleLineCheck(Hit, this, CurrentPosition+Delta, CurrentPosition, TRACE_Others|TRACE_Volumes|TRACE_World|TRACE_Blocking, CollisionExtent);
	if ( Hit.Actor )
	{
        	CurrentPosition = Hit.Location;
	}
	else
	{
		CurrentPosition = CurrentPosition+Delta;
	}
}

/* walkMove()
Move direction must not be adjusted.
*/
ETestMoveResult APawn::walkMove(FVector Delta, FVector &CurrentPosition, const FVector &CollisionExtent, FCheckResult& Hit, AActor* GoalActor, FLOAT threshold)
{
	FVector StartLocation = CurrentPosition;
	Delta.Z = 0.f;

	//-------------------------------------------------------------------------------------------
	//Perform the move
	FVector GravDir = GetGravityDirection();
	FVector Down = GravDir * MaxStepHeight;

	TestMove(Delta, CurrentPosition, Hit, CollisionExtent);
	if( GoalActor && (Hit.Actor == GoalActor) )
	{
		return HitGoal( GoalActor );
	}

	FVector StopLocation = Hit.Location;
	if(Hit.Time < 1.f) //try to step up
	{
		Delta = Delta * (1.f - Hit.Time);
		TestMove(-1.f*Down, CurrentPosition, Hit, CollisionExtent);
		TestMove(Delta, CurrentPosition, Hit, CollisionExtent);
		if( GoalActor && (Hit.Actor == GoalActor) )
		{
			return HitGoal(GoalActor);
		}

		TestMove(Down, CurrentPosition, Hit, CollisionExtent);
		if( Hit.Time < 1.f )
		{
			if( (GravDir.Z < 0.f && Hit.Normal.Z <  WalkableFloorZ) ||	// valid floor for walking
				(GravDir.Z > 0.f && Hit.Normal.Z > -WalkableFloorZ) )   // valid floor for spidering
			{
				// Want only good floors, else undo move
				CurrentPosition = StopLocation;
				return TESTMOVE_Stopped;
			}
		}
	}

	//drop to floor
	FVector Loc = CurrentPosition;
	Down = GravDir * (MaxStepHeight + MAXSTEPHEIGHTFUDGE);
	TestMove( Down, CurrentPosition, Hit, CollisionExtent );

	// If there was no hit OR
	// If gravity is down and hit normal is too steep for floor OR
	// If gravity is up and hit normal is too steep for ceiling
	if( (Hit.Time == 1.f) || 
		(GravDir.Z < 0.f && Hit.Normal.Z <  WalkableFloorZ) || 
		(GravDir.Z > 0.f && Hit.Normal.Z > -WalkableFloorZ) )	// occurs w/ phys_spider
	{
		// Then falling
		CurrentPosition = Loc;
		return TESTMOVE_Fell;
	}

	if( GoalActor && (Hit.Actor == GoalActor) )
	{
		return HitGoal(GoalActor);
	}

	//check if move successful
	if( (CurrentPosition - StartLocation).SizeSquared() < threshold * threshold )
	{
		return TESTMOVE_Stopped;
	}
	return TESTMOVE_Moved;
}

ETestMoveResult APawn::flyMove(FVector Delta, FVector &CurrentPosition, AActor* GoalActor, FLOAT threshold)
{
	FVector StartLocation = Location;
	FVector Down = FVector(0,0,-1) * MaxStepHeight;
	FVector Up = -1 * Down;
	FVector CollisionExtent = GetDefaultCollisionSize();
	FCheckResult Hit(1.f);

	TestMove(Delta, CurrentPosition, Hit, CollisionExtent);
	if ( GoalActor && (Hit.Actor == GoalActor) )
		return HitGoal(GoalActor);
	if (Hit.Time < 1.f) //try to step up
	{
		Delta = Delta * (1.f - Hit.Time);
		TestMove(Up, CurrentPosition, Hit, CollisionExtent);
		TestMove(Delta, CurrentPosition, Hit, CollisionExtent);
		if ( GoalActor && (Hit.Actor == GoalActor) )
			return HitGoal(GoalActor);
	}

	if ((CurrentPosition - StartLocation).SizeSquared() < threshold * threshold)
		return TESTMOVE_Stopped;

	return TESTMOVE_Moved;
}

ETestMoveResult APawn::swimMove(FVector Delta, FVector &CurrentPosition, AActor* GoalActor, FLOAT threshold)
{
	FVector StartLocation = CurrentPosition;
	FVector Down = FVector(0,0,-1) * MaxStepHeight;
	FVector Up = -1 * Down;
	FCheckResult Hit(1.f);
	FVector CollisionExtent = GetDefaultCollisionSize();

	TestMove(Delta, CurrentPosition, Hit, CollisionExtent);
	if ( GoalActor && (Hit.Actor == GoalActor) )
		return HitGoal(GoalActor);
	if ( !PhysicsVolume->bWaterVolume )
	{
		FVector End = findWaterLine(StartLocation, CurrentPosition);
		if (End != CurrentPosition)
				TestMove(End - CurrentPosition, CurrentPosition, Hit, CollisionExtent);
		return TESTMOVE_Stopped;
	}
	else if (Hit.Time < 1.f) //try to step up
	{
		Delta = Delta * (1.f - Hit.Time);
		TestMove(Up, CurrentPosition, Hit, CollisionExtent);
		TestMove(Delta, CurrentPosition, Hit, CollisionExtent);
		if ( GoalActor && (Hit.Actor == GoalActor) )
			return HitGoal(GoalActor); //bumped into goal
	}

	if ((CurrentPosition - StartLocation).SizeSquared() < threshold * threshold)
		return TESTMOVE_Stopped;

	return TESTMOVE_Moved;
}

/* TryJumpUp()
Check if could jump up over obstruction
*/
UBOOL APawn::TryJumpUp(FVector Dir, FVector Destination, DWORD TraceFlags, UBOOL bNoVisibility)
{
	FVector Out = 14.f * Dir;
	FCheckResult Hit(1.f);
	FVector Up = FVector(0.f,0.f,MaxJumpHeight);

	if ( bNoVisibility )
	{
		// do quick trace check first
		FVector Start = Location + FVector(0.f, 0.f, CylinderComponent->CollisionHeight);
		FVector End = Start + Up;
		GWorld->SingleLineCheck(Hit, this, End, Start, TRACE_World);
		if ( Hit.Time < 1.f )
			End = Hit.Location;
		GWorld->SingleLineCheck(Hit, this, Destination, End, TraceFlags);
		if ( (Hit.Time < 1.f) && (Hit.Actor != Controller->MoveTarget) )
			return FALSE;
	}

	GWorld->SingleLineCheck(Hit, this, Location + Up, Location, TRACE_World, GetCylinderExtent());
	FLOAT FirstHit = Hit.Time;
	if ( FirstHit > 0.5f )
	{
		GWorld->SingleLineCheck(Hit, this, Location + Up * Hit.Time + Out, Location + Up * Hit.Time, TraceFlags, GetCylinderExtent());
		return (Hit.Time == 1.f);
	}
	return FALSE;
}

/* PickWallAdjust()
Check if could jump up over obstruction (only if there is a knee height obstruction)
If so, start jump, and return current destination
Else, try to step around - return a destination 90 degrees right or left depending on traces
out and floor checks

returns TRUE if successful adjustment was found
*/
UBOOL APawn::PickWallAdjust(FVector WallHitNormal, AActor* HitActor)
{
	if ( (Physics == PHYS_Falling) || !Controller )
		return FALSE;

	if ( (Physics == PHYS_Flying) || (Physics == PHYS_Swimming) )
		return Pick3DWallAdjust(WallHitNormal, HitActor);

	DWORD TraceFlags = TRACE_World | TRACE_StopAtAnyHit;
	if ( HitActor && !HitActor->bWorldGeometry )
		TraceFlags = TRACE_AllBlocking | TRACE_StopAtAnyHit;

	// first pick likely dir with traces, then check with testmove
	FCheckResult Hit(1.f);
	FVector ViewPoint = Location + FVector(0.f,0.f,BaseEyeHeight);
	FVector Dir = Controller->DesiredDirection();
	FVector Dest = Controller->GetDestinationPosition();
	FLOAT zdiff = Dir.Z;
	Dir.Z = 0.f;
	FLOAT AdjustDist = 2.5f * CylinderComponent->CollisionRadius;
	AActor *MoveTarget = ( Controller->MoveTarget ? Controller->MoveTarget->AssociatedLevelGeometry() : NULL );

	if ( (zdiff < CylinderComponent->CollisionHeight) && ((Dir | Dir) - CylinderComponent->CollisionRadius * CylinderComponent->CollisionRadius < 0.f) )
	{
		return FALSE;
	}
	FLOAT Dist = Dir.Size();
	if ( Dist == 0.f )
	{
		return FALSE;
	}
	Dir = Dir/Dist;
	GWorld->SingleLineCheck( Hit, this, Dest, ViewPoint, TraceFlags );
	if ( Hit.Actor && (Hit.Actor != MoveTarget) )
		AdjustDist += CylinderComponent->CollisionRadius;

	//look left and right
	FVector Left = FVector(Dir.Y, -1 * Dir.X, 0);
	INT bCheckRight = 0;
	FVector CheckLeft = Left * 1.4f * CylinderComponent->CollisionRadius;
	GWorld->SingleLineCheck(Hit, this, Dest, ViewPoint + CheckLeft, TraceFlags); 
	if ( Hit.Actor && (Hit.Actor != MoveTarget) ) //try right
	{
		bCheckRight = 1;
		Left *= -1;
		CheckLeft *= -1;
		GWorld->SingleLineCheck(Hit, this, Dest, ViewPoint + CheckLeft, TraceFlags); 
	}

	UBOOL bNoVisibility = Hit.Actor && (Hit.Actor != MoveTarget);

	if ( (Physics == PHYS_Walking) && bCanJump && TryJumpUp(Dir, Dest, TraceFlags, bNoVisibility) )
	{
		Controller->JumpOverWall(WallHitNormal);
		return TRUE;
	}

	if ( bNoVisibility )
	{
		return FALSE;
	}

	//try step left or right
	FVector Out = 14.f * Dir;
	Left *= AdjustDist;
	GWorld->SingleLineCheck(Hit, this, Location + Left, Location, TraceFlags, GetCylinderExtent());
	if (Hit.Time == 1.f)
	{
		GWorld->SingleLineCheck(Hit, this, Location + Left + Out, Location + Left, TraceFlags, GetCylinderExtent());
		if (Hit.Time == 1.f)
		{
			Controller->SetAdjustLocation( Location + Left, TRUE );
			return TRUE;
		}
	}
	
	if ( !bCheckRight ) // if didn't already try right, now try it
	{
		CheckLeft *= -1;
		GWorld->SingleLineCheck(Hit, this, Dest, ViewPoint + CheckLeft, TraceFlags); 
		if ( Hit.Time < 1.f )
			return FALSE;
		Left *= -1;
		GWorld->SingleLineCheck(Hit, this, Location + Left, Location, TraceFlags, GetCylinderExtent());
		if (Hit.Time == 1.f)
		{
			GWorld->SingleLineCheck(Hit, this, Location + Left + Out, Location + Left, TraceFlags, GetCylinderExtent());
			if (Hit.Time == 1.f)
			{
				Controller->SetAdjustLocation( Location + Left, TRUE );
				return TRUE;
			}
		}
	}
	return FALSE;
}

/* Pick3DWallAdjust()
pick wall adjust when swimming or flying
*/
UBOOL APawn::Pick3DWallAdjust(FVector WallHitNormal, AActor* HitActor)
{
	DWORD TraceFlags = TRACE_World | TRACE_StopAtAnyHit;
	if ( HitActor && !HitActor->bWorldGeometry )
		TraceFlags = TRACE_AllBlocking | TRACE_StopAtAnyHit;

	// first pick likely dir with traces, then check with testmove
	FCheckResult Hit(1.f);
	FVector ViewPoint = Location + FVector(0.f,0.f,BaseEyeHeight);
	FVector Dir = Controller->DesiredDirection();
	FVector Dest = Controller->GetDestinationPosition();
	FLOAT zdiff = Dir.Z;
	Dir.Z = 0.f;
	FLOAT AdjustDist = 2.5f * CylinderComponent->CollisionRadius;
	AActor *MoveTarget = ( Controller->MoveTarget ? Controller->MoveTarget->AssociatedLevelGeometry() : NULL );

	// if swimming, maybe just want to get out of water
	if ( bCanWalk && (Physics == PHYS_Swimming) && MoveTarget && MoveTarget->PhysicsVolume && !MoveTarget->PhysicsVolume->bWaterVolume )
	{
		FVector Up = FVector(0,0,4.f*CylinderComponent->CollisionHeight) - CylinderComponent->CollisionRadius * (MoveTarget->Location - Location).SafeNormal();
		GWorld->SingleLineCheck(Hit, this, Location + Up, Location, TraceFlags, GetCylinderExtent());
		if (Hit.Time > 0.5f )
		{
			Controller->SetAdjustLocation( Location + Up, TRUE );
			return TRUE;
		}
	}		
	INT bCheckUp = 0;
	if ( zdiff < CylinderComponent->CollisionHeight )
	{
		if ( (Dir | Dir) - CylinderComponent->CollisionRadius * CylinderComponent->CollisionRadius < 0 )
			return FALSE;
		if ( Dir.SizeSquared() < 4 * CylinderComponent->CollisionHeight * CylinderComponent->CollisionHeight )
		{
			FVector Up = FVector(0,0,2.f*CylinderComponent->CollisionHeight);
			bCheckUp = 1;
			if ( Location.Z > Dest.Z )
			{
				bCheckUp = -1;
				Up *= -1;
			}
			GWorld->SingleLineCheck(Hit, this, Location + Up, Location, TraceFlags, GetCylinderExtent());
			if (Hit.Time == 1.f)
			{
				FVector ShortDir = Dir.SafeNormal();
				ShortDir *= CylinderComponent->CollisionRadius;
				GWorld->SingleLineCheck(Hit, this, Location + Up + ShortDir, Location + Up, TraceFlags, GetCylinderExtent());
				if (Hit.Time == 1.f)
				{
					Controller->SetAdjustLocation( Location + Up, TRUE );
					return TRUE;
				}
			}
		}
	}

	FLOAT Dist = Dir.Size();
	if ( Dist == 0.f )
		return FALSE;
	Dir = Dir/Dist;
	GWorld->SingleLineCheck( Hit, this, Dest, ViewPoint, TraceFlags );
	if ( (Hit.Actor != MoveTarget) && (zdiff > 0) )
	{
		FVector Up = FVector(0,0, 2.f*CylinderComponent->CollisionHeight);
		GWorld->SingleLineCheck(Hit, this, Location + 2 * Up, Location, TraceFlags, GetCylinderExtent());
		if (Hit.Time == 1.f)
		{
			Controller->SetAdjustLocation( Location + Up, TRUE );
			return TRUE;
		}
	}

	//look left and right
	FVector Left = FVector(Dir.Y, -1 * Dir.X, 0);
	INT bCheckRight = 0;
	FVector CheckLeft = Left * 1.4f * CylinderComponent->CollisionRadius;
	GWorld->SingleLineCheck(Hit, this, Dest, ViewPoint + CheckLeft, TraceFlags); 
	if ( Hit.Actor != MoveTarget ) //try right
	{
		bCheckRight = 1;
		Left *= -1;
		CheckLeft *= -1;
		GWorld->SingleLineCheck(Hit, this, Dest, ViewPoint + CheckLeft, TraceFlags); 
	}

	if ( Hit.Actor != MoveTarget ) //neither side has visibility
		return FALSE;

	FVector Out = 14.f * Dir;

	//try step left or right
	Left *= AdjustDist;
	GWorld->SingleLineCheck(Hit, this, Location + Left, Location, TraceFlags, GetCylinderExtent());
	if (Hit.Time == 1.f)
	{
		GWorld->SingleLineCheck(Hit, this, Location + Left + Out, Location + Left, TraceFlags, GetCylinderExtent());
		if (Hit.Time == 1.f)
		{
			Controller->SetAdjustLocation( Location + Left, TRUE );
			return TRUE;
		}
	}
	
	if ( !bCheckRight ) // if didn't already try right, now try it
	{
		CheckLeft *= -1;
		GWorld->SingleLineCheck(Hit, this, Dest, ViewPoint + CheckLeft, TraceFlags); 
		if ( Hit.Time < 1.f )
			return FALSE;
		Left *= -1;
		GWorld->SingleLineCheck(Hit, this, Location + Left, Location, TraceFlags, GetCylinderExtent());
		if (Hit.Time == 1.f)
		{
			GWorld->SingleLineCheck(Hit, this, Location + Left + Out, Location + Left, TraceFlags, GetCylinderExtent());
			if (Hit.Time == 1.f)
			{
				Controller->SetAdjustLocation( Location + Left, TRUE );
				return TRUE;
			}
		}
	}

	//try adjust up or down if swimming or flying
	FVector Up = FVector(0,0,2.5f*CylinderComponent->CollisionHeight);

	if ( bCheckUp != 1 )
	{
		GWorld->SingleLineCheck(Hit, this, Location + Up, Location, TraceFlags, GetCylinderExtent());
		if ( Hit.Time > 0.7f )
		{
			GWorld->SingleLineCheck(Hit, this, Location + Up + Out, Location + Up, TraceFlags, GetCylinderExtent());
			if ( (Hit.Time == 1.f) || (Hit.Normal.Z > 0.7f) )
			{
				Controller->SetAdjustLocation( Location + Up, TRUE );
				return TRUE;
			}
		}
	}

	if ( bCheckUp != -1 )
	{
		Up *= -1; //try adjusting down
		GWorld->SingleLineCheck(Hit, this, Location + Up, Location, TraceFlags, GetCylinderExtent());
		if ( Hit.Time > 0.7f )
		{
			GWorld->SingleLineCheck(Hit, this, Location + Up + Out, Location + Up, TraceFlags, GetCylinderExtent());
			if (Hit.Time == 1.f)
			{
				Controller->SetAdjustLocation( Location + Up, TRUE );
				return TRUE;
			}
		}
	}

	return FALSE;
}

/*-----------------------------------------------------------------------------
	Networking functions.
-----------------------------------------------------------------------------*/
/** GetNetPriority()
@param Viewer		PlayerController owned by the client for whom net priority is being determined
@param InChannel	Channel on which this actor is being replicated.
@param Time			Time since actor was last replicated
@return				Priority of this actor for replication
*/
FLOAT APawn::GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, APlayerController* Viewer, UActorChannel* InChannel, FLOAT Time, UBOOL bLowBandwidth)
{
	if ( (this == Viewer->Pawn) || (DrivenVehicle && (DrivenVehicle->Controller == Viewer)) )
	{
		// viewer's pawn, or driver of viewer's vehicle have increased priority
		Time *= 4.f;
	}
	else if ( !bHidden )
	{
		FVector Dir = Location - ViewPos;
		FLOAT DistSq = Dir.SizeSquared();

		// adjust priority based on distance and whether pawn is in front of viewer or is controlled
		if ( (ViewDir | Dir) < 0.f )
		{
			if ( DistSq > NEARSIGHTTHRESHOLDSQUARED )
			Time *= 0.3f;
			else if ( DistSq > CLOSEPROXIMITYSQUARED )
				Time *= 0.5f;
		}
		else if ( Controller && (DistSq < FARSIGHTTHRESHOLDSQUARED) && (Square(ViewDir | Dir) > 0.5f * DistSq) )
		{
			Time *= 2.f;
		}
		else if (DistSq > MEDSIGHTTHRESHOLDSQUARED)
		{
				Time *= 0.5f;
		}
	}
	return NetPriority * Time;
}

/** GetNetPriority()
@param Viewer		PlayerController owned by the client for whom net priority is being determined
@param InChannel	Channel on which this actor is being replicated.
@param Time			Time since actor was last replicated
@return				Priority of this actor for replication
*/
FLOAT AProjectile::GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, APlayerController* Viewer, UActorChannel* InChannel, FLOAT Time, UBOOL bLowBandwidth)
{
	if ( Instigator && (Instigator == Viewer->Pawn) )
		Time *= 4.f; 
	else if ( !bHidden )
	{
		FVector Dir = Location - ViewPos;
		FLOAT DistSq = Dir.SizeSquared();
		if ( bLowBandwidth )
		{
			UBOOL bIsBehindViewer = FALSE;
			if ( (ViewDir | Dir) < 0.f )
			{
				bIsBehindViewer = TRUE;
				if ( DistSq > NEARSIGHTTHRESHOLDSQUARED )
					Time *= 0.2f;
				else if ( DistSq > CLOSEPROXIMITYSQUARED )
					Time *= 0.5f;
			}
			if ( !bIsBehindViewer )
			{
				Dir = Dir.SafeNormal();
				if ( (ViewDir | Dir) > 0.7f )
					Time *= 2.5f;
			}
			if ( DistSq > MEDSIGHTTHRESHOLDSQUARED )
				Time *= 0.2f;
		}
		else if ( (ViewDir | Dir) < 0.f )
		{
			if ( DistSq > NEARSIGHTTHRESHOLDSQUARED )
				Time *= 0.3f;
			else if ( DistSq > CLOSEPROXIMITYSQUARED )
				Time *= 0.5f;
		}
	}

	return NetPriority * Time;
}

UBOOL APawn::SharingVehicleWith(APawn *P)
{
	return ( P && ((Base == P) || (P->Base == this)) );
}

void APawn::PushedBy(AActor* Other)
{
	bForceFloorCheck = TRUE;
}

/** Update controller's view rotation as pawn's base rotates
*/
void APawn::UpdateBasedRotation(FRotator &FinalRotation, const FRotator& ReducedRotation)
{
	FLOAT ControllerRoll = 0;
	if( Controller != NULL && !bIgnoreBaseRotation )
	{
		Controller->OldBasedRotation = Controller->Rotation;
		ControllerRoll = Controller->Rotation.Roll;
		Controller->Rotation += ReducedRotation;
	}

	// If its a pawn, and its not a crawler, remove roll.
	if( !bCrawler )
	{
		FinalRotation.Roll = Rotation.Roll;
		if( Controller )
		{
			Controller->Rotation.Roll = appTrunc(ControllerRoll);
		}
	}
}

void APawn::ReverseBasedRotation()
{
	if (Controller != NULL && !bIgnoreBaseRotation)
	{
		Controller->Rotation = Controller->OldBasedRotation;
	}
}

/*
 * Route finding notification (sent to target)
 * Returns whether only visible anchors should be looked for if EndAnchor is not returned
 * (rather than actually checking if anchor is also reachable)
 */
ANavigationPoint* APawn::SpecifyEndAnchor(APawn* RouteFinder)
{
	ANavigationPoint* EndAnchor = NULL;

	if ( ValidAnchor() )
	{
		// use currently valid anchor
		EndAnchor = Anchor;
	}
	else
	{
		// use this pawn's destination as endanchor for routfinder
		if ( Controller && (Controller->GetStateFrame()->LatentAction == UCONST_LATENT_MOVETOWARD) )
		{
			EndAnchor = Cast<ANavigationPoint>(Controller->MoveTarget);
		}
	}

	// maybe we can just use a recently valid anchor for this pawn
	FLOAT MaxAnchorAge = (Physics == PHYS_Falling) ? 1.f : 0.25f;
	if ( !EndAnchor && LastAnchor && (RouteFinder->Anchor != LastAnchor) && (GWorld->GetTimeSeconds() - LastValidAnchorTime < MaxAnchorAge)
		&& Controller && Controller->LineOfSightTo(LastAnchor) )
	{
		EndAnchor = LastAnchor;
	}

	if(EndAnchor != NULL && EndAnchor->IsUsableAnchorFor(RouteFinder))
	{
		return EndAnchor;
	}
	else
	{
		return NULL;
	}	
}

/**
  * RETURN TRUE if will accept as anchor visible nodes that are not reachable
  */
UBOOL APawn::AnchorNeedNotBeReachable()
{
	if ( Physics == PHYS_Falling )
	{
		 return TRUE;
	}

	APlayerController *PC = Controller ? Controller->GetAPlayerController() : NULL;
	if ( PC && (Location == PC->FailedPathStart) )
	{
		return TRUE;
	}
	return FALSE;
}

/*
 * Notify actor of anchor finding result
 * @PARAM EndAnchor is the anchor found
 * @PARAM RouteFinder is the pawn which requested the anchor finding
 */
void APawn::NotifyAnchorFindingResult(ANavigationPoint* EndAnchor, APawn* RouteFinder)
{
	if ( EndAnchor )
	{
		// save valid anchor info
		LastValidAnchorTime = GWorld->GetTimeSeconds();
		LastAnchor = EndAnchor;
	}
}

/////////////////// MATINEE support


/** Update list of AnimSets for this Pawn */
void APawn::UpdateAnimSetList()
{
	// Restore default AnimSets
	RestoreAnimSetsToDefault();

	// Build New list
	BuildAnimSetList();

	// Force AnimTree to be updated with new AnimSets array
	if( Mesh )
	{
		Mesh->bDisableWarningWhenAnimNotFound = TRUE;
		Mesh->UpdateAnimations();
		Mesh->bDisableWarningWhenAnimNotFound = FALSE;
	}

	// Script event after the list is built.
	eventAnimSetListUpdated();
}

/** Build AnimSet list, called by UpdateAnimSetList() */
void APawn::BuildAnimSetList()
{
	// This will be done only once, the first time.
	if( Mesh )
	{
		Mesh->SaveAnimSets();
	}
	
	// Add the AnimSets from Matinee
	for(INT i=0; i<InterpGroupList.Num(); i++)
	{
		UInterpGroup* AnInterpGroup = InterpGroupList(i);
		if( AnInterpGroup )
		{
			AddAnimSets( AnInterpGroup->GroupAnimSets );
		}
	}

	// Let script also add animsets.
	eventBuildScriptAnimSetList();
}

/** 
 * Add a given list of anim sets on the top of the list (so they override the other ones 
 * !! Only use within BuildAnimSetList() !! 
 */
void APawn::AddAnimSets(const TArray<class UAnimSet*>& CustomAnimSets)
{
	if( Mesh )
	{
		for(INT i=0; i<CustomAnimSets.Num(); i++)
		{
			Mesh->AnimSets.AddItem( CustomAnimSets(i) );
		}
	}
}

/** Restore Mesh's AnimSets to defaults, as defined in the default properties */
void APawn::RestoreAnimSetsToDefault()
{
	// In Matinee we don't have script access, so do it in native code.
	// But otherwise, prefer Script.
	if( (GIsEditor && !GIsGame) || !eventRestoreAnimSetsToDefault() )
	{
		if( Mesh )
		{
			Mesh->RestoreSavedAnimSets();
		}
	}
}

/**
*	Called by Matinee when we open it to start controlling animation on this Actor.
*	Is also called again when the GroupAnimSets array changes in Matinee, so must support multiple calls.
*/
void APawn::PreviewBeginAnimControl(UInterpGroup* InInterpGroup)
{
	if( !Mesh )
	{
		debugf(TEXT("AGearPawn::PreviewBeginAnimControl, no Mesh!!!") );
		return;
	}

	// In the editor we don't have access to Script, so cache slot nodes here :(
	SlotNodes.Empty();

	if ( Mesh->Animations == NULL )
	{
		// We need an AnimTree in Matinee in the editor to preview the animations, so instance one now if we don't have one.
		// This function can get called multiple times, but this is safe - will only instance the first time.
		if ( Mesh->AnimTreeTemplate )
		{
			Mesh->SetAnimTreeTemplate(Mesh->AnimTreeTemplate);
		}
		else
		{
			Mesh->DeleteAnimTree();
			Mesh->Animations = ConstructObject<UAnimNodeSequence>(UAnimNodeSequence::StaticClass());
			if( Mesh->Animations )
			{
				Mesh->InitAnimTree(TRUE);
			}
		}
	}

	if( Mesh->AnimTreeTemplate && Mesh->Animations )
	{
		TArray<UAnimNode*> Nodes;
		Mesh->Animations->GetNodesByClass(Nodes, UAnimNodeSlot::StaticClass());

		for(INT i=0; i<Nodes.Num(); i++)
		{
			UAnimNodeSlot* SlotNode = Cast<UAnimNodeSlot>(Nodes(i));
			if( SlotNode )
			{
				SlotNodes.AddItem(SlotNode);
				if (SlotNode->NodeName == NAME_None)
				{
					// this is only for preview, so I'm setting default animslot name for it. 
					SlotNode->NodeName = FName(*GConfig->GetStr(TEXT("MatineePreview"), TEXT("DefaultAnimSlotName"), GEditorIni));
				}
			}
		}
	}

	// Add in AnimSets
	MAT_BeginAnimControl(InInterpGroup);

	// Toggle on cinematic weighting for the skeletal mesh component while previewing
	for ( INT LODIdx = 0; LODIdx < Mesh->LODInfo.Num(); ++LODIdx )
	{
		if ( Mesh->LODInfo(LODIdx).InstanceWeightUsage == IWU_FullSwap )
		{
			Mesh->ToggleInstanceVertexWeights( TRUE, LODIdx );
		}
	}
}

/** Start AnimControl. Add required AnimSets. */
void APawn::MAT_BeginAnimControl(UInterpGroup* InInterpGroup)
{
	if( !Mesh )
	{
		debugf(TEXT("MAT_BeginAnimControl, no Mesh!!!") );
		return;
	}

	// Add our InterpGroup to the list
	InterpGroupList.AddItem(InInterpGroup);

	// Update AnimSet list.
	UpdateAnimSetList();
}

void APawn::PreviewFinishAnimControl(UInterpGroup* InInterpGroup)
{
	if( !Mesh )
	{
		debugf(TEXT("AGearPawn::PreviewFinishAnimControl, no Mesh!!!") );
		return;
	}

	MAT_FinishAnimControl(InInterpGroup);

	// When done in Matinee in the editor, drop the AnimTree instance.
	Mesh->DeleteAnimTree();

	// In the editor, free up the slot nodes.
	SlotNodes.Empty();

	// Update space bases to reset it back to ref pose
	Mesh->UpdateSkelPose(0.f, FALSE);
	Mesh->ConditionalUpdateTransform();

	// Toggle off cinematic weighting for the skeletal mesh component while previewing
	for ( INT LODIdx = 0; LODIdx < Mesh->LODInfo.Num(); ++LODIdx )
	{
		if ( Mesh->LODInfo(LODIdx).InstanceWeightUsage == IWU_FullSwap )
		{
			Mesh->ToggleInstanceVertexWeights( FALSE, LODIdx );
		}
	}
}

/** End AnimControl. Release required AnimSets */
void APawn::MAT_FinishAnimControl(UInterpGroup* InInterpGroup)
{
	// Clear references to AnimSets which were added.
	FAnimSlotInfo SlotNodeInfo;
	// clear the weight
	SlotNodeInfo.ChannelWeights.AddItem(0.0f);

	for(INT SlotIdx=0; SlotIdx<SlotNodes.Num(); SlotIdx++)
	{
		UAnimNodeSlot* SlotNode = SlotNodes(SlotIdx);
		if( SlotNode )
		{	
			SlotNode->MAT_SetAnimWeights(SlotNodeInfo);
			//unset root motion
			SlotNode->SetRootBoneAxisOption(RBA_Default, RBA_Default, RBA_Default);
			SlotNode->bIsBeingUsedByInterpGroup = FALSE;
		}
	}

	// Take out our group from the list
	InterpGroupList.RemoveItem(InInterpGroup);

	// Update AnimSet list.
	UpdateAnimSetList();

	// detach if any arrow component that has been attached to the mesh
	if (GIsEditor && !GIsGame && Mesh)
	{
		Mesh->DetachAnyOf(UArrowComponent::StaticClass());
	}
}

void APawn::PreviewSetAnimPosition(FName SlotName, INT ChannelIndex, FName InAnimSeqName, FLOAT InPosition, UBOOL bLooping, UBOOL bFireNotifies, UBOOL bEnableRootMotion, FLOAT DeltaTime)
{
	if( !Mesh )
	{
		debugf(TEXT("AGearPawn::PreviewSetAnimPosition, no Mesh!!!") );
		return;
	}

	if ( SlotNodes.Num() > 0 )
	{
		MAT_SetAnimPosition(SlotName, ChannelIndex, InAnimSeqName, InPosition, bFireNotifies, bLooping, bEnableRootMotion);
	}
	else
	{
		UAnimNodeSequence* SeqNode = Cast<UAnimNodeSequence>(Mesh->Animations);

		// Do nothing if no anim tree or its not an AnimNodeSequence
		if(!SeqNode)
		{
			return;
		}

		if(SeqNode->AnimSeqName != InAnimSeqName)
		{
			SeqNode->SetAnim(InAnimSeqName);
			// first time just clear prev/currenttime
			SeqNode->SetPosition(InPosition, FALSE);

			if ( Mesh )
			{
				if ( bEnableRootMotion )
				{
					Mesh->RootMotionMode = RMM_Translate;
					SeqNode->SetRootBoneAxisOption(RBA_Translate, RBA_Translate, RBA_Translate);
					Mesh->RootMotionRotationMode = RMRM_RotateActor;
					SeqNode->SetRootBoneRotationOption(RRO_Extract, RRO_Extract, RRO_Extract);
				}
				else
				{
					Mesh->RootMotionMode = RMM_Ignore;
					SeqNode->SetRootBoneAxisOption(RBA_Default, RBA_Default, RBA_Default);				
					Mesh->RootMotionRotationMode = RMRM_Ignore;
					SeqNode->SetRootBoneRotationOption(RRO_Default, RRO_Default, RRO_Default);
				}
			}
		}

		SeqNode->Rate = 1.f;
		SeqNode->bLooping = bLooping;
		// set previous time here since there is other way to set this
		SeqNode->PreviousTime = SeqNode->CurrentTime;
		SeqNode->SetPosition(InPosition, bFireNotifies);
	}

	// Update space bases so new animation position has an effect.
	Mesh->UpdateSkelPose(DeltaTime, FALSE);
	Mesh->ConditionalUpdateTransform();
}

/** Update AnimTree from track info */
void APawn::MAT_SetAnimPosition(FName SlotName, INT ChannelIndex, FName InAnimSeqName, FLOAT InPosition, UBOOL bFireNotifies, UBOOL bLooping, UBOOL bEnableRootMotion)
{
	if( Mesh )
	{
		// Ensure anims are updated correctly in cinematics even when the mesh isn't rendered.
		Mesh->LastRenderTime = GWorld->GetTimeSeconds();
	}

	// Forward animation positions to slots. They will forward to relevant channels.
	for(INT i=0; i<SlotNodes.Num(); i++)
	{
		UAnimNodeSlot* SlotNode = SlotNodes(i);
		if( SlotNode && SlotNode->NodeName == SlotName )
		{	
			SlotNode->MAT_SetAnimPosition(ChannelIndex, InAnimSeqName, InPosition, bFireNotifies, bLooping, bEnableRootMotion);
		}
	}
}

void APawn::PreviewSetAnimWeights(TArray<FAnimSlotInfo>& SlotInfos)
{
	MAT_SetAnimWeights(SlotInfos);
}

/** Update AnimTree from track weights */
void APawn::MAT_SetAnimWeights(const TArray<FAnimSlotInfo>& SlotInfos)
{
#if 0
	//debugf( TEXT("-- SET ANIM WEIGHTS ---") );
	FLOAT fTotalWeight = 0.f;
	for(INT i=0; i<SlotInfos.Num(); i++)
	{
		//debugf( TEXT("SLOT: %s"), *SlotInfos(i).SlotName.GetNameString() );
		for(INT j=0; j<SlotInfos(i).ChannelWeights.Num(); j++)
		{
			//debugf( TEXT("   CHANNEL %d: %1.3f"), j, SlotInfos(i).ChannelWeights(j) );
			fTotalWeight += SlotInfos(i).ChannelWeights(j);
		}
	}


#endif

	// Forward channel weights to relevant slot(s)
	for(INT SlotInfoIdx=0; SlotInfoIdx<SlotInfos.Num(); SlotInfoIdx++)
	{
		const FAnimSlotInfo& SlotInfo = SlotInfos(SlotInfoIdx);

		for(INT SlotIdx=0; SlotIdx<SlotNodes.Num(); SlotIdx++)
		{
			UAnimNodeSlot* SlotNode = SlotNodes(SlotIdx);
			if( SlotNode && SlotNode->NodeName == SlotInfo.SlotName )
			{	
				SlotNode->MAT_SetAnimWeights(SlotInfo);
				SlotNode->bIsBeingUsedByInterpGroup = TRUE;
			}
			else
			{
				SlotNode->bIsBeingUsedByInterpGroup = FALSE;
			}
		}
	}
}

/* This is where we add information on slots and channels to the array that was passed in. */
/** Used to provide information on the slots that this Actor provides for animation to Matinee. */
void APawn::GetAnimControlSlotDesc(TArray<struct FAnimSlotDesc>& OutSlotDescs)
{
	if( !Mesh )
	{
		debugf(TEXT("AGearPawn::GetAnimControlSlotDesc, no Mesh!!!") );
		return;
	}

	if( !Mesh->Animations )
	{
		// fail
		appMsgf(AMT_OK, TEXT("SkeletalMeshActorMAT has no AnimTree Instance."));
		return;
	}

	// Find all AnimMATSlotNodes in the AnimTree
	for(INT i=0; i<SlotNodes.Num(); i++)
	{
		// Number of channels available on this slot node.
		const INT NumChannels = SlotNodes(i)->Children.Num() - 1;

		if( SlotNodes(i)->NodeName != NAME_None && NumChannels > 0 )
		{
			// Add a new element
			const INT Index = OutSlotDescs.Add(1);
			OutSlotDescs(Index).SlotName	= SlotNodes(i)->NodeName;
			OutSlotDescs(Index).NumChannels	= NumChannels;
		}
	}
}

/** Function used to control FaceFX animation in the editor (Matinee). */
void APawn::PreviewUpdateFaceFX(UBOOL bForceAnim, const FString& GroupName, const FString& SeqName, FLOAT InPosition)
{
	//debugf( TEXT("GroupName: %s  SeqName: %s  InPos: %f"), *GroupName, *SeqName, InPosition );

	check(Mesh);

	// Scrubbing case
	if(bForceAnim)
	{
#if WITH_FACEFX
		if(Mesh->FaceFXActorInstance)
		{
			// FaceFX animations start at a neagative time, where zero time is where the sound begins ie. a pre-amble before the audio starts.
			// Because Matinee just think of things as starting at zero, we need to determine this start offset to place ourselves at the 
			// correct point in the FaceFX animation.

			// Get the FxActor
			FxActor* fActor = Mesh->FaceFXActorInstance->GetActor();

			// Find the Group by name
			FxSize GroupIndex = fActor->FindAnimGroup(TCHAR_TO_ANSI(*GroupName));
			if(FxInvalidIndex != GroupIndex)
			{
				FxAnimGroup& fGroup = fActor->GetAnimGroup(GroupIndex);

				// Find the animation by name
				FxSize SeqIndex = fGroup.FindAnim(TCHAR_TO_ANSI(*SeqName));
				if(FxInvalidIndex != SeqIndex)
				{
					const FxAnim& fAnim = fGroup.GetAnim(SeqIndex);

					// Get the offset (will be 0.0 or negative)
					FLOAT StartOffset = fAnim.GetStartTime();

					// Force FaceFX to a particular point
					Mesh->FaceFXActorInstance->ForceTick(TCHAR_TO_ANSI(*SeqName), TCHAR_TO_ANSI(*GroupName), InPosition + StartOffset);

					// Update the skeleton, morphs etc.
					// The FALSE here stops us from calling regular Tick on the FaceFXActorInstance, and clobbering the ForceTick we just did.
					Mesh->UpdateSkelPose(0.f, FALSE);
					Mesh->ConditionalUpdateTransform();
				}
			}
		}
#endif
	}
	// Playback in Matinee case
	else
	{
		Mesh->UpdateSkelPose();
		Mesh->ConditionalUpdateTransform();
	}
}

/** Used by Matinee playback to start a FaceFX animation playing. */
void APawn::PreviewActorPlayFaceFX(const FString& GroupName, const FString& SeqName, USoundCue* InSoundCue)
{
	check(Mesh);
	// FaceFXAnimSetRef set to NULL because matinee mounts sets during initialization
	Mesh->PlayFaceFXAnim(NULL, SeqName, GroupName, InSoundCue);
}

/** Used by Matinee to stop current FaceFX animation playing. */
void APawn::PreviewActorStopFaceFX()
{
	check(Mesh);
	Mesh->StopFaceFXAnim();
}

/** Used in Matinee to get the AudioComponent we should play facial animation audio on. */
UAudioComponent* APawn::PreviewGetFaceFXAudioComponent()
{
	// FIXME:
	return NULL;
}

/** Get the UFaceFXAsset that is currently being used by this Actor when playing facial animations. */
class UFaceFXAsset* APawn::PreviewGetActorFaceFXAsset()
{
	check(Mesh);
	if(Mesh->SkeletalMesh)
	{
		return Mesh->SkeletalMesh->FaceFXAsset;
	}

	return NULL;
}

/** Called each frame by Matinee to update the weight of a particular MorphNodeWeight. */
void APawn::PreviewSetMorphWeight(FName MorphNodeName, FLOAT MorphWeight)
{
	MAT_SetMorphWeight(MorphNodeName, MorphWeight);
	Mesh->UpdateSkelPose(0.f, FALSE);
	Mesh->ConditionalUpdateTransform();
}

/** Called each frame by Matinee to update the scaling on a SkelControl. */
void APawn::PreviewSetSkelControlScale(FName SkelControlName, FLOAT Scale)
{
	MAT_SetSkelControlScale(SkelControlName, Scale);
	Mesh->UpdateSkelPose(0.f, FALSE);
	Mesh->ConditionalUpdateTransform();
}

/** Called each frame by Matinee to update the controlstrength on a SkelControl. */
void APawn::SetSkelControlStrength(FName SkelControlName, FLOAT ControlStrength)
{
	MAT_SetSkelControlStrength(SkelControlName, ControlStrength);
	Mesh->UpdateSkelPose(0.f, FALSE);
	Mesh->ConditionalUpdateTransform();
}


/** Called each from while the Matinee action is running, to set the animation weights for the actor. */
void APawn::SetAnimWeights( const TArray<struct FAnimSlotInfo>& SlotInfos )
{
	MAT_SetAnimWeights( SlotInfos );
}

void APawn::MAT_SetMorphWeight(FName MorphNodeName,FLOAT MorphWeight)
{
	if(Mesh)
	{
		UMorphNodeWeight* WeightNode = Cast<UMorphNodeWeight>(Mesh->FindMorphNode(MorphNodeName));
		if(WeightNode)
		{
			WeightNode->SetNodeWeight(MorphWeight);
		}
	}
}

void APawn::MAT_SetSkelControlScale(FName SkelControlName,FLOAT Scale)
{
	if(Mesh)
	{
		USkelControlBase* Control = Mesh->FindSkelControl(SkelControlName);
		if(Control)
		{
			Control->BoneScale = Scale;
		}
	}
}

void APawn::MAT_SetSkelControlStrength(FName SkelControlName,FLOAT ControlStrength)
{
	if(Mesh)
	{
		USkelControlBase* Control = Mesh->FindSkelControl(SkelControlName);
		if(Control)
		{
			Control->SetSkelControlStrength(ControlStrength, 0.f);
		}
	}
}
/* epic ===============================================
* ::GetTeamNum
*
 * Queries the PRI and returns our current team index.
* =====================================================
*/
BYTE APawn::GetTeamNum()
{
	if ( Controller )
	{
		return Controller->GetTeamNum();
	}
	else if ( PlayerReplicationInfo )
	{
		return (PlayerReplicationInfo->Team) ? PlayerReplicationInfo->Team->TeamIndex : 255;
	}
	else if ( DrivenVehicle )
	{
		return DrivenVehicle->GetTeamNum();
	}
	else if ( Base && Cast<APawn>(Base) )
	{
		return Cast<APawn>(Base)->GetTeamNum();
	}
	else
	{
		return eventScriptGetTeamNum();
	}
}

FVector APawn::GetPawnViewLocation()
{
	return Location + FVector(0.0f,0.0f,1.0f) * BaseEyeHeight;
}

FRotator APawn::GetViewRotation()
{
	APlayerController* PC = NULL;

	if ( Controller != NULL )
	{
		return Controller->Rotation;
	}
	else if ( Role < ROLE_Authority )
	{
		// check if being spectated
		
		for( INT Idx=0; Idx<GEngine->GamePlayers.Num(); Idx++ )
		{
			if (GEngine->GamePlayers(Idx) && GEngine->GamePlayers(Idx)->Actor)
			{
				PC = Cast<APlayerController>(GEngine->GamePlayers(Idx)->Actor);
				if(PC && PC->ViewTarget == this)
				{
					return PC->BlendedTargetViewRotation;
				}
			}
		}
	}

	return Rotation;

}

/**
 * Checks whether this pawn needs to have its base ticked first and does so if requested
 *
 * @return TRUE if the actor was ticked, FALSE if it was aborted (e.g. because it's in stasis)
 */
UBOOL APawn::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	if (Base != NULL && bNeedsBaseTickedFirst && Base->bTicked != GWorld->Ticked && Base->WantsTick())
	{
		if (TickGroup == Base->TickGroup)
		{
			// Make sure the base is ticked so there isn't an off by one frame
			Base->Tick(DeltaTime,TickType);
		}
		else
		{
			debugfSlow(TEXT("Potential problem ticking base across tick groups: this (%s), base (%s)"),
				*GetName(),
				*Base->GetName());
		}
	}
	return Super::Tick(DeltaTime,TickType);
}

/**
 *	Initializes the projectile with the given direction.
 *
 *	@param	Direction		The direction the projectile is heading
 */
void AProjectile::Init(FVector Direction)
{
	SetRotation(Direction.Rotation());
	Velocity = Speed * Direction;
}

/**
 * Queries the PRI and returns our current team index.
 */
BYTE AProjectile::GetTeamNum()
{
	if ( InstigatorController )
	{
		return InstigatorController->GetTeamNum();
	}
	else if ( Instigator )
	{
		return Instigator->GetTeamNum();
	}
	else
	{
		return 255;
	}
}

void APawn::SetRootMotionInterpCurrentTime( FLOAT inTime, FLOAT DeltaTime, UBOOL bUpdateSkelPose )
{
	RootMotionInterpCurrentTime = inTime;
	if( bUpdateSkelPose && Mesh != NULL )
	{
		FBoneAtom	ExtractedRootMotionDelta	= FBoneAtom::Identity;
		INT			bHasRootMotion				= 0;
		Mesh->ProcessRootMotion( DeltaTime, ExtractedRootMotionDelta, bHasRootMotion );
	}
}


void AMatineePawn::ReplacePreviewMesh(USkeletalMesh * NewPreviewMesh)
{
#if WITH_EDITORONLY_DATA
	if (GIsEditor)
	{
		PreviewMesh = NewPreviewMesh;
		Mesh->SetSkeletalMesh(PreviewMesh);
	}
#endif // WITH_EDITORONLY_DATA
}

/** Set a ScalarParameter to Interpolate */
void APawn::SetScalarParameterInterp(const struct FScalarParameterInterpStruct& ScalarParameterInterp)
{
	// See if we are already interpolating this parameter name.
	// If so delete it. Only have one interpolation at a time per parameter name.
	for(INT ParamIndex=0; ParamIndex<ScalarParameterInterpArray.Num(); ParamIndex++)
	{
		if( ScalarParameterInterpArray(ParamIndex).ParameterName == ScalarParameterInterp.ParameterName )
		{
			ScalarParameterInterpArray.Remove(ParamIndex, 1);
		}
	}

	// Add our item!
	ScalarParameterInterpArray.AddItem(ScalarParameterInterp);
}

/** Handle MaterialInstanceConstant Parameter interpolation. */
void APawn::UpdateScalarParameterInterp(FLOAT DeltaTime)
{
	if( Mesh && ScalarParameterInterpArray.Num() > 0 )
	{
		for(INT MaterialIndex=0; MaterialIndex<Mesh->GetNumElements(); MaterialIndex++)
		{
			UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Mesh->GetMaterial(MaterialIndex));
			if( MIC != NULL && MIC->IsInMapOrTransientPackage())
			{
				for(INT ParamIndex=ScalarParameterInterpArray.Num()-1; ParamIndex>=0; ParamIndex--)
				{
					FScalarParameterInterpStruct& ParamStruct = ScalarParameterInterpArray(ParamIndex);
					if( ParamStruct.WarmupTime > 0.f )
					{
						if( ParamStruct.WarmupTime >= DeltaTime )
						{
							ParamStruct.WarmupTime -= DeltaTime;
							continue;
						}
						ScalarParameterInterpArray(ParamIndex).WarmupTime = 0.f;
					}

					// Save it off, as we might remove that entry if we reach the end of the interp.
					FName const ParamName = ScalarParameterInterpArray(ParamIndex).ParameterName;

					FLOAT Value;
					if( MIC->GetScalarParameterValue(ParamName, Value) == FALSE )
					{
						Value = 0.f;
					}

					if( ScalarParameterInterpArray(ParamIndex).InterpTime >= DeltaTime )
					{
						const FLOAT BlendDelta = (ScalarParameterInterpArray(ParamIndex).ParameterValue - Value); 
						Value += (BlendDelta / ScalarParameterInterpArray(ParamIndex).InterpTime) * DeltaTime;
						ScalarParameterInterpArray(ParamIndex).InterpTime -= DeltaTime;
					}
					else
					{
						// Reached the end, snap to desired value, and kill interp entry.
						Value = ScalarParameterInterpArray(ParamIndex).ParameterValue;
						ScalarParameterInterpArray.Remove(ParamIndex, 1);
					}

					// Set new parameter value.
					MIC->SetScalarParameterValue(ParamName, Value);
				}
			}
		}
	}
}
