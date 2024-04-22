/*=============================================================================
	UnJPEG.cpp: Unreal JPEG support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

	Source code for JPEG decompression from:
	http://code.google.com/p/jpeg-compressor/
=============================================================================*/

#include "EnginePrivate.h"
#include "UnJPEG.h"

#if WITH_JPEG
#include "jpge.h"
#include "jpge.cpp"
#include "jpgd.h"
#include "jpgd.cpp"
#endif //WITH_JPEG

/** Only allow one thread to use JPEG decoder at a time (it's not thread safe) */
FCriticalSection GJPEGDecoder; 

/** Only allow one thread to use JPEG encoder at a time (it's not thread safe) */
FCriticalSection GJPEGEncoder; 

/**
 * FDecoderJPEG constructor
 *
 * @param InJPEGData ptr to byte array of JPEG data in memory
 * @param InJPEGDataSize size of JPEG data
 */
FDecoderJPEG::FDecoderJPEG(const BYTE* InJPEGData, DWORD InJPEGDataSize, const INT InNumColors)
	: JPEGData(InJPEGData)
	, JPEGDataSize(InJPEGDataSize)
	, Width(0)
	, Height(0)
	, NumColors(InNumColors)
{
}

/**
 * Starts the decoding process
 *
 * @return results of the decompressed JPEG data
 */	
BYTE* FDecoderJPEG::Decode()
{
	check(JPEGData != NULL && JPEGDataSize > 0);

	FScopeLock JPEGLock(&GJPEGDecoder);

	BYTE* Result = NULL;	
#if WITH_JPEG
	Result = jpgd::decompress_jpeg_image_from_memory(JPEGData, JPEGDataSize, &Width, &Height, &NumColors, NumColors == 1 ? 1 : 4);
#endif //WITH_JPEG

	return Result;
}


/**
 * FEncoderJPEG constructor
 *
 * @param InRawData ptr to byte array of RAW data in memory
 * @param InRawDataSize size of RAW data
 */
FEncoderJPEG::FEncoderJPEG(
	const BYTE* InRawData,
	DWORD InRawDataSize,
	const INT InWidth,
	const INT InHeight,
	const INT InNumColors)
		: RawData(InRawData)
		, RawDataSize(InRawDataSize)
		, Width(InWidth)
		, Height(InHeight)
		, NumColors(InNumColors)
		, EncodedSize(0)
{
	check(RawData);
	check(RawDataSize);
	check(Width);
	check(Height);
	check(NumColors);
}
	
/**
 * Starts the encoding process
 * @return results of the compressed JPEG data
*/
BYTE* FEncoderJPEG::Encode()
{
	FScopeLock JPEGLock(&GJPEGEncoder);
	
	BYTE* Result = NULL;
#if WITH_JPEG
	jpge::params CompParams;
	CompParams.m_quality = 90;

	if (1 == NumColors)
	{
		CompParams.m_subsampling = jpge::Y_ONLY;
	}

	INT OutLength = RawDataSize*2;
	Result = (BYTE*)appMalloc(OutLength);

	UBOOL res = compress_image_to_jpeg_file_in_memory(
		Result,
		OutLength,
		Width, 
		Height, 
		NumColors, 
		RawData,
		CompParams);

	if (!res)
	{
		appFree(Result);
		Result = NULL;
		OutLength = 0;
	}
	else
	{
		EncodedSize = OutLength;
	}

#endif //WITH_JPEG

	return Result;
}
