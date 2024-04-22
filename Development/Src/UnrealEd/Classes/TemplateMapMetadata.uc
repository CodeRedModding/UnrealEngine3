/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class TemplateMapMetadata extends Object	
	hidecategories(Object)
	native;
/**
 * Class used to store and edit template map metadata in packages.
 */

/** The thumbnail image used to display the map in the UI */
var() editoronly Texture2D Thumbnail;

cpptext
{
	/**
	 * Create a list of all current template map metadata objects.
	 * Current method for this is to add all needed metadata into packages
	 * that are always loaded in the editor so we can just iterate over all 
	 * UTemplateMapMetadata in memory.
	 *
	 * @param	Templates - list to which all metadata objects are added.
	 *			This should be empty when passed to this method.
	 */
	static void GenerateTemplateMetadataList(TArray<UTemplateMapMetadata*>& Templates);
}