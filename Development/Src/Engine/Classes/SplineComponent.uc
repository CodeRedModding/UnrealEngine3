/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SplineComponent extends PrimitiveComponent
	native(Spline);
	
/** Actual data for spline. Locations and tangents are in world space. */	
var()   InterpCurveVector	        SplineInfo;

/**
 * This is how curvy this spline is.  1.0f is straight and anything below that is curvy!
 * We are doing a simplistic calculate of:  vsize(points) / Length Of Spline 
 **/
var() editconst float 	        	SplineCurviness;

/** Color of spline */
var()   Color                       SplineColor;	

/** Resolution to draw spline at */
var()   float                       SplineDrawRes;

/** Size of arrow on end of spline. If zero, no arrow drawn */
var()   float                       SplineArrowSize;

/** If TRUE, this spline is for whatever reason disabled, and will be drawn in red. */
var()   bool                        bSplineDisabled;

/** Input, distance along curve, output, parameter that puts you there. */
var()    InterpCurveFloat           SplineReparamTable;

cpptext
{
	// UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void UpdateBounds();
	
}	


/** This will update the spline curviness value **/
native function UpdateSplineCurviness();

/** Update the SplineReparamTable */
native function UpdateSplineReparamTable();
/** Returns total length along this spline */
native function float GetSplineLength() const;
/** Given a distance along the length of this spline, return the point in space where this puts you */
native function vector GetLocationAtDistanceAlongSpline(float Distance) const;
/** Given a distance along the length of this spline, return the direction of the spline there. Note, result is non-unit length. */
native function vector GetTangentAtDistanceAlongSpline(float Distance) const;

defaultproperties
{
	SplineDrawRes=0.1
	SplineArrowSize=60.0
	SplineColor=(R=255,B=255,A=255)
}