/*=============================================================================
 	FFileManagerIPhone.cpp: Unreal iPhone based file manager.
 	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "CorePrivate.h"
#include "FFileManagerIPhone.h"
#include "IPhoneObjCWrapper.h"
#include "UnIpDrv.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>


// globals

FString FFileManagerIPhone::AppDir;
FString FFileManagerIPhone::DocDir;
FSocket* FFileManagerIPhone::FileServer;

/**
 * If we started with ..\.., skip over a ..\, since the TOC won't be expecting that 
 * 4 ifs so we are separator independent (/ or \)
 *
 * @param Filename [in/out] Filename to skip over the ..\ if ..\.. was passed in
 */ 
void FixupExtraDots(const TCHAR*& Filename)
{
	if (Filename[0] == '.' && Filename[1] == '.' && Filename[3] == '.' && Filename[4] == '.')
	{ 
		Filename += 3;
	}
}

/*-----------------------------------------------------------------------------
 FArchiveFileReaderIPhone implementation
 -----------------------------------------------------------------------------*/

FArchiveFileReaderIPhone::FArchiveFileReaderIPhone( int InHandle, const ANSICHAR* InFilename, FOutputDevice* InError, INT InSize )
:   Handle          ( InHandle )
,	Filename		( InFilename )
,   Error           ( InError )
,   Size            ( InSize )
,   Pos             ( 0 )
,   BufferBase      ( 0 )
,   BufferCount     ( 0 )
{
	ArIsLoading = ArIsPersistent = 1;
	StatsHandle = FILE_IO_STATS_GET_HANDLE( ANSI_TO_TCHAR(InFilename) );
}

FArchiveFileReaderIPhone::~FArchiveFileReaderIPhone()
{
	FILE_IO_STATS_CLOSE_HANDLE( StatsHandle );
	Close();
}

UBOOL FArchiveFileReaderIPhone::InternalPrecache( INT PrecacheOffset, INT PrecacheSize )
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
			TCHAR ErrorBuffer[1024];
			Error->Logf( TEXT("ReadFile failed: Count=%i BufferCount=%i Error=%s"), (INT) Count, BufferCount, appGetSystemErrorMessage(ErrorBuffer,1024) );
		}
	}
	return TRUE;
}

void FArchiveFileReaderIPhone::Seek( INT InPos )
{
	check(InPos>=0);
	check(InPos<=Size);

	if (InPos == Pos || (InPos >= BufferBase && InPos < BufferBase + BufferCount))
	{
		Pos = InPos;
		return;
	}

	if( ::lseek( Handle, InPos, SEEK_SET )==-1 )
	{
		ArIsError = 1;
		TCHAR ErrorBuffer[1024];
		Error->Logf( TEXT("SetFilePointer Failed %i/%i: %i %s"), InPos, Size, Pos, appGetSystemErrorMessage(ErrorBuffer,1024) );
	}
	Pos         = InPos;
	BufferBase  = Pos;
	BufferCount = 0;
}

INT FArchiveFileReaderIPhone::Tell()
{
	return Pos;
}

INT FArchiveFileReaderIPhone::TotalSize()
{
	return Size;
}

UBOOL FArchiveFileReaderIPhone::Close()
{
	if( Handle != -1)
	{
		close( Handle );
	}
	Handle = -1L;
	return !ArIsError;
}

void FArchiveFileReaderIPhone::Serialize( void* V, INT Length )
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
					TCHAR ErrorBuffer[1024];
					Error->Logf( TEXT("ReadFile failed: Count=%i Length=%i Error=%s"), (INT) Count, Length, appGetSystemErrorMessage(ErrorBuffer,1024) );
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
 FArchiveFileWriterIPhone implementation
 -----------------------------------------------------------------------------*/

FArchiveFileWriterIPhone::FArchiveFileWriterIPhone( int InHandle, const ANSICHAR* InFilename, FOutputDevice* InError, INT InPos )
:   Handle      ( InHandle )
,	Filename	( InFilename )
,   Error       ( InError )
,   Pos         ( InPos )
,   BufferCount ( 0 )
{
	ArIsSaving = ArIsPersistent = 1;
	StatsHandle = FILE_IO_STATS_GET_HANDLE( ANSI_TO_TCHAR(InFilename) );
}

FArchiveFileWriterIPhone::~FArchiveFileWriterIPhone()
{
	FILE_IO_STATS_CLOSE_HANDLE( StatsHandle );
	Close();
}

void FArchiveFileWriterIPhone::Seek( INT InPos )
{
	Flush();
	if( ::lseek( Handle, InPos, SEEK_SET ) == -1 )
	{
		ArIsError = 1;
		Error->Logf( *LocalizeError("SeekFailed",TEXT("Core")) );
	}
	Pos = InPos;
}

INT FArchiveFileWriterIPhone::Tell()
{
	return Pos;
}

UBOOL FArchiveFileWriterIPhone::Close()
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

void FArchiveFileWriterIPhone::Serialize( void* V, INT Length )
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

void FArchiveFileWriterIPhone::Flush()
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

	::fsync(Handle);
	BufferCount = 0;
}


/*-----------------------------------------------------------------------------
 FFileManagerIPhone implementation
 -----------------------------------------------------------------------------*/

/**
 * Perform early file manager initialization, particularly finding the application and document directories
 */
void FFileManagerIPhone::StaticInit()
{
	// set the game name as early as possible, since we need it (engine will call it again, but that's okay)
	extern void appSetGameName();
	appSetGameName();
	
	ANSICHAR dir[IPHONE_PATH_MAX];
	IPhoneGetDocumentDirectory( dir, IPHONE_PATH_MAX );
	DocDir = FString( ANSI_TO_TCHAR( dir ) ) + TEXT("/");
	
	IPhoneGetApplicationDirectory( dir, IPHONE_PATH_MAX );
	// lame hack
	FFilename WrongAppDir(ANSI_TO_TCHAR(dir));
	// strip off the "Applications"
	WrongAppDir = WrongAppDir.GetPath();
	AppDir = WrongAppDir + TEXT("/") + appGetGameName() + TEXT("Game.app/") ;	
}


/**
 * Converts passed in filename to use an absolute path.
 *
 * @param	Filename	filename to convert to use an absolute path, safe to pass in already using absolute path
 * 
 * @return	filename using absolute path
 */
FString FFileManagerIPhone::ConvertToAbsolutePath( const TCHAR* Filename )
{
	FIPhonePath PlatformFilename;
	ConvertToIPhonePath(Filename, PlatformFilename);

	return FString(PlatformFilename);
}

/**
 * Converts a path pointing into the iPhone read location, and converts it to the parallel
 * write location. This allows us to read files that the iPhone has written out
 *
 * @param AbsolutePath Source path to convert
 *
 * @return Path to the write directory
 */
FString FFileManagerIPhone::ConvertAbsolutePathToUserPath(const TCHAR* AbsolutePath)
{
	return AbsolutePath;
	FString UserPath( AbsolutePath );
	UserPath = UserPath.Replace( *AppDir, *DocDir, false );
	return UserPath;
}

/*-----------------------------------------------------------------------------
 Public interface
 -----------------------------------------------------------------------------*/

void FFileManagerIPhone::PreInit()
{
	// set up the read and write root directories

	// always write to documents directory
	appStrcpyANSI(WriteRootPath, TCHAR_TO_UTF8(*DocDir));
	WriteRootPathLen = DocDir.Len();

	// during pre init time, we need to read from the app directory, so the Network file host
	// text file can be read (it will always be part of the application)
	appStrcpyANSI(ReadRootPath, TCHAR_TO_UTF8(*AppDir));
	ReadRootPathLen = AppDir.Len();
}

void FFileManagerIPhone::Init(UBOOL Startup)
{	
	// allow base class initialization
	FFileManagerGeneric::Init(Startup);

	// read from documents when we aren't GFileManager (because the FFileManagerNetwork
	// is wrapping us and therefore our data files will be in the Documents dir)
	if (GFileManager == this)
	{
		appStrcpyANSI(ReadRootPath, TCHAR_TO_UTF8(*AppDir));
		ReadRootPathLen = AppDir.Len();
	}
	else
	{
		appStrcpyANSI(ReadRootPath, TCHAR_TO_UTF8(*DocDir));
		ReadRootPathLen = DocDir.Len();
	}

	// read in the TOC file
	TArray<BYTE> Buffer;
	const TCHAR* TOCName;
	TOCName = TEXT("IPhoneTOC.txt");

	// merge in a per-language TOC file if it exists
	FString Lang = appGetLanguageExt();
	if (Lang != TEXT("int"))
	{
		FString LocTOCName = FString::Printf(TEXT("IPhoneTOC_%s.txt"), *Lang);
		ReadTOC( TOC, *LocTOCName, FALSE );
	}

	// read in the main TOC - this needs to be after the loc toc reading
	ReadTOC( TOC, TOCName, TRUE );

	// cloud paths always use the write path (for reading/find files/etc)
	WritePaths.AddItem(appCloudDir());
	// same for files using the cache paths
	WritePaths.AddItem(appCacheDir());
}

UBOOL FFileManagerIPhone::SetDefaultDirectory()
{
	return chdir(TCHAR_TO_UTF8( *AppDir )) != -1;
}

UBOOL FFileManagerIPhone::SetCurDirectory(const TCHAR* Directory)
{
	return chdir(TCHAR_TO_UTF8( Directory )) != -1;
}

// !!! FIXME: all of this should happen in the base class, since it's all the same on Windows...
// !!! FIXME:  (maybe there should be a FFileManagerPC class above us, so consoles don't get this?)

FString FFileManagerIPhone::GetCurrentDirectory()
{
	return AppDir;
}

FArchive* FFileManagerIPhone::CreateFileReader( const TCHAR* InFilename, DWORD Flags, FOutputDevice* Error )
{
	FIPhonePath PlatformFilename;
	// read save games from the Write directory (passing TRUE as the 3rd param makes it use the Docs directory)
	ConvertToIPhonePath(InFilename, PlatformFilename, (Flags & FILEREAD_SaveGame) != 0);

	const int Handle = ::open(PlatformFilename, O_RDONLY);
	if( Handle == -1 )
	{
		if( Flags & FILEREAD_NoFail )
		{
			appErrorf( TEXT("Failed to read file: %s"), InFilename );
		}
		return NULL;
	}

	const INT FileSize = GetIPhoneFileSize(Handle);
	if( FileSize < 0)
	{
		::close(Handle);
		if( Flags & FILEREAD_NoFail )
		{
			appErrorf( TEXT("Failed to read file: %s"), InFilename );
		}
		return NULL;
	}

	FArchiveFileReaderIPhone* Ar = new FArchiveFileReaderIPhone(Handle,PlatformFilename,Error,FileSize);
	Ar->SetIsSaveGame((Flags & FILEREAD_SaveGame) != 0);
	return Ar;
}

FArchive* FFileManagerIPhone::CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT MaxFileSize )
{
	FIPhonePath PlatformFilename;
	ConvertToIPhonePath(Filename, PlatformFilename, TRUE);

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

	int Handle = ::open(PlatformFilename, UnixFlags, 0600);
	if ( (Handle == -1) && (errno == EACCES) && (errno == (Flags & FILEWRITE_EvenIfReadOnly)) )
	{
		struct stat StatBuf;
		if (::stat(PlatformFilename, &StatBuf) != -1)
		{
			StatBuf.st_mode |= S_IWUSR;
			if (::chmod(PlatformFilename, StatBuf.st_mode) != -1)
			{
				Handle = ::open(PlatformFilename, UnixFlags, 0600);
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

	FArchiveFileWriterIPhone* Ar = new FArchiveFileWriterIPhone(Handle,PlatformFilename,Error,Pos);
	Ar->SetIsSaveGame((Flags & FILEWRITE_SaveGame) != 0);
	return Ar;
}

INT FFileManagerIPhone::FileSize( const TCHAR* Filename )
{
	// skip extra ..\ if needed
	FixupExtraDots(Filename);

	// GetFileSize() automatically takes care of TOC synchronization for us
	INT FileSize = TOC.GetFileSize(Filename);
	if (FileSize == -1)
	{
		FIPhonePath PlatformFilename;
		ConvertToIPhonePath(Filename, PlatformFilename);

		// if not in the TOC, look on disk
		struct stat StatBuf;
		if (::stat(PlatformFilename, &StatBuf) == -1)
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

		FileSize = (INT)StatBuf.st_size;
	}

	return FileSize;
}

INT FFileManagerIPhone::UncompressedFileSize( const TCHAR* Filename )
{
	// skip extra ..\ if needed
	FixupExtraDots(Filename);

	return TOC.GetUncompressedFileSize(Filename);
}

UBOOL FFileManagerIPhone::Delete( const TCHAR* Filename, UBOOL RequireExists, UBOOL EvenReadOnly )
{
	// we can only delete from write directory
	FIPhonePath PlatformFilename;
	ConvertToIPhonePath(Filename, PlatformFilename, TRUE);

	// You can delete files in Unix if you don't have write permission to them
	//  (the question is whether you have write access to its parent dir), but
	//  in the spirit of what the Windows codebase probably wants, we'll just
	//  ignore EvenReadOnly here.

	INT Result = (::unlink(PlatformFilename) == -1);
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

UBOOL FFileManagerIPhone::IsReadOnly( const TCHAR* Filename )
{
	// we can only delete from write directory
	FIPhonePath PlatformFilename;
	ConvertToIPhonePath(Filename, PlatformFilename);

	if (::access(PlatformFilename, F_OK) == -1)
	{
		return FALSE;  // The Windows codepath returns 0 here too...
	}

	if (::access(PlatformFilename, W_OK) == -1)
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

UBOOL FFileManagerIPhone::Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace, UBOOL EvenIfReadOnly, UBOOL Attributes )
{
	// move from read
	FIPhonePath SrcPlatformFilename;
	ConvertToIPhonePath(Src, SrcPlatformFilename, FALSE);

	// to write dir
	FIPhonePath DestPlatformFilename;
	ConvertToIPhonePath(Dest, DestPlatformFilename, TRUE);

	int Result = ::rename(SrcPlatformFilename, DestPlatformFilename);
	if( Result == -1)
	{
		// !!! FIXME: do a copy and delete the original if errno==EXDEV
		debugf( NAME_Error, TEXT("Error moving file '%s' to '%s' (errno: %d)"), Src, Dest, errno );
	}
	return Result!=0;
}

UBOOL FFileManagerIPhone::MakeDirectory( const TCHAR* Path, UBOOL Tree )
{
	// only create directories in Write directory
	FIPhonePath PlatformFilename;
	ConvertToIPhonePath(Path, PlatformFilename, TRUE);

	// let ObjC handle this one
	return IPhoneCreateDirectory(PlatformFilename, Tree ? true : false) == true;
}

UBOOL FFileManagerIPhone::DeleteDirectory( const TCHAR* Path, UBOOL RequireExists, UBOOL Tree )
{
	appErrorf(TEXT("FFileManagerIPhone:: Probably don't get here"));

	if( Tree )
	{
		return FFileManagerGeneric::DeleteDirectory( Path, RequireExists, TRUE );
	}

	// can only delete from Write directory
	FIPhonePath PlatformFilename;
	ConvertToIPhonePath(Path, PlatformFilename, TRUE);

	return ( (rmdir(PlatformFilename) != -1) || ((errno==ENOENT) && (!RequireExists)) );
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

void FFileManagerIPhone::FindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories )
{
	// don't use the TOC before it's been read in (unlikely this would ever happen)
	UBOOL bUseTOC = !TOC.HasBeenInitialized();

	if (bUseTOC)
	{
		// skip extra ..\ if needd
		FixupExtraDots(Filename);

		TOC.FindFiles(Result, Filename, Files, Directories);
	}
	else
	{
		FIPhonePath PlatformFilename;
		ConvertToIPhonePath(Filename, PlatformFilename);

		// Separate path and filename.
		FFilename FileParts(PlatformFilename);
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
	}
}

DOUBLE FFileManagerIPhone::GetFileAgeSeconds( const TCHAR* Filename )
{
	FIPhonePath PlatformFilename;
	ConvertToIPhonePath(Filename, PlatformFilename);

	struct stat FileInfo;
	if (stat(PlatformFilename, &FileInfo) != -1)
	{
		time_t FileTime = FileInfo.st_mtime;
		time_t CurrentTime;
		time( &CurrentTime );
		return difftime( CurrentTime, FileTime );
	}
	return -1.0;
}

DOUBLE FFileManagerIPhone::GetFileTimestamp( const TCHAR* Filename )
{
	FIPhonePath PlatformFilename;
	ConvertToIPhonePath(Filename, PlatformFilename);

	struct stat FileInfo;
	if (stat(PlatformFilename, &FileInfo) != -1)
	{
		return (DOUBLE) FileInfo.st_mtime;
	}
	return -1.0;
}

UBOOL FFileManagerIPhone::GetTimestamp( const TCHAR* Filename, FTimeStamp& Timestamp )
{
	FIPhonePath PlatformFilename;
	ConvertToIPhonePath(Filename, PlatformFilename);

	appMemzero( &Timestamp, sizeof(Timestamp) );
	struct stat FileInfo;
	if (stat(PlatformFilename, &FileInfo) != -1)
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
UBOOL FFileManagerIPhone::TouchFile(const TCHAR* Filename)
{
	// can only touch in the write dir
	FIPhonePath PlatformFilename;
	ConvertToIPhonePath(Filename, PlatformFilename, TRUE);

	// Passing a NULL sets file access/mod time to current system time.
	return (utimes(PlatformFilename, NULL) == 0);
}


/**
 *	Threadsafely converts the platform-independent Unreal filename into platform-specific full path.
 *
 *	@param Filename		Platform-independent Unreal filename
 *	@return				Platform-dependent full filepath
 **/
FString FFileManagerIPhone::GetPlatformFilepath( const TCHAR* Filename )
{
	return ConvertToAbsolutePath(Filename);
}



/*-----------------------------------------------------------------------------
 Internal interface
 -----------------------------------------------------------------------------*/

INT FFileManagerIPhone::GetIPhoneFileSize(int Handle)
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
UBOOL FFileManagerIPhone::FindAlternateFileCase(char *Path)
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


/**
 * Convert a given UE3-style path to one usable on the iPhone
 *
 * @param WinPath The source path to convert
 * @param OutPath The resulting iPhone usable path
 * @param bIsForWriting TRUE of the path will be written to, FALSE if it will be read from
 */
DOUBLE DEBUG_CaseSensitivityOverhead = 0.0;
void FFileManagerIPhone::ConvertToIPhonePath(const TCHAR *WinPath, FIPhonePath& OutPath, UBOOL bIsForWriting)
{
	// if the path starts with a /, then it was already converted, nothing to do
	if (WinPath[0] == '/')
	{
		// convert to UTF8 and return
		appStrcpyANSI(OutPath, TCHAR_TO_UTF8(WinPath));
		return;
	}

	// If the file is using one of the known writable paths then convert to doc path
	FString WinPathStr(WinPath);
	for (INT PathIdx=0; PathIdx < WritePaths.Num(); PathIdx++)
	{
		if (WinPathStr.InStr(WritePaths(PathIdx),FALSE,TRUE) != INDEX_NONE)
		{
			bIsForWriting = TRUE;
			break;
		}
	}
	
	// decide which root path to use
	ANSICHAR* RootPath = bIsForWriting ? WriteRootPath : ReadRootPath;
	INT RootPathLen = bIsForWriting ? WriteRootPathLen : ReadRootPathLen;

	// start with the root reading directory
	appStrcpyANSI(OutPath, RootPath);
	
	// set up a pointer to skip over parts of WinPath
	const TCHAR* WinPathStart = WinPath;

	// strip off up to 2 initial ../ 's
	for (INT Dots = 0; Dots < 2; Dots++)
	{
		if (WinPathStart[0] == '.' && WinPathStart[1] == '.' && (WinPathStart[2] == '/' || WinPathStart[2] == '\\'))
		{
			WinPathStart += 3;
		}
	}

	// now skip over the game name directory (ie, ../../ExampleGame/a.txt is going to be converted to a.txt
	// since ExampleGame the executable name, so we never have an ExampleGame directory in the .app)
	static TCHAR StaticGameName[128];
	static INT StaticGameNameLen = 0;
	// cache the game name if we haven't before
	if (StaticGameNameLen == 0)
	{
		FString GameName = FString(appGetGameName()) + TEXT("Game");
		appStrcpy(StaticGameName, *GameName);
		StaticGameNameLen = GameName.Len();
	}

	// if we start with the GameName, skip over it
	if (appStrnicmp(WinPathStart, StaticGameName, StaticGameNameLen) == 0 &&
		(WinPathStart[StaticGameNameLen] == '/' || WinPathStart[StaticGameNameLen] == '\\'))
	{
		WinPathStart += StaticGameNameLen + 1;
	}

	// now append the final part of WinPath that we want to the OutPath
	appStrcpyANSI(OutPath + RootPathLen, sizeof(OutPath) - RootPathLen, TCHAR_TO_UTF8(WinPathStart));

	// finally, replace slashes for the non-RootPath portion
	for (ANSICHAR* Travel = OutPath + RootPathLen; *Travel; Travel++)
	{
		if (*Travel == '\\')
		{
			*Travel = '/';
		}
	}
	
	// fast rejection: only check this if the current path doesn't exist...
	if (::access(OutPath, F_OK) != 0)
	{
		char *Cur = OutPath;
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
			
			KeepLooking = FindAlternateFileCase(OutPath);
			
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
	}
}
