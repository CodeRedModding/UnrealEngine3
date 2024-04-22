/*=============================================================================
	UnMem.h: FMemStack class, ultra-fast temporary memory allocation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

// Enums for specifying memory allocation type.
enum EMemZeroed {MEM_Zeroed=1};
enum EMemOned   {MEM_Oned  =1};

/**
 * Simple linear-allocation memory stack.
 * Items are allocated via PushBytes() or the specialized operator new()s.
 * Items are freed en masse by using FMemMark to Pop() them.
 **/
class FMemStack
{
public:

	/** Initialization constructor. */
	FMemStack(INT DefaultChunkSize, UBOOL bUsedInGameThread, UBOOL bUsedInRenderingThread);

	/** Destructor. */
	~FMemStack();

	// Get bytes.
	FORCEINLINE BYTE* PushBytes( INT AllocSize, INT Align )
	{
		// Debug checks.
		checkSlow(AllocSize>=0);
		checkSlow((Align&(Align-1))==0);
		checkSlow(Top<=End);
		checkSlow(NumMarks > 0);

		// Critically important checks.
		check( !bUsedInGameThread || IsInGameThread() );
		check( !bUsedInRenderingThread || IsInRenderingThread() );

		// Try to get memory from the current chunk.
		BYTE* Result = (BYTE *)(((PTRINT)Top+(Align-1))&~(Align-1));
		BYTE* NewTop = Result + AllocSize;

		// Make sure we didn't overflow.
		if ( NewTop <= End )
		{
			Top	= NewTop;
		}
		else
		{
			// We'd pass the end of the current chunk, so allocate a new one.
			AllocateNewChunk( AllocSize + Align );
			Result = (BYTE *)(((PTRINT)Top+(Align-1))&~(Align-1));
			Top    = Result + AllocSize;
		}
		return Result;
	}

	/** Timer tick. Makes sure the memory stack is empty. */
	void Tick() const;

	/** @return the number of bytes allocated for this FMemStack that are currently in use. */
	INT GetByteCount() const;

	/** @return the number of bytes allocated for this FMemStack that are currently unused and available. */
	INT GetUnusedByteCount() const;

	// Friends.
	friend class FMemMark;
	friend void* operator new( size_t Size, FMemStack& Mem, INT Count=1, INT Align=DEFAULT_ALIGNMENT );
	friend void* operator new( size_t Size, FMemStack& Mem, EMemZeroed Tag, INT Count=1, INT Align=DEFAULT_ALIGNMENT );
	friend void* operator new( size_t Size, FMemStack& Mem, EMemOned Tag, INT Count=1, INT Align=DEFAULT_ALIGNMENT );
	friend void* operator new[]( size_t Size, FMemStack& Mem, INT Count=1, INT Align=DEFAULT_ALIGNMENT );
	friend void* operator new[]( size_t Size, FMemStack& Mem, EMemZeroed Tag, INT Count=1, INT Align=DEFAULT_ALIGNMENT );
	friend void* operator new[]( size_t Size, FMemStack& Mem, EMemOned Tag, INT Count=1, INT Align=DEFAULT_ALIGNMENT );

	// Types.
	struct FTaggedMemory
	{
		FTaggedMemory* Next;
		INT DataSize;
		BYTE Data[1];
	};

private:
	// Constants.
	enum {MAX_CHUNKS=1024};

	// Variables.
	BYTE*			Top;				// Top of current chunk (Top<=End).
	BYTE*			End;				// End of current chunk.
	INT				DefaultChunkSize;	// Maximum chunk size to allocate.
	FTaggedMemory*	TopChunk;			// Only chunks 0..ActiveChunks-1 are valid.

	/** The top mark on the stack. */
	class FMemMark*	TopMark;

	/** The memory chunks that have been allocated but are currently unused. */
	FTaggedMemory*	UnusedChunks;

	/** The number of marks on this stack. */
	INT NumMarks;

	/** Whether this stack is used in the game thread. */
	UBOOL			bUsedInGameThread;

	/** Whether this stack is used in the rendering thread. */
	UBOOL			bUsedInRenderingThread;

	/**
	 * Allocate a new chunk of memory of at least MinSize size,
	 * and return it aligned to Align. Updates the memory stack's
	 * Chunks table and ActiveChunks counter.
	 */
	BYTE* AllocateNewChunk( INT MinSize );

	/** Frees the chunks above the specified chunk on the stack. */
	void FreeChunks( FTaggedMemory* NewTopChunk );
};

/*-----------------------------------------------------------------------------
	FMemStack templates.
-----------------------------------------------------------------------------*/

// Operator new for typesafe memory stack allocation.
template <class T> inline T* New( FMemStack& Mem, INT Count=1, INT Align=DEFAULT_ALIGNMENT )
{
	return (T*)Mem.PushBytes( Count*sizeof(T), Align );
}
template <class T> inline T* NewZeroed( FMemStack& Mem, INT Count=1, INT Align=DEFAULT_ALIGNMENT )
{
	BYTE* Result = Mem.PushBytes( Count*sizeof(T), Align );
	appMemzero( Result, Count*sizeof(T) );
	return (T*)Result;
}
template <class T> inline T* NewOned( FMemStack& Mem, INT Count=1, INT Align=DEFAULT_ALIGNMENT )
{
	BYTE* Result = Mem.PushBytes( Count*sizeof(T), Align );
	appMemset( Result, 0xff, Count*sizeof(T) );
	return (T*)Result;
}

/*-----------------------------------------------------------------------------
	FMemStack operator new's.
-----------------------------------------------------------------------------*/

// Operator new for typesafe memory stack allocation.
inline void* operator new( size_t Size, FMemStack& Mem, INT Count, INT Align )
{
	// Get uninitialized memory.
	return Mem.PushBytes( Size*Count, Align );
}
inline void* operator new( size_t Size, FMemStack& Mem, EMemZeroed Tag, INT Count, INT Align )
{
	// Get zero-filled memory.
	BYTE* Result = Mem.PushBytes( Size*Count, Align );
	appMemzero( Result, Size*Count );
	return Result;
}
inline void* operator new( size_t Size, FMemStack& Mem, EMemOned Tag, INT Count, INT Align )
{
	// Get one-filled memory.
	BYTE* Result = Mem.PushBytes( Size*Count, Align );
	appMemset( Result, 0xff, Size*Count );
	return Result;
}
inline void* operator new[]( size_t Size, FMemStack& Mem, INT Count, INT Align )
{
	// Get uninitialized memory.
	return Mem.PushBytes( Size*Count, Align );
}
inline void* operator new[]( size_t Size, FMemStack& Mem, EMemZeroed Tag, INT Count, INT Align )
{
	// Get zero-filled memory.
	BYTE* Result = Mem.PushBytes( Size*Count, Align );
	appMemzero( Result, Size*Count );
	return Result;
}
inline void* operator new[]( size_t Size, FMemStack& Mem, EMemOned Tag, INT Count, INT Align )
{
	// Get one-filled memory.
	BYTE* Result = Mem.PushBytes( Size*Count, Align );
	appMemset( Result, 0xff, Size*Count );
	return Result;
}

/** A container allocator that allocates from a mem-stack. */
template<FMemStack& MemStack,DWORD Alignment = DEFAULT_ALIGNMENT>
class TMemStackAllocator
{
public:

	enum { NeedsElementType = TRUE };

	template<typename ElementType>
	class ForElementType
	{
	public:

		/** Default constructor. */
		ForElementType():
			Data(NULL)
		{}

		/** ENoInit constructor. */
		ForElementType(ENoInit)
		{}

		// FContainerAllocatorInterface
		FORCEINLINE ElementType* GetAllocation() const
		{
			return Data;
		}
		void ResizeAllocation(INT PreviousNumElements,INT NumElements,INT NumBytesPerElement)
		{
			void* OldData = Data;
			if( NumElements )
			{
				// Allocate memory from the stack.
				Data = (ElementType*)MemStack.PushBytes(
					NumElements * NumBytesPerElement,
					Max(Alignment,(DWORD)TElementAlignmentCalculator<ElementType>::Alignment)
					);

				// If the container previously held elements, copy them into the new allocation.
				if(OldData && PreviousNumElements)
				{
					const INT NumCopiedElements = Min(NumElements,PreviousNumElements);
					appMemcpy(Data,OldData,NumCopiedElements * NumBytesPerElement);
				}
			}
		}
		INT CalculateSlack(INT NumElements,INT NumAllocatedElements,INT NumBytesPerElement) const
		{
			return DefaultCalculateSlack(NumElements,NumAllocatedElements,NumBytesPerElement);
		}

		INT GetAllocatedSize(INT NumAllocatedElements, INT NumBytesPerElement) const
		{
			return NumAllocatedElements * NumBytesPerElement;
		}
			
	private:

		/** A pointer to the container's elements. */
		ElementType* Data;
	};
	
	typedef ForElementType<FScriptContainerElement> ForAnyElementType;
};

/**
 * FMemMark marks a top-of-stack position in the memory stack.
 * When the marker is constructed or initialized with a particular memory 
 * stack, it saves the stack's current position. When marker is popped, it
 * pops all items that were added to the stack subsequent to initialization.
 */
class FMemMark
{
public:
	// Constructors.
	FMemMark( FMemStack& InMem )
	:	Mem(InMem)
	,	Top(InMem.Top)
	,	SavedChunk(InMem.TopChunk)
	,	bPopped(FALSE)
	,	NextTopmostMark(InMem.TopMark)
	{
		Mem.TopMark = this;

		// Track the number of outstanding marks on the stack.
		Mem.NumMarks++;
	}

	/** Destructor. */
	~FMemMark()
	{
		Pop();
	}

	/** Free the memory allocated after the mark was created. */
	void Pop()
	{
		if(!bPopped)
		{
			check(Mem.TopMark == this);
			bPopped = TRUE;

			// Track the number of outstanding marks on the stack.
			--Mem.NumMarks;

			// Unlock any new chunks that were allocated.
			if( SavedChunk != Mem.TopChunk )
				Mem.FreeChunks( SavedChunk );

			// Restore the memory stack's state.
			Mem.Top = Top;
			Mem.TopMark = NextTopmostMark;

			// Ensure that the mark is only popped once by clearing the top pointer.
			Top = NULL;
		}
	}

private:
	// Implementation variables.
	FMemStack& Mem;
	BYTE* Top;
	FMemStack::FTaggedMemory* SavedChunk;
	UBOOL bPopped;
	FMemMark* NextTopmostMark;
};
