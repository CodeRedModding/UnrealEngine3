/*=============================================================================
	UnPNG.h: Unreal PNG support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*------------------------------------------------------------------------------
	FPNGHelper definition.
------------------------------------------------------------------------------*/

// don't include this on consoles or in gcc
#if !CONSOLE && defined(_MSC_VER)

#pragma pack (push,8)
#include "..\..\..\External\libpng\png.h"
#include "..\..\..\External\zlib\inc\zlib.h"
#pragma pack (pop)

/**
 * Given a BYTE* to some PNG data (probably read from disk), decompresses the PNG
 * into raw image data consumable by the engine.
 * The PNGLoader does not make copies of the data!
 */
class FPNGLoader
{
public:
	/**
	 * Construct a PNGLoader given some data (presumably PNG data)
	 * and its size.
	 *
	 * @param InPNGData Pointer to PNG data.
	 * @param InPNGDataSize Size of PNGData in bytes.
	 */
	FPNGLoader( const BYTE* InPNGData, UINT InPNGDataSize );

	/**
	 * Decode the PNG data into the provided buffer.
	 * Assumes that the output buffer is big enough
	 * for accommodate Width() x Height() x sizeof(Pixel)
	 *
	 * @param OutRawData Output buffer for decoded image data.
	 */
	UBOOL Decode( BYTE* OutRawData );

	/** @return Test whether the data this PNGLoader is pointing at is a PNG or not. */
	UBOOL IsPNG() const;
	/** @return The width of this PNG, based on the header. */
	UINT Width() const;
	/** @return The height of this PNG, based on the header. */
	UINT Height()  const;
	/** @return The color type of the PNG data (e.g. PNG_COLOR_TYPE_RGBA, PNG_COLOR_TYPE_RGB) */
	INT ColorType() const;
		
private:

	static void user_read_data( png_structp png_ptr, png_bytep data, png_size_t length );
	static void user_flush_data( png_structp png_ptr );

	static void user_error_fn( png_structp png_ptr, png_const_charp error_msg );
	static void user_warning_fn( png_structp png_ptr, png_const_charp warning_msg );

	INT ReadOffset;
	UINT PNGDataSize;
	const BYTE* PNGData;
	INT WidthFromHeader;
	INT HeightFromHeader;
	INT PNGColorType;

	
};

class FPNGHelper
{
public:
	void InitCompressed( const void* InCompressedData, INT InCompressedSize, INT InWidth, INT InHeight );
	void InitRaw( const void* InRawData, INT InRawSize, INT InWidth, INT InHeight );

	const TArray<BYTE>& GetRawData();
	const TArray<BYTE>& GetCompressedData();

	INT GetPNGHeaderWidth() const { return PNGHeaderWidth; }
	INT GetPNGHeaderHeight() const { return PNGHeaderHeight; }

protected:
	void Uncompress();
	void Compress();

	static void user_read_data( png_structp png_ptr, png_bytep data, png_size_t length );
	static void user_write_data( png_structp png_ptr, png_bytep data, png_size_t length );
	static void user_flush_data( png_structp png_ptr );

	static void user_error_fn( png_structp png_ptr, png_const_charp error_msg );
	static void user_warning_fn( png_structp png_ptr, png_const_charp warning_msg );

	// Variables.
	TArray<BYTE>	RawData;
	TArray<BYTE>	CompressedData;

	INT				ReadOffset,
					Width,
					Height,
					PNGHeaderWidth,
					PNGHeaderHeight;
};

#endif // !CONSOLE && defined(_MSC_VER)

