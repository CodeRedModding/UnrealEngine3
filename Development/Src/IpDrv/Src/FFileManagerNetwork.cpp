/*=============================================================================
 FFileManagerNetwork.h: Network file loader declarations
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "Engine.h"
#include "UnIpDrv.h"
#include "FFileManagerNetwork.h"

#if WITH_UE3_NETWORKING

/** Port used to communicate to host */
#define FILE_SERVING_PORT 41899

/** Temp variables for tracking network overhead - important while this code is being worked on */
DOUBLE DEBUG_NetworkFileTimeCopyOverhead = 0.0;
DOUBLE DEBUG_NetworkFileTimeFindOverhead = 0.0;
DOUBLE DEBUG_NetworkFileTimeSizeOverhead = 0.0;


/** 
 * Constructor.
 */
FFileManagerNetwork::FFileManagerNetwork(FFileManager* InUsedManager)
	: UsedManager(InUsedManager)
	, FileSocket(NULL)
{
	
};

/**
 * Initialize the file manager _before_ anything has used the commandline (allowing 
 * for this function to override the commandline from the file host)
 */
void FFileManagerNetwork::PreInit()
{
	UsedManager->PreInit();

	// open the socket
	if (GSocketSubsystem == NULL)
	{
		appOutputDebugString(TEXT("Sockets need to be initialized before file reading can start from the network. Check your platform's appSocketInit() for the bEarlyInit case and make that work."));
	}
	else
	{

		// default to no IP found
		UBOOL bFoundValidIP = FALSE;

		// get the IP address of the file serving host (ie UFE)
		FString HostIP;
		FInternetIpAddr HostAddr;

		// make sure we are in the directory we expect to start up in, before doing any file operations
		SetDefaultDirectory();

		// if the file host was specified on the commandline, just use it
		if (Parse(appCmdLine(), TEXT("FileHostIP="), HostIP))
		{
			// turn it into an IpAddr
			HostAddr.SetIp(*HostIP, bFoundValidIP);
		}
		// otherwise look in the cooked directory for what UFE may have spit out
		else
		{
			// make a path to the file containing the ip address
			FString HostFilename = FString::Printf(TEXT("%sCooked%s\\UE3NetworkFileHost.txt"),
				*appGameDir(), *appGetPlatformString());

			FString Contents;
			// try to read in the file
			if (appLoadFileToString(Contents, *HostFilename, UsedManager))
			{
				// use the IP address from the file
				HostAddr.SetIp(*Contents, bFoundValidIP);
			}
		}

		// do we have a good IP address?
		if (bFoundValidIP)
		{
			HostAddr.SetPort(FILE_SERVING_PORT);

			// create a socket
			FileSocket = GSocketSubsystem->CreateStreamSocket(TEXT("Networked File Reader Socket"));
			check(FileSocket);

			// connect to server
			if (FileSocket->Connect(HostAddr))
			{
				appOutputDebugStringf(TEXT("Connected to file serving host at %s!\n"), *HostAddr.ToString(TRUE));
			}
			else
			{
				appOutputDebugStringf(TEXT("Failed to connect to file serving host at %s, will not perform any network operations\n"), *HostAddr.ToString(TRUE));

				// kill the opened socket
				GSocketSubsystem->DestroySocket(FileSocket);
				FileSocket = NULL;
			}

			// allow the platform to recreate the commandline so that it can read the commandline
			// over the network (and also potentially use argv)
			TCHAR* NewCommandLine = new TCHAR[16384];
			if (appResetCommandLine(NewCommandLine))
			{
				debugf(TEXT("Applied new commandline [%s]"), NewCommandLine);

				// apply the new commandline
				appSetCommandline(NewCommandLine);
			}
			delete [] NewCommandLine;
		}
		else
		{
			appOutputDebugStringf(TEXT("Failed to find IP address on the command line or in UE3NetworkFileHost.txt, will not perform any network operations\n"));
		}
	}

	// if the network failed to start up, then make GFileManager be the UsedManager, so 
	// that this will be completely bypassed
	if (FileSocket == NULL)
	{
		GFileManager = UsedManager;
	}
}

void FFileManagerNetwork::Init(UBOOL Startup)
{
	UsedManager->Init(Startup);
}

/**
 * Copies given UE3 path filename from the network host to the local disk, if 
 * it is newer on the host
 *
 * @param Filename Filename as passed from UE3
 *
 * @return TRUE if the file is up to date and usable on the local disk
 */
UBOOL FFileManagerNetwork::EnsureFileIsLocal(const TCHAR* Filename)
{
	FScopeLock ScopeLock(&NetworkCriticalSection);

	// fail all network operations of the network is disabled
	if (FileSocket == NULL)
	{
		return FALSE;
	}
	
	DEBUG_NetworkFileTimeCopyOverhead -= appSeconds();

	// was this file already processed?
	UBOOL bWasAlreadyProcessed = AlreadyCopiedFiles.Contains(Filename);
	// if it was there before, then do nothing
	if (bWasAlreadyProcessed)
	{
		DEBUG_NetworkFileTimeCopyOverhead += appSeconds();
		return TRUE;
	}

	// add the filename to the set of filenames that have been processed,
	AlreadyCopiedFiles.Add(Filename);

	INT BytesSent, BytesRead;

	// send the command
	INT Command = 0;
	FileSocket->Send((BYTE*)&Command, sizeof(Command), BytesSent);

	// send the server the filename
	INT FilenameLen = appStrlen(Filename);
	FileSocket->Send((BYTE*)&FilenameLen, sizeof(FilenameLen), BytesSent);
	FileSocket->Send((BYTE*)TCHAR_TO_ANSI(Filename), FilenameLen, BytesSent);

	// receive host mod time
	SQWORD HostModTime;
	FileSocket->Recv((BYTE*)&HostModTime, sizeof(HostModTime), BytesRead);

	// if it doesn't exist on the host (-1 for mod time), then fail
	if (HostModTime == -1)
	{
		DEBUG_NetworkFileTimeCopyOverhead += appSeconds();
		return FALSE;
	}

	// decide if we want to copy it 
	INT bRequestFile = 0;

	// look for a __time file on the local drive
	FString TimeFilename = FString(Filename) + TEXT("__time");

	// we open the file inside the verification steps below
	FArchive* Writer = NULL;

	FArchive* TimeFile = UsedManager->CreateFileReader(*TimeFilename);
	if (!TimeFile)
	{
		// if the time file doesn't exist, cache locally
		bRequestFile = 1;
	}
	else
	{
		// otherwise, read the last updated file time from host
		SQWORD LocalModTime;
		TimeFile->Serialize(&LocalModTime, sizeof(LocalModTime));

		delete TimeFile;

		// if host is newer, then we need to cache it!
		if (HostModTime > LocalModTime)
		{
			// checks have all finished, we need the file!
			bRequestFile = 1;
		}
	}

	// make sure we can write the file before telling host we need if
	if (bRequestFile)
	{
		Writer = UsedManager->CreateFileWriter(Filename);

		// if we couldn't write the file, fail
		if (!Writer)
		{
			appOutputDebugStringf(TEXT("Failed to create caching writer for %s"), Filename);
			bRequestFile = 0;
		}
	}

	// tell the host what we want to do
	FileSocket->Send((BYTE*)&bRequestFile, sizeof(bRequestFile), BytesSent);

	// only expect something back if we asked for it
	if (bRequestFile)
	{
		appOutputDebugStringf(TEXT("Copying file %s from file host to local drive...\n"), Filename);

		// get the file size
		INT FileSize;
		FileSocket->Recv((BYTE*)&FileSize, sizeof(FileSize), BytesRead); 
		check(BytesRead == sizeof(FileSize));

		// receive the file contents
		BYTE* Buffer = (BYTE*)appMalloc(1024 * 1024);

		INT AmountLeft = FileSize;
		while (AmountLeft > 0)
		{
			// figure out how much to send
			INT AmountToRead = Min(AmountLeft, 1024 * 1024);

			// read/write it
			FileSocket->Recv(Buffer, AmountToRead, BytesRead);
			if (BytesRead)
			{
				Writer->Serialize(Buffer, BytesRead);
			}

			// update counter
			AmountLeft -= BytesRead;
		}

		// close the file
		delete Writer;

		// free temp mem
		appFree(Buffer);

		// write out the host mod time for checking next run
		TimeFile = UsedManager->CreateFileWriter(*TimeFilename);
		check(TimeFile);

		TimeFile->Serialize(&HostModTime, sizeof(HostModTime));
		delete TimeFile;
	}

	DEBUG_NetworkFileTimeCopyOverhead += appSeconds();

	return TRUE;
}

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
UBOOL FFileManagerNetwork::RemoteFindFiles(TArray<FString>& FileNames, const TCHAR* Wildcard, UBOOL bFindFiles, UBOOL bFindDirectories)
{
	
	FScopeLock ScopeLock(&NetworkCriticalSection);

	// fail all network operations of the network is disabled
	if (FileSocket == NULL)
	{
		return FALSE;
	}

	DEBUG_NetworkFileTimeFindOverhead -= appSeconds();

	INT BytesSent, BytesRead;

	// send the Command, Params, and Wildcard length
	INT SendData[3];
	SendData[0] = 1; // Command
	SendData[1] = (bFindFiles ? 1 : 0) | (bFindDirectories ? 2 : 0); // Params
	SendData[2] = appStrlen(Wildcard); // Wildcard length
	FileSocket->Send((BYTE*)SendData, sizeof(SendData), BytesSent);

	// now send the wildcard
	FileSocket->Send((BYTE*)TCHAR_TO_ANSI(Wildcard), SendData[2], BytesSent);

	// get the number of files found
	INT NumResults;
	FileSocket->Recv((BYTE*)&NumResults, sizeof(NumResults), BytesRead); 
	check(BytesRead == sizeof(NumResults));

	// receive each filename
	for (INT Result = 0; Result < NumResults; Result++)
	{
		// get the File Size, Uncompressed FileSize, and length of filename
		INT RecvData[3];
		FileSocket->Recv((BYTE*)RecvData, sizeof(RecvData), BytesRead);
		
		INT FileSize = RecvData[0];
		INT UncompressedFileSize = RecvData[1];
		INT FilenameLen = RecvData[2];

		// get the length of the return filename
		ANSICHAR* Buffer = (ANSICHAR*)appMalloc(FilenameLen + 1);

		// get the filename and add it to the list
		FileSocket->Recv((BYTE*)Buffer, FilenameLen, BytesRead);
		Buffer[FilenameLen] = 0;

		FString* FoundFile = new(FileNames) FString(Buffer);

		appFree(Buffer);

		// and cache the file sizes of each file, so that a subsequent FileSize is not needed (reduces overhead)
		if (bFindFiles)
		{
			// cache the file sizes in the local maps
			FFilename Path = FFilename(Wildcard).GetPath();
			AlreadySizedFiles.Set(Path * (*FoundFile), FileSize);
			AlreadyUncompressedSizedFiles.Set(Path * (*FoundFile), UncompressedFileSize);
		}
	}

	DEBUG_NetworkFileTimeFindOverhead += appSeconds();
	return TRUE;
}

/**
 * Gets the size of a file over the network, before copying it locally
 * 
 * @param Filename File to get size of
 * @param bGetUncompressedSize If TRUE, returns the size the file will be when uncompressed, if it's a fully compressed file
 * 
 * @param Size of the file, or -1 on failure or if the file doesn't exist
 */
INT FFileManagerNetwork::RemoteFileSize(const TCHAR* Filename, UBOOL bGetUncompressedSize)
{
	FScopeLock ScopeLock(&NetworkCriticalSection);

	// fail all network operations of the network is disabled
	if (FileSocket == NULL)
	{
		return -1;
	}

	DEBUG_NetworkFileTimeSizeOverhead -= appSeconds();

	// was this file already processed?
	if (bGetUncompressedSize)
	{
		INT* FileSize = AlreadyUncompressedSizedFiles.Find(Filename);
		// if it was there before, then do nothing
		if (FileSize)
		{
			DEBUG_NetworkFileTimeSizeOverhead += appSeconds();
			return *FileSize;
		}
	}
	else
	{
		INT* FileSize = AlreadySizedFiles.Find(Filename);
		// if it was there before, then do nothing
		if (FileSize)
		{
			DEBUG_NetworkFileTimeSizeOverhead += appSeconds();
			return *FileSize;
		}
	}


	INT BytesSent, BytesRead;

	// send the command
	INT Command = 2;
	FileSocket->Send((BYTE*)&Command, sizeof(Command), BytesSent);

	// send the param
	INT Param = bGetUncompressedSize ? 1 : 0;
	FileSocket->Send((BYTE*)&Param, sizeof(Param), BytesSent);

	// send the server the filename
	INT FilenameLen = appStrlen(Filename);
	FileSocket->Send((BYTE*)&FilenameLen, sizeof(FilenameLen), BytesSent);
	FileSocket->Send((BYTE*)TCHAR_TO_ANSI(Filename), FilenameLen, BytesSent);

	// get the file size
	INT FileSize;
	FileSocket->Recv((BYTE*)&FileSize, sizeof(FileSize), BytesRead);

	// add the filename to the set of filenames that have been processed,
	if (bGetUncompressedSize)
	{
		AlreadyUncompressedSizedFiles.Set(Filename, FileSize);
	}
	else
	{
		AlreadySizedFiles.Set(Filename, FileSize);
	}

	DEBUG_NetworkFileTimeSizeOverhead += appSeconds();

	// return it (it will be -1 if target file doesn't exist)
	return FileSize;
}

FArchive* FFileManagerNetwork::CreateFileReader( const TCHAR* Filename, DWORD ReadFlags, FOutputDevice* Error)
{
	// make sure it's local (if it exists on host)
	EnsureFileIsLocal(Filename);

	// read it now that it's local
	return UsedManager->CreateFileReader(Filename, ReadFlags, Error);
}

FArchive* FFileManagerNetwork::CreateFileWriter( const TCHAR* Filename, DWORD WriteFlags, FOutputDevice* Error, INT MaxFileSize )
{
	// no copy-from-host needed when writing
	// @todo: write back over the network
	return UsedManager->CreateFileWriter(Filename, WriteFlags, Error, MaxFileSize);
}

/**
 * If the given file is compressed, this will return the size of the uncompressed file,
 * if the platform supports it.
 * @param Filename Name of the file to get information about
 * @return Uncompressed size if known, otherwise -1
 */
INT FFileManagerNetwork::UncompressedFileSize( const TCHAR* Filename )
{
	// first look remotely
	INT FileSize = RemoteFileSize(Filename, TRUE);

	// if that fails, look locally
	if (FileSize == -1)
	{
		FileSize = UsedManager->UncompressedFileSize(Filename);
	}

	return FileSize;
}

UBOOL FFileManagerNetwork::IsReadOnly( const TCHAR* Filename )
{
	// copy the file locally and then check if it's read only
	if (EnsureFileIsLocal(Filename))
	{
		return UsedManager->IsReadOnly(Filename);
	}

	return FALSE;
}

UBOOL FFileManagerNetwork::Delete( const TCHAR* Filename, UBOOL RequireExists, UBOOL EvenReadOnly )
{
	// delete local version
	return UsedManager->Delete(Filename, RequireExists, EvenReadOnly);
}

DWORD FFileManagerNetwork::Copy( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace, UBOOL EvenIfReadOnly, UBOOL Attributes, FCopyProgress* Progress )
{
	// attempt to copy Src to local file. Ignore the return value because the file may be local already
	// and EnsureFileIsLocal will think it's a failed copy
	EnsureFileIsLocal(Src);

	// now copy to another local file
	return UsedManager->Copy(Dest, Src, Replace, EvenIfReadOnly, Attributes, Progress);
}

UBOOL FFileManagerNetwork::Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace, UBOOL EvenIfReadOnly, UBOOL Attributes )
{
	// attempt to copy Src to local file. Ignore the return value because the file may be local already
	// and EnsureFileIsLocal will think it's a failed copy
	EnsureFileIsLocal(Src);

	// now move locally
	return UsedManager->Move(Dest, Src, Replace, EvenIfReadOnly, Attributes);
}

/**
 * Updates the modification time of the file on disk to right now, just like the unix touch command
 * @param Filename Path to the file to touch
 * @return TRUE if successful
 */
UBOOL FFileManagerNetwork::TouchFile(const TCHAR* Filename)
{
	// only operate locally
	return UsedManager->TouchFile(Filename);
}

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
INT FFileManagerNetwork::FindAvailableFilename( const TCHAR* Base, const TCHAR* Extension, FString& OutFilename, INT StartVal )
{
	// operate locally (this is used for writing, so just use the local files to test against, ignoring source)
	return UsedManager->FindAvailableFilename(Base, Extension, OutFilename, StartVal);
}

UBOOL FFileManagerNetwork::MakeDirectory( const TCHAR* Path, UBOOL Tree )
{
	// operate locally
	return UsedManager->MakeDirectory(Path, Tree);
}

UBOOL FFileManagerNetwork::DeleteDirectory( const TCHAR* Path, UBOOL RequireExists, UBOOL Tree )
{
	// operate locally
	return UsedManager->DeleteDirectory(Path, RequireExists, Tree);
}

void FFileManagerNetwork::FindFiles( TArray<FString>& FileNames, const TCHAR* Filename, UBOOL Files, UBOOL Directories )
{
	// @todo: Allow file managers that don't have a TOC to look on the host for filenames
	RemoteFindFiles(FileNames, Filename, Files, Directories);

	// operate locally
	UsedManager->FindFiles(FileNames, Filename, Files, Directories);
}

DOUBLE FFileManagerNetwork::GetFileAgeSeconds( const TCHAR* Filename )
{
	EnsureFileIsLocal(Filename);

	return UsedManager->GetFileAgeSeconds(Filename);
}

DOUBLE FFileManagerNetwork::GetFileTimestamp( const TCHAR* Filename )
{
	EnsureFileIsLocal(Filename);
	return UsedManager->GetFileAgeSeconds(Filename);
}

UBOOL FFileManagerNetwork::SetDefaultDirectory()
{
	return UsedManager->SetDefaultDirectory();
}

UBOOL FFileManagerNetwork::SetCurDirectory( const TCHAR* Directory )
{
	return UsedManager->SetCurDirectory(Directory);
}

FString FFileManagerNetwork::GetCurrentDirectory()
{
	return UsedManager->GetCurrentDirectory();
}

/** 
 * Get the timestamp for a file
 * @param Path Path for file
 * @TimeStamp Output time stamp
 * @return success code
 */
UBOOL FFileManagerNetwork::GetTimestamp( const TCHAR* Path, FTimeStamp& TimeStamp )
{
	EnsureFileIsLocal(Path);

	return UsedManager->GetTimestamp(Path, TimeStamp);
}

/**
 * Converts passed in filename to use a relative path.
 *
 * @param	Filename	filename to convert to use a relative path
 * 
 * @return	filename using relative path
 */
FString FFileManagerNetwork::ConvertToRelativePath( const TCHAR* Filename )
{
	return UsedManager->ConvertToRelativePath(Filename);
}

/**
 * Converts passed in filename to use an absolute path.
 *
 * @param	Filename	filename to convert to use an absolute path, safe to pass in already using absolute path
 * 
 * @return	filename using absolute path
 */
FString FFileManagerNetwork::ConvertToAbsolutePath( const TCHAR* Filename )
{
	return UsedManager->ConvertToAbsolutePath(Filename);
}

/**
 * Converts a path pointing into the installed directory to a path that a least-privileged user can write to 
 *
 * @param AbsolutePath Source path to convert
 *
 * @return Path to the user directory
 */
FString FFileManagerNetwork::ConvertAbsolutePathToUserPath(const TCHAR* AbsolutePath)
{
	return UsedManager->ConvertAbsolutePathToUserPath(AbsolutePath);
}

/**
 *	Converts the platform-independent Unreal filename into platform-specific full path. (Thread-safe)
 *
 *	@param Filename		Platform-independent Unreal filename
 *	@return				Platform-dependent full filepath
 **/
FString FFileManagerNetwork::GetPlatformFilepath( const TCHAR* Filename )
{
	return UsedManager->GetPlatformFilepath(Filename);
}

/**
 *	Returns the size of a file. (Thread-safe)
 *
 *	@param Filename		Platform-independent Unreal filename.
 *	@return				File size in bytes or INDEX_NONE if the file didn't exist.
 **/
INT FFileManagerNetwork::FileSize( const TCHAR* Filename )
{
	INT FileSize = RemoteFileSize(Filename, FALSE);

	if (FileSize == -1)
	{
		// operate locally if destination file failed
		FileSize = UsedManager->FileSize(Filename);
	}

	return FileSize;
}

#endif