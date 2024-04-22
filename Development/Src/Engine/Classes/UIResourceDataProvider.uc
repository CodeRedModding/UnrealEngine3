/**
 * Base class for data providers which provide data for static game resources.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIResourceDataProvider extends UIPropertyDataProvider
	native(inherit)
	Config(Game)
	PerObjectConfig
	abstract;

/** Controls whether the object should be used or not. This is the only way to remove a per object config from the list */
var config bool bSkipDuringEnumeration;

/**
 * Provides the data provider with the chance to perform initialization, including preloading any content that will be needed by the provider.
 *
 * @param	bIsEditor	TRUE if the editor is running; FALSE if running in the game.
 */
event InitializeProvider( bool bIsEditor );

DefaultProperties
{
	//@fixme ronp - it might be better to handle this via the script custom handlers
	ComplexPropertyTypes.Remove(class'ArrayProperty')
}
