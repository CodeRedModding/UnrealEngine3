/*=============================================================================
	UnClient.cpp: UClient implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "EngineAudioDeviceClasses.h"

IMPLEMENT_CLASS(UClient);

UBOOL FViewport::bIsGameRenderingEnabled = TRUE;
INT FViewport::PresentAndStopMovieDelay = 0;


/**
 * Helper class for FViewport::TiledScreenshot().
 *
 * This class creates a BMP file and allows the user to copy pieces of
 * FColor bitmaps directly into the file, without ever having to store
 * the entire bitmap in memory.
 */
class FBitmapFile
{
public:
	FBitmapFile( );
	~FBitmapFile( );

	/**
	 * Creates the BMP file and writes the header and leaves it open.
	 *
	 * @param	FilenamePattern	Prefix for the file name
	 * @param	Width			Width in pixels
	 * @param	Height			Height in pixels
	 * 
	 * @return	TRUE if the file was successfully created, FALSE otherwise
	 */
	UBOOL		Create( const TCHAR* FilenamePattern, INT Width, INT Height );

	/**
	 * Copies an FColor image directly into the BMP file.
	 *
	 * @param	Source		Pointer to the source image
	 * @param	SrcWidth	Width of the source, in pixels
	 * @param	SrcHeight	Height of the source, in pixels
	 * @param	DstX		Upper-left x-coordinate of where to store the source image
	 * @param	DstY		Upper-left y-coordinate of where to store the source image
	 * @param	SrcRect		Optional sub-rectangle to constrain the source image, may be NULL
	 */
	void		CopyRect( FColor* Source, INT SrcWidth, INT SrcHeight, INT DstX=0, INT DstY=0, const FIntRect* SrcRect=NULL );

	/**
	 * Closes the BMP file.
	 *
	 * This function is automatically called by the destructor.
	 */
	void		Close( );

protected:
	FArchive*	Ar;
	INT			Width;
	INT			Height;
	INT			BytesPerLine;
	INT			FileOffset;
	static INT	BitmapIndex;
};

INT	FBitmapFile::BitmapIndex = -1;

FBitmapFile::FBitmapFile()
{
	appMemzero( this, sizeof(FBitmapFile) );
}

FBitmapFile::~FBitmapFile()
{
	Close();
}

/**
 * Creates the BMP file and writes the header and leaves it open.
 *
 * @param	FilenamePattern	Prefix for the file name
 * @param	Width			Width in pixels
 * @param	Height			Height in pixels
 * 
 * @return	TRUE if the file was successfully created, FALSE otherwise
 */
UBOOL FBitmapFile::Create( const TCHAR* FilenamePattern, INT Width, INT Height )
{
#if ALLOW_DEBUG_FILES

	TCHAR File[MAX_SPRINTF]=TEXT("");
	if( BitmapIndex == -1 )
	{
		for( INT TestBitmapIndex=0; TestBitmapIndex<65536; TestBitmapIndex++ )
		{
			appSprintf( File, TEXT("%s%05i.bmp"), FilenamePattern, TestBitmapIndex );
			if( GFileManager->FileSize(File) < 0 )
			{
				BitmapIndex = TestBitmapIndex;
				break;
			}
		}
	}

	appSprintf( File, TEXT("%s%05i.bmp"), FilenamePattern, BitmapIndex++ );

	if( GFileManager->FileSize(File)<0 )
	{
		Ar = GFileManager->CreateDebugFileWriter( File );
		if( Ar )
		{
			// Types.
#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,1)
#endif
			struct BITMAPFILEHEADER
			{
				WORD	bfType GCC_PACK(1);
				DWORD	bfSize GCC_PACK(1);
				WORD	bfReserved1 GCC_PACK(1); 
				WORD	bfReserved2 GCC_PACK(1);
				DWORD	bfOffBits GCC_PACK(1);
			} FH; 
			struct BITMAPINFOHEADER
			{
				DWORD	biSize GCC_PACK(1); 
				INT		biWidth GCC_PACK(1);
				INT		biHeight GCC_PACK(1);
				WORD	biPlanes GCC_PACK(1);
				WORD	biBitCount GCC_PACK(1);
				DWORD	biCompression GCC_PACK(1);
				DWORD	biSizeImage GCC_PACK(1);
				INT		biXPelsPerMeter GCC_PACK(1); 
				INT		biYPelsPerMeter GCC_PACK(1);
				DWORD	biClrUsed GCC_PACK(1);
				DWORD	biClrImportant GCC_PACK(1); 
			} IH;
#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

			BytesPerLine = Align(Width * 3,4);
			this->Width = Width;
			this->Height = Height;

			// File header.
			FH.bfType       		= INTEL_ORDER16((WORD) ('B' + 256*'M'));
			FH.bfSize       		= INTEL_ORDER32((DWORD) (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + BytesPerLine * Height));
			FH.bfReserved1  		= INTEL_ORDER16((WORD) 0);
			FH.bfReserved2  		= INTEL_ORDER16((WORD) 0);
			FH.bfOffBits    		= INTEL_ORDER32((DWORD) (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)));
			Ar->Serialize( &FH, sizeof(FH) );

			// Info header.
			IH.biSize               = INTEL_ORDER32((DWORD) sizeof(BITMAPINFOHEADER));
			IH.biWidth              = INTEL_ORDER32((DWORD) Width);
			IH.biHeight             = INTEL_ORDER32((DWORD) Height);
			IH.biPlanes             = INTEL_ORDER16((WORD) 1);
			IH.biBitCount           = INTEL_ORDER16((WORD) 24);
			IH.biCompression        = INTEL_ORDER32((DWORD) 0); //BI_RGB
			IH.biSizeImage          = INTEL_ORDER32((DWORD) BytesPerLine * Height);
			IH.biXPelsPerMeter      = INTEL_ORDER32((DWORD) 0);
			IH.biYPelsPerMeter      = INTEL_ORDER32((DWORD) 0);
			IH.biClrUsed            = INTEL_ORDER32((DWORD) 0);
			IH.biClrImportant       = INTEL_ORDER32((DWORD) 0);
			Ar->Serialize( &IH, sizeof(IH) );

			FileOffset = Ar->Tell();
		}
		else 
			return FALSE;
	}
	else 
		return FALSE;

#endif

	// Success.
	return TRUE;
}

/**
 * Copies an FColor image directly into the BMP file.
 *
 * @param	Source		Pointer to the source image
 * @param	SrcWidth	Width of the source, in pixels
 * @param	SrcHeight	Height of the source, in pixels
 * @param	DstX		Upper-left x-coordinate of where to store the source image
 * @param	DstY		Upper-left y-coordinate of where to store the source image
 * @param	SrcRect		Optional sub-rectangle to constrain the source image, may be NULL
 */
void FBitmapFile::CopyRect( FColor* Source, INT SrcWidth, INT SrcHeight, INT DstX/*=0*/, INT DstY/*=0*/, const FIntRect* SrcRect/*=NULL*/ )
{
#if ALLOW_DEBUG_FILES
	// In pixels:
	BYTE PadBytes[4] = { 0, 0, 0, 0 };
	INT Padding		= 0;
	INT SrcOffset	= SrcRect ? (SrcRect->Min.Y * SrcWidth + SrcRect->Min.X) : 0;
	INT NumRows		= SrcRect ? SrcRect->Height() : SrcHeight;
	INT NumColumns	= SrcRect ? SrcRect->Width() : SrcWidth;
	Source			+= SrcOffset;

	// Clamp to bottom
	if ( NumRows >= (Height - DstY) )
		NumRows		= Height - DstY;

	// Clamp to right edge
	if ( NumColumns >= (Width - DstX) )
	{
		NumColumns	= Width - DstX;
		Padding		= BytesPerLine - Width*3;	// Pad line to 4 byte alignment
	}

	// In Bytes:
	INT DstOffset	= FileOffset + (Height - DstY - NumRows) * BytesPerLine + DstX * 3;

	for ( INT Y=0; Y < NumRows; ++Y )
	{
		FColor *Src = Source + (NumRows - Y - 1) * SrcWidth;
		Ar->Seek( DstOffset + Y*BytesPerLine );
		for ( INT X=0; X < NumColumns; ++X )
		{
			Ar->Serialize( &Src[X].B, 1 );
			Ar->Serialize( &Src[X].G, 1 );
			Ar->Serialize( &Src[X].R, 1 );
		}
		if ( Padding )
		{
			Ar->Serialize( PadBytes, Padding );
		}
	}
#endif
}

/**
 * Closes the BMP file.
 *
 * This function is automatically called by the destructor.
 */
void FBitmapFile::Close()
{
	if ( Ar )
		delete Ar;
	appMemzero( this, sizeof(FBitmapFile) );
}

/**
* Reads the viewport's displayed pixels into a preallocated color buffer.
* @param OutImageData - RGBA8 values will be stored in this buffer
* @param TopLeftX - Top left X pixel to capture
* @param TopLeftY - Top left Y pixel to capture
* @param Width - Width of image in pixels to capture
* @param Height - Height of image in pixels to capture
* @return True if the read succeeded.
*/
UBOOL FRenderTarget::ReadPixels(TArray< BYTE >& OutImageData,FReadSurfaceDataFlags InFlags, UINT TopLeftX, UINT TopLeftY, UINT Width, UINT Height )
{
	// Read the render target surface data back.	
	struct FReadSurfaceContext
	{
		FRenderTarget* SrcRenderTarget;
		TArray<BYTE>* OutData;
		UINT MinX;
		UINT MinY;
		UINT MaxX;
		UINT MaxY;
		FReadSurfaceDataFlags Flags;
	};

	OutImageData.Reset();
	FReadSurfaceContext ReadSurfaceContext =
	{
		this,
		&OutImageData,
		TopLeftX,
		TopLeftY,
		TopLeftX + Width - 1,
		TopLeftY + Height - 1,
		InFlags
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReadSurfaceCommand,
		FReadSurfaceContext,Context,ReadSurfaceContext,
	{
		RHIReadSurfaceData(
			Context.SrcRenderTarget->RenderTargetSurfaceRHI,
			Context.MinX,
			Context.MinY,
			Context.MaxX,
			Context.MaxY,
			*Context.OutData,
			Context.Flags
			);
	});
	FlushRenderingCommands();

	return TRUE;
}


/**
* Reads the viewport's displayed pixels into a preallocated color buffer.
* @param OutputBuffer - RGBA8 values will be stored in this buffer
* @return True if the read succeeded.
*/
UBOOL FRenderTarget::ReadPixels(BYTE* OutImageBytes, FReadSurfaceDataFlags InFlags)
{
	TArray<BYTE> SurfaceData;
	SurfaceData.Add( GetSizeX() * GetSizeY() * sizeof( FColor ) );
	
	UBOOL bResult = ReadPixels( SurfaceData, InFlags, 0, 0, GetSizeX(), GetSizeY() );
	if( bResult )
	{
		appMemcpy( OutImageBytes, &SurfaceData( 0 ), SurfaceData.Num() );
	}

	return bResult;
}



/**
* Reads the viewport's displayed pixels into the given color buffer.
* @param OutputBuffer - RGBA8 values will be stored in this buffer
* @return True if the read succeeded.
*/
UBOOL FRenderTarget::ReadPixels(TArray<FColor>& OutputBuffer,FReadSurfaceDataFlags InFlags)
{
	// Copy the surface data into the output array.
	OutputBuffer.Empty();
	OutputBuffer.Add( GetSizeX() * GetSizeY() );
	return ReadPixels( ( BYTE* )&OutputBuffer( 0 ), InFlags );
}

/**
 * Reads the viewport's displayed pixels into a preallocated color buffer.
 * @param OutImageBytes - RGBA16F values will be stored in this buffer.  Buffer must be preallocated with the correct size!
 * @param CubeFace - optional cube face for when reading from a cube render target
 * @return True if the read succeeded.
 */
UBOOL FRenderTarget::ReadFloat16Pixels(FFloat16Color* OutImageData,ECubeFace CubeFace)
{
	// Read the render target surface data back.	
	struct FReadSurfaceFloatContext
	{
		FRenderTarget* SrcRenderTarget;
		TArray<FFloat16Color>* OutData;
		UINT MinX;
		UINT MinY;
		UINT MaxX;
		UINT MaxY;
		ECubeFace CubeFace;
	};
	
	UINT MaxX = GetSizeX() - 1;
	UINT MaxY = GetSizeY() - 1;

	TArray<FFloat16Color> SurfaceData;
	FReadSurfaceFloatContext ReadSurfaceFloatContext =
	{
		this,
		&SurfaceData,
		0, 0,
		MaxX, MaxY,
		CubeFace	
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReadSurfaceFloatCommand,
		FReadSurfaceFloatContext,Context,ReadSurfaceFloatContext,
	{
		RHIReadSurfaceFloatData(
			Context.SrcRenderTarget->RenderTargetSurfaceRHI,
			Context.MinX,
			Context.MinY,
			Context.MaxX,
			Context.MaxY,
			*Context.OutData,
			Context.CubeFace
			);
	});
	FlushRenderingCommands();

	// Copy the surface data into the output array.
	FFloat16Color* OutImageColors = reinterpret_cast< FFloat16Color* >(OutImageData);

	// Cache width and height as its very expensive to call these virtuals in inner loop (never inlined)
	const INT ImageWidth = GetSizeX();
	const INT ImageHeight = GetSizeY();
	for (INT Y = 0; Y < ImageHeight; Y++)
	{
		FFloat16Color* SourceData = ((FFloat16Color*)&SurfaceData(0)) + Y * ImageWidth;
		for (INT X = 0; X < ImageWidth; X++)
		{
			OutImageColors[ Y * ImageWidth + X ] = SourceData[X];
		}
	}

	return TRUE;
}

/**
 * Reads the viewport's displayed pixels into the given color buffer.
 * @param OutputBuffer - RGBA16F values will be stored in this buffer
 * @param CubeFace - optional cube face for when reading from a cube render target
 * @return True if the read succeeded.
 */
UBOOL FRenderTarget::ReadFloat16Pixels(TArray<FFloat16Color>& OutputBuffer,ECubeFace CubeFace)
{
	// Copy the surface data into the output array.
	OutputBuffer.Empty();
	OutputBuffer.Add(GetSizeX() * GetSizeY());
	return ReadFloat16Pixels((FFloat16Color*)&(OutputBuffer(0)), CubeFace);
}

/** 
* @return display gamma expected for rendering to this render target 
*/
FLOAT FRenderTarget::GetDisplayGamma() const
{
	if (GEngine == NULL || GEngine->Client == NULL)
	{
		return 2.2f;
	}
	else
	{
		if (Abs(GEngine->Client->DisplayGamma) <= 0.0f)
		{
			debugf(NAME_Error, TEXT("Invalid DisplayGamma! Resetting to the default of 2.2"));
			GEngine->Client->DisplayGamma = 2.2f;
		}
		return GEngine->Client->DisplayGamma;
	}
}

/**
* Accessor for the surface RHI when setting this render target
* @return render target surface RHI resource
*/
const FSurfaceRHIRef& FRenderTarget::GetRenderTargetSurface() const
{
	return RenderTargetSurfaceRHI;
}

/*=============================================================================
//
// FViewport implementation.
//
=============================================================================*/

FViewport::FViewport(FViewportClient* InViewportClient):
	ViewportClient(InViewportClient),
	SizeX(0),
	SizeY(0),
	bIsFullscreen(FALSE),
	bHitProxiesCached(FALSE),
	bHasRequestedToggleFreeze(FALSE),
	bShouldClearMotionBlurInfo(FALSE)
{
	//initialize the hit proxy kernel
	HitProxySize = 5;
	if (GIsEditor) 
	{
		GConfig->GetInt( TEXT("UnrealEd.HitProxy"), TEXT("HitProxySize"), (INT&)HitProxySize, GEditorIni );
		Clamp( HitProxySize, (UINT)1, (UINT)MAX_HITPROXYSIZE );
	}

	// Cache the viewport client's hit proxy storage requirement.
	bRequiresHitProxyStorage = ViewportClient && ViewportClient->RequiresHitProxyStorage();
#if CONSOLE && !FINAL_RELEASE
	if ( bRequiresHitProxyStorage )
	{
		warnf( TEXT("Consoles don't need hitproxy storage - wasting memory!?") );
	}
#endif

	AppVersionString = FString::Printf( TEXT( "Version: %d (%d)" ), GEngineVersion, GBuiltFromChangeList );

	bIsPlayInEditorViewport = FALSE;
}

extern UBOOL	GIsTiledScreenshot;
extern INT		GScreenshotTile;
extern INT		GScreenshotMargin;
extern INT		GScreenshotResolutionMultiplier;
extern FIntRect	GScreenshotRect;

/**
 * Calculate the changes necessary to the view in order to facilitate tiled screenshot drawing 
 *
 * @View - the view to take the screenshot from
 */
void FViewport::CalculateTiledScreenshotSettings(FSceneView* View)
{
	// Calculate number of overlapping tiles:
	INT TileWidth	= GetSizeX();
	INT TileHeight	= GetSizeY();
	INT TotalWidth	= GScreenshotResolutionMultiplier * TileWidth;
	INT TotalHeight	= GScreenshotResolutionMultiplier * TileHeight;
	INT NumColumns	= appCeil( FLOAT(TotalWidth) / FLOAT(TileWidth - 2*GScreenshotMargin) );
	INT NumRows		= appCeil( FLOAT(TotalHeight) / FLOAT(TileHeight - 2*GScreenshotMargin) );
	TileWidth		= appTrunc(View->SizeX);
	TileHeight		= appTrunc(View->SizeY);
	FLOAT XMarginScale = TileWidth / (FLOAT) GetSizeX();
	FLOAT YMarginScale = TileHeight / (FLOAT) GetSizeY();
	TotalWidth		= GScreenshotResolutionMultiplier * TileWidth;
	TotalHeight		= GScreenshotResolutionMultiplier * TileHeight;

	// Report back to UD3DRenderDevice::TiledScreenshot():
	GScreenshotRect.Min.X = appTrunc(View->X);
	GScreenshotRect.Min.Y = appTrunc(View->Y);
	GScreenshotRect.Max.X = appTrunc(View->X + View->SizeX);
	GScreenshotRect.Max.Y = appTrunc(View->Y + View->SizeY);

	// Calculate tile position (upper-left corner, screen space):
	INT TileRow		= GScreenshotTile / NumColumns;
	INT TileColumn	= GScreenshotTile % NumColumns;
	INT PosX		= TileColumn*TileWidth - (2*TileColumn + 1)*GScreenshotMargin*XMarginScale;
	INT PosY		= TileRow*TileHeight - (2*TileRow + 1)*GScreenshotMargin*YMarginScale;

	// Calculate offsets to center tile (screen space):
	INT OffsetX		= (TotalWidth - TileWidth) / 2 - PosX;
	INT OffsetY		= (TotalHeight - TileHeight) / 2 - PosY;

	// Convert to projection space:
	FLOAT Scale		= FLOAT(GScreenshotResolutionMultiplier);
	FLOAT OffsetXp	= 2.0f * FLOAT(OffsetX) / FLOAT(TotalWidth);
	FLOAT OffsetYp	= -2.0f * FLOAT(OffsetY) / FLOAT(TotalHeight);

	// Apply offsets and scales:
	FTranslationMatrix OffsetMtx( FVector( OffsetXp, OffsetYp, 0.0f) );
	FScaleMatrix ScaleMtx( FVector(Scale, Scale, 1.0f) );
	View->ProjectionMatrix = View->ProjectionMatrix * OffsetMtx * ScaleMtx;
	View->InvProjectionMatrix = View->ProjectionMatrix.Inverse();
	View->ViewProjectionMatrix = View->ViewMatrix * View->ProjectionMatrix;
	View->InvViewProjectionMatrix = View->ViewProjectionMatrix.Inverse();
	View->TranslatedViewMatrix = FTranslationMatrix(-View->PreViewTranslation) * View->ViewMatrix;
	View->TranslatedViewProjectionMatrix = View->TranslatedViewMatrix * View->ProjectionMatrix;
	View->InvTranslatedViewProjectionMatrix = View->TranslatedViewProjectionMatrix.Inverse();
	//RI->SetOrigin2D( -PosX, -PosY );
	//RI->SetZoom2D( Scale );
}

/**
 * Take a tiled, high-resolution screenshot and save to disk.
 *
 * @ResolutionMultiplier Increase resolution in each dimension by this multiplier.
 */
void FViewport::TiledScreenshot( INT ResolutionMultiplier )
{
	FViewportClient* ViewportClient = GetClient();
	GScreenshotResolutionMultiplier = ResolutionMultiplier;

	// Calculate number of overlapping tiles:
	INT TileWidth	= (INT) GetSizeX();
	INT TileHeight	= (INT) GetSizeY();
	INT InnerWidth	= TileWidth - 2*GScreenshotMargin;
	INT InnerHeight	= TileHeight - 2*GScreenshotMargin;
	INT TotalWidth	= GScreenshotResolutionMultiplier * TileWidth;
	INT TotalHeight	= GScreenshotResolutionMultiplier * TileHeight;
	INT NumColumns	= appCeil( FLOAT(TotalWidth) / FLOAT(InnerWidth) );
	INT NumRows		= appCeil( FLOAT(TotalHeight) / FLOAT(InnerHeight) );
	INT NumTiles	= NumColumns * NumRows;
	UBOOL IsOk		= TRUE;

	// Create screenshot folder if not already present.
	GFileManager->MakeDirectory( *appScreenShotDir(), TRUE );

	TCHAR Filename[MAX_SPRINTF]=TEXT("");
	FBitmapFile BitmapFile;
	appSprintf( Filename, ( GIsDumpingTileShotMovie ? TEXT( "%sHighres_MovieFrame" ) : TEXT( "%sHighres_Screenshot_") ), *appScreenShotDir() );

	for ( GScreenshotTile = 0; IsOk && GScreenshotTile < NumTiles; ++GScreenshotTile )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			BeginDrawingCommand,
			FViewport*,Viewport,this,
		{
			Viewport->BeginRenderFrame();
		});

		FCanvas Canvas(this,NULL);
		{
			ViewportClient->Draw(this,&Canvas);
		}
		Canvas.Flush();

		// Read the contents of the viewport into an temp array.
		TArray<FColor> TempBitmap;
		IsOk = ReadPixels(TempBitmap);

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			EndDrawingCommand,
			FViewport*,Viewport,this,
		{
			Viewport->EndRenderFrame( FALSE, FALSE );
		});

		// If everything went fine, save the rendered tile.
		if ( IsOk )
		{
			// Copy tile into final bitmap file:
			check(TempBitmap.Num() == TileWidth * TileHeight);
			INT ShotWidth	= GScreenshotRect.Width();
			INT ShotHeight	= GScreenshotRect.Height();
			FLOAT XMarginScale = (FLOAT)ShotWidth / (FLOAT)GetSizeX();
			FLOAT YMarginScale = (FLOAT)ShotHeight / (FLOAT)GetSizeY();
			GScreenshotRect.Min += FIntPoint((INT)((FLOAT)GScreenshotMargin * XMarginScale),(INT)((FLOAT)GScreenshotMargin * YMarginScale));
			GScreenshotRect.Max -= FIntPoint((INT)((FLOAT)GScreenshotMargin * XMarginScale),(INT)((FLOAT)GScreenshotMargin * YMarginScale));
			InnerWidth		= GScreenshotRect.Width();
			InnerHeight		= GScreenshotRect.Height();
			INT TileRow		= GScreenshotTile / NumColumns;
			INT TileColumn	= GScreenshotTile % NumColumns;
			INT DstX		= TileColumn * InnerWidth;
			INT DstY		= TileRow * InnerHeight;

			if ( GScreenshotTile == 0 )
			{
				TotalWidth		= GScreenshotResolutionMultiplier * ShotWidth;
				TotalHeight		= GScreenshotResolutionMultiplier * ShotHeight;
				if ( !BitmapFile.Create( Filename, TotalWidth, TotalHeight ) )
				{
					GIsTiledScreenshot = FALSE;
					GAreScreenMessagesEnabled = GScreenMessagesRestoreState;
					return;
				}
			}
//			FIntRect SrcRect( GScreenshotMargin, GScreenshotMargin, GScreenshotMargin + InnerWidth, GScreenshotMargin + InnerHeight );
//			BitmapFile.CopyRect( &TempBitmap(0), TileWidth, TileHeight, DstX, DstY, &SrcRect );
			BitmapFile.CopyRect( &TempBitmap(0), TileWidth, TileHeight, DstX, DstY, &GScreenshotRect );
		}
	}

	BitmapFile.Close();
	// only reset this if we are not dumping a tileshot movie
	if( GIsDumpingTileShotMovie == FALSE )
	{
		GIsTiledScreenshot = FALSE;
		GAreScreenMessagesEnabled = GScreenMessagesRestoreState;
	}
}

/**
 * A bare-bones viewport interface used to generate high-resolution screenshots. See FViewport::HighResScreenshot()
 */
class FDummyViewport : public FViewport
{
public:
	// Constructor.
	FDummyViewport(FViewportClient* InViewportClient, UINT InSizeX, UINT InSizeY)
		: FViewport(InViewportClient)
	{
		SizeX = InSizeX;
		SizeY = InSizeY;
		
		// Create the RHI viewport.
		UpdateViewportRHI(FALSE, InSizeX, InSizeY, FALSE);
	}

	// Destructor.
	virtual ~FDummyViewport()
	{
		// Destroy the RHI viewport.
		UpdateViewportRHI(TRUE, 0, 0, FALSE);
	}

	virtual void InitDynamicRHI()
	{
		//Create the resolve target and surface for this viewport.
		DWORD RenderTargetFlags = TargetSurfCreate_None;
		RenderTargetTexture   = RHICreateTexture2D(SizeX,SizeY,PF_A8R8G8B8, 1, TexCreate_ResolveTargetable, NULL);
		RenderTargetSurfaceRHI = RHICreateTargetableSurface(SizeX, SizeY, PF_A8R8G8B8, RenderTargetTexture, RenderTargetFlags, TEXT("DummyViewportColor")); 
	}

	virtual void ReleaseDynamicRHI()
	{
		RenderTargetSurfaceRHI.SafeRelease();
		RenderTargetTexture.SafeRelease();
	}

	virtual void*	GetWindow() { return 0; }
	virtual UBOOL	CaptureJoystickInput(UBOOL Capture) { return FALSE; }
	virtual UBOOL	KeyState(FName Key) const { return FALSE; }
	virtual INT		GetMouseX() { return 0; }
	virtual INT		GetMouseY() { return 0; }
	virtual void	GetMousePos( FIntPoint& MousePosition ) { MousePosition = FIntPoint(0, 0); }
	virtual void	SetMouse(INT x, INT y) { }
	virtual void	ProcessInput( FLOAT DeltaTime ) { }
	virtual void InvalidateDisplay() { }
	virtual void DeferInvalidateHitProxy() { }
	virtual FViewportFrame* GetViewportFrame() { return 0; }

private:
	/** The resolve target for the viewport surface. */
	FTexture2DRHIRef RenderTargetTexture;

	/** The width of the viewport. */
	UINT SizeX;

	/** The height of the viewport. */
	UINT SizeY;
};

/**
 * Take a high-resolution screenshot and save to disk.
 */
void FViewport::HighResScreenshot()
{
	//Create a dummy viewport with a render target at the resolution we require for the screenshot.
	UINT ViewportSizeX = SizeX * GScreenshotResolutionMultiplier;
	UINT ViewportSizeY = SizeY * GScreenshotResolutionMultiplier;

	FDummyViewport* DummyViewport = new FDummyViewport(ViewportClient, ViewportSizeX, ViewportSizeY);
	BeginInitResource(DummyViewport);

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		BeginDrawingCommand,
		FViewport*,Viewport, DummyViewport,
	{
		Viewport->BeginRenderFrame();
	});

	FCanvas Canvas(DummyViewport,NULL);
	{
		ViewportClient->Draw(DummyViewport,&Canvas);
	}	
	Canvas.Flush();

	FIntPoint RestoreSize(SizeX, SizeY);

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		EndDrawingCommand,
		FViewport*,Viewport,DummyViewport,
		FIntPoint,InRestoreSize,RestoreSize,
	{
		Viewport->EndRenderFrame( FALSE, FALSE );
		
		//Restore the scene's render targets to original size
		GSceneRenderTargets.SetBufferSize(InRestoreSize.X, InRestoreSize.Y);
		GSceneRenderTargets.UpdateRHI();
	});

	//Clean up the dummy viewport.
	BeginReleaseResource(DummyViewport);
	FlushRenderingCommands();
	delete DummyViewport;

	//Once the screenshot is done we disable the feature to get only one frame
	GIsHighResScreenshot = FALSE;
}

/**
 * Request to clear the MB info. Game thread only
 *
 * @param bShouldClear - if TRUE then the clear occurs at the end of the current frame
 */
void FViewport::SetClearMotionBlurInfoGameThread(UBOOL bShouldClear)
{
	check(IsInGameThread());

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
	    ShouldClearMBInfoCommand,
	    FViewport*,Viewport,this,
		UBOOL,bShouldClear,bShouldClear,
    {
		Viewport->bShouldClearMotionBlurInfo = bShouldClear;
    });
}

struct FEndDrawingCommandParams
{
	FViewport* Viewport;
	BITFIELD bLockToVsync : 1;
	BITFIELD bShouldTriggerTimerEvent : 1;
	BITFIELD bShouldPresent : 1;
};

/**
 * Helper function used in ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER below. Needed to be split out due to
 * use of macro and former already being one.
 *
 * @param Parameters	Parameters passed from the gamethread to the renderthread command.
 */
static void ViewportEndDrawing( FEndDrawingCommandParams Parameters )
{
    // Calculate renderthread time (excluding idle time).

    DWORD StartTime		= appCycles();
	DWORD IdleStartTime	= GRenderThreadIdle - StartTime;
	GInputLatencyTimer.RenderThreadTrigger = Parameters.bShouldTriggerTimerEvent;
	Parameters.Viewport->EndRenderFrame( Parameters.bShouldPresent, Parameters.bLockToVsync );
    DWORD EndTime		= appCycles();

	GSwapBufferTime		= EndTime - StartTime;
	SET_CYCLE_COUNTER(STAT_PresentTime, GSwapBufferTime, 1);

	static DWORD LastTimestamp	= 0;
	DWORD ThreadTime	= EndTime - LastTimestamp;
    LastTimestamp		= EndTime;

#if USE_XeD3D_RHI
	// We keep track of time spent waiting on GPU on Xbox 360. Time is tracked in ms so we need to convert it back
	// into cycles. VSYNC time is already part of CPU wait time so we don't (double) count it here.
	extern FLOAT GCPUWaitingOnGPUTimeLastFrame;
	GRenderThreadIdle	+= appTrunc( GCPUWaitingOnGPUTimeLastFrame / 1000.f / GSecondsPerCycle );
#else
	// Inner functions may have added to GRenderThreadIdle. We disregard their changes since we're counting it from here instead.
	GRenderThreadIdle	= IdleStartTime + appCycles();
#endif
    GRenderThreadTime	= (ThreadTime > GRenderThreadIdle) ? (ThreadTime - GRenderThreadIdle) : ThreadTime;
    GRenderThreadIdle	= 0;
}

static const FName GSceneUpdateName( TEXT("SceneUpdate"), FNAME_Add );

/** Starts a new rendering frame. Called from the rendering thread. */
void FViewport::BeginRenderFrame()
{
	check( IsInRenderingThread() );

	appStopCPUTrace( GSceneUpdateName );

	RHIBeginDrawingViewport( GetViewportRHI() );
	UpdateRenderTargetSurfaceRHIToCurrentBackBuffer( );

	// Notify GSceneRenderTargets of our current backbuffer.
	GSceneRenderTargets.SetBackBuffer( RHIGetViewportBackBuffer( GetViewportRHI() ), RHIGetViewportDepthBuffer( GetViewportRHI() ) );
}

/**
 *	Ends a rendering frame. Called from the rendering thread.
 *	@param bPresent		Whether the frame should be presented to the screen
 *	@param bLockToVsync	Whether the GPU should block until VSYNC before presenting
 */
void FViewport::EndRenderFrame( UBOOL bPresent, UBOOL bLockToVsync )
{
	check( IsInRenderingThread() );

	extern UBOOL GIsCurrentlyPrecaching;
	RHIEndDrawingViewport( GetViewportRHI(), GIsCurrentlyPrecaching ? FALSE : bPresent, bLockToVsync );

	//grab the new transform out of the proxies for next frame
	FScene::UpdateMotionBlurCache();

	if (bPresent)
	{
		if (bShouldClearMotionBlurInfo || !GSystemSettings.bAllowMotionBlurPause)
		{
			// Clear the motion blur information for this frame.		
			FScene::ClearMotionBlurInfo();
			bShouldClearMotionBlurInfo = FALSE;
			// Clear motion blur info entries that have not been updated
			FScene::ClearStaleMotionBlurInfo();
		}
	}

	// Notify GSceneRenderTargets of our current backbuffer.
	GSceneRenderTargets.SetBackBuffer( RHIGetViewportBackBuffer( GetViewportRHI() ), RHIGetViewportDepthBuffer( GetViewportRHI() ) );

	appStartCPUTrace( GSceneUpdateName, TRUE, FALSE, 40, NULL );
}


void FViewport::Draw( UBOOL bShouldPresent /*= TRUE */)
{
	// Ignore reentrant draw calls, since we can only redraw one viewport at a time.
	static UBOOL bReentrant = FALSE;
	if(!bReentrant)
	{
	    // if this is a game viewport, and game rendering is disabled, then we don't want to actually draw anything
	    if (GIsGame && !bIsGameRenderingEnabled)
	    {
		    // since we aren't drawing the viewport, we still need to update streaming, which needs valid view info
		    FSceneViewFamilyContext ViewFamily(this, GWorld->Scene, SHOW_DefaultGame, GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds());
		    for(INT PlayerIndex = 0;PlayerIndex < GEngine->GamePlayers.Num();PlayerIndex++)
		    {
			    ULocalPlayer* Player = GEngine->GamePlayers(PlayerIndex);
			    if(Player->Actor)
			    {
				    // Calculate the player's view information.
				    FVector		ViewLocation;
				    FRotator	ViewRotation;
				    FSceneView* View = Player->CalcSceneView( &ViewFamily, ViewLocation, ViewRotation, this);
    
				    // if we have a valid view, use it for resource streaming
				    if(View)
				    {
					    // Add view information for resource streaming.
					    GStreamingManager->AddViewInformation( View->ViewOrigin, View->SizeX, View->SizeX * View->ProjectionMatrix.M[0][0] );
    
					    // Add scene captures - if their streaming is not disabled (SceneCaptureStreamingMultiplier == 0.0f)
					    if (View->Family && View->Family->Scene && (GSystemSettings.SceneCaptureStreamingMultiplier > 0.0f))
					    {
						    View->Family->Scene->AddSceneCaptureViewInformation(GStreamingManager, View);
					    }
				    }
			    }
		    }
    
		    // Update level streaming.
		    GWorld->UpdateLevelStreaming( &ViewFamily );
	    }
	    else
	    {
		    // Tiled rendering for high-res screenshots.
		    if( ( GIsTiledScreenshot || GIsDumpingTileShotMovie ) && IsValidRef(ViewportRHI) )
		    {
			    TiledScreenshot( GScreenshotResolutionMultiplier );
		    }

		    if(GIsHighResScreenshot)
		    {
			    HighResScreenshot();
		    }

		    if(IsValidRef(ViewportRHI))
		    {
			    UBOOL bLockToVsync = FALSE;
			    if ( GEngine->GamePlayers.Num() )
			    {
				    ULocalPlayer* Player = GEngine->GamePlayers(0);
				    bLockToVsync = (Player && Player->Actor && Player->Actor->bCinematicMode);
			    }
    
			    ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				    BeginDrawingCommand,
				    FViewport*,Viewport,this,
			    {
					Viewport->BeginRenderFrame();
			    });
    
			    FCanvas Canvas(this,NULL);
			    {
				    ViewportClient->Draw(this,&Canvas);
			    }
			    Canvas.Flush();
    
			    // Calculate gamethread time (excluding idle time)
			    {
				    static DWORD Lastimestamp = 0;
				    DWORD CurrentTime	= appCycles();
				    DWORD ThreadTime	= CurrentTime - Lastimestamp;
				    Lastimestamp		= CurrentTime;
				    GGameThreadTime		= (ThreadTime > GGameThreadIdle) ? (ThreadTime - GGameThreadIdle) : ThreadTime;
				    GGameThreadIdle		= 0;
			    }

				FEndDrawingCommandParams Params = { this, bLockToVsync, GInputLatencyTimer.GameThreadTrigger, PresentAndStopMovieDelay > 0 ? 0 : bShouldPresent };
				ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				    EndDrawingCommand,
					FEndDrawingCommandParams,Parameters,Params,
			    {
					ViewportEndDrawing( Parameters );
			    });

				if ( IsInRenderingThread() )
				{
					GGameThreadIdle += GSwapBufferTime;
				}

			    GInputLatencyTimer.GameThreadTrigger = FALSE;
		    }
	    }

		// Reset the camera cut flags
		for(INT PlayerIndex = 0;PlayerIndex < GEngine->GamePlayers.Num();PlayerIndex++)
		{
			ULocalPlayer* Player = GEngine->GamePlayers(PlayerIndex);
			if ( Player->Actor )
			{
				Player->Actor->bCameraCut = FALSE;
			}
		}

		// countdown the present delay, and then stop the movie at the end
		// this doesn't need to be on rendering thread as long as we have a long enough delay (2 or 3 frames), because
		// the rendering thread will never be more than one frame behind
		if (PresentAndStopMovieDelay > 0)
		{
			PresentAndStopMovieDelay--;
			// stop any playing movie
			if (PresentAndStopMovieDelay == 0)
			{
				GFullScreenMovie->GameThreadStopMovie(0.0f, FALSE, TRUE);
				// Enable game rendering again if it isn't already.
				bIsGameRenderingEnabled = TRUE;
			}
		}
	}
}


void FViewport::InvalidateHitProxy()
{
	bHitProxiesCached = FALSE;
	HitProxyMap.Invalidate();
}



void FViewport::Invalidate()
{
	InvalidateHitProxy();
	InvalidateDisplay();
}



void FViewport::GetHitProxyMap(UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<HHitProxy*>& OutMap)
{
	// If the hit proxy map isn't up to date, render the viewport client's hit proxies to it.
	if(!bHitProxiesCached)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			BeginDrawingCommand,
			FViewport*,Viewport,this,
		{
			Viewport->BeginRenderFrame();

			// Set the hit proxy map's render target.
			RHISetRenderTarget(Viewport->HitProxyMap.GetRenderTargetSurface(), FSurfaceRHIRef());

			// Clear the hit proxy map to white, which is overloaded to mean no hit proxy.
			RHIClear(TRUE,FLinearColor::White,FALSE,0,FALSE,0);
		});

		// Let the viewport client draw its hit proxies.
		FCanvas Canvas(&HitProxyMap,&HitProxyMap);
		{
			ViewportClient->Draw(this,&Canvas);
		}
		Canvas.Flush();

		//Resolve surface to texture.
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				UpdateHitProxyRTCommand,
				FHitProxyMap*, HitProxyMap, &HitProxyMap,
			{
				// Copy (resolve) the rendered thumbnail from the render target to its texture
				RHICopyToResolveTarget(HitProxyMap->GetRenderTargetSurface(), FALSE, FResolveParams() );
			});

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			EndDrawingCommand,
			FViewport*,Viewport,this,
		{
			Viewport->EndRenderFrame( FALSE, FALSE );
		});

		// Cache the hit proxies for the next GetHitProxyMap call.
		bHitProxiesCached = TRUE;
	}

	// Read the hit proxy map surface data back.
	struct FReadSurfaceContext
	{
		FViewport* Viewport;
		TArray<BYTE>* OutData;
		UINT MinX;
		UINT MinY;
		UINT MaxX;
		UINT MaxY;
	};
	TArray<BYTE> SurfaceData;
	FReadSurfaceContext ReadSurfaceContext =
	{
		this,
		&SurfaceData,
		MinX, MinY,
		MaxX, MaxY
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReadSurfaceCommand,
		FReadSurfaceContext,Context,ReadSurfaceContext,
		{
			RHIReadSurfaceData(
				*const_cast<FSurfaceRHIRef*>(&Context.Viewport->HitProxyMap.GetRenderTargetSurface()),
				Context.MinX,
				Context.MinY,
				Context.MaxX,
				Context.MaxY,
				*Context.OutData,
				FReadSurfaceDataFlags()
				);
		});
	FlushRenderingCommands();

	// Map the hit proxy map surface data to hit proxies.
	OutMap.Empty((MaxY - MinY + 1) * (MaxX - MinX + 1));
	for(UINT Y = MinY;Y <= MaxY;Y++)
	{
		FColor* SourceData = ((FColor*)&SurfaceData(0)) + (Y - MinY) * (MaxX - MinX + 1);
		for(UINT X = MinX;X <= MaxX;X++)
		{
			FHitProxyId HitProxyId(SourceData[X - MinX]);
			OutMap.AddItem(GetHitProxyById(HitProxyId));
		}
	}
}

HHitProxy* FViewport::GetHitProxy(INT X,INT Y)
{
	// Compute a HitProxySize x HitProxySize test region with the center at (X,Y).
	INT		MinX = X - HitProxySize,
			MinY = Y - HitProxySize,
			MaxX = X + HitProxySize,
			MaxY = Y + HitProxySize;
	
	// Clip the region to the viewport bounds.
	MinX = Max(MinX,0);
	MinY = Max(MinY,0);
	MaxX = Min(MaxX,appTrunc(GetSizeX()) - 1);
	MaxY = Min(MaxY,appTrunc(GetSizeY()) - 1);

	INT			TestSizeX	= MaxX - MinX + 1,
				TestSizeY	= MaxY - MinY + 1;
	HHitProxy*	HitProxy	= NULL;

	if(TestSizeX > 0 && TestSizeY > 0)
	{
		// Read the hit proxy map from the device.
		TArray<HHitProxy*>	ProxyMap;
		GetHitProxyMap((UINT)MinX,(UINT)MinY,(UINT)MaxX,(UINT)MaxY,ProxyMap);
		check(ProxyMap.Num() == TestSizeX * TestSizeY);

		// Find the hit proxy in the test region with the highest order.
		INT ProxyIndex = TestSizeY/2 * TestSizeX + TestSizeX/2;
		check(ProxyIndex<ProxyMap.Num());
		HitProxy = ProxyMap(ProxyIndex);
	
		UBOOL bIsOrtho = GetClient()->IsOrtho();

		for(INT TestY = 0;TestY < TestSizeY;TestY++)
		{
			for(INT TestX = 0;TestX < TestSizeX;TestX++)
			{
				HHitProxy* TestProxy = ProxyMap(TestY * TestSizeX + TestX);
				if(TestProxy && (!HitProxy || (bIsOrtho ? TestProxy->OrthoPriority : TestProxy->Priority) > (bIsOrtho ? HitProxy->OrthoPriority : HitProxy->Priority)))
				{
					HitProxy = TestProxy;
				}
			}
		}
	}

	return HitProxy;
}

void FViewport::UpdateViewportRHI(UBOOL bDestroyed,UINT NewSizeX,UINT NewSizeY,UBOOL bNewIsFullscreen)
{
	// Make sure we're not in the middle of streaming textures.
	(*GFlushStreamingFunc)();

	{
		// Temporarily stop rendering thread.
#if !WIIU
		// @todo wiiu hack: This isn't needed on WiiU, and causes problems with starting/stopping the rendering thread
		SCOPED_SUSPEND_RENDERING_THREAD(FSuspendRenderingThread::ST_RecreateThread);
#endif

		// Update the viewport attributes.
		// This is done AFTER the command flush done by UpdateViewportRHI, to avoid disrupting rendering thread accesses to the old viewport size.
		SizeX = NewSizeX;
		SizeY = NewSizeY;
		bIsFullscreen = bNewIsFullscreen;

		// Release the viewport's resources.
		BeginReleaseResource(this);
		GSceneRenderTargets.SetBackBuffer(NULL,NULL);

		GCallbackEvent->Send(CALLBACK_PreViewportResized, this, 0);
		
		// Don't reinitialize the viewport RHI if the viewport has been destroyed.
		if(bDestroyed)
		{
			if(IsValidRef(ViewportRHI))
			{
				// If the viewport RHI has already been initialized, release it.
				ViewportRHI.SafeRelease();
			}
		}
		else
		{
			if(IsValidRef(ViewportRHI))
			{
				// If the viewport RHI has already been initialized, resize it.
				RHIResizeViewport(
					ViewportRHI,
					SizeX,
					SizeY,
					bIsFullscreen
					);
			}
			else
			{
				// Initialize the viewport RHI with the new viewport state.
				ViewportRHI = RHICreateViewport(
					GetWindow(),
					SizeX,
					SizeY,
					bIsFullscreen
					);
			}

 			// Initialize the viewport's resources.
 			BeginInitResource(this);
		}
	}

#if WITH_ES2_RHI && MOBILESHADER_THREADED_INIT
	if ( GUsingES2RHI && !bDestroyed )
	{
		// Initialize and compile ES2 shaders at startup (will only do this once, ever).
		GMobileShaderInitialization.StartCompilingShaderGroup(TEXT("StartupPackages"), TRUE); // Global & Fallback Shaders
		GMobileShaderInitialization.StartCompilingShaderGroup(TEXT("Ungrouped"), FALSE); // Ungrouped Shaders/Generated at runtime
	}
#endif

	if ( !bDestroyed && GCallbackEvent != NULL )
	{
		// send a notification that the viewport has been resized
		GCallbackEvent->Send(CALLBACK_ViewportResized, this, 0);
	}
}

/**
 * Calculates the view inside the viewport when the aspect ratio is locked.
 * Used for creating cinematic bars.
 * @param Aspect [in] ratio to lock to
 * @param CurrentX [in][out] coordinates of aspect locked view
 * @param CurrentY [in][out]
 * @param CurrentSizeX [in][out] size of aspect locked view
 * @param CurrentSizeY [in][out]
 */
void FViewport::CalculateViewExtents( FLOAT AspectRatio, INT& CurrentX, INT& CurrentY, UINT& CurrentSizeX, UINT& CurrentSizeY )
{
	// the viewport's SizeX/SizeY may not always match the GetDesiredAspectRatio(), so adjust the requested AspectRatio to compensate
	FLOAT AdjustedAspectRatio = AspectRatio / (GetDesiredAspectRatio() / ((FLOAT)GetSizeX() / (FLOAT)GetSizeY()));

	// If desired, enforce a particular aspect ratio for the render of the scene. 
	// Results in black bars at top/bottom etc.
	FLOAT AspectRatioDifference = AdjustedAspectRatio - ( ( FLOAT ) CurrentSizeX ) / ( ( FLOAT )CurrentSizeY );

	if( ::Abs( AspectRatioDifference ) > 0.01f )
	{
		// If desired aspect ratio is bigger than current - we need black bars on top and bottom.
		if( AspectRatioDifference > 0.0f )
		{
			// Calculate desired Y size.
			INT NewSizeY = appRound( ( ( FLOAT )CurrentSizeX ) / AdjustedAspectRatio );
			CurrentY = appRound( 0.5f * ( ( FLOAT )( CurrentSizeY - NewSizeY ) ) );
			CurrentSizeY = NewSizeY;
		}
		// Otherwise - will place bars on the sides.
		else
		{
			INT NewSizeX = appRound( ( ( FLOAT )CurrentSizeY ) * AdjustedAspectRatio );
			CurrentX = appRound( 0.5f * ( ( FLOAT )( CurrentSizeX - NewSizeX ) ) );
			CurrentSizeX = NewSizeX;
		}
	}
}

/**
 *	Sets a viewport client if one wasn't provided at construction time.
 *	@param InViewportClient	- The viewport client to set.
 **/
void FViewport::SetViewportClient( FViewportClient* InViewportClient )
{
	ViewportClient = InViewportClient;
}

void FViewport::InitDynamicRHI()
{
	// Capture the viewport's back buffer surface for use through the FRenderTarget interface.
	UpdateRenderTargetSurfaceRHIToCurrentBackBuffer();

	if(bRequiresHitProxyStorage)
	{
		// Initialize the hit proxy map.
		HitProxyMap.Init(SizeX,SizeY);
	}
}

void FViewport::ReleaseDynamicRHI()
{
	HitProxyMap.Release();
	RenderTargetSurfaceRHI.SafeRelease();
}

UBOOL IsCtrlDown(FViewport* Viewport) { return (Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl)); }
UBOOL IsShiftDown(FViewport* Viewport) { return (Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift)); }
UBOOL IsAltDown(FViewport* Viewport) { return (Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt)); }


/** Constructor */
FViewport::FHitProxyMap::FHitProxyMap()
{
	GCallbackEvent->Register( CALLBACK_CleanseEditor, this );
}


/** Destructor */
FViewport::FHitProxyMap::~FHitProxyMap()
{
	GCallbackEvent->UnregisterAll( this );
}


void FViewport::FHitProxyMap::Init(UINT NewSizeX,UINT NewSizeY)
{
	SizeX = NewSizeX;
	SizeY = NewSizeY;

	// Create a render target to store the hit proxy map.
	HitProxyTexture = RHICreateTexture2D(SizeX,SizeY,PF_A8R8G8B8, 1, TexCreate_ResolveTargetable, NULL);

	INT RenderTargetFlags = 0;//TargetSurfCreate_Dedicated|TargetSurfCreate_Readable;
	RenderTargetSurfaceRHI = RHICreateTargetableSurface(SizeX,SizeY,PF_A8R8G8B8,HitProxyTexture, RenderTargetFlags, TEXT("HitProxyColor"));
}

void FViewport::FHitProxyMap::Release()
{
	HitProxyTexture.SafeRelease();
	RenderTargetSurfaceRHI.SafeRelease();
}

void FViewport::FHitProxyMap::Invalidate()
{
	HitProxies.Empty();
}

void FViewport::FHitProxyMap::AddHitProxy(HHitProxy* HitProxy)
{
	HitProxies.AddItem(HitProxy);
}


/** FSerializableObject: Serialize this object */
void FViewport::FHitProxyMap::Serialize( FArchive& Ar )
{
	// Allow all of our hit proxy objects to serialize their references
	for( INT CurProxyIndex = 0; CurProxyIndex < HitProxies.Num(); ++CurProxyIndex )
	{
		HHitProxy* CurProxy = HitProxies( CurProxyIndex );
		if( CurProxy != NULL )
		{
			CurProxy->Serialize( Ar );
		}
	}
}


/** FCallbackEventDevice: Called when a registered global event is fired */
void FViewport::FHitProxyMap::Send( ECallbackEventType InType )
{
	if( InType == CALLBACK_CleanseEditor )
	{
		// Clear our hit proxies as they may retain references to objects that need to be cleaned up
		Invalidate();
	}
}



/**
 * Globally enables/disables rendering
 *
 * @param bIsEnabled TRUE if drawing should occur
 * @param PresentAndStopMovieDelay Number of frames to delay before enabling bPresent in RHIEndDrawingViewport, and before stopping the movie
 */
void FViewport::SetGameRenderingEnabled(UBOOL bIsEnabled, INT InPresentAndStopMovieDelay)
{
	bIsGameRenderingEnabled = bIsEnabled;
	PresentAndStopMovieDelay = Max(PresentAndStopMovieDelay, InPresentAndStopMovieDelay);
}

/**
 * Handles freezing/unfreezing of rendering
 */
void FViewport::ProcessToggleFreezeCommand()
{
	bHasRequestedToggleFreeze = 1;
}

/**
 * Returns if there is a command to toggle freezing
 */
UBOOL FViewport::HasToggleFreezeCommand()
{
	// save the current command
	UBOOL ReturnVal = bHasRequestedToggleFreeze;
	
	// make sure that we no longer have the command, as we are now passing off "ownership"
	// of the command
	bHasRequestedToggleFreeze = FALSE;

	// return what it was
	return ReturnVal;
}

/**
 * Update the render target surface RHI to the current back buffer 
 */
void FViewport::UpdateRenderTargetSurfaceRHIToCurrentBackBuffer()
{
	RenderTargetSurfaceRHI = RHIGetViewportBackBuffer(ViewportRHI);
}


//
// UClient implementation.
//

void UClient::StaticConstructor()
{
	new(GetClass(),TEXT("DisplayGamma"),	RF_Public)UFloatProperty(CPP_PROPERTY(DisplayGamma	), TEXT("Display"), CPF_Config );
	new(GetClass(),TEXT("MinDesiredFrameRate"),	RF_Public)UFloatProperty(CPP_PROPERTY(MinDesiredFrameRate	), TEXT("Display"), CPF_Config );
	new(GetClass(),TEXT("InitialButtonRepeatDelay"), RF_Public) UFloatProperty(CPP_PROPERTY(InitialButtonRepeatDelay), TEXT("Input"), CPF_Config);
	new(GetClass(),TEXT("ButtonRepeatDelay"), RF_Public) UFloatProperty(CPP_PROPERTY(ButtonRepeatDelay), TEXT("Input"), CPF_Config);
}

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UClient::InitializeIntrinsicPropertyValues()
{
	InitialButtonRepeatDelay = 0.2f;
	ButtonRepeatDelay = 0.1f;
}
/**
 * Exec handler used to parse console commands.
 *
 * @param	Cmd		Command to parse
 * @param	Ar		Output device to use in case the handler prints anything
 * @return	TRUE if command was handled, FALSE otherwise
 */
UBOOL UClient::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	if (ParseCommand(&Cmd, TEXT("GAMMA")))
	{
		DisplayGamma = (*Cmd != 0) ? Clamp<FLOAT>(appAtof(*ParseToken(Cmd, FALSE)), 0.5f, 5.0f) : GetClass()->GetDefaultObject<UClient>()->DisplayGamma;
		return TRUE;
	}
	else if( GetAudioDevice() && GetAudioDevice()->Exec(Cmd,Ar) )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


//
// FInputLatencyTimer implementation.
//

/** Potentially starts the timer on the gamethread, based on the UpdateFrequency. */
void FInputLatencyTimer::GameThreadTick()
{
#if STATS
	SET_CYCLE_COUNTER( STAT_InputLatencyTime, DeltaTime, 1 );

	// Only trigger measurements if the stat is being displayed.
	FStatGroup* MemGroup = GStatManager.GetGroup(STATGROUP_Engine);
	if (MemGroup->bShowGroup == TRUE)
	{
		if ( !bInitialized )
		{
			LastCaptureTime	= appSeconds();
			bInitialized	= TRUE;
		}
		FLOAT CurrentTimeInSeconds = appSeconds();
		if ( (CurrentTimeInSeconds - LastCaptureTime) > UpdateFrequency )
		{
			LastCaptureTime		= CurrentTimeInSeconds;
			StartTime			= appCycles();
			GameThreadTrigger	= TRUE;
		}
	}
#endif
}

FInputLatencyTimer GInputLatencyTimer( 2.0f );

