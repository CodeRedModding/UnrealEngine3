//=============================================================================
// Scene - script exposed scene enums
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class Scene extends Object
	native(Scene);

/**
 * A priority for sorting scene elements by depth.
 * Elements with higher priority occlude elements with lower priority, disregarding distance.
 */
enum ESceneDepthPriorityGroup
{
	// unreal ed background scene DGP
	SDPG_UnrealEdBackground,
	// world scene DPG
	SDPG_World,
	// foreground scene DPG
	SDPG_Foreground,
	// unreal ed scene DPG
	SDPG_UnrealEdForeground,
	// after all scene rendering
	SDPG_PostProcess
};

/** Detail mode for primitive component rendering. */
enum EDetailMode
{
	DM_Low,
	DM_Medium,
	DM_High,
};

/** bits needed to store DPG value */
const SDPG_NumBits = 3;