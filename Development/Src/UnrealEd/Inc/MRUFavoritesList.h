/*=============================================================================
	MRUFavoritesList : Helper class for handling MRU and favorited maps

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef __MAINMRUFAVORITESLIST_H__
#define __MAINMRUFAVORITESLIST_H__

#include "MRUList.h"

/** Simple class to represent a combined MRU and favorite map list */
class FMainMRUFavoritesList : public FMRUList
{
public:
	/**
	 * Constructor
	 *
	 * @param	InMRUMenu		Menu to be populated with MRU items
	 * @param	InFavoritesMenu	Menu to be populated with favorite items
	 * @param	InCombinedMenu	Menu to be populated with both the MRU and favorite items
	 */
	FMainMRUFavoritesList( wxMenu* InMRUMenu, wxMenu* InFavoritesMenu, wxMenu* InCombinedMenu );
	
	/** Destructor */
	~FMainMRUFavoritesList();
	
	/** Populate MRU/Favorites list by reading saved values from the relevant INI file */
	virtual void ReadFromINI();

	/** Save off the state of the MRU and favorites lists to the relevant INI file */
	virtual void WriteToINI() const;

	/**
	 * Add a new file item to the favorites list
	 *
	 * @param	Item	Filename of the item to add to the favorites list
	 */
	void AddFavoritesItem( const FFilename& Item );
	
	/**
	 * Remove a file from the favorites list
	 *
	 * @param	Item	Filename of the item to remove from the favorites list
	 */
	void RemoveFavoritesItem( const FFilename& Item );

	/**
	 * Returns whether a filename is favorited or not
	 *
	 * @param	Item	Filename of the item to check
	 *
	 * @return	TRUE if the provided item is in the favorite's list; FALSE if it is not
	 */
	UBOOL ContainsFavoritesItem( const FFilename& Item );

	/**
	 * Return the favorites item specified by the provided index
	 *
	 * @param	ItemIndex	Index of the favorites item to return
	 *
	 * @return	The favorites item specified by the provided index
	 */
	FFilename GetFavoritesItem( INT ItemIndex );

	/**
	 * Verifies that the favorites item specified by the provided index still exists. If it does not, the item
	 * is removed from the favorites list and the user is notified.
	 *
	 * @param	ItemIndex	Index of the favorites item to check
	 *
	 * @return	TRUE if the item specified by the index was verified and still exists; FALSE if it does not
	 */
	UBOOL VerifyFavoritesFile( INT ItemIndex );

private:

	/** Updates all of the relevant menus for the MRU/favorites list */
	virtual void UpdateMenu();

	/** Favorited items */
	TArray<FFilename> FavoriteItems;

	/** Menu to populated with favorited items; NOTE: NOT OWNED OR ALLOCATED BY THIS CLASS */
	wxMenu* FavoritesMenu;

	/** Menu to be populated with both the MRU and favorited items; NOTE: NOT OWNED OR ALLOCATED BY THIS CLASS */
	wxMenu* CombinedMenu;

	/** INI section to read/write favorite items to */
	static const FString FAVORITES_INI_SECTION;
};

#endif
