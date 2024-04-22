/*=============================================================================
	UnPNG.cpp: Unreal PNG support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

	http://www.libpng.org/pub/png/libpng-manual.txt
	http://www.libpng.org/pub/png/libpng-manual.html (outdated)

	The below code isn't documented as it's black box behavior is almost 
	entirely based on the sample from the libPNG documentation. 
	
	InitCompressed and InitRaw will set initial state and you will then be 
	able to fill in Raw or CompressedData by calling Uncompress or Compress 
	respectively.

=============================================================================*/

#include "EnginePrivate.h"

#if !CONSOLE && !PLATFORM_MACOSX

#if PNG_LIBPNG_VER >= 10513

#if _WIN64
	#pragma comment(lib, "libpng15_64.lib")
#else
	#pragma comment(lib, "libpng15.lib")
#endif

// Functions that are here to help the GFx libraries link until they are updated to be compiled with libPNG-1.5.13

#ifdef png_check_sig
#undef png_check_sig
#endif

extern "C" {

/* (Obsolete) function to check signature bytes.  It does not allow one
 * to check a partial signature.  This function might be removed in the
 * future - use png_sig_cmp().  Returns true (nonzero) if the file is PNG.
 */
int PNGAPI
png_check_sig(png_bytep sig, int num)
{
  return ((int)!png_sig_cmp(sig, (png_size_t)0, (png_size_t)num));
}

/* (Obsolete) function to expand grayscale images of less than 8-bit depth
 * to 8 bits.
 */
void PNGAPI
png_set_gray_1_2_4_to_8(png_structp png_ptr)
{
	png_set_expand_gray_1_2_4_to_8(png_ptr);
}

};

#else

#if _WIN64
#pragma comment(lib, "libpng_64.lib")
#else
#pragma comment(lib, "libpng.lib")
#endif

#endif

/** Only allow one thread to use libpng at a time (it's not thread safe) */
FCriticalSection GPNGSection;

// Guard that safely releases PNG reader resources
class PNGReadGuard
{
public:
	PNGReadGuard( png_structp* InReadStruct, png_infop* InInfo )
		: PNGReadStruct(InReadStruct)
		, PNGInfo(InInfo)
		, PNGRowPointers(NULL)
	{
	}

	void SetRowPointers( png_bytep** InRowPointers )
	{
		PNGRowPointers = InRowPointers;
	}

	~PNGReadGuard()
	{
		if (PNGRowPointers != NULL)
		{
			png_free( *PNGReadStruct, *PNGRowPointers );
		}
		png_destroy_read_struct( PNGReadStruct, PNGInfo, NULL );
	}

private:
	png_structp* PNGReadStruct;
	png_infop* PNGInfo;
	png_bytep** PNGRowPointers;
};


/**
 * Construct a PNGLoader given some data (presumably PNG data)
 * and its size.
 *
 * @param InPNGData Pointer to PNG data.
 * @param InPNGDataSize Size of PNGData in bytes.
 */
FPNGLoader::FPNGLoader( const BYTE* InPNGData, UINT InPNGDataSize )
	: ReadOffset(0)
	, PNGDataSize(InPNGDataSize)
	, PNGData(InPNGData)
	, WidthFromHeader(0)
	, HeightFromHeader(0)
	, PNGColorType(0)
{
	check(PNGData != NULL);

	if ( this->IsPNG() )
	{
		// thread safety
		FScopeLock PNGLock(&GPNGSection);

		png_structp PNGReadStruct = png_create_read_struct( PNG_LIBPNG_VER_STRING, this, user_error_fn, user_warning_fn );
		check( PNGReadStruct );

		png_infop PNGInfo = png_create_info_struct( PNGReadStruct );
		check( PNGInfo );

		PNGReadGuard PNGGuard( &PNGReadStruct, &PNGInfo );
		{
			png_set_read_fn( PNGReadStruct, this, user_read_data );

			png_read_info( PNGReadStruct, PNGInfo );

			WidthFromHeader = png_get_image_width( PNGReadStruct, PNGInfo );			// PNGInfo->width
			HeightFromHeader = png_get_image_height( PNGReadStruct, PNGInfo );			// PNGInfo->height
			PNGColorType = png_get_color_type( PNGReadStruct, PNGInfo );				// PNGInfo->color_type
		}
	}
}

/**
 * Decode the PNG data into the provided buffer.
 * Assumes that the output buffer is big enough
 * for accommodate Width() x Height() x sizeof(Pixel)
 *
 * @param OutRawData Output buffer for decoded image data.
 */
UBOOL FPNGLoader::Decode( BYTE* OutDecompressedData )
{
	check( PNGData != NULL );
	
	const INT BytesPerPixel = 4;

	const INT Height = this->Height();
	const INT Width = this->Width();

	// Reset to the beginning of file so we can use png_read_png(), which expects to start at the beginning.
	ReadOffset = 0;

	// thread safety
	FScopeLock PNGLock(&GPNGSection);

	png_structp PNGReadStruct = png_create_read_struct( PNG_LIBPNG_VER_STRING, this, user_error_fn, user_warning_fn );
	check( PNGReadStruct );

	png_infop PNGInfo = png_create_info_struct( PNGReadStruct );
	check( PNGInfo );

	PNGReadGuard PNGGuard( &PNGReadStruct, &PNGInfo );
	{
		png_set_read_fn( PNGReadStruct, this, user_read_data );

		png_bytep* row_pointers = (png_bytep*) png_malloc( PNGReadStruct, Height*sizeof(png_bytep) );
		PNGGuard.SetRowPointers( &row_pointers );

		for( INT RowIndex=0; RowIndex<Height; RowIndex++ )
		{
			row_pointers[RowIndex] = &OutDecompressedData[RowIndex * Width * BytesPerPixel];
		}

		if ( ColorType() != PNG_COLOR_TYPE_RGBA && ColorType() != PNG_COLOR_TYPE_RGB )
		{
			return FALSE;
		}

		if (ColorType() != PNG_COLOR_TYPE_RGBA)
		{
			png_set_add_alpha(PNGReadStruct, 255, PNG_FILLER_AFTER);
		}

		png_set_rows(PNGReadStruct, PNGInfo, row_pointers);

		// Note that PNGs on PC tend to be BGR
		// @todo : How to detect BGR vs RGB. Is it just an endian thing; wouldn't that be ARGB vs BGRA? Seems like We are faced with RGBA vs BGRA!
		png_read_png( PNGReadStruct, PNGInfo, PNG_TRANSFORM_BGR, NULL );
	}

	////row_pointers = png_get_rows( png_ptr, info_ptr );
	//png_free( PNGReadStruct, row_pointers );
	//
	//// @todo: make sure this is scope guarded
	//png_destroy_read_struct( &PNGReadStruct, &PNGInfo, NULL );

	return TRUE;
}

/** Test whether the data this PNGLoader is pointing at is a PNG or not. */
UBOOL FPNGLoader::IsPNG() const
{
	check(PNGData!=NULL);

	const INT PNGSigSize = sizeof(png_size_t);
	if ( PNGDataSize > PNGSigSize )
	{
		png_size_t PNGSignature = *reinterpret_cast<const png_size_t*>(PNGData);
		return ( 0 == png_sig_cmp( reinterpret_cast<png_bytep>(&PNGSignature), 0, PNGSigSize ) );
	}

	return FALSE;
}

/** The width of this PNG, based on the header. */
UINT FPNGLoader::Width() const
{
	return WidthFromHeader;
}

/** The height of this PNG, based on the header. */
UINT FPNGLoader::Height()  const
{
	return HeightFromHeader;
}

/** @return The color type of the PNG data (e.g. PNG_COLOR_TYPE_RGBA, PNG_COLOR_TYPE_RGB) */
INT FPNGLoader::ColorType() const
{
	return PNGColorType;
}

void FPNGLoader::user_read_data( png_structp png_ptr, png_bytep data, png_size_t length )
{
	FPNGLoader* ctx = reinterpret_cast<FPNGLoader*>( png_get_io_ptr( png_ptr ) );

	check( ctx->ReadOffset + length <= ctx->PNGDataSize );
	appMemcpy( data, &ctx->PNGData[ctx->ReadOffset], length );
	ctx->ReadOffset += length;
}

void FPNGLoader::user_flush_data( png_structp png_ptr )
{
}

void FPNGLoader::user_error_fn( png_structp png_ptr, png_const_charp error_msg )
{
	appThrowf(TEXT("PNG Error: %s"), ANSI_TO_TCHAR(error_msg));
}

void FPNGLoader::user_warning_fn( png_structp png_ptr, png_const_charp warning_msg )
{
	appErrorf(TEXT("PNG Warning: %s"), ANSI_TO_TCHAR(warning_msg));
}








/*------------------------------------------------------------------------------
	FPNGHelper implementation.
------------------------------------------------------------------------------*/
void FPNGHelper::InitCompressed( const void* InCompressedData, INT InCompressedSize, INT InWidth, INT InHeight )
{
	Width	= InWidth;
	Height	= InHeight;

	PNGHeaderWidth  = 0;
	PNGHeaderHeight = 0;

	ReadOffset = 0;
	RawData.Empty();
	
	CompressedData.Empty( InCompressedSize );
	CompressedData.Add( InCompressedSize );

	appMemcpy( &CompressedData(0), InCompressedData, InCompressedSize );
}

void FPNGHelper::InitRaw( const void* InRawData, INT InRawSize, INT InWidth, INT InHeight )
{
	Width	= InWidth;
	Height	= InHeight;

	PNGHeaderWidth  = InWidth;
	PNGHeaderHeight = InHeight;

	CompressedData.Empty();

	ReadOffset = 0;
	RawData.Empty( InRawSize );
	RawData.Add( InRawSize );

	appMemcpy( &RawData(0), InRawData, InRawSize );
}

void FPNGHelper::Uncompress()
{
	if( !RawData.Num() )
	{
		// thread safety
		FScopeLock PNGLock(&GPNGSection);

		check( CompressedData.Num() );

		png_structp png_ptr	= png_create_read_struct( PNG_LIBPNG_VER_STRING, this, user_error_fn, user_warning_fn );
		check( png_ptr );

		png_infop info_ptr	= png_create_info_struct( png_ptr );
		check( info_ptr );
		
		png_set_read_fn( png_ptr, this, user_read_data );

		RawData.Empty( Width * Height * 4 );
		RawData.Add( Width * Height * 4 );

		png_bytep* row_pointers = (png_bytep*) png_malloc( png_ptr, Height*sizeof(png_bytep) );
		for( INT i=0; i<Height; i++ )
			row_pointers[i]= &RawData(i * Width * 4);

		png_set_rows(png_ptr, info_ptr, row_pointers);

		png_read_png( png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL );
		
		PNGHeaderWidth  = png_get_image_width(png_ptr, info_ptr);	//info_ptr->width
		PNGHeaderHeight = png_get_image_height(png_ptr, info_ptr);	//info_ptr->height;

		row_pointers = png_get_rows( png_ptr, info_ptr );
		
		png_free( png_ptr, row_pointers );
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
	}
}

void FPNGHelper::Compress()
{
	if( !CompressedData.Num() )
	{
		// thread safety
		FScopeLock PNGLock(&GPNGSection);

		check( RawData.Num() );

		png_structp png_ptr	= png_create_write_struct( PNG_LIBPNG_VER_STRING, this, user_error_fn, user_warning_fn );
		check( png_ptr );

		png_infop info_ptr	= png_create_info_struct( png_ptr );
		check( info_ptr );
		
		png_set_compression_level( png_ptr, Z_BEST_COMPRESSION );
		png_set_write_fn( png_ptr, this, user_write_data, user_flush_data );

		png_set_IHDR( png_ptr, info_ptr, Width, Height, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );

		png_bytep* row_pointers = (png_bytep*) png_malloc( png_ptr, Height*sizeof(png_bytep) );
		for( INT i=0; i<Height; i++ )
			row_pointers[i]= &RawData(i * Width * 4);
		png_set_rows( png_ptr, info_ptr, row_pointers );

		png_write_png( png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL );

		png_free( png_ptr, row_pointers );
		png_destroy_write_struct( &png_ptr, &info_ptr );
	}
}

const TArray<BYTE>& FPNGHelper::GetRawData()
{
	Uncompress();
	return RawData;
}

const TArray<BYTE>& FPNGHelper::GetCompressedData()
{
	Compress();
	return CompressedData;
}

void FPNGHelper::user_read_data( png_structp png_ptr, png_bytep data, png_size_t length )
{
	FPNGHelper* ctx = (FPNGHelper*) png_get_io_ptr( png_ptr );

	check( ctx->ReadOffset + length <= (DWORD)ctx->CompressedData.Num() );
	appMemcpy( data, &ctx->CompressedData(ctx->ReadOffset), length );
	ctx->ReadOffset += length;
}

void FPNGHelper::user_write_data( png_structp png_ptr, png_bytep data, png_size_t length )
{
	FPNGHelper* ctx = (FPNGHelper*) png_get_io_ptr( png_ptr );

	INT Offset = ctx->CompressedData.Add( length );
	appMemcpy( &ctx->CompressedData(Offset), data, length );
}

void FPNGHelper::user_flush_data( png_structp png_ptr )
{
}

void FPNGHelper::user_error_fn( png_structp png_ptr, png_const_charp error_msg )
{
	appThrowf(TEXT("PNG Error: %s"), ANSI_TO_TCHAR(error_msg));
}

void FPNGHelper::user_warning_fn( png_structp png_ptr, png_const_charp warning_msg )
{
	appErrorf(TEXT("PNG Warning: %s"), ANSI_TO_TCHAR(warning_msg));
}

#endif
