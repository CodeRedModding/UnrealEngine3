/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DRAGTOOL_MEASURE_H__
#define __DRAGTOOL_MEASURE_H__

#include "UnEdDragTools.h"

/**
 * Draws a line in the current viewport and displays the distance between
 * its start/end points in the center of it.
 */
class FDragTool_Measure : public FDragTool
{
public:
	FDragTool_Measure();

	virtual void Render3D(const FSceneView* View,FPrimitiveDrawInterface* PDI);
	virtual void Render(const FSceneView* View,FCanvas* Canvas);
};

#endif // __DRAGTOOL_MEASURE_H__
