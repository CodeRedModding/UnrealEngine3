/*=============================================================================
	UnAudioCompress.h: Unreal audio compression - ogg vorbis
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNAUDIOCOMPRESS_H__
#define __UNAUDIOCOMPRESS_H__

/**
 * PC (ogg vorbis) sound cooker interface.
 */
class FPCSoundCooker : public FConsoleSoundCooker
{
public:
	/**
	 * Constructor
	 */
	FPCSoundCooker( void )
	{
	}

	/**
	 * Virtual destructor
	 */
	~FPCSoundCooker( void )
	{
	}

	/**
	 * Cooks the source data for the platform and stores the cooked data internally.
	 *
	 * @param	SrcBuffer		Pointer to source buffer
	 * @param	QualityInfo		All the information the compressor needs to compress the audio
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	virtual bool Cook( const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo );

	/**
	 * Cooks upto 8 mono files into a multistream file (eg. 5.1). The front left channel is required, the rest are optional.
	 *
	 * @param	SrcBuffers		Pointers to source buffers
	 * @param	QualityInfo		All the information the compressor needs to compress the audio
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	virtual bool CookSurround( const BYTE* SrcBuffers[8], FSoundQualityInfo* QualityInfo );

	/**
	 * Returns the size of the cooked data in bytes.
	 *
	 * @return The size in bytes of the cooked data including any potential header information.
	 */
	virtual UINT GetCookedDataSize( void );

	/**
	 * Copies the cooked data into the passed in buffer of at least size GetCookedDataSize()
 	 *
	 * @param CookedData	Buffer of at least GetCookedDataSize() bytes to copy data to.
	 */
	virtual void GetCookedData( BYTE* CookedData );

	/** 
	 * Decompresses the platform dependent format to raw PCM. Used for quality previewing.
	 *
	 * @param	SrcData					Uncompressed PCM data
	 * @param	DstData					Uncompressed PCM data after being compressed		
	 * @param	QualityInfo				All the information the compressor needs to compress the audio
	 */
	virtual INT Recompress( const BYTE* SrcBuffer, BYTE* DestBuffer, FSoundQualityInfo* QualityInfo );

private:
	TArray<BYTE>		CompressedDataStore;
	DWORD				BufferOffset;
};

/**
 * Singleton to return the cooking class for PC sounds
 */
FPCSoundCooker* GetPCSoundCooker( void );

// end
#endif
