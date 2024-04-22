/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"

#if WITH_MANAGED_CODE
#include "ContentBrowserShared.h"
#include "GameAssetDatabaseShared.h"
#include "LocalizationExport.h"
#endif


/**
 * Notifies all observers that are registered for this event type
 * that the event has fired. This version stores/restores the GWorld
 * object before passing on the message.
 *
 * @param InType the event that was fired
 * @param InViewport the viewport associated with this event
 * @param InMessage the message for this event
 */
void FCallbackEventDeviceEditor::Send(ECallbackEventType InType,
	FViewport* InViewport,UINT InMessage)
{
	if ( InViewport != NULL )
	{
		switch( InType )
		{
			case CALLBACK_PreWindowsMessage:
			{
				// Make sure the proper GWorld is set before handling the windows message
				if( GEditor->GameViewport && GEditor->GameViewport->Viewport == InViewport )
				{
					// remember the current GWorld that will be restored in the PostWindowsMessage callback
					SavedGWorld = GWorld;
					SetPlayInEditorWorld( GEditor->PlayWorld );
				}
				else
				{
					SavedGWorld = NULL;
				}
				// Let any other registered parties do their thing
				FCallbackEventObserver::Send(InType,InViewport,InMessage);
				break;
			}
			case CALLBACK_PostWindowsMessage:
			{
				// Let any other registered parties do their thing
				FCallbackEventObserver::Send(InType,InViewport,InMessage);
				// Restore the proper GWorld.
				if( SavedGWorld )
				{
					RestoreEditorWorld( SavedGWorld );
				}			
				break;
			}

			default:
			{
				FCallbackEventObserver::Send(InType,InViewport,InMessage);
				break;
			}
		}
	}
}

/**
 * Notifies all observers that are registered for this event type
 * that the event has fired. This version handles updating FEdMode's
 * mode bars
 *
 * @param InType the event that was fired
 * @param InEdMode the FEdMode that is changing
 *
 * @todo figure out which FEdModes need this and register them directly
 */
void FCallbackEventDeviceEditor::Send(ECallbackEventType InType,FEdMode* InEdMode)
{
	// Let others get the event
	FCallbackEventObserver::Send(InType,InEdMode);
}

/**
 * Notifies all observers that are registered for this event type
 * that the event has fired.
 *
 * If EventMessage is set, this version stores/restores the GWorld object before passing on the message
 * If EventMode is EventEditorMode is set, this version handles updating FEdMode's mode bars.
 *
 * @param	Parms	the parameters for the event
 */
void FCallbackEventDeviceEditor::Send( const FCallbackEventParameters& Parms )
{
	switch( Parms.EventType )
	{
	case CALLBACK_PreWindowsMessage:
		{
			check(Parms.EventViewport);

			// Make sure the proper GWorld is set before handling the windows message
			if( GEditor->GameViewport && GEditor->GameViewport->Viewport == Parms.EventViewport )
			{
				// remember the current GWorld that will be restored in the PostWindowsMessage callback
				SavedGWorld = GWorld;
				SetPlayInEditorWorld( GEditor->PlayWorld );
			}
			else
			{
				SavedGWorld = NULL;
			}
			// Let any other registered parties do their thing
			FCallbackEventObserver::Send(Parms);
			break;
		}
	case CALLBACK_PostWindowsMessage:
		{
			check(Parms.EventViewport);

			// Let any other registered parties do their thing
			FCallbackEventObserver::Send(Parms);
			
			// Restore the proper GWorld.
			if( SavedGWorld )
			{
				RestoreEditorWorld( SavedGWorld );
			}			
			break;
		}
	case CALLBACK_EditorModeEnter:
		{
			FCallbackEventObserver::Send(Parms);
			break;
		}
	case CALLBACK_EditorModeExit:
		{
			FCallbackEventObserver::Send(Parms);
			break;
		}
	default:
		{
			FCallbackEventObserver::Send(Parms);
			break;
		}
	}
}


UBOOL FCallbackQueryDeviceEditor::Query( ECallbackQueryType InType, const FString& InString )
{
	switch (InType)
	{
		case CALLBACK_LoadObjectsOnTop:
			return GEditor && (FFilename(InString).GetBaseFilename() == FFilename(GEditor->UserOpenedFile).GetBaseFilename());

		default:
			check(0);	// Unknown callback
	}

	return FALSE;
}

UBOOL FCallbackQueryDeviceEditor::Query( ECallbackQueryType InType, UObject* QueryObject )
{
	switch ( InType )
	{
		case CALLBACK_AllowPackageSave:
			{
				UPackage* Pkg = Cast<UPackage>(QueryObject);
				if ( GUnrealEd != NULL && Pkg != NULL )
				{
					return GUnrealEd->CanSavePackage(Pkg);
				}

				break;
			}

		default:
			checkf(0, TEXT("Unknown query callback type encountered by FCallbackQueryDeviceEditor::Query: %d"), InType);
	}

	return FALSE;
}

UBOOL FCallbackQueryDeviceEditor::Query( FCallbackQueryParameters& Parms )
{
	UBOOL bResult = FALSE;

	switch ( Parms.QueryType )
	{
	case CALLBACK_LoadObjectsOnTop:
		{
			bResult = GEditor && (FFilename(Parms.QueryString).GetBaseFilename() == FFilename(GEditor->UserOpenedFile).GetBaseFilename());
			break;
		}
	case CALLBACK_AllowPackageSave:
		{
			UPackage* Pkg = Cast<UPackage>(Parms.QueryObject);
			if ( GUnrealEd != NULL && Pkg != NULL )
			{
				bResult = GUnrealEd->CanSavePackage(Pkg);
			}
			break;
		}
	case CALLBACK_QuerySelectedAssets:
		{
#if WITH_MANAGED_CODE
			if ( FContentBrowser::IsInitialized() )
			{
				FContentBrowser& CB = FContentBrowser::GetActiveInstance();

				FString SelectedAssetPaths;
				if ( CB.GenerateSelectedAssetString(&SelectedAssetPaths) > 0 )
				{
					Parms.ResultString = SelectedAssetPaths;
					bResult = TRUE;
				}
			}
#endif
			break;
		}
	case CALLBACK_LocalizationExportFilter:
		{
#if WITH_MANAGED_CODE
			// If the game asset database is accessible and a filter request has been made, see if the query
			// object passes the filter.
			if ( FGameAssetDatabase::IsInitialized() && Parms.QueryObject && !Parms.QueryString.IsEmpty() )
			{
				const FGameAssetDatabase& GAD = FGameAssetDatabase::Get();

				// Query all of the GAD tags associated with the query object and put them in a set for fast lookup
				TArray<FString> AssetTags;
				GAD.QueryTagsForAsset( FName( *Parms.QueryObject->GetFullName() ), ETagQueryOptions::AllTags, AssetTags );

				TSet<FString> AssetTagSet;
				AssetTagSet.Add( AssetTags );

				// Build a filter object from the query string
				FLocalizationExportFilter CurFilter( Parms.QueryString );
				const TArray<FString>& FilterTags = CurFilter.GetFilterTags();
				FLocalizationExportFilter::ETagFilterType TagFilterType = CurFilter.GetTagFilterType();

				// If the user specified filtering to occur, check the filter tags vs. the asset tags to see if the
				// asset passes the filter
				if ( FilterTags.Num() > 0 && TagFilterType != FLocalizationExportFilter::TFT_None )
				{					
					// "Match Any:" If any of the filter tags are present on the asset, it passes the filter
					if ( TagFilterType == FLocalizationExportFilter::TFT_MatchAny )
					{
						bResult = FALSE;
						for ( TArray<FString>::TConstIterator FilterTagIter( FilterTags ); FilterTagIter; ++FilterTagIter )
						{
							if ( AssetTagSet.Contains( *FilterTagIter ) )
							{
								bResult = TRUE;
								break;
							}
						}
					}
					// "Match All:" If all of the filter tags are present on the asset, it passes the filter
					else
					{
						bResult = TRUE;
						for ( TArray<FString>::TConstIterator FilterTagIter( FilterTags ); FilterTagIter; ++FilterTagIter )
						{
							if ( !AssetTagSet.Contains( *FilterTagIter ) )
							{
								bResult = FALSE;
								break;
							}
						}
					}
				}
				// Assume the asset passes the filter if no filtering was specified
				else
				{
					bResult = TRUE;
				}
			}
#endif // #if WITH_MANAGED_CODE
			break;
		}
	case CALLBACK_DoReferenceChecks:
		{
			return GEditor && GEditor->bDoReferenceChecks;
		}
	default:
		checkf(0, TEXT("Unknown query callback type encountered by FCallbackQueryDeviceEditor::Query: %d"), Parms.QueryType);
	}

	return bResult;
}


INT FCallbackQueryDeviceEditor::Query( ECallbackQueryType InType, const FString& InString, UINT InMessage )
{
	switch( InType )
	{
	case CALLBACK_ModalErrorMessage:
		{
			EAppMsgType MessageType = static_cast<EAppMsgType>(InMessage);
			return WxChoiceDialog::WxAppMsgF(InString, MessageType);
		}
		break;
	}

	return 0;
}



