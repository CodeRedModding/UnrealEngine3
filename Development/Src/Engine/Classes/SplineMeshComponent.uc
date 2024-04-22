/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SplineMeshComponent extends StaticMeshComponent
	native(Mesh);

/** 
 * Structure that holds info about spline, passed to renderer to deform StaticMesh.
 * Also used by Lightmass, so be sure to update Lightmass::FSplineMeshParams and the static lighting code if this changes!
 */
struct native SplineMeshParams
{
	/** Start location of spline, in component space */
	var vector      StartPos;
	/** Start tangent of spline, in component space */
	var vector      StartTangent;
	/** X and Y scale applied to mesh at start of spline */
	var vector2D    StartScale;
	/** Roll around spline applied at start */
	var float       StartRoll;
	/** Starting offset of the mesh from the spline, in component space */
	var vector2D    StartOffset;
		
	/** End location of spline, in component space */
	var vector      EndPos;
	/** End tangent of spline, in component space */
	var vector      EndTangent;
	/** X and Y scale applied to mesh at end of spline */
	var vector2D    EndScale;
	/** Roll around spline applied at end */
	var float       EndRoll;
	/** Ending offset of the mesh from the spline, in component space */
	var vector2D    EndOffset;

};	

/** Spline that is used to deform mesh */
var	    SplineMeshParams    SplineParams;

/** Axis (in component space) that is used to determine X axis for co-ordinates along spline */
var     vector              SplineXDir;

/** If TRUE, will use smooth interpolation (ease in/out) for Scale, Roll, and Offset along this section of spline. If FALSE, uses linear */
var     bool                bSmoothInterpRollScale;

cpptext
{
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void UpdateBounds();

	virtual UBOOL PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags);
	virtual UBOOL LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags);


	/** Allocates an implementation of FStaticLightingMesh that will handle static lighting for this component */
	virtual class FStaticMeshStaticLightingMesh* AllocateStaticLightingMesh(INT LODIndex, const TArray<ULightComponent*>& InRelevantLights);

	/** Calculates the spline transform, including roll, scale, and offset along the spline at a specified distance */
	FMatrix CalcSliceTransform(const USplineComponent* SplineComp, const FLOAT DistanceAlong);
}

defaultproperties
{
    // Temporary hack to avoid self-z-fighting on spline meshes on PC, 
    // Which happens because the shader compiler optimizes the depth only pass differently than the base pass, 
    // Which produces different depth results due to fp imprecision.  
    // This needs to be removed at some point to get rendering performance with spline meshes.
    bUseAsOccluder=FALSE
	SplineXDir=(X=1)
	bUsePrecomputedShadows=TRUE
}