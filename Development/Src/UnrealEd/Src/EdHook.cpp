/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Includes.
#include "UnrealEd.h"
#include "..\..\Core\Inc\UnMsg.h"
#include "PropertyWindow.h"

// Thread exchange.
HANDLE			hEngineThreadStarted;
HANDLE			hEngineThread;
HWND			hWndEngine;
DWORD			EngineThreadId;
const TCHAR*	GItem;
const TCHAR*	GValue;
TCHAR*			GCommand;

extern int GLastScroll;

// Misc.
UEngine* Engine;

/*-----------------------------------------------------------------------------
	Editor hook exec.
-----------------------------------------------------------------------------*/

void UUnrealEdEngine::NotifyDestroy( void* Src )
{
	const INT idx = ActorProperties.FindItemIndex( (WxPropertyWindowFrame*)Src );
	if( idx != INDEX_NONE )
	{
		ActorProperties.Remove( idx );
	}
	if( Src==LevelProperties )
		LevelProperties = NULL;
	if( Src==GApp->ObjectPropertyWindow )
		GApp->ObjectPropertyWindow = NULL;
}
void UUnrealEdEngine::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{
	BeginTransaction( *LocalizeUnrealEd("EditProperties") );
}
void UUnrealEdEngine::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	EndTransaction();

	if( ActorProperties.FindItemIndex( (WxPropertyWindowFrame*)Src ) != INDEX_NONE )
	{
		GCallbackEvent->Send( CALLBACK_ActorPropertiesChange );
	}

	// Notify all active modes of actor property changes.
	TArray<FEdMode*> ActiveModes;
	GEditorModeTools().GetActiveModes( ActiveModes );

	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		ActiveModes(ModeIndex)->ActorPropChangeNotify();
	}
}
void UUnrealEdEngine::NotifyExec( void* Src, const TCHAR* Cmd )
{
	// Keep a pointer to the beginning of the string to use for message displaying purposes
	const TCHAR* const FullCmd = Cmd;

	if( ParseCommand(&Cmd,TEXT("BROWSECLASS")) )
	{
		appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"),FullCmd));
	}
	else if( ParseCommand(&Cmd,TEXT("USECURRENT")) )
	{
		appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"),FullCmd));
	}
}

/** Updates the property windows of selected actors */
void UUnrealEdEngine::UpdatePropertyWindows()
{
	TArray<UObject*> SelectedObjects;

	UBOOL bProcessed = FALSE;
	if (GetSelectedActorCount() == 1)
	{
		FSelectionIterator It( GetSelectedActorIterator() );
		if ( It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if ( !Actor->bDeleteMe )
			{
				// Currently only for Landscape
				// Landscape uses Component Selection
				bProcessed = Actor->GetSelectedComponents(SelectedObjects);
			}
		}
	}
	
	if (!bProcessed)
	{
		// Assemble a set of valid selected actors.
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if ( !Actor->bDeleteMe )
			{
				SelectedObjects.AddItem( Actor );
			}
		}
	}

	UpdatePropertyWindowFromActorList( SelectedObjects );
}

/** 
*	Updates the property windows of the actors in the supplied ActorList
*
*	@param	ActorList	The list of actors whose property windows should be updated
*
*/
void UUnrealEdEngine::UpdatePropertyWindowFromActorList( const TArray< UObject *>& ActorList )
{
	for( INT x = 0 ; x < ActorProperties.Num() ; ++x )
	{
		if( !ActorProperties(x)->IsLocked() )
		{
			// Update the unlocked window.
			ActorProperties(x)->SetObjectArray( ActorList, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories  );
		}
		else
		{
			ActorProperties(x)->Freeze();
			// Validate the contents of any locked windows.  Any object that is being viewed inside
			// of a locked property window but is no longer valid needs to be removed.
			TArray<AActor*> ActorResetArray;
			UBOOL bAllFound = TRUE;

			for ( WxPropertyWindowFrame::TObjectIterator Itor( ActorProperties(x)->ObjectIterator() ) ; Itor ; ++Itor )
			{
				AActor* Actor = Cast<AActor>( *Itor );

				// Iterate over all actors, searching for the selected actor.
				// @todo DB: Optimize -- much object iteration happening here...
				UBOOL Found = FALSE;
				for( FActorIterator It; It; ++It )
				{
					if( Actor == *It )
					{
						Found = TRUE;
						break;
					}
				}

				// If the selected actor no longer exists, remove it from the property window.
				if( Found )
				{
					ActorResetArray.AddItem(Actor);
				}
				else
				{
					bAllFound = FALSE;
				}
			}
			if (!bAllFound)
			{
				//although locked, force a rebuild on this array
				ActorProperties(x)->SetObjectArray(ActorResetArray, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories);
			}

			ActorProperties(x)->Thaw();
		}
	}
}
