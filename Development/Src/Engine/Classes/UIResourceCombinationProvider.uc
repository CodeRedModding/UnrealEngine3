/**
 * Base class for all data providers which provide additional dynamic information about a specific static data provider instance.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIResourceCombinationProvider extends UIDataProvider
	native(UIPrivate)
	PerObjectConfig
	Config(Game)
	abstract;

/**
 * Each combo provider is linked to a single static resource data provider.  The name of the combo provider should match the name of the
 * static resource it's associated with, as the dynamic resource data store will match combo providers to the static provider with the same name.
 */
var	transient	UIResourceDataProvider					StaticDataProvider;

/**
 * The data provider which provides access to a player's profile data.
 */
var	transient	UIDataProvider_OnlineProfileSettings	ProfileProvider;

/**
 * Provides the data provider with the chance to perform initialization, including preloading any content that will be needed by the provider.
 *
 * @param	bIsEditor					TRUE if the editor is running; FALSE if running in the game.
 * @param	InStaticResourceProvider	the data provider that provides the static resource data for this combo provider.
 * @param	InProfileProvider			the data provider that provides profile data for the player associated with the owning data store.
 */
event InitializeProvider( bool bIsEditor, UIResourceDataProvider InStaticResourceProvider, UIDataProvider_OnlineProfileSettings InProfileProvider )
{
	StaticDataProvider = InStaticResourceProvider;
	ProfileProvider = InProfileProvider;
}

`define		debug_resourcecombo_provider	1==0

/**
 * Clears all references in this data provider.  Called when the owning data store is unregistered.
 */
function ClearProviderReferences()
{
	StaticDataProvider = None;
	ProfileProvider = None;
}


DefaultProperties
{

}
