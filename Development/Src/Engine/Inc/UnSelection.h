/*=============================================================================
	UnSelection.h: Manages various kinds of selections

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/**
 * Manages selections of objects.  Used in the editor for selecting
 * objects in the various browser windows.
 */

class FSelectionTools
{
public:
	FSelectionTools();
	~FSelectionTools();

	/** List of selected objects in order of selection */
	TArray<UObject*> SelectedObjects;

	/** Tracks the most recently selected actor classes.  Used for UnrealEd menus. */
	TMRUArray<UClass*> SelectedClasses;

	void Select( UObject* InObject, UBOOL InSelect = 1 );
	void ToggleSelect( UObject* InObject );
	UBOOL IsSelected( UObject* InObject );
	void SelectNone( UClass* InClass );

	template< class T > void SelectNone()
	{
		for( TSelectedObjectIterator It(T::StaticClass()) ; It ; ++It )
		{
			It->ClearFlags( RF_EdSelected );
		}
		SelectedObjects.Empty();
	}

	UObject* GetTop(UClass* Class)
	{
		for(TSelectedObjectIterator It(Class);It;++It)
			return *It;
		return NULL;
	}

	template< class T > T* GetTop()
	{
		UObject* Selected = GetTop(T::StaticClass());
		return Selected ? CastChecked<T>(Selected) : NULL;
	}

	template< class T > INT CountSelections()
	{
		INT Num = 0;

		for( TSelectedObjectIterator It(T::StaticClass()) ; It ; ++It )
		{
			Num++;
		}

		return Num;
	}

	UBOOL ShouldSnapMovement();

	void RebuildSelectedList();
};

/**
 * Fills in the specified array with all selected actors of the desired type.
 * 
 * @param	selectedObjs - array to fill with selected objects of type T
 */
template< class T > void GetSelectedObjects(TArray<T*> &selectedObjs)
{
	selectedObjs.Empty();
	for (TSelectedObjectIterator It(T::StaticClass()); It; ++It)
	{
		if (It->IsA(T::StaticClass()))
		{
			selectedObjs.AddItem(Cast<T>(*It));
		}
	}
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
