/*=============================================================================
	Texture2DComposite.cpp: Implementation of UTexture2DComposite.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UTexture2DComposite);

/** A composite 2D texture resource. */
class FTexture2DCompositeResource : public FTextureResource
{
public:

	/** The width of the texture. */
	INT SizeX;

	/** The height of the texture. */
	INT SizeY;

	/** The format of the texture. */
	INT Format;

	/** Whether the texture stores SRGB or linear colors. */
	UBOOL bSRGB;

	/** Whether to unpack the texture -1..1 */
	UBOOL bBiasNormalMap;

	/** The number of mip-maps in the texture. */
	INT NumMips;

	/** Initialization constructor. */
	FTexture2DCompositeResource(UTexture2DComposite* InOwner):
		SizeX(0),
		SizeY(0),
		Format(PF_Unknown),
		bSRGB(FALSE),
		bBiasNormalMap(FALSE),
		NumMips(0),
		Owner(InOwner)
	{}

	// FTexture interface.
	virtual UINT GetSizeX() const
	{
		return SizeX;
	}
	/** Returns the height of the texture in pixels. */
	virtual UINT GetSizeY() const
	{
		return SizeY;
	}

	// FRenderResource interface.
	virtual void InitRHI()
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

		// The texture RHI resource is only created when the composition happens.  Until then, we use the global white texture.
		TextureRHI = GWhiteTexture->TextureRHI;
	}

private:

	/** The owner of this resource. */
	UTexture2DComposite* Owner;
};

/*-----------------------------------------------------------------------------
UTexture2DComposite
-----------------------------------------------------------------------------*/

/**
* Initializes the list of ValidRegions with only valid entries from the list of source regions
*/
static void ValidateSourceRegions(const TArray<FSourceTexture2DRegion>& SourceRegions,TArray<FSourceTexture2DRegion>& ValidRegions, const INT DestSizeX=0, const INT DestSizeY=0)
{
	UTexture2D* CompareTex = NULL;

	// update source regions list with only valid entries
	for( INT SrcIdx=0; SrcIdx < SourceRegions.Num(); SrcIdx++ )
	{
		const FSourceTexture2DRegion& SourceRegion = SourceRegions(SrcIdx);
		// the source texture for this region must exist
		if( SourceRegion.Texture2D == NULL )
		{
			debugf( NAME_Warning, TEXT("FCompositeTexture2DUtil: Source texture missing for region [%d] - skipping..."),
				SrcIdx );
		}
		// texture formats should match
		else if( CompareTex && SourceRegion.Texture2D->Format != CompareTex->Format )
		{
			debugf( NAME_Warning, TEXT("FCompositeTexture2DUtil: Source texture format mismatch [%s] - skipping..."), 
				*SourceRegion.Texture2D->GetPathName() );				
		}
		// SRGB
		else if( CompareTex && SourceRegion.Texture2D->SRGB != CompareTex->SRGB )
		{
			debugf( NAME_Warning, TEXT("FCompositeTexture2DUtil: Source texture SRGB mismatch [%s] - skipping..."),
				*SourceRegion.Texture2D->GetPathName() );
		}
		// RGBE
		else if( CompareTex && SourceRegion.Texture2D->RGBE != CompareTex->RGBE )
		{
			debugf( NAME_Warning, TEXT("FCompositeTexture2DUtil: Source texture RGBE mismatch [%s] - skipping..."),
				*SourceRegion.Texture2D->GetPathName() );
		}
		// If DestSizeX/DestSizeY are set, SourceRegion size must be less than or equal to destination size
		else if( DestSizeX > 0 && DestSizeY > 0 && 
				 (SourceRegion.Texture2D->SizeX > DestSizeX || SourceRegion.Texture2D->SizeY > DestSizeY))
		{
			debugf( NAME_Warning, TEXT("FCompositeTexture2DUtil: Source texture size is larger than destination [%s] (%d,%d) > (%d,%d) - skipping..."), 
				*SourceRegion.Texture2D->GetPathName(),
				SourceRegion.Texture2D->SizeX,
				SourceRegion.Texture2D->SizeY,
				DestSizeX,
				DestSizeY );
		}
		// source textures must match in SizeX/SizeY if DestSizeX/DestSizeY are not set
		else if( CompareTex && DestSizeX <= 0 && DestSizeY <= 0 && 
				 (SourceRegion.Texture2D->SizeX != CompareTex->SizeX || SourceRegion.Texture2D->SizeY != CompareTex->SizeY) )
		{
			debugf( NAME_Warning, TEXT("FCompositeTexture2DUtil: Source texture size mismatch [%s] (%d,%d) not (%d,%d) from [%s] - skipping..."), 
				*SourceRegion.Texture2D->GetPathName(),
				SourceRegion.Texture2D->SizeX,
				SourceRegion.Texture2D->SizeY,
				CompareTex->SizeX,
				CompareTex->SizeY,
				*CompareTex->GetPathName() );
		}
		// source textures must have the same number of mips
		else if( CompareTex && DestSizeX <= 0 && DestSizeY <= 0 && 
				 SourceRegion.Texture2D->Mips.Num() != CompareTex->Mips.Num() )
		{
			debugf( NAME_Warning, TEXT("FCompositeTexture2DUtil: Source texture number Mips mismatch [%s] %d not %d - skipping..."), 
				*SourceRegion.Texture2D->GetPathName(),
				SourceRegion.Texture2D->Mips.Num(),
				CompareTex->Mips.Num() );
		}
		// source region not outside texture area
		else if( CompareTex && DestSizeX <= 0 && DestSizeY <= 0 &&
				 ((SourceRegion.OffsetX + SourceRegion.SizeX > CompareTex->SizeX) || (SourceRegion.OffsetY + SourceRegion.SizeY > CompareTex->SizeY)) )
		{
			debugf( NAME_Warning, TEXT("FCompositeTexture2DUtil: Source region outside texture area [%s] - skipping..."), 
				*SourceRegion.Texture2D->GetPathName() );
		}
		// make sure source textures are not streamable
		else if( !SourceRegion.Texture2D->IsFullyStreamedIn() )
		{
			debugf( NAME_Warning, TEXT("FCompositeTexture2DUtil: Source texture is not fully streamed in [%s] - skipping..."), 
				*SourceRegion.Texture2D->GetPathName() );
		}
		// valid so add it to the list
		else
		{
			ValidRegions.AddItem( SourceRegion );
			// First valid texture - remember to compare to others that follow.
			if(!CompareTex)
			{
				CompareTex = SourceRegion.Texture2D;
			}
		}
	}
}
/**
* Regenerates this composite texture using the list of source texture regions.
* The existing mips are reallocated and the RHI resource for the texture is updated
*
* @param NumMipsToGenerate - number of mips to generate. if 0 then all mips are created
*/
void UTexture2DComposite::UpdateCompositeTexture(INT NumMipsToGenerate)
{
	// Validate the source regions.
	TArray<FSourceTexture2DRegion> ValidRegions;
	ValidateSourceRegions(SourceRegions,ValidRegions,DestSizeX,DestSizeY);

	if( ValidRegions.Num() == 0 )
	{
		debugf( NAME_Warning, TEXT("UTexture2DComposite: no regions to process") );
	}
	else
	{
		// calc index of first available mip common to the set of source textures
		INT FirstSrcMipIdx = GetFirstAvailableMipIndex(ValidRegions);

		INT SizeX = 0;
		INT SizeY = 0;
		if( DestSizeX != 0 && DestSizeY != 0)
		{
			// calc the texture size for the comp texture based on the MaxLODBias
			SizeX = DestSizeX >> FirstSrcMipIdx;
			SizeY = DestSizeY >> FirstSrcMipIdx;
		}
		else
		{
			// if DestSize is not set get the size from the valid regions
			SizeX = ValidRegions(0).Texture2D->Mips(FirstSrcMipIdx).SizeX;
			SizeY = ValidRegions(0).Texture2D->Mips(FirstSrcMipIdx).SizeY;
		}

		// use the same format as the source textures
		EPixelFormat SrcFormat = (EPixelFormat)ValidRegions(0).Texture2D->Format;

		// max number of mips based on texture size
		const INT MaxTexMips = appCeilLogTwo(Max(SizeX,SizeY))+1;
		// clamp num mips to maxmips or force to maxmips if set <= 0
		const INT NumTexMips	= NumMipsToGenerate > 0 ? Min( NumMipsToGenerate, MaxTexMips ) : MaxTexMips;

		// Initialize the resource.
		UpdateResource();
		FTexture2DCompositeResource* CompositeResource = (FTexture2DCompositeResource*)Resource;
		CompositeResource->SizeX = SizeX;
		CompositeResource->SizeY = SizeY;
		CompositeResource->Format = SrcFormat;
		CompositeResource->bSRGB = ValidRegions(0).Texture2D->SRGB;
		CompositeResource->bBiasNormalMap = ValidRegions(0).Texture2D->BiasNormalMap();
		CompositeResource->NumMips = NumTexMips;

		// fill in all of the Mip data 
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			CopyRectRegions,
			UTexture2DComposite*,Texture,this,
			TArray<FSourceTexture2DRegion>,ValidRegions,ValidRegions,
			{
				Texture->RenderThread_CopyRectRegions(ValidRegions);
			});

		// Copy other settings.
		SRGB = ValidRegions(0).Texture2D->SRGB;
		RGBE = ValidRegions(0).Texture2D->RGBE;

		for(INT i=0; i<4; i++)
		{
			UnpackMin[i] = ValidRegions(0).Texture2D->UnpackMin[i];
			UnpackMax[i] = ValidRegions(0).Texture2D->UnpackMax[i];
		}

		LODGroup = ValidRegions(0).Texture2D->LODGroup;
		LODBias = ValidRegions(0).Texture2D->LODBias;
	}
}

/** Utility that checks to see if all Texture2Ds specified in the SourceRegions array are fully streamed in. */
UBOOL UTexture2DComposite::SourceTexturesFullyStreamedIn()
{
	for(INT i=0; i<SourceRegions.Num(); i++)
	{
		if(SourceRegions(i).Texture2D)
		{
			// Update the streaming status whenever this is checked, so we don't need to wait for the texture streaming system to poll
			// the texture, which has a latency of up to 1 second.
			SourceRegions(i).Texture2D->UpdateStreamingStatus();

			UBOOL bAllStreamedIn = SourceRegions(i).Texture2D->IsFullyStreamedIn();
			if(!bAllStreamedIn)
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

/** Utils to reset all source region info. */
void UTexture2DComposite::ResetSourceRegions()
{
	SourceRegions.Empty();
}

/**
* Calculate the first available mip from a set of textures based on the LOD bias for each
* texture.
* 
* @return first available mip index from the source regions
*/
INT UTexture2DComposite::GetFirstAvailableMipIndex(const TArray<FSourceTexture2DRegion>& ValidRegions)
{
	check(ValidRegions.Num() > 0);

 	// find the max LOD Bias from the source textures
	INT MaxLODBias = 0;
	for( INT RegionIdx=0; RegionIdx < ValidRegions.Num(); RegionIdx++ )
	{
		const FSourceTexture2DRegion& Region = ValidRegions(RegionIdx);			
		MaxLODBias = Max<INT>( MaxLODBias, Region.Texture2D->GetCachedLODBias() );
	}

	// If destination size was manually set, assume source size is not the same. 
	// Use MaxLODBias instead of checking number of resident mips.
	if( DestSizeX > 0 && DestSizeY > 0 )
	{
		return MaxLODBias;
	}

	// max num of mips that can be loaded
	INT	MaxResidentMips	= Max( 1, Min(ValidRegions(0).Texture2D->Mips.Num() - MaxLODBias, GMaxTextureMipCount) );

	// clamp number of mips based on the maximum allowed texture size. if 0 then no clamping
	if( MaxTextureSize > 0 )
	{
		MaxResidentMips = Min<INT>(MaxResidentMips,appCeilLogTwo(MaxTextureSize)+1);
	}
	
	// find the largest common resident num of mips
	INT MaxCurrentResidentMips = MaxResidentMips;
	for( INT RegionIdx=0; RegionIdx < ValidRegions.Num(); RegionIdx++ )
	{
		const FSourceTexture2DRegion& Region = ValidRegions(RegionIdx);			
		MaxCurrentResidentMips = Min<INT>( MaxCurrentResidentMips, Region.Texture2D->ResidentMips );
	}

	// first source mip level to be used
	INT FirstMipIdx = ValidRegions(0).Texture2D->Mips.Num() - MaxCurrentResidentMips;

	return FirstMipIdx;
}

/**
* Locks each region of the source RHI texture 2d resources and copies the block of data
* for that region to the destination mip buffer. This is done for all mip levels.
*
* (Only called by the rendering thread)
*/
void UTexture2DComposite::RenderThread_CopyRectRegions(const TArray<FSourceTexture2DRegion>& ValidRegions)
{
	check(ValidRegions.Num() > 0);

	FTexture2DCompositeResource* CompositeResource = (FTexture2DCompositeResource*)Resource;

	// calc index of first available mip common to the set of source textures
	INT FirstSrcMipIdx = GetFirstAvailableMipIndex(ValidRegions);

	// create a temp RHI texture 2d used to hold intermediate mip data
	DWORD CreateFlags = CompositeResource->bSRGB ? TexCreate_SRGB : 0;
	if( CompositeResource->bBiasNormalMap )
	{
		CreateFlags |= TexCreate_BiasNormalMap;
	}
	FTexture2DRHIRef CompositeTexture2D = RHICreateTexture2D(
		CompositeResource->SizeX,
		CompositeResource->SizeY,
		CompositeResource->Format,
		CompositeResource->NumMips,
		CreateFlags,
		NULL
		);

	// process each mip level
	for( INT MipIdx=0; MipIdx < CompositeResource->NumMips; MipIdx++ )
	{
		// list of source regions that need to be copied for this mip level
		TArray<FCopyTextureRegion2D> CopyRegions;

		for( INT RegionIdx=0; RegionIdx < ValidRegions.Num(); RegionIdx++ )		
		{
			const FSourceTexture2DRegion& Region = ValidRegions(RegionIdx);
			FTexture2DResource* SrcTex2DResource = (FTexture2DResource*)Region.Texture2D->Resource;		
			if( SrcTex2DResource && 
				SrcTex2DResource->IsInitialized() &&
				Region.Texture2D->IsFullyStreamedIn() &&
				Region.Texture2D->ResidentMips == Region.Texture2D->RequestedMips &&
				Region.Texture2D->Mips.IsValidIndex(MipIdx + FirstSrcMipIdx) )
			{
				// scale regions for the current mip level. The regions are assumed to be sized w/ respect to the base (maybe not loaded) mip level
				INT RegionOffsetX = Region.OffsetX >> (MipIdx + FirstSrcMipIdx);
				INT RegionOffsetY = Region.OffsetY >> (MipIdx + FirstSrcMipIdx);

				INT RegionDestOffsetX = Region.DestOffsetX >> (MipIdx + FirstSrcMipIdx);
				INT RegionDestOffsetY = Region.DestOffsetY >> (MipIdx + FirstSrcMipIdx);
				INT RegionSizeX = Max(Region.SizeX >> (MipIdx + FirstSrcMipIdx),1);
				INT RegionSizeY = Max(Region.SizeY >> (MipIdx + FirstSrcMipIdx),1);
				// scale create the new region for the RHI texture copy
				FCopyTextureRegion2D& CopyRegion = *new(CopyRegions) FCopyTextureRegion2D(
					SrcTex2DResource->GetTexture2DRHI(),
					Region.Texture2D,
					RegionOffsetX,
					RegionOffsetY,
					RegionSizeX,
					RegionSizeY,
					Max(0, FirstSrcMipIdx),
					RegionDestOffsetX,
					RegionDestOffsetY
					);
			}
		}

		// copy from all of the source regions for the current mip level to the temp texture
		RHICopyTexture2D(
			CompositeTexture2D,
			MipIdx,
			CompositeResource->SizeX,
			CompositeResource->SizeY,
			CompositeResource->Format,
			CopyRegions
			);
	}

	// Set the resource's texture to the composite texture.
	Resource->TextureRHI = CompositeTexture2D;
}

FTextureResource* UTexture2DComposite::CreateResource()
{
	return new FTexture2DCompositeResource(this);
}

FLOAT UTexture2DComposite::GetSurfaceWidth() const
{
	return 1.0f;
}

FLOAT UTexture2DComposite::GetSurfaceHeight() const
{
	return 1.0f;
}

void UTexture2DComposite::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.Ver() < VER_TEXTURE2DCOMPOSITE_BASE_CHANGE)
	{
		// Serialize the mips array to remain backward compatible with Texture2DComposites saved
		// when it inherited from Texture2D instead of Texture.
		TIndirectArray<FTexture2DMipMap> Mips;
		Mips.Serialize(Ar,this);
	}
}
