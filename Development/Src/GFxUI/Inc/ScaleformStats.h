/**********************************************************************

Filename    :   ScaleformStats.h
Content     :   Declarations for GFx statistics tracking

Copyright   :   (c) 2001-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

#ifndef ScaleformStats_h
#define ScaleformStats_h

#if WITH_GFx

/*!
Stat group for Scaleform GFx
*/
enum EScaleformStatGroup
{
	// Amount of time it takes to render the UI
	STAT_GFxRenderUIRT = STAT_GFxFirstStat,
	// Amount of time it takes to render the RTT
	STAT_GFxRenderTexturesRT,

	// Amount of time in Tick for HUD movies
	STAT_GFxTickUI,
	// Amount of time in Tick for RTT movies
	STAT_GFxTickRTT,

	// Amount of memory that Scaleform allocates outside of the normal Unreal Object tree.
	STAT_GFxInternalMem,
	// Amount of memory peak (per frame) that Scaleform has allocated
	STAT_GFxFramePeakMem,
	// Total estimate of memory used by Scaleform
	STAT_GFxTotalMem,

	// UTextures - either from SetExternalTexture or gfxexport
	STAT_GFxUTextureCount,
	STAT_GFxUTextureMem,

    // Internal textures - either embedded in SWF file or dynamic (e.g., gradient)
	STAT_GFxFTextureCount,
	STAT_GFxFTextureMem,

	// Number of meshes drawn
	STAT_GFxMeshesDrawn,
	// Number of Triangles
	STAT_GFxTrianglesDrawn,
    // Number of Primitives
    STAT_GFxPrimitivesDrawn,
	// Number of objects in GC array
	STAT_GFxGCManagedCount
};

#endif // WITH_GFx

#endif // ScaleformStats_h
