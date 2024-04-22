//! @file ImageInputThumbnailRenderer.uc
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.
//!
//! @brief This thumbnail renderer displays the texture for the object in question

class ImageInputThumbnailRenderer extends ThumbnailRenderer
	native(ThumbnailRenderer);

cpptext
{
	virtual UBOOL SupportsCPUGeneratedThumbnail(UObject *InObject) const
    {
        return TRUE;
    }

	virtual void DrawCPU( UObject* InObject, FObjectThumbnail& OutThumbnailBuffer) const;
}
