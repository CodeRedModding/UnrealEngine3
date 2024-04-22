//! @file SubstanceAirEd.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include <UnrealEd.h>

#include "SubstanceAirEdThumbnailRendererClasses.h"

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirHelpers.h"
#include "SubstanceAirInstanceFactoryClasses.h"
#include "SubstanceAirImageInputClasses.h"

void UImageInputThumbnailRenderer::DrawCPU( 
	UObject* InObject, 
	FObjectThumbnail& OutThumbnailBuffer) const
{
	USubstanceAirImageInput* ImgInput = Cast<USubstanceAirImageInput>(InObject);

	if (ImgInput)
	{
		INT Width = 0;
		INT Height = 0;

		// the thumbnail system expects a 8bpp BGRA image
		const INT ReqComp = 4;

		void* DecompressedImage = 
			SubstanceAir::Helpers::DecompressJpeg(
				(BYTE*)ImgInput->CompressedImageRGB.Lock(LOCK_READ_ONLY),
				ImgInput->CompressedImageRGB.GetBulkDataSize(),
				&Width,
				&Height);

		ImgInput->CompressedImageRGB.Unlock();

		if (DecompressedImage)
		{
			OutThumbnailBuffer.SetImageSize(Width, Height);
			OutThumbnailBuffer.AccessImageData().Init(Width*Height*ReqComp);

			appMemcpy(
				&OutThumbnailBuffer.AccessImageData()(0),
				DecompressedImage, 
				Width*Height*ReqComp);

			appFree(DecompressedImage);
		}
	}
}

IMPLEMENT_CLASS(UImageInputThumbnailRenderer)
 
