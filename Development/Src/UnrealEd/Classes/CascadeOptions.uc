/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// CascadeOptions
//
// A configuration class that holds information for the setup of Cascade.
// Supplied so that the editor 'remembers' the last setup the user had.
//=============================================================================
class CascadeOptions extends Object	
	hidecategories(Object)
	config(EditorUserSettings)
	native;	

var(Options)		config bool			bShowModuleDump;
var(Options)		config color		BackgroundColor;
var(Options)		config bool			bUseSubMenus;
var(Options)		config bool			bUseSpaceBarReset;
var(Options)		config bool			bUseSpaceBarResetInLevel;
var(Options)		config color		Empty_Background;
var(Options)		config color		Emitter_Background;
var(Options)		config color		Emitter_Unselected;
var(Options)		config color		Emitter_Selected;
var(Options)		config color		ModuleColor_General_Unselected;
var(Options)		config color		ModuleColor_General_Selected;
var(Options)		config color		ModuleColor_TypeData_Unselected;
var(Options)		config color		ModuleColor_TypeData_Selected;
var(Options)		config color		ModuleColor_Beam_Unselected;
var(Options)		config color		ModuleColor_Beam_Selected;
var(Options)		config color		ModuleColor_Trail_Unselected;
var(Options)		config color		ModuleColor_Trail_Selected;
var(Options)		config color		ModuleColor_Spawn_Unselected;
var(Options)		config color		ModuleColor_Spawn_Selected;
var(Options)		config color		ModuleColor_Required_Unselected;
var(Options)		config color		ModuleColor_Required_Selected;
var(Options)		config color		ModuleColor_Event_Unselected;
var(Options)		config color		ModuleColor_Event_Selected;

var(Options)		config bool			bShowGrid;
var(Options)		config color		GridColor_Hi;
var(Options)		config color		GridColor_Low;
var(Options)		config float		GridPerspectiveSize;

var(Options)		config bool			bShowParticleCounts;
var(Options)		config bool			bShowParticleEvents;
var(Options)		config bool			bShowParticleTimes;
var(Options)		config bool			bShowParticleDistance;
var(Options)		config bool			bShowParticleMemory;
var(Options)		config float		ParticleMemoryUpdateTime;

var(Options)		config bool			bShowFloor;
var(Options)		config string		FloorMesh;
var(Options)		config vector		FloorPosition;
var(Options)		config rotator		FloorRotation;
var(Options)		config float		FloorScale;
var(Options)		config vector		FloorScale3D;

var(Options)		config string		PostProcessChainName;

var(Options)		config int			ShowPPFlags;

/** If TRUE, use the 'slimline' module drawing method in cascade. */
var(Options)		config bool			bUseSlimCascadeDraw;
/** The height to use for the 'slimline' module drawing method in cascade. */
var(Options)		config int			SlimCascadeDrawHeight;
/** If TRUE, center the module name and buttons in the module box. */
var(Options)		config bool			bCenterCascadeModuleText;
/** The number of units the mouse must move before considering the module as dragged. */
var(Options)		config int			Cascade_MouseMoveThreshold;

/**
 *	TypeData-to-base module mappings.
 *	These will disallow complete 'sub-menus' depending on the TypeData utilized.
 */
var deprecated config array<ModuleMenuMapper> ModuleMenu_TypeDataToBaseModuleRejections;
/** Module-to-TypeData mappings. */
var deprecated config array<ModuleMenuMapper> ModuleMenu_TypeDataToSpecificModuleRejections;
/** Modules that Cascade should ignore in the menu system. */
var deprecated config array<string> ModuleMenu_ModuleRejections;

/** The radius of the motion mode */
var(Options) config float MotionModeRadius;

defaultproperties
{
}
