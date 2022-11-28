/*=============================================================================
	UnNavigationPoint.cpp:

  NavigationPoint and subclass functions

	Copyright 2001-2004 Epic Games, Inc. All Rights Reserved.

  	Revision history:
		* Created by Steven Polge
=============================================================================*/

#include "EnginePrivate.h"
#include "UnPath.h"

void ANavigationPoint::SetVolumes(const TArray<class AVolume*>& Volumes)
{
	Super::SetVolumes( Volumes );

	if ( PhysicsVolume )
		bMayCausePain = (PhysicsVolume->DamagePerSec != 0);
}

void ANavigationPoint::SetVolumes()
{
	Super::SetVolumes();

	if ( PhysicsVolume )
		bMayCausePain = (PhysicsVolume->DamagePerSec != 0);

}

UBOOL ANavigationPoint::CanReach(ANavigationPoint *Dest, FLOAT Dist, UBOOL bUseFlag)
{
	if ( (bUseFlag && bCanReach) || (this == Dest) )
	{
		bCanReach = true;
		return true;
	}
	if ( visitedWeight >= Dist )
		return false;

	visitedWeight = Dist;
	if ( Dist <= 1.f )
		return false;
	
	for ( INT i=0; i<PathList.Num(); i++ )
		if ( (PathList(i)->reachFlags & R_PROSCRIBED) == 0 )
		{
			if( PathList(i)->Distance <= 1.f )
			{
				debugf(TEXT("%s negative or zero distance to %s!"), GetName(),PathList(i)->End->GetName());
				GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf(TEXT("negative or zero distance to %s!"), PathList(i)->End->GetName()));
			}
			else if ( PathList(i)->End->CanReach(Dest, Dist - PathList(i)->Distance, false) )
			{
				bCanReach = true;
			return true;
		}
		}

	return false;
}

UBOOL ANavigationPoint::ReviewPath(APawn* Scout)
{
	if ( bMustBeReachable )
	{
		for ( ANavigationPoint* M=Level->NavigationPointList; M!=NULL; M=M->nextNavigationPoint )
			M->bCanReach = false;

		// check that all other paths can reach me
		INT NumFailed = 0;
		for ( ANavigationPoint* N=Level->NavigationPointList; N!=NULL; N=N->nextNavigationPoint )
		{
			for ( ANavigationPoint* M=Level->NavigationPointList; M!=NULL; M=M->nextNavigationPoint )
				M->visitedWeight = 0.f;
			if ( !N->CanReach(this, 10000000.f, true) )
			{
				GWarn->MapCheck_Add( MCTYPE_ERROR, N, *FString::Printf(TEXT("Cannot reach %s from this node!"), GetName()));
				NumFailed++;
				if ( NumFailed > 8 )
					break;
			}
		}
	}

	// check that there aren't any really long paths from here
	for ( INT i=0; i<PathList.Num(); i++ )
		if ( ((PathList(i)->reachFlags & (R_PROSCRIBED | R_FORCED | R_JUMP)) == 0) && (PathList(i)->Distance > 1000.f)
			&& (Abs(PathList(i)->Start->Location.Z - PathList(i)->End->Location.Z) < 500.f) )
		{
			// check if alternate route
			UBOOL bFoundRoute = false;
			for ( ANavigationPoint* M=Level->NavigationPointList; M!=NULL; M=M->nextNavigationPoint )
			{
				M->visitedWeight = 0.f;
				M->bCanReach = false;
			}

			for ( INT j=0; j<PathList.Num(); j++ )
				if ( (PathList(j)->Distance < 700.f)
					&& PathList(j)->End->CanReach(PathList(i)->End,::Max((PathList(i)->Distance - PathList(j)->Distance) * 1.25f,700.f), false) )
				{
					bFoundRoute = true;
					break;
				}
			if ( !bFoundRoute )
				GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("Path to %s is very long - add a pathnode in between."), PathList(i)->End->GetName()));
		}
	// do walks from this path, check that when lose this path, find another
	// FIXME implement

	return true;
}

UBOOL APathNode::ReviewPath(APawn* Scout)
{
	// check if should be JumpDest
	for ( INT i=0; i<PathList.Num(); i++ )
		if ( (PathList(i)->reachFlags & R_PROSCRIBED) == 0 )
		PathList(i)->End->CheckSymmetry(this);

	Super::ReviewPath(Scout);
	return true;
}

void APathNode::CheckSymmetry(ANavigationPoint* Other)
{
	for ( INT i=0; i<PathList.Num(); i++ )
		if ( (PathList(i)->End == Other) && ((PathList(i)->reachFlags & R_PROSCRIBED) == 0) )
			return;

	// check for short enough alternate path to warrant no symmetry
	FLOAT Dist = (Location - Other->Location).Size();
	for ( ANavigationPoint* N=Level->NavigationPointList; N!=NULL; N=N->nextNavigationPoint )
	{
		N->bCanReach = false;
		N->visitedWeight = 0.f;
	}
	if ( CanReach(Other,Dist * 1.5f * PATHPRUNING,false) )
		return;

	GWarn->MapCheck_Add( MCTYPE_ERROR, Other, *FString::Printf(TEXT("Should be JumpDest for %s!"), GetName()));
}

UReachSpec* ANavigationPoint::GetReachSpecTo(ANavigationPoint *Nav)
{
	for (INT i=0; i<PathList.Num(); i++ )
		if ( PathList(i)->End == Nav )
			return PathList(i);
	return NULL;
}

AActor* AActor::AssociatedLevelGeometry()
{
	if ( bWorldGeometry )
		return this;

	return NULL;
}

UBOOL AActor::HasAssociatedLevelGeometry(AActor *Other)
{
	return ( bWorldGeometry && (Other == this) );
}

/* if navigationpoint is moved, paths are invalid
*/
void ANavigationPoint::PostEditMove()
{
	Level->bPathsRebuilt = 0;
	bPathsChanged = true;
}

/* if navigationpoint is spawned, paths are invalid
*/
void ANavigationPoint::Spawned()
{
	Level->bPathsRebuilt = 0;
	bPathsChanged = true;
}

/* if navigationpoint is spawned, paths are invalid
NOTE THAT NavigationPoints should not be destroyed during gameplay!!!
*/
void ANavigationPoint::Destroy()
{
	AActor::Destroy();

	if ( !Level->bBegunPlay && GIsEditor )
	{
		Level->bPathsRebuilt = 0;

		// clean up reachspecs referring to this NavigationPoint
		for ( INT i=0; i<PathList.Num(); i++ )
			if ( PathList(i) )
				PathList(i)->Start = NULL;

		for ( INT i=0; i<GetLevel()->Actors.Num(); i++ )
		{
			ANavigationPoint *Nav = Cast<ANavigationPoint>(GetLevel()->Actors(i));
			if ( Nav )
			{
				for ( INT j=0; j<Nav->PathList.Num(); j++ )
					if ( Nav->PathList(j) && Nav->PathList(j)->End == this )
					{
						Nav->PathList(j)->bPruned = true;
						Nav->PathList(j)->End = NULL;
					}
				Nav->CleanUpPruned();
			}
		}
	}

}

void ANavigationPoint::CleanUpPruned()
{
	for ( INT i=PathList.Num()-1; i>=0; i-- )
		if ( PathList(i) && PathList(i)->bPruned )
			PathList.Remove(i);
	PathList.Shrink();
}

/* ProscribedPathTo()
returns 2 if no path is permited because L.D. proscribed it
returns 1 if no path from this to dest is permited for other reasons
*/
INT ANavigationPoint::ProscribedPathTo(ANavigationPoint *Nav)
{
	if ( (bOneWayPath && (((Nav->Location - Location) | Rotation.Vector()) <= 0))
		|| ((Location - Nav->Location).SizeSquared() > MAXPATHDISTSQ) )
		return 1;
					
	// see if destination is in list of proscribed paths
	for (INT idx = 0; idx < ProscribedPaths.Num(); idx++)
	{
		if (Nav == ProscribedPaths(idx))
		{
			return 2;
		}
	}
	return 0;
}

INT ALadder::ProscribedPathTo(ANavigationPoint *Nav)
{
	ALadder *L = Cast<ALadder>(Nav);

	if ( L && (MyLadder == L->MyLadder) )
		return 1;

	return ANavigationPoint::ProscribedPathTo(Nav);
}

UBOOL ANavigationPoint::ShouldBeBased()
{
	return ( !PhysicsVolume->bWaterVolume && !bNotBased );
}

/**
 * Builds a forced reachspec from this navigation point to the
 * specified navigation point.
 */
void ANavigationPoint::ForcePathTo(ANavigationPoint *Nav, AScout *Scout)
{
	// if specified a valid nav point
	if (Nav != NULL &&
		Nav != this)
	{
		// search for the scout if not specified
		if (Scout == NULL)
		{
			Scout = FPathBuilder::GetScout(GetLevel());
		}
		if (Scout != NULL)
		{
			// create the forced spec
			UReachSpec *newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
			newSpec->Init();
			FVector CommonSize = Scout->GetSize(FName(TEXT("Common"),FNAME_Find));
			newSpec->CollisionRadius = CommonSize.X;
			newSpec->CollisionHeight = CommonSize.Y;
			newSpec->reachFlags = R_FORCED;
			newSpec->Start = this;
			newSpec->End = Nav;
			newSpec->Distance = (INT)((Location - Nav->Location).Size());
			newSpec->bForced = true;
			// and add the spec to the path list
			PathList.AddItem(newSpec);
		}
	}
}

/**
 * Builds a proscribed reachspec fromt his navigation point to the
 * specified navigation point.
 */
void ANavigationPoint::ProscribePathTo(ANavigationPoint *Nav, AScout *Scout)
{
	// if specified a valid nav point
	if (Nav != NULL &&
		Nav != this)
	{
		// search for the scout if not specified
		if (Scout == NULL)
		{
			Scout = FPathBuilder::GetScout(GetLevel());
		}
		if (Scout != NULL)
		{
			// create the forced spec
			UReachSpec *newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
			// no path allowed because LD marked it - mark it with a reachspec so LDs will know there is a proscribed path here
			newSpec->reachFlags = R_PROSCRIBED;
			newSpec->Start = this;
			newSpec->End = Nav;
			PathList.AddItem(newSpec);
		}
	}
}

/* addReachSpecs()
Virtual function - adds reachspecs to path for every path reachable from it.
*/
void ANavigationPoint::addReachSpecs(AScout *Scout, UBOOL bOnlyChanged)
{
	// warn if no base
	if (Base == NULL &&
		ShouldBeBased() &&
		GetClass()->ClassFlags & CLASS_Placeable)
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("Navigation point not on valid base, or too close to steep slope"));
	}
	// warn if bad base
	if (Base &&
		Base->bPathColliding)
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf(TEXT("This type of NavigationPoint cannot be based on %s"), Base->GetName()) );
	}
	// warn if imbedded in wall
	if (Region.ZoneNumber == 0)
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("Navigation point imbedded in level geometry"));
	}

	// try to build a spec to every other pathnode in the level
	UReachSpec *newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
	newSpec->Init();
	for (ANavigationPoint *Nav = Level->NavigationPointList; Nav != NULL; Nav = Nav->nextNavigationPoint)
	{
		if (Nav != NULL &&
			!Nav->bDeleteMe &&
			!Nav->bNoAutoConnect &&
			!Nav->bSourceOnly &&
			(Nav->Location - Location).SizeSquared() <= MAXPATHDISTSQ &&
			Nav != this &&
			(!bOnlyChanged || bPathsChanged || Nav->bPathsChanged) )
		{
			//@fixme - find a better way to handle this
			// don't connect cover to cover, only cover to normal nodes
			if (!IsA(ACoverNode::StaticClass()) || 
				!Nav->IsA(ACoverNode::StaticClass()))
			{
				INT Proscribed = ProscribedPathTo(Nav);
				if (Proscribed == 2)
				{
					ProscribePathTo(Nav,Scout);
				}
				else
				{
					// check if forced path
					UBOOL bForced = 0;
					for (INT idx = 0; idx < ForcedPaths.Num() && !bForced; idx++)
					{
						if (Nav == ForcedPaths(idx))
						{
							bForced = 1;
						}
					}
					if ( bForced )
					{
						ForcePathTo(Nav,Scout);
					}
					else
					if ( !bDestinationOnly && !Proscribed && newSpec->defineFor(this, Nav, Scout) )
					{
						debugf(TEXT("***********added new spec from %s to %s"),GetName(),Nav->GetName());
						PathList.AddItem(newSpec);
						newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
					}
				}
			}
		}
	}
}

void ALadder::addReachSpecs(AScout *Scout, UBOOL bOnlyChanged)
{
	UReachSpec *newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);

	//debugf("Add Reachspecs for Ladder at (%f, %f, %f)", Location.X,Location.Y,Location.Z);
	bPathsChanged = bPathsChanged || !bOnlyChanged;

	// connect to all ladders in same LadderVolume
	if ( MyLadder )
	{
		for (INT i=0; i<GetLevel()->Actors.Num(); i++)
		{
			ALadder *Nav = Cast<ALadder>(GetLevel()->Actors(i));
			if ( Nav && (Nav != this) && (Nav->MyLadder == MyLadder) && (bPathsChanged || Nav->bPathsChanged) )
			{
				newSpec->Init();
				// add reachspec from this to other Ladder
				// FIXME - support single direction ladders (e.g. zipline)
				FVector CommonSize = Scout->GetSize(FName(TEXT("Common"),FNAME_Find));
				newSpec->CollisionRadius = CommonSize.X;
				newSpec->CollisionHeight = CommonSize.Y;
				newSpec->reachFlags = R_LADDER;
				newSpec->Start = this;
				newSpec->End = Nav;
				newSpec->Distance = (INT)(Location - Nav->Location).Size();
				PathList.AddItem(newSpec);
				newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
			}
		}
	}
	ANavigationPoint::addReachSpecs(Scout,bOnlyChanged);

	// Prune paths that require jumping
	for ( INT i=0; i<PathList.Num(); i++ )
		if ( PathList(i) && (PathList(i)->reachFlags & R_JUMP)
			&& (PathList(i)->End->Location.Z < PathList(i)->Start->Location.Z - PathList(i)->Start->CylinderComponent->CollisionHeight) )
			PathList(i)->bPruned = true;

}

void ATeleporter::addReachSpecs(AScout *Scout, UBOOL bOnlyChanged)
{
	UReachSpec *newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
	//debugf("Add Reachspecs for node at (%f, %f, %f)", Location.X,Location.Y,Location.Z);
	bPathsChanged = bPathsChanged || !bOnlyChanged;

	for (INT i=0; i<GetLevel()->Actors.Num(); i++)
	{
		ATeleporter *Nav = Cast<ATeleporter>(GetLevel()->Actors(i));
		if ( Nav && (Nav != this) && (Nav->Tag != NAME_None) && (URL==*Nav->Tag) && (bPathsChanged || Nav->bPathsChanged) )
		{
			newSpec->Init();
			FVector MaxCommonSize = Scout->GetSize(FName(TEXT("Max"),FNAME_Find));
			newSpec->CollisionRadius = MaxCommonSize.X;
			newSpec->CollisionHeight = MaxCommonSize.Y;
			newSpec->reachFlags = R_SPECIAL;
			newSpec->Start = this;
			newSpec->End = Nav;
			newSpec->Distance = 100;
			PathList.AddItem(newSpec);
			newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
			break;
		}
	}

	ANavigationPoint::addReachSpecs(Scout, bOnlyChanged);
}

void APlayerStart::addReachSpecs(AScout *Scout, UBOOL bOnlyChanged)
{
	ANavigationPoint::addReachSpecs(Scout, bOnlyChanged);

	// check that playerstart is useable
	FVector HumanSize = Scout->GetSize(FName(TEXT("Human"),FNAME_Find));
	Scout->SetCollisionSize(HumanSize.X, HumanSize.Y);
	if ( !GetLevel()->FarMoveActor(Scout,Location,1) )
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("PlayerStart is not useable"));
	}

}

void ALadder::InitForPathFinding()
{
	// find associated LadderVolume
	MyLadder = NULL;
	for( INT i=0; i<GetLevel()->Actors.Num(); i++ )
	{
		ALadderVolume *V = Cast<ALadderVolume>(GetLevel()->Actors(i));
		if ( V && (V->Encompasses(Location) || V->Encompasses(Location - FVector(0.f, 0.f, CylinderComponent->CollisionHeight))) )
		{
			MyLadder = V;
			break;
		}
	}
	if ( !MyLadder )
	{
		// Warn if there is no ladder volume
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, TEXT("Ladder is not in a LadderVolume"));
		return;
	}

	LadderList = MyLadder->LadderList;
	MyLadder->LadderList = this;
}

void AJumpPad::addReachSpecs(AScout *Scout, UBOOL bOnlyChanged)
{
	ANavigationPoint::addReachSpecs(Scout,bOnlyChanged);
	Scout->JumpZ = 10000.f;
	FVector HumanSize = Scout->GetSize(FName(TEXT("Human"),FNAME_Find));
	Scout->SetCollisionSize(HumanSize.X,HumanSize.Y);
	if ( (PathList.Num() > 0) && GetLevel()->FarMoveActor(Scout, Location) )
	{
		JumpTarget = PathList(0)->End;
		FVector XYDir = JumpTarget->Location - Location;
		FLOAT ZDiff = XYDir.Z;
		//FIXMESTEVE - NEED TO TEST WITH CORRECT FALLING BEHAVIOR
		FLOAT Time = 2.5f * JumpZModifier * appSqrt(Abs(ZDiff/PhysicsVolume->Gravity.Z));
		JumpVelocity = XYDir/Time;
		JumpVelocity.Z = ZDiff/Time - PhysicsVolume->Gravity.Z * Time;
	}
	else
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("No forced destination for this jumppad!"));
		JumpVelocity = FVector(0.f,0.f,3.f*Scout->TestJumpZ);
	}
	Scout->JumpZ = Scout->TestJumpZ;
}

/* ClearForPathFinding()
clear transient path finding properties right before a navigation network search
*/
void ANavigationPoint::ClearForPathFinding()
{
	visitedWeight = 10000000;
	nextOrdered = NULL;
	prevOrdered = NULL;
	previousPath = NULL;
	bEndPoint = bTransientEndPoint;
	bTransientEndPoint = false;
	cost = ExtraCost + TransientCost + FearCost;
	TransientCost = 0;
	bAlreadyVisited = false;
}

/* ClearPaths()
remove all path information from a navigation point. (typically before generating a new version of this
information
*/
void ANavigationPoint::ClearPaths()
{
	nextNavigationPoint = NULL;
	nextOrdered = NULL;
	prevOrdered = NULL;
	previousPath = NULL;
	PathList.Empty();
}

void ALadder::ClearPaths()
{
	Super::ClearPaths();

	if ( MyLadder )
		MyLadder->LadderList = NULL;
	LadderList = NULL;
	MyLadder = NULL;
}

void ANavigationPoint::FindBase()
{
	if (!Level->bBegunPlay)
	{
		SetZone(1,1);
		if (ShouldBeBased() &&
			Region.ZoneNumber != 0)
		{
			// not using find base, because don't want to fail if LD has navigationpoint slightly interpenetrating floor
			FCheckResult Hit(1.f);
			AScout *Scout = FPathBuilder::GetScout(GetLevel());
			check(Scout != NULL && "Failed to find scout for point placement");
			// get the dimensions for the average human player
			FVector HumanSize = Scout->GetSize(FName(TEXT("Human"),FNAME_Find));
			FVector CollisionSlice(HumanSize.X, HumanSize.X, 1.f);
			// and use this node's smaller collision radius if possible
			if (CylinderComponent->CollisionRadius < HumanSize.X)
			{
				CollisionSlice = FVector(CylinderComponent->CollisionRadius, CylinderComponent->CollisionRadius, 1.f);
				CollisionSlice.X = CollisionSlice.Y = CylinderComponent->CollisionRadius;
			}
			// check for placement
			GetLevel()->SingleLineCheck( Hit, this, Location - FVector(0,0, 2.f * CylinderComponent->CollisionHeight), Location, TRACE_AllBlocking, CollisionSlice );
			if (Hit.Actor != NULL)
			{
				if (Hit.Normal.Z > UCONST_MINFLOORZ)
				{
					GetLevel()->FarMoveActor(this, Hit.Location + FVector(0.f,0.f,CylinderComponent->CollisionHeight-1.f),0,1,0);
				}
				else
				{
					Hit.Actor = NULL;
				}
			}
			SetBase(Hit.Actor, Hit.Normal);
		}
	}
}

/* Prune paths when an acceptable route using an intermediate path exists
*/
INT ANavigationPoint::PrunePaths()
{
	INT pruned = 0;
	debugf(TEXT("Prune paths from %s"),GetName());

	for ( INT i=0; i<PathList.Num(); i++ )
	{
		for ( INT j=0; j<PathList.Num(); j++ )
			if ( (i!=j) && !PathList(j)->bPruned && (*PathList(j) <= *PathList(i))
				&& PathList(j)->End->FindAlternatePath(PathList(i), PathList(j)->Distance) )
			{
				PathList(i)->bPruned = true;
				//debugf(TEXT("Pruned path to %s because of path through %s"),PathList(i)->End->GetName(),PathList(j)->End->GetName());
				j = PathList.Num();
				pruned++;
			}
	}

	CleanUpPruned();

	return pruned;
}

UBOOL ANavigationPoint::FindAlternatePath(UReachSpec* StraightPath, INT AccumulatedDistance)
{
	FVector StraightDir = StraightPath->End->Location - StraightPath->Start->Location;

	// check if the endpoint is directly reachable
	INT i;
	for ( i=0; i<PathList.Num(); i++ )
		if ( !PathList(i)->bPruned && (PathList(i)->End == StraightPath->End)
				&& ((StraightDir | (StraightPath->End->Location - Location)) >= 0.f) )
		{
			if ( (AccumulatedDistance + PathList(i)->Distance < PATHPRUNING * StraightPath->Distance)
					&& (*PathList(i) <= *StraightPath) )
			{
				//debugf(TEXT("Direct path from %s to %s"),GetName(), PathList(i)->End->GetName());
				return true;
			}
			else
				return false;
		}

	// now continue looking for path
	for ( INT i=0; i<PathList.Num(); i++ )
		if ( !PathList(i)->bPruned
			&& (PathList(i)->Distance > 0.f)
			&& (AccumulatedDistance + PathList(i)->Distance < PATHPRUNING * StraightPath->Distance)
			&& (*PathList(i) <= *StraightPath)
			&& (PathList(i)->End != StraightPath->Start)
			&& ((StraightDir | (PathList(i)->End->Location - Location)) > 0.f)
			&& PathList(i)->End->FindAlternatePath(StraightPath, AccumulatedDistance + PathList(i)->Distance) )
		{
			//debugf(TEXT("Partial path from %s to %s"),GetName(), PathList(i)->End->GetName());
			return true;
		}

	return false;
}

IMPLEMENT_CLASS(ACoverNode);

void ACoverNode::InitialPass()
{
	// clear previous cover links
	LeftNode = NULL;
	RightNode = NULL;
	TransitionNode = NULL;
	bLeanLeft = 0;
	bLeanRight = 0;
	FVector cylExtent = GetCylinderExtent();
	FCheckResult checkResult;
	// orient to the nearest feature by tracing
	// in various directions
	{
		FRotator checkRot = Rotation;
		FCheckResult bestCheck;
		FLOAT angle = 0.f;
		const INT AngleCheckCount = 256;
		for (INT idx = 0; idx < AngleCheckCount; idx++)
		{
			angle += 65536.f/AngleCheckCount * idx;
			checkRot.Yaw += angle;
			FVector checkDir = checkRot.Vector();
			if (!GetLevel()->SingleLineCheck(checkResult,
											this,
											Location + (checkRot.Vector() * (cylExtent.X*2.f)),
											Location,
											TRACE_World,
											FVector(1,1,1)))
			{
				// scale by the current rotation to allow ld's a bit of control
				checkResult.Time *= checkResult.Normal | (Rotation.Vector());
				// compare against our best check, if not set or
				// this check resulted in a closer hit
				if (bestCheck.Actor == NULL ||
					(bestCheck.Time > checkResult.Time &&
					 checkResult.Actor != NULL))
				{
					bestCheck = checkResult;
				}
			}
		}
		// if an actor was found, set the new rotation
		if (bestCheck.Actor != NULL)
		{
			FRotator newRotation = Rotation;
			//newRotation.Yaw = (bestCheck.Location-Location).Rotation().Yaw;
			newRotation.Yaw = (bestCheck.Normal * -1).Rotation().Yaw;
			Rotation = newRotation;
		}
	}
	// determine what the height of this node is
	// by checking the normal height of the node
	{
		FVector checkDir = Rotation.Vector();
		FVector checkLoc = Location;
		// start at the bottom and trace up
		checkLoc.Z += -cylExtent.Z + CrouchHeight;
		FLOAT checkDist = MidHeight - CrouchHeight;
		UBOOL bHit = 1;
		while (checkDist > 0 &&
			   bHit)
		{
			bHit = !GetLevel()->SingleLineCheck(checkResult,
												this,
												checkLoc + checkDir * (cylExtent.X*3.f),
												checkLoc,
												TRACE_World,
												FVector(1,1,1));
			checkDist -= 16.f;
			checkLoc.Z += 16.f;
		}
		// if we found a gap, assume crouch cover
		if (!bHit)
		{
			debugf(TEXT("%s is crouching cover"),GetName());
			CoverType = CT_Crouching;
		}
		else
		{
			// check for mid to standing
			checkLoc = Location;
			checkLoc.Z += -cylExtent.Z + MidHeight;
			checkDist = StandHeight - MidHeight;
			while (checkDist > 0 &&
				   bHit)
			{
				bHit = !GetLevel()->SingleLineCheck(checkResult,
													this,
													checkLoc + checkDir * (cylExtent.X*3.f),
													checkLoc,
													TRACE_World,
													FVector(1,1,1));
				checkDist -= 16.f;
				checkLoc.Z += 16.f;
			}
			// if we found a gap, assume mid level cover
			if (!bHit)
			{
				debugf(TEXT("%s is midlevel cover"),GetName());
				CoverType = CT_MidLevel;
			}
			else
			{
				// otherwise it's full standing cover
				debugf(TEXT("%s is standing cover"),GetName());
				CoverType = CT_Standing;
			}
		}
	}
}

//#define DEBUG_COVER

void ACoverNode::LinkPass()
{
	FVector cylExtent = GetCylinderExtent();
	FCheckResult checkResult;
	debugf(TEXT("%s build cover ======================="),GetName());
	// sort the nodes base on distance
	TArray<ACoverNode*> sortedList;
	for (ANavigationPoint *nav = Level->NavigationPointList; nav != NULL; nav = nav->nextNavigationPoint)
	{
		ACoverNode *node = Cast<ACoverNode>(nav);
		if (nav != this &&
			node != NULL &&
			(node->Location-Location).SizeSquared() < MAXPATHDISTSQ &&
			GetLevel()->SingleLineCheck(checkResult,this,node->Location,Location,TRACE_World,FVector(1.f,1.f,1.f)))
		{
			UBOOL bInserted = 0;
			FLOAT dist2D = (node->Location-Location).Size2D();
			for (INT cIdx = 0; cIdx < sortedList.Num() && !bInserted; cIdx++)
			{
				if ((sortedList(cIdx)->Location-Location).Size2D() > dist2D)
				{
					bInserted = 1;
					sortedList.Insert(cIdx,1);
					sortedList(cIdx) = node;
				}
			}
			if (!bInserted)
			{
				// append the node
				sortedList.AddItem(node);
			}
		}
	}
	// find all links between matching cover nodes
	for (INT idx = 0; idx < sortedList.Num(); idx++)
	{
		ACoverNode *node = sortedList(idx);
		if (node != NULL)
		{
			debugf(TEXT("%s checking cover links to %s"),GetName(),node->GetName());
			// if the node is with transition distance,
			if ((node->Location - Location).Size() <= TransitionDist)
			{
				debugf(TEXT("%s possible transition to %s"),GetName(),node->GetName());
				// see if they are perpindicular
				FLOAT orientDot = node->Rotation.Vector() | Rotation.Vector();
				if (orientDot < 0.3f &&
					orientDot > -0.3f)
				{
					debugf(TEXT("- marked as transition"));
					TransitionNode = node;
					node->TransitionNode = this;
				}
			}
			else
			{
				// determine whether or not this is to the left/right
				FVector dir = (Rotation.Vector() ^ FVector(0.f,0.f,1.f)).SafeNormal();
				FVector checkDir = (Location - node->Location).SafeNormal();
				FLOAT dot = dir | checkDir;
				if ((dot > 0.8f && RightNode == NULL) ||
					(dot < -0.8f && LeftNode == NULL))
				{
					// recalculate dir as the proper perpendicular axis
					//dir = checkDir ^ FVector(0.f,0.f,1.f);
					dir = Rotation.Vector();
					// walk in the direction of the node, making sure there is cover between the nodes
					INT stepCnt = 0;
					// use collision radius as the step distance
					FLOAT stepDist = cylExtent.X * 2.f;	
					FVector lastCheckLoc = Location - FVector(0.f,0.f,1.f) * cylExtent.Z;
					lastCheckLoc += FVector(0.f,0.f,1.f) * CrouchHeight;
					FLOAT dist = (Location - node->Location).Size();
					UBOOL bLink = 1;
					while (dist > stepDist &&
						   bLink == 1)
					{
						// first check from our last location to the new location towards the node
						FVector checkLoc = lastCheckLoc - checkDir * stepDist;
	#ifdef DEBUG_COVER
						GetLevel()->PersistentLineBatcher->DrawLine(checkLoc,lastCheckLoc,FColor(255,0,0));
	#endif
						// if that fails then there is no link
						if (!GetLevel()->SingleLineCheck(checkResult,this,checkLoc,lastCheckLoc,TRACE_World,FVector(1,1,1)))
						{
							 if (checkResult.Actor != node)
							 {
								 bLink = 0;
							 }
						}
						// otherwise then check for obstructing geometry perpendicular
						if (bLink == 1)
						{
	#ifdef DEBUG_COVER
							GetLevel()->PersistentLineBatcher->DrawLine(checkLoc,checkLoc + dir * (stepDist*1.5f),FColor(128,0,0));
	#endif
							GetLevel()->SingleLineCheck(checkResult,this,checkLoc + dir * (stepDist*1.5f),checkLoc,TRACE_World,FVector(1,1,1));
							if (checkResult.Actor == NULL)
							{
								// no cover, don't link
								bLink = 0;
							}
							else
							{
								// decrement the remaining distance
								dist -= stepDist;
								lastCheckLoc = checkLoc;
							}
						}
					}
					// if we found a link, and pointing same direction within a threshold
					if (bLink == 1 &&
						(node->Rotation.Vector() | Rotation.Vector()) >= 0.3f)
					{
						if (dot > 0.8f)
						{
							debugf(TEXT("%s linked to %s, right node"),GetName(),node->GetName());
							// right node
							RightNode = node;
							node->LeftNode = this;
						}
						else
						if (dot < -0.8f)
						{
							debugf(TEXT("%s linked to %s, left node"),GetName(),node->GetName());
							// left node
							LeftNode = node;
							node->RightNode = this;
						}
					}
				}
			}
		}
	}
	// remove specs to other cover nodes as they aren't needed
	for (INT pathIdx = 0; pathIdx < PathList.Num(); pathIdx++)
	{
		if (Cast<ACoverNode>(PathList(pathIdx)->End) != NULL)
		{
			PathList.Remove(pathIdx--,1);
		}
	}
	debugf(TEXT("%s end build cover ==================="),GetName());
}

struct FFeatureInfo
{
public:
	/** Location of this feature */
	FVector	Location;
	/** Distance from node of the feature */
	FLOAT Distance;
	/** Type of feature */
	enum EFeatureType
	{
		/** Default, no feature */
		FT_None,
		/** An opposing surface creating a corner */
		FT_Corner,
		/** Or a normal edge */
		FT_Edge,
	};
	EFeatureType Type;

	/**
	 * Is this feature more pertinent than the other feature?
	 * 
	 * @param	otherFeature - feature to test against
	 * 
	 * @return	true if this feature is the more interesting one
	 */
	UBOOL operator>(const FFeatureInfo &otherFeature)
	{
		return this->Distance > otherFeature.Distance;
	}

	FFeatureInfo()
	: Type(FT_None)
	, Distance(0.f)
	, Location(0.f,0.f,0.f)
	{
	}
};

/**
 * Performs a series of line checks, attempting to find an edge
 * somewhat parallel to checkDir.  Will trace the initialOffset,
 * then if hit geometry, it will walk down checkDir until
 * an edge is found.
 * 
 * @param		level - ULevel used for performing line checks
 * 
 * @param		srcActor - source actor for perfomining line 
 * 				checks
 * 
 * @param		startLoc - start location for all checks
 * 
 * @param		initialOffset - offset applied from startLoc along
 * 				checkDir, to determine what type of checks to use
 * 
 * @param		startDir - initial direction vector used to start
 * 				the checks, should be perpindicular to wall
 * 
 * @param		checkDir - direction to walk along looking for the
 * 				nearest feature
 * 
 * @return		true if an edge was found
 */
static FFeatureInfo FindPerpindicularFeature(ULevel *level, AActor *srcActor, const FVector &startLoc, const FLOAT &initialOffset, 
											 const FVector &startDir, const FVector &checkDir)
{
	check(level != NULL &&
		  srcActor != NULL);
	FCheckResult checkResult;
	FVector checkLoc = startLoc;
	FFeatureInfo feature;
	UBOOL bHit = 0;
	const FLOAT MaxCheckDist = 512.f;
	// start walking down the wall looking for a gap in the geometry
	const INT MaxChecks = 16;
	const FLOAT CheckDist = MaxCheckDist/MaxChecks;
	FLOAT lastDist = initialOffset;
	// do an initial forward trace to find the current wall distance
#ifdef DEBUG_COVER
	level->PersistentLineBatcher->DrawLine(srcActor->Location,checkLoc,FColor(0,255,0));
	level->PersistentLineBatcher->DrawLine(checkLoc,checkLoc + (startDir * initialOffset),FColor(0,255,0));
#endif
	if (!level->SingleLineCheck(checkResult,
								srcActor,
								checkLoc + (startDir * initialOffset),
								checkLoc,
								TRACE_World,
								FVector(1.f,1.f,1.f)))
	{
		lastDist = (checkResult.Location-checkLoc).Size();
	}
	for (INT chks = 0; chks <= MaxChecks && feature.Distance == FFeatureInfo::FT_None; ++chks)
	{
		debugf(TEXT("chks: %d MaxChecks: %d"), chks, MaxChecks);

		// first trace towards the new check location
		FVector newCheckLoc = checkLoc + (checkDir * CheckDist);
#ifdef DEBUG_COVER
		level->PersistentLineBatcher->DrawLine(checkLoc,newCheckLoc,FColor(0,128,0));
#endif
		bHit = !level->SingleLineCheck(checkResult,
									   srcActor,
								        newCheckLoc,
									   checkLoc,
									   TRACE_World,
									   FVector(1.f,1.f,1.f));
		FVector lastCheckLoc = newCheckLoc;
		// if we hit something, then mark the corner
		if (bHit)
		{							
			feature.Location = checkResult.Location;
			feature.Distance = (feature.Location - srcActor->Location).Size2D();
			feature.Type = FFeatureInfo::FT_Corner;
			debugf(TEXT("+ %s hit corner"),srcActor->GetName());
		}
		else
		{
			// perform an interior trace to see if we've found a gap
#ifdef DEBUG_COVER
			level->PersistentLineBatcher->DrawLine(newCheckLoc,newCheckLoc + (startDir * initialOffset),FColor(0,0,255));
#endif
			bHit = !level->SingleLineCheck(checkResult,
										   srcActor,
										   newCheckLoc + (startDir * initialOffset),
										   newCheckLoc,
										   TRACE_World,
										   FVector(1.f,1.f,1.f));
			// if there is no hit, then find the edge feature location
			if (!bHit)
			{
				debugf(TEXT("+ %s found gap [last dist: %2.1f]"),srcActor->GetName(),lastDist);
				feature.Type = FFeatureInfo::FT_Edge;
				// trace inward to find the actual location
				newCheckLoc = newCheckLoc + (startDir * (lastDist + 4.f));
#ifdef DEBUG_COVER
				level->PersistentLineBatcher->DrawLine(newCheckLoc,newCheckLoc + (checkDir * CheckDist * -1.5f),FColor(0,0,128));
#endif
				bHit = !level->SingleLineCheck(checkResult,
											   srcActor,
											   newCheckLoc + (checkDir * CheckDist * -1.5f),
											   newCheckLoc,
											   TRACE_World,
											   FVector(1.f,1.f,1.f));
				if (bHit)
				{
					debugf(TEXT("+ found interior edge [%2.1f %2.1f %2.1f]"),
						   checkResult.Location.X,checkResult.Location.Y,checkResult.Location.Z);
					feature.Location = checkResult.Location;
				}
				else
				{
					debugf(TEXT("+ failed to find interior edge"));
					feature.Location = newCheckLoc;
				}
				// save the distance
				feature.Distance = (feature.Location - srcActor->Location).Size2D();
			}
			else
			{
				// save the last distance for future interior checks
				lastDist = (checkResult.Location - newCheckLoc).Size();
			}
		}
		checkLoc = lastCheckLoc;
	}
	return feature;
}

void ACoverNode::FinalPass()
{
	FVector cylExtent = GetCylinderExtent();
	FCheckResult checkResult;
	//const FLOAT checkDist = cylExtent.X * 3.f;
	const FLOAT checkDist = AlignDist + 16.f;
	// first align against the nearest surface
	{
		FVector checkLoc = Location;
		FVector checkDir = Rotation.Vector();
		if (!GetLevel()->SingleLineCheck(checkResult,
										 this,
										 Location + checkDir * checkDist * 2.f,
										 Location,
										 TRACE_World,
										 FVector(1,1,1)))
		{
			// ran into something, so figure out the best distance
			GetLevel()->FarMoveActor(this,
									 checkResult.Location + checkResult.Normal * AlignDist,
									 0,
									 0,
									 0);
		}
	}
	// figure out the correct check height
	FVector startLoc = Location;
	startLoc.Z -= cylExtent.Z;
	if (CoverType == CT_Crouching)
	{
		startLoc.Z += CrouchHeight;
	}
	else
	if (CoverType == CT_MidLevel)
	{
		startLoc.Z += MidHeight;
	}
	else
	{
		startLoc.Z += StandHeight;
	}
	FVector startDir = Rotation.Vector();
	UBOOL bHitCorner = 0;
	// then check if we are an interior node
	UBOOL bInteriorNode = LeftNode != NULL && RightNode != NULL;

	// Single node
	UBOOL bSingleNode = LeftNode == NULL && RightNode == NULL;

	// check to see if we should center
	// - either as a standlone node, or
	// - interior node, with no exact match nodes on both sides
	UBOOL bShouldCenter = (bSingleNode || 
						  (bInteriorNode &&
							CoverType != RightNode->CoverType &&
							CoverType != LeftNode->CoverType));

	if( bShouldCenter )
	{
		debugf(TEXT("%s searching for center"),GetName());
	
		if( bSingleNode )
		{
			// if single node, then shoot right at surfare to recenter the node
			startLoc.Z -= 8.f;
		}
		else
		{
			// add enough fuzz fuzz!
			startLoc.Z += 8.f;
		}

		// find the middle of the two edges
		FVector checkDir = (Rotation.Vector() ^ FVector(0.f,0.f,1.f)).SafeNormal();
		FFeatureInfo feature = FindPerpindicularFeature(GetLevel(),this,startLoc,checkDist,startDir,checkDir);
		if (feature.Type != FFeatureInfo::FT_None)
		{
			// search the other direction as well
			checkDir = (Rotation.Vector() ^ FVector(0.f,0.f,-1.f)).SafeNormal();
			FFeatureInfo otherFeature = FindPerpindicularFeature(GetLevel(),this,startLoc,checkDist,startDir,checkDir);
			if (otherFeature.Type != FFeatureInfo::FT_None)
			{
				// move to the middle of the two locations
				FVector center = (feature.Location + otherFeature.Location)/2.f;

				#ifdef DEBUG_COVER
					GetLevel()->PersistentLineBatcher->DrawLine(center, center+FVector(0.f,0.f,32.f), FColor(255,0,255));
					GetLevel()->PersistentLineBatcher->DrawLine(feature.Location, feature.Location+FVector(0.f,0.f,32.f), FColor(255,0,128));
					GetLevel()->PersistentLineBatcher->DrawLine(otherFeature.Location, otherFeature.Location+FVector(0.f,0.f,32.f), FColor(128,0,255));
				#endif
				checkDir *= -1.f;
				// project center onto properly aligned plane
				FVector newLoc = Location + ((center-Location) | checkDir) * checkDir;
				debugf(TEXT("> Centered node %s to [%2.1f %2.1f %2.1f] from [%2.1f %2.1f %2.1f]"),
					   GetName(),
					   newLoc.X,newLoc.Y,newLoc.Z,
					   Location.X,Location.Y,Location.Z);
				GetLevel()->FarMoveActor(this,newLoc,0,1,0);
			}
			else
			{
				debugf(NAME_Warning,TEXT("> Failed to find second feature for %s"),GetName());
			}
		}
		else
		{
			debugf(NAME_Warning,TEXT("> Failed to find first feature for %s"),GetName());
		}
	}
	else
	// if we are an interior node,
	if( bInteriorNode )
	{
		// check both features and look for a transition
		FVector checkDir(0.f,0.f,0.f);
		if (LeftNode->CoverType != CoverType)
		{
			checkDir = Rotation.Vector() ^ FVector(0.f,0.f,1.f);
		}
		else
		if (RightNode->CoverType != CoverType)
		{
			checkDir = Rotation.Vector() ^ FVector(0.f,0.f,-1.f);
		}
		if (!checkDir.IsZero())
		{
			debugf(TEXT("%s searching for edge"),GetName());
			// look to see if there is a feature
			checkDir = checkDir.SafeNormal();
			// figure out the correct check height
			FVector startLoc = Location + (FVector(0.f,0.f,-1.f) * cylExtent.Z);
			if (CoverType == CT_Crouching)
			{
				startLoc.Z += CrouchHeight;
			}
			else
			if (CoverType == CT_MidLevel)
			{
				startLoc.Z += MidHeight;
			}
			else
			{
				startLoc.Z += StandHeight;
			}
			FVector startDir = Rotation.Vector();
			// and perform the actual search
			FFeatureInfo feature = FindPerpindicularFeature(GetLevel(),this,startLoc,checkDist,startDir,checkDir);
			if (feature.Type != FFeatureInfo::FT_None)
			{
				// if we hit a corner disallow any leaning
				if (feature.Type == FFeatureInfo::FT_Corner)
				{
					debugf(TEXT("> hit a corner"));
					bHitCorner = 1;
				}
				// align to the feature location, including the AlignDist
				FVector newLoc = Location + (((feature.Location - Location) | checkDir) * checkDir);
				newLoc += (-checkDir * (feature.Type == FFeatureInfo::FT_Edge ? EdgeAlignDist : CornerAlignDist));

				#ifdef DEBUG_COVER
					GetLevel()->PersistentLineBatcher->DrawLine(newLoc, newLoc+FVector(0.f,0.f,32.f), FColor(255,0,255));
					GetLevel()->PersistentLineBatcher->DrawLine(feature.Location, feature.Location+FVector(0.f,0.f,32.f), FColor(255,0,128));
				#endif

				debugf(TEXT("> Aligned interior edge node %s to [%2.1f %2.1f %2.1f] from [%2.1f %2.1f %2.1f]"),
					   GetName(),
					   newLoc.X,newLoc.Y,newLoc.Z,
					   Location.X,Location.Y,Location.Z);
				GetLevel()->FarMoveActor(this,newLoc,0,1,0);
			}
			else
			{
				debugf(NAME_Warning,TEXT("> Failed to find nearest feature for %s"),GetName());
			}
		}
	}
	// otherwise, check for the exterior edge to align to
	else
	{
		startLoc = Location;

		FVector checkDir = Rotation.Vector() ^ (LeftNode == NULL ? FVector(0.f,0.f,1.f) : FVector(0.f,0.f,-1.f));
		checkDir = checkDir.SafeNormal();
		FFeatureInfo feature = FindPerpindicularFeature(GetLevel(),this,startLoc,checkDist,startDir,checkDir);

		if( feature.Type != FFeatureInfo::FT_None )
		{
			// if we hit a corner disallow any leaning
			if( feature.Type == FFeatureInfo::FT_Corner )
			{
				debugf(TEXT("> hit a corner"));
				bHitCorner = 1;
			}

			// align to the feature location minus collision radius
			FVector newLoc = Location + ( ((feature.Location - checkDir - Location) | checkDir)) * checkDir;
			newLoc += (-checkDir * (feature.Type == FFeatureInfo::FT_Edge ? EdgeAlignDist : CornerAlignDist));

			#ifdef DEBUG_COVER
				GetLevel()->PersistentLineBatcher->DrawLine(newLoc, newLoc+FVector(0.f,0.f,32.f), FColor(255,0,255));
				GetLevel()->PersistentLineBatcher->DrawLine(feature.Location, feature.Location+FVector(0.f,0.f,32.f), FColor(255,0,128));
			#endif

			debugf(TEXT("> Aligned edge node %s to [%2.1f %2.1f %2.1f] from [%2.1f %2.1f %2.1f]"),
				   GetName(),
				   newLoc.X,newLoc.Y,newLoc.Z,
				   Location.X,Location.Y,Location.Z);
			GetLevel()->FarMoveActor(this,newLoc,0,1,0);
		}
		else
		{
			debugf(NAME_Warning,TEXT("> Failed to find nearest exterior feature for %s"),GetName());
		}
	}

	// determine leaning capabilities
	if (CoverType != CT_Crouching &&
		!bHitCorner)
	{
		if (LeftNode == NULL ||
			(LeftNode->CoverType != CT_Standing &&
			 LeftNode->CoverType != CoverType))
		{
			bLeanLeft = 1;
		}
		if (RightNode == NULL ||
			(RightNode->CoverType != CT_Standing &&
			 RightNode->CoverType != CoverType))
		{
			bLeanRight = 1;
		}
	}

	// update the sprite
	UpdateSprite();
}

void ACoverNode::UpdateSprite()
{
	// set the sprite component's texture to match this cover's capabilities
	for (INT idx = 0; idx < Components.Num(); idx++)
	{
		USpriteComponent *spriteComp = Cast<USpriteComponent>(Components(idx));
		if (spriteComp != NULL)
		{
			FString materialName = FString::Printf(TEXT("CoverNode%s%s"),
												   CoverType == CT_Standing ? TEXT("Standing") : CoverType == CT_MidLevel ? TEXT("Mid") : CoverType == CT_Crouching ? TEXT("Crouch") : TEXT("None"),
												   !bAutoSetup ? TEXT("") : TEXT("Locked"));
			spriteComp->Sprite = FindObject<UTexture2D>(ANY_PACKAGE,*materialName,0);
			break;
		}
	}
}

void ACoverNode::FindBase()
{
	//ANavigationPoint::FindBase();
	
	if( !Level->bBegunPlay )
	{
		// move actor down
		FVector boxExtent(CylinderComponent->CollisionRadius,CylinderComponent->CollisionRadius,CylinderComponent->CollisionHeight);
		FCheckResult chkResult;
		GetLevel()->SingleLineCheck(chkResult, this, Location - FVector(0,0,128.f), Location, TRACE_AllBlocking, boxExtent);
		if( chkResult.Actor != NULL )
		{
			// try to fit it into world (no encroachment).
			FVector newLoc = chkResult.Location;

			GetLevel()->FindSpot(boxExtent, newLoc);
			if( newLoc != Location )
			{
				// if found a more suitable location, move actor
				GetLevel()->FarMoveActor(this,newLoc,0,1,0);
			}
		}
		SetBase(chkResult.Actor,chkResult.Normal);
	}
}
