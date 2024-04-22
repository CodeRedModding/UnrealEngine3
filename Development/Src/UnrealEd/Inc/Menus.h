/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MENUS_H__
#define __MENUS_H__

/**
 * Baseclass for menus created when right clicking in the main editor viewports.
 */
class WxMainContextMenuBase : public wxMenu
{	
public:
	wxMenu* ActorFactoryMenu;
	wxMenu* ActorFactoryMenuAdv;
	wxMenu* ReplaceWithActorFactoryMenu;
	wxMenu* ReplaceWithActorFactoryMenuAdv;
	wxMenu* RecentClassesMenu;

	WxMainContextMenuBase();

	void AppendEditMenuItems( const UBOOL bCanCutCopy, const UBOOL bCanPaste );
	void AppendActorVisibilityMenuItems( const UBOOL bAnySelectedActors );
	void AppendPlayLevelMenu();
	void AppendAddActorMenu();

	/**
	 * Helper method to append the convert volumes submenu to a provided menu
	 *
	 * @param	InParentMenu	Menu to append the convert volumes submenu to
	 */
	void AppendConvertVolumeSubMenu( wxMenu* InParentMenu );

	/**
	 * Creates and populates actor factory context sub-menus.
	 *
	 * @param	SelectedAssets		the list of currently selected assets
	 * @param	pmenu_CreateActor	receives the pointer to the wxMenu that holds Create Actor factory options.  *pmenu_CreateActor must be NULL.
	 * @param	pmenu_ReplaceActor	receives the pointer to the wxMenu for Replace Actor Using Factory options.  *pmenu_ReplaceActor must be NULL.
	 * @param	OutSelectedAssetMenuOptions	An array of Add actor menu options which are valid for the selected asset.  Can be NULL if not used.
	 */
	static void CreateActorFactoryMenus( const TArray<struct FSelectedAssetInfo>& SelectedAssets, wxMenu** pmenu_CreateActor, wxMenu** pMenu_ReplaceActor, TArray<FString>* OutSelectedAssetMenuOptions );

	/** Index of the last menu separator that we appended to the menu */
	INT LastMenuSeparatorIndex;

	/** Appends a separator to the menu if the last appended item isn't already a separator */
	void AppendSeparatorIfNeeded()
	{
		if( GetMenuItemCount() > 0 &&
			( LastMenuSeparatorIndex == INDEX_NONE || LastMenuSeparatorIndex < ( (INT)GetMenuItemCount() - 1 ) ) )
		{
			LastMenuSeparatorIndex = GetMenuItemCount();
			AppendSeparator();
		}
	}
};

/**
 * Main level viewport context menu.
 */
class WxMainContextMenu : public WxMainContextMenuBase
{
public:
	WxMainContextMenu();

private:
	wxMenu* OrderMenu;
	wxMenu* PolygonsMenu;
	wxMenu* CSGMenu;
	wxMenu* SolidityMenu;
	wxMenu* SelectMenu;
	wxMenu* AlignMenu;
	wxMenu* PivotMenu;
	wxMenu* TransformMenu;
	wxMenu* DetailModeMenu;
	wxMenu* VolumeMenu;
	wxMenu* PathMenu;
	wxMenu* ComplexPathMenu;
	wxMenu* BlockingVolumeMenu;

	/** Adds the Material sub menu */
	void AppendMaterialsAndTexturesMenu(AActor* FirstActor);
	void AppendMaterialsMultipleSelectedMenu();
};

/**
 * Main level viewport context menu for BSP surfaces.
 */
class WxMainContextSurfaceMenu : public WxMainContextMenuBase
{
public:
	WxMainContextSurfaceMenu();

private:
	wxMenu* SelectSurfMenu;
	wxMenu* ExtrudeMenu;
	wxMenu* AlignmentMenu;
};

/**
 * Main level viewport context menu for cover slots.
 */
class WxMainContextCoverSlotMenu : public WxMainContextMenuBase
{
public:
	WxMainContextCoverSlotMenu(class ACoverLink *Link, struct FCoverSlot &Slot);
};

#endif // __MENUS_H__
