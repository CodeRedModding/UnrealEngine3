/*=============================================================================
	FTableOfContents.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"
#include "FTableOfContents.h"


/**
 * Constructor
 */
FTableOfContents::FTableOfContents()
{
}

/**
 * Parse the table of contents from a given buffer
 *
 * @param Buffer Buffer of text representing a TOC, zero terminated
 *
 * @return TRUE if successful
 */
UBOOL FTableOfContents::ParseFromBuffer(FString& Buffer, UBOOL bIsMasterTOC)
{
	// protect TOC access 
	FScopeLock ScopeLock(&TOCCriticalSection);

	Buffer.ReplaceInline( TEXT( "\r" ), TEXT( "\n" ) );
	TArray<FString> TOCEntries;
	Buffer.ParseIntoArray( &TOCEntries, TEXT( "\n" ), TRUE );

	for( INT TocIndex = 0; TocIndex < TOCEntries.Num(); TocIndex++ )
	{
		TArray<FString> TOCEntry;
		INT Count = TOCEntries( TocIndex ).ParseIntoArrayWS( &TOCEntry );
		if( Count == 4 )
		{
			INT FileSize = appAtoi( *TOCEntry( 0 ) );
			INT UncompressedFileSize = appAtoi( *TOCEntry( 1 ) );

			// Don't allow overwriting existing entries.
			// This assumes ALL languages will be generated from the same content!!!
			FTOCEntry* ExistingEntry = Entries.Find( TOCEntry( 2 ) );
			if( ExistingEntry == NULL )
			{
				// insert this entry into the map
				FTOCEntry& Entry = Entries.Set( TOCEntry( 2 ), FTOCEntry() );
				Entry.FileSize = FileSize;
				Entry.UncompressedFileSize = UncompressedFileSize;
			}
		}
	}

	// mark it as initialzied
	if (bIsMasterTOC)
	{
		bIsInitialized = TRUE;
	}

	return TRUE;
}


/**
 * Return the size of a file in the TOC
 * 
 * @param Filename Name of file to look up
 *
 * @return Size of file, or -1 if the filename was not found
 */
INT FTableOfContents::GetFileSize(const TCHAR* Filename)
{
	// protect TOC access so loading thread can use it
	FScopeLock ScopeLock(&TOCCriticalSection);

	//default to not found
	INT FileSize = -1;

	// lookup the file in the TOC
	FTOCEntry* Entry = Entries.Find(Filename);

	// if the file doesn't exist, return -1
	if (Entry)
	{
		// if the file was compressed, then return that size
		FileSize = Entry->FileSize;
	}

	return FileSize;
}


/**
 * Return the uncompressed size of a file in the TOC
 * 
 * @param Filename Name of file to look up
 *
 * @return Size of file, or -1 if the filename was not found
 */
INT FTableOfContents::GetUncompressedFileSize(const TCHAR* Filename)
{
	// protect TOC access so loading thread can use it
	FScopeLock ScopeLock(&TOCCriticalSection);

	//default to not found
	INT FileSize = -1;

	// lookup the file in the TOC
	FTOCEntry* Entry = Entries.Find(Filename);

	// if the file doesn't exist or wasn't compressed, return -1
	if (Entry && Entry->UncompressedFileSize != 0)
	{
		// if the file was compressed, then return that size
		FileSize = Entry->UncompressedFileSize;
	}

	return FileSize;
}

/**
 * Finds files in the TOC based on a wildcard
 *
 * @param Result Resulting filename strings
 * @param Wildcard Wildcard to match against
 * @param bShouldFindFiles If TRUE, return files in Result
 * @param bShouldFindDirectories If TRUE, return directories in Result
 */
void FTableOfContents::FindFiles( TArray<FString>& Result, const TCHAR* Wildcard, UBOOL bShouldFindFiles, UBOOL bShouldFindDirectories )
{
	// protect TOC access so loading thread can use it
	FScopeLock ScopeLock( &TOCCriticalSection );

	// cache a ffilename version
	FFilename FullWildcard( Wildcard );
	// get the part of the wildcard after last slash, before . (like '*' in ..\Config\*.ini)
	FString FileWildcard = FullWildcard.GetBaseFilename();

	// Only full wildcards allowed
	if( FileWildcard != TEXT( "*" ) )
	{
		return;
	}

	// get the start of the path
	FFilename BasePath = FullWildcard.GetPath() + TEXT( "\\" );

	// Must be of the form ..\GearGame\CookedXbox360\*.xxx
	if( BasePath.Left( 2 ) != TEXT( ".." ) )
	{
		return;
	}

	// get the part after the .
	FString Ext = FullWildcard.GetExtension();
	// are we doing *.*?
	UBOOL bMatchAnyExtension = ( Ext == TEXT( "*" ) );

	// get the length of the string to match
	INT BasePathLen = BasePath.Len();
	
	// go through the TOC looking for matches
	for( TMap<FFilename, FTOCEntry>::TConstIterator It( Entries ); It; ++It )
	{
		const FFilename& Filename = It.Key();

		// if the start of the TOC string matches the start of the wildcard search
		if( appStrnicmp( *Filename, *BasePath, BasePathLen ) == 0 )
		{
			FFilename PotentialSubFolderName = Filename.Right( Filename.Len() - BasePathLen );
			FFilename SubFolderName = PotentialSubFolderName.GetPath();

			// If we are a subfolder, add to the list of subfolders if we're looking for folders
			if( SubFolderName.Len() > 0 )
			{
				if( bShouldFindDirectories )
				{
					Result.AddUniqueItem( SubFolderName.GetCleanFilename() );
				}
			}
			else
			{
				// If we're a file, add to the list of files if we're looking for files
				if( bShouldFindFiles )
				{
					// path matches, how about extension (since we know the filename without extension is using *)
					if( bMatchAnyExtension || Ext == Filename.GetExtension() )
					{
						// we have a match!
						Result.AddUniqueItem( Filename.GetCleanFilename() );
					}
				}
			}
		}
	}
}

/**
 * Add a file to the TOC at runtime
 * 
 * @param Filename Name of the file
 * @param FileSize Size of the file
 * @param UncompressedFileSize Size of the file when uncompressed, defaults to 0
 * @param StartSector Sector on disk for the file, defaults to 0
 */
void FTableOfContents::AddEntry(const TCHAR* Filename, INT FileSize, INT UncompressedFileSize)
{
	// insert this entry into the map
	FTOCEntry& Entry = Entries.Set(Filename, FTOCEntry());
	Entry.FileSize = FileSize;
	Entry.UncompressedFileSize = UncompressedFileSize;
}
