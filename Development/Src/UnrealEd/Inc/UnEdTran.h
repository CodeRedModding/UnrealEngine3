/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNEDTRAN_H__
#define __UNEDTRAN_H__

/*-----------------------------------------------------------------------------
	UTransactor.
-----------------------------------------------------------------------------*/

/**
 * Base class for tracking transactions for undo/redo.
 */
class UTransactor : public UObject
{
	DECLARE_ABSTRACT_CLASS_INTRINSIC(UTransactor,UObject,CLASS_Transient|0,UnrealEd)

	// UTransactor interface.

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
	virtual INT Begin( const TCHAR* SessionName ) PURE_VIRTUAL(UTransactor::Begin,return 0;);

	/**
	 * Attempts to close an undo transaction.  Only successful if the transaction's action
	 * counter is 1.
	 * 
	 * @return	Number of active actions when End() was called; a value of 1 indicates that the
	 *			transaction was successfully closed
	 */
	virtual INT End() PURE_VIRTUAL(UTransactor::End,return 0;);

	/**
	 * Cancels the current transaction, no longer capture actions to be placed in the undo buffer.
	 *
	 * @param	StartIndex	the value of ActiveIndex when the transaction to be cancelled was began. 
	 */
	virtual void Cancel( INT StartIndex = 0 ) PURE_VIRTUAL(UTransactor::Cancel,);

	/**
	 * Resets the entire undo buffer;  deletes all undo transactions.
	 */
	virtual void Reset( const TCHAR* Action ) PURE_VIRTUAL(UTransactor::Reset,);

	/**
	 * Returns whether there are any active actions; i.e. whether actions are currently
	 * being captured into the undo buffer.
	 */
	virtual UBOOL IsActive() PURE_VIRTUAL(UTransactor::IsActive,return FALSE;);

	/**
	 * Determines whether the undo option should be selectable.
	 * 
	 * @param	Str		[out] the reason that undo is disabled
	 *
	 * @return	TRUE if the "Undo" option should be selectable.
	 */
	virtual UBOOL CanUndo( FString* Str=NULL ) PURE_VIRTUAL(UTransactor::CanUndo,return FALSE;);
	/**
	 * Determines whether the redo option should be selectable.
	 * 
	 * @param	Str		[out] the reason that redo is disabled
	 *
	 * @return	TRUE if the "Redo" option should be selectable.
	 */
	virtual UBOOL CanRedo( FString* Str=NULL ) PURE_VIRTUAL(UTransactor::CanRedo,return FALSE;);
	/**
	 * Returns the description of the undo action that will be performed next.
	 * This is the text that is shown next to the "Undo" item in the menu.
	 * 
	 * @param	bCheckWhetherUndoPossible	Perform test whether undo is possible and return Error if not and option is set
	 *
	 * @return	text describing the next undo transaction
	 */
	virtual FString GetUndoDesc( UBOOL bCheckWhetherUndoPossible = TRUE ) PURE_VIRTUAL(UTransactor::GetUndoDesc,return TEXT(""););
	/**
	 * Returns the description of the redo action that will be performed next.
	 * This is the text that is shown next to the "Redo" item in the menu.
	 * 
	 * @return	text describing the next redo transaction
	 */
	virtual FString GetRedoDesc() PURE_VIRTUAL(UTransactor::GetRedoDesc,return TEXT(""););
	/**
	 * Executes an undo transaction, undoing all actions contained by that transaction.
	 * 
	 * @return				TRUE if the transaction was successfully undone
	 */
	virtual UBOOL Undo() PURE_VIRTUAL(UTransactor::Undo,return FALSE;);
	/**
	 * Executes an redo transaction, redoing all actions contained by that transaction.
	 * 
	 * @return				TRUE if the transaction was successfully redone
	 */
	virtual UBOOL Redo() PURE_VIRTUAL(UTransactor::Redo,return FALSE;);

	/**
	 * Checks if we have exceeded our buffer's max memory.
	 *
	 * @return True if we have exceeded our buffer's max memory.
	 */
	virtual UBOOL IsTransactionBufferBreeched() const PURE_VIRTUAL(UTransactor::IsTransactionBufferBreeched,return FALSE;);

	/**
	 * Returns amount of free space in the transaction buffer.
	 *
	 * @return The amount of free space in the transaction buffer.
	 */
	virtual INT GetBufferFreeSpace() const PURE_VIRTUAL(UTransactor::GetFreeSpace,return 0;);

	/**
	 * Returns the last transaction size.
	 *
	 * @return The transaction size.
	 */
	virtual INT GetLastTransactionSize() PURE_VIRTUAL(UTransactor::GetLastTransactionSize,return 0;);

	/**
	 * Enables the transaction buffer to serialize the set of objects it references.
	 *
	 * @return	TRUE if the transaction buffer is able to serialize object references.
	 */
	virtual UBOOL EnableObjectSerialization() { return FALSE; }

	/**
	 * Disables the transaction buffer from serializing the set of objects it references.
	 *
	 * @return	TRUE if the transaction buffer is able to serialize object references.
	 */
	virtual UBOOL DisableObjectSerialization() { return FALSE; }

	/**
	 * Wrapper for checking if the transaction buffer is allowed to serialize object references.
	 */
	virtual UBOOL IsObjectSerializationEnabled() { return FALSE; }

	virtual FTransactionBase* CreateInternalTransaction() PURE_VIRTUAL(UTransactor::CreateInternalTransaction,return NULL;);
};

/*-----------------------------------------------------------------------------
	FTransaction.
-----------------------------------------------------------------------------*/

/**
 * A single transaction, representing a set of serialized, undo-able changes to a set of objects.
 *
 * warning: The undo buffer cannot be made persistent because of its dependence on offsets
 * of arrays from their owning UObjects.
 *
 * warning: Transactions which rely on Preload calls cannot be garbage collected
 * since references to objects point to the most recent version of the object, not
 * the ordinally correct version which was referred to at the time of serialization.
 * Therefore, Preload-sensitive transactions may only be performed using
 * a temporary UTransactor::CreateInternalTransaction transaction, not a
 * garbage-collectable UTransactor::Begin transaction.
 *
 * warning: UObject::Serialize implicitly assumes that class properties do not change
 * in between transaction resets.
 */
class FTransaction : public FTransactionBase
{
protected:
	// Record of an object.
	class FObjectRecord
	{
	public:
		// Variables.
		TArray<BYTE>	Data;
		TArray<UObject*> ReferencedObjects;
		TArray<FName> ReferencedNames;
		UObject*		Object;
		FScriptArray*			Array;
		INT				Index;
		INT				Count;
		INT				Oper;
		INT				ElementSize;
		STRUCT_AR		Serializer;
		STRUCT_DTOR		Destructor;
		UBOOL			Restored;

		// Constructors.
		FObjectRecord()
		{}
		FObjectRecord( FTransaction* Owner, UObject* InObject, FScriptArray* InArray, INT InIndex, INT InCount, INT InOper, INT InElementSize, STRUCT_AR InSerializer, STRUCT_DTOR InDestructor )
			:	Object		( InObject )
			,	Array		( InArray )
			,	Index		( InIndex )
			,	Count		( InCount )
			,	Oper		( InOper )
			,	ElementSize	( InElementSize )
			,	Serializer	( InSerializer )
			,	Destructor	( InDestructor )
		{
			FWriter Writer( Data, ReferencedObjects, ReferencedNames );
			SerializeContents( Writer, Oper );
		}

		// Functions.
		void SerializeContents( FArchive& Ar, INT InOper );
		void Restore( FTransaction* Owner );
	
		/** Transfers data from an array. */
		class FReader : public FArchive
		{
		public:
			FReader(
				FTransaction* InOwner,
				const TArray<BYTE>& InData,
				const TArray<UObject*>& InReferencedObjects,
				const TArray<FName>& InReferencedNames
				):
				Owner(InOwner),
				Data(InData),
				ReferencedObjects(InReferencedObjects),
				ReferencedNames(InReferencedNames),
				Offset(0)
			{
				ArIsLoading = ArIsTransacting = ArWantBinaryPropertySerialization = 1;
			}
		private:
			void Serialize( void* SerData, INT Num )
			{
				if( Num )
				{
					checkSlow(Offset+Num<=Data.Num());
					appMemcpy( SerData, &Data(Offset), Num );
					Offset += Num;
				}
			}
			FArchive& operator<<( class FName& N )
			{
				INT NameIndex = 0;
				(FArchive&)*this << NameIndex;
				N = ReferencedNames(NameIndex);
				return *this;
			}
			FArchive& operator<<( class UObject*& Res )
			{
				INT ObjectIndex = 0;
				(FArchive&)*this << ObjectIndex;
				Res = ReferencedObjects(ObjectIndex);
				return *this;
			}
			void Preload( UObject* InObject )
			{
				if( Owner )
				{
					for( INT i=0; i<Owner->Records.Num(); i++ )
					{
						if( Owner->Records(i).Object==InObject )
						{
							Owner->Records(i).Restore( Owner );
						}
					}
				}
			}
			FTransaction* Owner;
			const TArray<BYTE>& Data;
			const TArray<UObject*>& ReferencedObjects;
			const TArray<FName>& ReferencedNames;
			INT Offset;
		};

		/**
		 * Transfers data to an array.
		 */
		class FWriter : public FArchive
		{
		public:
			FWriter(
				TArray<BYTE>& InData,
				TArray<UObject*>& InReferencedObjects,
				TArray<FName>& InReferencedNames
				):
				Data(InData),
				ReferencedObjects(InReferencedObjects),
				ReferencedNames(InReferencedNames)
			{
				ArIsSaving = ArIsTransacting = ArWantBinaryPropertySerialization = 1;
			}
		private:
			void Serialize( void* SerData, INT Num )
			{
				if( Num )
				{
					INT Index = Data.Add(Num);
					appMemcpy( &Data(Index), SerData, Num );
				}
			}
			FArchive& operator<<( class FName& N )
			{
				INT NameIndex = ReferencedNames.AddUniqueItem(N);
				return (FArchive&)*this << NameIndex;
			}
			FArchive& operator<<( class UObject*& Res )
			{
				INT ObjectIndex = ReferencedObjects.AddUniqueItem(Res);
				return (FArchive&)*this << ObjectIndex;
			}
			TArray<BYTE>& Data;
			TArray<UObject*>& ReferencedObjects;
			TArray<FName>& ReferencedNames;
		};
	};

	// Transaction variables.
	TArray<FObjectRecord>	Records;
	FString					Title;

	/** Used to prevent objects from being serialized to a transaction more than once. */
	typedef	TMap<UObject*,INT> ObjectMapType;
	ObjectMapType			ObjectMap;

	UBOOL					Flip;
	INT						Inc;
	INT						NumModelsModified;

public:
	// Constructor.
	FTransaction( const TCHAR* InTitle=NULL, UBOOL InFlip=0 )
		:	Title( InTitle ? InTitle : TEXT("") )
		,	Flip( InFlip )
		,	Inc( -1 )
	{}

	// FTransactionBase interface.
	virtual void SaveObject( UObject* Object );
	virtual void SaveArray( UObject* Object, FScriptArray* Array, INT Index, INT Count, INT Oper, INT ElementSize, STRUCT_AR Serializer, STRUCT_DTOR Destructor );

	/**
	 * Enacts the transaction.
	 */
	virtual void Apply();

	/**
	 * Returns a unique string to serve as a type ID for the FTranscationBase-derived type.
	 */
	virtual const TCHAR* GetTransactionType() const
	{
		return TEXT("FTransaction");
	}

	// FTransaction interface.
	SIZE_T DataSize() const;

	const TCHAR* GetTitle() const
	{
		return *Title;
	}
	friend FArchive& operator<<( FArchive& Ar, FTransaction& T )
	{
		check( Ar.IsAllowingReferenceElimination() == FALSE );
		return Ar << T.Records << T.Title << T.ObjectMap;
	}

	/**
	 * Returns the number of models that were modified by the last call to FTransaction::Apply().
	 */
	INT GetNumModelsModified() const
	{
		return NumModelsModified;
	}

	/**
	 * Get all the objects that are part of this transaction.
	 * @param	Objects		[out] Receives the object list.  Previous contents are cleared.
	 */
	void GetTransactionObjects(TArray<UObject*>& Objects);
	void RemoveRecords( INT Count = 1 );

	/**
	 * Outputs the contents of the ObjectMap to the specified output device.
	 */
	void DumpObjectMap(FOutputDevice& Ar) const;

	// Transaction friends.
	friend FArchive& operator<<( FArchive& Ar, FTransaction::FObjectRecord& R );

	friend class FObjectRecord;
	friend class FObjectRecord::FReader;
	friend class FObjectRecord::FWriter;
};

/*-----------------------------------------------------------------------------
	UTransBuffer.
-----------------------------------------------------------------------------*/

/**
 * Transaction tracking system, manages the undo and redo buffer.
 */
class UTransBuffer : public UTransactor
{
	DECLARE_CLASS_INTRINSIC(UTransBuffer,UTransactor,CLASS_Transient,UnrealEd)
	NO_DEFAULT_CONSTRUCTOR(UTransBuffer)

	// Variables.
	TArray<FTransaction>	UndoBuffer;

	/** Number of transactions that have been undone, and are eligable to be redone */
	INT						UndoCount;

	/** Text describing the reason that the undo buffer is empty */
	FString					ResetReason;

	/** Number of actions in the current transaction */
	INT						ActiveCount;

	/** Maximum number of bytes the transaction buffer is allowed to occupy */
	SIZE_T					MaxMemory;

private:
	/**
	 * Controls whether the transaction buffer is allowed to serialize object references
	 */
	INT						DisallowObjectSerialization;

public:

	// Constructor.
	UTransBuffer( SIZE_T InMaxMemory );

	// UObject interface.
	virtual void Serialize( FArchive& Ar );
	virtual void FinishDestroy();

	// UTransactor interface.

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
	virtual INT Begin( const TCHAR* SessionName );

	/**
	 * Attempts to close an undo transaction.  Only successful if the transaction's action
	 * counter is 1.
	 * 
	 * @return	Number of active actions when End() was called; a value of 1 indicates that the
	 *			transaction was successfully closed
	 */
	virtual INT End();

	/**
	 * Cancels the current transaction, no longer capture actions to be placed in the undo buffer.
	 *
	 * @param	StartIndex	the value of ActiveIndex when the transaction to be cancelled was began. 
	 */
	virtual void Cancel( INT StartIndex = 0 );

	/**
	 * Resets the entire undo buffer;  deletes all undo transactions.
	 */
	virtual void Reset( const TCHAR* Reason );

	/**
	 * Determines whether the undo option should be selectable.
	 * 
	 * @param	Str		[out] the reason that undo is disabled
	 *
	 * @return	TRUE if the "Undo" option should be selectable.
	 */
	virtual UBOOL CanUndo( FString* Str=NULL );

	/**
	 * Determines whether the redo option should be selectable.
	 * 
	 * @param	Str		[out] the reason that redo is disabled
	 *
	 * @return	TRUE if the "Redo" option should be selectable.
	 */
	virtual UBOOL CanRedo( FString* Str=NULL );

	/**
	 * Returns the description of the undo action that will be performed next.
	 * This is the text that is shown next to the "Undo" item in the menu.
	 * 
	 * @param	bCheckWhetherUndoPossible	Perform test whether undo is possible and return Error if not and option is set
	 *
	 * @return	text describing the next undo transaction
	 */
	virtual FString GetUndoDesc( UBOOL bCheckWhetherUndoPossible = TRUE );

	/**
	 * Returns the description of the redo action that will be performed next.
	 * This is the text that is shown next to the "Redo" item in the menu.
	 * 
	 * @return	text describing the next redo transaction
	 */
	virtual FString GetRedoDesc();

	/**
	 * Executes an undo transaction, undoing all actions contained by that transaction.
	 * 
	 * @return				TRUE if the transaction was successfully undone
	 */
	virtual UBOOL Undo();

	/**
	 * Executes an redo transaction, redoing all actions contained by that transaction.
	 * 
	 * @return				TRUE if the transaction was successfully redone
	 */
	virtual UBOOL Redo();

	/**
	 * Enables the transaction buffer to serialize the set of objects it references.
	 *
	 * @return	TRUE if the transaction buffer is able to serialize object references.
	 */
	virtual UBOOL EnableObjectSerialization();

	/**
	 * Disables the transaction buffer from serializing the set of objects it references.
	 *
	 * @return	TRUE if the transaction buffer is able to serialize object references.
	 */
	virtual UBOOL DisableObjectSerialization();

	/**
	 * Wrapper for checking if the transaction buffer is allowed to serialize object references.
	 */
	virtual UBOOL IsObjectSerializationEnabled() { return DisallowObjectSerialization == 0; }

	virtual FTransactionBase* CreateInternalTransaction();

	/**
	 * Determines the amount of data currently stored by the transaction buffer.
	 *
	 * @return	number of bytes stored in the undo buffer
	 */
	SIZE_T GetUndoSize() const;

	/**
	 * Checks if we have exceeded our buffer's max memory.
	 *
	 * @return True if we have exceeded our buffer's max memory.
	 */
	UBOOL IsTransactionBufferBreeched() const;

	/**
	 * Returns amount of free space in the transaction buffer.
	 *
	 * @return The amount of free space in the transaction buffer.
	 */
	INT GetBufferFreeSpace() const;

	/**
	 * Returns the last transaction size.
	 *
	 * @return The transaction size.
	 */
	INT GetLastTransactionSize();

	/**
	 * Validates the state of the transaction buffer.
	 */
	void CheckState() const;

	virtual UBOOL IsActive()
	{
		return ActiveCount > 0;
	}
};

#endif // __UNEDTRAN_H__
