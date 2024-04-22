/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is an abstract base class that is used to define the interface that
 * UnrealEd will use when rendering a given object's thumbnail. The editor
 * only calls the virtual rendering function.
 */
class ThumbnailRenderer extends Object
	abstract
	native;

cpptext
{
	/**
	 * Allows the thumbnail renderer object the chance to reject rendering a
	 * thumbnail for an object based upon the object's data. For instance, an
	 * archetype should only be rendered if it's flags have RF_ArchetypeObject.
	 *
	 * @param Object			the object to inspect
	 * @param bCheckObjectState	TRUE indicates that the object's state should be inspected to determine whether it can be supported;
	 *							FALSE indicates that only the object's type should be considered (for caching purposes)
	 *
	 * @return TRUE if it needs a thumbnail, FALSE otherwise
	 */
	virtual UBOOL SupportsThumbnailRendering(UObject*,UBOOL bCheckObjectState=TRUE)
	{
		return TRUE;
	}


	/** 
	 * Checks to see if the passed in object supports a thumbnail rendered directly into a system memory buffer for thumbnails
	 * instead of setting up a render target and rendering to a texture from the GPU. 
	 *
	 * @param InObject	The object to check
	 */
	virtual UBOOL SupportsCPUGeneratedThumbnail(UObject *InObject) const
	{
		return FALSE;
	}

	/**
	 * Calculates the size the thumbnail would be at the specified zoom level
	 *
	 * @param Object the object the thumbnail is of
	 * @param PrimType the primitive type to use for rendering
	 * @param Zoom the current multiplier of size
	 * @param OutWidth the var that gets the width of the thumbnail
	 * @param OutHeight the var that gets the height
	 */
	virtual void GetThumbnailSize(UObject* Object,EThumbnailPrimType PrimType,
		FLOAT Zoom,DWORD& OutWidth,DWORD& OutHeight) PURE_VIRTUAL(UThumbnailRenderer::GetThumbnailSize,);

	/**
	 * Draws a thumbnail for the object that was specified.
	 *
	 * @param Object the object to draw the thumbnail for
	 * @param PrimType the primitive to draw on (sphere, plane, etc.)
	 * @param X the X coordinate to start drawing at
	 * @param Y the Y coordinate to start drawing at
	 * @param Width the width of the thumbnail to draw
	 * @param Height the height of the thumbnail to draw
	 * @param Viewport the viewport being drawn in
	 * @param Canvas the render interface to draw with
	 * @param BackgroundType type of background for the thumbnail
	 * @param PreviewBackgroundColor background color for material previews
	 * @param PreviewBackgroundColorTranslucent background color for translucent material previews
	 */
	virtual void Draw(UObject* Object,EThumbnailPrimType PrimType,
		INT X,INT Y,DWORD Width,DWORD Height,FRenderTarget* Viewport,
		FCanvas* Canvas,EThumbnailBackgroundType BackgroundType,
		FColor PreviewBackgroundColor,
		FColor PreviewBackgroundColorTranslucent) PURE_VIRTUAL(UThumbnailRenderer::Draw,);

	/**
	 * Draws the thumbnail directly to a CPU memory buffer
	 *
	 * @param InObject				The object to draw
	 * @param OutThumbnailBuffer	The thumbnail buffer to draw to
	 */
	virtual void DrawCPU( UObject* InObject, FObjectThumbnail& OutThumbnailBuffer) const
	{
		// Do nothing by default
	}

}
