/*=============================================================================
	FFileManagerWindows.h: Unreal Windows OS based file manager.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#if _WINDOWS

#include "FFileManagerWindows.h"

#pragma pack (push,8)
#include <sys/stat.h>
#include <sys/utime.h>
#include <time.h>
#include <shlobj.h>
#pragma pack(pop)



/*-----------------------------------------------------------------------------
	FArchiveFileReaderWindows implementation
-----------------------------------------------------------------------------*/

FArchiveFileReaderWindows::FArchiveFileReaderWindows( HANDLE InHandle, INT InStatsHandle, const TCHAR* InFilename, FOutputDevice* InError, INT InSize )
:   Handle          ( InHandle )
,	StatsHandle		( InStatsHandle )
,	Filename		( InFilename )
,   Error           ( InError )
,   Size            ( InSize )
,   Pos             ( 0 )
,   BufferBase      ( 0 )
,   BufferCount     ( 0 )
{
	ArIsLoading = ArIsPersistent = 1;
}

FArchiveFileReaderWindows::~FArchiveFileReaderWindows()
{
	FILE_IO_STATS_CLOSE_HANDLE( StatsHandle );
	if( Handle )
	{
		Close();
	}
}

UBOOL FArchiveFileReaderWindows::InternalPrecache( INT PrecacheOffset, INT PrecacheSize )
{
	// Only precache at current position and avoid work if precaching same offset twice.
	if( Pos == PrecacheOffset && (!BufferBase || !BufferCount || BufferBase != Pos) )
	{
		BufferBase = Pos;
		BufferCount = Min( Min( PrecacheSize, (INT)(ARRAY_COUNT(Buffer) - (Pos&(ARRAY_COUNT(Buffer)-1))) ), Size-Pos );
		BufferCount = Max(BufferCount,0); // clamp to 0
		INT Count=0;

		// Read data from device via Win32 ReadFile API.
		{
			SCOPED_FILE_IO_READ_STATS( StatsHandle, BufferCount, Pos );
			ReadFile( Handle, Buffer, BufferCount, (DWORD*)&Count, NULL );
		}

		if( Count!=BufferCount )
		{
			TCHAR ErrorBuffer[1024];
			ArIsError = 1;
			Error->Logf( TEXT("ReadFile failed: Count=%i BufferCount=%i Error=%s"), Count, BufferCount, appGetSystemErrorMessage(ErrorBuffer,1024) );
		}
	}
	return TRUE;
}

void FArchiveFileReaderWindows::Seek( INT InPos )
{
	check(InPos>=0);
	check(InPos<=Size);
	if( SetFilePointer( Handle, InPos, NULL, FILE_BEGIN )==INVALID_SET_FILE_POINTER )
	{
		TCHAR ErrorBuffer[1024];
		ArIsError = 1;
		Error->Logf( TEXT("SetFilePointer Failed %i/%i: %i %s"), InPos, Size, Pos, appGetSystemErrorMessage(ErrorBuffer,1024) );
	}
	Pos         = InPos;
	BufferBase  = Pos;
	BufferCount = 0;
}

INT FArchiveFileReaderWindows::Tell()
{
	return Pos;
}

INT FArchiveFileReaderWindows::TotalSize()
{
	return Size;
}

UBOOL FArchiveFileReaderWindows::Close()
{
	if( Handle )
	{
		CloseHandle( Handle );
	}
	Handle = NULL;
	return !ArIsError;
}

void FArchiveFileReaderWindows::Serialize( void* V, INT Length )
{
	while( Length>0 )
	{
		INT Copy = Min( Length, BufferBase+BufferCount-Pos );
		if( Copy<=0 )
		{
			if( Length >= ARRAY_COUNT(Buffer) )
			{
				INT Count=0;
				// Read data from device via Win32 ReadFile API.
				{
					SCOPED_FILE_IO_READ_STATS( StatsHandle, Length, Pos );
					ReadFile( Handle, V, Length, (DWORD*)&Count, NULL );
				}
				if( Count!=Length )
				{
					TCHAR ErrorBuffer[1024];
					ArIsError = 1;
					Error->Logf( TEXT("ReadFile failed: Count=%i Length=%i Error=%s for file %s"), 
						Count, Length, appGetSystemErrorMessage(ErrorBuffer,1024), *Filename );
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
				Error->Logf( TEXT("ReadFile beyond EOF %i+%i/%i for file %s"), 
					Pos, Length, Size, *Filename );
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
	FArchiveFileWriterWindows implementation
-----------------------------------------------------------------------------*/

FArchiveFileWriterWindows::FArchiveFileWriterWindows( HANDLE InHandle, INT InStatsHandle, const TCHAR* InFilename, FOutputDevice* InError, INT InPos )
:   Handle      ( InHandle )
,	StatsHandle ( InStatsHandle )
,	Filename	( InFilename )
,   Error       ( InError )
,   Pos         ( InPos )
,   BufferCount ( 0 )
{
	ArIsSaving = ArIsPersistent = 1;
}

FArchiveFileWriterWindows::~FArchiveFileWriterWindows()
{
	if( Handle )
	{
		Close();
	}
	Handle = NULL;
}

void FArchiveFileWriterWindows::Seek( INT InPos )
{
	Flush();
	if( SetFilePointer( Handle, InPos, NULL, FILE_BEGIN )==0xFFFFFFFF )
	{
		ArIsError = 1;
		Error->Logf( *LocalizeError("SeekFailed",TEXT("Core")) );
	}
	Pos = InPos;
}
	
INT FArchiveFileWriterWindows::Tell()
{
	return Pos;
}

INT FArchiveFileWriterWindows::TotalSize()
{
	// Make sure that all data is written before looking at file size.
	Flush();

	// Determine size of file.
	LARGE_INTEGER FileSize;
	FileSize.QuadPart = -1;
	if( GetFileSizeEx( Handle, &FileSize ) == FALSE )
	{
		ArIsError = 1;
		Error->Logf( *LocalizeError("WriteFailed",TEXT("Core")) );
	}
	return FileSize.QuadPart;
}

UBOOL FArchiveFileWriterWindows::Close()
{
	Flush();
	if( Handle && !CloseHandle(Handle) )
	{
		ArIsError = 1;
		Error->Logf( *LocalizeError("WriteFailed",TEXT("Core")) );
	}
	Handle = NULL;
	return !ArIsError;
}

void FArchiveFileWriterWindows::Serialize( void* V, INT Length )
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

void FArchiveFileWriterWindows::Flush()
{
	if( BufferCount )
	{
		SCOPED_FILE_IO_WRITE_STATS( StatsHandle, BufferCount, Pos );

		INT Result=0;
		if( !WriteFile( Handle, Buffer, BufferCount, (DWORD*)&Result, NULL ) )
		{
			ArIsError = 1;
			Error->Logf( *LocalizeError("WriteFailed",TEXT("Core")) );
		}
	}
	BufferCount = 0;
}


/*-----------------------------------------------------------------------------
	FFileManagerWindows implementation
-----------------------------------------------------------------------------*/

/**
 * Converts passed in filename to use a relative path.
 *
 * @param	Filename	filename to convert to use a relative path
 * 
 * @return	filename using relative path
 */
FString FFileManagerWindows::ConvertToRelativePath( const TCHAR* Filename )
{
	//default to the full absolute path of this file
	FString RelativePath = Filename;

	// See whether it is a relative path.
	FString RootDir = appRootDir();
	//the default relative directory it to the app root which is 2 directories up from the starting directory
	//Drive:\Build\UnrealEngine3\ from Drive:\Build\UnrealEngine3\Binaries\Win32
	INT NumberOfDirectoriesToGoUp = 2;

	//temp holder for current position of the slash
	INT CurrentSlashPosition;

	//while we haven't run out of parent directories until we which a drive name
	const UBOOL bStartFromEnd = TRUE;
	const UBOOL bIgnoreCase = TRUE;
	while ( (CurrentSlashPosition = RootDir.InStr(TEXT("\\"), bStartFromEnd)) != INDEX_NONE)
	{
		if( RelativePath.StartsWith( RootDir ) )
		{
			// And if that's the case append the base dir.
			RelativePath = RelativePath.Right(RelativePath.Len() - RootDir.Len());
			for (INT ParentDirectoryLoopIndex = 0; ParentDirectoryLoopIndex < NumberOfDirectoriesToGoUp; ParentDirectoryLoopIndex++)
			{
				RelativePath = 	FString::Printf( TEXT("..") PATH_SEPARATOR TEXT("%s"), *RelativePath);
			}
			break;
		}
		INT PositionOfNextSlash = RootDir.InStr(TEXT("\\"), bStartFromEnd, bIgnoreCase, CurrentSlashPosition);
		//if there is another slash to find
		if (PositionOfNextSlash != INDEX_NONE)
		{
			//move up a directory and on an extra .. PATH_SEPARATOR
			// the +1 from "InStr" moves to include the "\" at the end of the directory name
			NumberOfDirectoriesToGoUp++;
			RootDir = RootDir.Left( PositionOfNextSlash + 1 );
		}
		else
		{
			RootDir.Empty();
		}
	}

	
	return RelativePath;
}

/**
 * Converts passed in filename to use an absolute path.
 *
 * @param	Filename	filename to convert to use an absolute path, safe to pass in already using absolute path
 * 
 * @return	filename using absolute path
 */
FString FFileManagerWindows::ConvertToAbsolutePath( const TCHAR* Filename )
{
	FString AbsolutePath = Filename;

	// See whether it is a relative path.
	if( AbsolutePath.StartsWith( TEXT("..") ) )
	{
		// And if that's the case append the base dir.
		AbsolutePath = FString(appBaseDir()) + AbsolutePath;
	}

	return AbsolutePath;
}

/**
 * Converts a path pointing into the installed directory (C:\Program Files\MyGame\ExampleGame\Config\ExampleEngine.ini)
 * to a path that a least-privileged user can write to (C:\<UserDir>\MyGame\ExampleGame\Config\ExampleEngine.ini)
 *
 * @param AbsolutePath Source path to convert
 *
 * @return Path to the user directory
 */
FString FFileManagerWindows::ConvertAbsolutePathToUserPath(const TCHAR* AbsolutePath)
{
	// if the user directory has been setup, use it
	if (WindowsUserDir.Len() > 0)
	{
		const UBOOL bUseUnpublished = GIsEditor
			|| (appStristr(appCmdLine(), TEXT("Editor")) != NULL)
			|| (appStristr(appCmdLine(), TEXT("CookPackages")) != NULL)
			|| (appStristr(appCmdLine(), TEXT("UseUnpublished")) != NULL);

		const TCHAR* ReplaceBit;
		const TCHAR* UnpublishedPath;
		const TCHAR* PublishedPath;

		// @todo ship: Use GSys->Paths or something for the replacement, instead of hardcoded Content
		FString UserContentPath(AbsolutePath);
		for (INT Path = 0; Path < 2; Path++)
		{
			if (Path == 0)
			{
				ReplaceBit = TEXT("\\CookedPC\\");
				UnpublishedPath = TEXT("\\Unpublished\\CookedPC\\");
				PublishedPath = TEXT("\\Published\\CookedPC\\");
			}
			else
			{
				ReplaceBit = TEXT("\\Content\\");
				UnpublishedPath = TEXT("\\Unpublished\\Content\\");
				PublishedPath = TEXT("\\Published\\Content\\");
			}

			// make sure we aren't using one of the special paths
			if (UserContentPath.InStr(UnpublishedPath, FALSE, TRUE) == INDEX_NONE && UserContentPath.InStr(PublishedPath, FALSE, TRUE) == INDEX_NONE)
			{
				UserContentPath = UserContentPath.Replace(ReplaceBit, bUseUnpublished ? UnpublishedPath : PublishedPath);
			}
		}

		// replace C:\Program Files\MyGame\Binaries with C:\<UserDir>\Binaries
		return UserContentPath.Replace(*WindowsRootDir, *WindowsUserDir, TRUE);
	}
	else
	{
		// in the non-installed (dev) case, just return the original path in all cases
		return AbsolutePath;
	}
}


/*-----------------------------------------------------------------------------
	Public interface
-----------------------------------------------------------------------------*/

void FFileManagerWindows::Init(UBOOL Startup)
{
	// a shipped PC game will always run as if installed
#if SHIPPING_PC_GAME && !UDK && !DEDICATED_SERVER
	// shipping PC game 
	bIsRunningInstalled = TRUE;
#else
	// for development, use a commandline param (-installed)
	bIsRunningInstalled = ParseParam(appCmdLine(),TEXT("installed"));
#endif

	// Allow overriding use of My Documents folder with -NOHOMEDIR
	if( ParseParam(appCmdLine(),TEXT("NOHOMEDIR") ) )
	{
		bIsRunningInstalled = FALSE;
	}

	if (bIsRunningInstalled)
	{
		debugf( TEXT( " ... running in INSTALLED mode" ) );

		TCHAR UserPath[MAX_PATH];
		// get the My Documents directory
		HRESULT Ret = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, UserPath);

		// get the per-game directory name to use inside the My Documents directory 
		FString DefaultIniContents;
		// load the DefaultEngine.ini config file into a string for later parsing (ConvertAbsolutePathToUserPath will use
		// original location since WindowsUserDir hasn't been set yet)
		// can't use GDefaultEngineIni, because that may be something that doesn't have the tag
		if (!appLoadFileToString(DefaultIniContents, *(appGameConfigDir() + TEXT("DefaultEngine.ini")), this))
		{
			// appMsgf won't write to a log if GWarn is NULL, which it should be at this point
			appMsgf(AMT_OK, TEXT("Failed to find default engine .ini file to retrieve My Documents subdirectory to use. Force quitting."));
			exit(1);
			return;
		}

		#define MYDOC_KEY_NAME TEXT("MyDocumentsSubDirName=")

		// find special key in the .ini file (can't use GConfig because it can't be used yet until after filemanager is made)
		INT KeyLocation = DefaultIniContents.InStr(MYDOC_KEY_NAME, FALSE, TRUE);
		if (KeyLocation == INDEX_NONE)
		{
			// appMsgf won't write to a log if GWarn is NULL, which it should be at this point
			appMsgf(AMT_OK, TEXT("Failed to find %s key in DefaultEngine.ini. Force quitting."), MYDOC_KEY_NAME);
			exit(1);
			return;
		}

		// skip over the key to get the value (skip key and = sign) and everything after it
		FString ValueAndLeftover = DefaultIniContents.Mid(KeyLocation + appStrlen(MYDOC_KEY_NAME));
		
		// now chop off this string at an end of line
		TArray<FString> Tokens;
		ValueAndLeftover.ParseIntoArray(&Tokens, TEXT("\r\n"), TRUE);

		// make the base user dir path
		WindowsUserDir = FString(UserPath) 
							+ TEXT("\\My Games\\") 
							+ Tokens(0) 
#if DEMOVERSION
							+ TEXT(" Demo")
#endif
							+ TEXT("\\");

		// find out our executable path
		WindowsRootDir = appBaseDir();
		// strip off the Binaries directory
		WindowsRootDir = WindowsRootDir.Left(WindowsRootDir.InStr(TEXT("\\Binaries\\"), TRUE, TRUE) + 1);

		// Now that the root directory has been set, create directories at startup.
		// Note this must come after the above because MakeDirectory calls
		// ConvertAbsolutePathToUserPath which uses WindowsRootDir and WindowsUserDir.
		#define DIRSTOCREATATSTARTUP_KEY_NAME TEXT("DirsToCreateAtStartup=")
		INT FindStartPos = INDEX_NONE;
		while ( TRUE )
		{
			// find special key in the .ini file (can't use GConfig because it can't be used yet until after filemanager is made)
			const INT KeyLocation = DefaultIniContents.InStr(DIRSTOCREATATSTARTUP_KEY_NAME, FALSE, TRUE, FindStartPos);
			if (KeyLocation == INDEX_NONE)
			{
				break;
			}
			// Advance the find pos because we're doing a multi find.
			FindStartPos = KeyLocation + appStrlen(DIRSTOCREATATSTARTUP_KEY_NAME);

			// skip over the key to get the value (skip key and = sign) and everything after it
			FString ValueAndLeftover = DefaultIniContents.Mid(KeyLocation + appStrlen(DIRSTOCREATATSTARTUP_KEY_NAME));
			
			// now chop off this string at an end of line
			TArray<FString> Tokens;
			ValueAndLeftover.ParseIntoArray(&Tokens, TEXT("\r\n"), TRUE);

			// Create the directory.
			MakeDirectory( *Tokens(0), TRUE );
		}
	}

	FFileManagerGeneric::Init(Startup);
}

UBOOL FFileManagerWindows::SetDefaultDirectory()
{
	return SetCurrentDirectoryW(appBaseDir())!=0;
}

UBOOL FFileManagerWindows::SetCurDirectory(const TCHAR* Directory)
{
	return SetCurrentDirectoryW(Directory)!=0;
}

FString FFileManagerWindows::GetCurrentDirectory()
{
	return appBaseDir();
}

FArchive* FFileManagerWindows::CreateFileReader( const TCHAR* InFilename, DWORD Flags, FOutputDevice* Error )
{
	// first look in User Directory
	FArchive* ReturnValue = InternalCreateFileReader( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(InFilename)), Flags, Error );

	// if not found there, then look in the install directory
	if (ReturnValue == NULL)
	{
		ReturnValue = InternalCreateFileReader( *ConvertToAbsolutePath(InFilename), Flags, Error );
	}

	return ReturnValue;
}

/**
* Dummy archive that doesn't actually write anything
* it just updates the file pos when seeking
*/
class FArchiveFileWriterDummy : public FArchive
{
public:
	FArchiveFileWriterDummy()
		:	Pos(0)
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	virtual ~FArchiveFileWriterDummy()
	{
		Close();
	}
	virtual void Seek( INT InPos )
	{
		Pos = InPos;
	}
	virtual INT Tell()
	{
		return Pos;
	}
	virtual void Serialize( void* V, INT Length )
	{
		Pos += Length;
	}
	virtual FString GetArchiveName() const { return TEXT("FArchiveFileWriterDummy"); }
protected:
	INT             Pos;
};

FArchive* FFileManagerWindows::CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT MaxFileSize )
{
	// Only allow writes to files that are not signed 
	// Except if the file is missing (that way corrupt ini files can be autogenerated by deleting them)
	if( FSHA1::GetFileSHAHash(Filename, NULL) &&
		FileSize(Filename) != -1 )
	{
		debugfSuppressed(NAME_DevSHA,TEXT("Can't write to signed game file: %s"),Filename);
		return new FArchiveFileWriterDummy();
	}

	return InternalCreateFileWriter( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Filename)), Flags, Error );
}

/**
 *	Returns the size of a file. (Thread-safe)
 *
 *	@param Filename		Platform-independent Unreal filename.
 *	@return				File size in bytes or -1 if the file didn't exist.
 **/
INT FFileManagerWindows::FileSize( const TCHAR* Filename )
{
	// try user directory first
	INT ReturnValue = InternalFileSize( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Filename)) );
	if (ReturnValue == -1)
	{
		ReturnValue = InternalFileSize( *ConvertToAbsolutePath(Filename) );
	}
	return ReturnValue;
}

INT FFileManagerWindows::UncompressedFileSize( const TCHAR* Filename )
{
	// default to not using the uncompressed file size
	INT Result = -1;	
	// check for the presence of a .uncompressed_size manifest file which mirrors the Filename
	// if it exists, then the file was fully compressed, so we get the original uncompressed size for it
	FString UnCompressedSizeFilename = FString(Filename) + FString(TEXT(".uncompressed_size"));
	if( FileSize(*UnCompressedSizeFilename) != -1 )
	{
		FString SizeString;
		appLoadFileToString(SizeString, *UnCompressedSizeFilename);
		check(SizeString.Len());
		// get the uncompressed size from the file
		Result = appAtoi(*SizeString);
	}
	return Result;
}

DWORD FFileManagerWindows::Copy( const TCHAR* DestFile, const TCHAR* SrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, FCopyProgress* Progress )
{
	// we can only write to user directory, but source may be user or install (try user first)
	DWORD ReturnValue = InternalCopy( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(DestFile)), *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(SrcFile)), ReplaceExisting, EvenIfReadOnly, Attributes, Progress );
	if (ReturnValue != COPY_OK)
	{
		ReturnValue = InternalCopy( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(DestFile)), *ConvertToAbsolutePath(SrcFile), ReplaceExisting, EvenIfReadOnly, Attributes, Progress );
	}

	return ReturnValue;
}

UBOOL FFileManagerWindows::Delete( const TCHAR* Filename, UBOOL RequireExists, UBOOL EvenReadOnly )
{
	// Only allow writes to files that are not signed 
	// Except if the file is missing (that way corrupt ini files can be autogenerated by deleting them)
	if( FSHA1::GetFileSHAHash(Filename, NULL) )
	{
		debugfSuppressed(NAME_DevSHA,TEXT("Can't delete signed game file: %s"),Filename);
		return FALSE;
	}
	
	// we can only delete from user directory
	return InternalDelete( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Filename)), RequireExists, EvenReadOnly );
}

UBOOL FFileManagerWindows::IsReadOnly( const TCHAR* Filename )
{
	return InternalIsReadOnly( *ConvertToAbsolutePath(Filename) );
}

UBOOL FFileManagerWindows::Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace, UBOOL EvenIfReadOnly, UBOOL Attributes )
{
	// we can only write to user directory, but source may be user or install (try user first)
	UBOOL ReturnValue = InternalMove( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Dest)), *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Src)), Replace, EvenIfReadOnly, Attributes );
	if (ReturnValue == FALSE)
	{
		ReturnValue = InternalMove( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Dest)), *ConvertToAbsolutePath(Src), Replace, EvenIfReadOnly, Attributes );
	}

	return ReturnValue;
}

UBOOL FFileManagerWindows::MakeDirectory( const TCHAR* Path, UBOOL Tree )
{
	return InternalMakeDirectory( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Path)), Tree );
}

UBOOL FFileManagerWindows::DeleteDirectory( const TCHAR* Path, UBOOL RequireExists, UBOOL Tree )
{
	return InternalDeleteDirectory( *ConvertToAbsolutePath(Path), RequireExists, Tree );
}

void FFileManagerWindows::FindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories )
{
	// first perform the find in the User directory
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

DOUBLE FFileManagerWindows::GetFileAgeSeconds( const TCHAR* Filename )
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

DOUBLE FFileManagerWindows::GetFileTimestamp( const TCHAR* Filename )
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

UBOOL FFileManagerWindows::GetTimestamp( const TCHAR* Filename, FTimeStamp& Timestamp )
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
UBOOL FFileManagerWindows::TouchFile(const TCHAR* Filename)
{
	// first look for the file in the user dir
	UBOOL ReturnValue = InternalTouchFile( *ConvertAbsolutePathToUserPath(*ConvertToAbsolutePath(Filename)) );
	// then look in install dir if it failed to be found
	if (ReturnValue == FALSE)
	{
		ReturnValue = InternalTouchFile( *ConvertToAbsolutePath(Filename) );
	}

	return ReturnValue;
}

UBOOL FFileManagerWindows::GetDiskFreeSpace(const TCHAR* Drive, QWORD& FreeBytesToCaller, QWORD& TotalBytes, QWORD& FreeBytes )
{
	return InternalGetDiskFreeSpace( Drive, FreeBytesToCaller,TotalBytes, FreeBytes );
}

/*-----------------------------------------------------------------------------
	Internal interface
-----------------------------------------------------------------------------*/

FArchive* FFileManagerWindows::InternalCreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
{
	INT StatsHandle = FILE_IO_STATS_GET_HANDLE( Filename );
	SCOPED_FILE_IO_READ_OPEN_STATS( StatsHandle );

	DWORD  Access    = GENERIC_READ;
	DWORD  WinFlags  = FILE_SHARE_READ;
	DWORD  Create    = OPEN_EXISTING;
	HANDLE Handle    = CreateFileW( Filename, Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL );
	if( Handle==INVALID_HANDLE_VALUE )
	{
		if( Flags & FILEREAD_NoFail )
		{
			const DWORD LastError = GetLastError();
			appErrorf( TEXT("Failed to read file: %s, GetLastError %u"), Filename, LastError );
		}
		return NULL;
	}

	FArchive* retArch = new FArchiveFileReaderWindows(Handle,StatsHandle,Filename,Error,GetFileSize(Handle,NULL));
	if( retArch && (Flags & FILEREAD_SaveGame) )
	{
		retArch->SetIsSaveGame( TRUE );
	}
	return retArch;
}

FArchive* FFileManagerWindows::InternalCreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
{
	INT StatsHandle = FILE_IO_STATS_GET_HANDLE( Filename );
	SCOPED_FILE_IO_WRITE_OPEN_STATS( StatsHandle );

	MakeDirectory(*FFilename(Filename).GetPath(), TRUE);

	if( (FileSize (Filename) >= 0) && (Flags & FILEWRITE_EvenIfReadOnly) )
	{
		SetFileAttributesW(Filename, 0);
	}
	DWORD  Access    = GENERIC_WRITE;
	DWORD  WinFlags  = (Flags & FILEWRITE_AllowRead) ? FILE_SHARE_READ : 0;
	DWORD  Create    = (Flags & FILEWRITE_Append) ? OPEN_ALWAYS : (Flags & FILEWRITE_NoReplaceExisting) ? CREATE_NEW : CREATE_ALWAYS;
	HANDLE Handle    = CreateFileW( Filename, Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL );
	INT    Pos       = 0;
	if( Handle==INVALID_HANDLE_VALUE )
	{
		if( Flags & FILEWRITE_NoFail )
		{
			const DWORD LastError = GetLastError();
			appErrorf( TEXT("Failed to create file: %s, GetLastError %u"), Filename, LastError );
		}
		return NULL;
	}
	if( Flags & FILEWRITE_Append )
	{
		Pos = SetFilePointer( Handle, 0, NULL, FILE_END );
	}
	FArchive* retArch = new FArchiveFileWriterWindows(Handle,StatsHandle,Filename,Error,Pos);
	if( retArch && (Flags & FILEWRITE_SaveGame) )
	{
		retArch->SetIsSaveGame( TRUE );
	}
	return retArch;
}

INT FFileManagerWindows::InternalFileSize( const TCHAR* Filename )
{
	HANDLE Handle = CreateFileW( Filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if( Handle==INVALID_HANDLE_VALUE )
	{
		return -1;
	}
	DWORD Result = GetFileSize( Handle, NULL );
	CloseHandle( Handle );
	return Result;
}

DWORD FFileManagerWindows::InternalCopy( const TCHAR* DestFile, const TCHAR* SrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, FCopyProgress* Progress )
{
	if( EvenIfReadOnly )
	{
		SetFileAttributesW(DestFile, 0);
	}
	DWORD Result;
	if( Progress )
	{
		Result = FFileManagerGeneric::Copy( DestFile, SrcFile, ReplaceExisting, EvenIfReadOnly, Attributes, Progress );
	}
	else
	{
		MakeDirectory(*FFilename(DestFile).GetPath(), TRUE);
		if( CopyFileW(SrcFile, DestFile, !ReplaceExisting) != 0)
		{
			Result = COPY_OK;
		}
		else
		{
			Result = COPY_MiscFail;
		}
	}
	if( Result==COPY_OK && !Attributes )
	{
		SetFileAttributesW(DestFile, 0);
	}
	return Result;
}

UBOOL FFileManagerWindows::InternalDelete( const TCHAR* Filename, UBOOL RequireExists, UBOOL EvenReadOnly )
{
	if( EvenReadOnly )
	{
		SetFileAttributesW(Filename,FILE_ATTRIBUTE_NORMAL);
	}
	INT Result = DeleteFile(Filename) != 0;
    DWORD Error = GetLastError();
	Result = Result || (!RequireExists && (Error==ERROR_FILE_NOT_FOUND || Error==ERROR_PATH_NOT_FOUND));
	if( !Result )
	{
		if ((appStristr(appCmdLine(), TEXT("CookPackages")) != NULL) || GIsCooking || ParseParam(appCmdLine(), TEXT("MTCHILD")) || ParseParam(appCmdLine(), TEXT("MT")))
		{
			// this is not an error while doing MT commandlets
			// the cooker will retry and fail gracefully if there is a true issue
			debugf( TEXT("Could not delete '%s'"), Filename);
		}
		else
		{
			DWORD error = GetLastError();
			debugf( NAME_Error, TEXT("Error deleting file '%s' (GetLastError: %d)"), Filename, error );
		}
	}
	return Result!=0;
}

UBOOL FFileManagerWindows::InternalIsReadOnly( const TCHAR* Filename )
{
	DWORD rc;
	if( FileSize( Filename ) < 0 )
	{
		return( 0 );
	}
	rc = GetFileAttributesW(Filename);
	if (rc != 0xFFFFFFFF)
	{
		return ((rc & FILE_ATTRIBUTE_READONLY) != 0);
	}
	else
	{
		debugf( NAME_Error, TEXT("Error reading attributes for '%s'"), Filename );
		return (0);
	}
}

UBOOL FFileManagerWindows::InternalMove( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace, UBOOL EvenIfReadOnly, UBOOL Attributes )
{
	MakeDirectory(*FFilename(Dest).GetPath(), TRUE);
	INT DeleteResult = DeleteFile(Dest);
	DWORD error = 0;
	// Retry on failure, unless the file wasn't there anyway.
	if(DeleteResult == 0 && ( error = GetLastError() ) != ERROR_FILE_NOT_FOUND )
	{
		// If the delete failed, throw a warning but retry before we throw an error
		warnf( NAME_Warning, TEXT("DeleteFile was unable to delete '%s', retrying... (GetLastE-r-r-o-r: %d)"), Dest, error );

		// Wait just a little bit (i.e. a totally arbitrary amount)...
		Sleep(500);

		// Try again
		DeleteResult = DeleteFile(Dest);
		if(DeleteResult == 0)
		{
			error = GetLastError();
			warnf( NAME_Error, TEXT("Error deleting file '%s' (GetLastError: %d)"), Dest, error );
			return FALSE;
		}
		else
		{
			warnf( NAME_Warning, TEXT("DeleteFile recovered during retry!") );
		}		
	}
	
	INT MoveResult = MoveFile(Src,Dest);
	if(MoveResult == 0)
	{
		// If the move failed, throw a warning but retry before we throw an error
		DWORD error = GetLastError();
		warnf( NAME_Warning, TEXT("MoveFileExW was unable to move '%s' to '%s', retrying... (GetLastE-r-r-o-r: %d)"), Src, Dest, error );

		// Wait just a little bit (i.e. a totally arbitrary amount)...
		Sleep(500);

		// Try again
		MoveResult = MoveFile(Src,Dest);
		if(MoveResult == 0)
		{
			error = GetLastError();
			warnf( NAME_Error, TEXT("Error moving file '%s' to '%s' (GetLastError: %d)"), Src, Dest, error );
			return FALSE;
		}
		else
		{
			warnf( NAME_Warning, TEXT("MoveFileExW recovered during retry!") );
		}
	}
	return (MoveResult != 0);
}

UBOOL FFileManagerWindows::InternalMakeDirectory( const TCHAR* Path, UBOOL Tree )
{
	if( Tree )
	{
		return FFileManagerGeneric::MakeDirectory( Path, Tree );
	}

	// it's possible that the path exists, but the user doesn't have access to create it,
	// so CreateDirectory() would return ERROR_ACCESS_DENIED instead of ERROR_ALREADY_EXISTS.
	// since ERROR_ACCESS_DENIED is a valid error for a non-existent path, we want to allow both scenarios to return properly.
	DWORD PathAttributes = GetFileAttributesW(Path);
	if ((PathAttributes != INVALID_FILE_ATTRIBUTES) && ((PathAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0))
	{
		return TRUE;
	}
	else
	{
		return CreateDirectoryW(Path,NULL)!=0 || GetLastError()==ERROR_ALREADY_EXISTS;
	}
}

UBOOL FFileManagerWindows::InternalDeleteDirectory( const TCHAR* Path, UBOOL RequireExists, UBOOL Tree )
{
	if( Tree )
		return FFileManagerGeneric::DeleteDirectory( Path, RequireExists, Tree );
	return RemoveDirectoryW(Path)!=0 || (!RequireExists && GetLastError()==ERROR_FILE_NOT_FOUND);
}

void FFileManagerWindows::InternalFindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories )
{
	HANDLE Handle=NULL;
	WIN32_FIND_DATAW Data;
	Handle=FindFirstFileW(Filename,&Data);
	if( Handle!=INVALID_HANDLE_VALUE )
	{
		do
		{
			if
			(   appStricmp(Data.cFileName,TEXT("."))
			&&  appStricmp(Data.cFileName,TEXT(".."))
			&&  ((Data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)?Directories:Files) )
			{
				new(Result)FString(Data.cFileName);
			}
		}
		while( FindNextFileW(Handle,&Data) );
	}
	if( Handle!=INVALID_HANDLE_VALUE )
	{
		FindClose( Handle );
	}
}

DOUBLE FFileManagerWindows::InternalGetFileAgeSeconds( const TCHAR* Filename )
{
	DOUBLE ReturnValue = -1.0;
	HANDLE FileHandle = CreateFile( Filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
	if( FileHandle != INVALID_HANDLE_VALUE )
	{
		// Get the system time as a file time
		SYSTEMTIME SystemTimeRaw;
		FILETIME SystemTimeFileTime;
		GetSystemTime( &SystemTimeRaw );
		SystemTimeToFileTime( &SystemTimeRaw, &SystemTimeFileTime );

		// Get the file time
		FILETIME FileCreationTime, FileLastAccessTime, FileLastWriteTime;
		GetFileTime( FileHandle, &FileCreationTime, &FileLastAccessTime, &FileLastWriteTime );

		// Compare
		LARGE_INTEGER SystemTimeQuad, FileTimeQuad;
		SystemTimeQuad.LowPart = SystemTimeFileTime.dwLowDateTime;
		SystemTimeQuad.HighPart = SystemTimeFileTime.dwHighDateTime;
		FileTimeQuad.LowPart = FileLastWriteTime.dwLowDateTime;
		FileTimeQuad.HighPart = FileLastWriteTime.dwHighDateTime;

		// Time is in 100-nanosecond units
		ReturnValue = (DOUBLE)( SystemTimeQuad.QuadPart - FileTimeQuad.QuadPart ) / 10000000.0;

		CloseHandle( FileHandle );
	}
	return ReturnValue;
}

DOUBLE FFileManagerWindows::InternalGetFileTimestamp( const TCHAR* Filename )
{
	struct _stat FileInfo;
	if( 0 == _wstat( Filename, &FileInfo ) )
	{
		time_t FileTime;	
		FileTime = FileInfo.st_mtime;
		return FileTime;
	}
	return -1.0;
}

UBOOL FFileManagerWindows::InternalGetTimestamp( const TCHAR* Filename, FTimeStamp& Timestamp )
{
	appMemzero( &Timestamp, sizeof(Timestamp) );
	struct _stat FileInfo;
	if( 0 == _wstat( Filename, &FileInfo ) )
	{
		time_t	FileTime;	
		FileTime = FileInfo.st_mtime;
#if USE_SECURE_CRT
		tm Time;
		gmtime_s(&Time,&FileTime);
#else
		tm& Time = *gmtime(&FileTime);
#endif
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
UBOOL FFileManagerWindows::InternalTouchFile(const TCHAR* Filename)
{
	time_t Now;
	// get the current time
	time(&Now);

	// use now as the time to set
	_utimbuf Time;
	Time.modtime = Now;
	Time.actime = Now;

	// set it to the file
	INT ReturnCode = _wutime(Filename, &Time);

	// 0 return code was a success
	return ReturnCode == 0;
}

/**
 *	Sets the timestamp of a file.
 *
 *	@param Filename		Full path to the file.
 *	@param Timestamp	Timestamp to set
 *	@return				File size in bytes or -1 if the file didn't exist.
 **/
UBOOL FFileManagerWindows::InternalSetFileTimestamp( const TCHAR* Filename, DOUBLE TimeStamp )
{
	_utimbuf Time;
	Time.modtime = TimeStamp;
	Time.actime = TimeStamp;

	// set it to the file
	INT ReturnCode = _wutime(Filename, &Time);

	// 0 return code was a success
	return ReturnCode == 0;
}

/**
 *	Truncates an existing file, discarding data at the end to make it smaller. (Thread-safe)
 *
 *	@param Filename		Full path to the file.
 *	@param FileSize		New file size to truncate to. If this is larger than current file size, the function won't do anything.
 *	@return				Resulting file size or INDEX_NONE if the file didn't exist.
 **/
INT FFileManagerWindows::InternalFileTruncate( const TCHAR* Filename, INT FileSize )
{
	INT NewFileSize = INDEX_NONE;
	HANDLE WinHandle = CreateFile( Filename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	UBOOL bSuccess = (WinHandle != INVALID_HANDLE_VALUE);
	if ( bSuccess )
	{
		DWORD CurrentFileSize = GetFileSize( WinHandle, NULL );
		NewFileSize = INT(CurrentFileSize);
		UBOOL bSuccess = (CurrentFileSize != INVALID_FILE_SIZE);
		if ( bSuccess && CurrentFileSize > DWORD(FileSize) )
		{
			DWORD NewPosition = SetFilePointer( WinHandle, FileSize, NULL, FILE_BEGIN );
			bSuccess = bSuccess && NewPosition == DWORD(FileSize) && SetEndOfFile( WinHandle );
			NewFileSize = FileSize;
		}
		CloseHandle( WinHandle );
	}
	return bSuccess ? NewFileSize : INDEX_NONE;
}

UBOOL FFileManagerWindows::InternalGetDiskFreeSpace(const TCHAR* Drive, QWORD& FreeBytesToCaller, QWORD& TotalBytes, QWORD& FreeBytes )
{
	QWORD LocalFreeBytesToCaller;
	QWORD LocalTotalBytes;
	QWORD LocalFreeBytes;

	UBOOL Result = GetDiskFreeSpaceEx( Drive, (PULARGE_INTEGER)&LocalFreeBytesToCaller, (PULARGE_INTEGER)&LocalTotalBytes, (PULARGE_INTEGER)&LocalFreeBytes );

	if( Result )
	{
		FreeBytesToCaller = LocalFreeBytesToCaller;
		TotalBytes = LocalTotalBytes;
		FreeBytes = LocalFreeBytes;
	}

	return Result;
}


#endif
