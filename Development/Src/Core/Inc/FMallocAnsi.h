/*=============================================================================
	FMallocAnsi.h: ANSI memory allocator.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
		* Alignment support by Daniel Vogel
=============================================================================*/

#ifdef _MSC_VER
#define USE_ALIGNED_MALLOC 1
#else
#error this should be implemented more elegantly on other platforms
#define USE_ALIGNED_MALLOC 0
#endif

//
// ANSI C memory allocator.
//
class FMallocAnsi : public FMalloc
{
	// Alignment.
	enum {ALLOCATION_ALIGNMENT=16};

public:
	// FMalloc interface.
	void* Malloc( DWORD Size )
	{
		check(Size>=0);
#if USE_ALIGNED_MALLOC
		void* Ptr = _aligned_malloc( Size, ALLOCATION_ALIGNMENT );
		check(Ptr);
		return Ptr;
#else
		void* Ptr = malloc( Size + ALLOCATION_ALIGNMENT + sizeof(void*) + sizeof(DWORD) );
		check(Ptr);
		void* AlignedPtr = Align( (BYTE*)Ptr + sizeof(void*) + sizeof(DWORD), ALLOCATION_ALIGNMENT );
		*((void**)( (BYTE*)AlignedPtr - sizeof(void*)					))	= Ptr;
		*((DWORD*)( (BYTE*)AlignedPtr - sizeof(void*) - sizeof(DWORD)	))	= Size;
		return AlignedPtr;
#endif
	}
	void* Realloc( void* Ptr, DWORD NewSize )
	{
		checkSlow(NewSize>=0);
		void* Result;
#if USE_ALIGNED_MALLOC
		if( Ptr && NewSize )
		{
			Result = _aligned_realloc( Ptr, NewSize, ALLOCATION_ALIGNMENT );
		}
		else if( Ptr == NULL )
		{
			Result = _aligned_malloc( NewSize, ALLOCATION_ALIGNMENT );
		}
		else
		{
			_aligned_free( Ptr );
			Result = NULL;
		}
#else
		if( Ptr && NewSize )
		{
			// Can't use realloc as it might screw with alignment.
			Result = Malloc( NewSize, Tag );
			appMemcpy( Result, Ptr, Min(NewSize, *((DWORD*)( (BYTE*)Ptr - sizeof(void*) - sizeof(DWORD))) ) );
			Free( Ptr );
		}
		else if( Ptr == NULL )
		{
			Result = Malloc( NewSize, Tag );
		}
		else
		{
			free( *((void**)((BYTE*)Ptr-sizeof(void*))) );
			Result = NULL;
		}
#endif
		return Result;
	}
	void Free( void* Ptr )
	{
#ifdef USE_ALIGNED_MALLOC
		_aligned_free( Ptr );
#else
		if( Ptr )
			free( *((void**)((BYTE*)Ptr-sizeof(void*))) );
#endif
	}
	void DumpAllocs()
	{
		debugf( NAME_Exit, TEXT("Allocation checking disabled") );
	}
	void HeapCheck()
	{
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
	}
	void Exit()
	{
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

