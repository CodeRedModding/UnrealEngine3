/*=============================================================================
	FFileManagerAnsi.h: Unreal ANSI C based file manager.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "FFileManagerGeneric.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <errno.h>
#include <direct.h>

/*-----------------------------------------------------------------------------
	File Manager.
-----------------------------------------------------------------------------*/

// File manager.
class FArchiveFileReader : public FArchive
{
public:
	FArchiveFileReader( FILE* InFile, FOutputDevice* InError, INT InSize )
	:	File			( InFile )
	,	Error			( InError )
	,	Size			( InSize )
	,	Pos				( 0 )
	,	BufferBase		( 0 )
	,	BufferCount		( 0 )
	{
		fseek( File, 0, SEEK_SET );
		ArIsLoading = ArIsPersistent = 1;
	}
	~FArchiveFileReader()
	{
		if( File )
			Close();
	}
	void Precache( INT HintCount )
	{
		checkSlow(Pos==BufferBase+BufferCount);
		BufferBase = Pos;
		BufferCount = Min( Min( HintCount, (INT)(ARRAY_COUNT(Buffer) - (Pos&(ARRAY_COUNT(Buffer)-1))) ), Size-Pos );
		if( fread( Buffer, BufferCount, 1, File )!=1 && BufferCount!=0 )
		{
			ArIsError = 1;
			Error->Logf( TEXT("fread failed: BufferCount=%i Error=%i"), BufferCount, ferror(File) );
			return;
		}
	}
	void Seek( INT InPos )
	{
		check(InPos>=0);
		check(InPos<=Size);
		if( fseek(File,InPos,SEEK_SET) )
		{
			ArIsError = 1;
			Error->Logf( TEXT("seek Failed %i/%i: %i %i"), InPos, Size, Pos, ferror(File) );
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
		if( File )
			fclose( File );
		File = NULL;
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
					if( fread( V, Length, 1, File )!=1 )
					{
						ArIsError = 1;
						Error->Logf( TEXT("fread failed: Length=%i Error=%i"), Length, ferror(File) );
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
	FILE*			File;
	FOutputDevice*	Error;
	INT				Size;
	INT				Pos;
	INT				BufferBase;
	INT				BufferCount;
	BYTE			Buffer[1024];
};
class FArchiveFileWriter : public FArchive
{
public:
	FArchiveFileWriter( FILE* InFile, FOutputDevice* InError )
	:	File		(InFile)
	,	Error		( InError )
	,	Pos			(0)
	,	BufferCount	(0)
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	~FArchiveFileWriter()
	{
		if( File )
			Close();
		File = NULL;
	}
	void Seek( INT InPos )
	{
		Flush();
		if( fseek(File,InPos,SEEK_SET) )
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
		if( File && fclose( File ) )
		{
			ArIsError = 1;
			Error->Logf( *LocalizeError("WriteFailed",TEXT("Core")) );
		}
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
		if( BufferCount && fwrite( Buffer, BufferCount, 1, File )!=1 )
		{
			ArIsError = 1;
			Error->Logf( *LocalizeError("WriteFailed",TEXT("Core")) );
		}
		BufferCount=0;
	}
protected:
	FILE*			File;
	FOutputDevice*	Error;
	INT				Pos;
	INT				BufferCount;
	BYTE			Buffer[4096];
};
class FFileManagerAnsi : public FFileManagerGeneric
{
public:
	FArchive* CreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		FILE* File = TCHAR_CALL_OS(_wfopen(Filename,TEXT("rb")),fopen(TCHAR_TO_ANSI(Filename),TCHAR_TO_ANSI(TEXT("rb"))));
		if( !File )
		{
			if( Flags & FILEREAD_NoFail )
				appErrorf(TEXT("Failed to read file: %s"),Filename);
			return NULL;
		}
		fseek( File, 0, SEEK_END );
		return new(TEXT("AnsiFileReader"))FArchiveFileReader(File,Error,ftell(File));
	}
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		if( Flags & FILEWRITE_EvenIfReadOnly )
			TCHAR_CALL_OS(_wchmod(Filename, _S_IREAD | _S_IWRITE),_chmod(TCHAR_TO_ANSI(Filename), _S_IREAD | _S_IWRITE));
		if( (Flags & FILEWRITE_NoReplaceExisting) && FileSize(Filename)>=0 )
			return NULL;
		const TCHAR* Mode = (Flags & FILEWRITE_Append) ? TEXT("ab") : TEXT("wb"); 
		FILE* File = TCHAR_CALL_OS(_wfopen(Filename,Mode),fopen(TCHAR_TO_ANSI(Filename),TCHAR_TO_ANSI(Mode)));
		if( !File )
		{
			if( Flags & FILEWRITE_NoFail )
				appErrorf( TEXT("Failed to write: %s"), Filename );
			return NULL;
		}
		if( Flags & FILEWRITE_Unbuffered )
			setvbuf( File, 0, _IONBF, 0 );
		return new(TEXT("AnsiFileWriter"))FArchiveFileWriter(File,Error);
	}
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
	{
		if( EvenReadOnly )
			TCHAR_CALL_OS(_wchmod(Filename, _S_IREAD | _S_IWRITE),_chmod(TCHAR_TO_ANSI(Filename), _S_IREAD | _S_IWRITE));
		return TCHAR_CALL_OS(_wunlink(Filename),_unlink(TCHAR_TO_ANSI(Filename)))==0 || (errno==ENOENT && !RequireExists);
	}
    UBOOL IsReadOnly( const TCHAR* Filename )
    {
        if( FileSize( Filename ) < 0 )
            return( 0 );
		return (TCHAR_CALL_OS(_waccess(Filename, 2),_access(TCHAR_TO_ANSI(Filename), 2)) == 0);
    }
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )
	{
		if( Tree )
			return FFileManagerGeneric::MakeDirectory( Path, Tree );
		//warning: ANSI compliance is questionable here.
		return TCHAR_CALL_OS( _wmkdir(Path), _mkdir(TCHAR_TO_ANSI(Path)) )==0 || errno==EEXIST;
	}
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )
	{
		if( Tree )
			return FFileManagerGeneric::DeleteDirectory( Path, RequireExists, Tree );
		//warning: ANSI compliance is questionable here.
		return TCHAR_CALL_OS( _wrmdir(Path), _rmdir(TCHAR_TO_ANSI(Path)) )==0 || (errno==ENOENT && !RequireExists);
	}
	void FindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories )
	{
		//warning: ANSI compliance is questionable here.
#if UNICODE
		if( GUnicodeOS )
		{
			_wfinddata_t Found;
			long hFind = _wfindfirst( (wchar_t*)Filename, &Found );
			if( hFind != -1 )
			do
				if
				(	appStrcmp(Found.name,TEXT("."))!=0
				&&	appStrcmp(Found.name,TEXT(".."))!=0
				&&	((Found.attrib & _A_SUBDIR) ? Directories : Files) )
					new(Result)FString( Found.name );
			while( _wfindnext( hFind, &Found )!=-1 );
			_findclose(hFind);
		}
		else
#endif
		{
			_finddata_t Found;
			long hFind = _findfirst( TCHAR_TO_ANSI(Filename), &Found );
			if( hFind != -1 )
			{
				do
				{
					const TCHAR* Name = ANSI_TO_TCHAR(Found.name);
					if
					(	appStrcmp(Name,TEXT("."))!=0
					&&	appStrcmp(Name,TEXT(".."))!=0
					&&	((Found.attrib & _A_SUBDIR) ? Directories : Files) )
						new(Result)FString( Name );
				}
				while( _findnext( hFind, &Found )!=-1 );
			}
			_findclose(hFind);
		}
	}
	DOUBLE GetFileAgeSeconds( const TCHAR* Filename )
	{
		//@warning: ANSI compliance is questionable here.
		struct _stat FileInfo;
		if( TCHAR_CALL_OS( _wstat( Filename, &FileInfo ), _stat( TCHAR_TO_ANSI( Filename ), &FileInfo ) ) )
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
		//warning: ANSI compliance is questionable here.
		return TCHAR_CALL_OS( _wchdir(appBaseDir()), chdir(TCHAR_TO_ANSI(appBaseDir())) )==0;
	}
	FString GetCurrentDirectory()
	{
		//warning: ANSI compliance is questionable here.
#if UNICODE
		if( GUnicodeOS )
		{
			TCHAR Buffer[1024]=TEXT("");
			_wgetcwd(Buffer,ARRAY_COUNT(Buffer));
			return FString(Buffer);
		}
		else
#endif
		{
			ANSICHAR Buffer[1024]="";
			getcwd( Buffer, ARRAY_COUNT(Buffer) );
			return FString(Buffer);
		}
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

