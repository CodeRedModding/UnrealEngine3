/*=============================================================================
	FTableOfContents.h
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _TABLE_OF_CONTENTS_INC__
#define _TABLE_OF_CONTENTS_INC__


class FTableOfContents
{
public:

	/**
	 * Constructor
	 */
	FTableOfContents();

	/**
	 * @return if the TOC has been initialized with any files
	 */
	UBOOL HasBeenInitialized() const
	{
		return bIsInitialized;
	}

	/**
	 * Parse the table of contents from a given buffer
	 *
	 * @param Buffer Buffer of text representing a TOC, zero terminated
	 * @param bIsMasterTOC True if this the main TOC file, when it is done being read in, the TOC will be considered initialized
	 *
	 * @return TRUE if successful
	 */
	UBOOL ParseFromBuffer(FString& Buffer, UBOOL bIsMasterTOC);

	/**
	 * Return the size of a file in the TOC
	 * 
	 * @param Filename Name of file to look up
	 *
	 * @return Size of file, or -1 if the filename was not found
	 */
	INT GetFileSize(const TCHAR* Filename);

	/**
	 * Return the uncompressed size of a file in the TOC
	 * 
	 * @param Filename Name of file to look up
	 *
	 * @return Size of file, or -1 if the filename was not found
	 */
	INT GetUncompressedFileSize(const TCHAR* Filename);

	/**
	 * Finds files in the TOC based on a wildcard
	 *
	 * @param Result Resulting filename strings
	 * @param Wildcard Wildcard to match against
	 * @param bShouldFindFiles If TRUE, return files in Result
	 * @param bShouldFindDirectories If TRUE, return directories in Result
	 */
	void FindFiles(TArray<FString>& Result, const TCHAR* Wildcard, UBOOL bShouldFindFiles, UBOOL bShouldFindDirectories);


	/**
	 * Add a file to the TOC at runtime
	 * 
	 * @param Filename Name of the file
	 * @param FileSize Size of the file
	 * @param UncompressedFileSize Size of the file when uncompressed, defaults to 0
	 * @param StartSector Sector on disk for the file, defaults to 0
	 */
	void AddEntry(const TCHAR* Filename, INT FileSize, INT UncompressedFileSize=0);

private:
	/**
	 * Helper struct to store compressed and uncompressed filesize
	 */
    struct FTOCEntry
	{
		/** Size on disk (compressed size if it was compressed) */
		INT FileSize;

		/** Uncompressed filesize (size file will take in memory if compressed, 0 if not compressed) */
		INT UncompressedFileSize;
	};

	/** Table of contents of the DVD */
	TMap<FFilename, FTOCEntry> Entries;

	/** Object used for synchronization via a scoped lock */
	FCriticalSection TOCCriticalSection;

	/** Whether or not the master TOC file has finished being read in */
	UBOOL bIsInitialized;

};



#endif // _TABLE_OF_CONTENTS_INC__
