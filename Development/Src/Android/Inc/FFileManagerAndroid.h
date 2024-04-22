/*=============================================================================
 	FFileManagerAndroid.h: Android file manager declarations
 	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#ifndef __FFILEMANAGERANDROID_H__
#define __FFILEMANAGERANDROID_H__

#include "FTableOfContents.h"
#include "FFileManagerGeneric.h"

/*-----------------------------------------------------------------------------
 FArchiveFileReaderUnix
 -----------------------------------------------------------------------------*/

// File manager.
class FArchiveFileReaderAndroid : public FArchive
	{
	public:
		FArchiveFileReaderAndroid( int InHandle, const TCHAR* InFilename, FOutputDevice* InError, INT InSize, SQWORD InInternalFileOffset );
		~FArchiveFileReaderAndroid();
		
		virtual void Seek( INT InPos );
		virtual INT Tell();
		virtual INT TotalSize();
		virtual UBOOL Close();
		virtual void Serialize( void* V, INT Length );
		
	protected:
		UBOOL InternalPrecache( INT PrecacheOffset, INT PrecacheSize );
		
		SQWORD			InternalFileOffset;
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
 FArchiveFileWriterAndroid
 -----------------------------------------------------------------------------*/

class FArchiveFileWriterAndroid : public FArchive
	{
	public:
		FArchiveFileWriterAndroid( int InHandle, const TCHAR* InFilename, FOutputDevice* InError, INT InPos );
		~FArchiveFileWriterAndroid();
		
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

// Struct used to hold TOC look-up data
struct AndroidTOCEntry
{
	SQWORD Offset;
	INT Size;
};

struct AndroidTOCLookup
{
	FString FileName;
	FName DirectoryName;
};

class AAsset;

/*-----------------------------------------------------------------------------
 FFileManagerAndroid
 -----------------------------------------------------------------------------*/

class FFileManagerAndroid : public FFileManagerGeneric
{
public:
	void Init(UBOOL Startup);
	
	static FString ConvertToAndroidPath(const TCHAR *WinPath);
	
	UBOOL SetDefaultDirectory();
	UBOOL SetCurDirectory(const TCHAR* Directory);
	FString GetCurrentDirectory();
	
	FArchive* CreateFileReader( const TCHAR* InFilename, DWORD Flags, FOutputDevice* Error );
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT MaxFileSize );
	INT FileSize( const TCHAR* Filename );
	INT	UncompressedFileSize( const TCHAR* Filename );
	DWORD Copy( const TCHAR* DestFile, const TCHAR* SrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, FCopyProgress* Progress );
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
	 * Converts a path pointing into the Android read location, and converts it to the parallel
	 * write location. This allows us to read files that the Android has written out
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

	/**
	 * Makes sure that the given file is update and local on this machine
	 *
	 * This is probably temporary and will be replaced with a SideBySide caching scheme
	 *
	 * @return TRUE if the file exists on the file server side
	 */
	static UBOOL VerifyFileIsLocal(const TCHAR* Filename);

	/**
	 * Tries to find handle to file either in Expansions or loose files
	 */
	int GetFileHandle(const TCHAR* Filename, SQWORD &FileOffset, SQWORD &FileLength);

protected:
	static INT GetAndroidFileSize(int Handle);
	static UBOOL FindAlternateFileCase(char *Path);
	
	FArchive* InternalCreateFileReader( const TCHAR* InFilename, DWORD Flags, FOutputDevice* Error );
	FArchive* InternalCreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error );
	/**
	 * Looks up the size of a file by opening a handle to the file.
	 *
	 * @param	Filename	The path to the file.
	 * @return	The size of the file or -1 if it doesn't exist.
	 */
	virtual INT InternalFileSize( const TCHAR* Filename );
	DWORD InternalCopy( const TCHAR* DestFile, const TCHAR* SrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, FCopyProgress* Progress );
	UBOOL InternalDelete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 );
	UBOOL InternalIsReadOnly( const TCHAR* Filename );
	UBOOL InternalMove( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 );
	UBOOL InternalMakeDirectory( const TCHAR* Path, UBOOL Tree=0 );
	UBOOL InternalDeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 );
	void InternalFindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories );
	DOUBLE InternalGetFileAgeSeconds( const TCHAR* Filename );
	DOUBLE InternalGetFileTimestamp( const TCHAR* Filename );
	UBOOL InternalGetTimestamp( const TCHAR* Filename, FTimeStamp& Timestamp );
	void InternalGenerateTOC(int Handle, TMap<FName, AndroidTOCEntry> &TOCMap);
	void InternalGenerateTOC(AAsset* Asset, TMap<FName, AndroidTOCEntry> &TOCMap);
	
	/**
	 * Updates the modification time of the file on disk to right now, just like the unix touch command
	 * @param Filename Path to the file to touch
	 * @return TRUE if successful
	 */
	UBOOL InternalTouchFile(const TCHAR* Filename);
	
	/** Android sandboxed paths for the root of the read-only Application bundle directory */
	static FString AppDir;
	/** Android sandboxed paths for the writable documents directory */
	static FString DocDir;
	
	/** Android map of filenames to offsets and sizes for obb file */
	static TMap<FName, AndroidTOCEntry> MainTOCMap;
	static TMap<FName, AndroidTOCEntry> PatchTOCMap;

	/** The socket used to talk to the file server to download files */
	static class FSocket* FileServer;

	/** The path to the main data file */
	static FString MainPath;

	/** The path to the patch datafile */
	static FString PatchPath;

	/** Cache the paths for directory lookup */
	static TArray<AndroidTOCLookup> PathLookup;

	/** Whether or not the FileManager should load from installed packages */
	UBOOL bIsRunningInstalled;

	/** Whether or not the FileManager should load from internally in the APK */
	UBOOL bIsExpansionInAPK;

	/** Helper struct for using the offline created TOC file */
	FTableOfContents TOC;
};

#endif
