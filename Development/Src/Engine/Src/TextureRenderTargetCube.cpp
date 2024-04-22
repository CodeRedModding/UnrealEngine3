/*=============================================================================
	TextureRenderTargetCube.cpp: UTextureRenderTargetCube implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	UTextureRenderTargetCube
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UTextureRenderTargetCube);

/** 
 * Initialize the settings needed to create a render target texture
 * and create its resource
 * @param	InSizeX - width of the texture
 * @param	InFormat - format of the texture
 */
void UTextureRenderTargetCube::Init(UINT InSizeX, EPixelFormat InFormat)
{
	check(InSizeX > 0);
	check(!(InSizeX % GPixelFormats[InFormat].BlockSizeX));
	check(FTextureRenderTargetResource::IsSupportedFormat(InFormat));

	// set required size/format
	SizeX		= InSizeX;
	Format		= InFormat;

	// override for platforms that don't support PF_G8 as render target
	if (GIsGame &&
		!GSupportsRenderTargetFormat_PF_G8 &&
		Format == PF_G8)
	{
		Format = PF_A8R8G8B8;					
	}

	// Recreate the texture's resource.
	UpdateResource();
}

/**
 * Serialize properties (used for backwards compatibility with main branch)
 */
void UTextureRenderTargetCube::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

/**
* Returns the size of the object/ resource for display to artists/ LDs in the Editor.
*
* @return size of resource as to be displayed to artists/ LDs in the Editor.
*/
INT UTextureRenderTargetCube::GetResourceSize()
{
	// Calculate size based on format.
	INT BlockSizeX	= GPixelFormats[Format].BlockSizeX;
	INT BlockSizeY	= GPixelFormats[Format].BlockSizeY;
	INT BlockBytes	= GPixelFormats[Format].BlockBytes;
	INT NumBlocksX	= (SizeX + BlockSizeX - 1) / BlockSizeX;
	INT NumBlocksY	= (SizeX + BlockSizeY - 1) / BlockSizeY;
	INT NumBytes	= NumBlocksX * NumBlocksY * BlockBytes * 6;

	if (GExclusiveResourceSizeMode)
	{
		return NumBytes;
	}

	FArchiveCountMem CountBytesSize( this );
	return CountBytesSize.GetNum() + NumBytes;
}

/**
 * Create a new 2D render target texture resource
 * @return newly created FTextureRenderTarget2DResource
 */
FTextureResource* UTextureRenderTargetCube::CreateResource()
{
	FTextureRenderTargetCubeResource* Result = new FTextureRenderTargetCubeResource(this);
	return Result;
}

/**
 * Materials should treat a render target 2D texture like a regular 2D texture resource.
 * @return EMaterialValueType for this resource
 */
EMaterialValueType UTextureRenderTargetCube::GetMaterialType()
{
	return MCT_TextureCube;
}

/** 
 * Called when any property in this object is modified in UnrealEd
 * @param	PropertyThatChanged - changed property
 */
void UTextureRenderTargetCube::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const INT MaxSize=2048;
	SizeX = Clamp<INT>(SizeX - (SizeX % GPixelFormats[Format].BlockSizeX),1,MaxSize);

#if CONSOLE
	// clamp the render target size in order to avoid reallocating the scene render targets
	// (note, PEC shouldn't really be called on consoles)
	SizeX = Min<INT>(SizeX,Min<INT>(GScreenWidth,GScreenHeight));
#endif

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/** 
 * Called after the object has been loaded
 */
void UTextureRenderTargetCube::PostLoad()
{
	Super::PostLoad();

	// override for platforms that don't support PF_G8 as render target
	if (GIsGame &&
		!GSupportsRenderTargetFormat_PF_G8 &&
		Format == PF_G8)
	{
		Format = PF_A8R8G8B8;					
	}

#if CONSOLE
	// clamp the render target size in order to avoid reallocating the scene render targets
	SizeX = Min<INT>(SizeX,Min<INT>(GScreenWidth,GScreenHeight));
#endif
}

/** 
 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
 */
FString UTextureRenderTargetCube::GetDesc()
{
	return FString::Printf( TEXT("Render to Texture Cube %dx%d[%s]"), SizeX, SizeX, GPixelFormats[Format].Name);
}

/** 
 * Returns detailed info to populate listview columns
 */
FString UTextureRenderTargetCube::GetDetailedDescription( INT InIndex )
{
	FString Description = TEXT( "" );
	switch( InIndex )
	{
	case 0:
		Description = FString::Printf( TEXT( "%dx%d" ), SizeX, SizeX );
		break;
	case 1:
		Description = GPixelFormats[Format].Name;
		break;
	}
	return( Description );
}

/**
 * Convert from ECubeFace to text string
 * @param Face - ECubeFace type to convert
 * @return text string for cube face enum value
 */
static const TCHAR* GetCubeFaceName( ECubeFace Face )
{
	switch( Face )
	{
	case CubeFace_PosX:
		return TEXT("PosX");
	case CubeFace_NegX:
		return TEXT("NegX");
	case CubeFace_PosY:
		return TEXT("PosY");
	case CubeFace_NegY:
		return TEXT("NegY");
	case CubeFace_PosZ:
		return TEXT("PosZ");
	case CubeFace_NegZ:
		return TEXT("NegZ");
	}
	return TEXT("");
}

/**
 *	Utility for creating a new UTextureCube from a TextureRenderTargetCube.
 *	TextureRenderTargetCube must be square and a power of two size.
 *	@param	ObjOuter		Outer to use when constructing the new TextureCube.
 *	@param	NewTexName		Name of new UTextureCube object.
 *	@param	Flags			Various control flags for operation (see EObjectFlags)
 *	@param	bSaveSourceArt	If true, source art will be saved.
 *	@return					New UTextureCube object.
 */
UTextureCube* UTextureRenderTargetCube::ConstructTextureCube(
	UObject* ObjOuter, 
	const FString& NewTexName, 
	EObjectFlags InFlags,
	UBOOL bSaveSourceArt
	)
{
	UTextureCube* Result = NULL;
#if !CONSOLE
	// Check render target size is valid and power of two.
	if( SizeX != 0 && !(SizeX & (SizeX-1)) )
	{
		// The r2t resource will be needed to read its surface contents
		FRenderTarget* RenderTarget = GameThread_GetRenderTargetResource();
		if( RenderTarget &&
			Format == PF_A8R8G8B8)
		{
			// create the cube texture
			Result = CastChecked<UTextureCube>( 
				StaticConstructObject(UTextureCube::StaticClass(), ObjOuter, FName(*NewTexName), InFlags) );

			UBOOL bSRGB=TRUE;
			// if render target gamma used was 1.0 then disable SRGB for the static texture
			if( Abs(RenderTarget->GetDisplayGamma() - 1.0f) < KINDA_SMALL_NUMBER )
			{
				bSRGB = FALSE;
			}

			// create a 2d texture for each cube face and copy the contents of the r2t cube texture to them
			for( INT FaceIdx=0; FaceIdx < CubeFace_MAX; FaceIdx++ )
			{
				FString Tex2DName( NewTexName + FString(GetCubeFaceName((ECubeFace)FaceIdx)) );
				// create a new 2d texture for the current cube face
				UTexture2D* Tex2D = CastChecked<UTexture2D>( 
					StaticConstructObject(UTexture2D::StaticClass(), ObjOuter, FName(*Tex2DName), InFlags) );
				// init to the same size as the cube texture face
				Tex2D->Init(SizeX,SizeX,PF_A8R8G8B8);

				// read current cube face surface 
				TArray<FColor> SurfData;
				// Assumes PF_A8R8G8B8 only format
				RenderTarget->ReadPixels(SurfData,FReadSurfaceDataFlags(RCM_UNorm, (ECubeFace)FaceIdx));

				// copy the cube face surface data to the 2D texture for this face
				FTexture2DMipMap& Mip = Tex2D->Mips(0);
				DWORD* TextureData = (DWORD*)Mip.Data.Lock(LOCK_READ_WRITE);
				INT TextureDataSize = Mip.Data.GetBulkDataSize();
				check(TextureDataSize==SurfData.Num()*sizeof(FColor));
				appMemcpy(TextureData,&SurfData(0),TextureDataSize);
				Mip.Data.Unlock();

				if( bSaveSourceArt )
				{
					Tex2D->SetUncompressedSourceArt( TextureData, TextureDataSize );
				}

				// Compress source art.
				Tex2D->CompressSourceArt();

				// Set compression options.
				Tex2D->CompressionSettings	= TC_Default;
				Tex2D->MipGenSettings		= TMGS_NoMipmaps;
				Tex2D->CompressionNoAlpha	= TRUE;
				Tex2D->DeferCompression		= FALSE;
				Tex2D->SRGB					= bSRGB;

				// This will trigger compressions.
				Tex2D->PostEditChange();

				// set this face of the cube texture
				switch( FaceIdx )
				{
				case CubeFace_PosX:
					Result->FacePosX = Tex2D; 
					break;
				case CubeFace_NegX:
					Result->FaceNegX = Tex2D; 
					break;
				case CubeFace_PosY:
					Result->FacePosY = Tex2D;
					break;
				case CubeFace_NegY:
					Result->FaceNegY = Tex2D;
					break;
				case CubeFace_PosZ:
					Result->FacePosZ = Tex2D;
					break;
				case CubeFace_NegZ:
					Result->FaceNegZ = Tex2D;
					break;
				}
			}
			Result->SRGB = bSRGB;
			Result->PostEditChange();
		}
	}	
#endif
	return Result;
}

/*-----------------------------------------------------------------------------
	FTextureRenderTargetCubeResource
-----------------------------------------------------------------------------*/

/**
 * We can only render to one face as a time. So, set the current 
 * face which will be used as the render target surface.
 * @param Face - face to use as current target face
 */
void FTextureRenderTargetCubeResource::SetCurrentTargetFace(ECubeFace Face)
{
	RenderTargetSurfaceRHI = CubeFaceSurfacesRHI[Face];
	CurrentTargetFace = Face;
}

/**
 * Initializes the RHI render target resources used by this resource.
 * Called when the resource is initialized, or when reseting all RHI resources.
 * This is only called by the rendering thread.
 */
void FTextureRenderTargetCubeResource::InitDynamicRHI()
{
	if( Owner->SizeX > 0 )
	{
		UBOOL bSRGB=TRUE;
		// if render target gamma used was 1.0 then disable SRGB for the static texture
		if( Abs(GetDisplayGamma() - 1.0f) < KINDA_SMALL_NUMBER )
		{
			bSRGB = FALSE;
		}

		// Create the RHI texture. Only one mip is used and the texture is targetable for resolve.
		DWORD TexCreateFlags = bSRGB ? TexCreate_SRGB : 0;
		TextureCubeRHI = RHICreateTextureCube(
			Owner->SizeX,
			Owner->Format, 
			1, 
			TexCreate_ResolveTargetable | TexCreateFlags | (Owner->bRenderOnce ? TexCreate_WriteOnce : 0),
			NULL
			);
		TextureRHI = (FTextureRHIRef&)TextureCubeRHI;

		// Create the RHI target surfaces used for rendering to. One for each cube face.
		for( INT FaceIdx=CubeFace_PosX; FaceIdx < CubeFace_MAX; FaceIdx++ )
		{
			CubeFaceSurfacesRHI[FaceIdx] = RHICreateTargetableCubeSurface(
				Owner->SizeX,
				Owner->Format,
				TextureCubeRHI,
				(ECubeFace)FaceIdx,
				(Owner->bNeedsTwoCopies ? TargetSurfCreate_Dedicated : 0) |
				(Owner->bRenderOnce ? TargetSurfCreate_WriteOnce : 0),
				TEXT("AuxColor")
				);
		}
		// default to positive X face
		SetCurrentTargetFace(CubeFace_PosX);

		// make sure the texture target gets cleared when possible after init
		if(Owner->bUpdateImmediate)
		{
			UpdateResource();
		}
		else
		{
			AddToDeferredUpdateList(TRUE);
		}
	}

	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer
	(
		GSystemSettings.TextureLODSettings.GetSamplerFilter( Owner ),
		AM_Clamp,
		AM_Clamp,
		AM_Clamp
	);
	SamplerStateRHI = RHICreateSamplerState( SamplerStateInitializer );
}

/**
 * Release the RHI render target resources used by this resource.
 * Called when the resource is released, or when reseting all RHI resources.
 * This is only called by the rendering thread.
 */
void FTextureRenderTargetCubeResource::ReleaseDynamicRHI()
{
	// release the FTexture RHI resources here as well
	ReleaseRHI();

	for( INT FaceIdx=CubeFace_PosX; FaceIdx < CubeFace_MAX; FaceIdx++ )
	{
		CubeFaceSurfacesRHI[FaceIdx].SafeRelease();
	}
	TextureCubeRHI.SafeRelease();
	RenderTargetSurfaceRHI.SafeRelease();	

	// remove grom global list of deferred clears
	RemoveFromDeferredUpdateList();
}

/**
 * Clear contents of the render target. Clears each face of the cube
 */
void FTextureRenderTargetCubeResource::UpdateResource()
{	
	const FLinearColor ClearColor(0.f,1.f,0.f,1.f);
	for( INT FaceIdx=CubeFace_PosX; FaceIdx < CubeFace_MAX; FaceIdx++ )
	{
		// clear each face of the cube target texture to green
		RHISetRenderTarget(CubeFaceSurfacesRHI[FaceIdx],FSurfaceRHIRef());
		RHISetViewport(0,0,0.0f,Owner->SizeX,Owner->SizeX,1.0f);	
		RHIClear(TRUE,ClearColor,FALSE,0.f,FALSE,0);
		// copy surface to the texture for use
		FResolveParams ResolveParams;
		ResolveParams.CubeFace = (ECubeFace)FaceIdx;
		RHICopyToResolveTarget(CubeFaceSurfacesRHI[FaceIdx], TRUE, ResolveParams);	
	}
}

/** 
 * @return width of target surface
 */
UINT FTextureRenderTargetCubeResource::GetSizeX() const
{ 
	return Owner->SizeX; 
}

/** 
 * @return height of target surface
 */
UINT FTextureRenderTargetCubeResource::GetSizeY() const
{ 
	return Owner->SizeX; 
}
