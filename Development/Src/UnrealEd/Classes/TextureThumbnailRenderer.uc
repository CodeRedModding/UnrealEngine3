/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This thumbnail renderer displays the texture for the object in question
 */
class TextureThumbnailRenderer extends ThumbnailRenderer
	native;

cpptext
{

	/** 
	 * Checks to see if the passed in object supports a thumbnail rendered directly into a system memory buffer for thumbnails
	 * instead of setting up a render target and rendering to a texture from the GPU. 
	 *
	 * @param InObject	The object to check
	 */
	virtual UBOOL SupportsCPUGeneratedThumbnail(UObject *InObject) const;

	/**
	 * Calculates the size the thumbnail would be at the specified zoom level
	 *
	 * @param Object the object the thumbnail is of
	 * @param PrimType ignored
	 * @param Zoom the current multiplier of size
	 * @param OutWidth the var that gets the width of the thumbnail
	 * @param OutHeight the var that gets the height
	 */
	virtual void GetThumbnailSize(UObject* Object,EThumbnailPrimType,
		FLOAT Zoom,DWORD& OutWidth,DWORD& OutHeight);

	/**
	 * Draws a thumbnail for the object that was specified
	 *
	 * @param Object the object to draw the thumbnail for
	 * @param PrimType ignored
	 * @param X the X coordinate to start drawing at
	 * @param Y the Y coordinate to start drawing at
	 * @param Width the width of the thumbnail to draw
	 * @param Height the height of the thumbnail to draw
	 * @param Viewport ignored
	 * @param Canvas the render interface to draw with
	 * @param BackgroundType type of background for the thumbnail
	 * @param PreviewBackgroundColor background color for material previews
	 * @param PreviewBackgroundColorTranslucent background color for translucent material previews
	 */
	virtual void Draw(UObject* Object,EThumbnailPrimType,INT X,INT Y,
		DWORD Width,DWORD Height,FRenderTarget*,FCanvas* Canvas,
		EThumbnailBackgroundType BackgroundType,
		FColor PreviewBackgroundColor,
		FColor PreviewBackgroundColorTranslucent);

	/**
	 * Draws the thumbnail directly to a CPU memory buffer
	 *
	 * @param InObject				The object to draw
	 * @param OutThumbnailBuffer	The thumbnail buffer to draw to
	 */
	virtual void DrawCPU( UObject* InObject, FObjectThumbnail& OutThumbnailBuffer) const;

private:
	/**
	 * Converts a 1 bit monochrome texture into a thumbnail for the content browser 
	 *
	 * @param MonochromeTexture	The texture to convert
	 * @param OutThumbnail	The thumbnail object where the thumbnail image data should be stored
	 */
	void MakeThumbnailFromMonochrome( UTexture2D* MonochromeTexture, FObjectThumbnail& OutThumbnail ) const;
}
