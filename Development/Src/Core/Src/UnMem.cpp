/*=============================================================================
	UnMem.cpp: Unreal memory grabbing functions
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	FMemStack implementation.
-----------------------------------------------------------------------------*/

FMemStack::FMemStack( INT InDefaultChunkSize, UBOOL bInUsedInGameThread, UBOOL bInUsedInRenderingThread )
:	Top(NULL)
,	End(NULL)
,	DefaultChunkSize(InDefaultChunkSize)
,	TopChunk(NULL)
,	TopMark(NULL)
,	UnusedChunks(NULL)
,	NumMarks(0)
,	bUsedInGameThread(bInUsedInGameThread)
,	bUsedInRenderingThread(bInUsedInRenderingThread)
{
}

FMemStack::~FMemStack()
{
	check(GIsCriticalError || !NumMarks);

	Tick();
	while( UnusedChunks )
	{
		void* Old = UnusedChunks;
		UnusedChunks = UnusedChunks->Next;
		appFree( Old );
	}
}

void FMemStack::Tick() const
{
	check(TopChunk==NULL);
}

INT FMemStack::GetByteCount() const
{
	INT Count = 0;
	for( FTaggedMemory* Chunk=TopChunk; Chunk; Chunk=Chunk->Next )
	{
		if( Chunk!=TopChunk )
		{
			Count += Chunk->DataSize;
		}
		else
		{
			Count += Top - Chunk->Data;
		}
	}
	return Count;
}
INT FMemStack::GetUnusedByteCount() const
{
	INT Count = 0;
	for( FTaggedMemory* Chunk=UnusedChunks; Chunk; Chunk=Chunk->Next )
	{
		Count += Chunk->DataSize;
	}
	return Count;
}

BYTE* FMemStack::AllocateNewChunk( INT MinSize )
{
	FTaggedMemory* Chunk=NULL;
	for( FTaggedMemory** Link=&UnusedChunks; *Link; Link=&(*Link)->Next )
	{
		// Find existing chunk.
		if( (*Link)->DataSize >= MinSize )
		{
			Chunk = *Link;
			*Link = (*Link)->Next;
			break;
		}
	}

	if( !Chunk )
	{
		// Create new chunk.
		INT DataSize	= AlignArbitrary<INT>( MinSize + (INT)sizeof(FTaggedMemory), DefaultChunkSize ) - sizeof(FTaggedMemory);
		Chunk           = (FTaggedMemory*)appMalloc( DataSize + sizeof(FTaggedMemory) );
		Chunk->DataSize = DataSize;
	}
	Chunk->Next = TopChunk;
	TopChunk    = Chunk;
	Top         = Chunk->Data;
	End         = Top + Chunk->DataSize;
	return Top;
}

void FMemStack::FreeChunks( FTaggedMemory* NewTopChunk )
{
	while( TopChunk!=NewTopChunk )
	{
		FTaggedMemory* RemoveChunk = TopChunk;
		TopChunk                   = TopChunk->Next;
		RemoveChunk->Next          = UnusedChunks;
		UnusedChunks               = RemoveChunk;
	}
	Top = NULL;
	End = NULL;
	if( TopChunk )
	{
		Top = TopChunk->Data;
		End = Top + TopChunk->DataSize;
	}
}

