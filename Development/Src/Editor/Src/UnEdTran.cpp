/*=============================================================================
	UnEdTran.cpp: Unreal transaction-tracking functions.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	A single transaction.
-----------------------------------------------------------------------------*/

void FTransaction::FObjectRecord::SerializeContents( FArchive& Ar, INT InOper )
{
	if( Array )
	{
		//debugf( "Array %s %i*%i: %i",Object->GetFullName(),Index,ElementSize,InOper);
		checkSlow((SIZE_T)Array>=(SIZE_T)Object+sizeof(UObject));
		checkSlow((SIZE_T)Array+sizeof(FArray)<=(SIZE_T)Object+Object->GetClass()->GetPropertiesSize());
		checkSlow(ElementSize!=0);
		checkSlow(Serializer!=NULL);
		checkSlow(Index>=0);
		checkSlow(Count>=0);
		if( InOper==1 )
		{
			// "Saving add order" or "Undoing add order" or "Redoing remove order".
			if( Ar.IsLoading() )
			{
				checkSlow(Index+Count<=Array->Num());
				for( INT i=Index; i<Index+Count; i++ )
					Destructor( (BYTE*)Array->GetData() + i*ElementSize );
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
			checkSlow(Index+Count<=Array->Num());
			for( INT i=Index; i<Index+Count; i++ )
				Serializer( Ar, (BYTE*)Array->GetData() + i*ElementSize );
		}
	}
	else
	{
		//debugf( "Object %s",Object->GetFullName());
		checkSlow(Index==0);
		checkSlow(ElementSize==0);
		checkSlow(Serializer==NULL);
		Object->Serialize( Ar );
	}
}
void FTransaction::FObjectRecord::Restore( FTransaction* Owner )
{
	if( !Restored )
	{
		Restored = 1;
		TArray<BYTE> FlipData;
		if( Owner->Flip )
		{
			FWriter Writer( FlipData );
			SerializeContents( Writer, -Oper );
		}
		FTransaction::FObjectRecord::FReader Reader( Owner, Data );
		SerializeContents( Reader, Oper );
		if( Owner->Flip )
		{
			ExchangeArray( Data, FlipData );
			Oper *= -1;
		}
	}
}

FArchive& operator<<( FArchive& Ar, FTransaction::FObjectRecord& R )
{
	checkSlow(R.Object);
	FMemMark Mark(GMem);
	Ar << R.Object;
	FTransaction::FObjectRecord::FReader Reader( NULL, R.Data );
	if( !R.Array )
	{
		//warning: Relies on the safety of calling UObject::Serialize
		// on pseudoobjects.
		UClass*  Class        = R.Object->GetClass();
		UObject* PseudoObject = (UObject*)New<BYTE>(GMem,Class->GetPropertiesSize());
		PseudoObject->InitClassDefaultObject( Class );
		Class->ClassConstructor( PseudoObject );
		PseudoObject->Serialize( Reader );
		PseudoObject->Serialize( Ar );
		PseudoObject->~UObject();
	}
	else if( R.Data.Num() )
	{
		checkSlow(R.Serializer);
		FArray* Temp = (FArray*)NewZeroed<BYTE>(GMem,R.ElementSize);
		for( INT i=R.Index; i<R.Index+R.Count; i++ )
		{
			appMemzero( Temp, R.ElementSize );
			R.Serializer( Reader, Temp );
			R.Serializer( Ar,     Temp );
			R.Destructor( Temp );
		}
	}
	Mark.Pop();
	return Ar;
}


// FTransactionBase interface.
void FTransaction::SaveObject( UObject* Object )
{
	check(Object);

	// Save the object.
	new( Records )FObjectRecord( this, Object, NULL, 0, 0, 0, 0, NULL, NULL );

}
void FTransaction::SaveArray( UObject* Object, FArray* Array, INT Index, INT Count, INT Oper, INT ElementSize, STRUCT_AR Serializer, STRUCT_DTOR Destructor )
{
	checkSlow(Object);
	checkSlow(Array);
	checkSlow(ElementSize);
	checkSlow(Serializer);
	checkSlow(Object->IsValid());
	checkSlow((SIZE_T)Array>=(SIZE_T)Object);
	checkSlow((SIZE_T)Array+sizeof(FArray)<=(SIZE_T)Object+Object->GetClass()->PropertiesSize);
	checkSlow(Index>=0);
	checkSlow(Count>=0);
	checkSlow(Index+Count<=Array->Num());

	if(Object->GetFlags() & RF_Transactional)
	{
		// Save the array.
		new( Records )FObjectRecord( this, Object, Array, Index, Count, Oper, ElementSize, Serializer, Destructor );
	}
}
void FTransaction::Apply()
{
	checkSlow(Inc==1||Inc==-1);

	// Figure out direction.
	INT Start = Inc==1 ? 0             : Records.Num()-1;
	INT End   = Inc==1 ? Records.Num() :              -1;

	// Init objects.
	TArray<UObject*> ChangedObjects;
	for( INT i=Start; i!=End; i+=Inc )
	{
		Records(i).Restored = 0;
		Records(i).Object->SetFlags( RF_NeedPostLoad );
		Records(i).Object->ClearFlags( RF_Destroyed );
		if(ChangedObjects.FindItemIndex(Records(i).Object) != INDEX_NONE)
		{
			Records(i).Object->PreEditChange();
			ChangedObjects.AddItem(Records(i).Object);
		}
	}
	for( INT i=Start; i!=End; i+=Inc )
	{
		Records(i).Restore( this );
	}
	for( INT i=Start; i!=End; i+=Inc )
	{
		if( Records(i).Object->GetFlags() & RF_NeedPostLoad )
		{
			Records(i).Object->ConditionalPostLoad();
			UModel* Model = Cast<UModel>(Records(i).Object);
			if( Model )
				if( Model->Nodes.Num() )
					GEditor->bspBuildBounds( Model );
		}
	}
	for(INT ObjectIndex = 0;ObjectIndex < ChangedObjects.Num();ObjectIndex++)
	{
		ChangedObjects(ObjectIndex)->PostEditChange(NULL);
	}

	// Flip it.
	if( Flip )
		Inc *= -1;
}


// Get all the objects that are part of this transaction.
void FTransaction::GetTransactionObjects(TArray<UObject*>& Objects)
{
	Objects.Empty(); // Just in case.

	for(INT i=0; i<Records.Num(); i++)
	{
		UObject* obj = Records(i).Object;
		if(obj)
			Objects.AddUniqueItem(obj);
	}
}


/*-----------------------------------------------------------------------------
	Transaction tracking system.
-----------------------------------------------------------------------------*/

// UObject interface.
void UTransBuffer::Serialize( FArchive& Ar )
{
	CheckState();

	// Handle garbage collection.
	Super::Serialize( Ar );
	Ar << UndoBuffer << ResetReason << UndoCount << ActiveCount << Overflow;

	CheckState();
}
void UTransBuffer::Destroy()
{
	CheckState();
	debugf( NAME_Exit, TEXT("Transaction tracking system shut down") );
	Super::Destroy();
}

// UTransactor interface.
void UTransBuffer::Reset( const TCHAR* Reason )
{
	CheckState();

	check(ActiveCount==0);

	// Reset all transactions.
	UndoBuffer.Empty();
	UndoCount    = 0;
	ResetReason  = Reason;
	ActiveCount  = 0;
	Overflow     = 0;

	CheckState();
}
void UTransBuffer::Begin( const TCHAR* SessionName )
{
	GEditor->GetLevel()->MarkPackageDirty();

	CheckState();
	if( ActiveCount++==0 )
	{
		// Cancel redo buffer.
		//debugf(TEXT("BeginTrans %s"), SessionName);
		if( UndoCount )
			UndoBuffer.Remove( UndoBuffer.Num()-UndoCount, UndoCount );
		UndoCount = 0;

		// Purge previous transactions if too much data occupied.
		while( UndoDataSize() > MaxMemory )
			UndoBuffer.Remove( 0 );

		// Begin a new transaction.
		GUndo = new(UndoBuffer)FTransaction( SessionName, 1 );
		Overflow = 0;
	}
	CheckState();
}
void UTransBuffer::End()
{
	CheckState();
	check(ActiveCount>=1);
	if( --ActiveCount==0 )
	{
		// End the current transaction.
		//debugf(TEXT("EndTrans"));
		//FTransaction& Trans = UndoBuffer.Last();
		GUndo = NULL;
	}
	CheckState();
}
void UTransBuffer::Continue()
{
	CheckState();
	if( ActiveCount==0 && UndoBuffer.Num()>0 && UndoCount==0 )
	{
		// Continue the previous transaction.
		ActiveCount++;
		GUndo = &UndoBuffer.Last();
	}
	CheckState();
}
UBOOL UTransBuffer::CanUndo( FString* Str )
{
	CheckState();
	if( ActiveCount )
	{
		if( Str )
			*Str = TEXT("Can't undo during a transaction");
		return 0;
	}
	if( UndoBuffer.Num()==UndoCount )
	{
		if( Str )
			*Str = US + TEXT("Can't undo after ") + ResetReason;
		return 0;
	}
	return 1;
}
UBOOL UTransBuffer::CanRedo( FString* Str )
{
	CheckState();
	if( ActiveCount )
	{
		if( Str )
			*Str = TEXT("Can't redo during a transaction");
		return 0;
	}
	if( UndoCount==0 )
	{
		if( Str )
			*Str = TEXT("Nothing to redo");
		return 0;
	}
	return 1;
}

// Returns the description of the undo action that will be performed next

FString UTransBuffer::GetUndoDesc()
{
	FString Title;
	if( !CanUndo( &Title ) )
		return *Title;

	FTransaction* Transaction = &UndoBuffer( UndoBuffer.Num() - (UndoCount + 1) );
	return Transaction->GetTitle();

}

// Returns the description of the redo action that will be performed next

FString UTransBuffer::GetRedoDesc()
{
	FString Title;
	if( !CanRedo( &Title ) )
		return *Title;

	FTransaction* Transaction = &UndoBuffer( UndoBuffer.Num() - UndoCount );
	return Transaction->GetTitle();

}

UBOOL UTransBuffer::Undo()
{
	CheckState();
	if( !CanUndo() )
		return 0;

	// Apply the undo changes.
	FTransaction& Transaction = UndoBuffer( UndoBuffer.Num() - ++UndoCount );

	TArray<UObject*> TransObjects;
	Transaction.GetTransactionObjects(TransObjects);

	for(INT i=0; i<TransObjects.Num(); i++)
	{
		UObject* T = TransObjects(i);
		TransObjects(i)->PreEditUndo();
	}

	debugf( TEXT("Undo %s"), Transaction.GetTitle() );
	Transaction.Apply();
	FinishDo();

	CheckState();

	// Run through all transaction records and call PreEditUndo on each UObject.
	for(INT i=0; i<TransObjects.Num(); i++)
	{
		TransObjects(i)->PostEditUndo();
	}

	return 1;
}
UBOOL UTransBuffer::Redo()
{
	CheckState();
	if( !CanRedo() )
		return 0;

	// Apply the redo changes.
	FTransaction& Transaction = UndoBuffer( UndoBuffer.Num() - UndoCount-- );

	TArray<UObject*> TransObjects;
	Transaction.GetTransactionObjects(TransObjects);

	for(INT i=0; i<TransObjects.Num(); i++)
	{
		TransObjects(i)->PreEditUndo();
	}

	debugf( TEXT("Redo %s"), Transaction.GetTitle() );
	Transaction.Apply();
	FinishDo();

	CheckState();

	for(INT i=0; i<TransObjects.Num(); i++)
	{
		TransObjects(i)->PostEditUndo();
	}

	return 1;
}
FTransactionBase* UTransBuffer::CreateInternalTransaction()
{
	return new FTransaction( TEXT("Internal"), 0 );
}

// Functions.
void UTransBuffer::FinishDo()
{
	GEditor->NoteSelectionChange( GEditor->Level );
}
SIZE_T UTransBuffer::UndoDataSize()
{
	SIZE_T Result=0;
	for( INT i=0; i<UndoBuffer.Num(); i++ )
		Result += UndoBuffer(i).DataSize();
	return Result;
}
void UTransBuffer::CheckState()
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
	return new UTransBuffer( 8*1024*1024 );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
