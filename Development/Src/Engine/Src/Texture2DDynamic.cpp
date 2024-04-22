/*=============================================================================
	Texture2DDynamic.cpp: Implementation of UTexture2DDynamic.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnJPEG.h"

IMPLEMENT_CLASS(UTexture2DDynamic);

/*-----------------------------------------------------------------------------
	FTexture2DDynamicResource
-----------------------------------------------------------------------------*/

/** Initialization constructor. */
FTexture2DDynamicResource::FTexture2DDynamicResource(UTexture2DDynamic* InOwner)
:	Owner(InOwner)
{
}

/** Returns the width of the texture in pixels. */
UINT FTexture2DDynamicResource::GetSizeX() const
{
	return Owner->SizeX;
}

/** Returns the height of the texture in pixels. */
UINT FTexture2DDynamicResource::GetSizeY() const
{
	return Owner->SizeY;
}

/** Called when the resource is initialized. This is only called by the rendering thread. */
void FTexture2DDynamicResource::InitRHI()
{
	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer
	(
		GSystemSettings.TextureLODSettings.GetSamplerFilter( Owner ),
		AM_Wrap,
		AM_Wrap,
		AM_Wrap
	);
	SamplerStateRHI = RHICreateSamplerState( SamplerStateInitializer );

	DWORD Flags = 0;
	if ( Owner->bIsResolveTarget )
	{
		Flags |= TexCreate_ResolveTargetable;
		bIgnoreGammaConversions = TRUE;		// Note, we're ignoring Owner->SRGB (it should be FALSE).
	}
	else if ( Owner->SRGB )
	{
		Flags |= TexCreate_SRGB;
	}
	if ( Owner->bNoTiling )
	{
		Flags |= TexCreate_NoTiling;
	}
	Texture2DRHI = RHICreateTexture2D( Owner->SizeX, Owner->SizeY, Owner->Format, Owner->NumMips, Flags, NULL );
	TextureRHI = Texture2DRHI;
}

/** Called when the resource is released. This is only called by the rendering thread. */
void FTexture2DDynamicResource::ReleaseRHI()
{
	FTextureResource::ReleaseRHI();
	Texture2DRHI.SafeRelease();
}

/** Returns the Texture2DRHI, which can be used for locking/unlocking the mips. */
FTexture2DRHIRef FTexture2DDynamicResource::GetTexture2DRHI()
{
	return Texture2DRHI;
}


/*-----------------------------------------------------------------------------
	UTexture2DDynamic
-----------------------------------------------------------------------------*/

/**

 * Initializes the texture and creates the render resource.
 * It will create 1 miplevel with the format PF_A8R8G8B8.
 *
 * @param InSizeX	- Width of the texture, in texels
 * @param InSizeY	- Height of the texture, in texels
 * @param InFormat	- Format of the texture, defaults to PF_A8R8G8B8
 */
void UTexture2DDynamic::Init( INT InSizeX, INT InSizeY, BYTE InFormat/*=2*/, UBOOL InIsResolveTarget/*=FALSE*/ )
{
	SizeX = InSizeX;
	SizeY = InSizeY;
	Format = (EPixelFormat) InFormat;
	NumMips = 1;
	bIsResolveTarget = InIsResolveTarget;

	// Initialize the resource.
	UpdateResource();
}

/**
 * Update the mip data for the texture and its render resource
 * It is assumed that the data is being copied is already of the right format/size for the mip.
 *
 * @param MipIdx 0 based index to the mip level to update
 * @param MipData byte array of data to copy to mip
 */
void UTexture2DDynamic::UpdateMip(INT MipIdx,const TArray<BYTE>& MipData)
{
	check(MipIdx < NumMips && MipData.Num() > 0);

	struct FMipUpdateParams
	{
		INT MipIdx;
		TArray<BYTE> MipData;
		FTexture2DDynamicResource* Resource;
	};
	FMipUpdateParams* MipUpdateData = new FMipUpdateParams();
	MipUpdateData->MipIdx = MipIdx;
	MipUpdateData->MipData = MipData;
	MipUpdateData->Resource = (FTexture2DDynamicResource*)Resource;

	// queue command to update texture mip, copies MipData for asynch operation
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FUpdateMipData,
		FMipUpdateParams*, MipUpdateData, MipUpdateData,
		{
			// Lock new texture.
			UINT DestPitch;
			void* LockedMipData = RHILockTexture2D(MipUpdateData->Resource->GetTexture2DRHI(), MipUpdateData->MipIdx, TRUE, DestPitch, FALSE);
			if (LockedMipData != NULL)
			{
				appMemcpy(LockedMipData,MipUpdateData->MipData.GetData(),MipUpdateData->MipData.Num());
				RHIUnlockTexture2D(MipUpdateData->Resource->GetTexture2DRHI(), MipUpdateData->MipIdx, FALSE);
			}
			delete MipUpdateData;
		});
}

/**
 * Update the mip data for the texture and its render resource from JPEG data.
 * If the decoded JPEG size doesn't match the existing texture size 
 * then the texture is resized.
 *
 * @param MipIdx 0 based index to the mip level to update
 * @param MipData byte array of JPEG to decode and then copy to mip
 */
void UTexture2DDynamic::UpdateMipFromJPEG(INT MipIdx,const TArray<BYTE>& MipData)
{
	FDecoderJPEG Decoder(MipData.GetData(),MipData.Num());
	BYTE* UncompressedImage = Decoder.Decode();
	if (UncompressedImage != NULL)
	{
		if (MipIdx == 0 &&
			Decoder.GetWidth() != SizeX ||
			Decoder.GetHeight() != SizeY)
		{
			SizeX = Decoder.GetWidth();
			SizeY = Decoder.GetHeight();
			UpdateResource();
		}
		TArray<BYTE> MipDataUncompressed;
		MipDataUncompressed.AddZeroed(SizeX*SizeY*4);
		appMemcpy(MipDataUncompressed.GetData(),UncompressedImage,MipDataUncompressed.Num());
		UpdateMip(MipIdx,MipDataUncompressed);
	}
}

FTextureResource* UTexture2DDynamic::CreateResource()
{
	return new FTexture2DDynamicResource(this);
}

FLOAT UTexture2DDynamic::GetSurfaceWidth() const
{
	return SizeX;
}

FLOAT UTexture2DDynamic::GetSurfaceHeight() const
{
	return SizeY;
}

void UTexture2DDynamic::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

/** Script-accessible function to create and initialize a new Texture2DDynamic with the requested settings */
void UTexture2DDynamic::execCreate(FFrame& Stack, RESULT_DECL)
{
	P_GET_INT(InSizeX);
	P_GET_INT(InSizeY);
	P_GET_BYTE_OPTX(InFormat, PF_A8R8G8B8);
	P_GET_UBOOL_OPTX(InIsResolveTarget, FALSE);
	P_FINISH;

	EPixelFormat DesiredFormat = EPixelFormat(InFormat);
	if (InSizeX > 0 && InSizeY > 0 )
	{
		UTexture2DDynamic* NewTexture = Cast<UTexture2DDynamic>(StaticConstructObject(GetClass(), GetTransientPackage(), NAME_None, RF_Transient));
		if (NewTexture != NULL)
		{
			// Disable compression
			NewTexture->CompressionNone			= TRUE;
			NewTexture->CompressionSettings		= TC_Default;
			NewTexture->MipGenSettings			= TMGS_NoMipmaps;
			NewTexture->CompressionNoAlpha		= TRUE;
			NewTexture->DeferCompression		= FALSE;
			if ( InIsResolveTarget )
			{
//				NewTexture->SRGB				= FALSE;
				NewTexture->bNoTiling			= FALSE;
			}
			else
			{
				// Untiled format
				NewTexture->bNoTiling			= TRUE;
			}

			NewTexture->Init(InSizeX, InSizeY, DesiredFormat, InIsResolveTarget);
		}
		*(UTexture2DDynamic**)Result = NewTexture;
	}
	else
	{
		debugf(NAME_Warning, TEXT("Invalid parameters specified for UTexture2DDynamic::Create()"));
		*(UTexture2DDynamic**)Result = NULL;
	}
}
