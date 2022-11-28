/*=============================================================================
	UnPNG.cpp: Unreal PNG support.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel

	http://www.libpng.org/pub/png/libpng-1.2.5-manual.html

	The below code isn't documented as it's black box behavior is almost 
	entirely based on the sample from the libPNG documentation. 
	
	InitCompressed and InitRaw will set initial state and you will then be 
	able to fill in Raw or CompressedData by calling Uncompress or Compress 
	respectively.

=============================================================================*/

#include "EnginePrivate.h"

#pragma comment(lib, "libpng.lib")

/*------------------------------------------------------------------------------
	FPNGHelper implementation.
------------------------------------------------------------------------------*/

void FPNGHelper::InitCompressed( void* InCompressedData, INT InCompressedSize, INT InWidth, INT InHeight )
{
	Width	= InWidth;
	Height	= InHeight;

	ReadOffset = 0;
	RawData.Empty();
	
	CompressedData.Empty( InCompressedSize );
	CompressedData.Add( InCompressedSize );

	appMemcpy( &CompressedData(0), InCompressedData, InCompressedSize );
}

void FPNGHelper::InitRaw( void* InRawData, INT InRawSize, INT InWidth, INT InHeight )
{
	Width	= InWidth;
	Height	= InHeight;

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
		
		row_pointers = png_get_rows( png_ptr, info_ptr );
		
		png_free( png_ptr, row_pointers );
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
	}
}

void FPNGHelper::Compress()
{
	if( !CompressedData.Num() )
	{
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

TArray<BYTE> FPNGHelper::GetRawData()
{
	Uncompress();
	return RawData;
}

TArray<BYTE> FPNGHelper::GetCompressedData()
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
	appErrorf(TEXT("PNG Error: %s"), ANSI_TO_TCHAR(error_msg));
}

void FPNGHelper::user_warning_fn( png_structp png_ptr, png_const_charp warning_msg )
{
	appErrorf(TEXT("PNG Warning: %s"), ANSI_TO_TCHAR(warning_msg));
}

/*------------------------------------------------------------------------------
	The end.
------------------------------------------------------------------------------*/
