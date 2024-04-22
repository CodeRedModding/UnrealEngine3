/*=============================================================================
	UnObjGC.h: Unreal realtime garbage collection helpers
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/** Context sensitive keep flags for garbage collection */
#define GARBAGE_COLLECTION_KEEPFLAGS	(GIsEditor ? RF_Native | RF_Standalone : RF_Native)

/*-----------------------------------------------------------------------------
	Realtime garbage collection helper classes.
-----------------------------------------------------------------------------*/

/**
 * Enum of different supported reference type tokens.
 */
enum EGCReferenceType
{
	GCRT_None			= 0,
	GCRT_Object,
	GCRT_PersistentObject,
	GCRT_ArrayObject,
	GCRT_ArrayStruct,
	GCRT_FixedArray,
	GCRT_EndOfStream,
	GCRT_ScriptDelegate,
	GCRT_StateLocals,
};

/** 
 * Convenience struct containing all necessary information for a reference.
 */
struct FGCReferenceInfo
{
	/**
	 * Constructor
	 *
	 * @param InType	type of reference
	 * @param InOffset	offset into object/ struct
	 */
	FORCEINLINE FGCReferenceInfo( EGCReferenceType InType, DWORD InOffset )
	:	ReturnCount( 0 )
	,	Type( InType )
	,	Offset( InOffset )	
	{
		check( InType != GCRT_None );
		// Change to allow lots of actors that all have replication
		check( (InOffset & ~0xFFFFF) == 0 );
	}
	/**
	 * Constructor
	 *
	 * @param InValue	value to set union mapping to a DWORD to
	 */
	FORCEINLINE FGCReferenceInfo( DWORD InValue )
	:	Value( InValue )
	{}
	/**
	 * DWORD conversion operator
	 *
	 * @return DWORD value of struct
	 */
	FORCEINLINE operator DWORD() const 
	{ 
		return Value; 
	}

	/** Mapping to exactly one DWORD */
	union
	{
		/** Mapping to exactly one DWORD */
		struct
		{
			/** Return depth, e.g. 1 for last entry in an array, 2 for last entry in an array of structs of arrays, ... */
			DWORD ReturnCount	: 8;
			/** Type of reference */
			DWORD Type			: 4;
			/** Offset into struct/ object */
			DWORD Offset		: 20;
		};
		/** DWORD value of reference info, used for easy conversion to/ from DWORD for token array */
		DWORD Value;
	};
};

/** 
 * Convenience structure containing all necessary information for skipping a dynamic array
 */
struct FGCSkipInfo
{
	/**
	 * Default constructor
	 */
	FORCEINLINE FGCSkipInfo()
	{}

	/**
	 * Constructor
	 *
	 * @param InValue	value to set union mapping to a DWORD to
	 */
	FORCEINLINE FGCSkipInfo( DWORD InValue )
	:	Value( InValue )
	{}
	/**
	 * DWORD conversion operator
	 *
	 * @return DWORD value of struct
	 */
	FORCEINLINE operator DWORD() const 
	{ 
		return Value; 
	}

	/** Mapping to exactly one DWORD */
	union
	{
		/** Mapping to exactly one DWORD */
		struct
		{
			/** Return depth not taking into account embedded arrays. This is needed to return appropriately when skipping empty dynamic arrays as we never step into them */
			DWORD InnerReturnCount	: 8;
			/** Skip index */
			DWORD SkipIndex			: 24;
		};
		/** DWORD value of skip info, used for easy conversion to/ from DWORD for token array */
		DWORD Value;
	};
};

/**
 * Reference token stream class. Used for creating and parsing stream of object references.
 */
struct FGCReferenceTokenStream
{
	/** Initialization value to ensure that we have the right skip index index */
	enum EGCArrayInfoPlaceholder { E_GCSkipIndexPlaceholder = 0xDEADBABE }; 

	/** Constructor */
	FGCReferenceTokenStream()
	{
		check( sizeof(FGCReferenceInfo) == sizeof(DWORD) );
	}

	/**
	 * Shrinks the token stream, removing array slack.
	 */
	void Shrink()
	{
		Tokens.Shrink();
	}

	/**
	 * Prepends passed in stream to existing one.
	 *
	 * @param Other	stream to concatenate
	 */
	void PrependStream( const FGCReferenceTokenStream& Other );

	/**
	 * Emit reference info
	 *
	 * @param ReferenceInfo	reference info to emit
	 */
	void EmitReferenceInfo( FGCReferenceInfo ReferenceInfo );

	/**
	 * Emit placeholder for aray skip index, updated in UpdateSkipIndexPlaceholder
	 *
	 * @return the index of the skip index, used later in UpdateSkipIndexPlaceholder
	 */
	DWORD EmitSkipIndexPlaceholder();

	/**
	 * Updates skip index place holder stored and passed in skip index index with passed
	 * in skip index. The skip index is used to skip over tokens in the case of an emtpy 
	 * dynamic array.
	 * 
	 * @param SkipIndexIndex index where skip index is stored at.
	 * @param SkipIndex index to store at skip index index
	 */
	void UpdateSkipIndexPlaceholder( DWORD SkipIndexIndex, DWORD SkipIndex );

	/**
	 * Emit count
	 *
	 * @param Count count to emit
	 */
	void EmitCount( DWORD Count );

	/**
	 * Emit stride
	 *
	 * @param Stride stride to emit
	 */
	void EmitStride( DWORD Stride );

	/**
	 * Increase return count on last token.
	 *
	 * @return index of next token
	 */
	DWORD EmitReturn();

	/**
	 * Reads count and advances stream.
	 *
	 * @return read in count
	 */
	FORCEINLINE DWORD ReadCount( DWORD& CurrentIndex )
	{
		return Tokens(CurrentIndex++);
	}

	/**
	 * Reads stride and advances stream.
	 *
	 * @return read in stride
	 */
	FORCEINLINE DWORD ReadStride( DWORD& CurrentIndex )
	{
		return Tokens(CurrentIndex++);
	}

	/**
	 * Reads in reference info and advances stream.
	 *
	 * @return read in reference info
	 */
	FORCEINLINE FGCReferenceInfo ReadReferenceInfo( DWORD& CurrentIndex )
	{
		return Tokens(CurrentIndex++);
	}

	/**
	 * Access reference info at passed in index. Used as helper to eliminate LHS.
	 *
	 * @return Reference info at passed in index
	 */
	FORCEINLINE FGCReferenceInfo AccessReferenceInfo( DWORD CurrentIndex ) const
	{
		return Tokens(CurrentIndex);
	}

	/**
	 * Read in skip index and advances stream.
	 *
	 * @return read in skip index
	 */
	FORCEINLINE FGCSkipInfo ReadSkipInfo( DWORD& CurrentIndex )
	{
		FGCSkipInfo SkipInfo = Tokens(CurrentIndex);
		SkipInfo.SkipIndex += CurrentIndex;
		CurrentIndex++;
		return SkipInfo;
	}

	/**
	 * Read return count stored at the index before the skip index. This is required 
	 * to correctly return the right amount of levels when skipping over an empty array.
	 *
	 * @param SkipIndex index of first token after array
	 */
	FORCEINLINE DWORD GetSkipReturnCount( FGCSkipInfo SkipInfo )
	{
		check( SkipInfo.SkipIndex > 0 && SkipInfo.SkipIndex <= (DWORD)Tokens.Num() );		
		FGCReferenceInfo ReferenceInfo = Tokens(SkipInfo.SkipIndex-1);
		check( ReferenceInfo.Type != GCRT_None );
		return ReferenceInfo.ReturnCount - SkipInfo.InnerReturnCount;		
	}

	/**
	 * Queries the stream for an end of stream condition
	 *
	 * @return TRUE if the end of the stream has been reached, FALSE otherwise
	 */
	FORCEINLINE UBOOL EndOfStream( DWORD CurrentIndex )
	{
		return CurrentIndex >= (DWORD)Tokens.Num();
	}

private:
	/** Token array */
	TArray<DWORD>	Tokens;
};

