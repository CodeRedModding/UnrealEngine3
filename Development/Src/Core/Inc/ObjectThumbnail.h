/*=============================================================================
	ObjectThumbnail.h: Stored thumbnail support for Unreal objects
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if _MSC_VER
#pragma once
#endif

#ifndef __ObjectThumbnail_h__
#define __ObjectThumbnail_h__



/**
 * Thumbnail compression interface for packages.  The engine registers a class that can compress and
 * decompress thumbnails that the package linker uses while loading and saving data.
 */
class FThumbnailCompressionInterface
{
	
public:

	/**
	 * Compresses an image
	 *
	 * @param	InUncompressedData	The uncompressed image data
	 * @param	InWidth				Width of the image
	 * @param	InHeight			Height of the image
	 * @param	OutCompressedData	[Out] Compressed image data
	 *
	 * @return	TRUE if the image was compressed successfully, otherwise FALSE if an error occurred
	 */
	virtual UBOOL CompressImage( const TArray< BYTE >& InUncompressedData, const INT InWidth, const INT InHeight, TArray< BYTE >& OutCompressedData ) = 0;


	/**
	 * Decompresses an image
	 *
	 * @param	InCompressedData	The compressed image data
	 * @param	InWidth				Width of the image
	 * @param	InHeight			Height of the image
	 * @param	OutUncompressedData	[Out] Uncompressed image data
	 *
	 * @return	TRUE if the image was decompressed successfully, otherwise FALSE if an error occurred
	 */
	virtual UBOOL DecompressImage( const TArray< BYTE >& InCompressedData, const INT InWidth, const INT InHeight, TArray< BYTE >& OutUncompressedData ) = 0;

};




/** Thumbnail image data for an object */
class FObjectThumbnail
{

public:

	/**
	 * Static: Sets the thumbnail compressor to use when loading/saving packages.  The caller is
	 * responsible for the object's lifespan
	 *
	 * @param	InThumbnailCompressor	A class derived from FThumbnailCompressionInterface
	 */
	static void SetThumbnailCompressor( FThumbnailCompressionInterface* InThumbnailCompressor )
	{
		ThumbnailCompressor = InThumbnailCompressor;
	}


private:

	/** Static: Thumbnail compressor */
	static FThumbnailCompressionInterface* ThumbnailCompressor;


public:

	/** Default constructor */
	FObjectThumbnail();


	/** Returns the width of the thumbnail */
	INT GetImageWidth() const
	{
		return ImageWidth;
	}


	/** Returns the height of the thumbnail */
	INT GetImageHeight() const
	{
		return ImageHeight;
	}

	/** @return		the number of bytes in this thumbnail's compressed image data */
	INT GetCompressedDataSize() const
	{
		return CompressedImageData.Num();
	}


	/** Sets the image dimensions */
	void SetImageSize( INT InWidth, INT InHeight )
	{
		ImageWidth = InWidth;
		ImageHeight = InHeight;
	}

	/** Returns true if the thumbnail was loaded from disk and not dynamically generated */
	UBOOL IsLoadedFromDisk(void) const { return bLoadedFromDisk; }

	/** Returns true if the thumbnail was saved AFTER custom-thumbnails for shared thumbnail asset types was supported */
	UBOOL IsCreatedAfterCustomThumbsEnabled (void) const { return bCreatedAfterCustomThumbForSharedTypesEnabled; }
	/** For newly generated custom thumbnails, mark it as valid in the future */
	void SetCreatedAfterCustomThumbsEnabled(void) { bCreatedAfterCustomThumbForSharedTypesEnabled = TRUE; }


	/** Returns true if the thumbnail is dirty and needs to be regenerated at some point */
	UBOOL IsDirty() const
	{
		return bIsDirty;
	}

	
	/** Marks the thumbnail as dirty */
	void MarkAsDirty()
	{
		bIsDirty = TRUE;
	}
	
	/** Access the image data in place (does not decompress) */
	TArray< BYTE >& AccessImageData()
	{
		return ImageData;
	}

	/** Returns true if this is an empty thumbnail */
	UBOOL IsEmpty() const
	{
		return ImageWidth == 0 || ImageHeight == 0;
	}


	/** Returns uncompressed image data, decompressing it on demand if needed */
	const TArray< BYTE >& GetUncompressedImageData() const;

	/** Serializer */
	void Serialize( FArchive& Ar );
	
	/** Compress image data */
	void CompressImageData();

	/** Decompress image data */
	void DecompressImageData();

	/**
	 * Calculates the memory usage of this FObjectThumbnail.
	 *
	 * @param	Ar	the FArchiveCountMem (or similar) archive that will store the results of the memory usage calculation.
	 */
	void CountBytes( FArchive& Ar ) const;

	/**
	 * Calculates the amount of memory used by the compressed bytes array
	 *
	 * @param	Ar	the FArchiveCountMem (or similar) archive that will store the results of the memory usage calculation.
	 */
	void CountImageBytes_Compressed( FArchive& Ar ) const;
	/**
	 * Calculates the amount of memory used by the uncompressed bytes array
	 *
	 * @param	Ar	the FArchiveCountMem (or similar) archive that will store the results of the memory usage calculation.
	 */
	void CountImageBytes_Uncompressed( FArchive& Ar ) const;

	/** I/O operator */
	friend FArchive& operator<<( FArchive& Ar, FObjectThumbnail& Thumb )
	{
		if ( Ar.IsCountingMemory() )
		{
			Thumb.CountBytes(Ar);
		}
		else
		{
			Thumb.Serialize(Ar);
		}
		return Ar;
	}
	friend FArchive& operator<<( FArchive& Ar, const FObjectThumbnail& Thumb )
	{
		Thumb.CountBytes(Ar);
		return Ar;
	}

	/** Comparison operator */
	UBOOL operator ==( const FObjectThumbnail& Other ) const
	{
		return	ImageWidth			== Other.ImageWidth
			&&	ImageHeight			== Other.ImageHeight
			&&	bIsDirty			== Other.bIsDirty
			&&	CompressedImageData	== Other.CompressedImageData;
	}
	UBOOL operator !=( const FObjectThumbnail& Other ) const
	{
		return	ImageWidth			!= Other.ImageWidth
			||	ImageHeight			!= Other.ImageHeight
			||	bIsDirty			!= Other.bIsDirty
			||	CompressedImageData	!= Other.CompressedImageData;
	}

private:

	/** Thumbnail width (serialized) */
	INT ImageWidth;

	/** Thumbnail height (serialized) */
	INT ImageHeight;

	/** Compressed image data (serialized) */
	TArray< BYTE > CompressedImageData;

	/** Image data bytes */
	TArray< BYTE > ImageData;

	/** True if the thumbnail is dirty and should be regenerated at some point */
	UBOOL bIsDirty;

	/** Whether the thumbnail has a backup on disk*/
	UBOOL bLoadedFromDisk;
	/** Whether this was saved AFTER custom-thumbnails for shared thumbnail asset types was supported */
	UBOOL bCreatedAfterCustomThumbForSharedTypesEnabled;

};


/** Maps an object's full name to a thumbnail */
typedef TMap< FName, FObjectThumbnail > FThumbnailMap;


/** Wraps an object's full name and thumbnail */
struct FObjectFullNameAndThumbnail
{
	/** Full name of the object */
	FName ObjectFullName;
	
	/** Thumbnail data */
	const FObjectThumbnail* ObjectThumbnail;

	/** Offset in the file where the data is stored */
	INT FileOffset;


	/** Constructor */
	FObjectFullNameAndThumbnail()
		: ObjectFullName(),
		  ObjectThumbnail( NULL ),
		  FileOffset( 0 )
	{
	}

	/** Constructor */
	FObjectFullNameAndThumbnail( const FName InFullName, const FObjectThumbnail* InThumbnail )
		: ObjectFullName( InFullName ),
		  ObjectThumbnail( InThumbnail ),
		  FileOffset( 0 )
	{
	}

	/**
	 * Calculates the memory usage of this FObjectFullNameAndThumbnail.
	 *
	 * @param	Ar	the FArchiveCountMem (or similar) archive that will store the results of the memory usage calculation.
	 */
	void CountBytes( FArchive& Ar ) const;

	/** I/O operator */
	friend FArchive& operator<<( FArchive& Ar, FObjectFullNameAndThumbnail& NameThumbPair )
	{
		if ( Ar.IsCountingMemory() )
		{
			NameThumbPair.CountBytes(Ar);
		}
		else
		{
			Ar << NameThumbPair.ObjectFullName << NameThumbPair.FileOffset;
		}
		return Ar;
	}
	friend FArchive& operator<<( FArchive& Ar, const FObjectFullNameAndThumbnail& NameThumbPair )
	{
		NameThumbPair.CountBytes(Ar);
		return Ar;
	}
};



#endif	// __ObjectThumbnail_h__