/*=============================================================================
	UnEdTran.h: Unreal transaction tracking system
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*-----------------------------------------------------------------------------
	UTransactor.
-----------------------------------------------------------------------------*/

//
// Object responsible for tracking transactions for undo/redo.
//
class UTransactor : public UObject
{
	DECLARE_ABSTRACT_CLASS(UTransactor,UObject,CLASS_Transient,Editor)

	// UTransactor interface.
	virtual void Reset( const TCHAR* Action )=0;
	virtual void Begin( const TCHAR* SessionName )=0;
	virtual void End()=0;
	virtual void Continue()=0;
	virtual UBOOL CanUndo( FString* Str=NULL )=0;
	virtual UBOOL CanRedo( FString* Str=NULL )=0;
	virtual FString GetUndoDesc()=0;
	virtual FString GetRedoDesc()=0;
	virtual UBOOL Undo()=0;
	virtual UBOOL Redo()=0;
	virtual FTransactionBase* CreateInternalTransaction()=0;
	virtual UBOOL IsActive()=0;
};

/*-----------------------------------------------------------------------------
	A single transaction.
-----------------------------------------------------------------------------*/

//
// A single transaction, representing a set of serialized, undoable changes to a set of objects.
//
//warning: The undo buffer cannot be made persistent because of its dependence on offsets 
// of arrays from their owning UObjects.
//
//warning: Transactions which rely on Preload calls cannot be garbage collected
// since references to objects point to the most recent version of the object, not
// the ordinally correct version which was referred to at the time of serialization.
// Therefore, Preload-sensitive transactions may only be performed using
// a temporary UTransactor::CreateInternalTransaction transaction, not a
// garbage-collectable UTransactor::Begin transaction.
//
//warning: UObject::Serialize implicitly assumes that class properties do not change
// in between transaction resets.
//
class FTransaction : public FTransactionBase
{
protected:
	// Record of an object.
	class FObjectRecord
	{
	public:
		// Variables.
		TArray<BYTE>	Data;
		UObject*		Object;
		FArray*			Array;
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
		FObjectRecord( FTransaction* Owner, UObject* InObject, FArray* InArray, INT InIndex, INT InCount, INT InOper, INT InElementSize, STRUCT_AR InSerializer, STRUCT_DTOR InDestructor )
		:	Object		( InObject )
		,	Array		( InArray )
		,	Index		( InIndex )
		,	Count		( InCount )
		,	Oper		( InOper )
		,	ElementSize	( InElementSize )
		,	Serializer	( InSerializer )
		,	Destructor	( InDestructor )
		{
			FWriter Writer( Data );
			SerializeContents( Writer, Oper );
		}

		// Functions.
		void SerializeContents( FArchive& Ar, INT InOper );
		void Restore( FTransaction* Owner );
	
		// Transfers data from an array.
		class FReader : public FArchive
		{
		public:
			FReader( FTransaction* InOwner, TArray<BYTE>& InBytes )
			:	Bytes	( InBytes )
			,	Offset	( 0 )
			,	Owner	( InOwner )
			{
				ArIsLoading = ArIsTrans = 1;
			}
		private:
			void Serialize( void* Data, INT Num )
			{
				checkSlow(Offset+Num<=Bytes.Num());
				appMemcpy( Data, &Bytes(Offset), Num );
				Offset += Num;
			}
			FArchive& operator<<( class FName& N )
			{
				checkSlow(Offset+(INT)sizeof(FName)<=Bytes.Num());
				N = *(FName*)&Bytes(Offset);
				Offset += sizeof(FName);
				return *this;
			}
			FArchive& operator<<( class UObject*& Res )
			{
				checkSlow(Offset+(INT)sizeof(UObject*)<=Bytes.Num());
				Res = *(UObject**)&Bytes(Offset);
				Offset += sizeof(UObject*);
				return *this;
			}
			void Preload( UObject* Object )
			{
				if( Owner )
					for( INT i=0; i<Owner->Records.Num(); i++ )
						if( Owner->Records(i).Object==Object )
							Owner->Records(i).Restore( Owner );
			}
			FTransaction* Owner;
			TArray<BYTE>& Bytes;
			INT Offset;
		};

		// Transfers data to an array.
		class FWriter : public FArchive
		{
		public:
			FWriter( TArray<BYTE>& InBytes )
			: Bytes( InBytes )
			{
				ArIsSaving = ArIsTrans = 1;
			}
		private:
			void Serialize( void* Data, INT Num )
			{
				INT Index = Bytes.Add(Num);
				appMemcpy( &Bytes(Index), Data, Num );
			}
			FArchive& operator<<( class FName& N )
			{
				INT Index = Bytes.Add( sizeof(FName) );
				*(FName*)&Bytes(Index) = N;
				return *this;
			}
			FArchive& operator<<( class UObject*& Res )
			{
				INT Index = Bytes.Add( sizeof(UObject*) );
				*(UObject**)&Bytes(Index) = Res;
				return *this;
			}
			TArray<BYTE>& Bytes;
		};
	};

	// Transaction variables.
	TArray<FObjectRecord>	Records;
	FString					Title;
	UBOOL					Flip;
	INT						Inc;

public:
	// Constructor.
	FTransaction( const TCHAR* InTitle=NULL, UBOOL InFlip=0 )
	: Title( InTitle ? InTitle : TEXT("") )
	, Flip( InFlip )
	, Inc( -1 )
	{}

	// FTransactionBase interface.
	void SaveObject( UObject* Object );
	void SaveArray( UObject* Object, FArray* Array, INT Index, INT Count, INT Oper, INT ElementSize, STRUCT_AR Serializer, STRUCT_DTOR Destructor );
	void Apply();

	// FTransaction interface.
	SIZE_T DataSize()
	{
		SIZE_T Result=0;
		for( INT i=0; i<Records.Num(); i++ )
			Result += Records(i).Data.Num();
		return Result;
	}
	const TCHAR* GetTitle()
	{
		return *Title;
	}
	friend FArchive& operator<<( FArchive& Ar, FTransaction& T )
	{
		return Ar << T.Records << T.Title;
	}

	// Get all the objects that are part of this transaction.
	void GetTransactionObjects(TArray<UObject*>& Objects);

	// Transaction friends.
	friend FArchive& operator<<( FArchive& Ar, FTransaction::FObjectRecord& R );

	friend class FObjectRecord;
	friend class FObjectRecord::FReader;
	friend class FObjectRecord::FWriter;
};

/*-----------------------------------------------------------------------------
	Transaction tracking system.
-----------------------------------------------------------------------------*/

//
// Transaction tracking system, manages the undo and redo buffer.
//
class UTransBuffer : public UTransactor
{
	DECLARE_CLASS(UTransBuffer,UObject,CLASS_Transient,Editor)
	NO_DEFAULT_CONSTRUCTOR(UTransBuffer)

	// Variables.
	TArray<FTransaction>	UndoBuffer;
	INT						UndoCount;
	FString					ResetReason;
	INT						ActiveCount;
	SIZE_T					MaxMemory;
	UBOOL					Overflow;

	// Constructor.
	UTransBuffer( SIZE_T InMaxMemory )
	:	MaxMemory( InMaxMemory )
	{
		// Reset.
		Reset( TEXT("startup") );
		CheckState();

		debugf( NAME_Init, TEXT("Transaction tracking system initialized") );
	}

	// UObject interface.
	void Serialize( FArchive& Ar );
	void Destroy();

	// UTransactor interface.
	void Reset( const TCHAR* Reason );
	void Begin( const TCHAR* SessionName );
	void End();
	void Continue();
	UBOOL CanUndo( FString* Str=NULL );
	UBOOL CanRedo( FString* Str=NULL );

	// Returns the description of the undo action that will be performed next
	FString GetUndoDesc();

	// Returns the description of the redo action that will be performed next
	FString GetRedoDesc();

	UBOOL Undo();
	UBOOL Redo();
	FTransactionBase* CreateInternalTransaction();
	
	// Functions.
	void FinishDo();
	SIZE_T UndoDataSize();
	void CheckState();

	virtual UBOOL IsActive() { return ActiveCount > 0; }
};

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
