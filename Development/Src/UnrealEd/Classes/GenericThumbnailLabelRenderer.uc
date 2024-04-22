/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is a simple thumbnail label renderer that lists the object name
 * and the object type for rendering the labels
 */
class GenericThumbnailLabelRenderer extends ThumbnailLabelRenderer
	native;

cpptext
{
protected:
	/**
	 * Adds the name of the object and the friendly class name
	 *
	 * @param Object the object to build the labels for
	 * @param OutLabels the array that is added to
	 */
	void BuildLabelList(UObject* Object, const ThumbnailOptions& InOptions, TArray<FString>& OutLabels)
	{
		new(OutLabels)FString(Object->GetName());
		new(OutLabels)FString(Object->GetDesc());
	}
}
