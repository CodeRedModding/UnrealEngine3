/*=============================================================================
	UnMisc.h: Misc helper classes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * Growable compressed buffer. Usage is to append frequently but only request and therefore decompress
 * very infrequently. The prime usage case is the memory profiler keeping track of full call stacks.
 */
struct FCompressedGrowableBuffer
{
	/**
	 * Constructor
	 *
	 * @param	MaxPendingBufferSize	Max chunk size to compress in uncompressed bytes
	 * @param	CompressionFlags		Compression flags to compress memory with
	 */
	FCompressedGrowableBuffer( INT MaxPendingBufferSize, ECompressionFlags CompressionFlags );

	/**
	 * Locks the buffer for reading. Needs to be called before calls to Access and needs
	 * to be matched up with Unlock call.
	 */
	void Lock();
	/**
	 * Unlocks the buffer and frees temporary resources used for accessing.
	 */
	void Unlock();

	/**
	 * Appends passed in data to the buffer. The data needs to be less than the max
	 * pending buffer size. The code will assert on this assumption.	 
	 *
	 * @param	Data	Data to append
	 * @param	Size	Size of data in bytes.
	 * @return	Offset of data, used for retrieval later on
	 */
	INT Append( void* Data, INT Size );

	/**
	 * Accesses the data at passed in offset and returns it. The memory is read-only and
	 * memory will be freed in call to unlock. The lifetime of the data is till the next
	 * call to Unlock, Append or Access
	 *
	 * @param	Offset	Offset to return corresponding data for
	 */
	void* Access( INT Offset );

	/**
	 * @return	Number of entries appended.
	 */
	INT Num() const
	{
		return NumEntries;
	}

	/** 
	 * Helper function to return the amount of memory allocated by this buffer 
	 *
	 * @return number of bytes allocated by this buffer
	 */
	DWORD GetAllocatedSize() const
	{
		return CompressedBuffer.GetAllocatedSize()
			+ PendingCompressionBuffer.GetAllocatedSize()
			+ DecompressedBuffer.GetAllocatedSize()
			+ BookKeepingInfo.GetAllocatedSize();
	}

private:
	/** Helper structure for book keeping. */
	struct FBufferBookKeeping
	{
		/** Offset into compressed data.				*/
		INT CompressedOffset;
		/** Size of compressed data in this chunk.		*/
		INT CompressedSize;
		/** Offset into uncompressed data.				*/
		INT UncompressedOffset;
		/** Size of uncompressed data in this chunk.	*/
		INT UncompressedSize;
	};

	/** Maximum chunk size to compress in uncompressed bytes.				*/
	INT					MaxPendingBufferSize;
	/** Compression flags used to compress the data.						*/
	ECompressionFlags	CompressionFlags;
	/** Current offset in uncompressed data.								*/
	INT					CurrentOffset;
	/** Number of entries in buffer.										*/
	INT					NumEntries;
	/** Compressed data.													*/
	TArray<BYTE>		CompressedBuffer;
	/** Data pending compression once size limit is reached.				*/
	TArray<BYTE>		PendingCompressionBuffer;
	/** Temporary decompression buffer used between Lock/ Unlock.			*/
	TArray<BYTE>		DecompressedBuffer;
	/** Index into book keeping info associated with decompressed buffer.	*/
	INT					DecompressedBufferBookKeepingInfoIndex;
	/** Book keeping information for decompression/ access.					*/
	TArray<FBufferBookKeeping>	BookKeepingInfo;
};


/**
 * Serializes a string as ANSI char array.
 *
 * @param	String			String to serialize
 * @param	Ar				Archive to serialize with
 * @param	MinCharacters	Minimum number of characters to serialize.
 */
extern void SerializeStringAsANSICharArray( const FString& String, FArchive& Ar, INT MinCharacters=0 );

/**
 * Parses a string into tokens, separating switches (beginning with - or /) from
 * other parameters
 *
 * @param	CmdLine		the string to parse
 * @param	Tokens		[out] filled with all parameters found in the string
 * @param	Switches	[out] filled with all switches found in the string
 */
void appParseCommandLine(const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches);

/**
 * Function to encrypt and decrypt an array of bytes using obscurity
 * 
 * @param InAndOutData data to encrypt or decrypt, and also the result
 * @param Offset byte-offset to start encrypting/decrypting
 */
void SecurityByObscurityEncryptAndDecrypt(TArray<BYTE>& InAndOutData, INT Offset=0);


/**
 * Takes the property name and breaks it down into a human readable string.
 * For example - "bCreateSomeStuff" becomes "Create Some Stuff?" and "DrawScale3D" becomes "Draw Scale 3D".
 * 
 * @param	InOutPropertyDisplayName	[In, Out] The property name to sanitize
 * @param	bIsBoolProperty				True if the property is a bool property
 */
void SanitizePropertyDisplayName( FString& InOutPropertyDisplayName, const UBOOL bIsBoolProperty );

/**
 * @return The name of the directory where cooked ini files go
 */
FString GetConfigOutputDirectory(UE3::EPlatformType Platform);

/**
 * @return The prefix to pass to appCreateIniNamesAndThenCheckForOutdatedness for non-platform specific inis
 */
FString GetConfigOutputPrefix(UE3::EPlatformType Platform);

/**
 * @return The prefix to pass to appCreateIniNamesAndThenCheckForOutdatedness for platform specific inis
 */
FString GetPlatformConfigOutputPrefix(UE3::EPlatformType Platform);

/**
 * @return The default ini prefix to pass to appCreateIniNamesAndThenCheckForOutdatedness for 
 */
FString GetPlatformDefaultIniPrefix(UE3::EPlatformType Platform);

/*
 * Make sure all the cooked platform inis are up to date for a given platform
 *
 * @param Platform - the platform being cooked
 * @param PlatformEngineConfigFilename - [out] the generated "engine.ini" file that results for the platform
 * @param PlatformSystemSettingsConfigName - [out] the generated "systemsettings.ini" file that results for the platform
 */
void UpdateCookedPlatformIniFilesFromDefaults(UE3::EPlatformType Platform, TCHAR PlatformEngineConfigFilename[], TCHAR PlatformSystemSettingsConfigName[]);


/**
 * Creates the entry point for a simple profiling node
 *
 * @param TimerName - The name of the section
 * @return The TimerIndex this node was inserted at
 */
INT ProfNodeStart(const TCHAR * TimerName);

/**
 * Stops the current scope and logs out the timing information
 * @param AssumedTimerIndex - The TimerIndex that was returned from ProfNodeStart.
 *                          - This can be used to help track down mismatched nodes.
 */
void ProfNodeStop(INT AssumedTimerIndex = -1);


/**
 * Inserts an event into the ProfNode output
 *
 * @param TimerName - The name of the event
 */
void ProfNodeEvent(const TCHAR * TimerName);

/**
 * @param Threshold - The threshold in seconds below which timing information will not be printed out. Default is 0.1f
 * @return The old time treshold
 */
FLOAT ProfNodeSetTimeThresholdSeconds(FLOAT Threshold);

/**
 * This sets the current thread to be the master thread for the ProfNode.log file
 */
void ProfNodeSetCurrentThreadAsMasterThread();


/**
 * @param Depth The depth level above shich timings will always be printed out.  Default is 2.
 * @return The old depth threshold
 */
INT ProfNodeSetDepthThreshold(INT Depth);

class ProfNodeScoped
{
public:
	FORCEINLINE explicit ProfNodeScoped(const TCHAR * TimerName)
	{
		NodeIndex = ProfNodeStart(TimerName);
	}

	FORCEINLINE ~ProfNodeScoped()
	{
		ProfNodeStop(NodeIndex);
	}

private:
	INT NodeIndex;
};


#if PROFNODE_ENABLED
#define PROFNODE_GENNAME(Name, Line) Name##Line
#define PROFNODE_VARNAME(Name, Line) PROFNODE_GENNAME(Name, Line)

#define PROFNODE_SCOPED(Name) ProfNodeScoped  PROFNODE_VARNAME(ProfNode, __LINE__) (Name)
#define PROFNODE_START(Name)  ProfNodeStart(Name)
#define PROFNODE_STOP()       ProfNodeStop()
#define PROFNODE_STOP_CHECKED(Index) ProfNodeStop(Index)
#define PROFNODE_EVENT(Name)  ProfNodeEvent(Name)
#else
#define PROFNODE_SCOPED(Name) 
#define PROFNODE_START(Name) 
#define PROFNODE_STOP() 
#define PROFNODE_STOP_CHECKED(Index)
#define PROFNODE_EVENT(Name)
#endif

