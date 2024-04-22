#include "EnginePrivate.h"

#if USE_AI_PROFILER
#include "EngineAIClasses.h"

/** Magic number used for .aiprof file type and endianness detection */
#define AIP_MAGIC_NUMBER	0xE408F9A2

/** File version number for generated .aiprof files */
#define AIP_VERSION_NUMBER	1

/** Enumeration of token types */
enum EAIPTokenType
{
	AIPToken_AILog,						// Token for AI logging
	AIPToken_ControllerDestroyed,		// Token signifying the destruction of an AI controller
	AIPToken_EOS,						// End of stream token
	AIPToken_Invalid,					// Invalid token
};

/** Base class for all tokens emitted to the profiling file */
struct FAIPBaseToken
{
	/** Type of the token emitted */
	BYTE TokenType;

	/**
	 * Constructor
	 *
	 * @param	InTokenType	Token type specified by this token
	 */
	FAIPBaseToken( BYTE InTokenType )
		: TokenType( InTokenType )
	{}

	/**
	 * Serialization operator
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Token		Token to serialize
	 *
	 * @return	Passed in archive
	 */
	friend FArchive& operator<<( FArchive& Ar, FAIPBaseToken& Token )
	{
		Ar << Token.TokenType;
		return Ar;
	}
};

/** Base class for all tokens emitted by an AI controller */
struct FAIPEmittedToken : public FAIPBaseToken
{
	/** Index of the token's controller in the profiler's controller table */
	INT		ControllerInfoIndex;

	/** Index of the token's category name in the profiler's name table */
	INT		EventCategoryNameIndex;

	/** Index of the token's controller's pawn's name in the profiler's name table, if any */
	INT		PawnNameIndex;

	/** Instance number of the token's controller's pawn's name, if any */
	INT		PawnNameInstance;

	/** Index of the token's controller's pawn's class name in the profiler's name table, if any */
	INT		PawnClassNameIndex;

	/** Index of the token's controller's active command's class name in the profiler's name table, if any */
	INT		CommandClassNameIndex;

	/** Index of the controller's state name in the profiler's name table */
	INT		StateNameIndex;

	/** Time when the token was emitted, as measured by WorldInfo.TimeSeconds */
	FLOAT	WorldTimeSeconds;

	/**
	 * Constructor
	 *
	 * @param	InTokenType	Token type of the token
	 */
	FAIPEmittedToken( BYTE InTokenType )
		:	FAIPBaseToken( InTokenType ),
			ControllerInfoIndex( INDEX_NONE ),
			EventCategoryNameIndex( INDEX_NONE ),
			PawnNameIndex( INDEX_NONE ),
			PawnNameInstance( INDEX_NONE ),
			PawnClassNameIndex( INDEX_NONE ),
			CommandClassNameIndex( INDEX_NONE ),
			StateNameIndex( INDEX_NONE ),
			WorldTimeSeconds( 0.0f )
	{}

	/**
	 * Serialization operator
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Token		Token to serialize
	 *
	 * @return	Passed in archive
	 */
	friend FArchive& operator<<( FArchive& Ar, FAIPEmittedToken& Token )
	{
		// Ensure base class token is serialized first
		Ar << static_cast<FAIPBaseToken&>( Token );
		Ar << Token.ControllerInfoIndex;
		Ar << Token.EventCategoryNameIndex;
		Ar << Token.PawnNameIndex;
		Ar << Token.PawnNameInstance;
		Ar << Token.PawnClassNameIndex;
		Ar << Token.CommandClassNameIndex;
		Ar << Token.StateNameIndex;
		Ar << Token.WorldTimeSeconds;
		return Ar;
	}
};

/** Token representing an AI log event */
struct FAIPLogToken : public FAIPEmittedToken
{
	/** 
	 * Log text for the AI event; Cannot be stored in the name table because it could contain unique data (such as a timestamp or position)
	 * causing the name table to be extremely large
	 */
	FString	LogText;

	/**
	 * Constructor
	 *
	 * @param	InLogText	Logging text to be emitted in the token
	 */
	FAIPLogToken( const FString& InLogText )
		:	FAIPEmittedToken( AIPToken_AILog ),
			LogText( InLogText )
	{}

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Token		Token to serialize
	 *
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FAIPLogToken& Token )
	{
		// Ensure the base class token is serialized first
		Ar << static_cast<FAIPEmittedToken&>( Token );
		SerializeStringAsANSICharArray( Token.LogText, Ar );
		return Ar;
	}
};

/** Token for an event marking the destruction of an AI controller */
struct FAIPControllerDestroyedToken : public FAIPEmittedToken
{

	/** Constructor */
	FAIPControllerDestroyedToken()
		:	FAIPEmittedToken( AIPToken_ControllerDestroyed )
	{}

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Token		Token to serialize
	 *
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FAIPControllerDestroyedToken& Token )
	{
		// Ensure the base class token is serialized
		Ar	<< static_cast<FAIPEmittedToken&>( Token );
		return Ar;
	}
};

/** Token representing the end of the profiling stream; not emitted by any AI controller */
struct FAIPEndOfStreamToken : public FAIPBaseToken
{

	/** Constructor */
	FAIPEndOfStreamToken()
		: FAIPBaseToken( AIPToken_EOS )
	{}

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Token		Token to serialize
	 *
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FAIPEndOfStreamToken& Token )
	{
		Ar << static_cast<FAIPBaseToken&>( Token );
		return Ar;
	}
};

/** File header for the profiling output file */
struct FAIPHeader
{
	/** Magic to ensure we're opening the right file */
	DWORD Magic;

	/** Version number of the file */
	DWORD VersionNumber;

	/** Offset in file for the name table */
	DWORD NameTableOffset;

	/** Number of name table entries */
	DWORD NameTableEntries;

	/** Offset in file for the controller info table */
	DWORD ControllerInfoTableOffset;

	/** Number of controller info table entries */
	DWORD ControllerInfoEntries;

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Header		Header to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FAIPHeader Header )
	{
		Ar	<< Header.Magic
			<< Header.VersionNumber
			<< Header.NameTableOffset
			<< Header.NameTableEntries
			<< Header.ControllerInfoTableOffset
			<< Header.ControllerInfoEntries;
		check( Ar.IsSaving() );
		return Ar;
	}
};

/** Initialize the profiler and create the profiling output file */
void FAIProfiler::Init()
{
	// Ensure the profiling directory exists and create a new profiling file there
	FileName = appProfilingDir() + GGameName + TEXT("-") + appSystemTimeString() + TEXT(".aiprof");
	GFileManager->MakeDirectory( *appProfilingDir() );
	FileWriter = GFileManager->CreateFileWriter( *FileName, FILEWRITE_NoFail | FILEWRITE_Async );
	checkf( FileWriter );

	// Write a dummy header to the file (will be overwritten at the end of profiling with correct info)
	FAIPHeader DummyFileHeader;
	appMemzero( &DummyFileHeader, sizeof( DummyFileHeader ) );
	(*FileWriter) << DummyFileHeader;

	// Initialize the memory writer to reduce allocations
	MemoryWriter.Empty( 100 * 1024 );

	bInitialized = TRUE;
	bShouldToggleCaptureNextTick = FALSE;
}

/** Shutdown the profiler, finalizing output to the profiling file */
void FAIProfiler::Shutdown()
{
	FlushMemoryWriter();

	// Emit a token signifying the end of the stream
	FAIPEndOfStreamToken EndStreamToken;
	(*FileWriter) << EndStreamToken;

	// Populate the real file header now that profiling is complete
	// and all of the data is available
	FAIPHeader FileHeader;
	FileHeader.Magic = AIP_MAGIC_NUMBER;
	FileHeader.VersionNumber = AIP_VERSION_NUMBER;
	FileHeader.NameTableOffset = FileWriter->Tell();
	FileHeader.NameTableEntries = NameIndexLookupMap.Num();

	// Serialize the name table to the file
	for ( INT NameIndex = 0; NameIndex < NameIndexLookupMap.Num(); ++NameIndex )
	{
		SerializeStringAsANSICharArray( NameIndexLookupMap(NameIndex), *FileWriter );
	}

	FileHeader.ControllerInfoTableOffset = FileWriter->Tell();
	FileHeader.ControllerInfoEntries = ControllerIndexLookupMap.Num();

	// Serialize the controller info table to the file
	for ( INT ControllerIndex = 0; ControllerIndex < ControllerIndexLookupMap.Num(); ++ControllerIndex )
	{
		FAIPControllerInfo CurControllerInfo = ControllerIndexLookupMap(ControllerIndex);
		(*FileWriter) << CurControllerInfo;
	}

	// Seek back to the beginning of the file and serialize the correct header out
	FileWriter->Seek( 0 );
	(*FileWriter) << FileHeader;

	// Clean up the file writer
	FileWriter->Close();
	delete FileWriter;
	FileWriter = NULL;

	bInitialized = FALSE;
	bShouldToggleCaptureNextTick = FALSE;
}

/**
 * Returns whether the profiler is initialized or not
 *
 * @return	TRUE if the profiler is initialized, FALSE if it is not
 */
UBOOL FAIProfiler::IsInitialized() const
{
	return bInitialized;
}

/** Toggles the state of the profiler during the next update tick (from initialized to shutdown, or vice versa) */
void FAIProfiler::ToggleCaptureStateNextTick()
{
	bShouldToggleCaptureNextTick = TRUE;
}

/** Tick/update the profiler, called every game thread tick */
void FAIProfiler::Tick()
{
	// Flush the memory writer if actively profiling
	if ( bInitialized )
	{
		FlushMemoryWriter();
	}

	// If a state toggle is requested, shutdown or initialize the profiler as appropriate
	if ( bShouldToggleCaptureNextTick )
	{
		if ( bInitialized )
		{
			Shutdown();
		}
		else
		{
			Init();
		}
	}
}


/**
 * Emit a profiling token signifying an AI log event
 *
 * @param	InAIController	AI controller emitting the log event (*must* be non-NULL)
 * @param	InCommand		Current active AI command for the provided controller, if any
 * @param	InLogText		Log text that makes up the log event
 * @param	InEventCategory	Category of the log event
 */
void FAIProfiler::AILog( AAIController* InAIController, UAICommandBase* InCommand, const FString& InLogText, const FName& InEventCategory )
{
	if ( bInitialized )
	{
		check( InAIController );

		FAIPLogToken LogToken( InLogText );
		PopulateEmittedToken( InAIController, InCommand, InEventCategory, LogToken );
		
		MemoryWriter << LogToken;
	}
}

/**
 * Emit a profiling token signifying the destruction of an AIController
 *
 * @param	InAIController	AIController about to be destroyed (*must* be non-NULL)
 * @param	InCommand		Current active AI command for the provided controller, if any
 * @param	InEventCategory	Category of the event
 */
void FAIProfiler::AIControllerDestroyed( AAIController* InAIController, UAICommandBase* InCommand, const FName& InEventCategory )
{
	if ( bInitialized )
	{
		check( InAIController );
		FAIPControllerDestroyedToken DestroyedToken;
		PopulateEmittedToken( InAIController, InCommand, InEventCategory, DestroyedToken );

		MemoryWriter << DestroyedToken;
	}
}

/**
 * Exec handler. Parses command and returns TRUE if handled.
 *
 * @param	Cmd		Command to parse
 * @param	Ar		Output device to use for logging
 * @return	TRUE if handled, FALSE otherwise
 */
UBOOL FAIProfiler::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( ParseCommand( &Cmd ,TEXT("PROFILEAI") ) || ParseCommand( &Cmd, TEXT("AIPROFILER") ) )
	{
		// User has requested to start profiling, toggle the capture state of the profiler
		// if it's inactive
		if( ParseCommand( &Cmd, TEXT("START") ) )
		{
			if ( !FAIProfiler::GetInstance().IsInitialized() )
			{
				FAIProfiler::GetInstance().ToggleCaptureStateNextTick();
			}
			return TRUE;
		}

		// User has requested to stop profiling, toggle the capture state of the profiler
		// if it's active
		else if( ParseCommand( &Cmd, TEXT("STOP") ) )
		{
			if ( FAIProfiler::GetInstance().IsInitialized() )
			{
				FAIProfiler::GetInstance().ToggleCaptureStateNextTick();
			}
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Singleton interface, returns the lone instance of the profiler class.
 *
 * @return Instance of the profiler
 */
FAIProfiler& FAIProfiler::GetInstance()
{
	static FAIProfiler Instance;
	return Instance;
}

/** Constructor */
FAIProfiler::FAIProfiler()
:	bInitialized( FALSE ),
	bShouldToggleCaptureNextTick( FALSE ),
	FileWriter( NULL )
{

}

/** Destructor */
FAIProfiler::~FAIProfiler()
{
	if ( bInitialized )
	{
		Shutdown();
	}
}

/**
 * Returns the index of the provided string in the profiler's name table. If the name doesn't
 * already exist in the table, it is added.
 *
 * @param	InName		Name to find the index of in the name table
 *
 * @return	Index of the provided name in the profiler's name table
 */
INT FAIProfiler::GetNameIndex( const FString& InName )
{
	return NameIndexLookupMap.AddItem( InName );
}


/**
 * Returns the index of the provided controller info in the profiler's controller table. If the controller
 * doesn't already exist in the table, it is added.
 *
 * @param	InControllerInfo	Controller info to find the index of in the controller table
 *
 * @return	Index of the provided controller info in the profiler's controller table
 */
INT FAIProfiler::GetControllerIndex( const FAIPControllerInfo& InControllerInfo )
{
	return ControllerIndexLookupMap.AddItem( InControllerInfo );
}

/**
 * Helper method to populate a token with basic information about the controller causing
 * its emission
 *
 * @param	InAIController	AI controller causing the token emission (*must* be non-NULL)
 * @param	InCommand		Current command of the provided AI controller, if any
 * @param	InEventCategory	Event category of the token emission
 * @param	OutEmittedToken	Token to populate
 */
void FAIProfiler::PopulateEmittedToken( AAIController* InAIController, UAICommandBase* InCommand, const FName& InEventCategory, struct FAIPEmittedToken& OutEmittedToken )
{
	FAIPControllerInfo CurController;
	ExtractControllerInfo( InAIController, CurController );

	OutEmittedToken.ControllerInfoIndex = GetControllerIndex( CurController );
	OutEmittedToken.EventCategoryNameIndex = GetNameIndex( InEventCategory.GetNameString() );
	OutEmittedToken.WorldTimeSeconds = InAIController->WorldInfo->TimeSeconds;

	if ( InCommand )
	{
		OutEmittedToken.CommandClassNameIndex = GetNameIndex( InCommand->GetClass()->GetFName().GetNameString() );
		OutEmittedToken.StateNameIndex = GetNameIndex( InCommand->GetStateName().GetNameString() );
	}
	else
	{
		OutEmittedToken.StateNameIndex = GetNameIndex( InAIController->GetStateName().GetNameString() );
	}

	if ( InAIController->Pawn )
	{
		OutEmittedToken.PawnNameIndex = GetNameIndex( InAIController->Pawn->GetFName().GetNameString() );
		OutEmittedToken.PawnNameInstance = InAIController->Pawn->GetFName().GetNumber();
		OutEmittedToken.PawnClassNameIndex = GetNameIndex( InAIController->Pawn->GetClass()->GetFName().GetNameString() );
	}
}

/**
 * Helper method to populate a controller info struct with information from the provided AI controller
 *
 * @param	InAIController		Controller to use to populate the controller info struct with
 * @param	OutControllerInfo	Controller info to populate from the provided controller
 */
void FAIProfiler::ExtractControllerInfo( const AAIController* InAIController, FAIPControllerInfo& OutControllerInfo )
{
	check( InAIController );
	OutControllerInfo.ControllerNameIndex = GetNameIndex( InAIController->GetFName().GetNameString() );
	OutControllerInfo.ControllerNameInstance = InAIController->GetFName().GetNumber();
	OutControllerInfo.ControllerClassNameIndex = GetNameIndex( InAIController->GetClass()->GetFName().GetNameString() );
	OutControllerInfo.ControllerCreationTime = InAIController->CreationTime;
}

/** Helper method to flush the memory writer to HDD */
void FAIProfiler::FlushMemoryWriter()
{
	FileWriter->Serialize( MemoryWriter.GetData(), MemoryWriter.Num() );
	MemoryWriter.Seek( 0 );
	MemoryWriter.Empty( 100 * 1024 );
}

#endif // #if USE_AI_PROFILER
