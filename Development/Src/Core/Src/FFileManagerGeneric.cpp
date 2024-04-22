/*=============================================================================
	FFileManagerGeneric.cpp: Unreal generic file manager support code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

	This base class simplifies FFileManager implementations by providing
	simple, unoptimized implementations of functions whose implementations
	can be derived from other functions.

=============================================================================*/

// for compression
#include "CorePrivate.h"
#include "FTableOfContents.h"
#include "FFileManagerGeneric.h"

#define COPYBLOCKSIZE	32768

/*-----------------------------------------------------------------------------
	File I/O tracking.
-----------------------------------------------------------------------------*/

#if PERF_TRACK_FILEIO_STATS

/** Returns global file I/O stats collector and creates it if necessary. */
FFileIOStats* GetFileIOStats()
{
	static FFileIOStats* GFileIOStats = NULL;
	if( !GFileIOStats )
	{
		GFileIOStats = new FFileIOStats();
	}
	return GFileIOStats;
}

/**
 * Returns a handle associated with the passed in file if already existing, otherwise
 * it will create it first.
 *
 * @param	Filename	Filename to map to a handle
 * @return	unique handle associated with filename
 */
INT FFileIOStats::GetHandle( const TCHAR* InFilename )
{
	// Code is not re-entrant and can be called from multiple threads.
	FScopeLock ScopeLock(&CriticalSection);

	// Unique handle counter. 0 means invalid handle.
	static INT UniqueFileIOStatsHandle = 0;

	// Make sure multiple ways to accessing a file via relative paths leads to the same handle.
	FString FullyQualifiedFilename = appConvertRelativePathToFull( InFilename );

	// Check whether we already have a handle associated with this filename.
	INT Handle = FilenameToHandleMap.FindRef( FullyQualifiedFilename );
	// And create one if not.
	if( !Handle )
	{
		// Pre-increment as 0 means invalid.
		Handle = ++UniqueFileIOStatsHandle;
		// Associate handle with fresh summary.
		HandleToSummaryMap.Set( Handle, FFileIOSummary( *FullyQualifiedFilename ) );
	}
	// Make sure existing file handle is marked as open in summary.
	else
	{
		// Find summary associated with handle, guaranteed to exist at this point.
		FFileIOSummary* FileIOSummary = HandleToSummaryMap.Find( Handle );
		check( FileIOSummary );
		// Mark file as open as we're reading from it or writing to it.
		FileIOSummary->bIsOpen = TRUE;
	}

	return Handle;
}

/**
 * Marks the associated file as being closed. Used to track which files are open.
 * 
 * @param	StatsHandle	Stats handle to mark associated file as closed.
 */
void FFileIOStats::MarkFileAsClosed( INT StatsHandle )
{
	// Code is not re-entrant and can be called from multiple threads.
	FScopeLock ScopeLock(&CriticalSection);

	// Find summary associated with handle, guaranteed to exist at this point.
	FFileIOSummary* FileIOSummary = HandleToSummaryMap.Find( StatsHandle );
	check( FileIOSummary );

	// Mark summary to indicate file as being closed.
	FileIOSummary->bIsOpen = FALSE;
}


/**
 * Adds I/O request to the stat collection.
 *
 * @param	StatsHandle	Handle this request is for
 * @param	Size		Size of request
 * @param	Offset		Offset of request
 * @param	Duration	Time request took
 * @param	RequestType	Determines type of request
 */
void FFileIOStats::AddRequest( INT StatsHandle, QWORD Size, QWORD Offset, DOUBLE Duration, EFileIOType RequestType )
{
	// Code is not re-entrant and can be called from multiple threads.
	FScopeLock ScopeLock(&CriticalSection);

	// Look up summary associated with request.
	FFileIOSummary* Summary = HandleToSummaryMap.Find( StatsHandle );
	check(Summary);
	
	switch( RequestType )
	{
	case FIOT_ReadOpenRequest:
		// We're opening a file via file manager for read.
		Summary->ReadOpenTime += Duration;
		break;
	case FIOT_ReadRequest:
		// We're reading from file via file manager.
		Summary->ReadSize += Size;
		Summary->ReadTime += Duration;
		Summary->ReadCount++;
		break;
	case FIOT_AsyncReadOpenRequest:
		// We're opening a file via async IO for read.
		Summary->AsyncReadOpenTime += Duration;
		break;
	case FIOT_AsyncReadRequest:
		// We're reading from file via async IO.
		Summary->AsyncReadSize += Size;
		Summary->AsyncReadTime += Duration;
		Summary->AsyncReadCount++;
		break;
	case FIOT_WriteOpenRequest:
		// We're opening a file via file manager for writing.
		Summary->WriteOpenTime += Duration;
		break;
	case FIOT_WriteRequest:
		// We're writing to file via file manager.
		Summary->WriteSize += Size;
		Summary->WriteTime += Duration;
		Summary->WriteCount++;
		break;
	default:
		break;
	}
}

/**
 * Dumps a file summary to the log.
 *
 * @param Summary	Summary to dump
 * @param FileSize	Current size of associated file
 */
void FFileIOStats::DumpSummary( const FFileIOSummary& Summary, QWORD FileSize )
{
	// Log in CSV format.
	debugf(	TEXT(",%s,%s")
		TEXT(",%llu,%llu,%7.2f,%7.2f")
		TEXT(",%llu,%llu,%7.2f,%7.2f")
		TEXT(",%llu,%5.2f,%5.2f")
		TEXT(",%llu,%llu,%7.2f,%7.2f"),

		*Summary.Filename,
		Summary.bIsOpen ? TEXT("Open") : TEXT("Closed"),

		Summary.ReadSize,
		Summary.ReadCount,
		Summary.ReadOpenTime,
		Summary.ReadTime,

		Summary.AsyncReadSize,
		Summary.AsyncReadCount,
		Summary.AsyncReadOpenTime,
		Summary.AsyncReadTime,

		FileSize,
		FileSize ? 100.f * Summary.ReadSize / FileSize : 0,
		FileSize ? 100.f * Summary.AsyncReadSize / FileSize : 0,

		Summary.WriteSize,
		Summary.WriteCount,
		Summary.WriteOpenTime,
		Summary.WriteTime );
}

/**
 * Dumps collected stats to log in CSV format to make it easier to import into Excel for sorting.
 */
void FFileIOStats::DumpStats()
{	
	// Code is not re-entrant and can be called from multiple threads.
	FScopeLock ScopeLock(&CriticalSection);

	// Header row. Initial comma allows getting rid of "Log: " prefix.
	debugf(TEXT(",Filename,Status,Read Size,Read Count,Read Open Time,Read Time,Async Read Size,Async Read Count,Async Read Open Time,Async Read Time,File Size,Read Percentage,Async Read Percentage,Write Size,Write Count,Write Open Time,Write Time"));
	
	// Iterate over all files and gather totals.
	FFileIOSummary	Total( TEXT("Total") );
	QWORD			TotalFileSize = 0;
	for( TMap<INT,FFileIOSummary>::TConstIterator It(HandleToSummaryMap); It; ++It )
	{
		const FFileIOSummary& Summary = It.Value();
		Total			+= Summary;
#if !PS3 //@todo PS3; file size requires platform file name and will assert internally otherwise
		TotalFileSize	+= GFileManager->FileSize( *Summary.Filename );
#endif
	}
	// Dump totals.
	DumpSummary( Total, TotalFileSize );

	// Iterate over all files and emit a row per file with gathered data.
	for( TMap<INT,FFileIOSummary>::TConstIterator It(HandleToSummaryMap); It; ++It )
	{
		const FFileIOSummary& Summary = It.Value();
#if PS3 //@todo PS3; file size requires platform file name and will assert internally otherwise
		QWORD FileSize = 0;
#else
		// Use file size to determine read percentages.
		QWORD FileSize = GFileManager->FileSize( *Summary.Filename );
#endif
		// Dump entry for file.
		DumpSummary( Summary, FileSize );
	}
}

#endif // PERF_TRACK_FILEIO_STATS

/*-----------------------------------------------------------------------------
	File Manager.
-----------------------------------------------------------------------------*/

/**
 * Initialize the file manager
 */
void FFileManagerGeneric::Init(UBOOL Startup) 
{
#if !FINAL_RELEASE
	if( ParseParam(appCmdLine(),TEXT("CLEANSCREENSHOTS")) )
	{
		DeleteDirectory( *appScreenShotDir(), FALSE, TRUE );
	}

	if( ParseParam(appCmdLine(),TEXT("CLEANLOGS")) )
	{
		DeleteDirectory( *appGameLogDir(), FALSE, TRUE );
	}
#endif
	FFileManager::Init( Startup );
}

INT FFileManagerGeneric::FileSize( const TCHAR* Filename )
{
	// Create a generic file reader, get its size, and return it.
	FArchive* Ar = CreateFileReader( Filename );
	if( !Ar )
	{
		return -1;
	}
	INT Result = Ar->TotalSize();
	delete Ar;
	return Result;
}

INT FFileManagerGeneric::UncompressedFileSize( const TCHAR* Filename )
{
	// if the platform doesn't support knowing uncompressed file sizes, then indicate it here
	return -1;
}

DWORD FFileManagerGeneric::Copy( const TCHAR* InDestFile, const TCHAR* InSrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, FCopyProgress* Progress )
{
	// Direct file copier.
	if( Progress && !Progress->Poll( 0.0 ) )
	{
		return COPY_Canceled;
	}
	DWORD	Result		= COPY_OK;
	FString SrcFile		= InSrcFile;
	FString DestFile	= InDestFile;
	
	FArchive* Src = CreateFileReader( *SrcFile );
	if( !Src )
	{
		Result = COPY_ReadFail;
	}
	else
	{
		INT Size = Src->TotalSize();
		FArchive* Dest = CreateFileWriter( *DestFile, (ReplaceExisting?0:FILEWRITE_NoReplaceExisting) | (EvenIfReadOnly?FILEWRITE_EvenIfReadOnly:0), GNull, Size );
		if( !Dest )
		{
			Result = COPY_WriteFail;
		}
		else
		{
			INT Percent=0, NewPercent=0;
			BYTE* Buffer = new BYTE[COPYBLOCKSIZE];
			for( INT Total=0; Total<Size; Total+=sizeof(Buffer) )
			{
				INT Count = Min( Size-Total, (INT)sizeof(Buffer) );
				Src->Serialize( Buffer, Count );
				if( Src->IsError() )
				{
					Result = COPY_ReadFail;
					break;
				}
				Dest->Serialize( Buffer, Count );
				if( Dest->IsError() )
				{
					Result = COPY_WriteFail;
					break;
				}
				NewPercent = Total * 100 / Size;
				if( Progress && Percent != NewPercent && !Progress->Poll( (FLOAT)NewPercent / 100.f ) )
				{
					Result = COPY_Canceled;
					break;
				}
				Percent = NewPercent;
			}
			delete [] Buffer;
			if( Result == COPY_OK )
			{
				if( !Dest->Close() )
				{
					Result = COPY_WriteFail;
				}
			}
			delete Dest;
			if( Result != COPY_OK )
			{
				Delete( *DestFile );
			}
		}
		if( Result == COPY_OK )
		{
			if( !Src->Close() )
			{
				Result = COPY_ReadFail;
			}
		}
		delete Src;
	}
	if( Progress && Result==COPY_OK && !Progress->Poll( 1.0 ) )
	{
		Result = COPY_Canceled;
	}
	return Result;
}

UBOOL FFileManagerGeneric::MakeDirectory( const TCHAR* Path, UBOOL Tree )
{
	// Support code for making a directory tree.
	check(Tree);
	INT CreateCount=0;
	for( TCHAR Full[256]=TEXT(""), *Ptr=Full; ; *Ptr++=*Path++ )
	{
		if( appIsPathSeparator(*Path) || *Path==0 )
		{
			*Ptr = 0;
			if( Ptr != Full && !IsDrive(Full) )
			{
				if( !MakeDirectory( Full, 0 ) )
				{
					return 0;
				}
				CreateCount++;
			}
		}
		if( *Path==0 )
		{
			break;
		}
	}
	return CreateCount!=0;
}

UBOOL FFileManagerGeneric::DeleteDirectory( const TCHAR* Path, UBOOL RequireExists, UBOOL Tree )
{
	// Support code for removing a directory tree.
	check(Tree);
	if( !appStrlen(Path) )
	{
		return 0;
	}
	FString Spec = FString(Path) * TEXT("*");
	TArray<FString> List;
	FindFiles( List, *Spec, 1, 0 );
	for( INT i=0; i<List.Num(); i++ )
	{
		if( !Delete(*(FString(Path) * List(i)),1,1) )
		{
			return 0;
		}
	}
	// clear out the list of found files
	List.Empty();
	FindFiles( List, *Spec, 0, 1 );
	for( INT i=0; i<List.Num(); i++ )
	{
		if( !DeleteDirectory(*(FString(Path) * List(i)),1,1) )
		{
			return 0;
		}
	}
	return DeleteDirectory( Path, RequireExists, 0 );
}

UBOOL FFileManagerGeneric::Move( const TCHAR* Dest, const TCHAR* Src, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes )
{
	// Move file manually.
	if( Copy(Dest,Src,ReplaceExisting,EvenIfReadOnly,Attributes,NULL) != COPY_OK )
	{
		return 0;
	}
	Delete( Src, 1, 1 );
	return 1;
}

UBOOL FFileManagerGeneric::IsDrive( const TCHAR* Path )
{
	FString ConvertedPathString(Path);
	ConvertedPathString = ConvertedPathString.Replace(TEXT("/"), TEXT("\\"));
	const TCHAR* ConvertedPath= *ConvertedPathString;

	// Does Path refer to a drive letter or BNC path?
	if( appStricmp(ConvertedPath,TEXT(""))==0 )
	{
		return 1;
	}
	else if( appToUpper(ConvertedPath[0])!=appToLower(ConvertedPath[0]) && ConvertedPath[1]==':' && ConvertedPath[2]==0 )
	{
		return 1;
	}
	else if( appStricmp(ConvertedPath,TEXT("\\"))==0 )
	{
		return 1;
	}
	else if( appStricmp(ConvertedPath,TEXT("\\\\"))==0 )
	{
		return 1;
	}
	else if( ConvertedPath[0]=='\\' && ConvertedPath[1]=='\\' && !appStrchr(ConvertedPath+2,'\\') )
	{
		return 1;
	}
	else if( ConvertedPath[0]=='\\' && ConvertedPath[1]=='\\' && appStrchr(ConvertedPath+2,'\\') && !appStrchr(appStrchr(ConvertedPath+2,'\\')+1,'\\') )
	{
		return 1;
	}
	else
	{
		// Need to handle cases such as X:\A\B\..\..\C\..
		// This assumes there is no actual filename in the path (ie, not c:\DIR\File.ext)!
		FString TempPath(ConvertedPath);
		// Make sure there is a '\' at the end of the path
		if (TempPath.InStr(TEXT("\\"), TRUE) != (TempPath.Len() - 1))
		{
			TempPath += TEXT("\\");
		}

		FString CheckPath = TEXT("");
		INT ColonSlashIndex = TempPath.InStr(TEXT(":\\"));
		if (ColonSlashIndex != INDEX_NONE)
		{
			// Remove the 'X:\' from the start
			CheckPath = TempPath.Right(TempPath.Len() - ColonSlashIndex - 2);
		}
		else
		{
			// See if the first two characters are '\\' to handle \\Server\Foo\Bar cases
			if (TempPath.StartsWith(TEXT("\\\\")) == TRUE)
			{
				CheckPath = TempPath.Right(TempPath.Len() - 2);
				// Find the next slash
				INT SlashIndex = CheckPath.InStr(TEXT("\\"));
				if (SlashIndex != INDEX_NONE)
				{
					CheckPath = CheckPath.Right(CheckPath.Len() - SlashIndex  - 1);
				}
				else
				{
					CheckPath = TEXT("");
				}
			}
		}

		if (CheckPath.Len() > 0)
		{
			// Replace any remaining '\\' instances with '\'
			CheckPath.Replace(TEXT("\\\\"), TEXT("\\"));

			INT CheckCount = 0;
			INT SlashIndex = CheckPath.InStr(TEXT("\\"));
			while (SlashIndex != INDEX_NONE)
			{
				FString FolderName = CheckPath.Left(SlashIndex);
				if (FolderName == TEXT(".."))
				{
					// It's a relative path, so subtract one from the count
					CheckCount--;
				}
				else
				{
					// It's a real folder, so add one to the count
					CheckCount++;
				}
				CheckPath = CheckPath.Right(CheckPath.Len() - SlashIndex  - 1);
				SlashIndex = CheckPath.InStr(TEXT("\\"));
			}

			if (CheckCount <= 0)
			{
				// If there were the same number or greater relative to real folders, it's the root dir
				return 1;
			}
		}
	}

	// It's not a drive...
	return 0;
}

INT FFileManagerGeneric::FindAvailableFilename( const TCHAR* Base, const TCHAR* Extension, FString& OutFilename, INT StartVal )
{
	check( Base );
	check( Extension );

	FString FullPath( Base );
	const INT IndexMarker = FullPath.Len();			// Marks location of the four-digit index.
	FullPath += TEXT("0000.");
	FullPath += Extension;

	// Iterate over indices, searching for a file that doesn't exist.
	for ( DWORD i = StartVal+1 ; i < 10000 ; ++i )
	{
		FullPath[IndexMarker  ] = i / 1000     + '0';
		FullPath[IndexMarker+1] = (i/100) % 10 + '0';
		FullPath[IndexMarker+2] = (i/10)  % 10 + '0';
		FullPath[IndexMarker+3] =   i     % 10 + '0';

		if ( GFileManager->FileSize( *FullPath ) == -1 )
		{
			// The file doesn't exist; output success.
			OutFilename = FullPath;
			return static_cast<INT>( i );
		}
	}

	// Can't find an empty filename slot with index in (StartVal, 9999].
	return -1;
}

/** 
 * Read the contents of a TOC file
 */
void FFileManagerGeneric::ReadTOC( FTableOfContents& TOC, const TCHAR* ToCName, UBOOL bRequired )
{
	// read in the TOC file into a string buffer
	FString Result;
	if( appLoadFileToString( Result, *( appGameDir() + ToCName ) ) )
	{
		// the required TOC is the master TOC file
		TOC.ParseFromBuffer( Result, bRequired );
	}
	else
	{
		if( bRequired )
		{
			// TOC is required to exist now
			checkf( FALSE, TEXT( "Missing %s.txt. Make sure to use UnrealFrontend or CookerSync generate %s%s"), ToCName, *appGameDir(), ToCName );
			appHandleIOFailure( NULL );
		}
	}
}

