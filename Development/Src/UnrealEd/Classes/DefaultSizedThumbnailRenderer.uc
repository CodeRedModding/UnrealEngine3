/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This thumbnail renderer holds some commonly shared properties
 */
class DefaultSizedThumbnailRenderer extends ThumbnailRenderer
	native
	abstract
	config(Editor);

/**
 * The default width of this thumbnail
 */
var config int DefaultSizeX;

/**
 * The default height of this thumbnail
 */
var config int DefaultSizeY;

cpptext
{
	/**
	 * Calculates the size the thumbnail would be at the specified zoom level.
	 *
	 * @param Object the object the thumbnail is of
	 * @param PrimType ignored
	 * @param Zoom the current multiplier of size
	 * @param OutWidth the var that gets the width of the thumbnail
	 * @param OutHeight the var that gets the height
	 */
	virtual void GetThumbnailSize(UObject* Object,EThumbnailPrimType,
		FLOAT Zoom,DWORD& OutWidth,DWORD& OutHeight);
}
