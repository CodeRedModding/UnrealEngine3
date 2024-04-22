/*=============================================================================
	UnNavigationPoint.cpp:

  NavigationPoint and subclass functions

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnPath.h"

#if WITH_EDITOR
void ANavigationPoint::SetNetworkID(INT InNetworkID)
{
	NetworkID = InNetworkID;
}


void MergeNetworkAIntoB(INT A, INT B)
{
	for( ANavigationPoint *Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint )
	{
		if(Nav && Nav->NetworkID == A)
		{
			Nav->SetNetworkID(B);
		}
	}
}

void ANavigationPoint::BuildNetworkIDs()
{
	// Clear all IDs
	for( ANavigationPoint *Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint )
	{
		Nav->NetworkID = -1;
	}	


	INT NewNetworkID = -1;
	for( ANavigationPoint *Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint )
	{
		if(Nav->NetworkID == -1)
		{
			//debugf(TEXT("%s was not on a network, adding new network.. (%i)"),*Nav->GetName(),NewNetworkID);
			Nav->SetNetworkID(++NewNetworkID);
		}

		for(INT Idx=0;Idx<Nav->PathList.Num();Idx++)
		{
			UReachSpec* Spec = Nav->PathList(Idx);
			if(Spec != NULL && Spec->End.Nav() != NULL)
			{
				ANavigationPoint* CurEnd = Spec->End.Nav();
				if(CurEnd->NetworkID == -1)
				{
					CurEnd->SetNetworkID(Nav->NetworkID);
				}
				else if(CurEnd->NetworkID != Nav->NetworkID)
				{
					//debugf(TEXT("%s encountered neighbor (%s) which already had a network, merging %i and %i"),*Nav->GetName(),*CurEnd->GetName(),Nav->NetworkID,CurEnd->NetworkID);
					MergeNetworkAIntoB(Nav->NetworkID,CurEnd->NetworkID);
				}
			}
		}
	}

	// print report :D
	//for( ANavigationPoint *Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint )
	//{
	//	debugf(TEXT("%s NETWORK %i"),*Nav->GetName(), Nav->NetworkID);
	//}
}
#endif

/** 
 *	Detect when path building is going to move a pathnode around
 *	This may be undesirable for LDs (ie b/c cover links define slots by offsets)
 */
void ANavigationPoint::Validate()
{
	AScout *Scout = FPathBuilder::GetScout();
	if( Scout && ShouldBeBased() && (GoodSprite || BadSprite) )
	{
		FVector OrigLocation = Location;

		FCheckResult Hit(1.f);
		FVector HumanSize = Scout->GetSize(FName(TEXT("Human"), FNAME_Find));
		FVector Slice(HumanSize.X, HumanSize.X, 1.f);
		if( CylinderComponent->CollisionRadius < HumanSize.X )
		{
			Slice.X = CylinderComponent->CollisionRadius;
			Slice.Y = CylinderComponent->CollisionRadius;
		}

		UBOOL bResult = TRUE;

		// Check for adjustment
		GWorld->SingleLineCheck( Hit, this, Location - FVector(0,0, 4.f * CylinderComponent->CollisionHeight), Location, TRACE_AllBlocking, Slice );
		if( Hit.Actor )
		{
			FVector Dest = Hit.Location + FVector(0.f,0.f,CylinderComponent->CollisionHeight-2.f);

			// Move actor (TEST ONLY) to see if navigation point moves
			GWorld->FarMoveActor( this, Dest, FALSE, TRUE, TRUE );

			// If only adjustment was down towards the floor
			if( Location.X == OrigLocation.X &&  
				Location.Y == OrigLocation.Y && 
				Location.Z <= OrigLocation.Z )
			{
				// Valid placement
				bResult = TRUE;
			}
			else
			{
				// Otherwise, pathnode is moved unexpectedly
				bResult = FALSE;
			}

			// Move actor back to original position
			GWorld->FarMoveActor( this, OrigLocation, FALSE, TRUE, TRUE );
		}	
		
		// Update sprites by result
		if( GoodSprite )
		{
			GoodSprite->HiddenEditor = !bResult;
		}
		if( BadSprite )
		{
			BadSprite->HiddenEditor = bResult;
		}
	}
	FPathBuilder::DestroyScout();

	// Force update of icon
	ForceUpdateComponents(FALSE,FALSE);
}

void ANavigationPoint::AddToNavigationOctree()
{
	// Don't add nav point to octree if it already has an octree node (already in the octree)
	if( CylinderComponent != NULL &&
		NavOctreeObject.OctreeNode == NULL )
	{
		// add ourselves to the octree
		NavOctreeObject.SetOwner(this);
		FVector Extent(CylinderComponent->CollisionRadius, CylinderComponent->CollisionRadius, CylinderComponent->CollisionHeight);
		NavOctreeObject.SetBox(FBox(Location - Extent, Location + Extent));
		GWorld->NavigationOctree->AddObject(&NavOctreeObject);
	}
	// add ReachSpecs to the octree
	for (INT i = 0; i < PathList.Num(); i++)
	{
		if (PathList(i) != NULL)
		{
			PathList(i)->AddToNavigationOctree();
		}
	}
}

void ANavigationPoint::RemoveFromNavigationOctree()
{
	GWorld->NavigationOctree->RemoveObject(&NavOctreeObject);
	for (INT Idx = 0; Idx < PathList.Num(); Idx++)
	{
		UReachSpec *Spec = PathList(Idx);
		if (Spec != NULL)
		{
			Spec->RemoveFromNavigationOctree();
		}
	}
}

void ANavigationPoint::ClearCrossLevelReferences()
{
	Super::ClearCrossLevelReferences();

	for( INT PathIdx = 0; PathIdx < PathList.Num(); PathIdx++ )
	{
		UReachSpec *Spec = PathList(PathIdx);
		if( Spec == NULL ||
			Spec->Start == NULL ||
			(*Spec->End == NULL && !Spec->End.Guid.IsValid()) ||
			Spec->Start != this )
		{
			PathList.Remove(PathIdx--,1);
			continue;
		}
		if( *Spec->End != NULL && Spec->Start->GetOutermost() != Spec->End->GetOutermost() )
		{
			bHasCrossLevelPaths = TRUE;
			Spec->End.Guid = *Spec->End->GetGuid();
		}
	}

	for( INT VolIdx = 0; VolIdx < Volumes.Num(); VolIdx++ )
	{
		FActorReference& VolRef = Volumes(VolIdx);
		if( *VolRef == NULL && !VolRef.Guid.IsValid() )
		{
			Volumes.Remove( VolIdx--, 1 );
			continue;
		}

		if( *VolRef != NULL && GetOutermost() != VolRef->GetOutermost() )
		{
			bHasCrossLevelPaths = TRUE;
			VolRef.Guid = *VolRef->GetGuid();
		}
	}
}

void ANavigationPoint::GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel)
{
	Super::GetActorReferences(ActorRefs,bIsRemovingLevel);
	if( bHasCrossLevelPaths )
	{
		// look at each path,
		for (INT PathIdx = 0; PathIdx < PathList.Num(); PathIdx++)
		{
			// if it crosses a level and isn't already valid,
			UReachSpec *Spec = PathList(PathIdx);
			if (Spec->End.Guid.IsValid())
			{
				// if removing a level, only valid if not null,
				// if not removing, only if null
				if ((bIsRemovingLevel && *Spec->End != NULL) ||
					(!bIsRemovingLevel && *Spec->End == NULL))
				{
					ActorRefs.AddItem(&Spec->End);
				}
			}
		}
		for( INT VolIdx = 0; VolIdx < Volumes.Num(); VolIdx++ )
		{
			FActorReference& VolRef = Volumes(VolIdx);
			if( VolRef.Guid.IsValid() )
			{
				if( ( bIsRemovingLevel && *VolRef != NULL) ||
					(!bIsRemovingLevel && *VolRef == NULL) )
				{
					ActorRefs.AddItem(&VolRef);
				}
			}
		}

#if WITH_EDITORONLY_DATA
		// handle the forced/proscribed lists as well
		if (GIsEditor)
		{
			for (INT PathIdx = 0; PathIdx < EditorForcedPaths.Num(); PathIdx++)
			{
				FActorReference &ActorRef = EditorForcedPaths(PathIdx);
				if ((bIsRemovingLevel && ActorRef.Actor != NULL) ||
					(!bIsRemovingLevel && ActorRef.Actor == NULL))
				{
					ActorRefs.AddItem(&ActorRef);
				}
			}
			for (INT PathIdx = 0; PathIdx < EditorProscribedPaths.Num(); PathIdx++)
			{
				FActorReference &ActorRef = EditorProscribedPaths(PathIdx);
				if ((bIsRemovingLevel && ActorRef.Actor != NULL) ||
					(!bIsRemovingLevel && ActorRef.Actor == NULL))
				{
					ActorRefs.AddItem(&ActorRef);
				}
			}
		}
#endif // WITH_EDITORONLY_DATA
	}
}

void ANavigationPoint::PostScriptDestroyed()
{
	Super::PostScriptDestroyed();

	UReachSpec* OutSpec = NULL;
	UReachSpec* InSpec = NULL;

	// if this is a dynamic path node, and we're being destroyed during gameplay clean up specs that point back to us
	if(	!IsStatic() )
	{
		// remove specs which point back to me
		for(INT Idx=0;Idx<PathList.Num();Idx++)
		{
			OutSpec = PathList(Idx);
			if(OutSpec && OutSpec->End.Nav())
			{
				ANavigationPoint* End = OutSpec->End.Nav();
				for(INT Jdx=0;Jdx<End->PathList.Num();Jdx++)
				{
					InSpec = End->PathList(Jdx);
					if(InSpec != NULL && InSpec->End.Actor == this)
					{
						InSpec->RemoveFromNavigationOctree();
						End->PathList.RemoveItem(InSpec);
						break;
					}
				}
			}
		}
	}

	GetLevel()->RemoveFromNavList( this, TRUE );
	RemoveFromNavigationOctree();
}

/**
 * Works through the component arrays marking entries as pending kill so references to them
 * will be NULL'ed.
 *
 * @param	bAllowComponentOverride		Whether to allow component to override marking the setting
 */
void ANavigationPoint::MarkComponentsAsPendingKill(UBOOL bAllowComponentOverride)
{
	Super::MarkComponentsAsPendingKill(bAllowComponentOverride);

	if (!bAllowComponentOverride)
	{
		// also mark ReachSpecs as pending kill so that any lingering references to them don't force this level to stay in memory
		for (INT i = 0; i < PathList.Num(); i++)
		{
			if (PathList(i) != NULL)
			{
				PathList(i)->MarkPendingKill();
			}
		}
	}
}

void ANavigationPoint::UpdateComponentsInternal(UBOOL bCollisionUpdate)
{
	//@fixme FIXME: What about ReachSpecs using this NavigationPoint? Should they be updated? Should we not add them to the octree in the first place?

	UBOOL bUpdateInOctree = CylinderComponent != NULL && (CylinderComponent->NeedsReattach() || CylinderComponent->NeedsUpdateTransform()) && (!bCollisionUpdate || CylinderComponent == CollisionComponent);
	
	Super::UpdateComponentsInternal(bCollisionUpdate);

	if (bUpdateInOctree)
	{
		FVector Extent(CylinderComponent->CollisionRadius, CylinderComponent->CollisionRadius, CylinderComponent->CollisionHeight);
		NavOctreeObject.SetBox(FBox(Location - Extent, Location + Extent));
	}
}

#if WITH_EDITOR
/** When a NavigationPoint is added to a Prefab, clear all the pathing information held in it. */
void ANavigationPoint::OnAddToPrefab()
{
	ClearPaths();
}
#endif

//
// Get height/radius of big cylinder around this actors colliding components.
//
void ANavigationPoint::GetBoundingCylinder(FLOAT& CollisionRadius, FLOAT& CollisionHeight) const
{
	if ( CylinderComponent )
	{
		CollisionRadius = CylinderComponent->CollisionRadius;
		CollisionHeight = CylinderComponent->CollisionHeight;
	}
	else
	{
		Super::GetBoundingCylinder(CollisionRadius, CollisionHeight);
	}
}

void ANavigationPoint::SetVolumes(const TArray<class AVolume*>& Volumes)
{
	PhysicsVolume = WorldInfo->GetDefaultPhysicsVolume();
	Super::SetVolumes( Volumes );

	if ( PhysicsVolume )
		bMayCausePain = (PhysicsVolume->DamagePerSec != 0);
}

void ANavigationPoint::SetVolumes()
{
	PhysicsVolume = WorldInfo->GetDefaultPhysicsVolume();
	Super::SetVolumes();

	if ( PhysicsVolume )
		bMayCausePain = (PhysicsVolume->DamagePerSec != 0);

}

UBOOL ANavigationPoint::CanReach(ANavigationPoint *Dest, FLOAT Dist, UBOOL bUseFlag, UBOOL bAllowFlying)
{
	if (Dist < 1.f)
	{
		return FALSE;
	}
	if ( (bUseFlag && bCanReach) || (this == Dest) )
	{
		bCanReach = TRUE;
		return TRUE;
	}

	INT NewWeight = appTrunc(Dist);
	if ( visitedWeight >= NewWeight)
	{
		return FALSE;
	}
	visitedWeight = NewWeight;
	
	for (INT i = 0; i < PathList.Num(); i++)
	{
		if ( !PathList(i)->IsProscribed() && (bAllowFlying || !(PathList(i)->reachFlags & R_FLY)))
		{
			if (PathList(i)->Distance > KINDA_SMALL_NUMBER && ~PathList(i)->End != NULL && PathList(i)->End.Nav()->CanReach(Dest, Dist - PathList(i)->Distance, FALSE, bAllowFlying))
			{
				bCanReach = TRUE;
				return TRUE;
			}
		}
	}

	return FALSE;
}

#if WITH_EDITOR
void ANavigationPoint::ReviewPath(APawn* Scout)
{
	// check for invalid path distances
	for (INT i = 0; i < PathList.Num(); i++)
	{
		if (PathList(i)->Distance <= KINDA_SMALL_NUMBER && !PathList(i)->IsProscribed())
		{
			GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NegativeOrZeroDistance" ), *PathList(i)->End->GetName() ) ), TEXT( "NegativeOrZeroDistance" ) );
		}
	}

	if ( bMustBeReachable )
	{
		for ( ANavigationPoint* M=GWorld->GetFirstNavigationPoint(); M!=NULL; M=M->nextNavigationPoint )
			M->bCanReach = false;

		// check that all other paths can reach me
		INT NumFailed = 0;
		for ( ANavigationPoint* N=GWorld->GetFirstNavigationPoint(); N!=NULL; N=N->nextNavigationPoint )
		{
			if ( !N->bDestinationOnly )
			{
				for ( ANavigationPoint* M=GWorld->GetFirstNavigationPoint(); M!=NULL; M=M->nextNavigationPoint )
				{
					M->visitedWeight = 0;
				}
				if (!N->CanReach(this, UCONST_INFINITE_PATH_COST, TRUE, TRUE))
				{
					GWarn->MapCheck_Add( MCTYPE_ERROR, N, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NotReachableFromAll" ), *GetName() ) ), TEXT( "NotReachableFromAll" ) );
					NumFailed++;
					if ( NumFailed > 8 )
						break;
				}
			}
		}
	}
}

/** Check whether there is an acceptably short connection (relative to the distance between them) between this NavigationPoint and Other
*/
UBOOL ANavigationPoint::CheckSatisfactoryConnection(ANavigationPoint* Other)
{
	for ( INT i=0; i<PathList.Num(); i++ )
		if ( PathList(i)->End == Other )
			return true;

	// check for short enough alternate path to warrant no symmetry
	FLOAT Dist = (Location - Other->Location).Size();
	for ( ANavigationPoint* N=GWorld->GetFirstNavigationPoint(); N!=NULL; N=N->nextNavigationPoint )
	{
		N->bCanReach = false;
		N->visitedWeight = 0;
	}
	return CanReach(Other, MAXTESTMOVESIZE + Dist * PATHPRUNING, FALSE, bFlyingPreferred || Other->bFlyingPreferred);
}
#endif

UReachSpec* ANavigationPoint::GetReachSpecTo( ANavigationPoint *Nav, UClass* SpecClass )
{
	for( INT i = 0; i < PathList.Num(); i++ )
	{
		UReachSpec* Spec = PathList(i);
		if(  Spec && 
			 (SpecClass == NULL || SpecClass == Spec->GetClass()) &&
			 (!Spec->bDisabled || SpecClass != NULL) &&
			 Spec->End == Nav )
		{
			return Spec;
		}
	}
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
void ANavigationPoint::PostEditMove( UBOOL bFinished )
{
	// Update all of the components of paths we connect to.  So they can update their 
	// path lines to point to our new location.
	for(INT ReachIdx=0; ReachIdx < PathList.Num(); ReachIdx++)
	{
		UReachSpec* Reach = PathList(ReachIdx);
		if( Reach )
		{
			ANavigationPoint* Nav = (ANavigationPoint*)(~Reach->End);
			if( Nav && Nav->WorldInfo )
			{
				// Only force update Nav Points that have already been added to the world
				Nav->ForceUpdateComponents(FALSE,FALSE);
			}
		}
	}

	if( bFinished )
	{
		// AddToWorld() will call this during the game if streaming level offset is specified,
		// but that case obviously doesn't require a path rebuild
		if (!GIsAssociatingLevel)
		{
			if (GWorld->GetWorldInfo()->bPathsRebuilt)
			{
				debugf(TEXT("PostEditMove Clear paths rebuilt"));
			}
			GWorld->GetWorldInfo()->bPathsRebuilt = FALSE;
		}
		bPathsChanged = TRUE;

		// Validate collision
		Validate();
	}

	Super::PostEditMove( bFinished );
}

/* if navigationpoint is spawned, paths are invalid
*/
void ANavigationPoint::Spawned()
{
	Super::Spawned();

	// Only desired update of paths for static nodes (ie NOT dynamic anchors)
	if( IsStatic() || bNoDelete )
	{
		if ( GWorld->GetWorldInfo()->bPathsRebuilt )
		{
			debugf(TEXT("Spawned Clear paths rebuilt"));
		}
		GWorld->GetWorldInfo()->bPathsRebuilt = FALSE;
		bPathsChanged = true;
	}

	if (GWorld->HasBegunPlay())
	{
		// this navpoint was dynamically spawned, make sure it's in the proper lists
		ULevel* const Level = GetLevel();
		Level->AddToNavList(this);
		Level->CrossLevelActors.AddItem( this );
		bHasCrossLevelPaths = TRUE;
	}

}

#if WITH_EDITOR
void ANavigationPoint::CleanUpPruned()
{
	for( INT i = PathList.Num() - 1; i >= 0; i-- )
	{
		if( PathList(i) && PathList(i)->bPruned )
		{
			PathList.Remove(i);
		}
	}

	PathList.Shrink();
}

UBOOL ANavigationPoint::CanConnectTo(ANavigationPoint* Nav, UBOOL bCheckDistance)
{
	if ( (bOneWayPath && (((Nav->Location - Location) | Rotation.Vector()) <= 0))
		|| (bCheckDistance && (Location - Nav->Location).SizeSquared() > MAXPATHDISTSQ) )
	{
		return false;
	}
	else
	{
		return (!Nav->bDeleteMe &&	!Nav->bNoAutoConnect &&	!Nav->bSourceOnly && !Nav->bMakeSourceOnly && Nav != this);
	}
}

UBOOL ALadder::CanConnectTo(ANavigationPoint* Nav, UBOOL bCheckDistance)
{
	// don't allow normal connection to other Ladder actors on same ladder
	ALadder *L = Cast<ALadder>(Nav);
	if ( L && (MyLadder == L->MyLadder) )
	{
		return false;
	}
	else
	{
		return Super::CanConnectTo(Nav, bCheckDistance);
	}
}
#endif

UBOOL ANavigationPoint::ShouldBeBased()
{
	return ((PhysicsVolume == NULL || !PhysicsVolume->bWaterVolume) && !bNotBased && CylinderComponent);
}

#if WITH_EDITOR
void ANavigationPoint::AddForcedSpecs( AScout *Scout )
{
}

/**
 * Builds a forced reachspec from this navigation point to the
 * specified navigation point.
 */
UReachSpec* ANavigationPoint::ForcePathTo(ANavigationPoint *Nav, AScout *Scout, UClass* ReachSpecClass )
{
	// if specified a valid nav point
	if (Nav != NULL &&
		Nav != this)
	{
		// search for the scout if not specified
		if (Scout == NULL)
		{
			Scout = FPathBuilder::GetScout();
		}
		if (Scout != NULL)
		{
			if( !ReachSpecClass )
			{
				ReachSpecClass = UForcedReachSpec::StaticClass();
			}

			// create the forced spec
			UReachSpec *newSpec = ConstructObject<UReachSpec>(ReachSpecClass,GetOuter(),NAME_None);
			FVector ForcedSize = newSpec->GetForcedPathSize( this, Nav, Scout );
			newSpec->CollisionRadius = appTrunc(ForcedSize.X);
			newSpec->CollisionHeight = appTrunc(ForcedSize.Y);
			newSpec->Start = this;
			newSpec->End = Nav;
			newSpec->Distance = appTrunc((Location - Nav->Location).Size());
			// and add the spec to the path list
			PathList.AddItem(newSpec);

			return newSpec;
		}
	}

	return NULL;
}

/**
 * If path from this NavigationPoint to Nav should be proscribed,
 * Builds a proscribed reachspec fromt his navigation point to the
 * specified navigation point.
 */
UBOOL ANavigationPoint::ProscribePathTo(ANavigationPoint *Nav, AScout *Scout)
{
	// if specified a valid nav point
	if ( Nav == NULL || Nav == this )
	{
		return TRUE;
	}

	// see if destination is in list of proscribed paths
	UBOOL bHasPath = FALSE;
	for (INT PathIdx = 0; PathIdx < EditorProscribedPaths.Num(); PathIdx++)
	{
		if (EditorProscribedPaths(PathIdx).Actor == Nav)
		{
			bHasPath = TRUE;
			break;
		}
	}
	if (!bHasPath)
	{
		return FALSE;
	}

	// create the forced spec
	UReachSpec *newSpec = ConstructObject<UReachSpec>(UProscribedReachSpec::StaticClass(),GetOuter(),NAME_None);
	// no path allowed because LD marked it - mark it with a reachspec so LDs will know there is a proscribed path here
	newSpec->Start = this;
	newSpec->End = Nav;
	newSpec->Distance = appTrunc((Location - Nav->Location).Size());
	PathList.AddItem(newSpec);
	return TRUE;
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
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NavPointBadBase" ), *GetName() ) ), TEXT( "NavPointBadBase" ) );
	}
	// warn if bad base
	if( Base && Base->bPathColliding )
	{
		if( !Base->IsStatic() && IsStatic() )
		{
			GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NavPointLocationInvalid" ), *GetName(), *Base->GetName() ) ), TEXT( "NavPointLocationInvalid" ) );
		}
		else if( Base->IsStatic() && !IsStatic() )
		{
			GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NoNeedDynamicNavPoint" ), *GetName(), *Base->GetName() ) ), TEXT( "NoNeedDynamicNavPoint" ) );
		}
	}


	// try to build a spec to every other pathnode in the level
	FVector HumanSize = Scout->GetSize(FName(TEXT("Human"),FNAME_Find));
	for( ANavigationPoint *Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint )
	{
		if((!bOnlyChanged || bPathsChanged || Nav->bPathsChanged) && Nav != this)
		{
			if( !ProscribePathTo(Nav, Scout) )
			{
				// check if paths are too close together
				if( ((Nav->Location - Location).SizeSquared() < 2.f * HumanSize.X) && 
					(Nav->GetClass()->ClassFlags & CLASS_Placeable) )
				{
					GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NavPointTooClose" ), *GetName() ) ), TEXT( "NavPointTooClose" ) );
				}

				// check if forced path
				UBOOL bForcedPath = FALSE;
				for (INT PathIdx = 0; PathIdx < EditorForcedPaths.Num(); PathIdx++)
				{
					if (EditorForcedPaths(PathIdx).Actor == Nav)
					{
						// If this node is not one way OR
						// connection is in the direction we respect
						if( !bOneWayPath ||
							((Nav->Location - Location) | Rotation.Vector()) >= 0 )
						{
							// Force the path
							ForcePathTo(Nav,Scout);
							bForcedPath = TRUE;
						}
						break;
					}
				}
				if( !bForcedPath && !bDestinationOnly && CanConnectTo( Nav, TRUE ) )
				{
					UClass*		ReachSpecClass  = GetReachSpecClass( Nav, Scout->GetDefaultReachSpecClass() );
					UReachSpec *newSpec			= ConstructObject<UReachSpec>(ReachSpecClass,GetOuter(),NAME_None);
					if( newSpec->defineFor( this, Nav, Scout ) )
					{
						// debugf(TEXT("***********added new spec from %s to %s"),*GetName(),*Nav->GetName());
						PathList.AddItem(newSpec);

						// look for paths coming the opposite direction and use the smallest of the collision found
						UReachSpec* ReturnSpec = newSpec->End.Nav()->GetReachSpecTo(this);
						if(ReturnSpec != NULL && !ReturnSpec->IsForced())
						{
							ReturnSpec->CollisionHeight = Min<INT>(ReturnSpec->CollisionHeight,newSpec->CollisionHeight);
							newSpec->CollisionHeight = ReturnSpec->CollisionHeight;
							
							ReturnSpec->CollisionRadius = Min<INT>(ReturnSpec->CollisionRadius,newSpec->CollisionRadius);
							newSpec->CollisionRadius = ReturnSpec->CollisionRadius;

							Scout->SetPathColor(ReturnSpec);
							Scout->SetPathColor(newSpec);
						}

					}
				}
			}
		}
	}
}
#endif

UBOOL ANavigationPoint::GetAllNavInRadius(class AActor* chkActor,FVector ChkPoint,FLOAT Radius,TArray<class ANavigationPoint*>& out_NavList,UBOOL bSkipBlocked,INT inNetworkID,FCylinder MinSize)
{
	TArray<FNavigationOctreeObject*> NavObjects;
	GWorld->NavigationOctree->RadiusCheck(ChkPoint,Radius,NavObjects);
	for (INT Idx = 0; Idx < NavObjects.Num(); Idx++)
	{
		ANavigationPoint *Nav = NavObjects(Idx)->GetOwner<ANavigationPoint>();
		if (Nav != NULL)
		{
			if (inNetworkID >= 0 && Nav->NetworkID != inNetworkID)
			{
				continue;
			}

			if (bSkipBlocked && Nav->bBlocked)
			{
				continue;
			}

			if( (MinSize.Height > 0 && MinSize.Height > Nav->MaxPathSize.Height) ||
				(MinSize.Radius > 0 && MinSize.Radius > Nav->MaxPathSize.Radius) )
			{
				continue;
			}

			FLOAT DistSq = (Nav->Location - ChkPoint).SizeSquared();
			UBOOL bInserted = FALSE;
			for (INT ListIdx = 0; ListIdx < out_NavList.Num(); ListIdx++)
			{
				if ((out_NavList(ListIdx)->Location - ChkPoint).SizeSquared() >= DistSq)
				{
					bInserted = TRUE;
					out_NavList.InsertItem(Nav,ListIdx);
					break;
				}
			}
			if (!bInserted)
			{
				out_NavList.AddItem(Nav);
			}
		}
	}
	return (out_NavList.Num() > 0);
}

/** Returns if this navigation point is on a different network than the given */
UBOOL ANavigationPoint::IsOnDifferentNetwork( ANavigationPoint* Nav )
{
	if( Nav != NULL )
	{
		if( Nav->NetworkID != -1 &&
			NetworkID	   != -1 && 
			NetworkID	   != Nav->NetworkID )
		{
			return TRUE;
		}
	}

	return FALSE;
}

/** sorts the PathList by distance, shortest first */
void ANavigationPoint::SortPathList()
{
	UReachSpec* TempSpec = NULL;
	for (INT i = 0; i < PathList.Num(); i++)
	{
		for (INT j = 0; j < PathList.Num() - 1; j++)
		{
			if (PathList(j)->Distance > PathList(j + 1)->Distance)
			{
				TempSpec = PathList(j+1);
				PathList(j+1) = PathList(j);
				PathList(j) = TempSpec;
			}
		}
	}
}

/** builds long range paths (> MAXPATHDIST) between this node and all other reachable nodes
 * for which a straight path would be significantly shorter or the only way to reach that node
 * done in a separate pass at the end because it's expensive and so we want to early out in the maximum number
 * of cases (e.g. if suitable short range paths already get there)
 */
#if WITH_EDITOR
void ANavigationPoint::AddLongPaths(AScout* Scout, UBOOL bOnlyChanged)
{
	if (bBuildLongPaths && !bDestinationOnly)
	{
		UReachSpec* NewSpec = ConstructObject<UReachSpec>(Scout->GetDefaultReachSpecClass(), GetOuter(), NAME_None);
		for (ANavigationPoint *Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint)
		{
			FCheckResult Hit(1.0f);
			if ( (!bOnlyChanged || bPathsChanged || Nav->bPathsChanged) && Nav->bBuildLongPaths && CanConnectTo(Nav, false) && (Nav->Location - Location).SizeSquared() > MAXPATHDISTSQ &&
				GetReachSpecTo(Nav) == NULL && GWorld->SingleLineCheck(Hit, this, Nav->Location, Location, TRACE_World | TRACE_StopAtAnyHit) && !CheckSatisfactoryConnection(Nav) &&
				NewSpec->defineFor(this, Nav, Scout) )
			{
				//debugf(TEXT("***********added long range spec from %s to %s"), *GetName(), *Nav->GetName());
				PathList.AddItem(NewSpec);
				NewSpec = ConstructObject<UReachSpec>(Scout->GetDefaultReachSpecClass(), GetOuter(), NAME_None);
			}
		}
	}
}

void ALadder::addReachSpecs(AScout *Scout, UBOOL bOnlyChanged)
{
	UReachSpec *newSpec = ConstructObject<UReachSpec>(ULadderReachSpec::StaticClass(),GetOuter(),NAME_None);

	//debugf("Add Reachspecs for Ladder at (%f, %f, %f)", Location.X,Location.Y,Location.Z);
	bPathsChanged = bPathsChanged || !bOnlyChanged;

	// connect to all ladders in same LadderVolume
	if ( MyLadder )
	{
		for( FActorIterator It; It; ++ It )
		{
			ALadder *Nav = Cast<ALadder>(*It);
			if ( Nav && (Nav != this) && (Nav->MyLadder == MyLadder) && (bPathsChanged || Nav->bPathsChanged) && Nav->GetOutermost() == GetOutermost() )
			{
				// add reachspec from this to other Ladder
				// FIXME - support single direction ladders (e.g. zipline)
				FVector CommonSize = Scout->GetSize(FName(TEXT("Common"),FNAME_Find));
				newSpec->CollisionRadius = appTrunc(CommonSize.X);
				newSpec->CollisionHeight = appTrunc(CommonSize.Y);
				newSpec->Start = this;
				newSpec->End = Nav;
				newSpec->Distance = appTrunc((Location - Nav->Location).Size());
				PathList.AddItem(newSpec);
				newSpec = ConstructObject<UReachSpec>(Scout->GetDefaultReachSpecClass(),GetOuter(),NAME_None);
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
	//debugf("Add Reachspecs for node at (%f, %f, %f)", Location.X,Location.Y,Location.Z);
	bPathsChanged = bPathsChanged || !bOnlyChanged;

	for( FActorIterator It; It; ++ It )
	{
		ATeleporter *Nav = Cast<ATeleporter>(*It);
		if (Nav != NULL && Nav != this && Nav->Tag != NAME_None && URL == Nav->Tag.ToString() && (bPathsChanged || Nav->bPathsChanged))
		{
			UReachSpec* NewSpec = ConstructObject<UReachSpec>(UTeleportReachSpec::StaticClass(),GetOuter(),NAME_None);
			FVector MaxSize = Scout->GetMaxSize();
			NewSpec->CollisionRadius = appTrunc(MaxSize.X);
			NewSpec->CollisionHeight = appTrunc(MaxSize.Y);
			NewSpec->Start = this;
			NewSpec->End = Nav;
			NewSpec->Distance = 100;
			PathList.AddItem(NewSpec);
			break;
		}
	}

	ANavigationPoint::addReachSpecs(Scout, bOnlyChanged);
}
#endif

UBOOL ATeleporter::CanTeleport(AActor* A)
{
	return (A != NULL && A->bCanTeleport && (bCanTeleportVehicles || Cast<AVehicle>(A) == NULL));
}

#if WITH_EDITOR
void APlayerStart::addReachSpecs(AScout *Scout, UBOOL bOnlyChanged)
{
	ANavigationPoint::addReachSpecs(Scout, bOnlyChanged);

	// check that playerstart is useable
	FVector HumanSize = Scout->GetSize(FName(TEXT("Human"),FNAME_Find));
	Scout->SetCollisionSize(HumanSize.X, HumanSize.Y);
	if ( !GWorld->FarMoveActor(Scout,Location,1) )
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_PlayerStartInvalid" ), *GetName() ) ), TEXT( "PlayerStartInvalid" ) );
	}

}
#endif

void ALadder::InitForPathFinding()
{
	// find associated LadderVolume
	MyLadder = NULL;
	for( FActorIterator It; It; ++ It )
	{
		ALadderVolume *V = Cast<ALadderVolume>(*It);
		if ( V && (V->Encompasses(Location) || V->Encompasses(Location - FVector(0.f, 0.f, CylinderComponent->CollisionHeight))) )
		{
			MyLadder = V;
			break;
		}
	}
	if ( !MyLadder )
	{
		// Warn if there is no ladder volume
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NoLadderVolume" ), *GetName() ) ), TEXT( "NoLadderVolume" ) );
		return;
	}

	LadderList = MyLadder->LadderList;
	MyLadder->LadderList = this;
}

/* ClearForPathFinding()
clear transient path finding properties right before a navigation network search
*/
void ANavigationPoint::ClearForPathFinding()
{
	visitedWeight	= UCONST_INFINITE_PATH_COST;
	nextOrdered		= NULL;
	prevOrdered		= NULL;
	previousPath	= NULL;
	bEndPoint		= bTransientEndPoint;
	bTransientEndPoint = FALSE;

	// Figure out total cost of movement to this node
	Cost =	ExtraCost + 
			TransientCost + 
			FearCost;

	CostArray.Empty();
	DEBUGREGISTERCOST( this, TEXT("Extra"), ExtraCost );
	DEBUGREGISTERCOST( this, TEXT("Transient"), TransientCost );
	DEBUGREGISTERCOST( this, TEXT("Fear"), FearCost );
	
	TransientCost = 0;
	bAlreadyVisited = FALSE;

	// check to see if we should delete our anchored pawn
	if (AnchoredPawn != NULL &&
		!AnchoredPawn->ActorIsPendingKill())
	{
		if (AnchoredPawn->Controller == NULL ||
			AnchoredPawn->Health <= 0)
		{
			AnchoredPawn = NULL;
		}
	}
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
	if ( GWorld->HasBegunPlay() )
	{
		return;
	}

	SetZone(1,1);
	if( ShouldBeBased() )
	{
		// not using find base, because don't want to fail if LD has navigationpoint slightly interpenetrating floor
		FCheckResult Hit(1.f);
		AScout *Scout = FPathBuilder::GetScout();
		check(Scout != NULL && "Failed to find scout for point placement");
		// get the dimensions for the average human player
		FVector HumanSize = Scout->GetSize(FName(TEXT("Human"),FNAME_Find));
		FVector CollisionSlice(HumanSize.X, HumanSize.X, 1.f);
		// and use this node's smaller collision radius if possible
		if (CylinderComponent->CollisionRadius < HumanSize.X)
		{
			CollisionSlice.X = CollisionSlice.Y = CylinderComponent->CollisionRadius;
		}
		// check for placement
#if WITH_EDITOR 

		FMemMark Mark(GMainThreadMemStack);
		FCheckResult* FirstHit = GWorld->MultiLineCheck
			(
			GMainThreadMemStack, 
			Location - FVector(0,0, 4.f * CylinderComponent->CollisionHeight),
			Location,
			CollisionSlice,
			TRACE_AllBlocking,
			Scout
			);

		//
		TArray<AActor*> ActorsWeMessedWith;
		for( FCheckResult* Check = FirstHit; Check!=NULL; Check=Check->GetNext() )
		{
			if (Check->Actor != NULL && !Check->Actor->bPathTemp)
			{
				ActorsWeMessedWith.AddItem(Check->Actor);
				Check->Actor->SetCollisionForPathBuilding(TRUE);
			}
		}
		Mark.Pop();
#endif
		
		GWorld->SingleLineCheck( Hit, Scout, Location - FVector(0,0, 4.f * CylinderComponent->CollisionHeight), Location, TRACE_AllBlocking, CollisionSlice );

		if( Hit.Actor != NULL )
		{
			if (Hit.Normal.Z > Scout->WalkableFloorZ)
			{
				GWorld->FarMoveActor(this, Hit.Location + FVector(0.f,0.f,CylinderComponent->CollisionHeight-2.f),0,1,0);
			}
			else
			{
				Hit.Actor = NULL;
			}
		}

#if WITH_EDITOR
		for(INT ActorIdx=0;ActorIdx<ActorsWeMessedWith.Num();++ActorIdx)
		{
			ActorsWeMessedWith(ActorIdx)->SetCollisionForPathBuilding(FALSE);
		}
#endif
		

		SetBase(Hit.Actor, Hit.Normal);
		if (GoodSprite != NULL)
		{
			GoodSprite->HiddenEditor = FALSE;
		}
		if (BadSprite != NULL)
		{
			BadSprite->HiddenEditor = TRUE;
		}

	}
}

#define DEBUG_PRUNEPATHS (0)

/* Prune paths when an acceptable route using an intermediate path exists
*/
#if WITH_EDITOR
INT ANavigationPoint::PrunePaths()
{
	INT pruned = 0;

#if DEBUG_PRUNEPATHS
	debugf(TEXT("Prune paths from %s"),*GetName());
#endif

	for ( INT i=0; i<PathList.Num(); i++ )
	{
		UReachSpec* iSpec = PathList(i);
		if (CanPrunePath(i) && !iSpec->IsProscribed() && !iSpec->IsForced())
		{
#if DEBUG_PRUNEPATHS
			debugf(TEXT("Try to prune %s to %s (%s)"), *iSpec->Start->GetName(), *iSpec->End->GetName(), *iSpec->GetName() );
#endif
			for (ANavigationPoint* Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint)
			{
				Nav->visitedWeight = UCONST_INFINITE_PATH_COST;
			}

			for ( INT j=0; j<PathList.Num(); j++ )
			{
				UReachSpec* jSpec = PathList(j);
				if( i != j && 
					jSpec->ShouldPruneAgainst( iSpec ) )
				{
					if( jSpec->End.Nav()->FindAlternatePath( iSpec, jSpec->Distance ) )
					{
						iSpec->bPruned = TRUE;
#if DEBUG_PRUNEPATHS
						debugf(TEXT("!!!!! Pruned path to %s (%s) because of path through %s (%s)"), *PathList(i)->End->GetName(), *PathList(i)->GetName(), *PathList(j)->End->GetName(), *PathList(j)->GetName() );
#endif

						j = PathList.Num();
						pruned++;
					}
				}
#if DEBUG_PRUNEPATHS
				else
				{

					debugf(TEXT("Reject spec %s to %s (%s) ... %d %d ShouldPrune %s - %s %s %s %s"), 
						*jSpec->Start->GetName(),
						*jSpec->End->GetName(), 
						*jSpec->GetName(),
						i, j,
						jSpec->ShouldPruneAgainst( iSpec )?TEXT("TRUE"):TEXT("FALSE"),
						jSpec->bPruned?TEXT("TRUE"):TEXT("FALSE"),
						jSpec->bSkipPrune?TEXT("TRUE"):TEXT("FALSE"),
						(*jSpec<=*iSpec)?TEXT("TRUE"):TEXT("FALSE"),
						(*jSpec->End!=NULL)?TEXT("TRUE"):TEXT("FALSE") );
				}
#endif
			}
		}
	}

	CleanUpPruned();

	// make sure ExtraCost is not negative
	ExtraCost = ::Max<INT>(ExtraCost, 0);

	UpdateMaxPathSize();
	
	return pruned;
}
#endif

#define MAX_CHECK_DIST 512.f
UBOOL NodeSupportsCollisionSize(ANavigationPoint* Node, UReachSpec* SupportsCollisionofThis)
{
	if(Node == NULL || SupportsCollisionofThis == NULL || SupportsCollisionofThis->Start == NULL || SupportsCollisionofThis->End.Nav() == NULL)
	{
		return FALSE;
	}

	// if the incomign node is smaller than both the endpoints of our spec
	if((SupportsCollisionofThis->Start->MaxPathSize.Height > Node->MaxPathSize.Height ||
		SupportsCollisionofThis->Start->MaxPathSize.Radius > Node->MaxPathSize.Radius) &&
		(SupportsCollisionofThis->End.Nav()->MaxPathSize.Height > Node->MaxPathSize.Height ||
		SupportsCollisionofThis->End.Nav()->MaxPathSize.Radius > Node->MaxPathSize.Radius)
		)
	{
		return FALSE;
	}

	return TRUE;
}
/*
 * NodeAHasShortishAlteranteRouteToNodeB
 * - recursive function which searches for an alternate route to NodeB from NodeA which does not use ProscribedSpec, and people that could use ProscribedSpec could also use (e.g. the alternate route is wide enough),
 *   and is no more than MAX_CHECK_DIST longer of a detour to get to NodeB than ProscribedSpec is
 */
UBOOL NodeAHasShortishAlteranteRouteToNodeBWorker(ANavigationPoint* NodeA, ANavigationPoint* NodeB, UReachSpec* ProscribedSpec, INT TraveledDistance, FLOAT CheckDelta)
{
	// if we've traversed too far, bail
	//debugf(TEXT("-> %s->%s"),*NodeA->GetName(),*NodeB->GetName());
	if(TraveledDistance > ProscribedSpec->Distance + CheckDelta)
	{
		//debugf(TEXT("not progressing because traveleddistance is too big %i vs %i"),TraveledDistance,ProscribedSpec->Distance+CheckDelta);
		return FALSE;
	}
	else if(NodeB == NodeA) // then we found the node we're looking for
	{
		//debugf(TEXT("Found alternate route, googgogo!"));
		return TRUE;
	}

	
	if(NodeA->visitedWeight <= TraveledDistance)
	{
		return FALSE;
	}
	else
	{
		NodeA->visitedWeight = TraveledDistance;
	}

	UReachSpec* CurSpec = NULL;
	for(INT Idx=0; Idx < NodeA->PathList.Num(); Idx++)
	{
		CurSpec = NodeA->PathList(Idx);
		// if this is the spec we're checking against, or it doesn't support the size of the spec we're checking against.., or the endpoints don't support the proscribed spec's size skip
		if(CurSpec != ProscribedSpec &&
			CurSpec->ShouldPruneAgainst(ProscribedSpec)			
		  )
		{
			if(NodeAHasShortishAlteranteRouteToNodeBWorker(CurSpec->End.Nav(),NodeB,ProscribedSpec,TraveledDistance+CurSpec->Distance,CheckDelta))
			{
				return TRUE;
			}		
		}
		else if( CurSpec != ProscribedSpec)
		{
		/*	debugf(TEXT("ShouldPruneAgainst forbade %s->%s Pruned? %i SkipPrune? %i EndNULL? %i Rad/height: %i/%i vs %i/%i Flags: %i vs %i LandingVel: %i vs %i <=? %i"),
				*CurSpec->Start->GetName(),*CurSpec->End.Actor->GetName(),
				CurSpec->bPruned,
				CurSpec->bSkipPrune,
				(INT)CurSpec->End.Actor,
				CurSpec->CollisionRadius,
				CurSpec->CollisionHeight,
				ProscribedSpec->CollisionRadius,
				ProscribedSpec->CollisionHeight,
				CurSpec->reachFlags,
				ProscribedSpec->reachFlags,
				CurSpec->MaxLandingVelocity,
				ProscribedSpec->MaxLandingVelocity,
				*CurSpec<=*ProscribedSpec);*/
		}
	}
	return FALSE;
}

UBOOL NodeAHasShortishAlteranteRouteToNodeB(ANavigationPoint* NodeA, ANavigationPoint* NodeB, UReachSpec* ProscribedSpec, INT TraveledDistance, FLOAT CheckDelta)
{
	//debugf(TEXT("NodeAHasShortishAlteranteRouteToNodeB -----------------------------"));
	for (ANavigationPoint* Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint)
	{
		Nav->visitedWeight = UCONST_INFINITE_PATH_COST;
	}
	return NodeAHasShortishAlteranteRouteToNodeBWorker(NodeA,NodeB,ProscribedSpec,TraveledDistance,CheckDelta);
}

UBOOL PruneLongerPath(UReachSpec* iSpec, UReachSpec *jSpec, INT& pruned, FLOAT CheckDelta)
{
	
	// we need to check to make sure the opposite spec can be pruned before we determine based on distance..
	// this is so that we will prune a spec even if it's shorter when the longer spec isn't eligible for pruning
	UBOOL bOtherSpecCanBePruned = NodeAHasShortishAlteranteRouteToNodeB(jSpec->Start,jSpec->End.Nav(),jSpec,0,CheckDelta);
	if(bOtherSpecCanBePruned)
	{
		// if there is a spec going in the opposite direction prune that at the same time
		UReachSpec* OtherDirSpec = jSpec->End.Nav()->GetReachSpecTo(jSpec->Start);
		// see if we prune iSpec, if the nodes that spec connects will still be connected to each other
		bOtherSpecCanBePruned = (OtherDirSpec == NULL || NodeAHasShortishAlteranteRouteToNodeB(OtherDirSpec->Start,OtherDirSpec->End.Nav(),OtherDirSpec,0,CheckDelta));
	}

	// pick the longer edge and prune it
	if(iSpec->Distance > jSpec->Distance || !bOtherSpecCanBePruned)
	{

		// see if we prune iSpec, if the nodes that spec connects will still be connected to each other

		if(NodeAHasShortishAlteranteRouteToNodeB(iSpec->Start,iSpec->End.Nav(),iSpec,0,CheckDelta))
		{
			// if there is a spec going in the opposite direction prune that at the same time
			UReachSpec* OtherDirSpec = iSpec->End.Nav()->GetReachSpecTo(iSpec->Start);

			// see if we prune iSpec, if the nodes that spec connects will still be connected to each other
			if(OtherDirSpec == NULL || NodeAHasShortishAlteranteRouteToNodeB(OtherDirSpec->Start,OtherDirSpec->End.Nav(),OtherDirSpec,0,CheckDelta))
			{
				/*debugf(TEXT("Pruning %s->%s in favor of %s->%s %i/%i"),
					*iSpec->Start->GetName(),
					*iSpec->End->GetName(),
					*jSpec->Start->GetName(),
					*jSpec->End->GetName(),
					iSpec->Distance,
					jSpec->Distance);*/
				
				// there was another path to get to that node
				iSpec->bPruned=TRUE;

				if(OtherDirSpec)
				{
					OtherDirSpec->bPruned = TRUE;
					pruned++;
				}
				pruned++;
				return TRUE;
			}
		}
	}

	return FALSE;
}

IMPLEMENT_COMPARE_POINTER( UReachSpec, Distance, { return (B->Distance - A->Distance); } )

#if WITH_EDITOR
INT ANavigationPoint::AggressivePrunePaths()
{
	INT pruned = 0;
	AScout* Scout = FPathBuilder::GetScout();	
	for ( INT i=0; i<PathList.Num(); i++ )
	{
		UReachSpec* iSpec = PathList(i);
		TArray<UReachSpec*> IntersectingSpecs;
		if (!iSpec->bPruned && CanPrunePath(i) && !iSpec->IsProscribed() && !iSpec->IsForced())
		{
			TArray<FNavigationOctreeObject*> NavObjects;  
			GWorld->NavigationOctree->RadiusCheck(Location, MAXPATHDIST, NavObjects);
			for(INT Idx=0; Idx < NavObjects.Num(); Idx++)
			{
				ANavigationPoint* Nav = NavObjects(Idx)->GetOwner<ANavigationPoint>();
				if(Nav == NULL || Nav == this || *iSpec->End == Nav)
				{
					continue;
				}

				FVector Pt1, Pt2;
				for ( INT j=0; j<Nav->PathList.Num(); j++ )
				{	
					UReachSpec* jSpec = Nav->PathList(j);	
					// if this edge has been pruned, or it shares an endpoint with iSpec, or shouldpruneagainst fails, skip it
					if(jSpec->bPruned ||
						jSpec->End == iSpec->End || jSpec->End == iSpec->Start || iSpec->End == jSpec->Start || iSpec->Start == jSpec->Start ||
						!jSpec->ShouldPruneAgainst(iSpec) ||
						!Nav->CanPrunePath(j)) // if this is a spec which can't be pruned, don't care if it intersects other specs
					{			
						continue;
					}

					//debugf(TEXT("Checking to see if spec for %s->%s intersects %s->%s! --" ),*iSpec->Start->GetName(),*iSpec->End->GetName(),*jSpec->Start->GetName(),*jSpec->End->GetName());
					SegmentDistToSegmentSafe(iSpec->Start->Location,iSpec->End->Location,jSpec->Start->Location,jSpec->End->Location,Pt1,Pt2);

			
					if((Pt1-Pt2).Size2D() < 25.f && Abs<FLOAT>(Pt1.Z - Pt2.Z) < Scout->GetDefaultCollisionSize().Z )
					{			
						IntersectingSpecs.AddItem(jSpec);
					}	
				}

				if(iSpec->bPruned)
				{
					break;
				}
			}

			// we add all the intersecting specs to a list, and then sort the list based on length of the edges, so that 
			// we prune the longest edges first 
			Sort<USE_COMPARE_POINTER(UReachSpec,Distance)>(&IntersectingSpecs(0),IntersectingSpecs.Num());
			for( INT j=0;j<IntersectingSpecs.Num();j++)
			{
				UReachSpec* jSpec = IntersectingSpecs(j);
				PruneLongerPath(jSpec,iSpec,pruned,Max<FLOAT>(iSpec->Distance * 0.6f,MAX_CHECK_DIST));
			}
		}
	}

	CleanUpPruned();

	// make sure ExtraCost is not negative
	ExtraCost = ::Max<INT>(ExtraCost, 0);

	UpdateMaxPathSize();

	return pruned;
}

INT ANavigationPoint::SecondPassAggressivePrunePaths()
{
	INT pruned = 0;
	// second pass, prune out paths close to the same angle
	for ( INT i=0; i<PathList.Num(); i++ )
	{
		UReachSpec* iSpec = PathList(i);
		if (!iSpec->bPruned && CanPrunePath(i) && !iSpec->IsProscribed() && !iSpec->IsForced())
		{
			for ( INT j=0; j<PathList.Num(); j++ )
			{	
				UReachSpec* jSpec = PathList(j);	
				// if this edge has been pruned, or it shares an endpoint with iSpec, or shouldpruneagainst fails, skip it
				if(jSpec->bPruned ||
					jSpec == iSpec ||
					!jSpec->ShouldPruneAgainst(iSpec) ||
					!CanPrunePath(j) || 
					(iSpec->Direction | jSpec->Direction) < 0.807f ) // within 35 degrees of each other
				{			
					continue;
				}

				if(PruneLongerPath(iSpec,jSpec,pruned,PATHPRUNING * iSpec->Distance))
				{
					break;
				}
			}
		}
	}

	CleanUpPruned();

	// make sure ExtraCost is not negative
	ExtraCost = ::Max<INT>(ExtraCost, 0);

	UpdateMaxPathSize();

	return pruned;
}
#endif

void ANavigationPoint::UpdateMaxPathSize()
{
	// set MaxPathSize based on remaining paths
	MaxPathSize.Radius = MaxPathSize.Height = 0.f;
	for (INT i = 0; i < PathList.Num(); i++)
	{
		UReachSpec* Spec = PathList(i);
		if( !Spec->bDisabled )
		{
			MaxPathSize.Radius = ::Max<FLOAT>(MaxPathSize.Radius, PathList(i)->CollisionRadius);
			MaxPathSize.Height = ::Max<FLOAT>(MaxPathSize.Height, PathList(i)->CollisionHeight);
		}
	}
}

/** recursively determines if there is an acceptable alternate path to the passed in one
 * used by path pruning
 * this routine uses each NavigationPoint's visitedWeight to avoid unnecessarily checking nodes
 * that have already been checked with a better distance, so you need to initialize them by
 * setting it to UCONST_INFINITE_PATH_COST on all NavigationPoints before calling this function
 */
UBOOL ANavigationPoint::FindAlternatePath(UReachSpec* StraightPath, INT AccumulatedDistance)
{
	if ( bBlocked || bBlockable )
	{
		return FALSE;
	}
	if (StraightPath->Start == NULL || *StraightPath->End == NULL)
	{
		return FALSE;
	}

	// if we have already visited this node with a lower distance, there is no point in trying it again
	if (visitedWeight <= AccumulatedDistance)
	{
		return FALSE;
	}
	visitedWeight = AccumulatedDistance;

#if DEBUG_PRUNEPATHS
	debugf(TEXT("%s FindAlternatePath Straight %s to %s (%s) AccDist %d"), *GetName(), *StraightPath->Start->GetName(), *StraightPath->End->GetName(), *StraightPath->GetName(), AccumulatedDistance );
#endif

	FVector StraightDir = (StraightPath->End->Location - StraightPath->Start->Location).SafeNormal();
	FLOAT DistThresh = PATHPRUNING * StraightPath->Distance;

#if DEBUG_PRUNEPATHS
	debugf(TEXT("Dir %s Thresh %f"), *StraightDir.ToString(), DistThresh );
#endif

	// check if the endpoint is directly reachable
	for (INT i = 0; i < PathList.Num(); i++)
	{
		UReachSpec* Spec = PathList(i);
		if( !Spec->bPruned && 
			 Spec->End.Nav() == StraightPath->End.Nav() )
		{
			FLOAT DotP = (StraightDir | (StraightPath->End->Location - Location).SafeNormal());

#if DEBUG_PRUNEPATHS
			debugf(TEXT("Test direct reach %s to %s (%s) DotP %f"), *Spec->Start->GetName(), *Spec->End->GetName(), *Spec->GetName(), DotP );
#endif

			if( DotP >= 0.f )
			{
				FLOAT Dist = AccumulatedDistance + Spec->Distance;

#if DEBUG_PRUNEPATHS
				debugf( TEXT("Dist check %f thresh %f ShouldPrune %d"), Dist, DistThresh, Spec->ShouldPruneAgainst( StraightPath ) );
				if( !Spec->ShouldPruneAgainst(StraightPath) )
				{
					debugf(TEXT("Str8 %s (%s) bPruned %s bSkipPrune %s End %s PruneList %d %d operator %s %s %s Rad %d %d Height %d %d"), 
						*StraightPath->GetName(), 
						*Spec->GetName(), 
						Spec->bPruned?TEXT("TRUE"):TEXT("FALSE"),
						Spec->bSkipPrune?TEXT("TRUE"):TEXT("FALSE"),
						*Spec->End->GetName(),
						Spec->PruneSpecList.FindItemIndex(StraightPath->GetClass()),
						StraightPath->PruneSpecList.FindItemIndex(Spec->GetClass()),
						(*Spec <= *StraightPath) ? TEXT("TRUE") : TEXT("FALSE"),
						Spec->IsProscribed()?TEXT("TRUE"):TEXT("FALSE"),
						Spec->IsForced()?TEXT("TRUE"):TEXT("FALSE"),
						Spec->CollisionRadius, StraightPath->CollisionRadius,
						Spec->CollisionHeight, StraightPath->CollisionHeight );
				}
#endif
				if( (Dist < DistThresh) && Spec->ShouldPruneAgainst( StraightPath ) )
				{
#if DEBUG_PRUNEPATHS
					debugf(TEXT(">>>> Direct path from %s to %s (%s)"),*GetName(), *Spec->End->GetName(), *Spec->GetName() );
#endif
					return TRUE;
				}
				else
				{
#if DEBUG_PRUNEPATHS
					debugf( TEXT(">>>> No direct path from %s to %s (%s)"), *GetName(), *Spec->End->GetName(), *Spec->GetName() );
#endif

					return FALSE;
				}
			}
		}
	}

	// now continue looking for path
	for ( INT i=0; i<PathList.Num(); i++ )
	{
		UReachSpec* Spec = PathList(i);

#if DEBUG_PRUNEPATHS
		debugf(TEXT("try to recurse with %s to %s (%s)... ShouldPrune %s SpecDist %d AccDist %d Thresh %d"), 
			*Spec->Start->GetName(), 
			*Spec->End->GetName(), 
			*Spec->GetName(),
			Spec->ShouldPruneAgainst(StraightPath)?TEXT("TRUE"):TEXT("FALSE"),
			Spec->Distance,
			AccumulatedDistance + Spec->Distance, 
			appTrunc(PATHPRUNING * StraightPath->Distance) );
#endif

		if ( Spec->ShouldPruneAgainst( StraightPath )
			&& (Spec->Distance > 0)
			&& (AccumulatedDistance + Spec->Distance < appTrunc(PATHPRUNING * StraightPath->Distance))
			&& (Spec->End != StraightPath->Start)
			&& ((StraightDir | (Spec->End->Location - Location).SafeNormal()) > 0.f)
			)
		{
			if( Spec->End.Nav()->FindAlternatePath(StraightPath, AccumulatedDistance + Spec->Distance) )
			{
#if DEBUG_PRUNEPATHS
				debugf(TEXT("Partial path from %s to %s"),*GetName(), *Spec->End->GetName());
#endif

				return TRUE;
			}
		}
	}

	return FALSE;
}

//----------------------------------------------------------------
// Lift navigation support

void ALiftExit::ReviewPath(APawn* Scout)
{
	if ( !MyLiftCenter )
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NoLiftCenter" ), *GetName() ) ), TEXT( "NoLiftCenter" ) );
	}
}

void ALiftCenter::FindBase()
{
	if ( GWorld->HasBegunPlay() )
	{
		return;
	}

	SetZone(1,1);
	// FIRST, turn on collision temporarily for InterpActors
	for( FActorIterator It; It; ++It )
	{
		AInterpActor *Actor = Cast<AInterpActor>(*It);
		if( Actor && !Actor->bDeleteMe && Actor->bPathTemp )
		{
			Actor->SetCollision( TRUE, Actor->bBlockActors, Actor->bIgnoreEncroachers );
		}
	}

	// not using find base, because don't want to fail if LD has navigationpoint slightly interpenetrating floor
	FCheckResult Hit(1.f);
	AScout *Scout = FPathBuilder::GetScout();
	check(Scout != NULL && "Failed to find scout for point placement");
	// get the dimensions for the average human player
	FVector HumanSize = Scout->GetSize(FName(TEXT("Human"),FNAME_Find));
	FVector CollisionSlice(HumanSize.X, HumanSize.X, 1.f);
	// and use this node's smaller collision radius if possible
	if (CylinderComponent->CollisionRadius < HumanSize.X)
	{
		CollisionSlice.X = CollisionSlice.Y = CylinderComponent->CollisionRadius;
	}
	GWorld->SingleLineCheck( Hit, Scout, Location - FVector(0,0, 2.f * CylinderComponent->CollisionHeight), Location, TRACE_AllBlocking );

	// check for placement
	GWorld->SingleLineCheck( Hit, Scout, Location - FVector(0,0, 2.f * CylinderComponent->CollisionHeight), Location, TRACE_AllBlocking, CollisionSlice );
	if (Hit.Actor != NULL)
	{
		if (Hit.Normal.Z > Scout->WalkableFloorZ)
		{
			GWorld->FarMoveActor(this, Hit.Location + FVector(0.f,0.f,CylinderComponent->CollisionHeight-1.f),0,1,0);
		}
		else
		{
			Hit.Actor = NULL;
		}
	}

	SetBase(Hit.Actor, Hit.Normal);

	// Turn off collision for InterpActors
	for( FActorIterator It; It; ++It )
	{
		AInterpActor *Actor = Cast<AInterpActor>(*It);
		if( Actor && !Actor->bDeleteMe && Actor->bPathTemp )
		{
			Actor->SetCollision( FALSE, Actor->bBlockActors, Actor->bIgnoreEncroachers );
		}
	}
}

UBOOL ALiftCenter::ShouldBeBased()
{
	return true;
}

#if WITH_EDITOR
void ALiftCenter::ReviewPath(APawn* Scout)
{
	if ( !MyLift || (MyLift != Base) )
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NeedInterpActorBase" ), *GetName() ) ), TEXT( "NeedInterpActorBase" ) );
	}
	if ( PathList.Num() == 0 )
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NoLiftExit" ), *GetName() ) ), TEXT( "NoLiftExit" ) );
	}
}


/* addReachSpecs()
Virtual function - adds reachspecs to LiftCenter for every associated LiftExit.
*/
void ALiftCenter::addReachSpecs(AScout *Scout, UBOOL bOnlyChanged)
{
	bPathsChanged = bPathsChanged || !bOnlyChanged;

	// find associated mover
	FindBase();
	MyLift = Cast<AInterpActor>(Base);
	if (  MyLift && (MyLift->GetOutermost() != GetOutermost()) )
		MyLift = NULL;

	// Warn if there is no lift
	if ( !MyLift )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NeedInterpActorBase" ), *GetName() ) ), TEXT( "NeedInterpActorBase" ) );
	}
	else
	{
		MyLift->MyMarker = this;
	}

	UReachSpec *newSpec = ConstructObject<UReachSpec>(UAdvancedReachSpec::StaticClass(),GetOuter(),NAME_None);
	//debugf("Add Reachspecs for LiftCenter at (%f, %f, %f)", Location.X,Location.Y,Location.Z);
	INT NumExits = 0;
	FVector MaxCommonSize = Scout->GetSize(FName(TEXT("Max"),FNAME_Find));

	for (ANavigationPoint *Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint)
	{
		ALiftExit *LE = Cast<ALiftExit>(Nav); 
		if ( LE && !LE->bDeleteMe && (LE->MyLiftCenter == this) && (!bOnlyChanged || bPathsChanged || LE->bPathsChanged) && (LE->GetOutermost() == GetOutermost()) ) 
		{
			NumExits++;

			// add reachspec from LiftCenter to LiftExit
			newSpec->CollisionRadius = appTrunc(MaxCommonSize.X);
			newSpec->CollisionHeight = appTrunc(MaxCommonSize.Y);
			newSpec->Start = this;
			newSpec->End = LE;
			newSpec->Distance = 500;
			PathList.AddItem(newSpec);
			newSpec = ConstructObject<UReachSpec>(UAdvancedReachSpec::StaticClass(),GetOuter(),NAME_None);

			// add reachspec from LiftExit to LiftCenter
			if ( !LE->bExitOnly )
			{
				newSpec->CollisionRadius = appTrunc(MaxCommonSize.X);
				newSpec->CollisionHeight = appTrunc(MaxCommonSize.Y);
				newSpec->Start = LE;
				newSpec->End = this;
				newSpec->Distance = 500;
				LE->PathList.AddItem(newSpec);
				newSpec = ConstructObject<UReachSpec>(UAdvancedReachSpec::StaticClass(),GetOuter(),NAME_None);
			}
		}
	}
	
	// Warn if no lift exits
	if ( NumExits == 0 )
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NoLiftExit" ), *GetName() ) ), TEXT( "NoLiftExit" ) );
	}
}
#endif

UBOOL ALadder::ReachedBy(APawn *P, const FVector &TestPosition, const FVector& Dest)
{
	// look at difference along ladder direction
	return (P && P->OnLadder && (Abs((Dest - TestPosition) | P->OnLadder->ClimbDir) < P->CylinderComponent->CollisionHeight) );
}

/** returns the position the AI should move toward to reach this actor */
FVector ANavigationPoint::GetDestination(AController* C)
{
	FVector Dest = Super::GetDestination(C);

	if ((!bCollideActors || !bMustTouchToReach) && C != NULL && C->CurrentPath != NULL && C->Pawn != NULL && !(C->CurrentPath->reachFlags & R_JUMP))
	{
		if (C->bUsingPathLanes)
		{
			// move to the right side of the path as far as possible
			Dest -= (C->CurrentPathDir ^ FVector(0.f, 0.f, 1.f)) * C->LaneOffset;
		}
		// if the Controller is on a normal path (not requiring jumping or special movement and not forced)
		else if ( !bSpecialMove && C->ShouldOffsetCorners() && C->NextRoutePath != NULL && C->NextRoutePath->Start != NULL && *C->NextRoutePath->End != NULL &&
				C->Pawn->Physics != PHYS_RigidBody && C->CurrentPath->bCanCutCorners &&
				C->NextRoutePath->bCanCutCorners )
		{
			// offset destination in the direction of the next path (cut corners)
			FLOAT ExtraRadius = FLOAT(C->CurrentPath->CollisionRadius) - C->Pawn->CylinderComponent->CollisionRadius;
			if (ExtraRadius > 0.f)
			{
				Dest += (C->NextRoutePath->End->Location - C->NextRoutePath->Start->Location).SafeNormal2D() * ExtraRadius;
			}
		}
	}

	return Dest;
}		

UBOOL ANavigationPoint::ReachedBy(APawn *P, const FVector &TestPosition, const FVector& Dest)
{
	if ( TouchReachSucceeded(P, TestPosition) )
	{
		return TRUE;
	}

	// if touch reach failed and bMustTouchToReach=true, fail
	if ( bCollideActors && P->bCollideActors )
	{
		// rigid bodies don't get based on their floor, so if we're both blocking, it's impossible for TouchReachSucceeded() to ever return true, so ignore bMustTouchToReach
		if (P->Physics != PHYS_RigidBody || !bBlockActors || !P->bBlockActors || (CollisionComponent != NULL && CollisionComponent->CollideActors && !CollisionComponent->BlockActors))
		{
			if ( bBlockActors && !bMustTouchToReach )
			{
				if( P->bBlockActors && CollisionComponent )
				{
					FCheckResult Hit(1.f);

					if ( !CollisionComponent->LineCheck(Hit, TestPosition + 30.f*(Location - TestPosition).SafeNormal(), TestPosition, P->GetCylinderExtent(), 0) )
						return true;
				}

			}
			else if ( !GIsEditor && bMustTouchToReach )
			{
				return FALSE;
			}
		}
	}

	// allow success if vehicle P is based on is covering this node
	APawn *V = P->GetVehicleBase();
	if ( V && (Abs(V->Location.Z - Dest.Z) < V->CylinderComponent->CollisionHeight) )
	{
		FVector VDir = V->Location - Dest;
		VDir.Z = 0.f;
		if ( VDir.SizeSquared() < 1.21f * V->CylinderComponent->CollisionRadius * V->CylinderComponent->CollisionRadius )
			return true;
	}

	if ( P->Controller && P->Controller->ForceReached(this, TestPosition) )
	{
		return TRUE;
	}

	// get the pawn's normal height (might be crouching or a Scout, so use the max of current/default)
	FLOAT PawnHeight = Max<FLOAT>(P->CylinderComponent->CollisionHeight, ((APawn *)(P->GetClass()->GetDefaultObject()))->CylinderComponent->CollisionHeight);

	return P->ReachThresholdTest(TestPosition, Dest, this, 
		::Max(0.f,CylinderComponent->CollisionHeight - PawnHeight + P->MaxStepHeight + MAXSTEPHEIGHTFUDGE), 
		::Max(0.f,2.f + P->MaxStepHeight - CylinderComponent->CollisionHeight), 
		0.f);	
}

UBOOL ANavigationPoint::TouchReachSucceeded(APawn* P, const FVector& TestPosition)
{
	return (bCanWalkOnToReach && TestPosition == P->Location && P->Base == this && !GIsEditor) ? TRUE : Super::TouchReachSucceeded(P, TestPosition);
}

UBOOL APickupFactory::ReachedBy(APawn* P, const FVector& TestPosition, const FVector& Dest)
{
	UBOOL bResult;

	if (bMustTouchToReach)
	{
		// only actually need to touch if pawn can pick me up
		bMustTouchToReach = P->bCanPickupInventory;
		bResult = Super::ReachedBy(P, TestPosition, Dest);
		bMustTouchToReach = TRUE;
	}
	else
	{
		bResult = Super::ReachedBy(P, TestPosition, Dest);
	}

	return bResult;
}

ANavigationPoint* APickupFactory::SpecifyEndAnchor(APawn* RouteFinder)
{
	APickupFactory* Result = this;
	while (Result->OriginalFactory != NULL)
	{
		Result = Result->OriginalFactory;
	}

	return Result;
}

/** returns whether this NavigationPoint is valid to be considered as an Anchor (start or end) for pathfinding by the given Pawn
 * @param P the Pawn doing pathfinding
 * @return whether or not we can be an anchor
 */
UBOOL ANavigationPoint::IsUsableAnchorFor(APawn* P)
{
	return ( !bBlocked && (!bFlyingPreferred || P->bCanFly) && (!bBlockedForVehicles || !P->IsA(AVehicle::StaticClass())) &&
		MaxPathSize.Radius >= P->CylinderComponent->CollisionRadius && MaxPathSize.Height >= P->CylinderComponent->CollisionHeight && P->IsValidAnchor(this) );
}

void AVolumePathNode::InitForPathFinding()
{
	// calculate flightradius
	// assume starting with reasonable estimate
	CylinderComponent->CollisionHeight = StartingHeight;
	CylinderComponent->CollisionRadius = StartingRadius;

	// look for floor
	FCheckResult Hit(1.f);
	GWorld->SingleLineCheck(Hit, this, Location - FVector(0.f,0.f,CylinderComponent->CollisionHeight), Location, TRACE_World);
	if ( Hit.Actor )
		CylinderComponent->CollisionHeight *= Hit.Time;
	GWorld->SingleLineCheck(Hit, this, Location + FVector(0.f,0.f,CylinderComponent->CollisionHeight), Location, TRACE_World);
	if ( Hit.Actor )
		CylinderComponent->CollisionHeight *= Hit.Time;
	FLOAT MaxHeight = CylinderComponent->CollisionHeight;

	GWorld->SingleLineCheck(Hit, this, Location - FVector(CylinderComponent->CollisionRadius,0.f,0.f), Location, TRACE_World);
	if ( Hit.Actor )
		CylinderComponent->CollisionRadius *= Hit.Time;
	GWorld->SingleLineCheck(Hit, this, Location + FVector(CylinderComponent->CollisionRadius,0.f,0.f), Location, TRACE_World);
	if ( Hit.Actor )
		CylinderComponent->CollisionRadius *= Hit.Time;

	GWorld->SingleLineCheck(Hit, this, Location - FVector(0.f,CylinderComponent->CollisionRadius,0.f), Location, TRACE_World);
	if ( Hit.Actor )
		CylinderComponent->CollisionRadius *= Hit.Time;
	GWorld->SingleLineCheck(Hit, this, Location + FVector(0.f,CylinderComponent->CollisionRadius,0.f), Location, TRACE_World);
	if ( Hit.Actor )
		CylinderComponent->CollisionRadius *= Hit.Time;

	// refine radius with non-zero extent point checks
	FVector Extent(CylinderComponent->CollisionRadius,CylinderComponent->CollisionRadius, CylinderComponent->CollisionRadius);
	FVector Unknown = 0.5f * Extent;
	while ( Unknown.X > 2.f )
	{
		if ( GWorld->EncroachingWorldGeometry( Hit, Location, Extent ) )
			Extent -= Unknown;
		else if ( Extent.X >= CylinderComponent->CollisionRadius )
			Unknown.X = 0.f;
		else
			Extent += Unknown;
		Unknown *= 0.5f;
	}
	Extent = Extent - Unknown - FVector(2.f,2.f,2.f);
	if ( Extent.X < 2.f )
	{
		CylinderComponent->CollisionRadius = 2.f;
		CylinderComponent->CollisionHeight = 2.f;
		return;
	}
	CylinderComponent->CollisionRadius = Extent.X;
	CylinderComponent->CollisionHeight = CylinderComponent->CollisionRadius;

	Extent = FVector(CylinderComponent->CollisionRadius,CylinderComponent->CollisionRadius,CylinderComponent->CollisionHeight+4.f);
	if ( !GWorld->EncroachingWorldGeometry( Hit, Location, Extent ) )
	{
		// try to increase height
		Extent.Z = MaxHeight;
		Unknown = 0.5f * Extent;
		Unknown.X = 0.f;
		Unknown.Y = 0.f;
		while ( Unknown.Z > 2.f )
		{
			if ( GWorld->EncroachingWorldGeometry( Hit, Location, Extent ) )
				Extent -= Unknown;
			else if ( Extent.Z >= MaxHeight )
				Unknown.Z = 0.f;
			else
				Extent += Unknown;
			Unknown *= 0.5f;
		}
		CylinderComponent->CollisionHeight = Extent.Z;
	}
	// try to increase radius
	Extent.Z = CylinderComponent->CollisionHeight;
	Extent.X = 4.f * CylinderComponent->CollisionRadius;
	Extent.Y = Extent.X;
	Unknown = 0.5f * Extent;
	Unknown.Z = 0.f;
	while ( Unknown.X > 2.f )
	{
		if ( GWorld->EncroachingWorldGeometry( Hit, Location, Extent ) )
			Extent -= Unknown;
		else if ( Extent.X >= 6.f * CylinderComponent->CollisionRadius )
			Unknown.X = 0.f;
		else
			Extent += Unknown;
		Unknown *= 0.5f;
	}
	CylinderComponent->CollisionRadius = Extent.X;
}

/* addReachSpecs()
Virtual function - adds reachspecs to path for every path reachable from it. 
*/
#if WITH_EDITOR
void AVolumePathNode::addReachSpecs(AScout * Scout, UBOOL bOnlyChanged)
{
	bPathsChanged = bPathsChanged || !bOnlyChanged;
	UReachSpec *newSpec = ConstructObject<UReachSpec>(Scout->GetDefaultReachSpecClass(),GetOuter(),NAME_None);

	FVector HumanSize = Scout->GetSize(FName(TEXT("Human"),FNAME_Find));

	// add paths to paths that are within FlightRadius, or intersecting flightradius (intersection radius defines path radius, or dist from edge of radius)
	// Note that none flying nodes need to have a path added from them as well
	for (ANavigationPoint *Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint)
	{
		if ( !Nav->bDeleteMe && (!Nav->bNoAutoConnect || Cast<AVolumePathNode>(Nav)) && !Nav->bSourceOnly && !Nav->bMakeSourceOnly
				&& (Nav != this) && (bPathsChanged || Nav->bPathsChanged) && (Nav->GetOutermost() == GetOutermost()) )
		{
			if ( !ProscribePathTo(Nav, Scout) )
			{
				// check if paths are too close together
				if ( ((Nav->Location - Location).SizeSquared() < 2.f * HumanSize.X) && (Nav->GetClass()->ClassFlags & CLASS_Placeable) )
				{
					GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NavPointTooClose" ), *GetName() ) ), TEXT( "NavPointTooClose" ) );
				}
			
				// check if forced path
				UBOOL bForced = 0;
				for (INT idx = 0; idx < EditorForcedPaths.Num() && !bForced; idx++)
				{
					if (Nav == EditorForcedPaths(idx).Nav())
					{
						bForced = 1;
					}
				}
				if ( bForced )
				{
					ForcePathTo(Nav,Scout);
				}
				else if ( !bDestinationOnly )
				{
					AVolumePathNode *FlyNav = Cast<AVolumePathNode>(Nav);
					FLOAT SpecRadius = -1.f;
					FLOAT SpecHeight = -1.f;
					FLOAT Dist2D = (Nav->Location - Location).Size2D();
					if ( Dist2D < Nav->CylinderComponent->CollisionRadius + CylinderComponent->CollisionRadius )
					{
						// check if visible
						FCheckResult Hit(1.f);
						GWorld->SingleLineCheck( Hit, this, Nav->Location, Location, TRACE_World|TRACE_StopAtAnyHit );
						if ( !Hit.Actor || (Hit.Actor == Nav) )
						{
							if ( FlyNav )
							{
								SpecHeight = CylinderComponent->CollisionHeight + FlyNav->CylinderComponent->CollisionHeight - Abs(FlyNav->Location.Z - Location.Z);
								if ( SpecHeight >= HumanSize.Y )
								{
									// reachspec radius based on intersection of circles
									if ( Dist2D > 1.f )
									{
										FLOAT R1 = CylinderComponent->CollisionRadius;
										FLOAT R2 = FlyNav->CylinderComponent->CollisionRadius;
										FLOAT Part = 0.5f*Dist2D + (R1*R1 - R2*R2)/(2.f*Dist2D);
										SpecRadius = ::Max<FLOAT>(SpecRadius,appSqrt(R1*R1 - Part*Part));
									}
									else
									{
										SpecRadius = ::Min<FLOAT>(CylinderComponent->CollisionRadius,FlyNav->CylinderComponent->CollisionRadius);
									}
									//debugf(TEXT("Radius from %s to %s is %f"),*GetName(),*FlyNav->GetName(),SpecRadius);
								}
							}
							else if ( Dist2D < CylinderComponent->CollisionRadius )
							{
								// if nav inside my cylinder, definitely add
								if ( Abs(Nav->Location.Z - Location.Z) < CylinderComponent->CollisionHeight )
								{
									SpecRadius = 0.75f * CylinderComponent->CollisionRadius;
									SpecHeight = 0.75f * CylinderComponent->CollisionHeight;
								}
								else if ( Location.Z > Nav->Location.Z )
								{
									// otherwise, try extent trace to cylinder
									FVector Intersect = Nav->Location; 
									Intersect.Z = Location.Z - CylinderComponent->CollisionHeight + 2.f;

									FCheckResult CylinderHit(1.f);
									for ( INT SizeIndex=Scout->PathSizes.Num()-1; SizeIndex >= 0; SizeIndex-- )
									{
                                        GWorld->SingleLineCheck( CylinderHit, this, Intersect, Nav->Location, TRACE_World|TRACE_StopAtAnyHit, FVector(Scout->PathSizes(SizeIndex).Radius,Scout->PathSizes(SizeIndex).Radius,1.f) );
										if ( !CylinderHit.Actor || (CylinderHit.Actor == Nav) )
										{
											SpecHeight = Scout->PathSizes(SizeIndex).Height;
											SpecRadius = Scout->PathSizes(SizeIndex).Radius;
											break;
										}
									}
								}
							}
						}
					}
					if ( SpecRadius > 0.f )
					{
						// we found a good connection
						FLOAT RealDist = (Location - Nav->Location).Size();
						newSpec->CollisionRadius = appTrunc(SpecRadius);
						newSpec->CollisionHeight = appTrunc(SpecHeight);
						INT NewFlags = R_FLY;
						if ( Nav->PhysicsVolume->bWaterVolume && PhysicsVolume->bWaterVolume )
							NewFlags = R_SWIM;
						else if ( Nav->PhysicsVolume->bWaterVolume || PhysicsVolume->bWaterVolume )
							NewFlags = R_SWIM + R_FLY;
						else
						{
							if ( Nav->IsA(AVolumePathNode::StaticClass()) && (RealDist > SHORTTRACETESTDIST) )
							{
								RealDist = ::Max(SHORTTRACETESTDIST, Dist2D - ::Max(CylinderComponent->CollisionRadius, Nav->CylinderComponent->CollisionRadius));
							}
							else
							{
								RealDist = ::Max(1.f, Dist2D);
							}
						}
						newSpec->reachFlags = NewFlags;
						newSpec->Start = this;
						newSpec->End = Nav;
						newSpec->Distance = appTrunc(RealDist);
						PathList.AddItem(newSpec);
						newSpec = ConstructObject<UReachSpec>(Scout->GetDefaultReachSpecClass(),GetOuter(),NAME_None);

						if ( !FlyNav && !Nav->bDestinationOnly )
						{
							newSpec->CollisionRadius = appTrunc(SpecRadius);
							newSpec->CollisionHeight = appTrunc(SpecHeight);
							newSpec->reachFlags = NewFlags;
							newSpec->Start = Nav;
							newSpec->End = this;
							newSpec->Distance = appTrunc(RealDist);
							Nav->PathList.AddItem(newSpec);
							newSpec = ConstructObject<UReachSpec>(Scout->GetDefaultReachSpecClass(),GetOuter(),NAME_None);
						}
						debugfSuppressed(NAME_DevPath, TEXT("***********added new spec from %s to %s size %f %f"),*GetName(),*Nav->GetName(), SpecRadius, SpecHeight);
					}
				}
			}
		}
	}
}
#endif

UBOOL AVolumePathNode::ShouldBeBased()
{
	return false;
}

UBOOL AVolumePathNode::ReachedBy(APawn *P, const FVector &TestPosition, const FVector& Dest)
{ 
	if ( !P->bCanFly && !PhysicsVolume->bWaterVolume )
		return false;
	FVector Dir = TestPosition - Dest;
	if ( Abs(Dir.Z) > CylinderComponent->CollisionHeight )
		return false;
	Dir.Z = 0.f;
	return ( Dir.SizeSquared() < CylinderComponent->CollisionRadius * CylinderComponent->CollisionRadius );
}

#if WITH_EDITOR
void AVolumePathNode::ReviewPath(APawn* Scout)
{
}

UBOOL AVolumePathNode::CanPrunePath(INT index) 
{ 
	return !PathList(index)->End->IsA(AVolumePathNode::StaticClass());
}
#endif

AActor* ADoorMarker::AssociatedLevelGeometry()
{
	return MyDoor;
}

UBOOL ADoorMarker::HasAssociatedLevelGeometry(AActor *Other)
{
	return (Other != NULL && Other == MyDoor);
}

void ADoorMarker::PrePath()
{
	// turn off associated mover collision temporarily
	if (MyDoor != NULL)
	{
		MyDoor->MyMarker = this;
		if (MyDoor->bBlockActors && MyDoor->bCollideActors)
		{
			MyDoor->SetCollision( FALSE, MyDoor->bBlockActors, MyDoor->bIgnoreEncroachers );
			bTempDisabledCollision = 1;
		}
	}
}

void ADoorMarker::PostPath()
{
	if (bTempDisabledCollision && MyDoor != NULL)
	{
		MyDoor->SetCollision( TRUE, MyDoor->bBlockActors, MyDoor->bIgnoreEncroachers );
	}
}

void ADoorMarker::FindBase()
{
	if (!GWorld->HasBegunPlay())
	{
		PrePath();
		Super::FindBase();
		PostPath();
	}
}

#if WITH_EDITOR
void ADoorMarker::CheckForErrors()
{
	Super::CheckForErrors();

	if (MyDoor == NULL)
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_DoorMarkerNoDoor" ), *GetName() ) ), TEXT( "DoorMarkerNoDoor" ) );
	}
}

INT APortalTeleporter::AddMyMarker(AActor* S)
{
	AScout* Scout = Cast<AScout>(S);
	if (Scout == NULL)
	{
		return 0;
	}
	else
	{
		if (MyMarker == NULL || MyMarker->bDeleteMe)
		{
			FVector MaxCommonSize = Scout->GetSize(FName(TEXT("Max"),FNAME_Find));
			FLOAT BottomZ = GetComponentsBoundingBox(FALSE).Min.Z;
			// trace downward to find the floor to place the marker on
			FCheckResult Hit(1.0f);
			if (GWorld->SingleLineCheck(Hit, this, Location + FVector(0.f, 0.f, BottomZ), Location, TRACE_AllBlocking, MaxCommonSize))
			{
				Hit.Location = Location + FVector(0.f, 0.f, BottomZ);
			}
			MyMarker = CastChecked<APortalMarker>(GWorld->SpawnActor(APortalMarker::StaticClass(), NAME_None, Hit.Location));
			if (MyMarker != NULL)
			{
				MyMarker->MyPortal = this;
			}
			else
			{
				GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_PortalMarkerFailed" ), *GetName() ) ), TEXT( "PortalMarkerFailed" ) );
			}
		}
		return (MyMarker != NULL) ? 1 : 0;
	}
}

void APortalMarker::addReachSpecs(AScout* Scout, UBOOL bOnlyChanged)
{
	// if our portal has collision and a valid destination portal, add a reachspec to the destination portal
	if ( MyPortal != NULL && (MyPortal->bCollideActors || MyPortal->bPathTemp) && MyPortal->SisterPortal != NULL && MyPortal->SisterPortal->MyMarker != NULL &&
		(!bOnlyChanged || bPathsChanged || MyPortal->SisterPortal->MyMarker->bPathsChanged) )
	{
		UReachSpec* NewSpec = ConstructObject<UReachSpec>(UTeleportReachSpec::StaticClass(), GetOuter(), NAME_None);
		FVector MaxSize = Scout->GetMaxSize();
		NewSpec->CollisionRadius = appTrunc(MaxSize.X);
		NewSpec->CollisionHeight = appTrunc(MaxSize.Y);
		NewSpec->Start = this;
		NewSpec->End = MyPortal->SisterPortal->MyMarker;
		NewSpec->Distance = 100;
		PathList.AddItem(NewSpec);
	}

	ANavigationPoint::addReachSpecs(Scout, bOnlyChanged);
}
#endif

UBOOL APortalMarker::ReachedBy(APawn *P, const FVector &TestPosition, const FVector& Dest)
{
	return (P != NULL && (MyPortal == NULL || !MyPortal->bCollideActors || MyPortal->TouchReachSucceeded(P, TestPosition)) && Super::ReachedBy(P, TestPosition, Dest));
}

UBOOL APortalMarker::CanTeleport(AActor* A)
{
	return (MyPortal != NULL && MyPortal->CanTeleport(A));
}

/** PlaceScout()
Place a scout at the location of this NavigationPoint, or as close as possible
*/
UBOOL ANavigationPoint::PlaceScout(AScout * Scout)
{
	// Try placing above and moving down
	FCheckResult Hit(1.f);
	UBOOL bSuccess = FALSE;

	if( Base )
	{
		FVector Up( 0.f, 0.f, 1.f );
		GetUpDir( Up );
		Up *= Scout->CylinderComponent->CollisionHeight - CylinderComponent->CollisionHeight + ::Max(0.f, Scout->CylinderComponent->CollisionRadius - CylinderComponent->CollisionRadius);

		if( GWorld->FarMoveActor( Scout, Location + Up ) )
		{
			bSuccess = TRUE;
			GWorld->MoveActor(Scout, -1.f * Up, Scout->Rotation, 0, Hit);
		}
	}

	if( !bSuccess && !GWorld->FarMoveActor(Scout, Location) )
	{
		return FALSE;
	}

	// If scout is walking, make sure it is on the ground
	if( (Scout->Physics == PHYS_Walking || Scout->Physics == PHYS_Spider) && 
		!Scout->bCrawler &&
		!Scout->PhysicsVolume->bWaterVolume )
	{
		FVector Up(0,0,1);
		GetUpDir( Up );
		GWorld->MoveActor(Scout, -Up * CylinderComponent->CollisionHeight, Scout->Rotation, 0, Hit);
	}
	return TRUE;
}

UBOOL ANavigationPoint::CanTeleport(AActor* A)
{
	return FALSE;
}

void ARoute::GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel)
{
	Super::GetActorReferences(ActorRefs,bIsRemovingLevel);
	for (INT Idx = 0; Idx < RouteList.Num(); Idx++)
	{
		FActorReference &ActorRef = RouteList(Idx);
		if (ActorRef.Guid.IsValid())
		{
			if ((bIsRemovingLevel && ActorRef.Actor != NULL) ||
				(!bIsRemovingLevel && ActorRef.Actor == NULL))
			{
				ActorRefs.AddItem(&ActorRef);
			}
		}
	}
}
////// EditorLinkSelectionInterface
void ARoute::LinkSelection(USelection* SelectedActors)
{
	TArray<AActor*> Points;
	SelectedActors->GetSelectedObjects<AActor>(Points);
	Points.RemoveItem(this);
	AutoFillRoute(RFA_Overwrite,Points);
}

void ARoute::AutoFillRoute( ERouteFillAction RFA, TArray<AActor*>& Points )
{
	// If overwriting or clearing
	if( RFA == RFA_Overwrite || 
		RFA == RFA_Clear )
	{
		// Empty list
		RouteList.Empty();
	}

	// If overwriting or adding selected items
	if( RFA == RFA_Overwrite || RFA == RFA_Add )
	{
		// Go through each selected point
		for( INT Idx = 0; Idx < Points.Num(); Idx++ )
		{
			AActor* Actor = Points(Idx);
			if( Actor )
			{
				// Add to the list
				FActorReference Item(EC_EventParm);
				Item.Actor = Actor;

				// If point is in another map from the route
				if( Item.Actor &&
					GetOutermost() != Item.Actor->GetOutermost() )
				{
					// Use GUID
					Item.Guid = *Item.Actor->GetGuid();
				}

				RouteList.AddItem( Item );
			}
		}
	}
	else
	// Otherwise, if removing selected items
	if( RFA == RFA_Remove )
	{
		// Go through each selected point
		for( INT Idx = 0; Idx < Points.Num(); Idx++ )
		{
			// Remove from the list
			for( INT ItemIdx = 0; ItemIdx < RouteList.Num(); ItemIdx++ )
			{
				if( RouteList(ItemIdx).Actor == Points(Idx) )
				{
					RouteList.Remove( ItemIdx-- );
				}
			}
		}
	}

	ForceUpdateComponents(FALSE,FALSE);
}

#if WITH_EDITOR
void ARoute::CheckForErrors()
{
	Super::CheckForErrors();

	for( INT Idx = 0; Idx < RouteList.Num(); Idx++ )
	{
		AActor* Actor = ~RouteList(Idx);
		if( !Actor )
		{
			GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_InvalidActorAtRouteListIndex" ), *GetName(), Idx ) ), TEXT( "InvalidActorAtRouteListIndex" ) );
		}
	}
}

UBOOL ARoute::HasRefToActor( AActor* A, INT* out_Idx /*=NULL*/ )
{
	for( INT Idx = 0; Idx < RouteList.Num(); Idx++ )
	{
		AActor* Actor = ~RouteList(Idx);
		if( Actor == A )
	{
			if( out_Idx != NULL )
		{
				*out_Idx = Idx;
		}
			return TRUE;
		}
	}
	return FALSE;
}
#endif
