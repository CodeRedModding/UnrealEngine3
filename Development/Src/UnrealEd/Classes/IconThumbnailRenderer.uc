/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is a simple thumbnail renderer that uses a specified icon as the
 * thumbnail view for a resource.
 */
class IconThumbnailRenderer extends ThumbnailRenderer
	native;

/**
 * Name of the texture to load and use as the icon
 */
var String IconName;

/**
 * This is the icon once it has been loaded
 */
var Texture2D Icon;

cpptext
{
protected:
	/**
	 * Returns the icon for this icon renderer instance. Loads it if it
	 * isn't already loaded
	 *
	 * @param A valid icon or the default texture if it couldn't be loaded
	 */
	inline UTexture2D* GetIcon(void)
	{
		// If this hasn't been loaded yet, load it
		if (Icon == NULL)
		{
			Icon = LoadObject<UTexture2D>(NULL,*IconName,NULL,LOAD_None,NULL);
			// Just in case the resource is bogus, check and return the default
			if (Icon == NULL)
			{
				Icon = GWorld->GetWorldInfo()->DefaultTexture;
			}
		}
		return Icon;
	}

public:
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
		FLOAT Zoom,DWORD& OutWidth,DWORD& OutHeight)
	{
		OutWidth = appTrunc(Zoom * (FLOAT)GetIcon()->SizeX);
		OutHeight = appTrunc(Zoom * (FLOAT)GetIcon()->SizeY);
	}

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
}
