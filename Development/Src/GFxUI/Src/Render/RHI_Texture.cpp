/**************************************************************************

Filename    :   RHI_Texture.cpp
Content     :   RHI Texture and TextureManager implementation
Created     :   January 2010
Authors     :

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#include "GFxUI.h"

#if WITH_GFx

#include "ScaleformEngine.h"
#include "GFxUIClasses.h"
#include "GFxUIUISequenceClasses.h"
#include "ScaleformStats.h"

#include "RHI_Texture.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "Render/Render_TextureUtil.h"
#include "Kernel/SF_Debug.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

#include "../../Engine/Src/SceneRenderTargets.h"


namespace Scaleform
{
namespace Render
{
namespace RHI
{

extern TextureFormat RHITextureFormatMapping[];
#if WITH_ES2_RHI
extern TextureFormat RHITextureFormatMapping_Mobile[];
#endif

Texture::Texture ( TextureManagerLocks* pmanagerLocks, const TextureFormat* pformat,
                   unsigned mipLevels, const ImageSize& size, unsigned use,
                   ImageBase* pimage)
	: Render::Texture(pmanagerLocks, size, mipLevels, use, pimage),
	  pFormat(pformat), Type(Type_Managed), pMap(0)
{
	TextureCount = ( UByte ) pformat->GetPlaneCount();
	if ( TextureCount > 1 )
	{
		pTextures = ( HWTextureDesc* )
		            SF_HEAP_AUTO_ALLOC ( this, sizeof ( HWTextureDesc ) * TextureCount );
	}
	else
	{
		pTextures = &Texture0;
	}
	appMemset ( pTextures, 0, sizeof ( HWTextureDesc ) * TextureCount );

#if STATS
    INC_DWORD_STAT( STAT_GFxFTextureCount );
#endif
}

Texture::Texture(TextureManagerLocks* pmanagerLocks, UTexture* ptexture, const ImageSize& size, ImageBase* pimage) :
	Render::Texture(pmanagerLocks, size, 0, 0, pimage),
	pFormat(0), Type(Type_Normal), pMap(0)
{
	TextureFlags |= TF_UserAlloc;

	if ( ptexture )
	{
		GGFxGCManager->AddGCReferenceFor ( ptexture, StatRender_TextureManager_Mem );
	}

	pTextures = &Texture0;
	appMemset ( pTextures, 0, sizeof ( HWTextureDesc ) * TextureCount );

	pTextures[0].UTex = ptexture;
	pTextures[0].UpdateResource();
	pTextures[0].Size = size;
}

Texture::~Texture()
{
	//  pImage must be null, since ImageLost had to be called externally.
	SF_ASSERT ( pImage == 0 );

	Mutex::Locker  lock ( &pManagerLocks->TextureMutex );

	if ( ( State == State_Valid ) || ( State == State_Lost ) )
	{
		// pManagerLocks->pManager should still be valid for these states.
		SF_ASSERT ( pManagerLocks->pManager );
        if ( pNext )
        {
		    RemoveNode();
        }
		pNext = pPrev = 0;
		// If not on Render thread, add HW textures to queue.
		ReleaseHWTextures();
	}

	if ( ( pTextures != &Texture0 ) && pTextures )
	{
		SF_FREE ( pTextures );
	}

#if STATS
    if ( !( TextureFlags & TF_UserAlloc ) )
    {
        INC_DWORD_STAT( STAT_GFxFTextureCount );
    }
#endif

}

void Texture::GetUVGenMatrix ( Matrix2F* mat ) const
{
	// UV Scaling rules are as follows:
	//  - If image got scaled, divide by original size.
	//  - If image is padded, divide by new texture size.
	const ImageSize& sz = ( TextureFlags & TF_Rescale ) ? ImgSize : pTextures[0].Size;
	*mat = Matrix2F::Scaling ( 1.0f / ( float ) sz.Width, 1.0f / ( float ) sz.Height );
}


// LoseManager is called from ~TextureManager, cleaning out most of the
// texture internal state. Clean-out needs to be done during a Lock,
// in case ~Texture is called due to release in another thread.
void Texture::LoseManager()
{
	SF_ASSERT ( pMap == 0 );
	Render::Texture::LoseManager();
}

void Texture::LoseTextureData()
{
	SF_ASSERT ( pMap == 0 );

	Lock::Locker lock ( &pManagerLocks->ImageLock );
	ReleaseHWTextures();
	State   = State_Lost;

	if ( pImage )
	{
		// TextureLost may release 'this' Texture, so do it last.
		SF_ASSERT ( pImage->GetImageType() != Image::Type_ImageBase );
		Image* pimage = ( Image* ) pImage;
		pimage->TextureLost ( Image::TLR_DeviceLost );
	}
}

bool Texture::Initialize()
{
	if ( TextureFlags & TF_UserAlloc )
	{
		// NOTE: If this is after a reset, pTexture will be NULL, and the Initialize call will fail.
		// The user must manually call Texture::Initialize with the new pointer to the new texture.
		// This can be done for example, during a HALNotify event.
		return Initialize ( pTextures[0].UTex );
	}

	bool            resize  = false;
	ImageFormat     format  = GetImageFormat();
	TextureManager* pmanager = GetManager();
	unsigned        itex;

	// Determine sizes of textures.
	if ( State != State_Lost )
	{
		for ( itex = 0; itex < TextureCount; itex++ )
		{
			HWTextureDesc& tdesc = pTextures[itex];
			tdesc.Size = ImageData::GetFormatPlaneSize ( format, ImgSize, itex );
			if ( !pmanager->IsNonPow2Supported ( format, Use ) )
			{
				ImageSize roundedSize = ImageSize_RoundUpPow2 ( tdesc.Size );
				if ( roundedSize != tdesc.Size )
				{
					tdesc.Size = roundedSize;
					resize = true;
				}
			}
		}

		if ( resize && ( Use & ImageUse_Wrap ) )
		{
			if ( ImageData::IsFormatCompressed ( format ) )
			{
				SF_DEBUG_ERROR ( 1,
				                 "CreateTexture failed - Can't rescale compressed Wrappable image to Pow2" );
				State = State_InitFailed;
				return false;
			}
			TextureFlags |= TF_Rescale;
		}
	}

	// Determine how many mipLevels we should have and whether we can
	// auto-generate them or not.
	unsigned allocMipLevels = MipLevels;
	unsigned CreateFlags = 0;

	if ( Use & ImageUse_GenMipmaps )
	{
		SF_ASSERT ( MipLevels == 1 );
		//if (!pFormat->CanAutoGenMipmaps())
		{
			TextureFlags |= TF_SWMipGen;
			// If using SW MipGen, determine how many mip-levels we should have.
			allocMipLevels = 31;
			for ( itex = 0; itex < TextureCount; itex++ )
			{
				allocMipLevels = Alg::Min ( allocMipLevels,
				                            ImageSize_MipLevelCount ( pTextures[itex].Size ) );
			}
			MipLevels = ( UByte ) allocMipLevels;
		}
	}

	// MA: Gamebryo-compatibility work-around from GFx 3.x, TBD whether still necessary.
	// For some reason we need to specify levelsNeeded-1, otherwise surface accesses
	// may crash (when running with Gamebryo). So, 256x256 texture has levelCount of 8 (not 9).
	// There is no problem with dropping one level unless user intends to Map it (not typical).
	if ( ( allocMipLevels > 1 ) && ( ( Use & ImageUse_Map_Mask ) == 0 ) )
	{
		allocMipLevels--;
		MipLevels--;
	}

	// Only use Dynamic textures for updatable/mappable textures.
	bool allowDynamicTexture = ( ( Use & ( ImageUse_PartialUpdate | ImageUse_Map_Mask ) ) != 0 );

	Type = Texture::Type_Managed;
	if ( allowDynamicTexture )
	{
		CreateFlags |= TexCreate_Dynamic;
		Type = Type_Dynamic;
	}

	if ( Use & ImageUse_RenderTarget )
	{
		CreateFlags |= TexCreate_ResolveTargetable;
		Type = Type_Dynamic;
	}
	else
	{
		// Use NoTiling for all gfx-created textures since they are accessed with Map or Update
		CreateFlags |= TexCreate_NoTiling;

#if XBOX || WIIU
		CreateFlags |= TexCreate_Uncooked;
#endif
	}

	// Create textures
	for ( itex = 0; itex < TextureCount; itex++ )
	{
		HWTextureDesc& tdesc = pTextures[itex];

		tdesc.Tex = new TextureResource ( this );
		tdesc.Tex->InitTexture ( pFormat->RHIFormat, CreateFlags, tdesc );
		tdesc.UpdateResource();
	}

	if ( Use & ImageUse_RenderTarget )
	{
		ImgSize = pTextures[0].Size;
	}

	// Upload image content to texture, if any.
	if ( pImage && !Update() )
	{
		SF_DEBUG_ERROR ( 1, "CreateTexture failed - couldn't initialize texture" );
		ReleaseHWTextures();
		return false;
	}

	State = State_Valid;
	return true;
}

bool Texture::Initialize ( UTexture* InTexture )
{
	if ( !InTexture )
	{
		return false;
	}

	check ( pTextures[0].UTex == InTexture );

	pTextures[0].Tex = NULL;
	pTextures[0].UTex = InTexture;
	pTextures[0].UTex2D = Cast<UTexture2D> ( InTexture );
	pTextures[0].UTexRT2D = Cast<UTextureRenderTarget2D> ( InTexture );
	pTextures[0].UpdateResource();

    if (pTextures[0].UTex2D)
    {
	MipLevels = pTextures[0].UTex2D->ResidentMips;
		  ImgSize = ImageSize(pTextures[0].UTex2D->OriginalSizeX, pTextures[0].UTex2D->OriginalSizeY);
    }
    else
		  MipLevels = 1;

	pFormat = 0;
	TextureManager* pmanager = GetManager();

	// If an image is provided, try to obtain the texture format from the image.
	if ( pImage )
	{
		pFormat = (TextureFormat*)pmanager->getTextureFormat( pImage->GetFormatNoConv() );
	}

	// Otherwise, figure out the texture format, based on the mapping table.
	if ( pFormat == 0 )
	{
        TextureFormat* Formats;
    #if WITH_ES2_RHI
        if (GUsingMobileRHI)
        {
            Formats = RHITextureFormatMapping_Mobile;
        }
        else
    #endif
        {
            Formats = RHITextureFormatMapping;
        }

		for ( ; Formats->Format != Image_None; Formats++ )
		{
			if ( Formats->RHIFormat == ( pTextures[0].UTex2D ? pTextures[0].UTex2D->Format : pTextures[0].UTexRT2D->Format ) )
			{
				pFormat = (TextureFormat*)pmanager->getTextureFormat ( Formats->Format );
				break;
			}
		}
	}

	// Could not determine the format.
	if ( !pFormat )
	{
		SF_DEBUG_WARNING(1, "Texture::Initialize - couldn't determine ImageFormat of user supplied texture.");
		pFormat = (const TextureFormat*)pmanager->getTextureFormat( Image_B8G8R8A8 );
	}

	// Fill out the HW description.
	if (pTextures[0].UTex2D)
		pTextures[0].Size.SetSize ( pTextures[0].UTex2D->SizeX, pTextures[0].UTex2D->SizeY );
	else
	{
		SF_ASSERT(pTextures[0].UTexRT2D);
		pTextures[0].Size.SetSize ( pTextures[0].UTexRT2D->SizeX, pTextures[0].UTexRT2D->SizeY );
	}

	// Override the image size if it was not provided.
	if ( ImgSize == ImageSize ( 0 ) )
	{
		ImgSize = pTextures[0].Size;
	}

	State = State_Valid;
	return true;
}

bool Texture::Update()
{
	ImageFormat     format   = GetImageFormat();
	TextureManager* pmanager = GetManager();
	bool            rescale  = ( TextureFlags & TF_Rescale ) ? true : false;
	bool            swMipGen = ( TextureFlags & TF_SWMipGen ) ? true : false;
	bool            convert  = false;
	ImageData       *psource;
	ImageData       *pdecodeTarget = 0, *prescaleTarget = 0;
	ImageData       imageData1, imageData2;
	Ptr<RawImage>   pimage1, pimage2;
	unsigned        sourceMipLevels = GetMipmapCount(); // May be different from MipLevels

	// Texture update proceeds in four (optional) steps:
	//   1. Image::Decode - Done unless rescaling directly from RawImage.
	//   2. Rescale       - Done if non-pow2 textures are not supported as necessary.
	//   3. Convert       - Needed if conversion wasn't applied in Decode.
	//   4. SW Mipmap Gen - Loop to generate SW mip-maps, may also have a convert step.

	// Although Decode can do scan-line conversion, Convert step is necessary
	// because our Rescale and GenerateMipLevel functions don't support all RHI
	// destination formats. If non-supported format is encountered, conversion
	// is delayed till after rescale (which, of course, requires an extra image buffer).

	ImageFormat      rescaleBuffFromat = format;
	ImageRescaleType rescaleType = ResizeNone;

	if ( rescale )
	{
		if ( pFormat->BytesPerPixel == 4 )
		{
			rescaleType = ResizeRgbaToRgba;
			rescaleBuffFromat = Image_R8G8B8A8;
		}
		else if ( pFormat->BytesPerPixel == 1 )
		{
			rescaleType = ResizeGray;
		}
		else
		{
			rescaleBuffFromat = Image_R8G8B8A8;
			convert = true;
		}
	}
	if ( swMipGen && ! ( pFormat->BytesPerPixel == 4 || pFormat->BytesPerPixel == 1 ) )
	{
		convert = true;
	}

	// *** 1. Decode from source pImage to Image1/MappedTexture

	Lock::Locker  imageLock ( &pManagerLocks->ImageLock );

	if ( !pImage || ( TextureFlags & TF_UserAlloc ) )
	{
		return false;
	}

	// Decode is not needed if RawImage is used directly as a source for rescale.
	if ( ! ( ( pImage->GetImageType() == Image::Type_RawImage ) && rescale ) )
	{
		// Determine decodeTarget -> Image1 if rescale / convert will take place
		if ( rescale || convert )
		{
			pimage1 = *RawImage::Create ( rescaleBuffFromat, sourceMipLevels, ImgSize, 0 );
			if ( !pimage1 )
			{
				return false;
			}
			pimage1->GetImageData ( &imageData1 );
			imageData1.Format = ( ImageFormat ) ( format | ImageFormat_Convertible );
			pdecodeTarget = &imageData1;
		}
		else
		{
			if ( !pmanager->mapTexture ( this ) )
			{
				return false;
			}
			pdecodeTarget = &pMap->Data;
		}

		// Decode to decode_Target (Image1 or MappedTexture)
		if (!pImage->Decode ( pdecodeTarget, convert ? &Image::CopyScanlineDefault : pFormat->CopyFunc ))
        {
            // Note: if decoding failed, still return true, but do not apply the update.
            // This doesn't necessarily mean there is an issue with the image, for instance,
            // with video, a new frame might not be available immediately.
            pmanager->unmapTexture(this, false);
            return true;
        }
		psource = pdecodeTarget;
	}
	else
	{
		( ( RawImage* ) pImage )->GetImageData ( &imageData1 );
		psource = &imageData1;
	}

	// *** 2. Rescale - from source to Image2/MappedTexture

	if ( rescale )
	{
		if ( convert )
		{
			pimage2 = *RawImage::Create ( format, sourceMipLevels, pTextures[0].Size, 0 );
			if ( !pimage2 )
			{
				return false;
			}
			pimage2->GetImageData ( &imageData2 );
			prescaleTarget = &imageData2;
		}
		else
		{
			if ( !pmanager->mapTexture ( this ) )
			{
				return false;
			}
			prescaleTarget = &pMap->Data;
		}

		if ( rescaleType == ResizeNone )
		{
			rescaleType = GetImageFormatRescaleType ( format );
			SF_ASSERT ( rescaleType != ResizeNone );
		}
		RescaleImageData ( *prescaleTarget, *psource, rescaleType );
		psource = prescaleTarget;
	}

	// *** 3. Convert - from temp source to MappedTexture

	if ( convert )
	{
		if ( !pmanager->mapTexture ( this ) )
		{
			return false;
		}
		ConvertImageData ( pMap->Data, *psource, pFormat->CopyFunc );
	}

	// *** 4. Generate Mip-Levels

	if ( swMipGen )
	{
		unsigned formatPlaneCount = ImageData::GetFormatPlaneCount ( format );
		SF_ASSERT ( sourceMipLevels == 1 );

		// For each level, generate next mip-map from source to target.
		// Source may be either RawImage, Image1/2, or even MappedTexture itself.
		// Target will be Image1/2 if conversion is needed, MappedTexture otherwise.

		for ( unsigned iplane = 0; iplane < formatPlaneCount; iplane++ )
		{
			ImagePlane splane, tplane;
			psource->GetMipLevelPlane ( 0, iplane, &splane );

			for ( unsigned level = 1; level < MipLevels; level++ )
			{
				pMap->Data.GetMipLevelPlane ( level, iplane, &tplane );

				if ( !convert )
				{
					GenerateMipLevel ( tplane, splane, format, iplane );
					// If we generated directly into MappedTexture,
					// texture will be used as source for next iteration.
					splane = tplane;
				}
				else
				{
					// Extra conversion step means, source has only one level.
					// We reuse it through GenerateMipLevel, which allows source
					// and destination to be the same.
					ImagePlane dplane ( splane );
					dplane.SetNextMipSize();
					GenerateMipLevel ( dplane, splane, format, iplane );
					ConvertImagePlane ( tplane, dplane, format, iplane,
					                    pFormat->CopyFunc, psource->GetColorMap() );
					splane.Width  = dplane.Width;
					splane.Height = dplane.Height;
				}
			}
		}
	}

	pmanager->unmapTexture ( this );
	return true;
}

void Texture::ReleaseHWTextures()
{
	TextureManager* pmanager = GetManager();

    // If in the game thread, and the rendering thread is not suspending, we must
	// queue texture deletion. Otherwise, it can be done immediately.
	UBOOL useKillList = IsInGameThread() && !GIsRenderingThreadSuspended;

	for ( unsigned itex = 0; itex < TextureCount; itex++ )
	{
		TextureResource* ptexture = pTextures[itex].Tex;
		if ( ptexture )
		{
#if STATS
            if ( pFormat )
            {
                DEC_DWORD_STAT_BY( STAT_GFxFTextureMem, pTextures[itex].Size.Area() * pFormat->BytesPerPixel );
            }
#endif
			if ( useKillList )
			{
				pmanager->TextureKillList.PushBack ( ptexture );
			}
			else
			{
				delete ptexture;
			}
		}
		if ( pTextures[itex].UTex )
		{
			GGFxGCManager->RemoveGCReferenceFor ( pTextures[itex].UTex );
		}

		pTextures[itex].Tex = 0;
		pTextures[itex].UTex = 0;
		pTextures[itex].UTex2D = 0;
		pTextures[itex].Resource = 0;
	}
}

bool Texture::Map ( ImageData* pdata, unsigned mipLevel, unsigned levelCount )
{
	SF_ASSERT ( ( Use & ImageUse_Map_Mask ) != 0 );
	SF_ASSERT ( !pMap );

	if ( levelCount == 0 )
	{
		levelCount = MipLevels - mipLevel;
	}

	if ( !GetManager()->mapTexture ( this, mipLevel, levelCount ) )
	{
		SF_DEBUG_WARNING ( 1, "Texture::Map failed - couldn't map texture" );
		return false;
	}

	pdata->Initialize ( GetImageFormat(), levelCount,
	                    pMap->Data.pPlanes, pMap->Data.RawPlaneCount, true );
	pdata->Use = Use;
	return true;
}

bool Texture::Unmap()
{
	if ( !pMap )
	{
		return false;
	}
	GetManager()->unmapTexture ( this );
	return true;
}


bool Texture::Update ( const UpdateDesc* updates, unsigned count, unsigned mipLevel )
{
	if ( !GetManager()->mapTexture ( this, mipLevel, 1 ) )
	{
		SF_DEBUG_WARNING ( 1, "Texture::Update failed - couldn't map texture" );
		return false;
	}

	ImageFormat format = GetImageFormat();
	ImagePlane  dplane;

	for ( unsigned i = 0; i < count; i++ )
	{
		const UpdateDesc &desc = updates[i];
		ImagePlane splane ( desc.SourcePlane );

		pMap->Data.GetPlane ( desc.PlaneIndex, &dplane );
		dplane.pData += desc.DestRect.y1 * dplane.Pitch +
		                desc.DestRect.x1 * pFormat->BytesPerPixel;

		splane.SetSize ( desc.DestRect.GetSize() );
		dplane.SetSize ( desc.DestRect.GetSize() );
		ConvertImagePlane ( dplane, splane, format, desc.PlaneIndex, pFormat->CopyFunc, 0 );
	}

	GetManager()->unmapTexture ( this );
	return true;
}

#ifdef SF_AMP_SERVER
bool Texture::Copy ( ImageData* pdata )
{
	Image::CopyScanlineFunc puncopyFunc = pFormat->UncopyFunc;
	if ( !GetManager() || pFormat->Format != pdata->Format || !puncopyFunc )
	{
		// - No texture manager, OR
		// - Output format is different from the source input format of this texture (unexpected, because
		//   we should be copying back into the image's original source format) OR
		// - We don't know how to uncopy this format.
		return false;
	}

	// Map the texture.
	bool alreadyMapped = ( pMap != 0 );
	unsigned mipCount = GetMipmapCount();
	if ( !alreadyMapped && !GetManager()->mapTexture ( this, 0, mipCount ) )
	{
		SF_DEBUG_WARNING ( 1, "Texture::Copy failed - couldn't map texture" );
		return false;
	}
	SF_ASSERT ( pMap );

	// Copy the planes into pdata, using the reverse copy function.
	SF_ASSERT ( pdata->GetPlaneCount() == pMap->Data.GetPlaneCount() );
	int ip;
	for ( ip = 0; ip < pdata->RawPlaneCount; ip++ )
	{
		ImagePlane splane, dplane;
		pdata->GetPlane ( ip, &dplane );
		pMap->Data.GetPlane ( ip, &splane );

		ConvertImagePlane ( dplane, splane, GetFormat(), ip, puncopyFunc, 0 );
	}

	// Unmap the texture, if we mapped it.
	if ( !alreadyMapped )
	{
		GetManager()->unmapTexture ( this );
	}

	return true;
}
#endif


// ***** MappedTexture

bool MappedTexture::Map ( Texture* ptexture, unsigned mipLevel, unsigned levelCount )
{
	SF_ASSERT ( !IsMapped() );
	SF_ASSERT ( ( mipLevel + levelCount ) <= ptexture->MipLevels );

	// Initialize Data as efficiently as possible.
	if ( levelCount <= PlaneReserveSize )
	{
		Data.Initialize ( ptexture->GetImageFormat(), levelCount, Planes, ptexture->GetPlaneCount(), true );
	}
	else if ( !Data.Initialize ( ptexture->GetImageFormat(), levelCount, true ) )
	{
		return false;
	}

	pTexture      = ptexture;
	StartMipLevel = mipLevel;
	LevelCount    = levelCount;

	bool     failedLock   = false;
	unsigned textureCount = ptexture->TextureCount;

	for ( unsigned itex = 0; itex < textureCount; itex++ )
	{
		Texture::HWTextureDesc &tdesc = pTexture->pTextures[itex];
		ImagePlane              plane ( tdesc.Size, 0 );

		for ( unsigned i = 0; i < StartMipLevel; i++ )
		{
			plane.SetNextMipSize();
		}

		for ( unsigned level = 0; level < levelCount; level++ )
		{
			UINT tempPitch = plane.Pitch;
			plane.pData = ( UByte* ) RHILockTexture2D ( tdesc.Get2DRHI(), level, TRUE, tempPitch, FALSE );
			plane.Pitch = tempPitch;
			SF_ASSERT(plane.Pitch >= plane.Width);

			if ( plane.pData )
			{
				plane.DataSize = ImageData::GetMipLevelSize ( Data.GetFormat(), plane.GetSize(), level );
			}
			else
			{
				plane.Pitch    = 0;
				plane.pData    = 0;
				plane.DataSize = 0;
				failedLock = true;
			}

			Data.SetPlane ( level * textureCount + itex, plane );
			// Prepare for next level.
			plane.SetNextMipSize();
		}
	}

	if ( failedLock )
	{
		SF_DEBUG_ERROR ( 1, "RHI::MappedTexture::Map failed - LockRect failure" );
		Unmap(false);
		return false;
	}

	pTexture->pMap = this;
	return true;
}

void MappedTexture::Unmap(bool applyUpdate)
{
	unsigned textureCount = pTexture->TextureCount;

	for ( unsigned itex = 0; itex < textureCount; itex++ )
	{
		Texture::HWTextureDesc &tdesc = pTexture->pTextures[itex];
		ImagePlane plane;

		for ( unsigned level = 0; level < LevelCount; level++ )
		{
			Data.GetPlane ( level * textureCount + itex, &plane );
			if ( plane.pData )
			{
                if ( GRHIShaderPlatform == SP_PCD3D_SM4 || GRHIShaderPlatform == SP_PCD3D_SM5 )
                    RHIUnlockTexture2D ( tdesc.Get2DRHI(), level + StartMipLevel, !applyUpdate );
                else
				    RHIUnlockTexture2D ( tdesc.Get2DRHI(), level + StartMipLevel, FALSE );

				plane.pData = 0;
			}
		}
	}

	pTexture->pMap = 0;
	pTexture       = 0;
	StartMipLevel  = 0;
	LevelCount     = 0;
}

void TextureResource::InitTexture ( EPixelFormat InFormat, unsigned InFlags, Texture::HWTextureDesc& InTDesc )
{
	TextureRHI = Texture2DRHI = RHICreateTexture2D ( InTDesc.Size.Width, InTDesc.Size.Height, InFormat, Owner->MipLevels, InFlags, NULL );

#if STATS
    INC_DWORD_STAT_BY( STAT_GFxFTextureMem, InTDesc.Size.Area() * GPixelFormats[InFormat].BlockBytes );
#endif
}

void TextureResource::InitDynamicRHI()
{
	if ( Owner->State == Texture::State_Lost )
	{
		Owner->Initialize();
	}
}

void TextureResource::ReleaseDynamicRHI()
{
	if ( Owner->Type != Texture::Type_Managed )
	{
		Owner->LoseTextureData();
	}
}

void TextureResource::ReleaseRHI()
{
	TextureRHI.SafeRelease();
	SurfaceRHI.SafeRelease();
}


// ***** DepthStencilSurface

void DepthStencilResource::InitDynamicRHI()
{
	if ( Owner )
	{
		DepthBuffer = Owner->GetDepthTargetSurface();
#if !XBOX
//        check ( DepthBuffer );
#endif
	}
    else if ( VPOwner )
    {
        DepthBuffer = RHIGetViewportDepthBuffer ( VPOwner->GetViewportRHI() );
    }

    if ( !DepthBuffer )
	{
		DepthBuffer = RHICreateTargetableSurface ( Size.Width, Size.Height, PF_DepthStencil, FTexture2DRHIRef(),
		              TargetSurfCreate_Dedicated | TargetSurfCreate_DepthBufferToMatchBackBuffer, TEXT ( "GFxDepth" ) );
        bIsAllocated = TRUE;
	}
}

void DepthStencilResource::ReleaseDynamicRHI()
{
	DepthBuffer.SafeRelease();
    bIsAllocated = FALSE;
}

void DepthStencilResource::Initialize ( FSceneDepthTargetProxy* InOwner )
{
	Owner = InOwner;
	if ( bInitialized )
	{
		ReleaseDynamicRHI();
		InitDynamicRHI();
	}
	else
	{
		InitResource();
	}
}

void DepthStencilResource::Initialize ( FViewport* InOwner )
{
    VPOwner = InOwner;
    if ( bInitialized )
    {
        ReleaseDynamicRHI();
        InitDynamicRHI();
    }
    else
    {
        InitResource();
    }
}


DepthStencilSurface::DepthStencilSurface(TextureManagerLocks* pmanagerLocks, const ImageSize& size)
	:   Render::DepthStencilSurface( pmanagerLocks ), State( Texture::State_InitPending )
	,   Resource(size)
{
}

DepthStencilSurface::~DepthStencilSurface()
{
	check ( IsInRenderingThread() );
	Resource.ReleaseResource();
}

bool DepthStencilSurface::Initialize()
{
	Resource.ReleaseDynamicRHI();
	Resource.InitDynamicRHI();
	State = Texture::State_Valid;
	return State == Texture::State_Valid;
}


// ***** TextureManager

TextureManager::TextureManager(ThreadCommandQueue* commandQueue)
	: pRTCommandQueue(commandQueue)
{
}

TextureManager::~TextureManager()
{
	Mutex::Locker lock ( &pLocks->TextureMutex );
	Reset();
	pLocks->pManager = 0;
}

void TextureManager::Reset()
{
	Mutex::Locker lock ( &pLocks->TextureMutex );

	// InitTextureQueue MUST be empty, or there was a thread service problem.
	SF_ASSERT ( TextureInitQueue.IsEmpty() );
	processTextureKillList();

	// Notify all textures
	while ( !Textures.IsEmpty() )
	{
		Textures.GetFirst()->LoseManager();
	}
}


// Image to Texture format conversion and mapping table,
// organized by the order of preferred image conversions.

#if WITH_ES2_RHI
TextureFormat RHITextureFormatMapping_Mobile[] =
{
	{ Image_R8G8B8A8,    PF_A8R8G8B8, 4, true,  &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault },
	{ Image_B8G8R8A8,    PF_A8R8G8B8, 4, false, &Image_CopyScanline32_SwapBR,          &Image_CopyScanline32_SwapBR },

	{ Image_R8G8B8,      PF_A8R8G8B8, 4, false, &Image_CopyScanline24_Extend_RGB_RGBA, &Image_CopyScanline32_Retract_RGBA_RGB },
	{ Image_B8G8R8,      PF_A8R8G8B8, 4, false, &Image_CopyScanline24_Extend_RGB_BGRA, &Image_CopyScanline32_Retract_BGRA_RGB },
	{ Image_A8,          PF_G8,       1, true,  &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault },

	{ Image_DXT1,        PF_DXT1,     0, true,  &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault},
	{ Image_DXT3,        PF_DXT3,     0, true,  &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault},
	{ Image_DXT5,        PF_DXT5,     0, true,  &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault},

	{ Image_Y8_U2_V2,    PF_G8,       1, true,  &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault },
	{ Image_Y8_U2_V2_A8, PF_G8,       1, true,  &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault },

	{ Image_None,        PF_Unknown,  0, false, 0, 0 }
};
#endif

TextureFormat RHITextureFormatMapping[] =
{
#if XBOX
	{ Image_R8G8B8A8,    PF_A8R8G8B8, 4, false, &Image_CopyScanline32_RGBA_ARGB,       &Image_CopyScanline32_RGBA_ARGB },
	{ Image_B8G8R8A8,    PF_A8R8G8B8, 4, false, &Image_CopyScanline32_SwapBR,          &Image_CopyScanline32_SwapBR },

	{ Image_R8G8B8,      PF_A8R8G8B8, 4, false, &Image_CopyScanline24_Extend_RGB_RGBA, &Image_CopyScanline32_Retract_RGBA_RGB },
	{ Image_B8G8R8,      PF_A8R8G8B8, 4, false, &Image_CopyScanline24_Extend_RGB_BGRA, &Image_CopyScanline32_Retract_BGRA_RGB },

#elif NGP
	{ Image_R8G8B8A8,    PF_A8R8G8B8, 4, false, &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault },
	{ Image_B8G8R8A8,    PF_A8R8G8B8, 4, true,  &Image_CopyScanline32_SwapBR,          &Image_CopyScanline32_SwapBR },

	{ Image_R8G8B8,      PF_A8R8G8B8, 4, false, &Image_CopyScanline24_Extend_RGB_RGBA, &Image_CopyScanline32_Retract_RGBA_RGB },
	{ Image_B8G8R8,      PF_A8R8G8B8, 4, false, &Image_CopyScanline24_Extend_RGB_BGRA, &Image_CopyScanline32_Retract_BGRA_RGB },

#else
	{ Image_R8G8B8A8,    PF_A8R8G8B8, 4, false, &Image_CopyScanline32_SwapBR,          &Image_CopyScanline32_SwapBR },
	{ Image_B8G8R8A8,    PF_A8R8G8B8, 4, true,  &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault },

	{ Image_R8G8B8,      PF_A8R8G8B8, 4, false, &Image_CopyScanline24_Extend_RGB_BGRA, &Image_CopyScanline32_Retract_BGRA_RGB },
	{ Image_B8G8R8,      PF_A8R8G8B8, 4, false, &Image_CopyScanline24_Extend_RGB_RGBA, &Image_CopyScanline32_Retract_RGBA_RGB },
#endif
	{ Image_A8,          PF_G8,       1, true,  &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault },

	{ Image_DXT1,        PF_DXT1,     0, true,  &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault},
	{ Image_DXT3,        PF_DXT3,     0, true,  &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault},
	{ Image_DXT5,        PF_DXT5,     0, true,  &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault},

	{ Image_Y8_U2_V2,    PF_G8,       1, true,  &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault },
	{ Image_Y8_U2_V2_A8, PF_G8,       1, true,  &Image::CopyScanlineDefault,           &Image::CopyScanlineDefault },

	{ Image_None,        PF_Unknown,  0, false, 0, 0 }
};


const Render::TextureFormat*  TextureManager::getTextureFormat ( ImageFormat format ) const
{
    TextureFormat* Formats;
#if WITH_ES2_RHI
    if (GUsingMobileRHI)
    {
        Formats = RHITextureFormatMapping_Mobile;
    }
    else
#endif
    {
        Formats = RHITextureFormatMapping;
    }
	for ( ; Formats->Format != Image_None; Formats++ )
	{
		if ( Formats->GetImageFormat() == format )
		{
			return (Render::TextureFormat*)Formats;
		}
	}
	return 0;
}


MappedTexture* TextureManager::mapTexture ( Texture* ptexture, unsigned mipLevel, unsigned levelCount )
{
	MappedTexture* pmap;

	if ( !MappedTexture0.IsMapped() )
	{
		pmap = &MappedTexture0;
	}
	else
	{
		pmap = SF_HEAP_AUTO_NEW ( this ) MappedTexture;
		if ( !pmap )
		{
			return 0;
		}
	}

	if ( pmap->Map ( ptexture, mipLevel, levelCount ) )
	{
		return pmap;
	}
	if ( pmap != &MappedTexture0 )
	{
		delete pmap;
	}
	return 0;
}

void TextureManager::unmapTexture ( Texture *ptexture, bool applyUpdate )
{
	MappedTexture *pmap = ptexture->pMap;
	pmap->Unmap(applyUpdate);
	if ( pmap != &MappedTexture0 )
	{
		delete pmap;
	}
}

void TextureManager::processTextureKillList()
{
	for ( unsigned i = 0; i < TextureKillList.GetSize(); i++ )
	{
		delete TextureKillList[i];
	}
	TextureKillList.Clear();
}

void TextureManager::processInitTextures()
{
	if ( !TextureInitQueue.IsEmpty() )
	{
		do
		{
			Texture* ptexture = TextureInitQueue.GetFirst();
			ptexture->RemoveNode();
			ptexture->pPrev = ptexture->pNext = 0;
			if ( ptexture->Initialize() )
			{
				Textures.PushBack ( ptexture );
			}
		}
		while ( !TextureInitQueue.IsEmpty() );
		pLocks->TextureInitWC.NotifyAll();
	}
}

Render::Texture* TextureManager::CreateTexture ( ImageFormat format, unsigned mipLevels,
        const ImageSize& size,
        unsigned use, ImageBase* pimage,
        Render::MemoryManager* allocManager )
{
	SF_UNUSED ( allocManager );

	TextureFormat* ptformat = (TextureFormat*)precreateTexture(format, use, pimage);

	if ( !ptformat )
	{
		return 0;
	}

	Texture* ptexture = SF_HEAP_AUTO_NEW ( this ) Texture ( pLocks, ptformat, mipLevels, size, use, pimage );
	if ( !ptexture )
	{
		return 0;
	}
	if ( !ptexture->IsValid() )
	{
		ptexture->Release();
		return 0;
	}

	Mutex::Locker lock ( &pLocks->TextureMutex );

	if ( IsInRenderingThread() )
	{
		// Before initializing texture, process previous requests, if any.
		processTextureKillList();
		processInitTextures();
		if ( ptexture->Initialize() )
		{
			Textures.PushBack ( ptexture );
		}
	}
	else
	{
		TextureInitQueue.PushBack ( ptexture );
		if ( pRTCommandQueue )
		{
			pLocks->TextureMutex.Unlock();
			pRTCommandQueue->PushThreadCommand ( &ServiceCommandInstance );
			pLocks->TextureMutex.DoLock();
		}
		while ( ptexture->State == Texture::State_InitPending )
		{
			pLocks->TextureInitWC.Wait ( &pLocks->TextureMutex );
		}
	}

	// Clear out 'pImage' reference if it's not supposed to be kept. It is safe to do this
	// without ImageLock because texture hasn't been returned yet, so this is the only
	// thread which has access to it.
	if ( use & ImageUse_InitOnly )
	{
		ptexture->pImage = 0;
	}

	// If texture was properly initialized, it would've been added to list.
	if ( ptexture->State == Texture::State_InitFailed )
	{
		ptexture->Release();
		return 0;
	}
	return ptexture;
}

Render::Texture* TextureManager::CreateTexture ( UTexture* InTexture, ImageSize imgSize, Image* image )
{
	if ( !InTexture )
	{
		return 0;
	}

	Texture* ptexture = SF_HEAP_AUTO_NEW ( this ) Texture ( pLocks, InTexture, imgSize, image );
	if ( !ptexture )
	{
		return 0;
	}
	if ( !ptexture->IsValid() )
	{
		ptexture->Release();
		return 0;
	}

	if ( ptexture->Initialize ( InTexture ) )
	{
		Mutex::Locker lock ( &pLocks->TextureMutex );
		Textures.PushBack ( ptexture );
	}

	// If texture was properly initialized, it would've been added to list.
	if ( ptexture->State == Texture::State_InitFailed )
	{
		ptexture->Release();
		return 0;
	}
	return ptexture;
}

DepthStencilSurface* TextureManager::CreateDepthStencilSurface ( const ImageSize& size, MemoryManager* manager )
{
	return SF_NEW DepthStencilSurface ( pLocks, size );
}

unsigned TextureManager::GetTextureUseCaps ( ImageFormat format )
{
	// ImageUse_InitOnly (ImageUse_NoDataLoss alias) ok while textures are Managed
	unsigned use = ImageUse_InitOnly | ImageUse_Update;
	if ( !ImageData::IsFormatCompressed ( format ) && !GUsingES2RHI )
	{
		use |= ImageUse_PartialUpdate | ImageUse_GenMipmaps;
	}

	const TextureFormat* ptformat = (const TextureFormat*)getTextureFormat( format );
	if (!ptformat)
	{
		return 0;
	}
	if ( ptformat->Mappable && !GUsingES2RHI )
	{
		use |= ImageUse_MapInUpdate;
	}
	return use;
}


}
}
};  // namespace Scaleform::Render::RHI

#endif//WITH_GFx
