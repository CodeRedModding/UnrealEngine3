/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "ScopedTransaction.h"
#if ENABLE_SIMPLYGON_MESH_PROXIES
#include "DlgCreateMeshProxy.h"
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES

/**
 * Creates a new group from the current selection maintaining existing groups.
 */
UBOOL UUnrealEdEngine::edactGroupFromSelected()
{
	UBOOL bRet = FALSE;
	if(GEditor->bGroupingActive)
	{
		TArray<AActor*> ActorsToAdd;
		ULevel* ActorLevel = NULL;

		UBOOL bActorsInSameLevel = TRUE;
		INT iProxyCount = 0;
		TArray<AGroupActor*> GroupsToAdd;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = CastChecked<AActor>( *It );

			if( !ActorLevel )
			{
				ActorLevel = Actor->GetLevel();
			}
			else if( ActorLevel != Actor->GetLevel() )
			{
				bActorsInSameLevel = FALSE;
				break;
			}

			// See if a group selected to be added into the new group
			AGroupActor* GroupActor = Cast<AGroupActor>(Actor);
			if(GroupActor == NULL) // Aren't directly selecting a group, see if the actor has a locked parent
			{
				GroupActor = AGroupActor::GetParentForActor(Actor);
				// If the actor has a locked parent, add it. Otherwise, ignore it.
				if(GroupActor && GroupActor->IsLocked())
				{
					GroupsToAdd.AddUniqueItem(GroupActor);
					continue;
				}
			}
#if ENABLE_SIMPLYGON_MESH_PROXIES
			// If this actor is part of the proxy, make note as we can't group multiple proxies (only 1 per group)
			AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(Actor);
			if ( SMActor && ( SMActor->IsProxy() || SMActor->IsHiddenByProxy() ) )
			{
				iProxyCount++;
			}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
			ActorsToAdd.AddUniqueItem(Actor);
		}

		// Do we have a situation where we have to remerge proxies?...
		UBOOL bRemergeProxy = FALSE;
#if ENABLE_SIMPLYGON_MESH_PROXIES
		if ( iProxyCount > 1 )
		{
			bRemergeProxy = TRUE;

			// ...Ask the user if they want to go ahead with it
			if (!appMsgf(AMT_YesNo, *LocalizeUnrealEd("Group_ContainsMultipleProxies")))
			{
				return FALSE;
			}
		}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES

		// Remove sub groups to avoid loosing hierarchy when doing our adds below.
		AGroupActor::RemoveSubGroupsFromArray(GroupsToAdd);
		for(INT GroupIdx=0; GroupIdx < GroupsToAdd.Num(); ++GroupIdx)
		{
			ActorsToAdd.AddItem(GroupsToAdd(GroupIdx));
		}

		// Must be creating a group with at least two actors (actor + group, two groups, etc)
		if( ActorsToAdd.Num() > 1 && bActorsInSameLevel )
		{
			check(ActorLevel);

			// Store off the current level and make the level that contain the actors to group as the current level
			ULevel* PrevLevel = GWorld->CurrentLevel;
			GWorld->CurrentLevel = ActorLevel;

			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd("Group_Create") );

				AGroupActor* SpawnedGroupActor = Cast<AGroupActor>( GWorld->SpawnActor(AGroupActor::StaticClass()) );

				for ( INT ActorIndex = 0; ActorIndex < ActorsToAdd.Num(); ++ActorIndex )
				{
					AActor* Actor = ActorsToAdd(ActorIndex);
					Actor->Modify();

					// Add each selected actor to our new group
					SpawnedGroupActor->Add(*Actor, TRUE, TRUE );
					bRet = TRUE;
				}
#if ENABLE_SIMPLYGON_MESH_PROXIES
				// If we need to remerge the proxy (due to multiple proxies being present)
				if ( bRemergeProxy )
				{
					WxDlgCreateMeshProxy*	DlgCreateMeshProxy = GApp->GetDlgCreateMeshProxy();
					check(DlgCreateMeshProxy);
					if ( DlgCreateMeshProxy->UpdateActorGroup( NULL, SpawnedGroupActor, NULL, FALSE, TRUE, FALSE ) )	// Ungroup the proxy, but maintain the hiddenflags
					{
						if ( DlgCreateMeshProxy->EvaluateGroup( SpawnedGroupActor ) )	// Recalculate what needs remerging
						{
							DlgCreateMeshProxy->Remerge( TRUE, FALSE );	// Remerge the proxy
						}
					}
				}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
				SpawnedGroupActor->CenterGroupLocation();
				SpawnedGroupActor->Lock();
			}

			// Restore the previous level that was current
			GWorld->CurrentLevel = PrevLevel;

			// Refresh all editor browsers after adding
			FScopedRefreshEditor_AllBrowsers LevelRefreshAllBrowsers;
			LevelRefreshAllBrowsers.Request();
		}
		else if( !bActorsInSameLevel )
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Group_CantCreateGroupMultipleLevels") );
		}
	}
	return bRet;
}

/**
 * Creates a new group from the current selection removing any attachments to existing groups.
 */
UBOOL UUnrealEdEngine::edactRegroupFromSelected()
{
	UBOOL bRet = FALSE;
	if(GEditor->bGroupingActive)
	{
		TArray<AActor*> ActorsToAdd;
		ULevel* ActorLevel = NULL;

		UBOOL bActorsInSameLevel = TRUE;
		INT iProxyCount = 0;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ); It; ++It )
		{
			AActor* Actor = CastChecked<AActor>(*It);
			if( !ActorLevel )
			{
				ActorLevel = Actor->GetLevel();
			}
			else if( ActorLevel != Actor->GetLevel() )
			{
				bActorsInSameLevel = FALSE;
				break;
			}

			AGroupActor* GroupActor = Cast<AGroupActor>(Actor);
			if(GroupActor == NULL) // Aren't directly selecting a group
			{
#if ENABLE_SIMPLYGON_MESH_PROXIES
				// If this actor is part of the proxy, make note as we can't regroup multiple proxies (only 1 per group)
				AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(Actor);
				if ( SMActor && ( SMActor->IsProxy() || SMActor->IsHiddenByProxy() ) )
				{
					iProxyCount++;
				}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
				// Add each selected actor to our new group
				// Adding an actor will remove it from any existing groups.
				ActorsToAdd.AddItem( Actor );
			}
		}

		// Do we have a situation where we have to remerge proxies?...
		UBOOL bRemergeProxy = FALSE;
#if ENABLE_SIMPLYGON_MESH_PROXIES
		if ( iProxyCount > 1 )
		{
			bRemergeProxy = TRUE;

			// ...Ask the user if they want to go ahead with it
			if (!appMsgf(AMT_YesNo, *LocalizeUnrealEd("Group_ContainsMultipleProxies")))
			{
				return FALSE;
			}
		}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
		if( bActorsInSameLevel )
		{
			if( ActorsToAdd.Num() > 1 )
			{
				// Store off the current level and make the level that contain the actors to group as the current level
				ULevel* PrevLevel = GWorld->CurrentLevel;
				GWorld->CurrentLevel = ActorLevel;

				{
					const FScopedTransaction Transaction( *LocalizeUnrealEd("Group_Regroup") );

					AGroupActor* SpawnedGroupActor = Cast<AGroupActor>( GWorld->SpawnActor(AGroupActor::StaticClass()) );

					for( INT ActorIndex = 0; ActorIndex < ActorsToAdd.Num(); ++ActorIndex )
					{
						SpawnedGroupActor->Add( *ActorsToAdd(ActorIndex), TRUE, TRUE );
						bRet = TRUE;
					}
#if ENABLE_SIMPLYGON_MESH_PROXIES
					// If we need to remerge the proxy (due to multiple proxies being present)
					if ( bRemergeProxy )
					{
						WxDlgCreateMeshProxy*	DlgCreateMeshProxy = GApp->GetDlgCreateMeshProxy();
						check(DlgCreateMeshProxy);
						if ( DlgCreateMeshProxy->UpdateActorGroup( NULL, SpawnedGroupActor, NULL, FALSE, TRUE, FALSE ) )	// Ungroup the proxy, but maintain the hiddenflags
						{
							if ( DlgCreateMeshProxy->EvaluateGroup( SpawnedGroupActor ) )	// Recalculate what needs remerging
							{
								DlgCreateMeshProxy->Remerge( TRUE, FALSE );	// Remerge the proxy
							}
						}
					}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
					SpawnedGroupActor->CenterGroupLocation();
					SpawnedGroupActor->Lock();
				}

				// Restore the previous level that was current
				GWorld->CurrentLevel = PrevLevel;
			}
		}
		else
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Group_CantCreateGroupMultipleLevels") );
		}
	}
	return bRet;
}

/**
 * Disbands any groups in the current selection, does not attempt to maintain any hierarchy
 */
UBOOL UUnrealEdEngine::edactUngroupFromSelected()
{
	UBOOL bRet = FALSE;
	if(GEditor->bGroupingActive)
	{
		TArray<AGroupActor*> OutermostGroupActors;
		
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = CastChecked<AActor>( *It );

			// Get the outermost locked group
			AGroupActor* OutermostGroup = AGroupActor::GetRootForActor( Actor, TRUE );
			if( OutermostGroup == NULL )
			{
				// Failed to find locked root group, try to find the immediate parent
				OutermostGroup = AGroupActor::GetParentForActor( Actor );
			}
			
			if( OutermostGroup )
			{
				OutermostGroupActors.AddUniqueItem( OutermostGroup );
			}
		}

		if( OutermostGroupActors.Num() )
		{
			UBOOL bAskAboutUngrouping = TRUE;
			UBOOL bMaintainProxy = FALSE;
			const FScopedTransaction Transaction( *LocalizeUnrealEd("Group_Disband") );
			for( INT GroupIndex = 0; GroupIndex < OutermostGroupActors.Num(); ++GroupIndex )
			{
				AGroupActor* GroupActor = OutermostGroupActors(GroupIndex);
#if ENABLE_SIMPLYGON_MESH_PROXIES
				// If any group contains a proxy, verify with the user that they definitely want to ungroup it
				if ( GroupActor->ContainsProxy() )
				{
					if (bAskAboutUngrouping)
					{
						UINT MsgResult = appMsgf(AMT_YesNoYesAllNoAllCancel, LocalizeSecure(LocalizeUnrealEd("Group_ContainsProxyUngroup"), *GroupActor->GetName()));
						if ( MsgResult == ART_Cancel )
						{
							return FALSE;
						}
						bMaintainProxy = MsgResult == ART_No || MsgResult == ART_NoAll;
						bAskAboutUngrouping = MsgResult == ART_No || MsgResult == ART_Yes;
					}
				}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
				GroupActor->ClearAndRemove( bMaintainProxy );
			}
			GEditor->NoteSelectionChange();
			bRet = TRUE;
		}
	}
	return bRet;
}

/**
 * Locks any groups in the current selection
 */
UBOOL UUnrealEdEngine::edactLockSelectedGroups()
{
	UBOOL bRet = FALSE;
	if(GEditor->bGroupingActive)
	{
		bRet = AGroupActor::LockSelectedGroups();
	}
	return bRet;
}

/**
 * Unlocks any groups in the current selection
 */
UBOOL UUnrealEdEngine::edactUnlockSelectedGroups()
{
	UBOOL bRet = FALSE;
	if(GEditor->bGroupingActive)
	{
		bRet = AGroupActor::UnlockSelectedGroups();
	}
	return bRet;
}

/**
 * Activates "Add to Group" mode which allows the user to select a group to append current selection
 */
UBOOL UUnrealEdEngine::edactAddToGroup()
{
	UBOOL bRet = FALSE;
	if(GEditor->bGroupingActive)
	{
		bRet = AGroupActor::AddSelectedActorsToSelectedGroup();
	}
	return bRet;
}

/** 
 * Removes any groups or actors in the current selection from their immediate parent.
 * If all actors/subgroups are removed, the parent group will be destroyed.
 */
UBOOL UUnrealEdEngine::edactRemoveFromGroup()
{
	UBOOL bRet = FALSE;
	if(GEditor->bGroupingActive)
	{
		bRet = AGroupActor::RemoveSelectedActorsFromSelectedGroup();
	}
	return bRet;
}

/**
 * Gather stats for selected group
 */
UBOOL UUnrealEdEngine::edactReportStatsForSelectedGroups()
{
	UBOOL bRet = FALSE;
	if(GEditor->bGroupingActive)
	{
		bRet = AGroupActor::ReportStatsForSelectedGroups();
	}
	return bRet;
}

/**
 * Gather stats for selected group
 */
UBOOL UUnrealEdEngine::edactReportStatsForSelection()
{
	UBOOL bRet = AGroupActor::ReportStatsForSelectedActors();
	return bRet;
}