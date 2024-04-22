/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnEdTran.h"
#include "BSPOps.h"

/*-----------------------------------------------------------------------------
	A single transaction.
-----------------------------------------------------------------------------*/

void FTransaction::FObjectRecord::SerializeContents( FArchive& Ar, INT InOper )
{
	if( Array )
	{
		//debugf( "Array %s %i*%i: %i",Object->GetFullName(),Index,ElementSize,InOper);
		check((SIZE_T)Array>=(SIZE_T)Object+sizeof(UObject));
		check((SIZE_T)Array+sizeof(FScriptArray)<=(SIZE_T)Object+Object->GetClass()->GetPropertiesSize());
		check(ElementSize!=0);
		check(Serializer!=NULL);
		check(Index>=0);
		check(Count>=0);
		if( InOper==1 )
		{
			// "Saving add order" or "Undoing add order" or "Redoing remove order".
			if( Ar.IsLoading() )
			{
				checkSlow(Index+Count<=Array->Num());
				for( INT i=Index; i<Index+Count; i++ )
				{
					Destructor( (BYTE*)Array->GetData() + i*ElementSize );
				}
				Array->Remove( Index, Count, ElementSize );
			}
		}
		else
		{
			// "Undo/Redo Modify" or "Saving remove order" or "Undoing remove order" or "Redoing add order".
			if( InOper==-1 && Ar.IsLoading() )
			{
				Array->Insert( Index, Count, ElementSize );
				appMemzero( (BYTE*)Array->GetData() + Index*ElementSize, Count*ElementSize );
			}

			// Serialize changed items.
			check(Index+Count<=Array->Num());
			for( INT i=Index; i<Index+Count; i++ )
			{
				Serializer( Ar, (BYTE*)Array->GetData() + i*ElementSize );
			}
		}
	}
	else
	{
		//debugf( "Object %s",Object->GetFullName());
		check(Index==0);
		check(ElementSize==0);
		check(Serializer==NULL);
		Object->Serialize( Ar );
	}
}
void FTransaction::FObjectRecord::Restore( FTransaction* Owner )
{
	if( !Restored )
	{
		Restored = 1;
		TArray<BYTE> FlipData;
		TArray<UObject*> FlipReferencedObjects;
		TArray<FName> FlipReferencedNames;
		if( Owner->Flip )
		{
			FWriter Writer( FlipData, FlipReferencedObjects, FlipReferencedNames );
			SerializeContents( Writer, -Oper );
		}
		FTransaction::FObjectRecord::FReader Reader( Owner, Data, ReferencedObjects, ReferencedNames );
		SerializeContents( Reader, Oper );
		if( Owner->Flip )
		{
			Exchange( Data, FlipData );
			Exchange( ReferencedObjects, FlipReferencedObjects );
			Exchange( ReferencedNames, FlipReferencedNames );
			Oper *= -1;
		}
	}
}

void FTransaction::RemoveRecords( INT Count /* = 1  */ )
{
	if ( Records.Num() >= Count )
	{
		Records.Remove( Records.Num() - Count, Count );

		// Kill our object maps that are used to track redundant saves
		ObjectMap.Empty();
	}
}

/**
 * Outputs the contents of the ObjectMap to the specified output device.
 */
void FTransaction::DumpObjectMap(FOutputDevice& Ar) const
{
	Ar.Logf( TEXT("===== DumpObjectMap %s ==== "), *Title );
	for ( ObjectMapType::TConstIterator It(ObjectMap) ; It ; ++It )
	{
		const UObject* CurrentObject	= It.Key();
		const INT SaveCount				= It.Value();
		Ar.Logf( TEXT("%i\t: %s"), SaveCount, *CurrentObject->GetPathName() );
	}
	Ar.Logf( TEXT("=== EndDumpObjectMap %s === "), *Title );
}

FArchive& operator<<( FArchive& Ar, FTransaction::FObjectRecord& R )
{
	check(R.Object);
	FMemMark Mark(GMainThreadMemStack);
	Ar << R.Object;
	Ar << R.Data;
	Ar << R.ReferencedObjects;
	Ar << R.ReferencedNames;
	Mark.Pop();
	return Ar;
}


// FTransactionBase interface.
void FTransaction::SaveObject( UObject* Object )
{
	check(Object);

	INT* SaveCount = ObjectMap.Find(Object);
	if ( !SaveCount )
	{
		ObjectMap.Set(Object,1);
		// Save the object.
		new( Records )FObjectRecord( this, Object, NULL, 0, 0, 0, 0, NULL, NULL );
	}
	else
	{
		++(*SaveCount);
	}
}

void FTransaction::SaveArray( UObject* Object, FScriptArray* Array, INT Index, INT Count, INT Oper, INT ElementSize, STRUCT_AR Serializer, STRUCT_DTOR Destructor )
{
	check(Object);
	check(Array);
	check(ElementSize);
	check(Serializer);
	check(Object->IsValid());
	check((SIZE_T)Array>=(SIZE_T)Object);
	check((SIZE_T)Array+sizeof(FScriptArray)<=(SIZE_T)Object+Object->GetClass()->PropertiesSize);
	check(Index>=0);
	check(Count>=0);
	check(Index+Count<=Array->Num());

	// don't serialize the array if the object is contained within a PIE package
	if( Object->HasAnyFlags(RF_Transactional) && (Object->GetOutermost()->PackageFlags&PKG_PlayInEditor) == 0 )
	{
		// Save the array.
		new( Records )FObjectRecord( this, Object, Array, Index, Count, Oper, ElementSize, Serializer, Destructor );
	}
}

/**
 * Enacts the transaction.
 */
void FTransaction::Apply()
{
	checkSlow(Inc==1||Inc==-1);

	// Figure out direction.
	const INT Start = Inc==1 ? 0             : Records.Num()-1;
	const INT End   = Inc==1 ? Records.Num() :              -1;

	// Init objects.
	TArray<UObject*> ChangedObjects;
	for( INT i=Start; i!=End; i+=Inc )
	{
		Records(i).Restored = 0;
		if(ChangedObjects.FindItemIndex(Records(i).Object) == INDEX_NONE)
		{
			Records(i).Object->PreEditUndo();
			ChangedObjects.AddItem(Records(i).Object);
		}
	}
	for( INT i=Start; i!=End; i+=Inc )
	{
		Records(i).Restore( this );
	}

	NumModelsModified = 0;		// Count the number of UModels that were changed.
	for(INT ObjectIndex = 0;ObjectIndex < ChangedObjects.Num();ObjectIndex++)
	{
		UObject* ChangedObject = ChangedObjects(ObjectIndex);
		UModel* Model = Cast<UModel>(ChangedObject);
		if( Model && Model->Nodes.Num() )
		{
			FBSPOps::bspBuildBounds( Model );
			++NumModelsModified;
		}
		ChangedObject->PostEditUndo();
	}

	// Flip it.
	if( Flip )
	{
		Inc *= -1;
	}
}

SIZE_T FTransaction::DataSize() const
{
	SIZE_T Result=0;
	for( INT i=0; i<Records.Num(); i++ )
	{
		Result += Records(i).Data.Num();
	}
	return Result;
}

/**
 * Get all the objects that are part of this transaction.
 * @param	Objects		[out] Receives the object list.  Previous contents are cleared.
 */
void FTransaction::GetTransactionObjects(TArray<UObject*>& Objects)
{
	Objects.Empty(); // Just in case.

	for(INT i=0; i<Records.Num(); i++)
	{
		UObject* obj = Records(i).Object;
		if(obj)
		{
			Objects.AddUniqueItem(obj);
		}
	}
}


/*-----------------------------------------------------------------------------
	Transaction tracking system.
-----------------------------------------------------------------------------*/

UTransBuffer::UTransBuffer( SIZE_T InMaxMemory )
	:	MaxMemory( InMaxMemory )
{
	// Reset.
	Reset( TEXT("startup") );
	CheckState();

	debugf( NAME_Init, TEXT("Transaction tracking system initialized") );
}

// UObject interface.
void UTransBuffer::Serialize( FArchive& Ar )
{
	check( !Ar.IsPersistent() );

	CheckState();

	// Handle garbage collection.
	Super::Serialize( Ar );

	// We cannot support undoing across GC if we allow it to eliminate references so we need
	// to suppress it.
	if ( IsObjectSerializationEnabled() || !Ar.IsObjectReferenceCollector() )
	{
		Ar.AllowEliminatingReferences( FALSE );
		Ar << UndoBuffer;
		Ar.AllowEliminatingReferences( TRUE );
	}
	Ar << ResetReason << UndoCount << ActiveCount;

	CheckState();
}
void UTransBuffer::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		CheckState();
		debugf( NAME_Exit, TEXT("Transaction tracking system shut down") );
	}
	Super::FinishDestroy();
}

/**
 * Begins a new undo transaction.  An undo transaction is defined as all actions
 * which take place when the user selects "undo" a single time.
 * If there is already an active transaction in progress, increments that transaction's
 * action counter instead of beginning a new transaction.
 * 
 * @param	SessionName		the name for the undo session;  this is the text that 
 *							will appear in the "Edit" menu next to the Undo item
 *
 * @return	Number of active actions when Begin() was called;  values greater than
 *			0 indicate that there was already an existing undo transaction in progress.
 */
INT UTransBuffer::Begin(const TCHAR* SessionName)
{
	CheckState();
	const INT Result = ActiveCount;
	if( ActiveCount++==0 )
	{
		// Cancel redo buffer.
		if( UndoCount )
		{
			UndoBuffer.Remove( UndoBuffer.Num()-UndoCount, UndoCount );
		}
		UndoCount = 0;

		// Purge previous transactions if too much data occupied.
		while( GetUndoSize() > MaxMemory )
		{
			UndoBuffer.Remove( 0 );
		}

		// Begin a new transaction.
		GUndo = new(UndoBuffer)FTransaction( SessionName, 1 );
	}
	CheckState();
	return Result;
}

/**
 * Attempts to close an undo transaction.  Only successful if the transaction's action
 * counter is 1.
 * 
 * @return	Number of active actions when End() was called; a value of 1 indicates that the
 *			transaction was successfully closed
 */
INT UTransBuffer::End()
{
	CheckState();
	const INT Result = ActiveCount;
	// Don't assert as we now purge the buffer when resetting.
	// So, the active count could be 0, but the code path may still call end.
	if (ActiveCount >= 1)
	{
		if( --ActiveCount==0 )
		{
#if 0 // @todo DB: please don't remove this code -- thanks! :)
			// End the current transaction.
			if ( GUndo && GLog )
			{
				// @todo DB: Fix this potentially unsafe downcast.
				static_cast<FTransaction*>(GUndo)->DumpObjectMap( *GLog );
			}
#endif
			GUndo = NULL;
		}
		CheckState();
	}
	return ActiveCount;
}

/**
 * Resets the entire undo buffer;  deletes all undo transactions.
 */
void UTransBuffer::Reset( const TCHAR* Reason )
{
	CheckState();

	if( ActiveCount != 0 )
	{
		FString ErrorMessage = TEXT("");
		ErrorMessage += FString::Printf(TEXT("Non zero active count in UTransBuffer::Reset") LINE_TERMINATOR );
		ErrorMessage += FString::Printf(TEXT("ActiveCount : %d"	) LINE_TERMINATOR, ActiveCount );
		ErrorMessage += FString::Printf(TEXT("SessionName : %s"	) LINE_TERMINATOR, *GetUndoDesc(FALSE) );
		ErrorMessage += FString::Printf(TEXT("Reason      : %s"	) LINE_TERMINATOR, Reason );

		debugf(*ErrorMessage);

		ErrorMessage += FString::Printf( LINE_TERMINATOR );
		ErrorMessage += FString::Printf(TEXT("Purging the undo buffer...") LINE_TERMINATOR );

		appMsgf(AMT_OK, *ErrorMessage);

		// Clear out the transaction buffer...
		Cancel(0);
	}

	// Reset all transactions.
	UndoBuffer.Empty();
	UndoCount    = 0;
	ResetReason  = Reason;
	ActiveCount  = 0;

	CheckState();
}

/**
 * Cancels the current transaction, no longer capture actions to be placed in the undo buffer.
 *
 * @param	StartIndex	the value of ActiveIndex when the transaction to be cancelled was began. 
 */
void UTransBuffer::Cancel( INT StartIndex /*=0*/ )
{
	CheckState();

	// if we don't have any active actions, we shouldn't have an active transaction at all
	if ( ActiveCount > 0 )
	{
		if ( StartIndex == 0 )
		{
			// clear the global pointer to the soon-to-be-deleted transaction
			GUndo = NULL;
			
			// remove the currently active transaction from the buffer
			UndoBuffer.Pop();
		}
		else
		{
			FTransaction& Transaction = UndoBuffer.Last();
			Transaction.RemoveRecords(ActiveCount - StartIndex);
		}

		// reset the active count
		ActiveCount = StartIndex;
	}

	CheckState();
}

/**
 * Determines whether the undo option should be selectable.
 * 
 * @param	Str		[out] the reason that undo is disabled
 *
 * @return	TRUE if the "Undo" option should be selectable.
 */
UBOOL UTransBuffer::CanUndo( FString* Str )
{
	CheckState();
	if( ActiveCount )
	{
		if( Str )
		{
			*Str = TEXT("(Can't undo during a transaction)");
		}
		return FALSE;
	}
	if( UndoBuffer.Num()==UndoCount )
	{
		if( Str )
		{
			*Str = US + TEXT("(Can't undo after ") + ResetReason + TEXT(")");
		}
		return FALSE;
	}
	return TRUE;
}

/**
 * Determines whether the redo option should be selectable.
 * 
 * @param	Str		[out] the reason that redo is disabled
 *
 * @return	TRUE if the "Redo" option should be selectable.
 */
UBOOL UTransBuffer::CanRedo( FString* Str )
{
	CheckState();
	if( ActiveCount )
	{
		if( Str )
		{
			*Str = TEXT("(Can't redo during a transaction)");
		}
		return 0;
	}
	if( UndoCount==0 )
	{
		if( Str )
		{
			*Str = TEXT("(Nothing to redo)");
		}
		return 0;
	}
	return 1;
}

/**
 * Returns the description of the undo action that will be performed next.
 * This is the text that is shown next to the "Undo" item in the menu.
 * 
 * @param	bCheckWhetherUndoPossible	Perform test whether undo is possible and return Error if not and option is set
 *
 * @return	text describing the next undo transaction
 */
FString UTransBuffer::GetUndoDesc( UBOOL bCheckWhetherUndoPossible )
{
	FString Title;
	if( bCheckWhetherUndoPossible && !CanUndo( &Title ) )
	{
		return *Title;
	}

	const FTransaction* Transaction = &UndoBuffer( UndoBuffer.Num() - (UndoCount + 1) );
	return Transaction->GetTitle();
}

/**
 * Returns the description of the redo action that will be performed next.
 * This is the text that is shown next to the "Redo" item in the menu.
 * 
 * @return	text describing the next redo transaction
 */
FString UTransBuffer::GetRedoDesc()
{
	FString Title;
	if( !CanRedo( &Title ) )
	{
		return *Title;
	}

	const FTransaction* Transaction = &UndoBuffer( UndoBuffer.Num() - UndoCount );
	return Transaction->GetTitle();
}

/**
 * Executes an undo transaction, undoing all actions contained by that transaction.
 * 
 * @return				TRUE if the transaction was successfully undone
 */
UBOOL UTransBuffer::Undo()
{
	CheckState();
	if( !CanUndo() )
	{
		return FALSE;
	}

	// Apply the undo changes.
	FTransaction& Transaction = UndoBuffer( UndoBuffer.Num() - ++UndoCount );

	debugf( TEXT("Undo %s"), Transaction.GetTitle() );
	Transaction.Apply();

	CheckState();

	return TRUE;
}

/**
 * Executes an redo transaction, redoing all actions contained by that transaction.
 * 
 * @return				TRUE if the transaction was successfully redone
 */
UBOOL UTransBuffer::Redo()
{
	CheckState();
	if( !CanRedo() )
	{
		return FALSE;
	}

	// Apply the redo changes.
	FTransaction& Transaction = UndoBuffer( UndoBuffer.Num() - UndoCount-- );

	debugf( TEXT("Redo %s"), Transaction.GetTitle() );
	Transaction.Apply();

	CheckState();

	return TRUE;
}

/**
 * Enables the transaction buffer to serialize the set of objects it references.
 *
 * @return	TRUE if the transaction buffer is able to serialize object references.
 */
UBOOL UTransBuffer::EnableObjectSerialization()
{
	return --DisallowObjectSerialization == 0;
}

/**
 * Disables the transaction buffer from serializing the set of objects it references.
 *
 * @return	TRUE if the transaction buffer is able to serialize object references.
 */
UBOOL UTransBuffer::DisableObjectSerialization()
{
	return ++DisallowObjectSerialization == 0;
}

FTransactionBase* UTransBuffer::CreateInternalTransaction()
{
	return new FTransaction( TEXT("Internal"), 0 );
}

/**
 * Determines the amount of data currently stored by the transaction buffer.
 *
 * @return	number of bytes stored in the undo buffer
 */
SIZE_T UTransBuffer::GetUndoSize() const
{
	SIZE_T Result=0;
	for( INT i=0; i<UndoBuffer.Num(); i++ )
	{
		Result += UndoBuffer(i).DataSize();
	}
	return Result;
}

/**
 * Checks if we have exceeded our buffer's max memory.
 *
 * @return True if we have exceeded our buffer's max memory.
 */
UBOOL UTransBuffer::IsTransactionBufferBreeched() const
{
	if(GetUndoSize() > MaxMemory)
	{
		return TRUE;
	}
	return FALSE;
}

/**
 * Returns amount of free space in the transaction buffer.
 *
 * @return The amount of free space in the transaction buffer.
 */
INT UTransBuffer::GetBufferFreeSpace() const
{
	return MaxMemory - GetUndoSize();	
}

/**
 * Returns the last transaction size.
 *
 * @return The transaction size.
 */
INT UTransBuffer::GetLastTransactionSize()
{
	return UndoBuffer(UndoBuffer.Num() - 1).DataSize();
}

/**
 * Validates the state of the transaction buffer.
 */
void UTransBuffer::CheckState() const
{
	// Validate the internal state.
	check(UndoBuffer.Num()>=UndoCount);
	check(ActiveCount>=0);
}

IMPLEMENT_CLASS(UTransBuffer);
IMPLEMENT_CLASS(UTransactor);

/*-----------------------------------------------------------------------------
	Allocator.
-----------------------------------------------------------------------------*/

UTransactor* UEditorEngine::CreateTrans()
{
	INT UndoBufferSize;
	if( !GConfig->GetInt( TEXT("Undo"), TEXT("UndoBufferSize"), UndoBufferSize, GEditorUserSettingsIni ) )
	{
		UndoBufferSize = 16;
	}
	return new UTransBuffer( UndoBufferSize*1024*1024 );
}
