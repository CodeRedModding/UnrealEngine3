/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "AssetSelection.h"
//#include "..\..\Launch\Resources\resource.h"
#include "EngineSequenceClasses.h"
#include "EnginePhysicsClasses.h"
#include "EnginePrefabClasses.h"
#include "EngineMeshClasses.h"
#include "EngineAnimClasses.h"
#include "Menus.h"
#include "SpeedTree.h"
#include "UnConsoleSupportContainer.h"
#include "EngineFogVolumeClasses.h"
#include "EngineFluidClasses.h"
#include "EngineMaterialClasses.h"
#include "EngineSplineClasses.h"
#include "EngineProcBuildingClasses.h"
#include "EngineFoliageClasses.h"
#include "EngineAIClasses.h"
#include "GameFrameworkClasses.h"
#include "EditorLevelUtils.h"

#if WITH_SIMPLYGON
#include "SimplygonMeshUtilities.h"
#endif // #if WITH_SIMPLYGON

#if WITH_MANAGED_CODE
// CLR includes
#include "ContentBrowserShared.h"
#endif


/*-----------------------------------------------------------------------------
	WxMainContextMenuBase.
-----------------------------------------------------------------------------*/

WxMainContextMenuBase::WxMainContextMenuBase()
:	ActorFactoryMenu( NULL ),
	ActorFactoryMenuAdv( NULL ),
	ReplaceWithActorFactoryMenu( NULL ),
	ReplaceWithActorFactoryMenuAdv( NULL ),
	RecentClassesMenu( NULL ),
	LastMenuSeparatorIndex( INDEX_NONE )
{
}

/**
 * @param	Class	The class to query.
 * @return			TRUE if the specified class can be added to a level.
 */
static UBOOL IsClassPlaceable(const UClass* Class)
{
	const UBOOL bIsAdable =
		Class
		&&  (Class->ClassFlags & CLASS_Placeable)
		&& !(Class->ClassFlags & CLASS_Abstract)
		&& !(Class->ClassFlags & CLASS_Deprecated)
		&& Class->IsChildOf( AActor::StaticClass() );
	return bIsAdable;
}

/**
 * @return	TRUE if the builder brush is in the list of selected actors; FALSE, otherwise. 
 */
static UBOOL IsBuilderBrushSelected()
{
	UBOOL bHasBuilderBrushSelected = FALSE;

	for( FSelectionIterator SelectionIter = GEditor->GetSelectedActorIterator(); SelectionIter; ++SelectionIter )
	{
		if( (*SelectionIter)->IsA(AActor::StaticClass()) && CastChecked<AActor>(*SelectionIter)->IsABuilderBrush() )
		{
			bHasBuilderBrushSelected = TRUE;
			break;
		}
	}

	return bHasBuilderBrushSelected;
}

void WxMainContextMenuBase::AppendAddActorMenu()
{
	AppendSeparatorIfNeeded();
	
	USelection* SelectionSet = GEditor->GetSelectedObjects();

	// Add 'add actor of selected class' option.
	UClass* SelClass = SelectionSet->GetTop<UClass>();
	ABrush* BrushDefs = NULL;
	UBOOL bPlaceable = TRUE;
	if(SelClass->IsChildOf(ABrush::StaticClass()))
	{
		BrushDefs = CastChecked<ABrush>(SelClass->GetDefaultActor());
		bPlaceable = BrushDefs->bPlaceableFromClassBrowser;
	}

	if( IsClassPlaceable( SelClass ) && bPlaceable )
	{
		const FString wk = FString::Printf( LocalizeSecure(LocalizeUnrealEd("AddHereF"), *SelClass->GetName()) );
		Append( ID_BackdropPopupAddClassHere, *wk, TEXT("") );
		if (GEditor->GetSelectedActorCount() > 0 && !IsBuilderBrushSelected())
		{
			Append(ID_BackdropPopupReplaceWithClass, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("ReplaceWithClass"), *SelClass->GetName())), TEXT(""));
		}
	}

	// 'Add Prefab' menu option, if a prefab is selected.
	UPrefab* Prefab = SelectionSet->GetTop<UPrefab>();
	if(Prefab)
	{
		Append( IDM_ADDPREFAB, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("AddPrefabF"), *Prefab->GetName()) ), TEXT("") );
	}

	// gather the list of selected assets, some (or all) of which may not be loaded yet
	TArray<FSelectedAssetInfo> SelectedLoadedAssets, SelectedUnloadedAssets;

#if WITH_MANAGED_CODE
	// when using the content browser, use GCallbackQuery to retrieve the list of selected assets
	FCallbackQueryParameters Parms(NULL, CALLBACK_QuerySelectedAssets);
	if ( FContentBrowser::IsInitialized() && GCallbackQuery->Query(Parms) && Parms.ResultString.Len() > 0 )
	{
		TArray<FSelectedAssetInfo> SelectedAssets;
		FContentBrowser::UnmarshalAssetItems(Parms.ResultString, SelectedAssets);

		for ( INT Idx = 0; Idx < SelectedAssets.Num(); Idx++ )
		{
			FSelectedAssetInfo& AssetInfo = SelectedAssets(Idx);
			if ( AssetInfo.IsValid(TRUE) )
			{
				SelectedLoadedAssets.AddItem(AssetInfo);
			}
			else
			{
				SelectedUnloadedAssets.AddItem(AssetInfo);
			}
		}
	}
	else
#endif
	{
		// no content browser - normal generic browser selection
		for( USelection::TObjectIterator It( SelectionSet->ObjectItor() ); It; ++It )
		{
			UObject* SelectedObject = *It;
			new(SelectedLoadedAssets) FSelectedAssetInfo(SelectedObject);
		}
	}

	ActorFactoryMenu = NULL;
	ReplaceWithActorFactoryMenu = NULL;

	TArray<FString> SelectedAssetMenuOptions;
	// this will create and populate the actor factory menus with available actor factories
	CreateActorFactoryMenus(SelectedLoadedAssets, &ActorFactoryMenu, &ReplaceWithActorFactoryMenu, &SelectedAssetMenuOptions );

	for( INT OptionIndex = 0; OptionIndex < SelectedAssetMenuOptions.Num(); ++OptionIndex )
	{		
		wxString MenuString = *SelectedAssetMenuOptions(OptionIndex);
		if( MenuString.Len() > 0 )
		{
			Append( IDMENU_ActorFactory_Start+OptionIndex, MenuString, TEXT("") );
		}
	}

	// Add an "add <class>" option here for the most recent actor classes that were selected in the level.
	RecentClassesMenu = new wxMenu();

	USelection::TClassConstIterator Itor = GEditor->GetSelectedActors()->ClassConstItor();
	for( ; Itor ; ++Itor )
	{
		if( IsClassPlaceable( *Itor ) && !(*Itor)->IsChildOf(ABrush::StaticClass()) )
		{
			const FString wk = FString::Printf( LocalizeSecure(LocalizeUnrealEd("AddF"), *(*Itor)->GetName()) );
			RecentClassesMenu->Append( ID_BackdropPopupAddLastSelectedClassHere_START+Itor.GetIndex(), *wk, TEXT("") );
		}
	}

	Append( IDMENU_SurfPopupAddRecentMenu, *LocalizeUnrealEd("AddRecent"), RecentClassesMenu );

	wxMenuItem* tmpItem = ActorFactoryMenu->FindChildItem(IDMENU_SurfPopupAddActorAdvMenu);
	ActorFactoryMenuAdv = tmpItem->GetSubMenu();
	if ( ReplaceWithActorFactoryMenu != NULL )
	{
		tmpItem = ReplaceWithActorFactoryMenu->FindChildItem(IDMENU_SurfPopupReplaceActorAdvMenu);
		ReplaceWithActorFactoryMenuAdv = tmpItem->GetSubMenu();
	}
	
	// if we had any assets selected which are not loaded but have factories that deal with that type, offer
	// the user the option to load these assets
	if ( SelectedUnloadedAssets.Num() > 0 )
	{
		for ( INT AssetIdx = 0; AssetIdx < SelectedUnloadedAssets.Num(); AssetIdx++ )
		{
			FSelectedAssetInfo& AssetInfo = SelectedUnloadedAssets(AssetIdx);
			if ( FActorFactoryAssetProxy::GetFactoryForAsset(AssetInfo, FALSE) != NULL )
			{
				const wxString MenuItemString = *FString::Printf(LocalizeSecure(LocalizeUnrealEd(TEXT("LoadAssetF")), *AssetInfo.GetAssetFullName()));

				ActorFactoryMenu->AppendSeparator();
				ActorFactoryMenu->Append(IDMENU_ActorFactory_LoadSelectedAsset, MenuItemString);

				if ( ReplaceWithActorFactoryMenu != NULL )
				{
					ReplaceWithActorFactoryMenu->AppendSeparator();
					ReplaceWithActorFactoryMenu->Append(IDMENU_ActorFactory_LoadSelectedAsset, MenuItemString);
				}
				break;
			}
		}
	}

	// finally, append the actor factory submenus to the main context menu
	Append( IDMENU_SurfPopupAddActorMenu, *LocalizeUnrealEd("AddActor"), ActorFactoryMenu );
	if (ReplaceWithActorFactoryMenu != NULL)
	{
		Append( IDMENU_SurfPopupReplaceActorMenu, *LocalizeUnrealEd("ReplaceWithActorFactory"), ReplaceWithActorFactoryMenu );
	}
}

/**
 * Helper method to append the convert volumes submenu to a provided menu
 *
 * @param	InParentMenu	Menu to append the convert volumes submenu to
 */
void WxMainContextMenuBase::AppendConvertVolumeSubMenu( wxMenu* InParentMenu )
{
	wxMenu* ConvertVolumeMenu = new wxMenu();

	// Get all of the volume classes sorted alphabetically
	TArray<UClass*> VolumeClasses;
	GApp->EditorFrame->GetSortedVolumeClasses( &VolumeClasses );

	// Add each volume class to the menu as a possible choice
	INT MenuID = IDM_ConvertVolumeClasses_START;
	for ( TArray<UClass*>::TConstIterator VolumeIter(VolumeClasses); VolumeIter; ++VolumeIter )
	{
		const UClass* CurVolumeClass = *VolumeIter;
		ConvertVolumeMenu->Insert( 0, MenuID, *CurVolumeClass->GetName(), TEXT("") );
		++MenuID;
	}
	InParentMenu->AppendSubMenu( ConvertVolumeMenu, *LocalizeUnrealEd("ConvertToVolume") );
}

void WxMainContextMenuBase::AppendPlayLevelMenu()
{
	AppendSeparatorIfNeeded();

	wxMenu* PlayLevelMenu = new wxMenu();

	// if we have any console plugins, add them to the list of places we can play the level
	if (FConsoleSupportContainer::GetConsoleSupportContainer()->GetNumConsoleSupports() > 0)
	{
		// loop through all consoles (only support 20 consoles)
		INT ConsoleIndex = 0;
		for (FConsoleSupportIterator It; It && ConsoleIndex < 20; ++It, ConsoleIndex++)
		{
			FString ConsoleName = It->GetPlatformName();
			if( ConsoleName == CONSOLESUPPORT_NAME_ANDROID ||
				ConsoleName == CONSOLESUPPORT_NAME_MAC )
			{
				// No "Play On" support for the Android or Mac platforms (yet!)
			}
			else
			{
				if( ConsoleName == CONSOLESUPPORT_NAME_IPHONE )
				{
					// For iPhones add an "Install on" menu option
					PlayLevelMenu->Append(
						IDM_BackDropPopupPlayFromHereConsole_START + ConsoleIndex, 
						*FString::Printf(*LocalizeUnrealEd("ToolTip_InstallOnIOSDevice")), 
						*FString::Printf(LocalizeSecure(LocalizeUnrealEd("LevelViewportContext_PlayOn_Desc"), *ConsoleName))
						);
				}
				else
				{
					// add a per-console Play From Here On XXX menu
					PlayLevelMenu->Append(
						IDM_BackDropPopupPlayFromHereConsole_START + ConsoleIndex, 
						*FString::Printf(LocalizeSecure(LocalizeUnrealEd("LevelViewportContext_PlayOnF"), *ConsoleName)), 
						*FString::Printf(LocalizeSecure(LocalizeUnrealEd("LevelViewportContext_PlayOn_Desc"), *ConsoleName))
						);
				}
			
				if( ConsoleName == CONSOLESUPPORT_NAME_IPHONE )
				{
					PlayLevelMenu->Append(
						IDM_BackDropPopupPlayFromHereUsingMobilePreview, 
						*FString::Printf(LocalizeSecure(LocalizeUnrealEd("LevelViewportContext_PlayOnF"), *LocalizeUnrealEd(TEXT("MobilePreview")))), 
						*FString::Printf(LocalizeSecure(LocalizeUnrealEd("LevelViewportContext_PlayOn_Desc"), *LocalizeUnrealEd(TEXT("MobilePreview"))))
						);
				}

				PlayLevelMenu->AppendSeparator();
			}
		}
	}

	{
		const FString PlayFromHereLabel = GEditor->OnlyLoadEditorVisibleLevelsInPIE() ? LocalizeUnrealEd("LevelViewportContext_PlayFromHereInViewport_VisibleOnly") : LocalizeUnrealEd("LevelViewportContext_PlayFromHereInViewport");
		PlayLevelMenu->Append(IDM_BackDropPopupPlayFromHereInEditorViewport, *PlayFromHereLabel, *LocalizeUnrealEd("LevelViewportContext_PlayFromHereInViewport_Desc"));
	}

	Append( wxID_ANY, *LocalizeUnrealEd("LevelViewportContext_PlayLevelMenu"), PlayLevelMenu );

	// NOTE: LDs prefer this to be the bottom-most entry in the context menu.  Also we don't
	//		want to bury this one in a sub-menu as it's used very frequently
	{
		Append( IDM_BackDropPopupForcePlayFromHereInEditor,  *LocalizeUnrealEd("LevelViewportContext_ForcePlayFromHere"),  *LocalizeUnrealEd("LevelViewportContext_ForcePlayFromHere_Desc") );
		const FString PlayFromHereLabel = GEditor->OnlyLoadEditorVisibleLevelsInPIE() ? LocalizeUnrealEd("LevelViewportContext_PlayFromHere_VisibleOnly") : LocalizeUnrealEd("LevelViewportContext_PlayFromHere");
		Append( IDM_BackDropPopupPlayFromHereInEditor, *PlayFromHereLabel, *LocalizeUnrealEd("LevelViewportContext_PlayFromHere_Desc"));
	}
}


void WxMainContextMenuBase::AppendActorVisibilityMenuItems( const UBOOL bAnySelectedActors )
{
	// Actor visibility menu
	wxMenu* HideMenu = new wxMenu();

	HideMenu->Append( IDM_SHOW_ALL, *LocalizeUnrealEd("ShowAll"), TEXT("") );
	
	if( bAnySelectedActors )
	{
		HideMenu->Append( IDM_SELECT_SHOW, *LocalizeUnrealEd("ShowSelectedOnly"), TEXT("") );
		HideMenu->Append( IDM_SELECT_HIDE, *LocalizeUnrealEd("HideSelected"), TEXT("") );
	}

	HideMenu->AppendSeparator();	

	// Append option to show all actors at editor startup
	HideMenu->Append( IDM_SHOW_ALL_AT_STARTUP, *LocalizeUnrealEd("ShowAllAtStartup"), TEXT("") );
	if( bAnySelectedActors )
	{
		// Append options to show/hide the selected actors at editor startup
		HideMenu->Append( IDM_SELECT_SHOW_AT_STARTUP, *LocalizeUnrealEd("ShowSelectedAtStartup") , TEXT("") );
		HideMenu->Append( IDM_SELECT_HIDE_AT_STARTUP, *LocalizeUnrealEd("HideSelectedAtStartup"), TEXT("") );
	}
	
	Append( IDMENU_HideMenu, *LocalizeUnrealEd("ShowHideActors"), HideMenu );
}


void WxMainContextMenuBase::AppendEditMenuItems( const UBOOL bCanCutCopy, const UBOOL bCanPaste )
{
	if( bCanPaste || bCanCutCopy )
	{
		AppendSeparatorIfNeeded();

		{
			wxMenuItem* NewMenuItem = Append( IDM_CUT, *LocalizeUnrealEd("LevelViewportContext_Cut"), *LocalizeUnrealEd("ToolTip_93") );
			NewMenuItem->Enable( bCanCutCopy ? true : false );
		}
		{
			wxMenuItem* NewMenuItem = Append( IDM_COPY, *LocalizeUnrealEd("LevelViewportContext_Copy"), *LocalizeUnrealEd("ToolTip_94") );
			NewMenuItem->Enable( bCanCutCopy ? true : false  );
		}

		{
			wxMenuItem* NewMenuItem = Append( IDM_PASTE, *LocalizeUnrealEd("LevelViewportContext_Paste"), *LocalizeUnrealEd("ToolTip_95") );
			NewMenuItem->Enable( bCanPaste ? true : false  );
		}
		{
			wxMenuItem* NewMenuItem = Append( IDM_PASTE_HERE, *LocalizeUnrealEd("LevelViewportContext_PasteHere"), *LocalizeUnrealEd("ToolTip_158") );
			NewMenuItem->Enable( bCanPaste ? true : false  );
		}
	}
}

/**
 * Creates and populates actor factory context sub-menus.
 *
 * @param	SelectedAssets		the list of currently selected assets
 * @param	pmenu_CreateActor	receives the pointer to the wxMenu that holds Create Actor factory options.  *pmenu_CreateActor must be NULL.
 * @param	pmenu_ReplaceActor	receives the pointer to the wxMenu for Replace Actor Using Factory options.  *pmenu_ReplaceActor must be NULL.
 */
void WxMainContextMenuBase::CreateActorFactoryMenus( const TArray<FSelectedAssetInfo>& SelectedAssets, wxMenu** pmenu_CreateActor, wxMenu** pMenu_ReplaceActor, TArray<FString>* OutSelectedAssetMenuOptions )
{
	if ( pmenu_CreateActor == NULL || pMenu_ReplaceActor == NULL )
	{
		return;
	}

	if ( *pmenu_CreateActor != NULL || *pMenu_ReplaceActor != NULL )
	{
		warnf(TEXT("CreateActorFactoryMenus - expected to pass menu references which are pointing to NULL"));
		return;
	}

	// Create actor factory entries, both for adding new and replacing existing actors
	wxMenu* menu_PlaceActor = new wxMenu();
	wxMenu* menu_PlaceActorAdv = new wxMenu();

	wxMenu* menu_ReplaceActor = NULL;
	wxMenu* menu_ReplaceActorAdv = NULL;
	if ( GEditor->GetSelectedActorCount() > 0 && pMenu_ReplaceActor != NULL && !IsBuilderBrushSelected() )
	{
		menu_ReplaceActor = new wxMenu();
		menu_ReplaceActorAdv = new wxMenu();
	}

	FString Unused;
	TArray<FString> QuickMenuItems, AdvancedMenuItems;
	FActorFactoryAssetProxy::GenerateActorFactoryMenuItems(SelectedAssets, QuickMenuItems, AdvancedMenuItems, OutSelectedAssetMenuOptions, FALSE, FALSE, TRUE);
	
	// Add selected asset menu options to the replace with menu
	if( OutSelectedAssetMenuOptions && menu_ReplaceActor != NULL )
	{
		INT MenuIndex;
		for( MenuIndex = 0; MenuIndex < OutSelectedAssetMenuOptions->Num(); ++MenuIndex )
		{
			wxString MenuString = *(*OutSelectedAssetMenuOptions)(MenuIndex);
			if( MenuString.Len() > 0 )
			{
				menu_ReplaceActor->Append( IDMENU_ReplaceWithActorFactory_Start+MenuIndex, MenuString, TEXT("") );
			}
		}

		menu_ReplaceActor->AppendSeparator();
	}

	for(INT i=0; i<QuickMenuItems.Num(); i++)
	{
		check(i < AdvancedMenuItems.Num());

		wxString QuickMenuString = *QuickMenuItems(i);
		wxString AdvancedMenuString = *AdvancedMenuItems(i);

		// The basic menu only shows factories that can be run without any intervention
		if( QuickMenuString.Len() > 0 )
		{
			menu_PlaceActor->Append( IDMENU_ActorFactory_Start+i, QuickMenuString, TEXT("") );
			if (menu_ReplaceActor != NULL)
			{
				menu_ReplaceActor->Append( IDMENU_ReplaceWithActorFactory_Start+i, QuickMenuString, TEXT("") );
			}
		}

		// The advanced menu shows all of them.
		if ( IDMENU_ActorFactoryAdv_Start + i > IDMENU_ActorFactoryAdv_End ||
			IDMENU_ReplaceWithActorFactoryAdv_Start + i > IDMENU_ReplaceWithActorFactoryAdv_End )
		{
			appErrorf(TEXT("Maximum of %i Actor Factory classes exceeded! Increase IDMENU_ActorFactoryAdv_End and IDMENU_ReplaceWithActorFactoryAdv_End to allocate more space."), i);
		}

		menu_PlaceActorAdv->Append( IDMENU_ActorFactoryAdv_Start+i, AdvancedMenuString, TEXT("") );
		if (menu_ReplaceActorAdv != NULL)
		{
			menu_ReplaceActorAdv->Append( IDMENU_ReplaceWithActorFactoryAdv_Start+i, AdvancedMenuString, TEXT("") );
		}
	}

	menu_PlaceActor->AppendSeparator();
	menu_PlaceActor->Append( IDMENU_SurfPopupAddActorAdvMenu, *LocalizeUnrealEd("AllTemplates"), menu_PlaceActorAdv );
	if ( menu_ReplaceActor != NULL )
	{
		menu_ReplaceActor->AppendSeparator();
		menu_ReplaceActor->Append( IDMENU_SurfPopupReplaceActorAdvMenu, *LocalizeUnrealEd("AllTemplates"), menu_ReplaceActorAdv );
	}

	*pmenu_CreateActor = menu_PlaceActor;
	if ( pMenu_ReplaceActor != NULL )
	{
		*pMenu_ReplaceActor = menu_ReplaceActor;
	}
}

/*-----------------------------------------------------------------------------
	WxMainContextMenu.
-----------------------------------------------------------------------------*/


WxMainContextMenu::WxMainContextMenu()
{
	// Look at what is selected and record information about it for later.

	UBOOL bHaveBrush = FALSE;
	UBOOL bHaveBuilderBrush = FALSE;
	UBOOL bHaveStaticMesh = FALSE;
	UBOOL bHaveInteractiveFoliageMesh = FALSE;
	UBOOL bHaveStaticMeshComponent = FALSE;
	UBOOL bHaveCollisionComponent = FALSE;
	UBOOL bHaveNonSimplifiedStaticMesh = FALSE;
	UBOOL bHaveSimplifiedStaticMesh = FALSE;
	UBOOL bHaveFracturedStaticMesh = FALSE;
	UBOOL bHaveDynamicStaticMesh = FALSE;
	UBOOL bHaveSkeletalMesh = FALSE;
	UBOOL bHaveKAsset = FALSE;
	UBOOL bHavePawn = FALSE;
	UBOOL bHaveSelectedActors = FALSE;
	UBOOL bSelectedActorsBelongToSameLevel = TRUE;
	UBOOL bSelectedActorsBelongToSameLevelGridVolume = TRUE;
	UBOOL bAllSelectedActorsBelongToCurrentLevel = TRUE;
	UBOOL bAllSelectedActorsBelongToCurrentLevelGridVolume = TRUE;
	UBOOL bHaveCamera = FALSE;					// True if a camera is selected.
	UBOOL bHaveKActor = FALSE;
	UBOOL bHaveMover = FALSE;
	UBOOL bHaveEmitter = FALSE;
	UBOOL bHaveRoute = FALSE;
	UBOOL bHaveCoverGroup = FALSE;
	UBOOL bHavePrefabInstance = FALSE;
	UBOOL bHaveActorInPrefab = FALSE;
	UBOOL bHaveLight = FALSE;
	UBOOL bHaveSpeedTree = FALSE;
	UBOOL bHaveSpline = FALSE;
	UBOOL bHaveProcBuilding = FALSE;
	UBOOL bHaveImageReflectionSceneCapture = FALSE;
	UBOOL bAllSelectedLightsHaveSameClassification = TRUE;
	UBOOL bAllSelectedStaticMeshesHaveCollisionModels = TRUE;	// For deciding whether or not to offer StaticMesh->KActor conversion
	UBOOL bFoundLockedActor = FALSE;
	UBOOL bSelectedActorsHaveAttachedActors = FALSE;
	UBOOL bSelectedLockedGroup = FALSE; // a locked group is in our selection
	UBOOL bSelectedUnlockedGroup = FALSE; // an unlocked group is in our selection
	UBOOL bSelectedSubGroup = FALSE; // selected a subgroup
	
	AActor* FirstActor = NULL;
	INT NumSelectedActors = 0;
	INT NumSelectedUngroupedActors = 0;
	INT NumSelectedBrushes = 0;
	INT NumNavPoints = 0;
	INT NumCoverLinks = 0;
	INT NumCrowdDestinations = 0;
	ALevelStreamingVolume* FirstSelectedLevelStreamingVolume = NULL;
	ALevelGridVolume* FirstSelectedLevelGridVolume = NULL;

	// For light conversions, whether or not we can perform these conversions
	UBOOL bCanConvertToMovable = FALSE;
	UBOOL bCanConvertToDominant = FALSE;
	UClass* SelectedLightClass = NULL;

	ULevel* SharedLevel = NULL;			// If non-NULL, all selected actors belong to this level.
	ALevelGridVolume* SharedLevelGridVolume = NULL;		// If non-NULL, all selected actors belong to this level grid volume
	ELightAffectsClassification LightAffectsClassification = LAC_MAX; // Only valid if bAllSelectedLightsHaveSameClassification is TRUE.

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if( bAllSelectedActorsBelongToCurrentLevel )
		{
			ULevel* ActorLevel = Actor->GetLevel();
			if( ActorLevel != GWorld->CurrentLevel )
			{
				bAllSelectedActorsBelongToCurrentLevel = FALSE;
			}
		}

		if( bAllSelectedActorsBelongToCurrentLevelGridVolume )
		{
			if( GWorld->CurrentLevelGridVolume == NULL ||
				!GWorld->CurrentLevelGridVolume->IsActorMemberOfGrid( Actor ) )
			{
				bAllSelectedActorsBelongToCurrentLevelGridVolume = FALSE;
			}
		}

		// To prevent move to other level for Landscape if its components are distributed in streaming levels
		if (Actor->IsA(ALandscape::StaticClass()))
		{
			ALandscape* Landscape = CastChecked<ALandscape>(Actor);
			if (!Landscape || !Landscape->HasAllComponent())
			{
				if( !bAllSelectedActorsBelongToCurrentLevel )
				{
					bAllSelectedActorsBelongToCurrentLevel = TRUE;
				}
				if (!bAllSelectedActorsBelongToCurrentLevelGridVolume)
				{
					bAllSelectedActorsBelongToCurrentLevelGridVolume = TRUE;
				}
			}
		}

		if ( bSelectedActorsBelongToSameLevel )
		{
			ULevel* ActorLevel = Actor->GetLevel();
			if ( !SharedLevel )
			{
				// This is the first selected actor we've encountered.
				SharedLevel = ActorLevel;
			}
			else
			{
				// Does this actor's level match the others?
				if ( SharedLevel != ActorLevel )
				{
					bSelectedActorsBelongToSameLevel = FALSE;
					SharedLevel = NULL;
				}
			}
		}

		if ( bSelectedActorsBelongToSameLevelGridVolume )
		{
			ALevelGridVolume* ActorLevelGridVolume = EditorLevelUtils::GetLevelGridVolumeForActor( Actor );
			if( ActorLevelGridVolume != NULL )
			{
				if ( !SharedLevelGridVolume )
				{
					// This is the first selected actor we've encountered.
					SharedLevelGridVolume = ActorLevelGridVolume;
				}
				else
				{
					// Does this actor's level match the others?
					if ( SharedLevelGridVolume != ActorLevelGridVolume )
					{
						bSelectedActorsBelongToSameLevelGridVolume = FALSE;
						SharedLevelGridVolume = NULL;
					}
				}
			}
			else
			{
				// At least one actor doesn't belong to any level grid volume
				bSelectedActorsBelongToSameLevelGridVolume = FALSE;
				SharedLevelGridVolume = NULL;
			}
		}

		bSelectedActorsHaveAttachedActors = bSelectedActorsHaveAttachedActors || (Actor->Attached.Num() > 0);
		
		FirstActor = Actor;

		bHaveSelectedActors = TRUE;

		NumSelectedActors++;

		if ( Actor->bLockLocation )
		{
			bFoundLockedActor = TRUE;
		}

		for(INT j=0; j<Actor->AllComponents.Num(); j++)
		{
			UActorComponent* Comp = Actor->AllComponents(j);
			UStaticMeshComponent* SMComp = Cast<UStaticMeshComponent>(Comp);
			if( SMComp )
			{
				bHaveStaticMeshComponent = TRUE;
			}
		}

		if( Actor->CollisionComponent != NULL )
		{
			bHaveCollisionComponent = TRUE;
		}

		if( Actor->IsBrush() )
		{
			if( !bHaveBrush )
			{
				bHaveBrush = TRUE;

				if(Actor->IsA(AProcBuilding::StaticClass()))
				{
					bHaveProcBuilding = TRUE;
				}

				if( FirstSelectedLevelStreamingVolume == NULL && Actor->IsA( ALevelStreamingVolume::StaticClass() ) )
				{
					FirstSelectedLevelStreamingVolume = CastChecked<ALevelStreamingVolume>( Actor );
				}

				if( FirstSelectedLevelGridVolume == NULL && Actor->IsA( ALevelGridVolume::StaticClass() ) )
				{
					FirstSelectedLevelGridVolume = CastChecked<ALevelGridVolume>( Actor );
				}

				ABrush* Brush = CastChecked<ABrush>( Actor );
				bHaveBuilderBrush = Brush->IsABuilderBrush();
			}
				
			NumSelectedBrushes++;
		}
		else if(Actor->IsA(ASpeedTreeActor::StaticClass()))
		{
			bHaveSpeedTree = TRUE;
		}
		else if(Actor->IsA(AImageReflectionSceneCapture::StaticClass()))
		{
			bHaveImageReflectionSceneCapture = TRUE;
		}
		else if( Actor->IsA(AStaticMeshActor::StaticClass()) )
		{
			bHaveStaticMesh = TRUE;
			AStaticMeshActor* StaticMeshActor = (AStaticMeshActor*)( Actor );
			if ( StaticMeshActor->StaticMeshComponent )
			{
				UStaticMesh* StaticMesh = StaticMeshActor->StaticMeshComponent->StaticMesh;

				bAllSelectedStaticMeshesHaveCollisionModels &= ( (StaticMesh && StaticMesh->BodySetup) ? TRUE : FALSE );

				if( StaticMesh != NULL )
				{
					bHaveSimplifiedStaticMesh = StaticMesh->bHasBeenSimplified;
					bHaveNonSimplifiedStaticMesh = !StaticMesh->bHasBeenSimplified;
				}
			}
			else
			{
				bAllSelectedStaticMeshesHaveCollisionModels = FALSE;
				//appErrorf( TEXT("Static mesh actor has no static mesh component") );
			}
			if (StaticMeshActor->IsA(AInteractiveFoliageActor::StaticClass()))
			{
				bHaveInteractiveFoliageMesh = TRUE;
			}
		}
		else if( Actor->IsA(AFracturedStaticMeshActor::StaticClass()) )
		{
			bHaveFracturedStaticMesh = TRUE;
		}
		else if( Actor->IsA(ADynamicSMActor::StaticClass()) )
		{
			bHaveDynamicStaticMesh = TRUE;
			ADynamicSMActor* StaticMeshActor = (ADynamicSMActor*)( Actor );
			if ( StaticMeshActor->StaticMeshComponent )
			{
				UStaticMesh* StaticMesh = StaticMeshActor->StaticMeshComponent->StaticMesh;

				bAllSelectedStaticMeshesHaveCollisionModels &= ( (StaticMesh && StaticMesh->BodySetup) ? TRUE : FALSE );

				if( StaticMesh != NULL )
				{
					bHaveSimplifiedStaticMesh = StaticMesh->bHasBeenSimplified;
					bHaveNonSimplifiedStaticMesh = !StaticMesh->bHasBeenSimplified;
				}
			}
			else
			{
				bAllSelectedStaticMeshesHaveCollisionModels = FALSE;
				//appErrorf( TEXT("DynamicSM actor has no static mesh component") );
			}

			if( Actor->IsA(AKActor::StaticClass()) )
			{
				bHaveKActor = TRUE;
			}
			else if( Actor->IsA(AInterpActor::StaticClass()) )
			{ 
				bHaveMover = TRUE; 
			} 
		}
		else if( Actor->IsA(APawn::StaticClass()) )
		{
			bHavePawn = TRUE;
		}
		else if( Actor->IsA(ASkeletalMeshActor::StaticClass()) )
		{
			bHaveSkeletalMesh = TRUE;
		}
		else if( Actor->IsA(AKAsset::StaticClass()) )
		{
			bHaveKAsset = TRUE;
		}
		else if( Actor->IsA(ANavigationPoint::StaticClass()) )
		{
			NumNavPoints++;
			if (Actor->IsA(ACoverLink::StaticClass()))
			{
				NumCoverLinks++;
			}
		}
		else if (Actor->IsA(APrefabInstance::StaticClass()))
		{
			bHavePrefabInstance = TRUE;
		}
		else if ( Actor->IsA(ACameraActor::StaticClass()) )
		{
			bHaveCamera = TRUE;
		}
		else if (Actor->IsA(AEmitter::StaticClass()))
		{
			bHaveEmitter = TRUE;
		}
		else if ( Actor->IsA(ARoute::StaticClass()) )
		{
			bHaveRoute = TRUE;
		}
		else if ( Actor->IsA(ACoverGroup::StaticClass()) )
		{
			bHaveCoverGroup = TRUE;
		}
		else if( Actor->IsA(ALight::StaticClass()) )
		{
			SelectedLightClass = Actor->GetClass();
			bHaveLight = TRUE;
			if( bAllSelectedLightsHaveSameClassification )
			{
				ALight* Light = (ALight*)( Actor );
				if( Light->LightComponent )
				{
					if( LightAffectsClassification == LAC_MAX )
					{
						LightAffectsClassification = (ELightAffectsClassification)Light->LightComponent->LightAffectsClassification;
					}
					else if( LightAffectsClassification != Light->LightComponent->LightAffectsClassification )
					{
						bAllSelectedLightsHaveSameClassification = FALSE;
					}
				}
				else
				{
					bAllSelectedLightsHaveSameClassification = FALSE;			
				}				
			}

			if( Actor->IsA( APointLight::StaticClass() ) || Actor->IsA( ASpotLight::StaticClass() ) )
			{
				bCanConvertToDominant = TRUE;
				bCanConvertToMovable = TRUE;
			}
			else if( Actor->IsA( ADirectionalLight::StaticClass() ) )
			{
				bCanConvertToDominant = TRUE;
			}

		}
		else if( Actor->IsA(ASplineActor::StaticClass()) )
		{
			bHaveSpline = TRUE;
		}
		else if ( Actor->IsA(AGameCrowdDestination::StaticClass()) )
		{
			NumCrowdDestinations++;
		}

		if( !bHaveActorInPrefab )
		{
			bHaveActorInPrefab = Actor->IsInPrefabInstance();
		}
		
		AGroupActor* FoundGroup = Cast<AGroupActor>(Actor);
		if(!FoundGroup)
		{
			FoundGroup = AGroupActor::GetParentForActor(Actor);
		}
		if( FoundGroup )
		{
			if( !bSelectedSubGroup )
			{
				bSelectedSubGroup = AGroupActor::GetParentForActor(FoundGroup) != NULL;
			}
			if( !bSelectedLockedGroup )
			{
				 bSelectedLockedGroup = FoundGroup->IsLocked();
			}
			if( !bSelectedUnlockedGroup )
			{
				AGroupActor* FoundRoot = AGroupActor::GetRootForActor(Actor);
				bSelectedUnlockedGroup = !FoundGroup->IsLocked() || ( FoundRoot && !FoundRoot->IsLocked() );
			}
		}
		else
		{
			NumSelectedUngroupedActors++;
		}
	}


	FGetInfoRet gir = GApp->EditorFrame->GetInfo( GI_NUM_SELECTED | GI_CLASSNAME_SELECTED | GI_CLASS_SELECTED );

	if( bHaveSelectedActors )
	{
		// Properties
		if( gir.iValue > 1 )
		{
			Append( IDMENU_ActorPopupProperties, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("LevelViewportContext_Properties_MultipleF"), *(gir.String), gir.iValue) ), TEXT("") );
		}
		else
		{
			Append( IDMENU_ActorPopupProperties, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("LevelViewportContext_PropertiesF"), *(gir.String)) ), TEXT("") );
		}

		// Find
		AppendSeparatorIfNeeded();
		Append( ID_SyncContentBrowser, *LocalizeUnrealEd("SyncContentBrowser"), TEXT(""), 0 );

		// If only 1 Actor is selected, and its referenced by Kismet, offer option to find it.
		if(NumSelectedActors == 1)
		{
			check(FirstActor);
			// Get the kismet sequence for the level the actor belongs to.
			USequence* RootSeq = GWorld->GetGameSequence( FirstActor->GetLevel() );
			if( RootSeq && RootSeq->ReferencesObject(FirstActor) )
			{
				Append( IDMENU_FindActorInKismet, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("LevelViewportContext_FindKismetF"), *FirstActor->GetName())), TEXT("") );
			}
		}

		// Go to Actor (Move viewport cameras)
		Append( IDMENU_ActorPopupAlignCameras, *LocalizeUnrealEd("LevelViewportContext_MoveCamerasToActor"), TEXT("") );

		// Snap View to Actor
		Append( IDMENU_ActorPopupSnapViewToActor, *LocalizeUnrealEd("SnapViewToActor"), TEXT("") );

		// Teleport to Click Point (Only show for perspective viewport)
		for( INT CurViewportIndex = 0; CurViewportIndex < GApp->EditorFrame->ViewportConfigData->GetViewportCount(); ++CurViewportIndex )
		{
			FVCD_Viewport& CurViewport = GApp->EditorFrame->ViewportConfigData->AccessViewport( CurViewportIndex );
			if( CurViewport.bEnabled && CurViewport.ViewportType == LVT_Perspective && CurViewport.ViewportWindow == GCurrentLevelEditingViewportClient)
			{
				Append( IDMENU_PopupMoveCameraToPoint, *LocalizeUnrealEd("MoveCameraToPoint"), TEXT("") );
				break;
			}
		}
	}



	// Select
	{
		wxMenu* SelectMenu = new wxMenu();
	
		SelectMenu->Append( IDMENU_ActorPopupSelectKismetReferenced, *LocalizeUnrealEd("SelectKismetReferencedActors"), TEXT("") );
		SelectMenu->Append( IDMENU_ActorPopupSelectKismetReferencedAll, *LocalizeUnrealEd("SelectKismetReferencedActorsAll"), TEXT("") );

		UBOOL AllSelectedActorsOfSameType = FALSE;
		UBOOL AllSelectedActorsHaveSameArchetype = FALSE;
		if( bHaveSelectedActors )
		{
			// Check if multiple types of actors are selected
			AllSelectedActorsOfSameType = TRUE;

			// Get the class type of the first actor.
			AActor* FirstActor = GEditor->GetSelectedActors()->GetTop<AActor>();
			check(FirstActor);// SelectedActor should definitely exist here
			UClass* FirstClass = FirstActor->GetClass();
			UObject* FirstArchetype = FirstActor->GetArchetype();

			// If the first actor doesn't have a valid archetype, then not all actors will have the same archetype
			AllSelectedActorsHaveSameArchetype = FirstArchetype ? TRUE : FALSE;

			// Start the iteration on the second actor.  The first is our baseline
			FSelectionIterator SelectedActorItr( GEditor->GetSelectedActorIterator() );
			++SelectedActorItr;

			// Compare all actor types with the baseline.
			for ( SelectedActorItr; SelectedActorItr; ++SelectedActorItr )
			{
				AActor* CurrentActor = static_cast<AActor*>( *SelectedActorItr );
				UClass* CurrentClass = CurrentActor->GetClass();
				if( FirstClass != CurrentClass )
				{
					AllSelectedActorsOfSameType = FALSE;
					AllSelectedActorsHaveSameArchetype = FALSE;
					break;
				}

				UObject* CurrentArchetype = CurrentActor->GetArchetype();
				if ( FirstArchetype != CurrentArchetype )
				{
					AllSelectedActorsHaveSameArchetype = FALSE;
				}
			}
		}


		UBOOL AllSelectedAreBrushes = FALSE;
		if( bHaveBrush )
		{
			AllSelectedAreBrushes = TRUE;
			for ( FSelectionIterator ActorItr( GEditor->GetSelectedActorIterator() ) ; ActorItr ; ++ActorItr )
			{
				AActor* CurrentActor = static_cast<AActor*>( *ActorItr );
				if( !(CurrentActor->IsBrush()) )
				{
					AllSelectedAreBrushes = FALSE;
					break;
				}
			}
		}

		if( !bHaveBrush && AllSelectedActorsOfSameType )
		{
			//Select actors of the currently selected type
			SelectMenu->Append( IDMENU_ActorPopupSelectAllClass, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("LevelViewportContext_SelectAllActorsOfTypeF"), *gir.String) ), TEXT("") );
			
			// Select actors of the currently selected type and archetype
			if ( AllSelectedActorsHaveSameArchetype )
			{
				SelectMenu->Append( IDMENU_ActorPopupSelectAllClassWithArchetype, *FString::Printf( LocalizeSecure( LocalizeUnrealEd("LevelViewportContext_SelectAllActorsOfTypeFWithArchetype"), *gir.String, *( GEditor->GetSelectedActors()->GetTop<AActor>()->GetArchetype()->GetName() ) ) ), TEXT("") );
			}
		}

		// Only add select all of the currently selected type if they are all brushes 
		if ( AllSelectedAreBrushes )
		{
			// Use a special string for brush to handle the different plural suffix
			if(gir.pClass != ABrush::StaticClass())
			{
				SelectMenu->Append( IDMENU_ActorPopupSelectAllClass, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("LevelViewportContext_SelectAllBrushesOfTypeF"), *(gir.String)) ), TEXT("") );
			}
			else
			{
				SelectMenu->Append( IDMENU_ActorPopupSelectAllClass, *LocalizeUnrealEd("LevelViewportContext_SelectAllBrushes"), TEXT("") );
			}
		}
		
		if( bHaveSelectedActors )
		{
			if( bSelectedActorsHaveAttachedActors )
			{
				SelectMenu->Append( IDMENU_ActorPopupSelectAllBased, *LocalizeUnrealEd("LevelViewportContext_SelectBasedActors") );
			}

			// Select by property.
			FString PropertyValue;
			UProperty* Property;
			UClass* CommonBaseClass;
			FEditPropertyChain* PropertyChain;
			GEditor->GetPropertyColorationTarget( PropertyValue, Property, CommonBaseClass, PropertyChain );
			if ( Property && CommonBaseClass )
			{
				SelectMenu->Append( IDM_SELECT_ByProperty, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("SelectByPropertyF"), *CommonBaseClass->GetName(), *Property->GetName(), *PropertyValue) ), TEXT("") );
			}

			SelectMenu->Append( IDM_SELECT_RELEVANT_LIGHTS, *LocalizeUnrealEd("SelectRelevantLights"), TEXT("") );
			SelectMenu->Append( IDM_SELECT_RELEVANT_DOMINANT_LIGHTS, *LocalizeUnrealEd("SelectRelevantDominantLights"), TEXT("") );

			if( bHaveStaticMesh || bHaveDynamicStaticMesh || bHaveFracturedStaticMesh)
			{
				SelectMenu->Append( IDMENU_ActorPopupSelectMatchingStaticMeshesThisClass, *LocalizeUnrealEd("SelectMatchingStaticMeshesThisClass"), TEXT("") );
				SelectMenu->Append( IDMENU_ActorPopupSelectMatchingStaticMeshesAllClasses, *LocalizeUnrealEd("SelectMatchingStaticMeshesAllClasses"), TEXT("") );
			}

			if(bHaveActorInPrefab)
			{
				SelectMenu->Append( IDM_SELECTALLACTORSINPREFAB, *LocalizeUnrealEd("Prefab_SelectActors"), TEXT("") );
			}

			if( bHaveLight )
			{
				if( bAllSelectedLightsHaveSameClassification )
				{
					SelectMenu->Append( IDMENU_ActorPopupSelectAllLightsWithSameClassification, *LocalizeUnrealEd("SelectAllLightsWithSameClassification"), TEXT("") );
				}
			}

			if( bHavePawn || bHaveSkeletalMesh || bHaveKAsset )
			{
				SelectMenu->Append( IDMENU_ActorPopupSelectMatchingSkeletalMeshesThisClass, *LocalizeUnrealEd("SelectMatchingSkeletalMeshesThisClass"), TEXT("") );
				SelectMenu->Append( IDMENU_ActorPopupSelectMatchingSkeletalMeshesAllClasses, *LocalizeUnrealEd("SelectMatchingSkeletalMeshesAllClasses"), TEXT("") );
			}

			if ( bHaveSkeletalMesh )
			{
				// Add a menu for converting skeletal meshes
				wxMenu* ConvertSkelMeshMenu = new wxMenu();

				INT ClassID = IDMENU_ConvertSkelMesh_START;
				for( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
				{
					UClass* CurClass = *ClassIt;
					if( ( CurClass->ClassFlags & CLASS_Placeable ) && CurClass->IsChildOf( ASkeletalMeshActor::StaticClass() ) )
					{			
						ConvertSkelMeshMenu->Append( ClassID, *CurClass->GetName(), TEXT("") );
						++ClassID;
					}
				}

				Append( IDMENU_ConvertSkelMeshMenu, *LocalizeUnrealEd("ConvertSkeletalMesh"), ConvertSkelMeshMenu );
			}

			if ( bHaveEmitter )
			{
				SelectMenu->Append(IDMENU_ActorPopupSelectMatchingEmitter, *LocalizeUnrealEd("SelectMatchingEmitter"), TEXT(""));
			}


			if(bHaveSpeedTree)
			{
				SelectMenu->Append( IDMENU_ActorPopupSelectMatchingSpeedTrees, *LocalizeUnrealEd("SelectMatchingSpeedTrees"), TEXT("") );
			}

			if( bHaveProcBuilding )
			{
				SelectMenu->Append( IDMENU_ActorPopupSelectMatchingProcBuildingsByRuleset, *LocalizeUnrealEd("SelectMatchingProcBuildingsByRuleset"), TEXT("") );
			}
		}

		if( bHaveBrush || bHaveSelectedActors )
		{
			// If we have a brush or other types of actors add a menu option for selecting all actors that use the same materials as currently selected ones.
			SelectMenu->Append( IDMENU_ActorPopupSelectAllWithMatchingMaterial, *LocalizeUnrealEd("SelectAllWithMatchingMaterial"), TEXT("") );
		}

		if( bHaveBrush )
		{
			SelectMenu->Append( IDMENU_ActorPopupSelectBrushesAdd, *LocalizeUnrealEd("AddsSolids"), TEXT("") );
			SelectMenu->Append( IDMENU_ActorPopupSelectBrushesSubtract, *LocalizeUnrealEd("Subtracts"), TEXT("") );
			SelectMenu->Append( IDMENU_ActorPopupSelectBrushesSemisolid, *LocalizeUnrealEd("SemiSolids"), TEXT("") );
			SelectMenu->Append( IDMENU_ActorPopupSelectBrushesNonsolid, *LocalizeUnrealEd("NonSolids"), TEXT("") );
		}

		SelectMenu->Append( IDMENU_ActorPopupSelectAllLights, *LocalizeUnrealEd("SelectAllLights"), TEXT("") );
		SelectMenu->Append( IDMENU_ActorPopupSelectAllRendered, *LocalizeUnrealEd("SelectAllRendered"), TEXT("") );

		SelectMenu->AppendSeparator( );

		SelectMenu->Append( IDM_SELECT_ALL, *LocalizeUnrealEd("SelectAll"), TEXT("") );
		SelectMenu->Append( IDM_SELECT_NONE, *LocalizeUnrealEd("SelectNone"), TEXT("") );
		SelectMenu->Append( IDM_SELECT_INVERT, *LocalizeUnrealEd("InvertSelections"), TEXT("") );

		SelectMenu->AppendSeparator();

		SelectMenu->Append( IDM_SELECT_POST_PROCESS_VOLUME, *LocalizeUnrealEd("SelectPostProcessVolume"), TEXT("") );

		Append( wxID_ANY, *LocalizeUnrealEd( "LevelViewportContext_SelectMenu" ), SelectMenu );

		SelectMenu->AppendSeparator();
		SelectMenu->Append( IDM_SELECT_SHOWSTATS, *LocalizeUnrealEd("SelectShowRenderStats"), TEXT("") );
	}

	// Group Menu
	if(GEditor->bGroupingActive)
	{
		// Grouping Sub Menu
		wxMenu* GroupMenu = NULL;

		// Grouping based on selection (must have selected at least two actors)
		if( NumSelectedActors > 1 )
		{
			if( !bSelectedLockedGroup && !bSelectedUnlockedGroup )
			{
				Append( IDM_CREATEGROUP, *LocalizeUnrealEd("Group_SelectionTopLevelMenu"), TEXT("") );
			}
			else
			{
				GroupMenu = new wxMenu();
				GroupMenu->Append ( IDM_CREATEGROUP, *LocalizeUnrealEd("Group_Selection"), TEXT("") );
			}
		}

		if( bSelectedLockedGroup || bSelectedUnlockedGroup )
		{
			if( !GroupMenu )
			{
				GroupMenu = new wxMenu();
			}

			const INT NumActiveGroups = AGroupActor::NumActiveGroups(TRUE);

			// Regroup will clear any existing groups and create a new one from the selection
			// Only allow regrouping if multiple groups are selected, or a group and ungrouped actors are selected
			if( NumActiveGroups > 1 || (NumActiveGroups && NumSelectedUngroupedActors) )
			{
				GroupMenu->Append ( IDM_REGROUP, *LocalizeUnrealEd("Group_Regroup"), TEXT("") );
			}
			GroupMenu->Append ( IDM_UNGROUP, *LocalizeUnrealEd("Group_Ungroup"), TEXT("") );
			if( bSelectedUnlockedGroup )
			{
				// Only allow removal of loose actors or locked subgroups
				if( !bSelectedLockedGroup || ( bSelectedLockedGroup && bSelectedSubGroup) )
				{
					GroupMenu->Append ( IDM_REMOVEFROMGROUP, *LocalizeUnrealEd("Group_Remove"), TEXT("") );
				}
				GroupMenu->Append ( IDM_LOCKGROUP, *LocalizeUnrealEd("Group_Lock"), TEXT("") );
			}
			if( bSelectedLockedGroup )
			{
				GroupMenu->Append ( IDM_UNLOCKGROUP, *LocalizeUnrealEd("Group_Unlock"), TEXT("") );
			}
			// Only allow group adds if a single group is selected in addition to ungrouped actors
			if( AGroupActor::NumActiveGroups(TRUE, FALSE) == 1 && NumSelectedUngroupedActors )
			{ 
				GroupMenu->Append ( IDM_ADDTOGROUP, *LocalizeUnrealEd("Group_Add"), TEXT("") );
			}
			// Create a proxy for the selected static meshes.
#if ENABLE_SIMPLYGON_MESH_PROXIES
			GroupMenu->Append( IDM_COMBINEGROUP, *LocalizeUnrealEd( "ActorProperties_CreateMeshProxy" ), TEXT("") );
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
		}

		if(GroupMenu)
		{
			Append( wxID_ANY, *LocalizeUnrealEd("Groups"), GroupMenu );
		}
	}

	// Edit
	{
		const UBOOL bCanCutCopy = bHaveSelectedActors;
		const UBOOL bCanPaste = TRUE;
		AppendEditMenuItems( bCanCutCopy, bCanPaste );
	}



	// Single separator for transform, pivot, snapping, etc
	AppendSeparatorIfNeeded();


	// Transform
	if( bHaveSelectedActors )
	{
		TransformMenu = new wxMenu();

		// If any selected actors have locked locations, don't present the option to align/snap to floor.
		if ( !bFoundLockedActor )
		{
			TransformMenu->Append( IDMENU_MoveToGrid, *LocalizeUnrealEd("LevelViewportContext_SnapOriginToGrid"), TEXT("") );
			if(bHaveBrush)
			{
				TransformMenu->Append( IDMENU_QuantizeVertices, *LocalizeUnrealEd("LevelViewportContext_SnapVerticesToGrid"), TEXT("") );
			}

			TransformMenu->Append( IDMENU_SnapToFloor, *LocalizeUnrealEd("SnapToFloor"), TEXT("") );
			TransformMenu->Append( IDMENU_AlignToFloor, *LocalizeUnrealEd("AlignToFloor"), TEXT("") );
			TransformMenu->Append( IDMENU_SnapPivotToFloor, *LocalizeUnrealEd("SnapPivotToFloor"), TEXT("") );
			TransformMenu->Append( IDMENU_AlignPivotToFloor, *LocalizeUnrealEd("AlignPivotToFloor"), TEXT("") );

			TransformMenu->AppendSeparator();
		}

		TransformMenu->Append( IDMENU_ActorPopupMirrorX, *LocalizeUnrealEd("MirrorXAxis"), TEXT("") );
		TransformMenu->Append( IDMENU_ActorPopupMirrorY, *LocalizeUnrealEd("MirrorYAxis"), TEXT("") );
		TransformMenu->Append( IDMENU_ActorPopupMirrorZ, *LocalizeUnrealEd("MirrorZAxis"), TEXT("") );

		TransformMenu->AppendSeparator();

		{
			wxMenuItem* NewMenuItem = TransformMenu->AppendCheckItem( IDMENU_ActorPopupLockMovement, *LocalizeUnrealEd("LevelViewportContext_LockActorMovement"), TEXT("") );
			NewMenuItem->Check( bFoundLockedActor ? true : false );
		}

		Append( ID_BackdropPopupTransformMenu, *LocalizeUnrealEd("Transform"), TransformMenu );
	}



	// Pivot
	if( bHaveSelectedActors )
	{
		PivotMenu = new wxMenu();

		PivotMenu->Append( IDMENU_ActorPopupBakePrePivot, *LocalizeUnrealEd("BakePivot"), TEXT("") );
		PivotMenu->Append( IDMENU_ActorPopupUnBakePrePivot, *LocalizeUnrealEd("UnBakePivot"), TEXT("") );
		PivotMenu->AppendSeparator();
		PivotMenu->Append( IDMENU_ActorPopupResetPivot, *LocalizeUnrealEd("Reset"), TEXT("") );
		PivotMenu->Append( ID_BackdropPopupPivot, *LocalizeUnrealEd("MoveHere"), TEXT("") );
		PivotMenu->Append( ID_BackdropPopupPivotSnapped, *LocalizeUnrealEd("MoveHereSnapped"), TEXT("") );
		PivotMenu->Append( ID_BackdropPopupPivotSnappedCenterSelection, *LocalizeUnrealEd("MoveCenterSelection"), TEXT("") );
		Append( ID_BackdropPopupPivotMenu, *LocalizeUnrealEd("Pivot"), PivotMenu );
	}


	
	// ...............



	// Brush Operations
	if(bHaveBrush)
	{
		AppendSeparatorIfNeeded();

		// Order
		{
			OrderMenu = new wxMenu();
			OrderMenu->Append( IDMENU_ActorPopupToFirst, *LocalizeUnrealEd("ToFirst"), TEXT("") );
			OrderMenu->Append( IDMENU_ActorPopupToLast, *LocalizeUnrealEd("ToLast"), TEXT("") );
			Append( IDMENU_ActorPopupOrderMenu, *LocalizeUnrealEd("Order"), OrderMenu );
		}

		// Polygons
		{
			PolygonsMenu = new wxMenu();

			// The "To Brush" and "From Builder Brush" options cannot 
			// be used if only the builder brush is selected. 
			const bool bEnableBrushTransformOptions = !bHaveBuilderBrush || (NumSelectedBrushes > 1);

			wxMenuItem* ToBrushItem = PolygonsMenu->Append( IDMENU_ActorPopupToBrush, *LocalizeUnrealEd("ToBrush"), TEXT("") );
			ToBrushItem->Enable( bEnableBrushTransformOptions );

			wxMenuItem* FromBrushItem = PolygonsMenu->Append( IDMENU_ActorPopupFromBrush, *LocalizeUnrealEd("FromBrush"), TEXT("") );
			FromBrushItem->Enable( bEnableBrushTransformOptions );

			PolygonsMenu->AppendSeparator();
			PolygonsMenu->Append( IDMENU_ActorPopupMerge, *LocalizeUnrealEd("Merge"), TEXT("") );
			PolygonsMenu->Append( IDMENU_ActorPopupSeparate, *LocalizeUnrealEd("Separate"), TEXT("") );
			Append( IDMENU_ActorPopupPolysMenu, *LocalizeUnrealEd("Polygons"), PolygonsMenu );
		}

		// CSG
		{
			CSGMenu = new wxMenu();
			CSGMenu->Append( IDMENU_ActorPopupMakeAdd, *LocalizeUnrealEd("Additive"), TEXT("") );
			CSGMenu->Append( IDMENU_ActorPopupMakeSubtract, *LocalizeUnrealEd("Subtractive"), TEXT("") );
			Append( IDMENU_ActorPopupCSGMenu, *LocalizeUnrealEd("CSG"), CSGMenu );
		}


		// Solidity
		{
			SolidityMenu = new wxMenu();
			SolidityMenu->Append( IDMENU_ActorPopupMakeSolid, *LocalizeUnrealEd("Solid"), TEXT("") );
			SolidityMenu->Append( IDMENU_ActorPopupMakeSemiSolid, *LocalizeUnrealEd("SemiSolid"), TEXT("") );
			SolidityMenu->Append( IDMENU_ActorPopupMakeNonSolid, *LocalizeUnrealEd("NonSolid"), TEXT("") );
			Append( IDMENU_ActorPopupSolidityMenu, *LocalizeUnrealEd("Solidity"), SolidityMenu );
		}
	}
	else
	{	
		OrderMenu = NULL;
		PolygonsMenu = NULL;
		CSGMenu = NULL;
	}


	AppendSeparatorIfNeeded();


	// Static Mesh: Create Blocking Volume
	if( bHaveStaticMeshComponent )
	{
		BlockingVolumeMenu = new wxMenu();

		BlockingVolumeMenu->Append( IDMENU_BlockingVolumeBBox, *LocalizeUnrealEd("BlockingVolumeBBox"), TEXT("") );
		BlockingVolumeMenu->AppendSeparator();
		BlockingVolumeMenu->Append( IDMENU_BlockingVolumeConvexVolumeHeavy, *LocalizeUnrealEd("BlockingVolumeConvexHeavy"), TEXT("") );
		BlockingVolumeMenu->Append( IDMENU_BlockingVolumeConvexVolumeNormal, *LocalizeUnrealEd("BlockingVolumeConvexNormal"), TEXT("") );
		BlockingVolumeMenu->Append( IDMENU_BlockingVolumeConvexVolumeLight, *LocalizeUnrealEd("BlockingVolumeConvexLight"), TEXT("") );
		BlockingVolumeMenu->Append( IDMENU_BlockingVolumeConvexVolumeRough, *LocalizeUnrealEd("BlockingVolumeConvexRough"), TEXT("") );
		BlockingVolumeMenu->AppendSeparator();
		BlockingVolumeMenu->Append( IDMENU_BlockingVolumeColumnX, *LocalizeUnrealEd("BlockingVolumeColumnX"), TEXT("") );
		BlockingVolumeMenu->Append( IDMENU_BlockingVolumeColumnY, *LocalizeUnrealEd("BlockingVolumeColumnY"), TEXT("") );
		BlockingVolumeMenu->Append( IDMENU_BlockingVolumeColumnZ, *LocalizeUnrealEd("BlockingVolumeColumnZ"), TEXT("") );
		BlockingVolumeMenu->AppendSeparator();
		BlockingVolumeMenu->Append( IDMENU_BlockingVolumeAutoConvex, *LocalizeUnrealEd("BlockingVolumeAutoConvex"), TEXT("") );

		Append( IDMENU_ActorPopupPolysMenu, *LocalizeUnrealEd("BlockingVolumeCreation"), BlockingVolumeMenu );
	}


	// Set Collision Type
	if( bHaveCollisionComponent )
	{
		wxMenu* CollisionMenu = new wxMenu();

		CollisionMenu->Append(IDMENU_SetCollisionBlockAll, *LocalizeUnrealEd("BlockAll"), TEXT("") );
		CollisionMenu->Append(IDMENU_SetCollisionBlockWeapons, *LocalizeUnrealEd("BlockWeapons"), TEXT("") );
		CollisionMenu->Append(IDMENU_SetCollisionBlockNone, *LocalizeUnrealEd("BlockNone"), TEXT("") );

		Append( wxID_ANY, *LocalizeUnrealEd("SetCollision"), CollisionMenu);
	}


	// Static Mesh options
	if( bHaveStaticMesh || bHaveDynamicStaticMesh )
	{
		// Set Mesh's Collision from Builder Brush
		Append( IDMENU_SaveBrushAsCollision, *LocalizeUnrealEd("LevelViewportContext_SaveBrushAsCollision"), TEXT("") );
	
		// Add a menu item for updating the base of a static mesh to a proc building
		Append( IDMENU_ActorPoupupUpdateBaseToProcBuilding, *LocalizeUnrealEd("UpdateBaseToProcBuilding"), TEXT("") );

#if WITH_SIMPLYGON
		if( NumSelectedActors == 1 )
		{
			// Check to see if we have any meshes that can be simplified
			if( bHaveNonSimplifiedStaticMesh )
			{
				Append( IDMENU_ActorPopup_SimplifyMesh, *LocalizeUnrealEd( "ActorProperties_SimplifyMesh" ), TEXT( "" ) );
			}
			else if( bHaveSimplifiedStaticMesh )
			{
				Append( IDMENU_ActorPopup_SimplifyMesh, *LocalizeUnrealEd( "ActorProperties_ResimplifyMesh" ), TEXT( "" ) );
			}
		}
		else
		{
			// Simplifies all selected static meshes.
			Append( IDMENU_ActorPopup_SimplifySelectedMeshes, *LocalizeUnrealEd( "ActorProperties_SimplifySelectedMeshes" ), TEXT( "" ) );
		}
#endif // #if WITH_SIMPLYGON
	}

#if WITH_FBX
	// Add the export option if a static mesh actor, skeletal mesh actor, interp actor or brush is selected
	if( bHaveStaticMesh || bHaveSkeletalMesh || bHaveDynamicStaticMesh || bHaveBrush )
	{
		AppendSeparatorIfNeeded();

		Append( IDMENU_ActorPoupupExportFBX, *LocalizeUnrealEd("ExportActorToFBX"), TEXT("") );
	}
#endif


	// Nav Points: Path options
	if( NumNavPoints > 0 || 
		bHaveRoute ||
		bHaveCoverGroup )
	{
		AppendSeparatorIfNeeded();
		PathMenu = new wxMenu();

		if( NumNavPoints > 0 )
		{
			PathMenu->Append( IDMENU_ActorPopupPathPosition, *LocalizeUnrealEd("AutoPosition"), TEXT("") );
			PathMenu->AppendSeparator();
		
			// if multiple path nodes are selected, add options to force/proscribe
			if( NumNavPoints > 1 )
			{
				PathMenu->Append( IDMENU_ActorPopupPathProscribe, *LocalizeUnrealEd("ProscribePaths"), TEXT("") );
				PathMenu->Append( IDMENU_ActorPopupPathForce, *LocalizeUnrealEd("ForcePaths"), TEXT("") );
				PathMenu->AppendSeparator();
			}
			PathMenu->Append( IDMENU_ActorPopupPathClearProscribed, *LocalizeUnrealEd("ClearProscribedPaths"), TEXT("") );
			PathMenu->Append( IDMENU_ActorPopupPathClearForced, *LocalizeUnrealEd("ClearForcedPaths"), TEXT("") );

			if ( NumCoverLinks > 1 )
			{
				PathMenu->AppendSeparator();
				PathMenu->Append( IDMENU_ActorPopupPathStitchCover, *LocalizeUnrealEd("StitchCover"), TEXT("") );
			}
		}

		if( bHaveRoute || bHaveCoverGroup  )
		{
			ComplexPathMenu = new wxMenu();
			if( bHaveRoute )
			{
				ComplexPathMenu->Append( IDMENU_ActorPopupPathOverwriteRoute, *LocalizeUnrealEd("OverwriteNavPointsInRoute"), TEXT("") );
				ComplexPathMenu->Append( IDMENU_ActorPopupPathAddRoute, *LocalizeUnrealEd("AddNavPointsToRoute"), TEXT("") );
				ComplexPathMenu->Append( IDMENU_ActorPopupPathRemoveRoute, *LocalizeUnrealEd("RemoveNavPointsFromRoute"), TEXT("") );
				ComplexPathMenu->Append( IDMENU_ActorPopupPathClearRoute, *LocalizeUnrealEd("ClearNavPointsFromRoute"), TEXT("") );
				ComplexPathMenu->Append( IDMENU_ActorPopupPathSelectRoute, *LocalizeUnrealEd("SelectNavPointsFromRoute"), TEXT("") );				
			}
			if( bHaveRoute && bHaveCoverGroup )
			{
				ComplexPathMenu->AppendSeparator();
			}
			if( bHaveCoverGroup )
			{
				ComplexPathMenu->Append( IDMENU_ActorPopupPathOverwriteCoverGroup, *LocalizeUnrealEd("OverwriteLinksInCoverGroup"), TEXT("") );
				ComplexPathMenu->Append( IDMENU_ActorPopupPathAddCoverGroup, *LocalizeUnrealEd("AddLinksToCoverGroup"), TEXT("") );
				ComplexPathMenu->Append( IDMENU_ActorPopupPathRemoveCoverGroup, *LocalizeUnrealEd("RemoveLinksFromCoverGroup"), TEXT("") );
				ComplexPathMenu->Append( IDMENU_ActorPopupPathClearCoverGroup, *LocalizeUnrealEd("ClearLinksFromCoverGroup"), TEXT("") );
			}

			if( NumNavPoints > 0 )
			{
				PathMenu->AppendSeparator();
			}
			PathMenu->Append( IDMENU_ActorPopupComplexPathMenu, *LocalizeUnrealEd("ComplexPathOptions"), ComplexPathMenu );
		}
		else
		{
			ComplexPathMenu = NULL;
		}

		Append( IDMENU_ActorPopupPathMenu, *LocalizeUnrealEd("PathOptions"), PathMenu );
	}
	else
	{
		PathMenu = NULL;
	}


	// Crowd destinations: Link/unlink
	if (NumCrowdDestinations > 1)
	{
		AppendSeparatorIfNeeded();
		Append( IDMENU_ActorPopupLinkCrowdDestinations, *LocalizeUnrealEd("LinkCrowdDestinations"), TEXT("") );
		Append( IDMENU_ActorPopupUnlinkCrowdDestinations, *LocalizeUnrealEd("UnlinkCrowdDestinations"), TEXT("") );
	}


	// ProcBuildings
	if(bHaveProcBuilding)
	{
		wxMenu* ProcBuildingMenu = new wxMenu();
		
		// Add named variation options
		if( GEditorModeTools().IsModeActive( EM_Geometry ) )
		{
			GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded ); // Make sure any selected rulesets are loaded
			
			// Add named variation options
			UProcBuildingRuleset* Ruleset = GUnrealEd->GetGeomEditedBuildingRuleset();
			if(Ruleset)
			{
				ProcBuildingMenu->Append(IDMENU_ApplyRulesetVariationToFace_START+0, TEXT("Variation: Default"), TEXT(""));

				for(INT VarIdx=0; VarIdx<Ruleset->Variations.Num(); VarIdx++)
				{
					FName VariationName = Ruleset->Variations(VarIdx).VariationName;
					FString VariationString = FString::Printf( TEXT("Variation: %s"), *VariationName.ToString() );
					ProcBuildingMenu->Append(IDMENU_ApplyRulesetVariationToFace_START+1+VarIdx, *VariationString, TEXT(""));
				}

				ProcBuildingMenu->AppendSeparator();
			}
			
			// Add material (for roofs)
			UMaterialInterface* Material = GEditor->GetSelectedObjects()->GetTop<UMaterialInterface>();
			if(Material)
			{
				FString MaterialString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("ApplyMaterialToPBFaceF"), *Material->GetName()) );
				ProcBuildingMenu->Append(IDMENU_ApplySelectedMaterialToPBFace, *MaterialString, TEXT(""));

				ProcBuildingMenu->AppendSeparator();
			}			
		}

		// Add parameter swatch options to menu, if some are set in the ruleset
		ProcBuildingMenu->Append(IDMENU_ChoosePBSwatch_START+0, TEXT("Swatch: None"), TEXT(""));

		UProcBuildingRuleset* Ruleset = GUnrealEd->GetSelectedBuildingRuleset();
		if(Ruleset && Ruleset->ParamSwatches.Num() > 0)
		{
			for(INT SwatchIdx=0; SwatchIdx<Ruleset->ParamSwatches.Num(); SwatchIdx++)
			{
				FName SwatchName = Ruleset->ParamSwatches(SwatchIdx).SwatchName;
				FString SwatchString = FString::Printf( TEXT("Swatch: %s"), *SwatchName.ToString() );
				ProcBuildingMenu->Append(IDMENU_ChoosePBSwatch_START+1+SwatchIdx, *SwatchString, TEXT(""));
			}
		}

		ProcBuildingMenu->AppendSeparator();


		// Other options
		ProcBuildingMenu->Append(IDMENU_ClearFaceRulesetVariations, *LocalizeUnrealEd("ClearFaceVariationAssignments"), TEXT(""));
		ProcBuildingMenu->Append(IDMENU_ClearPBFaceMaterials, *LocalizeUnrealEd("ClearPBMaterialFaceAssignments"), TEXT(""));
		ProcBuildingMenu->Append(IDMENU_SelectBaseBuilding, *LocalizeUnrealEd("SelectBaseBuilding"), TEXT(""));
		ProcBuildingMenu->Append(IDMENU_GroupSelectedBuildings, *LocalizeUnrealEd("GroupBuildings"), TEXT(""));
		
		ProcBuildingMenu->Append(IDMENU_ProcBuildingResourceInfo, *LocalizeUnrealEd("ProcBuildingResourceInfo"), TEXT(""));
		
		Append( IDMENU_ProcBuildingMenu, *LocalizeUnrealEd("ProcBuildingOptions"), ProcBuildingMenu );
	}

	if (bHaveImageReflectionSceneCapture)
	{
		AppendSeparatorIfNeeded();
		Append(IDMENU_ImageReflectionSceneCapture, *LocalizeUnrealEd("UpdateSceneCapture"), TEXT(""));
	}

	// Lights
	if( bHaveLight )
	{
		AppendSeparatorIfNeeded();

		Append( IDMENU_ActorPopupToggleDynamicChannel, *LocalizeUnrealEd("ToggleDynamicChannel"), TEXT("") );
		
		wxMenu* LightConvertMenu = new wxMenu();
		LightConvertMenu->Append( IDMENU_IDMENU_ActorPopupConvertLightToLightDynamicAffecting, *LocalizeUnrealEd("ConvertLightToLightDynamicAffecting"), TEXT("") );
		LightConvertMenu->Append( IDMENU_IDMENU_ActorPopupConvertLightToLightStaticAffecting, *LocalizeUnrealEd("ConvertLightToLightStaticAffecting"), TEXT("") );
		LightConvertMenu->Append( IDMENU_IDMENU_ActorPopupConvertLightToLightDynamicAndStaticAffecting, *LocalizeUnrealEd("ConvertLightToLightDynamicAndStaticAffecting"), TEXT("") );
		Append( wxID_ANY, *LocalizeUnrealEd("LightAffectsClassificationMenuEntry"), LightConvertMenu);

		// Convert lights menu
		wxMenu* ConvertLightsMenu = new wxMenu();

		// Mapping of UClass* types to their wxMenu;
		TMap< UClass*, wxMenu* > ParentMenus;

		// Support conversion to an from these types of lights and their child classes
		TArray< UClass* > SupportedLightClasses;
		SupportedLightClasses.AddItem( APointLight::StaticClass() );
		SupportedLightClasses.AddItem( ASpotLight::StaticClass() );
		SupportedLightClasses.AddItem( ADirectionalLight::StaticClass() );
		SupportedLightClasses.AddItem( ASkyLight::StaticClass() );

		// Go through each supported light class and add a parent menu under the convert lights menu. 
		// This is so we can place child class types in sections separated by parent type
		for( INT SupportedIdx = 0; SupportedIdx < SupportedLightClasses.Num(); ++SupportedIdx )
		{
			wxMenu* ParentMenu = new wxMenu;
			// Build a friendly menu name. Will be in the form of "LightClassName" + "s".  All child classes of this type will have a menu entry here
			const FString MenuName = SupportedLightClasses(SupportedIdx)->GetName()+TEXT("s");
			ConvertLightsMenu->Append( IDMENU_ConvertLightsMenu, *MenuName, ParentMenu );
			// Set the map so that child classes know which parent menu they should be appended to
			ParentMenus.Set( SupportedLightClasses(SupportedIdx), ParentMenu );
		}

		INT ClassID = IDMENU_ConvertLights_START;
		for( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
		{
			UClass* CurClass = *ClassIt;

			// Skip non-placables and non lights.
			if( (CurClass->ClassFlags & CLASS_Placeable) && CurClass->IsChildOf( ALight::StaticClass() ) )
			{			
				// Check to make sure this type of light is supported
				UBOOL bLightClassSupported = FALSE;
				for( INT SupportedIdx = 0; SupportedIdx < SupportedLightClasses.Num(); ++SupportedIdx )
				{
					if( CurClass->IsChildOf( SupportedLightClasses( SupportedIdx) ) )
					{
						bLightClassSupported = TRUE;
						break;
					}
				}

				
				if( bLightClassSupported )
				{
					// If the light class is supported add a menu item for it.
					UClass* ParentClass = CurClass;
					while( !ParentMenus.HasKey( ParentClass ) )
					{
						ParentClass = ParentClass->GetSuperClass();
						check( ParentClass != NULL );
					}
					wxMenu* Menu = *ParentMenus.Find( ParentClass );
					Menu->Append( ClassID, *CurClass->GetName(), TEXT("") );

					// Increment the ID of this class, so we get a unique event id for each menu item
					// This is needed during conversion to determine what menu item the user clicked.
					++ClassID;
				}
			}
		}

		Append( IDMENU_ConvertLightsMenu, *LocalizeUnrealEd("ConvertLights"), ConvertLightsMenu );

	}



	// Prefab
	if(bHavePrefabInstance)
	{
		APrefabInstance* PrefInst = GEditor->GetSelectedActors()->GetTop<APrefabInstance>();
		check(PrefInst);

		Append( IDM_UPDATEPREFABFROMINSTANCE, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("Prefab_UpdateFromInstance"), *PrefInst->TemplatePrefab->GetName())), TEXT("") );
		Append( IDM_RESETFROMPREFAB, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("Prefab_ResetFromPrefab"), *PrefInst->TemplatePrefab->GetName())), TEXT("") );
		Append( IDM_CONVERTPREFABTONORMALACTORS, *LocalizeUnrealEd("Prefab_ToNormalActors"), TEXT("") );

		if(PrefInst->SequenceInstance)
		{
			Append( IDM_OPENPREFABINSTANCESEQUENCE, *LocalizeUnrealEd("Prefab_OpenInstanceSequence"), TEXT("") );
		}
	}


	// Emitter
	if (bHaveEmitter)
	{
		AppendSeparatorIfNeeded();

		wxMenu* EmitterOptionsMenu = new wxMenu();

		EmitterOptionsMenu->Append(IDMENU_EmitterPopupOptionsAutoPopulate, *LocalizeUnrealEd("EmitterAutoPopulate"), TEXT(""));
		EmitterOptionsMenu->Append(IDMENU_EmitterPopupOptionsReset, *LocalizeUnrealEd("EmitterReset"), TEXT(""));
		Append(IDMENU_EmitterPopupOptionsMenu, *LocalizeUnrealEd("EmitterPopupMenu"), EmitterOptionsMenu);
	}

	// Spline
	if(bHaveSpline)
	{
		AppendSeparatorIfNeeded();

		wxMenu* SplineOptionsMenu = new wxMenu();
		
		SplineOptionsMenu->Append(IDMENU_SplinePopupConnect, *LocalizeUnrealEd("SplineConnect"), TEXT(""));
		SplineOptionsMenu->Append(IDMENU_SplinePopupBreak, *LocalizeUnrealEd("SplineBreak"), TEXT(""));
		SplineOptionsMenu->Append(IDMENU_SplinePopupBreakAllLinks, *LocalizeUnrealEd("SplineBreakAllLinks"), TEXT(""));
		SplineOptionsMenu->Append(IDMENU_SplinePopupReverseAllDirections, *LocalizeUnrealEd("SplineReverseAllDirections"), TEXT(""));
		SplineOptionsMenu->Append(IDMENU_SplinePopupSetStraightTangents, *LocalizeUnrealEd("SplineSetStraightTangents"), TEXT(""));
		SplineOptionsMenu->AppendSeparator();
		SplineOptionsMenu->Append(IDMENU_SplinePopupSelectAllNodes, *LocalizeUnrealEd("SplineSelectAllNode"), TEXT(""));
		SplineOptionsMenu->AppendSeparator();
		SplineOptionsMenu->Append(IDMENU_SplinePopupTestRoute, *LocalizeUnrealEd("SplineTestRoute"), TEXT(""));
		
		Append(IDMENU_SplinePopupOptionsMenu, *LocalizeUnrealEd("SplinePopupMenu"), SplineOptionsMenu);
	}



	// ............




	// Level streaming/grid volume actions
	{
		AppendSeparatorIfNeeded();
		if( FirstSelectedLevelGridVolume != NULL )
		{
			if( FirstSelectedLevelGridVolume != GWorld->CurrentLevelGridVolume )
			{
				Append( IDMENU_MakeLevelGridVolumeCurrent, *LocalizeUnrealEd( "LevelViewportContext_MakeLevelGridVolumeCurrent" ), TEXT("") );
			}
		}
		if( GWorld->CurrentLevelGridVolume != NULL )
		{
			Append( IDMENU_ClearCurrentLevelGridVolume, *LocalizeUnrealEd( "LevelViewportContext_ClearCurrentLevelGridVolume" ), TEXT("") );
		}

		if( FirstSelectedLevelGridVolume != NULL || FirstSelectedLevelStreamingVolume != NULL )
		{
			Append( IDM_LevelViewportContext_FindStreamingVolumeLevelsInLevelBrowser, *LocalizeUnrealEd( "LevelViewportContext_FindStreamingVolumeLevelsInLevelBrowser" ), TEXT( "LevelViewportContext_FindStreamingVolumeLevelsInLevelBrowser_Help" ), 0 );
		}
	}


	AppendSeparatorIfNeeded();


	// Actor visibility
	{
		AppendActorVisibilityMenuItems( bHaveSelectedActors );
	}

	// Materials
	if( NumSelectedActors == 1 )
	{
		AppendMaterialsAndTexturesMenu(FirstActor);
	}
	else if( NumSelectedActors > 1 )
	{
		AppendMaterialsMultipleSelectedMenu();
	}


	// Level operations
	if( bHaveSelectedActors )
	{
		wxMenu* LevelMenu = new wxMenu();

		// "Make actor level current"
		{
			// If all selected actors belong to the same level, offer the option to make that level current.
			FString MakeCurrentLevelString;
			if ( bSelectedActorsBelongToSameLevel )
			{
				check( SharedLevel );
				UPackage* LevelPackage = CastChecked<UPackage>( SharedLevel->GetOutermost() );
				MakeCurrentLevelString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("MakeActorLevelCurrentF"), *LevelPackage->GetName()) );
			}
			else
			{
				MakeCurrentLevelString = LocalizeUnrealEd("MakeActorLevelCurrentMultiple");
			}

			// NOTE: Enable/disable state for this menu item is handled by UI_ContextMenuMakeCurrentLevel()
			LevelMenu->Append( ID_MakeSelectedActorsLevelCurrent, *MakeCurrentLevelString, TEXT("") );
		}

		// "Make actor level grid volume current"
		{
			FString MakeCurrentLevelString;
			if( bSelectedActorsBelongToSameLevelGridVolume )
			{
				check( SharedLevelGridVolume != NULL );
				MakeCurrentLevelString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("MakeActorLevelGridVolumeCurrentF"), *SharedLevelGridVolume->GetLevelGridVolumeName() ) );
			}
			else
			{
				MakeCurrentLevelString = LocalizeUnrealEd("MakeActorLevelGridVolumeCurrentMultiple");
			}

			// NOTE: Enable/disable state for this menu item is handled by UI_ContextMenuMakeCurrentLevelGridVolume()
			LevelMenu->Append( ID_MakeSelectedActorsLevelGridVolumeCurrent, *MakeCurrentLevelString, TEXT("") );
		}

		LevelMenu->AppendSeparator();

		// If a grid volume is 'current' then we'll display an option to move actors to that instead of
		// to the current level
		if( GWorld->CurrentLevelGridVolume != NULL )
		{
			wxMenuItem* NewItem = LevelMenu->Append( ID_MoveSelectedActorsToCurrentLevel, *LocalizeUnrealEd("MoveSelectedActorsToCurrentLevelGridVolume"), TEXT("") );
			NewItem->Enable( !bAllSelectedActorsBelongToCurrentLevelGridVolume );
		}
		else
		{
			wxMenuItem* NewItem = LevelMenu->Append( ID_MoveSelectedActorsToCurrentLevel, *LocalizeUnrealEd("MoveSelectedActorsToCurrentLevel"), TEXT("") );
			NewItem->Enable( !bAllSelectedActorsBelongToCurrentLevel );
		}

		LevelMenu->AppendSeparator();

		LevelMenu->Append( ID_SelectLevelOnlyInLevelBrowser, *LocalizeUnrealEd("SelectLevelsOnly"), TEXT(""), 0 );
		LevelMenu->Append( ID_SelectLevelInLevelBrowser, *LocalizeUnrealEd("SelectLevels"), TEXT(""), 0 );
		LevelMenu->Append( ID_DeselectLevelInLevelBrowser, *LocalizeUnrealEd("DeselectLevels"), TEXT(""), 0 );

		Append( wxID_ANY, *LocalizeUnrealEd( "LevelViewportContext_LevelMenu" ), LevelMenu );
	}



	// LOD operations
	if(NumSelectedActors > 0)
	{
		wxMenu* LODMenu = new wxMenu();

		// Set Detail Mode
		{
			DetailModeMenu = new wxMenu();

			DetailModeMenu->Append( IDMENU_ActorPopupDetailModeLow, *LocalizeUnrealEd("Low"), TEXT("") );
			DetailModeMenu->Append( IDMENU_ActorPopupDetailModeMedium, *LocalizeUnrealEd("Medium"), TEXT("") );
			DetailModeMenu->Append( IDMENU_ActorPopupDetailModeHigh, *LocalizeUnrealEd("High"), TEXT("") );
			LODMenu->Append( ID_BackdropPopupDetailModeMenu, *LocalizeUnrealEd("SetDetailMode"), DetailModeMenu );

			LODMenu->AppendSeparator();
		}


		if( NumSelectedActors == 1 )
		{
			LODMenu->Append( IDMENU_ActorPopup_SetLODParent, *LocalizeUnrealEd("ActorProperties_SetLODParent"), TEXT("") );
		}
		
		// if one has been selected, start using it
		if (GUnrealEd->CurrentLODParentActor)
		{
			LODMenu->Append( IDMENU_ActorPopup_AddToLODParent, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("ActorProperties_AddToLODParent"), *GUnrealEd->CurrentLODParentActor->GetName()) ), TEXT("") );
		}

		LODMenu->Append( IDMENU_ActorPopup_RemoveFromLODParent, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("ActorProperties_RemoveFromLODParent"), *GUnrealEd->CurrentLODParentActor->GetName()) ), TEXT("") );

		Append(IDMenu_ActorPopup_LODMenu, *LocalizeUnrealEd("ActorProperties_LODSubMenu"), LODMenu);
	}




	// Builder Brush: Add Volume
	if( bHaveBuilderBrush )
	{
		TArray< UClass* > VolumeClasses;

		GApp->EditorFrame->GetSortedVolumeClasses( &VolumeClasses );

		// Create the actual menu by looping through our sorted array and adding the menu items
		VolumeMenu = new wxMenu();

		INT ID = IDM_VolumeClasses_START;

		for( INT VolumeIdx = 0; VolumeIdx < VolumeClasses.Num(); VolumeIdx++ )
		{
			VolumeMenu->Insert( 0, ID, *VolumeClasses( VolumeIdx )->GetName(), TEXT(""), 0 );

			ID++;
		}

		Append( IDMENU_ActorPopupVolumeMenu, *LocalizeUnrealEd("AddVolumePopup"), VolumeMenu );
	}




	// Add Actor
	{
		AppendAddActorMenu();
	}

	// Convert

	if( bHaveBrush || bHaveKActor || bHaveStaticMesh || bHaveMover || bHaveFracturedStaticMesh )
	{
		wxMenu* ConvertMenu = new wxMenu();

		if(bHaveBrush)
		{
 			ConvertMenu->Append( IDMENU_ConvertToStaticMesh, *LocalizeUnrealEd("ConvertStaticMesh"), TEXT("") );
			if( !bHaveBuilderBrush )
			{
				ConvertMenu->Append( IDMENU_ConvertToBlockingVolume, *LocalizeUnrealEd("ConvertToBlockingVolume"), TEXT("") );
				ConvertMenu->Append( IDMENU_ConvertToProcBuilding, *LocalizeUnrealEd("ConvertProcBuilding"), TEXT("") );
				AppendConvertVolumeSubMenu( ConvertMenu );
			}
		}

		if(bHaveKActor)
		{
			ConvertMenu->Append(IDMENU_ActorPopupConvertKActorToStaticMesh, *LocalizeUnrealEd("ConvertKActorToStaticMesh"), TEXT("") );
			ConvertMenu->Append(IDMENU_ActorPopupConvertKActorToMover, *LocalizeUnrealEd("ConvertKActorToMover"), TEXT("") );
		}

		if(bHaveStaticMesh)
		{
			if ( bAllSelectedStaticMeshesHaveCollisionModels )
			{
				ConvertMenu->Append(IDMENU_ActorPopupConvertStaticMeshToKActor, *LocalizeUnrealEd("ConvertStaticMeshToKActor"), TEXT("") );
			}
			ConvertMenu->Append(IDMENU_ActorPopupConvertStaticMeshToMover, *LocalizeUnrealEd("ConvertStaticMeshToMover"), TEXT("") );
			ConvertMenu->Append(IDMENU_ActorPopupConvertStaticMeshToSMBasedOnExtremeContent, *LocalizeUnrealEd("ConvertStaticMeshToStaticMeshBasedOnExtremeContent"), TEXT("") );
			
			ConvertMenu->Append(IDMENU_ActorPopupConvertStaticMeshToInteractiveFoliageMesh, *LocalizeUnrealEd("ConvertStaticMeshToFoliage"), TEXT("") );

			// NOTE: It's too slow to check to see if the selected mesh can actually be converted to a FSMA here, so we
			// always add the menu item and we'll do a proper check when the user selects this command.
			ConvertMenu->Append(IDMENU_ActorPopupConvertStaticMeshToFSMA, *LocalizeUnrealEd("ConvertStaticMeshToFSMA"), TEXT("") );

			ConvertMenu->Append(IDMENU_ActorPopup_ConvertStaticMeshToNavMesh, *LocalizeUnrealEd("ConvertStaticMeshToNavMesh"), TEXT("") );
		}

		if (bHaveInteractiveFoliageMesh)
		{
			ConvertMenu->Append(IDMENU_ActorPopupConvertInteractiveFoliageMeshToStaticMesh, *LocalizeUnrealEd("ConvertFoliageToStaticMesh"), TEXT("") );
		}

		if(bHaveMover)
		{
			ConvertMenu->Append(IDMENU_ActorPopupConvertMoverToStaticMesh, *LocalizeUnrealEd("ConvertMoverToStaticMesh"), TEXT("") );
			ConvertMenu->Append(IDMENU_ActorPopupConvertMoverToKActor, *LocalizeUnrealEd("ConvertMoverToKActor"), TEXT("") );
		}

		if(bHaveFracturedStaticMesh)
		{
			ConvertMenu->Append(IDMENU_ActorPopupConvertFSMAToStaticMesh, *LocalizeUnrealEd("ConvertFSMAToStaticMesh"), TEXT("") );
		}

		Append( IDMENU_ActorPopupConvertMenu, *LocalizeUnrealEd("ConvertMenu"), ConvertMenu);
	}



	// Create From...
	if(NumSelectedActors > 0 && !bHaveBuilderBrush)
	{
		// Create Archetype
		if(NumSelectedActors == 1)
		{
			Append( IDM_CREATEARCHETYPE, *LocalizeUnrealEd("Archetype_Create"), TEXT("") );
			// Update existing archetype
			if(FirstActor->GetArchetype() != FirstActor->GetClass()->GetDefaultActor())
			{
				Append( IDM_UPDATEARCHETYPE, *LocalizeUnrealEd("Archetype_Update"), TEXT("") );
			}
		}

		// Create Prefab
		Append( IDM_CREATEPREFAB, *LocalizeUnrealEd("Prefab_Create"), TEXT("") );
	}		


	// Play level
	{
		AppendPlayLevelMenu();
	}
}

/** 
 * Adds entries to the material submenu for different actions that can be done
 * @param Materials - Array of materials whose actions need menus
 * @param ParentMenu - The parent menu to append new menus to
 * @param ComponentIndex - Index into the displayable components of the selected actor, this will be used to find the component again in the event handlers.
 */
static void AddMaterialInterfaceMenu(const TArray<UMaterialInterface*>& Materials, wxMenu* ParentMenu, INT ComponentIndex, UBOOL bAllowAssigning)
{
	check(ComponentIndex >= 0 && ComponentIndex < MATERIAL_MENU_NUM_COMPONENT_ENTRIES);
	check(Materials.Num() > 0);
	check(ParentMenu);

	const UBOOL bOnlyOneMaterial = Materials.Num() == 1;
	// Limit the number of material menus added to the number we have allocated Id's for
	for (INT MaterialIdx = 0; MaterialIdx < Materials.Num() && MaterialIdx < MATERIAL_MENU_NUM_MATERIAL_ENTRIES; MaterialIdx++)
	{
		// Encode the component index and material index in the menu Id
		const INT IdOffset = ComponentIndex * MATERIAL_MENU_NUM_MATERIAL_ENTRIES + MaterialIdx;
		UMaterialInterface* MaterialInterface = Materials(MaterialIdx);
		const UBOOL bIsMaterialInstance = MaterialInterface && MaterialInterface->IsA(UMaterialInstance::StaticClass());

		wxMenu* MaterialMenu = NULL;
		if (bOnlyOneMaterial)
		{
			// Collapse this material into ParentMenu if there is only one entry
			MaterialMenu = ParentMenu;
			wxMenuItem* MaterialName = MaterialMenu->Append(wxID_ANY, *MaterialInterface->GetName(), TEXT(""));
			MaterialName->Enable(false);
			MaterialMenu->AppendSeparator();
		}
		else
		{
			// Create a nested menu as there are multiple materials
			MaterialMenu = new wxMenu;
		}

		if (bIsMaterialInstance)
		{
			// Expose syncing to the material instance and the base material for material instances
			MaterialMenu->Append(IDM_SYNC_GENERICBROWSER_TO_MATERIALINTERFACE_START + IdOffset, *LocalizeUnrealEd("LevelViewportContext_SyncMaterialInstance"), TEXT(""));
			MaterialMenu->Append(IDM_SYNC_GENERICBROWSER_TO_BASEMATERIAL_START + IdOffset, *LocalizeUnrealEd("LevelViewportContext_SyncBaseMaterial"), TEXT(""));
		}
		else if (MaterialInterface)
		{
			MaterialMenu->Append(IDM_SYNC_GENERICBROWSER_TO_MATERIALINTERFACE_START + IdOffset, *LocalizeUnrealEd("LevelViewportContext_SyncMaterial"), TEXT(""));
		}
		
		if (bAllowAssigning)
		{
			wxMenuItem* AssignOption = MaterialMenu->Append(IDM_ASSIGN_MATERIALINTERFACE_START + IdOffset, *LocalizeUnrealEd("LevelViewportContext_AssignMaterial"), TEXT(""));
			AssignOption->Enable(false);

			// @todo CB: Needs support for unloaded assets (add a fast AnyAssetsOfClassSelected() method?)
			USelection* MaterialSelection = GEditor->GetSelectedObjects();
			if (MaterialSelection && MaterialSelection->Num() == 1)
			{
				UMaterialInterface* MaterialToAssign = MaterialSelection->GetTop<UMaterialInterface>();
				if (MaterialToAssign)
				{
					// Enable the assign option if the user has selected a UMaterialInterface in the Generic Browser
					AssignOption->Enable(true);
				}
			}
		}
	
		if (MaterialInterface)
		{
			// Only allow these actions if the material exists
			MaterialMenu->Append(IDM_EDIT_MATERIALINTERFACE_START + IdOffset, *LocalizeUnrealEd("LevelViewportContext_EditMaterialInterface"), TEXT(""));
			if (bAllowAssigning)
			{
				MaterialMenu->Append(IDM_CREATE_MATERIAL_INSTANCE_CONSTANT_START + IdOffset, *LocalizeUnrealEd("CreateMaterialInstanceConstant"), TEXT(""));
				MaterialMenu->Append(IDM_CREATE_MATERIAL_INSTANCE_TIME_VARYING_START + IdOffset, *LocalizeUnrealEd("CreateMaterialInstanceTimeVarying"), TEXT(""));
			}
			MaterialMenu->Append(IDM_COPY_MATERIAL_NAME_TO_CLIPBOARD_START + IdOffset, *LocalizeUnrealEd("LevelViewportContext_CopyMaterialPath"), TEXT(""));

			// Add a menu of textures in the current material so users can sync the content browser to them.
			TArray<UTexture*> Textures;
			MaterialInterface->GetUsedTextures( Textures );
			if( Textures.Num() > 0 )
			{
				wxMenu* TexturesMenu = new wxMenu();
				for( INT TextureIndex = 0; TextureIndex < Textures.Num(); ++TextureIndex )
				{
					INT Offset = (ComponentIndex * MATERIAL_MENU_NUM_MATERIAL_ENTRIES + MaterialIdx) * MATERIAL_MENU_NUM_TEXTURE_ENTRIES + TextureIndex;
					TexturesMenu->Append( IDM_SYNC_TO_TEXTURE_START + Offset, *Textures(TextureIndex)->GetName() );
				}
				MaterialMenu->AppendSeparator();
				MaterialMenu->Append( wxID_ANY, *LocalizeUnrealEd("LevelViewportContext_SyncTexture"), TexturesMenu );
			}
		}

		if (!bOnlyOneMaterial)
		{
			// Add the per-material menu to the parent menu
			ParentMenu->Append( wxID_ANY, *MaterialInterface->GetName(), MaterialMenu );
		}
	}
}

/** Adds the Material sub menu */
void WxMainContextMenu::AppendMaterialsAndTexturesMenu(AActor* FirstActor)
{
	if(FirstActor != NULL)
	{
		TArray<UActorComponent*> DisplayableComponents;
		for (INT ComponentIdx = 0; ComponentIdx < FirstActor->Components.Num(); ComponentIdx++)
		{
			UActorComponent* CurrentComponent = FirstActor->Components(ComponentIdx);
			// Search for components with editable material references that should be displayed
			// @Note - these criteria must match up with the event handler criteria, 
			// As an index into DisplayableComponents is used to figure out which component to operate on in the event handlers.
			if (CurrentComponent &&
				(CurrentComponent->IsA(UFogVolumeDensityComponent::StaticClass())
				|| CurrentComponent->IsA(UFluidSurfaceComponent::StaticClass())
				|| CurrentComponent->IsA(UMeshComponent::StaticClass()) && Cast<UMeshComponent>(CurrentComponent)->GetNumElements() > 0
				|| CurrentComponent->IsA(UParticleSystemComponent::StaticClass()) && Cast<UParticleSystemComponent>(CurrentComponent)->Template
				|| CurrentComponent->IsA(USpeedTreeComponent::StaticClass()) && Cast<USpeedTreeComponent>(CurrentComponent)->SpeedTree))
			{
				DisplayableComponents.AddUniqueItem(CurrentComponent);
			}
		}

		if (DisplayableComponents.Num() > 0)
		{
			wxMenu* MaterialAccessMenu = new wxMenu;

			// Limit the number of component menus added to the number we have allocated Id's for
			for (INT ComponentIndex = 0; ComponentIndex < DisplayableComponents.Num() && ComponentIndex < MATERIAL_MENU_NUM_COMPONENT_ENTRIES; ComponentIndex++)
			{
				wxMenu* MeshMenu = NULL;
				// Inline the component menu if there is only one displayable component
				if (DisplayableComponents.Num() == 1)
				{
					MeshMenu = MaterialAccessMenu;
				}
				else
				{
					MeshMenu = new wxMenu;
				}

				UActorComponent* CurrentComponent = DisplayableComponents(ComponentIndex);
				UFogVolumeDensityComponent* FogVolumeComponent = Cast<UFogVolumeDensityComponent>(CurrentComponent);
				UFluidSurfaceComponent* FluidSurfaceComponent = Cast<UFluidSurfaceComponent>(CurrentComponent);
				UMeshComponent* MeshComponent = Cast<UMeshComponent>(CurrentComponent);
				UParticleSystemComponent* ParticleComponent = Cast<UParticleSystemComponent>(CurrentComponent);
				USpeedTreeComponent* SpeedTreeComponent = Cast<USpeedTreeComponent>(CurrentComponent);

				check(FogVolumeComponent || FluidSurfaceComponent || MeshComponent || ParticleComponent || SpeedTreeComponent);

				if (FogVolumeComponent)
				{
					TArray<UMaterialInterface*> MaterialsToAdd;
					MaterialsToAdd.AddItem(FogVolumeComponent->FogMaterial);
					AddMaterialInterfaceMenu(MaterialsToAdd, MeshMenu, ComponentIndex, TRUE);
				}
				else if (FluidSurfaceComponent)
				{
					TArray<UMaterialInterface*> MaterialsToAdd;
					MaterialsToAdd.AddItem(FluidSurfaceComponent->FluidMaterial);
					AddMaterialInterfaceMenu(MaterialsToAdd, MeshMenu, ComponentIndex, TRUE);
				}
				else if (MeshComponent)
				{
					check(MeshComponent->GetNumElements() > 0);
					TArray<UMaterialInterface*> MaterialsToAdd;
					for (INT MaterialIdx=0; MaterialIdx < MeshComponent->GetNumElements(); MaterialIdx++)
					{
						MaterialsToAdd.AddItem(MeshComponent->GetMaterial(MaterialIdx));
					}
					AddMaterialInterfaceMenu(MaterialsToAdd, MeshMenu, ComponentIndex, TRUE);
				}
				else if (ParticleComponent)
				{
					check(ParticleComponent->Template);
					TArray<UMaterialInterface*> MaterialsToAdd;
					for (INT EmitterIndex=0; EmitterIndex < ParticleComponent->Template->Emitters.Num(); EmitterIndex++)
					{
						const UParticleEmitter* CurrentEmitter = ParticleComponent->Template->Emitters(EmitterIndex);
						if (CurrentEmitter && CurrentEmitter->LODLevels.Num() > 0 && CurrentEmitter->LODLevels(0)->RequiredModule)
						{
							MaterialsToAdd.AddItem(CurrentEmitter->LODLevels(0)->RequiredModule->Material);
						}
					}
					// Don't allow assigning to particle components as they have no per-component override
					AddMaterialInterfaceMenu(MaterialsToAdd, MeshMenu, ComponentIndex, FALSE);
				}
				else if (SpeedTreeComponent)
				{
					check(SpeedTreeComponent->SpeedTree);
					TArray<UMaterialInterface*> MaterialsToAdd;
					for (BYTE MeshType = STMT_MinMinusOne + 1; MeshType < STMT_Max; MeshType++)
					{
						MaterialsToAdd.AddItem(SpeedTreeComponent->GetMaterial(MeshType));
					}
					AddMaterialInterfaceMenu(MaterialsToAdd, MeshMenu, ComponentIndex, TRUE);
				}

				if (MaterialAccessMenu != MeshMenu)
				{
					// Add the component sub menu
					MaterialAccessMenu->Append(wxID_ANY, *CurrentComponent->GetName(), MeshMenu);
				}
			}

			// Add the main Material sub menu to the right click menu.
			Append( wxID_ANY, *LocalizeUnrealEd("MaterialsSubMenu"), MaterialAccessMenu );	
		}
	}
}

/*
 * Append the options for the materials context option when multiple actors are selected.
 *
 * If a material is selected in the content browser, enable the option for assigning this to each actor.
 *
 */
void WxMainContextMenu::AppendMaterialsMultipleSelectedMenu()
{
	wxMenu* MaterialAccessMenu = new wxMenu;

	wxMenuItem* AssignOption = MaterialAccessMenu->Append(IDM_ASSIGN_MATERIALINTERFACE_MULTIPLEACTORS, *LocalizeUnrealEd("LevelViewportContext_AssignMaterialMultiple"), TEXT(""));
	AssignOption->Enable(false);

	// @todo CB: Needs support for unloaded assets (add a fast AnyAssetsOfClassSelected() method?)
	USelection* MaterialSelection = GEditor->GetSelectedObjects();
	if ( MaterialSelection )
	{
		UMaterialInterface* MaterialToAssign = MaterialSelection->GetTop<UMaterialInterface>();
		if (MaterialToAssign)
		{
			// Enable the assign option if the user has selected a UMaterialInterface in the Generic Browser
			AssignOption->Enable(true);
		}
	}

	// Add the main Material sub menu to the right click menu.
	Append( wxID_ANY, *LocalizeUnrealEd("MaterialsSubMenu"), MaterialAccessMenu );	

}

/*-----------------------------------------------------------------------------
	WxMainContextSurfaceMenu.
-----------------------------------------------------------------------------*/

WxMainContextSurfaceMenu::WxMainContextSurfaceMenu()
{
	FGetInfoRet gir = GApp->EditorFrame->GetInfo( GI_NUM_SURF_SELECTED );

	// Properties
	{
		FString TmpString;
		if( gir.iValue > 1 )
		{
			TmpString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("LevelViewportContext_SurfaceProperties_MultipleF"), gir.iValue) );
		}
		else
		{
			TmpString = LocalizeUnrealEd("LevelViewportContext_SurfaceProperties");
		}
		Append( IDM_SURFACE_PROPERTIES, *TmpString, TEXT("") );
	}

	// Find
	{
		AppendSeparatorIfNeeded();

		Append( ID_SyncContentBrowser, *LocalizeUnrealEd("LevelViewportContext_SyncContentBrowser"), TEXT(""), 0 );
	}


	// Select
	{
		wxMenu* SelectMenu = new wxMenu();
	
		SelectMenu->Append( IDMENU_ActorPopupSelectKismetReferenced, *LocalizeUnrealEd("SelectKismetReferencedActors"), TEXT("") );
		SelectMenu->Append( IDMENU_ActorPopupSelectKismetReferencedAll, *LocalizeUnrealEd("SelectKismetReferencedActorsAll"), TEXT("") );

		UBOOL AllSelectedAreBrushes = TRUE;
		for ( FSelectionIterator ActorItr( GEditor->GetSelectedActorIterator() ) ; ActorItr ; ++ActorItr )
		{
			AActor* CurrentActor = static_cast<AActor*>( *ActorItr );
			if( !(CurrentActor->IsBrush()) )
			{
				AllSelectedAreBrushes = FALSE;
				break;
			}
		}

		// Only add select all of the currently selected type if they are all brushes 
		if ( AllSelectedAreBrushes && gir.String != TEXT("") )
		{
			SelectMenu->Append( IDMENU_ActorPopupSelectAllClass, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("LevelViewportContext_SelectAllBrushesOfTypeF"), *(gir.String)) ), TEXT("") );
		}

		SelectMenu->Append( IDMENU_ActorPopupSelectBrushesAdd, *LocalizeUnrealEd("AddsSolids"), TEXT("") );
		SelectMenu->Append( IDMENU_ActorPopupSelectBrushesSubtract, *LocalizeUnrealEd("Subtracts"), TEXT("") );
		SelectMenu->Append( IDMENU_ActorPopupSelectBrushesSemisolid, *LocalizeUnrealEd("SemiSolids"), TEXT("") );
		SelectMenu->Append( IDMENU_ActorPopupSelectBrushesNonsolid, *LocalizeUnrealEd("NonSolids"), TEXT("") );

		SelectMenu->Append( IDMENU_ActorPopupSelectAllLights, *LocalizeUnrealEd("SelectAllLights"), TEXT("") );
		SelectMenu->Append( IDMENU_ActorPopupSelectAllRendered, *LocalizeUnrealEd("SelectAllRendered"), TEXT("") );

		SelectMenu->AppendSeparator( );

		SelectMenu->Append( IDM_SELECT_ALL, *LocalizeUnrealEd("SelectAll"), TEXT("") );
		SelectMenu->Append( IDM_SELECT_NONE, *LocalizeUnrealEd("SelectNone"), TEXT("") );

		SelectMenu->Append( IDM_SELECT_INVERT, *LocalizeUnrealEd("InvertSelections"), TEXT("") );

		SelectMenu->AppendSeparator( );

		SelectMenu->Append(IDM_SELECT_POST_PROCESS_VOLUME, *LocalizeUnrealEd("SelectPostProcessVolume"), TEXT("") );

		Append( wxID_ANY, *LocalizeUnrealEd( "LevelViewportContext_SelectMenu" ), SelectMenu );

		// Go to Click Point (Move viewport cameras)
		Append( IDMENU_PopupMoveCameraToPoint, *LocalizeUnrealEd("MoveCameraToPoint"), TEXT("") );
	}


	// Edit
	{
		const UBOOL bCanCutCopy = TRUE;
		const UBOOL bCanPaste = TRUE;
		AppendEditMenuItems( bCanCutCopy, bCanPaste );
	}


	// Select Surfaces
	{
		AppendSeparatorIfNeeded();

		SelectSurfMenu = new wxMenu();
		SelectSurfMenu->Append( ID_EditSelectAllSurfs, *LocalizeUnrealEd("LevelViewportContext_SelectAllSurfaces"), TEXT("") );
		SelectSurfMenu->AppendSeparator();
		SelectSurfMenu->Append( ID_SurfPopupSelectMatchingGroups, *LocalizeUnrealEd("MatchingGroups"), TEXT("") );
		SelectSurfMenu->Append( ID_SurfPopupSelectMatchingItems, *LocalizeUnrealEd("MatchingItems"), TEXT("") );
		SelectSurfMenu->Append( ID_SurfPopupSelectMatchingBrush, *LocalizeUnrealEd("MatchingBrush"), TEXT("") );
		SelectSurfMenu->Append( ID_SurfPopupSelectMatchingTexture, *LocalizeUnrealEd("MatchingTexture"), TEXT("") );
		SelectSurfMenu->Append( ID_SurfPopupSelectMatchingResolutionCurrentLevel, *LocalizeUnrealEd("MatchingResolution_Current"), TEXT("") );
		SelectSurfMenu->Append( ID_SurfPopupSelectMatchingResolution, *LocalizeUnrealEd("MatchingResolution"), TEXT("") );
		SelectSurfMenu->AppendSeparator();
		SelectSurfMenu->Append( ID_SurfPopupSelectAllAdjacents, *LocalizeUnrealEd("AllAdjacents"), TEXT("") );
		SelectSurfMenu->Append( ID_SurfPopupSelectAdjacentCoplanars, *LocalizeUnrealEd("AdjacentCoplanars"), TEXT("") );
		SelectSurfMenu->Append( ID_SurfPopupSelectAdjacentWalls, *LocalizeUnrealEd("AdjacentWalls"), TEXT("") );
		SelectSurfMenu->Append( ID_SurfPopupSelectAdjacentFloors, *LocalizeUnrealEd("AdjacentFloors"), TEXT("") );
		SelectSurfMenu->Append( ID_SurfPopupSelectAdjacentSlants, *LocalizeUnrealEd("AdjacentSlants"), TEXT("") );
		SelectSurfMenu->AppendSeparator();
		SelectSurfMenu->Append( ID_SurfPopupSelectReverse, *LocalizeUnrealEd("LevelViewportContext_Reverse"), TEXT("") );
		SelectSurfMenu->AppendSeparator();
		SelectSurfMenu->Append( ID_SurfPopupMemorize, *LocalizeUnrealEd("MemorizeSet"), TEXT("") );
		SelectSurfMenu->Append( ID_SurfPopupRecall, *LocalizeUnrealEd("RecallMemory"), TEXT("") );
		SelectSurfMenu->Append( ID_SurfPopupOr, *LocalizeUnrealEd("OrWithMemory"), TEXT("") );
		SelectSurfMenu->Append( ID_SurfPopupAnd, *LocalizeUnrealEd("AndWithMemory"), TEXT("") );
		SelectSurfMenu->Append( ID_SurfPopupXor, *LocalizeUnrealEd("XorWithMemory"), TEXT("") );
		Append( IDMENU_SurfPopupSelectSurfMenu, *LocalizeUnrealEd("SelectSurfaces"), SelectSurfMenu );
	}

	AppendSeparatorIfNeeded();

	// @todo CB: Needs support for unloaded assets (add a fast AnyAssetsOfClassSelected() method?)
	UMaterialInterface* mi = GEditor->GetSelectedObjects()->GetTop<UMaterialInterface>();
	FString Wk = *LocalizeUnrealEd("ApplyMaterial");
	if( mi )
	{
		Wk = FString::Printf( LocalizeSecure(LocalizeUnrealEd("ApplyMaterialF"), *mi->GetName()) );
	}
	Append( ID_SurfPopupApplyMaterial, *Wk, TEXT("") );
	Append( ID_SurfPopupReset, *LocalizeUnrealEd("Reset"), TEXT("") );

	// Align (Surfaces)
	{
		AlignmentMenu = new wxMenu();
		AlignmentMenu->Append( ID_SurfPopupUnalign, *LocalizeUnrealEd("Default"), TEXT("") );
		AlignmentMenu->Append( ID_SurfPopupAlignPlanarAuto, *LocalizeUnrealEd("Planar"), TEXT("") );
		AlignmentMenu->Append( ID_SurfPopupAlignPlanarWall, *LocalizeUnrealEd("PlanarWall"), TEXT("") );
		AlignmentMenu->Append( ID_SurfPopupAlignPlanarFloor, *LocalizeUnrealEd("PlanarFloor"), TEXT("") );
		AlignmentMenu->Append( ID_SurfPopupAlignBox, *LocalizeUnrealEd("Box"), TEXT("") );
		AlignmentMenu->Append( ID_SurfPopupAlignFit, *LocalizeUnrealEd("Fit"), TEXT("") );
		Append( IDMENU_SurfPopupAlignmentMenu, *LocalizeUnrealEd("Alignment"), AlignmentMenu );
	}

	AppendSeparatorIfNeeded();

	// Actor visibility
	{
		const UBOOL bAnySelectedActors = TRUE;
		AppendActorVisibilityMenuItems( bAnySelectedActors );
	}
	
	// Add actor
	{
		AppendAddActorMenu();
	}

	
	// Play level
	{
		AppendPlayLevelMenu();
	}
}

/*-----------------------------------------------------------------------------
	WxMainContextCoverSlotMenu.
-----------------------------------------------------------------------------*/

WxMainContextCoverSlotMenu::WxMainContextCoverSlotMenu(class ACoverLink *Link, FCoverSlot &Slot)
{
	Append( IDM_CoverEditMenu_ToggleEnabled, *LocalizeUnrealEd("EnabledQ"), TEXT(""), wxITEM_CHECK );
	Check( IDM_CoverEditMenu_ToggleEnabled, Slot.bEnabled );

	Append( IDM_CoverEditMenu_ToggleAutoAdjust, *LocalizeUnrealEd("AutoAdjustQ"), TEXT(""), wxITEM_CHECK );
	Check( IDM_CoverEditMenu_ToggleAutoAdjust, Link->bAutoAdjust );

	AppendSeparatorIfNeeded();

	Append( IDM_CoverEditMenu_ToggleTypeAutomatic, *LocalizeUnrealEd("Automatic"), TEXT(""), wxITEM_CHECK );
	Append( IDM_CoverEditMenu_ToggleTypeStanding, *LocalizeUnrealEd("Standing"), TEXT(""), wxITEM_CHECK );
	Append( IDM_CoverEditMenu_ToggleTypeMidLevel, *LocalizeUnrealEd("MidLevel"), TEXT(""), wxITEM_CHECK );

	switch( Slot.ForceCoverType )
	{
		case CT_Standing:
			Check( IDM_CoverEditMenu_ToggleTypeStanding, TRUE );
			break;
		case CT_MidLevel:
			Check( IDM_CoverEditMenu_ToggleTypeMidLevel, TRUE );
			break;
		default:
			Check( IDM_CoverEditMenu_ToggleTypeAutomatic, TRUE );
			break;
	}

	AppendSeparatorIfNeeded();

	Append( IDM_CoverEditMenu_ToggleCoverslip, *LocalizeUnrealEd("Coverslip"), TEXT(""), wxITEM_CHECK );
	Check( IDM_CoverEditMenu_ToggleCoverslip, Slot.bAllowCoverSlip );

	Append( IDM_CoverEditMenu_ToggleSwatTurn, *LocalizeUnrealEd("SwatTurn"), TEXT(""), wxITEM_CHECK );
	Check( IDM_CoverEditMenu_ToggleSwatTurn, Slot.bAllowSwatTurn );

	Append( IDM_CoverEditMenu_ToggleMantle, *LocalizeUnrealEd("Mantle"), TEXT(""), wxITEM_CHECK );
	Check( IDM_CoverEditMenu_ToggleMantle, Slot.bAllowMantle );

	Append( IDM_CoverEditMenu_TogglePopup, *LocalizeUnrealEd("Popup"), TEXT(""), wxITEM_CHECK );
	Check( IDM_CoverEditMenu_TogglePopup, Slot.bAllowPopup );

	Append( IDM_CoverEditMenu_ToggleLeanLeft, *LocalizeUnrealEd("LeanLeft"), TEXT(""), wxITEM_CHECK );
	Check( IDM_CoverEditMenu_ToggleLeanLeft, Slot.bLeanLeft );

	Append( IDM_CoverEditMenu_ToggleLeanRight, *LocalizeUnrealEd("LeanRight"), TEXT(""), wxITEM_CHECK );
	Check( IDM_CoverEditMenu_ToggleLeanRight, Slot.bLeanRight );

	Append( IDM_CoverEditMenu_ToggleClimbUp, *LocalizeUnrealEd("ClimbUp"), TEXT(""), wxITEM_CHECK );
	Check( IDM_CoverEditMenu_ToggleClimbUp, Slot.bAllowClimbUp );

	Append( IDM_CoverEditMenu_TogglePlayerOnly, *LocalizeUnrealEd("PlayerOnly"), TEXT(""), wxITEM_CHECK );
	Check( IDM_CoverEditMenu_TogglePlayerOnly, Slot.bPlayerOnly );

	Append( IDM_CoverEditMenu_TogglePreferLean, *LocalizeUnrealEd("PreferLeanOverPopup"), TEXT(""), wxITEM_CHECK );
	Check( IDM_CoverEditMenu_TogglePreferLean, Slot.bAllowClimbUp );

	AppendSeparatorIfNeeded();

	Append( IDM_CoverEditMenu_ToggleForceCanPopup, *LocalizeUnrealEd("ForceCanPopUp"), TEXT(""), wxITEM_CHECK );
	Check( IDM_CoverEditMenu_ToggleForceCanPopup, Slot.bForceCanPopUp );

	Append( IDM_CoverEditMenu_ToggleForceCanCoverslip_Left, *LocalizeUnrealEd("ForceCanCoverSlip_Left"), TEXT(""), wxITEM_CHECK );
	Check( IDM_CoverEditMenu_ToggleForceCanCoverslip_Left, Slot.bForceCanCoverSlip_Left );

	Append( IDM_CoverEditMenu_ToggleForceCanCoverslip_Right, *LocalizeUnrealEd("ForceCanCoverSlip_Right"), TEXT(""), wxITEM_CHECK );
	Check( IDM_CoverEditMenu_ToggleForceCanCoverslip_Right, Slot.bForceCanCoverSlip_Right );
}
