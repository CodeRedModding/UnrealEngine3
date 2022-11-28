/*=============================================================================
	FFileManagerWindows.h: Unreal Windows OS based file manager.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "FFileManagerGeneric.h"

#include <sys/types.h>
#include <sys/stat.h>

/*-----------------------------------------------------------------------------
	File Manager.
-----------------------------------------------------------------------------*/

// File manager.
class FArchiveFileReader : public FArchive
{
public:
	FArchiveFileReader( HANDLE InHandle, FOutputDevice* InError, INT InSize )
	:   Handle          ( InHandle )
	,   Error           ( InError )
	,   Size            ( InSize )
	,   Pos             ( 0 )
	,   BufferBase      ( 0 )
	,   BufferCount     ( 0 )
	{
		ArIsLoading = ArIsPersistent = 1;
	}
	~FArchiveFileReader()
	{
		if( Handle )
			Close();
	}
	void Precache( INT HintCount )
	{
		checkSlow(Pos==BufferBase+BufferCount);
		BufferBase = Pos;
		BufferCount = Min( Min( HintCount, (INT)(ARRAY_COUNT(Buffer) - (Pos&(ARRAY_COUNT(Buffer)-1))) ), Size-Pos );
		INT Count=0;
		ReadFile( Handle, Buffer, BufferCount, (DWORD*)&Count, NULL );
		if( Count!=BufferCount )
		{
			ArIsError = 1;
			Error->Logf( TEXT("ReadFile failed: Count=%i BufferCount=%i Error=%s"), Count, BufferCount, appGetSystemErrorMessage() );
		}
	}
	void Seek( INT InPos )
	{
		check(InPos>=0);
		check(InPos<=Size);
		if( SetFilePointer( Handle, InPos, 0, FILE_BEGIN )==0xFFFFFFFF )
		{
			ArIsError = 1;
			Error->Logf( TEXT("SetFilePointer Failed %i/%i: %i %s"), InPos, Size, Pos, appGetSystemErrorMessage() );
		}
		Pos         = InPos;
		BufferBase  = Pos;
		BufferCount = 0;
	}
	INT Tell()
	{
		return Pos;
	}
	INT TotalSize()
	{
		return Size;
	}
	UBOOL Close()
	{
		if( Handle )
			CloseHandle( Handle );
		Handle = NULL;
		return !ArIsError;
	}
	void Serialize( void* V, INT Length )
	{
		while( Length>0 )
		{
			INT Copy = Min( Length, BufferBase+BufferCount-Pos );
			if( Copy==0 )
			{
				if( Length >= ARRAY_COUNT(Buffer) )
				{
					INT Count=0;
					ReadFile( Handle, V, Length, (DWORD*)&Count, NULL );
					if( Count!=Length )
					{
						ArIsError = 1;
						Error->Logf( TEXT("ReadFile failed: Count=%i Length=%i Error=%s"), Count, Length, appGetSystemErrorMessage() );
					}
					Pos += Length;
					BufferBase += Length;
					return;
				}
				Precache( MAXINT );
				Copy = Min( Length, BufferBase+BufferCount-Pos );
				if( Copy<=0 )
				{
					ArIsError = 1;
					Error->Logf( TEXT("ReadFile beyond EOF %i+%i/%i"), Pos, Length, Size );
				}
				if( ArIsError )
					return;
			}
			appMemcpy( V, Buffer+Pos-BufferBase, Copy );
			Pos       += Copy;
			Length    -= Copy;
			V          = (BYTE*)V + Copy;
		}
	}
protected:
	HANDLE          Handle;
	FOutputDevice*  Error;
	INT             Size;
	INT             Pos;
	INT             BufferBase;
	INT             BufferCount;
	BYTE            Buffer[1024];
};
class FArchiveFileWriter : public FArchive
{
public:
	FArchiveFileWriter( HANDLE InHandle, FOutputDevice* InError, INT InPos )
	:   Handle      ( InHandle )
	,   Error       ( InError )
	,   Pos         ( InPos )
	,   BufferCount ( 0 )
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	~FArchiveFileWriter()
	{
		if( Handle )
			Close();
		Handle = NULL;
	}
	void Seek( INT InPos )
	{
		Flush();
		if( SetFilePointer( Handle, InPos, 0, FILE_BEGIN )==0xFFFFFFFF )
		{
			ArIsError = 1;
			Error->Logf( *LocalizeError("SeekFailed",TEXT("Core")) );
		}
		Pos = InPos;
	}
	INT Tell()
	{
		return Pos;
	}
	UBOOL Close()
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
	void Serialize( void* V, INT Length )
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
	void Flush()
	{
		if( BufferCount )
		{
			INT Result=0;
			if( !WriteFile( Handle, Buffer, BufferCount, (DWORD*)&Result, NULL ) )
			{
				ArIsError = 1;
				Error->Logf( *LocalizeError("WriteFailed",TEXT("Core")) );
			}
		}
		BufferCount = 0;
	}
protected:
	HANDLE          Handle;
	FOutputDevice*  Error;
	INT             Pos;
	INT             BufferCount;
	BYTE            Buffer[4096];
};
class FFileManagerWindows : public FFileManagerGeneric
{
public:
	FArchive* CreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		DWORD  Access    = GENERIC_READ;
		DWORD  WinFlags  = FILE_SHARE_READ;
		DWORD  Create    = OPEN_EXISTING;
		HANDLE Handle    = TCHAR_CALL_OS( CreateFileW( Filename, Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL ), CreateFileA( TCHAR_TO_ANSI(Filename), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL ) );
		if( Handle==INVALID_HANDLE_VALUE )
		{
			if( Flags & FILEREAD_NoFail )
				appErrorf( TEXT("Failed to read file: %s"), Filename );
			return NULL;
		}
		return new FArchiveFileReader(Handle,Error,GetFileSize(Handle,NULL));
	}
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		if( (GFileManager->FileSize (Filename) >= 0) && (Flags & FILEWRITE_EvenIfReadOnly) )
			TCHAR_CALL_OS(SetFileAttributesW(Filename, 0),SetFileAttributesA(TCHAR_TO_ANSI(Filename), 0));
		DWORD  Access    = GENERIC_WRITE;
		DWORD  WinFlags  = (Flags & FILEWRITE_AllowRead) ? FILE_SHARE_READ : 0;
		DWORD  Create    = (Flags & FILEWRITE_Append) ? OPEN_ALWAYS : (Flags & FILEWRITE_NoReplaceExisting) ? CREATE_NEW : CREATE_ALWAYS;
		HANDLE Handle    = TCHAR_CALL_OS( CreateFileW( Filename, Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL ), CreateFileA( TCHAR_TO_ANSI(Filename), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL ) );
		INT    Pos       = 0;
		if( Handle==INVALID_HANDLE_VALUE )
		{
			if( Flags & FILEWRITE_NoFail )
				appErrorf( TEXT("Failed to create file: %s"), Filename );
			return NULL;
		}
		if( Flags & FILEWRITE_Append )
			Pos = SetFilePointer( Handle, 0, 0, FILE_END );
		return new FArchiveFileWriter(Handle,Error,Pos);
	}
	INT FileSize( const TCHAR* Filename )
	{
		HANDLE Handle = TCHAR_CALL_OS( CreateFileW( Filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ), CreateFileA( TCHAR_TO_ANSI(Filename), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) );
		if( Handle==INVALID_HANDLE_VALUE )
			return -1;
		DWORD Result = GetFileSize( Handle, NULL );
		CloseHandle( Handle );
		return Result;
	}
	DWORD Copy( const TCHAR* DestFile, const TCHAR* SrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, DWORD Compress, FCopyProgress* Progress )
	{
		if( EvenIfReadOnly )
			TCHAR_CALL_OS(SetFileAttributesW(DestFile, 0),SetFileAttributesA(TCHAR_TO_ANSI(DestFile), 0));
		DWORD Result;
		if( Progress || Compress != FILECOPY_Normal )
			Result = FFileManagerGeneric::Copy( DestFile, SrcFile, ReplaceExisting, EvenIfReadOnly, Attributes, Compress, Progress );
		else
		{
			if( TCHAR_CALL_OS(CopyFileW(SrcFile, DestFile, !ReplaceExisting),CopyFileA(TCHAR_TO_ANSI(SrcFile), TCHAR_TO_ANSI(DestFile), !ReplaceExisting)) != 0)
				Result = COPY_OK;
			else
				Result = COPY_MiscFail;
		}
		if( Result==COPY_OK && !Attributes )
			TCHAR_CALL_OS(SetFileAttributesW(DestFile, 0),SetFileAttributesA(TCHAR_TO_ANSI(DestFile), 0));
		return Result;
	}
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
	{
		if( EvenReadOnly )
			TCHAR_CALL_OS(SetFileAttributesW(Filename,FILE_ATTRIBUTE_NORMAL),SetFileAttributesA(TCHAR_TO_ANSI(Filename),FILE_ATTRIBUTE_NORMAL));
		INT Result = TCHAR_CALL_OS(DeleteFile(Filename),DeleteFileA(TCHAR_TO_ANSI(Filename)))!=0 || (!RequireExists && GetLastError()==ERROR_FILE_NOT_FOUND);
		if( !Result )
		{
			DWORD error = GetLastError();
			debugf( NAME_Warning, TEXT("Error deleting file '%s' (0x%d)"), Filename, error );
		}
		return Result!=0;
	}
	UBOOL IsReadOnly( const TCHAR* Filename )
	{
		DWORD rc;
		if( FileSize( Filename ) < 0 )
			return( 0 );
		rc = TCHAR_CALL_OS(GetFileAttributesW(Filename),GetFileAttributesA(TCHAR_TO_ANSI(Filename)));
		if (rc != 0xFFFFFFFF)
			return ((rc & FILE_ATTRIBUTE_READONLY) != 0);
		else
		{
			debugf( NAME_Warning, TEXT("Error reading attributes for '%s'"), Filename );
			return (0);
		}
	}
	UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 )
	{
		//warning: MoveFileEx is broken on Windows 95 (Microsoft bug).
		Delete( Dest, 0, EvenIfReadOnly );
		INT Result = TCHAR_CALL_OS( MoveFileW(Src,Dest), MoveFileA(TCHAR_TO_ANSI(Src),TCHAR_TO_ANSI(Dest)) );
		if( !Result )
		{
			DWORD error = GetLastError();
			debugf( NAME_Warning, TEXT("Error moving file '%s' to '%s' (%d)"), Src, Dest, error );
		}
		return Result!=0;
	}
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )
	{
		if( Tree )
			return FFileManagerGeneric::MakeDirectory( Path, Tree );
		return TCHAR_CALL_OS( CreateDirectoryW(Path,NULL), CreateDirectoryA(TCHAR_TO_ANSI(Path),NULL) )!=0 || GetLastError()==ERROR_ALREADY_EXISTS;
	}
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )
	{
		if( Tree )
			return FFileManagerGeneric::DeleteDirectory( Path, RequireExists, Tree );
		return TCHAR_CALL_OS( RemoveDirectoryW(Path), RemoveDirectoryA(TCHAR_TO_ANSI(Path)) )!=0 || (!RequireExists && GetLastError()==ERROR_FILE_NOT_FOUND);
	}
	void FindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories )
	{
		HANDLE Handle=NULL;
#if UNICODE
		if( GUnicodeOS )
		{
			WIN32_FIND_DATAW Data;
			Handle=FindFirstFileW(Filename,&Data);
			if( Handle!=INVALID_HANDLE_VALUE )
				do
					if
					(   appStricmp(Data.cFileName,TEXT("."))
					&&  appStricmp(Data.cFileName,TEXT(".."))
					&&  ((Data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)?Directories:Files) )
						new(Result)FString(Data.cFileName);
				while( FindNextFileW(Handle,&Data) );
		}
		else
#endif
		{
			WIN32_FIND_DATAA Data;
			Handle=FindFirstFileA(TCHAR_TO_ANSI(Filename),&Data);
			if( Handle!=INVALID_HANDLE_VALUE )
			{
				do
				{
					TCHAR Name[MAX_PATH];
					winToUNICODE( Name, Data.cFileName, Min<INT>( ARRAY_COUNT(Name), winGetSizeUNICODE(Data.cFileName) ) );              
					if
					(	appStricmp(Name,TEXT("."))
					&&	appStricmp(Name,TEXT(".."))
					&&	((Data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)?Directories:Files) )
						new(Result)FString(Name);
				}
				while( FindNextFileA(Handle,&Data) );
			}
		}
		if( Handle!=INVALID_HANDLE_VALUE )
			FindClose( Handle );
	}
	DOUBLE GetFileAgeSeconds( const TCHAR* Filename )
	{
		struct _stat FileInfo;
		if( 0 == TCHAR_CALL_OS( _wstat( Filename, &FileInfo ), _stat( TCHAR_TO_ANSI( Filename ), &FileInfo ) ) )
		{
			time_t	CurrentTime,
					FileTime;	
			FileTime = FileInfo.st_mtime;
			time( &CurrentTime );

			return difftime( CurrentTime, FileTime );
		}
		return -1.0;
	}
	UBOOL SetDefaultDirectory()
	{
		return TCHAR_CALL_OS(SetCurrentDirectoryW(appBaseDir()),SetCurrentDirectoryA(TCHAR_TO_ANSI(appBaseDir())))!=0;
	}
	FString GetCurrentDirectory()
	{
#if UNICODE
		if( GUnicodeOS )
		{
			TCHAR Buffer[1024]=TEXT("");
			::GetCurrentDirectoryW(ARRAY_COUNT(Buffer),Buffer);
			return FString(Buffer);
		}
		else
#endif
		{
			ANSICHAR Buffer[1024]="";
			::GetCurrentDirectoryA(ARRAY_COUNT(Buffer),Buffer);
			return FString( Buffer );
		}
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

