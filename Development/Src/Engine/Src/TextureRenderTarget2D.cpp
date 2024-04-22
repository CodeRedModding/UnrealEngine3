/*=============================================================================
	TextureRenderTarget2D.cpp: UTextureRenderTarget2D implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	UTextureRenderTarget2D
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UTextureRenderTarget2D);

/**
 * Create a new 2D render target texture resource
 * @return newly created FTextureRenderTarget2DResource
 */
FTextureResource* UTextureRenderTarget2D::CreateResource()
{
	FTextureRenderTarget2DResource* Result = new FTextureRenderTarget2DResource(this);
	return Result;
}

/**
 * Materials should treat a render target 2D texture like a regular 2D texture resource.
 * @return EMaterialValueType for this resource
 */
EMaterialValueType UTextureRenderTarget2D::GetMaterialType()
{
	return MCT_Texture2D;
}

/**
 * Serialize properties (used for backwards compatibility with main branch)
 */
void UTextureRenderTarget2D::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT UTextureRenderTarget2D::GetResourceSize()
{
	// Calculate size based on format.
	INT BlockSizeX	= GPixelFormats[Format].BlockSizeX;
	INT BlockSizeY	= GPixelFormats[Format].BlockSizeY;
	INT BlockBytes	= GPixelFormats[Format].BlockBytes;
	INT NumBlocksX	= (SizeX + BlockSizeX - 1) / BlockSizeX;
	INT NumBlocksY	= (SizeY + BlockSizeY - 1) / BlockSizeY;
	INT NumBytes	= NumBlocksX * NumBlocksY * BlockBytes;

	if (GExclusiveResourceSizeMode)
	{
		return NumBytes;
	}

	FArchiveCountMem CountBytesSize( this );
	return CountBytesSize.GetNum() + NumBytes;
}

/** 
 * Initialize the settings needed to create a render target texture and create its resource
 * @param	InSizeX - width of the texture
 * @param	InSizeY - height of the texture
 * @param	InFormat - format of the texture
 */
void UTextureRenderTarget2D::Init( UINT InSizeX, UINT InSizeY, EPixelFormat InFormat, UBOOL bInForceLinearGamma )
{
	check(InSizeX > 0 && InSizeY > 0);
	check(!(InSizeX % GPixelFormats[InFormat].BlockSizeX));
	check(!(InSizeY % GPixelFormats[InFormat].BlockSizeY));
	check(FTextureRenderTargetResource::IsSupportedFormat(InFormat));

	// set required size/format
	SizeX		= InSizeX;
	SizeY		= InSizeY;
	Format		= InFormat;

	// override for platforms that don't support PF_G8 as render target
	if (GIsGame &&
		!GSupportsRenderTargetFormat_PF_G8 &&
		Format == PF_G8)
	{
		Format = PF_A8R8G8B8;					
	}

	bForceLinearGamma = bInForceLinearGamma;

	// Recreate the texture's resource.
	UpdateResource();
}

/** script accessible function to create and initialize a new TextureRenderTarget2D with the requested settings */
void UTextureRenderTarget2D::execCreate(FFrame& Stack, RESULT_DECL)
{
	P_GET_INT(InSizeX);
	P_GET_INT(InSizeY);
	P_GET_BYTE_OPTX(InFormat, PF_A8R8G8B8);
	/** Initial optional parameter is the default color value of the TextureRenderTarget2D */
	P_GET_STRUCT_OPTX(FLinearColor, InClearColor, GetClass()->GetDefaultObject<UTextureRenderTarget2D>()->ClearColor);
	P_GET_UBOOL_OPTX(bOnlyRenderOnce,FALSE);
	P_FINISH;

	EPixelFormat DesiredFormat = EPixelFormat(InFormat);
	if (InSizeX > 0 && InSizeY > 0 && FTextureRenderTargetResource::IsSupportedFormat(DesiredFormat))
	{
		UTextureRenderTarget2D* NewTexture = Cast<UTextureRenderTarget2D>(StaticConstructObject(GetClass(), GetTransientPackage(), NAME_None, RF_Transient));
		if (NewTexture != NULL)
		{
			NewTexture->ClearColor = InClearColor;
			NewTexture->bRenderOnce = bOnlyRenderOnce;
			NewTexture->Init(InSizeX, InSizeY, DesiredFormat);
		}
		*(UTextureRenderTarget2D**)Result = NewTexture;
	}
	else
	{
		debugf(NAME_Warning, TEXT("Invalid parameters specified for TextureRenderTarget2D::Create()"));
		*(UTextureRenderTarget2D**)Result = NULL;
	}
}

/** 
 * Called when any property in this object is modified in UnrealEd
 * @param	PropertyThatChanged - changed property
 */
void UTextureRenderTarget2D::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const INT MaxSize=2048; 
	SizeX = Clamp<INT>(SizeX - (SizeX % GPixelFormats[Format].BlockSizeX),1,MaxSize);
	SizeY = Clamp<INT>(SizeY - (SizeY % GPixelFormats[Format].BlockSizeY),1,MaxSize);

#if CONSOLE
	// clamp the render target size in order to avoid reallocating the scene render targets
	// (note, PEC shouldn't really be called on consoles)
	SizeX = Min<INT>(SizeX,GScreenWidth);
	SizeY = Min<INT>(SizeY,GScreenHeight);
#endif

    Super::PostEditChangeProperty(PropertyChangedEvent);
}

/** 
* Called after the object has been loaded
*/
void UTextureRenderTarget2D::PostLoad()
{
#if CONSOLE
	// Clamp the render target size in order to avoid reallocating the scene render targets,
	// before the FTextureRenderTarget2DResource() is created in Super::PostLoad().
	SizeX = Min<INT>(SizeX,GScreenWidth);
	SizeY = Min<INT>(SizeY,GScreenHeight);
#endif

	Super::PostLoad();

	// override for platforms that don't support PF_G8 as render target
	if (GIsGame &&
		!GSupportsRenderTargetFormat_PF_G8 &&
		Format == PF_G8)
	{
		Format = PF_A8R8G8B8;					
	}
}

/** 
 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
 */
FString UTextureRenderTarget2D::GetDesc()	
{
	// size and format string
	return FString::Printf( TEXT("Render to Texture %dx%d[%s]"), SizeX, SizeY, GPixelFormats[Format].Name );
}

/** 
 * Returns detailed info to populate listview columns
 */
FString UTextureRenderTarget2D::GetDetailedDescription( INT InIndex )
{
	FString Description = TEXT( "" );
	switch( InIndex )
	{
	case 0:
		Description = FString::Printf( TEXT( "%dx%d" ), SizeX, SizeY );
		break;
	case 1:
		Description = GPixelFormats[Format].Name;
		break;
	}
	return( Description );
}

/**
 * Utility for creating a new UTexture2D from a TextureRenderTarget2D
 * TextureRenderTarget2D must be square and a power of two size.
 * @param Outer - Outer to use when constructing the new Texture2D.
 * @param NewTexName - Name of new UTexture2D object.
 * @param ObjectFlags - Flags to apply to the new Texture2D object
 * @param Flags - Various control flags for operation (see EConstructTextureFlags)
 * @param AlphaOverride - If specified, the values here will become the alpha values in the resulting texture
 * @param bSaveSourceArt - If true, source art will be saved.
 * @return New UTexture2D object.
 */
UTexture2D* UTextureRenderTarget2D::ConstructTexture2D(UObject* Outer, const FString& NewTexName, EObjectFlags ObjectFlags, DWORD Flags, TArray<BYTE>* AlphaOverride, UBOOL bSaveSourceArt)
{
	UTexture2D* Result = NULL;
#if !CONSOLE
	// Check render target size is valid and power of two.
	if( SizeX != 0 && !(SizeX & (SizeX-1)) &&
		SizeY != 0 && !(SizeY & (SizeY-1)) )
	{
		// The r2t resource will be needed to read its surface contents
		FRenderTarget* RenderTarget = GameThread_GetRenderTargetResource();
		if( RenderTarget && 
			Format == PF_A8R8G8B8)
		{
			// create the 2d texture
			Result = CastChecked<UTexture2D>( 
				StaticConstructObject(UTexture2D::StaticClass(), Outer, FName(*NewTexName), ObjectFlags) );
			// init to the same size as the 2d texture
			Result->Init(SizeX,SizeY,PF_A8R8G8B8);

			// read the 2d surface 
			TArray<FColor> SurfData;
			// Assumes PF_A8R8G8B8 only format
			RenderTarget->ReadPixels(SurfData);

			// override the alpha if desired
			if (AlphaOverride)
			{
				check(SurfData.Num() == AlphaOverride->Num());
				for (INT Pixel = 0; Pixel < SurfData.Num(); Pixel++)
				{
					SurfData(Pixel).A = (*AlphaOverride)(Pixel);
				}
			}
			else if (Flags & CTF_RemapAlphaAsMasked)
			{
				// if the target was rendered with a masked texture, then the depth will probably have been written instead of 0/255 for the
				// alpha, and the depth when unwritten will be 255, so remap 255 to 0 (masked out area) and anything else as 255 (written to area)
				for (INT Pixel = 0; Pixel < SurfData.Num(); Pixel++)
				{
					SurfData(Pixel).A = (SurfData(Pixel).A == 255) ? 0 : 255;
				}
			}
			else if (Flags & CTF_ForceOpaque)
			{
				for (INT Pixel = 0; Pixel < SurfData.Num(); Pixel++)
				{
					SurfData(Pixel).A = 255;
				}
			}

			// copy the 2d surface data to the first mip of the static 2d texture
			FTexture2DMipMap& Mip = Result->Mips(0);
			DWORD* TextureData = (DWORD*)Mip.Data.Lock(LOCK_READ_WRITE);
			INT TextureDataSize = Mip.Data.GetBulkDataSize();
			check(TextureDataSize==SurfData.Num()*sizeof(FColor));
			appMemcpy(TextureData,&SurfData(0),TextureDataSize);
			Mip.Data.Unlock();
	
			if( bSaveSourceArt )
			{
				Result->SetUncompressedSourceArt( TextureData, TextureDataSize );
			}

			// if render target gamma used was 1.0 then disable SRGB for the static texture
			if( Abs(RenderTarget->GetDisplayGamma() - 1.0f) < KINDA_SMALL_NUMBER )
			{
				Flags &= ~CTF_SRGB;
			}

			Result->SRGB = (Flags & CTF_SRGB) ? TRUE : FALSE;
			Result->MipGenSettings = TMGS_FromTextureGroup;

			if((Flags & CTF_AllowMips) == 0)
			{
				Result->MipGenSettings = TMGS_NoMipmaps;
			}

			if (Flags & CTF_Compress)
			{
				// Set compression options.
				Result->CompressionSettings	= (Flags & CTF_ForceOneBitAlpha) ? TC_OneBitAlpha : TC_Default;
				Result->DeferCompression	= (Flags & CTF_DeferCompression) ? TRUE : FALSE;

				// This will trigger compressions.
				Result->PostEditChange();
			}
			else
			{
				// Disable compression
				Result->CompressionNone = TRUE;
				Result->CompressionSettings	= TC_Default;
				Result->DeferCompression	= FALSE;
				
				Result->UpdateResource();
			}

		}
	}	
#endif
	return Result;
}

/*-----------------------------------------------------------------------------
	FTextureRenderTarget2DResource
-----------------------------------------------------------------------------*/

FTextureRenderTarget2DResource::FTextureRenderTarget2DResource(const class UTextureRenderTarget2D* InOwner)
	:	Owner(InOwner)
	,	ClearColor(InOwner->ClearColor)
	,	TargetSizeX(Owner->SizeX)
	,	TargetSizeY(Owner->SizeY)
{
}

/**
 * Clamp size of the render target resource to max values
 *
 * @param MaxSizeX max allowed width
 * @param MaxSizeY max allowed height
 */
void FTextureRenderTarget2DResource::ClampSize(INT MaxSizeX,INT MaxSizeY)
{
	// upsize to go back to original or downsize to clamp to max
 	INT NewSizeX = Min<INT>(Owner->SizeX,MaxSizeX);
 	INT NewSizeY = Min<INT>(Owner->SizeY,MaxSizeY);
	if (NewSizeX != TargetSizeX || NewSizeY != TargetSizeY)
	{
		TargetSizeX = NewSizeX;
		TargetSizeY = NewSizeY;
		// reinit the resource with new TargetSizeX,TargetSizeY
		UpdateRHI();
	}	
}

/**
 * Initializes the RHI render target resources used by this resource.
 * Called when the resource is initialized, or when reseting all RHI resources.
 * This is only called by the rendering thread.
 */
void FTextureRenderTarget2DResource::InitDynamicRHI()
{
	if( TargetSizeX > 0 && TargetSizeY > 0 )
	{
		UBOOL bSRGB=TRUE;
		// if render target gamma used was 1.0 then disable SRGB for the static texture
		if( Abs(GetDisplayGamma() - 1.0f) < KINDA_SMALL_NUMBER )
		{
			bSRGB = FALSE;
		}

		// Create the RHI texture. Only one mip is used and the texture is targetable for resolve.
		DWORD TexCreateFlags = bSRGB ? TexCreate_SRGB : 0;
		Texture2DRHI = RHICreateTexture2D(
			TargetSizeX, 
			TargetSizeY, 
			Owner->Format, 
			1,
			TexCreate_ResolveTargetable | TexCreateFlags | (Owner->bRenderOnce ? TexCreate_WriteOnce : 0),
			NULL
			);
		TextureRHI = (FTextureRHIRef&)Texture2DRHI;

		// Create the RHI target surface used for rendering to
		RenderTargetSurfaceRHI = RHICreateTargetableSurface(
			TargetSizeX,
			TargetSizeY,
			Owner->Format,
			Texture2DRHI,
			(Owner->bNeedsTwoCopies ? TargetSurfCreate_Dedicated : 0) |
			(Owner->bRenderOnce ? TargetSurfCreate_WriteOnce : 0),
			TEXT("AuxColor")
			);

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
		Owner->AddressX == TA_Wrap ? AM_Wrap : (Owner->AddressX == TA_Clamp ? AM_Clamp : AM_Mirror),
		Owner->AddressY == TA_Wrap ? AM_Wrap : (Owner->AddressY == TA_Clamp ? AM_Clamp : AM_Mirror),
		AM_Wrap
	);
	SamplerStateRHI = RHICreateSamplerState( SamplerStateInitializer );
}

/**
 * Release the RHI render target resources used by this resource.
 * Called when the resource is released, or when reseting all RHI resources.
 * This is only called by the rendering thread.
 */
void FTextureRenderTarget2DResource::ReleaseDynamicRHI()
{
	// release the FTexture RHI resources here as well
	ReleaseRHI();

	Texture2DRHI.SafeRelease();
	RenderTargetSurfaceRHI.SafeRelease();	

	// remove grom global list of deferred clears
	RemoveFromDeferredUpdateList();
}

/**
 * Clear contents of the render target. 
 */
void FTextureRenderTarget2DResource::UpdateResource()
{	
 	// clear the target surface to green
 	RHISetRenderTarget(RenderTargetSurfaceRHI,FSurfaceRHIRef());
 	RHISetViewport(0,0,0.0f,TargetSizeX,TargetSizeY,1.0f);
 	RHIClear(TRUE,ClearColor,FALSE,0.f,FALSE,0);
 
 	// copy surface to the texture for use
 	RHICopyToResolveTarget(RenderTargetSurfaceRHI, TRUE, FResolveParams());
}

/** 
 * @return width of target surface
 */
UINT FTextureRenderTarget2DResource::GetSizeX() const
{ 
	return TargetSizeX; 
}

/** 
 * @return height of target surface
 */
UINT FTextureRenderTarget2DResource::GetSizeY() const
{ 
	return TargetSizeY; 
}

/** 
* Render target resource should be sampled in linear color space
*
* @return display gamma expected for rendering to this render target 
*/
FLOAT FTextureRenderTarget2DResource::GetDisplayGamma() const
{
    if (Owner->TargetGamma > KINDA_SMALL_NUMBER * 10.0f)
	{
        return Owner->TargetGamma;
	}
	if (Owner->Format == PF_FloatRGB || Owner->bForceLinearGamma )
	{
		return 1.0f;
	}
	return FTextureRenderTargetResource::GetDisplayGamma();
}

//
// ScriptedTexture implementation
//
IMPLEMENT_CLASS(UScriptedTexture);

TArray<UScriptedTexture*> UScriptedTexture::GScriptedTextures;

UScriptedTexture::UScriptedTexture()
{
	if (!IsTemplate())
	{
		GScriptedTextures.AddItem(this);
	}
}

void UScriptedTexture::BeginDestroy()
{
	GScriptedTextures.RemoveItem(this);

	Super::BeginDestroy();
}

void UScriptedTexture::UpdateResource()
{
	Super::UpdateResource();

	bNeedsUpdate = TRUE;
}

void UScriptedTexture::CheckUpdate()
{
	if (bNeedsUpdate)
	{
		// reset bNeedsUpdate first so that the Render() call can request a subsequent update by setting the flag TRUE again
		bNeedsUpdate = FALSE;

		// construct and initialize a Canvas if necessary
		UCanvas* CanvasObject = (UCanvas*)StaticFindObjectFast(UCanvas::StaticClass(), UObject::GetTransientPackage(), FName(TEXT("CanvasObject")));
		if (CanvasObject == NULL)
		{
			CanvasObject = ConstructObject<UCanvas>(UCanvas::StaticClass(), UObject::GetTransientPackage(), TEXT("CanvasObject"));
			CanvasObject->AddToRoot();
		}
		
		CanvasObject->Init();
		CanvasObject->SizeX = SizeX;
		CanvasObject->SizeY = SizeY;
		CanvasObject->Update();

		// skip the clear if we are told to
		if (bSkipNextClear)
		{
			// set the render target, but don't clear
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
							SkipClearTextureRTCommand,
							FTextureRenderTarget2DResource*, TextureRenderTarget, (FTextureRenderTarget2DResource*)GameThread_GetRenderTargetResource(),
						{
							RHISetRenderTarget(TextureRenderTarget->GetRenderTargetSurface(), FSurfaceRHIRef());
							RHISetViewport(0, 0, 0.0f, TextureRenderTarget->GetSizeX(), TextureRenderTarget->GetSizeY(), 1.0f);
						});

			// reset the skip clear flag each frame
			bSkipNextClear = FALSE;
		}
		else
		{
			// clear the texture
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
							ClearTextureRTCommand,
							FTextureRenderTarget2DResource*, TextureRenderTarget, (FTextureRenderTarget2DResource*)GameThread_GetRenderTargetResource(),
						{
							RHISetRenderTarget(TextureRenderTarget->GetRenderTargetSurface(), FSurfaceRHIRef());
							RHISetViewport(0, 0, 0.0f, TextureRenderTarget->GetSizeX(), TextureRenderTarget->GetSizeY(), 1.0f);
							RHIClear(TRUE, TextureRenderTarget->GetClearColor(), FALSE, 0.f, FALSE, 0);
						});
		}

		// render to the texture resource
		FCanvas InternalCanvas(GameThread_GetRenderTargetResource(), NULL);
		CanvasObject->Canvas = &InternalCanvas;
		Render(CanvasObject);
		CanvasObject->Canvas = NULL;
		InternalCanvas.Flush();
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
						ResolveCanvasRTCommand,
						FRenderTarget*, CanvasRenderTarget, GameThread_GetRenderTargetResource(),
					{
						RHICopyToResolveTarget(CanvasRenderTarget->GetRenderTargetSurface(), FALSE, FResolveParams());
					});
	}
}

void UScriptedTexture::Render(UCanvas* C)
{
	// only allow script to be called during gameplay
	if (GWorld != NULL && GWorld->HasBegunPlay())
	{
		delegateRender(C);
	}
}
