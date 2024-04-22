/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEdCLR.h"
#include "ThumbnailToolsCLR.h"

namespace ThumbnailToolsCLR
{
	/**
	 * Generates a WPF bitmap for the specified thumbnail
	 * 
	 * @param	InThumbnail		The thumbnail object to create a bitmap for
	 * 
	 * @return	Returns the BitmapSource for the thumbnail, or null on failure
	 */
	Imaging::BitmapSource^ CreateBitmapSourceForThumbnail( const FObjectThumbnail& InThumbnail )
	{
		// Grab the raw thumbnail image data.  Note that this will decompress the image on demand if needed.
		const TArray< BYTE >& ThumbnailImageData = InThumbnail.GetUncompressedImageData();

		// Setup thumbnail metrics
		PixelFormat ImagePixelFormat = PixelFormats::Bgr32;
		const UINT ImageStride = InThumbnail.GetImageWidth() * sizeof( FColor );

		Imaging::BitmapSource^ MyBitmapSource = nullptr;
		if( ThumbnailImageData.Num() > 0 )
		{
			const UINT TotalImageBytes = ThumbnailImageData.Num();

			// @todo: Dots per inch.  Does it matter what we set this to?
			const int DPI = 96;

			System::IntPtr ImageDataPtr( ( PTRINT )&ThumbnailImageData( 0 ) );

			// Create the WPF bitmap object from our source data!
			MyBitmapSource = Imaging::BitmapSource::Create(
				InThumbnail.GetImageWidth(),		// Width
				InThumbnail.GetImageHeight(),		// Height
				DPI,								// Horizontal DPI
				DPI,								// Vertical DPI
				ImagePixelFormat,					// Pixel format
				nullptr,							// Palette
				ImageDataPtr,						// Image data
				TotalImageBytes,					// Size of image data
				ImageStride );						// Stride
		}

		return MyBitmapSource;
	}



	/**
	 * Updates a writeable WPF bitmap with the specified thumbnail's data
	 * 
	 * @param	InThumbnail		The thumbnail object to create a bitmap for
	 */
	void CopyThumbnailToWriteableBitmap( const FObjectThumbnail& InThumbnail, Imaging::WriteableBitmap^ WriteableBitmap )
	{
		// Grab the raw thumbnail image data.  Note that this will decompress the image on demand if needed.
		const TArray< BYTE >& ThumbnailImageData = InThumbnail.GetUncompressedImageData();

		// Setup thumbnail metrics
		check( ThumbnailImageData.Num() > 0 );


		// Copy data to the bitmap
		{
			// Lock bitmap for writing
			FColor* SourceImageData = (FColor*)&ThumbnailImageData( 0 );
			WriteableBitmap->Lock();

			FColor* ImageBackBuffer = (FColor*)(PTRINT)WriteableBitmap->BackBuffer;
			int ImageRowStride = WriteableBitmap->BackBufferStride / sizeof( FColor );;

			const int Width = InThumbnail.GetImageWidth();
			const int Height = InThumbnail.GetImageHeight();
			if( ImageRowStride == Width )
			{
				appMemcpy( ImageBackBuffer, SourceImageData, ThumbnailImageData.Num() );
			}
			else
			{
				for( int Y = 0; Y < Height; ++Y )
				{
					for( int X = 0; X < Width; ++X )
					{
						ImageBackBuffer[ Y * ImageRowStride + X ] = SourceImageData[ Y * Width + X ];
					}
				}
			}

			// Dirty the entire bitmap
			Int32Rect DirtyRect( 0, 0, Width, Height );
			WriteableBitmap->AddDirtyRect( DirtyRect );

			// Unlock the bitmap
			WriteableBitmap->Unlock();
		}
	}



	/**
	 * Generates a writeable WPF bitmap for the specified thumbnail
	 * 
	 * @param	InThumbnail		The thumbnail object to create a bitmap for
	 * 
	 * @return	Returns the BitmapSource for the thumbnail, or null on failure
	 */
	Imaging::WriteableBitmap^ CreateWriteableBitmapForThumbnail( const FObjectThumbnail& InThumbnail )
	{
		// Setup thumbnail metrics
		PixelFormat ImagePixelFormat = PixelFormats::Bgr32;

		// @todo: Dots per inch.  Does it matter what we set this to?
		const int DPI = 96;


		// Create the WPF bitmap object from our source data!
		Imaging::WriteableBitmap^ MyBitmap = gcnew Imaging::WriteableBitmap(
			InThumbnail.GetImageWidth(),		// Width
			InThumbnail.GetImageHeight(),		// Height
			DPI,								// Horizontal DPI
			DPI,								// Vertical DPI
			ImagePixelFormat,					// Pixel format
			nullptr );							// Stride


		// Copy data to the bitmap
		CopyThumbnailToWriteableBitmap( InThumbnail, MyBitmap );

		return MyBitmap;
	}


	/**
	 * Attempts to get a thumbnail for the specified object, returning from cache or generating otherwise.
	 * 
	 * @param	Object	the object that needs a thumbnail
	 * 
	 * @return	Returns the BitmapSource for the generated thumbnail, or null if a thumbnail is not available
	 */
	Imaging::BitmapSource^ GetBitmapSourceForObject( UObject* InObject )
	{
		Imaging::BitmapSource^ MyBitmapSource = nullptr;

		// Check to see if the thumbnail is already in memory for this object.
		const FObjectThumbnail* FoundThumbnail = ThumbnailTools::FindCachedThumbnail( *InObject->GetFullName() );
			
		// We don't care about empty thumbnails here
		if( FoundThumbnail != NULL && FoundThumbnail->IsEmpty() )
		{
			FoundThumbnail = NULL;
		}
		
		// If we didn't find it in memory, OR if the thumbnail needs to be refreshed...
		if( FoundThumbnail == NULL || FoundThumbnail->IsDirty() )
		{
			// could not find in cache, generate a thumbnail for it now!
			FoundThumbnail = ThumbnailTools::GenerateThumbnailForObject( InObject );
			if( FoundThumbnail == NULL )
			{
				// Couldn't generate a thumb; perhaps this object doesn't support thumbnails?
			}
		}

		if( FoundThumbnail != NULL )
		{
			// Create a WPF bitmap object for the thumbnail
			MyBitmapSource = CreateBitmapSourceForThumbnail( *FoundThumbnail );
			if( MyBitmapSource == nullptr )
			{
				// Couldn't create the bitmap for some reason
			}
		}

		return MyBitmapSource;
	}
}

