/*=============================================================================
	ObjectThumbnail.cpp: Stored thumbnail support for Unreal objects
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"


/** Static: Thumbnail compressor */
FThumbnailCompressionInterface* FObjectThumbnail::ThumbnailCompressor = NULL;



/** Default constructor */
FObjectThumbnail::FObjectThumbnail()
	: ImageWidth( 0 ),
	  ImageHeight( 0 ),
	  bIsDirty( FALSE ),
	  bLoadedFromDisk(FALSE),
	  bCreatedAfterCustomThumbForSharedTypesEnabled(FALSE)
{
}



/** Returns uncompressed image data, decompressing it on demand if needed */
const TArray< BYTE >& FObjectThumbnail::GetUncompressedImageData() const
{
	if( ImageData.Num() == 0 )
	{
		// Const cast here so that we can populate the image data on demand	(write once)
		FObjectThumbnail* MutableThis = const_cast< FObjectThumbnail* >( this );
		MutableThis->DecompressImageData();
	}
	return ImageData;
}



/** Serializer */
void FObjectThumbnail::Serialize( FArchive& Ar )
{
	Ar << ImageWidth;
	Ar << ImageHeight;

	//if the image thinks it's empty, ensure there is no memory waste
	if ((ImageWidth == 0) || (ImageHeight==0))
	{
		CompressedImageData.Reset();
	}

	// Compress the image on demand if we don't have any compressed bytes yet.
	if( CompressedImageData.Num() == 0 &&
		( Ar.IsSaving() || Ar.IsCountingMemory() ) )
	{
		CompressImageData();
	}
	Ar << CompressedImageData;

	if ( Ar.IsCountingMemory() )
	{
		Ar << ImageData << bIsDirty;
	}

	if (Ar.IsLoading())
	{
		bLoadedFromDisk = TRUE;
		if (Ar.Ver() >= VER_ENABLED_CUSTOM_THUMBNAILS_FOR_SHARED_TYPES)
		{
			if ((ImageWidth>0) && (ImageHeight>0))
			{
				bCreatedAfterCustomThumbForSharedTypesEnabled = TRUE;
			}
		}
	}
}


	
/** Compress image data */
void FObjectThumbnail::CompressImageData()
{
	CompressedImageData.Reset();
	if( ThumbnailCompressor != NULL && ImageData.Num() > 0 && ImageWidth > 0 && ImageHeight > 0 )
	{
		ThumbnailCompressor->CompressImage( ImageData, ImageWidth, ImageHeight, CompressedImageData );
	}
}



/** Decompress image data */
void FObjectThumbnail::DecompressImageData()
{
	ImageData.Reset();
	if( ThumbnailCompressor != NULL && CompressedImageData.Num() > 0 && ImageWidth > 0 && ImageHeight > 0 )
	{
		ThumbnailCompressor->DecompressImage( CompressedImageData, ImageWidth, ImageHeight, ImageData );
	}
}

/**
 * Calculates the memory usage of this FObjectThumbnail.
 *
 * @param	Ar	the FArchiveCountMem (or similar) archive that will store the results of the memory usage calculation.
 */
void FObjectThumbnail::CountBytes( FArchive& Ar ) const
{
	SIZE_T StaticSize = sizeof(FObjectThumbnail);
	Ar.CountBytes(StaticSize, Align(StaticSize, __alignof(FObjectThumbnail)));

	FObjectThumbnail* UnconstThis = const_cast<FObjectThumbnail*>(this);
	UnconstThis->CompressedImageData.CountBytes(Ar);
	UnconstThis->ImageData.CountBytes(Ar);
}

/**
 * Calculates the amount of memory used by the compressed bytes array
 *
 * @param	Ar	the FArchiveCountMem (or similar) archive that will store the results of the memory usage calculation.
 */
void FObjectThumbnail::CountImageBytes_Compressed( FArchive& Ar ) const
{
	const_cast<FObjectThumbnail*>(this)->CompressedImageData.CountBytes(Ar);
}

/**
 * Calculates the amount of memory used by the uncompressed bytes array
 *
 * @param	Ar	the FArchiveCountMem (or similar) archive that will store the results of the memory usage calculation.
 */
void FObjectThumbnail::CountImageBytes_Uncompressed( FArchive& Ar ) const
{
	const_cast<FObjectThumbnail*>(this)->ImageData.CountBytes(Ar);
}

/**
 * Calculates the memory usage of this FObjectFullNameAndThumbnail.
 *
 * @param	Ar	the FArchiveCountMem (or similar) archive that will store the results of the memory usage calculation.
 */
void FObjectFullNameAndThumbnail::CountBytes( FArchive& Ar ) const
{
	SIZE_T StaticSize = sizeof(FObjectFullNameAndThumbnail);
	Ar.CountBytes(StaticSize, Align(StaticSize, __alignof(FObjectFullNameAndThumbnail)));


	FNameEntry* NameEntry = FName::GetEntry(ObjectFullName.GetIndex());
	SIZE_T NameSize = NameEntry->GetSize(NAME_SIZE, FALSE);
	Ar.CountBytes(NameSize, NameSize);

	if ( ObjectThumbnail != NULL )
	{
		ObjectThumbnail->CountBytes(Ar);
	}
}



