/*=============================================================================
	UnFaceFXSupport.h: FaceFX support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _UNFACEFXSUPPORT_H_
#define _UNFACEFXSUPPORT_H_

#if WITH_FACEFX

#if PS3_SNC
#pragma diag_suppress=112,552
#endif

#include "../../../External/FaceFX/FxSDK/Inc/FxSDK.h"
#include "../../../External/FaceFX/FxSDK/Inc/FxMemory.h"
#include "../../../External/FaceFX/FxSDK/Inc/FxActor.h"
#include "../../../External/FaceFX/FxSDK/Inc/FxActorInstance.h"
#include "../../../External/FaceFX/FxSDK/Inc/FxArchiveStoreFile.h"
#include "../../../External/FaceFX/FxSDK/Inc/FxArchiveStoreMemory.h"

#if PS3_SNC
#pragma diag_default=112,552
#endif

// Set this to 1 to log FaceFX performance timing data to the log.
#define LOG_FACEFX_PERF 0

// Initialize FaceFX.
void UnInitFaceFX( void );
// Shut down FaceFX.
void UnShutdownFaceFX( void );

/** FaceFX stats */
enum EFaceFXStats
{
	STAT_FaceFX_TickTime = STAT_FaceFXFirstStat,
	STAT_FaceFX_BeginFrameTime,
	STAT_FaceFX_MorphPassTime,
	STAT_FaceFX_MaterialPassTime,
	STAT_FaceFX_BoneBlendingPassTime,
	STAT_FaceFX_EndFrameTime,
	STAT_FaceFX_PlayAnim,
	STAT_FaceFX_PlayAnim1,
	STAT_FaceFX_PlayAnim2,
	STAT_FaceFX_PlayAnim3,
	STAT_FaceFX_PlayAnim4,
	STAT_FaceFX_PlayAnim5,
	STAT_FaceFX_PlayAnim6,
	STAT_FaceFX_PlayAnim7,
	STAT_FaceFX_Mount,
	STAT_FaceFX_UnMount	
};

#endif // WITH_FACEFX

#endif // _UNFACEFXSUPPORT_H_