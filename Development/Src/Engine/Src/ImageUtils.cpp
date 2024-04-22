/*=============================================================================
ImageUtils.cpp: Image utility functions.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ImageUtils.h"

/**
 * Resizes the given image using a simple average filter and stores it in the destination array.
 *
 * @param SrcWidth		Source image width.
 * @param SrcHeight		Source image height.
 * @param SrcData		Source image data.
 * @param DstWidth		Destination image width.
 * @param DstHeight		Destination image height.
 * @param DstData		Destination image data.
 * @param bLinearSpace	If TRUE, convert colors into linear space before interpolating (slower but more accurate)
 */
void FImageUtils::ImageResize(INT SrcWidth, INT SrcHeight, const TArray<FColor> &SrcData, INT DstWidth, INT DstHeight, TArray<FColor> &DstData, UBOOL bLinearSpace )
{
	DstData.Empty();
	DstData.AddZeroed(DstWidth*DstHeight);

	FLOAT SrcX = 0;
	FLOAT SrcY = 0;

	const FLOAT StepSizeX = SrcWidth / (FLOAT)DstWidth;
	const FLOAT StepSizeY = SrcHeight / (FLOAT)DstHeight;

	for(INT Y=0; Y<DstHeight;Y++)
	{
		INT PixelPos = Y * DstWidth;
		SrcX = 0.0f;	
	
		for(INT X=0; X<DstWidth; X++)
		{
			INT PixelCount = 0;
			FLOAT EndX = SrcX + StepSizeX;
			FLOAT EndY = SrcY + StepSizeY;
			
			// Generate a rectangular region of pixels and then find the average color of the region.
			INT PosY = appTrunc(SrcY+0.5f);
			PosY = Clamp<INT>(PosY, 0, (SrcHeight - 1));

			INT PosX = appTrunc(SrcX+0.5f);
			PosX = Clamp<INT>(PosX, 0, (SrcWidth - 1));

			INT EndPosY = appTrunc(EndY+0.5f);
			EndPosY = Clamp<INT>(EndPosY, 0, (SrcHeight - 1));

			INT EndPosX = appTrunc(EndX+0.5f);
			EndPosX = Clamp<INT>(EndPosX, 0, (SrcWidth - 1));

			FColor FinalColor;
			if(bLinearSpace)
			{
				FLinearColor LinearStepColor(0.0f,0.0f,0.0f,0.0f);
				for(INT PixelX = PosX; PixelX <= EndPosX; PixelX++)
				{
					for(INT PixelY = PosY; PixelY <= EndPosY; PixelY++)
					{
						INT StartPixel =  PixelX + PixelY * SrcWidth;

						// Convert from gamma space to linear space before the addition.
						LinearStepColor += SrcData(StartPixel);
						PixelCount++;
					}
				}
				LinearStepColor /= (FLOAT)PixelCount;

				// Convert back from linear space to gamma space.
				FinalColor = FColor(LinearStepColor);
			}
			else
			{
				FVector StepColor(0,0,0);
				for(INT PixelX = PosX; PixelX <= EndPosX; PixelX++)
				{
					for(INT PixelY = PosY; PixelY <= EndPosY; PixelY++)
					{
						INT StartPixel =  PixelX + PixelY * SrcWidth;
						StepColor.X += (FLOAT)SrcData(StartPixel).R;
						StepColor.Y += (FLOAT)SrcData(StartPixel).G;
						StepColor.Z += (FLOAT)SrcData(StartPixel).B;
						PixelCount++;
					}
				}
				StepColor /= (FLOAT)PixelCount;
				BYTE FinalR = Clamp(appTrunc(StepColor.X), 0, 255);
				BYTE FinalG = Clamp(appTrunc(StepColor.Y), 0, 255);
				BYTE FinalB = Clamp(appTrunc(StepColor.Z), 0, 255);
				FinalColor = FColor(FinalR, FinalG, FinalB);
			}

			// Store the final averaged pixel color value.
			FinalColor.A = 255;
			DstData(PixelPos) = FinalColor;

			SrcX = EndX;	
			PixelPos++;
		}

		SrcY += StepSizeY;
	}
}

/**
 * Creates a 2D texture from a array of raw color data.
 *
 * @param SrcWidth		Source image width.
 * @param SrcHeight		Source image height.
 * @param SrcData		Source image data.
 * @param Outer			Outer for the texture object.
 * @param Name			Name for the texture object.
 * @param Flags			Object flags for the texture object.
 * @param InParams		Params about how to set up the texture.
 * @return				Returns a pointer to the constructed 2D texture object.
 *
 */
UTexture2D* FImageUtils::CreateTexture2D(INT SrcWidth, INT SrcHeight, const TArray<FColor> &SrcData, UObject* Outer, const FString& Name, const EObjectFlags &Flags, const FCreateTexture2DParameters& InParams)
{
#if !CONSOLE && defined(_MSC_VER)

	UTexture2D* Tex2D;

	Tex2D = CastChecked<UTexture2D>( UObject::StaticConstructObject(UTexture2D::StaticClass(), Outer, FName(*Name), Flags) );
	Tex2D->Init(SrcWidth, SrcHeight, PF_A8R8G8B8);
	
	// Create base mip for the texture we created.
	BYTE* MipData = (BYTE*) Tex2D->Mips(0).Data.Lock(LOCK_READ_WRITE);
	for( INT y=0; y<SrcHeight; y++ )
	{
		BYTE* DestPtr = &MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FColor)];
		FColor* SrcPtr = const_cast<FColor*>(&SrcData((SrcHeight - 1 - y) * SrcWidth));
		for( INT x=0; x<SrcWidth; x++ )
		{
			*DestPtr++ = SrcPtr->B;
			*DestPtr++ = SrcPtr->G;
			*DestPtr++ = SrcPtr->R;
			if( InParams.bUseAlpha )
			{
				*DestPtr++ = SrcPtr->A;
			}
			else
			{
				*DestPtr++ = 0xFF;
			}
			SrcPtr++;
		}
	}
	Tex2D->Mips(0).Data.Unlock();

	// Set compression options.
	Tex2D->SRGB = InParams.bSRGB;
	Tex2D->CompressionSettings	= InParams.CompressionSettings;
	if( !InParams.bUseAlpha )
	{
		Tex2D->CompressionNoAlpha = TRUE;
	}
	Tex2D->DeferCompression	= InParams.bDeferCompression;

	if( InParams.bWantSourceArt )
	{
	    // Store uncompressed source art, which is, at the very least, compressed before saving.
	    Tex2D->SetUncompressedSourceArt(Tex2D->Mips(0).Data.Lock(LOCK_READ_ONLY), Tex2D->Mips(0).Data.GetBulkDataSize());
	    Tex2D->Mips(0).Data.Unlock();

		// Compress the source art ASAP if not deferring compression
	    if ( !Tex2D->DeferCompression )
	    {
		    Tex2D->CompressSourceArt();
	    }
	}

	// This will trigger compressions.
	// NOTE: If bWantSourceArt was FALSE, then the compression will use the first mip level as the
	//		 source art.  We rely on this behavior so that we don't need to waste time filling in
	//		 the source art buffer when it's not needed for anything else.
	Tex2D->PostEditChange();

	return Tex2D;
#else
	appErrorf(TEXT("ConstructTexture2D not supported on console."));
	return NULL;
#endif

}




/**
 * Adjusts the colors of the image using the specified settings (alpha channel will not be modified.)
 *
 * @param	ImageColors	Image to adjust
 * @param	InWidth		Width of image
 * @param	InHeight	Height of image
 * @param	bIsSRGB		True if image is in SRGB color space
 * @param	InParams	Color adjustment parameters
 */
void FImageUtils::AdjustImageColors( FColor* ImageColors, const INT InWidth, const INT InHeight, const UBOOL bIsSRGB, const FColorAdjustmentParameters& InParams )
{
	check( ImageColors != NULL );
	check( InWidth > 0 && InHeight > 0 );

	if( !appIsNearlyEqual( InParams.AdjustBrightness, 1.0f, (FLOAT)KINDA_SMALL_NUMBER ) ||
		!appIsNearlyEqual( InParams.AdjustBrightnessCurve, 1.0f, (FLOAT)KINDA_SMALL_NUMBER ) ||
		!appIsNearlyEqual( InParams.AdjustSaturation, 1.0f, (FLOAT)KINDA_SMALL_NUMBER ) ||
		!appIsNearlyEqual( InParams.AdjustVibrance, 0.0f, (FLOAT)KINDA_SMALL_NUMBER ) ||
		!appIsNearlyEqual( InParams.AdjustRGBCurve, 1.0f, (FLOAT)KINDA_SMALL_NUMBER ) ||
		!appIsNearlyEqual( InParams.AdjustHue, 0.0f, (FLOAT)KINDA_SMALL_NUMBER ) )
	{
		const INT NumPixels = InWidth * InHeight;
		for( INT CurPixelIndex = 0; CurPixelIndex < NumPixels; ++CurPixelIndex )
		{
			const FColor OriginalColor = ImageColors[ CurPixelIndex ];

			// Convert to a linear color
			FLinearColor LinearColor;
			{
				if( bIsSRGB )
				{
					LinearColor = FLinearColor( OriginalColor );
				}
				else
				{
					LinearColor = OriginalColor.ReinterpretAsLinear();
				}
			}

			// Convert to HSV
			FLinearColor HSVColor = LinearColor.LinearRGBToHSV();
			FLOAT& PixelHue = HSVColor.R;
			FLOAT& PixelSaturation = HSVColor.G;
			FLOAT& PixelValue = HSVColor.B;


			// Apply brightness adjustment
			if( !appIsNearlyEqual( InParams.AdjustBrightness, 1.0f, (FLOAT)KINDA_SMALL_NUMBER ) )
			{
				PixelValue *= InParams.AdjustBrightness;
			}


			// Apply brightness power adjustment
			if( !appIsNearlyEqual( InParams.AdjustBrightnessCurve, 1.0f, (FLOAT)KINDA_SMALL_NUMBER ) && InParams.AdjustBrightnessCurve != 0.0f )
			{
				// Raise HSV.V to the specified power
				PixelValue = appPow( PixelValue, InParams.AdjustBrightnessCurve );
			}


			// Apply "vibrance" adjustment
			if( !appIsNearlyZero( InParams.AdjustVibrance, (FLOAT)KINDA_SMALL_NUMBER ) )
			{
				const FLOAT SatRaisePow = 5.0f;
				const FLOAT InvSatRaised = appPow( 1.0f - PixelSaturation, SatRaisePow );

				const FLOAT ClampedVibrance = Clamp( InParams.AdjustVibrance, 0.0f, 1.0f );
				const FLOAT HalfVibrance = ClampedVibrance * 0.5f;

				const FLOAT SatProduct = HalfVibrance * InvSatRaised;

				PixelSaturation += SatProduct;
			}


			// Apply saturation adjustment
			if( !appIsNearlyEqual( InParams.AdjustSaturation, 1.0f, (FLOAT)KINDA_SMALL_NUMBER ) )
			{
				PixelSaturation *= InParams.AdjustSaturation;
			}


			// Apply hue adjustment
			if( !appIsNearlyZero( InParams.AdjustHue, (FLOAT)KINDA_SMALL_NUMBER ) )
			{
				PixelHue += InParams.AdjustHue;
			}



			// Clamp HSV values
			{
				PixelHue = appFmod( PixelHue, 360.0f );
				if( PixelHue < 0.0f )
				{
					// Keep the hue value positive as HSVToLinearRGB prefers that
					PixelHue += 360.0f;
				}
				PixelSaturation = Clamp( PixelSaturation, 0.0f, 1.0f );
				PixelValue = Clamp( PixelValue, 0.0f, 1.0f );
			}


			// Convert back to a linear color
			LinearColor = HSVColor.HSVToLinearRGB();


			// Apply RGB curve adjustment (linear space)
			if( !appIsNearlyEqual( InParams.AdjustRGBCurve, 1.0f, (FLOAT)KINDA_SMALL_NUMBER ) && InParams.AdjustRGBCurve != 0.0f )
			{
				LinearColor.R = appPow( LinearColor.R, InParams.AdjustRGBCurve );
				LinearColor.G = appPow( LinearColor.G, InParams.AdjustRGBCurve );
				LinearColor.B = appPow( LinearColor.B, InParams.AdjustRGBCurve );
			}

			
			// Convert to gamma space (if needed)
			FColor NewColor;
			{
				if( bIsSRGB )
				{
					NewColor = FColor( LinearColor );
				}
				else
				{
					NewColor.R = Clamp( appTrunc( LinearColor.R * 255.0f ), 0, 255 );
					NewColor.G = Clamp( appTrunc( LinearColor.G * 255.0f ), 0, 255 );
					NewColor.B = Clamp( appTrunc( LinearColor.B * 255.0f ), 0, 255 );
				}
			}


			// We retain the original alpha channel
			NewColor.A = OriginalColor.A;

			ImageColors[ CurPixelIndex ] = NewColor;
		}
		
	}

}

/**
* Compute the alpha channel how BokehDOF needs it setup
*
* @param	ImageColors			Image to adjust
* @param	InWidth				Width of image
* @param	InHeight			Height of image
* @param	bIsSRGB				True if image is in SRGB color space
 */
void FImageUtils::ComputeBokehAlpha(FColor* ImageColors, const INT InWidth, const INT InHeight, const UBOOL bIsSRGB)
{
	check( ImageColors != NULL );
	check( InWidth > 0 && InHeight > 0 );

	// compute LinearAverage
	FLinearColor LinearAverage;
	{
		FLinearColor LinearSum(0, 0, 0, 0);
		const INT NumPixels = InWidth * InHeight;
		for( INT CurPixelIndex = 0; CurPixelIndex < NumPixels; ++CurPixelIndex )
		{
			const FColor OriginalColor = ImageColors[ CurPixelIndex ];

			// Convert to a linear color
			FLinearColor LinearColor;
			if( bIsSRGB )
			{
				LinearColor = FLinearColor( OriginalColor );
			}
			else
			{
				LinearColor = OriginalColor.ReinterpretAsLinear();
			}

			LinearSum += LinearColor;
		}

		LinearAverage = LinearSum / (FLOAT)NumPixels;
	}

	FLinearColor Scale(1, 1, 1, 1);

	// we want to normalize the image to have 0.5 as average luminance, this is assuming clamping doesn't happen (can happen when using a very small Bokeh shape)
	{
		FLOAT RGBLum = (LinearAverage.R + LinearAverage.G + LinearAverage.B) / 3.0f;

		// ideally this would be 1 but then some pixels would need to be >1 which is not supported for the textureformat we want to use.
		// The value affects the occlusion computation of the BokehDOF
		const FLOAT LumGoal = 0.25f;

		// clamp to avoid division by 0
		Scale *= LumGoal / Max(RGBLum, 0.001f);
	}

	{
		const INT NumPixels = InWidth * InHeight;
		for( INT CurPixelIndex = 0; CurPixelIndex < NumPixels; ++CurPixelIndex )
		{
			const FColor OriginalColor = ImageColors[ CurPixelIndex ];

			// Convert to a linear color
			FLinearColor LinearColor;
			{
				if( bIsSRGB )
				{
					LinearColor = FLinearColor( OriginalColor );
				}
				else
				{
					LinearColor = OriginalColor.ReinterpretAsLinear();
				}
			}

			LinearColor *= Scale;

			FLOAT RGBLum = (LinearColor.R + LinearColor.G + LinearColor.B) / 3.0f;
	
			FColor NewColor = OriginalColor;
			
			NewColor.A = Clamp( appTrunc( RGBLum * 255.0f ), 0, 255 );

			ImageColors[ CurPixelIndex ] = NewColor;
		}
		
	}

}

