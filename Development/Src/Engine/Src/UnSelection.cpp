/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"
#include "UnSelection.h"

IMPLEMENT_CLASS( USelection );

USelection::USelection()
:	SelectionMutex( 0 ), bIsBatchDirty(FALSE)
{
	SelectedClasses.MaxItems = 5;
}

/**
 * Selects the specified object.
 *
 * @param	InObject	The object to select/deselect.  Must be non-NULL.
 */
void USelection::Select(UObject* InObject)
{
	check( InObject );

	// Warn if we attempt to select a PIE object.
	if ( InObject->GetOutermost()->PackageFlags & PKG_PlayInEditor )
	{
		debugf( NAME_Warning, TEXT("PIE object was selected: \"%s\""), *InObject->GetFullName() );
	}

	const UBOOL bSelectionChanged = !InObject->IsSelected();
	InObject->SetFlags( RF_EdSelected );

	// Add to selected lists.
	SelectedObjects.AddUniqueItem( InObject );
	SelectedClasses.AddUniqueItem( InObject->GetClass() );

	// Call this after the item has been added from the selection set.
	GCallbackEvent->Send( CALLBACK_SelectObject, InObject );

	if ( bSelectionChanged )
	{
		MarkBatchDirty();
		if ( !IsBatchSelecting() )
		{
			// new version - includes which selection set was modified
			GCallbackEvent->Send(CALLBACK_SelChange, this);
		}
	}
}

/**
 * Deselects the specified object.
 *
 * @param	InObject	The object to deselect.  Must be non-NULL.
 */
void USelection::Deselect(UObject* InObject)
{
	check( InObject );

	const UBOOL bSelectionChanged = InObject->IsSelected();
	InObject->ClearFlags( RF_EdSelected );

	// Remove from selected list.
	SelectedObjects.RemoveItem( InObject );

	// Call this after the item has been removed from the selection set.
	GCallbackEvent->Send(CALLBACK_SelectObject, InObject);

	if ( bSelectionChanged )
	{
		MarkBatchDirty();
		if ( !IsBatchSelecting() )
		{
			// new version - includes which selection set was modified
			GCallbackEvent->Send(CALLBACK_SelChange, this);
		}
	}
}

/**
 * Selects or deselects the specified object, depending on the value of the bSelect flag.
 *
 * @param	InObject	The object to select/deselect.  Must be non-NULL.
 * @param	bSelect		TRUE selects the object, FALSE deselects.
 */
void USelection::Select(UObject* InObject, UBOOL bSelect)
{
	if( bSelect )
	{
		Select( InObject );
	}
	else
	{
		Deselect( InObject );
	}
}

/**
 * Toggles the selection state of the specified object.
 *
 * @param	InObject	The object to select/deselect.  Must be non-NULL.
 */
void USelection::ToggleSelect( UObject* InObject )
{
	Select( InObject, !InObject->IsSelected() );
}

/**
 * Deselects all objects.
 */
void USelection::DeselectAll()
{
	const UBOOL bSelectionChanged = (SelectedObjects.Num() > 0);

	for ( INT i = 0 ; i < SelectedObjects.Num() ; ++i )
	{
		UObject* Obj = SelectedObjects( i );
		if( Obj )
		{
			Obj->ClearFlags( RF_EdSelected );
			GCallbackEvent->Send(CALLBACK_SelectObject, Obj);
		}
	}

	SelectedObjects.Empty();

	if ( bSelectionChanged )
	{
		MarkBatchDirty();
		if ( !IsBatchSelecting() )
		{
			GCallbackEvent->Send(CALLBACK_SelChange);

			// new version - includes which selection set was modified
			GCallbackEvent->Send(CALLBACK_SelChange, this);
		}
	}
}

/**
 * Deselects all objects of the specified class.
 *
 * @param	InClass		The type of object to deselect.  Must be non-NULL.
 * @param	InFlags		[opt] Flags that the object must have if it is to be deselected.  Ignored if 0.
 */
void USelection::SelectNone( UClass* InClass, EObjectFlags InFlags )
{
	check( InClass );

	// Fast path for deselecting all UObjects with any flags
	if ( InClass == UObject::StaticClass() && InFlags == 0 )
	{
		DeselectAll();
	}
	// We want specific classes and/or flags - check each object.
	else
	{
		UBOOL bSelectionChanged = FALSE;

		// Remove from the end to minimize memmoves.
		for ( INT i = SelectedObjects.Num()-1 ; i >= 0 ; --i )
		{
			UObject* Object = SelectedObjects( i );
			// Remove NULLs from SelectedObjects array.
			if ( !Object )
			{
				SelectedObjects.Remove( i );
			}
			// If its the right class and has the right flags. 
			// Note that if InFlags is 0, HasAllFlags(0) always returns true.
			else if( Object->IsA( InClass ) && Object->HasAllFlags(InFlags) )
			{
				Object->ClearFlags( RF_EdSelected );
				SelectedObjects.Remove( i );

				// Call this after the item has been removed from the selection set.
				GCallbackEvent->Send(CALLBACK_SelectObject,Object);

				bSelectionChanged = TRUE;
			}
		}

		if ( bSelectionChanged )
		{
			MarkBatchDirty();
			if ( !IsBatchSelecting() )
			{
				GCallbackEvent->Send(CALLBACK_SelChange);

				// new version - includes which selection set was modified
				GCallbackEvent->Send(CALLBACK_SelChange, this);
			}
		}
	}
}

/**
 * If batch selection is active, sets flag indicating something actually changed.
 */
void USelection::MarkBatchDirty()
{
	if ( IsBatchSelecting() )
	{
		bIsBatchDirty = TRUE;
	}
}


/**
 * Returns TRUE if the specified object is non-NULL and selected.
 *
 * @param	InObject	The object to query.  Can be NULL.
 * @return				TRUE if the object is selected, or FALSE if InObject is unselected or NULL.
 */
UBOOL USelection::IsSelected(UObject* InObject) const
{
	if ( InObject )
	{
		return SelectedObjects.ContainsItem(InObject);
	}

	return FALSE;
}

/**
 * Sync's all objects' RF_EdSelected flag based on the current selection list.
 */
void USelection::RefreshObjectFlags()
{
	for( INT Index = 0 ; Index < SelectedObjects.Num() ; ++Index )
	{
		if ( SelectedObjects(Index) )
		{
			SelectedObjects(Index)->SetFlags( RF_EdSelected );
		}
	}
}

/**
 * Serialize function used to serialize object references to to avoid stale
 * object pointers.
 *
 * @param	Ar		The archive to serialize with.
 */
void USelection::Serialize(FArchive& Ar)
{
	Super::Serialize( Ar );
	Ar << SelectedObjects;
}
