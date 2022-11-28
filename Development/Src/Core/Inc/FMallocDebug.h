/*=============================================================================
	FMallocDebug.h: Debug memory allocator.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
		* Alignment support by Daniel Vogel
=============================================================================*/

#include <malloc.h>

// Debug memory allocator.
class FMallocDebug : public FMalloc
{
	// Tags.
	enum {MEM_PreTag =0xf0ed1cee};
	enum {MEM_PostTag=0xdeadf00f};
	enum {MEM_Tag    =0xfe      };
	enum {MEM_WipeTag=0xcd      };

	// Alignment.
	enum {ALLOCATION_ALIGNMENT=16};

private:
	// Structure for memory debugging.
	struct FMemDebug
	{
		SIZE_T		Size;
		INT			RefCount;
		INT*		PreTag;
		FMemDebug*	Next;
		FMemDebug**	PrevLink;
	};

	// Variables.
	FMemDebug* GFirstDebug;
	UBOOL MemInited;
	/** Total size of allocations */
	SIZE_T TotalAllocationSize;

public:
	// FMalloc interface.
	FMallocDebug()
	:	GFirstDebug	( NULL )
	,	MemInited	( 0 )
	{}
	void* Malloc( DWORD Size )
	{
		checkSlow(MemInited);
		check((INT)Size>=0);
		FMemDebug* Ptr = NULL;
		Ptr = (FMemDebug*)malloc( sizeof(FMemDebug) + sizeof(FMemDebug*) + sizeof(INT) + ALLOCATION_ALIGNMENT + Size + sizeof(INT) );
		check(Ptr);
		BYTE* AlignedPtr = Align( (BYTE*) Ptr + sizeof(FMemDebug) + sizeof(FMemDebug*) + sizeof(INT), ALLOCATION_ALIGNMENT );
		Ptr->RefCount = 1;
		Ptr->Size     = Size;
		Ptr->Next     = GFirstDebug;
		Ptr->PrevLink = &GFirstDebug;
		Ptr->PreTag   = (INT*) (AlignedPtr - sizeof(INT));
		*Ptr->PreTag  = MEM_PreTag;
		*((FMemDebug**)(AlignedPtr - sizeof(INT) - sizeof(FMemDebug*)))	= Ptr;
		*(INT*)(AlignedPtr+Size) = MEM_PostTag;
		appMemset( AlignedPtr, MEM_Tag, Size );
		if( GFirstDebug )
		{
			check(GIsCriticalError||GFirstDebug->PrevLink==&GFirstDebug);
			GFirstDebug->PrevLink = &Ptr->Next;
		}
		GFirstDebug = Ptr;
		TotalAllocationSize += Size;

		check(!(DWORD(AlignedPtr) & 0xf));
		return AlignedPtr;
	}
	void* Realloc( void* InPtr, DWORD NewSize )
	{
		checkSlow(MemInited);
		if( InPtr && NewSize )
		{
			FMemDebug* Ptr = *((FMemDebug**)((BYTE*)InPtr - sizeof(INT) - sizeof(FMemDebug*)));
			check(GIsCriticalError||(Ptr->RefCount==1));
			check(GIsCriticalError||(Ptr->Size>0));
			void* Result = Malloc( NewSize );
			appMemcpy( Result, InPtr, Min(Ptr->Size,NewSize) );
			Free( InPtr );
			return Result;
		}
		else if( InPtr == NULL )
		{
			return Malloc( NewSize );
		}
		else
		{
			Free( InPtr );
			return NULL;
		}
	}
	void Free( void* InPtr )
	{
		checkSlow(MemInited);
		if( !InPtr )
			return;

		FMemDebug* Ptr = *((FMemDebug**)((BYTE*)InPtr - sizeof(INT) - sizeof(FMemDebug*)));

		check(GIsCriticalError||Ptr->Size>=0);
		check(GIsCriticalError||Ptr->RefCount==1);
		check(GIsCriticalError||*Ptr->PreTag==MEM_PreTag);
		check(GIsCriticalError||*(INT*)((BYTE*)InPtr+Ptr->Size)==MEM_PostTag);
		
		TotalAllocationSize -= Ptr->Size;
		
		appMemset( InPtr, MEM_WipeTag, Ptr->Size );	
		Ptr->Size = 0;
		Ptr->RefCount = 0;

		check(GIsCriticalError||Ptr->PrevLink);
		check(GIsCriticalError||*Ptr->PrevLink==Ptr);
		*Ptr->PrevLink = Ptr->Next;
		if( Ptr->Next )
			Ptr->Next->PrevLink = Ptr->PrevLink;

		free( Ptr );
	}
	/**
	 * Gathers memory allocations for both virtual and physical allocations.
	 *
	 * @param Virtual	[out] size of virtual allocations
	 * @param Physical	[out] size of physical allocations	
	 */
	void GetAllocationInfo( SIZE_T& Virtual, SIZE_T& Physical )
	{
		Virtual		= TotalAllocationSize;
		Physical	= 0;
	}
	void DumpAllocs()
	{
		INT Count=0;
		INT Chunks=0;

		debugf( TEXT("Unfreed memory:") );
		for( FMemDebug* Ptr=GFirstDebug; Ptr; Ptr=Ptr->Next )
		{
			TCHAR	Temp[256];
			TCHAR*	SrcPtr = (TCHAR*)(Ptr+1);
			for(UINT Index = 0;Index < 255 && Index < Ptr->Size / sizeof(TCHAR);Index++)
				Temp[Index] = SrcPtr[Index] == 0xffff ? 0 : SrcPtr[Index]; // 0xffff is a special Unicode character "reserved for app", strip it out.
			Temp[255] = 0;
			//debugf( TEXT("   % 10i <%s>"), Ptr->Size, Temp ); // This is usually not that interesting information.
			Count += Ptr->Size;
			Chunks++;
		}
		debugf( TEXT("End of list: %i Bytes still allocated"), Count );
		debugf( TEXT("             %i Chunks allocated"), Chunks );
	}
	void HeapCheck()
	{
		for( FMemDebug** Link = &GFirstDebug; *Link; Link=&(*Link)->Next )
			check(GIsCriticalError||*(*Link)->PrevLink==*Link);
#if (defined _MSC_VER)
		INT Result = _heapchk();
		check(Result!=_HEAPBADBEGIN);
		check(Result!=_HEAPBADNODE);
		check(Result!=_HEAPBADPTR);
		check(Result!=_HEAPEMPTY);
		check(Result==_HEAPOK);
#endif
	}
	void Init( UBOOL Reset )
	{
		check(!MemInited);
		MemInited=1;
	}
	void Exit()
	{
		check(MemInited);
		MemInited=0;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

