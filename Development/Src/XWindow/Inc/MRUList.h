/*=============================================================================
	MRUList : Helper class for handling MRU lists

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

#define MRU_MAX_ITEMS 8

class FMRUList
{
public:
	FMRUList()
	{
		check(0);	// wrong constructor
	}
	FMRUList( FString InINISection, wxMenu* InMenu );
	~FMRUList();

	TArray<FFilename> Items;		// The filenames
	FString INISection;				// The INI section we read/write the filenames to
	wxMenu* Menu;					// The menu that we write these MRU items to

	void Cull();
	void ReadINI();
	void WriteINI();
	void MoveToTop( INT InItem );
	void AddItem( FFilename InItem );
	INT FindItemIdx( FFilename InItem );
	void RemoveItem( FFilename InItem );
	void RemoveItem( INT InItem );
	void UpdateMenu();
	UBOOL VerifyFile( INT InItem );
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
