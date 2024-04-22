/*=============================================================================
 	FFileManagerAndroid.cpp: Unreal Android based file manager.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "CorePrivate.h"
#include "FFileManagerAndroid.h"
#include "UnIpDrv.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>

#include <android/asset_manager.h>

// globals

FString FFileManagerAndroid::AppDir;
FString FFileManagerAndroid::DocDir;
TMap<FName, AndroidTOCEntry> FFileManagerAndroid::MainTOCMap;
TMap<FName, AndroidTOCEntry> FFileManagerAndroid::PatchTOCMap;
TArray<AndroidTOCLookup> FFileManagerAndroid::PathLookup;
FString FFileManagerAndroid::MainPath;
FString FFileManagerAndroid::PatchPath;
FSocket* FFileManagerAndroid::FileServer;

// JNI
extern FString CallJava_GetMainAPKExpansionName();
extern FString CallJava_GetPatchAPKExpansionName();
extern UBOOL CallJava_IsExpansionInAPK();
extern AAssetManager* CallJava_GetAssetManager();

/**
 * If we started with ..\.., skip over a ..\, since the TOC won't be expecting that 
 * 4 ifs so we are separator independent (/ or \)
 *
 * @param Filename [in] Filename to skip over the ..\ if ..\.. was passed in
 */ 
INT FixupExtraDotsAmount(const TCHAR* Filename)
{
	if (Filename[0] == '.' && Filename[1] == '.' && Filename[3] == '.' && Filename[4] == '.')
	{ 
		// skip 3 characters
		return 3;
	}

	return 0;
}

/*-----------------------------------------------------------------------------
 FArchiveFileReaderAndroid implementation
 -----------------------------------------------------------------------------*/

FArchiveFileReaderAndroid::FArchiveFileReaderAndroid( int InHandle, const TCHAR* InFilename, FOutputDevice* InError, INT InSize, SQWORD InInternalFileOffset )
:   Handle          ( InHandle )
,	Filename		( InFilename )
,   Error           ( InError )
,   Size            ( InSize )
,   Pos             ( 0 )
,   BufferBase      ( 0 )
,   BufferCount     ( 0 )
,	InternalFileOffset (InInternalFileOffset)
{
	ArIsLoading = ArIsPersistent = 1;
	StatsHandle = FILE_IO_STATS_GET_HANDLE( InFilename );
}

FArchiveFileReaderAndroid::~FArchiveFileReaderAndroid()
{
	FILE_IO_STATS_CLOSE_HANDLE( StatsHandle );
	Close();
}

UBOOL FArchiveFileReaderAndroid::InternalPrecache( INT PrecacheOffset, INT PrecacheSize )
{
	// Only precache at current position and avoid work if precaching same offset twice.
	if( Pos == PrecacheOffset && (!BufferBase || !BufferCount || BufferBase != Pos) )
	{
		BufferBase = Pos;
		BufferCount = Min( Min( PrecacheSize, (INT)(ARRAY_COUNT(Buffer) - (Pos&(ARRAY_COUNT(Buffer)-1))) ), Size-Pos );
		ssize_t Count = 0;
		
		// Read data from device via POSIX read() API.
		{
			SCOPED_FILE_IO_READ_STATS( StatsHandle, BufferCount, Pos );
			Count = ::read( Handle, Buffer, BufferCount );
		}
		
		if( Count!=((ssize_t)BufferCount) )
		{
			ArIsError = 1;
			char buf[1024];
			const char *msg = appGetSystemErrorMessage( buf, ARRAY_COUNT( buf ) );
			Error->Logf( TEXT("ReadFile failed: Count=%i BufferCount=%i Error=%s"), (INT) Count, BufferCount, msg );
		}
	}
	return TRUE;
}

void FArchiveFileReaderAndroid::Seek( INT InPos )
{
	check(InPos>=0);
	check(InPos<=Size);

	if (InPos == Pos || (InPos >= BufferBase && InPos < BufferBase + BufferCount))
	{
		Pos = InPos;
		return;
	}

	if( ::lseek( Handle, InPos + InternalFileOffset, SEEK_SET )==-1 )
	{
		ArIsError = 1;
		char buf[1024];
		const char *msg = appGetSystemErrorMessage( buf, ARRAY_COUNT( buf ) );
		Error->Logf( TEXT("SetFilePointer Failed %i/%i: %i %s"), InPos, Size, Pos, msg );
	}
	Pos         = InPos;
	BufferBase  = Pos;
	BufferCount = 0;
}

INT FArchiveFileReaderAndroid::Tell()
{
	return Pos;
}

INT FArchiveFileReaderAndroid::TotalSize()
{
	return Size;
}

UBOOL FArchiveFileReaderAndroid::Close()
{
	if( Handle != -1)
	{
		close( Handle );
	}
	Handle = -1L;
	return !ArIsError;
}

void FArchiveFileReaderAndroid::Serialize( void* V, INT Length )
{
	while( Length>0 )
	{
		INT Copy = Min( Length, BufferBase+BufferCount-Pos );
		if( Copy==0 )
		{
			if( Length >= ARRAY_COUNT(Buffer) )
			{
				ssize_t Count = 0;
				// Read data from device via POSIX read() API.
				{
					SCOPED_FILE_IO_READ_STATS( StatsHandle, Length, Pos );
					Count = ::read( Handle, V, Length );
				}
				if( Count!=((ssize_t) Length) )
				{
					ArIsError = 1;
					char buf[1024];
					const char *msg = appGetSystemErrorMessage( buf, ARRAY_COUNT( buf ) );
					Error->Logf( TEXT("ReadFile failed: Count=%i Length=%i Error=%s"), (INT) Count, Length, msg );
				}
				Pos += Length;
				BufferBase += Length;
				return;
			}
			InternalPrecache( Pos, MAXINT );
			Copy = Min( Length, BufferBase+BufferCount-Pos );
			if( Copy<=0 )
			{
				ArIsError = 1;
				Error->Logf( TEXT("ReadFile beyond EOF %i+%i/%i"), Pos, Length, Size );
			}
			if( ArIsError )
			{
				return;
			}
		}
		appMemcpy( V, Buffer+Pos-BufferBase, Copy );
		Pos       += Copy;
		Length    -= Copy;
		V          = (BYTE*)V + Copy;
	}
}



/*-----------------------------------------------------------------------------
 FArchiveFileWriterAndroid implementation
 -----------------------------------------------------------------------------*/

FArchiveFileWriterAndroid::FArchiveFileWriterAndroid( int InHandle, const TCHAR* InFilename, FOutputDevice* InError, INT InPos )
:   Handle      ( InHandle )
,	Filename	( InFilename )
,   Error       ( InError )
,   Pos         ( InPos )
,   BufferCount ( 0 )
{
	ArIsSaving = ArIsPersistent = 1;
	StatsHandle = FILE_IO_STATS_GET_HANDLE( InFilename );
}

FArchiveFileWriterAndroid::~FArchiveFileWriterAndroid()
{
	FILE_IO_STATS_CLOSE_HANDLE( StatsHandle );
	Close();
}

void FArchiveFileWriterAndroid::Seek( INT InPos )
{
	Flush();
	if( ::lseek( Handle, InPos, SEEK_SET ) == -1 )
	{
		ArIsError = 1;
		Error->Logf( *LocalizeError("SeekFailed",TEXT("Core")) );
	}
	Pos = InPos;
}

INT FArchiveFileWriterAndroid::Tell()
{
	return Pos;
}

UBOOL FArchiveFileWriterAndroid::Close()
{
	Flush();
	if( (Handle != -1) && (close(Handle) == -1) )
	{
		ArIsError = 1;
		Error->Logf( *LocalizeError("WriteFailed",TEXT("Core")) );
	}
	Handle = -1;
	return !ArIsError;
}

void FArchiveFileWriterAndroid::Serialize( void* V, INT Length )
{
	Pos += Length;
	INT Copy;
	while( Length > (Copy=ARRAY_COUNT(Buffer)-BufferCount) )
	{
		appMemcpy( Buffer+BufferCount, V, Copy );
		BufferCount += Copy;
		Length      -= Copy;
		V            = (BYTE*)V + Copy;
		Flush();
	}
	if( Length )
	{
		appMemcpy( Buffer+BufferCount, V, Length );
		BufferCount += Length;
	}
}

void FArchiveFileWriterAndroid::Flush()
{
	if( BufferCount )
	{
		SCOPED_FILE_IO_WRITE_STATS( StatsHandle, BufferCount, Pos );
		
		const ssize_t Result = ::write( Handle, Buffer, BufferCount );
		if(Result != BufferCount)
		{
			ArIsError = 1;
			Error->Logf( *LocalizeError("WriteFailed",TEXT("Core")) );
		}
	}
	BufferCount = 0;
}


/*-----------------------------------------------------------------------------
 FFileManagerAndroid implementation
 -----------------------------------------------------------------------------*/

/**
 * Perform early file manager initialization, particularly finding the application and document directories
 */
void FFileManagerAndroid::StaticInit()
{
	// set the game name as early as possible, since we need it (engine will call it again, but that's okay)
	extern void appSetGameName();
	appSetGameName();

	extern FString GAndroidRootPath;
	
	DocDir = GAndroidRootPath;
	AppDir = GAndroidRootPath;
}



/**
 * Converts passed in filename to use an absolute path.
 *
 * @param	Filename	filename to convert to use an absolute path, safe to pass in already using absolute path
 * 
 * @return	filename using absolute path
 */
FString FFileManagerAndroid::ConvertToAbsolutePath( const TCHAR* Filename )
{
	FString AbsolutePath = ConvertToAndroidPath(Filename);
	
	// if it is already an absolute path, just return it
	if (AbsolutePath.StartsWith(TEXT("/"))) 
	{
		return AbsolutePath;
	}
	
	// remove any initial ../
	if (AbsolutePath.StartsWith(TEXT("../")))
	{
		AbsolutePath = AbsolutePath.Mid(3);
	}
	
	// remove potential second ../
	if (AbsolutePath.StartsWith(TEXT("../")))
	{
		AbsolutePath = AbsolutePath.Mid(3);
	}

	AbsolutePath = AppDir + AbsolutePath;	
	return AbsolutePath;
}

/**
 * Converts a path pointing into the Android read location, and converts it to the parallel
 * write location. This allows us to read files that the Android has written out
 *
 * @param AbsolutePath Source path to convert
 *
 * @return Path to the write directory
 */
FString FFileManagerAndroid::ConvertAbsolutePathToUserPath(const TCHAR* AbsolutePath)
{
	FString UserPath( AbsolutePath );
	UserPath = UserPath.Replace( *AppDir, *DocDir, false );
	return UserPath;
}

/*-----------------------------------------------------------------------------
 Public interface
 -----------------------------------------------------------------------------*/

void FFileManagerAndroid::Init(UBOOL Startup)
{
	bIsExpansionInAPK = CallJava_IsExpansionInAPK();

	// a final release game will always run as if installed
#if FINAL_RELEASE
	// shipping PC game 
	bIsRunningInstalled = TRUE;
#else
	// for development, use a commandline param (-installed)
	bIsRunningInstalled = ParseParam(appCmdLine(),TEXT("installed")) || bIsExpansionInAPK;
#endif

	if (bIsRunningInstalled)
	{
		if (bIsExpansionInAPK)
		{
			AAssetManager* AssetMgr = CallJava_GetAssetManager();
			AAsset* PatchAsset = AAssetManager_open(AssetMgr, TCHAR_TO_ANSI(*CallJava_GetPatchAPKExpansionName()), AASSET_MODE_RANDOM);
			AAsset* MainAsset = AAssetManager_open(AssetMgr, TCHAR_TO_ANSI(*CallJava_GetMainAPKExpansionName()), AASSET_MODE_RANDOM);

			// Generate PatchTOC if Patch Expansion file opened
			if (PatchAsset != NULL)
			{
				InternalGenerateTOC(PatchAsset, PatchTOCMap);
				PatchPath = *CallJava_GetPatchAPKExpansionName();
				AAsset_close(PatchAsset);
			}

			// Generate MainTOC if Main Expansion file opened
			if (MainAsset != NULL)
			{
				InternalGenerateTOC(MainAsset, MainTOCMap);
				MainPath = *CallJava_GetMainAPKExpansionName();
				AAsset_close(MainAsset);
			}
		}
		else
		{
			const INT PatchHandle = ::open(TCHAR_TO_UTF8(*CallJava_GetPatchAPKExpansionName()), O_RDONLY);
			const INT MainHandle = ::open(TCHAR_TO_UTF8(*CallJava_GetMainAPKExpansionName()), O_RDONLY);

			// Generate PatchTOC if Patch Expansion file opened
			if (PatchHandle >= 0)
			{
				InternalGenerateTOC(PatchHandle, PatchTOCMap);
				PatchPath = *CallJava_GetPatchAPKExpansionName();
				close(PatchHandle);
			}

			// Generate MainTOC if Main Expansion file opened
			if (MainHandle >= 0)
			{
				InternalGenerateTOC(MainHandle, MainTOCMap);
				MainPath = *CallJava_GetMainAPKExpansionName();
				close(MainHandle);
			}
		}
	}

	// read in the TOC file
	TArray<BYTE> Buffer;
	const TCHAR* TOCName;
	TOCName = TEXT("AndroidTOC.txt");

	// merge in a per-language TOC file if it exists
	FString Lang = appGetLanguageExt();
	if (Lang != TEXT("int"))
	{
		FString LocTOCName = FString::Printf(TEXT("AndroidTOC_%s.txt"), *Lang);
		ReadTOC( TOC, *LocTOCName, FALSE );
	}

	// read in the main TOC - this needs to be after the loc toc reading
	ReadTOC( TOC, TOCName, TRUE );
}

UBOOL FFileManagerAndroid::SetDefaultDirectory()
{
	return chdir(TCHAR_TO_UTF8( *AppDir )) != -1;
}

UBOOL FFileManagerAndroid::SetCurDirectory(const TCHAR* Directory)
{
	return chdir(TCHAR_TO_UTF8( Directory )) != -1;
}

// !!! FIXME: all of this should happen in the base class, since it's all the same on Windows...
// !!! FIXME:  (maybe there should be a FFileManagerPC class above us, so consoles don't get this?)

FString FFileManagerAndroid::GetCurrentDirectory()
{
	return AppDir;
}

FArchive* FFileManagerAndroid::CreateFileReader( const TCHAR* InFilename, DWORD Flags, FOutputDevice* Error )
{
	// cache over the network if necessary
	VerifyFileIsLocal(InFilename);
	
	// first look in User Directory
	FArchive* ReturnValue = InternalCreateFileReader( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(InFilename)), Flags, Error );
	
	// if not found there, then look in the install directory
	if (ReturnValue == NULL)
	{
		ReturnValue = InternalCreateFileReader( *ConvertToAbsolutePath(InFilename), Flags, Error );
	}
	
	return ReturnValue;
}

FArchive* FFileManagerAndroid::CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT MaxFileSize )
{
	return InternalCreateFileWriter( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Filename)), Flags, Error );
}

INT FFileManagerAndroid::FileSize( const TCHAR* Filename )
{
	INT ReturnValue = InternalFileSize(Filename + FixupExtraDotsAmount(Filename));

	if (ReturnValue == -1)
	{
		// try user directory first
		ReturnValue = InternalFileSize( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Filename)) );
		if (ReturnValue == -1)
		{
			ReturnValue = InternalFileSize( *ConvertToAbsolutePath(Filename) );
		}
	}
	
	return ReturnValue;
}

INT FFileManagerAndroid::UncompressedFileSize( const TCHAR* Filename )
{
	return TOC.GetUncompressedFileSize(Filename + FixupExtraDotsAmount(Filename));
}

DWORD FFileManagerAndroid::Copy( const TCHAR* DestFile, const TCHAR* SrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, FCopyProgress* Progress )
{
	// we can only write to write directory, but source may be write or read (try write first)
	DWORD ReturnValue = InternalCopy( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(DestFile)), *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(SrcFile)), ReplaceExisting, EvenIfReadOnly, Attributes, Progress );
	if (ReturnValue != COPY_OK)
	{
		ReturnValue = InternalCopy( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(DestFile)), *ConvertToAbsolutePath(SrcFile), ReplaceExisting, EvenIfReadOnly, Attributes, Progress );
	}
	
	return ReturnValue;
}

UBOOL FFileManagerAndroid::Delete( const TCHAR* Filename, UBOOL RequireExists, UBOOL EvenReadOnly )
{
	// we can only delete from write directory
	return InternalDelete( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Filename)), RequireExists, EvenReadOnly );
}

UBOOL FFileManagerAndroid::IsReadOnly( const TCHAR* Filename )
{
	return InternalIsReadOnly( *ConvertToAbsolutePath(Filename) );
}

UBOOL FFileManagerAndroid::Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace, UBOOL EvenIfReadOnly, UBOOL Attributes )
{
	// we can only write to write directory, but source may be write or read (try write first)
	UBOOL ReturnValue = InternalMove( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Dest)), *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Src)), Replace, EvenIfReadOnly, Attributes );
	if (ReturnValue == FALSE)
	{
		ReturnValue = InternalMove( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Dest)), *ConvertToAbsolutePath(Src), Replace, EvenIfReadOnly, Attributes );
	}
	
	return ReturnValue;
}

UBOOL FFileManagerAndroid::MakeDirectory( const TCHAR* Path, UBOOL Tree )
{
	return InternalMakeDirectory( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Path)), Tree );
}

UBOOL FFileManagerAndroid::DeleteDirectory( const TCHAR* Path, UBOOL RequireExists, UBOOL Tree )
{
	// !!! FIXME: user path?
	return InternalDeleteDirectory( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Path)), RequireExists, Tree );
}

void FFileManagerAndroid::FindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories )
{
	// first perform the find in the User directory
	TArray<FString> UserDirResults;
	InternalFindFiles( Result, *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Filename)), Files, Directories );
	
	// now do the find in the install directory
	TArray<FString> InstallDirResults;
	InternalFindFiles( InstallDirResults, *ConvertToAbsolutePath(Filename), Files, Directories );
	
	// now add any new files to the results (so that user dir files override install dir files)
	for (INT InstallFileIndex = 0; InstallFileIndex < InstallDirResults.Num(); InstallFileIndex++)
	{
		Result.AddUniqueItem(*InstallDirResults(InstallFileIndex));
	}
}

DOUBLE FFileManagerAndroid::GetFileAgeSeconds( const TCHAR* Filename )
{
	// first look for the file in the user dir
	DOUBLE ReturnValue = InternalGetFileAgeSeconds( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Filename)) );
	// then look in install dir if it failed to be found
	if (ReturnValue == -1.0)
	{
		ReturnValue = InternalGetFileAgeSeconds( *ConvertToAbsolutePath(Filename) );
	}
	
	return ReturnValue;
}

DOUBLE FFileManagerAndroid::GetFileTimestamp( const TCHAR* Filename )
{
	// first look for the file in the user dir
	DOUBLE ReturnValue = InternalGetFileTimestamp( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Filename)) );
	// then look in install dir if it failed to be found
	if (ReturnValue == -1.0)
	{
		ReturnValue = InternalGetFileTimestamp( *ConvertToAbsolutePath(Filename) );
	}
	
	return ReturnValue;
}

UBOOL FFileManagerAndroid::GetTimestamp( const TCHAR* Filename, FTimeStamp& Timestamp )
{
	// first look for the file in the user dir
	UBOOL ReturnValue = InternalGetTimestamp( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Filename)), Timestamp );
	// then look in install dir if it failed to be found
	if (ReturnValue == FALSE)
	{
		ReturnValue = InternalGetTimestamp( *ConvertToAbsolutePath(Filename), Timestamp );
	}
	
	return ReturnValue;
}


/**
 * Updates the modification time of the file on disk to right now, just like the unix touch command
 * @param Filename Path to the file to touch
 * @return TRUE if successful
 */
UBOOL FFileManagerAndroid::TouchFile(const TCHAR* Filename)
{
	// first look for the file in the user dir
	UBOOL ReturnValue = InternalTouchFile( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Filename)) );
	// then look in install dir if it failed to be found
	if (ReturnValue == FALSE)
	{
		ReturnValue = (InternalGetFileTimestamp( *ConvertToAbsolutePath(Filename) ) == -1.0);
	}
	
	return ReturnValue;
}


/**
 *	Threadsafely converts the platform-independent Unreal filename into platform-specific full path.
 *
 *	@param Filename		Platform-independent Unreal filename
 *	@return				Platform-dependent full filepath
 **/
FString FFileManagerAndroid::GetPlatformFilepath( const TCHAR* Filename )
{
	return ConvertToAbsolutePath(Filename);
}



/*-----------------------------------------------------------------------------
 Internal interface
 -----------------------------------------------------------------------------*/

INT FFileManagerAndroid::GetAndroidFileSize(int Handle)
{
	struct stat StatBuf;
	if (::fstat(Handle, &StatBuf) == -1)
	{
		return -1;
	}
	if (S_ISREG(StatBuf.st_mode) == 0)  // not a regular file?
	{
		return -1;
	}
	if (StatBuf.st_size > 0x7FFFFFFF)  // bigger than largest positive value that fits in an INT?
	{
		return -1;
	}
	return (INT) StatBuf.st_size;
}

FArchive* FFileManagerAndroid::InternalCreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
{
	AndroidTOCEntry* TOCEntry = NULL;
	UBOOL bIsEntryInPatchFile = TRUE;

	// Only load up the entry if running installed, otherwise will fall back to loose files
	if (bIsRunningInstalled)
	{
		FName FilenameName = FName(Filename);

		// First check the patch file
		TOCEntry = PatchTOCMap.Find(FilenameName);

		// Then the main expansion file
		if (TOCEntry == NULL)
		{
			TOCEntry = MainTOCMap.Find(FilenameName);
			bIsEntryInPatchFile = FALSE;
		}
	}

	// If there is an entry use the data to set up a gzFile handle at the correct offset
	if (TOCEntry != NULL)
	{	
		if (bIsExpansionInAPK)
		{
			AAssetManager* AssetMgr = CallJava_GetAssetManager();
			AAsset* ExpansionFileAsset = AAssetManager_open(AssetMgr, TCHAR_TO_ANSI(*(bIsEntryInPatchFile ? PatchPath : MainPath)), AASSET_MODE_RANDOM);
			
			checkf(ExpansionFileAsset != NULL);

			off_t AssetFileSize, StartOffset;
			const int ExpansionFileHandle = AAsset_openFileDescriptor(ExpansionFileAsset, &StartOffset, &AssetFileSize );
			AAsset_close(ExpansionFileAsset);

			const INT FileSize = TOCEntry->Size;
			::lseek(ExpansionFileHandle, StartOffset + TOCEntry->Offset, SEEK_SET);

			// uncompressed in archive
			return new FArchiveFileReaderAndroid(ExpansionFileHandle,Filename,Error, FileSize, StartOffset + TOCEntry->Offset);
		}
		else
		{
			const int ExpansionFileHandle = bIsEntryInPatchFile ? ::open(TCHAR_TO_UTF8(*PatchPath), O_RDONLY) : ::open(TCHAR_TO_UTF8(*MainPath), O_RDONLY);
			::lseek(ExpansionFileHandle, TOCEntry->Offset, SEEK_SET);
			const INT FileSize = TOCEntry->Size;

			// uncompressed in archive
			return new FArchiveFileReaderAndroid(ExpansionFileHandle,Filename,Error,FileSize,TOCEntry->Offset);
		}
	}
	else // Loose Files (may occur in shipping builds with app generated files)
	{
		const int Handle = ::open(TCHAR_TO_UTF8(Filename), O_RDONLY);
		if( Handle == -1 )
		{
			if( Flags & FILEREAD_NoFail )
			{
				appErrorf( TEXT("Failed to read file: %s"), Filename );
			}
			return NULL;
		}

		const INT FileSize = GetAndroidFileSize(Handle);
		if( FileSize < 0)
		{
			::close(Handle);
			if( Flags & FILEREAD_NoFail )
			{
				appErrorf( TEXT("Failed to read file: %s"), Filename );
			}
			return NULL;
		}

		return new FArchiveFileReaderAndroid(Handle,Filename,Error,FileSize, 0);
	}	
}

FArchive* FFileManagerAndroid::InternalCreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
{
	MakeDirectory(*FFilename(Filename).GetPath(), TRUE);
	
	int UnixFlags = O_WRONLY | O_CREAT;  // open for writing, create if doesn't exist.
	if (Flags & FILEWRITE_NoReplaceExisting)
	{
		UnixFlags |= O_EXCL;
	}
	
	if (Flags & FILEWRITE_Append)
	{
		UnixFlags |= O_APPEND; // put file pointer at end of existing file.
	}
	else
	{
		UnixFlags |= O_TRUNC;  // truncate existing file to zero bytes.
	}
	
//	if (Flags & FILEWRITE_Unbuffered)
//	{
//		UnixFlags |= O_SYNC;
//	}
	
	int Handle = ::open(TCHAR_TO_UTF8(Filename), UnixFlags, 0600);
	if ( (Handle == -1) && (errno == EACCES) && (errno == (Flags & FILEWRITE_EvenIfReadOnly)) )
	{
		struct stat StatBuf;
		if (::stat(TCHAR_TO_UTF8(Filename), &StatBuf) != -1)
		{
			StatBuf.st_mode |= S_IWUSR;
			if (::chmod(TCHAR_TO_UTF8(Filename), StatBuf.st_mode) != -1)
			{
				Handle = ::open(TCHAR_TO_UTF8(Filename), UnixFlags, 0600);
			}
		}
	}
	
	INT Pos = 0;
	if ( (Handle != -1) && (Flags & FILEWRITE_Append) )
	{
		off_t SeekPos = ::lseek(Handle, 0, SEEK_END);
		if (SeekPos > 0x7FFFFFFF)  // Larger positive value than an INT can hold?
		{
			::close(Handle);
			Handle = -1;  // let this pass through the NoFail check coming up.
		}
		else
		{
			Pos = (INT) SeekPos;
		}
	}
	
	if (Handle == -1)
	{
		if( Flags & FILEWRITE_NoFail )
		{
			appErrorf( TEXT("Failed to create file: %s"), Filename );
		}
		return NULL;
	}
	
	return new FArchiveFileWriterAndroid(Handle,Filename,Error,Pos);
}

INT FFileManagerAndroid::InternalFileSize( const TCHAR* Filename )
{
	// GetFileSize() automatically takes care of TOC synchronization for us
	INT FileSize = TOC.GetFileSize(Filename);
	if (FileSize == -1)
	{
		struct stat StatBuf;
		if (::stat(TCHAR_TO_UTF8(Filename), &StatBuf) == -1)
		{
			return -1;
		}
		if (S_ISREG(StatBuf.st_mode) == 0)  // not a regular file?
		{
			return -1;
		}
		if (StatBuf.st_size > 0x7FFFFFFF)  // bigger than largest positive value that fits in an INT?
		{
			return -1;
		}
	
		FileSize = (INT) StatBuf.st_size;
	}

	// If installed check expansions as well
	if (bIsRunningInstalled && FileSize == -1)
	{
		AndroidTOCEntry* TOCEntry = NULL;

		// First check the patch file
		TOCEntry = PatchTOCMap.Find(FName(Filename));

		// Then the main expansion file
		if (TOCEntry == NULL)
		{
			TOCEntry = MainTOCMap.Find(FName(Filename));
		}

		if (TOCEntry != NULL)
		{
			FileSize = TOCEntry->Size;
		}
	}

	return FileSize;
}

DWORD FFileManagerAndroid::InternalCopy( const TCHAR* DestFile, const TCHAR* SrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, FCopyProgress* Progress )
{
	DWORD Result = FFileManagerGeneric::Copy( DestFile, SrcFile, ReplaceExisting, EvenIfReadOnly, Attributes, Progress );
	return Result;
}

UBOOL FFileManagerAndroid::InternalDelete( const TCHAR* Filename, UBOOL RequireExists, UBOOL EvenReadOnly )
{
	// You can delete files in Unix if you don't have write permission to them
	//  (the question is whether you have write access to its parent dir), but
	//  in the spirit of what the Windows codebase probably wants, we'll just
	//  ignore EvenReadOnly here.
	
	INT Result = (::unlink(TCHAR_TO_UTF8(Filename)) == -1);
	if( !Result )
	{
		if ( (!RequireExists) && ((errno == ENOENT) || (errno == ENOTDIR)) )
		{
			Result = 0;  // we'll call that success.
		}
		else
		{
			debugf( NAME_Error, TEXT("Error deleting file '%s' (errno: %d)"), Filename, errno );
		}
	}
	return Result!=-1;
}

UBOOL FFileManagerAndroid::InternalIsReadOnly( const TCHAR* Filename )
{
	if (::access(TCHAR_TO_UTF8(Filename), F_OK) == -1)
	{
		return FALSE;  // The Windows codepath returns 0 here too...
	}
	
	if (::access(TCHAR_TO_UTF8(Filename), W_OK) == -1)
	{
		if (errno != EACCES)
		{
			debugf( NAME_Error, TEXT("Error reading attributes for '%s'"), Filename );
			return FALSE;  // The Windows codepath returns 0 here too...
		}
		return TRUE;  // it's read only.
	}
	return FALSE;  // it exists and we can write to it.
}


UBOOL FFileManagerAndroid::InternalMove( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace, UBOOL EvenIfReadOnly, UBOOL Attributes )
{
	int Result = ::rename(TCHAR_TO_UTF8(Src), TCHAR_TO_UTF8(Dest));
	if( Result == -1)
	{
		// !!! FIXME: do a copy and delete the original if errno==EXDEV
		debugf( NAME_Error, TEXT("Error moving file '%s' to '%s' (errno: %d)"), Src, Dest, errno );
	}
	return Result!=0;
}

UBOOL FFileManagerAndroid::InternalMakeDirectory( const TCHAR* Path, UBOOL Tree )
{
	if( Tree )
	{
		return FFileManagerGeneric::MakeDirectory( Path, TRUE );
	}
	return (mkdir(TCHAR_TO_UTF8(Path), 0766) || (errno == EEXIST));
}

UBOOL FFileManagerAndroid::InternalDeleteDirectory( const TCHAR* Path, UBOOL RequireExists, UBOOL Tree )
{
	if( Tree )
		return FFileManagerGeneric::DeleteDirectory( Path, RequireExists, TRUE );
	return ( (rmdir(TCHAR_TO_UTF8(Path)) != -1) || ((errno==ENOENT) && (!RequireExists)) );
}

static inline UBOOL WildcardMatch(const TCHAR *Str, const TCHAR *Pattern)
{
	TCHAR SourceChar = *(Str++);
	while (TRUE)
	{
		const TCHAR PatternChar = *(Pattern++);
		if (PatternChar == TEXT('?'))
		{
			if ((SourceChar == TEXT('\0')) || (SourceChar == TEXT('/')))
			{
				return FALSE;
			}
			SourceChar = *(Str++);
		}
		
		else if (PatternChar == TEXT('*'))
		{
			const TCHAR NextPatternChar = *Pattern;
			if ((NextPatternChar != TEXT('?')) && (NextPatternChar != TEXT('*')))
			{
				while ((SourceChar != TEXT('\0')) && (SourceChar != NextPatternChar))
				{
					SourceChar = *(Str++);
				}
			}
		}
		
		else
		{
			if (PatternChar != SourceChar)
			{
				return FALSE;
			}
			else if (PatternChar == TEXT('\0'))
			{
				return TRUE;
			}
			SourceChar = *(Str++);
		}
	}
	
	return TRUE;   // shouldn't hit this.
}


void FFileManagerAndroid::InternalFindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories )
{
	// Separate path and filename.
	FFilename FileParts(Filename);
	FString FileNameStr = FileParts.GetCleanFilename();
	FString DirStr = FileParts.GetPath();
	
	// Check for empty path.
	if (DirStr.Len() == 0)
	{
		DirStr = TEXT(".");
	}
	
	// Open directory, get first entry.
	DIR *Dirp = ::opendir(TCHAR_TO_UTF8(*DirStr));
	if (Dirp != NULL)
	{
		const UBOOL bMatchStarDotStar = (FileNameStr == TEXT("*.*"));
		const UBOOL bMatchWildcard = ( (FileNameStr.InStr(TEXT("*")) != INDEX_NONE) ||
									  (FileNameStr.InStr(TEXT("?")) != INDEX_NONE) );
		const UBOOL bMatchAllTypes = (Files && Directories);
		
		// Check each entry.
		struct dirent *Direntp;
		while( (Direntp = ::readdir(Dirp)) != NULL )
		{
			const FString Entry(UTF8_TO_TCHAR(Direntp->d_name));
			UBOOL bIsMatch = FALSE;
			
			if ( (Entry == TEXT(".")) || (Entry == TEXT("..")) )
			{
				bIsMatch = FALSE;  // never return current/parent dir entries.
			}
			else if (bMatchStarDotStar)  // match everything, like Windows expects.
			{
				bIsMatch = TRUE;
			}
			else if (bMatchWildcard)  // can't avoid the pattern match?
			{
				bIsMatch = WildcardMatch(*Entry, *FileNameStr);
			}
			else  // fast path if there aren't any wildcard chars...
			{
				bIsMatch = (FileNameStr == Entry);
			}
			
			if ((bIsMatch) && (!bMatchAllTypes))  // can avoid the stat()?
			{
				bIsMatch = FALSE;  // reset this in case of failure...
				FString FullPath(DirStr);
				FullPath += TEXT("/");
				FullPath += Entry;
				struct stat StatBuf;
				if (::stat(TCHAR_TO_UTF8(*FullPath), &StatBuf) != -1)
				{
					if ( ((Directories) && (S_ISDIR(StatBuf.st_mode))) ||
						((Files) && (S_ISREG(StatBuf.st_mode))) )
					{
						bIsMatch = TRUE;
					}
				}
			}
			
			if (bIsMatch)
			{
				new(Result)FString(Entry);
			}		

		}

		::closedir( Dirp );
	}

	// If running installed also check expansion paths
	if (bIsRunningInstalled)
	{
		const UBOOL bMatchStarDotStar = (FileNameStr == TEXT("*.*"));
		const UBOOL bMatchWildcard = ( (FileNameStr.InStr(TEXT("*")) != INDEX_NONE) ||
			(FileNameStr.InStr(TEXT("?")) != INDEX_NONE) );
		const UBOOL bMatchAllTypes = (Files && Directories);

		for (INT LookupIndex = 0; LookupIndex < PathLookup.Num(); ++LookupIndex)
		{
			// Early out if directories dont match
			if (FName(*DirStr) != PathLookup(LookupIndex).DirectoryName)
			{
				continue;
			}

			UBOOL bIsMatch = FALSE;

			if ( (PathLookup(LookupIndex).FileName == TEXT(".")) || (PathLookup(LookupIndex).FileName == TEXT("..")) )
			{
				bIsMatch = FALSE;  // never return current/parent dir entries.
			}
			else if (bMatchStarDotStar)  // match everything, like Windows expects.
			{
				bIsMatch = TRUE;
			}
			else if (bMatchWildcard)  // can't avoid the pattern match?
			{
				bIsMatch = WildcardMatch(*PathLookup(LookupIndex).FileName, *FileNameStr);
			}
			else  // fast path if there aren't any wildcard chars...
			{
				bIsMatch = (FileNameStr == PathLookup(LookupIndex).FileName);
			}

			if (bIsMatch && !Result.ContainsItem(PathLookup(LookupIndex).FileName))
			{
				Result.AddItem(PathLookup(LookupIndex).FileName);
			}	
		}
	}
}

DOUBLE FFileManagerAndroid::InternalGetFileAgeSeconds( const TCHAR* Filename )
{
	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(Filename), &FileInfo) != -1)
	{
		time_t FileTime = FileInfo.st_mtime;
		time_t CurrentTime;
		time( &CurrentTime );
		return difftime( CurrentTime, FileTime );
	}
	return -1.0;
}

DOUBLE FFileManagerAndroid::InternalGetFileTimestamp( const TCHAR* Filename )
{
	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(Filename), &FileInfo) != -1)
	{
		return (DOUBLE) FileInfo.st_mtime;
	}
	return -1.0;
}

UBOOL FFileManagerAndroid::InternalGetTimestamp( const TCHAR* Filename, FTimeStamp& Timestamp )
{
	appMemzero( &Timestamp, sizeof(Timestamp) );
	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(Filename), &FileInfo) != -1)
	{
		time_t FileTime = FileInfo.st_mtime;
		struct tm Time;
		gmtime_r(&FileTime, &Time);
		Timestamp.Day       = Time.tm_mday;
		Timestamp.Month     = Time.tm_mon;
		Timestamp.DayOfWeek = Time.tm_wday;
		Timestamp.Hour      = Time.tm_hour;
		Timestamp.Minute    = Time.tm_min;
		Timestamp.Second    = Time.tm_sec;
		Timestamp.Year      = Time.tm_year + 1900;
		return TRUE;
	}
	return FALSE;
}

/**
 * Updates the modification time of the file on disk to right now, just like the unix touch command
 * @param Filename Path to the file to touch
 * @return TRUE if successful
 */
UBOOL FFileManagerAndroid::InternalTouchFile(const TCHAR* Filename)
{
	// Passing a NULL sets file access/mod time to current system time.
	return (utimes(TCHAR_TO_UTF8(Filename), NULL) == 0);
}

// Called during init for the app to try to load the expansion blob
void FFileManagerAndroid::InternalGenerateTOC(int Handle, TMap<FName, AndroidTOCEntry> &TOCMap)
{
	const INT FileSize = GetAndroidFileSize(Handle);

	// "UE3AndroidOBB"
	char MagicIdentifier[14]; 
	INT Count = ::read( Handle, MagicIdentifier, 13 );
	MagicIdentifier[13] = NULL; // NULL terminate

	// Test identifier
	if (appStricmp(ANSI_TO_TCHAR(MagicIdentifier), TEXT("UE3AndroidOBB")) != 0)
	{
		debugf(NAME_Error, TEXT("Expansion file has incorrect header!"));
		return;
	}

	// Get TOC count
	UINT TOCCount = 0;
	Count = ::read( Handle, &TOCCount, 4 );

	// Build TOC Map
	for (UINT EntryNumber = 0; EntryNumber < TOCCount; ++EntryNumber)
	{
		UINT NameLength = 0;
		Count = ::read( Handle, &NameLength, 4 );

		char* NameChars;
		NameChars = new char[NameLength];
		Count = ::read( Handle, NameChars, NameLength);

		// map with absolute path
		FName FileName(*ConvertToAbsolutePath(ANSI_TO_TCHAR(NameChars)));

		AndroidTOCEntry TOCEntry;
		Count = ::read( Handle, &TOCEntry.Offset, 8 );
		Count = ::read( Handle, &TOCEntry.Size, 4 );

		// Add data to TOC
		TOCMap.Set(FileName, TOCEntry);

		// Also add info for directory lookups
		FFilename FileParts(*ConvertToAbsolutePath(ANSI_TO_TCHAR(NameChars)));
		AndroidTOCLookup LookupEntry;
		LookupEntry.FileName = FileParts.GetCleanFilename();
		LookupEntry.DirectoryName = FName(*FileParts.GetPath());
		PathLookup.AddItem(LookupEntry);

		delete[] NameChars;
	}
}

// Called during init for the app to try to load the expansion blob
void FFileManagerAndroid::InternalGenerateTOC(AAsset* Asset, TMap<FName, AndroidTOCEntry> &TOCMap)
{
	off_t FileSize, StartOffset;
	INT Handle = AAsset_openFileDescriptor(Asset, &StartOffset, &FileSize);
	::lseek(Handle, StartOffset, SEEK_SET);

	// "UE3AndroidOBB"
	char MagicIdentifier[14]; 
	INT Count = ::read( Handle, MagicIdentifier, 13 );
	MagicIdentifier[13] = NULL; // NULL terminate

	// Test identifier
	if (appStricmp(ANSI_TO_TCHAR(MagicIdentifier), TEXT("UE3AndroidOBB")) != 0)
	{
		debugf(NAME_Error, TEXT("Expansion file has incorrect header!"));
		return;
	}

	// Get TOC count
	UINT TOCCount = 0;
	Count = ::read( Handle, &TOCCount, 4 );

	// Build TOC Map
	for (UINT EntryNumber = 0; EntryNumber < TOCCount; ++EntryNumber)
	{
		UINT NameLength = 0;
		Count = ::read( Handle, &NameLength, 4 );

		char* NameChars;
		NameChars = new char[NameLength];
		Count = ::read( Handle, NameChars, NameLength);

		// map with absolute path
		FName FileName(*ConvertToAbsolutePath(ANSI_TO_TCHAR(NameChars)));
			
		AndroidTOCEntry TOCEntry;
		Count = ::read( Handle, &TOCEntry.Offset, 8 );
		Count = ::read( Handle, &TOCEntry.Size, 4 );

		// Add data to TOC
		TOCMap.Set(FileName, TOCEntry);

		// Also add info for directory lookups
		FFilename FileParts(*ConvertToAbsolutePath(ANSI_TO_TCHAR(NameChars)));
		AndroidTOCLookup LookupEntry;
		LookupEntry.FileName = FileParts.GetCleanFilename();
		LookupEntry.DirectoryName = FName(*FileParts.GetPath());
		PathLookup.AddItem(LookupEntry);

		delete[] NameChars;
	}

	::close(Handle);
}

// See if file specified by (Path) exists with a different case. This
//  assumes that the directories specified exist with the given case, and
//  only looks at the file part of the path. The first file found via
//  readdir() is the one picked, which may not be the first in ASCII order.
//  If a file is found, (Path) is overwritten to reflect the new case, and
//  (true) is returned. If no file is found, (Path) is untouched, and (false)
//  is returned.
//
// Path must be in UTF-8 format! We will be writing to this string, so you
//  must own the buffer.
UBOOL FFileManagerAndroid::FindAlternateFileCase(char *Path)
{
	// do insensitive check only if the current path doesn't exist...
	if (::access(Path, F_OK) == 0)
	{
		return TRUE;
	}
	
	UBOOL retval = FALSE;
	char *ptr = ::strrchr(Path, TEXT('/'));
	char *filename = (ptr != NULL) ? ptr + 1 : Path;
	char *basedir = (ptr != NULL) ? Path : ((char *) ".");
	
	if (ptr != NULL)
		*ptr = '\0';  // separate dir and filename.
	
	// fast rejection: only check this if there's no wildcard in filename.
	if (::strchr(filename, '*') == NULL)
	{
		DIR *dir = ::opendir(basedir);
		if (dir != NULL)
		{
			struct dirent *ent;
			while (((ent = ::readdir(dir)) != NULL) && (!retval))
			{
				if (appStricmp(UTF8_TO_TCHAR(ent->d_name), UTF8_TO_TCHAR(filename)) == 0)  // a match?
				{
					::strcpy(filename, ent->d_name);  // overwrite with new case.
					retval = TRUE;
				}
			}
			::closedir(dir);
		}
	}
	
	if (ptr != NULL)
		*ptr = '/';  // recombine dir and filename into one path.
	
	return retval;
}


FString FFileManagerAndroid::ConvertToAndroidPath(const TCHAR *WinPath)
{
	FString RetVal(WinPath);
	
	// Convert dir separators.
	RetVal = RetVal.Replace(TEXT("\\"), TEXT("/"));
	
	char *Scratch = new char[(RetVal.Len() + 1) * 6];
	strcpy(Scratch, TCHAR_TO_UTF8(*RetVal));
	
	// fast rejection: only check this if the current path doesn't exist...
	if (::access(Scratch, F_OK) != 0)
	{
		char *Cur = Scratch;
		if (*Cur == '/')
		{
			Cur++;
		}
		
		UBOOL KeepLooking = TRUE;
		while (KeepLooking)
		{
			Cur = ::strchr(Cur, TEXT('/'));
			if (Cur != NULL)
			{
				*Cur = '\0';  // null-terminate so we have this chunk of path.
			}
			
			KeepLooking = FindAlternateFileCase(Scratch);
			
			if (Cur == NULL)
			{
				KeepLooking = FALSE;
			}
			else
			{
				*Cur = '/';   // reset string for next check.
				Cur++;
			}
		}
		RetVal = UTF8_TO_TCHAR(Scratch);
	}
	
	delete[] Scratch;
	return RetVal;
}

/**
 * Makes sure that the given file is update and local on this machine
 *
 * This is probably temporary and will be replaced with a SideBySide caching scheme
 *
 * @return TRUE if the file exists on the file server side
 */
UBOOL FFileManagerAndroid::VerifyFileIsLocal(const TCHAR* Filename)
{
	// if we load files over the network, or some other mechanism, then this is the \
	// place to do it.

	return TRUE;
}

/**
 * Tries to find handle to file either in Expansions or loose files (returns POSIX descriptor)
 */
int FFileManagerAndroid::GetFileHandle(const TCHAR* Filename, SQWORD &FileOffset, SQWORD &FileLength)
{
	AndroidTOCEntry* TOCEntry = NULL;
	UBOOL bIsEntryInPatchFile = TRUE;

	// Only load up the entry if running installed, otherwise will fall back to loose files
	if (bIsRunningInstalled)
	{
		// First check the patch file
		TOCEntry = PatchTOCMap.Find(FName(Filename));

		// Then the main expansion file
		if (TOCEntry == NULL)
		{
			TOCEntry = MainTOCMap.Find(FName(Filename));
			bIsEntryInPatchFile = FALSE;
		}
	}

	if (TOCEntry != NULL)
	{	
		int ExpansionFileHandle = -1;
		if (bIsExpansionInAPK)
		{
			
			AAssetManager* AssetMgr = CallJava_GetAssetManager();
			AAsset* ExpansionFileAsset = AAssetManager_open(AssetMgr, TCHAR_TO_ANSI(*(bIsEntryInPatchFile ? PatchPath : MainPath)), AASSET_MODE_RANDOM);

			off_t FileSize, StartOffset;
			ExpansionFileHandle = AAsset_openFileDescriptor(ExpansionFileAsset, &StartOffset, &FileSize );
			AAsset_close(ExpansionFileAsset);

			::lseek(ExpansionFileHandle, StartOffset + TOCEntry->Offset, SEEK_SET);

			FileOffset = StartOffset + TOCEntry->Offset;
			FileLength = TOCEntry->Size;
		}
		else
		{
			ExpansionFileHandle = bIsEntryInPatchFile ? ::open(TCHAR_TO_UTF8(*PatchPath), O_RDONLY) : ::open(TCHAR_TO_UTF8(*MainPath), O_RDONLY);

			FileOffset = TOCEntry->Offset;
			FileLength = TOCEntry->Size;
		}

		
		return ExpansionFileHandle;
	}
	else // Loose Files
	{
		const int Handle = ::open(TCHAR_TO_UTF8(Filename), O_RDONLY);

		if( Handle == -1 )
		{
			FileOffset = FileLength = 0;
			return -1;
		}

		const INT FileSize = GetAndroidFileSize(Handle);
		if( FileSize < 0)
		{
			::close(Handle);
			FileOffset = FileLength = 0;
			return -1;
		}

		FileLength = FileSize;
		FileOffset = 0;
		return Handle;
	}	
}