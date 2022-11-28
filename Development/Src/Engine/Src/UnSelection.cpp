
#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	FSelectionTools
-----------------------------------------------------------------------------*/

FSelectionTools::FSelectionTools()
{
	SelectedClasses.MaxItems = 5;
}

FSelectionTools::~FSelectionTools()
{
	SelectedObjects.Empty();
	SelectedClasses.Empty();
}

void FSelectionTools::Select( UObject* InObject, UBOOL InSelect )
{
	if( InSelect )
	{
		InObject->SetFlags( RF_EdSelected );
		GCallback->Send( CALLBACK_SelectObject, InObject );

		// add to selected list
		SelectedObjects.AddUniqueItem(InObject);

		SelectedClasses.AddUniqueItem( InObject->GetClass() );
	}
	else
	{
		InObject->ClearFlags( RF_EdSelected );

		// remove from selected list
		SelectedObjects.RemoveItem(InObject);
	}
}

void FSelectionTools::ToggleSelect( UObject* InObject )
{
	if( InObject->IsSelected() )
	{
		InObject->ClearFlags( RF_EdSelected );
		// remove from selected list
		SelectedObjects.RemoveItem(InObject);
	}
	else
	{
		InObject->SetFlags( RF_EdSelected );
		GCallback->Send( CALLBACK_SelectObject, InObject );
		// add to selected list
		SelectedObjects.AddUniqueItem(InObject);
	}
}

UBOOL FSelectionTools::IsSelected( UObject* InObject )
{
	return InObject->IsSelected();
}

void FSelectionTools::SelectNone( UClass* InClass )
{
	for( TSelectedObjectIterator It(InClass) ; It ; ++It )
	{
		It->ClearFlags( RF_EdSelected );
		// remove from selected list
		SelectedObjects.RemoveItem(*It);
	}
}

// Looks at all the selected actors and determines if they
// should use grid snapping when being moved.

UBOOL FSelectionTools::ShouldSnapMovement()
{
	for( TSelectedActorIterator It(GEngine->GetLevel()) ; It ; ++It )
	{
		// If any actor in the selection requires snapping, they all need to be snapped.

		if( It->bEdShouldSnap )
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Clears the selected list and rebuilds it from scratch.  Note that
 * the selection order is no longer valid.
 */
void FSelectionTools::RebuildSelectedList()
{
	SelectedObjects.Empty();
	for (TSelectedObjectIterator It(UObject::StaticClass()); It; ++It)
	{
		SelectedObjects.AddUniqueItem(*It);
	}
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
