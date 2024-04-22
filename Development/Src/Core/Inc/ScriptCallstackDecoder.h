/*=============================================================================
	ScriptCallstackDecoder.h: Tracks the current script callstack for memory profiling
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SCRIPTCALLSTACKDECODER_H__
#define __SCRIPTCALLSTACKDECODER_H__

#if USE_MALLOC_PROFILER_DECODE_SCRIPT

/** This class represents one frame in a script callstack. */
struct FScriptCallstackFrame
{
public:
	/** Function name. */
	const FName FunctionName;

	/** Class name. */
	const FName ClassName;

	/** Package name. */
	const FName PackageName;

	/** Constructor for a frame in a stript callstck. */
	FScriptCallstackFrame(const UStruct* Function);

	UBOOL operator ==(const FScriptCallstackFrame& Other) const;
	UBOOL operator !=(const FScriptCallstackFrame& Other) const;
};

/** This class represents a script callstack. */
struct FScriptCallstack
{
public:
	/** All frames which belongs to this script callstack. */
	TArray<FScriptCallstackFrame> Frames;

	/** Constructor for a script callstack. */
	FScriptCallstack(const FFrame* Callstack);

	UBOOL operator ==(const FScriptCallstack& Other) const;
	UBOOL operator !=(const FScriptCallstack& Other) const;
};

/** This class tracks unique script call stacks at allocation/deallocation sites. */
class FScriptCallStackDecoder
{
public:
	/** Constructor for the script callstack tracker. */
	FScriptCallStackDecoder()
		: LatestStack(NULL)
		, AllocatingObjectTypeName(NAME_None)
		, StackSize(0)
	{ }

	/** Destructor for the script call stack tracker. */
	~FScriptCallStackDecoder() { }

	/** 
	 * Register that an object has started internal function call processing. 
	 *
	 * @param	CurrentFrame	Current execution stack
	 *
	 */
	void EnterFunction( const FFrame* CurrentStack );

	/** 
	 * Register that an object has finished internal function call processing. 
	 *
	 * @param	CurrentFrame	Current execution stack
	 *
	 */
	void ExitFunction( const FFrame* CurrentStack );

	/**
	 * Serializes data pertaining to script memory callstacks to the output archiver.
	 * If called on the game thread, a callstack index is serialized as well as a flag and optional-value if an allocation type ID follows.
	 * Otherwise, it serializes a sentinel value indicating no script call stack data is available (e.g., on the render thread).
	 *
	 * @param	Ar			Archive to serialize to
	 *
	 */
	void SerializeScriptCallstack(FArchive& Ar);

	/** 
	 * Writes out all of the stored unique script call stacks to the passed in archiver. 
	 *
	 * @param	Ar			Archive to serialize to
	 *
	 */
	void SerializeCallstackTable(FArchive& Ar);

	/** 
	 * Writes out the name table to the passed in archiver. 
	 *
	 * @param	Ar			Archive to serialize to
	 *
	 */
	void SerializeNameTable(FArchive& Ar);

	/** Returns the number of bytes currently being used by this structure and others it owns. */
	DWORD GetAllocatedSize();

	/** 
	 * Allocation of an object has started. 
	 *
	 * @param	ObjectClass		Class the object belongs to
	 *
	 */
	void BeginAllocateObject( const UClass* ObjectClass );

	/** Allocation of an object has finished. */
	void EndAllocateObject();

protected:
	/** The set of all unique callstacks seen. */
	TArray<FScriptCallstack> AllocatingCallstacks;

	/** Mapping from name to index in name array. */
	TMap<FString, INT> NameToNameTableIndexMap;

	/** Array of unique names. */
	TArray<FString> NameArray;

	/** The top-most script stack frame. */
	const FFrame* LatestStack;

	/** The type name of the object currently being allocated by script code, or NAME_None. */
	FName AllocatingObjectTypeName;

	/** The current height of the script call stack. */
	INT StackSize;
};

#endif // USE_MALLOC_PROFILER

#endif // __SCRIPTCALLSTACKDECODER_H__
