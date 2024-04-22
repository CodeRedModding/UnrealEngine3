/*=============================================================================
	GameplayProfiler.cpp: Gameplay profiling support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#if USE_GAMEPLAY_PROFILER

#include "ProfilingHelpers.h"

/*-----------------------------------------------------------------------------
	Misc
-----------------------------------------------------------------------------*/

/** Global gameplay profiler object. */
FGameplayProfiler* GGameplayProfiler;

/** Magic number used for .gprof file type and endianness detection. */
//*** This MUST match the value in the Tools\GameplayProfiler project!
//#define GPP_MAGIC_NUMBER 0x07210322	// The old magic number...
#define GPP_MAGIC_NUMBER	0x03220721
/** Version number for allowing changes to the gprof format	*/
#define GPP_VERSION			1


/*-----------------------------------------------------------------------------
	Tokens emitted by stats code
-----------------------------------------------------------------------------*/


struct FGPPObjectInfo
{
	/** Constructor, initializing all members */
	FGPPObjectInfo( EGPPTokenType Type, UObject* Object, UObject* AssetObject )
	:	TokenType( Type )
	{
		ObjectNameInstance		= (WORD)Object->GetFName().GetNumber();
		ObjectNameIndex			= Object->GetFName().GetIndex();
		ObjectClassNameIndex	= Object->GetClass()->GetFName().GetIndex();
		OuterNameIndex			= Object->GetOuter()->GetFName().GetIndex();
		OuterNameInstance		= Object->GetOuter()->GetFName().GetNumber();
		OutermostNameIndex		= Object->GetOutermost()->GetFName().GetIndex();
		AssetObjectNameIndex	= AssetObject ? AssetObject->GetFName().GetIndex() : 0;
	}

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Token		Token to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FGPPObjectInfo Token )
	{
		Ar << Token.TokenType;
		// Function's don't have _Number
		if( Token.TokenType != GPPToken_Function )
		{
			Ar << Token.ObjectNameInstance;
		}
		Ar << Token.ObjectNameIndex;
		Ar << Token.ObjectClassNameIndex;
		// An Actor's outer is always the ULevel
		if( Token.TokenType != GPPToken_Actor )
		{
			Ar << Token.OuterNameIndex;
		}
		if( Token.TokenType == GPPToken_Component )
		{
			Ar << Token.OuterNameInstance;
		}
		Ar << Token.OutermostNameIndex;
		Ar << Token.AssetObjectNameIndex;
		return Ar;
	}

private:
	/** Token type */
	BYTE	TokenType;
	/** Instance # of object name, like e.g. 1 for ActorComponent_1 */
	INT		ObjectNameInstance;
	/** Index into name table for object */
	INT		ObjectNameIndex;
	/** Index into name table for object's class */
	INT		ObjectClassNameIndex;
	/** Index into name table for outer */
	INT		OuterNameIndex;
	/** Instance # of outer. Useful for components to identify the actor. */
	INT		OuterNameInstance;
	/** Index into name table for outermost */
	INT		OutermostNameIndex;
	/** Index into name table for asset object */
	INT		AssetObjectNameIndex;
};

struct FGPPEndOfScopeMarker
{
	/** Token type */
	BYTE	TokenType;
	/** Whether this scope should be skipped */
	BYTE	bShouldSkipInDetailedView;
	/** Delta time for scope that just ended */
	DWORD	DeltaCycles;

	/** Constructor, initializing token type */
	FGPPEndOfScopeMarker()
	:	TokenType( GPPToken_EndOfScope )
	{}

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Token		Token to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FGPPEndOfScopeMarker Token )
	{
		Ar << Token.TokenType;
		Ar << Token.bShouldSkipInDetailedView;
		Ar << Token.DeltaCycles;
		return Ar;
	}
};

struct FGPPFrameMarker
{
	/** Token type */
	BYTE	TokenType;
	/** Delta time for frame that just ended. */
	FLOAT	FrameTime;

	/** Constructor, initializing token type */
	FGPPFrameMarker()
	:	TokenType( GPPToken_Frame )
	{}

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Token		Token to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FGPPFrameMarker Token )
	{
		Ar << Token.TokenType;
		Ar << Token.FrameTime;
		return Ar;
	}
};

struct FGPPEndOfStreamMarker
{
	/** Token type */
	BYTE	TokenType;

	/** Constructor, initializing token type */
	FGPPEndOfStreamMarker()
	:	TokenType( GPPToken_EOS )
	{}

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Token		Token to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FGPPEndOfStreamMarker Token )
	{
		Ar << Token.TokenType;
		return Ar;
	}
};


#if STATS
struct FGPPCycleStatInfo
{
	/** Constructor, initializing all members */
	FGPPCycleStatInfo( const FCycleStat& InitCycleStat )
	:	TokenType( GPPToken_CycleStat ),
		CycleStatNameIndex( InitCycleStat.CounterFName.GetIndex() )
	{
	}

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Token		Token to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FGPPCycleStatInfo Token )
	{
		Ar << Token.TokenType;
		Ar << Token.CycleStatNameIndex;
		return Ar;
	}

private:
	/** Token type */
	BYTE	TokenType;
	/** Index into name table for cycle stat name */
	INT		CycleStatNameIndex;
};
#endif		// STATS

/*-----------------------------------------------------------------------------
	Header for file.
-----------------------------------------------------------------------------*/


struct FGPPHeader
{
	/** Magic to ensure we're opening the right file.	*/
	DWORD	Magic;
	/** Version number									*/
	DWORD	Version;
	/** Conversion from cycles to seconds.				*/
	DOUBLE	SecondsPerCycle;

	/** Offset in file for name table.					*/
	DWORD	NameTableOffset;
	/** Number of name table entries.					*/
	DWORD	NameTableEntries;

	/** Offset in file for class hierarchy.				*/
	DWORD	ClassHierarchyOffset;
	/** Number of classes in hierarchy.					*/
	DWORD	ClassCount;

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Header		Header to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FGPPHeader Header )
	{
		Ar	<< Header.Magic
			<< Header.Version
			<< Header.SecondsPerCycle
			<< Header.NameTableOffset
			<< Header.NameTableEntries
			<< Header.ClassHierarchyOffset
			<< Header.ClassCount;
		check( Ar.IsSaving() );
		return Ar;
	}
};




/*-----------------------------------------------------------------------------
	FGameplayProfiler implementation
-----------------------------------------------------------------------------*/

/** Whether to toggle capturing the next tick. */
UBOOL FGameplayProfiler::bShouldToggleCaptureNextTick = FALSE;
/** Last frame time. */
DOUBLE FGameplayProfiler::LastFrameTime = 0.0;
/** If > 0 indicates the time at which to stop the capture. */
DOUBLE FGameplayProfiler::TimeToStopCapture = 0.0;


/**
 * Constructor.
 */
FGameplayProfiler::FGameplayProfiler()
:	FileWriter(NULL)
{
	FileName = appProfilingDir() + GGameName + TEXT("-") + appSystemTimeString() + TEXT(".gprof");
	GFileManager->MakeDirectory( *appProfilingDir() );
	FileWriter = GFileManager->CreateFileWriter( *FileName, FILEWRITE_NoFail | FILEWRITE_Async);
	checkf( FileWriter );
	
	// Write out dummy header.
	FGPPHeader Header;
	appMemzero( &Header, sizeof(Header) );
	(*FileWriter) << Header;

	// Presize memory writer to avoid re-allocation spikes.
	MemoryWriter.Empty( 100 * 1024 );
}

/**
 * Destructur, finishing up serialization of data.
 */
FGameplayProfiler::~FGameplayProfiler()
{
	EmitEndMarker();

	// Serialize out real header.
	FGPPHeader Header;
	Header.Magic = GPP_MAGIC_NUMBER;
	Header.Version = GPP_VERSION;
	Header.SecondsPerCycle	= GSecondsPerCycle;
	Header.NameTableOffset	= FileWriter->Tell();
	Header.NameTableEntries	= FName::GetPureNamesTable().Num();

	// Serialize out full name table. In theory we could avoid ones we don't encounter but it's only a few MByte.
	const TArray<FNameEntry*>& NameTable = FName::GetPureNamesTable();
	for( INT NameIndex=0; NameIndex<NameTable.Num(); NameIndex++ )
	{
		SerializeStringAsANSICharArray( NameTable(NameIndex) ? *NameTable(NameIndex)->GetNameString() : TEXT("Invalid"), *FileWriter );
	}

	// Class hierarchy.
	Header.ClassHierarchyOffset = FileWriter->Tell();
	Header.ClassCount = 0;
	for( TObjectIterator<UClass> It; It; ++It )
	{
		// Serialize the classes parent chain, from child/ current to outermost object class.
		UClass* Class = *It;
		while( Class )
		{
			INT ClassNameIndex = Class->GetFName().GetIndex();
			(*FileWriter) << ClassNameIndex;
			Class = Class->GetSuperClass();
		}
		// Emit end marker using invalid index -1.
		INT Stopper = INDEX_NONE;
		(*FileWriter) << Stopper;

		Header.ClassCount++;
	}

	// Serialize out header with proper offsets to beginning of file.
	FileWriter->Seek(0);
	(*FileWriter) << Header;

	// Wrap it up.
	FileWriter->Close();
	delete FileWriter;
	FileWriter = NULL;
}

/**
 * Start tracking a function. Code relies on matching nested begin/ end.
 *
 * @param	Object	Object to track
 */
void FGameplayProfiler::BeginTrackObject( UObject* Object, UObject* AssetObject, EGPPTokenType Type )
{
	FGPPObjectInfo Token(Type,Object,AssetObject);
	MemoryWriter << Token;
}
	
/**
 * End tracking object.
 *
 * @param	DeltaCycles		Cycles elapsed since tracking started.
 * @param	bShouldSkipInDetailedView	Whether this scope should be skipped in detailed view
 */
void FGameplayProfiler::EndTrackObject( DWORD DeltaCycles, UBOOL bShouldSkipInDetailedView )
{
	FGPPEndOfScopeMarker Token;
	Token.DeltaCycles = DeltaCycles;
	Token.bShouldSkipInDetailedView = bShouldSkipInDetailedView ? TRUE : FALSE;
	MemoryWriter << Token;
}

#if STATS
/**
 * Start tracking a cycle stat. Code relies on matching nested begin/ end.
 *
 * @param	Name	Name of stat to track
 */
void FGameplayProfiler::BeginTrackCycleStat( const FCycleStat& CycleStat )
{
	FGPPCycleStatInfo Token(CycleStat);
	MemoryWriter << Token;
}
#endif
	
/**
 * Exec handler. Parses command and returns TRUE if handled.
 *
 * @param	Str		Command to parse
 * @param	Ar		Output device to use for logging
 * @return	TRUE if handled, FALSE otherwise
 */
UBOOL FGameplayProfiler::Exec( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("PROFILEGAME")) || ParseCommand(&Str,TEXT("GAMEPROFILER")) )
	{
		if( ParseCommand(&Str,TEXT("START")) )
		{
			if( !GGameplayProfiler )
			{
				FGameplayProfiler::bShouldToggleCaptureNextTick = TRUE;
			}
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("STOP")) )
		{
			if( GGameplayProfiler )
			{
				FGameplayProfiler::bShouldToggleCaptureNextTick = TRUE;
			}
			return TRUE;
		}
		else
		{
			FLOAT Duration = appAtof( Str );
			if( !GGameplayProfiler )
			{
				FGameplayProfiler::bShouldToggleCaptureNextTick = TRUE;
				FGameplayProfiler::TimeToStopCapture = GCurrentTime + Duration;
			}
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Flushes the memory writer to HDD.
 */	
void FGameplayProfiler::FlushMemoryWriter()
{
	//debugf(TEXT("FGameplayProfiler::FlushMemoryWriter writing %5.2f KBytes"), MemoryWriter.Num() / 1024);
	FileWriter->Serialize( MemoryWriter.GetData(), MemoryWriter.Num() );
	MemoryWriter.Seek(0);
	MemoryWriter.Empty( 100 * 1024 );
}

/**
 * Called every game thread tick.
 */
void FGameplayProfiler::Tick()
{
	// Emit frame marker for ending frame including time it took.
	if( GGameplayProfiler )
	{
		GGameplayProfiler->FlushMemoryWriter();
		FGPPFrameMarker Token;
		Token.FrameTime = appSeconds() - LastFrameTime;
		(*(GGameplayProfiler->FileWriter)) << Token;
	}
	LastFrameTime = appSeconds();

	// Start/ stop capture if wanted.
	if( bShouldToggleCaptureNextTick 
	||	(TimeToStopCapture > 0 && GCurrentTime > TimeToStopCapture) )
	{
		if( GGameplayProfiler )
		{
			TimeToStopCapture = 0;
			FString FileName = GGameplayProfiler->GetFileName();
			delete GGameplayProfiler;
			GGameplayProfiler = NULL;
			debugf( TEXT( "GameplayProfiler STOPPING capture.") );
			SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!SCRIPT:"), *FileName );
		}
		else
		{
			GGameplayProfiler = new FGameplayProfiler();
			debugf( TEXT( "GameplayProfiler STARTING capture.") );
		}
		bShouldToggleCaptureNextTick = FALSE;
	}	
}

/**
 * Emits an end of file marker into the data stream.
 */
void FGameplayProfiler::EmitEndMarker()
{
	FlushMemoryWriter();
	FGPPEndOfStreamMarker Token;
	(*FileWriter) << Token;
}

/**
* Returns true if currently active
*/
UBOOL FGameplayProfiler::IsActive()
{
	return (GGameplayProfiler != NULL);
}


#endif // USE_GAMEPLAY_PROFILER