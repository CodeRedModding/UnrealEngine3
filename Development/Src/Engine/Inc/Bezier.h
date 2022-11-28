/*=============================================================================
	Bezier.h: Support for Bezier curve computations
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#ifndef _HEADER_BEZIER_H_
#define _HEADER_BEZIER_H_

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

class FBezier
{
public:

	// Constructor.
	FBezier()
	{
	}
	virtual ~FBezier()
	{
	}

	// Generates a list of sample points between 2 points.
	//
	// ControlPoints = Array of 4 FVectors (vert1, controlpoint1, controlpoint2, vert2)
	float Evaluate( FVector* ControlPoints, INT InNumPoints, TArray<FVector>* InResult )
	{
		// Path length.
		FLOAT Length = 0.f;

		// var q is the change in t between successive evaluations.
		FLOAT q = 1.f/(InNumPoints-1); // q is dependent on the number of GAPS = POINTS-1

		// recreate the names used in the derivation
		FVector& P0 = ControlPoints[0];
		FVector& P1 = ControlPoints[1];
		FVector& P2 = ControlPoints[2];
		FVector& P3 = ControlPoints[3];

		// coefficients of the cubic polynomial that we're FDing -
		FVector a = P0;
		FVector b = 3*(P1-P0);
		FVector c = 3*(P2-2*P1+P0);
		FVector d = P3-3*P2+3*P1-P0;

		// initial values of the poly and the 3 diffs -
		FVector S  = a;						// the poly value
		FVector U  = b*q + c*q*q + d*q*q*q;	// 1st order diff (quadratic)
		FVector V  = 2*c*q*q + 6*d*q*q*q;	// 2nd order diff (linear)
		FVector W  = 6*d*q*q*q;				// 3rd order diff (constant)

		FVector OldPos = P0;
		InResult->AddItem( P0 );	// first point on the curve is always P0.

		for( INT i = 1 ; i < InNumPoints ; ++i )
		{
			// calculate the next value and update the deltas
			S += U;			// update poly value
			U += V;			// update 1st order diff value
			V += W;			// update 2st order diff value
			// 3rd order diff is constant => no update needed.

			// Update Length.
			Length += FDist( S, OldPos );
			OldPos  = S;

			InResult->AddItem( S );
		}

		// Return path length as experienced in sequence (linear interpolation between points).
		return Length;
	}
};

#endif
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

