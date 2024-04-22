/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is a simple thumbnail label renderer that lists the object name
 * and the amount of memory used by the object. It is an example of how
 * you can use a different thumbnail label for different information
 */
class MemCountThumbnailLabelRenderer extends ThumbnailLabelRenderer
	native;

/**
 * An aggregated thumbnail label renderer component. Used when appending the
 * memory usage information to an existing label renderer's list.
 */
var ThumbnailLabelRenderer AggregatedLabelRenderer;

cpptext
{
protected:
	/**
	 * Adds the name of the object and the amount of memory used to the array
	 *
	 * @param Object the object to build the labels for
	 * @param OutLabels the array that is added to
	 */
	void BuildLabelList(UObject* Object, const ThumbnailOptions& InOptions, TArray<FString>& OutLabels);

public:
	/**
	 * Calculates the size the thumbnail labels will be for the specified font.
	 * Doesn't serialize the object so that it's faster
	 *
	 * @param Object the object the thumbnail is of
	 * @param Font the font object to render with
	 * @param RI the render interface to use for getting the size
	 * @param OutWidth the var that gets the width of the labels
	 * @param OutHeight the var that gets the height
	 */
	virtual void GetThumbnailLabelSize(UObject* Object,UFont* Font,
		FCanvas* Canvas, const ThumbnailOptions& InOptions, DWORD& OutWidth,
		DWORD& OutHeight);
}
