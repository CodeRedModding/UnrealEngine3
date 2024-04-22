/**************************************************************************

Filename    :   RHI_Texture.h
Content     :   RHI Texture and TextureManager header
Created     :   January 2010
Authors     :   Michael Antonov

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#ifndef INC_SF_Render_RHI_Texture_H
#define INC_SF_Render_RHI_Texture_H

#if WITH_GFx

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "Render/Render_Image.h"
#include "Render/Render_ThreadCommandQueue.h"
#include "Kernel/SF_List.h"
#include "Kernel/SF_Threads.h"
#include "Kernel/SF_HeapNew.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

#include "Engine.h"

class FSceneDepthTargetProxy;

namespace Scaleform
{
namespace Render
{
namespace RHI
{


// TextureFormat describes format of the texture and its caps.
// Format includes allowed usage capabilities and ImageFormat
// from which texture is supposed to be initialized.

struct TextureFormat
{
	ImageFormat              Format;
	EPixelFormat             RHIFormat;
	UByte                    BytesPerPixel;
	bool                     Mappable;
	Image::CopyScanlineFunc  CopyFunc;
	Image::CopyScanlineFunc  UncopyFunc;

	ImageFormat     GetImageFormat() const
	{
		return Format;
	}

	unsigned        GetPlaneCount() const
	{
		return ImageData::GetFormatPlaneCount ( GetImageFormat() );
	}
};

class MappedTexture;
class TextureManager;
class TextureResource;

// RHI Texture class implementation; it many actually include several HW
// textures (one for each ImageFormat plane).

class Texture : public Render::Texture
{
	public:
		enum AllocType
		{
		    Type_Normal,  // Externally allocated (UTexture).
		    Type_Managed,
		    Type_Dynamic,
		};

		// Bits stored in TextureFlags.
		enum TextureFlagBits
		{
		    TF_Rescale    = 0x01,
		    TF_SWMipGen   = 0x02,
		    TF_UserAlloc  = 0x04,
		};

		const TextureFormat*    pFormat;
		AllocType               Type;

		// If texture is currently mapped, it is here.
		MappedTexture*          pMap;

		struct HWTextureDesc
		{
			ImageSize               Size;
			FTextureResource*       Resource;
			UTexture*               UTex;
			UTexture2D*             UTex2D;
			UTextureRenderTarget2D* UTexRT2D;
			class TextureResource*  Tex;

			inline void    UpdateResource();

			FTextureRHIRef      GetRHI()
			{
				return Resource->TextureRHI;
			}

			inline FTexture2DRHIRef      Get2DRHI();
		};

		// TextureDesc array is allocated if more then one is needed.
		HWTextureDesc*          pTextures;
		HWTextureDesc           Texture0;

		Texture ( TextureManagerLocks* pmanagerLocks, const TextureFormat* pformat, unsigned mipLevels,
		          const ImageSize& size, unsigned use, ImageBase* pimage );
		Texture ( TextureManagerLocks* pmanagerLocks, UTexture* texture,
		          const ImageSize& size, ImageBase* pimage );
		~Texture();

		ImageFormat             GetImageFormat() const
		{
			return pFormat->Format;
		}
		TextureManager*         GetManager() const
		{
			return (TextureManager*)pManagerLocks->pManager;
		}
		bool                    IsValid() const
		{
			return pTextures != 0;
		}

		void                    LoseManager();
		void                    LoseTextureData();
		bool                    Initialize();
		bool                    Initialize ( UTexture* texture );
		void                    ReleaseHWTextures();

		// *** Interface implementation

//    virtual HAL*      GetRenderHAL() const { SF_ASSERT(0); return 0; } // TBD

		virtual Image*          GetImage() const
		{
			SF_ASSERT ( !pImage || ( pImage->GetImageType() != Image::Type_ImageBase ) );
			return ( Image* ) pImage;
		}
		virtual ImageFormat     GetFormat() const
		{
			return GetImageFormat();
		}

		virtual void            GetUVGenMatrix ( Matrix2F* mat ) const;

		virtual bool            Map ( ImageData* pdata, unsigned mipLevel, unsigned levelCount );
		virtual bool            Unmap();

		virtual bool            Update ( const UpdateDesc* updates, unsigned count = 1, unsigned mipLevel = 0 );
		virtual bool            Update();

		// Copies the image data from the hardware.
		SF_AMP_CODE ( virtual bool Copy ( ImageData* pdata ); )
};

class TextureResource : public FTextureResource
{
	public:
		Texture*         Owner;
		FTexture2DRHIRef Texture2DRHI;
		FSurfaceRHIRef   SurfaceRHI;

		TextureResource ( Texture* InOwner ) : Owner ( InOwner ) {}

		void InitTexture ( EPixelFormat InFormat, unsigned InFlags, Texture::HWTextureDesc& InTDesc );
		void ReleaseRHI();

		void InitDynamicRHI();
		void ReleaseDynamicRHI();
};

void Texture::HWTextureDesc::UpdateResource()
{
	if ( Tex )
	{
		Resource = Tex;
	}
	else if ( UTex )
	{
		Resource = UTex->Resource;
	}
	else
	{
		Resource = 0;
	}
}

inline FTexture2DRHIRef      Texture::HWTextureDesc::Get2DRHI()
{
	if ( Tex )
	{
		return Tex->Texture2DRHI;
	}
	else if ( UTex2D )
	{
		return ( ( FTexture2DResource* ) UTex2D->Resource )->GetTexture2DRHI();
	}
	else
	{
		return FTexture2DRHIRef();
	}
}

class DepthStencilResource : public FRenderResource
{
	public:
		FSceneDepthTargetProxy*   Owner;
        FViewport*                VPOwner;
		ImageSize                 Size;
        UBOOL                     bIsAllocated;
		FSurfaceRHIRef            DepthBuffer;

		DepthStencilResource ( ImageSize InSize ) : Owner ( NULL ), VPOwner ( NULL ), Size ( InSize ), bIsAllocated ( FALSE ) {}

		void Initialize ( FSceneDepthTargetProxy* InOwner );
		void Initialize ( FViewport* InOwner );

		// FRenderResource
		virtual void InitDynamicRHI();
		virtual void ReleaseDynamicRHI();
};

class DepthStencilSurface : public Render::DepthStencilSurface
{
	public:
		DepthStencilSurface ( TextureManagerLocks* pmanagerLocks, const ImageSize& size );
		~DepthStencilSurface();

		virtual ImageSize               GetSize() const
		{
			return Resource.Size;
		}
		bool                            Initialize();

		Texture::CreateState      State;
		DepthStencilResource      Resource;
};

// MappedTexture object repents a Texture mapped into memory with Texture::Map() call;
// it is also used internally during updates.
// The key part of this class is the Data object, stored Locked texture level plains.

class MappedTexture : public NewOverrideBase<StatRender_TextureManager_Mem>
{
		friend class Texture;

		Texture*      pTexture;
		// We support mapping sub-range of levels, in which case
		// StartMipLevel may be non-zero.
		unsigned      StartMipLevel;
		unsigned      LevelCount;
		// Pointer data that can be copied to.
		ImageData     Data;

		enum { PlaneReserveSize = 4 };
		ImagePlane    Planes[PlaneReserveSize];

	public:
		MappedTexture()
			: pTexture ( 0 ), StartMipLevel ( false ), LevelCount ( 0 ) { }
		~MappedTexture()
		{
			SF_ASSERT ( !IsMapped() );
		}

		bool        IsMapped()
		{
			return ( LevelCount != 0 );
		}
		bool        Map ( Texture* ptexture, unsigned mipLevel, unsigned levelCount );
		void        Unmap( bool applyUpdate );
};

// RHI Texture Manger.
// This class is responsible for creating textures and keeping track of them
// in the list.
//

class TextureManager : public Render::TextureManager
{
		friend class Texture;
		friend class DepthStencilSurface;

		typedef ArrayConstPolicy<8, 8, false>   KillListArrayPolicy;
		typedef ArrayLH < TextureResource*,
		        StatRender_TextureManager_Mem,
		        KillListArrayPolicy >    TextureArray;
		typedef ArrayLH < DepthStencilResource*,
		        StatRender_TextureManager_Mem,
		        KillListArrayPolicy>    DepthStencilArray;
		typedef List<Texture, Render::Texture>  TextureList;
		typedef List<DepthStencilSurface, Render::DepthStencilSurface> DepthStencilList;

		MappedTexture       MappedTexture0;
		ThreadCommandQueue* pRTCommandQueue;

		// Lists protected by TextureManagerLocks::TextureMutex.
		TextureList              Textures;
		TextureList              TextureInitQueue;
		DepthStencilList         DepthStencilInitQueue;
		TextureArray             TextureKillList;
		DepthStencilArray        DepthStencilKillList;
		ImageUpdateQueue         ImageUpdates;

		void            initTextureFormats();
		const Render::TextureFormat* getTextureFormat( ImageFormat format ) const;

		// Texture Memory-mapping support.
		MappedTexture*  mapTexture ( Texture* p, unsigned mipLevel, unsigned levelCount );
		MappedTexture*  mapTexture ( Texture* p )
		{
			return mapTexture ( p, 0, p->MipLevels );
		}
		void            unmapTexture ( Texture *ptexture, bool applyUpdate = true);
		bool            isMappable( const Render::TextureFormat* ptformat )
		{
			return ((TextureFormat*)ptformat)->Mappable;
		}

		void            processTextureKillList();
		void            processInitTextures();

	public:
		TextureManager ( ThreadCommandQueue* commandQueue = 0 );
		~TextureManager();

		void    PrepareForReset();
		void    RestoreAfterReset();
		// Used once texture manager is no longer necessary.
		void    Reset();

		// *** TextureManager
		virtual Render::Texture* CreateTexture ( ImageFormat format, unsigned mipLevels,
		        const ImageSize& size,
		        unsigned use, ImageBase* pimage,
		        Render::MemoryManager* manager = 0 );
		virtual Render::Texture* CreateTexture ( UTexture* InTexture,
		        ImageSize imgSize = ImageSize ( 0 ), Image* pimage = 0 );

		virtual DepthStencilSurface* CreateDepthStencilSurface ( const ImageSize& size, MemoryManager* manager = 0 );
		//virtual Render::DepthStencilSurface* CreateDepthStencilSurface()

		virtual unsigned        GetTextureUseCaps ( ImageFormat format );
		virtual bool            IsNonPow2Supported ( ImageFormat, UInt16 use )
		{
			return ( use & ImageUse_RenderTarget ) != 0;
		}
};


}
}
};  // namespace Scaleform::Render::RHI

#endif//WITH_GFx

#endif
