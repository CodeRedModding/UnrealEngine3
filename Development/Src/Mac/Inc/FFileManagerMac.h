/*=============================================================================
 FFileManagerMac.h: Mac file manager declarations
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#ifndef __FFILEMANAGERMAC_H__
#define __FFILEMANAGERMAC_H__

#include "FTableOfContents.h"
#include "FFileManagerGeneric.h"

// make a buffer for the largest file path (1024 was found on the net)
typedef ANSICHAR FMacPath[1024];

/*-----------------------------------------------------------------------------
 FArchiveFileReaderMac
 -----------------------------------------------------------------------------*/

// File manager.
class FArchiveFileReaderMac : public FArchive
	{
	public:
		FArchiveFileReaderMac( int InHandle, const ANSICHAR* InFilename, FOutputDevice* InError, INT InSize );
		~FArchiveFileReaderMac();
		
		virtual void Seek( INT InPos );
		virtual INT Tell();
		virtual INT TotalSize();
		virtual UBOOL Close();
		virtual void Serialize( void* V, INT Length );
		
	protected:
		UBOOL InternalPrecache( INT PrecacheOffset, INT PrecacheSize );
		
		int             Handle;
		/** Handle for stats tracking */
		INT             StatsHandle;
		/** Filename for debugging purposes. */
		FString         Filename;
		FOutputDevice*  Error;
		INT             Size;
		INT             Pos;
		INT             BufferBase;
		INT             BufferCount;
		BYTE            Buffer[4096];
	};


/*-----------------------------------------------------------------------------
 FArchiveFileWriterMac
 -----------------------------------------------------------------------------*/

class FArchiveFileWriterMac : public FArchive
	{
	public:
		FArchiveFileWriterMac( int InHandle, const ANSICHAR* InFilename, FOutputDevice* InError, INT InPos );
		~FArchiveFileWriterMac();
		
		virtual void Seek( INT InPos );
		virtual INT Tell();
		virtual UBOOL Close();
		virtual void Serialize( void* V, INT Length );
		virtual void Flush();
		
	protected:
		int             Handle;
		/** Handle for stats tracking */
		INT             StatsHandle;
		/** Filename for debugging purposes */
		FString         Filename;
		FOutputDevice*  Error;
		INT             Pos;
		INT             BufferCount;
		BYTE            Buffer[4096];
	};


/*-----------------------------------------------------------------------------
 FFileManagerMac
 -----------------------------------------------------------------------------*/

class FFileManagerMac : public FFileManagerGeneric
{
public:
	void Init(UBOOL Startup);
	
	/**
	 * Convert a given UE3-style path to one usable on the Mac
	 *
	 * @param WinPath The source path to convert
	 * @param OutPath The resulting Mac usable path
	 */
	void ConvertToMacPath(const TCHAR *WinPath, FMacPath& OutPath);

	UBOOL SetDefaultDirectory();
	UBOOL SetCurDirectory(const TCHAR* Directory);
	FString GetCurrentDirectory();
	
	FArchive* CreateFileReader( const TCHAR* InFilename, DWORD Flags, FOutputDevice* Error );
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT MaxFileSize );
	INT FileSize( const TCHAR* Filename );
	INT	UncompressedFileSize( const TCHAR* Filename );
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 );
	UBOOL IsReadOnly( const TCHAR* Filename );
	UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 );
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 );
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 );
	void FindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories );
	DOUBLE GetFileAgeSeconds( const TCHAR* Filename );
	DOUBLE GetFileTimestamp( const TCHAR* Filename );
	UBOOL GetTimestamp( const TCHAR* Filename, FTimeStamp& Timestamp );
	
	/**
	 * Updates the modification time of the file on disk to right now, just like the unix touch command
	 * @param Filename Path to the file to touch
	 * @return TRUE if successful
	 */
	UBOOL TouchFile(const TCHAR* Filename);
	
	/**
	 * Converts passed in filename to use an absolute path.
	 *
	 * @param	Filename	filename to convert to use an absolute path, safe to pass in already using absolute path
	 * 
	 * @return	filename using absolute path
	 */
	virtual FString ConvertToAbsolutePath( const TCHAR* Filename );

	/**
	 * Converts a path pointing into the Mac read location, and converts it to the parallel
	 * write location. This allows us to read files that the Mac has written out
	 *
	 * @param AbsolutePath Source path to convert
	 *
	 * @return Path to the write directory
	 */
	virtual FString ConvertAbsolutePathToUserPath(const TCHAR* AbsolutePath);
	
	/**
	 *	Threadsafely converts the platform-independent Unreal filename into platform-specific full path.
	 *
	 *	@param Filename		Platform-independent Unreal filename
	 *	@return				Platform-dependent full filepath
	 **/
	virtual FString GetPlatformFilepath( const TCHAR* Filename );

protected:
	FArchive* InternalCreateFileReader( const TCHAR* InFilename, DWORD Flags, FOutputDevice* Error );
	FArchive* InternalCreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT MaxFileSize );
	INT InternalFileSize( const TCHAR* Filename );
	UBOOL InternalDelete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 );
	UBOOL InternalIsReadOnly( const TCHAR* Filename );
	UBOOL InternalMove( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 );
	UBOOL InternalMakeDirectory( const TCHAR* Path, UBOOL Tree=0 );
	UBOOL InternalDeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 );
	void InternalFindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories );
	DOUBLE InternalGetFileAgeSeconds( const TCHAR* Filename );
	DOUBLE InternalGetFileTimestamp( const TCHAR* Filename );
	UBOOL InternalGetTimestamp( const TCHAR* Filename, FTimeStamp& Timestamp );
	UBOOL InternalTouchFile(const TCHAR* Filename);

	static INT GetMacFileSize(int Handle);
	static UBOOL FindAlternateFileCase(char *Path);

	/** Directory where a Standard User can write to (to save settings, etc) */
	FString MacUserDir;

	/** Directory where the game in installed to */
	FString MacRootDir;
	
	/** The socket used to talk to the file server to download files */
	static class FSocket* FileServer;
};

#endif
