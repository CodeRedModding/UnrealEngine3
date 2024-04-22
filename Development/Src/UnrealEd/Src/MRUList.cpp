/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "FConfigCacheIni.h"

FMRUList::FMRUList(const FString& InINISection, wxMenu* InMenu, INT StartMenuID, INT EndMenuID)
	:	MRU_MENU_START_ID( StartMenuID ),
		MRU_MENU_END_ID( EndMenuID ),
		INISection( InINISection ),
		Menu( InMenu )
{}

FMRUList::~FMRUList()
{
	Items.Empty();
}

// Make sure we don't have more than MRU_MAX_ITEMS in the list.
void FMRUList::Cull()
{
	while( Items.Num() > GetMaxItems() )
	{
		Items.Remove( Items.Num()-1 );
	}
}

void FMRUList::ReadFromINI()
{
	InternalReadINI( Items, INISection, TEXT("MRUItem"), GetMaxItems() );
	UpdateMenu();
}

void FMRUList::WriteToINI() const
{
	InternalWriteINI( Items, INISection, TEXT("MRUItem") );
}

// Moves the specified item to the top of the list.
void FMRUList::MoveToTop(INT InItem)
{
	check( InItem > -1 && InItem < Items.Num() );

	TArray<FFilename> WkArray;
	WkArray = Items;

	const FFilename Save = WkArray(InItem);
	WkArray.Remove( InItem );

	Items.Empty();
	new(Items)FString( *Save );
	Items += WkArray;
}

// Attempts to add an item to the MRU list.
void FMRUList::AddMRUItem(const FFilename& InItem)
{
	// See if the item already exists in the list.  If so,
	// move it to the top of the list and leave.
	const INT ItemIndex = Items.FindItemIndex( InItem );
	if ( ItemIndex != INDEX_NONE )
	{
		MoveToTop( ItemIndex );
		UpdateMenu();
		WriteToINI();
		return;
	}

	// Item is new, so add it to the bottom of the list.
	if( InItem.Len() )
	{
		new(Items)FFilename( *InItem );
		MoveToTop( Items.Num()-1 );
	}

	Cull();
	UpdateMenu();
	WriteToINI();
}

// Returns the index of the specified item, or INDEX_NONE if not found.
INT FMRUList::FindMRUItemIdx(const FFilename& InItem) const
{
	for( INT mru = 0 ; mru < Items.Num() ; ++mru )
	{
		if( Items(mru) == InItem )
		{
			return mru;
		}
	}

	return INDEX_NONE;
}

// Removes the specified item from the list.
void FMRUList::RemoveMRUItem(const FFilename& InItem)
{
	RemoveMRUItem( FindMRUItemIdx( InItem ) );
}

// Removes the item at the specified index.
void FMRUList::RemoveMRUItem(INT InItem)
{
	// Find the item and remove it.
	check( InItem > -1 && InItem < GetMaxItems() );
	Items.Remove( InItem );
}

// Adds all the MRU items to the menu.
void FMRUList::UpdateMenu()
{
	// Remove all existing MRU menu items.
	for( INT x = MRU_MENU_START_ID ; x < MRU_MENU_END_ID ; ++x )
	{
		if( Menu->FindItem( x ) )
		{
			Menu->Delete( x );
		}
	}

	// Add the MRU items to the menu.
	for( INT x = 0 ; x < Items.Num() ; ++x )
	{
		const FFilename Item = Items(x);
		const FString Text( FString::Printf( TEXT("&%d %s"), x+1, *Item ) );
		const FString Help( FString::Printf( LocalizeSecure(LocalizeUnrealEd("OpenTheFile_F"), *Item.GetBaseFilename()) ) );
		const INT ID = MRU_MENU_START_ID + x;
		Menu->Append( ID, *Text, *Help );
	}
}

/**
 * Internal helper function to populate a provided array with data from the provided ini section using the provided ini key as a
 * base, searching for the number of provided elements at a maximum
 *
 * @param	OutItems	Array to populate with items found in the INI file
 * @param	INISection	Section of the INI file to check for items
 * @param	INIKeyBase	Base string to use to search for individual items; this value will have a number appended to it while searching
 * @param	NumElements	Maximum number of elements to search in the INI for
 */
void FMRUList::InternalReadINI( TArray<FFilename>& OutItems, const FString& INISection, const FString& INIKeyBase, INT NumElements )
{
	// Clear existing items
	OutItems.Empty();

	// Iterate over the maximum number of provided elements
	for( INT ItemIdx = 0 ; ItemIdx < NumElements ; ++ItemIdx )
	{
		// Try to find data for a key formed as "INIKeyBaseItemIdx" for the provided INI section. If found, add the data to the output item array.
		FString CurItem;
		if ( GConfig->GetString( *INISection, *FString::Printf( TEXT("%s%d"), *INIKeyBase, ItemIdx ), CurItem, GEditorUserSettingsIni ) )
		{
			OutItems.AddItem( CurItem );
		}
	}
}

/**
 * Internal helper function to write out the provided array of data to the provided INI section, using the provided key base as a prefix
 * for each item written to the INI file
 *
 * @param	InItems		Array of filenames to output to the INI file
 * @param	INISection	Section of the INI file to write to
 * @param	INIKeyBase	Base prefix to use for each item written to the INI file
 */
void FMRUList::InternalWriteINI( const TArray<FFilename>& InItems, const FString& INISection, const FString& INIKeyBase )
{
	GConfig->EmptySection( *INISection, GEditorUserSettingsIni );

	for ( INT ItemIdx = 0; ItemIdx < InItems.Num(); ++ItemIdx )
	{
		GConfig->SetString( *INISection, *FString::Printf( TEXT("%s%d"), *INIKeyBase, ItemIdx ), *InItems( ItemIdx ), GEditorUserSettingsIni );
	}

	GConfig->Flush( FALSE, GEditorUserSettingsIni );
}

// Checks to make sure the file specified still exists.  If it does, it is moved to the top
// of the MRU list and we return 1.  If not, we remove it from the list and return 0.
UBOOL FMRUList::VerifyMRUFile(INT InItem)
{
	check( InItem > -1 && InItem < GetMaxItems() );
	const FFilename filename = Items(InItem);

	// If the file doesn't exist, tell the user about it, remove the file from the list and update the menu.
	if( GFileManager->FileSize( *filename ) == -1 )
	{
		appMsgf( AMT_OK, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("Error_FileDoesNotExist"), *filename) ) );
		RemoveMRUItem( InItem );
		UpdateMenu();
		WriteToINI();

		return FALSE;
	}

	// Otherwise, move the file to the top of the list and update the menu.
	MoveToTop( InItem );
	UpdateMenu();
	WriteToINI();

	return TRUE;
}
