/**
 * This is the anim tree label renderer
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AnimTreeLabelRenderer extends ThumbnailLabelRenderer
	native;

cpptext
{
protected:
	/**
	 * Adds the name of the object and information about the anim tree
	 *
	 * @param Object the object to build the labels for
	 * @param OutLabels the array that is added to
	 */
	void BuildLabelList(UObject* Object, const ThumbnailOptions& InOptions, TArray<FString>& OutLabels);
}
