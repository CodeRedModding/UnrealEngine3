/*=============================================================================
	PrimitiveSceneProxyOcclusionTracker.cpp: Scene proxy for occlusion percentage implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EngineMaterialClasses.h"
#include "ScenePrivate.h"
#include "PrimitiveSceneProxyOcclusionTracker.h"

/**
 * Encapsulates the data which is mirrored to render a primitive parallel to the game thread.
 */
/** Initialization constructor. */
FPrimitiveSceneProxyOcclusionTracker::FPrimitiveSceneProxyOcclusionTracker(const UPrimitiveComponent* InComponent) :
	  CoveragePercentage(0.0f)
{
}

/** Virtual destructor. */
FPrimitiveSceneProxyOcclusionTracker::~FPrimitiveSceneProxyOcclusionTracker()
{
}

void FPrimitiveSceneProxyOcclusionTracker::UpdateOcclusionBounds(const FBoxSphereBounds& InOcclusionBounds)
{
	// queue a call to update this data
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		PrimitiveSceneProxyOcclusionTrackUpdateOcclusionBounds,
		FPrimitiveSceneProxyOcclusionTracker*, Tracker, this,
		FBoxSphereBounds, InOcclusionBounds, InOcclusionBounds,
	{
		Tracker->UpdateOcclusionBounds_RenderThread(InOcclusionBounds);
	}
	);
}
void FPrimitiveSceneProxyOcclusionTracker::UpdateOcclusionBounds_RenderThread(const FBoxSphereBounds& InOcclusionBounds)
{
	OcclusionBounds = InOcclusionBounds;
}

/** 
 *	Get the results of the last frames occlusion and kick off the one for the next frame
 *
 *	@param	PrimitiveComponent - the primitive component being rendered
 *	@param	PDI - draw interface to render to
 *	@param	View - current view
 *	@param	DPGIndex - current depth priority 
 *	@param	Flags - optional set of flags from EDrawDynamicElementFlags
 */
UBOOL FPrimitiveSceneProxyOcclusionTracker::UpdateAndRenderOcclusionData(UPrimitiveComponent* PrimitiveComponent, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
{
	if (View->State == NULL)
	{
		return FALSE;
	}

	if (GIsEditor)
	{
		if (View->ProjectionMatrix.M[3][3] >= 1.0f)
		{
			return FALSE;
		}
	}
	FSceneViewState* State = (FSceneViewState*)(View->State);
	check(State);

	FCoverageInfo* StoredCoverage = CoverageMap.Find(State);
	if (StoredCoverage == NULL)
	{
		FCoverageInfo TempInfo;
		CoverageMap.Set(State, TempInfo);
		StoredCoverage = CoverageMap.Find(State);
	}
	check(StoredCoverage);
	if (StoredCoverage->LastSampleTime == State->LastRenderTime)
	{
		CoveragePercentage = StoredCoverage->Percentage;
		return TRUE;
	}

	FLOAT LocalCoveragePercentage;
	if (State->GetPrimitiveCoveragePercentage(PrimitiveComponent, LocalCoveragePercentage) == TRUE)
	{
		if (View->Family->ShowFlags & SHOW_Game)
		{
			CoveragePercentage = StoredCoverage->Percentage;
		}

		// Determine the expected screenspace coverage...
		// Translate the occlusion bounds to the translated world-space used for rendering.
		const FBoxSphereBounds TranslatedOcclusionBounds(
			OcclusionBounds.Origin + View->PreViewTranslation,
			OcclusionBounds.BoxExtent,
			OcclusionBounds.SphereRadius
			);

		// Set up an array of points containing the 8 corners of the bounds
		FVector TransformedPoints[8];
		FVector WorldPoint;

		WorldPoint = OcclusionBounds.Origin;
		TransformedPoints[0] = FVector(WorldPoint.X + OcclusionBounds.BoxExtent.X, WorldPoint.Y + OcclusionBounds.BoxExtent.Y, WorldPoint.Z + OcclusionBounds.BoxExtent.Z);
		TransformedPoints[1] = FVector(WorldPoint.X - OcclusionBounds.BoxExtent.X, WorldPoint.Y + OcclusionBounds.BoxExtent.Y, WorldPoint.Z + OcclusionBounds.BoxExtent.Z);
		TransformedPoints[2] = FVector(WorldPoint.X + OcclusionBounds.BoxExtent.X, WorldPoint.Y - OcclusionBounds.BoxExtent.Y, WorldPoint.Z + OcclusionBounds.BoxExtent.Z);
		TransformedPoints[3] = FVector(WorldPoint.X - OcclusionBounds.BoxExtent.X, WorldPoint.Y - OcclusionBounds.BoxExtent.Y, WorldPoint.Z + OcclusionBounds.BoxExtent.Z);
		TransformedPoints[4] = FVector(WorldPoint.X + OcclusionBounds.BoxExtent.X, WorldPoint.Y + OcclusionBounds.BoxExtent.Y, WorldPoint.Z - OcclusionBounds.BoxExtent.Z);
		TransformedPoints[5] = FVector(WorldPoint.X - OcclusionBounds.BoxExtent.X, WorldPoint.Y + OcclusionBounds.BoxExtent.Y, WorldPoint.Z - OcclusionBounds.BoxExtent.Z);
		TransformedPoints[6] = FVector(WorldPoint.X + OcclusionBounds.BoxExtent.X, WorldPoint.Y - OcclusionBounds.BoxExtent.Y, WorldPoint.Z - OcclusionBounds.BoxExtent.Z);
		TransformedPoints[7] = FVector(WorldPoint.X - OcclusionBounds.BoxExtent.X, WorldPoint.Y - OcclusionBounds.BoxExtent.Y, WorldPoint.Z - OcclusionBounds.BoxExtent.Z);

		// Determine the min and max screen position of the rendered bounds
		FVector2D MinScreenPos( 10000.0f,  10000.0f);
		FVector2D MaxScreenPos(-10000.0f, -10000.0f);
		FVector2D PixelLoc;
		for (INT Index = 0; Index < 8; Index++)
		{
			if (View->WorldToPixel(TransformedPoints[Index], PixelLoc))
			{
				MinScreenPos.X = Min<FLOAT>(PixelLoc.X, MinScreenPos.X);
				MinScreenPos.Y = Min<FLOAT>(PixelLoc.Y, MinScreenPos.Y);
				MaxScreenPos.X = Max<FLOAT>(PixelLoc.X, MaxScreenPos.X);
				MaxScreenPos.Y = Max<FLOAT>(PixelLoc.Y, MaxScreenPos.Y);
			}
		}

		// Calculate the percentage of actual screen coverage
		INT Width = (INT)(MaxScreenPos.X - MinScreenPos.X);
		INT Height = (INT)(MaxScreenPos.Y - MinScreenPos.Y);
		FLOAT ExpectedArea = (FLOAT)(Width * Height);
		FLOAT ViewArea = (FLOAT)(View->SizeX * View->SizeY);

		FLOAT IdealRatio = ExpectedArea / ViewArea;

		FLOAT NewLocalCoveragePercentage;
		if (IdealRatio == 0.0f)
		{
			NewLocalCoveragePercentage = 0.0f;
		}
		else
		{
			NewLocalCoveragePercentage = LocalCoveragePercentage / IdealRatio;
		}

		// Clamp it to 0..1 range
		LocalCoveragePercentage = Clamp<FLOAT>(NewLocalCoveragePercentage, 0.0f, 1.0f);

		// Don't 'pop' to a different value if the delta is large
//		FLOAT Diff = (GIsEditor == FALSE) ? (LocalCoveragePercentage - CoveragePercentage) : 0.0f;
		FLOAT Diff = LocalCoveragePercentage - CoveragePercentage;
		if (Abs(Diff) > GEngine->MaxTrackedOcclusionIncrement)
		{
			CoveragePercentage += (Diff >= 0.0f) ? GEngine->TrackedOcclusionStepSize : -GEngine->TrackedOcclusionStepSize;
		}
		else
		{
			CoveragePercentage = LocalCoveragePercentage;
		}

		if (View->Family->ShowFlags & SHOW_Game)
		{
			check(StoredCoverage);
			StoredCoverage->Percentage = CoveragePercentage;
			StoredCoverage->LastSampleTime = State->LastRenderTime;
		}
	}

	return TRUE;
}
