/*=============================================================================
	PostprocessAA.h: For post process anti aliasing.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineClasses.h"

#pragma once

class FPostProcessAA
{
public:
	/** 
	* Initialization constructor. 
	* @param InEffect - PostProcessEffect where the techniques properties are stored in
	*/
	FPostProcessAA(const UUberPostProcessEffect* InEffect,const FPostProcessSettings* WorldSettings);

	UBOOL IsEnabled(const FViewInfo& View) const;

	/**
	* Render the post process effect
	* Called by the rendering thread during scene rendering
	* @param View - current view
	*/
	void Render(FViewInfo& View) const;

	/** set static DeferredObject */
	void SetDeferredObject() const;

	/** get static DeferredObject */
	static const FPostProcessAA* GetDeferredObject();

protected:
	// MLAA passes
	void RenderEdgeDetectingPass(FViewInfo& View) const;
	void RenderComputeEdgeLengthPass(FViewInfo& View) const;
	void RenderBlendColorPass(FViewInfo& View) const;	

	// FXAA pass
	void RenderFXAA(FViewInfo& View) const;

	FLOAT EdgeDetectionThreshold;
	EPostProcessAAType PostProcessAAType;

	static const FPostProcessAA* DeferredObject;
};
