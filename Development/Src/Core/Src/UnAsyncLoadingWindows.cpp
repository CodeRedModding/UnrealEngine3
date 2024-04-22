/*=============================================================================
	UnAsyncLoadingWindows.cpp: Unreal async loading code, Windows implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#if _WINDOWS

#include "FFileManagerWindows.h"

/*-----------------------------------------------------------------------------
	Async loading stats.
-----------------------------------------------------------------------------*/

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Platform read time"),STAT_AsyncIO_PlatformReadTime,STATGROUP_AsyncIO);


/*-----------------------------------------------------------------------------
	FAsyncIOSystemWindows implementation.
-----------------------------------------------------------------------------*/

/** 
 * Reads passed in number of bytes from passed in file handle.
 *
 * @param	FileHandle	Handle of file to read from.
 * @param	Offset		Offset in bytes from start, INDEX_NONE if file pointer shouldn't be changed
 * @param	Size		Size in bytes to read at current position from passed in file handle
 * @param	Dest		Pointer to data to read into
 *
 * @return	TRUE if read was successful, FALSE otherwise
 */
UBOOL FAsyncIOSystemWindows::PlatformReadDoNotCallDirectly( FAsyncIOHandle FileHandle, INT Offset, INT Size, void* Dest )
{
	DWORD BytesRead		= 0;
	UBOOL bSeekFailed	= FALSE;
	UBOOL bReadFailed	= FALSE;
	{
		SCOPED_FILE_IO_ASYNC_READ_STATS(FileHandle.StatsHandle,Size,Offset);
		if( Offset != INDEX_NONE )
		{
			bSeekFailed = SetFilePointer( FileHandle.Handle, Offset, NULL, FILE_BEGIN ) == INVALID_SET_FILE_POINTER;
		}
		if( !bSeekFailed )
		{
			bReadFailed = ReadFile( FileHandle.Handle, Dest, Size, &BytesRead, NULL ) == 0;
		}
	}
	return BytesRead == Size;
}

/** 
 * Creates a file handle for the passed in file name
 *
 * @param	Filename	Pathname to file
 *
 * @return	INVALID_HANDLE if failure, handle on success
 */
FAsyncIOHandle FAsyncIOSystemWindows::PlatformCreateHandle( const TCHAR* Filename )
{
	FAsyncIOHandle FileHandle;
	FileHandle.StatsHandle	= FILE_IO_STATS_GET_HANDLE( Filename );
	SCOPED_FILE_IO_READ_OPEN_STATS( FileHandle.StatsHandle );

	// for network file loading, make sure it's local before we read it directly
	GFileManager->EnsureFileIsLocal(Filename);

	// first look in the User Directory (look in FFileManagerWindows.cpp)
	FString AbsPath			= GFileManager->ConvertAbsolutePathToUserPath(*appConvertRelativePathToFull( Filename ));
	FileHandle.Handle		= CreateFileW( *AbsPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

	// if that failed, then look in install dir
	if (FileHandle.Handle == INVALID_HANDLE_VALUE)
	{
		FString AbsPath			= appConvertRelativePathToFull( Filename );
		FileHandle.Handle		= CreateFileW( *AbsPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	}

	return FileHandle;
}

/**
 * Closes passed in file handle.
 */
void FAsyncIOSystemWindows::PlatformDestroyHandle( FAsyncIOHandle FileHandle )
{
	FILE_IO_STATS_CLOSE_HANDLE( FileHandle.StatsHandle );
	CloseHandle( FileHandle.Handle );
}

/**
 * Returns whether the passed in handle is valid or not.
 *
 * @param	FileHandle	File hande to check validity
 *
 * @return	TRUE if file handle is valid, FALSE otherwise
 */
UBOOL FAsyncIOSystemWindows::PlatformIsHandleValid( FAsyncIOHandle FileHandle )
{
	return FileHandle.Handle != INVALID_HANDLE_VALUE;
}

#endif

/*----------------------------------------------------------------------------
	End.
----------------------------------------------------------------------------*/
