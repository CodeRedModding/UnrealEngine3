/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#pragma once

using namespace System;

namespace ConsoleInterface
{
	ref class FileGroup;

	public ref class TOCInfo
	{
	public:
		// The full path name of the file
		String ^FileName;
		// The md5 checksum of the file
		String ^CRC;
		// The timestamp of the last write time
		DateTime LastWriteTime;
		// The size in bytes
		int Size;
		// The fully compressed size
		int CompressedSize;
		// Whether to sync this file when copying to the console
		bool bIsForSync;
		// Whether to add this file to the TOC
		bool bIsForTOC;
		// Whether this file is a TOC file and should be copied last
		bool bIsTOC;
		// Whether to move this file to the root folder when copying to the console
		bool bDeploy;

	public:
		TOCInfo( String ^InFileName, String ^Hash, DateTime LastWrite, int SizeInBytes, int CompressedSizeInBytes, FileGroup^ Group );
	};
}
