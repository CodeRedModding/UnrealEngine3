/*=============================================================================
	SplineEdit.h
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _SPLINE_EDIT_H
#define _SPLINE_EDIT_H

/** Hit proxy used for SplineActor tangent handles */
struct HSplineHandleProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HSplineHandleProxy,HHitProxy);

	/** SplineActor that this is a handle for */
	ASplineActor* SplineActor;
	/** If this is an arrive or leave handle */
	UBOOL bArrive;

	HSplineHandleProxy(ASplineActor* InSplineActor, UBOOL bInArrive): 
	HHitProxy(HPP_UI), 
		SplineActor(InSplineActor),
		bArrive(bInArrive)
	{}

	/** Show cursor as cross when over this handle */
	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}
	
	virtual void Serialize(FArchive& Ar)
	{
		Ar << SplineActor;
	}
};

/** Util to break a spline, given the hit proxy that was clicked on */
void SplineBreakGivenProxy(HSplineProxy* SplineProxy);

/*------------------------------------------------------------------------------
	FEdModeSpline
------------------------------------------------------------------------------*/


/** Editor mode switched to when selecting SplineActors */
class FEdModeSpline : public FEdMode
{
	/** SplineActor that we are currently modifying the tangent of */
	ASplineActor* ModSplineActor;
	/** If we are modifying the arrive or leave handle on this actor */
	UBOOL bModArrive;
	/** How much to scale tangent handles when drawing them */
	FLOAT TangentHandleScale;

public:
	FEdModeSpline();
	virtual ~FEdModeSpline();

	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI);
	virtual UBOOL InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event);
	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);	
	virtual UBOOL AllowWidgetMove();
	virtual void ActorsDuplicatedNotify(TArray<AActor*>& PreDuplicateSelection, TArray<AActor*>& PostDuplicateSelection, UBOOL bOffsetLocations);
};

class AAmbientSoundSpline;

/** State of FEdModeAmbientSoundSpline class - it says which control point of edited spline is selected */
enum EAmbientSoundSplineState
{
	/** none control point is selected */
	SSS_None,
	/** The test point is selected (test point is used only in editor to check The nearest-point-finding algorithm */
	SSS_TestPoint,
	/** The 'position' control point is selected */
	SSS_Point,
	/** The first 'tangent' control point is selected */
	SSS_ArriveTangent,
	/** The second 'tangent' control point is selected */
	SSS_LeaveTangent,
	SSS_RangeOnSpline,
};

enum ESoundRangeOnSpline
{
	SRS_Start,
	SRS_Stop,
};

/**
 * Editor for actors inherited form AmbientSoundSpline. It allows to modify spline curve.
 */
class FEdModeAmbientSoundSpline : public FEdMode
{
	/** currently selected actor */
	AAmbientSoundSpline* ModSplineActor;
	/** What kind of control point is selected */
	EAmbientSoundSplineState State;
	/** index of selected 'knot' (in one knot there are 3 control points) */
	INT PointIndex;

	// For Simple:
	INT SlotIndex;
	ESoundRangeOnSpline RangePoint;
	INT RangePointIndex;
	FVector MovedPointPos;

public:
	FEdModeAmbientSoundSpline()
	{
		ID = EM_AmbientSoundSpline;
		Desc = TEXT("Ambient Sound Spline Editing");
		PointIndex = -1;
		State = SSS_None;
		ModSplineActor = NULL;
	}
	virtual ~FEdModeAmbientSoundSpline()
	{

	}

	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI);
	virtual UBOOL InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event);
	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);	
	/** If the EdMode is handling InputDelta (ie returning true from it), this allows a mode to indicated whether or not the Widget should also move. */
	virtual UBOOL AllowWidgetMove();
};

#endif

