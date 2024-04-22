/*=============================================================================
	FFileManagerGeneric.h: Unreal generic file manager support code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

	This base class simplifies FFileManager implementations by providing
	simple, unoptimized implementations of functions whose implementations
	can be derived from other functions.
=============================================================================*/

#ifndef _INC_FILEMANAGERGENERIC
#define _INC_FILEMANAGERGENERIC

/*-----------------------------------------------------------------------------
	File I/O tracking.
-----------------------------------------------------------------------------*/

#if !PERF_TRACK_FILEIO_STATS

#define FILE_IO_STATS_GET_HANDLE(Filename) 0
#define FILE_IO_STATS_CLOSE_HANDLE(StatsHandle)
#define SCOPED_FILE_IO_READ_OPEN_STATS(StatsHandle)
#define SCOPED_FILE_IO_READ_STATS(StatsHandle,Size,Offset)
#define SCOPED_FILE_IO_ASYNC_READ_OPEN_STATS(StatsHandle)
#define SCOPED_FILE_IO_ASYNC_READ_STATS(StatsHandle,Size,Offset)
#define SCOPED_FILE_IO_WRITE_OPEN_STATS(StatsHandle)
#define SCOPED_FILE_IO_WRITE_STATS(StatsHandle,Size,Offset)

#else

#define FILE_IO_STATS_GET_HANDLE(Filename)	GetFileIOStats()->GetHandle(Filename)
#define FILE_IO_STATS_CLOSE_HANDLE(StatsHandle) GetFileIOStats()->MarkFileAsClosed(StatsHandle)
#define SCOPED_FILE_IO_READ_OPEN_STATS(StatsHandle) FScopedFileIORequestStats ScopedFileIORequestStats(StatsHandle,(QWORD)-1,(QWORD)-1,FIOT_ReadOpenRequest)
#define SCOPED_FILE_IO_READ_STATS(StatsHandle,Size,Offset) FScopedFileIORequestStats ScopedFileIORequestStats(StatsHandle,Size,Offset,FIOT_ReadRequest)
#define SCOPED_FILE_IO_ASYNC_READ_OPEN_STATS(StatsHandle) FScopedFileIORequestStats ScopedFileIORequestStats(StatsHandle,(QWORD)-1,(QWORD)-1,FIOT_AsyncReadOpenRequest)
#define SCOPED_FILE_IO_ASYNC_READ_STATS(StatsHandle,Size,Offset) FScopedFileIORequestStats ScopedFileIORequestStats(StatsHandle,Size,Offset,FIOT_AsyncReadRequest)
#define SCOPED_FILE_IO_WRITE_OPEN_STATS(StatsHandle) FScopedFileIORequestStats ScopedFileIORequestStats(StatsHandle,(QWORD)-1,(QWORD)-1,FIOT_WriteOpenRequest)
#define SCOPED_FILE_IO_WRITE_STATS(StatsHandle,Size,Offset) FScopedFileIORequestStats ScopedFileIORequestStats(StatsHandle,Size,Offset,FIOT_WriteRequest)

/** Enum for I/O request classification. */
enum EFileIOType
{
	FIOT_ReadOpenRequest,
	FIOT_ReadRequest,
	FIOT_AsyncReadOpenRequest,
	FIOT_AsyncReadRequest,
	FIOT_WriteOpenRequest,
	FIOT_WriteRequest,
};

/** 
 * File I/O stats collector object.
 */
struct FFileIOStats
{
	/**
	 * Returns a handle associated with the passed in file if already existing, otherwise
	 * it will create it first.
	 *
	 * @param	Filename	Filename to map to a handle
	 * @return	unique handle associated with filename
	 */
	INT GetHandle( const TCHAR* Filename );

	/**
	 * Marks the associated file as being closed. Used to track which files are open.
	 * 
	 * @param	StatsHandle	Stats handle to mark associated file as closed.
	 */
	void MarkFileAsClosed( INT StatsHandle );

	/**
	 * Adds I/O request to the stat collection.
	 *
	 * @param	StatsHandle	Handle this request is for
	 * @param	Size		Size of request
	 * @param	Offset		Offset of request
	 * @param	Duration	Time request took
	 * @param	RequestType	Determines type of request
	 */
	void AddRequest( INT StatsHandle, QWORD Size, QWORD Offset, DOUBLE Duration, EFileIOType RequestType );

	/** Dumps collected stats to log. */
	void DumpStats();

protected:
	/** Helper struct containing all gathered information for a file. */
	struct FFileIOSummary
	{
		/** Constructor, initializing all members. */
		FFileIOSummary( const TCHAR* InFilename )
		:	Filename			( InFilename )
		,	bIsOpen				( TRUE )
		,	ReadSize			( 0 )
		,	ReadCount			( 0 )
		,	ReadOpenTime		( 0 )
		,	ReadTime			( 0 )
		,	AsyncReadSize		( 0 )
		,	AsyncReadCount		( 0 )
		,	AsyncReadOpenTime	( 0 )
		,	AsyncReadTime		( 0 )
		,	WriteSize			( 0 )
		,	WriteCount			( 0 )
		,	WriteOpenTime		( 0 )
		,	WriteTime			( 0 )
		{}

		FFileIOSummary& operator+=( const FFileIOSummary& Other )
		{
			ReadSize			+= Other.ReadSize;
			ReadCount			+= Other.ReadCount;
			ReadOpenTime		+= Other.ReadOpenTime;
			ReadTime			+= Other.ReadTime;
			AsyncReadSize		+= Other.AsyncReadSize;
			AsyncReadCount		+= Other.AsyncReadCount;
			AsyncReadOpenTime	+= Other.AsyncReadOpenTime;
			AsyncReadTime		+= Other.AsyncReadTime;
			WriteSize			+= Other.WriteSize;
			WriteCount			+= Other.WriteCount;
			WriteOpenTime		+= Other.WriteOpenTime;
			WriteTime			+= Other.WriteTime;
			return *this;
		}

		/** Filename all this data is for. */
		FString	Filename;
		/** Whether file handle is currently open. */
		UBOOL	bIsOpen;
		/** Total amount of bytes read. */
		QWORD	ReadSize;
		/** Number of read requests. */
		QWORD	ReadCount;
		/** Total time spent opening file for read. */
		DOUBLE	ReadOpenTime;
		/** Total time spent reading. */
		DOUBLE	ReadTime;
		/** Total amount of bytes read async. */
		QWORD	AsyncReadSize;
		/** Number of async read requests. */
		QWORD	AsyncReadCount;
		/** Total time spent opening file for async reading. */
		DOUBLE	AsyncReadOpenTime;
		/** Total time spent reading async. */
		DOUBLE	AsyncReadTime;
		/** Total amount of bytes written. */
		QWORD	WriteSize;
		/** Number of write requests */
		QWORD	WriteCount;
		/** Total time spent opening file for writing. */
		DOUBLE	WriteOpenTime;
		/** Total amount of time spent writing. */
		DOUBLE	WriteTime;
	};

	/**
	 * Dumps a file summary to the log.
	 *
	 * @param Summary	Summary to dump
	 * @param FileSize	Current size of associated file
	 */
	void DumpSummary( const FFileIOSummary& Summary, QWORD FileSize );

	/** Critical section used to syncronize access to stats. */
	FCriticalSection CriticalSection;
	/** Map from handle to summary, used by AddRequest. */
	TMap<INT,FFileIOSummary> HandleToSummaryMap;
	/** Map from filename to handle used by GetHandle. */
	TMap<FString,INT> FilenameToHandleMap;
};

/** Returns global file I/O stats collector and creates it if necessary. */
extern FFileIOStats* GetFileIOStats();

/** Scoped file I/O request helper. */
struct FScopedFileIORequestStats
{
	/** Constructor, initializing all members and keeping track of start  time. */
	FScopedFileIORequestStats( INT InStatsHandle, QWORD InSize, QWORD InOffset, EFileIOType InRequestType )
	:	StatsHandle	( InStatsHandle )
	,	Size		( InSize )
	,	Offset		( InOffset )
	,	StartTime	( appSeconds() )
	,	RequestType	( InRequestType )
	{}

	/** Destructor, adding IO request to global I/O stats collector after measuring delta time. */
	~FScopedFileIORequestStats()
	{
		DOUBLE Duration = appSeconds() - StartTime;
		GetFileIOStats()->AddRequest( StatsHandle, Size, Offset, Duration, RequestType );
	}

private:
	/** Stats handle used. */
	INT StatsHandle;
	/** Size of request. */
	QWORD Size;
	/** Offset of request. */
	QWORD Offset;
	/** Start time of request. */
	DOUBLE StartTime;
	/** Classifies I/O request type. */
	EFileIOType RequestType;
};

#endif // PERF_TRACK_FILEIO_STATS

/*-----------------------------------------------------------------------------
	File Manager.
-----------------------------------------------------------------------------*/

class FFileManagerGeneric : public FFileManager
{
public:
	virtual void	Init(UBOOL Startup);
	virtual INT		FileSize( const TCHAR* Filename );
	virtual INT		UncompressedFileSize( const TCHAR* Filename );
	virtual DWORD	Copy( const TCHAR* InDestFile, const TCHAR* InSrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, FCopyProgress* Progress );
	virtual UBOOL	MakeDirectory( const TCHAR* Path, UBOOL Tree=0 );
	virtual UBOOL	DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 );
	virtual UBOOL	Move( const TCHAR* Dest, const TCHAR* Src, UBOOL ReplaceExisting=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 );
	virtual INT		FindAvailableFilename( const TCHAR* Base, const TCHAR* Extension, FString& OutFilename, INT StartVal=-1 );
	/** 
	 * Read the contents of a TOC file
	 */
	void ReadTOC( FTableOfContents& TOC, const TCHAR* ToCName, UBOOL bRequired );

protected:
	virtual UBOOL	IsDrive( const TCHAR* Path );
};



#endif
