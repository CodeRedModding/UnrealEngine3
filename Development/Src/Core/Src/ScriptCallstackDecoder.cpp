/*=============================================================================
	ScriptCallstackDecoder.cpp: Tracks the current script callstack for memory profiling
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#include "FMallocProfiler.h"
#include "ScriptCallstackDecoder.h"

#if USE_MALLOC_PROFILER_DECODE_SCRIPT

/** Compacts FName so it can be stored as an INT. */
inline INT CompactFName( const FName Name )
{
	INT Index = Name.GetIndex();
	INT Number = Name.GetNumber();
	checkSlow((Index <= 0x00FFFFFF) && (Number < 256));

	// Number is always 0
	return Index | (Number << 24);
}

/*=============================================================================
	FScriptCallstackFrame
=============================================================================*/

/** Constructor for a frame in a stript callstack. */
FScriptCallstackFrame::FScriptCallstackFrame(const UStruct* Function)
	: FunctionName(Function->GetFName())
	, ClassName(Function->GetOuter()->GetFName())
	, PackageName(Function->GetOuter()->GetOuter()->GetFName())
{
}

UBOOL FScriptCallstackFrame::operator ==(const FScriptCallstackFrame& Other) const
{
	return (FunctionName == Other.FunctionName)
		&& (ClassName == Other.ClassName)
		&& (PackageName == Other.PackageName);
}

UBOOL FScriptCallstackFrame::operator !=(const FScriptCallstackFrame& Other) const
{
	return (FunctionName != Other.FunctionName)
		|| (ClassName != Other.ClassName)
		|| (PackageName != Other.PackageName);
}

/*=============================================================================
	FScriptCallstack
=============================================================================*/

/** Constructor for a script callstack. */
FScriptCallstack::FScriptCallstack(const FFrame* Callstack)
{
	while ((Callstack != NULL) && (Callstack->Node != NULL))
	{
		Frames.AddItem(FScriptCallstackFrame(Callstack->Node));
		Callstack = Callstack->PreviousFrame;
	}
}

UBOOL FScriptCallstack::operator ==(const FScriptCallstack& Other) const
{
	if (Frames.Num() != Other.Frames.Num())
	{
		return FALSE;
	}

	for (INT i = 0; i < Frames.Num(); i++)
	{
		if (Frames(i) != Other.Frames(i))
		{
			return FALSE;
		}
	}

	return TRUE;
}

UBOOL FScriptCallstack::operator !=(const FScriptCallstack& Other) const
{
	if (Frames.Num() != Other.Frames.Num())
	{
		return TRUE;
	}

	for (INT i = 0; i < Frames.Num(); i++)
	{
		if (Frames(i) != Other.Frames(i))
		{
			return TRUE;
		}
	}

	return FALSE;
}

/*=============================================================================
	FScriptCallStackDecoder
=============================================================================*/

/** 
 * Register that an object has started internal function call processing. 
 *
 * @param	CurrentFrame	Current execution stack
 *
 */
void FScriptCallStackDecoder::EnterFunction( const FFrame* CurrentStack )
{
	LatestStack = CurrentStack;
	StackSize++;
}

/** 
 * Register that an object has finished internal function call processing. 
 *
 * @param	CurrentFrame	Current execution stack
 *
 */
void FScriptCallStackDecoder::ExitFunction( const FFrame* CurrentStack )
{
	LatestStack = CurrentStack->PreviousFrame;
	StackSize--;
	check(StackSize >= 0);
}

/**
 * Serializes data pertaining to script memory callstacks to the output archiver.
 * If called on the game thread, a callstack index is serialized as well as a flag and optional-value if an allocation type ID follows.
 * Otherwise, it serializes a sentinel value indicating no script call stack data is available (e.g., on the render thread).
 *
 * @param	Ar			Archive to serialize to
 *
 */
void FScriptCallStackDecoder::SerializeScriptCallstack(FArchive& Ar)
{
	UBOOL bAllocatingScriptObject = FALSE;
	WORD CallstackIndex = 0x7FFF;

	if( appGetCurrentThreadId() == GGameThreadId )
	{
		// On the game thread, may have a call stack and/or currently allocating script object.
		if( (StackSize > 0) && (LatestStack != NULL) )
		{
			FScriptCallstack CurrentCallstack(LatestStack);
			CallstackIndex = (WORD)AllocatingCallstacks.AddUniqueItem(CurrentCallstack);
		}

		bAllocatingScriptObject = AllocatingObjectTypeName != NAME_None;
		if( bAllocatingScriptObject )
		{
			CallstackIndex |= 0x8000;
		}
	}

	Ar << CallstackIndex;

	if( bAllocatingScriptObject )
	{
		INT AllocatingObjectTypeIndex = CompactFName(AllocatingObjectTypeName);
		Ar << AllocatingObjectTypeIndex;
	}
}

/** 
 * Writes out all of the stored unique script call stacks to the passed in archiver. 
 *
 * @param	Ar			Archive to serialize to
 *
 */
void FScriptCallStackDecoder::SerializeCallstackTable(FArchive& Ar)
{
	// Write out the number of call stacks
	INT NumCallstacks = AllocatingCallstacks.Num();
	Ar << NumCallstacks;

	debugf(TEXT("FScriptCallStackDecoder::SerializeCallstackTable writing %i callstacks"), NumCallstacks);

	// Write out all the call stacks.
	for (INT i = 0; i < NumCallstacks; i++)
	{
		FScriptCallstack& Callstack = AllocatingCallstacks(i);

		INT NumFrames = Callstack.Frames.Num();
		Ar << NumFrames;

		for (INT j = 0; j < NumFrames; j++)
		{
			FScriptCallstackFrame& Frame = Callstack.Frames(j);

			INT FunctionNameIndex = CompactFName(Frame.FunctionName);
			INT ClassNameIndex = CompactFName(Frame.ClassName);
			INT PackageNameIndex = CompactFName(Frame.PackageName);

			Ar << FunctionNameIndex;
			Ar << ClassNameIndex;
			Ar << PackageNameIndex;
		}
	}
}

/** 
 * Writes out the name table to the passed in archiver. 
 *
 * @param	Ar			Archive to serialize to
 *
 */
void FScriptCallStackDecoder::SerializeNameTable(FArchive& Ar)
{
	// Write out the names.
	INT NumNames = FName::GetMaxNames();

	debugf(TEXT("FScriptCallStackDecoder::SerializeNameTable writing %i names"), NumNames);

	Ar << NumNames;
	for (INT i = 0; i < NumNames; ++i)
	{
		FString Value;

		FNameEntry* Entry = FName::GetEntry(i);
		if (Entry != NULL)
		{
			Value = Entry->GetNameString();
		}

		SerializeStringAsANSICharArray(Value, Ar);
	}
}

/** Returns the number of bytes currently being used by this structure and others it owns. */
DWORD FScriptCallStackDecoder::GetAllocatedSize()
{
	DWORD Total = 0;

	// This doesn't include TArray's owned by ScriptCallstackFrames, so we have to add them up manually below.
	Total += AllocatingCallstacks.GetAllocatedSize();

	for (INT i = 0; i < AllocatingCallstacks.Num(); i++)
	{
		Total += AllocatingCallstacks(i).Frames.GetAllocatedSize();
	}

	Total += NameToNameTableIndexMap.GetAllocatedSize();
	Total += NameArray.GetAllocatedSize();

	return Total;
}

/** 
 * Allocation of an object has started. 
 *
 * @param	ObjectClass		Class the object belongs to
 *
 */
void FScriptCallStackDecoder::BeginAllocateObject( const UClass* ObjectClass )
{
	check(appGetCurrentThreadId() == GGameThreadId);
	AllocatingObjectTypeName = ObjectClass->GetFName();
}

/** Allocation of an object has finished. */
void FScriptCallStackDecoder::EndAllocateObject()
{
	check(appGetCurrentThreadId() == GGameThreadId);
	AllocatingObjectTypeName = NAME_None;
}

#endif // USE_MALLOC_PROFILER