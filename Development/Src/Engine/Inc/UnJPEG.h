/*=============================================================================
	UnJPEG.h: Unreal JPEG support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * Uncompresses JPEG data to raw 24bit RGB image that can be used by Unreal textures
 */
class FDecoderJPEG
{
public:
	/**
	 * Constructor
	 *
	 * @param InJPEGData ptr to byte array of JPEG data in memory
	 * @param InJPEGDataSize size of JPEG data
	 */
	FDecoderJPEG(const BYTE* InJPEGData=NULL, DWORD InJPEGDataSize=0, const INT InNumColors=0);
	/**
	 * Starts the decoding process
	 *
	 * @return results of the decompressed JPEG data
	 */
	BYTE* Decode();
	/** @return The width of this JPEG */
	UINT GetWidth() const { return Width; }
	/** @return The height of this JPEG */
	UINT GetHeight()  const { return Height; }
	/** @return The number of actual color components of this JPEG */
	UINT GetNumColors()  const { return NumColors; }

private:
	/** Ptr to compressed image data to be processed */
	const BYTE* JPEGData;
	/** Size of compressed image data to be processed */
	const DWORD JPEGDataSize;
	/** Width of image in pixels. Filled in after Decode() */
	INT Width;
	/** Height of image in pixels. Filled in after Decode() */
	INT Height;
	/** Number of colors per pixel. Filled in after Decode() */
	INT NumColors;
};

/**
 * Compresses raw 24bit RGB image to JPEG
 */
class FEncoderJPEG
{
public:
	/**
	 * Constructor
	 *
	 * @param InRawData ptr to byte array of RAW data in memory
	 * @param InRawDataSize size of RAW data
	 */
	FEncoderJPEG(const BYTE* InRawData, DWORD InRawDataSize, const INT InWidth, const INT InHeight, const INT InNumColors);
	
	/**
	 * Starts the encoding process
	 *
	 * @return results of the compressed JPEG data
	 */
	BYTE* Encode();

	const INT GetEncodedSize() const {return EncodedSize;}
		
private:
	/** Ptr to decompressed image data to be processed */
	const BYTE* RawData;
	/** Size of decompressed image data to be processed */
	const DWORD RawDataSize;

	/** Width of image in pixels. */
	INT Width;
	/** Height of image in pixels. */
	INT Height;
	/** Number of colors per pixel. */
	INT NumColors;

	/** Size of compressed output. Valid after call Encode() */
	INT EncodedSize;
};
