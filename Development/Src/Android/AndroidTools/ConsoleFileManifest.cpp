/*=============================================================================
	ConsoleFileManifest - Manifest of file names, sizes, time stamps made during a cook for Android.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "Common.h"
#include "ConsoleFileManifest.h"
#include <fstream>
#include <io.h>
#include <vcclr.h>

#define LOCAL_MANIFEST_FILE_NAME "../Binaries/Android/LocalFileSyncManifest.txt"
#define REMOTE_MANIFEST_FILE_NAME "FileSyncManifest.txt"

#define CONSOLE_MANIFEST_VERSION "2"

#define MAX_INPUT_BUFFER 1024

//forward declarations
extern bool RunChildProcess(const char* CommandLine, char* Errors, int ErrorsSize, DWORD* ProcessReturnValue, int Timeout);

/**
 * Helper function to get timestamp and size
 */
void GetFileAttribs(const wchar_t* InFilename, QWORD& OutTimeStamp, QWORD& OutSize)
{
	//get file size
	WIN32_FILE_ATTRIBUTE_DATA LocalAttrs;
	if(GetFileAttributesExW(InFilename, GetFileExInfoStandard, &LocalAttrs) == 0)
	{
		//should never happen...
	}

	OutTimeStamp = ( ( ( QWORD )LocalAttrs.ftLastWriteTime.dwHighDateTime ) << 32 ) + LocalAttrs.ftLastWriteTime.dwLowDateTime;
	OutSize = ( ( ( QWORD )LocalAttrs.nFileSizeHigh ) << 32 ) + LocalAttrs.nFileSizeLow;
}

//default constructor
FConsoleFileManifest::FConsoleFileManifest()
{

}

/**
 * Reads the manifest from the device and parses the data within
 * If there is no manifest, or the file was made from a different machine, the entire manifest is emptied
 * Emptying will result in a full copy
 */
void FConsoleFileManifest::PullFromDevice(const char* AndroidRoot)
{
	char Errors[1024];
	char CommandLine[1024];
	DWORD RetVal;

	// Retry on failure, unless the file wasn't there anyway.
	DWORD DeleteError = 0;
	INT DeleteResult = DeleteFileA(LOCAL_MANIFEST_FILE_NAME) != 0;
	if(DeleteResult == 0 && ( DeleteError = GetLastError() ) != ERROR_FILE_NOT_FOUND )
	{
		// Wait just a little bit (i.e. a totally arbitrary amount)...
		Sleep(500);

		// Try again
		DeleteResult = DeleteFileA(LOCAL_MANIFEST_FILE_NAME);
		if(DeleteResult == 0)
		{
			DeleteError = GetLastError();
			sprintf_s(CommandLine, "Error deleting file '%s' (GetLastError: %d)", LOCAL_MANIFEST_FILE_NAME, DeleteError);
			::MessageBoxA(NULL, CommandLine, "Failed Intermediate Manifest Deletion", MB_OK);
		}
	}

	sprintf_s(CommandLine, "%s\\android-sdk-windows\\platform-tools\\adb.exe pull /sdcard/UnrealEngine3/%s %s", 
		AndroidRoot, REMOTE_MANIFEST_FILE_NAME, LOCAL_MANIFEST_FILE_NAME);
	RunChildProcess(CommandLine, Errors, 1023, &RetVal, 0);

	ReadManifestFromLocalFile();
}

/**
 * Packages up the manifest in memory and pushes it to the target device
 */
void FConsoleFileManifest::Push(const char* AndroidRoot)
{
	char Errors[1024];
	char CommandLine[1024];
	DWORD RetVal;

	WriteManifestToLocalFile();

	//const wchar_t* TargetName = GetTargetName(0);
	sprintf_s(CommandLine, "%s\\android-sdk-windows\\platform-tools\\adb.exe push %s /sdcard/UnrealEngine3/%s", 
		AndroidRoot, LOCAL_MANIFEST_FILE_NAME, REMOTE_MANIFEST_FILE_NAME);
	RunChildProcess(CommandLine, Errors, 1023, &RetVal, 0);
}

/**
 * Returns true if the file is not considered to be up-to-date or on the device at all
 * @param InFileName - The name of the file to be considered for copying
 */
int FConsoleFileManifest::ShouldCopyFile(const wchar_t* InFilename)
{
	bool bShouldCopy = true;
	QWORD Size = 0;
	QWORD TimeStamp;

	// See if the filename already exist in the manifest
	for (UINT EntryIndex = 0; EntryIndex < ManifestEntries.size(); ++EntryIndex)
	{
		ManifestEntry& Entry = ManifestEntries[EntryIndex];
		if (Entry.Name == InFilename)
		{
			GetFileAttribs(InFilename, TimeStamp, Size);
			bShouldCopy = ((TimeStamp != Entry.TimeStamp) || (Size != Entry.Size));
			// We have a match and we know if we should copy or not
			break;
		}
	}

	if( bShouldCopy )
	{
		// Get the size of the file if it isn't in the manifest
		if( Size == 0 )
		{
			GetFileAttribs(InFilename, TimeStamp, Size);
		}

		// Best guess at a timeout value - 15 seconds plus the size in MB as seconds
		return ( int )( 15 + ( Size >> 20 ) );
	}
	
	// Do not copy
	return 0;
}

/**
 * Updates the file's representation in the manifest, adding or amending as needed
 */
void FConsoleFileManifest::UpdateFileStatusInLog(const wchar_t* InFilename)
{
	//see if the filename already exist in the manifest
	UINT EntryIndex;
	for (EntryIndex = 0; EntryIndex < ManifestEntries.size(); ++EntryIndex)
	{
		if (ManifestEntries[EntryIndex].Name == InFilename)
		{
			break;
		}
	}

	//if it doesn't exist, add it
	if (EntryIndex == ManifestEntries.size())
	{
		ManifestEntries.push_back(ManifestEntry());
	}
	
	ManifestEntry &Entry = ManifestEntries[EntryIndex];
	//file name
	Entry.Name = InFilename;

	GetFileAttribs(InFilename, Entry.TimeStamp, Entry.Size);
}

/**
 * Reads the local manifest file into our internal format.
 */
void FConsoleFileManifest::ReadManifestFromLocalFile()
{
	//Open file
	wifstream ManifestFile;
	wstring InputBuffer;

	//If file does not exist, leave the manifest empty
	ManifestFile.open(LOCAL_MANIFEST_FILE_NAME);
	if (!ManifestFile.is_open())
	{
		return;
	}

	//Read the version number (for format change comparisons)
	ManifestFile >> InputBuffer;
	//if the version number of the manifest does not match, leave the manifest empty
	if (InputBuffer != ToWString(CONSOLE_MANIFEST_VERSION))
	{
		return;
	}

	//Read machine name that wrote the manifest file
	ManifestFile >> InputBuffer;
	pin_ptr<const wchar_t> CLRMachineName = PtrToStringChars(System::String::Concat(L"UE3-", System::Environment::MachineName));
	wstring MachineName = CLRMachineName;
	//if machine name is different, leave the manifest empty
	if (InputBuffer != MachineName)
	{
		return;
	}

	//Read the number of files listed in the manifest
	INT NumberManifestFiles;
	ManifestFile >> NumberManifestFiles;
	//allocated enough room for that many files
	ManifestEntries.resize(NumberManifestFiles);

	//Read all the individual files
	for (UINT i = 0; i < ManifestEntries.size(); ++i)
	{
		ManifestEntry &Entry = ManifestEntries[i];
		//read file name
		ManifestFile >> Entry.Name;

		//read time stamp
		ManifestFile >> Entry.TimeStamp;

		//read file size
		ManifestFile >> Entry.Size;
	}

	//close manifest
	ManifestFile.close();
}


/**
 * Writes the local manifest file from our internal format.
 */
void FConsoleFileManifest::WriteManifestToLocalFile()
{
	wofstream ManifestFile;
	wstring OutputBuffer;

	//Open file for writing
	ManifestFile.open(LOCAL_MANIFEST_FILE_NAME);
	if (!ManifestFile.is_open())
	{
		return;
	}

	//Write the version number
	ManifestFile << ToWString(CONSOLE_MANIFEST_VERSION) << endl;

	//Write machine name writing the manifest
	pin_ptr<const wchar_t> CLRMachineName = PtrToStringChars(System::String::Concat(L"UE3-", System::Environment::MachineName));
	wstring MachineName = CLRMachineName;
	ManifestFile << MachineName << endl;

	//Write the number of files in the manifest
	ManifestFile << ManifestEntries.size() << endl;

	//Write all files info to the manifest
	for (UINT i = 0; i < ManifestEntries.size(); ++i)
	{
		ManifestEntry &Entry = ManifestEntries[i];
		//write file name
		ManifestFile << Entry.Name << endl;

		//write time stamp
		ManifestFile << Entry.TimeStamp << endl;

		//write file size
		ManifestFile << Entry.Size << endl;
	}

	//close manifest
	ManifestFile.close();
}

