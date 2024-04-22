/*=============================================================================
	ChunkedArray.h: Chunked array definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/** An array that uses multiple allocations to avoid allocation failure due to fragmentation. */
template<typename ElementType>
class TChunkedArray
{
public:

	/** Initialization constructor. */
	TChunkedArray(INT InNumElements):
		NumElements(InNumElements)
	{
		// Compute the number of chunks needed.
		const INT NumChunks = (NumElements + NumElementsPerChunk - 1) / NumElementsPerChunk;

		// Allocate the chunks.
		Chunks.Empty(NumChunks);
		for(INT ChunkIndex = 0;ChunkIndex < NumChunks;ChunkIndex++)
		{
			new(Chunks) FChunk;
		}
	}

	// Accessors.
	ElementType& operator()(INT ElementIndex)
	{
		const UINT ChunkIndex = ElementIndex / NumElementsPerChunk;
		const UINT ChunkElementIndex = ElementIndex % NumElementsPerChunk;
		return Chunks(ChunkIndex).Elements[ChunkElementIndex];
	}
	const ElementType& operator()(INT ElementIndex) const
	{
		const INT ChunkIndex = ElementIndex / NumElementsPerChunk;
		const INT ChunkElementIndex = ElementIndex % NumElementsPerChunk;
		return Chunks(ChunkIndex).Elements[ChunkElementIndex];
	}
	INT Num() const { return NumElements; }

private:

	enum { TargetBytesPerChunk = 16384 };
	enum { NumElementsPerChunk = TargetBytesPerChunk / sizeof(ElementType) };

	/** A chunk of the array's elements. */
	struct FChunk
	{
		/** The elements in the chunk. */
		ElementType Elements[NumElementsPerChunk];
	};

	/** The chunks of the array's elements. */
	TIndirectArray<FChunk> Chunks;

	/** The number of elements in the array. */
	INT NumElements;
};
