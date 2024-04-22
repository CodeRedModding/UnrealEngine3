/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


/*===========================================================================
    Class and struct declarations which are coupled to
	EngineUIPrivateClasses.h but shouldn't be declared inside a class
===========================================================================*/

#ifndef NAMES_ONLY

#include "UnPrefab.h"	// Required for access to FPrefabUpdateArc

#ifndef __ENGINEUIPRIVATEGLOBALINCLUDES_H__
#define __ENGINEUIPRIVATEGLOBALINCLUDES_H__

// Forward declaration for the animation data struct
struct FUIAnimSequence;


enum EUISceneTickStats
{
	STAT_UISceneTickTime = STAT_UIDrawingTime+1,
	STAT_UISceneUpdateTime,
	STAT_UIPreRenderCallbackTime,
	STAT_UIRefreshFormatting,
	STAT_UIRebuildDockingStack,
	STAT_UIResolveScenePositions,
	STAT_UIRebuildNavigationLinks,
	STAT_UIRefreshWidgetStyles,

	STAT_UIAddDockingNode,
	STAT_UIAddDockingNode_String,

	STAT_UIResolvePosition,
	STAT_UIResolvePosition_String,
	STAT_UIResolvePosition_List,
	STAT_UIResolvePosition_AutoAlignment,

	STAT_UISceneRenderTime,

	STAT_UIGetStringFormatParms,
	STAT_UIApplyStringFormatting,

	STAT_UIParseString,

	STAT_UISetWidgetPosition,
	STAT_UIGetWidgetPosition,
	STAT_UICalculateBaseValue,
	STAT_UIGetPositionValue,
	STAT_UISetPositionValue,

	STAT_UIProcessInput,
};

#if SUPPORTS_DEBUG_LOGGING

#define LOG_DATAFIELD_UPDATE(SourceDataStore,bValuesInvalidated,PropertyTag,SourceProvider,ArrayIndex) \
	debugf(NAME_DevDataStore, TEXT("NotifyDataStoreUpdated PropertyTag:%s %s bValuesInvalidated:%i ArrayIndex:%i DS:%s    Provider:%s   Widget:%s"), \
	*PropertyTag.ToString(), bBoundToDataStore ? GTrue : GFalse, bValuesInvalidated, ArrayIndex, *SourceDataStore->GetName(), *SourceProvider->GetPathName(), *GetPathName());

/** the number of spaces to indent focus chain debug log messages */
extern INT FocusDebugIndent;

#else	//	SUPPORTS_DEBUG_LOGGING

#define LOG_DATAFIELD_UPDATE(SourceDataStore,bValuesInvalidated,PropertyTag,SourceProvider,ArrayIndex)

#endif	//	SUPPORTS_DEBUG_LOGGING

#endif	// __ENGINEUIPRIVATEGLOBALINCLUDES_H__
#endif	// NAMES_ONLY
