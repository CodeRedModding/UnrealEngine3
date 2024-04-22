/*=============================================================================
	TextureCube.cpp: UTextureCube implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#if _WINDOWS
#include "UnConsoleTools.h"
#include "UnConsoleSupportContainer.h"
#endif

IMPLEMENT_CLASS(UTextureCube);

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UTextureCube::InitializeIntrinsicPropertyValues()
{
	SRGB = TRUE;
	UnpackMin[0] = UnpackMin[1] = UnpackMin[2] = UnpackMin[3] = 0.0f;
	UnpackMax[0] = UnpackMax[1] = UnpackMax[2] = UnpackMax[3] = 1.0f;
}
void UTextureCube::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

/**
* Called after property was modified
* @param PropertyThatChanged - modified property of this object
*/
void UTextureCube::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Validate();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
* Called after loading
*/
void UTextureCube::PostLoad()
{
	//Dedicated servers have no texture internals
	if ((appGetPlatformType() & UE3::PLATFORM_WindowsServer) == 0 &&
		(GCookingTarget & UE3::PLATFORM_WindowsServer) == 0)
	{
		Validate();
	}

	Super::PostLoad();	
}

/**
 * Validates cubemap which entails verifying that all faces are non-NULL and share the same format, width, height and number of
 * miplevels. The results are cached in the respective mirrored properties and bIsCubemapValid is set accordingly.
 */
void UTextureCube::Validate()
{
	// Assume invalid cubemap.
	bIsCubemapValid = FALSE;

	// Make sure all faces are non NULL.
	if( FacePosX &&	FaceNegX &&	
		FacePosY &&	FaceNegY &&	
		FacePosZ &&	FaceNegZ )
	{
		// Make sure all faces use the same format, have the same width and height and number of mips
		Format	= FacePosX->Format;
		SizeX	= FacePosX->SizeX;
		SizeY	= FacePosX->SizeY;
		NumMips	= FacePosX->Mips.Num();
		if( (SizeX == SizeY) &&	
			(SizeX > 0) &&	
			(FaceNegX->Format == Format) && (FaceNegX->SizeX == SizeX) && (FaceNegX->SizeY == SizeY) && (FaceNegX->Mips.Num() == NumMips) &&
			(FacePosY->Format == Format) && (FacePosY->SizeX == SizeX) && (FacePosY->SizeY == SizeY) && (FacePosY->Mips.Num() == NumMips) &&
			(FaceNegY->Format == Format) && (FaceNegY->SizeX == SizeX) && (FaceNegY->SizeY == SizeY) && (FaceNegY->Mips.Num() == NumMips) &&
			(FacePosZ->Format == Format) && (FacePosZ->SizeX == SizeX) && (FacePosZ->SizeY == SizeY) && (FacePosZ->Mips.Num() == NumMips) &&
			(FaceNegZ->Format == Format) && (FaceNegZ->SizeX == SizeX) && (FaceNegZ->SizeY == SizeY) && (FaceNegZ->Mips.Num() == NumMips) )
		{
			bIsCubemapValid = TRUE;
		}
	}

	// Use a 1x1 RGBA8 in the case of the cubemap being invalid.
	if( !bIsCubemapValid )
	{
		SizeX	= 1;
		SizeY	= 1;
		NumMips	= 1;
		Format	= PF_A8R8G8B8;
	}
	else
	{
		// find the largest allowable number of mips from all faces. 
		// This is needed since each face texture could be using a different LOD bias
		NumMips = Min(GMaxTextureMipCount,NumMips);
		for( INT FaceIndex=0; FaceIndex<6; FaceIndex++ )
		{
			UTexture2D* FaceTexture = GetFace( FaceIndex );
			if( FaceTexture )
			{
				INT LODBias = GSystemSettings.TextureLODSettings.CalculateLODBias(FaceTexture);
				// Make sure the LODBias doesn't exceed the number of available mips.
				if ( LODBias >= FaceTexture->Mips.Num() )
				{
					LODBias = 0;
				}

				NumMips = Min(FaceTexture->Mips.Num() - LODBias, NumMips);
				if( NumMips > 0 )
				{
					const INT FirstMip = FaceTexture->Mips.Num() - NumMips; 
					if( FirstMip >= 0 )
					{
						SizeX = Min(FaceTexture->Mips(FirstMip).SizeX, SizeX);
					}
					else
					{
						warnf( NAME_Error, TEXT( "FirstMip of (%s) must be greater or equal than 0.  Please recreate the CubeMap: %s" ), *FaceTexture->GetFullName(), *this->GetFullName() );
					}
				}
				else
				{
					warnf( NAME_Error, TEXT( "NumMips of (%s) must be greater than 0.  Please recreate the CubeMap: %s" ), *FaceTexture->GetFullName(), *this->GetFullName() );
				}
			}
		}
	}
}

/** 
 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
 */
FString UTextureCube::GetDesc()
{
	return FString::Printf( TEXT("Cube: %dx%d [%s%s]"), SizeX, SizeY, GPixelFormats[Format].Name, DeferCompression ? TEXT("*") : TEXT(""));
}

/** 
 * Returns detailed info to populate listview columns
 */
FString UTextureCube::GetDetailedDescription( INT InIndex )
{
	FString Description = TEXT( "" );
	switch( InIndex )
	{
	case 0:
		Description = FString::Printf( TEXT( "%dx%d" ), SizeX, SizeY );
		break;
	case 1:
		Description = GPixelFormats[Format].Name;
		if( DeferCompression )
		{
			Description += TEXT( "*" );
		}
		break;
	}
	return( Description );
}

/**
 * Returns the face associated with the passed in index.
 *
 * @param	FaceIndex	index of face to return
 * @return	texture object associated with passed in face index
 */
UTexture2D*	UTextureCube::GetFace( INT FaceIndex ) const
{
	UTexture2D* FaceTexture = NULL;
	switch( FaceIndex )
	{
	case 0:
		FaceTexture = FacePosX;
		break;
	case 1:
		FaceTexture = FaceNegX;
		break;
	case 2:
		FaceTexture = FacePosY;
		break;
	case 3:
		FaceTexture = FaceNegY;
		break;
	case 4:
		FaceTexture = FacePosZ;
		break;
	case 5:
		FaceTexture = FaceNegZ;
		break;
	}
	return FaceTexture;
}

/**
 * Sets the face associated with the passed in index.
 *
 * @param	FaceIndex	index of face to return
 * @param	FaceTexture	texture object to associate with passed in face index
 */
void UTextureCube::SetFace(INT FaceIndex,UTexture2D* FaceTexture)
{
	switch( FaceIndex )
	{
	case 0:
		FacePosX = FaceTexture;
		break;
	case 1:
		FaceNegX = FaceTexture;
		break;
	case 2:
		FacePosY = FaceTexture;
		break;
	case 3:
		FaceNegY = FaceTexture;
		break;
	case 4:
		FacePosZ = FaceTexture;
		break;
	case 5:
		FaceNegZ = FaceTexture;
		break;
	}
}

/**
 * Returns the size of this texture in bytes on 360 if it had MipCount miplevels streamed in.
 *
 * @param	MipCount	Number of toplevel mips to calculate size for
 * @return	size of top mipcount mips in bytes
 */
INT UTextureCube::Get360Size( INT MipCount ) const
{
#if _WINDOWS && !CONSOLE
	INT Size = 0;
	UTexture2D*	FaceTexture = GetFace(0);
	if (FaceTexture)
	{
		DWORD TexCreateFlags = SRGB ? TexCreate_SRGB : 0;
		// if no miptail is available then create the texture without a packed miptail
		if( FaceTexture->MipTailBaseIdx == -1 )
		{
			TexCreateFlags |= TexCreate_NoMipTail;
		}

		FConsoleSupport* ConsoleSupport = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(CONSOLESUPPORT_NAME_360);
		if (ConsoleSupport)
		{
			FConsoleTextureCooker* TextureCooker = ConsoleSupport->GetGlobalTextureCooker();
			if (TextureCooker)
			{
				// Figure out what the first mip to use is.
				INT FirstMip = Max( 0, FaceTexture->Mips.Num() - MipCount );
				if (FirstMip < FaceTexture->Mips.Num())
				{
					const FTexture2DMipMap& MipMap = FaceTexture->Mips(FirstMip);
					Size = TextureCooker->GetPlatformCubeTextureSize(FaceTexture->Format, MipMap.SizeX, MipCount, TexCreateFlags);
				}
			}
		}
	}
	return Size;
#else
	return 0;
#endif
}

/**
 * Calculates the size of this texture if it had MipCount miplevels streamed in.
 *
 * @param	MipCount	Which mips to calculate size for.
 * @return	Total size of all specified mips, in bytes
 */
INT UTextureCube::CalcTextureMemorySize( ETextureMipCount MipCount ) const
{
	UINT SumSize = 0;
	for(UINT FaceIdx = 0; FaceIdx < 6; ++FaceIdx)
	{
		UTexture2D* Texture2D = GetFace(FaceIdx);
		if(Texture2D)
		{
			SumSize += Texture2D->CalcTextureMemorySize( MipCount );
		}
	}
	return SumSize;
}

class FTextureCubeResource : public FTextureResource
{
public:
	/**
	 * Minimal initialization constructor.
	 * @param InOwner - The UTextureCube which this FTextureCubeResource represents.
	 */
	FTextureCubeResource(UTextureCube* InOwner)
	:	Owner(InOwner)
#if STATS
	,	TextureSize(0)
#if _WINDOWS
	,	TextureSize_360(0)
#endif
#endif
	{
		//Initialize the MipData array
		for ( INT FaceIndex=0;FaceIndex<6; FaceIndex++)
		{
			for( INT MipIndex=0; MipIndex<ARRAY_COUNT(MipData[FaceIndex]); MipIndex++ )
			{
				MipData[FaceIndex][MipIndex] = NULL;
			}
		}

		for( INT FaceIndex=0; FaceIndex<6; FaceIndex++ )
		{
			UTexture2D* FaceTexture = Owner->GetFace( FaceIndex );
			if( FaceTexture )
			{
#if STATS
				if( TextureSize == 0 )
				{
					TextureSize = 6 * FaceTexture->CalcTextureMemorySize( Owner->NumMips );
				}
#if _WINDOWS
				if (TextureSize_360 == 0)
				{
					TextureSize_360 = Owner->Get360Size(Owner->NumMips);
				}
#endif
#endif
				INT FirstMip = FaceTexture->Mips.Num() - Owner->NumMips; check(FirstMip>=0);
				for( INT MipIndex=0; MipIndex<ARRAY_COUNT(MipData[FaceIndex]); MipIndex++ )
				{
					if( MipIndex >= FirstMip &&
						MipIndex < FaceTexture->Mips.Num() &&
						FaceTexture->Mips(MipIndex).Data.IsAvailableForUse() )
					{
						if( FaceTexture->Mips(MipIndex).Data.IsStoredInSeparateFile() )
						{
							debugf( NAME_Error, TEXT("Corrupt texture [%s]! Missing bulk data for MipIndex=%d"),*FaceTexture->GetFullName(),MipIndex );
						}
						else			
						{
							// Get copy of data, potentially loading array or using already loaded version.
							FaceTexture->Mips(MipIndex).Data.GetCopy( &MipData[FaceIndex][MipIndex], TRUE );
							check(MipData[FaceIndex][MipIndex]);
						}
					}
				}
			}
		}
	}

	/**
	 * Destructor, freeing MipData in the case of resource being destroyed without ever 
	 * having been initialized by the rendering thread via InitRHI.
	 */	
	~FTextureCubeResource()
	{
		// Make sure we're not leaking memory if InitRHI has never been called.
		for (INT i=0; i<6; i++)
		{
			for( INT MipIndex=0; MipIndex<ARRAY_COUNT(MipData[i]); MipIndex++ )
			{
				// free any mip data that was copied 
				if( MipData[i][MipIndex] )
				{
					appFree( MipData[i][MipIndex] );
				}
				MipData[i][MipIndex] = NULL;
			}
		}
	}
	
	/**
	 * Called when the resource is initialized. This is only called by the rendering thread.
	 */
	virtual void InitRHI()
	{
		// No support for cubemaps in Flash.
#if !FLASH
		INC_DWORD_STAT_BY( STAT_TextureMemory, TextureSize );
		INC_DWORD_STAT_BY( Owner->LODGroup + STAT_TextureGroupFirst, TextureSize );
		STATWIN( INC_DWORD_STAT_BY( STAT_XboxTextureMemory, TextureSize_360 ) );

		// Create the RHI texture.
		DWORD TexCreateFlags = Owner->SRGB ? TexCreate_SRGB : 0;
		FTextureCubeRHIRef TextureCube = RHICreateTextureCube( Owner->SizeX, Owner->Format, Owner->NumMips, TexCreateFlags, NULL );
		TextureRHI = TextureCube;

		// Read the mip-levels into the RHI texture.
		for( INT FaceIndex=0; FaceIndex<6; FaceIndex++ )
		{
			UTexture2D* FaceTexture = Owner->GetFace( FaceIndex );
			if( FaceTexture )
			{
				INT FirstMip = FaceTexture->Mips.Num() - Owner->NumMips; check(FirstMip>=0);            
				for( INT MipIndex=0; MipIndex<Owner->NumMips; MipIndex++ )
				{
					if( MipData[FaceIndex][MipIndex+FirstMip] != NULL )
					{
						UINT DestStride;
						void* TheMipData = RHILockTextureCubeFace( TextureCube, FaceIndex, MipIndex, TRUE, DestStride, FALSE );
						GetData( FaceIndex, MipIndex+FirstMip, TheMipData, DestStride );
						RHIUnlockTextureCubeFace( TextureCube, FaceIndex, MipIndex, FALSE );
					}
				}
			}
		}

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer
		(
			Owner->bIsCubemapValid ? GSystemSettings.TextureLODSettings.GetSamplerFilter( Owner->FacePosX ) : SF_Point,
			AM_Clamp,
			AM_Clamp,
			AM_Clamp
		);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);

		// Set the greyscale format flag appropriately.
		bGreyScaleFormat = (Owner->Format == PF_G8);
#endif
	}

#if STATS
	virtual void ReleaseRHI()
	{
		DEC_DWORD_STAT_BY( STAT_TextureMemory, TextureSize );
		DEC_DWORD_STAT_BY( Owner->LODGroup + STAT_TextureGroupFirst, TextureSize );
		STATWIN( DEC_DWORD_STAT_BY( STAT_XboxTextureMemory, TextureSize_360 ) );
		FTextureResource::ReleaseRHI();
	}
#endif

	/** Returns the width of the texture in pixels. */
	virtual UINT GetSizeX() const
	{
		return Owner->SizeX;
	}

	/** Returns the height of the texture in pixels. */
	virtual UINT GetSizeY() const
	{
		return Owner->SizeY;
	}

private:
	/** Local copy/ cache of mip data. Only valid between creation and first call to InitRHI */
	void* MipData[6][MAX_TEXTURE_MIP_COUNT];

	/** The UTextureCube which this resource represents. */
	const UTextureCube* Owner;

#if STATS
	// Cached texture size for stats. */
	INT	TextureSize;
#if _WINDOWS
	INT TextureSize_360;
#endif
#endif

	/**
	 * Writes the data for a single mip-level into a destination buffer.
	 * @param FaceIndex		The index of the face of the mip-level to read.
	 * @param MipIndex		The index of the mip-level to read.
	 * @param Dest			The address of the destination buffer to receive the mip-level's data.
	 * @param DestPitch		Number of bytes per row
	 */
	void GetData( INT FaceIndex, INT MipIndex, void* Dest, UINT DestPitch )
	{
		if(Owner->bIsCubemapValid )
		{
			const UTexture2D*		FaceTexture = Owner->GetFace( FaceIndex );
			if( FaceTexture )
			{
				const FTexture2DMipMap& MipMap		= FaceTexture->Mips(MipIndex);		
				check( MipData[FaceIndex][MipIndex] );

#if XBOX || WIIU
				appMemcpy( Dest, MipData[FaceIndex][MipIndex], MipMap.Data.GetBulkDataSize() );
#else
				UINT NumRows = 0;
				UINT SrcPitch = 0;
#if WITH_MOBILE_RHI
				if( GUsingMobileRHI )
				{
					extern UINT GetMipStride(UINT SizeX, EPixelFormat Format, UINT MipIndex);
					extern UINT GetMipNumRows(UINT SizeY, EPixelFormat Format, UINT MipIndex);
					EPixelFormat EffectiveFormat = (EPixelFormat)Owner->Format;
					// apply PVRTC4 override
					if ( Owner->bForcePVRTC4 && (GTextureFormatSupport & TEXSUPPORT_PVRTC) )
					{
						EffectiveFormat = PF_DXT5;
					}
					// Must use the mipmap's size (with a MipIndex of 0), because the Owner's SizeX
					// has already been resized down to the texture's FirstMip's size, so mimic
					// the below non-ES2 code and only use MipMap.Size
					SrcPitch = GetMipStride(MipMap.SizeX, EffectiveFormat, 0);
					NumRows = GetMipNumRows(MipMap.SizeY, EffectiveFormat, 0);
				}
				else
#endif		// WITH_MOBILE_RHI
				{
					UINT BlockSizeX = GPixelFormats[Owner->Format].BlockSizeX;	// Block width in pixels
					UINT BlockSizeY = GPixelFormats[Owner->Format].BlockSizeY;	// Block height in pixels
					UINT BlockBytes = GPixelFormats[Owner->Format].BlockBytes;
					UINT NumColumns = (MipMap.SizeX + BlockSizeX - 1) / BlockSizeX;	// Num-of columns in the source data (in blocks)
					NumRows    = (MipMap.SizeY + BlockSizeY - 1) / BlockSizeY;	// Num-of rows in the source data (in blocks)
					SrcPitch   = NumColumns * BlockBytes;		// Num-of bytes per row in the source data
				}

				if ( SrcPitch == DestPitch )
				{
					// Copy data, not taking into account stride!
					appMemcpy( Dest, MipData[FaceIndex][MipIndex], MipMap.Data.GetBulkDataSize() );
				}
				else
				{
					// Copy data, taking the stride into account!
					BYTE *Src = (BYTE*) MipData[FaceIndex][MipIndex];
					BYTE *Dst = (BYTE*) Dest;
					for ( UINT Row=0; Row < NumRows; ++Row )
					{
						appMemcpy( Dst, Src, SrcPitch );
						Src += SrcPitch;
						Dst += DestPitch;
					}
// @todo flash: Can we clean this up?
#if !FLASH

					check( (PTRINT(Src) - PTRINT(MipData[FaceIndex][MipIndex])) == PTRINT(MipMap.Data.GetBulkDataSize()) );
#endif
				}
#endif
				if( MipMap.Data.ShouldFreeOnEmpty() )
				{
					appFree( MipData[FaceIndex][MipIndex] );
				}
				MipData[FaceIndex][MipIndex] = NULL;
			}
		}
		else
		{
			// Default invalid cubemap is a 1x1 black RGBA8 one.	
			appMemset( Dest,0,sizeof(FColor) );
		}
	}
};

FTextureResource* UTextureCube::CreateResource()
{
	Validate();
	return new FTextureCubeResource(this);
}
