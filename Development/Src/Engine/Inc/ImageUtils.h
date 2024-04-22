/*=============================================================================
ImageUtils.h: Image utility functions.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __IMAGEUTILS_H__
#define __IMAGEUTILS_H__



/**
 * Color adjustment parameters for FImageUtils::AdjustImageColors()
 */
struct FColorAdjustmentParameters 
{
	/** Brightness adjustment (scales HSV value) */
	FLOAT AdjustBrightness;

	/** Curve adjustment (raises HSV value to the specified power) */
	FLOAT AdjustBrightnessCurve;

	/** Saturation adjustment (scales HSV saturation) */
	FLOAT AdjustSaturation;

	/** "Vibrance" adjustment (HSV saturation algorithm adjustment) */
	FLOAT AdjustVibrance;

	/** RGB curve adjustment (raises linear-space RGB color to the specified power) */
	FLOAT AdjustRGBCurve;

	/** Hue adjustment (offsets HSV hue by value in degrees) */
	FLOAT AdjustHue;


	/** Constructor */
	FColorAdjustmentParameters()
		: AdjustBrightness( 1.0f ),
		  AdjustBrightnessCurve( 1.0f ),
		  AdjustSaturation( 1.0f ),
		  AdjustVibrance( 0.0f ),
		  AdjustRGBCurve( 1.0f ),
		  AdjustHue( 0.0f )
	{
	}

};

/**
 *	Parameters used for creating a Texture2D frmo a simple color buffer.
 */
struct FCreateTexture2DParameters
{
	/** True if alpha channel is used */
	UBOOL						bUseAlpha;

	/** Compression settings to use for texture */
	TextureCompressionSettings	CompressionSettings;

	/** If texture should be compressed right away, or defer until package is saved */
	UBOOL						bDeferCompression;

	/** If texture should be set as SRGB */
	UBOOL						bSRGB;

	/** True if we should also store source art for this texture */
	UBOOL						bWantSourceArt;

	FCreateTexture2DParameters()
		:	bUseAlpha(FALSE),
			CompressionSettings(TC_Default),
			bDeferCompression(FALSE),
			bSRGB(TRUE),
			bWantSourceArt(TRUE)
	{
	}
};

/**
 * Class of static image utility functions.
 */
class FImageUtils
{
public:
	/**
	 * Resizes the given image using a simple average filter and stores it in the destination array.  This version constrains aspect ratio.
	 *
	 * @param SrcWidth	Source image width.
	 * @param SrcHeight	Source image height.
	 * @param SrcData	Source image data.
	 * @param DstWidth	Destination image width.
	 * @param DstHeight Destination image height.
	 * @param DstData	Destination image data.
	 */
	static void ImageResize(INT SrcWidth, INT SrcHeight, const TArray<FColor> &SrcData,  INT DstWidth, INT DstHeight, TArray<FColor> &DstData, UBOOL bLinearSpace );

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
	static UTexture2D* CreateTexture2D(INT SrcWidth, INT SrcHeight, const TArray<FColor> &SrcData, UObject* Outer, const FString& Name, const EObjectFlags &Flags, const FCreateTexture2DParameters& InParams);

	/**
	 * Adjusts the colors of the image using the specified settings (alpha channel will not be modified.)
	 *
	 * @param	ImageColors	Image to adjust
	 * @param	InWidth		Width of image
	 * @param	InHeight	Height of image
	 * @param	bIsSRGB		True if image is in SRGB color space
	 * @param	InParams	Color adjustment parameters
	 */
	static void AdjustImageColors( FColor* ImageColors, const INT InWidth, const INT InHeight, const UBOOL bIsSRGB, const FColorAdjustmentParameters& InParams );

	/**
	 * Compute the alpha channel how BokehDOF needs it setup
	 *
	 * @param	ImageColors			Image to adjust
	 * @param	InWidth				Width of image
	 * @param	InHeight			Height of image
	 * @param	bIsSRGB				True if image is in SRGB color space
	 */
	static void ComputeBokehAlpha(FColor* ImageColors, const INT InWidth, const INT InHeight, const UBOOL bIsSRGB);
};

#endif

