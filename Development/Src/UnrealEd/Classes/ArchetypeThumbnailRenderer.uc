/**
 * This is a simple thumbnail renderer that uses a specified icon as the
 * thumbnail view for a resource. It will only render UClass objects with
 * the CLASS_Archetype flag
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ArchetypeThumbnailRenderer extends IconThumbnailRenderer
	native;

cpptext
{
	/**
	 * Allows the thumbnail renderer object the chance to reject rendering a
	 * thumbnail for an object based upon the object's data. For instance, an
	 * archetype should only be rendered if it's flags have RF_ArchetypeObject.
	 *
	 * @param Object 			the object to inspect
	 * @param bCheckObjectState	TRUE indicates that the object's state should be inspected to determine whether it can be supported;
	 *							FALSE indicates that only the object's type should be considered (for caching purposes)
	 *
	 * @return TRUE if it needs a thumbnail, FALSE otherwise
	 */
	virtual UBOOL SupportsThumbnailRendering(UObject* Object,UBOOL bCheckObjectState=TRUE);
}
