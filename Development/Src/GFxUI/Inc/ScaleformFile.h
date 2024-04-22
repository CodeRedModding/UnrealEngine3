/**********************************************************************

Filename    :   ScaleformFile.h
Content     :   Implements FGFxFile used to access package data

Copyright   :   (c) 2006-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

#ifndef ScaleformFile_h
#define ScaleformFile_h

#if WITH_GFx

#include "ScaleformEngine.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "Kernel/SF_File.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

class FGFxFile : public Scaleform::File
{
	public:

		static const SIZE_T MAX_FILENAME_LEN = 64;

	public:

		/** Default constructor */
		FGFxFile();

		/** Constructor with arguments */
		FGFxFile ( const char* const InFilename, UByte* const pBuffer, const int NumBytes );

		/** Hook destructor to verify ref counting is working in scaleform dll */
		~FGFxFile();

		/**
		 * Returns a file name path relative to the 'reference' directory
		 * This is often a path that was used to create a file
		 * (this is not a global path, global path can be obtained with help of directory)
		 */
		virtual const char* GetFilePath()
		{
			return Filename;
		}

		// ** File Information

		/** Return 1 if file's usable (open) */
		virtual bool        IsValid()
		{
			return Buffer != NULL ? true : false;
		}
		/** Return 1 if file's writable, otherwise 0 */
		virtual bool        IsWritable()
		{
			/* @todo - should do proper validation.*/ return IsValid();
		}

		/** Return position */
		virtual int         Tell ()
		{
			return Position;
		}
		virtual SInt64      LTell ()
		{
			return Position;
		}

		/** File size */
		virtual int         GetLength ()
		{
			return Length;
		}
		virtual SInt64      LGetLength ()
		{
			return Length;
		}

		/**
		 * Return errno-based error code
		 * Useful if any other function failed
		 */
		virtual int         GetErrorCode()
		{
			return ErrorCode;
		}


		// ** GFxStream implementation & I/O

		/**
		 * Blocking write, will write in the given number of bytes to the stream
		 * Returns : -1 for error
		 *           Otherwise number of bytes read
		 */
		virtual int         Write ( const UByte *pbufer, int numBytes );

		/**
		 * Blocking read, will read in the given number of bytes or less from the stream
		 * Returns : -1 for error
		 *           Otherwise number of bytes read,
		 *           if 0 or < numBytes, no more bytes available; end of file or the other side of stream is closed
		 */
		virtual int        Read ( UByte *pbufer, int numBytes );

		/**
		 * Skips (ignores) a given # of bytes
		 * Same return values as Read
		 */
		virtual int        SkipBytes ( int numBytes );

		/**
		 * Returns the number of bytes available to read from a stream without blocking
		 * For a file, this should generally be number of bytes to the end
		 */
		virtual int        BytesAvailable();

		/**
		 * Causes any implementation's buffered data to be delivered to destination
		 * Return 0 for error
		 */
		virtual bool        Flush();

		/** Need to provide a more optimized implementation that does not necessarily involve a lot of seeking */
		SF_INLINE bool      IsEOF()
		{
			return !BytesAvailable();
		}

		// Seeking
		/** Returns new position, -1 for error */
		virtual int        Seek ( int offset, int origin = Seek_Set );
		virtual SInt64      LSeek ( SInt64 offset, int origin = Seek_Set );

		/** Seek simplification */
		int                SeekToBegin()
		{
			return Seek ( 0 );
		}
		int                SeekToEnd()
		{
			return Seek ( 0, Seek_End );
		}
		int                Skip ( int numBytes )
		{
			return Seek ( numBytes, Seek_Cur );
		}

		/** Resizing the file, Return 0 for failure */
		virtual bool        ChangeSize ( int newSize );

		/**
		 * Appends other file data from a stream
		 * Return -1 for error, else # of bytes written
		 */
		virtual int        CopyFromStream ( File *pstream, int byteSize );

		/**
		 * Closes the file
		 * After close, file cannot be accessed
		 */
		virtual bool        Close();

	private:
		/** The load file's byte stream. */
		UByte* Buffer;

		/** The number of bytes in the buffer. */
		int Length;

		/** Position in buffer */
		int Position;

		/** The name of the file that the data contained in the class originates from. */
		char Filename[MAX_FILENAME_LEN];

		int ErrorCode;
};

#endif //WITH_GFx

#endif // ScaleformFile_h
