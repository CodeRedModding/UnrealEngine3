/*=============================================================================
	MRUList : Helper class for handling MRU lists

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MRULIST_H__
#define __MRULIST_H__

/**
 * An MRU list of files.
 */
class FMRUList
{
public:
	FMRUList(const FString& InINISection, wxMenu* InMenu, INT StartMenuID = IDM_MRU_START, INT EndMenuID = IDM_MRU_END);
	virtual ~FMRUList();

	virtual void ReadFromINI();
	virtual void WriteToINI() const;
	
	/**
	 * Adds an item to the MRU list, moving it to the top.
	 *
	 * @param	Item		The item to add.
	 */
	void AddMRUItem(const FFilename& Item);
	
	/**
	 * Returns the index of the specified item, or INDEX_NONE if not found.
	 *
	 * @param	Item		The item to query.
	 * @return				The index of the specified item.
	 */
	INT FindMRUItemIdx(const FFilename& Item) const;

	/**
	 * Removes the specified item from the list if it exists.
	 *
	 * @param	Item		The item to remove.
	 */
	void RemoveMRUItem(const FFilename& Item);

	/**
	 * Removes the item at the specified index.
	 *
	 * @param	ItemIndex	Index of the item to remove.  Must be in [0, GetMaxItems()-1].
	 */
	void RemoveMRUItem(INT ItemIndex);

	/**
	 * Checks to make sure the file specified still exists.  If it does, it is
	 * moved to the top of the MRU list and we return TRUE.  If not, we remove it
	 * from the list and return FALSE.
	 *
	 * @param	ItemIndex		Index of the item to query
	 * @return					TRUE if the item exists, FALSE if it doesn't
	 */
	UBOOL VerifyMRUFile(INT ItemIndex);

	/**
	 * Accessor.
	 *
	 * @param	ItemIndex		Index of the item to access.
	 * @return					The filename at that index.
	 */
	const FFilename& GetMRUItem(INT ItemIndex) const { return Items(ItemIndex); }

	/**
	 * @return					The maximum number of items this MRUList can contain
	 */
	inline INT GetMaxItems() {	return MRU_MENU_END_ID - MRU_MENU_START_ID; }

private:
	/**
	 * Make sure we don't have more than GetMaxItems() in the list.
	 */
	void Cull();

	/**
	 * Moves the specified item to the top of the MRU list.
	 *
	 * @param	ItemIndex		Index of the item to bring to top.
	 */
	void MoveToTop(INT ItemIndex);

protected:
	/**
	 * Adds all the MRU items to the menu.
	 */
	virtual void UpdateMenu();

	/**
	 * Internal helper function to populate a provided array with data from the provided ini section using the provided ini key as a
	 * base, searching for the number of provided elements at a maximum
	 *
	 * @param	OutItems	Array to populate with items found in the INI file
	 * @param	INISection	Section of the INI file to check for items
	 * @param	INIKeyBase	Base string to use to search for individual items; this value will have a number appended to it while searching
	 * @param	NumElements	Maximum number of elements to search in the INI for
	 */
	static void InternalReadINI( TArray<FFilename>& OutItems, const FString& INISection, const FString& INIKeyBase, INT NumElements );
	
	/**
	 * Internal helper function to write out the provided array of data to the provided INI section, using the provided key base as a prefix
	 * for each item written to the INI file
	 *
	 * @param	InItems		Array of filenames to output to the INI file
	 * @param	INISection	Section of the INI file to write to
	 * @param	INIKeyBase	Base prefix to use for each item written to the INI file
	 */
	static void InternalWriteINI( const TArray<FFilename>& InItems, const FString& INISection, const FString& INIKeyBase );

	
	/** The start ID for the MRU menu.  Default to IDM_MRU_START */
	const INT MRU_MENU_START_ID;

	/** The end ID for the MRU menu.  Default to IDM_MRU_END */
	const INT MRU_MENU_END_ID;

	/** The filenames. */
	TArray<FFilename> Items;

	/** The INI section we read/write the filenames to. */
	FString INISection;

	/** The menu that we write these MRU items to. */
	wxMenu* Menu;
};

#endif // #ifndef __MRULIST_H__
