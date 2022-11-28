/*=============================================================================
	UnPNG.h: Unreal PNG support.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

/*------------------------------------------------------------------------------
	FPNGHelper definition.
------------------------------------------------------------------------------*/

#ifndef clock
#error remember to remove the below hack when deprecating clock
#endif
#undef clock
#pragma pack (push,8)
#include "..\..\..\External\libpng\png.h"
#pragma pack (pop)
#define clock(Timer)   {Timer -= appCycles();}

class FPNGHelper
{
public:
	void InitCompressed( void* InCompressedData, INT InCompressedSize, INT InWidth, INT InHeight );
	void InitRaw( void* InRawData, INT InRawSize, INT InWidth, INT InHeight );

	TArray<BYTE> GetRawData();
	TArray<BYTE> GetCompressedData();

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
					Height;
};


/*------------------------------------------------------------------------------
	The end.
------------------------------------------------------------------------------*/
