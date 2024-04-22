/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// CascadeConfiguration
//
// Settings for Cascade that users are not allowed to alter.
//=============================================================================
class CascadeConfiguration extends Object	
	hidecategories(Object)
	config(Editor)
	native;	

/** Module-to-TypeData mapping helper. */
struct native ModuleMenuMapper
{
	var string ObjName;
	var array<string> InvalidObjNames;
};

/**
 *	TypeData-to-base module mappings.
 *	These will disallow complete 'sub-menus' depending on the TypeData utilized.
 */
var(Configure) config array<ModuleMenuMapper> ModuleMenu_TypeDataToBaseModuleRejections;
/** Module-to-TypeData mappings. */
var(Configure) config array<ModuleMenuMapper> ModuleMenu_TypeDataToSpecificModuleRejections;
/** Modules that Cascade should ignore in the menu system. */
var(Configure) config array<string> ModuleMenu_ModuleRejections;

defaultproperties
{
}
