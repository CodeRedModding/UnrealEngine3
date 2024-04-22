/**********************************************************************

Filename    :   ScaleformFile.cpp
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

#include "GFxUI.h"

#if WITH_GFx


#include "ScaleformFile.h"

/** Default constructor */
FGFxFile::FGFxFile()
	:	Buffer ( NULL ),
	    Length ( 0 ),
	    Position ( 0 ),
	    ErrorCode ( 0 )
{
	Filename[0] = 0;
}

/** Constructor with arguments */
FGFxFile::FGFxFile ( const char* const InFilename, UByte* const pBuffer, const int NumBytes )
	:	Buffer ( pBuffer ),
	    Length ( NumBytes ),
	    Position ( 0 ),
	    ErrorCode ( 0 )
{
	Filename[0] = 0;
	check ( pBuffer );

	if ( InFilename )
	{
		check ( 0 < NumBytes );
		const SIZE_T FilenameLength = Min ( MAX_FILENAME_LEN - 2, static_cast<SIZE_T> ( strlen ( InFilename ) ) );
		check ( ( MAX_FILENAME_LEN - 2 ) >= 0 );
		appMemcpy ( & ( Filename[0] ), InFilename, FilenameLength * sizeof ( char ) );
		Filename[ FilenameLength ] = '\0';
	}
}

/** Hook destructor to verify ref counting is working in GFx dll */
FGFxFile::~FGFxFile()
{
}

// ** GFxStream implementation & I/O

/**
 * Blocking write, will write in the given number of bytes to the stream
 * Returns : -1 for error
 *           Otherwise number of bytes read
 */
int FGFxFile::Write ( const UByte *pbufer, int numBytes )
{
	if ( numBytes < 0 || pbufer == NULL )
	{
		return -1;
	}

	if ( Position + numBytes > Length )
	{
		numBytes = Length - Position;
	}

	appMemcpy ( Buffer + Position, pbufer, numBytes );

	Position += numBytes;

	return numBytes;
}

/**
 * Blocking read, will read in the given number of bytes or less from the stream
 * Returns : -1 for error
 *           Otherwise number of bytes read,
 *           if 0 or < numBytes, no more bytes available; end of file or the other side of stream is closed
 */
int FGFxFile::Read ( UByte *pbufer, int numBytes )
{
	if ( numBytes < 0 )
	{
		return -1;
	}

	if ( Position + numBytes > Length )
	{
		numBytes = Length - Position;
	}

	appMemcpy ( pbufer, Buffer + Position, numBytes );

	Position += numBytes;

	return numBytes;
}

/**
 * Skips (ignores) a given # of bytes
 * Same return values as Read
 */
int FGFxFile::SkipBytes ( int numBytes )
{
	if ( numBytes < 0 )
	{
		return -1;
	}

	if ( Position + numBytes > Length )
	{
		numBytes = Length - Position;
	}

	Position += numBytes;

	return numBytes;
}

/**
 * Returns the number of bytes available to read from a stream without blocking
 * For a file, this should generally be number of bytes to the end
 */
int FGFxFile::BytesAvailable()
{
	return Length - Position;
}

/**
 * Causes any implementation's buffered data to be delivered to destination
 * Return 0 for error
 */
bool FGFxFile::Flush()
{
	return 1;
}


// Seeking

/** Returns new position, -1 for error */
int FGFxFile::Seek ( int offset, int origin )
{
	switch ( origin )
	{
		case Seek_Set:
		{
			Position = offset >= Length ? Length - 1 : offset;
			break;
		}

		case Seek_Cur:
		{
			Position = Position + offset >= Length ? Length - 1 : Position + offset;
			break;
		}

		case Seek_End:
		{
			Position = offset >= Length ? 0 : Length - offset - 1;
			break;
		}
	}

	return Position;
}

/** Returns new position, -1 for error */
SInt64 FGFxFile::LSeek ( SInt64 offset, int origin )
{
	return Seek ( ( int ) offset, origin );
}

// Resizing the file

/** Return 0 for failure */
bool FGFxFile::ChangeSize ( int newSize )
{
	return true;
}

/**
 * Appends other file data from a stream
 * Return -1 for error, else # of bytes written
 */
int FGFxFile::CopyFromStream ( File *pstream, int byteSize )
{
	return 0;
}

/**
 * Closes the file
 * After close, file cannot be accessed
 */
bool FGFxFile::Close()
{
	return true;
}


#endif // WITH_GFx