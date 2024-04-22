/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNSELECTION_H__
#define __UNSELECTION_H__

/**
 * Manages selections of objects.  Used in the editor for selecting
 * objects in the various browser windows.
 */
class USelection : public UObject
{
private:
	typedef TArray<UObject*>	ObjectArray;
	typedef TMRUArray<UClass*>	ClassArray;

	friend class FSelectionIterator;

public:
	DECLARE_CLASS_INTRINSIC(USelection,UObject,CLASS_Transient|0,Engine)

	USelection();

	typedef ObjectArray::TIterator TObjectIterator;
	typedef ObjectArray::TConstIterator TObjectConstIterator;

	typedef ClassArray::TIterator TClassIterator;
	typedef ClassArray::TConstIterator TClassConstIterator;

	TObjectIterator			ObjectItor()			{ return TObjectIterator( SelectedObjects ); }
	TObjectConstIterator	ObjectConstItor() const	{ return TObjectConstIterator( SelectedObjects ); }
	
	TClassIterator			ClassItor()				{ return TClassIterator( SelectedClasses ); }
	TClassConstIterator		ClassConstItor() const	{ return TClassConstIterator( SelectedClasses ); }

	/**
	 * Returns the number of objects in the selection set.  This function is used by clients in
	 * conjunction with op::() to iterate over selected objects.  Note that some of these objects
	 * may be NULL, and so clients should use CountSelections() to get the true number of
	 * non-NULL selected objects.
	 * 
	 * @return		Number of objects in the selection set.
	 */
	INT Num() const
	{
		return SelectedObjects.Num();
	}

	/**
	 * @return	The Index'th selected objects.  May be NULL.
	 */
	UObject* operator()(INT InIndex)
	{
		return SelectedObjects(InIndex);
	}

	/**
	 * @return	The Index'th selected objects.  May be NULL.
	 */
	const UObject* operator()(INT InIndex) const
	{
		return SelectedObjects(InIndex);
	}

	/**
	 * Call before beginning selection operations
	 */
	void BeginBatchSelectOperation()
	{
		SelectionMutex++;
	}

	/**
	 * Should be called when selection operations are complete.  If all selection operations are complete, notifies all listeners
	 * that the selection has been changed.
	 */
	void EndBatchSelectOperation()
	{
		if ( --SelectionMutex == 0 )
		{
			const UBOOL bSelectionChanged = bIsBatchDirty;
			bIsBatchDirty = FALSE;

			if ( bSelectionChanged )
			{
				GCallbackEvent->Send(CALLBACK_SelChange);

				// new version - includes which selection set was modified
				GCallbackEvent->Send(CALLBACK_SelChange, this);
			}
		}
	}

	/**
	 * @return	Returns whether or not the selection object is currently in the middle of a batch select block.
	 */
	UBOOL IsBatchSelecting() const
	{
		return SelectionMutex != 0;
	}

	/**
	 * Selects the specified object.
	 *
	 * @param	InObject	The object to select/deselect.  Must be non-NULL.
	 */
	void Select(UObject* InObject);

	/**
	 * Deselects the specified object.
	 *
	 * @param	InObject	The object to deselect.  Must be non-NULL.
	 */
	void Deselect(UObject* InObject);

	/**
	 * Selects or deselects the specified object, depending on the value of the bSelect flag.
	 *
	 * @param	InObject	The object to select/deselect.  Must be non-NULL.
	 * @param	bSelect		TRUE selects the object, FALSE deselects.
	 */
	void Select(UObject* InObject, UBOOL bSelect);

	/**
	 * Toggles the selection state of the specified object.
	 *
	 * @param	InObject	The object to select/deselect.  Must be non-NULL.
	 */
	void ToggleSelect(UObject* InObject);

	/**
	 * Deselects all objects.
	 */
	void DeselectAll();

	/**
	 * Deselects all objects of the specified class.
	 *
	 * @param	InClass		The type of object to deselect.  Must be non-NULL.
	 * @param	InFlags		[opt] Flags that the object must have if it is to be deselected.  Ignored if 0.
	 */
	void SelectNone( UClass* InClass, EObjectFlags InFlags = 0 );

	/**
	 * If batch selection is active, sets flag indicating something actually changed.
	 */
	void MarkBatchDirty();

	/**
	 * Returns the first selected object of the specified class.
	 *
	 * @param	InClass		The class of object to return.  Must be non-NULL.
	 * @return				The first selected object of the specified class.
	 */
	UObject* GetTop(UClass* InClass)
	{
		check( InClass );
		for( INT i=0; i<SelectedObjects.Num(); ++i )
		{
			UObject* SelectedObject = SelectedObjects(i);
			if( SelectedObject && (SelectedObject->IsA(InClass) || (SelectedObject->GetClass()->ImplementsInterface(InClass))))
			{
				return SelectedObject;
			}
		}
		return NULL;
	}

	/**
	* Returns the last selected object of the specified class.
	*
	* @param	InClass		The class of object to return.  Must be non-NULL.
	* @return				The last selected object of the specified class.
	*/
	UObject* GetBottom(UClass* InClass)
	{
		check( InClass );
		for( INT i = SelectedObjects.Num()-1 ; i > -1 ; --i )
		{
			UObject* SelectedObject = SelectedObjects(i);
			if( SelectedObject && SelectedObject->IsA(InClass) )
			{
				return SelectedObject;
			}
		}
		return NULL;
	}

	/**
	 * Returns the first selected object.
	 *
	 * @return				The first selected object.
	 */
	template< class T > T* GetTop()
	{
		UObject* Selected = GetTop(T::StaticClass());
		return Selected ? CastChecked<T>(Selected) : NULL;
	}

	/**
	* Returns the last selected object.
	*
	* @return				The last selected object.
	*/
	template< class T > T* GetBottom()
	{
		UObject* Selected = GetBottom(T::StaticClass());
		return Selected ? CastChecked<T>(Selected) : NULL;
	}

	/**
	 * Returns TRUE if the specified object is non-NULL and selected.
	 *
	 * @param	InObject	The object to query.  Can be NULL.
	 * @return				TRUE if the object is selected, or FALSE if InObject is unselected or NULL.
	 */
	UBOOL IsSelected(UObject* InObject) const;

	/**
	 * Returns the number of selected objects of the specified type.
	 *
	 * @param	bIgnorePendingKill	specify TRUE to count only those objects which are not pending kill (marked for garbage collection)
	 * @return						The number of objects of the specified type.
	 */
	template< class T >
	INT CountSelections( UBOOL bIgnorePendingKill=FALSE )
	{
		INT Count = 0;
		for( INT i=0; i<SelectedObjects.Num(); ++i )
		{
			UObject* SelectedObject = SelectedObjects(i);
			if( SelectedObject && SelectedObject->IsA(T::StaticClass()) && !(bIgnorePendingKill && SelectedObject->IsPendingKill()) )
			{
				++Count;
			}
		}
		return Count;
	}

	/**
	 * Untemplated version of CountSelections.
	 */
	INT CountSelections(UClass *ClassToCount, UBOOL bIgnorePendingKill=FALSE)
	{
		INT Count = 0;
		for( INT i=0; i<SelectedObjects.Num(); ++i )
		{
			UObject* SelectedObject = SelectedObjects(i);
			if( SelectedObject && SelectedObject->IsA(ClassToCount) && !(bIgnorePendingKill && SelectedObject->IsPendingKill()) )
			{
				++Count;
			}
		}
		return Count;
	}

	/**
	 * Gets selected class by index.
	 *
	 * @return			The selected class at the specified index.
	 */
	UClass* GetSelectedClass(INT InIndex) const
	{
		return SelectedClasses( InIndex );
	}

	/**
	 * Sync's all objects' RF_EdSelected flag based on the current selection list.
	 */
	void RefreshObjectFlags();

	/**
	 * Serialize function used to serialize object references to to avoid stale
	 * object pointers.
	 *
	 * @param	Ar		The archive to serialize with.
	 */
	virtual void Serialize(FArchive& Ar);

	/**
	 * Returns TRUE if the selection set matches the input, object to object.
	 */
	template< class T > 
	UBOOL CompareWithSelection(const TArray<T*> &selectedObjs) const
	{
		if ( selectedObjs.Num() != SelectedObjects.Num() )
		{
			return FALSE;
		}

		// Count number of selected objects of type T.
		INT ctr = 0;
		for ( INT j = 0 ; j < SelectedObjects.Num() ; ++j )
		{
			const UObject* CurObj = SelectedObjects(j);
			if ( CurObj && CurObj->IsA(T::StaticClass()) )
			{
				++ctr;
			}
		}

		if ( ctr != selectedObjs.Num() )
		{
			return FALSE;
		}

		// Match object for object.
		ctr = 0;
		for ( INT i = 0 ; i < selectedObjs.Num() ; ++i )
		{
			const UObject* MatchingObj = selectedObjs(i);
			for ( INT j = 0 ; j < SelectedObjects.Num() ; ++j )
			{
				const UObject* CurObj = SelectedObjects(j);
				if ( CurObj && CurObj->IsA(T::StaticClass()) && CurObj == MatchingObj )
				{
					++ctr;
					break;
				}
			}
		}

		return ctr == SelectedObjects.Num();
	}

	/**
	 * Fills in the specified array with all selected objects of the desired type.
	 * 
	 * @param	OutSelectedObjects		[out] Array to fill with selected objects of type T
	 * @return							The number of selected objects of the specified type.
	 */
	template< class T > 
	INT GetSelectedObjects(TArray<T*> &OutSelectedObjects)
	{
		OutSelectedObjects.Empty();
		for ( INT Idx = 0 ; Idx < SelectedObjects.Num() ; ++Idx )
		{
			if ( SelectedObjects(Idx) && SelectedObjects(Idx)->IsA(T::StaticClass()) )
			{
				OutSelectedObjects.AddItem( (T*)SelectedObjects(Idx) );
			}
		}
		return OutSelectedObjects.Num();
	}
	INT GetSelectedObjects( TArray<UObject*>& out_SelectedObjects )
	{
		out_SelectedObjects = SelectedObjects;
		return SelectedObjects.Num();
	}

	INT GetSelectedObjects(UClass *FilterClass, TArray<UObject*> &OutSelectedObjects)
	{
		OutSelectedObjects.Empty();
		for ( INT Idx = 0 ; Idx < SelectedObjects.Num() ; ++Idx )
		{
			if ( SelectedObjects(Idx) && SelectedObjects(Idx)->IsA(FilterClass) )
			{
				OutSelectedObjects.AddItem( SelectedObjects(Idx) );
			}
		}
		return OutSelectedObjects.Num();
	}


protected:
	/** List of selected objects, ordered as they were selected. */
	ObjectArray	SelectedObjects;

	/** Tracks the most recently selected actor classes.  Used for UnrealEd menus. */
	ClassArray	SelectedClasses;

	/** Tracks the number of active selection operations.  Allows batched selection operations to only send one notification at the end of the batch */
	INT			SelectionMutex;

	/** Tracks whether the selection set changed during a batch selection operation */
	UBOOL		bIsBatchDirty;

private:
	// Hide IsSelected(), as calling IsSelected() on a selection set almost always indicates
	// an error where the caller should use IsSelected(UObject* InObject).
	UBOOL IsSelected() const
	{
		return UObject::IsSelected();
	}
};

/**
 * Manages selections of objects.  Used in the editor for selecting
 * objects in the various browser windows.
 */
class FSelectionIterator
{
public:
	FSelectionIterator(USelection& InSelection)
		: Selection( InSelection )
	{
		Reset();
	}

	/** Advances iterator to the next valid element in the container. */
	void operator++()
	{
		while ( true )
		{
			++Index;

			// Halt if the end of the selection set has been reached.
			if ( !IsIndexValid() )
			{
				return;
			}

			// Halt if at a valid object.
			if ( IsObjectValid() )
			{
				return;
			}
		}
	}

	/** Element access. */
	UObject* operator*() const
	{
		return GetCurrentObject();
	}

	/** Element access. */
	UObject* operator->() const
	{
		return GetCurrentObject();
	}

	/** Returns TRUE if the iterator has not yet reached the end of the selection set. */
	operator UBOOL() const
	{
		return IsIndexValid();
	}

	/** Resets the iterator to the beginning of the selection set. */
	void Reset()
	{
		Index = -1;
		++( *this );
	}

	/** Returns an index to the current element. */
	INT GetIndex() const
	{
		return Index;
	}

private:
	UObject* GetCurrentObject() const
	{
		return Selection.SelectedObjects( Index );
	}

	UBOOL IsObjectValid() const
	{
		return GetCurrentObject() != NULL;
	}

	UBOOL IsIndexValid() const
	{
		return Selection.SelectedObjects.IsValidIndex( Index );
	}

	USelection&	Selection;
	INT			Index;
};

#endif // __UNSELECTION_H__
