/*=============================================================================
	UnPath.cpp: Unreal pathnode placement

	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	These methods are members of the FPathBuilder class, which adds pathnodes to a level.
	Paths are placed when the level is built.  FPathBuilder is not used during game play

   General comments:
   Path building
   The FPathBuilder does a tour of the level (or a part of it) using the "Scout" actor, starting from
   every actor in the level.
   This guarantees that correct reachable paths are generated to all actors.  It starts by going to the
   wall to the right of the actor, and following that wall, keeping it to the left.  NOTE: my definition
   of left and right is based on normal Cartesian coordinates, and not screen coordinates (e.g. Y increases
   upward, not downward).  This wall following is accomplished by moving along the wall, constantly looking
   for a leftward passage.  If the FPathBuilder is stopped, he rotates clockwise until he can move forward
   again.  While performing this tour, it keeps track of the set of previously placed pathnodes which are
   visible and reachable.  If a pathnode is removed from this set, then the FPathBuilder searches for an
   acceptable waypoint.  If none is found, a new pathnode is added to provide a waypoint back to the no longer
   reachable pathnode.  The tour ends when a full circumlocution has been made, or if the FPathBuilder
   encounters a previously placed path node going in the same direction.  Pathnodes that were not placed as
   the result of a left turn are marked, since they are due to some possibly unmapped obstruction.  In the
   final phase, these obstructions are inspected.  Then, the pathnode due to the obstruction is removed (since
   the obstruction is now mapped, and any needed waypoints have been built).

	Revision history:
		* Created by Steven Polge 3/97
=============================================================================*/

#include "EnginePrivate.h"
#include "UnPath.h"
#include "EngineAIClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"

void FPathBuilder::undefinePaths (ULevel *ownerLevel, UBOOL bOnlyChanged)
{
	Level = ownerLevel;

	//remove all reachspecs
	debugf(NAME_DevPath,TEXT("Remove old reachspecs"));

	// clear navigationpointlist
	Level->GetLevelInfo()->NavigationPointList = NULL;

	GWarn->BeginSlowTask( TEXT("Undefining Paths"), 1);

	//clear NavigationPoints
	for (INT i=0; i<Level->Actors.Num(); i++)
	{
		GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Undefining") );

		ANavigationPoint *Nav = Cast<ANavigationPoint>(Level->Actors(i));
		if (Nav != NULL)
		{
			if ( !(Nav->GetClass()->ClassFlags & CLASS_Placeable) )
			{
				/* delete any nodes which aren't placeable, because they were automatically generated,
				  and will be automatically generated again. */
				Level->DestroyActor(Nav);
			}
			else
			{
				if (!bOnlyChanged ||
					Nav->bPathsChanged)
				{
					Nav->ClearPaths();
				}
			}
		}
		else
		if (Level->Actors(i) != NULL)
		{
			Level->Actors(i)->ClearMarker();
		}
	}

	Level->GetLevelInfo()->bPathsRebuilt = 0;
	GWarn->EndSlowTask();
}

void FPathBuilder::ReviewPaths(ULevel *ownerLevel)
{
	debugf(TEXT("FPathBuilder reviewing paths"));
    GWarn->MapCheck_Clear();
	GWarn->BeginSlowTask( TEXT("Reviewing paths..."), 1 );

	Level = ownerLevel;

	if ( !Level || !Level->GetLevelInfo() || !Level->GetLevelInfo()->NavigationPointList )
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("No navigation point list. Paths define needed."));
	else
	{
		INT NumDone = 0;
		INT NumPaths = 0;
		INT NumStarts = 0;
		for ( ANavigationPoint* N=Level->GetLevelInfo()->NavigationPointList; N!=NULL; N=N->nextNavigationPoint )
		{
			if ( Cast<APlayerStart>(N) )
				NumStarts++;
			NumPaths++;
		}
		if ( NumStarts < 16 )
			GWarn->MapCheck_Add( MCTYPE_ERROR, Level->GetLevelInfo()->NavigationPointList, *FString::Printf(TEXT("Only %d PlayerStarts in this level"),NumStarts));

		Scout = GetScout(ownerLevel);
		SetPathCollision(1);
		for ( ANavigationPoint* N=Level->GetLevelInfo()->NavigationPointList; N!=NULL; N=N->nextNavigationPoint )
		{
			GWarn->StatusUpdatef( NumDone, NumPaths, TEXT("Reviewing Paths") );
			N->ReviewPath(Scout);
			NumDone++;
		}
		SetPathCollision(0);
		DestroyScout(ownerLevel);
	}
	GWarn->EndSlowTask();
    GWarn->MapCheck_ShowConditionally();
}

void FPathBuilder::SetPathCollision(INT bEnabled)
{
	if ( bEnabled )
	{
		for ( INT i=0; i<Level->Actors.Num(); i++)
		{
			AActor *Actor = Level->Actors(i);
			// turn off collision - for non-static actors with bPathColliding false
			if ( Actor && !Actor->bDeleteMe )
			{
				if ( Actor->bBlockActors && Actor->bCollideActors && !Actor->bStatic && !Actor->bPathColliding )
				{
					Actor->bPathTemp = 1;
					Actor->SetCollision(0,Actor->bBlockActors);
					//debugf(TEXT("Collision turned OFF for %s"),Actor->GetName());
				}
				else
				{
					//if ( Actor->bCollideActors && Actor->bBlockActors )
					//	debugf(TEXT("Collision left ON for %s"),Actor->GetName());
					Actor->bPathTemp = 0;
				}
			}
		}
	}
	else
	{
		for ( INT i=0; i<Level->Actors.Num(); i++)
		{
			AActor *Actor = Level->Actors(i);
			if ( Actor && Actor->bPathTemp )
			{
				Actor->bPathTemp = 0;
				Actor->SetCollision(1,Actor->bBlockActors);
			}
		}
	}
}

void FPathBuilder::definePaths(ULevel *ownerLevel, UBOOL bOnlyChanged)
{
	// remove old paths
	undefinePaths(ownerLevel, bOnlyChanged);

	Level = ownerLevel;
	Scout = GetScout(ownerLevel);
	Level->GetLevelInfo()->NavigationPointList = NULL;
	Level->GetLevelInfo()->bHasPathNodes = false;
    GWarn->MapCheck_Clear();
	GWarn->BeginSlowTask( TEXT("Defining Paths"), 1);

	// Position interpolated actors in desired locations for path-building.
	TArray<USequenceObject*> InterpActs;

	UBOOL bProblemsMoving = false;

	if(Level->GameSequence)
	{	
		// Find all SeqAct_Interp actions in the level.
		Level->GameSequence->FindSeqObjectsByClass(USeqAct_Interp::StaticClass(), InterpActs);

		// Keep track of which actors we are updating - so we can check for multiple actions updating the same actor.
		TArray<AActor*> MovedActors;

		for(INT i=0; i<InterpActs.Num() && !bProblemsMoving; i++)
		{
			// For each SeqAct_Interp we flagged as having to be updated for pathing...
			USeqAct_Interp* Interp = CastChecked<USeqAct_Interp>( InterpActs(i) );
			if(Interp->bInterpForPathBuilding)
			{
				// Initialise it..
				Interp->InitInterp();

				for(INT j=0; j<Interp->GroupInst.Num(); j++)
				{
					// Check if any of the actors its working on have already been affected..
					AActor* InterpActor = Interp->GroupInst(j)->GroupActor;
					if(InterpActor)
					{
						if( MovedActors.ContainsItem(InterpActor) )
						{
							// This Actor is being manipulated by another SeqAct_Interp - so bail out.
							appMsgf(0, TEXT("Multiple SeqAct_Interps refer to the same Actor (%s) for path building."), InterpActor->GetName());
							bProblemsMoving = true;
						}
						else
						{
							// Things are ok - just add it to the list.
							MovedActors.AddItem(InterpActor);
						}
					}
				}

				if(!bProblemsMoving)
				{
					// If there were no problem initialising, first backup the current state of the actors..
					for(INT i=0; i<Interp->GroupInst.Num(); i++)
					{
						Interp->GroupInst(i)->SaveGroupActorState();
					}

					// ..then interpolate them to the 'PathBuildTime'.
					Interp->UpdateInterp( Interp->InterpData->PathBuildTime, true );
				}
				else
				{
					// If there were problems, term this interp now (so we won't try and restore its state later on).
					Interp->TermInterp();
				}
			}
		}
	}

	if(!bProblemsMoving)
	{
		INT NumPaths = 0; //used for progress bar
		SetPathCollision(1);

		// Add NavigationPoint markers to any actors which want to be marked
		for (INT i=0; i<Level->Actors.Num(); i++)
		{
			GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Defining") );

			AActor *Actor = Level->Actors(i);
			if ( Actor )
			{
				NumPaths += Actor->AddMyMarker(Scout);
			}
		}

		// stick navigation points on bases  (level or terrain)
		// at the same time, create the navigation point list
		INT NumDone = 0;
		FCheckResult Hit(1.f);
		for ( INT i=0; i<Level->Actors.Num(); i++)
		{
			GWarn->StatusUpdatef( NumDone, NumPaths, TEXT("Navigation Points on Bases") );

			ANavigationPoint *Nav = Cast<ANavigationPoint>(Level->Actors(i));
			if ( Nav && !Nav->bDeleteMe )
			{
				NumDone++;
				Nav->nextNavigationPoint = Level->GetLevelInfo()->NavigationPointList;
				Level->GetLevelInfo()->NavigationPointList = Nav;
				Nav->FindBase();
			}
		}
		// initialize all navs for path finding
		for (ANavigationPoint *Nav=Level->GetLevelInfo()->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		{
			if (!bOnlyChanged ||
				Nav->bPathsChanged)
			{
				Nav->InitForPathFinding();
			}
		}
		// calculate and add reachspecs to pathnodes
		debugf(NAME_DevPath,TEXT("Add reachspecs"));
		NumDone = 0;
		for( ANavigationPoint *Nav=Level->GetLevelInfo()->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		{
			if (!bOnlyChanged ||
				Nav->bPathsChanged)
			{
				GWarn->StatusUpdatef( NumDone, NumPaths, TEXT("Adding Reachspecs") );
				Nav->addReachSpecs(Scout,bOnlyChanged);
				NumDone++;
				debugf( NAME_DevPath, TEXT("Added reachspecs to %s"),Nav->GetName() );
			}
			else
			{
				debugf( NAME_DevPath, TEXT("Skipped unchanged pathnode %s"),Nav->GetName() );
			}
		}
		//prune excess reachspecs
		debugf(NAME_DevPath,TEXT("Prune reachspecs"));
		INT numPruned = 0;
		NumDone = 0;
		for( ANavigationPoint *Nav=Level->GetLevelInfo()->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		{
			GWarn->StatusUpdatef( NumDone, NumPaths, TEXT("Pruning Reachspecs") );
			numPruned += Nav->PrunePaths();
			NumDone++;
		}
		debugf(NAME_DevPath,TEXT("Pruned %d reachspecs"), numPruned);

		// set up terrain navigation
		GWarn->StatusUpdatef( NumDone, NumPaths, TEXT("Terrain Navigation") );

		// turn off collision and reset temporarily changed actors
		SetPathCollision(0);
		DestroyScout(ownerLevel);

		// clear pathschanged flags
		for( ANavigationPoint *Nav=Level->GetLevelInfo()->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		{
			Nav->bPathsChanged = 0;
			Nav->PostEditChange(NULL);
		}

		Level->GetLevelInfo()->bPathsRebuilt = 1;
	}

	GWarn->EndSlowTask();

	// build cover
	buildCover(Level);

	// Reset any interpolated actors.
	for(INT i=0; i<InterpActs.Num(); i++)
	{
		USeqAct_Interp* Interp = CastChecked<USeqAct_Interp>( InterpActs(i) );

		if(Interp->bInterpForPathBuilding)
		{
			for(INT i=0; i<Interp->GroupInst.Num(); i++)
			{
				Interp->GroupInst(i)->RestoreGroupActorState();
			}

			Interp->TermInterp();
			Interp->Position = 0.f;
		}
	}

	// Don't check for errors if we didn't bother building paths!
	if(!bProblemsMoving)
	{
		for (INT i=0; i<Level->Actors.Num(); i++)
		{
			AActor *Actor = Level->Actors(i);
			if ( Actor )
			{
				Actor->CheckForErrors();
			}
		}
	}

	debugf(NAME_DevPath,TEXT("All done"));
	if (!bOnlyChanged)
	{
		GWarn->MapCheck_ShowConditionally();
	}
}

void FPathBuilder::buildCover(ULevel *ownerLevel)
{
	Level = ownerLevel;

	INT numCover = 0;
	// make sure all covernodes are in the navigation list
	for (INT idx = 0; idx < Level->Actors.Num(); idx++)
	{
		ACoverNode *cover = Cast<ACoverNode>(Level->Actors(idx));
		if (cover != NULL)
		{
			numCover++;
			// see if it's in the list already
			UBOOL bFound = 0;
			for (ANavigationPoint *nav = Level->GetLevelInfo()->NavigationPointList; nav != NULL && !bFound; nav = nav->nextNavigationPoint)
			{
				if (nav == cover)
				{
					bFound = 1;
				}
			}
			if (!bFound)
			{
				// if not found, add to the front of the list
				cover->nextNavigationPoint = Level->GetLevelInfo()->NavigationPointList;
				Level->GetLevelInfo()->NavigationPointList = cover;
			}
		}
	}
	
	if (numCover > 0)
	{
		GWarn->BeginSlowTask( TEXT("Building Cover"), 1);
		// initial alignment/orientation pass
		INT numDone = 0;
		for (ANavigationPoint *Nav = Level->GetLevelInfo()->NavigationPointList; Nav != NULL; Nav = Nav->nextNavigationPoint)
		{
			GWarn->StatusUpdatef( numDone++, numCover, TEXT("Orienting Cover") );
			ACoverNode *cover = Cast<ACoverNode>(Nav);
			if (cover != NULL &&
				cover->bAutoSetup)
			{
				cover->InitialPass();
			}
		}

		// link pass for cover <-> cover
		numDone = 0;
		for (ANavigationPoint *Nav = Level->GetLevelInfo()->NavigationPointList; Nav != NULL; Nav = Nav->nextNavigationPoint)
		{
			GWarn->StatusUpdatef( numDone++, numCover, TEXT("Linking Cover") );
			ACoverNode *cover = Cast<ACoverNode>(Nav);
			if (cover != NULL &&
				cover->bAutoSetup)
			{
				cover->LinkPass();
			}
		}

		// add a final pass for cover to perform any cleanup
		numDone = 0;
		for (ANavigationPoint *Nav = Level->GetLevelInfo()->NavigationPointList; Nav != NULL; Nav = Nav->nextNavigationPoint)
		{
			GWarn->StatusUpdatef( numDone++, numCover, TEXT("Adjusting Cover") );
			ACoverNode *cover = Cast<ACoverNode>(Nav);
			if (cover != NULL &&
				cover->bAutoSetup)
			{
				cover->FinalPass();
			}
		}

		GWarn->EndSlowTask();
	}
}

//------------------------------------------------------------------------------------------------
// certain actors add paths to mark their position
INT ANavigationPoint::AddMyMarker(AActor *S)
{
	return 1;
}

INT APathNode::AddMyMarker(AActor *S)
{
	Level->bHasPathNodes = true;
	return 1;
}

FVector ALadderVolume::FindCenter()
{
	FVector Center(0.f,0.f,0.f);
	check(BrushComponent);
	check(Brush);
	for(INT PolygonIndex = 0;PolygonIndex < Brush->Polys->Element.Num();PolygonIndex++)
	{
		FPoly&	Poly = Brush->Polys->Element(PolygonIndex);
		FVector NewPart(0.f,0.f,0.f);
		for(INT VertexIndex = 0;VertexIndex < Poly.NumVertices;VertexIndex++)
			NewPart += Poly.Vertex[VertexIndex];
		NewPart /= Poly.NumVertices;
		Center += NewPart;
	}
	Center /= Brush->Polys->Element.Num();
	return Center;
}

INT ALadderVolume::AddMyMarker(AActor *S)
{
	check(BrushComponent);

	if ( !bAutoPath || !Brush )
		return 0;

	FVector Center = FindCenter();
	Center = LocalToWorld().TransformFVector(Center);

	AScout* Scout = Cast<AScout>(S);
	if ( !Scout )
		return 0 ;

	// JAG_COLRADIUS_HACK
	//UClass *pathClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("AutoLadder") );
	UClass *pathClass = AAutoLadder::StaticClass();
	AAutoLadder* DefaultLadder = (AAutoLadder*)( pathClass->GetDefaultActor() );

	// find ladder bottom
	FCheckResult Hit(1.f);
	GetLevel()->SingleLineCheck(Hit, this, Center - 10000.f * ClimbDir, Center, TRACE_World);
	if ( Hit.Time == 1.f )
		return 0;
	FVector Position = Hit.Location + DefaultLadder->CylinderComponent->CollisionHeight * ClimbDir;

	// place Ladder at bottom of volume
	GetLevel()->SpawnActor(pathClass, NAME_None, Position);

	// place Ladder at top of volume + 0.5 * CollisionHeight of Ladder
	Position = FindTop(Center + 500.f * ClimbDir);
	GetLevel()->SpawnActor(pathClass, NAME_None, Position - 5.f * ClimbDir);
	return 2;
}

// find the edge of the brush in the ClimbDir direction
FVector ALadderVolume::FindTop(FVector V)
{
	if ( Encompasses(V) )
		return FindTop(V + 500.f * ClimbDir);

	// trace back to brush edge from this outside point
	FCheckResult Hit(1.f);
	check(BrushComponent);
	BrushComponent->LineCheck( Hit, V - 10000.f * ClimbDir, V, FVector(0.f,0.f,0.f), 0 );
	return Hit.Location;

}

//------------------------------------------------------------------------------------------------
//Private methods

AScout* FPathBuilder::Scout = NULL;

void FPathBuilder::DestroyScout(ULevel *Level)
{
	check(Level != NULL);
	if (FPathBuilder::Scout != NULL)
	{
		Level->DestroyActor(FPathBuilder::Scout->Controller);
		Level->DestroyActor(FPathBuilder::Scout);
		FPathBuilder::Scout = NULL;
	}
}

/*getScout()
Find the scout actor in the level. If none exists, add one.
*/
AScout* FPathBuilder::GetScout(ULevel *Level)
{
	check(Level != NULL);
	AScout* newScout = FPathBuilder::Scout;
	if (newScout == NULL)
	{
		// search for an existing scout
		for (INT i = 0; i < Level->Actors.Num() && newScout == NULL; i++)
		{
			newScout = Cast<AScout>(Level->Actors(i));
		}
		// if one wasn't found create a new one
		if (newScout == NULL)
		{
			FString scoutClassName = GEngine->ScoutClassName;
			debugf(TEXT("using scout %s"),*scoutClassName);
			UClass *scoutClass = FindObjectChecked<UClass>( ANY_PACKAGE, *scoutClassName );
			check(scoutClass != NULL && "Failed to find scout class for path building!");
			newScout = (AScout*)Level->SpawnActor(scoutClass);
			// give it a default controller
			newScout->Controller = (AController*)Level->SpawnActor(FindObjectChecked<UClass>(ANY_PACKAGE, TEXT("AIController")));
		}
	}
	if (newScout != NULL)
	{
		// initialize scout collision properties
		newScout->SetCollision(1,1);
		newScout->bCollideWorld = 1;
		newScout->SetZone( 1,1 );
		newScout->PhysicsVolume = newScout->Level->GetDefaultPhysicsVolume();
		newScout->SetVolumes();
		newScout->bHiddenEd = 1;
	}
	return newScout;
}

/* Find a starting spot for Scout to perform perambulation
*/
int AScout::findStart(FVector start)
{
	if (GetLevel()->FarMoveActor(this, start)) //move Scout to starting point
	{
		//slide down to floor
		FCheckResult Hit(1.f);
		FVector Down = FVector(0,0,-100);
		Hit.Normal.Z = 0.f;
		INT iters = 0;
		while (Hit.Normal.Z < UCONST_MINFLOORZ)
		{
			GetLevel()->MoveActor(this, Down, Rotation, Hit, 1,1);
			if ( (Hit.Time < 1.f) && (Hit.Normal.Z < UCONST_MINFLOORZ) )
			{
				//adjust and try again
				FVector OldHitNormal = Hit.Normal;
				FVector Delta = (Down - Hit.Normal * (Down | Hit.Normal)) * (1.f - Hit.Time);
				if( (Delta | Down) >= 0 )
				{
					GetLevel()->MoveActor(this, Delta, Rotation, Hit, 1,1);
					if ((Hit.Time < 1.f) && (Hit.Normal.Z < UCONST_MINFLOORZ))
					{
						FVector downDir = Down.SafeNormal();
						TwoWallAdjust(downDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
						GetLevel()->MoveActor(this, Delta, Rotation, Hit, 1,1);
					}
				}
			}
			iters++;
			if (iters >= 10)
			{
				debugf(NAME_DevPath,TEXT("No valid start found"));
				return 0;
			}
		}
		//debugf(NAME_DevPath,TEXT("scout placed on valid floor"));
		return 1;
 	}

	//debugf(NAME_DevPath,TEXT("Scout didn't fit"));
	return 0;
}

//=============================================================================
// UPathRenderingComponent

IMPLEMENT_CLASS(UPathRenderingComponent);

void UPathRenderingComponent::UpdateBounds()
{
	FBox BoundingBox(0);

	ANavigationPoint *nav = Cast<ANavigationPoint>(Owner);
	if (nav != NULL &&
		nav->PathList.Num() > 0)
	{
		for (INT idx = 0; idx < nav->PathList.Num(); idx++)
		{
			UReachSpec* reach = nav->PathList(idx);
			if (reach != NULL &&
				reach->Start != NULL &&
				reach->End != NULL)
			{
				BoundingBox += reach->Start->Location;
				BoundingBox += reach->End->Location;
			}
		}
	}
	ACoverNode *node = Cast<ACoverNode>(Owner);
	if (node != NULL)
	{
		BoundingBox += node->Location;
		if (node->LeftNode != NULL)
		{
			BoundingBox += node->LeftNode->Location;
		}
		if (node->RightNode != NULL)
		{
			BoundingBox += node->RightNode->Location;
		}
	}

	Bounds = FBoxSphereBounds(BoundingBox);
}

static void DrawLineArrow(FPrimitiveRenderInterface* PRI,const FVector &start,const FVector &end,const FColor &color,FLOAT mag)
{
	// draw a pretty arrow
	FVector dir = end - start;
	FLOAT dirMag = dir.Size();
	dir /= dirMag;
	FMatrix arrowTM = FMatrix::Identity;
	arrowTM.SetOrigin(start);
	FVector YAxis, ZAxis;
	dir.FindBestAxisVectors(YAxis,ZAxis);
	arrowTM.SetAxis(0,dir);
	arrowTM.SetAxis(1,YAxis);
	arrowTM.SetAxis(2,ZAxis);
	PRI->DrawDirectionalArrow(arrowTM,color,dirMag,mag);
}

void UPathRenderingComponent::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if (Context.View != NULL &&
		Context.View->ShowFlags & SHOW_Paths)
	{
		// draw reach specs
		ANavigationPoint *nav = Cast<ANavigationPoint>(Owner);
		if (nav != NULL &&
			nav->PathList.Num() > 0)
		{
			for (INT idx = 0; idx < nav->PathList.Num(); idx++)
			{
				UReachSpec* reach = nav->PathList(idx);
				if (reach != NULL &&
					reach->Start != NULL &&
					reach->End != NULL)
				{
					DrawLineArrow(PRI,reach->Start->Location,reach->End->Location - FVector(0,0,-8),reach->PathColor(),8.f);
				}
			}
		}
		// draw some basic cover links
		ACoverNode *node = Cast<ACoverNode>(Owner);
		if (node != NULL &&
			!node->bCircular)
		{
			if (node->LeftNode != NULL)
			{
				FColor drawColor = node->LeftNode->RightNode == node ? FColor(0,255,0) : FColor(255,0,0);
				DrawLineArrow(PRI,node->Location - FVector(0,0,8.f),node->LeftNode->Location - FVector(0,0,8.f),drawColor,16.f);
			}
			if (node->RightNode != NULL)
			{
				FColor drawColor = node->RightNode->LeftNode == node ? FColor(0,255,0) : FColor(255,0,0);
				DrawLineArrow(PRI,node->Location + FVector(0,0,8.f),node->RightNode->Location + FVector(0,0,8.f),drawColor,16.f);
			}
		}
	}
}

