/**
 * Generic browser type for editing UIScenes
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GenericBrowserType_TemplateMapMetadata extends GenericBrowserType
	native;

cpptext
{
	/**
	 * Initialize the supported classes for this browser type.
	 */
	virtual void Init();
}

DefaultProperties
{
	Description="TemplateMapMetaData"
}