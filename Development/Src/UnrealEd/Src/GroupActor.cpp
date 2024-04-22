// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "ScopedTransaction.h"
#if ENABLE_SIMPLYGON_MESH_PROXIES
#include "DlgCreateMeshProxy.h"
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES

//#define		KEEP_PROXIES_WHEN_UNDEFINED		// if ENABLE_SIMPLYGON_MESH_PROXIES is undefined, this define will keep the proxy intact, otherwise it will revert it

const FLinearColor BOXCOLOR_LOCKEDGROUPS( 0.0f, 1.0f, 0.0f );
const FLinearColor BOXCOLOR_UNLOCKEDGROUPS( 1.0f, 0.0f, 0.0f );
static UBOOL bProxyUnsupportedWarned = TRUE;

void AGroupActor::Spawned()
{
	// Cache our newly created group
	if( !GIsPlayInEditorWorld && !GIsUCC && GIsEditor )
	{
		GEditor->ActiveGroupActors.AddUniqueItem(this);
	}
	Super::Spawned();
}

void AGroupActor::PostLoad()
{
	if( !GIsPlayInEditorWorld && !GIsUCC && GIsEditor )
	{
		// Cache group on de-serialization
		GEditor->ActiveGroupActors.AddUniqueItem(this);
	}

	// If proxies aren't definied, make sure they're reset correctly (we can't do it PostLoad as GWorld is NULL, and we can't do it in StaticMeshActor as it doesn't Tick)
#if !ENABLE_SIMPLYGON_MESH_PROXIES
	bProxyUnsupportedWarned = FALSE;
	bResetProxy = TRUE;
#endif // #if !ENABLE_SIMPLYGON_MESH_PROXIES

	Super::PostLoad();
}

void AGroupActor::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	// Re-instate group as active if it had children after undo/redo
	if(GroupActors.Num() || SubGroups.Num())
	{
		GEditor->ActiveGroupActors.AddUniqueItem(this);
	}
	else // Otherwise, attempt to remove them
	{
		GEditor->ActiveGroupActors.RemoveItem(this);
	}

	// Check grouping is correct
	if ( ValidateGroup() )
	{
		VerifyProxy( FALSE, FALSE );
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void AGroupActor::PostScriptDestroyed()
{
	if( !GIsPlayInEditorWorld && !GIsUCC && GIsEditor )
	{
		// Relieve this group from active duty if not done so already
		GEditor->ActiveGroupActors.RemoveItem(this);
	}
	Super::PostScriptDestroyed();
}

UBOOL AGroupActor::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	UBOOL bRet = Super::Tick( DeltaTime, TickType );

	// Reset the proxy (based on the define; keeps the proxy but prevents reverting OR reverts the proxy)
	if ( bResetProxy )
	{
		bResetProxy = FALSE;

		UBOOL bProxyUnsupported = FALSE;
		for(INT ActorIndex=0; ActorIndex < GroupActors.Num(); ++ActorIndex)
		{
			AStaticMeshActor* SMActor = Cast< AStaticMeshActor >( GroupActors(ActorIndex) );
			if ( SMActor )
			{
#ifdef KEEP_PROXIES_WHEN_UNDEFINED
				if ( SMActor->IsHiddenByProxy() )	// If this was hidden by proxy, clear the flag, remove from group and delete the mesh
				{
					warnf( NAME_Warning, TEXT("ENABLE_SIMPLYGON_MESH_PROXIES is undefined but utilized by actor %s as hidden by proxy, saving this map will prevent proxies being reverted"), *GetName() );
#else
				if ( SMActor->IsProxy() )	// If this was a proxy, clear the flag, remove from group and delete the mesh
				{
					warnf( NAME_Warning, TEXT("ENABLE_SIMPLYGON_MESH_PROXIES is undefined but utilized by actor %s as proxy, saving this map will remove the proxy from it"), *GetName() );
#endif // #if !KEEP_PROXIES_WHEN_UNDEFINED
					bProxyUnsupported = TRUE;
					SMActor->SetProxy( FALSE );
					SMActor->SetHiddenByProxy( FALSE );
					Remove( *SMActor, FALSE );
					GWorld->EditorDestroyActor( SMActor, TRUE );
					--ActorIndex;
				}
#ifdef KEEP_PROXIES_WHEN_UNDEFINED
				else if ( SMActor->IsProxy() )	// If this was a proxy, just clear the flag
#else
				else if ( SMActor->IsHiddenByProxy() )	// If this was hidden by proxy, just clear the flag
#endif // #if !KEEP_PROXIES_WHEN_UNDEFINED
				{
					bProxyUnsupported = TRUE;
					SMActor->SetProxy( FALSE );
					SMActor->ShowProxy( TRUE, TRUE );	// Calls SetHiddenByProxy
				}
			}
		}

		if ( !bProxyUnsupportedWarned && bProxyUnsupported )	// Display a dialog to the user, inform them of the change
		{
			bProxyUnsupportedWarned = TRUE;
#ifdef KEEP_PROXIES_WHEN_UNDEFINED
			appMsgf( AMT_OK, *LocalizeUnrealEd( "Group_ProxiesUnsupported1") );
#else
			appMsgf( AMT_OK, *LocalizeUnrealEd( "Group_ProxiesUnsupported2") );
#endif // #if !KEEP_PROXIES_WHEN_UNDEFINED
		}
	}

	return bRet;
}

UBOOL AGroupActor::IsSelected() const
{
	// Group actors can only count as 'selected' if they are locked 
	return IsLocked() && HasSelectedActors() || Super::IsSelected();
}

/**
	Checks the group to make sure it doesn't conflict with other groups or is attempting anything invalid
	*/
UBOOL AGroupActor::ValidateGroup()
{
	// Make sure that any group actors this group is referencing aren't referenced by other groups 
	// (can occur when duplicating groups as hidden actors aren't part of the selection when duplicated, but get referenced instead)
	UBOOL bModified = FALSE;
	for(INT GroupIndex=0; GroupIndex < GEditor->ActiveGroupActors.Num(); ++GroupIndex)
	{
		const AGroupActor *ActiveGroupActor = GEditor->ActiveGroupActors(GroupIndex);
		if ( ActiveGroupActor && ActiveGroupActor != this )
		{
			for(INT ActorIndex=0; ActorIndex < ActiveGroupActor->GroupActors.Num(); ++ActorIndex)
			{
				AActor* GroupActor = ActiveGroupActor->GroupActors(ActorIndex);
				if ( GroupActor && GroupActors.ContainsItem(GroupActor) )
				{
					GroupActors.RemoveItem(GroupActor);
					bModified = TRUE;
				}
			}
		}
	}
	return bModified;
}
void AGroupActor::VerifyProxy( const UBOOL bSkipProxy, const UBOOL bSkipHidden )
{
#if ENABLE_SIMPLYGON_MESH_PROXIES
	// Make sure there's only 1 proxy in the group, and it can be reverted if necessary
	AStaticMeshActor* Proxy = ContainsProxy();
	if ( Proxy )
	{
		UBOOL bRevertable = bSkipHidden;
		for(INT ActorIndex=0; ActorIndex < GroupActors.Num(); ++ActorIndex)
		{
			AStaticMeshActor* SMActor = Cast< AStaticMeshActor >( GroupActors(ActorIndex) );
			if ( SMActor && SMActor != Proxy )
			{
				if ( !bSkipProxy && SMActor->IsProxy() )
				{
					SMActor->SetProxy( FALSE );
				}
				else if ( !bSkipHidden && SMActor->IsHiddenByProxy() )
				{
					bRevertable = TRUE;
				}
			}
		}		
		if ( !bRevertable )
		{
			Proxy->SetProxy( FALSE );
		}
	}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
}

/**
 * Apply given deltas to all actors and subgroups for this group.
 * @param	Viewport		The viewport to draw to apply our deltas
 * @param	InDrag			Delta Transition
 * @param	InRot			Delta Rotation
 * @param	InScale			Delta Scale
 */
void AGroupActor::GroupApplyDelta(FEditorLevelViewportClient* Viewport, const FVector& InDrag, const FRotator& InRot, const FVector& InScale )
{
	check(Viewport);
	for(INT ActorIndex=0; ActorIndex<GroupActors.Num(); ++ActorIndex)
	{
		AActor* GroupActor = GroupActors(ActorIndex);
		if ( GroupActor )
		{
			Viewport->ApplyDeltaToActor( GroupActor, InDrag, InRot, InScale );
		}
	}
	for(INT SubGroupIndex=0; SubGroupIndex<SubGroups.Num(); ++SubGroupIndex)
	{
		AGroupActor* SubGroup = SubGroups(SubGroupIndex);
		if ( SubGroup )
		{
			SubGroup->GroupApplyDelta(Viewport, InDrag, InRot, InScale);
		}
	}
	Viewport->ApplyDeltaToActor(this, InDrag, InRot, InScale);
}

void GetBoundingVectorsForGroup(AGroupActor* GroupActor, FViewport* Viewport, FVector& OutVectorMin, FVector& OutVectorMax)
{
	// Draw a bounding box for grouped actors using the vector range we can gather from any child actors (including subgroups)
	OutVectorMin = FVector(BIG_NUMBER);
	OutVectorMax = FVector(-BIG_NUMBER);

	// Grab all actors for this group, including those within subgroups
	TArray<AActor*> ActorsInGroup;
	GroupActor->GetGroupActors(ActorsInGroup, TRUE);

	// Loop through and collect each actor, using their bounding box to create the bounds for this group
	for(INT ActorIndex = 0; ActorIndex < ActorsInGroup.Num(); ++ActorIndex)
	{
		AActor* Actor = ActorsInGroup(ActorIndex);
		QWORD HiddenClients = Actor->HiddenEditorViews;
		UBOOL bActorHiddenForViewport = FALSE;
		if(!Actor->IsHiddenEd())
		{
			if(Viewport)
			{
				for(INT ViewIndex=0; ViewIndex<GEditor->ViewportClients.Num(); ++ViewIndex)
				{
					// If the current viewport is hiding this actor, don't draw brackets around it
					if(Viewport->GetClient() == GEditor->ViewportClients(ViewIndex) && HiddenClients & ((QWORD)1 << ViewIndex))
					{
						bActorHiddenForViewport = TRUE;
						break;
					}
				}
			}

			if(!bActorHiddenForViewport)
			{
				FBox ActorBox;

				// First check to see if we're dealing with a sprite, otherwise just use the normal bounding box
				USpriteComponent* SpriteComponent = Actor->GetActorSpriteComponent();
				if(SpriteComponent != NULL)
				{
					ActorBox = SpriteComponent->Bounds.GetBox();
				}
				else
				{
					ActorBox = Actor->GetComponentsBoundingBox( TRUE );
				}

				// MinVector
				OutVectorMin.X = Min<FLOAT>( ActorBox.Min.X, OutVectorMin.X );
				OutVectorMin.Y = Min<FLOAT>( ActorBox.Min.Y, OutVectorMin.Y );
				OutVectorMin.Z = Min<FLOAT>( ActorBox.Min.Z, OutVectorMin.Z );
				// MaxVector
				OutVectorMax.X = Max<FLOAT>( ActorBox.Max.X, OutVectorMax.X );
				OutVectorMax.Y = Max<FLOAT>( ActorBox.Max.Y, OutVectorMax.Y );
				OutVectorMax.Z = Max<FLOAT>( ActorBox.Max.Z, OutVectorMax.Z );
			}
		}
	}	
}

/**
 * Draw brackets around all given groups
 * @param	PDI			FPrimitiveDrawInterface used to draw lines in active viewports
 * @param	Viewport	Current viewport being rendered
 * @param	InGroupList	Array of groups to draw brackets for
 */
void PrivateDrawBracketsForGroups( FPrimitiveDrawInterface* PDI, FViewport* Viewport, const TArray<AGroupActor*>& InGroupList )
{
	// Loop through each given group and draw all subgroups and actors
	for(INT GroupIndex=0; GroupIndex<InGroupList.Num(); ++GroupIndex)
	{
		AGroupActor* GroupActor = InGroupList(GroupIndex);
		const FLinearColor GROUP_COLOR = GroupActor->IsLocked() ? BOXCOLOR_LOCKEDGROUPS : BOXCOLOR_UNLOCKEDGROUPS;
		
		FVector MinVector;
		FVector MaxVector;
		GetBoundingVectorsForGroup( GroupActor, Viewport, MinVector, MaxVector );

		// Create a bracket offset to pad the space between brackets and actor(s) and determine the length of our corner axises
		FLOAT BracketOffset = FDist(MinVector, MaxVector) * 0.1f;
		MinVector = MinVector - BracketOffset;
		MaxVector = MaxVector + BracketOffset;

		// Calculate bracket corners based on min/max vectors
		TArray<FVector> BracketCorners;

		// Bottom Corners
		BracketCorners.AddItem(FVector(MinVector.X, MinVector.Y, MinVector.Z));
		BracketCorners.AddItem(FVector(MinVector.X, MaxVector.Y, MinVector.Z));
		BracketCorners.AddItem(FVector(MaxVector.X, MaxVector.Y, MinVector.Z));
		BracketCorners.AddItem(FVector(MaxVector.X, MinVector.Y, MinVector.Z));

		// Top Corners
		BracketCorners.AddItem(FVector(MinVector.X, MinVector.Y, MaxVector.Z));
		BracketCorners.AddItem(FVector(MinVector.X, MaxVector.Y, MaxVector.Z));
		BracketCorners.AddItem(FVector(MaxVector.X, MaxVector.Y, MaxVector.Z));
		BracketCorners.AddItem(FVector(MaxVector.X, MinVector.Y, MaxVector.Z));
		
		for(INT BracketCornerIndex=0; BracketCornerIndex<BracketCorners.Num(); ++BracketCornerIndex)
		{
			// Direction corner axis should be pointing based on min/max
			const FVector CORNER = BracketCorners(BracketCornerIndex);
			const INT DIR_X = CORNER.X == MaxVector.X ? -1 : 1;
			const INT DIR_Y = CORNER.Y == MaxVector.Y ? -1 : 1;
			const INT DIR_Z = CORNER.Z == MaxVector.Z ? -1 : 1;

			PDI->DrawLine( CORNER, FVector(CORNER.X + (BracketOffset * DIR_X), CORNER.Y, CORNER.Z), GROUP_COLOR, SDPG_Foreground );
			PDI->DrawLine( CORNER, FVector(CORNER.X, CORNER.Y + (BracketOffset * DIR_Y), CORNER.Z), GROUP_COLOR, SDPG_Foreground );
			PDI->DrawLine( CORNER, FVector(CORNER.X, CORNER.Y, CORNER.Z + (BracketOffset * DIR_Z)), GROUP_COLOR, SDPG_Foreground );
		}

		// Recurse through to any subgroups
		TArray<AGroupActor*> SubGroupsInGroup;
		GroupActor->GetSubGroups(SubGroupsInGroup);
		PrivateDrawBracketsForGroups(PDI, Viewport, SubGroupsInGroup);
	}
}

/**
 * Draw brackets around all selected groups
 * @param	PDI				FPrimitiveDrawInterface used to draw lines in active viewports
 * @param	Viewport		Current viewport being rendered
 * @param	bMustBeSelected	Flag to only draw currently selected groups. Defaults to TRUE.
 */
void AGroupActor::DrawBracketsForGroups( FPrimitiveDrawInterface* PDI, FViewport* Viewport, UBOOL bMustBeSelected/*=TRUE*/ )
{
	if( GUnrealEd->bGroupingActive )
	{
		check(PDI);
	
		if(bMustBeSelected)
		{
			// If we're only drawing for selected group, grab only those that have currently selected actors
			TArray<AGroupActor*> SelectedGroups;
			for(INT GroupIndex=0; GroupIndex < GEditor->ActiveGroupActors.Num(); ++GroupIndex)
			{
				AGroupActor *SelectedGroupActor = GEditor->ActiveGroupActors(GroupIndex);		
				if(SelectedGroupActor->HasSelectedActors())
				{
					// We want to start drawing groups from the highest root level.
					// Subgroups will be propagated through during the draw code.
					SelectedGroupActor = GetRootForActor(SelectedGroupActor);
					SelectedGroups.AddItem(SelectedGroupActor);
				}
			}
			PrivateDrawBracketsForGroups(PDI, Viewport, SelectedGroups);
		}
		else
		{
			PrivateDrawBracketsForGroups(PDI, Viewport, GEditor->ActiveGroupActors );
		}
	}
}

/**
 * Checks to see if the given GroupActor has any parents in the given Array.
 * @param	InGroupActor	Group to check lineage
 * @param	InGroupArray	Array to search for the given group's parent
 * @return	True if a parent was found.
 */
UBOOL GroupHasParentInArray(AGroupActor* InGroupActor, TArray<AGroupActor*>& InGroupArray)
{
	check(InGroupActor);
	AGroupActor* CurrentParentNode = AGroupActor::GetParentForActor(InGroupActor);

	// Use a cursor pointer to continually move up from our starting pointer (InGroupActor) through the hierarchy until
	// we find a valid parent in the given array, or run out of nodes.
	while( CurrentParentNode )
	{
		if(InGroupArray.ContainsItem(CurrentParentNode))
		{
			return TRUE;
		}
		CurrentParentNode = AGroupActor::GetParentForActor(CurrentParentNode);
	}
	return FALSE;
}

/**
 * Changes the given array to remove any existing subgroups
 * @param	GroupArray	Array to remove subgroups from
 */
void AGroupActor::RemoveSubGroupsFromArray(TArray<AGroupActor*>& GroupArray)
{
	for(INT GroupIndex=0; GroupIndex<GroupArray.Num(); ++GroupIndex)
	{
		AGroupActor* GroupToCheck = GroupArray(GroupIndex);
		if(GroupHasParentInArray(GroupToCheck, GroupArray))
		{
			GroupArray.RemoveItem(GroupToCheck);
			--GroupIndex;
		}
	}
}

/**
 * Returns the highest found root for the given actor or null if one is not found. Qualifications of root can be specified via optional parameters.
 * @param	InActor			Actor to find a group root for.
 * @param	bMustBeLocked	Flag designating to only return the topmost locked group.
 * @param	bMustBeSelected	Flag designating to only return the topmost selected group.
 * @return	The topmost group actor for this actor. Returns null if none exists using the given conditions.
 */
AGroupActor* AGroupActor::GetRootForActor(AActor* InActor, UBOOL bMustBeLocked/*=FALSE*/, UBOOL bMustBeSelected/*=FALSE*/)
{
	AGroupActor* RootNode = NULL;
	// If InActor is a group, use that as the beginning iteration node, else try to find the parent
	AGroupActor* InGroupActor = Cast<AGroupActor>(InActor);
	AGroupActor* IteratingNode = InGroupActor == NULL ? GetParentForActor(InActor) : InGroupActor;
	while( IteratingNode )
	{
		if ( (!bMustBeLocked || IteratingNode->IsLocked()) && (!bMustBeSelected || IteratingNode->HasSelectedActors()) )
		{
			RootNode = IteratingNode;
		}
		IteratingNode = GetParentForActor(IteratingNode);
	}
	return RootNode;
}

/**
 * Returns the direct parent for the actor or null if one is not found.
 * @param	InActor	Actor to find a group parent for.
 * @return	The direct parent for the given actor. Returns null if no group has this actor as a child.
 */
AGroupActor* AGroupActor::GetParentForActor(AActor* InActor)
{
	for(INT GroupActorIndex = 0; GroupActorIndex < GEditor->ActiveGroupActors.Num(); ++GroupActorIndex)
	{
		AGroupActor* GroupActor = GEditor->ActiveGroupActors(GroupActorIndex);
		if(GroupActor->Contains(*InActor))
		{
			return GroupActor;
		}
	}
	return NULL;
}

/**
 * Query to find how many active groups are currently in the editor.
 * @param	bSelected	Flag to only return currently selected groups (defaults to FALSE).
 * @param	bDeepSearch	Flag to do a deep search when checking group selections (defaults to TRUE).
 * @return	Number of active groups currently in the editor.
 */
const INT AGroupActor::NumActiveGroups( UBOOL bSelected/*=FALSE*/, UBOOL bDeepSearch/*=TRUE*/ )
{
	if(!bSelected)
	{
		return GEditor->ActiveGroupActors.Num();
	}

	INT ActiveSelectedGroups = 0;
	for(INT GroupIdx=0; GroupIdx < GEditor->ActiveGroupActors.Num(); ++GroupIdx )
	{
		if(GEditor->ActiveGroupActors(GroupIdx)->HasSelectedActors(bDeepSearch))
		{
			++ActiveSelectedGroups;
		}
	}
	return ActiveSelectedGroups;
}

/**
 * Get the selected group matching the supplied index
 * @param iGroupIdx		Index of selected groups to gather the actors from.
 * @param	bDeepSearch	Flag to do a deep search when checking group selections (defaults to TRUE).
 * @return	Selected group actor, NULL if failed to find it
 */
AGroupActor* AGroupActor::GetSelectedGroup( INT iGroupIdx, UBOOL bDeepSearch/*=TRUE*/ )
{
	INT ActiveSelectedGroups = 0;
	for(INT GroupIdx=0; GroupIdx < GEditor->ActiveGroupActors.Num(); ++GroupIdx )
	{
		if(GEditor->ActiveGroupActors(GroupIdx)->HasSelectedActors(bDeepSearch))
		{
			if(ActiveSelectedGroups == iGroupIdx)
			{
				return GEditor->ActiveGroupActors(GroupIdx);
			}
			++ActiveSelectedGroups;
		}
	}
	return NULL;
}

/**
* Adds selected ungrouped actors to a selected group. Does nothing if more than one group is selected.
*/
UBOOL AGroupActor::AddSelectedActorsToSelectedGroup()
{
	UBOOL bRet = FALSE;
	INT SelectedGroupIndex = -1;
	for(INT GroupIdx=0; GroupIdx < GEditor->ActiveGroupActors.Num(); ++GroupIdx )
	{
		if(GEditor->ActiveGroupActors(GroupIdx)->HasSelectedActors(FALSE))
		{
			// Assign the index of the selected group.
			// If this is the second group we find, too many groups are selected, return.
			if( SelectedGroupIndex == -1 )
			{
				SelectedGroupIndex = GroupIdx;
			}
			else { return FALSE; }
		}
	}

	if( SelectedGroupIndex != -1 )
	{
		AGroupActor* SelectedGroup = GEditor->ActiveGroupActors(SelectedGroupIndex);
		
		ULevel* GroupLevel = SelectedGroup->GetLevel();

		// We've established that only one group is selected, so we can just call Add on all these actors.
		// Any actors already in the group will be ignored.
		
		TArray<AActor*> ActorsToAdd;

		UBOOL bActorsInSameLevel = TRUE;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = CastChecked<AActor>( *It );
		
			if( Actor->GetLevel() == GroupLevel )
			{
				ActorsToAdd.AddItem( Actor );
			}
			else
			{
				bActorsInSameLevel = FALSE;
				break;
			}
		}

		if( bActorsInSameLevel )
		{
			if( ActorsToAdd.Num() > 0 )
			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd("Group_Add") );
				for( INT ActorIndex = 0; ActorIndex < ActorsToAdd.Num(); ++ActorIndex )
				{
					SelectedGroup->Add( *ActorsToAdd(ActorIndex) );
				}
				SelectedGroup->CenterGroupLocation();
				bRet = TRUE;
			}
		}
		else
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd( "Group_CantCreateGroupMultipleLevels") );
		}
	}
	return bRet;
}

/**
	* Removes selected ungrouped actors from a selected group. Does nothing if more than one group is selected.
	*/
UBOOL AGroupActor::RemoveSelectedActorsFromSelectedGroup()
{
	UBOOL bRet = FALSE;
	TArray<AActor*> ActorsToRemove;
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// See if an entire group is being removed
		AGroupActor* GroupActor = Cast<AGroupActor>(Actor);
		if(GroupActor == NULL)
		{
			// See if the actor selected belongs to a locked group, if so remove the group in lieu of the actor
			GroupActor = GetParentForActor(Actor);
			if(GroupActor && !GroupActor->IsLocked())
			{
				GroupActor = NULL;
			}
		}

		if(GroupActor)
		{
			// If the GroupActor has no parent, do nothing, otherwise just add the group for removal
			if(GetParentForActor(GroupActor))
			{
				ActorsToRemove.AddUniqueItem(GroupActor);
			}
		}
		else
		{
			ActorsToRemove.AddUniqueItem(Actor);
		}
	}

	if ( ActorsToRemove.Num() )
	{
		// Clear the selection
		GEditor->SelectNone(TRUE, TRUE);

		UBOOL bAskAboutRemoving = TRUE;
		UBOOL bMaintainProxy = FALSE;
		const FScopedTransaction Transaction( *LocalizeUnrealEd("Group_Remove") );
		for ( INT ActorIndex = 0; ActorIndex < ActorsToRemove.Num(); ++ActorIndex )
		{
			AActor* Actor = ActorsToRemove(ActorIndex);
			AGroupActor* ActorGroup = GetParentForActor(Actor);

			if(ActorGroup)
			{
				AGroupActor* ActorGroupParent = GetParentForActor(ActorGroup);
				if(ActorGroupParent)
				{
					ActorGroupParent->Add(*Actor);
				}
				else
				{
#if ENABLE_SIMPLYGON_MESH_PROXIES
					// If any of the actors are a proxy, verify with the user that they definitely want to remove it
					AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(Actor);
					if ( SMActor && SMActor->IsProxy() )
					{
						if (bAskAboutRemoving)
						{
							UINT MsgResult = appMsgf(AMT_YesNoYesAllNoAllCancel, LocalizeSecure(LocalizeUnrealEd("Group_ContainsProxyRemove"), *SMActor->GetName()));
							if ( MsgResult == ART_Cancel )
							{
								return FALSE;
							}
							bMaintainProxy = MsgResult == ART_No || MsgResult == ART_NoAll;
							bAskAboutRemoving = MsgResult == ART_No || MsgResult == ART_Yes;
						}
					}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
					ActorGroup->Remove(*Actor, TRUE, bMaintainProxy);
				}
				bRet = TRUE;
			}
		}

		// Do a re-selection of each actor, to maintain group selection rules
		for ( INT ActorIndex = 0; ActorIndex < ActorsToRemove.Num(); ++ActorIndex )
		{
			GEditor->SelectActor( ActorsToRemove(ActorIndex), TRUE, NULL, FALSE);
		}
	}
	return bRet;
}

/**
 * Locks the lowest selected groups in the current selection.
 */
UBOOL AGroupActor::LockSelectedGroups()
{
	UBOOL bRet = FALSE;
	TArray<AGroupActor*> GroupsToLock;
	for ( INT GroupIndex=0; GroupIndex<GEditor->ActiveGroupActors.Num(); ++GroupIndex )
	{
		AGroupActor* GroupToLock = GEditor->ActiveGroupActors(GroupIndex);

		if( GroupToLock->HasSelectedActors(FALSE) )
		{
			// If our selected group is already locked, move up a level to add it's potential parent for locking
			if( GroupToLock->IsLocked() )
			{
				AGroupActor* GroupParent = GetParentForActor(GroupToLock);
				if(GroupParent && !GroupParent->IsLocked())
				{
					GroupsToLock.AddUniqueItem(GroupParent);
				}
			}
			else // if it's not locked, add it instead!
			{
				GroupsToLock.AddUniqueItem(GroupToLock);
			}
		}
	}

	if( GroupsToLock.Num() > 0 )
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd("Group_Lock") );
		for ( INT GroupIndex=0; GroupIndex<GroupsToLock.Num(); ++GroupIndex )
		{
			AGroupActor* GroupToLock = GroupsToLock(GroupIndex);
			GroupToLock->Modify();
			GroupToLock->Lock();
			GEditor->SelectGroup(GroupToLock, FALSE );
		}
		GEditor->NoteSelectionChange();
		bRet = TRUE;
	}
	return bRet;
}

/**
 * Unlocks the highest locked parent group for actors in the current selection.
 */
UBOOL AGroupActor::UnlockSelectedGroups()
{
	UBOOL bRet = FALSE;
	TArray<AGroupActor*> GroupsToUnlock;
	for ( INT GroupIndex=0; GroupIndex<GEditor->ActiveGroupActors.Num(); ++GroupIndex )
	{
		AGroupActor* GroupToUnlock = GEditor->ActiveGroupActors(GroupIndex);
		if( GroupToUnlock->IsSelected() )
		{
			GroupsToUnlock.AddItem(GroupToUnlock);
		}
	}

	// Only unlock topmost selected group(s)
	AGroupActor::RemoveSubGroupsFromArray(GroupsToUnlock);
	if( GroupsToUnlock.Num() > 0 )
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd("Group_Unlock") );
		for ( INT GroupIndex=0; GroupIndex<GroupsToUnlock.Num(); ++GroupIndex)
		{
			AGroupActor* GroupToUnlock = GroupsToUnlock(GroupIndex);
			GroupToUnlock->Modify();
			GroupToUnlock->Unlock();
		}
		GEditor->NoteSelectionChange();
		bRet = TRUE;
	}
	return bRet;
}

/**
 * Toggle group mode
 */
void AGroupActor::ToggleGroupMode()
{
	// Group mode can only be toggled when not in InterpEdit mode
	if( !GEditorModeTools().IsModeActive(EM_InterpEdit) )
	{
		GUnrealEd->bGroupingActive = !GUnrealEd->bGroupingActive;

		// Update group selection in the editor to reflect the toggle
		SelectGroupsInSelection();

#if ENABLE_SIMPLYGON_MESH_PROXIES
		WxDlgCreateMeshProxy*	DlgCreateMeshProxy = GApp->GetDlgCreateMeshProxy();
		check(DlgCreateMeshProxy);
		DlgCreateMeshProxy->EvaluateGroup( NULL );	// Attempt to reevaluate the selected group
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES

		GEditor->RedrawAllViewports();

		GEditor->SaveConfig();
	}
}

/**
 * Reselects any valid groups based on current editor selection
 */
void AGroupActor::SelectGroupsInSelection()
{
	if( GUnrealEd->bGroupingActive )
	{
		TArray<AGroupActor*> GroupsToSelect;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );
			AGroupActor* GroupActor = GetRootForActor(Actor, TRUE);

			if(GroupActor)
			{
				GroupsToSelect.AddUniqueItem(GroupActor);
			}
		}

		// Select any groups from the currently selected actors
		for ( INT GroupIndex=0; GroupIndex<GroupsToSelect.Num(); ++GroupIndex)
		{
			AGroupActor* GroupToSelect = GroupsToSelect(GroupIndex);
			GEditor->SelectGroup(GroupToSelect);
		}
		GEditor->NoteSelectionChange();
	}
}

/**
 * Loops through the active groups and checks to see if any need remerging
 */
void AGroupActor::RemergeActiveGroups()
{
#if ENABLE_SIMPLYGON_MESH_PROXIES
	/*	Warning, atm this will remerge the group with whatever options are set in the dialog, which is not necessarily the options
			that were set when the original proxy was setup, if we want this we'll need some way of preserving this information.				*/
	for(INT GroupIndex=0; GroupIndex < GEditor->ActiveGroupActors.Num(); ++GroupIndex)
	{
		AGroupActor *GroupActor = GEditor->ActiveGroupActors(GroupIndex);	
		if ( GroupActor && GroupActor->GetRemergeProxy() )
		{
			GroupActor->SetRemergeProxy( FALSE );
			WxDlgCreateMeshProxy*	DlgCreateMeshProxy = GApp->GetDlgCreateMeshProxy();
			check(DlgCreateMeshProxy);
			if ( DlgCreateMeshProxy->EvaluateGroup( GroupActor ) )	// Recalculate what needs remerging
			{
				DlgCreateMeshProxy->Remerge( TRUE, FALSE );	// Remerge the proxy
			}
		}
	}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
}

/**
 * Generates all the static meshes that are used by a single including proxy meshes (including proxy meshes and excluding meshes that are used by the proxy)
 */
static void CollectVisibleMeshesFromActor( AActor* Actor, TArray<UStaticMeshComponent*> *OutMeshComponents )
{
	AStaticMeshActor* SMActor = Cast<AStaticMeshActor>( Actor );
	if( SMActor && SMActor->StaticMeshComponent && SMActor->StaticMeshComponent->StaticMesh )
	{
		// Ignore any hidden actors (either hidden because they are replaced by a proxy or hidden by the editor)
		if ( !SMActor->IsHiddenByProxy() && !SMActor->IsHiddenEd())
		{
			OutMeshComponents->AddItem( SMActor->StaticMeshComponent );
		}
	}
}

/**
 * Generates all the static meshes that are used by a list of actors (including proxy meshes and excluding meshes that are used by the proxy)
 */
static void CollectVisibleMeshesFromActors( TArray<AActor*> &Actors, TArray<UStaticMeshComponent*> *OutMeshComponents )
{
	for ( INT ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex )
	{
		AActor* Actor = Actors(ActorIndex);
		CollectVisibleMeshesFromActor(Actor, OutMeshComponents);
	}
}

/**
 * Shared structure for statistics gathering
 */
struct FMeshStatResults
{
	INT NumMeshes;
	INT NumElements;
	INT NumMaterials;
	INT NumTriangles;
	INT NumVertices;
};

/**
 * Collect stats for a list of meshes
 */
static void CollectStatsForMeshes( TArray<UStaticMeshComponent*> &StaticMeshComponents, FMeshStatResults* OutStats )
{
	INT NumMeshes    = 0;
	INT NumElements  = 0;
	INT NumMaterials = 0;
	INT NumTriangles = 0;
	INT NumVertices  = 0;

	TArray<UMaterialInterface*> UsedMaterials;

	for (INT ComponentIndex=0; ComponentIndex < StaticMeshComponents.Num(); ++ComponentIndex)
	{
		UStaticMeshComponent* StaticMeshComponent = StaticMeshComponents(ComponentIndex);
		if (StaticMeshComponent == NULL)
		{
			continue;
		}

		UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh;
		if (StaticMesh == NULL)
		{
			continue;
		}

		NumMeshes++;

		if( StaticMesh->LODModels.Num() > 0 )
		{
			FStaticMeshRenderData* LODModel = &StaticMesh->LODModels(0);
			check(LODModel);

			NumTriangles += LODModel->IndexBuffer.Indices.Num() / 3;
			NumVertices  += LODModel->NumVertices;

			TArray<FStaticMeshElement>& Elements = LODModel->Elements;
			for (INT ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
			{
				UMaterialInterface* Material = StaticMeshComponent->GetMaterial(ElementIndex, 0);
				UsedMaterials.AddUniqueItem(Material);

				NumElements++;
			}
		}
	}

	OutStats->NumMeshes    = NumMeshes;
	OutStats->NumElements  = NumElements;
	OutStats->NumMaterials = UsedMaterials.Num();
	OutStats->NumTriangles = NumTriangles;
	OutStats->NumVertices  = NumVertices;
}

/**
 * Generates a modal-less message box UI with render specific statistics for a collection of static meshes.
 */
static void ReportStatsForMeshes( TArray<UStaticMeshComponent*> &MeshComponents )
{
	FMeshStatResults MeshStats;

	CollectStatsForMeshes(MeshComponents, &MeshStats);

	FString StatMessage = FString::Printf(		
		TEXT("Visible Meshes:\n")
		TEXT("    Meshes:\t%d\n")
		TEXT("    Elements:\t%d\n")
		TEXT("    Materials:\t%d\n")
		TEXT("    Triangles:\t%d (%dk)\n")
		TEXT("    Vertices:\t%d (%dk)\n")
		TEXT("\n"),

		MeshStats.NumMeshes,
		MeshStats.NumElements,
		MeshStats.NumMaterials,
		MeshStats.NumTriangles,
		MeshStats.NumTriangles/1000,
		MeshStats.NumVertices,
		MeshStats.NumVertices/1000);		

	if (!GIsUCC)
	{
		WxModelessPrompt* StatPrompt = new WxModelessPrompt( *StatMessage, LocalizeUnrealEd("RenderStats") );
		StatPrompt->Show();
	}
	debugf(*StatMessage);
}

/**
 * Uses the selected actors to collect rendering stats
 */
UBOOL AGroupActor::ReportStatsForSelectedActors()
{
	TArray<UStaticMeshComponent*> MeshComponents;

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = CastChecked<AActor>( *It );
		CollectVisibleMeshesFromActor(Actor, &MeshComponents);
	}

	ReportStatsForMeshes(MeshComponents);

	return TRUE;
}

/**
 * Uses the active group to collect rendering stats
 */
UBOOL AGroupActor::ReportStatsForSelectedGroups()
{
	const UBOOL bDeepSearch = FALSE;

	// Validate that we have one and only one group selected
	const INT NumActiveGroups = AGroupActor::NumActiveGroups( TRUE, bDeepSearch );
	if ( NumActiveGroups == 0 )
	{		
		return FALSE;	// No group selected
	}
	else if ( NumActiveGroups > 1 )
	{		
		return FALSE;	// Too many groups selected
	}

	// Get the selected group
	AGroupActor* SelectedGroup = AGroupActor::GetSelectedGroup( 0, bDeepSearch );
	if ( !SelectedGroup )
	{
		return FALSE;	// Couldn't find selected group (shouldn't happen!)
	}

	// Grab all the actors that belong to this group
	TArray<AActor*> Actors;
	SelectedGroup->GetGroupActors( Actors, FALSE );

	// Get all the static mesh actors (and components) we wish to collect information for

	TArray<UStaticMeshComponent*> MeshComponents;

	CollectVisibleMeshesFromActors( Actors, &MeshComponents );
	ReportStatsForMeshes(MeshComponents);

	return TRUE;
}

/**
 * Lock this group and all subgroups.
 */
void AGroupActor::Lock()
{
	bLocked = TRUE;
	for(INT SubGroupIdx=0; SubGroupIdx < SubGroups.Num(); ++SubGroupIdx )
	{
		AGroupActor* SubGroup = SubGroups(SubGroupIdx);
		if ( SubGroup )
		{
			SubGroup->Lock();
		}
	}
}

/**
 * @param	InActor	Actor to add to this group
 * @param	bInformProxy Should the proxy dialog be informed of this change (only when it's not moved to another group)
 * @param	bMaintainProxy Special case for when were remerging the proxy in the group, but don't want to unmark it as proxy too soon
 */
void AGroupActor::Add(AActor& InActor, const UBOOL bInformProxy, const UBOOL bMaintainProxy)
{	
	// See if the incoming actor already belongs to a group
	AGroupActor* InActorParent = GetParentForActor(&InActor);
	if(InActorParent) // If so, detach it first
	{
		if(InActorParent == this)
		{
			return;
		}
		InActorParent->Modify();
		InActorParent->Remove(InActor, FALSE);
	}
	
	Modify();
	AGroupActor* InGroupPtr = Cast<AGroupActor>(&InActor);
	if(InGroupPtr)
	{
		check(InGroupPtr != this);
		SubGroups.AddUniqueItem(InGroupPtr);
	}
	else
	{
		GroupActors.AddUniqueItem(&InActor);
#if ENABLE_SIMPLYGON_MESH_PROXIES
		if ( bInformProxy  )
		{
			// Make sure there's still only 1 proxy in the group
			VerifyProxy( bMaintainProxy, TRUE );

			// Make sure any actors related to this one are moved to, i.e. meshes hidden by proxy
			if ( InActorParent )
			{
				WxDlgCreateMeshProxy*	DlgCreateMeshProxy = GApp->GetDlgCreateMeshProxy();
				check(DlgCreateMeshProxy);
				DlgCreateMeshProxy->UpdateActorGroup( this, InActorParent, &InActor );
			}
		}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
	}
}

/**
 * Removes the given actor from this group. If the group has no actors after this transaction, the group itself is removed.
 * @param	InActor	Actor to remove from this group
 * @param	bInformProxy Should the proxy dialog be informed of this change (only when it's not moved to another group)
 * @param	bMaintainProxy Special case for when were removing the proxy from the group, but don't want to unmerge it
 * @param	bMaintainGroup Special case for when were reverting the proxy but want to keep it's meshes part of the group
 */
void AGroupActor::Remove(AActor& InActor, const UBOOL bInformProxy, const UBOOL bMaintainProxy, const UBOOL bMaintainGroup)
{
	AGroupActor* InGroupPtr = Cast<AGroupActor>(&InActor);
	if(InGroupPtr && SubGroups.ContainsItem(InGroupPtr))
	{
		Modify();
		SubGroups.RemoveItem(InGroupPtr);
	}
	else if(GroupActors.ContainsItem(&InActor))
	{
		Modify();
		GroupActors.RemoveItem(&InActor);

#if ENABLE_SIMPLYGON_MESH_PROXIES
		// Make sure any actors related to this one are moved to, i.e. meshes hidden by proxy
		if ( bInformProxy )
		{
			WxDlgCreateMeshProxy*	DlgCreateMeshProxy = GApp->GetDlgCreateMeshProxy();
			check(DlgCreateMeshProxy);
			DlgCreateMeshProxy->UpdateActorGroup( NULL, this, &InActor, bMaintainProxy, bMaintainGroup );
		}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
	}
	
	// If all children have been removed (or only one subgroup remains), this group is no longer active.
	if( GroupActors.Num() == 0 && SubGroups.Num() <= 1 )
	{
		// Remove any potentially remaining subgroups
		SubGroups.Empty();

		// Destroy the actor and remove it from active groups
		AGroupActor* ParentGroup = GetParentForActor(this);
		if(ParentGroup)
		{
			ParentGroup->Modify();
			ParentGroup->Remove(*this);
		}

		// Group is no longer active
		GEditor->ActiveGroupActors.RemoveItem(this);

		if( GWorld )
		{
			GWorld->ModifyLevel(GetLevel());
			
			// Mark the group actor for removal
			MarkPackageDirty();

			// If not currently garbage collecting (changing maps, saving, etc), remove the group immediately
			if(!GIsGarbageCollecting)
			{
				// Refresh all editor browsers after removal
				FScopedRefreshEditor_AllBrowsers LevelRefreshAllBrowsers;

				// Let the object propagator report the deletion
				GObjectPropagator->OnActorDelete(this);

				// Destroy group and clear references.
				GWorld->EditorDestroyActor( this, FALSE );			
				
				LevelRefreshAllBrowsers.Request();
			}
		}
	}
}

/**
 * @param InActor	Actor to search for
 * @return True if the group contains the given actor.
 */
UBOOL AGroupActor::Contains(AActor& InActor) const
{
	AActor* InActorPtr = &InActor;
	AGroupActor* InGroupPtr = Cast<AGroupActor>(InActorPtr);
	if(InGroupPtr)
	{
		return SubGroups.ContainsItem(InGroupPtr);
	}
	return GroupActors.ContainsItem(InActorPtr);
}

/**
	* Searches the group for a proxy
	* @return The proxy, if found
	*/
AStaticMeshActor* AGroupActor::ContainsProxy() const
{
#if ENABLE_SIMPLYGON_MESH_PROXIES
	// Check to see if there is a proxy in the group
	for(INT ActorIndex=0; ActorIndex<GroupActors.Num(); ++ActorIndex)
	{
		AStaticMeshActor* GroupActor = Cast< AStaticMeshActor >( GroupActors(ActorIndex) );
		if( GroupActor && GroupActor->StaticMeshComponent && GroupActor->StaticMeshComponent->StaticMesh && GroupActor->IsProxy() ) 
		{ 
			return GroupActor; 
		}
	}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
	return NULL;
}

/**
 * @param bDeepSearch	Flag to check all subgroups as well. Defaults to TRUE.
 * @return True if the group contains any selected actors.
 */
UBOOL AGroupActor::HasSelectedActors(UBOOL bDeepSearch/*=TRUE*/) const
{
	for(INT ActorIndex=0; ActorIndex<GroupActors.Num(); ++ActorIndex)
	{
		const AActor* GroupActor = GroupActors(ActorIndex);
		if( GroupActor && GroupActor->IsSelected() ) { return TRUE; }
	}
	if(bDeepSearch)
	{
		for(INT GroupIndex=0; GroupIndex<SubGroups.Num(); ++GroupIndex)
		{
			const AGroupActor* SubGroup = SubGroups(GroupIndex);
			if( SubGroup && SubGroup->HasSelectedActors(bDeepSearch) ) { return TRUE; }
		}
	}
	return false;
}

/**
 * Detaches all children (actors and subgroups) from this group and then removes it.
 * @param	bMaintainProxy Special case for when were removing the proxy from the group, but don't want to unmerge it
 */
void AGroupActor::ClearAndRemove(const UBOOL bMaintainProxy)
{
	for(INT ActorIndex=0; ActorIndex<GroupActors.Num(); ++ActorIndex)
	{
		AActor* GroupActor = GroupActors(ActorIndex);
		if ( GroupActor )
		{
			Remove(*GroupActor, TRUE, bMaintainProxy);
			--ActorIndex;
		}
	}
	for(INT SubGroupIndex=0; SubGroupIndex<SubGroups.Num(); ++SubGroupIndex)
	{
		AGroupActor* SubGroup = SubGroups(SubGroupIndex);
		if ( SubGroup )
		{
			Remove(*SubGroup);
			--SubGroupIndex;
		}
	}
}

/**
 * Sets this group's location to the center point based on current location of its children.
 */
void AGroupActor::CenterGroupLocation()
{
	FVector MinVector;
	FVector MaxVector;
	GetBoundingVectorsForGroup( this, NULL, MinVector, MaxVector );

	SetLocation((MinVector + MaxVector) * 0.5f);
	GEditor->NoteSelectionChange();
}

/**
 * @param	OutGroupActors	Array to fill with all actors for this group.
 * @param	bRecurse		Flag to recurse and gather any actors in this group's subgroups.
 */
void AGroupActor::GetGroupActors(TArray<AActor*>& OutGroupActors, UBOOL bRecurse/*=FALSE*/) const
{
	if( bRecurse )
	{
		for(INT i=0; i<SubGroups.Num(); ++i)
		{
			const AGroupActor* SubGroup = SubGroups(i);
			if ( SubGroup )
			{
				SubGroup->GetGroupActors(OutGroupActors, bRecurse);
			}
		}
	}
	else
	{
		OutGroupActors.Empty();
	}
	for(INT i=0; i<GroupActors.Num(); ++i)
	{
		AActor* GroupActor = GroupActors(i);
		if ( GroupActor )
		{
			OutGroupActors.AddItem(GroupActor);
		}
	}
}

/**
 * @param	OutSubGroups	Array to fill with all subgroups for this group.
 * @param	bRecurse		Flag to recurse and gather any subgroups in this group's subgroups.
 */
void AGroupActor::GetSubGroups(TArray<AGroupActor*>& OutSubGroups, UBOOL bRecurse/*=FALSE*/) const
{
	if( bRecurse )
	{
		for(INT i=0; i<SubGroups.Num(); ++i)
		{
			AGroupActor* SubGroup = SubGroups(i);
			if ( SubGroup )
			{
				SubGroup->GetSubGroups(OutSubGroups, bRecurse);
			}
		}
	}
	else
	{
		OutSubGroups.Empty();
	}
	for(INT i=0; i<SubGroups.Num(); ++i)
	{
		AGroupActor* SubGroup = SubGroups(i);
		if ( SubGroup )
		{
			OutSubGroups.AddItem(SubGroup);
		}
	}
}

/**
 * @param	OutChildren		Array to fill with all children for this group.
 * @param	bRecurse		Flag to recurse and gather any children in this group's subgroups.
 */
void AGroupActor::GetAllChildren(TArray<AActor*>& OutChildren, UBOOL bRecurse/*=FALSE*/) const
{
	GetGroupActors(OutChildren, bRecurse);
	TArray<AGroupActor*> OutSubGroups;
	GetSubGroups(OutSubGroups, bRecurse);
	for(INT SubGroupIdx=0; SubGroupIdx<OutSubGroups.Num(); ++SubGroupIdx)
	{
		OutChildren.AddItem(OutSubGroups(SubGroupIdx));
	}
}

IMPLEMENT_CLASS(AGroupActor);


