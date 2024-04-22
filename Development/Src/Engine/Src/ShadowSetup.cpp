/*=============================================================================
	ShadowSetup.cpp: Dynamic shadow setup implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#if WITH_REALD
	#include "RealD/RealD.h"
#endif

/**
 * Whether preshadows can be cached as an optimization.  
 * Disabling the caching through this setting is useful when debugging.
 */
UBOOL GCachePreshadows = TRUE;

/**
 * This value specifies how much bounds will be expanded when rendering a cached preshadow (0.15 = 15% larger).
 * Larger values result in more cache hits, but lower resolution and pull more objects into the depth pass.
 */
FLOAT GPreshadowExpandFraction = 0.15f;

/**
  * Extend the near plane for directional light preshadow frustums by this factor
  *  (0.0 = no extension, 1.0 = double the Z length of the frustum)
  *
  * Since vertices beyond the near are clamped to the near plane for directional light preshadow depth rendering,
  * artifacts can occur with any triangles that crosses the near plane.  The worst of these will occur when a long
  * triangle only has one of it's vertices clamped, causing it to change orientation and potentially cut thru the
  * subject, resulting it lighting where there should have been shadow.
  */
FLOAT GDirectionalLightPreshadowNearPlaneExtensionFactor = 1.0f;

/**
 * Helper variables for logging shadow generation and rendering
 */
UBOOL GLogNextFrameShadowGenerationData = FALSE;

/**
 * Helper function to determine fade alpha value for shadows based on resolution. In the below ASCII art (1) is
 * the MinShadowResolution and (2) is the ShadowFadeResolution. Alpha will be 0 below the min resolution and 1
 * above the fade resolution. In between it is going to be an exponential curve with the values between (1) and (2)
 * being normalized in the 0..1 range.
 *
 *  
 *  |    /-------
 *  |  /
 *  |/
 *  1-----2-------
 *
 * @param	MaxUnclampedResolution		Requested resolution, unclamped so it can be below min
 * @param	ShadowFadeResolution		Resolution at which fade begins
 * @param	MinShadowResolution			Minimum resolution of shadow
 *
 * @return	fade value between 0 and 1
 */
FLOAT CalculateShadowFadeAlpha(INT MaxUnclampedResolution, INT ShadowFadeResolution, INT MinShadowResolution)
{
	FLOAT FadeAlpha = 0.0f;
	// Shadow size is above fading resolution.
	if (MaxUnclampedResolution > ShadowFadeResolution)
	{
		FadeAlpha = 1.0f;
	}
	// Shadow size is below fading resolution but above min resolution.
	else if (MaxUnclampedResolution > MinShadowResolution)
	{
		const FLOAT InverseRange = 1.0f / (ShadowFadeResolution - MinShadowResolution);
		const FLOAT FirstFadeValue = appPow(InverseRange, GSystemSettings.ShadowFadeExponent);
		const FLOAT SizeRatio = (FLOAT)(MaxUnclampedResolution - MinShadowResolution) * InverseRange;
		// Rescale the fade alpha to reduce the change between no fading and the first value, which reduces popping with small ShadowFadeExponent's
		FadeAlpha = (appPow(SizeRatio, GSystemSettings.ShadowFadeExponent) - FirstFadeValue) / (1.0f - FirstFadeValue);
	}
	return FadeAlpha;
}

typedef TArray<FVector,TInlineAllocator<8> > FBoundingBoxVertexArray;

/** Stores the indices for an edge of a bounding volume. */
struct FBoxEdge
{
	WORD FirstEdgeIndex;
	WORD SecondEdgeIndex;
	FBoxEdge(WORD InFirst, WORD InSecond) :
		FirstEdgeIndex(InFirst),
		SecondEdgeIndex(InSecond)
	{}
};

typedef TArray<FBoxEdge,TInlineAllocator<12> > FBoundingBoxEdgeArray;

/**
 * Creates an array of vertices and edges for a bounding box.
 * @param Box - The bounding box
 * @param OutVertices - Upon return, the array will contain the vertices of the bounding box.
 * @param OutEdges - Upon return, will contain indices of the edges of the bounding box.
 */
static void GetBoundingBoxVertices(const FBox& Box,FBoundingBoxVertexArray& OutVertices, FBoundingBoxEdgeArray& OutEdges)
{
	OutVertices.Empty(8);
	OutVertices.Add(8);
	for(INT X = 0;X < 2;X++)
	{
		for(INT Y = 0;Y < 2;Y++)
		{
			for(INT Z = 0;Z < 2;Z++)
			{
				OutVertices(X * 4 + Y * 2 + Z) = FVector(
					X ? Box.Min.X : Box.Max.X,
					Y ? Box.Min.Y : Box.Max.Y,
					Z ? Box.Min.Z : Box.Max.Z
					);
			}
		}
	}

	OutEdges.Empty(12);
	OutEdges.Add(12);
	for(WORD X = 0;X < 2;X++)
	{
		WORD BaseIndex = X * 4;
		OutEdges(X * 4 + 0) = FBoxEdge(BaseIndex, BaseIndex + 1);
		OutEdges(X * 4 + 1) = FBoxEdge(BaseIndex + 1, BaseIndex + 3);
		OutEdges(X * 4 + 2) = FBoxEdge(BaseIndex + 3, BaseIndex + 2);
		OutEdges(X * 4 + 3) = FBoxEdge(BaseIndex + 2, BaseIndex);
	}
	for(WORD XEdge = 0;XEdge < 4;XEdge++)
	{
		OutEdges(8 + XEdge) = FBoxEdge(XEdge, XEdge + 4);
	}
}

/**
 * Computes the transform contains a set of bounding box vertices and minimizes the pre-transform volume inside the post-transform clip space.
 * @param ZAxis - The Z axis of the transform.
 * @param Points - The points that represent the bounding volume.
 * @param Edges - The edges of the bounding volume.
 * @param OutAspectRatio - Upon successful return, contains the aspect ratio of the AABB; the ratio of width:height.
 * @param OutTransform - Upon successful return, contains the transform.
 * @return TRUE if it successfully found a non-zero area projection of the bounding points.
 */
static UBOOL GetBestShadowTransform(const FVector& ZAxis,const FBoundingBoxVertexArray& Points, const FBoundingBoxEdgeArray& Edges, FLOAT& OutAspectRatio,FMatrix& OutTransform)
{
	// Find the axis parallel to the edge between any two boundary points with the smallest projection of the bounds onto the axis.
	FVector XAxis(0,0,0);
	FVector YAxis(0,0,0);
	FVector Translation(0,0,0);
	FLOAT BestProjectedExtent = FLT_MAX;
	UBOOL bValidProjection = FALSE;

	// Cache unaliased pointers to point and edge data
	const FVector* RESTRICT PointsPtr = Points.GetData();
	const FBoxEdge* RESTRICT EdgesPtr = Edges.GetData();

	const INT NumPoints = Points.Num();
	const INT NumEdges = Edges.Num();

	// We're always dealing with box geometry here, so we can hint the compiler
	ASSUME( NumPoints == 8 );
	ASSUME( NumEdges == 12 );

	for(INT EdgeIndex = 0;EdgeIndex < NumEdges; ++EdgeIndex)
	{
		const FVector Point = PointsPtr[EdgesPtr[EdgeIndex].FirstEdgeIndex];
		const FVector OtherPoint = PointsPtr[EdgesPtr[EdgeIndex].SecondEdgeIndex];
		const FVector PointDelta = OtherPoint - Point;
		const FVector TrialXAxis = (PointDelta - ZAxis * (PointDelta | ZAxis)).SafeNormal();
		const FVector TrialYAxis = (ZAxis ^ TrialXAxis).SafeNormal();

		// Calculate the size of the projection of the bounds onto this axis and an axis orthogonal to it and the Z axis.
		FLOAT MinProjectedX = FLT_MAX;
		FLOAT MaxProjectedX = -FLT_MAX;
		FLOAT MinProjectedY = FLT_MAX;
		FLOAT MaxProjectedY = -FLT_MAX;
		for(INT ProjectedPointIndex = 0;ProjectedPointIndex < NumPoints; ++ProjectedPointIndex)
		{
			const FLOAT ProjectedX = PointsPtr[ProjectedPointIndex] | TrialXAxis;
			MinProjectedX = Min(MinProjectedX,ProjectedX);
			MaxProjectedX = Max(MaxProjectedX,ProjectedX);
			const FLOAT ProjectedY = PointsPtr[ProjectedPointIndex] | TrialYAxis;
			MinProjectedY = Min(MinProjectedY,ProjectedY);
			MaxProjectedY = Max(MaxProjectedY,ProjectedY);
		}

		FLOAT ProjectedExtentX;
		FLOAT ProjectedExtentY;
		if (GSystemSettings.bUseConservativeShadowBounds)
		{
			ProjectedExtentX = 2 * Max(Abs(MaxProjectedX), Abs(MinProjectedX));
			ProjectedExtentY = 2 * Max(Abs(MaxProjectedY), Abs(MinProjectedY));
		}
		else
		{
			ProjectedExtentX = MaxProjectedX - MinProjectedX;
			ProjectedExtentY = MaxProjectedY - MinProjectedY;
		}

		const FLOAT ProjectedExtent = ProjectedExtentX * ProjectedExtentY;
		if(ProjectedExtent < BestProjectedExtent - .05f 
			// Only allow projections with non-zero area
			&& ProjectedExtent > DELTA)
		{
			bValidProjection = TRUE;
			BestProjectedExtent = ProjectedExtent;
			XAxis = TrialXAxis * 2.0f / ProjectedExtentX;
			YAxis = TrialYAxis * 2.0f / ProjectedExtentY;

			// Translating in post-transform clip space can cause the corners of the world space bounds to be outside of the transform generated by this function
			// This usually manifests in cinematics where the character's head is near the top of the bounds
			if (!GSystemSettings.bUseConservativeShadowBounds)
			{
				Translation.X = (MinProjectedX + MaxProjectedX) * 0.5f;
				Translation.Y = (MinProjectedY + MaxProjectedY) * 0.5f;
			}

			if(ProjectedExtentY > ProjectedExtentX)
			{
				// Always make the X axis the largest one.
				Exchange(XAxis,YAxis);
				Exchange(Translation.X,Translation.Y);
				XAxis *= -1.0f;
				Translation.X *= -1.0f;
				OutAspectRatio = ProjectedExtentY / ProjectedExtentX;
			}
			else
			{
				OutAspectRatio = ProjectedExtentX / ProjectedExtentY;
			}
		}
	}

	// Only create the shadow if the projected extent of the given points has a non-zero area.
	if(bValidProjection && BestProjectedExtent > DELTA)
	{
		OutTransform = FBasisVectorMatrix(XAxis,YAxis,ZAxis,FVector(0,0,0)) * FTranslationMatrix(Translation);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/** A transform the remaps depth and potentially projects onto some plane. */
struct FShadowProjectionMatrix: FMatrix
{
	FShadowProjectionMatrix(FLOAT MinZ,FLOAT MaxZ,const FVector4& WAxis):
		FMatrix(
			FPlane(1,	0,	0,													WAxis.X),
			FPlane(0,	1,	0,													WAxis.Y),
			FPlane(0,	0,	(WAxis.Z * MaxZ + WAxis.W) / (MaxZ - MinZ),			WAxis.Z),
			FPlane(0,	0,	-MinZ * (WAxis.Z * MaxZ + WAxis.W) / (MaxZ - MinZ),	WAxis.W)
			)
	{}
};

UBOOL FProjectedShadowInitializer::CalcObjectShadowTransforms(
	const FVector& InPreShadowTranslation,
	const FMatrix& WorldToLight,
	const FVector& FaceDirection,
	const FBoxSphereBounds& SubjectBounds,
	const FVector4& WAxis,
	FLOAT MinLightW,
	FLOAT MaxDistanceToCastInLightW,
	UBOOL bInDirectionalLight
	)
{
	PreShadowTranslation = InPreShadowTranslation;
	bDirectionalLight = bInDirectionalLight;
	bFullSceneShadow = FALSE;
	SplitIndex = INDEX_NONE;
	BoundingRadius = SubjectBounds.SphereRadius;

	// Create an array of the extreme vertices of the subject's bounds.
	FBoundingBoxVertexArray BoundsPoints;
	FBoundingBoxEdgeArray BoundsEdges;
	GetBoundingBoxVertices(SubjectBounds.GetBox(),BoundsPoints,BoundsEdges);

	// Project the bounding box vertices.
	FBoundingBoxVertexArray ProjectedBoundsPoints;
	for(INT PointIndex = 0;PointIndex < BoundsPoints.Num();PointIndex++)
	{
		const FVector TransformedBoundsPoint = WorldToLight.TransformFVector(BoundsPoints(PointIndex));
		const FLOAT TransformedBoundsPointW = Dot4(FVector4(0,0,TransformedBoundsPoint | FaceDirection,1),WAxis);
		if(TransformedBoundsPointW >= DELTA)
		{
			ProjectedBoundsPoints.AddItem(TransformedBoundsPoint / TransformedBoundsPointW);
		}
		else
		{
			ProjectedBoundsPoints.AddItem(FVector(FLT_MAX, FLT_MAX, FLT_MAX));
		}
	}

	// Compute the transform from light-space to shadow-space.
	FMatrix LightToShadow;
	if(GetBestShadowTransform(FaceDirection.SafeNormal(),ProjectedBoundsPoints,BoundsEdges,AspectRatio,LightToShadow))
	{
		const FMatrix WorldToShadow = WorldToLight * LightToShadow;

		const FBox ShadowSubjectBounds = SubjectBounds.GetBox().TransformBy(WorldToShadow);
		const FLOAT ClampedMinSubjectZ = Max(MinLightW, ShadowSubjectBounds.Min.Z);
		const FLOAT ClampedMaxSubjectZ = Min(ClampedMinSubjectZ + MaxDistanceToCastInLightW, (FLOAT)HALF_WORLD_MAX);

		PreSubjectMatrix = WorldToShadow * FShadowProjectionMatrix(MinLightW,ShadowSubjectBounds.Max.Z,WAxis);
		SubjectMatrix = WorldToShadow * FShadowProjectionMatrix(ClampedMinSubjectZ,ShadowSubjectBounds.Max.Z,WAxis);
		PostSubjectMatrix = WorldToShadow * FShadowProjectionMatrix(ClampedMinSubjectZ, ClampedMaxSubjectZ, WAxis);

		MinPreSubjectZ = MinLightW;
		MaxSubjectDepth = SubjectBounds.GetBox().TransformBy(SubjectMatrix).Max.Z;
		MaxPreSubjectDepth = bDirectionalLight ? MaxSubjectDepth : SubjectBounds.GetBox().TransformBy(PreSubjectMatrix).Max.Z;

		if (bDirectionalLight)
		{
			const FLOAT ZLength = ShadowSubjectBounds.Max.Z - ShadowSubjectBounds.Min.Z;
			const FLOAT PushedOutClampedMinSubjectZ = Max(MinLightW, ShadowSubjectBounds.Min.Z - ZLength * GDirectionalLightPreshadowNearPlaneExtensionFactor);

			SubjectMatrixFudged = WorldToShadow * FShadowProjectionMatrix(PushedOutClampedMinSubjectZ, ShadowSubjectBounds.Max.Z, WAxis);
		}

		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

UBOOL FProjectedShadowInitializer::CalcWholeSceneShadowTransforms(
	const FVector& InPreShadowTranslation,
	const FMatrix& WorldToLight,
	const FVector& FaceDirection,
	const FBoxSphereBounds& SubjectBounds,
	const FVector4& WAxis,
	FLOAT MinLightW,
	FLOAT MaxDistanceToCastInLightW,
	UBOOL bInDirectionalLight,
	UBOOL bInOnePassPointLight,
	INT InSplitIndex
	)
{
	PreShadowTranslation = InPreShadowTranslation;
	bDirectionalLight = bInDirectionalLight;
	bFullSceneShadow = TRUE;
	SplitIndex = InSplitIndex;
	BoundingRadius = SubjectBounds.SphereRadius;

	FVector	XAxis, YAxis;
	FaceDirection.FindBestAxisVectors(XAxis,YAxis);
	const FMatrix WorldToFace = WorldToLight * FBasisVectorMatrix(-XAxis,YAxis,FaceDirection.SafeNormal(),FVector(0,0,0));

	const FLOAT MaxSubjectZ = WorldToFace.TransformFVector(SubjectBounds.Origin).Z + SubjectBounds.SphereRadius;
	const FLOAT MinSubjectZ = Max(MaxSubjectZ - SubjectBounds.SphereRadius * 2,MinLightW);
	const FLOAT ClampedMaxLightW = Min(MinSubjectZ + MaxDistanceToCastInLightW, (FLOAT)HALF_WORLD_MAX);

	MinPreSubjectZ = MinLightW;
	PreSubjectMatrix = WorldToFace * FShadowProjectionMatrix(MinLightW,MaxSubjectZ,WAxis);
	SubjectMatrix = WorldToFace * FShadowProjectionMatrix(MinSubjectZ,MaxSubjectZ,WAxis);

	if (bInDirectionalLight)
	{
		const FIntPoint ShadowBufferResolution = GSceneRenderTargets.GetShadowDepthTextureResolution(InSplitIndex == 0);
		const UINT ShadowDepthBufferSizeX = ShadowBufferResolution.X - SHADOW_BORDER * 2;
		const UINT ShadowDepthBufferSizeY = ShadowBufferResolution.Y - SHADOW_BORDER * 2;
		// Transform the shadow's position into shadowmap space
		const FVector TransformedPosition = WorldToFace.TransformFVector(-PreShadowTranslation);
		// Determine the distance necessary to snap the shadow's position to the nearest texel
		const FLOAT SnapX = appFmod(TransformedPosition.X, 2.0f / ShadowDepthBufferSizeX);
		const FLOAT SnapY = appFmod(TransformedPosition.Y, 2.0f / ShadowDepthBufferSizeY);
		// Snap the shadow's position and transform it back into world space
		// This snapping prevents sub-texel camera movements which removes view dependent aliasing from the final shadow result
		// This only maintains stable shadows under camera translation and rotation
		const FVector SnappedWorldPosition = WorldToFace.Inverse().TransformFVector(TransformedPosition - FVector(SnapX, SnapY, 0.0f));
		PreShadowTranslation = -SnappedWorldPosition;
	}
	
	PostSubjectMatrix = WorldToFace * FShadowProjectionMatrix(MinSubjectZ, ClampedMaxLightW, WAxis);

	MaxSubjectDepth = SubjectMatrix.TransformFVector(SubjectBounds.Origin + WorldToLight.Inverse().TransformNormal(FaceDirection) * SubjectBounds.SphereRadius).Z;
	MaxPreSubjectDepth = PreSubjectMatrix.TransformFVector(SubjectBounds.Origin + WorldToLight.Inverse().TransformNormal(FaceDirection) * SubjectBounds.SphereRadius).Z;

	if (bInOnePassPointLight)
	{
		MaxSubjectDepth = SubjectBounds.SphereRadius;
	}

	AspectRatio = 1.0f;
	return TRUE;
}

FProjectedShadowInfo::FProjectedShadowInfo(
	FLightSceneInfo* InLightSceneInfo,
	const FPrimitiveSceneInfo* InParentSceneInfo,
	const FLightPrimitiveInteraction* const InParentInteraction,
	const FProjectedShadowInitializer& Initializer,
	UBOOL bInPreShadow,
	UINT InResolutionX,
	UINT InResolutionY,
	FLOAT InMaxScreenPercent,
	const TArray<FLOAT, TInlineAllocator<2> >& InFadeAlphas
	):
	LightSceneInfo(InLightSceneInfo),
	LightSceneInfoCompact(InLightSceneInfo),
	ParentSceneInfo(InParentSceneInfo),
	ParentInteraction(InParentInteraction),
	DependentView(NULL),
	ShadowId(INDEX_NONE),
	PreShadowTranslation(Initializer.PreShadowTranslation),
	MinPreSubjectZ(Initializer.MinPreSubjectZ),
	ShadowBounds(-Initializer.PreShadowTranslation, Initializer.BoundingRadius),
	ResolutionX(InResolutionX),
	ResolutionY(InResolutionY),
	MaxScreenPercent(InMaxScreenPercent),
	FadeAlphas(InFadeAlphas),
	SplitIndex(Initializer.SplitIndex),
	bAllocated(FALSE),
	bRendered(FALSE),
	bAllocatedInPreshadowCache(FALSE),
	bDepthsCached(FALSE),
	bDirectionalLight(Initializer.bDirectionalLight),
	bFullSceneShadow(Initializer.bFullSceneShadow),
	bPreShadow(bInPreShadow),
	bForegroundCastingOnWorld(FALSE),
	bSelfShadowOnly((InParentSceneInfo->bSelfShadowOnly || InLightSceneInfo->bSelfShadowOnly) && !(Initializer.bFullSceneShadow || bInPreShadow))
{
	if(bPreShadow)
	{
		// Presize the static mesh elements array since preshadows usually have quite a few
		SubjectMeshElements.Empty(60);
		ReceiverMatrix = Initializer.SubjectMatrix;
		// The PreSubjectMatrix for directional lights starts at the other side of the world, so shadow depths get very poor precision.
		// This shows up as lack of shadowing when an object is close to the caster.
		// As a workaround, use the SubjectMatrix when rendering preshadows for directional lights, and clamp vertices to the near plane so they don't get clipped.
		// This breaks down and causes artifacts with poorly tessellated meshes like BSP unfortunately.
		SubjectAndReceiverMatrix = bDirectionalLight ? Initializer.SubjectMatrixFudged : Initializer.PreSubjectMatrix;
		MaxSubjectDepth = Initializer.MaxPreSubjectDepth;
		GetViewFrustumBounds(SubjectAndReceiverFrustum,Initializer.PreSubjectMatrix,TRUE);
	}
	else
	{
		ReceiverMatrix = Initializer.PostSubjectMatrix;
		SubjectAndReceiverMatrix = Initializer.SubjectMatrix;
		MaxSubjectDepth = Initializer.MaxSubjectDepth;
		GetViewFrustumBounds(SubjectAndReceiverFrustum,SubjectAndReceiverMatrix,TRUE);
	}

	InvReceiverMatrix = ReceiverMatrix.Inverse();
	GetViewFrustumBounds(ReceiverFrustum,ReceiverMatrix,TRUE);
}

FProjectedShadowInfo::FProjectedShadowInfo(
	FLightSceneInfo* InLightSceneInfo,
	const FViewInfo* InDependentView,
	const FProjectedShadowInitializer& Initializer,
	UINT InResolutionX,
	UINT InResolutionY,
	const TArray<FLOAT, TInlineAllocator<2> >& InFadeAlphas
	):
	LightSceneInfo(InLightSceneInfo),
	LightSceneInfoCompact(InLightSceneInfo),
	ParentSceneInfo(NULL),
	ParentInteraction(NULL),
	DependentView(InDependentView),
	ShadowId(INDEX_NONE),
	PreShadowTranslation(Initializer.PreShadowTranslation),
	SubjectAndReceiverMatrix(Initializer.SubjectMatrix),
	MaxSubjectDepth(Initializer.MaxSubjectDepth),
	MinPreSubjectZ(Initializer.MinPreSubjectZ),
	ResolutionX(InResolutionX),
	ResolutionY(InResolutionY),
	MaxScreenPercent(1.0f),
	FadeAlphas(InFadeAlphas),
	SplitIndex(Initializer.SplitIndex),
	bAllocated(FALSE),
	bRendered(FALSE),
	bAllocatedInPreshadowCache(FALSE),
	bDepthsCached(FALSE),
	bDirectionalLight(Initializer.bDirectionalLight),
	bFullSceneShadow(Initializer.bFullSceneShadow),
	bPreShadow(FALSE),
	bForegroundCastingOnWorld(FALSE),
	bSelfShadowOnly(FALSE)
{
	ReceiverMatrix = Initializer.PostSubjectMatrix;
	InvReceiverMatrix = ReceiverMatrix.Inverse();

	if (SplitIndex >= 0 && bDirectionalLight)
	{
		checkSlow(InDependentView);
		ShadowBounds = InLightSceneInfo->GetShadowSplitBounds(*InDependentView, SplitIndex);
	}
	else
	{
		ShadowBounds = FSphere(-Initializer.PreShadowTranslation, Initializer.BoundingRadius);
	}
	
	GetViewFrustumBounds(PreSubjectFrustum,Initializer.PreSubjectMatrix,TRUE);
	GetViewFrustumBounds(SubjectAndReceiverFrustum,SubjectAndReceiverMatrix,TRUE);
	GetViewFrustumBounds(ReceiverFrustum,ReceiverMatrix,TRUE);
}

/**
 * Adds a primitive to the whole scene shadow's subject list.
 */
void FProjectedShadowInfo::AddWholeSceneSubjectPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	checkSlow(LightSceneInfo->Scene->NumWholeSceneShadowLights > 0 || GIsEditor);

	UBOOL bDrawingStaticMeshes = FALSE;
	if (PrimitiveSceneInfo->StaticMeshes.Num() > 0)
	{
		checkSlow(DependentView);
		const INT PrimitiveId = PrimitiveSceneInfo->Id;
		// Update visibility for meshes which weren't visible in the main views or were visible with static relevance
		if (!DependentView->PrimitiveVisibilityMap(PrimitiveId) || DependentView->PrimitiveViewRelevanceMap(PrimitiveId).bStaticRelevance)
		{
			UBOOL bUseExistingVisibility = FALSE;
			for (INT MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshes.Num(); MeshIndex++)
			{
				const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes(MeshIndex);
				if (DependentView->StaticMeshShadowDepthMap(StaticMesh.Id))
				{
					bUseExistingVisibility = TRUE;
					StaticMeshWholeSceneShadowDepthMap(StaticMesh.Id) = TRUE;
				}
			}

			if (bUseExistingVisibility)
			{
				bDrawingStaticMeshes = TRUE;
			}
			// Don't overwrite visibility set by the main views
			// This is necessary to avoid popping when transitioning between LODs, because on the frame of the transition, 
			// The old LOD will continue to be drawn even though a different LOD would be chosen by distance.
			else
			{
				FLOAT DistanceSquared = 0.0f;
				if (DependentView->ViewOrigin.W > 0.0f)
				{	
					// Calculate LOD in the same way it is done for other static mesh draw lists
					DistanceSquared = CalculateDistanceSquaredForLOD(PrimitiveSceneInfo->Bounds, DependentView->ViewOrigin);
				}
				DistanceSquared *= Square(DependentView->LODDistanceFactor);

				// Add the primitive's static mesh elements to the draw lists.
				for (INT MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshes.Num(); MeshIndex++)
				{
					const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes(MeshIndex);
					const FLOAT AdjustedMinDrawDistanceSquared = StaticMesh.MinDrawDistanceSquared * (StaticMesh.LODIndex == 0 ? 1.0f : Square(GSystemSettings.MaxDrawDistanceScale));
					const FLOAT AdjustedMaxDrawDistanceSquared = StaticMesh.MaxDrawDistanceSquared * Square(GSystemSettings.MaxDrawDistanceScale);
					if (StaticMesh.CastShadow 
						&& DistanceSquared >= AdjustedMinDrawDistanceSquared
						&& DistanceSquared <  AdjustedMaxDrawDistanceSquared)
					{
						StaticMeshWholeSceneShadowDepthMap(StaticMesh.Id) = TRUE;
						bDrawingStaticMeshes = TRUE;
					}
				}
			}
		}
	}

	if (!bDrawingStaticMeshes)
	{
		SubjectPrimitives.AddItem(PrimitiveSceneInfo);
	}
}

void FProjectedShadowInfo::AddSubjectPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo, TArray<FViewInfo>& Views)
{
	if (!ReceiverPrimitives.ContainsItem(PrimitiveSceneInfo))
	{
		// Logical or together all the subject's bSelfShadowOnly options
		// If any of the subjects have the option, the shadow will be rendered with it
		bSelfShadowOnly = bSelfShadowOnly || PrimitiveSceneInfo->bSelfShadowOnly && !(bPreShadow || bFullSceneShadow);

		UBOOL bDrawingStaticMeshes = FALSE;
		if (PrimitiveSceneInfo->StaticMeshes.Num() > 0)
		{
			for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				FViewInfo& CurrentView = Views(ViewIndex);
				if (DependentView && DependentView != &CurrentView)
				{
					continue;
				}
				const INT PrimitiveId = PrimitiveSceneInfo->Id;
				// Update visibility for meshes which weren't visible in the main views or were visible with static relevance
				if (!CurrentView.PrimitiveVisibilityMap(PrimitiveId) || CurrentView.PrimitiveViewRelevanceMap(PrimitiveId).bStaticRelevance)
				{
					UBOOL bUseExistingVisibility = FALSE;
					for (INT MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshes.Num() && !bUseExistingVisibility; MeshIndex++)
					{
						const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes(MeshIndex);
						bUseExistingVisibility = bUseExistingVisibility || CurrentView.StaticMeshShadowDepthMap(StaticMesh.Id) && StaticMesh.CastShadow;
					}

					if (bUseExistingVisibility)
					{
						bDrawingStaticMeshes = TRUE;
					}
					// Don't overwrite visibility set by the main views
					// This is necessary to avoid popping when transitioning between LODs, because on the frame of the transition, 
					// The old LOD will continue to be drawn even though a different LOD would be chosen by distance.
					else
					{
						FLOAT DistanceSquared = 0.0f;
						if (CurrentView.ViewOrigin.W > 0.0f)
						{	
							// Calculate LOD in the same way it is done for other static mesh draw lists
							DistanceSquared = CalculateDistanceSquaredForLOD(PrimitiveSceneInfo->Bounds, CurrentView.ViewOrigin);
						}
						DistanceSquared *= Square(CurrentView.LODDistanceFactor);

						// Add the primitive's static mesh elements to the draw lists.
						for (INT MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshes.Num(); MeshIndex++)
						{
							const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes(MeshIndex);
							const FLOAT AdjustedMinDrawDistanceSquared = StaticMesh.MinDrawDistanceSquared * (StaticMesh.LODIndex == 0 ? 1.0f : Square(GSystemSettings.MaxDrawDistanceScale));
							const FLOAT AdjustedMaxDrawDistanceSquared = StaticMesh.MaxDrawDistanceSquared * Square(GSystemSettings.MaxDrawDistanceScale);
							if (StaticMesh.CastShadow 
								&& DistanceSquared >= AdjustedMinDrawDistanceSquared 
								&& DistanceSquared <  AdjustedMaxDrawDistanceSquared)
							{
								CurrentView.StaticMeshShadowDepthMap(StaticMesh.Id) = TRUE;
								bDrawingStaticMeshes = TRUE;
							}
						}
					}
				}
			}
		}

		if (bDrawingStaticMeshes)
		{
			// Add the primitive's static mesh elements to the draw lists.
			for (INT MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshes.Num(); MeshIndex++)
			{
				FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes(MeshIndex);
				if (StaticMesh.CastShadow)
				{
					const FMaterialRenderProxy* MaterialRenderProxy = StaticMesh.MaterialRenderProxy;
					const FMaterial* Material = MaterialRenderProxy->GetMaterial();
					const EBlendMode BlendMode = Material->GetBlendMode();

					if (!IsTranslucentBlendMode(BlendMode) || Material->CastLitTranslucencyShadowAsMasked())
					{
						if (!Material->IsMasked() && !Material->IsTwoSided() && !Material->CastLitTranslucencyShadowAsMasked() && !Material->MaterialModifiesMeshPosition())
						{
							// Override with the default material for opaque materials that are not two sided
							MaterialRenderProxy = GEngine->DefaultMaterial->GetRenderProxy(FALSE);
						}

						SubjectMeshElements.AddItem(FShadowStaticMeshElement(MaterialRenderProxy, &StaticMesh));
					}
				}
			}
		}
		else
		{
			// Add the primitive to the subject primitive list.
			SubjectPrimitives.AddItem(PrimitiveSceneInfo);
		}
	}
}

void FProjectedShadowInfo::AddReceiverPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	// Add the primitive to the receiver primitive list.
	ReceiverPrimitives.AddItem(PrimitiveSceneInfo);
}

/**
 * @return TRUE if this shadow info has any casting subject prims to render
 */
UBOOL FProjectedShadowInfo::HasSubjectPrims() const
{
	checkSlow(bPreShadow);
	return( SubjectPrimitives.Num() > 0 || SubjectMeshElements.Num() > 0 );
}

/** 
 * @param View view to check visibility in
 * @return TRUE if this shadow info has any subject prims visible in the view
 */
UBOOL FProjectedShadowInfo::SubjectsVisible(const FViewInfo& View) const
{
	checkSlow(!IsWholeSceneDominantShadow());
	for(INT PrimitiveIndex = 0;PrimitiveIndex < SubjectPrimitives.Num();PrimitiveIndex++)
	{
		const FPrimitiveSceneInfo* SubjectPrimitiveSceneInfo = SubjectPrimitives(PrimitiveIndex);
		if(View.PrimitiveVisibilityMap(SubjectPrimitiveSceneInfo->Id))
		{
			return TRUE;
		}
	}
	return FALSE;
}

/** Clears arrays allocated with the scene rendering allocator. */
void FProjectedShadowInfo::ClearTransientArrays()
{
	SubjectPrimitives.Empty();
	ReceiverPrimitives.Empty();
	SubjectMeshElements.Empty();
}

/** Returns a cached preshadow matching the input criteria if one exists. */
TRefCountPtr<FProjectedShadowInfo> FSceneRenderer::GetCachedPreshadow(
	const FLightPrimitiveInteraction* InParentInteraction, 
	const FProjectedShadowInitializer& Initializer,
	const FBoxSphereBounds& Bounds,
	UINT InResolutionX,
	UINT InResolutionY)
{
	if (GCachePreshadows 
		&& !bIsSceneCapture 
		// Only allow caching when translucent and opaque preshadows will use the same depth format,
		// Since only one preshadow is cached for both.
		&& !(GSceneRenderTargets.IsFetch4Supported() || GSceneRenderTargets.IsHardwarePCFSupported()))
	{
		const FPrimitiveSceneInfo* PrimitiveInfo = InParentInteraction->GetPrimitiveSceneInfo();
		const FLightSceneInfo* LightInfo = InParentInteraction->GetLight();
		const FSphere QueryBounds(Bounds.Origin, Bounds.SphereRadius);
		for (INT ShadowIndex = 0; ShadowIndex < Scene->CachedPreshadows.Num(); ShadowIndex++)
		{
			TRefCountPtr<FProjectedShadowInfo> CachedShadow = Scene->CachedPreshadows(ShadowIndex);
			// Only reuse a cached preshadow if it was created for the same primitive and light
			if (CachedShadow->ParentSceneInfo == PrimitiveInfo
				&& CachedShadow->LightSceneInfo == LightInfo
				// Only reuse if it contains the bounds being queried
				&& QueryBounds.IsInside(CachedShadow->ShadowBounds, CachedShadow->ShadowBounds.W * .04f)  // Is QueryBounds inside 96% of cached shadow bounds?
				// Only reuse if the resolution matches
				&& CachedShadow->ResolutionX == InResolutionX
				&& CachedShadow->ResolutionY == InResolutionY)
			{
				// Reset any allocations using the scene rendering allocator, 
				// Since those will point to freed memory now that we are using the shadow on a different frame than it was created on.
				CachedShadow->ClearTransientArrays();
				return CachedShadow;
			}
		}
	}
	// No matching cached preshadow was found
	return NULL;
}

class FComparePreshadows
{
public:
	/** Sorts shadows by resolution, largest to smallest. */
	static inline INT Compare(const TRefCountPtr<FProjectedShadowInfo>& A, const TRefCountPtr<FProjectedShadowInfo>&  B)
	{
		return (B->ResolutionX * B->ResolutionY - A->ResolutionX * A->ResolutionY);
	}
};

/** Removes stale shadows and attempts to add new preshadows to the cache. */
void FSceneRenderer::UpdatePreshadowCache()
{
	if (GCachePreshadows
		&& !bIsSceneCapture
		// Only allow caching when translucent and opaque preshadows will use the same depth format,
		// Since only one preshadow is cached for both.
		&& !(GSceneRenderTargets.IsFetch4Supported() || GSceneRenderTargets.IsHardwarePCFSupported()))
	{
		if (Scene->PreshadowCacheLayout.GetSizeX() == 0)
		{
			// Initialize the texture layout if necessary
			const FIntPoint PreshadowCacheBufferSize = GSceneRenderTargets.GetPreshadowCacheTextureResolution();
			Scene->PreshadowCacheLayout = FTextureLayout(1, 1, PreshadowCacheBufferSize.X, PreshadowCacheBufferSize.Y, FALSE, FALSE);
		}

		// Iterate through the cached preshadows, removing those that are not going to be rendered this frame
		for (INT CachedShadowIndex = Scene->CachedPreshadows.Num() - 1; CachedShadowIndex >= 0; CachedShadowIndex--)
		{
			TRefCountPtr<FProjectedShadowInfo> CachedShadow = Scene->CachedPreshadows(CachedShadowIndex);

			UBOOL bShadowBeingRenderedThisFrame = FALSE;
			for (INT LightIndex = 0; LightIndex < VisibleLightInfos.Num() && !bShadowBeingRenderedThisFrame; LightIndex++)
			{
				bShadowBeingRenderedThisFrame = VisibleLightInfos(LightIndex).ProjectedPreShadows.FindItemIndex(CachedShadow) != INDEX_NONE;
			}

			if (!bShadowBeingRenderedThisFrame)
			{
				// Must succeed, since we added it to the layout earlier
				verify(Scene->PreshadowCacheLayout.RemoveElement(
					CachedShadow->X,
					CachedShadow->Y,
					CachedShadow->ResolutionX + SHADOW_BORDER * 2,
					CachedShadow->ResolutionY + SHADOW_BORDER * 2));
				Scene->CachedPreshadows.Remove(CachedShadowIndex);
			}
		}

		TArray<TRefCountPtr<FProjectedShadowInfo>, SceneRenderingAllocator> UncachedPreShadows;

		// Gather a list of preshadows that can be cached
		for (INT LightIndex = 0; LightIndex < VisibleLightInfos.Num(); LightIndex++)
		{
			for (INT ShadowIndex = 0; ShadowIndex < VisibleLightInfos(LightIndex).ProjectedPreShadows.Num(); ShadowIndex++)
			{
				TRefCountPtr<FProjectedShadowInfo> CurrentShadow = VisibleLightInfos(LightIndex).ProjectedPreShadows(ShadowIndex);
				checkSlow(CurrentShadow->bPreShadow);

				if (!CurrentShadow->bAllocatedInPreshadowCache)
				{
					UncachedPreShadows.AddItem(CurrentShadow);
				}
			}
		}

		// Sort them from largest to smallest, based on the assumption that larger preshadows will have more objects in their depth only pass
		//@todo - weight preshadows used by both opaque and translucent materials has higher priority
		Sort<TRefCountPtr<FProjectedShadowInfo>, FComparePreshadows>(&UncachedPreShadows(0),UncachedPreShadows.Num());

		for (INT ShadowIndex = 0; ShadowIndex < UncachedPreShadows.Num(); ShadowIndex++)
		{
			TRefCountPtr<FProjectedShadowInfo> CurrentShadow = UncachedPreShadows(ShadowIndex);

			// Try to find space for the preshadow in the texture layout
			if (Scene->PreshadowCacheLayout.AddElement(
					CurrentShadow->X,
					CurrentShadow->Y,
					CurrentShadow->ResolutionX + SHADOW_BORDER * 2,
					CurrentShadow->ResolutionY + SHADOW_BORDER * 2))
			{
#if XBOX
				check( ((CurrentShadow->X % D3DRESOLVEALIGNMENT) == 0) &&
					((CurrentShadow->Y % D3DRESOLVEALIGNMENT) == 0) &&
					(((CurrentShadow->ResolutionX + SHADOW_BORDER * 2) & 31) == 0) &&
					(((CurrentShadow->ResolutionY + SHADOW_BORDER * 2) & 31) == 0) );
#endif

				// Mark the preshadow as existing in the cache
				// It must now use the preshadow cache render target to render and read its depths instead of the usual shadow depth buffers
				CurrentShadow->bAllocatedInPreshadowCache = TRUE;
				// Indicate that the shadow's X and Y have been initialized
				CurrentShadow->bAllocated = TRUE;
				Scene->CachedPreshadows.AddItem(CurrentShadow);
			}
		}
	}
}

/**
 * Generates recursive list of child scene infos
 * @param InPrimitiveSceneInfo - Root of the shadow tree
 * @param OutChildSceneInfos - List of all child scene infos connected to the input scene info for shadowing
 */
void FSceneRenderer::GenerateChildSceneInfos(const FPrimitiveSceneInfo* InPrimitiveSceneInfo, UBOOL bEditor, TArray <FPrimitiveSceneInfo*, SceneRenderingAllocator>& OutChildSceneInfos) const
{
	// Check if this primitive is the parent of a shadow group.
	FShadowGroupSceneInfo* ShadowGroup = Scene->ShadowGroups.Find(InPrimitiveSceneInfo->Component);

	//find all child scene infos
	if(ShadowGroup)
	{
		for(INT ChildIndex = 0;ChildIndex < ShadowGroup->Primitives.Num();ChildIndex++)
		{
			FPrimitiveSceneInfo* ShadowChild = ShadowGroup->Primitives(ChildIndex);

#if !FINAL_RELEASE
			if (OutChildSceneInfos.ContainsItem(ShadowChild))
			{
				appErrorf(NAME_Error, TEXT("CreateProjectedShadow has a circular list!"));
			}
#endif

			// Apply hidden logic to shadow parented primitives
			//@todo - what about the other conditions in FPrimitiveSceneProxy::IsShadowCast?
			if (bEditor && !ShadowChild->Proxy->IsHiddenEditor()
				|| !bEditor && !ShadowChild->Proxy->IsHiddenGame()
				|| ShadowChild->bCastHiddenShadow)
			{
				//append the new child to the list
				OutChildSceneInfos.AddItem(ShadowChild);
			}
			
			//recurse to the next level in the tree
			GenerateChildSceneInfos(ShadowChild, bEditor, OutChildSceneInfos);
		}
	}
}


FProjectedShadowInfo* FSceneRenderer::CreateProjectedShadow(
	FLightPrimitiveInteraction* Interaction,
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& OutPreShadows,
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ViewDependentWholeSceneShadows
	)
{
	const FPrimitiveSceneInfo* PrimitiveSceneInfo = Interaction->GetPrimitiveSceneInfo();
	FLightSceneInfo* LightSceneInfo = Interaction->GetLight();
	FProjectedShadowInfo* ProjectedShadowInfo = NULL;
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos(LightSceneInfo->Id);

	if(!PrimitiveSceneInfo->ShadowParent)
	{
		// Check if the shadow is visible in any of the views.
		UBOOL bShadowIsPotentiallyVisibleNextFrame = FALSE;
		UBOOL bShadowIsVisibleThisFrame = FALSE;
		UBOOL bSubjectIsVisible = FALSE;
		UBOOL bTranslucentRelevance = FALSE;
		UBOOL bOpaqueRelevance = FALSE;
		UBOOL bForegroundRelevance = FALSE;
		for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views(ViewIndex);

			// Lookup the primitive's cached view relevance
			FPrimitiveViewRelevance ViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);

			if (!ViewRelevance.bInitializedThisFrame)
			{
				// Compute the subject primitive's view relevance since it wasn't cached
				ViewRelevance = PrimitiveSceneInfo->Proxy->GetViewRelevance(&View);
			}

			// Check if the subject primitive's shadow is view relevant.
			const UBOOL bShadowIsViewRelevant = (ViewRelevance.IsRelevant() || ViewRelevance.bShadowRelevance);

			// Check if the shadow and preshadow are occluded.
			const UBOOL bShadowIsOccluded =
				!View.bIgnoreExistingQueries &&
				View.State &&
				((FSceneViewState*)View.State)->IsShadowOccluded(PrimitiveSceneInfo->Component, LightSceneInfo->LightComponent, INDEX_NONE);

			// The shadow is visible if it is view relevant and unoccluded.
			bShadowIsVisibleThisFrame |= (bShadowIsViewRelevant && !bShadowIsOccluded);
			bShadowIsPotentiallyVisibleNextFrame |= bShadowIsViewRelevant;
			bTranslucentRelevance |= ViewRelevance.bTranslucentRelevance;
			bOpaqueRelevance |= ViewRelevance.bOpaqueRelevance;
			bForegroundRelevance |= ViewRelevance.GetDPG(SDPG_Foreground);
			
			// Check if the subject is visible this frame.
			const UBOOL bSubjectIsVisibleInThisView = View.PrimitiveVisibilityMap(PrimitiveSceneInfo->Id);
		    bSubjectIsVisible |= bSubjectIsVisibleInThisView;
		}

		if(!bShadowIsVisibleThisFrame && !bShadowIsPotentiallyVisibleNextFrame)
		{
			// Don't setup the shadow info for shadows which don't need to be rendered or occlusion tested.
			return NULL;
		}

		TArray <FPrimitiveSceneInfo*, SceneRenderingAllocator> ChildSceneInfos;
		GenerateChildSceneInfos(PrimitiveSceneInfo, (Views(0).Family->ShowFlags & SHOW_Editor) != 0, ChildSceneInfos);

		// Compute the composite bounds of this group of shadow primitives.
		FBoxSphereBounds OriginalBounds(PrimitiveSceneInfo->Bounds);
		//Iterate through all recursive child scene infos
		for ( TArray <FPrimitiveSceneInfo*, SceneRenderingAllocator>::TConstIterator ChildSceneInfoIter(ChildSceneInfos); ChildSceneInfoIter; ++ChildSceneInfoIter )
		{
			FPrimitiveSceneInfo* ShadowChild = *ChildSceneInfoIter;
			OriginalBounds = OriginalBounds + ShadowChild->Bounds;
		}

		// Shadowing constants.
		const UINT MinShadowResolution = (LightSceneInfo->MinShadowResolution > 0) ? LightSceneInfo->MinShadowResolution : GSystemSettings.MinShadowResolution;
		const UINT MaxShadowResolutionSetting = (LightSceneInfo->MaxShadowResolution > 0) ? LightSceneInfo->MaxShadowResolution : GSystemSettings.MaxShadowResolution;
		const FIntPoint ShadowBufferResolution = GSceneRenderTargets.GetShadowDepthTextureResolution();
		const UINT MaxShadowResolution = Min(MaxShadowResolutionSetting - SHADOW_BORDER * 2, ShadowBufferResolution.X - SHADOW_BORDER * 2);
		const UINT MaxShadowResolutionY = Min(MaxShadowResolutionSetting - SHADOW_BORDER * 2, ShadowBufferResolution.Y - SHADOW_BORDER * 2);
		const INT ShadowFadeResolution = (LightSceneInfo->ShadowFadeResolution > 0) ? LightSceneInfo->ShadowFadeResolution : GSystemSettings.ShadowFadeResolution;

		// Compute the maximum resolution required for the shadow by any view. Also keep track of the unclamped resolution for fading.
		UINT MaxDesiredResolution = 0;
		UINT MaxUnclampedResolution	= 0;
		FLOAT MaxScreenPercent = 0;
		TArray<FLOAT, TInlineAllocator<2> > ResolutionFadeAlphas;
		TArray<FLOAT, TInlineAllocator<2> > ResolutionPreShadowFadeAlphas;
		FLOAT MaxResolutionFadeAlpha = 0;
		FLOAT MaxResolutionPreShadowFadeAlpha = 0;

		for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views(ViewIndex);

			// Determine the size of the subject's bounding sphere in this view.
			const FVector4 ScreenPosition = View.WorldToScreen(OriginalBounds.Origin);
			const FLOAT ScreenRadius = Max(
				View.SizeX / 2.0f * View.ProjectionMatrix.M[0][0],
				View.SizeY / 2.0f * View.ProjectionMatrix.M[1][1]
				) *
				OriginalBounds.SphereRadius /
				Max(ScreenPosition.W,1.0f);

			const FLOAT ScreenPercent = Max(
				1.0f / 2.0f * View.ProjectionMatrix.M[0][0],
				1.0f / 2.0f * View.ProjectionMatrix.M[1][1]
				) *
				OriginalBounds.SphereRadius /
				Max(ScreenPosition.W,1.0f);

			MaxScreenPercent = Max(MaxScreenPercent, ScreenPercent);

			// Determine the amount of shadow buffer resolution needed for this view.
			const UINT UnclampedResolution = appTrunc(ScreenRadius * GSystemSettings.ShadowTexelsPerPixel);
			MaxUnclampedResolution = Max( MaxUnclampedResolution, UnclampedResolution );
			MaxDesiredResolution = Max(
				MaxDesiredResolution,
				Clamp<UINT>(
					UnclampedResolution,
					Min(MinShadowResolution,ShadowBufferResolution.X - SHADOW_BORDER*2),
					MaxShadowResolution
					)
				);

			// Calculate fading based on resolution
			const FLOAT ViewSpecificAlpha = PrimitiveSceneInfo->bAllowShadowFade ? 
				CalculateShadowFadeAlpha( UnclampedResolution, ShadowFadeResolution, MinShadowResolution ) :
				1.0f;
			MaxResolutionFadeAlpha = Max(MaxResolutionFadeAlpha, ViewSpecificAlpha);
			ResolutionFadeAlphas.AddItem(ViewSpecificAlpha);

			const FLOAT ViewSpecificPreShadowAlpha = PrimitiveSceneInfo->bAllowShadowFade ? 
				CalculateShadowFadeAlpha( appTrunc(UnclampedResolution * GSystemSettings.PreShadowResolutionFactor), GSystemSettings.PreShadowFadeResolution, GSystemSettings.MinPreShadowResolution ) :
				1.0f;
			MaxResolutionPreShadowFadeAlpha = Max(MaxResolutionPreShadowFadeAlpha, ViewSpecificPreShadowAlpha);
			ResolutionPreShadowFadeAlphas.AddItem(ViewSpecificPreShadowAlpha);
		}

		const UBOOL bRenderPreShadow = LightSceneInfo->bAllowPreShadow 
			// Preshadows should be disabled when emulating mobile features
			&& !GEmulateMobileRendering
			// Allow primitives with unbuilt lighting to create preshadows in the editor for previewing
			&& (PrimitiveSceneInfo->bAllowPreShadow || GIsEditor && PrimitiveSceneInfo->bStaticShadowing)
			&& bSubjectIsVisible
			// Don't allow preshadows from dominant lights on primitives that don't render their depth in the depth only pass,
			// Since that depth is required to project dominant shadows correctly.  
			// Other dominant shadows will still draw over primitives with bUseAsOccluder=FALSE, but this at least prevents reading uninitialized shadow depths.
			// Also don't create preshadows from dominant lights on primitives with bAcceptsDynamicDominantLightShadows = false, since the shadowing will not be visible anyway.
			&& (!IsDominantLightType(LightSceneInfo->LightType) || PrimitiveSceneInfo->bUseAsOccluder && PrimitiveSceneInfo->bAcceptsDynamicDominantLightShadows);

		FBoxSphereBounds Bounds = OriginalBounds;

		if (bRenderPreShadow && GCachePreshadows)
		{
			// If we're creating a preshadow, expand the bounds somewhat so that the preshadow will be cached more often as the shadow caster moves around.
			//@todo - only expand the preshadow bounds for this, not the per object shadow.
			Bounds.SphereRadius += (Bounds.BoxExtent * GPreshadowExpandFraction).Size();
			Bounds.BoxExtent *= GPreshadowExpandFraction + 1.0f;
		}

		// Compute the projected shadow initializer for this primitive-light pair.
		FProjectedShadowInitializer ShadowInitializer;
		if ((MaxResolutionFadeAlpha > 1.0f / 256.0f || bRenderPreShadow && MaxResolutionPreShadowFadeAlpha > 1.0f / 256.0f)
			&& LightSceneInfo->GetProjectedShadowInitializer(Bounds, ShadowInitializer))
		{
			// Find the minimum elapsed time in any view since the subject was visible
			const FLOAT MinElapsedVisibleTime = Min(
				LightSceneInfo->ModShadowFadeoutTime,
				ViewFamily.CurrentWorldTime - PrimitiveSceneInfo->LastRenderTime
				);		
			const FLOAT MinElapsedVisChangeTime = Min(
				LightSceneInfo->ModShadowFadeoutTime,
				ViewFamily.CurrentWorldTime - PrimitiveSceneInfo->LastVisibilityChangeTime
				);

			// If the light has a positive value set for fade time, then fade the shadow based on the subject's visibility
			FLOAT ElapsedTimeFadeFraction = 1.0f;
			if (LightSceneInfo->ModShadowFadeoutTime > KINDA_SMALL_NUMBER)
			{
				if (bSubjectIsVisible)
				{
					// Fade the shadow in the longer the subject is visible
					// Invert the exponent for fading in
					const FLOAT FadeInExponent = 1.0f / Max(0.01f, LightSceneInfo->ModShadowFadeoutExponent);
					// Calculate a fade percent between 0 (completely faded out) and 1 (completely faded in) based on the light's fade time and exponent
					const FLOAT NormalizedFadePercent = appPow(MinElapsedVisChangeTime / LightSceneInfo->ModShadowFadeoutTime, FadeInExponent);
					// Convert the percent to the range [ModShadowStartFadeInPercent, 1]
					const FLOAT CurrentFade = NormalizedFadePercent * (1.0f - Interaction->ModShadowStartFadeInPercent) + Interaction->ModShadowStartFadeInPercent;
					ElapsedTimeFadeFraction = Clamp(CurrentFade, 0.0f, 1.0f);

					// Todo - fix the one-frame glitch when the subject first becomes visible, because LastVisibilityChangeTime hasn't been updated yet

					// Set the percent that fading out will start at as the current fade percent
					// This allows visibility transitions when fading hasn't completed to start from the current fade percent
					Interaction->ModShadowStartFadeOutPercent = ElapsedTimeFadeFraction;
				}
				else
				{
					// Fade the shadow out the longer the subject is not visible
					const FLOAT FadeOutExponent = Max(0.01f, LightSceneInfo->ModShadowFadeoutExponent);
					// Calculate a fade percent between 0 (completely faded in) and 1 (completely faded out) based on the light's fade time and exponent
					const FLOAT NormalizedFadePercent = appPow(MinElapsedVisibleTime / LightSceneInfo->ModShadowFadeoutTime, FadeOutExponent);
					// Convert the percent to the range [0, ModShadowStartFadeOutPercent]
					const FLOAT CurrentFade = Interaction->ModShadowStartFadeOutPercent - Interaction->ModShadowStartFadeOutPercent * NormalizedFadePercent;
					ElapsedTimeFadeFraction = Clamp(CurrentFade, 0.0f, 1.0f);

					// Set the percent that fading in will start at as the current fade percent
					// This allows visibility transitions when fading hasn't completed to start from the current fade percent
					Interaction->ModShadowStartFadeInPercent = ElapsedTimeFadeFraction;
				}
			}

			// Combine fading based on resolution and visibility time
			const FLOAT MaxFadeAlpha = MaxResolutionFadeAlpha * ElapsedTimeFadeFraction;

			for (INT ViewIndex = 0; ViewIndex < ResolutionFadeAlphas.Num(); ViewIndex++)
			{
				ResolutionFadeAlphas(ViewIndex) *= ElapsedTimeFadeFraction;
			}

			// Only create a shadow from this object if it hasn't completely faded away
			if(MaxFadeAlpha > 1.0f / 256.0f 
				&& bOpaqueRelevance
				// Don't create per-object shadows for movable dominant lights
				&& (!IsDominantLightType(LightSceneInfo->LightType) || LightSceneInfo->bStaticShadowing))
			{
				// Round down to the nearest power of two so that resolution changes are always doubling or halving the resolution, which increases filtering stability
				// Use the max resolution if the desired resolution is larger than that
				const INT SizeX = MaxDesiredResolution >= MaxShadowResolution ? MaxShadowResolution : (1 << (appCeilLogTwo(MaxDesiredResolution) - 1));
				const UINT DesiredSizeY = appTrunc(MaxDesiredResolution / ShadowInitializer.AspectRatio);
				const INT SizeY = DesiredSizeY >= MaxShadowResolutionY ? MaxShadowResolutionY : (1 << (appCeilLogTwo(DesiredSizeY) - 1));

				if( GLogNextFrameShadowGenerationData )
				{
					debugf( TEXT("    Creating Shadow Data: (%d, %d) %s, %s"), SizeX, SizeY, *PrimitiveSceneInfo->Proxy->ResourceName.ToString(), *PrimitiveSceneInfo->Component->GetPathName() );
				}

				// Create a projected shadow for this interaction's shadow.
				ProjectedShadowInfo = new(GRenderingThreadMemStack,1,16) FProjectedShadowInfo(
					LightSceneInfo,
					PrimitiveSceneInfo,
					Interaction,
					ShadowInitializer,
					FALSE,
					SizeX,
					SizeY,
					MaxScreenPercent,
					ResolutionFadeAlphas
					);

				// Store the view-specific mask for this shadow
				VisibleLightInfo.AllProjectedShadows.AddItem(ProjectedShadowInfo);
				VisibleLightInfo.MemStackProjectedShadows.AddItem(ProjectedShadowInfo);

				for ( TArray <FPrimitiveSceneInfo*, SceneRenderingAllocator>::TConstIterator ChildSceneInfoIter(ChildSceneInfos); ChildSceneInfoIter; ++ChildSceneInfoIter )
				{
					FPrimitiveSceneInfo* ShadowChild = *ChildSceneInfoIter;
					// Add the subject primitive to the projected shadow.
					ProjectedShadowInfo->AddSubjectPrimitive(ShadowChild, Views);
				}

				// Add the subject primitive to the projected shadow.
				ProjectedShadowInfo->AddSubjectPrimitive(Interaction->GetPrimitiveSceneInfo(), Views);
			}

			// Combine fading based on resolution and visibility time
			const FLOAT MaxPreFadeAlpha = MaxResolutionPreShadowFadeAlpha * ElapsedTimeFadeFraction;

			for (INT ViewIndex = 0; ViewIndex < ResolutionPreShadowFadeAlphas.Num(); ViewIndex++)
			{
				ResolutionPreShadowFadeAlphas(ViewIndex) *= ElapsedTimeFadeFraction;
			}

			// If the subject is visible in at least one view, create a preshadow for static primitives shadowing the subject.
			if(MaxPreFadeAlpha > 1.0f / 256.0f 
				&& bRenderPreShadow
				&& (bOpaqueRelevance || bTranslucentRelevance))
			{
				// Round down to the nearest power of two so that resolution changes are always doubling or halving the resolution, which increases filtering stability.
				INT PreshadowSizeX = 1 << (appCeilLogTwo(appTrunc(MaxDesiredResolution * GSystemSettings.PreShadowResolutionFactor)) - 1);
				INT PreshadowSizeY = Min<UINT>(1 << (appCeilLogTwo(appTrunc(MaxDesiredResolution * GSystemSettings.PreShadowResolutionFactor / ShadowInitializer.AspectRatio)) - 1), 
					appTrunc(GSystemSettings.PreShadowResolutionFactor * MaxShadowResolutionY));
				
#if XBOX
				// Round up the texture size to a multiple of 32 pixels so the textures will be proper, which will also implicitly
				// enforce the 8 byte D3DRESOLVEALIGNMENT on the starting coordinates in the EDRAM layout
				PreshadowSizeX = Align(PreshadowSizeX + SHADOW_BORDER * 2, 32) - SHADOW_BORDER * 2;
				PreshadowSizeY = Align(PreshadowSizeY + SHADOW_BORDER * 2, 32) - SHADOW_BORDER * 2;
#endif

				const FIntPoint PreshadowCacheResolution = GSceneRenderTargets.GetPreshadowCacheTextureResolution();
				checkSlow(PreshadowSizeX <= PreshadowCacheResolution.X && PreshadowSizeY <= PreshadowCacheResolution.Y);

				UBOOL bIsOutsideWholeSceneShadow = TRUE;
				if (bOpaqueRelevance && !bForegroundRelevance)
				{
					for (INT i = 0; i < ViewDependentWholeSceneShadows.Num(); i++)
					{
						const FProjectedShadowInfo* WholeSceneShadow = ViewDependentWholeSceneShadows(i);
						FVector2D DistanceFadeValues;
						const UBOOL bRender = WholeSceneShadow->LightSceneInfo->GetDirectionalLightDistanceFadeParameters(DistanceFadeValues);
						// Should only be in the ViewDependentWholeSceneShadows array if it is getting rendered
						check(bRender);
						const FLOAT DistanceFromShadowCenterSquared = (WholeSceneShadow->ShadowBounds.Center - Bounds.Origin).SizeSquared();
						//@todo - if view dependent whole scene shadows are ever supported in splitscreen, 
						// We can only disable the preshadow at this point if it is inside a whole scene shadow for all views
						const FLOAT DistanceFromViewSquared = ((FVector)WholeSceneShadow->DependentView->ViewOrigin - Bounds.Origin).SizeSquared();
						// Mark the preshadow as inside the whole scene shadow if its bounding sphere is inside the near fade distance
						if (DistanceFromShadowCenterSquared < Square(Max(WholeSceneShadow->ShadowBounds.W - Bounds.SphereRadius, 0.0f))
							//@todo - why is this extra threshold required?
							&& DistanceFromViewSquared < Square(Max(DistanceFadeValues.X - 200.0f - Bounds.SphereRadius, 0.0f)))
						{
							bIsOutsideWholeSceneShadow = FALSE;
							break;
						}
					}
				}
				
				// Only create opaque preshadows when part of the caster is outside the whole scene shadow.
				// Opaque preshadows with foreground relevance always need to be created because whole scene shadows don't handle the foreground DPG
				if (bTranslucentRelevance || bForegroundRelevance || bIsOutsideWholeSceneShadow)
				{
					// Try to reuse a preshadow from the cache
					TRefCountPtr<FProjectedShadowInfo> ProjectedPreShadowInfo = GetCachedPreshadow(Interaction, ShadowInitializer, OriginalBounds, PreshadowSizeX, PreshadowSizeY);

					if (ProjectedPreShadowInfo)
					{
						// Update fade alpha on the cached preshadow
						ProjectedPreShadowInfo->FadeAlphas = ResolutionPreShadowFadeAlphas;
					}
					else
					{
						// Create a new projected shadow for this interaction's preshadow
						// Not using the scene rendering mem stack because this shadow info may need to persist for multiple frames if it gets cached
						ProjectedPreShadowInfo = new FProjectedShadowInfo(
							LightSceneInfo,
							PrimitiveSceneInfo,
							Interaction,
							ShadowInitializer,
							TRUE,
							PreshadowSizeX,
							PreshadowSizeY,
							MaxScreenPercent,
							ResolutionPreShadowFadeAlphas
							);
					}

					VisibleLightInfo.AllProjectedShadows.AddItem(ProjectedPreShadowInfo);
					VisibleLightInfo.ProjectedPreShadows.AddItem(ProjectedPreShadowInfo);

					// Only add to OutPreShadows if the preshadow doesn't already have depths cached, 
					// Since OutPreShadows is used to generate information only used when rendering the shadow depths.
					if (!ProjectedPreShadowInfo->bDepthsCached)
					{
						OutPreShadows.AddItem(ProjectedPreShadowInfo);
					}

					for ( TArray <FPrimitiveSceneInfo*, SceneRenderingAllocator>::TConstIterator ChildSceneInfoIter(ChildSceneInfos); ChildSceneInfoIter; ++ChildSceneInfoIter )
					{
						FPrimitiveSceneInfo* ShadowChild = *ChildSceneInfoIter;
						// Add the subject primitive to the projected shadow.
						ProjectedPreShadowInfo->AddReceiverPrimitive(ShadowChild);
					}

					// Add the subject primitive to the projected shadow as the receiver.
					ProjectedPreShadowInfo->AddReceiverPrimitive(Interaction->GetPrimitiveSceneInfo());
				}
			}
		}
	}
	return ProjectedShadowInfo;
}

/**
 * Creates a projected shadow for all primitives affected by a light.  If the light doesn't support whole-scene shadows, it returns FALSE.
 * @param LightSceneInfo - The light to create a shadow for.
 * @return TRUE if a whole scene shadow was created
 */
UBOOL FSceneRenderer::CreateWholeSceneProjectedShadow(FLightSceneInfo* LightSceneInfo)
{
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos(LightSceneInfo->Id);

	// Try to create a whole-scene projected shadow initializer for the light.
	TArray<FProjectedShadowInitializer, TInlineAllocator<6> > ProjectedShadowInitializers;
	if (LightSceneInfo->GetWholeSceneProjectedShadowInitializer(Views, ProjectedShadowInitializers))
	{
		checkSlow(ProjectedShadowInitializers.Num() > 0);

		const UBOOL bRenderOnePassPointLightShadow = GRenderOnePassPointLightShadows 
			&& GRHIShaderPlatform == SP_PCD3D_SM5
			&& (LightSceneInfo->LightType == LightType_Point || LightSceneInfo->LightType == LightType_DominantPoint);

		// Shadow resolution constants.
		const UINT EffectiveDoubleShadowBorder = bRenderOnePassPointLightShadow ? 0 : SHADOW_BORDER * 2;
		const UINT MinShadowResolution = (LightSceneInfo->MinShadowResolution > 0) ? LightSceneInfo->MinShadowResolution : GSystemSettings.MinShadowResolution;
		const UINT MaxShadowResolutionSetting = (LightSceneInfo->MaxShadowResolution > 0) ? LightSceneInfo->MaxShadowResolution : GSystemSettings.MaxShadowResolution;
		const FIntPoint ShadowBufferResolution = GSceneRenderTargets.GetShadowDepthTextureResolution();
		const UINT MaxShadowResolution = Min(MaxShadowResolutionSetting - EffectiveDoubleShadowBorder, ShadowBufferResolution.X - EffectiveDoubleShadowBorder);
		const UINT MaxShadowResolutionY = Min(MaxShadowResolutionSetting - EffectiveDoubleShadowBorder, ShadowBufferResolution.Y - EffectiveDoubleShadowBorder);
		const INT ShadowFadeResolution = (LightSceneInfo->ShadowFadeResolution > 0) ? LightSceneInfo->ShadowFadeResolution : GSystemSettings.ShadowFadeResolution;

		// Compute the maximum resolution required for the shadow by any view. Also keep track of the unclamped resolution for fading.
		UINT MaxDesiredResolution = 0;
		UINT MaxUnclampedResolution	= 0;
		TArray<FLOAT, TInlineAllocator<2> > FadeAlphas;
		FLOAT MaxFadeAlpha = 0;

		for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views(ViewIndex);

			// Determine the size of the light's bounding sphere in this view.
			const FVector4 ScreenPosition = View.WorldToScreen(LightSceneInfo->GetOrigin());
			const FLOAT ScreenRadius = Max(
				View.SizeX / 2.0f * View.ProjectionMatrix.M[0][0],
				View.SizeY / 2.0f * View.ProjectionMatrix.M[1][1]
				) *
				LightSceneInfo->GetRadius() /
				Max(ScreenPosition.W,1.0f);

			// Determine the amount of shadow buffer resolution needed for this view.
			const UINT UnclampedResolution = appTrunc(ScreenRadius * GSystemSettings.ShadowTexelsPerPixel);
			MaxUnclampedResolution = Max( MaxUnclampedResolution, UnclampedResolution );
			MaxDesiredResolution = Max(
				MaxDesiredResolution,
				Clamp<UINT>(
					UnclampedResolution,
					Min(MinShadowResolution,ShadowBufferResolution.X - EffectiveDoubleShadowBorder),
					MaxShadowResolution
					)
				);

			const FLOAT FadeAlpha = CalculateShadowFadeAlpha( MaxUnclampedResolution, ShadowFadeResolution, MinShadowResolution );
			MaxFadeAlpha = Max(MaxFadeAlpha, FadeAlpha);
			FadeAlphas.AddItem(FadeAlpha);
		}

		if (MaxFadeAlpha > 1.0f / 256.0f)
		{
			for (INT ShadowIndex = 0; ShadowIndex < ProjectedShadowInitializers.Num(); ShadowIndex++)
			{
				const FProjectedShadowInitializer& ProjectedShadowInitializer = ProjectedShadowInitializers(ShadowIndex);

				// Round down to the nearest power of two so that resolution changes are always doubling or halving the resolution, which increases filtering stability
				// Use the max resolution if the desired resolution is larger than that
				INT SizeX = MaxDesiredResolution >= MaxShadowResolution ? MaxShadowResolution : (1 << (appCeilLogTwo(MaxDesiredResolution) - 1));
				const UINT DesiredSizeY = appTrunc(MaxDesiredResolution / ProjectedShadowInitializer.AspectRatio);
				INT SizeY = DesiredSizeY >= MaxShadowResolutionY ? MaxShadowResolutionY : (1 << (appCeilLogTwo(DesiredSizeY) - 1));

				if (bRenderOnePassPointLightShadow)
				{
					// Round to a resolution that is supported for one pass point light shadows
					SizeX = SizeY = GSceneRenderTargets.GetCubeShadowDepthZResolution(GSceneRenderTargets.GetCubeShadowDepthZIndex(MaxDesiredResolution));
				}

				// Create the projected shadow info.
				FProjectedShadowInfo* ProjectedShadowInfo = new(GRenderingThreadMemStack,1,16) FProjectedShadowInfo(
					LightSceneInfo,
					NULL,
					ProjectedShadowInitializer,
					SizeX,
					SizeY,
					FadeAlphas
					);
				VisibleLightInfo.MemStackProjectedShadows.AddItem(ProjectedShadowInfo);
				VisibleLightInfo.AllProjectedShadows.AddItem(ProjectedShadowInfo);

				if (bRenderOnePassPointLightShadow)
				{
					const static FVector CubeDirections[6] = 
					{
						FVector(-1, 0, 0),
						FVector(1, 0, 0),
						FVector(0, -1, 0),
						FVector(0, 1, 0),
						FVector(0, 0, -1),
						FVector(0, 0, 1)
					};

					const static FVector UpVectors[6] = 
					{
						FVector(0, 1, 0),
						FVector(0, 1, 0),
						FVector(0, 0, -1),
						FVector(0, 0, 1),
						FVector(0, 1, 0),
						FVector(0, 1, 0)
					};

					const FMatrix FaceProjection = FPerspectiveMatrix(PI / 4.0f, 1, 1, 1, ProjectedShadowInfo->LightSceneInfo->GetRadius());
					const FVector LightPosition = ProjectedShadowInfo->LightSceneInfo->GetPosition();
					
					ProjectedShadowInfo->OnePassShadowViewProjectionMatrices.Empty(6);
					ProjectedShadowInfo->OnePassShadowFrustums.Empty(6);
					ProjectedShadowInfo->OnePassShadowFrustums.AddZeroed(6);
					const FMatrix ScaleMatrix = FScaleMatrix(FVector(1, -1, 1));
					for (INT FaceIndex = 0; FaceIndex < 6; FaceIndex++)
					{
						// Create a view projection matrix for each cube face
						const FMatrix ShadowViewProjectionMatrix = FLookAtMatrix(LightPosition, LightPosition + CubeDirections[FaceIndex], UpVectors[FaceIndex]) * ScaleMatrix * FaceProjection;
						ProjectedShadowInfo->OnePassShadowViewProjectionMatrices.AddItem(ShadowViewProjectionMatrix);
						// Create a convex volume out of the frustum so it can be used for object culling
						GetViewFrustumBounds(ProjectedShadowInfo->OnePassShadowFrustums(FaceIndex), ShadowViewProjectionMatrix, FALSE);
					}
				}

				// Add all the shadow casting primitives affected by the light to the shadow's subject primitive list.
				for(FLightPrimitiveInteraction* Interaction = LightSceneInfo->DynamicPrimitiveList;
					Interaction;
					Interaction = Interaction->GetNextPrimitive())
				{
					if(Interaction->HasShadow())
					{
						ProjectedShadowInfo->AddSubjectPrimitive(Interaction->GetPrimitiveSceneInfo(), Views);
					}
				}
			}
		}

		return TRUE;
	}

	return FALSE;
}

void FSceneRenderer::InitProjectedShadowVisibility()
{
	// Initialize the views' ProjectedShadowVisibilityMaps and remove shadows without subjects.
	for(TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights);LightIt;++LightIt)
	{
		FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos(LightIt.GetIndex());

		// Allocate the light's projected shadow visibility and view relevance maps for this view.
		for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			FViewInfo& View = Views(ViewIndex);
			FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos(LightIt.GetIndex());
			VisibleLightViewInfo.ProjectedShadowVisibilityMap.Init(FALSE,VisibleLightInfo.AllProjectedShadows.Num());
			VisibleLightViewInfo.ProjectedShadowViewRelevanceMap.Empty(VisibleLightInfo.AllProjectedShadows.Num());
			VisibleLightViewInfo.ProjectedShadowViewRelevanceMap.AddZeroed(VisibleLightInfo.AllProjectedShadows.Num());
		}

		for( INT ShadowIndex=0; ShadowIndex<VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++ )
		{
			FProjectedShadowInfo& ProjectedShadowInfo = *VisibleLightInfo.AllProjectedShadows(ShadowIndex);

			// Assign the shadow its id.
			ProjectedShadowInfo.ShadowId = ShadowIndex;

			for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				FViewInfo& View = Views(ViewIndex);
				UBOOL bSkipOtherView = TRUE;
#if WITH_REALD
				if (RealD::IsStereoEnabled())
				{
					bSkipOtherView = FALSE;
				}
#endif
				if (bSkipOtherView && ProjectedShadowInfo.DependentView && ProjectedShadowInfo.DependentView != &View)
				{
					continue;
				}
				FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos(LightIt.GetIndex());

				if(VisibleLightViewInfo.bInViewFrustum)
				{
					// Compute the subject primitive's view relevance.  Note that the view won't necessarily have it cached,
					// since the primitive might not be visible.
					FPrimitiveViewRelevance ViewRelevance;
					if(ProjectedShadowInfo.ParentSceneInfo)
					{
						ViewRelevance = ProjectedShadowInfo.ParentSceneInfo->Proxy->GetViewRelevance(&View);

						// hidden primitives won't have DPG relevance, so allow shadows to render in world DPG
						if( !ViewRelevance.bWorldDPG && !ViewRelevance.bForegroundDPG )
						{
							ViewRelevance.SetDPG(SDPG_World,TRUE);
						}
					}
					else
					{
						ViewRelevance.bStaticRelevance = ViewRelevance.bDynamicRelevance = ViewRelevance.bShadowRelevance = TRUE;
						ViewRelevance.SetDPG(SDPG_World,TRUE);
					}							
					VisibleLightViewInfo.ProjectedShadowViewRelevanceMap(ShadowIndex) = ViewRelevance;

					// Check if the subject primitive's shadow is view relevant.
					const UBOOL bShadowIsViewRelevant = (ViewRelevance.IsRelevant() || ViewRelevance.bShadowRelevance);

					// Check if the shadow and preshadow are occluded.
					const UBOOL bShadowIsOccluded =
						!View.bIgnoreExistingQueries &&
						View.State &&
						((FSceneViewState*)View.State)->IsShadowOccluded(
							ProjectedShadowInfo.ParentSceneInfo ? 
								ProjectedShadowInfo.ParentSceneInfo->Component :
								NULL,
							ProjectedShadowInfo.LightSceneInfo->LightComponent,
							ProjectedShadowInfo.SplitIndex
							);

					// The shadow is visible if it is view relevant and unoccluded.
					if(bShadowIsViewRelevant && !bShadowIsOccluded)
					{
						VisibleLightViewInfo.ProjectedShadowVisibilityMap(ShadowIndex) = TRUE;
					}

					// Draw the shadow frustum.
					if(bShadowIsViewRelevant && !bShadowIsOccluded)
					{
						if (ProjectedShadowInfo.bPreShadow)
						{
							if ((ViewFamily.ShowFlags & SHOW_PreShadowFrustums))
							{
								FViewElementPDI ShadowFrustumPDI(&Views(ViewIndex),NULL);
								ProjectedShadowInfo.RenderFrustumWireframe(&ShadowFrustumPDI);
							}
						}
						else
						{
							if ((ViewFamily.ShowFlags & SHOW_ShadowFrustums))
							{
								FViewElementPDI ShadowFrustumPDI(&Views(ViewIndex),NULL);
								ProjectedShadowInfo.RenderFrustumWireframe(&ShadowFrustumPDI);
							}
						}
					}
				}
			}
		}
	}
}

/**
 * Checks to see if this primitive is affected by any of the given modulate better or pre shadows (used 
 * to filter out modulate better shadows on emissive/backfaces, as well as check for pre-shadow interactions)
 *
 * @param PrimitiveSceneInfoCompact The primitive to check for shadow interaction
 * @param ValidViews Which views to process this primitive for
 * @param PreShadows The list of pre-shadows to check against
 *
 * @return The views that the children should be checked in
 */
BYTE FSceneRenderer::GatherShadowsForPrimitiveInner(
	const FPrimitiveSceneInfoCompact& PrimitiveSceneInfoCompact, 
	BYTE ValidViews,
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& PreShadows,
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ForwardObjectShadows,
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ViewDependentWholeSceneShadows)
{
	FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneInfoCompact.PrimitiveSceneInfo;
	const FBoxSphereBounds& PrimitiveBounds = PrimitiveSceneInfoCompact.Bounds;

	// keep track of which views we should process any LOD children for
	BYTE ProcessChildrenForViews = 0;

	// See if it is visible (ie. not occluded) in any view
	UBOOL bIsPrimitiveVisible = FALSE;

#if USE_MASSIVE_LOD

	// we use a moving bit here to avoid (1 << ViewIndex) which will generate microcode on consoles and stall the pipeline
	INT ViewBit = 1;
	for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++, ViewBit <<= 1)
	{
		const FViewInfo& View = Views(ViewIndex);
		// skip views we don't want to process
		if ((ValidViews & ViewBit) == 0)
		{
			continue;
		}

		if( View.PrimitiveVisibilityMap(PrimitiveSceneInfo->Id) )
		{
			bIsPrimitiveVisible = TRUE;
		}
		// if this is a parent, and it's not visible, but it was processed, that means its children may be visible, and should be processed recursively
		else if (View.PrimitiveParentProcessedMap(PrimitiveSceneInfo->Id))
		{
			ProcessChildrenForViews |= ViewBit;
		}
	}

#else

	INT ViewBit = 1;
	for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++, ViewBit <<= 1)
	{
		const FViewInfo& View = Views(ViewIndex);
		if( View.PrimitiveVisibilityMap(PrimitiveSceneInfo->Id) )
		{
			bIsPrimitiveVisible = TRUE;
			break;
		}
	}

#endif

	if(PrimitiveSceneInfoCompact.bCastDynamicShadow)
	{
		// Check if the primitive is a subject for any of the preshadows.
		// Only allow preshadows from lightmapped primitives that cast both dynamic and static shadows.
		if (PrimitiveSceneInfo->bCastStaticShadow && PrimitiveSceneInfo->bStaticShadowing)
		{
			for(INT ShadowIndex = 0;ShadowIndex < PreShadows.Num();ShadowIndex++)
			{
				FProjectedShadowInfo* ProjectedShadowInfo = PreShadows(ShadowIndex);

				// Check if this primitive is in the shadow's frustum.
				if( ProjectedShadowInfo->SubjectAndReceiverFrustum.IntersectBox(
						PrimitiveBounds.Origin,
						ProjectedShadowInfo->PreShadowTranslation,
						PrimitiveBounds.BoxExtent)
					&& ProjectedShadowInfo->LightSceneInfoCompact.AffectsPrimitive(PrimitiveSceneInfoCompact) )
				{
					checkSlow(ProjectedShadowInfo->LightSceneInfoCompact.LightSceneInfo->bAllowPreShadow);
					// Add this primitive to the shadow.
					ProjectedShadowInfo->AddSubjectPrimitive(PrimitiveSceneInfo, Views);
				}
			}
		}

		for(INT ShadowIndex = 0;ShadowIndex < ForwardObjectShadows.Num();ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = ForwardObjectShadows(ShadowIndex);

			// Check if this primitive is in the shadow's frustum.
			if( ProjectedShadowInfo->ReceiverFrustum.IntersectBox(
				PrimitiveBounds.Origin,
				ProjectedShadowInfo->PreShadowTranslation,
				PrimitiveBounds.BoxExtent)
				// Filter by the light's allowed forward receivers list to reduce draw calls
				&& ProjectedShadowInfo->LightSceneInfo->ForwardShadowReceivers.ContainsItem(PrimitiveSceneInfo->Component)
				// Prevent self shadowing
				&& !ProjectedShadowInfo->IsSubjectPrimitive(PrimitiveSceneInfo))
			{
				// Add this primitive to the shadow.
				ProjectedShadowInfo->AddReceiverPrimitive(PrimitiveSceneInfo);
			}
		}

		for(INT ShadowIndex = 0;ShadowIndex < ViewDependentWholeSceneShadows.Num();ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = ViewDependentWholeSceneShadows(ShadowIndex);
			const FVector LightDirection = ProjectedShadowInfo->LightSceneInfo->GetDirection();
			// Project the primitive's bounds origin onto the light vector
			const FLOAT ProjectedDistanceFromShadowOrigin = (PrimitiveBounds.Origin - ProjectedShadowInfo->ShadowBounds.Center) | -LightDirection;
			// Calculate the primitive's squared distance to the cylinder's axis
			const FLOAT PrimitiveDistanceFromCylinderAxisSq = (-LightDirection * ProjectedDistanceFromShadowOrigin + ProjectedShadowInfo->ShadowBounds.Center - PrimitiveBounds.Origin).SizeSquared();
			
			// Include all primitives for movable lights, but only statically shadowed primitives from a light with static shadowing,
			// Since lights with static shadowing still create per-object shadows for primitives without static shadowing.
			if( (!ProjectedShadowInfo->LightSceneInfo->bStaticShadowing || PrimitiveSceneInfo->bCastStaticShadow && PrimitiveSceneInfo->bStaticShadowing)
				// Check if this primitive is in the shadow's cylinder
				//@todo - a more complex collision test could be used to cull more objects since the extruded shape of the shadow cascade is not actually a cylinder
				//@todo - also if we projected the shadows using cylinder shapes (instead of spheres) then we could exclude primitives that are completely in a previous cascade
				&& PrimitiveDistanceFromCylinderAxisSq < Square(ProjectedShadowInfo->ShadowBounds.W + PrimitiveBounds.SphereRadius)
				&& ProjectedDistanceFromShadowOrigin + PrimitiveBounds.SphereRadius > -ProjectedShadowInfo->ShadowBounds.W
				&& ProjectedDistanceFromShadowOrigin - PrimitiveBounds.SphereRadius < -ProjectedShadowInfo->MinPreSubjectZ
				&& ProjectedShadowInfo->LightSceneInfoCompact.AffectsPrimitive(PrimitiveSceneInfoCompact) )
			{
				// Add this primitive to the shadow.
				ProjectedShadowInfo->AddWholeSceneSubjectPrimitive(PrimitiveSceneInfo);
			}
		}

		if (!PrimitiveSceneInfo->bStaticShadowing)
		{
			for (INT ShadowIndex = 0; ShadowIndex < PlanarReflectionShadows.Num(); ShadowIndex++)
			{
				FReflectionPlanarShadowInfo& ReflectionShadowInfo = PlanarReflectionShadows(ShadowIndex);

				if (ReflectionShadowInfo.ViewFrustum.IntersectBox(PrimitiveBounds.Origin, PrimitiveBounds.BoxExtent))
				{
					// Add primitives to the reflection shadow's primitive list if they pass a frustum check
					ReflectionShadowInfo.VisibleDynamicPrimitives.AddItem(PrimitiveSceneInfo);
				}
			}
		}
	}

	return ProcessChildrenForViews;
}

#if USE_MASSIVE_LOD

/**
 * Calls GatherShadowsForPrimitiveForInner for all given primitives, and recurses over children
 *
 * @param Primitives The list of primitives to check for shadow interaction
 * @param NumPrimitives The number of primitives pointed to by Primitives
 * @param ValidViews Which views to process this primitive for
 * @param PreShadows The list of pre-shadows to check against
 */
void FSceneRenderer::GatherShadowsForPrimitives(const FPrimitiveSceneInfoCompact** Primitives, INT NumPrimitives, BYTE ValidViews,
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& PreShadows,
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ForwardObjectShadows,
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ViewDependentWholeSceneShadows)
{
	for (INT PrimitiveIndex = 0; PrimitiveIndex < NumPrimitives; PrimitiveIndex++)
	{
		const FPrimitiveSceneInfoCompact* Primitive = Primitives[PrimitiveIndex];
		// gather the shadows for the primitive, and discover if it is visible
		BYTE ProcessChildrenForViews = GatherShadowsForPrimitiveInner(*Primitive, ValidViews, PreShadows, ForwardObjectShadows, ViewDependentWholeSceneShadows);

		// if we need to process children for any of the views, do so
		if (ProcessChildrenForViews && Primitive->ChildPrimitives.Num())
		{
			const FPrimitiveSceneInfoCompact** ChildPrimitiveList = (const FPrimitiveSceneInfoCompact**)Primitive->ChildPrimitives.GetTypedData();
			GatherShadowsForPrimitives(ChildPrimitiveList, Primitive->ChildPrimitives.Num(), ProcessChildrenForViews, PreShadows, ForwardObjectShadows, ViewDependentWholeSceneShadows);
		}
	}
}

#endif

/** Gathers the list of primitives used to draw pre-shadows and modulate-better shadows. */
void FSceneRenderer::GatherShadowPrimitives(
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& PreShadows,
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ForwardObjectShadows,
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ViewDependentWholeSceneShadows
	)
{
	if(PreShadows.Num() || ForwardObjectShadows.Num() > 0 || ViewDependentWholeSceneShadows.Num() || PlanarReflectionShadows.Num())
	{
		check(ViewDependentWholeSceneShadows.Num() == 0 || Scene->NumWholeSceneShadowLights > 0 || GIsEditor);
		for(INT ShadowIndex = 0;ShadowIndex < ViewDependentWholeSceneShadows.Num();ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = ViewDependentWholeSceneShadows(ShadowIndex);
			checkSlow(ProjectedShadowInfo->DependentView);
			// Initialize the whole scene shadow's depth map with the shadow independent depth map from the view
			ProjectedShadowInfo->StaticMeshWholeSceneShadowDepthMap.Init(FALSE,Scene->StaticMeshes.GetMaxIndex());
		}

		// Find primitives that are in a shadow frustum in the octree.
		for(FScenePrimitiveOctree::TConstIterator<SceneRenderingAllocator> PrimitiveOctreeIt(Scene->PrimitiveOctree);
			PrimitiveOctreeIt.HasPendingNodes();
			PrimitiveOctreeIt.Advance())
		{
			const FScenePrimitiveOctree::FNode& PrimitiveOctreeNode = PrimitiveOctreeIt.GetCurrentNode();
			const FOctreeNodeContext& PrimitiveOctreeNodeContext = PrimitiveOctreeIt.GetCurrentContext();

			// Find children of this octree node that may contain relevant primitives.
			FOREACH_OCTREE_CHILD_NODE(ChildRef)
			{
				if(PrimitiveOctreeNode.HasChild(ChildRef))
				{
					// Check that the child node is in the frustum for at least one shadow.
					const FOctreeNodeContext ChildContext = PrimitiveOctreeNodeContext.GetChildContext(ChildRef);
					UBOOL bIsInFrustum = FALSE;

					// Check for subjects of preshadows.
					if(!bIsInFrustum)
					{
						for(INT ShadowIndex = 0;ShadowIndex < PreShadows.Num();ShadowIndex++)
						{
							FProjectedShadowInfo* ProjectedShadowInfo = PreShadows(ShadowIndex);

							// Check if this primitive is in the shadow's frustum.
							if(ProjectedShadowInfo->SubjectAndReceiverFrustum.IntersectBox(
								ChildContext.Bounds.Center + ProjectedShadowInfo->PreShadowTranslation,
								ChildContext.Bounds.Extent
								))
							{
								bIsInFrustum = TRUE;
								break;
							}
						}
					}

					if(!bIsInFrustum)
					{
						for(INT ShadowIndex = 0;ShadowIndex < ForwardObjectShadows.Num();ShadowIndex++)
						{
							FProjectedShadowInfo* ProjectedShadowInfo = ForwardObjectShadows(ShadowIndex);

							// Check if this primitive is in the shadow's frustum.
							if(ProjectedShadowInfo->ReceiverFrustum.IntersectBox(
								ChildContext.Bounds.Center + ProjectedShadowInfo->PreShadowTranslation,
								ChildContext.Bounds.Extent
								))
							{
								bIsInFrustum = TRUE;
								break;
							}
						}
					}

					if (!bIsInFrustum)
					{
						for(INT ShadowIndex = 0;ShadowIndex < ViewDependentWholeSceneShadows.Num();ShadowIndex++)
						{
							FProjectedShadowInfo* ProjectedShadowInfo = ViewDependentWholeSceneShadows(ShadowIndex);

							// Check if this primitive is in the shadow's frustum.
							if(ProjectedShadowInfo->PreSubjectFrustum.IntersectBox(
								ChildContext.Bounds.Center + ProjectedShadowInfo->PreShadowTranslation,
								ChildContext.Bounds.Extent
								))
							{
								bIsInFrustum = TRUE;
								break;
							}
						}
					}

					if (!bIsInFrustum)
					{
						for(INT ShadowIndex = 0;ShadowIndex < PlanarReflectionShadows.Num();ShadowIndex++)
						{
							const FReflectionPlanarShadowInfo& ReflectionShadowInfo = PlanarReflectionShadows(ShadowIndex);

							if (ReflectionShadowInfo.ViewFrustum.IntersectBox(ChildContext.Bounds.Center,ChildContext.Bounds.Extent))
							{
								bIsInFrustum = TRUE;
								break;
							}
						}
					}

					if(bIsInFrustum)
					{
						// If the child node was in the frustum of at least one preshadow, push it on
						// the iterator's pending node stack.
						PrimitiveOctreeIt.PushChild(ChildRef);
					}
				}
			}

			// Check all the primitives in this octree node.
			for(FScenePrimitiveOctree::ElementConstIt NodePrimitiveIt(PrimitiveOctreeNode.GetElementIt());NodePrimitiveIt;++NodePrimitiveIt)
			{
#if USE_MASSIVE_LOD
				// gather the shadows for this primitive and all of it's children
				const FPrimitiveSceneInfoCompact* PrimitiveSceneInfoCompact = &(*NodePrimitiveIt);
				GatherShadowsForPrimitives(&PrimitiveSceneInfoCompact, 1, 0xFF, PreShadows, ForwardObjectShadows, ViewDependentWholeSceneShadows);
#else
				// prefetch the next cache line of the compact scene info and the next primitive scene info
				const FPrimitiveSceneInfoCompact* PrimitiveSceneInfoCompact = &(*NodePrimitiveIt);
				CONSOLE_PREFETCH_NEXT_CACHE_LINE( PrimitiveSceneInfoCompact );
				const FPrimitiveSceneInfoCompact* NextPrimitiveSceneInfoCompact = PrimitiveSceneInfoCompact +
					(( NodePrimitiveIt.GetIndex() < PrimitiveOctreeNode.GetElementCount() - 1 ) ? 1 : 0);
				CONSOLE_PREFETCH( NextPrimitiveSceneInfoCompact->PrimitiveSceneInfo );


				// gather the shadows for this one primitive
				GatherShadowsForPrimitiveInner(*NodePrimitiveIt, 0xFF, PreShadows, ForwardObjectShadows, ViewDependentWholeSceneShadows);
#endif
			}
		}

		for(INT ShadowIndex = 0;ShadowIndex < PreShadows.Num();ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = PreShadows(ShadowIndex);
			//@todo - sort other shadow types' subject mesh elements?
			// Probably needed for good performance with non-dominant whole scene shadows (spotlightmovable)
			ProjectedShadowInfo->SortSubjectMeshElements();
		}
	}
}

/**
 * Calculates what shadows are made by the given the LightPrimInteractions
 * When using MassiveLOD, if the primitive in the interaction isn't drawn due to MinDrawDistance,
 * then the children light primitive interactions will be checked (by recursively calling this function).
 * If the primitive is drawn, it will never iterate over the children interactions
 *
 * @param Interactions A list of interactions to check for shadowing
 * @param VisibleLightInfo Information about the current light, will be updated
 * @param PreShadows Collects any preshadows
 * @param Origin View origin (used to calculate LOD)
 */
void FSceneRenderer::SetupInteractionShadows(
	TArray<FLightPrimitiveInteraction*>& Interactions, 
	FVisibleLightInfo& VisibleLightInfo, 
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& PreShadows, 
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ForwardObjectShadows, 
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ViewDependentWholeSceneShadows,
	const FVector& Origin)
{
#if USE_MASSIVE_LOD
	for (INT InteractionIndex = 0; InteractionIndex < Interactions.Num(); InteractionIndex++)
	{
		FLightPrimitiveInteraction* Interaction = Interactions(InteractionIndex);

		// check to see if the primitive is going to be rendered this frame (if it will, we can skip children)
		FPrimitiveSceneInfo* PrimitiveSceneInfo = Interaction->GetPrimitiveSceneInfo();

		// will this primitive be drawn due to massive lod
		UBOOL bWillBeDrawn = TRUE;
		if (Interaction->GetChildInteractions().Num() > 0)
		{
			const FLOAT DistanceSquared = CalculateDistanceSquaredForLOD(PrimitiveSceneInfo->Bounds, Origin);
			bWillBeDrawn = (DistanceSquared > Square(PrimitiveSceneInfo->MassiveLODDistance));
		}

		if (bWillBeDrawn)
		{
			if (!Interaction->IsParentOnly())
			{
#else
				// in the non-MassiveLOD case, this is just a single array entry
				checkSlow(Interactions.Num() == 1);
				FLightPrimitiveInteraction* Interaction = Interactions(0);

				// check to see if the primitive is going to be rendered this frame (if it will, we can skip children)
				FPrimitiveSceneInfo* PrimitiveSceneInfo = Interaction->GetPrimitiveSceneInfo();
#endif
				if (Interaction->HasShadow())
				{
					// Create per object shadows for primitives that should be lightmapped but the interaction is dynamic (unbuilt)
					if ((PrimitiveSceneInfo->bStaticShadowing 
						// Only create per object shadows if the light is built, otherwise a whole scene shadow will be created to preview the light's shadows
						&& Interaction->GetLight()->bPrecomputedLightingIsValid
						&& IsDominantLightType(Interaction->GetLight()->LightType)
						// Create per object shadows for dynamically shadowed primitives with a shadowed interaction
						|| !PrimitiveSceneInfo->bStaticShadowing)
						// Don't create per object shadows for movable dominant lights
						&& !(IsDominantLightType(Interaction->GetLight()->LightType) && !Interaction->GetLight()->bStaticShadowing))
					{
						// Create projected shadow infos and add the one created for the preshadow (if used) to the PreShadows array.
						FProjectedShadowInfo* ProjectedShadowInfo = CreateProjectedShadow(Interaction, PreShadows, ViewDependentWholeSceneShadows);

#if WITH_MOBILE_RHI
						if (GUsingMobileRHI 
							&& !GSupportsDepthTextures 
							&& ProjectedShadowInfo
							&& !ProjectedShadowInfo->bFullSceneShadow 
							&& !ProjectedShadowInfo->bPreShadow)
						{
							ForwardObjectShadows.AddItem(ProjectedShadowInfo);
						}
#endif
					}
				}
#if USE_MASSIVE_LOD
			}
		}
		// loop over children who may cast shadows
		else
		{
			if (Interaction->GetChildInteractions().Num() > 0)
			{
				SetupInteractionShadows(Interaction->GetChildInteractions(), VisibleLightInfo, PreShadows, ForwardObjectShadows, ViewDependentWholeSceneShadows, Origin);
			}
		}
	}
#endif
}

void FSceneRenderer::InitDynamicShadows()
{
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_DynamicShadowSetupTime, !bIsSceneCapture);
	
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> PreShadows;
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> ForwardObjectShadows;
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> ViewDependentWholeSceneShadows;
	{
		SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_InitDynamicShadowsTime, !bIsSceneCapture);

		// make an array to store a single interaction (since SetupInteractionShadows takes a TArray)
		TArray<FLightPrimitiveInteraction*> LightInteractionArray;
		LightInteractionArray.Add(1);

		// Pre-allocate for worst case.
		VisibleShadowCastingLightInfos.Empty(Scene->Lights.GetMaxIndex());

		if( GLogNextFrameShadowGenerationData )
		{
			debugf( TEXT("Logging stats for all shadows generated this frame"));
		}

		for(TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights);LightIt;++LightIt)
		{
			const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
			FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;
			FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos(LightSceneInfo->Id);

			// Only consider lights that may have shadows.
			if( LightSceneInfoCompact.bCastStaticShadow || LightSceneInfoCompact.bCastDynamicShadow )
			{
				// see if the light is visible in any view
				UBOOL bIsVisibleInAnyView = FALSE;

				for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
				{
					// View frustums are only checked when lights have visible primitives or have modulated shadows,
					// so we don't need to check for that again here
					bIsVisibleInAnyView = Views(ViewIndex).VisibleLightInfos(LightSceneInfo->Id).bInViewFrustum;
					if (bIsVisibleInAnyView) 
					{
						break;
					}
				}

				if( bIsVisibleInAnyView )
				{
					// Add to array of visible shadow casting lights if there is a view/ DPG that is affected.
					VisibleShadowCastingLightInfos.AddItem( LightSceneInfo );

					// If the light casts dynamic projected shadows, try creating a whole-scene shadow for it.
					UBOOL bCreatePrimitiveShadows = TRUE;
					
					// Don't create whole scene shadows for self shadow modes, those are best suited to per-object shadows
					if (!LightSceneInfo->bSelfShadowOnly 
						&& !LightSceneInfo->bNonModulatedSelfShadowing 
						&& (LightSceneInfoCompact.bCastDynamicShadow 
						// Only create whole scene shadows for lights that don't precompute shadowing (movable lights)
						&& !LightSceneInfoCompact.bStaticShadowing 
						// Only create a whole scene shadow if using non-modulated shadows, as a whole scene modulated shadow wouldn't be very useful
						&& LightSceneInfo->LightShadowMode == LightShadow_Normal
						// Also create a whole scene shadow for lights with precomputed shadows that are unbuilt and selected in the editor for previewing
						|| GIsEditor 
						&& LightSceneInfoCompact.bStaticShadowing 
						&& LightSceneInfoCompact.bCastStaticShadow 
						&& !LightSceneInfo->bPrecomputedLightingIsValid 
						&& LightSceneInfo->bOwnerSelected) 
						&& CreateWholeSceneProjectedShadow(LightSceneInfo) )
					{
						// If the light has a whole-scene shadow, it doesn't need individual primitive shadows.
						bCreatePrimitiveShadows = FALSE;
					}
					
					if(bCreatePrimitiveShadows)
					{
						TArray<FProjectedShadowInfo*,SceneRenderingAllocator> CurrentLightViewDependentWholeSceneShadows;
						if (GSystemSettings.bAllowWholeSceneDominantShadows)
						{
							TArray<FLOAT, TInlineAllocator<2> > FadeAlphas;
							// Allow each view to create a whole scene view dependent shadow
							for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
							{
								FViewInfo& View = Views(ViewIndex);
							
								FadeAlphas.AddItem(1.0f);

								//@todo - support view dependent shadows in splitscreen
								if (ViewIndex > 0 
									|| LightSceneInfoCompact.bStaticShadowing && !View.RenderingOverrides.bAllowDominantWholeSceneDynamicShadows)
								{
									break;
								}

								const INT NumSplits = LightSceneInfo->GetNumViewDependentWholeSceneShadows();
								for (INT SplitIndex = 0; SplitIndex < NumSplits; SplitIndex++)
								{
									FProjectedShadowInitializer ProjectedShadowInitializer;
									if (LightSceneInfo->GetViewDependentWholeSceneProjectedShadowInitializer(View, SplitIndex, ProjectedShadowInitializer))
									{
										const FIntPoint ShadowBufferResolution = GSceneRenderTargets.GetShadowDepthTextureResolution(SplitIndex == 0);
										// Create the projected shadow info.
										FProjectedShadowInfo* ProjectedShadowInfo = new(GRenderingThreadMemStack,1,16) FProjectedShadowInfo(
											LightSceneInfo,
											&View,
											ProjectedShadowInitializer,
											//@todo - remove the shadow border for whole scene dominant shadows
											ShadowBufferResolution.X - SHADOW_BORDER * 2,
											ShadowBufferResolution.Y - SHADOW_BORDER * 2,
											FadeAlphas
											);

										FVisibleLightInfo& LightViewInfo = VisibleLightInfos(LightSceneInfo->Id);
										VisibleLightInfo.MemStackProjectedShadows.AddItem(ProjectedShadowInfo);
										VisibleLightInfo.AllProjectedShadows.AddItem(ProjectedShadowInfo);
										ViewDependentWholeSceneShadows.AddItem(ProjectedShadowInfo);
										CurrentLightViewDependentWholeSceneShadows.AddItem(ProjectedShadowInfo);
									}
								}
							}
						}

						// Look for individual primitives with a dynamic shadow.
						for(FLightPrimitiveInteraction* Interaction = LightSceneInfo->DynamicPrimitiveList;
							Interaction;
							Interaction = Interaction->GetNextPrimitive()
							)
						{
							// Only allow creating shadows from non-dominant lights and the brightest dominant light
							if (!(IsDominantLightType(LightSceneInfo->LightType) 
								&& Interaction->GetPrimitiveSceneInfo()->BrightestDominantLightSceneInfo 
								&& Interaction->GetPrimitiveSceneInfo()->BrightestDominantLightSceneInfo != Interaction->GetLight()))
							{
								LightInteractionArray(0) = Interaction;
								SetupInteractionShadows(LightInteractionArray, VisibleLightInfo, PreShadows, ForwardObjectShadows, CurrentLightViewDependentWholeSceneShadows, Views(0).ViewOrigin);
							}
						}
					}
				}
			}
		}

		// Calculate visibility of the projected shadows.
		InitProjectedShadowVisibility();

		// Done logging, disable the single frame toggle
		if( GLogNextFrameShadowGenerationData )
		{
			GLogNextFrameShadowGenerationData = FALSE;
		}
	}

	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_GatherShadowPrimitivesTime, !bIsSceneCapture);

	// Clear old preshadows and attempt to add new ones to the cache
	UpdatePreshadowCache();

	CreatePlanarReflectionShadows();

	// Gather the primitives used to draw the pre-shadows and modulate-better shadows.
	GatherShadowPrimitives(PreShadows,ForwardObjectShadows,ViewDependentWholeSceneShadows);
}
