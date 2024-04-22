/*=============================================================================
 	FFileManagerNetwork.h: Network file loader declarations
 	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#ifndef __FFILEMANAGERNETWORK_H__
#define __FFILEMANAGERNETWORK_H__

#include "FTableOfContents.h"
#include "FFileManagerGeneric.h"

#if WITH_UE3_NETWORKING


class FFileManagerNetwork : public FFileManager
{
public:
	/** 
	 * Constructor.
	 */
	FFileManagerNetwork(FFileManager* InUsedManager);

	/**
	 * Initialize the file manager _before_ anything has used the commandline (allowing 
	 * for this function to override the commandline from the file host)
	 */
	virtual void PreInit();
	virtual void Init(UBOOL Startup);

	virtual FArchive* CreateFileReader( const TCHAR* Filename, DWORD ReadFlags, FOutputDevice* Error) ;
	virtual FArchive* CreateFileWriter( const TCHAR* Filename, DWORD WriteFlags, FOutputDevice* Error, INT MaxFileSize );

	// If you're writing to a debug file, you should use CreateDebugFileWriter, and wrap the calling code in #if ALLOW_DEBUG_FILES.
#if ALLOW_DEBUG_FILES
	FArchive* CreateDebugFileWriter(const TCHAR* Filename, DWORD WriteFlags, FOutputDevice* Error, INT MaxFileSize )
	{
		return CreateFileWriter(Filename,WriteFlags,Error,MaxFileSize);
	}
#endif

	/**
	 * If the given file is compressed, this will return the size of the uncompressed file,
	 * if the platform supports it.
	 * @param Filename Name of the file to get information about
	 * @return Uncompressed size if known, otherwise -1
	 */
	virtual INT UncompressedFileSize( const TCHAR* Filename );
	virtual UBOOL IsReadOnly( const TCHAR* Filename );
	virtual UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists, UBOOL EvenReadOnly );
	virtual DWORD Copy( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace, UBOOL EvenIfReadOnly, UBOOL Attributes, FCopyProgress* Progress );
	virtual UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace, UBOOL EvenIfReadOnly, UBOOL Attributes );

	/**
	 * Updates the modification time of the file on disk to right now, just like the unix touch command
	 * @param Filename Path to the file to touch
	 * @return TRUE if successful
	 */
	virtual UBOOL TouchFile(const TCHAR* Filename);

	/**
	 * Creates filenames belonging to the set  "Base####.Extension" where #### is a 4-digit number in [0000-9999] and
	 * no file of that name currently exists.  The return value is the index of first empty filename, or -1 if none
	 * could be found.  Clients that call FindAvailableFilename repeatedly will want to cache the result and pass it
	 * in to the next call thorugh the StartVal argument.  Base and Extension must valid pointers.
	 * Example usage:
	 * \verbatim
	   // Get a free filename of form <appGameDir>/SomeFolder/SomeFilename####.txt
	   FString Output;
	   FindAvailableFilename( *(appGameDir() * TEXT("SomeFolder") * TEXT("SomeFilename")), TEXT("txt"), Output );
	   \enverbatim
	 *
	 * @param	Base			Filename base, optionally including a path.
	 * @param	Extension		File extension.
	 * @param	OutFilename		[out] A free filename (untouched on fail).
	 * @param	StartVal		[opt] Can be used to hint beginning of index search.
	 * @return					The index of the created file, or -1 if no free file with index (StartVal, 9999] was found.
	 */
	virtual INT FindAvailableFilename( const TCHAR* Base, const TCHAR* Extension, FString& OutFilename, INT StartVal );

	virtual UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree );
	virtual UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists, UBOOL Tree );
	virtual void FindFiles( TArray<FString>& FileNames, const TCHAR* Filename, UBOOL Files, UBOOL Directories );
	virtual DOUBLE GetFileAgeSeconds( const TCHAR* Filename );
	virtual DOUBLE GetFileTimestamp( const TCHAR* Filename );
	virtual UBOOL SetDefaultDirectory();
	virtual UBOOL SetCurDirectory( const TCHAR* Directory );
	virtual FString GetCurrentDirectory();
	/** 
	 * Get the timestamp for a file
	 * @param Path Path for file
	 * @TimeStamp Output time stamp
	 * @return success code
	 */
	virtual UBOOL GetTimestamp( const TCHAR* Path, FTimeStamp& TimeStamp );

	/**
	 * Converts passed in filename to use a relative path.
	 *
	 * @param	Filename	filename to convert to use a relative path
	 * 
	 * @return	filename using relative path
	 */
	virtual FString ConvertToRelativePath( const TCHAR* Filename );

	/**
	 * Converts passed in filename to use an absolute path.
	 *
	 * @param	Filename	filename to convert to use an absolute path, safe to pass in already using absolute path
	 * 
	 * @return	filename using absolute path
	 */
	virtual FString ConvertToAbsolutePath( const TCHAR* Filename );
	/**
	 * Converts a path pointing into the installed directory to a path that a least-privileged user can write to 
	 *
	 * @param AbsolutePath Source path to convert
	 *
	 * @return Path to the user directory
	 */
	virtual FString ConvertAbsolutePathToUserPath(const TCHAR* AbsolutePath);

	/**
	 *	Converts the platform-independent Unreal filename into platform-specific full path. (Thread-safe)
	 *
	 *	@param Filename		Platform-independent Unreal filename
	 *	@return				Platform-dependent full filepath
	 **/
	virtual FString GetPlatformFilepath( const TCHAR* Filename );

	/**
	 *	Returns the size of a file. (Thread-safe)
	 *
	 *	@param Filename		Platform-independent Unreal filename.
	 *	@return				File size in bytes or INDEX_NONE if the file didn't exist.
	 **/
	virtual INT FileSize( const TCHAR* Filename );

	/**Enables file system drive addressing where appropriate (360)*/
	virtual void EnableLogging (void) {};

private:

	/**
	 * Copies given UE3 path filename from the network host to the local disk, if 
	 * it is newer on the host
	 *
	 * @param Filename Filename as passed from UE3
	 *
	 * @return TRUE if the file is up to date and usable on the local disk
	 */
	UBOOL EnsureFileIsLocal(const TCHAR* Filename);

	/**
	 * Performs a FindFiles operation over the network to get a list of matching files
	 * from the file serving host
	 *
	 * @param FileNames Output list of files (no paths, just filenames)
	 * @param Wildcard The wildcard used to search for files (..\\*.txt)
	 * @param bFindFiles If TRUE, will look for file in the wildcard
	 * @param bFindDirectories If TRUE, will look for directories in the wildcard
	 *
	 * @return TRUE if the operation succeeded (no matter how many files were found)
	 */
	UBOOL RemoteFindFiles(TArray<FString>& FileNames, const TCHAR* Wildcard, UBOOL bFindFiles, UBOOL bFindDirectories);

	/**
	 * Gets the size of a file over the network, before copying it locally
	 * 
	 * @param Filename File to get size of
	 * @param bGetUncompressedSize If TRUE, returns the size the file will be when uncompressed, if it's a fully compressed file
	 * 
	 * @param Size of the file, or -1 on failure or if the file doesn't exist
	 */
	INT RemoteFileSize(const TCHAR* Filename, UBOOL bGetUncompressedSize);

	/** The actual file manager used to read/write files from local disk */
	FFileManager* UsedManager;

	/** The socket used to read data from the host */
	FSocket* FileSocket;
	
	/** Only one thread may talk to server at a time */
	FCriticalSection NetworkCriticalSection;

	/** Only process a particular file one time */
	TSet<FString> AlreadyCopiedFiles;
	TMap<FString, INT> AlreadySizedFiles;
	TMap<FString, INT> AlreadyUncompressedSizedFiles;
};

#endif

#endif