/*=============================================================================
	UnTexCompress.cpp: Unreal texture compression functions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	#include "nvtt/nvtt.h"
#endif

#include "ImageUtils.h"

#if WITH_EDITOR
extern UBOOL ConditionalCachePVRTCTextures(UTexture2D* Texture, UBOOL bUseFastCompression/* =FALSE */, UBOOL bForceCompression/*=FALSE*/, TArray<FColor>* SourceArtOverride /* = NULL */ );
#endif
/*-----------------------------------------------------------------------------
	DXT functions.
-----------------------------------------------------------------------------*/

#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER

class FDXTTestTimer
{
public:
	FDXTTestTimer(const TCHAR* InInfoStr=TEXT("Test"))
		: InfoStr(InInfoStr),
		bAlreadyStopped(FALSE)
	{
		StartTime = appSeconds();
	}

	void Stop(UBOOL DisplayLog = TRUE)
	{
		if (!bAlreadyStopped)
		{
			bAlreadyStopped = TRUE;
			EndTime = appSeconds();
			TimeElapsed = EndTime-StartTime;
			if (DisplayLog)
			{
				debugf(TEXT("		[%s] took [%.4f] s"),*InfoStr,TimeElapsed);
			}
		}
	}

	~FDXTTestTimer()
	{
		Stop(TRUE);
	}

	DOUBLE StartTime,EndTime;
	DOUBLE TimeElapsed;
	FString InfoStr;
	UBOOL bAlreadyStopped;

};

// Structures required by nv Texture Tools 2 library.
struct FNVOutputHandler : public nvtt::OutputHandler
{
public:
	~FNVOutputHandler()
	{
	}
	void ReserveMemory(UINT PreAllocateSize )
	{
		CompressedData.Reserve(PreAllocateSize);
	}

	virtual void beginImage(int size, int width, int height, int depth, int face, int miplevel)
	{}

	virtual bool writeData(const void * data, int size)
	{
		check(data);
		const INT StartIndex = CompressedData.Num();
		CompressedData.Add(size);
		appMemcpy(&CompressedData(StartIndex), data, size);
		return true;
	}

	TArray<BYTE> CompressedData;
};

struct FNVErrorHandler : public nvtt::ErrorHandler
{
public:
	FNVErrorHandler() : 
		bSuccess(TRUE)
	{}

	virtual void error(nvtt::Error e)
	{
		warnf(NAME_Warning, *FString::Printf(TEXT("nvtt::compress() failed with error '%s'"), ANSI_TO_TCHAR(nvtt::errorString(e))));
		bSuccess = FALSE;
	}

	UBOOL bSuccess;
};

/** Critical section to isolate construction of nvtt objects */
FCriticalSection GNVCompressionCriticalSection;

/** Helper struct that encapsulates everything needed for the NVidia DXT compression tool. */
struct FNVCompression
{
	/**
	 * Constructor
	 *
	 * @param	SourceData				Source texture data to DXT compress, in BGRA 8bit per channel unsigned format.
	 * @param	PixelFormat				Texture format
	 * @param	SizeX					Number of texels along the X-axis
	 * @param	SizeY					Number of texels along the Y-axis
	 * @param	SRGB					Whether the texture is in SRGB space
	 * @param	bIsNormalMap			Whether the texture is a normal map
	 * @param	bUseCUDAAcceleration	Whether to use CUDA acceleration
	 * @param	bSupportDXT1a			Whether to use DXT1a or DXT1 format (if PixelFormat is DXT1)
	 * @param	QualityLevel			A value from nvtt::Quality that represents the quality of the lightmap encoding
	 */
	FNVCompression( void* InSourceData, EPixelFormat InPixelFormat, INT InSizeX, INT InSizeY, UBOOL InSRGB, UBOOL InbIsNormalMap, UBOOL InbUseCUDAAcceleration, UBOOL InbSupportDXT1a, INT InQualityLevel )
	:	SourceData( InSourceData )
	,	PixelFormat( InPixelFormat )
	,	SizeX( InSizeX )
	,	SizeY( InSizeY )
	,	SRGB( InSRGB )
	,	bIsNormalMap( InbIsNormalMap )
	,	bUseCUDAAcceleration(InbUseCUDAAcceleration)
	,	bSupportDXT1a( InbSupportDXT1a )
	,	QualityLevel( InQualityLevel )
	{}

	/** Performs the DXT compression. */
	UBOOL Process()
	{
		/** Handles any errors from the NVidia DXT-compression tool. */
		FNVErrorHandler				ErrorHandler;
		
		/** nvtt::CompressionOptions constructor uses reference counting which is not thread safe. 
		    We establish a lock during construction only. The actual compression is not inside the lock */
		GNVCompressionCriticalSection.Lock();

		bool bSuccess = FALSE;

		/** Scope for nvtt objects */
		{
			/** NVidia object that contains options that describes the input data. */
			nvtt::InputOptions			InputOptions;
			/** NVidia object that contains compression options that describes the compression work. */
			nvtt::CompressionOptions	CompressionOptions;
			/** NVidia object that contains output options. */
			nvtt::OutputOptions			OutputOptions;
			/** NVidia object that performs that DXT compression. */
			nvtt::Compressor			Compressor;

			//FDXTTestTimer Timer(*FString::Printf(TEXT("NVTT 2.0.7 CUDA %u %ux%u bIsNormalMap %u"), bUseCUDAAcceleration, SizeX, SizeY, bIsNormalMap));
			nvtt::Format TextureFormat = nvtt::Format_DXT1;
			if (PixelFormat == PF_DXT1)
			{
				TextureFormat = bSupportDXT1a ? nvtt::Format_DXT1a : nvtt::Format_DXT1;
			}
			else if (PixelFormat == PF_DXT3)
			{
				TextureFormat = nvtt::Format_DXT3;
			}
			else if (PixelFormat == PF_DXT5)
			{
				TextureFormat = nvtt::Format_DXT5;
			}
			else if (PixelFormat == PF_A8R8G8B8)
			{
				TextureFormat = nvtt::Format_RGBA;
			}
			else if (PixelFormat == PF_BC5)
			{
				TextureFormat = nvtt::Format_BC5;
			}
			else
			{
				appErrorf(TEXT("Unsupported EPixelFormat for compression: %u"), (UINT)PixelFormat);
			}

			InputOptions.setTextureLayout(nvtt::TextureType_2D, SizeX, SizeY);

			// Not generating mips with NVTT, we will pass each mip in and compress it individually
			InputOptions.setMipmapGeneration(false, -1);
			verify(InputOptions.setMipmapData(SourceData, SizeX, SizeY));

			if (SRGB)
			{
				InputOptions.setGamma(2.2f, 2.2f);
			}
			else
			{
				InputOptions.setGamma(1.0f, 1.0f);
			}

			// Only used for mip and normal map generation
			InputOptions.setWrapMode(nvtt::WrapMode_Mirror);
			InputOptions.setFormat(nvtt::InputFormat_BGRA_8UB);

			CompressionOptions.setFormat(TextureFormat);

			// Highest quality is 2x slower with only a small visual difference
			// Might be worthwhile for normal maps though
			CompressionOptions.setQuality((nvtt::Quality)QualityLevel);

			if (bIsNormalMap)
			{
				// Use the weights originally used by nvDXT for normal maps
				//@todo - re-evaluate these
				CompressionOptions.setColorWeights(0.4f, 0.4f, 0.2f);
				InputOptions.setNormalMap(true);
			}
			else
			{
				CompressionOptions.setColorWeights(1, 1, 1);
			}

			Compressor.enableCudaAcceleration(bUseCUDAAcceleration != 0);
			OutputHandler.ReserveMemory( Compressor.estimateSize(InputOptions, CompressionOptions) );

			// We're not outputting a dds file so disable the header
			OutputOptions.setOutputHeader( false);
			OutputOptions.setOutputHandler( &OutputHandler );
			OutputOptions.setErrorHandler( &ErrorHandler );

			// Release critical section
			GNVCompressionCriticalSection.Unlock();

			// Begin compression
			bSuccess = Compressor.process(InputOptions, CompressionOptions, OutputOptions);

			// Lock critical section for nvtt object destruction.
			GNVCompressionCriticalSection.Lock();

			// nvtt object destruction occurs here
		}
		GNVCompressionCriticalSection.Unlock();

		return bSuccess && ErrorHandler.bSuccess;
	}

	/** Handles the output from the NVidia DXT-compression tool. Contains the resulting data buffer. */
	FNVOutputHandler			OutputHandler;

private:
	/** Source texture data to DXT compress, in BGRA 8bit per channel unsigned format. */
	void*				SourceData;
	/** Texture format */
	enum EPixelFormat	PixelFormat;
	/** Number of texels along the X-axis */
	INT					SizeX;
	/** Number of texels along the Y-axis */
	INT					SizeY;
	/** Whether the texture is in SRGB space */	
	UBOOL SRGB;
	/** Whether the texture is a normal map */
	UBOOL bIsNormalMap;
	/** Whether to use CUDA acceleraion for the compression */
	UBOOL bUseCUDAAcceleration;
	/** Whether to use DXT1a or DXT1 format (if PixelFormat is DXT1) */
	UBOOL bSupportDXT1a;
	/** A value from nvtt::Quality that represents the quality of the lightmap encoding */
	INT QualityLevel;
};


/**
 * Compresses using Nvidia Texture Tools 2.0.7.
 */
UBOOL NVTTCompress(UTexture2D* Texture2D, void* SourceData, EPixelFormat PixelFormat, INT SizeX, INT SizeY, UBOOL SRGB, UBOOL bIsNormalMap,  UBOOL bUseCUDAAcceleration, UBOOL bSupportDXT1a, INT QualityLevel )
{
	FNVCompression Compression( SourceData, PixelFormat, SizeX, SizeY, SRGB, bIsNormalMap, bUseCUDAAcceleration, bSupportDXT1a, QualityLevel );
	const UBOOL bSuccess = Compression.Process();
	if (bSuccess)
	{
		check(Compression.OutputHandler.CompressedData.Num() > 0);
		
		// Add a mip level to Texture2D and copy over the compressed data
		FTexture2DMipMap* MipMap = new(Texture2D->Mips) FTexture2DMipMap;
		MipMap->SizeX = Max<UINT>(SizeX, GPixelFormats[PixelFormat].BlockSizeX);
		MipMap->SizeY = Max<UINT>(SizeY, GPixelFormats[PixelFormat].BlockSizeY);
		MipMap->Data.Lock(LOCK_READ_WRITE);

		UINT Size = Compression.OutputHandler.CompressedData.Num();

		check(Compression.OutputHandler.CompressedData.GetTypeSize()==1);

		appMemcpy(MipMap->Data.Realloc(Size), Compression.OutputHandler.CompressedData.GetData(), Size);

		MipMap->Data.Unlock();
	}
	return bSuccess;
}

// CUDA acceleration currently disabled, needs more robust error handling
// With one core of a Xeon 3GHz CPU, compressing a 2048^2 normal map to DXT1 with NVTT 2.0.4 takes 7.49s.
// With the same settings but using CUDA and a Geforce 8800 GTX it takes 1.66s.
// To use CUDA, a CUDA 2.0 capable driver is required (178.08 or greater) and a Geforce 8 or higher.
//static UBOOL GUseCUDAAcceleration = FALSE;
UBOOL GUseCUDAAcceleration = FALSE;

/** 
 * Compresses SourceData and adds the compressed data to Texture2D->Mips.
 *
 * @param Texture2D - The texture whose Mips array should be augmented with the compressed data
 * @param SourceData - Input data to compress, in BGRA 8bit per channel unsigned format.
 * @param PixelFormat - Output format, must be one of: PF_DXT1, PF_DXT3, PF_DXT5 or PF_A8R8G8B8
 * @param SizeX - Dimensions of SourceData
 * @param SizeY - Dimensions of SourceData
 * @param SRGB - Whether the input data is in gamma space and needs to be converted to linear before operations
 * @param bIsNormalMap - Whether the input data contains normals instead of color.  
 * @param bSupportDXT1a	- Whether to use DXT1a or DXT1 format (if PixelFormat is DXT1)
 * @param QualityLevel - A value from nvtt::Quality that represents the quality of the lightmap encoding
 */
void DXTCompress(UTexture2D* Texture2D, void* SourceData, EPixelFormat PixelFormat, INT SizeX, INT SizeY, UBOOL SRGB, UBOOL bIsNormalMap, UBOOL bSupportDXT1a, INT QualityLevel)
{
	if(PixelFormat == PF_G8)
	{
		// format is not handled by NVTTCompress

		// Add a mip level to Texture2D and copy over the compressed data
		FTexture2DMipMap* MipMap = new(Texture2D->Mips) FTexture2DMipMap;
		MipMap->SizeX = SizeX;
		MipMap->SizeY = SizeY;
		MipMap->Data.Lock(LOCK_READ_WRITE);

		UINT Size = SizeX * SizeY;
		BYTE* DstPtr = (BYTE*)MipMap->Data.Realloc(Size);
		FColor*	Color = (FColor*)SourceData;

		for(UINT i = 0; i < Size; ++i, ++Color)
		{
			*(DstPtr++) = Color->R;
		}

		MipMap->Data.Unlock();
	}
	else if(PixelFormat == PF_V8U8)
	{
		// format is not handled by NVTTCompress

		// Add a mip level to Texture2D and copy over the compressed data
		FTexture2DMipMap* MipMap = new(Texture2D->Mips) FTexture2DMipMap;
		MipMap->SizeX = SizeX;
		MipMap->SizeY = SizeY;
		MipMap->Data.Lock(LOCK_READ_WRITE);

		UINT ElementCount = SizeX * SizeY;
		FColor* RawColor	= (FColor*)SourceData;
		SBYTE*	DestColor	= (SBYTE*)MipMap->Data.Realloc(ElementCount * 2);

		for(UINT i = 0; i < ElementCount; i++)
		{
			*(DestColor++) = (INT)RawColor->R - 128;
			*(DestColor++) = (INT)RawColor->G - 128;
			RawColor++;
		}

		MipMap->Data.Unlock();
	}
	else
	{
		verify(NVTTCompress(Texture2D, SourceData, PixelFormat, SizeX, SizeY, SRGB, bIsNormalMap, GUseCUDAAcceleration, bSupportDXT1a, QualityLevel));
	}
}

/**
 * Initializes the data and creates the event.
 *
 * @param	SourceData		Source texture data to DXT compress, in BGRA 8bit per channel unsigned format.
 * @param	InPixelFormat	Texture format
 * @param	InSizeX			Number of texels along the X-axis
 * @param	InSizeY			Number of texels along the Y-axis
 * @param	SRGB			Whether the texture is in SRGB space
 * @param	bIsNormalMap	Whether the texture is a normal map
 * @param	bSupportDXT1a	Whether to use DXT1a or DXT1 format (if PixelFormat is DXT1)
 * @param	QualityLevel	A value from nvtt::Quality that represents the quality of the lightmap encoding
 */
FAsyncDXTCompress::FAsyncDXTCompress(void* SourceData, EPixelFormat InPixelFormat, INT InSizeX, INT InSizeY, UBOOL SRGB, UBOOL bIsNormalMap,  UBOOL bSupportDXT1a, INT QualityLevel)
:	PixelFormat( InPixelFormat )
,	SizeX( InSizeX )
,	SizeY( InSizeY )
{
	Compression = new FNVCompression( SourceData, PixelFormat, SizeX, SizeY, SRGB, bIsNormalMap, GUseCUDAAcceleration, bSupportDXT1a, QualityLevel );
}

/** Destructor. Frees the memory used for the DXT-compressed resulting data.  */
FAsyncDXTCompress::~FAsyncDXTCompress()
{
	delete Compression;
}
	
/**
 * Performs the async decompression
 */
void FAsyncDXTCompress::DoWork()
{
	verify( Compression->Process() );
}

/**
 * Returns the texture format.
 * @return	Texture format
 */
EPixelFormat FAsyncDXTCompress::GetPixelFormat() const
{
	return PixelFormat;
};

/**
 * Returns the size of the image.
 * @return	Number of texels along the X-axis
 */
INT FAsyncDXTCompress::GetSizeX() const
{
	return SizeX;
}

/**
 * Returns the size of the image.
 * @return	Number of texels along the Y-axis
 */
INT FAsyncDXTCompress::GetSizeY() const
{
	return SizeY;
}

/**
 * Returns the compressed data, once the work is done. This buffer will be deleted when the work is deleted.
 * @return	Start address of the compressed data
 */
const void* FAsyncDXTCompress::GetResultData() const
{
	void* CompressedData = Compression->OutputHandler.CompressedData.GetData();
	return CompressedData;
}

/**
 * Returns the size of compressed data in number of bytes, once the work is done.
 * @return	Size of compressed data, in number of bytes
 */
INT FAsyncDXTCompress::GetResultSize() const
{
	INT NumBytes = Compression->OutputHandler.CompressedData.Num() * Compression->OutputHandler.CompressedData.GetTypeSize();
	return NumBytes;
}

/**
 * Initializes the data and creates the event.
 *
 * @param	SourceData		Source texture data to PVRTC compress, in BGRA 8bit per channel unsigned format.
 * @param	InPixelFormat	Texture format
 * @param	InSizeX			Number of texels along the X-axis
 * @param	InSizeY			Number of texels along the Y-axis
 * @param	InTexture		The texture being compressed
 * @param	InUseFastPVRTC	If true, a fast,low quality compression will be used
 */
FAsyncPVRTCCompressWork::FAsyncPVRTCCompressWork(void* SourceData, INT InSizeX, INT InSizeY, UTexture2D* InTexture, UBOOL InUseFastPVRTC)
		:	Texture( InTexture )
		,	bUseFastPVRTC( InUseFastPVRTC )
		,	SizeX( InSizeX )
		,	SizeY( InSizeY )
{
	if( SourceData )
	{
		// Copy raw data 
		RawData.Add( SizeX * SizeY );
		appMemcpy( RawData.GetData(), SourceData, SizeX * SizeY * sizeof(FColor) );
	}
}

/**
 * Performs the async decompression
 */
void FAsyncPVRTCCompressWork::DoWork()
{
#if WITH_EDITOR
	if( RawData.Num() > 0 )
	{
		// Raw data is in BGRA but we need it to be in RGBA for PVRTC so swap red and blue.
		for( INT RawDataIndex = 0; RawDataIndex <  RawData.Num(); ++RawDataIndex )
		{
			FColor& Color = RawData( RawDataIndex );
			BYTE Tmp = Color.R;
			Color.R = Color.B;
			Color.B = Tmp;
			Color.A = 255;
		}

		// By the time we get here, any existing PVRTC data has been removed, so forcing the compression is not required
		ConditionalCachePVRTCTextures( Texture, bUseFastPVRTC, TRUE, &RawData );
	}
	else
	{
		// By the time we get here, any existing PVRTC data has been removed, so forcing the compression is not required
		ConditionalCachePVRTCTextures( Texture, bUseFastPVRTC, TRUE, NULL );
	}
#endif
}

// Used to sort pending PVRTC tasks based on size so that the slowest (largest size textures) are scheduled first.
INT ComparePVRTCTasks( const FAsyncTask<FAsyncPVRTCCompressWork>& TaskA, const FAsyncTask<FAsyncPVRTCCompressWork>& TaskB )
{
	const FAsyncPVRTCCompressWork* A = &TaskA.GetTask();
	const FAsyncPVRTCCompressWork* B = &TaskB.GetTask();
	// Determine the max squared size of each texture.  PVRTC for mobile must be square
	const INT MaxSizeA =  Max( A->GetSizeX(), A->GetSizeY() );
	const INT SquaredSizeA = MaxSizeA * MaxSizeA;

	const INT MaxSizeB =  Max( B->GetSizeX(), B->GetSizeY() );
	const INT SquaredSizeB = MaxSizeB * MaxSizeB;

	if( SquaredSizeA > SquaredSizeB )
	{
		return -1;
	}
	else if( SquaredSizeA < SquaredSizeB )
	{
		return 1;
	}
	else 
	{
		return 0;
	}
}

typedef FAsyncTask<FAsyncPVRTCCompressWork> FAsyncTask_FAsyncPVRTCCompressWork; // sort macros can't handle ordinary C++ types
IMPLEMENT_COMPARE_CONSTREF( FAsyncTask_FAsyncPVRTCCompressWork, UnTexCompress, { return ComparePVRTCTasks(A,B); });


/** 
 * Adds a texture for compression.  
 * Textures do not necessarily compress in the order they were added.
 *
 * @param TextureToCompress		The texture needing compression
 * @param RawData				Optional RawData to compress if the texture does not contain it.
 */
void FAsyncPVRTCCompressor::AddTexture( UTexture2D* TextureToCompress, UBOOL bUseFastPVRTC, void* RawData /* = NULL */ )
{	
	// If we are here, we are definitely recompressing this texture.  
	// Clear out mip data now when we are still single threaded or else there could be race conditions calling Empty later.
	// Calling empty, on this array deletes the bulk data which calls ULinkerLoad::DetachBulkData which is not thread safe.
	TextureToCompress->CachedPVRTCMips.Empty();

	if( !RawData )
	{
		// loading bulk data is not thread safe so force it to be loaded now
		if( TextureToCompress->SourceArt.GetBulkDataSizeOnDisk() > 0 )
		{
			TextureToCompress->SourceArt.ForceBulkDataResident();
		}
		else
		{
			const UINT TexSizeX = TextureToCompress->SizeX;
			const UINT TexSizeY = TextureToCompress->SizeY;
			const UINT TexSizeSquare = Max(TexSizeX, TexSizeY);

			// the PVRTC compressor can't handle 4096 or bigger textures, so we have to go down in mips and use those
			INT FirstMipIndex = 0;
			while ((TexSizeSquare >> FirstMipIndex) > 2048)
			{
				FirstMipIndex++;
			}

			// Source art is not available so the first mip will be used to convert DXT to PVRTC
			// This cant be loaded from disk from another thread.
			TextureToCompress->Mips(FirstMipIndex).Data.ForceBulkDataResident();
		}
		
	}
	// Add a new compression task to the list of pending tasks
	// Only PVRTC compress top level mip.  The other mips will be generated by the PVR compress tool

	new (PendingWork) FAsyncTask<FAsyncPVRTCCompressWork>(  RawData, TextureToCompress->Mips(0).SizeX, TextureToCompress->Mips(0).SizeY, TextureToCompress, bUseFastPVRTC );
}

/**
 * Compresses all queued textures.  This function blocks until all texture have been compressed
 */
void FAsyncPVRTCCompressor::CompressTextures()
{
	if( PendingWork.Num() > 0 )
	{
		GWarn->StatusUpdatef( -1, -1, *LocalizeUnrealEd("PVRTCCompression_CompressingTextures"));

		// Sort compression tasks based on the size of the texture they compress. We will kick off async tasks in the order of largest to smallest texture which will 
		// minimize the amount of time we wait for all textures to be compressed.
		SortMemswap<USE_COMPARE_CONSTREF(FAsyncTask_FAsyncPVRTCCompressWork,UnTexCompress)>((FAsyncTask<FAsyncPVRTCCompressWork>*)PendingWork.GetData(), PendingWork.Num() );

		// Queue each compression task
		for( INT TaskIndex = 0; TaskIndex < PendingWork.Num(); ++TaskIndex )
		{
			PendingWork(TaskIndex).StartBackgroundTask();
		}

		// Wait for compression tasks to end.
		FinishTasks();

		// All compression tasks have completed
		PendingWork.Empty();
	}
}

/** Waits for all outstanding tasks to finish */
void FAsyncPVRTCCompressor::FinishTasks()
{
	INT NumCompleted = 0;
	INT NumTasks = PendingWork.Num();
	do
	{
		// Check for completed async compression tasks.
		INT NumNowCompleted = 0;
		for ( INT TaskIndex=0; TaskIndex < PendingWork.Num(); ++TaskIndex )
		{
			if ( PendingWork(TaskIndex).IsDone() )
			{
				NumNowCompleted++;
			}
		}
		if (NumNowCompleted > NumCompleted)
		{
			NumCompleted = NumNowCompleted;
			GWarn->StatusUpdatef( NumCompleted, NumTasks, LocalizeSecure( LocalizeUnrealEd( "PVRTCCompression_CompressingTexturesF" ), NumCompleted, NumTasks ) );
		}
		appSleep(0.1f);
	} while ( NumCompleted < NumTasks );
}

#endif   //_MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER

/**
 * Shared compression functionality. 
 */
void UTexture::Compress()
{
	// High dynamic range textures are currently always stored as RGBE (shared exponent) textures.
	RGBE = (CompressionSettings == TC_HighDynamicRange);

	// better this would be data driven, also TextureMipGenSettings::TMGS_None should be introduced
	if(LODGroup == TEXTUREGROUP_ColorLookupTable)
	{
		MipGenSettings = TMGS_NoMipmaps;
		SRGB = FALSE;
		RGBE = FALSE;
	}
}

/** Defines an image's data. */
class FImageData
{
public:

	BYTE* Buffer;
	INT SizeX;
	INT SizeY;
	INT SizeZ;
	INT StrideY;
	INT StrideZ;

	/** Initialization constructor for all types of textures */
	FImageData(BYTE* InBuffer,INT InSizeX,INT InSizeY,INT InSizeZ,INT InStrideY,INT InStrideZ):
		Buffer(InBuffer),
		SizeX(InSizeX),
		SizeY(InSizeY),
		SizeZ(InSizeZ),
		StrideY(InStrideY),
		StrideZ(InStrideZ)
	{}
	/** Initialization constructor for 2d textures */
	FImageData(BYTE* InBuffer,UINT InSizeX,UINT InSizeY,UINT InStrideY):
		Buffer(InBuffer),
			SizeX(InSizeX),
			SizeY(InSizeY),
			SizeZ(1),
			StrideY(InStrideY),
			StrideZ(0)
	{}
};

/** Defines the format of an image. */
class FImageFormat
{
public:

	INT Format;
	UBOOL bSRGB;
	UBOOL bRGBE;

	/** Initialization constructor. */
	FImageFormat(INT InFormat,UBOOL bInSRGB,UBOOL bInRGBE):
		Format(InFormat),
		bSRGB(bInSRGB),
		bRGBE(bInRGBE)
	{}	
};


enum EMipGenAddressMode
{
	MGTAM_Wrap,
	MGTAM_Clamp,
	MGTAM_BorderBlack,
};

// 2D sample lookup with input conversion
// requires SourceImageData.SizeX and SourceImageData.SizeY to be power of two
template <EMipGenAddressMode AddressMode>
FLinearColor LookupSourceMip( const FImageData& SourceImageData, const FImageFormat& ImageFormat, INT X, INT Y )
{
	if(AddressMode == MGTAM_Wrap)
	{
		// wrap
		X = (INT)((UINT)X) & (SourceImageData.SizeX - 1);
		Y = (INT)((UINT)Y) & (SourceImageData.SizeY - 1);
	}
	else if(AddressMode == MGTAM_Clamp)
	{
		// clamp
		X = Clamp(X, 0, SourceImageData.SizeX - 1);
		Y = Clamp(Y, 0, SourceImageData.SizeY - 1);
	}
	else if(AddressMode == MGTAM_BorderBlack)
	{
		// border color 0
		if((UINT)X >= (UINT)SourceImageData.SizeX
		|| (UINT)Y >= (UINT)SourceImageData.SizeY)
		{
			return FLinearColor(0, 0, 0, 0);
		}
	}
	else
	{
		check(0);
	}

	const FColor& SourceColor = *(FColor*)
		(SourceImageData.Buffer +
		X * sizeof(FColor) +
		Y * SourceImageData.StrideY);

	// Transform the source color into a linear color.
	FLinearColor LinearSourceColor;

	if(ImageFormat.bRGBE)
	{
		LinearSourceColor = SourceColor.FromRGBE();
	}
	else if(ImageFormat.bSRGB)
	{
		LinearSourceColor = FLinearColor(SourceColor);
	}
	else
	{
		LinearSourceColor = SourceColor.ReinterpretAsLinear();
	}

	return LinearSourceColor;
}

// Transform the linear color into a 32 bit color
static FColor FormatColor(const FImageFormat& ImageFormat, const FLinearColor InColor)
{
	FColor ret;

	if(ImageFormat.bRGBE)
	{
		ret = InColor.ToRGBE();
	}
	else
	{
		ret = InColor.ToFColor(ImageFormat.bSRGB);
	}

	return ret;
}

// Kernel class for image filtering operations like image downsampling
// at max MaxKernelExtend x MaxKernelExtend
class ImageKernel2D
{
public:
	ImageKernel2D() :FilterTableSize(0)
	{
	}

	// @param TableSize1D 2 for 2x2, 4 for 4x4, 6 for 6x6, 8 for 8x8
	// @param SharpenFactor can be negative to blur
	// generate normalized 2D Kernel with sharpening
	void BuildSeparatableGaussWithSharpen(UINT TableSize1D, FLOAT SharpenFactor = 0.0f)
	{
		if(TableSize1D > MaxKernelExtend)
		{
			TableSize1D = MaxKernelExtend;
		}

		FLOAT Table1D[MaxKernelExtend];
		FLOAT NegativeTable1D[MaxKernelExtend];

		FilterTableSize = TableSize1D;

		if(SharpenFactor < 0.0f)
		{
			// blur only
			BuildGaussian1D(Table1D, TableSize1D, 1.0f, -SharpenFactor);
			BuildFilterTable2DFrom1D(KernelWeights, Table1D, TableSize1D);
			return;
		}
		else if(TableSize1D == 2)
		{
			// 2x2 kernel: simple average
			KernelWeights[0] = KernelWeights[1] = KernelWeights[2] = KernelWeights[3] = 0.25f;
			return;
		}
		else if(TableSize1D == 4)
		{
			// 4x4 kernel with sharpen or blur: can alias a bit
			BuildFilterTable1DBase(Table1D, TableSize1D, 1.0f + SharpenFactor);
			BuildFilterTable1DBase(NegativeTable1D, TableSize1D, -SharpenFactor);
			BlurFilterTable1D(NegativeTable1D, TableSize1D, 1);
		}
		else if(TableSize1D == 6)
		{
			// 6x6 kernel with sharpen or blur: still can alias
			BuildFilterTable1DBase(Table1D, TableSize1D, 1.0f + SharpenFactor);
			BuildFilterTable1DBase(NegativeTable1D, TableSize1D, -SharpenFactor);
			BlurFilterTable1D(NegativeTable1D, TableSize1D, 2);
		}
		else if(TableSize1D == 8)
		{
			//8x8 kernel with sharpen or blur
		
			// * 2 to get similar appearance as for TableSize 6
			SharpenFactor = SharpenFactor * 2.0f;

			BuildFilterTable1DBase(Table1D, TableSize1D, 1.0f + SharpenFactor);
			// positive lobe is blurred a bit for better quality
			BlurFilterTable1D(Table1D, TableSize1D, 1);
			BuildFilterTable1DBase(NegativeTable1D, TableSize1D, -SharpenFactor);
			BlurFilterTable1D(NegativeTable1D, TableSize1D, 3);
		}
		else 
		{
			// not yet supported
			check(0);
		}

		AddFilterTable1D(Table1D, NegativeTable1D, TableSize1D);
		BuildFilterTable2DFrom1D(KernelWeights, Table1D, TableSize1D);
	}

	inline UINT GetFilterTableSize() const
	{
		return FilterTableSize;
	}

	inline FLOAT GetAt(UINT X, UINT Y) const
	{
		checkSlow(X < FilterTableSize);
		checkSlow(Y < FilterTableSize);
		return KernelWeights[X + Y * FilterTableSize];
	}

	inline FLOAT& GetRefAt(UINT X, UINT Y)
	{
		checkSlow(X < FilterTableSize);
		checkSlow(Y < FilterTableSize);
		return KernelWeights[X + Y * FilterTableSize];
	}

private:

	inline static FLOAT NormalDistribution(FLOAT X, FLOAT Variance)
	{
		const FLOAT StandardDeviation = appSqrt(Variance);
		return appExp(-Square(X) / (2.0f * Variance)) / (StandardDeviation * appSqrt(2.0f * (FLOAT)PI));
	}

	static void BuildGaussian1D(FLOAT *InOutTable, UINT TableSize, FLOAT Sum, FLOAT Variance)
	{
		// we require a even sized filter
		check(TableSize % 2 == 0);

		FLOAT Center = TableSize / 2;
		FLOAT CurrentSum = 0;
		for(UINT i = 0; i < TableSize; ++i)
		{
			FLOAT Actual = NormalDistribution(i - Center + 0.5f, Variance);
			InOutTable[i] = Actual;
			CurrentSum += Actual;
		}
		// Normalize
		FLOAT InvSum = Sum / CurrentSum;
		for(UINT i = 0; i < TableSize; ++i)
		{
			InOutTable[i] *= InvSum;
		}
	}

	//
	static void BuildFilterTable1DBase(FLOAT *InOutTable, UINT TableSize, FLOAT Sum )
	{
		// we require a even sized filter
		check(TableSize % 2 == 0);

		FLOAT Inner = 0.5f * Sum;

		UINT Center = TableSize / 2;
		for(UINT x = 0; x < TableSize; ++x)
		{
			if(x == Center || x == Center - 1)
			{
				// center elements
				InOutTable[x] = Inner;
			}
			else
			{
				// outer elements
				InOutTable[x] = 0.0f;
			}
		}
	}

	// InOutTable += InTable
	static void AddFilterTable1D( FLOAT *InOutTable, FLOAT *InTable, UINT TableSize )
	{
		for(UINT x = 0; x < TableSize; ++x)
		{
			InOutTable[x] += InTable[x];
		}
	}

	// @param Times 1:box, 2:triangle, 3:pow2, 4:pow3, ...
	// can be optimized with double buffering but doesn't need to be fast
	static void BlurFilterTable1D( FLOAT *InOutTable, UINT TableSize, UINT Times )
	{
		check(Times>0);
		check(TableSize<32);

		FLOAT Intermediate[32];

		for(UINT Pass = 0; Pass < Times; ++Pass)
		{
			for(UINT x = 0; x < TableSize; ++x)
			{
				Intermediate[x] = InOutTable[x];
			}

			for(UINT x = 0; x < TableSize; ++x)
			{
				FLOAT sum = Intermediate[x];

				if(x)
				{
					sum += Intermediate[x-1];	
				}
				if(x < TableSize - 1)
				{
					sum += Intermediate[x+1];	
				}

				InOutTable[x] = sum / 3.0f;
			}
		}
	}

	static void BuildFilterTable2DFrom1D( FLOAT *OutTable2D, FLOAT *InTable1D, UINT TableSize )
	{
		for(UINT y = 0; y < TableSize; ++y)
		{
			for(UINT x = 0; x < TableSize; ++x)
			{
				OutTable2D[x + y * TableSize] = InTable1D[y] * InTable1D[x];
			}
		}
	}

	// at max we support MaxKernelExtend x MaxKernelExtend kernels
	const static UINT MaxKernelExtend = 12;
	// 0 if no kernel was setup yet
	UINT FilterTableSize;
	// normalized, means the sum of it should be 1.0f
	FLOAT KernelWeights[MaxKernelExtend * MaxKernelExtend];
};


/**
* Generates a mip-map for an 2D A8R8G8B8 image using a 4x4 filter with sharpening
* @param SourceImageData - The source image's data.
* @param DestImageData - The destination image's data.
* @param ImageFormat - The format of both the source and destination images.
* @param FilterTable2D - [FilterTableSize * FilterTableSize]
* @param FilterTableSize - >= 2
* @param bPreserveBorderR - If TRUE, preserve color in border pixels.
* @param bPreserveBorderG - If TRUE, preserve color in border pixels.
* @param bPreserveBorderB - If TRUE, preserve color in border pixels.
* @param bPreserveBorderA - If TRUE, preserve color in border pixels.
*/
template <EMipGenAddressMode AddressMode>
static void GenerateSharpenedMipA8R8G8B8Templ(
	const FImageData& SourceImageData, 
	const FImageData& DestImageData, 
	const FImageFormat& ImageFormat, 
	UBOOL bDitherMipMapAlpha,
	const ImageKernel2D &Kernel,
	UBOOL bSharpenWithoutColorShift)
{
	check(SourceImageData.Buffer != DestImageData.Buffer);
	check(SourceImageData.SizeX == 2 * DestImageData.SizeX || DestImageData.SizeX == 1);
	check(SourceImageData.SizeY == 2 * DestImageData.SizeY || DestImageData.SizeY == 1);
	check(DestImageData.SizeZ == 1);
	check(Kernel.GetFilterTableSize() >= 2);

	const INT KernelCenter = (INT)Kernel.GetFilterTableSize() / 2 - 1;

	// Set up a random number stream for dithering.
	FRandomStream RandomStream(0);

	for(INT DestY = 0;DestY < DestImageData.SizeY; DestY++)
	{
		for(INT DestX = 0;DestX < DestImageData.SizeX; DestX++)
		{
			const INT SourceX = DestX * 2;
			const INT SourceY = DestY * 2;

			FLinearColor FilteredColor(0, 0, 0, 0);

			if(bSharpenWithoutColorShift)
			{
				FLOAT NewLuminance = 0;

				for(UINT KernelY = 0; KernelY < Kernel.GetFilterTableSize();  ++KernelY)
				{
					for(UINT KernelX = 0; KernelX < Kernel.GetFilterTableSize();  ++KernelX)
					{
						FLOAT Weight = Kernel.GetAt(KernelX, KernelY);
						FLinearColor Sample = LookupSourceMip<AddressMode>(SourceImageData, ImageFormat, SourceX + KernelX - KernelCenter, SourceY + KernelY - KernelCenter);
						FLOAT LuminanceSample = Sample.ComputeLuminance();

						NewLuminance += Weight * LuminanceSample;
					}
				}

				// simple 2x2 kernel to compute the color
				FilteredColor =
					( LookupSourceMip<AddressMode>(SourceImageData, ImageFormat, SourceX + 0, SourceY + 0)
					+ LookupSourceMip<AddressMode>(SourceImageData, ImageFormat, SourceX + 1, SourceY + 0)
					+ LookupSourceMip<AddressMode>(SourceImageData, ImageFormat, SourceX + 0, SourceY + 1)
					+ LookupSourceMip<AddressMode>(SourceImageData, ImageFormat, SourceX + 1, SourceY + 1) ) * 0.25f;

				FLOAT OldLuminance = FilteredColor.ComputeLuminance();

				if(OldLuminance > 0.001f)
				{
					FLOAT Factor = NewLuminance / OldLuminance;

					FilteredColor.R *= Factor;
					FilteredColor.G *= Factor;
					FilteredColor.B *= Factor;
				}
			}
			else
			{
				for(UINT KernelY = 0; KernelY < Kernel.GetFilterTableSize();  ++KernelY)
				{
					for(UINT KernelX = 0; KernelX < Kernel.GetFilterTableSize();  ++KernelX)
					{
						FLOAT Weight = Kernel.GetAt(KernelX, KernelY);
						FLinearColor Sample = LookupSourceMip<AddressMode>(SourceImageData, ImageFormat, SourceX + KernelX - KernelCenter, SourceY + KernelY - KernelCenter);

						FilteredColor += Weight	* Sample;
					}
				}
			}

			FColor FormattedColor = FormatColor(ImageFormat, FilteredColor);

			if(bDitherMipMapAlpha)
			{
				// Dither the alpha of any pixel which passes an alpha threshold test.
				static const INT AlphaThreshold = 5;
				static const FLOAT MinRandomAlpha = 85.0f;
				static const FLOAT MaxRandomAlpha = 255.0f;

				if(FormattedColor.A > AlphaThreshold)
				{
					FormattedColor.A = appTrunc(Lerp(MinRandomAlpha, MaxRandomAlpha, RandomStream.GetFraction()));
				}
			}

			// Set the destination pixel.
			FColor& DestColor = *(FColor*)
				(DestImageData.Buffer +
				DestX * sizeof(FColor) +
				DestY * DestImageData.StrideY);

			DestColor = FormattedColor;
		}
	}
}

// to switch conveniently beween different texture wrapping modes for the mip map generation
// the template can optimize the inner loop using a constant AddressMode
static void GenerateSharpenedMipA8R8G8B8(
	EMipGenAddressMode AddressMode, 
	const FImageData& SourceImageData, 
	const FImageData& DestImageData, 
	const FImageFormat& ImageFormat, 
	UBOOL bDitherMipMapAlpha,
	const ImageKernel2D &Kernel,
	UBOOL bSharpenWithoutColorShift)
{
	switch(AddressMode)
	{
		case MGTAM_Wrap:
			GenerateSharpenedMipA8R8G8B8Templ<MGTAM_Wrap>(SourceImageData, DestImageData, ImageFormat, bDitherMipMapAlpha, Kernel, bSharpenWithoutColorShift);
			break;
		case MGTAM_Clamp:
			GenerateSharpenedMipA8R8G8B8Templ<MGTAM_Clamp>(SourceImageData, DestImageData, ImageFormat, bDitherMipMapAlpha, Kernel, bSharpenWithoutColorShift);
			break;
		case MGTAM_BorderBlack:
			GenerateSharpenedMipA8R8G8B8Templ<MGTAM_BorderBlack>(SourceImageData, DestImageData, ImageFormat, bDitherMipMapAlpha, Kernel, bSharpenWithoutColorShift);
			break;
		default:
			check(0);
	}
}

/**
* Generates a mip-map for an 2D A8R8G8B8 image using a 4x4 filter with sharpening
* @param SourceImageData - The source image's data.
* @param DestImageData - The destination image's data.
* @param ImageFormat - The format of both the source and destination images.
* @param FilterTable2D - [FilterTableSize * FilterTableSize]
* @param FilterTableSize - >= 2
* @param bPreserveBorderR - If TRUE, preserve color in border pixels.
* @param bPreserveBorderG - If TRUE, preserve color in border pixels.
* @param bPreserveBorderB - If TRUE, preserve color in border pixels.
* @param bPreserveBorderA - If TRUE, preserve color in border pixels.
*/
template <EMipGenAddressMode AddressMode>
static void GenerateSharpenedScaledMipA8R8G8B8Templ(
	const FImageData& SourceImageData, 
	const FImageData& DestImageData, 
	const FImageFormat& ImageFormat, 
	UBOOL bDitherMipMapAlpha,
	const ImageKernel2D &Kernel,
	UBOOL bSharpenWithoutColorShift,
	const FVector2D& Scale)
{
#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	check(SourceImageData.Buffer != DestImageData.Buffer);
	//check(SourceImageData.SizeX == 2 * DestImageData.SizeX || DestImageData.SizeX == 1);
	//check(SourceImageData.SizeY == 2 * DestImageData.SizeY || DestImageData.SizeY == 1);
	check(DestImageData.SizeZ == 1);
	check(Kernel.GetFilterTableSize() >= 2);

	const INT KernelCenter = (INT)Kernel.GetFilterTableSize() / 2 - 1;

	// Set up a random number stream for dithering.
	FRandomStream RandomStream(0);

	FLOAT fInvSizeX = (FLOAT)SourceImageData.SizeX / (FLOAT)DestImageData.SizeX;
	FLOAT fInvSizeY = (FLOAT)SourceImageData.SizeY / (FLOAT)DestImageData.SizeY;

	for(INT DestY = 0; DestY < DestImageData.SizeY; DestY++)
	{
		for(INT DestX = 0; DestX < DestImageData.SizeX; DestX++)
		{
			const FLOAT fSourceX = (FLOAT)DestX * fInvSizeX;
			const FLOAT fSourceY = (FLOAT)DestY * fInvSizeY;
			const INT iSourceX = appRound(fSourceX);
			const INT iSourceY = appRound(fSourceY);
			const FLOAT fWeightX = fSourceX - iSourceX;
			const FLOAT fWeightY = fSourceY - iSourceY;

			FLinearColor FilteredColor(0, 0, 0, 0);

			if(bSharpenWithoutColorShift)
			{
				FLOAT NewLuminance = 0;

				for(UINT KernelY = 0; KernelY < Kernel.GetFilterTableSize();  ++KernelY)
				{
					for(UINT KernelX = 0; KernelX < Kernel.GetFilterTableSize();  ++KernelX)
					{
						FLOAT Weight = Kernel.GetAt(KernelX, KernelY);
						FLinearColor Sample = LookupSourceMip<AddressMode>(SourceImageData, ImageFormat, iSourceX + KernelX - KernelCenter, iSourceY + KernelY - KernelCenter);
						FLOAT LuminanceSample = Sample.ComputeLuminance();

						NewLuminance += Weight * LuminanceSample;
					}
				}

				// simple 2x2 kernel to compute the color
				FilteredColor =
					( LookupSourceMip<AddressMode>(SourceImageData, ImageFormat, iSourceX + 0, iSourceY + 0)
					+ LookupSourceMip<AddressMode>(SourceImageData, ImageFormat, iSourceX + 1, iSourceY + 0)
					+ LookupSourceMip<AddressMode>(SourceImageData, ImageFormat, iSourceX + 0, iSourceY + 1)
					+ LookupSourceMip<AddressMode>(SourceImageData, ImageFormat, iSourceX + 1, iSourceY + 1) ) * 0.25f;

				FLOAT OldLuminance = FilteredColor.ComputeLuminance();

				if(OldLuminance > 0.001f)
				{
					FLOAT Factor = NewLuminance / OldLuminance;

					FilteredColor.R *= Factor;
					FilteredColor.G *= Factor;
					FilteredColor.B *= Factor;
				}
			}
			else
			{
				for(UINT KernelY = 0; KernelY < Kernel.GetFilterTableSize();  ++KernelY)
				{
					for(UINT KernelX = 0; KernelX < Kernel.GetFilterTableSize();  ++KernelX)
					{
						FLOAT Weight = Kernel.GetAt(KernelX, KernelY);
						FLinearColor Sample = LookupSourceMip<AddressMode>(SourceImageData, ImageFormat, iSourceX + KernelX - KernelCenter, iSourceY + KernelY - KernelCenter);

						FilteredColor += Weight	* Sample;
					}
				}
			}

			FColor FormattedColor = FormatColor(ImageFormat, FilteredColor);

			if(bDitherMipMapAlpha)
			{
				// Dither the alpha of any pixel which passes an alpha threshold test.
				static const INT AlphaThreshold = 5;
				static const FLOAT MinRandomAlpha = 85.0f;
				static const FLOAT MaxRandomAlpha = 255.0f;

				if(FormattedColor.A > AlphaThreshold)
				{
					FormattedColor.A = appTrunc(Lerp(MinRandomAlpha, MaxRandomAlpha, RandomStream.GetFraction()));
				}
			}

			// Set the destination pixel.
			FColor& DestColor = *(FColor*)
				(DestImageData.Buffer +
				DestX * sizeof(FColor) +
				DestY * DestImageData.StrideY);

			DestColor = FormattedColor;
		}
	}
#endif
}

// to switch conveniently beween different texture wrapping modes for the mip map generation
// the template can optimize the inner loop using a constant AddressMode
static void GenerateSharpenedScaledMipA8R8G8B8(
	EMipGenAddressMode AddressMode, 
	const FImageData& SourceImageData, 
	const FImageData& DestImageData, 
	const FImageFormat& ImageFormat, 
	UBOOL bDitherMipMapAlpha,
	const ImageKernel2D &Kernel,
	UBOOL bSharpenWithoutColorShift,
	FVector2D Scale = FVector2D(1.f,1.f))
{
#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	switch(AddressMode)
	{
		case MGTAM_Wrap:
			GenerateSharpenedScaledMipA8R8G8B8Templ<MGTAM_Wrap>(SourceImageData, DestImageData, ImageFormat, bDitherMipMapAlpha, Kernel, bSharpenWithoutColorShift, Scale);
			break;
		case MGTAM_Clamp:
			GenerateSharpenedScaledMipA8R8G8B8Templ<MGTAM_Clamp>(SourceImageData, DestImageData, ImageFormat, bDitherMipMapAlpha, Kernel, bSharpenWithoutColorShift, Scale);
			break;
		case MGTAM_BorderBlack:
			GenerateSharpenedScaledMipA8R8G8B8Templ<MGTAM_BorderBlack>(SourceImageData, DestImageData, ImageFormat, bDitherMipMapAlpha, Kernel, bSharpenWithoutColorShift, Scale);
			break;
		default:
			check(0);
	}
#endif
}


// Update border texels after normal mip map generation to preserve the colors there (useful for particles and decals).
static void GenerateMipBorder(
	const FImageData& SourceImageData, 
	const FImageData& DestImageData, 
	const FImageFormat& ImageFormat)
{
	check(SourceImageData.Buffer != DestImageData.Buffer);
	check(SourceImageData.SizeX == 2 * DestImageData.SizeX || DestImageData.SizeX == 1);
	check(SourceImageData.SizeY == 2 * DestImageData.SizeY || DestImageData.SizeY == 1);
	check(DestImageData.SizeZ == 1);

	for(INT DestY = 0;DestY < DestImageData.SizeY; DestY++)
	{
		for(INT DestX = 0;DestX < DestImageData.SizeX;)
		{
			FLinearColor FilteredColor(0, 0, 0, 0);
			{
				FLOAT WeightSum = 0;
				for(INT KernelY = 0; KernelY < 2;  ++KernelY)
				{
					for(INT KernelX = 0; KernelX < 2;  ++KernelX)
					{
						const INT SourceX = DestX * 2 + KernelX;
						const INT SourceY = DestY * 2 + KernelY;

						// only average the source border
						if(SourceX == 0
						|| SourceX == SourceImageData.SizeX - 1
						|| SourceY == 0
						|| SourceY == SourceImageData.SizeY - 1)
						{
							FLinearColor Sample = LookupSourceMip<MGTAM_Wrap>(SourceImageData, ImageFormat, SourceX, SourceY);

							FilteredColor += Sample;
							++WeightSum;
						}
					}
				}
				FilteredColor /= WeightSum;
			}

			FColor FormattedColor = FormatColor(ImageFormat, FilteredColor);

			// Set the destination pixel.
			FColor& DestColor = *(FColor*)
				(DestImageData.Buffer +
				DestX * sizeof(FColor) +
				DestY * DestImageData.StrideY);

			DestColor = FormattedColor;

			++DestX;

			if(DestY > 0
			&& DestY < DestImageData.SizeY - 1
			&& DestX > 0
			&& DestX < DestImageData.SizeX - 1)
			{
				// jump over the non border area
				DestX += Max(1, DestImageData.SizeX - 2);
			}
		}
	}
}

// required to pass data between mip-map generation and compression
struct IntermediateMipChain
{
	// data needs to be allocated with new[] ()
	TArray<FImageData> Mips;

	~IntermediateMipChain()
	{
		UINT MipCount = Mips.Num();
		for(UINT MipIndex = 0; MipIndex < MipCount; ++MipIndex)
		{
			delete [] Mips(MipIndex).Buffer;
		}
	}
};

// @return TRUE if the case was processed, FALSE if the fallback needs to kick in
static UBOOL GenerateMipChainBaseFromRaw(
	UTexture2D &Texture,
	IntermediateMipChain &OutMipChain,
	UINT *RawColors,
	UINT SrcWidth,
	UINT SrcHeight)
{
	check(RawColors);
	check(SrcWidth);
	check(SrcHeight);


	UBOOL bAllowNonPowerOfTwo = FALSE;
	GConfig->GetBool( TEXT("TextureImporter"), TEXT("AllowNonPowerOfTwoTextures"), bAllowNonPowerOfTwo, GEditorIni );

	if( !bAllowNonPowerOfTwo && ( !appIsPowerOfTwo(SrcWidth) || !appIsPowerOfTwo(SrcHeight) ) )
	{
		// If we aren't allowing NPT textures return now.
		return FALSE;
	}

	// create base for the mip chain
	OutMipChain.Mips.AddItem(FImageData(new BYTE[SrcWidth * SrcHeight * sizeof(FColor)], SrcWidth, SrcHeight, SrcWidth * sizeof(FColor)));

	// copy base source content to the base of the mip chain
	appMemcpy(OutMipChain.Mips(0).Buffer, RawColors, SrcWidth * SrcHeight * sizeof(FColor));

	// Apply modifications to the source mip
	{
		// Apply color adjustments
		{
			FColorAdjustmentParameters ColorAdjustParams;
			ColorAdjustParams.AdjustBrightness = Texture.AdjustBrightness;
			ColorAdjustParams.AdjustBrightnessCurve = Texture.AdjustBrightnessCurve;
			ColorAdjustParams.AdjustSaturation = Texture.AdjustSaturation;
			ColorAdjustParams.AdjustVibrance = Texture.AdjustVibrance;
			ColorAdjustParams.AdjustRGBCurve = Texture.AdjustRGBCurve;
			ColorAdjustParams.AdjustHue = Texture.AdjustHue;

			FColor* Color = (FColor*)OutMipChain.Mips(0).Buffer;
			FImageUtils::AdjustImageColors( Color, SrcWidth, SrcHeight, Texture.SRGB, ColorAdjustParams );

			if(Texture.LODGroup == TEXTUREGROUP_Bokeh)
			{
				// To get the occlusion in the BokehDOF shader working for all Bokeh textures.
				FImageUtils::ComputeBokehAlpha(Color, SrcWidth, SrcHeight, Texture.SRGB);
			}
		}

		if(Texture.CompressionSettings == TC_OneBitAlpha)
		{
			// threshold the alpha channel
			FColor* Color = (FColor*)OutMipChain.Mips(0).Buffer;
			for(UINT i = 0; i < SrcWidth * SrcHeight; ++i)
			{
				if(Color->A < 10)
				{
					Color->A = 0;
				}
				else
				{
					Color->A = 255;
				}
				++Color;
			}
		}
	}

	return TRUE;
}

// @param OutMipChain			existing mip is the input, new mips are getting added
static void GenerateMipChain(UTexture2D &Texture, IntermediateMipChain &OutMipChain)
{
	check(OutMipChain.Mips.Num() == 1);

	const UINT SrcWidth = OutMipChain.Mips(0).SizeX;
	const UINT SrcHeight= OutMipChain.Mips(0).SizeY;
	FImageFormat Format(Texture.Format, Texture.SRGB, Texture.RGBE);

	TArray<FColor> IntermediateSrc;
	TArray<FColor> IntermediateDst;

	// space for one source mip and one destination mip
	IntermediateSrc.Add(SrcWidth * SrcHeight);
	IntermediateDst.Add((SrcWidth + 1) / 2 * (SrcHeight + 1) / 2);

	// copy base mip
	appMemcpy(IntermediateSrc.GetData(), OutMipChain.Mips(0).Buffer, SrcWidth * SrcHeight * sizeof(FColor));

	ImageKernel2D KernelSimpleAverage;

	KernelSimpleAverage.BuildSeparatableGaussWithSharpen(2);

	// sharpening can be negative to blur
	FLOAT MipSharpening;
	// size e.g. 8 for a large kernel with blor or sharpening, 2 for the default down sampling kernel
	UINT SharpenMipKernelSize;
	// TRUE:downsample with 2x2 average (default, avoids oversharpening during downsample), FALSE: use the other kernel (used for blur to propagate strong blur donw the mips)
	UBOOL bDownsampleWithAverage;
	// avoiding the color shift assumes we deal with colors which is not true for normalmaps
	// or we blur where it's good to blur the color as well
	UBOOL bSharpenWithoutColorShift;
	// TRUE: in case we use "preserve border" but we don't want wrap but we want to use black outside
	UBOOL bOutBorderColorBlack;

	GSystemSettings.TextureLODSettings.GetMipGenSettings(Texture, MipSharpening, SharpenMipKernelSize, bDownsampleWithAverage, bSharpenWithoutColorShift, bOutBorderColorBlack);

	ImageKernel2D KernelDownsample;

	KernelDownsample.BuildSeparatableGaussWithSharpen(SharpenMipKernelSize, MipSharpening);

	UBOOL bPreserveBorder = Texture.bPreserveBorderA || Texture.bPreserveBorderR || Texture.bPreserveBorderG || Texture.bPreserveBorderB;

	// TRUE: preserve the border color
	UBOOL bReDrawBorder = FALSE;

	// how should be treat lookups outside of the image
	EMipGenAddressMode AddressMode = MGTAM_Wrap;
	if(bPreserveBorder)
	{
		AddressMode = bOutBorderColorBlack ? MGTAM_BorderBlack : MGTAM_Clamp;
		bReDrawBorder = !bOutBorderColorBlack;
	}

	// generate OutMipChain
	{
		UINT MipIndex = 1;
		for(;;)
		{
			const UINT SrcSizeX = Max((UINT)1, (UINT)(SrcWidth >> (MipIndex - 1)));
			const UINT SrcSizeY = Max((UINT)1, (UINT)(SrcHeight >> (MipIndex - 1)));
			const UINT DestSizeX = Max((UINT)1, (UINT)(SrcWidth >> MipIndex));
			const UINT DestSizeY = Max((UINT)1, (UINT)(SrcHeight >> MipIndex));
			const SIZE_T DestImageSize = CalculateImageBytes(DestSizeX, DestSizeY, 0, PF_A8R8G8B8);

			check(DestImageSize > 0);
			OutMipChain.Mips.AddItem(FImageData(new BYTE[DestImageSize], DestSizeX, DestSizeY, DestSizeX * sizeof(FColor)));

			FImageData DestImage(OutMipChain.Mips(MipIndex).Buffer, DestSizeX, DestSizeY, 1, DestSizeX * sizeof(FColor),0);
			FImageData IntermediateSrcImage((BYTE *)IntermediateSrc.GetData(), SrcSizeX, SrcSizeY, 1, SrcSizeX * sizeof(FColor),0);
			FImageData IntermediateDstImage((BYTE *)IntermediateDst.GetData(), DestSizeX, DestSizeY, 1, DestSizeX * sizeof(FColor),0);

			// generate DestImage: down sample with sharpening
			GenerateSharpenedMipA8R8G8B8(AddressMode, IntermediateSrcImage, DestImage, Format, Texture.bDitherMipMapAlpha, KernelDownsample, bSharpenWithoutColorShift);

			// generate IntermediateDstImage:
			if(bDownsampleWithAverage)
			{
				//  down sample without sharpening for the next iteration
				GenerateSharpenedMipA8R8G8B8(AddressMode, IntermediateSrcImage, IntermediateDstImage, Format, Texture.bDitherMipMapAlpha, KernelSimpleAverage, bSharpenWithoutColorShift);
			}
			else
			{
				appMemcpy(IntermediateDstImage.Buffer, DestImage.Buffer, DestSizeX * DestSizeY * sizeof(FColor));
			}

			if(bReDrawBorder)
			{
				GenerateMipBorder(IntermediateSrcImage, DestImage, Format);
				GenerateMipBorder(IntermediateSrcImage, IntermediateDstImage, Format);
			}

			// Once we've created mip-maps down to 1x1, we're done.
			if(DestSizeX == 1 && DestSizeY == 1)
			{
				break;
			}

			++MipIndex;

			// last destination becomes next source
			appMemcpy(IntermediateSrc.GetData(), IntermediateDst.GetData(), DestSizeX * DestSizeY * sizeof(FColor));
		}
	}
}

// @param OutMipChain			existing mip is the input, resize to scale
static void GenerateScaledMip(UTexture2D &Texture, IntermediateMipChain &OutMipChain, const FVector2D& Scale)
{
#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	check(OutMipChain.Mips.Num() == 1);

	const UINT SrcWidth = OutMipChain.Mips(0).SizeX;
	const UINT SrcHeight= OutMipChain.Mips(0).SizeY;
	FImageFormat Format(Texture.Format, Texture.SRGB, Texture.RGBE);

	TArray<FColor> IntermediateSrc;

	// space for one source mip and one destination mip
	IntermediateSrc.Add(SrcWidth * SrcHeight);

	// copy base mip
	appMemcpy(IntermediateSrc.GetData(), OutMipChain.Mips(0).Buffer, SrcWidth * SrcHeight * sizeof(FColor));

	// Remove all the current mip data
	OutMipChain.Mips.Empty();

	ImageKernel2D KernelSimpleAverage;

	KernelSimpleAverage.BuildSeparatableGaussWithSharpen(2);

	// sharpening can be negative to blur
	FLOAT MipSharpening;
	// size e.g. 8 for a large kernel with blor or sharpening, 2 for the default down sampling kernel
	UINT SharpenMipKernelSize;
	// TRUE:downsample with 2x2 average (default, avoids oversharpening during downsample), FALSE: use the other kernel (used for blur to propagate strong blur donw the mips)
	UBOOL bDownsampleWithAverage;
	// avoiding the color shift assumes we deal with colors which is not true for normalmaps
	// or we blur where it's good to blur the color as well
	UBOOL bSharpenWithoutColorShift;
	// TRUE: in case we use "preserve border" but we don't want wrap but we want to use black outside
	UBOOL bOutBorderColorBlack;

	GSystemSettings.TextureLODSettings.GetMipGenSettings(Texture, MipSharpening, SharpenMipKernelSize, bDownsampleWithAverage, bSharpenWithoutColorShift, bOutBorderColorBlack);

	ImageKernel2D KernelDownsample;

	KernelDownsample.BuildSeparatableGaussWithSharpen(SharpenMipKernelSize, MipSharpening);

	UBOOL bPreserveBorder = Texture.bPreserveBorderA || Texture.bPreserveBorderR || Texture.bPreserveBorderG || Texture.bPreserveBorderB;

	// how should be treat lookups outside of the image
	EMipGenAddressMode AddressMode = MGTAM_Clamp;
	if(bPreserveBorder)
	{
		AddressMode = bOutBorderColorBlack ? MGTAM_BorderBlack : MGTAM_Clamp;
	}

	// generate scaled texture data
	{
		const UINT SrcSizeX = SrcWidth;
		const UINT SrcSizeY = SrcHeight;
		const UINT DestSizeX = Max((UINT)2, (UINT)(SrcWidth * Scale.X));
		const UINT DestSizeY = Max((UINT)2, (UINT)(SrcHeight * Scale.Y));
		const SIZE_T DestImageSize = CalculateImageBytes(DestSizeX, DestSizeY, 0, PF_A8R8G8B8);

		check(DestImageSize > 0);
		OutMipChain.Mips.AddItem(FImageData(new BYTE[DestImageSize], DestSizeX, DestSizeY, DestSizeX * sizeof(FColor)));

		FImageData DestImage(OutMipChain.Mips(0).Buffer, DestSizeX, DestSizeY, 1, DestSizeX * sizeof(FColor),0);
		FImageData SrcImage((BYTE *)IntermediateSrc.GetData(), SrcSizeX, SrcSizeY, 1, SrcSizeX * sizeof(FColor),0);

		if(bDownsampleWithAverage)
		{
			// generate DestImage: down sample without sharpening
			GenerateSharpenedScaledMipA8R8G8B8(AddressMode, SrcImage, DestImage, Format, Texture.bDitherMipMapAlpha, KernelSimpleAverage, bSharpenWithoutColorShift, Scale);
		}
		else
		{
			// generate DestImage: down sample with sharpening
			GenerateSharpenedScaledMipA8R8G8B8(AddressMode, SrcImage, DestImage, Format, Texture.bDitherMipMapAlpha, KernelDownsample, bSharpenWithoutColorShift, Scale);
		}
	}
#endif
}

// compress mip-maps in InMipChain and add mips to Texture, might alter the source content
static void CompressMipChainToTexture(UTexture2D &Texture, IntermediateMipChain &InMipChain)
{
#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	UBOOL bIsNormalMap	= 
		   (Texture.CompressionSettings == TC_Normalmap) 
		|| (Texture.CompressionSettings == TC_NormalmapAlpha) 
		|| (Texture.CompressionSettings == TC_NormalmapBC5);
	UBOOL bReplicateAlpha = FALSE;
	UBOOL bReplicateRed = FALSE;

	EPixelFormat PixelFormat = PF_Unknown;

	UBOOL bNoCompression = Texture.CompressionNone;

	// this would be better data driven
	if(Texture.LODGroup == TEXTUREGROUP_ColorLookupTable)
	{
		bNoCompression = TRUE;
	}

	// this would be better data driven
	if(Texture.LODGroup == TEXTUREGROUP_Bokeh)
	{
		bNoCompression = TRUE;
	}

	if(Texture.CompressionSettings == TC_Displacementmap)
	{
		PixelFormat = PF_G8;
		bReplicateAlpha = TRUE;
	}
	else if(Texture.CompressionSettings == TC_VectorDisplacementmap)
	{
		PixelFormat = PF_A8R8G8B8;
	}
	else if(Texture.CompressionSettings == TC_Grayscale)
	{
		PixelFormat = PF_G8;
		bReplicateRed = TRUE;
	}
	else if( bNoCompression ||
		(Texture.CompressionSettings == TC_HighDynamicRange && Texture.CompressionFullDynamicRange) ||
		(Texture.CompressionSettings == TC_SimpleLightmapModification))
	{
		// Certain textures (icons in Editor) need to be accessed by code so we can't compress them.
		PixelFormat = PF_A8R8G8B8;
	}
	else if(Texture.CompressionSettings == TC_NormalmapUncompressed)
	{
		PixelFormat = PF_V8U8;
	}
	else if(Texture.CompressionSettings == TC_NormalmapBC5)
	{
		// BC5 (DXN/3Dc)
		PixelFormat = PF_BC5;
	}
	else if(Texture.RGBE)
	{
		// DXT3's explicit 4 bit alpha works well with RGBE textures as we can limit the exponent to 4 bit.
		PixelFormat = PF_DXT3;
	}
	else
	{
		// we only look at the source mip
		const FImageData &SrcMip = InMipChain.Mips(0);
		UBOOL bOpaque = TRUE;

		if (Texture.LODGroup == TEXTUREGROUP_ImageBasedReflection)
		{
			// Textures used for image reflections all need to be the same format since they are used in a texture array,
			// So don't treat the texture as opaque even if all the alpha values are 255.
			bOpaque = FALSE;
		}
		// Artists sometimes have alpha channel in source art though don't want to use it.
		else if(!(Texture.CompressionNoAlpha || Texture.CompressionSettings == TC_Normalmap || 
			Texture.CompressionSettings == TC_NormalmapBC5 || Texture.CompressionSettings == TC_OneBitAlpha))
		{
			if(Texture.bDitherMipMapAlpha)
			{
				// bDitherMipMapAlpha adds noise to the alpha channel when enabled	
				bOpaque = FALSE;
			}
			else
			{
				// Figure out whether texture is opaque or not.
				FColor*	Color = (FColor*)SrcMip.Buffer;
				for(INT y = 0; y < SrcMip.SizeY; y++)
				{
					for(INT x = 0; x < SrcMip.SizeX; x++)
					{
						if((Color++)->A != 255)
						{
							bOpaque = FALSE;
							break;
						}
					}
				}
			}
		}

		// DXT1 if opaque (or override) and DXT5 otherwise. DXT3 is only suited for masked textures though DXT5 works fine for this purpose as well.
		PixelFormat = bOpaque ? PF_DXT1 : PF_DXT5;
	}
	
	check(PixelFormat != PF_Unknown);

	PixelFormat = UTexture2D::GetEffectivePixelFormat(PixelFormat, Texture.SRGB);

	// if required, alter the source content
	{
		UINT MipCount = InMipChain.Mips.Num();
		for(UINT MipIndex = 0; MipIndex < MipCount; ++MipIndex)
		{
			const FImageData &SrcMip = InMipChain.Mips(MipIndex);
			FColor*	Color = (FColor*)SrcMip.Buffer;

			// We need to fiddle with the exponent for RGBE textures.
			if(Texture.CompressionSettings == TC_HighDynamicRange && Texture.RGBE)
			{
				check(PixelFormat == PF_DXT3);

				// Clamp exponent to -8, 7 range, translate into 0..15 and shift into most significant bits so compressor doesn't throw the data away.
				for(UINT i = 0; i < (UINT)(SrcMip.SizeX) * (UINT)(SrcMip.SizeY); ++i, ++Color)
				{
					Color->A = (Clamp(Color->A - 128, -8, 7) + 8) * 16;
				}
			}

			// replicate red to the other channels
			if(bReplicateRed)
			{
				for(UINT i = 0; i < (UINT)(SrcMip.SizeX) * (UINT)(SrcMip.SizeY); ++i, ++Color)
				{
					*Color = FColor(Color->R, Color->R, Color->R, Color->R);
				}
			}

			// replicate alpha to the other channels
			if(bReplicateAlpha)
			{
				for(UINT i = 0; i < (UINT)(SrcMip.SizeX) * (UINT)(SrcMip.SizeY); ++i, ++Color)
				{
					*Color = FColor(Color->A, Color->A, Color->A, Color->A);
				}
			}

		}
	}

	// convert to the destination format
	{
		Texture.Format = PixelFormat;

		UINT MipCount = InMipChain.Mips.Num();
		for(UINT MipIndex = 0; MipIndex < MipCount; ++MipIndex)
		{
			FImageData &SrcMip = InMipChain.Mips(MipIndex);

			DXTCompress(&Texture, (BYTE*)SrcMip.Buffer, PixelFormat, SrcMip.SizeX, SrcMip.SizeY, Texture.SRGB, bIsNormalMap, Texture.CompressionSettings == TC_OneBitAlpha, nvtt::Quality_Production);
		}
	}
#endif // _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
}

void UTexture2D::Compress()
{
#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	Super::Compress();

	switch( Format )
	{
	case PF_A8R8G8B8:
	case PF_G8:
	case PF_DXT1:
	case PF_DXT3:
	case PF_DXT5:
	case PF_BC5:
	case PF_V8U8:
	case PF_A1:
		// Handled formats, break.
		break;

	case PF_Unknown:
	case PF_A32B32G32R32F:
	case PF_G16:
	default:
		// Unhandled, return.
		return;
	}

	// Return if no source art is present (maybe old package), and it's not RGBA8 (which we can just use directly)
	if( !HasSourceArt() )
	{
		return;
	}

	// Do not touch textures with programatically generated mip data, eg landscape heightmaps
	if( CompressionNone && (TextureMipGenSettings)MipGenSettings==TMGS_LeaveExistingMips )
	{
		return;
	}

	// Restore the size to that of the source art.
	SizeX = OriginalSizeX;
	SizeY = OriginalSizeY;

	// Remove any internal LOD bias.
	InternalFormatLODBias = 0;

	// Clear out any system memory data.  Texture compression may change whether or not we use system memory data at all.
	SystemMemoryData.Empty();

	TArray<BYTE> RawData;
	GetUncompressedSourceArt(RawData);

	// Don't compress textures smaller than DXT blocksize.
	if( SizeX < 4 || SizeY < 4 )
	{
		CompressionNone = 1;

		// Don't create mipmaps for 1x1 textures
		if ( SizeX == 1 && SizeY == 1 )
		{
			MipGenSettings = TMGS_NoMipmaps;
		}
	}

	IntermediateMipChain MipChain;

	if(GenerateMipChainBaseFromRaw(*this, MipChain, (UINT*)RawData.GetData(), SizeX, SizeY))
	{
		// We need to flush all rendering commands before we can modify a textures' mip array.
		if(Mips.Num())
		{
			// Flush rendering commands.
			FlushRenderingCommands();
		}

		// empty source texture, we fill data in later
		Mips.Empty();

		// generate mipmaps if required
		if((TextureMipGenSettings)MipGenSettings != TMGS_NoMipmaps)
		{
			GenerateMipChain(*this, MipChain);
		}

		CompressMipChainToTexture(*this, MipChain);

		if( CompressionSettings == TC_NormalmapUncompressed )
		{
			// Reduce the uncompressed normal's resolution to match DXT1 memory usage.
			if( Mips.Num() > 1 && SizeX > 1 && SizeY > 1 )
			{
				Mips.Remove(0);
				SizeX >>= 1;
				SizeY >>= 1;
				InternalFormatLODBias = 1;
			}
		}
		else
		{
			for(INT i = 0; i < MipsToRemoveOnCompress; i++)
			{
				if (Mips.Num() > 1 && SizeX > 1 && SizeY > 1)
				{
					Mips.Remove(0);
					SizeX >>= 1;
					SizeY >>= 1;
					InternalFormatLODBias++;
				}
			}
		}
	}
	else
	{
		// cannot handle this
		return;
	}
	
	// Initialize the mip tail base index to the index of the smallest mip map level
	MipTailBaseIdx = Max(0,Mips.Num()-1);

	// We modified the texture data and potentially even the format so we can't stream it from disk.
	bHasBeenLoadedFromPersistentArchive = FALSE;

	// Create the texture's resource.
	UpdateResource();
#endif  //_MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	// Update the content browser since the compression scheme of the texture may have changed and the content browser should update the thumbnail text
	GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI, this ) );

	// update GUID to propagate changes to texture file cache
	GenerateTextureFileCacheGUID(TRUE);
}

void UTexture2D::ResizeTexture(const FVector2D& Scale)
{
#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER

	// Check if the caller really wants to scale
	if( Scale.X == 1.f && Scale.Y == 1.f )
	{
		return;
	}

	switch( Format )
	{
	case PF_A8R8G8B8:
	case PF_G8:
	case PF_DXT1:
	case PF_DXT3:
	case PF_DXT5:
	case PF_BC5:
	case PF_V8U8:
	case PF_A1:
		// Handled formats, break.
		break;

	case PF_Unknown:
	case PF_A32B32G32R32F:
	case PF_G16:
	default:
		// Unhandled, return.
		return;
	}

	// Return if no source art is present (maybe old package), and it's not RGBA8 (which we can just use directly)
	if( !HasSourceArt() )
	{
		return;
	}

	// Restore the size to that of the source art.
	SizeX = OriginalSizeX;
	SizeY = OriginalSizeY;

	// Remove any internal LOD bias.
	InternalFormatLODBias = 0;

	// Clear out any system memory data.  Texture compression may change whether or not we use system memory data at all.
	SystemMemoryData.Empty();

	TArray<BYTE> RawData;
	GetUncompressedSourceArt(RawData);

	// Don't resize textures smaller than DXT blocksize.
	if( SizeX < 4 || SizeY < 4 )
	{
		CompressionNone = 1;

		// Don't create mipmaps for 1x1 textures
		if ( SizeX == 1 && SizeY == 1 )
		{
			MipGenSettings = TMGS_NoMipmaps;
		}
	}

	IntermediateMipChain MipChain;

	if(GenerateMipChainBaseFromRaw(*this, MipChain, (UINT*)RawData.GetData(), SizeX, SizeY))
	{
		// We need to flush all rendering commands before we can modify a textures' mip array.
		if(Mips.Num())
		{
			// Flush rendering commands.
			FlushRenderingCommands();
		}

		// empty source texture, we fill data in later
		Mips.Empty();

		// Scale the texture
		GenerateScaledMip(*this, MipChain, Scale);

		// Update the actual texture data with the new scaled data
		CompressMipChainToTexture(*this, MipChain);
		SizeX = MipChain.Mips(0).SizeX;
		SizeY = MipChain.Mips(0).SizeY;
	}
	else
	{
		// cannot handle this
		return;
	}
	
	// Initialize the mip tail base index to the index of the smallest mip map level
	MipTailBaseIdx = Max(0,Mips.Num()-1);

	// We modified the texture data and potentially even the format so we can't stream it from disk.
	bHasBeenLoadedFromPersistentArchive = FALSE;

	// Create the texture's resource.
	UpdateResource();

	// TODO - need to updat source art with resized data or it won't work
	// Recompress if needed (will regenerate mips too)
	//Compress();
#endif  //_MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
}

