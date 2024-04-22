/*=============================================================================
 FFileManagerIPhone.h: iPhone file manager declarations
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#ifndef __FFILEMANAGERIPHONE_H__
#define __FFILEMANAGERIPHONE_H__

#include "FTableOfContents.h"
#include "FFileManagerGeneric.h"

// make a buffer for the largest file path (1024 was found on the net)
typedef ANSICHAR FIPhonePath[1024];

/*-----------------------------------------------------------------------------
 FArchiveFileReaderIPhone
 -----------------------------------------------------------------------------*/

// File manager.
class FArchiveFileReaderIPhone : public FArchive
	{
	public:
		FArchiveFileReaderIPhone( int InHandle, const ANSICHAR* InFilename, FOutputDevice* InError, INT InSize );
		~FArchiveFileReaderIPhone();
		
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
 FArchiveFileWriterIPhone
 -----------------------------------------------------------------------------*/

class FArchiveFileWriterIPhone : public FArchive
	{
	public:
		FArchiveFileWriterIPhone( int InHandle, const ANSICHAR* InFilename, FOutputDevice* InError, INT InPos );
		~FArchiveFileWriterIPhone();
		
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
 FFileManagerIPhone
 -----------------------------------------------------------------------------*/

class FFileManagerIPhone : public FFileManagerGeneric
{
public:
	void PreInit();
	void Init(UBOOL Startup);
	
	/**
	 * Convert a given UE3-style path to one usable on the iPhone
	 *
	 * @param WinPath The source path to convert
	 * @param OutPath The resulting iPhone usable path
	 * @param bIsForWriting TRUE of the path will be written to, FALSE if it will be read from
	 */
	void ConvertToIPhonePath(const TCHAR *WinPath, FIPhonePath& OutPath, UBOOL bIsForWriting=FALSE);
	
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
	 * Converts a path pointing into the iPhone read location, and converts it to the parallel
	 * write location. This allows us to read files that the iPhone has written out
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
		
	/**
	 * Perform early file manager initialization, particularly finding the application and document directories
	 */
	static void StaticInit();
	
	/**
	 * @return the application directory that is filled out in StaticInit()
	 */
	static const FString& GetApplicationDirectory()
	{
		return AppDir;
	}
	
	/**
	 * @return the document directory that is filled out in StaticInit()
	 */
	static const FString& GetDocumentDirectory()
	{
		return DocDir;
	}

protected:
	static INT GetIPhoneFileSize(int Handle);
	static UBOOL FindAlternateFileCase(char *Path);
		
	/** iPhone sandboxed paths for the root of the read-only Application bundle directory */
	static FString AppDir;
	/** iPhone sandboxed paths for the writable documents directory */
	static FString DocDir;
	
	/** Helper struct for using the offline created TOC file */
	FTableOfContents TOC;

	/** The socket used to talk to the file server to download files */
	static class FSocket* FileServer;

	/** The root path to read files from */
	FIPhonePath ReadRootPath;
	/** The length of the string in ReadRootPath */
	INT ReadRootPathLen;

	/** The root path to write files to */
	FIPhonePath WriteRootPath;
	/** The length of the string in WriteRootPath */
	INT WriteRootPathLen;

	/** list of paths that will be used for writing so we can convert to the writeable path when reading */
	TArray<FString> WritePaths;
};

#endif
