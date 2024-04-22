/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "MRUFavoritesList.h"
#include "FConfigCacheIni.h"

const FString FMainMRUFavoritesList::FAVORITES_INI_SECTION = TEXT("FavoriteFiles");

/**
 * Constructor
 *
 * @param	InMRUMenu		Menu to be populated with MRU items
 * @param	InFavoritesMenu	Menu to be populated with favorite items
 * @param	InCombinedMenu	Menu to be populated with both the MRU and favorite items
 */
FMainMRUFavoritesList::FMainMRUFavoritesList( wxMenu* InMRUMenu, wxMenu* InFavoritesMenu, wxMenu* InCombinedMenu )
:	FMRUList( TEXT("MRU"), InMRUMenu ),
	FavoritesMenu( InFavoritesMenu ),
	CombinedMenu( InCombinedMenu )
{
	check( FavoritesMenu );
	check( CombinedMenu );
}

/** Destructor */
FMainMRUFavoritesList::~FMainMRUFavoritesList()
{
	FavoriteItems.Empty();

	FavoritesMenu = NULL;
	CombinedMenu = NULL;
}

/** Populate MRU/Favorites list by reading saved values from the relevant INI file */
void FMainMRUFavoritesList::ReadFromINI()
{
	// Read in the MRU items
	InternalReadINI( Items, INISection, TEXT("MRUItem"), GetMaxItems() );

	// Read in the Favorite items
	InternalReadINI( FavoriteItems, FAVORITES_INI_SECTION, TEXT("FavoritesItem"), IDM_FAVORITES_END - IDM_FAVORITES_START );	

	// Update all the menus as necessary
	UpdateMenu();
}

/** Save off the state of the MRU and favorites lists to the relevant INI file */
void FMainMRUFavoritesList::WriteToINI() const
{
	InternalWriteINI( Items, INISection, TEXT("MRUItem") );
	InternalWriteINI( FavoriteItems, FAVORITES_INI_SECTION, TEXT("FavoritesItem") );
}

/**
 * Add a new file item to the favorites list
 *
 * @param	Item	Filename of the item to add to the favorites list
 */
void FMainMRUFavoritesList::AddFavoritesItem( const FFilename& Item )
{
	// Only add the item if it isn't already a favorite!
	if ( !FavoriteItems.ContainsItem( Item ) )
	{
		FavoriteItems.AddItem( Item );
		UpdateMenu();
		WriteToINI();
	}
}

/**
 * Remove a file from the favorites list
 *
 * @param	Item	Filename of the item to remove from the favorites list
 */
void FMainMRUFavoritesList::RemoveFavoritesItem( const FFilename& Item )
{
	if ( FavoriteItems.ContainsItem( Item ) )
	{
		FavoriteItems.RemoveSingleItem( Item );
		UpdateMenu();
		WriteToINI();
	}
}

/**
 * Returns whether a filename is favorited or not
 *
 * @param	Item	Filename of the item to check
 *
 * @return	TRUE if the provided item is in the favorite's list; FALSE if it is not
 */
UBOOL FMainMRUFavoritesList::ContainsFavoritesItem( const FFilename& Item )
{
	return FavoriteItems.ContainsItem( Item );
}

/**
 * Return the favorites item specified by the provided index
 *
 * @param	ItemIndex	Index of the favorites item to return
 *
 * @return	The favorites item specified by the provided index
 */
FFilename FMainMRUFavoritesList::GetFavoritesItem( INT ItemIndex )
{
	check( FavoriteItems.IsValidIndex( ItemIndex ) );
	return FavoriteItems( ItemIndex );
}

/**
 * Verifies that the favorites item specified by the provided index still exists. If it does not, the item
 * is removed from the favorites list and the user is notified.
 *
 * @param	ItemIndex	Index of the favorites item to check
 *
 * @return	TRUE if the item specified by the index was verified and still exists; FALSE if it does not
 */
UBOOL FMainMRUFavoritesList::VerifyFavoritesFile( INT ItemIndex )
{
	check( FavoriteItems.IsValidIndex( ItemIndex ) );
	const FFilename& CurFileName = FavoriteItems( ItemIndex );

	const UBOOL bFileExists = GFileManager->FileSize( *CurFileName ) != INDEX_NONE;
	
	// If the file doesn't exist any more, remove it from the favorites list and alert the user
	if ( !bFileExists )
	{
		appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd("Error_FavoritesFileDoesNotExist"), *CurFileName ) ) );
		RemoveFavoritesItem( CurFileName );
	}

	return bFileExists;
}

/** Updates all of the relevant menus for the MRU/favorites list */
void FMainMRUFavoritesList::UpdateMenu()
{
	// Clear any pre-existing MRU menu items
	while ( Menu->GetMenuItemCount() > 0 )
	{
		Menu->Delete( Menu->FindItemByPosition( 0 ) );
	}

	// Clear any pre-existing favorite menu items
	while ( FavoritesMenu->GetMenuItemCount() > 0 )
	{
		FavoritesMenu->Delete( FavoritesMenu->FindItemByPosition( 0 ) );
	}

	// Clear any existing combined menu items
	while ( CombinedMenu->GetMenuItemCount() > 0 )
	{
		CombinedMenu->Delete( CombinedMenu->FindItemByPosition( 0 ) );
	}

	// Append all of the MRU items to the MRU and combined menus
	for( INT FileIndex = 0 ; FileIndex < Items.Num() ; ++FileIndex )
	{
		const FFilename& Item = Items(FileIndex);
		const FString Text( FString::Printf( TEXT("&%d %s"), FileIndex+1, *Item ) );
		const FString Help( FString::Printf( LocalizeSecure( LocalizeUnrealEd("OpenTheFile_F"), *Item.GetBaseFilename() ) ) );
		const INT ID = MRU_MENU_START_ID + FileIndex;
		Menu->Append( ID, *Text, *Help );
		CombinedMenu->Append( ID, *Text, *Help );
	}

	// Append a separator to the combined menu if necessary so that it looks nice
	if ( Items.Num() > 0 && FavoriteItems.Num() > 0 )
	{
		CombinedMenu->AppendSeparator();
	}

	// Append all of the favorite items to the favorite and combined menus
	for ( INT FileIndex = 0; FileIndex < FavoriteItems.Num(); ++FileIndex )
	{
		const FFilename& CurFile = FavoriteItems(FileIndex);
		const FString MenuText = FString::Printf( TEXT("&%d %s"), FileIndex + 1, *CurFile );
		const FString HelpText = FString::Printf( LocalizeSecure( LocalizeUnrealEd("OpenTheFile_F"), *CurFile.GetBaseFilename() ) );
		const INT CurFileID = IDM_FAVORITES_START + FileIndex;
		FavoritesMenu->Append( CurFileID, *MenuText, *HelpText );
		CombinedMenu->Append( CurFileID, *CurFile, *HelpText );
	}
}