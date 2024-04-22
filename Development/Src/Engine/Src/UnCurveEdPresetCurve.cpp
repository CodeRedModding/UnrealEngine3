/*=============================================================================
	UnCurveEdPresetCurve.cpp: Shader implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

/**
 *	UCurveEdPresetCurve
 */
IMPLEMENT_CLASS(UCurveEdPresetCurve);

UBOOL UCurveEdPresetCurve::StoreCurvePoints(INT CurveIndex, FCurveEdInterface* Distribution)
{
	if (CurveIndex >= Distribution->GetNumSubCurves())
	{
		return FALSE;
	}

	Points.Empty();
	for (INT PointIndex = 0; PointIndex < Distribution->GetNumKeys(); PointIndex++)
	{
		INT NewIndex = Points.AddZeroed();
		FPresetGeneratedPoint* NewPoint = &Points(NewIndex);

		NewPoint->KeyIn		= Distribution->GetKeyIn(PointIndex);
		NewPoint->KeyOut	= Distribution->GetKeyOut(CurveIndex, PointIndex);
		NewPoint->IntepMode	= Distribution->GetKeyInterpMode(PointIndex);
		Distribution->GetTangents(CurveIndex, PointIndex, NewPoint->TangentIn, NewPoint->TangentOut);
	}

	return TRUE;
}
