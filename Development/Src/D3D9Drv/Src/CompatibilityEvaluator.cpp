/*=============================================================================
CompatibilityEvaluator.cpp:
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D9DrvPrivate.h"
#include "HardwareID.h"
#include "VideoDevice.h"

#include <algorithm>
#include <functional>

#if !DEDICATED_SERVER

/**
 *	Assign compatibility settings based on specified Level.
 */
UBOOL SetCompatibilityLevelWindows( FCompatibilityLevelInfo Level, UBOOL bWriteToIni )
{
	return TRUE;
}

/**
 * Determine the default compatibility level for the machine.
 */
FCompatibilityLevelInfo GetCompatibilityLevelWindows()
{
	UINT CompositeCompatLevel = 5;
	UINT CPUCompatLevel = 5;
	UINT GPUCompatLevel = 5;

	return FCompatibilityLevelInfo(CompositeCompatLevel, CPUCompatLevel, GPUCompatLevel);
}

#endif